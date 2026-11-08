// Original: volition/rfg/code/multi/multi_game/multi_game_object_turret.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_object_turret.cpp
//
// Networked turrets: the turret state bundle and registration (prioritized by distance and direction to the player).
#pragma once

#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_types.h"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_state.h"

class dc_state_list;
class multi_connection;
class object;
class turret;
class vector;

// size 0x68
struct turret_data_bundle : public object_data_bundle<unsigned char>, public multi_turret_state
{
};

// size 0x814
class turret_multi_registration : public multi_core_object_registration
{
public:
    virtual void create(unsigned int game_handle) const; // vtbl+0x04 0x0099D1E0 (:332)
    virtual void send_create_packet(const remote_object* rop, unsigned char* packet_data, unsigned short& packet_size, multi_connection* dest_conn) const; // vtbl+0x08 0x009CCE20 (:363)
    virtual void process_create_packet(unsigned int& game_handle, unsigned short net_handle, int& packet_offset, unsigned char* packet_data) const; // vtbl+0x0C 0x009CCFB0 (:385)
    virtual void send_destroy_packet(unsigned int game_handle, unsigned char* packet_data, unsigned short& packet_size) const; // vtbl+0x10 0x0097F870 (:433)
    virtual void process_destroy_packet(unsigned int game_handle, int& packet_offset, unsigned char* packet_data) const; // vtbl+0x14 0x0097FB00 (:444)
    virtual void process_object_free(object* obj) const; // vtbl+0x18 0x009F60D0 (:348)
    virtual void post_update_cleanup() const; // vtbl+0x24 0x0095DDA0 (:940)
    virtual void move(unsigned int game_handle) const; // vtbl+0x28 0x0099D260 (:945)
    virtual void get_state(unsigned int game_handle, dc_state_list* dcsl) const; // vtbl+0x2C 0x009F6120 (:619)
    virtual void free_state(dc_state_list* dcsl) const; // vtbl+0x30 0x009B1080 (:681)
    virtual bool check_object_is_dirty(dc_state_list* ref_state, dc_state_list* cur_state, const remote_object* rop) const; // vtbl+0x54 0x00980020 (:689)
    virtual bool check_full_update(dc_state_list* ref_state, multi_connection* cp) const; // vtbl+0x58 0x0095DD80 (:932)
    virtual void get_compressed_state(dc_state_list* current_state, dc_state_list* reference_state, dc_state_list* compressed_state, multi_connection* cp, multi_core_state_priority priority, unsigned short mask_bits) const; // vtbl+0x34 0x00980080 (:706)
    virtual bool apply_compressed_state(unsigned int game_handle, dc_state_list* reference_state, dc_state_list* compressed_state, dc_state_list** resultant_state, unsigned int network_time) const; // vtbl+0x38 0x009FA7D0 (:815)
    virtual unsigned short query_possible_mask_bits(dc_state_list* current_state, dc_state_list* reference_state, multi_connection* cp) const; // vtbl+0x3C 0x0097FF80 (:494)
    virtual void query_space_for_priority(const remote_object* rop, dc_state_list* current_state, dc_state_list* reference_state, unsigned short possible_mask_bits, multi_connection* cp, query_size_results* qs_results) const; // vtbl+0x40 0x009CD2D0 (:574)
    virtual void query_space_for_mask(query_size_results* qs_results, void* additional_data) const; // vtbl+0x44 0x0095DB50 (:524)
    virtual void notify_transmitted_updated(const remote_object* rop, dc_state_list* current_state, multi_connection* recipient, unsigned short possible_mask_bits, unsigned short mask_bits_transmitted) const; // vtbl+0x48 0x0095DCF0 (:909)
    virtual void reset_state_data() const; // vtbl+0x4C 0x009CD4E0 (:961)
    virtual void free_mp_state_data() const; // vtbl+0x50 0x009B10A0 (:966)

};

float calc_distance_from_player(const turret_data_bundle* ref_data, const turret_data_bundle* cur_data, const vector& send_to_player_pos); // 0x0095D840 (:253)
float calc_dot_from_player(const turret_data_bundle* ref_data, const turret_data_bundle* cur_data, const vector& send_to_player_pos, const vector& send_to_player_fvec); // 0x0095D950 (:268)
void calc_distance_and_dot_from_player(float& distance, float& dot, const turret_data_bundle* ref_data, const turret_data_bundle* cur_data, const multi_connection* cp); // 0x009CCB30 (:286)
int calc_location_priority_bias(float distance, float dot); // 0x0095DAE0 (:301)
void multi_game_object_turret_init(bool clear_allocators); // 0x0095DDB0 (:988)
bool multi_game_object_turret_is_client_authoritative(const turret* tp); // 0x0099D2E0 (:1069)
float get_nearest_rad_delta(float from, float to); // 0x0095DDC0 (:1088)
void multi_game_object_turret_apply_sim_state(const multi_turret_state* t_state, turret* tp); // 0x009802E0 (:1100)
