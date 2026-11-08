// Original: volition/rfg/code/multi/multi_core/global/multi_leave.cpp
//
// Leaving: the host tells the session a connection left (PT_CONNECTION_LEFT: its index and disconnect reason); a
// player leaving a game tells the host, the host tells everyone (PT_LEFT_GAME).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_core/include/multi_leave.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"

// 0x009B8700 (:95): to every connected peer but the leaver and ourselves
void multi_leave_send_connection_left_packet_to_all(multi_connection* conn)
{
    if (conn == nullptr || conn->owner == nullptr)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_CONNECTION_LEFT;
    const unsigned int header = Multi_packets[PT_CONNECTION_LEFT].group_id != 0 ? 5 : 3;
    packet[header] = conn->index_in_session;
    packet[header + 1] = (unsigned char)conn->disconnect_reason;
    const unsigned int size = header + 2;
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_connection* cp = conn->owner->get_connection_list();
    while (cp != nullptr)
    {
        if (cp->connection_status != CS_DISCONNECTED && cp != conn && cp != cp->owner->get_me_connection())
        {
            multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
        }
        cp = cp->next;
        if (cp == conn->owner->get_connection_list())
        {
            break;
        }
    }
}

// 0x009DB690 (:118): only from the host. from.connection is not checked for null (as in rfg.exe).
void multi_leave_process_connection_left_packet(unsigned char* packet_data, const sender& from)
{
    multi_session* sp = from.connection->owner;
    multi_connection* host = sp->get_host_connection();
    if (host == nullptr || host != from.connection)
    {
        return;
    }
    multi_connection* const connections = sp->get_connection_list();
    multi_connection* cp = connections;
    while (cp != nullptr)
    {
        if (cp->index_in_session == packet_data[0])
        {
            cp->disconnect_reason = (multi_connection_disconnect_reason)packet_data[1];
            from.connection->owner->received_connection_left(cp);
            return;
        }
        cp = cp->next;
        if (cp == connections)
        {
            break;
        }
    }
}

// 0x009AAB10 (:152): the host tells everyone and sends at once; a client tells the host
void multi_game_send_left_game(multi_session* session)
{
    multi_connection* host = session->get_host_connection();
    if (host == nullptr || host->connection_status == CS_DISCONNECTED)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_LEFT_GAME;
    const unsigned int size = Multi_packets[PT_LEFT_GAME].group_id != 0 ? 5 : 3;
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    if (session->am_i_host())
    {
        multi_io_send_to_all(session, packet, size);
        multi_io_force_send_all_buffered_packets();
        return;
    }
    multi_io_send(host, packet, size);
    multi_connection_flush_current_packet(host);
}

// 0x009DB6F0 (:185): received_connection_left inlined
void multi_game_process_left_game(unsigned char* packet_data, const sender& from)
{
    if (from.connection != nullptr)
    {
        from.connection->owner->received_connection_left(from.connection);
    }
}
