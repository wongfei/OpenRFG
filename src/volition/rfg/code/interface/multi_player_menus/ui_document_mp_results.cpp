// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_results.cpp
//
// The match results (GS_MULTIPLAYER_RESULTS, UI.md UI5) on rfg_ui_mp_stats: the players sorted (by experience per team
// with the winners first, else by score; spectators last) with their name, badge, level, team, voice, host or party
// leader, party member and four stat columns (score, experience, kills, deaths; the mode's stats; favourites; bonus
// experience), paged four columns at a time and into the experience tally; the mode, map and team scores. Back leaves
// (the menus popped, the lobby session deleted unless system link or a party); matchmaking goes on to the next game.

#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_results.h"

#include "kaiko/gamelink/gamelink.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_string_processing.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_xp_tally.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_core/include/multi_join.h"
#include "volition/rfg/code/multi/multi_core/include/multi_voice.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_party.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode_shared.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/varray.h"

#include <string.h>

// ---- file-private types (rfg.pdb)
// mp_menu_exit_actions_results, mp_menu_states_results and mp_match_results_shell_info are in
// ui_document_mp_results.h since UI5 (the experience tally uses them).

// The results' stat columns (rfg.pdb; no other file names it).
enum mp_stat_entries
{
    MSE_SCORE = 0,
    MSE_XP = 1,
    MSE_KILLS = 2,
    MSE_DEATHS = 3,
    MSE_MODE_SPECIFIC_1 = 4,
    MSE_MODE_SPECIFIC_2 = 5,
    MSE_MODE_SPECIFIC_3 = 6,
    MSE_MODE_SPECIFIC_4 = 7,
    MSE_FAVORITE_WEAPON_BLANK = 8,
    MSE_FAVORITE_WEAPON = 9,
    MSE_FAVORITE_BACKPACK_BLANK = 10,
    MSE_FAVORITE_BACKPACK = 11,
    MSE_BONUS_XP = 12,
    MSE_NUM_UNIQUE_BONUSES = 13,
    MSE_LAST_PAGE_BLANK = 14,
    MSE_LAST_PAGE_BLANK2 = 15,
    NUM_MP_STAT_ENTRIES = 16,
};

enum results_hint_choices
{
    MPR_HINT_INVALID = -1,
    MPR_HINT_ABORT = 0,
    MPR_HINT_PARTY_UP = 1,
    MPR_HINT_MATCHMAKING = 2,
    MPR_HINT_CYCLE_LEFT = 3,
    MPR_HINT_CYCLE_RIGHT = 4,
    MPR_HINT_GAMERTAG = 5,
    NUM_MPR_HINTS = 6,
};

// ---- file-static globals
static const char* Ui_mp_shell_name = {"rfg_ui_mp_stats"}; // 0x01663704
static const char* Menu_data_item_names[6] = {"StatsHighlightIndex", "StatsHintInfo", "StatsTitles", "StatsHeaderInfo", "StatsModeInfo", "StatsScoreInfo"}; // 0x01663708
static const char* Stat_column_names[16] = {"MENU_SCORE", "MENU_XP", "MENU_KILLS", "MENU_DEATHS", "", "", "", "", "", "MENU_FAVORITE_WEAPON", "", "MENU_FAVORITE_BACKPACK", "MENU_BONUS_XP", "MENU_UNIQUE_BONUSES", "", ""}; // 0x01663720
static const char* Mode_colon_string = {"MENU_MODE_COLON"}; // 0x01663760
static const char* Map_colon_string = {"MENU_MAP_COLON"}; // 0x01663764
static const char* Back_string_results = {"MENU_BACK"}; // 0x01663768
static const char* Leave_party_string_results = {"MENU_LEAVE_PARTY"}; // 0x01663770
static const char* Match_results_string = {"MENU_MATCH_RESULTS"}; // 0x01663778
static results_hint_choices Current_mouse_hint_choice_results = MPR_HINT_INVALID; // 0x01663784, initial data ffffffff
// Shared with the experience tally (ui_document_mp_results.h, UI5).
mp_match_results_shell_info Mp_match_results_info; // 0x01666900
timestamp_realtime Auto_continue_timestamp; // 0x0166698C, initial data ffffffff
int Num_players; // 0x02BFD420
static int results_hint_config[6]; // 0x02C00DA0
static unsigned int Players[16]; // 0x02C00E88
// Written by the results' enter and the experience tally (a static of rfg.exe's interface unity; placed here).
bool Entering_results; // 0x02C03636
bool No_matchmaking; // 0x02C03637
static int Match_results_selection; // 0x02C03638
static int Stat_offset; // 0x02C0363C
bool Pending_pop; // 0x02C03640
static int results_num_hints; // 0x02C03644
// The hint groups (header constants in rfg.exe, this file's copies).
static const char* kGroup_name = {"row_group"}; // 0x01668640
static const char* kHintGroups[5] = {"hint_1_group", "hint_2_group", "hint_3_group", "hint_4_group", "hint_5_group"}; // 0x01668644

bool (*g_openrfgResultsContinue)();

// ---- internal functions

static void results_load_global_data_items();
static void results_unload_ui_information();

// 0x008F2E60
// Loaded: the data items and the rows' group; unloading: the session's left players removed by the host (system link
// or party lobby) or the session deleted (matchmaking), then the menus popped (the tally first; matchmaking pops its
// menus too, back to the party lobby or out of it).
static void results_do_state_change()
{
    do
    {
        Mp_match_results_info.state = Mp_match_results_info.pending_state;
        if (Mp_match_results_info.pending_state == MMS_R_LOADED)
        {
            results_load_global_data_items();
            vint_data_group* group = vint_data_group::create_new("StatsSetData");
            if (group != nullptr)
            {
                Mp_match_results_info.data_group_handle = group->get_handle();
                ui_add_data_group(Mp_match_results_info.doc_handle, Mp_match_results_info.data_group_handle);
            }
            Auto_continue_timestamp.invalidate();
        }
        else
        {
            if (Mp_match_results_info.pending_state != MMS_R_UNLOADING)
            {
                return;
            }
            results_unload_ui_information();
            if (Lobby_session == nullptr || Lobby_session->get_state() == MSS_NONE || Lobby_session->get_info()->systemlink_or_lan || Lobby_session->get_info()->party_lobby)
            {
                if (Lobby_session->am_i_host())
                {
                    // The players without a connection any more: their game data removed everywhere.
                    const session_types type = Lobby_session->get_info()->type;
                    const multi_game_data* data = multi_game_data_begin(type, false, false);
                    const multi_game_data* next = multi_game_data_next(data, false, false);
                    while (data != multi_game_data_end(Lobby_session->get_info()->type))
                    {
                        const multi_game_data* current = next;
                        if (Lobby_session->get_player_connection(data->handle) == nullptr)
                        {
                            multi_connection* connection = Lobby_session->get_connection_list();
                            while (connection != nullptr)
                            {
                                if (connection->connection_status != CS_DISCONNECTED && connection != Lobby_session->get_host_connection())
                                {
                                    multi_join_send_remove_gamedata(connection, data->handle);
                                }
                                connection = connection->next;
                                if (connection == Lobby_session->get_connection_list())
                                {
                                    break;
                                }
                            }
                            multi_game_data_remove_player(data->handle);
                        }
                        next = multi_game_data_next(current, false, false);
                        data = current;
                    }
                }
            }
            else
            {
                Lobby_session->session_delete();
            }
            if (Mp_match_results_info.exit_action == MMEA_R_POP)
            {
                if (gameseq_get_state() == GS_MULTIPLAYER_XP_TALLY)
                {
                    gameseq_pop_state();
                }
                gameseq_pop_state();
                if (!Multi_syslink)
                {
                    gameseq_pop_state();
                    if (No_matchmaking)
                    {
                        No_matchmaking = false;
                        gameseq_pop_state();
                        gameseq_pop_state();
                        if (!multi_game_party_in_party() || !multi_game_party_am_i_leader(false))
                        {
                            gameseq_pop_state();
                        }
                    }
                }
            }
            else if (Mp_match_results_info.exit_action == MMEA_R_PUSH)
            {
                gameseq_push_state(Mp_match_results_info.exit_push_target, true, true);
            }
            Mp_match_results_info.pending_state = MMS_R_UNLOADED;
        }
    } while (Mp_match_results_info.state != Mp_match_results_info.pending_state);
}

// 0x0088D230
// LTCG: new_state in EAX.
static void results_set_state(mp_menu_states_results new_state)
{
    Mp_match_results_info.pending_state = new_state;
}

// 0x0088D240
static void results_init_state()
{
    Mp_match_results_info.state = MMS_R_UNLOADED;
    Mp_match_results_info.pending_state = MMS_R_UNLOADED;
    Mp_match_results_info.prev_state = MMS_R_UNLOADED;
    Mp_match_results_info.exit_action = MMEA_R_POP;
    Mp_match_results_info.exit_push_target = GS_MULTIPLAYER_LIVE;
}

// 0x008AE210
static void results_init_info()
{
    Mp_match_results_info.group_item_handles.clear();
    for (int i = 0; i < 6; i++)
    {
        Mp_match_results_info.data_item_handles[i] = -1;
    }
    Mp_match_results_info.data_group_handle = -1;
}

// 0x0088D270
// LTCG: column in EAX, column_name in ESI. A column's title: the fixed names, or the mode's stat (only the even mode
// columns name one when the mode has two stats or fewer).
static void get_column_name(char* column_name, int column_name_length, mp_stat_entries column)
{
    if (column_name == nullptr || (int)column < 0)
    {
        return;
    }
    const unsigned int stat_index = (unsigned int)column - 4;
    if (stat_index < 4)
    {
        if (multi_gameplay_mode_get_mode_specific_stat_count() > 2)
        {
            multi_gameplay_mode_get_mode_specific_stat_name(column_name, column_name_length, (int)stat_index);
            return;
        }
        if (((int)stat_index % 2) == 0)
        {
            multi_gameplay_mode_get_mode_specific_stat_name(column_name, column_name_length, (int)stat_index / 2);
            return;
        }
        _snprintf_safe(column_name, (unsigned int)column_name_length, "%s", "");
        return;
    }
    strcpy(column_name, Stat_column_names[column]);
}

// 0x0088D2F0
// LTCG: text in ESI, text_len in EDI. A player's stat as text.
static void get_value_of_stat(char* text, int text_len, mp_stat_entries stat_entry, unsigned int player_game_data_handle)
{
    if (text == nullptr)
    {
        return;
    }
    text[0] = '\0';
    // Magic number: 16 stat entries.
    if ((unsigned int)stat_entry > 15)
    {
        return;
    }
    const multi_player_stats* stats = multi_game_stats_get_player_stats(player_game_data_handle);
    if (stats == nullptr)
    {
        return;
    }
    switch ((int)stat_entry)
    {
    case 0:
        _snprintf_safe(text, (unsigned int)text_len, "%d", (int)stats->score);
        return;
    case 1:
        _snprintf_safe(text, (unsigned int)text_len, "%d", (int)stats->xp);
        return;
    case 2:
        _snprintf_safe(text, (unsigned int)text_len, "%d", stats->kills);
        return;
    case 3:
        _snprintf_safe(text, (unsigned int)text_len, "%d", stats->deaths);
        return;
    case 4:
    case 5:
    case 6:
    case 7:
    {
        const int stat_index = (int)stat_entry - 4;
        if (multi_gameplay_mode_get_mode_specific_stat_count() > 2)
        {
            multi_gameplay_mode_get_mode_specific_stat_value(text, text_len, stat_index, player_game_data_handle);
        }
        else if ((stat_index % 2) == 0)
        {
            multi_gameplay_mode_get_mode_specific_stat_value(text, text_len, stat_index / 2, player_game_data_handle);
        }
        return;
    }
    case 9:
    {
        const weapon_info* weapon = weapons_get_weapon_info(stats->favorite_weapon);
        if (weapon != nullptr && weapon->weapon_inv_item_info != nullptr && weapon->weapon_inv_item_info->display_name != nullptr)
        {
            strncpy_safe(text, weapon->weapon_inv_item_info->display_name, (unsigned int)text_len);
            return;
        }
        break;
    }
    case 11:
        if (stats->favorite_backpack != -1)
        {
            strncpy_safe(text, multi_game_backpack_get_display_name(stats->favorite_backpack), (unsigned int)text_len);
            return;
        }
        break;
    case 12:
        _snprintf_safe(text, (unsigned int)text_len, "%d", (int)stats->bonus_xp);
        return;
    case 13:
        _snprintf_safe(text, (unsigned int)text_len, "%d", stats->count_unique_bonus_xp_events());
        return;
    default:
        // Magic number: 0x40 (the callers' buffer size, not text_len).
        strncpy_safe(text, "", 0x40);
        return;
    }
    strncpy_safe(text, "MENU_NONE", (unsigned int)text_len);
}

// 0x008D3820
// The six data items: the highlight index, hints, titles, column headers, the mode and map, the team scores.
static void results_load_global_data_items()
{
    int handle = 0;
    for (int i = 0; i < 6; i++)
    {
        vint_data_item* item = vint_data_item::create_new(Menu_data_item_names[i]);
        if (item != nullptr)
        {
            switch (i)
            {
            case 0:
            {
                vint_variant_type types[1] = {VINT_PROP_TYPE_INT};
                item->set_template(types, 1);
                break;
            }
            case 2:
            {
                vint_variant_type types[2] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL};
                item->set_template(types, 2);
                break;
            }
            case 3:
            {
                vint_variant_type types[5] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
                item->set_template(types, 5);
                break;
            }
            case 4:
            {
                vint_variant_type types[5] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
                item->set_template(types, 5);
                const char* mode_name = multi_gameplay_mode_get_display_name(multi_gameplay_mode_get());
                const multi_level_info* level = Mp_server_options.get_level();
                item->set_element(0, vint_variant(true));
                item->set_element(1, vint_variant(Mode_colon_string));
                item->set_element(2, vint_variant(mode_name));
                item->set_element(3, vint_variant(Map_colon_string));
                item->set_element(4, vint_variant(level->display_name));
                break;
            }
            case 5:
            {
                vint_variant_type types[3] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT};
                item->set_template(types, 3);
                const bool teams = multi_game_mode_has_teams();
                item->set_element(0, vint_variant(teams));
                if (teams)
                {
                    int element = 1;
                    for (int team = 0; team < 2; team++)
                    {
                        const multi_team_stats* stats = multi_game_stats_get_team_stats((human_teams)team);
                        if (stats != nullptr)
                        {
                            item->set_element(element, vint_variant((int)stats->score));
                            element++;
                        }
                    }
                }
                break;
            }
            default:
                break;
            }
            handle = (int)item->get_handle();
        }
        // Original: a failed create stores (and adds) the previous item's handle (0 for the first).
        Mp_match_results_info.data_item_handles[i] = handle;
        ui_add_data_item(Mp_match_results_info.doc_handle, handle);
    }
}

// 0x008C1110
// A player row: {name, badge, level, team, voice, host/leader, party, (unused), connected, selected, (unused), in a
// matchmaking party, the four stat columns}.
static void results_add_menu_option_data_item()
{
    vint_data_group* group = vint_data_group::find((unsigned int)Mp_match_results_info.data_group_handle);
    vint_data_item* item = group->add_data_item(nullptr);
    if (item == nullptr)
    {
        return;
    }
    vint_variant_type types[16] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT,
                                   VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL,
                                   VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL,
                                   VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
    item->set_template(types, 16);
    Mp_match_results_info.group_item_handles.add((int)item->get_handle());
}

// 0x008AE250
static void results_remove_menu_option_data_item()
{
    vint_data_group* group = vint_data_group::find((unsigned int)Mp_match_results_info.data_group_handle);
    const int last = Mp_match_results_info.group_item_handles.size() - 1;
    group->remove_data_item((unsigned int)Mp_match_results_info.group_item_handles[last]);
    Mp_match_results_info.group_item_handles.remove(last);
}

// 0x008D3BA0
static void results_load_data_groups()
{
    vint_data_group* group = vint_data_group::create_new("StatsSetData");
    if (group != nullptr)
    {
        Mp_match_results_info.data_group_handle = group->get_handle();
        ui_add_data_group(Mp_match_results_info.doc_handle, Mp_match_results_info.data_group_handle);
    }
}

// 0x008D3BD0
static void results_load_ui_information()
{
    results_load_global_data_items();
    results_load_data_groups();
}

// 0x008E43C0
// LTCG: the document handle in EDI.
static void results_unload_ui_information()
{
    vint_doc_info* info = ui_get_doc_info(Mp_match_results_info.doc_handle);
    if (info != nullptr)
    {
        info->release_data_groups();
        info->release_data_items();
        info->release_data_events();
    }
    results_init_info();
    multi_player_menu_system_close_message_box();
}

// 0x008E4450
// Back (or, in a matchmaking party, leave party or back to the party lobby) and the next game's countdown; with the
// tally reachable, the stat cycling hints.
static void results_process_data_item_hint_text(float dtime)
{
    vint_data_item* vdi = vint_data_item::find((unsigned int)Mp_match_results_info.data_item_handles[1]);
    if (vdi == nullptr)
    {
        return;
    }
    multi_player_menu_system_clear_hint_config(results_hint_config, 6, results_num_hints);
    // multi_player_menu_system_begin_hints, inlined.
    Num_hints_added = 0;
    Hint_data_item = vdi;
    if (!Lobby_session->get_info()->systemlink_or_lan)
    {
        g_gameLinkMutex.lock(0);
        kaiko::gamelink::GameLink* link = get_game_link_session();
        if (link->isPartyOwner() || !link->isPartyEnabled())
        {
            const char* back = Back_string_results;
            if (multi_game_party_am_i_leader(true) && multi_game_party_get_num_players_in_party() > 1)
            {
                back = "MENU_TIP_PARTY_LOBBY";
            }
            multi_player_menu_system_add_hint(localization_get_display_string_req(back, nullptr), CBA_MENU_ABORT);
            multi_player_menu_system_add_hint_config(results_num_hints, results_hint_config, MPR_HINT_ABORT);
            if (!Lobby_session->get_info()->party_lobby)
            {
                wchar_t next[256];
                if (!Auto_continue_timestamp.valid() || Auto_continue_timestamp.elapsed())
                {
                    _snprintf_safe(next, 0x100, L"%ls", localization_get_display_string_req("MENU_NEXT_GAME", nullptr));
                }
                else
                {
                    // Magic number: the time left in whole seconds.
                    _snprintf_safe(next, 0x100, L"%ls (%d)", localization_get_display_string_req("MENU_NEXT_GAME", nullptr), Auto_continue_timestamp.time_until() / 1000);
                }
                multi_player_menu_system_add_hint(next, CBA_MENU_X);
                multi_player_menu_system_add_hint_config(results_num_hints, results_hint_config, MPR_HINT_MATCHMAKING);
            }
        }
        else
        {
            multi_player_menu_system_add_hint(localization_get_display_string_req(Leave_party_string_results, nullptr), CBA_MENU_ABORT);
            multi_player_menu_system_add_hint_config(results_num_hints, results_hint_config, MPR_HINT_ABORT);
        }
        g_gameLinkMutex.unlock();
    }
    else
    {
        multi_player_menu_system_add_hint(localization_get_display_string_req(Back_string_results, nullptr), CBA_MENU_ABORT);
        multi_player_menu_system_add_hint_config(results_num_hints, results_hint_config, MPR_HINT_ABORT);
    }
    if (Xp_tally_connected_when_entered)
    {
        multi_player_menu_system_add_hint(localization_get_display_string_req("", nullptr), CBA_MENU_L2);
        multi_player_menu_system_add_hint_config(results_num_hints, results_hint_config, MPR_HINT_CYCLE_LEFT);
        multi_player_menu_system_add_hint(localization_get_display_string_req("MENU_CYCLE_STATS", nullptr), CBA_MENU_R2);
        multi_player_menu_system_add_hint_config(results_num_hints, results_hint_config, MPR_HINT_CYCLE_RIGHT);
    }
    multi_player_menu_system_end_hints();
}

// 0x0088D4C0
static void results_process_data_item_titles(float dtime)
{
    vint_data_item* vdi = vint_data_item::find((unsigned int)Mp_match_results_info.data_item_handles[2]);
    if (vdi == nullptr)
    {
        return;
    }
    vdi->set_element(0, vint_variant(Match_results_string));
    vdi->set_element(1, vint_variant(gameseq_get_state() != GS_MULTIPLAYER_XP_TALLY));
}

// 0x0088D550
// The column headers: the gamer tag, then the four stat columns of the page (the score column is blank in team modes).
static void results_process_data_item_info_text(float dtime)
{
    vint_data_item* vdi = vint_data_item::find((unsigned int)Mp_match_results_info.data_item_handles[3]);
    if (vdi == nullptr)
    {
        return;
    }
    vdi->set_element(0, vint_variant("PLT_GAMERTAG"));
    char name[64];
    for (int i = 0; i < 4; i++)
    {
        const int column = Stat_offset + i;
        if (column == 0 && multi_game_mode_has_teams())
        {
            vdi->set_element(i + 1, vint_variant(""));
            continue;
        }
        if (column >= 0)
        {
            get_column_name(name, sizeof(name), (mp_stat_entries)column);
        }
        vdi->set_element(i + 1, vint_variant((const char*)name));
    }
}

// 0x008C1210
// One row per player (see results_add_menu_option_data_item); spectators and columns past the stats are blank.
static void results_process_data_group_menu_options(float dtime)
{
    vint_data_group* group = vint_data_group::find((unsigned int)Mp_match_results_info.data_group_handle);
    if (group == nullptr)
    {
        return;
    }
    const int rows = (int)group->get_num_data_items();
    const int players = Num_players;
    for (int i = rows; i < players; i++)
    {
        results_add_menu_option_data_item();
    }
    const int total_rows = rows < players ? players : rows;
    char value[64];
    char name[192];
    for (int row = 0; row < players; row++)
    {
        const multi_game_data* data = multi_game_data_get(Players[row], true, false);
        vint_data_item* item = vint_data_item::find((unsigned int)Mp_match_results_info.group_item_handles[row]);
        if (item == nullptr || data == nullptr)
        {
            continue;
        }
        const int voice = (int)multi_voice_get_icon_talk_state(data->player_uid);
        bool leader = false;
        bool party = false;
        const bool connected = Lobby_session->get_player_connection(data->player_uid) != nullptr || multi_game_party_has_player(data->player_uid);
        bool matchmaking_party = false;
        if (multi_game_party_in_party() && !Multi_syslink)
        {
            leader = multi_game_party_is_player_leader(data->player_uid);
            party = multi_game_party_has_player(data->player_uid);
            if (Lobby_session->get_state() != MSS_NONE && !Lobby_session->get_info()->party_lobby)
            {
                matchmaking_party = true;
            }
        }
        else if (Lobby_session->get_state() != MSS_NONE && Lobby_session->get_info()->systemlink_or_lan && Lobby_session->get_host_connection() != nullptr)
        {
            // A system link host's row is marked.
            leader = Lobby_session->get_player_connection(data->player_uid) == Lobby_session->get_host_connection();
        }
        vint_encode_wide_text(name, sizeof(name), data->display_name);
        item->set_element(0, vint_variant((const char*)name));
        item->set_element(1, vint_variant((int)data->player_badge));
        item->set_element(2, vint_variant((int)data->player_level));
        item->set_element(3, vint_variant((int)data->team));
        item->set_element(4, vint_variant(voice));
        item->set_element(5, vint_variant(leader));
        item->set_element(6, vint_variant(party));
        item->set_element(7, vint_variant(false));
        item->set_element(8, vint_variant(connected));
        item->set_element(9, vint_variant(row == Match_results_selection));
        item->set_element(10, vint_variant(false));
        item->set_element(11, vint_variant(matchmaking_party));
        for (int column = 0; column < 4; column++)
        {
            const int stat = Stat_offset + column;
            if (data->team == HUMAN_TEAM_MP_SPECTATOR || stat > 15 || (stat == 0 && multi_game_mode_has_teams()))
            {
                item->set_element(column + 12, vint_variant(""));
            }
            else
            {
                get_value_of_stat(value, sizeof(value), (mp_stat_entries)stat, data->handle);
                item->set_element(column + 12, vint_variant((const char*)value));
            }
        }
    }
    for (int i = players; i < total_rows; i++)
    {
        results_remove_menu_option_data_item();
    }
}

// 0x008E4740
static void results_process_data_items()
{
    results_process_data_item_hint_text(0.0f);
    results_process_data_item_titles(0.0f);
    results_process_data_item_info_text(0.0f);
    results_process_data_group_menu_options(0.0f);
}

// 0x008F30C0
static void results_process_loading()
{
    switch (ui_get_doc_asset_state(Mp_match_results_info.doc_handle))
    {
    case ST_LOADING:
        return;
    case ST_LOADED:
        Mp_match_results_info.pending_state = MMS_R_LOADED;
        return;
    case ST_UNLOADED:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        // fall through
    case ST_ERROR:
        // Magic number: a 0.5 s fade out.
        ui_unload_document(Mp_match_results_info.doc_handle, DFOT_NONE, 0.5f);
        Mp_match_results_info.doc_handle = -1;
        Mp_match_results_info.pending_state = MMS_R_UNLOADING;
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
}

// 0x008D3C00
// With the focus (or under the tally): the hints, then (not under the tally) the gamer tag button; a click on a row
// selects it.
static void results_input_process_mouse(bool& events_consumed)
{
    const int top = Current_input_lock_id != -1 ? Current_input_lock_id : (Input_types.size() - 1 < 0 ? 1 : Input_types[Input_types.size() - 1].id);
    if (Mp_match_results_info.input_id != top && gameseq_get_state() != GS_MULTIPLAYER_XP_TALLY)
    {
        return;
    }
    const char* names[3] = {kHintGroups[0], kHintGroups[1], kHintGroups[2]};
    int hint = ui_mouse_process_hints(names, 3, Mp_match_results_info.doc_handle, true);
    if (hint == -1)
    {
        names[0] = kHintGroups[3];
        names[1] = kHintGroups[4];
        hint = ui_mouse_process_hints(names, 2, Mp_match_results_info.doc_handle, true);
        if (hint != -1)
        {
            hint += 3;
        }
    }
    if (gameseq_get_state() == GS_MULTIPLAYER_XP_TALLY)
    {
        if (hint != -1)
        {
            Current_mouse_hint_choice_results = hint == 5 ? MPR_HINT_GAMERTAG : (results_hint_choices)results_hint_config[hint];
        }
    }
    else if (hint == -1)
    {
        names[0] = "row_highlight_button";
        names[1] = "row_highlight_bg";
        if (ui_mouse_process_hints(names, 2, Mp_match_results_info.doc_handle, false) != -1)
        {
            Current_mouse_hint_choice_results = MPR_HINT_GAMERTAG;
        }
    }
    else
    {
        Current_mouse_hint_choice_results = hint == 5 ? MPR_HINT_GAMERTAG : (results_hint_choices)results_hint_config[hint];
    }
    if (!mouse_just_released(0) || (Utility_doc_handle != -1 && ui_is_msgbox_active(-2)))
    {
        return;
    }
    vint_doc_info* info = ui_get_doc_info(Mp_match_results_info.doc_handle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    char name[128];
    for (int i = 0; i < Mp_match_results_info.group_item_handles.size(); i++)
    {
        if (i == 0)
        {
            keen::formatString(name, sizeof(name), "%s", kGroup_name);
        }
        else
        {
            keen::formatString(name, sizeof(name), "%s%d", kGroup_name, i + 1);
        }
        if (ui_is_mouse_over_vint_object(name, info->m_doc))
        {
            multi_player_menu_system_set_menu_selection(i, Match_results_selection, Mp_match_results_info.group_item_handles.size());
            return;
        }
    }
}

// 0x008D3E30
// LTCG: reads ECX (the events flag).
static void results_process_input()
{
    bool events_consumed = false;
    Current_mouse_hint_choice_results = MPR_HINT_INVALID;
    results_input_process_mouse(events_consumed);
    multi_player_menu_system_handle_menu_key_input(Match_results_selection, Mp_match_results_info.group_item_handles.size(), Mp_match_results_info.input_id, events_consumed);
}

// A Button_info button pressed (and not waiting for its release) with the results' input focus.
static bool results_button_pressed(int button)
{
    const int lock = Current_input_lock_id != -1 ? Current_input_lock_id : 0;
    bool focus = Mp_match_results_info.input_id == -1 && lock == 0;
    if (!focus)
    {
        const int top = Current_input_lock_id != -1 ? Current_input_lock_id : (Input_types.size() - 1 < 0 ? 1 : Input_types[Input_types.size() - 1].id);
        focus = Mp_match_results_info.input_id == top;
    }
    return focus && !Button_info.binfo[button].m_button_wait_for_up && Button_info.binfo[button].m_button_input > 0.0f;
}

// 0x0090D3E0
// Buttons (Button_info indices): 10 the player's options, 11 back, 0x13/0x11 the next and previous page, 14 the next
// game (matchmaking; also after the auto continue).
static void results_process_loaded()
{
    results_process_data_items();
    bool gamertag = results_button_pressed(10);
    bool back = results_button_pressed(0xB);
    bool next_page = results_button_pressed(0x13);
    bool prev_page = results_button_pressed(0x11);
    const bool next_game_pressed = results_button_pressed(0xE);
    // results_process_input, inlined.
    Current_mouse_hint_choice_results = MPR_HINT_INVALID;
    bool events_consumed = false;
    results_input_process_mouse(events_consumed);
    multi_player_menu_system_handle_menu_key_input(Match_results_selection, Mp_match_results_info.group_item_handles.size(), Mp_match_results_info.input_id, events_consumed);
    back = back || Current_mouse_hint_choice_results == MPR_HINT_ABORT;
    // Test seam (UI.md 2.7 item 2, off by default): the command line's results time ran out.
    if (g_openrfgResultsContinue != nullptr && g_openrfgResultsContinue())
    {
        back = true;
    }
    next_page = next_page || Current_mouse_hint_choice_results == MPR_HINT_CYCLE_RIGHT;
    prev_page = prev_page || Current_mouse_hint_choice_results == MPR_HINT_CYCLE_LEFT;
    const bool next_game = next_game_pressed || Current_mouse_hint_choice_results == MPR_HINT_MATCHMAKING;
    gamertag = gamertag || Current_mouse_hint_choice_results == MPR_HINT_GAMERTAG;
    vint_data_item* highlight = vint_data_item::find((unsigned int)Mp_match_results_info.data_item_handles[0]);
    if (highlight != nullptr)
    {
        highlight->set_element(0, vint_variant(Match_results_selection));
    }
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = get_game_link_session();
    if (gamertag)
    {
        if (Match_results_selection < Num_players && Match_results_selection >= 0)
        {
            const multi_game_data* data = multi_game_data_get(Players[Match_results_selection], true, false);
            if (data == nullptr)
            {
                audiolib_cue_play(Audio_info[11], nullptr, nullptr);
            }
            else
            {
                multi_player_menu_system_pull_up_player_option_message_box(data->player_uid, data->display_name, true);
                audiolib_cue_play(Audio_info[4], nullptr, nullptr);
            }
        }
        g_gameLinkMutex.unlock();
        return;
    }
    if (back)
    {
        if (!multi_game_party_in_party() || multi_game_party_am_i_leader(false))
        {
            Pending_pop = false;
            No_matchmaking = true;
            Mp_match_results_info.exit_action = MMEA_R_POP;
            Mp_match_results_info.pending_state = MMS_R_EXITING;
        }
        else
        {
            multi_player_menu_system_pull_up_leave_confirm_message_box();
        }
        audiolib_cue_play(Audio_info[11], nullptr, nullptr);
        g_gameLinkMutex.unlock();
        return;
    }
    if (!Lobby_session->get_info()->systemlink_or_lan && !Lobby_session->get_info()->party_lobby
        && (next_game || (Auto_continue_timestamp.valid() && Auto_continue_timestamp.elapsed()))
        && (multi_game_party_am_i_leader(false) || !link->isPartyEnabled()))
    {
        Lobby_session->session_delete();
        ui_document_mp_results_ui_pop(false);
        audiolib_cue_play(Audio_info[6], nullptr, nullptr);
        g_gameLinkMutex.unlock();
        return;
    }
    if (next_page)
    {
        ui_document_mp_results_next_page();
    }
    else if (prev_page)
    {
        ui_document_mp_results_prev_page();
    }
    else if ((Lobby_session->get_state() == MSS_NONE && multi_game_lobby_left_by_choice()) || Pending_pop)
    {
        ui_document_mp_results_ui_pop(true);
    }
    g_gameLinkMutex.unlock();
}

// ---- external interface

// 0x00917300 (:1386)
void ui_document_mp_results_init()
{
    state_info info;
    info.enter = ui_document_mp_results_enter;
    info.exit = ui_document_mp_results_exit;
    info.process = ui_document_mp_results_process;
    info.handshake = ui_document_mp_results_handshake;
    info.render = ui_document_mp_results_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_MULTIPLAYER_RESULTS, info);
}

// 0x0090D8A0 (:1403)
// The players sorted: in team modes the winners (guerrillas on a draw) then the losers by experience, else all by
// score; the spectators after them; at least me.
void ui_document_mp_results_enter(game_state prev_state)
{
    Mp_match_results_info.state = MMS_R_UNLOADED;
    Mp_match_results_info.pending_state = MMS_R_UNLOADED;
    Mp_match_results_info.prev_state = MMS_R_UNLOADED;
    Entering_results = true;
    results_init_info();
    Mp_match_results_info.exit_action = MMEA_R_POP;
    Mp_match_results_info.exit_push_target = GS_MULTIPLAYER_LIVE;
    // Magic number: a 0.5 s fade in.
    Mp_match_results_info.doc_handle = ui_load_document(Ui_mp_shell_name, DFIT_NONE, 0.5f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    Mp_match_results_info.control_id = ui_request_control_id();
    Mp_match_results_info.input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    if (multi_game_mode_has_teams())
    {
        Num_players = 0;
        human_teams winner = multi_game_mode_get_round_winner_team();
        if (winner == HUMAN_TEAM_NONE)
        {
            winner = HUMAN_TEAM_GUERILLA;
        }
        multi_game_stats_sort_players_based_on_xp(winner, Players, Num_players);
        unsigned int losers[16];
        int num_losers = 0;
        multi_game_stats_sort_players_based_on_xp(multi_game_mode_get_opposite_team(winner), losers, num_losers);
        memcpy(Players + Num_players, losers, num_losers * sizeof(unsigned int));
        Num_players += num_losers;
    }
    else
    {
        multi_game_stats_sort_players_based_on_score(HUMAN_TEAM_NONE, Players, Num_players);
    }
    // Magic number: NUM_SESSION_TYPES (3) for the session type, as rfg.exe passes it (the game data's current set).
    for (const multi_game_data* data = multi_game_data_begin(NUM_SESSION_TYPES, false, false); data != multi_game_data_end(NUM_SESSION_TYPES); data = multi_game_data_next(data, false, false))
    {
        if (data->team == HUMAN_TEAM_MP_SPECTATOR)
        {
            Players[Num_players] = data->handle;
            Num_players++;
        }
    }
    if (Num_players < 1)
    {
        Num_players = 1;
        Players[0] = multi_game_data_get_my_handle(NUM_SESSION_TYPES);
    }
    Mp_match_results_info.state = MMS_R_INVALID;
    Stat_offset = 0;
    No_matchmaking = false;
    Mp_match_results_info.pending_state = MMS_R_LOADING;
}

// 0x008F3160 (:1478)
// Unless the level load follows, the game data's players are cleared.
void ui_document_mp_results_exit(game_state new_state)
{
    results_unload_ui_information();
    ui_relinquish_control_id(Mp_match_results_info.control_id);
    // Magic number: a 0.5 s fade out.
    ui_unload_document(Mp_match_results_info.doc_handle, DFOT_NONE, 0.5f);
    Mp_match_results_info.doc_handle = -1;
    ui_relinquish_input_id(Mp_match_results_info.input_id);
    multi_game_lobby_clear_pushed_to_game();
    if (gameseq_get_pending_state() == GS_LOAD)
    {
        return;
    }
    multi_game_data_clear_players(NUM_SESSION_TYPES);
    multi_game_lobby_clear_pushed_to_game();
}

// 0x009153F0 (:1502)
void ui_document_mp_results_process()
{
    if (Bink_info_main.handle != 0)
    {
        mm_process_bink_video();
    }
    multi_player_menu_system_process();
    ui_add_for_processing(Mp_match_results_info.doc_handle);
    switch (Mp_match_results_info.state)
    {
    case MMS_R_LOADING:
        results_process_loading();
        break;
    case MMS_R_LOADED:
        results_process_loaded();
        break;
    case MMS_R_EXITING:
        results_set_state(MMS_R_UNLOADING);
        break;
    case MMS_R_UNLOADED:
        gameseq_pop_state();
        break;
    default:
        break;
    }
    ui_process(Mp_match_results_info.control_id, false);
}

// 0x008F3210 (:1540)
void ui_document_mp_results_handshake(rl_renderer*)
{
    Mp_match_results_info.prev_state = Mp_match_results_info.state;
    if (Mp_match_results_info.pending_state != Mp_match_results_info.state)
    {
        results_do_state_change();
    }
}

// 0x008FC120 (:1548)
void ui_document_mp_results_render(rl_renderer*)
{
    if (Mp_match_results_info.doc_handle == -1 || Mp_match_results_info.control_id == 0)
    {
        return;
    }
    if (Mp_match_results_info.state == MMS_R_LOADED && Mp_match_results_info.prev_state == MMS_R_LOADED)
    {
        ui_add_for_rendering(Mp_match_results_info.doc_handle);
    }
    ui_render(Mp_match_results_info.control_id, false);
}

// 0x008AE2B0 (:1565)
// The next four stat columns, after the last page the tally (unless spectating), from the tally back to the results.
void ui_document_mp_results_next_page()
{
    if (gameseq_get_state() == GS_MULTIPLAYER_XP_TALLY)
    {
        gameseq_pop_state();
    }
    else
    {
        // Magic numbers: four columns a page, 16 stat entries.
        if (Stat_offset + 4 < 16)
        {
            Stat_offset += 4;
            audiolib_cue_play(Audio_info[8], nullptr, nullptr);
            return;
        }
        if (!multi_game_spectating())
        {
            gameseq_push_state(GS_MULTIPLAYER_XP_TALLY, true, false);
            audiolib_cue_play(Audio_info[8], nullptr, nullptr);
            return;
        }
    }
    Stat_offset = 0;
    audiolib_cue_play(Audio_info[8], nullptr, nullptr);
}

// 0x008AE330 (:1584)
void ui_document_mp_results_prev_page()
{
    if (gameseq_get_state() == GS_MULTIPLAYER_XP_TALLY)
    {
        gameseq_pop_state();
    }
    else
    {
        if (Stat_offset - 4 >= 0)
        {
            Stat_offset -= 4;
            audiolib_cue_play(Audio_info[7], nullptr, nullptr);
            return;
        }
        if (!multi_game_spectating())
        {
            gameseq_push_state(GS_MULTIPLAYER_XP_TALLY, true, false);
            audiolib_cue_play(Audio_info[7], nullptr, nullptr);
            return;
        }
    }
    // Magic number: the last page's first column.
    Stat_offset = 12;
    audiolib_cue_play(Audio_info[7], nullptr, nullptr);
}

// 0x0088D6B0 (:1603)
void ui_document_mp_results_ui_pop(bool no_matchmaking)
{
    Pending_pop = false;
    No_matchmaking = no_matchmaking;
    Mp_match_results_info.exit_action = MMEA_R_POP;
    Mp_match_results_info.pending_state = MMS_R_EXITING;
}

// 0x0088D6E0 (:1611)
void ui_document_mp_results_ui_set_pending_pop()
{
    Pending_pop = true;
}

// 0x0088D6F0 (:1616)
bool ui_document_mp_results_pending_pop()
{
    return Pending_pop;
}
