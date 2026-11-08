// Original: volition/rfg/code/ai/combat/combat_common.h
// Implementation: volition/rfg/code/ai/combat/combat_common.cpp
//
// Shared combat helpers: firing at targets (from turrets too), suppressing fire, reloading, covering fire requests,
// weapon range categories, charging decisions, destructive turret shots.
#pragma once

#include "volition/vlib/code/math/vector3.h"

class cover_node;
class human;
class matrix;
class npc;
class object;
class turret;
class weapon;

enum ai_combat_should_charge_results
{
    AI_COMBAT_SHOULD_NOT_CHARGE = 0,
    AI_COMBAT_SHOULD_CHARGE = 1,
    AI_COMBAT_MUST_CHARGE = 2,
};

enum weapon_range_categories
{
    WEAPON_RANGE_CLOSE = 0,
    WEAPON_RANGE_OPTIMAL = 1,
    WEAPON_RANGE_LONG = 2,
    WEAPON_OUT_OF_RANGE = 3,
    NUM_WEAPON_RANGE_CATEGORIES = 4,
};

// size 0x38
class ai_combat_action_parameters
{
public:
    void init_npc(npc* np_); // 0x0061D920 (:414)
    bool init_target(bool use_attention_target); // 0x0061D9B0 (:433)
    bool init_cover_node(); // 0x005E2FB0 (:460)
    void set_weapon(weapon* new_wp); // 0x005E2F50 (:423)
    ai_combat_action_parameters(); // 0x005DD7A0

    npc* np; // 0x00
    weapon* weapon_inst; // 0x04
    human* target; // 0x08
    vector target_dir; // 0x0C
    float dist_to_target; // 0x18
    const weapon* target_weap_inst; // 0x1C
    cover_node* node; // 0x20
    vector node_to_npc_dir; // 0x24
    float node_to_npc_dist; // 0x30
    bool penetrating_weapon; // 0x34
    bool in_penetrating_range; // 0x35
};

bool cover_me_collect_cb(object* obj, unsigned int shape_key, const void* cb_data); // 0x006574B0 (:305)
void ai_combat_post_attack_events(npc* np, const object* target); // 0x0061DB20 (:481)
ai_combat_should_charge_results ai_combat_should_charge_target(const npc* np); // 0x0061DBA0 (:513)
ai_combat_should_charge_results ai_combat_should_charge_target(const ai_combat_action_parameters& params); // 0x0061DE10 (:549)
bool ai_combat_should_request_covering_fire(npc* np, const cover_node* node, const human* target); // 0x0061E0E0 (:593)
npc* ai_combat_find_someone_to_cover_me(npc* np, const human* target); // 0x00657770 (:629)
bool ai_combat_is_going_to_cover(const npc* np); // 0x005E3050 (:658)
bool ai_combat_do_reload(npc* np); // 0x005E3090 (:683)
bool ai_combat_fire_at_target(const ai_combat_action_parameters& params, bool ignore_range); // 0x00693F20 (:781)
bool ai_combat_do_suppressing_fire(npc* np, const vector& target_pos); // 0x00657850 (:911)
bool ai_combat_fire_check_range(const ai_combat_action_parameters& params); // 0x0061E220 (:968)
bool ai_combat_fire_at_target_if_available(npc* np, bool quick_lower, bool plant_feet); // 0x00694180 (:998)
void ai_combat_fire_turret_at_target(npc* np, turret* tp, const human* target_hp, bool alternate_fire); // 0x006943C0 (:1059)
void ai_combat_fire_turret_at_pos(npc* np, turret* tp, const vector& pos, bool alternate_fire); // 0x00694560 (:1130)
bool test_destructive_shot_callback(object* hit_objp, void* cb_data); // 0x0061E290 (:1203)
bool ai_combat_target_too_close_to_explosive_turret(const turret* tp, const object* target); // 0x0061E2E0 (:1228)
bool ai_combat_turret_test_destructive_shot(const npc* np, const turret* tp, float test_radius); // 0x00676100 (:1277)
bool ai_combat_is_close_enough_to_use_turret(const npc* np, const turret* tp, float tolerance, vector* use_pos); // 0x005E3200 (:1366)
bool ai_combat_is_oriented_with_turret(const npc* np, const turret* tp, matrix* use_orient); // 0x005E3270 (:1376)
const weapon* ai_combat_get_action_firearm(const npc* np); // 0x0061E430 (:1398)
bool ai_combat_has_scoped_weapon(const npc* np); // 0x0061E4B0 (:1416)
