// Original: volition/rfg/code/multi/multi_game/multi_game_lobby.cpp
//
// The game lobby: the lobby session (created by the host, joined by the clients), its state machine (lobby_process each
// frame, multi_game_lobby_set_state on every change), the best-host handshake before a match, pushing the lobby into
// the game (server and client start), ready-ups and vetoes, the lobby state packet the host sends its clients, and the
// game packet table.
// A public lobby also waits for players and fair teams, and searches for balanced teams; a system link or private lobby
// never enters those states.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_lobby.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_game_bandwidth.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_core/include/multi_leave.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_core/include/multi_voice.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_spectator.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_backpack_rack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_callbacks.h"
#include "volition/rfg/code/multi/multi_game/multi_game_classes.h"
#include "volition/rfg/code/multi/multi_game/multi_game_client_create.h"
#include "volition/rfg/code/multi/multi_game/multi_game_client_predict.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_destruction.h"
#include "volition/rfg/code/multi/multi_game/multi_game_destruction_migration.h"
#include "volition/rfg/code/multi/multi_game/multi_game_io.h"
#include "volition/rfg/code/multi/multi_game/multi_game_matchmaking_generic.h"
#include "volition/rfg/code/multi/multi_game/multi_game_migration.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_movers.h"
#include "volition/rfg/code/multi/multi_game/multi_game_party.h"
#include "volition/rfg/code/multi/multi_game/multi_game_player.h"
#include "volition/rfg/code/multi/multi_game/multi_game_playlists.h"
#include "volition/rfg/code/multi/multi_game/multi_game_preferences.h"
#include "volition/rfg/code/multi/multi_game/multi_game_respawn.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/multi/multi_game/multi_game_weapons.h"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/os/scheduler.h"
#include "volition/vlib/code/util/varray.h"
#include "keen/base/systemtimer.hpp"
#include "kaiko/gamelink/gamelink.hpp"

#include <string.h>

// ---- file-private types (rfg.pdb)
enum ending_game_state_machine
{
    EGSM_STOP_GAME = 0,
    EGSM_RESET_CONTEXTS_AND_PROPERTIES = 1,
    EGSM_ENDED = 2,
    NUM_ENDING_GAME_STATE_MACHINES = 3,
};

// The file statics and internal functions; the packet table at the end is the part MP3 took first.
// ---- file-static globals
// session_types Game_bandwidth_test_session_type (0x0167D83C) is used here too; placed in multi_core_game_bandwidth.cpp.
// bool Current_mode_has_exited (0x0167DD7B) is used here too; placed in multi_game.cpp.
// player_bandwidth_info Player_bandwidth[6] (0x0167DE2C) is used here too; placed in multi_core_game_bandwidth.cpp.
// multi_gameplay_mode Multi_gameplay_mode (0x0167DEE0) is used here too; placed in multi_gameplay_mode.cpp.
// unsigned int Local_game_data_handles[4] (0x0167E5F4) is used here too; placed in multi_join.cpp.
static int Team_assignment_attempts_per_frame = 1000; // 0x0167E70C
// timestamp_realtime Game_last_test_packet_rcvd_time (0x01691798) is used here too; placed in multi_core_game_bandwidth.cpp.
// timestamp_realtime Game_bandwidth_test_start_time (0x0169179C) is used here too; placed in multi_core_game_bandwidth.cpp.
static timestamp_realtime Lobby_timeout; // 0x01692C48, initial data ffffffff
static game_match Pending_session_info; // 0x01692C50
static multi_session_info Pending_session_coordinator_id; // 0x01692C90
static timestamp_realtime Server_options_transmit_interval; // 0x01692CC0, initial data ffffffff
static multi_session_info Game_session_info; // 0x01692CC8
static farray<multi_player_uid,16> Players_acked_for_start; // 0x01692CF8
// multi_timer Timer_time (0x01693A64) is used here too; placed in multi_core_platform.cpp.
static const multi_game_playlist_entry* Auto_launch_veto_entry; // 0x02FE5BA0
static bool I_am_ready; // 0x02FE5C62
// multi_game_playlist* Playlists (0x02FE6C70) is used here too; placed in multi_core_matchmaking_generic.cpp.
static bool Pending_session_exists; // 0x02FE6C76
// bool jip_destruction_sync_possible (0x02FE6C77) is used here too; placed in multi.cpp.
static int Team_combinations_attempted; // 0x02FE6D04
static team_info Teams[2]; // 0x02FE6D08
static unsigned int Lobby_timeout_set_time; // 0x02FE80F0
static bool Lobby_best_host_id_acked; // 0x02FE82EE
static bool Lobby_best_host_state_acked; // 0x02FE82EF
// multi_game_lobby_state_machine Lobby_state (0x02FE8344) is used here too; placed in multi_core_session_base.cpp.
// team_info Team_disparity (0x02FE8348) is used here too; placed in multi_game_camera.cpp.
static multi_game_lobby_state_machine Lobby_last_state; // 0x02FE8D14
// multi_backpack_info* Backpack_info[16] (0x02FE8D18) is used here too; placed in multi_game_backpack_rack.cpp.
static bool Ready_packet_sent; // 0x02FE8D58
// test_bw_states Game_bandwidth_state (0x02FE8DC8) is used here too; placed in multi_core_game_bandwidth.cpp.
static bool Server_thinks_I_am_ready; // 0x02FE8DD7
static int Max_players_observed; // 0x02FE8E48
multi_session* Lobby_session; // 0x02FE6D40
static int Num_groups; // 0x02FE8E54
// test_bw_result Bandwidth_test_result (0x02FE8E58) is used here too; placed in multi_core_game_bandwidth.cpp.
static team_assignment_group Groups[16]; // 0x02FE9080
static int Group_team_assignments[16]; // 0x02FEB300
static const multi_game_playlist_entry* Auto_launch_match_entry; // 0x02FEB388
static int Split_groups; // 0x02FEB390
static multi_player_uid Lobby_best_host_player_uid; // 0x02FEB4AC
// unsigned int Clock_current_time (0x02FEB538) is used here too; placed in multi.cpp.
// test_management_state Tm_state (0x02FEB568) is used here too; placed in multi_core_game_bandwidth.cpp.
// bool (*Game_voice_cb)(multi_connection*) (0x02FEB628) is used here too; placed in multi_voice.cpp.
// bool Spectating (0x02FEB659) is used here too; placed in multi.cpp.
static bool Left_lobby_by_choice; // 0x02FEB68B
// mp_player_class_info* Default_class_info (0x02FEB694) is used here too; placed in multi_game.cpp.
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.
static bool Pushed_to_game; // 0x02FEB6B0
static bool Send_lobby_state_update; // 0x02FEB6B1
static bool Sent_play_history; // 0x02FEB6B2
// bool Vetoes_allowed (0x02FEB6B3) is used here too; placed in multi_game_mode_common.cpp.
static ending_game_state_machine Endgamestate; // 0x02FEB6B4
// bool Vetoed_map (0x02FEB6B8) is used here too; placed in multi_game_mode_common.cpp.
static bool Force_allow_single_player; // 0x02FEB6B9
static bool Launch_regardless_of_teams_or_players; // 0x02FEB6BA
static bool Ignore_minimum_players; // 0x02FEB6BB
static int Veto_count; // 0x02FEB6BC
static bool Assume_bad_teams; // 0x02FEB6C0
static bool Ignore_ready_ups; // 0x02FEB6C1
// migration_state Migration_state (0x02FEB6C4) is used here too; placed in multi.cpp.
// preferences_members* Preferences (0x02FEB70C) is used here too; placed in multi_core_session_generic.cpp.

// The lobby timeouts per state (ms) and the server options resend interval.
static const int Min_players_timeout_ms = 50000;
static const int Countdown_timeout_ms = 16000;
static const int Best_host_timeout_ms = 5000;
static const int Late_joiner_timeout_ms = 60000;
static const int Server_options_interval_ms = 500;
// The match query resend interval (ms, keen::SystemTimer milliseconds).
static const unsigned int Match_query_interval_ms = 200;
// PT_LOBBY_CHOOSE_BEST_HOST subtypes (the byte after the header).
static const unsigned char Best_host_id = 1;
static const unsigned char Best_host_id_ack = 2;
static const unsigned char Best_host_state_ack = 4;
// PT_READY_FOR_GAME_START flags.
static const unsigned char Ready_flag_ready = 1;
static const unsigned char Ready_flag_sender_is_player = 2;

// ---- internal functions
// Defined in multi_gameplay_mode.cpp (later in the unity TU).
static void multi_gameplay_mode_change(multi_gameplay_mode new_mode); // 0x009EFFB0 (multi_gameplay_mode.cpp:580)
static void find_best_team_assignments(team_info& best_disparity, bool split_teams_to_minimize_disparity, int team_offset); // 0x009AF280 (:839)
// LTCG: num_combinations in ECX.
static void search_team_combinations(int num_combinations, bool allow_split); // 0x00999120 (:1002)
static void multi_game_lobby_set_state(multi_game_lobby_state_machine state, unsigned int as_of_multi_time); // 0x009FE5E0 (:1296)
static multi_session_join_status multi_game_player_may_join(multi_connection* cp, int join_group_size); // 0x0097BBD0 (:1255)
static bool multi_game_lobby_voice_should_send(multi_connection* cp); // 0x0097BC60 (:1473)
static multi_session_join_status multi_game_lobby_player_may_join(multi_connection* cp, int join_group_size); // 0x009AF640 (:1500)
static void multi_game_lobby_player_joined(multi_connection* cp); // 0x009AF910 (:1624)
static void multi_game_lobby_player_left(multi_connection* cp); // 0x009AFAA0 (:1686)
static bool multi_game_lobby_migration_allowed(multi_session_base* sp); // 0x009569B0 (:1715)
static void multi_game_lobby_migration_succeeded(multi_session_base* sp); // 0x009FE9B0 (:1732)
static void multi_game_lobby_migration_failed(multi_session_base* sp); // 0x009FD430 (:1741)

// Port helper (rfg.exe inlines it at every use): a playlist entry's mode, level and options become the server options.
static void apply_playlist_entry(const multi_game_playlist_entry* entry)
{
    Mp_server_options.set_mode(entry->mode);
    Mp_server_options.set_level(entry->level);
    Mp_server_options.generic_options = *entry->settings->generic_options;
    Mp_server_options.mode_options = *entry->settings->mode_specific_options;
}

// Port helper (rfg.exe inlines it): the playlist with this id in the Playlists ring, or nullptr.
static multi_game_playlist* find_playlist(unsigned int playlist_id)
{
    multi_game_playlist* plp = Playlists;
    while (plp != nullptr)
    {
        if (plp->playlist_id.get_crc() == playlist_id)
        {
            return plp;
        }
        plp = plp->next;
        if (plp == Playlists)
        {
            break;
        }
    }
    return nullptr;
}

// 0x0097B8E0 (:231): console command (unregistered in retail): the next lobby timeout now
void unused_dcf_mp_auto_launch_force_next_state()
{
    Lobby_timeout.set(0);
    Lobby_timeout_set_time = Clock_current_time;
    Send_lobby_state_update = true;
}

// 0x0097B900 (:236): console command (unregistered in retail): the lobby timeout in ms (-1: none)
void unused_dcf_mp_auto_launch_set_timer()
{
    // 8: an integer argument
    if (!console_get_arg(8, false))
    {
        return;
    }
    if (Console_arg_int == -1)
    {
        Lobby_timeout.invalidate();
    }
    else
    {
        Lobby_timeout.set(Console_arg_int);
    }
    Lobby_timeout_set_time = Clock_current_time;
    Send_lobby_state_update = true;
}

// 0x0097BA40
team_assignment_player_info& team_assignment_player_info::operator=(const team_assignment_player_info& __that)
{
    player_uid = __that.player_uid;
    skill = __that.skill;
    return *this;
}

// 0x0097BA80
team_assignment_group& team_assignment_group::operator=(const team_assignment_group& __that)
{
    leader_uid = __that.leader_uid;
    for (int i = 0; i < 16; i++)
    {
        players[i] = __that.players[i];
    }
    num_players = __that.num_players;
    aggregate_skill = __that.aggregate_skill;
    return *this;
}

// 0x00955FF0 (:572). LTCG: lhs in ESI, rhs in EDX.
static bool team_info_less_than(const team_info& lhs, const team_info& rhs)
{
    if (lhs.num_players < rhs.num_players)
    {
        return true;
    }
    if (lhs.num_players != rhs.num_players)
    {
        return false;
    }
    return lhs.skill.skill_ranking < rhs.skill.skill_ranking || (lhs.skill.skill_ranking == rhs.skill.skill_ranking && lhs.skill.skill_uncertainty < rhs.skill.skill_uncertainty);
}

// Port helper (inlined twice in rfg.exe): the spread between the teams, per value on its own
static void team_info_spread(team_info& disparity, const team_info* teams, const int num_teams)
{
    int min_players = teams[0].num_players;
    int max_players = teams[0].num_players;
    double min_ranking = teams[0].skill.skill_ranking;
    double max_ranking = teams[0].skill.skill_ranking;
    double min_uncertainty = teams[0].skill.skill_uncertainty;
    double max_uncertainty = teams[0].skill.skill_uncertainty;
    for (int t = 1; t < num_teams; t++)
    {
        if (teams[t].num_players < min_players)
        {
            min_players = teams[t].num_players;
        }
        if (teams[t].num_players > max_players)
        {
            max_players = teams[t].num_players;
        }
        if (teams[t].skill.skill_ranking < min_ranking)
        {
            min_ranking = teams[t].skill.skill_ranking;
        }
        if (teams[t].skill.skill_ranking > max_ranking)
        {
            max_ranking = teams[t].skill.skill_ranking;
        }
        if (teams[t].skill.skill_uncertainty < min_uncertainty)
        {
            min_uncertainty = teams[t].skill.skill_uncertainty;
        }
        if (teams[t].skill.skill_uncertainty > max_uncertainty)
        {
            max_uncertainty = teams[t].skill.skill_uncertainty;
        }
    }
    disparity.num_players = max_players - min_players;
    disparity.skill.skill_ranking = max_ranking - min_ranking;
    disparity.skill.skill_uncertainty = max_uncertainty - min_uncertainty;
}

// Port helper (inlined in rfg.exe)
static void team_info_add_group(team_info& team, const team_assignment_group& group)
{
    team.num_players += group.num_players;
    team.skill.skill_ranking += group.aggregate_skill.skill_ranking;
    team.skill.skill_uncertainty += group.aggregate_skill.skill_uncertainty;
}

// 0x00956030 (:591). LTCG: teams in EAX, the others on the stack.
static void calculate_team_disparity(team_info& disparity, int* group_team_assignments, const team_assignment_group* groups, const int num_groups, team_info* teams, const int num_teams)
{
    memset(teams, 0, num_teams * sizeof(team_info));
    for (int g = 0; g < num_groups; g++)
    {
        team_info_add_group(teams[group_team_assignments[g]], groups[g]);
    }
    team_info_spread(disparity, teams, num_teams);
}

// 0x009563C0 (:642). LTCG: best_disparity in EAX, ignore_trueskill on the stack. The ranking and the uncertainty are
// compared as floats, through doubles (rfg.exe: cvtpd2ps, cvtss2sd, subsd 0.0, abs).
static bool disparity_is_close_enough(const team_info& best_disparity, bool ignore_trueskill)
{
    if (best_disparity.num_players >= 2)
    {
        return false;
    }
    return ignore_trueskill || (float)fabs((double)(float)best_disparity.skill.skill_ranking - 0.0) < (float)best_disparity.skill.skill_uncertainty;
}

// 0x00956410 (:656): tries the first group on the first team, then on the others while that leaves a difference of
// more than one player. Original bug kept: the search over the other teams stops as soon as the best is not close
// enough (the check is the wrong way round), so with two teams it only runs once anyway.
static void assign_teams_by_whole_groups(team_info& best_disparity, int* group_team_assignments, const team_assignment_group* remaining_groups, const int num_remaining_groups, team_info* teams, const int num_teams)
{
    memset(&best_disparity, 0, sizeof(best_disparity));
    if (num_teams <= 0)
    {
        return;
    }
    if (num_remaining_groups < 1)
    {
        team_info_spread(best_disparity, teams, num_teams);
        return;
    }
    team_info start_teams[16];
    memcpy(start_teams, teams, num_teams * sizeof(team_info));
    team_info_add_group(teams[0], remaining_groups[0]);
    group_team_assignments[0] = 0;
    assign_teams_by_whole_groups(best_disparity, group_team_assignments + 1, remaining_groups + 1, num_remaining_groups - 1, teams, num_teams);
    if (best_disparity.num_players <= 1)
    {
        return;
    }
    int best_assignments[16];
    team_info best_teams[16];
    memcpy(best_assignments, group_team_assignments, num_remaining_groups * sizeof(int));
    memcpy(best_teams, teams, num_teams * sizeof(team_info));
    for (int t = 1; t < num_teams; t++)
    {
        memcpy(teams, start_teams, num_teams * sizeof(team_info));
        team_info_add_group(teams[t], remaining_groups[0]);
        group_team_assignments[0] = t;
        team_info disparity;
        assign_teams_by_whole_groups(disparity, group_team_assignments + 1, remaining_groups + 1, num_remaining_groups - 1, teams, num_teams);
        if (team_info_less_than(disparity, best_disparity))
        {
            best_disparity = disparity;
            memcpy(best_assignments, group_team_assignments, num_remaining_groups * sizeof(int));
            memcpy(best_teams, teams, num_teams * sizeof(team_info));
        }
        if (!disparity_is_close_enough(best_disparity, true))
        {
            break;
        }
    }
    memcpy(group_team_assignments, best_assignments, num_remaining_groups * sizeof(int));
    memcpy(teams, best_teams, num_teams * sizeof(team_info));
}

// 0x00956890 (:760)
static void calculate_group_aggregate_skill(team_assignment_group* group)
{
    skill_info skills[16];
    for (int i = 0; i < group->num_players; i++)
    {
        skills[i] = group->players[i].skill;
    }
    multi_core_platform_internal_get_aggregate_skill(group->aggregate_skill, skills, (unsigned int)group->num_players);
}

// 0x00999040 (:810): the second half of a group becomes a new group at the end. LTCG: groups in ECX, split_group_idx
// in EAX, the others on the stack.
static void split_group(team_assignment_group* groups, int* num_groups, const int max_groups, int split_group_idx)
{
    if (split_group_idx < 0 || split_group_idx >= *num_groups || *num_groups >= max_groups)
    {
        return;
    }
    team_assignment_group& group = groups[split_group_idx];
    team_assignment_group& new_group = groups[*num_groups];
    if (group.num_players <= 1)
    {
        return;
    }
    const int first_half = group.num_players / 2;
    new_group.leader_uid = group.players[first_half].player_uid;
    new_group.num_players = group.num_players - first_half;
    for (int i = 0; i < new_group.num_players; i++)
    {
        new_group.players[i] = group.players[first_half + i];
    }
    group.num_players = first_half;
    calculate_group_aggregate_skill(&group);
    calculate_group_aggregate_skill(&new_group);
    (*num_groups)++;
}

// Port helper (three group assignments in a row in rfg.exe)
static void team_assignment_group_swap(team_assignment_group& a, team_assignment_group& b)
{
    team_assignment_group temp;
    temp = a;
    a = b;
    b = temp;
}

// Port helper (inlined twice in rfg.exe): the group to split for a difference of disparity players: one of exactly
// the even part of that size, else the first biggest from first_group on.
static int find_group_to_split(const team_assignment_group* groups, const int first_group, const int num_groups, const int disparity)
{
    const int wanted = disparity - disparity % 2;
    for (int g = first_group; g < num_groups; g++)
    {
        if (groups[g].num_players == wanted)
        {
            return g;
        }
    }
    int best = first_group;
    for (int g = first_group + 1; g < num_groups; g++)
    {
        if (groups[g].num_players > groups[best].num_players)
        {
            best = g;
        }
    }
    return best;
}

// 0x009AF000 (:769): the parties among the used slots (players sharing a group uid), marauders left out, each player's
// level standing in for its skill; at most max_groups groups.
// LTCG: num_groups in ECX, slot_player_uids in EDX.
static void create_group_information(team_assignment_group* groups, int* num_groups, const int max_groups, const multi_player_uid* slot_player_uids, const multi_player_uid* slot_group_uids, const int num_used_slots)
{
    int count = 0;
    for (int slot = 0; slot < num_used_slots; slot++)
    {
        const multi_player_uid& player_uid = slot_player_uids[slot];
        const multi_player_uid& group_uid = slot_group_uids[slot];
        const multi_game_data* data = multi_game_data_get_by_player_uid(ST_GAME, player_uid);
        if (data != nullptr && data->team == HUMAN_TEAM_MARAUDER)
        {
            continue;
        }
        int g = 0;
        for (; g < count; g++)
        {
            if (groups[g].leader_uid.get_internal_id().is_equal(group_uid.get_internal_id()))
            {
                team_assignment_player_info& info = groups[g].players[groups[g].num_players];
                info.player_uid = player_uid;
                const multi_game_data* player_data = multi_game_data_get_by_player_uid(ST_GAME, player_uid);
                if (player_data != nullptr && player_data->team != HUMAN_TEAM_MARAUDER)
                {
                    info.skill.skill_ranking = (double)player_data->player_level;
                    info.skill.skill_uncertainty = 0.0;
                }
                groups[g].num_players++;
                break;
            }
        }
        if (g < count || count >= max_groups)
        {
            continue;
        }
        team_assignment_group& group = groups[count];
        group.leader_uid = group_uid;
        group.players[0].player_uid = player_uid;
        const multi_game_data* player_data = multi_game_data_get_by_player_uid(ST_GAME, player_uid);
        if (player_data != nullptr && player_data->team != HUMAN_TEAM_MARAUDER)
        {
            group.players[0].skill.skill_ranking = (double)player_data->player_level;
            group.players[0].skill.skill_uncertainty = 0.0;
        }
        group.num_players = 1;
        count++;
    }
    for (int g = 0; g < count; g++)
    {
        // Original: the group's leader becomes its first player (the uid bytes, not the real-uid flag).
        groups[g].leader_uid.get_internal_id() = groups[g].players[0].player_uid.get_internal_id();
        skill_info skills[16];
        for (int i = 0; i < groups[g].num_players; i++)
        {
            skills[i] = groups[g].players[i].skill;
        }
        multi_core_platform_internal_get_aggregate_skill(groups[g].aggregate_skill, skills, (unsigned int)groups[g].num_players);
    }
    *num_groups = count;
}

// 0x009AF560 (:965): the lobby's parties are read again and the balance search starts over
static void reset_team_assignment()
{
    int num_used_slots = 0;
    multi_player_uid slot_player_uids[16];
    multi_player_uid slot_group_uids[16];
    memset(slot_player_uids, 0, sizeof(slot_player_uids));
    memset(slot_group_uids, 0, sizeof(slot_group_uids));
    Lobby_session->get_used_slot_information(&num_used_slots, slot_player_uids, slot_group_uids, 16);
    Num_groups = 0;
    memset(Groups, 0, sizeof(Groups));
    create_group_information(Groups, &Num_groups, 16, slot_player_uids, slot_group_uids, num_used_slots);
    Split_groups = 0;
    Team_combinations_attempted = 0;
    memset(&Team_disparity, 0, sizeof(Team_disparity));
}

// 0x009AF280 (:839): the best split of the lobby's parties by whole groups, and optionally with the group that fits the
// difference divided between the teams; team_offset is the number of players the first team starts with.
// Original bug kept: after the swaps the two halves are the first and the last group, but the second half's size is
// read from the second group and the search goes on without the last group's assignment being the second half.
static void find_best_team_assignments(team_info& best_disparity, bool split_teams_to_minimize_disparity, int team_offset)
{
    int num_used_slots = 0;
    multi_player_uid slot_player_uids[16];
    multi_player_uid slot_group_uids[16];
    memset(slot_player_uids, 0, sizeof(slot_player_uids));
    memset(slot_group_uids, 0, sizeof(slot_group_uids));
    Lobby_session->get_used_slot_information(&num_used_slots, slot_player_uids, slot_group_uids, 16);
    team_assignment_group groups[16];
    int num_groups = 0;
    memset(groups, 0, sizeof(groups));
    create_group_information(groups, &num_groups, 16, slot_player_uids, slot_group_uids, num_used_slots);
    int assignments[16];
    team_info teams[2];
    memset(assignments, 0, sizeof(assignments));
    memset(teams, 0, sizeof(teams));
    teams[0].num_players = team_offset;
    assign_teams_by_whole_groups(best_disparity, assignments, groups, num_groups, teams, 2);
    if (best_disparity.num_players <= 1 || !split_teams_to_minimize_disparity)
    {
        return;
    }
    memset(teams, 0, sizeof(teams));
    assignments[0] = teams[1].num_players <= team_offset ? 1 : 0;
    teams[0].num_players = team_offset;
    const int split = find_group_to_split(groups, 0, num_groups, best_disparity.num_players);
    split_group(groups, &num_groups, 16, split);
    team_assignment_group_swap(groups[0], groups[split]);
    team_assignment_group_swap(groups[0], groups[num_groups - 1]);
    assignments[1] = (assignments[0] + 1) % 2;
    teams[assignments[0]].num_players += groups[0].num_players;
    teams[assignments[1]].num_players += groups[1].num_players;
    assign_teams_by_whole_groups(best_disparity, assignments + 2, groups + 2, num_groups - 2, teams, 2);
}

// 0x00999120 (:1002): goes on with the search over all group combinations (a combination's bits are the groups' teams),
// num_combinations more of them; with all tried, the best is applied to the players' game data. LTCG: num_combinations
// in ECX, allow_split on the stack.
// Original bug kept: the group split below is never reached: the loop leaves the count at the limit that the check
// compares it with (and the only caller passes no combinations and no split). The split's swaps are as in
// find_best_team_assignments.
static void search_team_combinations(int num_combinations, bool allow_split)
{
    int assignments[17];
    team_info teams[2];
    for (;;)
    {
        int total = 1;
        for (int g = 0; g < Num_groups; g++)
        {
            total *= 2;
        }
        const int limit = Team_combinations_attempted + num_combinations < total ? Team_combinations_attempted + num_combinations : total;
        const int remaining = Team_combinations_attempted - limit + num_combinations;
        while (Team_combinations_attempted < limit)
        {
            int combination = Team_combinations_attempted;
            for (int g = 0; g < Num_groups; g++)
            {
                const int bit = combination % 2;
                assignments[g] = bit;
                combination = (combination - bit) / 2;
            }
            team_info disparity;
            calculate_team_disparity(disparity, assignments, Groups, Num_groups, teams, 2);
            if (Team_combinations_attempted == 0 || team_info_less_than(disparity, Team_disparity))
            {
                memcpy(Group_team_assignments, assignments, Num_groups * sizeof(int));
                Teams[0] = teams[0];
                Teams[1] = teams[1];
                Team_disparity = disparity;
            }
            Team_combinations_attempted++;
        }
        if (!allow_split || Team_combinations_attempted >= limit || remaining < 1 || disparity_is_close_enough(Team_disparity, false))
        {
            if (Team_combinations_attempted >= total)
            {
                for (int g = 0; g < Num_groups; g++)
                {
                    for (int p = 0; p < Groups[g].num_players; p++)
                    {
                        const multi_game_data* data = multi_game_data_get_by_player_uid(ST_GAME, Groups[g].players[p].player_uid);
                        const human_teams team = (human_teams)Group_team_assignments[g];
                        if (data != nullptr && data->team != team)
                        {
                            const unsigned int handle = data->handle;
                            if (handle != 0xFFFFFFFF)
                            {
                                game_data_of(handle)->team = team;
                                if (Local_game_data_handles[handle >> 16] == handle)
                                {
                                    Spectating = team == HUMAN_TEAM_MARAUDER;
                                }
                            }
                            Send_lobby_state_update = true;
                        }
                    }
                }
            }
            return;
        }
        memset(teams, 0, sizeof(teams));
        for (int g = 0; g < Split_groups; g++)
        {
            teams[assignments[g]].num_players += Groups[g].num_players;
        }
        const int first_team = teams[0].num_players >= teams[1].num_players ? 1 : 0;
        const int split = find_group_to_split(Groups, Split_groups, Num_groups, Team_disparity.num_players);
        if (split >= Num_groups)
        {
            return;
        }
        split_group(Groups, &Num_groups, 16, split);
        team_assignment_group_swap(Groups[Split_groups], Groups[split]);
        team_assignment_group_swap(Groups[Split_groups], Groups[Num_groups - 1]);
        const int second_team = (first_team + 1) % 2;
        assignments[Split_groups] = first_team;
        assignments[Split_groups + 1] = second_team;
        teams[first_team].num_players += Groups[Split_groups].num_players;
        teams[second_team].num_players += Groups[Split_groups + 1].num_players;
        Split_groups += 2;
        Team_combinations_attempted = 0;
        num_combinations = remaining;
    }
}

// 0x009C5F20 (:247): console command (unregistered in retail); the same as reset_team_assignment
void unused_dcf_mp_lobby_team_assignments_reset()
{
    reset_team_assignment();
}

// 0x00955F20 (:328): the player owns the entry's map and mode
// LTCG: entry in EDX, data in ESI.
static bool multi_game_has_req_dlc(const multi_game_playlist_entry* entry, const multi_game_data* data)
{
    if (entry->level->is_downloadable_content && (data->dlc_mask & (1u << (entry->level->dlc_bundle_id & 0x1F))) == 0)
    {
        return false;
    }
    return (entry->mode != MGPM_BAGMAN && entry->mode != MGPM_TEAM_BAGMAN) || data->bagman_dlc_unlocked;
}

// 0x009C5F30 (:346): a ready-up change, from the host to all or from a client to the host
// LTCG: player_uid in EDI.
static void multi_game_lobby_send_ready_packet(const multi_player_uid& player_uid, bool player_is_ready)
{
    unsigned char packet[1200];
    packet[0] = PT_READY_FOR_GAME_START;
    unsigned short packet_size = packet_header_size(PT_READY_FOR_GAME_START);
    unsigned char flags = 0;
    if (player_uid.get_internal_id().is_equal(My_player_uid.get_internal_id()))
    {
        flags = Ready_flag_sender_is_player;
    }
    if (player_is_ready)
    {
        flags |= Ready_flag_ready;
    }
    packet[packet_size++] = flags;
    if ((flags & Ready_flag_sender_is_player) == 0)
    {
        player_uid.write_to_packet(packet, packet_size);
    }
    packet_set_size(packet, packet_size);
    if (Lobby_session->am_i_host())
    {
        multi_io_send_to_all_reliable(Lobby_session, packet, packet_size);
        return;
    }
    multi_io_send_reliable(Lobby_session->get_host_connection(), packet, packet_size, nullptr, nullptr, nullptr);
}

// 0x009AEF20 (:374): host of a system link or private lobby: a level, a mode the players own, and backpacks that exist
static void multi_game_lobby_validate_mp_server_options()
{
    if (Lobby_session == nullptr || !Lobby_session->am_i_host())
    {
        return;
    }
    const multi_session_info_base* info = Lobby_session->get_info();
    if (!info->systemlink_or_lan && !info->private_match)
    {
        return;
    }
    if (Mp_server_options.get_level() == nullptr)
    {
        Mp_server_options.set_level(Mp_server_options.next_level());
    }
    if ((Mp_server_options.get_mode() == MGPM_BAGMAN || Mp_server_options.get_mode() == MGPM_TEAM_BAGMAN) && !Bagman_dlc_unlocked)
    {
        Mp_server_options.set_mode(MGPM_DEATHMATCH);
    }
    multi_game_generic_options& options = Mp_server_options.generic_options;
    if (options.starting_backpack_type != -1 && ((unsigned int)options.starting_backpack_type > 15 || Backpack_info[options.starting_backpack_type] == nullptr))
    {
        options.starting_backpack_type = -1;
    }
    if (options.backpack_spawns == MGBSO_SPECIFIC && options.backpack_spawn_type != -1 && multi_game_backpack_get_info(options.backpack_spawn_type) == nullptr)
    {
        options.backpack_spawn_type = -1;
    }
    if (Mp_server_options.get_mode() == MGPM_SIEGE && Mp_server_options.mode_options.siege.defense_starting_backpack_type != -1 &&
        multi_game_backpack_get_info(Mp_server_options.mode_options.siege.defense_starting_backpack_type) == nullptr)
    {
        Mp_server_options.mode_options.siege.defense_starting_backpack_type = -1;
    }
}

// 0x0097B950 (:414): the player owns the server options' map, mode and backpacks
// LTCG: cp in EAX.
static client_start_test_results client_start_test(const multi_connection* cp)
{
    const multi_game_data* data = multi_game_data_get(cp->game_data, false, false);
    if (data == nullptr)
    {
        return CST_INVALID;
    }
    const multi_level_info* level = Mp_server_options.get_level();
    if (level->is_downloadable_content && (data->dlc_mask & (1u << (level->dlc_bundle_id & 0x1F))) == 0)
    {
        return CST_NO_LEVEL;
    }
    if ((Mp_server_options.get_mode() == MGPM_BAGMAN || Mp_server_options.get_mode() == MGPM_TEAM_BAGMAN) && !data->bagman_dlc_unlocked)
    {
        return CST_NO_MODE;
    }
    const multi_game_generic_options& options = Mp_server_options.generic_options;
    if (options.starting_backpack_type != -1 && ((1u << (options.starting_backpack_type & 0x1F)) & data->backpack_mask) == 0)
    {
        return CST_NO_STARTING_BACKPACK;
    }
    if (options.backpack_spawns == MGBSO_SPECIFIC && ((1u << (options.backpack_spawn_type & 0x1F)) & data->backpack_mask) == 0)
    {
        return CST_NO_DEFAULT_BACKPACK;
    }
    return CST_OK;
}

// 0x009DEF20 (:450): the host with the best measured upstream (system link: this host), else this host
static void multi_game_lobby_choose_best_host()
{
    if (Lobby_session == nullptr || Lobby_session->get_state() != MSS_READY)
    {
        return;
    }
    multi_game_send_full_bw_results(Lobby_session);
    if (!Multi_syslink)
    {
        int best_upstream = 0;
        multi_connection* first = Lobby_session->get_connection_list();
        for (multi_connection* cp = first; cp != nullptr;)
        {
            int upstream;
            int downstream;
            int unused;
            if (multi_connection_is_connected(cp) && cp->cannot_serve == 0 && multi_connection_is_connected(cp) &&
                tracked_bandwidth_info_get_info(cp->player_uid, upstream, downstream, unused) && best_upstream < upstream)
            {
                // Original: the real-uid flag of the previous choice is kept.
                Lobby_best_host_player_uid.get_internal_id() = cp->player_uid.get_internal_id();
                best_upstream = upstream;
            }
            cp = cp->next;
            if (cp == first)
            {
                break;
            }
        }
    }
    else
    {
        Lobby_best_host_player_uid = My_player_uid;
    }
    if (!Lobby_best_host_player_uid.get_internal_id().isValid)
    {
        Lobby_best_host_player_uid.get_internal_id() = Lobby_session->get_me_connection()->player_uid.get_internal_id();
    }
}

// 0x009C6020 (:523): the host names the best host to all
static void multi_game_lobby_send_best_host()
{
    if (!Lobby_best_host_player_uid.get_internal_id().isValid)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_LOBBY_CHOOSE_BEST_HOST;
    unsigned short packet_size = packet_header_size(PT_LOBBY_CHOOSE_BEST_HOST);
    packet[packet_size++] = Best_host_id;
    Lobby_best_host_player_uid.write_to_packet(packet, packet_size);
    packet_set_size(packet, packet_size);
    multi_io_send_to_all_reliable(Lobby_session, packet, packet_size);
}

// 0x009C60C0 (:535): a client acknowledges the best host selection state
static void multi_game_lobby_send_best_host_ack()
{
    unsigned char packet[1200];
    packet[0] = PT_LOBBY_CHOOSE_BEST_HOST;
    unsigned short packet_size = packet_header_size(PT_LOBBY_CHOOSE_BEST_HOST);
    packet[packet_size++] = Best_host_state_ack;
    packet_set_size(packet, packet_size);
    multi_io_send_reliable(Lobby_session->get_host_connection(), packet, packet_size, nullptr, nullptr, nullptr);
}

// 0x00955F60 (:544)
static multi_connection* multi_game_lobby_get_best_host_connection()
{
    multi_connection* first = Lobby_session->get_connection_list();
    for (multi_connection* cp = first; cp != nullptr;)
    {
        if (cp->player_uid.get_internal_id().isValid && cp->player_uid.get_internal_id().is_equal(Lobby_best_host_player_uid.get_internal_id()))
        {
            return cp;
        }
        cp = cp->next;
        if (cp == first)
        {
            break;
        }
    }
    return nullptr;
}

// 0x00955FB0 (:558): -1 clears the timeout
// LTCG: time in EAX.
static void set_lobby_timeout(int time)
{
    if (time == -1)
    {
        Lobby_timeout.invalidate();
    }
    else
    {
        Lobby_timeout.set(time);
    }
    Lobby_timeout_set_time = Clock_current_time;
    Send_lobby_state_update = true;
}

// 0x0097BB20 (:1149): the acked players as a bit per connection index (BE)
static void append_ready_ups_to_packet(unsigned char* packet_data, unsigned short& packet_size)
{
    unsigned short bits = 0;
    for (int i = 0; i < Players_acked_for_start.size(); i++)
    {
        multi_connection* first = Lobby_session->get_connection_list();
        for (multi_connection* cp = first; cp != nullptr;)
        {
            if (cp->player_uid.get_internal_id().is_equal(Players_acked_for_start[i].get_internal_id()))
            {
                bits |= (unsigned short)(1 << (cp->index_in_session & 0x1F));
                break;
            }
            cp = cp->next;
            if (cp == first)
            {
                break;
            }
        }
    }
    write_u16_be(packet_data, packet_size, bits);
}

// 0x00999650 (:1163)
// LTCG: packet_data in EAX, packet_offset in ECX.
static void read_ready_ups_from_packet(unsigned char* packet_data, int& packet_offset)
{
    const unsigned short bits = read_u16_be(packet_data, packet_offset);
    Players_acked_for_start.clear();
    for (int index = 0; index < 16; index++)
    {
        if ((bits & (1 << index)) == 0)
        {
            continue;
        }
        multi_connection* first = Lobby_session->get_connection_list();
        for (multi_connection* cp = first; cp != nullptr;)
        {
            if (cp->index_in_session == index)
            {
                Players_acked_for_start.add(cp->player_uid);
                break;
            }
            cp = cp->next;
            if (cp == first)
            {
                break;
            }
        }
    }
}

// 0x009C6150 (:1181): the host's lobby state to every client: state, timeout, match and veto entries, vetoes, server
// options, teams, desired weapons and ready-ups
static void update_lobby_states()
{
    if (Lobby_state == MGLSM_HOST)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_UPDATE_LOBBY_STATE;
    unsigned short packet_size = packet_header_size(PT_UPDATE_LOBBY_STATE);
    packet[packet_size++] = (unsigned char)Lobby_state;
    write_u32_be(packet, packet_size, Lobby_timeout_set_time);
    write_u32_be(packet, packet_size, (unsigned int)(Lobby_timeout.valid() ? Lobby_timeout.time_until() : -1));
    write_u32_be(packet, packet_size, Auto_launch_match_entry == nullptr ? 0 : Auto_launch_match_entry->entry_id.get_crc());
    write_u32_be(packet, packet_size, Auto_launch_veto_entry == nullptr ? 0 : Auto_launch_veto_entry->entry_id.get_crc());
    write_u32_be(packet, packet_size, (unsigned int)Veto_count);
    packet[packet_size++] = Vetoes_allowed;
    multi_game_append_server_options_to_packet(packet, packet_size, &Mp_server_options);
    multi_game_player_append_teams_to_packet(packet, packet_size, Lobby_session->get_info()->type);
    multi_game_player_append_desired_weapons_to_packet(packet, packet_size, Lobby_session);
    append_ready_ups_to_packet(packet, packet_size);
    packet_set_size(packet, packet_size);
    multi_connection* first = Lobby_session->get_connection_list();
    for (multi_connection* cp = first; cp != nullptr;)
    {
        if (cp != Lobby_session->get_me_connection())
        {
            multi_io_send_reliable(cp, packet, packet_size, nullptr, nullptr, nullptr);
        }
        cp = cp->next;
        if (cp == first)
        {
            break;
        }
    }
}

// 0x00956910 (:1215): each state's timeout, less the time since the state change
// LTCG: state in EAX, delta_time in ECX.
static void set_timeout_for_state(multi_game_lobby_state_machine state, int delta_time)
{
    switch (state)
    {
    case MGLSM_NONE:
    case MGLSM_GAME_OVER:
        set_lobby_timeout(-1);
        return;
    case MGLSM_WAITING_FOR_MIN_PLAYERS:
        set_lobby_timeout(Min_players_timeout_ms - delta_time);
        return;
    case MGLSM_GAME_COUNTDOWN:
    case MGLSM_VETOED:
        set_lobby_timeout(Countdown_timeout_ms - delta_time);
        return;
    case MGLSM_SELECTING_BEST_HOST:
    case MGLSM_COMMUNICATING_BEST_HOST:
    case MGLSM_MIGRATING_BEST_HOST:
        set_lobby_timeout(Best_host_timeout_ms - delta_time);
        return;
    default:
        return;
    }
}

// 0x0097BBD0 (:1255): the game session's join test: no joins without a destruction sync or during a migration, within
// the playlist's size, and only during gameplay with more than 2 minutes of the round left
static multi_session_join_status multi_game_player_may_join(multi_connection* cp, int join_group_size)
{
    if (!jip_destruction_sync_possible || Migration_state != MIGRATION_STATE_NONE)
    {
        return MSJS_JOIN_DENY_SESSION_LOCKED;
    }
    const multi_game_playlist* playlist = find_playlist(Current_user_playlist);
    if (playlist != nullptr)
    {
        if (playlist->max_players <= Lobby_session->get_slots_used())
        {
            return MSJS_JOIN_DENY_SESSION_FULL;
        }
    }
    else if (!Lobby_session->get_info()->party_lobby)
    {
        return MSJS_JOIN_DENY_SESSION_LOCKED;
    }
    if (Multi_match_info.match_state == MGMS_GAMEPLAY)
    {
        // Accepted while more than 119,999 ms of the round are left.
        return Timer_time.time_until() > 119999 ? MSJS_JOIN_ACCEPT : MSJS_JOIN_DENY_SESSION_LOCKED;
    }
    return MSJS_JOIN_DENY_SESSION_LOCKED;
}

// 0x009FE5E0 (:1296): the lobby state machine's transitions; a state's entry may move on to the next one at once
// LTCG: state in EAX, as_of_multi_time in ECX.
static void multi_game_lobby_set_state(multi_game_lobby_state_machine state, unsigned int as_of_multi_time)
{
    for (;;)
    {
        // Selecting the best host may be entered again (a new selection).
        if (state == Lobby_state && state != MGLSM_SELECTING_BEST_HOST)
        {
            return;
        }
        Lobby_last_state = Lobby_state;
        Lobby_state = state;
        multi_session* sp = Lobby_session;
        if (sp == nullptr || sp->get_state() == MSS_NONE || sp->am_i_host())
        {
            set_timeout_for_state(state, (int)(Clock_current_time - as_of_multi_time));
            sp = Lobby_session;
        }
        // Original: Lobby_session is not checked for null here.
        const bool host = sp->am_i_host();
        if (host)
        {
            Send_lobby_state_update = Lobby_state != MGLSM_NONE;
        }
        switch (Lobby_state)
        {
        case MGLSM_NONE:
            if (gameseq_get_state() == GS_MULTIPLAYER_XP_TALLY || gameseq_get_state() == GS_MULTIPLAYER_RESULTS)
            {
                return;
            }
            multi_game_stats_match_init();
            return;
        case MGLSM_CLIENT:
            if (Multi_join_in_progress)
            {
                sp->session_start_gameplay();
            }
            return;
        case MGLSM_HOST:
            Max_players_observed = sp->get_slots_used();
            if (!host || Pushed_to_game)
            {
                return;
            }
            if (sp->get_info()->private_match || sp->get_info()->systemlink_or_lan)
            {
                multi_game_lobby_validate_mp_server_options();
                return;
            }
            state = MGLSM_WAITING_FOR_MIN_PLAYERS;
            continue;
        case MGLSM_WAITING_FOR_MIN_PLAYERS:
            multi_game_lobby_set_actual_game_mode_map_etc();
            return;
        case MGLSM_WAITING_FOR_MARGINALLY_FAIR_TEAMS:
            Players_acked_for_start.clear();
            I_am_ready = false;
            Vetoes_allowed = false;
            Vetoed_map = false;
            Veto_count = 0;
            return;
        case MGLSM_WAITING_FOR_PLAYERS:
            Vetoed_map = false;
            Veto_count = 0;
            Vetoes_allowed = true;
            return;
        case MGLSM_WAITING_FOR_BALANCED_TEAMS:
            if (!host || sp->get_slots_reserved() > 0 || Team_disparity.num_players != 0 || Assume_bad_teams)
            {
                return;
            }
            state = MGLSM_GAME_COUNTDOWN;
            continue;
        case MGLSM_VETOED:
            // The veto entry replaces the match entry.
            if (Auto_launch_match_entry != nullptr)
            {
                Vetoes_allowed = false;
            }
            if (sp->am_i_host())
            {
                Auto_launch_match_entry = Auto_launch_veto_entry;
                Auto_launch_veto_entry = nullptr;
                apply_playlist_entry(Auto_launch_match_entry);
            }
            multi_player_menu_system_close_leave_confirm_box();
            return;
        case MGLSM_GAME_COUNTDOWN:
            multi_player_menu_system_close_leave_confirm_box();
            return;
        case MGLSM_SELECTING_BEST_HOST:
            Lobby_best_host_player_uid.invalidate();
            Vetoes_allowed = false;
            Lobby_best_host_id_acked = false;
            Lobby_best_host_state_acked = false;
            if (!sp->am_i_host())
            {
                multi_game_lobby_send_best_host_ack();
                return;
            }
            multi_game_lobby_choose_best_host();
            if (Lobby_best_host_player_uid.get_internal_id().is_equal(My_player_uid.get_internal_id()))
            {
                Lobby_best_host_state_acked = true;
            }
            return;
        case MGLSM_COMMUNICATING_BEST_HOST:
            if (!sp->am_i_host())
            {
                return;
            }
            if (Lobby_best_host_player_uid.get_internal_id().is_equal(My_player_uid.get_internal_id()))
            {
                Lobby_best_host_id_acked = true;
                return;
            }
            multi_game_lobby_send_best_host();
            return;
        case MGLSM_MIGRATING_BEST_HOST:
        {
            if (!sp->am_i_host())
            {
                return;
            }
            update_lobby_states();
            Send_lobby_state_update = false;
            multi_connection* new_host = multi_game_lobby_get_best_host_connection();
            if (new_host != nullptr)
            {
                Lobby_session->session_migrate(new_host, SMR_HOST_REQUESTED);
            }
            return;
        }
        case MGLSM_STARTING_GAME:
            if (sp->am_i_host() && sp->get_state() == MSS_READY)
            {
                sp->session_start_gameplay();
            }
            return;
        case MGLSM_IN_GAME:
            // The lobby session becomes the game session.
            Game_session = sp;
            sp->register_migration_callbacks(multi_game_migration_allow_migration, multi_game_migration_about_to_start, multi_game_migration_just_occurred, multi_game_migration_failed);
            Game_session->register_join_callbacks(multi_game_player_may_join, nullptr, multi_game_about_to_remove_connection, multi_game_player_left);
            multi_game_bw_stop();
            if (Lobby_session->am_i_host())
            {
                multi_game_lobby_server_start_game();
            }
            return;
        case MGLSM_GAME_OVER:
            if (sp->get_state() == MSS_IN_GAME && host)
            {
                sp->session_end_gameplay();
            }
            Players_acked_for_start.clear();
            if (Current_game_mode_context != PGMC_MULTIPLAYER)
            {
                Current_game_mode_context = PGMC_MULTIPLAYER;
            }
            return;
        default:
            return;
        }
    }
}

// 0x0097BC60 (:1473): the lobby's voice goes to everyone
static bool multi_game_lobby_voice_should_send(multi_connection* cp)
{
    return cp != nullptr;
}

// 0x009AF640 (:1500): the lobby's join test (host): only from the lobby menus and not once the match is starting; the
// public matchmaking limits on marauders, DLC and team sizes
static multi_session_join_status multi_game_lobby_player_may_join(multi_connection* cp, int join_group_size)
{
    if (!Lobby_session->am_i_host() || Lobby_session->get_info()->party_lobby)
    {
        return MSJS_JOIN_ACCEPT;
    }
    switch (gameseq_get_state())
    {
    case GS_INGAME_OPTIONS:
    case GS_MULTIPLAYER_CHANGE_MATCHMAKING:
    case GS_MULTIPLAYER_SEARCH_MATCHMAKING:
    case GS_MULTIPLAYER_GAME_LOBBY:
    case GS_MULTIPLAYER_SYSLINK:
    case GS_MULTIPLAYER_SYSLINK_FIND_SERVERS:
    case GS_MULTIPLAYER_CHANGE_GAME_OPTIONS:
    case GS_MULTIPLAYER_LIVE_FIND_SERVERS:
        break;
    default:
        return MSJS_JOIN_DENY_SESSION_LOCKED;
    }
    if (Lobby_state > MGLSM_WAITING_FOR_BALANCED_TEAMS && Lobby_state < MGLSM_GAME_OVER)
    {
        return MSJS_JOIN_DENY_SESSION_LOCKED;
    }
    const int num_marauders = multi_game_mode_get_team_size(ST_GAME, HUMAN_TEAM_MARAUDER, 0xFFFFFFFFu);
    const multi_game_data* data = multi_game_data_get(cp->game_data, false, false);
    const multi_session_info_base* info = Lobby_session->get_info();
    // 4 marauders (spectators) at most in a public lobby.
    if (data != nullptr && data->team == HUMAN_TEAM_MARAUDER && !info->private_match && !info->systemlink_or_lan && num_marauders > 4)
    {
        return MSJS_JOIN_DENY_SESSION_FULL;
    }
    const int slots = Lobby_session->get_slots_reserved() + Lobby_session->get_slots_used();
    const int num_players = slots - num_marauders;
    if (info->systemlink_or_lan)
    {
        return MSJS_JOIN_ACCEPT;
    }
    // Original: the match and veto entries are tested against a missing game data too.
    if ((Auto_launch_match_entry != nullptr && !multi_game_has_req_dlc(Auto_launch_match_entry, data)) ||
        (Auto_launch_veto_entry != nullptr && !multi_game_has_req_dlc(Auto_launch_veto_entry, data)))
    {
        return MSJS_JOIN_DENY_DLC_MISSING;
    }
    const multi_game_playlist* playlist = multi_game_playlist_get_by_id(Current_user_playlist);
    if (playlist == nullptr)
    {
        return MSJS_JOIN_ACCEPT;
    }
    int max_marauders = 16 - playlist->min_players;
    if (max_marauders > 4)
    {
        max_marauders = 4;
    }
    if ((!info->private_match && max_marauders < num_marauders) || playlist->max_players < num_players)
    {
        return MSJS_JOIN_DENY_SESSION_FULL;
    }
    if (Auto_launch_match_entry == nullptr || multi_gameplay_mode_is_team_mode(Auto_launch_match_entry->mode))
    {
        team_info disparity;
        find_best_team_assignments(disparity, false, 0);
        if (playlist->max_players - num_players < disparity.num_players)
        {
            return MSJS_JOIN_DENY_SESSION_FULL;
        }
        if (disparity.num_players - Lobby_session->get_info()->max_slots + slots > 0)
        {
            // Original: the joining player's game data is not checked for null.
            if (data->team == HUMAN_TEAM_MARAUDER)
            {
                return MSJS_JOIN_DENY_SESSION_FULL;
            }
            // The marauders make room.
            multi_connection* first = Lobby_session->get_connection_list();
            for (multi_connection* other = first; other != nullptr;)
            {
                if (multi_game_data_get(other->game_data, false, false)->team == HUMAN_TEAM_MARAUDER)
                {
                    multi_connection_disconnect(other, CDR_NONE);
                }
                other = other->next;
                if (other == first)
                {
                    break;
                }
            }
        }
    }
    return MSJS_JOIN_ACCEPT;
}

// 0x009AF910 (:1624): the host registers itself when its own connection joins; a new player gets a team when the
// lobby allows team changes, and a late joiner restarts the waiting timeout
static void multi_game_lobby_player_joined(multi_connection* cp)
{
    reset_team_assignment();
    if (cp == Lobby_session->get_host_connection())
    {
        if (Lobby_session->am_i_host())
        {
            const multi_session_info_base* info = Lobby_session->get_info();
            if (!info->dedicated)
            {
                cp->game_data = multi_game_data_register_server_player(info->type, cp->player_uid);
                if (cp->game_data != 0xFFFFFFFFu)
                {
                    Local_game_data_handles[cp->game_data >> 16] = cp->game_data;
                }
            }
            multi_game_preferences_reset_total_history();
            multi_game_preferences_apply_my_history(cp->index_in_session);
        }
        Game_voice_cb = multi_game_lobby_voice_should_send;
        return;
    }
    if (!Lobby_session->am_i_host())
    {
        return;
    }
    const multi_game_data* data = multi_game_data_get(cp->game_data, false, false);
    if (data == nullptr)
    {
        return;
    }
    if (data->team != HUMAN_TEAM_MARAUDER)
    {
        const int used = Lobby_session->get_slots_used();
        int observed = Max_players_observed;
        if (Max_players_observed < used)
        {
            observed = used;
            if (Lobby_state > MGLSM_HOST && Lobby_state < MGLSM_WAITING_FOR_BALANCED_TEAMS && Lobby_timeout.valid())
            {
                Lobby_timeout.set(Late_joiner_timeout_ms);
                Lobby_timeout_set_time = Clock_current_time;
                Send_lobby_state_update = true;
            }
        }
        Max_players_observed = observed;
        if (multi_game_lobby_allow_team_changing())
        {
            if (multi_gameplay_mode_is_team_mode(Mp_server_options.get_mode()))
            {
                human_teams team = multi_game_mode_get_disadvantaged_team(ST_GAME, 0xFFFFFFFFu);
                if (team == HUMAN_TEAM_NONE)
                {
                    team = HUMAN_TEAM_EDF;
                }
                multi_game_data_set_player_team(cp->game_data, team);
                Send_lobby_state_update = true;
                return;
            }
            multi_game_data_set_player_team(cp->game_data, HUMAN_TEAM_CIVILIAN);
        }
    }
    Send_lobby_state_update = true;
}

// 0x009AFAA0 (:1686): the player's data lingers 1.5 s with the disconnect reason
static void multi_game_lobby_player_left(multi_connection* cp)
{
    reset_team_assignment();
    if (cp->game_data != 0xFFFFFFFFu)
    {
        multi_game_data* data = game_data_of(cp->game_data);
        data->disconnect_reason = cp->disconnect_reason;
        data->linger_time.set(1500);
        data->in_use = false;
        if (cp->game_data == Local_game_data_handles[cp->game_data >> 16])
        {
            Local_game_data_handles[cp->game_data >> 16] = 0xFFFFFFFFu;
        }
        cp->game_data = 0xFFFFFFFFu;
    }
    multi_game_preferences_reset_player_history(cp->index_in_session);
}

// 0x009569B0 (:1715): no lobby migration while the game lobby is up under a game, nor while spectating
static bool multi_game_lobby_migration_allowed(multi_session_base* sp)
{
    if (Game_session != nullptr && gameseq_get_state() == GS_MULTIPLAYER_GAME_LOBBY)
    {
        return false;
    }
    return !Spectating;
}

// 0x009FE9B0 (:1732): a new host takes the lobby over unless the migration was the best-host handshake
static void multi_game_lobby_migration_succeeded(multi_session_base* sp)
{
    if (Lobby_session->am_i_host() && Lobby_state != MGLSM_SELECTING_BEST_HOST && Lobby_state != MGLSM_COMMUNICATING_BEST_HOST &&
        Lobby_state != MGLSM_MIGRATING_BEST_HOST && Lobby_state != MGLSM_HOST)
    {
        multi_game_lobby_set_state(MGLSM_HOST, Clock_current_time);
    }
}

// 0x009FD430 (:1741): outside the game and its menus (or in a system link game) the player goes back to the main menu;
// the lobby session is ended and deleted
static void multi_game_lobby_migration_failed(multi_session_base* sp)
{
    if ((Game_session == nullptr || Game_session->get_state() == MSS_NONE) && !sp->get_info()->party_lobby)
    {
        const game_state gs = gameseq_get_state();
        if ((gs != GS_MAINMENU && gs != GS_MULTIPLAYER_XP_TALLY && gs != GS_MULTIPLAYER_RESULTS && gs != GS_MULTIPLAYER_SEARCH_MATCHMAKING &&
             gs != GS_MULTIPLAYER_GAME_LOBBY) ||
            Multi_syslink)
        {
            multi_player_menu_system_close_message_box();
            game_boot_to_main_menu(BC_LOBBY_MIGRATION_FAIL);
        }
    }
    if (Lobby_session->get_state() == MSS_IN_GAME)
    {
        Lobby_session->session_end_gameplay();
        while (Lobby_session->get_state() == MSS_ENDING)
        {
            Lobby_session->process();
        }
    }
    if (Lobby_session->get_state() != MSS_NONE)
    {
        // multi_session_base::session_delete, inlined
        Lobby_session->session_delete();
        while (Lobby_session->get_state() == MSS_DELETING)
        {
            Lobby_session->process();
        }
    }
}

// 0x009FED70 (:1779): after a match: the contexts reset once the lobby is ready again, then the lobby state clears
static void process_ending_game()
{
    if (Endgamestate == EGSM_STOP_GAME)
    {
        if (Lobby_session->get_state() == MSS_READY)
        {
            Current_game_mode_context = PGMC_MULTIPLAYER;
            Current_game_type_context = PGTC_PUBLIC;
            Current_user_playlist = 0;
            Current_user_dlc_mask = 0;
            Endgamestate = EGSM_RESET_CONTEXTS_AND_PROPERTIES;
        }
        return;
    }
    if (Endgamestate == EGSM_RESET_CONTEXTS_AND_PROPERTIES)
    {
        Endgamestate = EGSM_ENDED;
        return;
    }
    if (Endgamestate == EGSM_ENDED && !Pushed_to_game && Lobby_state != MGLSM_NONE)
    {
        multi_game_lobby_set_state(MGLSM_NONE, Clock_current_time);
    }
}

// 0x009569E0 (:1811)
static void mp_close_all_message_boxes_lobby()
{
    // -2: every message box
    if (ui_is_msgbox_active(-2))
    {
        ui_hide_msgbox(-2, true);
    }
}

// 0x009C6440 (:1824): the lobby session object (one, from the session pool)
void multi_game_lobby_init()
{
    // multi_session_allocate, inlined
    multi_session* sp = Session_pool.alloc(0);
    if (sp != nullptr)
    {
        sp->init(true);
    }
    Lobby_session = sp;
    Pending_session_info.init();
    Pending_session_coordinator_id.init();
    Lobby_state = MGLSM_NONE;
    Pending_session_exists = false;
    Left_lobby_by_choice = false;
    Pushed_to_game = false;
    Lobby_best_host_player_uid.invalidate();
    Lobby_best_host_id_acked = false;
    Lobby_best_host_state_acked = false;
    Multi_join_in_progress = false;
}

// 0x009FD500 (:1845): the session deleted (processed until gone) and freed
void multi_game_lobby_shutdown()
{
    if (Lobby_session == nullptr)
    {
        return;
    }
    // multi_session_base::session_delete, inlined
    Lobby_session->session_delete();
    while (Lobby_session->get_state() != MSS_NONE)
    {
        Lobby_session->process();
    }
    Session_pool.free(Lobby_session);
    Lobby_session = nullptr;
}

// 0x00A01350 (:1860): every frame: classes, the lobby state packet, the lingering players, and the state machine
void multi_game_lobby_process()
{
    static unsigned int s_last_time_match_query_sent;
    if (Lobby_session != nullptr && Lobby_session->get_state() == MSS_READY)
    {
        if (!Lobby_session->am_i_host())
        {
            multi_connection* host = Lobby_session->get_host_connection();
            if (host != nullptr)
            {
                if (!Sent_play_history)
                {
                    multi_game_preferences_send_my_history(host);
                    Sent_play_history = true;
                }
                // Original: the game session's handle is tested, the lobby session type's is set.
                if (Local_game_data_handles[ST_GAME] != 0xFFFFFFFFu)
                {
                    multi_game_data_set_desired_class(Local_game_data_handles[Lobby_session->get_info()->type], Default_class_info->name);
                }
            }
        }
        else
        {
            for (unsigned int handle = multi_game_data_handles_begin(Lobby_session->get_info()->type, false, false); handle != 0xFFFFFFFFu;
                 handle = multi_game_data_handles_next(handle, false, false))
            {
                multi_game_data_set_desired_class(handle, Default_class_info->name);
            }
        }
    }
    // Original: Lobby_session is not checked for null from here on.
    if (Lobby_session->get_state() == MSS_MIGRATING)
    {
        return;
    }
    if (Lobby_state < MGLSM_SELECTING_BEST_HOST)
    {
        multi_game_lobby_assign_teams();
    }
    // The players who left stop lingering when their time is up.
    for (const multi_game_data* data = multi_game_data_begin(ST_GAME, false, true); data != nullptr; data = multi_game_data_next(data, false, true))
    {
        if (data->linger_time.valid() && data->linger_time.elapsed() && data->handle != 0xFFFFFFFFu)
        {
            game_data_of(data->handle)->linger_time.invalidate();
        }
    }
    if (Send_lobby_state_update && (Lobby_session->get_state() == MSS_READY || Lobby_session->get_state() == MSS_IN_GAME) && Lobby_session->am_i_host())
    {
        update_lobby_states();
        Send_lobby_state_update = false;
    }
    const multi_game_playlist* playlist = find_playlist(Current_user_playlist);
    if (Lobby_session->get_state() == MSS_NONE && Lobby_state != MGLSM_NONE)
    {
        // The lobby session ended under the lobby.
        Pending_session_info.init();
        Pending_session_coordinator_id.init();
        Pending_session_exists = false;
        multi_game_lobby_set_state(MGLSM_NONE, Clock_current_time);
    }
    if (Lobby_session->get_state() != MSS_NONE)
    {
        // A public lobby's host repeats its match query to the members.
        const multi_session_info_base* info = Lobby_session->get_info();
        if (Lobby_state != MGLSM_NONE && Lobby_session->am_i_host() && !gameseq_state_is_active(GS_GAMEPLAY) && info != nullptr && !info->systemlink_or_lan &&
            !info->party_lobby && keen::SystemTimer::getCurrentMilliseconds() - s_last_time_match_query_sent > Match_query_interval_ms)
        {
            s_last_time_match_query_sent = keen::SystemTimer::getCurrentMilliseconds();
            multi_game_lobby_send_match_query_to_all(Preferences->current_match);
        }
    }
    const int num_marauders = multi_game_mode_get_team_size(ST_GAME, HUMAN_TEAM_MARAUDER, 0xFFFFFFFFu);
    const int num_players = Lobby_session->get_slots_used() - num_marauders;
    const bool enough_ready_ups = Players_acked_for_start.size() >= multi_game_lobby_get_ready_ups_needed() && !Ignore_ready_ups;
    switch (Lobby_state)
    {
    case MGLSM_NONE:
    {
        const multi_session_state session_state = Lobby_session->get_state();
        if (session_state == MSS_NONE)
        {
            if (Pending_session_exists)
            {
                multi_game_lobby_join(&Pending_session_info, &Pending_session_coordinator_id, false, false);
                Pending_session_exists = false;
            }
            return;
        }
        if (session_state != MSS_READY)
        {
            return;
        }
        multi_game_lobby_set_state(Lobby_session->am_i_host() ? MGLSM_HOST : MGLSM_CLIENT, Clock_current_time);
        if (Lobby_session->am_i_host() && (!Server_options_transmit_interval.valid() || Server_options_transmit_interval.elapsed()))
        {
            Server_options_transmit_interval.set(Server_options_interval_ms);
            multi_game_send_server_options(Lobby_session);
        }
        return;
    }
    case MGLSM_CLIENT:
        // The lobby migrated to this machine.
        if (Lobby_session->am_i_host())
        {
            multi_game_lobby_set_state(MGLSM_HOST, Clock_current_time);
        }
        return;
    case MGLSM_HOST:
        if (!Lobby_session->am_i_host())
        {
            multi_game_lobby_set_state(MGLSM_NONE, Clock_current_time);
            return;
        }
        if (Lobby_session->get_state() == MSS_IN_GAME)
        {
            Lobby_session->session_end_gameplay();
            return;
        }
        if (Server_options_transmit_interval.valid() && !Server_options_transmit_interval.elapsed())
        {
            return;
        }
        Server_options_transmit_interval.set(Server_options_interval_ms);
        multi_game_send_server_options(Lobby_session);
        return;
    // The public matchmaking states (a system link or private lobby never enters them). A change to the state the lobby
    // is already in does nothing (multi_game_lobby_set_state); rfg.exe inlines that test with the function.
    case MGLSM_WAITING_FOR_MIN_PLAYERS:
        if (!Lobby_session->am_i_host())
        {
            return;
        }
        // Magic number: the host is still alone when the timeout runs out, so the lobby closes.
        if (Lobby_session->get_state() == MSS_READY && num_players == 1 && (!Lobby_timeout.valid() || Lobby_timeout.elapsed()))
        {
            Lobby_session->session_delete();
        }
        if (playlist == nullptr)
        {
            return;
        }
        if (Ignore_minimum_players || (playlist->min_players <= num_players && Team_disparity.num_players < num_players) || Force_allow_single_player ||
            Launch_regardless_of_teams_or_players)
        {
            multi_game_lobby_set_state(MGLSM_WAITING_FOR_MARGINALLY_FAIR_TEAMS, Clock_current_time);
        }
        if (Lobby_session->get_state() == MSS_IN_GAME)
        {
            Lobby_session->session_end_gameplay();
        }
        return;
    case MGLSM_WAITING_FOR_MARGINALLY_FAIR_TEAMS:
        if (!Lobby_session->am_i_host() || Auto_launch_match_entry == nullptr)
        {
            return;
        }
        // Magic number: teams at most 1 player apart are marginally fair.
        if (!multi_gameplay_mode_is_team_mode(Auto_launch_match_entry->mode) || Team_disparity.num_players <= 1 || Launch_regardless_of_teams_or_players)
        {
            multi_game_lobby_set_state(MGLSM_WAITING_FOR_PLAYERS, Clock_current_time);
        }
        if (playlist != nullptr && num_players < playlist->min_players && Team_disparity.num_players < num_players && !Force_allow_single_player &&
            !Launch_regardless_of_teams_or_players && !Ignore_minimum_players)
        {
            multi_game_lobby_set_state(MGLSM_WAITING_FOR_MIN_PLAYERS, Clock_current_time);
        }
        if (Lobby_session->am_i_host() && Lobby_session->get_state() == MSS_IN_GAME)
        {
            Lobby_session->session_end_gameplay();
        }
        return;
    case MGLSM_WAITING_FOR_PLAYERS:
        if (!Lobby_session->am_i_host())
        {
            return;
        }
        // Original: the playlist is not checked for null in this test, only inside it.
        if (Lobby_session->get_slots_reserved() > 0 || (Lobby_timeout.valid() && !Lobby_timeout.elapsed() && num_players < playlist->max_players && !enough_ready_ups))
        {
            // Still waiting: too few players or unfair teams send the lobby back.
            if (playlist != nullptr && !Force_allow_single_player && !Launch_regardless_of_teams_or_players)
            {
                if (!Ignore_minimum_players && num_players < playlist->min_players)
                {
                    multi_game_lobby_set_state(MGLSM_WAITING_FOR_MIN_PLAYERS, Clock_current_time);
                }
                else if (Auto_launch_match_entry != nullptr && multi_gameplay_mode_is_team_mode(Auto_launch_match_entry->mode) && Team_disparity.num_players > 1)
                {
                    multi_game_lobby_set_state(MGLSM_WAITING_FOR_MARGINALLY_FAIR_TEAMS, Clock_current_time);
                }
            }
        }
        else if (Launch_regardless_of_teams_or_players)
        {
            multi_game_lobby_set_state(MGLSM_WAITING_FOR_BALANCED_TEAMS, Clock_current_time);
        }
        else if (enough_ready_ups)
        {
            multi_game_lobby_set_state(MGLSM_GAME_COUNTDOWN, Clock_current_time);
        }
        else if (num_players > 2) // Magic number: 2 players or fewer have no teams worth balancing and keep waiting.
        {
            multi_game_lobby_set_state(MGLSM_WAITING_FOR_BALANCED_TEAMS, Clock_current_time);
        }
        if (Lobby_session->am_i_host() && Lobby_session->get_state() == MSS_IN_GAME)
        {
            Lobby_session->session_end_gameplay();
        }
        return;
    case MGLSM_WAITING_FOR_BALANCED_TEAMS:
        if (!Lobby_session->am_i_host())
        {
            return;
        }
        if (Launch_regardless_of_teams_or_players)
        {
            multi_game_lobby_set_state(MGLSM_GAME_COUNTDOWN, Clock_current_time);
        }
        else if (Team_disparity.num_players > 1) // Magic number: as in MGLSM_WAITING_FOR_MARGINALLY_FAIR_TEAMS.
        {
            multi_game_lobby_set_state(MGLSM_WAITING_FOR_MARGINALLY_FAIR_TEAMS, Clock_current_time);
        }
        else if (Lobby_session->get_slots_reserved() < 1 && (enough_ready_ups || (Team_disparity.num_players == 0 && !Assume_bad_teams)))
        {
            multi_game_lobby_set_state(MGLSM_GAME_COUNTDOWN, Clock_current_time);
        }
        if (Lobby_session->get_state() == MSS_IN_GAME)
        {
            Lobby_session->session_end_gameplay();
        }
        return;
    case MGLSM_GAME_COUNTDOWN:
    case MGLSM_VETOED:
        if (!Lobby_session->am_i_host())
        {
            return;
        }
        if (Lobby_session->get_state() == MSS_READY && (!Lobby_timeout.valid() || Lobby_timeout.elapsed()) && !Pushed_to_game)
        {
            multi_game_lobby_signal_game_start();
            return;
        }
        if (Team_disparity.num_players > 1)
        {
            multi_game_lobby_set_state(MGLSM_WAITING_FOR_MARGINALLY_FAIR_TEAMS, Clock_current_time);
            return;
        }
        if (Vetoes_allowed && multi_game_lobby_get_vetoes_needed() <= Veto_count)
        {
            multi_game_lobby_set_state(MGLSM_VETOED, Clock_current_time);
            return;
        }
        if (playlist != nullptr && num_players < playlist->min_players && Team_disparity.num_players < num_players && !Force_allow_single_player &&
            !Launch_regardless_of_teams_or_players && !Ignore_minimum_players)
        {
            multi_game_lobby_set_state(MGLSM_WAITING_FOR_MIN_PLAYERS, Clock_current_time);
        }
        return;
    case MGLSM_SELECTING_BEST_HOST:
        if (!Lobby_session->am_i_host())
        {
            return;
        }
        if (multi_game_lobby_get_best_host_connection() == nullptr)
        {
            multi_game_lobby_set_state(MGLSM_SELECTING_BEST_HOST, Clock_current_time);
            return;
        }
        if (Lobby_best_host_state_acked)
        {
            multi_game_lobby_set_state(MGLSM_COMMUNICATING_BEST_HOST, Clock_current_time);
            return;
        }
        if (Lobby_timeout.elapsed())
        {
            multi_game_lobby_set_state(MGLSM_SELECTING_BEST_HOST, Clock_current_time);
        }
        return;
    case MGLSM_COMMUNICATING_BEST_HOST:
        if (!Lobby_session->am_i_host())
        {
            return;
        }
        if (multi_game_lobby_get_best_host_connection() == nullptr)
        {
            multi_game_lobby_set_state(MGLSM_SELECTING_BEST_HOST, Clock_current_time);
            return;
        }
        if (Lobby_best_host_id_acked)
        {
            multi_game_lobby_set_state(MGLSM_MIGRATING_BEST_HOST, Clock_current_time);
            return;
        }
        if (Lobby_timeout.elapsed())
        {
            multi_game_lobby_set_state(MGLSM_SELECTING_BEST_HOST, Clock_current_time);
        }
        return;
    case MGLSM_MIGRATING_BEST_HOST:
        if (!Lobby_session->am_i_host() || Lobby_session->get_state() != MSS_READY)
        {
            return;
        }
        if (Lobby_best_host_player_uid.get_internal_id().is_equal(My_player_uid.get_internal_id()))
        {
            // The best host starts the game once GameLink has made it the lobby owner.
            g_gameLinkMutex.lock(0);
            if (Remote_game_session.getGameLinkSession()->isLobbyOwner())
            {
                multi_game_lobby_set_state(MGLSM_STARTING_GAME, Clock_current_time);
            }
            g_gameLinkMutex.unlock();
            return;
        }
        multi_game_lobby_set_state(MGLSM_SELECTING_BEST_HOST, Clock_current_time);
        return;
    case MGLSM_STARTING_GAME:
        if (!Lobby_session->am_i_host())
        {
            return;
        }
        // A system link lobby needs no session start.
        if (Lobby_session->get_info()->systemlink_or_lan || Lobby_session->get_state() == MSS_IN_GAME)
        {
            multi_game_lobby_set_state(MGLSM_IN_GAME, Clock_current_time);
            return;
        }
        if (Lobby_session->get_state() == MSS_READY)
        {
            Lobby_session->session_start_gameplay();
        }
        return;
    case MGLSM_GAME_OVER:
        process_ending_game();
        return;
    default:
        return;
    }
}

// 0x0097BC70 (:2246): the lobby's session info: a peer to peer game session, 16 slots in a system link or private
// lobby, the playlist's size plus 4 (16 at most) otherwise
void multi_game_lobby_fill_session_info(multi_session_info* pInfo, const game_query* pGameQuery, const multi_player_uid& hostId, const kaiko::GameLinkLobby& lobbyId, bool syslink, bool private_lobby, bool dedicated, bool party_lobby)
{
    const multi_game_playlist* playlist = find_playlist(pGameQuery->m_playlist_id);
    pInfo->init();
    pInfo->host_player_uid = hostId;
    pInfo->m_lobbyId = lobbyId;
    pInfo->uses_presence = false;
    pInfo->reports_stats = false;
    pInfo->advertise_for_matchmaking = !syslink;
    pInfo->party_lobby = party_lobby;
    pInfo->type = ST_GAME;
    pInfo->systemlink_or_lan = syslink;
    pInfo->uses_peer_to_peer_networking = true;
    pInfo->uses_abitration = false;
    pInfo->private_match = private_lobby;
    pInfo->invite_type = SIT_ALL;
    pInfo->dedicated = dedicated;
    if (!private_lobby && !syslink)
    {
        // Original: the playlist is not checked for null.
        int max_slots = playlist->max_players + 4;
        if (max_slots > 15)
        {
            max_slots = 16;
        }
        pInfo->max_slots = max_slots;
        pInfo->allow_join_in_progress = true;
        return;
    }
    pInfo->max_slots = 16;
    pInfo->allow_join_in_progress = true;
}

// 0x009FF190 (:2278): the host creates the lobby session (the lobby state machine starts over)
void multi_game_lobby_create(bool syslink, bool private_lobby, bool dedicated, bool party_lobby)
{
    if (Lobby_session == nullptr || Lobby_session->get_state() != MSS_NONE)
    {
        return;
    }
    if (!Lobby_session->get_info()->systemlink_or_lan && !Lobby_session->get_info()->private_match)
    {
        Mp_server_options.reinit();
    }
    if (Lobby_state != MGLSM_NONE)
    {
        multi_game_lobby_set_state(MGLSM_NONE, Clock_current_time);
    }
    multi_session_info info;
    kaiko::GameLinkLobby no_lobby;
    no_lobby.id.asUint64 = 0;
    multi_game_lobby_fill_session_info(&info, &Preferences->current_match, My_player_uid, no_lobby, syslink, private_lobby, dedicated, party_lobby);
    multi_game_data_clear_players(ST_GAME);
    Lobby_session->register_join_callbacks(multi_game_lobby_player_may_join, multi_game_lobby_player_joined, multi_game_about_to_remove_connection, multi_game_lobby_player_left);
    Lobby_session->register_migration_callbacks(multi_game_lobby_migration_allowed, nullptr, multi_game_lobby_migration_succeeded, multi_game_lobby_migration_failed);
    Lobby_session->register_event_callbacks(nullptr, nullptr);
    Lobby_session->session_create(&info, nullptr);
    Pushed_to_game = false;
    Left_lobby_by_choice = false;
    Current_session_is_started = 0;
    Auto_launch_match_entry = nullptr;
    Server_thinks_I_am_ready = false;
    Ready_packet_sent = false;
    I_am_ready = false;
    Auto_launch_veto_entry = nullptr;
    Players_acked_for_start.clear();
}

// 0x009FF700 (:2317): a client joins a lobby session (or, while in another lobby, leaves it and joins once it is gone)
void multi_game_lobby_join(const multi_session_info* session_info, const multi_session_info_base* coordinator_id, bool reservedSlot, bool ignoreGameSeqState)
{
    if (session_info == nullptr || Lobby_session == nullptr)
    {
        return;
    }
    if (!Lobby_session->get_info()->systemlink_or_lan && !Lobby_session->get_info()->private_match)
    {
        Mp_server_options.reinit();
    }
    if (Lobby_state != MGLSM_NONE)
    {
        multi_game_lobby_set_state(MGLSM_NONE, Clock_current_time);
    }
    if (Lobby_session->get_state() != MSS_NONE)
    {
        if (Lobby_session->get_info()->compare_by_id(*session_info) != 0)
        {
            // multi_session_base::session_delete, inlined
            Lobby_session->session_delete();
            Pending_session_info.copy(*session_info);
            if (coordinator_id == nullptr)
            {
                Pending_session_coordinator_id.init();
            }
            else
            {
                Pending_session_coordinator_id.copy(*coordinator_id);
            }
            Pending_session_exists = true;
        }
        return;
    }
    if (gameseq_get_state() != GS_MAINMENU && !ignoreGameSeqState)
    {
        // Back out of the menus that cannot host the lobby.
        g_gameLinkMutex.lock(0);
        kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
        const bool party_member = link->isPartyEnabled() && !link->isPartyOwner();
        g_gameLinkMutex.unlock();
        if (gameseq_get_state() != GS_MULTIPLAYER_GAME_LOBBY || !party_member)
        {
            const game_state gs = gameseq_get_state();
            if (gs != GS_MULTIPLAYER_SEARCH_MATCHMAKING && gs != GS_MULTIPLAYER_PARTY_LOBBY && gs != GS_MULTIPLAYER_GAME_LOBBY &&
                gs != GS_MULTIPLAYER_SYSLINK_FIND_SERVERS && gs != GS_MULTIPLAYER_LIVE_FIND_SERVERS)
            {
                if (gameseq_get_state() != GS_MULTIPLAYER_XP_TALLY && gameseq_get_state() != GS_MULTIPLAYER_RESULTS)
                {
                    gameseq_set_state(GS_MAINMENU, false);
                }
                else
                {
                    if (gameseq_get_state() == GS_MULTIPLAYER_XP_TALLY)
                    {
                        gameseq_pop_state();
                    }
                    gameseq_pop_state();
                    gameseq_pop_state();
                    if (session_info->party_lobby)
                    {
                        gameseq_pop_state();
                        gameseq_pop_state();
                    }
                }
            }
        }
        else if (session_info->party_lobby)
        {
            gameseq_pop_state();
            gameseq_pop_state();
        }
    }
    if (!session_info->systemlink_or_lan && !session_info->private_match)
    {
        const multi_game_playlist* playlist = multi_game_playlist_get_by_id(Preferences->current_match.m_playlist_id);
        if (playlist != nullptr)
        {
            multi_core_platform_set_game_type_context(playlist->game_type);
            multi_core_platform_set_game_mode_context(playlist->mode_type);
        }
    }
    else if (Current_game_type_context != PGTC_PUBLIC)
    {
        Current_game_type_context = PGTC_PUBLIC;
    }
    multi_game_data_clear_players(ST_GAME);
    Lobby_session->register_join_callbacks(multi_game_lobby_player_may_join, multi_game_lobby_player_joined, multi_game_about_to_remove_connection, multi_game_lobby_player_left);
    Lobby_session->register_migration_callbacks(multi_game_lobby_migration_allowed, nullptr, multi_game_lobby_migration_succeeded, multi_game_lobby_migration_failed);
    Lobby_session->register_event_callbacks(nullptr, nullptr);
    multi_session_info info;
    multi_game_lobby_fill_session_info(&info, &Preferences->current_match, session_info->host_player_uid, session_info->m_lobbyId, session_info->systemlink_or_lan,
                                       session_info->private_match, session_info->dedicated, session_info->party_lobby);
    Lobby_session->session_join(&info, nullptr, reservedSlot);
    Left_lobby_by_choice = false;
    Pushed_to_game = false;
    Sent_play_history = false;
    Players_acked_for_start.clear();
    I_am_ready = false;
    Server_thinks_I_am_ready = false;
    Ready_packet_sent = false;
    Current_session_is_started = 0;
}

// 0x00956A00 (:2482)
bool multi_game_lobby_pending_join()
{
    return Pending_session_exists;
}

// 0x00999740 (:2487): the logging info to every game session member
void multi_game_lobby_logging_reinited()
{
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    multi_connection* first = sp->get_connection_list();
    for (multi_connection* cp = first; cp != nullptr;)
    {
        if (cp != sp->get_me_connection())
        {
            gamelog_send_logging_info(cp);
        }
        cp = cp->next;
        if (cp == first)
        {
            break;
        }
    }
}

// Port helper (rfg.exe inlines it in both start functions): the game snapshot set is the game session's players, all
// in the game.
static void snapshot_players_in_game()
{
    multi_game_data_clone_set(ST_GAME, ST_GAME_SNAPSHOT);
    for (int i = 0; i < 16; i++)
    {
        Mp_game_data->data[i][ST_GAME_SNAPSHOT].in_game = false;
    }
    for (unsigned int handle = multi_game_data_handles_begin(ST_GAME_SNAPSHOT, false, false); handle != 0xFFFFFFFFu;
         handle = multi_game_data_handles_next(handle, false, false))
    {
        game_data_of(handle)->in_game = true;
    }
}

// 0x009F0F10 (:2498): the host starts the match: the mode, the level load, and the join game start to every client
void multi_game_lobby_server_start_game()
{
    gamelog_local_reinit(false, multi_game_lobby_logging_reinited);
    snapshot_players_in_game();
    multi_gameplay_mode_change(Mp_server_options.get_mode());
    World.set_pending_territory_load(Mp_server_options.get_level()->filename, true, false, false, -1, false);
    if (Auto_launch_match_entry != nullptr)
    {
        multi_game_preferences_mark_played_entry(Auto_launch_match_entry->entry_id.get_crc());
    }
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    multi_connection* host = sp->get_host_connection();
    multi_connection* first = sp->get_connection_list();
    for (multi_connection* cp = first; cp != nullptr;)
    {
        if (cp != host)
        {
            multi_game_send_join_game_start(cp, Mp_server_options.get_level()->filename);
            // The send thread is woken and the packet goes out at once.
            if (MP_send_process_trigger != nullptr)
            {
                scheduler_relinquish_cpu(MP_send_process_trigger, ccjt_any);
                MP_send_process_trigger = nullptr;
            }
            multi_connection_flush_current_packet_no_sync(cp);
            Multi_connections_acquired = 0;
        }
        cp = cp->next;
        if (cp == first)
        {
            break;
        }
    }
    // -2: every message box
    if (ui_is_msgbox_active(-2))
    {
        ui_hide_msgbox(-2, true);
    }
    Current_session_is_started = 1;
}

// 0x009F8130 (:2542): a client follows the join game start: the lobby session becomes the game session, the mode starts
// (unless a match is already playing, a later round) and the level loads
void multi_game_lobby_client_start_game()
{
    Game_session = Lobby_session;
    Lobby_session->register_migration_callbacks(multi_game_migration_allow_migration, multi_game_migration_about_to_start, multi_game_migration_just_occurred, multi_game_migration_failed);
    Game_session->register_join_callbacks(multi_game_player_may_join, nullptr, multi_game_about_to_remove_connection, multi_game_player_left);
    if (Game_bandwidth_test_session_type != ST_INVALID)
    {
        send_bandwidth_test_stop();
        multi_game_bw_system_init();
    }
    if (!Multi_match_info.playing_match)
    {
        snapshot_players_in_game();
        multi_gameplay_mode_change(Mp_server_options.get_mode());
        if (Auto_launch_match_entry != nullptr)
        {
            multi_game_preferences_mark_played_entry(Auto_launch_match_entry->entry_id.get_crc());
        }
    }
    World.set_pending_territory_load(Mp_server_options.get_level()->filename, true, false, false, -1, false);
    if (ui_is_msgbox_active(-2))
    {
        ui_hide_msgbox(-2, true);
    }
    Current_session_is_started = 1;
}

// 0x009FFDD0 (:2577): the lobby session is deleted and the lobby state cleared
void multi_game_lobby_leave(bool left_by_choice)
{
    if (Lobby_session == nullptr)
    {
        return;
    }
    // multi_session_base::session_delete, inlined
    Lobby_session->session_delete();
    if (Lobby_state != MGLSM_NONE)
    {
        multi_game_lobby_set_state(MGLSM_NONE, Clock_current_time);
    }
    Pushed_to_game = false;
    Left_lobby_by_choice = left_by_choice;
}

// 0x00956A10 (:2588)
bool multi_game_lobby_left_by_choice()
{
    return Left_lobby_by_choice;
}

// 0x00956A20 (:2593)
void multi_game_lobby_reset_leaving_state()
{
    Left_lobby_by_choice = false;
}

// 0x0097BD50 (:2599): every client of a system link or private lobby can play the server options
client_start_test_results multi_game_lobby_can_start_game()
{
    const multi_session_info_base* info = Lobby_session->get_info();
    if (!info->systemlink_or_lan && !info->private_match)
    {
        return CST_OK;
    }
    multi_connection* first = Lobby_session->get_connection_list();
    for (multi_connection* cp = first; cp != nullptr;)
    {
        if (cp != Lobby_session->get_me_connection())
        {
            const client_start_test_results result = client_start_test(cp);
            if (result != CST_OK)
            {
                return result;
            }
        }
        cp = cp->next;
        if (cp == first)
        {
            break;
        }
    }
    return CST_OK;
}

// 0x00A00290 (:2624): the host's launch: the best-host handshake starts the game
void multi_game_lobby_signal_game_start()
{
    const client_start_test_results result = multi_game_lobby_can_start_game();
    if (result != CST_OK)
    {
        multi_player_menu_system_pull_up_unable_to_launch_message_box(result);
        return;
    }
    if (Lobby_session->get_info()->party_lobby == true)
    {
        Mp_server_options.last_match_was_custom_game = true;
    }
    multi_game_lobby_set_state(MGLSM_SELECTING_BEST_HOST, Clock_current_time);
}

// 0x00A002D0 (:2656): the match is over: the game session is the lobby again and the bandwidth test restarts
void multi_game_lobby_signal_game_end()
{
    if (Lobby_state != MGLSM_GAME_OVER)
    {
        multi_game_lobby_set_state(MGLSM_GAME_OVER, Clock_current_time);
    }
    const bool bandwidth_test = Game_bandwidth_test_session_type != ST_INVALID;
    Game_session = nullptr;
    Lobby_session->register_join_callbacks(multi_game_lobby_player_may_join, multi_game_lobby_player_joined, multi_game_about_to_remove_connection, multi_game_lobby_player_left);
    Lobby_session->register_migration_callbacks(multi_game_lobby_migration_allowed, nullptr, multi_game_lobby_migration_succeeded, multi_game_lobby_migration_failed);
    Lobby_session->register_event_callbacks(nullptr, nullptr);
    Game_voice_cb = multi_game_lobby_voice_should_send;
    if (bandwidth_test)
    {
        send_bandwidth_test_stop();
        multi_game_bw_system_init();
    }
    Tm_state = TM_NONE;
    Game_bandwidth_state = TEST_BW_DONE;
    Game_last_test_packet_rcvd_time.set(0);
    Game_bandwidth_test_session_type = ST_GAME;
    // The game session's bandwidth test in 3 s.
    Game_bandwidth_test_start_time.set(3000);
}

// 0x00956A30 (:2668): from the best-host selection to the game start
bool multi_game_lobby_game_starting()
{
    return Lobby_state > MGLSM_VETOED && Lobby_state < MGLSM_IN_GAME;
}

// 0x00999780 (:2681): the game session's info is kept for the results screens
void multi_game_lobby_push_to_game()
{
    Pushed_to_game = true;
    Game_session_info.copy(*Game_session->get_info());
}

// 0x00956A50 (:2687)
bool multi_game_lobby_pushed_to_game()
{
    return Pushed_to_game;
}

// 0x00956A60 (:2692)
void multi_game_lobby_clear_pushed_to_game()
{
    Pushed_to_game = false;
}

// 0x00956A70 (:2697)
const multi_session_info* multi_game_lobby_get_game_session_info()
{
    return &Game_session_info;
}

// 0x009C6550 (:2702): PT_LOBBY_CHOOSE_BEST_HOST: the client named best host acknowledges; the host collects the acks
void multi_game_lobby_process_best_host(unsigned char* packet_data, const sender& from)
{
    multi_session* sp = Lobby_session;
    int packet_offset = 1;
    switch (packet_data[0])
    {
    case Best_host_id:
        if (!Lobby_session->am_i_host())
        {
            Lobby_best_host_player_uid.read_from_packet(packet_data, packet_offset);
            if (Lobby_best_host_player_uid.get_internal_id().is_equal(My_player_uid.get_internal_id()) && !sp->am_i_host())
            {
                unsigned char packet[1200];
                packet[0] = PT_LOBBY_CHOOSE_BEST_HOST;
                unsigned short packet_size = packet_header_size(PT_LOBBY_CHOOSE_BEST_HOST);
                packet[packet_size++] = Best_host_id_ack;
                packet_set_size(packet, packet_size);
                multi_io_send_reliable(sp->get_host_connection(), packet, packet_size, nullptr, nullptr, nullptr);
            }
        }
        return;
    case Best_host_id_ack:
        if (Lobby_session->am_i_host() && from.connection == multi_game_lobby_get_best_host_connection())
        {
            Lobby_best_host_id_acked = true;
        }
        return;
    case Best_host_state_ack:
        if (Lobby_session->am_i_host() && from.connection->player_uid.get_internal_id().is_equal(Lobby_best_host_player_uid.get_internal_id()))
        {
            Lobby_best_host_state_acked = true;
        }
        return;
    }
}

// 0x00A00760 (:2756): PT_UPDATE_LOBBY_STATE (client): the host's lobby state (see update_lobby_states)
void multi_game_lobby_process_update_state(unsigned char* packet_data, const sender& from)
{
    if (packet_data[0] == MGLSM_NONE)
    {
        return;
    }
    int packet_offset = 1;
    Lobby_timeout_set_time = read_u32_be(packet_data, packet_offset);
    multi_game_lobby_set_state((multi_game_lobby_state_machine)packet_data[0], Lobby_timeout_set_time);
    const int time_left = (int)read_u32_be(packet_data, packet_offset);
    const unsigned int match_entry_id = read_u32_be(packet_data, packet_offset);
    const unsigned int veto_entry_id = read_u32_be(packet_data, packet_offset);
    const multi_game_playlist_entry* match_entry = multi_game_playlist_get_entry_by_id(find_playlist(Preferences->current_match.m_playlist_id), match_entry_id);
    Auto_launch_match_entry = match_entry;
    Auto_launch_veto_entry = multi_game_playlist_get_entry_by_id(find_playlist(Preferences->current_match.m_playlist_id), veto_entry_id);
    if (match_entry != nullptr)
    {
        apply_playlist_entry(Auto_launch_match_entry);
    }
    Veto_count = (int)read_u32_be(packet_data, packet_offset);
    Vetoes_allowed = packet_data[packet_offset++] != 0;
    multi_game_read_server_options_from_packet(&Mp_server_options, packet_data, packet_offset);
    multi_game_player_read_teams_from_packet(packet_data, packet_offset);
    multi_game_player_read_desired_weapons_from_packet(packet_data, packet_offset);
    read_ready_ups_from_packet(packet_data, packet_offset);
    if (Mp_server_options.get_mode() < MGPM_NUM_MODES && multi_gameplay_mode_is_team_mode(Mp_server_options.get_mode()))
    {
        const int disparity = multi_game_mode_get_team_size(ST_GAME, HUMAN_TEAM_EDF, 0xFFFFFFFFu) - multi_game_mode_get_team_size(ST_GAME, HUMAN_TEAM_GUERILLA, 0xFFFFFFFFu);
        Team_disparity.num_players = disparity < 0 ? -disparity : disparity;
    }
    if (time_left == -1)
    {
        Lobby_timeout.invalidate();
        return;
    }
    // Original: the timeout is taken over only while vetoing has not started (vetoes allowed, none cast) or after it
    // closed with vetoes cast.
    if (Vetoes_allowed ? Veto_count != 0 : Veto_count == 0)
    {
        return;
    }
    Lobby_timeout.set((int)(Lobby_timeout_set_time - Clock_current_time) + time_left);
}

// 0x00956A80 (:2815)
multi_game_lobby_state_machine multi_game_lobby_get_state()
{
    return Lobby_state;
}

// 0x00956A90 (:2820): -1 without a timeout
int multi_game_lobby_get_time_left()
{
    if (!Lobby_timeout.valid())
    {
        return -1;
    }
    return Lobby_timeout.time_until();
}

// 0x0097BEE0 (:2829): a public lobby's vote, once the countdown is reached
bool multi_game_lobby_veto_allowed()
{
    if (Spectating || !Vetoes_allowed)
    {
        return false;
    }
    const multi_session_info_base* info = Lobby_session->get_info();
    return (info->private_match || (!info->systemlink_or_lan && Lobby_state > MGLSM_WAITING_FOR_BALANCED_TEAMS)) && !info->systemlink_or_lan && !info->private_match;
}

// 0x00956AB0 (:2834)
bool multi_game_lobby_did_i_veto()
{
    return Vetoed_map;
}

// 0x0097BF30 (:2839): not as a marauder, once per match entry
bool multi_game_lobby_can_i_veto()
{
    const multi_game_data* my_data = multi_game_data_get(Local_game_data_handles[ST_GAME], false, false);
    if (my_data != nullptr && my_data->team == HUMAN_TEAM_MARAUDER)
    {
        return false;
    }
    return Auto_launch_match_entry != nullptr && Vetoes_allowed && !Vetoed_map;
}

// 0x009C66D0 (:2849): the local vote against the match entry (to the host when a client)
void multi_game_lobby_veto_map()
{
    if (Lobby_session == nullptr || Lobby_session->get_state() == MSS_NONE || Auto_launch_match_entry == nullptr || !Vetoes_allowed || Vetoed_map)
    {
        return;
    }
    Veto_count++;
    Vetoed_map = true;
    multi_game_preferences_mark_vetoed_entry(Auto_launch_match_entry->entry_id.get_crc());
    if (!Lobby_session->am_i_host())
    {
        unsigned char packet[8];
        packet[0] = PT_VETO;
        const unsigned short packet_size = packet_header_size(PT_VETO);
        packet_set_size(packet, packet_size);
        multi_io_send_reliable(Lobby_session->get_host_connection(), packet, packet_size, nullptr, nullptr, nullptr);
        if (!Lobby_session->am_i_host())
        {
            return;
        }
    }
    Send_lobby_state_update = true;
}

// 0x00956AC0 (:2873): PT_VETO (host)
void multi_game_lobby_process_veto(unsigned char* packet_data, const sender& from)
{
    if (from.connection == nullptr || !Vetoes_allowed)
    {
        return;
    }
    Veto_count++;
    if (Lobby_session->am_i_host())
    {
        Send_lobby_state_update = true;
    }
}

// 0x009997B0 (:2887): a majority of the players
int multi_game_lobby_get_vetoes_needed()
{
    const int needed = (Lobby_session->get_slots_used() - multi_game_mode_get_team_size(ST_GAME, HUMAN_TEAM_MARAUDER, 0xFFFFFFFFu)) / 2 + 1;
    return needed < 1 ? 1 : needed;
}

// 0x00956B00 (:2898)
int multi_game_lobby_get_veto_count()
{
    return Veto_count;
}

// 0x00956B10 (:2903)
const multi_game_playlist_entry* multi_game_lobby_get_veto_entry()
{
    return Auto_launch_veto_entry;
}

// 0x00956B20 (:2909): the host of a ready lobby that is not starting a game
bool multi_game_lobby_waiting_for_user_launch()
{
    if (Lobby_session->am_i_host() && (Lobby_state < MGLSM_SELECTING_BEST_HOST || Lobby_state > MGLSM_STARTING_GAME) && Lobby_session->get_state() == MSS_READY)
    {
        return Lobby_state == MGLSM_HOST;
    }
    return false;
}

// 0x00956B50 (:2930)
bool multi_game_lobby_has_game_settings()
{
    const multi_session_info_base* info = Lobby_session->get_info();
    if (!info->private_match && !info->systemlink_or_lan)
    {
        return Lobby_state > MGLSM_WAITING_FOR_BALANCED_TEAMS;
    }
    return true;
}

// 0x00956B80 (:2947)
bool multi_game_lobby_has_player_list()
{
    return true;
}

// 0x00956B90 (:2967): in a system link lobby, or a private custom game, before the countdown
bool multi_game_lobby_allow_team_changing()
{
    if (Lobby_state >= MGLSM_GAME_COUNTDOWN && Lobby_state <= MGLSM_GAME_OVER)
    {
        return false;
    }
    const multi_session_info_base* info = Lobby_session->get_info();
    return (info->private_match && Party_lobby_type == MPPLT_CUSTOM_GAME) || info->systemlink_or_lan;
}

// 0x00956BD0 (:2994)
bool multi_game_lobby_is_in_lobby()
{
    return Lobby_session != nullptr && (Lobby_session->get_state() != MSS_NONE || Pending_session_exists);
}

// 0x00956BF0 (:3005)
bool multi_game_lobby_is_in_party_lobby()
{
    return Lobby_session != nullptr && Lobby_session->get_state() != MSS_NONE && Lobby_session->get_info()->private_match == true;
}

// 0x0097BFA0 (:3016)
int multi_game_lobby_get_ready_up_count()
{
    return Players_acked_for_start.size();
}

// 0x009997E0 (:3021): two thirds of the players (2 at least)
int multi_game_lobby_get_ready_ups_needed()
{
    const int slots = Lobby_session->get_slots_reserved() + Lobby_session->get_slots_used();
    // 0.6666667: two thirds
    const int needed = fl_round((float)(slots - multi_game_mode_get_team_size(ST_GAME, HUMAN_TEAM_MARAUDER, 0xFFFFFFFFu)) * 0.6666667f);
    return needed < 3 ? 2 : needed;
}

// 0x009C67B0 (:3030)
void multi_game_lobby_set_ready(bool readyness)
{
    if (!multi_game_lobby_can_ready_up())
    {
        return;
    }
    I_am_ready = readyness;
    if (Lobby_session->am_i_host())
    {
        if (readyness)
        {
            if (Players_acked_for_start.contains(My_player_uid) < 1)
            {
                Players_acked_for_start.add(My_player_uid);
            }
        }
        else if (Players_acked_for_start.contains(My_player_uid) > 0)
        {
            Players_acked_for_start.remove_matching(My_player_uid);
        }
    }
    multi_game_lobby_send_ready_packet(My_player_uid, I_am_ready);
}

// 0x00956C10 (:3049)
bool multi_game_lobby_is_ready()
{
    return I_am_ready;
}

// 0x00956C20 (:3054)
bool multi_game_lobby_ready_allowed()
{
    return !Spectating && (Lobby_state == MGLSM_WAITING_FOR_PLAYERS || Lobby_state == MGLSM_WAITING_FOR_BALANCED_TEAMS);
}

// 0x00999840 (:3059)
bool multi_game_lobby_player_is_ready(const multi_player_uid& player_uid)
{
    if (player_uid.get_internal_id().is_equal(My_player_uid.get_internal_id()))
    {
        return I_am_ready;
    }
    for (int i = 0; i < Players_acked_for_start.size(); i++)
    {
        if (Players_acked_for_start[i].get_internal_id().is_equal(player_uid.get_internal_id()))
        {
            return true;
        }
    }
    return false;
}

// 0x0097BFC0 (:3068): public lobbies only, not as a marauder
bool multi_game_lobby_can_ready_up()
{
    const multi_game_data* my_data = multi_game_data_get(Local_game_data_handles[ST_GAME], false, false);
    if (my_data != nullptr && my_data->team == HUMAN_TEAM_MARAUDER)
    {
        return false;
    }
    const multi_session_info_base* info = Lobby_session->get_info();
    if (info->systemlink_or_lan || info->private_match)
    {
        return false;
    }
    return !Spectating && (Lobby_state == MGLSM_WAITING_FOR_PLAYERS || Lobby_state == MGLSM_WAITING_FOR_BALANCED_TEAMS);
}

// 0x009AFB60 (:3082): PT_READY_FOR_GAME_START: the sender's (or the named player's) ready-up
void multi_game_lobby_process_ready(unsigned char* packet_data, const sender& from)
{
    multi_player_uid player_uid = from.connection->player_uid;
    const unsigned char flags = packet_data[0];
    int packet_offset = 1;
    if ((flags & Ready_flag_sender_is_player) == 0)
    {
        player_uid.read_from_packet(packet_data, packet_offset);
    }
    int count = 0;
    for (int i = 0; i < Players_acked_for_start.size(); i++)
    {
        if (Players_acked_for_start[i].get_internal_id().is_equal(player_uid.get_internal_id()))
        {
            count++;
        }
    }
    if ((flags & Ready_flag_ready) == 0)
    {
        if (count > 0)
        {
            Players_acked_for_start.remove_matching(player_uid);
        }
    }
    else if (count == 0)
    {
        Players_acked_for_start.add(player_uid);
    }
    if (Lobby_session->am_i_host())
    {
        Send_lobby_state_update = true;
    }
}

// 0x00956C50 (:3111)
int multi_game_lobby_get_team_disparity()
{
    return Team_disparity.num_players;
}

// 0x00999890 (:3117): the host's teams: everyone civilian in a free-for-all mode; in a team mode, a system link or
// party lobby puts the unassigned players on the smaller team, a public lobby searches balanced teams
void multi_game_lobby_assign_teams()
{
    if (Lobby_session == nullptr || !Lobby_session->am_i_host() || Lobby_state == MGLSM_GAME_OVER)
    {
        return;
    }
    const multi_session_info_base* info = Lobby_session->get_info();
    if (Mp_server_options.get_mode() < MGPM_NUM_MODES && multi_gameplay_mode_is_team_mode(Mp_server_options.get_mode()))
    {
        if (!info->party_lobby && !info->systemlink_or_lan)
        {
            search_team_combinations(0, false);
            return;
        }
        for (const multi_game_data* data = multi_game_data_begin(info->type, false, false); data != nullptr; data = multi_game_data_next(data, false, false))
        {
            if (data->team == HUMAN_TEAM_CIVILIAN || data->team == HUMAN_TEAM_NONE)
            {
                const int edf = multi_game_mode_get_team_size(ST_GAME, HUMAN_TEAM_EDF, 0xFFFFFFFFu);
                const int guerilla = multi_game_mode_get_team_size(ST_GAME, HUMAN_TEAM_GUERILLA, 0xFFFFFFFFu);
                multi_game_data_set_player_team(data->handle, edf <= guerilla ? HUMAN_TEAM_EDF : HUMAN_TEAM_GUERILLA);
            }
        }
        const int disparity = multi_game_mode_get_team_size(ST_GAME, HUMAN_TEAM_EDF, 0xFFFFFFFFu) - multi_game_mode_get_team_size(ST_GAME, HUMAN_TEAM_GUERILLA, 0xFFFFFFFFu);
        Team_disparity.num_players = disparity < 0 ? -disparity : disparity;
        return;
    }
    for (const multi_game_data* data = multi_game_data_begin(info->type, false, false); data != nullptr; data = multi_game_data_next(data, false, false))
    {
        if (data->team != HUMAN_TEAM_MARAUDER && data->handle != 0xFFFFFFFFu)
        {
            game_data_of(data->handle)->team = HUMAN_TEAM_CIVILIAN;
            if (Local_game_data_handles[data->handle >> 16] == data->handle)
            {
                Spectating = false;
            }
        }
    }
    Team_disparity.num_players = 0;
}

// 0x00956C60 (:3151)
void multi_game_lobby_jip_accept(unsigned char* packet_data, const sender& from)
{
}

// 0x009AFC60 (:3230): PT_MATCH_QUERY, a playlist query (not a quickmatch), to every connected lobby member but the
// host (the host's members follow its query).
void multi_game_lobby_send_match_query_to_all(const game_query& matchQuery)
{
    if (matchQuery.m_query_type == QT_QUICKMATCH)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_MATCH_QUERY;
    unsigned short packet_size = packet_header_size(PT_MATCH_QUERY);
    matchQuery.write_to_packet(packet, packet_size);
    packet_set_size(packet, packet_size);
    multi_connection* cp = Lobby_session->get_connection_list();
    do
    {
        if (cp == nullptr)
        {
            return;
        }
        if (cp->connection_status != CS_DISCONNECTED && cp != cp->owner->get_host_connection())
        {
            multi_io_send(cp, packet, packet_size);
        }
        cp = cp->next;
    } while (cp != Lobby_session->get_connection_list());
}

// 0x0097C050 (:3254): PT_MATCH_QUERY from the host: its query becomes ours.
void multi_game_lobby_process_match_query(unsigned char* packet_data, const sender& from)
{
    multi_connection* host = from.connection->owner->get_host_connection();
    if (host == nullptr || host != from.connection)
    {
        return;
    }
    game_query query;
    int packet_offset = 0;
    query.read_from_packet(packet_data, packet_offset);
    Preferences->current_match.copy(query);
}

// 0x00956C70 (:3278)
void multi_game_lobby_send_playlist_request_to_host(const game_query& matchQuery)
{
}

// 0x00956C80 (:3307)
void multi_game_lobby_process_playlist_request(unsigned char* packet_data, const sender& from)
{
}

// 0x009AFD40 (:3361): the host draws a match entry and a veto entry from its preferred playlist entries
void multi_game_lobby_set_actual_game_mode_map_etc()
{
    if (!Lobby_session->am_i_host())
    {
        return;
    }
    multi_game_preferences_read_from_profile();
    Auto_launch_match_entry = multi_game_preferences_choose_random_preferred_playlist_entry(Auto_launch_match_entry);
    Auto_launch_veto_entry = multi_game_preferences_choose_random_preferred_playlist_entry(Auto_launch_match_entry);
    apply_playlist_entry(Auto_launch_match_entry);
}

// 0x00956C90 (:3375): PT_INVITE_TYPE from the host: the lobby's invite type (party lobbies only).
void multi_game_lobby_process_invite_type(unsigned char* packet_data, const sender& from)
{
    if (Lobby_session != nullptr)
    {
        Lobby_session->process_invite_type_packet(packet_data, from); // (inlined)
    }
}

// ---- packet table
// The game packet types (ids 0x2E-0x7F; table dumped from rfg.exe .data on 2026-10-19). 0x2E "update" has no
// receive function (object updates arrive as core packet 0x12). The owner file is inferred from the .data order (right
// after Team_assignment_attempts_per_frame, before the matchmaking_internal statics).
packet_def Multi_game_packets[82] = { // 0x0167E710
    // {id, receive_func, packet_name, packet_short_name, direction, connection_status, connection_required,
    //  drop_on_cheating, group_id}
    {PT_CLIENT_UPDATE, nullptr, "update", "update", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x00000000
    {PT_CLIENT_CONTROLS, multi_game_cp_process_client_controls, "client_controls", "cl ctls", PD_CLIENT_TO_SERVER, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009DDB90
    {PT_LOGGING, gamelog_process_logging_info, "logging_info", "log nfo", PD_SERVER_TO_CLIENT, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x00750100
    {PT_PARTY_KICKED, multi_game_party_process_kicked, "pty_kicked", "pty_kicked", PD_SERVER_TO_CLIENT, PCS_CONNECTED_ANY, true, false, GID_NOT_IN_ORDER}, // 0x00A00D00
    {PT_PRIVATE_LOBBY_TYPE_REQ, ui_document_mp_party_lobby_type_request, "pty_lby_state_req", "pt_lby_st_rq", PD_CLIENT_TO_SERVER, PCS_CONNECTED_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x008AE100
    {PT_PRIVATE_LOBBY_TYPE, ui_document_mp_party_lobby_type, "pty_lby_state", "pt_lby_st", PD_SERVER_TO_CLIENT, PCS_CONNECTED_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x0088CE10
    {PT_GO_MATCHMAKING, ui_document_mp_party_lobby_go_matchmake, "go_mm", "go mm", PD_SERVER_TO_CLIENT, PCS_CONNECTED_ANY, true, false, GID_NOT_IN_ORDER}, // 0x008AE020
    {PT_RETURN_TO_LOBBY, ui_document_mp_party_lobby_return_to_lobby, "rtn_2_lby", "rtn to lbby", PD_SERVER_TO_CLIENT, PCS_CONNECTED_ANY, true, false, GID_NOT_IN_ORDER}, // 0x008AE110
    {PT_SERVER_OPTIONS, multi_game_process_server_options, "server_options", "srv opt", PD_SERVER_TO_CLIENT, PCS_CONNECTED_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x009BBD60
    {PT_READY_FOR_GAME_START, multi_game_lobby_process_ready, "ready", "rdy", PD_BOTH, PCS_CONNECTED_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x009AFB60
    {PT_JOIN_GAME_START, multi_game_process_join_game_start, "game_start", "gm strt", PD_SERVER_TO_CLIENT, PCS_CONNECTED_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x009C5E60
    {PT_CLIENT_LOADED_GAME, multi_game_join_process_client_loaded_game, "client_loaded_game", "cl ld gm", PD_CLIENT_TO_SERVER, PCS_CONNECTED_ANY, true, false, GID_NOT_IN_ORDER}, // 0x009DEE90
    {PT_PLAYER_DIED, multi_game_player_process_player_death, "player_died", "pl died", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_GAMEPLAY}, // 0x009E2460
    {PT_RESPAWN_PLAYER, multi_game_respawn_process_respawn, "player_respawn", "pl respn", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_GAMEPLAY}, // 0x009D2B60
    {PT_UPDATE_LOBBY_STATE, multi_game_lobby_process_update_state, "update_lobby_state", "up lby st", PD_SERVER_TO_CLIENT, PCS_CONNECTED_NOT_GAMEPLAY, true, false, GID_CONNECTION_PROCESSING}, // 0x00A00760
    {PT_LOBBY_CHOOSE_BEST_HOST, multi_game_lobby_process_best_host, "choose best host", "best host", PD_BOTH, PCS_CONNECTED_NOT_GAMEPLAY, true, false, GID_CONNECTION_PROCESSING}, // 0x009C6550
    {PT_REQUEST_JIP, multi_game_jip_request, "request jip", "req jip", PD_CLIENT_TO_SERVER, PCS_CONNECTED_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x009DC280
    {PT_ACCEPT_JIP, multi_game_process_join_game_start, "accept jip", "acc jip", PD_SERVER_TO_CLIENT, PCS_CONNECTED_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x009C5E60
    {PT_CLIENT_JOINED_JIP, multi_game_jip_client_joined, "joined jip", "joi jip", PD_SERVER_TO_CLIENT, PCS_CONNECTED_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x009958E0
    {PT_PLAY_HISTORY, multi_game_preferences_read_players_history, "play_history", "pl hist", PD_CLIENT_TO_SERVER, PCS_CONNECTED_NOT_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x00960810
    {PT_VETO, multi_game_lobby_process_veto, "veto", "veto", PD_CLIENT_TO_SERVER, PCS_CONNECTED_NOT_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x00956AC0
    {PT_PLAYER_REQUEST_TEAM, multi_game_player_process_team_request, "team_request", "team req", PD_CLIENT_TO_SERVER, PCS_ANY, true, false, GID_NOT_IN_ORDER}, // 0x009D0F10
    {PT_TEAM_CHANGE, multi_game_player_process_team_change, "team_change", "team ch", PD_SERVER_TO_CLIENT, PCS_CONNECTED_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x00982F60
    {PT_REQUEST_CLASS_CHANGE, multi_game_classes_process_class_request, "class_request", "cls req", PD_CLIENT_TO_SERVER, PCS_CONNECTED_NOT_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009BD000
    {PT_CLASS_CHANGE, multi_game_classes_process_class_change, "class_change", "cls ch", PD_SERVER_TO_CLIENT, PCS_CONNECTED_NOT_GAMEPLAY, true, false, GID_CONNECTION_PROCESSING}, // 0x0097ADD0
    {PT_REQUEST_DESIRED_WEAPON_CHANGE, multi_game_player_process_desired_weapon_request, "desired weapon req", "dweap req", PD_CLIENT_TO_SERVER, PCS_CONNECTED_NOT_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009D12A0
    {PT_DESIRED_WEAPON_CHANGE, multi_game_player_process_desired_weapon_change, "desired weapon change", "dweap ch", PD_SERVER_TO_CLIENT, PCS_CONNECTED_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x0095FD30
    {PT_PLAYER_NAME_CHANGED, multi_game_process_player_name_changed, "name changed", "name ch", PD_BOTH, PCS_CONNECTED_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x009BC9B0
    {PT_MODE_COMMON, multi_game_mode_process_mode_common, "mode_common packet", "mode cmmn", PD_BOTH, PCS_CONNECTED_GAMEPLAY, true, false, GID_GAMEPLAY}, // 0x009FE2C0
    {PT_MODE_SPECIFIC, multi_gameplay_mode_process_mode_specific, "mode_specific packet", "mode spcfc", PD_BOTH, PCS_CONNECTED_GAMEPLAY, true, false, GID_GAMEPLAY}, // 0x0094EB90
    {PT_BULLET_HIT, multi_gw_process_bullet_hit, "bullet_hit", "bullet hit", PD_BOTH, PCS_CONNECTED_GAMEPLAY, true, true, GID_NOT_IN_ORDER}, // 0x009D3D30
    {PT_TRACER, multi_gw_process_tracer, "tracer", "tracer", PD_BOTH, PCS_CONNECTED_GAMEPLAY, true, true, GID_NOT_IN_ORDER}, // 0x009D4C90
    {PT_MELEE_HIT, multi_gw_process_melee_hit, "melee_hit", "melee hit", PD_BOTH, PCS_CONNECTED_GAMEPLAY, true, true, GID_NOT_IN_ORDER}, // 0x009D5560
    {PT_EXPLOSION_HIT, multi_gw_process_explosion_hit, "expl_hit", "expl hit", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009D5FF0
    {PT_COLLISION_HIT, multi_gw_process_collision_hit, "collision_hit", "coll hit", PD_CLIENT_TO_SERVER, PCS_CONNECTED_GAMEPLAY, true, true, GID_NOT_IN_ORDER}, // 0x009F6D90
    {PT_IMPALING_COLLIDE, multi_gw_process_impaling_collide, "impaling collide", "impale", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009D6BD0
    {PT_CREATE_EXPLOSION, multi_gw_process_explosion_create, "explosion_create", "expl cr", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x006F02D0
    {PT_MOVER_APPLY_DAMAGE, multi_game_object_mover_process_apply_damage, "mover apply damage", "mvr apl dam", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009C85F0
    {PT_MOVER_ACTIVATED, multi_game_object_mover_process_activated, "mover activated", "mvr actv", PD_CLIENT_TO_SERVER, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009C8720
    {PT_DESTROYED_MOVERS_LIST, multi_game_object_process_destroyed_movers_list, "destroy list", "dstr list", PD_BOTH, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009C89B0
    {PT_REQUEST_DAMAGE, multi_game_player_process_request_damage, "request damage", "req dam", PD_CLIENT_TO_SERVER, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009D15D0
    {PT_REMOTE_CONSOLE, multi_game_process_remote_console_command, "remote console command", "rcon commnd", PD_BOTH, PCS_ANY, true, false, GID_NOT_IN_ORDER}, // 0x0094E920
    {PT_SPECTATOR, multi_process_spectator, "spectator", "spectator", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009DBCB0
    {PT_PLAYER_STATS, multi_game_stats_process_player_stats, "player stats", "pl stats", PD_BOTH, PCS_CONNECTED_ANY, true, false, GID_NOT_IN_ORDER}, // 0x00966490
    {PT_TEAM_STATS, multi_game_stats_process_team_stats, "team stats", "tm stats", PD_SERVER_TO_CLIENT, PCS_CONNECTED_ANY, true, false, GID_NOT_IN_ORDER}, // 0x009664B0
    {PT_PLAYER_WEAPONS_UPDATE, multi_game_process_player_weapons_update, "player weapons update", "pl weap up", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_GAMEPLAY}, // 0x009E2170
    {PT_LEFT_GAME, multi_game_process_left_game, "left game", "left gm", PD_BOTH, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009DB6F0
    {PT_DESTRUCTION_FRAME, multi_game_destruction_process_packet, "dest fr", "dest fr", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x00955810
    {PT_MIGRATE_REQ_STATUS, multi_game_process_migrate_req_status, "migrate req status", "mig req st", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009DF2E0
    {PT_MIGRATE_LAST_DF, multi_game_process_migrate_last_df, "migrate last df", "mig last df", PD_CLIENT_TO_SERVER, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x00957020
    {PT_MIGRATE_REQ_DF_UPDATE, multi_game_process_migrate_req_df_update, "migrate req df update", "mig req up", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009C5860
    {PT_MIGRATE_DF_UPDATE, multi_game_process_migrate_df_update, "destr update", "dest up", PD_BOTH, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009DEA30
    {PT_MIGRATE_DESTRUCT_COMPLETE, multi_game_process_migrate_destruct_complete, "destr complete", "dest comp", PD_CLIENT_TO_SERVER, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x00955BA0
    {PT_VALIDATE_DESTRUCTION, multi_game_validate_destruction, "validate_destr", "val dest", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009C4EB0
    {PT_VALIDATE_DESTRUCTION_SYNC, multi_game_process_validate_destruction_sync, "validate destr sync", "val dest syc", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x00955A20
    {PT_JIP_START_DESTRUCTION_SYNC, multi_game_destruction_start_jip_sync, "start destr sync", "str dest syc", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009C5760
    {PT_JIP_CLIENT_READY_FOR_DATA, multi_game_destruction_server_sync_send_data, "start destr data", "str dest dat", PD_CLIENT_TO_SERVER, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x00998B90
    {PT_JIP_FINISH_DESTRUCTION_SYNC, multi_game_destruction_finish_jip_sync, "finish destr sync", "fin dest syc", PD_CLIENT_TO_SERVER, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009DE750
    {PT_CLIENT_OBJECT_CREATE, multi_game_process_client_object_create, "client object create", "cl obj cr", PD_CLIENT_TO_SERVER, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009DD850
    {PT_CLIENT_CREATE_NO, multi_game_process_client_create_no, "client create no", "create no", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x00951270
    {PT_FLOOD_UPDATE, multi_game_process_flood_update, "flood up", "fld up", PD_BOTH, PCS_CONNECTED_NOT_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009A91E0
    {PT_FLOOD_FULL_RESULTS, multi_game_process_full_flood_results, "flood all", "fld all", PD_SERVER_TO_CLIENT, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x009A9340
    {PT_FIRE_REPAIR, multi_gw_process_fire_repair, "client fired repair", "cl rep", PD_CLIENT_TO_SERVER, PCS_CONNECTED_GAMEPLAY, true, true, GID_NOT_IN_ORDER}, // 0x009D6750
    {PT_BACKPACK_RACK_DESTROYED, multi_game_backpack_rack_process_destroyed, "bp rack destroyed", "bp rack dest", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_GAMEPLAY}, // 0x009DCBC0
    {PT_BACKPACK_RACK_REPAIRED, multi_game_backpack_rack_process_repaired, "bp rack repaired", "bp rack rep", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_GAMEPLAY}, // 0x009DCC80
    {PT_SYNC_BACKPACK_RACKS, multi_game_backpack_rack_process_sync, "sync bp racks", "syn bp rack", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_GAMEPLAY}, // 0x009BC510
    {PT_PICKED_UP_BACKPACK, multi_game_backpack_process_picked_up, "picked up bp", "pick up bp", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_GAMEPLAY}, // 0x009F4270
    {PT_DROPPED_BACKPACK, multi_game_backpack_process_dropped, "dropped bp", "drop bp", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_GAMEPLAY}, // 0x009DF630
    {PT_PLAYER_DIED_WITH_BACKPACK, multi_game_backpack_process_player_died_with_backpack, "player died with bp", "died bp", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_GAMEPLAY}, // 0x009F43E0
    {PT_TOGGLED_BACKPACK_ACTIVATION, multi_game_backpack_process_toggled_activation, "toggled bp", "tog bp", PD_BOTH, PCS_CONNECTED_GAMEPLAY, true, false, GID_GAMEPLAY}, // 0x009F8900
    {PT_SWAP_BACKPACK, multi_game_backpack_process_swap, "client swapped bp", "swap bp", PD_CLIENT_TO_SERVER, PCS_CONNECTED_GAMEPLAY, true, false, GID_GAMEPLAY}, // 0x009F4690
    {PT_RHINO_PLAYER_HIT, multi_game_backpack_process_rhino_hit, "rhino hit", "rhino hit", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009C7430
    {PT_RIPPLE_PLAYERS_HIT, multi_game_backpack_process_ripple_hits, "ripple hits", "ripple hits", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_GAMEPLAY}, // 0x009F44B0
    {PT_PLAYER_RAGDOLL, multi_game_player_process_player_ragdoll, "player_ragdoll", "pl rag", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009D1680
    {PT_PLAYER_RAGDOLL_POINT, multi_game_player_process_player_ragdoll_point_impulse, "player_ragdoll pt", "pl rag pt", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009D1F30
    {PT_XP_EVENT, multi_game_xp_process_event, "xp event", "xp", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009E2F20
    {PT_LEVELED_UP, multi_game_xp_process_leveled_up, "player leveled up", "lvl up", PD_BOTH, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009E3060
    {PT_HIT_SOMEONE, multi_game_process_player_hit_someone, "player hit someone", "hit some1", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x0094E820
    {PT_VOICE_INFO, multi_process_game_voice_info_update, "voice info", "vce info", PD_BOTH, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x00974570
    {PT_REMOVE_WEAPON, multi_game_player_process_remove_weapon, "rmv_wpn", "rmv wpn", PD_CLIENT_TO_SERVER, PCS_CONNECTED_GAMEPLAY, true, false, GID_GAMEPLAY}, // 0x009D0710
    {PT_TOOLTIP, multi_game_process_tooltip_packet, "tooltip", "tooltip", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009BBEE0
    {PT_TEXT_CHAT_MSG, multi_game_process_textchat_packet, "textchat", "chat", PD_BOTH, PCS_CONNECTED_ANY, true, false, GID_NOT_IN_ORDER}, // 0x009BB7B0
};
