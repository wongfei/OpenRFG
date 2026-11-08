// Original: volition/rfg/code/camera/camera_free.h
// Implementation: volition/rfg/code/camera/camera_free.cpp
//
// The free camera mode: the player's normal camera (on foot, in vehicles and turrets), with submodes blended by their
// parameters and the vehicle/tank control types. Placement of the parameters and enums inferred.
#pragma once

#include "volition/rfg/code/camera/camera_lookaround.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/vlib/code/math/vector3.h"

class object;
struct vehicle_camera_settings;

enum Camera_free_control_types
{
    CAMERA_FREE_CONTROL_TYPE_NORMAL = 0,
    CAMERA_FREE_CONTROL_TYPE_PREDICTIVE = 1,
    CAMERA_FREE_CONTROL_TYPE_TREAD = 2,
    CAMERA_FREE_CONTROL_TYPE_TREAD_CAM_REL = 3,
    CAMERA_FREE_CONTROL_TYPE_COUNT = 4,
};

// The vehicle camera's mouse control: the camera's own turn input, or the mouse moving an absolute offset.
enum vehicle_camera_control_mode
{
    VCCM_DEFAULT = 0,
    VCCM_ALTERNATE1 = 1,
    VCCM_COUNT = 2,
};

enum free_camera_submodes
{
    FREE_CAMERA_SUBMODE_INTERIOR = 0,
    FREE_CAMERA_SUBMODE_INTERIOR_COMBAT = 1,
    FREE_CAMERA_SUBMODE_INTERIOR_CLOSE = 2,
    FREE_CAMERA_SUBMODE_EXTERIOR = 3,
    FREE_CAMERA_SUBMODE_EXTERIOR_CROUCH = 4,
    FREE_CAMERA_SUBMODE_EXTERIOR_STAND = 5,
    FREE_CAMERA_SUBMODE_VEHICLE_DRIVER = 6,
    FREE_CAMERA_SUBMODE_VEHICLE_CLOSE = 7,
    FREE_CAMERA_SUBMODE_VEHICLE_PASSENGER = 8,
    FREE_CAMERA_SUBMODE_VEHICLE_EXTERIOR = 9,
    FREE_CAMERA_SUBMODE_BIRDS_EYE_DEBUG = 10,
    FREE_CAMERA_SUBMODE_ZOOM = 11,
    FREE_CAMERA_SUBMODE_SPECTATOR = 12,
    NUM_FREE_CAMERA_SUBMODES = 13,
};

// size 0x74
struct free_submode_params
{
    float xz_dist; // 0x00
    float y_dist; // 0x04
    float lookat_offset_x; // 0x08
    float lookat_offset_y; // 0x0C
    float lookat_offset_z; // 0x10
    float lookat_offset_y_delta; // 0x14
    float min_elev_angle_rad; // 0x18
    float max_elev_angle_rad; // 0x1C
    float free_fov; // 0x20
    camera_swoop_params fp_swoop_params; // 0x24
    float base_pitch; // 0x64
    float dist_from_lookat; // 0x68
    float y_axis_rotation_speed; // 0x6C
    int vehicle_camera_selection; // 0x70
};

// size 0xDC
struct free_mode_params
{
    free_mode_params(); // 0x006C8ED0

    free_submode_params* submode_params; // 0x00
    free_submode_params* cur_interior_params; // 0x04
    free_submode_params* cur_exterior_params; // 0x08
    free_submode_params* cur_vehicle_params; // 0x0C
    Camera_free_control_types control_type; // 0x10
    vector last_lookat_pos; // 0x14
    float suppressed_target_lag_pct; // 0x20
    float user_rot; // 0x24
    float auto_rot; // 0x28
    float auto_rot_vel; // 0x2C
    float user_rot_vel; // 0x30
    float user_elev; // 0x34
    float last_elev_angle; // 0x38
    float last_vehicle_pitch; // 0x3C
    float desired_elev_angle; // 0x40
    float control_multiplier; // 0x44
    float pitch_drift_vel; // 0x48
    float heading_drift_vel; // 0x4C
    float camera_swing_rate; // 0x50
    vector debug_offset; // 0x54
    float swing_percent; // 0x60
    float swing_time; // 0x64
    float swing_start_heading; // 0x68
    float swing_goal_heading; // 0x6C
    vector bash_heading_dir; // 0x70
    vector bash_heading_start_dir; // 0x7C
    float bash_heading_percent; // 0x88
    float bash_heading_time; // 0x8C
    float cur_dist_from_lookat; // 0x90
    float goal_dist_from_lookat; // 0x94
    float steering_angle; // 0x98
    float last_gyro_heading_offset; // 0x9C
    float last_gyro_pitch_offset; // 0xA0
    vector last_gyro_offset; // 0xA4
    float last_mouse_heading_offset; // 0xB0
    float last_mouse_pitch_offset; // 0xB4
    vector last_mouse_offset; // 0xB8
    vector mouse_absolute_pos; // 0xC4
    control_axis_action last_h_action; // 0xD0
    control_axis_action last_v_action; // 0xD4
    struct
    {
        unsigned char target_moved_last_frame : 1; // 0x00:0
        unsigned char suppress_target_lag : 1; // 0x00:1
        unsigned char swinging_to_dir : 1; // 0x00:2
        unsigned char elev_relaxing : 1; // 0x00:3
        unsigned char heading_is_bashed : 1; // 0x00:4
        unsigned char unresolvable_obstruction : 1; // 0x00:5
        unsigned char debug_mode : 1; // 0x00:6
        unsigned char look_back : 1; // 0x00:7
        unsigned char look_back_last : 1; // 0x01:0
    } flags; // 0xD8
};

extern free_submode_params Free_camera_submode_params[NUM_FREE_CAMERA_SUBMODES]; // 0x016481E8
extern vehicle_camera_control_mode Camera_free_mouse_control_mode; // 0x01DE48B0

void unused_dcf_vehicle_camera_type(); // 0x006C4230 (:376)
void unused_dcf_tank_camera_type(); // 0x006C4320 (:404)
void unused_dcf_cam_heading_stop_time(); // 0x006C4410 (:467)
void unused_dcf_cam_config(); // 0x006CABE0 (:484)
void unused_dcf_cam_fov(); // 0x006CAEC0 (:559)
void unused_dcf_sniper_zoom_min(); // 0x006C4470 (:624)
void unused_dcf_birds_eye_cam(); // 0x006CAF30 (:672)
bool camera_free_submodes_are_equivalent(free_submode_params* params, free_submode_params* desired_params); // 0x006C44C0 (:1515)
void camera_free_system_init(); // 0x006C45B0 (:1548)
void camera_free_level_init_post(); // 0x006D5010 (:1561)
void camera_free_mode_init(); // 0x006D5610 (:1604)
void camera_free_process(float step_time, bool last_step_of_frame); // 0x006D9780 (:1630)
vector camera_free_get_lookat(float pitch); // 0x006D17D0 (:1808)
vector camera_free_get_default_pos(); // 0x006D1CD0 (:1907)
void camera_free_set_rotation(float rot, control_axis_action h_action); // 0x006CAFB0 (:1939)
void camera_free_set_elevation(float elev, control_axis_action v_action); // 0x006CB1A0 (:1988)
void camera_free_set_control_multiplier(float multiplier); // 0x006C4640 (:2041)
void camera_free_set_drift(float pitch, float heading); // 0x006C4650 (:2051)
void camera_free_set_target(object* objp); // 0x006D5BA0 (:2059)
void camera_free_suppress_orientation_lag(); // 0x006C4670 (:2086)
void camera_free_set_submode(free_camera_submodes submode, float smooth, float transition_time); // 0x006CB3B0 (:2097)
free_camera_submodes camera_free_get_submode(); // 0x006C4680 (:2128)
void camera_free_set_vehicle_submode_params(float follow_distance, float follow_height, const vector& lookat_offset); // 0x006C46A0 (:2139)
void camera_free_swing_to_dir(const vector& to_target, float swing_time_sec); // 0x006C4810 (:2170)
void camera_free_update_swing_to_dir(const vector& to_target); // 0x006C49F0 (:2215)
bool camera_free_is_swinging(); // 0x006C4C10 (:2264)
void camera_free_set_swing_rate(float csr); // 0x006C4C30 (:2271)
void camera_free_reset_pitch(); // 0x006C4C40 (:2280)
void camera_free_bash_heading(const vector& to_target, float time_sec); // 0x006C4C80 (:2293)
void camera_free_set_snap_time(int snap_time_ms); // 0x006C4D40 (:2310)
void camera_free_clear_snap_time(); // 0x006C4D80 (:2324)
void camera_free_stop_bashing_heading(float time_sec, bool swing_to_front); // 0x006C4DA0 (:2336)
void camera_free_render_debug(); // 0x006C4DE0 (:2352)
void camera_free_toggle_close(); // 0x006CB490 (:2361)
void camera_free_recalc_vehicle_submode_params(); // 0x006C4E30 (:2407)
void camera_free_reset(); // 0x006C4F10 (:2422)
bool camera_free_modify_fov(float fov_delta); // 0x006C4FF0 (:2459)
void camera_free_reset_fov(); // 0x006C5290 (:2490)
float camera_free_get_fov_ratio(); // 0x006C52B0 (:2501)
void camera_free_set_fov(float new_fov); // 0x006C52D0 (:2510)
void camera_free_cycle_camera(); // 0x006D5CA0 (:2519)
void camera_free_update_vehicle_camera_settings(); // 0x006D22F0 (:2536)
const vehicle_camera_settings& camera_free_get_vehicle_camera_settings(); // 0x006CB560 (:2588)
void camera_free_change_vehicle_control_type(Camera_free_control_types control_type); // 0x006CB5C0 (:2601)
Camera_free_control_types camera_free_get_vehicle_control_type(); // 0x006C52F0 (:2616)
void camera_free_set_tank_control_type(Camera_free_control_types control_type); // 0x006CB610 (:2621)
Camera_free_control_types camera_free_get_tank_control_type(); // 0x006C5300 (:2636)
