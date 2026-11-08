// Original: volition/rfg/code/multi/multi_core/global/multi_core_delta_compression.cpp
//
// Decompiled whole (multiplayer.md MP5). Each replicated object has a master object (dc_master_object, found through
// the game's dcmo index callbacks) with the circular list of its recent states (dc_state_list, newest first, tagged with
// the master ack id of the frame), and per connection a dc_object that remembers the last state the peer acknowledged
// (the reference an update is delta compressed against). The receiver acknowledges with PT_UPDATE_ACK_LIST.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_core/include/multi_core_delta_compression.h"

#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_session_base.h"
#include "volition/rfg/code/multi/multi_core/include/multi_pool.inl"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/vlib/code/util/hash_table.h"

#include <string.h>

struct network_game_handle;

// ---- file-static globals
// (Base_score_penalty_for_vetoing, 0x02FEB37C, was listed here by the stub: multi_core's only references to that address
// are the end of Object_type_registration_list's loops. It is placed in multi_game_preferences.cpp.)
multi_pool<dc_master_object, 255> Master_object_allocator; // 0x016916D0 (:84)
static multi_pool<dc_state_list, 6375> State_list_allocator; // 0x01691710
static multi_pool<dc_object, 4080> Object_allocator; // 0x01691750
// The game handle to remote object lookup (filled by the object registrations, multi_core_objects.cpp).
static dynamic_uint_hash_table<remote_object*> Game_handle_to_rop_hash; // 0x01691B40
static dynamic_uint_hash_table<remote_object*> Network_handle_to_rop_hash; // 0x01691B88
static dynamic_uint_hash_table<network_game_handle*> Reserved_network_to_game_hash; // 0x01691C60
static dc_master_object* Master_object_lookup_table[255]; // 0x02FE6D88
unsigned short Master_ack_id_number = 1; // 0x0167D838

// ---- port-only helpers: the circular doubly linked list operations rfg.exe expands inline (vlib's list macros). The
// head is the newest element; adding puts the element before the old head and makes it the head.

template<typename T>
static void mc_list_add_head(T*& head, T* item, T* T::*next, T* T::*prev)
{
    if (head == nullptr)
    {
        item->*next = item;
        item->*prev = item;
    }
    else
    {
        (head->*prev)->*next = item;
        item->*prev = head->*prev;
        head->*prev = item;
        item->*next = head;
    }
    head = item;
}

// Removing the head moves it to the next element (or empties the list); the element's links are cleared. The
// dc_state_list form checks its neighbours for null (as rfg.exe does there), the dc_object form does not.
template<typename T>
static void mc_list_remove(T*& head, T* item, T* T::*next, T* T::*prev, bool check_links)
{
    if (head == item)
    {
        if (item->*next == item)
        {
            item->*next = nullptr;
            item->*prev = nullptr;
            head = nullptr;
            return;
        }
        head = item->*next;
    }
    if (!check_links || item->*prev != nullptr)
    {
        (item->*prev)->*next = item->*next;
    }
    if (!check_links || item->*next != nullptr)
    {
        (item->*next)->*prev = item->*prev;
    }
    item->*next = nullptr;
    item->*prev = nullptr;
}

static dc_master_object* allocate_dc_master_object(); // 0x0098F690 (:133)
static void free_dc_master_object(dc_master_object* dcmo); // 0x009A8D10 (:152)

// 0x00947310 (:105)
void multi_core_dc_register_get_dcmo_index_from_game_handle(unsigned char (*cb)(unsigned int))
{
    Get_dcmo_index_from_game_handle_cb = cb;
}

// 0x00947320 (:109)
void multi_core_dc_register_set_dcmo_index_for_game_handle(void (*cb)(unsigned int, unsigned char))
{
    Set_dcmo_index_for_game_handle_cb = cb;
}

// 0x0098F690 (:133): the first free slot of the lookup table is the object's dcmo index (255 slots; slot 255 would be
// the "none" index 0xFF and is never used)
static dc_master_object* allocate_dc_master_object()
{
    int index = 0;
    while (Master_object_lookup_table[index] != nullptr)
    {
        index++;
        if (index > 254)
        {
            return nullptr;
        }
    }
    dc_master_object* dcmo = Master_object_allocator.alloc(0);
    Master_object_lookup_table[index] = dcmo;
    if (dcmo == nullptr)
    {
        return nullptr;
    }
    dcmo->m_dcmo_index = (unsigned char)index;
    return dcmo;
}

// 0x009A8D10 (:152)
// LTCG: dcmo in ECX (the port has no register convention).
static void free_dc_master_object(dc_master_object* dcmo)
{
    if (dcmo->m_dcmo_index != 0xFF)
    {
        Master_object_lookup_table[dcmo->m_dcmo_index] = nullptr;
        Master_object_allocator.free(dcmo);
    }
}

// 0x00947330 (:195): the links are left as they are
dc_object::dc_object()
{
    m_dirty = false;
    m_network_handle = 0xFFFF;
    m_current_reference_ack_id = 0;
    m_last_received_ack_id = 0;
    m_first_clean_ack_id = 0;
    m_validated_clean_ack_id = 0;
}

// 0x00947350 (:207)
dc_master_object::dc_master_object()
{
    m_network_handle = 0xFFFF;
    m_state_list = nullptr;
    m_current_state_ack_id = 0;
    m_first_clean_ack_id = 0;
    m_dirty_check = 0;
    m_dcmo_index = 0xFF;
}

// 0x00947380 (:218)
dc_state_list::dc_state_list()
{
    m_ack_id = 0;
    m_reference_ackid = 0;
    m_size = 0;
    m_network_handle = 0xFFFF;
    m_data = nullptr;
}

// 0x009D8190 (:227): 255 master objects, 6375 states, 4080 per connection objects
void multi_core_dc_init()
{
    Master_object_allocator.init(MP_allocators_mempool);
    State_list_allocator.init(MP_allocators_mempool);
    Object_allocator.init(MP_allocators_mempool);
    memset(Master_object_lookup_table, 0, sizeof(Master_object_lookup_table));
    multi_core_dc_reset();
}

// 0x009B5600 (:248)
void multi_core_dc_reset()
{
    Master_object_allocator.clear();
    for (dc_state_list* dcsl = State_list_allocator.get_first(0); dcsl != nullptr;)
    {
        dcsl = State_list_allocator.free(dcsl);
    }
    for (dc_object* dco = Object_allocator.get_first(0); dco != nullptr;)
    {
        dco = Object_allocator.free(dco);
    }
    memset(Master_object_lookup_table, 0, sizeof(Master_object_lookup_table));
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    if (sp != nullptr)
    {
        multi_connection* cp = sp->get_connection_list();
        do
        {
            if (cp == nullptr)
            {
                break;
            }
            cp->dc_objects = nullptr;
            cp = cp->next;
        } while (cp != sp->get_connection_list());
    }
    for (int i = 0; i < 12; i++)
    {
        if (Object_type_registration_list[i] != nullptr)
        {
            Object_type_registration_list[i]->reset_state_data();
        }
    }
}

// 0x009D8210 (:276): a server tracks the object for every connection, a client for its host
dc_master_object* multi_core_dc_create_master_object(unsigned short net_handle)
{
    dc_master_object* dcmo = allocate_dc_master_object();
    if (dcmo == nullptr)
    {
        return nullptr;
    }
    dcmo->m_network_handle = net_handle;
    dcmo->m_current_state_ack_id = 0;
    dcmo->m_objects = nullptr;
    dcmo->m_state_list = nullptr;
    if (Multiplayer && Server)
    {
        multi_session* sp = multi_session_get_by_type(ST_GAME);
        // Original: the session is not checked for null.
        multi_connection* cp = sp->get_connection_list();
        do
        {
            if (cp == nullptr)
            {
                break;
            }
            multi_core_dc_add_object_to_connection(dcmo, cp);
            cp = cp->next;
        } while (cp != sp->get_connection_list());
    }
    else
    {
        multi_core_dc_add_object_to_connection(dcmo, multi_session_get_by_type(ST_GAME)->get_host_connection());
    }
    Set_dcmo_index_for_game_handle_cb(multi_core_game_handle_from_network_handle(net_handle, false), dcmo->m_dcmo_index);
    return dcmo;
}

// 0x009E3610 (:308): every stored state goes back to the pool (its data through the registration while the remote object
// is still in the game handle lookup), then the per connection objects
void multi_core_dc_destroy_master_object(remote_object* rop)
{
    const unsigned char index = Get_dcmo_index_from_game_handle_cb(rop->m_game_handle);
    if (index == 0xFF)
    {
        return;
    }
    dc_master_object* dcmo = Master_object_lookup_table[index];
    if (dcmo == nullptr)
    {
        return;
    }
    dc_state_list* dcsl = dcmo->m_state_list;
    while (dcsl != nullptr)
    {
        dc_state_list* const next = dcsl->next != dcmo->m_state_list ? dcsl->next : nullptr;
        mc_list_remove(dcmo->m_state_list, dcsl, &dc_state_list::next, &dc_state_list::prev, true);
        remote_object* owner = Game_handle_to_rop_hash.get(rop->m_game_handle);
        if (owner != nullptr)
        {
            Object_type_registration_list[owner->current_type]->free_state(dcsl);
        }
        State_list_allocator.free(dcsl);
        dcsl = next;
    }
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    if (sp != nullptr)
    {
        multi_connection* cp = sp->get_connection_list();
        do
        {
            if (cp == nullptr)
            {
                break;
            }
            multi_core_dc_remove_object_from_connection(dcmo, cp);
            cp = cp->next;
        } while (cp != sp->get_connection_list());
    }
    Set_dcmo_index_for_game_handle_cb(rop->m_game_handle, 0xFF);
    free_dc_master_object(dcmo);
}

// 0x009473A0 (:338)
dc_master_object* get_master_object_from_remote_object_ptr(const remote_object* rop)
{
    const unsigned char index = Get_dcmo_index_from_game_handle_cb(rop->m_game_handle);
    if (index == 0xFF)
    {
        return nullptr;
    }
    return Master_object_lookup_table[index];
}

// 0x0098F7E0 (:350)
void multi_core_dc_add_object_to_connection(dc_master_object* p_master_object, multi_connection* cp)
{
    if (cp == nullptr)
    {
        return;
    }
    // Original: the allocation is not checked for null.
    dc_object* dco = Object_allocator.alloc(0);
    dco->m_network_handle = p_master_object->m_network_handle;
    dco->m_dirty = false;
    dco->m_current_reference_ack_id = 0;
    dco->m_last_received_ack_id = 0;
    dco->m_cp = cp;
    mc_list_add_head(cp->dc_objects, dco, &dc_object::connection_next, &dc_object::connection_prev);
    mc_list_add_head(p_master_object->m_objects, dco, &dc_object::master_next, &dc_object::master_prev);
}

// 0x009A8D40 (:368): every object of the connection with the master's network handle
void multi_core_dc_remove_object_from_connection(dc_master_object* p_master_object, multi_connection* cp)
{
    dc_object* dco = cp->dc_objects;
    while (dco != nullptr)
    {
        dc_object* const next = dco->connection_next != cp->dc_objects ? dco->connection_next : nullptr;
        if (dco->m_network_handle == p_master_object->m_network_handle)
        {
            mc_list_remove(p_master_object->m_objects, dco, &dc_object::master_next, &dc_object::master_prev, false);
            mc_list_remove(cp->dc_objects, dco, &dc_object::connection_next, &dc_object::connection_prev, false);
            Object_allocator.free(dco);
        }
        dco = next;
    }
}

// 0x009473D0 (:380)
void multi_core_dc_store_state_for_object(const remote_object* rop, unsigned short ack_id, dc_state_list* dcsl)
{
    if (dcsl == nullptr)
    {
        return;
    }
    dc_master_object* dcmo = get_master_object_from_remote_object_ptr(rop);
    if (dcmo == nullptr)
    {
        return;
    }
    dcsl->m_ack_id = ack_id;
    dcsl->m_network_handle = rop->m_network_handle;
    mc_list_add_head(dcmo->m_state_list, dcsl, &dc_state_list::next, &dc_state_list::prev);
}

// 0x009A8E10 (:395): the object's current state through its registration; an empty state is not kept
void multi_core_dc_store_state_for_object(const remote_object* rop, unsigned short p_ack_id)
{
    dc_master_object* dcmo = get_master_object_from_remote_object_ptr(rop);
    if (dcmo == nullptr)
    {
        return;
    }
    dc_state_list* dcsl = State_list_allocator.alloc(0);
    if (dcsl == nullptr)
    {
        return;
    }
    dcsl->m_ack_id = p_ack_id;
    dcsl->m_network_handle = rop->m_network_handle;
    dcsl->m_data = nullptr;
    dcsl->m_reference_ackid = 0;
    dcsl->m_size = 0;
    Object_type_registration_list[rop->current_type]->get_state(rop->m_game_handle, dcsl);
    if (dcsl->m_size == 0)
    {
        State_list_allocator.free(dcsl);
        return;
    }
    mc_list_add_head(dcmo->m_state_list, dcsl, &dc_state_list::next, &dc_state_list::prev);
}

// 0x009A8EE0 (:424): the state of this frame (the master ack id); once per ack id the object's dirty check against the
// previous current state records the first ack id since which it is clean
void multi_core_dc_store_state_for_object_for_this_frame(const remote_object* rop)
{
    dc_master_object* dcmo = get_master_object_from_remote_object_ptr(rop);
    if (dcmo == nullptr)
    {
        return;
    }
    dc_state_list* dcsl = State_list_allocator.alloc(0);
    if (dcsl == nullptr)
    {
        return;
    }
    // Original: the state's fields are not cleared before get_state here (the pool's objects keep their last values).
    Object_type_registration_list[rop->current_type]->get_state(rop->m_game_handle, dcsl);
    if (dcsl->m_size == 0)
    {
        State_list_allocator.free(dcsl);
        return;
    }
    const unsigned short previous_ack_id = dcmo->m_current_state_ack_id;
    dcmo->m_current_state_dcsl = dcsl;
    dcmo->m_current_state_ack_id = Master_ack_id_number;
    if (dcmo->m_dirty_check != Master_ack_id_number)
    {
        dcmo->m_dirty_check = Master_ack_id_number;
        dc_state_list* ref_state = multi_core_dc_get_state_for_object_for_ackid(dcmo, previous_ack_id);
        if (multi_core_object_check_dirty(rop, ref_state, dcsl))
        {
            dcmo->m_first_clean_ack_id = 0;
        }
        else if (dcmo->m_first_clean_ack_id == 0)
        {
            dcmo->m_first_clean_ack_id = Master_ack_id_number;
        }
    }
    dcsl->m_ack_id = Master_ack_id_number;
    dcsl->m_network_handle = rop->m_network_handle;
    dcsl->m_reference_ackid = 0;
    mc_list_add_head(dcmo->m_state_list, dcsl, &dc_state_list::next, &dc_state_list::prev);
}

// 0x00947440 (:476)
dc_state_list* multi_core_dc_get_state_for_object_for_ackid(dc_master_object* p_master_object, unsigned long p_ack_id)
{
    dc_state_list* dcsl = p_master_object->m_state_list;
    while (dcsl != nullptr)
    {
        if (dcsl->m_ack_id == p_ack_id)
        {
            return dcsl;
        }
        dcsl = dcsl->next;
        if (dcsl == p_master_object->m_state_list)
        {
            return nullptr;
        }
    }
    return nullptr;
}

// 0x00947470 (:491): a client received an update (its ack goes out with the next ack list)
void multi_core_dc_update_ackids_for_object(multi_connection* cp, unsigned short net_handle, unsigned short p_current_ack_id, unsigned short p_reference_ack_id)
{
    dc_object* dco = cp->dc_objects;
    if (dco == nullptr)
    {
        return;
    }
    while (dco->m_network_handle != net_handle)
    {
        dco = dco->connection_next;
        if (dco == cp->dc_objects || dco == nullptr)
        {
            return;
        }
    }
    dco->m_last_received_ack_id = p_current_ack_id;
    dco->m_dirty = true;
    if (dco->m_current_reference_ack_id < p_reference_ack_id)
    {
        dco->m_current_reference_ack_id = p_reference_ack_id;
    }
}

// 0x009B5700 (:517)
// PT_UPDATE_ACK_LIST to the host: the count (32 bits), then per dirty object its network handle and the last received
// ack id (16 bits each), all big endian.
void multi_core_dc_send_acks()
{
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    if (sp == nullptr)
    {
        return;
    }
    multi_connection* cp = sp->get_host_connection();
    if (cp == nullptr || (cp->connection_status != CS_CONNECTED && cp->connection_status != CS_CONNECTED_GAMEPLAY))
    {
        return;
    }
    dc_object* dco = cp->dc_objects;
    if (dco == nullptr)
    {
        return;
    }
    // Magic number: 509 entries fit the stack array; rfg.exe does not check the count against it.
    struct ack_entry
    {
        unsigned short net_handle;
        unsigned int ack_id;
    };
    ack_entry acks[509];
    unsigned int count = 0;
    do
    {
        if (dco->m_dirty)
        {
            acks[count].ack_id = dco->m_last_received_ack_id;
            acks[count].net_handle = dco->m_network_handle;
            count++;
            dco->m_dirty = false;
        }
        dco = dco->connection_next;
    } while (dco != cp->dc_objects && dco != nullptr);
    if (count == 0)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_UPDATE_ACK_LIST;
    unsigned int size = Multi_packets[PT_UPDATE_ACK_LIST].group_id != 0 ? 5 : 3;
    packet[size] = (unsigned char)(count >> 24);
    packet[size + 1] = (unsigned char)(count >> 16);
    packet[size + 2] = (unsigned char)(count >> 8);
    packet[size + 3] = (unsigned char)count;
    size += 4;
    for (unsigned int i = 0; i < count; i++)
    {
        packet[size & 0xFFFF] = (unsigned char)(acks[i].net_handle >> 8);
        packet[(size & 0xFFFF) + 1] = (unsigned char)acks[i].net_handle;
        const unsigned short ack_id = (unsigned short)acks[i].ack_id;
        packet[(size + 2) & 0xFFFF] = (unsigned char)(ack_id >> 8);
        packet[((size + 2) & 0xFFFF) + 1] = (unsigned char)ack_id;
        size += 4;
    }
    const unsigned short payload = (unsigned short)(size - 3);
    packet[1] = (unsigned char)(payload >> 8);
    packet[2] = (unsigned char)payload;
    multi_io_send(cp, packet, size & 0xFFFF);
}

// 0x009D82C0 (:562): the server takes each acknowledged ack id as the connection's reference state when that state is
// still stored (or when it is 0: no reference)
void multi_core_dc_process_ack_list(unsigned char* packet_data, const sender& from)
{
    multi_connection* cp = from.connection;
    if (cp == nullptr)
    {
        return;
    }
    unsigned int count = ((unsigned int)packet_data[0] << 24) | ((unsigned int)packet_data[1] << 16) | ((unsigned int)packet_data[2] << 8) | packet_data[3];
    unsigned char* entry = packet_data + 4;
    for (; count != 0; count--, entry += 4)
    {
        const unsigned short net_handle = (unsigned short)((entry[0] << 8) | entry[1]);
        const unsigned short ack_id = (unsigned short)((entry[2] << 8) | entry[3]);
        dc_object* dco = cp->dc_objects;
        if (dco == nullptr)
        {
            continue;
        }
        bool found = true;
        while (dco->m_network_handle != net_handle)
        {
            dco = dco->connection_next;
            if (dco == cp->dc_objects || dco == nullptr)
            {
                found = false;
                break;
            }
        }
        if (!found)
        {
            continue;
        }
        remote_object* rop = multi_core_get_remote_object_from_network_handle(net_handle);
        if (rop == nullptr)
        {
            continue;
        }
        dc_master_object* dcmo = get_master_object_from_remote_object_ptr(rop);
        if (dcmo == nullptr)
        {
            continue;
        }
        if (multi_core_dc_get_state_for_object_for_ackid(dcmo, ack_id) != nullptr || ack_id == 0)
        {
            dco->m_current_reference_ack_id = ack_id;
        }
    }
}

// 0x009F70F0 (:620): when the pool is empty, old states are flushed once
dc_state_list* multi_core_dc_alloc_state_list()
{
    dc_state_list* dcsl = State_list_allocator.alloc(0);
    if (dcsl == nullptr)
    {
        multi_core_dc_flush_old_object_states();
        dcsl = State_list_allocator.alloc(0);
    }
    return dcsl;
}

// 0x009A8FF0 (:634)
void multi_core_dc_free_state_list(dc_state_list* dcsl)
{
    State_list_allocator.free(dcsl);
}

// 0x009E3780 (:640)
// Each master object with more than 24 states frees up to 5 of its oldest states that no connection uses as its
// reference or last received state (the walk goes from the oldest towards the head).
void multi_core_dc_flush_old_object_states()
{
    for (dc_master_object* dcmo = Master_object_allocator.get_first(0); dcmo != nullptr; dcmo = Master_object_allocator.get_next(dcmo))
    {
        dc_state_list* const head = dcmo->m_state_list;
        if (head == nullptr)
        {
            continue;
        }
        unsigned int num_states = 0;
        dc_state_list* dcsl = head;
        do
        {
            dcsl = dcsl->next;
            num_states++;
        } while (dcsl != head && dcsl != nullptr);
        if (num_states <= 24) // Magic number: the states kept per object before flushing
        {
            continue;
        }
        unsigned int num_freed = 0;
        dc_state_list* candidate = head->prev;
        while (candidate != nullptr && num_freed < 5) // Magic number: the states freed per object and flush
        {
            dc_state_list* const cur = candidate;
            const unsigned int game_handle = multi_core_game_handle_from_network_handle(cur->m_network_handle, true);
            dc_object* in_use = dcmo->m_objects;
            if (in_use != nullptr)
            {
                do
                {
                    if (in_use->m_current_reference_ack_id == cur->m_ack_id || in_use->m_last_received_ack_id == cur->m_ack_id)
                    {
                        break;
                    }
                    in_use = in_use->master_next;
                    if (in_use == dcmo->m_objects)
                    {
                        in_use = nullptr;
                        break;
                    }
                } while (in_use != nullptr);
            }
            candidate = dcmo->m_state_list == cur ? nullptr : cur->prev;
            if (in_use != nullptr)
            {
                continue;
            }
            remote_object* owner = Game_handle_to_rop_hash.get(game_handle);
            if (owner != nullptr)
            {
                Object_type_registration_list[owner->current_type]->free_state(cur);
            }
            mc_list_remove(dcmo->m_state_list, cur, &dc_state_list::next, &dc_state_list::prev, true);
            State_list_allocator.free(cur);
            num_freed++;
        }
    }
}

// 0x009474C0 (:701): the server's reference state is gone; the client will send a full update request with its next ack
void multi_core_dc_mark_reference_state_not_found(multi_connection* cp, unsigned short net_handle)
{
    dc_object* dco = cp->dc_objects;
    if (dco == nullptr)
    {
        return;
    }
    while (dco->m_network_handle != net_handle)
    {
        dco = dco->connection_next;
        if (dco == cp->dc_objects || dco == nullptr)
        {
            return;
        }
    }
    dco->m_dirty = true;
    dco->m_last_received_ack_id = 0;
}

// 0x00947500 (:721): 0 is skipped (it means "no reference")
void multi_core_dc_update_ack_id()
{
    Master_ack_id_number++;
    if (Master_ack_id_number == 0)
    {
        Master_ack_id_number = 1;
    }
}

// 0x00947520 (:733)
unsigned short multi_core_dc_get_master_ack_id()
{
    return Master_ack_id_number;
}
