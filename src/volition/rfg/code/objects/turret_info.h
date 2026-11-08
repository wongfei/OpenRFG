// Original: volition/rfg/code/objects/turret_info.h
// Implementation: volition/rfg/code/objects/turret_info.cpp
//
// Turret infos (turrets table): per turret type the field of view and rotation speeds, camera offsets, hitpoints,
// sounds and effects, the three physical components and their masses and motors, and the weapons.
#pragma once

#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"

struct item_info;
struct weapon_info;

// size 0xE8
struct turret_info
{
    // size 0x4
    struct turret_info_flags
    {
        bool has_side_fov_limits : 1; // 0x00:0
        bool has_rear_fov_limits : 1; // 0x00:1
        bool transform_to_tag : 1; // 0x00:2
        bool maintain_constraint_stability : 1; // 0x00:3
        bool force_fine_aim : 1; // 0x00:4
        bool center_on_player_entry : 1; // 0x00:5
        bool second_weapon_alt_fire : 1; // 0x00:6
        bool automated : 1; // 0x00:7
        bool stationary_operator : 1; // 0x01:0
        bool manned_vehicle_turret : 1; // 0x01:1
        bool no_operator_animation : 1; // 0x01:2
        bool limited_ammo_primary : 1; // 0x01:3
        bool limited_ammo_secondary : 1; // 0x01:4
        bool invulnerable : 1; // 0x01:5
        bool primary_weapon_physics : 1; // 0x01:6
        bool cycle_primary_weapon : 1; // 0x01:7
        bool cycle_secondary_weapon : 1; // 0x02:0
        bool primary_reset_cycle : 1; // 0x02:1
        bool secondary_reset_cycle : 1; // 0x02:2
        bool primary_weapon_local_door : 1; // 0x02:3
        bool secondary_weapon_local_door : 1; // 0x02:4
        bool primary_weapon_all_doors : 1; // 0x02:5
        bool secondary_weapon_all_doors : 1; // 0x02:6
        bool secondary_uses_primary_tags : 1; // 0x02:7
        bool auto_return_dont_vertical : 1; // 0x03:0
        bool auto_return_dont_horizontal : 1; // 0x03:1
        bool allow_simul_fire : 1; // 0x03:2
        bool link_overheats : 1; // 0x03:3
        bool smooth_camera : 1; // 0x03:4
        bool gun_of_tharsis : 1; // 0x03:5
        bool aim_offset_valid : 1; // 0x03:6
    };

    // size 0x8
    struct turret_component_info
    {
        const item_info* i_info; // 0x00
        const char* vehicle_component_name; // 0x04
    };

    const char* name; // 0x00
    checksum_stri name_cksum_stri; // 0x04
    float h_fov_pos; // 0x08
    float h_fov_neg; // 0x0C
    vector2 v_fov_blend_spline_points[7]; // 0x10
    float h_rot_speed; // 0x48
    float v_rot_speed; // 0x4C
    float h_rot_max_mul; // 0x50
    float v_rot_max_mul; // 0x54
    float h_rot_max_ramp_time; // 0x58
    float v_rot_max_ramp_time; // 0x5C
    float movement_limit_left; // 0x60
    float movement_limit_right; // 0x64
    vector default_camera_offset; // 0x68
    vector fine_aim_camera_offset; // 0x74
    float recoil_comp; // 0x80
    unsigned int max_hitpoints; // 0x84
    int turning_loop_cue_id; // 0x88
    int door_open_cue_id; // 0x8C
    int door_close_cue_id; // 0x90
    int firing_pattern_prefix; // 0x94
    unsigned int primary_alt_fire_effect; // 0x98
    unsigned int secondary_alt_fire_effect; // 0x9C
    turret_info_flags flags; // 0xA0
    turret_component_info components[3]; // 0xA4
    float mass[3]; // 0xBC
    float middle_motor_power; // 0xC8
    float top_motor_power; // 0xCC
    const weapon_info* w_info; // 0xD0
    const weapon_info* secondary_w_info; // 0xD4
    const item_info* weapon_corpse_info; // 0xD8
    vector aim_offset; // 0xDC
};

extern int Num_turret_infos; // 0x0302F22C

void turret_info_read_table(bool reload, int dlc_id); // 0x00A207A0 (:370)
const turret_info* turret_find_info(const char* name); // 0x00A208F0 (:420)
const turret_info* turret_find_info_from_crc(unsigned int name_crc); // 0x00A20950 (:434)
void unused_dcf_turret_reload(); // 0x00A20990 (:451)
