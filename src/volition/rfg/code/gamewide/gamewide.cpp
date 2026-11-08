// Original: volition/rfg/code/gamewide/gamewide.cpp
//
// The game-wide frame and shutdown helpers (gamewide.h), whole (multiplayer.md MP1). rfg_gamewide_unity.obj is a unity
// build: this file reads gameseq.cpp's State_stack and Stack_top_index and gamestate_gameplay.cpp's
// gamemusic_current_music_type (rfg_gamewide_unity.cpp includes both first).
//
// The return to the main menu (game_boot_to_main_menu's flag handling) is inlined into gamewide_login_changed with small
// differences per cause; boot_to_main_menu_record keeps those inlined copies as one helper with the differences as
// parameters.

#include "volition/rfg/code/gamewide/gamewide.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_thread.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/game_render_thread.h"
#include "volition/rfg/code/gamewide/game_save.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/geom/cloth_simulation.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_matchmaking_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_core/include/multi_voice.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_demolition.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/objects/object_light.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/video_player/video_player.h"
#include "volition/stream2/code/stream2_os.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/os/os.h"
#include "volition/vlib/code/os/scheduler.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/zlib_vlib.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <stdio.h>
#include <string.h>

bool Handled_debug_key; // 0x0165CBD1
bool Gameplay_shutdown_in_progress; // 0x02533876
bool Screenshot_pending; // 0x0253386B
bool Dump_tga; // 0x0253387B

// ---- file-static globals
// bool Sound (0x0165C942), notify_info* gameaudio_gem_handle (0x0227AEE4) and bool gamemusic_shutdown (0x0253384B) are
// statics of this unity TU in rfg.exe; placed in gameaudio.cpp (rfg_audio), whose shutdown is the only user here.
// multiplayer_music_play_id and gamemusic_forced_stop: static in rfg.exe's unity TU; external here for gamemusic.cpp (a
// TU of rfg_audio, SFX.md SX5).
int multiplayer_music_play_id = -1; // 0x0165CB74
static int Invite_msgbox_handle = -1; // 0x0165CBE0
static boot_to_main_menu_info Booted_to_main_menu; // 0x0227AFB0
bool gamemusic_forced_stop; // 0x0227AFC0
static char Pending_screenshot_filename[129]; // 0x02532C38
static bool Debug_key_down_last_frame; // 0x02533877
static bool Accepted_invite; // 0x02533878

// ---- internal functions
static bool prtsc_message_proc(unsigned int msg, unsigned int wparam, unsigned int lparam); // 0x007CD490 (:157)
static void gamewide_cleanup_invite(); // 0x007C05A0 (:174)
static bool invite_msgbox_cb(int mbox_handle, int selection_index, msgbox_choice choice); // 0x007C05C0 (:185)
static void get_next_screenshot_filename(char* filename); // 0x007C0630 (:220)
static void game_begin_gameplay_shutdown(); // 0x007C0730 (:291)

// The music part of the return to the main menu (inlined): stop the game music, fade out the multiplayer music, back
// to the front end music.
static void boot_to_main_menu_music()
{
    gamemusic_forced_stop = true;
    gamemusic_stop();
    gamemusic_forced_stop = false;
    if (multiplayer_music_play_id != -1)
    {
        // Magic number: a 5 s fade.
        audiolib_play_fade_out(multiplayer_music_play_id, 5000);
        multiplayer_music_play_id = -1;
    }
    gamemusic_current_music_type = GAMEMUSIC_MUSIC_FRONTEND;
    frontend_music_update();
}

// The cause part of the return to the main menu, as gamewide_login_changed inlines it: the first cause starts the
// gameplay shutdown, a later one replaces the recorded one when it ranks before it (`replace_above`: the recorded
// causes it replaces). The first cause marks the player as leaving by choice once the system message box is gone
// (`check_msgbox`) or at once.
static void boot_to_main_menu_record(boot_to_main_menu_cause cause, bool check_msgbox)
{
    if (Booted_to_main_menu.boot_cause == cause)
    {
        return;
    }
    Gameplay_shutdown_in_progress = true;
    if (!Booted_to_main_menu.booted)
    {
        Booted_to_main_menu.booted = true;
        Booted_to_main_menu.boot_cause = cause;
        if (!check_msgbox || !ui_is_msgbox_active(-2) || ui_hide_msgbox(-2, true))
        {
            Multi_left_game_by_choice = true;
        }
    }
    else if ((int)cause < (int)Booted_to_main_menu.boot_cause)
    {
        Booted_to_main_menu.boot_cause = cause;
    }
}

// 0x007CD490 (:157)
// The print screen key hook (the screenshot itself is not in rfg.exe).
static bool prtsc_message_proc(unsigned int msg, unsigned int wparam, unsigned int lparam)
{
    return false;
}

// 0x007C05A0 (:174)
static void gamewide_cleanup_invite()
{
    if (Invite_msgbox_handle != -1)
    {
        ui_hide_msgbox(Invite_msgbox_handle, false);
        Invite_msgbox_handle = -1;
    }
}

// 0x007C05C0 (:185)
static bool invite_msgbox_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Invite_msgbox_handle != mbox_handle)
    {
        return false;
    }
    Accepted_invite = false;
    if (choice != MBC_OK)
    {
        if ((int)choice < 1 || (int)choice > 2)
        {
            VLIB_CRITICAL_FAILURE();
        }
        multi_core_platform_clear_pending_invite();
        Invite_msgbox_handle = -1;
        return true;
    }
    Accepted_invite = true;
    Invite_msgbox_handle = -1;
    return true;
}

// 0x007C0630 (:220)
// "screen<date>_NNN.bmp" (.tga with Dump_tga), the first number that does not exist.
static void get_next_screenshot_filename(char* filename)
{
    char date[16];
    os_get_date(date, 16);
    // "screen" and four characters of the date (0, 1, 3, 4), then "_".
    char base[16];
    memcpy(base, "screen", 6);
    base[6] = date[0];
    base[7] = date[1];
    base[8] = date[3];
    base[9] = date[4];
    base[10] = '_';
    base[11] = '\0';
    const char* ext = Dump_tga ? ".tga" : ".bmp";
    int i = 0;
    char number[4];
    do
    {
        sprintf(number, "%03d", i);
        sprintf(filename, "%s%s%s", base, number, ext);
        if (!cf_exist(filename))
        {
            break;
        }
        i++;
    } while (i < 1000);
    if (i == 1000)
    {
        sprintf(filename, "%s%d%s", base, 0, ext);
    }
}

// 0x007C0730 (:291)
static void game_begin_gameplay_shutdown()
{
    Gameplay_shutdown_in_progress = true;
}

// 0x007C0740 (:304)
// Notes (decompiled behaviour, kept as is): an invite in single player asks to quit (the quit confirmation texts).
void gamewide_process_invite()
{
    if (Invite_msgbox_handle != -1)
    {
        if (ui_is_msgbox_active(Invite_msgbox_handle))
        {
            return;
        }
        ui_hide_msgbox(Invite_msgbox_handle, false);
        Invite_msgbox_handle = -1;
    }
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        const char* description = wrecking_crew_is_active() ? "MENU_QUIT_WRECKING_CREW" : "MENU_QUIT_CONFIRM";
        Invite_msgbox_handle = ui_add_msgbox(MBT_ACCEPT_CANCEL_SELECT, "MENU_WARNING", description, invite_msgbox_cb, false, false, nullptr, nullptr, false);
    }
}

// 0x007C07D0 (:324)
bool gamewide_check_and_clear_invite_flag()
{
    const bool accepted = Accepted_invite;
    Accepted_invite = false;
    return accepted;
}

// 0x007C07E0 (:331)
void game_clear_boot_to_main_menu()
{
    Booted_to_main_menu.booted = false;
    Booted_to_main_menu.boot_cause = INVALID_BOOT_CAUSE;
}

// 0x007D91B0 (:337)
void game_boot_to_main_menu(boot_to_main_menu_cause cause)
{
    if (cause == BC_REMOTE_GAME_SESSION || cause == BC_GAME_ALREADY_STARTED || cause == BC_LOBBY_MIGRATION_FAIL || cause == BC_GAME_MIGRATION_FAIL ||
        cause == BC_KICKED_FROM_LOBBY || cause == BC_HOST_LEFT || cause == BC_ACCEPTED_INVITE)
    {
        game_save_write_profile_data();
        multi_core_matchmake_internal_stop_search_timer();
    }
    boot_to_main_menu_music();
    if ((cause == BC_LOBBY_MIGRATION_FAIL || cause == BC_HOST_LEFT) && multi_core_platform_signin_change_is_pending())
    {
        cause = BC_LOGIN_CHANGE;
    }
    if (cause == Booted_to_main_menu.boot_cause)
    {
        return;
    }
    Gameplay_shutdown_in_progress = true;
    if (!Booted_to_main_menu.booted)
    {
        Booted_to_main_menu.booted = true;
        Booted_to_main_menu.boot_cause = cause;
        if (cause != BC_REMOTE_GAME_SESSION)
        {
            if (ui_is_msgbox_active(-2) && !ui_hide_msgbox(-2, true))
            {
                return;
            }
            // Only the login change and the lost connections leave "by choice".
            if ((int)cause < 2 || (int)cause > 4)
            {
                return;
            }
        }
        Multi_left_game_by_choice = true;
    }
    else if ((int)cause < (int)Booted_to_main_menu.boot_cause)
    {
        Booted_to_main_menu.boot_cause = cause;
    }
}

// 0x007C07F0 (:429)
void game_show_boot_to_main_menu_message_box()
{
    const char* title = "MENU_MBOX_CONNECTION_LOST";
    const char* description;
    switch (Booted_to_main_menu.boot_cause)
    {
    case BC_DLC_REMOVED:
        description = "MENU_DLC_REMOVAL_RESULT";
        title = "MENU_MBOX_DEVICE_REMOVED";
        break;
    case BC_LOGIN_CHANGE:
        description = "MENU_SIGN_IN_CHANGE";
        title = "MENU_MBOX_SIGN_IN_CHANGE";
        break;
    case BC_CONNECTION_LOST_XBL:
    {
        wchar_t text[128];
        _snwprintf(text, 128, L"%s", localization_get_display_string_req("MENU_LOST_LIVE_CONNECTION", nullptr));
        char encoded[384];
        vint_encode_wide_text(encoded, 384, text);
        Booted_to_main_menu.msgbox_handle = ui_add_msgbox(MBT_OK, "MENU_MBOX_CONNECTION_LOST", encoded, nullptr, false, false, nullptr, nullptr, false);
        return;
    }
    case BC_GAME_MIGRATION_FAIL:
        description = "MENU_LOST_OPPONENT_CONNECTION";
        break;
    case BC_HOST_LEFT:
        description = "MENU_HOST_LEFT_GAME";
        break;
    case BC_KICKED_FROM_LOBBY:
        description = "MENU_KICK_FROM_LOBBY";
        break;
    case BC_LOBBY_MIGRATION_FAIL:
        description = "MENU_LOST_LOBBY_CONNECTION";
        break;
    case BC_GAME_ALREADY_STARTED:
        description = "MENU_NO_GAME_SESSION";
        break;
    case BC_SAVE_ERROR:
        description = "MENU_UNABLE_TO_RETRIEVE";
        title = "MENU_SAVE_FAILED_TITLE";
        break;
    case BC_PROFILE_LOAD_FAILED:
        description = "MENU_CANT_LOAD_PROFILE_DATA";
        title = "MENU_MBOX_CANT_LOAD_PROFILE_DATA";
        break;
    case BC_ACCEPTED_INVITE:
    case BC_REMOTE_GAME_SESSION:
        return;
    default:
        description = "MENU_LOST_CONNECTION";
        break;
    }
    Booted_to_main_menu.msgbox_handle = ui_add_msgbox(MBT_OK, title, description, nullptr, false, false, nullptr, nullptr, false);
}

// 0x007D92B0 (:500)
void gamewide_shutdown()
{
    // Magic number: 100 ms for the other threads to settle.
    os_sleep(100);
    video_player_system_shutdown();
    // Inlined in rfg.exe (one unity TU with gameaudio.cpp); called here, where gameaudio.cpp is the audio library's TU
    // and holds the statics it changes.
    gameaudio_system_shutdown();
    if (Rt_init)
    {
        Rt_init = false;
        Rtcb.exit_thread = true;
        Rtcb.sim_done_trigger->fire();
    }
    input_system_shutdown();
    stream2_os_system_shutdown();
    multi_system_shutdown();
    object_light_system_shutdown();
    if (game_render_get_main_scene() != nullptr)
    {
        game_render_get_main_scene()->release();
    }
    rl_shutdown();
    havok_system_shutdown();
    scheduler_shutdown();
    cfile_system_shutdown();
    stats_shutdown_update_achievements_thread();
    animlib_system_close();
    zlib_shutdown();
    cloth_sim_system_shutdown();
    loading_system_shutdown();
    ui_shutdown();
}

// 0x007E9010 (:571)
void gamewide_system_init()
{
    multi_core_platform_register_login_changed_callback(gamewide_login_changed);
    Booted_to_main_menu.booted = false;
    Booted_to_main_menu.boot_cause = INVALID_BOOT_CAUSE;
    Booted_to_main_menu.msgbox_handle = -1;
    os_hook_message(prtsc_message_proc);
}

// 0x007D9390 (:596)
void gamewide_login_changed(bool only_changed_live_connectivity, bool remote_game_session_related)
{
    bool voice = true;
    if (remote_game_session_related)
    {
        game_save_write_profile_data();
        multi_core_matchmake_internal_stop_search_timer();
        boot_to_main_menu_music();
        boot_to_main_menu_record(BC_REMOTE_GAME_SESSION, false);
        voice = !only_changed_live_connectivity;
    }
    else if (!game_should_exit_due_to_profile_change())
    {
        // In the menus a login change only ends multiplayer (or the Wrecking Crew restart).
        if (Multiplayer)
        {
            game_set_multi_shutdown(GS_MAINMENU);
        }
        else if (wrecking_crew_should_restart())
        {
            wrecking_crew_should_restart(false);
        }
        voice = !only_changed_live_connectivity;
    }
    else if (only_changed_live_connectivity)
    {
        // Only the network went away: a multiplayer game without the network (system link) or the Internet ends.
        if (Multiplayer)
        {
            if (multi_is_syslink_game())
            {
                if (!multi_core_platform_is_connected_to_network())
                {
                    boot_to_main_menu_music();
                    boot_to_main_menu_record(BC_CONNECTION_LOST_SYSLINK, true);
                }
            }
            else if (!multi_core_platform_is_connected_to_internet())
            {
                boot_to_main_menu_music();
                boot_to_main_menu_record(BC_CONNECTION_LOST_XBL, true);
            }
        }
        voice = false;
    }
    else
    {
        boot_to_main_menu_music();
        boot_to_main_menu_record(BC_LOGIN_CHANGE, true);
    }
    if (voice)
    {
        multi_voice_login_changed();
    }
    if (!multi_core_platform_is_connected_to_internet())
    {
        multi_core_platform_clear_pending_invite();
    }
}

// 0x007C09C0 (:647)
bool game_should_exit_due_to_profile_change()
{
    if (Stack_top_index > -1)
    {
        switch (State_stack[Stack_top_index])
        {
        case GS_MAINMENU:
        case GS_BOOT:
        case GS_MULTI_SHUTDOWN:
        case GS_MAINMENU_IDLE:
        case GS_VIEW_DLC_SCREEN:
        case GS_DLC_SHUTDOWN:
            return Multiplayer;
        case GS_MAINMENU_OPTIONS:
            return !gameseq_state_is_active(GS_MAINMENU);
        case GS_RESOLUTION_CHANGE:
            return false;
        default:
            break;
        }
    }
    return true;
}

// 0x007C0A60 (:684)
bool game_booted_to_main_menu()
{
    return Booted_to_main_menu.booted;
}

// 0x007C0A70 (:690)
boot_to_main_menu_cause game_get_booted_to_main_menu_cause()
{
    return Booted_to_main_menu.booted ? Booted_to_main_menu.boot_cause : INVALID_BOOT_CAUSE;
}

// 0x007D96E0 (:700)
void game_check_for_gameplay_shutdown()
{
    const int num = State_request_queue.size();
    if (num > 0 && State_request_queue[num - 1].state == GS_LOAD)
    {
        return;
    }
    if (Gameplay_shutdown_in_progress && Stack_top_index > -1 && State_stack[Stack_top_index] == GS_LOAD)
    {
        World.abort_load();
        return;
    }
    if (Booted_to_main_menu.msgbox_handle != -1 && !ui_is_msgbox_active(Booted_to_main_menu.msgbox_handle))
    {
        Booted_to_main_menu.msgbox_handle = -1;
    }
    if (!Gameplay_shutdown_in_progress)
    {
        return;
    }
    if (!Multiplayer)
    {
        if (wrecking_crew_is_active() || gameseq_state_is_active(GS_WRECKING_CREW_MAIN_MENU))
        {
            game_set_wc_shutdown(GS_MAINMENU);
        }
        else if (World.dlc_bundle_id == -1 || Multiplayer || wrecking_crew_is_active())
        {
            const game_state state = gameseq_get_state();
            if (state != GS_MAINMENU && state != GS_MULTI_SHUTDOWN)
            {
                gameseq_set_state(GS_MAINMENU, true);
            }
        }
        else
        {
            const game_state state = gameseq_get_state();
            if (state != GS_MAINMENU && state != GS_DLC_SHUTDOWN && gameseq_get_pending_state() != GS_DLC_SHUTDOWN)
            {
                game_set_dlc_shutdown(GS_MAINMENU);
            }
        }
    }
    else if ((Stack_top_index < 0 || State_stack[Stack_top_index] != GS_MULTI_SHUTDOWN) && gameseq_get_pending_state() != GS_MULTI_SHUTDOWN)
    {
        game_set_multi_shutdown(GS_MAINMENU);
    }
    Gameplay_shutdown_in_progress = false;
}

// 0x007D9830 (:776)
void game_input_poll()
{
    controls_poll();
    if (Debug_key_down_last_frame && !Handled_debug_key)
    {
        if (console_is_active())
        {
            console_deactivate();
        }
        else
        {
            console_activate();
        }
    }
    if (multi_game_allow_chat_say() && controls_is_button_just_pressed(CBA_OPEN_CHAT, Player_controller_index))
    {
        if (!console_is_active())
        {
            console_activate();
            console_activate_chat_say();
        }
    }
    else if (multi_game_allow_chat_teamsay() && controls_is_button_just_pressed(CBA_OPEN_TEAM_CHAT, Player_controller_index) && !console_is_active() && !demolition_mode_is_vote_active())
    {
        console_activate();
        console_activate_chat_teamsay();
    }
    if (!Debug_key_down_last_frame)
    {
        Handled_debug_key = false;
    }
    if (os_should_close())
    {
        if (Stack_top_index > -1)
        {
            if (State_stack[Stack_top_index] == GS_LOAD)
            {
                Gameplay_shutdown_in_progress = true;
                Game_boot_exit_flag = true;
                return;
            }
            if (State_stack[Stack_top_index] == GS_SHUTDOWN)
            {
                return;
            }
        }
        const int num = State_request_queue.size();
        if (num < 1 || State_request_queue[num - 1].state != GS_SHUTDOWN)
        {
            gameseq_set_state(GS_SHUTDOWN, true);
        }
    }
}

// 0x007C0A90 (:901)
void game_print_screen(char* filename)
{
}

// 0x007C0AA0 (:925)
void game_do_pending_screenshot()
{
    if (!Screenshot_pending)
    {
        return;
    }
    timestamp_pause();
    game_enable_standard_access();
    if (Pending_screenshot_filename[0] == '\0')
    {
        get_next_screenshot_filename(Pending_screenshot_filename);
    }
    game_disable_standard_access();
    Screenshot_pending = false;
    timestamp_unpause();
}

// 0x007C0AE0 (:948)
void game_display_loading_screen()
{
    Common_mempool.clear();
}
