// Original: volition/rfg/code/gamewide/game_options.cpp
//
// The user's options: the options cache (Last_options_from_save_data), the user settings ini file
// (%APPDATA%/kaiko/rfg/config.ini, read by game_options_ini_file_read_writer<0> at boot and written by <1>), applying
// the cache to the game (game_options_load_from_cache), taking the game's current settings into it
// (game_options_write_to_cache), saving it with the save data (game_save_write_options) and the language switch.
// keen::rfg::Application::fillGameframeworkParameters loads the ini file into the cache and takes the display options
// from it; Application::initialize and rfg_init_stage_1_prepare read the display options again, and the input API
// visibility comes from the cache.
// The save system (game_save_generic.cpp) is stubbed until stub_impl.md S10: without save data game_options_load only
// applies the cache, game_options_save_in_progress answers true and nothing calls the ini writer (its only caller is
// the save system's ini thread).
//
// Notes (decompiled behaviour, kept as is):
// - read_or_write_ini_file reads and writes tank_stick_controls twice.
// - The ini file's camera_shake is the inverse of the option's value (read and written inverted).
// - parse_display_settings reads performance_mode but only accepts the modes the PC build enumerates: one,
//   GOPM_HIGH_PERFORMANCE, so the mode is always that.
// - The control configuration of setting 5 is neither read, written nor copied.
// - game_options_load_from_cache enables the satellite gyro from the options, then disables it again.
// - game_options_are_changed compares both effect volumes (4 and 5) with volume_sfx_interface and does not compare
//   the voice volume 3, aim assist or camera shake.
//
// Split TU (SFX.md 2.6, SX3): compiled twice. The rfg library builds the part the boot reads (the cache, the ini
// reader, the display options, the performance modes, the settings path); the audio library (rfg_audio,
// OPENRFG_AUDIO), which only OpenRFG links, builds the rest: it reaches the audio, the menus and the save system, and
// only the menus (rfg_ui) call it. Game_performance_mode, a file static in rfg.exe, has external linkage for it.

#ifndef OPENRFG_AUDIO

#include "volition/rfg/code/gamewide/game_options.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/interface/option_menus/option_menu_system.h"

#include "keen/base/filepath.hpp"
#include "keen/base/inifile.hpp"
#include "keen/base/nativefile.hpp"
#include "keen/base/string.hpp"

#include <string.h>

// ---- globals
rfg_game_options Last_options_from_save_data; // 0x02535858, the options cache
// The control configurations as the options last gave them (placement inferred).
control_config Control_config_from_options[1][NUM_CONTROL_CONFIG_SETTINGS]; // 0x02273E78

// File static in rfg.exe; shared with the OPENRFG_AUDIO half of the split TU.
game_options_performance_mode Game_performance_mode; // 0x02533890

// ---- file-static globals
static bool Had_display_options; // 0x0253388D

// 0x007C3550
rfg_new_options::rfg_new_options()
{
}

// 0x007D0D70
// The default options: a 1280x720 window and the default level of detail; everything else zero.
rfg_game_options::rfg_game_options()
{
}

// 0x007C1CC0 (:905)
bool game_options_load_display_options(rl_screen_info& info, rl_level_of_detail& lod, bool& vsync_enabled, int& camera_fov)
{
    display_options_load_from_cache(&Last_options_from_save_data, info, lod, vsync_enabled, camera_fov);
    Had_display_options = true;
    return true;
}

// 0x007C1CA0 (:276)
bool game_options_are_valid(rfg_game_options* options)
{
    return options != nullptr;
}

// 0x007C1CF0 (:926)
bool game_options_had_display_options()
{
    return Had_display_options;
}

// 0x007C1D00 (:932)
// LTCG: num_modes in ECX, mode in EDX, out_modes in ESI.
static void game_options_add_performance_mode(game_options_performance_mode* out_modes, game_options_performance_mode mode, unsigned int& num_modes, unsigned int capacity)
{
    if (num_modes < capacity)
    {
        out_modes[num_modes] = mode;
        num_modes++;
    }
}

// 0x007C1D10 (:947)
// The PC build has one mode.
unsigned int game_options_enumerate_performance_modes(game_options_performance_mode* out_modes, unsigned int capacity)
{
    unsigned int num_modes = 0;
    game_options_add_performance_mode(out_modes, GOPM_HIGH_PERFORMANCE, num_modes, capacity); // inlined in the binary
    return num_modes;
}

// 0x007C1D30 (:981)
game_options_performance_mode game_options_get_performance_mode()
{
    return Game_performance_mode;
}

// 0x007C1D40 (:986)
void game_options_set_performance_mode(game_options_performance_mode mode)
{
    Game_performance_mode = mode;
}

// 0x007C1D50 (:993)
// The roaming app data folder.
bool game_options_get_user_settings_path(keen::FilePath& path)
{
    char folder[0x100];
    if (!keen::getKnownWindowsFolder(folder, 0x100, keen::KnownWindowsFolderId_RoamingAppData))
    {
        return false;
    }
    path = keen::FilePath(folder, true);
    return true;
}

// 0x007C1DE0 (:1008)
// Creates the path's directories; true if they exist afterwards.
bool game_options_create_ini_file_and_folder(keen::FilePath& path)
{
    const keen::IoError error = keen::createNativeDirectoryPath(path.getCompletePath());
    return error == keen::IoError_AlreadyExists || error == keen::IoError_Ok;
}

// 0x007C1EA0 (:1415)
const rfg_game_options& game_options_get_last_options_from_save()
{
    return Last_options_from_save_data;
}

// 0x007E76E0 (:1088)
// The [graphics] section: the screen (its aspect ratio from the size), the field of view, the level of detail and the
// performance mode.
template<>
void game_options_ini_file_read_writer<0>::parse_display_settings(keen::FilePath path, rl_screen_info& info, rl_level_of_detail& lod, unsigned char& camera_fov)
{
    int rr_num;
    int rr_den;
    info.get_refresh_rate(rr_num, rr_den);
    const int width = keen::readIniFileInt(path, "graphics", "current_width", info.get_screen_width());
    const int height = keen::readIniFileInt(path, "graphics", "current_height", info.get_screen_height());
    rr_num = keen::readIniFileInt(path, "graphics", "rr_num", rr_num);
    rr_den = keen::readIniFileInt(path, "graphics", "rr_den", rr_den);
    const int monitor = keen::readIniFileInt(path, "graphics", "monitor", (int)info.get_monitor());
    const int window_mode = keen::readIniFileInt(path, "graphics", "window_mode", (int)info.get_window_mode());
    const bool vsync_enabled = keen::readIniFileBool(path, "graphics", "vsync_enabled", info.get_vsync_enabled());
    camera_fov = (unsigned char)keen::readIniFileInt(path, "graphics", "camera_fov", camera_fov);
    // rfg.exe stores the fields directly; the aspect ratio is width / height.
    info.set_screen_width(width);
    info.set_screen_height(height);
    info.set_refresh_rate(rr_num, rr_den);
    info.set_monitor((unsigned int)monitor);
    info.set_window_mode((keen::graphics::WindowMode)window_mode);
    info.set_vsync_enabled(vsync_enabled);
    lod.m_shadow_lod = (rl_shadow_lod)keen::readIniFileInt(path, "graphics", "shadow_lod", lod.m_shadow_lod);
    lod.m_aa_lod = (rl_antialias_lod)keen::readIniFileInt(path, "graphics", "aa_lod", lod.m_aa_lod);
    lod.m_particle_lod = (rl_particle_lod)keen::readIniFileInt(path, "graphics", "particle_lod", lod.m_particle_lod);
    lod.m_soft_shadows = keen::readIniFileBool(path, "graphics", "soft_shadows", lod.m_soft_shadows);
    lod.m_bloom_enabled = keen::readIniFileBool(path, "graphics", "bloom_enabled", lod.m_bloom_enabled);
    lod.m_motion_blur_enabled = keen::readIniFileBool(path, "graphics", "motion_blur_enabled", lod.m_motion_blur_enabled);
    lod.m_depth_of_field_enabled = keen::readIniFileBool(path, "graphics", "depth_of_field_enabled", lod.m_depth_of_field_enabled);
    lod.m_distortion_enabled = keen::readIniFileBool(path, "graphics", "distortion_enabled", lod.m_distortion_enabled);
    lod.m_ssao_enabled = keen::readIniFileBool(path, "graphics", "ssao_enabled", lod.m_ssao_enabled);
    lod.m_sun_shafts_enabled = keen::readIniFileBool(path, "graphics", "sun_shafts_enabled", lod.m_sun_shafts_enabled);
    lod.m_new_data.m_texture_reduction = (unsigned char)keen::readIniFileInt(path, "graphics", "texture_resolution", lod.m_new_data.m_texture_reduction);
    lod.m_new_data.m_anisotropic_filtering = (rl_anisotropic_filtering_lod)keen::readIniFileInt(path, "graphics", "anisotropic_filtering", lod.m_new_data.m_anisotropic_filtering);
    const int mode = keen::readIniFileInt(path, "graphics", "performance_mode", Game_performance_mode);
    // The modes of the PC build (game_options_enumerate_performance_modes, inlined): only GOPM_HIGH_PERFORMANCE. A
    // mode that is not in the list becomes the first one.
    const game_options_performance_mode modes[1] = {GOPM_HIGH_PERFORMANCE};
    Game_performance_mode = modes[0];
    for (unsigned int i = 0; i < 1; i++)
    {
        if (modes[i] == mode)
        {
            Game_performance_mode = modes[i];
            break;
        }
    }
}

// 0x007E7BD0 (:1162)
// The [input] bindings of one control configuration: two buttons per button action, two axes and their inversion per
// axis action, keyed by the configuration and the action ("dev_<config>_button_<action>_<n>").
template<>
void game_options_ini_file_read_writer<0>::parse_controls_config(keen::FilePath path, unsigned int controller_type, control_config& config)
{
    char key0[0x40];
    char key1[0x40];
    // Magic number: 0x6C, the button actions the ini file has.
    for (size_t i = 0; i < 0x6C; i++)
    {
        keen::formatString(key0, sizeof(key0), "dev_%zu_button_%zu_0", (size_t)controller_type, i);
        keen::formatString(key1, sizeof(key1), "dev_%zu_button_%zu_1", (size_t)controller_type, i);
        button_binding_data& binding = config.button_bindings[i];
        binding.button[0] = (unsigned int)keen::readIniFileInt(path, "input", key0, (int)binding.button[0]);
        binding.button[1] = (unsigned int)keen::readIniFileInt(path, "input", key1, (int)binding.button[1]);
    }
    char key0_inverted[0x40];
    char key1_inverted[0x40];
    // Magic number: 0x1A, the axis actions (NUM_CONTROL_AXIS_ACTIONS).
    for (size_t i = 0; i < 0x1A; i++)
    {
        keen::formatString(key0, sizeof(key0), "dev_%zu_axis_%zu_0", (size_t)controller_type, i);
        keen::formatString(key1, sizeof(key1), "dev_%zu_axis_%zu_1", (size_t)controller_type, i);
        keen::formatString(key0_inverted, sizeof(key0_inverted), "dev_%zu_axis_%zu_0_inverted", (size_t)controller_type, i);
        keen::formatString(key1_inverted, sizeof(key1_inverted), "dev_%zu_axis_%zu_1_inverted", (size_t)controller_type, i);
        axis_binding_data& binding = config.axis_bindings[i];
        binding.axis[0] = (unsigned int)keen::readIniFileInt(path, "input", key0, (int)binding.axis[0]);
        binding.axis[1] = (unsigned int)keen::readIniFileInt(path, "input", key1, (int)binding.axis[1]);
        binding.inverted[0] = keen::readIniFileBool(path, "input", key0_inverted, binding.inverted[0]);
        binding.inverted[1] = keen::readIniFileBool(path, "input", key1_inverted, binding.inverted[1]);
    }
}

// 0x007EB740 (:1199)
// The whole ini file into the options cache; each value keeps the cache's when the key is missing.
template<>
void game_options_ini_file_read_writer<0>::read_or_write_ini_file(keen::FilePath path)
{
    rfg_game_options& o = Last_options_from_save_data;
    o.tool_tips_enabled = keen::readIniFileBool(path, "game", "tool_tips_enabled", o.tool_tips_enabled != 0);
    o.subtitles_enabled = keen::readIniFileBool(path, "game", "subtitles_enabled", o.subtitles_enabled != 0);
    parse_display_settings(path, o.screen_info, o.level_of_detail, o.camera_fov);
    o.audio_enabled = keen::readIniFileBool(path, "audio", "audio_enabled", o.audio_enabled != 0);
    o.volume_ambience = (unsigned char)keen::readIniFileInt(path, "audio", "volume_ambience", o.volume_ambience);
    o.volume_music = (unsigned char)keen::readIniFileInt(path, "audio", "volume_music", o.volume_music);
    o.volume_voice_radio = (unsigned char)keen::readIniFileInt(path, "audio", "volume_voice_radio", o.volume_voice_radio);
    o.volume_sfx_interface = (unsigned char)keen::readIniFileInt(path, "audio", "volume_sfx_interface", o.volume_sfx_interface);
    o.new_options.volume_voicechat = (unsigned char)keen::readIniFileInt(path, "audio", "volume_voicechat", o.new_options.volume_voicechat);
    o.volume_overall = (unsigned char)keen::readIniFileInt(path, "audio", "volume_overall", o.volume_overall);
    o.on_foot_controls = (unsigned char)keen::readIniFileInt(path, "input", "on_foot_controls", o.on_foot_controls);
    o.vehicle_controls = (unsigned char)keen::readIniFileInt(path, "input", "vehicle_controls", o.vehicle_controls);
    o.stick_controls = (unsigned char)keen::readIniFileInt(path, "input", "stick_controls", o.stick_controls);
    o.tank_stick_controls = (unsigned char)keen::readIniFileInt(path, "input", "tank_stick_controls", o.tank_stick_controls);
    // Original: read a second time.
    o.tank_stick_controls = (unsigned char)keen::readIniFileInt(path, "input", "tank_stick_controls", o.tank_stick_controls);
    o.push_to_talk_enabled = keen::readIniFileBool(path, "input", "push_to_talk_enabled", o.push_to_talk_enabled != 0);
    o.vibration = keen::readIniFileBool(path, "input", "vibration", o.vibration != 0);
    o.crouch_hold = keen::readIniFileBool(path, "input", "crouch_hold", o.crouch_hold != 0);
    o.new_options.zoom_hold = keen::readIniFileBool(path, "input", "zoom_hold", o.new_options.zoom_hold != 0);
    o.invert_x = keen::readIniFileBool(path, "input", "invert_x", o.invert_x != 0);
    o.invert_y = keen::readIniFileBool(path, "input", "invert_y", o.invert_y != 0);
    o.aim_assist = keen::readIniFileBool(path, "input", "aim_assist", o.aim_assist != 0);
    o.pitch_sensitivity = keen::readIniFileFloat(path, "input", "pitch_sensitivity", o.pitch_sensitivity);
    o.heading_sensitivity = keen::readIniFileFloat(path, "input", "heading_sensitivity", o.heading_sensitivity);
    o.mouse_sensitivity = keen::readIniFileFloat(path, "input", "mouse_sensitivity", o.mouse_sensitivity);
    rfg_new_options& n = o.new_options;
    n.gyro_enabled_third_person = keen::readIniFileBool(path, "input", "gyro_enabled_third_person", n.gyro_enabled_third_person != 0);
    n.gyro_enabled_vehicle = keen::readIniFileBool(path, "input", "gyro_enabled_vehicle", n.gyro_enabled_vehicle != 0);
    n.gyro_enabled_turret = keen::readIniFileBool(path, "input", "gyro_enabled_turret", n.gyro_enabled_turret != 0);
    n.gyro_enabled_satellite = keen::readIniFileBool(path, "input", "gyro_enabled_satellite", n.gyro_enabled_satellite != 0);
    n.gyro_sensitivity_third_person_x = keen::readIniFileFloat(path, "input", "gyro_sensitivity_third_person_x", n.gyro_sensitivity_third_person_x);
    n.gyro_sensitivity_third_person_y = keen::readIniFileFloat(path, "input", "gyro_sensitivity_third_person_y", n.gyro_sensitivity_third_person_y);
    n.gyro_sensitivity_vehicle_x = keen::readIniFileFloat(path, "input", "gyro_sensitivity_vehicle_x", n.gyro_sensitivity_vehicle_x);
    n.gyro_sensitivity_vehicle_y = keen::readIniFileFloat(path, "input", "gyro_sensitivity_vehicle_y", n.gyro_sensitivity_vehicle_y);
    n.gyro_sensitivity_turret_x = keen::readIniFileFloat(path, "input", "gyro_sensitivity_turret_x", n.gyro_sensitivity_turret_x);
    n.gyro_sensitivity_turret_y = keen::readIniFileFloat(path, "input", "gyro_sensitivity_turret_y", n.gyro_sensitivity_turret_y);
    n.gyro_sensitivity_satellite_x = keen::readIniFileFloat(path, "input", "gyro_sensitivity_satellite_x", n.gyro_sensitivity_satellite_x);
    n.gyro_sensitivity_satellite_y = keen::readIniFileFloat(path, "input", "gyro_sensitivity_satellite_y", n.gyro_sensitivity_satellite_y);
    n.gyro_orientation_config_x.sensor_id = (gyro_sensor)keen::readIniFileInt(path, "input", "gyro_orientation_config_x_sensor", n.gyro_orientation_config_x.sensor_id);
    n.gyro_orientation_config_x.direction = keen::readIniFileFloat(path, "input", "gyro_orientation_config_x_dir", n.gyro_orientation_config_x.direction);
    n.gyro_orientation_config_y.sensor_id = (gyro_sensor)keen::readIniFileInt(path, "input", "gyro_orientation_config_y_sensor", n.gyro_orientation_config_y.sensor_id);
    n.gyro_orientation_config_y.direction = keen::readIniFileFloat(path, "input", "gyro_orientation_config_y_dir", n.gyro_orientation_config_y.direction);
    n.gyro_angular_velocity_config_x.sensor_id = (gyro_sensor)keen::readIniFileInt(path, "input", "gyro_angular_velocity_config_x_sensor", n.gyro_angular_velocity_config_x.sensor_id);
    n.gyro_angular_velocity_config_x.direction = keen::readIniFileFloat(path, "input", "gyro_angular_velocity_config_x_dir", n.gyro_angular_velocity_config_x.direction);
    n.gyro_angular_velocity_config_y.sensor_id = (gyro_sensor)keen::readIniFileInt(path, "input", "gyro_angular_velocity_config_y_sensor", n.gyro_angular_velocity_config_y.sensor_id);
    n.gyro_angular_velocity_config_y.direction = keen::readIniFileFloat(path, "input", "gyro_angular_velocity_config_y_dir", n.gyro_angular_velocity_config_y.direction);
    n.hide_xinput = keen::readIniFileBool(path, "input", "hide_xinput", n.hide_xinput != 0);
    n.hide_dinput = keen::readIniFileBool(path, "input", "hide_dinput", n.hide_dinput != 0);
    n.hide_steam_controller = keen::readIniFileBool(path, "input", "hide_steam_controller", n.hide_steam_controller != 0);
    n.vehicle_mouse_cam = (unsigned char)keen::readIniFileInt(path, "input", "vehicle_mouse_cam", n.vehicle_mouse_cam);
    o.disable_map_rotation = keen::readIniFileBool(path, "game", "disable_map_rotation", o.disable_map_rotation != 0);
    // Original: the ini file has the inverse of camera_shake.
    n.camera_shake = n.camera_shake == 0;
    n.camera_shake = !keen::readIniFileBool(path, "game", "camera_shake", n.camera_shake != 0);
    o.preferred_text_language = (signed char)keen::readIniFileInt(path, "game", "preferred_text_base", o.preferred_text_language);
    o.preferred_audio_language = (signed char)keen::readIniFileInt(path, "game", "preferred_audio_base", o.preferred_audio_language);
    for (unsigned int setting = 0; setting < NUM_CONTROL_CONFIG_SETTINGS; setting++)
    {
        // Magic number: setting 5 is not in the ini file.
        if (setting != 5)
        {
            Control_config_from_options[0][setting] = Control_config[0][setting];
            parse_controls_config(path, setting, Control_config_from_options[0][setting]);
        }
    }
}

// 0x007EE6A0 (:1356)
// The user settings ini file: <roaming app data>/kaiko/rfg/config.ini.
void game_options_load_ini_file_into_cache()
{
    keen::FilePath path;
    char folder[0x104];
    if (keen::getKnownWindowsFolder(folder, 0x100, keen::KnownWindowsFolderId_RoamingAppData))
    {
        path = keen::FilePath(folder, true);
        path.pushDirectory("kaiko");
        path.pushDirectory("rfg");
        path.setFileNameWithExtension("config.ini");
        game_options_ini_file_read_writer<0>::read_or_write_ini_file(path);
    }
}

#else // OPENRFG_AUDIO

#include "volition/rfg/code/gamewide/game_options.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/camera/camera_first_person.h"
#include "volition/rfg/code/camera/camera_free.h"
#include "volition/rfg/code/camera/camera_shake.h"
#include "volition/rfg/code/camera/camera_view.h"
#include "volition/rfg/code/control/control_gameplay.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/game_save.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/gamewide/rfgsaveinterface.hpp"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/option_menus/controller_options.h"
#include "volition/rfg/code/interface/option_menus/option_menu_system.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_core/include/multi_voice.h"
#include "volition/rfg/code/objects/humans/player_autoaim.h"
#include "volition/rfg/code/objects/humans/player_cover.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/os/scheduler.h"
#include "volition/vlib/code/os/thread.h"

#include "keen/base/filepath.hpp"
#include "keen/base/inifile.hpp"
#include "keen/base/nativefile.hpp"
#include "keen/base/string.hpp"
#include "keen/input/inputsystem.hpp"

#include <string.h>

static_assert(sizeof(keen::rfg::RfgSaveData) == 0x1E0589C, "RfgSaveData"); // RfgSaveInterface::getSaveDataSize

// The rfg library's half.
extern rfg_game_options Last_options_from_save_data; // 0x02535858
extern control_config Control_config_from_options[1][NUM_CONTROL_CONFIG_SETTINGS]; // 0x02273E78
extern game_options_performance_mode Game_performance_mode; // 0x02533890

// ---- globals
bool Game_has_loaded_options_at_least_once; // 0x0253388E

// ---- file-static globals
static bool Save_options; // 0x0253388C, set by game_options_notify_change, saved by game_options_process

// Magic number: the language of an option that follows the system's (-1 in rfg_game_options).
static const keen::LanguageId System_language = (keen::LanguageId)-1;

// The sensitivity comparison of game_options_are_changed (inlined): closer than 0.05 either way.
static bool game_options_float_equal(float a, float b)
{
    return a - b < 0.05f && -(a - b) < 0.05f;
}

// 0x007C1C80 (:120)
void unused_dcf_autosave_now()
{
    World.save_and_continue(nullptr, false, true);
}

// 0x007CDA60 (:135)
// LTCG: user_index in EAX (the callers pass the player's controller, 0 when there is none).
// The game's current settings into the options: inversion, sensitivities, hold modes, vibration, gyro, map rotation,
// field of view, performance mode, tool tips, the controller APIs, the vehicle camera, the volumes (when the audio
// system is up; 0-10 each, the voice chat attenuation inverted), subtitles, languages (-1 when the system's),
// push to talk, the control configurations, aim assist and camera shake (inverted).
static void game_options_write_to_cache(rfg_game_options* pstore, unsigned int user_index)
{
    if (pstore == nullptr)
    {
        return;
    }
    pstore->invert_x = controls_get_axis_action_inverted_state(CAA_CAMERA_ROTATE, user_index, true);
    pstore->invert_y = controls_get_axis_action_inverted_state(CAA_CAMERA_ELEVATE, user_index, true);
    pstore->heading_sensitivity = Camera_first_person_look_sensitivity_heading;
    pstore->pitch_sensitivity = Camera_first_person_look_sensitivity_pitch;
    pstore->crouch_hold = Control_crouch_hold;
    pstore->new_options.zoom_hold = Control_zoom_hold;
    pstore->vibration = input_get_vibration();
    pstore->gyro_enabled = 0;
    pstore->gyro_sensitivity = 0.0f;
    rfg_new_options& n = pstore->new_options;
    n.gyro_enabled_third_person = input_get_gyro_enabled(GYRO_MODE_THIRD_PERSON);
    n.gyro_enabled_vehicle = input_get_gyro_enabled(GYRO_MODE_VEHICLE);
    n.gyro_enabled_turret = input_get_gyro_enabled(GYRO_MODE_TURRET);
    n.gyro_enabled_satellite = input_get_gyro_enabled(GYRO_MODE_SATELLITE);
    n.gyro_sensitivity_third_person_x = input_get_gyro_sensitivity(GYRO_MODE_THIRD_PERSON, GYRO_CAMERA_AXIS_X);
    n.gyro_sensitivity_third_person_y = input_get_gyro_sensitivity(GYRO_MODE_THIRD_PERSON, GYRO_CAMERA_AXIS_Y);
    n.gyro_sensitivity_vehicle_x = input_get_gyro_sensitivity(GYRO_MODE_VEHICLE, GYRO_CAMERA_AXIS_X);
    n.gyro_sensitivity_vehicle_y = input_get_gyro_sensitivity(GYRO_MODE_VEHICLE, GYRO_CAMERA_AXIS_Y);
    n.gyro_sensitivity_turret_x = input_get_gyro_sensitivity(GYRO_MODE_TURRET, GYRO_CAMERA_AXIS_X);
    n.gyro_sensitivity_turret_y = input_get_gyro_sensitivity(GYRO_MODE_TURRET, GYRO_CAMERA_AXIS_Y);
    n.gyro_sensitivity_satellite_x = input_get_gyro_sensitivity(GYRO_MODE_SATELLITE, GYRO_CAMERA_AXIS_X);
    n.gyro_sensitivity_satellite_y = input_get_gyro_sensitivity(GYRO_MODE_SATELLITE, GYRO_CAMERA_AXIS_Y);
    n.gyro_orientation_config_x = input_gyro_get_orientation_config(GYRO_CAMERA_AXIS_X);
    n.gyro_orientation_config_y = input_gyro_get_orientation_config(GYRO_CAMERA_AXIS_Y);
    n.gyro_angular_velocity_config_x = input_gyro_get_angular_velocity_config(GYRO_CAMERA_AXIS_X);
    n.gyro_angular_velocity_config_y = input_gyro_get_angular_velocity_config(GYRO_CAMERA_AXIS_Y);
    pstore->disable_map_rotation = Minimap_rot0;
    pstore->camera_fov = (unsigned char)camera_view_get_fov_raw();
    pstore->performance_mode = (unsigned char)Game_performance_mode;
    pstore->tool_tips_enabled = Tool_tips_enabled;
    n.hide_xinput = keen::input::getControllerApiHidden(Keen_input_system, keen::WindowsControllerApi_XInput);
    n.hide_dinput = keen::input::getControllerApiHidden(Keen_input_system, keen::WindowsControllerApi_DInput);
    n.hide_steam_controller = keen::input::getControllerApiHidden(Keen_input_system, keen::WindowsControllerApi_SteamController);
    n.vehicle_mouse_cam = (unsigned char)Camera_free_mouse_control_mode;
    float volume;
    if (audiolib_system_get_volume_scalar(0, &volume) != AUDIOLIB_ERR_NOT_INITED)
    {
        pstore->audio_enabled = audiolib_get_sound_enabled();
        // Magic numbers: the user volume ids (0 ambience, 1 music, 2 voice and radio, 4 effects and interface,
        // 6 overall, 7 voice chat) and the 10 steps.
        audiolib_system_get_volume_scalar(0, &volume);
        pstore->volume_ambience = (unsigned char)fl_round(volume * 10.0f);
        audiolib_system_get_volume_scalar(1, &volume);
        pstore->volume_music = (unsigned char)fl_round(volume * 10.0f);
        audiolib_system_get_volume_scalar(2, &volume);
        pstore->volume_voice_radio = (unsigned char)fl_round(volume * 10.0f);
        audiolib_system_get_volume_scalar(7, &volume);
        volume = 1.0f - volume;
        if (volume < 0.0f)
        {
            volume = 0.0f;
        }
        if (volume >= 1.0f)
        {
            volume = 1.0f;
        }
        n.volume_voicechat = (unsigned char)fl_round(volume * 10.0f);
        audiolib_system_get_volume_scalar(4, &volume);
        pstore->volume_sfx_interface = (unsigned char)fl_round(volume * 10.0f);
        audiolib_system_get_volume_scalar(6, &volume);
        pstore->volume_overall = (unsigned char)fl_round(volume * 10.0f);
    }
    pstore->subtitles_enabled = Subtitles_enabled;
    pstore->preferred_text_language = Preferred_text_language == localization_query_system_language() ? -1 : (signed char)Preferred_text_language;
    pstore->preferred_audio_language = Preferred_audio_language == localization_query_system_language() ? -1 : (signed char)Preferred_audio_language;
    pstore->mouse_sensitivity = Mouse_sensitivity;
    pstore->push_to_talk_enabled = multi_voice_push_to_talk_enabled();
    for (unsigned int setting = 0; setting < NUM_CONTROL_CONFIG_SETTINGS; setting++)
    {
        // Magic number: setting 5 is not copied (as not in the ini file).
        if (setting != 5)
        {
            Control_config_from_options[0][setting] = Control_config[0][setting];
        }
    }
    pstore->aim_assist = player_get_autoaim_enabled();
    n.camera_shake = !Camera_shake_enabled;
}

// 0x007E9040 (:282)
// A change notified since the last frame is taken into the cache and saved.
void game_options_process()
{
    if (Save_options)
    {
        Save_options = false;
        game_options_write_to_cache(&Last_options_from_save_data, Player_controller_index != 0xFFFFFFFFu ? Player_controller_index : 0);
        game_save_write_options();
    }
}

// 0x007C1CB0 (:291)
void game_options_notify_change()
{
    Save_options = true;
}

// 0x007CDF20 (:299)
// The default options; the control configurations become the current ones, and aim assist is switched on.
void game_options_reset(rfg_game_options* game_opt)
{
    game_opt->tool_tips_enabled = 1;
    game_opt->vibration = input_get_vibration();
    game_opt->new_options.zoom_hold = 0;
    game_opt->crouch_hold = 1;
    game_opt->audio_enabled = 1;
    rfg_new_options& n = game_opt->new_options;
    if (Keen_input_system == nullptr)
    {
        n.hide_xinput = 0;
        n.hide_dinput = 0;
        n.hide_steam_controller = 0;
    }
    else
    {
        n.hide_xinput = keen::input::getControllerApiHidden(Keen_input_system, keen::WindowsControllerApi_XInput);
        n.hide_dinput = keen::input::getControllerApiHidden(Keen_input_system, keen::WindowsControllerApi_DInput);
        n.hide_steam_controller = keen::input::getControllerApiHidden(Keen_input_system, keen::WindowsControllerApi_SteamController);
    }
    // Magic number: 0.611111f (0x3F1C71C7, 11/18), the default look and mouse sensitivity.
    const float sensitivity = 0.611111104f;
    game_opt->heading_sensitivity = sensitivity;
    game_opt->pitch_sensitivity = sensitivity;
    n.vehicle_mouse_cam = 0;
    // Magic number: 10, the full volume (0 attenuation for the voice chat).
    game_opt->volume_ambience = 10;
    game_opt->volume_music = 10;
    game_opt->volume_voice_radio = 10;
    game_opt->volume_sfx_interface = 10;
    n.volume_voicechat = 0;
    game_opt->volume_overall = 10;
    game_opt->gyro_enabled = 0;
    game_opt->gyro_sensitivity = 0.0f;
    n.gyro_enabled_third_person = input_get_gyro_enabled(GYRO_MODE_THIRD_PERSON);
    n.gyro_enabled_vehicle = input_get_gyro_enabled(GYRO_MODE_VEHICLE);
    n.gyro_enabled_turret = input_get_gyro_enabled(GYRO_MODE_TURRET);
    // Original: the satellite gyro is queried, then reset to off.
    input_get_gyro_enabled(GYRO_MODE_SATELLITE);
    // Magic number: 0.722222f (0x3F38E38E, 13/18), the default gyro sensitivity.
    const float gyro_sensitivity = 0.722222209f;
    n.gyro_sensitivity_third_person_x = gyro_sensitivity;
    n.gyro_sensitivity_third_person_y = gyro_sensitivity;
    n.gyro_sensitivity_vehicle_x = gyro_sensitivity;
    n.gyro_sensitivity_vehicle_y = gyro_sensitivity;
    n.gyro_sensitivity_turret_x = gyro_sensitivity;
    n.gyro_sensitivity_turret_y = gyro_sensitivity;
    n.gyro_sensitivity_satellite_x = gyro_sensitivity;
    n.gyro_sensitivity_satellite_y = gyro_sensitivity;
    n.gyro_orientation_config_x.sensor_id = GYRO_ORIENTATION_Z;
    n.gyro_orientation_config_y.sensor_id = GYRO_ORIENTATION_X;
    n.gyro_orientation_config_y.direction = -1.0f;
    n.gyro_orientation_config_x.direction = 1.0f;
    n.gyro_angular_velocity_config_x.sensor_id = GYRO_ANGULAR_VELOCITY_Z;
    n.gyro_enabled_satellite = 0;
    n.gyro_angular_velocity_config_x.direction = 1.0f;
    n.gyro_angular_velocity_config_y.sensor_id = GYRO_ANGULAR_VELOCITY_X;
    n.gyro_angular_velocity_config_y.direction = -1.0f;
    game_opt->disable_map_rotation = 0;
    // Magic number: 45, the default field of view.
    game_opt->camera_fov = 45;
    game_opt->performance_mode = GOPM_HIGH_PERFORMANCE;
    game_opt->subtitles_enabled = 1;
    game_opt->preferred_text_language = -1;
    game_opt->preferred_audio_language = -1;
    game_opt->mouse_sensitivity = sensitivity;
    game_opt->push_to_talk_enabled = 1;
    controls_set_custom_from_current();
    game_opt->aim_assist = 1;
    player_set_autoaim_enabled(true);
    n.camera_shake = 0;
    display_options_write_to_cache(game_opt);
}

// 0x007CE140 (:413)
// Whether the game's current settings differ from the cache (the options menus ask before saving).
bool game_options_are_changed()
{
    const rfg_game_options& o = Last_options_from_save_data;
    const rfg_new_options& n = o.new_options;
    if (audiolib_get_sound_enabled() != (o.audio_enabled != 0))
    {
        return true;
    }
    float volume;
    audiolib_system_get_volume_scalar(0, &volume);
    if ((unsigned int)fl_round(volume * 10.0f) != o.volume_ambience)
    {
        return true;
    }
    audiolib_system_get_volume_scalar(1, &volume);
    if ((unsigned int)fl_round(volume * 10.0f) != o.volume_music)
    {
        return true;
    }
    audiolib_system_get_volume_scalar(2, &volume);
    if ((unsigned int)fl_round(volume * 10.0f) != o.volume_voice_radio)
    {
        return true;
    }
    audiolib_system_get_volume_scalar(7, &volume);
    volume = 1.0f - volume;
    if (volume < 0.0f)
    {
        volume = 0.0f;
    }
    if (volume >= 1.0f)
    {
        volume = 1.0f;
    }
    if ((unsigned int)fl_round(volume * 10.0f) != n.volume_voicechat)
    {
        return true;
    }
    audiolib_system_get_volume_scalar(4, &volume);
    if ((unsigned int)fl_round(volume * 10.0f) != o.volume_sfx_interface)
    {
        return true;
    }
    // Original: the interface volume (5) is compared with the effects volume too.
    audiolib_system_get_volume_scalar(5, &volume);
    if ((unsigned int)fl_round(volume * 10.0f) != o.volume_sfx_interface)
    {
        return true;
    }
    audiolib_system_get_volume_scalar(6, &volume);
    if ((unsigned int)fl_round(volume * 10.0f) != o.volume_overall)
    {
        return true;
    }
    controller_options options;
    controller_options_get_control_config(options);
    if (!game_options_float_equal(Camera_first_person_look_sensitivity_pitch, o.pitch_sensitivity)
        || !game_options_float_equal(Camera_first_person_look_sensitivity_heading, o.heading_sensitivity)
        || !game_options_float_equal(Mouse_sensitivity, o.mouse_sensitivity)
        || options.gyro_enabled_third_person != (n.gyro_enabled_third_person != 0)
        || options.gyro_enabled_vehicle != (n.gyro_enabled_vehicle != 0)
        || options.gyro_enabled_turret != (n.gyro_enabled_turret != 0)
        || options.gyro_enabled_satellite != (n.gyro_enabled_satellite != 0)
        || !game_options_float_equal(options.gyro_sensitivity_third_person_x, n.gyro_sensitivity_third_person_x)
        || !game_options_float_equal(options.gyro_sensitivity_third_person_y, n.gyro_sensitivity_third_person_y)
        || !game_options_float_equal(options.gyro_sensitivity_vehicle_x, n.gyro_sensitivity_vehicle_x)
        || !game_options_float_equal(options.gyro_sensitivity_vehicle_y, n.gyro_sensitivity_vehicle_y)
        || !game_options_float_equal(options.gyro_sensitivity_turret_x, n.gyro_sensitivity_turret_x)
        || !game_options_float_equal(options.gyro_sensitivity_turret_y, n.gyro_sensitivity_turret_y)
        || !game_options_float_equal(options.gyro_sensitivity_satellite_x, n.gyro_sensitivity_satellite_x)
        || !game_options_float_equal(options.gyro_sensitivity_satellite_y, n.gyro_sensitivity_satellite_y))
    {
        return true;
    }
    if (!options.gyro_orientation_config_x.is_equal(n.gyro_orientation_config_x)
        || !options.gyro_orientation_config_y.is_equal(n.gyro_orientation_config_y)
        || !options.gyro_angular_velocity_config_x.is_equal(n.gyro_angular_velocity_config_x)
        || !options.gyro_angular_velocity_config_y.is_equal(n.gyro_angular_velocity_config_y))
    {
        return true;
    }
    if (options.crouch_hold != (o.crouch_hold != 0) || options.zoom_hold != (n.zoom_hold != 0)
        || options.inverted_x != (o.invert_x != 0) || options.inverted_y != (o.invert_y != 0)
        || options.vibration_enabled != (o.vibration != 0) || Minimap_rot0 != (o.disable_map_rotation != 0))
    {
        return true;
    }
    if ((unsigned char)camera_view_get_fov_raw() != o.camera_fov || o.performance_mode != Game_performance_mode
        || Tool_tips_enabled != (o.tool_tips_enabled != 0))
    {
        return true;
    }
    if (multi_voice_push_to_talk_enabled() != (o.push_to_talk_enabled != 0))
    {
        return true;
    }
    if ((n.hide_xinput != 0) != keen::input::getControllerApiHidden(Keen_input_system, keen::WindowsControllerApi_XInput)
        || (n.hide_dinput != 0) != keen::input::getControllerApiHidden(Keen_input_system, keen::WindowsControllerApi_DInput)
        || (n.hide_steam_controller != 0) != keen::input::getControllerApiHidden(Keen_input_system, keen::WindowsControllerApi_SteamController))
    {
        return true;
    }
    return n.vehicle_mouse_cam != (unsigned char)Camera_free_mouse_control_mode || Subtitles_enabled != (o.subtitles_enabled != 0)
        || (signed char)Preferred_text_language != o.preferred_text_language || (signed char)Preferred_audio_language != o.preferred_audio_language;
}

// 0x007CE870 (:603)
// No save data yet counts as a save in progress.
bool game_options_save_in_progress()
{
    return Save_system_save_data == nullptr || Save_system_ini_reader_is_active;
}

// 0x007C1E10 (:1393)
// The language switch on a sim thread: the localization tables and the selected languages, with the file open lock
// released meanwhile. arg1 is the text language, arg2 the audio language.
static void game_options_language_switch_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2)
{
    const bool lock = cf_is_locked();
    cf_open_lock(false);
    localization_set_and_save_selected_language((keen::LanguageId)arg1.int_val, (keen::LanguageId)arg2.int_val);
    cf_open_lock(lock);
}

// 0x007CE8A0 (:609)
// The options cache applied to the game: sound and the user volumes (0-10 each, as volume scalars 0.0-1.0; the voice
// chat attenuation is the inverse), hold modes, look and mouse sensitivity (the mouse's from the pitch when nearly 0),
// vibration, gyro (the old single switch and sensitivity count when the new ones are not set; directions are snapped to
// +-1, a zero direction resets the axis to its default), map rotation, field of view, the performance mode, the
// sensitivities clamped to [0.1, 1], tool tips, subtitles, push to talk, the controller APIs, the vehicle camera, the
// languages (a switch job when they differ from the current ones), the control configurations, inversion, aim assist
// and camera shake.
void game_options_load_from_cache()
{
    rfg_game_options& o = Last_options_from_save_data;
    rfg_new_options& n = o.new_options;
    audiolib_set_sound_enabled(o.audio_enabled != 0);
    // Magic numbers: the user volume ids (0 ambience, 1 music, 2/3 voice and radio, 4/5 effects and interface,
    // 6 overall, 7 voice chat) and the 0.1 step.
    audiolib_system_set_volume_scalar(0, (float)fl_round((float)o.volume_ambience) * 0.1f);
    audiolib_system_set_volume_scalar(1, (float)fl_round((float)o.volume_music) * 0.1f);
    const float voice = (float)fl_round((float)o.volume_voice_radio) * 0.1f;
    audiolib_system_set_volume_scalar(2, voice);
    audiolib_system_set_volume_scalar(3, voice);
    float voicechat = 1.0f - (float)fl_round((float)n.volume_voicechat) * 0.1f;
    if (voicechat < 0.0f)
    {
        voicechat = 0.0f;
    }
    if (voicechat >= 1.0f)
    {
        voicechat = 1.0f;
    }
    audiolib_system_set_volume_scalar(7, voicechat);
    const float sfx = (float)fl_round((float)o.volume_sfx_interface) * 0.1f;
    audiolib_system_set_volume_scalar(4, sfx);
    audiolib_system_set_volume_scalar(5, sfx);
    audiolib_system_set_volume_scalar(6, (float)fl_round((float)o.volume_overall) * 0.1f);
    Control_crouch_hold = o.crouch_hold != 0;
    Camera_first_person_look_sensitivity_heading = o.heading_sensitivity;
    Control_zoom_hold = n.zoom_hold != 0;
    Camera_first_person_look_sensitivity_pitch = o.pitch_sensitivity;
    Mouse_sensitivity = o.mouse_sensitivity;
    // Magic number: 0.001, below it the mouse takes the pitch sensitivity (options from before the mouse setting).
    if (o.mouse_sensitivity < 0.001f)
    {
        Mouse_sensitivity = o.pitch_sensitivity;
    }
    input_set_vibration(o.vibration != 0);
    input_set_gyro_enabled(GYRO_MODE_THIRD_PERSON, o.gyro_enabled != 0 || n.gyro_enabled_third_person != 0);
    input_set_gyro_enabled(GYRO_MODE_VEHICLE, o.gyro_enabled != 0 || n.gyro_enabled_vehicle != 0);
    input_set_gyro_enabled(GYRO_MODE_TURRET, o.gyro_enabled != 0 || n.gyro_enabled_turret != 0);
    input_set_gyro_enabled(GYRO_MODE_SATELLITE, o.gyro_enabled != 0 || n.gyro_enabled_satellite != 0);
    // Original: the satellite gyro is switched off right after.
    input_set_gyro_enabled(GYRO_MODE_SATELLITE, false);
    input_set_gyro_sensitivity(GYRO_MODE_THIRD_PERSON, GYRO_CAMERA_AXIS_X, n.gyro_sensitivity_third_person_x == 0.0f ? o.gyro_sensitivity : n.gyro_sensitivity_third_person_x);
    input_set_gyro_sensitivity(GYRO_MODE_THIRD_PERSON, GYRO_CAMERA_AXIS_Y, n.gyro_sensitivity_third_person_y == 0.0f ? o.gyro_sensitivity : n.gyro_sensitivity_third_person_y);
    input_set_gyro_sensitivity(GYRO_MODE_VEHICLE, GYRO_CAMERA_AXIS_X, n.gyro_sensitivity_vehicle_x == 0.0f ? o.gyro_sensitivity : n.gyro_sensitivity_vehicle_x);
    input_set_gyro_sensitivity(GYRO_MODE_VEHICLE, GYRO_CAMERA_AXIS_Y, n.gyro_sensitivity_vehicle_y == 0.0f ? o.gyro_sensitivity : n.gyro_sensitivity_vehicle_y);
    input_set_gyro_sensitivity(GYRO_MODE_TURRET, GYRO_CAMERA_AXIS_X, n.gyro_sensitivity_turret_x == 0.0f ? o.gyro_sensitivity : n.gyro_sensitivity_turret_x);
    input_set_gyro_sensitivity(GYRO_MODE_TURRET, GYRO_CAMERA_AXIS_Y, n.gyro_sensitivity_turret_y == 0.0f ? o.gyro_sensitivity : n.gyro_sensitivity_turret_y);
    input_set_gyro_sensitivity(GYRO_MODE_SATELLITE, GYRO_CAMERA_AXIS_X, n.gyro_sensitivity_satellite_x == 0.0f ? o.gyro_sensitivity : n.gyro_sensitivity_satellite_x);
    input_set_gyro_sensitivity(GYRO_MODE_SATELLITE, GYRO_CAMERA_AXIS_Y, n.gyro_sensitivity_satellite_y == 0.0f ? o.gyro_sensitivity : n.gyro_sensitivity_satellite_y);
    // The axis directions snapped to +-1; a zero direction (options without the setting) takes the axis's default
    // (reset_options' values).
    gyro_axis_config* const axes[4] = {&n.gyro_orientation_config_x, &n.gyro_orientation_config_y, &n.gyro_angular_velocity_config_x, &n.gyro_angular_velocity_config_y};
    const gyro_axis_config defaults[4] = {gyro_axis_config(GYRO_ORIENTATION_Z, 1.0f), gyro_axis_config(GYRO_ORIENTATION_X, -1.0f), gyro_axis_config(GYRO_ANGULAR_VELOCITY_Z, 1.0f), gyro_axis_config(GYRO_ANGULAR_VELOCITY_X, -1.0f)};
    for (int i = 0; i < 4; i++)
    {
        if (!(axes[i]->direction >= 0.0f))
        {
            axes[i]->direction = -1.0f;
        }
        else if (axes[i]->direction <= 0.0f)
        {
            *axes[i] = defaults[i];
        }
        else
        {
            axes[i]->direction = 1.0f;
        }
    }
    input_gyro_set_orientation_config(GYRO_CAMERA_AXIS_X, n.gyro_orientation_config_x);
    input_gyro_set_orientation_config(GYRO_CAMERA_AXIS_Y, n.gyro_orientation_config_y);
    input_gyro_set_angular_velocity_config(GYRO_CAMERA_AXIS_X, n.gyro_angular_velocity_config_x);
    input_gyro_set_angular_velocity_config(GYRO_CAMERA_AXIS_Y, n.gyro_angular_velocity_config_y);
    Minimap_rot0 = o.disable_map_rotation != 0;
    camera_view_set_custom_fov((int)o.camera_fov, true);
    // The PC build's only mode (the cache's mode is not read).
    Game_performance_mode = GOPM_HIGH_PERFORMANCE;
    // Magic numbers: the look sensitivities are kept in [0.1, 1].
    if (Camera_first_person_look_sensitivity_pitch < 0.1f)
    {
        Camera_first_person_look_sensitivity_pitch = 0.1f;
    }
    else if (Camera_first_person_look_sensitivity_pitch > 1.0f)
    {
        Camera_first_person_look_sensitivity_pitch = 1.0f;
    }
    if (Camera_first_person_look_sensitivity_heading < 0.1f)
    {
        Camera_first_person_look_sensitivity_heading = 0.1f;
    }
    else if (Camera_first_person_look_sensitivity_heading > 1.0f)
    {
        Camera_first_person_look_sensitivity_heading = 1.0f;
    }
    Tool_tips_enabled = o.tool_tips_enabled != 0;
    Player_disable_cover = false;
    Subtitles_enabled = o.subtitles_enabled != 0;
    multi_voice_push_to_talk_set_enabled(o.push_to_talk_enabled != 0);
    keen::input::setControllerApiHidden(Keen_input_system, keen::WindowsControllerApi_XInput, n.hide_xinput != 0);
    keen::input::setControllerApiHidden(Keen_input_system, keen::WindowsControllerApi_DInput, n.hide_dinput != 0);
    keen::input::setControllerApiHidden(Keen_input_system, keen::WindowsControllerApi_SteamController, n.hide_steam_controller != 0);
    Camera_free_mouse_control_mode = (vehicle_camera_control_mode)n.vehicle_mouse_cam;
    Preferred_text_language = (keen::LanguageId)o.preferred_text_language;
    Preferred_audio_language = (keen::LanguageId)o.preferred_audio_language;
    // A language that is the current one, or the system's (-1) while the current one is the system's, needs no switch.
    const bool text_current = Preferred_text_language == localization_get_language()
        || (Preferred_text_language == System_language && localization_get_language() == localization_query_system_language());
    const bool audio_current = Preferred_audio_language == localization_get_audio_language()
        || (Preferred_audio_language == System_language && localization_get_audio_language() == localization_query_system_language());
    if (!text_current || !audio_current)
    {
        game_options_start_language_switch(Preferred_text_language, Preferred_audio_language); // inlined in the binary
    }
    for (unsigned int setting = 0; setting < NUM_CONTROL_CONFIG_SETTINGS; setting++)
    {
        // Magic number: setting 5 is not copied (as not in the ini file).
        if (setting != 5)
        {
            Control_config[0][setting] = Control_config_from_options[0][setting];
        }
    }
    controls_set_axis_action_inverted_state(CAA_CAMERA_ROTATE, o.invert_x != 0, multi_core_platform_get_active_controller(), true);
    controls_set_axis_action_inverted_state(CAA_DRIVE_CAMERA_ROTATE, o.invert_x != 0, multi_core_platform_get_active_controller(), true);
    controls_set_axis_action_inverted_state(CAA_TANK_CAMERA_ROTATE, o.invert_x != 0, multi_core_platform_get_active_controller(), true);
    controls_set_axis_action_inverted_state(CAA_CAMERA_ELEVATE, o.invert_y != 0, multi_core_platform_get_active_controller(), true);
    controls_set_axis_action_inverted_state(CAA_DRIVE_CAMERA_ELEVATE, o.invert_y != 0, multi_core_platform_get_active_controller(), true);
    controls_set_axis_action_inverted_state(CAA_TANK_CAMERA_ELEVATE, o.invert_y != 0, multi_core_platform_get_active_controller(), true);
    controls_verify_control_config(true);
    player_set_autoaim_enabled(o.aim_assist != 0);
    Camera_shake_enabled = n.camera_shake == 0;
}

// 0x007F03A0 (:877)
// The profile (stats and tool tips) from the save data when there is some, then the options cache applied.
void game_options_load()
{
    if (Save_system_save_data != nullptr)
    {
        memcpy(&Last_profile_data_from_save_data, &Save_system_save_data->profile_data, sizeof(player_profile_data));
        stats_serialize_in_profile_stats();
        ui_tool_tips_system_load_from_profile();
    }
    game_options_load_from_cache();
    Game_has_loaded_options_at_least_once = true;
}

// 0x007CF1F0 (:890)
void game_options_update_cache()
{
    game_options_write_to_cache(&Last_options_from_save_data, Player_controller_index != 0xFFFFFFFFu ? Player_controller_index : 0);
}

// 0x007D9980 (:895)
void game_options_save()
{
    game_options_write_to_cache(&Last_options_from_save_data, Player_controller_index != 0xFFFFFFFFu ? Player_controller_index : 0);
    game_save_write_options();
}

// 0x007D99B0 (:919)
void game_options_save_display_options()
{
    display_options_write_to_cache(&Last_options_from_save_data);
    game_save_write_options();
}

// 0x007E7E30 (:1088)
// The [graphics] section, written from the screen, the level of detail and the performance mode (then reset to the
// only mode). The screen's values are stored back (its aspect ratio from the size).
template<>
void game_options_ini_file_read_writer<1>::parse_display_settings(keen::FilePath path, rl_screen_info& info, rl_level_of_detail& lod, unsigned char& camera_fov)
{
    int rr_num;
    int rr_den;
    info.get_refresh_rate(rr_num, rr_den);
    const int width = info.get_screen_width();
    const int height = info.get_screen_height();
    const unsigned int monitor = info.get_monitor();
    const keen::graphics::WindowMode window_mode = info.get_window_mode();
    const bool vsync_enabled = info.get_vsync_enabled();
    keen::writeIniFileInt(path, "graphics", "current_width", width);
    keen::writeIniFileInt(path, "graphics", "current_height", height);
    keen::writeIniFileInt(path, "graphics", "rr_num", rr_num);
    keen::writeIniFileInt(path, "graphics", "rr_den", rr_den);
    keen::writeIniFileInt(path, "graphics", "monitor", (int)monitor);
    keen::writeIniFileInt(path, "graphics", "window_mode", (int)window_mode);
    keen::writeIniFileBool(path, "graphics", "vsync_enabled", vsync_enabled);
    keen::writeIniFileInt(path, "graphics", "camera_fov", camera_fov);
    // rfg.exe stores the fields directly; the aspect ratio is width / height.
    info.set_screen_width(width);
    info.set_screen_height(height);
    info.set_refresh_rate(rr_num, rr_den);
    info.set_monitor(monitor);
    info.set_window_mode(window_mode);
    info.set_vsync_enabled(vsync_enabled);
    keen::writeIniFileInt(path, "graphics", "shadow_lod", lod.m_shadow_lod);
    keen::writeIniFileInt(path, "graphics", "aa_lod", lod.m_aa_lod);
    keen::writeIniFileInt(path, "graphics", "particle_lod", lod.m_particle_lod);
    keen::writeIniFileBool(path, "graphics", "soft_shadows", lod.m_soft_shadows);
    keen::writeIniFileBool(path, "graphics", "bloom_enabled", lod.m_bloom_enabled);
    keen::writeIniFileBool(path, "graphics", "motion_blur_enabled", lod.m_motion_blur_enabled);
    keen::writeIniFileBool(path, "graphics", "depth_of_field_enabled", lod.m_depth_of_field_enabled);
    keen::writeIniFileBool(path, "graphics", "distortion_enabled", lod.m_distortion_enabled);
    keen::writeIniFileBool(path, "graphics", "ssao_enabled", lod.m_ssao_enabled);
    keen::writeIniFileBool(path, "graphics", "sun_shafts_enabled", lod.m_sun_shafts_enabled);
    keen::writeIniFileInt(path, "graphics", "texture_resolution", lod.m_new_data.m_texture_reduction);
    keen::writeIniFileInt(path, "graphics", "anisotropic_filtering", lod.m_new_data.m_anisotropic_filtering);
    keen::writeIniFileInt(path, "graphics", "performance_mode", Game_performance_mode);
    // The modes of the PC build (game_options_enumerate_performance_modes, inlined): only GOPM_HIGH_PERFORMANCE.
    Game_performance_mode = GOPM_HIGH_PERFORMANCE;
}

// 0x007E82C0 (:1162)
// The [input] bindings of one control configuration (the reader's keys).
template<>
void game_options_ini_file_read_writer<1>::parse_controls_config(keen::FilePath path, unsigned int controller_type, control_config& config)
{
    char key0[0x40];
    char key1[0x40];
    // Magic number: 0x6C, the button actions the ini file has.
    for (size_t i = 0; i < 0x6C; i++)
    {
        keen::formatString(key0, sizeof(key0), "dev_%zu_button_%zu_0", (size_t)controller_type, i);
        keen::formatString(key1, sizeof(key1), "dev_%zu_button_%zu_1", (size_t)controller_type, i);
        const button_binding_data& binding = config.button_bindings[i];
        keen::writeIniFileInt(path, "input", key0, (int)binding.button[0]);
        keen::writeIniFileInt(path, "input", key1, (int)binding.button[1]);
    }
    char key0_inverted[0x40];
    char key1_inverted[0x40];
    // Magic number: 0x1A, the axis actions (NUM_CONTROL_AXIS_ACTIONS).
    for (size_t i = 0; i < 0x1A; i++)
    {
        keen::formatString(key0, sizeof(key0), "dev_%zu_axis_%zu_0", (size_t)controller_type, i);
        keen::formatString(key1, sizeof(key1), "dev_%zu_axis_%zu_1", (size_t)controller_type, i);
        keen::formatString(key0_inverted, sizeof(key0_inverted), "dev_%zu_axis_%zu_0_inverted", (size_t)controller_type, i);
        keen::formatString(key1_inverted, sizeof(key1_inverted), "dev_%zu_axis_%zu_1_inverted", (size_t)controller_type, i);
        const axis_binding_data& binding = config.axis_bindings[i];
        keen::writeIniFileInt(path, "input", key0, (int)binding.axis[0]);
        keen::writeIniFileInt(path, "input", key1, (int)binding.axis[1]);
        keen::writeIniFileBool(path, "input", key0_inverted, binding.inverted[0]);
        keen::writeIniFileBool(path, "input", key1_inverted, binding.inverted[1]);
    }
}

// 0x007EC300 (:1199)
// The options cache into the ini file, with the reader's sections and keys.
template<>
void game_options_ini_file_read_writer<1>::read_or_write_ini_file(keen::FilePath path)
{
    rfg_game_options& o = Last_options_from_save_data;
    keen::writeIniFileBool(path, "game", "tool_tips_enabled", o.tool_tips_enabled != 0);
    keen::writeIniFileBool(path, "game", "subtitles_enabled", o.subtitles_enabled != 0);
    parse_display_settings(path, o.screen_info, o.level_of_detail, o.camera_fov);
    keen::writeIniFileBool(path, "audio", "audio_enabled", o.audio_enabled != 0);
    keen::writeIniFileInt(path, "audio", "volume_ambience", o.volume_ambience);
    keen::writeIniFileInt(path, "audio", "volume_music", o.volume_music);
    keen::writeIniFileInt(path, "audio", "volume_voice_radio", o.volume_voice_radio);
    keen::writeIniFileInt(path, "audio", "volume_sfx_interface", o.volume_sfx_interface);
    keen::writeIniFileInt(path, "audio", "volume_voicechat", o.new_options.volume_voicechat);
    keen::writeIniFileInt(path, "audio", "volume_overall", o.volume_overall);
    keen::writeIniFileInt(path, "input", "on_foot_controls", o.on_foot_controls);
    keen::writeIniFileInt(path, "input", "vehicle_controls", o.vehicle_controls);
    keen::writeIniFileInt(path, "input", "stick_controls", o.stick_controls);
    keen::writeIniFileInt(path, "input", "tank_stick_controls", o.tank_stick_controls);
    // Original: written a second time.
    keen::writeIniFileInt(path, "input", "tank_stick_controls", o.tank_stick_controls);
    keen::writeIniFileBool(path, "input", "push_to_talk_enabled", o.push_to_talk_enabled != 0);
    keen::writeIniFileBool(path, "input", "vibration", o.vibration != 0);
    keen::writeIniFileBool(path, "input", "crouch_hold", o.crouch_hold != 0);
    keen::writeIniFileBool(path, "input", "zoom_hold", o.new_options.zoom_hold != 0);
    keen::writeIniFileBool(path, "input", "invert_x", o.invert_x != 0);
    keen::writeIniFileBool(path, "input", "invert_y", o.invert_y != 0);
    keen::writeIniFileBool(path, "input", "aim_assist", o.aim_assist != 0);
    keen::writeIniFileFloat(path, "input", "pitch_sensitivity", o.pitch_sensitivity);
    keen::writeIniFileFloat(path, "input", "heading_sensitivity", o.heading_sensitivity);
    keen::writeIniFileFloat(path, "input", "mouse_sensitivity", o.mouse_sensitivity);
    rfg_new_options& n = o.new_options;
    keen::writeIniFileBool(path, "input", "gyro_enabled_third_person", n.gyro_enabled_third_person != 0);
    keen::writeIniFileBool(path, "input", "gyro_enabled_vehicle", n.gyro_enabled_vehicle != 0);
    keen::writeIniFileBool(path, "input", "gyro_enabled_turret", n.gyro_enabled_turret != 0);
    keen::writeIniFileBool(path, "input", "gyro_enabled_satellite", n.gyro_enabled_satellite != 0);
    keen::writeIniFileFloat(path, "input", "gyro_sensitivity_third_person_x", n.gyro_sensitivity_third_person_x);
    keen::writeIniFileFloat(path, "input", "gyro_sensitivity_third_person_y", n.gyro_sensitivity_third_person_y);
    keen::writeIniFileFloat(path, "input", "gyro_sensitivity_vehicle_x", n.gyro_sensitivity_vehicle_x);
    keen::writeIniFileFloat(path, "input", "gyro_sensitivity_vehicle_y", n.gyro_sensitivity_vehicle_y);
    keen::writeIniFileFloat(path, "input", "gyro_sensitivity_turret_x", n.gyro_sensitivity_turret_x);
    keen::writeIniFileFloat(path, "input", "gyro_sensitivity_turret_y", n.gyro_sensitivity_turret_y);
    keen::writeIniFileFloat(path, "input", "gyro_sensitivity_satellite_x", n.gyro_sensitivity_satellite_x);
    keen::writeIniFileFloat(path, "input", "gyro_sensitivity_satellite_y", n.gyro_sensitivity_satellite_y);
    keen::writeIniFileInt(path, "input", "gyro_orientation_config_x_sensor", n.gyro_orientation_config_x.sensor_id);
    keen::writeIniFileFloat(path, "input", "gyro_orientation_config_x_dir", n.gyro_orientation_config_x.direction);
    keen::writeIniFileInt(path, "input", "gyro_orientation_config_y_sensor", n.gyro_orientation_config_y.sensor_id);
    keen::writeIniFileFloat(path, "input", "gyro_orientation_config_y_dir", n.gyro_orientation_config_y.direction);
    keen::writeIniFileInt(path, "input", "gyro_angular_velocity_config_x_sensor", n.gyro_angular_velocity_config_x.sensor_id);
    keen::writeIniFileFloat(path, "input", "gyro_angular_velocity_config_x_dir", n.gyro_angular_velocity_config_x.direction);
    keen::writeIniFileInt(path, "input", "gyro_angular_velocity_config_y_sensor", n.gyro_angular_velocity_config_y.sensor_id);
    keen::writeIniFileFloat(path, "input", "gyro_angular_velocity_config_y_dir", n.gyro_angular_velocity_config_y.direction);
    keen::writeIniFileBool(path, "input", "hide_xinput", n.hide_xinput != 0);
    keen::writeIniFileBool(path, "input", "hide_dinput", n.hide_dinput != 0);
    keen::writeIniFileBool(path, "input", "hide_steam_controller", n.hide_steam_controller != 0);
    keen::writeIniFileInt(path, "input", "vehicle_mouse_cam", n.vehicle_mouse_cam);
    keen::writeIniFileBool(path, "game", "disable_map_rotation", o.disable_map_rotation != 0);
    // Original: the ini file has the inverse of camera_shake (inverted around the write).
    n.camera_shake = n.camera_shake == 0;
    keen::writeIniFileBool(path, "game", "camera_shake", n.camera_shake != 0);
    n.camera_shake = n.camera_shake == 0;
    keen::writeIniFileInt(path, "game", "preferred_text_base", o.preferred_text_language);
    keen::writeIniFileInt(path, "game", "preferred_audio_base", o.preferred_audio_language);
    for (unsigned int setting = 0; setting < NUM_CONTROL_CONFIG_SETTINGS; setting++)
    {
        // Magic number: setting 5 is not in the ini file.
        if (setting != 5)
        {
            parse_controls_config(path, setting, Control_config_from_options[0][setting]);
        }
    }
}

// 0x007EE770 (:1371)
// The options cache into <roaming app data>/kaiko/rfg/config.ini, its folders created first.
void game_options_write_ini_file_from_cache()
{
    keen::FilePath path;
    char folder[0x104];
    if (keen::getKnownWindowsFolder(folder, 0x100, keen::KnownWindowsFolderId_RoamingAppData))
    {
        path = keen::FilePath(folder, true);
        path.pushDirectory("kaiko");
        path.pushDirectory("rfg");
        const keen::IoError error = keen::createNativeDirectoryPath(path.getCompletePath());
        if (error == keen::IoError_AlreadyExists || error == keen::IoError_Ok)
        {
            path.setFileNameWithExtension("config.ini");
            game_options_ini_file_read_writer<1>::read_or_write_ini_file(path);
        }
    }
}

// 0x007C1E40 (:1404)
// The localization shows its loading state while a sim job switches the languages.
void game_options_start_language_switch(keen::LanguageId text_lang, keen::LanguageId audio_lang)
{
    localization_set_loading(text_lang, audio_lang);
    cpu_job job;
    job.trigger = nullptr;
    job.flags = 0;
    job.scratch_buffer = nullptr;
    job.scratch_buffer_size = 0;
    job.thread_id = get_current_thread_id();
    job.arg1.int_val = (unsigned int)text_lang;
    job.arg2.int_val = (unsigned int)audio_lang;
    job.job_func = game_options_language_switch_job;
    job.type = ccjt_sim;
    scheduler_add_job(nullptr, &job, nullptr);
}

#endif // OPENRFG_AUDIO
