// Original: volition/rfg/code/multi/multi_game/multi_game_overrides.cpp
//
// Multiplayer balance overrides: mp_backpack_overrides.xtbl, mp_explosion_overrides.xtbl and mp_weapon_overrides.xtbl
// (through the settings files) name backpacks, explosions and weapons and give new values for some of their fields.
// They are applied to the shared infos while multiplayer runs and restored afterwards.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_overrides.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"

#include <string.h>

// ---- globals
backpack_override Backpack_overrides; // 0x02FE80F4
explosion_override Explosion_overrides; // 0x02FE6C68
weapon_override Weapon_overrides; // 0x02FE9078

// 0x0095F260 (:67)
void backpack_override_info::init()
{
    memset(this, 0, sizeof(*this));
}

// 0x0095F270 (:80)
void explosion_override_info::init()
{
    memset(this, 0, sizeof(*this));
}

// 0x0095F280 (:104)
void weapon_override_info::init()
{
    memset(this, 0, sizeof(*this));
}

// ---- internal functions

// 0x009815B0 (:162). LTCG: node in ECX, tag in EAX, override_float_val in ESI.
static void get_optional_override_float(xml_element* node, const char* tag, override_val<float>& override_float_val)
{
    float value;
    if (xml_table_get_opt_float(value, node, tag))
    {
        override_float_val.set_overrride_val(value);
    }
}

// 0x009815E0 (:180). LTCG: node in ECX, tag in EAX, override_float_val in ESI.
static void get_optional_override_uint(xml_element* node, const char* tag, override_val<unsigned int>& override_float_val)
{
    unsigned int value;
    if (xml_table_get_opt_uint(value, node, tag))
    {
        override_float_val.set_overrride_val(value);
    }
}

// 0x00981600 (:189). LTCG: node in ECX, tag in EAX, override_float_val in ESI.
static void get_optional_override_ushort(xml_element* node, const char* tag, override_val<unsigned short>& override_float_val)
{
    unsigned int value;
    if (xml_table_get_opt_uint(value, node, tag))
    {
        override_float_val.set_overrride_val((unsigned short)value);
    }
}

// 0x00981630 (:199). LTCG: node in ECX, tag in EAX, override_float_val in ESI.
static void get_optional_override_bool(xml_element* node, const char* tag, override_val<bool>& override_float_val)
{
    bool value;
    if (xml_table_get_opt_bool(value, node, tag))
    {
        override_float_val.set_overrride_val(value);
    }
}

// 0x00981650 (:208). LTCG: pool in EAX. Original bug kept: an entry naming an unknown backpack is never left (the
// next entry is only looked up after a known one), so the loop does not end; the shipped table names known ones.
static void parse_backpack_overrides(mempool_base& pool)
{
    xml_element* root = multi_game_settings_retrieve_settings_file("mp_backpack_overrides.xtbl");
    if (root == nullptr)
    {
        return;
    }
    Backpack_overrides.backpack_infos = (backpack_override_info*)pool.alloc(xml_count(root, "mp_backpacks") * sizeof(backpack_override_info), 0);
    Backpack_overrides.num_infos = 0;
    xml_element* entry = xml_table_find_opt(root, "mp_backpacks");
    while (entry != nullptr)
    {
        const multi_backpack_info* binfo = multi_game_backpack_get_info(xml_table_get_req_string_ref(entry, "Name"));
        if (binfo == nullptr)
        {
            continue;
        }
        backpack_override_info& info = Backpack_overrides.backpack_infos[Backpack_overrides.num_infos++];
        info.init();
        info.binfo = binfo;
        xml_element* fuel = xml_table_find_opt(entry, "uses_fuel");
        if (fuel != nullptr)
        {
            get_optional_override_float(fuel, "fuel_use_per_second", info.m_fuel_use_per_second);
            get_optional_override_float(fuel, "fuel_regen_per_second", info.m_fuel_regen_per_second);
            get_optional_override_float(fuel, "fuel_activation_cost", info.m_fuel_activation_cost);
            get_optional_override_float(fuel, "minimum_fuel_to_activate", info.m_minimum_fuel_to_activate);
        }
        get_optional_override_bool(entry, "player_deactivatable", info.m_player_deactivatable);
        entry = xml_table_find_next(root, entry, "mp_backpacks");
    }
    xml_table_close(root);
}

// 0x009817B0 (:247)
static void apply_backpack_overrides()
{
    for (int i = 0; i < Backpack_overrides.num_infos; i++)
    {
        backpack_override_info& info = Backpack_overrides.backpack_infos[i];
        multi_backpack_info* binfo = (multi_backpack_info*)info.binfo;
        info.m_fuel_activation_cost.apply_override(binfo->fuel_activation_cost);
        info.m_fuel_regen_per_second.apply_override(binfo->fuel_regen_per_second);
        info.m_fuel_use_per_second.apply_override(binfo->fuel_use_per_second);
        info.m_minimum_fuel_to_activate.apply_override(binfo->minimum_fuel_to_activate);
        info.m_player_deactivatable.apply_override(binfo->player_deactivatable);
    }
}

// 0x00981870 (:262)
static void restore_backpack_overrides()
{
    for (int i = 0; i < Backpack_overrides.num_infos; i++)
    {
        backpack_override_info& info = Backpack_overrides.backpack_infos[i];
        multi_backpack_info* binfo = (multi_backpack_info*)info.binfo;
        info.m_fuel_activation_cost.recover_override(binfo->fuel_activation_cost);
        info.m_fuel_regen_per_second.recover_override(binfo->fuel_regen_per_second);
        info.m_fuel_use_per_second.recover_override(binfo->fuel_use_per_second);
        info.m_minimum_fuel_to_activate.recover_override(binfo->minimum_fuel_to_activate);
        info.m_player_deactivatable.recover_override(binfo->player_deactivatable);
    }
}

// 0x009818F0 (:277). LTCG: pool in EAX. The unknown-name loop of parse_backpack_overrides is kept here too.
static void parse_explosion_overrides(mempool_base& pool)
{
    xml_element* root = multi_game_settings_retrieve_settings_file("mp_explosion_overrides.xtbl");
    if (root == nullptr)
    {
        return;
    }
    Explosion_overrides.explosion_infos = (explosion_override_info*)pool.alloc(xml_count(root, "mp_explosions") * sizeof(explosion_override_info), 0);
    Explosion_overrides.num_infos = 0;
    xml_element* entry = xml_table_find_opt(root, "mp_explosions");
    while (entry != nullptr)
    {
        const explosion_info* einfo = explosion_lookup(xml_table_get_req_string_ref(entry, "Name"));
        if (einfo == nullptr)
        {
            continue;
        }
        explosion_override_info& info = Explosion_overrides.explosion_infos[Explosion_overrides.num_infos++];
        info.init();
        info.einfo = einfo;
        get_optional_override_float(entry, "KnockdownRadius", info.m_knockdown_radius);
        get_optional_override_uint(entry, "Human_Damage_Max", info.m_human_max_damage_hitpoints);
        get_optional_override_uint(entry, "human_damage_min", info.m_human_min_damage_hitpoints);
        get_optional_override_float(entry, "Impulse", info.m_impulse_magnitude);
        entry = xml_table_find_next(root, entry, "mp_explosions");
    }
    xml_table_close(root);
}

// 0x009819F0 (:310)
static void apply_explosion_overrides()
{
    for (int i = 0; i < Explosion_overrides.num_infos; i++)
    {
        explosion_override_info& info = Explosion_overrides.explosion_infos[i];
        explosion_info* einfo = (explosion_info*)info.einfo;
        info.m_knockdown_radius.apply_override(einfo->m_knockdown_radius);
        info.m_human_max_damage_hitpoints.apply_override(einfo->m_human_max_damage_hitpoints);
        info.m_human_min_damage_hitpoints.apply_override(einfo->m_human_min_damage_hitpoints);
        info.m_impulse_magnitude.apply_override(einfo->m_impulse_magnitude);
    }
}

// 0x00981A80 (:323)
static void restore_explosion_overrides()
{
    for (int i = 0; i < Explosion_overrides.num_infos; i++)
    {
        explosion_override_info& info = Explosion_overrides.explosion_infos[i];
        explosion_info* einfo = (explosion_info*)info.einfo;
        info.m_knockdown_radius.recover_override(einfo->m_knockdown_radius);
        info.m_human_max_damage_hitpoints.recover_override(einfo->m_human_max_damage_hitpoints);
        info.m_human_min_damage_hitpoints.recover_override(einfo->m_human_min_damage_hitpoints);
        info.m_impulse_magnitude.recover_override(einfo->m_impulse_magnitude);
    }
}

// 0x00981AF0 (:336). LTCG: pool in EAX. The unknown-name loop of parse_backpack_overrides is kept here too. The
// player damage overrides are the player entries (index 1) of the near and far damage scaling.
static void parse_weapon_overrides(mempool_base& pool)
{
    xml_element* root = multi_game_settings_retrieve_settings_file("mp_weapon_overrides.xtbl");
    if (root == nullptr)
    {
        return;
    }
    Weapon_overrides.weapon_infos = (weapon_override_info*)pool.alloc(xml_count(root, "mp_weapons") * sizeof(weapon_override_info), 0);
    Weapon_overrides.num_infos = 0;
    xml_element* entry = xml_table_find_opt(root, "mp_weapons");
    while (entry != nullptr)
    {
        const weapon_info* winfo = weapons_get_weapon_info(xml_table_get_req_string_ref(entry, "Name"));
        if (winfo == nullptr)
        {
            continue;
        }
        weapon_override_info& info = Weapon_overrides.weapon_infos[Weapon_overrides.num_infos++];
        info.init();
        info.winfo = winfo;
        xml_element* ammo = xml_table_find_opt(entry, "Ammo_count");
        if (ammo != nullptr)
        {
            get_optional_override_ushort(ammo, "Magazine_Size", info.m_magazine_size);
            get_optional_override_ushort(ammo, "Num_Magazines", info.m_magazine_start_num);
            get_optional_override_ushort(ammo, "Max_Rounds", info.m_max_rounds);
        }
        get_optional_override_float(entry, "Spread_Min", info.m_min_spread);
        get_optional_override_float(entry, "Spread_Max", info.m_max_spread);
        get_optional_override_float(entry, "Spread_Fine_Aim_Min", info.m_fine_aim_min_spread);
        get_optional_override_float(entry, "Spread_Fine_Aim_Max", info.m_fine_aim_max_spread);
        get_optional_override_float(entry, "Range_Red", info.m_red_range);
        get_optional_override_float(entry, "Headshot_Multiplier", info.m_headshot_multiplier);
        get_optional_override_float(entry, "Threshold_Min_Range", info.m_min_range_threshold);
        get_optional_override_float(entry, "Player_Damage_Min_Range", info.m_min_range_player_damage);
        get_optional_override_float(entry, "Threshold_Max_Range", info.m_max_range_threshold);
        get_optional_override_float(entry, "Player_Damage_Max_Range", info.m_max_range_player_damage);
        entry = xml_table_find_next(root, entry, "mp_weapons");
    }
    xml_table_close(root);
}

// 0x00981CB0 (:388)
static void apply_weapon_overrides()
{
    for (int i = 0; i < Weapon_overrides.num_infos; i++)
    {
        weapon_override_info& info = Weapon_overrides.weapon_infos[i];
        weapon_info* winfo = (weapon_info*)info.winfo;
        info.m_magazine_size.apply_override(winfo->magazine_size);
        info.m_magazine_start_num.apply_override(winfo->magazine_start_num);
        info.m_max_rounds.apply_override(winfo->max_rounds);
        info.m_min_spread.apply_override(winfo->min_spread);
        info.m_max_spread.apply_override(winfo->max_spread);
        info.m_fine_aim_min_spread.apply_override(winfo->fine_aim_min_spread);
        info.m_fine_aim_max_spread.apply_override(winfo->fine_aim_max_spread);
        info.m_red_range.apply_override(winfo->red_range);
        info.m_headshot_multiplier.apply_override(winfo->headshot_multiplier);
        info.m_min_range_threshold.apply_override(winfo->low_scale_damage.scale_value);
        info.m_min_range_player_damage.apply_override(winfo->low_scale_damage.damage[1]);
        info.m_max_range_threshold.apply_override(winfo->high_scale_damage.scale_value);
        info.m_max_range_player_damage.apply_override(winfo->high_scale_damage.damage[1]);
    }
}

// 0x00981E70 (:420)
static void restore_weapon_overrides()
{
    for (int i = 0; i < Weapon_overrides.num_infos; i++)
    {
        weapon_override_info& info = Weapon_overrides.weapon_infos[i];
        weapon_info* winfo = (weapon_info*)info.winfo;
        info.m_magazine_size.recover_override(winfo->magazine_size);
        info.m_magazine_start_num.recover_override(winfo->magazine_start_num);
        info.m_max_rounds.recover_override(winfo->max_rounds);
        info.m_min_spread.recover_override(winfo->min_spread);
        info.m_max_spread.recover_override(winfo->max_spread);
        info.m_fine_aim_min_spread.recover_override(winfo->fine_aim_min_spread);
        info.m_fine_aim_max_spread.recover_override(winfo->fine_aim_max_spread);
        info.m_red_range.recover_override(winfo->red_range);
        info.m_headshot_multiplier.recover_override(winfo->headshot_multiplier);
        info.m_min_range_threshold.recover_override(winfo->low_scale_damage.scale_value);
        info.m_min_range_player_damage.recover_override(winfo->low_scale_damage.damage[1]);
        info.m_max_range_threshold.recover_override(winfo->high_scale_damage.scale_value);
        info.m_max_range_player_damage.recover_override(winfo->high_scale_damage.damage[1]);
    }
}

// 0x00981FA0 (:451)
void multi_game_mp_overrides_init(mempool_base& pool)
{
    parse_backpack_overrides(pool);
    parse_explosion_overrides(pool);
    parse_weapon_overrides(pool);
}

// 0x00981FC0 (:458)
void multi_game_overrides_apply_overrides()
{
    apply_backpack_overrides();
    apply_explosion_overrides();
    apply_weapon_overrides();
}

// 0x00981FD0 (:465)
void multi_game_overrides_restore_overrides()
{
    restore_backpack_overrides();
    restore_explosion_overrides();
    restore_weapon_overrides();
}
