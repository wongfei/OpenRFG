// Original: volition/rfg/code/multi/multi_game/multi_game_respawn.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_respawn.cpp
//
// Respawning: when a player may respawn, the best spawn point, respawn messages and vehicle respawns.
#pragma once


class multi_connection;
class player;
struct sender;
class vehicle;

enum respawn_request_deny_reasons
{
    RRDR_NO_REASON = 0,
    RRDR_NO = 1,
    RRDR_TIME_UNTIL_RESPAWN = 2,
    RRDR_NOT_IN_GAMEPLAY = 3,
    NUM_RESPAWN_REQUEST_DENY_REASONS = 4,
};

void multi_game_respawn_init(); // 0x009611C0 (:179)
void multi_game_respawn_shutdown(); // 0x009611D0 (:190)
void multi_game_respawn_match_init(); // 0x009611E0 (:198)
void multi_game_respawn_match_shutdown(); // 0x009611F0 (:211)
void multi_game_respawn_match_process(); // 0x00961200 (:222)
respawn_request_deny_reasons multi_game_respawn_can_player_respawn(const player* pp); // 0x00961210 (:234)
int multi_game_respawn_get_best_spawn_point(const player* respawning_player, bool first_spawn); // 0x009A0980 (:257)
void multi_game_respawn_send_respawn_to_all(player* pp, int player_start_idx, bool decrement_num_respawns); // 0x009D26E0 (:541)
void multi_game_respawn_send_respawn_to_player(multi_connection* cp, player* pp, int player_start_idx, bool decrement_num_respawns); // 0x009D2920 (:572)
void multi_game_respawn_process_respawn(unsigned char* packet_data, const sender& from); // 0x009D2B60 (:603)
unsigned int multi_game_respawn_find_spawning_point_handle(const vehicle* vp); // 0x009A15C0 (:640)
void multi_game_respawn_request_vehicle_respawn(const vehicle* vp, int delay_ms); // 0x009B21B0 (:656)
