// Original: volition/rfg/code/multi/multi_core/include/multi_core_io.h
// Implementation: volition/rfg/code/multi/multi_core/global/multi_core_io.cpp
//
// Packet input and output: the registered packet types (packet_def: handler, direction, the connection status it needs),
// sending to one or all connections, snapshots, and dispatching received packets. Placement of the packet types inferred.
#pragma once


struct connection_address;
struct cpu_job;
union cpu_job_arg;
class mempool_base;
struct mp_transmit_list;
class multi_connection;
class multi_session;
struct sender;

namespace kaiko
{
    namespace gamelink
    {
        class GameLink;
    }
}

enum packet_direction
{
    PD_SERVER_TO_CLIENT = 0,
    PD_CLIENT_TO_SERVER = 1,
    PD_BOTH = 2,
};

enum packet_connection_status
{
    PCS_ANY = 0,
    PCS_CONNECTED_ANY = 1,
    PCS_CONNECTED_NOT_GAMEPLAY = 2,
    PCS_CONNECTED_GAMEPLAY = 3,
};

// size 0x1C
struct packet_def
{
    unsigned char packet_id; // 0x00
    void (*receive_func)(unsigned char*, const sender&); // 0x04
    const char* packet_name; // 0x08
    const char* packet_short_name; // 0x0C
    packet_direction direction; // 0x10
    packet_connection_status connection_status; // 0x14
    bool connection_required; // 0x18
    bool drop_on_cheating; // 0x19
    unsigned char group_id; // 0x1A
};

// size 0x20
struct snapshot_packet_header
{
    multi_connection* cp; // 0x00
    int cp_local_nonce; // 0x04
    unsigned short type; // 0x08
    unsigned int* p_reliable_id; // 0x0C
    unsigned short packet_size; // 0x10
    unsigned char* packet_data; // 0x14
    void (*callback)(void*); // 0x18
    void* callback_data; // 0x1C
};

// size 0x8
struct sender
{
    sender(multi_connection* cp, connection_address* addr); // 0x00947B00 (:1282)
    sender(multi_connection* cp); // 0x00947AF0 (:1277)
    sender(connection_address* addr); // 0x00947AD0 (:1272)

    connection_address* address; // 0x00
    multi_connection* connection; // 0x04
};

extern packet_def Multi_core_packets[46]; // 0x0167D848, the core packet types (multi_core_io.cpp)
extern packet_def Multi_game_packets[82]; // 0x0167E710, the game packet types (multi_game_lobby.cpp)
extern volatile unsigned int Multi_connections_acquired; // 0x02FEB5E8, 1 while the send job owns the connections

void unused_dcf_mp_display_bandwidth_settings(); // 0x00947730 (:244)
void unused_dcf_mp_set_bandwidth(); // 0x009A9520 (:258)
void unused_dcf_mp_set_packet_rate(); // 0x009477D0 (:274)
int multi_io_queue_read(void* data, connection_address& from_conn); // 0x009A96C0 (:704)
unsigned int multi_io_get_max_data_packet(); // 0x00947B20 (:1286)
unsigned int multi_io_get_max_rate_per_second(); // 0x00947B30 (:1295)
unsigned int multi_io_get_packet_rate_per_second(); // 0x0098FD30 (:1304)
void multi_io_set_packet_rate_per_second(unsigned int rate_ms); // 0x0098FD80 (:1319)
void multi_io_set_bandwidth_in_bits_sec(unsigned int bits_sec); // 0x0098FDA0 (:1332)
void multi_io_init(mempool_base& pool); // 0x009FB440 (:1342)
void multi_io_shutdown(); // 0x009FDA00 (:1359)
void multi_io_process_incoming(); // 0x009D96E0 (:1368)
void multi_io_send_low_level_message_via_game_link(kaiko::gamelink::GameLink* game_link, unsigned char packet_type, const unsigned char* packet_data, unsigned short data_size, const connection_address& connection); // 0x00972690 (:1410)
void multi_io_send(multi_connection* cp, unsigned char* data, unsigned int len); // 0x009A97C0 (:1441)
void multi_io_send_to_all(multi_session* session, unsigned char* data, unsigned int len); // 0x009A9840 (:1462)
void multi_io_send_to_all_except_one(multi_session* session, unsigned char* data, unsigned int len, multi_connection* except); // 0x009A98F0 (:1483)
unsigned int multi_io_send_reliable(multi_connection* cp, unsigned char* data, unsigned int len, void (*callback)(void*), void* callback_data, unsigned int* p_reliable_id); // 0x009B5AA0 (:1511)
void multi_io_send_to_all_reliable(multi_session* session, unsigned char* data, unsigned int len); // 0x009B5BA0 (:1525)
void multi_io_send_to_all_except_one_reliable(multi_session* session, unsigned char* data, unsigned int len, multi_connection* except); // 0x009B5BF0 (:1547)
void multi_io_force_send_all_buffered_packets(); // 0x0098FE90 (:1597)
bool multi_io_verify_packet_direction_send(const packet_def* packet, const sender* receiver); // 0x00972750 (:1620)
bool multi_io_verify_packet_direction_receive(const packet_def* packet, const sender* packet_sender); // 0x00972790 (:1644)
bool multi_io_verify_packet_connection_state(const packet_def* packet, const multi_connection* cp); // 0x00947B40 (:1667)
bool multi_core_io_allow_object_update_send(const multi_session* game_session); // 0x009727E0 (:1685)
void multi_io_send_process_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2); // 0x00A00E60 (:1711)
void multi_io_send_process(const mp_transmit_list& transmit_list); // 0x009FDA10 (:1732)
void multi_io_recalculate_bandwidth(); // 0x0098FF60 (:1838)
