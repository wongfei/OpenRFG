// Original: volition/rfg/code/multi/multi_core/global/multi_connection.cpp
//
// Connections to the other stations: allocation and deletion, the unreliable buffer of the current network packet,
// reliable data (buffers resent every 500 ms until acked, with completion callbacks), in-order reliable buffers,
// packet trackers (ids per sent packet, acked back for latency and loss), the per-slot bandwidth accounting, and the
// standby cheat detection. multi_connection_flush_current_packet_no_sync builds the network packet.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py). Globals with a
// dynamic initializer are placed by the initializer's source line (rfg.pdb).
//
// The network packet (multi_connection_flush_current_packet_no_sync): the sender's station id (8 bytes), the packet
// tracker id (2, big endian), then reliable groups (ack id 2, packet count 1, the packets) up to an ack id of 0xFFFF,
// then the unreliable packets (high priority first). Every packet is type (1), payload size (2, big endian),
// [in-order sequence id (2)], payload.

#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_delta_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_qos.h"
#include "volition/rfg/code/multi/multi_core/include/multi_pool.inl"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_migration.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/mutex.h"
#include "volition/vlib/code/os/scheduler.h"
#include "volition/vlib/code/os/timer.h"
#include "keen/base/trace.hpp"

#include <winsock2.h>
#include <stdio.h>
#include <string.h>


// ---- file-private types (rfg.pdb)
// size 0x6200
struct mp_game_data_wrapper
{
    multi_game_data data[16][4]; // 0x00
};

// ---- globals
multi_pool<multi_connection,48> Connection_pool; // 0x01691640 (:168)

// ---- file-static globals
static bool Allow_cheat_detection = true; // 0x0167D7CB
static int Reliable_packet_disconnect_ms = 20000; // 0x0167D824
static int Floodfill_safety_time = 20500; // 0x0167D828
static unsigned char High_priority_packet_types[3] = {0x28, 0x22, 0x13}; // 0x0167D830, PACKET_ACK, PT_ACK_REPLY, PT_UPDATE_ACK_LIST
static unsigned int Max_data_per_packet = 9; // 0x0167DD70
static multi_pool<reliable_callback,256> Reliable_callback_allocator; // 0x01691680 (:216)
static timestamp Client_standby_hack_check; // 0x016916C0, initial data ffffffff
static unsigned char Transmission_buffer[1024]; // 0x02FE83D0
static reliable_in_order_buffer* Free_reliable_in_order_buffer_list; // 0x02FE8DC4
// unsigned int Clock_current_time (0x02FEB538) is used here too; placed in multi.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
static bool Reliable_half_max_size_limit; // 0x02FEB5B2
static unsigned int Last_ack_time_ms; // 0x02FEB5C4
static int Nonce_seed; // 0x02FEB5CC
static mp_game_data_wrapper* Mp_game_data; // 0x02FEB698
// migration_state Migration_state (0x02FEB6C4) is used here too; placed in multi.cpp.
static object_pool_server<reliable_data> Reliable_buffer_pool(nullptr, nullptr); // 0x02FEB8CC, no init/release callbacks
cs_wrapper Add_packet_wrapper(0); // 0x02FED534 (:227)

// ---- internal functions
static void reset_tracker(multi_connection* cp); // 0x009433F0 (:608)
static void advance_to_next_tracker(multi_connection* cp); // 0x00943430 (:620)
static tracked_packet* get_current_tracker(multi_connection* cp); // 0x009434A0 (:640)
static void mark_packet_received(multi_connection* cp, unsigned short packet_id); // 0x00971800 (:648)
static float get_packet_loss_pct(const multi_connection* cp); // 0x009434C0 (:671)
static void packet_trackers_process(const mp_transmit_list& transmit_list); // 0x00943530 (:702)
static void transmit_packet_id_acks(const mp_transmit_list& transmit_list); // 0x009B4DE0 (:756)
static multi_connection* get_connection_with_most_allocated_buffers(); // 0x0098E7A0 (:802)
static reliable_data* find_this_frames_reliable_buffer_to_send(multi_connection* cp, unsigned int other_used_size, unsigned int max_size); // 0x0098E860 (:820)
static unsigned int find_this_frames_reliable_size(multi_connection* cp, unsigned int max_size); // 0x0098EAF0 (:937)
static reliable_data* find_reliable_buffer_to_add(multi_connection* cp, const unsigned int packet_size, const unsigned int max_packet_size); // 0x009B4FE0 (:993)
static bool valid_connection_for_space_filler(const multi_connection* cp); // 0x009718D0 (:1834)
static bool is_high_priority_packet(unsigned char packet_type); // 0x009437D0 (:1856)

// File statics of later files of the unity that this file calls (rfg.exe declares them earlier in the TU).
static void calculate_new_bandwidth_values(); // multi_core_io.cpp

// The x87 FISTP of an out-of-range or non-finite value stores the 64-bit "integer indefinite" 0x8000000000000000;
// rfg.exe keeps the low 32 bits of the result.
static int64_t x87_trunc_to_int64(double value)
{
    if (!(value > -9223372036854775808.0 && value < 9223372036854775808.0))
    {
        return INT64_MIN;
    }
    return (int64_t)value;
}

// 0x009433D0 (:295): console command, the release timeouts
void unused_dcf_mp_release_final_timeouts()
{
    Reliable_packet_disconnect_ms = 20000;
    Floodfill_safety_time = 20500;
}

// 0x009433F0 (:608): the trackers restart with one empty tracker; the ids start at 1 (0 is "no tracker")
static void reset_tracker(multi_connection* cp)
{
    if (cp == nullptr)
    {
        return;
    }
    cp->tracked_packets.current_id = 0;
    cp->tracked_packets.last_index = 0;
    cp->tracked_packets.acks_count = 0;
    cp->tracked_packets.last_sends_ack_count = 0;
    cp->tracked_packets.packets[0].init(0);
    cp->tracked_packets.current_id++;
}

// 0x00943430 (:620): a sent packet gets the next tracker, unless packet loss detection is still off for the
// connection (the timestamp is checked before the null pointer, as in rfg.exe)
static void advance_to_next_tracker(multi_connection* cp)
{
    if (!cp->packet_loss_disable_timestamp.valid() || !cp->packet_loss_disable_timestamp.elapsed() || cp == nullptr)
    {
        return;
    }
    // Magic number: 799, the last of the 800 trackers.
    if (cp->tracked_packets.last_index >= 799)
    {
        return;
    }
    cp->tracked_packets.last_index++;
    cp->tracked_packets.packets[cp->tracked_packets.last_index].init(cp->tracked_packets.current_id);
    if (++cp->tracked_packets.current_id == 0)
    {
        cp->tracked_packets.current_id = 1;
    }
}

// 0x009434A0 (:640)
static tracked_packet* get_current_tracker(multi_connection* cp)
{
    if (cp != nullptr && cp->tracked_packets.last_index < 800)
    {
        return &cp->tracked_packets.packets[cp->tracked_packets.last_index];
    }
    return nullptr;
}

// 0x00971800 (:648): the peer acked a tracked packet: the half round trip goes to QoS (once per packet). Only the
// trackers before the current one are searched. Magic number: 16, the session slots.
static void mark_packet_received(multi_connection* cp, unsigned short packet_id)
{
    if (cp == nullptr || cp->index_in_session >= 16)
    {
        return;
    }
    Last_ack_time_ms = Frametime_total_milliseconds_increment_always;
    for (unsigned int i = 0; i < cp->tracked_packets.last_index; i++)
    {
        tracked_packet& tp = cp->tracked_packets.packets[i];
        if (tp.m_packet_id == packet_id)
        {
            if (!tp.m_received)
            {
                tp.m_received = true;
                multi_core_qos_report_ping_time(cp, (Frametime_total_milliseconds_increment_always - tp.m_transmission_time) >> 1);
            }
            return;
        }
    }
}

// 0x009434C0 (:671): the share of tracked packets older than 1 s that were not acked, once there are at least 20.
// rfg.exe divides the two counts as floats in double precision.
static float get_packet_loss_pct(const multi_connection* cp)
{
    if (cp == nullptr)
    {
        return 0.0f;
    }
    int lost = 0;
    int total = 0;
    for (unsigned int i = 0; i < cp->tracked_packets.last_index; i++)
    {
        const tracked_packet& tp = cp->tracked_packets.packets[i];
        if (tp.m_transmission_time + 1000 < Frametime_total_milliseconds_increment_always && tp.m_packet_id != 0)
        {
            total++;
            if (!tp.m_received)
            {
                lost++;
            }
        }
    }
    if (total < 20)
    {
        return 0.0f;
    }
    return (float)((double)(float)lost / (double)(float)total);
}

// 0x00943530 (:702): trackers older than 8 s are dropped (one older tracker is kept before the first recent one)
static void packet_trackers_process(const mp_transmit_list& transmit_list)
{
    const unsigned int cutoff = Frametime_total_milliseconds_increment_always - 8000;
    for (int c = 0; c < transmit_list.num_transmit_connections; c++)
    {
        packet_tracking& tracking = transmit_list.connections[c]->tracked_packets;
        for (unsigned int i = 0; i <= tracking.last_index; i++)
        {
            if (tracking.packets[i].m_transmission_time >= cutoff)
            {
                if (i != 0)
                {
                    const unsigned int drop = i - 1;
                    for (unsigned int k = 0; k < tracking.last_index - drop + 1; k++)
                    {
                        tracking.packets[k] = tracking.packets[k + drop];
                    }
                    tracking.last_index -= drop;
                }
                break;
            }
        }
    }
}

// 0x00971870 (:726): the tracker id of a received packet is queued to be acked back (not to clients of an active game
// session we host: the server does not measure its clients this way). Magic number: 100 queued acks.
void register_packet_ack(multi_connection* cp, unsigned short packet_ack_id)
{
    if (cp == nullptr || (cp->connection_status != CS_CONNECTED && cp->connection_status != CS_CONNECTED_GAMEPLAY) || packet_ack_id == 0)
    {
        return;
    }
    multi_session* sp = cp->owner;
    if (sp != nullptr && cp != sp->get_host_connection() && multi_session_is_active_game_session(sp))
    {
        return;
    }
    if (cp->tracked_packets.acks_count < 100)
    {
        cp->tracked_packets.acks_to_send[cp->tracked_packets.acks_count] = packet_ack_id;
        cp->tracked_packets.acks_count++;
    }
}

// 0x009B4DE0 (:756): the queued tracker acks go out as PACKET_ACK; each ack is sent twice (the acks of the previous
// send are dropped after this one)
static void transmit_packet_id_acks(const mp_transmit_list& transmit_list)
{
    multi_session* game_session = multi_session_get_by_type(ST_GAME);
    for (int c = 0; c < transmit_list.num_transmit_connections; c++)
    {
        multi_connection* cp = transmit_list.connections[c];
        packet_tracking& tracking = cp->tracked_packets;
        if (tracking.acks_count == 0 || (cp->owner != nullptr && cp == cp->owner->get_me_connection()))
        {
            continue;
        }
        if ((cp->connection_status != CS_CONNECTED && cp->connection_status != CS_CONNECTED_GAMEPLAY) ||
            (game_session != nullptr && game_session->am_i_host() && multi_session_is_active_game_session(game_session)))
        {
            reset_tracker(cp);
            continue;
        }
        unsigned char packet[1200];
        packet[0] = PACKET_ACK;
        unsigned int size = Multi_packets[PACKET_ACK].group_id != 0 ? 5 : 3;
        packet[size] = (unsigned char)tracking.acks_count;
        size++;
        for (unsigned int a = 0; a < tracking.acks_count; a++)
        {
            const unsigned short ack_id = tracking.acks_to_send[a];
            packet[size & 0xFFFF] = (unsigned char)(ack_id >> 8);
            packet[(size & 0xFFFF) + 1] = (unsigned char)ack_id;
            size += 2;
        }
        const unsigned short payload = (unsigned short)(size - 3);
        packet[1] = (unsigned char)(payload >> 8);
        packet[2] = (unsigned char)payload;
        multi_io_send(cp, packet, size & 0xFFFF);
        for (int a = 0; a < (int)tracking.last_sends_ack_count; a++)
        {
            tracking.acks_to_send[a] = tracking.acks_to_send[tracking.last_sends_ack_count + a];
        }
        tracking.acks_count -= tracking.last_sends_ack_count;
        tracking.last_sends_ack_count = tracking.acks_count;
    }
}

// 0x0098E7A0 (:802): the peer holding the most reliable buffers (not our own connection; the owner is not checked
// for null, as in rfg.exe)
static multi_connection* get_connection_with_most_allocated_buffers()
{
    multi_connection* most = nullptr;
    Connection_pool.thread_access.enter();
    for (multi_connection* cp = Connection_pool.get_first(0); cp != nullptr; cp = Connection_pool.get_next(cp))
    {
        if (cp != cp->owner->get_me_connection() && (most == nullptr || most->m_reliable_data.get_count(0) < cp->m_reliable_data.get_count(0)))
        {
            most = cp;
        }
    }
    Connection_pool.thread_access.leave();
    return most;
}

// 0x0098E860 (:820): a reliable buffer due for a resend that fits, else the oldest one never sent. The first buffer of
// a packet (nothing used yet) may take up to 511 bytes whatever max_size is (original behaviour).
static reliable_data* find_this_frames_reliable_buffer_to_send(multi_connection* cp, unsigned int other_used_size, unsigned int max_size)
{
    unsigned int limit = 0x1FF;
    if (other_used_size != 0 && max_size <= 0x1FF)
    {
        limit = max_size;
    }
    // Magic numbers: 0x400 bytes in a network packet, 9 for the header (station id 8 is not counted twice: 2 tracker,
    // 2 ack id, 1 count, 2 end marker, 2 slack).
    cp->m_reliable_data.thread_access.enter();
    for (reliable_data* rd = cp->m_reliable_data.get_first(0); rd != nullptr; rd = cp->m_reliable_data.get_next(rd))
    {
        if (rd->m_next_send_time <= Frametime_total_milliseconds_increment_always &&
            other_used_size + cp->unreliable_buffer_size + rd->m_buffer_size + 9 < 0x400 && rd->m_buffer_size + other_used_size <= limit)
        {
            cp->m_reliable_data.thread_access.leave();
            return rd;
        }
    }
    cp->m_reliable_data.thread_access.leave();
    unsigned int oldest_time = 0xFFFFFFFFu;
    reliable_data* oldest = nullptr;
    cp->m_reliable_data.thread_access.enter();
    for (reliable_data* rd = cp->m_reliable_data.get_first(0); rd != nullptr; rd = cp->m_reliable_data.get_next(rd))
    {
        if (rd->m_connection_ack_id == 0xFFFF && rd->m_buffer_size != 0 &&
            other_used_size + cp->unreliable_buffer_size + rd->m_buffer_size + 9 < 0x400 && rd->m_buffer_size + other_used_size <= limit &&
            rd->m_creation_time < oldest_time)
        {
            oldest_time = rd->m_creation_time;
            oldest = rd;
        }
    }
    cp->m_reliable_data.thread_access.leave();
    return oldest;
}

// 0x0098EA60 (:867)
bool connection_has_unsent_reliable_data(multi_connection* cp)
{
    if (cp == nullptr)
    {
        return false;
    }
    cp->m_reliable_data.thread_access.enter();
    for (reliable_data* rd = cp->m_reliable_data.get_first(0); rd != nullptr; rd = cp->m_reliable_data.get_next(rd))
    {
        if (rd->m_connection_ack_id == 0xFFFF && rd->m_buffer_size != 0)
        {
            cp->m_reliable_data.thread_access.leave();
            return true;
        }
    }
    cp->m_reliable_data.thread_access.leave();
    return false;
}

// 0x0098EAF0 (:937): the reliable bytes the next network packet will carry (the resends due, then the unsent buffers
// oldest first), 3 header bytes per buffer
static unsigned int find_this_frames_reliable_size(multi_connection* cp, unsigned int max_size)
{
    unsigned int used = 0;
    if (Reliable_half_max_size_limit)
    {
        max_size >>= 1;
    }
    cp->m_reliable_data.thread_access.enter();
    for (reliable_data* rd = cp->m_reliable_data.get_first(0); rd != nullptr; rd = cp->m_reliable_data.get_next(rd))
    {
        if (rd->m_next_send_time <= Frametime_total_milliseconds_increment_always &&
            used + cp->unreliable_buffer_size + rd->m_buffer_size + 9 < 0x400 && used + rd->m_buffer_size <= max_size)
        {
            used = used + rd->m_buffer_size + 3;
        }
    }
    cp->m_reliable_data.thread_access.leave();
    unsigned int last_time = 0;
    for (;;)
    {
        unsigned int oldest_time = 0xFFFFFFFFu;
        reliable_data* oldest = nullptr;
        cp->m_reliable_data.thread_access.enter();
        for (reliable_data* rd = cp->m_reliable_data.get_first(0); rd != nullptr; rd = cp->m_reliable_data.get_next(rd))
        {
            if (rd->m_connection_ack_id == 0xFFFF && rd->m_buffer_size != 0 &&
                used + cp->unreliable_buffer_size + rd->m_buffer_size + 9 < 0x400 && used + rd->m_buffer_size <= max_size &&
                rd->m_creation_time < oldest_time && rd->m_creation_time > last_time)
            {
                oldest_time = rd->m_creation_time;
                oldest = rd;
            }
        }
        cp->m_reliable_data.thread_access.leave();
        if (oldest == nullptr)
        {
            break;
        }
        used += oldest->m_buffer_size + 3;
        last_time = oldest_time;
    }
    return used;
}

// 0x009B4FE0 (:993): the oldest unsent buffer the packet fits in (255 packets at most, 512 bytes), else a new one; when
// the pool is empty, the peer holding the most buffers loses them all and is disconnected
static reliable_data* find_reliable_buffer_to_add(multi_connection* cp, const unsigned int packet_size, const unsigned int max_packet_size)
{
    reliable_data* best = nullptr;
    cp->m_reliable_data.thread_access.enter();
    unsigned int best_time = 0xFFFFFFFFu;
    for (reliable_data* rd = cp->m_reliable_data.get_first(0); rd != nullptr; rd = cp->m_reliable_data.get_next(rd))
    {
        const unsigned int new_size = packet_size + rd->m_buffer_size;
        if (rd->m_connection_ack_id == 0xFFFF && rd->m_packet_count != 0xFF && new_size < 0x200 && new_size + 9 < max_packet_size &&
            rd->m_creation_time < best_time)
        {
            best = rd;
            best_time = rd->m_creation_time;
        }
    }
    cp->m_reliable_data.thread_access.leave();
    if (best != nullptr)
    {
        return best;
    }
    reliable_data* rd = cp->m_reliable_data.alloc(0);
    if (rd == nullptr)
    {
        multi_connection* victim = get_connection_with_most_allocated_buffers();
        if (victim == nullptr)
        {
            return nullptr;
        }
        victim->m_reliable_data.thread_access.enter();
        for (reliable_data* v = victim->m_reliable_data.get_first(0); v != nullptr; v = victim->m_reliable_data.free(v))
        {
        }
        victim->m_reliable_data.thread_access.leave();
        if (victim->connection_status != CS_DISCONNECTED)
        {
            victim->connection_status = CS_DISCONNECTED;
            victim->disconnect_reason = CDR_RELIABLE_BUFFER_FULL;
        }
        rd = cp->m_reliable_data.alloc(0);
        if (rd == nullptr)
        {
            return nullptr;
        }
    }
    rd->reset(cp->next_reliable_identifier++);
    rd->m_callbacks = nullptr;
    return rd;
}

// 0x0098ED00 (:1129)
int multi_connection_get_connection_req_id_count(unsigned int req_id)
{
    int count = 0;
    Connection_pool.thread_access.enter();
    for (multi_connection* cp = Connection_pool.get_first(0); cp != nullptr; cp = Connection_pool.get_next(cp))
    {
        if (cp->connection_req_id == req_id)
        {
            count++;
        }
    }
    Connection_pool.thread_access.leave();
    return count;
}

// 0x00943600 (:1144)
void multi_core_cheat_detect_shutdown()
{
    Client_standby_hack_check.invalidate();
}

// 0x00943610 (:1149): magic number: the check starts 15 s after the match starts
void multi_core_cheat_detect_start()
{
    Client_standby_hack_check.set(15000);
}

// 0x009A82F0 (:1154)
bool multi_core_cheat_detection_enabled()
{
    if (!Allow_cheat_detection)
    {
        return false;
    }
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    return sp != nullptr && sp->get_info()->type == ST_GAME && (sp->get_state() == MSS_IN_GAME || sp->get_state() == MSS_MIGRATING);
}

// 0x00943620 (:1169)
bool multi_core_cheat_detection_should_check()
{
    return Client_standby_hack_check.valid() && Client_standby_hack_check.elapsed();
}

// 0x009A8330 (:1174): an empty buffer (not sent, no ack id); its callbacks go back to the allocator without being called
void reliable_data::reset(unsigned int identifier)
{
    reliable_callback* cb = m_callbacks;
    m_connection_ack_id = 0xFFFF;
    m_buffer_size = 0;
    m_next_send_time = 0xFFFFFFFFu;
    m_identifier = identifier;
    m_packet_count = 0;
    m_creation_time = 0;
    m_transmit_count = 0;
    m_first_transmission_time = 0;
    while (cb != nullptr)
    {
        reliable_callback* next = cb->reliable_next;
        reliable_callback* head = m_callbacks;
        if (head == cb && next == cb)
        {
            cb->reliable_next = nullptr;
            cb->reliable_prev = nullptr;
            m_callbacks = nullptr;
        }
        else
        {
            if (head == cb)
            {
                m_callbacks = next;
            }
            cb->reliable_prev->reliable_next = cb->reliable_next;
            cb->reliable_next->reliable_prev = cb->reliable_prev;
            cb->reliable_next = nullptr;
            cb->reliable_prev = nullptr;
        }
        Reliable_callback_allocator.free(cb);
        cb = next != head ? next : nullptr;
    }
    m_callbacks = nullptr;
}

// 0x009D7DA0 (:1202): the reliable buffer and callback pools, the connections, and 100 in-order receive buffers.
// Magic numbers: 2048 reliable buffers, 256 callbacks, 48 connections.
void multi_connection_init()
{
    Reliable_buffer_pool.init(&MP_allocators_mempool, 0x800);
    Reliable_callback_allocator.init(MP_allocators_mempool);
    Connection_pool.init(MP_allocators_mempool);
    reliable_in_order_buffer* buffers = (reliable_in_order_buffer*)MP_allocators_mempool.alloc(100 * sizeof(reliable_in_order_buffer), 0);
    Free_reliable_in_order_buffer_list = nullptr;
    for (int i = 0; i < 100; i++)
    {
        multi_core_release_reliable_in_order_buffer(&buffers[i]);
    }
}

// 0x009B5190 (:1224)
void multi_connection_shutdown()
{
}

// 0x00943650 (:1230): the head of the circular free list
reliable_in_order_buffer* multi_core_get_free_reliable_in_order_buffer()
{
    reliable_in_order_buffer* buf = Free_reliable_in_order_buffer_list;
    if (buf == nullptr)
    {
        return nullptr;
    }
    if (buf->next == buf)
    {
        Free_reliable_in_order_buffer_list = nullptr;
    }
    else
    {
        if (buf->prev != nullptr)
        {
            buf->prev->next = buf->next;
        }
        Free_reliable_in_order_buffer_list = buf->next;
        if (buf->next != nullptr)
        {
            buf->next->prev = buf->prev;
        }
    }
    buf->prev = nullptr;
    buf->next = nullptr;
    return buf;
}

// 0x009436C0 (:1240): inserted before the head, then made the head
void multi_core_release_reliable_in_order_buffer(reliable_in_order_buffer* buf)
{
    reliable_in_order_buffer* head = Free_reliable_in_order_buffer_list;
    Free_reliable_in_order_buffer_list = buf;
    if (head == nullptr)
    {
        buf->next = buf;
        buf->prev = buf;
        return;
    }
    head->prev->next = buf;
    buf->prev = head->prev;
    head->prev = buf;
    buf->next = head;
}

// 0x009B51A0 (:1247)
void multi_connection_process(const mp_transmit_list& transmit_list)
{
    packet_trackers_process(transmit_list);
    transmit_packet_id_acks(transmit_list);
}

// 0x00943710 (:1253)
void multi_connection_activate_connection(multi_connection*)
{
}

// 0x00943720 (:1260)
void multi_connection_update_connection_pending_status(multi_connection* cp)
{
    if (cp->connection_status == CS_PENDING)
    {
        cp->connection_status = CS_CONNECTED;
    }
}

// 0x009A8490 (:1279): a pending connection with fresh buffers, trackers and timers. On the server (or towards the host)
// in a game session it gets every delta compressed master object. QoS is off for 20 s and loss detection for 10 s.
// next_ack_id is only fixed up when it is 0xFFFF, so a reused connection keeps counting (original behaviour).
multi_connection* multi_connection_allocate(const connection_address* address, multi_session* owner, const multi_player_uid* player_uid,
    const multi_player_uid* reserver_uid, bool to_host, bool)
{
    if (address == nullptr)
    {
        return nullptr;
    }
    multi_connection* cp = Connection_pool.alloc(0);
    if (cp == nullptr)
    {
        return nullptr;
    }
    cp->local_nonce = Nonce_seed++;
    cp->m_reliable_data.init(&Reliable_buffer_pool);
    cp->index_in_session = 0xFF;
    cp->allocated = true;
    cp->registered = false;
    memset(cp->reliable_ordered_data.next_expected_recv_sequence_id, 0, sizeof(cp->reliable_ordered_data.next_expected_recv_sequence_id));
    memset(cp->reliable_ordered_data.next_send_sequence_id, 0, sizeof(cp->reliable_ordered_data.next_send_sequence_id));
    cp->reliable_ordered_data.list = nullptr;
    if (player_uid == nullptr)
    {
        cp->player_uid.invalidate();
    }
    else
    {
        cp->player_uid = *player_uid;
    }
    if (reserver_uid == nullptr)
    {
        cp->reserver_uid = cp->player_uid;
    }
    else
    {
        cp->reserver_uid = *reserver_uid;
    }
    cp->addr.init();
    if (address->connection_id.isValid)
    {
        cp->addr.copy(*address);
    }
    cp->owner = owner;
    cp->game_data = 0xFFFFFFFFu;
    cp->join_time = timer_get(TIMER_MILLISECONDS);
    cp->upstream_bits_sec = 0;
    cp->interpolation_latency_ms = 0;
    cp->clock_update_timestamp.invalidate();
    cp->qos_disable_timestamp.invalidate();
    cp->dc_objects = nullptr;
    if (cp->next_ack_id == 0xFFFF)
    {
        cp->next_ack_id = 0;
    }
    cp->cannot_serve = 0;
    cp->unreliable_buffer_size = 0;
    cp->next_reliable_identifier = 0;
    cp->ack_queue_count = 0;
    cp->ack_stored_count = 0;
    cp->connection_status = CS_PENDING;
    cp->last_update_time = 0;
    cp->standby_ignore_time.invalidate();
    cp->last_flood_test_time.invalidate();
    for (int i = 0; i < 32; i++)
    {
        cp->ack_queue[i].ack_id = 0;
        cp->ack_queue[i].ack_sent_count = 0;
    }
    memset(cp->ack_stored, 0, sizeof(cp->ack_stored));
    cp->ping_pos = 0;
    cp->qos_inited = 0;
    cp->num_ping_samples = 0;
    cp->mean_ping = 0;
    cp->last_upstream_adjustment = 0;
    cp->peer_heartbeat_timer.set(0);
    cp->joining_in_progress = false;
    if (Multiplayer && (Server || to_host) && owner != nullptr && owner->get_info()->type == ST_GAME)
    {
        cp->dc_objects = nullptr;
        Master_object_allocator.thread_access.enter();
        for (dc_master_object* dcmo = Master_object_allocator.get_first(0); dcmo != nullptr; dcmo = Master_object_allocator.get_next(dcmo))
        {
            multi_core_dc_add_object_to_connection(dcmo, cp);
        }
        Master_object_allocator.thread_access.leave();
    }
    multi_connection_disable_qos(cp, 20000);
    cp->packet_loss_disable_timestamp.set(10000);
    cp->connection_retry_count = 0;
    calculate_new_bandwidth_values();
    reset_tracker(cp);
    return cp;
}

// 0x009B51C0 (:1443): the delta compression objects, the reliable buffers and the in-order buffers are released
void multi_connection_delete(multi_connection* cp)
{
    if (cp == nullptr)
    {
        keen::debug::nativePrintArgs("[multi_connection] tried to delete nullptr connection!\n");
        return;
    }
    Master_object_allocator.thread_access.enter();
    for (dc_master_object* dcmo = Master_object_allocator.get_first(0); dcmo != nullptr; dcmo = Master_object_allocator.get_next(dcmo))
    {
        multi_core_dc_remove_object_from_connection(dcmo, cp);
    }
    Master_object_allocator.thread_access.leave();
    cp->owner = nullptr;
    if (cp->connection_status != CS_DISCONNECTED)
    {
        cp->connection_status = CS_DISCONNECTED;
        cp->disconnect_reason = CDR_DELETED;
    }
    cp->allocated = false;
    cp->m_reliable_data.thread_access.enter();
    for (reliable_data* rd = cp->m_reliable_data.get_first(0); rd != nullptr; rd = cp->m_reliable_data.free(rd))
    {
    }
    cp->m_reliable_data.thread_access.leave();
    reliable_in_order_buffer* buf = cp->reliable_ordered_data.list;
    while (buf != nullptr)
    {
        reliable_in_order_buffer* next = buf->next;
        reliable_in_order_buffer* head = cp->reliable_ordered_data.list;
        reliable_in_order_buffer* following = next != head ? next : nullptr;
        if (head == buf && next == buf)
        {
            buf->next = nullptr;
            buf->prev = nullptr;
            cp->reliable_ordered_data.list = nullptr;
        }
        else
        {
            if (head == buf)
            {
                cp->reliable_ordered_data.list = next;
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
        buf = following;
    }
    Connection_pool.free(cp);
    calculate_new_bandwidth_values();
}

// 0x009A8720 (:1490)
multi_connection* multi_session_game_host_connection()
{
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    if (sp != nullptr)
    {
        return sp->get_host_connection();
    }
    return nullptr;
}

// 0x0098EDB0 (:1506)
multi_connection* multi_find_connection_by_addr(const connection_address& from_conn)
{
    Connection_pool.thread_access.enter();
    for (multi_connection* cp = Connection_pool.get_first(0); cp != nullptr; cp = Connection_pool.get_next(cp))
    {
        if (cp->addr.connection_id.is_equal(from_conn.connection_id))
        {
            Connection_pool.thread_access.leave();
            return cp;
        }
    }
    Connection_pool.thread_access.leave();
    return nullptr;
}

// 0x00943740 (:1530)
unsigned int multi_connection_get_index(const multi_connection* cp)
{
    return cp->index_in_session;
}

// 0x0098EE20 (:1538): the interpolation delay of each connection is its mean ping
void multi_connection_update_latencies()
{
    Connection_pool.thread_access.enter();
    for (multi_connection* cp = Connection_pool.get_first(0); cp != nullptr; cp = Connection_pool.get_next(cp))
    {
        cp->interpolation_latency_ms = cp->mean_ping;
    }
    Connection_pool.thread_access.leave();
}

// 0x00943750 (:1550): extends (never shortens) the time QoS ignores the connection
void multi_connection_disable_qos(multi_connection* cp, int num_ms_to_disable)
{
    if (cp->qos_disable_timestamp.valid() && cp->qos_disable_timestamp.time_until() >= num_ms_to_disable)
    {
        return;
    }
    cp->qos_disable_timestamp.set(num_ms_to_disable);
}

// 0x0098EED0 (:1563): not an in-order type (those must go reliable), fits, connected, and the right direction
bool multi_connection_is_valid_unreliable_packet(multi_connection* cp, const unsigned char* data, unsigned int len)
{
    if (cp == nullptr)
    {
        return false;
    }
    const unsigned int type = data[0];
    if (Multi_packets[type].group_id != 0)
    {
        char text[1024];
        sprintf(text, "RIO packet type \"%s\" (%d) being sent with multi_io_send(). Assuming the programmer meant to use multi_io_send_reliable().  NOT SENDING!!",
            Multi_packets[type].packet_name, type);
        keen::debug::nativePrintArgs(text);
        return false;
    }
    if (len < 0x3F7 && cp->connection_status != CS_PENDING && cp->connection_status != CS_DISCONNECTED)
    {
        sender receiver(cp);
        return multi_io_verify_packet_direction_send(&Multi_packets[type], &receiver);
    }
    return false;
}

// 0x0098EFA0 (:1615)
bool multi_connection_add_unreliable_packet_internal(multi_connection* cp, const unsigned char* packet, unsigned int packet_size)
{
    Add_packet_wrapper.enter();
    if (multi_connection_is_valid_unreliable_packet(cp, packet, packet_size) && packet_size <= 0x3F7u - cp->unreliable_buffer_size)
    {
        memcpy(cp->unreliable_buffer + cp->unreliable_buffer_size, packet, packet_size);
        cp->unreliable_buffer_size = (unsigned short)(cp->unreliable_buffer_size + packet_size);
        Add_packet_wrapper.leave();
        return true;
    }
    Add_packet_wrapper.leave();
    return false;
}

// 0x0098F020 (:1669): not to ourselves (the owner is not checked for null, as in rfg.exe), not disconnected, fits, the
// right direction; in-order packets at most 512 bytes in groups below 64
bool multi_connection_is_valid_reliable_packet(multi_connection* cp, const unsigned char* data, unsigned int len)
{
    if (cp == nullptr || cp == cp->owner->get_me_connection())
    {
        return false;
    }
    const packet_def* packet = &Multi_packets[data[0]];
    const unsigned int group_id = packet->group_id;
    if (cp->connection_status == CS_DISCONNECTED || len >= 0x3F7)
    {
        return false;
    }
    sender receiver(cp);
    if (!multi_io_verify_packet_direction_send(packet, &receiver))
    {
        return false;
    }
    if (group_id != 0)
    {
        if (len > 0x200)
        {
            keen::debug::nativePrintArgs("In-order reliable packet is too large, either compress it or bump MAX_RELIABLE_IN_ORDER_PACKET_SIZE");
            return false;
        }
        if (group_id >= 0x40)
        {
            keen::debug::nativePrintArgs("In-order reliable group id is too large, either reduce it or bump MAX_RELIABLE_IN_ORDER_GROUPS");
            return false;
        }
    }
    return true;
}

// 0x009B5380 (:1730): the packet joins a reliable buffer (and its callback the buffer's list); returns the buffer's
// identifier, 0xFFFFFFFF on failure (a full buffer pool disconnects the connection)
unsigned int multi_connection_add_reliable_packet_internal(multi_connection* cp, const unsigned char* packet, const unsigned int packet_size,
    void (*callback)(void*), void* cb_data)
{
    Add_packet_wrapper.enter();
    if (!multi_connection_is_valid_reliable_packet(cp, packet, packet_size))
    {
        Add_packet_wrapper.leave();
        return 0xFFFFFFFFu;
    }
    unsigned int max_size = Max_data_per_packet;
    if (Reliable_half_max_size_limit)
    {
        max_size = Max_data_per_packet >> 1;
    }
    reliable_data* rd = find_reliable_buffer_to_add(cp, packet_size, max_size);
    if (rd == nullptr)
    {
        // rfg.exe inlines multi_connection_disconnect here.
        if (cp == nullptr)
        {
            keen::debug::nativePrintArgs("[multi_connection] tried to disconnect nullptr connection!\n");
        }
        else if (cp->connection_status != CS_DISCONNECTED)
        {
            cp->connection_status = CS_DISCONNECTED;
            cp->disconnect_reason = CDR_RELIABLE_BUFFER_FULL;
        }
        Add_packet_wrapper.leave();
        return 0xFFFFFFFFu;
    }
    if (callback != nullptr)
    {
        reliable_callback* cb = Reliable_callback_allocator.alloc(0);
        if (cb == nullptr)
        {
            multi_connection_disconnect(cp, CDR_RELIABLE_CALLBACK_BUFFER_FULL);
            Add_packet_wrapper.leave();
            return 0xFFFFFFFFu;
        }
        cb->callback = callback;
        cb->callback_data_ptr = cb_data;
        if (rd->m_callbacks == nullptr)
        {
            cb->reliable_next = cb;
            cb->reliable_prev = cb;
        }
        else
        {
            rd->m_callbacks->reliable_prev->reliable_next = cb;
            cb->reliable_prev = rd->m_callbacks->reliable_prev;
            rd->m_callbacks->reliable_prev = cb;
            cb->reliable_next = rd->m_callbacks;
        }
        rd->m_callbacks = cb;
    }
    if (rd->m_buffer_size == 0)
    {
        rd->m_creation_time = Frametime_total_milliseconds_increment_always;
    }
    memcpy(rd->m_buffer + rd->m_buffer_size, packet, packet_size);
    rd->m_buffer_size = (unsigned short)(rd->m_buffer_size + packet_size);
    rd->m_packet_count++;
    const unsigned int identifier = rd->m_identifier;
    Add_packet_wrapper.leave();
    return identifier;
}

// 0x0098F0D0 (:1794): magic number: 9 header bytes
unsigned int multi_connection_space_used_in_network_packet(multi_connection* cp, unsigned int max_size)
{
    return cp->unreliable_buffer_size + 9 + find_this_frames_reliable_size(cp, max_size);
}

// 0x0098F0F0 (:1807)
unsigned int multi_connection_space_left_in_network_packet(multi_connection* cp, unsigned int max_size)
{
    return 0x3F7u - cp->unreliable_buffer_size - find_this_frames_reliable_size(cp, max_size);
}

// 0x00943790 (:1818)
unsigned int multi_connection_space_left_in_network_packet_physical(multi_connection* cp)
{
    return 0x3F7u - cp->unreliable_buffer_size;
}

// 0x009437B0 (:1824): no compression or voice: a copy of `length` bytes
bool multi_connection_massage_send_data(const unsigned char* in_buf, int, unsigned char* out_buf, int, int& length, unsigned char*, int&)
{
    memcpy(out_buf, in_buf, length);
    return true;
}

// 0x009718D0 (:1834): the bandwidth filler only goes to clients of the game session we host in game
static bool valid_connection_for_space_filler(const multi_connection* cp)
{
    multi_session* sp = cp->owner;
    if (sp != nullptr && sp->am_i_host() && sp->get_state() == MSS_IN_GAME)
    {
        return sp->get_info()->type == ST_GAME;
    }
    return false;
}

// 0x009437D0 (:1856)
static bool is_high_priority_packet(unsigned char packet_type)
{
    for (int i = 0; i < 3; i++)
    {
        if (High_priority_packet_types[i] == packet_type)
        {
            return true;
        }
    }
    return false;
}

// 0x009437F0 (:1867): the acks first, then the rest, each in its order
void prioritize_unreliable_buffer(multi_connection* cp)
{
    if (cp->unreliable_buffer_size == 0)
    {
        return;
    }
    unsigned char sorted[1200];
    int sorted_size = 0;
    int offset = 0;
    do
    {
        const int payload = (cp->unreliable_buffer[offset + 1] << 8) | cp->unreliable_buffer[offset + 2];
        if (is_high_priority_packet(cp->unreliable_buffer[offset]))
        {
            memcpy(sorted + sorted_size, cp->unreliable_buffer + offset, payload + 3);
            sorted_size += payload + 3;
        }
        offset += 3 + payload;
    } while (offset < (int)cp->unreliable_buffer_size);
    offset = 0;
    do
    {
        const int payload = (cp->unreliable_buffer[offset + 1] << 8) | cp->unreliable_buffer[offset + 2];
        if (!is_high_priority_packet(cp->unreliable_buffer[offset]))
        {
            memcpy(sorted + sorted_size, cp->unreliable_buffer + offset, payload + 3);
            sorted_size += payload + 3;
        }
        offset += 3 + payload;
    } while (offset < (int)cp->unreliable_buffer_size);
    memcpy(cp->unreliable_buffer, sorted, cp->unreliable_buffer_size);
}

// 0x0098F120 (:1917): builds and sends the network packet of a connected connection (see the file comment): the
// reliable buffers due (each gets its ack id on its first send and is resent after 500 ms), then the unreliable
// packets that fit 1024 bytes (the rest stays for the next packet), then on the host of a running game with bandwidth
// filling on a filler packet (PT_BULL_TACO) up to the slot's budget. The send is counted in the tracker and in the
// bandwidth slot (with 45 bytes of UDP/IP overhead); the unreliable buffer is emptied whatever happened.
bool multi_connection_flush_current_packet_no_sync(multi_connection* cp)
{
    if (cp->connection_status != CS_CONNECTED && cp->connection_status != CS_CONNECTED_GAMEPLAY)
    {
        return false;
    }
    unsigned short size = 0;
    connection_address my_address;
    my_address.init();
    unit4::RemoteUserId my_id = {};
    if (cp->owner != nullptr && cp->owner->get_me_connection() != nullptr)
    {
        my_id = cp->owner->get_me_connection()->addr.connection_id;
    }
    my_address.set(my_id);
    my_address.append_to_packet(Transmission_buffer, size);
    tracked_packet* tracker = cp->tracked_packets.last_index < 800 ? &cp->tracked_packets.packets[cp->tracked_packets.last_index] : nullptr;
    *(unsigned short*)(Transmission_buffer + size) = htons(tracker->m_packet_id);
    unsigned int offset = size + 2u;
    tracker->m_transmission_time = Frametime_total_milliseconds_increment_always;
    unsigned int reliable_used = 0;
    for (;;)
    {
        reliable_data* rd = find_this_frames_reliable_buffer_to_send(cp, reliable_used, Max_data_per_packet);
        if (rd == nullptr || rd->m_buffer_size == 0)
        {
            break;
        }
        if (rd->m_connection_ack_id == 0xFFFF)
        {
            rd->m_connection_ack_id = cp->next_ack_id;
            rd->m_first_transmission_time = Frametime_total_milliseconds_increment_always;
            if (++cp->next_ack_id == 0xFFFF)
            {
                cp->next_ack_id = 0;
            }
        }
        *(unsigned short*)(Transmission_buffer + (offset & 0xFFFF)) = htons(rd->m_connection_ack_id);
        rd->m_next_send_time = Frametime_total_milliseconds_increment_always + 500; // Magic number: the resend interval
        Transmission_buffer[(offset + 2) & 0xFFFF] = rd->m_packet_count;
        rd->m_transmit_count++;
        memcpy(Transmission_buffer + ((offset + 3) & 0xFFFF), rd->m_buffer, rd->m_buffer_size);
        offset = offset + 3 + rd->m_buffer_size;
        reliable_used = reliable_used + rd->m_buffer_size + 3;
    }
    *(unsigned short*)(Transmission_buffer + (offset & 0xFFFF)) = 0xFFFF; // the end of the reliable groups
    offset += 2;
    prioritize_unreliable_buffer(cp);
    unsigned short unreliable_used = 0;
    if (cp->unreliable_buffer_size != 0)
    {
        const unsigned int start = offset & 0xFFFF;
        do
        {
            const unsigned int payload = (cp->unreliable_buffer[unreliable_used + 1] << 8) | cp->unreliable_buffer[unreliable_used + 2];
            if ((int)(payload + start + 3) >= 0x400)
            {
                break;
            }
            unreliable_used = (unsigned short)(unreliable_used + payload + 3);
        } while (unreliable_used < cp->unreliable_buffer_size);
        if (unreliable_used != 0)
        {
            memcpy(Transmission_buffer + start, cp->unreliable_buffer, unreliable_used);
            if (unreliable_used < cp->unreliable_buffer_size)
            {
                memmove(cp->unreliable_buffer, cp->unreliable_buffer + unreliable_used, cp->unreliable_buffer_size - unreliable_used);
            }
            cp->unreliable_buffer_size = (unsigned short)(cp->unreliable_buffer_size - unreliable_used);
        }
    }
    unsigned short packet_size = (unsigned short)(offset + unreliable_used);
    // The budget of this slot in bytes, at most 1024, less the 45 bytes of overhead (a 16-bit value).
    unsigned int per_packet = Max_data_per_packet;
    if (per_packet != 0)
    {
        per_packet += 45;
    }
    unsigned int budget_bits = cp->available_bandwidth_tracker.get_available_bandwidth();
    if (budget_bits >= per_packet * 8)
    {
        budget_bits = per_packet * 8;
    }
    const unsigned short budget_bytes = (unsigned short)(budget_bits >> 3);
    unsigned short budget;
    if (budget_bytes > 0x400)
    {
        budget = (unsigned short)(0x400 - 45);
    }
    else if (budget_bytes > 45)
    {
        budget = (unsigned short)(budget_bytes + 0xFFD3);
    }
    else
    {
        budget = 0;
    }
    if (valid_connection_for_space_filler(cp) && multi_core_qos_bandwidth_filling_is_on() && Migration_state == MIGRATION_STATE_NONE &&
        packet_size < budget && (int)budget - (int)packet_size > 3)
    {
        Transmission_buffer[packet_size] = PT_BULL_TACO;
        const unsigned int header = Multi_packets[PT_BULL_TACO].group_id != 0 ? 5 : 3;
        const unsigned short payload = (unsigned short)(header - packet_size + budget - 6);
        Transmission_buffer[packet_size + 1] = (unsigned char)(payload >> 8);
        Transmission_buffer[packet_size + 2] = (unsigned char)payload;
        packet_size = budget;
    }
    if (packet_size > 4)
    {
        WSASetLastError(0);
        Multi_thread_mutex.lock(true);
        const bool sent = multi_core_platform_internal_send_packet(cp->addr, Transmission_buffer, packet_size);
        Multi_thread_mutex.unlock();
        if (sent)
        {
            // Magic number: 0x168 bits (45 bytes) of UDP/IP overhead per packet.
            const unsigned int bits = packet_size * 8u + 0x168;
            tracker->m_size += bits;
            advance_to_next_tracker(cp);
            cp->available_bandwidth_tracker.mark_bandwidth_used(bits);
        }
        cp->unreliable_buffer_size = 0;
    }
    return true;
}

// 0x0098F4E0 (:2172): the send job's cpu is given up first, then the connections are released
bool multi_connection_flush_current_packet(multi_connection* cp)
{
    if (MP_send_process_trigger != nullptr)
    {
        scheduler_relinquish_cpu(MP_send_process_trigger, ccjt_any);
        MP_send_process_trigger = nullptr;
    }
    const bool result = multi_connection_flush_current_packet_no_sync(cp);
    Multi_connections_acquired = 0;
    return result;
}

// 0x0098F520 (:2195): false when a reliable buffer stayed unacked for longer than the disconnect time (not while a
// flood test result is fresh)
bool multi_connection_test_connection(multi_connection* cp)
{
    if (cp->connection_status == CS_PENDING)
    {
        return true;
    }
    if (cp->last_flood_test_time.valid() && cp->last_flood_test_time.time_since() > 0 && cp->last_flood_test_time.time_since() < Floodfill_safety_time)
    {
        return true;
    }
    cp->m_reliable_data.thread_access.enter();
    const unsigned int now = Frametime_total_milliseconds_increment_always;
    for (reliable_data* rd = cp->m_reliable_data.get_first(0); rd != nullptr; rd = cp->m_reliable_data.get_next(rd))
    {
        if (rd->m_connection_ack_id != 0xFFFF && (unsigned int)Reliable_packet_disconnect_ms < now - rd->m_first_transmission_time)
        {
            cp->m_reliable_data.thread_access.leave();
            return false;
        }
    }
    cp->m_reliable_data.thread_access.leave();
    return true;
}

// 0x00943950 (:2227)
int multi_connection_get_connection_timeout()
{
    return Reliable_packet_disconnect_ms;
}

// 0x00943960 (:2232): the first reason sticks
void multi_connection_disconnect(multi_connection* cp, multi_connection_disconnect_reason reason)
{
    if (cp == nullptr)
    {
        keen::debug::nativePrintArgs("[multi_connection] tried to disconnect nullptr connection!\n");
        return;
    }
    if (cp->connection_status != CS_DISCONNECTED)
    {
        cp->connection_status = CS_DISCONNECTED;
        cp->disconnect_reason = reason;
    }
}

// 0x00943990 (:2261)
bool multi_connection_is_connected(const multi_connection* cp)
{
    return cp->connection_status == CS_CONNECTED || cp->connection_status == CS_CONNECTED_GAMEPLAY;
}

// 0x009439B0 (:2266)
void multi_connection_set_gameplay(multi_connection* cp)
{
    if (cp->connection_status == CS_CONNECTED)
    {
        cp->connection_status = CS_CONNECTED_GAMEPLAY;
    }
}

// 0x009439D0 (:2275)
void multi_connection_end_gameplay(multi_connection* cp)
{
    if (cp->connection_status == CS_CONNECTED_GAMEPLAY)
    {
        cp->connection_status = CS_CONNECTED;
    }
}

// 0x009439F0 (:2289): the received reliable ack id is remembered (the last 128, a repeat moves to the end) and queued
// to be acked back (a queued one is acked again from zero; at most 32 queued)
void multi_connection_queue_ack_for_send(multi_connection* cp, unsigned short ack_id)
{
    const unsigned int count = cp->ack_stored_count;
    bool stored = false;
    for (int i = 0; i < (int)cp->ack_stored_count; i++)
    {
        if (cp->ack_stored[i] == ack_id)
        {
            memmove(cp->ack_stored + i, cp->ack_stored + i + 1, (count - i) * 2 - 2);
            cp->ack_stored[cp->ack_stored_count - 1] = ack_id;
            stored = true;
            break;
        }
    }
    if (!stored)
    {
        if (cp->ack_stored_count == 0x80)
        {
            memmove(cp->ack_stored, cp->ack_stored + 1, 0xFE);
            cp->ack_stored[0x7F] = ack_id;
        }
        else
        {
            cp->ack_stored[count] = ack_id;
            cp->ack_stored_count++;
        }
    }
    const unsigned char queued = cp->ack_queue_count;
    for (int i = 0; i < (int)cp->ack_queue_count; i++)
    {
        if (cp->ack_queue[i].ack_id == ack_id)
        {
            cp->ack_queue[i].ack_sent_count = 0;
            return;
        }
    }
    if (queued != 0x20)
    {
        cp->ack_queue[queued].ack_id = ack_id;
        cp->ack_queue[cp->ack_queue_count].ack_sent_count = 0;
        cp->ack_queue_count++;
    }
}

// 0x009B54F0 (:2366): the queued acks that fit max_size go out as PT_ACK_REPLY; acks sent twice leave the queue
void multi_connection_poll_for_acks(multi_connection* cp, unsigned int max_size)
{
    unsigned int num_acks = cp->ack_queue_count;
    const unsigned int max_acks = (max_size - 4) >> 1;
    if (max_acks < num_acks)
    {
        num_acks = max_acks;
    }
    if (num_acks == 0)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_ACK_REPLY;
    unsigned int size = Multi_packets[PT_ACK_REPLY].group_id != 0 ? 5 : 3;
    packet[size] = (unsigned char)num_acks;
    size++;
    for (unsigned int a = 0; a < num_acks; a++)
    {
        const unsigned short ack_id = cp->ack_queue[a].ack_id;
        cp->ack_queue[a].ack_sent_count++;
        packet[size & 0xFFFF] = (unsigned char)(ack_id >> 8);
        packet[(size & 0xFFFF) + 1] = (unsigned char)ack_id;
        size += 2;
    }
    const unsigned short payload = (unsigned short)(size - 3);
    packet[1] = (unsigned char)(payload >> 8);
    packet[2] = (unsigned char)payload;
    multi_io_send(cp, packet, size & 0xFFFF);
    unsigned char kept = 0;
    for (int a = 0; a < (int)cp->ack_queue_count; a++)
    {
        if (cp->ack_queue[a].ack_sent_count < 2)
        {
            cp->ack_queue[kept++] = cp->ack_queue[a];
        }
    }
    cp->ack_queue_count = kept;
}

// 0x009A8740 (:2429): PT_ACK_REPLY: the reliable buffers with these ack ids are done; their callbacks run, then the
// buffers are freed. The walk stops once as many buffers as ack ids were freed.
void multi_core_connection_process_ack_reply(unsigned char* packet_data, const sender& from)
{
    multi_connection* cp = from.connection;
    if (cp == nullptr)
    {
        return;
    }
    const unsigned char num_acks = packet_data[0];
    unsigned short ack_ids[32];
    for (int i = 0; i < (int)num_acks; i++)
    {
        ack_ids[i] = (unsigned short)(packet_data[1 + i * 2] << 8 | packet_data[2 + i * 2]);
    }
    unsigned char num_freed = 0;
    cp->m_reliable_data.thread_access.enter();
    reliable_data* rd = cp->m_reliable_data.get_first(0);
    while (rd != nullptr)
    {
        reliable_data* next = cp->m_reliable_data.get_next(rd);
        bool acked = false;
        if (rd->m_connection_ack_id != 0xFFFF)
        {
            for (unsigned char a = 0; a < num_acks; a++)
            {
                if (ack_ids[a] == rd->m_connection_ack_id)
                {
                    acked = true;
                    break;
                }
            }
        }
        if (acked)
        {
            reliable_callback* cb = rd->m_callbacks;
            while (cb != nullptr)
            {
                reliable_callback* following = cb->reliable_next != rd->m_callbacks ? cb->reliable_next : nullptr;
                cb->callback(cb->callback_data_ptr);
                if (rd->m_callbacks == cb && cb->reliable_next == cb)
                {
                    cb->reliable_next = nullptr;
                    cb->reliable_prev = nullptr;
                    rd->m_callbacks = nullptr;
                }
                else
                {
                    if (rd->m_callbacks == cb)
                    {
                        rd->m_callbacks = cb->reliable_next;
                    }
                    cb->reliable_prev->reliable_next = cb->reliable_next;
                    cb->reliable_next->reliable_prev = cb->reliable_prev;
                    cb->reliable_next = nullptr;
                    cb->reliable_prev = nullptr;
                }
                Reliable_callback_allocator.free(cb);
                cb = following;
            }
            rd->reset(cp->next_reliable_identifier++);
            cp->m_reliable_data.free(rd);
            if (++num_freed == num_acks)
            {
                break;
            }
        }
        rd = next;
    }
    cp->m_reliable_data.thread_access.leave();
}

// 0x00943B00 (:2525)
bool multi_connection_have_received_reliable_ack_id_already(multi_connection* cp, unsigned short ack_id)
{
    for (int i = 0; i < (int)cp->ack_stored_count; i++)
    {
        if (cp->ack_stored[i] == ack_id)
        {
            return true;
        }
    }
    return false;
}

// 0x0098F5E0 (:2543): the buffer with this identifier is gone (acked); 0xFFFFFFFF (a failed send) counts as received
bool multi_connection_reliable_data_received(multi_connection* cp, unsigned int reliable_id)
{
    if (reliable_id == 0xFFFFFFFFu)
    {
        return true;
    }
    cp->m_reliable_data.thread_access.enter();
    for (reliable_data* rd = cp->m_reliable_data.get_first(0); rd != nullptr; rd = cp->m_reliable_data.get_next(rd))
    {
        if (rd->m_identifier == reliable_id)
        {
            cp->m_reliable_data.thread_access.leave();
            return false;
        }
    }
    cp->m_reliable_data.thread_access.leave();
    return true;
}

// 0x00943B40 (:2562): PT_HEARTBEAT: nothing to do (any packet updates the connection)
void multi_core_heartbeat(unsigned char*, const sender&)
{
}

// 0x009D7F50 (:2567): tells the host we cannot reach the target's player (PT_CANNOT_CONNECT); rfg.exe inlines
// multi_game_data_get(target, false, false) on Mp_game_data
void multi_connection_send_cannot_connect(multi_connection* host_cp, multi_connection* target_cp)
{
    const unsigned int game_data = target_cp->game_data;
    if (Mp_game_data == nullptr || game_data == 0xFFFFFFFFu)
    {
        return;
    }
    const multi_game_data* data = multi_game_data_get(game_data, false, false);
    if (data == nullptr)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_CANNOT_CONNECT;
    unsigned short size = Multi_packets[PT_CANNOT_CONNECT].group_id != 0 ? 5 : 3;
    data->player_uid.write_to_packet(packet, size);
    const unsigned short payload = (unsigned short)(size - 3);
    packet[1] = (unsigned char)(payload >> 8);
    packet[2] = (unsigned char)payload;
    multi_io_send_reliable(host_cp, packet, size, nullptr, nullptr, nullptr);
}

// 0x009E3540 (:2581): on the host, the two peers cannot serve each other (a bit per session index); the next host in
// line learns the blocks
void multi_connection_cannot_connect(unsigned char* packet_data, const sender& from)
{
    multi_connection* cp = from.connection;
    int offset = 0;
    if (cp == nullptr || cp->owner == nullptr || !cp->owner->am_i_host())
    {
        return;
    }
    multi_session* sp = cp->owner;
    multi_player_uid uid;
    uid.read_from_packet(packet_data, offset);
    multi_connection* first = sp->get_connection_list();
    for (multi_connection* other = first; other != nullptr;)
    {
        if (other->player_uid.get_internal_id().is_equal(uid.get_internal_id()))
        {
            cp->cannot_serve |= (unsigned short)(1 << (other->index_in_session & 0x1F));
            other->cannot_serve |= (unsigned short)(1 << (cp->index_in_session & 0x1F));
            break;
        }
        other = other->next;
        if (other == first)
        {
            break;
        }
    }
    multi_connection* next_host = cp->owner->get_next_host_connection();
    if (next_host != nullptr)
    {
        multi_connection_send_cannot_connect_block(cp->owner, next_host);
    }
}

// 0x009D8050 (:2623): the host sends every connection's block mask to the target (PT_CANNOT_CONNECT_BLOCK): a 16-bit
// mask of the indices that follow, then one 16-bit block mask each
void multi_connection_send_cannot_connect_block(multi_session_base* sp, multi_connection* target_cp)
{
    if (sp == nullptr || target_cp == nullptr || !sp->am_i_host() || sp->get_me_connection() == target_cp)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_CANNOT_CONNECT_BLOCK;
    const unsigned short mask_offset = Multi_packets[PT_CANNOT_CONNECT_BLOCK].group_id != 0 ? 5 : 3;
    packet[mask_offset] = 0;
    packet[mask_offset + 1] = 0;
    unsigned short size = (unsigned short)(mask_offset + 2);
    unsigned short mask = 0;
    for (int i = 0; i < 16; i++)
    {
        multi_connection* cp = sp->get_connection_by_index((unsigned char)i);
        if (cp != nullptr && cp->cannot_serve != 0)
        {
            mask |= (unsigned short)(1 << (i & 0x1F));
            packet[size] = (unsigned char)(cp->cannot_serve >> 8);
            packet[size + 1] = (unsigned char)cp->cannot_serve;
            size += 2;
        }
    }
    packet[mask_offset] = (unsigned char)(mask >> 8);
    packet[mask_offset + 1] = (unsigned char)mask;
    const unsigned short payload = (unsigned short)(size - 3);
    packet[1] = (unsigned char)(payload >> 8);
    packet[2] = (unsigned char)payload;
    if (mask != 0)
    {
        multi_io_send_reliable(target_cp, packet, size, nullptr, nullptr, nullptr);
    }
}

// 0x00971900 (:2661): a client of the session takes the host's block masks
void multi_connection_cannot_connect_block(unsigned char* packet_data, const sender& from)
{
    if (from.connection == nullptr)
    {
        return;
    }
    multi_session* sp = from.connection->owner;
    if (sp == nullptr || sp->am_i_host())
    {
        return;
    }
    const unsigned short mask = (unsigned short)(packet_data[0] << 8 | packet_data[1]);
    const unsigned char* block = packet_data + 2;
    for (int i = 0; i < 16; i++)
    {
        if ((mask & (1 << (i & 0x1F))) != 0)
        {
            const unsigned short cannot_serve = (unsigned short)(block[0] << 8 | block[1]);
            block += 2;
            multi_connection* cp = sp->get_connection_by_index((unsigned char)i);
            if (cp != nullptr)
            {
                cp->cannot_serve = cannot_serve;
            }
        }
    }
}

// 0x00943B50 (:2687): PT_BULL_TACO, the bandwidth filler: dropped
void multi_core_process_bull_taco(unsigned char*, const sender&)
{
}

// 0x009719C0 (:2698)
unsigned int multi_core_connection_get_queued_realibles(multi_connection* cp)
{
    if (cp == nullptr)
    {
        return 0;
    }
    return cp->m_reliable_data.get_count(0);
}

// 0x00943B60 (:2707)
float multi_core_connection_get_packet_loss(const multi_connection* cp)
{
    return get_packet_loss_pct(cp);
}

// 0x009719D0 (:2713): the same as register_packet_ack
void multi_connection_packet_received(multi_connection* from_connection, unsigned short packet_ack_id)
{
    register_packet_ack(from_connection, packet_ack_id);
}

// 0x009719E0 (:2720): PACKET_ACK: the peer received our tracked packets (mark_packet_received inlined per id)
void multi_connection_process_packet_acks(unsigned char* packet_data, const sender& from)
{
    multi_connection* cp = from.connection;
    if (cp == nullptr)
    {
        return;
    }
    for (unsigned int count = packet_data[0]; count != 0; count--)
    {
        mark_packet_received(cp, (unsigned short)(packet_data[1] << 8 | packet_data[2]));
        packet_data += 2;
    }
}

// 0x00943B80 (:2743): rfg.exe inlines reset_tracker
void multi_core_connection_reset_packet_loss(multi_connection* cp)
{
    reset_tracker(cp);
}

// 0x00943BC0 (:2749)
void multi_core_connection_enable_packet_loss_detection(multi_connection* cp)
{
    cp->packet_loss_disable_timestamp.set(0);
}

// 0x00971AB0 (:2759): magic number: 0xE000 bits per second at most
void available_bandwidth::reset(unsigned int starting_bandwidth_per_second)
{
    if (starting_bandwidth_per_second > 0xE000)
    {
        starting_bandwidth_per_second = 0xE000;
    }
    m_bandwidth_max = starting_bandwidth_per_second;
    for (int i = 0; i < 30; i++)
    {
        m_bandwidth_slots[i].m_bits_sent = 0;
        m_bandwidth_slots[i].m_send_time.invalidate();
    }
    m_slot_index = 0;
}

// 0x00971AF0 (:2777): the next of the 30 slots starts empty
void available_bandwidth::process(unsigned int bandwidth_per_second)
{
    if (bandwidth_per_second > 0xE000)
    {
        bandwidth_per_second = 0xE000;
    }
    m_bandwidth_max = bandwidth_per_second;
    m_slot_index = (m_slot_index + 1) % 30;
    m_bandwidth_slots[m_slot_index].m_bits_sent = 0;
    m_bandwidth_slots[m_slot_index].m_send_time.invalidate();
}

// 0x00971B30 (:2792): what is left of the maximum after the bits sent in the last second
unsigned int available_bandwidth::get_available_bandwidth()
{
    unsigned int sent = 0;
    for (int i = 0; i < 30; i++)
    {
        if (m_bandwidth_slots[i].m_send_time.valid() && m_bandwidth_slots[i].m_send_time.time_since() < 1000)
        {
            sent += m_bandwidth_slots[i].m_bits_sent;
        }
    }
    if (sent < m_bandwidth_max)
    {
        return m_bandwidth_max - sent;
    }
    return 0;
}

// 0x00943BE0 (:2798)
unsigned int available_bandwidth::get_bandwidth_consumed(int time_ms)
{
    unsigned int sent = 0;
    for (int i = 0; i < 30; i++)
    {
        if (m_bandwidth_slots[i].m_send_time.valid() && m_bandwidth_slots[i].m_send_time.time_since() < time_ms)
        {
            sent += m_bandwidth_slots[i].m_bits_sent;
        }
    }
    return sent;
}

// 0x00971B80 (:2817): at most one packet's worth (with 45 bytes of overhead)
unsigned int available_bandwidth::get_timeslice_available_bandwidth()
{
    unsigned int per_packet = Max_data_per_packet;
    if (per_packet != 0)
    {
        per_packet += 45;
    }
    const unsigned int available = get_available_bandwidth();
    if (available >= per_packet * 8)
    {
        return per_packet * 8;
    }
    return available;
}

// 0x00943C20 (:2837)
void available_bandwidth::mark_bandwidth_used(unsigned int bandwidth_used)
{
    m_bandwidth_slots[m_slot_index].m_bits_sent += bandwidth_used;
    m_bandwidth_slots[m_slot_index].m_send_time.set(0);
}

// 0x009A8960 (:2880): on the host of a running game, a client silent for 1.5 s (not a marauder) is flagged for 500 ms
void multi_core_process_client_using_standby_hack(multi_connection* cp)
{
    const unsigned int now = Clock_current_time;
    if (cp->owner->get_host_connection() != cp->owner->get_me_connection() || !Allow_cheat_detection)
    {
        return;
    }
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    if (sp == nullptr || sp->get_info()->type != ST_GAME || (sp->get_state() != MSS_IN_GAME && sp->get_state() != MSS_MIGRATING))
    {
        return;
    }
    if (!Client_standby_hack_check.valid() || !Client_standby_hack_check.elapsed() || cp->last_update_time >= now)
    {
        return;
    }
    if (now - cp->last_update_time > 1500) // Magic number: 1.5 s without packets
    {
        const multi_game_data* data = multi_game_data_get(cp->game_data, false, false);
        if (data == nullptr || data->team != HUMAN_TEAM_MARAUDER)
        {
            cp->standby_ignore_time.set(500);
        }
    }
}

// 0x009A8A10 (:2924): the server itself is suspect when its acks stopped and at least half of the clients that joined
// over 15 s ago look like standby users (with more than two connections)
bool multi_core_is_server_using_standby_hack()
{
    if (!Multiplayer || !Server || !Allow_cheat_detection)
    {
        return false;
    }
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    if (sp == nullptr || sp->get_info()->type != ST_GAME || (sp->get_state() != MSS_IN_GAME && sp->get_state() != MSS_MIGRATING) ||
        Last_ack_time_ms > Frametime_total_milliseconds_increment_always)
    {
        return false;
    }
    sp = multi_session_get_by_type(ST_GAME);
    if (!multi_session_is_active_game_session(sp))
    {
        return false;
    }
    bool any_checked = false;
    int suspects = 0;
    int count = 0;
    const int now = timer_get(TIMER_MILLISECONDS);
    multi_connection* first = sp->get_connection_list();
    if (first == nullptr)
    {
        return false;
    }
    multi_connection* cp = first;
    do
    {
        count++;
        if (cp != cp->owner->get_host_connection() && cp->join_time + 15000 < now)
        {
            any_checked = true;
            if (cp->possibly_using_standby_hack())
            {
                suspects++;
            }
        }
        cp = cp->next;
    } while (cp != first && cp != nullptr);
    if (any_checked && count > 2)
    {
        return count <= suspects * 2;
    }
    return false;
}

// 0x00971BC0 (:3115): bits per second sent on a connection over the last previous_ms (from the trackers; with
// ignore_drops a packet not acked within 1 s does not count); for our own connection, the sum over the session's
// connected peers. rfg.exe computes in x87 doubles and truncates through a 64-bit integer.
unsigned int multi_core_connection_get_bandwidth_bits_sec(const multi_connection* cp, bool ignore_drops, unsigned int previous_ms)
{
    if (cp == nullptr || cp->owner == nullptr)
    {
        return 0;
    }
    if (previous_ms >= Frametime_total_milliseconds_increment_always)
    {
        previous_ms = Frametime_total_milliseconds_increment_always;
    }
    const unsigned int cutoff = Frametime_total_milliseconds_increment_always - previous_ms;
    multi_session* sp = cp->owner;
    multi_connection* me = sp->get_me_connection();
    struct helper
    {
        static unsigned int bits_sec(const multi_connection* c, bool ignore_drops, unsigned int cutoff)
        {
            unsigned int bits = 0;
            unsigned int first_time = 0xFFFFFFFFu;
            unsigned int last_time = 0;
            for (unsigned int i = 0; i < c->tracked_packets.last_index; i++)
            {
                const tracked_packet& tp = c->tracked_packets.packets[i];
                if (tp.m_transmission_time < cutoff)
                {
                    continue;
                }
                if (ignore_drops && !tp.m_received && tp.m_transmission_time + 1000 < Frametime_total_milliseconds_increment_always)
                {
                    continue;
                }
                bits += tp.m_size;
                if (first_time >= tp.m_transmission_time)
                {
                    first_time = tp.m_transmission_time;
                }
                if (last_time <= tp.m_transmission_time)
                {
                    last_time = tp.m_transmission_time;
                }
            }
            return (unsigned int)x87_trunc_to_int64((double)bits / ((double)(last_time - first_time) / 1000.0));
        }
    };
    unsigned int result = 0;
    if (cp == me)
    {
        multi_connection* first = sp->get_connection_list();
        if (first == nullptr)
        {
            return 0;
        }
        multi_connection* other = first;
        do
        {
            if (multi_connection_is_connected(other) && other != me)
            {
                result += helper::bits_sec(other, ignore_drops, cutoff);
            }
            other = other->next;
        } while (other != first && other != nullptr);
    }
    else
    {
        if (cp->connection_status != CS_CONNECTED && cp->connection_status != CS_CONNECTED_GAMEPLAY)
        {
            return 0;
        }
        result = helper::bits_sec(cp, ignore_drops, cutoff);
    }
    return result;
}

// 0x009A8B30 (:3368): on the host of the game session, the mean loss of the peers (each capped at 20%), when more than
// one peer loses over 1% (or there is only one). rfg.exe sums the floats in double precision.
float compute_server_packet_loss(multi_connection* me_cp)
{
    if (me_cp == nullptr || me_cp->owner == nullptr || !me_cp->owner->am_i_host())
    {
        return 0.0f;
    }
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    if (sp != me_cp->owner)
    {
        return 0.0f;
    }
    float sum = 0.0f;
    int lossy = 0;
    int count = 0;
    multi_connection* first = sp->get_connection_list();
    if (first != nullptr)
    {
        multi_connection* cp = first;
        do
        {
            if (multi_connection_is_connected(cp) && cp != me_cp && cp->packet_loss_disable_timestamp.valid() && cp->packet_loss_disable_timestamp.elapsed())
            {
                const float loss = get_packet_loss_pct(cp);
                const float capped = (float)((double)loss - (double)0.2f) >= 0.0f ? 0.2f : loss;
                sum = (float)((double)capped + (double)sum);
                if (loss > 0.01f)
                {
                    lossy++;
                }
                count++;
            }
            cp = cp->next;
        } while (cp != sp->get_connection_list() && cp != nullptr);
        if (count != 0 && (lossy > 1 || count <= 1))
        {
            return (float)((double)sum / (double)count); // x87 FIDIV, rounded once
        }
    }
    return 0.0f;
}

// 0x00943E50 (:3409)
bool multi_connection_is_connection_in_transmit_list(const mp_transmit_list& transmit_list, const multi_connection* cp)
{
    for (int i = 0; i < transmit_list.num_transmit_connections; i++)
    {
        if (transmit_list.connections[i] == cp)
        {
            return true;
        }
    }
    return false;
}

// 0x00941DB0 (.h:91)
mp_transmit_list::mp_transmit_list()
{
    num_transmit_connections = 0;
}

// 0x00941DC0 (.h:116)
void tracked_packet::init(unsigned short packet_id)
{
    m_transmission_time = 0;
    m_packet_id = packet_id;
    m_received = false;
    m_size = 0;
}

// 0x00941DE0 (.h:151)
reliable_data::reliable_data()
{
    m_callbacks = nullptr;
}

// 0x0098E000 (.h:302): the timestamps invalid, the reliable client unbound
multi_connection::multi_connection()
{
}

// 0x00970660 (.h:303)
multi_connection::~multi_connection()
{
}

// 0x00941DF0 (.h:305): the standby flag is still running
bool multi_connection::possibly_using_standby_hack() const
{
    return standby_ignore_time.valid() && !standby_ignore_time.elapsed();
}

// 0x00941E30
available_bandwidth_slot_info::available_bandwidth_slot_info()
{
}

// 0x00970670
available_bandwidth::available_bandwidth()
{
}


// ---- connection addresses. A connection address is
// the GameLink station id of the peer.

// 0x00971BB0 (:2845)
void connection_address::init()
{
    memset(connection_id.providerData, 0, sizeof(connection_id.providerData));
    connection_id.guestIndex = 0;
    connection_id.isValid = false;
}

// 0x00943C50 (:2850)
void connection_address::set(unit4::RemoteUserId in_connection_id)
{
    connection_id = in_connection_id;
}

// 0x00943CF0 (:2855)
void connection_address::copy(const connection_address& src)
{
    connection_id = src.connection_id;
}

// 0x00943D50 (:2860): the provider data only (rfg.exe tail-jumps into RemoteUserId::is_equal)
bool connection_address::is_equal(const connection_address& other_addr)
{
    return connection_id.is_equal(other_addr.connection_id);
}


// 0x00943D60 (:2865): the 8 bytes of the station id
void connection_address::append_to_packet(unsigned char* packet_data, unsigned short& packet_size) const
{
    for (int i = 0; i < 8; i++)
    {
        packet_data[packet_size] = (unsigned char)connection_id.providerData[i];
        packet_size++;
    }
}

// 0x00943DE0 (:2870): the guest index is left as it is
bool connection_address::read_from_packet(unsigned char* packet_data, int& packet_offset)
{
    for (int i = 0; i < 8; i++)
    {
        connection_id.providerData[i] = (char)packet_data[packet_offset];
        packet_offset++;
    }
    connection_id.isValid = true;
    return true;
}

