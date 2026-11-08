// Original: volition/vlib/code/input/input.h
// Implementation: volition/vlib/code/input/input.cpp
//
// vlib controller input on top of keen input (decompiled behaviour):
// - keen::rfg::Application::processInputEvent (main thread) writes raw state into
//   Cached_input_states[Next_cached_input_state] (input_internal.h).
// - input_poll (sim thread, via controls_poll) copies that buffer over the current one and swaps the indices, so the
//   sim reads a stable snapshot while the main thread keeps writing. It then copies buttons, sticks and gyros into
//   Controllers[0], clears all masks, evaluates stick "scrolls", derives just_pressed / just_released / repeated
//   per button from Global_time, and accumulates idle_time while nothing is pressed.
// Only one controller is supported (controller_id 0). Button ids index controller::buttons[128], joy ids
// controller::joys[32].
#pragma once

#include "keen/input/inputevents.hpp"

namespace keen
{
    struct InputSystem;
}

enum gyro_sensor
{
    GYRO_ANGULAR_VELOCITY_X = 0,
    GYRO_ANGULAR_VELOCITY_Y = 1,
    GYRO_ANGULAR_VELOCITY_Z = 2,
    GYRO_ORIENTATION_X = 3,
    GYRO_ORIENTATION_Y = 4,
    GYRO_ORIENTATION_Z = 5,
    GYRO_ORIENTATION_W = 6,
    NUM_GYRO_SENSORS = 7,
};

enum gyro_mode
{
    GYRO_MODE_THIRD_PERSON = 0,
    GYRO_MODE_VEHICLE = 1,
    GYRO_MODE_TURRET = 2,
    GYRO_MODE_SATELLITE = 3,
    NUM_GYRO_MODES = 4,
};

enum gyro_camera_axis
{
    GYRO_CAMERA_AXIS_X = 0,
    GYRO_CAMERA_AXIS_Y = 1,
    NUM_GYRO_CAMERA_AXES = 2,
};

// size 0x8
struct gyro_axis_config
{
    gyro_axis_config(gyro_sensor sensor_id, float direction); // 0x00566BB0 (.h:203)
    gyro_axis_config(); // 0x00753D50 (.h:199)
    bool is_equal(const gyro_axis_config& other) const; // 0x007B9A00 (.h:210)

    gyro_sensor sensor_id; // 0x00
    float direction; // 0x04
};

extern keen::InputSystem* Keen_input_system; // 0x01CE86C0, set by keen::rfg::Application::initialize
extern bool Input_playing_with_controller; // 0x01CE86C4
extern keen::ControllerType Input_controller_type; // 0x01CE86C8
extern bool Input_all_input_masked; // 0x01CE86C6

void input_init(); // 0x005671E0 (:109)
void input_system_shutdown(); // 0x005672E0 (:129)
void input_poll(); // 0x00599B30 (:151)
bool input_is_connected(unsigned int controller_id); // 0x005673C0 (:316)
void input_flush(unsigned int controller_id); // 0x005673E0 (:328)
float input_gyro_get_raw_value(gyro_sensor sensor_id, unsigned int controller_id); // 0x00567400 (:335)
float input_gyro_get_camera_orientation_value(gyro_mode mode, gyro_camera_axis axis, unsigned int controller_id); // 0x00567420 (:340)
float input_gyro_get_camera_angular_velocity_value(gyro_mode mode, gyro_camera_axis axis, unsigned int controller_id); // 0x00567480 (:348)
void input_get_joy(float& x, float& y, unsigned int controller_id, unsigned int joy_id); // 0x005674E0 (:366), deadzone (per axis, or radial when Input_deadzone_square) and masks applied
void input_get_joy_raw(float& x, float& y, unsigned int controller_id, unsigned int joy_id); // 0x005678E0 (:463)
void input_get_x_sensor_raw(float& x, unsigned int controller_id); // 0x00567950 (:500)
void input_get_y_sensor_raw(float& y, unsigned int controller_id); // 0x00567960 (:516)
void input_get_z_sensor_raw(float& z, unsigned int controller_id); // 0x00567970 (:532)
void input_get_yaw_sensor_raw(float& yaw, unsigned int controller_id); // 0x00567980 (:548)
void input_get_roll(float& roll, unsigned int controller_id); // 0x00567990 (:564)
void input_get_pitch(float& pitch, unsigned int controller_id); // 0x005679A0 (:580)
bool input_did_scroll(unsigned int controller_id, unsigned int joy_id, unsigned int which_dir); // 0x005679B0 (:597)
bool input_is_pressed(unsigned int controller_id, unsigned int button_id); // 0x005679E0 (:615)
bool input_just_pressed(unsigned int controller_id, unsigned int button_id); // 0x00567A50 (:636)
bool input_just_released(unsigned int controller_id, unsigned int button_id); // 0x00567A90 (:656)
bool input_just_released_no_clear(unsigned int controller_id, unsigned int button_id); // 0x00567AC0 (:673)
bool input_did_repeat(unsigned int controller_id, unsigned int button_id); // 0x00567B60 (:696)
float input_get_value(unsigned int controller_id, unsigned int button_id); // 0x00567BA0 (:715)
float input_get_last_raw_value(unsigned int controller_id, unsigned int button_id); // 0x00567CA0 (:744)
float input_get_raw_value(unsigned int controller_id, unsigned int button_id); // 0x00567CE0 (:765)
void input_process_controller(void (*btn_callback)(unsigned int, float), unsigned int controller_id); // 0x00567D30 (:789)
void input_invert_x(unsigned int controller_id, unsigned int joy_id); // 0x00567DD0 (:806)
void input_set_invert_x(unsigned int controller_id, unsigned int joy_id, bool value); // 0x00567E00 (:813)
bool input_get_invert_x(unsigned int controller_id, unsigned int joy_id); // 0x00567E20 (:820)
bool input_get_invert_y(unsigned int controller_id, unsigned int joy_id); // 0x00567E50 (:827)
void input_invert_y(unsigned int controller_id, unsigned int joy_id); // 0x00567E80 (:839)
void input_set_invert_y(unsigned int controller_id, unsigned int joy_id, bool value); // 0x00567EB0 (:846)
void input_set_deadzone(unsigned int controller_id, unsigned int button_id, float new_zone); // 0x00567ED0 (:860)
float input_get_deadzone(unsigned int controller_id, unsigned int button_id); // 0x00567F00 (:876)
void input_reset_deadzone(unsigned int controller_id, unsigned int button_id); // 0x00567F20 (:889)
void input_set_btn_repeat_delay(unsigned int controller_id, unsigned int button_id, unsigned int new_delay); // 0x00567F40 (:903)
unsigned int input_get_btn_repeat_delay(unsigned int controller_id, unsigned int button_id); // 0x00567F60 (:917)
void input_set_btn_repeat_rate(unsigned int controller_id, unsigned int button_id, unsigned int new_rate); // 0x00567F80 (:930)
unsigned int input_get_btn_repeat_rate(unsigned int controller_id, unsigned int button_id); // 0x00567FA0 (:944)
void input_set_feedback(unsigned int controller_id, float left_motor, float right_motor); // 0x00567FC0 (:957)
void input_mask_button(unsigned int controller_id, unsigned int button_id); // 0x00568030 (:975)
void input_mask_joy_axis(unsigned int controller_id, unsigned int joy_id, unsigned int axis); // 0x00568060 (:982)
void input_mask_all(); // 0x005680A0 (:1000)
bool input_is_all_masked(); // 0x005680E0 (:1021)
void input_clear_all_masks(); // 0x005680F0 (:1026)
bool input_get_vibration(); // 0x00568120 (:1042)
void input_set_vibration(bool on); // 0x00568130 (:1047)
bool input_get_vibration_unwanted(); // 0x00568180 (:1060)
void input_set_vibration_unwanted(bool unwanted); // 0x00568190 (:1065), rfg_do_frame sets it while the window is in the background
bool input_get_gyro_enabled(gyro_mode mode); // 0x005681E0 (:1077)
void input_set_gyro_enabled(gyro_mode mode, bool enabled); // 0x005681F0 (:1082)
float input_get_gyro_sensitivity(gyro_mode mode, gyro_camera_axis axis); // 0x00568200 (:1087)
void input_set_gyro_sensitivity(gyro_mode mode, gyro_camera_axis axis, float sensitivity); // 0x00568220 (:1092)
void input_reset_gyro_orientation_origin(); // 0x00568240 (:1097)
const gyro_axis_config& input_gyro_get_orientation_config(gyro_camera_axis axis); // 0x00568260 (:1104)
void input_gyro_set_orientation_config(gyro_camera_axis axis, const gyro_axis_config& config); // 0x00568270 (:1109)
const gyro_axis_config& input_gyro_get_angular_velocity_config(gyro_camera_axis axis); // 0x00568290 (:1114)
void input_gyro_set_angular_velocity_config(gyro_camera_axis axis, const gyro_axis_config& config); // 0x005682A0 (:1119)
float input_get_idle_time(); // 0x005682C0 (:1126)
void input_idle_pause(bool _pause); // 0x00568300 (:1142)
bool input_any_controller_active(); // 0x00568310 (:1149)
void input_apply_deadzone(float deadzone, float& x, float& y); // 0x00568330 (:1161)
void input_update_idle_time(); // 0x0059A080 (:1217)
void input_apply_axis_deadzone(float deadzone, float& value); // 0x005686F0 (:1245)
bool input_has_vibration(); // 0x005687B0 (:1254)
bool input_has_controller(); // 0x005687C0 (:1259)
void input_set_playing_with_controller(bool with_controller, keen::ControllerType controller_type); // 0x005687E0 (:1265)
bool input_get_playing_with_controller(); // 0x00568800 (:1271)
keen::ControllerType input_get_controller_type(); // 0x00568810 (:1276)
