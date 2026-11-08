// Original: volition/rfg/code/multi/multi_game/multi_game_object_vehicle_automobile.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_object_vehicle_automobile.cpp
//
// Networked automobiles: the automobile state bundle and registration. Placement of the automobile data inferred.
#pragma once

#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_types.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/vlib/code/math/quaternion.h"
#include "volition/vlib/code/math/vector3.h"

class automobile;
class dc_state_list;
struct multi_automobile_state;
class multi_connection;
class object;
class vehicle;

// size 0x1
struct component_flags
{
    union
    {
        struct
        {
            unsigned char detached : 1; // 0x00:0
            unsigned char unlatched : 1; // 0x00:1
            unsigned char shattered : 1; // 0x00:2
        };
        unsigned char data; // 0x00
    };
};

// size 0x4
struct automobile_data
{
    union
    {
        struct
        {
            unsigned int accelerate : 8; // 0x00:0
            unsigned int brake_reverse : 1; // 0x00:8
            unsigned int handbrake : 1; // 0x00:9
            unsigned int honk : 1; // 0x00:10
            unsigned int steering : 7; // 0x00:11
            unsigned int on_fire : 1; // 0x00:18
            unsigned int smoking : 1; // 0x00:19
            unsigned int at_rest : 1; // 0x00:20
            unsigned int flat_tire : 8; // 0x00:21
        };
        unsigned int data; // 0x00
    };
};

// size 0x98
struct automobile_data_bundle : public object_data_bundle<unsigned short>
{
    vector pos; // 0x04
    quaternion orient; // 0x10
    vector linear_velocity; // 0x20
    vector angular_velocity; // 0x2C
    automobile_data data; // 0x38
    unsigned int maxhp; // 0x3C
    unsigned int hp; // 0x40
    bool smoking; // 0x44
    bool on_fire; // 0x45
    unsigned char component_count; // 0x46
    unsigned char flat_tire; // 0x47
    component_flags c_flags[66]; // 0x48
    unsigned char lights_bitmask[7]; // 0x8A
    human_teams mp_team; // 0x94
};

// size 0x814
class automobile_multi_registration : public multi_core_object_registration
{
    virtual void create(unsigned int game_handle) const; // vtbl+0x04 0x0099D4F0 (:588)
    virtual void send_create_packet(const remote_object* rop, unsigned char* packet_data, unsigned short& packet_size, multi_connection* dest_conn) const; // vtbl+0x08 0x009804F0 (:607)
    virtual void process_create_packet(unsigned int& game_handle, unsigned short net_handle, int& packet_offset, unsigned char* packet_data) const; // vtbl+0x0C 0x0099D550 (:659)
    virtual void send_destroy_packet(unsigned int game_handle, unsigned char* packet_data, unsigned short& packet_size) const; // vtbl+0x10 0x0095DEC0 (:712)
    virtual void process_destroy_packet(unsigned int game_handle, int& packet_offset, unsigned char* packet_data) const; // vtbl+0x14 0x00980830 (:725)
    virtual void process_object_free(object* obj) const; // vtbl+0x18 0x009F6350 (:737)
    virtual void post_update_cleanup() const; // vtbl+0x24 0x0095DED0 (:753)
    virtual void move(unsigned int game_handle) const; // vtbl+0x28 0x009B1170 (:765)
    virtual void get_state(unsigned int game_handle, dc_state_list* dcsl) const; // vtbl+0x2C 0x009F63A0 (:798)
    virtual void free_state(dc_state_list* dcsl) const; // vtbl+0x30 0x009B1210 (:953)
    virtual bool check_object_is_dirty(dc_state_list* reference_state, dc_state_list* cur_state, const remote_object* rop) const; // vtbl+0x54 0x0095DF00 (:961)
    virtual bool check_full_update(dc_state_list* ref_state, multi_connection* cp) const; // vtbl+0x58 0x0095DF20 (:969)
    virtual void get_compressed_state(dc_state_list* current_state, dc_state_list* reference_state, dc_state_list* compressed_state, multi_connection* cp, multi_core_state_priority priority, unsigned short mask_bits) const; // vtbl+0x34 0x0095DF40 (:980)
    virtual bool apply_compressed_state(unsigned int game_handle, dc_state_list* reference_state, dc_state_list* compressed_state, dc_state_list** resultant_state, unsigned int network_time) const; // vtbl+0x38 0x009FAA00 (:1223)
    virtual unsigned short query_possible_mask_bits(dc_state_list* current_state, dc_state_list* reference_state, multi_connection* cp) const; // vtbl+0x3C 0x009CD530 (:502)
    virtual void query_space_for_mask(query_size_results* qs_results, void* additional_data) const; // vtbl+0x44 0x0095E440 (:1417)
    virtual void query_space_for_priority(const remote_object* rop, dc_state_list* current_state, dc_state_list* reference_state, unsigned short possible_mask_bits, multi_connection* cp, query_size_results* qs_results) const; // vtbl+0x40 0x0095E450 (:1422)
    virtual void notify_transmitted_updated(const remote_object* rop, dc_state_list* current_state, multi_connection* recipient, unsigned short possible_mask_bits, unsigned short mask_bits_transmitted) const; // vtbl+0x48 0x0095E460 (:1698)
    virtual void reset_state_data() const; // vtbl+0x4C 0x009CD8A0 (:1717)
    virtual void free_mp_state_data() const; // vtbl+0x50 0x009B1230 (:1722)

};

void multi_game_object_vehicle_automobile_init(bool clear_allocators); // 0x0095E4C0 (:2006)
bool multi_game_object_vehicle_automobile_is_client_authoritative(const vehicle* vp); // 0x0099D9D0 (:2148)
bool multi_game_object_vehicle_is_local_player_vehicle(const vehicle* vp); // 0x00980870 (:2168)
void multi_game_object_vehicle_automobile_apply_sim_state(const multi_automobile_state* auto_state, automobile* vap); // 0x0099DA50 (:2182)
void multi_game_auto_predict_collision(automobile* ap); // 0x0099E520 (:2380)
