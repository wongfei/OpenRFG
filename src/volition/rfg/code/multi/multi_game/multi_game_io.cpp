// Original: volition/rfg/code/multi/multi_game/multi_game_io.cpp
//
// The match start between server and clients: the server sends join-game-start (the server options, the desired
// weapons, the skill mismatch and time-of-day flags) to each client, which then loads the level and answers with
// client-loaded; the server queues the loaded clients until it is in gameplay itself and then creates each client's
// player at a spawn point and starts replicating to it. The client and server process callbacks of multi.cpp run
// these steps through Client_state.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_io.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_character.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_destruction.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_player.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_join.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/event_logging/definitions_events/events.h"
#include "volition/rfg/code/gamewide/gamewide.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/vlib/code/os/vconsole.h"
#include "keen/base/trace.hpp"

// ---- file-static globals
// multi_gameplay_mode Multi_gameplay_mode (0x0167DEE0) is used here too; placed in multi_gameplay_mode.cpp.
// human_voice_persona_handle Multi_general_announcer (0x0167DEE4) is used here too; placed in multi_game_mode_bagman.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// bool Skill_mismatch (0x02FEB63A) is used here too; placed in multi_game_mode_common.cpp.
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.
static void (*Client_connection_dropped_callback)(); // 0x02FEB6AC
// object_pool<loaded_client_reference,16,1> Multi_game_join_loaded_clients (0x02FED940) is used here too; placed in multi_game_mode_demolition.cpp.

// Magic number: packets are built in a 1,200 byte buffer.
// Packet_buffer_size (1200) is defined in multi_game.cpp, earlier in the unity TU.

// ---- internal functions

// 0x00955EB0 (:71): the debug output is compiled out
static void debug_spew_game_io(const char*, ...)
{
}

// Port helper: the updated connection goes to every other connection in gameplay of the game session (rfg.exe inlines
// this walk twice).
static void send_updated_connection_to_gameplay_connections(multi_connection* cp)
{
    multi_session* session = multi_session_get_by_type(ST_GAME);
    multi_connection* first = session->get_connection_list();
    for (multi_connection* other = first; other != nullptr;)
    {
        if (other != cp && other->connection_status == CS_CONNECTED_GAMEPLAY)
        {
            multi_join_send_updated_connection_packet(other, cp);
        }
        other = other->next;
        if (other == first)
        {
            break;
        }
    }
}

// 0x009DEA70 (:244): a spectator (marauder team) only gets replication; a player gets a player object at a spawn
// point (with its game data handle and team) and the mode adds it. A client joining in progress starts
// invulnerable.
static void multi_game_join_initialize_player(multi_connection* cp)
{
    multi_connection* me = Game_session->get_me_connection();
    if (me->connection_status == CS_CONNECTED)
    {
        me->connection_status = CS_CONNECTED_GAMEPLAY;
    }
    const multi_game_data* data = multi_game_data_get(cp->game_data, false, false);
    if (data == nullptr)
    {
        return;
    }
    if (data->team == HUMAN_TEAM_MARAUDER)
    {
        if (cp->connection_status == CS_CONNECTED)
        {
            cp->connection_status = CS_CONNECTED_GAMEPLAY;
        }
        multi_core_object_connection_setup(cp);
        send_updated_connection_to_gameplay_connections(cp);
        multi_join_send_updated_connection_packet_to_all(cp);
        return;
    }
    int spawn_index = -1;
    vector pos;
    matrix orient;
    multi_game_get_player_starting_position(spawn_index, pos, orient);
    const human_teams team = multi_gameplay_mode_is_team_mode(multi_gameplay_mode_get()) ? data->team : HUMAN_TEAM_CIVILIAN;
    const human_info* h_info = multi_game_charcter_get_human_info(team, data->player_selection_preference);
    obj_human_props props(h_info, pos, orient, nullptr);
    props.set<unsigned int>(MULTI_PLAYER_HANDLE, cp->game_data);
    props.set<int>(MULTI_TEAM, (int)data->team);
    player* pp = World.create_object<player>(props, 0xFFFFFFFF);
    // As in rfg.exe: spawn point 0 is not remembered.
    if (spawn_index > 0 && World.get_object<player_start>(spawn_index) != nullptr)
    {
        pp->mp_last_spawn_point_handle = World.get_object<player_start>(spawn_index)->get_handle();
    }
    human_set_state(pp, HS_STANDARD, HMS_IDLE, -1.0f);
    pp->update_pos_and_orient(pos, orient, true);
    human_set_state(pp, HS_STANDARD, HMS_IDLE, -1.0f);
    player_set_zoom_state(pp, PZS_OFF, false);
    const unsigned short net_handle = multi_core_network_handle_from_game_handle(pp->get_handle(), false);
    if (cp->game_data != 0xFFFFFFFF)
    {
        game_data_of(cp->game_data)->player_network_handle = net_handle;
    }
    if (cp->connection_status == CS_CONNECTED)
    {
        cp->connection_status = CS_CONNECTED_GAMEPLAY;
    }
    multi_core_object_connection_setup(cp);
    send_updated_connection_to_gameplay_connections(cp);
    multi_gameplay_mode_add_player(pp);
    if (cp->joining_in_progress)
    {
        player_set_player_invulnerable(pp, true);
    }
    multi_join_send_updated_connection_packet_to_all(cp);
    log_event_player_create(gamelog_get_player_checksum(), gamelog_get_logging_time(), gamelog_get_player_index(pp),
        gamelog_get_locale(), gamelog_get_game_duration(), pp->pos);
    pp->backpack_equip_log_handle = log_start_event_backpack_equipped(gamelog_get_player_checksum(), gamelog_get_logging_time(),
        gamelog_get_player_index(pp), gamelog_get_backpack_crc(pp->backpack), gamelog_get_player_xp_level(pp));
}

// 0x009DEDD0 (:346): every queued client gets its player, then the queue empties
static void multi_game_join_process_queued_loaded_clients()
{
    Multi_game_join_loaded_clients.thread_access.enter();
    for (loaded_client_reference* ref = Multi_game_join_loaded_clients.get_first(0); ref != nullptr;
         ref = Multi_game_join_loaded_clients.get_next(ref))
    {
        multi_game_join_initialize_player(ref->cp);
    }
    Multi_game_join_loaded_clients.thread_access.leave();
    Multi_game_join_loaded_clients.thread_access.enter();
    for (loaded_client_reference* ref = Multi_game_join_loaded_clients.get_first(0); ref != nullptr;
         ref = Multi_game_join_loaded_clients.free(ref))
    {
    }
    Multi_game_join_loaded_clients.thread_access.leave();
}

// 0x00999020 (:359)
static void multi_game_join_add_queued_loaded_client(multi_connection* cp)
{
    Multi_game_join_loaded_clients.alloc(0)->cp = cp;
}

// 0x009AEE90 (:368). LTCG: cp in EDI.
static void multi_game_join_remove_queued_loaded_client(const multi_connection* cp)
{
    Multi_game_join_loaded_clients.thread_access.enter();
    for (loaded_client_reference* ref = Multi_game_join_loaded_clients.get_first(0); ref != nullptr;
         ref = Multi_game_join_loaded_clients.get_next(ref))
    {
        if (ref->cp == cp)
        {
            Multi_game_join_loaded_clients.free(ref);
            break;
        }
    }
    Multi_game_join_loaded_clients.thread_access.leave();
}

// ---- external functions

// 0x009DEE90 (:386): in gameplay the client gets its player at once (a client joining in progress then gets the
// destruction state); before that it is queued
void multi_game_join_process_client_loaded_game(unsigned char* packet_data, const sender& from)
{
    multi_connection* cp = from.connection;
    if (cp == nullptr)
    {
        return;
    }
    if (gameseq_state_is_active(GS_GAMEPLAY))
    {
        multi_game_join_initialize_player(cp);
        if (cp->joining_in_progress)
        {
            multi_game_destruction_start_server_sync(cp);
        }
    }
    else
    {
        multi_game_join_add_queued_loaded_client(cp);
    }
}

// 0x009AEF10 (:415)
void multi_game_join_remove_client(const multi_connection* cp)
{
    multi_game_join_remove_queued_loaded_client(cp);
}

// 0x009DEEE0 (:422): the server's load states (multi_server_process): the queued clients get their players once the
// server is in gameplay
void multi_game_call_server_process_callback()
{
    if (Client_state == CLIENT_STATE_ESTABLISHED)
    {
        Client_state = CLIENT_STATE_LOADING_GAME;
    }
    else if (Client_state == CLIENT_STATE_LOADING_GAME && gameseq_state_is_active(GS_GAMEPLAY))
    {
        Client_state = CLIENT_STATE_IN_GAME;
        multi_game_join_process_queued_loaded_clients();
    }
}

// 0x009FD3F0 (:451): the client's load states (multi_client_process): join-game-start starts the level load, and
// gameplay reports it loaded
void multi_game_call_client_process_callback()
{
    if (Client_state == CLIENT_STATE_PENDING_GAME_LOAD)
    {
        multi_game_lobby_client_start_game();
        Client_state = CLIENT_STATE_LOADING_GAME;
    }
    else if (Client_state == CLIENT_STATE_LOADING_GAME && gameseq_state_is_active(GS_GAMEPLAY))
    {
        Client_state = CLIENT_STATE_IN_GAME;
        multi_game_send_client_loaded_level();
    }
}

// 0x00955ED0 (:485)
void multi_game_call_client_connection_established_callback()
{
    Client_state = CLIENT_STATE_ESTABLISHED;
}

// 0x009C5D90 (:495): the server options, the desired weapons of the session's players, the skill mismatch and the
// default time of day flags (the level parameter is not sent: the server options name the level)
void multi_game_send_join_game_start(multi_connection* conn, const char* level)
{
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_JOIN_GAME_START;
    unsigned short size = packet_header_size(PT_JOIN_GAME_START);
    multi_game_append_server_options_to_packet(packet, size, &Mp_server_options);
    multi_game_player_append_desired_weapons_to_packet(packet, size, conn->owner);
    packet[size] = Skill_mismatch;
    packet[(unsigned short)(size + 1)] = Multi_match_info.use_default_time_of_day;
    size = (unsigned short)(size + 2);
    packet_set_size(packet, size);
    multi_io_send_reliable(conn, packet, size, nullptr, nullptr, nullptr);
}

// 0x009C5E60 (:520): a client outside a level load takes the server options and waits to load (an unknown level
// boots to the main menu)
void multi_game_process_join_game_start(unsigned char* packet_data, const sender& from)
{
    console_output("GOT GAME START\n", nullptr, false);
    if (!Multiplayer || Server || gameseq_get_state() == GS_LOAD)
    {
        return;
    }
    int offset = 0;
    multi_game_read_server_options_from_packet(&Mp_server_options, packet_data, offset);
    if (Mp_server_options.get_level() == nullptr)
    {
        keen::debug::nativePrintArgs("Cannot load level indicated by server. Booting to main menu.");
        game_boot_to_main_menu(BC_GAME_MIGRATION_FAIL);
    }
    multi_game_player_read_desired_weapons_from_packet(packet_data, offset);
    Client_state = CLIENT_STATE_PENDING_GAME_LOAD;
    Skill_mismatch = packet_data[offset] != 0;
    Multi_match_info.use_default_time_of_day = packet_data[offset + 1] != 0;
    gamelog_remote_reinit();
}

// 0x00955EE0 (:563)
void multi_game_register_client_connection_dropped_callback(void (*callback)())
{
    Client_connection_dropped_callback = callback;
}

// 0x00955EF0 (:570)
void multi_game_call_client_connection_dropped_callback()
{
    if (Client_connection_dropped_callback != nullptr)
    {
        Client_connection_dropped_callback();
    }
}

// 0x00955F00 (:579)
void multi_game_call_client_become_server_callback_pre()
{
}

// 0x00955F10 (:586)
void multi_game_call_client_become_server_callback_post()
{
    Client_state = CLIENT_STATE_IN_GAME;
}
