// Original: volition/rfg/code/code_effects/explosion_shared.cpp
//
// Partial: unused_dcf_max_explosion_unique_id (a console command, needs console_printf) is not implemented yet.
//
// Notes (decompiled behaviour, kept as is):
// - explosion_parse_entry stores up to 8 effect handles into m_effects[4] (the extra ones run into
//   m_num_material_effects and m_material_effects, which are reset right after) and up to 9 material effects into
//   arrays of 8. The <Effects> list is read into 8 names without a bound check.
// - ConeAngle is clamped to 0..180 and half_angle_dot is the cosine of the whole angle.
// - explosion_parse_table stops at 79 explosions without closing the table.

#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/objects/salvage.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/string_comp.h"

#include "keen/base/trace.hpp"

#include <math.h>
#include <stdio.h>

unsigned int Num_explosion_infos; // 0x01DEE490

// ---- file-static globals
static explosion_info Explosion_infos[79]; // 0x01DE6CD8
static const char* Explosion_info_flag_names[6] = { "incendiary", "weaken", "no gibs", "ignore projectiles", "destroy if not streamed", "nano dissolve" }; // 0x01648F38
static int Explosions_max_unique_id; // 0x01DEE494

// 0x006E8100 (:104)
static void explosion_parse_entry(explosion_info* e_info, xml_element* root)
{
    xml_table_get_req_string(e_info->m_name, sizeof(e_info->m_name), root, "Name");
    e_info->m_name_crc_str = crc_str(e_info->m_name, 0);
    xml_table_get_req_int(e_info->m_unique_id, root, "Unique_ID");
    if (e_info->m_unique_id < 0 || e_info->m_unique_id > 0xFE)
    {
        keen::debug::nativePrintArgs("explosion_unique id out of range");
    }
    if (e_info->m_unique_id >= Explosions_max_unique_id)
    {
        Explosions_max_unique_id = e_info->m_unique_id;
    }
    e_info->flags = 0;
    xml_element* flags = xml_table_find_opt(root, "Flags");
    if (flags != nullptr)
    {
        e_info->flags = (unsigned int)xml_table_get_flags(Explosion_info_flag_names, 6, flags, false);
    }

    float value;
    xml_table_get_req_float(value, root, "Radius");
    e_info->m_radius = value;
    xml_table_get_req_float(value, root, "CrumbleRadius");
    e_info->m_secondary_radius = value < e_info->m_radius ? e_info->m_radius : value;
    xml_table_get_req_float(value, root, "KnockdownRadius");
    e_info->m_knockdown_radius = value;
    xml_table_get_req_float(value, root, "FlinchRadius");
    e_info->m_flinch_radius = value;
    xml_table_get_req_float(value, root, "AI_Sound_Radius");
    e_info->m_ai_sound_radius = value;

    // Human damage defaults to the vehicle damage (-1).
    xml_table_get_req_uint(e_info->m_vehicle_max_damage_hitpoints, root, "vehicle_damage_max");
    xml_table_get_req_uint(e_info->m_vehicle_min_damage_hitpoints, root, "vehicle_damage_min");
    int human_damage;
    xml_table_get_req_int(human_damage, root, "human_damage_max");
    e_info->m_human_max_damage_hitpoints = human_damage == -1 ? e_info->m_vehicle_max_damage_hitpoints : (unsigned int)human_damage;
    xml_table_get_req_int(human_damage, root, "human_damage_min");
    e_info->m_human_min_damage_hitpoints = human_damage == -1 ? e_info->m_vehicle_min_damage_hitpoints : (unsigned int)human_damage;

    xml_table_get_req_float(e_info->player_damage_mult, root, "player_mult");
    if (0.0f - e_info->player_damage_mult >= 0.0f)
    {
        e_info->player_damage_mult = 0.0f;
    }
    xml_table_get_req_float(e_info->player_veh_damage_mult, root, "player_veh_mult");
    if (0.0f - e_info->player_veh_damage_mult >= 0.0f)
    {
        e_info->player_veh_damage_mult = 0.0f;
    }
    xml_table_get_req_float(e_info->player_vehicle_impulse_mult, root, "player_vehicle_impulse_mult");
    unsigned int impulse;
    xml_table_get_req_uint(impulse, root, "Impulse");
    e_info->m_impulse_magnitude = (float)impulse;
    xml_table_get_req_int(e_info->m_structural_damage, root, "Structural_Damage");
    if (e_info->m_structural_damage == -1)
    {
        e_info->m_structural_damage = (int)e_info->m_impulse_magnitude;
    }
    e_info->expanding_explosion_duration = 0;
    if (xml_table_get_opt_uint(e_info->expanding_explosion_duration, root, "Duration"))
    {
        e_info->flags |= 0x8000;
    }
    e_info->expanding_explosion_delay = 0;
    xml_table_get_opt_uint(e_info->expanding_explosion_delay, root, "Delay");

    char effect_names[8][32];
    const int num_effect_names = xml_table_get_string_list(&effect_names[0][0], 32, 8, xml_table_find_req(root, "Effects"));
    e_info->m_num_effects = 0;
    for (int i = 0; i < num_effect_names; ++i)
    {
        const unsigned int handle = effects_get_handle(effect_names[i]);
        if (handle != 0xFFFFFFFF)
        {
            e_info->m_effects[e_info->m_num_effects++] = handle;
        }
    }

    e_info->m_num_material_effects = 0;
    xml_element* material_effects = xml_table_find_opt(root, "Material_Effects");
    if (material_effects != nullptr)
    {
        for (xml_element* entry = xml_table_find_opt(material_effects, "Entry"); entry != nullptr && e_info->m_num_material_effects < 9; entry = xml_table_find_next(material_effects, entry, "Entry"))
        {
            e_info->m_material_effects[e_info->m_num_material_effects] = effects_get_handle(xml_table_get_req_string_ref(entry, "Effect"));
            e_info->m_material_referenece[e_info->m_num_material_effects] = rfg_get_effect_material_index(xml_table_get_req_string_ref(entry, "Material"));
            ++e_info->m_num_material_effects;
        }
    }

    e_info->salvage_material = salvage_resolve_material_name(xml_table_get_opt_string_ref(root, "SalvageMaterial"));
    xml_table_get_req_int(e_info->salvage_max_pieces, root, "MaxSalvagePieces");
    xml_table_get_req_float(e_info->salvage_probability, root, "SalvageProbability");
    if (!xml_table_get_opt_int(e_info->time_between_breaks, xml_table_find_opt(root, "Break_off_pieces"), "Time_between_breaks"))
    {
        e_info->time_between_breaks = 0;
    }

    e_info->half_angle_dot = 0.0f;
    if (xml_table_get_opt_float(e_info->half_angle_dot, root, "ConeAngle"))
    {
        float angle = e_info->half_angle_dot;
        if (angle - 0.0f < 0.0f)
        {
            angle = 0.0f;
        }
        if (angle - 180.0f >= 0.0f)
        {
            angle = 180.0f;
        }
        e_info->flags |= 0x10000;
        e_info->half_angle_dot = cosf(angle * 0.017453f);
    }
    if (!xml_table_get_opt_float(e_info->blast_decal_radius, root, "Blast_decal_radius"))
    {
        e_info->blast_decal_radius = (e_info->m_radius + e_info->m_secondary_radius) * 0.5f;
    }
    if (!xml_table_get_opt_string(e_info->camera_shake_type, sizeof(e_info->camera_shake_type), root, "camera_shake_type"))
    {
        sprintf(e_info->camera_shake_type, "explosion_small");
    }
    if (!xml_table_get_opt_float(e_info->camera_shake_multiplier, root, "camera_shake_multiplier"))
    {
        e_info->camera_shake_multiplier = 1.0f;
    }
    if (!xml_table_get_opt_float(e_info->camera_shake_falloff, root, "camera_shake_falloff"))
    {
        e_info->camera_shake_falloff = 40.0f;
    }
    xml_table_get_req_bool(e_info->ignore_orientation, root, "Ignore_Orientation");
    xml_table_get_req_bool(e_info->always_ragdoll, root, "Always_Ragdoll");
}

// 0x006E8670 (:317)
void explosion_parse_table(int dlc_id)
{
    // The base table resets the list; a DLC table ("dlcNN_explosions.xtbl") appends to it.
    xml_element* root;
    if (dlc_id == -1)
    {
        root = xml_table_parse("explosions.xtbl");
        Num_explosion_infos = 0;
    }
    else
    {
        char filename[256];
        sprintf(filename, "dlc%02d_%s", dlc_id, "explosions.xtbl");
        root = xml_table_parse(filename);
    }
    for (xml_element* explosion = xml_table_find_opt(root, "Explosion"); explosion != nullptr; explosion = xml_table_find_next(root, explosion, "Explosion"))
    {
        if (Num_explosion_infos >= 79)
        {
            return;
        }
        explosion_parse_entry(&Explosion_infos[Num_explosion_infos], explosion);
        ++Num_explosion_infos;
    }
    xml_table_close(root);
}

// 0x006E2140 (:391)
static void validate_explosion_unique_ids()
{
}

// 0x006E8740 (:418)
void explosion_system_shared_init()
{
    explosion_parse_table(-1);
}

// 0x006E2150 (:433)
const explosion_info* explosion_lookup(const char* explosion_name)
{
    for (unsigned int i = 0; i < Num_explosion_infos; ++i)
    {
        if (v_stricmp(Explosion_infos[i].m_name, explosion_name) == 0)
        {
            return &Explosion_infos[i];
        }
    }
    return nullptr;
}

// 0x006E21A0 (:446)
const explosion_info* explosion_lookup_by_unique_id(int unique_id)
{
    if (unique_id != 0xFF)
    {
        for (unsigned int i = 0; i < Num_explosion_infos; ++i)
        {
            if (Explosion_infos[i].m_unique_id == unique_id)
            {
                return &Explosion_infos[i];
            }
        }
    }
    return nullptr;
}

// 0x006E21E0 (:462)
unsigned char explosion_get_explosion_unique_id(const explosion_info* e_info)
{
    if (e_info == nullptr)
    {
        return 0xFF;
    }
    return (unsigned char)e_info->m_unique_id;
}

// 0x006E21F0 (:478)
unsigned int explosion_get_handle(const char* explosion_name)
{
    for (unsigned int i = 0; i < Num_explosion_infos; ++i)
    {
        if (v_stricmp(Explosion_infos[i].m_name, explosion_name) == 0)
        {
            return i;
        }
    }
    return 0xFFFFFFFF;
}

// 0x006E2230 (:495)
const explosion_info* explosion_lookup(unsigned int handle)
{
    if (handle < Num_explosion_infos)
    {
        return &Explosion_infos[handle];
    }
    return nullptr;
}

// 0x006E2250 (:511)
unsigned int explosion_get_handle_from_ptr(const explosion_info* e_info)
{
    const int handle = (int)(e_info - Explosion_infos);
    if (handle < 0 || handle >= (int)Num_explosion_infos)
    {
        return 0xFFFFFFFF;
    }
    return (unsigned int)handle;
}
