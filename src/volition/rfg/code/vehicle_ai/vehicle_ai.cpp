// Original: volition/rfg/code/vehicle_ai/vehicle_ai.cpp
//
// Not ported: unused_dcf_vehicle_stop and unused_dcf_vai (debug console commands). The city lane half (the lane choosers,
// vehicle_create_ambient_path_from_points) is ported but cannot run while no traffic data is loaded: a nav mesh node is
// not a city spline (spline_flags 0x80).

#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/object_debris.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/traffic/traffic.h"
#include "volition/rfg/code/traffic/traffic_shared.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_flyer.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle_ai/intersection_controller.h"
#include "volition/rfg/code/vehicle_ai/lane_pathfind.h"
#include "volition/rfg/code/vehicle_ai/vehicle_idle_ai.h"
#include "volition/rfg/code/vehicle_ai/vehicle_lane_common.h"
#include "volition/rfg/code/vehicle_ai/vehicle_navmesh_common.h"
#include "volition/rfg/code/vehicle_ai/vehicle_navmesh_path_create.h"
#include "volition/rfg/code/vehicle_ai/vehicle_path_request.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/os/frametime.h"

// size 0x8
struct vai_mode_callback_info
{
    void (*exit)(vehicle_ai*);
    void (*process)(vehicle*);
};

// 0x01339F80: both states use the idle pair.
static const vai_mode_callback_info Vai_state_callbacks[NUM_VEHICLE_AI_STATES] = {
    { vehicle_ai_exit_idle, vehicle_ai_process_idle },
    { vehicle_ai_exit_idle, vehicle_ai_process_idle },
};

vehicle_ai_debug_info Vehicle_ai_debug_info;

static const float VPF_BAD_START_DIST_SQ = 9.0f; // 0x01339F90
static const float VPF_BAD_DEST_DIST_SQ = 25.0f;
static const float CLOSEST_LANE_RADIUS = 3.0f;
static const int LANE_INCLUDE_TRAFFIC = 8;
static const int LANE_EXCLUDE_TRAFFIC = 0x104;
static const float ROUTE_INDEX_MAX_DIST_SQ = 100.0f; // a point within 10 m of a route segment is on it
static const float TERRAIN_CLEARANCE_MIN = 7.5f; // a flyer closer than this over the terrain climbs to TERRAIN_CLEARANCE
static const float TERRAIN_CLEARANCE = 10.0f;

// 0x00B8B230
vehicle_ai::vehicle_ai()
{
}

// 0x00B729C0
vehicle_ai::~vehicle_ai()
{
}

// 0x00B645E0
stuck_info::stuck_info()
{
}

// 0x00B64600
vehicle_steer_info::vehicle_steer_info()
{
}

// 0x00B64630
route_segment::route_segment()
{
}

// 0x00B768B0
vehicle_route_info::vehicle_route_info()
{
}

// 0x005DCCB0 (.h:203)
vehicle_path_info::vehicle_path_info()
{
    start_pos.zero();
    start_nav_cell_handle = 0xFFFFFFFFFFFFFFFFULL;
    end_pos.zero();
    end_nav_cell_handle = 0xFFFFFFFFFFFFFFFFULL;
    closest_road_spline = 0xFFFFFFFF;
    error_code = PR_UNKNOWN;
    closest_road_index = 0.0f;
    memset(&flags, 0, sizeof(flags));
}

// 0x00B62F00 (.h:223)
vehicle_pathfind_record::vehicle_pathfind_record()
{
}

// 0x00BC2E30 (.h:224)
vehicle_pathfind_record::vehicle_pathfind_record(const vector& start_, const vector& dest_, vehicle_pathfind_results result_)
    : start(start_), dest(dest_), result(result_)
{
}

// 0x00BC2E70 (.h:739)
vehicle_ai_debug_info::vehicle_ai_debug_info()
    : debug_vehicle_handle(0xFFFFFFFF)
{
    memset(&flags, 0, sizeof(flags));
}

// 0x00BC3EC0 (:236)
vehicle_ambient_pathfind_info::vehicle_ambient_pathfind_info(int _num_points, const vector* _points, bool _allow_uturn, const lane_spline* _force_start_lane)
    : num_points(_num_points), points(_points), allow_uturn(_allow_uturn), force_start_lane(_force_start_lane), supress_error(false)
{
}

// 0x00BC3EF0 (:763)
static int get_state_time_to_stuck(const vehicle_ai* vai)
{
    return vai->vai_flags.u_turn_active ? 7000 : 3000;
}

// 0x00BCD300 (:769): LTCG, vp in ECX. Damages the object_movers and fades the debris under the vehicle.
static void do_debris_cleanup(vehicle* vp, clear_debris_states clear_debris_state)
{
    vector bmin, bmax;
    vp->get_local_bbox(bmin, bmax);
    bmin.y -= 0.25f;
    bmax.y = bmin.y + 0.5f;
    unsigned int movers[10];
    const int num_movers = World.collect_objects<object_mover>(bmin, bmax, vp->pos, vp->orient, movers, 10, NULL, NULL);
    for (int i = 0; i < num_movers; i++)
    {
        object_mover* mover = World.object_from_handle<object_mover>(movers[i]);
        if (mover == NULL || (mover->mover_flags & 4) == 0)
        {
            continue;
        }
        switch (clear_debris_state)
        {
        case CDS_DAMAGE1:
            mover->apply_damage(250.0f, (damaged_by_types)1, mover->pos, mover->orient.uvec, NULL, 0xFFFFFFFF, false);
            break;
        case CDS_DAMAGE2:
            mover->apply_damage(500.0f, (damaged_by_types)1, mover->pos, mover->orient.uvec, NULL, 0xFFFFFFFF, false);
            break;
        case CDS_DAMAGE3:
            mover->apply_damage(50000.0f, (damaged_by_types)1, mover->pos, mover->orient.uvec, NULL, 0xFFFFFFFF, false);
            break;
        case CDS_DELETE:
            object_mover_mark_for_fade(mover, 0);
            break;
        default:
            break;
        }
    }
    unsigned int debris[10];
    const int num_debris = World.collect_objects<object_debris>(bmin, bmax, vp->pos, vp->orient, debris, 10, NULL, NULL);
    for (int i = 0; i < num_debris; i++)
    {
        object_debris* d_obj = World.object_from_handle<object_debris>(debris[i]);
        if (d_obj != NULL)
        {
            object_debris_mark_for_fade(d_obj, 0, 1000, false);
        }
    }
}

// 0x00BC9010 (:836)
void vehicle_ai_get_route_segment_info(const vehicle_route_info& route_info, const route_segment*& start_segment, int& num_segments)
{
    start_segment = vehicle_ai_get_center_route_segment(route_info);
    num_segments = start_segment != NULL ? route_info.num_route_segments - route_info.center_info.route_index : 0;
}

// 0x00BC3F10 (:853)
const route_segment* vehicle_ai_get_center_route_segment(const vehicle_route_info& route_info)
{
    if (route_info.flags.m_route_segment_inited && route_info.num_route_segments > 0 && route_info.center_info.route_index >= 0 && route_info.center_info.route_index < route_info.num_route_segments)
    {
        return &route_info.route[route_info.center_info.route_index];
    }
    return NULL;
}

// 0x00BC3F50 (:871)
const route_segment* vehicle_ai_get_next_route_segment(const vehicle_route_info& route_info)
{
    if (route_info.flags.m_route_segment_inited && route_info.num_route_segments > 0 && route_info.center_info.route_index >= 0 && route_info.center_info.route_index + 1 < route_info.num_route_segments)
    {
        return &route_info.route[route_info.center_info.route_index + 1];
    }
    return NULL;
}

// 0x00BC3F90 (:893)
const route_segment* get_front_bumper_route_segment(const vehicle_route_info& route_info)
{
    if (route_info.flags.m_route_segment_inited && route_info.bumper_info.route_index >= 0 && route_info.bumper_info.route_index < route_info.num_route_segments)
    {
        return &route_info.route[route_info.bumper_info.route_index];
    }
    return NULL;
}

// 0x00BC3FC0 (:909)
void vehicle_ai_flush_route(vehicle_route_info& route_info)
{
    route_info.flags.m_route_segment_inited = 0;
    route_info.num_route_segments = 0;
    route_info.center_info.route_index = 0;
    route_info.bumper_info.route_index = 0;
    route_info.last_next_spline = NULL;
}

// 0x00BCD500 (:924)
void vehicle_ai_add_route_segment(vehicle_route_info& route_info, const lane_spline_base* lsp, bool ambient_path, bool navmesh_path, bool from_rails)
{
    if (lsp == NULL)
    {
        return;
    }
    route_segment* segment;
    const short count = route_info.num_route_segments;
    if (count < 5)
    {
        for (int i = 0; i < count; i++)
        {
            if (route_info.route[i].base_spline == lsp)
            {
                return;
            }
        }
        segment = &route_info.route[count];
        route_info.num_route_segments = count + 1;
    }
    else
    {
        // Original bug kept: the oldest segment falls out, but center_info and bumper_info keep their indices.
        memmove(&route_info.route[0], &route_info.route[1], 4 * sizeof(route_segment));
        segment = &route_info.route[4];
    }
    segment->flags.terminal = 0;
    segment->flags.from_ambient_path = ambient_path;
    segment->flags.from_navmesh_path = navmesh_path;
    segment->flags.signal_processed = 0;
    segment->flags.stopped_for_sign = 0;
    segment->base_spline = const_cast<lane_spline_base*>(lsp);
    segment->cb_spline.set_points(lsp->num_knots, lsp->knots.to_pointer(), lsp->in_handles.to_pointer(), lsp->out_handles.to_pointer());
    segment->length = lsp->arc_length[0];
    segment->num_knots_minus_one = lsp->num_knots - 1;
    segment->speed_limit = get_traffic_speed_from_lane_spline(lsp);
    route_info.flags.m_route_segment_inited = 1;
}

// 0x00BC9070 (:992)
void vehicle_ai_free_all_paths(vehicle_ai* vai)
{
    vehicle_free_ls_path(&vai->navmesh_path);
    vehicle_ai_clear_path_info(vai);
}

// 0x00BC3FF0 (:999)
void vehicle_ai_clear_path_info(vehicle_ai* vai)
{
    vehicle_path_info& info = vai->current_path_info;
    info.end_pos = ZERO_VECTOR;
    info.start_pos = ZERO_VECTOR;
    info.end_nav_cell_handle = 0xFFFFFFFFFFFFFFFFULL;
    info.start_nav_cell_handle = 0xFFFFFFFFFFFFFFFFULL;
    memset(&info.flags, 0, sizeof(info.flags));
    info.closest_road_spline = 0xFFFFFFFF;
    info.closest_road_index = 0.0f;
}

// 0x00BC4070 (:1022)
static int compare_lane_vehicle_counts(const void* elem1, const void* elem2)
{
    const int count1 = static_cast<const int*>(elem1)[1];
    const int count2 = static_cast<const int*>(elem2)[1];
    if (count1 < count2)
    {
        return -1;
    }
    return count2 < count1 ? 1 : 0;
}

// A ray 10 m up and down from pos against the static level.
static bool raycast_level_around(const vector& pos)
{
    raycast_input in;
    in.radius = 0.0f;
    in.start = pos;
    in.start.y += 10.0f;
    in.end = pos;
    in.end.y -= 10.0f;
    in.m_filter_info = COLLIDE_RAY_DEFAULT;
    in.filter_params = NULL;
    in.filter_callback = NULL;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 0;
    raycast_output out;
    return havok_raycast_against_static_level_only(in, out);
}

// 0x00BD47A0 (:1052)
static bool check_level_geometry_beneath_spline(const vector& pos)
{
    return raycast_level_around(pos);
}

// A lane's entry in its next_splines array. rfg.exe reads the value as an absolute pointer in the lane choosers, unlike
// vehicle_choose_next_lane_straight, which adds the entry's own address.
static const lane_spline* next_spline_absolute(const lane_spline* lsp, int index)
{
    return reinterpret_cast<const lane_spline* const*>(lsp->next_splines.to_pointer())[index];
}

static const lane_spline* next_spline_relative(const lane_spline* lsp, int index)
{
    return lsp->next_splines.to_pointer()[index].to_pointer();
}

// 0x00BCD5E0 (:1075): up to 4 lanes downstream, while there is no fork. Flag 0x800 is an obstruction.
static bool check_downstream_for_obstructions(const lane_spline* lsp)
{
    for (int i = 4; lsp != NULL && i > 0; i--)
    {
        if ((lsp->spline_flags & 0x800) != 0)
        {
            return true;
        }
        if (lsp->num_next_splines != 1)
        {
            return false;
        }
        lsp = next_spline_relative(lsp, 0);
    }
    return false;
}

// 0x00BC4090 (:1103)
float get_spline_curvature(const cubic_bezier_spline& spline, float u)
{
    vector tan, sec;
    spline.get_tangent(tan, u);
    spline.get_second_derivative(sec, u);
    float mag = sqrtf(tan.x * tan.x + tan.y * tan.y + tan.z * tan.z);
    mag = mag * mag * mag;
    if (mag <= 0.001f)
    {
        mag = 0.001f;
    }
    const float cx = sec.z * tan.y - sec.y * tan.z;
    const float cy = tan.z * sec.x - sec.z * tan.x;
    const float cz = sec.y * tan.x - sec.x * tan.y;
    return sqrtf(cz * cz + cy * cy + cx * cx) / mag;
}

// Picks at random a next lane that is not flagged 0x100, whose flag 0x200 equals want_highway and which does not have
// the excluded flag.
static const lane_spline* choose_next_lane_by_kind(const lane_spline* lsp, bool want_highway, unsigned int exclude_flag)
{
    const lane_spline* candidates[30];
    int count = 0;
    for (int i = 0; i < lsp->num_next_splines; i++)
    {
        const lane_spline* next = next_spline_absolute(lsp, i);
        const unsigned short flags = (unsigned short)next->spline_flags;
        if ((flags & 0x100) == 0 && ((flags & 0x200) != 0) == want_highway && (flags & exclude_flag) == 0)
        {
            candidates[count++] = next;
        }
    }
    return count > 0 ? candidates[int_rand_range(0, count - 1)] : NULL;
}

// 0x00BC9090 (:1290)
static const lane_spline* choose_highway_next_lane_base(const lane_spline* lsp)
{
    return choose_next_lane_by_kind(lsp, true, 0x2000);
}

// 0x00BC9110 (:1326)
static const lane_spline* choose_non_highway_next_lane_base(const lane_spline* lsp)
{
    return choose_next_lane_by_kind(lsp, false, 0x1000);
}

// 0x00BCD620 (:1364): LTCG, vp in EAX, in_lane in ESI. Preference 1 asks for a highway, 2 for anything else.
static const lane_spline* vehicle_road_preference_choose_next_lane_random(const vehicle* vp, const lane_spline* in_lane)
{
    const int preference = vp->info->m_road_preference;
    if (preference != 1 && preference != 2)
    {
        return NULL;
    }
    const bool highway = preference == 1;
    const lane_spline* chosen = highway ? choose_highway_next_lane_base(in_lane) : choose_non_highway_next_lane_base(in_lane);
    if (chosen != NULL)
    {
        return chosen;
    }
    const unsigned short flags = (unsigned short)in_lane->spline_flags;
    if ((flags & (highway ? 0x2000 : 0x1000)) == 0)
    {
        return NULL;
    }
    const lane_spline* neighbours[2] = { in_lane->left.to_pointer(), in_lane->right.to_pointer() };
    for (int i = 0; i < 2; i++)
    {
        if (neighbours[i] != NULL && (in_lane->spline_flags & 1) == 0)
        {
            chosen = highway ? choose_highway_next_lane_base(neighbours[i]) : choose_non_highway_next_lane_base(neighbours[i]);
            if (chosen != NULL)
            {
                return chosen;
            }
        }
    }
    return NULL;
}

// 0x00BCF920 (:1435)
const lane_spline* vehicle_choose_next_lane_straight(const lane_spline* cur_lane)
{
    int best = -1;
    float best_curvature = 3.4028235e+38f;
    for (int i = 0; i < cur_lane->num_next_splines; i++)
    {
        const lane_spline* next = next_spline_relative(cur_lane, i);
        if ((next->spline_flags & 0x100) == 0)
        {
            const float curvature = fabsf(spline_common_get_curvature_on_lane_spline(next, (float)(next->num_knots - 1) * 0.5f));
            if (curvature < best_curvature)
            {
                best = i;
                best_curvature = curvature;
            }
        }
    }
    return best != -1 ? next_spline_relative(cur_lane, best) : NULL;
}

// 0x00BD4890 (:1470)
const lane_spline_base* vehicle_choose_next_lane_spline_random(vehicle_ai* vai, const lane_spline_base* cur_lsp, bool straight, bool no_raycast)
{
    const lane_spline* city = get_city_spline_from_base_spline(cur_lsp);
    if (city == NULL)
    {
        return NULL;
    }
    const vehicle* vp = vai->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(vai->vehicle_handle) : NULL;
    const lane_spline* preferred = vehicle_road_preference_choose_next_lane_random(vp, city);
    if (preferred != NULL)
    {
        return preferred;
    }
    const lane_spline* drivable[30];
    int num_drivable = 0;
    for (int i = 0; i < city->num_next_splines; i++)
    {
        const lane_spline* next = next_spline_absolute(city, i);
        if ((next->spline_flags & 0x100) == 0 && (next->spline_flags & 8) != 0)
        {
            drivable[num_drivable++] = next;
        }
    }
    if (num_drivable == 0)
    {
        return NULL;
    }
    if (num_drivable == 1)
    {
        return drivable[0];
    }
    const lane_spline* candidates[30];
    int num_candidates = 0;
    if (vrand() % 100 <= 80)
    {
        // 81% of the time lanes with an obstruction ahead are avoided.
        for (int i = 0; i < num_drivable; i++)
        {
            if (!check_downstream_for_obstructions(drivable[i]))
            {
                candidates[num_candidates++] = drivable[i];
            }
        }
        if (num_candidates == 1)
        {
            return candidates[0];
        }
    }
    if (num_candidates == 0)
    {
        num_candidates = num_drivable;
        memcpy(candidates, drivable, num_drivable * sizeof(drivable[0]));
    }
    if (straight)
    {
        int best = -1;
        float best_curvature = 3.4028235e+38f;
        for (int i = 0; i < num_candidates; i++)
        {
            const float curvature = fabsf(spline_common_get_curvature_on_lane_spline(candidates[i], (float)(candidates[i]->num_knots - 1) * 0.5f));
            if (curvature < best_curvature)
            {
                best = i;
                best_curvature = curvature;
            }
        }
        if (best != -1)
        {
            const lane_spline* lane = candidates[best];
            // Original quirk kept: no_raycast is what asks for the raycast here, the opposite of the other branch.
            if (!no_raycast || check_level_geometry_beneath_spline(lane->knots.to_pointer()[lane->num_knots - 1]))
            {
                return lane;
            }
        }
        return NULL;
    }
    // Least busy lanes first, in random order within a vehicle count.
    struct lane_count
    {
        const lane_spline* lane;
        int count;
    };
    lane_count counts[8];
    for (int i = 0; i < num_candidates; i++)
    {
        counts[i].lane = candidates[i];
        counts[i].count = traffic_count_vehicles_on_spline(candidates[i], true);
    }
    qsort(counts, num_candidates, sizeof(lane_count), compare_lane_vehicle_counts);
    int group_first[8];
    int group_last[8];
    int num_groups = 0;
    int first = 0;
    int current_count = counts[0].count;
    for (int i = 0; i < num_candidates; i++)
    {
        if (counts[i].count != current_count)
        {
            group_first[num_groups] = first;
            group_last[num_groups] = i - 1;
            num_groups++;
            first = i;
            current_count = counts[i].count;
        }
    }
    group_first[num_groups] = first;
    group_last[num_groups] = num_candidates - 1;
    num_groups++;
    for (int g = 0; g < num_groups; g++)
    {
        const int size = group_last[g] - group_first[g] + 1;
        const unsigned int offset = vrand() % (unsigned int)size;
        for (int i = 0; i < size; i++)
        {
            const lane_spline* lane = counts[((int)offset + i) % size + group_first[g]].lane;
            if (no_raycast || raycast_level_around(lane->knots.to_pointer()[lane->num_knots - 1]))
            {
                return lane;
            }
        }
    }
    return NULL;
}

// 0x00BCFA00 (:1666): walks advance_dist along the splines. False when the route ends first: the end point and what is left.
bool vehicle_ai_find_approx_advanced_position_along_route(vector& spline_pos, const lane_spline_base*& out_lane, float& out_u, float& dist_remaining, const lane_spline_base** target_splines, int num_splines, float start_u, float start_dist, float advance_dist, bool road_spline_path, float road_offset)
{
    const lane_spline_base* lane = *target_splines;
    float u = start_u;
    while (advance_dist > 0.0f)
    {
        num_splines--;
        target_splines++;
        if (start_dist + advance_dist <= lane->arc_length[0])
        {
            break;
        }
        if (num_splines < 1)
        {
            out_lane = lane;
            out_u = (float)(lane->num_knots - 1);
            dist_remaining = advance_dist - (lane->arc_length[0] - start_dist);
            spline_pos = lane->knots.to_pointer()[lane->num_knots - 1];
            return false;
        }
        advance_dist -= lane->arc_length[0] - start_dist;
        lane = *target_splines;
        u = 0.0f;
        start_dist = 0.0f;
    }
    const float max_u = (float)(lane->num_knots - 1);
    const cubic_bezier_spline cbs(lane->num_knots, lane->knots.to_pointer(), lane->in_handles.to_pointer(), lane->out_handles.to_pointer());
    // Steps of at most 2 m along the tangent.
    bool more;
    do
    {
        float step = advance_dist;
        more = step > 2.0f;
        if (more)
        {
            step = 2.0f;
        }
        vector tangent;
        cbs.get_tangent(tangent, u);
        const float speed = tangent.mag_and_normalize_safe();
        u += step / speed;
        if (u >= max_u)
        {
            u = max_u;
        }
        advance_dist -= step;
    } while (more);
    out_u = u;
    dist_remaining = 0.0f;
    out_lane = lane;
    cbs.get_point(spline_pos, u);
    return true;
}

// 0x00BC4220 (:1781): LTCG, vai in EDI.
static float get_accelerator_max_value(vehicle_ai* vai, float cur_speed)
{
    if (vai->stuck.stuck_clear_time.valid() && vai->stuck.stuck_clear_time.time_since() < 3000)
    {
        return 1.0f;
    }
    if (vai->get_state() == VS_AI_PATHFIND)
    {
        return 1.0f;
    }
    return fl_parameter(cur_speed, 5.0f, 10.0f, true) * 0.3f + 0.7f;
}

// 0x00BCD6F0 (:1822)
void vehicle_ai_presim_process()
{
    vehicle_navmesh_path_requests_process();
    intersection_controller_do_frame();
}

// 0x00BC42A0 (:1900)
void vehicle_ai_get_collecter_base(vehicle* vp, vector& box_bmin, vector& box_bmax)
{
    vector bmin, bmax;
    vp->get_local_bbox(bmin, bmax);
    bmax.y += 0.5f;
    if (bmax.y <= 1.2f)
    {
        bmax.y = 1.2f;
    }
    bmin.y -= 0.5f;
    if (bmin.y > -1.2f)
    {
        bmin.y = -1.2f;
    }
    box_bmin.y = bmin.y;
    box_bmin.z = -3.75f;
    box_bmin.x = bmin.x - 0.5f;
    box_bmax.x = bmax.x + 0.5f;
    box_bmax.y = bmax.y;
    box_bmax.z = 3.75f;
}

// 0x00BC4390 (:1919)
void vehicle_ai_get_collector_bbox(vehicle* vp, vector& box_bmin, vector& box_bmax, vector& box_pos, matrix& box_orient, bool forward)
{
    vector local_min, local_max;
    vp->get_local_bbox(local_min, local_max);
    vehicle_ai_get_collecter_base(vp, box_bmin, box_bmax);
    box_pos = vp->pos;
    const float direction = forward ? 1.0f : -1.0f;
    const float steer = automobile_get_vehicle_steering_input_raw(vp->get_handle());
    matrix steer_rotation;
    steer_rotation.make_ry(direction * 0.5f * vp->info->max_steering_angle * steer);
    box_orient = steer_rotation * vp->orient;
    vector offset = vp->orient.fvec * (((local_max.z - 0.3f) + 3.75f) * direction);
    offset.unrotate(steer_rotation);
    box_pos += offset;
}

// 0x00BC4650 (:1938)
static void vehicle_ai_repulsor_init(vehicle_ai* vai)
{
    vehicle_repulsor_info& info = vai->repulsor_info;
    info.affected_by_repulsors = 0;
    info.num_path_repulsors = 0;
    info.path_repulsors_collection_time.invalidate();
    for (int i = 0; i < 10; i++)
    {
        info.vpath_repulsor_handles[i] = 0xFFFFFFFF;
        info.vpath_repulsor_time[i].invalidate();
    }
}

// 0x00BC46E0 (:2105): LTCG, cb_spline in ECX, lookahead_info in ESI, cur_dist in EBX. True when past max_lookahead_dist.
static bool get_lookahead_curvature_info(vehicle_lookahead_info& lookahead_info, float& cur_dist, const cubic_bezier_spline& cb_spline, const lane_spline_base* base_spline, bool use_center)
{
    float us[3];
    float lengths[3];
    int num_samples;
    us[0] = 0.0f;
    if (use_center)
    {
        us[1] = (float)(base_spline->num_knots - 1) * 0.5f;
        us[2] = (float)(base_spline->num_knots - 1);
        lengths[0] = base_spline->arc_length[1];
        lengths[1] = base_spline->arc_length[0] - base_spline->arc_length[1];
        lengths[2] = 0.0f;
        num_samples = 3;
    }
    else
    {
        us[1] = (float)(base_spline->num_knots - 1);
        lengths[0] = base_spline->arc_length[0];
        lengths[1] = 0.0f;
        num_samples = 2;
    }
    for (int i = 0; i < num_samples; i++)
    {
        if (cur_dist > 0.0f)
        {
            const float curvature = get_spline_curvature(cb_spline, us[i]);
            if (curvature > 0.001f)
            {
                lookahead_info.entries[lookahead_info.num_lookahead_points].lookahead_curv = curvature;
                lookahead_info.entries[lookahead_info.num_lookahead_points].lookahead_dist = cur_dist;
                lookahead_info.num_lookahead_points++;
            }
        }
        cur_dist += lengths[i];
        if (cur_dist > lookahead_info.max_lookahead_dist)
        {
            return true;
        }
    }
    return false;
}

// 0x00BC4860 (:2162): LTCG, lookahead_info in EAX, vai in ECX.
static void get_lookahead_info(vehicle_ai* vai, vehicle_lookahead_info& lookahead_info, bool force_use_center)
{
    const vehicle_route_info& route = vai->route_info;
    if (get_front_bumper_route_segment(route) == NULL)
    {
        lookahead_info.num_lookahead_points = 0;
        return;
    }
    float cur_dist = route.bumper_info.distance_remaining - route.route[route.bumper_info.route_index].base_spline->arc_length[0];
    lookahead_info.num_lookahead_points = 0;
    for (int i = route.bumper_info.route_index; i < route.num_route_segments; i++)
    {
        if (get_lookahead_curvature_info(lookahead_info, cur_dist, route.route[i].cb_spline, route.route[i].base_spline, force_use_center))
        {
            return;
        }
    }
}

// 0x00BC4920 (:2199)
void get_cornering_target_speed_and_braking(const havok_vehicle_ai_info& info, vehicle_ai* vai, float& braking_decel, float& curve_speed)
{
    const float aggression = vai->vai_flags.is_courier ? 1.2f : (vai->vai_flags.is_riding_shotgun ? 1.0f : 0.8f);
    const float max_radial_accel = vai->active_control_info.max_radial_accel * aggression;
    const float speed_sq = info.speed * info.speed;
    float needed_decel = 0.0f;
    if (info.speed > 5.0f)
    {
        vehicle_lookahead_info lookahead;
        lookahead.max_lookahead_dist = speed_sq / (vai->active_control_info.max_braking_decel * aggression * 2.0f);
        get_lookahead_info(vai, lookahead, false);
        for (int i = 0; i < lookahead.num_lookahead_points; i++)
        {
            const float decel = (speed_sq - max_radial_accel / lookahead.entries[i].lookahead_curv) / (lookahead.entries[i].lookahead_dist * 2.0f + 0.001f);
            if (decel > needed_decel)
            {
                needed_decel = decel;
            }
        }
    }
    braking_decel = needed_decel;
    float speed = 1000.0f;
    const route_segment* bumper = get_front_bumper_route_segment(vai->route_info);
    if (bumper != NULL)
    {
        const float curvature = get_spline_curvature(bumper->cb_spline, vai->route_info.bumper_info.u);
        if (curvature > 0.001f)
        {
            speed = sqrtf(max_radial_accel / curvature);
        }
    }
    if (speed > 1000.0f)
    {
        speed = 1000.0f;
    }
    if (speed < 7.0f)
    {
        speed = 7.0f;
    }
    curve_speed = speed;
}

// 0x00BC91C0 (:2264)
void vehicle_ai_get_curvature_speed_decel(vehicle_ai* vai, float& desired_speed, float& desired_braking_decel)
{
    havok_vehicle_ai_info info;
    info.flags = 6; // pos and speed
    vehicle_get_ai_info(vai->vehicle_handle, info);
    float needed_decel;
    float curve_speed;
    get_cornering_target_speed_and_braking(info, vai, needed_decel, curve_speed);
    const float max_decel = vai->active_control_info.max_braking_decel;
    float decel = needed_decel > max_decel * 0.7f ? needed_decel : 0.0f;
    if (info.speed > curve_speed * 1.2f)
    {
        decel = (info.speed / curve_speed - 1.2f) * max_decel;
    }
    desired_speed = curve_speed;
    desired_braking_decel = decel;
}

// 0x00BCD700 (:2300)
void vehicle_ai_set_accel_for_spline_path(vehicle_ai* vai, const havok_vehicle_ai_info& info, float target_speed, float target_braking_decel)
{
    const unsigned int handle = vai->vehicle_handle;
    automobile_set_handbrake(handle, false);
    vector to_goal(vai->steer_info.goal_pos.x - info.pos.x, 0.0f, vai->steer_info.goal_pos.z - info.pos.z);
    to_goal.normalize_safe();
    float dot = info.orient.fvec.y * to_goal.y + info.orient.fvec.x * to_goal.x + info.orient.fvec.z * to_goal.z;
    if (dot < -1.0f)
    {
        dot = -1.0f;
    }
    if (dot >= 1.0f)
    {
        dot = 1.0f;
    }
    const float angle = fl_acos(dot);
    float turn_limit = info.steering_max_angle * (info.moving_in_reverse ? 1.9f : 2.1f);
    if (info.is_tank)
    {
        turn_limit = info.steering_max_angle * 0.25f;
    }
    vai->vai_flags.turning_in_place = 0;
    if (turn_limit * 1.3f < angle)
    {
        if (!info.is_tank)
        {
            automobile_set_brake_pedal_input(handle, 0.0f);
            automobile_set_forward_or_reverse_accelerator(handle, -get_accelerator_max_value(vai, info.speed));
        }
        else
        {
            if (info.speed <= 2.0f)
            {
                automobile_set_brake_pedal_input(handle, 0.0f);
                automobile_set_forward_or_reverse_accelerator(handle, get_accelerator_max_value(vai, info.speed));
            }
            else
            {
                automobile_set_brake_pedal_input(handle, 1.0f);
                automobile_set_forward_or_reverse_accelerator(handle, 0.0f);
            }
            const float side = info.orient.rvec.y * to_goal.y + info.orient.rvec.x * to_goal.x + info.orient.rvec.z * to_goal.z;
            automobile_set_steering(handle, side < 0.0f ? -1.0f : 1.0f);
            vai->vai_flags.turning_in_place = 1;
        }
    }
    else if (info.moving_in_reverse)
    {
        automobile_set_forward_or_reverse_accelerator(handle, 0.001f);
        automobile_set_brake_pedal_input(handle, 1.0f);
    }
    else if (target_braking_decel <= 0.0f)
    {
        vehicle_ai_set_accel_for_speed(vai, target_speed, info.speed, false);
        vai->steer_info.desired_vel = target_speed;
    }
    else
    {
        float brake = target_braking_decel / vai->active_control_info.max_braking_decel;
        if (brake >= 1.0f)
        {
            brake = 1.0f;
        }
        automobile_set_forward_or_reverse_accelerator(handle, 0.001f);
        automobile_set_brake_pedal_input(handle, brake);
    }
    vai->steer_info.accel_val = automobile_get_accelerator_value(handle);
    vai->steer_info.brake_val = automobile_get_brake_value(handle);
    vai->steer_info.desired_vel = target_speed;
    vai->steer_info.vel = info.speed;
}

// 0x00BC9190 (:2030) and 0x00BD4D00 (:2399), the second inlined into the third: the next nav mesh node, or a random lane.
static const lane_spline_base* base_choose_next_lane(vehicle_ai* vai, const lane_spline_base* cur_lane, bool& ambient_path, bool& navmesh_path)
{
    ambient_path = false;
    navmesh_path = false;
    if (vai->navmesh_path == NULL)
    {
        return vehicle_choose_next_lane_spline_random(vai, cur_lane, false, false);
    }
    navmesh_path = true;
    navmesh_lane_spline_node* node = vai->navmesh_path;
    vehicle_free_ls_node(&vai->navmesh_path, node);
    return &node->lane;
}

// 0x00BD4D50 (:2432): LTCG, navmesh_path in ECX, ambient_path in EDX.
static const lane_spline_base* vehicle_ai_choose_next_spline(vehicle_ai* vai, const lane_spline_base* cur_lane, bool& ambient_path, bool& navmesh_path, bool primary)
{
    ambient_path = false;
    navmesh_path = false;
    if (vai->get_state() == VS_AI_PATHFIND)
    {
        return base_choose_next_lane(vai, cur_lane, ambient_path, navmesh_path);
    }
    return NULL;
}

// 0x00BC4BA0 (:2501): a quarter circle ahead of the vehicle, at most 6 m along it.
bool process_u_turn_steering(vehicle_ai* vai, const havok_vehicle_ai_info& info)
{
    if (!vai->vai_flags.u_turn_active)
    {
        return false;
    }
    if (!vai->vai_flags.slowing_for_u_turn && vai->vai_flags.turning)
    {
        const float curvature = vai->active_control_info.max_steer_curvature;
        const float radius = 1.0f / curvature;
        float arc = radius * 1.5707964f; // a quarter of a circle
        if (6.0f - arc < 0.0f)
        {
            arc = 6.0f;
        }
        const float angle = curvature * arc;
        const float forward = sinf(angle) * radius;
        const float side = -((1.0f - cosf(angle)) * radius);
        vai->steer_info.goal_pos.x = info.pos.x + info.orient.rvec.x * side + info.orient.fvec.x * forward;
        vai->steer_info.goal_pos.y = info.pos.y + info.orient.rvec.y * side + info.orient.fvec.y * forward;
        vai->steer_info.goal_pos.z = info.pos.z + info.orient.rvec.z * side + info.orient.fvec.z * forward;
    }
    return true;
}

// 0x00BC4DA0 (:2564)
float get_steering_goal_dist(object_sub_type sub_type, float vehicle_speed)
{
    float min_dist = 6.0f;
    float max_dist = 15.0f;
    if (sub_type == OT_SUB_VEHICLE_FLYER)
    {
        min_dist = 15.0f;
        max_dist = 20.0f;
    }
    float dist = (vehicle_speed - 10.0f) * 0.67f + min_dist;
    if (dist < min_dist)
    {
        dist = min_dist;
    }
    return dist >= max_dist ? max_dist : dist;
}

// 0x00BCFC50 (:2580)
void get_steering_lateral_offset(const vehicle_ai* vai, const lane_spline_base* steering_spline, float steer_u, const vector& vehicle_pos, vector& steering_offset)
{
    steering_offset.zero();
    if (vehicle_ai_get_center_route_segment(vai->route_info) == NULL)
    {
        return;
    }
    const bool lateral = vai->vai_flags.m_lateral_offset;
    const bool road = vai->vai_flags.m_road_offset;
    if (!lateral && !road)
    {
        return;
    }
    const cubic_bezier_spline cbs(steering_spline->num_knots, steering_spline->knots.to_pointer(), steering_spline->in_handles.to_pointer(), steering_spline->out_handles.to_pointer());
    vector tangent;
    cbs.get_tangent(tangent, steer_u);
    vector right(tangent.z * UP_VECTOR.y - tangent.y * UP_VECTOR.z, UP_VECTOR.z * tangent.x - tangent.z * UP_VECTOR.x, tangent.y * UP_VECTOR.x - UP_VECTOR.y * tangent.x);
    right.normalize_safe();
    tangent.normalize_safe();
    if (lateral)
    {
        steering_offset += right * vai->steer_info.lateral_offset;
    }
    if (road)
    {
        steering_offset += right * vai->steer_info.road_offset;
    }
}

// 0x00BC4E30 (:2623)
void get_steering_vertical_offset(vector& steering_offset, const vehicle* vp, const vehicle_ai* vai)
{
    steering_offset = ZERO_VECTOR;
    if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
    {
        steering_offset.y += vai->flying_offset;
    }
}

// The flyer's constant-height mode: holds desired_flying_offset unless the ground under it has come up too close.
static bool flyer_stays_at_constant_height(const vehicle* vp)
{
    return static_cast<const flyer*>(vp)->flyer_flags.stay_at_constant_height != 0;
}

// 0x00BCFF70 (:2639)
bool process_vehicle_steering(vehicle* vp, vehicle_ai* vai, const havok_vehicle_ai_info& info)
{
    if (process_u_turn_steering(vai, info))
    {
        return true;
    }
    if (vai->vai_flags.need_to_rise)
    {
        // Straight up until the flyer is within 2 m of the height it asked for.
        const float height = vai->necessary_height;
        if (height - info.pos.y > 2.0f)
        {
            vai->steer_info.curvature_goal = 0.0f;
            vai->steer_info.goal_pos = info.pos;
            vai->steer_info.goal_pos.y = height;
            return true;
        }
        vai->vai_flags.need_to_rise = 0;
    }
    const bool is_flyer = vp->get_sub_type() == OT_SUB_VEHICLE_FLYER;
    const float goal_dist = get_steering_goal_dist(vp->get_sub_type(), info.speed);
    vai->steer_info.lookahead_dist = goal_dist;
    const route_segment* segment;
    int num_segments;
    vehicle_ai_get_route_segment_info(vai->route_info, segment, num_segments);
    if (segment == NULL)
    {
        vai->steer_info.goal_pos = info.pos + info.orient.fvec * goal_dist;
        vai->steer_info.curvature_goal = 0.0f;
        return false;
    }
    const lane_spline_base* splines[5];
    for (int i = 0; i < num_segments; i++)
    {
        splines[i] = segment[i].base_spline;
    }
    vai->vai_flags.at_end_of_route = 0;
    vector spline_pos;
    const lane_spline_base* steer_lane;
    float steer_u;
    float dist_left;
    const bool found = vehicle_ai_find_approx_advanced_position_along_route(spline_pos, steer_lane, steer_u, dist_left, splines, num_segments, vai->route_info.center_info.u, segment->length - vai->route_info.center_info.distance_remaining, goal_dist, vai->current_path_info.flags.road_spline_path, vai->steer_info.road_offset);
    if (is_flyer)
    {
        vai->flying_offset = vai->desired_flying_offset + vai->building_offset + vai->target_offset + vai->terrain_offset;
    }
    float terrain_y = 0.0f;
    if (found)
    {
        havok_terrain_height(terrain_y, vai->steer_info.goal_pos.x, vai->steer_info.goal_pos.z, false, NULL);
        if (is_flyer)
        {
            // Original quirk kept: the terrain height of last frame's goal becomes this frame's goal height, and the
            // spline's own height is replaced below when the velocity test passes.
            vai->steer_info.goal_pos.y = terrain_y;
            vector vel;
            vehicle_get_velocity(vp, vel);
            if (vel.mag_and_normalize_safe() > 0.05f)
            {
                matrix vel_orient;
                vel_orient.make_quick(vel);
                vector to_goal = vai->steer_info.goal_pos - vp->pos;
                to_goal.flatten_mag_and_normalize_safe();
                matrix goal_orient;
                goal_orient.make_quick(to_goal);
                const vector pos = vp->pos;
                vector bmin, bmax;
                vp->get_local_bbox(bmin, bmax);
                bbox_grow(bmin, bmax, vector(2.5f, 5.0f, 20.0f));
                bbox_grow(bmin, bmax, vector(-2.5f, -5.0f, 0.0f));
                float box_height;
                if (havok_terrain_height(box_height, bmin, bmax, pos, vel_orient, true) && terrain_y < box_height)
                {
                    terrain_y = box_height;
                }
                if (havok_terrain_height(box_height, bmin, bmax, pos, goal_orient, true) && terrain_y < box_height)
                {
                    terrain_y = box_height;
                }
                if (terrain_y > 1000.0f)
                {
                    terrain_y = vai->steer_info.goal_pos.y;
                }
                if (flyer_stays_at_constant_height(vp))
                {
                    spline_pos.y = terrain_y;
                }
                else if (vai->flying_offset + spline_pos.y < terrain_y + TERRAIN_CLEARANCE_MIN)
                {
                    const float new_terrain_offset = (terrain_y + TERRAIN_CLEARANCE) - ((vai->flying_offset - vai->terrain_offset) + spline_pos.y);
                    vai->terrain_offset = new_terrain_offset;
                    vai->flying_offset += new_terrain_offset;
                }
            }
        }
        vector lateral;
        get_steering_lateral_offset(vai, steer_lane, steer_u, info.pos, lateral);
        vector vertical;
        get_steering_vertical_offset(vertical, vp, vai);
        vai->steer_info.goal_pos = lateral + spline_pos + vertical;
        if (is_flyer)
        {
            if (flyer_stays_at_constant_height(vp))
            {
                const float clearance = vai->building_offset + terrain_y;
                if (clearance + 5.0f <= vai->desired_flying_offset)
                {
                    vai->steer_info.goal_pos.y = vai->desired_flying_offset;
                }
                else
                {
                    vai->desired_flying_offset = clearance + 10.0f;
                    vai->steer_info.goal_pos.y = clearance + 10.0f;
                }
            }
            const float climb = vai->steer_info.goal_pos.y - info.pos.y;
            float speed;
            vehicle_get_speed(vp, speed);
            if (!vehicle_is_bomber(vp) && (climb > 20.0f || (speed > 10.0f && climb > 10.0f)))
            {
                vai->vai_flags.need_to_rise = 1;
                vai->necessary_height = vai->steer_info.goal_pos.y;
            }
        }
        const cubic_bezier_spline cbs(steer_lane->num_knots, steer_lane->knots.to_pointer(), steer_lane->in_handles.to_pointer(), steer_lane->out_handles.to_pointer());
        vai->steer_info.curvature_goal = get_spline_curvature(cbs, steer_u);
        return true;
    }
    vai->vai_flags.at_end_of_route = 1;
    if (goal_dist - dist_left > 6.0f || is_flyer)
    {
        const lane_spline_base* last = segment->base_spline;
        vai->steer_info.goal_pos = last->knots.to_pointer()[segment->num_knots_minus_one];
    }
    else
    {
        vai->steer_info.goal_pos = spline_pos;
    }
    vector lateral;
    get_steering_lateral_offset(vai, steer_lane, steer_u, info.pos, lateral);
    vector vertical;
    get_steering_vertical_offset(vertical, vp, vai);
    vai->steer_info.goal_pos += lateral + vertical;
    if (is_flyer)
    {
        if (flyer_stays_at_constant_height(vp))
        {
            float box_height = 0.0f;
            havok_terrain_height(terrain_y, vai->steer_info.goal_pos.x, vai->steer_info.goal_pos.z, false, NULL);
            vector to_goal = vai->steer_info.goal_pos - vp->pos;
            to_goal.flatten_mag_and_normalize_safe();
            matrix goal_orient;
            goal_orient.make_quick(to_goal);
            vector bmin, bmax;
            vp->get_local_bbox(bmin, bmax);
            bbox_grow(bmin, bmax, vector(-2.5f, -5.0f, 0.0f));
            bbox_grow(bmin, bmax, vector(2.5f, 5.0f, 20.0f));
            if (havok_terrain_height(box_height, bmin, bmax, vp->pos, goal_orient, true) && terrain_y < box_height)
            {
                terrain_y = box_height;
            }
            if (vai->building_offset + terrain_y + 5.0f <= vai->desired_flying_offset)
            {
                vai->steer_info.goal_pos.y = vai->desired_flying_offset;
            }
            else
            {
                // Original quirk kept: uses the box height, which stays 0 when the box test hits nothing.
                const float new_offset = vai->building_offset + box_height + 10.0f;
                vai->desired_flying_offset = new_offset;
                vai->steer_info.goal_pos.y = new_offset;
            }
        }
        if (!vehicle_is_bomber(vp) && vai->steer_info.goal_pos.y - info.pos.y > 20.0f)
        {
            vai->vai_flags.need_to_rise = 1;
            vai->necessary_height = vai->steer_info.goal_pos.y;
        }
    }
    vai->steer_info.curvature_goal = 0.0f;
    return false;
}

// 0x00BC9290 (:2968): LTCG, vp in ESI.
static void check_for_dead_driver(vehicle* vp)
{
    human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
    if (driver != NULL && !driver->is_killed())
    {
        return;
    }
    vehicle_free_ls_path(&vp->vai.navmesh_path);
    vehicle_ai_clear_path_info(&vp->vai);
    vehicle_ai_flush_route(vp->vai.route_info);
    vehicle_set_controls_when_abandoned(vp);
    if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER && vehicle_engine_is_running(vp))
    {
        vp->engine_stop(false, false);
    }
}

// 0x00BC9320 (:3009)
static void vehicle_process_tire_grip(vehicle* vp)
{
    vp->vai.active_control_info.max_braking_decel = vp->vai.base_control_info.max_braking_decel;
    vp->vai.active_control_info.max_radial_accel = vp->vai.base_control_info.max_radial_accel;
}

// Drops the extra mass a stuck vehicle was given.
static void remove_stuck_mass(vehicle* vp)
{
    const float mass = vp->get_extra_mass_value() - vp->vai.stuck.stuck_mass;
    vp->set_extra_mass_value(mass);
    if (mass <= 1.0f)
    {
        vp->set_extra_mass(false);
    }
    vp->vai.stuck.stuck_mass = 0.0f;
    vp->vai.stuck.stuck_mass_timer.invalidate();
}

// 0x00BC9340 (:3033): LTCG, vp in ESI.
static void vehicle_ai_check_extra_mass(vehicle* vp)
{
    if (vp->vai.stuck.stuck_mass <= 0.0f)
    {
        return;
    }
    object_path_road* road = World.object_from_handle<object_path_road>(vp->vai.current_path_info.closest_road_spline);
    if (road != NULL)
    {
        vector closest = ZERO_VECTOR;
        road->find_closest_point_on_road(vp->pos, closest, false);
        unsigned int navpoint;
        if (vec_xz_dist(closest, vp->pos) < 5.0f && !is_vehicle_on_bridge(vp, navpoint))
        {
            return;
        }
    }
    remove_stuck_mass(vp);
    vp->vai.stuck.clear_debris_state = CDS_DONT;
    vp->vai.stuck.clear_debris_timer.invalidate();
}

// 0x00BC9480 (:3075)
void vehicle_collect_objects_fast(const vector& bmin, const vector& bmax, const vector& bpos, const matrix& borient, const unsigned int collect_type, int& num_objects, int max_objects, unsigned int* object_handles, bool (*&cb)(object*, unsigned int, const void*), vehicle* vp)
{
    vector world_min = bmin + bpos;
    vector world_max = bmax + bpos;
    vector aabb_min, aabb_max;
    bbox_transform(aabb_min, aabb_max, borient, bpos, world_min, world_max);
    unsigned int found[32];
    const int num_found = havok_collect_objects_fast(aabb_min, aabb_max, found, 32, collect_type, cb, vp, NULL);
    num_objects = 0;
    if (num_found <= 0)
    {
        return;
    }
    vector extents, center;
    bbox_extract_obb_info(bmin, bmax, bpos, borient, extents, center);
    for (int i = 0; i < num_found; i++)
    {
        if (num_objects >= max_objects)
        {
            return;
        }
        object* repulsor = World.object_from_handle(found[i]);
        repulsor_oriented_bbox obb;
        if (repulsor != NULL && bb_pf_get_repulsor_oriented_bounding_box(&obb, repulsor))
        {
            const vector half_size = obb.size * 0.5f;
            if (ix_oriented_box_box(half_size, obb.pos, obb.orient, extents, center, borient))
            {
                object_handles[num_objects++] = found[i];
            }
        }
    }
}

// 0x00BD0BD0 (:3174): LTCG, vp in ESI.
static void vehicle_ai_process_u_turn(vehicle* vp)
{
    vehicle_ai& vai = vp->vai;
    if (!vai.vai_flags.u_turn_active || vai.route_info.num_route_segments == 0)
    {
        return;
    }
    vector closest, tangent;
    spline_common_get_closest_point_and_tangent_on_spline(vai.route_info.route[vai.route_info.center_info.route_index].base_spline, vp->pos, closest, tangent);
    tangent.normalize_safe();
    if (vp->orient.fvec.y * tangent.y + vp->orient.fvec.x * tangent.x + vp->orient.fvec.z * tangent.z > 0.6f)
    {
        vai.vai_flags.u_turn_active = 0;
        return;
    }
    if (!vai.current_path_info.flags.road_spline_path || !(vai.vai_flags.turning || vai.vai_flags.backing_up))
    {
        return;
    }
    object_path_road* road = World.object_from_handle<object_path_road>(vai.current_path_info.closest_road_spline);
    if (road == NULL)
    {
        return;
    }
    vector bmin, bmax;
    if (!havok_body_get_aabb(vp->havok_handle, ZERO_VECTOR, IDENTITY_MATRIX, bmin, bmax))
    {
        vp->get_local_bbox(bmin, bmax);
    }
    const vector front = vp->pos + vp->orient.fvec * bmax.z;
    const vector back = vp->pos + vp->orient.fvec * bmin.z;
    vector on_road;
    const float front_width = road->get_width(road->find_closest_point_on_road(front, on_road, false));
    const float front_off_road = vec_xz_dist(front, on_road) - front_width;
    const float back_width = road->get_width(road->find_closest_point_on_road(back, on_road, false));
    const float back_off_road = vec_xz_dist(back, on_road) - back_width;
    if (vai.vai_flags.turning)
    {
        if (front_off_road > 0.0f && (back_off_road <= 0.0f || back_off_road < front_off_road))
        {
            vai.vai_flags.backing_up = 1;
            vai.vai_flags.turning = 0;
        }
    }
    else if (back_off_road > 0.0f && (front_off_road <= 0.0f || front_off_road < back_off_road))
    {
        vai.vai_flags.backing_up = 0;
        vai.vai_flags.turning = 1;
    }
}

// 0x00BC4E80 (:3264)
bool vehicle_in_water(const vehicle* vp)
{
    return false;
}

// 0x00BD0F40 (:3274)
void vehicle_ai_do_frame(vehicle* vp)
{
    vehicle_ai_process_u_turn(vp);
    check_for_dead_driver(vp);
    vehicle_process_tire_grip(vp);
    vehicle_ai_check_extra_mass(vp);
    intersection_controller_process_vehicle(vp);
}

// 0x00BCDB00 (:3330)
void vehicle_ai_init(vehicle* vp)
{
    vehicle_ai* vai = &vp->vai;
    memset(vai, 0, sizeof(vehicle_ai));
    vai->vehicle_handle = vp->get_handle();
    vai->active_control_info.max_braking_decel = vp->info->m_ai_max_braking_decel;
    vai->active_control_info.max_radial_accel = vp->info->m_ai_max_radial_accel;
    vai->base_control_info.max_braking_decel = vp->info->m_ai_max_braking_decel;
    vai->base_control_info.max_radial_accel = vp->info->m_ai_max_radial_accel;
    vehicle_ai_repulsor_init(vai);
    vai->navmesh_path_update.set(0);
    vai->stuck.stuck_active_time.invalidate();
    vai->stuck.stuck_threshold_time.invalidate();
    vai->stuck.extended_stuck_time.invalidate();
    vai->stuck.stuck_clear_time.invalidate();
    vai->stuck.stuck_mass_timer.invalidate();
    vai->stuck.stuck_mass = 0.0f;
    vai->stuck.clear_debris_state = CDS_DONT;
    vai->stuck.clear_debris_timer.invalidate();
    vai->swerve_timer.invalidate();
    vai->initial_swerve_timer.invalidate();
    vai->checkpoint_timer.invalidate();
    vai->ambient_wait_timer.invalidate();
    vai->ambient_wait_object = 0xFFFFFFFF;
    vai->change_sides_timer.invalidate();
    vai->pull_off_timer.invalidate();
    vai->set_state(VS_AI_IDLE);
    vai->u_turn_global_delay.set(500);
    vehicle_ai_clear_path_info(vai);
    vai->checkpoint_handle = 0xFFFFFFFF;
    vai->update_failures = 0;
    vai->desired_flying_offset = 20.0f;
    vai->target_offset = 0.0f;
    vai->building_offset = 0.0f;
    vai->flying_offset = 20.0f;
    vai->terrain_offset = 0.0f;
    vai->necessary_height = 0.0f;
    vai->active_intersection_controller = 0xFFFF;
    // pf_failure_history's num and offset were zeroed by the memset, as rfg.exe sets them here.
}

// 0x00BC4E90 (:3521)
void vehicle_ai_debug_render()
{
}

// 0x00BC4EA0 (:3929)
float vehicle_ai_get_curvature_to_point(const vector& start_pos, const matrix& orient, const vector& goal_pos)
{
    const vector delta = goal_pos - start_pos;
    vector forward(orient.fvec.x, 0.0f, orient.fvec.z);
    forward.normalize_safe();
    vector right(orient.rvec.x, 0.0f, orient.rvec.z);
    right.normalize_safe();
    const float along = delta.x * forward.x + delta.y * forward.y + forward.z * delta.z;
    const float side = right.x * delta.x + delta.y * right.y + right.z * delta.z;
    return (side * 2.0f) / (side * side + along * along);
}

// 0x00BC9700 (:3954)
void vehicle_ai_steer_toward_goal(vehicle_ai* vai, const havok_vehicle_ai_info& info)
{
    vector from = info.pos;
    vehicle* vp = World.object_from_handle<vehicle>(vai->vehicle_handle);
    if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
    {
        from = static_cast<flyer*>(vp)->chassis_com_ws;
    }
    if (vai->vai_flags.u_turn_active)
    {
        if (vai->vai_flags.backing_up)
        {
            automobile_set_steering(vai->vehicle_handle, 1.0f);
            return;
        }
        if (vai->vai_flags.turning)
        {
            automobile_set_steering(vai->vehicle_handle, -1.0f);
            return;
        }
    }
    const vector delta = vai->steer_info.goal_pos - from;
    float steer_angle;
    if (delta.x * delta.x + delta.y * delta.y + delta.z * delta.z <= 225.0f) // within 15 m
    {
        vector forward(info.orient.fvec.x, 0.0f, info.orient.fvec.z);
        forward.normalize_safe();
        vector right(info.orient.rvec.x, 0.0f, info.orient.rvec.z);
        right.normalize_safe();
        const float along = delta.x * forward.x + delta.y * forward.y + forward.z * delta.z;
        const float side = right.x * delta.x + delta.y * right.y + right.z * delta.z;
        const float inv_radius = (side * 2.0f) / (along * along + side * side);
        // asin(wheelbase / radius), clamped to asin's range.
        float sine = vai->steer_info.wheelbase * inv_radius;
        if (sine < -1.0f)
        {
            sine = -1.0f;
        }
        if (sine >= 1.0f)
        {
            sine = 1.0f;
        }
        steer_angle = asinf(sine);
        vai->steer_info.inv_radius_steer = inv_radius;
    }
    else
    {
        steer_angle = fl_atan2_safe(info.orient.rvec.y * delta.y + delta.x * info.orient.rvec.x + info.orient.rvec.z * delta.z, info.orient.fvec.y * delta.y + info.orient.fvec.x * delta.x + info.orient.fvec.z * delta.z);
        vai->steer_info.inv_radius_steer = steer_angle / vai->steer_info.wheelbase;
    }
    float steer = steer_angle / info.steering_max_angle;
    if (steer < -1.0f)
    {
        steer = -1.0f;
    }
    if (steer >= 1.0f)
    {
        steer = 1.0f;
    }
    if (info.moving_in_reverse)
    {
        steer = -steer;
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO)
    {
        const float current = automobile_get_steering_t(vai->vehicle_handle);
        const float max_step = info.steering_max_angle * Frametime * 4.0f;
        float step = steer - current;
        if (step < -max_step)
        {
            step = -max_step;
        }
        if (step >= max_step)
        {
            step = max_step;
        }
        float result = step + current;
        // The steering goes through the middle when it changes sides.
        if ((steer < 0.0f) != (result < 0.0f))
        {
            result = 0.0f;
        }
        automobile_set_steering(vai->vehicle_handle, result);
    }
    else if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
    {
        static_cast<flyer*>(vp)->set_request_flyto_point(vai->steer_info.goal_pos, vai->vai_flags.at_end_of_route);
    }
}

// 0x00BCDC80 (:4041)
void vehicle_ai_accel_toward_goal(vehicle_ai* vai, const havok_vehicle_ai_info& self_info)
{
    const unsigned int handle = vai->vehicle_handle;
    automobile_set_handbrake(handle, false);
    float decel = vai->accel_info.desired_braking_decel;
    if (decel <= 5.0f)
    {
        automobile_set_brake_pedal_input(handle, 0.0f);
        vehicle_ai_set_accel_for_speed(vai, vai->accel_info.desired_speed, self_info.speed, false);
    }
    else
    {
        if (decel >= 30.0f)
        {
            decel = 30.0f;
        }
        automobile_set_forward_or_reverse_accelerator(handle, 0.001f);
        automobile_set_brake_pedal_input(handle, decel / 30.0f);
    }
    vai->steer_info.accel_val = automobile_get_accelerator_value(handle);
    vai->steer_info.brake_val = automobile_get_brake_value(handle);
    vai->steer_info.vel = self_info.speed;
}

// 0x00BCDD90 (:4076)
void vehicle_ai_set_state(vehicle_ai* vai, vehicle_ai_state state)
{
    vai->set_state(state);
}

// 0x00BC9CA0 (:4086)
void vehicle_ai::set_state(vehicle_ai_state _state)
{
    if (_state != ai_state)
    {
        Vai_state_callbacks[ai_state].exit(this);
    }
    const vehicle_ai_state old_state = ai_state;
    vai_flags.m_override_stop = 0;
    vai_flags.m_lateral_offset = 0;
    ai_state = _state;
    if (ai_last_state != old_state)
    {
        ai_last_state = old_state;
    }
}

// 0x00BCDDC0 (:4178)
void vehicle_ai_free_resources(vehicle_ai* vai)
{
    vehicle_free_ls_path(&vai->navmesh_path);
    vehicle* vp = World.object_from_handle<vehicle>(vai->vehicle_handle);
    if (vp != NULL)
    {
        if (vp->vai.active_intersection_controller != 0xFFFF)
        {
            intersection_controller_handle_vehicle_destroyed(vp);
        }
        vp->vai.active_intersection_controller = 0xFFFF;
    }
}

// 0x00BCDE20 (:4199): LTCG, leadoff in EAX. Which of the target lane and its two neighbours really leads to leadoff.
static const lane_spline_base* find_true_leadoff_spline(const lane_spline_base* target, const lane_spline_base* leadoff)
{
    const lane_spline* leadoff_city = get_city_spline_from_base_spline(leadoff);
    if (leadoff_city == NULL)
    {
        return NULL;
    }
    const lane_spline* target_city = get_city_spline_from_base_spline(target);
    if (target_city == NULL)
    {
        return NULL;
    }
    const lane_spline* lanes[3] = { target_city, target_city->right.to_pointer(), target_city->left.to_pointer() };
    for (int i = 0; i < 3; i++)
    {
        if (lanes[i] == NULL)
        {
            continue;
        }
        for (int j = 0; j < lanes[i]->num_next_splines; j++)
        {
            if (next_spline_relative(lanes[i], j) == leadoff_city)
            {
                return lanes[i];
            }
        }
    }
    return NULL;
}

// 0x00BC5030 (:4239): LTCG, route_index in EAX, route_info in ECX, pos in EDI.
static bool check_closest_route_index(const vehicle_route_info& route_info, const vector& pos, int route_index, float& u, bool flying)
{
    if (route_index < 0 || route_index >= route_info.num_route_segments)
    {
        return false;
    }
    const route_segment& segment = route_info.route[route_index];
    if (vec_dist_squared(segment.cb_spline.knots[0], segment.cb_spline.knots[1]) < 1e-10)
    {
        return false;
    }
    u = segment.cb_spline.get_closest_point_approx(pos);
    if (u <= 0.0f || (float)segment.num_knots_minus_one <= u)
    {
        return false;
    }
    vector point;
    segment.cb_spline.get_point(point, u);
    vector delta = point - pos;
    if (flying)
    {
        delta.y = 0.0f;
    }
    return delta.y * delta.y + delta.x * delta.x + delta.z * delta.z < ROUTE_INDEX_MAX_DIST_SQ;
}

// 0x00BC9CD0 (:4272): LTCG, vai in EAX, straight_ahead_pos in ESI.
static void find_straight_ahead_pos(vehicle_ai* vai, float advance_length, vector& straight_ahead_pos)
{
    const vehicle* vp = World.object_from_handle<vehicle>(vai->vehicle_handle);
    straight_ahead_pos = vp->pos + vp->orient.fvec * advance_length;
}

// 0x00BD4DB0 (:4312)
bool vehicle_ai_process_route(vehicle_ai* vai, vehicle_route_info& route_info, const havok_vehicle_ai_info& info, bool primary, bool flying)
{
    const vector front = info.pos + info.orient.fvec * info.bbox_max.z;
    const route_segment* center = vehicle_ai_get_center_route_segment(route_info);
    if (center == NULL)
    {
        return false;
    }
    const float u = center->cb_spline.get_closest_point_approx(info.pos);
    const float remaining = center->cb_spline.get_length_approx(10, u, (float)center->num_knots_minus_one);
    if (remaining < 5.0f && route_info.last_next_spline != center->base_spline)
    {
        route_info.last_next_spline = center->base_spline;
        if (primary && route_info.route[route_info.center_info.route_index].flags.terminal)
        {
            return true;
        }
        route_segment& last_segment = route_info.route[route_info.num_route_segments - 1];
        const lane_spline_base* cur_lane = last_segment.base_spline;
        bool ambient_path;
        bool navmesh_path;
        const lane_spline_base* next = vehicle_ai_choose_next_spline(vai, cur_lane, ambient_path, navmesh_path, primary);
        if (next == NULL)
        {
            last_segment.flags.terminal = 1;
        }
        else
        {
            const lane_spline_base* leadoff = find_true_leadoff_spline(cur_lane, next);
            if (leadoff != NULL && leadoff != cur_lane)
            {
                last_segment.base_spline = const_cast<lane_spline_base*>(leadoff);
                if (route_info.last_next_spline == cur_lane)
                {
                    route_info.last_next_spline = leadoff;
                }
            }
            vehicle_ai_add_route_segment(route_info, next, ambient_path, navmesh_path, false);
        }
    }
    const int center_index = route_info.center_info.route_index;
    const int search_order[4] = { center_index, center_index + 1, center_index - 1, center_index + 2 };
    vehicle_ai_update_route_segment_info(route_info, search_order, 4, route_info.center_info, info.pos, vai->vai_flags.m_road_offset, vai->steer_info.road_offset, flying);
    vehicle_ai_update_route_segment_info(route_info, search_order, 4, route_info.bumper_info, front, vai->vai_flags.m_road_offset, vai->steer_info.road_offset, flying);
    return true;
}

// 0x00BC5170 (:4444)
float vehicle_ai_get_dist_remaining_on_route_segment(const route_segment& segment, const vector& pos)
{
    const float u = segment.cb_spline.get_closest_point_approx(pos);
    return segment.cb_spline.get_length_approx(10, u, (float)segment.num_knots_minus_one);
}

// 0x00BC51B0 (:4463)
float vehicle_ai_get_dist_remaining_on_route(const vehicle_route_info& route_info)
{
    float dist = route_info.center_info.distance_remaining;
    for (int i = route_info.center_info.route_index + 1; i < route_info.num_route_segments; i++)
    {
        dist += route_info.route[i].length;
    }
    return dist;
}

// 0x00BC52A0 (:4479)
bool vehicle_ai_is_at_end_of_line(vehicle_ai* vai)
{
    human* driver = vehicle_passenger_in_seat(vai->vehicle_handle, VSI_DRIVER);
    if (driver == NULL || object_is_player(driver->get_handle()) || vai->get_state() != VS_AI_IDLE)
    {
        return false;
    }
    const route_segment* segment = vehicle_ai_get_center_route_segment(vai->route_info);
    return segment == NULL || segment->flags.terminal;
}

// 0x00BC5300 (:4498): the highest listed segment the point is on, or else the first valid one.
void vehicle_ai_update_route_segment_info(vehicle_route_info& route_info, const int* segment_search_order, int num_segments, vehicle_point_info& info, const vector& pos, bool use_road_offset, float road_offset, bool flying)
{
    // Indexed by route index, so search index -1 lands before the array: rfg.exe has the same layout and the same value
    // is written and read back. Original bug kept in spirit: the slot is only written for an index below 5 here.
    float us_storage[6] = {};
    float* us = us_storage + 1;
    int best = -1;
    for (int i = 0; i < num_segments; i++)
    {
        const int index = segment_search_order[i];
        if (index < route_info.num_route_segments && check_closest_route_index(route_info, pos, index, us[index], flying) && best < index)
        {
            best = index;
        }
    }
    if (best == -1)
    {
        for (int i = 0; i < num_segments; i++)
        {
            const int index = segment_search_order[i];
            if (index >= 0 && index < route_info.num_route_segments)
            {
                best = index;
                break;
            }
        }
    }
    info.route_index = best;
    info.u = us[best];
    const route_segment& segment = route_info.route[best];
    const float u = segment.cb_spline.get_closest_point_approx(pos);
    info.distance_remaining = segment.cb_spline.get_length_approx(10, u, (float)segment.num_knots_minus_one);
}

// 0x00BC5410 (:4664)
void vehicle_ai_track_player()
{
}

// 0x00BC9DA0 (:4735)
void vehicle_ai_set_accel_for_speed(vehicle_ai* vai, float desired_speed, float cur_speed, bool no_limit)
{
    float input;
    if (desired_speed < 1.0f)
    {
        input = 0.001f;
    }
    else
    {
        float gain = (desired_speed / 5.0f) * (desired_speed / 5.0f);
        if (gain < 2.0f)
        {
            gain = 2.0f;
        }
        float speed = desired_speed;
        if (speed < 1.0f)
        {
            speed = 1.0f;
        }
        if (speed >= 100.0f)
        {
            speed = 100.0f;
        }
        input = (1.0f - (cur_speed * cur_speed) / (speed * speed)) * gain;
        const float max_input = no_limit ? 1.0f : get_accelerator_max_value(vai, cur_speed);
        if (input < 0.0f)
        {
            if (input < -max_input)
            {
                input = -max_input;
            }
            if (input >= -0.001f)
            {
                input = -0.001f;
            }
        }
        else
        {
            if (speed < cur_speed * 1.7f)
            {
                // Light vehicles near their speed ease off.
                const vehicle* vp = World.object_from_handle<vehicle>(vai->vehicle_handle);
                if (vp != NULL)
                {
                    const float mass = vp->info->chassis_mass;
                    if (mass > 0.0f && mass < 5000.0f)
                    {
                        input = (mass / 10000.0f) * input;
                    }
                }
            }
            if (input < 0.001f)
            {
                input = 0.001f;
            }
            if (input >= max_input)
            {
                input = max_input;
            }
        }
    }
    automobile_set_forward_or_reverse_accelerator(vai->vehicle_handle, input);
}

// 0x00BCDEC0 (:4775)
bool vehicle_ai_does_lane_exist_at_position(const vector& pos)
{
    return lane_pathfind_get_closest_lane(pos, CLOSEST_LANE_RADIUS, LANE_INCLUDE_TRAFFIC, LANE_EXCLUDE_TRAFFIC) != NULL;
}

static float lane_path_length(lane_path_node* path)
{
    float length = 0.0f;
    lane_path_node* node = path;
    while (node != NULL)
    {
        length += node->lane->arc_length[0];
        node = node->next;
        if (node == path)
        {
            break;
        }
    }
    return length;
}

// 0x00BD5050 (:4792)
lane_path_node* vehicle_create_ambient_path_from_points(vehicle_ambient_pathfind_info& pathfind_info)
{
    static const int MAX_POINTS = 40;
    if (pathfind_info.num_points < 2 || pathfind_info.num_points > MAX_POINTS)
    {
        return NULL;
    }
    const lane_spline* lanes[MAX_POINTS];
    const lane_spline* last_lane = NULL;
    pathfind_info.uses_uturn = false;
    for (int i = 0; i < pathfind_info.num_points; i++)
    {
        if (i == 0 && pathfind_info.force_start_lane != NULL)
        {
            lanes[0] = pathfind_info.force_start_lane;
        }
        else
        {
            last_lane = lane_pathfind_get_closest_lane(pathfind_info.points[i], CLOSEST_LANE_RADIUS, LANE_INCLUDE_TRAFFIC, LANE_EXCLUDE_TRAFFIC);
            if (last_lane == NULL)
            {
                return NULL;
            }
            lanes[i] = last_lane;
        }
    }
    const lane_spline* uturn_lane = NULL;
    pathfind_info.start_lane = lanes[0];
    if (pathfind_info.allow_uturn)
    {
        if (lanes[0]->left.to_pointer() != NULL && (lanes[0]->spline_flags & 1) != 0)
        {
            uturn_lane = lanes[0]->left.to_pointer();
            pathfind_info.uturn_left = true;
        }
        if (lanes[0]->right.to_pointer() != NULL && (lanes[0]->spline_flags & 2) != 0)
        {
            uturn_lane = lanes[0]->right.to_pointer();
            pathfind_info.uturn_left = false;
        }
    }
    lane_path_node* paths[2] = { NULL, NULL };
    const lane_spline* starts[2] = { lanes[0], uturn_lane };
    for (int p = 0; p < 2; p++)
    {
        const lane_spline* cur = starts[p];
        if (cur == NULL)
        {
            continue;
        }
        for (int i = 1; i < pathfind_info.num_points; i++)
        {
            last_lane = lanes[i];
            if (cur == last_lane)
            {
                continue;
            }
            lane_path_info info(cur, last_lane, LANE_INCLUDE_TRAFFIC, 0x100);
            lane_path_node* part = lane_pathfind_get_path(info);
            if (part == NULL)
            {
                vehicle_free_lane_path(&paths[0]);
                vehicle_free_lane_path(&paths[1]);
                return NULL;
            }
            cur = last_lane;
            if (paths[p] == NULL)
            {
                paths[p] = part;
                continue;
            }
            free_lane_path_node(&paths[p], paths[p]->prev);
            lane_path_node* head = paths[p];
            if (head == NULL)
            {
                paths[p] = part;
            }
            else
            {
                head->prev->next = part;
                part->prev->next = head;
                lane_path_node* part_last = part->prev;
                part->prev = head->prev;
                head->prev = part_last;
            }
        }
        if (starts[p] == last_lane && paths[p] == NULL)
        {
            lane_pathnode_append_lane(&paths[p], last_lane);
        }
    }
    if (!pathfind_info.allow_uturn || paths[1] == NULL)
    {
        return paths[0];
    }
    if (paths[0] == NULL)
    {
        pathfind_info.uses_uturn = true;
        return paths[1];
    }
    // A U-turn only when it saves more than 100 m.
    if (lane_path_length(paths[1]) + 100.0f < lane_path_length(paths[0]))
    {
        pathfind_info.uses_uturn = true;
        vehicle_free_lane_path(&paths[0]);
        return paths[1];
    }
    vehicle_free_lane_path(&paths[1]);
    return paths[0];
}

// 0x00BD6880 (:4965)
void vehicle_ai_system_init()
{
    vehicle_navmesh_lane_spline_system_init();
}

// 0x00BC9FF0 (:4993)
void vehicle_ai_level_init_pre()
{
    vehicle_navmesh_path_requests_level_init_pre();
    lane_path_level_init_pre();
    vehicle_navmesh_lane_spline_level_init_pre();
    intersection_controller_level_init();
}

// 0x00BD0F80 (:5023)
void vehicle_ai_get_advanced_spline_pos(vehicle_ai* vai, float advance_length, vector& closest_spline_pos)
{
    const route_segment* segment = vehicle_ai_get_center_route_segment(vai->route_info);
    if (segment == NULL)
    {
        find_straight_ahead_pos(vai, advance_length, closest_spline_pos);
        return;
    }
    const int num_segments = vai->route_info.num_route_segments - vai->route_info.center_info.route_index;
    const lane_spline_base* splines[5];
    for (int i = 0; i < num_segments; i++)
    {
        splines[i] = segment[i].base_spline;
    }
    const lane_spline_base* lane;
    float u;
    float dist_left;
    if (vehicle_ai_find_approx_advanced_position_along_route(closest_spline_pos, lane, u, dist_left, splines, num_segments, vai->route_info.center_info.u, segment->length - vai->route_info.center_info.distance_remaining, advance_length, false, 0.0f))
    {
        const vehicle* vp = World.object_from_handle<vehicle>(vai->vehicle_handle);
        if (vp != NULL)
        {
            vector lateral;
            get_steering_lateral_offset(vai, lane, u, vp->pos, lateral);
            vector vertical;
            get_steering_vertical_offset(vertical, vp, vai);
            closest_spline_pos += lateral + vertical;
        }
        return;
    }
    // Past the end of the route: straight on along the last tangent.
    const cubic_bezier_spline cbs(lane->num_knots, lane->knots.to_pointer(), lane->in_handles.to_pointer(), lane->out_handles.to_pointer());
    vector tangent;
    cbs.get_tangent(tangent, u);
    tangent.normalize_safe();
    closest_spline_pos += tangent * dist_left;
}

// 0x00BC5420 (:5088)
bool vehicle_retains_collision_control(const vehicle* vp)
{
    return vehicle_player_in_vehicle(vp);
}

// 0x00BC5440 (:5120)
void vehicle_ai_set_speed_override(vehicle_ai* vai, float speed_meter_per_sec)
{
    vai->vai_flags.m_override_speed = 1;
    vai->override_speed = speed_meter_per_sec;
}

// 0x00BC5460 (:5136)
void vehicle_ai_clear_speed_override(vehicle_ai* vai)
{
    vai->vai_flags.m_override_speed = 0;
    vai->override_speed = 0.0f;
}

// 0x00BCA020 (:5170): the result of the driver's is_killed call is not used.
void vehicle_ai_inform_dead_passenger(unsigned int vehicle_handle, unsigned int attacker_handle)
{
}

// 0x00BC5480 (:5192)
static const lane_spline* get_current_lane_spline_for_ambient_pathfind(const vehicle* vp)
{
    return NULL;
}

// The start of 0x00BD5350 and 0x00BD5440, which are the same code: the first point becomes the vehicle's lane.
static bool script_path_setup_first_point(vehicle* vp, vector* points)
{
    if (lane_pathfind_get_closest_lane(vp->pos, CLOSEST_LANE_RADIUS, LANE_INCLUDE_TRAFFIC, LANE_EXCLUDE_TRAFFIC) != NULL)
    {
        points[0] = vp->pos;
        return true;
    }
    closest_lane_search_info search_info(vp->pos, &vp->orient.fvec, 200.0f);
    search_info.include_flags = LANE_INCLUDE_TRAFFIC;
    search_info.exclude_flags = LANE_EXCLUDE_TRAFFIC;
    const lane_spline* lane = lane_pathfind_get_closest_oriented_traffic_lane_slow(search_info);
    if (lane == NULL)
    {
        return false;
    }
    traffic_lane_get_position(points[0], lane, 0.5f);
    return true;
}

// 0x00BD5350 (:5210): Original quirk kept: only reports whether a path exists. stop_at_goal is not used, and the path is
// not freed.
bool vehicle_ai_begin_script_ambient_pathfind_to(vehicle* vp, vector* points, int num_points, bool stop_at_goal, bool allow_start_uturn, bool supress_error)
{
    if (num_points < 2 || !script_path_setup_first_point(vp, points))
    {
        return false;
    }
    vehicle_ambient_pathfind_info info(num_points, points, allow_start_uturn, NULL);
    info.supress_error = supress_error;
    return vehicle_create_ambient_path_from_points(info) != NULL;
}

// 0x00BD5440 (:5281): the same code as the ambient one.
bool vehicle_ai_begin_script_navmesh_rail_path(vehicle* vp, vector* points, int num_points, bool allow_start_uturn, bool stop_at_goal, bool supress_error)
{
    if (num_points < 2 || !script_path_setup_first_point(vp, points))
    {
        return false;
    }
    vehicle_ambient_pathfind_info info(num_points, points, allow_start_uturn, NULL);
    info.supress_error = supress_error;
    return vehicle_create_ambient_path_from_points(info) != NULL;
}

// 0x00BC5490 (:5341)
bool vehicle_ai_begin_script_navmesh_pathfind_to(vehicle* vp, const navpoint** points, int num_points, bool loop, bool stop_at_goal)
{
    return true;
}

// 0x00BC54A0 (:5360)
void vehicle_ai_set_lateral_offset(vehicle_ai* vai, float lateral_offset)
{
    vai->vai_flags.m_lateral_offset = 1;
    vai->steer_info.lateral_offset = lateral_offset;
}

// 0x00BC54C0 (:5373)
void vehicle_ai_clear_lateral_offset(vehicle_ai* vai)
{
    vai->vai_flags.m_lateral_offset = 0;
}

// 0x00BC54D0 (:5383)
void vehicle_ai_set_road_offset(vehicle_ai* vai, float road_offset)
{
    if (!vai->vai_flags.m_ignore_lanes)
    {
        vai->vai_flags.m_road_offset = 1;
        vai->steer_info.road_offset = road_offset;
    }
}

// 0x00BC54F0 (:5394)
void vehicle_ai_clear_road_offset(vehicle_ai* vai)
{
    vai->vai_flags.m_road_offset = 0;
}

// 0x00BC5500 (:5405)
void vehicle_ai_set_ignore_lanes(vehicle_ai* vai, bool ignore_lanes)
{
    vai->vai_flags.m_ignore_lanes = ignore_lanes;
    if (ignore_lanes)
    {
        vai->vai_flags.m_road_offset = 0;
    }
}

// 0x00BC5520 (:5421)
bool vehicle_ai_is_vehicle_stuck(vehicle_ai* vai)
{
    return vai->stuck.stuck_active_time.valid() && vai->stuck.stuck_active_time.time_since() < 2000;
}

// 0x00BC5560 (:5431)
void vehicle_ai_force_enter_stuck(vehicle* vp)
{
    if (vp->vai.stuck.stuck_active_time.valid() && vp->vai.stuck.stuck_active_time.time_since() < 2000)
    {
        return;
    }
    vp->vai.stuck.stuck_active_time.set(0);
    vp->vai.stuck.stuck_pos = vp->pos;
    vp->vai.vai_flags.m_unstick_reverse = !automobile_moving_in_reverse(vp->get_handle());
}

// 0x00BC55D0 (:5449)
bool maybe_exit_on_extended_stuck(vehicle* vp)
{
    return true;
}

// 0x00BCDEF0 (:5496): only a ground vehicle, and only when it is streamed in.
bool vehicle_ai_process_stuck(vehicle* vp, bool allow_enter_stuck)
{
    vehicle_ai& vai = vp->vai;
    if (!vp->obj_flags.streamed || vp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
    {
        return false;
    }
    if (vai.stuck.stuck_clear_time.time_since() > 3000)
    {
        vai.stuck.stuck_clear_time.invalidate();
    }
    if (allow_enter_stuck && !vai.stuck.stuck_active_time.valid() && vai.stuck.stuck_threshold_time.time_since() > get_state_time_to_stuck(&vai))
    {
        vai.stuck.stuck_active_time.set(0);
        vai.stuck.stuck_clear_time.invalidate();
        vai.stuck.stuck_pos = vp->pos;
        vai.vai_flags.m_unstick_reverse = !automobile_moving_in_reverse(vp->get_handle());
        if (!vai.stuck.extended_stuck_time.valid())
        {
            vai.stuck.extended_stuck_time.set(0);
        }
        object_path_road* road = World.object_from_handle<object_path_road>(vai.current_path_info.closest_road_spline);
        if (road != NULL)
        {
            vector closest = ZERO_VECTOR;
            road->find_closest_point_on_road(vp->pos, closest, false);
            unsigned int navpoint;
            if (vec_xz_dist(closest, vp->pos) < 5.0f && !is_vehicle_on_bridge(vp, navpoint))
            {
                const float old_stuck_mass = vai.stuck.stuck_mass;
                const float new_stuck_mass = old_stuck_mass + 1.0f;
                vai.stuck.stuck_mass = new_stuck_mass;
                const float mass = new_stuck_mass + (vp->get_extra_mass_value() - old_stuck_mass);
                vai.stuck.stuck_mass_timer.set(get_state_time_to_stuck(&vai) * 3);
                vp->set_extra_mass(true);
                vp->set_extra_mass_value(mass);
                if (!vai.stuck.clear_debris_timer.valid())
                {
                    vai.stuck.clear_debris_timer.set(3000);
                    vai.stuck.clear_debris_state = CDS_DONT;
                }
            }
        }
    }
    if (vai.stuck.stuck_mass_timer.elapsed())
    {
        remove_stuck_mass(vp);
    }
    if (vai.stuck.clear_debris_timer.valid() && vai.stuck.clear_debris_timer.elapsed())
    {
        if (vai.stuck.clear_debris_state < CDS_DELETE)
        {
            vai.stuck.clear_debris_state = (clear_debris_states)(vai.stuck.clear_debris_state + 1);
        }
        vai.stuck.clear_debris_timer.set(1000);
        do_debris_cleanup(vp, vai.stuck.clear_debris_state);
    }
    if (vec_dist_squared(vai.stuck.stuck_pos, vp->pos) > 36.0f) // moved 6 m
    {
        vai.stuck.clear_debris_state = CDS_DONT;
        vai.stuck.clear_debris_timer.invalidate();
    }
    if (vai.stuck.stuck_active_time.valid())
    {
        vai.stuck.stuck_clear_time.set(0);
        const unsigned int since = vai.stuck.stuck_active_time.time_since();
        if (since < 2001)
        {
            const float dist_sq = vec_dist_squared(vai.stuck.stuck_pos, vp->pos);
            const float stopping_dist = automobile_get_speed_squared(vp->get_handle()) / (vai.active_control_info.max_braking_decel * 2.0f);
            const float stopping_dist_sq = stopping_dist * stopping_dist;
            if ((dist_sq >= stopping_dist_sq ? dist_sq : stopping_dist_sq) < 9.0f)
            {
                automobile_set_brake_pedal_input(vai.vehicle_handle, 0.0f);
                automobile_set_forward_or_reverse_accelerator(vai.vehicle_handle, vai.vai_flags.m_unstick_reverse ? -1.0f : 1.0f);
                automobile_set_steering(vai.vehicle_handle, 0.0f);
                vai.vai_flags.u_turn_active = 0;
                return true;
            }
        }
        if ((int)since > 3000)
        {
            vai.stuck.stuck_active_time.invalidate();
        }
    }
    float speed;
    vehicle_get_speed(vp, speed);
    if (speed <= 3.0f && automobile_get_accelerator_value(vai.vehicle_handle) >= 0.2f && (!vp->m_flags.m_all_wheels_on_ground || vp->orient.fvec.y <= 0.5f))
    {
        if (!vai.stuck.stuck_threshold_time.valid())
        {
            vai.stuck.stuck_threshold_time.set(0);
        }
        return false;
    }
    vai.stuck.stuck_threshold_time.invalidate();
    vai.stuck.extended_stuck_time.invalidate();
    return false;
}

// 0x00BC55E0 (:6007)
void vehicle_ai_call_exit_state(vehicle_ai* vai, vehicle_ai_state state)
{
    Vai_state_callbacks[state].exit(vai);
}

// 0x00BC5600 (:6018)
bool vehicle_ai_is_scripted_unstoppable_vehicle(const vehicle* vp)
{
    return false;
}

// 0x00BC5610 (:6047)
void notify_vehicle_vehicle_contact_started(vehicle* vp1, vehicle* vp2)
{
}

// 0x00BC5620 (:6188)
lane_path_node* get_ambient_path_from_route(const vehicle_route_info& route)
{
    if (!route.flags.m_route_segment_inited)
    {
        return NULL;
    }
    lane_path_node* path = NULL;
    for (int i = route.center_info.route_index; i < route.num_route_segments; i++)
    {
        if (route.route[i].flags.from_ambient_path)
        {
            const lane_spline* lane = get_city_spline_from_base_spline(route.route[i].base_spline);
            if (lane != NULL)
            {
                lane_pathnode_append_lane(&path, lane);
            }
        }
    }
    return path;
}

// 0x00BD1360 (:6248)
position_on_route_result find_position_fixed_distance_along_route_beyond_center(route_position_info& position_info)
{
    const vehicle_route_info& route = position_info.route;
    const route_segment* center = vehicle_ai_get_center_route_segment(route);
    if (center == NULL)
    {
        return PRR_ERROR;
    }
    const int num_segments = route.num_route_segments - route.center_info.route_index;
    if (num_segments < 1)
    {
        return PRR_ERROR;
    }
    const lane_spline_base* splines[5];
    float lengths[5];
    splines[0] = center->base_spline;
    lengths[0] = route.center_info.distance_remaining;
    for (int i = 1; i < num_segments; i++)
    {
        splines[i] = center[i].base_spline;
        lengths[i] = splines[i]->arc_length[0];
    }
    if (position_info.flags.add_intermediate_points && position_info.max_intermediate_points > 0)
    {
        const float offset = position_info.intermediate_offset_dist;
        const float last_dist = position_info.distance_along_route - offset;
        float dist = 0.0f;
        for (int i = 0; i < num_segments; i++)
        {
            dist += lengths[i];
            if (last_dist < dist)
            {
                break;
            }
            if (offset <= dist && position_info.num_intermediate_points < position_info.max_intermediate_points)
            {
                position_info.intermediate_points[position_info.num_intermediate_points++] = splines[i]->knots.to_pointer()[splines[i]->num_knots - 1];
            }
        }
    }
    const lane_spline_base* lane;
    float u;
    float dist_left;
    const bool found = vehicle_ai_find_approx_advanced_position_along_route(*position_info.target_pos, lane, u, dist_left, splines, num_segments, route.center_info.u, center->base_spline->arc_length[0] - route.center_info.distance_remaining, position_info.distance_along_route, false, 0.0f);
    return found ? PRR_OK : PRR_SHORT;
}

// 0x00BC5690 (:6313)
bool vehicle_continuing_along_highway(const vehicle* vp)
{
    return false;
}

// 0x00BCA060 (:6332): LTCG, vai in EDI. The newest matching record.
static const vehicle_pathfind_record* vehicle_ai_find_failed_pf_record(const vehicle_ai* vai, const vector& start, const vector& dest)
{
    const circ_array<vehicle_pathfind_record, 5>& history = vai->pf_failure_history;
    for (int i = 0; i < history.size(); i++)
    {
        const vehicle_pathfind_record& record = history[i];
        switch (record.result)
        {
        case VPF_RESULT_FAILURE_BAD_START:
            if (vec_dist_squared(record.start, start) <= VPF_BAD_START_DIST_SQ)
            {
                return &record;
            }
            break;
        case VPF_RESULT_FAILURE_BAD_DESTINATION:
            if (vec_dist_squared(record.dest, dest) <= VPF_BAD_DEST_DIST_SQ)
            {
                return &record;
            }
            break;
        case VPF_RESULT_FAILURE_NO_PATH:
            if (vec_dist_squared(record.start, start) < VPF_BAD_DEST_DIST_SQ && vec_dist_squared(record.dest, dest) <= VPF_BAD_DEST_DIST_SQ)
            {
                return &record;
            }
            break;
        default:
            break;
        }
    }
    return NULL;
}

// 0x00BCA130 (:6374)
bool vehicle_ai_has_cached_pf_failure(const vehicle_ai* vai, const vector& start, const vector& dest, vehicle_pathfind_results& reason)
{
    const vehicle_pathfind_record* record = vehicle_ai_find_failed_pf_record(vai, start, dest);
    if (record != NULL)
    {
        reason = record->result;
    }
    return record != NULL;
}

// 0x00BCA160 (:6391)
void vehicle_ai_record_pf_failure(vehicle_ai* vai, const vector& start, const vector& dest, vehicle_pathfind_results failure_reason)
{
    if (failure_reason >= VPF_RESULT_FAILURE_BAD_START && failure_reason <= VPF_RESULT_FAILURE_NO_PATH)
    {
        vai->pf_failure_history.add(vehicle_pathfind_record(start, dest, failure_reason));
    }
}
