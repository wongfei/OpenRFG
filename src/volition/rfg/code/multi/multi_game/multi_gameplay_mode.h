// Original: volition/rfg/code/multi/multi_game/multi_gameplay_mode.h
// Implementation: volition/rfg/code/multi/multi_game/multi_gameplay_mode.cpp
//
// The multiplayer gameplay modes (multi_gameplay_mode) and their names.
#pragma once

#include "volition/rfg/code/multi/multi_game/multi_game_announcer.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"

struct hud_icon;
class item;
class mempool_base;
class multi_connection;
struct multi_game_equipment_loadout;
union multi_round_winner;
class player;
class player_start;
class rfg_mover;
struct sender;
struct t_usable_object;
class vint_data_item;
struct weapon_info;

enum multi_gameplay_mode
{
    MGPM_NONE = -1,
    MGPM_DEATHMATCH = 0,
    MGPM_TEAM_DEATHMATCH = 1,
    MGPM_CAPTURE_THE_FLAG = 2,
    MGPM_SIEGE = 3,
    MGPM_KING_OF_THE_HILL = 4,
    MGPM_BAGMAN = 5,
    MGPM_TEAM_BAGMAN = 6,
    MGPM_DEMOLITION = 7,
    MGPM_NUM_MODES = 8,
};

void multi_gameplay_mode_round_begin_jip(multi_connection* cp); // 0x0094E9A0 (:601)
void multi_gameplay_mode_level_init_pre(); // 0x00978F50 (:614)
void multi_gameplay_mode_level_init_post(); // 0x009ABA90 (:630)
void multi_gameplay_mode_level_shutdown(); // 0x009F0020 (:652)
void multi_gameplay_mode_process(); // 0x009FE5B0 (:678)
void multi_gameplay_mode_handshake(); // 0x0094E9C0 (:695)
void multi_gameplay_mode_render(); // 0x00995A30 (:712)
void multi_gameplay_mode_add_player(player* pp); // 0x0094E9E0 (:731)
void multi_gameplay_mode_remove_player(player* pp); // 0x009BC330 (:748)
void multi_gameplay_mode_player_died(player* victim, player* killer, damaged_by_types damage_type, const weapon_info* w_info); // 0x009F0070 (:769)
void multi_gameplay_mode_player_respawn(player* pp, int player_start_index); // 0x009F80F0 (:787)
float multi_gameplay_mode_adjust_respawn_weight(const player* pp, const player_start* pstart); // 0x0094EA20 (:805)
void multi_gameplay_mode_building_damage_changed(rfg_mover* building, float structural_mass_delta); // 0x009DC5C0 (:826)
bool multi_gameplay_mode_can_item_respawn(const item* itemp, bool& force_respawn); // 0x0094EA70 (:847)
bool multi_gameplay_mode_item_touched(const player* pp, const item* itemp); // 0x0094EAA0 (:861)
bool multi_gameplay_mode_player_set_action_object(const player* pp, t_usable_object& action_object); // 0x0094EAC0 (:874)
bool multi_gameplay_mode_player_can_swap_weapons(const player* pp); // 0x0094EAE0 (:887)
bool multi_gameplay_mode_player_can_attack(const player* pp, const weapon_info* w_info, damaged_by_types attack_type); // 0x0094EB00 (:904)
void multi_gameplay_mode_get_default_equipment(const player* pp, multi_game_equipment_loadout* loadout); // 0x0094EB20 (:921)
void multi_gameplay_mode_process_mode_specific(unsigned char* packet_data, const sender& from); // 0x0094EB90 (:936)
void multi_gameplay_mode_countdown_begin(); // 0x00978FC0 (:947)
void multi_gameplay_mode_round_begin(); // 0x009F00B0 (:960)
void multi_gameplay_mode_round_over(multi_round_winner winner); // 0x009FD3C0 (:976)
int multi_gameplay_mode_get_time_ms(); // 0x00978FE0 (:991)
bool multi_gameplay_mode_is_round_over(); // 0x009ABAB0 (:1007)
multi_round_winner multi_gameplay_mode_get_round_winner(); // 0x009ABAF0 (:1028)
void multi_gameplay_mode_server_migration_pre(); // 0x009F00E0 (:1051)
void multi_gameplay_mode_server_migration_post(); // 0x009ABB50 (:1064)
void multi_gameplay_mode_server_migration_update_clients(); // 0x009F0100 (:1077)
bool multi_gameplay_mode_is_team_mode(multi_gameplay_mode mode); // 0x0094EBB0 (:1092)
void multi_gameplay_mode_get_decorated_name(char* buffer); // 0x00979000 (:1098)
void multi_gameplay_mode_get_objective_description(char* buffer); // 0x0094EBD0 (:1110)
const char* multi_gameplay_mode_get_hud_data_item_name(); // 0x0094EC00 (:1121)
bool multi_gameplay_mode_hud_init(vint_data_item* item); // 0x00979040 (:1127)
bool multi_gameplay_mode_hud_process(vint_data_item* item); // 0x0094EC20 (:1141)
bool multi_gameplay_mode_alter_player_icon(const player* pp, hud_icon* icon); // 0x0094EC50 (:1152)
multi_announcer_lines multi_gameplay_mode_get_intro_line(); // 0x0094EC70 (:1163)
multi_gameplay_mode multi_gameplay_mode_get(); // 0x0094EC90 (:1177)
void multi_gameplay_mode_set(multi_gameplay_mode mode); // 0x009F0120 (:1199)
void multi_gameplay_mode_system_init(mempool_base& pool); // 0x009F0130 (:1212)
void multi_gameplay_mode_system_process(); // 0x0094ECB0 (:1226)
void multi_gameplay_mode_system_shutdown(); // 0x009F01A0 (:1238)
void multi_gameplay_mode_append_mode_specific_stats(unsigned char* packet_data, unsigned short& packet_size, unsigned int player_game_data_handle); // 0x0094ECC0 (:1252)
void multi_gameplay_mode_read_mode_specific_stats(unsigned char* packet_data, int& packet_offset, unsigned int player_game_data_handle); // 0x0094ECE0 (:1259)
int multi_gameplay_mode_get_mode_specific_stat_count(); // 0x0094ED00 (:1266)
void multi_gameplay_mode_get_mode_specific_stat_name(char* name, const int max_name_len, int stat_index); // 0x0094ED20 (:1274)
void multi_gameplay_mode_get_mode_specific_stat_value(char* value, const int max_value_len, int stat_index, unsigned int player_game_data_handle); // 0x0094ED70 (:1283)
