// Original: volition/rfg/code/multi/multi_game/modes/multi_game_mode_demolition.h
// Implementation: volition/rfg/code/multi/multi_game/modes/multi_game_mode_demolition.cpp
//
// The Demolition mode: scored by building damage (demolition_mode_building_damage_changed); players have an
// action object, and a vote can be running.
// Each mode implements the mode callbacks of multi_game_mode_common (enter, process, round over and winner, player died,
// respawn weights, HUD, objective text, migration, mode specific stats).
#pragma once

#include "volition/rfg/code/objects/level_object_properties_shared.h"

struct hud_icon;
union multi_round_winner;
class player;
class player_start;
class rfg_mover;
struct sender;
struct t_usable_object;
class vint_data_item;
struct weapon_info;

void demolition_mode_enter(); // 0x009939D0 (:720)
void demolition_mode_exit(); // 0x0094D870 (:741)
void demolition_mode_process(); // 0x009EC540 (:747)
void demolition_mode_handshake(); // 0x00993B80 (:867)
void demolition_mode_remove_player(player* pp); // 0x00976760 (:946)
void demolition_mode_player_died(player* victim, player* killer, damaged_by_types damage_type, const weapon_info* w_info); // 0x009EC960 (:955)
void demolition_mode_player_respawn(player* pp, int player_start_index); // 0x0094D880 (:1012)
float demolition_mode_adjust_respawn_weight(const player* pp, const player_start* pstart); // 0x009767A0 (:1019)
void demolition_mode_building_damage_changed(player* pp, rfg_mover* building, float structural_mass_delta); // 0x0094D8A0 (:1047)
bool demolition_mode_player_set_action_object(const player* local_player, t_usable_object& action_object); // 0x009BADA0 (:1061)
void demolition_server_migration_post(); // 0x00994100 (:1105)
void demolition_mode_process_mode_specific(unsigned char* packet_data, const sender& from); // 0x009ECF20 (:1111)
void demolition_mode_round_begin(); // 0x00994110 (:1214)
void demolition_mode_round_over(multi_round_winner winner); // 0x00994130 (:1222)
multi_round_winner demolition_mode_get_round_winner(); // 0x00976930 (:1237)
void demolition_mode_hud_init(vint_data_item* item); // 0x0094D900 (:1245)
void demolition_mode_hud_process(vint_data_item* item); // 0x00976970 (:1269)
void demolition_mode_get_objective_description(char* buffer); // 0x0094D990 (:1340)
bool demolition_mode_alter_player_icon(const player* pp, hud_icon* icon); // 0x0094D9C0 (:1345)
void demolition_mode_append_mode_specific_stats(unsigned char* packet_data, unsigned short& packet_size, unsigned int player_game_data_handle); // 0x00976D80 (:1369)
void demolition_mode_read_mode_specific_stats(unsigned char* packet_data, int& packet_offset, unsigned int player_game_data_handle); // 0x00976FF0 (:1385)
int demolition_mode_get_mode_specific_stat_count(); // 0x0094DAA0 (:1401)
void demolition_mode_get_mode_specific_stat_name(char* name, const int max_name_len, int stat_index); // 0x0094DAB0 (:1406)
void demolition_mode_get_mode_specific_stat_value(char* value, const int max_value_len, int stat_index, unsigned int player_game_data_handle); // 0x009772B0 (:1429)
bool demolition_mode_is_vote_active(); // 0x0094DB60 (:1456)
