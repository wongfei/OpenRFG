// Original: volition/rfg/code/objects/object_zone.cpp
//
// obj_zone, the object of a streamed territory cell (decompiled). Created from the zone's .rfgzone_pc (or, for files
// before version 26, from obj_zone_props), it finds the cell's terrain (terrain_find on "<terrain_file_name>.cterrain_pc")
// and instantiates it: the terrain's physics (height field body 255.5 units off the cell centre, stitch pieces,
// invisible barriers), the path finding layers, the occluders, its district and spawn settings and an AABB phantom
// below the lowest terrain point (one of 9, Zone_phantoms). Its bounding box is the cell (511 x 1022 x 511).
// get_effect_material reads the terrain layer map (512x512, 2 bits per texel, 4 materials) for contacts with the
// height field body.
//
// LTCG: request_aabb_phantom takes the zone in ECX and the terrain data in EAX.

#include "volition/rfg/code/objects/object_zone.h"

#include "volition/render_lib/code/rl_aabb.h"
#include "volition/render_lib/code/rl_terrain_occluder.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/game_render_terrain.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_cm_manager.h"
#include "volition/rfg/code/level/rfg_terrainx.h"
#include "volition/rfg/code/level/rfg_zonex_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/spawn/ambient_spawn_info.h"
#include "volition/rfg/code/spawn/ambient_spawn_manager.h"
#include "volition/rfg/code/spawn/spawn_resource_data.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/memory/object_pool.h"

#include <float.h>
#include <math.h>
#include <string.h>

// ---- file-static globals
static object_pool<obj_zone,9,1> obj_zonepool; // 0x03042670
static obj_zone_phatom Zone_phantoms[9]; // 0x03187220
static obj_class_instantiator obj_zone_instantiator("obj_zone", alloc_obj_zone, types_obj_zone, bpo_index_obj_zone, classname_obj_zone, obj_zone::pool_remaining, 0xF10, 9); // 0x034264FC

// 0x00A1E0A0 (:38)
obj_zone_phatom::obj_zone_phatom()
{
    handle = nullptr;
    in_use = false;
}

// 0x00A71230 (:71)
static int request_aabb_phantom(obj_zone* zone, terrain_data* data)
{
    // The phantom spans the cell (the whole broadphase in multiplayer) from the bottom of the world to 10 units below
    // the terrain: the height field body's AABB, else the lowest low LOD patch.
    float min_y = FLT_MAX;
    vector body_min;
    vector body_max;
    if (havok_body_get_aabb(zone->havok_handle, zone->pos, zone->orient, body_min, body_max))
    {
        min_y = body_min.y;
    }
    else if (data != nullptr && data->m_low_lod_patches_p.to_pointer() != nullptr)
    {
        for (int i = 0; i < 9; i++)
        {
            const float patch_min_y = data->m_low_lod_patches_p[i].m_aabb_min.y;
            if (0.0f <= min_y - patch_min_y)
            {
                min_y = patch_min_y;
            }
        }
    }
    vector broadphase_min;
    vector broadphase_max;
    havok_get_broadphase_extents(broadphase_min, broadphase_max);
    vector bmin;
    vector bmax;
    if (Multiplayer || wrecking_crew_is_active())
    {
        bmin.x = broadphase_min.x + 1.0f;
        bmin.z = broadphase_min.z + 1.0f;
        bmax.x = broadphase_max.x - 1.0f;
        bmax.z = broadphase_max.z - 1.0f;
    }
    else
    {
        zone->get_bbox(bmin, bmax);
    }
    bmax.y = min_y - 10.0f;
    bmin.y = broadphase_min.y + 1.0f;
    for (int i = 0; i < 9; i++)
    {
        obj_zone_phatom& phantom = Zone_phantoms[i];
        if (!phantom.in_use)
        {
            if (phantom.handle != nullptr)
            {
                havok_aabb_phantom_update_aabb(phantom.handle, bmin, bmax);
            }
            else
            {
                phantom.handle = havok_aabb_phantom_create(bmin, bmax);
            }
            phantom.in_use = true;
            return i;
        }
    }
    return -1;
}

// 0x00A1E0B0 (:133)
static void release_aabb_phantom(int index)
{
    if ((unsigned int)index < 9)
    {
        Zone_phantoms[index].in_use = false;
    }
}

// 0x00A1E100 (:150)
unsigned int obj_zone::get_classname_hash()
{
    return obj_zone_instantiator.this_hash;
}

// 0x00A40E30 (:150)
int obj_zone::pool_remaining()
{
    return obj_zonepool.get_free();
}

// 0x00A1E0F0 (:150)
const char* classname_obj_zone()
{
    return "obj_zone";
}

// 0x00A715A0 (:150)
object* alloc_obj_zone()
{
    return obj_zonepool.alloc(0);
}

// 0x00A1E0D0 (:150)
void types_obj_zone(signed char& type, signed char& subtype)
{
    type = OT_ZONE;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A95330 (:150)
void obj_zone::pool_free()
{
    obj_zonepool.free(this);
}

// 0x00A1E0E0 (:150)
int bpo_index_obj_zone()
{
    return 1;
}

// 0x00A715B0 (:151)
obj_zone::obj_zone()
{
}

// 0x00A71610
obj_zone::~obj_zone()
{
}

// 0x00A1E110 (:156)
void obj_zone::get_bbox(vector& bmin, vector& bmax) const
{
    bmin.x = local_bmin.x + pos.x;
    bmin.y = local_bmin.y + pos.y;
    bmin.z = local_bmin.z + pos.z;
    bmax.x = pos.x + local_bmax.x;
    bmax.y = local_bmax.y + pos.y;
    bmax.z = local_bmax.z + pos.z;
}

// 0x00A1E210 (:162)
void obj_zone::get_local_bbox(vector& bmin, vector& bmax) const
{
    bmin = local_bmin;
    bmax = local_bmax;
}

// 0x00AADA50 (:176)
bool obj_zone::serialize(obj_prop_block& pblock, bool load)
{
    bool result = object::serialize(pblock, load);
    if (!result)
    {
        return result;
    }
    if (!load)
    {
        return true;
    }
    // A zone at the top of a hierarchy (or under a persistent object) is persistent.
    amb_spawn_info = nullptr;
    const unsigned int parent_handle = get_parent();
    if (parent_handle == 0xFFFFFFFF)
    {
        obj_flags.persistent = 1;
    }
    else
    {
        object* parent_objp = World.object_from_handle(parent_handle);
        if (parent_objp != nullptr && parent_objp->obj_flags.persistent)
        {
            obj_flags.persistent = 1;
        }
    }
    district_handle = 0xFFFFFFFF;
    district_handle = district_handle_from_pos(pos);
    if (district_handle == 0xFFFFFFFF)
    {
        district_handle = district_get_default();
    }
    char name[256];
    if (pblock.get_optional(ZONE_PROP_SPAWN_INFO, &name[0]))
    {
        amb_spawn_info = amb_spawn_info_from_name(name);
    }
    else
    {
        amb_spawn_info = nullptr;
    }
    if (pblock.get_optional(ZONE_PROP_SPAWN_RESOURCE, &name[0]))
    {
        spawn_res_data = spawn_res_data_find(name);
    }
    else
    {
        spawn_res_data = spawn_res_data_default();
    }
    char terrain_name[256];
    pblock.get(ZONE_PROP_TERRAIN_NAME, &terrain_name[0]);
    World.set_object_name(this, terrain_name);
    float speed = 50.0f;
    pblock.get_optional(ZONE_PROP_WIND_MIN_SPEED, speed);
    wind_min_speed = speed;
    speed = 80.0f;
    pblock.get_optional(ZONE_PROP_WIND_MAX_SPEED, speed);
    wind_max_speed = speed;

    // The terrain of the cell.
    char terrain_filename[64];
    cf_add_ext(terrain_filename, sizeof(terrain_filename), terrain_name, ".cterrain_pc", false);
    terrain = terrain_find(terrain_filename);
    if (terrain == nullptr)
    {
        return false;
    }
    terrain_instantiate(terrain, this);
    terrain_data* data = terrain->terrain_data_p;

    // Path finding layers (on foot and for vehicles) with their navigation mesh shapes.
    zone_pfg.pfg = nullptr;
    zone_pfg.pfg_shape = 0xFFFFFFFF;
    zone_pfg.pfg_havok_handle = 0xFFFFFFFF;
    zone_pfg.vehicle_pfg = nullptr;
    zone_pfg.vehicle_pfg_shape = 0xFFFFFFFF;
    zone_pfg.vehicle_pfg_havok_handle = 0xFFFFFFFF;
    terrain_pf_data* pf = data->m_pf_data_p.to_pointer();
    if (pf->pf_data_block_size != 0)
    {
        zone_pfg.pfg = (bb_pfg_layer*)pf->pf_data_block.to_pointer();
        zone_pfg.pfg_shape = havok_create_navmesh_shape(pf->pf_mopp_block.to_pointer(), zone_pfg.pfg);
    }
    if (pf->vehicle_pf_data_block_size != 0)
    {
        zone_pfg.vehicle_pfg = (bb_pfg_layer*)pf->vehicle_pf_data_block.to_pointer();
        zone_pfg.vehicle_pfg_shape = havok_create_navmesh_shape(pf->vehicle_pf_mopp_block.to_pointer(), zone_pfg.vehicle_pfg);
    }
    rfg_pf_instantiate(this);
    rfg_zonex_get_zone_bbox(ZERO_VECTOR, local_bmin, local_bmax);

    // Occluders: vertical walls between two points, from min_y to max_y.
    m_occluders.remove_all();
    for (int i = 0; i < data->m_num_occluders; i++)
    {
        const terrain_occluder& occluder_data = data->m_occluders[i];
        rl_terrain_occluder* occluder = rl_terrain_occluder::create();
        occluder->set_height_base(occluder_data.min_y);
        occluder->set_height(occluder_data.max_y);
        occluder->set_point1(occluder_data.pt1);
        occluder->set_point2(occluder_data.pt2);
        vector bmin;
        vector bmax;
        bbox_create_from_points(bmin, bmax, vector(occluder_data.pt1.x, occluder_data.min_y, occluder_data.pt1.y), vector(occluder_data.pt2.x, occluder_data.max_y, occluder_data.pt2.y));
        occluder->set_world_aabb(rl_aabb(bmin, bmax));
        occluder->set_position(vector((bmax.x + bmin.x) * 0.5f, (bmin.y + bmax.y) * 0.5f, (bmin.z + bmax.z) * 0.5f));
        occluder->set_rotation(IDENTITY_MATRIX);
        occluder->set_bounding_volume_dirty(false);
        game_render_scene_update(occluder);
        m_occluders.add(occluder);
    }

    // Physics: the height field (cm index from the terrain), the stitch pieces and the invisible barriers.
    physics_create_info pci;
    pci.flags |= 0x1D;
    pci.simulation_quality_type = HK_COLLIDABLE_QUALITY_FIXED;
    pci.motion_type = 5;
    pci.precalc_tensor.make_identity();
    pci.precalc_center_of_mass.zero();
    pci.mass = 1.0f;
    heightfield_shape = data->m_shape_handle;
    pci.obj_ptr = this;
    if (heightfield_shape == 0xFFFFFFFF)
    {
        heightfield_shape = 0xFFFFFFFF;
        stitch_piece_havok_handle = 0xFFFFFFFF;
    }
    else
    {
        // The height field's origin is the cell corner.
        pci.pos = vector(pos.x - 255.5f, pos.y - 0.0f, pos.z - 255.5f);
        pci.shape_type = HAVOK_SHAPE_TYPE_WORLD;
        pci.collision_type = COLLIDE_WORLD;
        pci.cm_index = heightfield_shape;
        pci.orient.make_identity();
        pci.size = vector(local_bmax.x - local_bmin.x, local_bmax.y - local_bmin.y, local_bmax.z - local_bmin.z);
        pci.friction = 1.0f;
        pci.restitution = 0.0f;
        havok_handle = havok_body_create(pci);
        if (data->m_stitch_piece_cm_index != 0xFFFFFFFF)
        {
            pci.pos = pos;
            pci.orient.make_identity();
            pci.cm_index = data->m_stitch_piece_cm_index;
            stitch_piece_havok_handle = havok_body_create(pci);
        }
        else
        {
            stitch_piece_havok_handle = 0xFFFFFFFF;
        }
    }
    pci.pos = pos;
    pci.orient.make_identity();
    pci.collision_type = COLLIDE_INVISIBLE_BARRIER;
    pci.shape_type = HAVOK_SHAPE_TYPE_COLLISION_MODEL;
    for (int i = 0; i < data->m_num_invisible_barriers; i++)
    {
        invisible_barrier_data& barrier = data->m_invisible_barriers[i];
        if (barrier.cm_index != 0xFFFFFFFF)
        {
            pci.cm_index = barrier.cm_index;
            barrier.havok_handle = havok_body_create(pci);
        }
    }
    get_bbox(last_known_bmin, last_known_bmax);
    const int phantom_index = request_aabb_phantom(this, data);
    obj_flags.special_lifetime = 1;
    aabb_phantom_index = phantom_index;
    havok_add_entity_flush();
    return true;
}

// 0x00A1E250 (:424)
obj_serialize_mode obj_zone::serialize_mode()
{
    return OSM_NONE;
}

// 0x00A1E260 (:429)
bool obj_zone::stream(bool load)
{
    if (load)
    {
        amb_spawn_zone_load_complete((unsigned short)(get_handle() >> 16));
    }
    else if (obj_flags.destroy_on_stream)
    {
        World.destroy_object(this, false);
    }
    return true;
}

// 0x00A1E2A0 (:448)
unsigned short obj_zone::get_grid_xy() const
{
    return (unsigned short)(get_handle() >> 24);
}

// 0x00A71630 (:464)
void obj_zone::deinstantiate_resources()
{
    if (havok_handle != 0xFFFFFFFF)
    {
        havok_body_free(havok_handle, false, true, true);
        havok_handle = 0xFFFFFFFF;
    }
    terrain_data* data = terrain->terrain_data_p;
    if (stitch_piece_havok_handle != 0xFFFFFFFF)
    {
        havok_body_free(stitch_piece_havok_handle, false, true, true);
        stitch_piece_havok_handle = 0xFFFFFFFF;
    }
    for (int i = 0; i < data->m_num_invisible_barriers; i++)
    {
        invisible_barrier_data& barrier = data->m_invisible_barriers[i];
        if (barrier.havok_handle != 0xFFFFFFFF)
        {
            havok_body_free(barrier.havok_handle, false, true, true);
            barrier.havok_handle = 0xFFFFFFFF;
        }
        if (barrier.cm_index != 0xFFFFFFFF)
        {
            havok_cm_manager_remove_from_list(barrier.cm_index);
            barrier.cm_index = 0xFFFFFFFF;
        }
    }
    rfg_pf_delete(this);
    terrain_deinstantiate(terrain);
    release_aabb_phantom(aabb_phantom_index);
    for (int i = 0; i < m_occluders.size(); i++)
    {
        m_occluders[i]->release();
    }
    havok_remove_entity_flush();
    if (zone_pfg.pfg_shape != 0xFFFFFFFF)
    {
        havok_cm_manager_remove_from_list(zone_pfg.pfg_shape);
    }
    if (zone_pfg.vehicle_pfg_shape != 0xFFFFFFFF)
    {
        havok_cm_manager_remove_from_list(zone_pfg.vehicle_pfg_shape);
    }
    zone_pfg.pfg = nullptr;
    zone_pfg.pfg_shape = 0xFFFFFFFF;
    zone_pfg.pfg_havok_handle = 0xFFFFFFFF;
    zone_pfg.vehicle_pfg = nullptr;
    zone_pfg.vehicle_pfg_shape = 0xFFFFFFFF;
    zone_pfg.vehicle_pfg_havok_handle = 0xFFFFFFFF;
}

// 0x00A71780 (:525)
unsigned char obj_zone::get_effect_material(unsigned int shape_key, const vector* contact_point, const hkpWorldObject* hk_objp) const
{
    terrain_data* data = terrain->terrain_data_p;
    const hkpWorldObject* height_field = (const hkpWorldObject*)havok_body_get_pointer(havok_handle);
    if (data->m_layer_map.data_p.to_pointer() != nullptr && height_field == hk_objp)
    {
        if (contact_point == nullptr)
        {
            return 0;
        }
        // The layer map texel under the contact (512x512 over the cell, 2 bits each) selects one of 4 materials.
        vector bmin;
        vector bmax;
        get_bbox(bmin, bmax);
        int x = (int)(contact_point->x - bmin.x + 0.5f);
        int z = (int)(contact_point->z - bmin.z + 0.5f);
        x = x < 0 ? 0 : (x > 0x1FE ? 0x1FE : x);
        z = z < 0 ? 0 : (z > 0x1FE ? 0x1FE : z);
        const int texel = z * 0x200 + x;
        const int layer = (data->m_layer_map.data_p[texel >> 2] >> ((texel & 3) * 2)) & 3;
        return (unsigned char)data->m_layer_map.material_index[layer];
    }
    if (hk_objp == nullptr)
    {
        hk_objp = (const hkpWorldObject*)havok_body_get_pointer(havok_handle);
        if (hk_objp == nullptr)
        {
            return 0xFF;
        }
    }
    return (unsigned char)(unsigned int)havok_get_property(hk_objp, 4, (void*)(unsigned int)Rfg_effect_material_default_index, true);
}

// 0x00A1E2B0 (:565)
void obj_zone_level_shutdown()
{
    for (int i = 0; i < 9; i++)
    {
        if (Zone_phantoms[i].handle != nullptr)
        {
            havok_aabb_phantom_release(Zone_phantoms[i].handle);
            Zone_phantoms[i].handle = nullptr;
        }
    }
}

// 0x00A1E2E0 (:581)
bool obj_zone_streamed(const vector& pos)
{
    obj_zone* zone = World.get_zone_by_pos(pos);
    return zone != nullptr && zone->streamed();
}

// 0x00A1E300 (:596)
bool obj_zone_streamed_grid_id(unsigned short zone_id)
{
    obj_zone* zone = World.get_object_zone_by_grid_id(zone_id);
    return zone != nullptr && zone->streamed();
}

// 0x00A1E320 (:610)
bool obj_zone_streamed_xy(unsigned short zone_xy)
{
    if (zone_xy >= 0x100)
    {
        return false;
    }
    obj_zone* zone = World.get_object_zone_by_grid_id((unsigned short)(zone_xy << 8));
    return zone != nullptr && zone->streamed();
}

// 0x00A1E350 (:621)
bool obj_zone_streamed_handle(unsigned int handle)
{
    obj_zone* zone = World.get_object_zone_by_grid_id((unsigned short)((handle >> 24) << 8));
    return zone != nullptr && zone->streamed();
}

// 0x00A715F0 (.h:34)
const char* obj_zone::get_classname() const
{
    return "obj_zone";
}

// 0x00743420 (.h:34)
const object_sub_type obj_zone::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A069F0 (.h:34)
const char* obj_zone::get_classname_static()
{
    return "obj_zone";
}

// 0x00A06A00 (.h:34)
int obj_zone::get_bpo_index_static()
{
    return 1;
}

// 0x00743410 (.h:34)
const object_type obj_zone::get_type_static()
{
    return OT_ZONE;
}

// 0x00A71600 (.h:47)
bool obj_zone::process_melee_hit(object* attacker, float damage, const vector& impulse, const vector& hit_pos, const vector& hit_normal, unsigned int shape_key, const weapon_info& w_info)
{
    return false;
}

// 0x00743430 (.h:99)
terrain_data* obj_zone::get_terrain_data() const
{
    return terrain->terrain_data_p;
}

// 0x009308B0 (.h:114)
obj_zone_props::obj_zone_props(const char* terrain_name, const vector& pos, unsigned int hash)
    : obj_prop_block(pos, IDENTITY_MATRIX, nullptr)
{
    set(ZONE_PROP_NAME, terrain_name);
    set(ZONE_PROP_TERRAIN_NAME, terrain_name);
    set(ZONE_PROP_HASH, hash);
}

// 0x0092E620
obj_zone_props::~obj_zone_props()
{
}
