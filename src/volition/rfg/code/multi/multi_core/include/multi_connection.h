// Original: volition/rfg/code/multi/multi_core/include/multi_connection.h
// Implementation: volition/rfg/code/multi/multi_core/global/multi_connection.cpp
//
// Connections to the other stations: per connection, reliable (in order) data with acknowledges and resends, tracked
// packets, the bandwidth available to each send slot, and the connection status and disconnect reasons. Placement of the
// reliable and tracking types inferred.
#pragma once

#include <stdint.h>
#include "unit4/systemservices/systemservices.hpp"
#include "volition/rfg/code/multi/multi_core/include/multi_player_uid.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/time/timestamp.h"

class dc_object;
class multi_connection;
class multi_session;
class multi_session_base;
class reliable_in_order_buffer;
struct sender;

enum multi_connection_status
{
    CS_PENDING = 0,
    CS_DISCONNECTED = 1,
    CS_CONNECTED = 2,
    CS_CONNECTED_GAMEPLAY = 3,
    NUM_MULTI_CONNECTION_STATUSES = 4,
};

enum multi_connection_disconnect_reason
{
    CDR_NONE = 0,
    CDR_DELETED = 1,
    CDR_RELIABLE_BUFFER_FULL = 2,
    CDR_RELIABLE_CALLBACK_BUFFER_FULL = 3,
    CDR_NETSEND_FAILED = 4,
    CDR_TEST_ADDRESS_FAILED = 5,
    CDR_GAME_MIGRATE_DESTRUCTION_LAG = 6,
    CDR_HEARTBEAT_TIMEOUT = 7,
    CDR_RELIABLE_ACK_TIMEOUT = 8,
    CDR_PENDING_STATE_TIMEOUT = 9,
    CDR_FAILED_SESSION_OP = 10,
    CDR_FAILED_PLATFORM_ADD = 11,
    CDR_SESSION_REMOVED = 12,
    CDR_SESSION_CONNECTION_LEFT = 13,
    CDR_SESSION_MIGRATE_FAILED = 14,
    CDR_SESSION_MODIFY_FAILED = 15,
    CDR_SESSION_ARBITRATE_FAILED = 16,
    CDR_SESSION_START_FAILED = 17,
    NUM_MULTI_CONNECTION_DISCONNECT_REASONS = 18,
};

// size 0x8
class available_bandwidth_slot_info
{
public:
    available_bandwidth_slot_info(); // 0x00941E30

    unsigned int m_bits_sent; // 0x00
    timestamp_realtime m_send_time; // 0x04
};

// size 0xF8
class available_bandwidth
{
public:
    void reset(unsigned int starting_bandwidth_per_second); // 0x00971AB0 (:2759)
    void process(unsigned int bandwidth_per_second); // 0x00971AF0 (:2777)
    unsigned int get_available_bandwidth(); // 0x00971B30 (:2792)
    unsigned int get_timeslice_available_bandwidth(); // 0x00971B80 (:2817)
    unsigned int get_bandwidth_consumed(int time_ms); // 0x00943BE0 (:2798)
    void mark_bandwidth_used(unsigned int bandwidth_used); // 0x00943C20 (:2837)
    available_bandwidth(); // 0x00970670

private:
    unsigned int m_slot_index; // 0x00
    unsigned int m_bandwidth_max; // 0x04
    available_bandwidth_slot_info m_bandwidth_slots[30]; // 0x08
};

// size 0x10
struct reliable_callback
{
    void (*callback)(void*); // 0x00
    void* callback_data_ptr; // 0x04
    reliable_callback* reliable_next; // 0x08
    reliable_callback* reliable_prev; // 0x0C
};

// size 0x220
class reliable_data
{
public:
    reliable_data(); // 0x00941DE0 (.h:151)
    void reset(unsigned int identifier); // 0x009A8330 (:1174)

    unsigned short m_connection_ack_id; // 0x00
    unsigned char m_buffer[512]; // 0x02
    unsigned short m_buffer_size; // 0x202
    unsigned char m_packet_count; // 0x204
    unsigned int m_creation_time; // 0x208
    unsigned int m_first_transmission_time; // 0x20C
    unsigned int m_next_send_time; // 0x210
    unsigned int m_identifier; // 0x214
    unsigned short m_transmit_count; // 0x218
    reliable_callback* m_callbacks; // 0x21C
};

// size 0x104
class reliable_in_order_data
{
public:
    unsigned short next_expected_recv_sequence_id[64]; // 0x00
    unsigned short next_send_sequence_id[64]; // 0x80
    reliable_in_order_buffer* list; // 0x100
};

// size 0x214
class reliable_in_order_buffer
{
public:
    unsigned short group_id; // 0x00
    unsigned short sequence_id; // 0x02
    unsigned int debug_recv_time; // 0x04
    unsigned int debug_framecount; // 0x08
    unsigned char packet_data[512]; // 0x0C
    reliable_in_order_buffer* prev; // 0x20C
    reliable_in_order_buffer* next; // 0x210
};

// size 0x10
struct tracked_packet
{
    void init(unsigned short packet_id); // 0x00941DC0 (.h:116)

    unsigned short m_packet_id; // 0x00
    unsigned int m_transmission_time; // 0x04
    bool m_received; // 0x08
    unsigned int m_size; // 0x0C
};

// size 0x32D4
struct packet_tracking
{
    tracked_packet packets[800]; // 0x00
    unsigned int last_index; // 0x3200
    unsigned short current_id; // 0x3204
    unsigned short acks_to_send[100]; // 0x3206
    unsigned short acks_count; // 0x32CE
    unsigned short last_sends_ack_count; // 0x32D0
};

// size 0x4
struct ack_tracker
{
    unsigned short ack_id; // 0x00
    unsigned char ack_sent_count; // 0x02
};

// size 0xA
struct connection_address
{
    void init(); // 0x00971BB0 (:2845)
    void set(unit4::RemoteUserId in_connection_id); // 0x00943C50 (:2850)
    void copy(const connection_address& src); // 0x00943CF0 (:2855)
    bool is_equal(const connection_address& other_addr); // 0x00943D50 (:2860)
    void append_to_packet(unsigned char* packet_data, unsigned short& packet_size) const; // 0x00943D60 (:2865)
    bool read_from_packet(unsigned char* packet_data, int& packet_offset); // 0x00943DE0 (:2870)
    const uint64_t get_id_uint64_debug() const;

    unit4::RemoteUserId connection_id; // 0x00
};

// size 0xC8
struct mp_transmit_list
{
    mp_transmit_list(); // 0x00941DB0 (.h:91)

    int num_transmit_groups; // 0x00
    int num_transmit_connections; // 0x04
    multi_connection* connections[48]; // 0x08
};

// size 0x3BF8
class multi_connection
{
public:
    multi_connection(); // 0x0098E000 (.h:302)
    ~multi_connection(); // 0x00970660 (.h:303)
    bool possibly_using_standby_hack() const; // 0x00941DF0 (.h:305)

    connection_address addr; // 0x00
    bool allocated; // 0x0A
    bool registered; // 0x0B
    multi_player_uid player_uid; // 0x0C
    multi_player_uid reserver_uid; // 0x17
    multi_session* owner; // 0x24
    unsigned char index_in_session; // 0x28
    int local_nonce; // 0x2C
    unsigned int connection_req_id; // 0x30
    multi_connection_status connection_status; // 0x34
    multi_connection_disconnect_reason disconnect_reason; // 0x38
    bool was_invited; // 0x3C
    int connection_retry_count; // 0x40
    int join_time; // 0x44
    unsigned int game_data; // 0x48
    unsigned int last_update_time; // 0x4C
    timestamp_realtime last_flood_test_time; // 0x50
    timestamp standby_ignore_time; // 0x54
    bool joining_in_progress; // 0x58
    packet_tracking tracked_packets; // 0x5C
    reliable_in_order_data reliable_ordered_data; // 0x3330
    object_pool_client<reliable_data,1> m_reliable_data; // 0x3438
    unsigned short next_ack_id; // 0x3458
    unsigned char unreliable_buffer[1200]; // 0x345A
    unsigned short unreliable_buffer_size; // 0x390A
    unsigned int next_reliable_identifier; // 0x390C
    ack_tracker ack_queue[32]; // 0x3910
    unsigned char ack_queue_count; // 0x3990
    unsigned short ack_stored[128]; // 0x3992
    unsigned char ack_stored_count; // 0x3A92
    unsigned int upstream_bits_sec; // 0x3A94
    unsigned int game_upstream_bits_sec; // 0x3A98
    unsigned int calculated_upstream_bits_sec; // 0x3A9C
    unsigned short cannot_serve; // 0x3AA0
    unsigned char last_upstream_adjustment; // 0x3AA2
    dc_object* dc_objects; // 0x3AA4
    timestamp clock_update_timestamp; // 0x3AA8
    unsigned int interpolation_latency_ms; // 0x3AAC
    timestamp packet_loss_disable_timestamp; // 0x3AB0
    unsigned char ping_pos; // 0x3AB4
    unsigned char qos_inited; // 0x3AB5
    int pings[10]; // 0x3AB8
    unsigned int num_ping_samples; // 0x3AE0
    unsigned int mean_ping; // 0x3AE4
    timestamp_realtime qos_disable_timestamp; // 0x3AE8
    timestamp_realtime peer_heartbeat_timer; // 0x3AEC
    available_bandwidth available_bandwidth_tracker; // 0x3AF0
    bool previously_observed_disconnect; // 0x3BE8
    multi_connection* next; // 0x3BEC
    multi_connection* prev; // 0x3BF0
};

template<typename T, int N>
class multi_pool;

extern multi_pool<multi_connection, 48> Connection_pool; // 0x01691640, all connections (multi_connection.cpp :168)

void unused_dcf_mp_release_final_timeouts(); // 0x009433D0 (:295)
void register_packet_ack(multi_connection* cp, unsigned short packet_ack_id); // 0x00971870 (:726)
bool connection_has_unsent_reliable_data(multi_connection* cp); // 0x0098EA60 (:867)
int multi_connection_get_connection_req_id_count(unsigned int req_id); // 0x0098ED00 (:1129)
void multi_core_cheat_detect_shutdown(); // 0x00943600 (:1144)
void multi_core_cheat_detect_start(); // 0x00943610 (:1149)
bool multi_core_cheat_detection_enabled(); // 0x009A82F0 (:1154)
bool multi_core_cheat_detection_should_check(); // 0x00943620 (:1169)
void multi_connection_init(); // 0x009D7DA0 (:1202)
void multi_connection_shutdown(); // 0x009B5190 (:1224)
reliable_in_order_buffer* multi_core_get_free_reliable_in_order_buffer(); // 0x00943650 (:1230)
void multi_core_release_reliable_in_order_buffer(reliable_in_order_buffer* buf); // 0x009436C0 (:1240)
void multi_connection_process(const mp_transmit_list& transmit_list); // 0x009B51A0 (:1247)
void multi_connection_activate_connection(multi_connection* cp); // 0x00943710 (:1253)
void multi_connection_update_connection_pending_status(multi_connection* cp); // 0x00943720 (:1260)
multi_connection* multi_connection_allocate(const connection_address* address, multi_session* owner, const multi_player_uid* player_uid, const multi_player_uid* reserver_uid, bool to_host, bool to_me); // 0x009A8490 (:1279)
void multi_connection_delete(multi_connection* cp); // 0x009B51C0 (:1443)
multi_connection* multi_session_game_host_connection(); // 0x009A8720 (:1490)
multi_connection* multi_find_connection_by_addr(const connection_address& from_conn); // 0x0098EDB0 (:1506)
unsigned int multi_connection_get_index(const multi_connection* cp); // 0x00943740 (:1530)
void multi_connection_update_latencies(); // 0x0098EE20 (:1538)
void multi_connection_disable_qos(multi_connection* cp, int num_ms_to_disable); // 0x00943750 (:1550)
bool multi_connection_is_valid_unreliable_packet(multi_connection* cp, const unsigned char* data, unsigned int len); // 0x0098EED0 (:1563)
bool multi_connection_add_unreliable_packet_internal(multi_connection* cp, const unsigned char* packet, unsigned int packet_size); // 0x0098EFA0 (:1615)
bool multi_connection_is_valid_reliable_packet(multi_connection* cp, const unsigned char* data, unsigned int len); // 0x0098F020 (:1669)
unsigned int multi_connection_add_reliable_packet_internal(multi_connection* cp, const unsigned char* packet, const unsigned int packet_size, void (*callback)(void*), void* cb_data); // 0x009B5380 (:1730)
unsigned int multi_connection_space_used_in_network_packet(multi_connection* cp, unsigned int max_size); // 0x0098F0D0 (:1794)
unsigned int multi_connection_space_left_in_network_packet(multi_connection* cp, unsigned int max_size); // 0x0098F0F0 (:1807)
unsigned int multi_connection_space_left_in_network_packet_physical(multi_connection* cp); // 0x00943790 (:1818)
bool multi_connection_massage_send_data(const unsigned char* in_buf, int in_length, unsigned char* out_buf, int out_length, int& length, unsigned char* voice_out, int& voice_length); // 0x009437B0 (:1824)
void prioritize_unreliable_buffer(multi_connection* cp); // 0x009437F0 (:1867)
bool multi_connection_flush_current_packet_no_sync(multi_connection* cp); // 0x0098F120 (:1917)
bool multi_connection_flush_current_packet(multi_connection* cp); // 0x0098F4E0 (:2172)
bool multi_connection_test_connection(multi_connection* cp); // 0x0098F520 (:2195)
int multi_connection_get_connection_timeout(); // 0x00943950 (:2227)
void multi_connection_disconnect(multi_connection* cp, multi_connection_disconnect_reason reason); // 0x00943960 (:2232)
bool multi_connection_is_connected(const multi_connection* cp); // 0x00943990 (:2261)
void multi_connection_set_gameplay(multi_connection* cp); // 0x009439B0 (:2266)
void multi_connection_end_gameplay(multi_connection* cp); // 0x009439D0 (:2275)
void multi_connection_queue_ack_for_send(multi_connection* cp, unsigned short ack_id); // 0x009439F0 (:2289)
void multi_connection_poll_for_acks(multi_connection* cp, unsigned int max_size); // 0x009B54F0 (:2366)
void multi_core_connection_process_ack_reply(unsigned char* packet_data, const sender& from); // 0x009A8740 (:2429)
bool multi_connection_have_received_reliable_ack_id_already(multi_connection* cp, unsigned short ack_id); // 0x00943B00 (:2525)
bool multi_connection_reliable_data_received(multi_connection* cp, unsigned int reliable_id); // 0x0098F5E0 (:2543)
void multi_core_heartbeat(unsigned char* packet_data, const sender& from); // 0x00943B40 (:2562)
void multi_connection_send_cannot_connect(multi_connection* host_cp, multi_connection* target_cp); // 0x009D7F50 (:2567)
void multi_connection_cannot_connect(unsigned char* packet_data, const sender& from); // 0x009E3540 (:2581)
void multi_connection_send_cannot_connect_block(multi_session_base* sp, multi_connection* target_cp); // 0x009D8050 (:2623)
void multi_connection_cannot_connect_block(unsigned char* packet_data, const sender& from); // 0x00971900 (:2661)
void multi_core_process_bull_taco(unsigned char* packet_data, const sender& from); // 0x00943B50 (:2687)
unsigned int multi_core_connection_get_queued_realibles(multi_connection* cp); // 0x009719C0 (:2698)
float multi_core_connection_get_packet_loss(const multi_connection* cp); // 0x00943B60 (:2707)
void multi_connection_packet_received(multi_connection* from_connection, unsigned short packet_ack_id); // 0x009719D0 (:2713)
void multi_connection_process_packet_acks(unsigned char* packet_data, const sender& from); // 0x009719E0 (:2720)
void multi_core_connection_reset_packet_loss(multi_connection* cp); // 0x00943B80 (:2743)
void multi_core_connection_enable_packet_loss_detection(multi_connection* cp); // 0x00943BC0 (:2749)
void multi_core_process_client_using_standby_hack(multi_connection* cp); // 0x009A8960 (:2880)
bool multi_core_is_server_using_standby_hack(); // 0x009A8A10 (:2924)
unsigned int multi_core_connection_get_bandwidth_bits_sec(const multi_connection* cp, bool ignore_drops, unsigned int previous_ms); // 0x00971BC0 (:3115)
float compute_server_packet_loss(multi_connection* me_cp); // 0x009A8B30 (:3368)
bool multi_connection_is_connection_in_transmit_list(const mp_transmit_list& transmit_list, const multi_connection* cp); // 0x00943E50 (:3409)
