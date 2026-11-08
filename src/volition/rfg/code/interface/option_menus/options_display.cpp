// Original: volition/rfg/code/interface/option_menus/options_display.cpp
//
// The display options (GS_MAINMENU_OPTIONS_DISPLAY, UI.md UI6) on rfg_ui_options_details, two pages: general
// (resolution, refresh rate, window mode, vsync, field of view, the detail preset) and advanced (shadows, soft shadows,
// depth of field, motion blur, particles, ambient occlusion, sun shafts, texture resolution, anisotropic filtering).
// Accept applies the detail settings and the field of view, saves, and goes through GS_RESOLUTION_CHANGE when the
// screen changed: the render thread switches the mode, then a box asks to keep it (15 s, else it reverts).
//
// Compiled twice: rfg_app has the boot part (orig_main.md OM3: the current display settings and their read from the
// options cache, through game_options_load_display_options); rfg_ui the menus (OPENRFG_UI).

#include "volition/rfg/code/interface/option_menus/option_menu_system.h"
#include "volition/rfg/code/gamewide/game_options.h"

#if !defined(OPENRFG_UI)

display_info_pc Current_display_info; // 0x02C0F400

// 0x00892400 (:789)
// The cached options' screen and level of detail become the current display settings and the caller's; also the vsync
// flag and the camera field of view.
bool display_options_load_from_cache(rfg_game_options* game_options, rl_screen_info& screen_info, rl_level_of_detail& lod, bool& vsync_enabled, int& camera_fov)
{
    Current_display_info.m_screen_info = game_options->screen_info;
    screen_info = game_options->screen_info;
    Current_display_info.m_level_of_detail = game_options->level_of_detail;
    lod = game_options->level_of_detail;
    Current_display_info.m_vsyncEnabled = game_options->screen_info.get_vsync_enabled();
    vsync_enabled = game_options->screen_info.get_vsync_enabled();
    camera_fov = game_options->camera_fov;
    return true;
}

#else // OPENRFG_UI

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_string_processing.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_device.h"
#include "volition/rfg/code/camera/camera_view.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/option_menus/ingame_options.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"

#include <string.h>

// ---- file-private types (rfg.pdb)
// size 0x4
struct screen_size
{
    unsigned short width; // 0x00
    unsigned short height; // 0x02
};
static_assert(sizeof(screen_size) == 0x4, "screen_size");

// size 0x8
struct refresh_rate
{
    unsigned int num; // 0x00
    unsigned int den; // 0x04
};
static_assert(sizeof(refresh_rate) == 0x8, "refresh_rate");

// ---- file-static globals (UI0 left this file's statics out; addresses from the asm)
static const char* cPage_names_dxoptions[2] = {"MENU_GENERAL", "MENU_ADVANCED"}; // 0x01664154
static detail_option_pages Cur_page_dxoptions = DETAIL_OPTIONS_PAGE_INVALID; // 0x0166415C
static const char* cDisplay_settings_OLMH[4] = {"MENU_OFF", "MENU_LOW", "MENU_MED", "MENU_HIGH"}; // 0x01664160
static const char* cDisplay_settings_LMH[3] = {"MENU_LOW", "MENU_MED", "MENU_HIGH"}; // 0x01664170
static const char* cDisplay_settings_LMHV[4] = {"MENU_LOW", "MENU_MED", "MENU_HIGH", "MENU_VERY_HIGH"}; // 0x016641A0
static const char* cAnti_alias_lod_names[7] = {"MENU_OFF", "MENU_AA_LOD_MODE_2X", "MENU_AA_LOD_MODE_4X", "MENU_AA_LOD_MODE_8X", "MENU_AA_LOD_MODE_16X", "MENU_AA_LOD_MODE_8XQ", "MENU_AA_LOD_MODE_16XQ"}; // 0x016641B0
static const char* cDisplay_default_lod_setting_names[5] = {"MENU_LOW", "MENU_MED", "MENU_HIGH", "MENU_VERY_HIGH", "MENU_CUSTOM"}; // 0x016641CC
static const char* cDisplay_window_mode_names[3] = {"MENU_WINDOW_MODE_WINDOWED", "MENU_WINDOW_MODE_BORDERLESS", "MENU_WINDOW_MODE_FULLSCREEN"}; // 0x016641E0
// Magic number: 45 degrees, the default field of view.
static int Fov_value = 45; // 0x016641EC
static int Prev_fov_value = 45; // 0x016641F0
static const char* cDoc_name_doptions = {"rfg_ui_options_details"}; // 0x016641FC
static int Doc_handle_doptions = -1; // 0x01664200
static int Control_id_doptions = -1; // 0x01664204
static int Input_id_doptions = -1; // 0x01664208
static int Menu_handle_doptions = -1; // 0x0166420C
static int Message_box_handle_doptions = -1; // 0x01664210
// Initial 16 in the binary (set when the tab item is made).
static unsigned int Page_tab_handle_doptions = 16; // 0x01664224
static int Res_change_control_id = -1; // 0x01664230
static int Res_change_input_id = -1; // 0x01664234
static int Res_change_message_box_handle = -1; // 0x01664238
resolution_change_stage Res_change_state = RC_STAGE_INVALID; // 0x0166423C
// int Doc_handle_main (0x016631AC) is used here too; placed in ui_menu.cpp.
extern int Doc_handle_main; // 0x016631AC, ui_menu.cpp
// bool Display_character (0x0166316F) is used here too; placed in rfg_lua_actions.cpp.
extern bool Display_character; // 0x0166316F, rfg_lua_actions.cpp
static rl_screen_info Old_screen_info; // 0x01666D20
static rl_screen_info Pending_screen_info; // 0x01666DC0
static rl_level_of_detail Old_lod; // 0x01666E60
static rl_level_of_detail Pending_lod; // 0x01666EF8
static timestamp Revert_timer; // 0x01666F8C
// The header's object names (this file's copies).
static const char* kPageHeaders[2] = {"page1", "page2"}; // 0x016686CC
static const char* kRightArrow = {"row_highlight_right"}; // 0x016686D4
static const char* kLeftArrow = {"row_highlight_left"}; // 0x016686D8
static const char* kDetailArrows = {"row_highlight_arrows"}; // 0x016686DC
static const char* kRowGroup = {"details_row_main"}; // 0x016686E0
static const char* kHints3 = {"hints3"}; // 0x016686E4
static const char* kHints2 = {"hints2"}; // 0x016686E8
static const char* kHints1 = {"hints"}; // 0x016686EC
static keen::DisplayMode Pending_resolution; // 0x02BE7D70
static char bkgrnd_image_name[32]; // 0x02BF22F0
static char Supported_refresh_rate_names[512][16]; // 0x02BF2310
static int Temp_counter; // 0x02BF448C
// char Background_image_name[64] (0x02BFE030) is used here too; placed in ui_message_box.cpp.
static char Supported_resolution_names[512][16]; // 0x02BFE088
static resolution_change_stage Delay_next_state; // 0x02C00088
static int Delay_frame_count; // 0x02C02BB4
// Never written in rfg.exe (a revert restores this zeroed mode into the pending one).
static keen::DisplayMode Old_resolution; // 0x02C02C44
static bool Pending_vsync_setting; // 0x02C0375D
static bool Vsync_needs_update; // 0x02C0375E
static bool Assets_loaded_doptions; // 0x02C0375F
static int Num_supported_aa_modes; // 0x02C03760
static rl_antialias_lod Supported_aa_modes[RL_NUM_AA_LODS]; // 0x02C03764
static const char* Supported_aa_mode_names[RL_NUM_AA_LODS]; // 0x02C03780
static int Num_supported_resolutions; // 0x02C037A0
static display_default_settings Selected_default_lod; // 0x02C037A4
static keen::DisplayMode cSupported_resolution_modes[512]; // 0x02C037A8
static screen_size Supported_resolutions[512]; // 0x02C057A8
static refresh_rate Supported_refresh_rates[512]; // 0x02C05FA8
static const char* Supported_resolution_names_list[512]; // 0x02C06FA8
static const char* Supported_refresh_rate_names_list[512]; // 0x02C077A8
static int Num_supported_refresh_rates; // 0x02C07FA8
static int Num_different_resolutions; // 0x02C07FAC
static int Screen_size_index; // 0x02C07FB0
static int Refresh_rate_index; // 0x02C07FB4
static bool Waiting_on_save_completion_doptions; // 0x02C07FB8
static bool Resolution_msgbox_needs_load; // 0x02C07FB9
static bool image_loaded; // 0x02C07FBA
static bool Resolution_reverting; // 0x02C07FBB
static keen::graphics::WindowMode Pending_windowed_setting; // 0x02C07FBC
static bool presto_chango; // 0x02C07FC0
// bool Cursor_visible (0x02C081B5) is used here too; placed in ui_message_box.cpp.
// bool Display_background_image (0x02C081B6) is used here too; placed in ui_message_box.cpp.
static rl_level_of_detail Default_lods[4]; // 0x02C4BF70
// bool Active (0x02C03754) is used here too; placed in ui_document_hud_tool_tips.cpp.
extern bool Active; // 0x02C03754, ui_document_hud_tool_tips.cpp (the in-game options are up)

// ---- internal functions

// 0x00891910 (:365)
// LTCG: lhs in ECX, rhs in EAX. The menu's detail settings (antialiasing, bloom and distortion are not compared).
static bool lods_equal(const rl_level_of_detail& lhs, const rl_level_of_detail& rhs)
{
    return lhs.m_depth_of_field_enabled == rhs.m_depth_of_field_enabled && lhs.m_motion_blur_enabled == rhs.m_motion_blur_enabled && lhs.m_particle_lod == rhs.m_particle_lod && lhs.m_shadow_lod == rhs.m_shadow_lod && lhs.m_soft_shadows == rhs.m_soft_shadows && lhs.m_ssao_enabled == rhs.m_ssao_enabled && lhs.m_sun_shafts_enabled == rhs.m_sun_shafts_enabled && lhs.m_new_data.m_texture_reduction == rhs.m_new_data.m_texture_reduction && lhs.m_new_data.m_anisotropic_filtering == rhs.m_new_data.m_anisotropic_filtering;
}

// 0x008AF650 (:382)
// The four presets (low, medium, high, very high). Original: medium's antialiasing, shadows, particles and anisotropic
// filtering and high's shadows... are left as constructed where not set here.
static void display_info_initialize_default_detail_settings()
{
    Default_lods[0].m_new_data.m_texture_reduction = 2;
    Default_lods[2].m_shadow_lod = RL_SHADOW_LOD_MED;
    Default_lods[2].m_particle_lod = RL_PARTICLE_LOD_HIGH;
    Default_lods[2].m_new_data.m_anisotropic_filtering = RL_AF_LOD_MED;
    Default_lods[0].m_aa_lod = RL_AA_LOD_OFF;
    Default_lods[0].m_shadow_lod = RL_SHADOW_LOD_OFF;
    Default_lods[0].m_particle_lod = RL_PARTICLE_LOD_LOW;
    Default_lods[0].m_soft_shadows = false;
    Default_lods[0].m_motion_blur_enabled = false;
    Default_lods[0].m_depth_of_field_enabled = false;
    Default_lods[0].m_ssao_enabled = false;
    Default_lods[0].m_sun_shafts_enabled = false;
    Default_lods[0].m_new_data.m_anisotropic_filtering = RL_AF_LOD_OFF;
    Default_lods[1].m_aa_lod = RL_AA_LOD_OFF;
    Default_lods[1].m_shadow_lod = RL_SHADOW_LOD_LOW;
    Default_lods[1].m_particle_lod = RL_PARTICLE_LOD_MED;
    Default_lods[1].m_soft_shadows = false;
    Default_lods[1].m_motion_blur_enabled = true;
    Default_lods[1].m_depth_of_field_enabled = true;
    Default_lods[1].m_ssao_enabled = false;
    Default_lods[1].m_sun_shafts_enabled = false;
    Default_lods[1].m_new_data.m_texture_reduction = 1;
    Default_lods[1].m_new_data.m_anisotropic_filtering = RL_AF_LOD_LOW;
    Default_lods[2].m_aa_lod = RL_AA_LOD_2X;
    Default_lods[2].m_soft_shadows = true;
    Default_lods[2].m_motion_blur_enabled = true;
    Default_lods[2].m_depth_of_field_enabled = true;
    Default_lods[2].m_ssao_enabled = false;
    Default_lods[2].m_sun_shafts_enabled = false;
    Default_lods[2].m_new_data.m_texture_reduction = 0;
    Default_lods[3].m_aa_lod = RL_AA_LOD_16XQ;
    Default_lods[3].m_shadow_lod = RL_SHADOW_LOD_HIGH;
    Default_lods[3].m_particle_lod = RL_PARTICLE_LOD_VERY_HIGH;
    Default_lods[3].m_soft_shadows = true;
    Default_lods[3].m_motion_blur_enabled = true;
    Default_lods[3].m_depth_of_field_enabled = true;
    Default_lods[3].m_ssao_enabled = true;
    Default_lods[3].m_sun_shafts_enabled = true;
    Default_lods[3].m_new_data.m_texture_reduction = 0;
    Default_lods[3].m_new_data.m_anisotropic_filtering = RL_AF_LOD_HIGH;
}

// The device's antialiasing modes.
static void display_info_read_aa_modes()
{
    Num_supported_aa_modes = (int)rl_render_lib::get_instance()->get_num_msaa_modes();
    rl_render_lib::get_instance()->get_msaa_modes(Supported_aa_modes);
}

// A pending antialiasing level beyond the supported count: the last supported mode (none: off).
static void display_info_clamp_pending_aa()
{
    if ((int)Pending_lod.m_aa_lod > Num_supported_aa_modes)
    {
        Pending_lod.m_aa_lod = Num_supported_aa_modes == 0 ? RL_AA_LOD_OFF : Supported_aa_modes[Num_supported_aa_modes - 1];
    }
}

// 0x008AF730 (:439)
static void display_info_set_to_default_lod_settings(display_default_settings default_setting)
{
    if (default_setting == DISPLAY_DEFAULT_CUSTOM)
    {
        return;
    }
    Pending_lod = Default_lods[default_setting];
    display_info_read_aa_modes();
    // Original: a level beyond the supported count (the level, not its index) clamps to the last mode.
    if ((int)Pending_lod.m_aa_lod > Num_supported_aa_modes)
    {
        if (Num_supported_aa_modes == 0)
        {
            Pending_lod.m_aa_lod = (rl_antialias_lod)Num_supported_aa_modes;
            return;
        }
        Pending_lod.m_aa_lod = Supported_aa_modes[Num_supported_aa_modes - 1];
    }
}

// 0x00891960 (:463)
// The preset the pending details match (none: custom).
static display_default_settings display_info_get_current_lod_setting()
{
    if (lods_equal(Pending_lod, Default_lods[0]))
    {
        return DISPLAY_DEFAULT_LOW;
    }
    if (lods_equal(Pending_lod, Default_lods[1]))
    {
        return DISPLAY_DEFAULT_MED;
    }
    if (lods_equal(Pending_lod, Default_lods[2]))
    {
        return DISPLAY_DEFAULT_HIGH;
    }
    return lods_equal(Pending_lod, Default_lods[3]) ? DISPLAY_DEFAULT_VERY_HIGH : DISPLAY_DEFAULT_CUSTOM;
}

// 0x00891A90 (:473)
static void display_info_create_resolution_name(char* name, int str_len, const screen_size& res)
{
    _snprintf_safe(name, (unsigned int)str_len, "%d x %d", res.width, res.height);
}

// 0x00891AC0 (:478)
static void display_info_create_refresh_rate_name(char* name, int str_len, const keen::DisplayMode& mode)
{
    _snprintf_safe(name, (unsigned int)str_len, "%.2f Hz", (double)((float)mode.refreshRateNumerator / (float)mode.refreshRateDenominator));
}

// A refresh rate in Hz.
static float display_info_refresh_rate_hz(const refresh_rate& rate)
{
    return (float)rate.num / (float)rate.den;
}

// 0x00891D80 (:564)
// The refresh rates of the modes with that size (and their names).
static void display_info_populate_refresh_rate_list(const screen_size& res)
{
    Num_supported_refresh_rates = 0;
    for (int i = 0; i < Num_supported_resolutions; i++)
    {
        const keen::DisplayMode& mode = cSupported_resolution_modes[i];
        if (mode.height != res.height || mode.width != res.width)
        {
            continue;
        }
        // Magic number: 512 entries.
        if (Num_supported_refresh_rates > 0x1FF)
        {
            return;
        }
        Supported_refresh_rates[Num_supported_refresh_rates].num = mode.refreshRateNumerator;
        Supported_refresh_rates[Num_supported_refresh_rates].den = mode.refreshRateDenominator;
        display_info_create_refresh_rate_name(Supported_refresh_rate_names[Num_supported_refresh_rates], 0x10, mode);
        Supported_refresh_rate_names_list[Num_supported_refresh_rates] = Supported_refresh_rate_names[Num_supported_refresh_rates];
        Num_supported_refresh_rates++;
    }
}

// 0x00891E60 (:588)
static void display_info_resolution_changed(int screen_size_index)
{
    display_info_populate_refresh_rate_list(Supported_resolutions[screen_size_index]);
}

// 0x00891E70 (:595)
static void display_info_refresh_rate_changed(int screen_size_index, int refresh_rate_index)
{
}

// 0x00891E80 (:599)
// LTCG: info in EAX. The size row (exact, else the nearest by width plus height) and the refresh rate row (exact, else
// the nearest in Hz).
static void display_info_get_settings_indices(const rl_screen_info& info, int& screen_size_index, int& refresh_rate_index)
{
    int rr_num;
    int rr_den;
    info.get_refresh_rate(rr_num, rr_den);
    const int width = info.get_screen_width();
    const int height = info.get_screen_height();
    screen_size_index = -1;
    int nearest = -1;
    int nearest_distance = 0;
    for (int i = 0; i < Num_different_resolutions; i++)
    {
        if (Supported_resolutions[i].width == (unsigned int)width && Supported_resolutions[i].height == (unsigned int)height)
        {
            screen_size_index = i;
        }
        const int distance = abs(height - (int)Supported_resolutions[i].height) + abs(width - (int)Supported_resolutions[i].width);
        if (nearest == -1 || distance < nearest_distance)
        {
            nearest = i;
            nearest_distance = distance;
        }
    }
    if (screen_size_index < 0)
    {
        screen_size_index = nearest;
    }
    display_info_populate_refresh_rate_list(Supported_resolutions[screen_size_index]);
    const float hz = (float)rr_num / (float)rr_den;
    refresh_rate_index = -1;
    nearest = -1;
    float nearest_difference = 0.0f;
    for (int i = 0; i < Num_supported_refresh_rates; i++)
    {
        if (Supported_refresh_rates[i].num == (unsigned int)rr_num && Supported_refresh_rates[i].den == (unsigned int)rr_den)
        {
            refresh_rate_index = i;
        }
        const float difference = fabsf(hz - display_info_refresh_rate_hz(Supported_refresh_rates[i]));
        if (nearest == -1 || difference < nearest_difference)
        {
            nearest = i;
            nearest_difference = difference;
        }
    }
    if (refresh_rate_index < 0)
    {
        refresh_rate_index = nearest;
    }
}

// 0x008AF7A0 (:666)
static void display_info_get_current_resolution_settings()
{
    const rl_screen_info info = *rl_render_lib::get_instance()->get_screen_info();
    display_info_get_settings_indices(info, Screen_size_index, Refresh_rate_index);
    Pending_vsync_setting = rl_render_lib::get_instance()->get_vsync_enabled();
}

// 0x008924D0 (:857)
static void display_selection_callback(int selected_item)
{
}

// 0x008926D0 (:923)
static display_default_settings get_default_lod_setting()
{
    return DISPLAY_DEFAULT_MED;
}

// 0x008926E0 (:965)
static bool screen_info_changed()
{
    int pending_num;
    int pending_den;
    int old_num;
    int old_den;
    Pending_screen_info.get_refresh_rate(pending_num, pending_den);
    Old_screen_info.get_refresh_rate(old_num, old_den);
    return !(Pending_screen_info.get_window_mode() == Old_screen_info.get_window_mode() && Pending_screen_info.get_screen_height() == Old_screen_info.get_screen_height() && Pending_screen_info.get_screen_width() == Old_screen_info.get_screen_width() && pending_num == old_num && pending_den == old_den);
}

// 0x00892730 (:984)
static bool lod_info_changed()
{
    return !lods_equal(Pending_lod, Old_lod);
}

// 0x008AFA80 (:989)
static bool vsync_info_changed()
{
    return Pending_vsync_setting != rl_render_lib::get_instance()->get_vsync_enabled();
}

// 0x008927C0 (:994)
static bool misc_info_changed()
{
    return Fov_value != Prev_fov_value;
}

// 0x008AFAA0 (:999)
static bool changes_made()
{
    return screen_info_changed() || lod_info_changed() || vsync_info_changed() || misc_info_changed();
}

// 0x008AFB20 (:1004)
// The mode of the chosen size and refresh rate becomes the pending resolution and screen.
static void update_pending_resolution()
{
    for (int i = 0; i < Num_supported_resolutions; i++)
    {
        const keen::DisplayMode& mode = cSupported_resolution_modes[i];
        if (mode.height == Supported_resolutions[Screen_size_index].height && mode.width == Supported_resolutions[Screen_size_index].width && mode.refreshRateDenominator == Supported_refresh_rates[Refresh_rate_index].den && mode.refreshRateNumerator == Supported_refresh_rates[Refresh_rate_index].num)
        {
            Pending_resolution = mode;
            Pending_screen_info.set_screen_height((int)Pending_resolution.height);
            Pending_screen_info.set_screen_width((int)Pending_resolution.width);
            Pending_screen_info.set_refresh_rate((int)Pending_resolution.refreshRateNumerator, (int)Pending_resolution.refreshRateDenominator);
            return;
        }
    }
}

// 0x008C4130 (:1025)
// Accepted: the chosen preset; cancelled: back to custom.
static bool display_menu_default_lod_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Message_box_handle_doptions != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        display_info_set_to_default_lod_settings(Selected_default_lod);
    }
    else if (choice == MBC_ABORT)
    {
        Selected_default_lod = DISPLAY_DEFAULT_CUSTOM;
        // Magic numbers: row 5 (details), choice 4 (custom).
        ui_menu_set_char_list_index(Menu_handle_doptions, 5, 4);
    }
    else if (choice != MBC_FORCED_EXIT)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    Message_box_handle_doptions = -1;
    return true;
}

// 0x00901CF0 (:1057)
// General: size (the highest refresh rate of it), refresh rate, window mode (size and refresh rate only in fullscreen,
// from the main menu), the preset (asks before dropping custom details). Advanced: the char list rows (the bool rows
// write their variables themselves).
static void option_change_occurred_doptions(int choice, void* new_value)
{
    const int value = (int)(intptr_t)new_value;
    if (Cur_page_dxoptions == DETAIL_OPTIONS_PAGE_GENERAL)
    {
        const display_default_settings previous_lod = Selected_default_lod;
        switch (choice)
        {
        case 0:
        {
            Screen_size_index = value;
            display_info_populate_refresh_rate_list(Supported_resolutions[value]);
            float highest = 0.0f;
            Refresh_rate_index = 0;
            for (int i = 0; i < Num_supported_refresh_rates; i++)
            {
                const float hz = display_info_refresh_rate_hz(Supported_refresh_rates[i]);
                if (highest < hz)
                {
                    Refresh_rate_index = i;
                    highest = hz;
                }
            }
            update_pending_resolution();
            return;
        }
        case 1:
            Refresh_rate_index = value;
            update_pending_resolution();
            return;
        case 2:
            Pending_windowed_setting = (keen::graphics::WindowMode)value;
            Pending_screen_info.set_window_mode((keen::graphics::WindowMode)value);
            if (main_menu_get_active())
            {
                ui_menu_enable_menu_item(Menu_handle_doptions, 0, Pending_windowed_setting != keen::graphics::WindowMode_Borderless);
                ui_menu_enable_menu_item(Menu_handle_doptions, 1, Pending_windowed_setting == keen::graphics::WindowMode_Fullscreen);
            }
            return;
        case 5:
            Selected_default_lod = (display_default_settings)value;
            if (previous_lod == DISPLAY_DEFAULT_CUSTOM && display_info_get_current_lod_setting() == DISPLAY_DEFAULT_CUSTOM)
            {
                Message_box_handle_doptions = ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_WARNING", "MENU_LOSE_CUSTOM_SETTING_CONFIRM", display_menu_default_lod_cb, false, false, nullptr, nullptr, false);
                return;
            }
            display_info_set_to_default_lod_settings((display_default_settings)value);
            return;
        default:
            return;
        }
    }
    if (Cur_page_dxoptions == DETAIL_OPTIONS_PAGE_ADVANCED)
    {
        switch (choice)
        {
        case 0:
            Pending_lod.m_shadow_lod = (rl_shadow_lod)value;
            return;
        case 4:
            Pending_lod.m_particle_lod = (rl_particle_lod)value;
            return;
        case 7:
            // The row lists low to high; the reduction is the other way.
            Pending_lod.m_new_data.m_texture_reduction = (unsigned char)(2 - value);
            return;
        case 8:
            Pending_lod.m_new_data.m_anisotropic_filtering = (rl_anisotropic_filtering_lod)value;
            return;
        default:
            return;
        }
    }
}

// 0x008C41C0 (:1180)
static bool display_menu_default_controls_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Message_box_handle_doptions != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        display_reset_defaults();
    }
    else if ((int)choice < 1 || (int)choice > 2)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    Message_box_handle_doptions = -1;
    return true;
}

// 0x008927E0 (:1205)
static bool display_menu_cancel_message_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Message_box_handle_doptions != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        game_options_load_from_cache();
        if (gameseq_get_state() == GS_MAINMENU_OPTIONS_DISPLAY)
        {
            gameseq_pop_state();
        }
    }
    else if ((int)choice < 1 || (int)choice > 2)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    Message_box_handle_doptions = -1;
    return true;
}

// 0x00902080 (:1235)
// Accept: the details applied, the field of view set, the options saved when only details, vsync or the field of view
// changed (vsync applied in the handshake); the menu leaves, through the resolution change when the screen changed.
// Back: a box if changed. Misc: the defaults box (or a box saying they are already set).
static void display_hint_callback(hint_type hint)
{
    if (hint == HT_SELECTION)
    {
        if (lod_info_changed())
        {
            rl_render_lib::get_instance()->set_level_of_detail(Pending_lod);
            Current_display_info.m_level_of_detail = Pending_lod;
        }
        camera_view_set_custom_fov(Fov_value, true);
        if ((lod_info_changed() || vsync_info_changed() || misc_info_changed()) && !screen_info_changed())
        {
            if (vsync_info_changed())
            {
                Current_display_info.m_vsyncEnabled = Pending_vsync_setting;
                Vsync_needs_update = true;
            }
            game_options_save_display_options();
        }
        if (gameseq_get_state() == GS_MAINMENU_OPTIONS_DISPLAY)
        {
            gameseq_pop_state();
            if (screen_info_changed())
            {
                gameseq_push_state(GS_RESOLUTION_CHANGE, false, false);
            }
        }
        return;
    }
    if (hint == HT_ABORT)
    {
        if (changes_made())
        {
            Message_box_handle_doptions = ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_WARNING", "MENU_LOSE_CHANGES_CONFIRM", display_menu_cancel_message_cb, false, false, nullptr, nullptr, false);
            return;
        }
        if (gameseq_get_state() == GS_MAINMENU_OPTIONS_DISPLAY)
        {
            gameseq_pop_state();
        }
        return;
    }
    if (hint != HT_MISC)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
    if (!display_options_are_changes_different_from_defaults())
    {
        ui_add_msgbox(MBT_OK, "MENU_WARNING", "MENU_DEFAULTS_NOT_CHANGED", nullptr, false, false, nullptr, nullptr, false);
        return;
    }
    Message_box_handle_doptions = ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_WARNING", "MENU_RESET_DEFAULT_CONFIRM", display_menu_default_controls_cb, false, false, nullptr, nullptr, false);
}

// Magic numbers: the general page's rows.
enum
{
    DOPTIONS_ROW_RESOLUTION = 0,
    DOPTIONS_ROW_REFRESH_RATE = 1,
    DOPTIONS_ROW_WINDOW_MODE = 2,
};

// 0x00902210 (:1310)
// The general page: size and refresh rate (fullscreen only; refresh rate also needs more than one), window mode
// (none of the three in game), vsync, field of view (45 to 85), the preset.
static void display_info_update_menu()
{
    Pending_windowed_setting = Pending_screen_info.get_window_mode();
    ui_menu_add_char_list_item(Menu_handle_doptions, "MENU_RESOLUTION", Supported_resolution_names_list, Num_different_resolutions, option_change_occurred_doptions, Screen_size_index, true);
    ui_menu_add_char_list_item(Menu_handle_doptions, "MENU_REFRESH_RATE", Supported_refresh_rate_names_list, Num_supported_refresh_rates, option_change_occurred_doptions, Refresh_rate_index, true);
    ui_menu_add_char_list_item(Menu_handle_doptions, "MENU_WINDOW_MODE", cDisplay_window_mode_names, 3, option_change_occurred_doptions, Pending_windowed_setting, true);
    ui_menu_add_bool_item(Menu_handle_doptions, "MENU_VSYNC", &Pending_vsync_setting, option_change_occurred_doptions, true);
    // Magic numbers: the field of view goes from 45 to 85 in steps of 1.
    ui_menu_add_int_item(Menu_handle_doptions, "MENU_FOV", &Fov_value, 45, 85, 1, option_change_occurred_doptions, true);
    Selected_default_lod = display_info_get_current_lod_setting();
    ui_menu_add_char_list_item(Menu_handle_doptions, "MENU_DETAILS", cDisplay_default_lod_setting_names, 5, option_change_occurred_doptions, Selected_default_lod, true);
    ui_menu_enable_menu_item(Menu_handle_doptions, DOPTIONS_ROW_RESOLUTION, Pending_windowed_setting != keen::graphics::WindowMode_Borderless);
    ui_menu_enable_menu_item(Menu_handle_doptions, DOPTIONS_ROW_REFRESH_RATE, Pending_windowed_setting == keen::graphics::WindowMode_Fullscreen);
    if (gameseq_get_state() == GS_MAINMENU_IDLE || Doc_handle_main == -1)
    {
        ui_menu_enable_menu_item(Menu_handle_doptions, DOPTIONS_ROW_RESOLUTION, false);
        ui_menu_enable_menu_item(Menu_handle_doptions, DOPTIONS_ROW_REFRESH_RATE, false);
        ui_menu_enable_menu_item(Menu_handle_doptions, DOPTIONS_ROW_WINDOW_MODE, false);
    }
    if (Num_supported_refresh_rates < 2)
    {
        ui_menu_enable_menu_item(Menu_handle_doptions, DOPTIONS_ROW_REFRESH_RATE, false);
    }
}

// A menu hint: the tag's localized text with its button markup, and the button's image.
static void display_add_hint(hint_type ht, control_button_action action, const char* tag)
{
    char text[128];
    char image[256];
    // Magic number: 0x80, the hint text buffer's size.
    controls_fill_options_button_hint(text, 0x80, localization_get_display_string_req(tag, nullptr));
    ui_menu* menu = ui_menu_get_via_handle(Menu_handle_doptions);
    if (menu != nullptr)
    {
        // Original: 0x40 passed as the image buffer's size.
        controls_get_button_image_name(image, 0x40, action, Player_controller_index);
        menu->add_hint(ht, action, text, image);
    }
}

// 0x009029D0 (:1419)
// The page tabs: the one under the mouse full, the other (not current) half transparent; a click switches.
static bool display_options_process_mouse_tab_input()
{
    vint_doc_info* info = ui_get_doc_info(Doc_handle_doptions);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return false;
    }
    for (int page = DETAIL_OPTIONS_PAGE_GENERAL; page < NUM_DETAIL_OPTIONS_PAGES; page++)
    {
        vint_object_base* header = vint_object_base::find_object(kPageHeaders[page], info->m_doc);
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
                if (Cur_page_dxoptions != (detail_option_pages)page)
                {
                    set_detail_page((detail_option_pages)page);
                }
                return true;
            }
        }
        else if (Cur_page_dxoptions != (detail_option_pages)page)
        {
            // Magic number: half opacity.
            ui_pc_set_object_opacity(header, 0.5f);
        }
    }
    return false;
}

// 0x008AFC30 (:1778)
// The loading background image behind the resolution change.
static void show_image()
{
    strncpy(bkgrnd_image_name, "ui_loading_dust_0", sizeof(bkgrnd_image_name));
    image_loaded = false;
    const unsigned int srid = stream2_container_find(bkgrnd_image_name);
    if (srid != 0 && stream2_request(srid))
    {
        image_loaded = true;
        // Magic number: 0x40, Background_image_name's size.
        strncpy(Background_image_name, bkgrnd_image_name, 0x40);
        Display_background_image = true;
        return;
    }
    image_loaded = false;
}

// 0x008AFCB0 (:1791)
static void cleanup_image()
{
    Display_background_image = false;
    if (image_loaded)
    {
        const unsigned int srid = stream2_container_find(bkgrnd_image_name);
        if (srid != 0 && stream2_get_status(srid) != ST_UNLOADED)
        {
            stream2_release(srid);
        }
    }
}

// 0x00892890 (:1832)
static void resolution_change_trigger_exit()
{
    Revert_timer.set(0);
    if (gameseq_get_state() == GS_RESOLUTION_CHANGE)
    {
        gameseq_pop_state();
    }
}

// The resolution change's control and input ids go.
static void resolution_change_release_ids()
{
    ui_relinquish_input_id(Res_change_input_id);
    Res_change_input_id = -1;
    ui_relinquish_control_id((unsigned int)Res_change_control_id);
    Res_change_control_id = -1;
}

// 0x008C4220 (:1841)
// The old screen comes back through the stages again.
static void resolution_change_trigger_revert()
{
    Pending_resolution = Old_resolution;
    Pending_screen_info = Old_screen_info;
    Resolution_reverting = true;
    Res_change_state = RC_STAGE1;
    Revert_timer.set(0);
    resolution_change_release_ids();
}

// 0x008C42E0 (:1862)
// Kept: saved and done; otherwise reverted.
static bool res_change_menu_cancel_message_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Res_change_message_box_handle != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        game_options_save_display_options();
        Revert_timer.set(0);
        if (gameseq_get_state() == GS_RESOLUTION_CHANGE)
        {
            gameseq_pop_state();
        }
    }
    else if ((int)choice > 0 && (int)choice < 3)
    {
        resolution_change_trigger_revert();
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    Res_change_message_box_handle = -1;
    return true;
}

// The "keep these settings?" text with the seconds left.
static void resolution_change_confirm_text(char* encoded, int seconds)
{
    wchar_t text[128];
    // Magic numbers: 0x7F wide characters, 0x180 encoded bytes.
    _snprintf_safe(text, 0x7F, localization_get_display_string_req("MENU_PC_RES_CONFIRM_CHANGE_DESC", nullptr), seconds);
    vint_encode_wide_text(encoded, 0x180, text);
}

// 0x00902DA0 (:1934)
// The stages, each a few frames apart: unload, the mode switch (render thread), the cursor, ids, then the box asking to
// keep the change (15 s); a revert ends after its switch.
static void process_state()
{
    switch (Res_change_state)
    {
    case RC_STAGE1:
        Res_change_state = RC_STAGE_UNLOAD;
        return;
    case RC_STAGE2:
        Res_change_state = RC_STAGE_DELAY_FRAMES;
        Delay_frame_count = 2;
        Delay_next_state = RC_STAGE3;
        return;
    case RC_STAGE3:
        Res_change_state = RC_STAGE_DELAY_FRAMES;
        Delay_frame_count = 3;
        Delay_next_state = RC_STAGE4;
        return;
    case RC_STAGE4:
        if (!Cursor_visible)
        {
            Cursor_visible = true;
            mouse_auto_centering(false);
        }
        Res_change_control_id = (int)ui_request_control_id();
        Res_change_input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
        Res_change_state = RC_STAGE_DELAY_FRAMES;
        Delay_frame_count = 2;
        Delay_next_state = RC_STAGE5;
        return;
    case RC_STAGE5:
        Temp_counter = 0;
        Res_change_state = RC_STAGE_DELAY_FRAMES;
        Delay_frame_count = 2;
        Delay_next_state = RC_STAGE_LOADED;
        return;
    case RC_STAGE_UNLOAD:
        Res_change_state = RC_STAGE_DELAY_FRAMES;
        Delay_frame_count = 4;
        Delay_next_state = RC_STAGE_CHANGE_RES;
        return;
    case RC_STAGE_CHANGE_RES:
        Current_display_info.m_screen_info = Pending_screen_info;
        presto_chango = true;
        Res_change_state = RC_STAGE_DELAY_FRAMES;
        Delay_frame_count = 4;
        Delay_next_state = RC_STAGE2;
        return;
    case RC_STAGE_LOADED:
        if (!Resolution_reverting)
        {
            Temp_counter++;
            if (Temp_counter != 1 || Res_change_message_box_handle != -1)
            {
                return;
            }
            // Magic number: 15 s to keep the change.
            Revert_timer.set(15000);
            char encoded[384];
            resolution_change_confirm_text(encoded, 15);
            Resolution_msgbox_needs_load = true;
            Res_change_message_box_handle = ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_PC_RES_CONFIRM_CHANGE_TITLE", encoded, res_change_menu_cancel_message_cb, false, false, nullptr, nullptr, false);
            return;
        }
        resolution_change_trigger_exit();
        return;
    case RC_STAGE_EXIT:
        resolution_change_trigger_exit();
        return;
    case RC_STAGE_DELAY_FRAMES:
        Delay_frame_count--;
        if (Delay_frame_count < 0)
        {
            Res_change_state = Delay_next_state;
        }
        return;
    case RC_STAGE_PHASE2_BEGIN:
        Res_change_state = RC_STAGE1;
        return;
    default:
        return;
    }
}

// ---- external interface

// 0x00891B10 (:483)
void display_info_clear_supported_resolutions()
{
    Num_supported_resolutions = 0;
}

// 0x00891B20 (:488)
int display_info_find_resolution_mode(keen::DisplayMode new_mode, bool ignoreRefresh)
{
    for (int i = 0; i < Num_supported_resolutions; i++)
    {
        const keen::DisplayMode& mode = cSupported_resolution_modes[i];
        if (mode.width == new_mode.width && mode.height == new_mode.height)
        {
            if (ignoreRefresh || (mode.refreshRateNumerator == new_mode.refreshRateNumerator && mode.refreshRateDenominator == new_mode.refreshRateDenominator))
            {
                return i;
            }
        }
    }
    return -1;
}

// 0x00891B70 (:508)
int display_info_find_resolution_mode(unsigned int width, unsigned int height, unsigned int rr_num, unsigned int rr_denom, bool ignoreRefresh)
{
    keen::DisplayMode mode;
    mode.width = width;
    mode.height = height;
    mode.refreshRateNumerator = rr_num;
    mode.refreshRateDenominator = rr_denom;
    return display_info_find_resolution_mode(mode, ignoreRefresh);
}

// 0x00891BC0 (:518)
// At least 640x480 and 4:3, 5:4, 16:9, 16:10 (within 0.01) or 21:9 (within 0.1).
bool resolution_is_supported(const keen::DisplayMode& mode)
{
    // Magic numbers: the 640x480 minimum.
    if (mode.width < 640 || mode.height < 480)
    {
        return false;
    }
    const float aspect = (float)mode.width / (float)mode.height;
    const float aspects[4] = {1.3333334f, 1.25f, 1.7777778f, 1.6f};
    for (int i = 0; i < 4; i++)
    {
        if (aspect - aspects[i] < 0.01f && -(aspect - aspects[i]) < 0.01f)
        {
            return true;
        }
    }
    return aspect - 2.3333333f < 0.1f && -(aspect - 2.3333333f) < 0.1f;
}

// 0x00891CD0 (:534)
// A supported size not listed yet (512 at most), with its name.
void display_info_add_resolution(const keen::DisplayMode& res)
{
    if (!resolution_is_supported(res))
    {
        return;
    }
    for (int i = 0; i < Num_different_resolutions; i++)
    {
        if (res.width == Supported_resolutions[i].width && res.height == Supported_resolutions[i].height)
        {
            return;
        }
    }
    // Magic number: 512 entries.
    if (Num_different_resolutions >= 0x200)
    {
        return;
    }
    Supported_resolutions[Num_different_resolutions].width = (unsigned short)res.width;
    Supported_resolutions[Num_different_resolutions].height = (unsigned short)res.height;
    display_info_create_resolution_name(Supported_resolution_names[Num_different_resolutions], 0x10, Supported_resolutions[Num_different_resolutions]);
    Supported_resolution_names_list[Num_different_resolutions] = Supported_resolution_names[Num_different_resolutions];
    Num_different_resolutions++;
}

// 0x008AF800 (:676)
// The current screen's mode index (the refresh rate counts only in fullscreen).
int display_info_get_current_resolution_mode()
{
    const rl_screen_info* info = rl_render_lib::get_instance()->get_screen_info();
    int rr_num;
    int rr_den;
    info->get_refresh_rate(rr_num, rr_den);
    keen::DisplayMode mode;
    mode.width = (unsigned int)info->get_screen_width();
    mode.height = (unsigned int)info->get_screen_height();
    mode.refreshRateNumerator = (unsigned int)rr_num;
    mode.refreshRateDenominator = (unsigned int)rr_den;
    return display_info_find_resolution_mode(mode, info->get_window_mode() != keen::graphics::WindowMode_Fullscreen);
}

// 0x00892250 (:685)
// The adapter's modes (512 at most), the supported sizes among them, the first size's refresh rates.
void display_info_add_supported_resolutions()
{
    Num_supported_resolutions = 0;
    Num_supported_resolutions = (int)keen::graphics::enumerateDisplayModes(cSupported_resolution_modes, 0x200, rl_get_graphics_system(), nullptr);
    Num_different_resolutions = 0;
    for (int i = 0; i < Num_supported_resolutions; i++)
    {
        display_info_add_resolution(cSupported_resolution_modes[i]);
    }
    display_info_populate_refresh_rate_list(Supported_resolutions[0]);
}

// 0x008AF880 (:708)
void display_info_add_supported_aa_settings()
{
    display_info_read_aa_modes();
    for (int i = 0; i < Num_supported_aa_modes; i++)
    {
        Supported_aa_mode_names[i] = cAnti_alias_lod_names[Supported_aa_modes[i]];
    }
}

// 0x00892350 (:720)
int display_info_find_aa_setting(rl_antialias_lod setting)
{
    for (int i = 0; i < Num_supported_aa_modes; i++)
    {
        if (Supported_aa_modes[i] == setting)
        {
            return i;
        }
    }
    return -1;
}

// 0x008AF8D0 (:733)
keen::graphics::WindowMode display_get_window_mode()
{
    return rl_render_lib::get_instance()->get_screen_info()->get_window_mode();
}

// 0x00892380 (:739)
rl_screen_res display_info_get_default_resolution()
{
    rl_screen_res res;
    res.is_centered = 0;
    res.width = 0;
    res.height = 0;
    res.refresh_rate_num = 0;
    res.refresh_rate_den = 1;
    return res;
}

// 0x008923A0 (:744)
// The modes enumerated again; the exact mode listed.
bool display_info_is_resolution_supported(keen::DisplayMode resolution)
{
    display_info_add_supported_resolutions();
    return display_info_find_resolution_mode(resolution, false) != -1;
}

// 0x00892480 (:802)
bool display_options_write_to_cache(rfg_game_options* game_options)
{
    game_options->screen_info = Current_display_info.m_screen_info;
    game_options->level_of_detail = Current_display_info.m_level_of_detail;
    game_options->screen_info.set_vsync_enabled(Current_display_info.m_vsyncEnabled);
    game_options->camera_fov = (unsigned char)camera_view_get_fov_raw();
    return true;
}

// 0x008AF9A0 (:827)
// The default screen's rows (1280x720 at 60 Hz), windowed, no vsync, 45 degrees and the medium preset. Original: the
// pending screen's rows are recomputed into Screen_size_index/Refresh_rate_index on the way.
bool display_options_are_changes_different_from_defaults()
{
    rl_screen_info defaults;
    // Magic numbers: 60 Hz.
    defaults.set_refresh_rate(60, 1);
    defaults.set_vsync_enabled(false);
    int default_size_index = -1;
    int default_refresh_index = -1;
    display_info_get_settings_indices(defaults, default_size_index, default_refresh_index);
    display_info_get_settings_indices(Pending_screen_info, Screen_size_index, Refresh_rate_index);
    if (default_size_index == Screen_size_index && default_refresh_index == Refresh_rate_index && Pending_windowed_setting == keen::graphics::WindowMode_Windowed && !Pending_vsync_setting && Fov_value == 45)
    {
        return display_info_get_current_lod_setting() != DISPLAY_DEFAULT_MED;
    }
    return true;
}

// 0x008D6E80 (:861)
// The page tab item: {general, advanced, (unused), page, can go left, can go right, L2 image, R2 image}.
void detail_options_create_tab_data_item()
{
    vint_data_item* item = vint_data_item::create_new("PageTabData");
    if (item == nullptr)
    {
        return;
    }
    vint_variant_type types[8] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
    item->set_template(types, 8);
    Page_tab_handle_doptions = item->get_handle();
    ui_add_data_item(Doc_handle_doptions, Page_tab_handle_doptions);
}

// 0x008924E0 (:887)
void display_options_process_tab_data()
{
    vint_data_item* item = vint_data_item::find(Page_tab_handle_doptions);
    char l2_image[32];
    char r2_image[32];
    char l2_text[128];
    char r2_text[128];
    controls_get_button_image_name(l2_image, sizeof(l2_image), CBA_MENU_L2, Player_controller_index);
    controls_get_button_image_name(r2_image, sizeof(r2_image), CBA_MENU_R2, Player_controller_index);
    keen::formatString(l2_text, sizeof(l2_text), "[image:%s]", l2_image);
    keen::formatString(r2_text, sizeof(r2_text), "[image:%s]", r2_image);
    if (item == nullptr)
    {
        return;
    }
    item->set_element(0, vint_variant(cPage_names_dxoptions[0]));
    item->set_element(1, vint_variant(cPage_names_dxoptions[1]));
    // The page, at most the advanced one.
    item->set_element(3, vint_variant((int)Cur_page_dxoptions > 1 ? (int)DETAIL_OPTIONS_PAGE_ADVANCED + 1 : (int)Cur_page_dxoptions + 1));
    item->set_element(4, vint_variant((int)Cur_page_dxoptions > 0));
    item->set_element(5, vint_variant((int)Cur_page_dxoptions < 1));
    item->set_element(6, vint_variant((const char*)l2_text));
    item->set_element(7, vint_variant((const char*)r2_text));
}

// 0x008C3F90 (:928)
// From the main menu the screen goes back to 1280x720 at 60 Hz, windowed, no vsync; the medium preset, 45 degrees;
// the page remade on the same row.
void display_reset_defaults()
{
    if (gameseq_get_state() != GS_MAINMENU_IDLE && Doc_handle_main != -1)
    {
        // Magic numbers: 1280x720 at 60 Hz.
        Pending_resolution.width = 1280;
        Pending_resolution.height = 720;
        Pending_resolution.refreshRateNumerator = 60;
        Pending_resolution.refreshRateDenominator = 1;
        Pending_vsync_setting = false;
        Pending_windowed_setting = keen::graphics::WindowMode_Windowed;
        Pending_screen_info.set_screen_width(1280);
        Pending_screen_info.set_screen_height(720);
        Pending_screen_info.set_refresh_rate(60, 1);
        Pending_screen_info.set_vsync_enabled(false);
        Pending_screen_info.set_window_mode(keen::graphics::WindowMode_Windowed);
        display_info_get_settings_indices(Pending_screen_info, Screen_size_index, Refresh_rate_index);
    }
    Pending_lod = Default_lods[DISPLAY_DEFAULT_MED];
    Fov_value = 45;
    display_info_read_aa_modes();
    display_info_clamp_pending_aa();
    const int selected = ui_menu_get_selected_item(Menu_handle_doptions);
    set_detail_page(Cur_page_dxoptions);
    ui_menu_set_selected_item(Menu_handle_doptions, selected);
}

// 0x009024F0 (:1343)
// A new menu for the page (the old one goes after): the general rows, or the advanced ones (antialiasing unused, soft
// shadows hidden); the accept, cancel and defaults hints.
void set_detail_page(detail_option_pages page)
{
    const int old_menu = Menu_handle_doptions;
    Menu_handle_doptions = ui_menu_add(Doc_handle_doptions, "MENU_DISPLAY", display_selection_callback, display_hint_callback, "CM", true, -1, -1, false);
    if (page == DETAIL_OPTIONS_PAGE_GENERAL)
    {
        display_info_update_menu();
    }
    else if (page == DETAIL_OPTIONS_PAGE_ADVANCED)
    {
        // Original: the antialiasing mode's index is looked up and never used.
        ui_menu_add_char_list_item(Menu_handle_doptions, "MENU_SHADOWS", cDisplay_settings_OLMH, 4, option_change_occurred_doptions, Pending_lod.m_shadow_lod, true);
        ui_menu_add_bool_item(Menu_handle_doptions, "MENU_SOFT_SHADOWS", &Pending_lod.m_soft_shadows, option_change_occurred_doptions, true);
        ui_menu_add_bool_item(Menu_handle_doptions, "MENU_DEPTH_OF_FIELD", &Pending_lod.m_depth_of_field_enabled, option_change_occurred_doptions, true);
        ui_menu_add_bool_item(Menu_handle_doptions, "MENU_MOTION_BLUR", &Pending_lod.m_motion_blur_enabled, option_change_occurred_doptions, true);
        ui_menu_add_char_list_item(Menu_handle_doptions, "MENU_PARTICLE_DETAIL", cDisplay_settings_LMHV, 4, option_change_occurred_doptions, Pending_lod.m_particle_lod, true);
        ui_menu_add_bool_item(Menu_handle_doptions, "MENU_AMBIENT_OCCLUSION", &Pending_lod.m_ssao_enabled, option_change_occurred_doptions, true);
        ui_menu_add_bool_item(Menu_handle_doptions, "MENU_SUNSHAFTS", &Pending_lod.m_sun_shafts_enabled, option_change_occurred_doptions, true);
        ui_menu_add_char_list_item(Menu_handle_doptions, "MENU_TEXTURE_RESOLUTION", cDisplay_settings_LMH, 3, option_change_occurred_doptions, 2 - Pending_lod.m_new_data.m_texture_reduction, true);
        ui_menu_add_char_list_item(Menu_handle_doptions, "MENU_ANISOTROPIC_FILTERING", cDisplay_settings_OLMH, 4, option_change_occurred_doptions, Pending_lod.m_new_data.m_anisotropic_filtering, true);
        // Magic number: row 1, soft shadows.
        ui_menu_enable_menu_item(Menu_handle_doptions, 1, false);
        ui_menu_show_menu_item(Menu_handle_doptions, 1, false);
    }
    display_add_hint(HT_SELECTION, CBA_MENU_SELECT, "MENU_ACCEPT_WITH_BUTTON");
    display_add_hint(HT_ABORT, CBA_MENU_ABORT, "MENU_CANCEL_WITH_BUTTON");
    display_add_hint(HT_MISC, CBA_MENU_Y, "MENU_DEFAULT_SETTINGS");
    if (Cur_page_dxoptions != DETAIL_OPTIONS_PAGE_INVALID && old_menu != -1)
    {
        ui_menu_destroy(old_menu);
    }
    Cur_page_dxoptions = page;
}

// 0x0090F800 (:1475)
// The current screen, details, vsync and field of view as the old and pending settings, the presets, modes and
// antialiasing modes, the document (none: back out), a control id and an input id; the menu under it hidden.
void display_menu_enter(game_state prev_state)
{
    Assets_loaded_doptions = false;
    Fov_value = camera_view_get_fov_raw();
    Prev_fov_value = Fov_value;
    Pending_screen_info = *rl_render_lib::get_instance()->get_screen_info();
    Old_screen_info = Pending_screen_info;
    int rr_num;
    int rr_den;
    Pending_screen_info.get_refresh_rate(rr_num, rr_den);
    Pending_resolution.width = (unsigned int)Pending_screen_info.get_screen_width();
    Pending_resolution.height = (unsigned int)Pending_screen_info.get_screen_height();
    Pending_resolution.refreshRateNumerator = (unsigned int)rr_num;
    Pending_resolution.refreshRateDenominator = (unsigned int)rr_den;
    Old_lod = rl_render_lib::get_instance()->get_level_of_detail();
    Pending_lod = Old_lod;
    Pending_vsync_setting = rl_render_lib::get_instance()->get_vsync_enabled();
    game_options_load_from_cache();
    display_info_initialize_default_detail_settings();
    display_info_add_supported_resolutions();
    display_info_add_supported_aa_settings();
    display_info_get_current_resolution_settings();
    Doc_handle_doptions = ui_load_document(cDoc_name_doptions, DFIT_NONE, 0.0f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (Doc_handle_doptions == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        gameseq_pop_state();
        return;
    }
    Control_id_doptions = (int)ui_request_control_id();
    Input_id_doptions = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    if (!Active)
    {
        main_menu_show_menu(false);
        return;
    }
    ingame_options_show_menu(false);
}

// 0x008F3FA0 (:1528)
void display_menu_exit(game_state new_state)
{
    if (Message_box_handle_doptions != -1)
    {
        ui_hide_msgbox(Message_box_handle_doptions, true);
        Message_box_handle_doptions = -1;
    }
    if (changes_made())
    {
        game_options_load_from_cache();
    }
    ui_menu_destroy(Menu_handle_doptions);
    Menu_handle_doptions = -1;
    // Magic number: a 0.5 s fade out.
    ui_unload_document(Doc_handle_doptions, DFOT_NONE, 0.5f);
    Doc_handle_doptions = -1;
    ui_relinquish_input_id(Input_id_doptions);
    Input_id_doptions = -1;
    ui_relinquish_control_id((unsigned int)Control_id_doptions);
    Control_id_doptions = 0;
    Display_character = true;
    if (Active)
    {
        ingame_options_show_menu(true);
        return;
    }
    main_menu_show_menu(true);
}

// 0x00902AF0 (:1566)
// The hints (select, back, defaults), then the rows; a released click selects (on the arrows: left or right).
void display_menu_process_mouse_input()
{
    const char* names[3] = {kHints1, kHints2, kHints3};
    vint_object_base* objects[3] = {nullptr, nullptr, nullptr};
    vint_doc_info* info = ui_get_doc_info(Doc_handle_doptions);
    if (info != nullptr && info->m_doc != nullptr)
    {
        for (int i = 0; i < 3; i++)
        {
            vint_object_base* object = vint_object_base::find_object(names[i], info->m_doc);
            if (object == nullptr)
            {
                break;
            }
            objects[i] = object;
        }
    }
    const int hint = ui_mouse_process_clickables(objects, 3, true);
    if (hint > -1)
    {
        display_hint_callback((hint_type)hint);
        return;
    }
    const int index = ui_menu_process_mouseover(Doc_handle_doptions, Menu_handle_doptions, kRowGroup);
    if (index < 0 || !mouse_just_released(0))
    {
        return;
    }
    info = ui_get_doc_info(Doc_handle_doptions);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    bool left_arrow = false;
    bool right_arrow = false;
    if (ui_vint_object_visible(kDetailArrows, info->m_doc))
    {
        left_arrow = ui_is_mouse_over_vint_object(kLeftArrow, info->m_doc);
        if (!left_arrow)
        {
            right_arrow = ui_is_mouse_over_vint_object(kRightArrow, info->m_doc);
        }
    }
    ui_menu_selection_clicked(Menu_handle_doptions, index, left_arrow, right_arrow);
}

// The input id that may read the buttons (ui_input_id_has_focus, inlined): -1 while nothing is locked, else the lock
// or the top request.
static bool display_input_has_focus(int input_id)
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

// Magic numbers: buttons 0x11 (L2, the previous page) and 0x13 (R2, the next page).
static bool display_menu_page_button(int button)
{
    return Button_info.binfo[button].m_button_input > 0.0f && !Button_info.binfo[button].m_button_wait_for_up;
}

// 0x00902C40 (:1618)
// With this menu's input on top: L2/R2 switch pages (wrapping); then the page tabs, then the mouse.
void display_menu_process_input()
{
    detail_option_pages page = DETAIL_OPTIONS_PAGE_INVALID;
    const bool has_focus = display_input_has_focus(Input_id_doptions);
    if (has_focus && display_menu_page_button(0x11))
    {
        audiolib_cue_play(Audio_info[9], nullptr, nullptr);
        page = (int)Cur_page_dxoptions < 1 ? DETAIL_OPTIONS_PAGE_ADVANCED : (detail_option_pages)(Cur_page_dxoptions - 1);
    }
    else if (has_focus && display_menu_page_button(0x13))
    {
        // Magic number: Audio_info[10], the next page sound.
        audiolib_cue_play(Audio_info[10], nullptr, nullptr);
        page = (int)Cur_page_dxoptions < 1 ? (detail_option_pages)(Cur_page_dxoptions + 1) : DETAIL_OPTIONS_PAGE_GENERAL;
    }
    if (page != DETAIL_OPTIONS_PAGE_INVALID)
    {
        set_detail_page(page);
    }
    if (display_options_process_mouse_tab_input())
    {
        return;
    }
    display_menu_process_mouse_input();
}

// 0x00915F60 (:1646)
// With a box up only the document runs; a save in progress: leave once it is done (and succeeded); the page once the
// document is loaded, then the input and the tab data.
void display_menu_process()
{
    if (Utility_doc_handle != -1 && ui_is_msgbox_active(-2))
    {
        if (Assets_loaded_doptions)
        {
            ui_add_for_processing(Doc_handle_doptions);
        }
        ui_process((unsigned int)Control_id_doptions, false);
        return;
    }
    if (Waiting_on_save_completion_doptions)
    {
        if (game_options_save_in_progress())
        {
            return;
        }
        Waiting_on_save_completion_doptions = false;
        if (player_profile_last_save_successful(Player_controller_index != 0xFFFFFFFFu ? (int)Player_controller_index : 0) && gameseq_get_state() == GS_MAINMENU_OPTIONS_DISPLAY)
        {
            gameseq_pop_state();
        }
        return;
    }
    if (!Assets_loaded_doptions)
    {
        switch (ui_get_doc_asset_state(Doc_handle_doptions))
        {
        case ST_LOADING:
            break;
        case ST_LOADED:
            Assets_loaded_doptions = true;
            break;
        case ST_UNLOADED:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            [[fallthrough]];
        case ST_ERROR:
            if (gameseq_get_state() == GS_MAINMENU_OPTIONS_DISPLAY)
            {
                gameseq_pop_state();
            }
            break;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
        if (!Assets_loaded_doptions)
        {
            ui_process((unsigned int)Control_id_doptions, false);
            return;
        }
        detail_options_create_tab_data_item();
        set_detail_page(DETAIL_OPTIONS_PAGE_GENERAL);
    }
    if (Utility_doc_handle == -1 || !ui_is_msgbox_active(-2))
    {
        display_menu_process_input();
    }
    display_options_process_tab_data();
    ui_add_for_processing(Doc_handle_doptions);
    ui_process((unsigned int)Control_id_doptions, false);
}

// 0x008AFC00 (:1712)
// A changed vsync is applied here (the render side).
void display_menu_handshake(rl_renderer*)
{
    if (Vsync_needs_update)
    {
        rl_render_lib::get_instance()->set_vsync_enabled(Pending_vsync_setting);
        Vsync_needs_update = false;
    }
}

// 0x00902D70 (:1721)
void display_menu_render(rl_renderer*)
{
    if (Assets_loaded_doptions)
    {
        ui_add_for_rendering(Doc_handle_doptions);
    }
    ui_render((unsigned int)Control_id_doptions, false);
}

// 0x00892850 (:1741)
void display_parse_presets()
{
}

// 0x00892860 (:1745)
bool display_has_been_inited()
{
    return false;
}

// 0x00892870 (:1750)
void display_set_user_settings(int custom_gamma, int custom_brightness, int custom_contrast, int preset_choice)
{
}

// 0x00892880 (:1758)
void display_get_user_settings(int& custom_gamma, int& custom_brightness, int& custom_contrast, int& preset_choice)
{
}

// 0x008AFCF0 (:1897)
void resolution_change_enter(game_state prev_state)
{
    Res_change_state = RC_STAGE1;
    Res_change_message_box_handle = -1;
    Resolution_reverting = false;
    show_image();
}

// 0x008C4360 (:1906)
void resolution_change_exit(game_state new_state)
{
    cleanup_image();
    Display_background_image = false;
    resolution_change_release_ids();
}

// 0x009160E0 (:2045)
// The keep box up: its text counts down, run out it reverts; otherwise the stages.
void resolution_change_process()
{
    if (Utility_doc_handle != -1 && ui_is_msgbox_active(-2) && !Resolution_reverting)
    {
        if (!Resolution_msgbox_needs_load)
        {
            char encoded[384];
            // The seconds left, rounded up.
            resolution_change_confirm_text(encoded, Revert_timer.time_until() / 1000 + 1);
            ui_update_msgbox_caption(Res_change_message_box_handle, encoded, false);
            if (Revert_timer.elapsed())
            {
                ui_hide_msgbox(Res_change_message_box_handle, false);
                resolution_change_trigger_revert();
            }
            ui_process((unsigned int)Res_change_control_id, false);
            return;
        }
        Resolution_msgbox_needs_load = false;
    }
    process_state();
}

// 0x008928B0 (:2086)
void resolution_change_handshake(rl_renderer*)
{
}

// 0x009030C0 (:2090)
void resolution_change_render(rl_renderer*)
{
    ui_render((unsigned int)Res_change_control_id, false);
}

// 0x008AFD10 (:2095)
// The render thread's mode switch (windowed: the window size; else fullscreen or borderless), then the screen info.
void resolution_change_render_thread()
{
    if (!presto_chango)
    {
        return;
    }
    presto_chango = false;
    int rr_num;
    int rr_den;
    Pending_screen_info.get_refresh_rate(rr_num, rr_den);
    if (Pending_screen_info.get_window_mode() == keen::graphics::WindowMode_Windowed)
    {
        keen::graphics::switchToWindowedMode(rl_get_graphics_system(), nullptr);
        keen::graphics::setWindowSize(rl_get_graphics_system(), (unsigned int)Pending_screen_info.get_screen_width(), (unsigned int)Pending_screen_info.get_screen_height(), nullptr);
    }
    else
    {
        keen::graphics::switchToFullScreenMode(rl_get_graphics_system(), nullptr, (unsigned int)Pending_screen_info.get_screen_width(), (unsigned int)Pending_screen_info.get_screen_height(), (unsigned int)rr_num, (unsigned int)rr_den, Pending_screen_info.get_window_mode());
    }
    rl_render_lib::get_instance()->update_screen_info(rr_num, rr_den);
}

#endif // OPENRFG_UI
