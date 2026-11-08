// Original: volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_total_chaos.cpp
//
// wrecking.md WC1: the Total Chaos mode, where only the turn time limits the players.

#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_total_chaos.h"

#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_internal.h"
#include "volition/rfg/code/interface/ui_document_hud_wrecking_crew.h"

static int Total_chaos_time_per_turn_ms = 60000; // 0x0165192C

// 0x00761B40 (:36)
void wrecking_crew_mode_total_chaos_gameplay_process()
{
}

// 0x00761B50 (:40)
void wrecking_crew_mode_total_chaos_render(wrecking_crew_states state)
{
}

// 0x00761B60 (:46)
void wrecking_crew_mode_total_chaos_match_begin()
{
}

// 0x00761B70 (:50)
void wrecking_crew_mode_total_chaos_round_begin()
{
}

// 0x00761B80 (:54): the match winner, or -1 while nobody has the rounds to win.
int wrecking_crew_mode_total_chaos_round_end()
{
    const int winner = wrecking_crew_get_player_with_most_round_wins();
    // Original: a -1 winner (no players) reads through a null status.
    const wrecking_crew_player_status* status = (unsigned int)winner < 4 ? &Wrecking_crew_player_status[winner] : nullptr;
    if (status->round_wins < Wrecking_crew_options.rounds_to_win)
    {
        return -1;
    }
    return winner;
}

// 0x00761BB0 (:65)
int wrecking_crew_mode_total_chaos_players_turn_begin()
{
    return Total_chaos_time_per_turn_ms;
}

// 0x00761BC0 (:70)
int wrecking_crew_mode_total_chaos_players_turn_end(int time_left)
{
    return wrecking_crew_get_player_with_highest_round_score();
}

// 0x00761BD0 (:75)
hud_wrecking_crew_icon_state wrecking_crew_mode_total_chaos_get_time_icon_state()
{
    return HWCIS_NORMAL;
}
