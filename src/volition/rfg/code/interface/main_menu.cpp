// Original: volition/rfg/code/interface/main_menu.cpp
//
// The main menu state (GS_MAINMENU, UI.md UI4): the title screen (any input catches the local player, then the
// profile and save data load behind a loading box), the menu of rfg_ui_main_menu (new game, load game, wrecking crew,
// online, bonus campaign, system link, options, back to the title screen, exit), its delayed selections (the warning,
// difficulty and start weapon boxes before a new game), the background video and the menu character.
// UI.md 2.7 item 1: the single player entries (new game, load game, the bonus campaign), the online entry and options
// are logged // STUBs that leave the menu as it was (UI6, UI11, UI12 port them). Wrecking crew is ported (wrecking.md
// WC2).

#include "volition/rfg/code/interface/main_menu.h"

#include "game/build/win32/steam/master/generated_sources/version.hpp"
#include "keen/base/crc32.hpp"
#include "keen/base/language.hpp"
#include "keen/base/string.hpp"
#include "keen/base/stringencoding.hpp"
#include "keen/base/trace.hpp"
#include "keen/input/inputevents.hpp"
#include "keen/input/inputsystem.hpp"
#include "unit4/gamesession/localgamesession.hpp"
#include "unit4/gamesession/remotegamesession.hpp"
#include "library/modules/runtime/systemservices/sources/steam/systemservices_steam.hpp"
#include "unit4/systemservices/systemservices.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/objects/vint_element_base.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_project.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/gr/gr_font.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/rfg/code/camera/camera_view.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/cutscene/video_cutscene.h"
#include "volition/rfg/code/video_player/video_player.h"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_options.h"
#include "volition/rfg/code/game_render/fullscreen_effects/fb_accum_blur.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/game_save.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/gamewide.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/hud/fog_of_war.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu_idle.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/option_menus/ingame_options.h"
#include "volition/rfg/code/interface/option_menus/option_menu_system.h"
#include "volition/rfg/code/interface/option_menus/player_death_options.h"
#include "volition/rfg/code/interface/ui_character.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_document_menu_wrecking_crew.h"
#include "volition/rfg/code/interface/ui_document_save_load.h"
#include "volition/rfg/code/interface/ui_document_view_dlc.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/rfg/code/util/special_allocators.h"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/input/key.h"
#include "volition/vlib/code/input/input_internal.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/os.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/string_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- file-private types (rfg.pdb)
enum main_menu_selection_states
{
    MMSS_NONE = -1,
    MMSS_CHECK_PROFILE = 0,
    MMSS_CHECK_STORAGE_DEVICE = 1,
    MMSS_CHECK_MAX_SAVES = 2,
    MMSS_CHECK_FREE_SPACE = 3,
    MMSS_CHECK_AUTOSAVE = 4,
    MMSS_CHECK_NETWORK_CONNECTION = 5,
    MMSS_CHECK_INTERNET_CONNECTION = 6,
    MMSS_CHECK_INTERNET_PERMISSION = 7,
    MMSS_CHECK_NAT = 8,
    MMSS_CHECK_VOICE_CHAT = 9,
    MMSS_SELECT_DIFFICULTY = 10,
    MMSS_SELECT_GOLDEN_HAMMER = 11,
    MMSS_TRIGGER_SELECTION = 12,
    MMSS_NUM_STATES = 13,
};

// menu_option_types is in main_menu.h since UI3 (the document manager reads Main_menu_delayed_selection).

// ---- file-static globals
// bool s_localization_is_loading (0x01663023) is used here too; placed in localization_shared.h.
static bool First_time_loaded = true; // 0x0166316E
// Display_character is a static of rfg.exe's interface unity (rfg_lua_actions.cpp, external since UI4).
extern bool g_openrfgUseSteam; // application.cpp, deviation 1
extern bool Display_character; // 0x0166316F
static const char* s_groupIdGameplay = {"gameplay"}; // 0x01663180
static const char* cBinkname = {"bg_video"}; // 0x01663198
static const char* cBinkfilename = {"RFG_MainMenuBG.bik"}; // 0x0166319C
static const char* cProp_screen_size_main = {"screen_size"}; // 0x016631A0
static const char* cProp_video_id = {"vid_id_handle"}; // 0x016631A4
static int Menu_handle = -1; // 0x016631A8
extern int Doc_handle_main; // 0x016631AC, ui_menu.cpp
static unsigned int Main_menu_controller_select_handle = 0xFFFFFFFF; // 0x016631B0
int Control_id_main = -1; // 0x016631B4 (external since UI6: options_main.cpp)
static int Input_id_main = -1; // 0x016631B8
static int MM_delayed_process_input_id = -1; // 0x016631BC
static int Message_box_handle_main = -1; // 0x016631C0
static bool Do_character_handshake = true; // 0x016631C4
static bool Forced_character_invisible_last_frame = true; // 0x016631C5
static bool Main_menu_load_ui_character = true; // 0x016631C6
menu_option_types Main_menu_delayed_selection = MENU_OPTION_NONE; // 0x016631C8, initial data ffffffff
static main_menu_selection_states Main_menu_delayed_selection_state = MMSS_NONE; // 0x016631CC, initial data ffffffff
static const char* cMenu_group_name = {"text_group"}; // 0x016631D0
static const char* cMenu_item_name = {"text_clone"}; // 0x016631D4
static keen::LanguageId Main_menu_current_language; // 0x016631D8
static keen::ControllerType Main_menu_controller_type; // 0x016631DC
// The menu character's statics (ui_character.cpp; statics of rfg.exe's interface unity, external since UI4).
extern int Doc_handle_uichar; // 0x016642A8
extern vector Obj_pos; // 0x01666F9C
extern vector Off_screen_pos; // 0x01666FA8
extern bool Active_uichar; // 0x02C07FDF
extern model_info Player_info; // 0x02C3AA00
extern model_info Coat_info; // 0x02C3CA70
extern model_info Head_info; // 0x02C54118
extern static_mesh_info Weapon_info; // 0x02C54178
void ui_character_shutdown_internal(); // 0x008B0670 (ui_character.cpp:928)
void ui_character_init_resources(); // 0x008B0710 (ui_character.cpp:1095)
void ui_character_fade_main_menu(int dhandle); // 0x008D7100 (ui_character.cpp:1168)
extern timestamp Fx_delay_timer; // 0x01666FD8, ui_menu.cpp
// The interface unity's other shared statics (loading.h): Loading_screen_handle, Current_input_lock_*,
// Input_enabled_uimen, cBlank_string, Utility_doc_handle, Controller_slots, Input_types, Fade_out, Fade_ref,
// Loading_screen_ref_cnt; Menus (ui_menu.cpp), Message_box_info_set (ui_message_box.h).
extern farray<ui_menu*,8> Menus; // 0x01667698, ui_menu.cpp
extern int cur_id; // 0x01668594, loading.cpp
bink_data Bink_info_main; // 0x01666148 (a file static in the binary; external since UI5, the multiplayer menus)
// vector2i Screen_size (0x0166716C) is used here too; placed in rfg_lua_actions.cpp.
extern vector2i Screen_size; // 0x0166716C
static rfg_tracked_chunk_id Main_menu_last_chunk_id; // 0x02BF2204
static const char* Main_menu_last_chunk_error_locatext; // 0x02C0008C
static game_state Pending_state; // 0x02C00DB8
static bool Interface_debug_active; // 0x02C033E4
// bool s_localization_is_audio_loading (0x02C03509) is used here too (read through localization_is_audio_loading()); placed in localization_shared.cpp.
static bool Main_menu_active; // 0x02C03519
static bool Main_menu_showing_profile_load; // 0x02C0351A
static bool Was_in_attract; // 0x02C0351B
static int Selected_menu_option; // 0x02C0351C
static int MM_script; // 0x02C03530
static float Idle_time; // 0x02C03534
static int Num_times_idle; // 0x02C03538
static bool Assets_loaded_main; // 0x02C0353C
static bool Display_controller_select; // 0x02C0353E
static bool Main_menu_show_menu_next_frame; // 0x02C0353F
static bool Main_menu_delay_selection_a_frame; // 0x02C03566
static bool Main_menu_accepted_no_profile_warning; // 0x02C03567
static bool Main_menu_prompted_for_device; // 0x02C03568
static bool Main_menu_accepted_no_device_warning; // 0x02C03569
static bool Main_menu_accepted_no_space_warning; // 0x02C0356A
static bool Main_menu_accepted_max_save_warning; // 0x02C0356B
static bool Main_menu_accepted_autosave_warning; // 0x02C0356C
static bool Main_menu_accepted_voice_chat_warning; // 0x02C0356D
static bool Main_menu_accepted_nat_warning; // 0x02C0356E
static bool Main_menu_selected_difficulty; // 0x02C0356F
static bool Main_menu_selected_golden_hammer; // 0x02C03570
static bool Main_menu_auto_catch_done; // 0x02C03571
static bool Main_menu_waiting_for_device; // 0x02C03572
static bool Main_menu_auto_catch_started; // 0x02C03574
static bool Pending_state_transfer; // 0x02C03575
static bool Main_menu_waiting_for_options_load; // 0x02C03576
static bool Main_menu_msgbox_for_chunks; // 0x02C03577
static bool Main_menu_loading_options; // 0x02C03578
// Read (main_menu_process, main_menu_handle_selection) and never written in rfg.exe.
static bool Disable_new_game; // 0x02C03579
// The Steam save import's state (game_save_generic.cpp, not ported; the stand-ins hold none).
extern unsigned int Num_imported_save_file_infos; // 0x025338F4
extern bool Has_imported_any_save_data; // 0x025338F3
// NVIDIA Highlights (rfg.cpp, not declared in a recovered header).
void nvidia_highlights_open_summary(const char* groupId); // 0x005D4230
bool nvidia_highlights_is_enabled(); // 0x005D13B0
unsigned int nvidia_highlights_get_num_highlights(); // 0x005D13F0
save_load_ui_type Save_load_ui_type; // 0x02C0812C (external since UI6: ingame_options.cpp, player_death_options.cpp)

// Test seams (UI.md 2.7 items 2 and 3, deviation 4; off by default): g_openrfgMainMenuNoTitle catches the local
// player at once (--notitle, implied by --host/--join); g_openrfgMainMenuProcess runs at the end of each main menu
// frame once the menu exists (OpenRFG presses the command line's host or join through it).
bool g_openrfgMainMenuNoTitle = false;
void (*g_openrfgMainMenuProcess)() = nullptr;

// ---- internal functions
static void main_menu_show_controller_select(bool show); // 0x00889910 (:3401)

// 0x00889550 (:178)
void bink_data::reset()
{
    name[0] = '\0';
    handle = 0;
    cur_state = BINK_VIDEO_TRIGGER_PLAY;
    alpha = 1.0f;
    position.x = 0;
    position.y = 0;
    size.x = 0;
    size.y = 0;
    total_frames = 0;
    curr_frame = 0;
    bink_pool = nullptr;
}

// 0x008895A0 (:193)
bink_data::bink_data()
{
    reset();
}

// 0x008895F0 (:343)
// LTCG: filename in ESI. The console's level loads (single player).
static void load_level_internal(const char* filename, bool new_game, bool single_zone)
{
    char zone_name[64];
    char packfile_name[64];
    cf_add_ext(zone_name, sizeof(zone_name), filename, ".rfgzone_pc", false);
    cf_add_ext(packfile_name, sizeof(packfile_name), filename, "vpp_pc", false);
    if (!world_level_exists(filename, false))
    {
        dc_printf("Could not find (%s) to open.\n", filename);
        return;
    }
    World.set_pending_territory_load(zone_name, new_game, single_zone, false, -1, false);
    if (Player_controller_index == 0xFFFFFFFF)
    {
        multi_core_platform_set_active_controller(0);
    }
}

// 0x008896B0 (:375)
// LTCG: c in AL.
static bool is_digit(char c)
{
    return (unsigned char)(c - '0') <= 9;
}

// 0x008896C0 (:381)
// "level <name>"; a four-digit name is a terrain zone ("terr01_xx_<digits>").
void unused_dcf_level()
{
    if (Console_command == 0)
    {
        return;
    }
    if (Multiplayer)
    {
        console_printf("Can't use this command in Multiplayer.");
        return;
    }
    console_get_arg(2, 0);
    console_printf("Turning off cheats");
    stats_turn_off_all_cheats();
    char name[16];
    const char* level = Console_arg;
    if (strlen(Console_arg) == 4 && is_digit(Console_arg[0]) && is_digit(Console_arg[1]) && is_digit(Console_arg[2]) && is_digit(Console_arg[3]))
    {
        _snprintf_safe(name, sizeof(name), "terr01_%c%c_%c%c", Console_arg[0], Console_arg[1], Console_arg[2], Console_arg[3]);
        level = name;
    }
    load_level_internal(level, true, false);
}

// 0x008897D0 (:416)
void unused_dcf_level_single()
{
    if (Console_command == 0)
    {
        return;
    }
    if (Multiplayer)
    {
        console_printf("Can't use this command in Multiplayer.");
        return;
    }
    console_get_arg(2, 0);
    load_level_internal(Console_arg, true, true);
}

// 0x008CECC0 (:477)
// The background video's screen rectangle and alpha from the document's bg_video element.
static void mm_get_bink_screen_info()
{
    vint_doc_info* info = ui_get_doc_info(Doc_handle_main);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    vint_element_base* element = (vint_element_base*)vint_object_base::find_object(cBinkname, info->m_doc);
    if (element == nullptr)
    {
        return;
    }
    vector2 anchor;
    element->get_global_anchor(anchor);
    vint_variant size_value;
    element->get_property_value(cProp_screen_size_main, size_value, 0);
    vector2 size(0.0f, 0.0f);
    if (size_value.get_type() == VINT_PROP_TYPE_VECTOR2F)
    {
        size = (vector2)size_value;
    }
    vint_res_offset offset_x;
    vint_res_offset offset_y;
    vint_res_scale scale_x;
    vint_res_scale scale_y;
    element->get_res_offsets_and_scales_inherited(offset_x, offset_y, scale_x, scale_y);
    vector2 scale;
    vector2 offset;
    scale = vint_project_get_scale(scale_x, scale_y);
    offset = vint_project_get_offset(offset_x, offset_y);
    Bink_info_main.position.x = fl_round(scale.x * anchor.x + offset.x);
    Bink_info_main.position.y = fl_round(scale.y * anchor.y + offset.y);
    Bink_info_main.size.x = fl_round(size.x * scale.x);
    Bink_info_main.size.y = fl_round(size.y * scale.y);
    vint_variant alpha;
    element->get_property_value("alpha", alpha, 0);
    Bink_info_main.alpha = (float)alpha;
}

// 0x00889810 (:558)
static void mm_stop_bink_video()
{
    if (Bink_info_main.handle != 0)
    {
        video_player_stop(Bink_info_main.handle);
        Bink_info_main.cur_state = BINK_VIDEO_STOPPING;
    }
}

// 0x00889830 (:566)
static void mm_pause_bink_video(bool pause)
{
    if (pause)
    {
        video_player_pause(Bink_info_main.handle);
        return;
    }
    video_player_unpause(Bink_info_main.handle);
}

// 0x00889860 (:575)
// The video ended: its pool goes back to the pregame split.
static void mm_stop_bink_video_callback(int)
{
    Bink_info_main.handle = 0;
    Bink_info_main.bink_pool->clear();
    salloc_pregame_split_unlock((rfg_split_mempool*)Bink_info_main.bink_pool);
    Bink_info_main.bink_pool = nullptr;
    if (Bink_info_main.cur_state == BINK_VIDEO_PLAYING || Bink_info_main.cur_state == BINK_VIDEO_STOPPING)
    {
        Bink_info_main.cur_state = BINK_VIDEO_TRIGGER_PLAY;
    }
}

// 0x008CF160 (:592)
// The background video (looping, 12 MB of the pregame split); its handle goes to the bg_video element. Original bug
// kept: a failed play keeps the pregame split locked (the stop callback that unlocks it never comes).
void mm_process_bink_video()
{
    if (Bink_info_main.cur_state == BINK_VIDEO_TRIGGER_PLAY)
    {
        if (Bink_info_main.handle != 0)
        {
            video_player_stop(Bink_info_main.handle);
            Bink_info_main.cur_state = BINK_VIDEO_STOPPING;
            return;
        }
        if (Bink_info_main.bink_pool != nullptr)
        {
            return;
        }
        // Magic numbers: a 12 MB pool, 4 MB of it for the video's read buffer.
        Bink_info_main.bink_pool = (mempool_base*)salloc_pregame_split_lock(0xC00000, 0);
        Bink_info_main.handle = video_player_play(cBinkfilename, VPM_LOOPING, Bink_info_main.position.x, Bink_info_main.position.y, Bink_info_main.size.x, Bink_info_main.size.y, 0.0f, true, mm_stop_bink_video_callback, Bink_info_main.bink_pool, Bink_info_main.bink_pool, Bink_info_main.bink_pool, 0x400000, false, false);
        if (Bink_info_main.handle == 0)
        {
            Bink_info_main.cur_state = BINK_VIDEO_ERROR;
            return;
        }
        Bink_info_main.total_frames = video_player_get_total_frames(Bink_info_main.handle);
        Bink_info_main.cur_state = BINK_VIDEO_PLAYING;
        vint_document* document = ui_get_vint_document(Doc_handle_main);
        if (document != nullptr)
        {
            vint_object_base* element = vint_object_base::find_object(cBinkname, document);
            if (element == nullptr)
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                return;
            }
            vint_variant handle;
            handle = Bink_info_main.handle;
            element->set_property_value(cProp_video_id, handle, 0);
        }
    }
    else if (Bink_info_main.cur_state == BINK_VIDEO_PLAYING)
    {
        if (Bink_info_main.handle == 0)
        {
            Bink_info_main.cur_state = BINK_VIDEO_TRIGGER_PLAY;
            return;
        }
        mm_get_bink_screen_info();
        video_player_resize(Bink_info_main.handle, Bink_info_main.position.x, Bink_info_main.position.y, Bink_info_main.size.x, Bink_info_main.size.y);
        video_player_set_alpha(Bink_info_main.handle, 1.0f);
    }
}

// 0x008898B0 (:674)
static bool mm_can_start_live()
{
    return !Modified_table_data;
}

// 0x008F1850 (:686)
// The selection is dropped and the menu comes back the next frame (the profile's loading box goes).
static void main_menu_clear_delayed_selection()
{
    Main_menu_delayed_selection = MENU_OPTION_NONE;
    Main_menu_delayed_selection_state = MMSS_NONE;
    Main_menu_show_menu_next_frame = true;
    if (Main_menu_showing_profile_load)
    {
        // ui_hide_loading_screen, inlined.
        const int handle = Loading_screen_handle;
        if (Loading_screen_ref_cnt > 0)
        {
            Loading_screen_ref_cnt--;
            if (Loading_screen_ref_cnt < 1)
            {
                if (Loading_screen_handle != -1 && ui_is_msgbox_active(Loading_screen_handle))
                {
                    ui_hide_msgbox(handle, false);
                }
                Loading_screen_handle = -1;
            }
        }
        Main_menu_showing_profile_load = false;
    }
}

// The choices that cancel a box (abort, forced exit); anything else is a critical failure.
static bool is_cancel_choice(msgbox_choice choice)
{
    return choice == MBC_ABORT || choice == MBC_FORCED_EXIT;
}

// 0x008F18D0 (:883)
static bool main_menu_autosave_cb(int mbox_handle, int, msgbox_choice choice)
{
    if (Message_box_handle_main != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        Main_menu_accepted_autosave_warning = true;
    }
    else if (is_cancel_choice(choice))
    {
        main_menu_clear_delayed_selection();
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    Message_box_handle_main = -1;
    return true;
}

// 0x008DE5B0 (:954)
// The exit box: accept shuts the game down.
static bool exit_game_cb(int mbox_handle, int, msgbox_choice choice)
{
    main_menu_show_menu(true);
    if (Message_box_handle_main != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        gameseq_set_state(GS_SHUTDOWN, true);
    }
    else if (!is_cancel_choice(choice))
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    Message_box_handle_main = -1;
    return true;
}

// 0x008F1940 (:1006)
static bool main_menu_difficulty_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Message_box_handle_main != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        difficulty_set_level((difficulty_levels)selection_index);
        Main_menu_selected_difficulty = true;
    }
    else if (is_cancel_choice(choice))
    {
        main_menu_clear_delayed_selection();
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    Message_box_handle_main = -1;
    ui_wait_for_button_ups();
    return true;
}

// 0x008F19C0 (:1034)
static bool main_menu_golden_hammer_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Message_box_handle_main != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        Wants_to_start_game_with_golden_hammer = selection_index == 0;
        Main_menu_selected_golden_hammer = true;
    }
    else if (is_cancel_choice(choice))
    {
        main_menu_clear_delayed_selection();
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    Message_box_handle_main = -1;
    ui_wait_for_button_ups();
    return true;
}

// 0x008898C0 (:1060)
void main_menu_clear_auto_catch_done()
{
    Main_menu_auto_catch_started = false;
    Main_menu_auto_catch_done = false;
}

// 0x008ABCD0 (:1066)
// Back to the title screen: the save data and the local player go and the controllers wait for any button.
void main_menu_start_auto_catch(bool)
{
    Display_controller_select = true;
    if (Main_menu_auto_catch_started)
    {
        return;
    }
    if (keen::input::getControllerAutoCatch(Keen_input_system, nullptr) != keen::InputSystemControllerAutoCatchType_Disabled)
    {
        return;
    }
    Main_menu_loading_options = false;
    game_save_clear_save_data();
    // Magic number: no keen controller.
    Cached_input_states[Current_cached_input_state].controllers[0].keen_controller_index = 0xFFFFFFFF;
    Main_menu_auto_catch_done = false;
    Main_menu_auto_catch_started = true;
    Cached_input_states[Next_cached_input_state].controllers[0].keen_controller_index = 0xFFFFFFFF;
    rfg_get_local_game_session()->removeLocalPlayer(nullptr);
    keen::input::clearControllerPlayerId(Keen_input_system, nullptr);
    keen::input::setControllerAutoCatch(Keen_input_system, nullptr, keen::InputSystemControllerAutoCatchType_AnyButton);
}

// 0x008F1A40 (:1136)
static bool main_menu_chunks_cb(int mbox_handle, int, msgbox_choice choice)
{
    if (Message_box_handle_main != mbox_handle)
    {
        return false;
    }
    if ((int)choice < 0 || (int)choice > 2)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    Message_box_handle_main = -1;
    Main_menu_msgbox_for_chunks = false;
    main_menu_clear_delayed_selection();
    ui_wait_for_button_ups();
    return true;
}

// 0x008DE620 (:1185)
// The Steam save import box: accept opens the import list (the save/load screen, single player).
static bool main_menu_save_data_import_cb(int mbox_handle, int, msgbox_choice choice)
{
    if (Message_box_handle_main != mbox_handle)
    {
        return false;
    }
    if (MM_delayed_process_input_id == -1)
    {
        MM_delayed_process_input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    }
    if (choice == MBC_OK)
    {
        World.dlc_bundle_id = -1;
        Save_load_ui_type = SLUT_IMPORT_SELECT;
        gameseq_push_state(GS_SAVE_LOAD_SCREEN, true, false);
        main_menu_show_menu(false);
    }
    else if (!is_cancel_choice(choice))
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    Message_box_handle_main = -1;
    Has_imported_any_save_data = true;
    ui_wait_for_button_ups();
    return true;
}

// 0x008F7450 (:1218)
// A data chunk not yet installed (a Steam background download) gets priority and a box with its progress, updated
// while it is up. True when the chunk is there.
bool main_menu_check_chunk_ready(rfg_tracked_chunk_id chunk_id, const char* error_locatext)
{
    float progress = 1.0f;
    unit4::LocalGameSession* session = rfg_get_local_game_session();
    if (session == nullptr)
    {
        return false;
    }
    // LocalGameSession's private m_pSystemServices (0x00).
    unit4::SystemServices* services = *(unit4::SystemServices**)session;
    if (!unit4::systemservices::isDataChunkAvailable(services, chunk_id, &progress))
    {
        unit4::systemservices::prioritizeDataChunk(services, chunk_id);
        const wchar_t* format = localization_get_display_string_req(error_locatext, nullptr);
        wchar_t text[256];
        // Original: the localized text is the format, with no arguments.
        swprintf(text, 256, format);
        char caption[512];
        keen::convertUTF16ToUTF8(caption, sizeof(caption), (const uint16*)text);
        if (Message_box_handle_main != -1)
        {
            if (Main_menu_msgbox_for_chunks)
            {
                ui_update_msgbox_caption(Message_box_handle_main, caption, false);
                Main_menu_last_chunk_id = chunk_id;
                Main_menu_last_chunk_error_locatext = error_locatext;
                Main_menu_msgbox_for_chunks = true;
                return false;
            }
            ui_hide_msgbox(Message_box_handle_main, true);
            Message_box_handle_main = -1;
        }
        Message_box_handle_main = ui_add_msgbox(MBT_OK, "LOCAL_GAME_SESSION_ERROR_TITLE", caption, main_menu_chunks_cb, false, false, nullptr, nullptr, false);
        Main_menu_last_chunk_id = chunk_id;
        Main_menu_last_chunk_error_locatext = error_locatext;
        Main_menu_msgbox_for_chunks = true;
        return false;
    }
    if (Message_box_handle_main != -1 && Main_menu_msgbox_for_chunks)
    {
        ui_hide_msgbox(Message_box_handle_main, true);
        Message_box_handle_main = -1;
        Main_menu_msgbox_for_chunks = false;
    }
    return true;
}

// The delayed selection's box of a check: a box of another check goes first; only one box at a time.
static bool delayed_box_needed(main_menu_selection_states state, int& box)
{
    if (Main_menu_delayed_selection_state == MMSS_NONE || Main_menu_delayed_selection_state == state)
    {
        if (box != -1)
        {
            return false;
        }
    }
    else if (box != -1)
    {
        ui_hide_msgbox(box, false);
        box = -1;
        Message_box_handle_main = -1;
    }
    return !ui_is_msgbox_active(-2);
}

// 0x008F75F0 (:1273)
// The selection's checks run in order (a box stops them for this frame: free space, autosave, difficulty and start
// weapon for a new game), then the selection is made. The profile, storage, network and voice chat checks are empty
// on PC.
static void main_menu_process_delayed_selection()
{
    if (Main_menu_delay_selection_a_frame)
    {
        Main_menu_delay_selection_a_frame = false;
        return;
    }
    if (Main_menu_delayed_selection == MENU_OPTION_NONE)
    {
        if (MM_delayed_process_input_id != -1)
        {
            ui_relinquish_input_id(MM_delayed_process_input_id);
            MM_delayed_process_input_id = -1;
        }
        return;
    }
    if (multi_core_platform_ui_is_active() || gameseq_state_is_active(GS_SONY_OPTIONS) || player_profile_busy(Player_controller_index != 0xFFFFFFFF ? Player_controller_index : 0))
    {
        return;
    }
    int box = Message_box_handle_main;
    for (int state = MMSS_CHECK_PROFILE; state <= MMSS_TRIGGER_SELECTION; state++)
    {
        bool stop = false;
        // A finished check's box goes.
        auto close_box_of = [&](main_menu_selection_states check) {
            if (Main_menu_delayed_selection_state == check && box != -1)
            {
                ui_hide_msgbox(box, false);
                Message_box_handle_main = -1;
                Main_menu_delayed_selection_state = MMSS_NONE;
                box = -1;
            }
        };
        switch (state)
        {
        case MMSS_CHECK_FREE_SPACE:
            if (!Main_menu_accepted_no_profile_warning && !Main_menu_accepted_no_device_warning && Main_menu_delayed_selection == MENU_OPTION_NEW_GAME)
            {
                if (Main_menu_delayed_selection_state == MMSS_CHECK_FREE_SPACE && box != -1)
                {
                    close_box_of(MMSS_CHECK_FREE_SPACE);
                    Main_menu_accepted_no_device_warning = false;
                    Main_menu_accepted_no_space_warning = false;
                    Main_menu_accepted_autosave_warning = false;
                    Main_menu_accepted_max_save_warning = false;
                    Main_menu_accepted_voice_chat_warning = false;
                }
            }
            break;
        case MMSS_CHECK_AUTOSAVE:
            if (!Main_menu_accepted_no_profile_warning && !Main_menu_accepted_no_device_warning && !Main_menu_accepted_no_space_warning && Main_menu_delayed_selection == MENU_OPTION_NEW_GAME)
            {
                if (!game_save_is_autosave_present())
                {
                    // Original: the free space check's warnings are not reset here.
                    close_box_of(MMSS_CHECK_AUTOSAVE);
                    break;
                }
                if (Main_menu_accepted_autosave_warning)
                {
                    break;
                }
                if (delayed_box_needed(MMSS_CHECK_AUTOSAVE, box))
                {
                    Message_box_handle_main = ui_add_msgbox(MBT_ACCEPT_CANCEL_SELECT, "MENU_MBOX_AUTOSAVE_WARNING_TITLE", "MENU_MBOX_AUTOSAVE_WARNING", main_menu_autosave_cb, false, false, nullptr, nullptr, false);
                    box = Message_box_handle_main;
                }
                stop = true;
                Main_menu_delayed_selection_state = MMSS_CHECK_AUTOSAVE;
            }
            break;
        case MMSS_SELECT_DIFFICULTY:
            if (Main_menu_delayed_selection != MENU_OPTION_NEW_GAME)
            {
                break;
            }
            if (Main_menu_selected_difficulty)
            {
                close_box_of(MMSS_SELECT_DIFFICULTY);
                break;
            }
            if (delayed_box_needed(MMSS_SELECT_DIFFICULTY, box))
            {
                Message_box_handle_main = ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_DIFFICULTY", "", main_menu_difficulty_cb, false, false, nullptr, nullptr, false);
                ui_msgbox_add_caption_item(Message_box_handle_main, "MENU_DIFFICULTY_CASUAL", false, false);
                ui_msgbox_add_caption_item(Message_box_handle_main, "MENU_DIFFICULTY_NORMAL", false, false);
                ui_msgbox_add_caption_item(Message_box_handle_main, "MENU_DIFFICULTY_HARD", false, false);
                ui_msgbox_add_caption_item(Message_box_handle_main, "MENU_DIFFICULTY_INSANE", false, false);
                const int menu = ui_msgbox_get_menu_handle(Message_box_handle_main);
                // Insane opens with a completed game.
                bool insane = false;
                const player_profile_data* profile = player_profile_get("main_menu", Player_controller_index != 0xFFFFFFFF ? Player_controller_index : 0);
                if (profile != nullptr && profile->game_completed)
                {
                    insane = true;
                }
                ui_menu_enable_menu_item(menu, 3, insane);
                ui_menu_set_selected_item(menu, difficulty_get_level());
                box = Message_box_handle_main;
            }
            stop = true;
            Main_menu_delayed_selection_state = MMSS_SELECT_DIFFICULTY;
            break;
        case MMSS_SELECT_GOLDEN_HAMMER:
            if (Main_menu_delayed_selection != MENU_OPTION_NEW_GAME)
            {
                break;
            }
            if (Main_menu_selected_golden_hammer)
            {
                close_box_of(MMSS_SELECT_GOLDEN_HAMMER);
                break;
            }
            if (delayed_box_needed(MMSS_SELECT_GOLDEN_HAMMER, box))
            {
                const player_profile_data* profile = player_profile_get("main_menu", Player_controller_index != 0xFFFFFFFF ? Player_controller_index : 0);
                if (profile != nullptr && profile->golden_hammer_unlocked)
                {
                    Message_box_handle_main = ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_START_WEAPON", "", main_menu_golden_hammer_cb, false, false, nullptr, nullptr, false);
                    ui_msgbox_add_caption_item(Message_box_handle_main, "MENU_START_WITH_GOLDEN_HAMMER", false, false);
                    ui_msgbox_add_caption_item(Message_box_handle_main, "MENU_START_WITH_SLEDGEHAMMER", false, false);
                    box = Message_box_handle_main;
                    ui_menu_set_selected_item(ui_msgbox_get_menu_handle(Message_box_handle_main), Wants_to_start_game_with_golden_hammer ? 0 : 1);
                }
                else
                {
                    // Without the golden hammer the sledgehammer is chosen.
                    Main_menu_selected_golden_hammer = true;
                    Wants_to_start_game_with_golden_hammer = false;
                    box = Message_box_handle_main;
                }
            }
            stop = true;
            Main_menu_delayed_selection_state = MMSS_SELECT_GOLDEN_HAMMER;
            break;
        case MMSS_TRIGGER_SELECTION:
            if (multi_core_platform_has_pending_invite())
            {
                break;
            }
            switch (Main_menu_delayed_selection)
            {
            case MENU_OPTION_NEW_GAME:
                fog_of_war_clear();
                stats_reset_save_game_stats();
                World.dlc_bundle_id = -1;
                // Magic strings: the opening movie, then the campaign's first zone.
                video_cutscene_play("RFG_CINE_00A.bik", 0xFFFFFFFF, "terr01_03_12.rfgzone_pc", true, true, nullptr, nullptr, nullptr);
                human_voice_level_init_post();
                ui_fade_to_black_fade_out(true);
                difficulty_set_level_reload_overwrite(DIFFICULTY_LEVEL_INVALID);
                break;
            case MENU_OPTION_LOAD_GAME:
                World.dlc_bundle_id = -1;
                hud_ui_save_load_show(SLUT_LOAD);
                main_menu_show_menu(false);
                break;
            case MENU_OPTION_WRECKING_CREW:
                // wrecking.md WC2: GS_WC_INIT, then the wrecking crew menu.
                ui_wrecking_crew_show();
                main_menu_show_menu(false);
                gamemusic_frontend_start(true);
                break;
            case MENU_OPTION_XBOX_LIVE:
                // Deviation (application.cpp deviation 1): online needs Steam, which --no-steam turns off.
                if (!g_openrfgUseSteam)
                {
                    keen::debug::nativePrintArgs("[ui] main menu: online needs Steam (--no-steam)\n");
                    main_menu_show_menu(true);
                    break;
                }
                Obj_pos = Off_screen_pos;
                game_push_multi_init(GS_MULTIPLAYER_LIVE, true, true);
                main_menu_show_menu(false);
                gamemusic_frontend_start(true);
                break;
            case MENU_OPTION_DOWNLOAD_CONTENT:
                ui_view_dlc_show();
                main_menu_show_menu(false);
                break;
            case MENU_OPTION_SYSLINK:
                Obj_pos = Off_screen_pos;
                keen::debug::nativePrintArgs("[ui] main menu: system link\n");
                game_push_multi_init(GS_MULTIPLAYER_SYSLINK, true, true);
                main_menu_show_menu(false);
                gamemusic_frontend_start(true);
                break;
            default:
                break;
            }
            Main_menu_delayed_selection = MENU_OPTION_NONE;
            Main_menu_delayed_selection_state = MMSS_NONE;
            if (MM_delayed_process_input_id != -1)
            {
                ui_relinquish_input_id(MM_delayed_process_input_id);
                MM_delayed_process_input_id = -1;
            }
            break;
        default:
            break;
        }
        if (stop)
        {
            return;
        }
    }
}

// The selection waits for its checks (main_menu_process_delayed_selection) with its own input id; the menu hides.
static void delay_selection(menu_option_types option)
{
    Main_menu_delayed_selection = option;
    Main_menu_delayed_selection_state = MMSS_NONE;
    if (MM_delayed_process_input_id == -1)
    {
        MM_delayed_process_input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    }
    main_menu_show_menu(false);
}

// 0x008F7C30 (:2061)
// The menu items' selection callback (the item index). Options pushes GS_MAINMENU_OPTIONS (UI6).
static void main_menu_handle_selection(int choice)
{
    Idle_time = 0.0f;
    Was_in_attract = false;
    Main_menu_selected_difficulty = false;
    Main_menu_selected_golden_hammer = false;
    Main_menu_accepted_no_profile_warning = false;
    Main_menu_prompted_for_device = false;
    Main_menu_accepted_no_device_warning = false;
    Main_menu_accepted_no_space_warning = false;
    Main_menu_accepted_max_save_warning = false;
    Main_menu_accepted_autosave_warning = false;
    switch (choice)
    {
    case MENU_OPTION_NEW_GAME:
        if (!Disable_new_game && main_menu_check_chunk_ready(RFG_CHUNK_ID_MULTIPLAYER, "MENU_CHUNK_NOT_READY_SINGLEPLAYER"))
        {
            Main_menu_delayed_selection = MENU_OPTION_NEW_GAME;
            Main_menu_delayed_selection_state = MMSS_NONE;
            if (MM_delayed_process_input_id == -1)
            {
                MM_delayed_process_input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
            }
            gamelog_local_reinit(false, nullptr);
            main_menu_show_menu(false);
        }
        break;
    case MENU_OPTION_LOAD_GAME:
        if (main_menu_check_chunk_ready(RFG_CHUNK_ID_MULTIPLAYER, "MENU_CHUNK_NOT_READY_SINGLEPLAYER"))
        {
            Main_menu_delayed_selection = MENU_OPTION_LOAD_GAME;
            Main_menu_delayed_selection_state = MMSS_NONE;
            if (MM_delayed_process_input_id == -1)
            {
                MM_delayed_process_input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
            }
            main_menu_show_menu(false);
        }
        break;
    case MENU_OPTION_WRECKING_CREW:
        // Original: RFG_CHUNK_ID_MULTIPLAYER (0, every chunk id is 0 in rfg.pdb). Only the new game has checks, so the
        // selection triggers on the next delayed pass.
        if (main_menu_check_chunk_ready(RFG_CHUNK_ID_MULTIPLAYER, "MENU_CHUNK_NOT_READY_WRECKING_CREW"))
        {
            delay_selection(MENU_OPTION_WRECKING_CREW);
        }
        break;
    case MENU_OPTION_XBOX_LIVE:
        // Original: RFG_CHUNK_ID_MULTIPLAYER (0, every chunk id is 0 in rfg.pdb).
        if (main_menu_check_chunk_ready(RFG_CHUNK_ID_MULTIPLAYER, "MENU_CHUNK_NOT_READY_MULTIPLAYER"))
        {
            Main_menu_accepted_nat_warning = false;
            delay_selection(MENU_OPTION_XBOX_LIVE);
        }
        break;
    case MENU_OPTION_DOWNLOAD_CONTENT:
        // Original: RFG_CHUNK_ID_MULTIPLAYER (0, every chunk id is 0 in rfg.pdb), with the single player message. Not
        // delayed, and the menu stays shown, unlike the other items.
        if (main_menu_check_chunk_ready(RFG_CHUNK_ID_MULTIPLAYER, "MENU_CHUNK_NOT_READY_SINGLEPLAYER"))
        {
            ui_view_dlc_show();
        }
        break;
    case MENU_OPTION_SYSLINK:
        // Original: RFG_CHUNK_ID_MULTIPLAYER (0, every chunk id is 0 in rfg.pdb).
        if (main_menu_check_chunk_ready(RFG_CHUNK_ID_MULTIPLAYER, "MENU_CHUNK_NOT_READY_MULTIPLAYER"))
        {
            delay_selection(MENU_OPTION_SYSLINK);
        }
        break;
    case MENU_OPTION_OPTIONS:
        // UI.md UI6: the option menus are ported (options_main.cpp).
        gameseq_push_state(GS_MAINMENU_OPTIONS, true, false);
        break;
    case MENU_OPTION_TITLE_SCREEN:
        if (Main_menu_auto_catch_done)
        {
            main_menu_start_auto_catch(false);
        }
        break;
    case MENU_OPTION_EXIT:
        Message_box_handle_main = ui_add_msgbox(MBT_ACCEPT_CANCEL_SELECT, "MENU_WARNING", "MENU_EXIT_GAME_CONFIRM", exit_game_cb, false, false, nullptr, nullptr, false);
        main_menu_show_menu(false);
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        break;
    }
}

// 0x008CF320 (:2240)
// Magic numbers: the attract video after 60 s without input (120 s after the first time).
static void main_menu_handle_attract(bool enabled)
{
    if (Was_in_attract)
    {
        Was_in_attract = false;
    }
    if (!enabled || Button_info.m_received_input || mouse_just_pressed(0) || mouse_just_moved() || (Utility_doc_handle != -1 && ui_is_msgbox_active(-2)) || multi_core_platform_ui_is_active())
    {
        Idle_time = 0.0f;
        return;
    }
    const float dtime = Frametime_real > 1.0f ? Frametime : Frametime_real;
    const float limit = Num_times_idle < 1 ? 60.0f : 120.0f;
    if (dtime + Idle_time < limit)
    {
        Idle_time = dtime + Idle_time;
        return;
    }
    Num_times_idle++;
    Idle_time = 0.0f;
    Was_in_attract = true;
    Obj_pos = Off_screen_pos;
    gameseq_set_state(GS_MAINMENU_IDLE, false);
}

// 0x008ABD70 (:2282)
// Abort and misc go back to the title screen; misc 2 opens the NVIDIA Highlights summary.
static void main_menu_process_hints_callback(hint_type hint)
{
    if (hint == HT_ABORT || hint == HT_MISC)
    {
        if (Main_menu_auto_catch_done)
        {
            main_menu_start_auto_catch(false);
        }
    }
    else if (hint == HT_MISC_2)
    {
        nvidia_highlights_open_summary(s_groupIdGameplay);
    }
}

// 0x008DE730 (:2322)
// The hints under the mouse (hints2/hints3 back to the title screen, hints4 the highlights), then the menu items; a
// released click selects.
static void main_menu_process_mouse()
{
    const char* names[4] = {"hints", "hints2", "hints3", "hints4"};
    const int hints[4] = {HT_SELECTION, HT_ABORT, HT_MISC, HT_MISC_2};
    vint_object_base* objects[4] = {nullptr, nullptr, nullptr, nullptr};
    vint_doc_info* info = ui_get_doc_info(Doc_handle_main);
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
    const int clicked = ui_mouse_process_clickables(objects, 4, true);
    if (clicked > -1)
    {
        const int hint = hints[clicked];
        if (hint == HT_ABORT || hint == HT_MISC)
        {
            if (Main_menu_auto_catch_done)
            {
                main_menu_start_auto_catch(false);
            }
        }
        else if (hint == HT_MISC_2)
        {
            nvidia_highlights_open_summary(s_groupIdGameplay);
        }
        if (hint != HT_SELECTION)
        {
            return;
        }
    }
    int index = ui_menu_process_mouseover_extend(Doc_handle_main, Menu_handle, cMenu_group_name, cMenu_item_name, false);
    if (index < 0)
    {
        // The select hint clicked selects the current item.
        if (clicked > -1 && hints[clicked] == HT_SELECTION)
        {
            index = ui_menu_get_selected_item(Menu_handle);
        }
        if (index < 0)
        {
            return;
        }
    }
    if (mouse_just_released(0))
    {
        ui_menu_selection_clicked(Menu_handle, index, false, false);
    }
}

// 0x008DE8A0 (:2362)
// A pending state goes once the menu is loaded; with the menu's input id on top, the mouse and the abort button
// (it selects the last item, exit).
static void main_menu_process_input()
{
    if (Assets_loaded_main && Pending_state_transfer)
    {
        gameseq_push_state(Pending_state, true, false);
        Pending_state_transfer = false;
    }
    if (multi_core_platform_ui_is_active())
    {
        return;
    }
    int top = Current_input_lock_id;
    if (Current_input_lock_id == -1)
    {
        top = Input_types.size() - 1 < 0 ? 1 : Input_types[Input_types.size() - 1].id;
    }
    if (Input_id_main != top)
    {
        return;
    }
    main_menu_process_mouse();
    if (ui_get_button_info(BC_ABORT, Input_id_main) > 0.0f)
    {
        ui_menu_select_item_by_index(Menu_handle, ui_menu_get_total_item_count(Menu_handle) - 1);
    }
}

// The localized text of a key (localization_get_string_from_hash's hash lookup, inlined).
static const wchar_t* localized(const char* key)
{
    const unsigned int hash = keen::getCrc32LwrValue(key);
    wchar_t* text = nullptr;
    if (hash == 0 || s_localization_is_loading || !Localization_hash.get(hash, text))
    {
        return nullptr;
    }
    return text;
}

// A hint of the menu with the button image of its action.
static void add_menu_hint(hint_type type, control_button_action action, const char* key)
{
    char text[256];
    controls_fill_options_button_hint(text, sizeof(text), localized(key));
    ui_menu* menu = ui_menu_get_via_handle(Menu_handle);
    if (menu == nullptr)
    {
        return;
    }
    char image[256];
    controls_get_button_image_name(image, 64, action, Player_controller_index);
    menu->add_hint(type, action, text, image);
}

// 0x008DE940 (:2392)
// Select, back and switch input device hints, and the highlights hint when NVIDIA Highlights has some.
static void main_menu_create_button_hints()
{
    ui_menu_remove_all_hints(Menu_handle);
    add_menu_hint(HT_SELECTION, CBA_MENU_SELECT, "MENU_SELECT_WITH_BUTTON");
    add_menu_hint(HT_ABORT, CBA_MENU_ABORT, "MENU_BACK_WITH_BUTTON");
    add_menu_hint(HT_MISC, CBA_MENU_X, "MENU_SWITCH_INPUT_DEVICE");
    if (!nvidia_highlights_is_enabled() || nvidia_highlights_get_num_highlights() == 0)
    {
        return;
    }
    char text[256];
    controls_fill_options_button_hint(text, sizeof(text), localized("MENU_NVIDIA_WITH_BUTTON"));
    ui_menu_add_misc_hint_2(Menu_handle, text, CBA_MENU_Y);
}

// 0x008F7E40 (:2421)
// The menu: the controller select data item, the 9 entries (back to the title screen hidden, exit shown), the
// selection, the hints; online is disabled with modified tables.
static void main_menu_create_menu()
{
    if (Menu_handle != -1)
    {
        return;
    }
    if (Main_menu_controller_select_handle == 0xFFFFFFFF)
    {
        vint_data_item* item = vint_data_item::create_new("MainMenuControllerSelect");
        if (item != nullptr)
        {
            const vint_variant_type types[2] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING};
            item->set_template(types, 2);
            Main_menu_controller_select_handle = item->get_handle();
            ui_add_data_item(Doc_handle_main, (int)Main_menu_controller_select_handle);
        }
    }
    Menu_handle = ui_menu_add(Doc_handle_main, "", nullptr, main_menu_process_hints_callback, nullptr, true, Selected_menu_option, Input_id_main, false);
    ui_menu_add_function_item(Menu_handle, "MENU_NEW_GAME", main_menu_handle_selection, true, false, true);
    ui_menu_add_function_item(Menu_handle, "MENU_LOAD_GAME", main_menu_handle_selection, true, false, true);
    ui_menu_add_function_item(Menu_handle, "MENU_WRECKING_CREW", main_menu_handle_selection, true, false, true);
    ui_menu_add_function_item(Menu_handle, "NETWORK_PLATFORM_NAME", main_menu_handle_selection, true, false, true);
    ui_menu_add_function_item(Menu_handle, "MENU_BONUS_CAMPAIGN", main_menu_handle_selection, true, false, true);
    ui_menu_add_function_item(Menu_handle, "NETWORK_LOCAL_NAME", main_menu_handle_selection, true, false, true);
    ui_menu_add_function_item(Menu_handle, "MENU_OPTIONS", main_menu_handle_selection, false, false, false);
    ui_menu_add_function_item(Menu_handle, "MENU_BACK", main_menu_handle_selection, true, false, false);
    ui_menu_add_function_item(Menu_handle, "MENU_EXIT", main_menu_handle_selection, true, false, false);
    ui_menu_enable_menu_item(Menu_handle, MENU_OPTION_EXIT, true);
    ui_menu_show_menu_item(Menu_handle, MENU_OPTION_EXIT, true);
    ui_menu_enable_menu_item(Menu_handle, MENU_OPTION_TITLE_SCREEN, false);
    ui_menu_show_menu_item(Menu_handle, MENU_OPTION_TITLE_SCREEN, false);
    ui_menu_set_selected_item(Menu_handle, Selected_menu_option);
    main_menu_create_button_hints();
    if (Modified_table_data)
    {
        ui_menu_enable_menu_item(Menu_handle, MENU_OPTION_XBOX_LIVE, false);
    }
    audiolib_min_max_distance_set_extended(false);
}

// 0x008CF450 (:2528)
static void main_menu_destroy_menu()
{
    if (Menu_handle != -1)
    {
        ui_menu_destroy(Menu_handle);
        Menu_handle = -1;
    }
    audiolib_min_max_distance_set_extended(false);
}

// 0x00919AA0 (:2546)
// GS_MAINMENU (user flags 1: the state runs stream2 in its own handshake), the attract video, the option menus, the
// in-game and player death options, the multiplayer menus and the DLC view.
void main_menu_init()
{
    state_info info;
    info.enter = main_menu_enter;
    info.exit = main_menu_exit;
    info.process = main_menu_process;
    info.handshake = main_menu_handshake;
    info.render = main_menu_render;
    info.transparent = false;
    info.pause_beneath = false;
    info.user_flags = 1;
    gameseq_register_state(GS_MAINMENU, info);
    state_info idle;
    idle.enter = main_menu_idle_enter;
    idle.exit = main_menu_idle_exit;
    idle.process = main_menu_idle_process;
    idle.handshake = main_menu_idle_handshake;
    idle.render = main_menu_idle_render;
    idle.transparent = false;
    idle.pause_beneath = false;
    gameseq_register_state(GS_MAINMENU_IDLE, idle);
    options_init();
    state_info ingame;
    ingame.enter = ingame_options_enter;
    ingame.exit = ingame_options_exit;
    ingame.process = ingame_options_process;
    ingame.handshake = ingame_options_handshake;
    ingame.render = ingame_options_render;
    ingame.transparent = false;
    ingame.pause_beneath = false;
    ingame.hide_cursor = false;
    gameseq_register_state(GS_INGAME_OPTIONS, ingame);
    state_info death;
    death.enter = player_death_options_enter;
    death.exit = player_death_options_exit;
    death.process = player_death_options_process;
    death.handshake = player_death_options_handshake;
    death.render = player_death_options_render;
    death.transparent = false;
    death.pause_beneath = false;
    gameseq_register_state(GS_PLAYER_DEATH_OPTIONS, death);
    multi_player_menu_system_init();
    state_info dlc;
    dlc.enter = ui_view_dlc_enter;
    dlc.exit = ui_view_dlc_exit;
    dlc.exit_notify = ui_view_dlc_exit_notify;
    dlc.process = ui_view_dlc_process;
    dlc.handshake = ui_view_dlc_handshake;
    dlc.render = ui_view_dlc_render;
    dlc.transparent = true;
    dlc.pause_beneath = true;
    gameseq_register_state(GS_VIEW_DLC_SCREEN, dlc);
}

// 0x0090A290 (:2584)
// The main menu document (a load failure exits the game), the control and input ids, the menu character in single
// player, the box of a boot to the main menu.
void main_menu_enter(game_state)
{
    Idle_time = 0.0f;
    Was_in_attract = false;
    Assets_loaded_main = false;
    Main_menu_delayed_selection = MENU_OPTION_NONE;
    Main_menu_delayed_selection_state = MMSS_NONE;
    Main_menu_accepted_no_profile_warning = false;
    Main_menu_prompted_for_device = false;
    Main_menu_accepted_no_device_warning = false;
    Main_menu_accepted_no_space_warning = false;
    Main_menu_accepted_max_save_warning = false;
    Main_menu_accepted_autosave_warning = false;
    Main_menu_waiting_for_device = false;
    Forced_character_invisible_last_frame = false;
    Display_controller_select = false;
    Main_menu_controller_select_handle = 0xFFFFFFFF;
    Main_menu_msgbox_for_chunks = false;
    Main_menu_active = true;
    if (First_time_loaded)
    {
        First_time_loaded = false;
    }
    else
    {
        ui_force_unloads_complete();
    }
    audiolib_all_sounds_stop(true, false);
    gamemusic_set_minimum_music_level(GAMEMUSIC_THRESHOLD_SILENCE);
    gamemusic_set_limited_music_level(GAMEMUSIC_THRESHOLD_AMBIENCE);
    if (!Multiplayer && !wrecking_crew_should_restart())
    {
        Level_mempool.set_thread_ownership();
        Level_mempool.clear();
    }
    World.sl_flags.pending_save_game = false;
    Doc_handle_main = ui_load_document("rfg_ui_main_menu", DFIT_NONE, 0.0f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (Doc_handle_main == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        exit(1);
    }
    Control_id_main = (int)ui_request_control_id();
    Input_id_main = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    gamewide_check_and_clear_invite_flag();
    game_render_set_motion_blur_amount(0.0f);
    fb_accum_blur_set_percentage(0.0f);
    camera_set_view_player(PZS_OFF, true);
    stats_turn_off_all_cheats();
    gamelog_campaign_start_logged(false);
    if (!Multiplayer)
    {
        Main_menu_load_ui_character = true;
        Doc_handle_uichar = Doc_handle_main;
        ui_character_fade_main_menu(Doc_handle_main);
        ui_character_init_resources();
        Fx_delay_timer.invalidate();
        Active_uichar = true;
    }
    else
    {
        Main_menu_load_ui_character = false;
    }
    if (game_booted_to_main_menu() && (Utility_doc_handle == -1 || !ui_is_msgbox_active(-2)))
    {
        game_show_boot_to_main_menu_message_box();
    }
    game_clear_boot_to_main_menu();
}

// 0x008F8160 (:2681)
void main_menu_exit(game_state)
{
    if (Was_in_attract)
    {
        audiolib_all_sounds_unpause(true, true);
    }
    if (Main_menu_load_ui_character)
    {
        ui_character_shutdown_internal();
        Active_uichar = false;
    }
    mm_stop_bink_video();
    multi_player_menu_system_state_stack_unload();
    main_menu_destroy_menu();
    // Magic number: a 0.5 s fade out.
    ui_unload_document(Doc_handle_main, DFOT_NONE, 0.5f);
    ui_relinquish_control_id((unsigned int)Control_id_main);
    ui_relinquish_input_id(Input_id_main);
    if (MM_delayed_process_input_id != -1)
    {
        ui_relinquish_input_id(MM_delayed_process_input_id);
        MM_delayed_process_input_id = -1;
    }
    Control_id_main = -1;
    Doc_handle_main = -1;
    Main_menu_active = false;
    gamemusic_set_minimum_music_level(GAMEMUSIC_THRESHOLD_SILENCE);
    gamemusic_set_limited_music_level(GAMEMUSIC_THRESHOLD_SILENCE);
    audiolib_all_sounds_unpause(true, true);
}

// 0x008898D0 (:2721)
void main_menu_pause_video()
{
    if (Bink_info_main.handle != 0)
    {
        video_player_pause(Bink_info_main.handle);
    }
}

// 0x008898F0 (:2728)
void main_menu_unpause_video()
{
    if (Bink_info_main.handle != 0)
    {
        video_player_unpause(Bink_info_main.handle);
    }
}

// 0x008CF480 (:2735)
void main_menu_process_video()
{
    if (Bink_info_main.handle != 0)
    {
        mm_process_bink_video();
    }
}

// The title screen catch: the local player (with an account), the profile, the save data and, behind a loading box,
// the options once the save data is in; then the Steam save import box when there are saves to import.
static void main_menu_catch_local_player()
{
    Main_menu_auto_catch_done = true;
    Main_menu_auto_catch_started = false;
    Display_controller_select = false;
    keen::input::setControllerAutoCatch(Keen_input_system, nullptr, keen::InputSystemControllerAutoCatchType_Disabled);
    rfg_get_local_game_session()->addLocalPlayer(nullptr, unit4::LocalPlayerType_WithAccount);
    player_profile_init();
    game_save_start_get_save_data();
    Main_menu_loading_options = true;
    // ui_show_loading_screen's box without its reference count, inlined (ui_add_msgbox refuses it during a save or
    // load box).
    Message_box_handle_main = ui_add_msgbox(MBT_LOADING, "MENU_LOADING", "", nullptr, false, false, nullptr, nullptr, false);
    Main_menu_waiting_for_options_load = true;
}

// While the options load (the save data arrives), then the box goes once the localization is loaded.
// False while the save data is not in.
static bool main_menu_wait_for_options()
{
    if (Main_menu_loading_options)
    {
        if (!game_save_has_save_data())
        {
            return false;
        }
        game_options_load();
        wrecking_crew_reset_controls();
        Main_menu_loading_options = false;
    }
    if (!s_localization_is_loading && localization_is_audio_loading())
    {
        ui_update_msgbox_caption(Message_box_handle_main, "MENU_LOADING", true);
    }
    if (!Main_menu_loading_options && !s_localization_is_loading && !localization_is_audio_loading())
    {
        ui_hide_msgbox(Message_box_handle_main, true);
        Message_box_handle_main = -1;
        Main_menu_waiting_for_options_load = false;
        game_save_clear_load_directory_cache();
        game_save_check_load_directory_cache();
        if (Num_imported_save_file_infos != 0 && !Has_imported_any_save_data)
        {
            Message_box_handle_main = ui_add_msgbox(MBT_ACCEPT_CANCEL, "STEAM_SAVE_DATA_IMPORT_NAME", "STEAM_SAVE_DATA_IMPORT_DESCRIPTION", main_menu_save_data_import_cb, false, false, nullptr, nullptr, false);
        }
    }
    return true;
}

// 0x00913910 (:2760)
// The title screen until the local player is caught, the document and the menu character loading, then the menu
// (rebuilt on a language change), its input and selections, the restored multiplayer menus after a match, the
// background video, the invite into the online menus.
void main_menu_process()
{
    vint_data_item* select = vint_data_item::find(Main_menu_controller_select_handle);
    if (select != nullptr)
    {
        select->set_element(0, vint_variant(Display_controller_select));
        select->set_element(1, vint_variant(multi_core_platform_has_pending_invite() ? "SESSION_INTERACTION_SELECT_CONTROLLER_INVITATION" : "SESSION_INTERACTION_SELECT_CONTROLLER"));
    }
    bool hide = false;
    // Deviation 4: --notitle catches the local player at once.
    if (!Main_menu_auto_catch_done && (keen::input::getPrimaryInputDeviceClass(Keen_input_system, nullptr) != keen::ControllerClass_None || g_openrfgMainMenuNoTitle))
    {
        // With no key press to catch, the player gets the keyboard (controller 0) and mouse (controller 1) the catch
        // would give it; a player without a device raises the controller disconnected interaction.
        if (keen::input::getPrimaryInputDeviceClass(Keen_input_system, nullptr) == keen::ControllerClass_None)
        {
            keen::input::setControllerPlayerId(Keen_input_system, 0u, nullptr);
            keen::input::setControllerPlayerId(Keen_input_system, 1u, nullptr);
        }
        main_menu_catch_local_player();
        main_menu_wait_for_options();
    }
    else if (Main_menu_waiting_for_options_load)
    {
        main_menu_wait_for_options();
    }
    if (Utility_doc_handle == -1 || !ui_is_msgbox_active(-2))
    {
        video_player_unpause(Bink_info_main.handle);
    }
    else
    {
        main_menu_process_delayed_selection();
        ui_process((unsigned int)Control_id_main, false);
        video_player_pause(Bink_info_main.handle);
        hide = true;
    }
    if (!Assets_loaded_main)
    {
        switch (ui_get_doc_asset_state(Doc_handle_main))
        {
        case ST_UNLOADED:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            return;
        case ST_LOADING:
            break;
        case ST_LOADED:
            Assets_loaded_main = true;
            break;
        case ST_ERROR:
            return;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
        // The menu character's four parts must be in too (single player).
        const bool character_loaded = Player_info.loaded && Coat_info.loaded && Weapon_info.loaded && Head_info.loaded;
        Assets_loaded_main = Assets_loaded_main && (character_loaded || !Main_menu_load_ui_character);
        if (!Assets_loaded_main)
        {
            if (Main_menu_load_ui_character)
            {
                ui_character_process(Frametime);
            }
            ui_process((unsigned int)Control_id_main, false);
            return;
        }
        main_menu_create_menu();
        if (MM_script == 0)
        {
            console_run_script("gamestart_mm.txt");
            MM_script = 1;
        }
        mm_get_bink_screen_info();
    }
    if (Assets_loaded_main && Main_menu_current_language != Current_language && !s_localization_is_loading)
    {
        ui_update_resolution(Screen_size.x, Screen_size.y);
        main_menu_destroy_menu();
        main_menu_create_menu();
        // The cursor is set again (hidden, then shown as it was).
        const bool cursor = Cursor_visible;
        if (Cursor_visible)
        {
            ui_cursor_set_visible(false);
            ui_cursor_set_visible(cursor);
        }
        Main_menu_current_language = Current_language;
    }
    if (!Main_menu_auto_catch_done)
    {
        hide = true;
        main_menu_start_auto_catch(Message_box_handle_main != -1);
    }
    if (Main_menu_controller_type != Cached_input_states[Current_cached_input_state].controllers[0].controller_type)
    {
        main_menu_create_button_hints();
        Main_menu_controller_type = Cached_input_states[Current_cached_input_state].controllers[0].controller_type;
    }
    // The current control configuration's name.
    vint_doc_info* info = ui_get_doc_info(Doc_handle_main);
    vint_object_base* profile_text = vint_object_base::find_object("current_profile_text", info != nullptr ? info->m_doc : nullptr);
    profile_text->set_property_value("visible", vint_variant(true), 0);
    const char* config;
    switch (controls_get_current_config_setting())
    {
    case CCS_KEYBOARD_MOUSE:
        config = "MENU_CONTROL_CONFIG_KB_MOUSE";
        break;
    case CCS_XBOX:
        config = "MENU_CONTROL_CONFIG_XB360";
        break;
    case CCS_PS4:
        config = "MENU_CONTROL_CONFIG_PS4";
        break;
    case CCS_XBONE:
        config = "MENU_CONTROL_CONFIG_XBONE";
        break;
    case CCS_STEAM:
        config = "MENU_CONTROL_CONFIG_STEAM";
        break;
    case CCS_SWITCH:
        config = "MENU_CONTROL_CONFIG_SWITCH";
        break;
    default:
        config = "MENU_CONTROL_CONFIG_GENERIC";
        break;
    }
    profile_text->set_property_value("text_tag", config, 0);
    const game_state state = gameseq_get_state();
    if (state != GS_MAINMENU && gameseq_get_state() != GS_MAINMENU_OPTIONS && gameseq_get_state() != GS_MAINMENU_OPTIONS_EXTRAS)
    {
        hide = true;
    }
    if (Multiplayer || gameseq_state_is_active(GS_WRECKING_CREW_MAIN_MENU) || hide)
    {
        Obj_pos = Off_screen_pos;
        ui_character_set_frame_visible(false);
    }
    else
    {
        ui_character_set_frame_visible(true);
        ui_character_set_visible(Display_character);
    }
    if (Main_menu_load_ui_character)
    {
        ui_character_process(Frametime);
    }
    if (gameseq_get_state() == GS_MAINMENU && !ui_menu_get_tween_on_select_end(Menu_handle) && gameseq_get_state() == GS_MAINMENU && !ui_menu_get_tween_on_select_end(Menu_handle) && !ui_menu_is_fading_in(Menu_handle))
    {
        main_menu_process_input();
    }
    ui_menu_enable_menu_item(Menu_handle, MENU_OPTION_NEW_GAME, !Disable_new_game);
    if (gameseq_get_state() == GS_MAINMENU)
    {
        game_options_process();
    }
    if (Main_menu_msgbox_for_chunks)
    {
        main_menu_check_chunk_ready(Main_menu_last_chunk_id, Main_menu_last_chunk_error_locatext);
    }
    main_menu_process_delayed_selection();
    if (Main_menu_show_menu_next_frame)
    {
        main_menu_show_menu(true);
        Main_menu_show_menu_next_frame = false;
    }
    if (!hide)
    {
        if (Forced_character_invisible_last_frame)
        {
            main_menu_show_menu(true);
            Forced_character_invisible_last_frame = false;
        }
    }
    else
    {
        main_menu_show_menu(false);
        Forced_character_invisible_last_frame = true;
    }
    if (game_booted_to_main_menu())
    {
        if (Multiplayer)
        {
            mm_stop_bink_video();
        }
    }
    else if (!Multiplayer || (Bink_info_main.cur_state != BINK_VIDEO_PLAYING && Bink_info_main.cur_state != BINK_VIDEO_ERROR))
    {
        if (wrecking_crew_should_restart())
        {
            wrecking_crew_restart();
        }
    }
    else
    {
        // Back from a match (the video plays or failed): the multiplayer menus come back.
        multi_player_menu_system_state_stack_load();
    }
    unit4::LocalGameSession* session = rfg_get_local_game_session();
    if (!Main_menu_auto_catch_done || Message_box_handle_main != -1 || session == nullptr || !session->isLocalPlayerReady(nullptr) || !multi_core_platform_has_pending_invite() || gameseq_state_is_active(GS_WRECKING_CREW_MAIN_MENU))
    {
        mm_process_bink_video();
        ui_menu_enable_menu_item(Menu_handle, MENU_OPTION_DOWNLOAD_CONTENT, true);
        ui_menu_show_menu_item(Menu_handle, MENU_OPTION_DOWNLOAD_CONTENT, true);
        if (!console_is_active() && !Interface_debug_active)
        {
            key_in();
        }
        ui_add_for_processing(Doc_handle_main);
        ui_process((unsigned int)Control_id_main, false);
        gamemusic_update(true);
        if (Was_in_attract)
        {
            Was_in_attract = false;
        }
        Idle_time = 0.0f;
    }
    else if (main_menu_check_chunk_ready(RFG_CHUNK_ID_MULTIPLAYER, "MENU_CHUNK_NOT_READY_MULTIPLAYER"))
    {
        // A pending invite: into the online menus (unless an interaction or modified tables hold it back).
        if (gameseq_get_state() == GS_MAINMENU)
        {
            unit4::SessionInteractionData interaction;
            interaction.id = 0xFFFFFFFF;
            interaction.responseOptionCount = 0;
            interaction.params.parameterCount = 0;
            interaction.user.isValid = false;
            g_gameLinkMutex.lock(0);
            const bool required = get_remote_game_session()->isInteractionRequired(&interaction);
            g_gameLinkMutex.unlock();
            if (!required && !Modified_table_data)
            {
                Obj_pos = Off_screen_pos;
                if (gameseq_get_pending_state() != GS_MULTI_INIT)
                {
                    game_push_multi_init(GS_MULTIPLAYER_LIVE, true, true);
                }
                main_menu_show_menu(false);
            }
            mm_process_bink_video();
        }
        else
        {
            gameseq_pop_state();
            mm_process_bink_video();
        }
    }
    else
    {
        multi_core_platform_clear_pending_invite();
    }
    if (g_openrfgMainMenuProcess != nullptr && Assets_loaded_main)
    {
        g_openrfgMainMenuProcess();
    }
}

// 0x008CF490 (:3283)
void main_menu_handshake(rl_renderer*)
{
    if (!localization_is_audio_loading())
    {
        stream2_do_frame(true);
    }
    if (Do_character_handshake && Active_uichar && !os_minimized())
    {
        ui_character_process_internal();
    }
}

// 0x008F8290 (:3296)
// The main menu document (once loaded), the version text in the lower right while the items show; black while a
// multiplayer match is pushed over it (but its tally and results) or the menu character loads.
void main_menu_render(rl_renderer*)
{
    const bool match_over = Multiplayer && multi_game_lobby_pushed_to_game() && gameseq_get_state() != GS_MULTIPLAYER_XP_TALLY && gameseq_get_state() != GS_MULTIPLAYER_RESULTS;
    if (!match_over && (!Main_menu_load_ui_character || Assets_loaded_main))
    {
        if (Assets_loaded_main)
        {
            ui_add_for_rendering(Doc_handle_main);
        }
        ui_render((unsigned int)Control_id_main, false);
        if (fonts_are_initializing)
        {
            return;
        }
        vint_doc_info* info = ui_get_doc_info(Doc_handle_main);
        if (info == nullptr || info->m_doc == nullptr)
        {
            return;
        }
        vint_object_base* items = vint_object_base::find_object("item_group", info->m_doc);
        if (items == nullptr)
        {
            return;
        }
        vint_variant visible;
        if (items->get_property_value("visible", visible, 0) && (bool)visible)
        {
            char version[64];
            keen::formatString(version, sizeof(version), "VERSION: %s", keen::getBuildVersionString());
            const int width = Screen_size.x;
            const int height = Screen_size.y;
            const int text_width = ui_get_usage_character_width(version, Credits_body_font);
            const int text_height = ui_get_usage_character_height(version, Credits_body_font);
            gr_set_color(255, 255, 255, 128);
            // Magic numbers: 50 and 30 pixels from the corner at 720 lines.
            const float scale = (float)height / 720.0f;
            gr_string_scale((int)((float)width - (float)(text_width + 50) * scale), (int)((float)height - (float)(text_height + 30) * scale), version, scale, Credits_body_font, GM_FILTER);
        }
        return;
    }
    gr_set_color(0, 0, 0, 255);
    const rl_screen_info* screen = rl_render_lib::get_instance()->get_screen_info();
    gr_rect(0, 0, screen->get_screen_width(), screen->get_screen_height(), GM_FILTER);
    ui_render((unsigned int)Control_id_main, false);
}

// 0x008CF4D0 (:3365)
// The items, title and hints groups shown or hidden; showing fades the menu in again.
void main_menu_show_menu(bool show)
{
    if (show)
    {
        ui_menu* menu = ui_menu_get_via_handle(Menu_handle);
        if (menu != nullptr && !menu->get_awaiting_fade_in_end())
        {
            menu->set_awaiting_fade_in_end(true);
            menu->force_data_item_update();
        }
    }
    const vint_variant visible(show);
    vint_doc_info* info = ui_get_doc_info(Doc_handle_main);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    const char* groups[3] = {"item_group", "title_group", "hints_group"};
    for (int i = 0; i < 3; i++)
    {
        vint_object_base* group = vint_object_base::find_object(groups[i], info->m_doc);
        if (group != nullptr)
        {
            group->set_property_value("visible", visible, 0);
        }
    }
}

// 0x00889910 (:3401)
// LTCG: show in AL.
static void main_menu_show_controller_select(bool show)
{
    Display_controller_select = show;
}

// 0x00889920 (:3411)
void main_menu_start_title_music()
{
    audiolib_all_sounds_stop(true, false);
    gamemusic_set_minimum_music_level(GAMEMUSIC_THRESHOLD_SILENCE);
    gamemusic_set_limited_music_level(GAMEMUSIC_THRESHOLD_AMBIENCE);
}

// 0x00889940 (:3431)
void main_menu_stop_title_music()
{
    gamemusic_set_minimum_music_level(GAMEMUSIC_THRESHOLD_SILENCE);
    gamemusic_set_limited_music_level(GAMEMUSIC_THRESHOLD_SILENCE);
    audiolib_all_sounds_unpause(true, true);
}

// 0x00889960 (:3448)
int main_menu_get_doc_handle()
{
    return Doc_handle_main;
}

// 0x00889970 (:3454)
int main_menu_get_control_id()
{
    return Control_id_main;
}

// 0x00889980 (:3460)
int main_menu_get_input_id()
{
    return Input_id_main;
}

// 0x00889990 (:3466)
int main_menu_get_selected_option()
{
    return Selected_menu_option;
}

// 0x008899A0 (:3472)
bool main_menu_get_active()
{
    return gameseq_get_state() != GS_MAINMENU_IDLE && Doc_handle_main != -1;
}

// 0x008899C0 (:3479)
bool main_menu_is_processing_selection()
{
    return Main_menu_delayed_selection != MENU_OPTION_NONE;
}

// 0x008F1AB0 (:3485)
void main_menu_notify_profile_change()
{
    if (gameseq_get_state() == GS_MAINMENU_IDLE || Doc_handle_main == -1)
    {
        return;
    }
    if (Main_menu_delayed_selection != MENU_OPTION_NONE)
    {
        main_menu_clear_delayed_selection();
    }
    Main_menu_msgbox_for_chunks = false;
    if (Message_box_handle_main != -1)
    {
        ui_hide_msgbox(Message_box_handle_main, false);
        Message_box_handle_main = -1;
    }
}

// 0x008899D0 (:3504)
void main_menu_display_character(bool display)
{
    Display_character = display;
}

// 0x008899E0 (:3519)
void main_menu_set_pending_state(game_state state)
{
    Pending_state = state;
    Pending_state_transfer = true;
}
