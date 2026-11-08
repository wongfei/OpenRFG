// Original: volition/rfg/code/multi/multi_game/modes/multi_game_mode_deathmatch.h
// Implementation: volition/rfg/code/multi/multi_game/modes/multi_game_mode_deathmatch.cpp
//
// The Deathmatch mode: every player for themself; the best score wins the round.
// Each mode implements the mode callbacks of multi_game_mode_common (enter, process, round over and winner, player died,
// respawn weights, HUD, objective text, migration, mode specific stats).
#pragma once

#include "volition/rfg/code/objects/level_object_properties_shared.h"

union multi_round_winner;
class player;
struct weapon_info;

void deathmatch_mode_enter(); // 0x00993320 (:44)
void deathmatch_mode_process(); // 0x00993450 (:60)
void deathmatch_mode_handshake(); // 0x009DB930 (:96)
void deathmatch_mode_player_died(player* victim, player* killer, damaged_by_types damage_type, const weapon_info* w_info); // 0x009EBBE0 (:150)
void deathmatch_mode_player_respawn(player* pp, int player_start_index); // 0x00976130 (:203)
void deathmatch_mode_round_over(multi_round_winner winner); // 0x00993610 (:213)
multi_round_winner deathmatch_mode_get_round_winner(); // 0x00993740 (:230)
void deathmatch_mode_get_objective_description(char* buffer); // 0x0094D460 (:238)
void deathmatch_mode_append_mode_specific_stats(unsigned char* packet_data, unsigned short& packet_size, unsigned int player_game_data_handle); // 0x00976170 (:243)
void deathmatch_mode_read_mode_specific_stats(unsigned char* packet_data, int& packet_offset, unsigned int player_game_data_handle); // 0x00976300 (:257)
int deathmatch_mode_get_mode_specific_stat_count(); // 0x0094D490 (:271)
void deathmatch_mode_get_mode_specific_stat_name(char* name, const int max_name_len, int stat_index); // 0x0094D4A0 (:276)
void deathmatch_mode_get_mode_specific_stat_value(char* value, const int max_value_len, int stat_index, unsigned int player_game_data_handle); // 0x009764B0 (:297)
