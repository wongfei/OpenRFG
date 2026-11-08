// Original: volition/rfg/code/objects/turret_info.cpp
//
// The turret infos (turrets.xtbl, per DLC dlcNN_turrets.xtbl): field of view and rotation speeds, camera offsets, hit
// points, sounds and effects, the three physical components and the weapons (header comment in turret_info.h).
// Compiled in rfg_objects_unity.cpp.

#include "volition/rfg/code/objects/turret_info.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/string_comp.h"

#include <stdio.h>
#include <string.h>

// Magic numbers: the table is in degrees; the original multiplies in double by the float 0.017453 (3F91DF33_00000000).
static const double TURRET_DEG_TO_RAD = 0.017453f;
// Room left in the Turret_infos array for the turrets of a DLC bundle.
static const int MAX_DLC_TURRET_INFOS = 8;

int Num_turret_infos; // 0x0302F22C
static turret_info* Turret_infos; // 0x0302F228

static float turret_deg_to_rad(float degrees)
{
    return (float)((double)degrees * TURRET_DEG_TO_RAD);
}

// A copy of a table string in the pool.
static char* turret_info_copy_string(mempool* pool, const char* str)
{
    const unsigned int size = (unsigned int)strlen(str) + 1;
    char* dest = (char*)pool->alloc(size, 1);
    strcpy(dest, str);
    return dest;
}

// The flags of the <Flags> list; an unknown one only formats a debug message in the original.
static void parse_turret_flag(turret_info* info, const char* text)
{
    turret_info::turret_info_flags& flags = info->flags;
    if (v_stricmp(text, "Automated") == 0) { flags.automated = true; }
    else if (v_stricmp(text, "Stationary Operator") == 0) { flags.stationary_operator = true; }
    else if (v_stricmp(text, "Manned Vehicle Turret") == 0) { flags.manned_vehicle_turret = true; }
    else if (v_stricmp(text, "No Operator Animation") == 0) { flags.no_operator_animation = true; }
    else if (v_stricmp(text, "Limited Ammo Primary") == 0) { flags.limited_ammo_primary = true; }
    else if (v_stricmp(text, "Limited Ammo Secondary") == 0) { flags.limited_ammo_secondary = true; }
    else if (v_stricmp(text, "Force Fine Aim") == 0) { flags.force_fine_aim = true; }
    else if (v_stricmp(text, "Transform to Tag") == 0) { flags.transform_to_tag = true; }
    else if (v_stricmp(text, "Maintain Constraint Stability") == 0) { flags.maintain_constraint_stability = true; }
    else if (v_stricmp(text, "Sec Use Alt Fire") == 0) { flags.second_weapon_alt_fire = true; }
    else if (v_stricmp(text, "Center on Entry") == 0) { flags.center_on_player_entry = true; }
    else if (v_stricmp(text, "Invulnerable") == 0) { flags.invulnerable = true; }
    else if (v_stricmp(text, "Weapon Physics") == 0) { flags.primary_weapon_physics = true; }
    else if (v_stricmp(text, "Cycle Primary Weapon") == 0) { flags.cycle_primary_weapon = true; }
    else if (v_stricmp(text, "Cycle Secondary Weapon") == 0) { flags.cycle_secondary_weapon = true; }
    else if (v_stricmp(text, "Primary Reset Cycle") == 0) { flags.primary_reset_cycle = true; }
    else if (v_stricmp(text, "Secondary Reset Cycle") == 0) { flags.secondary_reset_cycle = true; }
    else if (v_stricmp(text, "Primary Weapon Local Door") == 0) { flags.primary_weapon_local_door = true; }
    else if (v_stricmp(text, "Secondary Weapon Local Door") == 0) { flags.secondary_weapon_local_door = true; }
    else if (v_stricmp(text, "Primary Weapon All Doors") == 0) { flags.primary_weapon_all_doors = true; }
    else if (v_stricmp(text, "Secondary Weapon All Doors") == 0) { flags.secondary_weapon_all_doors = true; }
    else if (v_stricmp(text, "Secondary Uses Primary Tags") == 0) { flags.secondary_uses_primary_tags = true; }
    else if (v_stricmp(text, "Dont Auto Return Vertical") == 0) { flags.auto_return_dont_vertical = true; }
    else if (v_stricmp(text, "Dont Auto Return Horizontal") == 0) { flags.auto_return_dont_horizontal = true; }
    else if (v_stricmp(text, "Allow Simul Fire") == 0) { flags.allow_simul_fire = true; }
    else if (v_stricmp(text, "Link Overheats") == 0) { flags.link_overheats = true; }
    else if (v_stricmp(text, "Extra Smooth Camera") == 0) { flags.smooth_camera = true; }
    else if (v_stricmp(text, "Gun of Tharsis") == 0) { flags.gun_of_tharsis = true; }
}

// 0x00A1FB70 (:39): info arrives in ESI. A reload keeps the name and the vehicle component names.
static void parse_turret_info(turret_info* info, xml_element* t, bool reload, mempool* pool)
{
    if (!reload)
    {
        const char* name = xml_table_get_req_string_ref(t, "Name");
        const unsigned int size = (unsigned int)strlen(name) + 1;
        char* dest = (char*)pool->alloc(size, 1);
        strncpy_safe(dest, name, size);
        info->name = dest;
        info->name_cksum_stri = checksum_stri(name, 0);
    }
    memset(&info->flags, 0, sizeof(info->flags));
    info->w_info = weapons_get_weapon_info(xml_table_get_req_string_ref(t, "Weapon"));
    // Original: the optional name is passed on without a null check.
    info->weapon_corpse_info = object_item_get_info(xml_table_get_opt_string_ref(t, "Weapon_Corpse"));
    const char* str = xml_table_get_opt_string_ref(t, "SecondaryWeapon");
    info->secondary_w_info = nullptr;
    if (str != nullptr)
    {
        info->secondary_w_info = weapons_get_weapon_info(str);
    }
    str = xml_table_get_opt_string_ref(t, "Turning_loop_cue");
    info->turning_loop_cue_id = -1;
    if (str != nullptr)
    {
        audiolib_cue_get_id(str, &info->turning_loop_cue_id);
    }
    str = xml_table_get_opt_string_ref(t, "Door_Open_Cue");
    info->door_open_cue_id = -1;
    if (str != nullptr)
    {
        audiolib_cue_get_id(str, &info->door_open_cue_id);
    }
    str = xml_table_get_opt_string_ref(t, "Door_Close_Cue");
    info->door_close_cue_id = -1;
    if (str != nullptr)
    {
        audiolib_cue_get_id(str, &info->door_close_cue_id);
    }
    str = xml_table_get_opt_string_ref(t, "Firing_Pattern_Prefix");
    info->firing_pattern_prefix = -1;
    if (str != nullptr)
    {
        audiolib_cue_get_id(str, &info->firing_pattern_prefix);
    }
    str = xml_table_get_opt_string_ref(t, "Primary_Alt_Fire_Effect");
    info->primary_alt_fire_effect = 0xFFFFFFFF;
    if (str != nullptr)
    {
        info->primary_alt_fire_effect = effects_get_handle(str);
    }
    str = xml_table_get_opt_string_ref(t, "Secondary_Alt_Fire_Effect");
    info->secondary_alt_fire_effect = 0xFFFFFFFF;
    if (str != nullptr)
    {
        info->secondary_alt_fire_effect = effects_get_handle(str);
    }

    xml_element* flags = xml_table_find_req(t, "Flags");
    for (xml_element* flag = xml_find(flags, "Flag"); flag != nullptr; flag = xml_find_next(flags, flag, "Flag"))
    {
        parse_turret_flag(info, flag->text);
    }

    // Original bug kept: a reload clears the two vehicle component names, and the !reload checks below keep them unset.
    for (int i = 0; i < 3; i++)
    {
        info->components[i].i_info = nullptr;
        info->components[i].vehicle_component_name = nullptr;
        info->mass[i] = 0.0f;
    }
    info->middle_motor_power = -1.0f; // below 0: assemble_components picks the default power
    info->top_motor_power = -1.0f;

    xml_element* section = xml_table_find_req(t, "TurretBase");
    if (section != nullptr)
    {
        str = xml_table_get_opt_string_ref(section, "Item");
        if (str != nullptr)
        {
            info->components[0].i_info = object_item_get_info(str);
        }
        xml_table_get_opt_float(info->mass[0], section, "Mass");
    }
    section = xml_table_find_req(t, "TurretMiddle");
    if (section != nullptr)
    {
        str = xml_table_get_opt_string_ref(section, "Item");
        if (str != nullptr)
        {
            info->components[1].i_info = object_item_get_info(str);
        }
        str = xml_table_get_opt_string_ref(section, "Component");
        if (str != nullptr && !reload)
        {
            info->components[1].vehicle_component_name = turret_info_copy_string(pool, str);
        }
        xml_table_get_opt_float(info->middle_motor_power, section, "Horsepower");
        xml_table_get_opt_float(info->mass[1], section, "Mass");
    }
    // The top is optional: without an item it is the weapon's mesh (create_item_component).
    section = xml_table_find_opt(t, "TurretTop");
    if (section != nullptr)
    {
        str = xml_table_get_opt_string_ref(section, "Component");
        if (str != nullptr && !reload)
        {
            info->components[2].vehicle_component_name = turret_info_copy_string(pool, str);
        }
        xml_table_get_opt_float(info->top_motor_power, section, "Horsepower");
        xml_table_get_opt_float(info->mass[2], section, "Mass");
    }

    float limit_left = 0.0f;
    float limit_right = 0.0f;
    xml_table_get_opt_float(limit_left, t, "Movement_Limit_Left");
    xml_table_get_opt_float(limit_right, t, "Movement_Limit_Right");
    info->movement_limit_left = turret_deg_to_rad(limit_left);
    info->movement_limit_right = turret_deg_to_rad(limit_right);
    xml_table_get_req_vector(info->default_camera_offset, t, "Default_Offset");
    xml_table_get_req_vector(info->fine_aim_camera_offset, t, "Fine_Aim_Offset");

    float h_fov_pos = 180.0f;
    xml_table_get_opt_float(h_fov_pos, t, "HFovPos");
    float h_fov_neg = -h_fov_pos;
    xml_table_get_opt_float(h_fov_neg, t, "HFovNeg");
    info->h_fov_pos = turret_deg_to_rad(h_fov_pos);
    info->h_fov_neg = turret_deg_to_rad(h_fov_neg);

    float v_fov_pos = 45.0f;
    float v_fov_neg = -45.0f;
    xml_table_get_opt_float(v_fov_pos, t, "VFovPos");
    xml_table_get_opt_float(v_fov_neg, t, "VFovNeg");
    float v_fov_pos_rear = v_fov_pos;
    float v_fov_neg_rear = v_fov_neg;
    float v_fov_pos_right = v_fov_pos;
    float v_fov_neg_right = v_fov_neg;
    float v_fov_pos_left = v_fov_pos;
    float v_fov_neg_left = v_fov_neg;
    bool has_side = xml_table_get_opt_float(v_fov_pos_right, t, "VFovPosRight");
    has_side = xml_table_get_opt_float(v_fov_neg_right, t, "VFovNegRight") || has_side;
    has_side = xml_table_get_opt_float(v_fov_pos_left, t, "VFovPosLeft") || has_side;
    has_side = xml_table_get_opt_float(v_fov_neg_left, t, "VFovNegLeft") || has_side;
    // A side limit also counts as a rear limit (the original carries the flag through the chain).
    bool has_rear = xml_table_get_opt_float(v_fov_pos_rear, t, "VFovPosRear") || has_side;
    has_rear = xml_table_get_opt_float(v_fov_neg_rear, t, "VFovNegRear") || has_rear;
    info->flags.has_side_fov_limits = has_side;
    info->flags.has_rear_fov_limits = has_rear;

    // The blend spline goes left, front, right, rear, left, front, right, so calculate_fov_blending can wrap.
    const vector2 rear(turret_deg_to_rad(v_fov_pos_rear), turret_deg_to_rad(v_fov_neg_rear));
    const vector2 left(turret_deg_to_rad(v_fov_pos_left), turret_deg_to_rad(v_fov_neg_left));
    const vector2 front(turret_deg_to_rad(v_fov_pos), turret_deg_to_rad(v_fov_neg));
    const vector2 right(turret_deg_to_rad(v_fov_pos_right), turret_deg_to_rad(v_fov_neg_right));
    info->v_fov_blend_spline_points[0] = left;
    info->v_fov_blend_spline_points[1] = front;
    info->v_fov_blend_spline_points[2] = right;
    info->v_fov_blend_spline_points[3] = rear;
    info->v_fov_blend_spline_points[4] = left;
    info->v_fov_blend_spline_points[5] = front;
    info->v_fov_blend_spline_points[6] = right;

    float value = 45.0f;
    xml_table_get_opt_float(value, t, "HRotSpeed");
    info->h_rot_speed = turret_deg_to_rad(value);
    value = 30.0f;
    xml_table_get_opt_float(value, t, "VRotSpeed");
    info->v_rot_speed = turret_deg_to_rad(value);
    value = 3.0f;
    xml_table_get_opt_float(value, t, "HRotMaxSpeedMul");
    info->h_rot_max_mul = value;
    value = 2.0f;
    xml_table_get_opt_float(value, t, "VRotMaxSpeedMul");
    info->v_rot_max_mul = value;
    // The ramp times are stored inverted, in double as the original divides.
    value = 0.3f;
    xml_table_get_opt_float(value, t, "HRotMaxRampTime");
    info->h_rot_max_ramp_time = (float)(1.0 / (double)value);
    value = 0.25f;
    xml_table_get_opt_float(value, t, "VRotMaxRampTime");
    info->v_rot_max_ramp_time = (float)(1.0 / (double)value);

    info->recoil_comp = 0.0f;
    xml_table_get_opt_float(info->recoil_comp, t, "RecoilComp");
    unsigned int hit_points = 500;
    xml_table_get_opt_uint(hit_points, t, "Hit Points");
    info->max_hitpoints = hit_points;
}

// 0x00A207A0 (:370)
void turret_info_read_table(bool reload, int dlc_id)
{
    const bool is_dlc = dlc_id != -1;
    int num_dlc_infos = 0;
    mempool* pool;
    xml_element* root;
    if (is_dlc)
    {
        char filename[256];
        pool = &Level_mempool;
        sprintf(filename, "dlc%02d_%s", dlc_id, "turrets.xtbl");
        root = xml_table_parse(filename);
    }
    else
    {
        pool = &Permanent_mempool;
        root = xml_table_parse("turrets.xtbl");
    }
    if (!reload)
    {
        const int count = xml_table_count(root, "Turret");
        if (!is_dlc)
        {
            Num_turret_infos = count;
            Turret_infos = (turret_info*)Permanent_mempool.alloc((count + MAX_DLC_TURRET_INFOS) * sizeof(turret_info), 4);
        }
        else
        {
            num_dlc_infos = count;
            if (count > MAX_DLC_TURRET_INFOS)
            {
                // Original bug kept: the too large table is skipped without xml_table_close.
                return;
            }
        }
    }
    xml_element* t = xml_table_find_opt(root, "Turret");
    // Original bug kept: a DLC reload parses over the first base infos, not over the bundle's.
    turret_info* info = Turret_infos;
    if (is_dlc && !reload)
    {
        info = Turret_infos + Num_turret_infos;
        Num_turret_infos += num_dlc_infos;
    }
    for (; t != nullptr; t = xml_table_find_next(root, t, "Turret"))
    {
        parse_turret_info(info, t, reload, pool);
        info++;
    }
    xml_table_close(root);
}

// 0x00A208F0 (:420)
const turret_info* turret_find_info(const char* name)
{
    if (name == nullptr)
    {
        return nullptr;
    }
    for (int i = 0; i < Num_turret_infos; i++)
    {
        if (v_stricmp(name, Turret_infos[i].name) == 0)
        {
            return &Turret_infos[i];
        }
    }
    return nullptr;
}

// 0x00A20950 (:434)
const turret_info* turret_find_info_from_crc(unsigned int name_crc)
{
    for (int i = 0; i < Num_turret_infos; i++)
    {
        if (Turret_infos[i].name_cksum_stri.get_crc() == name_crc)
        {
            return &Turret_infos[i];
        }
    }
    return nullptr;
}

// 0x00A20990 (:451)
void unused_dcf_turret_reload()
{
    if (Console_command == 0)
    {
        return;
    }
    cf_refresh_file_size("turrets.xtbl");
    turret_info_read_table(true, -1);
    const int dlc_id = dlc_get_override_id();
    if (dlc_id != -1)
    {
        turret_info_read_table(true, dlc_id);
    }
}
