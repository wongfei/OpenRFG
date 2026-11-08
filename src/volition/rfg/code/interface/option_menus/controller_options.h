// Original: volition/rfg/code/interface/option_menus/controller_options.h
// Implementation: volition/rfg/code/interface/option_menus/controller_options.cpp
//
// The controls options: key and button bindings (binding slots), sensitivity, inversion and the controller pages.
// Placement of the enums inferred.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/vlib/code/input/input.h"

class rl_renderer;

enum control_binding_slot
{
    CBS_INVALID = -1,
    CBS_PRIMARY = 0,
    CBS_SECONDARY = 1,
    NUM_CONTROL_BINDING_SLOTS = 2,
};

enum controller_option_pages
{
    CONTROLLER_OPTIONS_PAGE_INVALID = -1,
    CONTROLLER_OPTIONS_PAGE_GENERAL = 0,
    CONTROLLER_OPTIONS_PAGE_ON_FOOT = 1,
    CONTROLLER_OPTIONS_PAGE_AXIS = 2,
    CONTROLLER_OPTIONS_PAGE_GYRO_CONTROLS = 3,
    NUM_CONTROLLER_OPTIONS_PAGES = 4,
};

// size 0x58
struct controller_options
{
    controller_options() {} // 0x0075F870, does nothing

    int sensitivity; // 0x00
    bool inverted_x; // 0x04
    bool inverted_y; // 0x05
    bool crouch_hold; // 0x06
    bool zoom_hold; // 0x07
    bool vibration_enabled; // 0x08
    int mouse_sensitivity; // 0x0C
    bool gyro_enabled_third_person; // 0x10
    bool gyro_enabled_vehicle; // 0x11
    bool gyro_enabled_turret; // 0x12
    bool gyro_enabled_satellite; // 0x13
    float gyro_sensitivity_third_person_x; // 0x14
    float gyro_sensitivity_third_person_y; // 0x18
    float gyro_sensitivity_vehicle_x; // 0x1C
    float gyro_sensitivity_vehicle_y; // 0x20
    float gyro_sensitivity_turret_x; // 0x24
    float gyro_sensitivity_turret_y; // 0x28
    float gyro_sensitivity_satellite_x; // 0x2C
    float gyro_sensitivity_satellite_y; // 0x30
    gyro_axis_config gyro_orientation_config_x; // 0x34
    gyro_axis_config gyro_orientation_config_y; // 0x3C
    gyro_axis_config gyro_angular_velocity_config_x; // 0x44
    gyro_axis_config gyro_angular_velocity_config_y; // 0x4C
    unsigned char stick_config; // 0x54
    unsigned char tank_stick_config; // 0x55
    unsigned char on_foot_config; // 0x56
    unsigned char vehicle_config; // 0x57
};

bool controller_options_are_changes_different_from_defaults(); // 0x0088F700 (:1098)
void controller_options_set_config_visible(bool visible); // 0x008D6350 (:1235)
void controller_options_set_menu_visible(bool visible); // 0x008D63D0 (:1246)
int controller_options_get_num_configurable_controls(controller_option_pages new_page); // 0x0088FC80 (:1267)
void controller_options_create_row_data(); // 0x008D64A0 (:1287)
void controller_options_create_header_data(); // 0x008D64D0 (:1300)
void Controller_options_process_header_data(); // 0x0088FCA0 (:1321)
void controller_options_get_row_data(controller_option_pages page, int index, const char*& caption, wchar_t* primary, wchar_t* secondary, int str_len); // 0x0088FD60 (:1337)
void control_options_process_scrollbar(); // 0x008D6530 (:1372)
void control_options_config_handle_mouseover(int index, control_binding_slot slot); // 0x008FD9A0 (:1571)
void control_options_config_check_input(); // 0x008FDA90 (:1700)
void control_options_check_config_clicks(controller_option_pages page); // 0x008FDE30 (:1848)
void controller_options_process_rows(); // 0x00890060 (:1942)
void get_button_images(); // 0x00890370 (:2054)
void get_text_for_buttons(); // 0x008E68A0 (:2061)
void controller_option_reset_defaults(); // 0x008C2E90 (:2134)
void set_controller_page(int index); // 0x008FE4C0 (:2979)
void controller_options_create_button_image_data_item(); // 0x008D67A0 (:3273)
void controller_options_create_button_text_data_item(); // 0x008D6860 (:3316)
void controller_options_create_tab_data_item(); // 0x008D6930 (:3386)
void controller_options_process_button_images(); // 0x00890460 (:3413)
void controller_options_process_button_text(); // 0x008E6A20 (:3440)
void controller_options_animate_mouse_buttons(); // 0x008D69C0 (:3755)
void controller_options_process_mouse(); // 0x00900690 (:3774)
void controller_options_process_tab_data(); // 0x008906E0 (:3822)
void controller_options_process_input(); // 0x00900800 (:3872)
void controller_options_enter(game_state prev_state); // 0x0090F2A0 (:4064)
void controller_options_process(); // 0x00915AE0 (:4207)
void controller_options_handshake(rl_renderer*); // 0x00890940 (:4336)
void controller_options_render(rl_renderer*); // 0x00900C70 (:4341)
void controller_options_exit(game_state new_state); // 0x008F3C50 (:4350)
int controller_options_get_control_id(); // 0x00890950 (:4422)
int controller_options_get_doc_handle(); // 0x00890960 (:4428)
void controller_options_get_control_config(unsigned char& on_foot_buttons, unsigned char& vehicle_buttons, unsigned char& stick_config, unsigned char& tank_stick_config); // 0x00890970 (:4435)
void controller_options_reset_to_default(controller_options& options); // 0x008909B0 (:4445)
void controller_options_get_control_config(controller_options& options); // 0x00890AA0 (:4514)
void controller_options_set_control_config(const controller_options& options); // 0x00890C80 (:4558)
