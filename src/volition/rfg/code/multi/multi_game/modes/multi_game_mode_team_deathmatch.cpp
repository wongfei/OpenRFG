// Original: volition/rfg/code/multi/multi_game/modes/multi_game_mode_team_deathmatch.cpp
//
// The Team Deathmatch row of the mode table: the deathmatch slots with team scores (multi_game_mode_deathmatch.cpp),
// the "near the point limit" announcements per team and the round winner (the better team).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_team_deathmatch.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game_announcer.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/vlib/code/os/frametime.h"

#include <string.h>

// ---- file-static globals
// multi_announcer_line_info Multi_announcer_lines[52] (0x01691F00) is used here too; placed in multi_game_mode_capture_the_flag.cpp.
// multi_team_stats Mp_team_stats[2] (0x02FE82F0) is used here too; placed in multi_game_mode_bagman.cpp.
// point_of_interest Camera_points_of_interest[2] (0x02FE8300) is used here too; placed in multi_game_mode_bagman.cpp.
// multi_player_stats* Mp_player_stats (0x02FEB720) is used here too; placed in multi_game_mode_bagman.cpp.

// 0x00995310 (:28): "near the point limit" for each team (10 points or fewer to go, with a limit over 10), and the
// time alive
void team_deathmatch_mode_process()
{
    player* local_player = player_get_local_player();
    const unsigned short point_limit = Mp_server_options.generic_options.point_limit;
    if (local_player != nullptr && point_limit != 0)
    {
        for (int team = HUMAN_TEAM_GUERILLA; team <= HUMAN_TEAM_EDF; team++)
        {
            const multi_team_stats* stats = &Mp_team_stats[team];
            if (point_limit <= 10 || (int)(point_limit - stats->round_score) >= 11)
            {
                continue;
            }
            // The enemy line follows the ally line; lines past MAL_SGE_ENEMY_TARGET_WIN_NEAR have no play record.
            const bool enemy = human_teams_get_disposition(local_player, (human_teams)team) < TEAM_DISPOSITION_FRIENDLY;
            const multi_announcer_lines line = (multi_announcer_lines)(MAL_DM_ALLY_WIN_NEAR + (enemy ? 1 : 0));
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
        // Original: index 16 is accepted although Mp_player_stats has 16 entries.
        if (pp->mp_data == 0xFFFFFFFFu || (pp->mp_data & 0xFFFF) > 16)
        {
            continue;
        }
        multi_player_stats* stats = &Mp_player_stats[pp->mp_data & 0xFFFF];
        if (!pp->is_killed())
        {
            stats->a_total_time_alive += Frametime_real;
            stats->a_time_alive += Frametime_real;
        }
    }
}

// 0x00978A90 (:58): the team with the better round score; a tie has no winner (-1)
multi_round_winner team_deathmatch_mode_get_round_winner()
{
    multi_round_winner winner;
    if (Mp_team_stats[HUMAN_TEAM_EDF].round_score > Mp_team_stats[HUMAN_TEAM_GUERILLA].round_score)
    {
        winner.team = HUMAN_TEAM_EDF;
    }
    else if (Mp_team_stats[HUMAN_TEAM_EDF].round_score == Mp_team_stats[HUMAN_TEAM_GUERILLA].round_score)
    {
        winner.player_game_data_handle = 0xFFFFFFFFu;
    }
    else
    {
        winner.team = HUMAN_TEAM_GUERILLA;
    }
    return winner;
}

// 0x0094E380 (:66)
void team_deathmatch_mode_get_objective_description(char* buffer)
{
    memcpy(buffer, "HM_TEAM_DEATHMATCH_OBJECTIVE", sizeof("HM_TEAM_DEATHMATCH_OBJECTIVE"));
}
