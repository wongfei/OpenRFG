// Original: volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_fall_guy.h
// Implementation: volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_fall_guy.cpp
//
// The Fall Guy mode: scored by the movers destroyed during the turn, with a barrel count shown on the HUD; it tracks the
// map restore.
#pragma once

#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_enums.h"

struct hud_wrecking_crew_barrel_text_set;
enum hud_wrecking_crew_icon_state;
struct hud_wrecking_crew_text_set;
class object_mover;

void wrecking_crew_mode_fall_guy_gameplay_process(); // 0x00777BC0 (:173)
void wrecking_crew_mode_fall_guy_render(wrecking_crew_states state); // 0x00761520 (:198)
void wrecking_crew_mode_fall_guy_match_begin(); // 0x00761530 (:204)
void wrecking_crew_mode_fall_guy_round_begin(); // 0x00790F80 (:208)
int wrecking_crew_mode_fall_guy_round_end(); // 0x00761540 (:278)
int wrecking_crew_mode_fall_guy_players_turn_begin(); // 0x00761570 (:289)
int wrecking_crew_mode_fall_guy_players_turn_end(int time_left); // 0x00761580 (:294)
bool wrecking_crew_mode_fall_guy_is_players_turn_over(); // 0x00761590 (:299)
float wrecking_crew_mode_fall_guy_get_current_players_score(int time_left); // 0x00777C60 (:312)
void wrecking_crew_mode_fall_guy_map_is_being_restored(); // 0x00777C80 (:317)
void wrecking_crew_mode_fall_guy_mover_destroyed(const object_mover* mover); // 0x00777CB0 (:324)
void wrecking_crew_mode_fall_guy_get_score_text(hud_wrecking_crew_text_set* score_set, hud_wrecking_crew_text_set* high_score_set, bool* green_score); // 0x007615B0 (:354)
void wrecking_crew_mode_fall_guy_get_turn_end_score_text(hud_wrecking_crew_text_set* score_set, hud_wrecking_crew_text_set* high_score_set, bool* green_score); // 0x00761650 (:368)
void wrecking_crew_mode_fall_guy_get_barrel_text(hud_wrecking_crew_barrel_text_set* barrel_set); // 0x00761670 (:376)
hud_wrecking_crew_icon_state wrecking_crew_mode_fall_guy_get_time_icon_state(); // 0x00761840 (:420)
