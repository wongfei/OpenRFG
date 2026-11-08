// Original: volition/rfg/code/interface/option_menus/controller_options.cpp
//
// The control options (GS_CONTROLLER_OPTIONS, UI.md UI6) on rfg_ui_controls_pc, up to four pages: general (inversion,
// sensitivities, crouch and zoom hold, vibration, the controller APIs to hide, the vehicle mouse camera), buttons and
// axes (a spreadsheet of primary/secondary bindings: click (or the config button) to bind, right click to erase, middle
// click to invert an axis), and gyro (Steam controllers only). Accept checks the mandatory controls and saves, back asks
// before dropping changes, the third hint resets the defaults. Wrecking crew keeps per-player controls (wrecking.md WC2).

#include "volition/rfg/code/interface/option_menus/controller_options.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "keen/input/inputsystem.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_string_processing.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/camera/camera_first_person.h"
#include "volition/rfg/code/camera/camera_free.h"
#include "volition/rfg/code/control/control_gameplay.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_options.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/option_menus/ingame_options.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_document_menu_wrecking_crew_character_select.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/input/input_internal.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/unicode_tools.h"

#include <math.h>

// ---- file-private types (rfg.pdb)
enum control_action_type
{
    CAT_INVALID = -1,
    CAT_BUTTON = 0,
    CAT_AXIS = 1,
    NUM_CONTROL_ACTION_TYPES = 2,
};

// The sensitivity rows map 0..9 onto 0.3..1.0 (look, mouse) or 0.3..1.25 (gyro); the default row is 4.
static const float kSensitivityMin = 0.3f;
static const float kSensitivityRange = 0.7f;
static const float kGyroSensitivityRange = 0.95f;
static const int kDefaultSensitivity = 4;

// ---- file-static globals
static int Conoptions_current_num_pages = 4; // 0x01663D94
// The buttons page's actions (control_button_action values).
static control_button_action On_foot_buttons[36] = {
    (control_button_action)0x1C, (control_button_action)0x1D, (control_button_action)0x00, (control_button_action)0x34,
    (control_button_action)0x36, (control_button_action)0x32, (control_button_action)0x38, (control_button_action)0x14,
    (control_button_action)0x15, (control_button_action)0x16, (control_button_action)0x18, (control_button_action)0x19,
    (control_button_action)0x1A, (control_button_action)0x05, (control_button_action)0x56, (control_button_action)0x3A,
    (control_button_action)0x17, (control_button_action)0x53, (control_button_action)0x54, (control_button_action)0x62,
    (control_button_action)0x6B, (control_button_action)0x1E, (control_button_action)0x1F, (control_button_action)0x20,
    (control_button_action)0x23, (control_button_action)0x21, (control_button_action)0x2A, (control_button_action)0x2D,
    (control_button_action)0x24, (control_button_action)0x25, (control_button_action)0x26, (control_button_action)0x27,
    (control_button_action)0x28, (control_button_action)0x60, (control_button_action)0x69, (control_button_action)0x6A,
}; // 0x01663D98
// The axes page's actions (control_axis_action values).
static control_axis_action Stick_controls[12] = {
    (control_axis_action)0, (control_axis_action)1, (control_axis_action)3, (control_axis_action)2,
    (control_axis_action)10, (control_axis_action)11, (control_axis_action)8, (control_axis_action)6,
    (control_axis_action)7, (control_axis_action)4, (control_axis_action)5, (control_axis_action)13,
}; // 0x01663E28
static const char* On_foot_descriptions[36] = {"MENU_USE", "MENU_DETONATE_CHARGES", "MENU_WEAPON_MENU", "MENU_PC_WEAPON1", "MENU_PC_WEAPON2", "MENU_PC_WEAPON3", "MENU_PC_WEAPON4", "MENU_PRIMARY_ATTACK", "MENU_MELEE_ATTACK", "MENU_JUMP", "MENU_CROUCH", "MENU_RELOAD", "MENU_ZOOM", "MENU_ACTIVATE_BACKPACK", "MENU_PAUSE_MENU", "MENU_BRING_UP_MAP", "MENU_SPRINT", "MINIMAP_ZOOM_IN", "MINIMAP_ZOOM_OUT", "MENU_RESET_GYRO", "MENU_SHOULDER_SWITCH", "MENU_ACCELERATE", "MENU_BRAKE_REVERSE", "MENU_HANDBRAKE", "MENU_REAR_VIEW", "MENU_FIRE_ATTACK", "MENU_SECONDARY_FIRE", "MENU_CHANGE_SEAT", "MENU_MECH_BASH_DOWN", "MENU_MECH_BASH_UP", "MENU_MECH_SWING_LEFT", "MENU_MECH_SWING_RIGHT", "MENU_MECH_CHARGE", "MENU_PUSHTOTALK", "MENU_CBA_OPEN_CHAT", "MENU_CBA_OPEN_TEAM_CHAT"}; // 0x01663E68
static const char* Stick_descriptions[12] = {"MENU_ROTATE_CAMERA", "MENU_ELEVATE_CAMERA", "MENU_MOVE_FORWARD_BACK", "MENU_PC_MOVE_RIGHT_LEFT", "MENU_VEHICLE_ROTATE_CAMERA", "MENU_VEHICLE_ELEVATE_CAMERA", "MENU_PC_STEER_RIGHT_LEFT", "MENU_TANK_ROTATE_CAMERA", "MENU_TANK_ELEVATE_CAMERA", "MENU_PC_TANK_STEER", "MENU_PC_TANK_STEER_VERT", "MENU_CYCLE_WEAPONS"}; // 0x01663EF8
static const char* Stick_tank_steer_descriptions[4] = {"MENU_TANK_STEER_ABSOLUTE", "MENU_TANK_STEER_VERT_ABSOLUTE", "MENU_ROTATE_CAMERA", "MENU_ELEVATE_CAMERA"}; // 0x01663F38
static const char* cPage_names[4] = {"MENU_GENERAL_CONTROLS", "MENU_BUTTON_CONTROLS", "MENU_AXIS_CONTROLS", "MENU_GYRO_CONTROLS"}; // 0x01663F48
static bool cPage_enabled[4] = {true, false, false, false}; // 0x01663F58
static const char* cX_axis_list[2] = {"MENU_DEFAULT", "MENU_INVERTED"}; // 0x01663F5C
static const char* cY_axis_list[2] = {"MENU_DEFAULT", "MENU_INVERTED"}; // 0x01663F64
static const char* cSensitivity_list[10] = {"MENU_SENSITIVITY_1", "MENU_SENSITIVITY_2", "MENU_SENSITIVITY_3", "MENU_SENSITIVITY_4", "MENU_SENSITIVITY_5", "MENU_SENSITIVITY_6", "MENU_SENSITIVITY_7", "MENU_SENSITIVITY_8", "MENU_SENSITIVITY_9", "MENU_SENSITIVITY_10"}; // 0x01663F6C
static int Num_sensitivities = 10; // 0x01663F94
static const char* cCrouch_list[2] = {"MENU_HOLD", "MENU_TOGGLE"}; // 0x01663F98
static const char* cVibration_list[2] = {"MENU_ON", "MENU_OFF"}; // 0x01663FA0
static const char* cGyro_axes_list[2] = {"MENU_GYRO_AXIS_Y_MINUS", "MENU_GYRO_AXIS_Z_PLUS"}; // 0x01663FA8
static const char* cVehicle_mouse_cam_config[2] = {"MENU_DEFAULT", "MENU_ALTERNATE_1"}; // 0x01663FB0
static const char* cGyro_enabled_list[2] = {"MENU_ON", "MENU_OFF"}; // 0x01663FB8
static int Doc_handle_coptions = -1; // 0x01663FC0
static int Menu_handle_coptions = -1; // 0x01663FC4
static int Control_id_coptions = -1; // 0x01663FCC
static int Input_id_coptions = -1; // 0x01663FD0
static controller_option_pages Cur_page = CONTROLLER_OPTIONS_PAGE_INVALID; // 0x01663FD4
// Initial 16 in the binary (set when the items are made).
static unsigned int Page_tab_handle = 16; // 0x01663FD8
static const char* cDocname_pc = {"rfg_ui_controls_pc"}; // 0x01663FE8
static const char* cImagename_xbox = {"ui_fend_controller_config_xbox"}; // 0x01663FF0
static const char* cImagename_xbone = {"ui_fend_controller_config_xbone"}; // 0x01663FF4
static const char* cImagename_ps4 = {"ui_fend_controller_config_ps4"}; // 0x01663FFC
static const char* cUnused_string_tag = {"MENU_UNUSED"}; // 0x01664004
static const char* kHint1 = {"hints"}; // 0x01664008
static const char* kHint2 = {"hints2"}; // 0x0166400C
static const char* kHint3 = {"hints3"}; // 0x01664010
static const char* kHint4 = {"hints4"}; // 0x01664014
static const char* kControlsRowMain = {"controls_row_main"}; // 0x01664018
static const char* kControlArrows = {"row_highlight_arrows"}; // 0x01664020
static const char* kControlArrowLeft = {"row_highlight_left"}; // 0x01664024
static const char* kControlRowHighlightBG = {"row_highlight_bg"}; // 0x0166402C
static const char* kControlRowHighlightMain = {"highlight_main"}; // 0x01664030
static unsigned int Data_row_group_handle_coptions = 16; // 0x01664038
static const char* cConfig_group_name = {"spreadsheet"}; // 0x01664040
static const char* cConfig_row_group_name = {"row_group"}; // 0x01664044
static const char* cConfig_row_primary_name = {"primary_bg"}; // 0x01664048
static const char* cConfig_row_secondary_name = {"secondary_bg"}; // 0x0166404C
static control_button_action Control_button_to_config = (control_button_action)-1; // 0x01664050
static control_axis_action Control_axis_to_config = (control_axis_action)-1; // 0x01664054
static control_action_type Config_action_type = CAT_INVALID; // 0x01664058
static control_binding_slot Config_binding_slot = CBS_INVALID; // 0x0166405C
static control_binding_slot Config_mouseover_slot = CBS_INVALID; // 0x01664060
static unsigned int Config_current_button = 0xFFFFFFFF; // 0x01664064
static unsigned int Config_current_axis = 0xFFFFFFFF; // 0x0166406C
static const char* cConfig_press_control_messages[2] = {"MENU_PC_CONFIG_PRESS_BUTTON", "MENU_PC_CONFIG_PRESS_AXIS"}; // 0x01664070
static const char* cConfig_press_control_titles[2] = {"MENU_PC_CONFIG_BUTTON", "MENU_PC_CONFIG_AXIS"}; // 0x01664078
static const char* cAxis_config_error_title = {"MENU_PC_CONFIG_ERROR"}; // 0x01664080
static const char* cMismatched_axis_combo_desc = {"MENU_PC_CONFIG_AXIS_MISMATCH"}; // 0x01664084
static const char* cRepeated_axis_combo_desc = {"MENU_PC_CONFIG_AXIS_REPEAT_ERROR"}; // 0x01664088
// This file's copies of the header's object names (UI0 missed these statics; addresses from the asm).
static const char* cConfig_menu_highlight_row_name = {"highlight_main"}; // 0x01668690
static const char* cConfig_menu_row_name = {"controls_row_main"}; // 0x01668694
static const char* kScrollBarOutline = {"black_scrollfill"}; // 0x01668698
static const char* kPageArrowRight = {"page_right_arrow_main"}; // 0x016686B8
static const char* kPageArrowLeft = {"page_left_arrow_main"}; // 0x016686BC
static const char* kPageHeaders[3] = {"page1", "page2", "page3"}; // 0x016686C0
// int Player_character_select_selection (0x01664BFC) is used here too; placed in
// ui_document_menu_wrecking_crew_character_select.cpp.
static const int kNumVisibleTabs = 3; // 0x01666CF4
// The gyro's horizontal movement choices (orientation and angular velocity sensors).
static const gyro_axis_config cGyro_orientation_axes[2] = {gyro_axis_config((gyro_sensor)4, -1.0f), gyro_axis_config((gyro_sensor)5, 1.0f)}; // 0x01666CF8
static const gyro_axis_config cGyro_angular_velocity_axes[2] = {gyro_axis_config((gyro_sensor)1, -1.0f), gyro_axis_config((gyro_sensor)2, 1.0f)}; // 0x01666D08
// vector Obj_pos (0x01666F9C), Off_screen_pos (0x01666FA8), On_screen_pos (0x01666FB4) are used here too; placed in
// ui_character.cpp.
extern vector Obj_pos; // 0x01666F9C
extern vector Off_screen_pos; // 0x01666FA8
extern vector On_screen_pos; // 0x01666FB4
// The page's button texts, by button (UI0 missed this static; address from the asm).
static char cButton_text[128][256]; // 0x02BE9E30
static unsigned int Data_row_handles_coptions[9]; // 0x02BF43A4
static control_config Temporary_current_config; // 0x02BF47B0
static char cButton_image[128][256]; // 0x02BF5420
static control_config Control_options_main_player_ctrl_config_backup; // 0x02BFD478
static bool Hide_xinput; // 0x02C036EB
static bool Hide_dinput; // 0x02C036EC
static bool Hide_steam_controller; // 0x02C036ED
static bool Assets_loaded_coptions; // 0x02C036EE
static bool Waiting_on_save_completion; // 0x02C036EF
static vehicle_camera_control_mode Vehicle_mouse_cam_mode; // 0x02C036F0
static unsigned int Data_text_handle; // 0x02C036F4
static unsigned int Data_images_handle; // 0x02C036F8
static bool Change_made_coptions; // 0x02C03700
static bool Use_multiplayer_controls; // 0x02C03701
static bool Vibration_config_allowed; // 0x02C03702
static bool Custom_config_changes_made; // 0x02C03703
static int Message_box_handle_coptions; // 0x02C03704
static unsigned int On_foot_config; // 0x02C03708
static unsigned int Vehicle_config; // 0x02C0370C
static unsigned int Stick_config; // 0x02C03710
static unsigned int Tank_stick_config; // 0x02C03714
static int Display_page_start_index; // 0x02C03718
static int Selected_page_coptions; // 0x02C0371C
static bool Configuration_changed; // 0x02C03720
static bool Gyro_config_allowed; // 0x02C03721
static bool Controller_config_pressed; // 0x02C03722
static bool Controller_erase_pressed; // 0x02C03723
static int Display_start_index_coptions; // 0x02C03724
static bool Controller_invert_pressed; // 0x02C03728
static bool Wait_for_button_up; // 0x02C03729
static bool Update_list; // 0x02C0372A
static bool Awaiting_config_input; // 0x02C0372B
static int Highlighted_row; // 0x02C0372C
static int Highlighted_col; // 0x02C03730
static int Selected_row_coptions; // 0x02C03734
static unsigned int Data_header_handle_coptions; // 0x02C03738
static int Config_msgbox_handle; // 0x02C0373C
static int Config_confirm_msgbox_handle; // 0x02C03740
static int Config_mouseover_index; // 0x02C03748
static bool Config_right_click; // 0x02C0374C
static bool Config_invert_axis_clicked; // 0x02C0374D
static bool Awaiting_second_part_of_axis; // 0x02C0374F
// The spreadsheet's row tints, set each frame (UI0 missed these statics; addresses from the asm).
static vint_variant_color cColorWhite; // 0x02CA1648
static vint_variant_color cColorDark; // 0x02CA1654
static int Conoptions_current_num_visible_tabs; // 0x02C3CAF4
static controller_options Control_options_main_player_control_options_backup; // 0x02C7E218
// bool Active (0x02C03754) is used here too; placed in ui_document_hud_tool_tips.cpp.
extern bool Active; // 0x02C03754, ui_document_hud_tool_tips.cpp (the in-game options are up)

// Magic numbers: the spreadsheet shows 9 rows.
static const int kVisibleRows = 9;

// ---- internal functions

// A sensitivity row from a value (0.3 + row * step).
static int controller_options_sensitivity_row(float value, float range)
{
    return fl_round((value - kSensitivityMin) / (range / (float)(Num_sensitivities - 1)));
}

// A sensitivity value from a row.
static float controller_options_sensitivity_value(int row, float range)
{
    return (range / (float)(Num_sensitivities - 1)) * (float)row + kSensitivityMin;
}

// The controller config the options edit (the current setting's).
static control_config& controller_options_current_config()
{
    return Control_config[Player_controller_index][controls_get_current_config_setting()];
}

// Inlined: the wrecking crew player the character select has selected (four rows per player). Out of range: player 0
// (ui_document_menu_wrecking_crew_character_get_player_index answers -1 there instead).
static int controller_options_wc_selected_player()
{
    const unsigned int player = (unsigned int)(Player_character_select_selection / 4);
    return player > 3 ? 0 : (int)player;
}

// The input id that may read the buttons (ui_input_id_has_focus, inlined): -1 while nothing is locked, else the lock
// or the top request.
static bool controller_input_has_focus(int input_id)
{
    const int lock = Current_input_lock_id != -1 ? Current_input_lock_id : 0;
    if (input_id == -1 && lock == 0)
    {
        return true;
    }
    int top = Current_input_lock_id;
    if (Current_input_lock_id == -1)
    {
        top = Input_types.size() - 1 < 0 ? 1 : Input_types[Input_types.size() - 1].id;
    }
    return input_id == top;
}

// A button of Button_info pressed (and not waiting for its release).
static bool controller_button_pressed(int button)
{
    return !Button_info.binfo[button].m_button_wait_for_up && Button_info.binfo[button].m_button_input > 0.0f;
}

// 0x008FD490 (:1444)
// Right click: the slot's binding erased; middle click: the axis inverted (axes page); else the "press a control" box
// for the slot (a button or an axis).
static void control_options_start_config_edit()
{
    Wait_for_button_up = true;
    if (Config_right_click)
    {
        if (Cur_page == CONTROLLER_OPTIONS_PAGE_ON_FOOT)
        {
            Control_button_to_config = On_foot_buttons[Config_mouseover_index];
            Config_action_type = CAT_BUTTON;
            if (Config_mouseover_slot != CBS_PRIMARY)
            {
                // Original: the secondary slot erases the stick direction, not the alternate button.
                Temporary_current_config.button_bindings[Control_button_to_config].stick_dir = 0xFFFFFFFF;
            }
            else
            {
                Temporary_current_config.button_bindings[Control_button_to_config].button[0] = 0xFFFFFFFF;
            }
        }
        else if (Cur_page == CONTROLLER_OPTIONS_PAGE_AXIS)
        {
            Control_axis_to_config = Stick_controls[Config_mouseover_index];
            Config_action_type = CAT_AXIS;
            Temporary_current_config.axis_bindings[Control_axis_to_config].axis[Config_mouseover_slot != CBS_PRIMARY] = 0xFFFFFFFF;
        }
        Config_current_button = 0xFFFFFFFF;
        Config_current_axis = 0xFFFFFFFF;
        Custom_config_changes_made = true;
        Change_made_coptions = true;
        return;
    }
    if (Config_invert_axis_clicked)
    {
        if (Cur_page == CONTROLLER_OPTIONS_PAGE_AXIS)
        {
            Control_axis_to_config = Stick_controls[Config_mouseover_index];
            control_config_invert_axis_for_action(Temporary_current_config, Control_axis_to_config, Config_mouseover_slot != CBS_PRIMARY ? 1 : 0);
            Custom_config_changes_made = true;
            Change_made_coptions = true;
        }
        return;
    }
    if (Cur_page == CONTROLLER_OPTIONS_PAGE_ON_FOOT)
    {
        Control_button_to_config = On_foot_buttons[Config_mouseover_index];
        Config_action_type = CAT_BUTTON;
    }
    else if (Cur_page == CONTROLLER_OPTIONS_PAGE_AXIS)
    {
        Control_axis_to_config = Stick_controls[Config_mouseover_index];
        Config_action_type = CAT_AXIS;
    }
    const char* title = cConfig_press_control_titles[Config_action_type];
    const char* message = cConfig_press_control_messages[Config_action_type];
    Config_binding_slot = Config_mouseover_slot;
    Awaiting_second_part_of_axis = false;
    Config_current_button = 0xFFFFFFFF;
    Config_current_axis = 0xFFFFFFFF;
    Config_msgbox_handle = ui_add_msgbox(MBT_CONFIG_CONTROL, title, message, nullptr, false, false, nullptr, nullptr, false);
    Awaiting_config_input = true;
}

// 0x0088FE80 (:1545)
// Pause cannot be rebound in the primary slot.
static bool control_options_can_configure_control(int index, control_binding_slot slot)
{
    if (slot != CBS_SECONDARY)
    {
        if (Cur_page == CONTROLLER_OPTIONS_PAGE_ON_FOOT)
        {
            if (On_foot_buttons[index] == CBA_GAMEPLAY_PAUSE)
            {
                return false;
            }
        }
        else if (Cur_page != CONTROLLER_OPTIONS_PAGE_AXIS)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
    }
    return true;
}

// 0x0088FEC0 (:1616)
// Accepted: the chosen control leaves the actions grouped with this one, then binds to the slot.
static bool control_config_select_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (choice != MBC_OK)
    {
        return true;
    }
    if (Config_action_type == CAT_BUTTON)
    {
        int button_count = Config_action_type;
        int axis_count = Config_action_type;
        control_axis_action* axis_group = controls_get_axis_action_group_for_button(Control_button_to_config, axis_count);
        control_button_action* button_group = controls_get_button_action_group(Control_button_to_config, button_count);
        controls_remove_control_button_from_button_bindings(Config_current_button, button_group, button_count);
        controls_remove_control_button_from_axis_bindings(Config_current_button, axis_group, axis_count);
        if (Config_binding_slot != CBS_PRIMARY)
        {
            // Original: the secondary slot binds the stick direction.
            Temporary_current_config.button_bindings[Control_button_to_config].stick_dir = Config_current_button;
        }
        else
        {
            Temporary_current_config.button_bindings[Control_button_to_config].button[0] = Config_current_button;
        }
        Change_made_coptions = true;
        Custom_config_changes_made = true;
        return true;
    }
    if (Config_action_type == CAT_AXIS)
    {
        int count = 0;
        // Bit 27: an axis made of two buttons.
        if ((Config_current_axis >> 27) & 1)
        {
            control_button_action* button_group = controls_get_button_action_group_for_axis(Control_axis_to_config, count);
            controls_remove_control_axis_from_button_bindings(Config_current_axis, button_group, count);
        }
        control_axis_action* axis_group = controls_get_axis_action_group(Control_axis_to_config, count);
        controls_remove_control_axis_from_axis_bindings(Config_current_axis, axis_group, count);
        Temporary_current_config.axis_bindings[Control_axis_to_config].axis[Config_binding_slot != CBS_PRIMARY] = Config_current_axis;
    }
    Change_made_coptions = true;
    Custom_config_changes_made = true;
    return true;
}

// 0x00890030 (:1679)
// LTCG: button in EAX. Magic numbers: four keys that cannot be bound (keyboard 0x12, 0x17, 0x08, 0x0C).
static bool control_options_is_button_choice_valid(unsigned int button)
{
    return button != 0x40000012 && button != 0x40000017 && button != 0x40000008 && button != 0x4000000C;
}

// 0x008903B0 (:2256)
static void check_stick_config_allowed()
{
    Vibration_config_allowed = input_has_vibration() && input_get_playing_with_controller();
    Gyro_config_allowed = controls_get_current_config_setting() == CCS_STEAM;
}

// The gyro modes' sensitivities, x and y each.
static const gyro_mode kGyroModes[4] = {GYRO_MODE_THIRD_PERSON, GYRO_MODE_VEHICLE, GYRO_MODE_TURRET, GYRO_MODE_SATELLITE};

// 0x008C3180 (:2280)
static void controller_option_change_cb(int selected_item, void* new_value)
{
    const int value = (int)(intptr_t)new_value;
    switch (Cur_page)
    {
    case CONTROLLER_OPTIONS_PAGE_GENERAL:
        switch (selected_item)
        {
        case 0:
        case 1:
        {
            // X (rotate) or Y (elevate) for the on foot, driving and tank cameras.
            if (value != 0 && value != 1)
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                break;
            }
            const bool inverted = value == 1;
            if (selected_item == 0)
            {
                controls_set_axis_action_inverted_state(CAA_CAMERA_ROTATE, inverted, Temporary_current_config, true);
                controls_set_axis_action_inverted_state(CAA_DRIVE_CAMERA_ROTATE, inverted, Temporary_current_config, true);
                controls_set_axis_action_inverted_state(CAA_TANK_CAMERA_ROTATE, inverted, Temporary_current_config, true);
            }
            else
            {
                controls_set_axis_action_inverted_state(CAA_CAMERA_ELEVATE, inverted, Temporary_current_config, true);
                controls_set_axis_action_inverted_state(CAA_DRIVE_CAMERA_ELEVATE, inverted, Temporary_current_config, true);
                controls_set_axis_action_inverted_state(CAA_TANK_CAMERA_ELEVATE, inverted, Temporary_current_config, true);
            }
            break;
        }
        case 2:
            Camera_first_person_look_sensitivity_pitch = controller_options_sensitivity_value(value, kSensitivityRange);
            Camera_first_person_look_sensitivity_heading = Camera_first_person_look_sensitivity_pitch;
            break;
        case 3:
            if (value == 0 || value == 1)
            {
                Control_crouch_hold = value == 0;
            }
            else
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            }
            break;
        case 4:
            if (value == 0 || value == 1)
            {
                Control_zoom_hold = value == 0;
            }
            else
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            }
            break;
        case 5:
            if (value == 0 || value == 1)
            {
                input_set_vibration(value == 0);
            }
            else
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            }
            break;
        case 6:
            Mouse_sensitivity = controller_options_sensitivity_value(value, kSensitivityRange);
            break;
        case 7:
            // The row lists "on" first: on means shown.
            Hide_xinput = value == 0;
            break;
        case 8:
            Hide_dinput = value == 0;
            break;
        case 9:
            Hide_steam_controller = value == 0;
            break;
        case 10:
            Vehicle_mouse_cam_mode = (vehicle_camera_control_mode)value;
            break;
        default:
            return;
        }
        Change_made_coptions = true;
        return;
    case CONTROLLER_OPTIONS_PAGE_ON_FOOT:
    case CONTROLLER_OPTIONS_PAGE_AXIS:
        return;
    case CONTROLLER_OPTIONS_PAGE_GYRO_CONTROLS:
        if (selected_item >= 0 && selected_item < 12)
        {
            // Rows of three per mode: on/off, x sensitivity, y sensitivity.
            const gyro_mode mode = kGyroModes[selected_item / 3];
            const int row = selected_item % 3;
            if (row == 0)
            {
                const bool enabled = value == 0;
                input_set_gyro_enabled(mode, enabled);
                ui_menu_enable_menu_item(Menu_handle_coptions, selected_item + 1, enabled);
                ui_menu_enable_menu_item(Menu_handle_coptions, selected_item + 2, enabled);
            }
            else
            {
                input_set_gyro_sensitivity(mode, row == 1 ? GYRO_CAMERA_AXIS_X : GYRO_CAMERA_AXIS_Y, controller_options_sensitivity_value(value, kGyroSensitivityRange));
            }
            Change_made_coptions = true;
            return;
        }
        if (selected_item == 12)
        {
            input_gyro_set_orientation_config(GYRO_CAMERA_AXIS_X, cGyro_orientation_axes[value]);
            input_gyro_set_angular_velocity_config(GYRO_CAMERA_AXIS_X, cGyro_angular_velocity_axes[value]);
            Change_made_coptions = true;
            return;
        }
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
}

// 0x008C3960 (:2764)
static bool control_menu_default_controls_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Message_box_handle_coptions != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        controller_option_reset_defaults();
        Change_made_coptions = true;
    }
    else if ((int)choice < 1 || (int)choice > 2)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    Message_box_handle_coptions = -1;
    return true;
}

// 0x008903F0 (:2791)
static bool controls_menu_cancel_message_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Message_box_handle_coptions != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        game_options_load_from_cache();
        if (gameseq_get_state() == GS_CONTROLLER_OPTIONS)
        {
            gameseq_pop_state();
        }
    }
    else if ((int)choice < 1 || (int)choice > 2)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    Message_box_handle_coptions = -1;
    return true;
}

// 0x008FE280 (:2821)
// Accept: with changes (or a failed last save) the mandatory controls are checked, the custom config grouped and set,
// invalid mappings removed, the config and the hidden controller APIs applied, saved (the menu leaves once done); back:
// a box if changed; misc: the defaults box; misc 2: Steam's binding panel.
static void controller_options_hint_callback(hint_type hint)
{
    switch (hint)
    {
    case HT_SELECTION:
        if (Change_made_coptions || !player_profile_last_save_successful(Player_controller_index != 0xFFFFFFFFu ? (int)Player_controller_index : 0))
        {
            if (!controls_verify_mandatory_controls(Temporary_current_config))
            {
                Message_box_handle_coptions = ui_add_msgbox(MBT_OK, "MENU_WARNING", "MENU_PC_MISSING_CONTROLS", nullptr, false, false, nullptr, nullptr, false);
                return;
            }
            if (Custom_config_changes_made)
            {
                controls_copy_grouped_controls(Temporary_current_config);
                controls_set_custom_config(Temporary_current_config);
            }
            if (Configuration_changed || Custom_config_changes_made)
            {
                controls_remove_invalid_mappings();
            }
            controller_options_current_config() = Temporary_current_config;
            if (!gameseq_state_is_active(GS_WRECKING_CREW_CHARACTER_SELECT) && !wrecking_crew_is_active())
            {
                keen::input::setControllerApiHidden(Keen_input_system, keen::WindowsControllerApi_XInput, Hide_xinput);
                keen::input::setControllerApiHidden(Keen_input_system, keen::WindowsControllerApi_DInput, Hide_dinput);
                keen::input::setControllerApiHidden(Keen_input_system, keen::WindowsControllerApi_SteamController, Hide_steam_controller);
            }
            Camera_free_mouse_control_mode = Vehicle_mouse_cam_mode;
            // In wrecking crew the controls belong to the current player (written back on exit): saved only with none.
            if (!gameseq_state_is_active(GS_WRECKING_CREW_CHARACTER_SELECT) && (!wrecking_crew_is_active() || Wrecking_crew_current_player == -1))
            {
                game_options_save();
                Waiting_on_save_completion = true;
                return;
            }
        }
        break;
    case HT_ABORT:
        if (Change_made_coptions)
        {
            Message_box_handle_coptions = ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_WARNING", "MENU_CHANGES_LOST", controls_menu_cancel_message_cb, false, false, nullptr, nullptr, false);
            return;
        }
        break;
    case HT_MISC:
        if (controller_options_are_changes_different_from_defaults())
        {
            Message_box_handle_coptions = ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_WARNING", "MENU_RESET_DEFAULT_CONFIRM", control_menu_default_controls_cb, false, false, nullptr, nullptr, false);
            return;
        }
        ui_add_msgbox(MBT_OK, "MENU_WARNING", "MENU_DEFAULTS_NOT_CHANGED", nullptr, false, false, nullptr, nullptr, false);
        return;
    case HT_MISC_2:
        if (controls_get_current_config_setting() == CCS_STEAM)
        {
            keen::input::showSteamControllerBindingPanel(Keen_input_system);
        }
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
    if (gameseq_get_state() == GS_CONTROLLER_OPTIONS)
    {
        gameseq_pop_state();
    }
}

// The page of an absolute index (wrapping) with the tab window around it: the first page shows from the start, the last
// at the end, the others in the middle.
static void controller_options_go_to_page(int page)
{
    if (page <= 0 || page >= Conoptions_current_num_pages)
    {
        Display_page_start_index = 0;
        Selected_page_coptions = 0;
        set_controller_page(0);
        return;
    }
    int start;
    if (page == Conoptions_current_num_pages - 1)
    {
        Selected_page_coptions = Conoptions_current_num_visible_tabs - 1 > 0 ? Conoptions_current_num_visible_tabs - 1 : 0;
        start = page - Conoptions_current_num_visible_tabs + 1;
    }
    else
    {
        Selected_page_coptions = Conoptions_current_num_pages - 1 < 2 ? Conoptions_current_num_pages - 1 : 1;
        start = page - Conoptions_current_num_visible_tabs / 2;
    }
    Display_page_start_index = start > 0 ? start : 0;
    set_controller_page(page);
}

// The next/previous page (wrapping).
static void controller_options_step_page(int direction)
{
    int page = Selected_page_coptions + Display_page_start_index + direction;
    if (page < 0)
    {
        page = Conoptions_current_num_pages - 1;
    }
    controller_options_go_to_page(page);
}

// 0x009003E0 (:3643)
// The page tabs (under the mouse full, the others half transparent; a click goes there) and the page arrows.
static bool controller_options_process_mouse_tab_input()
{
    vint_doc_info* info = ui_get_doc_info(Doc_handle_coptions);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return false;
    }
    for (int tab = 0; tab < 3; tab++)
    {
        vint_object_base* header = vint_object_base::find_object(kPageHeaders[tab], info->m_doc);
        if (ui_is_mouse_over_vint_object(header))
        {
            // A static local (guarded by $S10 in rfg.exe).
            static const unsigned int crc_alpha = crc_str("alpha", 0);
            if (header != nullptr)
            {
                header->set_property_value(crc_alpha, vint_variant(1.0f), 0);
            }
            if (mouse_just_released(0))
            {
                // Magic number: Audio_info[9], the page switch sound.
                audiolib_cue_play(Audio_info[9], nullptr, nullptr);
                if (Selected_page_coptions != tab)
                {
                    controller_options_go_to_page(Display_page_start_index + tab);
                }
                return true;
            }
        }
        else if (Selected_page_coptions != tab)
        {
            // Magic number: half opacity.
            ui_pc_set_object_opacity(header, 0.5f);
        }
    }
    const char* arrows[2] = {kPageArrowLeft, kPageArrowRight};
    const int arrow = ui_mouse_process_hints(arrows, 2, Doc_handle_coptions, true);
    if (arrow < 0)
    {
        return false;
    }
    controller_options_step_page(arrow == 0 ? -1 : 1);
    return true;
}

// The spreadsheet row object's name: the group name, then numbered from the second row on.
static void controller_options_row_object_name(char* name, int row)
{
    if (row == 0)
    {
        keen::formatString(name, 0x100, "%s", cConfig_row_group_name);
    }
    else
    {
        keen::formatString(name, 0x100, "%s%d", cConfig_row_group_name, row);
    }
}

// A tint.
static void controller_options_tint(vint_object_base* object, const vint_variant_color& tint)
{
    object->set_property_value(crc_str("tint", 0), vint_variant(tint), 0);
}

// ---- external interface

// 0x0088F700 (:1098)
// Gyro (Steam only): every mode off, every sensitivity at the default row, the default axes; then the mouse sensitivity,
// the default bindings and inversions, the controller APIs shown, the default vehicle camera, look sensitivity, crouch
// hold, zoom toggle and vibration on.
bool controller_options_are_changes_different_from_defaults()
{
    int gyro_rows[8];
    for (int i = 0; i < 4; i++)
    {
        gyro_rows[i * 2] = controller_options_sensitivity_row(input_get_gyro_sensitivity(kGyroModes[i], GYRO_CAMERA_AXIS_X), kGyroSensitivityRange);
        gyro_rows[i * 2 + 1] = controller_options_sensitivity_row(input_get_gyro_sensitivity(kGyroModes[i], GYRO_CAMERA_AXIS_Y), kGyroSensitivityRange);
    }
    if (Gyro_config_allowed)
    {
        for (int i = 0; i < 4; i++)
        {
            if (input_get_gyro_enabled(kGyroModes[i]))
            {
                return true;
            }
        }
        for (int i = 0; i < 8; i++)
        {
            if (gyro_rows[i] != kDefaultSensitivity)
            {
                return true;
            }
        }
        if (!input_gyro_get_orientation_config(GYRO_CAMERA_AXIS_X).is_equal(gyro_axis_config(GYRO_ORIENTATION_Z, 1.0f)))
        {
            return true;
        }
        if (!input_gyro_get_angular_velocity_config(GYRO_CAMERA_AXIS_X).is_equal(gyro_axis_config(GYRO_ANGULAR_VELOCITY_Z, 1.0f)))
        {
            return true;
        }
    }
    if (controller_options_sensitivity_row(Mouse_sensitivity, kSensitivityRange) != kDefaultSensitivity)
    {
        return true;
    }
    control_config defaults;
    memset(&defaults, 0xFF, sizeof(defaults));
    control_config_populate_with_defaults(defaults);
    for (int i = 0; i < 36; i++)
    {
        for (int slot = 0; slot < 2; slot++)
        {
            if (control_config_get_button_for_action(Temporary_current_config, On_foot_buttons[i], slot) != control_config_get_button_for_action(defaults, On_foot_buttons[i], slot))
            {
                return true;
            }
        }
    }
    for (int i = 0; i < 12; i++)
    {
        for (int slot = 0; slot < 2; slot++)
        {
            if (control_config_get_axis_for_action(Temporary_current_config, Stick_controls[i], slot) != control_config_get_axis_for_action(defaults, Stick_controls[i], slot))
            {
                return true;
            }
        }
    }
    for (int i = 0; i < 12; i++)
    {
        for (int slot = 0; slot < 2; slot++)
        {
            if (control_config_get_invert_axis_for_action(Temporary_current_config, Stick_controls[i], slot) != control_config_get_invert_axis_for_action(defaults, Stick_controls[i], slot))
            {
                return true;
            }
        }
    }
    if (!gameseq_state_is_active(GS_WRECKING_CREW_CHARACTER_SELECT) && !wrecking_crew_is_active() && (Hide_xinput || Hide_dinput || Hide_steam_controller))
    {
        return true;
    }
    if (Vehicle_mouse_cam_mode != VCCM_DEFAULT || controller_options_sensitivity_row(Camera_first_person_look_sensitivity_pitch, kSensitivityRange) != kDefaultSensitivity || !Control_crouch_hold || Control_zoom_hold)
    {
        return true;
    }
    return Vibration_config_allowed && !input_get_vibration();
}

// 0x008D6350 (:1235)
void controller_options_set_config_visible(bool visible)
{
    vint_doc_info* info = ui_get_doc_info(Doc_handle_coptions);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    vint_object_base* group = vint_object_base::find_object(cConfig_group_name, info->m_doc);
    if (group != nullptr)
    {
        group->set_property_value(crc_str("visible", 0), vint_variant(visible), 0);
    }
}

// 0x008D63D0 (:1246)
void controller_options_set_menu_visible(bool visible)
{
    vint_doc_info* info = ui_get_doc_info(Doc_handle_coptions);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    vint_object_base* row = vint_object_base::find_object(cConfig_menu_row_name, info->m_doc);
    if (row != nullptr)
    {
        row->set_property_value(crc_str("visible", 0), vint_variant(visible), 0);
    }
    row = vint_object_base::find_object(cConfig_menu_highlight_row_name, info->m_doc);
    if (row != nullptr)
    {
        row->set_property_value(crc_str("visible", 0), vint_variant(visible), 0);
    }
}

// 0x0088FC80 (:1267)
int controller_options_get_num_configurable_controls(controller_option_pages new_page)
{
    if (new_page == CONTROLLER_OPTIONS_PAGE_ON_FOOT)
    {
        return 36;
    }
    if (new_page != CONTROLLER_OPTIONS_PAGE_AXIS)
    {
        return 0;
    }
    return 12;
}

// 0x008D64A0 (:1287)
void controller_options_create_row_data()
{
    vint_data_group* group = vint_data_group::create_new("RowData");
    if (group != nullptr)
    {
        Data_row_group_handle_coptions = group->get_handle();
        ui_add_data_group(Doc_handle_coptions, Data_row_group_handle_coptions);
    }
}

// 0x008D64D0 (:1300)
// {scroll bar shown, scroll position}.
void controller_options_create_header_data()
{
    vint_data_item* item = vint_data_item::create_new("HeaderData");
    if (item == nullptr)
    {
        return;
    }
    vint_variant_type types[2] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_FLOAT};
    item->set_template(types, 2);
    Data_header_handle_coptions = item->get_handle();
    ui_add_data_item(Doc_handle_coptions, Data_header_handle_coptions);
}

// 0x0088FCA0 (:1321)
void Controller_options_process_header_data()
{
    const int count = controller_options_get_num_configurable_controls(Cur_page);
    vint_data_item* item = vint_data_item::find(Data_header_handle_coptions);
    // Original: no null check (and a division by zero on pages without rows).
    item->set_element(0, vint_variant(count > kVisibleRows));
    item->set_element(1, vint_variant((float)Display_start_index_coptions / (float)(count - kVisibleRows)));
}

// 0x0088FD60 (:1337)
// A row's caption and its primary and secondary bindings' names.
void controller_options_get_row_data(controller_option_pages page, int index, const char*& caption, wchar_t* primary, wchar_t* secondary, int str_len)
{
    if (page == CONTROLLER_OPTIONS_PAGE_ON_FOOT)
    {
        caption = On_foot_descriptions[index];
        controls_get_button_name_wide(primary, str_len, control_config_get_button_for_action(Temporary_current_config, On_foot_buttons[index], 0), 0);
        controls_get_button_name_wide(secondary, str_len, control_config_get_button_for_action(Temporary_current_config, On_foot_buttons[index], 1), 0);
        return;
    }
    if (page != CONTROLLER_OPTIONS_PAGE_AXIS)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
    const control_axis_action action = Stick_controls[index];
    caption = Stick_descriptions[index];
    bool inverted = control_config_get_invert_axis_for_action(Temporary_current_config, action, 0);
    controls_get_axis_name_wide(primary, str_len, control_config_get_axis_for_action(Temporary_current_config, action, 0), 0, inverted);
    inverted = control_config_get_invert_axis_for_action(Temporary_current_config, action, 1);
    controls_get_axis_name_wide(secondary, str_len, control_config_get_axis_for_action(Temporary_current_config, action, 1), 0, inverted);
}

// 0x008D6530 (:1372)
// The wheel or the scroll stick scrolls a row; a press on the scroll bar jumps there.
void control_options_process_scrollbar()
{
    if (!mouse_just_pressed(0) && !mouse_just_released(0) && !mouse_is_pressed(0) && !mouse_just_scrolled_wheel() && controls_get_analog(CAA_SCROLL_BAR, Player_controller_index) == 0.0f)
    {
        return;
    }
    int count;
    if (Cur_page == CONTROLLER_OPTIONS_PAGE_ON_FOOT)
    {
        count = 36;
    }
    else if (Cur_page == CONTROLLER_OPTIONS_PAGE_AXIS)
    {
        count = 12;
    }
    else
    {
        return;
    }
    // Magic numbers: Audio_info[1] up, [0] down.
    if (Mouse_dwheel > 0 || controls_get_analog(CAA_SCROLL_BAR, Player_controller_index) > 0.0f)
    {
        if (Display_start_index_coptions > 0)
        {
            audiolib_cue_play(Audio_info[1], nullptr, nullptr);
            Display_start_index_coptions--;
            return;
        }
    }
    else if ((Mouse_dwheel < 0 || controls_get_analog(CAA_SCROLL_BAR, Player_controller_index) < 0.0f) && Display_start_index_coptions < count - kVisibleRows)
    {
        audiolib_cue_play(Audio_info[0], nullptr, nullptr);
        Display_start_index_coptions++;
        return;
    }
    vint_doc_info* info = ui_get_doc_info(Doc_handle_coptions);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    vint_object_base* bar = vint_object_base::find_object(kScrollBarOutline, info->m_doc);
    if (bar == nullptr || !ui_is_mouse_over_vint_object(bar))
    {
        return;
    }
    float x = 0.0f;
    float y = 0.0f;
    float height = 0.0f;
    ui_get_vint_obj_position(bar, x, y);
    ui_get_vint_obj_size(bar, x, height);
    const int start = (int)floor((double)(((Mouse_scaled_y - y) / height) * (float)(count - kVisibleRows) + 0.5f));
    if (start != Display_start_index_coptions)
    {
        audiolib_cue_play(Audio_info[0], nullptr, nullptr);
        Display_start_index_coptions = start;
    }
}

// 0x008FD9A0 (:1571)
// A click (left: bind, right: erase, middle: invert) or the config/erase/invert buttons start the edit of the slot.
void control_options_config_handle_mouseover(int index, control_binding_slot slot)
{
    if (!mouse_just_released(0) && !mouse_just_released(2) && !mouse_just_released(1) && !Controller_config_pressed && !Controller_invert_pressed)
    {
        return;
    }
    Config_right_click = mouse_just_released(2) || Controller_erase_pressed;
    Config_invert_axis_clicked = mouse_just_released(1) || Controller_invert_pressed;
    if (slot != CBS_SECONDARY)
    {
        if (Cur_page == CONTROLLER_OPTIONS_PAGE_ON_FOOT)
        {
            if (On_foot_buttons[index] == CBA_GAMEPLAY_PAUSE)
            {
                return;
            }
        }
        else if (Cur_page != CONTROLLER_OPTIONS_PAGE_AXIS)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
    }
    Config_mouseover_index = index;
    Config_mouseover_slot = slot;
    control_options_start_config_edit();
}

// 0x008FDA90 (:1700)
// The "press a control" box up: a button (escape cancels; some keys are refused) or an axis (or two buttons of the
// same device, one after the other) is confirmed in a box.
void control_options_config_check_input()
{
    if (!Awaiting_config_input || !ui_is_msgbox_active(Config_msgbox_handle))
    {
        return;
    }
    if ((controls_is_any_button_down() != 0xFFFFFFFF || controls_is_any_button_pressed() != 0xFFFFFFFF) && Wait_for_button_up)
    {
        return;
    }
    Wait_for_button_up = false;
    char text[256];
    // Magic number: 0x40000012, escape.
    const unsigned int cancel_button = 0x40000012;
    if (Config_action_type == CAT_BUTTON)
    {
        Config_current_button = 0xFFFFFFFF;
        Config_current_button = controls_is_any_button_pressed();
        if (Config_current_button == 0xFFFFFFFF)
        {
            return;
        }
        if (Config_current_button == cancel_button)
        {
            Config_current_button = 0xFFFFFFFF;
            Awaiting_config_input = false;
            ui_hide_msgbox(Config_msgbox_handle, false);
            return;
        }
        if (!control_options_is_button_choice_valid(Config_current_button))
        {
            Config_current_button = 0xFFFFFFFF;
            return;
        }
        Awaiting_config_input = false;
        ui_hide_msgbox(Config_msgbox_handle, false);
        controls_get_button_image_text_pc(text, sizeof(text), Config_current_button, 0);
        Config_confirm_msgbox_handle = ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_PC_CONFIG_CONFIRM_TITLE", text, control_config_select_cb, false, false, nullptr, nullptr, false);
        return;
    }
    if (Config_action_type != CAT_AXIS)
    {
        Wait_for_button_up = false;
        return;
    }
    Config_current_axis = controls_is_any_axis_pressed();
    if (Config_current_axis != 0xFFFFFFFF && !Awaiting_second_part_of_axis)
    {
        Awaiting_config_input = false;
        ui_hide_msgbox(Config_msgbox_handle, false);
        controls_get_axis_name(text, sizeof(text), Config_current_axis, 0, false);
        Config_confirm_msgbox_handle = ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_PC_CONFIG_CONFIRM_TITLE", text, control_config_select_cb, false, false, nullptr, nullptr, false);
        return;
    }
    const unsigned int button = controls_is_any_button_pressed();
    if (button == 0xFFFFFFFF)
    {
        return;
    }
    if (button == cancel_button)
    {
        Config_current_button = 0xFFFFFFFF;
        Awaiting_second_part_of_axis = false;
        Awaiting_config_input = false;
        ui_hide_msgbox(Config_msgbox_handle, false);
        return;
    }
    // Original: the previous button is validated, not the one just pressed.
    if (!control_options_is_button_choice_valid(Config_current_button))
    {
        return;
    }
    ui_hide_msgbox(Config_msgbox_handle, false);
    if (!Awaiting_second_part_of_axis)
    {
        Awaiting_second_part_of_axis = true;
        Config_current_button = button;
        Config_msgbox_handle = ui_add_msgbox(MBT_CONFIG_CONTROL, "MENU_PC_CONFIG_CONFIRM_TITLE", "MENU_PC_CONFIG_AXIS_SECOND", nullptr, false, false, nullptr, nullptr, false);
        Awaiting_config_input = true;
        return;
    }
    // Magic numbers: the top nibble is the device; an axis of two buttons packs the first in the high bits with the
    // device's button-axis flag (0x80000 keys, 0x280000 0x2 devices, 0x480000 0x4 devices).
    const unsigned int device = Config_current_button & 0xF0000000;
    Awaiting_config_input = false;
    if (device != (button & 0xF0000000))
    {
        Config_confirm_msgbox_handle = ui_add_msgbox(MBT_OK, cAxis_config_error_title, cMismatched_axis_combo_desc, nullptr, false, false, nullptr, nullptr, false);
        return;
    }
    if (Config_current_button == button)
    {
        Config_confirm_msgbox_handle = ui_add_msgbox(MBT_OK, cAxis_config_error_title, cRepeated_axis_combo_desc, nullptr, false, false, nullptr, nullptr, false);
        return;
    }
    if (device == 0)
    {
        Config_current_axis = (Config_current_button | 0x80000) << 8 | button;
    }
    else if (device == 0x20000000)
    {
        Config_current_axis = (Config_current_button | 0x280000) << 8 | button;
    }
    else if (device == 0x40000000)
    {
        Config_current_axis = (Config_current_button | 0x480000) << 8 | button;
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    controls_get_axis_name(text, sizeof(text), Config_current_axis, 0, false);
    Config_confirm_msgbox_handle = ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_PC_CONFIG_CONFIRM_TITLE", text, control_config_select_cb, false, false, nullptr, nullptr, false);
}

// 0x008FDE30 (:1848)
// The spreadsheet (when shown): the scroll bar, every row's slots dark, the slot under a moved/clicking mouse edited
// (and highlighted), the highlighted slot white.
void control_options_check_config_clicks(controller_option_pages page)
{
    // Magic numbers: 0x40 / 255 for the dark tint.
    cColorDark.r = 0.2509804f;
    cColorDark.g = 0.2509804f;
    cColorDark.b = 0.2509804f;
    cColorWhite.r = 1.0f;
    cColorWhite.g = 1.0f;
    cColorWhite.b = 1.0f;
    control_options_process_scrollbar();
    vint_doc_info* info = ui_get_doc_info(Doc_handle_coptions);
    if (info == nullptr || info->m_doc == nullptr || cConfig_group_name == nullptr)
    {
        return;
    }
    vint_document* doc = info->m_doc;
    if (!ui_vint_object_visible(vint_object_base::find_object(cConfig_group_name, doc)))
    {
        return;
    }
    vint_data_group* group = vint_data_group::find(Data_row_group_handle_coptions);
    if (group == nullptr)
    {
        return;
    }
    const bool mouse_moved = mouse_just_moved();
    const bool clicked = mouse_just_released(0) || mouse_just_released(2) || mouse_just_released(1);
    char name[260];
    for (unsigned int row = 0; row < group->get_num_data_items(); row++)
    {
        controller_options_row_object_name(name, (int)row);
        vint_object_base* row_object = vint_object_base::find_object(name, doc);
        if (row_object == nullptr)
        {
            return;
        }
        vint_object_base* primary = row_object->find_child(crc_str(cConfig_row_primary_name, 0));
        if (primary == nullptr)
        {
            return;
        }
        controller_options_tint(primary, cColorDark);
        if (ui_is_mouse_over_vint_object(primary))
        {
            if (mouse_moved || clicked)
            {
                Highlighted_col = 0;
                Highlighted_row = (int)row;
                control_options_config_handle_mouseover(Display_start_index_coptions + (int)row, CBS_PRIMARY);
            }
            vint_object_base* secondary = ui_get_parented_vint_object(name, cConfig_row_secondary_name, doc);
            if (secondary == nullptr)
            {
                return;
            }
            controller_options_tint(secondary, cColorDark);
        }
        else
        {
            vint_object_base* secondary = ui_get_parented_vint_object(name, cConfig_row_secondary_name, doc);
            // Original: no null check here.
            controller_options_tint(secondary, cColorDark);
            if (ui_is_mouse_over_vint_object(secondary) && (mouse_moved || clicked))
            {
                Highlighted_col = 1;
                Highlighted_row = (int)row;
                control_options_config_handle_mouseover(Display_start_index_coptions + (int)row, CBS_SECONDARY);
            }
        }
    }
    if ((int)group->get_num_data_items() <= Highlighted_row)
    {
        Highlighted_row = (int)group->get_num_data_items() - 1;
    }
    controller_options_row_object_name(name, Highlighted_row);
    vint_object_base* row_object = vint_object_base::find_object(name, doc);
    if (row_object != nullptr)
    {
        vint_object_base* slot = row_object->find_child(crc_str(Highlighted_col != 0 ? cConfig_row_secondary_name : cConfig_row_primary_name, 0));
        if (slot != nullptr)
        {
            controller_options_tint(slot, cColorWhite);
        }
    }
}

// 0x00890060 (:1942)
// The spreadsheet's nine rows (made or removed to match): {number, caption, primary, secondary, selected, false, odd
// row}.
void controller_options_process_rows()
{
    if (Cur_page != CONTROLLER_OPTIONS_PAGE_ON_FOOT && Cur_page != CONTROLLER_OPTIONS_PAGE_AXIS)
    {
        return;
    }
    const char* caption = nullptr;
    vint_data_group* group = vint_data_group::find(Data_row_group_handle_coptions);
    if (group != nullptr)
    {
        int count = (int)group->get_num_data_items();
        const int wanted = kVisibleRows;
        if (count < wanted)
        {
            for (; count < wanted; count++)
            {
                vint_data_item* item = group->add_data_item(nullptr);
                if (item != nullptr)
                {
                    vint_variant_type types[7] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL};
                    item->set_template(types, 7);
                    Data_row_handles_coptions[count] = item->get_handle();
                }
            }
        }
        else
        {
            while (--count >= wanted)
            {
                group->remove_data_item(Data_row_handles_coptions[count]);
            }
        }
    }
    // Original: the group is used without a null check from here on.
    for (unsigned int row = 0; row < group->get_num_data_items(); row++)
    {
        vint_data_item* item = vint_data_item::find(Data_row_handles_coptions[row]);
        if (item == nullptr)
        {
            continue;
        }
        const int index = Display_start_index_coptions + (int)row;
        wchar_t primary[256];
        wchar_t secondary[256];
        char primary_text[768];
        char secondary_text[768];
        controller_options_get_row_data(Cur_page, index, caption, primary, secondary, 0x100);
        vint_encode_wide_text(primary_text, 0x300, primary);
        vint_encode_wide_text(secondary_text, 0x300, secondary);
        item->set_element(0, vint_variant((int)row + 1));
        item->set_element(1, vint_variant(caption));
        item->set_element(2, vint_variant((const char*)primary_text));
        item->set_element(3, vint_variant((const char*)secondary_text));
        item->set_element(4, vint_variant(Selected_row_coptions == (int)row));
        item->set_element(5, vint_variant(false));
        item->set_element(6, vint_variant(index % 2 == 0));
    }
}

// 0x00890370 (:2054)
// Every button's image name.
void get_button_images()
{
    for (unsigned int button = 0; button < 128; button++)
    {
        controls_get_button_image_name(cButton_image[button], 0x100, button, 0);
    }
}

// 0x008E68A0 (:2061)
// The buttons page: each button's actions ("a / b" when it has several).
void get_text_for_buttons()
{
    for (int i = 0; i < 128; i++)
    {
        cButton_text[i][0] = '\0';
    }
    if (Cur_page != CONTROLLER_OPTIONS_PAGE_ON_FOOT)
    {
        return;
    }
    for (int i = 0; i < 36; i++)
    {
        const unsigned int button = (unsigned int)controls_get_button_for_action(On_foot_buttons[i], Player_controller_index);
        if (button >= 128)
        {
            continue;
        }
        const wchar_t* description = localization_get_display_string_req(On_foot_descriptions[i], nullptr);
        wchar_t text[64];
        if (cButton_text[button][0] == '\0')
        {
            _snprintf_safe(text, 0x40, L"%ls", description);
        }
        else
        {
            wchar_t previous[64];
            vint_decode_wide_text(previous, 0x40, cButton_text[button]);
            _snprintf_safe(text, 0x40, L"%ls / %ls", previous, description);
        }
        vint_encode_wide_text(cButton_text[button], 0x100, text);
    }
}

// 0x008C2E90 (:2134)
// The current config's defaults, the controller APIs shown, the default camera, crouch hold, zoom toggle, the default
// sensitivities, vibration on, every gyro mode off at the default sensitivity and axes; the page remade on the same row.
void controller_option_reset_defaults()
{
    Temporary_current_config = controller_options_current_config();
    control_config_populate_with_defaults(Temporary_current_config);
    if (!gameseq_state_is_active(GS_WRECKING_CREW_CHARACTER_SELECT) && !wrecking_crew_is_active())
    {
        Hide_xinput = false;
        Hide_dinput = false;
        Hide_steam_controller = false;
    }
    Vehicle_mouse_cam_mode = VCCM_DEFAULT;
    Control_crouch_hold = true;
    Control_zoom_hold = false;
    Mouse_sensitivity = controller_options_sensitivity_value(kDefaultSensitivity, kSensitivityRange);
    Camera_first_person_look_sensitivity_pitch = Mouse_sensitivity;
    Camera_first_person_look_sensitivity_heading = Mouse_sensitivity;
    if (Vibration_config_allowed)
    {
        input_set_vibration(true);
    }
    if (Gyro_config_allowed)
    {
        const float sensitivity = controller_options_sensitivity_value(kDefaultSensitivity, kGyroSensitivityRange);
        for (int i = 0; i < 4; i++)
        {
            input_set_gyro_enabled(kGyroModes[i], false);
        }
        for (int i = 0; i < 4; i++)
        {
            input_set_gyro_sensitivity(kGyroModes[i], GYRO_CAMERA_AXIS_X, sensitivity);
            input_set_gyro_sensitivity(kGyroModes[i], GYRO_CAMERA_AXIS_Y, sensitivity);
        }
        input_gyro_set_orientation_config(GYRO_CAMERA_AXIS_X, gyro_axis_config(GYRO_ORIENTATION_Z, 1.0f));
        input_gyro_set_angular_velocity_config(GYRO_CAMERA_AXIS_X, gyro_axis_config(GYRO_ANGULAR_VELOCITY_Z, 1.0f));
    }
    const int selected = ui_menu_get_selected_item(Menu_handle_coptions);
    int enabled_index = 0;
    for (int page = 0; page < (int)Cur_page; page++)
    {
        if (cPage_enabled[page])
        {
            enabled_index++;
        }
    }
    set_controller_page(enabled_index);
    ui_menu_set_selected_item(Menu_handle_coptions, selected);
}

// A menu hint: the tag's localized text with its button markup, and the button's image.
static void controller_options_add_hint(hint_type ht, control_button_action action, const char* tag)
{
    char text[128];
    char image[256];
    // Magic number: 0x80, the hint text buffer's size.
    controls_fill_options_button_hint(text, 0x80, localization_get_display_string_req(tag, nullptr));
    ui_menu* menu = ui_menu_get_via_handle(Menu_handle_coptions);
    if (menu != nullptr)
    {
        // Original: 0x40 passed as the image buffer's size.
        controls_get_button_image_name(image, 0x40, action, Player_controller_index);
        menu->add_hint(ht, action, text, image);
    }
}

// A misc hint's text.
static void controller_options_misc_hint_text(char* text, const char* tag)
{
    controls_fill_options_button_hint(text, 0x80, localization_get_display_string_req(tag, nullptr));
}

// A sensitivity row from a value, the default row when out of range.
static int controller_options_clamped_sensitivity_row(float value, float range)
{
    const int row = controller_options_sensitivity_row(value, range);
    return row < 0 || row > Num_sensitivities ? kDefaultSensitivity : row;
}

// 0x008FE4C0 (:2979)
// The index-th enabled page (none: general) in a new menu (the old one goes after): general's rows (camera and mouse
// look, crouch, zoom, vibration, the controller APIs (not in wrecking crew), the vehicle camera; inversion off for
// Steam), or the gyro rows (Steam; satellite hidden); the buttons and axes pages are spreadsheets. The hints, the button
// texts, the spreadsheet or the menu shown.
void set_controller_page(int index)
{
    if (Menu_handle_coptions != -1)
    {
        ui_menu_destroy(Menu_handle_coptions);
    }
    controller_option_pages page = CONTROLLER_OPTIONS_PAGE_GENERAL;
    int enabled_index = -1;
    for (int i = 0; i < NUM_CONTROLLER_OPTIONS_PAGES; i++)
    {
        if (cPage_enabled[i])
        {
            enabled_index++;
        }
        if (enabled_index == index)
        {
            page = (controller_option_pages)i;
            break;
        }
    }
    Display_start_index_coptions = 0;
    const int old_menu = Menu_handle_coptions;
    Menu_handle_coptions = ui_menu_add(Doc_handle_coptions, cPage_names[page], nullptr, controller_options_hint_callback, "CM", false, -1, -1, false);
    if (page == CONTROLLER_OPTIONS_PAGE_GENERAL)
    {
        const bool inverted_x = controls_get_axis_action_inverted_state(CAA_CAMERA_ROTATE, Temporary_current_config, true);
        const bool inverted_y = controls_get_axis_action_inverted_state(CAA_CAMERA_ELEVATE, Temporary_current_config, true);
        ui_menu_add_char_list_item(Menu_handle_coptions, "MENU_X_AXIS", cX_axis_list, 2, controller_option_change_cb, inverted_x ? 1 : 0, true);
        ui_menu_add_char_list_item(Menu_handle_coptions, "MENU_Y_AXIS", cY_axis_list, 2, controller_option_change_cb, inverted_y ? 1 : 0, true);
        const int look_row = controller_options_clamped_sensitivity_row(Camera_first_person_look_sensitivity_pitch, kSensitivityRange);
        const int mouse_row = controller_options_clamped_sensitivity_row(Mouse_sensitivity, kSensitivityRange);
        ui_menu_add_char_list_item(Menu_handle_coptions, "MENU_SENSITIVITY", cSensitivity_list, Num_sensitivities, controller_option_change_cb, look_row, true);
        ui_menu_add_char_list_item(Menu_handle_coptions, "MENU_CROUCH_BUTTON", cCrouch_list, 2, controller_option_change_cb, Control_crouch_hold ? 0 : 1, true);
        ui_menu_add_char_list_item(Menu_handle_coptions, "MENU_ZOOM", cCrouch_list, 2, controller_option_change_cb, Control_zoom_hold ? 0 : 1, true);
        ui_menu_add_char_list_item(Menu_handle_coptions, "MENU_VIBRATION", cVibration_list, 2, controller_option_change_cb, input_get_vibration() ? 0 : 1, true);
        ui_menu_add_char_list_item(Menu_handle_coptions, "MENU_MOUSE_SENSITIVITY", cSensitivity_list, Num_sensitivities, controller_option_change_cb, mouse_row, true);
        ui_menu_add_char_list_item(Menu_handle_coptions, "MENU_HIDE_XINPUT", cGyro_enabled_list, 2, controller_option_change_cb, Hide_xinput ? 0 : 1, true);
        ui_menu_add_char_list_item(Menu_handle_coptions, "MENU_HIDE_DINPUT", cGyro_enabled_list, 2, controller_option_change_cb, Hide_dinput ? 0 : 1, true);
        ui_menu_add_char_list_item(Menu_handle_coptions, "MENU_HIDE_STEAM_CONTROLLER", cGyro_enabled_list, 2, controller_option_change_cb, Hide_steam_controller ? 0 : 1, true);
        ui_menu_add_char_list_item(Menu_handle_coptions, "MENU_VEHICLE_CAMERA_MODE", cVehicle_mouse_cam_config, 2, controller_option_change_cb, Vehicle_mouse_cam_mode, true);
        if (gameseq_state_is_active(GS_WRECKING_CREW_CHARACTER_SELECT) || wrecking_crew_is_active())
        {
            // Magic numbers: rows 7-9, the controller APIs.
            ui_menu_enable_menu_item(Menu_handle_coptions, 7, false);
            ui_menu_enable_menu_item(Menu_handle_coptions, 8, false);
            ui_menu_enable_menu_item(Menu_handle_coptions, 9, false);
        }
        // Magic number: row 5, vibration.
        ui_menu_enable_menu_item(Menu_handle_coptions, 5, Vibration_config_allowed);
        if (controls_get_current_config_setting() == CCS_STEAM)
        {
            ui_menu_enable_menu_item(Menu_handle_coptions, 0, false);
            ui_menu_enable_menu_item(Menu_handle_coptions, 1, false);
            ui_menu_show_menu_item(Menu_handle_coptions, 0, false);
            ui_menu_show_menu_item(Menu_handle_coptions, 1, false);
        }
    }
    else if (page == CONTROLLER_OPTIONS_PAGE_GYRO_CONTROLS)
    {
        int rows[8];
        for (int i = 0; i < 4; i++)
        {
            rows[i * 2] = controller_options_sensitivity_row(input_get_gyro_sensitivity(kGyroModes[i], GYRO_CAMERA_AXIS_X), kGyroSensitivityRange);
            rows[i * 2 + 1] = controller_options_sensitivity_row(input_get_gyro_sensitivity(kGyroModes[i], GYRO_CAMERA_AXIS_Y), kGyroSensitivityRange);
        }
        for (int i = 0; i < 8; i++)
        {
            if (rows[i] < 0 || rows[i] > Num_sensitivities)
            {
                rows[i] = kDefaultSensitivity;
            }
        }
        static const char* const mode_captions[4] = {"MENU_GYRO_CONTROLS_THIRD_PERSON", "MENU_GYRO_CONTROLS_VEHICLES", "MENU_GYRO_CONTROLS_TURRETS", "MENU_GYRO_CONTROLS_SATELLITE"};
        for (int i = 0; i < 4; i++)
        {
            ui_menu_add_char_list_item(Menu_handle_coptions, mode_captions[i], cGyro_enabled_list, 2, controller_option_change_cb, input_get_gyro_enabled(kGyroModes[i]) ? 0 : 1, true);
            ui_menu_add_char_list_item(Menu_handle_coptions, "MENU_X_SENSITIVITY", cSensitivity_list, Num_sensitivities, controller_option_change_cb, rows[i * 2], true);
            ui_menu_add_char_list_item(Menu_handle_coptions, "MENU_Y_SENSITIVITY", cSensitivity_list, Num_sensitivities, controller_option_change_cb, rows[i * 2 + 1], true);
        }
        for (int i = 0; i < 4; i++)
        {
            if (!input_get_gyro_enabled(kGyroModes[i]))
            {
                ui_menu_enable_menu_item(Menu_handle_coptions, i * 3 + 1, false);
                ui_menu_enable_menu_item(Menu_handle_coptions, i * 3 + 2, false);
            }
        }
        // Magic numbers: rows 9-11, the satellite, disabled and hidden.
        for (int row = 9; row < 12; row++)
        {
            ui_menu_enable_menu_item(Menu_handle_coptions, row, false);
        }
        for (int row = 9; row < 12; row++)
        {
            ui_menu_show_menu_item(Menu_handle_coptions, row, false);
        }
        const gyro_axis_config& orientation_x = input_gyro_get_orientation_config(GYRO_CAMERA_AXIS_X);
        int axis_choice = 0;
        for (int i = 0; i < 2; i++)
        {
            if (orientation_x.sensor_id == cGyro_orientation_axes[i].sensor_id && cGyro_orientation_axes[i].direction * orientation_x.direction > 0.0f)
            {
                axis_choice = i;
                break;
            }
        }
        // Original: the y orientation is looked up the same way and never used.
        ui_menu_add_char_list_item(Menu_handle_coptions, "MENU_GYRO_HORZ_MOVEMENT", cGyro_axes_list, 2, controller_option_change_cb, axis_choice, true);
    }
    controller_options_add_hint(HT_SELECTION, CBA_MENU_SELECT, "MENU_ACCEPT_WITH_BUTTON");
    controller_options_add_hint(HT_ABORT, CBA_MENU_ABORT, "MENU_CANCEL_WITH_BUTTON");
    char text[128];
    controller_options_misc_hint_text(text, "MENU_DEFAULT_SETTINGS");
    ui_menu_add_misc_hint(Menu_handle_coptions, text, CBA_MENU_Y);
    if (controls_get_current_config_setting() == CCS_STEAM)
    {
        controller_options_misc_hint_text(text, "MENU_STEAM_CONFIG_ACTIONS");
        ui_menu_add_misc_hint_2(Menu_handle_coptions, text, CBA_MENU_X);
    }
    if (Cur_page != CONTROLLER_OPTIONS_PAGE_INVALID && old_menu != -1)
    {
        ui_menu_destroy(old_menu);
    }
    Cur_page = page;
    get_text_for_buttons();
    const bool spreadsheet = page != CONTROLLER_OPTIONS_PAGE_GENERAL && page != CONTROLLER_OPTIONS_PAGE_GYRO_CONTROLS;
    controller_options_set_config_visible(spreadsheet);
    controller_options_set_menu_visible(!spreadsheet);
}

// 0x008D67A0 (:3273)
void controller_options_create_button_image_data_item()
{
    vint_data_item* item = vint_data_item::create_new("ButtonImages");
    if (item == nullptr)
    {
        return;
    }
    vint_variant_type types[15];
    for (int i = 0; i < 15; i++)
    {
        types[i] = VINT_PROP_TYPE_STRING;
    }
    item->set_template(types, 15);
    Data_images_handle = item->get_handle();
    if (!ui_add_data_item(Doc_handle_coptions, Data_images_handle))
    {
        item->destroy();
        Data_images_handle = 0;
    }
}

// 0x008D6860 (:3316)
// {page name, (unused bool), controller image, then button texts}.
void controller_options_create_button_text_data_item()
{
    vint_data_item* item = vint_data_item::create_new("Controls");
    if (item == nullptr)
    {
        return;
    }
    vint_variant_type types[19];
    for (int i = 0; i < 19; i++)
    {
        types[i] = VINT_PROP_TYPE_STRING;
    }
    types[1] = VINT_PROP_TYPE_BOOL;
    item->set_template(types, 19);
    Data_text_handle = item->get_handle();
    if (!ui_add_data_item(Doc_handle_coptions, Data_text_handle))
    {
        item->destroy();
        Data_text_handle = 0;
    }
}

// 0x008D6930 (:3386)
// {three tab names, page, can go left, can go right, L2 image, R2 image, (unused)}.
void controller_options_create_tab_data_item()
{
    vint_data_item* item = vint_data_item::create_new("PageTabData");
    if (item == nullptr)
    {
        return;
    }
    vint_variant_type types[9] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
    item->set_template(types, 9);
    Page_tab_handle = item->get_handle();
    ui_add_data_item(Doc_handle_coptions, Page_tab_handle);
}

// The controller picture's buttons, in the document's element order (button indices).
static const int kControllerPictureButtons[15] = {0, 1, 2, 3, 4, 6, 5, 7, 14, 15, 12, 13, 9, 11, 8};

// 0x00890460 (:3413)
void controller_options_process_button_images()
{
    vint_data_item* item = vint_data_item::find(Data_images_handle);
    if (item == nullptr)
    {
        return;
    }
    for (int i = 0; i < 15; i++)
    {
        item->set_element(i, vint_variant((const char*)cButton_image[kControllerPictureButtons[i]]));
    }
}

// A button text (none when empty).
static const char* controller_options_button_text(int button)
{
    return cButton_text[button][0] != '\0' ? cButton_text[button] : nullptr;
}

// 0x008E6A20 (:3440)
// The page name, the controller picture (by controller type; none with mouse and keyboard), the buttons' texts (one
// unused slot).
void controller_options_process_button_text()
{
    vint_data_item* item = vint_data_item::find(Data_text_handle);
    if (item == nullptr)
    {
        return;
    }
    item->set_element(0, vint_variant(cPage_names[Cur_page]));
    item->set_element(1, vint_variant(false));
    if (input_get_playing_with_controller())
    {
        const keen::ControllerType type = input_get_controller_type();
        if (type == keen::ControllerType_DualShock4)
        {
            item->set_element(2, vint_variant(cImagename_ps4));
        }
        else if (type == keen::ControllerType_Xbox360)
        {
            item->set_element(2, vint_variant(cImagename_xbox));
        }
        else if (type == keen::ControllerType_XBone)
        {
            item->set_element(2, vint_variant(cImagename_xbone));
        }
    }
    else
    {
        item->set_element(2, vint_variant(""));
    }
    for (int i = 0; i < 12; i++)
    {
        item->set_element(3 + i, vint_variant(controller_options_button_text(kControllerPictureButtons[i])));
    }
    wchar_t unused[64];
    char unused_text[192];
    _snprintf_safe(unused, 0x40, L"%ls", localization_get_display_string_req(cUnused_string_tag, nullptr));
    vint_encode_wide_text(unused_text, 0xC0, unused);
    item->set_element(15, vint_variant((const char*)unused_text));
    item->set_element(16, vint_variant(controller_options_button_text(9)));
    item->set_element(17, vint_variant(controller_options_button_text(11)));
    item->set_element(18, vint_variant(controller_options_button_text(8)));
}

// 0x008D69C0 (:3755)
// The hint objects' mouse highlight.
void controller_options_animate_mouse_buttons()
{
    const char* names[4] = {kHint1, kHint2, kHint3, kHint4};
    vint_object_base* objects[4] = {nullptr, nullptr, nullptr, nullptr};
    vint_doc_info* info = ui_get_doc_info(Doc_handle_coptions);
    if (info != nullptr && info->m_doc != nullptr)
    {
        for (int i = 0; i < 4; i++)
        {
            vint_object_base* object = vint_object_base::find_object(names[i], info->m_doc);
            if (object == nullptr)
            {
                break;
            }
            objects[i] = object;
        }
    }
    ui_mouse_process_clickables(objects, 4, true);
}

// 0x00900690 (:3774)
// The hints, the page tabs, the menu rows (a released click selects; on the arrows: left or right), the spreadsheet.
void controller_options_process_mouse()
{
    const char* names[4] = {kHint1, kHint2, kHint3, kHint4};
    vint_object_base* objects[4] = {nullptr, nullptr, nullptr, nullptr};
    vint_doc_info* info = ui_get_doc_info(Doc_handle_coptions);
    if (info != nullptr && info->m_doc != nullptr)
    {
        for (int i = 0; i < 4; i++)
        {
            vint_object_base* object = vint_object_base::find_object(names[i], info->m_doc);
            if (object == nullptr)
            {
                break;
            }
            objects[i] = object;
        }
    }
    const int hint = ui_mouse_process_clickables(objects, 4, true);
    if (hint > -1)
    {
        controller_options_hint_callback((hint_type)hint);
        return;
    }
    if (controller_options_process_mouse_tab_input())
    {
        return;
    }
    const int index = ui_menu_process_mouseover(Doc_handle_coptions, Menu_handle_coptions, kControlsRowMain);
    if (index > -1 && Mouse_buttons[0].just_released)
    {
        info = ui_get_doc_info(Doc_handle_coptions);
        if (info == nullptr || info->m_doc == nullptr)
        {
            return;
        }
        bool left_arrow = false;
        bool right_arrow = false;
        if (ui_vint_object_visible(kControlArrows, info->m_doc))
        {
            left_arrow = ui_is_mouse_over_vint_object(kControlArrowLeft, info->m_doc);
            if (!left_arrow)
            {
                right_arrow = ui_is_mouse_over_vint_object(kControlRowHighlightBG, kControlRowHighlightMain, info->m_doc);
            }
        }
        ui_menu_selection_clicked(Menu_handle_coptions, index, left_arrow, right_arrow);
    }
    control_options_check_config_clicks(Cur_page);
}

// 0x008906E0 (:3822)
// The visible tabs' names (from the window's start, wrapping, enabled pages only; empty after), the selected tab, the
// arrows, L2/R2 images.
void controller_options_process_tab_data()
{
    vint_data_item* item = vint_data_item::find(Page_tab_handle);
    if (item == nullptr)
    {
        return;
    }
    char l2_image[32];
    char r2_image[32];
    char l2_text[128];
    char r2_text[128];
    controls_get_button_image_name(l2_image, sizeof(l2_image), CBA_MENU_L2, Player_controller_index);
    controls_get_button_image_name(r2_image, sizeof(r2_image), CBA_MENU_R2, Player_controller_index);
    keen::formatString(l2_text, sizeof(l2_text), "[image:%s]", l2_image);
    keen::formatString(r2_text, sizeof(r2_text), "[image:%s]", r2_image);
    int element = 0;
    int page = Display_page_start_index;
    while (element < Conoptions_current_num_visible_tabs)
    {
        if (cPage_enabled[page])
        {
            item->set_element(element, vint_variant(cPage_names[page]));
            element++;
        }
        // Magic number: four pages (wrapping).
        page = (page + 1) % 4;
    }
    for (int empty = Conoptions_current_num_visible_tabs; empty < kNumVisibleTabs; empty++)
    {
        item->set_element(empty, vint_variant(""));
    }
    item->set_element(3, vint_variant(Selected_page_coptions < Conoptions_current_num_visible_tabs ? Selected_page_coptions + 1 : 1));
    item->set_element(4, vint_variant(Display_page_start_index + Selected_page_coptions > 0));
    item->set_element(5, vint_variant(Display_page_start_index + Selected_page_coptions < Conoptions_current_num_pages - 1));
    item->set_element(6, vint_variant((const char*)l2_text));
    item->set_element(7, vint_variant((const char*)r2_text));
}

// 0x00900800 (:3872)
// Spreadsheet pages: left/right switch the column, down/up the row (scrolling, wrapping); the config (button 14), erase
// (16) and invert (18) buttons edit the highlighted slot. L2/R2 (17, 19) switch pages.
void controller_options_process_input()
{
    const int input_id = Input_id_coptions;
    Controller_config_pressed = false;
    Controller_erase_pressed = false;
    Controller_invert_pressed = false;
    // Original: the tab index (not the page) decides; magic numbers: buttons 3/4 left/right, 2 down, 1 up.
    if (Selected_page_coptions != 0 && Selected_page_coptions != 3)
    {
        if (ui_get_button_info(3, input_id) > 0.0f || ui_get_button_info(4, input_id) > 0.0f)
        {
            Highlighted_col = Highlighted_col == 0 ? 1 : 0;
            return;
        }
        if (ui_get_button_info(2, input_id) > 0.0f)
        {
            if (Highlighted_row + 1 < kVisibleRows)
            {
                Highlighted_row++;
                return;
            }
            Highlighted_row = kVisibleRows - 1;
            if (controller_options_get_num_configurable_controls((controller_option_pages)(Display_page_start_index + Selected_page_coptions)) <= Display_start_index_coptions + kVisibleRows)
            {
                Display_start_index_coptions = 0;
                Highlighted_row = 0;
                Selected_row_coptions = 0;
                return;
            }
            Display_start_index_coptions++;
            return;
        }
        if (ui_get_button_info(1, input_id) > 0.0f)
        {
            Highlighted_row--;
            if (Highlighted_row > -1)
            {
                return;
            }
            Highlighted_row = 0;
            if (Display_start_index_coptions < 1)
            {
                Display_start_index_coptions = controller_options_get_num_configurable_controls((controller_option_pages)(Display_page_start_index + Selected_page_coptions)) - kVisibleRows;
                if (Display_start_index_coptions < 0)
                {
                    Display_start_index_coptions = 0;
                }
                Highlighted_row = kVisibleRows - 1;
                Selected_row_coptions = kVisibleRows - 1;
                return;
            }
            Display_start_index_coptions--;
            return;
        }
    }
    if (Cur_page != CONTROLLER_OPTIONS_PAGE_GENERAL && Cur_page != CONTROLLER_OPTIONS_PAGE_GYRO_CONTROLS)
    {
        const int index = Display_start_index_coptions + Highlighted_row;
        const control_binding_slot slot = Highlighted_col != 0 ? CBS_SECONDARY : CBS_PRIMARY;
        // Original: these check the focus of input id -1 (any input while nothing is locked), not this menu's.
        if (controller_input_has_focus(-1) && controller_button_pressed(0xE))
        {
            ui_wait_for_button_ups();
            if (!control_options_can_configure_control(index, slot))
            {
                return;
            }
            Controller_config_pressed = true;
            control_options_config_handle_mouseover(index, slot);
            Wait_for_button_up = true;
            return;
        }
        if (controller_input_has_focus(-1) && controller_button_pressed(0x10))
        {
            if (!control_options_can_configure_control(index, slot))
            {
                return;
            }
            ui_wait_for_button_ups();
            Controller_erase_pressed = true;
            control_options_config_handle_mouseover(index, slot);
            Wait_for_button_up = true;
            return;
        }
        if (controller_input_has_focus(-1) && controller_button_pressed(0x12))
        {
            if (!control_options_can_configure_control(index, slot))
            {
                return;
            }
            ui_wait_for_button_ups();
            Controller_invert_pressed = true;
            control_options_config_handle_mouseover(index, slot);
            Wait_for_button_up = true;
            return;
        }
    }
    if (!controller_input_has_focus(input_id))
    {
        return;
    }
    if (controller_button_pressed(0x11))
    {
        controller_options_step_page(-1);
        return;
    }
    if (controller_button_pressed(0x13))
    {
        controller_options_step_page(1);
    }
}

// 0x0090F2A0 (:4064)
// Wrecking crew's character select edits the selected player's controls; otherwise the options are read (in wrecking
// crew only with no current player).
// The document, a control id and an input id, the button images (not Steam), the invalid mappings removed, the menu
// character away, the current config copied, the controller APIs, the vehicle camera; the pages by config (Steam: no
// buttons/axes pages; gyro only with Steam).
void controller_options_enter(game_state prev_state)
{
    Assets_loaded_coptions = false;
    Use_multiplayer_controls = Multiplayer;
    if (gameseq_state_is_active(GS_WRECKING_CREW_CHARACTER_SELECT))
    {
        // The main player's controls backed up, the selected wrecking crew player's loaded (controller_options_exit
        // reverses it).
        const int player = controller_options_wc_selected_player();
        Control_options_main_player_ctrl_config_backup = controller_options_current_config();
        controller_options_get_control_config(Control_options_main_player_control_options_backup);
        controller_options_current_config() = Wc_player_infos[player].ctrl_config;
        controller_options_set_control_config(Wc_player_infos[player].control_options);
    }
    else if (!wrecking_crew_is_active() || Wrecking_crew_current_player < 0)
    {
        game_options_load_from_cache();
    }
    Doc_handle_coptions = ui_load_document(cDocname_pc, DFIT_NONE, 0.0f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    Control_id_coptions = (int)ui_request_control_id();
    Input_id_coptions = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    if (controls_get_current_config_setting() != CCS_STEAM)
    {
        get_button_images();
    }
    controls_remove_invalid_mappings();
    Obj_pos = Off_screen_pos;
    Change_made_coptions = false;
    Temporary_current_config = controller_options_current_config();
    Configuration_changed = false;
    Custom_config_changes_made = false;
    Hide_xinput = keen::input::getControllerApiHidden(Keen_input_system, keen::WindowsControllerApi_XInput);
    Hide_dinput = keen::input::getControllerApiHidden(Keen_input_system, keen::WindowsControllerApi_DInput);
    Hide_steam_controller = keen::input::getControllerApiHidden(Keen_input_system, keen::WindowsControllerApi_SteamController);
    Vehicle_mouse_cam_mode = (vehicle_camera_control_mode)Camera_free_mouse_control_mode;
    Selected_page_coptions = 0;
    Display_page_start_index = 0;
    Display_start_index_coptions = 0;
    if (!Active)
    {
        main_menu_show_menu(false);
    }
    else
    {
        ingame_options_show_menu(false);
    }
    check_stick_config_allowed();
    for (int i = 0; i < 4; i++)
    {
        cPage_enabled[i] = true;
    }
    Conoptions_current_num_pages = 4;
    if (controls_get_current_config_setting() == CCS_STEAM)
    {
        Conoptions_current_num_pages -= 2;
        cPage_enabled[CONTROLLER_OPTIONS_PAGE_ON_FOOT] = false;
        cPage_enabled[CONTROLLER_OPTIONS_PAGE_AXIS] = false;
    }
    if (!Gyro_config_allowed)
    {
        Conoptions_current_num_pages--;
        cPage_enabled[CONTROLLER_OPTIONS_PAGE_GYRO_CONTROLS] = false;
    }
    Conoptions_current_num_visible_tabs = Conoptions_current_num_pages < kNumVisibleTabs ? Conoptions_current_num_pages : kNumVisibleTabs;
}

// 0x00915AE0 (:4207)
// A finished save: its box goes (failed: answered as cancel), the menu leaves. The data items and the first page once
// the document is loaded; then the mouse (or the "press a control" box), the input and every data item.
void controller_options_process()
{
    if (gameseq_state_is_active(GS_WRECKING_CREW_CHARACTER_SELECT) && Bink_info_main.handle != 0)
    {
        mm_process_bink_video();
    }
    if (Waiting_on_save_completion)
    {
        bool done = true;
        if (!Multiplayer)
        {
            if (Message_box_handle_coptions != -1)
            {
                ui_hide_msgbox(Message_box_handle_coptions, !player_profile_last_save_successful(Player_controller_index != 0xFFFFFFFFu ? (int)Player_controller_index : 0));
            }
            if (ui_is_msgbox_active(Message_box_handle_coptions))
            {
                done = false;
            }
            else
            {
                Message_box_handle_coptions = -1;
            }
        }
        if (done)
        {
            Waiting_on_save_completion = false;
            if (gameseq_get_state() == GS_CONTROLLER_OPTIONS)
            {
                gameseq_pop_state();
            }
        }
    }
    if (!Assets_loaded_coptions)
    {
        switch (ui_get_doc_asset_state(Doc_handle_coptions))
        {
        case ST_LOADING:
            break;
        case ST_LOADED:
            Assets_loaded_coptions = true;
            break;
        case ST_UNLOADED:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            [[fallthrough]];
        case ST_ERROR:
            Doc_handle_coptions = -1;
            if (gameseq_get_state() == GS_CONTROLLER_OPTIONS)
            {
                gameseq_pop_state();
            }
            break;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
        if (!Assets_loaded_coptions)
        {
            ui_process((unsigned int)Control_id_coptions, false);
            return;
        }
        controller_options_create_button_image_data_item();
        controller_options_create_button_text_data_item();
        controller_options_create_tab_data_item();
        controller_options_create_header_data();
        controller_options_create_row_data();
        set_controller_page(0);
    }
    if (Utility_doc_handle == -1 || !ui_is_msgbox_active(-2))
    {
        controller_options_process_mouse();
    }
    else if (Awaiting_config_input)
    {
        control_options_config_check_input();
    }
    controller_options_process_input();
    controller_options_process_tab_data();
    controller_options_process_button_images();
    controller_options_process_button_text();
    Controller_options_process_header_data();
    controller_options_process_rows();
    if (Update_list)
    {
        Update_list = false;
    }
    if (Menu_handle_coptions != -1)
    {
        ui_add_for_processing(Doc_handle_coptions);
        ui_process((unsigned int)Control_id_coptions, false);
    }
}

// 0x00890940 (:4336)
void controller_options_handshake(rl_renderer*)
{
}

// 0x00900C70 (:4341)
void controller_options_render(rl_renderer*)
{
    if (Assets_loaded_coptions)
    {
        ui_add_for_rendering(Doc_handle_coptions);
    }
    ui_render((unsigned int)Control_id_coptions, false);
}

// 0x008F3C50 (:4350)
// The box, menu, document and ids go, the menu character back, the menu under it shown (not when going to wrecking
// crew's character select). Wrecking crew's per-player controls are written back.
void controller_options_exit(game_state new_state)
{
    if (Message_box_handle_coptions != -1)
    {
        ui_hide_msgbox(Message_box_handle_coptions, true);
        Message_box_handle_coptions = -1;
    }
    ui_menu_destroy(Menu_handle_coptions);
    // Magic number: a 0.5 s fade out.
    ui_unload_document(Doc_handle_coptions, DFOT_NONE, 0.5f);
    ui_relinquish_control_id((unsigned int)Control_id_coptions);
    Control_id_coptions = -1;
    ui_relinquish_input_id(Input_id_coptions);
    Menu_handle_coptions = -1;
    Doc_handle_coptions = -1;
    Obj_pos = On_screen_pos;
    if (!Active)
    {
        if (new_state != GS_WRECKING_CREW_CHARACTER_SELECT)
        {
            main_menu_show_menu(true);
        }
    }
    else
    {
        ingame_options_show_menu(true);
    }
    if (gameseq_state_is_active(GS_WRECKING_CREW_CHARACTER_SELECT))
    {
        // The selected player's controls stored, the main player's (backed up by controller_options_enter) restored.
        const int player = controller_options_wc_selected_player();
        Wc_player_infos[player].ctrl_config = controller_options_current_config();
        controller_options_get_control_config(Wc_player_infos[player].control_options);
        controller_options_current_config() = Control_options_main_player_ctrl_config_backup;
        controller_options_set_control_config(Control_options_main_player_control_options_backup);
        return;
    }
    // Original: outside wrecking crew (and with no current player in it) the controls are still copied into the first
    // wrecking crew player's info, in every game.
    int player = 0;
    if (wrecking_crew_is_active() && Wrecking_crew_current_player >= 0)
    {
        // Magic number: a current player past the four slots writes player 0.
        player = Wrecking_crew_current_player > 3 ? 0 : Wrecking_crew_current_player;
    }
    Wc_player_infos[player].ctrl_config = controller_options_current_config();
    controller_options_get_control_config(Wc_player_infos[player].control_options);
}

// 0x00890950 (:4422)
int controller_options_get_control_id()
{
    return Control_id_coptions;
}

// 0x00890960 (:4428)
int controller_options_get_doc_handle()
{
    return Doc_handle_coptions;
}

// 0x00890970 (:4435)
void controller_options_get_control_config(unsigned char& on_foot_buttons, unsigned char& vehicle_buttons, unsigned char& stick_config, unsigned char& tank_stick_config)
{
    on_foot_buttons = (unsigned char)On_foot_config;
    vehicle_buttons = (unsigned char)Vehicle_config;
    stick_config = (unsigned char)Stick_config;
    tank_stick_config = (unsigned char)Tank_stick_config;
}

// 0x008909B0 (:4445)
void controller_options_reset_to_default(controller_options& options)
{
    const float gyro = controller_options_sensitivity_value(kDefaultSensitivity, kGyroSensitivityRange);
    options.vibration_enabled = true;
    options.gyro_orientation_config_x.sensor_id = GYRO_ORIENTATION_Z;
    options.gyro_sensitivity_third_person_x = gyro;
    options.gyro_sensitivity_third_person_y = gyro;
    options.gyro_sensitivity_vehicle_x = gyro;
    options.gyro_sensitivity_vehicle_y = gyro;
    options.gyro_sensitivity_turret_x = gyro;
    options.gyro_sensitivity_turret_y = gyro;
    options.gyro_sensitivity_satellite_x = gyro;
    options.gyro_sensitivity_satellite_y = gyro;
    options.gyro_orientation_config_x.direction = 1.0f;
    options.gyro_orientation_config_y.sensor_id = GYRO_ORIENTATION_X;
    options.gyro_orientation_config_y.direction = -1.0f;
    options.gyro_angular_velocity_config_x.sensor_id = GYRO_ANGULAR_VELOCITY_Z;
    options.gyro_angular_velocity_config_x.direction = 1.0f;
    options.inverted_x = false;
    options.inverted_y = false;
    options.crouch_hold = true;
    options.zoom_hold = false;
    options.sensitivity = kDefaultSensitivity;
    options.mouse_sensitivity = kDefaultSensitivity;
    options.gyro_enabled_third_person = false;
    options.gyro_enabled_vehicle = false;
    options.gyro_enabled_turret = false;
    options.gyro_enabled_satellite = false;
    options.gyro_angular_velocity_config_y.sensor_id = GYRO_ANGULAR_VELOCITY_X;
    options.gyro_angular_velocity_config_y.direction = -1.0f;
    options.stick_config = 0;
    options.tank_stick_config = 0;
    options.on_foot_config = 0;
    options.vehicle_config = 0;
}

// 0x00890AA0 (:4514)
void controller_options_get_control_config(controller_options& options)
{
    options.inverted_x = controls_get_axis_action_inverted_state(CAA_CAMERA_ROTATE, Player_controller_index, true);
    options.inverted_y = controls_get_axis_action_inverted_state(CAA_CAMERA_ELEVATE, Player_controller_index, true);
    options.crouch_hold = Control_crouch_hold;
    options.zoom_hold = Control_zoom_hold;
    options.vibration_enabled = input_get_vibration();
    options.sensitivity = controller_options_sensitivity_row(Camera_first_person_look_sensitivity_pitch, kSensitivityRange);
    options.mouse_sensitivity = controller_options_sensitivity_row(Mouse_sensitivity, kSensitivityRange);
    options.stick_config = (unsigned char)Stick_config;
    options.tank_stick_config = (unsigned char)Tank_stick_config;
    options.on_foot_config = (unsigned char)On_foot_config;
    options.vehicle_config = (unsigned char)Vehicle_config;
    options.gyro_enabled_third_person = input_get_gyro_enabled(GYRO_MODE_THIRD_PERSON);
    options.gyro_enabled_vehicle = input_get_gyro_enabled(GYRO_MODE_VEHICLE);
    options.gyro_enabled_turret = input_get_gyro_enabled(GYRO_MODE_TURRET);
    options.gyro_enabled_satellite = input_get_gyro_enabled(GYRO_MODE_SATELLITE);
    options.gyro_sensitivity_third_person_x = input_get_gyro_sensitivity(GYRO_MODE_THIRD_PERSON, GYRO_CAMERA_AXIS_X);
    options.gyro_sensitivity_third_person_y = input_get_gyro_sensitivity(GYRO_MODE_THIRD_PERSON, GYRO_CAMERA_AXIS_Y);
    options.gyro_sensitivity_vehicle_x = input_get_gyro_sensitivity(GYRO_MODE_VEHICLE, GYRO_CAMERA_AXIS_X);
    options.gyro_sensitivity_vehicle_y = input_get_gyro_sensitivity(GYRO_MODE_VEHICLE, GYRO_CAMERA_AXIS_Y);
    options.gyro_sensitivity_turret_x = input_get_gyro_sensitivity(GYRO_MODE_TURRET, GYRO_CAMERA_AXIS_X);
    options.gyro_sensitivity_turret_y = input_get_gyro_sensitivity(GYRO_MODE_TURRET, GYRO_CAMERA_AXIS_Y);
    options.gyro_sensitivity_satellite_x = input_get_gyro_sensitivity(GYRO_MODE_SATELLITE, GYRO_CAMERA_AXIS_X);
    options.gyro_sensitivity_satellite_y = input_get_gyro_sensitivity(GYRO_MODE_SATELLITE, GYRO_CAMERA_AXIS_Y);
    options.gyro_orientation_config_x = input_gyro_get_orientation_config(GYRO_CAMERA_AXIS_X);
    options.gyro_orientation_config_y = input_gyro_get_orientation_config(GYRO_CAMERA_AXIS_Y);
    options.gyro_angular_velocity_config_x = input_gyro_get_angular_velocity_config(GYRO_CAMERA_AXIS_X);
    options.gyro_angular_velocity_config_y = input_gyro_get_angular_velocity_config(GYRO_CAMERA_AXIS_Y);
}

// 0x00890C80 (:4558)
void controller_options_set_control_config(const controller_options& options)
{
    Camera_first_person_look_sensitivity_pitch = controller_options_sensitivity_value(options.sensitivity, kSensitivityRange);
    Mouse_sensitivity = controller_options_sensitivity_value(options.mouse_sensitivity, kSensitivityRange);
    Camera_first_person_look_sensitivity_heading = Camera_first_person_look_sensitivity_pitch;
    controls_set_axis_action_inverted_state(CAA_CAMERA_ROTATE, options.inverted_x, 0u, true);
    controls_set_axis_action_inverted_state(CAA_DRIVE_CAMERA_ROTATE, options.inverted_x, 0u, true);
    controls_set_axis_action_inverted_state(CAA_TANK_CAMERA_ROTATE, options.inverted_x, 0u, true);
    controls_set_axis_action_inverted_state(CAA_CAMERA_ELEVATE, options.inverted_y, 0u, true);
    controls_set_axis_action_inverted_state(CAA_DRIVE_CAMERA_ELEVATE, options.inverted_y, 0u, true);
    controls_set_axis_action_inverted_state(CAA_TANK_CAMERA_ELEVATE, options.inverted_y, 0u, true);
    Control_crouch_hold = options.crouch_hold;
    Control_zoom_hold = options.zoom_hold;
    input_set_vibration(options.vibration_enabled);
    input_set_gyro_enabled(GYRO_MODE_THIRD_PERSON, options.gyro_enabled_third_person);
    input_set_gyro_enabled(GYRO_MODE_VEHICLE, options.gyro_enabled_vehicle);
    input_set_gyro_enabled(GYRO_MODE_TURRET, options.gyro_enabled_turret);
    input_set_gyro_enabled(GYRO_MODE_SATELLITE, options.gyro_enabled_satellite);
    input_set_gyro_sensitivity(GYRO_MODE_THIRD_PERSON, GYRO_CAMERA_AXIS_X, options.gyro_sensitivity_third_person_x);
    input_set_gyro_sensitivity(GYRO_MODE_THIRD_PERSON, GYRO_CAMERA_AXIS_Y, options.gyro_sensitivity_third_person_y);
    input_set_gyro_sensitivity(GYRO_MODE_VEHICLE, GYRO_CAMERA_AXIS_X, options.gyro_sensitivity_vehicle_x);
    input_set_gyro_sensitivity(GYRO_MODE_VEHICLE, GYRO_CAMERA_AXIS_Y, options.gyro_sensitivity_vehicle_y);
    input_set_gyro_sensitivity(GYRO_MODE_TURRET, GYRO_CAMERA_AXIS_X, options.gyro_sensitivity_turret_x);
    input_set_gyro_sensitivity(GYRO_MODE_TURRET, GYRO_CAMERA_AXIS_Y, options.gyro_sensitivity_turret_y);
    input_set_gyro_sensitivity(GYRO_MODE_SATELLITE, GYRO_CAMERA_AXIS_X, options.gyro_sensitivity_satellite_x);
    input_set_gyro_sensitivity(GYRO_MODE_SATELLITE, GYRO_CAMERA_AXIS_Y, options.gyro_sensitivity_satellite_y);
    input_gyro_set_orientation_config(GYRO_CAMERA_AXIS_X, options.gyro_orientation_config_x);
    input_gyro_set_orientation_config(GYRO_CAMERA_AXIS_Y, options.gyro_orientation_config_y);
    input_gyro_set_angular_velocity_config(GYRO_CAMERA_AXIS_X, options.gyro_angular_velocity_config_x);
    input_gyro_set_angular_velocity_config(GYRO_CAMERA_AXIS_Y, options.gyro_angular_velocity_config_y);
}
