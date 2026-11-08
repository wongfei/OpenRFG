// Original: volition/rfg/code/pathfinding/nav_cell.h
// Implementation: volition/rfg/code/pathfinding/nav_cell.cpp
//
// Nav cells: the convex polygons of the nav meshes (vertices, edges, adjacent cells, traversal flags) addressed by
// handles that encode zone, layer and index. Point and segment classification against edges, forcing points into a
// cell, traversal costs, random points, zone membership and the longest line of sight through the mesh (stopped by
// the mesh border or a detour hull).
#pragma once

#include <stdint.h>
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/endian_type.h"
// vertex (the legacy 3D API's transformed point) was placed here by the header bootstrap; it lives in gr.h since
// VFX.md FX5 (gr_3d.cpp uses it), included for the code that found it here.
#include "volition/render_lib/code/gr/gr.h"

struct compound_hull;
struct compound_hull_vert;
struct nav_cell;
class nav_heap;
class obj_zone;
struct path_node;
class pf_calc_flags;
class plane;

enum line_classification
{
    LINE_CLASS_COLLINEAR = 0,
    LINE_CLASS_LINES_INTERSECT = 1,
    LINE_CLASS_SEGMENTS_INTERSECT = 2,
    LINE_CLASS_SEGMENT_BISECTS_EDGE = 3,
    LINE_CLASS_EDGE_BISECTS_SEGMENT = 4,
    LINE_CLASS_PARALLEL = 5,
};

enum point_classification
{
    POINT_CLASS_INVALID = -1,
    POINT_CLASS_ON = 0,
    POINT_CLASS_LEFT = 1,
    POINT_CLASS_RIGHT = 2,
};

enum path_result
{
    PATH_RESULT_NO_RELATIONSHIP = 0,
    PATH_RESULT_ENDING_CELL = 1,
    PATH_RESULT_EXITING_CELL = 2,
};

enum nav_cell_traversal_rule
{
    NAV_CELL_TRAVERSAL_ENABLED = 0,
    NAV_CELL_TRAVERSAL_ENABLED_PLUS_LIGHT = 1,
    NAV_CELL_TRAVERSAL_ALL = 2,
};

enum nav_cell_in_zone_result
{
    NAV_CELL_NOT_IN_ZONE = 0,
    NAV_CELL_IN_ZONE_PEDESTRIAN = 1,
    NAV_CELL_IN_ZONE_VEHICLE = 2,
};

enum nav_cell_longest_los_result
{
    NAV_CELL_LONGEST_LOS_RESULT_NONE = -1,
    NAV_CELL_LONGEST_LOS_RESULT_REACHED_DESTINATION = 0,
    NAV_CELL_LONGEST_LOS_RESULT_EXITED_NAVMESH = 1,
    NAV_CELL_LONGEST_LOS_RESULT_CONTACTED_DETOUR_HULL = 2,
    NUM_NAV_CELL_LONGEST_LOS_RESULTS = 3,
};

// size 0x4
struct nav_cell_los_obstruction_data_navmesh
{
    int edge_index; // 0x00
};

// size 0x8
struct nav_cell_los_obstruction_data_detour
{
    compound_hull* cmp_hull; // 0x00
    int entry_edge_index; // 0x04
};

// size 0x8
union nav_cell_los_obstruction_data
{
    nav_cell_los_obstruction_data_navmesh navmesh; // 0x00
    nav_cell_los_obstruction_data_detour detour; // 0x00
};

// size 0x20
class nav_cell_los_test_point
{
public:
    nav_cell_los_test_point(const nav_cell_los_test_point& __that); // 0x00AECF80
    nav_cell_los_test_point(const vector& _pos, uint64_t _nav_cell_handle, unsigned int compound_hull_handle, int compound_hull_vert_index); // 0x00AF3E20 (.h:91)
    nav_cell_los_test_point(); // 0x00AE9EB0 (.h:83)

    vector pos; // 0x00
    uint64_t nav_cell_handle; // 0x10
    const compound_hull* cmp_hull; // 0x18
    const compound_hull_vert* vert; // 0x1C
};

// size 0x28
struct nav_cell_longest_los_input
{
    nav_cell_longest_los_input(const nav_cell_longest_los_input& __that); // 0x00AECFC0
    nav_cell_longest_los_input(); // 0x005DCB50 (.h:117)

    struct
    {
        unsigned char check_start_in_detour_hull : 1; // 0x00:0
        unsigned char ignore_detour_hulls : 1; // 0x00:1
        unsigned char use_light_nav_cells : 1; // 0x00:2
        unsigned char use_disabled_nav_cells : 1; // 0x00:3
    } flags; // 0x00
    vector start_pos; // 0x04
    nav_cell* start_nav_cell; // 0x10
    vector2 delta_2d; // 0x14
    int num_fan_angle_increments; // 0x1C
    float max_fan_angle; // 0x20
    int max_visited_nav_cells; // 0x24
};

// size 0x28
struct nav_cell_longest_los_output
{
    nav_cell_longest_los_output(const nav_cell_longest_los_output& __that); // 0x00AED010
    nav_cell_longest_los_output(); // 0x005DCB90 (.h:153)

    nav_cell_longest_los_result result; // 0x00
    vector end_pos; // 0x04
    nav_cell* end_nav_cell; // 0x10
    float length; // 0x14
    nav_cell_los_obstruction_data obstruction; // 0x18
    int num_visited_nav_cells; // 0x20
    nav_cell** visited_nav_cells; // 0x24
};

// nav_cell::flags as rfg.exe tests them (the names are not recovered: read from their uses).
static const unsigned char NAV_CELL_FLAG_BLOCKED = 0x04;
static const unsigned char NAV_CELL_FLAG_DISABLED = 0x08;
static const unsigned char NAV_CELL_FLAG_LIGHT = 0x10; // disabled by a light structure
static const unsigned char NAV_CELL_FLAG_VEHICLE = 0x20; // belongs to the zone's vehicle graph

// The los_obstruction_flags of the classification and line of sight functions
// (nav_cell_get_los_obstruction_flags_from_path_calc_flags); the names are not recovered.
static const unsigned int NAV_CELL_LOS_NO_CLIMBING = 0x01;
static const unsigned int NAV_CELL_LOS_LAND_TO_WATER = 0x02;
static const unsigned int NAV_CELL_LOS_IGNORE_DETOUR = 0x04;
static const unsigned int NAV_CELL_LOS_ALLOW_LIGHT = 0x08;
static const unsigned int NAV_CELL_LOS_ALLOW_DISABLED = 0x10;

// size 0x20
struct nav_cell
{
    et_ptr_offset<nav_cell,1> adjacent_cells[3]; // 0x00
    unsigned int navp; // 0x0C
    unsigned int heap_session_id; // 0x10
    unsigned short vertices[3]; // 0x14
    unsigned short compound_hull_reference; // 0x1A
    unsigned short heap_data_index; // 0x1C
    unsigned char zone_id; // 0x1E
    unsigned char flags; // 0x1F
};

bool nav_cell_is_disabled(const nav_cell* ncp); // 0x005DCBD0 (.h:272)
void nav_cell_get_edge_midpoint(vector* edge_midpoint, const nav_cell* ncp, int edge_index); // 0x005DCBE0 (.h:296)
path_result nav_cell_classify_path_to_cell(const nav_cell* ncp, const vector& start_pos, const vector& end_pos, nav_cell** next_cell, int* edge_num, vector* ix_pos, unsigned int los_obstruction_flags); // 0x00B04DF0 (:261)
void nav_cell_system_init(); // 0x00AEC570 (:1060)
void nav_cell_level_init_pre(); // 0x00AEC580 (:1071)
void nav_cell_get_handle_components(unsigned int* z_handle, unsigned int* use_vehicle_pfg, unsigned int* cell_idx, unsigned int* dynamic_pfg, uint64_t handle); // 0x00AEC590 (:1103)
nav_cell* nav_cell_from_handle(uint64_t handle); // 0x00B01B60 (:1117)
uint64_t nav_cell_get_handle(const nav_cell* ncp); // 0x00B053B0 (:1153)
nav_cell_in_zone_result nav_cell_does_zone_contain_nav_cell(const nav_cell* ncp, const obj_zone* zone, int* nav_cell_index); // 0x00B01BE0 (:1227)
obj_zone* nav_cell_get_zone_containing_nav_cell(const nav_cell* ncp); // 0x00B01C60 (:1263)
bool nav_cell_is_point_in_cell_column(const nav_cell* ncp, const vector& pos); // 0x00B09340 (:1291)
bool nav_cell_is_point_in_cell_column(const nav_cell* ncp, const vector2& pos); // 0x00B09420 (:1331)
float nav_cell_get_closest_xz_dist_squared_to_point(const nav_cell* ncp, const vector& pos); // 0x00B054E0 (:1364)
vector nav_cell_force_point_to_cell(const nav_cell* ncp, const vector& pos); // 0x00B094D0 (:1394)
vector nav_cell_force_point_to_cell_no_height(const nav_cell* ncp, const vector& pos); // 0x00B09830 (:1477)
vector nav_cell_force_point_to_cell(uint64_t nav_cell_handle, const vector& pos); // 0x00B09B70 (:1567)
vector nav_cell_force_point_to_cell_no_height(uint64_t nav_cell_handle, const vector& pos); // 0x00B09BD0 (:1583)
void nav_cell_force_point_to_cell_plane(vector& pos, const nav_cell* ncp); // 0x00B05620 (:1597)
float nav_cell_get_traversal_cost(const nav_cell* start_ncp, const vector& start_pos, const nav_cell* end_ncp, const vector& end_pos); // 0x00AEC5D0 (:1617)
void nav_cell_get_random_point(vector& point, const nav_cell* ncp); // 0x00B056A0 (:1679)
point_classification nav_cell_classify_point_vs_edge(const nav_cell* ncp, int edge_num, const vector2& pos); // 0x00B05BD0 (:1742)
point_classification nav_cell_classify_point_vs_edge_vertices(const nav_cell* ncp, const vector* edge_vert0, const vector* edge_vert1, const vector2& pos); // 0x00AEC5F0 (:1799)
point_classification nav_cell_classify_point_vs_edge(const nav_cell* ncp, int edge_num, const vector& pos); // 0x00B05D80 (:1845)
point_classification nav_cell_classify_point_vs_edge_vertices(const nav_cell* ncp, const vector* edge_vert0, const vector* edge_vert1, const vector& pos); // 0x00AEC6E0 (:1859)
line_classification nav_cell_classify_segment_vs_edge(const vector& s1, const vector& s2, const vector& e1, const vector& e2, vector* ix_pos); // 0x00AEC7D0 (:1875)
line_classification nav_cell_classify_segment_vs_edge(const nav_cell* ncp, int edge_num, const vector& s1, const vector& s2, vector* ix_pos); // 0x00B05DC0 (:1928)
line_classification nav_cell_classify_segment_vs_edge_vertices(const nav_cell* ncp, const vector* edge_vert0, const vector* edge_vert1, const vector& s1, const vector& s2, vector* ix_pos); // 0x00AECA50 (:2012)
line_classification nav_cell_classify_segment_vs_edge(const nav_cell* ncp, int edge_num, const vector2& s1, const vector2& s2, vector2* ix_pos); // 0x00B061F0 (:2093)
bool nav_cell_path_get_intermediate_points(uint64_t start_nav_cell_handle, const vector& start_pos, uint64_t end_nav_cell_handle, const vector& end_pos, const float* intermediate_fractions, int num_intermediate_points, vector* intermediate_points, uint64_t* intermediate_handles, nav_cell_traversal_rule traversal_rule); // 0x00B09C30 (:2202)
bool nav_cell_test_line_of_sight(const nav_cell_los_test_point& start_point, const nav_cell_los_test_point& end_point, unsigned int los_obstruction_flags); // 0x00B12680 (:2350)
bool nav_cell_test_line_of_sight_no_height(const nav_cell_los_test_point& start_point, const nav_cell_los_test_point& end_point, unsigned int los_obstruction_flags); // 0x00B129C0 (:2473)
bool nav_cell_test_line_of_sight(uint64_t start_nav_cell_handle, const vector& start_pos, uint64_t end_nav_cell_handle, const vector& end_pos, unsigned int los_obstruction_flags); // 0x00B12D00 (:2607)
bool nav_cell_test_line_of_sight_no_height(uint64_t start_nav_cell_handle, const vector& start_pos, uint64_t end_nav_cell_handle, const vector& end_pos, unsigned int los_obstruction_flags); // 0x00B12D80 (:2616)
bool nav_cell_find_longest_line_of_sight_no_detour(vector& out_pos, const nav_cell*& out_nav_cell, const vector& start_pos, const nav_cell* start_nav_cell, const vector& end_pos, bool force_to_cell, int* final_out_edge); // 0x00B0A110 (:2740)
bool nav_cell_find_first_valid_cell(vector& out_pos, const nav_cell*& out_nav_cell, const vector& start_pos, const nav_cell* start_nav_cell, const vector& end_pos, bool force_to_cell, int* enter_edge); // 0x00B0A560 (:2862)
void nav_cell_find_longest_line_of_sight_point(nav_cell_longest_los_output& output, const nav_cell_longest_los_input& input); // 0x00B12E00 (:2973)
bool nav_cell_query_for_path(nav_cell* ncp, nav_cell* arrival_cell, const vector& arrival_pos, float arrival_cost, const vector& goal_pos, nav_heap* nhp, bool through_mover, unsigned short which_navpoint); // 0x00B06670 (:3040)
bool nav_cell_ix_linesegment_linesegment(vector2* ix_pos, const vector2& line0_vert0, const vector2& line0_vert1, const vector2& line1_vert0, const vector2& line1_vert1); // 0x00AED060 (:3160)
void nav_cell_get_centroid(vector* centroid, const nav_cell* ncp); // 0x00B01D10 (:3196)
void nav_cell_get_cell_plane(plane* cell_plane, const nav_cell* ncp); // 0x00B01E40 (:3221)
void nav_cell_get_edge_dir(vector* edge_dir, const nav_cell* ncp, int edge_index); // 0x00B06890 (:3237)
unsigned int nav_cell_get_los_obstruction_flags_from_path_calc_flags(const pf_calc_flags& flags); // 0x00AED250 (:3252)
nav_cell* find_first_valid_nav_cell_along_path(const vector& start_pos, const vector& end_pos, nav_cell* starting_nav_cell, vector& ending_position); // 0x00B06990 (:3282)
bool nav_cell_check_curve(path_node** curve_nodes, uint64_t start_nav_cell_handle, const vector& start_pos, const vector& end_pos, float radius, const vector2& center, float angleStart, float angleFinal, const vector& final_pos, float curveLength, bool turnRight, bool vehicle_pfg); // 0x00B17110 (:3332)
void nav_cell_get_edge_verts(vector* edge_vert0, vector* edge_vert1, const nav_cell* ncp, int edge_index, obj_zone* zone); // 0x00B01EA0 (:3564)
bool nav_cell_get_all_verts(const nav_cell* ncp, vector& vert_a, vector& vert_b, vector& vert_c); // 0x00B01F30 (:3581)
void nav_cell_get_xz_bounding_box(vector2* xz_min, vector2* xz_max, const nav_cell* ncp); // 0x00B01FD0 (:3606)
vector* nav_cell_get_vertex(const nav_cell* ncp, int vertex_index, obj_zone* zone); // 0x00B02190 (:3634)
void nav_cell_get_vertices(vector* vertex0, vector* vertex1, vector* vertex2, const nav_cell* ncp); // 0x00B021E0 (:3647)
float nav_cell_get_area_2d(const nav_cell* ncp); // 0x00B02270 (:3662)
