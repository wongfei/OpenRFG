// Original: volition/rfg/code/vehicle_ai/vehicle_ai.h
// Implementation: volition/rfg/code/vehicle_ai/vehicle_ai.cpp
//
// The driver AI of a vehicle (vehicle::m_ai): state, route (vehicle_route_info, Bezier segments along roads or nav
// mesh paths), steering, acceleration, stuck recovery, repulsors and pathfinding requests. Placement of the small types
// inferred.
#pragma once

#include <stdint.h>
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai_enums.h"
#include "volition/rfg/code/vehicle_ai/vehicle_repulsor.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/spline.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

struct lane_path_node;
struct lane_spline;
struct lane_spline_base;
struct navmesh_lane_spline_node;
class navpoint;
class object;
class vehicle;
struct vehicle_route_info;

enum position_on_route_result
{
    PRR_OK = 0,
    PRR_SHORT = 1,
    PRR_ERROR = 2,
};

// size 0x68
struct havok_vehicle_ai_info
{
    havok_vehicle_ai_info(); // 0x00B64200

    unsigned int flags; // 0x00
    vector pos; // 0x04
    vector vel; // 0x10
    float speed; // 0x1C
    matrix orient; // 0x20
    float steering_max_angle; // 0x44
    float steering_max_angle_speed; // 0x48
    vector bbox_min; // 0x4C
    vector bbox_max; // 0x58
    bool moving_in_reverse; // 0x64
    bool is_tank; // 0x65
};

// size 0x20
class route_position_info
{
public:
    route_position_info(const vehicle_route_info&, float, vector*);

    const vehicle_route_info& route; // 0x00
    float distance_along_route; // 0x04
    vector* target_pos; // 0x08
    struct
    {
        unsigned int add_intermediate_points : 1; // 0x00:0
    } flags; // 0x0C
    vector* intermediate_points; // 0x10
    float intermediate_offset_dist; // 0x14
    int max_intermediate_points; // 0x18
    int num_intermediate_points; // 0x1C
};

// size 0x8
struct lookahead_entry
{
    float lookahead_dist; // 0x00
    float lookahead_curv; // 0x04
};

// size 0xA8
struct vehicle_lookahead_info
{
    int num_lookahead_points; // 0x00
    float max_lookahead_dist; // 0x04
    lookahead_entry entries[20]; // 0x08
};

// size 0x4
struct vehicle_ai_flags
{
    unsigned char m_ignore_repulsors : 1; // 0x00:0
    unsigned char m_goal_stop_override : 1; // 0x00:1
    unsigned char m_override_speed : 1; // 0x00:2
    unsigned char m_override_stop : 1; // 0x00:3
    unsigned char m_force_stop_reverse : 1; // 0x00:4
    unsigned char m_follow_pathfinding : 1; // 0x00:5
    unsigned char m_lateral_offset : 1; // 0x00:6
    unsigned char m_road_offset : 1; // 0x00:7
    unsigned char m_unstick_reverse : 1; // 0x01:0
    unsigned char m_jumping : 1; // 0x01:1
    unsigned char m_should_not_swerve : 1; // 0x01:2
    unsigned char m_ignore_lanes : 1; // 0x01:3
    unsigned char m_ignore_road_directionality : 1; // 0x01:4
    unsigned char left_side : 1; // 0x01:5
    unsigned char middle : 1; // 0x01:6
    unsigned char fallen_off_navmesh : 1; // 0x01:7
    unsigned char pull_off : 1; // 0x02:0
    unsigned char at_end_of_route : 1; // 0x02:1
    unsigned char use_special_roads : 1; // 0x02:2
    unsigned char need_to_rise : 1; // 0x02:3
    unsigned char obstructed_by_player : 1; // 0x02:4
    unsigned char u_turn_active : 1; // 0x02:5
    unsigned char slowing_for_u_turn : 1; // 0x02:6
    unsigned char turning : 1; // 0x02:7
    unsigned char turning_in_place : 1; // 0x03:0
    unsigned char backing_up : 1; // 0x03:1
    unsigned char is_courier : 1; // 0x03:2
    unsigned char is_riding_shotgun : 1; // 0x03:3
};

// size 0x8
struct vehicle_accel_info
{
    float desired_speed; // 0x00
    float desired_braking_decel; // 0x04
};

// size 0xC
struct vehicle_control_info
{
    float max_braking_decel; // 0x00
    float max_radial_accel; // 0x04
    float max_steer_curvature; // 0x08
};

// size 0x2C
struct stuck_info
{
    stuck_info(); // 0x00B645E0

    timestamp stuck_threshold_time; // 0x00
    timestamp stuck_active_time; // 0x04
    timestamp stuck_clear_time; // 0x08
    vector stuck_pos; // 0x0C
    timestamp extended_stuck_time; // 0x18
    float stuck_mass; // 0x1C
    timestamp stuck_mass_timer; // 0x20
    clear_debris_states clear_debris_state; // 0x24
    timestamp clear_debris_timer; // 0x28
};

// size 0x34
struct vehicle_steer_info
{
    vehicle_steer_info(); // 0x00B64600

    vector goal_pos; // 0x00
    float curvature_goal; // 0x0C
    float lookahead_dist; // 0x10
    float lateral_offset; // 0x14
    float road_offset; // 0x18
    float wheelbase; // 0x1C
    float inv_radius_steer; // 0x20
    float vel; // 0x24
    float desired_vel; // 0x28
    float accel_val; // 0x2C
    float brake_val; // 0x30
};

// size 0xC
struct vehicle_point_info
{
    int route_index; // 0x00
    float u; // 0x04
    float distance_remaining; // 0x08
};

// size 0x20
struct route_segment
{
    // size 0x1
    struct route_segment_flags
    {
        unsigned char terminal : 1; // 0x00:0
        unsigned char from_ambient_path : 1; // 0x00:1
        unsigned char from_navmesh_path : 1; // 0x00:2
        unsigned char signal_processed : 1; // 0x00:3
        unsigned char stopped_for_sign : 1; // 0x00:4
    };

    route_segment(); // 0x00B64630

    cubic_bezier_spline cb_spline; // 0x00
    float length; // 0x10
    float speed_limit; // 0x14
    lane_spline_base* base_spline; // 0x18
    short num_knots_minus_one; // 0x1C
    route_segment_flags flags; // 0x1E
};

// size 0xC0
struct vehicle_route_info
{
    vehicle_route_info(); // 0x00B768B0

    route_segment route[5]; // 0x00
    const lane_spline_base* last_next_spline; // 0xA0
    vehicle_point_info center_info; // 0xA4
    vehicle_point_info bumper_info; // 0xB0
    short num_route_segments; // 0xBC
    struct
    {
        unsigned char m_route_segment_inited : 1; // 0x00:0
    } flags; // 0xBE
};

// size 0x40
struct vehicle_path_info
{
    // size 0x2
    struct vehicle_path_flags
    {
        unsigned char jumping : 1; // 0x00:0
        unsigned char on_bridge : 1; // 0x00:1
        unsigned char partial_path : 1; // 0x00:2
        unsigned char road_spline_path : 1; // 0x00:3
        unsigned char off_road : 1; // 0x00:4
        unsigned char use_road_offset : 1; // 0x00:5
        unsigned char gps_path : 1; // 0x00:6
        unsigned char next_point_inc : 1; // 0x00:7
        unsigned char ixc_can_control : 1; // 0x01:0
        unsigned char ixc_turning : 1; // 0x01:1
        unsigned char ixc_turning_right : 1; // 0x01:2
    };

    vehicle_path_info(); // 0x005DCCB0 (.h:203)

    vector start_pos; // 0x00
    uint64_t start_nav_cell_handle; // 0x10
    vector end_pos; // 0x18
    uint64_t end_nav_cell_handle; // 0x28
    path_outcome error_code; // 0x30
    unsigned int closest_road_spline; // 0x34
    float closest_road_index; // 0x38
    vehicle_path_flags flags; // 0x3C
};

// size 0x1C
struct vehicle_pathfind_record
{
    vehicle_pathfind_record(const vector& start_, const vector& dest_, vehicle_pathfind_results result_); // 0x00BC2E30 (.h:224)
    vehicle_pathfind_record(); // 0x00B62F00 (.h:223)

    vector start; // 0x00
    vector dest; // 0x0C
    vehicle_pathfind_results result; // 0x18
};

// size 0x338
class vehicle_ai
{
public:
    void set_state(vehicle_ai_state _state); // 0x00BC9CA0 (:4086)
    vehicle_ai_state get_state() const; // 0x00B62F10 (.h:667)
    void get_state(vehicle_ai_state&) const;
    vehicle_ai_state get_last_state() const;
    vehicle_ai(); // 0x00B8B230
    ~vehicle_ai(); // 0x00B729C0

private:
    vehicle_ai_state ai_state; // 0x00
    vehicle_ai_state ai_last_state; // 0x04
public:
    timestamp u_turn_global_delay; // 0x08
    vehicle_ai_flags vai_flags; // 0x0C
    float target_speed_to_goal; // 0x10
    float target_decel_to_goal; // 0x14
    float override_speed; // 0x18
    float initial_forward_speed; // 0x1C
    vehicle_repulsor_info repulsor_info; // 0x20
    timestamp navmesh_path_update; // 0xD0
    timestamp swerve_timer; // 0xD4
    timestamp initial_swerve_timer; // 0xD8
    timestamp pull_off_timer; // 0xDC
    stuck_info stuck; // 0xE0
    unsigned int vehicle_handle; // 0x10C
    timestamp checkpoint_timer; // 0x110
    unsigned int checkpoint_handle; // 0x114
    unsigned int ambient_wait_object; // 0x118
    timestamp ambient_wait_timer; // 0x11C
    timestamp change_sides_timer; // 0x120
    vehicle_steer_info steer_info; // 0x124
    vehicle_accel_info accel_info; // 0x158
    vehicle_control_info base_control_info; // 0x160
    vehicle_control_info active_control_info; // 0x16C
    navmesh_lane_spline_node* navmesh_path; // 0x178
    vehicle_route_info route_info; // 0x17C
    vehicle_path_info current_path_info; // 0x240
    float flying_offset; // 0x280
    float building_offset; // 0x284
    float desired_flying_offset; // 0x288
    float target_offset; // 0x28C
    float terrain_offset; // 0x290
    float original_offset; // 0x294
    float necessary_height; // 0x298
    int update_failures; // 0x29C
    unsigned short active_intersection_controller; // 0x2A0
    circ_array<vehicle_pathfind_record,5> pf_failure_history; // 0x2A4
};

// size 0x18
struct vehicle_ambient_pathfind_info
{
    vehicle_ambient_pathfind_info(int _num_points, const vector* _points, bool _allow_uturn, const lane_spline* _force_start_lane); // 0x00BC3EC0 (:236)

    int num_points; // 0x00
    const vector* points; // 0x04
    bool allow_uturn; // 0x08
    const lane_spline* force_start_lane; // 0x0C
    bool supress_error; // 0x10
    bool uses_uturn; // 0x11
    bool uturn_left; // 0x12
    const lane_spline* start_lane; // 0x14
};

// size 0xC
class vehicle_ai_debug_info
{
public:
    vehicle_ai_debug_info(); // 0x00BC2E70 (.h:739)

    struct
    {
        unsigned char break_on_state_change : 1; // 0x00:0
        unsigned char show_vehicle_ai : 1; // 0x00:1
        unsigned char show_vehicle_path : 1; // 0x00:2
        unsigned char show_vehicle_lane : 1; // 0x00:3
        unsigned char show_vehicle_handles : 1; // 0x00:4
        unsigned char show_nearby_lanes : 1; // 0x00:5
        unsigned char show_state : 1; // 0x00:6
        unsigned char show_clump_lanes : 1; // 0x00:7
        unsigned char show_wait : 1; // 0x01:0
        unsigned char show_notoriety_spawn_pos : 1; // 0x01:1
    } flags; // 0x00
    unsigned int debug_vehicle_handle; // 0x04
    float lookahead_dist; // 0x08
};

void unused_dcf_vehicle_stop(); // 0x00BCF860 (:339)
void unused_dcf_vai(); // 0x00BD5CD0 (:429)
void vehicle_ai_get_route_segment_info(const vehicle_route_info& route_info, const route_segment*& start_segment, int& num_segments); // 0x00BC9010 (:836)
const route_segment* vehicle_ai_get_center_route_segment(const vehicle_route_info& route_info); // 0x00BC3F10 (:853)
const route_segment* vehicle_ai_get_next_route_segment(const vehicle_route_info& route_info); // 0x00BC3F50 (:871)
const route_segment* get_front_bumper_route_segment(const vehicle_route_info& route_info); // 0x00BC3F90 (:893)
void vehicle_ai_flush_route(vehicle_route_info& route_info); // 0x00BC3FC0 (:909)
void vehicle_ai_add_route_segment(vehicle_route_info& route_info, const lane_spline_base* lsp, bool ambient_path, bool navmesh_path, bool from_rails); // 0x00BCD500 (:924)
void vehicle_ai_free_all_paths(vehicle_ai* vai); // 0x00BC9070 (:992)
void vehicle_ai_clear_path_info(vehicle_ai* vai); // 0x00BC3FF0 (:999)
float get_spline_curvature(const cubic_bezier_spline& spline, float u); // 0x00BC4090 (:1103)
const lane_spline* vehicle_choose_next_lane_straight(const lane_spline* cur_lane); // 0x00BCF920 (:1435)
const lane_spline_base* vehicle_choose_next_lane_spline_random(vehicle_ai* vai, const lane_spline_base* cur_lsp, bool straight, bool no_raycast); // 0x00BD4890 (:1470)
bool vehicle_ai_find_approx_advanced_position_along_route(vector& spline_pos, const lane_spline_base*& out_lane, float& out_u, float& dist_remaining, const lane_spline_base** target_splines, int num_splines, float start_u, float start_dist, float advance_dist, bool road_spline_path, float road_offset); // 0x00BCFA00 (:1666)
void vehicle_ai_presim_process(); // 0x00BCD6F0 (:1822)
void vehicle_ai_get_collecter_base(vehicle* vp, vector& box_bmin, vector& box_bmax); // 0x00BC42A0 (:1900)
void vehicle_ai_get_collector_bbox(vehicle* vp, vector& box_bmin, vector& box_bmax, vector& box_pos, matrix& box_orient, bool forward); // 0x00BC4390 (:1919)
void get_cornering_target_speed_and_braking(const havok_vehicle_ai_info& info, vehicle_ai* vai, float& braking_decel, float& curve_speed); // 0x00BC4920 (:2199)
void vehicle_ai_get_curvature_speed_decel(vehicle_ai* vai, float& desired_speed, float& desired_braking_decel); // 0x00BC91C0 (:2264)
void vehicle_ai_set_accel_for_spline_path(vehicle_ai* vai, const havok_vehicle_ai_info& info, float target_speed, float target_braking_decel); // 0x00BCD700 (:2300)
bool process_u_turn_steering(vehicle_ai* vai, const havok_vehicle_ai_info& info); // 0x00BC4BA0 (:2501)
float get_steering_goal_dist(object_sub_type sub_type, float vehicle_speed); // 0x00BC4DA0 (:2564)
void get_steering_lateral_offset(const vehicle_ai* vai, const lane_spline_base* steering_spline, float steer_u, const vector& vehicle_pos, vector& steering_offset); // 0x00BCFC50 (:2580)
void get_steering_vertical_offset(vector& steering_offset, const vehicle* vp, const vehicle_ai* vai); // 0x00BC4E30 (:2623)
bool process_vehicle_steering(vehicle* vp, vehicle_ai* vai, const havok_vehicle_ai_info& info); // 0x00BCFF70 (:2639)
void vehicle_collect_objects_fast(const vector& bmin, const vector& bmax, const vector& bpos, const matrix& borient, const unsigned int collect_type, int& num_objects, int max_objects, unsigned int* object_handles, bool (*&cb)(object*, unsigned int, const void*), vehicle* vp); // 0x00BC9480 (:3075)
bool vehicle_in_water(const vehicle* vp); // 0x00BC4E80 (:3264)
void vehicle_ai_do_frame(vehicle* vp); // 0x00BD0F40 (:3274)
void vehicle_ai_init(vehicle* vp); // 0x00BCDB00 (:3330)
void vehicle_ai_debug_render(); // 0x00BC4E90 (:3521)
float vehicle_ai_get_curvature_to_point(const vector& start_pos, const matrix& orient, const vector& goal_pos); // 0x00BC4EA0 (:3929)
void vehicle_ai_steer_toward_goal(vehicle_ai* vai, const havok_vehicle_ai_info& info); // 0x00BC9700 (:3954)
void vehicle_ai_accel_toward_goal(vehicle_ai* vai, const havok_vehicle_ai_info& self_info); // 0x00BCDC80 (:4041)
void vehicle_ai_set_state(vehicle_ai* vai, vehicle_ai_state state); // 0x00BCDD90 (:4076)
void vehicle_ai_free_resources(vehicle_ai* vai); // 0x00BCDDC0 (:4178)
bool vehicle_ai_process_route(vehicle_ai* vai, vehicle_route_info& route_info, const havok_vehicle_ai_info& info, bool primary, bool flying); // 0x00BD4DB0 (:4312)
float vehicle_ai_get_dist_remaining_on_route_segment(const route_segment& segment, const vector& pos); // 0x00BC5170 (:4444)
float vehicle_ai_get_dist_remaining_on_route(const vehicle_route_info& route_info); // 0x00BC51B0 (:4463)
bool vehicle_ai_is_at_end_of_line(vehicle_ai* vai); // 0x00BC52A0 (:4479)
void vehicle_ai_update_route_segment_info(vehicle_route_info& route_info, const int* segment_search_order, int num_segments, vehicle_point_info& info, const vector& pos, bool use_road_offset, float road_offset, bool flying); // 0x00BC5300 (:4498)
void vehicle_ai_track_player(); // 0x00BC5410 (:4664)
void vehicle_ai_set_accel_for_speed(vehicle_ai* vai, float desired_speed, float cur_speed, bool no_limit); // 0x00BC9DA0 (:4735)
bool vehicle_ai_does_lane_exist_at_position(const vector& pos); // 0x00BCDEC0 (:4775)
lane_path_node* vehicle_create_ambient_path_from_points(vehicle_ambient_pathfind_info& pathfind_info); // 0x00BD5050 (:4792)
void vehicle_ai_system_init(); // 0x00BD6880 (:4965)
void vehicle_ai_level_init_pre(); // 0x00BC9FF0 (:4993)
void vehicle_ai_get_advanced_spline_pos(vehicle_ai* vai, float advance_length, vector& closest_spline_pos); // 0x00BD0F80 (:5023)
bool vehicle_retains_collision_control(const vehicle* vp); // 0x00BC5420 (:5088)
void vehicle_ai_set_speed_override(vehicle_ai* vai, float speed_meter_per_sec); // 0x00BC5440 (:5120)
void vehicle_ai_clear_speed_override(vehicle_ai* vai); // 0x00BC5460 (:5136)
void vehicle_ai_inform_dead_passenger(unsigned int vehicle_handle, unsigned int attacker_handle); // 0x00BCA020 (:5170)
bool vehicle_ai_begin_script_ambient_pathfind_to(vehicle* vp, vector* points, int num_points, bool stop_at_goal, bool allow_start_uturn, bool supress_error); // 0x00BD5350 (:5210)
bool vehicle_ai_begin_script_navmesh_rail_path(vehicle* vp, vector* points, int num_points, bool allow_start_uturn, bool stop_at_goal, bool supress_error); // 0x00BD5440 (:5281)
bool vehicle_ai_begin_script_navmesh_pathfind_to(vehicle* vp, const navpoint** points, int num_points, bool loop, bool stop_at_goal); // 0x00BC5490 (:5341)
void vehicle_ai_set_lateral_offset(vehicle_ai* vai, float lateral_offset); // 0x00BC54A0 (:5360)
void vehicle_ai_clear_lateral_offset(vehicle_ai* vai); // 0x00BC54C0 (:5373)
void vehicle_ai_set_road_offset(vehicle_ai* vai, float road_offset); // 0x00BC54D0 (:5383)
void vehicle_ai_clear_road_offset(vehicle_ai* vai); // 0x00BC54F0 (:5394)
void vehicle_ai_set_ignore_lanes(vehicle_ai* vai, bool ignore_lanes); // 0x00BC5500 (:5405)
bool vehicle_ai_is_vehicle_stuck(vehicle_ai* vai); // 0x00BC5520 (:5421)
void vehicle_ai_force_enter_stuck(vehicle* vp); // 0x00BC5560 (:5431)
bool maybe_exit_on_extended_stuck(vehicle* vp); // 0x00BC55D0 (:5449)
bool vehicle_ai_process_stuck(vehicle* vp, bool allow_enter_stuck); // 0x00BCDEF0 (:5496)
void vehicle_ai_call_exit_state(vehicle_ai* vai, vehicle_ai_state state); // 0x00BC55E0 (:6007)
bool vehicle_ai_is_scripted_unstoppable_vehicle(const vehicle* vp); // 0x00BC5600 (:6018)
void notify_vehicle_vehicle_contact_started(vehicle* vp1, vehicle* vp2); // 0x00BC5610 (:6047)
lane_path_node* get_ambient_path_from_route(const vehicle_route_info& route); // 0x00BC5620 (:6188)
position_on_route_result find_position_fixed_distance_along_route_beyond_center(route_position_info& position_info); // 0x00BD1360 (:6248)
bool vehicle_continuing_along_highway(const vehicle* vp); // 0x00BC5690 (:6313)
bool vehicle_ai_has_cached_pf_failure(const vehicle_ai* vai, const vector& start, const vector& dest, vehicle_pathfind_results& reason); // 0x00BCA130 (:6374)
void vehicle_ai_record_pf_failure(vehicle_ai* vai, const vector& start, const vector& dest, vehicle_pathfind_results failure_reason); // 0x00BCA160 (:6391)

// 0x00B62F10 (.h:667)
inline vehicle_ai_state vehicle_ai::get_state() const
{
    return ai_state;
}

// 0x00B64200
inline havok_vehicle_ai_info::havok_vehicle_ai_info()
{
}
