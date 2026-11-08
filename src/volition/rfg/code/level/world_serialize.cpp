// Original: volition/rfg/code/level/world_serialize.cpp
//
// Partial: object creation and destruction, zone and layer serialization (zone file -> objects), zone deserialization
// (objects -> the zone's saved state), saving the world to memory (save_and_*, save_state_*) and to a save slot
// (platform_save_to_disk).
//
// Saving: save_and_* set sl_flags.pending_save_game; process_save_load calls save_state_data in the next handshake,
// which runs world frames until save_state_start, one collect and compress per loaded zone and save_state_finish have
// filled stored_world_state[cur_world_state]. The image starts "RFGS" version 0x34 and the start zone's name; then come
// sections "u32 own offset, u32 offset of the section's end, data" read back by world::init_world_zones, load_territory,
// post_load_stream and player_level_init_post, which must stay in this order.
//
// Creating objects. A stub is an object allocated from its class pool (obj_class_instantiator) with its handle in
// object_handle_hash and obj_flags.stub set; object_from_handle does not return stubs. create_object_from_stub links it
// under its parent, runs the class's serialize(load) and adds it to all_objects, its type list and its subtype list,
// creates its broadphase object and, outside stub list serialization, streams it at once when it can. Handles come from
// the origin zone's handle manager (new objects) or from the zone file (preexisting objects; their upper byte names
// the zone).
//
// serialize_zone (zone -> objects): when the zone has a saved state (stored_zone_state, zlib compressed), the saved
// objects are created first and the zone file's own objects are skipped unless they are pending restoration or
// missing from the save; otherwise every object of the zone file is created (persistent objects of a territory cell
// live in the persistent zone and are skipped there). Stubs are created parents first, constraints last
// (serialize_stub_list). With preload (zone_visit_cb, while the layout loads) nothing is created: the handles of saved
// objects are reserved and the object hierarchy of the zone file is cached for restoration. The first serialization
// also sizes the saved state buffer: 20% of the zone file (at least 0x6000, 0x400 for border zones) plus up to 64 KB by
// the size of the destructible chunks (rfg_movers) of the zone (8 KB... 64 KB, 0xC800 for the persistent zone).
//
// Deserialization (objects -> zone state) runs one step per world frame (process_deserialization): collect writes
// every top-level object of the zone (with its descendants) into a zone file image (a header with the handles of
// objects to restore and the objects' property blocks), compress zlib-compresses both parts into stored_zone_state,
// then the zone is destreamed. Objects of the persistent zone go into the persistent zone's image.
//
// Notes (decompiled behaviour, kept as is):
// - deserialize_object writes the zone_object sibling, child and num fields uninitialized.
// - create_object_internal uses an uninitialized position when neither OBJ_PROP_JUST_POS nor OBJ_PROP_COMBINED exists.
// - serialize_zone keeps the chunk name of the previous rfg_mover when one has no OM_PROP_CHUNK_NAME.

#include "volition/rfg/code/level/world_internal.h"

#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_raid.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_tech_level.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/restore_list.h"
#include "volition/rfg/code/gameplay/safehouse_vip.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_save.h"
#include "volition/rfg/code/gamewide/game_trace.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_bp.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/level/rfg_zonex.h"
#include "volition/rfg/code/level/territory_shared.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/objects/critical_area.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_meta_data.h"
#include "volition/rfg/code/objects/object_mission_start_node.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_player_start.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/object_safehouse.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/action_script/action_script.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/stream2/code/stream2_container.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/md5.h"
#include "volition/vlib/code/memory/page_pool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/os.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/zlib_vlib.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include "keen/base/trace.hpp"

#include <new>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef hash_table<unsigned int, &uint_hash, 7280, unsigned short> handle_table;

// ---- globals
cs_wrapper Temp_destroy_cs(0); // 0x02FDE514 (placement inferred)

// ---- file-static globals
static const unsigned int zone_hash = str_hash_string("obj_zone"); // 0x02F9B054, the class name hash of obj_zone

// The zone index of a handle (upper byte: the zone's grid x and y), 257 or more for none.
static unsigned int handle_zone_index(unsigned int handle)
{
    return ((handle >> 24) & 0xF) * 16 + (handle >> 28);
}

// 0x0093B2F0 (:206)
object* world::stub_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object** objp = object_handle_hash.get_ptr(handle);
    return objp != nullptr ? *objp : nullptr;
}

// 0x0093B340 (:216)
bool world::alloc_obj_handle(object* stub, world_zone* wz)
{
    stub->handle = wz->obj_handle_mgr.allocate_handle();
    object_handle_hash.insert(stub->handle, stub);
    return true;
}

// 0x0093B380 (:232)
bool world::alloc_obj_handle_preexisting(object* stub, unsigned int handle)
{
    // The handle must name an existing zone.
    if (handle == 0xFFFFFFFF)
    {
        return false;
    }
    const unsigned int index = handle_zone_index(handle);
    if (index >= 257 || global_zone_grid[index] == nullptr)
    {
        return false;
    }
    stub->handle = handle;
    object_handle_hash.insert(handle, stub);
    return true;
}

// 0x0093B3E0 (:260)
void world::free_obj_handle(object* stub)
{
    // A handle from a zone file (original) or one asked to be kept stays reserved in its zone.
    object_handle_hash.remove(stub->handle);
    if (stub->obj_flags.original || stub->obj_flags.preserve_handle || stub->handle == 0xFFFFFFFF)
    {
        return;
    }
    const unsigned int index = handle_zone_index(stub->handle);
    if (index < 257 && global_zone_grid[index] != nullptr)
    {
        global_zone_grid[index]->obj_handle_mgr.status[(stub->handle & 0xFFFF) >> 3] &= (unsigned char)~(1 << (stub->handle & 7));
    }
}

// 0x0091D010 (:282)
void world::begin_mission_object_creation()
{
    mission_object_creation_mode = true;
}

// 0x0091D020 (:289)
void world::end_mission_object_creation()
{
    mission_object_creation_mode = false;
}

// 0x0093B450 (:328)
object* world::alloc_object_stub(unsigned int classname_hash, world_zone* origin_zone, unsigned int parent)
{
    const unsigned int bucket = classname_hash % (unsigned int)obj_class_instantiator::hash_size;
    if (obj_class_instantiator::alloc_funcs[bucket] == nullptr || (int)bucket < 0)
    {
        return nullptr;
    }
    object* stub = obj_class_instantiator::alloc_funcs[bucket]();
    if (stub == nullptr)
    {
        return nullptr;
    }
    obj_class_instantiator::type_funcs[bucket](stub->obj_type, stub->sub_type);
    stub->handle = origin_zone->obj_handle_mgr.allocate_handle();
    object_handle_hash.insert(stub->handle, stub);
    stub->reset();
    stub->obj_flags.stub = 1;
    stub->parent = parent;
    stub->obj_flags.bpo_index = obj_class_instantiator::bpo_index_funcs[obj_class_instantiator::get_class_index(classname_hash)]();
    stub->set_streaming_dependent(stub->obj_flags.bpo_index == 0);
    if (mission_object_creation_mode)
    {
        stub->obj_flags.created_by_mission_or_activity = 1;
    }
    return stub;
}

// 0x0093B530 (:372)
object* world::alloc_object_stub_preexisting(unsigned int classname_hash, unsigned int handle, unsigned int parent)
{
    const unsigned int bucket = classname_hash % (unsigned int)obj_class_instantiator::hash_size;
    if (obj_class_instantiator::alloc_funcs[bucket] == nullptr || (int)bucket < 0)
    {
        return nullptr;
    }
    object* stub = obj_class_instantiator::alloc_funcs[bucket]();
    if (stub == nullptr)
    {
        return nullptr;
    }
    obj_class_instantiator::type_funcs[bucket](stub->obj_type, stub->sub_type);
    if (!alloc_obj_handle_preexisting(stub, handle))
    {
        stub->pool_free();
        return nullptr;
    }
    stub->reset();
    // Zone files store "no parent" as 0.
    if (parent == 0)
    {
        parent = 0xFFFFFFFF;
    }
    stub->obj_flags.stub = 1;
    stub->parent = parent;
    stub->obj_flags.bpo_index = obj_class_instantiator::bpo_index_funcs[obj_class_instantiator::get_class_index(classname_hash)]();
    stub->set_streaming_dependent(stub->obj_flags.bpo_index == 0);
    if (mission_object_creation_mode)
    {
        stub->obj_flags.created_by_mission_or_activity = 1;
    }
    return stub;
}

// 0x0093E410 (:421)
bool world::create_object_from_stub(object* stub, obj_prop_block& pblock)
{
    if (stub->parent != 0xFFFFFFFF)
    {
        object* parent_objp = object_from_handle(stub->parent);
        stub->parent = 0xFFFFFFFF;
        if (parent_objp != nullptr)
        {
            parent_objp->set_child(stub);
        }
    }
    stub->obj_flags.stub = 0;
    if (!stub->serialize(pblock, true))
    {
        if (stub->parent != 0xFFFFFFFF)
        {
            object_from_handle(stub->parent)->remove_child(stub);
        }
        if (stub->bp_handle != nullptr)
        {
            havok_bpo_delete(stub->bp_handle);
            stub->bp_handle = nullptr;
        }
        return false;
    }
    stub->all_index = (unsigned short)all_objects.add(stub);
    stub->type_index = (unsigned short)type_objects[stub->obj_type].add(stub->all_index);
    stub->subtype_index = 0xFFFF;
    if (stub->sub_type != -1)
    {
        stub->subtype_index = (unsigned short)subtype_objects[stub->sub_type].add(stub->all_index);
    }
    if (stub->bp_handle == nullptr)
    {
        stub->bp_handle = havok_bpo_create(stub->obj_flags.bpo_index, stub->last_known_bmin, stub->last_known_bmax, stub, calc_bpo_flags(stub));
    }
    else
    {
        havok_bpo_update(stub->bp_handle, stub->last_known_bmin, stub->last_known_bmax);
    }
    // Outside stub list serialization the object streams in at once when it can (and its parent has).
    if (!stub_serialization_in_progress && process_streaming_object(stub))
    {
        object* parent_objp = object_from_handle(stub->parent);
        if (parent_objp == nullptr || parent_objp->obj_flags.streamed)
        {
            stream_object(stub);
        }
    }
    return true;
}

// 0x0093E610 (:513)
bool world::create_object_from_stub(object* stub, zone_object* zobjp)
{
    obj_prop_block pblock;
    rfg_zonex_load_object_pblock(pblock, zobjp);
    stub->last_known_bmin = zobjp->bmin;
    stub->last_known_bmax = zobjp->bmax;
    return create_object_from_stub(stub, pblock);
}

// 0x0093E6B0 (:539)
object* world::create_object_internal(unsigned int classname_hash, obj_prop_block& pblock, world_zone* origin_zone, unsigned int parent)
{
    vector pos;
    if (!pblock.get_optional(OBJ_PROP_JUST_POS, pos))
    {
        object_properties props; // uninitialized when the property is missing (see the file comment)
        pblock.get_optional(OBJ_PROP_COMBINED, props);
        pos = props.pos;
    }
    if (origin_zone == nullptr)
    {
        // The zone under the position; outside the grid only multiplayer and Wrecking Crew fall back to the first zone.
        origin_zone = get_world_zone_by_pos(pos);
        if (origin_zone == nullptr)
        {
            if (!Multiplayer && !wrecking_crew_is_active())
            {
                return nullptr;
            }
            if (all_zones == nullptr)
            {
                return nullptr;
            }
            origin_zone = all_zones[0];
        }
    }
    object* stub = alloc_object_stub(classname_hash, origin_zone, parent);
    if (stub == nullptr)
    {
        return nullptr;
    }
    stub->last_known_bmin = vector(pos.x - 0.5f, pos.y - 0.5f, pos.z - 0.5f);
    stub->last_known_bmax = vector(pos.x + 0.5f, pos.y + 0.5f, pos.z + 0.5f);
    if (create_object_from_stub(stub, pblock))
    {
        return stub;
    }
    free_object_stub(stub);
    return nullptr;
}

// 0x0091D040 (:596)
unsigned char world::calc_bpo_flags(const object* objp)
{
    // 1 dependent, 2 fixed (streaming_fixed replaces dependent), 4 top of a hierarchy, 8 zone.
    unsigned char flags = objp->obj_flags.is_dependent;
    if (objp->obj_flags.streaming_fixed)
    {
        flags = 2;
    }
    if (objp->parent == 0xFFFFFFFF)
    {
        flags |= 4;
    }
    if (objp->obj_type == OT_ZONE)
    {
        flags |= 8;
    }
    return flags;
}

// 0x0093B600 (:617)
void world::free_object_stub(object* stub)
{
    free_obj_handle(stub);
    stub->obj_flags.stub = 0;
    stub->handle = 0xFFFFFFFF;
    stub->parent = 0xFFFFFFFF;
    stub->pool_free();
}

// 0x0093B630 (:635)
void world::free_object(object* objp)
{
    // Stream out, let the class free its resources, then remove the object from the lists (moving the last entry of each
    // list into its slot) and give it back to its pool.
    if (objp->obj_flags.streamed)
    {
        objp->stream(false);
    }
    if (objp->srid != 0 && (objp->obj_flags.streaming || objp->obj_flags.streamed))
    {
        stream2_release(objp->srid);
    }
    if (objp->obj_flags.streaming)
    {
        num_streaming_objects--;
    }
    objp->obj_flags.world_free = 1;
    objp->obj_flags.streamed = 0;
    objp->obj_flags.streaming = 0;
    objp->free_this();
    objp->obj_flags.world_free = 0;
    if (objp->bp_handle != nullptr)
    {
        havok_bpo_delete(objp->bp_handle);
        objp->bp_handle = nullptr;
    }
    objp->obj_flags.stub = 1;
    objp->set_name(nullptr);

    const unsigned short all_index = objp->all_index;
    const int type = objp->obj_type;
    const int sub_type = objp->sub_type;
    base_array<unsigned short>& type_list = type_objects[type];
    if ((int)objp->type_index < type_list.size() - 1)
    {
        object* last = all_objects[type_list[type_list.size() - 1]];
        last->type_index = objp->type_index;
        type_list[objp->type_index] = last->all_index;
    }
    type_list.resize(type_list.size() - 1);
    if (sub_type != -1)
    {
        base_array<unsigned short>& subtype_list = subtype_objects[sub_type];
        if ((int)objp->subtype_index < subtype_list.size() - 1)
        {
            object* last = all_objects[subtype_list[subtype_list.size() - 1]];
            last->subtype_index = objp->subtype_index;
            subtype_list[objp->subtype_index] = last->all_index;
        }
        subtype_list.resize(subtype_list.size() - 1);
    }
    if ((int)objp->all_index < all_objects.size() - 1)
    {
        object* last = all_objects[all_objects.size() - 1];
        last->all_index = all_index;
        all_objects[all_index] = last;
        type_objects[last->obj_type][last->type_index] = last->all_index;
        if (last->sub_type != -1)
        {
            subtype_objects[last->sub_type][last->subtype_index] = last->all_index;
        }
    }
    all_objects.resize(all_objects.size() - 1);
    objp->obj_type = -1;
    objp->sub_type = -1;
    free_object_stub(objp);
}

// 0x0093C720 (:740)
void world::free_all_dead_objects()
{
    process_flagged_objects(1);
    havok_remove_entity_flush();
    havok_remove_phantom_flush();
}

// 0x0091D070 (:754)
bool world::destroy_object(object* objp, bool preserve_handle)
{
    // Marks the object destroyed (telling its children and itself) and queues it to be freed.
    Temp_destroy_cs.enter();
    if (objp == nullptr || objp->handle == 0xFFFFFFFF || objp->obj_type == -1)
    {
        Temp_destroy_cs.leave();
        return false;
    }
    if (!objp->obj_flags.destroyed)
    {
        for (object* child = objp->child_ptr; child != nullptr;)
        {
            if (!child->obj_flags.destroyed && (Multiplayer != true || multi_is_server() || !child->remote_obj_flags.client_sync || child->remote_obj_flags.client_destroy_permission))
            {
                child->hierarchy_notify(HN_PARENT_DESTROYED);
            }
            child = child->child_next;
            if (child == objp->child_ptr)
            {
                break;
            }
        }
        objp->obj_flags.destroyed = 1;
        objp->obj_flags.preserve_handle = preserve_handle;
        objp->destroy_notify();
    }
    put_on_flagged_list(objp, 1);
    Temp_destroy_cs.leave();
    return true;
}

// 0x0093C740 (:853)
bool world::destroy_object(unsigned int handle, bool preserve_handle)
{
    object* objp = object_from_handle(handle);
    if (objp == nullptr)
    {
        return false;
    }
    return destroy_object(objp, preserve_handle);
}

// Applies the zone_object flags to a new stub (inlined in visit_stub and serialize_zone).
static void apply_zone_object_flags(object* stub, const zone_object* zobjp)
{
    // A handle whose third byte is not 0xFF was assigned by the level editor (a zone-allocated one has 0xFF there).
    if (((zobjp->handle >> 16) & 0xFF) != 0xFF)
    {
        stub->obj_flags.original = 1;
    }
    if ((zobjp->flags & 0x10) != 0)
    {
        stub->obj_flags.streaming_fixed = 1;
    }
    if ((zobjp->flags & 0x20) != 0)
    {
        stub->obj_flags.force_full_save = 1;
    }
}

// 0x0093B860 (:872)
object* world::visit_stub(zone_object*& zobjp)
{
    if ((zobjp->flags & 8) != 0)
    {
        return nullptr;
    }
    object* stub = alloc_object_stub_preexisting(zobjp->classname_hash, zobjp->handle, zobjp->parent);
    if (stub != nullptr)
    {
        apply_zone_object_flags(stub, zobjp);
    }
    return stub;
}

// Collects stub and its stub ancestors (at most 16) into chain, the stub first; returns the count (inlined in
// serialize_stub_list).
static int collect_stub_chain(dynamic_uint_hash_table<object*>& hash, object* stub, object** chain)
{
    int count = 1;
    chain[0] = stub;
    for (unsigned int parent_handle = stub->get_parent(); parent_handle != 0xFFFFFFFF;)
    {
        object** entry = hash.get_ptr(parent_handle);
        object* parent = entry != nullptr ? *entry : nullptr;
        if (parent == nullptr)
        {
            break;
        }
        if (parent->obj_flags.stub)
        {
            chain[count++] = parent;
        }
        parent_handle = parent->get_parent();
    }
    return count;
}

// 0x0093E8D0 (:904)
void world::serialize_stub_list(int num_stubs, object** stub_list, zone_object** zobj_list)
{
    // Every stub is created after its stub ancestors; constraints wait until all other objects exist.
    stub_serialization_in_progress = true;
    farray<object*,512> constraints;
    for (int i = 0; i < num_stubs; i++)
    {
        object* stub = stub_list[i];
        if (!stub->obj_flags.stub)
        {
            continue;
        }
        if (stub->obj_type == OT_CONSTRAINT)
        {
            constraints.add(stub);
            continue;
        }
        object* chain[17];
        for (int n = collect_stub_chain(object_handle_hash, stub, chain); n > 0; n--)
        {
            object* objp = chain[n - 1];
            bool create = true;
            if ((Multiplayer || wrecking_crew_is_active()) && objp->parent != 0xFFFFFFFF)
            {
                // In multiplayer a child whose parent is gone is dropped.
                object** parent = object_handle_hash.get_ptr(objp->parent);
                if (parent == nullptr || *parent == nullptr)
                {
                    free_obj_handle(objp);
                    create = false;
                }
            }
            if (create)
            {
                const unsigned short flags = zobj_list[objp->all_index]->flags;
                if (create_object_from_stub(objp, zobj_list[objp->all_index]))
                {
                    if ((flags & 1) != 0)
                    {
                        objp->set_persistent(true);
                    }
                    continue;
                }
                free_obj_handle(objp);
            }
            objp->obj_flags.stub = 0;
            objp->handle = 0xFFFFFFFF;
            objp->parent = 0xFFFFFFFF;
            objp->pool_free();
        }
    }
    for (int i = 0; i < constraints.size(); i++)
    {
        if (!constraints[i]->obj_flags.stub)
        {
            continue;
        }
        object* chain[17];
        for (int n = collect_stub_chain(object_handle_hash, constraints[i], chain); n > 0; n--)
        {
            object* objp = chain[n - 1];
            const unsigned short flags = zobj_list[objp->all_index]->flags;
            create_object_from_stub(objp, zobj_list[objp->all_index]);
            if ((flags & 1) != 0)
            {
                objp->set_persistent(true);
            }
        }
    }
    stub_serialization_in_progress = false;
}

// 0x0093EC70 (:1042)
bool world::serialize_layer(const char* layer_name, unsigned int* object_list, int max_object_list_size, int& returned_objects)
{
    // Creates the objects of a managed layer (<name>.layer_pc, at most 511 objects) as persistent, never-saved
    // objects and returns their handles.
    char filename[64];
    cf_add_ext(filename, sizeof(filename), layer_name, ".layer_pc", false);
    zone_file* zf = zone_find(filename);
    if (zf == nullptr || zf->header.num_objects > 0x1FF)
    {
        return false;
    }
    cfile* f = cf_open(zf->objects, zf->object_data_size, "rb", PLATFORM_PC);
    if (f == nullptr)
    {
        return false;
    }
    object* stubs[512];
    zone_object* zobjs[512];
    int num_stubs = 0;
    zone_object* zobjp;
    while (rfg_zonex_get_next_object(zobjp, f))
    {
        zone_object* current = zobjp;
        current->flags |= 1;
        object* stub = visit_stub(zobjp);
        if (stub != nullptr)
        {
            zobjs[num_stubs] = current;
            stubs[num_stubs] = stub;
            stub->all_index = (unsigned short)num_stubs;
            num_stubs++;
            if (num_stubs >= 512)
            {
                break;
            }
        }
    }
    cf_close(f);
    serialize_stub_list(num_stubs, stubs, zobjs);
    returned_objects = 0;
    for (int i = 0; i < num_stubs && returned_objects < max_object_list_size; i++)
    {
        if (stubs[i]->handle != 0xFFFFFFFF)
        {
            object_list[returned_objects] = stubs[i]->handle;
            stubs[i]->obj_flags.no_save = 1;
            returned_objects++;
        }
    }
    return true;
}

// Asks a temporary object of the zone object's class (inlined twice: should_restore_object_*).
bool world::ask_class(zone_object* zobjp, bool (object::*question)(obj_prop_block&))
{
    object* objp = obj_class_instantiator::alloc_object(zobjp->classname_hash);
    if (objp == nullptr)
    {
        return false;
    }
    objp->reset();
    obj_prop_block pblock;
    rfg_zonex_load_object_pblock(pblock, zobjp);
    const bool answer = (objp->*question)(pblock);
    objp->pool_free();
    return answer;
}

// 0x0092DA10 (:1157)
bool world::should_restore_object_if_not_in_safe_file(zone_object* zobjp)
{
    return ask_class(zobjp, &object::serialize_restore_if_missing_in_save_file);
}

// 0x0092DAE0 (:1180)
bool world::should_restore_object_if_resetting_destruction(zone_object* zobjp)
{
    return ask_class(zobjp, &object::serialize_restore_if_resetting_destruction);
}

// 0x0093EE40 (:1205)
bool world::serialize_zone(world_zone* wz, zone_file* orig_zone, bool preload)
{
    const unsigned int common_mark = Common_mempool.mark();
    zone_file saved_zone;
    unsigned char* saved_data = (unsigned char*)Common_mempool.alloc(0x80000, 0);
    handle_table* restore_table = (handle_table*)Common_mempool.alloc(sizeof(handle_table), 0);
    if (restore_table != nullptr)
    {
        new (restore_table) handle_table(false);
    }
    handle_table* saved_handles = (handle_table*)Common_mempool.alloc(sizeof(handle_table), 0);
    if (saved_handles != nullptr)
    {
        new (saved_handles) handle_table(false);
    }
    if (!preload)
    {
        restore_pending_pop(restore_table, nullptr, wz->gid, wz->stored_zone_state.cur_size > 0);
    }

    // The saved state: a zone file image whose handle block lists objects to restore.
    if (wz->stored_zone_state.cur_size > 0)
    {
        unsigned int written = 0;
        unsigned int read = 0;
        decompress_stream->decompress_init();
        decompress_stream->decompress(saved_data, 0x80000, &written, wz->stored_zone_state.buf, wz->stored_zone_state.cur_size, &read);
        decompress_stream->decompress_finish();
        rfg_zonex_get_data(&saved_zone, saved_data, written);
        if (saved_zone.handle_data_size > 0)
        {
            cfile* f = cf_open(saved_zone.handle_data, saved_zone.handle_data_size, "rb", PLATFORM_PC);
            while (!cf_eof(f))
            {
                restore_table->insert(cf_read_uint32(f));
            }
            cf_close(f);
        }
    }
    const unsigned int list_size = (orig_zone->header.num_objects + saved_zone.header.num_objects) * 4;
    zone_object** zobj_list = (zone_object**)Common_mempool.alloc(list_size, 0);
    object** stub_list = (object**)Common_mempool.alloc(list_size, 0);
    int num_stubs = 0;

    // The first serialization sizes the zone's saved state buffer.
    if (wz->stored_zone_state.max_size == 0)
    {
        char zone_filename[64];
        _snprintf_safe(zone_filename, sizeof(zone_filename), "%s%s", wz->name, ".rfgzone_pc");
        int extra;
        if ((orig_zone->header.flags & 2) == 0)
        {
            // Destructible buildings need room by the size of their chunk collision data (300 MB of chunks: 64 KB).
            static unsigned int rfg_mover_hash = str_hash_string("rfg_mover");
            int chunk_bytes = 0;
            cfile* f = cf_open(orig_zone->objects, orig_zone->object_data_size, "rb", PLATFORM_PC);
            char chunk_name[256];
            memset(chunk_name, 0, sizeof(chunk_name));
            char container_name[256];
            memset(container_name, 0, sizeof(container_name));
            zone_object* zobjp;
            while (rfg_zonex_get_next_object(zobjp, f))
            {
                if ((zobjp->flags & 1) == 0 && zobjp->classname_hash == rfg_mover_hash)
                {
                    obj_prop_block pblock;
                    rfg_zonex_load_object_pblock(pblock, zobjp);
                    pblock.get_optional(OM_PROP_CHUNK_NAME, &chunk_name[0]);
                    cf_add_ext(container_name, sizeof(container_name), chunk_name, ".rfgchunkx", false);
                    stream2_container* container = stream_container_find_container(container_name);
                    if (container != nullptr)
                    {
                        cf_add_ext(container_name, sizeof(container_name), chunk_name, ".cchk_pc", false);
                        stream2_prim* prim = container->get_prim(container_name);
                        if (prim != nullptr)
                        {
                            chunk_bytes += prim->cpu_size;
                        }
                    }
                }
            }
            cf_close(f);
            float ratio = (float)chunk_bytes / 3.0e8f;
            if (1.0f - ratio < 0.0f)
            {
                ratio = 1.0f;
            }
            extra = (int)(ratio * 65536.0f);
        }
        else
        {
            extra = 0xC800;
        }
        const int minimum = (orig_zone->header.flags & 1) != 0 ? 0x400 : 0x6000;
        unsigned int file_size = 0;
        if (!cf_get_file_size(&file_size, zone_filename, false))
        {
            file_size = orig_zone->data_size;
        }
        double size = (double)((float)file_size * 0.2f);
        if (size <= (double)minimum)
        {
            size = (double)minimum;
        }
        wz->stored_zone_state.max_size = (int)size + extra;
        wz->stored_zone_state.buf = (unsigned char*)Level_mempool.alloc((int)size + extra, 0);
        if (wz->stored_zone_state.buf == nullptr)
        {
            VLIB_CRITICAL_FAILURE();
        }
    }

    // Saved objects first (unless pending restoration, which recreates them from the zone file).
    if (saved_zone.object_data_size > 0)
    {
        cfile* f = cf_open(saved_zone.objects, saved_zone.object_data_size, "rb", PLATFORM_PC);
        zone_object* zobjp;
        while (rfg_zonex_get_next_object(zobjp, f))
        {
            if (preload)
            {
                // Runtime handles of saved objects stay reserved in their zones.
                if (((zobjp->handle >> 16) & 0xFF) == 0xFF && zobjp->handle != 0xFFFFFFFF)
                {
                    const unsigned int index = handle_zone_index(zobjp->handle);
                    if (index < 257 && global_zone_grid[index] != nullptr)
                    {
                        global_zone_grid[index]->obj_handle_mgr.status[(zobjp->handle & 0xFFFF) >> 3] |= (unsigned char)(1 << (zobjp->handle & 7));
                    }
                }
                continue;
            }
            if (restore_table->exists(zobjp->handle))
            {
                continue;
            }
            if (sl_flags.reset_destruction && should_restore_object_if_resetting_destruction(zobjp))
            {
                restore_table->insert(zobjp->handle);
                continue;
            }
            if ((zobjp->flags & 8) != 0)
            {
                continue;
            }
            object* stub = alloc_object_stub_preexisting(zobjp->classname_hash, zobjp->handle, zobjp->parent);
            if (stub == nullptr)
            {
                continue;
            }
            apply_zone_object_flags(stub, zobjp);
            saved_handles->insert(stub->handle);
            stub->all_index = (unsigned short)num_stubs;
            zobj_list[num_stubs] = zobjp;
            stub_list[num_stubs] = stub;
            num_stubs++;
        }
        cf_close(f);
    }

    // The zone file's objects.
    cfile* f = cf_open(orig_zone->objects, orig_zone->object_data_size, "rb", PLATFORM_PC);
    zone_object* zobjp;
    while (rfg_zonex_get_next_object(zobjp, f))
    {
        if (preload)
        {
            if (orig_zone->relation_data_size > 0)
            {
                restore_cache_in_table(&orig_zone->relation_data, zobjp, orig_zone->objects);
            }
            str_hash_string("object_mission_start_node");
            continue;
        }
        // Persistent objects of a cell are created with the persistent zone.
        if ((orig_zone->header.flags & 2) == 0 && (zobjp->flags & 1) != 0)
        {
            continue;
        }
        // With a saved state, an object comes from the zone file only to be restored (pending restoration, missing
        // from the save, or reset destruction).
        if (wz->stored_zone_state.cur_size > 0 && zobjp->classname_hash != zone_hash && !restore_table->exists(zobjp->handle))
        {
            const bool restore_missing = !saved_handles->exists(zobjp->handle) && should_restore_object_if_not_in_safe_file(zobjp);
            if (!restore_missing && !(sl_flags.reset_destruction && should_restore_object_if_resetting_destruction(zobjp)))
            {
                continue;
            }
        }
        // Multiplayer objects exist only in the game modes their flags name (bit 8 + mode).
        const bool in_mode = (!Multiplayer && !wrecking_crew_is_active()) || wrecking_crew_is_active() || zobjp->classname_hash == zone_hash || (zobjp->flags & ((1 << (multi_gameplay_mode_get() & 0x1F)) << 8)) != 0;
        if (!in_mode || (zobjp->flags & 8) != 0)
        {
            continue;
        }
        object* stub = alloc_object_stub_preexisting(zobjp->classname_hash, zobjp->handle, zobjp->parent);
        if (stub == nullptr)
        {
            continue;
        }
        apply_zone_object_flags(stub, zobjp);
        stub->all_index = (unsigned short)num_stubs;
        zobj_list[num_stubs] = zobjp;
        num_stubs++;
        stub_list[num_stubs - 1] = stub;
    }
    cf_close(f);
    if (!preload)
    {
        serialize_stub_list(num_stubs, stub_list, zobj_list);
    }
    if (restore_table != nullptr)
    {
        restore_table->~handle_table();
    }
    if (saved_handles != nullptr)
    {
        saved_handles->~handle_table();
    }
    Common_mempool.mark();
    if (!preload)
    {
        restore_post_zone_serialize_cleanup(orig_zone->objects, wz->gid);
    }
    Common_mempool.restore_to_mark(common_mark);
    return true;
}

// 0x0092DBB0 (:1580)
void world::deserialize_object(world_zone* wz, object* objp, bool force_delete, bool do_persistent)
{
    // Writes the object (after its children) into the zone image: OSM_FULL objects as property blocks, OSM_RESTORE ones
    // as a handle to restore from the zone file, OSM_NONE (and destroyed or unsaved) ones not at all. Non-original
    // restorable objects are saved in full.
    int mode = objp->serialize_mode();
    if (mode == OSM_RESTORE && !objp->obj_flags.original)
    {
        mode = OSM_FULL;
    }
    if (objp->obj_flags.destroyed || objp->obj_flags.no_save || objp->obj_flags.destroy_on_stream || force_delete)
    {
        mode = OSM_NONE;
    }
    objp->obj_flags.serialize_protected = 1;
    for (object* child = objp->child_ptr; child != nullptr;)
    {
        deserialize_object(wz, child, mode == OSM_NONE, do_persistent);
        child = child->child_next;
        if (child == objp->child_ptr)
        {
            break;
        }
    }
    objp->obj_flags.serialize_protected = 0;
    if (objp->obj_flags.persistent != do_persistent)
    {
        return;
    }
    if (mode == OSM_NONE)
    {
        if (!sl_flags.saving_state_data)
        {
            World.destroy_object(objp, false);
        }
        return;
    }
    if (do_persistent)
    {
        wz = global_zone_grid[15];
    }
    cfile* f = wz->deserialize_f;
    if (mode == OSM_RESTORE)
    {
        cf_write_uint32(objp->handle, wz->deserialize_header_f);
        wz->deserialize_header->num_handles++;
        if (!sl_flags.saving_state_data)
        {
            World.destroy_object(objp, true);
        }
        return;
    }
    obj_prop_block pblock;
    if (objp->serialize(pblock, false))
    {
        // The zone_object header is written last, once the block size is known.
        const unsigned int start = cf_tell(f);
        cf_seek(f, sizeof(zone_object), CF_SEEK_CUR);
        pblock.save(f);
        zone_object zobj;
        objp->get_bbox(zobj.bmin, zobj.bmax);
        zobj.classname_hash = str_hash_string(objp->get_classname());
        zobj.flags = (unsigned short)(((objp->obj_flags.force_full_save * 2) | objp->obj_flags.streaming_fixed) << 4 | objp->obj_flags.persistent);
        zobj.handle = objp->handle;
        zobj.parent = objp->parent;
        zobj.block_size = (unsigned short)(cf_tell(f) - start);
        const unsigned int end = cf_tell(f);
        if (!cf_seek(f, start, CF_SEEK_SET))
        {
            keen::debug::nativePrintArgs("[world_serialize.cpp] <deserialize_object> Failed to seek! Position:%u \n", start);
        }
        if (cf_write(&zobj, sizeof(zone_object), f) == 0)
        {
            keen::debug::nativePrintArgs("[world_serialize.cpp] <deserialize_object> Failed to write! Classname:%u \n", zobj.classname_hash);
        }
        if (!cf_seek(f, end, CF_SEEK_SET))
        {
            keen::debug::nativePrintArgs("[world_serialize.cpp] <deserialize_object> Failed to seek! Position:%u \n", end);
        }
        cf_pad_align_write(4, f);
        wz->deserialize_header->num_objects++;
    }
    if (!sl_flags.saving_state_data)
    {
        World.destroy_object(objp, true);
    }
}

// 0x0092DE70 (:1766)
bool world::deserialize_zone(world_zone* wz)
{
    if ((deserialize_state != DS_IDLE && deserialize_state != DS_PENDING) || wz->state != WZS_LOADED)
    {
        return false;
    }
    deserialize_list.add(wz);
    deserialize_state = DS_PENDING;
    return true;
}

// Starts a section of the world image: its own offset, then room for the offset of its end (inlined at every section of
// save_state_start and save_state_finish). Returns where that room is.
static unsigned int save_section_begin()
{
    cf_write_uint32(cf_tell(World.save_file), World.save_file);
    const unsigned int end_slot = cf_tell(World.save_file);
    cf_write_uint32(end_slot, World.save_file);
    return end_slot;
}

static void save_section_end(unsigned int end_slot)
{
    const unsigned int end = cf_tell(World.save_file);
    cf_seek(World.save_file, end_slot, CF_SEEK_SET);
    cf_write_uint32(end, World.save_file);
    cf_seek(World.save_file, end, CF_SEEK_SET);
}

// 0x0093C770 (:2171)
// Queues every loaded zone and writes the part of the image that comes before the zones. The zone states follow in
// save_state_finish, once process_deserialization has collected and compressed them.
bool world::save_state_start()
{
    sl_flags.pending_save_state = false;
    sl_flags.performing_save_state = true;
    for (int i = 0; i < num_territory_zones; i++)
    {
        if (all_zones[i]->state == WZS_LOADED)
        {
            deserialize_zone(all_zones[i]);
        }
    }
    world_state_buf& state = stored_world_state[cur_world_state];
    state.cur_size = 0;
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return false;
    }
    if (player_has_non_inventory(pp))
    {
        human_weapons_drop_equipped_weapon(pp);
        pp->desired_equipped_inv_item = pp->last_equipped_weapon;
        pp->last_equipped_weapon = pp->second_last_equipped_weapon;
        pp->second_last_equipped_weapon = nullptr;
    }

    // The zone the load starts in: the warp target, the activity's start node, the nearest safehouse outside a mission,
    // or where the player is.
    world_zone* start_zone = get_world_zone_by_pos(pp->pos);
    if (sl_flags.save_game_warp)
    {
        start_zone = get_world_zone_by_pos(pending_game_load_warp_to_pos);
        if (start_zone == nullptr)
        {
            return false;
        }
    }
    else if (activity_get_node_start_on_load() != 0xFFFFFFFF)
    {
        object* node = World.object_from_handle(activity_get_node_start_on_load());
        if (node == nullptr)
        {
            return false;
        }
        start_zone = get_world_zone_by_pos(node->pos);
        if (start_zone == nullptr)
        {
            return false;
        }
    }
    else if (!mission_is_active())
    {
        object_safehouse* safehouse = safehouse_find_spawn_safehouse(pp->pos);
        if (safehouse != nullptr)
        {
            start_zone = get_world_zone_by_pos(safehouse->pos);
        }
    }
    if (start_zone == nullptr)
    {
        return false;
    }
    World.save_file = cf_open(state.buf, state.max_size, "wb", PLATFORM_PC);
    if (World.save_file == nullptr)
    {
        return false;
    }
    GAME_TRACE("world", "save state starts: start zone %s warp=%d", start_zone->name, sl_flags.save_game_warp ? 1 : 0);
    cf_write_uint32(0x53474652, World.save_file); // "RFGS"
    cf_write_uint32(0x34, World.save_file); // the save version
    cf_write_string(start_zone->name, World.save_file);
    cf_pad_align_write(4, World.save_file);
    cf_write_int32(num_territory_zones, World.save_file);

    unsigned int end_slot = save_section_begin();
    if (!destroyed_object_table_save(World.save_file))
    {
        return false;
    }
    save_section_end(end_slot);

    end_slot = save_section_begin();
    if (!mission_system_save(World.save_file))
    {
        return false;
    }
    save_section_end(end_slot);

    // The scripts, zlib compressed in place: u32 compressed size, data.
    end_slot = save_section_begin();
    const unsigned int size_slot = cf_tell(World.save_file);
    cf_write_uint32(0, World.save_file);
    const unsigned int common_mark = Common_mempool.mark();
    // Magic number: 1 MB of script state before compression.
    unsigned char* script_buf = (unsigned char*)Common_mempool.alloc(0x100000, 0);
    cfile* script_file = script_buf != nullptr ? cf_open(script_buf, 0x100000, "wb", PLATFORM_PC) : nullptr;
    if (script_file == nullptr)
    {
        VLIB_CRITICAL_FAILURE();
        Common_mempool.restore_to_mark(common_mark);
        return false;
    }
    if (!script_system_save(script_file))
    {
        VLIB_CRITICAL_FAILURE();
        cf_close(script_file);
        Common_mempool.restore_to_mark(common_mark);
        return false;
    }
    unsigned int compressed = 0;
    unsigned int consumed = 0;
    const unsigned int script_size = cf_tell(script_file);
    const unsigned int image_pos = cf_tell(World.save_file);
    compress_stream->compress_init(ZC_BEST_SPEED);
    const bool compress_ok = compress_stream->compress(state.buf + image_pos, state.max_size - image_pos, &compressed, script_buf, script_size, &consumed, true);
    compress_stream->compress_finish();
    cf_close(script_file);
    Common_mempool.restore_to_mark(common_mark);
    if (!compress_ok)
    {
        VLIB_CRITICAL_FAILURE();
        return false;
    }
    cf_seek(World.save_file, size_slot, CF_SEEK_SET);
    cf_write_uint32(compressed, World.save_file);
    if (!cf_seek(World.save_file, compressed, CF_SEEK_CUR))
    {
        return false;
    }
    save_section_end(end_slot);

    end_slot = save_section_begin();
    progression_level_system_save(World.save_file);
    save_section_end(end_slot);
    end_slot = save_section_begin();
    activity_system_save(World.save_file);
    save_section_end(end_slot);
    end_slot = save_section_begin();
    restore_list_system_save(World.save_file);
    save_section_end(end_slot);
    end_slot = save_section_begin();
    activity_raid_system_save(World.save_file);
    save_section_end(end_slot);
    end_slot = save_section_begin();
    safehouse_vip_save(World.save_file);
    save_section_end(end_slot);
    end_slot = save_section_begin();
    object_spawn_node_system_save(World.save_file);
    save_section_end(end_slot);
    end_slot = save_section_begin();
    cf_write_float(tech_level, World.save_file);
    cf_write_float(tech_level_max, World.save_file);
    cf_write_bool(Allow_walker_spawn, World.save_file);
    save_section_end(end_slot);
    end_slot = save_section_begin();
    gameaudio_system_save(World.save_file);
    save_section_end(end_slot);
    end_slot = save_section_begin();
    ui_system_save(World.save_file);
    save_section_end(end_slot);
    end_slot = save_section_begin();
    restore_system_save(World.save_file);
    save_section_end(end_slot);
    end_slot = save_section_begin();
    critical_area_save(World.save_file);
    save_section_end(end_slot);
    end_slot = save_section_begin();
    public_address_kiosk_save(World.save_file);
    save_section_end(end_slot);
    end_slot = save_section_begin();
    human_teams_save(World.save_file);
    save_section_end(end_slot);
    end_slot = save_section_begin();
    stats_serialize_out_save_game_stats(World.save_file);
    save_section_end(end_slot);
    end_slot = save_section_begin();
    game_clock_save(World.save_file);
    save_section_end(end_slot);
    if (cf_error(World.save_file) != CF_ERROR_NONE)
    {
        sl_flags.performing_save_state = false;
        return false;
    }
    return true;
}

// 0x0093DB80 (:2442)
// The zone states, the districts, where the player starts next, the inventory, the meta data and the roads.
bool world::save_state_finish()
{
    for (int i = 0; i < num_territory_zones; i++)
    {
        world_zone* wz = all_zones[i];
        cf_write_string(wz->name, World.save_file);
        cf_pad_align_write(4, World.save_file);
        cf_write_int32(wz->stored_zone_state.cur_size, World.save_file);
        cf_write_int32(wz->stored_zone_state.max_size, World.save_file);
        if (wz->stored_zone_state.cur_size > 0)
        {
            cf_write(wz->stored_zone_state.buf, wz->stored_zone_state.cur_size, World.save_file);
            cf_pad_align_write(4, World.save_file);
        }
    }
    unsigned int end_slot = save_section_begin();
    object_district_save_districts(World.save_file);
    save_section_end(end_slot);

    // The safehouse start: the spawn safehouse's, else the first player start of the player's zone, else the player's
    // own position.
    player* pp = player_get_local_player();
    vector start_pos = pp->pos;
    matrix start_orient(pp->orient);
    vector safehouse_pos = start_pos;
    matrix safehouse_orient(start_orient);
    object_safehouse* safehouse = safehouse_find_spawn_safehouse(pp->pos);
    if (safehouse != nullptr)
    {
        safehouse_get_start_pos_orient(safehouse_pos, safehouse_orient, safehouse);
    }
    else
    {
        world_zone* wz = get_world_zone_by_pos(pp->pos);
        if (wz != nullptr)
        {
            unsigned int handle = 0xFFFFFFFF;
            if (collect_objects<player_start>(wz->bmin, wz->bmax, &handle, 1, nullptr, nullptr) != 0 && handle != 0xFFFFFFFF)
            {
                object* objp = object_from_handle(handle);
                if (objp != nullptr && objp->get_obj_type() == OT_PLAYER_START)
                {
                    safehouse_pos = objp->pos;
                    safehouse_orient = objp->orient;
                }
            }
        }
    }

    // The normal start: the warp target, the activity's start, the safehouse start outside a mission, the checkpoint's
    // start in a checkpoint save, else a mission start node near the player. Original: the object searches leave the
    // player's own position when they find nothing.
    if (sl_flags.save_game_warp)
    {
        start_pos = pending_game_load_warp_to_pos;
        start_orient = pending_game_load_warp_to_orient;
        sl_flags.save_game_warp = false;
    }
    else if (activity_get_node_start_on_load() != 0xFFFFFFFF)
    {
        object* start = activity_get_player_start(activity_get_node_start_on_load());
        if (start == nullptr)
        {
            return false;
        }
        start_pos = start->pos;
        start_orient = start->orient;
    }
    else if (!mission_is_active())
    {
        start_pos = safehouse_pos;
        start_orient = safehouse_orient;
    }
    else if (cur_world_state != WSM_CHECKPOINT)
    {
        unsigned int handle = 0xFFFFFFFF;
        // Magic number: 20 m.
        if (World.collect_objects<object_mission_start_node>(start_pos, 20.0f, &handle, 1, nullptr, nullptr) != 0 && handle != 0xFFFFFFFF)
        {
            object* objp = World.object_from_handle(handle);
            if (objp != nullptr && objp->get_obj_type() == OT_MISSION_START_NODE)
            {
                start_pos = objp->pos;
                start_orient = objp->orient;
            }
        }
    }
    else
    {
        const unsigned int handle = mission_get_checkpoint_start_location_handle();
        object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
        if (objp != nullptr && objp->get_obj_type() == OT_PLAYER_START)
        {
            start_pos = objp->pos;
            start_orient = objp->orient;
        }
    }
    if (pp->dead_timestamp.valid())
    {
        Player_died = true;
    }
    end_slot = save_section_begin();
    cf_write_bool(Player_died, World.save_file);
    cf_write_vector(start_pos, World.save_file);
    cf_write_matrix(start_orient, World.save_file);
    cf_write_vector(safehouse_pos, World.save_file);
    cf_write_matrix(safehouse_orient, World.save_file);
    save_section_end(end_slot);
    end_slot = save_section_begin();
    pp->save_inventory(World.save_file);
    save_section_end(end_slot);
    end_slot = save_section_begin();
    pp->meta_data.save_meta_data(World.save_file);
    save_section_end(end_slot);
    end_slot = save_section_begin();
    road_data_save(World.save_file);
    save_section_end(end_slot);

    world_state_buf& state = stored_world_state[cur_world_state];
    state.player_start_pos = start_pos;
    state.player_start_orient = start_orient;
    state.player_start_pos_safehouse = safehouse_pos;
    state.player_start_orient_safehouse = safehouse_orient;
    state.cur_size = cf_tell(World.save_file);
    const bool ok = cf_error(World.save_file) == CF_ERROR_NONE;
    cf_close(World.save_file);
    World.save_file = nullptr;
    sl_flags.performing_save_state = false;
    GAME_TRACE("world", "save state done: %d of %d bytes, start (%.1f %.1f %.1f) ok=%d", state.cur_size, state.max_size, start_pos.x, start_pos.y, start_pos.z, ok ? 1 : 0);
    return ok;
}

// 0x009413B0 (:2601)
// Runs world frames until the state is written.
bool world::save_state_data()
{
    if (sl_flags.saving_state_data)
    {
        return false;
    }
    if (sl_flags.game_save_checkpoint)
    {
        cur_world_state = WSM_CHECKPOINT;
        sl_flags.game_save_checkpoint = false;
    }
    else
    {
        cur_world_state = WSM_DEFAULT;
    }
    do
    {
        do_frame(false);
    } while (deserialize_state != DS_IDLE);
    sl_flags.saving_state_data = true;
    sl_flags.pending_save_state = true;
    do
    {
        do_frame(false);
    } while (deserialize_state != DS_IDLE);
    sl_flags.saving_state_data = false;
    return get_state_data() != nullptr;
}

// 0x0091D1C0 (:2641)
void world::save_and_continue(game_save_info* save_slot, bool checkpoint, bool auto_save)
{
    if (sl_flags.pending_save_game || sl_flags.performing_save_game)
    {
        return;
    }
    sl_flags.pending_save_game = true;
    sl_flags.game_save_checkpoint = checkpoint;
    sl_flags.auto_save_game = auto_save;
    if (checkpoint)
    {
        // Magic number: -1 keeps the message for its default time.
        hud_message_add_minor(localization_get_display_string_req("HM_CHECK_POINT", nullptr), -1.0f, false);
        sl_flags.game_save_to_disk = false;
    }
    else
    {
        sl_flags.game_save_to_disk = !cheats_are_enabled();
    }
    pending_game_save_slot = save_slot;
}

// 0x0091D260 (:2744)
void world::save_and_reload()
{
    if (deserialize_state != DS_IDLE || sl_flags.pending_save_game || sl_flags.performing_save_state)
    {
        return;
    }
    if (!mission_is_active())
    {
        save_and_continue(nullptr, false, true);
    }
    sl_flags.pending_load_game_from_memory = true;
}

// 0x0091D2A0 (:2850)
void world::save_and_warp(const vector& warp_pos, const matrix& warp_orient)
{
    if (deserialize_state != DS_IDLE || sl_flags.pending_save_game || sl_flags.performing_save_state || mission_is_active())
    {
        return;
    }
    sl_flags.save_game_warp = true;
    pending_game_load_warp_to_pos = warp_pos;
    pending_game_load_warp_to_orient = warp_orient;
    save_and_reload();
}

// 0x0091D330 (:2870)
void world::save_and_continue_warp(const vector& warp_pos, const matrix& warp_orient)
{
    if (deserialize_state != DS_IDLE || sl_flags.pending_save_game || sl_flags.performing_save_state || mission_is_active())
    {
        return;
    }
    sl_flags.save_game_warp = true;
    pending_game_load_warp_to_pos = warp_pos;
    pending_game_load_warp_to_orient = warp_orient;
    save_and_continue(nullptr, false, true);
}

// 0x0092D770: the saved state into a save slot. A null slot is the autosave: its slot if there is one, else a new one.
// The slot's summary is filled in, the state padded to whole 512 byte blocks with an MD5 in the last 16 bytes.
// Original quirks kept: a save too large for a slot is dropped without a message (and Common_mempool keeps the autosave
// list until the next restore). The year is the two digits os_get_date gives, masked to a byte like the other fields.
void platform_save_to_disk(void* data, int len, game_save_info* save_slot, bool auto_save)
{
    unsigned char* bytes = (unsigned char*)data;
    const unsigned int common_mark = Common_mempool.mark();
    game_save_info new_slot;
    memset(&new_slot, 0, sizeof(new_slot));
    if (save_slot == nullptr)
    {
        // Magic number: the 12 save slots.
        const int list_size = game_save_get_list_buf_size(12);
        game_save_info* list = (game_save_info*)Common_mempool.alloc(list_size, 0);
        save_slot = game_save_get_autosave(list, list_size, 12, 0, true, World.dlc_bundle_id);
        if (save_slot == nullptr)
        {
            save_slot = &new_slot;
        }
    }
    const tracked_stat* stat = nullptr;
    stats_get_stat(stat, STATS_MISSIONS_COMPLETED);
    save_slot->num_missions_completed = stat->value.integer;
    stats_get_stat(stat, STATS_ACTIVITIES_COMPLETED);
    save_slot->num_activities_completed = stat->value.integer;
    save_slot->districts_liberated = object_district_num_liberated();
    // The district of the safehouse a load will start at, or of the player.
    const player* pp = player_get_local_player();
    const object_safehouse* safehouse = safehouse_find_spawn_safehouse(pp->pos);
    const vector district_pos = safehouse != nullptr ? safehouse->pos : pp->pos;
    const district* save_district = district_get_from_pos(district_pos);
    // Deviation: rfg.exe does not check; a position outside every district would crash the save here.
    save_slot->district_index = save_district != nullptr ? save_district->get_district_index() : 0;
    const territory* save_territory = territory_shared_find(World.get_territory_name());
    save_slot->territory_index = save_territory != nullptr ? save_territory->index : 0;
    // os_get_date gives "MM/DD/YY", os_get_time "HH:MM:SS": the separators become terminators.
    char date_text[16];
    os_get_date(date_text, sizeof(date_text));
    date_text[2] = '\0';
    date_text[5] = '\0';
    save_slot->month = atoi(date_text) & 0xFF;
    save_slot->day = atoi(date_text + 3) & 0xFF;
    save_slot->year = atoi(date_text + 6) & 0xFF;
    char time_text[16];
    os_get_time(time_text, sizeof(time_text));
    time_text[2] = '\0';
    time_text[5] = '\0';
    save_slot->hours = atoi(time_text) & 0xFF;
    save_slot->minutes = atoi(time_text + 3) & 0xFF;
    save_slot->seconds = atoi(time_text + 6) & 0xFF;
    save_slot->auto_save = auto_save;
    save_slot->dlc_id = World.dlc_bundle_id;
    save_slot->hammers_unlocked = 0;
    const uint64_t hammers = meta_data_get_hammers_unlocked();
    memcpy(save_slot->new_data.hammers_unlocked_large.raw_data, &hammers, sizeof(hammers));
    save_slot->golden_hammer_desired = Wants_to_start_game_with_golden_hammer;
    const unsigned short backpacks = meta_data_get_backpacks_unlocked();
    memcpy(save_slot->new_data.backpacks_unlocked.raw_data, &backpacks, sizeof(backpacks));
    save_slot->new_data.jetpack_unlock_level = meta_data_get_jetpack_upgrade_level();
    // Magic numbers: the MD5's 16 bytes, 512 byte blocks (a Xenon sector), and the largest file: a slot's 0x280010 bytes
    // less the 16 bytes rfg.exe keeps free.
    const int with_md5 = len + 0x10;
    const int padding = (0x200 - with_md5 % 0x200) % 0x200;
    const int total = with_md5 + padding;
    if (total < 0x280000)
    {
        const md5_value md5 = console_md5_mem(bytes, total - 0x10);
        memcpy(bytes + total - 0x10, &md5, sizeof(md5));
        game_save_write(save_slot, bytes, total);
        Common_mempool.restore_to_mark(common_mark);
    }
}

// 0x0093F7F0 (:1784)
bool world::process_deserialization()
{
    static int deserialize_list_idx; // function static of the original
    if (deserialize_state == DS_IDLE && sl_flags.pending_save_state && !save_state_start())
    {
        VLIB_CRITICAL_FAILURE();
        if (World.save_file != nullptr)
        {
            cf_close(World.save_file);
            World.save_file = nullptr;
        }
        sl_flags.performing_save_state = false;
        deserialize_list.remove_all();
        deserialize_state = DS_IDLE;
        return false;
    }
    if (deserialize_list.size() < 1)
    {
        return true;
    }
    if (deserialize_state == DS_PENDING)
    {
        deserialize_state = DS_COLLECT;
        deserialize_list_idx = 0;
    }
    world_zone* wz = deserialize_list[deserialize_list_idx];
    bool ok;
    if (deserialize_state == DS_COLLECT)
    {
        ok = process_deserialize_collect(wz);
        deserialize_state = DS_COMPRESS;
    }
    else if (deserialize_state == DS_COMPRESS)
    {
        ok = process_deserialize_compress(wz);
        if (sl_flags.performing_save_state)
        {
            deserialize_list_idx++;
            deserialize_state = deserialize_list.size() <= deserialize_list_idx ? DS_SAVE_FINALIZE : DS_COLLECT;
        }
        else
        {
            destream_zone(wz);
            deserialize_list_idx++;
            if (deserialize_list_idx < deserialize_list.size())
            {
                deserialize_state = DS_COLLECT;
            }
            else
            {
                deserialize_list.remove_all();
                deserialize_state = DS_IDLE;
            }
        }
    }
    else if (deserialize_state == DS_SAVE_FINALIZE)
    {
        ok = save_state_finish();
        deserialize_list.remove_all();
        deserialize_state = DS_IDLE;
    }
    else
    {
        return true;
    }
    if (ok)
    {
        return true;
    }
    if (wz != nullptr)
    {
        process_deserialize_close_zone(wz);
    }
    if (deserialize_pool != nullptr)
    {
        Common_page_pool.release_virtual_mempool(deserialize_pool);
        deserialize_pool = nullptr;
    }
    if (World.save_file != nullptr)
    {
        cf_close(World.save_file);
        World.save_file = nullptr;
    }
    sl_flags.pending_save_state = false;
    sl_flags.performing_save_state = false;
    deserialize_list.remove_all();
    deserialize_state = DS_IDLE;
    return false;
}

// 0x0092DEF0 (:1906)
bool world::process_deserialize_collect(world_zone* wz)
{
    // A 512 KB page pool block: the header image (0x71F0 bytes: zone_header and up to 7280 handles) and the objects.
    deserialize_pool = Common_page_pool.get_virtual_mempool("deserialize_pool", 0x80000);
    zone_header* header = (zone_header*)deserialize_pool->alloc(0x80000, 0);
    if (header != nullptr)
    {
        new (header) zone_header();
    }
    wz->deserialize_header = header;
    wz->deserialize_header_f = cf_open((unsigned char*)header, 0x71F0, "wb", PLATFORM_PC);
    cf_seek(wz->deserialize_header_f, sizeof(zone_header), CF_SEEK_SET);
    wz->deserialize_header->signature = 0x454E4F5A; // "ZONE"
    wz->deserialize_header->version = 36;
    wz->deserialize_header->flags = 4;
    wz->deserialize_f = cf_open((unsigned char*)header + 0x71F0, 0x78E10, "wb", PLATFORM_PC);

    // Every top-level object of this zone (by position) or, for the persistent zone, every persistent one.
    world_zone* persistent = get_world_zone_by_index(15);
    for (int i = 0; i < all_objects.size(); i++)
    {
        object* objp = all_objects[i];
        if (objp == nullptr)
        {
            keen::debug::nativePrintArgs("[world_serialize.cpp] <process_deserialize_collect> get_object returned Nullpointer! Index:%u \n", i);
        }
        if (objp->obj_flags.destroyed || objp->obj_flags.special_lifetime || objp->parent != 0xFFFFFFFF)
        {
            continue;
        }
        // The position's zone, computed in double precision.
        world_zone* pos_zone = nullptr;
        const unsigned int x = (unsigned int)(int)(((double)objp->pos.x + 4088.0) / 511.0);
        const unsigned int y = (unsigned int)(int)((4088.0 - (double)objp->pos.z) / 511.0);
        if (x <= 15 && y <= 15 && y * 16 + x <= 0x100)
        {
            pos_zone = global_zone_grid[y * 16 + x];
        }
        const bool is_persistent = wz == persistent;
        if (is_persistent || wz == pos_zone)
        {
            deserialize_object(pos_zone, objp, false, is_persistent);
        }
    }
    if (wz->deserialize_header_f == nullptr || wz->deserialize_f == nullptr)
    {
        return false;
    }
    return cf_error(wz->deserialize_header_f) == CF_ERROR_NONE && cf_error(wz->deserialize_f) == CF_ERROR_NONE;
}

// 0x00923720 (:2005)
bool world::process_deserialize_compress(world_zone* wz)
{
    compress_stream->compress_init(ZC_BEST_SPEED);
    const unsigned int header_size = cf_tell(wz->deserialize_header_f);
    const unsigned int data_size = cf_tell(wz->deserialize_f);
    unsigned char* out = wz->stored_zone_state.buf;
    wz->stored_zone_state.cur_size = 0;
    unsigned int read = 0;
    unsigned int written = 0;
    if (!compress_stream->compress(out, wz->stored_zone_state.max_size, &written, (unsigned char*)cf_memory_fp(wz->deserialize_header_f), header_size, &read, false))
    {
        keen::debug::nativePrintArgs("[world] Failed to compress save data header.\n");
        VLIB_CRITICAL_FAILURE();
        compress_stream->compress_finish();
        process_deserialize_close_zone(wz);
        if (deserialize_pool != nullptr)
        {
            Common_page_pool.release_virtual_mempool(deserialize_pool);
            deserialize_pool = nullptr;
        }
        return false;
    }
    wz->stored_zone_state.cur_size += written;
    if (!compress_stream->compress(out + written, wz->stored_zone_state.max_size - wz->stored_zone_state.cur_size, &written, (unsigned char*)cf_memory_fp(wz->deserialize_f), data_size, &read, true))
    {
        keen::debug::nativePrintArgs("[world] Failed to compress save data header.\n");
        keen::debug::nativePrintArgs("   Max size : %d\n   sizeToCompress %d\n", wz->stored_zone_state.max_size, data_size);
        VLIB_CRITICAL_FAILURE();
        compress_stream->compress_finish();
        process_deserialize_close_zone(wz);
        if (deserialize_pool != nullptr)
        {
            Common_page_pool.release_virtual_mempool(deserialize_pool);
            deserialize_pool = nullptr;
        }
        return false;
    }
    wz->stored_zone_state.cur_size += written;
    compress_stream->compress_finish();
    process_deserialize_close_zone(wz);
    if (deserialize_pool != nullptr)
    {
        Common_page_pool.release_virtual_mempool(deserialize_pool);
        deserialize_pool = nullptr;
    }
    return true;
}

// 0x0091D140 (:2097)
bool world::process_deserialize_close_zone(world_zone* wz)
{
    if (wz == nullptr)
    {
        return false;
    }
    wz->deserialize_header = nullptr;
    if (wz->deserialize_header_f != nullptr)
    {
        cf_close(wz->deserialize_header_f);
        wz->deserialize_header_f = nullptr;
    }
    if (wz->deserialize_f != nullptr)
    {
        cf_close(wz->deserialize_f);
        wz->deserialize_f = nullptr;
    }
    return true;
}

// 0x0091D190 (:2118)
bool world::process_deserialize_close()
{
    if (deserialize_pool == nullptr)
    {
        return false;
    }
    Common_page_pool.release_virtual_mempool(deserialize_pool);
    deserialize_pool = nullptr;
    return true;
}

// 0x00923920 (:2131)
bool world::process_deserialization_level_shutdown()
{
    for (int i = 0; i < deserialize_list.size(); i++)
    {
        process_deserialize_close_zone(deserialize_list[i]);
    }
    deserialize_list.remove_all();
    if (deserialize_pool != nullptr)
    {
        Common_page_pool.release_virtual_mempool(deserialize_pool);
        deserialize_pool = nullptr;
    }
    deserialize_state = DS_IDLE;
    return true;
}
