// Original: volition/rfg/code/multi/multi_game/multi_game_destruction_migration.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_destruction_migration.cpp
//
// Destruction during server migration: the new server collects the destruction frames every client has applied and
// brings them to the same frame.
#pragma once


class multi_connection;
struct sender;

void multi_game_send_req_destruct_update(multi_connection* cp, unsigned short start_frame_index, unsigned short end_frame_index); // 0x009C57A0 (:75)
void multi_game_process_migrate_req_df_update(unsigned char* packet_data, const sender& from); // 0x009C5860 (:95)
bool is_first_frame_larger(unsigned short first_frame, unsigned short second_frame); // 0x00955B00 (:141)
void get_largest_client_applied_destruction_frame(unsigned short& client_largest_applied_frame, multi_connection** largest_destruction_frame_cp); // 0x00998BF0 (:149)
unsigned short get_target_destruction_frame(); // 0x00998CB0 (:170)
void multi_game_migration_send_client_destruction_update(multi_connection* cp, unsigned short start_index, unsigned short end_index); // 0x009C5980 (:187)
void multi_game_migration_send_client_destruction_updates(unsigned short last_destruction_frame); // 0x009C5B30 (:242)
void process_destruction_sent_update_state_change(); // 0x009C5BB0 (:271)
void request_destruction_updates_from_clients(); // 0x009C5C00 (:288)
bool have_all_clients_sent_destruction_status(); // 0x00998D20 (:328)
bool have_all_clients_sent_destruction_updates(); // 0x00998D90 (:349)
void process_migration_destruction(); // 0x00955B40 (:386)
unsigned short get_server_desired_last_destruction_frame(); // 0x00998EC0 (:395)
bool has_all_destruction_been_applied(); // 0x00955B60 (:501)
bool have_all_client_applied_destruction(); // 0x00998F70 (:513)
void multi_client_send_destruction_applied(); // 0x009C5D20 (:533)
void multi_game_process_migrate_df_update(unsigned char* packet_data, const sender& from); // 0x009DEA30 (:549)
void multi_game_process_migrate_destruct_complete(unsigned char* packet_data, const sender& from); // 0x00955BA0 (:562)
bool multi_game_migration_can_accept_destroyed_mover_list(); // 0x0097B870 (:578)
