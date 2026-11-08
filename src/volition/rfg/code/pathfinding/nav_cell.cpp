// Original: volition/rfg/code/pathfinding/nav_cell.cpp
//
// Nav cells (stub_impl.md S9): handles, classification of points and segments against cell edges, forcing points into a
// cell, line of sight walks across the mesh. rfg.exe's "Int3 critical function" prints (the release build of its asserts)
// are left out: they only log.

#include "volition/rfg/code/pathfinding/nav_cell.h"

#include <math.h>

#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding_data.h"
#include "volition/rfg/code/pathfinding/nav_cell_detour.h"
#include "volition/rfg/code/pathfinding/nav_heap.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/plane.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"

// bb_pathfinding.cpp; rfg.exe's whole-program build calls it across the two files.
bool search_for_nav_cell(nav_cell** closest_nav_cell, nav_cell* start_nav_cell, const vector& pos, nav_cell_traversal_rule traversal_rule); // 0x00B08AF0 (bb_pathfinding.cpp:1403)

// Magic number: the distance a fan or a segment end counts as on a vertex, squared.
static const float NAV_CELL_NEAR_SQ = 1.0000001e-06f;
// Magic number: below this a point is on the edge in the _vertices classifications.
static const float NAV_CELL_ON_EDGE_EPSILON = 0.0001f;
// Magic number: nearly degenerate intersection terms of nav_cell_classify_segment_vs_edge.
static const float NAV_CELL_SEGMENT_EPSILON = 0.001f;
// Magic number: how far outside its cell (squared) a point can be for nav_cell_check_curve still to search nearby.
static const float NAV_CELL_CURVE_HINT_MAX_DIST_SQ = 100.0f;
// Magic numbers: the angle step of a curve check, 30 degrees for vehicles and 11 for humans.
static const float NAV_CELL_CURVE_STEP_VEHICLE = 0.52358997f;
static const float NAV_CELL_CURVE_STEP_HUMAN = 0.191983f;
// Magic number: nav_cell_find_longest_line_of_sight_point keeps a fan direction only if it is this much longer.
static const float NAV_CELL_LOS_FAN_MIN_GAIN = 0.05f;
// Magic number: the points nav_cell_path_get_intermediate_points can follow.
static const int NAV_CELL_PATH_MAX_POINTS = 100;

static bb_pfg_layer* nav_cell_get_layer(const nav_cell* ncp, obj_zone* zone = nullptr)
{
    if (zone == nullptr) {
        zone = World.get_object_zone_by_index(ncp->zone_id);
    }
    return (ncp->flags & NAV_CELL_FLAG_VEHICLE) != 0 ? zone->zone_pfg.vehicle_pfg : zone->zone_pfg.pfg;
}

static nav_cell* nav_cell_adjacent(const nav_cell* ncp, int edge)
{
    return ncp->adjacent_cells[edge].to_pointer();
}

static void nav_cell_add_curve_node(path_node** curve_nodes, path_node* node)
{
    if (*curve_nodes == nullptr) {
        node->next = node;
        node->prev = node;
        *curve_nodes = node;
    } else {
        node->prev = (*curve_nodes)->prev;
        node->next = *curve_nodes;
        (*curve_nodes)->prev->next = node;
        (*curve_nodes)->prev = node;
    }
}

// 0x00AE9EB0 (.h:83)
nav_cell_los_test_point::nav_cell_los_test_point()
{
    pos.zero();
    nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
    cmp_hull = nullptr;
    vert = nullptr;
}

// 0x00AF3E20 (.h:91)
nav_cell_los_test_point::nav_cell_los_test_point(const vector& _pos, uint64_t _nav_cell_handle, unsigned int compound_hull_handle, int compound_hull_vert_index)
{
    pos = _pos;
    nav_cell_handle = _nav_cell_handle;
    cmp_hull = nav_cell_detour_get_compound_hull(compound_hull_handle);
    vert = cmp_hull != nullptr ? nav_cell_detour_get_compound_hull_vert_from_index(cmp_hull, compound_hull_vert_index) : nullptr;
}

// 0x00AECF80
nav_cell_los_test_point::nav_cell_los_test_point(const nav_cell_los_test_point& __that) = default;

// 0x005DCB50 (.h:117)
nav_cell_longest_los_input::nav_cell_longest_los_input()
{
    flags = {};
    start_pos.zero();
    start_nav_cell = nullptr;
    delta_2d.zero();
    num_fan_angle_increments = 0;
    max_fan_angle = 0.0f;
    max_visited_nav_cells = 0;
}

// 0x00AECFC0
nav_cell_longest_los_input::nav_cell_longest_los_input(const nav_cell_longest_los_input& __that) = default;

// 0x005DCB90 (.h:153)
nav_cell_longest_los_output::nav_cell_longest_los_output()
{
    result = NAV_CELL_LONGEST_LOS_RESULT_NONE;
    end_pos.zero();
    length = 0.0f;
    end_nav_cell = nullptr;
    obstruction.detour.cmp_hull = nullptr;
    obstruction.detour.entry_edge_index = 0;
    num_visited_nav_cells = 0;
    visited_nav_cells = nullptr;
}

// 0x00AED010
nav_cell_longest_los_output::nav_cell_longest_los_output(const nav_cell_longest_los_output& __that) = default;

// 0x005DCBD0 (.h:272)
bool nav_cell_is_disabled(const nav_cell* ncp)
{
    return (ncp->flags & NAV_CELL_FLAG_DISABLED) != 0;
}

// 0x005DCBE0 (.h:296)
void nav_cell_get_edge_midpoint(vector* edge_midpoint, const nav_cell* ncp, int edge_index)
{
    vector vert0, vert1;
    nav_cell_get_edge_verts(&vert0, &vert1, ncp, edge_index, nullptr);
    edge_midpoint->x = (vert1.x + vert0.x) * 0.5f;
    edge_midpoint->y = (vert1.y + vert0.y) * 0.5f;
    edge_midpoint->z = (vert0.z + vert1.z) * 0.5f;
}

// 0x00B01AF0 (:189) LTCG: ncp EDX, arrival_cell ESI
static int get_arrival_edge_index(const nav_cell* ncp, const nav_cell* arrival_cell)
{
    for (int i = 0; i < 3; i++) {
        if (nav_cell_adjacent(ncp, i) == arrival_cell) {
            return i;
        }
    }
    return -1;
}

// 0x00B01B20 (:210) LTCG: ncp EAX, los_obstruction_flags EDX, edge_num on the stack
static bool is_nav_cell_edge_passable(const nav_cell* ncp, int edge_num, unsigned int los_obstruction_flags)
{
    const nav_cell* adjacent = nav_cell_adjacent(ncp, edge_num);
    if (adjacent == nullptr) {
        return false;
    }
    if ((adjacent->flags & NAV_CELL_FLAG_DISABLED) == 0) {
        return true;
    }
    if ((adjacent->flags & NAV_CELL_FLAG_LIGHT) != 0 && (los_obstruction_flags & NAV_CELL_LOS_ALLOW_LIGHT) != 0) {
        return true;
    }
    return (los_obstruction_flags & NAV_CELL_LOS_ALLOW_DISABLED) != 0;
}

// The _vertices classifications order the edge by address, so both cells sharing an edge get the same numeric result.
static point_classification nav_cell_classify_xz_vs_edge_vertices(const vector* edge_vert0, const vector* edge_vert1, float x, float z)
{
    bool in_order = edge_vert0 <= edge_vert1;
    const vector* low = in_order ? edge_vert0 : edge_vert1;
    const vector* high = in_order ? edge_vert1 : edge_vert0;
    float side = (z - low->z) * (low->x - high->x) + (x - low->x) * (high->z - low->z);
    if (fabsf(side) < NAV_CELL_ON_EDGE_EPSILON) {
        side = 0.0f;
    }
    if (side == 0.0f) {
        return POINT_CLASS_ON;
    }
    if (in_order) {
        return side > 0.0f ? POINT_CLASS_RIGHT : POINT_CLASS_LEFT;
    }
    return side > 0.0f ? POINT_CLASS_LEFT : POINT_CLASS_RIGHT;
}

// 0x00B04DF0 (:261)
path_result nav_cell_classify_path_to_cell(const nav_cell* ncp, const vector& start_pos, const vector& end_pos, nav_cell** next_cell, int* edge_num, vector* ix_pos, unsigned int los_obstruction_flags)
{
    if (next_cell != nullptr) {
        *next_cell = nullptr;
    }
    if (edge_num != nullptr) {
        *edge_num = -1;
    }
    int num_inside = 0;
    vector verts[3];
    nav_cell_get_vertices(&verts[0], &verts[1], &verts[2], ncp);
    for (int edge = 0; edge < 3; edge++) {
        const vector* vert0 = &verts[edge];
        const vector* vert1 = &verts[(edge + 1) % 3];
        point_classification end_class = nav_cell_classify_xz_vs_edge_vertices(vert0, vert1, end_pos.x, end_pos.z);
        if (end_class == POINT_CLASS_ON || end_class == POINT_CLASS_RIGHT) {
            num_inside++;
            continue;
        }
        point_classification start_class = nav_cell_classify_xz_vs_edge_vertices(vert0, vert1, start_pos.x, start_pos.z);
        bool test_segment = start_class == POINT_CLASS_RIGHT;
        if (start_class == POINT_CLASS_ON && is_nav_cell_edge_passable(ncp, edge, los_obstruction_flags)) {
            test_segment = nav_cell_classify_path_to_cell(nav_cell_adjacent(ncp, edge), start_pos, end_pos, nullptr, nullptr, nullptr, 0) != PATH_RESULT_NO_RELATIONSHIP;
        }
        if (!test_segment) {
            continue;
        }
        line_classification line_class = nav_cell_classify_segment_vs_edge_vertices(ncp, vert0, vert1, start_pos, end_pos, ix_pos);
        if (line_class == LINE_CLASS_SEGMENTS_INTERSECT || line_class == LINE_CLASS_SEGMENT_BISECTS_EDGE) {
            if (next_cell != nullptr) {
                *next_cell = is_nav_cell_edge_passable(ncp, edge, los_obstruction_flags) ? nav_cell_adjacent(ncp, edge) : nullptr;
            }
            if (edge_num != nullptr) {
                *edge_num = edge;
            }
            return PATH_RESULT_EXITING_CELL;
        }
    }
    return num_inside == 3 ? PATH_RESULT_ENDING_CELL : PATH_RESULT_NO_RELATIONSHIP;
}

// 0x00B051D0 (:373) LTCG: ncp ECX, zone ESI, is_dynamic EDI
static bool get_nav_cell_handle_components_from_zone(unsigned int* z_handle, unsigned int* use_vehicle_pfg, unsigned int* cell_idx, unsigned int* is_dynamic, const nav_cell* ncp, const obj_zone* zone)
{
    int index = 0;
    nav_cell_in_zone_result result = nav_cell_does_zone_contain_nav_cell(ncp, zone, &index);
    if (result != NAV_CELL_IN_ZONE_PEDESTRIAN && result != NAV_CELL_IN_ZONE_VEHICLE) {
        return false;
    }
    *z_handle = zone->get_handle();
    *use_vehicle_pfg = result == NAV_CELL_IN_ZONE_VEHICLE ? 1 : 0;
    *cell_idx = (unsigned int)index;
    *is_dynamic = 0;
    return true;
}

// 0x00B05260 (:426) LTCG: edge_vert_index EAX
static bool is_exterior_navmesh_vert(const nav_cell* ncp, int edge_vert_index, unsigned char use_disabled_nav_cells)
{
    const vector* vert = nav_cell_get_vertex(ncp, edge_vert_index, nullptr);
    const nav_cell* cell = nav_cell_adjacent(ncp, edge_vert_index);
    while (cell != ncp) {
        if (cell == nullptr || (use_disabled_nav_cells == 0 && (cell->flags & NAV_CELL_FLAG_DISABLED) != 0)) {
            return true;
        }
        int i = 0;
        for (; i < 3; i++) {
            const vector* other = nav_cell_get_vertex(cell, i, nullptr);
            if (other == vert || (other->x == vert->x && other->y == vert->y && other->z == vert->z)) {
                break;
            }
        }
        if (i > 2) {
            return true;
        }
        cell = nav_cell_adjacent(cell, i);
    }
    return false;
}

// 0x00B09090 (:501)
static nav_cell* find_best_nav_cell_touching_start_vertex(const nav_cell* start_ncp, int vertex, const vector2& dest_pos_2d, unsigned int obstruction_flags, bool check_starting_cell)
{
    if (check_starting_cell && nav_cell_classify_point_vs_edge(start_ncp, vertex, dest_pos_2d) != POINT_CLASS_LEFT
        && nav_cell_classify_point_vs_edge(start_ncp, (vertex + 2) % 3, dest_pos_2d) != POINT_CLASS_LEFT) {
        return const_cast<nav_cell*>(start_ncp);
    }
    // Both ways around the vertex: first through the edge leaving it, then through the edge arriving at it.
    for (int direction = 0; direction < 2; direction++) {
        const vector* shared_vert = nav_cell_get_vertex(start_ncp, vertex, nullptr);
        int edge = ((direction != 0 ? 2 : 0) + vertex) % 3;
        const nav_cell* cell = start_ncp;
        while (is_nav_cell_edge_passable(cell, edge, obstruction_flags)) {
            nav_cell* next = nav_cell_adjacent(cell, edge);
            if (next == start_ncp) {
                return nullptr;
            }
            int vert_index = 0;
            for (; vert_index < 3; vert_index++) {
                if (*shared_vert == *nav_cell_get_vertex(next, vert_index, nullptr)) {
                    break;
                }
            }
            if (vert_index == 3) {
                // Magic number: cells of neighbouring zones share a vertex only within 0.1 m.
                float best_dist_sq = 0.010000001f;
                for (int i = 0; i < 3; i++) {
                    float dist_sq = vec_dist_squared(*shared_vert, *nav_cell_get_vertex(next, i, nullptr));
                    if (dist_sq < best_dist_sq) {
                        vert_index = i;
                        best_dist_sq = dist_sq;
                    }
                }
                if (vert_index == 3) {
                    break;
                }
                shared_vert = nav_cell_get_vertex(next, vert_index, nullptr);
            }
            int test_edge;
            if (nav_cell_adjacent(next, vert_index) == cell) {
                test_edge = vert_index;
                edge = (vert_index + 2) % 3;
            } else {
                edge = vert_index;
                test_edge = (vert_index + 2) % 3;
            }
            cell = next;
            if (nav_cell_classify_point_vs_edge(next, test_edge, dest_pos_2d) != POINT_CLASS_LEFT
                && nav_cell_classify_point_vs_edge(next, edge, dest_pos_2d) != POINT_CLASS_LEFT) {
                return next;
            }
        }
    }
    return nullptr;
}

static void los_output_end(nav_cell_longest_los_output& output, const nav_cell_longest_los_input& input, const vector& end_pos, nav_cell* cell, nav_cell_longest_los_result result, int edge)
{
    output.end_pos = end_pos;
    output.end_nav_cell = cell;
    output.length = vec_xz_dist(input.start_pos, output.end_pos);
    output.result = result;
    if (result == NAV_CELL_LONGEST_LOS_RESULT_EXITED_NAVMESH) {
        output.obstruction.navmesh.edge_index = edge;
    }
}

// 0x00B11BE0 (:596) LTCG: output ECX, input on the stack
static void find_longest_line_of_sight_point_internal(nav_cell_longest_los_output& output, const nav_cell_longest_los_input& input)
{
    // The fan calls run this several times on one output, so only a straight walk records its cells.
    int max_visited = input.max_fan_angle <= 0.0f || input.num_fan_angle_increments <= 0 ? input.max_visited_nav_cells : 0;
    output.end_pos = input.start_pos;
    output.end_nav_cell = input.start_nav_cell;
    output.length = 0.0f;
    output.result = NAV_CELL_LONGEST_LOS_RESULT_NONE;
    output.num_visited_nav_cells = 0;

    vector2 dest_2d(input.delta_2d.x + input.start_pos.x, input.delta_2d.y + input.start_pos.z);
    vector dest_pos;
    dest_pos.x = dest_2d.x;
    dest_pos.y = input.start_pos.y;
    dest_pos.z = dest_2d.y;
    vector dest_no_height;
    dest_no_height.x = dest_2d.x;
    dest_no_height.y = 0.0f;
    dest_no_height.z = dest_2d.y;

    unsigned int obstruction_flags = input.flags.use_disabled_nav_cells != 0 ? NAV_CELL_LOS_ALLOW_DISABLED : 0;
    nav_cell* prev_cell = nullptr;
    nav_cell* cell = input.start_nav_cell;
    float cur_x = input.start_pos.x;
    float cur_z = input.start_pos.z;
    // Original bug kept: each pass tests ix_2d before classify_segment_vs_edge_vertices writes this pass's intersection,
    // so it holds the previous pass's point of that edge (rfg.exe reads stale stack the first time; here zero).
    vector edge_ix[3] = {};

    for (;;) {
        bool hit_border = false;
        vector cur_input;
        cur_input.x = cur_x;
        cur_input.y = input.start_pos.y;
        cur_input.z = cur_z;
        vector cur_pos = nav_cell_force_point_to_cell(cell, cur_input);
        vector2 cur_2d(cur_pos.x, cur_pos.z);

        if (output.visited_nav_cells != nullptr && output.num_visited_nav_cells < max_visited) {
            output.visited_nav_cells[output.num_visited_nav_cells] = cell;
        }
        output.num_visited_nav_cells++;

        if (input.flags.ignore_detour_hulls == 0
            && nav_cell_detour_get_closest_compound_hull_intersection(&output.end_pos, &output.obstruction.detour.cmp_hull, &output.obstruction.detour.entry_edge_index, cell, input.start_pos, dest_pos)) {
            output.end_nav_cell = cell;
            output.length = vec_xz_dist(input.start_pos, output.end_pos);
            output.result = NAV_CELL_LONGEST_LOS_RESULT_CONTACTED_DETOUR_HULL;
            return;
        }
        if (nav_cell_is_point_in_cell_column(cell, dest_2d)) {
            los_output_end(output, input, nav_cell_force_point_to_cell(cell, dest_no_height), cell, NAV_CELL_LONGEST_LOS_RESULT_REACHED_DESTINATION, 0);
            return;
        }

        int on_edge = -1;
        int on_vertex = -1;
        vector verts[3];
        nav_cell_get_vertices(&verts[0], &verts[1], &verts[2], cell);
        nav_cell* crossed_to = nullptr;
        for (int edge = 0; edge < 3; edge++) {
            int next_edge = (edge + 1) % 3;
            if (nav_cell_classify_point_vs_edge_vertices(cell, &verts[edge], &verts[next_edge], cur_2d) == POINT_CLASS_ON) {
                if (on_vertex < 0) {
                    if (on_edge < 0) {
                        on_edge = edge;
                    } else if (on_edge == 0) {
                        // On two edges: the vertex they share.
                        on_vertex = edge == 1 ? 1 : (edge == 2 ? 0 : -1);
                        on_edge = -1;
                    } else if (on_edge == 1) {
                        on_vertex = edge == 2 ? 2 : -1;
                        on_edge = -1;
                    }
                }
                continue;
            }
            if (prev_cell != nullptr && nav_cell_adjacent(cell, edge) == prev_cell) {
                continue;
            }
            vector2 edge_start(verts[edge].x, verts[edge].z);
            vector2 edge_end(verts[next_edge].x, verts[next_edge].z);
            vector2 ix_2d(edge_ix[edge].x, edge_ix[edge].z);
            bool straight_crossing = true;
            line_classification line_class = nav_cell_classify_segment_vs_edge_vertices(cell, &verts[edge], &verts[next_edge], cur_pos, dest_pos, &edge_ix[edge]);
            if (line_class == LINE_CLASS_LINES_INTERSECT) {
                if (vec_xz_dist_squared(dest_pos, edge_ix[edge]) < NAV_CELL_NEAR_SQ
                    && (vec2_dist_squared(edge_start, ix_2d) < NAV_CELL_NEAR_SQ || vec2_dist_squared(edge_end, ix_2d) < NAV_CELL_NEAR_SQ)) {
                    los_output_end(output, input, nav_cell_force_point_to_cell(cell, dest_no_height), cell, NAV_CELL_LONGEST_LOS_RESULT_REACHED_DESTINATION, 0);
                    return;
                }
                continue;
            }
            if (line_class != LINE_CLASS_SEGMENTS_INTERSECT) {
                if (line_class != LINE_CLASS_EDGE_BISECTS_SEGMENT
                    || (vec2_dist_squared(edge_start, ix_2d) >= NAV_CELL_NEAR_SQ && vec2_dist_squared(edge_end, ix_2d) >= NAV_CELL_NEAR_SQ)) {
                    continue;
                }
                if (vec_xz_dist_squared(dest_pos, edge_ix[edge]) < NAV_CELL_NEAR_SQ) {
                    los_output_end(output, input, nav_cell_force_point_to_cell(cell, dest_no_height), cell, NAV_CELL_LONGEST_LOS_RESULT_REACHED_DESTINATION, 0);
                    return;
                }
                straight_crossing = false;
            }
            if (is_nav_cell_edge_passable(cell, edge, obstruction_flags)) {
                crossed_to = nav_cell_adjacent(cell, edge);
                cur_x = edge_ix[edge].x;
                cur_z = edge_ix[edge].z;
                break;
            }
            los_output_end(output, input, nav_cell_force_point_to_cell(cell, edge_ix[edge]), cell, NAV_CELL_LONGEST_LOS_RESULT_EXITED_NAVMESH, edge);
            if (straight_crossing) {
                return;
            }
            hit_border = true;
        }
        if (crossed_to != nullptr) {
            prev_cell = cell;
            cell = crossed_to;
            continue;
        }

        vector cur_no_height;
        cur_no_height.x = cur_2d.x;
        cur_no_height.y = 0.0f;
        cur_no_height.z = cur_2d.y;

        if (on_edge >= 0) {
            vector edge_vert0, edge_vert1;
            nav_cell_get_edge_verts(&edge_vert0, &edge_vert1, cell, on_edge, nullptr);
            vector2 edge0_2d(edge_vert0.x, edge_vert0.z);
            vector2 edge1_2d(edge_vert1.x, edge_vert1.z);
            float closest_param;
            if (vec2_find_closest_dist_squared_to_linesegment(closest_param, dest_2d, edge0_2d, edge1_2d) < NAV_CELL_NEAR_SQ) {
                los_output_end(output, input, nav_cell_force_point_to_cell(cell, dest_no_height), cell, NAV_CELL_LONGEST_LOS_RESULT_REACHED_DESTINATION, 0);
                return;
            }
            if (!is_nav_cell_edge_passable(cell, on_edge, obstruction_flags)) {
                los_output_end(output, input, nav_cell_force_point_to_cell(cell, cur_pos), cell, NAV_CELL_LONGEST_LOS_RESULT_EXITED_NAVMESH, on_edge);
                return;
            }
            nav_cell* next = nav_cell_adjacent(cell, on_edge);
            if (next == prev_cell) {
                los_output_end(output, input, nav_cell_force_point_to_cell(cell, cur_no_height), cell, NAV_CELL_LONGEST_LOS_RESULT_EXITED_NAVMESH, 0);
                return;
            }
            cur_x = cur_2d.x;
            cur_z = cur_2d.y;
            prev_cell = cell;
            cell = next;
            continue;
        }

        if (on_vertex >= 0) {
            nav_cell* next = find_best_nav_cell_touching_start_vertex(cell, on_vertex, dest_2d, obstruction_flags, false);
            if (next == nullptr || next == prev_cell) {
                los_output_end(output, input, *nav_cell_get_vertex(cell, on_vertex, nullptr), cell, NAV_CELL_LONGEST_LOS_RESULT_EXITED_NAVMESH, on_vertex);
                return;
            }
            cur_x = cur_2d.x;
            cur_z = cur_2d.y;
            prev_cell = cell;
            cell = next;
            continue;
        }

        if (hit_border) {
            return;
        }

        // On no edge and crossing none: if two of the three intersections are nearest one vertex, go on from that vertex.
        int nearest_count[3] = { 0, 0, 0 };
        for (int ix = 0; ix < 3; ix++) {
            int nearest = -1;
            float nearest_dist_sq = -1.0f;
            for (int i = 0; i < 3; i++) {
                float dist_sq = vec_xz_dist_squared(edge_ix[ix], *nav_cell_get_vertex(cell, i, nullptr));
                if (dist_sq < nearest_dist_sq || nearest < 0) {
                    nearest = i;
                    nearest_dist_sq = dist_sq;
                }
            }
            nearest_count[nearest]++;
        }
        int vert_index = 0;
        while (vert_index < 3 && nearest_count[vert_index] < 2) {
            vert_index++;
        }
        if (vert_index > 2) {
            los_output_end(output, input, nav_cell_force_point_to_cell(cell, cur_no_height), cell, NAV_CELL_LONGEST_LOS_RESULT_EXITED_NAVMESH, 0);
            return;
        }
        int edge = (vert_index + 2) % 3;
        if (prev_cell != nullptr && nav_cell_adjacent(cell, edge) == prev_cell) {
            edge = vert_index;
        }
        if (is_exterior_navmesh_vert(cell, vert_index, input.flags.use_disabled_nav_cells) || !is_nav_cell_edge_passable(cell, edge, obstruction_flags)) {
            los_output_end(output, input, *nav_cell_get_vertex(cell, vert_index, nullptr), cell, NAV_CELL_LONGEST_LOS_RESULT_EXITED_NAVMESH, on_vertex);
            return;
        }
        const vector* vert = nav_cell_get_vertex(cell, vert_index, nullptr);
        cur_x = vert->x;
        cur_z = vert->z;
        prev_cell = cell;
        cell = nav_cell_adjacent(cell, edge);
    }
}

// 0x00AEC570 (:1060)
void nav_cell_system_init()
{
}

// 0x00AEC580 (:1071)
void nav_cell_level_init_pre()
{
}

// 0x00AEC590 (:1103)
void nav_cell_get_handle_components(unsigned int* z_handle, unsigned int* use_vehicle_pfg, unsigned int* cell_idx, unsigned int* dynamic_pfg, uint64_t handle)
{
    // dynamic_pfg is not written: rfg.exe has no dynamic graphs left.
    *z_handle = (unsigned int)(handle >> 16);
    *use_vehicle_pfg = (unsigned int)(handle >> 15) & 1;
    *cell_idx = (unsigned int)handle & 0x7FFF;
}

// 0x00B01B60 (:1117)
nav_cell* nav_cell_from_handle(uint64_t handle)
{
    if (handle == 0xFFFFFFFFFFFFFFFFull) {
        return nullptr;
    }
    unsigned int zone_handle = (unsigned int)(handle >> 16);
    if (zone_handle == 0xFFFFFFFF) {
        return nullptr;
    }
    object* objp = World.object_from_handle(zone_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_ZONE) {
        return nullptr;
    }
    obj_zone* zone = (obj_zone*)objp;
    bb_pfg_layer* layer = (((unsigned int)handle >> 15) & 1) == 0 ? zone->zone_pfg.pfg : zone->zone_pfg.vehicle_pfg;
    if (layer == nullptr) {
        return nullptr;
    }
    return layer->nav_cells.to_pointer() + ((unsigned int)handle & 0x7FFF);
}

// 0x00B053B0 (:1153)
uint64_t nav_cell_get_handle(const nav_cell* ncp)
{
    unsigned int z_handle = 0xFFFFFFFF;
    unsigned int use_vehicle_pfg = 1;
    unsigned int cell_idx = 0x7FFF;
    unsigned int is_dynamic = 0;
    if (ncp == nullptr) {
        return 0xFFFFFFFFFFFFFFFFull;
    }
    obj_zone* zone = World.get_object_zone_by_index(ncp->zone_id);
    bool found = zone != nullptr && get_nav_cell_handle_components_from_zone(&z_handle, &use_vehicle_pfg, &cell_idx, &is_dynamic, ncp, zone);
    if (!found) {
        int num_zones = World.get_num_objects<obj_zone>();
        int i = 0;
        for (; i < num_zones; i++) {
            zone = World.get_object<obj_zone>(i);
            if (zone->zone_pfg.pfg != nullptr && get_nav_cell_handle_components_from_zone(&z_handle, &use_vehicle_pfg, &cell_idx, &is_dynamic, ncp, zone)) {
                break;
            }
        }
        if (i >= num_zones) {
            return 0xFFFFFFFFFFFFFFFFull;
        }
    }
    return ((uint64_t)z_handle << 16) | ((use_vehicle_pfg & 1) << 15) | (cell_idx & 0x7FFF);
}

// 0x00B01BE0 (:1227)
nav_cell_in_zone_result nav_cell_does_zone_contain_nav_cell(const nav_cell* ncp, const obj_zone* zone, int* nav_cell_index)
{
    const bb_pfg_layer* layer = zone->zone_pfg.pfg;
    if (layer != nullptr) {
        int index = (int)(((const char*)ncp - (const char*)layer->nav_cells.to_pointer()) >> 5);
        if (index >= 0 && index < (int)layer->num_nav_cells) {
            if (nav_cell_index != nullptr) {
                *nav_cell_index = index;
            }
            return NAV_CELL_IN_ZONE_PEDESTRIAN;
        }
    }
    layer = zone->zone_pfg.vehicle_pfg;
    if (layer != nullptr) {
        int index = (int)(((const char*)ncp - (const char*)layer->nav_cells.to_pointer()) >> 5);
        if (index >= 0 && index < (int)layer->num_nav_cells) {
            if (nav_cell_index != nullptr) {
                *nav_cell_index = index;
            }
            return NAV_CELL_IN_ZONE_VEHICLE;
        }
    }
    return NAV_CELL_NOT_IN_ZONE;
}

// 0x00B01C60 (:1263)
obj_zone* nav_cell_get_zone_containing_nav_cell(const nav_cell* ncp)
{
    int num_zones = World.get_num_objects<obj_zone>();
    for (int i = 0; i < num_zones; i++) {
        obj_zone* zone = World.get_object<obj_zone>(i);
        // Original quirk kept: a zone without a human graph is not searched for vehicle cells either.
        if (zone->zone_pfg.pfg != nullptr && nav_cell_does_zone_contain_nav_cell(ncp, zone, nullptr) != NAV_CELL_NOT_IN_ZONE) {
            return zone;
        }
    }
    return nullptr;
}

// 0x00B09340 (:1291)
bool nav_cell_is_point_in_cell_column(const nav_cell* ncp, const vector& pos)
{
    if (ncp == nullptr) {
        return false;
    }
    return nav_cell_is_point_in_cell_column(ncp, vector2(pos.x, pos.z));
}

// 0x00B09420 (:1331)
bool nav_cell_is_point_in_cell_column(const nav_cell* ncp, const vector2& pos)
{
    vector2 xz_min, xz_max;
    nav_cell_get_xz_bounding_box(&xz_min, &xz_max, ncp);
    if (pos.x < xz_min.x || pos.y < xz_min.y || xz_max.x < pos.x || xz_max.y < pos.y) {
        return false;
    }
    for (int edge = 0; edge < 3; edge++) {
        if (nav_cell_classify_point_vs_edge(ncp, edge, pos) == POINT_CLASS_LEFT) {
            return false;
        }
    }
    return true;
}

// 0x00B054E0 (:1364)
float nav_cell_get_closest_xz_dist_squared_to_point(const nav_cell* ncp, const vector& pos)
{
    vector2 pos_2d(pos.x, pos.z);
    float best = -1.0f;
    for (int edge = 0; edge < 3; edge++) {
        const vector* vert0 = nav_cell_get_vertex(ncp, edge, nullptr);
        const vector* vert1 = nav_cell_get_vertex(ncp, (edge + 1) % 3, nullptr);
        vector2 closest;
        float dist_sq = vec2_find_closest_dist_squared_to_linesegment(closest, pos_2d, vector2(vert0->x, vert0->z), vector2(vert1->x, vert1->z));
        if (best < 0.0f || dist_sq < best) {
            best = dist_sq;
        }
    }
    return best;
}

// The shared part of nav_cell_force_point_to_cell: from the nearest point on a violated edge, walks towards the centroid
// until the point is inside.
static vector nav_cell_force_point_xz(const nav_cell* ncp, const vector& pos)
{
    vector2 pos_2d(pos.x, pos.z);
    int best_edge = -1;
    float best_dist_sq = -1.0f;
    vector best_point;
    for (int edge = 0; edge < 3; edge++) {
        if (nav_cell_classify_point_vs_edge(ncp, edge, pos_2d) == POINT_CLASS_RIGHT) {
            continue;
        }
        const vector* vert0 = nav_cell_get_vertex(ncp, edge, nullptr);
        const vector* vert1 = nav_cell_get_vertex(ncp, (edge + 1) % 3, nullptr);
        vector2 closest;
        vec2_find_closest_point_on_linesegment(closest, pos_2d, vector2(vert0->x, vert0->z), vector2(vert1->x, vert1->z));
        float dist_sq = vec2_dist_squared(pos_2d, closest);
        if (dist_sq < best_dist_sq || best_dist_sq < 0.0f) {
            best_point.x = closest.x;
            best_point.y = 0.0f;
            best_point.z = closest.y;
            best_dist_sq = dist_sq;
            best_edge = edge;
        }
    }
    if (best_edge < 0) {
        return pos;
    }
    vector centroid;
    nav_cell_get_centroid(&centroid, ncp);
    vector to_centroid;
    to_centroid.x = centroid.x - best_point.x;
    to_centroid.y = 0.0f;
    to_centroid.z = centroid.z - best_point.z;
    float dist = to_centroid.mag_and_normalize_safe();
    // Magic number: 1 cm steps.
    float along = 0.0f;
    vector out;
    for (;;) {
        along += 0.01f;
        if (dist < along) {
            return centroid;
        }
        out.x = to_centroid.x * along + best_point.x;
        out.y = along * to_centroid.y + best_point.y;
        out.z = to_centroid.z * along + best_point.z;
        if (nav_cell_is_point_in_cell_column(ncp, out)) {
            return out;
        }
    }
}

// 0x00B094D0 (:1394)
vector nav_cell_force_point_to_cell(const nav_cell* ncp, const vector& pos)
{
    vector out = nav_cell_force_point_xz(ncp, pos);
    havok_terrain_height(out.y, out.x, out.z, false, nullptr);
    return out;
}

// 0x00B09830 (:1477)
vector nav_cell_force_point_to_cell_no_height(const nav_cell* ncp, const vector& pos)
{
    vector out = nav_cell_force_point_xz(ncp, pos);
    out.y = 0.0f;
    return out;
}

// 0x00B09B70 (:1567)
vector nav_cell_force_point_to_cell(uint64_t nav_cell_handle, const vector& pos)
{
    nav_cell* ncp = nav_cell_from_handle(nav_cell_handle);
    if (ncp != nullptr) {
        return nav_cell_force_point_to_cell(ncp, pos);
    }
    return ZERO_VECTOR;
}

// 0x00B09BD0 (:1583)
vector nav_cell_force_point_to_cell_no_height(uint64_t nav_cell_handle, const vector& pos)
{
    nav_cell* ncp = nav_cell_from_handle(nav_cell_handle);
    if (ncp != nullptr) {
        return nav_cell_force_point_to_cell_no_height(ncp, pos);
    }
    return ZERO_VECTOR;
}

// 0x00B05620 (:1597)
void nav_cell_force_point_to_cell_plane(vector& pos, const nav_cell* ncp)
{
    plane cell_plane;
    nav_cell_get_cell_plane(&cell_plane, ncp);
    pos.y = -((pos.z * cell_plane.normal.z + pos.x * cell_plane.normal.x + cell_plane.offset) / cell_plane.normal.y);
}

// 0x00AEC5D0 (:1617)
float nav_cell_get_traversal_cost(const nav_cell* start_ncp, const vector& start_pos, const nav_cell* end_ncp, const vector& end_pos)
{
    return vec_dist(start_pos, end_pos);
}

// 0x00B056A0 (:1679)
void nav_cell_get_random_point(vector& point, const nav_cell* ncp)
{
    if (ncp == nullptr) {
        return;
    }
    vector vert0, vert1, vert2;
    nav_cell_get_vertices(&vert0, &vert1, &vert2, ncp);
    vector edge1 = vert1 - vert0;
    vector edge2 = vert2 - vert0;
    if (edge1.is_null() || edge2.is_null()) {
        return;
    }
    float len1 = edge1.mag_and_normalize();
    float len2 = edge2.mag_and_normalize();
    float rand1 = fl_rand();
    float rand2 = fl_rand();
    // A point in the parallelogram of the edges; one outside the triangle mirrors back into it.
    if (rand2 + rand1 <= 1.0f) {
        point.x = edge1.x * rand1 * len1 + vert0.x + edge2.x * rand2 * len2;
        point.y = edge1.y * rand1 * len1 + vert0.y + edge2.y * rand2 * len2;
        point.z = edge1.z * rand1 * len1 + vert0.z + edge2.z * rand2 * len2;
    } else {
        float mirror1 = 1.0f - rand1;
        float mirror2 = 1.0f - rand2;
        point.x = mirror1 * edge1.x * len1 + vert0.x + edge2.x * mirror2 * len2;
        point.y = edge1.y * mirror1 * len1 + vert0.y + edge2.y * mirror2 * len2;
        point.z = edge1.z * mirror1 * len1 + vert0.z + edge2.z * mirror2 * len2;
    }
}

// 0x00B05BD0 (:1742)
point_classification nav_cell_classify_point_vs_edge(const nav_cell* ncp, int edge_num, const vector2& pos)
{
    const vector* low = nav_cell_get_vertex(ncp, edge_num, nullptr);
    const vector* high = nav_cell_get_vertex(ncp, (edge_num + 1) % 3, nullptr);
    bool swapped = high < low;
    if (swapped) {
        const vector* tmp = high;
        high = low;
        low = tmp;
    }
    float side = (high->z - low->z) * (pos.x - low->x) + (low->x - high->x) * (pos.y - low->z);
    if (side == 0.0f || side != side) {
        return POINT_CLASS_ON;
    }
    if (swapped) {
        return side > 0.0f ? POINT_CLASS_LEFT : POINT_CLASS_RIGHT;
    }
    return side > 0.0f ? POINT_CLASS_RIGHT : POINT_CLASS_LEFT;
}

// 0x00AEC5F0 (:1799)
point_classification nav_cell_classify_point_vs_edge_vertices(const nav_cell* ncp, const vector* edge_vert0, const vector* edge_vert1, const vector2& pos)
{
    return nav_cell_classify_xz_vs_edge_vertices(edge_vert0, edge_vert1, pos.x, pos.y);
}

// 0x00B05D80 (:1845)
point_classification nav_cell_classify_point_vs_edge(const nav_cell* ncp, int edge_num, const vector& pos)
{
    return nav_cell_classify_point_vs_edge(ncp, edge_num, vector2(pos.x, pos.z));
}

// 0x00AEC6E0 (:1859)
point_classification nav_cell_classify_point_vs_edge_vertices(const nav_cell* ncp, const vector* edge_vert0, const vector* edge_vert1, const vector& pos)
{
    return nav_cell_classify_xz_vs_edge_vertices(edge_vert0, edge_vert1, pos.x, pos.z);
}

static line_classification nav_cell_line_result(float segment_t, float edge_t, bool segment_outside)
{
    if (0.0f <= segment_t && segment_t <= 1.0f && !segment_outside) {
        if (0.0f <= edge_t && edge_t <= 1.0f) {
            return LINE_CLASS_SEGMENTS_INTERSECT;
        }
        return LINE_CLASS_EDGE_BISECTS_SEGMENT;
    }
    if (edge_t < 0.0f || 1.0f < edge_t) {
        return LINE_CLASS_LINES_INTERSECT;
    }
    return LINE_CLASS_SEGMENT_BISECTS_EDGE;
}

// 0x00AEC7D0 (:1875)
line_classification nav_cell_classify_segment_vs_edge(const vector& s1, const vector& s2, const vector& e1, const vector& e2, vector* ix_pos)
{
    float start_dz = s1.z - e1.z;
    float edge_dx = e2.x - e1.x;
    float start_dx = s1.x - e1.x;
    float edge_dz = e2.z - e1.z;
    float seg_dx = s2.x - s1.x;
    float seg_dz = s2.z - s1.z;
    float numerator = edge_dx * start_dz - edge_dz * start_dx;
    float denominator = seg_dx * edge_dz - seg_dz * edge_dx;
    if (denominator == 0.0f) {
        return numerator == 0.0f ? LINE_CLASS_COLLINEAR : LINE_CLASS_PARALLEL;
    }
    float segment_t = numerator / denominator;
    float edge_t = (seg_dx * start_dz - seg_dz * start_dx) / denominator;
    if (ix_pos != nullptr) {
        ix_pos->x = s1.x + seg_dx * segment_t;
        ix_pos->y = s1.y + (s2.y - s1.y) * segment_t;
        ix_pos->z = s1.z + (s2.z - s1.z) * segment_t;
    }
    return nav_cell_line_result(segment_t, edge_t, false);
}

// 0x00B05DC0 (:1928)
line_classification nav_cell_classify_segment_vs_edge(const nav_cell* ncp, int edge_num, const vector& s1, const vector& s2, vector* ix_pos)
{
    const vector* vert0 = nav_cell_get_vertex(ncp, edge_num, nullptr);
    const vector* vert1 = nav_cell_get_vertex(ncp, (edge_num + 1) % 3, nullptr);
    float start_dz = s1.z - vert0->z;
    float edge_dx = vert1->x - vert0->x;
    float start_dx = s1.x - vert0->x;
    float edge_dz = vert1->z - vert0->z;
    float seg_dx = s2.x - s1.x;
    float seg_dz = s2.z - s1.z;
    float numerator = edge_dx * start_dz - edge_dz * start_dx;
    float denominator = seg_dx * edge_dz - seg_dz * edge_dx;
    if (denominator == 0.0f) {
        return numerator == 0.0f ? LINE_CLASS_COLLINEAR : LINE_CLASS_PARALLEL;
    }
    float edge_t = (seg_dx * start_dz - seg_dz * start_dx) / denominator;
    float segment_t = numerator / denominator;
    bool segment_outside = false;
    if (fabsf(numerator) < NAV_CELL_SEGMENT_EPSILON || fabsf(denominator) < NAV_CELL_SEGMENT_EPSILON || fabsf(segment_t) < NAV_CELL_SEGMENT_EPSILON || fabsf(segment_t - 1.0f) < NAV_CELL_SEGMENT_EPSILON) {
        point_classification start_class = nav_cell_classify_point_vs_edge(ncp, edge_num, vector2(s1.x, s1.z));
        point_classification end_class = nav_cell_classify_point_vs_edge(ncp, edge_num, vector2(s2.x, s2.z));
        if (start_class == POINT_CLASS_ON) {
            segment_t = 0.0f;
        } else if (end_class == POINT_CLASS_ON) {
            segment_t = 1.0f;
        } else if (start_class == end_class) {
            segment_outside = true;
        } else if (segment_t < 0.0f) {
            segment_t = 0.0f;
        } else if (1.0f < segment_t) {
            segment_t = 1.0f;
        }
    }
    if (ix_pos != nullptr) {
        ix_pos->x = s1.x + (s2.x - s1.x) * segment_t;
        ix_pos->y = s1.y + (s2.y - s1.y) * segment_t;
        ix_pos->z = s1.z + (s2.z - s1.z) * segment_t;
    }
    return nav_cell_line_result(segment_t, edge_t, segment_outside);
}

// 0x00AECA50 (:2012)
line_classification nav_cell_classify_segment_vs_edge_vertices(const nav_cell* ncp, const vector* edge_vert0, const vector* edge_vert1, const vector& s1, const vector& s2, vector* ix_pos)
{
    float start_dz = s1.z - edge_vert0->z;
    float edge_dx = edge_vert1->x - edge_vert0->x;
    float start_dx = s1.x - edge_vert0->x;
    float edge_dz = edge_vert1->z - edge_vert0->z;
    float seg_dx = s2.x - s1.x;
    float seg_dz = s2.z - s1.z;
    float numerator = edge_dx * start_dz - edge_dz * start_dx;
    float denominator = seg_dx * edge_dz - seg_dz * edge_dx;
    if (denominator == 0.0f) {
        return numerator == 0.0f ? LINE_CLASS_COLLINEAR : LINE_CLASS_PARALLEL;
    }
    float segment_t = numerator / denominator;
    float edge_t = (seg_dx * start_dz - seg_dz * start_dx) / denominator;
    bool segment_outside = false;
    if (fabsf(numerator) < NAV_CELL_SEGMENT_EPSILON || fabsf(denominator) < NAV_CELL_SEGMENT_EPSILON || fabsf(segment_t) < NAV_CELL_SEGMENT_EPSILON || fabsf(segment_t - 1.0f) < NAV_CELL_SEGMENT_EPSILON) {
        point_classification start_class = nav_cell_classify_xz_vs_edge_vertices(edge_vert0, edge_vert1, s1.x, s1.z);
        point_classification end_class = nav_cell_classify_xz_vs_edge_vertices(edge_vert0, edge_vert1, s2.x, s2.z);
        if (start_class == POINT_CLASS_ON) {
            segment_t = 0.0f;
        } else if (end_class == POINT_CLASS_ON) {
            segment_t = 1.0f;
        } else if (start_class == end_class) {
            segment_outside = true;
        } else if (segment_t < 0.0f) {
            segment_t = 0.0f;
        } else if (1.0f < segment_t) {
            segment_t = 1.0f;
        }
    }
    if (ix_pos != nullptr) {
        ix_pos->x = s1.x + seg_dx * segment_t;
        ix_pos->y = s1.y + (s2.y - s1.y) * segment_t;
        ix_pos->z = s1.z + (s2.z - s1.z) * segment_t;
    }
    return nav_cell_line_result(segment_t, edge_t, segment_outside);
}

// 0x00B061F0 (:2093)
line_classification nav_cell_classify_segment_vs_edge(const nav_cell* ncp, int edge_num, const vector2& s1, const vector2& s2, vector2* ix_pos)
{
    const vector* vert0 = nav_cell_get_vertex(ncp, edge_num, nullptr);
    const vector* vert1 = nav_cell_get_vertex(ncp, (edge_num + 1) % 3, nullptr);
    float start_dz = s1.y - vert0->z;
    float start_dx = s1.x - vert0->x;
    float edge_dx = vert1->x - vert0->x;
    float edge_dz = vert1->z - vert0->z;
    float seg_dx = s2.x - s1.x;
    float seg_dz = s2.y - s1.y;
    float numerator = edge_dx * start_dz - edge_dz * start_dx;
    float denominator = seg_dx * edge_dz - seg_dz * edge_dx;
    if (denominator == 0.0f) {
        return numerator != 0.0f ? LINE_CLASS_PARALLEL : LINE_CLASS_COLLINEAR;
    }
    float edge_t = (seg_dx * start_dz - seg_dz * start_dx) / denominator;
    float segment_t = numerator / denominator;
    bool degenerate = fabsf(numerator) < NAV_CELL_SEGMENT_EPSILON || fabsf(denominator) < NAV_CELL_SEGMENT_EPSILON;
    if (degenerate || fabsf(segment_t) < NAV_CELL_SEGMENT_EPSILON) {
        point_classification start_class = nav_cell_classify_point_vs_edge(ncp, edge_num, s1);
        if (start_class == POINT_CLASS_ON || (start_class == POINT_CLASS_LEFT && 0.0f <= segment_t) || (start_class == POINT_CLASS_RIGHT && segment_t < 0.0f)) {
            segment_t = 0.0f;
        }
    }
    if (degenerate || fabsf(segment_t - 1.0f) < NAV_CELL_SEGMENT_EPSILON) {
        point_classification end_class = nav_cell_classify_point_vs_edge(ncp, edge_num, s2);
        if (end_class == POINT_CLASS_ON || (end_class == POINT_CLASS_LEFT && 0.0f <= segment_t - 1.0f) || (end_class == POINT_CLASS_RIGHT && segment_t - 1.0f < 0.0f)) {
            segment_t = 1.0f;
        }
    }
    if (ix_pos != nullptr) {
        ix_pos->x = s1.x + (s2.x - s1.x) * segment_t;
        ix_pos->y = s1.y + (s2.y - s1.y) * segment_t;
    }
    return nav_cell_line_result(segment_t, edge_t, false);
}

// 0x00B09C30 (:2202)
bool nav_cell_path_get_intermediate_points(uint64_t start_nav_cell_handle, const vector& start_pos, uint64_t end_nav_cell_handle, const vector& end_pos, const float* intermediate_fractions, int num_intermediate_points, vector* intermediate_points, uint64_t* intermediate_handles, nav_cell_traversal_rule traversal_rule)
{
    static vector target_points[NAV_CELL_PATH_MAX_POINTS];
    float distances[NAV_CELL_PATH_MAX_POINTS];
    uint64_t handles[NAV_CELL_PATH_MAX_POINTS];
    float total = 0.0f;
    int num_points;

    target_points[0] = start_pos;
    distances[0] = 0.0f;
    handles[0] = start_nav_cell_handle;
    if (start_nav_cell_handle == end_nav_cell_handle) {
        target_points[1] = end_pos;
        handles[1] = end_nav_cell_handle;
        total = vec_dist(end_pos, start_pos) + 0.0f;
        distances[1] = total;
        num_points = 2;
    } else {
        nav_cell* cell = nav_cell_from_handle(start_nav_cell_handle);
        if (cell == nullptr || nav_cell_from_handle(end_nav_cell_handle) == nullptr) {
            return false;
        }
        unsigned int los_flags = traversal_rule == NAV_CELL_TRAVERSAL_ALL ? NAV_CELL_LOS_ALLOW_DISABLED : (traversal_rule == NAV_CELL_TRAVERSAL_ENABLED_PLUS_LIGHT ? NAV_CELL_LOS_ALLOW_LIGHT : 0);
        int i = 1;
        num_points = 1;
        do {
            nav_cell* next = nullptr;
            vector ix;
            path_result result = nav_cell_classify_path_to_cell(cell, start_pos, end_pos, &next, nullptr, &ix, los_flags);
            if (result == PATH_RESULT_ENDING_CELL) {
                if (i >= NAV_CELL_PATH_MAX_POINTS) {
                    return false;
                }
                target_points[i] = end_pos;
                handles[i] = end_nav_cell_handle;
                total = vec_dist(end_pos, target_points[i - 1]) + total;
                distances[i] = total;
                num_points = i + 1;
                break;
            }
            if (result != PATH_RESULT_EXITING_CELL || i >= NAV_CELL_PATH_MAX_POINTS) {
                return false;
            }
            target_points[i] = ix;
            handles[i] = nav_cell_get_handle(next);
            total = vec_dist(ix, target_points[i - 1]) + total;
            distances[i] = total;
            num_points = i + 1;
            i++;
            cell = next;
        } while (cell != nullptr);
    }

    for (int point = 0; point < num_intermediate_points; point++) {
        float target = intermediate_fractions[point] * total;
        int i = 1;
        for (; i < num_points; i++) {
            if (target < distances[i]) {
                // Magic number: rfg.exe's floor on a segment's length.
                float segment = distances[i] - distances[i - 1];
                if (segment - 0.001f < 0.0f) {
                    segment = 0.001f;
                }
                float fraction = (distances[i] - target) / segment;
                if (0.0f <= fraction - 1.0f) {
                    fraction = 1.0f;
                }
                float rest = 1.0f - fraction;
                intermediate_points[point].x = target_points[i - 1].x * fraction + target_points[i].x * rest;
                intermediate_points[point].y = target_points[i - 1].y * fraction + target_points[i].y * rest;
                intermediate_points[point].z = target_points[i - 1].z * fraction + target_points[i].z * rest;
                intermediate_handles[point] = handles[i - 1];
                break;
            }
        }
        if (i == num_points) {
            return false;
        }
        intermediate_points[point] = nav_cell_force_point_to_cell(nav_cell_from_handle(intermediate_handles[point]), intermediate_points[point]);
    }
    return true;
}

// nav_cell_test_line_of_sight and nav_cell_test_line_of_sight_no_height have the same body in rfg.exe: both step with
// the no_height force.
static bool nav_cell_walk_line_of_sight(const nav_cell_los_test_point& start_point, const nav_cell_los_test_point& end_point, unsigned int los_obstruction_flags)
{
    if (start_point.pos.x == end_point.pos.x && start_point.pos.y == end_point.pos.y && start_point.pos.z == end_point.pos.z) {
        return true;
    }
    bool disabled_blocks = (los_obstruction_flags & NAV_CELL_LOS_ALLOW_DISABLED) == 0;
    bool test_detour = (los_obstruction_flags & NAV_CELL_LOS_IGNORE_DETOUR) == 0;
    nav_cell_los_test_point current = start_point;
    nav_cell* cell = nav_cell_from_handle(start_point.nav_cell_handle);
    if (cell == nullptr || (cell->flags & NAV_CELL_FLAG_BLOCKED) != 0) {
        return false;
    }
    const nav_cell* end_cell = nav_cell_from_handle(end_point.nav_cell_handle);
    if (end_cell == nullptr || (end_cell->flags & NAV_CELL_FLAG_BLOCKED) != 0) {
        return false;
    }
    while ((cell->flags & NAV_CELL_FLAG_BLOCKED) == 0) {
        if ((cell->flags & NAV_CELL_FLAG_DISABLED) != 0) {
            if ((cell->flags & NAV_CELL_FLAG_LIGHT) == 0) {
                if (disabled_blocks) {
                    return false;
                }
            } else if ((los_obstruction_flags & NAV_CELL_LOS_ALLOW_LIGHT) == 0 && !disabled_blocks) {
                // Original bug kept: a light cell blocks only when disabled cells are allowed.
                return false;
            }
        }
        nav_cell* next = nullptr;
        vector ix;
        path_result result = nav_cell_classify_path_to_cell(cell, start_point.pos, end_point.pos, &next, nullptr, &ix, los_obstruction_flags);
        if (result == PATH_RESULT_NO_RELATIONSHIP) {
            return false;
        }
        if (result == PATH_RESULT_ENDING_CELL) {
            if (test_detour && !nav_cell_detour_test_line_of_sight(current, end_point)) {
                return false;
            }
            if (cell == end_cell) {
                return true;
            }
            // Original bug kept: the loop steps by a nav_cell from the first adjacent cell, not through adjacent_cells.
            const nav_cell* adjacent = nav_cell_adjacent(cell, 0);
            for (int i = 0; i < 3; i++, adjacent++) {
                if (adjacent == end_cell) {
                    return true;
                }
            }
            for (int i = 0; i < 3; i++) {
                const vector* vert = nav_cell_get_vertex(cell, i, nullptr);
                // rfg.exe then searches the cell's vertices for this same vertex, which always succeeds.
                if (end_point.pos.x == vert->x && end_point.pos.z == vert->z) {
                    return true;
                }
            }
            return false;
        }
        if (result == PATH_RESULT_EXITING_CELL) {
            if (next == nullptr) {
                return false;
            }
            nav_cell_los_test_point crossing;
            crossing.pos = nav_cell_force_point_to_cell_no_height(next, ix);
            crossing.nav_cell_handle = nav_cell_get_handle(next);
            if (test_detour && !nav_cell_detour_test_line_of_sight(current, crossing)) {
                return false;
            }
            current = crossing;
        }
        cell = next;
        if (cell == nullptr) {
            return false;
        }
    }
    return false;
}

// 0x00B12680 (:2350)
bool nav_cell_test_line_of_sight(const nav_cell_los_test_point& start_point, const nav_cell_los_test_point& end_point, unsigned int los_obstruction_flags)
{
    return nav_cell_walk_line_of_sight(start_point, end_point, los_obstruction_flags);
}

// 0x00B129C0 (:2473)
bool nav_cell_test_line_of_sight_no_height(const nav_cell_los_test_point& start_point, const nav_cell_los_test_point& end_point, unsigned int los_obstruction_flags)
{
    return nav_cell_walk_line_of_sight(start_point, end_point, los_obstruction_flags);
}

// 0x00B12D00 (:2607)
bool nav_cell_test_line_of_sight(uint64_t start_nav_cell_handle, const vector& start_pos, uint64_t end_nav_cell_handle, const vector& end_pos, unsigned int los_obstruction_flags)
{
    nav_cell_los_test_point start_point;
    start_point.pos = start_pos;
    start_point.nav_cell_handle = start_nav_cell_handle;
    nav_cell_los_test_point end_point;
    end_point.pos = end_pos;
    end_point.nav_cell_handle = end_nav_cell_handle;
    return nav_cell_test_line_of_sight(start_point, end_point, los_obstruction_flags);
}

// 0x00B12D80 (:2616)
bool nav_cell_test_line_of_sight_no_height(uint64_t start_nav_cell_handle, const vector& start_pos, uint64_t end_nav_cell_handle, const vector& end_pos, unsigned int los_obstruction_flags)
{
    nav_cell_los_test_point start_point;
    start_point.pos = start_pos;
    start_point.nav_cell_handle = start_nav_cell_handle;
    nav_cell_los_test_point end_point;
    end_point.pos = end_pos;
    end_point.nav_cell_handle = end_nav_cell_handle;
    return nav_cell_test_line_of_sight_no_height(start_point, end_point, los_obstruction_flags);
}

// The vertex of the cell furthest along the line, the start of the edge walks below.
static int nav_cell_furthest_vert_along(const nav_cell* cell, const vector2& start, float dx, float dz)
{
    int best_vert = -1;
    float best_dot = 0.0f;
    for (int i = 0; i < 3; i++) {
        const vector* vert = nav_cell_get_vertex(cell, i, nullptr);
        float dot = (vert->z - start.y) * dz + (vert->x - start.x) * dx;
        if (best_vert == -1 || best_dot < dot) {
            best_vert = i;
            best_dot = dot;
        }
    }
    return best_vert;
}

// 0x00B0A110 (:2740)
bool nav_cell_find_longest_line_of_sight_no_detour(vector& out_pos, const nav_cell*& out_nav_cell, const vector& start_pos, const nav_cell* start_nav_cell, const vector& end_pos, bool force_to_cell, int* final_out_edge)
{
    vector2 start_2d(start_pos.x, start_pos.z);
    vector2 end_2d(end_pos.x, end_pos.z);
    float dx = end_2d.x - start_2d.x;
    float dz = end_2d.y - start_2d.y;
    if (final_out_edge != nullptr) {
        *final_out_edge = -1;
    }
    float length_sq = dx * dx + dz * dz;
    if (length_sq <= 0.0f) {
        out_pos = start_pos;
        out_nav_cell = start_nav_cell;
        return true;
    }
    int edge = nav_cell_furthest_vert_along(start_nav_cell, start_2d, dx, dz);
    if (edge < 0) {
        return false;
    }
    const nav_cell* cell = start_nav_cell;
    for (;;) {
        const vector* vert0 = nav_cell_get_vertex(cell, edge, nullptr);
        if (dx * (vert0->z - start_2d.y) - dz * (vert0->x - start_2d.x) <= 0.0f) {
            edge = (edge + 2) % 3;
        }
        vert0 = nav_cell_get_vertex(cell, edge, nullptr);
        const vector* vert1 = nav_cell_get_vertex(cell, (edge + 1) % 3, nullptr);
        vector2 ix;
        if (!ix2d_line_line(ix, start_2d, end_2d, vector2(vert0->x, vert0->z), vector2(vert1->x, vert1->z))) {
            vector midpoint;
            nav_cell_get_edge_midpoint(&midpoint, cell, edge);
            ix.x = midpoint.x;
            ix.y = midpoint.z;
        }
        bool reached = length_sq <= (ix.y - start_2d.y) * dz + (ix.x - start_2d.x) * dx;
        nav_cell* next = nullptr;
        if (reached) {
            out_pos = end_pos;
            out_nav_cell = cell;
        } else {
            next = nav_cell_adjacent(cell, edge);
            if (next == nullptr || (next->flags & NAV_CELL_FLAG_DISABLED) != 0) {
                out_pos.x = ix.x;
                out_pos.y = (vert1->y + vert0->y) * 0.5f;
                out_pos.z = ix.y;
                out_nav_cell = cell;
                if (final_out_edge != nullptr) {
                    *final_out_edge = edge;
                }
            }
        }
        if (reached || next == nullptr || (next->flags & NAV_CELL_FLAG_DISABLED) != 0) {
            if (force_to_cell) {
                out_pos = nav_cell_force_point_to_cell(out_nav_cell, out_pos);
            }
            return reached;
        }
        int arrival = get_arrival_edge_index(next, cell);
        if (arrival < 0) {
            return false;
        }
        edge = (arrival + 2) % 3;
        cell = next;
    }
}

// 0x00B0A560 (:2862)
bool nav_cell_find_first_valid_cell(vector& out_pos, const nav_cell*& out_nav_cell, const vector& start_pos, const nav_cell* start_nav_cell, const vector& end_pos, bool force_to_cell, int* enter_edge)
{
    vector2 start_2d(start_pos.x, start_pos.z);
    vector2 end_2d(end_pos.x, end_pos.z);
    float dx = end_2d.x - start_2d.x;
    float dz = end_2d.y - start_2d.y;
    if (enter_edge != nullptr) {
        *enter_edge = -1;
    }
    float length_sq = dx * dx + dz * dz;
    if (length_sq <= 0.0f) {
        out_pos = start_pos;
        out_nav_cell = start_nav_cell;
        return true;
    }
    int edge = nav_cell_furthest_vert_along(start_nav_cell, start_2d, dx, dz);
    if (edge < 0) {
        return false;
    }
    const nav_cell* cell = start_nav_cell;
    for (;;) {
        const vector* vert0 = nav_cell_get_vertex(cell, edge, nullptr);
        if (dx * (vert0->z - start_2d.y) - dz * (vert0->x - start_2d.x) <= 0.0f) {
            edge = (edge + 2) % 3;
        }
        vert0 = nav_cell_get_vertex(cell, edge, nullptr);
        const vector* vert1 = nav_cell_get_vertex(cell, (edge + 1) % 3, nullptr);
        vector2 ix;
        if (!ix2d_line_line(ix, start_2d, end_2d, vector2(vert0->x, vert0->z), vector2(vert1->x, vert1->z))) {
            vector midpoint;
            nav_cell_get_edge_midpoint(&midpoint, cell, edge);
            ix.x = midpoint.x;
            ix.y = midpoint.z;
        }
        if (length_sq <= (ix.y - start_2d.y) * dz + (ix.x - start_2d.x) * dx) {
            return false;
        }
        const nav_cell* next = nav_cell_adjacent(cell, edge);
        if (next == nullptr) {
            return false;
        }
        int arrival = get_arrival_edge_index(next, cell);
        if (arrival < 0) {
            return false;
        }
        edge = (arrival + 2) % 3;
        cell = next;
        if ((next->flags & (NAV_CELL_FLAG_DISABLED | NAV_CELL_FLAG_LIGHT)) == 0) {
            out_pos.x = ix.x;
            out_pos.y = (vert1->y + vert0->y) * 0.5f;
            out_pos.z = ix.y;
            out_nav_cell = next;
            if (force_to_cell) {
                out_pos = nav_cell_force_point_to_cell(next, out_pos);
            }
            if (enter_edge != nullptr) {
                *enter_edge = arrival;
            }
            return true;
        }
    }
}

// 0x00B12E00 (:2973)
void nav_cell_find_longest_line_of_sight_point(nav_cell_longest_los_output& output, const nav_cell_longest_los_input& input)
{
    if (input.flags.ignore_detour_hulls == 0 && input.flags.check_start_in_detour_hull != 0) {
        compound_hull* hull = nav_cell_detour_is_point_in_compound_hull(input.start_pos, input.start_nav_cell);
        if (hull != nullptr) {
            output.result = NAV_CELL_LONGEST_LOS_RESULT_CONTACTED_DETOUR_HULL;
            output.end_pos = input.start_pos;
            output.end_nav_cell = input.start_nav_cell;
            output.length = 0.0f;
            output.obstruction.detour.cmp_hull = hull;
            output.obstruction.detour.entry_edge_index = -1;
            output.num_visited_nav_cells = 1;
            if (output.visited_nav_cells == nullptr || input.max_visited_nav_cells < 1) {
                return;
            }
            output.visited_nav_cells[0] = input.start_nav_cell;
            return;
        }
    }
    if (input.max_fan_angle <= 0.0f || input.num_fan_angle_increments < 1) {
        find_longest_line_of_sight_point_internal(output, input);
        return;
    }
    nav_cell_longest_los_input fan_input(input);
    nav_cell_longest_los_output fan_output(output);
    vector straight_delta;
    straight_delta.x = input.delta_2d.x;
    straight_delta.y = 0.0f;
    straight_delta.z = input.delta_2d.y;
    find_longest_line_of_sight_point_internal(output, input);
    for (int i = 1; i <= input.num_fan_angle_increments; i++) {
        float angle = ((float)i / (float)input.num_fan_angle_increments) * input.max_fan_angle;
        vector rotated;
        vec_rot_point_around_line(rotated, straight_delta, angle, ZERO_VECTOR, UP_VECTOR);
        fan_input.delta_2d.x = rotated.x;
        fan_input.delta_2d.y = rotated.z;
        find_longest_line_of_sight_point_internal(fan_output, fan_input);
        if (output.length + NAV_CELL_LOS_FAN_MIN_GAIN < fan_output.length) {
            output = fan_output;
        }
        vec_rot_point_around_line(rotated, straight_delta, -angle, ZERO_VECTOR, UP_VECTOR);
        fan_input.delta_2d.x = rotated.x;
        fan_input.delta_2d.y = rotated.z;
        find_longest_line_of_sight_point_internal(fan_output, fan_input);
        if (output.length + NAV_CELL_LOS_FAN_MIN_GAIN < fan_output.length) {
            output = fan_output;
        }
    }
}

// 0x00B06670 (:3040)
bool nav_cell_query_for_path(nav_cell* ncp, nav_cell* arrival_cell, const vector& arrival_pos, float arrival_cost, const vector& goal_pos, nav_heap* nhp, bool through_mover, unsigned short which_navpoint)
{
    nav_heap_cell_data* data = nhp->get_cell_data(ncp);
    if (data != nullptr) {
        if (data->is_open && arrival_cost < data->arrival_cost) {
            data->arrival_cost = arrival_cost;
            data->arrival_pos = arrival_pos;
            nav_cell_force_point_to_cell_plane(data->arrival_pos, ncp);
            data->est_cost_to_goal = vec_dist(data->arrival_pos, goal_pos);
            // Magic number: arrival edges 3 and up name a mover's navpoint rather than an edge.
            data->arrival_edge = through_mover ? (char)which_navpoint + 3 : (char)get_arrival_edge_index(ncp, arrival_cell);
            nhp->percolate(data->heap_index);
        }
        return true;
    }
    vector pos = arrival_pos;
    nav_cell_force_point_to_cell_plane(pos, ncp);
    float est_cost = vec_dist(pos, goal_pos);
    char arrival_edge;
    if (through_mover) {
        arrival_edge = (char)which_navpoint + 3;
    } else if (arrival_cell == nullptr) {
        arrival_edge = -1;
    } else {
        arrival_edge = (char)get_arrival_edge_index(ncp, arrival_cell);
    }
    return nhp->insert_cell(ncp, arrival_cost, est_cost, arrival_edge, pos);
}

// 0x00AED060 (:3160)
bool nav_cell_ix_linesegment_linesegment(vector2* ix_pos, const vector2& line0_vert0, const vector2& line0_vert1, const vector2& line1_vert0, const vector2& line1_vert1)
{
    float line0_dx = line0_vert1.x - line0_vert0.x;
    float line0_dy = line0_vert1.y - line0_vert0.y;
    float line1_dx = line1_vert1.x - line1_vert0.x;
    float line1_dy = line1_vert1.y - line1_vert0.y;
    float denominator = line1_dy * line0_dx - line0_dy * line1_dx;
    if (denominator == 0.0f) {
        return false;
    }
    float offset_x = line0_vert0.x - line1_vert0.x;
    float offset_y = line0_vert0.y - line1_vert0.y;
    float t0 = (offset_y * line1_dx - line1_dy * offset_x) / denominator;
    if (t0 < 0.0f || 1.0f < t0) {
        return false;
    }
    float t1 = (offset_y * line0_dx - line0_dy * offset_x) / denominator;
    if (t1 < 0.0f || 1.0f < t1) {
        return false;
    }
    ix_pos->x = line0_vert0.x + line0_dx * t0;
    ix_pos->y = line0_vert0.y + line0_dy * t0;
    return true;
}

// 0x00B01D10 (:3196)
void nav_cell_get_centroid(vector* centroid, const nav_cell* ncp)
{
    vector vert0, vert1, vert2;
    nav_cell_get_vertices(&vert0, &vert1, &vert2, ncp);
    // Magic number: the plain average, written as a division by 3 as in rfg.exe.
    centroid->x = (vert2.x + vert0.x + vert1.x) / 3.0f;
    centroid->y = (vert2.y + vert1.y + vert0.y) / 3.0f;
    centroid->z = (vert2.z + vert1.z + vert0.z) / 3.0f;
}

// 0x00B01E40 (:3221)
void nav_cell_get_cell_plane(plane* cell_plane, const nav_cell* ncp)
{
    const vector* vertices = nav_cell_get_layer(ncp)->vertices.to_pointer();
    cell_plane->set(vertices[ncp->vertices[0]], vertices[ncp->vertices[1]], vertices[ncp->vertices[2]]);
}

// 0x00B06890 (:3237)
void nav_cell_get_edge_dir(vector* edge_dir, const nav_cell* ncp, int edge_index)
{
    const vector* vert0 = nav_cell_get_vertex(ncp, edge_index, nullptr);
    const vector* vert1 = nav_cell_get_vertex(ncp, (edge_index + 1) % 3, nullptr);
    edge_dir->x = vert1->x - vert0->x;
    edge_dir->y = vert1->y - vert0->y;
    edge_dir->z = vert1->z - vert0->z;
    edge_dir->normalize();
}

// 0x00AED250 (:3252)
unsigned int nav_cell_get_los_obstruction_flags_from_path_calc_flags(const pf_calc_flags& flags)
{
    unsigned int result = 0;
    if (flags.allow_land_to_water != 0) {
        result = NAV_CELL_LOS_LAND_TO_WATER;
    }
    if (flags.disallow_climbing != 0) {
        result |= NAV_CELL_LOS_NO_CLIMBING;
    }
    if (flags.ignore_detour_hulls != 0) {
        result |= NAV_CELL_LOS_IGNORE_DETOUR;
    }
    if (flags.use_disabled_cells != 0) {
        result |= NAV_CELL_LOS_ALLOW_DISABLED;
    }
    if (flags.use_light_structure_disabled_cells != 0) {
        result |= NAV_CELL_LOS_ALLOW_LIGHT;
    }
    return result;
}

// 0x00B06990 (:3282)
nav_cell* find_first_valid_nav_cell_along_path(const vector& start_pos, const vector& end_pos, nav_cell* starting_nav_cell, vector& ending_position)
{
    vector ix = ZERO_VECTOR;
    nav_cell* cell = starting_nav_cell;
    for (;;) {
        vector verts[3];
        nav_cell_get_vertices(&verts[0], &verts[1], &verts[2], cell);
        int crossed = -1;
        for (int edge = 0; edge < 3; edge++) {
            const vector* vert0 = &verts[edge];
            const vector* vert1 = &verts[(edge + 1) % 3];
            point_classification end_class = nav_cell_classify_xz_vs_edge_vertices(vert0, vert1, end_pos.x, end_pos.z);
            if (end_class != POINT_CLASS_LEFT) {
                continue;
            }
            point_classification start_class = nav_cell_classify_xz_vs_edge_vertices(vert0, vert1, start_pos.x, start_pos.z);
            bool test_segment = start_class == POINT_CLASS_RIGHT;
            if (start_class == POINT_CLASS_ON) {
                nav_cell* adjacent = nav_cell_adjacent(cell, edge);
                test_segment = adjacent != nullptr && nav_cell_classify_path_to_cell(adjacent, start_pos, end_pos, nullptr, nullptr, nullptr, 0) != PATH_RESULT_NO_RELATIONSHIP;
            }
            if (test_segment) {
                line_classification line_class = nav_cell_classify_segment_vs_edge_vertices(cell, vert0, vert1, start_pos, end_pos, &ix);
                if (line_class == LINE_CLASS_SEGMENTS_INTERSECT || line_class == LINE_CLASS_SEGMENT_BISECTS_EDGE) {
                    crossed = edge;
                    break;
                }
            }
        }
        if (crossed < 0) {
            return nullptr;
        }
        cell = nav_cell_adjacent(cell, crossed);
        if (cell == nullptr) {
            return nullptr;
        }
        if ((cell->flags & NAV_CELL_FLAG_DISABLED) == 0) {
            ending_position = ix;
            return cell;
        }
    }
}

// 0x00B17110 (:3332)
bool nav_cell_check_curve(path_node** curve_nodes, uint64_t start_nav_cell_handle, const vector& start_pos, const vector& end_pos, float radius, const vector2& center, float angleStart, float angleFinal, const vector& final_pos, float curveLength, bool turnRight, bool vehicle_pfg)
{
    vector adjusted;
    nav_cell* cell = nav_cell_from_handle(start_nav_cell_handle);
    if (cell == nullptr) {
        cell = bb_pf_get_nav_cell(start_pos, vehicle_pfg, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, &adjusted);
    } else if (nav_cell_is_point_in_cell_column(cell, start_pos)) {
        if ((cell->flags & NAV_CELL_FLAG_DISABLED) != 0) {
            return false;
        }
    } else {
        nav_cell* closest = nullptr;
        if (NAV_CELL_CURVE_HINT_MAX_DIST_SQ < nav_cell_get_closest_xz_dist_squared_to_point(cell, start_pos)
            || !search_for_nav_cell(&closest, cell, start_pos, NAV_CELL_TRAVERSAL_ENABLED)) {
            cell = bb_pf_get_nav_cell(start_pos, vehicle_pfg, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, &adjusted);
        } else {
            cell = closest;
        }
    }
    if (cell == nullptr) {
        return false;
    }
    nav_cell_force_point_to_cell(cell, start_pos);
    if (!nav_cell_is_point_in_cell_column(cell, start_pos) || nav_cell_detour_is_point_in_compound_hull(start_pos, cell) != nullptr) {
        return false;
    }

    *curve_nodes = nullptr;
    float step = vehicle_pfg ? NAV_CELL_CURVE_STEP_VEHICLE : NAV_CELL_CURVE_STEP_HUMAN;
    vector radial;
    radial.x = start_pos.x - center.x;
    radial.y = 0.0f;
    radial.z = start_pos.z - center.y;
    radial.normalize();
    float up_y = turnRight ? 1.0f : -1.0f;
    vector tangent;
    tangent.x = up_y * radial.z - radial.y * 0.0f;
    tangent.y = radial.x * 0.0f - radial.z * 0.0f;
    tangent.z = radial.y * 0.0f - up_y * radial.x;

    nav_cell* next = nullptr;
    int crossed_edge = 0;
    vector ix;
    vector point = start_pos;
    bool done = false;
    int steps = 0;
    do {
        steps++;
        float angle = (float)steps * step;
        vector previous = point;
        if (angle * radius <= curveLength) {
            float sine = (float)sin((double)angle) * radius;
            float cosine = (float)cos((double)angle) * radius;
            point.x = cosine * radial.x + center.x + sine * tangent.x;
            point.y = start_pos.y;
            point.z = radial.z * cosine + center.y + sine * tangent.z;
        } else {
            point = end_pos;
            done = true;
        }
        path_result result = nav_cell_classify_path_to_cell(cell, previous, point, &next, &crossed_edge, &ix, 0);
        while (result == PATH_RESULT_EXITING_CELL) {
            if (next == nullptr || (next->flags & NAV_CELL_FLAG_DISABLED) != 0) {
                bb_pf_free_path(curve_nodes);
                return false;
            }
            cell = next;
            result = nav_cell_classify_path_to_cell(cell, previous, point, &next, &crossed_edge, &ix, 0);
        }
        if ((!vehicle_pfg && nav_cell_detour_is_point_in_compound_hull(point, cell) != nullptr) || result != PATH_RESULT_ENDING_CELL) {
            bb_pf_free_path(curve_nodes);
            return false;
        }
        if (vehicle_pfg) {
            path_node* node = bb_pf_get_free_path_node(point, nav_cell_get_handle(cell), 0xFFFFFFFF);
            if (node == nullptr) {
                bb_pf_free_path(curve_nodes);
                return false;
            }
            nav_cell_add_curve_node(curve_nodes, node);
        }
    } while (!done);

    if (vehicle_pfg) {
        path_node* node = bb_pf_get_free_path_node(end_pos, nav_cell_get_handle(cell), 0xFFFFFFFF);
        if (node == nullptr) {
            bb_pf_free_path(curve_nodes);
            return false;
        }
        nav_cell_add_curve_node(curve_nodes, node);
    }

    // From the curve's end straight to the final point, else to 3/4, 1/2 or 1/4 of the way there from the start.
    path_result result = nav_cell_classify_path_to_cell(cell, end_pos, final_pos, &next, &crossed_edge, &ix, 0);
    while (result == PATH_RESULT_EXITING_CELL) {
        if (next == nullptr || (next->flags & NAV_CELL_FLAG_DISABLED) != 0) {
            break;
        }
        result = nav_cell_classify_path_to_cell(next, end_pos, final_pos, &next, &crossed_edge, &ix, 0);
    }
    if (result == PATH_RESULT_ENDING_CELL) {
        return true;
    }
    vector to_final = final_pos - start_pos;
    float dist_to_final = to_final.mag_and_normalize();
    for (int quarter = 3; quarter > 0; quarter--) {
        float dist = (float)quarter * 0.25f * dist_to_final;
        vector target;
        target.x = to_final.x * dist + start_pos.x;
        target.y = to_final.y * dist + start_pos.y;
        target.z = to_final.z * dist + start_pos.z;
        nav_cell* last = cell;
        result = nav_cell_classify_path_to_cell(cell, end_pos, target, &next, &crossed_edge, &ix, 0);
        bool blocked = false;
        while (result == PATH_RESULT_EXITING_CELL) {
            if (next == nullptr || (next->flags & NAV_CELL_FLAG_DISABLED) != 0) {
                blocked = true;
                break;
            }
            last = next;
            result = nav_cell_classify_path_to_cell(last, end_pos, target, &next, &crossed_edge, &ix, 0);
        }
        if (!blocked && result == PATH_RESULT_ENDING_CELL) {
            uint64_t target_handle = 0xFFFFFFFFFFFFFFFFull;
            bb_pf_get_nav_cell_from_hint(&target, nav_cell_get_handle(last), target, vehicle_pfg, &target_handle, false, false, NAV_CELL_TRAVERSAL_ENABLED);
            path_node* node = bb_pf_get_free_path_node(target, target_handle, 0xFFFFFFFF);
            if (node == nullptr) {
                break;
            }
            nav_cell_add_curve_node(curve_nodes, node);
            return true;
        }
    }
    bb_pf_free_path(curve_nodes);
    return false;
}

// 0x00B01EA0 (:3564)
void nav_cell_get_edge_verts(vector* edge_vert0, vector* edge_vert1, const nav_cell* ncp, int edge_index, obj_zone* zone)
{
    const vector* vertices = nav_cell_get_layer(ncp, zone)->vertices.to_pointer();
    *edge_vert0 = vertices[ncp->vertices[edge_index]];
    *edge_vert1 = vertices[ncp->vertices[(edge_index + 1) % 3]];
}

// 0x00B01F30 (:3581)
bool nav_cell_get_all_verts(const nav_cell* ncp, vector& vert_a, vector& vert_b, vector& vert_c)
{
    if (ncp == nullptr) {
        return false;
    }
    nav_cell_get_vertices(&vert_a, &vert_b, &vert_c, ncp);
    return true;
}

// 0x00B01FD0 (:3606)
void nav_cell_get_xz_bounding_box(vector2* xz_min, vector2* xz_max, const nav_cell* ncp)
{
    const vector* vertices = nav_cell_get_layer(ncp)->vertices.to_pointer();
    const vector& vert0 = vertices[ncp->vertices[0]];
    const vector& vert1 = vertices[ncp->vertices[1]];
    const vector& vert2 = vertices[ncp->vertices[2]];
    float min_x = vert0.x - vert1.x < 0.0f ? vert0.x : vert1.x;
    float min_z = vert0.z - vert1.z < 0.0f ? vert0.z : vert1.z;
    xz_min->x = 0.0f <= min_x - vert2.x ? vert2.x : min_x;
    xz_min->y = 0.0f <= min_z - vert2.z ? vert2.z : min_z;
    float max_x = 0.0f <= vert0.x - vert1.x ? vert0.x : vert1.x;
    float max_z = 0.0f <= vert0.z - vert1.z ? vert0.z : vert1.z;
    xz_max->x = max_x - vert2.x < 0.0f ? vert2.x : max_x;
    xz_max->y = max_z - vert2.z < 0.0f ? vert2.z : max_z;
}

// 0x00B02190 (:3634)
vector* nav_cell_get_vertex(const nav_cell* ncp, int vertex_index, obj_zone* zone)
{
    return nav_cell_get_layer(ncp, zone)->vertices.to_pointer() + ncp->vertices[vertex_index];
}

// 0x00B021E0 (:3647)
void nav_cell_get_vertices(vector* vertex0, vector* vertex1, vector* vertex2, const nav_cell* ncp)
{
    const vector* vertices = nav_cell_get_layer(ncp)->vertices.to_pointer();
    *vertex0 = vertices[ncp->vertices[0]];
    *vertex1 = vertices[ncp->vertices[1]];
    *vertex2 = vertices[ncp->vertices[2]];
}

// 0x00B02270 (:3662)
float nav_cell_get_area_2d(const nav_cell* ncp)
{
    vector vert0, vert1, vert2;
    nav_cell_get_vertices(&vert0, &vert1, &vert2, ncp);
    return fabsf((vert0.z - vert1.z) * vert2.x + vert1.x * (vert2.z - vert0.z) + vert0.x * (vert1.z - vert2.z)) * 0.5f;
}
