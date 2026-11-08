// Original: volition/rfg/code/multi/multi_core/include/multi_core_game_bandwidth.h
// Implementation: volition/rfg/code/multi/multi_core/global/multi_core_game_bandwidth.cpp
//
// The game's use of the bandwidth: how much of each packet the object states may use, by priority and distance to the
// client's player. Placement of test_target_info inferred.
#pragma once


class multi_connection;
class multi_player_uid;
class multi_session;
struct sender;

// size 0xC
struct test_target_info
{
    multi_connection* cp; // 0x00
    int upstream_bw; // 0x04
    bool bw_fail_by_limit; // 0x08
    bool bw_untested; // 0x09
};

void multi_game_send_full_bw_results(multi_session* sp); // 0x009D83F0 (:252)
void tracked_bandwidth_info_system_init(); // 0x009B58B0 (:395)
void tracked_bandwidth_info_set_info(const multi_player_uid& player_uid, int upstream_bw, int upstream_bw_test_limit, int age_ms); // 0x009A90C0 (:407)
bool tracked_bandwidth_info_get_info(const multi_player_uid& player_uid, int& upstream_bw, int& upstream_bw_test_limit, int& age_ms); // 0x009722D0 (:454)
void multi_game_bw_system_init(); // 0x00972610 (:1131)
void multi_game_bw_stop(); // 0x009D8F80 (:1146)
void multi_game_bw_reset(int int_session_type); // 0x009D8FA0 (:1156)
void multi_game_bw_process(); // 0x009D9000 (:1173)
void multi_game_process_bw_tester(unsigned char* packet_data, const sender& from); // 0x009476C0 (:1198)
void multi_game_process_bw_test_info(unsigned char* packet_data, const sender& from); // 0x009476F0 (:1214)
void multi_game_process_flood_update(unsigned char* packet_data, const sender& from); // 0x009A91E0 (:1233)
void multi_game_process_full_flood_results(unsigned char* packet_data, const sender& from); // 0x009A9340 (:1259)
