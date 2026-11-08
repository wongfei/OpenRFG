// Original: volition/rfg/code/multi/multi_core/include/multi_leave.h
// Implementation: volition/rfg/code/multi/multi_core/global/multi_leave.cpp
//
// Leaving a game: disconnecting from the server or shutting the server down.
#pragma once


class multi_connection;
class multi_session;
struct sender;

void multi_leave_send_connection_left_packet_to_all(multi_connection* conn); // 0x009B8700 (:95)
void multi_leave_process_connection_left_packet(unsigned char* packet_data, const sender& from); // 0x009DB690 (:118)
void multi_game_send_left_game(multi_session* session); // 0x009AAB10 (:152)
void multi_game_process_left_game(unsigned char* packet_data, const sender& from); // 0x009DB6F0 (:185)
