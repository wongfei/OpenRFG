// Original: volition/rfg/code/camera/camera_lookaround.h
// Implementation: volition/rfg/code/camera/camera_lookaround.cpp
//
// The lookaround camera mode: an orbit around the player, entered and left with an optional swoop along a spline.
// Placement of the swoop and lookaround parameters inferred.
#pragma once

// size 0x40
struct camera_swoop_params
{
    float swoop_in_speed; // 0x00
    float in_handle_lengths[3]; // 0x04
    float out_handle_lengths[3]; // 0x10
    float in_handle_lengths_swing[3]; // 0x1C
    float out_handle_lengths_swing[3]; // 0x28
    float behind_head_dist; // 0x34
    float swing_factor_x; // 0x38
    float swing_factor_z; // 0x3C
};

// size 0x44
struct lookaround_mode_params
{
    float rot_p; // 0x00
    float rot_h; // 0x04
    float pitch; // 0x08
    float pitch_base; // 0x0C
    float prev_pitch; // 0x10
    float heading; // 0x14
    float heading_base; // 0x18
    float prev_heading; // 0x1C
    float rotvel_p; // 0x20
    float rotvel_h; // 0x24
    float rotvel_added_p; // 0x28
    float rotvel_added_h; // 0x2C
    float rot_p_multiplier; // 0x30
    float rot_h_multiplier; // 0x34
    bool ragdoll_cam; // 0x38
    float recoil_start_pitch; // 0x3C
    unsigned char over_the_shoulder : 1; // 0x40:0
    unsigned char recoiling : 1; // 0x40:1
};

void camera_lookaround_process(float step_time, bool last_step_of_frame); // 0x006D3D50 (:357)
void camera_start_lookaround_mode(bool swoop); // 0x006D3FE0 (:515)
void camera_stop_lookaround_mode(bool swoop); // 0x006D4080 (:560)
void camera_lookaround_set_rotation(float pitch, float heading); // 0x006C6520 (:587)
