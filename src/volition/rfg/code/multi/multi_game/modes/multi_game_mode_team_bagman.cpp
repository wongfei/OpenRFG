// Original: volition/rfg/code/multi/multi_game/modes/multi_game_mode_team_bagman.cpp
//
// The Team Bagman row of the mode table: the bagman slots (multi_game_mode_bagman.cpp) with team scores; the round is
// over when a team reaches the point limit, and the better team wins.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_team_bagman.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/objects/humans/player.h"

#include <string.h>

// ---- file-static globals
// multi_team_stats Mp_team_stats[2] (0x02FE82F0) is used here too; placed in multi_game_mode_bagman.cpp.
// point_of_interest Camera_points_of_interest[2] (0x02FE8300) is used here too; placed in multi_game_mode_bagman.cpp.

// 0x009789F0 (:36): a team reached the point limit (0: none)
bool team_bagman_mode_is_round_over()
{
    if (Mp_server_options.generic_options.point_limit == 0)
    {
        return false;
    }
    // Original: the local player's info is fetched and not used.
    local_player_info info;
    player_get_local_player_info(&info);
    for (int team = HUMAN_TEAM_GUERILLA; team <= HUMAN_TEAM_EDF; team++)
    {
        if (Mp_server_options.generic_options.point_limit <= Mp_team_stats[team].round_score)
        {
            return true;
        }
    }
    return false;
}

// 0x00978A50 (:59): the team with the better round score; a tie has no winner (-1)
multi_round_winner team_bagman_mode_get_round_winner()
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

// 0x0094E340 (:67)
void team_bagman_mode_get_objective_description(char* buffer)
{
    memcpy(buffer, "HM_TEAM_BAGMAN_OBJECTIVE", sizeof("HM_TEAM_BAGMAN_OBJECTIVE"));
}
