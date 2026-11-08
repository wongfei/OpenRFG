// Original: volition/rfg/code/multi/multi_game/modes/multi_game_mode_bagman.h
// Implementation: volition/rfg/code/multi/multi_game/modes/multi_game_mode_bagman.cpp
//
// The Bagman mode: a bag (a flag object, picked up by touching it) that the carrier holds to score.
// Placement of bagman_flag inferred.
// Each mode implements the mode callbacks of multi_game_mode_common (enter, process, round over and winner, player died,
// respawn weights, HUD, objective text, migration, mode specific stats).
#pragma once

#include "volition/rfg/code/objects/level_object_properties_shared.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

struct hud_icon;
class human;
class item;
class multi_connection;
union multi_round_winner;
class player;
class player_start;
struct sender;
struct t_usable_object;
class vint_data_item;
struct weapon_info;

// size 0x20
struct bagman_flag
{
    bagman_flag(); // 0x0094BE50

    unsigned int bag_handle; // 0x00
    unsigned int bag_effect_handle; // 0x04
    unsigned int carrier_handle; // 0x08
    timestamp score_timer; // 0x0C
    timestamp return_timer; // 0x10
    vector base_position; // 0x14
};

void bagman_round_begin_jip(multi_connection* cp); // 0x009B93F0 (:486)
void bagman_mode_level_init_post(); // 0x009911C0 (:530)
void bagman_mode_enter(); // 0x009912A0 (:557)
void bagman_mode_process(); // 0x009E5880 (:573)
void bagman_mode_handshake(); // 0x009913A0 (:687)
void bagman_mode_remove_player(player* pp); // 0x009B97D0 (:745)
void bagman_mode_player_died(player* victim, player* killer, damaged_by_types damage_type, const weapon_info* w_info); // 0x009E6330 (:757)
float bagman_mode_adjust_respawn_weight(const player* pp, const player_start* pstart); // 0x00974950 (:820)
void bagman_mode_process_mode_specific(unsigned char* packet_data, const sender& from); // 0x009E68D0 (:845)
void bagman_mode_round_over(multi_round_winner winner); // 0x0094BEB0 (:915)
multi_round_winner bagman_mode_get_round_winner(); // 0x00991650 (:919)
void bagman_mode_hud_init(vint_data_item* item); // 0x00991660 (:927)
void bagman_mode_hud_process(vint_data_item* item); // 0x00991740 (:961)
void bagman_mode_get_objective_description(char* buffer); // 0x0094BEC0 (:1088)
bool bagman_mode_alter_player_icon(const player* pp, hud_icon* icon); // 0x0094BEF0 (:1093)
bool bagman_mode_player_set_action_object(const player* local_player, t_usable_object& action_object); // 0x009B9800 (:1104)
void bagman_mode_server_migration_post(); // 0x00974AC0 (:1148)
void bagman_mode_append_mode_specific_stats(unsigned char* packet_data, unsigned short& packet_size, unsigned int player_game_data_handle); // 0x00974B50 (:1162)
void bagman_mode_read_mode_specific_stats(unsigned char* packet_data, int& packet_offset, unsigned int player_game_data_handle); // 0x00974CA0 (:1175)
int bagman_mode_get_mode_specific_stat_count(); // 0x0094BF20 (:1188)
void bagman_mode_get_mode_specific_stat_name(char* name, const int max_name_len, int stat_index); // 0x0094BF30 (:1193)
void bagman_mode_get_mode_specific_stat_value(char* value, const int max_value_len, int stat_index, unsigned int player_game_data_handle); // 0x00974E00 (:1214)
bool item_touch_bagman_flag(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup); // 0x009E6C10 (:1256)
