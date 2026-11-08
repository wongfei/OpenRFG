// Original: volition/rfg/code/multi/multi_game/multi_game_object_items.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_object_items.cpp
//
// Networked items: the item state bundle and registration.
#pragma once

#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_types.h"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_state.h"

class dc_state_list;
class item;
class multi_connection;
class object;

// size 0x58
struct item_data_bundle : public object_data_bundle<unsigned char>, public multi_item_state
{
    item_data_bundle(); // 0x0096BCF0

    float pickup_value; // 0x54
};

// size 0x814
class item_multi_registration : public multi_core_object_registration
{
    virtual void create(unsigned int game_handle) const; // vtbl+0x04 0x00959610 (:387)
    virtual void send_create_packet(const remote_object* rop, unsigned char* packet_data, unsigned short& packet_size, multi_connection* dest_conn) const; // vtbl+0x08 0x0097D6E0 (:405)
    virtual void process_create_packet(unsigned int& game_handle, unsigned short net_handle, int& packet_offset, unsigned char* packet_data) const; // vtbl+0x0C 0x0099B110 (:499)
    virtual void send_destroy_packet(unsigned int game_handle, unsigned char* packet_data, unsigned short& packet_size) const; // vtbl+0x10 0x00959630 (:607)
    virtual void process_destroy_packet(unsigned int game_handle, int& packet_offset, unsigned char* packet_data) const; // vtbl+0x14 0x0097DAA0 (:617)
    virtual void process_object_free(object* obj) const; // vtbl+0x18 0x009F48F0 (:625)
    virtual void post_update_cleanup() const; // vtbl+0x24 0x00959640 (:673)
    virtual void move(unsigned int game_handle) const; // vtbl+0x28 0x0099B590 (:682)
    virtual void get_state(unsigned int game_handle, dc_state_list* dcsl) const; // vtbl+0x2C 0x009F4940 (:775)
    virtual void free_state(dc_state_list* dcsl) const; // vtbl+0x30 0x009B0B40 (:813)
    virtual bool check_object_is_dirty(dc_state_list* ref_state, dc_state_list* cur_state, const remote_object* rop) const; // vtbl+0x54 0x0097DAE0 (:821)
    virtual bool check_full_update(dc_state_list* ref_state, multi_connection* cp) const; // vtbl+0x58 0x00959E50 (:1370)
    virtual void get_compressed_state(dc_state_list* current_state, dc_state_list* reference_state, dc_state_list* compressed_state, multi_connection* cp, multi_core_state_priority priority, unsigned short mask_bits) const; // vtbl+0x34 0x00959690 (:840)
    virtual bool apply_compressed_state(unsigned int game_handle, dc_state_list* reference_state, dc_state_list* compressed_state, dc_state_list** resultant_state, unsigned int network_time) const; // vtbl+0x38 0x009F8A70 (:1023)
    virtual unsigned short query_possible_mask_bits(dc_state_list* current_state, dc_state_list* reference_state, multi_connection* cp) const; // vtbl+0x3C 0x009595B0 (:362)
    virtual void query_space_for_priority(const remote_object* rop, dc_state_list* current_state, dc_state_list* reference_state, unsigned short possible_mask_bits, multi_connection* cp, query_size_results* qs_results) const; // vtbl+0x40 0x009C76C0 (:1228)
    virtual void query_space_for_mask(query_size_results* qs_results, void* additional_data) const; // vtbl+0x44 0x00959AB0 (:1162)
    virtual void notify_transmitted_updated(const remote_object* rop, dc_state_list* current_state, multi_connection* recipient, unsigned short possible_mask_bits, unsigned short mask_bits_transmitted) const; // vtbl+0x48 0x00959DC0 (:1347)
    virtual void reset_state_data() const; // vtbl+0x4C 0x009C7AF0 (:1382)
    virtual void free_mp_state_data() const; // vtbl+0x50 0x009B0B60 (:1387)
public:
    item_multi_registration(); // 0x0096BC40
    virtual ~item_multi_registration(); // vtbl+0x00 0x0096BD50

};

void multi_game_object_items_init(bool clear_allocators); // 0x009DF700 (:1410)
void multi_game_object_item_cleanup(item* obj_item); // 0x009B0BD0 (:1533)

// 0x0096BCF0: nothing to set
inline item_data_bundle::item_data_bundle()
{
}
