// Original: volition/rfg/code/weapons/weapons_table_parser.cpp
//
// The weapons table (weapons.xtbl, the multiplayer overrides of its "MP Overrides" category and the DLC tables) into
// Weapon_infos (player_minimal.md P3). Compiled in rfg_weapons_unity.cpp.
// The arithmetic follows the binary: the conversions are double expressions of the float constants (0.001f, 0.017453f)
// rounded once to float.
// The external interface is declared in weapons_table_parser.h.

#include "volition/rfg/code/weapons/weapons_table_parser.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/camera/camera_shake.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/weapons/aim_drift.h"
#include "volition/rfg/code/weapons/weapon_firing_patterns.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/trace.hpp"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- file-static globals
static const char* Weapon_info_projectile_flag_names[18] = {"rocket flight", "attach on impact", "detonate on impact", "detonate on explosion impact", "bounce on impact", "attach to ground", "align orient to flightpath", "placeable", "tossable", "MP IED", "dont attach to humans", "pseudoseek", "damage on impact", "air bomb", "use cloud spline", "immediate effect detach", "allow mid air detonation", "attach orient to flight path"}; // 0x016EBC88
static const char* Weapon_trigger_type_names[3] = {"single", "automatic", "charge"}; // 0x016EBCD0
static int Weapons_max_unique_id; // 0x03882CC8

// Magic numbers: the float constants the binary widens to double (milliseconds to seconds, degrees to radians).
static const double MS_TO_SECONDS = (double)0.001f;
static const double DEG_TO_RAD = (double)0.017453f;

// ---- internal functions
// 0x00BDA230 (:132, LTCG: entry in EAX): entries of the "MP Overrides" editor category.
static bool is_mp_override(xml_element* entry)
{
    xml_element* editor = xml_table_find_req(entry, "_Editor");
    if (editor != nullptr)
    {
        const char* category = xml_table_get_req_string_ref(editor, "Category");
        if (category != nullptr)
        {
            return strstr(category, "MP Overrides") != nullptr;
        }
    }
    return false;
}

// 0x00BDA270 (:141): by unique id.
static int weapon_info_compare(const void* p1, const void* p2)
{
    const weapon_info* w1 = (const weapon_info*)p1;
    const weapon_info* w2 = (const weapon_info*)p2;
    if (w1->unique_id < w2->unique_id)
    {
        return -1;
    }
    return w2->unique_id < w1->unique_id ? 1 : 0;
}

// The string copies of parse_weapon_info (pool allocation and strncpy_safe, inlined).
static char* copy_string(mempool* pool, const char* text)
{
    const unsigned int size = (unsigned int)strlen(text) + 1;
    char* copy = (char*)pool->alloc(size, 1);
    strncpy_safe(copy, text, size);
    return copy;
}

// An optional effect of a group: the handle, or invalid when the entry is missing and this is not a refresh.
static void parse_effect(unsigned int& handle, xml_element* root, const char* name, bool refresh, char* buffer, int buffer_size)
{
    if (xml_table_get_opt_string(buffer, buffer_size, root, name))
    {
        handle = effects_get_handle(buffer);
    }
    else if (!refresh)
    {
        handle = 0xFFFFFFFF;
    }
}

// An optional camera shake by name.
static void parse_camera_shake(camera_shake*& shake, xml_element* root, const char* name, bool refresh, char* buffer, int buffer_size)
{
    if (xml_table_get_opt_string(buffer, buffer_size, root, name))
    {
        shake = camera_shake_get(buffer);
    }
    else if (!refresh)
    {
        shake = nullptr;
    }
}

// An optional angle in degrees as radians, 0 when missing and not refreshing.
static void parse_spread(float& value, xml_element* root, const char* name, bool refresh)
{
    if (xml_table_get_opt_float(value, root, name))
    {
        value = (float)((double)value * DEG_TO_RAD);
    }
    else if (!refresh)
    {
        value = 0.0f;
    }
}

// The dot product of half a cone angle in degrees (cos computed in double, as the binary's __CIcos), 1 when missing.
static void parse_cone_dot(float& value, xml_element* root, const char* name, bool refresh)
{
    if (xml_table_get_opt_float(value, root, name))
    {
        const float half_angle = (float)((double)value * 0.5 * DEG_TO_RAD);
        value = (float)cos((double)half_angle);
    }
    else if (!refresh)
    {
        value = 1.0f;
    }
}

// One damage scaling entry (NPC, player, vehicle and player vehicle damage; -1 vehicle damage means the human one).
// Original: Player_Vehicle_Damage of the minimum entry is read from the maximum entry (kept).
static void parse_damage_scaling(damage_scaling_info& scaling, xml_element* root, xml_element* player_vehicle_root)
{
    xml_table_get_req_float(scaling.damage[0], root, "NPC_Damage");
    xml_table_get_req_float(scaling.damage[1], root, "Player_Damage");
    xml_table_get_req_float(scaling.damage[2], root, "Vehicle_Damage");
    xml_table_get_req_float(scaling.damage[3], player_vehicle_root, "Player_Vehicle_Damage");
    if (scaling.damage[2] == -1.0f)
    {
        scaling.damage[2] = scaling.damage[0];
    }
    if (scaling.damage[3] == -1.0f)
    {
        scaling.damage[3] = scaling.damage[1];
    }
}

// 0x00BDDD60 (:161, LTCG: pool in ECX, info in ESI)
// One Weapon entry. A refresh re-parses an existing entry of the same name (case sensitive) and keeps what the
// entry does not set; otherwise the defaults are set too.
static bool parse_weapon_info(mempool* pool, weapon_info* info, xml_element* root, const char* name, bool refresh)
{
    if (refresh)
    {
        if (strcmp(info->name, name) != 0)
        {
            return false;
        }
    }
    else
    {
        info->name = copy_string(pool, name);
    }
    info->name_crc = checksum_stri(info->name, 0);
    int unique_id = 0;
    xml_table_get_req_int(unique_id, root, "Unique_ID");
    // Magic number: the unique ids are 0-254 (one byte in the multiplayer packets).
    if ((unsigned int)unique_id > 0xFE)
    {
        keen::debug::nativePrintArgs("Invalid weapon unique id");
    }
    info->unique_id = unique_id;
    if (Weapons_max_unique_id <= unique_id)
    {
        Weapons_max_unique_id = unique_id;
    }
    bool dummy = false;
    xml_table_get_req_bool(dummy, root, "dummy");
    info->weapon_inv_item_info = nullptr;
    info->weapon_obj_item_info = nullptr;
    if (!dummy)
    {
        info->weapon_inv_item_info = inv_item_get_info(xml_table_get_opt_string_ref(root, "Inventory_Item"));
        info->weapon_obj_item_info = object_item_get_info(xml_table_get_opt_string_ref(root, "Item_3d"));
    }
    const char* default_team = xml_table_get_opt_string_ref(root, "Default_team");
    info->default_team = default_team != nullptr ? human_team_from_name(default_team) : HUMAN_TEAM_NONE;
    if (!refresh)
    {
        info->icon_name = nullptr;
        const char* text = xml_table_get_opt_string_ref(root, "icon_name");
        if (text != nullptr)
        {
            info->icon_name = copy_string(pool, text);
        }
        info->small_icon_name = nullptr;
        text = xml_table_get_opt_string_ref(root, "small_icon_name");
        if (text != nullptr)
        {
            info->small_icon_name = copy_string(pool, text);
        }
        info->reticule_name = nullptr;
        text = xml_table_get_opt_string_ref(root, "reticule_name");
        if (text != nullptr)
        {
            info->reticule_name = copy_string(pool, text);
        }
        info->fine_aim_reticule_name = nullptr;
        text = xml_table_get_opt_string_ref(root, "fine_aim_reticule");
        if (text != nullptr)
        {
            info->fine_aim_reticule_name = copy_string(pool, text);
        }
        info->mp_kill_phrase = nullptr;
        text = xml_table_get_opt_string_ref(root, "mp_kill_phrase");
        if (text != nullptr)
        {
            info->mp_kill_phrase = copy_string(pool, text);
        }
    }
    info->melee_group_index = rfg_get_melee_group_index(xml_table_get_opt_string_ref(root, "Melee_Group"));
    info->bullet_group_index = rfg_get_bullet_group_index(xml_table_get_opt_string_ref(root, "Bullet_Group"));
    if (!refresh)
    {
        info->flags = weapon_info_flags();
    }
    xml_element* flags = xml_table_find_opt(root, "Flags");
    if (flags != nullptr)
    {
        for (xml_element* flag = xml_find(flags, "Flag"); flag != nullptr; flag = xml_find_next(flags, flag, "Flag"))
        {
            const char* text = flag->text;
            if (v_stricmp(text, "armor piercing") == 0)
            {
                info->flags.armor_piercing = true;
            }
            else if (v_stricmp(text, "can fine aim") == 0)
            {
                info->flags.can_fine_aim = true;
            }
            else if (v_stricmp(text, "can scope") == 0)
            {
                info->flags.can_scope = true;
            }
            else if (v_stricmp(text, "shatter") == 0)
            {
                info->flags.shatter = true;
            }
            else if (v_stricmp(text, "held in left hand") == 0)
            {
                info->flags.held_in_left_hand = true;
            }
            else if (v_stricmp(text, "draw projectile in opposite hand") == 0)
            {
                info->flags.draw_proj_in_opposite_hand = true;
            }
            else if (v_stricmp(text, "silent bullets") == 0)
            {
                info->flags.silent_bullets = true;
            }
            else if (v_stricmp(text, "penetrating bullets") == 0)
            {
                info->flags.penetrating_bullets = true;
            }
            else if (v_stricmp(text, "non-inventory") == 0)
            {
                info->flags.non_inventory = true;
            }
            else if (v_stricmp(text, "use even spread") == 0)
            {
                info->flags.use_even_spread = true;
            }
            else if (v_stricmp(text, "disable player cover") == 0)
            {
                info->flags.disable_player_cover = true;
            }
            else if (v_stricmp(text, "mp selectable") == 0)
            {
                info->flags.mp_selectable = true;
            }
            else if (v_stricmp(text, "is obvious weapon") == 0)
            {
                info->flags.is_obvious_weapon = true;
            }
            else if (v_stricmp(text, "auto-aim curved trail") == 0)
            {
                info->flags.auto_aim_curved_trail = true;
            }
            else if (v_stricmp(text, "looping effects") == 0)
            {
                info->flags.looping_effects = true;
            }
            else if (v_stricmp(text, "never in cabinet") == 0)
            {
                info->flags.never_in_cabinet = true;
            }
            else if (v_stricmp(text, "use secondary weapon heat") == 0)
            {
                info->flags.use_secondary_weapon_heat = true;
            }
            else if (v_stricmp(text, "dont drop on death") == 0)
            {
                info->flags.dont_drop_on_death = true;
            }
        }
    }
    info->weapon_class = (weapon_class_type)xml_table_get_type_from_list(Weapon_class_names, 17, xml_table_find_req(root, "Weapon_Class"));
    if (!refresh)
    {
        info->weapon_anim_group = vanim_lookup_group(xml_table_get_req_string_ref(root, "Animation_Group"));
    }
    else if (xml_table_get_opt_string_ref(root, "Animation_Group") != nullptr)
    {
        info->weapon_anim_group = vanim_lookup_group(xml_table_get_opt_string_ref(root, "Animation_Group"));
    }
    if (!xml_table_get_opt_float(info->player_move_speed_multiplier, root, "Player_Move_Speed") && !refresh)
    {
        info->player_move_speed_multiplier = 1.0f;
    }
    if (!xml_table_get_opt_float(info->npc_move_speed_multiplier, root, "NPC_Move_Speed") && !refresh)
    {
        info->npc_move_speed_multiplier = 1.0f;
    }

    // The original reads the strings into a thread-local 0x400 byte buffer.
    char buffer[0x400];
    xml_element* visuals = xml_table_find_opt(root, "Visuals");
    if (visuals == nullptr)
    {
        if (!refresh)
        {
            // Original: overheated_effect, tracer_effect and player_hit_camera_shake keep their zeroed values here.
            info->muzzle_flash_effect = 0xFFFFFFFF;
            info->muzzle_smoke_effect = 0xFFFFFFFF;
            info->special_hit_effect = 0xFFFFFFFF;
            info->special_effect = 0xFFFFFFFF;
            info->secondary_special_effect = 0xFFFFFFFF;
            info->fire_camera_shake = nullptr;
            info->secondary_camera_shake = nullptr;
            info->attachment_point[0] = '\0';
            info->tracer_frequency = 0;
        }
    }
    else
    {
        parse_effect(info->muzzle_flash_effect, visuals, "Muzzle_Flash", refresh, buffer, sizeof(buffer));
        parse_effect(info->muzzle_smoke_effect, visuals, "Muzzle_Smoke", refresh, buffer, sizeof(buffer));
        parse_effect(info->special_hit_effect, visuals, "Special_Hit_Effect", refresh, buffer, sizeof(buffer));
        parse_effect(info->special_effect, visuals, "Special_Effect", refresh, buffer, sizeof(buffer));
        parse_effect(info->secondary_special_effect, visuals, "Secondary_Special_Effect", refresh, buffer, sizeof(buffer));
        parse_effect(info->overheated_effect, visuals, "Overheated_effect", refresh, buffer, sizeof(buffer));
        parse_camera_shake(info->fire_camera_shake, visuals, "Fire_Camera_Shake", refresh, buffer, sizeof(buffer));
        if (!xml_table_get_opt_bool(info->fire_camera_shake_ignore_disabled, visuals, "Fire_Camera_Shake_Ignore_Disabled"))
        {
            info->fire_camera_shake_ignore_disabled = false;
        }
        parse_camera_shake(info->secondary_camera_shake, visuals, "Secondary_Camera_Shake", refresh, buffer, sizeof(buffer));
        parse_camera_shake(info->player_hit_camera_shake, visuals, "Player_Hit_Camera_Shake", refresh, buffer, sizeof(buffer));
        if (xml_table_get_opt_string(buffer, sizeof(buffer), visuals, "Attachment_prop"))
        {
            // Original: an unbounded copy into the 32 byte attachment_point (kept).
            strcpy(info->attachment_point, buffer);
        }
        else if (!refresh)
        {
            info->attachment_point[0] = '\0';
        }
        unsigned int tracer_frequency = 0;
        if (xml_table_get_opt_uint(tracer_frequency, visuals, "Tracer_Frequency"))
        {
            info->tracer_frequency = (unsigned char)tracer_frequency;
        }
        else if (!refresh)
        {
            info->tracer_frequency = 0;
        }
        parse_effect(info->tracer_effect, visuals, "Tracer_Effect", refresh, buffer, sizeof(buffer));
    }
    if (!refresh)
    {
        info->fire_sound = -1;
        info->secondary_sound = -1;
        info->upgrade_sound = -1;
        info->reload_sound = -1;
        info->reload_sound_delay = 0;
        info->no_ammo_sound = -1;
        info->special_sound = -1;
        info->flyby_sound = -1;
        info->firing_sound_radius = 0.0f;
        info->num_weapon_personas = 0;
    }
    xml_element* sounds = xml_table_find_opt(root, "Sounds");
    if (sounds != nullptr)
    {
        if (xml_table_get_opt_string(buffer, sizeof(buffer), sounds, "Fire_Sound"))
        {
            audiolib_cue_get_id(buffer, &info->fire_sound);
        }
        if (xml_table_get_opt_string(buffer, sizeof(buffer), sounds, "Secondary_Sound"))
        {
            audiolib_cue_get_id(buffer, &info->secondary_sound);
        }
        if (xml_table_get_opt_string(buffer, sizeof(buffer), sounds, "Upgrade_Sound"))
        {
            audiolib_cue_get_id(buffer, &info->upgrade_sound);
        }
        if (xml_table_get_opt_string(buffer, sizeof(buffer), sounds, "Special_Sound"))
        {
            audiolib_cue_get_id(buffer, &info->special_sound);
        }
        if (xml_table_get_opt_string(buffer, sizeof(buffer), sounds, "Reload_Sound"))
        {
            audiolib_cue_get_id(buffer, &info->reload_sound);
        }
        xml_table_get_opt_int(info->reload_sound_delay, sounds, "Reload_Sound_Delay");
        if (xml_table_get_opt_string(buffer, sizeof(buffer), sounds, "No_Ammo_Sound"))
        {
            audiolib_cue_get_id(buffer, &info->no_ammo_sound);
        }
        if (xml_table_get_opt_string(buffer, sizeof(buffer), sounds, "Flyby_Sound"))
        {
            audiolib_cue_get_id(buffer, &info->flyby_sound);
        }
        xml_table_get_opt_float(info->firing_sound_radius, sounds, "Sound_Radius");
        xml_element* npc_sounds = xml_find(sounds, "NPC_Fire_Sounds");
        if (npc_sounds != nullptr)
        {
            info->num_weapon_personas = 0;
            // Original: up to 17 cues into the 16 entries of npc_fire_sounds (kept; the 17th would land in max_range,
            // which is read later).
            for (xml_element* cue = npc_sounds->elements; cue != nullptr && info->num_weapon_personas < 17; cue = cue->next)
            {
                audiolib_cue_get_id(cue->text, &info->npc_fire_sounds[info->num_weapon_personas]);
                if (info->npc_fire_sounds[info->num_weapon_personas] != -1)
                {
                    info->num_weapon_personas++;
                }
            }
        }
    }
    xml_element* trigger_type = xml_table_find_req(root, "Trigger_Type");
    if (trigger_type != nullptr)
    {
        info->trigger_type = (weapon_trigger_type)xml_table_get_type_from_list(Weapon_trigger_type_names, 3, trigger_type);
    }
    xml_element* ammo_type = xml_table_find_req(root, "Ammo_Type");
    if (ammo_type != nullptr)
    {
        info->ammo_type = (weapon_ammo_type)xml_table_get_type_from_list(Weapon_ammo_type_names, 8, ammo_type);
    }
    unsigned int value = 0;
    xml_table_get_req_uint(value, root, "Magazine_Size");
    info->magazine_size = (unsigned short)value;
    xml_table_get_req_uint(value, root, "Num_Magazines");
    info->magazine_start_num = (unsigned short)value;
    xml_table_get_req_uint(value, root, "Max_Rounds");
    info->max_rounds = (unsigned short)value;
    info->max_rounds_upgrade = 0;
    if (xml_table_get_opt_uint(value, root, "Max_Rounds_Upgrade"))
    {
        info->max_rounds_upgrade = (unsigned short)value;
    }
    xml_table_get_req_uint(value, root, "Ammo_Box_Restock");
    info->ammo_box_restock = (unsigned short)value;
    xml_table_get_req_uint(value, root, "Default_Refire_Delay");
    info->default_refire_delay = (float)((double)value * MS_TO_SECONDS);
    value = 0;
    xml_table_get_opt_uint(value, root, "Prefire_Delay");
    info->prefire_delay = (float)((double)value * MS_TO_SECONDS);
    if (xml_table_get_opt_uint(value, root, "NPC_Refire_Delay"))
    {
        if (value == 0)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            value = 1;
        }
        info->npc_refire_delay = (float)((double)value * MS_TO_SECONDS);
    }
    else
    {
        info->npc_refire_delay = info->default_refire_delay;
    }
    info->default_reload_delay = xml_table_get_opt_uint(value, root, "Default_Reload_Delay") ? (int)value : 0;
    xml_element* melee_attacks = xml_table_find_opt(root, "MeleeAttacks");
    if (melee_attacks == nullptr)
    {
        if (!refresh)
        {
            info->standing_primary_melee_attack = INVALID_MELEE_COMBAT_MOVE_ID;
            info->standing_secondary_melee_attack = INVALID_MELEE_COMBAT_MOVE_ID;
            info->standing_tertiary_melee_attack = INVALID_MELEE_COMBAT_MOVE_ID;
            info->crouching_primary_melee_attack = INVALID_MELEE_COMBAT_MOVE_ID;
            info->crouching_secondary_melee_attack = INVALID_MELEE_COMBAT_MOVE_ID;
            info->crouching_tertiary_melee_attack = INVALID_MELEE_COMBAT_MOVE_ID;
        }
    }
    else
    {
        info->standing_primary_melee_attack = human_combat_lookup_combat_move(xml_table_get_opt_string_ref(melee_attacks, "StandingPrimary"));
        info->standing_secondary_melee_attack = human_combat_lookup_combat_move(xml_table_get_opt_string_ref(melee_attacks, "StandingSecondary"));
        info->standing_tertiary_melee_attack = human_combat_lookup_combat_move(xml_table_get_opt_string_ref(melee_attacks, "StandingTertiary"));
        info->crouching_primary_melee_attack = human_combat_lookup_combat_move(xml_table_get_opt_string_ref(melee_attacks, "CrouchingPrimary"));
        info->crouching_secondary_melee_attack = human_combat_lookup_combat_move(xml_table_get_opt_string_ref(melee_attacks, "CrouchingSecondary"));
        info->crouching_tertiary_melee_attack = human_combat_lookup_combat_move(xml_table_get_opt_string_ref(melee_attacks, "CrouchingTertiary"));
    }
    if (xml_table_get_opt_string(buffer, sizeof(buffer), root, "Explosion"))
    {
        info->m_explosion_info = explosion_lookup(buffer);
    }
    else if (!refresh)
    {
        info->m_explosion_info = nullptr;
    }
    if (xml_table_get_opt_string(buffer, sizeof(buffer), root, "AIExplosion"))
    {
        info->m_ai_explosion_info = explosion_lookup(buffer);
    }
    else if (!refresh)
    {
        info->m_ai_explosion_info = nullptr;
    }
    parse_cone_dot(info->fire_cone_dot, root, "Fire_Cone_Angle", refresh);
    parse_cone_dot(info->even_spread_accuracy_dot, root, "Even_Spread_Accuracy_Angle", refresh);
    if (xml_table_get_opt_uint(value, root, "Shots_Per_Round"))
    {
        info->shots_per_round = (unsigned char)value;
    }
    else if (!refresh)
    {
        info->shots_per_round = 1;
    }
    parse_spread(info->max_spread, root, "Spread_Max", refresh);
    parse_spread(info->min_spread, root, "Spread_Min", refresh);
    if (xml_table_get_opt_float(info->fine_aim_max_spread, root, "Spread_Fine_Aim_Max"))
    {
        info->fine_aim_max_spread = (float)((double)info->fine_aim_max_spread * DEG_TO_RAD);
    }
    else if (!refresh)
    {
        info->fine_aim_max_spread = info->max_spread;
    }
    if (xml_table_get_opt_float(info->fine_aim_min_spread, root, "Spread_Fine_Aim_Min"))
    {
        info->fine_aim_min_spread = (float)((double)info->fine_aim_min_spread * DEG_TO_RAD);
    }
    else if (!refresh)
    {
        info->fine_aim_min_spread = info->min_spread;
    }
    parse_spread(info->npc_max_spread, root, "NPC_Spread_Max", refresh);
    parse_spread(info->npc_min_spread, root, "NPC_Spread_Min", refresh);
    if (xml_table_get_opt_uint(value, root, "To_Spread_Max"))
    {
        info->to_max_spread = (unsigned short)value;
    }
    else if (!refresh)
    {
        info->to_max_spread = 0;
    }
    if (xml_table_get_opt_uint(value, root, "To_Spread_Min"))
    {
        info->to_min_spread = (unsigned short)value;
    }
    else if (!refresh)
    {
        info->to_min_spread = 0;
    }
    if (!xml_table_get_opt_float(info->spread_multiplier_run, root, "Spread_Multiplier_Run"))
    {
        info->spread_multiplier_run = 1.0f;
    }
    if (!xml_table_get_opt_float(info->ragdoll_force_shoot, root, "Ragdoll_Force_Shoot"))
    {
        info->ragdoll_force_shoot = 0.0f;
    }
    if (!xml_table_get_opt_float(info->ragdoll_chance, root, "Ragdoll_Chance"))
    {
        info->ragdoll_chance = 0.0f;
    }
    if (!xml_table_get_opt_int(info->out_of_ammo_reload_delay, root, "Reload_Delay"))
    {
        info->out_of_ammo_reload_delay = 0;
    }
    if (!xml_table_get_opt_float(info->recoil_camera_kick, root, "Recoil_Kick"))
    {
        info->recoil_camera_kick = 0.0f;
    }
    if (xml_table_get_opt_float(info->recoil_impulse, root, "RecoilImpulse"))
    {
        // Magic number: the table's impulse is in thousands.
        info->recoil_impulse = (float)((double)info->recoil_impulse * 1000.0);
    }
    else
    {
        info->recoil_impulse = 0.0f;
    }
    if (!xml_table_get_opt_float(info->overheat_cool_down_time, root, "Overheat_Cool_Down_Time"))
    {
        info->overheat_cool_down_time = 0.0f;
    }
    if (!xml_table_get_opt_float(info->overheat_percent_per_shot, root, "Overheat_Percent_Per_Shot"))
    {
        info->overheat_percent_per_shot = 0.0f;
    }
    if (!xml_table_get_opt_float(info->headshot_multiplier, root, "Headshot_Multiplier"))
    {
        info->headshot_multiplier = 4.0f; // magic number: the default headshot multiplier
    }
    if (!xml_table_get_opt_float(info->zoom_magnification, root, "Zoom_Magnification"))
    {
        info->zoom_magnification = 1.6f; // magic number: the default zoom
    }
    // Stored as the field of view factor.
    info->zoom_magnification = (float)(1.0 / (double)info->zoom_magnification);
    if (!xml_table_get_opt_float(info->autoaim_override, root, "Autoaim_override"))
    {
        info->autoaim_override = 0.0f;
    }
    if (!xml_table_get_opt_float(info->npc_autoaim, root, "NPC_autoaim"))
    {
        info->npc_autoaim = 0.0f;
    }
    xml_table_get_req_float(info->aim_assist, root, "aim_assist");
    if (!xml_table_get_opt_float(info->dropped_ammo_scale, root, "Dropped_ammo_scale"))
    {
        info->dropped_ammo_scale = 1.0f;
    }
    if (!xml_table_get_opt_float(info->bullet_hole_scale, root, "Bullet_hole_scale") && !refresh)
    {
        info->bullet_hole_scale = 1.0f;
    }
    if (!xml_table_get_opt_float(info->alert_multiplier, root, "Alert_Level_Multiplier"))
    {
        info->alert_multiplier = 1.0f;
    }
    xml_table_get_req_float(info->max_range, root, "Range_Max");
    xml_table_get_req_float(info->red_range, root, "Range_Red");
    xml_table_get_req_float(info->min_engagement_dist, root, "Min_Engagement_Distance");
    xml_table_get_req_float(info->max_engagement_dist, root, "Max_Engagement_Distance");
    info->max_ai_penetrating_dist = 0.0f;
    if (info->flags.penetrating_bullets)
    {
        xml_table_get_opt_float(info->max_ai_penetrating_dist, root, "Max_AI_Penetrating_Distance");
    }
    xml_element* scaling_max = xml_table_find_req(root, "Damage_Scaling_Max");
    if (scaling_max != nullptr)
    {
        if (!xml_table_get_opt_float(info->high_scale_damage.scale_value, scaling_max, "Threshold") && !refresh)
        {
            info->high_scale_damage.scale_value = info->max_range;
        }
        parse_damage_scaling(info->high_scale_damage, scaling_max, scaling_max);
    }
    xml_element* scaling_min = xml_table_find_opt(root, "Damage_Scaling_Min");
    if (scaling_min == nullptr)
    {
        if (!refresh)
        {
            info->low_scale_damage = info->high_scale_damage;
        }
    }
    else
    {
        if (!xml_table_get_opt_float(info->low_scale_damage.scale_value, scaling_min, "Threshold") && !refresh)
        {
            info->low_scale_damage.scale_value = 0.0f;
        }
        parse_damage_scaling(info->low_scale_damage, scaling_min, scaling_max);
    }
    info->npc_firing_pattern = nullptr;
    const char* firing_pattern = xml_table_get_opt_string_ref(root, "NPC_Firing_Pattern");
    if (firing_pattern != nullptr)
    {
        info->npc_firing_pattern = weapon_firing_pattern_lookup_internal(firing_pattern, Num_weapon_firing_patterns);
    }
    if (info->npc_firing_pattern == nullptr)
    {
        info->npc_firing_pattern = weapon_firing_pattern_lookup_internal("Other", Num_weapon_firing_patterns);
        if (info->npc_firing_pattern == nullptr)
        {
            keen::debug::nativePrintArgs("Unable to load default NPC firing pattern!");
        }
    }
    info->npc_aim_drift_profile = nullptr;
    const char* aim_drift = xml_table_get_opt_string_ref(root, "NPC_Aim_Drift");
    if (aim_drift != nullptr)
    {
        info->npc_aim_drift_profile = aim_drift_profile_from_name_internal(aim_drift);
    }
    if (info->npc_aim_drift_profile == nullptr)
    {
        info->npc_aim_drift_profile = aim_drift_profile_from_name_internal("Default");
        if (info->npc_aim_drift_profile == nullptr)
        {
            keen::debug::nativePrintArgs("Unable to find default NPC aim drift profile!");
        }
    }

    // Projectiles of launched and thrown weapons (required unless refreshing).
    xml_element* projectile = nullptr;
    if ((info->ammo_type == WEAPON_AMMO_TYPE_PROJECTILE || info->ammo_type == WEAPON_AMMO_TYPE_THROWN) && !dummy)
    {
        projectile = refresh ? xml_table_find_opt(root, "Projectile_Info") : xml_table_find_req(root, "Projectile_Info");
    }
    if (!refresh)
    {
        info->projectile_info.flags = 0;
    }
    if (projectile == nullptr)
    {
        return true;
    }
    xml_table_get_req_string(buffer, sizeof(buffer), projectile, "Model");
    info->projectile_info.obj_item_info = object_item_get_info(buffer);
    item_info* model = info->projectile_info.obj_item_info;
    if (model == nullptr)
    {
        return false;
    }
    if (!xml_table_get_opt_float(info->projectile_info.start_speed, projectile, "Start_Speed") && !refresh)
    {
        info->projectile_info.start_speed = 0.0f;
    }
    xml_table_get_req_float(info->projectile_info.max_speed, projectile, "Max_Speed");
    if (!xml_table_get_opt_float(info->projectile_info.acceleration, projectile, "Acceleration") && !refresh)
    {
        info->projectile_info.acceleration = 0.0f;
    }
    if (xml_table_get_opt_uint(value, projectile, "Fuse_Time"))
    {
        info->projectile_info.fuse_time = (unsigned short)value;
        // The distance covered at the start speed until the fuse (milliseconds) runs out.
        info->projectile_info.max_throw_dist = (float)((double)value * (double)info->projectile_info.start_speed / 1000.0);
    }
    else if (!refresh)
    {
        info->projectile_info.fuse_time = 0;
        info->projectile_info.max_throw_dist = 50.0f; // magic number: the default throw distance
    }
    const bool has_gravity = xml_table_get_opt_float(info->projectile_info.gravity, projectile, "Gravity");
    if (!refresh)
    {
        if (!has_gravity)
        {
            info->projectile_info.gravity = -9.8f; // magic number: gravity
        }
        info->projectile_info.sound = -1;
    }
    if (xml_table_get_opt_string(buffer, sizeof(buffer), projectile, "Sound"))
    {
        audiolib_cue_get_id(buffer, &info->projectile_info.sound);
    }
    parse_effect(info->projectile_info.effect1, projectile, "Attached_Effect", refresh, buffer, sizeof(buffer));
    parse_effect(info->projectile_info.effect2, projectile, "Attached_Effect2", refresh, buffer, sizeof(buffer));
    parse_effect(info->projectile_info.effect3, projectile, "Attached_Effect3", refresh, buffer, sizeof(buffer));
    parse_effect(info->projectile_info.effect4, projectile, "Attached_Effect4", refresh, buffer, sizeof(buffer));
    // The physics of the projectile's item come from here.
    if (!xml_table_get_opt_float(model->mass, projectile, "Mass") && !refresh)
    {
        model->mass = 1.0f;
    }
    if (!xml_table_get_opt_float(model->linear_damping, projectile, "Linear_Damp") && !refresh)
    {
        model->linear_damping = 0.0f;
    }
    if (!xml_table_get_opt_float(model->angular_damping, projectile, "Angular_Damp") && !refresh)
    {
        model->angular_damping = 0.0f;
    }
    if (!xml_table_get_opt_float(model->restitution, projectile, "Restitution") && !refresh)
    {
        model->restitution = 0.4f;
    }
    if (!xml_table_get_opt_float(model->friction, projectile, "Friction") && !refresh)
    {
        model->friction = 0.4f;
    }
    if (!xml_table_get_opt_float(info->projectile_info.inaccurate_flight, projectile, "Inaccurate_flight") && !refresh)
    {
        info->projectile_info.inaccurate_flight = 0.0f;
    }
    xml_element* angular_velocity = xml_table_find_opt(projectile, "Angular_Velocity");
    if (angular_velocity == nullptr)
    {
        if (!refresh)
        {
            model->angular_velocity.zero();
        }
    }
    else
    {
        xml_table_get_vector(model->angular_velocity, angular_velocity);
    }
    xml_element* projectile_flags = xml_table_find_opt(projectile, "Projectile_Flags");
    if (projectile_flags != nullptr)
    {
        info->projectile_info.flags = (unsigned int)xml_table_get_flags(Weapon_info_projectile_flag_names, 18, projectile_flags, false);
    }
    // Magic numbers: the default propulsion and drop times (milliseconds).
    if (!xml_table_get_opt_float(info->projectile_info.time_until_propelled, projectile, "Time_Until_Propelled") && !refresh)
    {
        info->projectile_info.time_until_propelled = 0.0f;
    }
    if (!xml_table_get_opt_float(info->projectile_info.time_until_prop_expire, projectile, "Time_Until_Prop_Expire") && !refresh)
    {
        info->projectile_info.time_until_prop_expire = 1500.0f;
    }
    if (!xml_table_get_opt_float(info->projectile_info.time_until_drop, projectile, "Time_Until_Drop") && !refresh)
    {
        info->projectile_info.time_until_drop = 2000.0f;
    }
    parse_effect(info->projectile_info.damage_effect, projectile, "Damage_effect", refresh, buffer, sizeof(buffer));
    const char* hit_camera_shake = xml_table_get_opt_string_ref(projectile, "hit_camera_shake");
    if (hit_camera_shake != nullptr)
    {
        info->projectile_info.hit_camera_shake = camera_shake_get(hit_camera_shake);
    }
    else if (!refresh)
    {
        info->projectile_info.hit_camera_shake = nullptr;
    }
    return true;
}

// 0x00BDA290 (:1118): checks only in debug builds.
static void check_weapon_name_crc_collisions()
{
}

// 0x00BDA2A0 (:1135): checks only in debug builds.
static void check_weapon_unique_ids()
{
}

// 0x00BDA2B0 (:1157): projectiles fly straight in multiplayer.
static void set_inaccuracy_for_multiplayer()
{
    if (Multiplayer)
    {
        for (int i = 0; i < Num_weapon_infos; i++)
        {
            Weapon_infos[i].projectile_info.inaccurate_flight = 0.0f;
        }
    }
}

// 0x00BDA2F0 (:1170): links the inventory items to their weapons (an item may serve one weapon, except the generic one).
static void assign_inv_item_w_infos(int starting_index)
{
    for (int i = starting_index; i < Num_weapon_infos; i++)
    {
        weapon_info* w_info = &Weapon_infos[i];
        inv_item_info* inv_info = w_info->weapon_inv_item_info;
        if (inv_info == nullptr)
        {
            continue;
        }
        if (inv_info->w_info == nullptr || inv_info->w_info == w_info || v_stricmp(inv_info->name, "generic_inventory_item") == 0)
        {
            inv_info->w_info = w_info;
        }
        else
        {
            char message[256];
            sprintf(message, "Weapon with name \"%s\" is trying to use inventory item info \"%s\", but that is already being used by weapon \"%s\"!\r\n", w_info->name,
                    inv_info->name, inv_info->w_info->name);
            keen::debug::nativePrintArgs(message);
        }
    }
}

// ---- functions
// 0x00BE4450 (:102): the console command; not ported (no console).

// 0x00BDF8A0 (:1202)
// weapons.xtbl (or a DLC's dlcNN_weapons.xtbl, appended in the level pool). The base table allocates room for 16 more
// weapons and sorts the weapons by unique id. mp_override parses only the "MP Overrides" entries, as a refresh.
void weapons_read_table(bool mp_override, bool refresh, int dlc_id)
{
    const bool dlc = dlc_id != -1;
    if (mp_override)
    {
        if (!refresh)
        {
            return;
        }
    }
    else
    {
        aim_drift_read_table(refresh, dlc_id);
    }
    char filename[256];
    mempool* pool;
    if (dlc)
    {
        pool = &Level_mempool;
        sprintf(filename, "dlc%02d_%s", dlc_id, "weapons.xtbl");
    }
    else
    {
        pool = &Permanent_mempool;
        strcpy(filename, "weapons.xtbl");
    }
    xml_element* table = xml_table_parse(filename);
    const int start_index = dlc ? Num_weapon_infos : 0;
    int num_weapons = start_index;
    if (!refresh)
    {
        if (!dlc)
        {
            Num_weapon_infos = 0;
        }
        for (xml_element* entry = xml_find(table, "Weapon"); entry != nullptr; entry = xml_find_next(table, entry, "Weapon"))
        {
            if (!is_mp_override(entry))
            {
                Num_weapon_infos++;
            }
        }
        if (!dlc)
        {
            // Magic number: 16 extra weapons for the DLC tables. Original: only the parsed count is cleared.
            Weapon_infos = (weapon_info*)Permanent_mempool.static_mempool_base::alloc((Num_weapon_infos + 16) * sizeof(weapon_info), 0);
            memset(Weapon_infos, 0, Num_weapon_infos * sizeof(weapon_info));
        }
    }
    bool table_full = false;
    for (xml_element* entry = xml_find(table, "Weapon"); entry != nullptr && !table_full; entry = xml_find_next(table, entry, "Weapon"))
    {
        if (mp_override != is_mp_override(entry))
        {
            continue;
        }
        const char* name = xml_table_get_opt_string_ref(entry, "Name");
        if (name == nullptr)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            continue;
        }
        weapon_info* info = nullptr;
        if (refresh)
        {
            for (int i = 0; i < Num_weapon_infos; i++)
            {
                if (strcmp(name, Weapon_infos[i].name) == 0)
                {
                    info = &Weapon_infos[i];
                    break;
                }
            }
            if (info == nullptr)
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                continue;
            }
        }
        else
        {
            if (num_weapons >= Num_weapon_infos)
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                table_full = true;
                continue;
            }
            info = &Weapon_infos[num_weapons];
        }
        if (parse_weapon_info(pool, info, entry, name, refresh))
        {
            num_weapons++;
        }
    }
    if (!refresh)
    {
        if (num_weapons < Num_weapon_infos)
        {
            Num_weapon_infos = num_weapons;
        }
        if (!dlc)
        {
            qsort(Weapon_infos, Num_weapon_infos, sizeof(weapon_info), weapon_info_compare);
        }
    }
    assign_inv_item_w_infos(start_index);
    set_inaccuracy_for_multiplayer();
    xml_table_close(table);
}
