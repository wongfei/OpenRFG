// Original: volition/rfg/code/pathfinding/bb_pathfinding.h
// Implementation: volition/rfg/code/pathfinding/bb_pathfinding.cpp
//
// The pathfinding front end: human paths over the nav meshes of the zone pathfinding graphs (bb_pfg_layer), path
// requests and their calculation parameters, raw and simplified paths, closest-point and nav cell queries, snapping to
// the ground or nav mesh, repulsors (oriented boxes around objects that paths steer around) and dynamic holes.
// rfg_pf_do_frame runs once per gameplay frame in single player before the AI. subpiece_data (A* state over the
// subpieces of a destroyable building) has no methods with line info; its constructor lies in the pathfinding code.
#pragma once

#include <stdint.h>
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai_enums.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

struct bb_pfg_layer;
class human;
struct navmesh_lane_spline_node;
class navpoint;
class npc;
class obj_zone;
class object;
class object_mover;
class rfg_destruction_event;
class rfg_mover;
struct rfg_subpiece;
class vehicle;
struct vehicle_path_info;

// size 0x3
class pf_calc_flags
{
public:
    pf_calc_flags(bool _vehicle_pfg); // 0x00AEB750 (:4883)

    unsigned char allow_dest_in_detour_hull : 1; // 0x00:0
    unsigned char allow_out_of_chunk : 1; // 0x00:1
    unsigned char allow_out_of_chunk_no_raycast : 1; // 0x00:2
    unsigned char allow_land_to_water : 1; // 0x00:3
    unsigned char disallow_climbing : 1; // 0x00:4
    unsigned char do_not_modify_simple_detour_hull_paths : 1; // 0x00:5
    unsigned char force_to_nav_cell : 1; // 0x00:6
    unsigned char force_complete_node_optimization : 1; // 0x00:7
    unsigned char ignore_detour_hulls : 1; // 0x01:0
    unsigned char pre_detour_simplification : 1; // 0x01:1
    unsigned char use_leash : 1; // 0x01:2
    unsigned char partial_path_ok : 1; // 0x01:3
    unsigned char vehicle_pfg : 1; // 0x01:4
    unsigned char vehicle_simplify_path_after_segment : 1; // 0x01:5
    unsigned char use_disabled_cells : 1; // 0x01:6
    unsigned char use_light_structure_disabled_cells : 1; // 0x01:7
    unsigned char use_smoothing : 1; // 0x02:0
    unsigned char encourage_edf_spread : 1; // 0x02:1
};

// size 0x20
class pf_calc_point
{
public:
    pf_calc_point(const pf_calc_point& __that); // 0x00AEC090
    pf_calc_point(const vector& _pos, uint64_t _nav_cell_hint, unsigned int _zone_handle); // 0x00AEB770 (:4895)

    vector pos; // 0x00
    uint64_t nav_cell_hint; // 0x10
    unsigned int zone_handle; // 0x18
};

// size 0x18
class pf_calc_params
{
public:
    pf_calc_params(int _num_points, const pf_calc_point* _points, const pf_calc_flags& _flags, bool _use_road, bool _use_special_roads, bool _flying, bool _on_bridge, unsigned int _bridge_navpoint_handle); // 0x00AEB7B0 (:4910)

    int num_points; // 0x00
    const pf_calc_point* points; // 0x04
    pf_calc_flags flags; // 0x08
    bool use_road; // 0x0B
    bool flying; // 0x0C
    bool on_bridge; // 0x0D
    bool use_special_roads; // 0x0E
    bool cap_max_points; // 0x0F
    bool ignore_road_directionality; // 0x10
    unsigned int bridge_navpoint_handle; // 0x14
};

// size 0x40
struct path_node
{
    path_node(); // 0x00AEA030

    path_node* next; // 0x00
    path_node* prev; // 0x04
    vector pos; // 0x08
    uint64_t nav_cell_handle; // 0x18
    unsigned int navpoint_handle; // 0x20
    vector normal; // 0x24
    float distance_left; // 0x30
    unsigned int compound_hull_handle; // 0x34
    int compound_hull_vert_index; // 0x38
    struct
    {
        bool do_not_remove : 1; // 0x00:0
        bool jumping : 1; // 0x00:1
        bool dropping_down : 1; // 0x00:2
        bool leap_over : 1; // 0x00:3
        bool ladder : 1; // 0x00:4
        bool optimized : 1; // 0x00:5
        bool off_navmesh : 1; // 0x00:6
        bool bridge : 1; // 0x00:7
        bool stairs : 1; // 0x01:0
    } flags; // 0x3C
};

// size 0x2C
struct raw_path
{
    raw_path(); // 0x00AEAC20

    path_node* path; // 0x00
    vector adjusted_start_pos; // 0x04
    nav_cell* start_nav_cell; // 0x10
    vector adjusted_end_pos; // 0x14
    nav_cell* end_nav_cell; // 0x20
    bool out_of_chunk; // 0x24
    path_outcome result; // 0x28
};

// size 0x2A24
struct path_info
{
    // UI0: compiler-generated (no line info), so left implicit: the default constructor 0x0086CAB0, the destructor
    // 0x00869DE0 and the copy assignment 0x008803A0 (the minimap's farray<path_info,4> needs the first two).

    unsigned int handle; // 0x00
    farray<vector,384> pts; // 0x04
    vector2 spline_points[512]; // 0x1214
    float spline_heights[512]; // 0x2214
    int num_spline_pts; // 0x2A14
    bool edf_route; // 0x2A18
    int last_line_index; // 0x2A1C
    bool render_dirty; // 0x2A20
};

// size 0x24
struct repulsor_edge
{
    repulsor_edge(); // 0x00AEA040

    vector2 edge_vert0; // 0x00
    vector2 edge_vert1; // 0x08
    vector2 edge_rvec; // 0x10
    vector2 closest_point; // 0x18
    float dist_squared_to_closest_point; // 0x20
};

// size 0x3C
struct repulsor_oriented_bbox
{
    repulsor_oriented_bbox(); // 0x00A0B8F0

    vector size; // 0x00
    vector pos; // 0x0C
    matrix orient; // 0x18
};

// size 0x40
struct repulsor_object
{
    repulsor_object(); // 0x00A31410

    object* objp; // 0x00
    repulsor_oriented_bbox oriented_bbox; // 0x04
};

// size 0x214
struct compound_repulsor
{
    compound_repulsor(); // 0x00A56A10

    int num_objects; // 0x00
    repulsor_object objects[8]; // 0x04
    vector pos; // 0x204
    float radius; // 0x210
};

// size 0x34
struct edge_constraint_info
{
    edge_constraint_info(); // 0x00AEAB20

    bool active; // 0x00
    vector edge_p0; // 0x04
    vector edge_p1; // 0x10
    vector edge_dir; // 0x1C
    float edge_length; // 0x28
    float ix_min; // 0x2C
    float ix_max; // 0x30
};

// size 0xC
class dynamic_hole_request
{
public:
    dynamic_hole_request(); // 0x00AF5440 (:9444)
    void reset(); // 0x00AEBD60 (:9452)

    rfg_destruction_event* devp; // 0x00
    timestamp last_modified_timestamp; // 0x04
    struct
    {
        unsigned char processed : 1; // 0x00:0
        unsigned char removed : 1; // 0x00:1
        unsigned char pass_1 : 1; // 0x00:2
        unsigned char pass_2 : 1; // 0x00:3
    } flags; // 0x08
};

// size 0x20
struct subpiece_data
{
    subpiece_data(); // 0x00AEB900

    rfg_subpiece* owner; // 0x00
    float travel_cost; // 0x04
    float heuristic; // 0x08
    unsigned int obj_index; // 0x0C
    unsigned int from_index; // 0x10
    vector mesh_pos; // 0x14
};

void rfg_pf_offsets_to_pointer(bb_pfg_layer* pfg); // 0x00AFA0E0 (:886)
void simplify_nav_path_by_los(path_node** path, const pf_calc_flags& flags); // 0x00B14520 (:2101)
void rfg_pf_instantiate(obj_zone* zonep); // 0x00B08D50 (:5076)
void rfg_pf_delete(obj_zone* zonep); // 0x00B08DC0 (:5107)
void rfg_pf_do_frame(); // 0x00B1F2C0 (:5144)
nav_cell* bb_pf_get_nav_cell_new(const vector& pos, bool vehicle_pfg, uint64_t* nav_cell_handle, bool force_to_cell, vector* adjust_pos); // 0x00B0DB40 (:5325)
nav_cell* bb_pf_get_nav_cell_original(const vector& pos, bool vehicle_pfg, uint64_t* nav_cell_handle, bool force_to_cell, nav_cell_traversal_rule traversal_rule, vector* adjust_pos); // 0x00B0DC10 (:5368)
nav_cell* bb_pf_get_nav_cell(const vector& pos, bool vehicle_pfg, uint64_t* nav_cell_handle, bool force_to_cell, nav_cell_traversal_rule traversal_rule, vector* adjust_pos); // 0x00B0DFD0 (:5475)
void bb_pf_enable_nav_cells(vector& bmin, vector& bmax, vector& pos, matrix& orient); // 0x00AEB810 (:5627)
bool rfg_snap_to_ground(vector& pos); // 0x00AEB8D0 (:5658)
bool rfg_snap_down_to_nav_mesh(vector& pos, const nav_cell* nav_cell); // 0x00B0E200 (:5667)
nav_cell* bb_pf_get_nav_cell_from_hint(vector* adjusted_pos, uint64_t nav_cell_hint_hande, const vector& pos, bool vehicle_pfg, uint64_t* nav_cell_handle, bool simple_search, bool force_to_cell, nav_cell_traversal_rule traversal_rule); // 0x00B159C0 (:5711)
nav_cell* bb_pf_get_nav_cell_from_hint(vector* adjusted_pos, nav_cell* nav_cell_hint, const vector& pos, bool vehicle_pfg, uint64_t* nav_cell_handle, bool simple_search, bool force_to_cell, nav_cell_traversal_rule traversal_rule); // 0x00B0E260 (:5717)
void bb_pf_simplify_raw_path(path_node** path, const pf_calc_flags& flags); // 0x00B15A00 (:5803)
bool bb_pf_is_nav_cell_within_vertical_tolerance(const nav_cell* ncp, const vector& pos, float* delta_y); // 0x00B0E3C0 (:5825)
bool bb_pf_find_closest_point_on_nav_mesh_fast(vector& adjusted_pos, nav_cell*& found_nav_cell, const vector& pos, float* dist_sq_out, bool* in_found_cell, bool vehicle_pfg, float collect_radius); // 0x00B0E440 (:5930)
bool bb_pf_find_closest_point_on_nav_mesh(const vector& pos, vector& adjusted_pos, nav_cell*& found_nav_cell, bool vehicle_pfg); // 0x00B0E680 (:5993)
bool bb_pf_find_closest_point_in_or_on_nav_mesh(const vector& pos, vector& adjusted_pos, nav_cell*& found_nav_cell, bool& on_navmesh, bool vehicle_pfg); // 0x00B0EA00 (:6122)
bool bb_pf_find_closest_point_on_nav_mesh_slow(const vector& pos, vector& adjusted_pos, nav_cell*& found_nav_cell); // 0x00B0EDB0 (:6203)
bool rfg_pf_find_closest_point_on_nav_mesh_using_collect(const vector& pos, bool vehicle_pfg, vector& adjusted_pos, nav_cell*& found_nav_cell, float box_size); // 0x00B0F040 (:6261)
bool rfg_pf_find_best_transition_node_from_pos(const vector& dest_pos, const vector& pos, navpoint*& found_node); // 0x00B15A30 (:6284)
navpoint* rfg_pf_find_closest_navpoint_to_pos(const vector& start_pos, navpoint*& other_navpoint, signed char low_type, signed char high_type); // 0x00B032F0 (:7299)
path_node* rfg_pf_pathfind(human* hp, const vector& start_pos, const vector& end_pos, uint64_t start_nav_cell_hint_handle, uint64_t end_nav_cell_hint_handle, bool out_of_chunk); // 0x00B1B330 (:7406)
nav_cell* bb_pf_get_closest_nav_cell(const vector& pos, bool vehicle_pfg, uint64_t* nav_cell_handle); // 0x00B0F140 (:7821)
void bb_pf_update_last_visited_nav_cell(vehicle* vp); // 0x00B15C80 (:7936)
void bb_pf_update_last_visited_nav_cell(human* hp); // 0x00B15CA0 (:7945)
void bb_pf_update_last_visited_nav_cell(uint64_t& last_visited_nav_cell_handle, timestamp& last_visited_nav_cell_update_timer, const vector& pos, bool vehicle_pfg); // 0x00B0F3F0 (:7967)
bool bb_pf_is_point_visible(const vector& start_pos, const vector& end_pos, uint64_t start_nav_cell_hint_handle, uint64_t end_nav_cell_hint_handle, bool vehicle_pfg, bool* no_start_nav_cell); // 0x00B15CD0 (:8013)
path_node* rfg_pf_advance_to_next_path_node(path_node** path, path_node* node); // 0x00AF4660 (:8058)
path_node* bb_pf_get_free_path_node(const vector& pos, uint64_t nav_cell_handle, unsigned int navpoint_handle); // 0x00AEB910 (:8075)
void bb_pf_free_path(path_node** path); // 0x00AF4690 (:8113)
void bb_pf_free_path_node(path_node** path, path_node* node); // 0x00AEB9B0 (:8127)
void rfg_pf_maybe_add_projectile_repulsors(human* hp); // 0x00AF4800 (:8223)
vector bb_pf_calculate_turn_to_target(human* hp, const vector& dest, int& num_active_repulsors, compound_repulsor* active_compound_repulsors, bool check_start_nav_cell); // 0x00B15DF0 (:8302)
bool bb_pf_get_repulsor_oriented_bounding_box(repulsor_oriented_bbox* oriented_bbox, const object* repulsor); // 0x00AF4950 (:8798)
float bb_pf_get_repulsor_xz_radius(const object* repulsor, const repulsor_oriented_bbox& oriented_bbox); // 0x00AF50A0 (:8899)
void bb_pf_check_for_obstructed_destinations(); // 0x00AFC470 (:8971)
void bb_pf_check_extended_stuck(); // 0x00AFC5F0 (:9035)
int bb_pf_collect_obstructions(unsigned int* handles, int max_handles, npc* np, const vector& dest); // 0x00AEBAA0 (:9126)
bool rfg_pf_should_avoid_mover(object_mover* mover); // 0x00AEBC30 (:9209)
bool bb_pf_find_closest_navpoint(const vector& hit_pos, navpoint*& closest_navpoint, float& closest_dist_sq); // 0x00B037C0 (:9484)
bool walls_floors_collect_cb(object* obj, unsigned int shape_key, const void* cb_data); // 0x00AEC070 (:10127)
void add_dynamic_hole_request(rfg_destruction_event* devp); // 0x00B040A0 (:11305)
void remove_dynamic_hole_request(rfg_destruction_event* devp); // 0x00B04100 (:11337)
void dynamic_hole_requests_level_init_post(); // 0x00B04130 (:11346)
void dynamic_hole_requests_do_frame(); // 0x00B119E0 (:11441)
void dynamic_hole_requests_remove_for_building(rfg_mover* rfgmp); // 0x00B04760 (:11554)
path_node* rfg_pf_vehicle_get_path(const pf_calc_params& params, path_outcome& error_code); // 0x00B1BF60 (:11693)
bool rfg_pf_create_route_segment(const vector& start_pos, const vector& end_pos, farray<vector,512>& route, bool include_start); // 0x00B04A70 (:12122)
navmesh_lane_spline_node* rfg_pf_vehicle_use_road_splines(pf_calc_params real_params, vehicle_path_info& path_info, bool& try_normal_pathfinding, vehicle* vp); // 0x00AFD7A0 (:12189)
bool rfg_find_closest_pt_on_road_to_bbox(vector& closest_pt, const vector& bmin, const vector& bmax, const vector& other_point); // 0x00B01800 (:13183)
const navpoint* rfg_find_best_exit_point(const object* building, navpoint* starting_navp, bool one_way, bool use_doors); // 0x00B17000 (:13234)
void rfg_pf_system_init(); // 0x00AEC200 (:13295)
void rfg_pf_level_init_pre(); // 0x00AF5DF0 (:13318)
void rfg_pf_level_shutdown(); // 0x00AEC210 (:13349)
void rfg_pf_get_smoothing_position(human* hp, vector& dest_pos); // 0x00AEC220 (:13353)
bool rfg_pf_calculate_smoothing_info(human* hp); // 0x00B1C610 (:13375)
int rfg_pf_calculate_turning_info_for_flyers(vehicle* vp, const vector& dest_point, int max_points, vector* points); // 0x00AF5EF0 (:13815)
bool rfg_pf_calculate_turning_info_for_vehicle(vehicle* vp, path_node** current_path); // 0x00B1D6D0 (:14117)
bool rfg_pf_add_impassable_nav_cell(nav_cell* ncp); // 0x00AEC4F0 (:14454)
void rfg_pf_clear_impassable_nav_cells(); // 0x00AEC530 (:14467)
