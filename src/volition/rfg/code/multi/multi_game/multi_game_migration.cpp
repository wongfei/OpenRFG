// Original: volition/rfg/code/multi/multi_game/multi_game_migration.cpp
//
// Server migration: when the server leaves, the session layer makes another machine the host and this file brings the
// game over. The new server asks every client for its last applied destruction frame (PT_MIGRATE_REQ_STATUS, answered
// with PT_MIGRATE_LAST_DF and the destroyed movers list), fetches the frames it misses, sends each client the frames it
// misses up to the agreed target (multi_game_destruction_migration.cpp), waits until everyone applied them, destroys the
// projectiles and the unreplicated general movers, rebuilds the delta compression state of its remote objects and sets
// the clients up again; after 5 s it tells them it is done (PT_MIGRATION_COMPLETE). A client waits for the frames,
// applies them, destroys its remote copies (but the ones the level owns: its own player, preplaced items, movers) and
// resets its remote object system. A client that lags 16 frames behind is dropped; a new server 16 frames behind its
// clients, or a step that times out, gives up and returns to the main menu.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_migration.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_destruction.h"
#include "volition/rfg/code/multi/multi_game/multi_game_destruction_migration.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_movers.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_projectile.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_rfg_movers.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_types.h"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_event.h"
#include "volition/rfg/code/multi/multi_game/multi_game_timer.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_delta_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_qos.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/gamewide.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/movers/general_mover.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/time/timestamp.h"
#include "keen/base/trace.hpp"

// ---- file-static globals
// unsigned short Cur_network_handle (0x0167DD78) is used here too; placed in multi.cpp.
// use_object_icon Team_swap_icons_ctf[2] (0x0167DEA8) is used here too; placed in multi_game_mode_common.cpp.
// int Score_play_id (0x0167DEB0) is used here too; placed in multi_game_mode_demolition.cpp.
// multi_gameplay_mode Multi_gameplay_mode (0x0167DEE0) is used here too; placed in multi_gameplay_mode.cpp.
// timestamp Client_standby_hack_check (0x016916C0) is used here too; placed in multi_connection.cpp.
// dynamic_uint_hash_table<remote_object*> Game_handle_to_rop_hash (0x01691B40) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<remote_object*> Network_handle_to_rop_hash (0x01691B88) is used here too; placed in multi_core_delta_compression.cpp.
static timestamp_realtime Server_migration_timeout; // 0x01692DB8, initial data ffffffff
static timestamp_realtime Client_migration_timout; // 0x01692DBC, initial data ffffffff
// Set when the migration starts; nothing reads it.
static timestamp_realtime Debug_time; // 0x01692DC0, initial data ffffffff
// timestamp_realtime Mgm_allow_migration (0x01692DC4) is used here too; placed in multi_game_mode_common.cpp.
// multi_timer Timer_time (0x01693A64) is used here too; placed in multi_core_platform.cpp.
// unsigned short Playback_next_client_destruction_frame (0x02FE6C74) is used here too; placed in multi_game_destruction.cpp.
// destruction_frame_data* MP_destruction_frame_used_list (0x02FE73C4) is used here too; placed in multi_game_destruction.cpp.
// int Base_score_penalty_for_vetoing (0x02FEB37C) is used here too; placed in multi_core_delta_compression.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.
// A client's answer to PT_MIGRATE_REQ_STATUS did not go out (the lobby was migrating or a send failed): it is sent
// again from client_migration_do_frame.
static bool s_retryClientSendLastDestructionFrame; // 0x02FEB6C3
// migration_state Migration_state (0x02FEB6C4) is used here too; placed in multi.cpp.
// bool Multi_game_has_migrated (0x02FEB6C8) is used here too; placed in multi_game.cpp.
// bool Multi_game_has_skipped_game_migration (0x02FEB6C9) is used here too; placed in multi_game.cpp.
// int Time_last_check (0x02FEB758) is used here too; placed in multi_game_mode_common.cpp.

// Magic numbers: the migration timeouts (ms) and the destruction lag (frames) that drops a client or ends the game.
static const int Migration_allow_ms = 10000;
static const int Client_wait_for_destruction_update_ms = 8000;
static const int Client_wait_for_create_process_ms = 12000;
static const int Server_wait_for_create_process_ms = 5000;
static const int Client_standby_hack_check_ms = 15000;
static const int Migration_max_destruction_lag = 16;

// write_u16_be/read_u16_be, packet_header_size, packet_set_size and game_data_of are the port helpers of
// multi_join.cpp.

// ---- internal functions

// 0x009DF060 (:188): the new server destroys its general movers that are not replicated (mover flag 0x4, see
// multi_game_object_movers.cpp), without destroy packets, and clears the destroyed general movers list.
static void multi_game_migration_server_cull_general_movers()
{
    const int count = World.get_num_objects<general_mover>();
    for (int i = 0; i < count; i++)
    {
        general_mover* gmp = World.get_object<general_mover>(i);
        if ((gmp->mover_flags & 0x4) != 0)
        {
            gmp->remote_obj_flags.no_destroy_packet = 1;
            World.destroy_object(gmp, false);
        }
    }
    multi_game_migration_destroy_general_mover_destroyed_list();
}

// 0x009F8330 (:205): no caller left in rfg.exe (process_migration_client_wait_for_destruction_applied_post has its
// body inlined).
static void multi_client_reset_remote_object_system()
{
    multi_core_sim_event_clear();
    Game_handle_to_rop_hash.clear();
    Network_handle_to_rop_hash.clear();
    // Magic number: the first network handle (multi.cpp's initial value).
    Cur_network_handle = 1000;
    multi_core_object_init_common();
    multi_game_init_remote_game_objects(true);
    multi_core_dc_reset();
}

// 0x009C6830 (:218): a client frees the state data of every registered type and destroys its remote objects, but the
// ones the level or the client owns: its own player, the preplaced items, the replicated general movers and the rfg
// movers lose their remote object flag instead. Another player is removed from the mode first.
static void destroy_client_remote_objects()
{
    player* local_player = player_get_local_player();
    for (int i = 0; i < MULTI_OBJECT_TYPE_MAX; i++)
    {
        if (Object_type_registration_list[i] != nullptr)
        {
            Object_type_registration_list[i]->free_mp_state_data();
        }
    }
    rfg_mover_free_mp_state_data();
    for (int type = MULTI_OBJECT_TYPE_FIRST; type < MULTI_OBJECT_TYPE_MAX; type++)
    {
        remote_object* rop = Remote_objects_by_type[type];
        while (rop != nullptr)
        {
            remote_object* next = rop->type_next != Remote_objects_by_type[type] ? rop->type_next : nullptr;
            object* objp = World.object_from_handle(rop->m_game_handle);
            rop = next;
            if (objp == nullptr)
            {
                continue;
            }
            bool keep = false;
            switch (type)
            {
            case MULTI_OBJECT_TYPE_PROJECTILE:
            case MULTI_OBJECT_TYPE_VEHICLE:
            case MULTI_OBJECT_TYPE_TRIGGER:
            case MULTI_OBJECT_TYPE_WEAPON_PICKUP:
                break;
            case MULTI_OBJECT_TYPE_PLAYER:
            {
                player* pp = objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)objp : nullptr;
                if (pp == local_player)
                {
                    // Original: a null local player keeps a remote object that is not a player.
                    keep = true;
                }
                else if (pp != nullptr)
                {
                    multi_gameplay_mode_remove_player(pp); // (inlined in rfg.exe)
                }
                break;
            }
            case MULTI_OBJECT_TYPE_ITEM:
                keep = objp->get_obj_type() == OT_ITEM && ((item*)objp)->item_flags.preplaced;
                break;
            case MULTI_OBJECT_TYPE_GEN_MOVER:
                keep = objp->get_obj_type() == OT_MOVER && objp->get_sub_type() == OT_SUB_MOVER_GENERAL &&
                       (((general_mover*)objp)->mover_flags & 0x4) == 0;
                break;
            case MULTI_OBJECT_TYPE_RFG_MOVER:
                keep = objp->get_obj_type() == OT_MOVER && objp->get_sub_type() == OT_SUB_MOVER_RFG;
                break;
            default:
                // NPCs, turrets and walkers have no remote objects in multiplayer.
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                break;
            }
            if (keep)
            {
                objp->remote_obj_flags.remote_object = 0;
                continue;
            }
            objp->remote_obj_flags.client_destroy_permission = 1;
            World.destroy_object(objp, false);
        }
    }
    multi_game_migration_cull_projectiles();
}

// 0x009C69F0 (:315): the new server's players keep their network handles (each player's handle is looked up and set
// to the found remote object's own, the same value); the client state becomes in game.
static void reset_as_server_pre()
{
    const int count = World.get_num_objects<player>();
    for (int i = 0; i < count; i++)
    {
        player* pp = World.get_object<player>(i);
        if (pp->obj_flags.destroyed)
        {
            continue;
        }
        const multi_game_data* data = multi_game_data_get(pp->mp_data, false, false);
        if (data == nullptr)
        {
            continue;
        }
        remote_object* rop = Network_handle_to_rop_hash.get(data->player_network_handle);
        if (rop != nullptr && pp->mp_data != 0xFFFFFFFF)
        {
            game_data_of(pp->mp_data)->player_network_handle = rop->m_network_handle;
        }
    }
    Client_state = CLIENT_STATE_IN_GAME;
    console_do_command("list_connections");
}

// 0x009DF0D0 (:347): the new server sets up every connected client's objects and tells everyone about it.
static void process_client_ready_to_go_on_server()
{
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    multi_connection* host = sp->get_host_connection();
    if (host == nullptr)
    {
        return;
    }
    multi_connection* cp = sp->get_connection_list();
    do
    {
        if (cp == nullptr)
        {
            return;
        }
        if (cp != host && (cp->connection_status == CS_CONNECTED || cp->connection_status == CS_CONNECTED_GAMEPLAY))
        {
            multi_core_object_connection_setup(cp);
            multi_join_send_updated_connection_packet_to_all(cp);
        }
        cp = cp->next;
    } while (cp != sp->get_connection_list());
}

// 0x009C6B30 (:385): PT_MIGRATE_LAST_DF, the client's last applied destruction frame (0xFFFF for none). LTCG: cp in
// ESI. Returns multi_io_send_reliable's result (0xFFFFFFFF when the send failed).
static unsigned int multi_migrate_send_last_df(multi_connection* cp)
{
    unsigned char packet[1200];
    packet[0] = PT_MIGRATE_LAST_DF;
    unsigned short packet_size = packet_header_size(PT_MIGRATE_LAST_DF);
    write_u16_be(packet, packet_size, multi_game_destruction_get_last_applied_destruction_frame()); // (inlined)
    packet_set_size(packet, packet_size);
    return multi_io_send_reliable(cp, packet, packet_size, nullptr, nullptr, nullptr);
}

// 0x00956F40 (:399): no caller in rfg.exe.
static void migration_init()
{
    Migration_state = MIGRATION_STATE_NONE;
}

// 0x00956F50 (:405)
static void migration_cannot_continue_return_to_main_menu()
{
    multi_player_menu_system_close_message_box();
    game_boot_to_main_menu(BC_GAME_MIGRATION_FAIL);
}

// 0x009DF130 (:538): the new server's destroyed movers list to every client.
static void multi_game_object_mover_send_destroyed_movers_list_to_all()
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
            multi_game_object_mover_send_destroyed_movers_list(cp);
        }
        cp = cp->next;
    } while (cp != sp->get_connection_list());
}

// 0x009C6BD0 (:554): PT_MIGRATE_REQ_STATUS (no payload) to every client.
static void process_migration_server_req_destruction_status()
{
    unsigned char packet[1200];
    packet[0] = PT_MIGRATE_REQ_STATUS;
    const unsigned short packet_size = packet_header_size(PT_MIGRATE_REQ_STATUS);
    packet_set_size(packet, packet_size);
    multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, packet_size);
    Migration_state = MIGRATION_SERVER_WAIT_DESTRUCTION_STATUS;
}

// 0x009C6C30 (:567)
static void process_migration_server_wait_for_destruction_status()
{
    if (have_all_clients_sent_destruction_status())
    {
        request_destruction_updates_from_clients();
        Migration_state = MIGRATION_SERVER_WAIT_FOR_DESTRUCTION_UPDATE;
    }
}

// 0x009DF180 (:578): with every client's frames in, the frames after the target are dropped and each client gets
// the ones it misses, with the destroyed movers list.
static void process_migration_server_wait_for_destruction_update()
{
    if (!have_all_clients_sent_destruction_updates())
    {
        return;
    }
    const unsigned short last_destruction_frame = get_target_destruction_frame();
    multi_game_destruction_cull_destruction_frames_starting(get_destruction_next_frame_id(last_destruction_frame)); // (inlined)
    multi_game_migration_send_client_destruction_updates(last_destruction_frame);
    multi_game_object_mover_send_destroyed_movers_list_to_all();
    Migration_state = MIGRATION_SERVER_APPLY_DESTRUCTION;
}

// 0x00999B00 (:592): plays the frames back; when all are applied here and on every client, the next frame number
// continues after them.
static void process_migration_server_apply_destruction()
{
    process_migration_destruction(); // (inlined in rfg.exe)
    if (has_all_destruction_been_applied() && have_all_client_applied_destruction()) // (the first inlined)
    {
        rfg_dyn_reset_destruction_frame(get_destruction_next_frame_id(multi_game_destruction_get_last_applied_destruction_frame()));
        Migration_state = MIGRATION_SERVER_DESTROY_OBJECTS;
    }
}

// 0x009DF1D0 (:610)
static void process_migration_server_destroy_objects()
{
    multi_game_migration_cull_projectiles();
    multi_game_migration_reset_projectile_nonce();
    multi_game_migration_server_cull_general_movers();
    Migration_state = MIGRATION_SERVER_RESET_REMOTE_OBJECT_SYSTEM;
}

// 0x009DF1F0 (:625): the new server rebuilds the delta compression state of its remote objects and sets the clients
// up again.
static void process_migration_server_reset_remote_object_system()
{
    for (int i = 0; i < MULTI_OBJECT_TYPE_MAX; i++)
    {
        if (Object_type_registration_list[i] != nullptr)
        {
            Object_type_registration_list[i]->free_mp_state_data();
        }
    }
    multi_core_dc_reset();
    remote_object* rop = Remote_objects_list;
    while (rop != nullptr)
    {
        multi_core_dc_create_master_object(rop->m_network_handle);
        rop = rop->next;
        if (rop == Remote_objects_list)
        {
            break;
        }
    }
    multi_core_set_next_network_handle();
    process_client_ready_to_go_on_server();
    multi_game_sim_event_reset();
    Server_migration_timeout.set(Server_wait_for_create_process_ms);
    Migration_state = MIGRATION_SERVER_WAIT_FOR_CREATE_PROCESS;
}

// 0x009F11F0 (:656): after the wait the mode updates the clients, the game and the match timer go on and the clients
// get PT_MIGRATION_COMPLETE.
static void process_migration_server_wait_for_create_process()
{
    if (!Server_migration_timeout.elapsed())
    {
        return;
    }
    multi_gameplay_mode_server_migration_update_clients(); // (inlined in rfg.exe)
    if (game_is_paused())
    {
        game_unpause();
    }
    multi_game_timer_unpause(); // (inlined in rfg.exe)
    Migration_state = MIGRATION_STATE_NONE;
    unsigned char packet[1200];
    packet[0] = PT_MIGRATION_COMPLETE;
    const unsigned short packet_size = packet_header_size(PT_MIGRATION_COMPLETE);
    packet_set_size(packet, packet_size);
    multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, packet_size);
    multi_core_qos_game_started();
    Client_standby_hack_check.set(Client_standby_hack_check_ms);
    multi_player_menu_system_close_message_box();
}

// 0x009F12F0 (:696)
static void server_migration_do_frame()
{
    switch (Migration_state)
    {
    case MIGRATION_SERVER_REQ_DESTRUCTION_STATUS:
        process_migration_server_req_destruction_status();
        break;
    case MIGRATION_SERVER_WAIT_DESTRUCTION_STATUS:
        process_migration_server_wait_for_destruction_status();
        break;
    case MIGRATION_SERVER_WAIT_FOR_DESTRUCTION_UPDATE:
        process_migration_server_wait_for_destruction_update();
        break;
    case MIGRATION_SERVER_APPLY_DESTRUCTION:
        process_migration_server_apply_destruction();
        break;
    case MIGRATION_SERVER_DESTROY_OBJECTS:
        process_migration_server_destroy_objects();
        break;
    case MIGRATION_SERVER_RESET_REMOTE_OBJECT_SYSTEM:
        process_migration_server_reset_remote_object_system();
        break;
    case MIGRATION_SERVER_WAIT_FOR_CREATE_PROCESS:
        process_migration_server_wait_for_create_process();
        break;
    default:
        break;
    }
}

// 0x00956FF0 (:738)
static void process_migration_client_wait_for_destruction_update()
{
    if (Client_migration_timout.elapsed())
    {
        migration_cannot_continue_return_to_main_menu(); // (inlined)
    }
}

// 0x009DF270 (:750): plays the frames back; when all are applied, the client says so, destroys its remote copies and
// waits.
static void process_migration_client_wait_for_destruction_applied()
{
    process_migration_destruction(); // (inlined in rfg.exe)
    if (has_all_destruction_been_applied()) // (inlined)
    {
        multi_game_migration_destroy_general_mover_destroyed_list();
        multi_client_send_destruction_applied();
        destroy_client_remote_objects();
        Client_migration_timout.set(Client_wait_for_create_process_ms);
        Migration_state = MIGRATION_CLIENT_WAIT_FOR_DESTRUCTION_APPLIED_POST;
    }
}

// 0x009F8370 (:776)
static void process_migration_client_wait_for_destruction_applied_post()
{
    multi_client_reset_remote_object_system(); // (inlined in rfg.exe)
    multi_game_sim_event_reset();
    Migration_state = MIGRATION_CLIENT_WAIT_FOR_CREATE_PROCESS;
}

// 0x009C6C50 (:791): until PT_MIGRATION_COMPLETE the client acknowledges the new server's updates.
static void process_migration_client_migration_wait_for_create_process()
{
    if (Client_migration_timout.elapsed())
    {
        migration_cannot_continue_return_to_main_menu(); // (inlined)
    }
    multi_core_dc_send_acks();
}

// 0x009F83C0 (:805): first the pending answer to PT_MIGRATE_REQ_STATUS, then the client's step.
static void client_migration_do_frame()
{
    if (s_retryClientSendLastDestructionFrame)
    {
        const multi_session_state state = Lobby_session->get_state();
        if (state != MSS_MIGRATING && state != MSS_NONE && state != MSS_DELETING)
        {
            multi_connection* host = Lobby_session->get_host_connection();
            if (host != nullptr)
            {
                sender from(host);
                multi_game_process_migrate_req_status(nullptr, from);
            }
        }
        // (the state read before the retry: the flag is only cleared without a lobby session state)
        if (state == MSS_NONE)
        {
            s_retryClientSendLastDestructionFrame = false;
        }
    }
    switch (Migration_state)
    {
    case MIGRATION_CLIENT_WAIT_FOR_DESTRUCTION_UPDATE:
        process_migration_client_wait_for_destruction_update(); // (inlined)
        break;
    case MIGRATION_CLIENT_WAIT_FOR_DESTRUCTION_APPLIED:
        process_migration_client_wait_for_destruction_applied();
        break;
    case MIGRATION_CLIENT_WAIT_FOR_DESTRUCTION_APPLIED_POST:
        process_migration_client_wait_for_destruction_applied_post();
        break;
    case MIGRATION_CLIENT_WAIT_FOR_CREATE_PROCESS:
        process_migration_client_migration_wait_for_create_process(); // (inlined)
        break;
    default:
        break;
    }
}

// ---- external interface

// 0x00956F60 (:421)
void multi_game_migration_set_allow_migration_timer()
{
    Mgm_allow_migration.set(Migration_allow_ms);
}

// 0x00956F70 (:426)
bool multi_game_migration_allow_migration_timer_elapsed()
{
    return Mgm_allow_migration.elapsed();
}

// 0x00956F80 (:431)
void multi_game_clear_has_migrated()
{
    Multi_game_has_migrated = false;
    Multi_game_has_skipped_game_migration = false;
}

// 0x00956F90 (:437)
bool multi_game_has_migrated()
{
    return Multi_game_has_migrated;
}

// 0x00956FA0 (:442)
bool multi_game_has_skipped_game_migration()
{
    return Multi_game_has_skipped_game_migration;
}

// 0x00956FB0 (:447)
void multi_game_migration_level_shutdown()
{
    Migration_state = MIGRATION_STATE_NONE;
}

// 0x00956FC0 (:452)
void multi_game_migration_set_state(migration_state state)
{
    Migration_state = state;
}

// 0x00956FD0 (:461): the game pauses behind the migration message box.
void multi_game_migration_prepare_for_migration(multi_session_base*)
{
    game_pause(true);
    multi_player_menu_system_pull_up_migrate_message_box();
}

// 0x00956FE0 (:469)
void multi_game_migration_set_skipped_game_migration()
{
    Multi_game_has_skipped_game_migration = true;
}

// 0x009F1140 (:476): the session layer chose the new host. A second migration while one runs ends the game.
void multi_game_migration_start_migration(multi_session_base* sp)
{
    if (Migration_state != MIGRATION_STATE_NONE)
    {
        migration_cannot_continue_return_to_main_menu(); // (inlined)
        return;
    }
    Debug_time.set(0);
    multi_game_timer_pause(); // (inlined in rfg.exe)
    const bool host = sp->am_i_host(); // (inlined)
    Multi_game_has_migrated = true;
    multi_gameplay_mode_server_migration_pre(); // (inlined)
    multi_game_sim_event_reset();
    if (host)
    {
        Migration_state = MIGRATION_SERVER_REQ_DESTRUCTION_STATUS;
        reset_as_server_pre();
    }
    else
    {
        Migration_state = MIGRATION_CLIENT_WAIT_FOR_DESTRUCTION_UPDATE;
        Client_migration_timout.set(Client_wait_for_destruction_update_ms);
    }
    multi_gameplay_mode_server_migration_post(); // (inlined)
}

// 0x009F8490 (:853)
void multi_game_migration_do_frame()
{
    if (Multiplayer && Server)
    {
        server_migration_do_frame();
        return;
    }
    client_migration_do_frame();
}

// 0x00957010 (:862)
bool multi_game_is_migrating()
{
    return Migration_state != MIGRATION_STATE_NONE;
}

// 0x00999BA0 (:867): PT_MIGRATION_COMPLETE on a client: the game and the match timer go on.
void multi_game_migration_complete(unsigned char* packet_data, const sender& from)
{
    if (Migration_state == MIGRATION_STATE_FAILED || Migration_state != MIGRATION_CLIENT_WAIT_FOR_CREATE_PROCESS)
    {
        return;
    }
    if (game_is_paused())
    {
        game_unpause();
    }
    multi_game_timer_unpause();
    Migration_state = MIGRATION_STATE_NONE;
    multi_player_menu_system_close_message_box();
}

// 0x00957020 (:895): PT_MIGRATE_LAST_DF on the new server: the client's last applied destruction frame. A client 16
// frames behind is dropped; this server 16 frames behind the client gives up.
void multi_game_process_migrate_last_df(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    const unsigned short last_applied_frame = read_u16_be(packet_data, offset);
    // Original: the connection is not checked for null here (only before the disconnect below).
    destruction_migration_data& dm_data = game_data_of(from.connection->game_data)->dm_data;
    dm_data.last_destruction_frame_recvd = true;
    dm_data.last_applied_frame = last_applied_frame;
    const unsigned short client_next = get_destruction_next_frame_id(last_applied_frame); // (inlined)
    const unsigned short server_next = get_destruction_next_frame_id(multi_game_destruction_get_last_applied_destruction_frame()); // (inlined)
    if (client_next < server_next)
    {
        if (server_next - client_next >= Migration_max_destruction_lag)
        {
            multi_connection_disconnect(from.connection, CDR_GAME_MIGRATE_DESTRUCTION_LAG); // (inlined)
        }
        return;
    }
    if (server_next < client_next && client_next - server_next >= Migration_max_destruction_lag)
    {
        migration_cannot_continue_return_to_main_menu(); // (inlined)
    }
}

// 0x009DF2E0 (:930): PT_MIGRATE_REQ_STATUS on a client: the last applied destruction frame and the destroyed movers
// list go to the new server, or are retried while the lobby migrates or a send fails.
void multi_game_process_migrate_req_status(unsigned char* packet_data, const sender& from)
{
    if (Lobby_session->get_state() == MSS_MIGRATING)
    {
        s_retryClientSendLastDestructionFrame = true;
        return;
    }
    if (multi_migrate_send_last_df(from.connection) == 0xFFFFFFFF)
    {
        s_retryClientSendLastDestructionFrame = true;
        return;
    }
    s_retryClientSendLastDestructionFrame = multi_game_object_mover_send_destroyed_movers_list(from.connection) == 0xFFFFFFFF;
}

// 0x00957110 (:968)
bool multi_game_is_migration_state(migration_state _state)
{
    return Migration_state == _state;
}

// 0x00957120 (:973)
void multi_game_migration_init()
{
    Multi_game_has_migrated = false;
    Multi_game_has_skipped_game_migration = false;
    Migration_state = MIGRATION_STATE_NONE;
}
