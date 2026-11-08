// Original: volition/rfg/code/control/control_map.h
// Implementation: volition/rfg/code/control/control_map.cpp, control_map_bind_internal.h (control_config methods)
//
// Game actions on top of vlib input (decompiled behaviour). controls_poll runs once per sim frame from
// game_input_poll (and from rfg_init_stage_2_loop_update during boot):
//   1. picks the Steam controller action set: 0 on foot; vehicle sub_type 4 -> 1 (2 when vehicle_is_tank);
//      sub_types 5-6 -> 3; 4 outside GS_GAMEPLAY and for message boxes, cutscenes, scoreboard, spectating;
//   2. input_poll, key_poll, mouse_poll;
//   3. os_received_input();
//   4. Control_info[0].read_controls(0): reads sticks, mouse axes and buttons into control_info; cleared again
//      when multiplayer UI owns the input. The controls_* queries map actions to controls through the active
//      control_config.
// A "control" value encodes a keyboard key, mouse button or gamepad button/axis (see the encoding helpers).
// The UI parts (names, images, rebinding) are declared for completeness; they are outside the simulation scope.
#pragma once

#include <stdint.h>
#include "keen/input/inputsystem.hpp"

enum control_button_action
{
    CBA_BUTTON_INVALID = -1,
    CBA_GAC_WEAPON_MENU = 0,
    CBA_GAC_HOLD_OBJECT = 1,
    CBA_GAC_SCOREBOARD = 2,
    CBA_GAC_SWITCH_CLASS = 3,
    CBA_GAC_RESPAWN = 4,
    CBA_GAC_TOGGLE_BACKPACK_ACTIVATION = 5,
    CBA_GAC_VOTE_YES = 6,
    CBA_GAC_VOTE_NO = 7,
    CBA_GAC_HELP = 8,
    CBA_SPECTATOR_NEXT_TARGET = 9,
    CBA_SPECTATOR_PREV_TARGET = 10,
    CBA_SPECTATOR_LOCK_CAMERA = 11,
    CBA_SPECTATOR_UNLOCK_CAMERA = 12,
    CBA_SPECTATOR_NEXT_PLAYER = 13,
    CBA_SPECTATOR_PREV_PLAYER = 14,
    CBA_SPECTATOR_MODE_CUSTOM = 15,
    CBA_SPECTATOR_MODE_AQUARIUM = 16,
    CBA_SPECTATOR_MODE_ACTION = 17,
    CBA_SPECTATOR_MODE_PLAYER = 18,
    CBA_SPECTATOR_TOGGLE_HUD = 19,
    CBA_OFC_ATTACK_PRIMARY = 20,
    CBA_OFC_ATTACK_MELEE = 21,
    CBA_OFC_JUMP = 22,
    CBA_OFC_SPRINT = 23,
    CBA_OFC_CROUCH = 24,
    CBA_OFC_RELOAD = 25,
    CBA_OFC_ZOOM = 26,
    CBA_OFC_COVER = 27,
    CBA_OFC_ACTION = 28,
    CBA_OFC_CHARGE_DETONATE = 29,
    CBA_VDC_ACCELERATE = 30,
    CBA_VDC_BRAKE_REVERSE = 31,
    CBA_VDC_HANDBRAKE = 32,
    CBA_VDC_FIREARM = 33,
    CBA_VDC_CYCLE_CAMERA_MODE = 34,
    CBA_VDC_LOOK_BACK = 35,
    CBA_VDC_MECH_MELEE1 = 36,
    CBA_VDC_MECH_MELEE2 = 37,
    CBA_VDC_MECH_MELEE3 = 38,
    CBA_VDC_MECH_MELEE4 = 39,
    CBA_VDC_MECH_CHARGE = 40,
    CBA_VDC_TANK_FIRE_PRIMARY = 41,
    CBA_VDC_TANK_FIRE_SECONDARY = 42,
    CBA_VDC_ACTION = 43,
    CBA_VDC_TURRET_ZOOM = 44,
    CBA_VDC_CHANGE_SEAT = 45,
    CBA_SAT_ZOOM = 46,
    CBA_SAT_FIRE = 47,
    CBA_SAT_HOLD = 48,
    CBA_SAT_RETURN = 49,
    CBA_WEAPON_MENU_UP = 50,
    CBA_WEAPON_MENU_UP_DPAD = 51,
    CBA_WEAPON_MENU_DOWN = 52,
    CBA_WEAPON_MENU_DOWN_DPAD = 53,
    CBA_WEAPON_MENU_LEFT = 54,
    CBA_WEAPON_MENU_LEFT_DPAD = 55,
    CBA_WEAPON_MENU_RIGHT = 56,
    CBA_WEAPON_MENU_RIGHT_DPAD = 57,
    CBA_MAP = 58,
    CBA_MENU_UP_NO_STICK = 59,
    CBA_MENU_DOWN_NO_STICK = 60,
    CBA_MENU_UP = 61,
    CBA_MENU_DOWN = 62,
    CBA_MENU_LEFT = 63,
    CBA_MENU_LEFT_NO_STICK = 64,
    CBA_MENU_RIGHT = 65,
    CBA_MENU_RIGHT_NO_STICK = 66,
    CBA_MENU_ALT_UP = 67,
    CBA_MENU_ALT_DOWN = 68,
    CBA_MENU_ALT_LEFT = 69,
    CBA_MENU_ALT_RIGHT = 70,
    CBA_MENU_START = 71,
    CBA_MENU_QUIT = 72,
    CBA_MENU_SELECT = 73,
    CBA_MENU_ABORT = 74,
    CBA_MENU_PGUP = 75,
    CBA_MENU_PGDN = 76,
    CBA_MENU_X = 77,
    CBA_MENU_Y = 78,
    CBA_MENU_L1 = 79,
    CBA_MENU_L2 = 80,
    CBA_MENU_R1 = 81,
    CBA_MENU_R2 = 82,
    CBA_MINIMAP_ZOOM_IN = 83,
    CBA_MINIMAP_ZOOM_OUT = 84,
    CBA_MENU_BACK = 85,
    CBA_GAMEPLAY_PAUSE = 86,
    CBA_GPS_MARKER = 87,
    CBA_TOOLTIP_RESUME = 88,
    CBA_START_GAME = 89,
    CBA_DELETE_SAVEGAME = 90,
    CBA_FSM_PLACE_BEACON = 91,
    CBA_MP_VIEW_GAMERCARD = 92,
    CBA_CNTRL_CONFIG = 93,
    CBA_CNTRL_CLEAR = 94,
    CBA_CNTRL_INVERT = 95,
    CBA_PUSHTOTALK = 96,
    CBA_SKIP_MODAL_SEQUENCE = 97,
    CBA_RESET_GYRO = 98,
    CBA_CYCLE_WEAPONS_BACK = 99,
    CBA_CYCLE_WEAPONS_FORWARD = 100,
    CBA_SCROLL_BAR_UP = 101,
    CBA_SCROLL_BAR_DOWN = 102,
    CBA_VDC_ACCELERATE_AXIS = 103,
    CBA_VDC_BRAKE_REVERSE_AXIS = 104,
    CBA_OPEN_CHAT = 105,
    CBA_OPEN_TEAM_CHAT = 106,
    CBA_SHOULDER_SWITCH = 107,
    NUM_CONTROL_BUTTON_ACTIONS = 108,
    RESERVED_NUM_CONTROL_BUTTON_ACTIONS = 200,
};

enum control_axis_action
{
    CAA_AXIS_INVALID = -1,
    CAA_CAMERA_ROTATE = 0,
    CAA_CAMERA_ELEVATE = 1,
    CAA_WALK_TURN_LEFT_RIGHT = 2,
    CAA_WALK_FORWARD_BACKWARD = 3,
    CAA_TANK_STEER = 4,
    CAA_TANK_STEER_VERT = 5,
    CAA_TANK_CAMERA_ROTATE = 6,
    CAA_TANK_CAMERA_ELEVATE = 7,
    CAA_DRIVE_STEER = 8,
    CAA_DRIVE_STEER_VERT = 9,
    CAA_DRIVE_CAMERA_ROTATE = 10,
    CAA_DRIVE_CAMERA_ELEVATE = 11,
    CAA_ZOOM_IN_OUT = 12,
    CAA_CYCLE_WEAPONS = 13,
    CAA_STUB_1 = 14,
    CAA_STUB_2 = 15,
    CAA_STUB_3 = 16,
    CAA_STUB_4 = 17,
    CAA_STUB_5 = 18,
    CAA_STUB_6 = 19,
    CAA_STUB_7 = 20,
    CAA_SCROLL_BAR = 21,
    CAA_VDC_ACCELERATE_AXIS = 22,
    CAA_VDC_BRAKE_REVERSE_AXIS = 23,
    CAA_MAP_PAN_CAMERA_LEFT_RIGHT = 24,
    CAA_MAP_PAN_CAMERA_UP_DOWN = 25,
    NUM_CONTROL_AXIS_ACTIONS = 26,
    RESERVED_NUM_CONTROL_AXIS_ACTIONS = 50,
};

enum control_config_setting
{
    CCS_KEYBOARD_MOUSE = 0,
    CCS_XBOX = 1,
    CCS_PS4 = 2,
    CCS_XBONE = 3,
    CCS_GENERIC = 4,
    CCS_STEAM = 5,
    CCS_SWITCH = 6,
    NUM_CONTROL_CONFIG_SETTINGS = 7,
};

enum mask_state
{
    NO_MASKING = 0,
    USE_MASKING = 1,
};

enum bind_result
{
    RESULT_OK = 0,
    RESULT_BAD_ACTION = 1,
    RESULT_LEVEL_AXISDIR = 2,
    RESULT_BAD_STICKID = 3,
    RESULT_BIND_AXIS_AS_BUTTON = 4,
    RESULT_BIND_AXISDIR_AS_AXIS = 5,
    RESULT_INVALID_SCANCODE = 6,
    RESULT_INVALID_AXIS = 7,
    RESULT_BAD_CONTROLLER_INDEX = 8,
};

enum bind_level
{
    LEVEL_PRIMARY = 0,
    LEVEL_ALTERNATE = 1,
    LEVEL_AXISDIR = 2,
};

// size 0xC
struct button_binding_data
{
    unsigned int button[2]; // 0x00
    unsigned int stick_dir; // 0x08
};

// size 0xC
struct axis_binding_data
{
    unsigned int axis[2]; // 0x00
    bool inverted[2]; // 0x08
};

// size 0xBB8
struct control_config
{
    bind_result perform_binding_button(control_button_action action, bind_level level, unsigned int new_value); // 0x006F77B0 (control_map_bind_internal.h:65)
    bind_result perform_binding_axis(control_axis_action action, bind_level level, unsigned int new_value); // 0x008883F0 (control_map_bind_internal.h:78)

    button_binding_data button_bindings[200]; // 0x00
    axis_binding_data axis_bindings[50]; // 0x960
};

// size 0x18
class analog_axis_info
{
public:
    analog_axis_info(); // 0x006FDE40 (:729)
    void clear_masks(); // 0x006F77F0 (:734)
    void clear(); // 0x006F7800 (:739)

    float value; // 0x00
    float last_value; // 0x04
    float raw_value; // 0x08
    int repeat; // 0x0C
    bool mask_axisdir_positive; // 0x10
    bool mask_axisdir_negative; // 0x11
    bool masked; // 0x12
    float square_mapped; // 0x14
};

// size 0x38
struct button_bits
{
    void clear(unsigned int button_id); // 0x006F7860 (:766)
    void clear(); // 0x006F7830 (:757)
    void set_all(); // 0x006F7940 (:794)
    void set(unsigned int button_id); // 0x006F7970 (:803)
    bool is_set(unsigned int button_id); // 0x006F7A40 (:831)

private:
    unsigned int m_keyboard_bits[8]; // 0x00
    uint64_t m_controller_bits_high; // 0x20
    uint64_t m_controller_bits_low; // 0x28
    unsigned char m_mouse_bits; // 0x30
};

// size 0x788
class control_info
{
public:
    control_info(); // 0x006FDE60 (:906)
    void clear(); // 0x006F7AD0 (:912)
    float get_stick_value(unsigned int stick_index, unsigned int axis_index) const; // 0x006F7D30 (:1076)
    float get_mouse_value(int axis_index) const; // 0x006F7D70 (:1082)
    float get_raw_stick_value(unsigned int stick_index, unsigned int axis_index) const; // 0x006F7DB0 (:1088)
    float get_raw_mouse_value(int axis_index) const; // 0x006F7DF0 (:1094)
    float scale_analog(float x) const; // 0x006F7E30 (:1100)
    float get_scaled_stick_value(int stick_index, int axis_index) const; // 0x006F7E40 (:1114)
    float get_scaled_mouse_value(int axis_index) const; // 0x006F7E80 (:1120)
    bool check_stick_as_digital_negative(int stick_index, int axis_index) const; // 0x006F7EC0 (:1127)
    bool check_stick_as_digital_positive(int stick_index, int axis_index) const; // 0x006F7F10 (:1133)
    bool check_mouse_as_digital_negative(int axis_index) const; // 0x006F7F50 (:1139)
    bool check_mouse_as_digital_positive(int axis_index) const; // 0x006F7FA0 (:1145)
    bool check_and_mask_stick_as_digital_negative(int, int);
    bool check_and_mask_stick_as_digital_positive(int, int);
    bool check_and_mask_mouse_as_digital_negative(int);
    bool check_and_mask_mouse_as_digital_positive(int);
    bool check_stick_as_digital_just_pressed_negative(int stick_index, int axis_index) const; // 0x006F7FE0 (:1193)
    bool check_stick_as_digital_just_pressed_positive(int stick_index, int axis_index) const; // 0x006F8040 (:1199)
    bool check_mouse_as_digital_just_pressed_negative(int axis_index) const; // 0x006F80A0 (:1205)
    bool check_mouse_as_digital_just_pressed_positive(int axis_index) const; // 0x006F8100 (:1211)
    bool check_and_mask_stick_as_digital_just_pressed_negative(int stick_index, int axis_index); // 0x006F8150 (:1218)
    bool check_and_mask_stick_as_digital_just_pressed_positive(int stick_index, int axis_index); // 0x006F81B0 (:1228)
    bool check_and_mask_mouse_as_digital_just_pressed_negative(int axis_index); // 0x006F8210 (:1238)
    bool check_and_mask_mouse_as_digital_just_pressed_positive(int axis_index); // 0x006F8270 (:1248)
    bool check_stick_repeat_negative(int stick_index, int axis_index) const; // 0x006F82D0 (:1258)
    bool check_stick_repeat_positive(int stick_index, int axis_index) const; // 0x006F82F0 (:1264)
    bool check_mouse_repeat_negative(int axis_index) const; // 0x006F8310 (:1270)
    bool check_mouse_repeat_positive(int axis_index) const; // 0x006F8330 (:1276)
    bool check_and_mask_stick_repeat_negative(int stick_index, int axis_index); // 0x006F8350 (:1282)
    bool check_and_mask_stick_repeat_positive(int stick_index, int axis_index); // 0x006F8380 (:1292)
    bool check_and_mask_mouse_repeat_negative(int axis_index); // 0x006F83B0 (:1302)
    bool check_and_mask_mouse_repeat_positive(int axis_index); // 0x006F83E0 (:1312)
    bool check_button_down(unsigned int button); // 0x006F8410 (:1322)
    bool check_button_pressed(unsigned int button); // 0x006F8420 (:1327)
    bool check_button_just_pressed(unsigned int button); // 0x006F8460 (:1332)
    void mask_button(unsigned int button); // 0x006F84A0 (:1337)
    void unmask_button(unsigned int button); // 0x006F84D0 (:1343)
    bool button_is_masked(unsigned int button); // 0x006F8500 (:1349)
    void mask_all_buttons(); // 0x006F8520 (:1354)
    void mask_stick_axis(unsigned int stick_id, unsigned int axis_id); // 0x006F85F0 (:1376)
    void mask_mouse_axis(int axis_id); // 0x006F8610 (:1381)
    void mask_all_axes(); // 0x006F8630 (:1386)
    void read_controls(unsigned int controller_id); // 0x006FE150 (:3472)

    // control_map.cpp's free functions read the masks and repeats (friends are not recorded in rfg.pdb).
    friend bool controls_test_button_action(control_button_action action, unsigned int controller_id, bool (control_info::*test_func)(unsigned int), mask_state should_mask);
    friend bool check_axisdir_pressed(unsigned int dir, unsigned int controller_id);

protected:
    analog_axis_info stick[32][2]; // 0x00
    analog_axis_info mouse[3]; // 0x600
    float mouse_spike_values[3]; // 0x648
    button_bits button_down; // 0x658
    button_bits button_pressed; // 0x690
    button_bits button_pressed_mask; // 0x6C8
    button_bits button_just_pressed; // 0x700
    button_bits button_just_pressed_mask; // 0x738
    float sensor[6]; // 0x770
};

// size 0x14
struct control_button_binding_info
{
    control_button_action action; // 0x00
    unsigned int button; // 0x04
    unsigned int alt_button; // 0x08
    unsigned int stick_dir; // 0x0C
    const char* btn_str; // 0x10
};

// size 0x14
struct control_axis_binding_info
{
    control_axis_action action; // 0x00
    unsigned int axis; // 0x04
    bool inverted; // 0x08
    unsigned int alt_axis; // 0x0C
    bool alt_inverted; // 0x10
};

extern unsigned int Player_controller_index; // 0x01E2A9CC (placement inferred)
extern control_config Control_config[1][NUM_CONTROL_CONFIG_SETTINGS]; // 0x01E25778, [controller][control_config_setting]
bool check_axisdir_pressed(unsigned int dir, unsigned int controller_id); // 0x006F8AF0 (:3417), static in rfg.exe

// ---- system and per-frame update
void unused_dcf_mouse_cap(); // 0x006F8950 (:3241)
float ConvertMouseValueToAnalog(int source_value, unsigned int axis_idx, float& uncapped_value); // 0x006FDFA0 (:3252)
void controls_system_init(); // 0x007046A0 (:3951)
void controls_poll(); // 0x00702230 (:3985)
void controls_set_control_config(); // 0x006F9360 (:4578)
void control_update_current_gamepad_config(); // 0x006FAFB0 (:6075)
void controls_verify_control_config(bool warnUser); // 0x00702B30 (:6080)
void controls_update_current_config_state(); // 0x006FB300 (:6226)

// ---- queries used by the simulation
bool controls_test_button_action(control_button_action action, unsigned int controller_id, bool (control_info::*test_func)(unsigned int), mask_state should_mask); // 0x006FE8C0 (:4113)
bool controls_is_button_down(control_button_action action, unsigned int controller_id); // 0x006FE970 (:4142)
bool controls_is_button_pressed(control_button_action action, unsigned int controller_id); // 0x006FE9D0 (:4164)
bool controls_is_button_just_pressed(control_button_action action, unsigned int controller_id); // 0x006FEA30 (:4176)
float controls_get_button_analog(control_button_action action, unsigned int controller_id); // 0x006F8C80 (:4188)
int controls_get_button_for_action(control_button_action action, unsigned int controller_id); // 0x006F8D40 (:4232)
unsigned int controls_get_primary_button_for_action(control_button_action action, unsigned int controller_id); // 0x006F8DB0 (:4244)
unsigned int controls_get_secondary_button_for_action(control_button_action action, unsigned int controller_id); // 0x006F8DF0 (:4250)
bool controls_process_button_pressed(control_button_action action, unsigned int controller_id); // 0x006FEA90 (:4257)
bool controls_process_button_just_pressed(control_button_action action, unsigned int controller_id); // 0x006FEAF0 (:4269)
float controls_get_analog(control_axis_action action, unsigned int controller_id); // 0x006F8E30 (:4282)
float controls_get_raw_analog_uncapped(control_axis_action action, unsigned int controller_id); // 0x006F90B0 (:4375)
float controls_get_raw_analog(control_axis_action action, unsigned int controller_id); // 0x006F92E0 (:4468)
float controls_get_scaled_analog(control_axis_action action, unsigned int controller_id); // 0x006FEB50 (:4475)
float controls_get_square_mapped_analog(control_axis_action action, unsigned int controller_id); // 0x006F9380 (:4589)
unsigned int controls_get_axis_for_action(control_axis_action action, unsigned int controller_id); // 0x006F96F0 (:4853)
void controls_mask_button(control_button_action action, unsigned int controller_id); // 0x006FEEB0 (:4866)
void controls_unmask_button(control_button_action action, unsigned int controller_id); // 0x006FEF00 (:4880)
bool controls_button_is_masked(control_button_action action, unsigned int controller_id); // 0x006FEF50 (:4894)
void controls_mask_axis(control_axis_action action, unsigned int controller_id); // 0x006F9760 (:4920)
void controls_mask_all(unsigned int controller_id); // 0x006F98D0 (:4989)
void controls_map_stick_to_circle(float& h, float& v, float sh, float sv); // 0x006F9930 (:5001)
void controls_map_stick_to_square(float& h, float& v, float sh, float sv); // 0x006F99B0 (:5015)
unsigned int controls_is_any_button_pressed(); // 0x006FEFC0 (:5037)
unsigned int controls_any_key_just_pressed(); // 0x006F9B80 (:5051)
unsigned int controls_any_mouse_button_just_pressed(); // 0x006F9BB0 (:5062)
unsigned int controls_any_gamepad_button_just_pressed(); // 0x006F9BE0 (:5073)
unsigned int controls_is_any_button_down(); // 0x006FF040 (:5085)
unsigned int controls_any_key_down(); // 0x006F9C10 (:5099)
unsigned int controls_any_mouse_button_down(); // 0x006F9C40 (:5110)
unsigned int controls_any_gamepad_button_down(); // 0x006F9C70 (:5121)
unsigned int controls_is_any_axis_pressed(); // 0x006FF0C0 (:5421)
unsigned int controls_any_mouse_axis_changed(); // 0x006FA120 (:5433)
unsigned int controls_any_gamepad_axis_changed(); // 0x006FA1A0 (:5457)
bool controls_is_using_weapon_menu_button(); // 0x006FAFD0 (:6097)
bool controls_is_button_face_button(control_button_action action, unsigned int controller_id); // 0x006FF300 (:6167)
bool control_is_controller_in_use(); // 0x006FB0A0 (:6173)
bool control_mouse_camera_control(control_axis_action h_action, control_axis_action v_action); // 0x006FB0B0 (:6179)
bool control_steam_camera_control(); // 0x006FB1A0 (:6189)

// ---- control encoding (inline in control_map.h)
bool controls_is_control_encoded_for_keyboard(unsigned int value); // 0x006F66B0 (.h:118)
bool controls_is_control_encoded_for_mouse(unsigned int value); // 0x006F66D0 (.h:126)
bool controls_is_control_encoded_for_gamepad(unsigned int value); // 0x006F66F0 (.h:134)
void controls_decode_axisdir(unsigned int axisdir, unsigned int& stick_id, unsigned int& axis_id, bool& is_positive); // 0x006F6700 (.h:142)
bool controls_should_decode_axis_as_two_buttons(unsigned int axis); // 0x006F6740 (.h:151)
void controls_decode_axis_as_axis(unsigned int axis, unsigned int& stick_id, unsigned int& axis_id); // 0x006F6750 (.h:158)
unsigned int controls_decode_button_as_keyboard_key(unsigned int but); // 0x006F6770 (.h:173)
void controls_decode_axis_as_keyboard_keys(unsigned int axis, unsigned int& positive_key, unsigned int& negative_key); // 0x006F6780 (.h:180)
unsigned int controls_decode_button_as_mouse_button(unsigned int but); // 0x006F67A0 (.h:188)
void controls_decode_axis_as_mouse_buttons(unsigned int axis, unsigned int& positive_button, unsigned int& negative_button); // 0x006F67B0 (.h:195)
void controls_decode_axis_as_gamepad_buttons(unsigned int axis, unsigned int& positive_button, unsigned int& negative_button); // 0x006F67D0 (.h:203)
bool controls_axis_encoded_as_buttons_contains(unsigned int axis, unsigned int button); // 0x006F67F0 (.h:218)
bool controls_encoded_for_same_control(unsigned int control, unsigned int other); // 0x006F6820 (.h:226)
// 0x006F6840 (.h:231): an axis bound as two buttons (the positive one in bits 8-15, the negative in 0-7, the type kept)
inline void controls_split_axis_into_two_buttons(unsigned int axis, unsigned int& positive_button, unsigned int& negative_button)
{
    positive_button = ((axis >> 8) & 0xFF) | (axis & 0xF0000000);
    negative_button = axis & 0xF00000FF;
}

// ---- configuration and rebinding (options menus)
unsigned int control_config_get_button_for_action(control_config& config, control_button_action action, int buttonNum); // 0x006F8660 (:2810)
unsigned int control_config_get_axis_for_action(control_config& config, control_axis_action action, int axisNum); // 0x006F8690 (:2817)
void control_config_invert_axis_for_action(control_config& config, control_axis_action action, int axisNum); // 0x006F86C0 (:2824)
bool control_config_get_invert_axis_for_action(control_config& config, control_axis_action action, int axisNum); // 0x006F86F0 (:2839)
void control_config_set_invert_axis_for_action(control_config& config, control_axis_action action, bool inverted, int axisNum); // 0x006F8720 (:2854)
void controls_set_custom_config(const control_config& new_config); // 0x006FE6B0 (:3805)
void controls_set_custom_from_current(); // 0x006F8C00 (:3810)
void controls_set_default_bindings(unsigned int controller_id, control_config_setting config_setting); // 0x007021F0 (:3817)
void control_config_populate_with_defaults(control_config& config); // 0x006FE7B0 (:3870)
void control_config_populate_with_defaults(control_config& config, control_config_setting config_setting); // 0x006FE870 (:3878)
bool control_config_is_equal_to_default(control_config& config); // 0x007045D0 (:3885)
control_config_setting controls_get_current_config_setting(); // 0x006F8C10 (:3917)
void controls_set_button_binding(control_button_action action, control_config_setting control_setting, unsigned int button, unsigned int alt_button, unsigned int stick_dir, unsigned int controller_id); // 0x006F9390 (:4633)
void controls_set_axis_binding(control_axis_action action, control_config_setting control_setting, unsigned int axis, bool inverted, unsigned int alt_axis, bool alt_inverted, unsigned int controller_id); // 0x006F93E0 (:4643)
void controls_set_button_binding(control_config& config, control_button_action action, unsigned int button, unsigned int alt_button, unsigned int stick_dir); // 0x006F9430 (:4668)
void controls_set_axis_binding(control_config& config, control_axis_action action, unsigned int axis, bool inverted, unsigned int alt_axis, bool alt_inverted); // 0x006F9460 (:4677)
void controls_set_button_bindings(control_config& config, const control_button_binding_info* button_bindings, int num_button_bindings); // 0x006F94A0 (:4693)
void controls_set_axis_bindings(control_config& config, const control_axis_binding_info* axis_bindings, int num_axis_bindings); // 0x006F94E0 (:4702)
void controls_set_button_bindings(const control_button_binding_info* button_bindings, int num_button_bindings, control_config_setting control_setting, unsigned int controller_id); // 0x006FEDB0 (:4713)
void controls_set_axis_bindings(const control_axis_binding_info* axis_bindings, int num_axis_bindings, control_config_setting control_setting, unsigned int controller_id); // 0x006F9550 (:4734)
bool controls_get_axis_action_inverted_state(control_axis_action action, const control_config& config, bool primary); // 0x006F95A0 (:4750)
bool controls_get_axis_action_inverted_state(control_axis_action action, unsigned int controller_id, bool primary); // 0x006F95E0 (:4794)
void controls_set_axis_action_inverted_state(control_axis_action action, bool inverted, control_config& config_setting, bool primary); // 0x006F9650 (:4804)
void controls_set_axis_action_inverted_state(control_axis_action action, bool inverted, unsigned int controller_id, bool primary); // 0x006F9690 (:4842)
bool controls_remove_invalid_mappings(control_config& config); // 0x00702930 (:5623)
bool controls_remove_invalid_mappings(); // 0x00702A90 (:5656)
bool controls_check_button_exists(unsigned int button); // 0x006FF0E0 (:5661)
bool controls_check_axis_exists(unsigned int axis); // 0x006FF130 (:5678)
bool controls_check_mouse_button_exists(unsigned int button); // 0x006FA410 (:5696)
bool controls_check_key_exists(unsigned int button); // 0x006FA420 (:5701)
bool controls_check_gamepad_button_exists(unsigned int button); // 0x006FA430 (:5706)
bool controls_check_gamepad_axis_exists(unsigned int axis); // 0x006FA440 (:5711)
bool controls_verify_control_button_is_mapped(const control_config& config, control_button_action action); // 0x006FA450 (:5716)
bool controls_verify_control_axis_is_mapped(const control_config& config, control_axis_action action); // 0x006FA490 (:5729)
bool controls_verify_mandatory_controls(const control_config& config); // 0x006FA4E0 (:5743)
void controls_check_control_button_is_used(unsigned int control, control_button_action* cba_array, int count, bool& result, control_button_action& other_action, unsigned int& level); // 0x006FA6B0 (:5768)
void controls_check_control_axis_is_used(unsigned int control, control_axis_action* caa_array, int count, bool& result, control_axis_action& other_action, unsigned int& level); // 0x006FA7D0 (:5797)
void controls_check_control_button_is_used_in_axis(unsigned int control, control_axis_action* caa_array, int count, bool& result, control_axis_action& other_action, unsigned int& level); // 0x006FF180 (:5823)
bool controls_check_control_contains(unsigned int control, unsigned int other_control); // 0x006FA8F0 (:5854)
void controls_remove_control_button_from_axis_bindings(unsigned int control, control_axis_action* caa_array, int count); // 0x006FA940 (:5877)
void controls_remove_control_button_from_button_bindings(unsigned int control, control_button_action* cba_array, int count); // 0x006FA9D0 (:5895)
void controls_remove_control_axis_from_axis_bindings(unsigned int control, control_axis_action* caa_array, int count); // 0x006FAA40 (:5912)
void controls_remove_control_axis_from_button_bindings(unsigned int control, control_button_action* cba_array, int count); // 0x006FAAB0 (:5929)
void controls_remove_control_button_from_all_axis_bindings(unsigned int control); // 0x006FAB80 (:5967)
void controls_remove_control_button_from_all_button_bindings(unsigned int control); // 0x006FACC0 (:5983)
void controls_remove_axis_from_all_axis_bindings(unsigned int axis); // 0x006FAD80 (:5998)
control_axis_action* controls_get_axis_action_group(control_axis_action action, int& size); // 0x006FAE50 (:6013)
control_button_action* controls_get_button_action_group(control_button_action action, int& size); // 0x006FAEA0 (:6029)
control_axis_action* controls_get_axis_action_group_for_button(control_button_action action, int& size); // 0x006FAEF0 (:6048)
control_button_action* controls_get_button_action_group_for_axis(control_axis_action action, int& size); // 0x006FAF50 (:6061)
bool control_check_custom_uses_gamepad(const control_config& config); // 0x006FB050 (:6139)
bool controls_copy_grouped_controls(control_config& config); // 0x006FB250 (:6196)
void controls_set_control_config_from_other(control_config& dest, const control_config& source); // 0x006FB2E0 (:6220)
unsigned int controls_get_action_set_from_button_action(control_button_action action); // 0x006FB320 (:6775)
unsigned int controls_get_action_set_from_axis_action(control_axis_action action); // 0x006FB350 (:7048)

// ---- names and button images (UI)
bool controls_get_button_text(char* result, int str_len, control_button_action action, unsigned int controller_id); // 0x00704CE0 (:2743)
bool controls_get_button_image_text(char* storage, int storage_len, control_button_action action, unsigned int controller_id); // 0x00705080 (:2750)
bool controls_get_button_image_text_pc(char* storage, int storage_len, unsigned int button, unsigned int action_set); // 0x00704D00 (:2774)
bool controls_get_button_image_name(char* storage, int storage_len, control_button_action action, unsigned int controller_id); // 0x00704DB0 (:2794)
bool controls_get_button_image_name(const control_config& config, char* storage, int storage_len, control_button_action action); // 0x00704E90 (:2870)
bool controls_get_gamepad_stick_image_name(char* storage, int storage_len, unsigned int stick, unsigned int axis, unsigned int action_set); // 0x00702120 (:2883)
const char* controls_get_key_image_name(unsigned int key); // 0x006F8750 (:2897)
bool controls_get_button_image_name_pc(char* storage, int storage_len, unsigned int button, unsigned int action_set); // 0x007043B0 (:2967)
bool controls_get_axis_image_name_pc(char* storage, char* storage2, int storage_len, unsigned int axis, unsigned int action_set); // 0x00704460 (:3010)
bool controls_get_button_image_name(char* storage, int storage_len, unsigned int button, unsigned int action_set); // 0x00702170 (:3087)
bool controls_get_dpad_image_name(char* storage, int storage_len); // 0x006F88D0 (:3121)
bool controls_get_display_info(char* storage, char* storage2, int storage_len, const char* name); // 0x00704F30 (:3160)
void controls_prepend_control_type(char* result, int str_len, unsigned int button, const char* btn_name); // 0x006F9CA0 (:5133)
bool controls_get_button_name(char* btnName, int str_len, unsigned int button, unsigned int action_set); // 0x00704740 (:5147)
bool controls_get_mouse_button_name(char* btnName, int str_len, unsigned int button); // 0x006F9D00 (:5174)
bool controls_get_key_name(char* btnName, int str_len, unsigned int button); // 0x006F9D70 (:5190)
bool controls_get_gamepad_button_name(char* btnName, int str_len, unsigned int button, unsigned int action_set); // 0x007023F0 (:5202)
void controls_prepend_control_type_wide(wchar_t* result, int str_len, unsigned int button, const wchar_t* btn_name); // 0x006F9DF0 (:5218)
bool controls_get_button_name_wide(wchar_t* btnName, int str_len, unsigned int button, unsigned int action_set); // 0x00704860 (:5232)
bool controls_get_mouse_button_name_wide(wchar_t* btnName, int str_len, unsigned int button); // 0x006F9E50 (:5259)
bool controls_get_key_name_wide(wchar_t* btnName, int str_len, unsigned int button); // 0x006F9E90 (:5269)
bool controls_get_gamepad_button_name_wide(wchar_t* btnName, int str_len, unsigned int button, unsigned int action_set); // 0x00702460 (:5281)
bool controls_get_axis_name_wide(wchar_t* btnName, int str_len, unsigned int button, unsigned int action_set, bool inverted); // 0x00704990 (:5294)
bool controls_get_mouse_axis_name_wide(wchar_t* btnName, int str_len, unsigned int button); // 0x006F9F10 (:5317)
bool controls_get_key_button_axis_name_wide(wchar_t* btnName, int str_len, unsigned int button); // 0x006F9F50 (:5329)
bool controls_get_mouse_button_axis_name_wide(wchar_t* btnName, int str_len, unsigned int button); // 0x006FA050 (:5353)
bool controls_get_gamepad_button_axis_name_wide(wchar_t* btnName, int str_len, unsigned int button, unsigned int action_set); // 0x007024D0 (:5370)
bool controls_get_button_axis_name_wide(wchar_t* btnName, int str_len, unsigned int button, unsigned int action_set); // 0x00702600 (:5385)
bool controls_get_gamepad_axis_name_wide(wchar_t* btnName, int str_len, unsigned int button, unsigned int action_set); // 0x00702680 (:5401)
bool controls_get_axis_name(char* btnName, int str_len, unsigned int button, unsigned int action_set, bool inverted); // 0x00704A40 (:5479)
bool controls_get_mouse_axis_name(char* btnName, int str_len, unsigned int button); // 0x006FA240 (:5502)
bool controls_get_key_button_axis_name(char* btnName, int str_len, unsigned int button); // 0x006FA280 (:5514)
bool controls_get_mouse_button_axis_name(char* btnName, int str_len, unsigned int button); // 0x006FA340 (:5531)
bool controls_get_gamepad_button_axis_name(char* btnName, int str_len, unsigned int button, unsigned int action_set); // 0x00702700 (:5548)
bool controls_get_button_axis_name(char* btnName, int str_len, unsigned int button, unsigned int action_set); // 0x00702830 (:5563)
bool controls_get_gamepad_axis_name(char* btnName, int str_len, unsigned int button, unsigned int action_set); // 0x007028B0 (:5579)
bool controls_get_primary_button_name(char* btnName, int str_len, control_button_action action); // 0x00704AE0 (:5597)
bool controls_get_secondary_button_name(char* btnName, int str_len, control_button_action action); // 0x00704B60 (:5603)
bool controls_get_primary_axis_name(char* btnName, int str_len, control_axis_action action); // 0x00704BE0 (:5609)
bool controls_get_secondary_axis_name(char* btnName, int str_len, control_axis_action action); // 0x00704C60 (:5615)
const char* controls_bake_steam_origins_string(const keen::SteamControllerActionOrigin* origins, unsigned int count); // 0x006FF320 (:6237)
const char* controls_get_button_action_image_names_steam(unsigned int button, unsigned int action_set); // 0x006FFB00 (:6788)
const char* controls_get_button_image_name(unsigned int button, unsigned int action_set); // 0x006FFB90 (:6813)
const char* controls_get_axis_action_image_names_steam(unsigned int axis, unsigned int stick, unsigned int action_set); // 0x006FFFA0 (:7061)
const char* controls_get_axis_image_name(unsigned int stick, unsigned int axis, unsigned int action_set); // 0x00700050 (:7093)
const char* controls_get_specialized_button_name(unsigned int button, unsigned int action_set); // 0x007002D0 (:7198)
const char* controls_get_specialized_axis_name(unsigned int stick, unsigned int axis, unsigned int action_set); // 0x00700310 (:7213)
void controls_fill_options_button_hint(char* out_buf, unsigned int buf_capacity, const wchar_t* loca_text); // 0x006FB380 (:7244)
