// Original: volition/rfg/code/multi/multi_core/global/multi_core_objects.cpp
//
// Decompiled whole (multiplayer.md MP5). The replicated ("remote") objects: a pool of 255 remote objects, each linked
// in the list of all and in the list of its registered type, found by game handle and by network handle; the server
// creates and destroys them with reliable packets (PT_REMOTE_OBJECT_CREATE/_DESTROY, applied on the client through
// sim events at their network time) and fills each connection's bandwidth budget with delta compressed state updates
// (PT_REMOTE_OBJECT_UPDATE), prioritized by distance and view direction. Each object type's registration
// (multi_core_object_registration) holds its state masks and implements the state handling.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"

#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_delta_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_session_base.h"
#include "volition/rfg/code/multi/multi_core/include/multi_pool.inl"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_event.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/quaternion.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/hash_table.h"

#include <algorithm>
#include <math.h>
#include <stdarg.h>
#include <string.h>

// ---- file-static globals
static bool Allow_destroy_remote_object = true; // 0x0167D82E
// unsigned short Master_ack_id_number (0x0167D838) is used here too; placed in multi_core_delta_compression.cpp.
// unsigned short Cur_network_handle (0x0167DD78) is used here too; placed in multi.cpp.
static multi_pool<object_create_data, 255> Event_create_allocator; // 0x01691AC0
static multi_pool<object_destroy_data, 255> Event_destroy_allocator; // 0x01691B00
// dynamic_uint_hash_table<remote_object*> Game_handle_to_rop_hash (0x01691B40) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<remote_object*> Network_handle_to_rop_hash (0x01691B88) is used here too; placed in multi_core_delta_compression.cpp.
static dynamic_uint_hash_table<unsigned int> Debug_remote_object_hash; // 0x01691BD0
static dynamic_uint_hash_table<network_game_handle*> Reserved_game_to_network_hash; // 0x01691C18
// dynamic_uint_hash_table<network_game_handle*> Reserved_network_to_game_hash (0x01691C60) is used here too; placed in multi_core_delta_compression.cpp.
static base_array<remote_object*> Snapshot_remote_object_list; // 0x01691CA8
// dc_master_object* Master_object_lookup_table[255] (0x02FE6D88) is used here too; placed in multi_core_delta_compression.cpp.
// The first remote object of each registered type (no source file in rfg.pdb; its users are all here).
static remote_object* Remote_objects_by_type[12]; // 0x02FE5BAC
static remote_object* Free_remote_objects_list; // 0x02FE88AC
static object_destroy_data Temp_destroy_event_data; // 0x02FE8900
static object_create_data Temp_create_event_data; // 0x02FE8B08
static unsigned int Remote_object_count; // 0x02FE8DDC
// Every remote object in use (no source file in rfg.pdb; its users are all here).
static remote_object* Remote_objects_list; // 0x02FE8E04
multi_core_object_registration* Object_type_registration_list[12]; // 0x02FEB34C
// unsigned int Clock_current_time (0x02FEB538) is used here too; placed in multi.cpp.
static int Registered_level_load_remote_object_local_create_types[10]; // 0x02FEB53C
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// unsigned char (*Get_dcmo_index_from_game_handle_cb)(unsigned int) (0x02FEB5D4) is used here too; placed in multi.cpp.
// remote_object* Remote_objects_pool (0x02FEB5EC) is used here too; placed in multi.cpp.
// rop_sort_element* Rop_sort_element (0x02FEB5F0) is used here too; placed in multi.cpp.
// rop_sort_element** PTR_rop_sort_element (0x02FEB5F4) is used here too; placed in multi.cpp.
// int Num_reserved_network_handles (0x02FEB5F8) is used here too; placed in multi.cpp.
// network_game_handle* Network_game_handles (0x02FEB5FC) is used here too; placed in multi.cpp.
static int Num_object_updates_sent; // 0x02FEB600
static int Num_registered_level_load_remote_object_local_create_types; // 0x02FEB604
// migration_state Migration_state (0x02FEB6C4) is used here too; placed in multi.cpp.
// bool Multi_game_has_skipped_game_migration (0x02FEB6C9) is used here too; placed in multi_game.cpp.

// ---- internal functions
static void validate_remote_object_allocations(unsigned int rop_game_handle); // 0x00947C80 (:250)
static remote_object* remote_object_allocate(unsigned int rop_game_handle); // 0x00947C90 (:317)
static void remote_object_delete(remote_object* rop); // 0x009B5DB0 (:345)
static void multi_core_object_get_compressed_state(remote_object* rop, dc_state_list* current_state, dc_state_list* reference_state, dc_state_list* compressed_state, multi_connection* cp, multi_core_state_priority priority, unsigned short mask_bits); // 0x00947D90 (:384)
static void create_process_event_destroy_callback(object_create_data* data); // 0x009A9A30 (:392)
static void add_remote_object_to_hash_tables(remote_object* rop); // 0x009B6060 (:429)
static remote_object* multi_core_create_remote_object_core_internal(unsigned int game_handle, unsigned short net_handle, unsigned int remote_type); // 0x009D9770 (:440)
static bool create_process_allow_callback(object_create_data* data); // 0x00947DF0 (:469)
static bool create_process_event_callback(object_create_data* data); // 0x009D9840 (:480)
static void destroy_process_event_destroy_callback(object_destroy_data* data); // 0x009A9A40 (:496)
static void destroy_remote_object_internal_no_packet(remote_object* rop); // 0x009E3A40 (:501)
static bool destroy_allow_process_callback(object_destroy_data* data); // 0x009D98A0 (:532)
static bool destroy_process_event_callback(object_destroy_data* data); // 0x009D9900 (:565)
static void multi_core_object_init_common(); // 0x00947E50 (:791)
static void multi_core_set_next_network_handle(); // 0x009D9960 (:810)
static unsigned short get_network_handle_for_remote_object(unsigned int game_handle); // 0x009D9A40 (:1006)
static bool is_registered_level_load_remote_object_local_create_type(int object_type); // 0x00948090 (:1017)
static dc_object* get_dc_object_from_connection(multi_connection* cp, unsigned short net_handle); // 0x00948310 (:1772)
static void notify_update_not_sent(const rop_sort_element& cur_sort_element, multi_connection* cp); // 0x00948360 (:1797)

// The packet header size: the type and the 16-bit payload size, plus a 16-bit order id for grouped packets.
static unsigned int multi_core_object_packet_header_size(unsigned char type)
{
    return Multi_packets[type].group_id != 0 ? 5 : 3;
}

// The payload size (everything after the type and the size field) into the header.
static void multi_core_object_packet_set_size(unsigned char* packet, unsigned int size)
{
    const unsigned short payload = (unsigned short)(size - 3);
    packet[1] = (unsigned char)(payload >> 8);
    packet[2] = (unsigned char)payload;
}

// 0x00947C60 (:218): the higher weight first
bool sort_rop_ptr_elements(const rop_sort_element* lhs, const rop_sort_element* rhs)
{
    return rhs->weight < lhs->weight;
}

// 0x00947C80 (:250): empty in the shipped build
static void validate_remote_object_allocations(unsigned int rop_game_handle)
{
}

// 0x00947C90 (:317): the head of the free list moves to the head of the used list; the update times start now
static remote_object* remote_object_allocate(unsigned int rop_game_handle)
{
    remote_object* rop = Free_remote_objects_list;
    if (rop == nullptr)
    {
        return nullptr;
    }
    mc_list_remove(Free_remote_objects_list, rop, &remote_object::next, &remote_object::prev, true);
    rop->current_type = 0xFF;
    rop->m_pending_delete = 0;
    memset(rop->m_state_priority_bundle, 0, sizeof(rop->m_state_priority_bundle));
    for (int i = 0; i < 16; i++)
    {
        rop->conn_data[i].time_last_updated = Clock_current_time;
    }
    Remote_object_count++;
    mc_list_add_head(Remote_objects_list, rop, &remote_object::next, &remote_object::prev);
    return rop;
}

// 0x009B5DB0 (:345)
// LTCG: rop in ESI (the port has no register convention).
static void remote_object_delete(remote_object* rop)
{
    Game_handle_to_rop_hash.remove(rop->m_game_handle);
    Network_handle_to_rop_hash.remove(rop->m_network_handle);
    // A reserved handle pair is dropped only when the network handle is reserved (the game handle lookup is done but
    // not used).
    if (Reserved_network_to_game_hash.exists(rop->m_network_handle))
    {
        Reserved_network_to_game_hash.remove(rop->m_network_handle);
        Reserved_game_to_network_hash.remove(rop->m_game_handle);
    }
    mc_list_remove(Remote_objects_list, rop, &remote_object::next, &remote_object::prev, true);
    Remote_object_count--;
    mc_list_add_head(Free_remote_objects_list, rop, &remote_object::next, &remote_object::prev);
}

// 0x00947D90 (:384)
// LTCG: rop in EAX (the port has no register convention).
static void multi_core_object_get_compressed_state(remote_object* rop, dc_state_list* current_state, dc_state_list* reference_state, dc_state_list* compressed_state, multi_connection* cp, multi_core_state_priority priority, unsigned short mask_bits)
{
    Object_type_registration_list[rop->current_type]->get_compressed_state(current_state, reference_state, compressed_state, cp, priority, mask_bits);
}

// 0x009A9A30 (:392)
static void create_process_event_destroy_callback(object_create_data* data)
{
    Event_create_allocator.free(data);
}

// 0x00947DD0 (:397)
unsigned short multi_core_get_next_reserved_network_handle()
{
    return (unsigned short)Num_reserved_network_handles++;
}

// 0x009B5F10 (:407): a game handle gets the next reserved network handle once (level objects every peer creates
// itself)
void multi_core_add_reserved_network_handle(unsigned int game_handle)
{
    if (Reserved_game_to_network_hash.exists(game_handle))
    {
        return;
    }
    const unsigned int index = Num_reserved_network_handles & 0xFFFF;
    network_game_handle* handles = &Network_game_handles[index];
    handles->m_game_handle = game_handle;
    handles->m_network_handle = (unsigned short)Num_reserved_network_handles;
    Num_reserved_network_handles++;
    Reserved_game_to_network_hash.insert(game_handle, handles);
    Reserved_network_to_game_hash.insert(index, handles);
}

// 0x009E39F0 (:420)
void multi_core_level_init_pre()
{
    multi_core_sim_event_clear();
    Game_handle_to_rop_hash.clear();
    Network_handle_to_rop_hash.clear();
    Reserved_game_to_network_hash.clear();
    Reserved_network_to_game_hash.clear();
    Num_reserved_network_handles = 0;
    Cur_network_handle = 1000; // Magic number: the first network handle (lower ones are reserved)
    multi_core_object_init_common();
}

// 0x009B6060 (:429)
static void add_remote_object_to_hash_tables(remote_object* rop)
{
    Game_handle_to_rop_hash.insert(rop->m_game_handle, rop);
    Network_handle_to_rop_hash.insert(rop->m_network_handle, rop);
}

// 0x009D9770 (:440): a remote object for a game object that exists, with its delta compression master object
// LTCG: game_handle in EDI, net_handle and remote_type on the stack (the port has no register convention).
static remote_object* multi_core_create_remote_object_core_internal(unsigned int game_handle, unsigned short net_handle, unsigned int remote_type)
{
    if ((int)remote_type < 0 || remote_type >= 12 || Object_type_registration_list[remote_type] == nullptr)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(game_handle);
    if (objp == nullptr)
    {
        return nullptr;
    }
    objp->remote_obj_flags.remote_object = 1;
    // Original: the free list is not checked for an empty pool (255 objects).
    remote_object* rop = remote_object_allocate(game_handle);
    rop->m_game_handle = game_handle;
    rop->m_network_handle = net_handle;
    rop->current_type = (unsigned char)remote_type;
    add_remote_object_to_hash_tables(rop);
    mc_list_add_head(Remote_objects_by_type[remote_type], rop, &remote_object::type_next, &remote_object::type_prev);
    multi_core_dc_create_master_object(net_handle);
    return rop;
}

// 0x00947DF0 (:469)
static bool create_process_allow_callback(object_create_data* data)
{
    if (data->type < 12 && Object_type_registration_list[data->type] != nullptr)
    {
        int packet_offset = 0;
        return Object_type_registration_list[data->type]->allow_create_packet(packet_offset, data->packet_data);
    }
    return true;
}

// 0x009D9840 (:480): the registration creates the game object, then the remote object is made for it
static bool create_process_event_callback(object_create_data* data)
{
    if (data->type < 12 && Object_type_registration_list[data->type] != nullptr)
    {
        unsigned int game_handle;
        int packet_offset = 0;
        Object_type_registration_list[data->type]->process_create_packet(game_handle, data->net_handle, packet_offset, data->packet_data);
        multi_core_create_remote_object_core_internal(game_handle, data->net_handle, data->type);
    }
    return true;
}

// 0x009A9A40 (:496)
static void destroy_process_event_destroy_callback(object_destroy_data* data)
{
    Event_destroy_allocator.free(data);
}

// 0x009E3A40 (:501)
// LTCG: rop in EAX (the port has no register convention).
static void destroy_remote_object_internal_no_packet(remote_object* rop)
{
    object* objp = World.object_from_handle(rop->m_game_handle);
    if (objp == nullptr)
    {
        return;
    }
    const unsigned char type = rop->current_type;
    objp->remote_obj_flags.remote_object = 0;
    multi_core_dc_destroy_master_object(rop);
    mc_list_remove(Remote_objects_by_type[type], rop, &remote_object::type_next, &remote_object::type_prev, false);
    remote_object_delete(rop);
}

// 0x009D98A0 (:532): a destroy for an unknown network handle is refused
static bool destroy_allow_process_callback(object_destroy_data* data)
{
    if (data->type < 12 && Object_type_registration_list[data->type] != nullptr)
    {
        remote_object* rop = multi_core_get_remote_object_from_network_handle(data->net_handle);
        if (rop == nullptr)
        {
            return false;
        }
        int packet_offset = 0;
        return Object_type_registration_list[rop->current_type]->allow_destroy_packet(rop->m_game_handle, packet_offset, data->packet_data);
    }
    return true;
}

// 0x009D9900 (:565)
static bool destroy_process_event_callback(object_destroy_data* data)
{
    if (data->type < 12 && Object_type_registration_list[data->type] != nullptr)
    {
        remote_object* rop = multi_core_get_remote_object_from_network_handle(data->net_handle);
        if (rop != nullptr)
        {
            int packet_offset = 0;
            Object_type_registration_list[rop->current_type]->process_destroy_packet(rop->m_game_handle, packet_offset, data->packet_data);
        }
    }
    return true;
}

// 0x009B6160 (:599): the frame's snapshot of the remote objects not pending deletion (by type); a host in game stores
// their states for this frame's master ack id; destroying remote objects waits until the send job ends
void multi_core_object_send_job_pre(const mp_transmit_list& transmit_list)
{
    Snapshot_remote_object_list.remove_all();
    for (int type = 0; type < 12; type++)
    {
        remote_object* rop = Remote_objects_by_type[type];
        while (rop != nullptr)
        {
            if (rop->m_pending_delete != 1)
            {
                Snapshot_remote_object_list.add(rop); // a critical failure message when full (255)
            }
            rop = rop->type_next;
            if (rop == Remote_objects_by_type[type])
            {
                break;
            }
        }
    }
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    if (sp != nullptr && sp->am_i_host() && sp->get_state() == MSS_IN_GAME && gameseq_state_is_active(GS_GAMEPLAY) && Migration_state == MIGRATION_STATE_NONE && !Multi_game_has_skipped_game_migration && transmit_list.num_transmit_connections > 0)
    {
        multi_core_store_object_states();
    }
    Allow_destroy_remote_object = false;
}

// 0x00947E30 (:627)
bool multi_core_object_registration::allow_create_packet(int& packet_offset, unsigned char* packet_data) const
{
    return true;
}

// 0x00947E40 (:632)
bool multi_core_object_registration::allow_destroy_packet(unsigned int game_handle, int& packet_offset, unsigned char* packet_data) const
{
    return true;
}

// 0x009E3AE0 (:783)
void multi_core_destroy_remote_object_no_packet_external(unsigned short net_handle)
{
    remote_object* rop = Network_handle_to_rop_hash.get(net_handle);
    if (rop != nullptr)
    {
        destroy_remote_object_internal_no_packet(rop);
    }
}

// 0x00947E50 (:791): every pool object on the free list (the last one at its head)
static void multi_core_object_init_common()
{
    Free_remote_objects_list = nullptr;
    Remote_objects_list = nullptr;
    Remote_object_count = 0;
    for (int i = 0; i < 12; i++)
    {
        Remote_objects_by_type[i] = nullptr;
    }
    for (int i = 0; i < 255; i++) // Magic number: the pool's 255 remote objects (multi_core_objects_init)
    {
        mc_list_add_head(Free_remote_objects_list, &Remote_objects_pool[i], &remote_object::next, &remote_object::prev);
    }
}

// 0x009D9960 (:810): the next handle no remote object uses (1000-65534)
static void multi_core_set_next_network_handle()
{
    do
    {
        Cur_network_handle++;
        if (Cur_network_handle == 0xFFFF)
        {
            Cur_network_handle = 1000;
        }
    } while (Network_handle_to_rop_hash.get(Cur_network_handle) != nullptr);
}

// 0x009D99C0 (:831): the same walk as multi_core_set_next_network_handle (inlined)
void multi_core_migrate_fixup_network_handles()
{
    multi_core_set_next_network_handle();
}

// 0x009D99D0 (:836): a migrating client keeps the reserved handles
void multi_core_objects_reinit(bool client_migrate)
{
    multi_core_sim_event_clear();
    Game_handle_to_rop_hash.clear();
    Network_handle_to_rop_hash.clear();
    if (!client_migrate)
    {
        Reserved_game_to_network_hash.clear();
        Reserved_network_to_game_hash.clear();
        Num_reserved_network_handles = 0;
    }
    Cur_network_handle = 1000;
    multi_core_object_init_common();
}

// 0x009B6240 (:861): the create and destroy events still queued
void multi_core_sim_event_clear()
{
    for (object_create_data* data = Event_create_allocator.get_first(0); data != nullptr;)
    {
        data = Event_create_allocator.free(data);
    }
    for (object_destroy_data* data = Event_destroy_allocator.get_first(0); data != nullptr;)
    {
        data = Event_destroy_allocator.free(data);
    }
}

// 0x009B62D0 (:869): the pools, the lookups (1024 slots and entries each) and the arrays from the multiplayer memory
void multi_core_objects_init()
{
    Event_create_allocator.init(MP_allocators_mempool);
    Event_destroy_allocator.init(MP_allocators_mempool);
    Reserved_game_to_network_hash.init(MP_allocators_mempool, 1024);
    Reserved_network_to_game_hash.init(MP_allocators_mempool, 1024);
    Game_handle_to_rop_hash.init(MP_allocators_mempool, 1024);
    Network_handle_to_rop_hash.init(MP_allocators_mempool, 1024);
    Debug_remote_object_hash.init(MP_allocators_mempool, 1024);
    Rop_sort_element = (rop_sort_element*)MP_allocators_mempool.alloc(255 * sizeof(rop_sort_element), 0);
    PTR_rop_sort_element = (rop_sort_element**)MP_allocators_mempool.alloc(255 * sizeof(rop_sort_element*), 0);
    Network_game_handles = (network_game_handle*)MP_allocators_mempool.alloc(1024 * sizeof(network_game_handle), 0);
    if (Remote_objects_pool == nullptr)
    {
        Remote_objects_pool = (remote_object*)MP_allocators_mempool.alloc(255 * sizeof(remote_object), 0);
    }
    for (int i = 0; i < 12; i++)
    {
        Object_type_registration_list[i] = nullptr;
    }
    Num_registered_level_load_remote_object_local_create_types = 0;
    Snapshot_remote_object_list.init((remote_object**)MP_allocators_mempool.alloc(255 * sizeof(remote_object*), 0), 255, 0);
    multi_core_object_init_common();
}

// 0x00947FE0 (:923): the arrays belong to the multiplayer memory
void multi_core_objects_shutdown()
{
    Remote_objects_pool = nullptr;
    Rop_sort_element = nullptr;
    PTR_rop_sort_element = nullptr;
    Network_game_handles = nullptr;
    Num_reserved_network_handles = 0;
    Cur_network_handle = 1000;
}

// 0x00948010 (:945)
unsigned int multi_core_object_remote_object_count()
{
    unsigned int count = 0;
    remote_object* rop = Remote_objects_list;
    while (rop != nullptr)
    {
        rop = rop->next;
        count++;
        if (rop == Remote_objects_list)
        {
            break;
        }
    }
    return count;
}

// 0x00948030 (:962)
unsigned int multi_core_object_remote_object_count(unsigned int registered_type)
{
    unsigned int count = 0;
    remote_object* rop = Remote_objects_by_type[registered_type];
    while (rop != nullptr)
    {
        rop = rop->type_next;
        count++;
        if (rop == Remote_objects_by_type[registered_type])
        {
            break;
        }
    }
    return count;
}

// 0x00948060 (:979): the first registration of a type stays
void multi_core_object_register_object_type(unsigned int type, const multi_core_object_registration* object_registration)
{
    if (type < 12 && Object_type_registration_list[type] == nullptr)
    {
        multi_core_object_registration* registration = const_cast<multi_core_object_registration*>(object_registration);
        registration->registration_type = type;
        Object_type_registration_list[type] = registration;
    }
}

// 0x009D9A20 (:997)
unsigned short multi_core_get_next_network_handle()
{
    const unsigned short net_handle = Cur_network_handle;
    multi_core_set_next_network_handle();
    return net_handle;
}

// 0x009D9A40 (:1006): the reserved or current handle of the game handle, else a new one
// LTCG: game_handle in EAX (the port has no register convention).
static unsigned short get_network_handle_for_remote_object(unsigned int game_handle)
{
    unsigned short net_handle = multi_core_network_handle_from_game_handle(game_handle, true);
    if (net_handle == 0xFFFF)
    {
        net_handle = Cur_network_handle;
        multi_core_set_next_network_handle();
    }
    return net_handle;
}

// 0x00948090 (:1017)
// LTCG: object_type in EDX (the port has no register convention).
static bool is_registered_level_load_remote_object_local_create_type(int object_type)
{
    for (int i = 0; i < Num_registered_level_load_remote_object_local_create_types; i++)
    {
        if (Registered_level_load_remote_object_local_create_types[i] == object_type)
        {
            return true;
        }
    }
    return false;
}

// 0x009D9A70 (:1028): a level object every peer creates itself, under its reserved handle
void multi_core_create_level_load_sync_remote_object(unsigned int _registered_type, unsigned int game_handle)
{
    const unsigned short net_handle = multi_core_network_handle_from_game_handle(game_handle, false);
    if (net_handle != 0xFFFF)
    {
        multi_core_create_remote_object_core_internal(game_handle, net_handle, _registered_type);
    }
}

// The PT_REMOTE_OBJECT_CREATE packet of a remote object (multi_core_object_create_remote_object and
// multi_core_object_connection_setup inline it): the type, the network handle, then the registration's data.
static unsigned int multi_core_object_build_create_packet(unsigned char* packet, multi_core_object_registration* registration, remote_object* rop, multi_connection* dest_conn, unsigned short net_handle)
{
    packet[0] = PT_REMOTE_OBJECT_CREATE;
    unsigned int size = multi_core_object_packet_header_size(PT_REMOTE_OBJECT_CREATE);
    packet[size] = rop->current_type;
    size++;
    packet[size & 0xFFFF] = (unsigned char)(net_handle >> 8);
    packet[(size & 0xFFFF) + 1] = (unsigned char)net_handle;
    size += 2;
    unsigned short packet_size = (unsigned short)size;
    registration->send_create_packet(rop, packet, packet_size, dest_conn);
    multi_core_object_packet_set_size(packet, packet_size);
    return packet_size;
}

// 0x009D9AA0 (:1052): the server creates the remote object and sends its create packet to every client in gameplay
void multi_core_object_create_remote_object(unsigned int _registered_type, unsigned int game_handle)
{
    if (!Multiplayer || !Server)
    {
        return;
    }
    const unsigned short net_handle = get_network_handle_for_remote_object(game_handle);
    remote_object* rop = multi_core_create_remote_object_core_internal(game_handle, net_handle, _registered_type);
    if (rop == nullptr)
    {
        return;
    }
    multi_core_object_registration* registration = Object_type_registration_list[_registered_type];
    registration->create(rop->m_game_handle);
    if (Multi_game_has_skipped_game_migration)
    {
        return;
    }
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    if (sp == nullptr || sp->get_host_connection() == nullptr)
    {
        return;
    }
    multi_connection* host = sp->get_host_connection();
    multi_connection* cp = sp->get_connection_list();
    do
    {
        if (cp == nullptr)
        {
            return;
        }
        if (cp->connection_status == CS_CONNECTED_GAMEPLAY && cp != host)
        {
            unsigned char packet[1200];
            const unsigned int size = multi_core_object_build_create_packet(packet, registration, rop, cp, multi_core_network_handle_from_game_handle(rop->m_game_handle, false));
            multi_io_send_reliable(cp, packet, size & 0xFFFF, nullptr, nullptr, nullptr);
        }
        cp = cp->next;
    } while (cp != sp->get_connection_list());
}

// 0x009480C0 (:1102): up to 10 types
void multi_core_objects_register_level_load_remote_object_local_create_types(int object_type)
{
    if (Num_registered_level_load_remote_object_local_create_types < 10 && (unsigned int)object_type < 12)
    {
        Registered_level_load_remote_object_local_create_types[Num_registered_level_load_remote_object_local_create_types++] = object_type;
    }
}

// 0x009D9C60 (:1114): a client entering gameplay gets a create packet for every remote object, type 2 first (players);
// the level load types every peer creates itself are skipped unless the session is migrating
void multi_core_object_connection_setup(multi_connection* cp)
{
    for (int pass = 0; pass < 13; pass++)
    {
        // Pass 0 is type 2; passes 1-12 are types 0-11 without type 2.
        const int type = pass == 0 ? 2 : pass - 1;
        if (pass != 0 && type == 2)
        {
            continue;
        }
        if (Migration_state == MIGRATION_STATE_NONE && is_registered_level_load_remote_object_local_create_type(type))
        {
            continue;
        }
        remote_object* rop = Remote_objects_by_type[type];
        while (rop != nullptr)
        {
            unsigned char packet[1200];
            const unsigned int size = multi_core_object_build_create_packet(packet, Object_type_registration_list[type], rop, cp, multi_core_network_handle_from_game_handle(rop->m_game_handle, true));
            multi_io_send_reliable(cp, packet, size & 0xFFFF, nullptr, nullptr, nullptr);
            rop = rop->type_next;
            if (rop == Remote_objects_by_type[type])
            {
                break;
            }
        }
    }
}

// 0x009D9FB0 (:1176): the server moves the remote object to another registered type's list
void multi_core_object_change_type(unsigned int registered_type, unsigned int game_handle)
{
    if (!Multiplayer || !Server || registered_type > 11 || Object_type_registration_list[registered_type] == nullptr)
    {
        return;
    }
    remote_object* rop = multi_core_get_remote_object_from_game_handle(game_handle);
    if (rop == nullptr || rop->current_type == registered_type)
    {
        return;
    }
    mc_list_remove(Remote_objects_by_type[rop->current_type], rop, &remote_object::type_next, &remote_object::type_prev, false);
    mc_list_add_head(Remote_objects_by_type[registered_type], rop, &remote_object::type_next, &remote_object::type_prev);
    rop->current_type = (unsigned char)registered_type;
}

// 0x009DA0E0 (:1204): the object is destroyed after the send job (multi_core_object_destroy_buffered_remote_objects)
void multi_core_object_buffer_destroy_remote_object(unsigned short net_handle)
{
    remote_object* rop = Network_handle_to_rop_hash.get(net_handle);
    if (rop != nullptr)
    {
        rop->m_pending_delete = 1;
    }
}

// The PT_REMOTE_OBJECT_DESTROY packet of a remote object to every client in gameplay (multi_core_object_destroy_remote_object
// and multi_core_object_destroy_buffered_remote_objects inline it): the network time (32 bits), the type, the network
// handle, then the registration's data.
static void multi_core_object_send_destroy_packet(remote_object* rop)
{
    unsigned char packet[1200];
    packet[0] = PT_REMOTE_OBJECT_DESTROY;
    const unsigned int header = multi_core_object_packet_header_size(PT_REMOTE_OBJECT_DESTROY);
    packet[header] = (unsigned char)(Clock_current_time >> 24);
    packet[header + 1] = (unsigned char)(Clock_current_time >> 16);
    packet[header + 2] = (unsigned char)(Clock_current_time >> 8);
    packet[header + 3] = (unsigned char)Clock_current_time;
    packet[header + 4] = rop->current_type;
    packet[header + 5] = (unsigned char)(rop->m_network_handle >> 8);
    packet[header + 6] = (unsigned char)rop->m_network_handle;
    unsigned short size = (unsigned short)(header + 7);
    Object_type_registration_list[rop->current_type]->send_destroy_packet(rop->m_game_handle, packet, size);
    multi_core_object_packet_set_size(packet, size);
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    if (sp == nullptr || sp->get_host_connection() == nullptr)
    {
        return;
    }
    multi_connection* host = sp->get_host_connection();
    multi_connection* cp = sp->get_connection_list();
    do
    {
        if (cp == nullptr)
        {
            break;
        }
        if (cp->connection_status == CS_CONNECTED_GAMEPLAY && cp != host)
        {
            multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
        }
        cp = cp->next;
    } while (cp != sp->get_connection_list());
}

// 0x009F7120 (:1216): the server destroys the objects marked during the send job
void multi_core_object_destroy_buffered_remote_objects()
{
    Allow_destroy_remote_object = true;
    remote_object* rop = Remote_objects_list;
    while (rop != nullptr)
    {
        remote_object* const next = rop->next != Remote_objects_list ? rop->next : nullptr;
        if (rop->m_pending_delete != 0 && Multiplayer && Server)
        {
            remote_object* found = Network_handle_to_rop_hash.get(rop->m_network_handle);
            if (found != nullptr)
            {
                if (!Multi_game_has_skipped_game_migration)
                {
                    multi_core_object_send_destroy_packet(found);
                }
                destroy_remote_object_internal_no_packet(found);
            }
        }
        rop = next;
    }
}

// 0x009480F0 (:1230)
void multi_core_object_set_allow_destroy_remote_object(bool allow)
{
    Allow_destroy_remote_object = allow;
}

// 0x009E3B20 (:1241)
void multi_core_object_destroy_remote_object(unsigned short net_handle)
{
    if (!Multiplayer || !Server)
    {
        return;
    }
    remote_object* rop = multi_core_get_remote_object_from_network_handle(net_handle);
    if (rop == nullptr)
    {
        return;
    }
    if (!Multi_game_has_skipped_game_migration)
    {
        multi_core_object_send_destroy_packet(rop);
    }
    destroy_remote_object_internal_no_packet(rop);
}

// 0x00948100 (:1301): the client moves every remote object (interpolation of the received states)
void multi_core_object_move_all()
{
    for (int type = 0; type < 12; type++)
    {
        if (Object_type_registration_list[type] == nullptr)
        {
            continue;
        }
        remote_object* rop = Remote_objects_by_type[type];
        while (rop != nullptr)
        {
            Object_type_registration_list[type]->move(rop->m_game_handle);
            rop = rop->type_next;
            if (rop == Remote_objects_by_type[type])
            {
                break;
            }
        }
    }
}

// 0x009DA120 (:1324)
// A client in game queues the create as a sim event (network time 0, or the clock during a migration); without a free
// event slot it is applied at once from the temporary data.
void multi_core_object_process_create_packet(unsigned char* packet_data, const sender& from)
{
    if (Client_state != CLIENT_STATE_IN_GAME)
    {
        return;
    }
    object_create_data* data = Event_create_allocator.alloc(0);
    const bool temporary = data == nullptr;
    if (temporary)
    {
        data = &Temp_create_event_data;
    }
    data->type = packet_data[0];
    data->net_handle = (unsigned short)((packet_data[1] << 8) | packet_data[2]);
    const unsigned int network_time = Migration_state != MIGRATION_STATE_NONE ? Clock_current_time : 0;
    // Original: 512 bytes are copied whatever the packet's size.
    memcpy(data->packet_data, packet_data + 3, sizeof(data->packet_data));
    if (temporary)
    {
        create_process_event_callback(data);
        return;
    }
    multi_game_sim_event_add(network_time, MULTI_GAME_SIM_EVENT_TYPE_CREATE_OBJECT, false, data, (bool (*)(void*))create_process_allow_callback, (bool (*)(void*))create_process_event_callback, (void (*)(void*))create_process_event_destroy_callback);
}

// 0x009DA1C0 (:1375): queued at the server's network time
void multi_core_object_process_destroy_packet(unsigned char* packet_data, const sender& from)
{
    object_destroy_data* data = Event_destroy_allocator.alloc(0);
    const bool temporary = data == nullptr;
    if (temporary)
    {
        data = &Temp_destroy_event_data;
    }
    const unsigned int network_time = ((unsigned int)packet_data[0] << 24) | ((unsigned int)packet_data[1] << 16) | ((unsigned int)packet_data[2] << 8) | packet_data[3];
    data->type = packet_data[4];
    data->net_handle = (unsigned short)((packet_data[5] << 8) | packet_data[6]);
    memcpy(data->packet_data, packet_data + 7, sizeof(data->packet_data));
    if (temporary)
    {
        destroy_process_event_callback(data);
        return;
    }
    multi_game_sim_event_add(network_time, MULTI_GAME_SIM_EVENT_TYPE_DESTROY_OBJECT, false, data, (bool (*)(void*))destroy_allow_process_callback, (bool (*)(void*))destroy_process_event_callback, (void (*)(void*))destroy_process_event_destroy_callback);
}

// 0x009E3CD0 (:1426)
// PT_REMOTE_OBJECT_UPDATE on a client: the network time (32 bits), the server's ack id (16), the object count (8), then
// per object its network handle, the reference ack id and the data size (16 bits each) and the compressed state. Each
// state is applied against the stored reference state; when that state is gone the host is told to send a full update.
void multi_core_object_process_update_packet(unsigned char* packet_data, const sender& from)
{
    if (Client_state != CLIENT_STATE_IN_GAME || Migration_state != MIGRATION_STATE_NONE)
    {
        return;
    }
    const unsigned int network_time = ((unsigned int)packet_data[0] << 24) | ((unsigned int)packet_data[1] << 16) | ((unsigned int)packet_data[2] << 8) | packet_data[3];
    const unsigned short ack_id = (unsigned short)((packet_data[4] << 8) | packet_data[5]);
    unsigned int num_objects = packet_data[6];
    int offset = 7;
    if (num_objects == 0)
    {
        return;
    }
    unsigned char data[1024];
    do
    {
        const unsigned short net_handle = (unsigned short)((packet_data[offset] << 8) | packet_data[offset + 1]);
        const unsigned short reference_ack_id = (unsigned short)((packet_data[offset + 2] << 8) | packet_data[offset + 3]);
        const unsigned short size = (unsigned short)((packet_data[offset + 4] << 8) | packet_data[offset + 5]);
        offset += 6;
        // Original: a size of 1024 or more ends nothing; the walk goes on at the same offset.
        if (size < 1024)
        {
            if (size != 0)
            {
                memcpy(data, packet_data + offset, size);
                offset += size;
            }
            remote_object* rop = Network_handle_to_rop_hash.get(net_handle);
            if (rop != nullptr)
            {
                multi_connection* cp = from.connection;
                dc_state_list compressed_state;
                compressed_state.m_network_handle = 0xFFFF;
                compressed_state.m_data = data;
                compressed_state.m_ack_id = ack_id;
                compressed_state.m_reference_ackid = reference_ack_id;
                compressed_state.m_size = size;
                dc_state_list* reference_state = nullptr;
                bool apply = true;
                if (reference_ack_id != 0)
                {
                    dc_master_object* dcmo = get_master_object_from_remote_object_ptr(rop);
                    if (dcmo == nullptr)
                    {
                        keen::debug::nativePrintArgs("Invalid DCMO");
                        apply = false;
                    }
                    else
                    {
                        reference_state = multi_core_dc_get_state_for_object_for_ackid(dcmo, reference_ack_id);
                        if (reference_state == nullptr)
                        {
                            apply = false;
                            if (cp == nullptr)
                            {
                                keen::debug::nativePrintArgs("Invalid connection 1");
                            }
                            else if (cp->owner == nullptr)
                            {
                                keen::debug::nativePrintArgs("Invalid connection owner 1");
                            }
                            else if (cp->owner->get_host_connection() == nullptr)
                            {
                                keen::debug::nativePrintArgs("Invalid host connection 1");
                            }
                            else
                            {
                                multi_core_dc_mark_reference_state_not_found(cp->owner->get_host_connection(), dcmo->m_network_handle);
                            }
                        }
                    }
                }
                if (apply)
                {
                    dc_state_list* resultant_state = nullptr;
                    remote_object* owner = Game_handle_to_rop_hash.get(rop->m_game_handle);
                    if (owner != nullptr && Object_type_registration_list[owner->current_type]->apply_compressed_state(rop->m_game_handle, reference_state, &compressed_state, &resultant_state, network_time))
                    {
                        if (resultant_state == nullptr)
                        {
                            multi_core_dc_store_state_for_object(rop, ack_id);
                        }
                        else
                        {
                            multi_core_dc_store_state_for_object(rop, ack_id, resultant_state);
                        }
                        if (cp == nullptr)
                        {
                            keen::debug::nativePrintArgs("Invalid connection 2");
                        }
                        else if (cp->owner == nullptr)
                        {
                            keen::debug::nativePrintArgs("Invalid connection owner 2");
                        }
                        else if (cp->owner->get_host_connection() == nullptr)
                        {
                            keen::debug::nativePrintArgs("Invalid host connection 2");
                        }
                        else
                        {
                            multi_core_dc_update_ackids_for_object(cp->owner->get_host_connection(), rop->m_network_handle, ack_id, reference_ack_id);
                        }
                    }
                }
            }
        }
        num_objects--;
    } while (num_objects != 0);
}

// 0x009DA290 (:1569): a game object freed while it is replicated
void multi_core_object_process_object_free(object* obj)
{
    if (!Multiplayer)
    {
        return;
    }
    const int type = multi_game_get_remote_object_type(obj);
    if (type != 0 && Object_type_registration_list[type] != nullptr)
    {
        Object_type_registration_list[type]->process_object_free(obj);
    }
}

// 0x00948150 (:1592)
const char* multi_core_object_complex_type_name(unsigned int object_type)
{
    if (object_type < 12 && Object_type_registration_list[object_type] != nullptr)
    {
        return Object_type_registration_list[object_type]->name;
    }
    return "UNKNOWN TYPE";
}

// 0x00948170 (:1605)
void multi_core_object_get_state(const remote_object* rop, dc_state_list* dcsl)
{
    Object_type_registration_list[rop->current_type]->get_state(rop->m_game_handle, dcsl);
}

// 0x009481A0 (:1614): without both states the object counts as dirty
bool multi_core_object_check_dirty(const remote_object* rop, dc_state_list* ref_state, dc_state_list* cur_state)
{
    if (cur_state == nullptr || ref_state == nullptr)
    {
        return true;
    }
    return Object_type_registration_list[rop->current_type]->check_object_is_dirty(ref_state, cur_state, rop);
}

// 0x009DA2C0 (:1633)
void multi_core_object_free_state(unsigned int game_handle, dc_state_list* dcsl)
{
    remote_object* rop = Game_handle_to_rop_hash.get(game_handle);
    if (rop != nullptr)
    {
        Object_type_registration_list[rop->current_type]->free_state(dcsl);
    }
}

// 0x009DA310 (:1660)
bool multi_core_object_apply_compressed_state(unsigned int game_handle, dc_state_list* reference_state, dc_state_list* compressed_state, dc_state_list** resultant_state, unsigned int network_time)
{
    remote_object* rop = Game_handle_to_rop_hash.get(game_handle);
    if (rop == nullptr)
    {
        return false;
    }
    return Object_type_registration_list[rop->current_type]->apply_compressed_state(game_handle, reference_state, compressed_state, resultant_state, network_time);
}

// 0x009A9A50 (:1673): every snapshot object's state for this frame (multi_core_dc_store_state_for_object_for_this_frame
// inlined)
void multi_core_store_object_states()
{
    const int count = Snapshot_remote_object_list.size();
    for (int i = 0; i < count; i++)
    {
        multi_core_dc_store_state_for_object_for_this_frame(Snapshot_remote_object_list[i]);
    }
}

// 0x009481E0 (:1687)
void multi_core_cleanup_object_states()
{
    for (int i = 0; i < 12; i++)
    {
        if (Object_type_registration_list[i] != nullptr)
        {
            Object_type_registration_list[i]->post_update_cleanup();
        }
    }
}

// 0x00948200 (:1706)
void multi_core_query_space_for_priority(const remote_object* rop, dc_state_list* current_state, dc_state_list* reference_state, unsigned short possible_mask_bits, multi_connection* cp, query_size_results* qs_results)
{
    Object_type_registration_list[rop->current_type]->query_space_for_priority(rop, current_state, reference_state, possible_mask_bits, cp, qs_results);
}

// 0x00948240 (:1716)
unsigned short multi_core_query_possible_mask_bits(remote_object* rop, dc_state_list* current_state, dc_state_list* reference_state, multi_connection* cp)
{
    if (current_state == nullptr)
    {
        return 0;
    }
    return Object_type_registration_list[rop->current_type]->query_possible_mask_bits(current_state, reference_state, cp);
}

// 0x00948270 (:1729)
bool multi_core_check_full_update(dc_state_list* ref_state, const remote_object* rop, multi_connection* cp)
{
    return Object_type_registration_list[rop->current_type]->check_full_update(ref_state, cp);
}

// 0x009482A0 (:1736)
void multi_core_notify_transmitted_update_callback(remote_object* rop, dc_state_list* current_state, multi_connection* recipient, unsigned short possible_mask_bits, unsigned short mask_bits_transmitted)
{
    Object_type_registration_list[rop->current_type]->notify_transmitted_updated(rop, current_state, recipient, possible_mask_bits, mask_bits_transmitted);
}

// 0x009482D0 (:1745)
void multi_core_object_reset_state_data()
{
    for (int i = 0; i < 12; i++)
    {
        if (Object_type_registration_list[i] != nullptr)
        {
            Object_type_registration_list[i]->reset_state_data();
        }
    }
}

// 0x009482F0 (:1759)
void multi_core_object_free_mp_sim_state()
{
    for (int i = 0; i < 12; i++)
    {
        if (Object_type_registration_list[i] != nullptr)
        {
            Object_type_registration_list[i]->free_mp_state_data();
        }
    }
}

// 0x00948310 (:1772)
// LTCG: cp in EAX, net_handle in DX (the port has no register convention).
static dc_object* get_dc_object_from_connection(multi_connection* cp, unsigned short net_handle)
{
    dc_object* dco = cp->dc_objects;
    while (dco != nullptr)
    {
        if (dco->m_network_handle == net_handle)
        {
            return dco;
        }
        dco = dco->connection_next;
        if (dco == cp->dc_objects)
        {
            return nullptr;
        }
    }
    return nullptr;
}

// 0x00948340 (:1785)
int multi_core_get_number_of_update_cycles_since_last_reset()
{
    return Num_object_updates_sent;
}

// 0x00948350 (:1790)
void multi_core_reset_number_object_update_cycles()
{
    Num_object_updates_sent = 0;
}

// 0x00948360 (:1797)
// LTCG: cur_sort_element in EDX (the port has no register convention).
static void notify_update_not_sent(const rop_sort_element& cur_sort_element, multi_connection* cp)
{
    if (cur_sort_element.possible_mask_bits != 0)
    {
        Object_type_registration_list[cur_sort_element.rop->current_type]->notify_transmitted_updated(cur_sort_element.rop, cur_sort_element.current_state, cp, cur_sort_element.possible_mask_bits, 0);
    }
}

// 0x009FB490 (:1841)
// The server's update packet for one connection within max_size bytes (appended to the connection's unreliable buffer;
// returns its size, 0 when nothing was written):
// 1. every snapshot object whose state changed against the connection's reference state (and that is not known clean
//    since that reference) is a candidate, weighted by distance to the connection's player (-10 per meter) plus 500
//    in front of its camera (cos > 0.97) or within 10 m;
// 2. the candidates sorted by weight ask their registrations for the sizes per priority; from the extreme priority
//    down, each candidate takes the largest size that fits the remaining budget (6 bytes of object header each);
// 3. the chosen states are compressed and written; those that did not fit are told so.
unsigned int multi_core_get_object_updates_for_connection(multi_connection* cp, unsigned long max_size)
{
    if (max_size <= 16 || cp->connection_status != CS_CONNECTED_GAMEPLAY)
    {
        return 0;
    }
    player* viewer = multi_game_data_get_player_instance(cp->game_data);
    vector view_dir = ZERO_VECTOR;
    unsigned int budget = max_size - 10;
    unsigned int written = 0;
    if (object_is_local_player(viewer))
    {
        view_dir = Rfg_camera.real_orient.fvec;
    }
    else if (viewer != nullptr)
    {
        view_dir = viewer->mp_camera_orient.fvec;
    }
    Num_object_updates_sent++;

    int num_candidates = 0;
    const int num_snapshot = Snapshot_remote_object_list.size();
    for (int i = 0; i < num_snapshot; i++)
    {
        remote_object* rop = Snapshot_remote_object_list[i];
        dc_master_object* dcmo = get_master_object_from_remote_object_ptr(rop);
        if (dcmo == nullptr)
        {
            continue;
        }
        dc_object* dco = get_dc_object_from_connection(cp, rop->m_network_handle);
        if (dco == nullptr)
        {
            continue;
        }
        dc_state_list* ref_state = multi_core_dc_get_state_for_object_for_ackid(dcmo, dco->m_current_reference_ack_id);
        if (ref_state != nullptr && dcmo->m_first_clean_ack_id != 0 && dco->m_current_reference_ack_id >= dcmo->m_first_clean_ack_id && multi_core_check_full_update(ref_state, rop, cp))
        {
            continue; // clean since the reference: nothing to send
        }
        if (dcmo->m_first_clean_ack_id == 0 || dcmo->m_first_clean_ack_id != dco->m_first_clean_ack_id)
        {
            dco->m_first_clean_ack_id = dcmo->m_first_clean_ack_id;
            dco->m_validated_clean_ack_id = 0;
        }
        if (dco->m_validated_clean_ack_id != 0 && dco->m_validated_clean_ack_id == dco->m_current_reference_ack_id)
        {
            continue;
        }
        unsigned short possible_mask_bits = 0;
        if (dcmo->m_current_state_dcsl != nullptr)
        {
            possible_mask_bits = Object_type_registration_list[rop->current_type]->query_possible_mask_bits(dcmo->m_current_state_dcsl, ref_state, cp);
        }
        if (possible_mask_bits == 0)
        {
            dco->m_validated_clean_ack_id = dco->m_current_reference_ack_id;
            continue;
        }
        rop_sort_element* element = &Rop_sort_element[num_candidates];
        dco->m_validated_clean_ack_id = 0;
        PTR_rop_sort_element[num_candidates] = element;
        num_candidates++;
        memset(element, 0, sizeof(*element));
        element->dco = dco;
        element->current_state = dcmo->m_current_state_dcsl;
        element->rop = rop;
        element->priority = MULTI_CORE_STATE_PRIORITY_NONE;
        if (viewer == nullptr)
        {
            element->weight = -1;
        }
        else
        {
            // Original: the object is not checked for null.
            object* objp = World.object_from_handle(rop->m_game_handle);
            vector delta;
            delta.x = (float)((double)objp->pos.x - (double)viewer->pos.x);
            delta.y = (float)((double)objp->pos.y - (double)viewer->pos.y);
            delta.z = (float)((double)objp->pos.z - (double)viewer->pos.z);
            const float dist = delta.mag();
            element->weight = (int)(-(double)dist * 10.0); // Magic number: -10 per meter
            float facing = 0.0f;
            if (objp != (object*)viewer && dist > 0.1f)
            {
                facing = (delta.x * view_dir.x + delta.y * view_dir.y + delta.z * view_dir.z) / dist;
            }
            if (facing > 0.97f || dist < 10.0f) // Magic numbers: in view (about 14 degrees) or within 10 m
            {
                element->weight = (int)(-(double)dist * 10.0) + 500;
            }
        }
        element->reference_state = ref_state;
        element->possible_mask_bits = possible_mask_bits;
    }
    if (num_candidates > 1)
    {
        // Original: VS2010's std::sort; equal weights may come out in another order with this build's.
        std::sort(PTR_rop_sort_element, PTR_rop_sort_element + num_candidates, sort_rop_ptr_elements);
    }

    for (int i = 0; i < num_candidates; i++)
    {
        rop_sort_element* element = PTR_rop_sort_element[i];
        Object_type_registration_list[element->rop->current_type]->query_space_for_priority(element->rop, element->current_state, element->reference_state, element->possible_mask_bits, cp, element->qs_results);
    }
    for (int priority = MULTI_CORE_STATE_PRIORITY_EXTREME; priority >= 0; priority--)
    {
        for (int i = 0; i < num_candidates; i++)
        {
            rop_sort_element* element = PTR_rop_sort_element[i];
            if (budget < 7)
            {
                break;
            }
            if (element->qs_results[priority].mask == 0)
            {
                continue;
            }
            unsigned int needed = element->qs_results[priority].num_bytes;
            const unsigned int available = element->last_size - 6 + budget;
            if (needed < available)
            {
                needed += 6;
                const int growth = (int)(needed - element->last_size);
                if (growth > 0)
                {
                    element->priority = (multi_core_state_priority)priority;
                    element->last_size = needed;
                    budget -= growth;
                    element->mask_bits = element->qs_results[priority].mask;
                }
            }
            else if (available < 20) // Magic number: less than 20 bytes left ends this priority's pass
            {
                break;
            }
        }
    }

    // The first half holds the packet, the second half each compressed state.
    unsigned char buffer[2400];
    unsigned char* const packet = buffer;
    packet[0] = PT_REMOTE_OBJECT_UPDATE;
    const unsigned int header = multi_core_object_packet_header_size(PT_REMOTE_OBJECT_UPDATE);
    packet[header] = (unsigned char)(Clock_current_time >> 24);
    packet[header + 1] = (unsigned char)(Clock_current_time >> 16);
    packet[header + 3] = (unsigned char)Clock_current_time;
    packet[header + 2] = (unsigned char)(Clock_current_time >> 8);
    packet[header + 4] = (unsigned char)(Master_ack_id_number >> 8);
    packet[header + 5] = (unsigned char)Master_ack_id_number;
    unsigned char* const num_objects = packet + header + 6;
    *num_objects = 0;
    unsigned int size = header + 7;
    for (int i = 0; i < num_candidates; i++)
    {
        rop_sort_element* element = PTR_rop_sort_element[i];
        remote_object* rop = element->rop;
        if (element->priority == MULTI_CORE_STATE_PRIORITY_NONE || element->last_size == 0)
        {
            notify_update_not_sent(*element, cp);
            continue;
        }
        dc_state_list compressed_state;
        compressed_state.m_ack_id = 0;
        compressed_state.m_reference_ackid = 0;
        compressed_state.m_network_handle = 0xFFFF;
        compressed_state.m_size = 0;
        compressed_state.m_data = buffer + 1200;
        multi_core_object_get_compressed_state(rop, element->current_state, element->reference_state, &compressed_state, cp, element->priority, element->mask_bits);
        // (rfg.exe looks the game object up when the size differs from the query's; a debug leftover without effect.)
        if (compressed_state.m_size == 0)
        {
            continue;
        }
        unsigned int end = size;
        packet[end & 0xFFFF] = (unsigned char)(rop->m_network_handle >> 8);
        packet[(end & 0xFFFF) + 1] = (unsigned char)rop->m_network_handle;
        packet[(end + 2) & 0xFFFF] = (unsigned char)(element->dco->m_current_reference_ack_id >> 8);
        packet[((end + 2) & 0xFFFF) + 1] = (unsigned char)element->dco->m_current_reference_ack_id;
        packet[(end + 4) & 0xFFFF] = (unsigned char)(compressed_state.m_size >> 8);
        packet[((end + 4) & 0xFFFF) + 1] = (unsigned char)compressed_state.m_size;
        end += 6;
        for (int b = 0; b < (int)compressed_state.m_size; b++)
        {
            packet[end & 0xFFFF] = compressed_state.m_data[b];
            end++;
        }
        // Original: an object that overruns max_size is written into the buffer but not counted (the next one
        // overwrites it).
        if ((end & 0xFFFF) <= max_size)
        {
            rop->conn_data[cp->index_in_session].time_last_updated = Clock_current_time;
            Object_type_registration_list[rop->current_type]->notify_transmitted_updated(rop, element->current_state, cp, element->possible_mask_bits, element->mask_bits);
            (*num_objects)++;
            size = end;
        }
    }
    multi_core_object_packet_set_size(packet, size);
    if (*num_objects != 0)
    {
        memcpy(cp->unreliable_buffer + cp->unreliable_buffer_size, packet, size & 0xFFFF);
        cp->unreliable_buffer_size += (unsigned short)size;
        written = size & 0xFFFF;
    }
    return written;
}

// 0x009B65D0 (:2211)
remote_object* multi_core_get_remote_object_from_network_handle(unsigned short net_handle)
{
    return Network_handle_to_rop_hash.get(net_handle);
}

// 0x009B6610 (:2216)
remote_object* multi_core_get_remote_object_from_game_handle(unsigned int game_handle)
{
    return Game_handle_to_rop_hash.get(game_handle);
}

// 0x00948390 (:2221)
state_priority_bundle* multi_core_get_state_priority_bundle(const remote_object* rop)
{
    return const_cast<state_priority_bundle*>(rop->m_state_priority_bundle);
}

// 0x009B6650 (:2227): the reserved handle first, then the remote objects; silent is unused in the shipped build
unsigned int multi_core_game_handle_from_network_handle(unsigned short net_handle, bool silent)
{
    if (net_handle == 0xFFFF)
    {
        return 0xFFFFFFFF;
    }
    network_game_handle* reserved = Reserved_network_to_game_hash.get(net_handle);
    if (reserved != nullptr)
    {
        return reserved->m_game_handle;
    }
    remote_object* rop = multi_core_get_remote_object_from_network_handle(net_handle);
    if (rop == nullptr)
    {
        return 0xFFFFFFFF;
    }
    return rop->m_game_handle;
}

// 0x009B66C0 (:2251)
unsigned short multi_core_network_handle_from_game_handle(unsigned int game_handle, bool silent)
{
    if (game_handle == 0xFFFFFFFF)
    {
        return 0xFFFF;
    }
    network_game_handle* reserved = Reserved_game_to_network_hash.get(game_handle);
    if (reserved != nullptr)
    {
        return reserved->m_network_handle;
    }
    remote_object* rop = multi_core_get_remote_object_from_game_handle(game_handle);
    if (rop == nullptr)
    {
        return 0xFFFF;
    }
    return rop->m_network_handle;
}

// 0x009DA380 (:2275)
void multi_core_create_remote_object_external(unsigned int game_handle, unsigned short net_handle, unsigned int remote_type)
{
    multi_core_create_remote_object_core_internal(game_handle, net_handle, remote_type);
}

// ---- multi_core_object_registration: the state masks of an object type (each mask a bit of the update's mask word:
// its bit count, the dirty epsilon, the compression range (pack threshold) and bias). A lookup of a mask that is not
// registered returns null, and the getters read through it (original: no check).

// 0x009483A0 (:2280): empty in the shipped build
void multi_core_object_registration::init_flag_bandwidth()
{
}

// 0x009483B0 (:2292)
const mask_info* multi_core_object_registration::get_mask_info(unsigned int mask) const
{
    for (unsigned int i = 0; i < num_masks; i++)
    {
        if (masks[i].mask == mask)
        {
            return &masks[i];
        }
    }
    return nullptr;
}

// 0x009483F0 (:2306)
void multi_core_object_registration::reset_state_masks()
{
    num_masks = 0;
}

// 0x00948400 (:2326): up to 64 masks
bool multi_core_object_registration::add_state_mask(const char* namep, const char* groupname, unsigned int mask, unsigned int num_bits, float epsilon, float pack_threshold, float pack_bias_value, float pack_bias)
{
    if (num_masks > 63)
    {
        return false;
    }
    mask_info* info = &masks[num_masks];
    num_masks++;
    info->epsilon = epsilon;
    info->pack_threshold = pack_threshold;
    info->name = namep;
    info->groupname = groupname;
    info->pack_bias_value = pack_bias_value;
    info->mask = mask;
    info->num_bits = num_bits;
    info->pack_bias = pack_bias;
    return true;
}

// 0x00948470 (:2355): the mask's name is its group
bool multi_core_object_registration::add_state_mask(const char* namep, unsigned int mask, unsigned int num_bits, float epsilon, float pack_threshold, float pack_bias_value, float pack_bias)
{
    return add_state_mask(namep, namep, mask, num_bits, epsilon, pack_threshold, pack_bias_value, pack_bias);
}

// 0x009484E0 (:2362)
unsigned int multi_core_object_registration::get_state_mask_num_bits(unsigned int mask) const
{
    return get_mask_info(mask)->num_bits;
}

// 0x00948520 (:2370)
float multi_core_object_registration::get_state_mask_epsilon(unsigned int mask) const
{
    return get_mask_info(mask)->epsilon;
}

// 0x00948560 (:2378)
float multi_core_object_registration::get_state_mask_pack_threshold(unsigned int mask) const
{
    return get_mask_info(mask)->pack_threshold;
}

// 0x009485A0 (:2386)
float multi_core_object_registration::get_state_mask_pack_bias_value(unsigned int mask) const
{
    return get_mask_info(mask)->pack_bias_value;
}

// 0x009485E0 (:2394)
float multi_core_object_registration::get_state_mask_pack_bias(unsigned int mask) const
{
    return get_mask_info(mask)->pack_bias;
}

// |a - b| in double, rounded to float (the asm's andpd with the sign mask)
static float mc_abs_diff(float a, float b)
{
    return (float)fabs((double)a - (double)b);
}

// 0x00948620 (:2400)
// The first of the candidate masks (variable arguments, up to 10, ended by 0xFFFFFFFF; ordered by preference) whose
// pack threshold holds every component of target - sync; 0 when the first mask's epsilon holds them all (no change) or
// none fits. The epsilon comes from the first mask.
unsigned int multi_core_object_registration::get_vector_mask(const vector& target_vec, const vector sync_vec, ...) const
{
    unsigned int candidates[10];
    int num_candidates = 0;
    va_list args;
    va_start(args, sync_vec);
    for (unsigned int mask = va_arg(args, unsigned int); mask != 0xFFFFFFFF; mask = va_arg(args, unsigned int))
    {
        if (num_candidates >= 10)
        {
            va_end(args);
            return 0;
        }
        candidates[num_candidates++] = mask;
    }
    va_end(args);
    if (num_candidates <= 0)
    {
        return 0;
    }
    const float epsilon = get_mask_info(candidates[0])->epsilon;
    const float dx = mc_abs_diff(sync_vec.x, target_vec.x);
    if (epsilon >= dx && epsilon >= mc_abs_diff(sync_vec.y, target_vec.y) && epsilon >= mc_abs_diff(sync_vec.z, target_vec.z))
    {
        return 0;
    }
    for (int i = 0; i < num_candidates; i++)
    {
        const float threshold = get_state_mask_pack_threshold(candidates[i]);
        if (threshold >= dx && threshold >= mc_abs_diff(sync_vec.y, target_vec.y) && threshold >= mc_abs_diff(sync_vec.z, target_vec.z))
        {
            return candidates[i];
        }
    }
    return 0;
}

// 0x00948810 (:2521)
// The orientation delta's angles: 0 within the mask's epsilon, else the compressed mask with the smaller pack threshold
// that holds them, then the other, else the full mask.
unsigned int multi_core_object_registration::get_orient_mask(const matrix& target_matrix, const matrix& sync_matrix, unsigned int mask, unsigned int compressed_mask, unsigned int compressed2_mask) const
{
    const float epsilon = get_mask_info(mask)->epsilon;
    float threshold = 0.0f;
    float threshold2 = 0.0f;
    if (compressed_mask != 0)
    {
        threshold = get_mask_info(compressed_mask)->pack_threshold;
    }
    if (compressed2_mask != 0)
    {
        threshold2 = get_mask_info(compressed2_mask)->pack_threshold;
    }
    unsigned int small_mask = compressed2_mask;
    unsigned int large_mask = compressed_mask;
    float small_threshold = threshold2;
    float large_threshold = threshold;
    if (threshold <= threshold2)
    {
        small_mask = compressed_mask;
        large_mask = compressed2_mask;
        small_threshold = threshold;
        large_threshold = threshold2;
    }
    const matrix delta = target_matrix * sync_matrix.copy_transpose();
    float p;
    float b;
    float h;
    delta.extract_angles(p, b, h);
    const float abs_p = fabsf(p);
    if (abs_p < epsilon && fabsf(b) < epsilon && fabsf(h) < epsilon)
    {
        return 0;
    }
    if (small_mask != 0 && abs_p < small_threshold && fabsf(b) < small_threshold && fabsf(h) < small_threshold)
    {
        return small_mask;
    }
    if (large_mask != 0 && abs_p < large_threshold && fabsf(b) < large_threshold && fabsf(h) < large_threshold)
    {
        return large_mask;
    }
    return mask;
}

// 0x00948A50 (:2586)
unsigned int multi_core_object_registration::get_orient_mask(const quaternion& target_quat, const quaternion& sync_quat, unsigned int mask, unsigned int compressed_mask, unsigned int compressed2_mask) const
{
    matrix target_matrix;
    matrix sync_matrix;
    target_quat.extract_matrix(target_matrix);
    sync_quat.extract_matrix(sync_matrix);
    return get_orient_mask(target_matrix, sync_matrix, mask, compressed_mask, compressed2_mask);
}

// The apply and buffer helpers below compress with the mask's bit count split over the components (a vector's
// num_bits / 3 each, ph num_bits / 2), its pack threshold as the range and its bias.

// 0x00948AA0 (:2608)
bool multi_core_object_registration::apply_delta_compressed_vector(bit_packer& packer, unsigned int mask, vector& result_vec, const vector& ref_vec) const
{
    vec_delta_decompress(packer, result_vec, ref_vec, (int)get_state_mask_num_bits(mask) / 3, get_state_mask_pack_threshold(mask), get_state_mask_pack_bias_value(mask), get_state_mask_pack_bias(mask));
    return true;
}

// 0x00948BA0 (:2635)
bool multi_core_object_registration::apply_delta_compressed_vector2(bit_packer& packer, unsigned int mask, vector2& result_vec2, const vector2& ref_vec2) const
{
    vec2_delta_decompress(packer, result_vec2, ref_vec2, (int)get_state_mask_num_bits(mask) / 2, get_state_mask_pack_threshold(mask), get_state_mask_pack_bias_value(mask), get_state_mask_pack_bias(mask));
    return true;
}

// 0x00948C90 (:2661)
bool multi_core_object_registration::apply_compressed_pbh(bit_packer& packer, unsigned int mask, matrix& result_matrix) const
{
    float p;
    float b;
    float h;
    pbh_decompress(packer, p, b, h, (int)get_state_mask_num_bits(mask) / 3, get_state_mask_pack_threshold(mask), get_state_mask_pack_bias_value(mask), get_state_mask_pack_bias(mask));
    result_matrix.set(p, b, h);
    return true;
}

// 0x00948DC0 (:2686)
bool multi_core_object_registration::apply_compressed_pbh(bit_packer& packer, unsigned int mask, quaternion& result_quat) const
{
    matrix m;
    apply_compressed_pbh(packer, mask, m);
    result_quat.set(m);
    return true;
}

// 0x00948F00 (:2711): the bank is the caller's
bool multi_core_object_registration::apply_compressed_ph(bit_packer& packer, unsigned int mask, float b, matrix& result_matrix) const
{
    float p;
    float h;
    ph_decompress(packer, p, h, (int)get_state_mask_num_bits(mask) / 2, get_state_mask_pack_threshold(mask), get_state_mask_pack_bias_value(mask), get_state_mask_pack_bias(mask));
    result_matrix.set(p, b, h);
    return true;
}

// 0x00949020 (:2739)
bool multi_core_object_registration::apply_compressed_ph(bit_packer& packer, unsigned int mask, float b, quaternion& result_quat) const
{
    float p;
    float h;
    ph_decompress(packer, p, h, (int)get_state_mask_num_bits(mask) / 2, get_state_mask_pack_threshold(mask), get_state_mask_pack_bias_value(mask), get_state_mask_pack_bias(mask));
    const matrix m(p, b, h);
    result_quat.set(m);
    return true;
}

// 0x00949150 (:2769)
bool multi_core_object_registration::apply_delta_compressed_pbh(bit_packer& packer, unsigned int mask, matrix& result_matrix, const matrix& ref_matrix) const
{
    mat_pbh_delta_decompress(packer, result_matrix, ref_matrix, (int)get_state_mask_num_bits(mask) / 3, get_state_mask_pack_threshold(mask), get_state_mask_pack_bias_value(mask), get_state_mask_pack_bias(mask));
    return true;
}

// 0x00949250 (:2795)
bool multi_core_object_registration::apply_delta_compressed_pbh(bit_packer& packer, unsigned int mask, quaternion& result_quat, const quaternion& ref_quat) const
{
    mat_pbh_delta_decompress(packer, result_quat, ref_quat, (int)get_state_mask_num_bits(mask) / 3, get_state_mask_pack_threshold(mask), get_state_mask_pack_bias_value(mask), get_state_mask_pack_bias(mask));
    return true;
}

// 0x00949350 (:2821)
bool multi_core_object_registration::apply_delta_compressed_ph(bit_packer& packer, unsigned int mask, matrix& result_matrix, const matrix& ref_matrix) const
{
    mat_ph_delta_decompress(packer, result_matrix, ref_matrix, (int)get_state_mask_num_bits(mask) / 2, get_state_mask_pack_threshold(mask), get_state_mask_pack_bias_value(mask), get_state_mask_pack_bias(mask));
    return true;
}

// 0x00949440 (:2847)
bool multi_core_object_registration::apply_delta_compressed_ph(bit_packer& packer, unsigned int mask, quaternion& result_quat, const quaternion& ref_quat) const
{
    mat_ph_delta_decompress(packer, result_quat, ref_quat, (int)get_state_mask_num_bits(mask) / 2, get_state_mask_pack_threshold(mask), get_state_mask_pack_bias_value(mask), get_state_mask_pack_bias(mask));
    return true;
}

// The buffer helpers compress the transmitted value and decompress it again into the client's value (what the receiver
// will have), leaving the packer after the compressed bits.

// 0x00949530 (:2873)
bool multi_core_object_registration::buffer_delta_compressed_vector(bit_packer& packer, unsigned int mask, vector& client_vec, const vector& ref_vec, const vector& transmitted_vec) const
{
    vec_delta_compress_decompress(packer, ref_vec, transmitted_vec, client_vec, (int)get_state_mask_num_bits(mask) / 3, get_state_mask_pack_threshold(mask), get_state_mask_pack_bias_value(mask), get_state_mask_pack_bias(mask));
    return true;
}

// 0x009496A0 (:2899)
bool multi_core_object_registration::buffer_delta_compressed_vector2(bit_packer& packer, unsigned int mask, vector2& client_vec2, const vector2& ref_vec2, const vector2& transmitted_vec2) const
{
    vec2_delta_compress_decompress(packer, ref_vec2, transmitted_vec2, client_vec2, (int)get_state_mask_num_bits(mask) / 2, get_state_mask_pack_threshold(mask), get_state_mask_pack_bias_value(mask), get_state_mask_pack_bias(mask));
    return true;
}

// 0x00949790 (:2925)
bool multi_core_object_registration::buffer_delta_compressed_pbh(bit_packer& packer, unsigned int mask, matrix& client_matrix, const matrix& ref_matrix, const matrix& transmitted_matrix) const
{
    mat_pbh_delta_compress_decompress(packer, ref_matrix, transmitted_matrix, client_matrix, (int)get_state_mask_num_bits(mask) / 3, get_state_mask_pack_threshold(mask), get_state_mask_pack_bias_value(mask), get_state_mask_pack_bias(mask));
    return true;
}

// 0x00949900 (:2964)
bool multi_core_object_registration::buffer_delta_compressed_pbh(bit_packer& packer, unsigned int mask, quaternion& client_quat, const quaternion& ref_quat, const quaternion& transmitted_quat) const
{
    mat_pbh_delta_compress_decompress(packer, ref_quat, transmitted_quat, client_quat, (int)get_state_mask_num_bits(mask) / 3, get_state_mask_pack_threshold(mask), get_state_mask_pack_bias_value(mask), get_state_mask_pack_bias(mask));
    return true;
}

// 0x00949A70 (:3003)
bool multi_core_object_registration::buffer_delta_compressed_ph(bit_packer& packer, unsigned int mask, matrix& client_matrix, const matrix& ref_matrix, const matrix& transmitted_matrix) const
{
    mat_ph_delta_compress_decompress(packer, ref_matrix, transmitted_matrix, client_matrix, (int)get_state_mask_num_bits(mask) / 2, get_state_mask_pack_threshold(mask), get_state_mask_pack_bias_value(mask), get_state_mask_pack_bias(mask));
    return true;
}

// 0x00949BD0 (:3042)
bool multi_core_object_registration::buffer_delta_compressed_ph(bit_packer& packer, unsigned int mask, quaternion& client_quat, const quaternion& ref_quat, const quaternion& transmitted_quat) const
{
    mat_ph_delta_compress_decompress(packer, ref_quat, transmitted_quat, client_quat, (int)get_state_mask_num_bits(mask) / 2, get_state_mask_pack_threshold(mask), get_state_mask_pack_bias_value(mask), get_state_mask_pack_bias(mask));
    return true;
}

// 0x00949D30 (:3080)
bool multi_core_object_registration::buffer_compressed_pbh(bit_packer& packer, unsigned int mask, matrix& client_matrix, const matrix& transmitted_matrix) const
{
    const float pack_amount = get_state_mask_pack_threshold(mask);
    const float pack_bias_value = get_state_mask_pack_bias_value(mask);
    const int pack_size = (int)get_state_mask_num_bits(mask) / 3;
    const float pack_bias = get_state_mask_pack_bias(mask);
    const unsigned int start = packer.get_start_bit();
    float p;
    float b;
    float h;
    transmitted_matrix.extract_angles(p, b, h);
    pbh_compress(packer, p, b, h, pack_size, pack_amount, pack_bias_value, pack_bias);
    const unsigned int end = packer.get_start_bit();
    packer.set_start_bit(start);
    pbh_decompress(packer, p, b, h, pack_size, pack_amount, pack_bias_value, pack_bias);
    client_matrix.set(p, b, h);
    packer.set_start_bit(end);
    return true;
}

// 0x00949F10 (:3118)
bool multi_core_object_registration::buffer_compressed_pbh(bit_packer& packer, unsigned int mask, quaternion& client_quat, const quaternion& transmitted_quat) const
{
    matrix transmitted_matrix;
    transmitted_quat.extract_matrix(transmitted_matrix);
    matrix client_matrix;
    buffer_compressed_pbh(packer, mask, client_matrix, transmitted_matrix);
    client_quat.set(client_matrix);
    return true;
}

// 0x0094A110 (:3156): the client keeps the transmitted bank
bool multi_core_object_registration::buffer_compressed_ph(bit_packer& packer, unsigned int mask, matrix& client_matrix, const matrix& transmitted_matrix) const
{
    const float pack_amount = get_state_mask_pack_threshold(mask);
    const float pack_bias_value = get_state_mask_pack_bias_value(mask);
    const float pack_bias = get_state_mask_pack_bias(mask);
    const unsigned int start = packer.get_start_bit();
    const int pack_size = (int)get_state_mask_num_bits(mask) / 2;
    ph_compress(packer, transmitted_matrix, pack_size, pack_amount, pack_bias_value, pack_bias);
    const unsigned int end = packer.get_start_bit();
    float p;
    float b;
    float h;
    transmitted_matrix.extract_angles(p, b, h);
    packer.set_start_bit(start);
    ph_decompress(packer, p, h, pack_size, pack_amount, pack_bias_value, pack_bias);
    client_matrix.set(p, b, h);
    packer.set_start_bit(end);
    return true;
}

// 0x0094A2B0 (:3199)
bool multi_core_object_registration::buffer_compressed_ph(bit_packer& packer, unsigned int mask, quaternion& client_quat, const quaternion& transmitted_quat) const
{
    matrix transmitted_matrix;
    transmitted_quat.extract_matrix(transmitted_matrix);
    matrix client_matrix;
    buffer_compressed_ph(packer, mask, client_matrix, transmitted_matrix);
    client_quat.set(client_matrix);
    return true;
}
