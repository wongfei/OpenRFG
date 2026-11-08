// Original: volition/rfg/code/multi/multi_game/multi_game_player.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_player.cpp
//
// Multiplayer players: state and death processing, weapon sync (pickups, removal), teams and team changes, damage
// requests. Placement of the weapon event data inferred.
#pragma once

#include "volition/rfg/code/multi/multi_core/include/multi_core_session_base.h"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_state.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"

class human;
struct inv_item_info;
struct inventory_item;
class multi_connection;
class multi_session;
class object;
class player;
struct sender;
class vector;
struct weapon_info;

enum request_damage_reason
{
    RDR_NONE = -1,
    RDR_RAGDOLL = 0,
    RDR_SUICIDE = 1,
    RDR_FALL = 2,
    NUM_REQUEST_DAMAGE_REASONS = 3,
};

// size 0x30
struct player_weapon_event_data
{
    player_weapon_event_data(); // 0x0096BCC0

    network_to_game_handle player_ng_handle; // 0x00
    unsigned char changes; // 0x08
    unsigned char inventory_slot[4]; // 0x09
    unsigned int item_info_index[4]; // 0x10
    unsigned char change_mask[4]; // 0x20
    unsigned char cur_rounds_in_magazine[4]; // 0x24
    unsigned short rounds_in_reserve[4]; // 0x28
};

int inv_item_get_weapon_slot(const player* pp, const weapon_info* winfo); // 0x00982360 (:226)
void multi_game_player_apply_vehicle_state(player* pp, const multi_player_state& player_state); // 0x009E1E20 (:1153)
void multi_game_player_process_state(player* pp); // 0x009E1F10 (:1204)
void multi_game_player_system_init(); // 0x009CF990 (:1306)
void multi_game_player_system_shutdown(); // 0x009B1EE0 (:1334)
void multi_game_player_do_frame(player* pp); // 0x009CFAA0 (:1340)
void multi_game_player_clear_weapon_changes(const player* pp); // 0x0095FA80 (:1395)
void multi_game_player_weapons_sync(multi_connection* cp); // 0x009D01E0 (:1419)
void multi_game_player_send_remove_weapon(inventory_item* iip); // 0x009D0640 (:1493)
void multi_game_player_process_remove_weapon(unsigned char* packet_data, const sender& from); // 0x009D0710 (:1507)
void multi_game_player_remove_weapon(player* pp, const inventory_item* inv_item); // 0x009829B0 (:1533)
void multi_game_player_picked_up_weapon(player* pp, const inventory_item* inv_item); // 0x00982B00 (:1574)
void multi_game_player_picked_up_ammo(player* pp, const inventory_item* inv_item, unsigned int reserve_rounds, unsigned int magazine_rounds); // 0x00982CD0 (:1621)
void multi_game_player_process_weapons(); // 0x009D0770 (:1656)
void multi_game_process_player_weapons_update(unsigned char* packet_data, const sender& from); // 0x009E2170 (:1755)
bool multi_game_player_can_view_object(const player* pp, const object* obj); // 0x0095FAE0 (:1831)
void multi_game_human_died(const human* victim, const human* attacker, damaged_by_types damage_type, const weapon_info* weapon_used); // 0x009D0C40 (:1896)
void multi_game_player_process_player_death(unsigned char* packet_data, const sender& from); // 0x009E2460 (:1945)
void multi_game_player_increment_my_team(multi_session* session); // 0x009F6C10 (:2015)
void multi_game_player_decrement_my_team(multi_session* session); // 0x009F6CD0 (:2046)
void multi_game_player_change_my_team(multi_session* session, human_teams team); // 0x009E25D0 (:2081)
void multi_game_player_send_team_request(multi_session* session, human_teams team); // 0x009D0DA0 (:2103)
void multi_game_player_send_team_change(const multi_connection* changed_player, human_teams new_team); // 0x009D0E30 (:2118)
void multi_game_player_process_team_request(unsigned char* packet_data, const sender& from); // 0x009D0F10 (:2133)
void multi_game_player_process_team_change(unsigned char* packet_data, const sender& from); // 0x00982F60 (:2172)
void multi_game_player_append_teams_to_packet(unsigned char* packet_data, unsigned short& packet_size, session_types session_type); // 0x00983000 (:2189)
void multi_game_player_read_teams_from_packet(unsigned char* packet_data, int& packet_offset); // 0x00983150 (:2212)
void multi_game_player_set_my_desired_weapon(multi_session* session, unsigned int desired_weapon_inv_item_info_handle); // 0x009E2690 (:2237)
void multi_game_player_set_desired_weapon(multi_session* session, unsigned int multi_game_data_handle, unsigned int desired_weapon_inv_item_info_handle); // 0x009D1000 (:2244)
void multi_game_player_process_desired_weapon_request(unsigned char* packet_data, const sender& from); // 0x009D12A0 (:2285)
void multi_game_player_process_desired_weapon_change(unsigned char* packet_data, const sender& from); // 0x0095FD30 (:2306)
void multi_game_player_append_desired_weapons_to_packet(unsigned char* packet_data, unsigned short& packet_size, multi_session* session); // 0x00983230 (:2319)
void multi_game_player_read_desired_weapons_from_packet(unsigned char* packet_data, int& packet_offset); // 0x0095FE00 (:2329)
const inv_item_info* multi_game_player_get_default_weapon(unsigned int multi_game_data_handle); // 0x009D13A0 (:2345)
void multi_game_player_request_damage(float damage, request_damage_reason reason); // 0x009D14A0 (:2378)
void multi_game_player_process_request_damage(unsigned char* packet_data, const sender& from); // 0x009D15D0 (:2403)
void multi_game_player_ragdoll(player* pp, const vector& linear_velocity, const vector& angular_velocity); // 0x0095FF20 (:2432)
void multi_game_player_process_player_ragdoll(unsigned char* packet_data, const sender& from); // 0x009D1680 (:2448)
void multi_game_player_ragdoll_point_impulse(const player* pp, const vector& relative_impulse_position, const vector& linear_impulse); // 0x009D1BB0 (:2492)
void multi_game_player_process_player_ragdoll_point_impulse(unsigned char* packet_data, const sender& from); // 0x009D1F30 (:2521)
void multi_game_player_weapon_update_clear(); // 0x009D22D0 (:2692)

// 0x0096BCC0: no player yet
inline player_weapon_event_data::player_weapon_event_data()
{
    // the handle constructs itself (no network handle, no game handle)
}
