// Original: volition/rfg/code/weapons/weapon_firing_patterns.cpp
//
// NPC firing patterns of weapon_firing_patterns.xtbl (player_minimal.md P3: weapons_system_init reads them before the
// weapons table). Compiled in rfg_weapons_unity.cpp.
// The external interface is declared in weapon_firing_patterns.h.

#include "volition/rfg/code/weapons/weapon_firing_patterns.h"

#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"

#include "keen/base/trace.hpp"

#include <math.h>
#include <new>
#include <stdio.h>
#include <string.h>

// ---- globals and file statics
// weapon_firing_pattern* Weapon_firing_patterns (0x03882CD0) is used here too; placed in weapons.cpp.
int Num_weapon_firing_patterns; // 0x03882CCC

// ---- internal functions
// 0x00BDA3B0 (:41)
static weapon_firing_pattern* weapon_firing_pattern_lookup_internal(const char* name, int num)
{
    for (int i = 0; i < num; i++)
    {
        if (v_stricmp(name, Weapon_firing_patterns[i].name) == 0)
        {
            return &Weapon_firing_patterns[i];
        }
    }
    return nullptr;
}

// 0x00BDA400 (:58)
// Up to 8 control points, each complete one clamped (range >= 0, min burst >= 1, max burst >= min burst, min interval
// >= 0, max interval >= min interval) and kept sorted by range (insertion step of the original's inlined sort).
static bool parse_control_points(weapon_firing_pattern* pattern, xml_element* control_points)
{
    pattern->num_control_points = 0;
    for (xml_element* entry = xml_find(control_points, "Control_Point"); entry != nullptr && pattern->num_control_points < 8;
         entry = xml_find_next(control_points, entry, "Control_Point"))
    {
        weapon_firing_pattern::control_point& point = pattern->points[pattern->num_control_points];
        const bool has_range = xml_table_get_opt_float(point.range_pct, entry, "Range_Percent");
        const bool has_min_burst = xml_table_get_opt_float(point.min_burst_size, entry, "Min_Burst_Size");
        const bool has_max_burst = xml_table_get_opt_float(point.max_burst_size, entry, "Max_Burst_Size");
        const bool has_min_interval = xml_table_get_opt_float(point.min_interval, entry, "Min_Burst_Interval");
        const bool has_max_interval = xml_table_get_opt_float(point.max_interval, entry, "Max_Burst_Interval");
        if (!(has_range && has_min_burst && has_max_burst && has_min_interval && has_max_interval))
        {
            continue;
        }
        if (0.0f - point.range_pct >= 0.0f)
        {
            point.range_pct = 0.0f;
        }
        if (1.0f - point.min_burst_size >= 0.0f)
        {
            point.min_burst_size = 1.0f;
        }
        if (point.min_burst_size - point.max_burst_size >= 0.0f)
        {
            point.max_burst_size = point.min_burst_size;
        }
        if (0.0f - point.min_interval >= 0.0f)
        {
            point.min_interval = 0.0f;
        }
        if (point.min_interval - point.max_interval >= 0.0f)
        {
            point.max_interval = point.min_interval;
        }
        pattern->num_control_points++;
        for (int i = pattern->num_control_points - 1; i > 0 && pattern->points[i - 1].range_pct > pattern->points[i].range_pct; i--)
        {
            const weapon_firing_pattern::control_point swap = pattern->points[i];
            pattern->points[i] = pattern->points[i - 1];
            pattern->points[i - 1] = swap;
        }
    }
    return pattern->num_control_points > 0;
}

// 0x00BDA780 (:142)
// Up to 3 ammo points (Control_Point entries), clamped to [0, 1] and kept sorted by descending ammo percentage.
static bool parse_ammo_points(weapon_firing_pattern* pattern, xml_element* ammo_points)
{
    pattern->num_ammo_points = 0;
    for (xml_element* entry = xml_find(ammo_points, "Control_Point"); entry != nullptr && pattern->num_ammo_points < 3;
         entry = xml_find_next(ammo_points, entry, "Control_Point"))
    {
        weapon_firing_pattern::ammo_point& point = pattern->ammo_points[pattern->num_ammo_points];
        const bool has_ammo = xml_table_get_opt_float(point.ammo_pct, entry, "Ammo_Percent");
        const bool has_range = xml_table_get_opt_float(point.min_effective_range, entry, "Min_Effective_Range");
        if (!(has_ammo && has_range))
        {
            continue;
        }
        point.ammo_pct = point.ammo_pct - 0.0f < 0.0f ? 0.0f : point.ammo_pct;
        if (point.ammo_pct - 1.0f >= 0.0f)
        {
            point.ammo_pct = 1.0f;
        }
        point.min_effective_range = point.min_effective_range - 0.0f < 0.0f ? 0.0f : point.min_effective_range;
        if (point.min_effective_range - 1.0f >= 0.0f)
        {
            point.min_effective_range = 1.0f;
        }
        pattern->num_ammo_points++;
        for (int i = pattern->num_ammo_points - 1; i > 0 && pattern->ammo_points[i].ammo_pct > pattern->ammo_points[i - 1].ammo_pct; i--)
        {
            const weapon_firing_pattern::ammo_point swap = pattern->ammo_points[i];
            pattern->ammo_points[i] = pattern->ammo_points[i - 1];
            pattern->ammo_points[i - 1] = swap;
        }
    }
    return true;
}

// 0x00BDAAD0 (:234, LTCG: pattern in EAX, entry in ESI; refresh is not passed)
static bool parse_weapon_firing_pattern(weapon_firing_pattern* pattern, xml_element* entry, bool refresh)
{
    xml_element* control_points = xml_find(entry, "Control_Points");
    if (control_points == nullptr || !parse_control_points(pattern, control_points))
    {
        return false;
    }
    xml_element* ammo_points = xml_find(entry, "Ammo_Minimums");
    if (ammo_points == nullptr || !parse_ammo_points(pattern, ammo_points))
    {
        return false;
    }
    parse_modifier(pattern->alert_mod, entry, "Alert_Modifier");
    parse_modifier(pattern->flyer_mod, entry, "Flyer_Modifier");
    parse_modifier(pattern->heavy_vehicle_mod, entry, "Heavy_Vehicle_Modifier");
    parse_modifier(pattern->normal_vehicle_mod, entry, "Normal_Vehicle_Modifier");
    parse_modifier(pattern->covering_fire_mod, entry, "Covering_Fire_Modifier");
    return true;
}

// 0x00BDFBE0 (:265)
// weapon_firing_patterns.xtbl into Permanent_mempool with room for 5 more patterns of a DLC table, whose patterns are
// appended. A refresh re-parses the existing patterns by name.
static void weapon_firing_patterns_table_parse(bool refresh, int dlc_id)
{
    const bool dlc = dlc_id != -1;
    xml_element* table;
    if (dlc)
    {
        char filename[256];
        sprintf(filename, "dlc%02d_weapon_firing_patterns.xtbl", dlc_id);
        table = xml_table_parse(filename);
    }
    else
    {
        table = xml_table_parse("weapon_firing_patterns.xtbl");
    }
    if (table == nullptr)
    {
        keen::debug::nativePrintArgs("Unable to load weapon firing patterns table!");
        return;
    }
    if (!refresh && !dlc)
    {
        Num_weapon_firing_patterns = xml_table_count(table, "Firing_Pattern");
        // Magic number: 5 extra patterns for a DLC table.
        Weapon_firing_patterns = (weapon_firing_pattern*)Permanent_mempool.static_mempool_base::alloc((Num_weapon_firing_patterns + 5) * sizeof(weapon_firing_pattern), 0);
        if (Weapon_firing_patterns == nullptr)
        {
            keen::debug::nativePrintArgs("Unable to allocate memory for weapon firing patterns data.");
            xml_table_close(table);
            return;
        }
        for (int i = 0; i < Num_weapon_firing_patterns + 5; i++)
        {
            new (&Weapon_firing_patterns[i]) weapon_firing_pattern();
        }
    }
    int num_patterns = dlc ? Num_weapon_firing_patterns : 0;
    for (xml_element* entry = xml_find(table, "Firing_Pattern"); entry != nullptr; entry = xml_find_next(table, entry, "Firing_Pattern"))
    {
        const char* name = xml_table_get_opt_string_ref(entry, "Name");
        if (name == nullptr)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            continue;
        }
        weapon_firing_pattern* pattern;
        if (refresh)
        {
            pattern = weapon_firing_pattern_lookup_internal(name, Num_weapon_firing_patterns);
            if (pattern == nullptr)
            {
                continue;
            }
        }
        else
        {
            if (num_patterns >= Num_weapon_firing_patterns + 5)
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                break;
            }
            pattern = &Weapon_firing_patterns[num_patterns];
            const unsigned int size = (unsigned int)strlen(name) + 1;
            pattern->name = (char*)Permanent_mempool.static_mempool_base::alloc(size, 1);
            memcpy(pattern->name, name, size);
        }
        if (parse_weapon_firing_pattern(pattern, entry, refresh))
        {
            num_patterns++;
        }
    }
    if (!refresh && num_patterns != Num_weapon_firing_patterns)
    {
        Num_weapon_firing_patterns = num_patterns;
    }
    xml_table_close(table);
}

// ---- functions
// 0x00BDA9F0 (:207): adjustment clamped to [-2, 2], multiplier to [0.1, 5]; defaults 0 and 1 without the element.
bool parse_modifier(weapon_firing_pattern::modifier& mod, xml_element* entry, const char* el_name)
{
    mod.adjustment = 0.0f;
    mod.multiplier = 1.0f;
    xml_element* element = xml_find(entry, el_name);
    if (element == nullptr)
    {
        return false;
    }
    xml_table_get_opt_float(mod.adjustment, element, "Adjustment");
    xml_table_get_opt_float(mod.multiplier, element, "Multiplier");
    // Magic numbers: the original's clamp ranges.
    float adjustment = mod.adjustment;
    if (adjustment - -2.0f < 0.0f)
    {
        adjustment = -2.0f;
    }
    if (adjustment - 2.0f >= 0.0f)
    {
        adjustment = 2.0f;
    }
    mod.adjustment = adjustment;
    float multiplier = mod.multiplier;
    if (multiplier - 0.1f < 0.0f)
    {
        multiplier = 0.1f;
    }
    if (multiplier - 5.0f >= 0.0f)
    {
        multiplier = 5.0f;
    }
    mod.multiplier = multiplier;
    return true;
}

// 0x00BDFE60 (:375)
int round_rand_range(float minimum, float maximum)
{
    const int value = fl_round(fl_rand_range(minimum, maximum));
    return value < 1 ? 1 : value;
}

// 0x00BDAB80 (:380)
int sec_to_ms_range(float minimum, float maximum)
{
    return fl_round(fl_rand_range(minimum, maximum) * 1000.0f);
}

// 0x00BD9580 (.h:43)
weapon_firing_pattern::modifier::modifier()
{
    multiplier = 1.0f;
    adjustment = 0.0f;
}

// 0x00BDABC0 (:392)
weapon_firing_pattern::weapon_firing_pattern()
{
    alert_mod.multiplier = 1.0f;
    alert_mod.adjustment = 0.0f;
    flyer_mod.multiplier = 1.0f;
    flyer_mod.adjustment = 0.0f;
    heavy_vehicle_mod.multiplier = 1.0f;
    heavy_vehicle_mod.adjustment = 0.0f;
    normal_vehicle_mod.multiplier = 1.0f;
    normal_vehicle_mod.adjustment = 0.0f;
    covering_fire_mod.multiplier = 1.0f;
    covering_fire_mod.adjustment = 0.0f;
    name = nullptr;
    num_control_points = 0;
    num_ammo_points = 0;
}

// 0x00BDAC30 (:413): interpolated over the ammo points (sorted descending). Original: the interpolation always starts
// at the first point's percentage, not at the lower neighbour's (kept).
float weapon_firing_pattern::get_min_effective_range(float ammo_pct) const
{
    if (num_ammo_points <= 0 || ammo_pct >= ammo_points[0].ammo_pct)
    {
        return 0.0f;
    }
    for (int i = 1; i < num_ammo_points; i++)
    {
        if (ammo_points[i].ammo_pct <= ammo_pct)
        {
            return fl_parameter(ammo_pct, ammo_points[0].ammo_pct, ammo_points[i].ammo_pct, ammo_points[i - 1].min_effective_range, ammo_points[i].min_effective_range, true);
        }
    }
    return ammo_points[num_ammo_points - 1].min_effective_range;
}

// 0x00BDFEA0 (:443): a random burst size between the interpolated minimum and maximum, scaled by the difficulty.
int weapon_firing_pattern::get_burst_size(float range_pct) const
{
    float min_burst;
    float max_burst;
    if (range_pct <= points[0].range_pct)
    {
        min_burst = difficulty_get_value(DIFFICULTY_ENEMY_BURST_SIZE, NUM_DIFFICULTY_LEVELS) * points[0].min_burst_size;
        max_burst = difficulty_get_value(DIFFICULTY_ENEMY_BURST_SIZE, NUM_DIFFICULTY_LEVELS) * points[0].max_burst_size;
    }
    else
    {
        int i = 1;
        for (; i < num_control_points; i++)
        {
            if (range_pct <= points[i].range_pct)
            {
                break;
            }
        }
        if (i < num_control_points)
        {
            const float t = fl_parameter(range_pct, points[i - 1].range_pct, points[i].range_pct, true);
            const float min_low = points[i - 1].min_burst_size;
            const float max_low = points[i - 1].max_burst_size;
            min_burst = difficulty_get_value(DIFFICULTY_ENEMY_BURST_SIZE, NUM_DIFFICULTY_LEVELS) * ((points[i].min_burst_size - min_low) * t + min_low);
            max_burst = difficulty_get_value(DIFFICULTY_ENEMY_BURST_SIZE, NUM_DIFFICULTY_LEVELS) * ((points[i].max_burst_size - max_low) * t + max_low);
        }
        else
        {
            min_burst = difficulty_get_value(DIFFICULTY_ENEMY_BURST_SIZE, NUM_DIFFICULTY_LEVELS) * points[num_control_points - 1].min_burst_size;
            max_burst = difficulty_get_value(DIFFICULTY_ENEMY_BURST_SIZE, NUM_DIFFICULTY_LEVELS) * points[num_control_points - 1].max_burst_size;
        }
    }
    if (1.0f - min_burst >= 0.0f)
    {
        min_burst = 1.0f;
    }
    if (1.0f - max_burst >= 0.0f)
    {
        max_burst = 1.0f;
    }
    const int burst = fl_round(fl_rand_range(min_burst, max_burst));
    return burst < 1 ? 1 : burst;
}

// 0x00BDACF0 (:485)
int weapon_firing_pattern::get_max_burst_size(float range_pct) const
{
    if (range_pct <= points[0].range_pct)
    {
        return (int)ceil((double)points[0].max_burst_size);
    }
    for (int i = 1; i < num_control_points; i++)
    {
        if (range_pct <= points[i].range_pct)
        {
            const float t = fl_parameter(range_pct, points[i - 1].range_pct, points[i].range_pct, true);
            const float low = points[i - 1].max_burst_size;
            return (int)ceil((double)((points[i].max_burst_size - low) * t + low));
        }
    }
    return (int)ceil((double)points[num_control_points - 1].max_burst_size);
}

// 0x00BDAE20 (:514): milliseconds.
int weapon_firing_pattern::get_firing_interval(float range_pct) const
{
    if (range_pct <= points[0].range_pct)
    {
        return fl_round(fl_rand_range(points[0].min_interval, points[0].max_interval) * 1000.0f);
    }
    for (int i = 1; i < num_control_points; i++)
    {
        if (range_pct <= points[i].range_pct)
        {
            const float t = fl_parameter(range_pct, points[i - 1].range_pct, points[i].range_pct, true);
            const float min_low = points[i - 1].min_interval;
            const float max_low = points[i - 1].max_interval;
            return fl_round(fl_rand_range((points[i].min_interval - min_low) * t + min_low, (points[i].max_interval - max_low) * t + max_low) * 1000.0f);
        }
    }
    return fl_round(fl_rand_range(points[num_control_points - 1].min_interval, points[num_control_points - 1].max_interval) * 1000.0f);
}

// 0x00BDAFB0 (:540)
int weapon_firing_pattern::get_max_firing_interval() const
{
    return fl_round(points[num_control_points - 1].max_interval * 1000.0f);
}

// 0x00BDAFE0 (:562)
const weapon_firing_pattern* weapon_firing_pattern_lookup(const char* name)
{
    return weapon_firing_pattern_lookup_internal(name, Num_weapon_firing_patterns);
}

// 0x00BDB000 (:573)
const weapon_firing_pattern* weapon_firing_pattern_from_handle(int handle)
{
    if (handle > -1 && handle < Num_weapon_firing_patterns)
    {
        return &Weapon_firing_patterns[handle];
    }
    return nullptr;
}

// 0x00BDB020 (:585)
int weapon_firing_pattern_get_handle(const weapon_firing_pattern* pattern)
{
    const int handle = (int)(pattern - Weapon_firing_patterns);
    if (handle < 0 || handle >= Num_weapon_firing_patterns)
    {
        return -1;
    }
    return handle;
}

// 0x00BE0130 (:593)
void weapon_firing_patterns_table_init(int dlc_id)
{
    if (Weapon_firing_patterns == nullptr || dlc_id != -1)
    {
        weapon_firing_patterns_table_parse(false, dlc_id);
    }
}
