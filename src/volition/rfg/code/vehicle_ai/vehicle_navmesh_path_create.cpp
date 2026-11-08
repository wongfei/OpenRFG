// Original: volition/rfg/code/vehicle_ai/vehicle_navmesh_path_create.cpp
//
// Turns a nav mesh path into a ring of lane spline nodes the route code follows: the path points are the control points
// of a cubic B-spline, which knot insertion converts to Bezier segments.

#include "volition/rfg/code/vehicle_ai/vehicle_navmesh_path_create.h"

#include <math.h>
#include <string.h>

#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/rfg/code/vehicle_ai/vehicle_navmesh_common.h"
#include "volition/vlib/code/math/spline.h"

static const int MAX_NAVMESH_LS_NODES = 512;
static const int MAX_BSPLINE_CONTROL_POINTS = 40; // the Bezier output of 10 path points fits in 40 vectors
static const int MAX_FLATTEN_POINTS = 10; // 4 path points, and at most 2 inserted on each of their 3 segments
static const float MIN_POINT_DIST_SQ = 1.0000001e-06f;

static navmesh_lane_spline_node* Free_navmesh_ls_nodes; // 0x0387111C
static navmesh_lane_spline_node Navmesh_ls_nodes[MAX_NAVMESH_LS_NODES]; // 0x0387B2D0
// Every node owns two knots and two handles of each kind.
static vector Ls_base_knots[MAX_NAVMESH_LS_NODES * 2]; // 0x0387F2E0
static vector Ls_base_in_handle[MAX_NAVMESH_LS_NODES * 2]; // 0x038751B0
static vector Ls_base_out_handle[MAX_NAVMESH_LS_NODES * 2]; // 0x038782D0

// 0x00BCA290
lane_spline_base::lane_spline_base()
{
}

// 0x00BCEA90
navmesh_lane_spline_node::navmesh_lane_spline_node()
{
}

// 0x00BC7500
smooth_path_point_info::smooth_path_point_info()
{
}

// Unlinks a node from wherever it is and appends it to the free ring.
static void ls_list_remove(navmesh_lane_spline_node** list, navmesh_lane_spline_node* node)
{
    if (*list == node)
    {
        if (node->next == node)
        {
            node->next = NULL;
            node->prev = NULL;
            *list = NULL;
            return;
        }
        *list = node->next;
    }
    if (node->prev != NULL)
    {
        node->prev->next = node->next;
    }
    if (node->next != NULL)
    {
        node->next->prev = node->prev;
    }
    node->next = NULL;
    node->prev = NULL;
}

static void ls_list_append(navmesh_lane_spline_node** list, navmesh_lane_spline_node* node)
{
    if (*list == NULL)
    {
        node->next = node;
        node->prev = node;
        *list = node;
        return;
    }
    node->prev = (*list)->prev;
    node->next = *list;
    (*list)->prev->next = node;
    (*list)->prev = node;
}

// 0x00BC5B10 (:136)
navmesh_lane_spline_node* get_free_ls_node()
{
    navmesh_lane_spline_node* node = Free_navmesh_ls_nodes;
    if (node != NULL)
    {
        ls_list_remove(&Free_navmesh_ls_nodes, node);
    }
    return node;
}

// 0x00BC5B60 (:165): LTCG, the insert index in EAX. Inserts 1 or 2 points on a segment of at least 5 m; 2 from 30 m.
static int add_internal_points_to_path_segment(int insert_index, smooth_path_point_info* points, float dist, int num_points, nav_cell_traversal_rule traversal_rule)
{
    if (dist < 5.0f)
    {
        return 0;
    }
    float fractions[2] = { 0.5f, 0.0f };
    int num_inserted = 1;
    if (dist >= 30.0f)
    {
        fractions[0] = 0.3333f;
        fractions[1] = 0.6667f;
        num_inserted = 2;
    }
    vector new_points[2];
    uint64_t new_handles[2];
    if (num_points < 3)
    {
        // Original quirk kept: with two points in the list the insert index can only be 1, so this is its segment.
        const vector delta = points[1].pos - points[0].pos;
        new_points[0] = points[0].pos + delta * fractions[0];
        new_points[1] = points[0].pos + delta * fractions[1];
        new_handles[0] = 0xFFFFFFFFFFFFFFFFULL;
        new_handles[1] = 0xFFFFFFFFFFFFFFFFULL;
    }
    else
    {
        const smooth_path_point_info& p0 = points[insert_index - 1];
        const smooth_path_point_info& p1 = points[insert_index];
        if (p0.jumping || p1.jumping || (p0.bridge && p1.bridge) || (p0.bridge && p0.m_navcell_handle == 0xFFFFFFFFFFFFFFFFULL) || (p1.bridge && p1.m_navcell_handle == 0xFFFFFFFFFFFFFFFFULL))
        {
            return 0;
        }
        if (!nav_cell_path_get_intermediate_points(p0.m_navcell_handle, p0.pos, p1.m_navcell_handle, p1.pos, fractions, num_inserted, new_points, new_handles, traversal_rule))
        {
            return 0;
        }
    }
    memmove(&points[insert_index + num_inserted], &points[insert_index], (num_points - insert_index) * sizeof(smooth_path_point_info));
    for (int i = 0; i < num_inserted; i++)
    {
        smooth_path_point_info& p = points[insert_index + i];
        p.pos = new_points[i];
        p.m_navcell_handle = new_handles[i];
        // Original quirk kept: jumping and the rest keep the bytes of the point moved away.
        p.bridge = false;
    }
    return num_inserted;
}

// 0x00BC5E90 (:242): LTCG, point_count in EDI.
static void add_internal_path_points(int& point_count, smooth_path_point_info* points, nav_cell_traversal_rule traversal_rule)
{
    for (int i = 0; i < point_count - 1; i++)
    {
        vector delta = points[i + 1].pos - points[i].pos;
        const float dist = delta.mag_and_normalize();
        const int inserted = add_internal_points_to_path_segment(i + 1, points, dist, point_count, traversal_rule);
        point_count += inserted;
        i += inserted;
    }
}

// 0x00BC5F60 (:265): LTCG, num_points in ECX. Pulls each interior point towards the outside of its bend, when the nav mesh
// allows it.
static void flatten_vehicle_path(int num_points, int num_smoothing_passes, smooth_path_point_info* points, nav_cell_traversal_rule traversal_rule)
{
    const int num_segments = num_points - 1;
    do
    {
        num_smoothing_passes--;
        for (int i = 0; i < num_segments; i++)
        {
            const vector delta = points[i + 1].pos - points[i].pos;
            points[i].dir_to_next = delta.copy_normalize();
        }
        for (int i = 0; i < num_segments - 1; i++)
        {
            smooth_path_point_info& p0 = points[i];
            smooth_path_point_info& p1 = points[i + 1];
            smooth_path_point_info& p2 = points[i + 2];
            const vector& d0 = p0.dir_to_next;
            const vector& d1 = p1.dir_to_next;
            const float dot = d1.x * d0.x + d1.y * d0.y + d1.z * d0.z;
            float cross_y = d1.x * d0.z - d0.x * d1.z;
            const float sign = cross_y <= 0.0f ? -1.0f : 1.0f;
            cross_y *= sign;
            if (cross_y >= 1.0f)
            {
                cross_y = 1.0f;
            }
            float angle = asinf(cross_y);
            if (dot < 0.0f)
            {
                angle = 3.1415927f - angle;
            }
            angle *= sign;
            const vector push0((d0.y * UP_VECTOR.z - d0.z * UP_VECTOR.y) * angle, (d0.z * UP_VECTOR.x - d0.x * UP_VECTOR.z) * angle, (d0.x * UP_VECTOR.y - d0.y * UP_VECTOR.x) * angle);
            p0.desired_delta_pos += push0;
            p1.desired_delta_pos -= push0;
            const vector push1((d1.y * UP_VECTOR.z - d1.z * UP_VECTOR.y) * angle, (d1.z * UP_VECTOR.x - d1.x * UP_VECTOR.z) * angle, (d1.x * UP_VECTOR.y - d1.y * UP_VECTOR.x) * angle);
            p1.desired_delta_pos -= push1;
            p2.desired_delta_pos += push1;
        }
        for (int i = 1; i < num_segments; i++)
        {
            smooth_path_point_info& prev = points[i - 1];
            smooth_path_point_info& cur = points[i];
            smooth_path_point_info& next = points[i + 1];
            if (cur.bridge && next.bridge)
            {
                continue;
            }
            const vector test_pos = cur.pos + cur.desired_delta_pos * 0.5f;
            vector adjusted;
            uint64_t handle;
            bb_pf_get_nav_cell_from_hint(&adjusted, cur.m_navcell_handle, test_pos, true, &handle, true, false, traversal_rule);
            if (handle == 0xFFFFFFFFFFFFFFFFULL)
            {
                continue;
            }
            const nav_cell_los_test_point test_prev(prev.pos, prev.m_navcell_handle, 0xFFFFFFFF, -1);
            const nav_cell_los_test_point test_cur(adjusted, handle, 0xFFFFFFFF, -1);
            const nav_cell_los_test_point test_next(next.pos, next.m_navcell_handle, 0xFFFFFFFF, -1);
            unsigned int los_flags = 4;
            if (traversal_rule == NAV_CELL_TRAVERSAL_ALL)
            {
                los_flags = 0x14;
            }
            else if (traversal_rule == NAV_CELL_TRAVERSAL_ENABLED_PLUS_LIGHT)
            {
                los_flags = 0xC;
            }
            // Both are tested, no short circuit in rfg.exe.
            const bool los0 = nav_cell_test_line_of_sight(test_prev, test_cur, los_flags);
            const bool los1 = nav_cell_test_line_of_sight(test_cur, test_next, los_flags);
            if (los0 && los1)
            {
                cur.pos = adjusted;
                cur.m_navcell_handle = handle;
            }
        }
    } while (num_smoothing_passes > 0);
}

// 0x00BC66D0 (:378): LTCG, knot_vector in EDX. Clamped uniform knots 0,0,0,0,1,2,...,n-3,n-3,n-3,n-3 (inlined by rfg.exe).
static void get_cubic_bspline_starting_knot_vector(int* knot_vector, int num_control_points)
{
    knot_vector[0] = 0;
    knot_vector[1] = 0;
    knot_vector[2] = 0;
    knot_vector[3] = 0;
    int i = 4;
    for (; i < num_control_points; i++)
    {
        knot_vector[i] = i - 3;
    }
    for (; i < num_control_points + 4; i++)
    {
        knot_vector[i] = num_control_points - 3;
    }
}

// 0x00BC6710 (:407): LTCG, knot_vector in EDX, num_control_points in ECX. Boehm's knot insertion. The insert index is
// knot_value * 4 because every smaller knot has already been raised to multiplicity 4.
static void cubic_bspline_add_knot(int* knot_vector, int& num_control_points, vector* control_points, int knot_value, int& num_knot_values)
{
    vector old_points[MAX_BSPLINE_CONTROL_POINTS];
    memcpy(old_points, control_points, num_control_points * sizeof(vector));
    const int k = knot_value * 4;
    for (int i = 0; i < k - 3; i++)
    {
        control_points[i] = old_points[i];
    }
    for (int i = k - 3; i < k; i++)
    {
        const int span = knot_vector[i + 3] - knot_vector[i];
        const float a = span == 0 ? 0.0f : (float)(knot_value - knot_vector[i]) / (float)span;
        const float b = 1.0f - a;
        control_points[i].x = old_points[i - 1].x * b + old_points[i].x * a;
        control_points[i].y = old_points[i - 1].y * b + old_points[i].y * a;
        control_points[i].z = old_points[i - 1].z * b + old_points[i].z * a;
    }
    for (int i = k; i < num_control_points + 1; i++)
    {
        control_points[i] = old_points[i - 1];
    }
    for (int i = num_knot_values; i > k; i--)
    {
        knot_vector[i] = knot_vector[i - 1];
    }
    knot_vector[k] = knot_value;
    num_control_points++;
    num_knot_values++;
}

// 0x00BC6EA0 (:473): raises every interior knot to multiplicity 4, which gives 4 control points per Bezier segment, with the
// shared end point doubled.
static void convert_bspline_to_bezier_control_points(vector* control_points, int num_control_points)
{
    int knot_vector[MAX_BSPLINE_CONTROL_POINTS];
    int num_knots = num_control_points + 4;
    get_cubic_bspline_starting_knot_vector(knot_vector, num_control_points);
    const int last_knot = knot_vector[num_knots - 1];
    for (int knot = 1; knot < last_knot; knot++)
    {
        for (int i = 0; i < 3; i++)
        {
            cubic_bspline_add_knot(knot_vector, num_control_points, control_points, knot, num_knots);
        }
    }
}

// 0x00BC6F60 (:513)
void convert_catmull_rom_to_bezier_spline_control_points(vector* cr_control_pts, vector* bez_control_pts, int num_cr_control_pts)
{
    static const float CATMULL_ROM_TO_BEZIER[4][4] = {
        { 0.0f, 1.0f, 0.0f, 0.0f },
        { -0.16666667f, 1.0f, 0.16666667f, 0.0f },
        { 0.0f, 0.16666667f, 1.0f, -0.16666667f },
        { 0.0f, 0.0f, 1.0f, 0.0f },
    };
    // The end points are doubled, one vector before and one after the list.
    vector points[42];
    points[0] = cr_control_pts[0];
    memcpy(&points[1], cr_control_pts, num_cr_control_pts * sizeof(vector));
    points[num_cr_control_pts + 1] = cr_control_pts[num_cr_control_pts - 1];
    for (int i = 0; i < num_cr_control_pts - 1; i++)
    {
        for (int row = 0; row < 4; row++)
        {
            vector& out = *bez_control_pts++;
            out.zero();
            for (int col = 0; col < 4; col++)
            {
                const float w = CATMULL_ROM_TO_BEZIER[row][col];
                out.x += points[i + col].x * w;
                out.y += points[i + col].y * w;
                out.z += points[i + col].z * w;
            }
        }
    }
}

// 0x00BD5C80 (:562)
void vehicle_navmesh_lane_spline_system_init()
{
    for (int i = 0; i < MAX_NAVMESH_LS_NODES; i++)
    {
        lane_spline_base& lane = Navmesh_ls_nodes[i].lane;
        lane.knots.from_pointer(&Ls_base_knots[i * 2]);
        lane.in_handles.from_pointer(&Ls_base_in_handle[i * 2]);
        lane.out_handles.from_pointer(&Ls_base_out_handle[i * 2]);
    }
}

// 0x00BC7310 (:581)
void vehicle_navmesh_lane_spline_level_init_pre()
{
    Free_navmesh_ls_nodes = NULL;
    for (int i = 0; i < MAX_NAVMESH_LS_NODES; i++)
    {
        ls_list_append(&Free_navmesh_ls_nodes, &Navmesh_ls_nodes[i]);
    }
}

// 0x00BC73C0 (:601)
void vehicle_free_ls_node(navmesh_lane_spline_node** path, navmesh_lane_spline_node* node)
{
    ls_list_remove(path, node);
    ls_list_append(&Free_navmesh_ls_nodes, node);
}

// 0x00BC7440 (:613)
void vehicle_free_individual_ls_node(navmesh_lane_spline_node* node)
{
    ls_list_append(&Free_navmesh_ls_nodes, node);
}

// 0x00BC7470 (:627)
void vehicle_free_ls_path(navmesh_lane_spline_node** path)
{
    navmesh_lane_spline_node* node = *path;
    while (node != NULL)
    {
        navmesh_lane_spline_node* next = node->next != *path ? node->next : NULL;
        vehicle_free_ls_node(path, node);
        node = next;
    }
}

// The arc lengths of a finished node: rfg.exe marks it 0x80, "not a city spline".
static void ls_node_finish(navmesh_lane_spline_node* node)
{
    lane_spline_base& lane = node->lane;
    const cubic_bezier_spline cbs(2, lane.knots.to_pointer(), lane.in_handles.to_pointer(), lane.out_handles.to_pointer());
    const float first_half = cbs.get_length_approx(5, 0.0f, 0.5f);
    const float second_half = cbs.get_length_approx(5, 0.5f, 1.0f);
    lane.spline_flags = 0x80;
    lane.arc_length[0] = first_half + second_half;
    lane.arc_length[1] = first_half;
}

// 0x00BCEAA0 (:644)
navmesh_lane_spline_node* vehicle_navmesh_setup_2pt_path_segment(const vector& pt0, const vector& pt1, bool force_success)
{
    vector end = pt1;
    if (vec_xz_dist_squared(pt0, end) < 0.010000001f)
    {
        if (!force_success)
        {
            return NULL;
        }
        end.x += 0.1f;
        end.y += 0.1f;
        end.z += 0.1f;
    }
    navmesh_lane_spline_node* node = get_free_ls_node();
    lane_spline_base& lane = node->lane;
    vector* knots = lane.knots.to_pointer();
    vector* in_handles = lane.in_handles.to_pointer();
    vector* out_handles = lane.out_handles.to_pointer();
    knots[0] = pt0;
    knots[1] = end;
    lane.num_knots = 2;
    const vector delta = end - pt0;
    in_handles[1] = pt0 + delta * 0.67f;
    out_handles[0] = pt0 + delta * 0.33f;
    ls_node_finish(node);
    return node;
}

// 0x00BCEDE0 (:695): LTCG, navmesh_num_points in EAX: the number of B-spline control points, n - 3 Bezier segments.
static navmesh_lane_spline_node* create_cubic_bezier_path(int navmesh_num_points, vector* bez_control_pts)
{
    if (navmesh_num_points == 2)
    {
        navmesh_lane_spline_node* node = vehicle_navmesh_setup_2pt_path_segment(bez_control_pts[0], bez_control_pts[1], true);
        if (node != NULL)
        {
            node->next = node;
            node->prev = node;
        }
        return node;
    }
    if (navmesh_num_points == 3)
    {
        // A quadratic: the middle point is both handles.
        navmesh_lane_spline_node* node = get_free_ls_node();
        lane_spline_base& lane = node->lane;
        lane.knots.to_pointer()[0] = bez_control_pts[0];
        lane.knots.to_pointer()[1] = bez_control_pts[2];
        lane.num_knots = 2;
        lane.in_handles.to_pointer()[1] = bez_control_pts[1];
        lane.out_handles.to_pointer()[0] = bez_control_pts[1];
        ls_node_finish(node);
        node->next = node;
        node->prev = node;
        return node;
    }
    navmesh_lane_spline_node* path = NULL;
    for (int i = 0; i < navmesh_num_points - 3; i++)
    {
        const vector* pts = &bez_control_pts[i * 4];
        navmesh_lane_spline_node* node = get_free_ls_node();
        lane_spline_base& lane = node->lane;
        lane.knots.to_pointer()[0] = pts[0];
        lane.knots.to_pointer()[1] = pts[3];
        lane.num_knots = 2;
        lane.in_handles.to_pointer()[1] = pts[2];
        lane.out_handles.to_pointer()[0] = pts[1];
        ls_node_finish(node);
        ls_list_append(&path, node);
    }
    return path;
}

// 0x00BCF0D0 (:772)
navmesh_lane_spline_node* create_cubic_bezier_path_from_bspline_control_points(vector* control_pts, int num_control_points, int control_pts_size)
{
    convert_bspline_to_bezier_control_points(control_pts, num_control_points);
    return create_cubic_bezier_path(num_control_points, control_pts);
}

// 0x00BCF0F0 (:788)
navmesh_lane_spline_node* vehicle_get_navmesh_path_internal(const pf_calc_params& params, vehicle_path_info& path_info, vehicle* vp)
{
    pf_calc_params local_params = params;
    local_params.flags.allow_dest_in_detour_hull = 1;
    local_params.flags.force_to_nav_cell = 1;
    local_params.flags.ignore_detour_hulls = 1;
    local_params.flags.partial_path_ok = 1;
    local_params.flags.vehicle_pfg = 1;
    path_info.flags.on_bridge = 0;
    path_info.error_code = PR_OK;
    if (is_vehicle_on_bridge(vp, local_params.bridge_navpoint_handle))
    {
        path_info.flags.on_bridge = 1;
        local_params.on_bridge = true;
    }
    if (params.use_road)
    {
        bool try_normal_pathfinding = true;
        navmesh_lane_spline_node* road_path = rfg_pf_vehicle_use_road_splines(local_params, path_info, try_normal_pathfinding, vp);
        if (road_path != NULL)
        {
            return road_path;
        }
        if (!try_normal_pathfinding)
        {
            return NULL;
        }
    }
    else
    {
        path_info.flags.use_road_offset = 0;
    }
    path_info.flags.jumping = 0;
    path_outcome outcome = PR_OK;
    path_node* path = rfg_pf_vehicle_get_path(local_params, outcome);
    if (path == NULL)
    {
        path_info.error_code = outcome;
        return NULL;
    }
    if (path->prev != NULL && path->prev->flags.jumping)
    {
        path_info.flags.jumping = 1;
    }
    path_info.flags.partial_path = 0;
    path_info.start_pos = path->pos;
    path_info.start_nav_cell_handle = path->nav_cell_handle;
    path_info.end_pos = path->prev->pos;
    path_info.end_nav_cell_handle = path->prev->nav_cell_handle;
    path_info.flags.road_spline_path = 0;
    if (vp != NULL && local_params.flags.use_smoothing)
    {
        rfg_pf_calculate_turning_info_for_vehicle(vp, &path);
    }
    // At most 10 path points become the spline. The nodes after them become straight segments below.
    vector control_points[MAX_BSPLINE_CONTROL_POINTS];
    int num_control_points = 0;
    path_node* node = path;
    while (node != NULL && num_control_points != 10 && (num_control_points < 2 || !node->flags.off_navmesh))
    {
        path_node* next = node->next != path ? node->next : NULL;
        if (!node->flags.bridge)
        {
            havok_terrain_height(node->pos.y, node->pos.x, node->pos.z, false, NULL);
        }
        control_points[num_control_points] = node->pos;
        num_control_points++;
        node = next;
    }
    for (int i = 0; i < num_control_points - 1; i++)
    {
        if (vec_dist_squared(control_points[i], control_points[i + 1]) < MIN_POINT_DIST_SQ)
        {
            if (num_control_points < 3)
            {
                // Original quirk kept: always the second point, which is the only one this can be with two points.
                control_points[1].x += 0.1f;
                control_points[1].y += 0.1f;
                control_points[1].z += 0.1f;
            }
            else
            {
                memmove(&control_points[i], &control_points[i + 1], (num_control_points - i - 1) * sizeof(vector));
                num_control_points--;
                i--;
            }
        }
    }
    convert_bspline_to_bezier_control_points(control_points, num_control_points);
    navmesh_lane_spline_node* ls_path = create_cubic_bezier_path(num_control_points, control_points);
    while (node != NULL)
    {
        // Original bug kept: reads through a null last node if the spline part made no path.
        navmesh_lane_spline_node* last = ls_path != NULL ? ls_path->prev : NULL;
        const vector& last_knot = last->lane.knots.to_pointer()[1];
        if (vec_dist(last_knot, node->pos) > MIN_POINT_DIST_SQ)
        {
            navmesh_lane_spline_node* segment = vehicle_navmesh_setup_2pt_path_segment(last_knot, node->pos, false);
            if (segment != NULL)
            {
                ls_list_append(&ls_path, segment);
            }
        }
        node = node->next;
        if (node == path)
        {
            break;
        }
    }
    bb_pf_free_path(&path);
    return ls_path;
}

// 0x00BCF480 (:964)
navmesh_lane_spline_node* vehicle_generate_direct_path_between_points(int num_points, const pf_calc_point* points, vehicle_path_info& path_info, vehicle* vp)
{
    path_info.flags.jumping = 0;
    path_info.flags.use_road_offset = 0;
    path_info.flags.partial_path = 0;
    path_info.flags.on_bridge = 0;
    path_info.start_pos = points[0].pos;
    path_info.start_nav_cell_handle = points[0].nav_cell_hint;
    path_info.end_pos = points[num_points - 1].pos;
    path_info.end_nav_cell_handle = points[num_points - 1].nav_cell_hint;
    path_info.flags.road_spline_path = 0;
    vector control_points[MAX_BSPLINE_CONTROL_POINTS];
    for (int i = 0; i < num_points; i++)
    {
        control_points[i] = points[i].pos;
    }
    convert_bspline_to_bezier_control_points(control_points, num_points);
    return create_cubic_bezier_path(num_points, control_points);
}

// 0x00BCA2A0 (:1004): the first 4 path nodes are smoothed. The rest are appended to the rebuilt path as they were.
void vehicle_path_flatten(path_node** path, nav_cell_traversal_rule traversal_rule)
{
    smooth_path_point_info points[MAX_FLATTEN_POINTS];
    memset(points, 0, sizeof(points));
    int num_points = 0;
    path_node* rest = NULL;
    path_node* node = *path;
    while (node != NULL)
    {
        path_node* next_in_list = node->next;
        path_node* next = next_in_list != *path ? next_in_list : NULL;
        if (num_points <= 3)
        {
            points[num_points].m_navcell_handle = node->nav_cell_handle;
            points[num_points].pos = node->pos;
            points[num_points].jumping = node->flags.jumping;
            points[num_points].bridge = node->flags.bridge;
            num_points++;
        }
        else
        {
            if (*path == node)
            {
                if (next_in_list == node)
                {
                    node->next = NULL;
                    node->prev = NULL;
                    *path = NULL;
                    goto append_rest;
                }
                *path = next_in_list;
            }
            if (node->prev != NULL)
            {
                node->prev->next = node->next;
            }
            if (node->next != NULL)
            {
                node->next->prev = node->prev;
            }
            node->next = NULL;
            node->prev = NULL;
append_rest:
            if (rest == NULL)
            {
                node->next = node;
                node->prev = node;
                rest = node;
            }
            else
            {
                node->prev = rest->prev;
                node->next = rest;
                rest->prev->next = node;
                rest->prev = node;
            }
        }
        node = next;
    }
    const int original_num_points = num_points;
    add_internal_path_points(num_points, points, traversal_rule);
    if (original_num_points > 2)
    {
        flatten_vehicle_path(num_points, 3, points, traversal_rule);
    }
    bb_pf_free_path(path);
    for (int i = 0; i < num_points; i++)
    {
        path_node* new_node = bb_pf_get_free_path_node(points[i].pos, points[i].m_navcell_handle, 0xFFFFFFFF);
        if (new_node == NULL)
        {
            continue;
        }
        new_node->flags.bridge = points[i].bridge;
        if (*path == NULL)
        {
            new_node->next = new_node;
            new_node->prev = new_node;
            *path = new_node;
        }
        else
        {
            new_node->prev = (*path)->prev;
            new_node->next = *path;
            (*path)->prev->next = new_node;
            (*path)->prev = new_node;
        }
    }
    if (rest != NULL)
    {
        if (*path == NULL)
        {
            *path = rest;
        }
        else
        {
            (*path)->prev->next = rest;
            rest->prev->next = *path;
            path_node* rest_last = rest->prev;
            rest->prev = (*path)->prev;
            (*path)->prev = rest_last;
        }
    }
}
