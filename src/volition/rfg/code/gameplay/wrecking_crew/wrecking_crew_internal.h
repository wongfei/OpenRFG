// Original: volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_internal.h
// Implementation: volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_internal.cpp
//
// Score helpers shared by the Wrecking Crew modes: score conversion, the total round score and the round winners.
//
// wrecking.md WC1: the match state the mode files share. In rfg.exe these are file statics of wrecking_crew.cpp that the
// mode files see through the unity build (rfg.pdb gives them no global linkage); here they are defined once in
// wrecking_crew.cpp and declared below, so the split files read the same copy.
#pragma once

#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"

extern wrecking_crew_mode_info Wrecking_crew_mode_info[5]; // 0x016515B8
extern wrecking_crew_states Wrecking_crew_state; // 0x01651888
extern wrecking_crew_player_status Wrecking_crew_player_status[4]; // 0x022278B8
extern const wrecking_crew_weapon_info* Wrecking_crew_current_weapon; // 0x02229694
extern const wrecking_crew_backpack_info* Wrecking_crew_current_backpack; // 0x0222969C
extern int Wrecking_crew_turn_time_left; // 0x022296A4

int wrecking_crew_convert_score(float score); // 0x00761040 (:43)
float wrecking_crew_unconvert_score(int score); // 0x00761060 (:48)
float wrecking_crew_get_total_round_score(); // 0x00761080 (:53)
int wrecking_crew_get_last_man_standing(); // 0x007611A0 (:64)
int wrecking_crew_get_player_with_most_round_wins(); // 0x007611E0 (:82)
bool wrecking_crew_is_players_turn(int player_index); // 0x00761230 (:97)
int wrecking_crew_get_player_with_highest_round_score(); // 0x00761260 (:102)
