// Original: volition/rfg/code/weapons/aim_drift.cpp
//
// The aim drift profiles of aim_drift.xtbl (player_minimal.md P3: weapons_read_table reads them first). Compiled in
// rfg_weapons_unity.cpp.
// The external interface is declared in aim_drift.h.

#include "volition/rfg/code/weapons/aim_drift.h"

#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/trace.hpp"

// ---- globals and file statics
static aim_drift_profile* Aim_drift_profiles; // 0x03882CAC
int Num_aim_drift_profiles; // 0x03882CB0

// ---- functions
// 0x00BD8CB0 (:39)
aim_drift_profile* aim_drift_profile_from_name_internal(const char* name)
{
    for (int i = 0; i < Num_aim_drift_profiles; i++)
    {
        if (v_stricmp(Aim_drift_profiles[i].name, name) == 0)
        {
            return &Aim_drift_profiles[i];
        }
    }
    return nullptr;
}

// 0x00BD8D00 (:56): fails when one of the required groups is missing.
bool parse_aim_drift_profile(aim_drift_profile* profile, const xml_element* profile_el)
{
    xml_element* acquire = xml_find(profile_el, "Acquire");
    if (acquire == nullptr)
    {
        return false;
    }
    xml_table_get_req_int(profile->acquire_time, acquire, "Time");
    xml_table_get_req_float(profile->max_acquire_penalty, acquire, "Penalty");
    xml_element* direction_change = xml_find(profile_el, "DirectionChange");
    if (direction_change == nullptr)
    {
        return false;
    }
    xml_table_get_req_int(profile->min_change_direction_time, direction_change, "MinTime");
    xml_table_get_req_int(profile->max_change_direction_time, direction_change, "MaxTime");
    xml_element* penalties = xml_find(profile_el, "Penalties");
    if (penalties == nullptr)
    {
        return false;
    }
    xml_table_get_req_float(profile->offscreen_penalty, penalties, "Offscreen");
    xml_table_get_req_float(profile->player_penalty, penalties, "Player");
    xml_table_get_req_float(profile->player_in_cover_penalty, penalties, "Player_in_cover");
    xml_table_get_req_float(profile->my_speed_penalty, penalties, "MySpeed");
    xml_table_get_req_float(profile->target_speed_penalty, penalties, "TargetSpeed");
    // The original does not check that Bonuses exists.
    xml_element* bonuses = xml_find(profile_el, "Bonuses");
    int kneeling = 0;
    int return_fire = 0;
    xml_table_get_req_int(kneeling, bonuses, "Kneeling");
    xml_table_get_req_int(return_fire, bonuses, "ReturnFire");
    profile->kneeling_bonus_pct = (float)kneeling / 100.0f;
    profile->return_fire_bonus_pct = (float)return_fire / 100.0f;
    xml_element* error = xml_find(profile_el, "Error");
    if (error == nullptr)
    {
        return false;
    }
    xml_table_get_req_float(profile->min_horizontal_error, error, "MinDegrees");
    xml_table_get_req_float(profile->max_horizontal_error, error, "MaxDegrees");
    xml_table_get_req_float(profile->pos_vertical_scale, error, "PosVertScale");
    xml_table_get_req_float(profile->neg_vertical_scale, error, "NegVertScale");
    xml_table_get_req_float(profile->max_grow_rate, error, "GrowRate");
    xml_table_get_req_float(profile->max_decay_rate, error, "DecayRate");
    xml_table_get_req_float(profile->max_vertical_error_dist, error, "MaxVertMeters");
    return true;
}

// 0x00BD8EF0 (:109)
// aim_drift.xtbl into Permanent_mempool, with room for 5 more profiles of a DLC table (dlcNN_aim_drift.xtbl), whose
// profiles are appended. A refresh re-parses the existing profiles by name.
void aim_drift_read_table(bool refresh, int dlc_id)
{
    const bool dlc = dlc_id != -1;
    xml_element* table;
    if (dlc)
    {
        char filename[256];
        _snprintf_safe(filename, sizeof(filename), "dlc%02d_aim_drift.xtbl", dlc_id);
        table = xml_table_parse(filename);
    }
    else
    {
        // Parsed once; a refresh needs the table loaded.
        if (refresh != (Aim_drift_profiles != nullptr))
        {
            return;
        }
        table = xml_table_parse("aim_drift.xtbl");
    }
    if (table == nullptr)
    {
        return;
    }
    if (!refresh && !dlc)
    {
        Num_aim_drift_profiles = (int)xml_count(table, "Profile");
        if (Num_aim_drift_profiles < 1)
        {
            // The original returns without closing the table here and below.
            keen::debug::nativePrintArgs("Cannot fine aim drift profile info!");
            return;
        }
        // Magic number: 5 extra profiles for a DLC table.
        Aim_drift_profiles = (aim_drift_profile*)Permanent_mempool.static_mempool_base::alloc((Num_aim_drift_profiles + 5) * sizeof(aim_drift_profile), 4);
        if (Aim_drift_profiles == nullptr)
        {
            keen::debug::nativePrintArgs("Cannot allocate aim drift profiles!");
            return;
        }
    }
    int num_profiles = dlc ? Num_aim_drift_profiles : 0;
    for (xml_element* entry = xml_find(table, "Profile"); entry != nullptr; entry = xml_find_next(table, entry, "Profile"))
    {
        const char* name = xml_table_get_req_string_ref(entry, "Name");
        aim_drift_profile* profile;
        if (refresh)
        {
            profile = aim_drift_profile_from_name_internal(name);
            if (profile == nullptr)
            {
                continue;
            }
        }
        else
        {
            profile = &Aim_drift_profiles[num_profiles];
            strncpy_safe(profile->name, name, sizeof(profile->name));
        }
        if (parse_aim_drift_profile(profile, entry))
        {
            num_profiles++;
        }
    }
    if (!refresh)
    {
        Num_aim_drift_profiles = num_profiles;
    }
    xml_table_close(table);
}

// 0x00BD90C0 (:180)
const aim_drift_profile* aim_drift_profile_from_name(const char* name)
{
    return aim_drift_profile_from_name_internal(name);
}
