// Original: volition/rfg/code/multi/multi_core/include/multi.h
// Implementation: volition/rfg/code/multi/multi_core/global/multi.cpp
//
// The multiplayer core: system init/shutdown and the per-frame processing of the network layer, starting and stopping
// multiplayer (Multiplayer is set while a multiplayer game runs), the server/client role and the local player.
#pragma once

#include "volition/vlib/code/time/timestamp.h"

class cs_wrapper;
class event_trigger;
class multi_connection;
class mutex;
class scheduling_trigger;
struct packet_def;

// The client's progress into a game (placement inferred).
enum client_state
{
    CLIENT_STATE_DISCONNECTED = 0,
    CLIENT_STATE_ESTABLISHED = 1,
    CLIENT_STATE_PENDING_GAME_LOAD = 2,
    CLIENT_STATE_LOADING_GAME = 3,
    CLIENT_STATE_IN_GAME = 4,
};

// The transmit group rotation (placement inferred).
// size 0xC
struct packet_send_info
{
    packet_send_info(); // 0x00943050

    int next_transmit_group; // 0x00
    timestamp_realtime last_transmit_time; // 0x04
    bool group_0_transmit_this_frame; // 0x08
};

struct mp_transmit_list;

// 0x00943050: the timestamp starts invalid
inline packet_send_info::packet_send_info()
{
}

extern bool Multiplayer; // 0x02FEB588
extern bool Multi_syslink; // 0x02FEB589, the game is a system link (LAN) game
extern bool Multi_join_in_progress; // 0x02FEB58A
extern bool Multi_last_session_start_succeeded; // 0x02FEB58B
extern packet_def* Multi_packets; // 0x02FEB58C, Multi_core_packets and Multi_game_packets, copied by multi_init
extern client_state Client_state; // 0x02FEB6A8
extern packet_send_info Packet_send_info; // 0x01691630
extern mp_transmit_list Transmit_list; // 0x03017748
extern mutex Multi_thread_mutex; // 0x02FED1B8, guards the game link's sockets against the send job

// The multiplayer send job handshake (game_handshake_do_frame starts the send job of the frame through these).
// Placement inferred: the multiplayer globals next to Multiplayer in .data (rfg_multi_unity).
extern cs_wrapper MP_send_setup_cs; // 0x02FEBC40
extern scheduling_trigger* MP_send_scheduling_trigger; // 0x02FEB5B4
extern event_trigger* MP_send_process_trigger; // 0x02FEB5B8
extern bool MP_reached_render_trigger_point; // 0x02FEB5B1
extern bool Multi_left_game_by_choice; // 0x02FEB658, the local player left the game (not kicked or disconnected)
extern bool Multi_partied; // 0x02FEB6F3, the local player came in with a party

void unused_dcf_mp_channel(); // 0x00943060 (:486)
void unused_dcf_mp_version(); // 0x0098E620 (:523)
void multi_core_register_pre_io_send_job_game_callback(void (*cb)()); // 0x00943100 (:581)
void multi_core_register_handshake_game_callback(void (*cb)()); // 0x00943110 (:586)
void multi_server_do_post_sim(); // 0x00943120 (:592)
void multi_system_init(); // 0x009D7CD0 (:859)
void multi_system_shutdown(); // 0x00971710 (:877)
void multi_init(bool game_is_syslink_or_lan); // 0x009FD690 (:889)
void multi_shutdown(); // 0x00A00D40 (:999)
void multi_process(bool booted_to_main_menu); // 0x00A04FB0 (:1076)
void multi_handshake(); // 0x009FB410 (:1357)
void multi_render(); // 0x009D7D60 (:1379)
void multi_do_post_sim(); // 0x00971770 (:1388)
int pc_get_my_ip(); // 0x009432D0 (:1400)
bool multi_allow_join_in_progress(); // 0x00971780 (:1415)
void multi_set_as_server(bool server); // 0x00943330 (:1424)
bool multi_is_server(); // 0x00943340 (:1432)
bool multi_is_syslink_game(); // 0x00943350 (:1437)
void multi_set_as_dedicated(bool dedicated); // 0x00943360 (:1442)
bool multi_is_dedicated_server(); // 0x00943370 (:1448)
unsigned int multi_get_multiplayer_version(); // 0x009717A0 (:1463)
void multi_get_transmit_group_info(int& num_transmit_groups, int& transmit_group_count); // 0x00943390 (:1476)
void multi_memory_alloc(); // 0x009717C0 (:1482)
void multi_memory_release(); // 0x009717D0 (:1488)
