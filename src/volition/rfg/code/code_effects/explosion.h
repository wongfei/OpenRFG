// Original: volition/rfg/code/code_effects/explosion.h
// Implementation: volition/rfg/code/code_effects/explosion.cpp
//
// Explosions: explosion_create queues an explosion (explosion_create_info: its explosion_info, source and owner,
// position and orientation, weapon) and explosion_do_frame applies the queued ones (damage, impulses, destruction,
// effects) once per frame before the simulation. Placement of explosion_create_info inferred.
#pragma once

#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"

struct explosion_info;
class human;
class object;
class player;
struct sender;
struct weapon_info;

// size 0x58
struct explosion_create_info
{
    explosion_create_info(); // 0x006E5E10

    const explosion_info* p_explosion_info; // 0x00
    unsigned int source_handle; // 0x04
    unsigned int owner_handle; // 0x08
    matrix orient; // 0x0C
    vector pos; // 0x30
    vector local_pos; // 0x3C
    const weapon_info* w_info; // 0x48
    vector dir; // 0x4C
};

void unused_dcf_explosion(); // 0x006EF640 (:144)
void explosion_system_init(); // 0x006EC710 (:512)
void explosion_level_init_pre(); // 0x006F2810 (:525)
player* explosion_owner_of_last_explosion(); // 0x006E76B0 (:537)
void explosion_do_frame(); // 0x006F0160 (:543)
bool explosion_clear_all_pending(); // 0x006F0270 (:579)
void explosion_create(const explosion_info* p_explosion_info, object* source, object* owner, const vector& position, const matrix& orientation, const vector* dir, const weapon_info* w_info, bool from_server); // 0x006EC720 (:605)
void explosion_modify_impulses(vector* modified_linear_impulse, vector* modified_angular_impulse, const vector* linear_impulse, const vector* angular_impulse, float mass, float linear_impulse_scalar, float angular_impulse_scalar, float max_linear_velocity, float max_angular_velocity); // 0x006E1E50 (:686)
void explosion_process_humans(const object* source, human* attacker, const weapon_info* w_info, const vector& pos, const vector* dir, const explosion_info* e_info, const object* owner); // 0x006EC930 (:724)
bool explosion_get_damage_and_impulse(unsigned int& p_damage, vector& p_linear_impulse, vector& p_angular_impulse, const explosion_info* e_info, const object* target, const vector& explosion_pos, const vector* dir, const vector* target_use_pos); // 0x006E76E0 (:861)
bool explosion_is_obj_in_shape(object* objp, const explosion_info* e_info, const vector& explosion_pos, const vector* explosion_dir); // 0x006E1FB0 (:1004)
bool explosion_is_bbox_in_shape(const vector& bmin, const vector& bmax, const explosion_info* e_info, const vector& explosion_pos, const vector* explosion_dir); // 0x006E20C0 (:1024)
void multi_gw_send_explosion_create(const explosion_info* p_explosion_info, const object* source, const object* owner, const matrix& orient, const vector& pos, const weapon_info* w_info); // 0x006E7E80 (:1064)
void multi_gw_process_explosion_create(unsigned char* packet_data, const sender& from); // 0x006F02D0 (:1099)
void multi_game_explosion_event_clear(); // 0x006F2860 (:1150)
