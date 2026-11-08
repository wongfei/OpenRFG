// Original: volition/rfg/code/multi/multi_game/multi_game_object_rfg_movers.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_object_rfg_movers.cpp
//
// Networked rfg_movers (destructible buildings): the multiplayer info of each rfg_mover (rfg_mover::mp_info: damage
// history, destroy causes, sync state), the state bundle and registration, and client sync of sleeping movers. Placement
// of the error info inferred.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/quaternion/hkquaternionf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_types.h"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_state.h"
#include "volition/vlib/code/time/timestamp.h"

class dc_state_list;
class multi_connection;
class object;
class object_mover;
class rfg_mover;

// size 0x1
union multi_rfg_mover_destroy_causes
{
    struct
    {
        unsigned char destruction : 1; // 0x00:0
        unsigned char mp_destroy : 1; // 0x00:1
        unsigned char level_shutdown : 1; // 0x00:2
        unsigned char level_init_destroy : 1; // 0x00:3
        unsigned char stream_fail : 1; // 0x00:4
        unsigned char has_been_on_list : 1; // 0x00:5
    };
    unsigned char data; // 0x00
};

// size 0x80
struct multi_rfg_mover_damage_history
{
    unsigned int damaged[16]; // 0x00
    unsigned int repaired[16]; // 0x40
};

// size 0x88
struct multi_rfg_mover_info
{
    multi_sim_state<multi_mover_state>* sim_state; // 0x00
    multi_rfg_mover_damage_history damage_history; // 0x04
    multi_rfg_mover_destroy_causes destroy_causes; // 0x84
    unsigned short last_destruction_frame; // 0x86
};

// size 0x54
struct rfg_mover_data_bundle : public object_data_bundle<unsigned short>, public multi_mover_state
{
    rfg_mover_data_bundle(); // 0x0096BD20

};

// size 0x8
struct object_mover_error_info
{
    object_mover_error_info(); // 0x0096BCA0

    timestamp hold_time; // 0x00
    int hold_count; // 0x04
};

// size 0x70
struct rfg_mover_soft_key_info
{
    rfg_mover_soft_key_info(); // 0x0095D140

    hkVector4f cur_center_of_mass_position; // 0x00
    hkVector4f target_center_of_mass_position; // 0x10
    hkQuaternionf cur_orient; // 0x20
    hkQuaternionf target_orient; // 0x30
    hkVector4f target_pos_world; // 0x40
    hkVector4f target_linear_velocity; // 0x50
    hkVector4f target_angular_velocity; // 0x60
};

// size 0x814
class rfg_mover_multi_registration : public multi_core_object_registration
{
    virtual void create(unsigned int game_handle) const; // vtbl+0x04 0x0097F1B0 (:724)
    virtual void send_create_packet(const remote_object* rop, unsigned char* packet_data, unsigned short& packet_size, multi_connection* dest_conn) const; // vtbl+0x08 0x0097F1E0 (:739)
    virtual void process_create_packet(unsigned int& game_handle, unsigned short net_handle, int& packet_offset, unsigned char* packet_data) const; // vtbl+0x0C 0x009CC520 (:769)
    virtual void send_destroy_packet(unsigned int game_handle, unsigned char* packet_data, unsigned short& packet_size) const; // vtbl+0x10 0x0097F660 (:801)
    virtual void process_destroy_packet(unsigned int game_handle, int& packet_offset, unsigned char* packet_data) const; // vtbl+0x14 0x0097F720 (:832)
    virtual void process_object_free(object* obj) const; // vtbl+0x18 0x009F5E40 (:844)
    virtual bool allow_destroy_packet(unsigned int game_handle, int& packet_offset, unsigned char* packet_data) const; // vtbl+0x20 0x0097F6B0 (:814)
    virtual void post_update_cleanup() const; // vtbl+0x24 0x0095D130 (:862)
    virtual void move(unsigned int game_handle) const; // vtbl+0x28 0x0099CD30 (:1185)
    virtual void get_state(unsigned int game_handle, dc_state_list* dcsl) const; // vtbl+0x2C 0x009F5EB0 (:1314)
    virtual void free_state(dc_state_list* dcsl) const; // vtbl+0x30 0x009B0F60 (:1364)
    virtual bool check_object_is_dirty(dc_state_list* ref_state, dc_state_list* cur_state, const remote_object* rop) const; // vtbl+0x54 0x0097F7E0 (:1372)
    virtual bool check_full_update(dc_state_list* ref_state, multi_connection* cp) const; // vtbl+0x58 0x0095D7B0 (:1801)
    virtual void get_compressed_state(dc_state_list* current_state, dc_state_list* reference_state, dc_state_list* compressed_state, multi_connection* cp, multi_core_state_priority priority, unsigned short mask_bits) const; // vtbl+0x34 0x0095D150 (:1402)
    virtual bool apply_compressed_state(unsigned int game_handle, dc_state_list* reference_state, dc_state_list* compressed_state, dc_state_list** resultant_state, unsigned int network_time) const; // vtbl+0x38 0x009FA4C0 (:1525)
    virtual unsigned short query_possible_mask_bits(dc_state_list* current_state, dc_state_list* reference_state, multi_connection* cp) const; // vtbl+0x3C 0x0095D0C0 (:695)
    virtual void query_space_for_priority(const remote_object* rop, dc_state_list* current_state, dc_state_list* reference_state, unsigned short possible_mask_bits, multi_connection* cp, query_size_results* qs_results) const; // vtbl+0x40 0x009CC790 (:1682)
    virtual void query_space_for_mask(query_size_results* qs_results, void* additional_data) const; // vtbl+0x44 0x0095D4C0 (:1649)
    virtual void notify_transmitted_updated(const remote_object* rop, dc_state_list* current_state, multi_connection* recipient, unsigned short possible_mask_bits, unsigned short mask_bits_transmitted) const; // vtbl+0x48 0x0095D720 (:1779)
    virtual void reset_state_data() const; // vtbl+0x4C 0x009CCAA0 (:1812)
    virtual void free_mp_state_data() const; // vtbl+0x50 0x009B0F80 (:1818)
public:
    rfg_mover_multi_registration(); // 0x0096BCB0
    virtual ~rfg_mover_multi_registration(); // vtbl+0x00 0x0096BD90

};

multi_sim_state<multi_mover_state>* rfg_mover_allocate_sim_mover_state(); // 0x0099C7A0 (:500)
object_mover_error_info* object_mover_allocate_error_info(); // 0x0099C7D0 (:511)
void object_mover_free_error_info(object_mover_error_info* error_info); // 0x009B0E60 (:528)
void rfg_mover_free_mp_state_data(); // 0x009B0ED0 (:547)
bool rfg_mover_get_client_sync_info(rfg_mover* rmp, float& pos_error, float& angle_error); // 0x0099C7F0 (:973)
bool object_mover_should_sleep(object_mover* omp, float pos_error, float angle_error); // 0x0097F760 (:1155)
void multi_game_object_rfg_mover_init(bool clear_allocators); // 0x009E0F30 (:1909)
void multi_game_object_rfg_mover_created(rfg_mover* mover); // 0x0099D130 (:2211)
void multi_game_object_rfg_mover_destroyed(rfg_mover* mover); // 0x009E18B0 (:2224)
void create_level_rfg_mover_remote_objects(); // 0x009E1900 (:2243)
void object_mover_get_sleep_tolerances(float& pos_sleep_tol, float& angle_sleep_tol); // 0x0095D7F0 (:2259)

// 0x0096BD20: nothing to set
inline rfg_mover_data_bundle::rfg_mover_data_bundle()
{
}

// 0x0096BCA0: the hold time starts invalid (timestamp's constructor)
inline object_mover_error_info::object_mover_error_info()
{
}

// 0x0095D140: nothing to set
inline rfg_mover_soft_key_info::rfg_mover_soft_key_info()
{
}
