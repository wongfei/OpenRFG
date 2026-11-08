// Original: volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_walker.h
// Implementation: volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_walker.cpp
//
// The walker mode: the players destroy the map with a walker; start positions the walker cannot use are disabled.
// Placement of disabled_start_pos inferred.
#pragma once

#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_enums.h"

enum hud_wrecking_crew_icon_state;
struct hud_wrecking_crew_text_set;

// size 0x8
struct disabled_start_pos
{
    int map_index; // 0x00
    int start_pos_index; // 0x04
};

int wrecking_crew_get_current_walker_vehicle_type(); // 0x00761C30 (:284)
const char* wrecking_crew_get_walker_display_name(wrecking_crew_walker_choices choice); // 0x00761C40 (:289)
void wrecking_crew_next_walker(); // 0x00761C70 (:298)
void wrecking_crew_assign_walker(); // 0x007916C0 (:309)
void wrecking_crew_create_walker(); // 0x00791720 (:324)
bool wrecking_crew_walker_loaded(); // 0x00791960 (:363)
void wrecking_crew_mode_walker_gameplay_process(); // 0x0079F6C0 (:378)
void wrecking_crew_mode_walker_render(wrecking_crew_states state); // 0x00761CD0 (:385)
void wrecking_crew_mode_walker_match_begin(); // 0x00761CE0 (:391)
void wrecking_crew_mode_walker_match_end(); // 0x00761CF0 (:396)
void wrecking_crew_mode_walker_round_begin(); // 0x0079F6D0 (:400)
int wrecking_crew_mode_walker_round_end(); // 0x00761D00 (:406)
int wrecking_crew_mode_walker_players_turn_begin(); // 0x0079F6E0 (:417)
int wrecking_crew_mode_walker_players_turn_end(int time_left); // 0x00761D30 (:429)
void wrecking_crew_mode_walker_player_respawned(); // 0x0079F730 (:434)
void wrecking_crew_mode_walker_get_weapon_text(wchar_t* buffer, int buffer_len, bool* override_icon, char* icon, int icon_buffer_len); // 0x00761D40 (:439)
void wrecking_crew_mode_walker_get_backpack_text(wchar_t* buffer, int buffer_len, bool* override_icon, char* icon, int icon_buffer_len); // 0x00761DD0 (:461)
bool wrecking_crew_mode_walker_get_veto_text(hud_wrecking_crew_text_set* weapon_set, bool* can_veto_weapon, hud_wrecking_crew_text_set* backpack_set, bool* can_veto_backpack); // 0x00761DF0 (:468)
hud_wrecking_crew_icon_state wrecking_crew_mode_walker_get_time_icon_state(); // 0x00761EB0 (:507)
void wrecking_crew_walker_clear_start_positions(); // 0x00777D90 (:512)
void wrecking_crew_walker_visible(bool visible); // 0x007919A0 (:517)
void wrecking_crew_mode_walker_disable_start_pos(int map_index, int start_pos_index); // 0x00791A00 (:530)
