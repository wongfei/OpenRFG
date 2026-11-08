// Original: volition/rfg/code/multi/multi_game/modes/multi_game_mode_capture_the_flag.h
// Implementation: volition/rfg/code/multi/multi_game/modes/multi_game_mode_capture_the_flag.cpp
//
// The Capture the Flag mode: each team's flag is carried into the capture zones; flag carriers
// cannot swap weapons or attack freely. Placement of ctf_flag inferred.
// Each mode implements the mode callbacks of multi_game_mode_common (enter, process, round over and winner, player died,
// respawn weights, HUD, objective text, migration, mode specific stats).
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

struct hud_icon;
class item;
class multi_object_marker;
union multi_round_winner;
class player;
class player_start;
struct sender;
struct t_usable_object;
class vint_data_item;
struct weapon_info;

enum ctf_flag_states
{
    CTFFS_NONE = -1,
    CTFFS_AT_BASE = 0,
    CTFFS_PICKED_UP = 1,
    CTFFS_DROPPED = 2,
    NUM_CTF_FLAG_STATES = 3,
};

// size 0x50
struct ctf_flag
{
    ctf_flag(); // 0x0094CBD0

    human_teams team; // 0x00
    unsigned int flag_handle; // 0x04
    unsigned int flag_base_handle; // 0x08
    unsigned int carrier_handle; // 0x0C
    ctf_flag_states state; // 0x10
    timestamp dropped_time; // 0x14
    timestamp touch_timer; // 0x18
    timestamp pickup_timer; // 0x1C
    vector base_pos; // 0x20
    matrix base_orient; // 0x2C
};

void capture_the_flag_mode_level_init_post(); // 0x009920C0 (:749)
void capture_the_flag_mode_enter(); // 0x009921E0 (:787)
void capture_the_flag_mode_process(); // 0x009E7A50 (:803)
void capture_the_flag_mode_remove_player(player* pp); // 0x009B9F90 (:907)
void capture_the_flag_mode_player_died(player* victim, player* killer, damaged_by_types damage_type, const weapon_info* w_info); // 0x009E8040 (:920)
bool capture_the_flag_mode_item_touched(const player* pp, const item* itemp); // 0x00975080 (:999)
bool capture_the_flag_mode_player_can_swap_weapons(const player* pp); // 0x0094CD40 (:1017)
bool capture_the_flag_mode_player_set_action_object(const player* pp, t_usable_object& action_object); // 0x009E8AA0 (:1029)
bool capture_the_flag_mode_player_can_attack(const player* pp, const weapon_info* w_info, damaged_by_types attack_type); // 0x0094CD70 (:1074)
void capture_the_flag_mode_process_mode_specific(unsigned char* packet_data, const sender& from); // 0x009E8E60 (:1085)
multi_round_winner capture_the_flag_mode_get_round_winner(); // 0x009750F0 (:1139)
void capture_the_flag_mode_hud_init(vint_data_item* item); // 0x0094CDB0 (:1146)
void capture_the_flag_mode_hud_process(vint_data_item* item); // 0x00975130 (:1166)
void capture_the_flag_mode_get_objective_description(char* buffer); // 0x0094CE30 (:1224)
bool capture_the_flag_mode_alter_player_icon(const player* pp, hud_icon* icon); // 0x0094CE70 (:1229)
float capture_the_flag_mode_adjust_respawn_weight(const player* pp, const player_start* pstart); // 0x00975440 (:1244)
void capture_the_flag_mode_server_migration_post(); // 0x009922E0 (:1272)
void capture_the_flag_mode_player_in_flag_capture_zone(player* pp, multi_object_marker* zone); // 0x009E8FE0 (:1285)
void capture_the_flag_mode_append_mode_specific_stats(unsigned char* packet_data, unsigned short& packet_size, unsigned int player_game_data_handle); // 0x009755F0 (:1307)
void capture_the_flag_mode_read_mode_specific_stats(unsigned char* packet_data, int& packet_offset, unsigned int player_game_data_handle); // 0x009757B0 (:1321)
int capture_the_flag_mode_get_mode_specific_stat_count(); // 0x0094CEC0 (:1335)
void capture_the_flag_mode_get_mode_specific_stat_name(char* name, const int max_name_len, int stat_index); // 0x0094CED0 (:1340)
void capture_the_flag_mode_get_mode_specific_stat_value(char* value, const int max_value_len, int stat_index, unsigned int player_game_data_handle); // 0x009759A0 (:1361)
