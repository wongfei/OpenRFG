// Original: volition/rfg/code/gameplay/road_distance_info.cpp

#include "volition/rfg/code/gameplay/road_distance_info.h"

#include "volition/rfg/code/ai/road_route.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/pathfinding/road_heap.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/os/frametime.h"

#include <float.h>

// size 0x18
struct road_info_player_info
{
    unsigned int road_handle; // 0x00
    vector pos; // 0x04
    int framecount; // 0x10
    unsigned short nearest_index; // 0x14
};

farray<road_distance_info, 256> Master_road_info_list; // 0x01658D28
static road_info_player_info Last_player_info; // 0x0224CF98
static const float ROAD_SEARCH_DISTANCE = 80.0f; // 0x012DD4C0

// 0x00775310. LTCG: nearest_index EDI.
static object_path_road* find_road_nearest_player(unsigned short& nearest_index)
{
    const player* local = player_get_local_player();
    if (local == nullptr)
    {
        return nullptr;
    }
    vector closest_point;
    float u_value;
    object_path_road* road = object_road_get_closest_road(local->pos, false, closest_point, u_value, ROAD_SEARCH_DISTANCE, false, nullptr, nullptr, false);
    if (road == nullptr)
    {
        return nullptr;
    }
    nearest_index = (unsigned short)fl_round(u_value);
    const unsigned short last_point = (unsigned short)road->spline_data.num_points - 1;
    if (last_point < nearest_index)
    {
        nearest_index = last_point;
    }
    return road;
}

// 0x00775390. Over 5 m from the last build, once a frame, and on another point of the roads.
bool should_rebuild_road_info()
{
    if (Framecount == Last_player_info.framecount)
    {
        return false;
    }
    const player* local = player_get_local_player();
    if (local == nullptr || vec_dist_squared(local->pos, Last_player_info.pos) < 25.0f)
    {
        return false;
    }
    Last_player_info.framecount = Framecount;
    Last_player_info.pos = local->pos;
    unsigned short nearest_index;
    const object_path_road* road = find_road_nearest_player(nearest_index);
    return road == nullptr || road->get_handle() != Last_player_info.road_handle || nearest_index != Last_player_info.nearest_index;
}

// 0x0078FD30. A search over the road points within 500 m, from the point nearest the player. The caller's road is
// never read and its index is overwritten: the nearest road is looked up again in the same frame, so they agree.
static void build_road_info_session(object_path_road*, unsigned short start_index)
{
    Master_road_info_list.clear();
    object_path_road* start_road = find_road_nearest_player(start_index);
    if (start_road == nullptr)
    {
        return;
    }
    const vector& start_pos = start_road->get_road_point(start_index);
    const vector goal_pos(start_pos.x + UP_VECTOR.x, start_pos.y + UP_VECTOR.y, start_pos.z + UP_VECTOR.z);
    if (!object_path_road_query_for_path(start_road, start_index, nullptr, start_index, 0.0f, goal_pos, &Road_heap, true, false))
    {
        return;
    }
    while (0 < Road_heap.num_road_splines)
    {
        const road_spline_identifier current = Road_heap.remove_best_road_spline();
        object_path_road* road = current.road_spline;
        if (road == nullptr || road->road_flags.gps_only)
        {
            continue;
        }
        const road_heap_data* data = Road_heap.get_road_spline_data(&current);
        if (data == nullptr || 250000.0f < vec_xz_dist_squared(road->get_road_point(current.point), goal_pos) || road->road_flags.special || !World.is_zone_streamed(road->get_handle()))
        {
            continue;
        }
        road_distance_info info;
        info.road_handle = road->get_handle();
        info.travel_distance = data->arrival_cost;
        info.index = current.point;
        Master_road_info_list.add(info);
        if (Master_road_info_list.capacity() <= Master_road_info_list.size())
        {
            return;
        }
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
            if (connection.from_pt != current.point)
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
}

// 0x00790120 (:251)
void build_road_info()
{
    const player* local = player_get_local_player();
    if (local == nullptr)
    {
        return;
    }
    Last_player_info.framecount = Framecount;
    Last_player_info.pos = local->pos;
    unsigned short nearest_index;
    object_path_road* road = find_road_nearest_player(nearest_index);
    if (road == nullptr)
    {
        Last_player_info.road_handle = 0xFFFFFFFF;
        Last_player_info.nearest_index = 0xFFFF;
        return;
    }
    Last_player_info.road_handle = road->get_handle();
    Last_player_info.nearest_index = nearest_index;
    Road_heap.start_session();
    build_road_info_session(road, nearest_index);
    Road_heap.end_session();
}

// 0x007901B0 (:304)
void road_info_level_init_post()
{
    Master_road_info_list.clear();
    build_road_info();
}

// 0x007901C0 (:313)
float road_info_player_travel_distance(unsigned int road_handle, unsigned short index)
{
    if (road_handle == 0xFFFFFFFF)
    {
        return FLT_MAX;
    }
    if (should_rebuild_road_info())
    {
        build_road_info();
    }
    for (int i = 0; i < Master_road_info_list.size(); i++)
    {
        const road_distance_info& info = Master_road_info_list[i];
        if (info.road_handle == road_handle && info.index == index)
        {
            return info.travel_distance;
        }
    }
    return FLT_MAX;
}

// 0x00790220 (:339)
const farray<road_distance_info, 256>& road_info_get_list()
{
    if (should_rebuild_road_info())
    {
        build_road_info();
    }
    return Master_road_info_list;
}
