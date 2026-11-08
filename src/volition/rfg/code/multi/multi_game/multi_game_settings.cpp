// Original: volition/rfg/code/multi/multi_game/multi_game_settings.cpp
//
// The multiplayer match settings. The settings xtbls give the levels (mp_levels.xtbl and the DLC level tables:
// display name, time of day, wind, weapons, the modes the level suits and its options set) and the options sets
// (mp_options_sets.xtbl: generic and mode specific options per mode, each set based on another). Mp_server_options is
// the current match: mode, level, generic and mode specific options; the server sends it to the clients in
// join-game-start. Mp_server_options has no static defaults: multi_game_settings_system_init clears it and sets the
// first mode with a level, its level and that level's options.
// Partial (multiplayer.md MP4): the option names, descriptions and texts of the menus, and the menus' "is the option
// at its default" check, are UI (stand-ins).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_rubberbanding.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode_shared.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/cfile/packfile.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/memory/split_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/unicode_tools.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

multi_game_settings Mp_server_options; // 0x02FE6C80

// 0x0096BCE0
multi_game_options_full_set::multi_game_options_full_set()
{
}

// 0x00962EC0
multi_level_info::multi_level_info()
{
}

// ---- file-static globals
// multi_gameplay_mode_names_multi_game Multi_gameplay_mode_names[8] (0x0167E4A8) is used here too; placed in multi_core_platform.cpp.
// const char* EMPTY_CHECKBOX_NAME (0x0167E50C) is used here too; placed in multi_game_mode_common.cpp.
static const char* Multi_starting_backpack_option_names[3] = {"none", "random", "specific"}; // 0x0167F2BC
// The menus' option names and descriptions, by multi_game_options (the lock options have no description).
struct multi_game_option_info
{
    const char* name; // 0x00
    const char* description; // 0x04
};
static const multi_game_option_info Multi_game_option_info[59] = {
    {"MENU_GAME_MODE", "MENU_GAME_MODE_DESC"},
    {"MENU_MAP", "MENU_LEVEL_DESC"},
    {"MENU_SCORE_LIMIT", "MENU_SCORE_LIMIT_DESC"},
    {"MENU_TIME_LIMIT", "MENU_TIME_LIMIT_DESC"},
    {"MENU_FRIENDLY_FIRE", "MENU_FRIENDLY_FIRE_DESC"},
    {"MENU_HANDICAPPING", "MENU_HANDICAPPING_DESC"},
    {"MENU_ENEMY_ICONS", "MENU_ENEMY_ICONS_DESC"},
    {"MENU_TIME_OF_DAY", "MENU_TIME_OF_DAY_DESC"},
    {"MENU_RESPAWN_DELAY", "MENU_RESPAWN_DELAY_DESC"},
    {"MENU_BACKPACK_SPAWNS", "MENU_BACKPACK_SPAWNS_DESC"},
    {"MENU_START_BACKPACK", "MENU_START_BACKPACK_DESC"},
    {"MENU_REGENERATION", "MENU_REGENERATION_DESC"},
    {"MENU_HEALTH", "MENU_HEALTH_DESC"},
    {"MENU_MOVEMENT_SPEED", "MENU_MOVEMENT_SPEED_DESC"},
    {"MENU_AMMO", "MENU_AMMO_DESC"},
    {"MENU_STARTING_WEAPON", "MENU_STARTING_WEAPON_DESC"},
    {"MENU_THIRD_WEAPON", "MENU_THIRD_WEAPON_DESC"},
    {"MENU_FOURTH_WEAPON", "MENU_FOURTH_WEAPON_DESC"},
    {"MENU_WEAPON_RESPAWN", "MENU_WEAPON_RESPAWN_DESC"},
    {"MENU_ANARCHY_LEADER_HIGHLIGHTING", "MENU_ANARCHY_LEADER_HIGHLIGHTING_DESC"},
    {"MENU_CTF_FLAG_RETURN_TIME", "MENU_CTF_FLAG_RETURN_TIME_DESC"},
    {"MENU_CTF_FLAG_TOUCH_RETURN", "MENU_CTF_FLAG_TOUCH_RETURN_DESC"},
    {"MENU_CTF_FLAG_CARRIER_ICON_ALWAYS_ON", "MENU_CTF_FLAG_CARRIER_ICON_ALWAYS_ON_DESC"},
    {"MENU_SIEGE_DEFENSE_BACPACK", "MENU_SIEGE_DEFENSE_BACPACK_DESC"},
    {"MENU_SIEGE_DEFENSE_STARTING_WEAPON", "MENU_SIEGE_DEFENSE_STARTING_WEAPON_DESC"},
    {"MENU_SIEGE_DEFENSE_THIRD_WEAPON", "MENU_SIEGE_DEFENSE_THIRD_WEAPON_DESC"},
    {"MENU_SIEGE_DEFENSE_FOURTH_WEAPON", "MENU_SIEGE_DEFENSE_FOURTH_WEAPON_DESC"},
    {"MENU_DC_ALL_TARGET_MULTIPLIER", "MENU_DC_ALL_TARGET_MULTIPLIER_DESC"},
    {"MENU_BAGMAN_RETURN_TIME", "MENU_BAGMAN_RETURN_TIME_DESC"},
    {"MENU_BAGMAN_KILL_BONUS", "MENU_BAGMAN_KILL_BONUS_DESC"},
    {"MENU_TBAGMAN_RETURN_TIME", "MENU_TBAGMAN_RETURN_TIME_DESC"},
    {"MENU_BAGMAN_KILL_BONUS", "MENU_BAGMAN_KILL_BONUS_DESC"},
    {"MENU_DEMOLITION_ENGINEER_ICON", "MENU_DEMOLITION_ENGINEER_ICON_DESC"},
    {"WEAP_EDF_PISTOL", ""},
    {"WEAP_M16", ""},
    {"WEAP_AUTOBLASTER", ""},
    {"WEAP_ENFORCER", ""},
    {"WEAP_GAUSS", ""},
    {"WEAP_SNIPER", ""},
    {"WEAP_RAIL", ""},
    {"WEAP_SINGULARITY", ""},
    {"WEAP_MINE_PLACER", ""},
    {"WEAP_CHARGE_PLACER", ""},
    {"WEAP_RPG", ""},
    {"WEAP_THERMOBARIC", ""},
    {"WEAP_NANO", ""},
    {"WEAP_GRINDER", ""},
    {"WEAP_ARC_WELDER", ""},
    {"WEAP_SHOTGUN", ""},
    {"HM_BACKPACK_JETPACK", ""},
    {"HM_BACKPACK_THRUST", ""},
    {"HM_BACKPACK_FLEETFOOT", ""},
    {"HM_BACKPACK_STEALTH", ""},
    {"HM_BACKPACK_VISION", ""},
    {"HM_BACKPACK_COMMANDO", ""},
    {"HM_BACKPACK_SHOCKWAVE", ""},
    {"HM_BACKPACK_HEAL", ""},
    {"HM_BACKPACK_RHINO", ""},
    {"HM_BACKPACK_EARTHQUAKE", ""},
}; // 0x0167F2C8
static const char* Multi_backpack_spawn_option_names[4] = {"default", "shuffle", "none", "specific"}; // 0x0167F4A0
static const char* Multi_weapon_respawn_speed_names[5] = {"Normal", "Fast", "Always", "None", "Slow"}; // 0x0167F4B0
// multi_rubberbanding_level_info Rubberbanding_levels[4] (0x01693320) is used here too; placed in multi_game_rubberbanding.cpp.
// dynamic_object_pool<multi_level_info,1> Level_info_pool (0x01693990) is used here too; placed in multi_game_rubberbanding.cpp.
// dynamic_object_pool<multi_game_options_full_set,1> Options_sets_pool (0x016939D0) is used here too; placed in multi_game_rubberbanding.cpp.
// multi_game_playlist* Playlists (0x02FE6C70) is used here too; placed in multi_core_matchmaking_generic.cpp.
// int Num_playlists (0x02FE6D38) is used here too; placed in multi_game_playlists.cpp.
// int Settings_version (0x02FE88B0) is used here too; placed in multi.cpp.
// multi_game_playlist_entry* Playlist_entries (0x02FE8E4C) is used here too; placed in multi_game_playlists.cpp.
static v_packfile* Settings_packfile; // 0x02FEB714
// unsigned int Available_dlc_flags (0x02FEB718) is used here too; placed in multi_game_preferences.cpp.
static str_hash_table<255,64,unsigned char>* Mp_level_precache_list; // 0x02FEB71C

// Magic numbers: the generic and mode specific options of a set (0x40 and 0x14 bytes); level names hold 64
// characters; at most 8 player weapons of 47 characters per level; percentages are stored as integers (x100).
static const int Max_level_name = 64;
static const int Max_level_weapons = 8;
static const int Level_weapon_name_size = 47;
static const float Percent_scale = 100.0f;

// ---- internal functions

// 0x009613A0 (:225). LTCG: pool in ESI. The settings packfile is not loaded in retail: the memory for its header is
// taken and given back at once (rfg.exe keeps no packfile).
static void expand_settings_files(mempool_base& pool)
{
    if (Settings_packfile != nullptr)
    {
        return;
    }
    split_mempool& split = (split_mempool&)pool;
    unsigned int cpu_mark = 0;
    unsigned int gpu_mark = 0;
    if ((pool.get_flags() & 0x40) == 0)
    {
        cpu_mark = pool.mark();
    }
    else
    {
        cpu_mark = split.mark_cpu();
        gpu_mark = split.mark_gpu();
    }
    Settings_packfile = (v_packfile*)pool.alloc(sizeof(v_packfile), 0);
    if ((pool.get_flags() & 0x40) != 0)
    {
        split.restore_to_mark_cpu(cpu_mark);
        split.restore_to_mark_gpu(gpu_mark);
    }
    else
    {
        pool.restore_to_mark(cpu_mark);
    }
    Settings_packfile = nullptr;
}

// 0x00961420 (:258). LTCG: option in EAX.
static bool option_is_mode_specific(multi_game_options option)
{
    return (unsigned int)(option - MGO_ANARCHY_LEADER_HIGHLIGHTING) <= MGO_DEMOLITION_ENGINEER_ICON - MGO_ANARCHY_LEADER_HIGHLIGHTING;
}

// 0x00961430 (:263). LTCG: lhs in EAX, rhs in ECX. The mode specific options count as equal here.
static bool option_is_equal(multi_game_options option, const multi_game_generic_options& lhs, const multi_game_generic_options& rhs)
{
    switch (option)
    {
    case MGO_SCORE_LIMIT:
        return lhs.point_limit == rhs.point_limit;
    case MGO_TIME_LIMIT:
        return lhs.round_time_limit_minutes == rhs.round_time_limit_minutes;
    case MGO_FRIENDLY_FIRE:
        return lhs.friendly_fire == rhs.friendly_fire;
    case MGO_RUBBERBANDING_LEVEL:
        return lhs.rubberbanding_level == rhs.rubberbanding_level;
    case MGO_ENEMY_ICONS:
        return lhs.enemy_icon_visibility == rhs.enemy_icon_visibility;
    case MGO_TIME_OF_DAY:
        return lhs.time_of_day == rhs.time_of_day;
    case MGO_RESPAWN_DELAY:
        return lhs.respawn_time_seconds == rhs.respawn_time_seconds;
    case MGO_BACKPACKS:
        return lhs.backpack_spawns == rhs.backpack_spawns && lhs.backpack_spawn_type == rhs.backpack_spawn_type;
    case MGO_STARTING_BACKPACK:
        return lhs.starting_backpack == rhs.starting_backpack && lhs.starting_backpack_type == rhs.starting_backpack_type;
    case MGO_HEALTH_REGEN_FACTOR:
        return lhs.health_regen_speed == rhs.health_regen_speed;
    case MGO_HEALTH_FACTOR:
        return lhs.max_health == rhs.max_health;
    case MGO_MOVEMENT_FACTOR:
        return lhs.movement_speed == rhs.movement_speed;
    case MGO_UNLIMITED_AMMO:
        return lhs.unlimited_ammo == rhs.unlimited_ammo;
    case MGO_STARTING_WEAPON:
        return lhs.replace_starting_weapon == rhs.replace_starting_weapon && lhs.starting_weapon == rhs.starting_weapon;
    case MGO_THIRD_WEAPON:
        return lhs.third_weapon == rhs.third_weapon;
    case MGO_FOURTH_WEAPON:
        return lhs.fourth_weapon == rhs.fourth_weapon;
    case MGO_WEAPON_RESPAWN_SPEED:
        return lhs.weapon_respawn_speed == rhs.weapon_respawn_speed;
    default:
        break;
    }
    if (option >= MGO_LOCK_WEAPON_PISTOL && option <= MGO_LOCK_WEAPON_SHOTGUN)
    {
        const unsigned short bit = (unsigned short)(1 << (option - MGO_LOCK_WEAPON_PISTOL));
        return (lhs.locked_weapons_mask & bit) == (rhs.locked_weapons_mask & bit);
    }
    if (option >= MGO_LOCK_BACKPACK_JETPACK && option <= MGO_LOCK_BACKPACK_TREMOR)
    {
        const unsigned short bit = (unsigned short)(1 << (option - MGO_LOCK_BACKPACK_JETPACK));
        return (lhs.locked_backpacks_mask & bit) == (rhs.locked_backpacks_mask & bit);
    }
    return true;
}

// 0x00961830 (:449). LTCG: lhs in EAX, rhs in ECX. An option of another mode counts as equal.
static bool option_is_equal(multi_game_options option, multi_gameplay_mode mode, const multi_game_mode_options& lhs, const multi_game_mode_options& rhs)
{
    switch (option)
    {
    case MGO_ANARCHY_LEADER_HIGHLIGHTING:
        if (mode == MGPM_DEATHMATCH)
        {
            return rhs.deathmatch.leader_highlighting == lhs.deathmatch.leader_highlighting;
        }
        break;
    case MGO_CTF_FLAG_RETURN_TIME:
        if (mode == MGPM_CAPTURE_THE_FLAG)
        {
            return rhs.capture_the_flag.flag_return_time_seconds == lhs.capture_the_flag.flag_return_time_seconds;
        }
        break;
    case MGO_CTF_FLAG_TOUCH_RETURN:
        if (mode == MGPM_CAPTURE_THE_FLAG)
        {
            return rhs.capture_the_flag.flag_touch_return == lhs.capture_the_flag.flag_touch_return;
        }
        break;
    case MGO_CTF_CARRIER_ICON_ALWAYS_ON:
        if (mode == MGPM_CAPTURE_THE_FLAG)
        {
            return rhs.capture_the_flag.flag_carrier_icon_always_on == lhs.capture_the_flag.flag_carrier_icon_always_on;
        }
        break;
    case MGO_SIEGE_DEFENSE_STARTING_BACKPACK:
        if (mode == MGPM_SIEGE)
        {
            return rhs.siege.defense_starting_backpack_type == lhs.siege.defense_starting_backpack_type;
        }
        break;
    case MGO_SIEGE_DEFENSE_STARTING_WEAPON:
        if (mode == MGPM_SIEGE)
        {
            return rhs.siege.defense_starting_weapon == lhs.siege.defense_starting_weapon;
        }
        break;
    case MGO_SIEGE_DEFENSE_THIRD_WEAPON:
        if (mode == MGPM_SIEGE)
        {
            return rhs.siege.defense_third_weapon == lhs.siege.defense_third_weapon;
        }
        break;
    case MGO_SIEGE_DEFENSE_FOURTH_WEAPON:
        if (mode == MGPM_SIEGE)
        {
            return rhs.siege.defense_fourth_weapon == lhs.siege.defense_fourth_weapon;
        }
        break;
    case MGO_DC_ALL_TARGET_MULTIPLIER:
        if (mode == MGPM_KING_OF_THE_HILL)
        {
            return rhs.king_of_the_hill.all_target_multiplier == lhs.king_of_the_hill.all_target_multiplier;
        }
        break;
    case MGO_BAGMAN_RETURN_TIME:
        if (mode == MGPM_BAGMAN)
        {
            return rhs.bagman.bag_return_time_seconds == lhs.bagman.bag_return_time_seconds;
        }
        break;
    case MGO_BAGMAN_KILL_BONUS:
        if (mode == MGPM_BAGMAN)
        {
            return rhs.bagman.bagman_kill_bonus == lhs.bagman.bagman_kill_bonus;
        }
        break;
    case MGO_TBAGMAN_RETURN_TIME:
        if (mode == MGPM_TEAM_BAGMAN)
        {
            return rhs.team_bagman.bag_return_time_seconds == lhs.team_bagman.bag_return_time_seconds;
        }
        break;
    case MGO_TBAGMAN_KILL_BONUS:
        if (mode == MGPM_TEAM_BAGMAN)
        {
            return rhs.team_bagman.bagman_kill_bonus == lhs.team_bagman.bagman_kill_bonus;
        }
        break;
    case MGO_DEMOLITION_ENGINEER_ICON:
        if (mode == MGPM_DEMOLITION)
        {
            return rhs.demolition.engineer_icon_always_on == lhs.demolition.engineer_icon_always_on;
        }
        break;
    default:
        break;
    }
    return true;
}

// 0x00961990 (:626). LTCG: dest in EAX, src in ECX. The mode specific options are not copied here.
static void copy_option(multi_game_generic_options& dest, const multi_game_generic_options& src, multi_game_options option)
{
    switch (option)
    {
    case MGO_SCORE_LIMIT:
        dest.point_limit = src.point_limit;
        return;
    case MGO_TIME_LIMIT:
        dest.round_time_limit_minutes = src.round_time_limit_minutes;
        return;
    case MGO_FRIENDLY_FIRE:
        dest.friendly_fire = src.friendly_fire;
        return;
    case MGO_RUBBERBANDING_LEVEL:
        dest.rubberbanding_level = src.rubberbanding_level;
        return;
    case MGO_ENEMY_ICONS:
        dest.enemy_icon_visibility = src.enemy_icon_visibility;
        return;
    case MGO_TIME_OF_DAY:
        dest.time_of_day = src.time_of_day;
        return;
    case MGO_RESPAWN_DELAY:
        dest.respawn_time_seconds = src.respawn_time_seconds;
        return;
    case MGO_BACKPACKS:
        dest.backpack_spawns = src.backpack_spawns;
        dest.backpack_spawn_type = src.backpack_spawn_type;
        return;
    case MGO_STARTING_BACKPACK:
        dest.starting_backpack = src.starting_backpack;
        dest.starting_backpack_type = src.starting_backpack_type;
        return;
    case MGO_HEALTH_REGEN_FACTOR:
        dest.health_regen_speed = src.health_regen_speed;
        return;
    case MGO_HEALTH_FACTOR:
        dest.max_health = src.max_health;
        return;
    case MGO_MOVEMENT_FACTOR:
        dest.movement_speed = src.movement_speed;
        return;
    case MGO_UNLIMITED_AMMO:
        dest.unlimited_ammo = src.unlimited_ammo;
        return;
    case MGO_STARTING_WEAPON:
        dest.replace_starting_weapon = src.replace_starting_weapon;
        dest.starting_weapon = src.starting_weapon;
        return;
    case MGO_THIRD_WEAPON:
        dest.third_weapon = src.third_weapon;
        return;
    case MGO_FOURTH_WEAPON:
        dest.fourth_weapon = src.fourth_weapon;
        return;
    case MGO_WEAPON_RESPAWN_SPEED:
        dest.weapon_respawn_speed = src.weapon_respawn_speed;
        return;
    default:
        break;
    }
    if (option >= MGO_LOCK_WEAPON_PISTOL && option <= MGO_LOCK_WEAPON_SHOTGUN)
    {
        const unsigned short bit = (unsigned short)(1 << (option - MGO_LOCK_WEAPON_PISTOL));
        dest.locked_weapons_mask = (unsigned short)((dest.locked_weapons_mask & ~bit) | (src.locked_weapons_mask & bit));
    }
    else if (option >= MGO_LOCK_BACKPACK_JETPACK && option <= MGO_LOCK_BACKPACK_TREMOR)
    {
        const unsigned short bit = (unsigned short)(1 << (option - MGO_LOCK_BACKPACK_JETPACK));
        dest.locked_backpacks_mask = (unsigned short)((dest.locked_backpacks_mask & ~bit) | (src.locked_backpacks_mask & bit));
    }
}

// 0x00961DF0 (:990): the generic options of src that differ from defaults (all of them without defaults); the weapons
// come along whole when any weapon option was copied, and the number of rounds always. LTCG: src in EDI.
static void copy_generic_options(multi_game_generic_options* dest, const multi_game_generic_options* src, const multi_game_generic_options* defaults)
{
    if (dest == nullptr || src == nullptr)
    {
        return;
    }
    bool weapons_copied = false;
    for (int i = 0; i < NUM_MULTI_GAME_OPTIONS; i++)
    {
        const multi_game_options option = (multi_game_options)i;
        if (option_is_mode_specific(option))
        {
            continue;
        }
        if (defaults != nullptr && option_is_equal(option, *src, *defaults))
        {
            continue;
        }
        copy_option(*dest, *src, option);
        if (option == MGO_STARTING_WEAPON || option == MGO_THIRD_WEAPON || option == MGO_FOURTH_WEAPON)
        {
            weapons_copied = true;
        }
    }
    if (weapons_copied)
    {
        dest->replace_starting_weapon = src->replace_starting_weapon;
        dest->starting_weapon = src->starting_weapon;
        dest->third_weapon = src->third_weapon;
        dest->fourth_weapon = src->fourth_weapon;
    }
    dest->num_rounds = src->num_rounds;
}

// Port helper: a weapon name of an options set (only multiplayer weapons count; rfg.exe inlines this four times).
static unsigned int parse_mp_weapon_handle(xml_element* wrapper, const char* tag)
{
    const char* name = xml_table_get_opt_string_ref(wrapper, tag);
    if (name == nullptr)
    {
        return 0xFFFFFFFF;
    }
    const weapon_info* w_info = weapons_get_weapon_info(name);
    if (w_info == nullptr || !w_info->flags.mp_selectable)
    {
        return 0xFFFFFFFF;
    }
    return weapons_get_weapon_info_handle(w_info);
}

// The lockable weapons and backpacks by table name (the order of rfg.exe's comparisons) and their bits.
struct lockable_name
{
    const char* name;
    unsigned short bit;
};

static const lockable_name Lockable_weapon_names[] = {
    {"m16", 1 << MGLW_ASSAULT_RIFLE},
    {"edf_pistol", 1 << MGLW_PISTOL},
    {"autoblaster", 1 << MGLW_PEACEKEEPER},
    {"enforcer", 1 << MGLW_ENFORCER},
    {"gauss_rifle", 1 << MGLW_SNOWBALL_GUN},
    {"sniper_rifle", 1 << MGLW_SNIPER_RIFLE},
    {"rail_driver", 1 << MGLW_RAIL_DRIVER},
    {"singularity_bomb", 1 << MGLW_SING_BOMB},
    {"charge_placer", 1 << MGLW_REMOTE_CHARGES},
    {"rpg_launcher", 1 << MGLW_ROCKET_LAUNCHER},
    {"mine_placer", 1 << MGLW_PROX_MINE},
    {"thermobaric_rockets", 1 << MGLW_THERMOBARIC_ROCKET},
    {"grinder", 1 << MGLW_GRINDER},
    {"arc_welder", 1 << MGLW_ARC_WELDER},
    {"nano_rifle", 1 << MGLW_NANO_RIFLE},
    {"shotgun", (unsigned short)(1 << MGLW_SHOTGUN)},
};

static const lockable_name Lockable_backpack_names[] = {
    {"Commando", 1 << MGLB_FIREPOWER},
    {"Fleetfoot", 1 << MGLB_FLEETFOOT},
    {"Jetpack", 1 << MGLB_JETPACK},
    {"Thrust", 1 << MGLB_THRUST},
    {"Rhino", 1 << MGLB_RHINO},
    {"Shockwave", 1 << MGLB_CONCUSSION},
    {"Tremor", 1 << MGLB_TREMOR},
    {"Heal", 1 << MGLB_HEAL},
    {"Vision", 1 << MGLB_VISION},
    {"Stealth", 1 << MGLB_STEALTH},
};

// 0x00961E80 (:1022): required options must all be there (and get the defaults of the base set first); optional
// ones override only what they name. LTCG: options in ESI, defaults in EAX.
static void parse_generic_options(multi_game_generic_options* options, xml_element* options_base, bool required, const multi_game_generic_options* defaults)
{
    if (defaults != nullptr)
    {
        copy_generic_options(options, defaults, nullptr);
    }
    const char* rubberbanding;
    if (!required)
    {
        int value;
        float scale;
        if (xml_table_get_opt_int(value, options_base, "point_limit"))
        {
            options->point_limit = (unsigned short)value;
        }
        if (xml_table_get_opt_int(value, options_base, "num_rounds"))
        {
            options->num_rounds = (unsigned char)value;
        }
        if (xml_table_get_opt_int(value, options_base, "round_time_limit_minutes"))
        {
            options->round_time_limit_minutes = (unsigned char)value;
        }
        xml_table_get_opt_bool(options->friendly_fire, options_base, "friendly_fire");
        if (xml_table_get_opt_int(value, options_base, "respawn_time_seconds"))
        {
            options->respawn_time_seconds = (unsigned char)value;
        }
        if (xml_table_get_opt_float(scale, options_base, "health_regen_speed_multiplier"))
        {
            options->health_regen_speed = (unsigned char)(int)(scale * Percent_scale);
        }
        if (xml_table_get_opt_float(scale, options_base, "max_health_multiplier"))
        {
            // As in rfg.exe: the health percentage is kept in a byte first.
            options->max_health = (unsigned char)(int)(scale * Percent_scale);
        }
        if (xml_table_get_opt_float(scale, options_base, "movement_speed_multiplier"))
        {
            options->movement_speed = (unsigned char)(int)(scale * Percent_scale);
        }
        rubberbanding = xml_table_get_opt_string_ref(options_base, "rubberbanding_level");
    }
    else
    {
        int value;
        float scale;
        xml_table_get_req_int(value, options_base, "point_limit");
        options->point_limit = (unsigned short)value;
        xml_table_get_req_int(value, options_base, "num_rounds");
        options->num_rounds = (unsigned char)value;
        xml_table_get_req_int(value, options_base, "round_time_limit_minutes");
        options->round_time_limit_minutes = (unsigned char)value;
        xml_table_get_req_bool(options->friendly_fire, options_base, "friendly_fire");
        xml_table_get_req_int(value, options_base, "respawn_time_seconds");
        options->respawn_time_seconds = (unsigned char)value;
        xml_table_get_req_float(scale, options_base, "health_regen_speed_multiplier");
        options->health_regen_speed = (unsigned char)(int)(scale * Percent_scale);
        xml_table_get_req_float(scale, options_base, "max_health_multiplier");
        options->max_health = (unsigned char)(int)(scale * Percent_scale);
        xml_table_get_req_float(scale, options_base, "movement_speed_multiplier");
        options->movement_speed = (unsigned char)(int)(scale * Percent_scale);
        rubberbanding = xml_table_get_req_string_ref(options_base, "rubberbanding_level");
        options->rubberbanding_level = MRL_DEFAULT;
    }
    if (rubberbanding != nullptr)
    {
        const checksum_stri crc(rubberbanding, 0);
        options->rubberbanding_level = MRL_NONE;
        for (int i = 0; i < NUM_MULTI_RUBBERBANDING_LEVELS; i++)
        {
            if (Rubberbanding_levels[i].internal_name_pair.str_checksum == crc)
            {
                options->rubberbanding_level = (multi_rubberbanding_levels)i;
                break;
            }
        }
    }
    const char* icons;
    if (!required)
    {
        icons = xml_table_get_opt_string_ref(options_base, "enemy_icon_visibility");
    }
    else
    {
        icons = xml_table_get_req_string_ref(options_base, "enemy_icon_visibility");
        options->enemy_icon_visibility = MGEIV_DEFAULT;
    }
    if (icons != nullptr)
    {
        if (v_stricmp(icons, "none") == 0)
        {
            options->enemy_icon_visibility = MGEIV_NONE;
        }
        else if (v_stricmp(icons, "delayed") == 0)
        {
            options->enemy_icon_visibility = MGEIV_DELAYED;
        }
        else if (v_stricmp(icons, "default") == 0)
        {
            options->enemy_icon_visibility = MGEIV_DEFAULT;
        }
        else if (v_stricmp(icons, "always") == 0)
        {
            options->enemy_icon_visibility = MGEIV_ALWAYS;
        }
    }
    if (required)
    {
        options->time_of_day = MGTOD_RANDOM;
        options->starting_backpack = MGSBO_NONE;
        options->starting_backpack_type = -1;
    }
    xml_element* wrapper = xml_table_find_opt(options_base, "starting_backpack_wrapper");
    if (wrapper != nullptr)
    {
        options->starting_backpack_type = -1;
        xml_element* element = xml_table_find_req(wrapper, "starting_backpack");
        if (element != nullptr)
        {
            for (int i = 0; i < NUM_MULTI_GAME_STARTING_BACKPACK_OPTIONS; i++)
            {
                if (v_stricmp(element->text, Multi_starting_backpack_option_names[i]) == 0)
                {
                    options->starting_backpack = (multi_game_starting_backpack_options)i;
                    break;
                }
            }
            if (options->starting_backpack == MGSBO_SPECIFIC)
            {
                const char* type = xml_table_get_req_string_ref(wrapper, "starting_backpack_type");
                if (type != nullptr)
                {
                    options->starting_backpack_type = multi_game_backpack_get_backpack_id(type);
                }
            }
        }
    }
    if (required)
    {
        options->backpack_spawns = MGBSO_DEFAULT;
        options->backpack_spawn_type = -1;
    }
    wrapper = xml_table_find_opt(options_base, "backpack_spawns_wrapper");
    if (wrapper != nullptr)
    {
        options->backpack_spawn_type = -1;
        xml_element* element = xml_table_find_req(wrapper, "backpack_spawns");
        if (element != nullptr)
        {
            for (int i = 0; i < NUM_MULTI_GAME_BACKPACK_SPAWN_OPTIONS; i++)
            {
                if (v_stricmp(element->text, Multi_backpack_spawn_option_names[i]) == 0)
                {
                    options->backpack_spawns = (multi_game_backpack_spawn_options)i;
                    break;
                }
            }
            if (options->backpack_spawns == MGBSO_SPECIFIC)
            {
                const char* type = xml_table_get_req_string_ref(wrapper, "default_backpack_type");
                if (type != nullptr)
                {
                    options->backpack_spawn_type = multi_game_backpack_get_backpack_id(type);
                }
            }
        }
    }
    // The default starting weapon is the assault rifle (m16), replacing the class weapon.
    if (required)
    {
        options->replace_starting_weapon = true;
        options->starting_weapon = weapons_get_weapon_info_handle("m16");
    }
    wrapper = xml_table_find_opt(options_base, "starting_weapon_wrapper");
    if (wrapper != nullptr)
    {
        options->replace_starting_weapon = true;
        options->starting_weapon = weapons_get_weapon_info_handle("m16");
        xml_element* replace = xml_table_find_opt(wrapper, "replace_starting_weapon");
        if (replace != nullptr)
        {
            options->replace_starting_weapon = true;
            options->starting_weapon = parse_mp_weapon_handle(replace, "starting_weapon");
        }
    }
    if (required)
    {
        options->third_weapon = 0xFFFFFFFF;
    }
    wrapper = xml_table_find_opt(options_base, "third_weapon_wrapper");
    if (wrapper != nullptr)
    {
        options->third_weapon = parse_mp_weapon_handle(wrapper, "third_weapon");
    }
    if (required)
    {
        options->fourth_weapon = 0xFFFFFFFF;
    }
    wrapper = xml_table_find_opt(options_base, "fourth_weapon_wrapper");
    if (wrapper != nullptr)
    {
        options->fourth_weapon = parse_mp_weapon_handle(wrapper, "fourth_weapon");
    }
    if (required)
    {
        options->unlimited_ammo = false;
    }
    xml_table_get_opt_bool(options->unlimited_ammo, options_base, "unlimited_ammo");
    if (required)
    {
        options->weapon_respawn_speed = MGWRS_NORMAL;
    }
    const char* respawn_speed = xml_table_get_opt_string_ref(options_base, "weapon_respawn_speed");
    if (respawn_speed != nullptr)
    {
        for (int i = 0; i < NUM_MULTI_GAME_WEAPON_RESPAWN_SPEEDS; i++)
        {
            if (v_stricmp(Multi_weapon_respawn_speed_names[i], respawn_speed) == 0)
            {
                options->weapon_respawn_speed = (multi_game_weapon_respawn_speeds)i;
                break;
            }
        }
    }
    if (required)
    {
        options->locked_weapons_mask = 0;
    }
    wrapper = xml_table_find_opt(options_base, "disable_weapons");
    if (wrapper != nullptr)
    {
        options->locked_weapons_mask = 0;
        for (xml_element* weapon = xml_find(wrapper, "weapon_list"); weapon != nullptr; weapon = xml_find_next(wrapper, weapon, "weapon_list"))
        {
            for (const lockable_name& lockable : Lockable_weapon_names)
            {
                if (v_stricmp(lockable.name, weapon->text) == 0)
                {
                    options->locked_weapons_mask |= lockable.bit;
                    break;
                }
            }
        }
    }
    if (required)
    {
        options->locked_backpacks_mask = 0;
    }
    wrapper = xml_table_find_opt(options_base, "disable_backpacks");
    if (wrapper != nullptr)
    {
        options->locked_backpacks_mask = 0;
        for (xml_element* backpack = xml_find(wrapper, "backpack_list"); backpack != nullptr; backpack = xml_find_next(wrapper, backpack, "backpack_list"))
        {
            for (const lockable_name& lockable : Lockable_backpack_names)
            {
                if (v_stricmp(lockable.name, backpack->text) == 0)
                {
                    options->locked_backpacks_mask |= lockable.bit;
                    break;
                }
            }
        }
    }
}

// 0x00962850 (:1434). LTCG: dest in ECX, src in EAX.
static void copy_mode_specific_options(multi_game_mode_options* dest, const multi_game_mode_options* src, multi_gameplay_mode mode)
{
    if (dest != nullptr && src != nullptr)
    {
        *dest = *src;
    }
}

// 0x00962880 (:1441): whether the mode's options are named in the set (and whether the mode has options at all).
// LTCG: options_base in ESI, mode in ECX, mode_has_options in EAX.
static bool mode_specific_options_exist(xml_element* options_base, multi_gameplay_mode mode, bool& mode_has_options)
{
    switch (mode)
    {
    case MGPM_DEATHMATCH:
        mode_has_options = true;
        return xml_table_find_opt(options_base, "leader_highlighting") != nullptr;
    case MGPM_TEAM_DEATHMATCH:
        mode_has_options = false;
        return false;
    case MGPM_CAPTURE_THE_FLAG:
        mode_has_options = true;
        return xml_table_find_opt(options_base, "flag_captures_to_win_round") != nullptr ||
               xml_table_find_opt(options_base, "flag_carrier_speed_multiplier") != nullptr ||
               xml_table_find_opt(options_base, "flag_carrier_regen_speed_multiplier") != nullptr ||
               xml_table_find_opt(options_base, "flag_return_time_seconds") != nullptr ||
               xml_table_find_opt(options_base, "civilian_flag_carrier") != nullptr ||
               xml_table_find_opt(options_base, "flag_touch_return") != nullptr ||
               xml_table_find_opt(options_base, "flag_as_backpack") != nullptr ||
               xml_table_find_opt(options_base, "flag_carrier_icon_always_on") != nullptr;
    case MGPM_SIEGE:
        mode_has_options = true;
        return xml_table_find_opt(options_base, "percent_destruction_required") != nullptr ||
               xml_table_find_opt(options_base, "defense_starting_backpack") != nullptr ||
               xml_table_find_opt(options_base, "defense_starting_weapon") != nullptr ||
               xml_table_find_opt(options_base, "defense_third_weapon") != nullptr ||
               xml_table_find_opt(options_base, "defense_fourth_weapon") != nullptr;
    case MGPM_KING_OF_THE_HILL:
        mode_has_options = true;
        return xml_table_find_opt(options_base, "all_target_multiplier") != nullptr;
    case MGPM_BAGMAN:
        mode_has_options = true;
        return xml_table_find_opt(options_base, "bag_return_time_seconds") != nullptr ||
               xml_table_find_opt(options_base, "bagman_kill_bonus") != nullptr;
    case MGPM_TEAM_BAGMAN:
        mode_has_options = true;
        return xml_table_find_opt(options_base, "num_bags") != nullptr ||
               xml_table_find_opt(options_base, "bag_return_time_seconds") != nullptr ||
               xml_table_find_opt(options_base, "bagman_kill_bonus") != nullptr;
    case MGPM_DEMOLITION:
        mode_has_options = true;
        return xml_table_find_opt(options_base, "engineer_icon_always_on") != nullptr ||
               xml_table_find_opt(options_base, "engineer_weapon_wrapper") != nullptr;
    default:
        return false;
    }
}

// 0x00962A30 (:1549): the base set's options first, then the mode's; required ones must be there. LTCG: mode in ECX,
// defaults in EAX.
static void parse_mode_specific_options(multi_game_mode_options* options, xml_element* options_base, multi_gameplay_mode mode, bool required, const multi_game_mode_options* defaults)
{
    if (defaults != nullptr && options != nullptr)
    {
        *options = *defaults;
    }
    switch (mode)
    {
    case MGPM_DEATHMATCH:
        if (required)
        {
            xml_table_get_req_bool(options->deathmatch.leader_highlighting, options_base, "leader_highlighting");
        }
        else
        {
            xml_table_get_opt_bool(options->deathmatch.leader_highlighting, options_base, "leader_highlighting");
        }
        break;
    case MGPM_TEAM_DEATHMATCH:
        break;
    case MGPM_CAPTURE_THE_FLAG:
    {
        float scale;
        int value;
        if (!required)
        {
            if (xml_table_get_opt_float(scale, options_base, "flag_carrier_speed_multiplier"))
            {
                options->capture_the_flag.flag_carrier_speed = (unsigned char)(int)(scale * Percent_scale);
            }
            if (xml_table_get_opt_float(scale, options_base, "flag_carrier_regen_speed"))
            {
                options->capture_the_flag.flag_carrier_regen_speed = (unsigned char)(int)(scale * Percent_scale);
            }
            if (xml_table_get_opt_int(value, options_base, "flag_return_time_seconds"))
            {
                options->capture_the_flag.flag_return_time_seconds = (unsigned char)value;
            }
            xml_table_get_opt_bool(options->capture_the_flag.civilian_flag_carrier, options_base, "civilian_flag_carrier");
            xml_table_get_opt_bool(options->capture_the_flag.flag_touch_return, options_base, "flag_touch_return");
            xml_table_get_opt_bool(options->capture_the_flag.flag_carrier_icon_always_on, options_base, "flag_carrier_icon_always_on");
        }
        else
        {
            xml_table_get_req_float(scale, options_base, "flag_carrier_speed_multiplier");
            options->capture_the_flag.flag_carrier_speed = (unsigned char)(int)(scale * Percent_scale);
            xml_table_get_req_float(scale, options_base, "flag_carrier_regen_speed");
            options->capture_the_flag.flag_carrier_regen_speed = (unsigned char)(int)(scale * Percent_scale);
            xml_table_get_req_int(value, options_base, "flag_return_time_seconds");
            options->capture_the_flag.flag_return_time_seconds = (unsigned char)value;
            xml_table_get_req_bool(options->capture_the_flag.civilian_flag_carrier, options_base, "civilian_flag_carrier");
            xml_table_get_req_bool(options->capture_the_flag.flag_touch_return, options_base, "flag_touch_return");
            xml_table_get_req_bool(options->capture_the_flag.flag_carrier_icon_always_on, options_base, "flag_carrier_icon_always_on");
        }
        break;
    }
    case MGPM_SIEGE:
    {
        int value;
        if (xml_table_get_opt_int(value, options_base, "percent_destruction_required"))
        {
            options->siege.percent_destruction_required = (unsigned char)value;
        }
        if (required)
        {
            options->siege.defense_starting_backpack_type = -1;
        }
        xml_element* wrapper = xml_table_find_opt(options_base, "defense_starting_backpack_wrapper");
        if (wrapper != nullptr)
        {
            const char* type = xml_table_get_opt_string_ref(wrapper, "starting_backpack_type");
            options->siege.defense_starting_backpack_type = type != nullptr ? multi_game_backpack_get_backpack_id(type) : -1;
        }
        if (required)
        {
            options->siege.defense_starting_weapon = 0xFFFFFFFF;
        }
        wrapper = xml_table_find_opt(options_base, "defense_starting_weapon_wrapper");
        if (wrapper != nullptr)
        {
            options->siege.defense_starting_weapon = parse_mp_weapon_handle(wrapper, "starting_weapon");
        }
        if (required)
        {
            options->siege.defense_third_weapon = 0xFFFFFFFF;
        }
        wrapper = xml_table_find_opt(options_base, "defense_third_weapon_wrapper");
        if (wrapper != nullptr)
        {
            options->siege.defense_third_weapon = parse_mp_weapon_handle(wrapper, "third_weapon");
        }
        if (required)
        {
            options->siege.defense_fourth_weapon = 0xFFFFFFFF;
        }
        wrapper = xml_table_find_opt(options_base, "defense_fourth_weapon_wrapper");
        if (wrapper != nullptr)
        {
            options->siege.defense_fourth_weapon = parse_mp_weapon_handle(wrapper, "fourth_weapon");
        }
        break;
    }
    case MGPM_KING_OF_THE_HILL:
    {
        const char* multiplier = xml_table_get_opt_string_ref(options_base, "all_target_multiplier");
        if (multiplier != nullptr)
        {
            if (v_stricmp(multiplier, "normal") == 0)
            {
                options->king_of_the_hill.all_target_multiplier = KOTHM_NORMAL;
            }
            else if (v_stricmp(multiplier, "only") == 0)
            {
                options->king_of_the_hill.all_target_multiplier = KOTHM_ONLY;
            }
            else if (v_stricmp(multiplier, "none") == 0)
            {
                options->king_of_the_hill.all_target_multiplier = KOTHM_NONE;
            }
        }
        break;
    }
    case MGPM_BAGMAN:
    case MGPM_TEAM_BAGMAN:
    {
        int value;
        if (xml_table_get_opt_int(value, options_base, "bag_return_time_seconds"))
        {
            options->bagman.bag_return_time_seconds = (unsigned char)value;
        }
        xml_table_get_opt_bool(options->bagman.bagman_kill_bonus, options_base, "bagman_kill_bonus");
        break;
    }
    case MGPM_DEMOLITION:
    {
        xml_element* wrapper;
        if (required)
        {
            options->demolition.engineer_weapon = 0xFFFFFFFF;
            wrapper = xml_table_find_req(options_base, "engineer_weapon_wrapper");
        }
        else
        {
            wrapper = xml_table_find_opt(options_base, "engineer_weapon_wrapper");
        }
        if (wrapper != nullptr)
        {
            options->demolition.engineer_weapon = parse_mp_weapon_handle(wrapper, "engineer_weapon");
        }
        if (required)
        {
            xml_table_get_req_bool(options->demolition.engineer_icon_always_on, options_base, "engineer_icon_always_on");
        }
        else
        {
            xml_table_get_opt_bool(options->demolition.engineer_icon_always_on, options_base, "engineer_icon_always_on");
        }
        break;
    }
    default:
        break;
    }
}

// 0x009B22E0 (:1830). LTCG: info in ECX, level_base in EDI.
static void parse_level_entry(const char* level_filename, multi_level_info* info, xml_element* level_base, mempool_base& pool)
{
    xml_table_get_req_string(info->display_name, sizeof(info->display_name), level_base, "Display_name");
    strncpy_safe(info->filename, level_filename, sizeof(info->filename));
    info->filename_crc = checksum_stri(info->filename, 0);
    xml_table_get_req_string(info->description, sizeof(info->description), level_base, "Description");
    xml_table_get_req_string(info->level_image_name, sizeof(info->level_image_name), level_base, "level_image_name");
    xml_table_get_req_string(info->level_load_image, sizeof(info->level_load_image), level_base, "Load_image");
    info->is_downloadable_content = false;
    const char* options_set = xml_table_get_req_string_ref(level_base, "options_set");
    info->options = options_set != nullptr ? multi_game_settings_find_game_option_full_set(options_set) : nullptr;
    const char* mode_names[MGPM_NUM_MODES];
    for (int i = 0; i < MGPM_NUM_MODES; i++)
    {
        mode_names[i] = Multi_gameplay_mode_names[i].internal_name;
    }
    info->suitable_mode_flags = (unsigned int)xml_table_get_flags(mode_names, MGPM_NUM_MODES, xml_table_find_req(level_base, "mode_flags"), false);
    const char* time_of_day = xml_table_get_req_string_ref(level_base, "time_of_day");
    if (time_of_day != nullptr)
    {
        multi_game_settings_parse_time_of_day(&info->time_of_day_hour, &info->time_of_day_minute, time_of_day);
    }
    xml_table_get_req_float(info->glow_multiplier, level_base, "glow_multiplier");
    const char* alt_time_of_day = xml_table_get_req_string_ref(level_base, "alternate_time_of_day");
    if (alt_time_of_day != nullptr)
    {
        multi_game_settings_parse_time_of_day(&info->alt_time_of_day_hour, &info->alt_time_of_day_minute, alt_time_of_day);
    }
    xml_table_get_req_float(info->alt_glow_multiplier, level_base, "alternate_glow_multiplier");
    xml_table_get_req_bool(info->play_wind_dust_effect, level_base, "dust_enabled");
    xml_table_get_req_float(info->wind_intensity, level_base, "wind_intensity");
    xml_table_get_req_float(info->wind_direction, level_base, "wind_direction");
    xml_table_get_req_float(info->distance_multiplier, level_base, "distance_multiplier");
    xml_table_get_req_float(info->demolition_damage_multiplier, level_base, "demolition_damage_multiplier");
    info->num_player_weapons = 0;
    char weapon_names[Max_level_weapons][Level_weapon_name_size];
    info->num_player_weapons = xml_table_get_string_list(&weapon_names[0][0], Level_weapon_name_size, Max_level_weapons, xml_table_find_req(level_base, "player_weapons"));
    for (int i = 0; i < info->num_player_weapons; i++)
    {
        info->player_weapons[i] = inv_item_get_info_handle(weapon_names[i]);
    }
    xml_table_get_req_int(info->ideal_min_players, level_base, "ideal_min_players");
    xml_table_get_req_int(info->ideal_max_players, level_base, "ideal_max_players");
}

// 0x009B2550 (:1898): the levels of mp_levels.xtbl whose level files exist (the precache list) and the DLC levels
// whose packfiles exist, sorted by display name
static void init_levels_pool(mempool_base& pool)
{
    char filename[Max_level_name];
    int num_dlc_levels = 0;
    const int num_dlc_bundles = (int)dlc_count_callback();
    if (Use_packfiles)
    {
        game_enable_standard_access();
    }
    for (int i = 0; i < num_dlc_bundles; i++)
    {
        if (!dlc_bundle_contains_mp((unsigned int)i))
        {
            continue;
        }
        sprintf(filename, "dlc%02d_mp_levels.xtbl", dlc_bundle_index_to_id((unsigned int)i));
        if (cf_exist(filename))
        {
            xml_element* root = xml_table_parse(filename);
            if (root != nullptr)
            {
                num_dlc_levels += xml_table_count(root, "mp_level_list");
                xml_table_close(root);
            }
        }
    }
    xml_element* root = multi_game_settings_retrieve_settings_file("mp_levels.xtbl");
    if (root != nullptr)
    {
        const int num_levels = num_dlc_levels + xml_table_count(root, "mp_level_list");
        if (num_levels <= 0)
        {
            // As in rfg.exe: the table stays open and standard access stays on.
            return;
        }
        Level_info_pool.init(pool, (unsigned int)num_levels);
        for (xml_element* entry = xml_table_find_opt(root, "mp_level_list"); entry != nullptr; entry = xml_table_find_next(root, entry, "mp_level_list"))
        {
            xml_table_get_req_string(filename, sizeof(filename), entry, "Filename");
            if (Mp_level_precache_list != nullptr && Mp_level_precache_list->exists(filename))
            {
                multi_level_info* info = Level_info_pool.alloc(0);
                parse_level_entry(filename, info, entry, pool);
            }
        }
        xml_table_close(root);
    }
    for (int i = 0; i < num_dlc_bundles; i++)
    {
        if (!dlc_bundle_contains_mp((unsigned int)i))
        {
            continue;
        }
        const int bundle_id = dlc_bundle_index_to_id((unsigned int)i);
        sprintf(filename, "dlc%02d_mp_levels.xtbl", bundle_id);
        if (!cf_exist(filename))
        {
            continue;
        }
        xml_element* dlc_root = xml_table_parse(filename);
        if (dlc_root == nullptr)
        {
            continue;
        }
        for (xml_element* entry = xml_table_find_opt(dlc_root, "mp_level_list"); entry != nullptr; entry = xml_table_find_next(dlc_root, entry, "mp_level_list"))
        {
            xml_table_get_req_string(filename, sizeof(filename), entry, "Filename");
            char packfile[256] = {};
            if (!Use_packfiles)
            {
                _snprintf_safe(packfile, sizeof(packfile), "%s.%s", filename, "vpp_pc");
            }
            else
            {
                _snprintf_safe(packfile, sizeof(packfile), "%s%s.%s", dlc_path_callback((unsigned int)i), filename, "vpp_pc");
            }
            if (cf_exist(packfile))
            {
                multi_level_info* info = Level_info_pool.alloc(0);
                parse_level_entry(filename, info, entry, pool);
                info->is_downloadable_content = true;
                info->dlc_bundle_id = bundle_id;
            }
        }
        xml_table_close(dlc_root);
    }
    if (Use_packfiles)
    {
        game_disable_standard_access();
    }
    // A selection sort by display name over the pool's storage (the levels were allocated in order).
    const int count = (int)(Level_info_pool.get_capacity() - (unsigned int)Level_info_pool.get_free());
    for (int i = 0; i < count - 1; i++)
    {
        multi_level_info* first = Level_info_pool.get_entry(i);
        multi_level_info* lowest = first;
        int lowest_index = i;
        for (int j = i + 1; j < count; j++)
        {
            multi_level_info* other = Level_info_pool.get_entry(j);
            if (v_stricmp(lowest->display_name, other->display_name) > 0)
            {
                lowest_index = j;
                lowest = other;
            }
        }
        if (lowest_index != i)
        {
            const multi_level_info swap = *lowest;
            *lowest = *first;
            *first = swap;
        }
    }
}

// 0x009B29A0 (:2027): every options set; a set based on a later set fails and the whole table is read again. Original
// bug kept: the sets of the failed pass stay allocated, so this ends only when the pool is used up (the shipped
// table names each base set before its users).
static void init_options_sets(mempool_base& perm_pool, mempool_base& temp_pool)
{
    xml_element* root = multi_game_settings_retrieve_settings_file("mp_options_sets.xtbl");
    if (root == nullptr)
    {
        return;
    }
    Options_sets_pool.init(perm_pool, (unsigned int)xml_table_count(root, "options_set"));
    bool all_parsed;
    do
    {
        all_parsed = true;
        for (xml_element* entry = xml_find(root, "options_set"); entry != nullptr; entry = xml_find_next(root, entry, "options_set"))
        {
            multi_game_options_full_set* set = Options_sets_pool.alloc(0);
            if (set == nullptr)
            {
                return;
            }
            const char* name = xml_table_get_req_string_ref(entry, "Name");
            set->name_crc = name != nullptr ? checksum_stri(name, 0) : checksum_stri::INVALID;
            if (!multi_game_settings_parse_game_option_full_set(*set, entry, perm_pool, temp_pool))
            {
                all_parsed = false;
            }
        }
    } while (!all_parsed && xml_find(root, "options_set") != nullptr);
    xml_table_close(root);
}

// 0x009A1AC0 (:2065)
static const multi_level_info* level_get_first_for_mode(multi_gameplay_mode mode)
{
    Level_info_pool.thread_access.enter();
    for (multi_level_info* info = Level_info_pool.get_first(0); info != nullptr; info = Level_info_pool.get_next(info))
    {
        if ((info->suitable_mode_flags & (1u << mode)) != 0)
        {
            Level_info_pool.thread_access.leave();
            return info;
        }
    }
    Level_info_pool.thread_access.leave();
    return nullptr;
}

// 0x009A1B30 (:2078)
static multi_gameplay_mode mode_get_first_with_any_level()
{
    for (int mode = MGPM_DEATHMATCH; mode < MGPM_NUM_MODES; mode++)
    {
        if (level_get_first_for_mode((multi_gameplay_mode)mode) != nullptr)
        {
            return (multi_gameplay_mode)mode;
        }
    }
    return MGPM_DEATHMATCH;
}

// ---- external functions

// 0x009B2AA0 (:2095): the levels of mp_levels.xtbl whose level files exist
void multi_game_settings_preparse_levels(mempool_base& perm_pool)
{
    xml_element* root = multi_game_settings_retrieve_settings_file("mp_levels.xtbl");
    if (root == nullptr)
    {
        return;
    }
    void* memory = perm_pool.alloc(sizeof(str_hash_table<255, 64, unsigned char>), 0);
    Mp_level_precache_list = memory != nullptr ? new (memory) str_hash_table<255, 64, unsigned char>() : nullptr;
    for (xml_element* entry = xml_table_find_opt(root, "mp_level_list"); entry != nullptr; entry = xml_table_find_next(root, entry, "mp_level_list"))
    {
        char filename[Max_level_name];
        xml_table_get_req_string(filename, sizeof(filename), entry, "Filename");
        if (world_level_exists(filename, true))
        {
            Mp_level_precache_list->insert(filename);
        }
    }
    xml_table_close(root);
}

// 0x009B2B70 (:2123)
void multi_game_settings_system_init(mempool_base& perm_pool, mempool_base& temp_pool)
{
    Settings_packfile = nullptr;
    expand_settings_files(perm_pool);
    init_options_sets(perm_pool, temp_pool);
    init_levels_pool(perm_pool);
    memset(&Mp_server_options, 0, sizeof(Mp_server_options));
    Mp_server_options.set_mode(mode_get_first_with_any_level());
    Playlist_entries = nullptr;
    Playlists = nullptr;
    Num_playlists = 0;
    xml_element* entries = multi_game_settings_retrieve_settings_file("mp_playlist_entries.xtbl");
    parse_playlist_entries(entries, perm_pool);
    xml_table_close(entries);
    xml_element* playlists = multi_game_settings_retrieve_settings_file("mp_playlists.xtbl");
    parse_playlists(playlists, perm_pool);
    xml_table_close(playlists);
    Current_user_playlist_version = (unsigned int)Settings_version;
}

// 0x00962ED0 (:2139)
void multi_game_settings_system_shutdown()
{
}

// 0x009A1BF0 (:2144)
const multi_level_info* multi_game_settings_find_level_by_name(const char* name)
{
    Level_info_pool.thread_access.enter();
    for (multi_level_info* info = Level_info_pool.get_first(0); info != nullptr; info = Level_info_pool.get_next(info))
    {
        if (v_stricmp(info->display_name, name) == 0)
        {
            Level_info_pool.thread_access.leave();
            return info;
        }
    }
    Level_info_pool.thread_access.leave();
    return nullptr;
}

// 0x009A1C60 (:2157)
const multi_level_info* multi_game_settings_find_level_by_filename(const char* filename)
{
    Level_info_pool.thread_access.enter();
    for (multi_level_info* info = Level_info_pool.get_first(0); info != nullptr; info = Level_info_pool.get_next(info))
    {
        if (v_stricmp(info->filename, filename) == 0)
        {
            Level_info_pool.thread_access.leave();
            return info;
        }
    }
    Level_info_pool.thread_access.leave();
    return nullptr;
}

// 0x009A1CD0 (:2170)
const multi_game_options_full_set* multi_game_settings_find_game_option_full_set(const char* set_name)
{
    const checksum_stri crc(set_name, 0);
    Options_sets_pool.thread_access.enter();
    for (multi_game_options_full_set* set = Options_sets_pool.get_first(0); set != nullptr; set = Options_sets_pool.get_next(set))
    {
        if (set->name_crc == crc)
        {
            Options_sets_pool.thread_access.leave();
            return set;
        }
    }
    Options_sets_pool.thread_access.leave();
    return nullptr;
}

// 0x009A1D50 (:2185): per mode, the generic options (required without a base set) and the mode's options set; false
// when the base set is not known yet
bool multi_game_settings_parse_game_option_full_set(multi_game_options_full_set& set, xml_element* options_base, mempool_base& perm_pool, mempool_base& temp_pool)
{
    bool parsed = true;
    const char* name = xml_table_get_req_string_ref(options_base, "Name");
    set.name_crc = name != nullptr ? checksum_stri(name, 0) : checksum_stri::INVALID;
    const multi_game_options_full_set* base = nullptr;
    const char* base_name = xml_table_get_opt_string_ref(options_base, "base_options_set");
    if (base_name != nullptr)
    {
        base = multi_game_settings_find_game_option_full_set(base_name);
        if (base == nullptr)
        {
            parsed = false;
        }
    }
    multi_game_generic_options* generic[MGPM_NUM_MODES];
    for (int i = 0; i < MGPM_NUM_MODES; i++)
    {
        generic[i] = (multi_game_generic_options*)perm_pool.alloc(sizeof(multi_game_generic_options), 0);
    }
    xml_element* mode_overrides = xml_table_find_req(options_base, "mode_specific_option_overrides");
    if (mode_overrides == nullptr)
    {
        return false;
    }
    const bool required = base == nullptr;
    for (int i = 0; i < MGPM_NUM_MODES; i++)
    {
        const multi_gameplay_mode mode = (multi_gameplay_mode)i;
        set.sets[i].generic_options = nullptr;
        set.sets[i].mode_specific_options = nullptr;
        xml_element* generic_overrides = xml_table_find_req(options_base, "generic_option_overrides");
        parse_generic_options(generic[i], generic_overrides, required, base != nullptr ? base->sets[i].generic_options : nullptr);
        xml_element* mode_base = xml_table_find_opt(mode_overrides, multi_gameplay_mode_get_internal_name(mode));
        multi_game_settings_parse_game_option_set(set.sets[i], mode_base, perm_pool, mode, required, generic[i],
            base != nullptr ? base->sets[i].mode_specific_options : nullptr);
    }
    return parsed;
}

// 0x00962EE0 (:2233): a mode's own generic overrides (on top of the set's) and its mode options (required, or when
// the mode names any)
void multi_game_settings_parse_game_option_set(multi_game_options_set& set, xml_element* options_base, mempool_base& perm_pool, multi_gameplay_mode mode, bool mode_specific_required, const multi_game_generic_options* generic_defaults, const multi_game_mode_options* mode_specific_defaults)
{
    xml_element* generic_overrides = xml_table_find_opt(options_base, "generic_option_overrides");
    bool mode_has_options;
    const bool mode_options_named = mode_specific_options_exist(options_base, mode, mode_has_options);
    if (generic_overrides != nullptr)
    {
        multi_game_generic_options* generic = (multi_game_generic_options*)perm_pool.alloc(sizeof(multi_game_generic_options), 4);
        parse_generic_options(generic, generic_overrides, false, generic_defaults);
        set.generic_options = generic;
    }
    else
    {
        set.generic_options = generic_defaults;
    }
    if (!mode_options_named && !mode_specific_required)
    {
        set.mode_specific_options = mode_specific_defaults;
        return;
    }
    multi_game_mode_options* mode_options = (multi_game_mode_options*)perm_pool.alloc(sizeof(multi_game_mode_options), 4);
    parse_mode_specific_options(mode_options, options_base, mode, mode_specific_required, mode_specific_defaults);
    set.mode_specific_options = mode_options;
}

// 0x00984510 (:2264): the mode, friendly fire, the level's filename crc, the generic options and the mode's options
void multi_game_append_server_options_to_packet(unsigned char* packet_data, unsigned short& packet_size, const multi_game_settings* const src)
{
    write_u32_be(packet_data, packet_size, (unsigned int)src->get_mode());
    packet_data[packet_size++] = src->generic_options.friendly_fire;
    const multi_level_info* level = src->get_level();
    write_u32_be(packet_data, packet_size, level != nullptr ? level->filename_crc.get_crc() : checksum_stri::INVALID.get_crc());
    const multi_game_generic_options& generic = src->generic_options;
    write_u16_be(packet_data, packet_size, generic.point_limit);
    packet_data[packet_size++] = generic.health_regen_speed;
    write_u16_be(packet_data, packet_size, generic.max_health);
    packet_data[packet_size++] = generic.movement_speed;
    packet_data[packet_size++] = generic.round_time_limit_minutes;
    packet_data[packet_size++] = generic.num_rounds;
    packet_data[packet_size++] = generic.respawn_time_seconds;
    packet_data[packet_size++] = (unsigned char)generic.rubberbanding_level;
    packet_data[packet_size++] = (unsigned char)generic.enemy_icon_visibility;
    packet_data[packet_size++] = (unsigned char)generic.time_of_day;
    packet_data[packet_size++] = (unsigned char)generic.backpack_spawns;
    write_u32_be(packet_data, packet_size, (unsigned int)generic.backpack_spawn_type);
    packet_data[packet_size++] = (unsigned char)generic.starting_backpack;
    write_u32_be(packet_data, packet_size, (unsigned int)generic.starting_backpack_type);
    packet_data[packet_size++] = generic.unlimited_ammo;
    packet_data[packet_size++] = generic.replace_starting_weapon;
    write_u32_be(packet_data, packet_size, generic.starting_weapon);
    write_u32_be(packet_data, packet_size, generic.third_weapon);
    write_u32_be(packet_data, packet_size, generic.fourth_weapon);
    packet_data[packet_size++] = (unsigned char)generic.weapon_respawn_speed;
    write_u16_be(packet_data, packet_size, generic.locked_weapons_mask);
    write_u16_be(packet_data, packet_size, generic.locked_backpacks_mask);
    const multi_game_mode_options& mode = src->mode_options;
    switch (src->get_mode())
    {
    case MGPM_DEATHMATCH:
    case MGPM_KING_OF_THE_HILL:
        packet_data[packet_size++] = mode.deathmatch.leader_highlighting;
        break;
    case MGPM_CAPTURE_THE_FLAG:
        packet_data[packet_size++] = mode.capture_the_flag.flag_carrier_speed;
        packet_data[packet_size++] = mode.capture_the_flag.flag_carrier_regen_speed;
        packet_data[packet_size++] = mode.capture_the_flag.flag_return_time_seconds;
        packet_data[packet_size++] = mode.capture_the_flag.civilian_flag_carrier;
        packet_data[packet_size++] = mode.capture_the_flag.flag_touch_return;
        packet_data[packet_size++] = mode.capture_the_flag.flag_carrier_icon_always_on;
        break;
    case MGPM_SIEGE:
        packet_data[packet_size++] = mode.siege.percent_destruction_required;
        write_u32_be(packet_data, packet_size, (unsigned int)mode.siege.defense_starting_backpack_type);
        write_u32_be(packet_data, packet_size, mode.siege.defense_starting_weapon);
        write_u32_be(packet_data, packet_size, mode.siege.defense_third_weapon);
        write_u32_be(packet_data, packet_size, mode.siege.defense_fourth_weapon);
        break;
    case MGPM_BAGMAN:
    case MGPM_TEAM_BAGMAN:
        packet_data[packet_size++] = mode.bagman.bag_return_time_seconds;
        packet_data[packet_size++] = mode.bagman.bagman_kill_bonus;
        break;
    case MGPM_DEMOLITION:
        packet_data[packet_size++] = mode.demolition.engineer_icon_always_on;
        write_u32_be(packet_data, packet_size, mode.demolition.engineer_weapon);
        break;
    default:
        break;
    }
}

// 0x009B2C60 (:2338): the server's options replace ours: the mode is set (with its default level and options), then
// the server's level (by crc, null when we do not know it) and the received options. The received byte values of
// the enums are not sign extended (as in rfg.exe: MRL_NONE arrives as 255).
void multi_game_read_server_options_from_packet(multi_game_settings* const dest, unsigned char* packet_data, int& packet_offset)
{
    multi_game_generic_options generic;
    // Port deviation: rfg.exe leaves the mode options of a mode without any (team deathmatch) as stack garbage.
    multi_game_mode_options mode_options = {};
    const multi_gameplay_mode mode = (multi_gameplay_mode)read_u32_be(packet_data, packet_offset);
    generic.friendly_fire = packet_data[packet_offset++] != 0;
    const unsigned int level_crc = read_u32_be(packet_data, packet_offset);
    generic.point_limit = read_u16_be(packet_data, packet_offset);
    generic.health_regen_speed = packet_data[packet_offset++];
    generic.max_health = read_u16_be(packet_data, packet_offset);
    generic.movement_speed = packet_data[packet_offset++];
    generic.round_time_limit_minutes = packet_data[packet_offset++];
    generic.num_rounds = packet_data[packet_offset++];
    generic.respawn_time_seconds = packet_data[packet_offset++];
    generic.rubberbanding_level = (multi_rubberbanding_levels)packet_data[packet_offset++];
    generic.enemy_icon_visibility = (multi_game_enemy_icon_visibilities)packet_data[packet_offset++];
    generic.time_of_day = (multi_game_times_of_day)packet_data[packet_offset++];
    generic.backpack_spawns = (multi_game_backpack_spawn_options)packet_data[packet_offset++];
    generic.backpack_spawn_type = (int)read_u32_be(packet_data, packet_offset);
    generic.starting_backpack = (multi_game_starting_backpack_options)packet_data[packet_offset++];
    generic.starting_backpack_type = (int)read_u32_be(packet_data, packet_offset);
    generic.unlimited_ammo = packet_data[packet_offset++] != 0;
    generic.replace_starting_weapon = packet_data[packet_offset++] != 0;
    generic.starting_weapon = read_u32_be(packet_data, packet_offset);
    generic.third_weapon = read_u32_be(packet_data, packet_offset);
    generic.fourth_weapon = read_u32_be(packet_data, packet_offset);
    generic.weapon_respawn_speed = (multi_game_weapon_respawn_speeds)packet_data[packet_offset++];
    generic.locked_weapons_mask = read_u16_be(packet_data, packet_offset);
    generic.locked_backpacks_mask = read_u16_be(packet_data, packet_offset);
    const multi_level_info* level = nullptr;
    Level_info_pool.thread_access.enter();
    for (multi_level_info* info = Level_info_pool.get_first(0); info != nullptr; info = Level_info_pool.get_next(info))
    {
        if (info->filename_crc.get_crc() == level_crc)
        {
            level = info;
            break;
        }
    }
    Level_info_pool.thread_access.leave();
    switch (mode)
    {
    case MGPM_DEATHMATCH:
        mode_options.deathmatch.leader_highlighting = packet_data[packet_offset++] != 0;
        break;
    case MGPM_CAPTURE_THE_FLAG:
        mode_options.capture_the_flag.flag_carrier_speed = packet_data[packet_offset++];
        mode_options.capture_the_flag.flag_carrier_regen_speed = packet_data[packet_offset++];
        mode_options.capture_the_flag.flag_return_time_seconds = packet_data[packet_offset++];
        mode_options.capture_the_flag.civilian_flag_carrier = packet_data[packet_offset++] != 0;
        mode_options.capture_the_flag.flag_touch_return = packet_data[packet_offset++] != 0;
        mode_options.capture_the_flag.flag_carrier_icon_always_on = packet_data[packet_offset++] != 0;
        break;
    case MGPM_SIEGE:
        mode_options.siege.percent_destruction_required = packet_data[packet_offset++];
        mode_options.siege.defense_starting_backpack_type = (int)read_u32_be(packet_data, packet_offset);
        mode_options.siege.defense_starting_weapon = read_u32_be(packet_data, packet_offset);
        mode_options.siege.defense_third_weapon = read_u32_be(packet_data, packet_offset);
        mode_options.siege.defense_fourth_weapon = read_u32_be(packet_data, packet_offset);
        break;
    case MGPM_KING_OF_THE_HILL:
        mode_options.king_of_the_hill.all_target_multiplier = (king_of_the_hill_multipliers)packet_data[packet_offset++];
        break;
    case MGPM_BAGMAN:
    case MGPM_TEAM_BAGMAN:
        mode_options.bagman.bag_return_time_seconds = packet_data[packet_offset++];
        mode_options.bagman.bagman_kill_bonus = packet_data[packet_offset++] != 0;
        break;
    case MGPM_DEMOLITION:
        mode_options.demolition.engineer_icon_always_on = packet_data[packet_offset++] != 0;
        mode_options.demolition.engineer_weapon = read_u32_be(packet_data, packet_offset);
        break;
    default:
        break;
    }
    dest->set_mode(mode);
    dest->set_level(level);
    copy_generic_options(&dest->generic_options, &generic, nullptr);
    dest->mode_options = mode_options;
}

// 0x009B33B0 (:2429): a custom match keeps its settings for the next match once
void multi_game_settings::reinit()
{
    if (!last_match_was_custom_game)
    {
        memset(this, 0, sizeof(*this));
        current_mode = MGPM_NONE;
        set_mode(mode_get_first_with_any_level());
    }
    last_match_was_custom_game = false;
}

// 0x009A1EC0 (:2441): a mode change keeps the level when it suits the new mode, else takes the mode's last or first
// level; the options become the level's defaults for the mode
void multi_game_settings::set_mode(multi_gameplay_mode mode)
{
    // As in rfg.exe: MGPM_NUM_MODES passes this check.
    if ((int)mode > MGPM_NUM_MODES)
    {
        mode = MGPM_DEATHMATCH;
    }
    const multi_gameplay_mode previous_mode = current_mode;
    current_mode = mode;
    if (current_level == nullptr || (current_level->suitable_mode_flags & (1u << mode)) == 0)
    {
        set_level(mode_levels[mode] != nullptr ? mode_levels[mode] : level_get_first_for_mode(mode));
    }
    reset_options_to_default(previous_mode);
}

// 0x00962F90 (:2461)
multi_gameplay_mode multi_game_settings::get_mode() const
{
    return current_mode;
}

// Port helper: a mode the menus can offer (the bagman modes need the DLC) with a level (rfg.exe inlines this into the
// mode cycling).
static bool mode_is_offered(multi_gameplay_mode mode)
{
    if (!Bagman_dlc_unlocked && (mode == MGPM_BAGMAN || mode == MGPM_TEAM_BAGMAN))
    {
        return false;
    }
    return level_get_first_for_mode(mode) != nullptr;
}

// 0x009A1F20 (:2466)
multi_gameplay_mode multi_game_settings::next_mode() const
{
    for (int step = 1; step < MGPM_NUM_MODES; step++)
    {
        const multi_gameplay_mode mode = (multi_gameplay_mode)((current_mode + step) % MGPM_NUM_MODES);
        if (mode_is_offered(mode))
        {
            return mode;
        }
    }
    return current_mode;
}

// 0x009A2020 (:2480)
multi_gameplay_mode multi_game_settings::prev_mode() const
{
    for (int step = 1; step < MGPM_NUM_MODES; step++)
    {
        const multi_gameplay_mode mode = (multi_gameplay_mode)((current_mode - step + MGPM_NUM_MODES) % MGPM_NUM_MODES);
        if (mode_is_offered(mode))
        {
            return mode;
        }
    }
    return current_mode;
}

// 0x009A2120 (:2494): one of the offered modes at random (counted from the current one)
multi_gameplay_mode multi_game_settings::rand_mode() const
{
    int count = 0;
    for (int step = 0; step < MGPM_NUM_MODES; step++)
    {
        if (mode_is_offered((multi_gameplay_mode)((current_mode + step) % MGPM_NUM_MODES)))
        {
            count++;
        }
    }
    if (count <= 0)
    {
        return current_mode;
    }
    // As in rfg.exe: the pick is decremented before it is tested, so picks 0 and 1 both give the first mode.
    int pick = (int)(vrand() % (unsigned int)count);
    for (int step = 0; step < MGPM_NUM_MODES; step++)
    {
        const multi_gameplay_mode mode = (multi_gameplay_mode)((current_mode + step) % MGPM_NUM_MODES);
        if (mode_is_offered(mode) && --pick < 1)
        {
            return mode;
        }
    }
    return current_mode;
}

// 0x00962FA0 (:2531): a level with options for the current mode becomes the last level of every mode it suits
void multi_game_settings::set_level(const multi_level_info* info)
{
    current_level = info;
    if (info == nullptr || info->options->sets[current_mode].mode_specific_options == nullptr)
    {
        return;
    }
    for (int mode = 0; mode < MGPM_NUM_MODES; mode++)
    {
        if ((current_level->suitable_mode_flags & (1u << mode)) != 0)
        {
            mode_levels[mode] = current_level;
        }
    }
}

// 0x00963040 (:2547)
const multi_level_info* multi_game_settings::get_level() const
{
    return current_level;
}

// Port helper: the level after or before another in the pool's list, wrapping (rfg.exe inlines the pool walk).
static multi_level_info* level_after(multi_level_info* info)
{
    multi_level_info* next = Level_info_pool.get_next(info);
    return next != nullptr ? next : Level_info_pool.get_first(0);
}

static multi_level_info* level_before(multi_level_info* info)
{
    multi_level_info* prev = Level_info_pool.get_prev(info);
    return prev != nullptr ? prev : Level_info_pool.get_last(0);
}

// 0x009A2290 (:2552): the next level suiting the mode (the current one when none other does)
const multi_level_info* multi_game_settings::next_level() const
{
    multi_level_info* current = (multi_level_info*)current_level;
    for (multi_level_info* info = level_after(current); info != current; info = level_after(info))
    {
        if ((info->suitable_mode_flags & (1u << current_mode)) != 0)
        {
            return info;
        }
    }
    return current;
}

// 0x009A2340 (:2567)
const multi_level_info* multi_game_settings::prev_level() const
{
    multi_level_info* current = (multi_level_info*)current_level;
    for (multi_level_info* info = level_before(current); info != current; info = level_before(info))
    {
        if ((info->suitable_mode_flags & (1u << current_mode)) != 0)
        {
            return info;
        }
    }
    return current;
}

// 0x009A2420 (:2582): one of the levels suiting the mode at random (the pick is decremented first, as in rand_mode)
const multi_level_info* multi_game_settings::rand_level() const
{
    int count = 0;
    Level_info_pool.thread_access.enter();
    for (multi_level_info* info = Level_info_pool.get_first(0); info != nullptr; info = Level_info_pool.get_next(info))
    {
        if ((info->suitable_mode_flags & (1u << current_mode)) != 0)
        {
            count++;
        }
    }
    Level_info_pool.thread_access.leave();
    if (count <= 0)
    {
        return nullptr;
    }
    int pick = (int)(vrand() % (unsigned int)count);
    Level_info_pool.thread_access.enter();
    for (multi_level_info* info = Level_info_pool.get_first(0); info != nullptr; info = Level_info_pool.get_next(info))
    {
        if ((info->suitable_mode_flags & (1u << current_mode)) != 0 && --pick < 1)
        {
            Level_info_pool.thread_access.leave();
            return info;
        }
    }
    Level_info_pool.thread_access.leave();
    return nullptr;
}

// 0x00963050 (:2613): the level's defaults for the current mode, except the generic options whose defaults are the
// same in the previous mode (those keep their values). LTCG: the current mode's defaults in EDI.
void multi_game_settings::reset_options_to_default(multi_gameplay_mode previous_mode)
{
    if (current_level == nullptr)
    {
        return;
    }
    const multi_game_generic_options* previous_defaults = nullptr;
    if (previous_mode != MGPM_NONE)
    {
        previous_defaults = current_level->options->sets[previous_mode].generic_options;
    }
    copy_generic_options(&generic_options, current_level->options->sets[current_mode].generic_options, previous_defaults);
    const multi_game_mode_options* mode_defaults = current_level->options->sets[current_mode].mode_specific_options;
    if (mode_defaults != nullptr)
    {
        mode_options = *mode_defaults;
    }
}

// 0x009630D0 (:2626): the same mode, level and options
bool multi_game_settings::is_equal(const multi_game_settings& rhs) const
{
    if (current_mode != rhs.current_mode || current_level != rhs.current_level)
    {
        return false;
    }
    for (int i = 0; i < NUM_MULTI_GAME_OPTIONS; i++)
    {
        const multi_game_options option = (multi_game_options)i;
        const bool equal = option_is_mode_specific(option) ? option_is_equal(option, current_mode, rhs.mode_options, mode_options)
                                                           : option_is_equal(option, generic_options, rhs.generic_options);
        if (!equal)
        {
            return false;
        }
    }
    return true;
}

// 0x00963140 (:2650): from the settings packfile when one is loaded (never in retail), else from the file system
xml_element* multi_game_settings_retrieve_settings_file(const char* filename)
{
    if (Settings_packfile != nullptr)
    {
        for (unsigned int i = 0; i < Settings_packfile->num_files; i++)
        {
            if (v_stricmp(Settings_packfile->dir[i].name, filename) == 0)
            {
                xml_element* root = xml_table_parse_from_memory(Settings_packfile->data + Settings_packfile->dir[i].start);
                if (root != nullptr)
                {
                    return root;
                }
                break;
            }
        }
    }
    return xml_table_parse(filename);
}

// 0x009631E0 (:2675)
unsigned int multi_game_settings_version()
{
    return (unsigned int)Settings_version;
}

// 0x009631F0 (:2684)
unsigned int multi_game_settings_available_dlc_flags()
{
    return Available_dlc_flags;
}

// 0x00984B00 (:3280)
int multi_game_settings_num_levels()
{
    return (int)Level_info_pool.get_capacity();
}

// 0x00984B10 (:3285)
const char* multi_game_settings_get_level_name_by_index(int index)
{
    if ((unsigned int)index < Level_info_pool.get_capacity())
    {
        return Level_info_pool.get_entry(index)->filename;
    }
    return nullptr;
}

// 0x00984B30 (:3290). Original bug kept: an index out of range reads the crc of a null level.
int multi_game_settings_get_level_name_crc_by_index(int index)
{
    if ((unsigned int)index < Level_info_pool.get_capacity())
    {
        return (int)Level_info_pool.get_entry(index)->filename_crc.get_crc();
    }
    const multi_level_info* volatile none = nullptr;
    return (int)none->filename_crc.get_crc();
}

// 0x00984B60 (:3295)
const multi_level_info* multi_game_settings_get_level_by_index(int index)
{
    if ((unsigned int)index < Level_info_pool.get_capacity())
    {
        return Level_info_pool.get_entry(index);
    }
    return nullptr;
}

// 0x00984B80 (:3300): "HH:MM" or "H:MM" (anything else is 13:00), clamped to 0:00-23:59
void multi_game_settings_parse_time_of_day(int* hour, int* minute, const char* time_of_day)
{
    const char* text = time_of_day;
    size_t hour_digits = 2;
    const size_t length = strlen(time_of_day);
    if (length == 5)
    {
        if (time_of_day[2] != ':')
        {
            text = "13:00";
        }
    }
    else if (length == 4 && time_of_day[1] == ':')
    {
        hour_digits = 1;
    }
    else
    {
        text = "13:00";
    }
    char hour_text[4] = {};
    char minute_text[4] = {};
    strncpy(hour_text, text, hour_digits);
    strncpy(minute_text, text + hour_digits + 1, 2);
    *hour = atoi(hour_text);
    if (*hour < 0)
    {
        *hour = 0;
    }
    else if (*hour > 23)
    {
        *hour = 23;
    }
    *minute = atoi(minute_text);
    if (*minute < 0)
    {
        *minute = 0;
    }
    else if (*minute > 59)
    {
        *minute = 59;
    }
}

// 0x00963A10 (:3338)
void multi_game_settings_swap_weapon_bit(multi_game_settings* settings, multi_game_lockable_weapons weapon)
{
    settings->generic_options.locked_weapons_mask ^= (unsigned short)(1 << weapon);
}

// 0x00963A30 (:3343)
void multi_game_settings_swap_backpack_bit(multi_game_settings* settings, multi_game_lockable_backpacks backpack)
{
    settings->generic_options.locked_backpacks_mask ^= (unsigned short)(1 << backpack);
}

// 0x00963A50 (:3348)
bool multi_game_settings_is_weapon_locked(multi_game_settings* settings, multi_game_lockable_weapons weapon)
{
    return (settings->generic_options.locked_weapons_mask & (1u << weapon)) != 0;
}

// 0x00963A70 (:3353)
bool multi_game_settings_is_backpack_locked(multi_game_settings* settings, multi_game_lockable_backpacks backpack)
{
    return (settings->generic_options.locked_backpacks_mask & (1u << backpack)) != 0;
}

// 0x00963A90 (:3358): by backpack id (the ids are the lockable backpack order)
bool multi_game_settings_is_backpack_locked(int backpack_id)
{
    if (backpack_id < 0 || backpack_id >= NUM_MULTI_GAME_LOCKABLE_BACKPACKS)
    {
        return false;
    }
    return (Mp_server_options.generic_options.locked_backpacks_mask & (1u << backpack_id)) != 0;
}

// UI.md UI5: the option texts the multiplayer menus show (OpenRFG reached them through generated stubs before: a null
// option name crashed the game lobby's game info).

// 0x009A25A0 (:2692)
// The option as the "Default" option set's for the settings' mode (no such set: everything is default). Options past
// the last one are never default.
bool multi_game_settings_is_option_default(const multi_game_settings* set, multi_game_options option)
{
    if (set == nullptr || option > MGO_LOCK_BACKPACK_TREMOR)
    {
        return false;
    }
    const multi_game_options_full_set* defaults = multi_game_settings_find_game_option_full_set("Default");
    if (defaults == nullptr)
    {
        return true;
    }
    const multi_game_options_set& mode_defaults = defaults->sets[set->get_mode()];
    if (option_is_mode_specific(option))
    {
        return option_is_equal(option, set->get_mode(), *mode_defaults.mode_specific_options, set->mode_options);
    }
    return option_is_equal(option, set->generic_options, *mode_defaults.generic_options);
}

// 0x00963200 (:2711)
const char* multi_game_settings_get_option_name(multi_game_options option)
{
    if (option < NUM_MULTI_GAME_OPTIONS)
    {
        return Multi_game_option_info[option].name;
    }
    return "MENU_NO_OPTION";
}

// 0x00963220 (:2722)
// The option's localized description (the game mode's mentions bagman once that DLC is unlocked), else the tag itself;
// "no option" for an unknown option.
void multi_game_settings_get_option_description(wchar_t* buffer, int buffer_len, multi_game_options option)
{
    wcsncpy(buffer, localization_get_display_string_req("MENU_NO_OPTION_DESC", nullptr), buffer_len);
    if (option >= NUM_MULTI_GAME_OPTIONS)
    {
        return;
    }
    const char* tag = Multi_game_option_info[option].description;
    if (option == MGO_GAME_MODE && Bagman_dlc_unlocked)
    {
        tag = "MENU_GAME_MODE_PLUS_BAGMAN_DESC_PATCH";
    }
    if (localization_does_tag_exist(tag))
    {
        wcsncpy(buffer, localization_get_display_string_req(tag, nullptr), buffer_len);
        buffer[buffer_len - 1] = L'\0';
        return;
    }
    copy_to_wide(buffer, tag, (unsigned int)buffer_len);
}

// A weapon's display name ("!!ERROR" without one).
static const char* option_text_weapon_name(unsigned int weapon)
{
    const weapon_info* info = weapons_get_weapon_info(weapon);
    if (info != nullptr && info->weapon_inv_item_info != nullptr && info->weapon_inv_item_info->display_name != nullptr)
    {
        return info->weapon_inv_item_info->display_name;
    }
    return "!!ERROR";
}

// "On" or "off".
static const char* option_text_on_off(bool on)
{
    return on ? "MENU_ON" : "MENU_OFF";
}

// 0x009632B0 (:2753)
// The option's value as menu text (a localization tag, a name or a number). Magic numbers: 0xFFFFFFFF is no weapon;
// times print "m:ss" from seconds; the factors print "%d%%%%" (two percent signs, as rfg.exe's format).
void multi_game_settings_get_option_text(char* output_buffer, int buffer_size_in, const multi_game_settings* set, multi_game_options option)
{
    if (output_buffer == nullptr || buffer_size_in < 1 || set == nullptr || option > MGO_LOCK_BACKPACK_TREMOR)
    {
        return;
    }
    output_buffer[0] = '\0';
    const multi_game_generic_options& generic = set->generic_options;
    const multi_game_mode_options& mode = set->mode_options;
    const char* text = nullptr;
    switch (option)
    {
    case MGO_GAME_MODE:
        text = multi_gameplay_mode_get_display_name(set->get_mode());
        break;
    case MGO_LEVEL:
        if (set->get_level() != nullptr)
        {
            text = set->get_level()->display_name;
            break;
        }
        _snprintf_safe(output_buffer, (unsigned int)buffer_size_in, "%s", "N/A");
        return;
    case MGO_SCORE_LIMIT:
        if (generic.point_limit == 0)
        {
            strncpy_safe(output_buffer, "MENU_NONE", buffer_size_in);
        }
        else
        {
            _snprintf_safe(output_buffer, (unsigned int)buffer_size_in, "%d", (int)generic.point_limit);
        }
        if (set->get_mode() == MGPM_SIEGE)
        {
            strncpy_safe(output_buffer, "MENU_NOT_APPLICABLE", buffer_size_in);
        }
        return;
    case MGO_TIME_LIMIT:
        _snprintf_safe(output_buffer, (unsigned int)buffer_size_in, "%d:00", (int)generic.round_time_limit_minutes);
        return;
    case MGO_FRIENDLY_FIRE:
        text = option_text_on_off(generic.friendly_fire);
        break;
    case MGO_RUBBERBANDING_LEVEL:
        text = multi_game_rubberbanding_get_level_display_name(generic.rubberbanding_level);
        break;
    case MGO_ENEMY_ICONS:
        if (generic.enemy_icon_visibility == MGEIV_NONE)
        {
            text = "MENU_NONE";
        }
        else if (generic.enemy_icon_visibility == MGEIV_DELAYED)
        {
            text = "MENU_DELAYED";
        }
        else if (generic.enemy_icon_visibility == MGEIV_ALWAYS)
        {
            text = "MENU_ALWAYS";
        }
        else
        {
            text = "MENU_DEFAULT_OPTION";
        }
        break;
    case MGO_TIME_OF_DAY:
        if (generic.time_of_day == MGTOD_RANDOM)
        {
            text = "MENU_RANDOM";
        }
        else if (generic.time_of_day == MGTOD_DEFAULT)
        {
            text = "MENU_DEFAULT_OPTION";
        }
        else if (generic.time_of_day == MGTOD_ALTERNATE)
        {
            text = "MENU_ALTERNATE";
        }
        break;
    case MGO_RESPAWN_DELAY:
        _snprintf_safe(output_buffer, (unsigned int)buffer_size_in, "%d:%02d", generic.respawn_time_seconds / 60, generic.respawn_time_seconds % 60);
        return;
    case MGO_BACKPACKS:
        if (generic.backpack_spawns == MGBSO_DEFAULT)
        {
            text = "MENU_DEFAULT_OPTION";
        }
        else if (generic.backpack_spawns == MGBSO_SHUFFLE)
        {
            text = "MENU_SHUFFLE";
        }
        else if (generic.backpack_spawns == MGBSO_NONE)
        {
            text = "MENU_NONE";
        }
        else if (generic.backpack_spawns == MGBSO_SPECIFIC)
        {
            text = multi_game_backpack_get_display_name(generic.backpack_spawn_type);
        }
        break;
    case MGO_STARTING_BACKPACK:
        if (generic.starting_backpack == MGSBO_NONE)
        {
            text = "MENU_NONE";
        }
        else if (generic.starting_backpack == MGSBO_RANDOM)
        {
            text = "MENU_RANDOM";
        }
        else if (generic.starting_backpack == MGSBO_SPECIFIC)
        {
            text = multi_game_backpack_get_display_name(generic.starting_backpack_type);
        }
        break;
    case MGO_HEALTH_REGEN_FACTOR:
        _snprintf_safe(output_buffer, (unsigned int)buffer_size_in, "%d%%%%", (int)generic.health_regen_speed);
        return;
    case MGO_HEALTH_FACTOR:
        _snprintf_safe(output_buffer, (unsigned int)buffer_size_in, "%d%%%%", (int)generic.max_health);
        return;
    case MGO_MOVEMENT_FACTOR:
        _snprintf_safe(output_buffer, (unsigned int)buffer_size_in, "%d%%%%", (int)generic.movement_speed);
        return;
    case MGO_UNLIMITED_AMMO:
        text = option_text_on_off(generic.unlimited_ammo);
        break;
    case MGO_STARTING_WEAPON:
        if (!generic.replace_starting_weapon)
        {
            text = "MENU_DEFAULT_OPTION";
        }
        else
        {
            text = generic.starting_weapon != 0xFFFFFFFF ? option_text_weapon_name(generic.starting_weapon) : "MENU_SLEDGEHAMMER_ONLY";
        }
        break;
    case MGO_THIRD_WEAPON:
        text = generic.third_weapon != 0xFFFFFFFF ? option_text_weapon_name(generic.third_weapon) : "MENU_NONE";
        break;
    case MGO_FOURTH_WEAPON:
        text = generic.fourth_weapon != 0xFFFFFFFF ? option_text_weapon_name(generic.fourth_weapon) : "MENU_NONE";
        break;
    case MGO_WEAPON_RESPAWN_SPEED:
        if (generic.weapon_respawn_speed == MGWRS_NORMAL)
        {
            text = "MENU_DEFAULT_OPTION";
        }
        else if (generic.weapon_respawn_speed == MGWRS_FAST)
        {
            text = "MENU_FAST";
        }
        else if (generic.weapon_respawn_speed == MGWRS_ALWAYS)
        {
            text = "MENU_ALWAYS";
        }
        else if (generic.weapon_respawn_speed == MGWRS_NONE)
        {
            text = "MENU_NONE";
        }
        else if (generic.weapon_respawn_speed == MGWRS_SLOW)
        {
            text = "MENU_SLOW";
        }
        break;
    case MGO_ANARCHY_LEADER_HIGHLIGHTING:
        text = option_text_on_off(mode.deathmatch.leader_highlighting);
        break;
    case MGO_CTF_FLAG_RETURN_TIME:
        _snprintf_safe(output_buffer, (unsigned int)buffer_size_in, "%d:%02d", mode.capture_the_flag.flag_return_time_seconds / 60, mode.capture_the_flag.flag_return_time_seconds % 60);
        return;
    case MGO_CTF_FLAG_TOUCH_RETURN:
        text = option_text_on_off(mode.capture_the_flag.flag_touch_return);
        break;
    case MGO_CTF_CARRIER_ICON_ALWAYS_ON:
        text = option_text_on_off(mode.capture_the_flag.flag_carrier_icon_always_on);
        break;
    case MGO_SIEGE_DEFENSE_STARTING_BACKPACK:
        text = mode.siege.defense_starting_backpack_type != -1 ? multi_game_backpack_get_display_name(mode.siege.defense_starting_backpack_type) : "MENU_DEFAULT_OPTION";
        break;
    case MGO_SIEGE_DEFENSE_STARTING_WEAPON:
        text = mode.siege.defense_starting_weapon != 0xFFFFFFFF ? option_text_weapon_name(mode.siege.defense_starting_weapon) : "MENU_DEFAULT_OPTION";
        break;
    case MGO_SIEGE_DEFENSE_THIRD_WEAPON:
        text = mode.siege.defense_third_weapon != 0xFFFFFFFF ? option_text_weapon_name(mode.siege.defense_third_weapon) : "MENU_DEFAULT_OPTION";
        break;
    case MGO_SIEGE_DEFENSE_FOURTH_WEAPON:
        text = mode.siege.defense_fourth_weapon != 0xFFFFFFFF ? option_text_weapon_name(mode.siege.defense_fourth_weapon) : "MENU_DEFAULT_OPTION";
        break;
    case MGO_DC_ALL_TARGET_MULTIPLIER:
        if (mode.king_of_the_hill.all_target_multiplier == KOTHM_NORMAL)
        {
            text = "MENU_DEFAULT_OPTION";
        }
        else if (mode.king_of_the_hill.all_target_multiplier == KOTHM_NONE)
        {
            text = "MENU_NONE";
        }
        else if (mode.king_of_the_hill.all_target_multiplier == KOTHM_ONLY)
        {
            text = "MENU_ONLY";
        }
        break;
    case MGO_BAGMAN_RETURN_TIME:
    case MGO_TBAGMAN_RETURN_TIME:
        // The bagman and team bagman options share their layout (a union).
        _snprintf_safe(output_buffer, (unsigned int)buffer_size_in, "%d:%02d", mode.bagman.bag_return_time_seconds / 60, mode.bagman.bag_return_time_seconds % 60);
        return;
    case MGO_BAGMAN_KILL_BONUS:
    case MGO_TBAGMAN_KILL_BONUS:
        text = option_text_on_off(mode.bagman.bagman_kill_bonus);
        break;
    case MGO_DEMOLITION_ENGINEER_ICON:
        text = option_text_on_off(mode.demolition.engineer_icon_always_on);
        break;
    default:
        // The lock options: a locked weapon or backpack is "off".
        if (option <= MGO_LOCK_WEAPON_SHOTGUN)
        {
            text = option_text_on_off((generic.locked_weapons_mask & (1u << (option - MGO_LOCK_WEAPON_PISTOL))) == 0);
        }
        else
        {
            text = option_text_on_off((generic.locked_backpacks_mask & (1u << (option - MGO_LOCK_BACKPACK_JETPACK))) == 0);
        }
        _snprintf_safe(output_buffer, (unsigned int)buffer_size_in, "%s", text);
        return;
    }
    // Original: an option without text (a backpack without a display name) leaves the buffer empty.
    if (text != nullptr)
    {
        strncpy_safe(output_buffer, text, buffer_size_in);
    }
}
