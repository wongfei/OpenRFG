// Original: volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_mayhem.h
// Implementation: volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_mayhem.cpp
//
// The Mayhem mode: attacks and weapon ammo are limited per turn (the HUD shows the remaining ammo).
#pragma once

#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_enums.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"

enum hud_wrecking_crew_icon_state;
struct weapon_info;

void wrecking_crew_mode_mayhem_gameplay_process(); // 0x007618A0 (:70)
void wrecking_crew_mode_mayhem_render(wrecking_crew_states state); // 0x007618B0 (:75)
void wrecking_crew_mode_mayhem_match_begin(); // 0x007618C0 (:81)
void wrecking_crew_mode_mayhem_round_begin(); // 0x007618D0 (:85)
int wrecking_crew_mode_mayhem_round_end(); // 0x007618E0 (:89)
int wrecking_crew_mode_mayhem_players_turn_begin(); // 0x00761910 (:100)
int wrecking_crew_mode_mayhem_players_turn_end(int time_left); // 0x00761930 (:107)
bool wrecking_crew_mode_mayhem_is_players_turn_over(); // 0x00791210 (:112)
bool wrecking_crew_mode_mayhem_can_player_attack(const weapon_info* w_info, damaged_by_types attack_type); // 0x00791280 (:117)
void wrecking_crew_mode_mayhem_player_attacked(const weapon_info* w_info, damaged_by_types attack_type); // 0x00761940 (:122)
void wrecking_crew_mode_mayhem_get_weapon_ammo(int* magazine, int* reserve); // 0x007912F0 (:129)
void wrecking_crew_mode_mayhem_get_weapon_text(wchar_t* buffer, int buffer_len, bool* override_icon, char* icon, int icon_buffer_len); // 0x00791360 (:146)
hud_wrecking_crew_icon_state wrecking_crew_mode_mayhem_get_weapon_icon_state(); // 0x00791410 (:154)
hud_wrecking_crew_icon_state wrecking_crew_mode_mayhem_get_time_icon_state(); // 0x00761960 (:171)
