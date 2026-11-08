// Original: volition/rfg/code/multi/multi_game/multi_game_object_movers.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_object_movers.cpp
//
// Networked general movers: the mover state bundle and registration, and the list of movers destroyed on the server.
#pragma once

#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_types.h"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_state.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"

class dc_state_list;
class general_mover;
class multi_connection;
class object;
struct sender;

// size 0x54
struct mover_data_bundle : public object_data_bundle<unsigned short>, public multi_mover_state
{
    mover_data_bundle(); // 0x0096BD00

};

// size 0x814
class multi_mover_registration : public multi_core_object_registration
{
    virtual void create(unsigned int game_handle) const; // vtbl+0x04 0x0097DC30 (:630)
    virtual void send_create_packet(const remote_object* rop, unsigned char* packet_data, unsigned short& packet_size, multi_connection* dest_conn) const; // vtbl+0x08 0x0097DC60 (:645)
    virtual void process_create_packet(unsigned int& game_handle, unsigned short net_handle, int& packet_offset, unsigned char* packet_data) const; // vtbl+0x0C 0x009C7B90 (:677)
    virtual void send_destroy_packet(unsigned int game_handle, unsigned char* packet_data, unsigned short& packet_size) const; // vtbl+0x10 0x009C8010 (:723)
    virtual void process_destroy_packet(unsigned int game_handle, int& packet_offset, unsigned char* packet_data) const; // vtbl+0x14 0x009C8060 (:741)
    virtual void process_object_free(object* obj) const; // vtbl+0x18 0x009F4A90 (:770)
    virtual void post_update_cleanup() const; // vtbl+0x24 0x0095A960 (:791)
    virtual void move(unsigned int game_handle) const; // vtbl+0x28 0x0099C140 (:797)
    virtual void get_state(unsigned int game_handle, dc_state_list* dcsl) const; // vtbl+0x2C 0x009F4AF0 (:903)
    virtual void free_state(dc_state_list* dcsl) const; // vtbl+0x30 0x009B0C60 (:942)
    virtual bool check_object_is_dirty(dc_state_list* ref_state, dc_state_list* cur_state, const remote_object* rop) const; // vtbl+0x54 0x0097E090 (:950)
    virtual bool check_full_update(dc_state_list* ref_state, multi_connection* cp) const; // vtbl+0x58 0x0095AFE0 (:1368)
    virtual void get_compressed_state(dc_state_list* current_state, dc_state_list* reference_state, dc_state_list* compressed_state, multi_connection* cp, multi_core_state_priority priority, unsigned short mask_bits) const; // vtbl+0x34 0x0095A980 (:974)
    virtual bool apply_compressed_state(unsigned int game_handle, dc_state_list* reference_state, dc_state_list* compressed_state, dc_state_list** resultant_state, unsigned int network_time) const; // vtbl+0x38 0x009F8DE0 (:1098)
    virtual unsigned short query_possible_mask_bits(dc_state_list* current_state, dc_state_list* reference_state, multi_connection* cp) const; // vtbl+0x3C 0x0095A900 (:604)
    virtual void query_space_for_priority(const remote_object* rop, dc_state_list* current_state, dc_state_list* reference_state, unsigned short possible_mask_bits, multi_connection* cp, query_size_results* qs_results) const; // vtbl+0x40 0x009C8100 (:1251)
    virtual void query_space_for_mask(query_size_results* qs_results, void* additional_data) const; // vtbl+0x44 0x0095ACF0 (:1219)
    virtual void notify_transmitted_updated(const remote_object* rop, dc_state_list* current_state, multi_connection* recipient, unsigned short possible_mask_bits, unsigned short mask_bits_transmitted) const; // vtbl+0x48 0x0095AF50 (:1345)
    virtual void reset_state_data() const; // vtbl+0x4C 0x009C8410 (:1379)
    virtual void free_mp_state_data() const; // vtbl+0x50 0x009C8460 (:1384)
public:
    multi_mover_registration(); // 0x0096BC60
    virtual ~multi_mover_registration(); // vtbl+0x00 0x0096BD60

};

void add_destroyed_network_handle_to_mover_destroyed_list(unsigned short net_handle); // 0x0095A2D0 (:331)
multi_sim_state<multi_mover_state>* general_mover_alloc_sim_state(); // 0x0099C100 (:462)
void general_mover_clean_up_mp_resources(general_mover* gmp); // 0x009C7B40 (:474)
void multi_game_object_mover_init(bool clear_allocators); // 0x009DFB40 (:1412)
void multi_game_general_mover_reset_destroyed_count(); // 0x0095B000 (:1697)
void multi_game_object_mover_apply_damage(unsigned int game_handle, int damage_hitpoints, damaged_by_types from_type); // 0x009C84F0 (:1704)
void multi_game_object_mover_process_apply_damage(unsigned char* packet_data, const sender& from); // 0x009C85F0 (:1738)
void multi_game_object_mover_process_activated(unsigned char* packet_data, const sender& from); // 0x009C8720 (:1765)
void multi_game_object_mover_send_activated(const general_mover* gmp); // 0x009C8820 (:1787)
unsigned int multi_game_object_mover_send_destroyed_movers_list(multi_connection* cp); // 0x009C88E0 (:1812)
void multi_game_object_process_destroyed_movers_list(unsigned char* packet_data, const sender& from); // 0x009C89B0 (:1835)
void create_level_general_mover_remote_objects(); // 0x009E0440 (:1889)
void multi_game_migration_destroy_general_mover_destroyed_list(); // 0x009C8AA0 (:1905)

// 0x0096BD00: nothing to set
inline mover_data_bundle::mover_data_bundle()
{
}
