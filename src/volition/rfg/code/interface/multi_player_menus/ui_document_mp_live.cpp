// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_live.cpp
//
// The online multiplayer menu (GS_MULTIPLAYER_LIVE, stub_impl.md S7) on rfg_ui_mp_shell: matchmaking, the party lobby and
// custom game, find games, spectator mode, player select, statistics, leaderboards and the training videos; back leaves
// multiplayer. Options the Steam client does not offer (invites for the party, leaderboards) are disabled on entry and
// stay so. The menu's state changes happen in the handshake. Entered after a match with the lobby up, or while in a
// party, it goes on to the lobby.
//
// Original bugs kept (commented): the mouse hint flag and the events flag start uninitialized in rfg.exe; an option once
// disabled is never enabled again.

#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_live.h"

#include "keen/base/string.hpp"
#include "library/modules/thirdparty/sw_api/sw_api.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/gamewide/gamewide.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_lobby.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_matchmaking_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_matchmaking_generic.h"
#include "volition/rfg/code/multi/multi_game/multi_game_party.h"
#include "volition/rfg/code/multi/multi_game/multi_game_playlists.h"
#include "volition/rfg/code/multi/multi_game/multi_game_preferences.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/util/varray.h"

// ---- file-private types (rfg.pdb)
// size 0x10
struct menu_option_info_live
{
    const char* m_option; // 0x00
    const char* m_description; // 0x04
    bool m_enabled; // 0x08
    int m_index; // 0x0C
};
static_assert(sizeof(menu_option_info_live) == 0x10, "menu_option_info_live");

enum mp_menu_exit_actions_live
{
    MMEA_LI_POP = 0,
    MMEA_LI_PUSH = 1,
    NUM_MP_LI_MENU_EXIT_ACTIONS = 2,
};

enum mp_menu_states_live
{
    MMS_LI_LOADING = 0,
    MMS_LI_LOADED = 1,
    MMS_LI_UNLOADING = 2,
    MMS_LI_UNLOADED = 3,
    NUM_MP_LI_MENU_STATES = 4,
};

enum mp_live_hints
{
    MPL_HINT_INVALID = -1,
    MPL_HINT_ABORT = 0,
    NUM_MPL_HINTS = 1,
};

// size 0x74
struct mp_live_shell_info
{
    mp_menu_states_live state; // 0x00
    mp_menu_states_live pending_state; // 0x04
    mp_menu_states_live prev_state; // 0x08
    mp_menu_exit_actions_live exit_action; // 0x0C
    game_state exit_push_target; // 0x10
    int doc_handle; // 0x14
    int data_group_handle; // 0x18
    farray<int,16> group_item_handles; // 0x1C
    unsigned int control_id; // 0x6C
    int input_id; // 0x70
};
static_assert(sizeof(mp_live_shell_info) == 0x74, "mp_live_shell_info");

// ---- file-static globals
static void live_process_data_item_hint_text(vint_data_item* vdi);
static void live_process_data_item_titles(vint_data_item* vdi);
static void live_process_data_item_info_text(vint_data_item* vdi);
static void live_process_data_item_selection_info(vint_data_item* vdi);
static void live_process_data_item_menu_type(vint_data_item* vdi);
static mp_shell_data_item_info Live_data_items[5] = { // 0x01663478
    {MPSDIT_HINT_INFO, live_process_data_item_hint_text},
    {MPSDIT_TITLES, live_process_data_item_titles},
    {MPSDIT_INFO_TEXT, live_process_data_item_info_text},
    {MPSDIT_SELECTION_INFO, live_process_data_item_selection_info},
    {MPSDIT_MENU_TYPE, live_process_data_item_menu_type},
};
static const char* Back_string_live = {"MENU_BACK"}; // 0x016634A0
static mp_live_hints Current_mouse_hint_choice_live = MPL_HINT_INVALID; // 0x016634A4
static menu_option_info_live Live_menu_options[9] = { // 0x016634A8
    {"MENU_MATCHMAKING", "MENU_MATCHMAKING_DESC", true, 0},
    {"MENU_PARTY_LOBBY", "MENU_PARTY_LOBBY_DESC", true, 0},
    {"MENU_PARTY_LOBBY_CUSTOM_GAME", "MENU_PARTY_LOBBY_CUSTOM_GAME_DESC", true, 0},
    {"MENU_FIND_GAME", "MENU_FIND_GAME_INTERNET_DESC", true, 0},
    {"MENU_SPECTATOR_MODE", "MENU_SPECTATOR_MODE_DESC", true, 0},
    {"MENU_CUSTOMIZATION_OPTION", "MENU_CHOOSE_PLAYER_DESC", true, 0},
    {"MENU_STATISTICS", "MENU_STATISTICS_DESC", true, 0},
    {"MENU_LEADERBOARDS", "MENU_LEADERBOARDS_DESC", true, 0},
    {"MENU_TRAINING_VIDEOS", "MENU_TRAINING_VIDEOS_DESC", true, 0},
};
static mp_live_shell_info Mp_live_info; // 0x01666600
static int live_hint_config[1]; // 0x02BF2178
static int Live_menu_selection; // 0x02C035F4
static int Current_num_live_menu_options; // 0x02C035F8
static int live_num_hints; // 0x02C035FC

// ---- internal functions
static void live_load_ui_information();
static void live_unload_ui_information();

// 0x0088BE10
// Out of the menu: a pending load pops it; else out of multiplayer to the main menu (through GS_MULTI_SHUTDOWN unless a
// load is on).
static void do_pop_gameseq_change()
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

// 0x008F24A0
// Loaded: the data items, the first option selected, and the quickmatch playlist (the first playlist without one) made
// the ideal and current query. Unloading: the exit action.
static void live_do_state_change()
{
    do
    {
        if (Mp_live_info.pending_state == MMS_LI_LOADED)
        {
            const bool was_loading = Mp_live_info.state == MMS_LI_LOADING;
            Mp_live_info.state = Mp_live_info.pending_state;
            if (was_loading)
            {
                live_load_ui_information();
            }
            Live_menu_selection = Live_menu_options[0].m_index;
            multi_game_set_spectating(false);
            if (player_profile_get("mp_live", Player_controller_index != 0xFFFFFFFFu ? (int)Player_controller_index : 0) != nullptr)
            {
                const multi_game_playlist* first = multi_game_playlist_get_first();
                const multi_game_playlist* playlist = first;
                while (playlist != nullptr && playlist->m_query_type != QT_QUICKMATCH)
                {
                    playlist = multi_game_playlist_get_next(playlist);
                }
                if (playlist == nullptr)
                {
                    playlist = first;
                }
                game_query query;
                query.copy(*multi_game_preferences_get_ideal_query());
                query.m_game_type = playlist->game_type;
                query.m_query_type = playlist->m_query_type;
                query.m_game_mode = playlist->mode_type;
                query.m_playlist_id = playlist->playlist_id.get_crc();
                multi_game_preferences_set_ideal_query(&query);
                multi_game_preferences_set_current_query(&query);
            }
        }
        else
        {
            if (Mp_live_info.pending_state != MMS_LI_UNLOADING)
            {
                Mp_live_info.state = Mp_live_info.pending_state;
                return;
            }
            Mp_live_info.state = Mp_live_info.pending_state;
            live_unload_ui_information();
            if (Mp_live_info.exit_action == MMEA_LI_POP)
            {
                do_pop_gameseq_change();
            }
            else if (Mp_live_info.exit_action == MMEA_LI_PUSH)
            {
                if (Mp_live_info.exit_push_target == GS_MULTIPLAYER_CHANGE_MATCHMAKING)
                {
                    gameseq_push_state(GS_MULTIPLAYER_PARTY_LOBBY, true, true);
                }
                gameseq_push_state(Mp_live_info.exit_push_target, true, true);
                if (Mp_live_info.exit_push_target == GS_MULTIPLAYER_STATS)
                {
                    gameseq_push_state(GS_MULTIPLAYER_XP_TALLY, true, false);
                }
            }
            Mp_live_info.pending_state = MMS_LI_UNLOADED;
        }
    } while (Mp_live_info.state != Mp_live_info.pending_state);
}

// 0x0088BE60
// LTCG: new_state in EAX.
static void live_set_state(mp_menu_states_live new_state)
{
    Mp_live_info.pending_state = new_state;
}

// 0x0088BE70
static void live_init_state()
{
    Mp_live_info.state = MMS_LI_UNLOADED;
    Mp_live_info.pending_state = MMS_LI_UNLOADED;
    Mp_live_info.prev_state = MMS_LI_UNLOADED;
    Mp_live_info.exit_action = MMEA_LI_POP;
    Mp_live_info.exit_push_target = GS_MULTIPLAYER_LIVE;
}

// 0x008AD220
static void live_init_info()
{
    Mp_live_info.group_item_handles.clear();
    Mp_live_info.data_group_handle = -1;
}

// 0x008C00D0
// Nine rows: {index, -1, option, -1, text, type, then six flags}.
static void live_load_menu_option_data_items()
{
    vint_data_group* group = vint_data_group::find((unsigned int)Mp_live_info.data_group_handle);
    for (int i = 0; i < 9; i++)
    {
        vint_data_item* item = group->add_data_item(nullptr);
        if (item == nullptr)
        {
            continue;
        }
        const vint_variant_type types[12] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL};
        item->set_template(types, 12);
        Mp_live_info.group_item_handles.add((int)item->get_handle());
    }
}

// 0x008D2230
static void live_load_data_groups()
{
    vint_data_group* group = vint_data_group::create_new("MpShellPlayerInfo");
    if (group != nullptr)
    {
        Mp_live_info.data_group_handle = group->get_handle();
        ui_add_data_group(Mp_live_info.doc_handle, Mp_live_info.data_group_handle);
        live_load_menu_option_data_items();
    }
}

// 0x008D2260
static void live_load_ui_information()
{
    multi_player_menu_system_shell_load_data_items(Live_data_items, 5);
    live_load_data_groups(); // inlined
}

// 0x008E2940
static void live_unload_ui_information()
{
    multi_player_menu_system_shell_unload_data_items(); // inlined
    Mp_live_info.group_item_handles.clear();
    Mp_live_info.data_group_handle = -1;
    multi_player_menu_system_close_message_box(); // inlined
}

// 0x008E29D0
static void live_process_data_item_hint_text(vint_data_item* vdi)
{
    multi_player_menu_system_begin_hints(vdi); // inlined
    multi_player_menu_system_add_hint(localization_get_display_string_req(Back_string_live, nullptr), CBA_MENU_ABORT);
    live_hint_config[0] = MPL_HINT_ABORT;
    live_num_hints = 1;
    multi_player_menu_system_end_hints();
}

// 0x0088BEA0
// The option at a menu row (the first without one).
static int live_selection_index_to_actual_index(int index)
{
    for (int i = 0; i < 9; i++)
    {
        if (Live_menu_options[i].m_index == index)
        {
            return i;
        }
    }
    return 0;
}

// 0x0088BEC0
static void live_process_data_item_titles(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant("NETWORK_PLATFORM_NAME"));
    vdi->set_element(1, vint_variant(Live_menu_options[live_selection_index_to_actual_index(Live_menu_selection)].m_option));
}

// 0x0088BF50
static void live_process_data_item_info_text(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(true));
    vdi->set_element(1, vint_variant(Live_menu_options[live_selection_index_to_actual_index(Live_menu_selection)].m_description));
    vdi->set_element(2, vint_variant(""));
    vdi->set_element(3, vint_variant(false));
    vdi->set_element(4, vint_variant(""));
}

// 0x0088C060
static void live_process_data_item_selection_info(vint_data_item* vdi)
{
    char image[128];
    char text[128];
    controls_get_button_image_name(image, sizeof(image), CBA_MENU_SELECT, Player_controller_index);
    keen::formatString(text, sizeof(text), "[image:%s]", image);
    vdi->set_element(0, vint_variant(Live_menu_selection));
    vdi->set_element(1, vint_variant((const char*)text));
    vdi->set_element(2, vint_variant(false));
}

// 0x0088C150
static void live_process_data_item_menu_type(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(0));
}

// 0x008AD240
// The rows in use show their options (a disabled one flagged); the others are blank and flagged.
static void live_process_data_group_menu_options()
{
    vint_data_group* group = vint_data_group::find((unsigned int)Mp_live_info.data_group_handle);
    if (group == nullptr)
    {
        return;
    }
    const int count = (int)group->get_num_data_items();
    for (int i = 0; i < Current_num_live_menu_options; i++)
    {
        vint_data_item* item = vint_data_item::find((unsigned int)Mp_live_info.group_item_handles[i]);
        if (item == nullptr)
        {
            continue;
        }
        const menu_option_info_live& option = Live_menu_options[live_selection_index_to_actual_index(i)];
        item->set_element(0, vint_variant(i + 1));
        item->set_element(1, vint_variant(-1));
        item->set_element(2, vint_variant(option.m_option));
        item->set_element(3, vint_variant(-1));
        item->set_element(4, vint_variant(" "));
        // Magic number: row type 3 (a plain option row of rfg_ui_mp_shell).
        item->set_element(5, vint_variant(3));
        item->set_element(6, vint_variant(false));
        item->set_element(11, vint_variant(!option.m_enabled));
    }
    for (int i = Current_num_live_menu_options; i < count; i++)
    {
        vint_data_item* item = vint_data_item::find((unsigned int)Mp_live_info.group_item_handles[i]);
        if (item == nullptr)
        {
            continue;
        }
        item->set_element(0, vint_variant(i + 1));
        item->set_element(1, vint_variant(-1));
        item->set_element(2, vint_variant(" "));
        item->set_element(3, vint_variant(-1));
        item->set_element(4, vint_variant(" "));
        item->set_element(5, vint_variant(3));
        item->set_element(6, vint_variant(false));
        item->set_element(11, vint_variant(true));
    }
}

// 0x008AD5C0
static void live_process_data_items()
{
    multi_player_menu_system_shell_process_data_items();
    live_process_data_group_menu_options();
}

// 0x008F2600
// The document loaded: the menu loads (and the DLC check runs); failed or missing: the menu goes.
static void live_process_loading()
{
    switch (ui_get_doc_asset_state(Mp_live_info.doc_handle))
    {
    case ST_LOADING:
        return;
    case ST_LOADED:
        Mp_live_info.pending_state = MMS_LI_LOADED;
        dlc_found_corrupt_content();
        return;
    case ST_UNLOADED:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        // fall through
    case ST_ERROR:
        ui_unload_document(Mp_live_info.doc_handle, DFOT_NONE, 0.5f);
        Mp_live_info.doc_handle = -1;
        Mp_live_info.pending_state = MMS_LI_UNLOADING;
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
}

// Port helper: a push out of the menu with the select sound, as rfg.exe inlines it in the selections.
static void live_select_push(game_state target)
{
    ui_document_mp_live_ui_push(target);
    audiolib_cue_play(Audio_info[4], nullptr, nullptr);
}

// 0x008AD5D0
// The selected row's option. Original: a row without an option takes the last one (the loop ends on it), which only an
// invalid selection can reach.
static void live_input_selection_made(bool& events_consumed)
{
    int option = 8;
    for (int i = 0; i < 9; i++)
    {
        if (Live_menu_options[i].m_index == Live_menu_selection)
        {
            option = i;
            break;
        }
    }
    switch (option)
    {
    case 1:
        Party_lobby_type = MPPLT_MATCHMAKING;
        live_select_push(GS_MULTIPLAYER_PARTY_LOBBY);
        break;
    case 2:
        Party_lobby_type = MPPLT_CUSTOM_GAME;
        live_select_push(GS_MULTIPLAYER_PARTY_LOBBY);
        break;
    case 3:
        live_select_push(GS_MULTIPLAYER_LIVE_FIND_SERVERS);
        break;
    case 4:
        multi_game_set_spectating(true);
        live_select_push(GS_MULTIPLAYER_CHANGE_MATCHMAKING);
        break;
    case 5:
        live_select_push(GS_MULTIPLAYER_PLAYER_SELECT);
        break;
    case 6:
        live_select_push(GS_MULTIPLAYER_STATS);
        break;
    case 7:
        live_select_push(GS_MULTIPLAYER_SCOREBOARD);
        break;
    case 8:
        live_select_push(GS_MULTIPLAYER_TUTORIALS);
        break;
    default:
        live_select_push(GS_MULTIPLAYER_CHANGE_MATCHMAKING);
        break;
    }
    events_consumed = true;
}

// 0x008D22A0
// LTCG: events_consumed in EDI. The back hint, then the rows: hovering an enabled option selects it, a click on the
// selected one chooses it.
static void live_input_process_mouse(bool& events_consumed)
{
    if (Utility_doc_handle == -1 || !ui_is_msgbox_active(-2))
    {
        if (ui_mouse_over_hint(kHintText1, Mp_live_info.doc_handle, true))
        {
            Current_mouse_hint_choice_live = (mp_live_hints)live_hint_config[0];
            return;
        }
    }
    const int new_selection = multi_player_menu_system_handle_menu_mouse_input(Live_menu_selection, Current_num_live_menu_options, Mp_live_info.input_id, Mp_live_info.doc_handle, events_consumed);
    if (new_selection < 0)
    {
        return;
    }
    if (new_selection != Live_menu_selection && Live_menu_options[live_selection_index_to_actual_index(new_selection)].m_enabled)
    {
        multi_player_menu_system_set_menu_selection(new_selection, Live_menu_selection, Current_num_live_menu_options);
    }
    if (mouse_just_released(0) && new_selection == Live_menu_selection)
    {
        live_input_selection_made(events_consumed);
    }
}

static void live_ui_pop();

// 0x008D2380
// LTCG: reads ECX (the events flag's stack slot only). The keys pass over disabled options.
static void live_process_input()
{
    // Original bug: rfg.exe never initializes the flag (its slot is ECX's top byte, pushed as stack space); false here.
    bool events_consumed = false;
    Current_mouse_hint_choice_live = MPL_HINT_INVALID;
    live_input_process_mouse(events_consumed);
    if (events_consumed)
    {
        return;
    }
    while (true)
    {
        multi_player_menu_system_handle_menu_key_input(Live_menu_selection, Current_num_live_menu_options, Mp_live_info.input_id, events_consumed);
        if (!events_consumed)
        {
            break;
        }
        if (Live_menu_options[live_selection_index_to_actual_index(Live_menu_selection)].m_enabled)
        {
            return;
        }
    }
    // Magic numbers: button 10 select, 11 back.
    const float select = ui_get_button_info(10, Mp_live_info.input_id);
    const float back = ui_get_button_info(11, Mp_live_info.input_id);
    const bool back_pressed = back > 0.0f || Current_mouse_hint_choice_live == MPL_HINT_ABORT;
    if (select > 0.0f)
    {
        live_input_selection_made(events_consumed);
        return;
    }
    if (back_pressed)
    {
        live_ui_pop();
        audiolib_cue_play(Audio_info[5], nullptr, nullptr);
        gamemusic_frontend_start(false);
    }
}

// 0x008D2480
static void live_process_loaded()
{
    live_process_data_items(); // inlined
    live_process_input();
}

// 0x0088C180
static void live_ui_pop()
{
    player_profile_save("live_ui_pop", Player_controller_index != 0xFFFFFFFFu ? (int)Player_controller_index : 0);
    multi_core_matchmake_internal_stop_search_timer();
    Mp_live_info.exit_action = MMEA_LI_POP;
    Mp_live_info.pending_state = MMS_LI_UNLOADING;
}

// ---- external interface

// 0x00917260 (:802)
void ui_document_mp_live_init()
{
    state_info info;
    info.enter = ui_document_mp_live_enter;
    info.exit = ui_document_mp_live_exit;
    info.process = ui_document_mp_live_process;
    info.handshake = ui_document_mp_live_handshake;
    info.render = ui_document_mp_live_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_MULTIPLAYER_LIVE, info);
}

// 0x0090C3B0 (:819)
// The shell document, a control id and an input id; the platform context; the server options reset unless coming back
// from a match; the menu rows numbered over the options Steam offers.
void ui_document_mp_live_enter(game_state prev_state)
{
    live_init_state();
    live_init_info();
    multi_player_menu_system_shell_load_doc(Mp_live_info.doc_handle, false); // inlined
    Mp_live_info.control_id = ui_request_control_id();
    Mp_live_info.input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false); // inlined
    multi_core_platform_set_game_mode_context(PGMC_MULTIPLAYER);
    multi_core_platform_set_game_type_context(PGTC_PUBLIC);
    if (!multi_game_lobby_pushed_to_game())
    {
        Mp_server_options.reinit();
    }
    main_menu_show_menu(false);
    // Original bug kept: these are not set true again if the client later offers them.
    if (SW_HasInvites() == 0)
    {
        Live_menu_options[1].m_enabled = false;
    }
    if (SW_HasLeaderboards() == 0)
    {
        Live_menu_options[7].m_enabled = false;
    }
    Current_num_live_menu_options = 0;
    for (int i = 0; i < 9; i++)
    {
        if (Live_menu_options[i].m_enabled)
        {
            Live_menu_options[i].m_index = Current_num_live_menu_options;
            Current_num_live_menu_options++;
        }
        else
        {
            Live_menu_options[i].m_index = -1;
        }
    }
}

// 0x008F26A0 (:880)
void ui_document_mp_live_exit(game_state new_state)
{
    live_unload_ui_information();
    ui_relinquish_control_id(Mp_live_info.control_id); // inlined
    multi_player_menu_system_shell_unload_doc(Mp_live_info.doc_handle); // inlined
    ui_relinquish_input_id(Mp_live_info.input_id);
    main_menu_show_menu(true);
}

// 0x00914AE0 (:899)
// Back from a match with the lobby up: on to the party lobby (a private match) or matchmaking; in a party: on to its
// lobby.
void ui_document_mp_live_process()
{
    if (Bink_info_main.handle != 0)
    {
        mm_process_bink_video();
    }
    if (multi_game_lobby_pushed_to_game())
    {
        if (Lobby_session != nullptr && Lobby_session->get_state() != MSS_NONE)
        {
            ui_document_mp_live_ui_push(Lobby_session->get_info()->private_match ? GS_MULTIPLAYER_PARTY_LOBBY : GS_MULTIPLAYER_CHANGE_MATCHMAKING);
            return;
        }
        multi_game_lobby_clear_pushed_to_game();
        return;
    }
    if (multi_game_party_in_party() && Mp_live_info.state == Mp_live_info.pending_state)
    {
        ui_document_mp_live_ui_push(GS_MULTIPLAYER_PARTY_LOBBY);
        return;
    }
    ui_add_for_processing(Mp_live_info.doc_handle);
    if (Mp_live_info.state == MMS_LI_LOADING)
    {
        live_process_loading();
    }
    else if (Mp_live_info.state == MMS_LI_LOADED)
    {
        live_process_loaded(); // inlined
    }
    else if (Mp_live_info.state == MMS_LI_UNLOADED)
    {
        live_set_state(MMS_LI_LOADING); // inlined
    }
    ui_process(Mp_live_info.control_id, false);
}

// 0x008F2750 (:942)
void ui_document_mp_live_handshake(rl_renderer*)
{
    Mp_live_info.prev_state = Mp_live_info.state;
    if (Mp_live_info.pending_state != Mp_live_info.state)
    {
        live_do_state_change();
    }
}

// 0x008FBD10 (:950)
// Drawn once loaded for a whole frame; nothing while going back to the lobby after a match.
void ui_document_mp_live_render(rl_renderer*)
{
    if (Mp_live_info.doc_handle == -1 || Mp_live_info.control_id == 0 || multi_game_lobby_pushed_to_game())
    {
        return;
    }
    if (Mp_live_info.state == MMS_LI_LOADED && Mp_live_info.prev_state == MMS_LI_LOADED)
    {
        ui_add_for_rendering(Mp_live_info.doc_handle);
    }
    ui_render(Mp_live_info.control_id, false);
}

// 0x0088C1C0 (:967)
void ui_document_mp_live_ui_push(game_state new_state)
{
    Mp_live_info.exit_action = MMEA_LI_PUSH;
    Mp_live_info.exit_push_target = new_state;
    Mp_live_info.pending_state = MMS_LI_UNLOADING;
}
