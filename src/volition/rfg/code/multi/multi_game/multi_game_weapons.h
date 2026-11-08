// Original: volition/rfg/code/multi/multi_game/multi_game_weapons.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_weapons.cpp
//
// Networked weapons: bullet hits, tracers, melee hits, explosion and collision hits, fire/repair and impaling collisions
// sent to the server or the clients; the custom weapons and the weapon info iteration. Placement of the hit types inferred.
#pragma once

#include "volition/rfg/code/ai/combat/combat_melee_actions.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/vector3.h"

struct explosion_info;
class human;
class multi_connection;
class object;
class player;
struct sender;

enum multi_custom_weapons
{
    MCW_NONE = -1,
    MCW_EXPLOSIVE_OBJECT = 0,
    MCW_DEBRIS = 1,
    MCW_KILLZONE_CLIFF_KILL = 2,
    MCW_KILLZONE_CLIFF_SUICIDE = 3,
    MCW_KILLZONE_MINE_KILL = 4,
    MCW_KILLZONE_MINE_SUICIDE = 5,
    MCW_CHARGE_STICK = 6,
    MCW_ROCKET_HIT = 7,
    MCW_THERMOBARIC_ROCKET_HIT = 8,
    MCW_FALL_DAMAGE = 9,
    NUM_MULTI_CUSTOM_WEAPONS = 10,
};

// size 0x5C
struct bullet_hit_event
{
    bullet_hit_event(); // 0x009673C0

    weapon_fire_info fire_info; // 0x00
    unsigned int attacker_game_handle; // 0x54
    float damage; // 0x58
};

// size 0x58
struct tracer_event
{
    tracer_event(); // 0x009673E0

    weapon_fire_info fire_info; // 0x00
    unsigned int attacker_game_handle; // 0x54
};

// size 0x34
struct melee_hit_info
{
    melee_hit_info(); // 0x00967400

    unsigned int hit_handle; // 0x00
    vector hit_pos; // 0x04
    vector hit_normal; // 0x10
    vector hit_direction; // 0x1C
    unsigned int hit_shape_key; // 0x28
    const weapon_info* w_info; // 0x2C
    human_melee_combat_move_id move_id; // 0x30
};

// size 0x4C
struct melee_hit_bundle
{
    melee_hit_bundle(); // 0x00966F90

    static const int MAX_KEYS; // 0x01309EF4
    unsigned int attacker_handle; // 0x00
    unsigned int victim_handle; // 0x04
    int weapon_unique_id; // 0x08
    int move_id; // 0x0C
    vector hit_pos; // 0x10
    vector hit_normal; // 0x1C
    int num_shape_keys; // 0x28
    unsigned int shape_keys[8]; // 0x2C
};

// size 0x38
struct melee_hit_event
{
    melee_hit_event(); // 0x009872F0

    melee_hit_info hit_info; // 0x00
    unsigned int attacker_game_handle; // 0x34
};

// size 0x20
struct explosion_hit_info
{
    explosion_hit_info(); // 0x00967440

    unsigned int target; // 0x00
    unsigned int attacker; // 0x04
    unsigned int owner; // 0x08
    vector explosion_pos; // 0x0C
    const weapon_info* w_info; // 0x18
    const explosion_info* e_info; // 0x1C
};

bool multi_game_weapon_bullet_strike_is_important_to_sync(const weapon_fire_info& fire_info, const object* victim); // 0x009D3830 (:375)
bool multi_gw_can_hit_target(const human* attacker, const object* objp); // 0x009A3530 (:440)
bool multi_gw_can_hit_target(const human* attacker, human_teams target_team); // 0x009872A0 (:454)
void multi_gw_send_bullet_hit_to_connection(multi_connection* cp, const human* attacker, const weapon_fire_info& fire_info, float damage); // 0x009D3920 (:521)
void multi_gw_send_bullet_hit(const human* attacker, const weapon_fire_info& fire_info, float damage); // 0x009D3C50 (:604)
void multi_gw_process_bullet_hit(unsigned char* packet_data, const sender& from); // 0x009D3D30 (:682)
void multi_gw_send_tracer(const human* attacker, const weapon_fire_info& fire_info); // 0x009D4BF0 (:862)
void multi_gw_process_tracer(unsigned char* packet_data, const sender& from); // 0x009D4C90 (:883)
void multi_gw_send_melee_hit(const human* attacker, unsigned int target_handle, const weapon_info* w_info, const vector& hit_pos, const vector& hit_normal, const vector& hit_direction, unsigned int hit_shape_key, human_melee_combat_move_id melee_combat_move_id); // 0x009D5320 (:1034)
void multi_gw_process_melee_hit(unsigned char* packet_data, const sender& from); // 0x009D5560 (:1117)
void multi_game_weapons_reset_melee_hit(const human* hp); // 0x00967410 (:1238)
void multi_game_weapons_process_melee_hit(const human* hp); // 0x009D5B20 (:1245)
void multi_gw_send_explosion_hit_to_connection(multi_connection* cp, const explosion_hit_info& eh_info); // 0x009D5B90 (:1262)
void multi_gw_send_explosion_hit(const human* attacker, const human* victim, const human* owner, const vector& explosion_pos, const weapon_info* w_info, const explosion_info* e_info); // 0x009D5F10 (:1299)
void multi_gw_process_explosion_hit(unsigned char* packet_data, const sender& from); // 0x009D5FF0 (:1339)
void multi_gw_send_collision_hit(const human* hp, const object* other_obj, float damage); // 0x009D6350 (:1384)
void multi_gw_process_collision_hit(unsigned char* packet_data, const sender& from); // 0x009F6D90 (:1404)
void multi_gw_send_fire_repair(const player* pp, const vector& pos); // 0x009D6530 (:1431)
void multi_gw_process_fire_repair(unsigned char* packet_data, const sender& from); // 0x009D6750 (:1448)
void multi_gw_send_impaling_collide(const vector& pos, const vector& direction, const weapon_info* w_info, unsigned char material_type, material_shape_type shape_type, const human* hit_obj, float speed); // 0x009D6920 (:1475)
void multi_gw_process_impaling_collide(unsigned char* packet_data, const sender& from); // 0x009D6BD0 (:1500)
const weapon_info* multi_gw_get_custom_weapon_info(multi_custom_weapons wep); // 0x00967450 (:1534)
void multi_gw_get_debris_kill_info(const human* hp, const object* other_obj, human** attacker, const weapon_info** w_info); // 0x009E26F0 (:1548)
unsigned int multi_gw_get_first_weapon_info_handle(); // 0x00987300 (:1569)
unsigned int multi_gw_get_last_weapon_info_handle(); // 0x00987330 (:1584)
unsigned int multi_gw_get_prev_weapon_info_handle(unsigned int handle); // 0x00967470 (:1599)
unsigned int multi_gw_get_next_weapon_info_handle(unsigned int handle); // 0x009674C0 (:1623)

// 0x009673E0: the fire info's own constructor (no hit, no weapon)
inline tracer_event::tracer_event()
{
}

// 0x00966F90: nothing to set
inline melee_hit_bundle::melee_hit_bundle()
{
}

// 0x00967440: nothing to set
inline explosion_hit_info::explosion_hit_info()
{
}
