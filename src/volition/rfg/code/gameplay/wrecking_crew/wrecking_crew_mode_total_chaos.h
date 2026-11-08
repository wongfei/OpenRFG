// Original: volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_total_chaos.h
// Implementation: volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_total_chaos.cpp
//
// The Total Chaos mode: only the turn time limits the players.
#pragma once

#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_enums.h"

enum hud_wrecking_crew_icon_state;

void wrecking_crew_mode_total_chaos_gameplay_process(); // 0x00761B40 (:36)
void wrecking_crew_mode_total_chaos_render(wrecking_crew_states state); // 0x00761B50 (:40)
void wrecking_crew_mode_total_chaos_match_begin(); // 0x00761B60 (:46)
void wrecking_crew_mode_total_chaos_round_begin(); // 0x00761B70 (:50)
int wrecking_crew_mode_total_chaos_round_end(); // 0x00761B80 (:54)
int wrecking_crew_mode_total_chaos_players_turn_begin(); // 0x00761BB0 (:65)
int wrecking_crew_mode_total_chaos_players_turn_end(int time_left); // 0x00761BC0 (:70)
hud_wrecking_crew_icon_state wrecking_crew_mode_total_chaos_get_time_icon_state(); // 0x00761BD0 (:75)
