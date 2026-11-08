// Original: volition/rfg/code/multi/multi_game/modes/multi_game_mode_siege.h
// Implementation: volition/rfg/code/multi/multi_game/modes/multi_game_mode_siege.cpp
//
// The Siege mode: the siege targets (buildings) and their damage decide the round; the mode has its own HUD
// document and default equipment. Placement of siege_target inferred.
// Each mode implements the mode callbacks of multi_game_mode_common (enter, process, round over and winner, player died,
// respawn weights, HUD, objective text, migration, mode specific stats).
#pragma once

#include "volition/rfg/code/objects/level_object_properties_shared.h"

class mempool_base;
class multi_connection;
struct multi_game_equipment_loadout;
class multi_object_marker;
union multi_round_winner;
class player;
class player_start;
class rfg_mover;
struct sender;
class vint_data_item;
struct weapon_info;

// size 0x78
struct siege_target
{
    wchar_t name[47]; // 0x00
    multi_object_marker* marker; // 0x60
    float percent_destroyed_last_frame; // 0x64
    float percent_destroyed; // 0x68
    float percent_destroyed_accumulator; // 0x6C
    float percent_repaired_accumulator; // 0x70
    float distance_to_local_player; // 0x74
};

void siege_mode_round_begin_jip(multi_connection* cp); // 0x009BB1C0 (:614)
void siege_mode_memory_init(mempool_base& pool); // 0x009BB330 (:634)
void siege_mode_memory_shutdown(); // 0x009DBC10 (:639)
void siege_mode_enter(); // 0x0094DF80 (:646)
void siege_mode_exit(); // 0x0094DFA0 (:653)
void siege_mode_level_init_post(); // 0x009AB6C0 (:659)
void siege_mode_level_shutdown(); // 0x009DBC60 (:774)
void siege_mode_process(); // 0x009EF260 (:781)
void siege_mode_handshake(); // 0x00994CF0 (:887)
void siege_mode_player_died(player* victim, player* killer, damaged_by_types damage_type, const weapon_info* w_info); // 0x009EF770 (:956)
float siege_mode_adjust_respawn_weight(const player* pp, const player_start* pstart); // 0x00977F60 (:986)
void siege_mode_process_mode_specific(unsigned char* packet_data, const sender& from); // 0x009EFA90 (:1009)
void siege_mode_round_begin(); // 0x00978060 (:1054)
void siege_mode_round_over(multi_round_winner winner); // 0x009EFB70 (:1082)
bool siege_mode_is_round_over(); // 0x00978130 (:1095)
multi_round_winner siege_mode_get_round_winner(); // 0x009BB350 (:1113)
void siege_mode_server_migration_post(); // 0x009EFD30 (:1156)
void siege_mode_building_damage_changed(player* pp, rfg_mover* building, float structural_mass_delta); // 0x009EFE00 (:1182)
void siege_mode_get_default_equipment(const player* pp, multi_game_equipment_loadout* loadout); // 0x0094DFB0 (:1207)
void siege_mode_get_decorated_name(char* buffer); // 0x00978160 (:1233)
void siege_mode_get_objective_description(char* buffer); // 0x0094E010 (:1257)
void siege_mode_hud_init(vint_data_item* item); // 0x0094E0B0 (:1273)
void siege_mode_hud_process(vint_data_item* item); // 0x00978180 (:1285)
void siege_mode_hud_doc_init(int doc_handle); // 0x0094E100 (:1301)
void siege_mode_hud_doc_process(int doc_handle); // 0x009782C0 (:1339)
bool siege_mode_hud_doc_should_render(int doc_handle); // 0x0094E290 (:1397)
void siege_mode_hud_doc_shutdown(int doc_handle); // 0x0094E2A0 (:1402)
void siege_mode_append_mode_specific_stats(unsigned char* packet_data, unsigned short& packet_size, unsigned int player_game_data_handle); // 0x0094E2E0 (:1413)
void siege_mode_read_mode_specific_stats(unsigned char* packet_data, int& packet_offset, unsigned int player_game_data_handle); // 0x0094E2F0 (:1418)
int siege_mode_get_mode_specific_stat_count(); // 0x0094E300 (:1423)
void siege_mode_get_mode_specific_stat_name(char* name, const int max_name_len, int stat_index); // 0x00978730 (:1428)
void siege_mode_get_mode_specific_stat_value(char* value, const int max_value_len, int stat_index, unsigned int player_game_data_handle); // 0x009787A0 (:1445)
