// Original: volition/rfg/code/multi/multi_core/include/multi_join.h
// Implementation: volition/rfg/code/multi/multi_core/global/multi_join.cpp
//
// Joining a server: the join handshake of a client.
#pragma once

#include "volition/rfg/code/multi/multi_core/include/multi_core_session_base.h"

class multi_connection;
class multi_player_uid;
class multi_session;
class multi_session_info;
struct sender;

extern int Join_request_timeout; // 0x0167DE60, ms a join request waits for the host's answer

void multi_join_send_join_request_packet(multi_connection* conn, multi_session* join_target, bool reservedSlot); // 0x009B7600 (:175)
void multi_join_process_join_request_packet(unsigned char* packet_data, const sender& from); // 0x009DB070 (:314)
void multi_join_send_join_accept_packet(multi_connection* conn); // 0x009B78B0 (:559)
void multi_join_process_join_accept_packet(unsigned char* packet_data, const sender& from); // 0x009FC3E0 (:616)
void multi_join_process_join_deny_packet(unsigned char* packet_data, const sender& from); // 0x009B7AC0 (:712)
void multi_join_send_join_new_session_packet(multi_connection* cp, multi_session_info* join_target_info); // 0x009B7C30 (:748)
void multi_join_process_join_new_session_packet(unsigned char* packet_data, const sender& from); // 0x009B7CF0 (:761)
void multi_join_send_join_success_packet(multi_connection* cp, multi_session* join_target, multi_session* join_source); // 0x0094B700 (:774)
void multi_join_process_join_success_packet(unsigned char* packet_data, const sender& from); // 0x0094B710 (:787)
void multi_join_send_join_failed_packet(multi_connection* cp, multi_session* join_target, multi_session* join_source); // 0x009B7DB0 (:809)
void multi_join_process_join_failed_packet(unsigned char* packet_data, const sender& from); // 0x009B7E50 (:819)
void multi_join_send_join_complete_packet(multi_connection* cp, multi_session* join_target, multi_session* join_source); // 0x009B7F10 (:833)
void multi_join_process_join_complete_packet(unsigned char* packet_data, const sender& from); // 0x009B7FB0 (:843)
void multi_join_send_remove_reservation(multi_connection* cp, multi_session* session, const multi_player_uid& reservee); // 0x009B8070 (:855)
void multi_join_process_remove_reservation(unsigned char* packet_data, const sender& from); // 0x00973F80 (:865)
void multi_join_send_updated_connection_packet_to_all(multi_connection* conn); // 0x009DB630 (:880)
void multi_join_send_updated_connection_packet(multi_connection* data_cp, multi_connection* to_cp); // 0x009B8110 (:898)
void multi_join_process_updated_connection_packet(unsigned char* packet_data, const sender& from); // 0x00973FD0 (:922)
void multi_join_process_new_connection_packet(unsigned char* packet_data, const sender& from); // 0x009AA910 (:950)
void multi_join_process_new_game_data_packet(unsigned char* packet_data, const sender& from); // 0x00974020 (:995)
void multi_join_process_remove_game_data_packet(unsigned char* packet_data, const sender& from); // 0x00974060 (:1011)
void multi_join_send_join_deny_packet(multi_connection* conn, multi_session_join_status reason); // 0x009B81C0 (:1032)
void multi_join_send_new_connection_packet_to_all(multi_connection* new_connection); // 0x009B8370 (:1060)
void multi_join_send_connections(multi_connection* conn); // 0x009B8450 (:1088)
void multi_join_send_remove_gamedata(multi_connection* conn, unsigned int game_data_handle); // 0x009B8620 (:1127)
