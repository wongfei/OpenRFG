// Original: volition/rfg/code/code_effects/explosion_shared.h
// Implementation: volition/rfg/code/code_effects/explosion_shared.cpp
//
// Explosion definitions from explosions.xtbl (up to 79), shared with the tools. Explosions are addressed by name,
// unique id (0..254, stored in packets) or handle (index into the table). flags: explosion_info_flags below (from the
// table's <Flags>), 0x8000 when a Duration is given (expanding explosion), 0x10000 when a ConeAngle is given
// (half_angle_dot holds its cosine). Placement of the flag constants inferred (only their names are in the binary).
#pragma once

#include "volition/rfg/code/objects/obj_enums.h"

// size 0xF0
struct explosion_info
{
    enum constants
    {
        MAX_NAME_SIZE = 32,
        MAX_EFFECTS = 4,
        MAX_MATERIAL_EFFECTS = 8,
    };

    char m_name[32]; // 0x00
    int m_unique_id; // 0x20
    unsigned int m_name_crc_str; // 0x24
    unsigned int flags; // 0x28
    float m_radius; // 0x2C
    float m_secondary_radius; // 0x30
    float m_knockdown_radius; // 0x34
    float m_flinch_radius; // 0x38
    float m_ai_sound_radius; // 0x3C
    unsigned int m_human_min_damage_hitpoints; // 0x40
    unsigned int m_human_max_damage_hitpoints; // 0x44
    unsigned int m_vehicle_min_damage_hitpoints; // 0x48
    unsigned int m_vehicle_max_damage_hitpoints; // 0x4C
    float player_damage_mult; // 0x50
    float player_veh_damage_mult; // 0x54
    float player_vehicle_impulse_mult; // 0x58
    float m_impulse_magnitude; // 0x5C
    int m_structural_damage; // 0x60
    unsigned int expanding_explosion_duration; // 0x64
    unsigned int expanding_explosion_delay; // 0x68
    unsigned int m_num_effects; // 0x6C
    unsigned int m_effects[4]; // 0x70
    unsigned int m_num_material_effects; // 0x80
    unsigned int m_material_effects[8]; // 0x84
    unsigned char m_material_referenece[8]; // 0xA4
    salvage_material_type salvage_material; // 0xAC
    int salvage_max_pieces; // 0xB0
    float salvage_probability; // 0xB4
    int time_between_breaks; // 0xB8
    float half_angle_dot; // 0xBC
    float blast_decal_radius; // 0xC0
    char camera_shake_type[32]; // 0xC4
    float camera_shake_multiplier; // 0xE4
    float camera_shake_falloff; // 0xE8
    bool ignore_orientation; // 0xEC
    bool always_ragdoll; // 0xED
};

extern unsigned int Num_explosion_infos; // 0x01DEE490

void unused_dcf_max_explosion_unique_id(); // 0x006E2120 (:86)
void explosion_parse_table(int dlc_id); // 0x006E8670 (:317)
void explosion_system_shared_init(); // 0x006E8740 (:418)
const explosion_info* explosion_lookup(const char* explosion_name); // 0x006E2150 (:433)
const explosion_info* explosion_lookup_by_unique_id(int unique_id); // 0x006E21A0 (:446)
unsigned char explosion_get_explosion_unique_id(const explosion_info* e_info); // 0x006E21E0 (:462)
unsigned int explosion_get_handle(const char* explosion_name); // 0x006E21F0 (:478)
const explosion_info* explosion_lookup(unsigned int handle); // 0x006E2230 (:495)
unsigned int explosion_get_handle_from_ptr(const explosion_info* e_info); // 0x006E2250 (:511)
