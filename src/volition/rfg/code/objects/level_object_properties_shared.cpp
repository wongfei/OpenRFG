// Original: volition/rfg/code/objects/level_object_properties_shared.cpp
//
// Level object base properties (decompiled; header comment in level_object_properties_shared.h): level_objects.xtbl
// (and dlcNN_level_objects.xtbl) is read into one array per table from the given pool, kept sorted by name with an
// insertion sort, so lopm_get_properties is a binary search (base table first, then the DLC table). Names, effect
// attachment points and script point effect infos come from the same pool, the impact override arrays from
// Effect_material_pool. Compiled in rfg_objects_unity.cpp.
//
// Notes (decompiled behaviour, kept as is):
// - lopm_parse_level_object_property loops forever when a DeathExplosionList entry names an unknown explosion (the
//   element is not advanced on that path).
// - A missing Prefix_Effects/Max_Duration resets the min duration instead of the max one.
// - m_explosion_prefix_min/max_duration, m_prefix_hitpoints and m_release_looping_ms are only written when the entry
//   has Prefix_Effects / Release_Effect; otherwise they keep the caller's (stack) garbage.
// - The "Multiple ... impact override effects" messages are formatted but never printed.
// - lopm_parse_table_file returns without closing the table when an entry fails to parse.
// - The DLC table is allocated from the pool again by every DLC init; lopm_dlc_shutdown only forgets it.

#include "volition/rfg/code/objects/level_object_properties_shared.h"

#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/trace.hpp"

#include <stdio.h>
#include <string.h>

static const unsigned int MAX_ATTACHMENT_POINT_NAME = 32; // attachment point names are cut to 31 characters
static const int MAX_SCRIPT_POINT_EFFECTS = 4; // m_script_point_effect_infos
static const int MAX_CUSTOM_EFFECTS = 3; // m_custom_effect_infos
static const int MAX_DEATH_EXPLOSIONS = 1; // m_death_explosions
static const int MAX_PREFIX_EFFECTS = 2; // m_prefix_effect_infos

// ---- file-static globals
static const char* Script_point_effect_info_flag_names[9] = {"Stop Custom on Detach", "Kill on Detach", "Prefix Effect No Loop", "No Bullet Damage", "No Melee Damage", "Kiosk Large Screen", "Kiosk Small Screen", "Night Only", "Kill on Move"}; // 0x01694504
static level_object_base_properties* Level_object_base_properties; // 0x030266A0
static unsigned int Num_level_object_property_entries; // 0x0302F0F0
static unsigned int Dlc_num_level_object_property_entries; // 0x0302F0F4
static level_object_base_properties* Dlc_level_object_base_properties; // 0x0302F0F8
static const level_object_base_properties* Default_level_object_base_properties; // 0x0302F0FC

// The thread's message buffer (TLS +0x38 in rfg.exe), see the notes.
static __declspec(thread) char Lopm_message[1024];

// ---- header inlines
// 0x00A05560 (.h:123)
custom_effect_info::custom_effect_info()
{
    effect_handle = 0xFFFFFFFF;
    attachment_point_name = nullptr;
    looping = true;
}

// 0x00A05580 (.h:135)
explosion_container::explosion_container()
{
    exp_info = nullptr;
    attachment_point_name = nullptr;
}

// 0x00A05590 (.h:161)
// prefix_effect_delay_max is left as is (the parser always writes it).
void script_point_effect_info::reset()
{
    script_point_name[0] = '\0';
    max_hitpoints = -1;
    bullet_impact_override = nullptr;
    custom_effect = 0xFFFFFFFF;
    prefix_effect = 0xFFFFFFFF;
    prefix_effect_delay_min = 0.0f;
    prefix_hitpoints = 0;
    death_effect = 0xFFFFFFFF;
    death_explosion = nullptr;
    spe_flags = 0;
}

// Not in rfg.exe (inferred, unused).
script_point_effect_info::script_point_effect_info()
{
    reset();
}

// 0x00A12DC0
level_object_base_properties::level_object_base_properties()
{
}

// 0x00A12B40 (:86)
void level_object_base_properties::clear_flags()
{
    memset(&m_flags, 0, sizeof(m_flags));
}

// 0x00A12B50 (:98)
// "Affected_by_objects" covers cars, explosions, level objects and melee (damaged and dislodged).
static bool lopm_parse_damaged_dislodged_by(level_object_base_properties* obj_prop, xml_element* root)
{
    bool affected_by_objects = false;
    xml_table_get_req_bool(affected_by_objects, root, "Affected_by_objects");
    level_object_properties_flags& flags = obj_prop->m_flags;
    flags.m_damaged_by_cars = affected_by_objects;
    flags.m_damaged_by_explosions = affected_by_objects;
    flags.m_damaged_by_level_objects = affected_by_objects;
    flags.m_damaged_by_melee = affected_by_objects;
    flags.m_dislodged_by_cars = affected_by_objects;
    flags.m_dislodged_by_explosions = affected_by_objects;
    flags.m_dislodged_by_level_objects = affected_by_objects;
    flags.m_dislodged_by_melee = affected_by_objects;
    bool damaged_by_bullets = false;
    xml_table_get_req_bool(damaged_by_bullets, root, "Damaged_by_bullets");
    flags.m_damaged_by_bullets = damaged_by_bullets;
    bool dislodged_by_bullets = false;
    xml_table_get_req_bool(dislodged_by_bullets, root, "Dislodged_by_bullets");
    flags.m_dislodged_by_bullets = dislodged_by_bullets;
    return true;
}

// 0x00A12BE0 (:149)
// An effect handle per melee group (0xFFFFFFFF = no override); the first definition of a group wins.
static unsigned int* melee_group_parse(level_object_base_properties* obj_prop, xml_element* melee_group)
{
    const int count = rfg_get_melee_group_count();
    unsigned int* handles = (unsigned int*)Effect_material_pool.static_mempool_base::alloc(count * sizeof(unsigned int), 0);
    if (handles == nullptr)
    {
        return nullptr;
    }
    for (int i = 0; i < count; i++)
    {
        handles[i] = 0xFFFFFFFF;
    }
    for (xml_element* def = xml_table_find_req(melee_group, "definition"); def != nullptr; def = xml_table_find_next(melee_group, def, "definition"))
    {
        const char* effect = xml_table_get_req_string_ref(def, "Effect");
        const char* group = xml_table_get_req_string_ref(def, "Melee_group");
        const unsigned char index = rfg_get_melee_group_index(group);
        if (handles[index] == 0xFFFFFFFF)
        {
            handles[index] = effects_get_handle(effect);
        }
        else
        {
            sprintf(Lopm_message, "Multiple melee impact override effects are defined for the same group (%s) in %s", group, obj_prop->m_name); // see the notes
        }
    }
    return handles;
}

// 0x00A12CD0 (:191)
// As melee_group_parse, per bullet group (4-byte aligned).
static unsigned int* bullet_group_parse(level_object_base_properties* obj_prop, xml_element* bullet_group)
{
    const int count = rfg_get_bullet_group_count();
    unsigned int* handles = (unsigned int*)Effect_material_pool.static_mempool_base::alloc(count * sizeof(unsigned int), 4);
    if (handles == nullptr)
    {
        return nullptr;
    }
    for (int i = 0; i < count; i++)
    {
        handles[i] = 0xFFFFFFFF;
    }
    for (xml_element* def = xml_table_find_req(bullet_group, "definition"); def != nullptr; def = xml_table_find_next(bullet_group, def, "definition"))
    {
        const char* effect = xml_table_get_req_string_ref(def, "Effect");
        const char* group = xml_table_get_req_string_ref(def, "Bullet_group");
        const unsigned char index = rfg_get_bullet_group_index(group);
        if (handles[index] == 0xFFFFFFFF)
        {
            handles[index] = effects_get_handle(effect);
        }
        else
        {
            sprintf(Lopm_message, "Multiple bullet impact override effects are defined for the same group (%s) in %s", group, obj_prop->m_name); // see the notes
        }
    }
    return handles;
}

// A copy of an attachment point name from the pool, cut to MAX_ATTACHMENT_POINT_NAME (inlined three times).
static char* lopm_copy_attachment_point(const char* name, mempool_base* pool)
{
    unsigned int size = (unsigned int)strlen(name) + 1;
    if (size > MAX_ATTACHMENT_POINT_NAME)
    {
        size = MAX_ATTACHMENT_POINT_NAME;
    }
    char* copy = (char*)pool->alloc(size, 1);
    strncpy_safe(copy, name, size);
    return copy;
}

// 0x00A36DC0 (:240)
// LTCG: obj_prop in ECX, pool in EDX. A <Level_Object> entry: Inherited_Attributes (impact overrides, script point
// effects), Non_Inherited_Attributes (movable by humans, custom effects) and Non_Destroyable_Attributes (hitpoints,
// physics, death/prefix/release effects; the defaults below apply without it).
static bool lopm_parse_level_object_property(level_object_base_properties* obj_prop, xml_element* root, mempool_base* pool)
{
    obj_prop->clear_flags(); // inlined
    char name[32];
    xml_table_get_req_string(name, sizeof(name), root, "Name");
    obj_prop->m_name = (char*)pool->alloc((unsigned int)strlen(name) + 1, 1);
    strcpy(obj_prop->m_name, name);

    obj_prop->m_collision_impact_override = 0xFFFFFFFF;
    obj_prop->m_collision_min_impact_energy = -1.0f;
    obj_prop->m_melee_impact_override = nullptr;
    obj_prop->m_bullet_impact_override = nullptr;
    for (int i = 0; i < MAX_SCRIPT_POINT_EFFECTS; i++)
    {
        obj_prop->m_script_point_effect_infos[i] = nullptr;
    }
    xml_element* inherited = xml_table_find_opt(root, "Inherited_Attributes");
    if (inherited != nullptr)
    {
        xml_element* overrides = xml_table_find_opt(inherited, "Effect_material_overrides");
        if (overrides != nullptr)
        {
            obj_prop->m_collision_impact_override = effects_get_handle(xml_table_get_opt_string_ref(overrides, "Collision_impact"));
            if (obj_prop->m_collision_impact_override != 0xFFFFFFFF)
            {
                xml_table_get_opt_float(obj_prop->m_collision_min_impact_energy, overrides, "Min_impact_energy");
            }
            xml_element* melee = xml_table_find_opt(overrides, "Melee_impact");
            if (melee != nullptr)
            {
                obj_prop->m_melee_impact_override = melee_group_parse(obj_prop, melee);
            }
            xml_element* bullet = xml_table_find_opt(overrides, "Bullet_impact");
            if (bullet != nullptr)
            {
                obj_prop->m_bullet_impact_override = bullet_group_parse(obj_prop, bullet);
            }
        }
        xml_element* script_points = xml_table_find_opt(inherited, "Script_Point_Effects");
        if (script_points != nullptr)
        {
            int count = 0;
            for (xml_element* cur = script_points->elements; cur != nullptr && count < MAX_SCRIPT_POINT_EFFECTS; cur = cur->next, count++)
            {
                script_point_effect_info* info = (script_point_effect_info*)pool->alloc(sizeof(script_point_effect_info), 4);
                obj_prop->m_script_point_effect_infos[count] = info;
                info->reset(); // inlined
                const char* sp_name = xml_table_get_opt_string_ref(cur, "Script_Point_Name");
                if (sp_name != nullptr)
                {
                    strncpy_safe(info->script_point_name, sp_name, sizeof(info->script_point_name));
                }
                xml_table_get_opt_int(info->max_hitpoints, cur, "Hitpoints");
                xml_element* bullet = xml_table_find_opt(cur, "Bullet_impact");
                if (bullet != nullptr)
                {
                    info->bullet_impact_override = bullet_group_parse(obj_prop, bullet);
                }
                const char* effect = xml_table_get_opt_string_ref(cur, "Custom_Effect");
                if (effect != nullptr)
                {
                    info->custom_effect = effects_get_handle(effect);
                }
                effect = xml_table_get_opt_string_ref(cur, "Prefix_Effect");
                if (effect != nullptr)
                {
                    info->prefix_effect = effects_get_handle(effect);
                }
                xml_table_get_req_float(info->prefix_effect_delay_min, cur, "Prefix_Effect_Delay_Min");
                if (!xml_table_get_opt_float(info->prefix_effect_delay_max, cur, "Prefix_Effect_Delay_Max"))
                {
                    info->prefix_effect_delay_max = 0.0f;
                }
                xml_table_get_opt_int(info->prefix_hitpoints, cur, "Prefix_Hitpoints");
                effect = xml_table_get_opt_string_ref(cur, "Death_Effect");
                if (effect != nullptr)
                {
                    info->death_effect = effects_get_handle(effect);
                }
                const char* explosion = xml_table_get_opt_string_ref(cur, "Death_Explosion");
                if (explosion != nullptr)
                {
                    info->death_explosion = explosion_lookup(explosion);
                }
                info->spe_flags = (unsigned int)xml_table_get_flags(Script_point_effect_info_flag_names, 9, xml_table_find_req(cur, "Flags"), false);
            }
        }
    }

    obj_prop->m_flags.m_movable_by_humans = true;
    xml_element* non_inherited = xml_table_find_opt(root, "Non_Inherited_Attributes");
    if (non_inherited != nullptr)
    {
        bool movable_by_humans = true;
        xml_table_get_req_bool(movable_by_humans, non_inherited, "Movable_By_Humans");
        obj_prop->m_flags.m_movable_by_humans = movable_by_humans;
        xml_element* custom_effects = xml_table_find_opt(non_inherited, "custom_effects");
        xml_element* cur = custom_effects != nullptr ? xml_table_find_req(custom_effects, "effect") : nullptr;
        for (int count = 0; cur != nullptr && count < MAX_CUSTOM_EFFECTS; count++)
        {
            custom_effect_info& info = obj_prop->m_custom_effect_infos[count];
            char effect_name[1024];
            xml_table_get_req_string(effect_name, sizeof(effect_name), cur, "effect_name");
            info.effect_handle = effects_get_handle(effect_name);
            const char* attachment = xml_table_get_opt_string_ref(cur, "attachment_point");
            if (attachment != nullptr)
            {
                info.attachment_point_name = lopm_copy_attachment_point(attachment, pool); // inlined
            }
            cur = xml_table_find_next(custom_effects, cur, "effect");
        }
    }

    // the defaults of an entry without Non_Destroyable_Attributes
    obj_prop->m_max_hitpoints = 50;
    obj_prop->m_max_dislodge_hitpoints = 1;
    level_object_properties_flags& flags = obj_prop->m_flags;
    flags.m_damaged_by_cars = true;
    flags.m_damaged_by_explosions = true;
    flags.m_damaged_by_bullets = true;
    flags.m_damaged_by_level_objects = true;
    flags.m_damaged_by_melee = true;
    flags.m_dislodged_by_cars = true;
    flags.m_dislodged_by_explosions = true;
    flags.m_dislodged_by_bullets = true;
    flags.m_dislodged_by_level_objects = true;
    flags.m_dislodged_by_melee = true;
    flags.m_allow_dynamic_link = false;
    flags.m_link_affects_effects = false;
    flags.m_dislodge_on_death = false;
    flags.m_generate_human_collision_event = false;
    obj_prop->m_mass = 1000.0f;
    obj_prop->m_corpse_mass = 1000.0f;
    obj_prop->m_physical_material = rfg_get_physical_material_index("DEFAULT");
    obj_prop->m_effect_material = rfg_get_effect_material_index(obj_prop->m_physical_material);
    obj_prop->m_death_effect = 0xFFFFFFFF;
    obj_prop->m_release_effect = 0xFFFFFFFF;
    obj_prop->m_friction = 0.5f;
    obj_prop->m_restitution = 0.05f;
    obj_prop->m_linear_dampening = 0.01f;
    obj_prop->m_angular_dampening = 0.05f;
    obj_prop->m_release_delay_ms = 0;

    xml_element* destroyable = xml_table_find_opt(root, "Non_Destroyable_Attributes");
    if (destroyable == nullptr)
    {
        return true;
    }
    xml_table_get_req_int(obj_prop->m_max_hitpoints, destroyable, "Hitpoints");
    xml_table_get_req_int(obj_prop->m_max_dislodge_hitpoints, destroyable, "Dislodge_Hitpoints");
    if (!lopm_parse_damaged_dislodged_by(obj_prop, destroyable))
    {
        sprintf(Lopm_message, "Failed to find what \"%s\" is affected by, it is required!\n", obj_prop->m_name);
        keen::debug::nativePrintArgs(Lopm_message);
        return false;
    }
    bool dislodge_on_death = false;
    xml_table_get_req_bool(dislodge_on_death, destroyable, "Dislodge_on_death");
    flags.m_dislodge_on_death = dislodge_on_death;
    unsigned int weight = 0;
    xml_table_get_req_uint(weight, destroyable, "Weight");
    obj_prop->m_mass = (float)weight;
    xml_table_get_opt_uint(weight, destroyable, "Corpse_mass"); // defaults to the weight
    obj_prop->m_corpse_mass = (float)weight;
    obj_prop->m_physical_material = rfg_get_physical_material_index(xml_table_get_req_string_ref(destroyable, "Physical_Material"));
    const char* effect_material = xml_table_get_opt_string_ref(destroyable, "Effect_Material");
    obj_prop->m_effect_material = effect_material != nullptr ? rfg_get_effect_material_index(effect_material) : rfg_get_effect_material_index(obj_prop->m_physical_material);
    xml_table_get_opt_float(obj_prop->m_friction, destroyable, "Friction");
    xml_table_get_opt_float(obj_prop->m_restitution, destroyable, "Restitution");
    xml_table_get_opt_float(obj_prop->m_linear_dampening, destroyable, "Linear_dampening");
    xml_table_get_opt_float(obj_prop->m_angular_dampening, destroyable, "Angular_dampening");
    bool allow_dynamic_link = false;
    xml_table_get_req_bool(allow_dynamic_link, destroyable, "Allow_Dynamic_Link");
    flags.m_allow_dynamic_link = allow_dynamic_link;
    bool link_affects_effects = false;
    xml_table_get_req_bool(link_affects_effects, destroyable, "Dynamic_link_affects_effects");
    flags.m_link_affects_effects = link_affects_effects;

    xml_element* death_effect = xml_table_find_opt(destroyable, "Death_Effect");
    if (death_effect != nullptr)
    {
        char effect_name[128];
        xml_table_get_req_string(effect_name, sizeof(effect_name), death_effect, nullptr); // the element's own text
        obj_prop->m_death_effect = effects_get_handle(effect_name);
    }

    xml_element* explosions = xml_table_find_opt(destroyable, "DeathExplosionList");
    if (explosions != nullptr)
    {
        int count = 0;
        xml_element* cur = xml_table_find_req(explosions, "explosion");
        while (cur != nullptr && count < MAX_DEATH_EXPLOSIONS)
        {
            explosion_container& container = obj_prop->m_death_explosions[count];
            char explosion_name[1024];
            xml_table_get_req_string(explosion_name, sizeof(explosion_name), cur, "Death_Explosion");
            container.exp_info = explosion_lookup(explosion_name);
            if (container.exp_info == nullptr)
            {
                continue; // see the notes
            }
            count++;
            const char* attachment = xml_table_get_opt_string_ref(cur, "attachment_point");
            if (attachment != nullptr)
            {
                container.attachment_point_name = lopm_copy_attachment_point(attachment, pool); // inlined
            }
            cur = xml_table_find_next(explosions, cur, "explosion");
        }
        flags.m_death_explosion = count > 0;
    }

    xml_element* prefix = xml_table_find_opt(destroyable, "Prefix_Effects");
    if (prefix != nullptr)
    {
        xml_element* effects = xml_table_find_req(prefix, "Effects");
        xml_element* cur = xml_table_find_opt(effects, "Effect");
        for (int count = 0; cur != nullptr && count < MAX_PREFIX_EFFECTS; count++)
        {
            custom_effect_info& info = obj_prop->m_prefix_effect_infos[count];
            char effect_name[1024];
            xml_table_get_req_string(effect_name, sizeof(effect_name), cur, "Effect_Name");
            info.effect_handle = effects_get_handle(effect_name);
            xml_table_get_req_bool(info.looping, cur, "Looping");
            const char* attachment = xml_table_get_opt_string_ref(cur, "Attachment_Point");
            if (attachment != nullptr)
            {
                info.attachment_point_name = lopm_copy_attachment_point(attachment, pool); // inlined
            }
            cur = xml_table_find_next(effects, cur, "Effect");
        }
        float seconds;
        if (xml_table_get_opt_float(seconds, prefix, "Min_Duration"))
        {
            obj_prop->m_explosion_prefix_min_duration = (int)(seconds * 1000.0f);
        }
        else
        {
            obj_prop->m_explosion_prefix_min_duration = 0;
        }
        if (xml_table_get_opt_float(seconds, prefix, "Max_Duration"))
        {
            obj_prop->m_explosion_prefix_max_duration = (int)(seconds * 1000.0f);
        }
        else
        {
            obj_prop->m_explosion_prefix_min_duration = 0; // see the notes
        }
        if (obj_prop->m_explosion_prefix_max_duration < obj_prop->m_explosion_prefix_min_duration)
        {
            obj_prop->m_explosion_prefix_max_duration = obj_prop->m_explosion_prefix_min_duration;
        }
        if (!xml_table_get_opt_int(obj_prop->m_prefix_hitpoints, prefix, "Prefix_Hitpoints"))
        {
            obj_prop->m_prefix_hitpoints = -1;
        }
    }

    xml_element* release = xml_table_find_opt(destroyable, "Release_Effect");
    if (release != nullptr)
    {
        obj_prop->m_release_effect = effects_get_handle(xml_table_get_req_string_ref(release, "Effect"));
        float seconds;
        xml_table_get_req_float(seconds, release, "Delay");
        obj_prop->m_release_delay_ms = (int)(seconds * 1000.0f);
        xml_table_get_req_float(seconds, release, "Loop");
        obj_prop->m_release_looping_ms = (int)(seconds * 1000.0f);
    }
    return true;
}

// 0x00A37820 (:686)
// The base table, or the DLC table for dlc_id != -1 (missing DLC tables are fine).
static bool lopm_parse_table_file(mempool_base* pool, int dlc_id)
{
    const bool dlc = dlc_id != -1;
    char filename[64];
    if (dlc)
    {
        sprintf(filename, "dlc%02d_%s", dlc_id, "level_objects.xtbl");
        if (!cf_exist(filename))
        {
            return true;
        }
    }
    else
    {
        strcpy(filename, "level_objects.xtbl");
    }
    xml_element* root = xml_table_parse(filename);
    if (root == nullptr)
    {
        return false;
    }
    unsigned int& num = dlc ? Dlc_num_level_object_property_entries : Num_level_object_property_entries;
    num = xml_count(root, "Level_Object");
    if (num == 0)
    {
        xml_table_close(root);
        return true;
    }
    level_object_base_properties* entries = (level_object_base_properties*)pool->alloc(num * sizeof(level_object_base_properties), 0);
    if (dlc)
    {
        Dlc_level_object_base_properties = entries;
    }
    else
    {
        Level_object_base_properties = entries;
    }
    num = 0;
    for (xml_element* cur = xml_table_find_opt(root, "Level_Object"); cur != nullptr; cur = xml_table_find_next(root, cur, "Level_Object"))
    {
        level_object_base_properties prop;
        if (!lopm_parse_level_object_property(&prop, cur, pool))
        {
            keen::debug::nativePrintArgs("Failed to read in an object property!\n");
            return false; // see the notes
        }
        // insertion sort by name
        unsigned int index = 0;
        while (index < num && v_stricmp(prop.m_name, entries[index].m_name) >= 0)
        {
            index++;
        }
        for (unsigned int i = num; i > index; i--)
        {
            entries[i] = entries[i - 1];
        }
        entries[index] = prop;
        num++;
    }
    if (!dlc)
    {
        Default_level_object_base_properties = lopm_get_properties("Default");
    }
    xml_table_close(root);
    return true;
}

// 0x00A37B60 (:808)
void lopm_system_init(mempool_base* pool, int dlc_id)
{
    if (dlc_id == -1)
    {
        Default_level_object_base_properties = nullptr;
    }
    if (lopm_parse_table_file(pool, dlc_id) && Default_level_object_base_properties == nullptr)
    {
        keen::debug::nativePrintArgs("Failed to find the \"Default\" level object properties in the table file!\n");
    }
}

// 0x00A12E20 (:831)
void lopm_dlc_shutdown()
{
    Dlc_num_level_object_property_entries = 0;
    Dlc_level_object_base_properties = nullptr; // see the notes
}

// Binary search of a table sorted by name (inlined twice).
static const level_object_base_properties* lopm_find(const level_object_base_properties* entries, unsigned int num, const char* name)
{
    unsigned int low = 0;
    unsigned int high = num;
    while (low < high)
    {
        const unsigned int mid = (low + high) >> 1;
        const int cmp = v_stricmp(name, entries[mid].m_name);
        if (cmp == 0)
        {
            return &entries[mid];
        }
        if (cmp < 0)
        {
            high = mid;
        }
        else
        {
            low = mid + 1;
        }
    }
    return nullptr;
}

// 0x00A12E30 (:843)
const level_object_base_properties* lopm_get_properties(const char* property_entry_name)
{
    if (property_entry_name == nullptr)
    {
        return Default_level_object_base_properties;
    }
    const level_object_base_properties* props = lopm_find(Level_object_base_properties, Num_level_object_property_entries, property_entry_name);
    if (props != nullptr)
    {
        return props;
    }
    if (Dlc_num_level_object_property_entries != 0 && Dlc_level_object_base_properties != nullptr)
    {
        props = lopm_find(Dlc_level_object_base_properties, Dlc_num_level_object_property_entries, property_entry_name);
        if (props != nullptr)
        {
            return props;
        }
    }
    return Default_level_object_base_properties;
}
