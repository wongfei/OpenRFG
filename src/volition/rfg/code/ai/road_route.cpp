// Original: volition/rfg/code/ai/road_route.cpp

#include "volition/rfg/code/ai/road_route.h"

#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/pathfinding/road_heap.h"
#include "volition/vlib/code/math/floating.h"

// 0x005F1420 (:250)
road_segment_info::road_segment_info(unsigned int new_road_handle, unsigned short new_start_index, unsigned short new_end_index)
    : road_handle(new_road_handle)
    , start_index(new_start_index)
    , end_index(new_end_index)
{
}

// 0x005F1440 (:258)
void road_segment_info::reset()
{
    road_handle = 0xFFFFFFFF;
    start_index = 0xFFFF;
    end_index = 0xFFFF;
}

// 0x00641180 (:134). The list runs from the destination back to the start, so the segment is built from its end.
bool extract_road_segment(road_segment_info& new_segment, farray<road_spline_identifier, 512>& road_waypoint_list)
{
    new_segment.road_handle = 0xFFFFFFFF;
    new_segment.start_index = 0xFFFF;
    new_segment.end_index = 0xFFFF;
    if (road_waypoint_list.size() < 1)
    {
        return false;
    }
    const road_spline_identifier first = road_waypoint_list[road_waypoint_list.size() - 1];
    road_waypoint_list.remove_unsorted(road_waypoint_list.size() - 1);
    new_segment.road_handle = first.road_spline->get_handle();
    new_segment.start_index = first.point;
    new_segment.end_index = first.point;
    while (0 < road_waypoint_list.size())
    {
        const road_spline_identifier next = road_waypoint_list[road_waypoint_list.size() - 1];
        if (next.road_spline->get_handle() != new_segment.road_handle)
        {
            break;
        }
        road_waypoint_list.remove_unsorted(road_waypoint_list.size() - 1);
        new_segment.end_index = next.point;
    }
    return true;
}

// 0x0066A650. LTCG: start_road_id EAX. A search over the road points from the start, ending at the destination point.
static bool build_road_route_session(const road_spline_identifier& start_road_id, const road_spline_identifier& destination_road_id)
{
    const vector goal_pos = destination_road_id.road_spline->get_road_point(destination_road_id.point);
    if (!object_path_road_query_for_path(start_road_id.road_spline, start_road_id.point, nullptr, start_road_id.point, 0.0f, goal_pos, &Road_heap, true, false))
    {
        return false;
    }
    while (0 < Road_heap.num_road_splines)
    {
        const road_spline_identifier current = Road_heap.remove_best_road_spline();
        object_path_road* road = current.road_spline;
        if (road == nullptr || road->road_flags.gps_only)
        {
            continue;
        }
        if (road == destination_road_id.road_spline && current.point == destination_road_id.point)
        {
            return true;
        }
        const road_heap_data* data = Road_heap.get_road_spline_data(&current);
        bool blocked_ahead = false;
        bool blocked_behind = false;
        if (road->has_breaks())
        {
            const road_broken_state ahead = road->is_broken_at_index(current.point);
            const road_broken_state behind = road->is_broken_at_index(current.point - 1);
            blocked_ahead = ahead == ROAD_BROKEN_COMPLETELY || ahead == ROAD_BROKEN_RIGHT;
            blocked_behind = behind == ROAD_BROKEN_COMPLETELY || behind == ROAD_BROKEN_LEFT;
        }
        if (road->has_road_blocks())
        {
            if (road->is_road_blocked_at_index(current.point))
            {
                blocked_ahead = true;
            }
            if (road->is_road_blocked_at_index(current.point - 1))
            {
                blocked_behind = true;
            }
        }
        const unsigned short next_point = current.point + 1;
        if (!blocked_ahead && next_point < road->spline_data.num_points)
        {
            const float step = vec_dist(road->get_road_point(current.point), road->get_road_point(next_point));
            object_path_road_query_for_path(road, next_point, road, current.point, step + data->arrival_cost, goal_pos, &Road_heap, true, false);
        }
        if (!blocked_behind && current.point != 0 && !road->road_flags.one_way)
        {
            const unsigned short previous_point = current.point - 1;
            const float step = vec_dist(road->get_road_point(current.point), road->get_road_point(previous_point));
            object_path_road_query_for_path(road, previous_point, road, current.point, step + data->arrival_cost, goal_pos, &Road_heap, true, false);
        }
        const road_spline_connection* connections = road->spline_data.connections.to_pointer();
        for (int i = 0; i < road->spline_data.num_connections; i++)
        {
            const road_spline_connection& connection = connections[i];
            if (connection.from_pt != current.point || connection.handle == 0xFFFFFFFF)
            {
                continue;
            }
            object_path_road* other = World.object_from_handle<object_path_road>(connection.handle);
            if (other != nullptr && connection.to_pt < other->spline_data.num_points)
            {
                object_path_road_query_for_path(other, connection.to_pt, road, current.point, data->arrival_cost, goal_pos, &Road_heap, true, false);
            }
        }
    }
    return false;
}

// 0x0066A960. Follows the arrival points from the destination back to the start and cuts the list into segments.
static bool build_road_route_extract(const road_spline_identifier& start_road_id, const road_spline_identifier& destination_road_id, base_array<road_segment_info>& route_result)
{
    farray<road_spline_identifier, 512> waypoints;
    route_result.clear();
    road_spline_identifier current = destination_road_id;
    if (current.road_spline->spline_data.num_points <= current.point)
    {
        return false;
    }
    while (true)
    {
        waypoints.add(current);
        if (current.road_spline == start_road_id.road_spline && current.point == start_road_id.point)
        {
            break;
        }
        const road_heap_data* data = Road_heap.get_road_spline_data(&current);
        current.road_spline = World.object_from_handle<object_path_road>(data->arrival_spline);
        if (current.road_spline == nullptr)
        {
            break;
        }
        current.point = data->arrival_point;
        if (current.road_spline->spline_data.num_points <= current.point || waypoints.capacity() <= waypoints.size())
        {
            break;
        }
    }
    road_segment_info segment;
    while (0 < waypoints.size())
    {
        if (extract_road_segment(segment, waypoints))
        {
            route_result.add(segment);
            if (route_result.capacity() <= route_result.size())
            {
                break;
            }
        }
    }
    return 0 < route_result.size();
}

// 0x0066AAB0 (:272)
bool road_route_create(const road_spline_identifier& start_road_id, const road_spline_identifier& destination_road_id, base_array<road_segment_info>& route_result)
{
    route_result.clear();
    if (start_road_id.road_spline == destination_road_id.road_spline)
    {
        route_result.add(road_segment_info(destination_road_id.road_spline->get_handle(), start_road_id.point, destination_road_id.point));
        return true;
    }
    Road_heap.start_session();
    if (build_road_route_session(start_road_id, destination_road_id))
    {
        build_road_route_extract(start_road_id, destination_road_id, route_result);
    }
    Road_heap.end_session();
    return 0 < route_result.size();
}

// 0x0066AB70 (:308)
bool road_route_create(const vector& start_pos, const vector& end_pos, base_array<road_segment_info>& route_result)
{
    vector closest_point;
    float u_value;
    road_spline_identifier start_road_id;
    start_road_id.road_spline = object_road_get_closest_road(start_pos, false, closest_point, u_value, 60.0f, false, nullptr, nullptr, false);
    if (start_road_id.road_spline == nullptr)
    {
        return false;
    }
    start_road_id.point = (unsigned short)fl_round(u_value);
    road_spline_identifier destination_road_id;
    destination_road_id.road_spline = object_road_get_closest_road(end_pos, false, closest_point, u_value, 60.0f, false, nullptr, nullptr, false);
    if (destination_road_id.road_spline == nullptr)
    {
        return false;
    }
    destination_road_id.point = (unsigned short)fl_round(u_value);
    return road_route_create(start_road_id, destination_road_id, route_result);
}
