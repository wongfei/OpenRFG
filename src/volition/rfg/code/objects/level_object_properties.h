// Original: volition/rfg/code/objects/level_object_properties.h
// Implementation: volition/rfg/code/objects/level_object_properties.cpp
//
// Per-instance state of a level object's properties (hitpoints, running effects), allocated from a fixed table of 7072
// (Level_object_inst_properties with a free list, file static in level_object_properties.cpp) by lopm_create_instance.
// Script point effects (effects attached to chunk script points) come from Script_point_effect_inst_pool and are
// updated by lopm_script_point_do_frame.
#pragma once

#include "volition/rfg/code/objects/level_object_properties_shared.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/memory/object_pool.h"

class object_mover;

enum script_point_effect_state
{
    SPES_INVALID = -1,
    SPES_NORMAL = 0,
    SPES_STOPPED = 1,
    SPES_PRE_DEATH = 2,
    SPES_DEAD = 3,
};

// size 0x20
struct script_point_effect_inst
{
    void reset(); // 0x00A055C0 (.h:50)
    script_point_effect_inst(); // 0x00A055E0 (.h:61)

    int script_point_idx; // 0x00
    unsigned int shape_key; // 0x04
    unsigned int current_effect; // 0x08
    int hitpoints; // 0x0C
    timestamp death_timer; // 0x10
    script_point_effect_state state; // 0x14
    object_mover* host_obj; // 0x18
    int host_idx; // 0x1C
};

// size 0x3C
struct level_object_inst_properties
{
    void init_from_base(const level_object_base_properties* base_props); // 0x00A12A60 (:57)
    void apply_damage(int damage, damaged_by_types from_type, bool from_server); // 0x00A36CC0 (:90)
    level_object_inst_properties(); // 0x00A12A40

    level_object_properties_flags m_flags; // 0x00
    const level_object_base_properties* m_base_properties; // 0x04
    unsigned int m_custom_effects[3]; // 0x08
    script_point_effect_inst* m_script_point_effect_insts[4]; // 0x14
    int m_hitpoints; // 0x24
    int m_dislodge_hitpoints; // 0x28
    unsigned int m_prefix_effects[2]; // 0x2C
    timestamp m_death_timer; // 0x34
    unsigned int m_release_effect_instance; // 0x38
};

extern object_pool<script_point_effect_inst,320,1> Script_point_effect_inst_pool; // 0x033B1FD0

void lopm_level_init_pre(); // 0x00A12AA0 (:148)
level_object_inst_properties* lopm_create_instance(const level_object_base_properties* base_props, const vector* pos); // 0x00A12AC0 (:166)
void lopm_free_instance(const level_object_inst_properties* prop_inst); // 0x00AA12B0 (:191)
script_point_effect_inst* lopm_new_script_point_effect_inst(); // 0x00A5E5D0 (:247)
void lopm_free_script_point_effect_inst(script_point_effect_inst* speip); // 0x00A8A6B0 (:261)
void lopm_script_point_do_frame(); // 0x00A5E600 (:266)
