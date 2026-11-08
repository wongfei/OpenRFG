// Original: volition/rfg/code/control/control_gameplay.h
// Implementation: volition/rfg/code/control/control_gameplay.cpp
//
// Gameplay controls: the player's input mapped into the simulation each frame (control_gameplay_process): the control
// permissions of the player state (standing, in a vehicle, on a ladder, ...), movement, camera controls, and the action
// button (just pressed, held, released). The per-state permission and movement helpers are file statics.
#pragma once

#include "volition/rfg/code/control/control_map.h"

class player;
class vector;

extern float Stick_h; // 0x01E25764, the movement stick (set by the player movement controls)
extern float Stick_v; // 0x01E2576C
extern float Stick_a; // 0x01E25770
extern bool Camera_auto_level; // 0x01E2A9C8, level the camera pitch while running (option)
extern bool Control_crouch_hold; // 0x0164AD31, crouch while the button is held (true) or toggle it
extern bool Control_zoom_hold; // 0x01E2A9C9, zoom while the button is held (option)

void control_gameplay_system_init(); // 0x006F7200 (:1904)
void control_gameplay_level_init_post(); // 0x006FDE10 (:1915)
bool control_gameplay_just_pressed_action(player* pp); // 0x006F7210 (:1922)
bool control_gameplay_continued_press_action(player* pp); // 0x006F7380 (:2030)
void control_gameplay_release_action(player* pp); // 0x006F7420 (:2133)
void control_gameplay_process(); // 0x00700C50 (:2171)
void control_gameplay_render(); // 0x006F7440 (:3087)
bool control_gameplay_general_controls_allowed(); // 0x006F7450 (:3124)
bool control_gameplay_on_foot_controls_allowed(); // 0x006F7460 (:3129)
bool control_gameplay_driving_controls_allowed(); // 0x006F7480 (:3134)
void control_gameplay_reset_movement_controls(); // 0x006F74A0 (:3141)
void control_gameplay_clear_crouch(); // 0x006F74C0 (:3150)
void calculate_normal_movement(player* pp, float forward, float sideways, vector& xz_fvec, vector& xz_rvec, bool set_turnto_target, vector& new_dest_point); // 0x006F74F0 (:3163)
void control_gameplay_enter(); // 0x006F7710 (:3216)
void control_gameplay_exit(); // 0x006F7720 (:3220)
control_button_action control_gameplay_get_action_button(); // 0x006F7730 (:3225)
void control_enable_player_input(bool enable); // 0x006F7770 (:3249)
void control_enable_player_camera_input(bool enable); // 0x006F7780 (:3254)
bool control_player_input_enabled(); // 0x006F7790 (:3259)
bool control_player_camera_input_enabled(); // 0x006F77A0 (:3264)
