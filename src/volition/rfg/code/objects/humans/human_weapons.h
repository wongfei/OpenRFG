// Original: volition/rfg/code/objects/humans/human_weapons.h
// Implementation: volition/rfg/code/objects/humans/human_weapons.cpp
//
// Human weapon handling: equipping, dropping and reloading weapons, firearm triggers (primary and secondary), the
// weapon animations of each state, spread, thrown weapons and offhand/temporary weapons.
#pragma once

#include "volition/rfg/code/ai/combat/combat_melee_actions.h"
#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/weapons/weapons_enums.h"

class human;
// The melee attack buttons (human_melee_attack). Placement inferred.
enum human_melee_attack_types
{
    MELEE_ATTACK_PRIMARY = 0,
    MELEE_ATTACK_SECONDARY = 1,
    MELEE_ATTACK_TERTIARY = 2,
    MELEE_ATTACK_FIREARM = 3,
    NUM_MELEE_ATTACK_TYPES = 4,
};

struct inventory_item;
class vector;
class weapon;
struct weapon_anim_flags;
struct weapon_info;

void human_weapons_get_spread(float& min_spread, float& max_spread, const human* hp, const weapon_info* w_info); // 0x00A83670 (:639)
weapon* human_weapons_get_weapon(const human* hp); // 0x00A5A4F0 (:755)
const weapon_info* human_weapons_get_weapon_info(const human* hp); // 0x00A5A540 (:770)
void human_weapons_level_init_post(); // 0x00A0E670 (:780)
void human_weapons_process_human(human* hp); // 0x00ACB320 (:785)
bool human_weapons_is_weapon_raised(human* hp); // 0x00A5A5A0 (:840)
void human_weapons_drop_weapon(human* hp, inventory_item* iip); // 0x00ABD280 (:849)
void human_weapons_drop_equipped_weapon(human* hp); // 0x00ABD9B0 (:917)
void human_weapons_immediately_holster_weapon(human* hp); // 0x00AB8040 (:974)
bool human_weapons_is_no_weapon_equipped(const human* hp); // 0x00A5A5C0 (:994)
bool human_weapons_is_thrown_weapon_equipped(human* hp); // 0x00A5A5E0 (:1000)
bool human_weapons_is_melee_weapon_equipped(const human* hp); // 0x00A5A610 (:1006)
bool human_weapons_is_firearm_equipped(const human* hp); // 0x00A5A640 (:1012)
bool human_weapons_is_penetrating_weapon_equipped(const human* hp); // 0x00A5A680 (:1018)
bool human_weapons_is_two_handed_firearm_equipped(const human* hp); // 0x00A5A6A0 (:1024)
bool human_weapons_is_obvious_weapon_equipped(const human* hp); // 0x00A5A730 (:1041)
bool human_weapons_is_reloading(const human* hp, bool secondary_weapon); // 0x00A5A750 (:1117)
void human_weapons_cancel_reload(human* hp); // 0x00A5A840 (:1132)
vector human_weapons_firearm_get_target_pos(const human* hp); // 0x00A0E680 (:1173)
animation_action human_weapons_get_throw_animation(human* hp, const weapon_info* w_info); // 0x00ABDB60 (:1188)
bool human_weapons_start_throw(human* hp, weapon* wp, const vector* npc_throw_vel, bool roll); // 0x00ACB520 (:1288)
void human_weapons_drop_shield(human* hp); // 0x00A9D710 (:1545)
bool human_weapons_drop_thrown_weapon(human* hp); // 0x00ACC140 (:1591)
bool human_is_throwing_weapon(const human* hp); // 0x00A0E760 (:1731)
void human_weapons_unattach_projectiles(human* hp); // 0x00A9D910 (:1736)
void human_weapons_get_state_animation(weapon_class_type weapon_class, weapon_anim_flags animation_flags, animation_state& anim_state, vanim_aliases& anim_alias, float& blend_time); // 0x00A0E780 (:1753)
animation_action human_weapons_firearm_get_attack_animation(weapon_class_type weapon_class, human* hp); // 0x00ABDD00 (:2047)
animation_action human_weapons_get_reload_animation(weapon_class_type weapon_class, human* hp); // 0x00A0ECA0 (:2298)
animation_action human_weapons_get_shuffle_animation(human* hp, bool right); // 0x00A0EE20 (:2375)
animation_action human_weapons_get_dir_change_animation(human* hp); // 0x00A0EEC0 (:2436)
human_melee_combat_move_id human_weapons_melee_get_combat_move(const human* hp, const weapon_info* wip, human_melee_attack_types attack_type); // 0x00A0EF60 (:2471)
animation_action human_weapons_get_equip_animation(weapon_class_type weapon_class, human* hp); // 0x00A0EFD0 (:2501)
animation_action human_weapons_get_unequip_animation(weapon_class_type weapon_class, human* hp); // 0x00A0F060 (:2537)
void human_weapons_firearm_start_reload(const human* hp, weapon* wp, int delay); // 0x00A331D0 (:3086)
void human_weapons_unequip_weapon(human* hp, weapon* wp); // 0x00A0F100 (:3097)
void human_weapons_process_weapons_pre(); // 0x00A5A950 (:3109)
void human_weapons_process_weapons_post(); // 0x00ACC9C0 (:3116)
bool human_weapons_firearm_pull_secondary_weapon_trigger(human* hp); // 0x00ACCBC0 (:3143)
bool human_weapons_firearm_pull_trigger(human* hp); // 0x00ACCC90 (:3184)
int human_weapons_get_rounds_fired(const human* hp); // 0x00A5A9A0 (:3215)
bool human_weapons_is_trigger_pulled(const human* hp); // 0x00A5A9C0 (:3230)
void human_weapons_firearm_release_secondary_weapon_trigger(human* hp); // 0x00A5A9F0 (:3244)
void human_weapons_firearm_release_trigger(human* hp); // 0x00A5AA60 (:3260)
bool human_weapons_firearm_reload(human* hp, bool play_animation, weapon* special_weapon); // 0x00A9D970 (:3298)
