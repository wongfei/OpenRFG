// Original: volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_rampage.h
// Implementation: volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_rampage.cpp
//
// The Rampage mode: attacks and backpack activations are counted per turn.
#pragma once

#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_enums.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"

struct weapon_info;

void wrecking_crew_mode_rampage_gameplay_process(); // 0x00761970 (:37)
void wrecking_crew_mode_rampage_render(wrecking_crew_states state); // 0x00761980 (:41)
void wrecking_crew_mode_rampage_match_begin(); // 0x00761990 (:47)
void wrecking_crew_mode_rampage_round_begin(); // 0x007619A0 (:51)
int wrecking_crew_mode_rampage_round_end(); // 0x007619B0 (:55)
int wrecking_crew_mode_rampage_players_turn_begin(); // 0x007619E0 (:66)
int wrecking_crew_mode_rampage_players_turn_end(int time_left); // 0x007619F0 (:71)
bool wrecking_crew_mode_rampage_can_player_attack(const weapon_info* w_info, damaged_by_types attack_type); // 0x007914D0 (:76)
void wrecking_crew_mode_rampage_player_attacked(const weapon_info* w_info, damaged_by_types attack_type); // 0x00761A00 (:93)
void wrecking_crew_mode_rampage_player_activated_backpack(); // 0x00761A50 (:108)
void wrecking_crew_mode_rampage_get_weapon_text(wchar_t* buffer, int buffer_len, bool* override_icon, char* icon, int icon_buffer_len); // 0x00761AA0 (:123)
void wrecking_crew_mode_rampage_get_backpack_text(wchar_t* buffer, int buffer_len, bool* override_icon, char* icon, int icon_buffer_len); // 0x00761AE0 (:133)
