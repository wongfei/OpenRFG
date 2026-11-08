// Original: volition/rfg/code/objects/humans/human_combat.h
// Implementation: volition/rfg/code/objects/humans/human_combat.cpp
//
// Combat bookkeeping of a human (human::combat): who last hit or killed it and whom it killed (handles, damage types,
// times), its target, melee state, and the timers of the combat behaviours (dodge, flinch, pursuit, grenade throw...).
// Placement of human_combat inferred.
#pragma once

#include "volition/rfg/code/ai/combat/combat_melee_actions.h"
#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/hash_table.h"

struct explosion_info;
enum flinch_directions;
class human;
enum human_melee_attack_types;
struct camera_shake;
class object;
class player;
class raycast_input;
struct weapon_fire_info;
struct weapon_info;

enum human_melee_attack_point
{
    HUMAN_MELEE_ATTACK_POINT_INVALID = 0,
    HUMAN_MELEE_ATTACK_POINT_RIGHT_HAND = 1,
    HUMAN_MELEE_ATTACK_POINT_LEFT_HAND = 2,
    HUMAN_MELEE_ATTACK_POINT_RIGHT_FOOT = 3,
    HUMAN_MELEE_ATTACK_POINT_LEFT_FOOT = 4,
    HUMAN_MELEE_ATTACK_POINT_WEAPON = 5,
    HUMAN_MELEE_ATTACK_POINT_COUNT = 6,
};

// One melee combat move (human_combat::cur_melee_combat_move). Placement inferred (H3: rfg_mover::apply_damage reads
// flags.structural_damage of the attacker's move).
// size 0x50
struct human_melee_combat_move_data
{
    // size 0x1
    struct move_flags
    {
        bool always_knock_down : 1; // 0x00:0
        bool npc_knockback : 1; // 0x00:1
        bool structural_damage : 1; // 0x00:2
        bool secondary_impact : 1; // 0x00:3
        bool no_orient_correction : 1; // 0x00:4
    };

    char* anim_set_name; // 0x00
    human_melee_combat_move_id move_id; // 0x04
    animation_action attack_animation; // 0x08
    human_melee_attack_point attack_point; // 0x0C
    camera_shake* impact_camera_shake; // 0x10
    float cast_radius; // 0x14
    float secondary_cast_range; // 0x18
    float secondary_cast_height; // 0x1C
    float ai_min_dist; // 0x20
    float ai_max_dist; // 0x24
    float impact_impulse; // 0x28
    float ragdoll_impulse; // 0x2C
    float away_impulse_scale; // 0x30
    float default_damage; // 0x34
    float damage_to_npcs; // 0x38
    float damage_to_players_from_npc; // 0x3C
    float damage_to_players_from_player; // 0x40
    float damage_to_vehicles; // 0x44
    float damage_to_structures; // 0x48
    move_flags flags; // 0x4C (an unnamed type in the PDB)
};

// A melee move of melee.xtbl: its name and its data per animation set (anim set name without the extension, up to five
// sets). Placement inferred.
// size 0xDC
struct human_melee_combat_move
{
    human_melee_combat_move(); // 0x00A9CB80
    ~human_melee_combat_move(); // 0x00A7EC60

    char* name; // 0x00
    str_data_hash_table<human_melee_combat_move_data*, 5, 32, unsigned char> move_data_table; // 0x04
};

extern unsigned char Riot_shield_melee_group; // 0x01694365
extern unsigned char Marauder_block_effect_idx; // 0x01694366
extern unsigned int Explosion_gibs_effect; // 0x01694380

// size 0xEC
struct human_combat
{
    human_combat(); // 0x00A0F260

    unsigned int last_hit_by_handle; // 0x00
    unsigned int last_hit_by_other_handle; // 0x04
    unsigned int last_hit_by_weapon_handle; // 0x08
    unsigned int last_hit_by_other_weapon_handle; // 0x0C
    damaged_by_types last_hit_by_damage_type; // 0x10
    damaged_by_types last_hit_by_other_damage_type; // 0x14
    int last_hit_time_ms; // 0x18
    int last_hit_by_other_time_ms; // 0x1C
    unsigned int last_killed_by_handle; // 0x20
    unsigned int last_killed_by_other_handle; // 0x24
    int last_killed_time_ms; // 0x28
    int last_killed_by_other_time_ms; // 0x2C
    vector last_killed_pos; // 0x30
    unsigned int last_kill_handle; // 0x3C
    unsigned int last_kill_other_handle; // 0x40
    int last_kill_time_ms; // 0x44
    int last_kill_other_time_ms; // 0x48
    unsigned int target_handle; // 0x4C
    unsigned int targets_building_handle; // 0x50
    const human_melee_combat_move_data* cur_melee_combat_move; // 0x54
    int hit_object_count; // 0x58
    vector last_continuous_pos; // 0x5C
    vector last_continuous_pos_2; // 0x68
    bool played_hit_effect; // 0x74
    float current_flank_offset; // 0x78
    float flank_delta; // 0x7C
    timestamp melee_damage_timer; // 0x80
    timestamp combat_maneuver_timestamp; // 0x84
    timestamp no_path_to_target_timestamp; // 0x88
    timestamp dodge_timestamp; // 0x8C
    timestamp hold_trigger_timestamp; // 0x90
    timestamp shoot_enemy_timestamp; // 0x94
    timestamp move_to_melee_timestamp; // 0x98
    timestamp melee_attack_interval_timer; // 0x9C
    timestamp flinch_timestamp; // 0xA0
    timestamp melee_delay_timer; // 0xA4
    timestamp beaten_up_timer; // 0xA8
    timestamp combat_ready_timestamp; // 0xAC
    timestamp combat_aim_timestamp; // 0xB0
    timestamp combat_blindfire_timestamp; // 0xB4
    timestamp pursuit_timestamp; // 0xB8
    timestamp last_pursuit_timestamp; // 0xBC
    timestamp throw_grenade_timestamp; // 0xC0
    unsigned int thrown_weapon; // 0xC4
    vector throw_velocity; // 0xC8
    bool thrown_weapon_released; // 0xD4
    unsigned int last_projectile_handle; // 0xD8
    unsigned int reserved_cover_node; // 0xDC
    unsigned int reserved_turret; // 0xE0
    timestamp not_undercover_timestamp; // 0xE4
    timestamp pain_sound_timestamp; // 0xE8
};

void unused_dcf_force_melee_combat_move(); // 0x00A09500 (:246)
void unused_dcf_use_attack(); // 0x00A095A0 (:334)
void unused_dcf_reload_melee_table(); // 0x00AB61F0 (:344)
animation_action human_combat_get_small_flinch_animation(const human* hp, flinch_directions flinch_dir, object_hit_location hit_location); // 0x00A095D0 (:442)
animation_action human_combat_get_large_flinch_animation(const human* hp, flinch_directions flinch_dir, object_hit_location hit_location, bool in_combat); // 0x00A09650 (:486)
flinch_directions human_combat_get_flinch_direction(const human* hp, const vector& dir); // 0x00A096E0 (:591)
void human_combat_play_flinch(human* hp, const vector& pos, const vector& hit_dir, object_hit_location hit_location, unsigned int large_flinch_percent); // 0x00A7E800 (:616)
void human_combat_play_coot_flinch(human* hp, const vector& hit_dir); // 0x00A09750 (:736)
void parse_melee_table(int dlc_id, bool reload); // 0x00AB2A80 (:1119)
bool bullet_probably_passed_through_a_wall(const weapon_fire_info& fire_info, const player* killer); // 0x00A54E00 (:1383)
void human_combat_system_init(); // 0x00AB2DA0 (:1418)
void human_combat_init(human_combat* hcp); // 0x00A09970 (:1444)
void human_combat_reset(human* hp); // 0x00A09A90 (:1520)
void human_combat_unified_melee_hit(object* target, human* attacker, const vector& hit_pos, const vector& hit_normal, const vector& hit_direction, const weapon_info* w_info, const human_melee_combat_move_data* attacker_cmp, unsigned int hit_shape_key, bool play_effect, bool do_check_swing); // 0x00AD13B0 (:1535)
void human_combat_process_hits(human* hp, raycast_input& ray_in, bool ignore_ground); // 0x00AD1930 (:1741)
void human_combat_process_melee(human* hp); // 0x00AD23A0 (:2166)
void human_combat_process_explosion_hit(human* target, human* attacker, const weapon_info* w_info, const vector& explosion_pos, const explosion_info* e_info, const object* owner, bool server_update); // 0x00ADBB30 (:2942)
void human_combat_apply_explosion_ragdoll_impulse(human* hp, const vector& impulse, const vector& angular_impulse, const human* perp); // 0x00AD4440 (:3190)
void human_combat_reset_melee_delay(human* hp); // 0x00A09AD0 (:3235)
bool human_combat_attack_melee_armed(human* hp, human_melee_attack_types attack_type, bool from_server, bool force_running); // 0x00A9CF80 (:3243)
bool human_combat_playing_combat_move(const human* hp); // 0x00A09AE0 (:3291)
bool human_combat_play_combat_move(human* hp, const human_melee_combat_move_data* move, bool from_server, bool force_running); // 0x00A7ECF0 (:3299)
animation_action human_combat_get_cover_melee_anim(const human* hp); // 0x00A09AF0 (:3422)
human_melee_combat_move_id human_combat_lookup_combat_move(const char* name); // 0x00A09C00 (:3505)
const human_melee_combat_move_data* human_combat_move_data_from_index(const human* hp, human_melee_combat_move_id move_id); // 0x00A55510 (:3533)
bool human_is_player_in_stealth(const human* hp); // 0x00A2F8F0 (:3563)
human* human_combat_get_target(const human* hp); // 0x00A2F920 (:3577)
unsigned int human_combat_get_num_melee_moves(); // 0x00A09C70 (:3589)
bool human_combat_is_blind_firing(const human* hp); // 0x00A09C80 (:3594)
void human_combat_process_hit_indicator(human* target, const vector* pos, const vector* impulse, float damage); // 0x00A2F9A0 (:3602)
bool human_combat_allowed_to_damage_target(const human* hp, const human* target); // 0x00A09CB0 (:3619)
bool human_combat_hit_human(const object* hit_objp, const human* hp); // 0x00A2FA60 (:3642)
human* human_combat_get_hit_human(const object* hit_objp); // 0x00A555A0 (:3681)
float human_combat_get_mp_leg_multiplier(); // 0x00A09CF0 (:3740)
float human_combat_get_mp_head_multiplier(); // 0x00A09D00 (:3745)
bool human_combat_maybe_block_melee(const object* victim, const object* attacker); // 0x00A7F1A0 (:3750)
bool human_combat_is_blocking_melee(const human* hp); // 0x00A09D10 (:3812)
