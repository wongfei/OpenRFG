// Original: volition/rfg/code/multi/multi_game/multi_game_callbacks.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_callbacks.cpp
//
// The callbacks the game layer registers with multi_core: delta compression of game objects, connections denied,
// dropped and removed, player names, migration to a new server, and the multiplayer memory.
#pragma once


class multi_connection;
class player;
struct sender;

enum multi_join_deny_reasons
{
    MULTI_JOIN_DENY_NONE = -1,
    MULTI_JOIN_DENY_VERSION = 0,
    MULTI_JOIN_DENY_SESSION_DNE = 1,
    MULTI_JOIN_DENY_SESSION_FULL = 2,
    MULTI_JOIN_DENY_NUM_REASONS = 3,
};

void multi_delta_compression_register_game_dcmo_callbacks(); // 0x0094F420 (:66)
void multi_game_connection_denied(multi_join_deny_reasons reason); // 0x0094F440 (:76)
void multi_game_connection_dropped(); // 0x0094F450 (:85)
void multi_game_about_to_remove_connection(multi_connection* conn); // 0x00996510 (:94)
void multi_game_removed_connection(multi_connection* conn); // 0x009DCFA0 (:164)
void multi_game_remove_player(player* pp); // 0x009BC740 (:192)
void multi_game_local_player_name_changed(const wchar_t* new_name); // 0x009BC830 (:232)
void multi_game_process_player_name_changed(unsigned char* packet_data, const sender& from); // 0x009BC9B0 (:268)
void multi_game_client_become_server_pre(); // 0x0094F460 (:308)
void multi_game_client_become_server_post(); // 0x0094F470 (:314)
void multi_game_memory_alloc(); // 0x0094F480 (:320)
void multi_game_memory_release(); // 0x0094F580 (:356)
