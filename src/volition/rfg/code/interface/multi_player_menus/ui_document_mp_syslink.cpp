// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_syslink.cpp
//
// The system link menu (GS_MULTIPLAYER_SYSLINK, UI.md UI5) on rfg_ui_mp_shell: host a game (a lobby created, the game
// lobby pushed), find games (the server browser), customization (player select), statistics (stats and the
// experience tally) and the training videos; back leaves multiplayer for the main menu. The menu's state changes
// (load, unload with its exit action) happen in the handshake. Entered after a match with the lobby still up, it goes
// on to the game lobby.

#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_syslink.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/gamewide.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/util/varray.h"

// ---- file-private types (rfg.pdb)
// size 0x8
struct menu_option_info_syslink
{
    const char* m_option; // 0x00
    const char* m_description; // 0x04
};
static_assert(sizeof(menu_option_info_syslink) == 0x8, "menu_option_info_syslink");

enum mp_menu_exit_actions_syslink
{
    MMEA_SY_POP = 0,
    MMEA_SY_PUSH = 1,
    NUM_MP_SY_MENU_EXIT_ACTIONS = 2,
};

enum mp_menu_states_syslink
{
    MMS_SY_LOADING = 0,
    MMS_SY_LOADED = 1,
    MMS_SY_UNLOADING = 2,
    MMS_SY_UNLOADED = 3,
    NUM_MP_SY_MENU_STATES = 4,
};

// size 0x74
struct mp_syslink_shell_info
{

    mp_menu_states_syslink state; // 0x00
    mp_menu_states_syslink pending_state; // 0x04
    mp_menu_states_syslink prev_state; // 0x08
    mp_menu_exit_actions_syslink exit_action; // 0x0C
    game_state exit_push_target; // 0x10
    int doc_handle; // 0x14
    int data_group_handle; // 0x18
    farray<int,16> group_item_handles; // 0x1C
    unsigned int control_id; // 0x6C
    int input_id; // 0x70
};
static_assert(sizeof(mp_syslink_shell_info) == 0x74, "mp_syslink_shell_info");

// ---- file-static globals
// The menu's entries (rfg.exe's .rdata).
static const menu_option_info_syslink Menu_options_syslink[5] = {
    {"MENU_HOST_GAME", "MENU_HOST_GAME_DESC"},
    {"MENU_FIND_GAME", "MENU_FIND_GAME_DESC"},
    {"MENU_CUSTOMIZATION_OPTION", "MENU_CHOOSE_PLAYER_DESC"},
    {"MENU_STATISTICS", "MENU_STATISTICS_DESC"},
    {"MENU_TRAINING_VIDEOS", "MENU_TRAINING_VIDEOS_DESC"},
}; // 0x01300748
// bool s_localization_is_loading (0x01663023) is used here too; placed in localization_shared.h.
// game_state Gs_switch_next_state (0x01663110) is used here too; placed in loading.h.
// const char* Mp_shell_doc_name (0x0166320C) is used here too; placed in multi_player_menu_system.cpp.
// int Mp_shell_doc_handle (0x0166325C) is used here too; placed in multi_player_menu_system.cpp.
// int Mp_msgbox_handle (0x016632E0) is used here too; placed in ui_document_hud_multiplayer.cpp.
static void syslink_process_data_item_hint_text(vint_data_item* vdi);
static void syslink_process_data_item_titles(vint_data_item* vdi);
static void syslink_process_data_item_info_text(vint_data_item* vdi);
static void syslink_process_data_item_selection_info(vint_data_item* vdi);
static void syslink_process_data_item_menu_type(vint_data_item* vdi);
static mp_shell_data_item_info Syslink_data_items[5] = {
    {MPSDIT_HINT_INFO, syslink_process_data_item_hint_text},
    {MPSDIT_TITLES, syslink_process_data_item_titles},
    {MPSDIT_INFO_TEXT, syslink_process_data_item_info_text},
    {MPSDIT_SELECTION_INFO, syslink_process_data_item_selection_info},
    {MPSDIT_MENU_TYPE, syslink_process_data_item_menu_type},
}; // 0x01663C7C
static const char* Back_string_syslink = {"MENU_BACK"}; // 0x01663CA4
// int Utility_doc_handle (0x01664FF8) is used here too; placed in loading.cpp.
// bink_data Bink_info_main (0x01666148) is used here too; placed in main_menu.cpp.
static mp_syslink_shell_info Mp_syslink_info; // 0x01666AB0
// farray<unsigned int,16> Controller_slots (0x01667378) is used here too; placed in loading.cpp.
// farray<input_request_data,16> Input_types (0x016673C8) is used here too; placed in loading.cpp.
// multi_player_uid Playercard_player_uid (0x02BE9E14) is used here too; placed in multi_player_menu_system.cpp.
// int Audio_info[12] (0x02BF21D0) is used here too; placed in ui_menu.cpp.
// mp_shell_data_item_info* Mp_shell_data_item_infos (0x02C0359C) is used here too; placed in multi_player_menu_system.cpp.
// int Num_mp_shell_data_item_infos (0x02C035A0) is used here too; placed in multi_player_menu_system.cpp.
// mp_message_box_type Mp_msgbox_type (0x02C035A8) is used here too; placed in multi_player_menu_system.cpp.
// vint_data_item* Hint_data_item (0x02C035B0) is used here too; placed in multi_player_menu_system.cpp.
// int Num_hints_added (0x02C035B4) is used here too; placed in multi_player_menu_system.cpp.
static int Syslink_selection; // 0x02C03698
// msgbox_info_set Message_box_info_set (0x02CA0340) is used here too; placed in ui_message_box.cpp.

int (*g_openrfgSyslinkSelection)();

// ---- internal functions

// 0x0088E780 (:199)
// Out of the menu: a pending load pops it; else out of multiplayer to the main menu (through GS_MULTI_SHUTDOWN unless
// a load is on).
static void do_pop_gameseq_change_syslink()
{
    if (gameseq_get_pending_state() == GS_LOAD)
    {
        gameseq_pop_state();
        return;
    }
    if (!Multiplayer)
    {
        gameseq_set_state(GS_MAINMENU, false);
        return;
    }
    if (gameseq_get_state() != GS_LOAD && gameseq_get_pending_state() != GS_LOAD)
    {
        Gs_switch_next_state = GS_MAINMENU;
        gameseq_set_state(GS_MULTI_SHUTDOWN, true);
    }
}

static void syslink_unload_ui_information();
static void syslink_load_menu_option_data_items();

// 0x008F35D0 (:213)
// Loaded: the data items and the menu rows' data group; unloading: the exit action (pop, or a push; statistics push
// the experience tally above them).
static void syslink_do_state_change()
{
    do
    {
        Mp_syslink_info.state = Mp_syslink_info.pending_state;
        if (Mp_syslink_info.pending_state == MMS_SY_LOADED)
        {
            Syslink_selection = 0;
            // syslink_load_ui_information, inlined.
            multi_player_menu_system_shell_load_data_items(Syslink_data_items, 5);
            vint_data_group* group = vint_data_group::create_new("MpShellPlayerInfo");
            if (group != nullptr)
            {
                Mp_syslink_info.data_group_handle = group->get_handle();
                ui_add_data_group(Mp_syslink_info.doc_handle, Mp_syslink_info.data_group_handle);
                syslink_load_menu_option_data_items();
            }
            multi_game_set_spectating(false);
        }
        else
        {
            if (Mp_syslink_info.pending_state != MMS_SY_UNLOADING)
            {
                return;
            }
            syslink_unload_ui_information();
            if (Mp_syslink_info.exit_action == MMEA_SY_POP)
            {
                do_pop_gameseq_change_syslink();
            }
            else if (Mp_syslink_info.exit_action == MMEA_SY_PUSH)
            {
                gameseq_push_state(Mp_syslink_info.exit_push_target, true, true);
                if (Mp_syslink_info.exit_push_target == GS_MULTIPLAYER_STATS)
                {
                    gameseq_push_state(GS_MULTIPLAYER_XP_TALLY, true, false);
                }
            }
            Mp_syslink_info.pending_state = MMS_SY_UNLOADED;
        }
    } while (Mp_syslink_info.state != Mp_syslink_info.pending_state);
}

// 0x0088E7D0 (:269)
// LTCG: new_state in EAX.
static void syslink_set_state(mp_menu_states_syslink new_state)
{
    Mp_syslink_info.pending_state = new_state;
}

// 0x0088E7E0 (:278)
static void syslink_init_state()
{
    Mp_syslink_info.state = MMS_SY_UNLOADED;
    Mp_syslink_info.pending_state = MMS_SY_UNLOADED;
    Mp_syslink_info.prev_state = MMS_SY_UNLOADED;
    Mp_syslink_info.exit_action = MMEA_SY_POP;
    Mp_syslink_info.exit_push_target = GS_MULTIPLAYER_LIVE;
}

// 0x008AE5E0 (:290)
static void syslink_init_info()
{
    Mp_syslink_info.group_item_handles.clear();
    Mp_syslink_info.data_group_handle = -1;
}

// 0x008C2550 (:298)
// Five rows: {index, -1, option, -1, text, 3, false}.
static void syslink_load_menu_option_data_items()
{
    vint_data_group* group = vint_data_group::find((unsigned int)Mp_syslink_info.data_group_handle);
    for (int i = 0; i < 5; i++)
    {
        vint_data_item* item = group->add_data_item(nullptr);
        if (item == nullptr)
        {
            continue;
        }
        vint_variant_type types[7] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL};
        item->set_template(types, 7);
        Mp_syslink_info.group_item_handles.add((int)item->get_handle());
    }
}

// 0x008D4970 (:330)
static void syslink_load_data_groups()
{
    vint_data_group* group = vint_data_group::create_new("MpShellPlayerInfo");
    if (group != nullptr)
    {
        Mp_syslink_info.data_group_handle = group->get_handle();
        ui_add_data_group(Mp_syslink_info.doc_handle, Mp_syslink_info.data_group_handle);
        syslink_load_menu_option_data_items();
    }
}

// 0x008D49A0 (:366)
static void syslink_load_ui_information()
{
    multi_player_menu_system_shell_load_data_items(Syslink_data_items, 5);
    syslink_load_data_groups();
}

// 0x008E58B0 (:377)
static void syslink_unload_ui_information()
{
    multi_player_menu_system_shell_unload_data_items();
    Mp_syslink_info.group_item_handles.clear();
    Mp_syslink_info.data_group_handle = -1;
    multi_player_menu_system_close_message_box();
}

// 0x008E5940 (:386)
static void syslink_process_data_item_hint_text(vint_data_item* vdi)
{
    multi_player_menu_system_begin_hints(vdi);
    multi_player_menu_system_add_hint(localization_get_display_string_req(Back_string_syslink, nullptr), CBA_MENU_ABORT);
    multi_player_menu_system_end_hints();
}

// 0x0088E810 (:395)
static void syslink_process_data_item_titles(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant("NETWORK_LOCAL_NAME"));
    vdi->set_element(1, vint_variant(Menu_options_syslink[Syslink_selection].m_option));
}

// 0x0088E880 (:401)
static void syslink_process_data_item_info_text(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(true));
    vdi->set_element(1, vint_variant(Menu_options_syslink[Syslink_selection].m_description));
    vdi->set_element(2, vint_variant(""));
    vdi->set_element(3, vint_variant(false));
    vdi->set_element(4, vint_variant(""));
}

// 0x0088E970 (:410)
static void syslink_process_data_item_selection_info(vint_data_item* vdi)
{
    char image[32];
    char text[128];
    controls_get_button_image_name(image, sizeof(image), CBA_MENU_SELECT, Player_controller_index);
    keen::formatString(text, sizeof(text), "[image:%s]", image);
    vdi->set_element(0, vint_variant(Syslink_selection));
    vdi->set_element(1, vint_variant((const char*)text));
    vdi->set_element(2, vint_variant(false));
}

// 0x0088EA60 (:423)
static void syslink_process_data_item_menu_type(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(0));
}

// 0x008AE600 (:428)
static void syslink_process_data_group_menu_options()
{
    vint_data_group* group = vint_data_group::find((unsigned int)Mp_syslink_info.data_group_handle);
    if (group == nullptr)
    {
        return;
    }
    const int count = (int)group->get_num_data_items();
    for (int i = 0; i < count; i++)
    {
        vint_data_item* item = vint_data_item::find((unsigned int)Mp_syslink_info.group_item_handles[i]);
        if (item == nullptr)
        {
            continue;
        }
        item->set_element(0, vint_variant(i + 1));
        item->set_element(1, vint_variant(-1));
        item->set_element(2, vint_variant(Menu_options_syslink[i].m_option));
        item->set_element(3, vint_variant(-1));
        item->set_element(4, vint_variant(" "));
        // Magic number: row type 3 (a plain option row of rfg_ui_mp_shell).
        item->set_element(5, vint_variant(3));
        item->set_element(6, vint_variant(false));
    }
}

// 0x008AE790 (:458)
static void syslink_process_data_items()
{
    multi_player_menu_system_shell_process_data_items();
    syslink_process_data_group_menu_options();
}

// 0x008F36A0 (:467)
// The document loaded: the menu loads (and the DLC check runs); failed or missing: the menu goes.
static void syslink_process_loading()
{
    switch (ui_get_doc_asset_state(Mp_syslink_info.doc_handle))
    {
    case ST_LOADING:
        return;
    case ST_LOADED:
        Mp_syslink_info.pending_state = MMS_SY_LOADED;
        dlc_found_corrupt_content();
        return;
    case ST_UNLOADED:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        // fall through
    case ST_ERROR:
        multi_player_menu_system_shell_unload_doc(Mp_syslink_info.doc_handle);
        Mp_syslink_info.pending_state = MMS_SY_UNLOADING;
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
}

// 0x008AE7A0 (:502)
// LTCG: events_consumed in EAX.
static void syslink_input_back_pressed(bool& events_consumed)
{
    Mp_syslink_info.exit_action = MMEA_SY_POP;
    Mp_syslink_info.pending_state = MMS_SY_UNLOADING;
    events_consumed = true;
    audiolib_cue_play(Audio_info[5], nullptr, nullptr);
    gamemusic_frontend_start(false);
}

// syslink_ui_push, inlined in the selections.
static void syslink_select_push(game_state target)
{
    Mp_syslink_info.exit_action = MMEA_SY_PUSH;
    Mp_syslink_info.exit_push_target = target;
    Mp_syslink_info.pending_state = MMS_SY_UNLOADING;
    audiolib_cue_play(Audio_info[4], nullptr, nullptr);
}

// 0x008AE7E0 (:510)
// LTCG: events_consumed in ESI. Host creates the system link lobby before the game lobby is pushed.
static void syslink_input_selection_made(bool& events_consumed)
{
    switch (Syslink_selection)
    {
    case 0:
        multi_game_lobby_create(true, false, false, false);
        syslink_select_push(GS_MULTIPLAYER_GAME_LOBBY);
        break;
    case 1:
        syslink_select_push(GS_MULTIPLAYER_SYSLINK_FIND_SERVERS);
        break;
    case 2:
        syslink_select_push(GS_MULTIPLAYER_PLAYER_SELECT);
        break;
    case 3:
        syslink_select_push(GS_MULTIPLAYER_STATS);
        break;
    case 4:
        syslink_select_push(GS_MULTIPLAYER_TUTORIALS);
        break;
    default:
        audiolib_cue_play(Audio_info[11], nullptr, nullptr);
        break;
    }
    events_consumed = true;
}

// 0x008D49E0 (:579)
// LTCG: events_consumed in EAX (and ESI for the selection). The back hint, then the rows: hovering selects, a click
// chooses.
static void syslink_process_mouse(bool& events_consumed)
{
    if (Utility_doc_handle == -1 || !ui_is_msgbox_active(-2))
    {
        if (ui_mouse_over_hint(kHintText1, Mp_syslink_info.doc_handle, true))
        {
            syslink_input_back_pressed(events_consumed);
            events_consumed = true;
            return;
        }
    }
    const int selection = multi_player_menu_system_handle_menu_mouse_input(Syslink_selection, Mp_syslink_info.group_item_handles.size(), Mp_syslink_info.input_id, Mp_syslink_info.doc_handle, events_consumed);
    if (selection >= 0)
    {
        if (selection != Syslink_selection)
        {
            multi_player_menu_system_set_menu_selection(selection, Syslink_selection, Mp_syslink_info.group_item_handles.size());
        }
        if (mouse_just_released(0))
        {
            syslink_input_selection_made(events_consumed);
        }
    }
}

// 0x008D4AB0 (:602)
// LTCG: reads ECX (the events flag's stack slot only).
static void syslink_process_input()
{
    // Original bug: rfg.exe never initializes the flag (its slot is ECX's top byte, pushed as stack space); false here.
    bool events_consumed = false;
    // Test seam (UI.md 2.7 item 2, off by default): the command line's choice (host or find games).
    if (g_openrfgSyslinkSelection != nullptr)
    {
        const int selection = g_openrfgSyslinkSelection();
        if (selection >= 0)
        {
            Syslink_selection = selection;
            syslink_input_selection_made(events_consumed);
            return;
        }
    }
    syslink_process_mouse(events_consumed);
    if (events_consumed)
    {
        return;
    }
    multi_player_menu_system_handle_menu_key_input(Syslink_selection, Mp_syslink_info.group_item_handles.size(), Mp_syslink_info.input_id, events_consumed);
    if (events_consumed)
    {
        return;
    }
    // Magic numbers: button 10 select, 11 back.
    const float select = ui_get_button_info(10, Mp_syslink_info.input_id);
    const float back = ui_get_button_info(11, Mp_syslink_info.input_id);
    if (select > 0.0f)
    {
        syslink_input_selection_made(events_consumed);
        return;
    }
    if (back > 0.0f)
    {
        syslink_input_back_pressed(events_consumed);
    }
}

// 0x008D4B50 (:625)
static void syslink_process_loaded()
{
    syslink_process_data_items();
    syslink_process_input();
}

// 0x0088EA90 (:752)
// LTCG: new_state in EAX.
static void syslink_ui_push(game_state new_state)
{
    Mp_syslink_info.exit_action = MMEA_SY_PUSH;
    Mp_syslink_info.exit_push_target = new_state;
    Mp_syslink_info.pending_state = MMS_SY_UNLOADING;
}

// 0x0088EAB0 (:759)
static void syslink_ui_pop()
{
    Mp_syslink_info.exit_action = MMEA_SY_POP;
    Mp_syslink_info.pending_state = MMS_SY_UNLOADING;
}

// ---- external interface

// 0x009173F0
void ui_document_mp_syslink_init()
{
    state_info info;
    info.enter = ui_document_mp_syslink_enter;
    info.exit = ui_document_mp_syslink_exit;
    info.process = ui_document_mp_syslink_process;
    info.handshake = ui_document_mp_syslink_handshake;
    info.render = ui_document_mp_syslink_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_MULTIPLAYER_SYSLINK, info);
}

// 0x0090E5E0 (:788)
// The shell document, a control id and an input id; the server options reset unless coming back from a match.
void ui_document_mp_syslink_enter(game_state prev_state)
{
    syslink_init_state();
    syslink_init_info();
    multi_player_menu_system_shell_load_doc(Mp_syslink_info.doc_handle, false);
    Mp_syslink_info.control_id = ui_request_control_id();
    Mp_syslink_info.input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    if (!multi_game_lobby_pushed_to_game())
    {
        Mp_server_options.reinit();
    }
    main_menu_show_menu(false);
}

// 0x008F3750 (:817)
void ui_document_mp_syslink_exit(game_state new_state)
{
    syslink_unload_ui_information();
    ui_relinquish_control_id(Mp_syslink_info.control_id);
    ui_relinquish_input_id(Mp_syslink_info.input_id);
    multi_player_menu_system_shell_unload_doc(Mp_syslink_info.doc_handle);
    main_menu_show_menu(true);
}

// 0x00915630 (:834)
// Back from a match with the lobby up: on to the game lobby (else the pushed flag is cleared).
void ui_document_mp_syslink_process()
{
    if (Bink_info_main.handle != 0)
    {
        mm_process_bink_video();
    }
    if (multi_game_lobby_pushed_to_game())
    {
        if (Lobby_session != nullptr && Lobby_session->get_state() != MSS_NONE)
        {
            syslink_ui_push(GS_MULTIPLAYER_GAME_LOBBY);
            return;
        }
        multi_game_lobby_clear_pushed_to_game();
        return;
    }
    ui_add_for_processing(Mp_syslink_info.doc_handle);
    switch (Mp_syslink_info.state)
    {
    case MMS_SY_LOADING:
        syslink_process_loading();
        break;
    case MMS_SY_LOADED:
        syslink_process_loaded();
        break;
    case MMS_SY_UNLOADED:
        syslink_set_state(MMS_SY_LOADING);
        break;
    default:
        break;
    }
    ui_process(Mp_syslink_info.control_id, false);
}

// 0x008F3800 (:873)
void ui_document_mp_syslink_handshake(rl_renderer*)
{
    Mp_syslink_info.prev_state = Mp_syslink_info.state;
    if (Mp_syslink_info.pending_state != Mp_syslink_info.state)
    {
        syslink_do_state_change();
    }
}

// 0x008FC8C0 (:881)
// Drawn once loaded for a whole frame; nothing while going back to the lobby after a match.
void ui_document_mp_syslink_render(rl_renderer*)
{
    if (Mp_syslink_info.doc_handle == -1 || Mp_syslink_info.control_id == 0 || multi_game_lobby_pushed_to_game())
    {
        return;
    }
    if (Mp_syslink_info.state == MMS_SY_LOADED && Mp_syslink_info.prev_state == MMS_SY_LOADED)
    {
        ui_add_for_rendering(Mp_syslink_info.doc_handle);
    }
    ui_render(Mp_syslink_info.control_id, false);
}
