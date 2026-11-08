// Original: volition/rfg/code/level/world_restore.cpp
//
// The restoration bookkeeping (pending, automatic and done restorations, the object hierarchy of the zone files), the
// restoration of objects when their zone is serialized and the clean-up around restored objects.
//
// Restoring an object brings a destroyed level object (and its descendants) back from its zone file. Hierarchy (built
// while the zone files are preloaded, restore_cache_in_table) knows every handle's parent, sibling and child, so
// restorations can walk hierarchies whose objects are not streamed. restore_object queues a handle in
// Pending_restorations; each frame restore_do_frame_pre destroys what is to be restored and notes the loaded zones
// involved (Zone_table), and restore_do_frame_post re-serializes those objects from the persistent zone and those
// zones. Objects flagged for automatic restoration (zone_object::flags 4, not persistent) are restored when their zone
// streams in (Auto_restorations). Restored_objects keeps what was restored, so the zone serialization that follows
// can clean up around it (restore_post_zone_serialize_cleanup).

#include "volition/rfg/code/level/world_internal.h"

#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/destruction/rfg_shape_cutter.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/level/rfg_zonex.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include "keen/base/memoryallocator.hpp"
#include "keen/base/trace.hpp"

#include <new>

// Parent/sibling/child links of every object handle in the territory, built from the zone relation tables at level
// init (restore_level_init) so restorations can walk a hierarchy whose objects are not streamed.
// size 0x10
class object_hierarchy_node
{
public:
    object_hierarchy_node(); // 0x0091CF90 (:45)

    unsigned int parent; // 0x00
    unsigned int sibling; // 0x04
    unsigned int child; // 0x08
    unsigned short flags; // 0x0C
};

typedef data_hash_table<unsigned int, object_hierarchy_node, &hash_uint, 65535, unsigned short> hierarchy_table;
typedef hash_table<unsigned int, &uint_hash, 7280, unsigned short> handle_table;

// External linkage, but no header declares it (its element type is private to this file).
hierarchy_table* Hierarchy; // 0x02CCB67C

// ---- globals (world_internal.h)
hash_table<unsigned int,&uint_hash,1024,unsigned short> Auto_restorations(false); // 0x01668B60
hash_table<unsigned int,&uint_hash,3072,unsigned short> Pending_restorations(false); // 0x0166AB68
hash_table<unsigned int,&uint_hash,2048,unsigned short> Restored_objects(false); // 0x01670B70
hash_table<unsigned int,&hash_uint,2048,unsigned short> Restored_shapes(false); // 0x01674BB8
hash_table<unsigned int,&uint_hash,9,unsigned char> Zone_table(false); // 0x01674B78

// 0x0091CF90 (:45)
object_hierarchy_node::object_hierarchy_node()
{
    parent = 0xFFFFFFFF;
    sibling = 0xFFFFFFFF;
    child = 0xFFFFFFFF;
    flags = 0;
}

// The zone index of a handle (upper byte: the zone's grid x and y), 257 or more for none.
static unsigned int handle_zone_index(unsigned int handle)
{
    return ((handle >> 24) & 0xF) * 16 + (handle >> 28);
}

// The hierarchy node of a handle, nullptr if unknown.
static object_hierarchy_node* hierarchy_node(unsigned int handle)
{
    return Hierarchy->get_ptr(handle);
}

// 0x009349F0 (world_restore.cpp:116)
void world::restore_level_init()
{
    Pending_restorations.clear();
    Restored_objects.clear();
    Auto_restorations.clear();
    Zone_table.clear();
    void* memory = Level_mempool.alloc(sizeof(hierarchy_table), 0);
    if (memory == nullptr)
    {
        VLIB_CRITICAL_FAILURE();
        Hierarchy = nullptr;
        return;
    }
    // The original also zeroes the nodes after constructing them (no effect: a node is written when its handle is
    // inserted).
    Hierarchy = new (memory) hierarchy_table(false);
}

// 0x009236A0 (world_restore.cpp:130)
void world::restore_system_init()
{
    Pending_restorations.clear();
    Restored_objects.clear();
    Auto_restorations.clear();
    Zone_table.clear();
    Hierarchy = nullptr;
}

// Writes the keys of a table as "u16 count, u32 keys" (restore_system_save, inlined twice).
template<typename TABLE>
static void save_handles(TABLE& table, cfile* file)
{
    const unsigned int count_pos = cf_tell(file);
    cf_seek(file, 2, CF_SEEK_CUR);
    unsigned short count = 0;
    unsigned int handle;
    if (table.get_first_key(handle))
    {
        do
        {
            cf_write_uint32(handle, file);
            count++;
        } while (table.get_next_key(handle, handle));
    }
    cf_seek(file, count_pos, CF_SEEK_SET);
    cf_write_uint16(count, file);
    cf_seek(file, 0, CF_SEEK_END);
}

// 0x0092D110 (world_restore.cpp:139)
void world::restore_system_save(cfile* file)
{
    save_handles(Pending_restorations, file);
    save_handles(Restored_objects, file);
}

// 0x00934740 (no callers in retail; debug check)
bool verify_hierarchy()
{
    // Notes the zones (handle upper byte) of every parent, sibling or child link that points at a handle missing from
    // Hierarchy; true when there are none. The original uses add (no duplicate check); insert gives the same result.
    hash_table<unsigned int, &hash_uint, 257, unsigned short> missing_zones(false);
    unsigned int handle;
    if (Hierarchy->get_first_key(handle))
    {
        object_hierarchy_node* node = hierarchy_node(handle);
        while (node != nullptr)
        {
            if (node->parent != 0xFFFFFFFF && !Hierarchy->exists(node->parent))
            {
                missing_zones.insert(node->parent & 0xFF000000);
            }
            if (node->sibling != 0xFFFFFFFF && !Hierarchy->exists(node->sibling))
            {
                missing_zones.insert(node->sibling & 0xFF000000);
            }
            if (node->child != 0xFFFFFFFF && !Hierarchy->exists(node->child))
            {
                missing_zones.insert(node->child & 0xFF000000);
            }
            if (!Hierarchy->get_next_key(handle, handle))
            {
                break;
            }
            node = hierarchy_node(handle);
        }
    }
    // The original walks the missing zones here with an empty body (a stripped debug print).
    unsigned int zone;
    if (missing_zones.get_first_key(zone))
    {
        while (missing_zones.get_next_key(zone, zone))
        {
        }
    }
    return missing_zones.get_count() == 0;
}

// 0x00934AC0 (world_restore.cpp:181)
void world::restore_system_load(cfile* file)
{
    for (unsigned short count = cf_read_uint16(file); count != 0; count--)
    {
        Pending_restorations.insert(cf_read_uint32(file));
    }
    for (unsigned short count = cf_read_uint16(file); count != 0; count--)
    {
        Restored_objects.insert(cf_read_uint32(file));
    }
}

// 0x0092D250 (world_restore.cpp:206)
void remove_descendants_from_auto_restore(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    object_hierarchy_node* node = hierarchy_node(handle);
    if (node == nullptr)
    {
        return;
    }
    for (unsigned int child = node->child; child != 0xFFFFFFFF;)
    {
        object_hierarchy_node* child_node = hierarchy_node(child);
        if (child_node == nullptr)
        {
            break;
        }
        remove_descendants_from_auto_restore(child);
        Auto_restorations.remove(child);
        child = child_node->sibling;
    }
}

// 0x00934C20 (world_restore.cpp:234)
bool world::restore_cache_in_table(data_hash_table<unsigned int,unsigned int,&uint_hash,7280,unsigned short>* table, zone_object* zobj, unsigned char* objects)
{
    // Records the zone object's links; an automatically restored object (flags 4, not persistent) whose ancestors are
    // not automatic yet becomes one, replacing its descendants.
    if (zobj == nullptr)
    {
        return false;
    }
    object_hierarchy_node node;
    node.parent = zobj->parent;
    node.sibling = zobj->sibling;
    node.child = zobj->child;
    node.flags = zobj->flags;
    Hierarchy->insert_key_data(zobj->handle, node);
    if ((zobj->flags & 4) != 0 && (zobj->flags & 1) == 0)
    {
        bool ancestor_restores = false;
        for (unsigned int parent = zobj->parent; parent != 0xFFFFFFFF;)
        {
            object_hierarchy_node* parent_node = hierarchy_node(parent);
            if (parent_node == nullptr)
            {
                break;
            }
            if (Auto_restorations.exists(parent))
            {
                ancestor_restores = true;
                break;
            }
            parent = parent_node->parent;
        }
        remove_descendants_from_auto_restore(zobj->handle);
        if (!ancestor_restores)
        {
            Auto_restorations.insert(zobj->handle);
        }
    }
    return true;
}

// 0x0092D2B0 (world_restore.cpp:287)
void restore_destroy_movers(hash_table<unsigned int,&hash_uint,2048,unsigned short>& shape_table)
{
    // Destroys the movers with mover_flags 4 whose shape was restored.
    const int num = World.get_num_objects<object_mover>();
    for (int i = 0; i < num; i++)
    {
        object_mover* omp = World.get_object<object_mover>(i);
        if ((omp->mover_flags & 4) != 0 && shape_table.exists(omp->shape_uid))
        {
            World.destroy_object(omp, false);
        }
    }
}

// 0x0091CFB0 (world_restore.cpp:302) (no callers in retail; restore_post_zone_serialize_cleanup_helper inlines it)
void restore_clean_up_object(hash_table<unsigned int,&hash_uint,2048,unsigned short>& restored_shapes, object* objp)
{
    // A restored building: note its shape, reconnect the roads over it (mover type flag 0x10) and drop its pending
    // navmesh hole requests.
    if (objp == nullptr || objp->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return;
    }
    rfg_mover* rfgmp = (rfg_mover*)objp;
    restored_shapes.insert(rfgmp->shape_uid);
    if ((rfgmp->mover_type_bit_flags & 0x10) != 0)
    {
        object_road_rejoin_closest_road_at_point(rfgmp->pos);
    }
    dynamic_hole_requests_remove_for_building(rfgmp);
}

// 0x0093D9F0 (world_restore.cpp:327)
void restore_destroy_object(hash_table<unsigned int,&hash_uint,2048,unsigned short>& restored_shapes, unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    restored_shapes.insert(handle);
    object** entry = World.object_handle_hash.get_ptr(handle);
    if (entry != nullptr && *entry != nullptr && !(*entry)->obj_flags.stub)
    {
        World.destroy_object(*entry, false);
    }
}

// 0x0093DA50 (world_restore.cpp:346)
void restore_destroy_object_recursive(hash_table<unsigned int,&hash_uint,2048,unsigned short>& restored_shapes, unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    object_hierarchy_node* node = hierarchy_node(handle);
    if (node == nullptr)
    {
        return;
    }
    for (unsigned int child = node->child; child != 0xFFFFFFFF;)
    {
        object_hierarchy_node* child_node = hierarchy_node(child);
        if (child_node == nullptr)
        {
            break;
        }
        restore_destroy_object_recursive(restored_shapes, child);
        child = child_node->sibling;
    }
    restore_destroy_object(restored_shapes, handle);
}

// 0x00934CE0 (world_restore.cpp:371)
bool world::restore_object(unsigned int handle)
{
    if (!Hierarchy->exists(handle))
    {
        return false;
    }
    Pending_restorations.insert(handle);
    return true;
}

// 0x0092D370 (world_restore.cpp:389)
bool world::restore_object_is_pending(unsigned int handle) const
{
    return Pending_restorations.exists(handle);
}

// 0x00923700 (world_restore.cpp:396)
bool world::restore_is_pending() const
{
    return Pending_restorations.get_count() != 0;
}

// 0x0092D3B0 (world_restore.cpp:407)
bool world::restore_object_is_immediate(unsigned int handle) const
{
    // Persistent objects and objects of loaded zones are restored right away.
    object_hierarchy_node* node = hierarchy_node(handle);
    if (node == nullptr)
    {
        return false;
    }
    if ((node->flags & 1) != 0)
    {
        return true;
    }
    if (handle == 0xFFFFFFFF)
    {
        return false;
    }
    const unsigned int index = handle_zone_index(handle);
    return index < 257 && global_zone_grid[index] != nullptr && global_zone_grid[index]->state == WZS_LOADED;
}

// 0x0092D410 (world_restore.cpp:433)
bool restore_object_just_restored(unsigned int handle)
{
    // The object or one of its ancestors was restored. Runtime handles (third byte 0xFF) are never restored.
    if (((handle >> 16) & 0xFF) == 0xFF)
    {
        return false;
    }
    while (handle != 0xFFFFFFFF)
    {
        object_hierarchy_node* node = hierarchy_node(handle);
        if (node == nullptr)
        {
            return false;
        }
        if (Restored_objects.exists(handle))
        {
            return true;
        }
        handle = node->parent;
    }
    return false;
}

// 0x00934D10 (world_restore.cpp:455)
int restore_pending_pop_helper(hash_table<unsigned int,&uint_hash,7280,unsigned short>* restore_table, unsigned int*& restore_list, unsigned int handle, bool is_persistent_zone)
{
    // Adds the handle and its descendants to the zone's restore table (and list); from the persistent zone a
    // non-persistent object is postponed to its own zone.
    object_hierarchy_node* node = hierarchy_node(handle);
    if (node == nullptr)
    {
        keen::debug::nativePrintArgs("[world_restore.cpp] <restore_pending_pop_helper> Failed to get node pointer! Handle:%u \n", handle);
        return 0;
    }
    if (is_persistent_zone && (node->flags & 1) == 0)
    {
        Pending_restorations.insert(handle);
        return 0;
    }
    int count = 0;
    if (is_persistent_zone == ((node->flags & 1) != 0) && restore_table->insert(handle))
    {
        if (restore_list != nullptr)
        {
            *restore_list++ = handle;
        }
        count = 1;
    }
    for (unsigned int child = node->child; child != 0xFFFFFFFF;)
    {
        object_hierarchy_node* child_node = hierarchy_node(child);
        if (child_node == nullptr)
        {
            break;
        }
        count += restore_pending_pop_helper(restore_table, restore_list, child, is_persistent_zone);
        child = child_node->sibling;
    }
    return count;
}

// 0x0092D470 (world_restore.cpp:507)
void remove_descendants_from_restored_table(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    object_hierarchy_node* node = hierarchy_node(handle);
    if (node == nullptr)
    {
        return;
    }
    for (unsigned int child = node->child; child != 0xFFFFFFFF;)
    {
        object_hierarchy_node* child_node = hierarchy_node(child);
        if (child_node == nullptr)
        {
            break;
        }
        remove_descendants_from_restored_table(child);
        Restored_objects.remove(child);
        child = child_node->sibling;
    }
}

// Notes a restored handle (replacing its descendants) unless an ancestor was restored already (inlined in the pops).
static void note_restored(unsigned int handle)
{
    if (!restore_object_just_restored(handle))
    {
        remove_descendants_from_restored_table(handle);
        Restored_objects.insert(handle);
    }
}

// 0x00934DE0 (world_restore.cpp:531)
int world::restore_pending_pop_handle(unsigned int handle, hash_table<unsigned int,&uint_hash,7280,unsigned short>* restore_table, unsigned int*& restore_list, bool is_persistent_zone)
{
    const int count = restore_pending_pop_helper(restore_table, restore_list, handle, is_persistent_zone);
    note_restored(handle);
    return count;
}

// 0x00934E30 (world_restore.cpp:555)
int world::restore_pending_pop(hash_table<unsigned int,&uint_hash,7280,unsigned short>* restore_table, unsigned int* restore_list, unsigned short zone_gid, bool include_auto_restore)
{
    // Moves the pending restorations of a zone (and, when asked, its automatic ones) into its restore table.
    const bool is_persistent_zone = (zone_gid & 0xF000) == 0xF000 && (zone_gid & 0x0F00) == 0;
    int count = 0;
    unsigned int handle;
    if (include_auto_restore && !is_persistent_zone && Auto_restorations.get_first_key(handle))
    {
        do
        {
            const unsigned int current = handle;
            if ((current >> 24) == (unsigned int)(zone_gid >> 8))
            {
                count += restore_pending_pop_helper(restore_table, restore_list, current, false);
                note_restored(current);
            }
            if (!Auto_restorations.get_next_key(handle, current))
            {
                break;
            }
        } while (true);
    }
    if (!Pending_restorations.get_first_key(handle))
    {
        return count;
    }
    for (;;)
    {
        const unsigned int current = handle;
        unsigned int next = 0xFFFFFFFF;
        const bool has_next = Pending_restorations.get_next_key(next, current);
        object_hierarchy_node* node = hierarchy_node(current);
        if (node != nullptr && ((node->flags & 1) != 0) == is_persistent_zone && (is_persistent_zone || ((current >> 16 ^ zone_gid) & 0xFF00) == 0))
        {
            count += restore_pending_pop_helper(restore_table, restore_list, current, is_persistent_zone);
            note_restored(current);
            Pending_restorations.remove(current);
        }
        if (!has_next)
        {
            return count;
        }
        handle = next;
    }
}

// 0x00940230 (world_restore.cpp:606)
bool world::restore_serialize_zone(world_zone* wz, zone_file* orig_zone)
{
    // Creates the zone's pending restorations from its zone file (through the relation table: handle -> offset).
    keen::MemoryAllocator* allocator = rfg_get_cpu_scratch_allocator();
    if (wz == nullptr)
    {
        return false;
    }
    handle_table* restore_table = (handle_table*)allocator->allocate(sizeof(handle_table), 16, 0, nullptr);
    if (restore_table != nullptr)
    {
        new (restore_table) handle_table(false);
    }
    unsigned int* restore_list = (unsigned int*)allocator->allocate(7280 * 4, 16, 0, nullptr);
    unsigned int* list_end = restore_list;
    const int num_restored = restore_pending_pop(restore_table, list_end, wz->gid, false);
    if (num_restored > 0)
    {
        zone_object** zobj_list = (zone_object**)allocator->allocate(num_restored * 4, 16, 0, nullptr);
        object** stub_list = (object**)allocator->allocate(num_restored * 4, 16, 0, nullptr);
        int num_stubs = 0;
        for (int i = 0; i < num_restored; i++)
        {
            unsigned int* offset = orig_zone->relation_data.get_ptr(restore_list[i]);
            if (offset == nullptr || *offset == 0xFFFFFFFF)
            {
                continue;
            }
            zone_object* zobjp = (zone_object*)(orig_zone->objects + *offset);
            if (zobjp->handle != restore_list[i])
            {
                keen::debug::nativePrintArgs("[world_restore.cpp] <restore_serialize_zone> zone_object has not the correct Handle! Zone_object:%u Correct:%u \n", zobjp->handle, restore_list[i]);
                continue;
            }
            object* stub = visit_stub(zobjp);
            if (stub != nullptr)
            {
                stub->all_index = (unsigned short)num_stubs;
                zobj_list[num_stubs] = zobjp;
                stub_list[num_stubs] = stub;
                num_stubs++;
            }
        }
        serialize_stub_list(num_stubs, stub_list, zobj_list);
        restore_post_zone_serialize_cleanup(orig_zone->objects, wz->gid);
        restore_table->~handle_table();
        allocator->free(zobj_list);
        allocator->free(stub_list);
        allocator->free(restore_list);
        allocator->free(restore_table);
        return true;
    }
    restore_table->~handle_table();
    allocator->free(restore_list);
    allocator->free(restore_table);
    return true;
}

// 0x0093DAB0 (world_restore.cpp:699)
void world::restore_do_frame_pre()
{
    // Destroys what is about to be restored and notes the loaded zones involved.
    if (sl_flags.pending_new_game)
    {
        return;
    }
    Zone_table.clear();
    Restored_shapes.clear();
    unsigned int handle;
    if (!Pending_restorations.get_first_key(handle))
    {
        return;
    }
    do
    {
        const unsigned int current = handle;
        restore_destroy_object_recursive(Restored_shapes, current);
        if (current != 0xFFFFFFFF)
        {
            const unsigned int index = handle_zone_index(current);
            if (index < 257 && global_zone_grid[index] != nullptr && global_zone_grid[index]->state == WZS_LOADED)
            {
                Zone_table.insert(global_zone_grid[index]->gid);
            }
        }
        if (!Pending_restorations.get_next_key(handle, current))
        {
            break;
        }
    } while (true);
}

// 0x00940430 (world_restore.cpp:733)
void world::restore_do_frame_post()
{
    // Restores pending objects from the persistent zone and from the zones noted by restore_do_frame_pre.
    world_zone* persistent = World.global_zone_grid[15];
    if (sl_flags.pending_new_game || persistent == nullptr)
    {
        return;
    }
    char filename[64];
    cf_add_ext(filename, sizeof(filename), persistent->name, ".rfgzone_pc", false);
    zone_file* persistent_zf = zone_find(filename);
    if (persistent_zf == nullptr)
    {
        return;
    }
    restore_serialize_zone(persistent, persistent_zf);
    unsigned int gid;
    if (Zone_table.get_first_key(gid))
    {
        do
        {
            const unsigned int current = gid;
            world_zone* wz = World.get_world_zone_by_grid_id((unsigned short)current);
            if (wz != nullptr)
            {
                cf_add_ext(filename, sizeof(filename), wz->name, ".rfgzone_pc", false);
                zone_file* zf = zone_find(filename);
                if (zf != nullptr)
                {
                    restore_serialize_zone(wz, zf);
                }
            }
            if (!Zone_table.get_next_key(gid, current))
            {
                break;
            }
        } while (true);
    }
    Zone_table.clear();
}

// ---- restore_clean_box callbacks: destroy what overlaps a restored object unless it was restored itself. They never
// collect anything (always false).

// 0x0092D4D0 (world_restore.cpp:784)
bool restore_clean_box_mover_cb(object* hit_objp, unsigned int, const void*)
{
    // Only movers with mover_flags 4 or 0x40000.
    const unsigned int flags = ((object_mover*)hit_objp)->mover_flags;
    if (((flags & 4) != 0 || (flags & 0x40000) != 0) && !restore_object_just_restored(hit_objp->get_handle()))
    {
        World.destroy_object(hit_objp, false);
    }
    return false;
}

// 0x0092D510 (world_restore.cpp:808)
bool restore_clean_box_npc_cb(object* hit_objp, unsigned int, const void*)
{
    if (!restore_object_just_restored(hit_objp->get_handle()))
    {
        World.destroy_object(hit_objp, false);
    }
    return false;
}

// 0x0092D540 (world_restore.cpp:829)
bool restore_clean_box_vehicle_cb(object* hit_objp, unsigned int, const void*)
{
    // Spares the local player's vehicle.
    if (!restore_object_just_restored(hit_objp->get_handle()))
    {
        player* pp = player_get_local_player();
        if (pp == nullptr || pp->vehicle_handle != hit_objp->get_handle())
        {
            World.destroy_object(hit_objp, false);
        }
    }
    return false;
}

// 0x0092D580 (world_restore.cpp:860)
bool restore_clean_box_shape_cutter_cb(object* hit_objp, unsigned int, const void*)
{
    if (!restore_object_just_restored(hit_objp->get_handle()))
    {
        World.destroy_object(hit_objp, false);
    }
    return false;
}

// 0x0092D5B0 (world_restore.cpp:875)
bool restore_clean_box_effect_cb(object* hit_objp, unsigned int, const void*)
{
    // Of the object effects only explosions go.
    if (!restore_object_just_restored(hit_objp->get_handle()) && (hit_objp->get_obj_type() != OT_EFFECT || ((object_effect*)hit_objp)->get_category() == EFFECTS_CATEGORY_EXPLOSION))
    {
        World.destroy_object(hit_objp, false);
    }
    return false;
}

// 0x0092D5F0 (world_restore.cpp:892)
bool restore_clean_box_navpoint_cb(object* hit_objp, unsigned int, const void*)
{
    // Only dynamic (not original), unparented navpoints.
    if (!restore_object_just_restored(hit_objp->get_handle()) && !hit_objp->is_original() && hit_objp->get_parent() == 0xFFFFFFFF)
    {
        World.destroy_object(hit_objp, false);
    }
    return false;
}

// 0x0092D630 (world_restore.cpp:912)
void restore_clean_box(bbox& clean_box)
{
    // The original passes the address of its clean_box argument as the 1-entry output buffer; the callbacks never
    // collect, so nothing is written to it.
    unsigned int unused;
    World.collect_objects<object_mover>(clean_box.min, clean_box.max, &unused, 1, restore_clean_box_mover_cb, nullptr);
    World.collect_objects<npc>(clean_box.min, clean_box.max, &unused, 1, restore_clean_box_npc_cb, nullptr);
    World.collect_objects<vehicle>(clean_box.min, clean_box.max, &unused, 1, restore_clean_box_vehicle_cb, nullptr);
    World.collect_objects<shape_cutter>(clean_box.min, clean_box.max, &unused, 1, restore_clean_box_shape_cutter_cb, nullptr);
    World.collect_objects<object_effect>(clean_box.min, clean_box.max, &unused, 1, restore_clean_box_effect_cb, nullptr);
    // Navpoints within 1 m of the box as well.
    const vector bmin(clean_box.min.x - 1.0f, clean_box.min.y - 1.0f, clean_box.min.z - 1.0f);
    const vector bmax(clean_box.max.x + 1.0f, clean_box.max.y + 1.0f, clean_box.max.z + 1.0f);
    World.collect_objects<navpoint>(bmin, bmax, &unused, 1, restore_clean_box_navpoint_cb, nullptr);
}

// 0x0093C2F0 (world_restore.cpp:970)
bool restore_post_zone_serialize_cleanup_helper(hash_table<unsigned int,&hash_uint,2048,unsigned short>& restored_shapes, bbox& root_box, unsigned int handle, unsigned short zone_gid)
{
    // Cleans up around the restored object and its descendants (each one's box when it sticks out of the root's).
    // Returns true when the whole hierarchy is in place: every object exists, and belongs to the serialized zone or is
    // persistent with its original zone serialized.
    bool complete = ((handle >> 16 ^ zone_gid) & 0xFF00) == 0;
    if (handle == 0xFFFFFFFF)
    {
        return false;
    }
    object** entry = World.object_handle_hash.get_ptr(handle);
    if (entry == nullptr)
    {
        return false;
    }
    object* objp = *entry;
    if (objp == nullptr || objp->obj_flags.stub)
    {
        return false;
    }
    object_hierarchy_node* node = hierarchy_node(handle);
    if (node == nullptr)
    {
        return false;
    }
    if (!complete && (node->flags & 1) != 0 && World.objects_original_zone_is_serialized(handle))
    {
        complete = true;
    }
    for (unsigned int child = node->child;;)
    {
        object_hierarchy_node* child_node = hierarchy_node(child);
        if (child == 0xFFFFFFFF || child_node == nullptr)
        {
            break;
        }
        // Every child is visited (no short circuit).
        complete &= restore_post_zone_serialize_cleanup_helper(restored_shapes, root_box, child, zone_gid);
        child = child_node->sibling;
    }
    bbox box;
    objp->get_bbox(box.min, box.max);
    // Written as !(a >= b): the original's COMISD/JC also cleans when a compare is unordered (NaN).
    if (!(box.min.x >= root_box.min.x) || !(box.min.y >= root_box.min.y) || !(box.min.z >= root_box.min.z)
        || !(root_box.max.x >= box.max.x) || !(root_box.max.y >= box.max.y) || !(root_box.max.z >= box.max.z))
    {
        restore_clean_box(box);
    }
    // restore_clean_up_object, inlined.
    if (objp->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        rfg_mover* rfgmp = (rfg_mover*)objp;
        restored_shapes.insert(rfgmp->shape_uid);
        if ((rfgmp->mover_type_bit_flags & 0x10) != 0)
        {
            object_road_rejoin_closest_road_at_point(rfgmp->pos);
        }
        dynamic_hole_requests_remove_for_building(rfgmp);
    }
    return complete;
}

// 0x0093C4C0 (world_restore.cpp:1039)
void world::restore_post_zone_serialize_cleanup(unsigned char* objects, unsigned short zone_gid)
{
    // For every restored object that exists now: clean up what overlaps it (and keep it noted only while its whole
    // hierarchy is in place); restored objects that do not exist yet stay pending. Movers whose shapes were restored are
    // destroyed (they are recreated from the restored objects).
    hash_table<unsigned int, &hash_uint, 2048, unsigned short> restored_shapes(false);
    farray<unsigned int,2048> done;
    unsigned int handle;
    if (Restored_objects.get_first_key(handle))
    {
        do
        {
            const unsigned int current = handle;
            object** entry = current != 0xFFFFFFFF ? World.object_handle_hash.get_ptr(current) : nullptr;
            object* objp = entry != nullptr ? *entry : nullptr;
            if (objp == nullptr || objp->obj_flags.stub)
            {
                Pending_restorations.insert(current);
            }
            else
            {
                bbox box;
                objp->get_bbox(box.min, box.max);
                const bool complete = restore_post_zone_serialize_cleanup_helper(restored_shapes, box, current, zone_gid);
                restore_clean_box(box);
                if (complete)
                {
                    done.add(current);
                }
            }
            if (!Restored_objects.get_next_key(handle, current))
            {
                break;
            }
        } while (true);
    }
    if (Pending_restorations.get_first_key(handle))
    {
        do
        {
            const unsigned int current = handle;
            Restored_objects.remove(current);
            if (!Pending_restorations.get_next_key(handle, current))
            {
                break;
            }
        } while (true);
    }
    if (restored_shapes.get_count() != 0)
    {
        restore_destroy_movers(restored_shapes);
    }
    for (int i = 0; i < done.size(); i++)
    {
        Restored_objects.remove(done[i]);
    }
}
