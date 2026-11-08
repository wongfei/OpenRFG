// Original: volition/rfg/code/multi/multi_game/modes/multi_game_mode_team_bagman.h
// Implementation: volition/rfg/code/multi/multi_game/modes/multi_game_mode_team_bagman.cpp
//
// The Team Bagman mode: Bagman with teams (its own round end, winner and objective text).
// Each mode implements the mode callbacks of multi_game_mode_common (enter, process, round over and winner, player died,
// respawn weights, HUD, objective text, migration, mode specific stats).
#pragma once


union multi_round_winner;

bool team_bagman_mode_is_round_over(); // 0x009789F0 (:36)
multi_round_winner team_bagman_mode_get_round_winner(); // 0x00978A50 (:59)
void team_bagman_mode_get_objective_description(char* buffer); // 0x0094E340 (:67)
