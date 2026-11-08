// Original: volition/rfg/code/hud/in_world_gps.cpp
//
// The chevrons along the GPS route in the world: a spline clone mesh per segment of the route, rebuilt on the render
// thread when the route changes.

#include "volition/rfg/code/hud/in_world_gps.h"

#include "volition/render_lib/code/rl_clone_mesh_instance.h"
#include "volition/render_lib/code/rl_material_defines.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_partition_system.h"
#include "volition/rfg/code/game_render/game_render_task.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/geom/foliage_mesh.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/hud/in_world_gps_shared.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/rfg.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/spline.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/util/varray.h"

#include "keen/base/trace.hpp"

// size 0x8
struct gps_path_segment
{
    int first_knot; // 0x00
    int num_knots; // 0x04
};

// One run of the route's spline points, split where two points coincide.
struct gps_spline_segment
{
    int start; // 0x00
    int count; // 0x04
};

// Magic numbers: the sizes of rfg.exe's arrays.
static const int Max_gps_segments = 32;
static const int Max_gps_knots = 128;

// ---- file-static globals
static bool R_in_world_gps = true; // 0x01660FF1
static int Num_chevrons_per_gps_segment = 2; // 0x0166117C
static float Gps_spline_max_render_dist = 250.0f; // 0x01661180
static float Gps_spline_fade_render_dist = 20.0f; // 0x01661184
static float Gps_spline_refinement_u_delta = 0.1f; // 0x01661188
static float Gps_spline_refinement_desired_knot_spacing = 10.0f; // 0x0166118C
static float Gps_spline_refinement_final_knot_spacing = 5.0f; // 0x01661190
static farray<rl_clone_mesh_instance*, Max_gps_segments> Gps_instances; // 0x01662110
static rl_clone_mesh_pool* Gps_instance_pool; // 0x02B95914
static gps_path_segment Gps_refined_segments[Max_gps_segments]; // 0x02B95918
static bool Gps_render_dirty; // 0x02B9DD36
int Num_gps_refined_path_segments; // 0x02B9DD38
static foliage_mesh* Chevron_fmesh; // 0x02B9DD3C
static float Chevron_y_offset; // 0x02B9DD40
static float Gps_spline_tension; // 0x02B9DD44
static game_render_task Update_render_task_gps; // 0x02BCE0DC
static rl_clone_mesh_element Gps_instance_element_array[Max_gps_knots]; // 0x02BD80D0
static vector Gps_refined_path_knots[Max_gps_knots]; // 0x02BE5228

// 0x008772B0 (:115)
void in_world_gps_system_init()
{
    in_world_gps_generate_stream_data(&Common_mempool, &Permanent_mempool, PLATFORM_PC);
    Gps_instance_pool = rl_clone_mesh_pool::create();
    if (Gps_instance_pool == nullptr)
    {
        return;
    }
    Gps_instance_pool->init(Max_gps_knots, Gps_instance_element_array);
    Chevron_fmesh = in_world_gps_get_chevron_fmesh();
    vector bmin;
    vector bmax;
    Chevron_fmesh->m_shared_header.m_mesh.to_pointer()->get_subpiece_local_bb(0, bmin, bmax);
    Chevron_y_offset = 0.25f - bmin.y; // Magic number: a quarter meter above the ground.
    Update_render_task_gps.m_callback = in_world_gps_update_render;
    game_render_update_render_task_register(&Update_render_task_gps);
    Num_gps_refined_path_segments = 0;
    Gps_render_dirty = false;
}

// 0x0084BCA0 (:138)
void in_world_gps_register_path(path_info* gps_path)
{
    mempool_mark_scope scope(Vlib_scratch_pool);
    if (gps_path == nullptr)
    {
        Num_gps_refined_path_segments = 0;
        return;
    }
    const int num_points = gps_path->num_spline_pts;
    vector* points = (vector*)Vlib_scratch_pool->alloc(num_points * sizeof(vector), 0);
    gps_spline_segment* segments = (gps_spline_segment*)Vlib_scratch_pool->alloc((num_points - 3) * sizeof(gps_spline_segment), 0);
    points[0].set(gps_path->spline_points[0].x, gps_path->spline_heights[0], gps_path->spline_points[0].y);
    int num_segments = 0;
    for (int i = 1; i < num_points; i++)
    {
        points[i].set(gps_path->spline_points[i].x, gps_path->spline_heights[i], gps_path->spline_points[i].y);
        // Magic number: points within 10 cm are the seam between two runs.
        if (vec_dist_squared(points[i], points[i - 1]) < 0.010000001f)
        {
            if (num_segments > 0)
            {
                segments[num_segments - 1].count = i - segments[num_segments - 1].start + 1;
            }
            segments[num_segments].start = i - 1;
            num_segments++;
        }
    }
    // Original bug, kept: with no seam found this writes one element before the array, into the scratch pool's last
    // allocation, and nothing below reads it.
    segments[num_segments - 1].count = num_points - segments[num_segments - 1].start;
    int num_knots = 0;
    for (int j = 0; j < num_segments; j++)
    {
        if (Num_gps_refined_path_segments >= Max_gps_segments)
        {
            break;
        }
        const gps_spline_segment& segment = segments[j];
        if (segment.count <= 3)
        {
            continue;
        }
        if (num_knots + 4 >= Max_gps_knots)
        {
            break;
        }
        gps_path_segment& refined = Gps_refined_segments[Num_gps_refined_path_segments];
        Num_gps_refined_path_segments++;
        refined.first_knot = num_knots;
        Gps_refined_path_knots[num_knots] = points[segment.start];
        Gps_refined_path_knots[num_knots + 1] = points[segment.start + 1];
        const vector last_point = points[segment.start + segment.count - 2];
        // Original bug, kept: this indexes the route's points with the knot number, not with segment.start.
        vector previous = points[refined.first_knot + 1];
        num_knots += 2;
        cardinal_spline spline(segment.count, &points[segment.start], 0.0f);
        const float last_u = (float)(segment.count - 2);
        // Magic number: 125, leaving room for the two end knots.
        for (float u = Gps_spline_refinement_u_delta + 1.0f; u < last_u && num_knots <= 125; u += Gps_spline_refinement_u_delta)
        {
            vector point;
            spline.get_point(point, u);
            if (vec_dist_squared(previous, point) >= Gps_spline_refinement_desired_knot_spacing * Gps_spline_refinement_desired_knot_spacing && vec_dist_squared(last_point, point) >= Gps_spline_refinement_final_knot_spacing * Gps_spline_refinement_final_knot_spacing)
            {
                // Deviation: rfg.exe's height starts as whatever the stack slot holds, a pointer's bits, when the terrain
                // has no answer. Here it starts at 0.
                float height = 0.0f;
                havok_terrain_height(height, point.x, point.z, false, nullptr);
                if (height - point.y >= 0.0f)
                {
                    point.y = height;
                }
                previous = point;
                Gps_refined_path_knots[num_knots] = point;
                num_knots++;
            }
        }
        Gps_refined_path_knots[num_knots] = last_point;
        Gps_refined_path_knots[num_knots + 1] = points[segment.start + segment.count - 1];
        num_knots += 2;
        refined.num_knots = num_knots - refined.first_knot;
    }
    Gps_render_dirty = true;
}

// 0x00869790 (:248)
void in_world_gps_update_render(void* ignored, rl_renderer*)
{
    (void)ignored;
    if (((Num_gps_refined_path_segments == 0 || !R_in_world_gps) && Gps_instances.size() != 0) || (Num_gps_refined_path_segments != 0 && Gps_render_dirty))
    {
        for (int i = 0; i < Gps_instances.size(); i++)
        {
            if (Gps_instances[i] != nullptr)
            {
                Gps_instances[i]->release();
                Gps_instances[i] = nullptr;
            }
        }
        Gps_instances.clear();
    }
    if (!R_in_world_gps || Num_gps_refined_path_segments == 0 || !Gps_render_dirty)
    {
        return;
    }
    // Magic numbers: a pool of 32 instances; each instance draws 8 sections of the spline.
    int knots_left = 32;
    int instances_left = 32;
    for (int s = 0; s < Num_gps_refined_path_segments; s++)
    {
        int section_knots = Gps_refined_segments[s].num_knots - 3;
        if (knots_left <= section_knots)
        {
            section_knots = knots_left;
        }
        knots_left -= section_knots;
        if (section_knots == 0)
        {
            break;
        }
        int num_instances = section_knots / 8;
        if (section_knots % 8 != 0)
        {
            num_instances++;
        }
        if (instances_left <= num_instances)
        {
            num_instances = instances_left;
        }
        instances_left -= num_instances;
        if (num_instances == 0)
        {
            break;
        }
        for (int i = 0; i < num_instances; i++)
        {
            mempool_mark_scope scope(Vlib_scratch_pool);
            const int count = section_knots > 7 ? 8 : section_knots;
            section_knots -= count;
            const int num_elements = count + 3;
            rl_clone_mesh_element* elements = (rl_clone_mesh_element*)Vlib_scratch_pool->alloc(num_elements * sizeof(rl_clone_mesh_element), 0);
            for (int k = 0; k < num_elements; k++)
            {
                const vector& knot = Gps_refined_path_knots[Gps_refined_segments[s].first_knot + i * 8 + k];
                rl_clone_spline_info info;
                info.knot_pos.set(knot.x, Chevron_y_offset + knot.y, knot.z);
                // Deviation: rfg.exe leaves the offset as an uninitialized stack slot.
                info.knot_parameter_offset = 0.0f;
                elements[k].set(info);
            }
            rl_clone_mesh_instance* instance = rl_clone_mesh_instance::create();
            if (instance == nullptr)
            {
                break;
            }
            instance->init(Chevron_fmesh->m_shared_header.m_mesh.to_pointer(), Chevron_fmesh->m_shared_header.m_material_map.to_pointer(), Gps_instance_pool, elements, num_elements, RLCMET_SPLINE, Gps_spline_tension, Num_chevrons_per_gps_segment);
            instance->set_position(ZERO_VECTOR);
            instance->set_rotation(IDENTITY_MATRIX);
            rl_clone_mesh_fade_range fade_range;
            fade_range.start = Gps_spline_max_render_dist - Gps_spline_fade_render_dist;
            fade_range.end = Gps_spline_max_render_dist;
            instance->set_fade_range(fade_range);
            instance->set_lod_descs(Chevron_fmesh->m_lod_descs.to_pointer(), Chevron_fmesh->m_num_lods);
            // Magic numbers: flag 0x40 of the scene entity is cleared, then the entity goes back into its partition unless
            // flag 2 is set. rfg.exe inlines a setter of the scene entity here.
            instance->set_flag(0x40, false);
            if (instance->get_partition_system() != nullptr && !instance->get_flag(2))
            {
                instance->get_partition_system()->insert(instance);
            }
            Gps_instances.add(instance);
        }
    }
    Gps_render_dirty = false;
}
