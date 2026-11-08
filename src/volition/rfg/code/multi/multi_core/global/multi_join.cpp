// Original: volition/rfg/code/multi/multi_core/global/multi_join.cpp
//
// Joining a session: the client's PT_JOIN_REQUEST (version, upstream, uid, session id, invited and reserved flags,
// party members, its game data); the host checks it, reserves slots for the party, registers the player's game data,
// adds the connection and answers PT_JOIN_ACCEPT (session info, the host's connection, the index, game data, clock,
// server options, join in progress) or PT_JOIN_DENY; then tells everyone of the new connection (PT_NEW_CONNECTION)
// and sends the newcomer the others and the game data without a connection (PT_NEW_GAME_DATA). Multi-byte values are
// big endian on the wire.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_core/include/multi_join.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_qos.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/vlib/code/os/scheduler.h"
#include "library/modules/runtime/base/include/keen/base/mutex.hpp"
#include "library/modules/runtime/base/include/keen/base/trace.hpp"
#include "library/modules/runtime/gamesession/include/unit4/gamesession/remotegamesession.hpp"

// ---- globals
int Join_request_timeout = 10000; // 0x0167DE60

// ---- file-static globals
static unsigned int Local_game_data_handles[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF}; // 0x0167E5F4
// int Settings_version (0x02FE88B0) is used here too; placed in multi.cpp.
// unsigned int Clock_current_time (0x02FEB538) is used here too; placed in multi.cpp.
// unsigned char Multi_version_channel (0x02FEB5B0) is used here too; placed in multi.cpp.
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.

// File statics of multi_game_data.cpp (later in the unity TU), declared here.
static void append_player_data_light(unsigned char* packet_data, unsigned short& packet_size, multi_game_data* player_data, unsigned int handle); // 0x00951AA0 (multi_game_data.cpp:532)

// Magic number: up to 16 party members in a join request
static const unsigned int Max_party_members = 16;

// ---- internal functions

// Port helpers: rfg.exe inlines these byte swaps and the game data lookup.
static void write_u32_be(unsigned char* packet_data, unsigned short& packet_size, unsigned int value)
{
    packet_data[packet_size] = (unsigned char)(value >> 24);
    packet_data[packet_size + 1] = (unsigned char)(value >> 16);
    packet_data[packet_size + 2] = (unsigned char)(value >> 8);
    packet_data[packet_size + 3] = (unsigned char)value;
    packet_size = (unsigned short)(packet_size + 4);
}

static unsigned int read_u32_be(const unsigned char* packet_data, int& packet_offset)
{
    const unsigned int value = ((unsigned int)packet_data[packet_offset] << 24) | ((unsigned int)packet_data[packet_offset + 1] << 16) |
                               ((unsigned int)packet_data[packet_offset + 2] << 8) | packet_data[packet_offset + 3];
    packet_offset += 4;
    return value;
}

static multi_game_data* game_data_of(unsigned int handle)
{
    return &Mp_game_data->data[handle & 0xFFFF][handle >> 16];
}

// Port helpers (the 16-bit swaps and the packet header), shared with the later files of the unity TU (multi_game).
static void write_u16_be(unsigned char* packet_data, unsigned short& packet_size, unsigned short value)
{
    packet_data[packet_size] = (unsigned char)(value >> 8);
    packet_data[packet_size + 1] = (unsigned char)value;
    packet_size = (unsigned short)(packet_size + 2);
}

static unsigned short read_u16_be(const unsigned char* packet_data, int& packet_offset)
{
    const unsigned short value = (unsigned short)(packet_data[packet_offset] << 8 | packet_data[packet_offset + 1]);
    packet_offset += 2;
    return value;
}

// A packet starts with its type and the payload size (big endian, the 3 header bytes not counted); in-order types
// (a packet group) have 2 more bytes for the sequence id, which the reliable layer fills in.
static unsigned short packet_header_size(unsigned char type)
{
    return Multi_packets[type].group_id != 0 ? 5 : 3;
}

static void packet_set_size(unsigned char* packet, unsigned short size)
{
    const unsigned short payload = (unsigned short)(size - 3);
    packet[1] = (unsigned char)(payload >> 8);
    packet[2] = (unsigned char)payload;
}

// 0x00973940 (:92): the address, index, nonce, game data handle, upstream, uid, reserver uid. LTCG: packet_data in
// EDI, packet_size in ESI.
static void append_join_connection_to_packet(unsigned char* packet_data, unsigned short& packet_size, multi_connection* data_cp)
{
    data_cp->addr.append_to_packet(packet_data, packet_size);
    packet_data[packet_size++] = data_cp->index_in_session;
    write_u32_be(packet_data, packet_size, (unsigned int)data_cp->local_nonce);
    write_u32_be(packet_data, packet_size, data_cp->game_data);
    write_u32_be(packet_data, packet_size, data_cp->upstream_bits_sec);
    data_cp->player_uid.write_to_packet(packet_data, packet_size);
    data_cp->reserver_uid.write_to_packet(packet_data, packet_size);
}

// 0x00973A70 (:104). LTCG: dest_cp in EDI, packet_offset in ESI.
static void read_join_connection_from_packet(unsigned char* packet_data, int& packet_offset, multi_connection* dest_cp)
{
    dest_cp->addr.read_from_packet(packet_data, packet_offset);
    dest_cp->index_in_session = packet_data[packet_offset++];
    dest_cp->local_nonce = (int)read_u32_be(packet_data, packet_offset);
    dest_cp->game_data = read_u32_be(packet_data, packet_offset);
    dest_cp->upstream_bits_sec = read_u32_be(packet_data, packet_offset);
    dest_cp->player_uid.read_from_packet(packet_data, packet_offset);
    dest_cp->reserver_uid.read_from_packet(packet_data, packet_offset);
}

// 0x00973C00 (:116): the join connection plus its full game data (in multiplayer). LTCG: packet_size in EAX,
// packet_data in ECX.
static void append_new_connection_to_packet(unsigned char* packet_data, unsigned short& packet_size, multi_connection* data_cp)
{
    append_join_connection_to_packet(packet_data, packet_size, data_cp);
    const unsigned int handle = data_cp->game_data;
    if (packet_data != nullptr && Multiplayer && handle != 0xFFFFFFFFu)
    {
        multi_game_data* data = game_data_of(handle);
        append_player_data_full(packet_data, packet_size, data, data->handle);
    }
}

// 0x00973D80 (:145): the game data handle, upstream and game data (light or full). LTCG: packet_size in EAX,
// packet_data in EDX.
static void append_connection_update_to_packet(unsigned char* packet_data, unsigned short& packet_size, multi_connection* data_cp, bool light_update)
{
    write_u32_be(packet_data, packet_size, data_cp->game_data);
    write_u32_be(packet_data, packet_size, data_cp->upstream_bits_sec);
    const unsigned int handle = data_cp->game_data;
    if (packet_data != nullptr && Multiplayer && handle != 0xFFFFFFFFu)
    {
        multi_game_data* data = game_data_of(handle);
        if (light_update)
        {
            append_player_data_light(packet_data, packet_size, data, data->handle);
            return;
        }
        append_player_data_full(packet_data, packet_size, data, data->handle);
    }
}

// 0x00973EA0 (:154). LTCG: packet_offset in EAX, packet_data in ECX, dest_cp in ESI.
static void read_connection_update_from_packet(unsigned char* packet_data, int& packet_offset, multi_connection* dest_cp, bool light_update)
{
    dest_cp->game_data = read_u32_be(packet_data, packet_offset);
    dest_cp->upstream_bits_sec = read_u32_be(packet_data, packet_offset);
    dest_cp->game_data = multi_game_data_update_player_data(packet_data, packet_offset, light_update);
}

// ---- join requests

// 0x009B7600 (:175): the version and our upstream bandwidth (big endian), our uid, the session id, invited, the
// reserved slot flag, the party (count with us, then the others' uids; 1 without a party), our game data
void multi_join_send_join_request_packet(multi_connection* conn, multi_session* join_target, bool reservedSlot)
{
    unsigned char packet[1200];
    packet[0] = PT_JOIN_REQUEST;
    unsigned short size = Multi_packets[PT_JOIN_REQUEST].group_id != 0 ? 5 : 3;
    write_u32_be(packet, size, multi_get_multiplayer_version());
    // rfg.exe calls multi_core_qos_up_bandwidth once per byte (a byte swap macro).
    write_u32_be(packet, size, multi_core_qos_up_bandwidth(nullptr));
    My_player_uid.write_to_packet(packet, size);
    join_target->get_info()->append_id_to_packet(packet, size);
    packet[size++] = join_target->get_info()->invited;
    packet[size++] = reservedSlot;
    if (join_target->is_party_owner_and_has_other_party_members())
    {
        unit4::NamedRemoteUserId members[Max_party_members];
        g_gameLinkMutex.lock(0);
        const unsigned int num_members = Remote_game_session.getGameLinkSession()->getPartyMembers(members, Max_party_members);
        packet[size++] = (unsigned char)num_members;
        for (unsigned int i = 0; i < num_members; i++)
        {
            if (!members[i].id.is_equal(My_player_uid.get_internal_id()))
            {
                multi_player_uid member;
                member.set(members[i].id, true);
                member.write_to_packet(packet, size);
            }
        }
        g_gameLinkMutex.unlock();
    }
    else
    {
        packet[size++] = 1;
    }
    multi_game_data_append_my_data(packet, size, ST_GAME);
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_io_send_reliable(conn, packet, size, nullptr, nullptr, nullptr);
}

// 0x009DB070 (:314): a request from an unknown address. A player with a reserved slot takes it (the reserver becomes
// its group); a player without one brings its party, for whom slots are reserved. Accepted: the game data is
// registered and the connection added; denied: PT_JOIN_DENY sent at once on a temporary connection.
void multi_join_process_join_request_packet(unsigned char* packet_data, const sender& from)
{
    connection_address address;
    address.copy(*from.address);
    if (from.connection != nullptr)
    {
        keen::debug::nativePrintArgs("multi_join_process_join_request_packet refusing to process join_request for client that already has a connection\n");
        return;
    }
    multi_session_join_status status = MSJS_JOIN_ACCEPT;
    multi_player_uid uid;
    uid.invalidate();
    unsigned int upstream_bits_sec = 0;
    int num_reservees = 0;
    bool invited = false;
    multi_session* sp = nullptr;
    multi_connection* cp = nullptr;
    multi_player_uid reservees[Max_party_members];
    int packet_offset = 0;
    const unsigned int version = read_u32_be(packet_data, packet_offset);
    if (version == multi_get_multiplayer_version())
    {
        upstream_bits_sec = read_u32_be(packet_data, packet_offset);
        uid.read_from_packet(packet_data, packet_offset);
        multi_session_info info;
        info.read_id_from_packet(packet_data, packet_offset);
        sp = multi_session_find_by_id(info);
        if (sp == nullptr)
        {
            keen::debug::nativePrintArgs("\tNew connection denied: MSJS_JOIN_DENY_SESSION_DNE\n");
            status = MSJS_JOIN_DENY_SESSION_DNE;
        }
        else if (sp->get_player_connection(uid) != nullptr)
        {
            keen::debug::nativePrintArgs("\tNew connection denied: MSJS_JOIN_DENY_DUPLICATE\n");
            status = MSJS_JOIN_DENY_DUPLICATE;
        }
        invited = packet_data[packet_offset] != 0;
        const bool reserved_slot = packet_data[packet_offset + 1] != 0;
        packet_offset += 2;
        if (status == MSJS_JOIN_ACCEPT)
        {
            // The last reservation for this player (from a copy of the slots).
            const slot_use_data slots = sp->get_slot_use_data();
            const multi_player_uid* reserver = nullptr;
            for (int i = 0; i < slots.reserved; i++)
            {
                if (slots.reservations[i].reservee.is_equal(uid))
                {
                    reserver = &slots.reservations[i].reserver;
                }
            }
            if (reserver != nullptr)
            {
                cp = multi_connection_allocate(&address, sp, &uid, reserver, false, false);
            }
            else if (reserved_slot)
            {
                // Original behaviour kept: a reserved-slot request without a reservation is dropped unanswered.
                return;
            }
            const unsigned char party_count = packet_data[packet_offset++];
            num_reservees = 0;
            if (reserver == nullptr)
            {
                // Original behaviour kept: with a reservation the party uids are not read (a party that follows
                // a reservation would misalign the game data).
                if (party_count != 0)
                {
                    reservees[0] = uid;
                    num_reservees = 1;
                }
                for (int i = 1; i < (int)party_count; i++)
                {
                    reservees[num_reservees++].read_from_packet(packet_data, packet_offset);
                }
            }
        }
    }
    else
    {
        keen::debug::nativePrintArgs("\tNew connection denied: MSJS_JOIN_DENY_VERSION. \n");
        status = MSJS_JOIN_DENY_VERSION;
        sp = nullptr;
    }
    if (cp == nullptr)
    {
        cp = multi_connection_allocate(&address, sp, &uid, nullptr, false, false);
    }
    cp->upstream_bits_sec = upstream_bits_sec;
    cp->was_invited = invited;
    cp->connection_status = CS_PENDING;
    if (status == MSJS_JOIN_ACCEPT)
    {
        if (sp->get_state() == MSS_NONE)
        {
            keen::debug::nativePrintArgs("\tNew connection denied: MSJS_JOIN_DENY_SESSION_DNE (session is already MSS_NONE)\n");
            status = MSJS_JOIN_DENY_SESSION_DNE;
        }
        else if (!sp->am_i_host())
        {
            keen::debug::nativePrintArgs("\tNew connection denied: MSJS_JOIN_DENY_SESSION_DNE (I am not host)\n");
            status = MSJS_JOIN_DENY_SESSION_DNE;
        }
    }
    bool slots_reserved = false;
    if (status == MSJS_JOIN_ACCEPT)
    {
        if (num_reservees < 1 || sp->reserve_slots(num_reservees, uid, reservees))
        {
            slots_reserved = true;
            cp->game_data = multi_game_data_register_client_player(packet_data, packet_offset, sp->get_info()->type);
            if (cp->game_data == 0xFFFFFFFFu)
            {
                keen::debug::nativePrintArgs("\tNew connection denied: MSJS_JOIN_DENY_SESSION_DNE (No game data slots available)\n");
                status = MSJS_JOIN_DENY_SESSION_FULL;
            }
            else
            {
                status = sp->add_connection(cp, 0xFF, num_reservees);
                if (status == MSJS_JOIN_ACCEPT)
                {
                    multi_join_send_join_accept_packet(cp);
                    multi_join_send_new_connection_packet_to_all(cp);
                    multi_join_send_connections(cp);
                    return;
                }
                // rfg.exe passes no arguments for the format.
                keen::debug::nativePrintArgs("\tNew connection (%p) denied: %p (Session-provided reason)\n");
            }
        }
        else
        {
            keen::debug::nativePrintArgs("\tNew connection denied: MSJS_JOIN_DENY_SESSION_FULL (failed to reserve slots)\n");
            status = MSJS_JOIN_DENY_SESSION_FULL;
        }
    }
    keen::debug::nativePrintArgs("\tNew connection (%p) denied: %p\n");
    if (cp->game_data != 0xFFFFFFFFu)
    {
        multi_game_data_remove_player(cp->game_data);
        cp->game_data = 0xFFFFFFFFu;
    }
    if (sp != nullptr && slots_reserved)
    {
        sp->unreserve_slots(uid);
    }
    cp->connection_status = CS_CONNECTED;
    multi_join_send_join_deny_packet(cp, status);
    if (MP_send_process_trigger != nullptr)
    {
        scheduler_relinquish_cpu(MP_send_process_trigger, ccjt_any);
        MP_send_process_trigger = nullptr;
    }
    multi_connection_flush_current_packet_no_sync(cp);
    Multi_connections_acquired = 0;
    multi_connection_delete(cp);
}

// 0x009B78B0 (:559): the session's join info, the host's connection, the joiner's index, the host's game data (not on
// a dedicated server) and the joiner's, the clock, the server options, whether the game is running (join in progress)
void multi_join_send_join_accept_packet(multi_connection* conn)
{
    multi_session* sp = conn->owner;
    if (!sp->am_i_host())
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_JOIN_ACCEPT;
    unsigned short size = Multi_packets[PT_JOIN_ACCEPT].group_id != 0 ? 5 : 3;
    sp->get_info()->append_join_info_to_packet(packet, size);
    append_join_connection_to_packet(packet, size, sp->get_host_connection());
    packet[size++] = conn->index_in_session;
    const unsigned int host_handle = sp->get_host_connection()->game_data;
    if (!sp->get_info()->dedicated && Multiplayer && host_handle != 0xFFFFFFFFu)
    {
        multi_game_data* data = game_data_of(host_handle);
        append_player_data_full(packet, size, data, data->handle);
    }
    const unsigned int handle = conn->game_data;
    if (Multiplayer && handle != 0xFFFFFFFFu)
    {
        multi_game_data* data = game_data_of(handle);
        append_player_data_full(packet, size, data, data->handle);
    }
    write_u32_be(packet, size, Clock_current_time);
    multi_game_append_server_options_to_packet(packet, size, &Mp_server_options);
    const bool in_progress = gameseq_get_state() == GS_LOAD || gameseq_get_state() == GS_GAMEPLAY || gameseq_get_state() == GS_INGAME_OPTIONS;
    packet[size++] = in_progress;
    conn->joining_in_progress = in_progress;
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_io_send_reliable(conn, packet, size, nullptr, nullptr, nullptr);
}

// 0x009FC3E0 (:616): a client waiting for the accept takes the session info, the host's connection, its index, the
// game data and the clock; a join in progress as a party is refused here (SESSION_LOCKED), otherwise it asks the host
// for the running game (PT_REQUEST_JIP). from.connection is not checked for null (as in rfg.exe).
void multi_join_process_join_accept_packet(unsigned char* packet_data, const sender& from)
{
    if (from.connection->owner->am_i_host())
    {
        return;
    }
    multi_session_info info;
    int packet_offset = 0;
    info.read_join_info_from_packet(packet_data, packet_offset);
    multi_session* sp = multi_session_find_by_id(info);
    if (sp == nullptr)
    {
        return;
    }
    sp->set_session_info(info);
    multi_connection* me = sp->get_me_connection();
    multi_connection* host = sp->get_host_connection();
    if (me == nullptr || host == nullptr || me == host || me->owner != sp || sp->get_join_state() != MSJ_WAITING_FOR_ACCEPT)
    {
        return;
    }
    read_join_connection_from_packet(packet_data, packet_offset, host);
    me->index_in_session = packet_data[packet_offset++];
    if (!sp->get_info()->dedicated)
    {
        host->game_data = multi_game_data_update_player_data(packet_data, packet_offset, false);
    }
    const unsigned int my_handle = multi_game_data_update_player_data(packet_data, packet_offset, false);
    me->owner->get_me_connection()->game_data = my_handle;
    Clock_current_time = read_u32_be(packet_data, packet_offset);
    multi_game_data_set_my_handle(my_handle);
    multi_game_read_server_options_from_packet(&Mp_server_options, packet_data, packet_offset);
    Multi_join_in_progress = packet_data[packet_offset] != 0;
    if (Multi_join_in_progress)
    {
        g_gameLinkMutex.lock(0);
        if (Remote_game_session.getGameLinkSession()->getNumPartyMembers() > 1)
        {
            Multi_join_in_progress = false;
            sp->join_received_join_denied(me, MSJS_JOIN_DENY_SESSION_LOCKED);
            g_gameLinkMutex.unlock();
            return;
        }
        g_gameLinkMutex.unlock();
    }
    Client_state = CLIENT_STATE_ESTABLISHED;
    sp->join_received_join_accept(from.connection);
    if (Multi_join_in_progress)
    {
        unsigned char packet[1200];
        packet[0] = PT_REQUEST_JIP;
        const unsigned int size = Multi_packets[PT_REQUEST_JIP].group_id != 0 ? 5 : 3;
        packet[1] = (unsigned char)((size - 3) >> 8);
        packet[2] = (unsigned char)(size - 3);
        multi_io_send_reliable(from.connection, packet, size, nullptr, nullptr, nullptr);
    }
}

// 0x009B7AC0 (:712): the version, the session id, the reason. Another version denies with MSJS_JOIN_DENY_VERSION
// (to the sender's session, when known).
void multi_join_process_join_deny_packet(unsigned char* packet_data, const sender& from)
{
    multi_session* sp = from.connection != nullptr ? from.connection->owner : nullptr;
    multi_session_join_status reason = MSJS_JOIN_DENY_VERSION;
    int packet_offset = 0;
    if (read_u32_be(packet_data, packet_offset) == multi_get_multiplayer_version())
    {
        // Magic number: the reason follows the version and the 8-byte session id
        reason = (multi_session_join_status)packet_data[12];
        multi_session_info info;
        info.read_id_from_packet(packet_data, packet_offset);
        sp = multi_session_find_by_id(info);
    }
    if (sp != nullptr)
    {
        sp->join_received_join_denied(from.connection, reason);
    }
}

// 0x009B7C30 (:748)
void multi_join_send_join_new_session_packet(multi_connection* cp, multi_session_info* join_target_info)
{
    unsigned char packet[1200];
    packet[0] = PT_JOIN_NEW_SESSION;
    unsigned short size = Multi_packets[PT_JOIN_NEW_SESSION].group_id != 0 ? 5 : 3;
    join_target_info->append_join_info_to_packet(packet, size);
    cp->owner->join_sending_join_new_target(packet, size, join_target_info);
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
}

// 0x009B7CF0 (:761): from.connection is not checked for null (as in rfg.exe)
void multi_join_process_join_new_session_packet(unsigned char* packet_data, const sender& from)
{
    multi_session_info info;
    int packet_offset = 0;
    info.read_join_info_from_packet(packet_data, packet_offset);
    info.invited = true;
    from.connection->owner->join_received_join_new_session(packet_data, packet_offset, from.connection, &info);
}

// 0x0094B700 (:774)
void multi_join_send_join_success_packet(multi_connection* cp, multi_session* join_target, multi_session* join_source)
{
}

// 0x0094B710 (:787)
void multi_join_process_join_success_packet(unsigned char* packet_data, const sender& from)
{
}

// 0x009B7DB0 (:809)
void multi_join_send_join_failed_packet(multi_connection* cp, multi_session* join_target, multi_session* join_source)
{
    unsigned char packet[1200];
    packet[0] = PT_JOIN_FAILED;
    unsigned short size = Multi_packets[PT_JOIN_FAILED].group_id != 0 ? 5 : 3;
    join_target->get_info()->append_id_to_packet(packet, size);
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
}

// 0x009B7E50 (:819): looks the session up and does nothing with it (join_received_join_failed is empty)
void multi_join_process_join_failed_packet(unsigned char* packet_data, const sender& from)
{
    multi_session_info info;
    int packet_offset = 0;
    info.read_id_from_packet(packet_data, packet_offset);
    multi_session_find_by_id(info);
}

// 0x009B7F10 (:833)
void multi_join_send_join_complete_packet(multi_connection* cp, multi_session* join_target, multi_session* join_source)
{
    unsigned char packet[1200];
    packet[0] = PT_JOIN_COMPLETE;
    unsigned short size = Multi_packets[PT_JOIN_COMPLETE].group_id != 0 ? 5 : 3;
    join_target->get_info()->append_id_to_packet(packet, size);
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
}

// 0x009B7FB0 (:843): looks the session up and does nothing with it (join_received_join_complete is empty)
void multi_join_process_join_complete_packet(unsigned char* packet_data, const sender& from)
{
    multi_session_info info;
    int packet_offset = 0;
    info.read_id_from_packet(packet_data, packet_offset);
    multi_session_find_by_id(info);
}

// 0x009B8070 (:855)
void multi_join_send_remove_reservation(multi_connection* cp, multi_session* session, const multi_player_uid& reservee)
{
    unsigned char packet[1200];
    packet[0] = PT_RESERVATION_REMOVE;
    unsigned short size = Multi_packets[PT_RESERVATION_REMOVE].group_id != 0 ? 5 : 3;
    reservee.write_to_packet(packet, size);
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
}

// 0x00973F80 (:865): from.connection is not checked for null (as in rfg.exe)
void multi_join_process_remove_reservation(unsigned char* packet_data, const sender& from)
{
    multi_player_uid uid;
    int packet_offset = 0;
    uid.read_from_packet(packet_data, packet_offset);
    from.connection->owner->unreserve_slots(uid);
}

// 0x009DB630 (:880): the host to every connected peer but the connection itself and us
void multi_join_send_updated_connection_packet_to_all(multi_connection* conn)
{
    multi_session* sp = conn->owner;
    if (!sp->am_i_host())
    {
        return;
    }
    multi_connection* to_cp = sp->get_connection_list();
    while (to_cp != nullptr)
    {
        if (to_cp->connection_status != CS_DISCONNECTED && to_cp != conn && to_cp != to_cp->owner->get_me_connection())
        {
            multi_join_send_updated_connection_packet(conn, to_cp);
        }
        to_cp = to_cp->next;
        if (to_cp == sp->get_connection_list())
        {
            break;
        }
    }
}

// 0x009B8110 (:898): the index and a light connection update
void multi_join_send_updated_connection_packet(multi_connection* data_cp, multi_connection* to_cp)
{
    unsigned char packet[1200];
    packet[0] = PT_UPDATED_CONNECTION;
    unsigned short size = Multi_packets[PT_UPDATED_CONNECTION].group_id != 0 ? 5 : 3;
    packet[size++] = data_cp->index_in_session;
    append_connection_update_to_packet(packet, size, data_cp, true);
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    if (to_cp->connection_status != CS_DISCONNECTED)
    {
        multi_io_send_reliable(to_cp, packet, size, nullptr, nullptr, nullptr);
    }
}

// 0x00973FD0 (:922): clients only
void multi_join_process_updated_connection_packet(unsigned char* packet_data, const sender& from)
{
    if (from.connection == nullptr || from.connection->owner->am_i_host())
    {
        return;
    }
    multi_connection* cp = from.connection->owner->get_connection_by_index(packet_data[0]);
    if (cp != nullptr)
    {
        int packet_offset = 1;
        read_connection_update_from_packet(packet_data, packet_offset, cp, true);
    }
}

// 0x009AA910 (:950): clients only: a connection by its index (allocated and added when new)
void multi_join_process_new_connection_packet(unsigned char* packet_data, const sender& from)
{
    multi_connection* from_cp = from.connection;
    if (from_cp == nullptr || from_cp->owner->am_i_host())
    {
        return;
    }
    connection_address address;
    int packet_offset = 0;
    if (!address.read_from_packet(packet_data, packet_offset))
    {
        return;
    }
    multi_session* sp = from_cp->owner;
    const unsigned char index = packet_data[packet_offset];
    multi_connection* cp = sp->get_connection_by_index(index);
    const bool known = cp != nullptr;
    if (!known)
    {
        cp = multi_connection_allocate(&address, sp, nullptr, nullptr, false, false);
        cp->index_in_session = index;
    }
    packet_offset++;
    cp->local_nonce = (int)read_u32_be(packet_data, packet_offset);
    cp->game_data = read_u32_be(packet_data, packet_offset);
    cp->upstream_bits_sec = read_u32_be(packet_data, packet_offset);
    cp->player_uid.read_from_packet(packet_data, packet_offset);
    cp->reserver_uid.read_from_packet(packet_data, packet_offset);
    cp->game_data = multi_game_data_update_player_data(packet_data, packet_offset, false);
    if (!known)
    {
        from.connection->owner->add_connection(cp, cp->index_in_session, 0);
    }
}

// 0x00974020 (:995): clients only
void multi_join_process_new_game_data_packet(unsigned char* packet_data, const sender& from)
{
    if (from.connection != nullptr && !from.connection->owner->am_i_host())
    {
        int packet_offset = 0;
        multi_game_data_update_player_data(packet_data, packet_offset, false);
    }
}

// 0x00974060 (:1011): clients only
void multi_join_process_remove_game_data_packet(unsigned char* packet_data, const sender& from)
{
    if (from.connection != nullptr && !from.connection->owner->am_i_host())
    {
        int packet_offset = 0;
        multi_game_data_remove_player(read_u32_be(packet_data, packet_offset));
    }
}

// 0x009B81C0 (:1032): the version, the session id (zero without a session), the reason; sent at once
void multi_join_send_join_deny_packet(multi_connection* conn, multi_session_join_status reason)
{
    unsigned char packet[1200];
    packet[0] = PT_JOIN_DENY;
    unsigned short size = Multi_packets[PT_JOIN_DENY].group_id != 0 ? 5 : 3;
    write_u32_be(packet, size, multi_get_multiplayer_version());
    const multi_session_info_base* info = conn != nullptr && conn->owner != nullptr ? conn->owner->get_info() : nullptr;
    if (info == nullptr)
    {
        for (int i = 0; i < 8; i++)
        {
            packet[size++] = 0;
        }
    }
    else
    {
        info->append_id_to_packet(packet, size);
    }
    packet[size++] = (unsigned char)reason;
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_io_send(conn, packet, size);
}

// 0x009B8370 (:1060): the host to every connected peer but the newcomer and the host
void multi_join_send_new_connection_packet_to_all(multi_connection* new_connection)
{
    multi_session* sp = new_connection->owner;
    if (!sp->am_i_host())
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_NEW_CONNECTION;
    unsigned short size = Multi_packets[PT_NEW_CONNECTION].group_id != 0 ? 5 : 3;
    append_new_connection_to_packet(packet, size, new_connection);
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_connection* cp = sp->get_connection_list();
    while (cp != nullptr)
    {
        if (cp->connection_status != CS_DISCONNECTED && cp != new_connection && cp != cp->owner->get_host_connection())
        {
            multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
        }
        cp = cp->next;
        if (cp == sp->get_connection_list())
        {
            break;
        }
    }
}

// 0x009B8450 (:1088): the host sends the newcomer every other connected peer, then the game data without a connection
void multi_join_send_connections(multi_connection* conn)
{
    if (!conn->owner->am_i_host())
    {
        return;
    }
    multi_connection* cp = conn->owner->get_connection_list();
    while (cp != nullptr)
    {
        if (cp->connection_status != CS_DISCONNECTED && cp != conn && cp != conn->owner->get_host_connection())
        {
            unsigned char packet[1200];
            packet[0] = PT_NEW_CONNECTION;
            unsigned short size = Multi_packets[PT_NEW_CONNECTION].group_id != 0 ? 5 : 3;
            append_new_connection_to_packet(packet, size, cp);
            packet[1] = (unsigned char)((size - 3) >> 8);
            packet[2] = (unsigned char)(size - 3);
            multi_io_send_reliable(conn, packet, size, nullptr, nullptr, nullptr);
        }
        cp = cp->next;
        if (cp == conn->owner->get_connection_list())
        {
            break;
        }
    }
    for (const multi_game_data* data = multi_game_data_begin(conn->owner->get_info()->type, false, false); data != nullptr;
         data = multi_game_data_next(data, false, false))
    {
        const unsigned int handle = data->handle;
        if (conn->owner->get_player_connection(handle) == nullptr)
        {
            unsigned char packet[1200];
            packet[0] = PT_NEW_GAME_DATA;
            unsigned short size = Multi_packets[PT_NEW_GAME_DATA].group_id != 0 ? 5 : 3;
            if (Multiplayer && handle != 0xFFFFFFFFu)
            {
                multi_game_data* full = game_data_of(handle);
                append_player_data_full(packet, size, full, full->handle);
            }
            packet[1] = (unsigned char)((size - 3) >> 8);
            packet[2] = (unsigned char)(size - 3);
            multi_io_send_reliable(conn, packet, size, nullptr, nullptr, nullptr);
        }
    }
}

// 0x009B8620 (:1127)
void multi_join_send_remove_gamedata(multi_connection* conn, unsigned int game_data_handle)
{
    if (!conn->owner->am_i_host())
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_REMOVE_GAME_DATA;
    unsigned short size = Multi_packets[PT_REMOVE_GAME_DATA].group_id != 0 ? 5 : 3;
    write_u32_be(packet, size, game_data_handle);
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_io_send_reliable(conn, packet, size, nullptr, nullptr, nullptr);
}
