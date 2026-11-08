// Original: volition/rfg/code/gamewide/game_options.h
// Implementation: volition/rfg/code/gamewide/game_options.cpp
//
// Game options: the user's options (rfg_game_options, with the PC additions in rfg_new_options),
// the cached copy per user, the display options and performance modes, and the user settings ini file.
// game_options_ini_file_read_writer<0> reads the ini file (game_options_load_ini_file_into_cache), <1> writes it
// (game_options_write_ini_file_from_cache). The ini file accessors game_options_{get,set}_ini_file_*_as_type<T> are
// function templates of game_options.cpp and are not declared.
#pragma once

#include "keen/base/language.hpp"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/vlib/code/input/input.h"

struct control_config;

namespace keen
{
    class FilePath;
}

enum game_options_performance_mode
{
    GOPM_HIGH_PERFORMANCE = 0,
    GOPM_HIGH_QUALITY = 1,
    GOPM_STABLE_PERFORMANCE = 2,
    GOPM_COUNT = 3,
};

// size 0x50
struct rfg_new_options
{
    rfg_new_options(); // 0x007C3550

    unsigned char volume_voicechat; // 0x00
    unsigned char camera_shake; // 0x01
    unsigned char gyro_enabled_third_person; // 0x02
    unsigned char gyro_enabled_vehicle; // 0x03
    unsigned char gyro_enabled_turret; // 0x04
    unsigned char gyro_enabled_satellite; // 0x05
    float gyro_sensitivity_third_person_x; // 0x08
    float gyro_sensitivity_third_person_y; // 0x0C
    float gyro_sensitivity_vehicle_x; // 0x10
    float gyro_sensitivity_vehicle_y; // 0x14
    float gyro_sensitivity_turret_x; // 0x18
    float gyro_sensitivity_turret_y; // 0x1C
    float gyro_sensitivity_satellite_x; // 0x20
    float gyro_sensitivity_satellite_y; // 0x24
    gyro_axis_config gyro_orientation_config_x; // 0x28
    gyro_axis_config gyro_orientation_config_y; // 0x30
    gyro_axis_config gyro_angular_velocity_config_x; // 0x38
    gyro_axis_config gyro_angular_velocity_config_y; // 0x40
    unsigned char hide_xinput; // 0x48
    unsigned char hide_dinput; // 0x49
    unsigned char hide_steam_controller; // 0x4A
    unsigned char vehicle_mouse_cam; // 0x4B
    unsigned char zoom_hold; // 0x4C
};

// size 0x564
struct rfg_game_options
{
    rfg_game_options(); // 0x007D0D70

    unsigned char tool_tips_enabled; // 0x00
    unsigned char subtitles_enabled; // 0x01
    unsigned char vibration; // 0x02
    unsigned char crouch_hold; // 0x03
    unsigned char audio_enabled; // 0x04
    unsigned char push_to_talk_enabled; // 0x05
    unsigned char invert_x; // 0x06
    unsigned char invert_y; // 0x07
    unsigned char aim_assist; // 0x08
    unsigned char volume_ambience; // 0x09
    unsigned char volume_music; // 0x0A
    unsigned char volume_voice_radio; // 0x0B
    unsigned char volume_sfx_interface; // 0x0C
    unsigned char volume_overall; // 0x0D
    rl_screen_info screen_info; // 0x10
    rl_level_of_detail level_of_detail; // 0xB0
    signed char preferred_text_language; // 0x144
    signed char preferred_audio_language; // 0x145
    float pitch_sensitivity; // 0x148
    float heading_sensitivity; // 0x14C
    float mouse_sensitivity; // 0x150
    unsigned char on_foot_controls; // 0x154
    unsigned char vehicle_controls; // 0x155
    unsigned char stick_controls; // 0x156
    unsigned char tank_stick_controls; // 0x157
    unsigned char gyro_enabled; // 0x158
    float gyro_sensitivity; // 0x15C
    unsigned char disable_map_rotation; // 0x160
    unsigned char camera_fov; // 0x161
    unsigned char performance_mode; // 0x162
    rfg_new_options new_options; // 0x164
    unsigned char padding[944]; // 0x1B4
};

// size (<0>) 0x1
template<int WRITE>
class game_options_ini_file_read_writer
{
    static void parse_display_settings(keen::FilePath path, rl_screen_info& info, rl_level_of_detail& lod, unsigned char& camera_fov); // <0> 0x007E76E0, <1> 0x007E7E30 (:1088)
    static void parse_controls_config(keen::FilePath path, unsigned int controller_type, control_config& config); // <0> 0x007E7BD0, <1> 0x007E82C0 (:1162)
public:
    static void read_or_write_ini_file(keen::FilePath path); // <0> 0x007EB740, <1> 0x007EC300 (:1199)

};

extern bool Game_has_loaded_options_at_least_once; // 0x0253388E

void unused_dcf_autosave_now(); // 0x007C1C80 (:120)
bool game_options_are_valid(rfg_game_options* options); // 0x007C1CA0 (:276)
void game_options_process(); // 0x007E9040 (:282)
void game_options_notify_change(); // 0x007C1CB0 (:291)
void game_options_reset(rfg_game_options* game_opt); // 0x007CDF20 (:299)
bool game_options_are_changed(); // 0x007CE140 (:413)
bool game_options_save_in_progress(); // 0x007CE870 (:603)
void game_options_load_from_cache(); // 0x007CE8A0 (:609)
void game_options_load(); // 0x007F03A0 (:877)
void game_options_update_cache(); // 0x007CF1F0 (:890)
void game_options_save(); // 0x007D9980 (:895)
bool game_options_load_display_options(rl_screen_info& info, rl_level_of_detail& lod, bool& vsync_enabled, int& camera_fov); // 0x007C1CC0 (:905)
void game_options_save_display_options(); // 0x007D99B0 (:919)
bool game_options_had_display_options(); // 0x007C1CF0 (:926)
unsigned int game_options_enumerate_performance_modes(game_options_performance_mode* out_modes, unsigned int capacity); // 0x007C1D10 (:947)
game_options_performance_mode game_options_get_performance_mode(); // 0x007C1D30 (:981)
void game_options_set_performance_mode(game_options_performance_mode mode); // 0x007C1D40 (:986)
bool game_options_get_user_settings_path(keen::FilePath& path); // 0x007C1D50 (:993)
bool game_options_create_ini_file_and_folder(keen::FilePath& path); // 0x007C1DE0 (:1008)
// template instance: void game_options_get_ini_file_bool_as_type<bool>(keen::FilePath path, const char* pSection, const char* pKey, bool& inOutTypeVar); // 0x007CA9D0 (:1021)
// template instance: void game_options_get_ini_file_bool_as_type<unsigned char>(keen::FilePath path, const char* pSection, const char* pKey, unsigned char& inOutTypeVar); // 0x007CA740 (:1021)
// template instance: void game_options_get_ini_file_int_as_type<signed char>(keen::FilePath path, const char* pSection, const char* pKey, signed char& inOutTypeVar); // 0x007CA820 (:1027)
// template instance: void game_options_get_ini_file_int_as_type<enum rl_antialias_lod>(keen::FilePath path, const char* pSection, const char* pKey, rl_antialias_lod& inOutTypeVar); // 0x007CAA40 (:1027)
// template instance: void game_options_get_ini_file_int_as_type<enum rl_shadow_lod>(keen::FilePath path, const char* pSection, const char* pKey, rl_shadow_lod& inOutTypeVar); // 0x007CAA10 (:1027)
// template instance: void game_options_get_ini_file_int_as_type<enum gyro_sensor>(keen::FilePath path, const char* pSection, const char* pKey, gyro_sensor& inOutTypeVar); // 0x007CA7F0 (:1027)
// template instance: void game_options_get_ini_file_int_as_type<unsigned char>(keen::FilePath path, const char* pSection, const char* pKey, unsigned char& inOutTypeVar); // 0x007CA780 (:1027)
// template instance: void game_options_get_ini_file_int_as_type<int>(keen::FilePath path, const char* pSection, const char* pKey, int& inOutTypeVar); // 0x007CA940 (:1027)
// template instance: void game_options_get_ini_file_int_as_type<enum rl_particle_lod>(keen::FilePath path, const char* pSection, const char* pKey, rl_particle_lod& inOutTypeVar); // 0x007CAA70 (:1027)
// template instance: void game_options_get_ini_file_int_as_type<enum rl_anisotropic_filtering_lod>(keen::FilePath path, const char* pSection, const char* pKey, rl_anisotropic_filtering_lod& inOutTypeVar); // 0x007CAAA0 (:1027)
// template instance: void game_options_get_ini_file_int_as_type<unsigned int>(keen::FilePath path, const char* pSection, const char* pKey, unsigned int& inOutTypeVar); // 0x007CA970 (:1027)
// template instance: void game_options_get_ini_file_int_as_type<enum game_options_performance_mode>(keen::FilePath path, const char* pSection, const char* pKey, game_options_performance_mode& inOutTypeVar); // 0x007CAAD0 (:1027)
// template instance: void game_options_get_ini_file_float_as_type<float>(keen::FilePath path, const char* pSection, const char* pKey, float& inOutTypeVar); // 0x007CA7B0 (:1033)
// template instance: void game_options_set_ini_file_bool_as_type<bool>(keen::FilePath path, const char* pSection, const char* pKey, bool& inOutTypeVar); // 0x007CAB90 (:1040)
// template instance: void game_options_set_ini_file_bool_as_type<unsigned char>(keen::FilePath path, const char* pSection, const char* pKey, unsigned char& inOutTypeVar); // 0x007CA850 (:1040)
// template instance: void game_options_set_ini_file_int_as_type<enum rl_particle_lod>(keen::FilePath path, const char* pSection, const char* pKey, rl_particle_lod& inOutTypeVar); // 0x007CAC20 (:1046)
// template instance: void game_options_set_ini_file_int_as_type<enum rl_shadow_lod>(keen::FilePath path, const char* pSection, const char* pKey, rl_shadow_lod& inOutTypeVar); // 0x007CABC0 (:1046)
// template instance: void game_options_set_ini_file_int_as_type<enum rl_anisotropic_filtering_lod>(keen::FilePath path, const char* pSection, const char* pKey, rl_anisotropic_filtering_lod& inOutTypeVar); // 0x007CAC50 (:1046)
// template instance: void game_options_set_ini_file_int_as_type<signed char>(keen::FilePath path, const char* pSection, const char* pKey, signed char& inOutTypeVar); // 0x007CA910 (:1046)
// template instance: void game_options_set_ini_file_int_as_type<unsigned int>(keen::FilePath path, const char* pSection, const char* pKey, unsigned int& inOutTypeVar); // 0x007CAB30 (:1046)
// template instance: void game_options_set_ini_file_int_as_type<enum rl_antialias_lod>(keen::FilePath path, const char* pSection, const char* pKey, rl_antialias_lod& inOutTypeVar); // 0x007CABF0 (:1046)
// template instance: void game_options_set_ini_file_int_as_type<unsigned char>(keen::FilePath path, const char* pSection, const char* pKey, unsigned char& inOutTypeVar); // 0x007CA880 (:1046)
// template instance: void game_options_set_ini_file_int_as_type<int>(keen::FilePath path, const char* pSection, const char* pKey, int& inOutTypeVar); // 0x007CAB00 (:1046)
// template instance: void game_options_set_ini_file_int_as_type<enum gyro_sensor>(keen::FilePath path, const char* pSection, const char* pKey, gyro_sensor& inOutTypeVar); // 0x007CA8E0 (:1046)
// template instance: void game_options_set_ini_file_int_as_type<enum game_options_performance_mode>(keen::FilePath path, const char* pSection, const char* pKey, game_options_performance_mode& inOutTypeVar); // 0x007CAC80 (:1046)
// template instance: void game_options_set_ini_file_float_as_type<float>(keen::FilePath path, const char* pSection, const char* pKey, float& inOutTypeVar); // 0x007CA8B0 (:1052)
void game_options_load_ini_file_into_cache(); // 0x007EE6A0 (:1356)
void game_options_write_ini_file_from_cache(); // 0x007EE770 (:1371)
void game_options_start_language_switch(keen::LanguageId text_lang, keen::LanguageId audio_lang); // 0x007C1E40 (:1404)
const rfg_game_options& game_options_get_last_options_from_save(); // 0x007C1EA0 (:1415)
