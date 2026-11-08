// Original: volition/rfg/code/multi/multi_game/multi_game_object_vehicle_walker.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_object_vehicle_walker.cpp
//
// Networked walkers: the walker state bundle and registration, and the steering correction of remote walkers.
#pragma once

#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_types.h"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_state.h"

class dc_state_list;
class multi_connection;
class object;
class vector;
class vehicle;
class walker;

// size 0x140
struct walker_data_bundle : public object_data_bundle<unsigned short>, public multi_walker_state
{
};

// size 0x814
class walker_multi_registration : public multi_core_object_registration
{
    virtual void create(unsigned int game_handle) const; // vtbl+0x04 0x0099F860 (:521)
    virtual void send_create_packet(const remote_object* rop, unsigned char* packet_data, unsigned short& packet_size, multi_connection* dest_conn) const; // vtbl+0x08 0x00980950 (:540)
    virtual void process_create_packet(unsigned int& game_handle, unsigned short net_handle, int& packet_offset, unsigned char* packet_data) const; // vtbl+0x0C 0x0099F8E0 (:592)
    virtual void send_destroy_packet(unsigned int game_handle, unsigned char* packet_data, unsigned short& packet_size) const; // vtbl+0x10 0x0095E8D0 (:644)
    virtual void process_destroy_packet(unsigned int game_handle, int& packet_offset, unsigned char* packet_data) const; // vtbl+0x14 0x00980C90 (:657)
    virtual void process_object_free(object* obj) const; // vtbl+0x18 0x009F6800 (:669)
    virtual void post_update_cleanup() const; // vtbl+0x24 0x0095E8E0 (:685)
    virtual void move(unsigned int game_handle) const; // vtbl+0x28 0x0099FD70 (:697)
    virtual void get_state(unsigned int game_handle, dc_state_list* dcsl) const; // vtbl+0x2C 0x009F6850 (:730)
    virtual void free_state(dc_state_list* dcsl) const; // vtbl+0x30 0x009B1310 (:849)
    virtual bool check_object_is_dirty(dc_state_list* ref_state, dc_state_list* cur_state, const remote_object* rop) const; // vtbl+0x54 0x00980CD0 (:857)
    virtual bool check_full_update(dc_state_list* ref_state, multi_connection* cp) const; // vtbl+0x58 0x0095F210 (:1599)
    virtual void get_compressed_state(dc_state_list* current_state, dc_state_list* reference_state, dc_state_list* compressed_state, multi_connection* cp, multi_core_state_priority priority, unsigned short mask_bits) const; // vtbl+0x34 0x0095E910 (:877)
    virtual bool apply_compressed_state(unsigned int game_handle, dc_state_list* reference_state, dc_state_list* compressed_state, dc_state_list** resultant_state, unsigned int network_time) const; // vtbl+0x38 0x009FAF60 (:1152)
    virtual unsigned short query_possible_mask_bits(dc_state_list* current_state, dc_state_list* reference_state, multi_connection* cp) const; // vtbl+0x3C 0x009CD8F0 (:480)
    virtual void query_space_for_priority(const remote_object* rop, dc_state_list* current_state, dc_state_list* reference_state, unsigned short possible_mask_bits, multi_connection* cp, query_size_results* qs_results) const; // vtbl+0x40 0x009CD9B0 (:1415)
    virtual void query_space_for_mask(query_size_results* qs_results, void* additional_data) const; // vtbl+0x44 0x0095F080 (:1317)
    virtual void notify_transmitted_updated(const remote_object* rop, dc_state_list* current_state, multi_connection* recipient, unsigned short possible_mask_bits, unsigned short mask_bits_transmitted) const; // vtbl+0x48 0x0095F180 (:1577)
    virtual void reset_state_data() const; // vtbl+0x4C 0x009CDF10 (:1610)
    virtual void free_mp_state_data() const; // vtbl+0x50 0x009B1330 (:1615)

};

void get_steering_correction(vector& steering_correction, const walker* wp, const vector& pos_diff); // 0x0095E4F0 (:370)
void multi_game_object_vehicle_walker_init(bool clear_allocators); // 0x0095F230 (:1844)
bool multi_game_object_vehicle_walker_is_client_authoritative(const vehicle* vp); // 0x0099FE10 (:2000)
void multi_game_object_vehicle_walker_apply_sim_state(const multi_walker_state* walker_state, walker* vwp); // 0x00980D40 (:2020)
void multi_game_walker_predict_collision(walker* wp); // 0x009B13B0 (:2204)
