// Original: volition/rfg/code/multi/multi_game/multi_game_callbacks.cpp
//
// The game side's answers to the multiplayer core: connections denied, dropped, about to be removed and removed (the
// player and its game data go), name changes, host migration, the delta compression master object index of game
// objects, and the multiplayer memory (the preload pools and the ragdolls).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_callbacks.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_io.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "keen/base/trace.hpp"

#include <string.h>
#include <wchar.h>

// ---- file-static globals
// multi_gameplay_mode Multi_gameplay_mode (0x0167DEE0) is used here too; placed in multi_gameplay_mode.cpp.
// unsigned int Local_game_data_handles[4] (0x0167E5F4) is used here too; placed in multi_join.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// unsigned char (*Get_dcmo_index_from_game_handle_cb)(unsigned int) (0x02FEB5D4) is used here too; placed in multi.cpp.
// void (*Set_dcmo_index_for_game_handle_cb)(unsigned int, unsigned char) (0x02FEB5D8) is used here too; placed in multi.cpp.
// mempool_base* Mp_interface_slot_allocator_mempool (0x02FEB660) is used here too; placed in multi_game.cpp.
static unsigned char* MP_effect_preload_buffer; // 0x02FEB670
static unsigned char* MP_effect_preload_vram_buffer; // 0x02FEB674
static unsigned char* MP_item_preload_buffer; // 0x02FEB678
static unsigned char* MP_item_preload_vram_buffer; // 0x02FEB67C
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.

// Magic numbers: the multiplayer preload pools (effects 5.5 MB, items about 77 MB), the ragdolls multiplayer keeps
// (8), a name of 64 characters, the packet buffer of 1,200 bytes.
static const unsigned int Mp_effect_preload_size = 0x580000;
static const unsigned int Mp_item_preload_size = 0x4CFEC00;
static const int Mp_num_ragdolls = 8;
static const int Max_player_name = 64;
// Packet_buffer_size (1200) is defined in multi_game.cpp, earlier in the unity TU.

// ---- internal functions

// 0x0094F3E0 (:40)
static unsigned char multi_get_dcmo_index_from_game_handle(unsigned int game_handle)
{
    object* objp = World.object_from_handle(game_handle);
    if (objp != nullptr)
    {
        return objp->mp_dcmo_index;
    }
    return 0xFF;
}

// 0x0094F400 (:51)
static void multi_set_dcmo_index_for_game_handle(unsigned int game_handle, unsigned char index)
{
    object* objp = World.object_from_handle(game_handle);
    if (objp != nullptr)
    {
        objp->mp_dcmo_index = index;
    }
}

// ---- external functions

// 0x0094F420 (:66)
void multi_delta_compression_register_game_dcmo_callbacks()
{
    Get_dcmo_index_from_game_handle_cb = multi_get_dcmo_index_from_game_handle;
    Set_dcmo_index_for_game_handle_cb = multi_set_dcmo_index_for_game_handle;
}

// 0x0094F440 (:76)
void multi_game_connection_denied(multi_join_deny_reasons reason)
{
    Client_state = CLIENT_STATE_DISCONNECTED;
}

// 0x0094F450 (:85)
void multi_game_connection_dropped()
{
    Client_state = CLIENT_STATE_DISCONNECTED;
}

// 0x00996510 (:94): the leaving player's snapshot slot remembers whether it timed out, and is marked as reported when
// its stats count (a stats session past registration)
void multi_game_about_to_remove_connection(multi_connection* conn)
{
    if (conn == nullptr)
    {
        keen::debug::nativePrintArgs("multi_game_about_to_remove_connection: conn == nullptr\n");
        return;
    }
    multi_game_data* data = (multi_game_data*)multi_game_data_begin(ST_GAME_SNAPSHOT, false, false);
    while (data != nullptr && !data->player_uid.get_internal_id().is_equal(conn->player_uid.get_internal_id()))
    {
        data = (multi_game_data*)multi_game_data_next(data, false, false);
    }
    if (data == nullptr)
    {
        return;
    }
    switch (conn->disconnect_reason)
    {
    case CDR_RELIABLE_BUFFER_FULL:
    case CDR_RELIABLE_CALLBACK_BUFFER_FULL:
    case CDR_NETSEND_FAILED:
    case CDR_TEST_ADDRESS_FAILED:
    case CDR_HEARTBEAT_TIMEOUT:
    case CDR_RELIABLE_ACK_TIMEOUT:
    case CDR_PENDING_STATE_TIMEOUT:
        data->timed_out = true;
        break;
    default:
        data->timed_out = false;
        break;
    }
    const multi_session_state state = conn->owner->get_state();
    if (conn->owner->get_info()->reports_stats && state > MSS_REGISTERING && (state < MSS_ENDING || state == MSS_MIGRATING) &&
        data->team != HUMAN_TEAM_MARAUDER && data->handle != 0xFFFFFFFF)
    {
        game_data_of(data->handle)->reported_stats = true;
    }
}

// 0x009DCFA0 (:164): the player object and the game data slot go
void multi_game_removed_connection(multi_connection* conn)
{
    multi_game_join_remove_queued_loaded_client(conn);
    if (conn->game_data == 0xFFFFFFFF)
    {
        return;
    }
    player* pp = multi_game_data_get_player_instance(conn->game_data);
    if (pp != nullptr)
    {
        multi_game_remove_player(pp);
    }
    multi_game_data_remove_player(conn->game_data);
    conn->game_data = 0xFFFFFFFF;
}

// 0x009BC740 (:192): a client hides and detaches the projectiles stuck to the player; the local player stays
// for the results of the last round
void multi_game_remove_player(player* pp)
{
    if (pp == nullptr || pp->obj_flags.destroyed)
    {
        return;
    }
    pp->pflags.mp_removing_player = true;
    multi_gameplay_mode_remove_player(pp);
    vehicle_interaction_remove(pp);
    if (Multiplayer && !Server && pp->has_child())
    {
        object* first = pp->child_ptr;
        object* child = first;
        while (child != nullptr)
        {
            object* next = child->child_next != pp->child_ptr ? child->child_next : nullptr;
            if (child->get_obj_type() == OT_ITEM && child->get_sub_type() == OT_SUB_ITEM_PROJECTILE)
            {
                child->set_is_visible(false);
                pp->remove_child(child);
            }
            child = next;
        }
    }
    if (pp != player_get_local_player() || Multi_match_info.match_state != MGMS_VIEWING_RESULTS || !multi_game_mode_is_last_round())
    {
        player_level_shutdown(pp);
        pp->mp_data = 0xFFFFFFFF;
        World.destroy_object(pp, false);
    }
}

// 0x009BC830 (:232): our new name goes into our slots and to the other peers of each session
void multi_game_local_player_name_changed(const wchar_t* new_name)
{
    for (int type = ST_PARTY; type < NUM_SESSION_TYPES; type++)
    {
        multi_session* session = multi_session_get_by_type((session_types)type);
        if (session == nullptr || session->get_state() == MSS_NONE)
        {
            continue;
        }
        const unsigned int game_data_handle = Local_game_data_handles[type];
        multi_game_data_set_player_name(game_data_handle, new_name);
        if (type == ST_GAME && Local_game_data_handles[ST_GAME_SNAPSHOT] != 0xFFFFFFFF)
        {
            multi_game_data_set_player_name(Local_game_data_handles[ST_GAME_SNAPSHOT], new_name);
        }
        unsigned char packet[Packet_buffer_size];
        packet[0] = PT_PLAYER_NAME_CHANGED;
        unsigned short size = packet_header_size(PT_PLAYER_NAME_CHANGED);
        write_u32_be(packet, size, game_data_handle);
        const unsigned short name_bytes = (unsigned short)(wcslen(new_name) * 2 + 2);
        memcpy(packet + size, new_name, name_bytes);
        size = (unsigned short)(size + name_bytes);
        packet_set_size(packet, size);
        if (session->am_i_host())
        {
            multi_io_send_to_all_reliable(session, packet, size);
        }
        else
        {
            multi_io_send_reliable(session->get_host_connection(), packet, size, nullptr, nullptr, nullptr);
        }
    }
}

// 0x009BC9B0 (:268): a peer's new name (the host passes it on); the snapshot slot of a game session player follows
void multi_game_process_player_name_changed(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    const unsigned int game_data_handle = read_u32_be(packet_data, offset);
    const wchar_t* received = (const wchar_t*)(packet_data + offset);
    wchar_t name[Max_player_name];
    if (wcslen(received) < Max_player_name)
    {
        wcscpy(name, received);
    }
    else
    {
        wcsncpy(name, received, Max_player_name);
        name[Max_player_name - 1] = L'\0';
    }
    if (from.connection == nullptr || from.connection->owner == nullptr)
    {
        return;
    }
    if (game_data_handle != 0xFFFFFFFF && Local_game_data_handles[game_data_handle >> 16] == game_data_handle)
    {
        return;
    }
    multi_game_data_set_player_name(game_data_handle, name);
    if (from.connection->owner->get_info()->type == ST_GAME)
    {
        const multi_game_data* data = multi_game_data_get(game_data_handle, false, false);
        if (data != nullptr)
        {
            const multi_game_data* snapshot = multi_game_data_get_by_player_uid(ST_GAME_SNAPSHOT, data->player_uid);
            if (snapshot != nullptr)
            {
                multi_game_data_set_player_name(snapshot->handle, name);
            }
        }
    }
    if (from.connection->owner->am_i_host())
    {
        unsigned char packet[Packet_buffer_size];
        packet[0] = PT_PLAYER_NAME_CHANGED;
        unsigned short size = packet_header_size(PT_PLAYER_NAME_CHANGED);
        write_u32_be(packet, size, game_data_handle);
        const unsigned short name_bytes = (unsigned short)(wcslen(name) * 2 + 2);
        memcpy(packet + size, name, name_bytes);
        size = (unsigned short)(size + name_bytes);
        packet_set_size(packet, size);
        multi_io_send_to_all_except_one_reliable(from.connection->owner, packet, size, from.connection);
    }
}

// 0x0094F460 (:308)
void multi_game_client_become_server_pre()
{
}

// 0x0094F470 (:314)
void multi_game_client_become_server_post()
{
    Client_state = CLIENT_STATE_IN_GAME;
}

// 0x0094F480 (:320): the preload pools come from the level memory (no video memory on PC); the ragdolls from the
// multiplayer allocators
void multi_game_memory_alloc()
{
    MP_effect_preload_vram_buffer = nullptr;
    MP_item_preload_vram_buffer = nullptr;
    MP_effect_preload_buffer = (unsigned char*)Level_memmgr.alloc(Mp_effect_preload_size, "multi_game_memory_alloc()", 16);
    MP_effect_preload_mempool.init_rfg_split_mempool("mp preload", MP_effect_preload_buffer, Mp_effect_preload_size, 4,
        MP_effect_preload_vram_buffer, 0, 16, 8);
    MP_effect_preload_mempool.clear();
    MP_item_preload_buffer = (unsigned char*)Level_memmgr.alloc(Mp_item_preload_size, "multi_game_memory_alloc()", 16);
    MP_item_preload_mempool.init_rfg_split_mempool("mp item preload", MP_item_preload_buffer, Mp_item_preload_size, 4,
        MP_item_preload_vram_buffer, 0, 16, 8);
    MP_item_preload_mempool.clear();
    Stream_mp_effect_preload_allocator.init(&MP_effect_preload_mempool);
    Stream_mp_item_preload_allocator.init(&MP_item_preload_mempool);
    multi_game_interface_memory_init(&MP_allocators_mempool);
    void* ragdoll_memory = MP_allocators_mempool.alloc(havok_ragdoll_get_ragdoll_size() * Mp_num_ragdolls, 0);
    havok_ragdoll_add_ragdoll_memory(Mp_num_ragdolls, ragdoll_memory);
}

// 0x0094F580 (:356)
void multi_game_memory_release()
{
    havok_ragdoll_remove_ragdoll_memory();
    if (Mp_interface_slot_allocator_mempool != nullptr)
    {
        Mp_interface_slot_allocator_mempool = nullptr;
    }
    MP_effect_preload_mempool.clear();
    MP_item_preload_mempool.clear();
    Level_memmgr.free(MP_effect_preload_buffer);
    MP_effect_preload_buffer = nullptr;
    MP_effect_preload_vram_buffer = nullptr;
    Level_memmgr.free(MP_item_preload_buffer);
    MP_item_preload_buffer = nullptr;
    MP_item_preload_vram_buffer = nullptr;
    MP_effect_preload_mempool.init_rfg_split_mempool("mp preload", MP_effect_preload_buffer, 0, 4, MP_effect_preload_vram_buffer, 0, 4, 8);
    MP_item_preload_mempool.init_rfg_split_mempool("mp item preload", MP_item_preload_buffer, 0, 4, MP_item_preload_vram_buffer, 0, 4, 8);
}
