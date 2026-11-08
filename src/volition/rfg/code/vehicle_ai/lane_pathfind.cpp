// Original: volition/rfg/code/vehicle_ai/lane_pathfind.cpp
//
// A* over the traffic lane graph and the lane queries (NPC.md NP5). All of it is whole, and all of it depends on
// traffic data: traffic_load_master has no caller in rfg.exe, so Traffic_data stays null and every lane query finds
// nothing. The path node pool does run: vehicle_ai.cpp frees and appends through it.

#include "volition/rfg/code/vehicle_ai/lane_pathfind.h"

#include "volition/rfg/code/traffic/traffic.h"
#include "volition/rfg/code/traffic/traffic_shared.h"
#include "volition/rfg/code/vehicle_ai/lane_heap.h"
#include "volition/vlib/code/math/spline.h"

#include <float.h>

extern traffic_data* Traffic_data; // traffic.cpp

// size 0x8
struct lane_selection_info
{
    float max_dist; // 0x00
    float min_dot; // 0x04
};

// 0x01339F50: search radius and the least agreement with the facing asked for, loosened step by step.
static const lane_selection_info Lane_selection_info[6] = {
    { 10.0f, 0.70710677f },
    { 10.0f, 0.25881904f },
    { 20.0f, 0.0f },
    { 20.0f, -1.0f },
    { 30.0f, -1.0f },
    { 5000.0f, -1.0f },
};

static lane_path_node Lane_path_nodes[1024]; // 0x03871120
static lane_path_node* Free_lane_path_nodes; // 0x03870CA0

// The spline flags that stop a lane change: no lane to the left, no lane to the right (read from their uses).
static const int LANE_FLAG_NO_LEFT = 1;
static const int LANE_FLAG_NO_RIGHT = 2;
static const unsigned short LANE_FLAG_GPS_HALF_COST = 0x200;
static const float LANE_CHANGE_COST = 6.0f;

static inline const vector& lane_last_knot(const lane_spline* lane)
{
    return lane->knots.to_pointer()[lane->num_knots - 1];
}

static inline cubic_bezier_spline lane_bezier(const lane_spline* lane)
{
    return cubic_bezier_spline(lane->num_knots, lane->knots.to_pointer(), lane->in_handles.to_pointer(), lane->out_handles.to_pointer());
}

// Appends to a circular list; the pool's and a path's lists have the same shape. Inlined at each use in rfg.exe.
static inline void path_list_append(lane_path_node*& head, lane_path_node* node)
{
    if (head == nullptr)
    {
        node->next = node;
        node->prev = node;
        head = node;
    }
    else
    {
        node->prev = head->prev;
        node->next = head;
        head->prev->next = node;
        head->prev = node;
    }
}

// Unlinks from a circular list, moving its head if needed.
static inline void path_list_remove(lane_path_node*& head, lane_path_node* node)
{
    if (head == node && node->next == node)
    {
        head = nullptr;
    }
    else
    {
        if (head == node)
        {
            head = node->next;
        }
        if (node->prev != nullptr)
        {
            node->prev->next = node->next;
        }
        if (node->next != nullptr)
        {
            node->next->prev = node->prev;
        }
    }
    node->next = nullptr;
    node->prev = nullptr;
}

// 0x00BC39B0 (:76)
lane_path_info::lane_path_info(const lane_spline* _start_lane, const lane_spline* _end_lane, int _lane_include_filter, int _lane_exclude_filter)
    : start_lane(_start_lane), end_lanes_multi(nullptr), end_lane_single(_end_lane), num_end_lanes(1), lane_include_filter(_lane_include_filter), lane_exclude_filter(_lane_exclude_filter), max_path_length(3000.0f), gps_path(false)
{
    goal_pos.zero();
}

// 0x00BC3A00 (:91)
lane_path_info::lane_path_info(const lane_spline* _start_lane, const lane_spline** _end_lanes, int _num_end_lanes, const vector& _goal_pos, int _lane_include_filter, int _lane_exclude_filter)
    : start_lane(_start_lane), end_lanes_multi(_end_lanes), end_lane_single(nullptr), num_end_lanes(_num_end_lanes), lane_include_filter(_lane_include_filter), lane_exclude_filter(_lane_exclude_filter), max_path_length(3000.0f), gps_path(false), goal_pos(_goal_pos)
{
}

// 0x00BC3A60 (:143)
void lane_path_level_init_pre()
{
    Free_lane_path_nodes = nullptr;
    for (int i = 0; i < 1024; i++)
    {
        path_list_append(Free_lane_path_nodes, &Lane_path_nodes[i]);
    }
}

// 0x00BC3B10 (:163)
static lane_path_node* get_free_lane_path_node()
{
    lane_path_node* node = Free_lane_path_nodes;
    if (node != nullptr)
    {
        path_list_remove(Free_lane_path_nodes, node);
    }
    return node;
}

// 0x00BC3B60 (:186)
void free_lane_path_node(lane_path_node** path, lane_path_node* node)
{
    path_list_remove(*path, node);
    path_list_append(Free_lane_path_nodes, node);
}

// 0x00BCCDD0 (:198): LTCG, lsn in EAX, goal_pos in ESI.
static float estimate_dist_to_goal(const lane_search_node* lsn, const vector& goal_pos)
{
    return vec_dist(lane_last_knot(lsn->lane.to_pointer()), goal_pos);
}

// 0x00BC3BE0 (:203): does nothing when the pool is empty.
void lane_pathnode_append_lane(lane_path_node** path, const lane_spline* lane)
{
    lane_path_node* node = get_free_lane_path_node();
    if (node != nullptr)
    {
        node->lane = lane;
        path_list_append(*path, node);
    }
}

// 0x00BD3FA0 (:223): LTCG, lsn in ESI, arrival_node in EAX. A null arrival_node is the search's start.
static void lane_node_query_for_path(lane_search_node* lsn, lane_search_node* arrival_node, float arrival_cost, const vector& goal_pos, bool adjacent, bool gps_path)
{
    const float estimate_scale = gps_path ? 0.5f : 1.0f;
    _heap_data& heap = lsn->heap_data;
    if (Lane_heap.session_id != heap.session_id)
    {
        heap.session_id = Lane_heap.session_id;
        if (arrival_node != nullptr)
        {
            heap.arrival_cost = arrival_cost;
            heap.flags.adjacent = adjacent;
            heap.flags.is_open = 1;
            lsn->parent.from_pointer(arrival_node);
        }
        else
        {
            heap.flags.is_open = 0;
            heap.flags.adjacent = 0;
            heap.arrival_cost = 0.0f;
            lsn->parent.from_pointer(nullptr);
        }
        heap.est_cost_to_goal = estimate_scale * estimate_dist_to_goal(lsn, goal_pos);
        Lane_heap.insert_node(lsn);
    }
    else if (heap.flags.is_open && arrival_cost < heap.arrival_cost)
    {
        heap.arrival_cost = arrival_cost;
        heap.est_cost_to_goal = estimate_scale * estimate_dist_to_goal(lsn, goal_pos);
        lsn->parent.from_pointer(arrival_node);
        Lane_heap.adjust_node(lsn);
    }
}

// 0x00BCCDF0 (:264): LTCG, path in EDI. Each node gets the distance from the end of the first lane to the end of its
// own. A lane change counts LANE_CHANGE_COST instead of the lane's length.
static void get_path_distance(lane_path_node* path)
{
    if (path == nullptr)
    {
        return;
    }
    float total = -path->lane->arc_length[0];
    const lane_path_node* prev = nullptr;
    lane_path_node* node = path;
    do
    {
        const lane_spline* lane = node->lane;
        float length = lane->arc_length[0];
        if (prev != nullptr && (prev->lane->left.to_pointer() == lane || prev->lane->right.to_pointer() == lane))
        {
            length = LANE_CHANGE_COST;
        }
        total += length;
        node->dist = total;
        prev = node;
        node = node->next;
    } while (node != path && node != nullptr);
}

// 0x00BCCE60 (:297): LTCG, test_lane in EAX, fixed_lane in EDI.
static bool are_lanes_adjacent(const lane_spline* test_lane, const lane_spline* fixed_lane)
{
    const lane_spline* lane = test_lane;
    while (lane->left.to_pointer() != nullptr && (lane->spline_flags & LANE_FLAG_NO_LEFT) == 0)
    {
        lane = lane->left.to_pointer();
        if (lane == fixed_lane)
        {
            return true;
        }
    }
    lane = test_lane;
    while (lane->right.to_pointer() != nullptr && (lane->spline_flags & LANE_FLAG_NO_RIGHT) == 0)
    {
        lane = lane->right.to_pointer();
        if (lane == fixed_lane)
        {
            return true;
        }
    }
    return false;
}

// 0x00BCCEC0 (:322): LTCG, tree_node in EAX. Walks the search tree back from the goal into a forward path, dropping the
// lane before a lane change. Original bug: an empty pool writes through null (0x00BCCF2B), as the inlined pop assumes a
// node. Written with the helpers, the null reaches the same write.
static void create_lane_path(lane_path_node** path, lane_search_node* tree_node)
{
    lane_path_node* backward = nullptr;
    for (lane_search_node* lsn = tree_node; lsn != nullptr; lsn = lsn->parent.to_pointer())
    {
        lane_path_node* node = get_free_lane_path_node();
        node->lane = lsn->lane.to_pointer();
        path_list_append(backward, node);
    }
    lane_path_node* forward = nullptr;
    while (backward != nullptr)
    {
        lane_path_node* node = backward->prev;
        path_list_remove(backward, node);
        path_list_append(forward, node);
    }
    lane_path_node* prev = nullptr;
    lane_path_node* node = forward;
    while (node != nullptr)
    {
        lane_path_node* next = node->next != forward ? node->next : nullptr;
        if (prev != nullptr && are_lanes_adjacent(prev->lane, node->lane))
        {
            free_lane_path_node(&forward, prev);
            // The test above compares with the head as it is now, as rfg.exe does.
            next = node->next != forward ? node->next : nullptr;
        }
        prev = node;
        node = next;
    }
    get_path_distance(forward);
    *path = forward;
}

// 0x00BC3C60 (:376)
void vehicle_free_lane_path(lane_path_node** path)
{
    lane_path_node* node = *path;
    while (node != nullptr)
    {
        lane_path_node* next = node->next != *path ? node->next : nullptr;
        free_lane_path_node(path, node);
        node = next;
    }
}

// A link of a search node, by index.
static inline lane_search_node* search_link(lane_search_node* lsn, int index)
{
    return lsn->links.to_pointer()[index].to_pointer();
}

static inline bool lane_passes_filter(const lane_search_node* lsn, int include_filter, int exclude_filter)
{
    const int flags = lsn->lane.to_pointer()->spline_flags;
    return (include_filter & flags) != 0 && (exclude_filter & flags) == 0;
}

// The start's own lane changes: walks one side as far as the filter and the flags allow, each hop a lane change.
static void query_start_side(lane_search_node* start_node, bool left_side, const vector& goal_pos, int include_filter, int exclude_filter, bool gps_search)
{
    lane_search_node* lsn = start_node;
    while (true)
    {
        const short link_index = left_side ? lsn->left_link_index : lsn->right_link_index;
        if (link_index == -1 || (lsn->lane.to_pointer()->spline_flags & (left_side ? LANE_FLAG_NO_LEFT : LANE_FLAG_NO_RIGHT)) != 0)
        {
            return;
        }
        lane_search_node* side = search_link(lsn, link_index);
        if (!lane_passes_filter(side, include_filter, exclude_filter))
        {
            return;
        }
        lane_node_query_for_path(side, lsn, lsn->heap_data.arrival_cost + LANE_CHANGE_COST, goal_pos, true, gps_search);
        lsn = side;
    }
}

// A lane change from a node the search has reached. Original bug: the GPS test reads the link after the forward links,
// whichever side is being changed to (EBP is left at the forward link count, 0x00BD4427).
static void query_side(lane_search_node* lsn, bool left_side, const vector& goal_pos, int include_filter, int exclude_filter, bool gps_search)
{
    const short link_index = left_side ? lsn->left_link_index : lsn->right_link_index;
    if (link_index == -1 || (lsn->lane.to_pointer()->spline_flags & (left_side ? LANE_FLAG_NO_LEFT : LANE_FLAG_NO_RIGHT)) != 0)
    {
        return;
    }
    lane_search_node* side = search_link(lsn, link_index);
    if (!lane_passes_filter(side, include_filter, exclude_filter) || lsn->heap_data.flags.adjacent)
    {
        return;
    }
    float cost = LANE_CHANGE_COST;
    if (gps_search && (search_link(lsn, lsn->num_forward_links)->lane.to_pointer()->spline_flags & LANE_FLAG_GPS_HALF_COST) != 0)
    {
        cost = 3.0f;
    }
    lane_node_query_for_path(side, lsn, lsn->heap_data.arrival_cost + cost, goal_pos, true, gps_search);
}

// 0x00BD4120 (:400): LTCG, start_node in EAX.
static lane_path_node* lane_get_path(lane_search_node* start_node, lane_search_node** end_node, int num_end_nodes, const vector& goal_pos, int lane_include_filter, int lane_exclude_filter, float max_path_length, bool gps_search)
{
    for (int i = 0; i < num_end_nodes; i++)
    {
        if (start_node == end_node[i])
        {
            lane_path_node* path = nullptr;
            lane_pathnode_append_lane(&path, start_node->lane.to_pointer());
            return path;
        }
    }
    Lane_heap.start_session();
    lane_node_query_for_path(start_node, nullptr, 0.0f, goal_pos, false, gps_search);
    query_start_side(start_node, true, goal_pos, lane_include_filter, lane_exclude_filter, gps_search);
    query_start_side(start_node, false, goal_pos, lane_include_filter, lane_exclude_filter, gps_search);

    lane_search_node* found = nullptr;
    while (found == nullptr && Lane_heap.num_nodes > 0)
    {
        lane_search_node* lsn = Lane_heap.remove_best_node();
        for (int i = 0; i < num_end_nodes; i++)
        {
            if (lsn == end_node[i])
            {
                found = lsn;
                break;
            }
        }
        if (found != nullptr || lsn->heap_data.arrival_cost > max_path_length)
        {
            continue;
        }
        lsn->heap_data.flags.is_open = 0;
        const int num_forward = lsn->num_forward_links;
        for (int i = 0; i < num_forward; i++)
        {
            lane_search_node* next = search_link(lsn, i);
            if (!lane_passes_filter(next, lane_include_filter, lane_exclude_filter))
            {
                continue;
            }
            const lane_spline* next_lane = next->lane.to_pointer();
            float length = next_lane->arc_length[0];
            if (gps_search && (next_lane->spline_flags & LANE_FLAG_GPS_HALF_COST) != 0)
            {
                length *= 0.5f;
            }
            lane_node_query_for_path(next, lsn, lsn->heap_data.arrival_cost + length, goal_pos, false, gps_search);
        }
        query_side(lsn, true, goal_pos, lane_include_filter, lane_exclude_filter, gps_search);
        query_side(lsn, false, goal_pos, lane_include_filter, lane_exclude_filter, gps_search);
    }
    Lane_heap.end_session();
    if (found == nullptr)
    {
        return nullptr;
    }
    lane_path_node* path;
    create_lane_path(&path, found);
    return path;
}

// The search node of a lane, or null.
static inline lane_search_node* lane_search_node_of(const lane_spline* lane)
{
    const int index = get_traffic_lane_index(lane);
    if (index < 0 || index >= Traffic_data->num_lane_splines)
    {
        return nullptr;
    }
    return &Traffic_data->lane_search_tree.to_pointer()[index];
}

// 0x00BD4590 (:576)
lane_path_node* lane_pathfind_get_path(const lane_path_info& pathfind_info)
{
    lane_search_node* start_node = lane_search_node_of(pathfind_info.start_lane);
    if (start_node == nullptr)
    {
        return nullptr;
    }
    lane_search_node* end_nodes[16];
    int num_end_nodes = 0;
    vector goal_pos;
    if (pathfind_info.num_end_lanes > 1)
    {
        if (pathfind_info.end_lanes_multi == nullptr)
        {
            return nullptr;
        }
        for (int i = 0; i < pathfind_info.num_end_lanes && num_end_nodes < 16; i++)
        {
            lane_search_node* node = lane_search_node_of(pathfind_info.end_lanes_multi[i]);
            if (node != nullptr)
            {
                end_nodes[num_end_nodes++] = node;
            }
        }
        if (num_end_nodes == 0)
        {
            return nullptr;
        }
        goal_pos = pathfind_info.goal_pos;
    }
    else
    {
        if (pathfind_info.num_end_lanes != 1)
        {
            return nullptr;
        }
        const lane_spline* end_lane = pathfind_info.end_lane_single;
        if (end_lane == nullptr)
        {
            if (pathfind_info.end_lanes_multi == nullptr || pathfind_info.end_lanes_multi[0] == nullptr)
            {
                return nullptr;
            }
            end_lane = pathfind_info.end_lanes_multi[0];
        }
        lane_search_node* node = lane_search_node_of(end_lane);
        if (node == nullptr)
        {
            return nullptr;
        }
        end_nodes[0] = node;
        num_end_nodes = 1;
        // A single lane's goal is the middle of its two ends.
        const lane_spline* lane = node->lane.to_pointer();
        goal_pos = (lane->knots.to_pointer()[0] + lane_last_knot(lane)) * 0.5f;
    }
    return lane_get_path(start_node, end_nodes, num_end_nodes, goal_pos, pathfind_info.lane_include_filter, pathfind_info.lane_exclude_filter, pathfind_info.max_path_length, pathfind_info.gps_path);
}

// 0x00BCD070 (:633)
const lane_spline* lane_pathfind_get_closest_lane(const vector& in_pos, float radius, int include_flags, int exclude_flags)
{
    lane_spline* found[30];
    traffic_lane_query query(in_pos, radius, 30, found);
    query.include_flags = include_flags;
    query.exclude_flags = exclude_flags;
    traffic_get_lanes_near_point(query);
    const lane_spline* best = nullptr;
    float best_dist = FLT_MAX;
    for (int i = 0; i < query.num_found_lane_splines; i++)
    {
        const lane_spline* lane = found[i];
        const vector* points = lane->approx_points.to_pointer();
        for (int j = 0; j < lane->seg_count; j++)
        {
            float t;
            const float dist = vec_find_closest_dist_squared_to_linesegment(t, in_pos, points[j], points[j + 1]);
            if (dist < best_dist)
            {
                best_dist = dist;
                best = lane;
            }
        }
    }
    return best;
}

// 0x00BCF560 (:673)
bool lane_pathfind_get_closest_oriented_lane(closest_spline_info& spline_info, lane_spline** lanes, int num_lanes)
{
    if (num_lanes == 0)
    {
        return false;
    }
    spline_info.lsp = nullptr;
    spline_info.best_dist_sqr = FLT_MAX;
    for (int i = 0; i < num_lanes; i++)
    {
        const lane_spline* lane = lanes[i];
        const cubic_bezier_spline spline = lane_bezier(lane);
        const float u = spline.get_closest_point_approx(spline_info.pos);
        vector point;
        spline.get_point(point, u);
        if (spline_info.fvec != nullptr)
        {
            vector tangent;
            spline.get_tangent(tangent, u);
            tangent.normalize_safe();
            const vector& fvec = *spline_info.fvec;
            if (fvec.y * tangent.y + fvec.x * tangent.x + fvec.z * tangent.z < spline_info.min_dot)
            {
                continue;
            }
        }
        const float dist = vec_dist_squared(spline_info.pos, point);
        if (dist < spline_info.best_dist_sqr)
        {
            spline_info.best_dist_sqr = dist;
            spline_info.lsp = lane;
            spline_info.parametric_pos = u;
        }
    }
    return spline_info.lsp != nullptr;
}

// 0x00BCD180 (:725): at most 4 lanes across, the start's side first. Original: the walk left stops at a lane that points
// back at the one it came from, which is how a road's two directions meet, and then lists the far side going right.
int lane_pathfind_get_adjacent_lanes(const lane_spline* start_lane, const lane_spline** adjacent_lanes)
{
    int count = 0;
    for (const lane_spline* lane = start_lane; lane != nullptr; lane = lane->right.to_pointer())
    {
        adjacent_lanes[count++] = lane;
        if (count == 4)
        {
            return 4;
        }
    }
    const lane_spline* lane = start_lane;
    while (lane->left.to_pointer() != nullptr && lane->left.to_pointer()->left.to_pointer() != lane)
    {
        lane = lane->left.to_pointer();
        adjacent_lanes[count++] = lane;
        if (count == 4)
        {
            return 4;
        }
    }
    for (const lane_spline* far_lane = lane->left.to_pointer(); far_lane != nullptr; far_lane = far_lane->right.to_pointer())
    {
        adjacent_lanes[count++] = far_lane;
        if (count == 4)
        {
            return 4;
        }
    }
    return count;
}

// 0x00BC3CF0 (:780): LTCG, target_radius in XMM0.
static int get_lane_search_index(float target_radius)
{
    for (int i = 0; i < 6; i++)
    {
        if (target_radius <= Lane_selection_info[i].max_dist)
        {
            return i;
        }
    }
    return 5;
}

// 0x00BC3D30 (:795)
closest_lane_search_info::closest_lane_search_info(const vector& _search_pos, const vector* _fvec, float _max_refind_search_dist)
    : search_pos(_search_pos), include_flags(8), exclude_flags(0x104), fvec(_fvec), max_refine_search_dist(_max_refind_search_dist)
{
}

// 0x00BC3D60 (:808): a bisection between 0 and 5000, at most 10 tries, then 100.
int find_smallest_multiple_of_10_radius_containing_splines(const closest_lane_search_info& search_info)
{
    const int max_refine = (int)search_info.max_refine_search_dist;
    int radius = 20;
    int high = 5000;
    int low = 0;
    for (int tries = 0; tries < 10; tries++)
    {
        const bool found = traffic_lanes_exist_near_point(search_info.search_pos, (float)radius, search_info.include_flags, search_info.exclude_flags);
        if (found)
        {
            high = radius;
            if (max_refine < radius)
            {
                return radius;
            }
        }
        else
        {
            low = radius;
        }
        if (high - low < 11)
        {
            return high;
        }
        if (found)
        {
            radius /= 2;
            if (radius <= low)
            {
                radius = (high + low) / 2;
            }
        }
        else
        {
            radius *= 2;
            if (high <= radius)
            {
                radius = (high + low) / 2;
            }
        }
    }
    return 100;
}

// 0x00BCF720 (:862)
const lane_spline* lane_pathfind_get_closest_oriented_traffic_lane_slow(closest_lane_search_info& search_info)
{
    const int smallest = find_smallest_multiple_of_10_radius_containing_splines(search_info);
    float radius = (float)smallest;
    closest_spline_info spline_info(0.0f, search_info.search_pos, search_info.fvec);
    lane_spline* lanes[15];
    int num_lanes = 0;
    float queried_radius = 0.0f;
    for (int index = get_lane_search_index((float)smallest); index <= 5; index++)
    {
        // Original: the last step keeps the radius of the one before.
        if (index != 5)
        {
            radius = Lane_selection_info[index].max_dist;
        }
        if (radius != queried_radius)
        {
            traffic_lane_query query(search_info.search_pos, radius, 15, lanes);
            query.include_flags = search_info.include_flags;
            query.exclude_flags = search_info.exclude_flags;
            traffic_get_lanes_near_point(query);
            queried_radius = radius;
            num_lanes = query.num_found_lane_splines;
        }
        spline_info.min_dot = Lane_selection_info[index].min_dot;
        if (lane_pathfind_get_closest_oriented_lane(spline_info, lanes, num_lanes))
        {
            search_info.best_dist_sqr = spline_info.best_dist_sqr;
            return spline_info.lsp;
        }
    }
    return nullptr;
}

// 0x00BC3E10 (:922)
float lane_get_path_length(const lane_path_node* path, bool gps_search)
{
    float total = 0.0f;
    const lane_path_node* node = path;
    while (node != nullptr)
    {
        float length = node->lane->arc_length[0];
        if (gps_search && (node->lane->spline_flags & LANE_FLAG_GPS_HALF_COST) != 0)
        {
            length *= 0.5f;
        }
        total += length;
        node = node->next;
        if (node == path)
        {
            break;
        }
    }
    return total;
}

// 0x00BCD210 (:947)
float lane_get_path_length(const lane_path_node* path, const vector& start, const vector& end, bool gps_search)
{
    if (path == nullptr)
    {
        return 0.0f;
    }
    const float length = lane_get_path_length(path, gps_search);
    const float start_scale = gps_search && (path->lane->spline_flags & LANE_FLAG_GPS_HALF_COST) != 0 ? 0.5f : 1.0f;
    const float start_dist = vec_xz_dist(path->lane->knots.to_pointer()[0], start);
    const float end_scale = gps_search && (path->prev->lane->spline_flags & LANE_FLAG_GPS_HALF_COST) != 0 ? 0.5f : 1.0f;
    const float end_dist = vec_xz_dist(lane_last_knot(path->prev->lane), end);
    return length - start_dist * start_scale - end_dist * end_scale;
}

// 0x00BC3E90 (:959)
closest_spline_info::closest_spline_info(float _min_dot, const vector& _pos, const vector* _fvec)
    : min_dot(_min_dot), pos(_pos), fvec(_fvec), lsp(nullptr)
{
}
