// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_game_lobby.cpp
//
// The game lobby (GS_MULTIPLAYER_GAME_LOBBY, UI.md UI5) on rfg_ui_mp_shell: the players sorted (me first, my party,
// my team, then by team and name) with their level, badge, team, voice and ready state; the lobby state's text
// (waiting, countdown, veto, starting), the countdown, veto and ready-up items, the loading tips until the game
// settings arrive, the level image; the host's settings and launch, team changes, the player options box, the lobby
// chat. The lobby is left (EXITING: multi_game_lobby_leave, then the session deleted) by the leave confirm box or when
// the session ends; the exit to the level load marks the lobby "pushed to game".
// The matchmaking and party paths (Live) are kept as rfg.exe has them; system link reaches only its own.

#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_game_lobby.h"

#include "kaiko/gamelink/gamelink.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_string_processing.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_core/include/multi_voice.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_matchmaking_generic.h"
#include "volition/rfg/code/multi/multi_game/multi_game_party.h"
#include "volition/rfg/code/multi/multi_game/multi_game_player.h"
#include "volition/rfg/code/multi/multi_game/multi_game_playlists.h"
#include "volition/rfg/code/multi/multi_game/multi_game_preferences.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode_shared.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/varray.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

// ---- file-private types (rfg.pdb)
enum game_lobby_hint_choices
{
    GL_HINT_INVALID = -1,
    GL_HINT_ABORT = 0,
    GL_HINT_SETTINGS = 1,
    GL_HINT_READY_UP = 2,
    GL_HINT_OPTIONS = 3,
    NUM_GL_HINTS = 4,
};

enum mp_menu_exit_actions_lobby
{
    MMEA_L_POP = 0,
    MMEA_L_PUSH = 1,
    NUM_MP_L_MENU_EXIT_ACTIONS = 2,
};

enum mp_menu_states_lobby
{
    MMS_L_LOADING = 0,
    MMS_L_LOADED = 1,
    MMS_L_EXITING = 2,
    MMS_L_UNLOADING = 3,
    MMS_L_UNLOADED = 4,
    NUM_MP_L_MENU_STATES = 5,
};

// size 0xD0
struct mp_game_lobby_shell_info
{

    mp_menu_states_lobby state; // 0x00
    mp_menu_states_lobby pending_state; // 0x04
    mp_menu_states_lobby prev_state; // 0x08
    mp_menu_exit_actions_lobby exit_action; // 0x0C
    game_state exit_push_target; // 0x10
    int doc_handle; // 0x14
    int data_group_handle; // 0x18
    farray<int,16> player_menu_rows; // 0x1C
    farray<unsigned int,16> sorted_player_list; // 0x6C
    unsigned int control_id; // 0xBC
    int input_id; // 0xC0
    bool exit_msg_box_created; // 0xC4
    timestamp_realtime exit_time; // 0xC8
    bool show_streamed_image; // 0xCC
};
static_assert(sizeof(mp_game_lobby_shell_info) == 0xD0, "mp_game_lobby_shell_info");

// ---- file-static globals
static game_lobby_hint_choices Current_mouse_hint_choice_lobby = GL_HINT_INVALID; // 0x016633AC, initial data ffffffff
static void game_lobby_process_data_item_hint_info(vint_data_item* vdi);
static void game_lobby_process_data_item_titles(vint_data_item* vdi);
static void game_lobby_process_data_item_info_text(vint_data_item* vdi);
static void game_lobby_process_data_item_selection_info(vint_data_item* vdi);
static void game_lobby_process_data_item_menu_type(vint_data_item* vdi);
static void game_lobby_process_data_item_game_info(vint_data_item* vdi);
static void game_lobby_process_data_item_party_info(vint_data_item* vdi);
static void game_lobby_process_data_item_veto_info(vint_data_item* vdi);
static void game_lobby_process_data_item_timer(vint_data_item* vdi);
static void game_lobby_process_data_item_press_start(vint_data_item* vdi);
static void game_lobby_process_data_item_tips(vint_data_item* vdi);
static mp_shell_data_item_info Game_lobby_data_items[11] = {
    {MPSDIT_HINT_INFO, game_lobby_process_data_item_hint_info},
    {MPSDIT_TITLES, game_lobby_process_data_item_titles},
    {MPSDIT_INFO_TEXT, game_lobby_process_data_item_info_text},
    {MPSDIT_SELECTION_INFO, game_lobby_process_data_item_selection_info},
    {MPSDIT_MENU_TYPE, game_lobby_process_data_item_menu_type},
    {MPSDIT_GAME_INFO, game_lobby_process_data_item_game_info},
    {MPSDIT_PARTY_INFO, game_lobby_process_data_item_party_info},
    {MPSDIT_VETO_INFO, game_lobby_process_data_item_veto_info},
    {MPSDIT_TIMER, game_lobby_process_data_item_timer},
    {MPSDIT_PRESS_START, game_lobby_process_data_item_press_start},
    {MPSDIT_TIPS, game_lobby_process_data_item_tips},
}; // 0x016633B0
static const char* Leave_party_string_lobby = {"MENU_LEAVE_PARTY"}; // 0x01663408
static const char* Back_string_lobby = {"MENU_BACK"}; // 0x0166340C
static const char* Change_game_settings_string = {"MENU_SETTINGS"}; // 0x01663410
static const char* Matchmaking_string = {"MENU_MATCHMAKING"}; // 0x01663414
static const char* Attemp_reconnect_string = {"MENU_ATTEMPTING_RECONNECT"}; // 0x01663420
static const char* Load_image_string = {"MENU_LOADING_IMAGE"}; // 0x01663424
static const char* Wait_for_players_string = {"MENU_WAITING_FOR_MIN_PLAYERS"}; // 0x01663428
static const char* Wait_for_diparity_string = {"MENU_WAIT_FOR_MORE_PLAYERS"}; // 0x0166342C
static const char* Wait_for_more_players_string = {"MENU_WAIT_FOR_MORE_PLAYERS"}; // 0x01663430
static const char* Wait_to_balance_string = {"MENU_WAIT_TO_BALANCE_TEAMS"}; // 0x01663434
static const char* Count_game_start_string = {"MENU_COUNTING_TO_START"}; // 0x01663438
static const char* Map_vetoed_string = {"MENU_MAP_VETOED"}; // 0x0166343C
static const char* Starting_game_string = {"MENU_STARTING_GAME"}; // 0x01663440
static const char* Game_over_string = {"MENU_GAME_OVER"}; // 0x01663444
static const char* Countdown_string = {"MENU_COUNTDOWN"}; // 0x01663448
static const char* Veto_string = {"MENU_VETO"}; // 0x0166344C
static const char* You_vetoed_string = {"MENU_YOU_VETOED"}; // 0x01663450
static const char* Veto_count_string = {"MENU_VETO_COUNT"}; // 0x01663454
static const char* Ready_up_string = {"MENU_READY_UP"}; // 0x01663458
static const char* You_ready_uped_string = {"MENU_YOU_ARE_READY"}; // 0x0166345C
static const char* Ready_up_count_string = {"MENU_READY_COUNT"}; // 0x01663460
static const char* Players_string_lobby = {"MENU_PLAYERS"}; // 0x01663464
static const char* Waiting_string = {"MENU_WAITING"}; // 0x01663468
static int Mouse_tip_clicked = -1; // 0x0166346C
static int Mouse_arrow_clicked = -1; // 0x01663470
static mp_game_lobby_shell_info Mp_game_lobby_info; // 0x01666418
static int game_lobby_hint_config[4]; // 0x02BE7D60
static bool Mp_game_lobby_selection_is_local_player; // 0x02C035E1
static bool Display_loading_bar; // 0x02C035E2
static int game_lobby_num_hints; // 0x02C035E4
static int Mp_game_lobby_selection; // 0x02C035E8
static bool Start_game_clicked; // 0x02C035EC
// The mouse hints of the shell (header constants in rfg.exe, this file's copies).
static const char* hint_names[2] = {"tips_button_1", "tips_button_2"}; // 0x01668700
static const char* hint_names2[1] = {"game_start_click"}; // 0x016686FC
static const char* hint_names3[2] = {"row_highlight_left", "row_highlight_right"}; // 0x016686F4

bool (*g_openrfgGameLobbyLaunch)();

// ---- internal functions

// 0x0088AD80
// LTCG: game_data_handle in EDI. Whether a player's name and level may show: always with a player list or for me,
// else only for my party's players.
static bool has_gamer_info(unsigned int game_data_handle)
{
    const multi_game_data* data = multi_game_data_get(game_data_handle, false, true);
    if (data == nullptr)
    {
        return false;
    }
    const unsigned int me = multi_game_data_get_my_handle(ST_GAME);
    if (multi_game_lobby_has_player_list() || me == game_data_handle)
    {
        return true;
    }
    return multi_game_party_in_party() && multi_game_party_has_player(data->player_uid);
}

static void game_lobby_load_ui_information();
static void game_lobby_unload_ui_information();
static void game_lobby_process_data_group_menu_options();

// 0x0090B6A0
// Loaded: the data items (and the system link "joining" box until the lobby is ready); exiting: the rows updated once
// more and the lobby left; unloading: the boxes go and the exit action runs (in-game options pushed without a
// transition).
static void game_lobby_do_state_change()
{
    do
    {
        Mp_game_lobby_info.state = Mp_game_lobby_info.pending_state;
        switch (Mp_game_lobby_info.pending_state)
        {
        case MMS_L_LOADING:
            Mp_game_lobby_info.exit_msg_box_created = false;
            return;
        case MMS_L_LOADED:
            game_lobby_load_ui_information();
            if (Multi_syslink)
            {
                ui_show_loading_screen(false, "MENU_MSJS_JOIN_IN_PROGRESS");
            }
            break;
        case MMS_L_EXITING:
            Mp_game_lobby_info.exit_time.set(0);
            Mp_game_lobby_info.exit_msg_box_created = false;
            Mp_game_lobby_info.show_streamed_image = false;
            multi_player_menu_system_shell_process_data_items();
            game_lobby_process_data_group_menu_options();
            if (Lobby_session != nullptr && Lobby_session->get_state() != MSS_NONE && Lobby_session->get_state() != MSS_DELETING)
            {
                multi_game_lobby_leave(true);
            }
            break;
        case MMS_L_UNLOADING:
            if (Mp_game_lobby_info.exit_msg_box_created)
            {
                ui_remove_loading_screen();
            }
            if (Lobby_session->get_state() == MSS_NONE)
            {
                Search_search_pong_enabled = false;
            }
            if (Loading_screen_ref_cnt > 0)
            {
                ui_remove_loading_screen();
            }
            game_lobby_unload_ui_information();
            if (Mp_game_lobby_info.exit_action == MMEA_L_POP)
            {
                gameseq_pop_state();
            }
            else if (Mp_game_lobby_info.exit_action == MMEA_L_PUSH)
            {
                if (Mp_game_lobby_info.exit_push_target == GS_INGAME_OPTIONS)
                {
                    gameseq_push_state(GS_INGAME_OPTIONS, true, false);
                }
                else
                {
                    gameseq_push_state(Mp_game_lobby_info.exit_push_target, true, true);
                }
            }
            Mp_game_lobby_info.pending_state = MMS_L_UNLOADED;
            break;
        default:
            return;
        }
    } while (Mp_game_lobby_info.state != Mp_game_lobby_info.pending_state);
}

// 0x0088ADE0
// LTCG: new_state in EAX.
static void game_lobby_set_state(mp_menu_states_lobby new_state)
{
    Mp_game_lobby_info.pending_state = new_state;
}

// 0x0088ADF0
static void lobby_init_state()
{
    Mp_game_lobby_info.state = MMS_L_UNLOADED;
    Mp_game_lobby_info.pending_state = MMS_L_UNLOADED;
    Mp_game_lobby_info.prev_state = MMS_L_UNLOADED;
}

// 0x008ACC90
static void game_lobby_init_info()
{
    Mp_game_lobby_info.player_menu_rows.clear();
    Mp_game_lobby_info.sorted_player_list.clear();
    Mp_game_lobby_info.data_group_handle = -1;
}

// 0x0088AE10
// Me first, then (in a party) my party's players, then my team's, then by team and by name.
static bool player_sort_less_than_or_equal_to(const unsigned int& lhs_handle, const unsigned int& rhs_handle)
{
    if (lhs_handle == rhs_handle)
    {
        return true;
    }
    const multi_game_data* me = multi_game_data_get_my_data(ST_GAME, false, true);
    const multi_game_data* lhs = multi_game_data_get(lhs_handle, false, true);
    const multi_game_data* rhs = multi_game_data_get(rhs_handle, false, true);
    if (lhs == nullptr)
    {
        return false;
    }
    if (rhs == nullptr || lhs == me)
    {
        return true;
    }
    if (rhs == me)
    {
        return false;
    }
    if (multi_game_party_in_party())
    {
        const bool lhs_party = multi_game_party_has_player(lhs->player_uid);
        const bool rhs_party = multi_game_party_has_player(rhs->player_uid);
        if (lhs_party)
        {
            if (!rhs_party)
            {
                return true;
            }
        }
        else if (rhs_party)
        {
            return false;
        }
    }
    if (me != nullptr)
    {
        const human_teams my_team = me->team;
        if (lhs->team == my_team && rhs->team != my_team)
        {
            return true;
        }
        if (lhs->team != my_team && rhs->team == my_team)
        {
            return false;
        }
    }
    if (lhs->team == rhs->team)
    {
        return _v_stricmp(lhs->display_name, rhs->display_name) < 1;
    }
    return (int)lhs->team < (int)rhs->team;
}

// 0x008BFE50
// A player row: {index, badge, name, team, level, voice, leader, party, (unused), can ready, ready}.
static void game_lobby_add_player_row(vint_data_group* vdg)
{
    vint_data_item* item = vdg->add_data_item(nullptr);
    if (item == nullptr)
    {
        return;
    }
    vint_variant_type types[10] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING,
                                   VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL};
    item->set_template(types, 10);
    Mp_game_lobby_info.player_menu_rows.add((int)item->get_handle());
}

// 0x008ACCB0
static void game_lobby_remove_player_row(vint_data_group* vdg)
{
    const int last = Mp_game_lobby_info.player_menu_rows.size() - 1;
    vdg->remove_data_item((unsigned int)Mp_game_lobby_info.player_menu_rows[last]);
    Mp_game_lobby_info.player_menu_rows.remove(last);
}

// 0x008D0070
static void game_lobby_load_data_groups()
{
    vint_data_group* group = vint_data_group::create_new("MpShellPlayerInfo");
    if (group != nullptr)
    {
        Mp_game_lobby_info.data_group_handle = group->get_handle();
        ui_add_data_group(Mp_game_lobby_info.doc_handle, Mp_game_lobby_info.data_group_handle);
    }
}

// 0x008D00A0
static void game_lobby_load_ui_information()
{
    multi_player_menu_system_shell_load_data_items(Game_lobby_data_items, 11);
    game_lobby_load_data_groups();
}

// 0x008D00E0
static void game_lobby_unload_ui_information()
{
    multi_player_menu_system_shell_unload_data_items();
    Mp_game_lobby_info.player_menu_rows.clear();
    Mp_game_lobby_info.sorted_player_list.clear();
    Mp_game_lobby_info.data_group_handle = -1;
}

// 0x0088AEF0
static void game_lobby_process_data_item_menu_type(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(1));
}

// 0x008E0FF0
// Back (or leave party for a party member) before the countdown; the host's settings while waiting for the launch,
// else ready up when allowed.
static void game_lobby_process_data_item_hint_info(vint_data_item* vdi)
{
    multi_player_menu_system_begin_hints(vdi);
    multi_player_menu_system_clear_hint_config(game_lobby_hint_config, 4, game_lobby_num_hints);
    // Magic number: 7, MGLSM_GAME_COUNTDOWN.
    if ((int)multi_game_lobby_get_state() < 7)
    {
        multi_player_menu_system_add_hint_config(game_lobby_num_hints, game_lobby_hint_config, GL_HINT_ABORT);
        if (multi_game_party_in_party() && !multi_game_party_am_i_leader(false))
        {
            multi_player_menu_system_add_hint(localization_get_display_string_req(Leave_party_string_lobby, nullptr), CBA_MENU_ABORT);
        }
        else
        {
            // Two spaces after the back text.
            wchar_t back[128];
            const wchar_t* text = localization_get_display_string_req(Back_string_lobby, nullptr);
            wcscpy(back, text != nullptr ? text : L"");
            wcscat(back, L"  ");
            multi_player_menu_system_add_hint(back, CBA_MENU_ABORT);
        }
    }
    if (multi_game_lobby_waiting_for_user_launch())
    {
        if (Lobby_session->am_i_host())
        {
            multi_player_menu_system_add_hint_config(game_lobby_num_hints, game_lobby_hint_config, GL_HINT_SETTINGS);
            multi_player_menu_system_add_hint(localization_get_display_string_req(Change_game_settings_string, nullptr), CBA_MENU_R2);
        }
    }
    else if (multi_game_lobby_can_ready_up())
    {
        multi_player_menu_system_add_hint_config(game_lobby_num_hints, game_lobby_hint_config, GL_HINT_READY_UP);
        multi_player_menu_system_add_hint(localization_get_display_string_req("MENU_READY_UP", nullptr), CBA_MENU_START);
    }
    multi_player_menu_system_end_hints();
}

// 0x0088AF20
// The network name (party, matchmaking or local) and the playlist's name.
static void game_lobby_process_data_item_titles(vint_data_item* vdi)
{
    const char* network;
    if (multi_game_lobby_is_in_party_lobby())
    {
        network = "NETWORK_PLATFORM_NAME";
    }
    else
    {
        network = Lobby_session->get_info()->systemlink_or_lan ? "NETWORK_LOCAL_NAME" : Matchmaking_string;
    }
    char playlist[128];
    const char* title = "";
    if (multi_game_lobby_is_in_party_lobby())
    {
        title = "PARTY LOBBY";
    }
    else if (!Lobby_session->get_info()->systemlink_or_lan)
    {
        const unsigned int playlist_id = multi_game_preferences_get_current_query()->m_playlist_id;
        if (playlist_id != 0)
        {
            strcpy(playlist, multi_game_playlist_get_by_id(playlist_id)->display_name);
            title = playlist;
        }
    }
    vdi->set_element(0, vint_variant(network));
    vdi->set_element(1, vint_variant(title));
}

// 0x008E11E0
// The lobby state's text, the level image once the settings are known, the loading bar while waiting.
static void game_lobby_process_data_item_info_text(vint_data_item* vdi)
{
    bool visible = true;
    Display_loading_bar = true;
    wchar_t text[128];
    memset(text, 0, sizeof(text));
    char image[64];
    memset(image, 0, sizeof(image));
    if (Lobby_session->get_state() == MSS_MIGRATING)
    {
        _snprintf_safe(text, 0x80, L"%ls", localization_get_display_string_req(Attemp_reconnect_string, nullptr));
    }
    else if (multi_game_lobby_has_game_settings())
    {
        if (Mp_game_lobby_info.show_streamed_image)
        {
            const char* level_image = multi_player_menu_system_level_image_get();
            if (level_image == nullptr)
            {
                _snprintf_safe(text, 0x80, L"%ls", localization_get_display_string_req(Load_image_string, nullptr));
            }
            else
            {
                strcpy(image, level_image);
            }
        }
        const multi_game_lobby_state_machine state = multi_game_lobby_get_state();
        // Magic numbers: the bar shows only from MGLSM_SELECTING_BEST_HOST (9) to MGLSM_GAME_OVER (13).
        if (state != MGLSM_NONE && ((int)state < 9 || (int)state > 13))
        {
            Display_loading_bar = false;
        }
    }
    else
    {
        const char* tag = nullptr;
        switch (multi_game_lobby_get_state())
        {
        case MGLSM_NONE:
        case MGLSM_CLIENT:
        case MGLSM_HOST:
            visible = false;
            break;
        case MGLSM_WAITING_FOR_MIN_PLAYERS:
        {
            wchar_t waiting[128];
            const multi_game_playlist* playlist = multi_game_playlist_get_by_id(multi_game_preferences_get_current_query()->m_playlist_id);
            _snprintf_safe(waiting, 0x80, localization_get_display_string_req(Wait_for_players_string, nullptr), playlist->min_players);
            _snprintf_safe(text, 0x80, L"%ls", waiting);
            break;
        }
        case MGLSM_WAITING_FOR_MARGINALLY_FAIR_TEAMS:
            Display_loading_bar = false;
            tag = Wait_for_diparity_string;
            break;
        case MGLSM_WAITING_FOR_PLAYERS:
            Display_loading_bar = false;
            tag = Wait_for_more_players_string;
            break;
        case MGLSM_WAITING_FOR_BALANCED_TEAMS:
            Display_loading_bar = false;
            tag = Wait_to_balance_string;
            break;
        case MGLSM_GAME_COUNTDOWN:
            Display_loading_bar = false;
            tag = Count_game_start_string;
            break;
        case MGLSM_VETOED:
            Display_loading_bar = false;
            tag = Map_vetoed_string;
            break;
        case MGLSM_SELECTING_BEST_HOST:
        case MGLSM_COMMUNICATING_BEST_HOST:
        case MGLSM_MIGRATING_BEST_HOST:
        case MGLSM_STARTING_GAME:
        case MGLSM_IN_GAME:
            tag = Starting_game_string;
            break;
        case MGLSM_GAME_OVER:
            tag = Game_over_string;
            break;
        default:
            break;
        }
        if (tag != nullptr)
        {
            _snprintf_safe(text, 0x80, L"%ls", localization_get_display_string_req(tag, nullptr));
        }
    }
    char encoded[384];
    vint_encode_wide_text(encoded, sizeof(encoded), text);
    vdi->set_element(0, vint_variant(visible));
    vdi->set_element(1, vint_variant((const char*)encoded));
    vdi->set_element(2, vint_variant((const char*)image));
    vdi->set_element(3, vint_variant(Display_loading_bar));
    vdi->set_element(4, vint_variant(Display_loading_bar ? Waiting_string : (const char*)nullptr));
}

// 0x0088B040
static void game_lobby_process_data_item_selection_info(vint_data_item* vdi)
{
    char image[32];
    char text[128];
    controls_get_button_image_name(image, sizeof(image), CBA_MENU_SELECT, Player_controller_index);
    keen::formatString(text, sizeof(text), "[image:%s]", image);
    vdi->set_element(0, vint_variant(Mp_game_lobby_selection));
    vdi->set_element(1, vint_variant((const char*)text));
    vdi->set_element(2, vint_variant(Mp_game_lobby_selection_is_local_player && multi_game_lobby_allow_team_changing()));
}

// 0x008ACD00
// The countdown (or the veto's, 15 s at most), rounded up by 750 ms.
static void game_lobby_process_data_item_timer(vint_data_item* vdi)
{
    const multi_game_lobby_state_machine state = multi_game_lobby_get_state();
    if (state == MGLSM_GAME_COUNTDOWN || state == MGLSM_VETOED)
    {
        int time_left = multi_game_lobby_get_time_left();
        bool show = true;
        // Magic number: 15000 ms, the veto's longest countdown.
        if (state == MGLSM_VETOED && time_left >= 15000)
        {
            time_left = 15000;
        }
        else if (time_left == -1)
        {
            show = false;
        }
        if (show)
        {
            // Magic number: 750 ms (0x2EE) added before the whole seconds are taken.
            time_left += 750;
            if (time_left < 1)
            {
                time_left = 0;
            }
            char seconds[16];
            sprintf(seconds, "%2d", time_left / 1000);
            vdi->set_element(0, vint_variant(true));
            vdi->set_element(1, vint_variant(Countdown_string));
            vdi->set_element(2, vint_variant((const char*)seconds));
            return;
        }
    }
    vdi->set_element(0, vint_variant(false));
    vdi->set_element(1, vint_variant((const char*)nullptr));
    vdi->set_element(2, vint_variant((const char*)nullptr));
}

// 0x008E15D0
static void game_lobby_process_data_item_game_info(vint_data_item* vdi)
{
    const bool show = Lobby_session->is_connected() && multi_game_lobby_has_game_settings();
    vdi->set_element(0, vint_variant(show));
    if (show)
    {
        multi_player_menu_system_shell_process_game_info_item(vdi);
    }
}

// 0x008E1640
// The veto (with its count and the next level) or the ready up (with its count).
static void game_lobby_process_data_item_veto_info(vint_data_item* vdi)
{
    char image_name[32];
    char image[128];
    if (multi_game_lobby_veto_allowed() && !Display_loading_bar)
    {
        vdi->set_element(0, vint_variant(true));
        if (multi_game_lobby_can_i_veto())
        {
            vdi->set_element(1, vint_variant(true));
            controls_get_button_image_name(image_name, sizeof(image_name), CBA_MENU_Y, Player_controller_index);
            keen::formatString(image, sizeof(image), "[image:%s]", image_name);
            vdi->set_element(2, vint_variant(Veto_string));
            vdi->set_element(3, vint_variant((const char*)image));
        }
        else if (multi_game_lobby_did_i_veto())
        {
            vdi->set_element(1, vint_variant(false));
            vdi->set_element(2, vint_variant(You_vetoed_string));
            vdi->set_element(3, vint_variant((const char*)nullptr));
        }
        else
        {
            vdi->set_element(1, vint_variant(false));
        }
        wchar_t count[32];
        char encoded_count[96];
        _snprintf_safe(count, 0x20, localization_get_display_string_req(Veto_count_string, nullptr), multi_game_lobby_get_veto_count(), multi_game_lobby_get_vetoes_needed());
        vint_encode_wide_text(encoded_count, sizeof(encoded_count), count);
        vdi->set_element(4, vint_variant((const char*)encoded_count));
        vdi->set_element(5, vint_variant(0));
        if (!multi_game_lobby_can_i_veto())
        {
            return;
        }
        const multi_game_playlist_entry* entry = multi_game_lobby_get_veto_entry();
        if (entry == nullptr)
        {
            vdi->set_element(6, vint_variant("MENU_NONE"));
        }
        else
        {
            wchar_t next[256];
            char encoded_next[768];
            sprintf(next, L"%ls %ls", localization_get_display_string_req("MENU_NEXT_LEVEL", nullptr), localization_get_display_string_req(entry->level->display_name, nullptr));
            vint_encode_wide_text(encoded_next, sizeof(encoded_next), next);
            vdi->set_element(6, vint_variant((const char*)encoded_next));
        }
        return;
    }
    if (!multi_game_lobby_ready_allowed() || Display_loading_bar)
    {
        vdi->set_element(0, vint_variant(false));
        return;
    }
    vdi->set_element(0, vint_variant(true));
    if (multi_game_lobby_is_ready())
    {
        vdi->set_element(1, vint_variant(false));
        vdi->set_element(2, vint_variant(You_ready_uped_string));
        vdi->set_element(3, vint_variant((const char*)nullptr));
    }
    else if (multi_game_lobby_can_ready_up())
    {
        vdi->set_element(1, vint_variant(true));
        controls_get_button_image_name(image_name, sizeof(image_name), CBA_MENU_START, Player_controller_index);
        keen::formatString(image, sizeof(image), "[image:%s]", image_name);
        vdi->set_element(2, vint_variant(Ready_up_string));
        vdi->set_element(3, vint_variant((const char*)image));
    }
    else
    {
        vdi->set_element(1, vint_variant(false));
    }
    char ready[192];
    if (localization_get_display_string_req(Ready_up_count_string, nullptr) == nullptr)
    {
        _snprintf_safe(ready, 0xC0, "!!%d READY (%d NEEDED)", multi_game_lobby_get_ready_up_count(), multi_game_lobby_get_ready_ups_needed());
    }
    else
    {
        wchar_t count[64];
        _snprintf_safe(count, 0x40, localization_get_display_string_req("MENU_READY_COUNT", nullptr), multi_game_lobby_get_ready_up_count(), multi_game_lobby_get_ready_ups_needed());
        vint_encode_wide_text(ready, sizeof(ready), count);
    }
    vdi->set_element(4, vint_variant((const char*)ready));
    vdi->set_element(5, vint_variant(0));
    vdi->set_element(6, vint_variant(""));
}

// 0x0088B140
// The host's "start game" while the lobby waits for the launch; a system link client's "waiting for the host".
static void game_lobby_process_data_item_press_start(vint_data_item* vdi)
{
    if (Lobby_session->get_state() == MSS_READY && !Display_loading_bar)
    {
        if (multi_game_lobby_waiting_for_user_launch())
        {
            char image_name[32];
            char image[128];
            controls_get_button_image_name(image_name, sizeof(image_name), CBA_MENU_START, Player_controller_index);
            keen::formatString(image, sizeof(image), "[image:%s]", image_name);
            vdi->set_element(0, vint_variant(true));
            vdi->set_element(1, vint_variant((const char*)image));
            vdi->set_element(2, vint_variant("MENU_MP_WORD_READY"));
            vdi->set_element(3, vint_variant("MENU_START_GAME"));
            return;
        }
        if (Lobby_session->get_info()->systemlink_or_lan && !Lobby_session->am_i_host())
        {
            vdi->set_element(0, vint_variant(true));
            vdi->set_element(1, vint_variant((const char*)nullptr));
            vdi->set_element(2, vint_variant(""));
            vdi->set_element(3, vint_variant("MENU_WAIT_FOR_HOST"));
            return;
        }
    }
    vdi->set_element(0, vint_variant(false));
}

// 0x008ACE70
// The current loading tip (with the tip buttons' images) until the game settings arrive.
static void game_lobby_process_data_item_tips(vint_data_item* vdi)
{
    char left_name[32];
    char right_name[32];
    char left[128];
    char right[128];
    controls_get_button_image_name(left_name, sizeof(left_name), CBA_MENU_L2, Player_controller_index);
    controls_get_button_image_name(right_name, sizeof(right_name), CBA_MENU_R2, Player_controller_index);
    keen::formatString(left, sizeof(left), "[image:%s]", left_name);
    keen::formatString(right, sizeof(right), "[image:%s]", right_name);
    const loading_screen_tip_data* tip = Current_tip >= 0 && Current_tip < Tip_data.size() ? &Tip_data[Current_tip] : nullptr;
    if (multi_game_lobby_has_game_settings() || tip == nullptr)
    {
        vdi->set_element(0, vint_variant(false));
        vdi->set_element(1, vint_variant(""));
        vdi->set_element(2, vint_variant(""));
        vdi->set_element(3, vint_variant(""));
        vdi->set_element(4, vint_variant(""));
        vdi->set_element(5, vint_variant(""));
        return;
    }
    vdi->set_element(0, vint_variant(true));
    vdi->set_element(1, vint_variant((const char*)left));
    vdi->set_element(2, vint_variant((const char*)right));
    vdi->set_element(3, vint_variant((const char*)tip->title));
    vdi->set_element(4, vint_variant((const char*)tip->text));
    vdi->set_element(5, vint_variant((const char*)tip->icon_image));
}

// 0x008E1BD0
// The player counts: per team (guerrillas, EDF, spectators) when teams can be chosen, else players and spectators.
static void game_lobby_process_data_item_party_info(vint_data_item* vdi)
{
    int total = 0;
    int guerillas = 0;
    int edf = 0;
    int spectators = 0;
    for (const multi_game_data* data = multi_game_data_begin(ST_GAME, false, false); data != multi_game_data_end(ST_GAME); data = multi_game_data_next(data, false, false))
    {
        if (data->team == HUMAN_TEAM_GUERILLA)
        {
            guerillas++;
        }
        else if (data->team == HUMAN_TEAM_EDF)
        {
            edf++;
        }
        else if (data->team == HUMAN_TEAM_MP_SPECTATOR)
        {
            spectators++;
        }
        total++;
    }
    wchar_t text[128];
    if (multi_game_lobby_has_player_list() && multi_gameplay_mode_is_team_mode(Mp_server_options.get_mode()) && multi_game_lobby_allow_team_changing())
    {
        const wchar_t* edf_name = human_team_get_display_name(HUMAN_TEAM_EDF);
        const wchar_t* guerilla_name = human_team_get_display_name(HUMAN_TEAM_GUERILLA);
        _snprintf_safe(text, 0x80, L"%ls (%d) %ls (%d) [format][scale:1.0][image_tint:ui_fend_icon_spec][/format] (%d)", guerilla_name, guerillas, edf_name, edf, spectators);
    }
    else
    {
        _snprintf_safe(text, 0x80, L"%ls (%d) [format][scale:1.0][image_tint:ui_fend_icon_spec][/format] (%d)", localization_get_display_string_req(Players_string_lobby, nullptr), total - spectators, spectators);
    }
    char encoded[384];
    vint_encode_wide_text(encoded, sizeof(encoded), text);
    vdi->set_element(0, vint_variant(total > 0));
    vdi->set_element(1, vint_variant((const char*)encoded));
}

// 0x008E1DA0
// A player row: the name (or why the player is gone, or an empty slot's text), badge, team, level, voice state, host
// or party leader, party member, ready state.
static void game_lobby_update_player_data(int index, const multi_game_data* data)
{
    vint_data_item* item = vint_data_item::find((unsigned int)Mp_game_lobby_info.player_menu_rows[index]);
    if (item == nullptr)
    {
        return;
    }
    bool can_ready = false;
    bool ready = false;
    wchar_t name[64];
    const char* empty_slot = nullptr;
    if (data == nullptr)
    {
        switch (multi_game_lobby_get_state())
        {
        case MGLSM_NONE:
        case MGLSM_CLIENT:
        case MGLSM_HOST:
        case MGLSM_WAITING_FOR_MIN_PLAYERS:
        case MGLSM_WAITING_FOR_MARGINALLY_FAIR_TEAMS:
        case MGLSM_WAITING_FOR_PLAYERS:
            empty_slot = "MENU_WAIT_FOR_PLAYER";
            break;
        default:
            empty_slot = "MENU_ADDITIONAL_PLAYER";
            break;
        }
        _snprintf_safe(name, 0x40, L"%ls", localization_get_display_string_req(empty_slot, nullptr));
    }
    else
    {
        if (data->in_use)
        {
            if (has_gamer_info(data->handle))
            {
                _snprintf_safe(name, 0x40, L"%ls", data->display_name);
            }
            else
            {
                _snprintf_safe(name, 0x40, L"%ls", localization_get_display_string_req("MENU_PLAYER_FOUND", nullptr));
            }
        }
        else
        {
            const char* reason;
            switch (data->disconnect_reason)
            {
            case CDR_NONE:
            case CDR_DELETED:
            case CDR_SESSION_REMOVED:
                reason = "MENU_PLAYER_KICKED";
                break;
            case CDR_TEST_ADDRESS_FAILED:
            case CDR_SESSION_CONNECTION_LEFT:
                reason = "MENU_PLAYER_LEFT";
                break;
            case CDR_HEARTBEAT_TIMEOUT:
            case CDR_RELIABLE_ACK_TIMEOUT:
            case CDR_PENDING_STATE_TIMEOUT:
            case CDR_FAILED_SESSION_OP:
            case CDR_SESSION_MIGRATE_FAILED:
            case CDR_SESSION_MODIFY_FAILED:
            case CDR_SESSION_ARBITRATE_FAILED:
            case CDR_SESSION_START_FAILED:
                reason = "MENU_PLAYER_TIMED_OUT";
                break;
            default:
                reason = "MENU_PLAYER_CONNECTION_ERROR";
                break;
            }
            _snprintf_safe(name, 0x40, L"%ls", localization_get_display_string_req(reason, nullptr));
        }
        ready = multi_game_lobby_player_is_ready(data->player_uid);
        const multi_game_lobby_state_machine state = multi_game_lobby_get_state();
        if (state == MGLSM_WAITING_FOR_PLAYERS || state == MGLSM_WAITING_FOR_BALANCED_TEAMS)
        {
            can_ready = data->team != HUMAN_TEAM_MP_SPECTATOR;
        }
        else if ((int)state > MGLSM_GAME_OVER || (int)state < MGLSM_NONE)
        {
            _snprintf_safe(name, 0x40, L"%ls", localization_get_display_string_req("MENU_ADDITIONAL_PLAYER", nullptr));
        }
    }
    char level[8] = {'-', '-', 0, 0, 0, 0, 0, 0};
    int badge = -1;
    // Magic number: team 2 (neutral) for empty and hidden teams.
    int team = 2;
    int voice = -1;
    bool leader = false;
    bool party = false;
    if (data != nullptr)
    {
        if (has_gamer_info(data->handle))
        {
            str_i2s(level, sizeof(level), (int)data->player_level);
            badge = (int)data->player_badge;
            voice = (int)multi_voice_get_icon_talk_state(data->player_uid);
        }
        if ((multi_game_lobby_has_player_list() && multi_game_lobby_allow_team_changing()) || data->team == HUMAN_TEAM_MP_SPECTATOR)
        {
            team = (int)data->team;
        }
        if (multi_game_party_in_party() && Lobby_session->get_state() != MSS_NONE && !Lobby_session->get_info()->systemlink_or_lan)
        {
            leader = multi_game_party_is_player_leader(data->player_uid);
            party = multi_game_party_has_player(data->player_uid);
        }
        else if (Lobby_session->get_state() != MSS_NONE && Lobby_session->get_info()->systemlink_or_lan && Lobby_session->get_host_connection() != nullptr)
        {
            // A system link host's row is marked.
            leader = Lobby_session->get_player_connection(data->handle) == Lobby_session->get_host_connection();
        }
    }
    char encoded[192];
    vint_encode_wide_text(encoded, sizeof(encoded), name);
    item->set_element(0, vint_variant(index + 1));
    item->set_element(1, vint_variant(badge));
    item->set_element(2, vint_variant((const char*)encoded));
    item->set_element(3, vint_variant(team));
    item->set_element(4, vint_variant((const char*)level));
    item->set_element(5, vint_variant(voice));
    item->set_element(6, vint_variant(leader));
    item->set_element(7, vint_variant(party));
    item->set_element(8, vint_variant(false));
    if (multi_game_lobby_can_ready_up())
    {
        item->set_element(9, vint_variant(can_ready));
        item->set_element(10, vint_variant(ready));
    }
    else
    {
        item->set_element(9, vint_variant(false));
        item->set_element(10, vint_variant(false));
    }
}

// 0x0088B340
static int visible_team_size(human_teams team)
{
    int count = 0;
    for (const multi_game_data* data = multi_game_data_begin(ST_GAME, false, true); data != multi_game_data_end(ST_GAME); data = multi_game_data_next(data, false, true))
    {
        if (data->team == team)
        {
            count++;
        }
    }
    return count;
}

// 0x008E22F0
// One row per player (sorted); in matchmaking also the empty slots up to the playlist's players; extra rows removed.
static void game_lobby_process_data_group_menu_options()
{
    if (Lobby_session == nullptr || Lobby_session->get_state() == MSS_NONE || Lobby_session->get_state() == MSS_DELETING)
    {
        return;
    }
    if (!(Lobby_session->get_info() != nullptr && Lobby_session->get_info()->systemlink_or_lan) && multi_game_preferences_get_current_query()->m_playlist_version == 0)
    {
        return;
    }
    vint_data_group* group = vint_data_group::find((unsigned int)Mp_game_lobby_info.data_group_handle);
    if (group == nullptr)
    {
        return;
    }
    Mp_game_lobby_info.sorted_player_list.clear();
    unsigned int count = 0;
    for (const multi_game_data* data = multi_game_data_begin(ST_GAME, false, true); data != multi_game_data_end(ST_GAME); data = multi_game_data_next(data, false, true))
    {
        Mp_game_lobby_info.sorted_player_list.add(data->handle);
        count++;
    }
    Mp_game_lobby_info.sorted_player_list.sort(player_sort_less_than_or_equal_to);
    unsigned int index = 0;
    for (; index < count; index++)
    {
        if (group->get_num_data_items() <= index)
        {
            game_lobby_add_player_row(group);
        }
        game_lobby_update_player_data((int)index, multi_game_data_get(Mp_game_lobby_info.sorted_player_list[(int)index], false, true));
    }
    if (!Lobby_session->get_info()->systemlink_or_lan && !multi_game_lobby_is_in_party_lobby())
    {
        int slots = -1;
        const multi_game_lobby_state_machine state = multi_game_lobby_get_state();
        if (state == MGLSM_WAITING_FOR_MIN_PLAYERS)
        {
            slots = visible_team_size(HUMAN_TEAM_MP_SPECTATOR) + multi_game_playlist_get_by_id(multi_game_preferences_get_current_query()->m_playlist_id)->min_players;
        }
        else if (state == MGLSM_WAITING_FOR_MARGINALLY_FAIR_TEAMS || state == MGLSM_WAITING_FOR_PLAYERS || state == MGLSM_WAITING_FOR_BALANCED_TEAMS)
        {
            slots = visible_team_size(HUMAN_TEAM_MP_SPECTATOR) + multi_game_playlist_get_by_id(multi_game_preferences_get_current_query()->m_playlist_id)->max_players;
        }
        if (slots >= 0)
        {
            if (Lobby_session->get_info()->max_slots <= slots)
            {
                slots = Lobby_session->get_info()->max_slots;
            }
            for (; (int)index < slots; index++)
            {
                if (group->get_num_data_items() <= index)
                {
                    game_lobby_add_player_row(group);
                }
                game_lobby_update_player_data((int)index, nullptr);
            }
        }
    }
    while (index < group->get_num_data_items())
    {
        game_lobby_remove_player_row(group);
    }
}

// 0x008E25F0
static void game_lobby_process_data_items()
{
    multi_player_menu_system_shell_process_data_items();
    game_lobby_process_data_group_menu_options();
}

// 0x008F21E0
static void game_lobby_process_loading()
{
    switch (ui_get_doc_asset_state(Mp_game_lobby_info.doc_handle))
    {
    case ST_LOADING:
        return;
    case ST_LOADED:
        Mp_game_lobby_info.pending_state = MMS_L_LOADED;
        return;
    case ST_UNLOADED:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        // fall through
    case ST_ERROR:
        multi_player_menu_system_shell_unload_doc(Mp_game_lobby_info.doc_handle);
        Mp_game_lobby_info.pending_state = MMS_L_UNLOADING;
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
}

// 0x0088B390
// LTCG: hint in EAX.
static void mp_game_lobby_input_hint_pressed(int hint, bool& events_consumed)
{
    Current_mouse_hint_choice_lobby = (game_lobby_hint_choices)game_lobby_hint_config[hint];
}

// 0x0090B7E0
// LTCG: the player's game data handle in EDI for has_gamer_info. The selected player's options box.
static void mp_game_lobby_input_selection_made(bool& events_consumed)
{
    const multi_game_data* data = multi_game_data_get(Mp_game_lobby_info.sorted_player_list[Mp_game_lobby_selection], false, true);
    if (data != nullptr && !data->linger_time.valid() && has_gamer_info(data->handle))
    {
        if (!ui_is_msgbox_active(Mp_msgbox_handle))
        {
            Mp_msgbox_handle = -1;
        }
        if (Mp_msgbox_handle == -1)
        {
            multi_player_menu_system_pull_up_player_option_message_box(data->player_uid, data->display_name, true);
            audiolib_cue_play(Audio_info[4], nullptr, nullptr);
            events_consumed = true;
            return;
        }
    }
    audiolib_cue_play(Audio_info[11], nullptr, nullptr);
    events_consumed = true;
}

// The top input id (ui_input_id_has_focus's lock or top request, inlined).
static int game_lobby_focus_input_id()
{
    if (Current_input_lock_id != -1)
    {
        return Current_input_lock_id;
    }
    return Input_types.size() - 1 < 0 ? 1 : Input_types[Input_types.size() - 1].id;
}

// 0x0090B8B0
// With the focus and no system UI: the chat's mouse, the hints (back, settings/ready, a third), the tip buttons, the
// start button, the team arrows, then the rows (hover selects, a click opens the player's options).
static void mp_game_lobby_input_process_mouse(bool& events_consumed)
{
    Mouse_tip_clicked = -1;
    Mouse_arrow_clicked = -1;
    Start_game_clicked = false;
    mp_game_input_process_chatmouse();
    if (Mp_game_lobby_info.input_id != game_lobby_focus_input_id() || multi_core_platform_ui_is_active())
    {
        return;
    }
    if (Utility_doc_handle == -1 || !ui_is_msgbox_active(-2))
    {
        int hint = -1;
        if (ui_mouse_over_hint(kHintText3, Mp_game_lobby_info.doc_handle, true))
        {
            hint = 2;
        }
        if (ui_mouse_over_hint(kHintText2, Mp_game_lobby_info.doc_handle, true))
        {
            hint = 1;
        }
        if (ui_mouse_over_hint(kHintText1, Mp_game_lobby_info.doc_handle, true))
        {
            Current_mouse_hint_choice_lobby = (game_lobby_hint_choices)game_lobby_hint_config[0];
            return;
        }
        if (hint >= 0)
        {
            Current_mouse_hint_choice_lobby = (game_lobby_hint_choices)game_lobby_hint_config[hint];
            return;
        }
    }
    const int tip = ui_mouse_process_hints(hint_names, 2, Mp_game_lobby_info.doc_handle, true);
    if (tip != -1)
    {
        Mouse_tip_clicked = tip;
        return;
    }
    Start_game_clicked = ui_mouse_process_hints(hint_names2, 1, Mp_game_lobby_info.doc_handle, true) != -1;
    Mouse_arrow_clicked = ui_mouse_process_hints(hint_names3, 2, Mp_game_lobby_info.doc_handle, true);
    if (Mouse_arrow_clicked != -1)
    {
        return;
    }
    const int selection = multi_player_menu_system_handle_menu_mouse_input(Mp_game_lobby_selection, Mp_game_lobby_info.sorted_player_list.size(), Mp_game_lobby_info.input_id, Mp_game_lobby_info.doc_handle, events_consumed);
    if (selection >= 0)
    {
        if (selection != Mp_game_lobby_selection)
        {
            multi_player_menu_system_set_menu_selection(selection, Mp_game_lobby_selection, Mp_game_lobby_info.sorted_player_list.size());
        }
        if (mouse_just_released(0))
        {
            mp_game_lobby_input_selection_made(events_consumed);
        }
    }
}

// 0x0090BA80
// Buttons (Button_info indices): 0x20 player options, 3/7 left, 4/8 right, 0xB back, 0xF veto, 9 start, 0x11 and
// 0x13 the previous and next tip (0x13 is also the host's settings).
static void mp_game_lobby_process_input()
{
    bool events_consumed = false;
    Current_mouse_hint_choice_lobby = GL_HINT_INVALID;
    mp_game_lobby_input_process_mouse(events_consumed);
    if (events_consumed)
    {
        return;
    }
    multi_player_menu_system_handle_menu_key_input(Mp_game_lobby_selection, Mp_game_lobby_info.sorted_player_list.size(), Mp_game_lobby_info.input_id, events_consumed);
    if (events_consumed)
    {
        return;
    }
    const int input_id = Mp_game_lobby_info.input_id;
    const bool options = ui_get_button_info(0x20, input_id) > 0.0f;
    bool left = ui_get_button_info(3, input_id) > 0.0f || ui_get_button_info(7, input_id) > 0.0f;
    const bool right_pressed = ui_get_button_info(4, input_id) > 0.0f || ui_get_button_info(8, input_id) > 0.0f;
    const float back_input = ui_get_button_info(0xB, input_id);
    const bool veto = ui_get_button_info(0xF, input_id) > 0.0f;
    bool start = ui_get_button_info(9, input_id) > 0.0f || Start_game_clicked;
    bool tip_back = ui_get_button_info(0x11, input_id) > 0.0f;
    const float settings_input = ui_get_button_info(0x13, input_id);
    const bool back = back_input > 0.0f || Current_mouse_hint_choice_lobby == GL_HINT_ABORT;
    const bool settings = settings_input > 0.0f || Current_mouse_hint_choice_lobby == GL_HINT_SETTINGS;
    start = start || Current_mouse_hint_choice_lobby == GL_HINT_READY_UP;
    // Test seam (UI.md 2.7 item 2, off by default): the command line's launch (the host's options set, the players in).
    if (g_openrfgGameLobbyLaunch != nullptr && g_openrfgGameLobbyLaunch())
    {
        start = true;
    }
    const bool tip_next = settings || Mouse_tip_clicked == 1;
    tip_back = tip_back || Mouse_tip_clicked == 0;
    left = left || Mouse_arrow_clicked == 0;
    const bool right = right_pressed || Mouse_arrow_clicked == 1;
    if (options)
    {
        if (Mp_game_lobby_selection < 0)
        {
            audiolib_cue_play(Audio_info[11], nullptr, nullptr);
            return;
        }
        const multi_game_data* data = multi_game_data_get(Mp_game_lobby_info.sorted_player_list[Mp_game_lobby_selection], false, true);
        if (data == nullptr || data->linger_time.valid())
        {
            audiolib_cue_play(Audio_info[11], nullptr, nullptr);
            return;
        }
        if (has_gamer_info(data->handle) && !multi_player_menu_system_has_open_message_box())
        {
            multi_player_menu_system_pull_up_player_option_message_box(data->player_uid, data->display_name, true);
            audiolib_cue_play(Audio_info[4], nullptr, nullptr);
            return;
        }
        audiolib_cue_play(Audio_info[11], nullptr, nullptr);
        return;
    }
    // Magic number: 7, MGLSM_GAME_COUNTDOWN (no leaving once the countdown runs).
    if (back && (int)multi_game_lobby_get_state() < 7)
    {
        multi_player_menu_system_pull_up_leave_confirm_message_box();
        audiolib_cue_play(Audio_info[5], nullptr, nullptr);
        return;
    }
    if (tip_next && !multi_game_lobby_has_game_settings())
    {
        loading_screen_tip_cycle(true);
        audiolib_cue_play(Audio_info[10], nullptr, nullptr);
        return;
    }
    if (tip_back && !multi_game_lobby_has_game_settings())
    {
        loading_screen_tip_cycle(false);
        audiolib_cue_play(Audio_info[9], nullptr, nullptr);
        return;
    }
    if (Mp_game_lobby_selection_is_local_player && (left || right) && multi_game_lobby_allow_team_changing())
    {
        if (!left)
        {
            multi_game_player_increment_my_team(Lobby_session);
            audiolib_cue_play(Audio_info[8], nullptr, nullptr);
            return;
        }
        multi_game_player_decrement_my_team(Lobby_session);
        audiolib_cue_play(Audio_info[7], nullptr, nullptr);
        return;
    }
    if (multi_game_lobby_waiting_for_user_launch())
    {
        if (tip_next)
        {
            // game_lobby_ui_push(GS_MULTIPLAYER_CHANGE_GAME_OPTIONS), inlined.
            Mp_game_lobby_info.exit_action = MMEA_L_PUSH;
            Mp_game_lobby_info.exit_push_target = GS_MULTIPLAYER_CHANGE_GAME_OPTIONS;
            Mp_game_lobby_info.pending_state = MMS_L_UNLOADING;
            audiolib_cue_play(Audio_info[6], nullptr, nullptr);
            return;
        }
        if (start)
        {
            multi_game_lobby_signal_game_start();
            audiolib_cue_play(Ga_cue_ids.multiplayer_select_start, nullptr, nullptr);
        }
        return;
    }
    if (multi_game_lobby_can_ready_up() && start)
    {
        multi_game_lobby_set_ready(!multi_game_lobby_is_ready());
        return;
    }
    if (multi_game_lobby_can_i_veto() && veto)
    {
        audiolib_cue_play(Audio_info[6], nullptr, nullptr);
        multi_game_lobby_veto_map();
    }
}

// 0x0090BF00
// The system link "joining" box goes once the lobby is ready; a lobby joined through a Steam invitation with a party
// filter goes to the party lobby; a lobby whose session ended (and whose link left the lobby) is left.
static void game_lobby_process_loaded()
{
    if (Multi_syslink && Loading_screen_ref_cnt > 0 && Lobby_session->get_state() == MSS_READY)
    {
        ui_remove_loading_screen();
    }
    game_lobby_process_data_items();
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = get_game_link_session();
    if (link != nullptr && g_joinedGameLobbyViaInvitation)
    {
        unsigned int filters[8];
        if (link->getLobbyFilters(filters, 8) != 0 && filters[2] != 0)
        {
            if (Mp_game_lobby_info.exit_msg_box_created)
            {
                ui_remove_loading_screen();
            }
            if (Lobby_session->get_state() == MSS_NONE)
            {
                Search_search_pong_enabled = false;
            }
            if (Loading_screen_ref_cnt > 0)
            {
                ui_remove_loading_screen();
            }
            game_lobby_unload_ui_information();
            gameseq_pop_state();
            Mp_game_lobby_info.pending_state = MMS_L_UNLOADED;
            gameseq_push_state(GS_MULTIPLAYER_PARTY_LOBBY, false, false);
        }
    }
    if (Lobby_session->get_state() == MSS_NONE || Lobby_session->get_state() == MSS_DELETING)
    {
        if (link == nullptr || link->getCurrentState() != kaiko::GameLinkState_InLobby)
        {
            // game_lobby_ui_pop, inlined.
            Mp_game_lobby_info.exit_action = MMEA_L_POP;
            Mp_game_lobby_info.pending_state = MMS_L_EXITING;
            g_gameLinkMutex.unlock();
            return;
        }
    }
    g_gameLinkMutex.unlock();
    Mp_game_lobby_selection_is_local_player = false;
    if (multi_gameplay_mode_is_team_mode(Mp_server_options.get_mode()) && Mp_game_lobby_selection >= 0)
    {
        const multi_game_data* data = multi_game_data_get(Mp_game_lobby_info.sorted_player_list[Mp_game_lobby_selection], false, false);
        if (data != nullptr)
        {
            multi_connection* connection = Lobby_session->get_player_connection(data->player_uid);
            if (connection != nullptr && Lobby_session->get_me_connection() == connection)
            {
                Mp_game_lobby_selection_is_local_player = true;
            }
        }
    }
    mp_game_lobby_process_input();
    if (Mp_game_lobby_info.sorted_player_list.size() > 0)
    {
        Mp_game_lobby_selection = Mp_game_lobby_selection % Mp_game_lobby_info.sorted_player_list.size();
        return;
    }
    Mp_game_lobby_selection = -1;
}

// 0x0090C0F0 (:1848)
// The session deleted once left; after 250 ms of deleting, the "disconnecting" box.
static void game_lobby_process_exit()
{
    if (Lobby_session == nullptr)
    {
        return;
    }
    const multi_session_state state = Lobby_session->get_state();
    if (state == MSS_NONE)
    {
        Mp_game_lobby_info.pending_state = MMS_L_UNLOADING;
    }
    else if (state == MSS_READY)
    {
        Lobby_session->session_delete();
    }
    else if (state == MSS_DELETING && Lobby_session->get_state() == MSS_DELETING && !Mp_game_lobby_info.exit_msg_box_created)
    {
        // Magic number: 250 ms (0xFA).
        if (Mp_game_lobby_info.exit_time.time_since() > 250)
        {
            ui_show_loading_screen(false, "MENU_DISCONNECT");
            Mp_game_lobby_info.exit_msg_box_created = true;
        }
    }
}

// 0x0088B3A0
// LTCG: new_state in EAX.
static void game_lobby_ui_push(game_state new_state)
{
    Mp_game_lobby_info.exit_action = MMEA_L_PUSH;
    Mp_game_lobby_info.exit_push_target = new_state;
    Mp_game_lobby_info.pending_state = MMS_L_UNLOADING;
}

// 0x0088B3C0
static void game_lobby_ui_pop()
{
    Mp_game_lobby_info.exit_action = MMEA_L_POP;
    Mp_game_lobby_info.pending_state = MMS_L_EXITING;
}

// ---- state callbacks (file statics in rfg.exe, external since UI5)

// 0x0090C170 (:1893)
// The lobby chat opens unless the game is over.
void ui_document_mp_game_lobby_enter(game_state prev_state)
{
    lobby_init_state();
    Mp_game_lobby_info.exit_action = MMEA_L_POP;
    Mp_game_lobby_info.exit_push_target = GS_MULTIPLAYER_LIVE;
    Mp_game_lobby_selection = 0;
    Mp_game_lobby_info.show_streamed_image = true;
    game_lobby_init_info();
    multi_player_menu_system_shell_load_doc(Mp_game_lobby_info.doc_handle, false);
    Mp_game_lobby_info.control_id = ui_request_control_id();
    Mp_game_lobby_info.input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    if (multi_game_lobby_get_state() != MGLSM_GAME_OVER)
    {
        Ui_text_chat_enabled = true;
        console_enable_chat_window(VCP_LOBBY);
    }
}

// 0x008BFF20 (:1929)
// Original: the shell document is not unloaded here (the level load or the next menu takes over). Leaving for the
// level load marks the lobby pushed to game; a system link lobby closes its chat.
void ui_document_mp_game_lobby_exit(game_state new_state)
{
    ui_relinquish_control_id(Mp_game_lobby_info.control_id);
    ui_relinquish_input_id(Mp_game_lobby_info.input_id);
    multi_player_menu_system_level_image_reset();
    if (gameseq_get_pending_state() == GS_LOAD)
    {
        Search_search_pong_enabled = false;
        multi_game_lobby_push_to_game();
    }
    if (multi_is_syslink_game())
    {
        console_disable_chat_window();
        Ui_text_chat_enabled = false;
    }
}

// 0x009148C0 (:1951)
void ui_document_mp_game_game_lobby_process()
{
    if (Bink_info_main.handle != 0)
    {
        mm_process_bink_video();
    }
    multi_player_menu_system_process();
    ui_add_for_processing(Mp_game_lobby_info.doc_handle);
    switch (Mp_game_lobby_info.state)
    {
    case MMS_L_LOADING:
        game_lobby_process_loading();
        break;
    case MMS_L_LOADED:
        game_lobby_process_loaded();
        break;
    case MMS_L_EXITING:
        game_lobby_process_exit();
        break;
    case MMS_L_UNLOADED:
        game_lobby_set_state(MMS_L_LOADING);
        break;
    default:
        break;
    }
    ui_process(Mp_game_lobby_info.control_id, false);
}

// 0x0090C290 (:1986)
void ui_document_mp_game_lobby_handshake(rl_renderer*)
{
    Mp_game_lobby_info.prev_state = Mp_game_lobby_info.state;
    if (Mp_game_lobby_info.pending_state != Mp_game_lobby_info.state)
    {
        game_lobby_do_state_change();
    }
    multi_player_menu_system_level_image_handshake();
}

// 0x008FBA80 (:1996)
void ui_document_mp_game_lobby_render(rl_renderer*)
{
    if (Mp_game_lobby_info.doc_handle == -1 || Mp_game_lobby_info.control_id == 0 || multi_game_lobby_pushed_to_game())
    {
        return;
    }
    if (Mp_game_lobby_info.state == MMS_L_LOADED && Mp_game_lobby_info.prev_state == MMS_L_LOADED)
    {
        ui_add_for_rendering(Mp_game_lobby_info.doc_handle);
    }
    ui_render(Mp_game_lobby_info.control_id, false);
}

// 0x00914970 (:2023)
void ui_document_mp_game_lobby_init()
{
    state_info info;
    info.enter = ui_document_mp_game_lobby_enter;
    info.exit = ui_document_mp_game_lobby_exit;
    info.process = ui_document_mp_game_game_lobby_process;
    info.handshake = ui_document_mp_game_lobby_handshake;
    info.render = ui_document_mp_game_lobby_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_MULTIPLAYER_GAME_LOBBY, info);
}
