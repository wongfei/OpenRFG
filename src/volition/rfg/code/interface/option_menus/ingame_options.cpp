// Original: volition/rfg/code/interface/option_menus/ingame_options.cpp
//
// The in-game options (GS_INGAME_OPTIONS, the Esc menu, UI.md UI6) on rfg_ui_ingame_options: resume, the handbook,
// statistics, the option menus (all options, or controls/display/game/audio directly), save and load, quit (with its
// confirmation box) and cancel mission. Single player pauses the game under it and blurs the screen; multiplayer keeps
// running and only locks the local player's controls. Leaving goes through the handshake, which quits to the main menu,
// restarts or cancels the mission when the matching box was accepted, else pops the state.

#include "volition/rfg/code/interface/option_menus/ingame_options.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_party.h"

#include "keen/base/trace.hpp"
#include "keen/input/inputsystem.hpp"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/cutscene/video_cutscene.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gamewide/game_options.h"
#include "volition/rfg/code/gamewide/game_save.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/hud/hud_ui_handbook.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/vlib/code/cfile/vdir.h"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/input/input_internal.h"
#include "volition/vlib/code/input/mouse.h"

// ---- file-private types (rfg.pdb)
enum ingame_msg_box_types
{
    INGAME_OPTION_MSGBOX_NONE = -1,
    INGAME_OPTION_MSGBOX_QUIT = 0,
    INGAME_OPTION_MSGBOX_RESTART = 1,
    INGAME_OPTION_MSGBOX_CANCEL_MISSION = 2,
};

// The menu's items, in creation order (rfg.pdb enum).
enum ingame_option_types
{
    INGAME_OPTIONS_RESUME = 0,
    INGAME_OPTION_HANDBOOK = 1,
    INGAME_OPTION_STATS = 2,
    INGAME_OPTION_OPTIONS = 3,
    INGAME_OPTION_CONTROL_SETTINGS = 4,
    INGAME_OPTION_DISPLAY_SETTINGS = 5,
    INGAME_OPTION_GAME_SETTINGS = 6,
    INGAME_OPTION_SOUND_SETTINGS = 7,
    INGAME_OPTION_SAVE_GAME = 8,
    INGAME_OPTION_LOAD_GAME = 9,
    INGAME_OPTION_QUIT_TO_MAIN = 10,
    INGAME_OPTION_CANCEL_MISSION = 11,
};

// ---- file-static globals (statics of rfg.exe's interface unity; the three the option menus share are external)
int Doc_handle_ioptions = -1; // 0x01664104
int Menu_handle_ioptions = -1; // 0x01664108
static int Msg_box_handle = -1; // 0x0166410C
static ingame_msg_box_types Msg_box_type = INGAME_OPTION_MSGBOX_NONE; // 0x01664110
int Control_id_ioptions = -1; // 0x01664114
static int Input_id_ioptions = -1; // 0x01664118
static bool Ingame_options_auto_catch_done = true; // 0x0166411C
static int Message_box_handle_ioptions = -1; // 0x01664120
static const char* cMenu_group_name_ioptions = {"text_group"}; // 0x01664124
// The controller type the hints were made for (initial 16, no type).
static keen::ControllerType Ingame_options_controller_type = (keen::ControllerType)16; // 0x01664128
static bool Save_on_return; // 0x02C03750
static bool Assets_loaded_ioptions; // 0x02C03751
static bool No_fade_out; // 0x02C03752
static bool Exiting_ioptions; // 0x02C03753
extern bool Active; // 0x02C03754, ui_document_hud_tool_tips.cpp (shared static of the interface unity)
// NVIDIA Highlights (rfg.cpp, not declared in a recovered header).
void nvidia_highlights_open_summary(const char* groupId); // 0x005D4230
bool nvidia_highlights_is_enabled(); // 0x005D13B0
unsigned int nvidia_highlights_get_num_highlights(); // 0x005D13F0
static const char* s_groupIdGameplay = {"gameplay"}; // 0x01663180 (the main menu's, shared in the interface unity)
static bool Just_loaded; // 0x02C03755
static int Last_menu_entered; // 0x02C03758

// ---- internal functions

// The selected item of the menu (-1 without the menu), kept for when the menu comes back (the inlined lookup).
static void ingame_options_remember_selection()
{
    Last_menu_entered = ui_menu_get_selected_item(Menu_handle_ioptions);
}

// 0x00917570 (:201)
// The document unloads (a slower fade after a confirmed box), the HUD comes back (single player, or in a match), the
// menu goes; the handshake then leaves.
static void ingame_options_request_exit()
{
    if (Exiting_ioptions)
    {
        return;
    }
    if (Doc_handle_ioptions != -1)
    {
        // Magic numbers: 0.5 s with no fade (the box confirmed), else a 0.05 s alpha fade.
        ui_unload_document(Doc_handle_ioptions, No_fade_out ? DFOT_NONE : DFOT_ALPHA, No_fade_out ? 0.5f : 0.05f);
        Doc_handle_ioptions = -1;
        ui_force_unloads_complete();
    }
    if ((!Multiplayer && !multi_core_platform_has_pending_invite()) || (Multiplayer && Lobby_session->get_state() == MSS_IN_GAME))
    {
        hud_ui_reload_items();
    }
    ingame_options_destroy_menu();
    Last_menu_entered = 0;
    Exiting_ioptions = true;
}

// 0x00890F80 (:232)
static bool ingame_options_auto_catch_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Msg_box_handle == mbox_handle && !Ingame_options_auto_catch_done)
    {
        ingame_options_start_auto_catch(true);
    }
    return false;
}

// 0x008E7480 (:262)
// Saving during an activity: accepted opens the save screen.
static bool save_game_check_should_proceed_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Message_box_handle_ioptions != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        Save_load_ui_type = SLUT_SAVE;
        gameseq_push_state(GS_SAVE_LOAD_SCREEN, true, false);
        ingame_options_destroy_menu();
    }
    else if ((int)choice < 1 || (int)choice > 2)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    ui_wait_for_button_ups();
    return true;
}

// 0x00917630 (:313)
// The quit/cancel mission box: accepted leaves (without a fade), else the box type is cleared.
static bool ingame_options_confirm_msgbox_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Msg_box_handle != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        ingame_options_request_exit();
        No_fade_out = true;
        Msg_box_handle = -1;
        return true;
    }
    if ((int)choice > 0 && (int)choice < 3)
    {
        Msg_box_type = INGAME_OPTION_MSGBOX_NONE;
        Msg_box_handle = -1;
        return true;
    }
    // Original: an unknown choice keeps the box type.
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    Msg_box_handle = -1;
    return true;
}

// 0x009176B0 (:373)
// Quit Game: the confirmation box (its text by mode), then the menu goes until the box is answered.
static void mainmenu_quit_to_mainmenu_callback(int delta)
{
    const char* text;
    if (Multiplayer)
    {
        // Original: a multiplayer game without a local player gets the wrecking crew text.
        text = player_get_local_player() != nullptr ? "MENU_QUIT_CONFIRM_MP" : "MENU_QUIT_WRECKING_CREW";
    }
    else
    {
        text = wrecking_crew_is_active() ? "MENU_QUIT_WRECKING_CREW" : "MENU_QUIT_CONFIRM";
    }
    const int handle = ui_add_msgbox(MBT_ACCEPT_CANCEL_SELECT, "MENU_WARNING", text, ingame_options_confirm_msgbox_cb, false, false, nullptr, nullptr, false);
    Msg_box_type = INGAME_OPTION_MSGBOX_QUIT;
    ingame_options_remember_selection();
    Msg_box_handle = handle;
    ingame_options_destroy_menu();
}

// 0x008C3DB0 (:400)
// All options (the menu stays).
static void options_callback(int delta)
{
    ingame_options_remember_selection();
    gameseq_push_state(GS_MAINMENU_OPTIONS, true, false);
}

// 0x008D6AC0 (:407)
static void stats_menu_callback(int delta)
{
    gameseq_push_state(GS_STATS_SCREEN, true, false);
    ingame_options_remember_selection();
    ingame_options_destroy_menu();
}

// 0x00917A90 (:415)
static void mainmenu_resume_mission_callback(int delta)
{
    if (gameseq_get_state() == GS_INGAME_OPTIONS)
    {
        ingame_options_request_exit();
    }
}

// An option menu pushed above this one (the menu goes, its selection kept).
static void ingame_options_push_option_menu(game_state state)
{
    gameseq_push_state(state, true, false);
    ingame_options_remember_selection();
    ingame_options_destroy_menu();
}

// 0x008D6B30 (:423)
static void control_settings(int delta)
{
    ingame_options_push_option_menu(GS_CONTROLLER_OPTIONS);
}

// 0x008D6BA0 (:431)
static void display_settings(int delta)
{
    ingame_options_push_option_menu(GS_MAINMENU_OPTIONS_DISPLAY);
}

// 0x008D6C10 (:439)
static void game_settings(int delta)
{
    ingame_options_push_option_menu(GS_MAINMENU_OPTIONS_GAME_OPTIONS);
}

// 0x008D6C80 (:448)
static void sound_levels(int delta)
{
    ingame_options_push_option_menu(GS_MAINMENU_OPTIONS_AUDIO);
}

// 0x008D6CF0 (:456)
static void handbook_callback(int delta)
{
    hud_ui_handbook_show();
    ingame_options_remember_selection();
    ingame_options_destroy_menu();
}

// 0x00901080 (:464)
// Saving: the save screen, after a box during an activity; during a mission only a box (whose accept also opens the
// save screen).
static void save_game_callback(int delta)
{
    ingame_options_remember_selection();
    ingame_options_destroy_menu();
    if (!game_save_is_saving_enbaled())
    {
        return;
    }
    if (mission_is_active())
    {
        Message_box_handle_ioptions = ui_add_msgbox(MBT_ACCEPT_CANCEL_SELECT, "MENU_WARNING", "MENU_SAVELOAD_DURING_MISSION_MSG", save_game_check_should_proceed_cb, false, false, nullptr, nullptr, false);
        return;
    }
    if (activity_none())
    {
        Save_load_ui_type = SLUT_SAVE;
        gameseq_push_state(GS_SAVE_LOAD_SCREEN, true, false);
        return;
    }
    Message_box_handle_ioptions = ui_add_msgbox(MBT_ACCEPT_CANCEL_SELECT, "MENU_WARNING", "MENU_SAVELOAD_DURING_ACTION_MSG", save_game_check_should_proceed_cb, false, false, nullptr, nullptr, false);
}

// 0x008D6D60 (:480)
static void load_game_callback(int delta)
{
    ingame_options_remember_selection();
    ingame_options_destroy_menu();
    if (game_save_is_saving_enbaled())
    {
        Save_load_ui_type = SLUT_LOAD;
        gameseq_push_state(GS_SAVE_LOAD_SCREEN, true, false);
        ingame_options_destroy_menu();
    }
}

// 0x00917AA0 (:516)
static void mainmenu_cancel_mission_callback(int delta)
{
    const int handle = ui_add_msgbox(MBT_ACCEPT_CANCEL_SELECT, "MENU_WARNING", "MENU_CANCEL_MISSION_CONFIRM", ingame_options_confirm_msgbox_cb, false, false, "MENU_CANCEL_MISSION", "MENU_CONTINUE", false);
    Msg_box_type = INGAME_OPTION_MSGBOX_CANCEL_MISSION;
    ingame_options_remember_selection();
    Msg_box_handle = handle;
    ingame_options_destroy_menu();
}

// 0x00917E40 (:535)
// Back resumes; the misc hint switches the input device; the second misc hint opens the highlights.
static void ingame_options_hint_callback(hint_type hint)
{
    if (hint == HT_ABORT)
    {
        if (gameseq_get_state() == GS_INGAME_OPTIONS)
        {
            ingame_options_request_exit();
        }
    }
    else if (hint == HT_MISC)
    {
        if (Ingame_options_auto_catch_done)
        {
            ingame_options_start_auto_catch(false);
        }
    }
    else if (hint == HT_MISC_2)
    {
        nvidia_highlights_open_summary(s_groupIdGameplay);
    }
}

// A hint text: the tag's localized text with its button markup.
static void ingame_options_fill_hint(char* text, const char* tag)
{
    // Magic number: 0x100, the hint buffers' size.
    controls_fill_options_button_hint(text, 0x100, localization_get_display_string_req(tag, nullptr));
}

// 0x008E7510 (:559)
// Select and resume (by the controller's buttons), switch input device (not in wrecking crew), the highlights (when
// there are some).
static void ingame_options_create_button_hints()
{
    char text[256];
    char image[256];
    ui_menu* menu = ui_menu_get_via_handle(Menu_handle_ioptions);
    if (menu != nullptr)
    {
        for (int hint = HT_SELECTION; hint < 4; hint++)
        {
            menu->remove_hint((hint_type)hint);
        }
    }
    ingame_options_fill_hint(text, "MENU_SELECT_WITH_BUTTON");
    menu = ui_menu_get_via_handle(Menu_handle_ioptions);
    if (menu != nullptr)
    {
        // Original: 0x40 passed as the image buffer's size.
        controls_get_button_image_name(image, 0x40, CBA_MENU_SELECT, Player_controller_index);
        menu->add_hint(HT_SELECTION, CBA_MENU_SELECT, text, image);
    }
    ingame_options_fill_hint(text, "MENU_RESUME_WITH_BUTTON");
    menu = ui_menu_get_via_handle(Menu_handle_ioptions);
    if (menu != nullptr)
    {
        controls_get_button_image_name(image, 0x40, CBA_MENU_ABORT, Player_controller_index);
        menu->add_hint(HT_ABORT, CBA_MENU_ABORT, text, image);
    }
    if (!wrecking_crew_is_active())
    {
        ingame_options_fill_hint(text, "MENU_SWITCH_INPUT_DEVICE");
        ui_menu_add_misc_hint(Menu_handle_ioptions, text, CBA_MENU_X);
    }
    if (!nvidia_highlights_is_enabled() || nvidia_highlights_get_num_highlights() == 0)
    {
        return;
    }
    ingame_options_fill_hint(text, "MENU_NVIDIA_WITH_BUTTON");
    ui_menu_add_misc_hint_2(Menu_handle_ioptions, text, CBA_MENU_Y);
}

// ---- external interface

// 0x00890F30 (:172)
// Not during a mission, an orange/red alert, demolitions master or with cheats; then when saving is enabled.
bool ingame_options_should_show_save_game()
{
    if (mission_is_active() || gsm_get_alert_level() == ALERT_LEVEL_ORANGE || gsm_get_alert_level() == ALERT_LEVEL_RED || activity_running_or_shutting_down(ACTIVITY_MODE_DEMOLITIONS_MASTER) || cheats_are_enabled())
    {
        return false;
    }
    return game_save_is_saving_enbaled();
}

// 0x00900D00 (:245)
// The controller select box (not when retrying, the box is up), then keen input catches the next button's device.
void ingame_options_start_auto_catch(bool retry)
{
    if (!retry)
    {
        Msg_box_handle = ui_add_msgbox(MBT_CONFIG_CONTROL, "SESSION_INTERACTION_SELECT_CONTROLLER_TITLE", "SESSION_INTERACTION_SELECT_CONTROLLER", ingame_options_auto_catch_cb, false, false, nullptr, nullptr, false);
    }
    // Magic number: no keen controller.
    Cached_input_states[Current_cached_input_state].controllers[0].keen_controller_index = 0xFFFFFFFF;
    Ingame_options_auto_catch_done = false;
    Cached_input_states[Next_cached_input_state].controllers[0].keen_controller_index = 0xFFFFFFFF;
    keen::input::clearControllerPlayerId(Keen_input_system, nullptr);
    keen::input::setControllerAutoCatch(Keen_input_system, nullptr, keen::InputSystemControllerAutoCatchType_AnyButton);
}

// 0x008D6AA0 (:305)
void ingame_options_destroy_menu()
{
    if (Menu_handle_ioptions != -1)
    {
        ui_menu_destroy(Menu_handle_ioptions);
        Menu_handle_ioptions = -1;
    }
}

// 0x00917E90 (:591)
// The twelve items; cancel mission only during a non-tutorial, non-cutscene, not autostarted mission; save off when
// not allowed. Multiplayer and wrecking crew drop save, load, the handbook, the option menus but controls, statistics
// and the game options (and quit, in multiplayer outside a match); single player drops the four option items (all
// through "options"), the handbook in the tutorial, the handbook and statistics in DLC.
void ingame_options_create_menu(bool animate_in)
{
    if (Menu_handle_ioptions != -1)
    {
        return;
    }
    // Magic number: -1, no item count limit.
    Menu_handle_ioptions = ui_menu_add(Doc_handle_ioptions, "MENU_PAUSE", nullptr, ingame_options_hint_callback, "_ingame", animate_in, -1, Input_id_ioptions, false);
    ui_menu_add_function_item(Menu_handle_ioptions, "MENU_RESUME", mainmenu_resume_mission_callback, true, false, false);
    ui_menu_add_function_item(Menu_handle_ioptions, "MENU_HANDBOOK", handbook_callback, true, false, false);
    ui_menu_add_function_item(Menu_handle_ioptions, "MENU_STATISTICS", stats_menu_callback, true, false, false);
    ui_menu_add_function_item(Menu_handle_ioptions, "MENU_OPTIONS", options_callback, false, false, false);
    ui_menu_add_function_item(Menu_handle_ioptions, "MENU_CONTROLS", control_settings, true, false, false);
    ui_menu_add_function_item(Menu_handle_ioptions, "MENU_DISPLAY", display_settings, true, false, false);
    ui_menu_add_function_item(Menu_handle_ioptions, "MENU_GAME_OPTIONS", game_settings, true, false, false);
    ui_menu_add_function_item(Menu_handle_ioptions, "MENU_AUDIO", sound_levels, true, false, false);
    ui_menu_add_function_item(Menu_handle_ioptions, "MENU_SAVE_GAME", save_game_callback, true, false, false);
    ui_menu_add_function_item(Menu_handle_ioptions, "MENU_LOAD_GAME", load_game_callback, true, false, false);
    ui_menu_add_function_item(Menu_handle_ioptions, "MENU_QUIT_GAME", mainmenu_quit_to_mainmenu_callback, true, false, false);
    ui_menu_add_function_item(Menu_handle_ioptions, "MENU_CANCEL_MISSION", mainmenu_cancel_mission_callback, true, false, false);
    ingame_options_create_button_hints();
    bool show_cancel_mission = false;
    if (mission_is_active() && mission_get_mode() != MISSION_MODE_TUTORIAL && mission_get_mode() != MISSION_MODE_CUTSCENE)
    {
        show_cancel_mission = !(mission_is_active() && mission_autostarted());
    }
    ui_menu_show_menu_item(Menu_handle_ioptions, INGAME_OPTION_CANCEL_MISSION, show_cancel_mission);
    ui_menu_set_selected_item(Menu_handle_ioptions, Last_menu_entered);
    if (!ingame_options_should_show_save_game())
    {
        ui_menu_enable_menu_item(Menu_handle_ioptions, INGAME_OPTION_SAVE_GAME, false);
    }
    if (Multiplayer || wrecking_crew_is_active())
    {
        const int hidden[6] = {INGAME_OPTION_SAVE_GAME, INGAME_OPTION_LOAD_GAME, INGAME_OPTION_HANDBOOK, INGAME_OPTION_OPTIONS, INGAME_OPTION_STATS, INGAME_OPTION_GAME_SETTINGS};
        for (int i = 0; i < 6; i++)
        {
            ui_menu_enable_menu_item(Menu_handle_ioptions, hidden[i], false);
            ui_menu_show_menu_item(Menu_handle_ioptions, hidden[i], false);
        }
        if (Multiplayer && Lobby_session->get_state() != MSS_IN_GAME)
        {
            ui_menu_enable_menu_item(Menu_handle_ioptions, INGAME_OPTION_QUIT_TO_MAIN, false);
            ui_menu_show_menu_item(Menu_handle_ioptions, INGAME_OPTION_QUIT_TO_MAIN, false);
        }
    }
    else
    {
        const int hidden[4] = {INGAME_OPTION_CONTROL_SETTINGS, INGAME_OPTION_DISPLAY_SETTINGS, INGAME_OPTION_GAME_SETTINGS, INGAME_OPTION_SOUND_SETTINGS};
        for (int i = 0; i < 4; i++)
        {
            ui_menu_enable_menu_item(Menu_handle_ioptions, hidden[i], false);
            ui_menu_show_menu_item(Menu_handle_ioptions, hidden[i], false);
        }
        if (mission_is_active() && mission_get_mode() == MISSION_MODE_TUTORIAL)
        {
            ui_menu_enable_menu_item(Menu_handle_ioptions, INGAME_OPTION_HANDBOOK, false);
            ui_menu_show_menu_item(Menu_handle_ioptions, INGAME_OPTION_HANDBOOK, false);
        }
        if (World.dlc_bundle_id != -1)
        {
            ui_menu_enable_menu_item(Menu_handle_ioptions, INGAME_OPTION_HANDBOOK, false);
            ui_menu_show_menu_item(Menu_handle_ioptions, INGAME_OPTION_HANDBOOK, false);
            ui_menu_enable_menu_item(Menu_handle_ioptions, INGAME_OPTION_STATS, false);
            ui_menu_show_menu_item(Menu_handle_ioptions, INGAME_OPTION_STATS, false);
        }
    }
    audiolib_cue_play(Ga_cue_ids.general_pause_menu_open, nullptr, nullptr);
}

// 0x00919850 (:715)
void ingame_options_init()
{
    state_info info;
    info.hide_cursor = false;
    info.enter = ingame_options_enter;
    info.exit = ingame_options_exit;
    info.process = ingame_options_process;
    info.handshake = ingame_options_handshake;
    info.render = ingame_options_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_INGAME_OPTIONS, info);
}

// 0x0090F590 (:731)
// The document (no document: back out at once), the HUD hidden, single player paused and blurred (multiplayer: the
// jetpack stopped), the local player's controls locked, a control id and an input id.
void ingame_options_enter(game_state prev_state)
{
    Assets_loaded_ioptions = false;
    Save_on_return = false;
    No_fade_out = true;
    Exiting_ioptions = false;
    Active = true;
    Msg_box_type = INGAME_OPTION_MSGBOX_NONE;
    Ingame_options_auto_catch_done = true;
    Just_loaded = true;
    Doc_handle_ioptions = ui_load_document("rfg_ui_ingame_options", DFIT_NONE, 0.0f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (Doc_handle_ioptions == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        gameseq_pop_state();
        return;
    }
    hud_ui_unload_items();
    if (!Multiplayer)
    {
        ui_set_screen_blur(true, true);
    }
    else
    {
        player* local = player_get_local_player();
        if (local != nullptr)
        {
            player_update_jetpack(local, false);
        }
    }
    hud_display_firearm_interface(false);
    player_lock_local_player_controls();
    Control_id_ioptions = ui_request_control_id();
    Input_id_ioptions = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    input_set_feedback(0, 0.0f, 0.0f);
}

// 0x009185B0 (:784)
// The hints under the mouse (back, switch device, highlights), then the menu items; a released click selects.
void ingame_options_process_mouse_input()
{
    const char* names[4] = {"hints", "hints2", "hints3", "hints4"};
    const int hints[4] = {HT_SELECTION, HT_ABORT, HT_MISC, HT_MISC_2};
    vint_object_base* objects[4] = {nullptr, nullptr, nullptr, nullptr};
    vint_doc_info* info = ui_get_doc_info(Doc_handle_ioptions);
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
        if (hint == HT_ABORT)
        {
            if (gameseq_get_state() == GS_INGAME_OPTIONS)
            {
                ingame_options_request_exit();
            }
        }
        else if (hint == HT_MISC)
        {
            if (Ingame_options_auto_catch_done)
            {
                ingame_options_start_auto_catch(false);
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
    int index = ui_menu_process_mouseover_extend(Doc_handle_ioptions, Menu_handle_ioptions, cMenu_group_name_ioptions, nullptr, Just_loaded);
    if (index < 0)
    {
        // The select hint clicked selects the current item.
        if (clicked > -1 && hints[clicked] == HT_SELECTION)
        {
            index = ui_menu_get_selected_item(Menu_handle_ioptions);
        }
        if (index < 0)
        {
            Just_loaded = false;
            return;
        }
    }
    if (Mouse_buttons[0].just_released)
    {
        ui_menu_selection_clicked(Menu_handle_ioptions, index, false, false);
    }
    Just_loaded = false;
}

// 0x00918720 (:822)
// The device catch finishing, the hints remade for a new controller type, a pending invite quitting (single player),
// the controls kept locked, the document loading, then the menu (with no box up), the save/load items, the game
// options, the pause button (button 29) resuming, the mouse.
void ingame_options_process()
{
    if (!Ingame_options_auto_catch_done && keen::input::getPrimaryInputDeviceClass(Keen_input_system, nullptr) != keen::ControllerClass_None)
    {
        Ingame_options_auto_catch_done = true;
        keen::input::setControllerAutoCatch(Keen_input_system, nullptr, keen::InputSystemControllerAutoCatchType_Disabled);
        if (Msg_box_handle != -1)
        {
            ui_hide_msgbox(Msg_box_handle, true);
            Msg_box_handle = -1;
        }
    }
    if (Exiting_ioptions)
    {
        ui_process(Control_id_ioptions, false);
        return;
    }
    if (Ingame_options_controller_type != Cached_input_states[Current_cached_input_state].controllers[0].controller_type)
    {
        ingame_options_create_button_hints();
        Ingame_options_controller_type = Cached_input_states[Current_cached_input_state].controllers[0].controller_type;
    }
    if (!Multiplayer)
    {
        if (multi_core_platform_has_pending_invite())
        {
            // A box other than the quit box goes; with no box left, quit (the quit box's answer otherwise).
            if (Utility_doc_handle != -1 && ui_is_msgbox_active(-2) && Msg_box_type != INGAME_OPTION_MSGBOX_QUIT)
            {
                ui_hide_msgbox(Msg_box_handle, false);
            }
            if (Utility_doc_handle == -1 || !ui_is_msgbox_active(-2))
            {
                mainmenu_quit_to_mainmenu_callback(0);
            }
        }
    }
    if (Multiplayer && !player_is_local_player_controls_locked())
    {
        player_lock_local_player_controls();
    }
    int top_input_id = Current_input_lock_id;
    if (Current_input_lock_id == -1)
    {
        top_input_id = Input_types.size() - 1 < 0 ? 1 : Input_types[Input_types.size() - 1].id;
    }
    if (Input_id_ioptions == top_input_id)
    {
        input_set_feedback(0, 0.0f, 0.0f);
    }
    if (!Assets_loaded_ioptions)
    {
        switch (ui_get_doc_asset_state(Doc_handle_ioptions))
        {
        case ST_LOADING:
            break;
        case ST_LOADED:
            Assets_loaded_ioptions = true;
            break;
        case ST_UNLOADED:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            ingame_options_request_exit();
            break;
        case ST_ERROR:
            ingame_options_request_exit();
            break;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
        if (!Assets_loaded_ioptions)
        {
            ui_process(Control_id_ioptions, false);
            return;
        }
    }
    if (Menu_handle_ioptions == -1 && gameseq_get_state() == GS_INGAME_OPTIONS && (Utility_doc_handle == -1 || !ui_is_msgbox_active(-2)))
    {
        ingame_options_create_menu(false);
    }
    if (gameseq_get_state() == GS_INGAME_OPTIONS)
    {
        if (!Multiplayer)
        {
            if (!wrecking_crew_is_active() && !ingame_options_should_show_save_game())
            {
                ui_menu_enable_menu_item(Menu_handle_ioptions, INGAME_OPTION_SAVE_GAME, false);
            }
            if (!wrecking_crew_is_active() && !game_save_is_saving_enbaled())
            {
                ui_menu_enable_menu_item(Menu_handle_ioptions, INGAME_OPTION_LOAD_GAME, false);
            }
        }
        game_options_process();
        if (!Exiting_ioptions && (Utility_doc_handle == -1 || !ui_is_msgbox_active(-2)))
        {
            // Magic number: button 29, pause.
            if (ui_get_button_info(29, Input_id_ioptions) > 0.0f && gameseq_get_state() == GS_INGAME_OPTIONS)
            {
                ingame_options_request_exit();
            }
            if (Menu_handle_ioptions != -1)
            {
                if (!ui_menu_get_tween_on_select_end(Menu_handle_ioptions) && !ui_menu_is_fading_in(Menu_handle_ioptions))
                {
                    ingame_options_process_mouse_input();
                }
                ui_add_for_processing(Doc_handle_ioptions);
            }
        }
    }
    ui_process(Control_id_ioptions, false);
}

// 0x008AF5E0 (:993)
// Leaving (single player waits for the HUD to be back): the quit, restart and cancel mission boxes' actions, else the
// pop.
void ingame_options_handshake(rl_renderer*)
{
    if (!Exiting_ioptions)
    {
        return;
    }
    if (!Multiplayer && !multi_core_platform_has_pending_invite() && !hud_ui_is_loaded())
    {
        return;
    }
    if (Msg_box_type == INGAME_OPTION_MSGBOX_QUIT)
    {
        ingame_options_exit_to_main_menu();
        return;
    }
    if (Msg_box_type == INGAME_OPTION_MSGBOX_RESTART)
    {
        vdir_restore(PLATFORM_PC, true);
        World.set_pending_reload();
        return;
    }
    if (Msg_box_type == INGAME_OPTION_MSGBOX_CANCEL_MISSION)
    {
        mission_cancel();
        return;
    }
    gameseq_pop_state();
}

// 0x00901480 (:1021)
void ingame_options_render(rl_renderer*)
{
    if (Assets_loaded_ioptions && !Exiting_ioptions)
    {
        ui_add_for_rendering(Doc_handle_ioptions);
    }
    ui_render(Control_id_ioptions, false);
}

// 0x00918A70 (:1031)
// The menu and the document go (the document's handle is kept), the ids, the controls and the HUD come back, the blur
// ends (single player).
void ingame_options_exit(game_state new_state)
{
    ingame_options_destroy_menu();
    if (Doc_handle_ioptions != -1)
    {
        // Original: Doc_handle_ioptions is not reset here.
        ui_unload_document(Doc_handle_ioptions, No_fade_out ? DFOT_NONE : DFOT_ALPHA, No_fade_out ? 0.5f : 0.05f);
    }
    ui_relinquish_control_id((unsigned int)Control_id_ioptions);
    ui_relinquish_input_id(Input_id_ioptions);
    Control_id_ioptions = -1;
    Menu_handle_ioptions = -1;
    player_unlock_local_player_controls();
    hud_ui_reload_items();
    hud_display_firearm_interface(true);
    if (!Multiplayer)
    {
        ui_set_screen_blur(false, true);
    }
    Active = false;
    input_reset_gyro_orientation_origin();
}

// 0x008D6E10 (:1068)
void ingame_options_show_menu(bool show)
{
    vint_doc_info* info = ui_get_doc_info(Doc_handle_ioptions);
    if (info != nullptr && info->m_doc != nullptr)
    {
        vint_object_base* group = vint_object_base::find_object("screen_group", info->m_doc);
        if (group != nullptr)
        {
            group->set_property_value("visible", vint_variant(show), 0);
        }
    }
}

// 0x008910D0 (:1083)
bool ingame_options_is_active()
{
    return Active;
}

// 0x008910E0 (:1089)
int ingame_options_get_control_id()
{
    return Control_id_ioptions;
}

// 0x008910F0 (:1095)
int ingame_options_get_doc_handle()
{
    return Doc_handle_ioptions;
}

// 0x00890FB0 (:944)
// Quit Game: the frontend music, every sound stopped; in multiplayer the loss for leaving early, the party left, the
// lobby session deleted and the leave marked as the player's choice; then the main menu (through the DLC or wrecking
// crew switch when one of those is running, unless a level load is under way).
void ingame_options_exit_to_main_menu()
{
    gamemusic_frontend_start(Multiplayer || multi_core_platform_has_pending_invite());
    audiolib_all_sounds_stop(true, false);
    if (Multiplayer)
    {
        multi_game_mode_common_report_loss_for_leaving_early();
        if (multi_game_party_in_party())
        {
            multi_game_party_leave_party(true);
        }
        if (Lobby_session != nullptr && Lobby_session->get_state() != MSS_NONE)
        {
            Lobby_session->session_delete();
        }
        Multi_left_game_by_choice = true;
    }
    if (wrecking_crew_is_active())
    {
        wrecking_crew_should_restart(!multi_core_platform_has_pending_invite());
    }
    game_state switch_state;
    if (World.dlc_bundle_id == -1 || Multiplayer || wrecking_crew_is_active())
    {
        if (!wrecking_crew_is_active() && !gameseq_state_is_active(GS_WRECKING_CREW_MAIN_MENU))
        {
            gameseq_set_state(GS_MAINMENU, false);
            return;
        }
        if (gameseq_get_state() == GS_LOAD || gameseq_get_pending_state() == GS_LOAD)
        {
            return;
        }
        switch_state = GS_WC_SHUTDOWN;
    }
    else
    {
        if (gameseq_get_state() == GS_LOAD || gameseq_get_pending_state() == GS_LOAD)
        {
            return;
        }
        switch_state = GS_DLC_SHUTDOWN;
    }
    Gs_switch_next_state = GS_MAINMENU;
    gameseq_set_state(switch_state, true);
}
