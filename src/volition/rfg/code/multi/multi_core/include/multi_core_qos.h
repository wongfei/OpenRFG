// Original: volition/rfg/code/multi/multi_core/include/multi_core_qos.h
// Implementation: volition/rfg/code/multi/multi_core/global/multi_core_qos.cpp
//
// Quality of service probes: latency and bandwidth of the connection to a session host.
#pragma once


class multi_connection;
struct sender;

void unused_dcf_mp_reset_upstreams(); // 0x00990030 (:201)
void unused_dcf_mp_override_stored_qos(); // 0x009900A0 (:211)
void unused_dcf_mp_toggle_fill(); // 0x00972B70 (:222)
void unused_dcf_mp_qos_info(); // 0x00972BC0 (:235)
void register_qos_bw_broadcast_cb(void (*cb)(int)); // 0x0094AC10 (:279)
const char* get_bandwidth_adjust_text(int adjust_code); // 0x0094AC20 (:284)
unsigned int multi_core_qos_get_min_game_bandwidth(); // 0x0094ACA0 (:366)
unsigned int multi_core_qos_get_max_game_bandwidth(); // 0x0094ACB0 (:371)
unsigned int multi_get_required_bandwidth_for_players(unsigned int num_players); // 0x0094ACC0 (:378)
unsigned int multi_get_num_players_from_bandwidth(unsigned int bandwidth_bits_per_sec); // 0x0094ACF0 (:392)
void multi_core_qos_system_init(); // 0x0094AD20 (:679)
void multi_core_qos_system_shutdown(); // 0x0094AD50 (:688)
void multi_core_qos_process(); // 0x009B67F0 (:712)
int multi_core_qos_find_latency_by_game_data(unsigned int game_data); // 0x009907C0 (:768)
unsigned int multi_core_qos_latency_ms(multi_connection* cp); // 0x0094AD60 (:782)
void multi_join_process_qos_packet(unsigned char* packet_data, const sender& from); // 0x00972C90 (:788)
unsigned int multi_core_qos_up_bandwidth(multi_connection* cp); // 0x00972D20 (:817)
void multi_core_qos_bandwidth_info(unsigned char* packet_data, const sender& from); // 0x009E4080 (:831)
float multi_core_qos_packet_loss(multi_connection* cp); // 0x0094AD70 (:850)
void multi_core_qos_report_ping_time(multi_connection* cp, unsigned int ping_ms); // 0x0094AD90 (:860)
unsigned int multi_core_qos_game_upstream_bandwidth(multi_connection* cp); // 0x0094ADD0 (:878)
void multi_core_qos_render(); // 0x0094ADE0 (:885)
unsigned int multi_core_qos_get_stored_upstream_bandwidth(); // 0x0094ADF0 (:1076)
void multi_core_qos_set_stored_upstream_bandwidth(unsigned int upstream_bandwidth); // 0x00990830 (:1108)
bool multi_core_qos_bandwidth_filling_is_on(); // 0x0094AE00 (:1151)
void multi_core_qos_bandwidth_filling_enable(); // 0x0094AE40 (:1161)
void multi_core_qos_query_live(); // 0x0094AE60 (:1168)
void multi_core_qos_live_process(); // 0x0094AE70 (:1175)
void multi_core_qos_done_using(); // 0x0094AE80 (:1210)
void multi_core_qos_game_started(); // 0x009AA230 (:1216)
