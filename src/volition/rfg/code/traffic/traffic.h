// Original: volition/rfg/code/traffic/traffic.h
// Implementation: volition/rfg/code/traffic/traffic.cpp
//
// Traffic: the lane splines loaded from the traffic master file, the registry of vehicles on each spline (entering and
// exiting lanes, occupancy, obstruction), lane queries near a point or in a box, and the city splines.
// gamestate_gp_simulate_frame calls traffic_do_frame.
#pragma once


struct lane_spline;
struct lane_spline_base;
class matrix;
struct spline_vehicles;
class vector;

// size 0x10
struct lane_path_node
{
    lane_path_node* next; // 0x00
    lane_path_node* prev; // 0x04
    float dist; // 0x08
    const lane_spline* lane; // 0x0C
};

// size 0x1C
class traffic_lane_query
{
public:
    traffic_lane_query(const vector& _search_center, float _radius, int _max_lane_splines, lane_spline** _found_lanes); // 0x00B5FF50 (:416)

    const vector& search_center; // 0x00
    float radius; // 0x04
    unsigned int include_flags; // 0x08
    unsigned int exclude_flags; // 0x0C
    int max_lane_splines; // 0x10
    int num_found_lane_splines; // 0x14
    lane_spline** found_lanes; // 0x18
};

void free_spline_vehicle_registry(spline_vehicles** registry_list, spline_vehicles* registry); // 0x00B60910 (:153)
void lane_spline_vehicle_registry_level_init(); // 0x00B609B0 (:167)
int traffic_count_vehicles_on_spline(const lane_spline* lsp, bool forward); // 0x00B60B60 (:430)
bool is_spline_city_spline(const lane_spline_base* lsp); // 0x00B5FF90 (:484)
const lane_spline* get_city_spline_from_base_spline(const lane_spline_base* lsp); // 0x00B5FFB0 (:495)
int get_traffic_lane_index(const lane_spline_base* lsp); // 0x00B60510 (:505)
lane_spline* get_traffic_lane_spline_from_index(int lane_spline_index); // 0x00B60550 (:519)
void traffic_load_master(const unsigned char* data); // 0x00B60C30 (:526)
void traffic_unload(); // 0x00B5FFD0 (:626)
bool traffic_is_loaded(); // 0x00B5FFE0 (:633)
void traffic_system_init(); // 0x00B5FFF0 (:638)
void traffic_level_init_post(); // 0x00B60000 (:645)
void traffic_render_path(lane_path_node* path); // 0x00B60570 (:652)
void traffic_do_frame(); // 0x00B60010 (:898)
void traffic_render(); // 0x00B60020 (:908)
void lane_spline_calc_pos_and_orient(const lane_spline* ls, float parametric_pos, vector* pos, matrix* orient); // 0x00B60600 (:921)
void traffic_lane_enter(lane_spline_base* lsp); // 0x00B60690 (:940)
void traffic_lane_exit(lane_spline* lsp); // 0x00B606D0 (:950)
bool traffic_lane_is_occupied(lane_spline_base* lsp); // 0x00B60710 (:960)
void traffic_lane_get_position(vector& out, const lane_spline* lane, float spline_pos); // 0x00B60760 (:978)
bool traffic_lanes_exist_near_point(const vector& pos, float radius, int lane_include_filter, int lane_exclude_filter); // 0x00B607B0 (:993)
void traffic_get_lanes_near_point(traffic_lane_query& lane_query); // 0x00B60810 (:1012)
void traffic_get_lanes_in_aabb_crude(const vector& bmin, const vector& bmax, const int lane_include_filter, const int lane_exclude_filter, lane_spline** lanes, int& num_lanes, int max_lanes); // 0x00B60820 (:1029)
void traffic_clear_lane_obstructed_flags(); // 0x00B60850 (:1141)
float get_traffic_speed_from_lane_spline(const lane_spline_base* lsbp); // 0x00B60030 (:1162)
