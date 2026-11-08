// Original: volition/rfg/code/objects/level_object_properties_shared.h
// Implementation: volition/rfg/code/objects/level_object_properties_shared.cpp
//
// Level object properties: damage, physics and effect settings shared by movers and other destructible level objects,
// parsed from level_objects.xtbl by lopm_system_init (plus a DLC table) and looked up by entry name
// (lopm_get_properties; entries are sorted, Default_level_object_base_properties when not found).
#pragma once

#include "volition/vlib/code/vlib_core/vlib_core.h"

class mempool_base;
struct explosion_info;
struct script_point_effect_info;

enum damaged_by_types
{
    DBT_NONE = 0,
    DBT_CAR = 1,
    DBT_EXPLOSION = 2,
    DBT_BULLET = 3,
    DBT_LEVEL_OBJECT = 4,
    DBT_MELEE = 5,
    DBT_WORLD = 6,
    DBT_RADIATION = 7,
};

// size 0x2
struct level_object_properties_flags
{
    unsigned char m_damaged_by_cars : 1; // 0x00:0
    unsigned char m_damaged_by_explosions : 1; // 0x00:1
    unsigned char m_damaged_by_bullets : 1; // 0x00:2
    unsigned char m_damaged_by_level_objects : 1; // 0x00:3
    unsigned char m_damaged_by_melee : 1; // 0x00:4
    unsigned char m_dislodged_by_cars : 1; // 0x00:5
    unsigned char m_dislodged_by_explosions : 1; // 0x00:6
    unsigned char m_dislodged_by_bullets : 1; // 0x00:7
    unsigned char m_dislodged_by_level_objects : 1; // 0x01:0
    unsigned char m_dislodged_by_melee : 1; // 0x01:1
    unsigned char m_allow_dynamic_link : 1; // 0x01:2
    unsigned char m_link_affects_effects : 1; // 0x01:3
    unsigned char m_death_explosion : 1; // 0x01:4
    unsigned char m_dislodge_on_death : 1; // 0x01:5
    unsigned char m_movable_by_humans : 1; // 0x01:6
    unsigned char m_generate_human_collision_event : 1; // 0x01:7
};

// size 0xC
struct custom_effect_info
{
    custom_effect_info(); // 0x00A05560 (.h:123)

    unsigned int effect_handle; // 0x00
    char* attachment_point_name; // 0x04
    bool looping; // 0x08
};

// size 0x8
struct explosion_container
{
    explosion_container(); // 0x00A05580 (.h:135)

    const explosion_info* exp_info; // 0x00
    char* attachment_point_name; // 0x04
};

// size 0x48
struct script_point_effect_info
{
    void reset(); // 0x00A05590 (.h:161)
    script_point_effect_info();

    char script_point_name[32]; // 0x00
    int max_hitpoints; // 0x20
    unsigned int* bullet_impact_override; // 0x24
    unsigned int custom_effect; // 0x28
    unsigned int prefix_effect; // 0x2C
    float prefix_effect_delay_min; // 0x30
    float prefix_effect_delay_max; // 0x34
    int prefix_hitpoints; // 0x38
    unsigned int death_effect; // 0x3C
    const explosion_info* death_explosion; // 0x40
    unsigned int spe_flags; // 0x44
};

// size 0xAC
struct level_object_base_properties
{
    void clear_flags(); // 0x00A12B40 (:86)
    level_object_base_properties(); // 0x00A12DC0

    char* m_name; // 0x00
    unsigned int m_collision_impact_override; // 0x04
    float m_collision_min_impact_energy; // 0x08
    unsigned int* m_melee_impact_override; // 0x0C
    unsigned int* m_bullet_impact_override; // 0x10
    script_point_effect_info* m_script_point_effect_infos[4]; // 0x14
    level_object_properties_flags m_flags; // 0x24
    custom_effect_info m_custom_effect_infos[3]; // 0x28
    int m_max_hitpoints; // 0x4C
    int m_max_dislodge_hitpoints; // 0x50
    float m_mass; // 0x54
    float m_corpse_mass; // 0x58
    float m_friction; // 0x5C
    float m_restitution; // 0x60
    float m_linear_dampening; // 0x64
    float m_angular_dampening; // 0x68
    unsigned char m_physical_material; // 0x6C
    unsigned char m_effect_material; // 0x6D
    unsigned int m_death_effect; // 0x70
    unsigned int m_release_effect; // 0x74
    int m_release_delay_ms; // 0x78
    int m_release_looping_ms; // 0x7C
    explosion_container m_death_explosions[1]; // 0x80
    int m_explosion_prefix_min_duration; // 0x88
    int m_explosion_prefix_max_duration; // 0x8C
    int m_prefix_hitpoints; // 0x90
    custom_effect_info m_prefix_effect_infos[2]; // 0x94
};

void lopm_system_init(mempool_base* pool, int dlc_id); // 0x00A37B60 (:808)
void lopm_dlc_shutdown(); // 0x00A12E20 (:831)
const level_object_base_properties* lopm_get_properties(const char* property_entry_name); // 0x00A12E30 (:843)
