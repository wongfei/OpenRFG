// Original: volition/rfg/code/multi/multi_game/multi_game_object_player.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_object_player.cpp
//
// Networked players: the player state bundle (position, aim, sticks, animation and weapon state) and registration.
#pragma once

#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_types.h"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_state.h"

class dc_state_list;
class human;
class multi_connection;
class object;
class player;
class vector;

enum player_mp_flag_types
{
    PMPF_CHARGE_WEAPON = 0,
    PMPF_RUNNING_ATTACK = 1,
    PMPF_DETONATED_CHARGES = 2,
    PMPF_IN_AIR_DETONATE = 3,
    PMPF_JETPACK = 4,
    NUM_PLAYER_MP_FLAG_TYPES = 5,
};

// size 0xB0
struct player_data_bundle : public object_data_bundle<unsigned short>, public multi_player_state
{
    player_data_bundle(); // 0x0098C500

};

// size 0x814
class player_multi_registration : public multi_core_object_registration
{
public:
    virtual void create(unsigned int game_handle) const; // vtbl+0x04 0x0097EB00 (:1173)
    virtual void send_create_packet(const remote_object* rop, unsigned char* packet_data, unsigned short& packet_size, multi_connection* dest_conn) const; // vtbl+0x08 0x009C8B70 (:1199)
    virtual void process_create_packet(unsigned int& game_handle, unsigned short net_handle, int& packet_offset, unsigned char* packet_data) const; // vtbl+0x0C 0x009E05A0 (:1243)
    virtual void send_destroy_packet(unsigned int game_handle, unsigned char* packet_data, unsigned short& packet_size) const; // vtbl+0x10 0x0095B290 (:1345)
    virtual void process_destroy_packet(unsigned int game_handle, int& packet_offset, unsigned char* packet_data) const; // vtbl+0x14 0x009C9010 (:1356)
    virtual void process_object_free(object* obj) const; // vtbl+0x18 0x009F4C50 (:1380)
    virtual void post_update_cleanup() const; // vtbl+0x24 0x0099C540 (:1389)
    virtual void move(unsigned int game_handle) const; // vtbl+0x28 0x009F4C60 (:1394)
    virtual void get_state(unsigned int game_handle, dc_state_list* dcsl) const; // vtbl+0x2C 0x009F4C90 (:1402)
    virtual void free_state(dc_state_list* dcsl) const; // vtbl+0x30 0x009B0CB0 (:1581)
    virtual bool check_object_is_dirty(dc_state_list* reference_state, dc_state_list* cur_state, const remote_object* rop) const; // vtbl+0x54 0x0095B2A0 (:1589)
    virtual bool check_full_update(dc_state_list* ref_state, multi_connection* cp) const; // vtbl+0x58 0x0095B2B0 (:1595)
    virtual void get_compressed_state(dc_state_list* current_state, dc_state_list* reference_state, dc_state_list* compressed_state, multi_connection* cp, multi_core_state_priority priority, unsigned short mask_bits) const; // vtbl+0x34 0x009C9080 (:1605)
    virtual bool apply_compressed_state(unsigned int game_handle, dc_state_list* reference_state, dc_state_list* compressed_state, dc_state_list** resultant_state, unsigned int network_time) const; // vtbl+0x38 0x009F90D0 (:2083)
    virtual unsigned short query_possible_mask_bits(dc_state_list* current_state, dc_state_list* reference_state, multi_connection* cp) const; // vtbl+0x3C 0x0097E110 (:863)
    virtual void query_space_for_priority(const remote_object* rop, dc_state_list* current_state, dc_state_list* reference_state, unsigned short possible_mask_bits, multi_connection* cp, query_size_results* qs_results) const; // vtbl+0x40 0x009C9C00 (:2661)
    virtual void query_space_for_mask(query_size_results* qs_results, void* additional_data) const; // vtbl+0x44 0x0095B2D0 (:2504)
    virtual void notify_transmitted_updated(const remote_object* rop, dc_state_list* current_state, multi_connection* recipient, unsigned short possible_mask_bits, unsigned short mask_bits_transmitted) const; // vtbl+0x48 0x0095B6E0 (:2876)
    virtual void reset_state_data() const; // vtbl+0x4C 0x009CA2B0 (:2896)
    virtual void free_mp_state_data() const; // vtbl+0x50 0x0095B740 (:2902)
    player_multi_registration(); // 0x0096BC70
    virtual ~player_multi_registration(); // vtbl+0x00 0x0096BD70

};

void get_stick_compression(unsigned char& radius, unsigned char& angle, float forward, float sideways); // 0x0095B110 (:841)
void get_stick_decompression(float& forward, float& sideways, unsigned char radius, unsigned char angle); // 0x0095B1F0 (:852)
void multi_game_object_player_init(bool clear_allocators); // 0x009F53C0 (:2915)
bool multi_game_player_get_last_pos_velocity_update(const player* pp, unsigned int& update_time, vector& pos, vector& linear_velocity); // 0x0097EB50 (:3337)
void multi_game_player_set_mp_flag(human* hp, player_mp_flag_types flag, unsigned int val); // 0x0095B930 (:3418)
void (*multi_game_player_get_mp_flags_snapshot())(); // 0x0095BA20 (:3461)
void (*multi_game_player_get_mp_flags_handshake())(); // 0x0095BA30 (:3466)

// 0x0098C500: the states construct themselves
inline player_data_bundle::player_data_bundle()
{
}
