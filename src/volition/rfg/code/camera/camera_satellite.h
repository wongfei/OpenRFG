// Original: volition/rfg/code/camera/camera_satellite.h
// Implementation: volition/rfg/code/camera/camera_satellite.cpp
//
// The satellite camera mode: the overhead view of the satellite targeting (air strikes), with the data the HUD shows.
// Placement of camera_satellite_hud_data inferred.
#pragma once

#include "volition/rfg/code/control/control_map.h"
#include "volition/vlib/code/math/vector2.h"

class vector;

// size 0x10
struct camera_satellite_hud_data
{
    camera_satellite_hud_data(); // 0x008987C0

    vector2 ticker_pct; // 0x00
    bool enemy_target_reticule; // 0x08
    bool reloading; // 0x09
    bool show_hints; // 0x0A
    bool can_fire; // 0x0B
    bool acquiring_target; // 0x0C
    bool shutting_down; // 0x0D
};

float camera_satellite_get_gyro_pitch(); // 0x006C66F0 (:167)
float camera_satellite_get_gyro_heading(); // 0x006C6700 (:172)
void unused_dcf_satellite_test(); // 0x006D4120 (:178)
void unused_dcf_satellite_test_stop(); // 0x006D4200 (:194)
bool allowed_to_fire(); // 0x006C6920 (:260)
void start_return_to_target_sound(); // 0x006C6950 (:270)
void stop_return_to_target_sound(); // 0x006C6980 (:280)
void camera_satellite_mode_init(); // 0x006C69B0 (:293)
void camera_start_satellite_mode(); // 0x006CCB90 (:298)
void camera_stop_satellite_mode(); // 0x006CCDB0 (:392)
unsigned int camera_satellite_get_target(); // 0x006C69D0 (:402)
void camera_satellite_set_target(unsigned int target_handle); // 0x006C69E0 (:407)
void camera_satellite_set_max_range(float max_range); // 0x006C6A40 (:421)
void camera_satellite_set_weapon_source_pos(const vector& weapon_source_pos); // 0x006C6A70 (:429)
void camera_satellite_fire_weapon(); // 0x006D5D00 (:434)
void camera_satellite_control(float vert_input, float horz_input, control_axis_action h_action, control_axis_action v_action); // 0x006C6A90 (:493)
void camera_satellite_process(float step_time, bool last_step_of_frame); // 0x006DA7A0 (:526)
void camera_satellite_hold_position_this_frame(); // 0x006C6B80 (:841)
void camera_satellite_return_to_target(); // 0x006C6BA0 (:852)
void camera_satellite_shutdown(); // 0x006C6C20 (:868)
void camera_satellite_system_init(); // 0x006C6C30 (:873)
void camera_satellite_level_init(); // 0x006C6C70 (:885)
void camera_satellite_get_hud_data(camera_satellite_hud_data& data); // 0x006D6470 (:923)
bool camera_satellite_get_hud_target(bool& show, vector2& fire_pos); // 0x006C6CB0 (:964)
