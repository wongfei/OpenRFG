// Original: volition/rfg/code/multi/multi_game/multi_game_destruction_migration.cpp
//
// Destruction during server migration (multi_game_migration.cpp): the new server compares the last destruction frame
// each client applied with its own, asks the client that got furthest for the frames it misses
// (PT_MIGRATE_REQ_DF_UPDATE, answered with one PT_MIGRATE_DF_UPDATE per frame, the frame's packet as it is), and sends
// every client the frames from its next one up to the target (PT_MIGRATE_DF_UPDATE: the frame after the last one, a
// has-data byte, the frame's packet). The clients answer PT_MIGRATE_DESTRUCT_COMPLETE when they applied them. The same
// client packet also brings a joining client's destruction in (jip_destruction_sync_state, multi_game_destruction.cpp).
// Frame numbers are 16 bits; 0xFFFF is "none" and the frame after it is 0.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_destruction_migration.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_destruction.h"
#include "volition/rfg/code/multi/multi_game/multi_game_migration.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"

// ---- file-static globals
// jip_destruction_sync jip_destruction_sync_state (0x0167E708) is used here too; placed in multi.cpp.
// unsigned short Playback_next_client_destruction_frame (0x02FE6C74) is used here too; placed in multi_game_destruction.cpp.
// destruction_frame_data* MP_destruction_frame_used_list (0x02FE73C4) is used here too; placed in multi_game_destruction.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.
// migration_state Migration_state (0x02FEB6C4) is used here too; placed in multi.cpp.

// write_u16_be/read_u16_be, packet_header_size, packet_set_size and game_data_of are the port helpers of
// multi_join.cpp. rfg.exe inlines get_destruction_next_frame_id and the destruction file's last frame queries
// (multi_game_destruction_get_last_applied_destruction_frame, _get_last_destruction_frame) everywhere here.

// ---- internal functions

// 0x00955AD0 (:133): no caller left in rfg.exe. LTCG: cp in EAX.
static unsigned short get_largest_applied_destruction_frame_by_connection(const multi_connection* cp)
{
    return game_data_of(cp->game_data)->dm_data.last_applied_frame;
}

// 0x0097B7F0 (:415): the new server stores a frame a client sent and remembers the newest one per client. LTCG:
// packet_data in EAX, from in EDI.
static void multi_game_process_destruct_update_server(unsigned char* packet_data, const sender& from)
{
    if (Migration_state != MIGRATION_SERVER_WAIT_FOR_DESTRUCTION_UPDATE)
    {
        return;
    }
    multi_game_destruction_process_packet(packet_data, from);
    int offset = 0;
    const unsigned short frame = read_u16_be(packet_data, offset); // the frame packet's destruction frame
    if (from.connection != nullptr)
    {
        unsigned short& last_sent_frame = game_data_of(from.connection->game_data)->dm_data.last_sent_frame;
        // Original: a plain compare (no wrap-around), unlike the other frame compares.
        if (last_sent_frame == 0xFFFF || frame > last_sent_frame)
        {
            last_sent_frame = frame;
        }
    }
}

// 0x009DE980 (:449): a migrating client (or a joining one) stores the frame and, when every frame before the
// announced next one is in, waits for them to be applied. Outside a join the frames from the next one on are dropped
// first. LTCG: packet_data in EAX.
static void multi_game_process_destruct_update_client(unsigned char* packet_data, const sender& from)
{
    if (Migration_state != MIGRATION_CLIENT_WAIT_FOR_DESTRUCTION_UPDATE && jip_destruction_sync_state == JIP_DESTRUCTION_STATE_NONE)
    {
        return;
    }
    int offset = 0;
    const unsigned short next_frame = read_u16_be(packet_data, offset);
    const bool has_data = packet_data[offset] != 0;
    if (jip_destruction_sync_state == JIP_DESTRUCTION_STATE_NONE)
    {
        multi_game_destruction_cull_destruction_frames_starting(next_frame);
    }
    if (has_data)
    {
        multi_game_destruction_process_packet(packet_data + 3, from);
    }
    if (next_frame != 0 && !multi_game_destruction_all_frames_present_up_to((unsigned short)(next_frame - 1)))
    {
        return;
    }
    if (jip_destruction_sync_state != JIP_DESTRUCTION_STATE_NONE)
    {
        jip_destruction_sync_state = JIP_DESTRUCTION_STATE_WAIT_FOR_DESTRUCTION_APPLIED;
        return;
    }
    Migration_state = MIGRATION_CLIENT_WAIT_FOR_DESTRUCTION_APPLIED;
}

// ---- external interface

// 0x009C57A0 (:75): PT_MIGRATE_REQ_DF_UPDATE, the first and the last frame wanted.
void multi_game_send_req_destruct_update(multi_connection* cp, unsigned short start_frame_index, unsigned short end_frame_index)
{
    unsigned char packet[1200];
    packet[0] = PT_MIGRATE_REQ_DF_UPDATE;
    unsigned short packet_size = packet_header_size(PT_MIGRATE_REQ_DF_UPDATE);
    write_u16_be(packet, packet_size, start_frame_index);
    write_u16_be(packet, packet_size, end_frame_index);
    packet_set_size(packet, packet_size);
    multi_io_send_reliable(cp, packet, packet_size, nullptr, nullptr, nullptr);
}

// 0x009C5860 (:95): a client sends the frames asked for, one packet each.
void multi_game_process_migrate_req_df_update(unsigned char* packet_data, const sender& from)
{
    multi_connection* cp = from.connection;
    int offset = 0;
    const unsigned short start_frame = read_u16_be(packet_data, offset);
    const unsigned short end_frame = read_u16_be(packet_data, offset);
    // rfg.exe reuses the start frame's variable for the byte count.
    int num_bytes = start_frame;
    for (int frame = start_frame; frame <= end_frame; frame++)
    {
        unsigned char packet[1200];
        packet[0] = PT_MIGRATE_DF_UPDATE;
        unsigned short packet_size = packet_header_size(PT_MIGRATE_DF_UPDATE);
        // Original: a frame the client does not have keeps the previous byte count and copies from null.
        const unsigned char* frame_data = multi_game_destruction_get_destruction_frame_data(frame, num_bytes);
        for (int i = 0; i < num_bytes; i++)
        {
            packet[packet_size++] = frame_data[i];
        }
        packet_set_size(packet, packet_size);
        multi_io_send_reliable(cp, packet, packet_size, nullptr, nullptr, nullptr);
    }
}

// 0x00955B00 (:141): whether the first frame comes after the second (0xFFFF, none, before every frame).
bool is_first_frame_larger(unsigned short first_frame, unsigned short second_frame)
{
    return get_destruction_next_frame_id(second_frame) < get_destruction_next_frame_id(first_frame);
}

// 0x00998BF0 (:149): the newest frame a client applied, and that client (none: 0xFFFF and null).
void get_largest_client_applied_destruction_frame(unsigned short& client_largest_applied_frame, multi_connection** largest_destruction_frame_cp)
{
    client_largest_applied_frame = 0xFFFF;
    *largest_destruction_frame_cp = nullptr;
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    multi_connection* host = sp->get_host_connection();
    multi_connection* cp = sp->get_connection_list();
    do
    {
        if (cp == nullptr)
        {
            return;
        }
        if (cp != host)
        {
            const unsigned short frame = game_data_of(cp->game_data)->dm_data.last_applied_frame;
            if (is_first_frame_larger(frame, client_largest_applied_frame)) // (inlined)
            {
                client_largest_applied_frame = frame;
                *largest_destruction_frame_cp = cp;
            }
        }
        cp = cp->next;
    } while (cp != sp->get_connection_list());
}

// 0x00998CB0 (:170): the newest frame anyone applied, this server or a client.
unsigned short get_target_destruction_frame()
{
    unsigned short client_frame;
    multi_connection* client;
    get_largest_client_applied_destruction_frame(client_frame, &client);
    const unsigned short server_frame = multi_game_destruction_get_last_applied_destruction_frame();
    if (!is_first_frame_larger(client_frame, server_frame)) // (inlined)
    {
        return server_frame;
    }
    return client_frame;
}

// 0x009C5980 (:187): the client's frames from start to end, one packet each, behind the frame after the end and a
// has-data byte; nothing to send (no end frame or an empty range) is one packet without data.
void multi_game_migration_send_client_destruction_update(multi_connection* cp, unsigned short start_index, unsigned short end_index)
{
    const unsigned short next_frame = get_destruction_next_frame_id(end_index); // (inlined)
    unsigned char packet[1200];
    if (end_index != 0xFFFF && start_index <= end_index)
    {
        // rfg.exe leaves the byte count uninitialized; a missing frame keeps the previous one.
        int num_bytes = 0;
        for (int frame = start_index; frame <= end_index; frame++)
        {
            packet[0] = PT_MIGRATE_DF_UPDATE;
            unsigned short packet_size = packet_header_size(PT_MIGRATE_DF_UPDATE);
            write_u16_be(packet, packet_size, next_frame);
            packet[packet_size++] = 1;
            // Original: a frame the server does not have copies from null.
            const unsigned char* frame_data = multi_game_destruction_get_destruction_frame_data(frame, num_bytes);
            for (int i = 0; i < num_bytes; i++)
            {
                packet[packet_size++] = frame_data[i];
            }
            packet_set_size(packet, packet_size);
            multi_io_send_reliable(cp, packet, packet_size, nullptr, nullptr, nullptr);
        }
        return;
    }
    packet[0] = PT_MIGRATE_DF_UPDATE;
    unsigned short packet_size = packet_header_size(PT_MIGRATE_DF_UPDATE);
    write_u16_be(packet, packet_size, next_frame);
    packet[packet_size++] = 0;
    packet_set_size(packet, packet_size);
    multi_io_send_reliable(cp, packet, packet_size, nullptr, nullptr, nullptr);
}

// 0x009C5B30 (:242): every client gets the frames after its last applied one, up to the target.
void multi_game_migration_send_client_destruction_updates(unsigned short last_destruction_frame)
{
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    multi_connection* cp = sp->get_connection_list();
    multi_connection* host = sp->get_host_connection();
    do
    {
        if (cp == nullptr)
        {
            return;
        }
        if (cp != host)
        {
            multi_game_migration_send_client_destruction_update(cp, get_destruction_next_frame_id(game_data_of(cp->game_data)->dm_data.last_applied_frame),
                                                                last_destruction_frame);
        }
        cp = cp->next;
    } while (cp != sp->get_connection_list());
}

// 0x009C5BB0 (:271): no caller in rfg.exe (process_migration_server_wait_for_destruction_update does the same).
void process_destruction_sent_update_state_change()
{
    const unsigned short last_destruction_frame = get_target_destruction_frame();
    multi_game_destruction_cull_destruction_frames_starting(get_destruction_next_frame_id(last_destruction_frame));
    multi_game_migration_send_client_destruction_updates(last_destruction_frame);
}

// 0x009C5C00 (:288): when a client applied frames this server has not, every connected client ahead of it is asked
// for the frames from this server's next one to the client's last.
void request_destruction_updates_from_clients()
{
    unsigned short client_frame;
    multi_connection* client;
    get_largest_client_applied_destruction_frame(client_frame, &client);
    const unsigned short server_frame = multi_game_destruction_get_last_applied_destruction_frame();
    if (!is_first_frame_larger(client_frame, server_frame)) // (inlined)
    {
        return;
    }
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    multi_connection* cp = sp->get_connection_list();
    multi_connection* host = sp->get_host_connection();
    do
    {
        if (cp == nullptr)
        {
            return;
        }
        if (cp != host && (cp->connection_status == CS_CONNECTED || cp->connection_status == CS_CONNECTED_GAMEPLAY))
        {
            const unsigned short frame = game_data_of(cp->game_data)->dm_data.last_applied_frame;
            if (is_first_frame_larger(frame, server_frame)) // (inlined)
            {
                multi_game_send_req_destruct_update(cp, get_destruction_next_frame_id(server_frame), frame);
            }
        }
        cp = cp->next;
    } while (cp != sp->get_connection_list());
}

// 0x00998D20 (:328): every client answered PT_MIGRATE_REQ_STATUS (its last frame and its destroyed movers list).
bool have_all_clients_sent_destruction_status()
{
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    multi_connection* list = sp->get_connection_list();
    multi_connection* cp = list;
    while (cp != nullptr)
    {
        if (cp != sp->get_host_connection())
        {
            const destruction_migration_data& dm_data = game_data_of(cp->game_data)->dm_data;
            if (!dm_data.last_destruction_frame_recvd || !dm_data.destroyed_general_movers_recvd)
            {
                return false;
            }
        }
        cp = cp->next;
        if (cp == list)
        {
            break;
        }
    }
    return true;
}

// 0x00998D90 (:349): every client ahead of this server sent its last frame, and every frame up to the target is here.
bool have_all_clients_sent_destruction_updates()
{
    const unsigned short server_frame = multi_game_destruction_get_last_applied_destruction_frame();
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    multi_connection* host = sp->get_host_connection();
    multi_connection* list = sp->get_connection_list();
    multi_connection* cp = list;
    while (cp != nullptr)
    {
        if (cp != host)
        {
            const destruction_migration_data& dm_data = game_data_of(cp->game_data)->dm_data;
            if (is_first_frame_larger(dm_data.last_applied_frame, server_frame) && dm_data.last_sent_frame != dm_data.last_applied_frame) // (inlined)
            {
                return false;
            }
        }
        cp = cp->next;
        if (cp == list)
        {
            break;
        }
    }
    return multi_game_destruction_all_frames_present_up_to(get_target_destruction_frame()); // (inlined)
}

// 0x00955B40 (:386): the stored frames play back.
void process_migration_destruction()
{
    if (multi_game_destruction_is_pending())
    {
        rfg_dyn_do_frame_pre();
        rfg_dyn_do_collide_post();
    }
}

// 0x00998EC0 (:395): no caller in rfg.exe. The newest frame a client applied.
unsigned short get_server_desired_last_destruction_frame()
{
    unsigned short desired_frame = 0xFFFF;
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    multi_connection* list = sp->get_connection_list();
    if (list == nullptr)
    {
        return 0xFFFF;
    }
    multi_connection* cp = list;
    do
    {
        if (cp != sp->get_host_connection())
        {
            const unsigned short frame = game_data_of(cp->game_data)->dm_data.last_applied_frame;
            if (is_first_frame_larger(frame, desired_frame)) // (inlined)
            {
                desired_frame = frame;
            }
        }
        cp = cp->next;
    } while (cp != list && cp != nullptr);
    return desired_frame;
}

// 0x00955B60 (:501): the newest stored frame is the last one played back.
bool has_all_destruction_been_applied()
{
    return multi_game_destruction_get_last_destruction_frame() == multi_game_destruction_get_last_applied_destruction_frame();
}

// 0x00998F70 (:513): every client answered PT_MIGRATE_DESTRUCT_COMPLETE.
bool have_all_client_applied_destruction()
{
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    multi_connection* list = sp->get_connection_list();
    multi_connection* cp = list;
    while (cp != nullptr)
    {
        if (cp != sp->get_host_connection() && !game_data_of(cp->game_data)->dm_data.applied_destruction)
        {
            return false;
        }
        cp = cp->next;
        if (cp == list)
        {
            break;
        }
    }
    return true;
}

// 0x009C5D20 (:533): PT_MIGRATE_DESTRUCT_COMPLETE (no payload) to the new server.
void multi_client_send_destruction_applied()
{
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    multi_connection* host = sp != nullptr ? sp->get_host_connection() : nullptr;
    unsigned char packet[1200];
    packet[0] = PT_MIGRATE_DESTRUCT_COMPLETE;
    const unsigned short packet_size = packet_header_size(PT_MIGRATE_DESTRUCT_COMPLETE);
    packet_set_size(packet, packet_size);
    multi_io_send_reliable(host, packet, packet_size, nullptr, nullptr, nullptr);
}

// 0x009DEA30 (:549): PT_MIGRATE_DF_UPDATE.
void multi_game_process_migrate_df_update(unsigned char* packet_data, const sender& from)
{
    if (Multiplayer && Server)
    {
        multi_game_process_destruct_update_server(packet_data, from);
        return;
    }
    multi_game_process_destruct_update_client(packet_data, from);
}

// 0x00955BA0 (:562): PT_MIGRATE_DESTRUCT_COMPLETE.
void multi_game_process_migrate_destruct_complete(unsigned char* packet_data, const sender& from)
{
    if (from.connection != nullptr)
    {
        game_data_of(from.connection->game_data)->dm_data.applied_destruction = true;
    }
}

// 0x0097B870 (:578): a client always takes the destroyed movers list; the new server only while it waits for the
// clients' status.
bool multi_game_migration_can_accept_destroyed_mover_list()
{
    if (!Multiplayer)
    {
        return false;
    }
    if (Multiplayer && Server)
    {
        return Migration_state == MIGRATION_SERVER_WAIT_DESTRUCTION_STATUS;
    }
    return true;
}
