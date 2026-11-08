// Original: volition/rfg/code/multi/multi_game/modes/multi_game_mode_king_of_the_hill.h
// Implementation: volition/rfg/code/multi/multi_game/modes/multi_game_mode_king_of_the_hill.cpp
//
// The King of the Hill mode: teams score by holding the hills; building damage changes them, and the
// mode has its own HUD document.
// Each mode implements the mode callbacks of multi_game_mode_common (enter, process, round over and winner, player died,
// respawn weights, HUD, objective text, migration, mode specific stats).
#pragma once

#include "volition/rfg/code/objects/level_object_properties_shared.h"

union multi_round_winner;
class player;
class player_start;
class rfg_mover;
struct sender;
struct weapon_info;

void king_of_the_hill_mode_level_init_post(); // 0x009944A0 (:429)
void king_of_the_hill_mode_enter(); // 0x00994630 (:477)
void king_of_the_hill_mode_process(); // 0x009EDB50 (:492)
void king_of_the_hill_mode_handshake(); // 0x00994710 (:637)
void king_of_the_hill_mode_player_died(player* victim, player* killer, damaged_by_types damage_type, const weapon_info* w_info); // 0x009EE1F0 (:705)
float king_of_the_hill_mode_adjust_respawn_weight(const player* pp, const player_start* pstart); // 0x00977390 (:726)
void king_of_the_hill_mode_process_mode_specific(unsigned char* packet_data, const sender& from); // 0x009EE4D0 (:759)
void king_of_the_hill_mode_round_begin(); // 0x00994AB0 (:822)
void king_of_the_hill_mode_round_over(multi_round_winner winner); // 0x0094DC30 (:833)
multi_round_winner king_of_the_hill_mode_get_round_winner(); // 0x00994B70 (:841)
void king_of_the_hill_mode_building_damage_changed(player* pp, rfg_mover* building, float structural_mass_delta); // 0x009EE600 (:857)
void king_of_the_hill_mode_server_migration_update_clients(); // 0x009BAF40 (:882)
void king_of_the_hill_mode_get_objective_description(char* buffer); // 0x0094DC70 (:911)
void king_of_the_hill_mode_hud_doc_init(int doc_handle); // 0x0094DCB0 (:921)
void king_of_the_hill_mode_hud_doc_process(int doc_handle); // 0x00977580 (:930)
bool king_of_the_hill_mode_hud_doc_should_render(int doc_handle); // 0x0094DCE0 (:981)
void king_of_the_hill_mode_hud_doc_shutdown(int doc_handle); // 0x0094DCF0 (:986)
void king_of_the_hill_mode_append_mode_specific_stats(unsigned char* packet_data, unsigned short& packet_size, unsigned int player_game_data_handle); // 0x009778E0 (:995)
void king_of_the_hill_mode_read_mode_specific_stats(unsigned char* packet_data, int& packet_offset, unsigned int player_game_data_handle); // 0x009779F0 (:1007)
int king_of_the_hill_mode_get_mode_specific_stat_count(); // 0x0094DD20 (:1019)
void king_of_the_hill_mode_get_mode_specific_stat_name(char* name, const int max_name_len, int stat_index); // 0x0094DD30 (:1024)
void king_of_the_hill_mode_get_mode_specific_stat_value(char* value, const int max_value_len, int stat_index, unsigned int player_game_data_handle); // 0x00977B20 (:1039)
