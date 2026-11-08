// Original: volition/rfg/code/multi/multi_core/global/multi_core_io.cpp
//
// Packet input and output. Sends go into a connection's unreliable buffer or reliable data (multi_connection.cpp);
// while the send job owns the connections (Multi_connections_acquired), they are snapshotted into a buffer and
// replayed when the job ends. Receives: multi_io_queue_read pulls datagrams from the platform layer (and routes the
// "low level via game link" party packets), multi_io_process_all walks a network packet (station id, tracker id,
// reliable groups with ack ids, unreliable packets), and multi_io_process_packet dispatches one packet through
// Multi_packets after the connection, direction, connection state and cheating checks; in-order packets wait in the
// connection's in-order list until their sequence id is next. Also the bandwidth values (bytes per packet from the
// base bandwidth, the player count and the packet rate) and the send process (object updates and clock packets fill
// each connection's budget, then every buffer is flushed).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py). Globals with a
// dynamic initializer are placed by the initializer's source line (rfg.pdb).

#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_clock.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_game_bandwidth.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_session_base.h"
#include "volition/rfg/code/multi/multi_core/include/multi_join.h"
#include "volition/rfg/code/multi/multi_core/include/multi_leave.h"
#include "volition/rfg/code/multi/multi_core/include/multi_voice.h"
#include "volition/rfg/code/multi/multi_core/include/multi_pool.inl"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_migration.h"
#include "volition/rfg/code/multi/multi_game/multi_game_party.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/mutex.h"
#include "volition/vlib/code/os/os.h"
#include "volition/vlib/code/os/scheduler.h"
#include "volition/vlib/code/os/vconsole.h"
#include "kaiko/gamelink/gamelink.hpp"
#include "keen/base/trace.hpp"

#include <winsock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- file-private types (rfg.pdb)
// size 0x8
struct low_level_message_via_game_link_package_def
{
    unsigned char packet_type; // 0x00
    void (*receive_func)(const unsigned char*, unsigned short, const connection_address&); // 0x04
};

// ---- globals
unsigned int Force_send_rate_ms = 100; // 0x0167DD60
unsigned int Simulate_n_players_online = 1; // 0x0167DD74, a console override of the player count for the bandwidth
volatile unsigned int Multi_connections_acquired; // 0x02FEB5E8, 1 while the send job owns the connections
// The snapshotted sends (headers and their data) and the send job's lock: file statics, defined here by their
// initializers' lines and used by earlier files of the unity (here with external linkage, see multi.cpp).
multi_pool<snapshot_packet_header,500> Snapshot_packet_headers; // 0x01691A80 (:230)
mempool Buffered_packet_data; // 0x02FEBB78 (:231)
cs_wrapper Core_io_send_job_wrapper(0); // 0x02FEB890 (:233)

// ---- file-static globals
// File statics of later files of the unity used here (rfg.exe: extern before the static definition; see multi.cpp).
extern bool Multi_game_has_skipped_game_migration; // 0x02FEB6C9, multi_game.cpp
static unsigned int Max_rate_sec = 0xA; // 0x0167DD64
static unsigned int Base_bandwidth_kbits = 0x40; // 0x0167DD68
static unsigned int Base_bandwidth_bytes = 0x2000; // 0x0167DD6C
// unsigned int Max_data_per_packet (0x0167DD70) is used here too; placed in multi_connection.cpp.
// reliable_in_order_buffer* Free_reliable_in_order_buffer_list (0x02FE8DC4) is used here too; placed in multi_connection.cpp.
// int Base_score_penalty_for_vetoing (0x02FEB37C) is used here too; placed in multi_core_delta_compression.cpp.
// unsigned int Clock_current_time (0x02FEB538) is used here too; placed in multi.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
static unsigned char Multi_io_read_count; // 0x02FEB5DD
static unsigned char* Multi_io_read_buffer; // 0x02FEB5E0
// migration_state Migration_state (0x02FEB6C4) is used here too; placed in multi.cpp.


// ---- internal functions
static void multi_core_snapshot_packet_data_init(mempool_base& pool); // 0x009B58F0 (:302)
static void multi_core_snapshot_packet(multi_connection* cp, bool reliable, unsigned char* packet_data, unsigned short packet_size, void (*callback)(void*), void* callback_data, unsigned int* p_reliable_id); // 0x009A9580 (:315)
static void send_snapshot_data(snapshot_packet_header* header); // 0x009D9070 (:340)
static void multi_core_process_snapshot_packet_and_reset(); // 0x009D90E0 (:358)
static unsigned int multi_io_send_reliable_internal(multi_connection* cp, unsigned char* data, unsigned int len, void (*callback)(void*), void* callback_data, unsigned int* p_reliable_id, bool try_connection_acquire); // 0x009B5960 (:395)
static unsigned int multi_io_send_reliable_no_sync(multi_connection* cp, unsigned char* data, unsigned int len, void (*callback)(void*), void* callback_data); // 0x009B5A30 (:459)
static void multi_io_send_internal(multi_connection* cp, unsigned char* data, unsigned int len, bool try_connection_acquire); // 0x009A9620 (:471)
static void multi_io_send_no_sync(multi_connection* cp, unsigned char* data, unsigned int len); // 0x009A9690 (:521)
static bool multi_io_drop_cheating_packet(const packet_def* packet, const multi_connection* cp); // 0x00947850 (:526)
static void spew_reliable_in_order_buffers(); // 0x00947890 (:540)
static bool multi_reliable_in_order_buffer_packet(multi_connection* cp, int group_id, unsigned char* data); // 0x009478A0 (:571)
static void multi_reliable_in_order_process(); // 0x0098F8E0 (:634)
static int multi_io_process_packet(unsigned char* data, connection_address& from_conn, unsigned short ack_id); // 0x0098FB20 (:774)
static bool packet_fits_current_format(unsigned char* data, int len); // 0x00947A00 (:873)
static int multi_io_process_all(unsigned char* data, int len, connection_address& from_conn); // 0x009D9250 (:954)
static void calculate_new_bandwidth_values(); // 0x0098FC20 (:1203)
static int connection_flush_list_sort(const void* arg1, const void* arg2); // 0x00947AA0 (:1248)
static void multi_io_force_send_all_buffered_packets(const mp_transmit_list& transmit_list, bool use_synchronization); // 0x0098FDC0 (:1564)

// File statics of the earlier multi_connection.cpp this file calls.
static unsigned int find_this_frames_reliable_size(multi_connection* cp, unsigned int max_size);

// ---- packet tables
// The core packet types (ids 0x00-0x2D; table dumped from rfg.exe .data on 2026-10-19). multi_init copies them and
// Multi_game_packets into Multi_packets. The owner file is inferred from the .data order (between the
// multi_core_game_bandwidth.cpp statics and the low level game link packets below, which multi_io_queue_read routes).
packet_def Multi_core_packets[46] = { // 0x0167D848
    // {id, receive_func, packet_name, packet_short_name, direction, connection_status, connection_required,
    //  drop_on_cheating, group_id}
    {PT_DATA_REQUEST, multi_session_process_data_request, "data_req", "d req", PD_CLIENT_TO_SERVER, PCS_ANY, false, false, GID_NOT_IN_ORDER}, // 0x009DAEE0
    {PT_DATA_RESPONSE, multi_session_process_data_response, "data_response", "d rsp", PD_SERVER_TO_CLIENT, PCS_ANY, false, false, GID_NOT_IN_ORDER}, // 0x009E52B0
    {PT_GOODBYE, multi_process_goodbye, "goodbye", "bye", PD_BOTH, PCS_ANY, false, false, GID_NOT_IN_ORDER}, // 0x0094B670
    {PT_JOIN_REQUEST, multi_join_process_join_request_packet, "join_req", "j req", PD_CLIENT_TO_SERVER, PCS_ANY, false, false, GID_NOT_IN_ORDER}, // 0x009DB070
    {PT_JOIN_ACCEPT, multi_join_process_join_accept_packet, "join_acc", "j acc", PD_SERVER_TO_CLIENT, PCS_ANY, false, false, GID_CONNECTION_PROCESSING}, // 0x009FC3E0
    {PT_JOIN_DENY, multi_join_process_join_deny_packet, "join_deny", "j dny", PD_SERVER_TO_CLIENT, PCS_ANY, false, false, GID_NOT_IN_ORDER}, // 0x009B7AC0
    {PT_JOIN_NEW_SESSION, multi_join_process_join_new_session_packet, "join_new_session", "j n ses", PD_SERVER_TO_CLIENT, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x009B7CF0
    {PT_JOIN_SUCCESS, multi_join_process_join_success_packet, "join_suc", "j suc", PD_CLIENT_TO_SERVER, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x0094B710
    {PT_JOIN_FAILED, multi_join_process_join_failed_packet, "join_failed", "j fail", PD_BOTH, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x009B7E50
    {PT_JOIN_COMPLETE, multi_join_process_join_complete_packet, "join_complete", "j cmplt", PD_SERVER_TO_CLIENT, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x009B7FB0
    {PT_RESERVATION_REMOVE, multi_join_process_remove_reservation, "remove_reserv", "rem rsv", PD_BOTH, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x00973F80
    {PT_NEW_CONNECTION, multi_join_process_new_connection_packet, "new_conn", "new cn", PD_SERVER_TO_CLIENT, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x009AA910
    {PT_NEW_GAME_DATA, multi_join_process_new_game_data_packet, "new_gd", "new gd", PD_SERVER_TO_CLIENT, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x00974020
    {PT_UPDATED_CONNECTION, multi_join_process_updated_connection_packet, "updated_conn", "up cn", PD_SERVER_TO_CLIENT, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x00973FD0
    {PT_CONNECTION_LEFT, multi_leave_process_connection_left_packet, "conn_left", "cn lft", PD_SERVER_TO_CLIENT, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x009DB690
    {PT_REMOVE_GAME_DATA, multi_join_process_remove_game_data_packet, "rm_gd", "rm gd", PD_SERVER_TO_CLIENT, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x00974060
    {PT_REMOTE_OBJECT_CREATE, multi_core_object_process_create_packet, "remote_object_create", "rem cr", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009DA120
    {PT_REMOTE_OBJECT_DESTROY, multi_core_object_process_destroy_packet, "remote_object_destroy", "rem dst", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009DA1C0
    {PT_REMOTE_OBJECT_UPDATE, multi_core_object_process_update_packet, "remote_object_update", "rem up", PD_SERVER_TO_CLIENT, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009E3CD0
    {PT_UPDATE_ACK_LIST, multi_core_dc_process_ack_list, "dc_process_ack_list", "dc ackl", PD_CLIENT_TO_SERVER, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x009D82C0
    {PT_QOS, multi_join_process_qos_packet, "qos", "qos", PD_BOTH, PCS_CONNECTED_GAMEPLAY, true, false, GID_NOT_IN_ORDER}, // 0x00972C90
    {PT_CLOCK, multi_core_process_clock_packet, "clock", "clk", PD_SERVER_TO_CLIENT, PCS_ANY, true, false, GID_NOT_IN_ORDER}, // 0x00971E10
    {PT_FORCE_MIGRATE, multi_process_force_migrate_packet, "force_migrate", "fmgt", PD_SERVER_TO_CLIENT, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x009F7C50
    {PT_MIGRATE, multi_process_migrate_packet, "migrate", "mgt", PD_BOTH, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x009B6FA0
    {PT_MIGRATED, multi_process_migrated_packet, "migrated", "mgtd", PD_BOTH, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x009737F0
    {PT_MIGRATION_COMPLETE, multi_game_migration_complete, "migration complete", "mgt comp", PD_SERVER_TO_CLIENT, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x00999BA0
    {PT_MODIFY_SESSION, multi_process_modify_session_packet, "modify session", "mod ss", PD_SERVER_TO_CLIENT, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x009AA7D0
    {PT_MODIFIED_SESSION, multi_process_modified_session_packet, "modified session", "modd ss", PD_CLIENT_TO_SERVER, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x00973840
    {PT_REGISTER, multi_process_register_for_arbitration, "register session", "reg ss", PD_SERVER_TO_CLIENT, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x009B7400
    {PT_REGISTERED, multi_process_registered_for_arbitration, "registered session", "regd ss", PD_CLIENT_TO_SERVER, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x00973890
    {PT_START, multi_process_start, "start session", "start ss", PD_SERVER_TO_CLIENT, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x009B7510
    {PT_STARTED, multi_process_started, "started session", "startd ss", PD_CLIENT_TO_SERVER, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x009738E0
    {PT_END, multi_process_end, "end session", "end ss", PD_SERVER_TO_CLIENT, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x009FC3A0
    {PT_BANDWIDTH_INFO, multi_core_qos_bandwidth_info, "qos_bandwidth_info", "qos_nfo", PD_CLIENT_TO_SERVER, PCS_ANY, true, false, GID_NOT_IN_ORDER}, // 0x009E4080
    {PT_ACK_REPLY, multi_core_connection_process_ack_reply, "reliable ack", "rel_ack", PD_BOTH, PCS_ANY, false, false, GID_NOT_IN_ORDER}, // 0x009A8740
    {PT_HEARTBEAT, multi_core_heartbeat, "heartbeat", "hbeat", PD_BOTH, PCS_ANY, true, false, GID_NOT_IN_ORDER}, // 0x00943B40
    {PT_CANNOT_CONNECT, multi_connection_cannot_connect, "cannot connect", "cant con", PD_CLIENT_TO_SERVER, PCS_ANY, true, false, GID_NOT_IN_ORDER}, // 0x009E3540
    {PT_CANNOT_CONNECT_BLOCK, multi_connection_cannot_connect_block, "cannot connect block", "no cn bl", PD_SERVER_TO_CLIENT, PCS_ANY, true, false, GID_CONNECTION_PROCESSING}, // 0x00971900
    {PT_YOU_ARE_NEXT_SERVER, multi_process_next_server_packet, "you are next", "UR nxt", PD_SERVER_TO_CLIENT, PCS_ANY, true, false, GID_NOT_IN_ORDER}, // 0x0094B690
    {PT_BULL_TACO, multi_core_process_bull_taco, "bull_taco space filler", "BS", PD_SERVER_TO_CLIENT, PCS_ANY, false, false, GID_NOT_IN_ORDER}, // 0x00943B50
    {PACKET_ACK, multi_connection_process_packet_acks, "packet ack", "pack ack", PD_BOTH, PCS_ANY, false, false, GID_NOT_IN_ORDER}, // 0x009719E0
    {PT_BW_TESTER, multi_game_process_bw_tester, "bw_tester", "bw tst", PD_BOTH, PCS_ANY, true, false, GID_NOT_IN_ORDER}, // 0x009476C0
    {PT_BW_TEST_INFO, multi_game_process_bw_test_info, "bw_test_info", "bw tnfo", PD_BOTH, PCS_ANY, true, false, GID_FLOOD_INFO}, // 0x009476F0
    {PT_MATCH_QUERY, multi_game_lobby_process_match_query, "match_query", "mq", PD_SERVER_TO_CLIENT, PCS_ANY, true, false, GID_NOT_IN_ORDER}, // 0x0097C050
    {PT_PLAYLIST_REQUEST, multi_game_lobby_process_playlist_request, "playlist_request", "plr", PD_CLIENT_TO_SERVER, PCS_ANY, true, false, GID_NOT_IN_ORDER}, // 0x00956C80
    {PT_INVITE_TYPE, multi_game_lobby_process_invite_type, "invite_type", "mq", PD_SERVER_TO_CLIENT, PCS_ANY, true, false, GID_NOT_IN_ORDER}, // 0x00956C90
};

// The two "low level via game link" party lobby packets (magic 0x3210DEAD, see multi_io_queue_read).
low_level_message_via_game_link_package_def Multi_core_low_level_game_link_packets[2] = { // 0x0167DD50
    {LLPT_PARTY_LOBBY_DATA_UPDATE, multi_game_party_lobby_data_update_received},
    {LLPT_PARTY_LOBBY_REQUEST_UPDATE, multi_game_party_lobby_data_update_request_received},
};

// 0x00947730 (:244): console command "mp_display_bandwidth_settings"; rfg.exe formats into the thread-local scratch
// buffer (TLS + 0x38)
void unused_dcf_mp_display_bandwidth_settings()
{
    if (Console_command == 0)
    {
        return;
    }
    char text[0x400];
    sprintf(text, "kbits/second: %u\n", Base_bandwidth_kbits);
    console_output(text, nullptr, false);
    sprintf(text, "bytes/second: %u\n", Base_bandwidth_bytes);
    console_output(text, nullptr, false);
    sprintf(text, "packet/second: %u\n", Max_rate_sec);
    console_output(text, nullptr, false);
    sprintf(text, "bytes/packet: %u\n", Max_data_per_packet);
    console_output(text, nullptr, false);
}

// 0x009A9520 (:258): console command "mp_set_bandwidth <kbits>" (32 to 1024); 128 bytes per kbit as in rfg.exe
void unused_dcf_mp_set_bandwidth()
{
    if (Console_command == 0)
    {
        return;
    }
    console_get_arg(8, false);
    if ((unsigned int)(Console_arg_int - 32) < 0x3E1)
    {
        Base_bandwidth_kbits = Console_arg_int & 0x3FFFFF;
        Base_bandwidth_bytes = (Console_arg_int & 0x3FFFFFu) << 7;
        calculate_new_bandwidth_values();
        console_do_command("mp_display_bandwidth_settings");
        return;
    }
    console_output("Bandwidth must fall between 32 and 1024 kbits", nullptr, false);
}

// 0x009477D0 (:274): console command "mp_set_packet_rate <n>" (1 to 50 per second). Magic numbers: 15 packets'
// worth per second, 45 bytes of overhead.
void unused_dcf_mp_set_packet_rate()
{
    if (Console_command == 0)
    {
        return;
    }
    console_get_arg(8, false);
    if ((unsigned int)(Console_arg_int - 1) < 0x32)
    {
        Force_send_rate_ms = 1000u / (unsigned int)Console_arg_int;
        Max_rate_sec = Console_arg_int;
        Max_data_per_packet = Base_bandwidth_bytes / 15 / (unsigned int)Console_arg_int - 45;
        console_do_command("mp_display_bandwidth_settings");
        return;
    }
    console_output("Packet rate must fall between 1 and 50 packets per second", nullptr, false);
}

// 0x009B58F0 (:302): 500 headers and 64000 bytes of snapshotted packet data
static void multi_core_snapshot_packet_data_init(mempool_base& pool)
{
    Snapshot_packet_headers.init(pool);
    void* mem = pool.alloc(64000, 0);
    Buffered_packet_data.init(mem, 64000, "buffered packet", 1, 8);
    Buffered_packet_data.clear();
    Multi_connections_acquired = 0;
}

// 0x009A9580 (:315): a send made while the send job owns the connections is kept until the job ends
static void multi_core_snapshot_packet(multi_connection* cp, bool reliable, unsigned char* packet_data, unsigned short packet_size,
    void (*callback)(void*), void* callback_data, unsigned int* p_reliable_id)
{
    Buffered_packet_data.set_thread_ownership();
    snapshot_packet_header* header = Snapshot_packet_headers.alloc(0);
    if (header == nullptr)
    {
        keen::debug::nativePrintArgs("out of snapshot headers");
    }
    header->cp = cp;
    header->packet_size = packet_size;
    header->cp_local_nonce = cp->local_nonce;
    header->type = reliable ? 1 : 0;
    header->p_reliable_id = p_reliable_id;
    header->callback = callback;
    header->callback_data = callback_data;
    header->packet_data = (unsigned char*)Buffered_packet_data.alloc(packet_size, 1);
    if (header->packet_data == nullptr)
    {
        keen::debug::nativePrintArgs("out of snapshot buffer data");
    }
    memcpy(header->packet_data, packet_data, packet_size);
}

// 0x009D9070 (:340): a snapshotted send, unless its connection was reused since (the nonce)
static void send_snapshot_data(snapshot_packet_header* header)
{
    multi_connection* cp = header->cp;
    if (cp == nullptr || cp->local_nonce != header->cp_local_nonce)
    {
        return;
    }
    if (header->type == 1)
    {
        const unsigned int reliable_id = multi_io_send_reliable_no_sync(cp, header->packet_data, header->packet_size, header->callback, header->callback_data);
        if (header->p_reliable_id != nullptr)
        {
            *header->p_reliable_id = reliable_id;
        }
    }
    else if (multi_connection_is_valid_unreliable_packet(cp, header->packet_data, header->packet_size))
    {
        multi_connection_add_unreliable_packet_internal(cp, header->packet_data, header->packet_size);
    }
}

// 0x009D90E0 (:358): the snapshotted sends in order, then the buffer is emptied. The send job's lock entered here is
// left by multi_io_send_process_job.
static void multi_core_process_snapshot_packet_and_reset()
{
    Core_io_send_job_wrapper.enter();
    Buffered_packet_data.set_thread_ownership();
    const int count = (int)Snapshot_packet_headers.get_count(0);
    if (count > 0)
    {
        snapshot_packet_header* header = Snapshot_packet_headers.get_first(0);
        send_snapshot_data(header);
        for (int i = 1; i < count; i++)
        {
            header = Snapshot_packet_headers.get_next(header);
            send_snapshot_data(header);
        }
    }
    Snapshot_packet_headers.thread_access.enter();
    for (snapshot_packet_header* header = Snapshot_packet_headers.get_first(0); header != nullptr; header = Snapshot_packet_headers.free(header))
    {
    }
    Snapshot_packet_headers.thread_access.leave();
    Buffered_packet_data.clear();
}

// 0x009B5960 (:395): in-order packets get their group's next sequence id at bytes 3-4
static unsigned int multi_io_send_reliable_internal(multi_connection* cp, unsigned char* data, unsigned int len, void (*callback)(void*),
    void* callback_data, unsigned int* p_reliable_id, bool try_connection_acquire)
{
    if (!multi_connection_is_valid_reliable_packet(cp, data, len))
    {
        return 0xFFFFFFFFu;
    }
    if (try_connection_acquire && InterlockedCompareExchange((volatile LONG*)&Multi_connections_acquired, 1, 0) == 1)
    {
        multi_core_snapshot_packet(cp, true, data, (unsigned short)len, callback, callback_data, p_reliable_id);
        return 0xFFFFFFF0u; // Magic number: the "snapshotted" reliable id
    }
    const unsigned int group_id = Multi_packets[data[0]].group_id;
    if (group_id != 0)
    {
        const unsigned short sequence_id = cp->reliable_ordered_data.next_send_sequence_id[group_id]++;
        data[3] = (unsigned char)(sequence_id >> 8);
        data[4] = (unsigned char)sequence_id;
    }
    const unsigned int reliable_id = multi_connection_add_reliable_packet_internal(cp, data, len, callback, callback_data);
    if (try_connection_acquire)
    {
        Multi_connections_acquired = 0;
    }
    return reliable_id;
}

// 0x009B5A30 (:459)
static unsigned int multi_io_send_reliable_no_sync(multi_connection* cp, unsigned char* data, unsigned int len, void (*callback)(void*), void* callback_data)
{
    if (!multi_connection_is_valid_reliable_packet(cp, data, len))
    {
        return 0xFFFFFFFFu;
    }
    const unsigned int group_id = Multi_packets[data[0]].group_id;
    if (group_id != 0)
    {
        const unsigned short sequence_id = cp->reliable_ordered_data.next_send_sequence_id[group_id]++;
        data[3] = (unsigned char)(sequence_id >> 8);
        data[4] = (unsigned char)sequence_id;
    }
    return multi_connection_add_reliable_packet_internal(cp, data, len, callback, callback_data);
}

// 0x009A9620 (:471)
static void multi_io_send_internal(multi_connection* cp, unsigned char* data, unsigned int len, bool try_connection_acquire)
{
    if (!multi_connection_is_valid_unreliable_packet(cp, data, len))
    {
        return;
    }
    if (try_connection_acquire && InterlockedCompareExchange((volatile LONG*)&Multi_connections_acquired, 1, 0) == 1)
    {
        multi_core_snapshot_packet(cp, false, data, (unsigned short)len, nullptr, nullptr, nullptr);
        return;
    }
    multi_connection_add_unreliable_packet_internal(cp, data, len);
    if (try_connection_acquire)
    {
        Multi_connections_acquired = 0;
    }
}

// 0x009A9690 (:521)
static void multi_io_send_no_sync(multi_connection* cp, unsigned char* data, unsigned int len)
{
    if (multi_connection_is_valid_unreliable_packet(cp, data, len))
    {
        multi_connection_add_unreliable_packet_internal(cp, data, len);
    }
}

// 0x00947850 (:526): packets of a connection flagged as a standby user are dropped while the flag runs
static bool multi_io_drop_cheating_packet(const packet_def* packet, const multi_connection* cp)
{
    if (!packet->drop_on_cheating || cp == nullptr)
    {
        return false;
    }
    return cp->standby_ignore_time.valid() && !cp->standby_ignore_time.elapsed();
}

// 0x00947890 (:540): debug output, empty in the retail build
static void spew_reliable_in_order_buffers()
{
}

// 0x009478A0 (:571): the packet waits in the connection's in-order list, sorted by group and sequence id
static bool multi_reliable_in_order_buffer_packet(multi_connection* cp, int group_id, unsigned char* data)
{
    reliable_in_order_buffer* buf = multi_core_get_free_reliable_in_order_buffer();
    if (buf == nullptr)
    {
        return false;
    }
    const unsigned short sequence_id = (unsigned short)(data[3] << 8 | data[4]);
    const int size = ((data[1] << 8) | data[2]) + 3;
    if (size > 0x200)
    {
        keen::debug::nativePrintArgs("In-order reliable packet is too large, either compress it or bump MAX_RELIABLE_IN_ORDER_PACKET_SIZE");
        multi_core_release_reliable_in_order_buffer(buf);
        return false;
    }
    if (group_id >= 0x40)
    {
        keen::debug::nativePrintArgs("In-order reliable packet group id is too large, either decrease it or bump MAX_RELIABLE_IN_ORDER_GROUPS");
        multi_core_release_reliable_in_order_buffer(buf);
        return false;
    }
    buf->group_id = (unsigned short)(group_id & 0xFF);
    buf->sequence_id = sequence_id;
    buf->debug_recv_time = Frametime_total_milliseconds_increment_always;
    buf->debug_framecount = Framecount;
    memcpy(buf->packet_data, data, size);
    reliable_in_order_buffer* head = cp->reliable_ordered_data.list;
    for (reliable_in_order_buffer* node = head; node != nullptr;)
    {
        if (group_id < (int)node->group_id || (group_id == (int)node->group_id && sequence_id < node->sequence_id))
        {
            node->prev->next = buf;
            buf->prev = node->prev;
            node->prev = buf;
            buf->next = node;
            if (cp->reliable_ordered_data.list == node)
            {
                cp->reliable_ordered_data.list = buf;
            }
            return true;
        }
        node = node->next;
        if (node == head)
        {
            break;
        }
    }
    if (head == nullptr)
    {
        buf->next = buf;
        buf->prev = buf;
        cp->reliable_ordered_data.list = buf;
        return true;
    }
    buf->prev = head->prev;
    buf->next = cp->reliable_ordered_data.list;
    cp->reliable_ordered_data.list->prev->next = buf;
    cp->reliable_ordered_data.list->prev = buf;
    return true;
}

// 0x0098F8E0 (:634): per connection, the in-order packets whose sequence id is next are handled (when the connection
// state allows it) and released; the others wait
static void multi_reliable_in_order_process()
{
    Connection_pool.thread_access.enter();
    for (multi_connection* cp = Connection_pool.get_first(0); cp != nullptr; cp = Connection_pool.get_next(cp))
    {
        reliable_in_order_buffer* buf = cp->reliable_ordered_data.list;
        while (buf != nullptr)
        {
            reliable_in_order_buffer* following = buf->next != cp->reliable_ordered_data.list ? buf->next : nullptr;
            unsigned short& expected = cp->reliable_ordered_data.next_expected_recv_sequence_id[buf->group_id];
            if (buf->sequence_id == expected)
            {
                expected++;
                const packet_def* packet = &Multi_packets[buf->packet_data[0]];
                if (multi_io_verify_packet_connection_state(packet, cp))
                {
                    sender from(cp);
                    packet->receive_func(buf->packet_data + 5, from);
                }
                if (cp->reliable_ordered_data.list == buf && buf->next == buf)
                {
                    buf->next = nullptr;
                    buf->prev = nullptr;
                    cp->reliable_ordered_data.list = nullptr;
                }
                else
                {
                    if (cp->reliable_ordered_data.list == buf)
                    {
                        cp->reliable_ordered_data.list = buf->next;
                    }
                    if (buf->prev != nullptr)
                    {
                        buf->prev->next = buf->next;
                    }
                    if (buf->next != nullptr)
                    {
                        buf->next->prev = buf->prev;
                    }
                    buf->next = nullptr;
                    buf->prev = nullptr;
                }
                multi_core_release_reliable_in_order_buffer(buf);
            }
            buf = following;
        }
    }
    Connection_pool.thread_access.leave();
}

// 0x009A96C0 (:704): the next game datagram (the first 8 bytes are the sender's station id, which becomes from_conn);
// the "low level via game link" party packets (magic 0x3210DEAD, type, size at 6, data at 8) are handled here.
// Returns 0 when nothing is queued.
int multi_io_queue_read(void* data, connection_address& from_conn)
{
    unsigned char buffer[1200];
    for (;;)
    {
        bool low_level = false;
        Multi_thread_mutex.lock(true);
        const unsigned int size = multi_core_platform_internal_receive_packet(from_conn, buffer, sizeof(buffer));
        if (size > 3 && *(unsigned int*)buffer == 0x3210DEAD)
        {
            low_level = true;
            if (buffer[4] < LLPT_NUM_PACKAGES)
            {
                Multi_core_low_level_game_link_packets[buffer[4]].receive_func(buffer + 8, *(unsigned short*)(buffer + 6), from_conn);
            }
        }
        Multi_thread_mutex.unlock();
        if (size == 0)
        {
            return 0;
        }
        if (!low_level)
        {
            memcpy(from_conn.connection_id.providerData, buffer, 8);
            from_conn.connection_id.isValid = true;
            multi_find_connection_by_addr(from_conn);
            memcpy(data, buffer, size);
            return (int)size;
        }
    }
}

// 0x0098FB20 (:774): one packet (type, size, payload) through the checks to its handler; returns its size. In-order
// packets are queued instead. Original bug kept: a type of 0x80 passes the range check (Multi_packets has 128 entries).
static int multi_io_process_packet(unsigned char* data, connection_address& from_conn, unsigned short)
{
    multi_connection* cp = multi_find_connection_by_addr(from_conn);
    sender from(cp, cp != nullptr ? &cp->addr : &from_conn);
    const unsigned char type = data[0];
    const int size = ((data[1] << 8) | data[2]) + 3;
    if (type > 0x80)
    {
        return size;
    }
    const packet_def* packet = &Multi_packets[type];
    if (packet->connection_required && cp == nullptr)
    {
        return size;
    }
    if (cp != nullptr && cp->owner != nullptr && (cp->owner->get_state() == MSS_DELETING || cp->owner->get_state() == MSS_NONE))
    {
        return size;
    }
    if (!multi_io_verify_packet_direction_receive(packet, &from))
    {
        return size;
    }
    if (packet->group_id != 0)
    {
        multi_reliable_in_order_buffer_packet(cp, packet->group_id, data);
        return size;
    }
    if (multi_io_verify_packet_connection_state(packet, cp) && !multi_io_drop_cheating_packet(packet, cp))
    {
        Multi_packets[type].receive_func(data + 3, from);
    }
    return size;
}

// 0x00947A00 (:873): the network packet after the station id parses to exactly len bytes: the tracker id, reliable
// groups (ack id, count, packets) up to an ack id of 0xFFFF, then whole packets
static bool packet_fits_current_format(unsigned char* data, int len)
{
    if (len <= 2)
    {
        return false;
    }
    int offset = 2;
    for (;;)
    {
        if (offset + 2 > len)
        {
            return false;
        }
        const unsigned short ack_id = (unsigned short)(data[offset] << 8 | data[offset + 1]);
        offset += 2;
        if (ack_id == 0xFFFF)
        {
            while (offset < len)
            {
                if (offset + 3 > len)
                {
                    return false;
                }
                offset += ((data[offset + 1] << 8) | data[offset + 2]) + 3;
                if (offset > len)
                {
                    return false;
                }
            }
            return offset == len;
        }
        if (offset + 4 > len)
        {
            return false;
        }
        const int count = data[offset];
        offset++;
        for (int i = 0; i < count; i++)
        {
            if (offset + 3 > len)
            {
                return false;
            }
            offset += ((data[offset + 1] << 8) | data[offset + 2]) + 3;
            if (offset > len)
            {
                return false;
            }
        }
        if (offset >= len)
        {
            return false;
        }
    }
}

// 0x009D9250 (:954): a network packet (see multi_connection.cpp): the sender's station id becomes from_conn, its
// tracker id is queued to be acked, each reliable group is handled once (a group already received only answers join
// requests again) and its ack id queued, then the unreliable packets. From a station without a connection only join
// requests and join accepts are looked at; when one makes the connection, the packet is parsed again from the start
// (once).
static int multi_io_process_all(unsigned char* data, int len, connection_address& from_conn)
{
    bool first_pass = true;
restart:
    memcpy(from_conn.connection_id.providerData, data, 8);
    from_conn.connection_id.isValid = true;
    multi_connection* cp = multi_find_connection_by_addr(from_conn);
    if (!packet_fits_current_format(data + 8, len - 8))
    {
        return 0;
    }
    if (cp != nullptr)
    {
        multi_core_process_client_using_standby_hack(cp);
        cp->last_update_time = Clock_current_time;
    }
    multi_connection_packet_received(cp, (unsigned short)(data[8] << 8 | data[9]));
    unsigned short ack_id = (unsigned short)(data[10] << 8 | data[11]);
    int offset = 12;
    while (ack_id != 0xFFFF)
    {
        const int count = data[offset];
        offset++;
        if (count != 0)
        {
            if (cp == nullptr)
            {
                for (int i = 0; i < count; i++)
                {
                    unsigned char* packet = data + offset;
                    const int payload = (packet[1] << 8) | packet[2];
                    if (packet[0] == PT_JOIN_REQUEST)
                    {
                        offset += multi_io_process_packet(packet, from_conn, 0);
                        cp = multi_find_connection_by_addr(from_conn);
                        if (cp != nullptr && first_pass)
                        {
                            first_pass = false;
                            goto restart;
                        }
                    }
                    else if (packet[0] == PT_JOIN_ACCEPT)
                    {
                        // The accept carries the session's lobby id (payload bytes 2-9) and the host's station id
                        // (10-17): the host connection of that session takes the address.
                        multi_session_info info;
                        memcpy(&info.m_lobbyId.id, packet + 5, 8);
                        multi_session* sp = multi_session_find_by_id(info);
                        if (sp != nullptr && sp->get_host_connection() != nullptr)
                        {
                            multi_connection* host = sp->get_host_connection();
                            memcpy(host->addr.connection_id.providerData, packet + 13, 8);
                            host->addr.connection_id.isValid = true;
                            cp = multi_find_connection_by_addr(from_conn);
                            if (cp != nullptr && first_pass)
                            {
                                first_pass = false;
                                goto restart;
                            }
                        }
                        offset += payload + 3;
                    }
                    else
                    {
                        offset += payload + 3;
                    }
                    if (offset > len)
                    {
                        return offset;
                    }
                }
            }
            else if (multi_connection_have_received_reliable_ack_id_already(cp, ack_id))
            {
                // Original behaviour kept: the end check here leaves out the 3 header bytes and returns the offset
                // before the packet.
                for (int i = 0; i < count; i++)
                {
                    const int payload = (data[offset + 1] << 8) | data[offset + 2];
                    if (data[offset] == PT_JOIN_REQUEST)
                    {
                        multi_join_send_join_accept_packet(cp);
                    }
                    if (payload + offset > len)
                    {
                        return offset;
                    }
                    offset += payload + 3;
                }
                multi_connection_queue_ack_for_send(cp, ack_id);
            }
            else
            {
                for (int i = 0; i < count; i++)
                {
                    if (data[offset] == PT_JOIN_REQUEST)
                    {
                        offset += ((data[offset + 1] << 8) | data[offset + 2]) + 3;
                        multi_join_send_join_accept_packet(cp);
                    }
                    else
                    {
                        offset += multi_io_process_packet(data + offset, from_conn, 0);
                    }
                    if (offset > len)
                    {
                        return offset;
                    }
                }
                multi_connection_queue_ack_for_send(cp, ack_id);
            }
        }
        const unsigned short next_ack_id = (unsigned short)(data[offset] << 8 | data[offset + 1]);
        offset += 2;
        if (offset > len)
        {
            return offset;
        }
        ack_id = next_ack_id;
    }
    while (offset < len)
    {
        const int size = multi_io_process_packet(data + offset, from_conn, 0);
        if (size == 0)
        {
            break;
        }
        offset += size;
    }
    return offset;
}

// 0x0098FC20 (:1203): the bytes per packet: the base bandwidth shared by the most players of any session (a client
// sends to one), per packet of the rate, less 45 bytes of overhead, at most 0x3F7 (an underflow becomes 0x3F7 too)
static void calculate_new_bandwidth_values()
{
    int players = 0;
    Session_pool.thread_access.enter();
    for (multi_session* sp = Session_pool.get_first(0); sp != nullptr; sp = Session_pool.get_next(sp))
    {
        const int others = sp->get_slot_use_data().used - 1;
        if (players <= others)
        {
            players = others;
        }
    }
    Session_pool.thread_access.leave();
    if (players <= (int)Simulate_n_players_online)
    {
        players = (int)Simulate_n_players_online;
    }
    if (players < 2)
    {
        players = 1;
    }
    if (Multiplayer && !Server)
    {
        players = 1;
    }
    Max_data_per_packet = Base_bandwidth_bytes / (unsigned int)players / Max_rate_sec - 45;
    if (Max_data_per_packet == 0)
    {
        Max_data_per_packet = 0;
        return;
    }
    if (Max_data_per_packet > 0x3F6)
    {
        Max_data_per_packet = 0x3F7;
    }
}

// 0x00947AA0 (:1248): by the random key of each pair
static int connection_flush_list_sort(const void* arg1, const void* arg2)
{
    const unsigned int a = ((const unsigned int*)arg1)[1];
    const unsigned int b = ((const unsigned int*)arg2)[1];
    if (a == b)
    {
        return 0;
    }
    return a > b ? 1 : -1;
}

// 0x00947AD0 (:1272)
sender::sender(connection_address* addr)
{
    address = addr;
    connection = nullptr;
}

// 0x00947AF0 (:1277)
sender::sender(multi_connection* cp)
{
    address = &cp->addr;
    connection = cp;
}

// 0x00947B00 (:1282)
sender::sender(multi_connection* cp, connection_address* addr)
{
    address = addr;
    connection = cp;
}

// 0x00947B20 (:1286)
unsigned int multi_io_get_max_data_packet()
{
    return Max_data_per_packet;
}

// 0x00947B30 (:1295)
unsigned int multi_io_get_max_rate_per_second()
{
    return Max_rate_sec;
}

// 0x0098FD30 (:1304): magic numbers: the server sends 10 packets per second (every 100 ms), a client 15 (every 66 ms)
unsigned int multi_io_get_packet_rate_per_second()
{
    if (Multiplayer && Server)
    {
        Max_rate_sec = 10;
        Force_send_rate_ms = 100;
    }
    else
    {
        Max_rate_sec = 15;
        Force_send_rate_ms = 66;
    }
    calculate_new_bandwidth_values();
    return Max_rate_sec;
}

// 0x0098FD80 (:1319): the parameter is a rate per second despite its name
void multi_io_set_packet_rate_per_second(unsigned int rate_ms)
{
    Force_send_rate_ms = 1000u / rate_ms;
    Max_rate_sec = rate_ms;
    calculate_new_bandwidth_values();
}

// 0x0098FDA0 (:1332)
void multi_io_set_bandwidth_in_bits_sec(unsigned int bits_sec)
{
    Base_bandwidth_kbits = bits_sec >> 10;
    Base_bandwidth_bytes = bits_sec >> 3;
    calculate_new_bandwidth_values();
}

// 0x009FB440 (:1342): magic numbers: a 16 KB read buffer, 4 sessions
void multi_io_init(mempool_base& pool)
{
    Multi_io_read_buffer = (unsigned char*)pool.alloc(0x4000, 0);
    multi_connection_init();
    Session_pool.init(MP_allocators_mempool);
    multi_core_objects_init();
    multi_core_snapshot_packet_data_init(pool);
}

// 0x009FDA00 (:1359): every session is deleted, then processed until its state is MSS_NONE, and freed
void multi_io_shutdown()
{
    bool pending = false;
    Session_pool.thread_access.enter();
    multi_session* sp = Session_pool.get_first(0);
    while (sp != nullptr)
    {
        multi_session* next = Session_pool.get_next(sp);
        sp->session_delete();
        if (sp->get_state() == MSS_NONE)
        {
            Session_pool.free(sp);
        }
        else
        {
            pending = true;
        }
        sp = next;
    }
    Session_pool.thread_access.leave();
    while (pending)
    {
        pending = false;
        Session_pool.thread_access.enter();
        sp = Session_pool.get_first(0);
        while (sp != nullptr)
        {
            multi_session* next = Session_pool.get_next(sp);
            sp->process();
            if (sp->get_state() == MSS_NONE)
            {
                Session_pool.free(sp);
            }
            else
            {
                pending = true;
            }
            sp = next;
        }
        Session_pool.thread_access.leave();
    }
}

// 0x009D96E0 (:1368): every queued datagram, then the in-order packets that became due
void multi_io_process_incoming()
{
    unsigned char* buffer = Multi_io_read_buffer;
    connection_address from;
    from.init();
    if (buffer == nullptr)
    {
        keen::debug::nativePrintArgs("ZOMG, NULL packet read buffer!");
    }
    Multi_io_read_count++;
    int size = multi_io_queue_read(buffer, from);
    while (size > 0)
    {
        multi_io_process_all(buffer, size, from);
        size = multi_io_queue_read(buffer, from);
    }
    multi_reliable_in_order_process();
    Multi_io_read_count--;
}

// 0x00972690 (:1410): a party packet straight through the game link (magic, type, size, data; see multi_io_queue_read)
void multi_io_send_low_level_message_via_game_link(kaiko::gamelink::GameLink* game_link, unsigned char packet_type, const unsigned char* packet_data,
    unsigned short data_size, const connection_address& connection)
{
    unsigned char buffer[1200];
    memset(buffer, 0, sizeof(buffer));
    *(unsigned int*)buffer = 0x3210DEAD;
    buffer[4] = packet_type;
    *(unsigned short*)(buffer + 6) = data_size;
    memcpy(buffer + 8, packet_data, data_size);
    const kaiko::GameLinkRemoteStation* station = game_link->getRemoteStationById(connection.connection_id);
    if (station != nullptr)
    {
        game_link->sendP2PPacketUnreliable(station, buffer, data_size + 8u);
    }
}

// 0x009A97C0 (:1441): into the connection's unreliable buffer, or snapshotted while the send job runs
void multi_io_send(multi_connection* cp, unsigned char* data, unsigned int len)
{
    Core_io_send_job_wrapper.enter();
    if (multi_connection_is_valid_unreliable_packet(cp, data, len))
    {
        if (InterlockedCompareExchange((volatile LONG*)&Multi_connections_acquired, 1, 0) == 1)
        {
            multi_core_snapshot_packet(cp, false, data, (unsigned short)len, nullptr, nullptr, nullptr);
        }
        else
        {
            multi_connection_add_unreliable_packet_internal(cp, data, len);
            Multi_connections_acquired = 0;
        }
    }
    Core_io_send_job_wrapper.leave();
}

// 0x009A9840 (:1462): every connection of the session but our own (rfg.exe inlines multi_io_send)
void multi_io_send_to_all(multi_session* session, unsigned char* data, unsigned int len)
{
    multi_connection* first = session->get_connection_list();
    multi_connection* cp = first;
    do
    {
        if (cp == nullptr)
        {
            return;
        }
        if (cp != session->get_me_connection())
        {
            multi_io_send(cp, data, len);
        }
        cp = cp->next;
    } while (cp != session->get_connection_list());
}

// 0x009A98F0 (:1483)
void multi_io_send_to_all_except_one(multi_session* session, unsigned char* data, unsigned int len, multi_connection* except)
{
    multi_connection* cp = session->get_connection_list();
    do
    {
        if (cp == nullptr)
        {
            return;
        }
        if (cp != session->get_me_connection() && cp != except)
        {
            multi_io_send(cp, data, len);
        }
        cp = cp->next;
    } while (cp != session->get_connection_list());
}

// 0x009B5AA0 (:1511): returns the reliable buffer's identifier (0xFFFFFFF0 when snapshotted, 0xFFFFFFFF on failure)
unsigned int multi_io_send_reliable(multi_connection* cp, unsigned char* data, unsigned int len, void (*callback)(void*), void* callback_data,
    unsigned int* p_reliable_id)
{
    Core_io_send_job_wrapper.enter();
    if (!multi_connection_is_valid_reliable_packet(cp, data, len))
    {
        Core_io_send_job_wrapper.leave();
        return 0xFFFFFFFFu;
    }
    if (InterlockedCompareExchange((volatile LONG*)&Multi_connections_acquired, 1, 0) == 1)
    {
        multi_core_snapshot_packet(cp, true, data, (unsigned short)len, callback, callback_data, p_reliable_id);
        Core_io_send_job_wrapper.leave();
        return 0xFFFFFFF0u;
    }
    const unsigned int group_id = Multi_packets[data[0]].group_id;
    if (group_id != 0)
    {
        const unsigned short sequence_id = cp->reliable_ordered_data.next_send_sequence_id[group_id]++;
        data[3] = (unsigned char)(sequence_id >> 8);
        data[4] = (unsigned char)sequence_id;
    }
    const unsigned int reliable_id = multi_connection_add_reliable_packet_internal(cp, data, len, callback, callback_data);
    Multi_connections_acquired = 0;
    Core_io_send_job_wrapper.leave();
    return reliable_id;
}

// 0x009B5BA0 (:1525)
void multi_io_send_to_all_reliable(multi_session* session, unsigned char* data, unsigned int len)
{
    if (session == nullptr)
    {
        return;
    }
    multi_connection* cp = session->get_connection_list();
    do
    {
        if (cp == nullptr)
        {
            return;
        }
        if (cp != session->get_me_connection())
        {
            multi_io_send_reliable(cp, data, len, nullptr, nullptr, nullptr);
        }
        cp = cp->next;
    } while (cp != session->get_connection_list());
}

// 0x009B5BF0 (:1547): rfg.exe inlines multi_io_send_reliable
void multi_io_send_to_all_except_one_reliable(multi_session* session, unsigned char* data, unsigned int len, multi_connection* except)
{
    multi_connection* cp = session->get_connection_list();
    do
    {
        if (cp == nullptr)
        {
            return;
        }
        if (cp != session->get_me_connection() && cp != except)
        {
            multi_io_send_reliable(cp, data, len, nullptr, nullptr, nullptr);
        }
        cp = cp->next;
    } while (cp != session->get_connection_list());
}

// 0x0098FDC0 (:1564): every connection's packet goes out, in a random order (vrand keys)
static void multi_io_force_send_all_buffered_packets(const mp_transmit_list& transmit_list, bool use_synchronization)
{
    unsigned int pairs[96];
    size_t count = 0;
    for (int i = 0; i < transmit_list.num_transmit_connections; i++)
    {
        pairs[count * 2] = (unsigned int)(uintptr_t)transmit_list.connections[i];
        pairs[count * 2 + 1] = (unsigned int)vrand();
        count++;
    }
    if ((int)count <= 0)
    {
        return;
    }
    qsort(pairs, count, 8, connection_flush_list_sort);
    for (int i = 0; i < (int)count; i++)
    {
        multi_connection* cp = (multi_connection*)(uintptr_t)pairs[i * 2];
        if (use_synchronization)
        {
            multi_connection_flush_current_packet(cp);
        }
        else
        {
            multi_connection_flush_current_packet_no_sync(cp);
        }
    }
}

// 0x0098FE90 (:1597): every connection but our own ones (the owner is not checked for null, as in rfg.exe)
void multi_io_force_send_all_buffered_packets()
{
    mp_transmit_list transmit_list;
    Connection_pool.thread_access.enter();
    for (multi_connection* cp = Connection_pool.get_first(0); cp != nullptr; cp = Connection_pool.get_next(cp))
    {
        if (cp != cp->owner->get_me_connection())
        {
            transmit_list.connections[transmit_list.num_transmit_connections++] = cp;
        }
    }
    Connection_pool.thread_access.leave();
    multi_io_force_send_all_buffered_packets(transmit_list, true);
}

// 0x00972750 (:1620): server-to-client packets go to anyone but our host, client-to-server ones only to the host
bool multi_io_verify_packet_direction_send(const packet_def* packet, const sender* receiver)
{
    multi_connection* cp = receiver->connection;
    if (cp == nullptr)
    {
        return false;
    }
    if (packet->direction == PD_SERVER_TO_CLIENT)
    {
        return cp->owner == nullptr || cp->owner->get_host_connection() != cp;
    }
    if (packet->direction != PD_CLIENT_TO_SERVER)
    {
        return true;
    }
    return cp->owner != nullptr && cp->owner->get_host_connection() == cp;
}

// 0x00972790 (:1644): a host takes no server-to-client packets, a client no client-to-server ones (unknown senders pass)
bool multi_io_verify_packet_direction_receive(const packet_def* packet, const sender* packet_sender)
{
    if (packet->direction == PD_SERVER_TO_CLIENT)
    {
        return packet_sender->connection == nullptr || packet_sender->connection->owner == nullptr || !packet_sender->connection->owner->am_i_host();
    }
    if (packet->direction != PD_CLIENT_TO_SERVER)
    {
        return true;
    }
    return packet_sender->connection == nullptr || packet_sender->connection->owner == nullptr || packet_sender->connection->owner->am_i_host();
}

// 0x00947B40 (:1667)
bool multi_io_verify_packet_connection_state(const packet_def* packet, const multi_connection* cp)
{
    switch (packet->connection_status)
    {
    case PCS_ANY:
        return true;
    case PCS_CONNECTED_ANY:
        return cp->connection_status == CS_CONNECTED || cp->connection_status == CS_CONNECTED_GAMEPLAY;
    case PCS_CONNECTED_NOT_GAMEPLAY:
        return cp->connection_status == CS_CONNECTED;
    case PCS_CONNECTED_GAMEPLAY:
        return cp->connection_status == CS_CONNECTED_GAMEPLAY;
    default:
        return false;
    }
}

// 0x009727E0 (:1685): object updates go out from the host of a running game session in gameplay, outside migration
bool multi_core_io_allow_object_update_send(const multi_session* game_session)
{
    if (game_session == nullptr || !game_session->am_i_host() || game_session->get_state() != MSS_IN_GAME)
    {
        return false;
    }
    if (gameseq_state_is_active(GS_GAMEPLAY) && Migration_state == MIGRATION_STATE_NONE)
    {
        return !Multi_game_has_skipped_game_migration;
    }
    return false;
}

// 0x00A00E60 (:1711): the gameplay send job (multi_process queues it): waits for the connections, sends, replays the
// sends made meanwhile, and leaves the lock multi_core_process_snapshot_packet_and_reset entered
void multi_io_send_process_job(cpu_job*, cpu_job_arg, cpu_job_arg)
{
    while (InterlockedCompareExchange((volatile LONG*)&Multi_connections_acquired, 1, 0) != 0)
    {
        os_micro_sleep(20); // Magic number: 20 microseconds between tries
    }
    multi_io_send_process(Transmit_list);
    multi_core_process_snapshot_packet_and_reset();
    Multi_connections_acquired = 0;
    Core_io_send_job_wrapper.leave();
}

// 0x009FDA10 (:1732): each connection's budget for this send (the bandwidth slot, the computed upstream per packet,
// less 45 bytes of overhead, the reliable data and 9 header bytes) is filled with clock packets and, from the host of
// a running game, object updates; the registrations clean up, the master ack id advances (skipping 0), every packet
// is flushed, and each connection's bandwidth moves to the next slot (one packet's worth per send at most 0xE000).
void multi_io_send_process(const mp_transmit_list& transmit_list)
{
    multi_session* game_session = multi_session_get_by_type(ST_GAME);
    Base_bandwidth_bytes = (Base_bandwidth_kbits & 0x3FFFFF) << 7;
    Base_bandwidth_kbits &= 0x3FFFFF;
    calculate_new_bandwidth_values();
    for (int i = 0; i < transmit_list.num_transmit_connections; i++)
    {
        multi_connection* cp = transmit_list.connections[i];
        const unsigned int available = cp->available_bandwidth_tracker.get_available_bandwidth();
        unsigned int per_send = (cp->calculated_upstream_bits_sec >> 3) / Max_rate_sec;
        if ((int)available <= (int)per_send)
        {
            per_send = available;
        }
        int space = (int)per_send - 45;
        if (space < 1)
        {
            space = 0;
        }
        space = space - (int)find_this_frames_reliable_size(cp, Max_data_per_packet) - 9;
        if (space < 1)
        {
            space = 0;
        }
        if ((int)cp->unreliable_buffer_size < space)
        {
            multi_core_clock_poll_for_packet(cp, space - cp->unreliable_buffer_size);
        }
        if (cp->owner == game_session && game_session != nullptr && game_session->am_i_host() && game_session->get_state() == MSS_IN_GAME &&
            gameseq_state_is_active(GS_GAMEPLAY) && Migration_state == MIGRATION_STATE_NONE && !Multi_game_has_skipped_game_migration &&
            (int)cp->unreliable_buffer_size < space)
        {
            multi_core_get_object_updates_for_connection(cp, space - cp->unreliable_buffer_size);
        }
    }
    if (game_session != nullptr && game_session->am_i_host() && game_session->get_state() == MSS_IN_GAME && gameseq_state_is_active(GS_GAMEPLAY) &&
        Migration_state == MIGRATION_STATE_NONE && !Multi_game_has_skipped_game_migration)
    {
        for (int i = 0; i < 12; i++)
        {
            if (Object_type_registration_list[i] != nullptr)
            {
                Object_type_registration_list[i]->post_update_cleanup();
            }
        }
        if (++Master_ack_id_number == 0)
        {
            Master_ack_id_number = 1;
        }
    }
    multi_io_force_send_all_buffered_packets(transmit_list, false);
    unsigned int per_packet = Max_data_per_packet + 54; // Magic number: 54 bytes of headers
    if (per_packet > 0x3FF)
    {
        per_packet = 0x400;
    }
    for (int i = 0; i < transmit_list.num_transmit_connections; i++)
    {
        transmit_list.connections[i]->available_bandwidth_tracker.process(Max_rate_sec * per_packet * 8);
    }
}

// 0x0098FF60 (:1838): the same as calculate_new_bandwidth_values
void multi_io_recalculate_bandwidth()
{
    calculate_new_bandwidth_values();
}
