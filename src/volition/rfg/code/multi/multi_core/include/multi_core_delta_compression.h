// Original: volition/rfg/code/multi/multi_core/include/multi_core_delta_compression.h
// Implementation: volition/rfg/code/multi/multi_core/global/multi_core_delta_compression.cpp
//
// Delta compression of object states: per object a list of the states sent to each connection, so that an update is
// encoded against the last state the client acknowledged.
#pragma once


class dc_state_list;
class multi_connection;
struct remote_object;
struct sender;

// size 0x20
class dc_object
{
public:
    dc_object(); // 0x00947330 (:195)

    unsigned short m_network_handle; // 0x00
    unsigned short m_current_reference_ack_id; // 0x02
    unsigned short m_last_received_ack_id; // 0x04
    unsigned short m_first_clean_ack_id; // 0x06
    unsigned short m_validated_clean_ack_id; // 0x08
    bool m_dirty; // 0x0A
    multi_connection* m_cp; // 0x0C
    dc_object* connection_next; // 0x10
    dc_object* connection_prev; // 0x14
    dc_object* master_next; // 0x18
    dc_object* master_prev; // 0x1C
};

// size 0x1C
class dc_master_object
{
public:
    dc_master_object(); // 0x00947350 (:207)

    unsigned short m_network_handle; // 0x00
    dc_state_list* m_state_list; // 0x04
    unsigned short m_current_state_ack_id; // 0x08
    dc_state_list* m_current_state_dcsl; // 0x0C
    dc_object* m_objects; // 0x10
    unsigned short m_first_clean_ack_id; // 0x14
    unsigned short m_dirty_check; // 0x16
    unsigned char m_dcmo_index; // 0x18
};

// size 0x18
class dc_state_list
{
public:
    dc_state_list(); // 0x00947380 (:218)

    unsigned short m_ack_id; // 0x00
    unsigned short m_reference_ackid; // 0x02
    unsigned short m_network_handle; // 0x04
    unsigned long m_size; // 0x08
    unsigned char* m_data; // 0x0C
    dc_state_list* prev; // 0x10
    dc_state_list* next; // 0x14
};

template<typename T, int N>
class multi_pool;

extern multi_pool<dc_master_object, 255> Master_object_allocator; // 0x016916D0, defined in multi_core_delta_compression.cpp (:84)

void multi_core_dc_register_get_dcmo_index_from_game_handle(unsigned char (*cb)(unsigned int)); // 0x00947310 (:105)
void multi_core_dc_register_set_dcmo_index_for_game_handle(void (*cb)(unsigned int, unsigned char)); // 0x00947320 (:109)
void multi_core_dc_init(); // 0x009D8190 (:227)
void multi_core_dc_reset(); // 0x009B5600 (:248)
dc_master_object* multi_core_dc_create_master_object(unsigned short net_handle); // 0x009D8210 (:276)
void multi_core_dc_destroy_master_object(remote_object* rop); // 0x009E3610 (:308)
dc_master_object* get_master_object_from_remote_object_ptr(const remote_object* rop); // 0x009473A0 (:338)
void multi_core_dc_add_object_to_connection(dc_master_object* p_master_object, multi_connection* cp); // 0x0098F7E0 (:350)
void multi_core_dc_remove_object_from_connection(dc_master_object* p_master_object, multi_connection* cp); // 0x009A8D40 (:368)
void multi_core_dc_store_state_for_object(const remote_object* rop, unsigned short ack_id, dc_state_list* dcsl); // 0x009473D0 (:380)
void multi_core_dc_store_state_for_object(const remote_object* rop, unsigned short p_ack_id); // 0x009A8E10 (:395)
void multi_core_dc_store_state_for_object_for_this_frame(const remote_object* rop); // 0x009A8EE0 (:424)
dc_state_list* multi_core_dc_get_state_for_object_for_ackid(dc_master_object* p_master_object, unsigned long p_ack_id); // 0x00947440 (:476)
void multi_core_dc_update_ackids_for_object(multi_connection* cp, unsigned short net_handle, unsigned short p_current_ack_id, unsigned short p_reference_ack_id); // 0x00947470 (:491)
void multi_core_dc_send_acks(); // 0x009B5700 (:517)
void multi_core_dc_process_ack_list(unsigned char* packet_data, const sender& from); // 0x009D82C0 (:562)
dc_state_list* multi_core_dc_alloc_state_list(); // 0x009F70F0 (:620)
void multi_core_dc_free_state_list(dc_state_list* dcsl); // 0x009A8FF0 (:634)
void multi_core_dc_flush_old_object_states(); // 0x009E3780 (:640)
void multi_core_dc_mark_reference_state_not_found(multi_connection* cp, unsigned short net_handle); // 0x009474C0 (:701)
void multi_core_dc_update_ack_id(); // 0x00947500 (:721)
unsigned short multi_core_dc_get_master_ack_id(); // 0x00947520 (:733)
