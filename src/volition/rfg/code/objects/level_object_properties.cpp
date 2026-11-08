// Original: volition/rfg/code/objects/level_object_properties.cpp
//
// Per-instance level object properties (decompiled; header comment in level_object_properties.h): a fixed table of
// 7072 instances handed out through a free list of indices, and the script point effect instances of
// Script_point_effect_inst_pool. Compiled in rfg_objects_unity.cpp.
//
// Notes (decompiled behaviour, kept as is):
// - lopm_free_instance frees the script point effect instances but leaves the pointers in the freed instance.
// - level_object_inst_properties::apply_damage ignores DBT_WORLD and DBT_RADIATION (and the dislodge hitpoints for them).

#include "volition/rfg/code/objects/level_object_properties.h"

#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/movers/object_mover.h"

static const unsigned int MAX_LEVEL_OBJECT_INSTANCES = 7072; // 0x1BA0, one per mover/chunk in the stream layout

object_pool<script_point_effect_inst, 320, 1> Script_point_effect_inst_pool; // 0x033B1FD0

// ---- file-static globals
static unsigned int Level_obj_inst_props_free_list[MAX_LEVEL_OBJECT_INSTANCES]; // 0x0301C7D8
static unsigned int Num_level_object_property_instances; // 0x0302F0EC, also the top of the free list
static level_object_inst_properties Level_object_inst_properties[MAX_LEVEL_OBJECT_INSTANCES]; // 0x03101CE0

// ---- script_point_effect_inst (header inlines)
// 0x00A055C0 (.h:50)
void script_point_effect_inst::reset()
{
    script_point_idx = -1;
    shape_key = 0xFFFFFFFF;
    current_effect = 0xFFFFFFFF;
    hitpoints = -1;
    death_timer.invalidate();
    state = SPES_INVALID;
    host_obj = nullptr;
    host_idx = -1;
}

// 0x00A055E0 (.h:61)
script_point_effect_inst::script_point_effect_inst()
{
    reset();
}

// ---- level_object_inst_properties
// 0x00A12A40
level_object_inst_properties::level_object_inst_properties()
{
}

// 0x00A12A60 (:57)
void level_object_inst_properties::init_from_base(const level_object_base_properties* base_props)
{
    m_hitpoints = base_props->m_max_hitpoints;
    m_dislodge_hitpoints = base_props->m_max_dislodge_hitpoints;
    for (int i = 0; i < 3; i++)
    {
        m_custom_effects[i] = 0xFFFFFFFF;
    }
    m_base_properties = base_props;
    m_flags = base_props->m_flags;
    m_release_effect_instance = 0xFFFFFFFF;
    m_death_timer.invalidate();
    for (int i = 0; i < 4; i++)
    {
        m_script_point_effect_insts[i] = nullptr;
    }
}

// 0x00A36CC0 (:90)
// On a multiplayer client only damage sent by the server counts.
void level_object_inst_properties::apply_damage(int damage, damaged_by_types from_type, bool from_server)
{
    if (Multiplayer && !multi_is_server() && !from_server)
    {
        return;
    }
    bool damaged;
    switch (from_type)
    {
    case DBT_CAR: damaged = m_flags.m_damaged_by_cars; break;
    case DBT_EXPLOSION: damaged = m_flags.m_damaged_by_explosions; break;
    case DBT_BULLET: damaged = m_flags.m_damaged_by_bullets; break;
    case DBT_LEVEL_OBJECT: damaged = m_flags.m_damaged_by_level_objects; break;
    case DBT_MELEE: damaged = m_flags.m_damaged_by_melee; break;
    default: return; // see the notes
    }
    if (damaged)
    {
        m_hitpoints -= damage < m_hitpoints ? damage : m_hitpoints;
    }
    bool dislodged;
    switch (from_type)
    {
    case DBT_CAR: dislodged = m_flags.m_dislodged_by_cars; break;
    case DBT_EXPLOSION: dislodged = m_flags.m_dislodged_by_explosions; break;
    case DBT_BULLET: dislodged = m_flags.m_dislodged_by_bullets; break;
    case DBT_LEVEL_OBJECT: dislodged = m_flags.m_dislodged_by_level_objects; break;
    case DBT_MELEE: dislodged = m_flags.m_dislodged_by_melee; break;
    default: return;
    }
    if (dislodged)
    {
        m_dislodge_hitpoints -= damage < m_dislodge_hitpoints ? damage : m_dislodge_hitpoints;
    }
}

// 0x00A12AA0 (:148)
void lopm_level_init_pre()
{
    for (unsigned int i = 0; i < MAX_LEVEL_OBJECT_INSTANCES; i++)
    {
        Level_obj_inst_props_free_list[i] = i;
    }
    Num_level_object_property_instances = 0;
}

// 0x00A12AC0 (:166)
// pos is unused. Returns nullptr when the table is full.
level_object_inst_properties* lopm_create_instance(const level_object_base_properties* base_props, const vector* pos)
{
    if (Num_level_object_property_instances >= MAX_LEVEL_OBJECT_INSTANCES)
    {
        return nullptr;
    }
    const unsigned int index = Level_obj_inst_props_free_list[Num_level_object_property_instances];
    if (index >= MAX_LEVEL_OBJECT_INSTANCES)
    {
        return nullptr;
    }
    Level_obj_inst_props_free_list[Num_level_object_property_instances] = 0xFFFFFFFF;
    level_object_inst_properties* prop_inst = &Level_object_inst_properties[index];
    prop_inst->init_from_base(base_props); // inlined
    Num_level_object_property_instances++;
    return prop_inst;
}

// 0x00AA12B0 (:191)
// Stops the custom effects, frees the script point effect instances and returns the index to the free list.
void lopm_free_instance(const level_object_inst_properties* prop_inst)
{
    if (prop_inst == nullptr)
    {
        return;
    }
    for (int i = 0; i < 3; i++)
    {
        if (prop_inst->m_custom_effects[i] == 0xFFFFFFFF)
        {
            continue;
        }
        object* obj = World.object_from_handle(prop_inst->m_custom_effects[i]);
        if (obj != nullptr && obj->get_obj_type() == OT_EFFECT)
        {
            ((object_effect*)obj)->stop(object_effect::SS_STOP_EMITTING);
        }
    }
    for (int i = 0; i < 4; i++)
    {
        if (prop_inst->m_script_point_effect_insts[i] != nullptr)
        {
            Script_point_effect_inst_pool.free(prop_inst->m_script_point_effect_insts[i]); // see the notes
        }
    }
    Num_level_object_property_instances--;
    Level_obj_inst_props_free_list[Num_level_object_property_instances] = (unsigned int)(prop_inst - Level_object_inst_properties);
}

// 0x00A5E5D0 (:247)
script_point_effect_inst* lopm_new_script_point_effect_inst()
{
    script_point_effect_inst* speip = Script_point_effect_inst_pool.alloc(0);
    if (speip == nullptr)
    {
        return nullptr;
    }
    speip->reset(); // inlined
    return speip;
}

// 0x00A8A6B0 (:261)
void lopm_free_script_point_effect_inst(script_point_effect_inst* speip)
{
    Script_point_effect_inst_pool.free(speip);
}

// 0x00A5E600 (:266)
// The next instance is taken before the update, which may free the current one.
void lopm_script_point_do_frame()
{
    Script_point_effect_inst_pool.thread_access.enter();
    script_point_effect_inst* speip = Script_point_effect_inst_pool.get_first(0);
    while (speip != nullptr)
    {
        script_point_effect_inst* next = Script_point_effect_inst_pool.get_next(speip);
        if (speip->host_obj != nullptr)
        {
            speip->host_obj->script_point_do_frame(speip->host_idx);
        }
        speip = next;
    }
    Script_point_effect_inst_pool.thread_access.leave();
}
