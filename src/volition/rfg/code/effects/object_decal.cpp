// Original: volition/rfg/code/effects/object_decal.cpp
//
// Object decals (VFX.md FX7): bullet holes (with glass cracks), blood splatters and blast marks put on objects through
// their renderable instances' add_decal: humans (at the unanimated hit location, on the head or body), destroyable
// movers (filtered to the hit subpiece's dlod), general movers, vehicle components, debris and zones (every terrain
// patch, stitched mesh, skirt and road mesh the decal sphere touches). Blast decals go on the surfaces a Havok closest
// point query of the blast sphere finds (havok_blast_decal_collector, one point per surface); those overlapping newer
// ones fade (Tracked_blast_decals). The decal tables are blood_decal.xtbl and the glass crack decals.
//
// Not ported: the console tests unused_dcf_blast_decal / unused_dcf_blood_decal (unreferenced in rfg.exe).
// Compiled only in rfg_effects (OPENRFG_EFFECTS); the other programs keep their stand-ins.

#include "volition/rfg/code/effects/object_decal.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkprootcdpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/sphere/hkpsphereshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpsimpleshapephantom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/render_lib/code/rl_decal_parameters.h"
#include "volition/render_lib/code/rl_deformable_mesh_instance.h"
#include "volition/render_lib/code/rl_destroyable_model_instance.h"
#include "volition/render_lib/code/rl_height_mesh_instance.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/render_lib/code/rl_skin_instance.h"
#include "volition/render_lib/code/rl_skirt_mesh_instance.h"
#include "volition/render_lib/code/rl_utility.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/destruction/rfg_shapes.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/geom/decal_info.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/level/rfg_terrainx.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/movers/general_mover.h"
#include "volition/rfg/code/objects/object_debris.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_component.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_util.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>

// ---- file-local types (rfg.pdb)
// size 0x40
struct blood_decal_info
{
    int* close_decal_infos; // 0x00
    int num_close_decal_infos; // 0x04
    int* medium_decal_infos; // 0x08
    int num_medium_decal_infos; // 0x0C
    int* far_decal_infos; // 0x10
    int num_far_decal_infos; // 0x14
    float close_distance; // 0x18
    float medium_distance; // 0x1C
    float far_distance; // 0x20
    float close_rand_mult; // 0x24
    float medium_rand_mult; // 0x28
    float far_rand_mult; // 0x2C
    float melee_dist_mult; // 0x30
    float melee_size_mult; // 0x34
    float decal_probability; // 0x38
    float death_size_mult; // 0x3C
};
static_assert(sizeof(blood_decal_info) == 0x40, "blood_decal_info");

// Decals are made (game_render.cpp).
extern bool R_enable_decals; // 0x0165CC74

// ---- globals
fp_blast_decal_tracker Tracked_blast_decals[32]; // 0x021828A8
int Num_tracked_blast_decals; // 0x02182E04
int fp_blast_decal_tracker::ms_next_global_index; // 0x02182E00

// ---- file-static globals
// fade_overlapped_blast_decals reads up to 0x0218286C, which Ghidra names s_backpackEffects (effects.cpp).
static const char* Glass_radial_decal_names[3] = {"decal_bullet_glass_crack_1", "decal_bullet_glass_crack_2", "decal_bullet_glass_crack_3"}; // 0x01650AE0
static unsigned int Glass_radial_crack_effect_handle = 0xFFFFFFFF; // 0x01650AEC
static int Glass_min_radial_decals = 2; // 0x01650AF0
static int Glass_max_radial_decals = 3; // 0x01650AF4
static int Glass_radial_decal_prob = 75; // 0x01650AF8
static float Blast_decal_min_bbox_dim = 1.5f; // 0x01650AFC
static float Blast_decal_min_radius = 1.0f; // 0x01650B00
static float Blast_decal_min_alpha = 0.5f; // 0x01650B04
static float Bullet_hole_decal_size_variation; // 0x0210A808
static float Blood_splatter_decal_max_dist; // 0x0210A80C
static int Glass_radial_decal_infos[3]; // 0x0218281C
static blood_decal_info Blood_decal_info; // 0x02182828
static float Bullet_hole_decal_max_dist_player; // 0x02182868
static float Bullet_hole_decal_max_dist; // 0x02182CA8
static unsigned char Glass_effect_material_index; // 0x02182CBB
static tweak_table_item Tweak_table_item_Glass_max_radial_decals("Glass_max_radial_decals", &Glass_max_radial_decals); // 0x02182E14
static tweak_table_item Tweak_table_item_Bullet_hole_decal_max_dist_player("Bullet_hole_decal_max_dist_player", &Bullet_hole_decal_max_dist_player); // 0x02182E20
static tweak_table_item Tweak_table_item_Bullet_hole_decal_size_variation("Bullet_hole_decal_size_variation", &Bullet_hole_decal_size_variation); // 0x02182E40
static tweak_table_item Tweak_table_item_Blast_decal_min_bbox_dim("Blast_decal_min_bbox_dim", &Blast_decal_min_bbox_dim); // 0x021A3200
static tweak_table_item Tweak_table_item_Blood_splatter_decal_max_dist("Blood_splatter_decal_max_dist", &Blood_splatter_decal_max_dist); // 0x021A320C
static tweak_table_item Tweak_table_item_Blast_decal_min_radius("Blast_decal_min_radius", &Blast_decal_min_radius); // 0x021A3218
static tweak_table_item Tweak_table_item_Blast_decal_min_alpha("Blast_decal_min_alpha", &Blast_decal_min_alpha); // 0x021A3224
static tweak_table_item Tweak_table_item_Bullet_hole_decal_max_dist("Bullet_hole_decal_max_dist", &Bullet_hole_decal_max_dist); // 0x021A3230
static tweak_table_item Tweak_table_item_Glass_radial_decal_prob("Glass_radial_decal_prob", &Glass_radial_decal_prob); // 0x021A323C
static tweak_table_item Tweak_table_item_Glass_min_radial_decals("Glass_min_radial_decals", &Glass_min_radial_decals); // 0x021EF9D0

// Magic number: a full turn (the random decal rotation).
static const float Decal_full_turn = 6.2831855f;

// ---- decal_circ_array_queue<10>

// <10> 0x0074B030 (.h:68)
// A full queue releases its oldest decal first.
template<>
void decal_circ_array_queue<10>::add(unsigned int handle, float opacity)
{
    if (num == 10)
    {
        num = 9;
        rl_decal_release(carray[9].handle);
    }
    const decal_fade_data data(handle, opacity);
    carray.add(data);
    num++;
}

// ---- fp_blast_decal_tracker

// 0x00743470 (:202)
// The box in 1/256 m units (floored) and the next global index (the age order).
void fp_blast_decal_tracker::init(const vector& bmin, const vector& bmax, unsigned int decal_handle)
{
    // Magic number: 256 units per metre.
    m_min_x = (int)floor((double)(bmin.x * 256.0f));
    m_min_y = (int)floor((double)(bmin.y * 256.0f));
    m_min_z = (int)floor((double)(bmin.z * 256.0f));
    m_max_x = (int)floor((double)(bmax.x * 256.0f));
    m_max_y = (int)floor((double)(bmax.y * 256.0f));
    m_max_z = (int)floor((double)(bmax.z * 256.0f));
    m_decal_handle = decal_handle;
    m_global_index = ms_next_global_index++;
}

// 0x00743730 (:218)
bool fp_blast_decal_tracker::intersects(const fp_blast_decal_tracker& other_tracker)
{
    return m_min_x < other_tracker.m_max_x && m_min_y < other_tracker.m_max_y && m_min_z < other_tracker.m_max_z && other_tracker.m_min_x < m_max_x && other_tracker.m_min_y < m_max_y && other_tracker.m_min_z < m_max_z;
}

// 0x00743770 (:224)
unsigned int fp_blast_decal_tracker::get_handle() const
{
    return m_decal_handle;
}

// 0x00743780 (:225)
int fp_blast_decal_tracker::get_global_index() const
{
    return m_global_index;
}

// 0x00743790 (:322)
// Newest first.
static int sort_overlapped_decals(const void* arg1, const void* arg2)
{
    return ((const fp_blast_decal_tracker*)arg2)->get_global_index() - ((const fp_blast_decal_tracker*)arg1)->get_global_index();
}

// 0x00745E70 (:336)
// The trackers the new blast decal overlaps are taken out (the last tracker moves into the freed slot); of those, all
// but the 3 newest fade out over 500 ms and the rest go back at the end. Then the new decal is tracked; with 32 tracked
// the oldest of the ones that do not overlap (lowest global index) fades out and is replaced.
// The oldest is chosen before the kept overlapped ones are appended.
void fade_overlapped_blast_decals(const vector& bmin, const vector& bmax, unsigned int decal_handle)
{
    fp_blast_decal_tracker tracker;
    tracker.init(bmin, bmax, decal_handle);
    int oldest_index = 0;
    if (Num_tracked_blast_decals > 0)
    {
        fp_blast_decal_tracker overlapped[32];
        int num_overlapped = 0;
        int oldest_global = 0x7FFFFFFF;
        int i = 0;
        while (i < Num_tracked_blast_decals)
        {
            if (Tracked_blast_decals[i].intersects(tracker))
            {
                overlapped[num_overlapped++] = Tracked_blast_decals[i];
                Num_tracked_blast_decals--;
                if (i < Num_tracked_blast_decals)
                {
                    Tracked_blast_decals[i] = Tracked_blast_decals[Num_tracked_blast_decals];
                }
                // The moved tracker is tested next (i is not advanced).
                continue;
            }
            if (Tracked_blast_decals[i].get_global_index() < oldest_global)
            {
                oldest_global = Tracked_blast_decals[i].get_global_index();
                oldest_index = i;
            }
            i++;
        }
        // Magic number: the 3 newest overlapped decals stay.
        if (num_overlapped > 3)
        {
            qsort(overlapped, num_overlapped, sizeof(fp_blast_decal_tracker), sort_overlapped_decals);
            for (int k = 3; k < num_overlapped; k++)
            {
                // Magic number: a 500 ms fade.
                rl_decal_fade_out(overlapped[k].get_handle(), 500, 0xFFFFFFFF);
            }
            num_overlapped = 3;
        }
        for (int k = 0; k < num_overlapped; k++)
        {
            Tracked_blast_decals[Num_tracked_blast_decals++] = overlapped[k];
        }
    }
    if (Num_tracked_blast_decals < 32)
    {
        Tracked_blast_decals[Num_tracked_blast_decals++] = tracker;
        return;
    }
    rl_decal_fade_out(Tracked_blast_decals[oldest_index].get_handle(), 500, 0xFFFFFFFF);
    Tracked_blast_decals[oldest_index] = tracker;
}

// ---- tables

// 0x007437A0 (:404)
// The decal info indices of a list's "Decal" entries (from the permanent mempool). Returns the count.
// Notes (decompiled behaviour, kept as is): a list shorter than its count returns the filled count plus one.
static int blood_decal_info_list_fill(int*& decal_infos, xml_element* list)
{
    const int count = (int)xml_count(list, "Decal");
    decal_infos = (int*)Permanent_mempool.alloc(count * sizeof(int), 4);
    xml_element* cur = xml_find(list, "Decal");
    for (int i = 0; i < count; i++)
    {
        if (cur == nullptr)
        {
            return i + 1;
        }
        decal_infos[i] = decal_info_find(cur->text);
        cur = xml_find_next(list, cur, "Decal");
    }
    return count;
}

// 0x00743830 (:426)
static void blood_decal_load_table()
{
    xml_element* table = xml_table_parse("blood_decal.xtbl");
    xml_element* info = xml_find(table, "Blood_decal_info");
    xml_table_get_req_float(Blood_decal_info.close_distance, info, "Close_distance");
    xml_table_get_req_float(Blood_decal_info.medium_distance, info, "Medium_distance");
    xml_table_get_req_float(Blood_decal_info.far_distance, info, "Far_distance");
    xml_table_get_req_float(Blood_decal_info.close_rand_mult, info, "Close_random_size_multiplier");
    xml_table_get_req_float(Blood_decal_info.medium_rand_mult, info, "Medium_random_size_multiplier");
    xml_table_get_req_float(Blood_decal_info.far_rand_mult, info, "Far_random_size_multiplier");
    xml_table_get_req_float(Blood_decal_info.melee_size_mult, info, "Melee_size_multiplier");
    xml_table_get_req_float(Blood_decal_info.melee_dist_mult, info, "Melee_distance_multiplier");
    xml_table_get_req_float(Blood_decal_info.decal_probability, info, "Decal_probability");
    xml_table_get_req_float(Blood_decal_info.death_size_mult, info, "Death_size_multiplier");
    Blood_decal_info.num_close_decal_infos = blood_decal_info_list_fill(Blood_decal_info.close_decal_infos, xml_find(info, "Close_decals"));
    Blood_decal_info.num_medium_decal_infos = blood_decal_info_list_fill(Blood_decal_info.medium_decal_infos, xml_find(info, "Medium_decals"));
    Blood_decal_info.num_far_decal_infos = blood_decal_info_list_fill(Blood_decal_info.far_decal_infos, xml_find(info, "Far_decals"));
    xml_table_close(table);
}

// 0x00743950 (:468)
// The glass effect material, the three crack decals and the crack effect.
static void glass_decal_info_init()
{
    Glass_effect_material_index = rfg_get_effect_material_index("Glass");
    for (int i = 0; i < 3; i++)
    {
        Glass_radial_decal_infos[i] = decal_info_find(Glass_radial_decal_names[i]);
    }
    for (unsigned int i = 0; i < Num_effect_types; i++)
    {
        if (v_stricmp("des_glass_crack_md", Effect_info[i].name) == 0)
        {
            Glass_radial_crack_effect_handle = i;
            return;
        }
    }
    Glass_radial_crack_effect_handle = 0xFFFFFFFF;
}

// ---- filters and callbacks

// 0x00748300 (:495)
// A destroyable mover's sub instance to its dlod (the subpiece's dlod key); sub instances past the subpieces count
// from 0.
// Notes (decompiled behaviour, kept as is): the mover's data is read without checking it is loaded.
static unsigned int rfg_mover_dlod_decal_filter(void* filter_data, unsigned int rl_sub_instance_idx)
{
    if (filter_data == nullptr)
    {
        return 0xFFFFFFFF;
    }
    rfg_mover* mover = (rfg_mover*)filter_data;
    rfg_destroyable_shape* shape = mover->rdd.is_loaded() ? mover->rdd.get_data()->shape : nullptr;
    const rfg_base_destroyable_shape* parent = shape->parent;
    if ((unsigned int)parent->num_objects <= rl_sub_instance_idx)
    {
        return rl_sub_instance_idx - (unsigned int)parent->num_objects;
    }
    return (unsigned short)parent->base_objects.to_pointer()[rl_sub_instance_idx].dlod_key;
}

// 0x007439E0 (:517)
// The blood ray passes humans.
static bool human_blood_decal_callback(object* hit_objp, void* cb_data)
{
    return hit_objp->get_obj_type() != OT_HUMAN;
}

// 0x007439F0 (:623)
static bool skip_humans_raycast_callback(object* hit_objp, void* filter_params)
{
    return hit_objp != nullptr && hit_objp->get_obj_type() != OT_HUMAN;
}

// ---- zones

// Not in the binary: the terrain data of a zone (obj_zone::get_terrain_data, inlined; the terrain is not checked).
static terrain_data* zone_terrain_data(object* op)
{
    return ((obj_zone*)op)->get_terrain_data();
}

static terrain_subzone_data* zone_subzone(terrain_data* data, int i)
{
    return data->m_subzones.to_pointer()[i].to_pointer();
}

// Not in the binary: the decal sphere in an entity's world box.
static bool decal_touches_entity(rl_scene_entity* entity, const rl_decal_parameters& decal_params, float decal_radius)
{
    rl_aabb box;
    entity->get_world_aabb(box);
    return ix_sphere_in_box(decal_params.m_pos, decal_radius, box.min, box.max);
}

// Not in the binary: the decal sphere in a terrain patch's box (relative to the patch).
static bool decal_touches_patch(const terrain_subzone_data::patch& patch, const rl_decal_parameters& decal_params, float decal_radius)
{
    const vector local(decal_params.m_pos.x - patch.position.x, decal_params.m_pos.y - patch.position.y, decal_params.m_pos.z - patch.position.z);
    return ix_sphere_in_box(local, decal_radius, patch.local_aabb_min, patch.local_aabb_max);
}

// 0x00748340 (:697)
// The terrain pieces of a zone a decal sphere touches, up to num_instances (in: the buffer size; out: the count):
// patches, then (when the sphere touches the subzone's render box) the stitched meshes and skirts of its stitch
// pieces, then the road meshes.
void object_zone_decal_collect(object* op, decal_collection_data* buffer, unsigned int& num_instances, const rl_decal_parameters& decal_params, float decal_radius)
{
    const unsigned int max_instances = num_instances;
    num_instances = 0;
    if (max_instances == 0 || !R_enable_decals || op == nullptr || op->get_obj_type() != OT_ZONE || !(0.0f < decal_radius))
    {
        return;
    }
    terrain_data* data = zone_terrain_data(op);
    if (data == nullptr)
    {
        return;
    }
    for (int s = 0; s < data->m_num_subzones; s++)
    {
        terrain_subzone_data* subzone = zone_subzone(data, s);
        if (subzone == nullptr)
        {
            continue;
        }
        terrain_subzone_data::patch* patches = subzone->patches.to_pointer();
        for (int p = 0; p < subzone->patch_count; p++)
        {
            if (patches[p].instance_p.to_pointer() == nullptr || !decal_touches_patch(patches[p], decal_params, decal_radius))
            {
                continue;
            }
            buffer->type = TERRAIN_TYPE_PATCH;
            buffer->terrain_ptr = &patches[p];
            buffer++;
            if (max_instances <= ++num_instances)
            {
                return;
            }
        }
        if (ix_sphere_in_box(decal_params.m_pos, decal_radius, subzone->renderable_data.aabb_min, subzone->renderable_data.aabb_max))
        {
            terrain_stitch_instance_data* stitches = subzone->stitch_pieces.to_pointer();
            for (int t = 0; t < subzone->num_stitch_pieces; t++)
            {
                terrain_stitch_instance_data& stitch = stitches[t];
                et_ptr_offset<rl_mesh_instance, 0>* meshes = stitch.stitched_meshes_p.to_pointer();
                for (int m = 0; m < stitch.num_stitched_meshes; m++)
                {
                    rl_mesh_instance* mesh = meshes[m].to_pointer();
                    if (mesh == nullptr || !decal_touches_entity(mesh, decal_params, decal_radius))
                    {
                        continue;
                    }
                    buffer->type = TERRAIN_TYPE_MESH;
                    buffer->terrain_ptr = mesh;
                    buffer++;
                    if (max_instances <= ++num_instances)
                    {
                        return;
                    }
                }
                terrain_stitch_instance_data::skirt* skirts = stitch.skirts_p.to_pointer();
                for (int k = 0; k < stitch.num_skirts; k++)
                {
                    rl_skirt_mesh_instance* blend = skirts[k].blend_renderable_instance_p.to_pointer();
                    if (blend != nullptr && decal_touches_entity(blend, decal_params, decal_radius))
                    {
                        buffer->type = TERRAIN_TYPE_SKIRT;
                        buffer->terrain_ptr = &skirts[k];
                        buffer++;
                        if (max_instances <= ++num_instances)
                        {
                            return;
                        }
                    }
                    rl_skirt_mesh_instance* generated = skirts[k].generated_renderable_instance_p.to_pointer();
                    if (generated != nullptr && decal_touches_entity(generated, decal_params, decal_radius))
                    {
                        buffer->type = TERRAIN_TYPE_SKIRT;
                        buffer->terrain_ptr = &skirts[k];
                        buffer++;
                        if (max_instances <= ++num_instances)
                        {
                            return;
                        }
                    }
                }
            }
        }
        road_mesh_data* roads = subzone->road_decal_meshes.to_pointer();
        for (int r = 0; r < subzone->num_road_decal_meshes; r++)
        {
            et_ptr_offset<rl_renderable_instance, 0>* instances = roads[r].m_mesh_instances_p.to_pointer();
            for (int m = 0; m < roads[r].m_num_mesh_instances; m++)
            {
                rl_renderable_instance* instance = instances[m].to_pointer();
                if (instance == nullptr || !decal_touches_entity(instance, decal_params, decal_radius))
                {
                    continue;
                }
                buffer->type = TERRAIN_TYPE_RI;
                buffer->terrain_ptr = instance;
                buffer++;
                if (max_instances <= ++num_instances)
                {
                    return;
                }
            }
        }
    }
}

// 0x007487B0 (:859)
// The decal on every collected piece it still touches, all under one handle (the first add_decal's).
unsigned int object_zone_decal_apply_collection(object* op, decal_collection_data* buffer, unsigned int num_instances, const rl_decal_parameters& decal_params, float decal_radius)
{
    if (!R_enable_decals || op == nullptr)
    {
        return 0xFFFFFFFF;
    }
    if (decal_radius <= 0.0f)
    {
        const float length = decal_params.m_planar_params.m_length;
        decal_radius = decal_params.m_planar_params.m_width;
        if (0.0f <= length - decal_radius)
        {
            decal_radius = length;
        }
    }
    if (op->get_obj_type() != OT_ZONE || zone_terrain_data(op) == nullptr)
    {
        return 0xFFFFFFFF;
    }
    unsigned int handle = 0xFFFFFFFF;
    for (unsigned int i = 0; i < num_instances; i++)
    {
        switch (buffer[i].type)
        {
        case TERRAIN_TYPE_PATCH:
        {
            terrain_subzone_data::patch* patch = (terrain_subzone_data::patch*)buffer[i].terrain_ptr;
            rl_height_mesh_instance* instance = patch->instance_p.to_pointer();
            if (instance != nullptr && decal_touches_patch(*patch, decal_params, decal_radius))
            {
                handle = instance->add_decal(&decal_params, handle);
            }
            break;
        }
        case TERRAIN_TYPE_MESH:
        case TERRAIN_TYPE_RI:
        {
            rl_renderable_instance* instance = (rl_renderable_instance*)buffer[i].terrain_ptr;
            if (instance != nullptr && decal_touches_entity(instance, decal_params, decal_radius))
            {
                handle = instance->add_decal(&decal_params, handle);
            }
            break;
        }
        case TERRAIN_TYPE_SKIRT:
        {
            terrain_stitch_instance_data::skirt* skirt = (terrain_stitch_instance_data::skirt*)buffer[i].terrain_ptr;
            if (skirt == nullptr)
            {
                break;
            }
            rl_renderable_instance* blend = skirt->blend_renderable_instance_p.to_pointer();
            if (blend != nullptr && decal_touches_entity(blend, decal_params, decal_radius))
            {
                handle = blend->add_decal(&decal_params, handle);
            }
            rl_renderable_instance* generated = skirt->generated_renderable_instance_p.to_pointer();
            if (generated != nullptr && decal_touches_entity(generated, decal_params, decal_radius))
            {
                handle = generated->add_decal(&decal_params, handle);
            }
            break;
        }
        }
    }
    return handle;
}

// ---- apply

// 0x00748A60 (:1022)
// The decal on the object's renderable instance(s) (nothing unless R_enable_decals); a radius of 0 means the larger of
// the decal's width and length. Returns the decal handle (-1 when nothing took it).
unsigned int object_decal_apply(object* op, const rl_decal_parameters& decal_params, float decal_radius, unsigned int hit_shape_key, unsigned int hit_alt_body_idx, unsigned int decal_handle)
{
    if (!R_enable_decals || op == nullptr)
    {
        return 0xFFFFFFFF;
    }
    if (decal_radius <= 0.0f)
    {
        const float length = decal_params.m_planar_params.m_length;
        decal_radius = decal_params.m_planar_params.m_width;
        if (0.0f <= length - decal_radius)
        {
            decal_radius = length;
        }
    }
    switch (op->get_obj_type())
    {
    case OT_HUMAN:
    {
        // The hit location of a short ray into the decal (from 0.2 m in front of it), moved onto the unanimated
        // skeleton, so the decal follows the body.
        human* hp = (human*)op;
        const vector& fvec = decal_params.m_transform.fvec;
        const vector dir(-fvec.x, -fvec.y, -fvec.z);
        // Magic number: the ray starts 0.2 m out of the surface.
        const vector start(decal_params.m_pos.x + fvec.x * 0.2f, decal_params.m_pos.y + fvec.y * 0.2f, decal_params.m_pos.z + fvec.z * 0.2f);
        vector hit_pos;
        int bone_index;
        vector hit_normal;
        const object_hit_location location = human_get_hit_location(hp, start, dir, hit_pos, 0.0f, &bone_index, &hit_normal);
        if (location == HIT_NONE || location == HIT_GENERAL)
        {
            break;
        }
        vector local_pos(hit_pos.x - op->pos.x, hit_pos.y - op->pos.y, hit_pos.z - op->pos.z);
        local_pos.rotate(op->orient);
        const vector local_normal = hit_normal.copy_rotate(op->orient);
        vector unanimated_pos;
        vector unanimated_normal;
        human_get_unanimated_hit_location(hp, local_pos, local_normal, bone_index, unanimated_pos, unanimated_normal);
        const vector world_offset = unanimated_pos.copy_unrotate(op->orient);
        vector world_normal = unanimated_normal.copy_unrotate(op->orient);
        world_normal.normalize_safe();
        rl_decal_parameters params(decal_params);
        params.m_pos.set(world_offset.x + op->pos.x, world_offset.y + op->pos.y, world_offset.z + op->pos.z);
        params.m_transform.make_quick(world_normal);
        if (location == HIT_HEAD)
        {
            if (hp->rdd.is_loaded() && hp->rdd.get_data()->head_renderable_instance_p != nullptr)
            {
                return ((rl_renderable_instance*)hp->rdd.get_data()->head_renderable_instance_p)->add_decal(&params, decal_handle);
            }
        }
        else
        {
            character_instance* ci = hp->char_inst;
            if (ci->rdd.is_loaded() && ci->rdd.get_data()->renderable_instance_p != nullptr)
            {
                return ((rl_renderable_instance*)ci->rdd.get_data()->renderable_instance_p)->add_decal(&params, decal_handle);
            }
        }
        break;
    }
    case OT_MOVER:
        if (op->get_sub_type() == OT_SUB_MOVER_RFG)
        {
            rfg_mover* mover = World.object_cast<rfg_mover>(op);
            if (mover == nullptr || !mover->rdd.is_loaded() || mover->rdd.get_data()->model_instance_p == nullptr)
            {
                break;
            }
            // Only the dlod of the hit subpiece takes the decal.
            rl_decal_parameters params(decal_params);
            if (hit_shape_key != 0xFFFFFFFF)
            {
                rfg_destroyable_shape* shape = mover->get_shape();
                if (shape != nullptr)
                {
                    const rfg_subpiece_base* base = &shape->parent->base_objects.to_pointer()[hit_shape_key];
                    if (base != nullptr)
                    {
                        params.m_filter = (unsigned short)base->dlod_key;
                        params.m_filter_cb = rfg_mover_dlod_decal_filter;
                        params.m_filter_data = mover;
                    }
                }
            }
            return ((rl_renderable_instance*)mover->rdd.get_data()->model_instance_p)->add_decal(&params, decal_handle);
        }
        if (op->get_sub_type() == OT_SUB_MOVER_GENERAL)
        {
            general_mover* mover = World.object_cast<general_mover>(op);
            if (mover != nullptr && mover->rdd.is_loaded() && mover->rdd.get_data()->model_instance_p != nullptr)
            {
                return mover->rdd.get_data()->model_instance_p->add_decal(&decal_params, decal_handle);
            }
        }
        break;
    case OT_VEHICLE:
    {
        vehicle* vp = (vehicle*)op;
        if (!vp->rdd.is_loaded())
        {
            break;
        }
        vehicle::resource_dependent_data* data = vp->rdd.get_data();
        const unsigned int component = vehicle_get_component_index_from_shape_key(vp, hit_shape_key, hit_alt_body_idx);
        if (component != 0xFFFFFFFF && data->m_components[component].m_renderable_instance_p != nullptr)
        {
            return ((rl_renderable_instance*)data->m_components[component].m_renderable_instance_p)->add_decal(&decal_params, decal_handle);
        }
        if (data->m_renderable_instance_p != nullptr)
        {
            return ((rl_renderable_instance*)data->m_renderable_instance_p)->add_decal(&decal_params, decal_handle);
        }
        break;
    }
    case OT_DEBRIS:
    {
        object_debris* debris = (object_debris*)op;
        if (debris->m_renderable_instance_p != nullptr)
        {
            return debris->m_renderable_instance_p->add_decal(&decal_params, decal_handle);
        }
        break;
    }
    case OT_ZONE:
    {
        // Every terrain piece the sphere touches (object_zone_decal_collect's walk, unbounded), under one handle.
        terrain_data* data = zone_terrain_data(op);
        if (data == nullptr)
        {
            break;
        }
        for (int s = 0; s < data->m_num_subzones; s++)
        {
            terrain_subzone_data* subzone = zone_subzone(data, s);
            if (subzone == nullptr)
            {
                continue;
            }
            terrain_subzone_data::patch* patches = subzone->patches.to_pointer();
            for (int p = 0; p < subzone->patch_count; p++)
            {
                rl_height_mesh_instance* instance = patches[p].instance_p.to_pointer();
                if (instance != nullptr && decal_touches_patch(patches[p], decal_params, decal_radius))
                {
                    decal_handle = instance->add_decal(&decal_params, decal_handle);
                }
            }
            terrain_stitch_instance_data* stitches = subzone->stitch_pieces.to_pointer();
            for (int t = 0; t < subzone->num_stitch_pieces; t++)
            {
                terrain_stitch_instance_data& stitch = stitches[t];
                et_ptr_offset<rl_mesh_instance, 0>* meshes = stitch.stitched_meshes_p.to_pointer();
                for (int m = 0; m < stitch.num_stitched_meshes; m++)
                {
                    rl_mesh_instance* mesh = meshes[m].to_pointer();
                    if (mesh != nullptr && decal_touches_entity(mesh, decal_params, decal_radius))
                    {
                        decal_handle = mesh->add_decal(&decal_params, decal_handle);
                    }
                }
                terrain_stitch_instance_data::skirt* skirts = stitch.skirts_p.to_pointer();
                for (int k = 0; k < stitch.num_skirts; k++)
                {
                    rl_renderable_instance* blend = skirts[k].blend_renderable_instance_p.to_pointer();
                    if (blend != nullptr && decal_touches_entity(blend, decal_params, decal_radius))
                    {
                        decal_handle = blend->add_decal(&decal_params, decal_handle);
                    }
                    rl_renderable_instance* generated = skirts[k].generated_renderable_instance_p.to_pointer();
                    if (generated != nullptr && decal_touches_entity(generated, decal_params, decal_radius))
                    {
                        decal_handle = generated->add_decal(&decal_params, decal_handle);
                    }
                }
            }
            road_mesh_data* roads = subzone->road_decal_meshes.to_pointer();
            for (int r = 0; r < subzone->num_road_decal_meshes; r++)
            {
                et_ptr_offset<rl_renderable_instance, 0>* instances = roads[r].m_mesh_instances_p.to_pointer();
                for (int m = 0; m < roads[r].m_num_mesh_instances; m++)
                {
                    rl_renderable_instance* instance = instances[m].to_pointer();
                    if (instance != nullptr && decal_touches_entity(instance, decal_params, decal_radius))
                    {
                        decal_handle = instance->add_decal(&decal_params, decal_handle);
                    }
                }
            }
        }
        return decal_handle;
    }
    default:
        break;
    }
    return 0xFFFFFFFF;
}

// ---- creators

// 0x0074B940 (:1294)
// A ray from the hit past the victim (humans skipped) of the far distance (scaled for melee); with a hit, a close,
// medium or far blood decal by the hit distance, randomly enlarged (melee and death scale it), randomly rotated. Only
// for allowed blood, a vulnerable victim, within the splatter distance of the camera (always for the player's hits)
// and with the decal probability (always on a kill).
void object_decal_create_blood_splatter_decal(const human* hp, const human* attacker, const vector& start_pos, const vector& dir, bool killed, damaged_by_types dbt)
{
    if (!R_enable_decals || !localization_allow_blood() || hp->is_invulnerable())
    {
        return;
    }
    if (!object_is_player(attacker))
    {
        const float dx = Rfg_camera.real_pos.x - start_pos.x;
        const float dy = Rfg_camera.real_pos.y - start_pos.y;
        const float dz = Rfg_camera.real_pos.z - start_pos.z;
        if (!(dx * dx + dy * dy + dz * dz <= Blood_splatter_decal_max_dist * Blood_splatter_decal_max_dist))
        {
            return;
        }
    }
    if (!killed && !(fl_rand() <= Blood_decal_info.decal_probability))
    {
        return;
    }
    const float dist_mult = dbt == DBT_MELEE ? Blood_decal_info.melee_dist_mult : 1.0f;
    raycast_input in;
    in.radius = 0.0f;
    in.start = start_pos;
    in.end.set(start_pos.x + dir.x * Blood_decal_info.far_distance * dist_mult, start_pos.y + dir.y * Blood_decal_info.far_distance * dist_mult, start_pos.z + dir.z * Blood_decal_info.far_distance * dist_mult);
    in.m_filter_info = COLLIDE_RAY_DEFAULT;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 0;
    in.filter_callback = human_blood_decal_callback;
    in.filter_params = (void*)hp;
    raycast_output out;
    rl_decal_parameters params;
    if (!havok_raycast(in, out))
    {
        return;
    }
    const raycast_output::hit_info& hit = out.m_hits[0];
    object* hit_obj = World.object_from_handle(hit.hit_object_handle);
    if (hit_obj == nullptr)
    {
        return;
    }
    const float distance = hit.hit_fraction * Blood_decal_info.far_distance;
    float rand_mult;
    if (!(Blood_decal_info.close_distance * dist_mult < distance))
    {
        decal_info_get(&params, Blood_decal_info.close_decal_infos[int_rand_range(0, Blood_decal_info.num_close_decal_infos - 1)]);
        rand_mult = Blood_decal_info.close_rand_mult;
    }
    else if (!(Blood_decal_info.medium_distance * dist_mult < distance))
    {
        decal_info_get(&params, Blood_decal_info.medium_decal_infos[int_rand_range(0, Blood_decal_info.num_medium_decal_infos - 1)]);
        rand_mult = Blood_decal_info.medium_rand_mult;
    }
    else
    {
        decal_info_get(&params, Blood_decal_info.far_decal_infos[int_rand_range(0, Blood_decal_info.num_far_decal_infos - 1)]);
        rand_mult = Blood_decal_info.far_rand_mult;
    }
    params.m_planar_params.m_width = fl_rand() * rand_mult + params.m_planar_params.m_width;
    params.m_planar_params.m_length = fl_rand() * rand_mult + params.m_planar_params.m_length;
    if (dbt == DBT_MELEE)
    {
        params.m_planar_params.m_width = Blood_decal_info.melee_size_mult * params.m_planar_params.m_width;
        params.m_planar_params.m_length = Blood_decal_info.melee_size_mult * params.m_planar_params.m_length;
    }
    if (killed)
    {
        params.m_planar_params.m_width = Blood_decal_info.death_size_mult * params.m_planar_params.m_width;
        params.m_planar_params.m_length = Blood_decal_info.death_size_mult * params.m_planar_params.m_length;
    }
    params.m_pos = hit.hit_pos;
    params.m_transform.make(hit.hit_normal);
    params.m_transform.rotate_around_local_z(fl_rand_range(0.0f, Decal_full_turn));
    const unsigned int alt_body = hit_obj->get_alt_hk_body_handle_index(hit.hit_havok_handle);
    object_decal_apply(hit_obj, params, 0.0f, hit.hit_havok_shape_key, alt_body, 0xFFFFFFFF);
}

// 0x0074C6C0 (:1401)
// The bullet hole of the hit's effect material for the weapon's bullet group (none on an invulnerable human or where
// blood is not allowed, nor beyond the bullet hole distance from the camera, longer for the player's shots): randomly
// rotated and sized, tracked by a hit player. On glass (not on vehicles; on a mover only when no link of the hit
// subpiece is weak enough for the damage) also on the back face, and with Glass_radial_decal_prob a crack effect and
// 2-3 crack decals spread around the hit, front and back.
void object_decal_create_bullet_impact_decal(object* target, const human* attacker, const weapon_info* w_info, const vector& pos, const vector& normal, float damage, const weapon_fire_info* fire_info)
{
    if (!R_enable_decals)
    {
        return;
    }
    if (target != nullptr && target->get_obj_type() == OT_HUMAN)
    {
        if (((human*)target)->is_invulnerable() || !localization_allow_blood())
        {
            return;
        }
    }
    const bool player_shot = object_is_player(attacker);
    const float dx = Rfg_camera.real_pos.x - pos.x;
    const float dy = Rfg_camera.real_pos.y - pos.y;
    const float dz = Rfg_camera.real_pos.z - pos.z;
    const float max_dist = player_shot ? Bullet_hole_decal_max_dist_player : Bullet_hole_decal_max_dist;
    if (max_dist * max_dist < dx * dx + dy * dy + dz * dz)
    {
        return;
    }
    unsigned int shape_key = 0xFFFFFFFF;
    unsigned int alt_body = 0xFFFFFFFF;
    unsigned char material_index = 0xFF;
    if (fire_info != nullptr)
    {
        alt_body = fire_info->hit_alt_body_idx;
        shape_key = fire_info->hit_shape_key;
        material_index = fire_info->hit_effect_material;
    }
    if (material_index == 0xFF)
    {
        material_index = target->get_effect_material(shape_key, &pos, nullptr);
    }
    rfg_effect_material* material = rfg_get_effect_material_by_index(material_index);
    const int decal_id = material->get_bullet_impact_decal_info(w_info->bullet_group_index);
    if (decal_id == -1)
    {
        return;
    }
    rl_decal_parameters params;
    decal_info_get(&params, decal_id);
    params.m_pos = pos;
    params.m_permanent = false;
    params.m_transform.make(normal);
    params.m_transform.rotate_around_local_z(fl_rand_range(0.0f, Decal_full_turn));
    const float scale = w_info->bullet_hole_scale * fl_rand_range(1.0f - Bullet_hole_decal_size_variation, Bullet_hole_decal_size_variation + 1.0f);
    params.m_planar_params.m_width = params.m_planar_params.m_width * scale;
    params.m_planar_params.m_length = params.m_planar_params.m_length * scale;
    const unsigned int handle = object_decal_apply(target, params, 0.0f, shape_key, alt_body, 0xFFFFFFFF);
    object_decal_player_track_blood_decal(target, handle);
    if (material_index != Glass_effect_material_index || target->get_obj_type() == OT_VEHICLE)
    {
        return;
    }
    rfg_mover* mover = World.object_cast<rfg_mover>(target);
    if (mover != nullptr)
    {
        if (mover->flags.invulnerable)
        {
            return;
        }
        // The glass breaks (and takes no decal) when any of the subpiece's links yields to the damage.
        rfg_destroyable_shape* shape = mover->get_shape();
        const rfg_subpiece* subpiece = shape->get_subpiece(shape_key);
        for (int i = 0; i < (int)subpiece->num_links; i++)
        {
            if (shape->get_link(subpiece, i)->yield_limit <= (int)damage)
            {
                return;
            }
        }
    }
    params.m_transform.fvec.x = params.m_transform.fvec.x * -1.0f;
    params.m_transform.fvec.y = params.m_transform.fvec.y * -1.0f;
    params.m_transform.fvec.z = params.m_transform.fvec.z * -1.0f;
    object_decal_apply(target, params, 0.0f, shape_key, alt_body, 0xFFFFFFFF);
    if (!int_rand_chance(Glass_radial_decal_prob))
    {
        return;
    }
    matrix crack_orient;
    crack_orient.make_quick(normal);
    effects_play(Glass_radial_crack_effect_handle, pos, crack_orient, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    const int num_cracks = int_rand_range(Glass_min_radial_decals, Glass_max_radial_decals);
    for (int i = 0; i < num_cracks; i++)
    {
        rl_decal_parameters crack;
        decal_info_get(&crack, Glass_radial_decal_infos[int_rand_range(0, 2)]);
        crack.m_permanent = false;
        crack.m_transform.make_quick(normal);
        // One sector of the circle per crack.
        const float sector = (float)(Decal_full_turn / (double)(float)num_cracks);
        crack.m_transform.rotate_around_local_z(fl_rand_range((float)i * sector, (float)(i + 1) * sector));
        const float half_width = crack.m_planar_params.m_width * 0.5f;
        crack.m_pos.set(pos.x - crack.m_transform.rvec.x * half_width, pos.y - crack.m_transform.rvec.y * half_width, pos.z - crack.m_transform.rvec.z * half_width);
        object_decal_apply(target, crack, 0.0f, shape_key, alt_body, 0xFFFFFFFF);
        crack.m_transform.fvec.x = crack.m_transform.fvec.x * -1.0f;
        crack.m_transform.fvec.y = crack.m_transform.fvec.y * -1.0f;
        crack.m_transform.fvec.z = crack.m_transform.fvec.z * -1.0f;
        object_decal_apply(target, crack, 0.0f, shape_key, alt_body, 0xFFFFFFFF);
    }
}

// ---- blast decals

// Not in the binary: the largest extent of a box (inlined; ties go to y over z, then x over the rest).
static float blast_decal_max_dim(const vector& bmin, const vector& bmax)
{
    const float dx = bmax.x - bmin.x;
    const float dy = bmax.y - bmin.y;
    const float dz = bmax.z - bmin.z;
    float dim = dy;
    if (dy - dz < 0.0f)
    {
        dim = dz;
    }
    if (0.0f <= dx - dim)
    {
        dim = dx;
    }
    return dim;
}

// Not in the binary: the root collidable of a body hierarchy (the topmost body, inlined).
static const hkpCollidable* blast_decal_root_collidable(const hkpCdBody* body)
{
    while (body->m_parent != nullptr)
    {
        body = body->m_parent;
    }
    return (const hkpCollidable*)body;
}

// 0x0074D750
havok_blast_decal_collector::havok_blast_decal_collector()
{
}

// 0x0074D930 (:110)
// Notes (decompiled behaviour, kept as is): the boxes are not cleared.
void havok_blast_decal_collector::reset()
{
    m_hits.clear();
    m_earlyOutDistance = FLT_MAX;
    center.setZero();
}

// 0x0074D990 (:120)
// One point per surface of an object: on a destroyable mover a surface is a dlod (the subpiece's dlod key), elsewhere
// an effect material. A point on a collected surface replaces that surface's point when it is closer to the center
// and grows the surface's box; a point on a new surface is collected with a box of its own (m_bboxes runs parallel to
// m_hits). Points of bodies without an object are dropped.
// Notes (decompiled behaviour, kept as is): the effect material at a collected point is looked up with the new point's
// shape key; a replaced point keeps its root collidable and shape key (only the position and normal change).
void havok_blast_decal_collector::addCdPoint(const hkpCdPoint& point)
{
    object* op = object_from_world_obj((const hkpWorldObject*)blast_decal_root_collidable(&point.m_cdBodyB)->getOwner());
    if (op == nullptr)
    {
        return;
    }
    vector pos;
    havok_to_game_vector(pos, point.m_contact.m_position);
    rfg_mover* mover = World.object_cast<rfg_mover>(op);
    rfg_destroyable_shape* shape = mover != nullptr && mover->rdd.is_loaded() ? mover->rdd.get_data()->shape : nullptr;
    const rfg_subpiece_base* base = shape != nullptr ? &shape->parent->base_objects.to_pointer()[point.m_cdBodyB.m_shapeKey] : nullptr;
    unsigned char material = 0xFF;
    if (base == nullptr)
    {
        material = op->get_effect_material(point.m_cdBodyB.m_shapeKey, &pos, nullptr);
    }
    for (int i = 0; i < m_hits.getSize(); i++)
    {
        object* hit_op = object_from_world_obj((const hkpWorldObject*)m_hits[i].m_rootCollidableB->getOwner());
        if (hit_op != op)
        {
            continue;
        }
        if (base == nullptr)
        {
            vector hit_pos;
            havok_to_game_vector(hit_pos, m_hits[i].m_contact.m_position);
            if (hit_op->get_effect_material(point.m_cdBodyB.m_shapeKey, &hit_pos, nullptr) != material)
            {
                continue;
            }
        }
        else
        {
            const rfg_subpiece_base* hit_base = &shape->parent->base_objects.to_pointer()[m_hits[i].m_shapeKeyB];
            if ((unsigned short)hit_base->dlod_key != (unsigned short)base->dlod_key)
            {
                continue;
            }
        }
        hkpRootCdPoint& hit = m_hits[i];
        const float px = center.m_quad.m128_f32[0] - point.m_contact.m_position.m_quad.m128_f32[0];
        const float py = center.m_quad.m128_f32[1] - point.m_contact.m_position.m_quad.m128_f32[1];
        const float pz = center.m_quad.m128_f32[2] - point.m_contact.m_position.m_quad.m128_f32[2];
        const float hx = center.m_quad.m128_f32[0] - hit.m_contact.m_position.m_quad.m128_f32[0];
        const float hy = center.m_quad.m128_f32[1] - hit.m_contact.m_position.m_quad.m128_f32[1];
        const float hz = center.m_quad.m128_f32[2] - hit.m_contact.m_position.m_quad.m128_f32[2];
        if (py * py + px * px + pz * pz < hy * hy + hx * hx + hz * hz)
        {
            hit.m_contact.m_position = point.m_contact.m_position;
            hit.m_contact.m_separatingNormal = point.m_contact.m_separatingNormal;
        }
        bbox_expand(m_bboxes[i], pos);
        return;
    }
    bbox& box = m_bboxes.expandOne();
    box.min = pos;
    box.max = pos;
    hkpAllCdPointCollector::addCdPoint(point);
}

// 0x0074DC10
havok_blast_decal_collector::~havok_blast_decal_collector()
{
}

// 0x0074DCE0
// The surfaces around a blast (in.start, in.radius): a closest point query of a sphere of the radius, centered on the
// start moved into the safe broadphase shrunk by the radius, collected per surface (havok_blast_decal_collector). A
// surface counts when its size (the larger of its collected box and, on a destroyable mover, its subpiece's box)
// exceeds its distance from the start by Blast_decal_min_bbox_dim. Each hit's fraction is the distance divided by the
// radius; the sphere center is the hit's ray start and end. Returns whether there is a hit.
// Notes (decompiled behaviour, kept as is): unlike the havok_raycast.cpp queries, Havok_raycast_cs is not held and no
// object filter is set; a mover's subpiece box counts only with subpiece flag bit 0 (meaning not recovered).
static bool blast_decal_collect_points(const raycast_input& in, raycast_output& out)
{
    const vector bmin(Havok_safe_broadphase.min.x + in.radius, Havok_safe_broadphase.min.y + in.radius, Havok_safe_broadphase.min.z + in.radius);
    const vector bmax(Havok_safe_broadphase.max.x - in.radius, Havok_safe_broadphase.max.y - in.radius, Havok_safe_broadphase.max.z - in.radius);
    vector closest;
    bbox_find_closest_point(closest, bmin, bmax, in.start);
    hkVector4f center;
    game_to_havok_vector(center, closest);
    hkpSphereShape sphere(in.radius);
    hkTransformf transform;
    transform.m_rotation.setIdentity();
    transform.m_translation = center;
    hkpSimpleShapePhantom phantom(&sphere, transform, COLLIDE_RAY_DEFAULT);
    havok_blast_decal_collector collector;
    collector.center = center;
    Havok_world->getClosestPoints(phantom.getCollidable(), *Havok_world->m_collisionInput, collector);
    hkArray<hkpRootCdPoint, hkContainerHeapAllocator> results;
    for (int i = 0; i < collector.m_hits.getSize(); i++)
    {
        results.pushBack(collector.m_hits[i]);
    }
    for (int i = 0; i < results.getSize(); i++)
    {
        const hkpRootCdPoint& point = results[i];
        vector offset;
        havok_to_game_vector(offset, point.m_contact.m_position);
        offset.x = offset.x - in.start.x;
        offset.y = offset.y - in.start.y;
        offset.z = offset.z - in.start.z;
        const float fraction = offset.mag() / sphere.m_radius;
        float subpiece_dim = 0.0f;
        rfg_mover* mover = World.object_cast<rfg_mover>(object_from_world_obj(hkpGetRigidBody(point.m_rootCollidableB)));
        if (mover != nullptr && mover->rdd.is_loaded())
        {
            rfg_destroyable_shape* shape = mover->rdd.get_data()->shape;
            const rfg_subpiece* subpiece = shape->get_subpiece(point.m_shapeKeyB);
            if (subpiece != nullptr && (subpiece->flags & 1) != 0)
            {
                const rfg_subpiece_base& base = shape->parent->base_objects.to_pointer()[point.m_shapeKeyB];
                subpiece_dim = blast_decal_max_dim(base.prel_bmin, base.prel_bmax);
            }
        }
        const bbox& box = collector.m_bboxes[i];
        float size = blast_decal_max_dim(box.min, box.max);
        if (0.0f <= subpiece_dim - size)
        {
            size = subpiece_dim;
        }
        if ((double)fraction * (double)in.radius + (double)Blast_decal_min_bbox_dim < (double)size)
        {
            out.add_hit(center, center, fraction, point.m_contact.m_separatingNormal, &point.m_contact.m_position, point.m_rootCollidableB, point.m_shapeKeyB);
        }
    }
    const bool hit = out.m_hits.getSize() != 0;
    results.clearAndDeallocate();
    return hit;
}

// 0x0074E340 (:1535)
// A blast mark on every surface around the blast (blast_decal_collect_points) whose effect material has a blast decal,
// all under one decal handle and one random rotation. The size falls off with the square of the hit fraction from the
// blast's diameter at its center (surfaces under Blast_decal_min_radius get none), the alpha down to
// Blast_decal_min_alpha. Movers and debris need an extent of Blast_decal_min_bbox_dim and must not be destroyed. On a
// zone the decal faces the terrain normal under the hit (sized by the distance of the projected center instead); on
// other bodies a ray through the body's shape from the blast to just past the hit gives the face normal (tried back to
// front when it misses; no decal when both miss). The decal sits where the line from the blast along the normal comes
// closest to the hit. Older blast decals the blast box overlaps fade. Returns the effect material of a surface at the
// blast center (null when there is none).
// Notes (decompiled behaviour, kept as is): the shape ray's hit point is interpolated in the body's frame (relative to
// its position, unrotated) and compared with the world hit position, and with the forward fraction even when the back
// to front ray hit; the decal is dropped when the two are within 0.1. The blast box is faded with handle -1 when no
// decal was made.
rfg_effect_material* object_decal_create_blast_decals(const vector& position, float radius)
{
    rfg_effect_material* center_material = nullptr;
    // Magic number: a blast narrower than the minimum surface extent makes no decals.
    if (!R_enable_decals || (double)radius < (double)Blast_decal_min_bbox_dim * 0.5)
    {
        return nullptr;
    }
    raycast_input in;
    in.start = position;
    in.radius = radius;
    raycast_output out;
    blast_decal_collect_points(in, out);
    const float rotation = fl_rand_range(0.0f, Decal_full_turn);
    unsigned int decal_handle = 0xFFFFFFFF;
    const double diameter = (double)radius * 2.0;
    for (int i = 0; i < out.m_hits.getSize(); i++)
    {
        const raycast_output::hit_info& hit = out.m_hits[i];
        object* hit_obj = World.object_from_handle(hit.hit_object_handle);
        if (hit_obj == nullptr)
        {
            continue;
        }
        if (hit_obj->get_obj_type() == OT_MOVER || hit_obj->get_obj_type() == OT_DEBRIS)
        {
            if (hit_obj->obj_flags.destroyed)
            {
                continue;
            }
            vector bmin;
            vector bmax;
            hit_obj->get_local_bbox(bmin, bmax);
            if ((double)Blast_decal_min_bbox_dim > (double)blast_decal_max_dim(bmin, bmax))
            {
                continue;
            }
        }
        const float size = (float)((double)hit.hit_fraction * (double)hit.hit_fraction * (0.0 - diameter) + diameter);
        if ((double)Blast_decal_min_radius > (double)size)
        {
            continue;
        }
        rfg_effect_material* material = rfg_get_effect_material_by_index(hit.hit_effect_material);
        const float dx = hit.hit_pos.x - position.x;
        const float dy = hit.hit_pos.y - position.y;
        const float dz = hit.hit_pos.z - position.z;
        const float dist_sq = (float)((double)dx * (double)dx + (double)dy * (double)dy + (double)dz * (double)dz);
        // Magic number: a surface within 0.2 (squared distance) of the blast is at its center.
        if ((double)dist_sq < 0.2)
        {
            center_material = material;
        }
        if (material == nullptr || material->blast_decal == -1)
        {
            continue;
        }
        rl_decal_parameters params;
        decal_info_get(&params, material->blast_decal);
        float width = size;
        if (hit_obj->get_obj_type() == OT_ZONE)
        {
            vector normal = UP_VECTOR;
            float height;
            havok_terrain_height(height, hit.hit_pos.x, hit.hit_pos.z, false, &normal);
            normal.normalize();
            params.m_transform.make_rvec(normal, RIGHT_VECTOR);
            line ray;
            ray.point = position;
            ray.dir.set(-normal.x, -normal.y, -normal.z);
            ix_closest_point_on_ray(hit.hit_pos, ray, params.m_pos);
            const float t = vec_dist(params.m_pos, position) / radius;
            width = (float)((double)t * (double)t * (0.0 - diameter) + diameter);
        }
        else
        {
            const unsigned int body = hit.hit_havok_handle;
            const hkpShape* shape = havok_body_get_shape(body);
            if (shape == nullptr)
            {
                params.m_transform.make(hit.hit_normal);
                params.m_pos = hit.hit_pos;
            }
            else
            {
                vector body_pos;
                matrix body_orient;
                havok_body_get_pos_orient(body, body_pos, body_orient);
                // Magic number: the ray starts and ends 0.1 m out along the hit normal (a double constant).
                const vector out_offset((float)((double)hit.hit_normal.x * 0.1), (float)((double)hit.hit_normal.y * 0.1), (float)((double)hit.hit_normal.z * 0.1));
                const vector from(position.x - body_pos.x + out_offset.x, position.y - body_pos.y + out_offset.y, position.z - body_pos.z + out_offset.z);
                const vector to(hit.hit_pos.x - body_pos.x - out_offset.x, hit.hit_pos.y - body_pos.y - out_offset.y, hit.hit_pos.z - body_pos.z - out_offset.z);
                hkVector4f hk_from;
                game_to_havok_vector(hk_from, from.copy_rotate(body_orient));
                hkVector4f hk_to;
                game_to_havok_vector(hk_to, to.copy_rotate(body_orient));
                hkpShapeRayCastInput ray_in;
                ray_in.m_from = hk_from;
                ray_in.m_to = hk_to;
                hkpShapeRayCastOutput ray_out;
                if (!shape->castRay(ray_in, ray_out).m_bool)
                {
                    ray_in.m_from = hk_to;
                    ray_in.m_to = hk_from;
                    if (!shape->castRay(ray_in, ray_out).m_bool)
                    {
                        continue;
                    }
                }
                const float f = ray_out.m_hitFraction;
                const float g = 1.0f - f;
                vector miss(g * from.x + to.x * f - hit.hit_pos.x, g * from.y + to.y * f - hit.hit_pos.y, g * from.z + to.z * f - hit.hit_pos.z);
                // Magic number: 0.01 is 0.1 squared.
                if (0.01f > miss.mag_squared())
                {
                    continue;
                }
                vector normal;
                havok_to_game_vector(normal, ray_out.m_normal);
                normal.unrotate(body_orient);
                normal.normalize_safe();
                params.m_transform.make_quick(normal);
                line ray;
                ray.point = position;
                ray.dir.set(-normal.x, -normal.y, -normal.z);
                ix_closest_point_on_ray(hit.hit_pos, ray, params.m_pos);
            }
        }
        const double fraction_sq = (double)hit.hit_fraction * (double)hit.hit_fraction;
        params.m_color.alpha = (float)(1.0 - fraction_sq * (1.0 - (double)Blast_decal_min_alpha));
        params.m_planar_params.m_width = width;
        params.m_planar_params.m_length = width;
        params.m_transform.rotate_around_local_z(rotation);
        decal_handle = object_decal_apply(hit_obj, params, 0.0f, 0xFFFFFFFF, 0xFFFFFFFF, decal_handle);
    }
    const vector bmin(position.x - radius, position.y - radius, position.z - radius);
    const vector bmax(position.x + radius, position.y + radius, position.z + radius);
    fade_overlapped_blast_decals(bmin, bmax, decal_handle);
    return center_material;
}

// 0x0074BE60 (:1740)
// A decal on a player joins its blood decal queue (the oldest is released past 10).
void object_decal_player_track_blood_decal(object* op, unsigned int decal_handle)
{
    if (decal_handle == 0xFFFFFFFF || op == nullptr || op->get_obj_type() != OT_HUMAN || op->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        return;
    }
    player* pp = (player*)op;
    // Magic number: the queue holds 10 decals.
    pp->blood_decals_fade_index++;
    if (pp->blood_decals_fade_index > 10)
    {
        pp->blood_decals_fade_index = 10;
    }
    pp->blood_decals.add(decal_handle, 1.0f);
}

// 0x00743AD0 (:1755)
void object_decal_system_init()
{
    blood_decal_load_table();
    glass_decal_info_init();
}

// 0x00743AE0 (:1762)
// Empty in rfg.exe.
void object_decal_level_init()
{
}
