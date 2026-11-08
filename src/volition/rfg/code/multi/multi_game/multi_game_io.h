// Original: volition/rfg/code/multi/multi_game/multi_game_io.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_io.cpp
//
// Game level packets: the join flow (client loaded, game start) and the server/client process callbacks.
#pragma once


class multi_connection;
struct sender;

void multi_game_join_process_client_loaded_game(unsigned char* packet_data, const sender& from); // 0x009DEE90 (:386)
void multi_game_join_remove_client(const multi_connection* cp); // 0x009AEF10 (:415)
void multi_game_call_server_process_callback(); // 0x009DEEE0 (:422)
void multi_game_call_client_process_callback(); // 0x009FD3F0 (:451)
void multi_game_call_client_connection_established_callback(); // 0x00955ED0 (:485)
void multi_game_send_join_game_start(multi_connection* conn, const char* level); // 0x009C5D90 (:495)
void multi_game_process_join_game_start(unsigned char* packet_data, const sender& from); // 0x009C5E60 (:520)
void multi_game_register_client_connection_dropped_callback(void (*callback)()); // 0x00955EE0 (:563)
void multi_game_call_client_connection_dropped_callback(); // 0x00955EF0 (:570)
void multi_game_call_client_become_server_callback_pre(); // 0x00955F00 (:579)
void multi_game_call_client_become_server_callback_post(); // 0x00955F10 (:586)
