// Original: volition/rfg/code/multi/multi_game/modes/multi_game_mode_deathmatch.cpp
//
// The Deathmatch row of the mode table: scoring on death, life-time stats, the "near the point limit" announcements,
// the leader's outline and the round winner (the best score).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_deathmatch.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game_announcer.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_render.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/render_lib/code/rl_outline_compositor_defines.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/util/bit_util.h"
#include "volition/vlib/code/util/color.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/vlib/code/util/string_util.h"

#include <float.h>
#include <math.h>
#include <string.h>

// ---- file-static globals
// multi_gameplay_mode Multi_gameplay_mode (0x0167DEE0) is used here too; placed in multi_gameplay_mode.cpp.
// human_voice_persona_handle Multi_general_announcer (0x0167DEE4) is used here too; placed in multi_game_mode_bagman.cpp.
// multi_announcer_line_info Multi_announcer_lines[52] (0x01691F00) is used here too; placed in multi_game_mode_capture_the_flag.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.
// multi_player_stats* Mp_player_stats (0x02FEB720) is used here too; placed in multi_game_mode_bagman.cpp.
// multi_xp_event Multi_xp_events[84] (0x0300D240) is used here too; placed in multi_game_mode_bagman.cpp.

// The player stats of a game data handle. Original (every user here): index 16 is accepted although Mp_player_stats
// has 16 entries.
static multi_player_stats* deathmatch_stats_of(unsigned int handle)
{
    if (handle == 0xFFFFFFFFu || (handle & 0xFFFF) > 16)
    {
        return nullptr;
    }
    return &Mp_player_stats[handle & 0xFFFF];
}

// 0x00993320 (:44): the life-time stats start over
void deathmatch_mode_enter()
{
    for (unsigned int handle = multi_game_data_handles_begin(ST_GAME_SNAPSHOT, true, false); handle != 0xFFFFFFFFu;
         handle = multi_game_data_handles_next(handle, true, false))
    {
        multi_player_stats* stats = deathmatch_stats_of(handle);
        if (stats != nullptr)
        {
            stats->a_total_time_alive = 0.0f;
            stats->a_time_alive = 0.0f;
            stats->a_shortest_life = FLT_MAX;
            stats->a_longest_life = 0.0f;
            stats->a_lives = 0;
        }
    }
}

// 0x00993450 (:60): "near the point limit" (10 points or fewer to go, with a limit over 10) and the time alive
void deathmatch_mode_process()
{
    player* local_player = player_get_local_player();
    const unsigned short point_limit = Mp_server_options.generic_options.point_limit;
    if (local_player != nullptr && point_limit != 0)
    {
        const int count = World.get_num_objects<player>();
        for (int i = 0; i < count; i++)
        {
            player* pp = World.get_object<player>(i);
            const multi_player_stats* stats = deathmatch_stats_of(pp->mp_data);
            if (stats == nullptr || point_limit <= 10 || (int)(point_limit - stats->round_score) >= 11)
            {
                continue;
            }
            // MAL_DM_ENEMY_WIN_NEAR follows MAL_DM_ALLY_WIN_NEAR. Lines past MAL_SGE_ENEMY_TARGET_WIN_NEAR have no play
            // record.
            const multi_announcer_lines line = (multi_announcer_lines)(MAL_DM_ALLY_WIN_NEAR + (pp != local_player ? 1 : 0));
            if (line > MAL_SGE_ENEMY_TARGET_WIN_NEAR || !Multi_announcer_lines[line].last_played_time.valid())
            {
                multi_game_announcer_play_line(line, 0xFFFFFFFF);
            }
        }
    }
    const int count = World.get_num_objects<player>();
    for (int i = 0; i < count; i++)
    {
        player* pp = World.get_object<player>(i);
        multi_player_stats* stats = deathmatch_stats_of(pp->mp_data);
        if (stats != nullptr && !pp->is_killed())
        {
            stats->a_total_time_alive += Frametime_real;
            stats->a_time_alive += Frametime_real;
        }
    }
}

// 0x009DB930 (:96): with leader highlighting, the round leader gets a pulsing outline in its team color (unless it is
// stealthed)
void deathmatch_mode_handshake()
{
    if (!Mp_server_options.mode_options.deathmatch.leader_highlighting)
    {
        return;
    }
    const int count = World.get_num_objects<player>();
    for (int i = 0; i < count; i++)
    {
        World.get_object<player>(i)->disable_object_outline();
    }
    if (player_get_local_player() == nullptr)
    {
        return;
    }
    const multi_game_data* data = multi_game_data_get(multi_game_stats_get_round_winning_player(false, false), false, false);
    if (data == nullptr)
    {
        return;
    }
    const unsigned int leader_handle = multi_core_game_handle_from_network_handle(data->player_network_handle, true);
    if (leader_handle == 0xFFFFFFFFu)
    {
        return;
    }
    object* objp = World.object_from_handle(leader_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        return;
    }
    player* leader = (player*)objp;
    if (leader->backpack != nullptr && leader->backpack->is_fully_stealthed())
    {
        return;
    }
    rl_scene* scene = game_render_get_main_scene();
    if (scene == nullptr)
    {
        return;
    }
    leader->enable_object_outline(0);
    rl_outline_state state;
    state.colors[1].set(0.0f, 0.0f, 0.0f, 1.0f);
    color team_color = color::WHITE;
    multi_game_render_get_team_color(team_color, MTCT_OUTLINE, leader);
    // A 0.5..1.5 pulse with a period of 1000 * pi ms.
    const float pulse = sinf((float)Frametime_total_milliseconds / 500.0f) * 0.5f + 1.0f;
    state.colors[0].set(team_color.red / 255.0f * pulse, team_color.green / 255.0f * pulse, team_color.blue / 255.0f * pulse, 1.0f * pulse);
    state.layer[0] = RLOL_USE_COLOR0;
    state.layer[1] = RLOL_DISABLED;
    state.layer[2] = RLOL_DISABLED;
    state.layer[3] = RLOL_DISABLED;
    scene->set_outline_state(state);
}

// 0x009EBBE0 (:150): a kill of an enemy is a point for the killer (for its team in team modes); a suicide or a kill of
// an ally costs the victim a point (team modes: the other team gets it). The victim's life ends: its longest and
// shortest life. Every peer scores the kills it learns of; the endgame sync makes the server's count final. The kill
// gives the killer's team the deathmatch kill experience (team modes) or the killer the solo one. Original: the killer
// is not checked for null.
void deathmatch_mode_player_died(player* victim, player* killer, damaged_by_types damage_type, const weapon_info* w_info)
{
    const int weapon_id = w_info != nullptr ? w_info->unique_id : 0xFF;
    const int backpack_id = killer->backpack != nullptr ? killer->backpack->get_backpack_info()->unique_id : -1;
    const bool team_mode = Multiplayer && Multi_gameplay_mode >= 0 && Multi_gameplay_mode < 8 && Multi_gameplay_mode_funcs[Multi_gameplay_mode].is_team_mode;
    if (victim != killer && human_teams_get_disposition(victim, killer) == TEAM_DISPOSITION_HOSTILE)
    {
        if (team_mode)
        {
            multi_game_xp_trigger_event(killer->current_team, MXE_DEATHMATCH_KILL, 1, false); // inlined
            multi_game_stats_alter_team_score(killer->current_team, 1);
        }
        else
        {
            multi_game_xp_trigger_event(killer->mp_data, MXE_DEATHMATCH_KILL_SOLO, 1, weapon_id, backpack_id, false); // inlined
            multi_game_stats_alter_player_score(killer->mp_data, 1);
        }
    }
    else if (!team_mode)
    {
        multi_game_stats_alter_player_score(victim->mp_data, -1);
    }
    else if (killer->current_team == HUMAN_TEAM_GUERILLA)
    {
        multi_game_stats_alter_team_score(HUMAN_TEAM_EDF, 1);
    }
    else if (killer->current_team == HUMAN_TEAM_EDF)
    {
        multi_game_stats_alter_team_score(HUMAN_TEAM_GUERILLA, 1);
    }
    multi_player_stats* stats = deathmatch_stats_of(victim->mp_data);
    if (stats != nullptr)
    {
        if (stats->a_longest_life < stats->a_time_alive)
        {
            stats->a_longest_life = stats->a_time_alive;
        }
        if (stats->a_time_alive < stats->a_shortest_life)
        {
            stats->a_shortest_life = stats->a_time_alive;
        }
    }
}

// 0x00976130 (:203)
void deathmatch_mode_player_respawn(player* pp, int player_start_index)
{
    multi_player_stats* stats = deathmatch_stats_of(pp->mp_data);
    if (stats != nullptr)
    {
        stats->a_lives++;
        stats->a_time_alive = 0.0f;
    }
}

// 0x00993610 (:213): the lives in progress count for the longest life, and for the shortest when nobody died
void deathmatch_mode_round_over(multi_round_winner winner)
{
    for (unsigned int handle = multi_game_data_handles_begin(ST_GAME_SNAPSHOT, true, false); handle != 0xFFFFFFFFu;
         handle = multi_game_data_handles_next(handle, true, false))
    {
        multi_player_stats* stats = deathmatch_stats_of(handle);
        if (stats != nullptr)
        {
            if (stats->a_longest_life < stats->a_time_alive)
            {
                stats->a_longest_life = stats->a_time_alive;
            }
            if (stats->a_shortest_life >= FLT_MAX)
            {
                stats->a_shortest_life = stats->a_time_alive;
            }
        }
    }
}

// 0x00993740 (:230)
multi_round_winner deathmatch_mode_get_round_winner()
{
    multi_round_winner winner;
    winner.player_game_data_handle = multi_game_stats_get_round_winning_player(false, false);
    return winner;
}

// 0x0094D460 (:238)
void deathmatch_mode_get_objective_description(char* buffer)
{
    memcpy(buffer, "HM_DEATHMATCH_OBJECTIVE", sizeof("HM_DEATHMATCH_OBJECTIVE"));
}

// 0x00976170 (:243): server: lives (BE), longest, shortest and total life (PACKET_SWAP_FLOAT)
void deathmatch_mode_append_mode_specific_stats(unsigned char* packet_data, unsigned short& packet_size, unsigned int player_game_data_handle)
{
    const multi_player_stats* stats = deathmatch_stats_of(player_game_data_handle);
    if (stats == nullptr || !Multiplayer || !Server)
    {
        return;
    }
    const unsigned int lives = (unsigned int)stats->a_lives;
    packet_data[packet_size] = (unsigned char)(lives >> 24);
    packet_data[packet_size + 1] = (unsigned char)(lives >> 16);
    packet_data[packet_size + 2] = (unsigned char)(lives >> 8);
    packet_data[packet_size + 3] = (unsigned char)lives;
    packet_size += 4;
    const float values[3] = {stats->a_longest_life, stats->a_shortest_life, stats->a_total_time_alive};
    for (int i = 0; i < 3; i++)
    {
        const float swapped = PACKET_SWAP_FLOAT(values[i]);
        memcpy(packet_data + packet_size, &swapped, 4);
        packet_size += 4;
    }
}

// 0x00976300 (:257): client
void deathmatch_mode_read_mode_specific_stats(unsigned char* packet_data, int& packet_offset, unsigned int player_game_data_handle)
{
    multi_player_stats* stats = deathmatch_stats_of(player_game_data_handle);
    if (stats == nullptr || !Multiplayer || Server)
    {
        return;
    }
    const unsigned char* p = packet_data + packet_offset;
    stats->a_lives = (int)((unsigned int)p[0] << 24 | (unsigned int)p[1] << 16 | (unsigned int)p[2] << 8 | p[3]);
    packet_offset += 4;
    float* values[3] = {&stats->a_longest_life, &stats->a_shortest_life, &stats->a_total_time_alive};
    for (int i = 0; i < 3; i++)
    {
        memcpy(values[i], packet_data + packet_offset, 4);
        *values[i] = PACKET_SWAP_FLOAT(*values[i]);
        packet_offset += 4;
    }
}

// 0x0094D490 (:271): the highest kill streak, the average life, the highest death streak, the shortest life
int deathmatch_mode_get_mode_specific_stat_count()
{
    return 4;
}

// 0x0094D4A0 (:276): the results screen's column names (string ids)
void deathmatch_mode_get_mode_specific_stat_name(char* name, const int max_name_len, int stat_index)
{
    const char* id;
    switch (stat_index)
    {
    case 0:
        id = "MENU_HIGHEST_KILL_STREAK";
        break;
    case 1:
        id = "MENU_AVERAGE_LIFETIME";
        break;
    case 2:
        id = "MENU_HIGHEST_DEATH_STREAK";
        break;
    case 3:
        id = "MENU_SHORTEST_LIFETIME";
        break;
    default:
        id = "";
        break;
    }
    _snprintf_safe(name, max_name_len, "%s", id);
}

// 0x009764B0 (:297): the column values (times as minutes and seconds). Original: no null check of the stats (an
// invalid handle with a stat index of 0 to 3 reads through null).
void deathmatch_mode_get_mode_specific_stat_value(char* value, const int max_value_len, int stat_index, unsigned int player_game_data_handle)
{
    const multi_player_stats* stats = deathmatch_stats_of(player_game_data_handle);
    switch (stat_index)
    {
    case 0:
        _snprintf_safe(value, max_value_len, "%d", stats->most_kills_in_a_row);
        return;
    case 1:
        if (stats->a_lives == 0)
        {
            hud_ui_convert_time_for_output(value, max_value_len, 0);
            return;
        }
        hud_ui_convert_time_for_output(value, max_value_len, stats->a_total_time_alive / (float)stats->a_lives);
        return;
    case 2:
        _snprintf_safe(value, max_value_len, "%d", stats->most_deaths_in_a_row);
        return;
    case 3:
        hud_ui_convert_time_for_output(value, max_value_len, stats->a_shortest_life);
        return;
    default:
        _snprintf_safe(value, max_value_len, "%s", "");
        return;
    }
}
