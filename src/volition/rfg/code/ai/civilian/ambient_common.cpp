// Original: volition/rfg/code/ai/civilian/ambient_common.cpp
//
// Routes for ambient vehicles: where one starts on the road network, and A* over the road splines to a despawn point or
// a road near the player. LTCG register mappings are noted above each function.

#include "volition/rfg/code/ai/civilian/ambient_common.h"

#include "volition/rfg/code/ai/road_route.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/pathfinding/road_heap.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"

#include <float.h>

extern road_heap Road_heap; // road_heap.cpp

static const float MIN_DESPAWN_DIST_SQ = 90000.0f; // 300 m from the player
static const float MAX_DESPAWN_SEARCH_DIST_SQ = 250000.0f; // 500 m from the start
static const float LOCAL_ROAD_DIST = 150.0f; // 0x012CEB50

// 0x005E0C10
ambient_road_info::ambient_road_info()
{
}

static object_path_road* get_road_from_handle(unsigned int handle)
{
    object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
    return objp != nullptr && objp->get_obj_type() == OT_PATH_ROAD ? static_cast<object_path_road*>(objp) : nullptr;
}

// 0x00610AA0 (:29). LTCG: road_route EAX.
static bool road_on_route_list(const road_spline_identifier& road_id, const base_array<road_segment_info>& road_route)
{
    for (int i = 0; i < road_route.size(); i++)
    {
        const road_segment_info& segment = road_route[i];
        if (segment.road_handle != road_id.road_spline->get_handle())
        {
            continue;
        }
        if (segment.start_index < segment.end_index)
        {
            if (segment.start_index <= road_id.point && road_id.point < segment.end_index)
            {
                return true;
            }
        }
        else if (road_id.point <= segment.start_index && segment.end_index <= road_id.point)
        {
            return true;
        }
    }
    return false;
}

// 0x00652B60 (:59). LTCG: pos EAX, size XMM0, road_list EDI.
static void collect_local_road_list(const vector& pos, float size, farray<object_path_road*, 322>& road_list)
{
    road_list.clear();
    const vector box_min(pos.x - size, pos.y - size, pos.z - size);
    const vector box_max(pos.x + size, pos.y + size, pos.z + size);
    const int num_roads = World.get_num_objects<object_path_road>();
    for (int i = 0; i < num_roads; i++)
    {
        object_path_road* road = World.get_object<object_path_road>(i);
        if (road->road_flags.special || !road->road_flags.ambient)
        {
            continue;
        }
        vector road_min;
        vector road_max;
        road->get_bbox(road_min, road_max);
        if (ix_box_box_aligned(box_min, box_max, road_min, road_max))
        {
            if (road_list.full())
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            }
            else
            {
                road_list.add(road);
            }
        }
    }
}

// 0x005E0AD0 (:314). LTCG: destination_road_id EAX.
static bool can_despawn_at_spline(const road_spline_identifier& destination_road_id, const player* pp)
{
    const vector point = destination_road_id.road_spline->get_road_point(destination_road_id.point);
    return MIN_DESPAWN_DIST_SQ < vec_dist_squared(pp->pos, point);
}

// Which ways a point can be left. Broken and blocked segments stop the search across them.
static void get_road_exits_blocked(const object_path_road* road, unsigned short point, road_broken_state forward_state, road_broken_state backward_state, bool& forward_blocked, bool& backward_blocked)
{
    forward_blocked = false;
    backward_blocked = false;
    if (road->has_breaks())
    {
        const road_broken_state at_point = road->is_broken_at_index(point);
        const road_broken_state before_point = road->is_broken_at_index(point - 1);
        forward_blocked = at_point == ROAD_BROKEN_COMPLETELY || at_point == forward_state;
        backward_blocked = before_point == ROAD_BROKEN_COMPLETELY || before_point == backward_state;
    }
    if (road->has_road_blocks())
    {
        if (road->is_road_blocked_at_index(point))
        {
            forward_blocked = true;
        }
        if (road->is_road_blocked_at_index(point - 1))
        {
            backward_blocked = true;
        }
    }
}

static void expand_along_road(object_path_road* road, const road_spline_identifier& id, float arrival_cost, const vector& goal_pos, bool forward_blocked, bool backward_blocked)
{
    const int next = (id.point + 1) & 0xFFFF;
    if (!forward_blocked && next < road->spline_data.num_points)
    {
        const float dist = vec_dist(road->get_road_point(id.point), road->get_road_point((unsigned short)next));
        object_path_road_query_for_path(road, (unsigned short)next, road, id.point, dist + arrival_cost, goal_pos, &Road_heap, true, false);
    }
    if (!backward_blocked && id.point != 0 && !road->road_flags.one_way)
    {
        const unsigned short previous = (unsigned short)(id.point - 1);
        const float dist = vec_dist(road->get_road_point(id.point), road->get_road_point(previous));
        object_path_road_query_for_path(road, previous, road, id.point, dist + arrival_cost, goal_pos, &Road_heap, true, false);
    }
}

// 0x00652D00 (:329). LTCG: start_road_id EAX; destination_road_id and last_road_route on the stack.
static bool build_despawn_route_session(const road_spline_identifier& start_road_id, road_spline_identifier& destination_road_id, const base_array<road_segment_info>& last_road_route)
{
    destination_road_id.point = 0;
    destination_road_id.road_spline = nullptr;
    const vector start_pos = start_road_id.road_spline->get_road_point(start_road_id.point);
    const player* pp = player_get_local_player();
    const vector goal_pos(UP_VECTOR.x + start_pos.x, UP_VECTOR.y + start_pos.y, start_pos.z + UP_VECTOR.z);
    if (!object_path_road_query_for_path(start_road_id.road_spline, start_road_id.point, nullptr, start_road_id.point, 0.0f, goal_pos, &Road_heap, true, false))
    {
        return false;
    }
    while (0 < Road_heap.num_road_splines)
    {
        const road_spline_identifier id = Road_heap.remove_best_road_spline();
        object_path_road* road = id.road_spline;
        if (road == nullptr || road->road_flags.gps_only || road_on_route_list(id, last_road_route))
        {
            continue;
        }
        if (can_despawn_at_spline(id, pp))
        {
            destination_road_id = id;
            return true;
        }
        const vector point = road->get_road_point(id.point);
        if (MAX_DESPAWN_SEARCH_DIST_SQ < vec_dist_squared(goal_pos, point))
        {
            continue;
        }
        const road_heap_data* data = Road_heap.get_road_spline_data(&id);
        // Original quirk kept: the left and right tests are the other way round from build_road_route_session_common.
        bool forward_blocked;
        bool backward_blocked;
        get_road_exits_blocked(road, id.point, ROAD_BROKEN_LEFT, ROAD_BROKEN_RIGHT, forward_blocked, backward_blocked);
        expand_along_road(road, id, data->arrival_cost, goal_pos, forward_blocked, backward_blocked);
        const road_spline_connection* connections = road->spline_data.connections.to_pointer();
        for (int i = 0; i < road->spline_data.num_connections; i++)
        {
            if (connections[i].from_pt != id.point)
            {
                continue;
            }
            object_path_road* other = get_road_from_handle(connections[i].handle);
            if (other != nullptr && other->road_flags.ambient && connections[i].to_pt < other->spline_data.num_points)
            {
                object_path_road_query_for_path(other, connections[i].to_pt, road, id.point, data->arrival_cost, goal_pos, &Road_heap, true, false);
            }
        }
    }
    return false;
}

// 0x00653100 (:473). LTCG: start_road_id EAX, the root of the search; destination_road_id on the stack, where it ends.
static bool build_road_route_session_common(const road_spline_identifier& start_road_id, const road_spline_identifier& destination_road_id)
{
    const vector goal_pos = destination_road_id.road_spline->get_road_point(destination_road_id.point);
    if (!object_path_road_query_for_path(start_road_id.road_spline, start_road_id.point, nullptr, start_road_id.point, 0.0f, goal_pos, &Road_heap, true, false))
    {
        return false;
    }
    while (0 < Road_heap.num_road_splines)
    {
        const road_spline_identifier id = Road_heap.remove_best_road_spline();
        object_path_road* road = id.road_spline;
        if (road == nullptr || road->road_flags.gps_only)
        {
            continue;
        }
        if (road == destination_road_id.road_spline && id.point == destination_road_id.point)
        {
            return true;
        }
        const road_heap_data* data = Road_heap.get_road_spline_data(&id);
        bool forward_blocked;
        bool backward_blocked;
        get_road_exits_blocked(road, id.point, ROAD_BROKEN_RIGHT, ROAD_BROKEN_LEFT, forward_blocked, backward_blocked);
        expand_along_road(road, id, data->arrival_cost, goal_pos, forward_blocked, backward_blocked);
        road_spline_connection* connections = road->spline_data.connections.to_pointer();
        for (int i = 0; i < road->spline_data.num_connections; i++)
        {
            if (connections[i].from_pt != id.point || !World.is_zone_streamed(connections[i].handle))
            {
                continue;
            }
            object_path_road* other = get_road_from_handle(connections[i].handle);
            if (other == nullptr || !other->road_flags.ambient)
            {
                continue;
            }
            if (connections[i].to_pt < other->spline_data.num_points)
            {
                object_path_road_query_for_path(other, connections[i].to_pt, road, id.point, data->arrival_cost, goal_pos, &Road_heap, true, false);
            }
            else
            {
                // Original quirk kept: a connection past the other road's end is removed from the road's data, swapping in the last one, which this loop then skips.
                road->spline_data.num_connections--;
                connections[i] = connections[road->spline_data.num_connections];
            }
        }
    }
    return false;
}

// 0x00653470 (:606)
static bool build_road_route_extract(const road_spline_identifier& start_road_id, const road_spline_identifier& destination_road_id, farray<road_segment_info, 8>& spawn_road_list)
{
    farray<road_spline_identifier, 512> waypoints;
    spawn_road_list.clear();
    road_spline_identifier id = destination_road_id;
    if (id.road_spline->spline_data.num_points <= id.point)
    {
        return false;
    }
    while (true)
    {
        waypoints.add(id);
        if (id.road_spline == start_road_id.road_spline && id.point == start_road_id.point)
        {
            break;
        }
        const road_heap_data* data = Road_heap.get_road_spline_data(&id);
        id.road_spline = World.object_from_handle<object_path_road>(data->arrival_spline);
        if (id.road_spline == nullptr)
        {
            break;
        }
        id.point = data->arrival_point;
        if (id.road_spline->spline_data.num_points <= id.point || waypoints.full())
        {
            break;
        }
    }
    road_segment_info segment;
    while (0 < waypoints.size())
    {
        if (extract_road_segment(segment, waypoints))
        {
            spawn_road_list.add(segment);
            if (spawn_road_list.full())
            {
                break;
            }
        }
    }
    return 0 < spawn_road_list.size();
}

// 0x006535C0 (:674). LTCG: start_road_id EDI, spawn_road_list ESI. If avoiding the last route finds nothing, tries again allowing it.
bool build_new_waypoints_despawn(const road_spline_identifier& start_road_id, farray<road_segment_info, 8>& spawn_road_list, const base_array<road_segment_info>& last_road_route)
{
    const farray<road_segment_info, 1> no_route;
    road_spline_identifier destination;
    spawn_road_list.clear();
    Road_heap.start_session();
    if (build_despawn_route_session(start_road_id, destination, last_road_route))
    {
        build_road_route_extract(start_road_id, destination, spawn_road_list);
    }
    Road_heap.end_session();
    if (0 < spawn_road_list.size())
    {
        return true;
    }
    Road_heap.start_session();
    if (build_despawn_route_session(start_road_id, destination, no_route))
    {
        build_road_route_extract(start_road_id, destination, spawn_road_list);
    }
    Road_heap.end_session();
    return 0 < spawn_road_list.size();
}

// 0x006536B0 (:717). LTCG: destination_road_id EDI, spawn_road_list EAX, start_road_id on the stack.
static void build_road_route(const road_spline_identifier& start_road_id, const road_spline_identifier& destination_road_id, farray<road_segment_info, 8>& spawn_road_list)
{
    if (start_road_id.road_spline != destination_road_id.road_spline)
    {
        Road_heap.start_session();
        if (build_road_route_session_common(start_road_id, destination_road_id))
        {
            build_road_route_extract(start_road_id, destination_road_id, spawn_road_list);
        }
        Road_heap.end_session();
        return;
    }
    spawn_road_list.clear();
    spawn_road_list.add(road_segment_info(destination_road_id.road_spline->get_handle(), start_road_id.point, destination_road_id.point));
}

// 0x00610B20 (:750). LTCG: road EDI, pp on the stack.
static unsigned short pick_destination_on_road(const object_path_road* road, const player* pp)
{
    unsigned short best_point = 0;
    float best_dist_sq = vec_dist_squared(pp->pos, road->get_road_point(0));
    for (int i = 1; i < road->spline_data.num_points; i++)
    {
        const float dist_sq = vec_dist_squared(pp->pos, road->get_road_point((unsigned short)i));
        if (dist_sq < best_dist_sq)
        {
            best_point = (unsigned short)i;
            best_dist_sq = dist_sq;
        }
    }
    return best_point;
}

// 0x00610BF0 (:778). LTCG: destination_road_id ESI, pp EDX, road_list on the stack; start_road_id is dropped.
static bool pick_destination_road_player(road_spline_identifier& destination_road_id, const player* pp, const farray<object_path_road*, 322>& road_list)
{
    vector closest;
    float u;
    destination_road_id.road_spline = object_road_get_closest_road_list(road_list, pp->pos, closest, u, false, nullptr, nullptr);
    if (destination_road_id.road_spline == nullptr)
    {
        return false;
    }
    destination_road_id.point = (unsigned short)fl_round(u);
    const unsigned short last = (unsigned short)(destination_road_id.road_spline->spline_data.num_points - 1);
    if (last < destination_road_id.point)
    {
        destination_road_id.point = last;
    }
    return true;
}

// 0x00653760 (:801). LTCG: start_road_id EAX. Any road that comes closer to the player than the start.
static bool pick_destination_road(road_spline_identifier& destination_road_id, const road_spline_identifier& start_road_id, const player* pp, const farray<object_path_road*, 322>& road_list)
{
    farray<object_path_road*, 322> candidates;
    const vector start_pos = start_road_id.road_spline->get_road_point(start_road_id.point);
    const float start_dist_sq = vec_dist_squared(start_pos, pp->pos);
    for (int i = 0; i < road_list.size(); i++)
    {
        object_path_road* road = road_list[i];
        vector closest;
        road->find_closest_point_on_road(start_pos, closest, false);
        if (vec_dist_squared(closest, pp->pos) < start_dist_sq)
        {
            candidates.add(road);
        }
    }
    if (candidates.size() == 0)
    {
        return false;
    }
    destination_road_id.road_spline = candidates[candidates.size() == 1 ? 0 : int_rand_range(0, candidates.size() - 1)];
    destination_road_id.point = pick_destination_on_road(destination_road_id.road_spline, pp);
    return true;
}

// 0x00610C60 (:852). LTCG: road_handle EAX. Faces along the road, on the player's side.
static bool create_road_orient(matrix& spawn_orient, unsigned int road_handle, unsigned short road_index)
{
    const object_path_road* road = get_road_from_handle(road_handle);
    if (road == nullptr)
    {
        return false;
    }
    const player* pp = player_get_local_player();
    const vector point = road->get_road_point(road_index);
    vector tangent;
    road->get_tangent(tangent, (float)road_index);
    tangent.y = 0.0f;
    tangent.normalize_safe();
    vector to_player(pp->pos.x - point.x, 0.0f, pp->pos.z - point.z);
    to_player.normalize_safe();
    if (to_player.is_null())
    {
        spawn_orient = IDENTITY_MATRIX;
        return true;
    }
    if (0.0f < tangent.x * to_player.x + to_player.y * tangent.y + to_player.z * tangent.z)
    {
        spawn_orient.make_quick(tangent);
    }
    else
    {
        spawn_orient.make_quick(vector(-tangent.x, -tangent.y, -tangent.z));
    }
    return true;
}

// 0x00610E60 (:895)
bool build_orient_from_segment(const road_segment_info& road_segment, matrix& spawn_orient)
{
    const unsigned short start = road_segment.start_index;
    if (start == road_segment.end_index)
    {
        return create_road_orient(spawn_orient, road_segment.road_handle, start);
    }
    const object_path_road* road = get_road_from_handle(road_segment.road_handle);
    if (road == nullptr)
    {
        return false;
    }
    const short step = start < road_segment.end_index ? 1 : -1;
    const vector point = road->get_road_point(start);
    const vector next = road->get_road_point((unsigned short)(start + step));
    vector dir(next.x - point.x, 0.0f, next.z - point.z);
    dir.normalize_safe();
    if (dir.is_null())
    {
        return false;
    }
    spawn_orient.make_quick(dir);
    return true;
}

// 0x00610FB0 (:930). LTCG: last_road_route ECX. Carries on along the last segment's road.
static bool find_route_start_point_consecutive(road_spline_identifier& start_road_id, const base_array<road_segment_info>& last_road_route)
{
    if (last_road_route.size() < 1)
    {
        return false;
    }
    const road_segment_info& last = last_road_route[last_road_route.size() - 1];
    if (last.start_index == last.end_index)
    {
        return false;
    }
    object_path_road* road = get_road_from_handle(last.road_handle);
    if (road == nullptr)
    {
        return false;
    }
    int next;
    if (last.end_index < last.start_index)
    {
        if (last.end_index == 0)
        {
            return false;
        }
        next = last.end_index - 1;
    }
    else
    {
        next = last.end_index + 1;
        if (road->spline_data.num_points <= next)
        {
            return false;
        }
    }
    if ((road->has_breaks() && road->is_broken_at_index(next) != ROAD_BROKEN_NEITHER) || (road->has_road_blocks() && road->is_road_blocked_at_index(next)))
    {
        return false;
    }
    start_road_id.road_spline = road;
    start_road_id.point = (unsigned short)next;
    return true;
}

// 0x006538A0 (:995). LTCG: last_road_route ECX.
static bool find_route_start_point_connection(road_spline_identifier& start_road_id, const base_array<road_segment_info>& last_road_route)
{
    if (last_road_route.size() < 1)
    {
        return false;
    }
    const road_segment_info& last = last_road_route[last_road_route.size() - 1];
    const object_path_road* road = get_road_from_handle(last.road_handle);
    if (road == nullptr)
    {
        return false;
    }
    const road_spline_connection* connections = road->spline_data.connections.to_pointer();
    for (int i = 0; i < road->spline_data.num_connections; i++)
    {
        if (connections[i].from_pt == last.end_index)
        {
            start_road_id.road_spline = get_road_from_handle(connections[i].handle);
            if (start_road_id.road_spline == nullptr || !start_road_id.road_spline->road_flags.ambient)
            {
                return false;
            }
            start_road_id.point = connections[i].to_pt;
            return true;
        }
    }
    return false;
}

// 0x006110C0 (:1039). LTCG: last_road_route ECX, start_road_id EDI. After a one-point segment on another road, a step either way along it.
static bool find_route_start_point_intersection(road_spline_identifier& start_road_id, const base_array<road_segment_info>& last_road_route)
{
    if (last_road_route.size() < 2)
    {
        return false;
    }
    const road_segment_info& last = last_road_route[last_road_route.size() - 1];
    if (last.start_index != last.end_index || last.road_handle == last_road_route[last_road_route.size() - 2].road_handle)
    {
        return false;
    }
    start_road_id.road_spline = get_road_from_handle(last.road_handle);
    if (start_road_id.road_spline == nullptr)
    {
        return false;
    }
    const int num_points = start_road_id.road_spline->spline_data.num_points;
    if (last.end_index == 0)
    {
        start_road_id.point = 1;
    }
    else if (last.end_index < num_points - 1)
    {
        start_road_id.point = (vrand() & 1) == 0 ? last.end_index - 1 : last.end_index + 1;
    }
    else
    {
        start_road_id.point = (unsigned short)(num_points - 2);
    }
    return true;
}

// 0x00653950 (:1087)
static bool collect_local_ambient_roads(const vector& point, farray<object_path_road*, 322>& road_list)
{
    road_list.clear();
    const int num_roads = World.get_num_objects<object_path_road>();
    for (int i = 0; i < num_roads; i++)
    {
        object_path_road* road = World.get_object<object_path_road>(i);
        if (!road->road_flags.ambient || road->road_flags.special)
        {
            continue;
        }
        vector road_min;
        vector road_max;
        road->get_bbox(road_min, road_max);
        if (ix_sphere_in_box(point, 40.0f, road_min, road_max))
        {
            if (road_list.full())
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            }
            else
            {
                road_list.add(road);
            }
        }
    }
    return 0 < road_list.size();
}

// 0x00611170 (:1111). The nearest clear point ahead of the vehicle off the last route, otherwise the nearest point of all.
static bool find_route_start_point_list(road_spline_identifier& start_road_id, const vehicle* vp, const base_array<road_segment_info>& last_road_route, const farray<object_path_road*, 322>& road_list)
{
    road_spline_identifier best_clear = { nullptr, 0 };
    road_spline_identifier best_any = { nullptr, 0 };
    float best_clear_dist_sq = FLT_MAX;
    float best_any_dist_sq = FLT_MAX;
    for (int i = 0; i < road_list.size(); i++)
    {
        road_spline_identifier id;
        id.road_spline = road_list[i];
        for (int j = 0; j < id.road_spline->spline_data.num_points; j++)
        {
            id.point = (unsigned short)j;
            const vector point = id.road_spline->get_road_point(id.point);
            const float dist_sq = vec_dist_squared(vp->pos, point);
            if (dist_sq < best_any_dist_sq)
            {
                best_any = id;
                best_any_dist_sq = dist_sq;
            }
            if (best_clear_dist_sq <= dist_sq || road_on_route_list(id, last_road_route))
            {
                continue;
            }
            vector dir(point.x - vp->pos.x, point.y - vp->pos.y, point.z - vp->pos.z);
            dir.normalize_safe();
            if (dir.is_null() || vp->orient.fvec.y * dir.y + vp->orient.fvec.x * dir.x + vp->orient.fvec.z * dir.z < 0.0f)
            {
                continue;
            }
            if ((id.road_spline->has_breaks() && id.road_spline->is_broken_at_index(id.point) != ROAD_BROKEN_NEITHER) || (id.road_spline->has_road_blocks() && id.road_spline->is_road_blocked_at_index(id.point)))
            {
                continue;
            }
            best_clear = id;
            best_clear_dist_sq = dist_sq;
        }
    }
    if (best_clear.road_spline != nullptr)
    {
        start_road_id = best_clear;
        return true;
    }
    if (best_any.road_spline == nullptr)
    {
        return false;
    }
    start_road_id = best_any;
    return true;
}

// 0x00653A40 (:1201). LTCG: start_road_id EAX, last_road_route EDI.
bool find_route_start_point(road_spline_identifier& start_road_id, const vehicle* vp, const base_array<road_segment_info>& last_road_route)
{
    if (find_route_start_point_consecutive(start_road_id, last_road_route) || find_route_start_point_intersection(start_road_id, last_road_route) || find_route_start_point_connection(start_road_id, last_road_route))
    {
        return true;
    }
    farray<object_path_road*, 322> road_list;
    if (collect_local_ambient_roads(vp->pos, road_list))
    {
        return find_route_start_point_list(start_road_id, vp, last_road_route, road_list);
    }
    vector closest;
    float u;
    start_road_id.road_spline = object_road_get_closest_road(vp->pos, false, closest, u, 60.0f, false, nullptr, nullptr, false);
    if (start_road_id.road_spline == nullptr)
    {
        return false;
    }
    start_road_id.point = (unsigned short)fl_round(u);
    return true;
}

// 0x00653B40 (:1250). Original quirk kept: desired_mode is dropped, every route built here is a despawn route.
bool ambient_build_road_waypoints(const vehicle* vp, farray<road_segment_info, 8>& spawn_road_list, ped_node_select_modes, const base_array<road_segment_info>& last_road_route)
{
    road_spline_identifier start_road_id;
    if (!find_route_start_point(start_road_id, vp, last_road_route))
    {
        return false;
    }
    spawn_road_list.clear();
    return build_new_waypoints_despawn(start_road_id, spawn_road_list, last_road_route) && 0 < spawn_road_list.size();
}

// 0x00653BA0 (:1283)
bool ambient_build_road_waypoints(farray<road_segment_info, 8>& spawn_road_list, matrix& spawn_orient, object_path_road* root_road, int start_index)
{
    if (root_road == nullptr || start_index < 0 || root_road->spline_data.num_points <= start_index || !root_road->road_flags.ambient)
    {
        return false;
    }
    spawn_road_list.clear();
    spawn_orient = IDENTITY_MATRIX;
    road_spline_identifier start_road_id;
    start_road_id.road_spline = root_road;
    start_road_id.point = (unsigned short)start_index;
    const player* pp = player_get_local_player();
    farray<object_path_road*, 322> road_list;
    collect_local_road_list(pp->pos, LOCAL_ROAD_DIST, road_list);
    if (road_list.size() < 1)
    {
        return false;
    }
    road_spline_identifier destination;
    const bool picked = bool_rand() ? pick_destination_road_player(destination, pp, road_list) : pick_destination_road(destination, start_road_id, pp, road_list);
    if (!picked)
    {
        return false;
    }
    build_road_route(start_road_id, destination, spawn_road_list);
    return 0 < spawn_road_list.size() && build_orient_from_segment(spawn_road_list[0], spawn_orient);
}
