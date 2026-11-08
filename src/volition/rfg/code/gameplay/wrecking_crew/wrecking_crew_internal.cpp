// Original: volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_internal.cpp
//
// wrecking.md WC1: score helpers of the Wrecking Crew modes.

#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_internal.h"

#include <float.h>
#include <limits.h>

// Status of a player index, or null outside 0..3 (rfg.exe inlines this bound check, wrecking_crew_get_player_status).
static wrecking_crew_player_status* wc_status(int player_index)
{
    return (unsigned int)player_index < 4 ? &Wrecking_crew_player_status[player_index] : nullptr;
}

// 0x00761040 (:43)
int wrecking_crew_convert_score(float score)
{
    // Magic numbers: the displayed score is the internal one times 50, in steps of 5.
    return (int)(score * 10.0) * 5;
}

// 0x00761060 (:48)
float wrecking_crew_unconvert_score(int score)
{
    return (float)(score / 50);
}

// 0x00761080 (:53)
float wrecking_crew_get_total_round_score()
{
    float total = 0.0f;
    for (int i = 0; i < Wrecking_crew_options.num_players; i++)
    {
        total = wc_status(i)->round_score + total;
    }
    return total;
}

// 0x007611A0 (:64): the only enabled player, or -1 while two or more remain (or none).
int wrecking_crew_get_last_man_standing()
{
    int last = -1;
    for (int i = 0; i < Wrecking_crew_options.num_players; i++)
    {
        if (wc_status(i)->enabled)
        {
            if (last != -1)
            {
                return last;
            }
            last = i;
        }
    }
    return last;
}

// 0x007611E0 (:82)
int wrecking_crew_get_player_with_most_round_wins()
{
    int best = -1;
    int best_wins = -INT_MAX;
    for (int i = 0; i < Wrecking_crew_options.num_players; i++)
    {
        const int wins = wc_status(i)->round_wins;
        if (best_wins < wins)
        {
            best = i;
            best_wins = wins;
        }
    }
    return best;
}

// 0x00761230 (:97)
bool wrecking_crew_is_players_turn(int player_index)
{
    return (Wrecking_crew_state == WCS_PRE_TURN || Wrecking_crew_state == WCS_PLAYERS_TURN || Wrecking_crew_state == WCS_PRE_BREAK) && player_index == Wrecking_crew_current_player;
}

// 0x00761260 (:102): the best round score, walking the players from the one who started the last round; -1 until every
// player had a turn this round.
int wrecking_crew_get_player_with_highest_round_score()
{
    float best_score = -FLT_MAX;
    int best = -1;
    for (int i = 0; i < Wrecking_crew_options.num_players; i++)
    {
        int player_index = i;
        if (Wrecking_crew_last_first_player != -1)
        {
            player_index = Wrecking_crew_last_first_player + i;
            if (Wrecking_crew_options.num_players <= player_index)
            {
                player_index = i + (Wrecking_crew_last_first_player - Wrecking_crew_options.num_players);
            }
        }
        const wrecking_crew_player_status* status = wc_status(player_index);
        if (!status->had_a_turn_for_current_round)
        {
            return -1;
        }
        if (best_score < status->round_score)
        {
            best = player_index;
            best_score = status->round_score;
        }
    }
    return best;
}
