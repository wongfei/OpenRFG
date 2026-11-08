// Original: volition/rfg/code/multi/multi_game/modes/multi_game_mode_team_deathmatch.h
// Implementation: volition/rfg/code/multi/multi_game/modes/multi_game_mode_team_deathmatch.cpp
//
// The Team Deathmatch mode: the team with the best score wins the round.
// Each mode implements the mode callbacks of multi_game_mode_common (enter, process, round over and winner, player died,
// respawn weights, HUD, objective text, migration, mode specific stats).
#pragma once


union multi_round_winner;

void team_deathmatch_mode_process(); // 0x00995310 (:28)
multi_round_winner team_deathmatch_mode_get_round_winner(); // 0x00978A90 (:58)
void team_deathmatch_mode_get_objective_description(char* buffer); // 0x0094E380 (:66)
