// Original: volition/rfg/code/multi/multi_game/multi_game_object_projectile.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_object_projectile.cpp
//
// Networked projectiles: the projectile state bundle and registration, and remote projectile creation.
#pragma once

#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_types.h"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_state.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"

class dc_state_list;
class multi_connection;
class object;
struct weapon_info;

// size 0x70
struct projectile_data_bundle : public object_data_bundle<unsigned char>, public multi_projectile_state
{
    projectile_data_bundle(); // 0x0096BD10

};

// size 0x5C
struct remote_projectile_create_info
{
    remote_projectile_create_info(); // 0x009511C0

    vector pos; // 0x00
    vector raycast_start_pos; // 0x0C
    float raycast_delta_pos_y; // 0x18
    matrix orient; // 0x1C
    vector velocity; // 0x40
    unsigned short owner_net_handle; // 0x4C
    const weapon_info* w_info; // 0x50
    unsigned int create_id; // 0x54
    bool try_launch_attach; // 0x58
};

// size 0x814
class projectile_multi_registration : public multi_core_object_registration
{
    virtual void create(unsigned int game_handle) const; // vtbl+0x04 0x0095BC70 (:539)
    virtual void send_create_packet(const remote_object* rop, unsigned char* packet_data, unsigned short& packet_size, multi_connection* dest_conn) const; // vtbl+0x08 0x009CA300 (:554)
    virtual void process_create_packet(unsigned int& game_handle, unsigned short net_handle, int& packet_offset, unsigned char* packet_data) const; // vtbl+0x0C 0x009CAAF0 (:671)
    virtual void send_destroy_packet(unsigned int game_handle, unsigned char* packet_data, unsigned short& packet_size) const; // vtbl+0x10 0x0095C1A0 (:718)
    virtual void process_destroy_packet(unsigned int game_handle, int& packet_offset, unsigned char* packet_data) const; // vtbl+0x14 0x0097EED0 (:728)
    virtual void process_object_free(object* obj) const; // vtbl+0x18 0x009F5AF0 (:737)
    virtual bool allow_create_packet(int& packet_offset, unsigned char* packet_data) const; // vtbl+0x1C 0x009CAC10 (:750)
    virtual void post_update_cleanup() const; // vtbl+0x24 0x0095C1B0 (:768)
    virtual void move(unsigned int game_handle) const; // vtbl+0x28 0x009CAC60 (:829)
    virtual void get_state(unsigned int game_handle, dc_state_list* dcsl) const; // vtbl+0x2C 0x009F5B40 (:1183)
    virtual void free_state(dc_state_list* dcsl) const; // vtbl+0x30 0x009B0D30 (:1277)
    virtual bool check_object_is_dirty(dc_state_list* ref_state, dc_state_list* cur_state, const remote_object* rop) const; // vtbl+0x54 0x0097EF80 (:1285)
    virtual bool check_full_update(dc_state_list* ref_state, multi_connection* cp) const; // vtbl+0x58 0x0095CA70 (:2071)
    virtual void get_compressed_state(dc_state_list* current_state, dc_state_list* reference_state, dc_state_list* compressed_state, multi_connection* cp, multi_core_state_priority priority, unsigned short mask_bits) const; // vtbl+0x34 0x0095C280 (:1305)
    virtual bool apply_compressed_state(unsigned int game_handle, dc_state_list* reference_state, dc_state_list* compressed_state, dc_state_list** resultant_state, unsigned int network_time) const; // vtbl+0x38 0x009F9FB0 (:1625)
    virtual unsigned short query_possible_mask_bits(dc_state_list* current_state, dc_state_list* reference_state, multi_connection* cp) const; // vtbl+0x3C 0x0095BBF0 (:501)
    virtual void query_space_for_priority(const remote_object* rop, dc_state_list* current_state, dc_state_list* reference_state, unsigned short possible_mask_bits, multi_connection* cp, query_size_results* qs_results) const; // vtbl+0x40 0x009CC120 (:1948)
    virtual void query_space_for_mask(query_size_results* qs_results, void* additional_data) const; // vtbl+0x44 0x0095C920 (:1870)
    virtual void notify_transmitted_updated(const remote_object* rop, dc_state_list* current_state, multi_connection* recipient, unsigned short possible_mask_bits, unsigned short mask_bits_transmitted) const; // vtbl+0x48 0x0095C9E0 (:2047)
    virtual void reset_state_data() const; // vtbl+0x4C 0x009CC450 (:2082)
    virtual void free_mp_state_data() const; // vtbl+0x50 0x009B0D50 (:2087)
public:
    projectile_multi_registration(); // 0x0096BC90
    virtual ~projectile_multi_registration(); // vtbl+0x00 0x0096BD80

};

void multi_game_object_extract_projectile_create_info(remote_projectile_create_info& create_info, unsigned char* packet_data, int& packet_offset); // 0x0095BC90 (:622)
void multi_game_object_projectile_init(bool clear_allocators); // 0x009E0B30 (:2109)
const multi_core_object_registration* multi_game_object_get_projectile_registration(); // 0x0095CA90 (:2220)
void multi_game_migration_cull_projectiles(); // 0x0099C5C0 (:2228)
void multi_game_migration_reset_projectile_nonce(); // 0x0097EFF0 (:2272)
void multi_game_enforce_mp_projectile_limit(); // 0x0099C620 (:2288)

// 0x0096BD10: nothing to set
inline projectile_data_bundle::projectile_data_bundle()
{
}

// 0x009511C0: nothing to set
inline remote_projectile_create_info::remote_projectile_create_info()
{
}
