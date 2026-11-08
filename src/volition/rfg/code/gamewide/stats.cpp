// Original: volition/rfg/code/gamewide/stats.cpp
//
// Tracked statistics, achievements, hidden challenges and cheats. parse_stats_data reads stats.xtbl (the stats, by
// the names of Stat_table_entries), achievements.xtbl (achievements and their requirements on stats) and
// mp_hidden_challenges.xtbl (hidden challenges over sets of stats), or a DLC bundle's dlcNN_ copies; each stat keeps
// the achievements and hidden challenges that read it, so an update re-checks only those. Stat_type_functions holds
// the per value type update, requirement check, string, reset and (de)serialization functions. Stats not stored in
// the save game are the profile stats: serialized into the player profile data as length-prefixed values. Cheats
// (Cheats_data) and a save game the player does not own (Savegame_owned false) or modified tables (Modified_table_data)
// freeze the stats and achievements. Achievements are awarded through the platform layer; their progress goes to it
// as the sum of the requirement values. The achievements update thread only marks them initialized.
// Part of the gamewide unity TU (rfg_gamewide_unity.cpp): downloadable_content.cpp calls stats_reset_specific.
//
// Notes (decompiled behaviour, kept as is):
// - get_weapon_info_from_stat tests the weapon kill range (sledgehammer to gutter) before the spiker, so the spiker's
//   kill stat (outside the range) has no weapon: it can be the best or worst weapon's stat but names none.
// - stats_level_init, stats_set_stat and stats_set_stat_fl check Hidden_challenges[0..n) for a stat with n hidden
//   challenge references, not its references (stats_increment checks the references).
// - parse_stats_data stops reading stats.xtbl at the first stat name Stat_table_entries does not know; a requirement
//   whose stat is unknown is skipped; a stat already in 8 achievements ends that achievement's requirements; a hidden
//   challenge's requirements are read without checking that the Requirements element exists, and a requirement whose
//   stat is unknown keeps the previous parse's slot; "Store_in_Save_Game" is compared with "True", the others with
//   "true".
// - stats_serialize_in_save_game_stats reads the bool stats but never sets them, and calls the (null) set function of
//   the percent complete, missions completed and Wrecking Crew stats if they are stored in the save game.
// - stats_serialize_in_profile_stats updates a stat without a set function with a null delta.
// - stats_get_achievement_progress reads Achievements[-1] for an unknown id (its null check never fails).
// - stats_get_req_string_value prints a bool requirement as a critical failure.

#include "volition/rfg/code/gamewide/stats.h"

#include "game/build/win32/steam/master/achievements/rfgachievements_adf.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "unit4/gamesession/localgamesession.hpp"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/cutscene/cutscene.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/activities/activity_register_damage.h"
#include "volition/rfg/code/gameplay/activities/activity_vrail.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/option_menus/option_menu_system.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/objects/gameplay_properties.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_area_defense_node.h"
#include "volition/rfg/code/objects/object_convoy_end_point.h"
#include "volition/rfg/code/objects/object_courier_end_point.h"
#include "volition/rfg/code/objects/object_delivery_node.h"
#include "volition/rfg/code/objects/object_demolitions_master_node.h"
#include "volition/rfg/code/objects/object_house_arrest_node.h"
#include "volition/rfg/code/objects/object_raid_node.h"
#include "volition/rfg/code/objects/object_riding_shotgun_node.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/os.h"
#include "volition/vlib/code/os/thread.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// size 0x8: a stat's name in stats.xtbl
struct stat_table_entry
{
    const char* table_name; // 0x00
    tracked_stats_indices stat_index; // 0x04
};

// ---- globals
// The stat names (one per stat, data from the image).
stat_table_entry Stat_table_entries[639] = { // 0x0165CCB0
#include "volition/rfg/code/gamewide/stats_table_entries.inl"
};

// ---- file-static globals
static const char STATS_STRING_POOL_NAME[14] = "Stats strings"; // 0x012EC0D8
// Magic number: 2e9 s, the "no time yet" value of the best time stats (stats_update_best_fl replaces it).
static const float INITIAL_TIME_VALUE = 2000000000.0f; // 0x012EC0E8
static bool Savegame_owned = true; // 0x0165CCAA, initial data 01
// Also read by gamestate_gameplay.cpp (the unity TU).
static bool Achievements_enabled = true; // 0x0165CCAB, initial data 01
static unsigned int update_achievements_thread_handle = 0xFFFF; // 0x0165E0A8, 0xFFFF: no thread
static achievement Achievements[64]; // 0x0227AFD0
static bool Cheats_data[NUM_CHEAT_INDEXES]; // 0x0250550C
static tracked_stat Stats[STATS_NUM_STATS]; // 0x02512568
static hidden_challenge Hidden_challenges[64]; // 0x0252D6F0
static bool Achievements_initialized; // 0x02533929
static bool Stats_system_initialized; // 0x0253392A
static bool Hidden_challenges_init_complete; // 0x0253392B
static char* Stats_string_pool_data; // 0x02533930
static int Num_stats; // 0x02533934
static int Num_achievements; // 0x02533938
static int Num_hidden_challenges; // 0x0253393C
static bool Cheats_enabled; // 0x02533940
static bool Stats_should_open_reset_message; // 0x02533943
static bool Update_achievements_thread_abort; // 0x02533944
static string_pool Stats_string_pool(STATS_STRING_POOL_NAME); // 0x02535658
static stat_function_pointers Stat_type_functions[NUM_TRACKED_STAT_TYPES]; // 0x02535700
static cs_wrapper Update_achievements_thread_cs(0); // 0x02535E40
static cs_wrapper Stats_serialize_cs(0); // 0x0253C8A4
// Placement inferred: not in rfg.pdb (set once by stats_system_init, read nowhere).
static bool profile_boot_inited;

// Magic number: the size of a stats string buffer (the thread-local scratch string of rfg.exe).
static const int STATS_STRING_SIZE = 0x400;

static void stats_reset_specific(bool reset_save_game_stats, bool reset_profile_stats); // 0x007E5CC0 (:4528)

// A stat by index, null past the table (stats_get_stat, inlined in most callers).
static tracked_stat* stats_get_stat_ptr(tracked_stats_indices index)
{
    if ((unsigned int)index < STATS_NUM_STATS)
    {
        return &Stats[index];
    }
    return nullptr;
}

// Stats, achievements and hidden challenges are frozen with cheats on, in a save the player does not own or with
// modified tables (inlined everywhere).
static bool stats_can_change()
{
    return !Cheats_enabled && Savegame_owned && !Modified_table_data;
}

// 0x007C6D40 (:124)
stat_function_pointers::stat_function_pointers(void (*usvp)(tracked_stat*, const void*), bool (*csrp)(const tracked_stat*, const achievement_requirement*), void (*gssvp)(char*, int, const tracked_stat*), void (*rsvp)(tracked_stat*), void (*gsdp)(void*&, int&, const tracked_stat*), bool (*ssdp)(tracked_stat*, void*, int))
{
    update_value_fn = usvp;
    check_req_fn = csrp;
    get_string_value_fn = gssvp;
    reset_value_fn = rsvp;
    get_stat_data_fn = gsdp;
    set_stat_data_fn = ssdp;
}

// 0x007C6D70 (:126)
// Original: the data functions are left as they are.
stat_function_pointers::stat_function_pointers()
{
    update_value_fn = nullptr;
    check_req_fn = nullptr;
    get_string_value_fn = nullptr;
    reset_value_fn = nullptr;
}

// 0x007C6D80 (:1351)
bool cheats_are_enabled()
{
    return !stats_can_change();
}

// 0x007C6DB0 (:1371)
bool stats_achievements_initialized()
{
    return Achievements_initialized;
}

// 0x007C6DC0 (:1376)
void stats_set_achievements_initialized(bool new_val)
{
    Achievements_initialized = new_val;
}

// ---- the value type functions

// 0x007D3780 (:1391)
// Clamped to the signed byte range.
static void update_int8_value(tracked_stat* stat_ptr, const void* int_ptr)
{
    const int value = stat_ptr->value.integer8 + *(const int*)int_ptr;
    if (value < -0x80)
    {
        stat_ptr->value.integer8 = -0x80;
        return;
    }
    stat_ptr->value.integer8 = (signed char)(value > 0x7F ? 0x7F : value);
}

// 0x007D37C0 (:1403)
static void update_int16_value(tracked_stat* stat_ptr, const void* int_ptr)
{
    const int value = stat_ptr->value.integer16 + *(const int*)int_ptr;
    if (value < -0x8000)
    {
        stat_ptr->value.integer16 = -0x8000;
        return;
    }
    stat_ptr->value.integer16 = (short)(value > 0x7FFF ? 0x7FFF : value);
}

// 0x007C6DD0 (:1415)
static void update_int_value(tracked_stat* stat_ptr, const void* int_ptr)
{
    stat_ptr->value.integer += *(const int*)int_ptr;
}

// 0x007C6DF0 (:1425)
static void update_float_value(tracked_stat* stat_ptr, const void* float_ptr)
{
    stat_ptr->value.floating = *(const float*)float_ptr + stat_ptr->value.floating;
}

// 0x007C6E20 (:1435)
static void update_time_value(tracked_stat* stat_ptr, const void* time_ptr)
{
    stat_ptr->value.floating = *(const float*)time_ptr + stat_ptr->value.floating;
}

// 0x007C6E50 (:1446)
static void update_bool_value(tracked_stat* stat_ptr, const void* bool_ptr)
{
    stat_ptr->value.flagged = *(const bool*)bool_ptr;
}

// 0x007C6E70 (:1456)
static void update_percent_value(tracked_stat* stat_ptr, const void* int_ptr)
{
    stat_ptr->value.integer += *(const int*)int_ptr;
}

// 0x007C6E90 (:1466)
static void update_percent_complete_value(tracked_stat* stat_ptr, const void* ignored_ptr)
{
}

// 0x007C6EA0 (:1472)
static void update_missions_completed_value(tracked_stat* stat_ptr, const void* ignored_ptr)
{
}

// The integer value of a stat (stats_get_stat_value, inlined in several functions).
static int stats_value_of(const tracked_stat* tsp)
{
    return stats_get_stat_value(tsp);
}

// 0x007C6EB0 (:1577)
// LTCG: stat in ECX. The weapon of a single player weapon kill stat.
static const weapon_info* get_weapon_info_from_stat(tracked_stats_indices stat)
{
    // Original: the range test comes first, so the spiker (outside it) never matches.
    if ((unsigned int)(stat - STATS_SP_TOTAL_WEAPON_KILLS_SLEDGEHAMMER) > (unsigned int)(STATS_SP_TOTAL_WEAPON_KILLS_GUTTER - STATS_SP_TOTAL_WEAPON_KILLS_SLEDGEHAMMER))
    {
        return nullptr;
    }
    if (stat > STATS_SP_TOTAL_WEAPON_KILLS_SPIKER)
    {
        return nullptr;
    }
    if (stat == STATS_SP_TOTAL_WEAPON_KILLS_SPIKER)
    {
        return weapons_get_weapon_info("spiker");
    }
    switch (stat)
    {
    case STATS_SP_TOTAL_WEAPON_KILLS_SLEDGEHAMMER: return weapons_get_weapon_info("sledgehammer");
    case STATS_SP_TOTAL_WEAPON_KILLS_REMOTE_CHARGES: return weapons_get_weapon_info("charge_placer");
    case STATS_SP_TOTAL_WEAPON_KILLS_PROXIMITY_MINES: return weapons_get_weapon_info("mine_placer");
    case STATS_SP_TOTAL_WEAPON_KILLS_THERMOBARIC_ROCKETS: return weapons_get_weapon_info("thermobaric_rockets");
    case STATS_SP_TOTAL_WEAPON_KILLS_ROCKET_LAUNCHER: return weapons_get_weapon_info("rpg_launcher");
    case STATS_SP_TOTAL_WEAPON_KILLS_GRINDER: return weapons_get_weapon_info("grinder");
    case STATS_SP_TOTAL_WEAPON_KILLS_ARC_WELDER: return weapons_get_weapon_info("arc_welder");
    case STATS_SP_TOTAL_WEAPON_KILLS_PISTOL: return weapons_get_weapon_info("edf_pistol");
    case STATS_SP_TOTAL_WEAPON_KILLS_M16: return weapons_get_weapon_info("m16");
    case STATS_SP_TOTAL_WEAPON_KILLS_ENFORCER: return weapons_get_weapon_info("enforcer");
    case STATS_SP_TOTAL_WEAPON_KILLS_AUTOBLASTER: return weapons_get_weapon_info("autoblaster");
    case STATS_SP_TOTAL_WEAPON_KILLS_GAUSS_RIFLE: return weapons_get_weapon_info("gauss_rifle");
    case STATS_SP_TOTAL_WEAPON_KILLS_SNIPER_RIFLE: return weapons_get_weapon_info("sniper_rifle");
    case STATS_SP_TOTAL_WEAPON_KILLS_RAIL_DRIVER: return weapons_get_weapon_info("rail_driver");
    case STATS_SP_TOTAL_WEAPON_KILLS_SINGULARITY_BOMB: return weapons_get_weapon_info("singularity_bomb");
    case STATS_SP_TOTAL_WEAPON_KILLS_NANO_RIFLE: return weapons_get_weapon_info("nano_rifle");
    case STATS_SP_TOTAL_WEAPON_KILLS_SHOTGUN: return weapons_get_weapon_info("shotgun");
    case STATS_SP_TOTAL_WEAPON_KILLS_GUTTER: return weapons_get_weapon_info("gutter");
    default: return nullptr;
    }
}

// 0x007E9DE0 (:1478)
// The most and least used weapons by kills (the stat indices into the most/least stats): 99/100 (both unused) when
// nothing was killed. When they would be the same weapon, the least is the first weapon without kills, if any.
static void set_best_and_worst_weapons()
{
    // Magic numbers: 99 and 100 are the "no weapon" most and least values, 98 (STATS_SP_TOTAL_WEAPON_KILLS_LEAST) the
    // "none found" mark of the second pass.
    int most = 0;
    unsigned int most_index = 99;
    int least = 0;
    unsigned int least_index = 100;
    for (unsigned int i = STATS_SP_TOTAL_WEAPON_KILLS_SLEDGEHAMMER; i <= STATS_SP_TOTAL_WEAPON_KILLS_GUTTER; i++)
    {
        if (Stats[i].name == nullptr)
        {
            continue;
        }
        const int value = stats_value_of(stats_get_stat_ptr((tracked_stats_indices)i)); // inlined in the binary
        if (most < value)
        {
            most = value;
            most_index = i;
        }
        if (value > 0 && (value < least || least == 0))
        {
            least = value;
            least_index = i;
        }
    }
    if (Stats[STATS_SP_TOTAL_WEAPON_KILLS_SPIKER].name != nullptr)
    {
        const int value = stats_value_of(&Stats[STATS_SP_TOTAL_WEAPON_KILLS_SPIKER]); // inlined in the binary
        if (most < value)
        {
            most_index = STATS_SP_TOTAL_WEAPON_KILLS_SPIKER;
            most = value;
        }
        if (value > 0 && (value < least || least == 0))
        {
            least_index = STATS_SP_TOTAL_WEAPON_KILLS_SPIKER;
            least = value;
        }
    }
    if (most == 0)
    {
        stats_set_stat(STATS_SP_TOTAL_WEAPON_KILLS_LEAST, 100);
        stats_set_stat(STATS_SP_TOTAL_WEAPON_KILLS_MOST, 99);
        return;
    }
    if (most != least && least_index != most_index)
    {
        stats_set_stat(STATS_SP_TOTAL_WEAPON_KILLS_LEAST, (int)least_index);
        stats_set_stat(STATS_SP_TOTAL_WEAPON_KILLS_MOST, (int)most_index);
        return;
    }
    // Second pass: the least among the others, and the first weapon without kills.
    unsigned int zero_index = STATS_SP_TOTAL_WEAPON_KILLS_LEAST;
    for (unsigned int i = STATS_SP_TOTAL_WEAPON_KILLS_SLEDGEHAMMER; i <= STATS_SP_TOTAL_WEAPON_KILLS_GUTTER; i++)
    {
        if (Stats[i].name == nullptr || i == most_index)
        {
            continue;
        }
        const int value = stats_value_of(stats_get_stat_ptr((tracked_stats_indices)i)); // inlined in the binary
        if (zero_index == STATS_SP_TOTAL_WEAPON_KILLS_LEAST && value == 0)
        {
            zero_index = i;
            continue;
        }
        if (value > 0 && value <= least)
        {
            least = value;
            least_index = i;
        }
    }
    if (Stats[STATS_SP_TOTAL_WEAPON_KILLS_SPIKER].name != nullptr)
    {
        const int value = stats_value_of(&Stats[STATS_SP_TOTAL_WEAPON_KILLS_SPIKER]); // inlined in the binary
        if (most_index != STATS_SP_TOTAL_WEAPON_KILLS_SPIKER)
        {
            if (zero_index == STATS_SP_TOTAL_WEAPON_KILLS_LEAST && value == 0)
            {
                zero_index = STATS_SP_TOTAL_WEAPON_KILLS_SPIKER;
            }
            else if (value > 0 && value <= least)
            {
                least_index = STATS_SP_TOTAL_WEAPON_KILLS_SPIKER;
            }
        }
    }
    if (most_index == least_index && zero_index != STATS_SP_TOTAL_WEAPON_KILLS_LEAST)
    {
        least_index = zero_index;
    }
    stats_set_stat(STATS_SP_TOTAL_WEAPON_KILLS_LEAST, (int)least_index);
    stats_set_stat(STATS_SP_TOTAL_WEAPON_KILLS_MOST, (int)most_index);
}

// 0x007C7040 (:1662)
static bool check_int8_requirement(const tracked_stat* tsp, const achievement_requirement* arp)
{
    if (arp->condition == REQUIREMENT_CONDITION_AT_LEAST)
    {
        return arp->requirement.i8_value <= tsp->value.integer8;
    }
    if (arp->condition == REQUIREMENT_CONDITION_AT_MOST)
    {
        return tsp->value.integer8 <= arp->requirement.i8_value;
    }
    return false;
}

// 0x007C7070 (:1682)
static bool check_int16_requirement(const tracked_stat* tsp, const achievement_requirement* arp)
{
    if (arp->condition == REQUIREMENT_CONDITION_AT_LEAST)
    {
        return arp->requirement.i16_value <= tsp->value.integer16;
    }
    if (arp->condition == REQUIREMENT_CONDITION_AT_MOST)
    {
        return tsp->value.integer16 <= arp->requirement.i16_value;
    }
    return false;
}

// 0x007C70A0 (:1702)
static bool check_int_requirement(const tracked_stat* tsp, const achievement_requirement* arp)
{
    if (arp->condition == REQUIREMENT_CONDITION_AT_LEAST)
    {
        return arp->requirement.i_value <= tsp->value.integer;
    }
    if (arp->condition == REQUIREMENT_CONDITION_AT_MOST)
    {
        return tsp->value.integer <= arp->requirement.i_value;
    }
    return false;
}

// 0x007C70D0 (:1722)
static bool check_float_requirement(const tracked_stat* tsp, const achievement_requirement* arp)
{
    if (arp->condition == REQUIREMENT_CONDITION_AT_LEAST)
    {
        return arp->requirement.f_value <= tsp->value.floating;
    }
    if (arp->condition == REQUIREMENT_CONDITION_AT_MOST)
    {
        return tsp->value.floating <= arp->requirement.f_value;
    }
    return false;
}

// 0x007C7120 (:1742)
static bool check_time_requirement(const tracked_stat* tsp, const achievement_requirement* arp)
{
    if (arp->condition == REQUIREMENT_CONDITION_AT_LEAST)
    {
        return arp->requirement.f_value <= tsp->value.floating;
    }
    if (arp->condition == REQUIREMENT_CONDITION_AT_MOST)
    {
        return tsp->value.floating <= arp->requirement.f_value;
    }
    return false;
}

// 0x007C7170 (:1762)
// At least: set; at most: not set.
static bool check_bool_requirement(const tracked_stat* tsp, const achievement_requirement* arp)
{
    if (arp->condition == REQUIREMENT_CONDITION_AT_LEAST)
    {
        return tsp->value.flagged;
    }
    if (arp->condition == REQUIREMENT_CONDITION_AT_MOST)
    {
        return !tsp->value.flagged;
    }
    return false;
}

// 0x007D3800 (:1782)
// The stat as a fraction of its percentage stat (1 when that is not positive).
static bool check_percent_requirement(const tracked_stat* tsp, const achievement_requirement* arp)
{
    const tracked_stat* total = stats_get_stat_ptr(tsp->value.percent.pct_stat); // inlined in the binary
    float fraction = 1.0f;
    if (total->value.integer > 0)
    {
        fraction = (float)tsp->value.integer / (float)total->value.integer;
    }
    if (arp->condition == REQUIREMENT_CONDITION_AT_LEAST)
    {
        return arp->requirement.f_value <= fraction;
    }
    if (arp->condition == REQUIREMENT_CONDITION_AT_MOST)
    {
        return fraction <= arp->requirement.f_value;
    }
    return false;
}

// 0x007C71A0 (:1807)
static void get_int8_data(void*& data_ptr, int& length, const tracked_stat* tsp)
{
    data_ptr = (void*)&tsp->value;
    length = 1;
}

// 0x007C71C0 (:1819)
static void get_int16_data(void*& data_ptr, int& length, const tracked_stat* tsp)
{
    data_ptr = (void*)&tsp->value;
    length = 2;
}

// 0x007C71E0 (:1831)
static void get_int_data(void*& data_ptr, int& length, const tracked_stat* tsp)
{
    data_ptr = (void*)&tsp->value;
    length = 4;
}

// 0x007C7200 (:1843)
static void get_float_data(void*& data_ptr, int& length, const tracked_stat* tsp)
{
    data_ptr = (void*)&tsp->value;
    length = 4;
}

// 0x007C7220 (:1855)
static void get_time_data(void*& data_ptr, int& length, const tracked_stat* tsp)
{
    data_ptr = (void*)&tsp->value;
    length = 4;
}

// 0x007C7240 (:1867)
static void get_bool_data(void*& data_ptr, int& length, const tracked_stat* tsp)
{
    data_ptr = (void*)&tsp->value;
    length = 1;
}

// 0x007C7260 (:1879)
static void get_percent_data(void*& data_ptr, int& length, const tracked_stat* tsp)
{
    data_ptr = (void*)&tsp->value;
    length = 4;
}

// 0x007C7280 (:1895)
// LTCG: max_chars in ECX, data in EAX.
static void get_int8_string_value(char* value, int max_chars, int data)
{
    _snprintf_safe(value, max_chars, "%d", data);
}

// 0x007C72A0 (:1899)
static void get_int8_string_value(char* value, int max_chars, const tracked_stat* tsp)
{
    _snprintf_safe(value, max_chars, "%d", (int)tsp->value.integer8);
}

// 0x007C72D0 (:1910)
// LTCG: max_chars in ECX, data in AX.
static void get_int16_string_value(char* value, int max_chars, unsigned short data)
{
    _snprintf_safe(value, max_chars, "%hd", (unsigned int)data);
}

// 0x007C72F0 (:1914)
static void get_int16_string_value(char* value, int max_chars, const tracked_stat* tsp)
{
    _snprintf_safe(value, max_chars, "%hd", (unsigned int)(unsigned short)tsp->value.integer16);
}

// 0x007C7320 (:1925)
// LTCG: max_chars in ECX, data in EAX.
static void get_int_string_value(char* value, int max_chars, int data)
{
    _snprintf_safe(value, max_chars, "%d", data);
}

// 0x007C7340 (:1929)
static void get_int_string_value(char* value, int max_chars, const tracked_stat* tsp)
{
    _snprintf_safe(value, max_chars, "%d", tsp->value.integer);
}

// 0x007C7370 (:1940)
// LTCG: value in ECX, max_chars in EAX, data in XMM0.
static void get_float_string_value(char* value, int max_chars, float data)
{
    _snprintf_safe(value, max_chars, "%0.2f", (double)data);
}

// 0x007C7390 (:1944)
static void get_float_string_value(char* value, int max_chars, const tracked_stat* tsp)
{
    _snprintf_safe(value, max_chars, "%0.2f", (double)tsp->value.floating);
}

// 0x007C73C0 (:1973)
// LTCG: total_seconds in ECX. hh:mm:ss, or mm:ss under an hour.
static void get_time_string_value(char* value, int max_chars, int total_seconds)
{
    // Magic numbers: 3600 seconds an hour, 60 a minute.
    const int hours = total_seconds / 3600;
    const int minutes = (total_seconds - hours * 3600) / 60;
    const int seconds = total_seconds % 60;
    if (hours > 0)
    {
        _snprintf_safe(value, max_chars, "%02d:%02d:%02d", hours, minutes, seconds);
        return;
    }
    _snprintf_safe(value, max_chars, "%02d:%02d", minutes, seconds);
}

// 0x007C7450 (:1985)
static void get_time_string_value(char* value, int max_chars, const tracked_stat* tsp)
{
    get_time_string_value(value, max_chars, (int)tsp->value.floating); // inlined in the binary
}

// 0x007C74F0 (:1996)
static void get_bool_string_value(char* value, int max_chars, const tracked_stat* tsp)
{
}

// 0x007C7500 (:2008)
// LTCG: value in ECX, total in EAX. current / total in percent (0 when total is not positive).
static void get_percent_string_value(char* value, int max_chars, int current, int total)
{
    float percent = 0.0f;
    if (total > 0)
    {
        // Magic number: 100, the percent scale (in doubles).
        percent = (float)((double)(float)current / (double)(float)total * 100.0);
    }
    _snprintf_safe(value, max_chars, "%0.2f%%", (double)percent);
}

// 0x007D38A0 (:2019)
static void get_percent_string_value(char* value, int max_chars, const tracked_stat* tsp)
{
    const tracked_stat* total = stats_get_stat_ptr(tsp->value.percent.pct_stat); // inlined in the binary
    if (total == nullptr)
    {
        return;
    }
    get_percent_string_value(value, max_chars, tsp->value.integer, total->value.integer); // inlined in the binary
}

// 0x007C7550 (:2027)
// LTCG: value in ECX, max_chars in EAX, percent in XMM0.
static void get_percent_string_value(char* value, int max_chars, float percent)
{
    _snprintf_safe(value, max_chars, "%0.2f%%", (double)percent);
}

// 0x007C7570 (:2038)
static void get_money_string_value(char* value, int max_chars, const tracked_stat* tsp)
{
}

// 0x007D3920 (:2050)
// LTCG: value in ECX, data in EAX. Clamped to 0-100.
static void get_percent_complete_string_value(char* value, int max_chars, int data)
{
    if (data >= 100)
    {
        data = 100;
    }
    else if (data <= 0)
    {
        data = 0;
    }
    _snprintf_safe(value, max_chars, "%d%%", data);
}

// 0x007D3960 (:2056)
static void get_percent_complete_string_value(char* value, int max_chars, const tracked_stat* tsp)
{
    get_percent_complete_string_value(value, max_chars, tsp->value.integer); // inlined in the binary
}

// 0x007C7580 (:2067)
static void get_missions_completed_string_value(char* value, int max_chars, const tracked_stat* tsp)
{
}

// 0x007C7590 (:2085)
static void reset_int8_value(tracked_stat* stat_ptr)
{
    stat_ptr->value.integer8 = 0;
}

// 0x007C75A0 (:2094)
static void reset_int16_value(tracked_stat* stat_ptr)
{
    stat_ptr->value.integer16 = 0;
}

// 0x007C75B0 (:2103)
static void reset_int_value(tracked_stat* stat_ptr)
{
    stat_ptr->value.integer = 0;
}

// 0x007C75C0 (:2112)
static void reset_float_value(tracked_stat* stat_ptr)
{
    stat_ptr->value.integer = 0;
}

// 0x007C75D0 (:2121)
static void reset_time_value(tracked_stat* stat_ptr)
{
    stat_ptr->value.integer = 0;
}

// 0x007C75E0 (:2130)
static void reset_bool_value(tracked_stat* stat_ptr)
{
    stat_ptr->value.flagged = false;
}

// 0x007C75F0 (:2139)
static void reset_percent_value(tracked_stat* stat_ptr)
{
    stat_ptr->value.integer = 0;
}

// 0x007C7600 (:2148)
static void reset_percent_complete_value(tracked_stat* stat_ptr)
{
}

// 0x007C7610 (:2157)
static void reset_missions_completed_value(tracked_stat* stat_ptr)
{
}

// 0x007C7620 (:2172)
static bool set_int8_data(tracked_stat* tsp, void* data_ptr, int length)
{
    if (length != 1)
    {
        return false;
    }
    memmove(&tsp->value, data_ptr, 1);
    return true;
}

// 0x007C7650 (:2189)
static bool set_int16_data(tracked_stat* tsp, void* data_ptr, int length)
{
    if (length != 2)
    {
        return false;
    }
    memmove(&tsp->value, data_ptr, 2);
    return true;
}

// 0x007C7680 (:2206)
static bool set_int_data(tracked_stat* tsp, void* data_ptr, int length)
{
    if (length != 4)
    {
        return false;
    }
    memmove(&tsp->value, data_ptr, 4);
    return true;
}

// 0x007C76B0 (:2223)
static bool set_float_data(tracked_stat* tsp, void* data_ptr, int length)
{
    if (length != 4)
    {
        return false;
    }
    memmove(&tsp->value, data_ptr, 4);
    return true;
}

// 0x007C76E0 (:2240)
static bool set_time_data(tracked_stat* tsp, void* data_ptr, int length)
{
    if (length != 4)
    {
        return false;
    }
    memmove(&tsp->value, data_ptr, 4);
    return true;
}

// 0x007C7710 (:2257)
static bool set_bool_data(tracked_stat* tsp, void* data_ptr, int length)
{
    if (length != 1)
    {
        return false;
    }
    memmove(&tsp->value, data_ptr, 1);
    return true;
}

// 0x007C7740 (:2274)
static bool set_percent_data(tracked_stat* tsp, void* data_ptr, int length)
{
    if (length != 4)
    {
        return false;
    }
    memmove(&tsp->value, data_ptr, 4);
    return true;
}

// The stat named name (null names skipped), -1 when none (inlined in parse_stats_data).
static int stats_find_index_by_name(const char* name)
{
    for (int i = 0; i < STATS_NUM_STATS; i++)
    {
        if (Stats[i].name != nullptr && strcmp(Stats[i].name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

// 0x007D39A0 (:2313)
// The stats, achievements and hidden challenges of the base game (dlc_id -1) or a DLC bundle; the stat names go into
// Stats_string_pool (0x7800 bytes of Permanent_mempool, allocated once).
static void parse_stats_data(int dlc_id)
{
    memset(Stats, 0, sizeof(Stats));
    const bool dlc = dlc_id != -1;
    if (Stats_string_pool_data == nullptr)
    {
        // Magic number: 0x7800 bytes of stat and achievement names.
        Stats_string_pool_data = (char*)Permanent_mempool.static_mempool_base::alloc(0x7800, 0);
        if (Stats_string_pool_data == nullptr)
        {
            keen::debug::nativePrintArgs("Insufficient memory to allocate the stats string table");
        }
        Stats_string_pool.init(Stats_string_pool_data, 0x7800, STATS_STRING_POOL_NAME, 0, 0);
    }
    Stats_string_pool.set_thread_ownership();
    Stats_string_pool.clear();
    char filename[256];
    xml_element* table;
    if (dlc)
    {
        sprintf(filename, "dlc%02d_%s", dlc_id, "stats.xtbl");
        table = xml_table_parse(filename);
    }
    else
    {
        table = xml_table_parse("stats.xtbl");
    }

    // ---- stats.xtbl
    Num_stats = 0;
    for (xml_element* element = table->elements; element != nullptr; element = element->next)
    {
        if (strcmp(element->name, "Stat") != 0)
        {
            continue;
        }
        const char* name = Stats_string_pool.add_unique(xml_table_get_req_string_ref(element, "Name"));
        int entry = 0;
        while (strcmp(name, Stat_table_entries[entry].table_name) != 0)
        {
            entry++;
            if (entry > STATS_NUM_STATS - 1)
            {
                break;
            }
        }
        if (entry > STATS_NUM_STATS - 1)
        {
            // Original: the rest of the table is not read.
            char message[STATS_STRING_SIZE];
            sprintf(message, "Unknown stat entry '%s' encountered", name);
            keen::debug::nativePrintArgs(message);
            break;
        }
        const tracked_stats_indices index = Stat_table_entries[entry].stat_index;
        tracked_stat& stat = Stats[index];
        stat.name = name;
        stat.num_achievement_refs = 0;
        keen::copyString(stat.display_name_loca_tag, 0x40, xml_table_get_req_string_ref(element, "DisplayName"));
        stat.display_name = localization_get_display_string_req(stat.display_name_loca_tag, nullptr);
        static const char* const value_type_names[] = {"integer8", "integer16", "integer", "float", "distance", "time", "money", "boolean", "percent", "complex", nullptr};
        xml_element* value_type_root = xml_table_find_req(element, "Value_Type");
        xml_element* value_type = nullptr;
        const char* type_name = nullptr;
        for (int i = 0; value_type_names[i] != nullptr; i++)
        {
            value_type = xml_table_find_opt(value_type_root, value_type_names[i]);
            if (value_type != nullptr)
            {
                type_name = value_type_names[i];
                break;
            }
        }
        if (type_name != nullptr)
        {
            if (strcmp("integer8", type_name) == 0)
            {
                stat.value_type = TRACKED_STAT_INT8;
            }
            else if (strcmp("integer16", type_name) == 0)
            {
                stat.value_type = TRACKED_STAT_INT16;
            }
            else if (strcmp("integer", type_name) == 0)
            {
                stat.value_type = TRACKED_STAT_INT;
            }
            else if (strcmp("float", type_name) == 0)
            {
                stat.value_type = TRACKED_STAT_FLOAT;
            }
            else if (strcmp("distance", type_name) == 0)
            {
                stat.value_type = TRACKED_STAT_DISTANCE;
            }
            else if (strcmp("time", type_name) == 0)
            {
                stat.value_type = TRACKED_STAT_TIME;
            }
            else if (strcmp("money", type_name) == 0)
            {
                stat.value_type = TRACKED_STAT_MONEY;
            }
            else if (strcmp("boolean", type_name) == 0)
            {
                stat.value_type = TRACKED_STAT_BOOL;
            }
            else if (strcmp("percent", type_name) == 0)
            {
                // The stat it is a percentage of, by name among the stats read so far.
                stat.value_type = TRACKED_STAT_PERCENT;
                stat.value.percent.pct_stat = STATS_INVALID;
                const int total = stats_find_index_by_name(xml_table_get_req_string_ref(value_type, "Percentage_Of_Stat"));
                if (total >= 0)
                {
                    stat.value.percent.pct_stat = (tracked_stats_indices)total;
                }
            }
            else if (strcmp("complex", type_name) == 0)
            {
                if (Stat_table_entries[entry].stat_index == STATS_MISSIONS_COMPLETED)
                {
                    stat.value_type = TRACKED_STAT_MISSIONS_COMPLETED;
                }
                else
                {
                    char message[STATS_STRING_SIZE];
                    sprintf(message, "Handlers not set up for complex stat %s.", stat.name);
                    keen::debug::nativePrintArgs(message);
                }
            }
            else
            {
                keen::debug::nativePrintArgs("Unknown value type.");
            }
        }
        stat.flags.display_on_stats_screen = strcmp("true", xml_table_get_req_string_ref(element, "Display_Stat")) == 0;
        stat.flags.allow_update_by_server = strcmp("true", xml_table_get_req_string_ref(element, "Allow_Update_By_Server")) == 0;
        stat.live_leaderboard_id = 0;
        stat.live_property_id = 0;
        xml_table_get_opt_int(stat.live_property_id, element, "LivePropertyID");
        xml_table_get_opt_int(stat.live_leaderboard_id, element, "LiveLeaderboardID");
        // Original: compared with "True".
        stat.flags.is_stored_in_save_game = strcmp("True", xml_table_get_req_string_ref(element, "Store_in_Save_Game")) == 0;
        Num_stats++;
    }
    xml_table_close(table);

    // ---- achievements.xtbl
    if (!dlc)
    {
        table = xml_table_parse("achievements.xtbl");
    }
    else
    {
        sprintf(filename, "dlc%02d_%s", dlc_id, "achievements.xtbl");
        table = xml_table_parse(filename);
    }
    Num_achievements = 0;
    int max_position = 0; // unused
    for (xml_element* element = table->elements; element != nullptr; element = element->next)
    {
        if (strcmp(element->name, "Achievement") != 0)
        {
            continue;
        }
        int position;
        xml_table_get_req_int(position, element, "DisplayPosition");
        achievement& ap = Achievements[position];
        if (max_position <= position)
        {
            max_position = position;
        }
        ap.has_been_achieved = false;
        ap.has_unlocked_cheat = false;
        ap.name = Stats_string_pool.add_unique(xml_table_get_req_string_ref(element, "Name"));
        xml_table_get_req_bool(ap.unlocks_cheat, element, "Unlocks_Cheat");
        xml_table_get_req_int(ap.achievement_id, element, "InternalId");
        keen::copyString(ap.display_name_loca_tag, 0x40, xml_table_get_req_string_ref(element, "DisplayName"));
        ap.display_name = localization_get_display_string_req(ap.display_name_loca_tag, nullptr);
        keen::copyString(ap.display_desc_loca_tag, 0x40, xml_table_get_req_string_ref(element, "Description"));
        ap.display_desc = localization_get_display_string_req(ap.display_desc_loca_tag, nullptr);
        ap.img_name = Stats_string_pool.add_unique(xml_table_get_req_string_ref(element, "ImageName"));
        xml_element* requirements = xml_table_find_opt(element, "Requirements");
        ap.num_requirements = 0;
        for (xml_element* req = requirements->elements; req != nullptr; req = req->next)
        {
            if (strcmp(req->name, "Requirement") != 0)
            {
                continue;
            }
            // Magic number: 40 requirements (achievement::requirements).
            if (ap.num_requirements >= 40)
            {
                break;
            }
            achievement_requirement& r = ap.requirements[ap.num_requirements];
            const char* stat_name = xml_table_get_req_string_ref(req, "Stat");
            r.stat = nullptr;
            const int index = stats_find_index_by_name(stat_name);
            if (index < 0)
            {
                continue;
            }
            tracked_stat& stat = Stats[index];
            r.stat = &stat;
            // Magic number: 8 achievement references per stat. Original: a full stat ends the achievement.
            if (stat.num_achievement_refs > 7)
            {
                break;
            }
            stat.achievement_refs[stat.num_achievement_refs] = &ap;
            stat.num_achievement_refs++;
            const char* condition = xml_table_get_req_string_ref(req, "Condition");
            if (strcmp("at least", condition) == 0)
            {
                r.condition = REQUIREMENT_CONDITION_AT_LEAST;
            }
            else if (strcmp("at most", condition) == 0)
            {
                r.condition = REQUIREMENT_CONDITION_AT_MOST;
            }
            else
            {
                char message[STATS_STRING_SIZE];
                sprintf(message, "Unknown achievement requirement condition: %s", condition);
                keen::debug::nativePrintArgs(message);
            }
            switch (stat.value_type)
            {
            case TRACKED_STAT_INT8:
            {
                int value;
                xml_table_get_req_int(value, req, "Value");
                r.requirement.i8_value = (signed char)value;
                break;
            }
            case TRACKED_STAT_INT16:
            {
                int value;
                xml_table_get_req_int(value, req, "Value");
                r.requirement.i16_value = (short)value;
                break;
            }
            case TRACKED_STAT_INT:
            case TRACKED_STAT_BOOL:
            case TRACKED_STAT_PERCENT_COMPLETE:
            case TRACKED_STAT_MISSIONS_COMPLETED:
                xml_table_get_req_int(r.requirement.i_value, req, "Value");
                break;
            case TRACKED_STAT_FLOAT:
            case TRACKED_STAT_DISTANCE:
            case TRACKED_STAT_TIME:
            case TRACKED_STAT_MONEY:
            case TRACKED_STAT_PERCENT:
                xml_table_get_req_float(r.requirement.f_value, req, "Value");
                break;
            case TRACKED_STAT_WRECKING_CREW_MAP_MODE_COMBINATIONS_PLAYED:
                r.requirement.i_value = 0;
                break;
            default:
            {
                char message[STATS_STRING_SIZE];
                sprintf(message, "Unexpected tracked stat requirement value %i\n", stat.value_type);
                keen::debug::nativePrintArgs(message);
                break;
            }
            }
            ap.num_requirements++;
        }
        Num_achievements++;
    }
    xml_table_close(table);

    // ---- mp_hidden_challenges.xtbl
    if (!dlc)
    {
        table = xml_table_parse("mp_hidden_challenges.xtbl");
    }
    else
    {
        sprintf(filename, "dlc%02d_%s", dlc_id, "mp_hidden_challenges.xtbl");
        table = xml_table_parse(filename);
    }
    Num_hidden_challenges = 0;
    for (xml_element* element = table->elements; element != nullptr; element = element->next)
    {
        if (strcmp(element->name, "Hidden_Challenge") != 0)
        {
            continue;
        }
        hidden_challenge& hc = Hidden_challenges[Num_hidden_challenges];
        hc.has_been_achieved = false;
        strncpy_safe(hc.display_name, xml_table_get_req_string_ref(element, "display_name"), 0x20);
        const char* award_condition = xml_table_get_req_string_ref(element, "Award_Condition");
        if (v_stricmp(award_condition, "Any Stat") == 0)
        {
            hc.award_condition = HCAC_AWARD_ON_ANY_STAT;
        }
        else if (v_stricmp(award_condition, "All Stats") == 0)
        {
            hc.award_condition = HCAC_AWARD_ON_ALL_STATS;
        }
        else
        {
            char message[STATS_STRING_SIZE];
            sprintf(message, "Unknown hidden_challenge award condition: %s", award_condition);
            keen::debug::nativePrintArgs(message);
        }
        const char* condition = xml_table_get_req_string_ref(element, "Condition");
        if (strcmp("at least", condition) == 0)
        {
            hc.condition = REQUIREMENT_CONDITION_AT_LEAST;
        }
        else if (strcmp("at most", condition) == 0)
        {
            hc.condition = REQUIREMENT_CONDITION_AT_MOST;
        }
        else
        {
            char message[STATS_STRING_SIZE];
            sprintf(message, "Unknown hidden_challenge requirement condition: %s", condition);
            keen::debug::nativePrintArgs(message);
        }
        xml_table_get_req_float(hc.required_value, element, "Value");
        // Original: the Requirements element is not checked.
        xml_element* requirements = xml_table_find_opt(element, "Requirements");
        hc.num_stats = 0;
        for (xml_element* req = requirements->elements; req != nullptr; req = req->next)
        {
            if (strcmp(req->name, "Requirement") != 0)
            {
                continue;
            }
            // Magic number: 64 stats (hidden_challenge::stats).
            if (hc.num_stats > 0x3F)
            {
                break;
            }
            const int index = stats_find_index_by_name(xml_table_get_req_string_ref(req, "Stat"));
            if (index >= 0)
            {
                hc.stats[hc.num_stats] = &Stats[index];
            }
            // Original: an unknown stat keeps the slot's previous value (and indexes past Stats if it is set).
            if (hc.stats[hc.num_stats] != nullptr)
            {
                tracked_stat& stat = Stats[index >= 0 ? index : STATS_NUM_STATS];
                // Magic number: 8 hidden challenge references per stat.
                if (stat.num_hidden_challenge_refs > 7)
                {
                    break;
                }
                stat.hidden_challenge_refs[stat.num_hidden_challenge_refs] = &hc;
                stat.num_hidden_challenge_refs++;
                hc.num_stats++;
            }
        }
        const char* display_type = xml_table_get_req_string_ref(element, "Display_Type");
        if (v_stricmp(display_type, "Number of Stats Complete") == 0)
        {
            hc.display_type = HCDT_NUM_COMPLETE;
        }
        else if (v_stricmp(display_type, "Most Progressed Stat") == 0)
        {
            hc.display_type = HCDT_MOST_COMPLETE;
        }
        else
        {
            char message[STATS_STRING_SIZE];
            sprintf(message, "Unknown hidden_challenge display type: %s", display_type);
            keen::debug::nativePrintArgs(message);
        }
        strncpy_safe(hc.icon_name, xml_table_get_req_string_ref(element, "icon_name"), 0x10);
        Num_hidden_challenges++;
    }
    xml_table_close(table);
}

// 0x007C7770 (:2743)
// LTCG: achievement_id in ESI. -1 for none.
static int get_achievement_index(int achievement_id)
{
    for (int i = 0; i < Num_achievements; i++)
    {
        if (Achievements[i].achievement_id == achievement_id)
        {
            return i;
        }
    }
    return -1;
}

// 0x007D4900 (:2757)
// The platform progress (the sum of the at-least requirements' values, floats in millionths, plus the met at-most time
// requirements) when below the achievement's unlock value, then the award when every requirement is met.
static void check_achievement(achievement* ap, bool unlock_cheats_only)
{
    if (!stats_can_change() || ap->has_been_achieved)
    {
        return;
    }
    const unsigned int unlock_value = keen::getAchievementUnlockValueMap()[ap->achievement_id];
    unsigned int progress = 0;
    for (int i = 0; i < ap->num_requirements; i++)
    {
        const achievement_requirement& req = ap->requirements[i];
        if (req.condition == REQUIREMENT_CONDITION_AT_LEAST)
        {
            const tracked_stat* stat = req.stat;
            switch (stat->value_type)
            {
            case TRACKED_STAT_INT8:
                progress += stat->value.integer8;
                break;
            case TRACKED_STAT_INT16:
                progress += stat->value.integer16;
                break;
            case TRACKED_STAT_INT:
                progress += stat->value.integer;
                break;
            case TRACKED_STAT_FLOAT:
                // Magic number: 1000000, the float stats' progress scale.
                progress += fl_round(stat->value.floating * 1000000.0f);
                break;
            default:
                VLIB_CRITICAL_FAILURE();
                break;
            }
        }
        else if (req.condition == REQUIREMENT_CONDITION_AT_MOST)
        {
            if (req.stat->value_type != TRACKED_STAT_TIME)
            {
                VLIB_CRITICAL_FAILURE();
            }
            else if (Stat_type_functions[TRACKED_STAT_TIME].check_req_fn != nullptr && Stat_type_functions[TRACKED_STAT_TIME].check_req_fn(req.stat, &req))
            {
                progress++;
            }
        }
    }
    if (unlock_value != 0 && progress < unlock_value && !unlock_cheats_only)
    {
        multi_core_platform_update_achievement_progress(ap->achievement_id, progress);
    }
    for (int i = 0; i < ap->num_requirements; i++)
    {
        const achievement_requirement& req = ap->requirements[i];
        const stat_function_pointers& functions = Stat_type_functions[req.stat->value_type];
        if (functions.check_req_fn == nullptr || !functions.check_req_fn(req.stat, &req))
        {
            return;
        }
    }
    stats_award_achievement(ap, unlock_cheats_only);
}

// A hidden challenge's required value as a requirement of the stat's type; false when it does not fit (inlined in
// check_hidden_challenge and update_hidden_challenges).
static bool hidden_challenge_requirement(const hidden_challenge* hcp, const tracked_stat* stat, achievement_requirement& req)
{
    req.stat = stat;
    switch (stat->value_type)
    {
    case TRACKED_STAT_INT8:
        // Magic number: 127.0, the signed byte maximum.
        if ((int)hcp->required_value < -0x80 || (double)hcp->required_value > 127.0)
        {
            return false;
        }
        req.requirement.i8_value = (signed char)(int)hcp->required_value;
        return true;
    case TRACKED_STAT_INT16:
        // Magic number: 32767.0, the short maximum.
        if ((int)hcp->required_value < -0x8000 || (double)hcp->required_value > 32767.0)
        {
            return false;
        }
        req.requirement.i16_value = (short)(int)hcp->required_value;
        return true;
    case TRACKED_STAT_INT:
    case TRACKED_STAT_BOOL:
    case TRACKED_STAT_PERCENT_COMPLETE:
    case TRACKED_STAT_MISSIONS_COMPLETED:
        req.requirement.i_value = (int)hcp->required_value;
        return true;
    case TRACKED_STAT_FLOAT:
    case TRACKED_STAT_DISTANCE:
    case TRACKED_STAT_TIME:
    case TRACKED_STAT_MONEY:
    case TRACKED_STAT_PERCENT:
        req.requirement.f_value = hcp->required_value;
        return true;
    default:
        return false;
    }
}

// The number of achieved hidden challenges into its best stat (inlined).
static void stats_update_hidden_challenges_complete()
{
    int complete = 0;
    for (int i = 0; i < Num_hidden_challenges; i++)
    {
        if (Hidden_challenges[i].has_been_achieved)
        {
            complete++;
        }
    }
    stats_update_best(STATS_MP_HIDDEN_CHALLENGES_COMPLETE, complete);
}

// 0x007E56A0 (:2859)
// Achieved when any or all of its stats meet the required value; the multiplayer HUD shows it when asked.
static void check_hidden_challenge(hidden_challenge* hcp, bool hud_display)
{
    if (hcp->has_been_achieved)
    {
        return;
    }
    achievement_requirement req;
    req.condition = hcp->condition;
    bool any_met = false;
    for (int i = 0; i < hcp->num_stats; i++)
    {
        if (!hidden_challenge_requirement(hcp, hcp->stats[i], req)) // inlined in the binary
        {
            return;
        }
        const stat_function_pointers& functions = Stat_type_functions[req.stat->value_type];
        if (functions.check_req_fn == nullptr || !functions.check_req_fn(req.stat, &req))
        {
            if (hcp->award_condition == HCAC_AWARD_ON_ALL_STATS)
            {
                return;
            }
        }
        else
        {
            any_met = true;
            if (hcp->award_condition == HCAC_AWARD_ON_ANY_STAT)
            {
                break;
            }
        }
    }
    if (!any_met)
    {
        return;
    }
    hcp->has_been_achieved = true;
    if (Multiplayer && hud_display)
    {
        multi_game_mode_hidden_challenge_complete(hcp);
    }
    stats_update_hidden_challenges_complete(); // inlined in the binary
}

// 0x007E5830 (:2945)
// Every hidden challenge re-checked from the current stats (without the HUD).
static void update_hidden_challenges()
{
    for (int i = 0; i < Num_hidden_challenges; i++)
    {
        Hidden_challenges[i].has_been_achieved = false;
    }
    for (int c = 0; c < Num_hidden_challenges; c++)
    {
        hidden_challenge* hcp = &Hidden_challenges[c];
        if (hcp->has_been_achieved)
        {
            continue;
        }
        achievement_requirement req;
        req.condition = hcp->condition;
        bool any_met = false;
        bool skip = false;
        for (int i = 0; i < hcp->num_stats; i++)
        {
            if (!hidden_challenge_requirement(hcp, hcp->stats[i], req)) // inlined in the binary
            {
                skip = true;
                break;
            }
            const stat_function_pointers& functions = Stat_type_functions[req.stat->value_type];
            if (functions.check_req_fn == nullptr || !functions.check_req_fn(req.stat, &req))
            {
                if (hcp->award_condition == HCAC_AWARD_ON_ALL_STATS)
                {
                    skip = true;
                    break;
                }
            }
            else
            {
                any_met = true;
                if (hcp->award_condition == HCAC_AWARD_ON_ANY_STAT)
                {
                    break;
                }
            }
        }
        if (skip || !any_met)
        {
            continue;
        }
        hcp->has_been_achieved = true;
        stats_update_hidden_challenges_complete(); // inlined in the binary
    }
    Hidden_challenges_init_complete = true;
}

// 0x007C77A0 (:2961)
// The update thread only marks the achievements initialized.
static int update_achievements_thread(void* param)
{
    Achievements_initialized = true;
    Update_achievements_thread_cs.enter();
    update_achievements_thread_handle = 0xFFFF;
    Update_achievements_thread_cs.leave();
    return 0;
}

// 0x007C77D0 (:3045)
void stats_spawn_update_achievements_thread()
{
    Update_achievements_thread_cs.enter();
    if (update_achievements_thread_handle == 0xFFFF)
    {
        update_achievements_thread_handle = thread_spawn(update_achievements_thread, nullptr, 0, THREAD_PRIOR_NORMAL, "RFG Thread", 0);
    }
    Update_achievements_thread_cs.leave();
}

// 0x007C7810 (:3053)
// Asks the thread to stop and waits for it.
void stats_shutdown_update_achievements_thread()
{
    Update_achievements_thread_cs.enter();
    if (update_achievements_thread_handle != 0xFFFF && !Update_achievements_thread_abort)
    {
        Update_achievements_thread_abort = true;
    }
    Update_achievements_thread_cs.leave();
    while (update_achievements_thread_handle != 0xFFFF)
    {
        os_sleep(1);
    }
    Update_achievements_thread_abort = false;
}

// 0x007E5A00 (:3080)
// The achievements cleared; signed in, the hidden challenges re-checked and the update thread started.
static void update_achievements()
{
    stats_reset_all_achievements(); // inlined in the binary
    if (multi_core_platform_is_signed_into_profile(Player_controller_index != 0xFFFFFFFFu ? (int)Player_controller_index : 0))
    {
        Achievements_initialized = false;
        update_hidden_challenges();
        stats_spawn_update_achievements_thread(); // inlined in the binary
    }
}

// 0x007EEC40 (:3109)
// The tables, the value type functions, the first Num_stats stats reset, the best/worst weapons, the achievements
// cleared and the profile stats read.
void stats_system_init(int dlc_id)
{
    parse_stats_data(dlc_id);
    if (!profile_boot_inited)
    {
        profile_boot_inited = true;
    }
    Stat_type_functions[TRACKED_STAT_INT8] = stat_function_pointers(update_int8_value, check_int8_requirement, get_int8_string_value, reset_int8_value, get_int8_data, set_int8_data);
    Stat_type_functions[TRACKED_STAT_INT16] = stat_function_pointers(update_int16_value, check_int16_requirement, get_int16_string_value, reset_int16_value, get_int16_data, set_int16_data);
    Stat_type_functions[TRACKED_STAT_INT] = stat_function_pointers(update_int_value, check_int_requirement, get_int_string_value, reset_int_value, get_int_data, set_int_data);
    Stat_type_functions[TRACKED_STAT_FLOAT] = stat_function_pointers(update_float_value, check_float_requirement, get_float_string_value, reset_float_value, get_float_data, set_float_data);
    Stat_type_functions[TRACKED_STAT_DISTANCE] = stat_function_pointers(update_float_value, check_float_requirement, get_float_string_value, reset_float_value, get_float_data, set_float_data);
    Stat_type_functions[TRACKED_STAT_TIME] = stat_function_pointers(update_time_value, check_time_requirement, get_time_string_value, reset_time_value, get_time_data, set_time_data);
    Stat_type_functions[TRACKED_STAT_MONEY] = stat_function_pointers(update_float_value, check_float_requirement, get_money_string_value, reset_float_value, get_float_data, set_float_data);
    Stat_type_functions[TRACKED_STAT_BOOL] = stat_function_pointers(update_bool_value, check_bool_requirement, get_bool_string_value, reset_bool_value, get_bool_data, set_bool_data);
    Stat_type_functions[TRACKED_STAT_PERCENT] = stat_function_pointers(update_percent_value, check_percent_requirement, get_percent_string_value, reset_percent_value, get_percent_data, set_percent_data);
    Stat_type_functions[TRACKED_STAT_PERCENT_COMPLETE] = stat_function_pointers(update_percent_complete_value, check_int_requirement, get_percent_complete_string_value, reset_percent_complete_value, nullptr, nullptr);
    Stat_type_functions[TRACKED_STAT_MISSIONS_COMPLETED] = stat_function_pointers(update_missions_completed_value, check_int_requirement, get_missions_completed_string_value, reset_missions_completed_value, nullptr, nullptr);
    for (int i = 0; i < Num_stats; i++)
    {
        if (Stat_type_functions[Stats[i].value_type].reset_value_fn != nullptr)
        {
            Stat_type_functions[Stats[i].value_type].reset_value_fn(&Stats[i]);
        }
    }
    set_best_and_worst_weapons();
    stats_reset_all_achievements(); // inlined in the binary
    stats_serialize_in_profile_stats();
    Stats_system_initialized = true;
}

// 0x007E5A90 (:3167)
// Single player: every stat's achievements and hidden challenges checked.
void stats_level_init(bool unlock_cheats_only)
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return;
    }
    for (int s = 0; s < STATS_NUM_STATS; s++)
    {
        for (int i = 0; i < Stats[s].num_achievement_refs; i++)
        {
            check_achievement(Stats[s].achievement_refs[i], unlock_cheats_only);
        }
        // Original: Hidden_challenges[i], not the stat's references.
        for (int i = 0; i < Stats[s].num_hidden_challenge_refs; i++)
        {
            check_hidden_challenge(&Hidden_challenges[i], unlock_cheats_only);
        }
    }
}

// 0x007D4AB0 (:3199)
void stats_level_shutdown()
{
    Achievements_enabled = true;
}

// 0x007C7870 (:3219)
// The display texts of every stat and achievement looked up again (after a language switch).
void stats_reload_display_texts()
{
    for (int i = 0; i < STATS_NUM_STATS; i++)
    {
        Stats[i].display_name = localization_get_display_string_req(Stats[i].display_name_loca_tag, nullptr);
    }
    for (int i = 0; i < 64; i++)
    {
        Achievements[i].display_name = localization_get_display_string_req(Achievements[i].display_name_loca_tag, nullptr);
        Achievements[i].display_desc = localization_get_display_string_req(Achievements[i].display_desc_loca_tag, nullptr);
    }
}

// 0x007D4AC0 (:3239)
// The stat updated by its type's function, then its achievements and hidden challenges checked.
void stats_increment(tracked_stats_indices stat, const void* delta_ptr)
{
    if (!stats_can_change() || stat == STATS_INVALID)
    {
        return;
    }
    if (Stat_type_functions[Stats[stat].value_type].update_value_fn == nullptr || stat >= STATS_NUM_STATS)
    {
        return;
    }
    Stat_type_functions[Stats[stat].value_type].update_value_fn(&Stats[stat], delta_ptr);
    for (int i = 0; i < Stats[stat].num_achievement_refs; i++)
    {
        check_achievement(Stats[stat].achievement_refs[i], false);
    }
    for (int i = 0; i < Stats[stat].num_hidden_challenge_refs; i++)
    {
        check_hidden_challenge(Stats[stat].hidden_challenge_refs[i], true);
    }
}

// 0x007D4B90 (:3278)
void stats_increment_by_one(tracked_stats_indices stat)
{
    if (stats_can_change())
    {
        int delta = 1;
        stats_increment(stat, &delta);
    }
}

// 0x007D4BD0 (:3292)
void stats_increment_by_one_fl(tracked_stats_indices stat)
{
    if (stats_can_change())
    {
        float delta = 1.0f;
        stats_increment(stat, &delta);
    }
}

// 0x007D4C10 (:3307)
// An integer stat raised to new_best.
void stats_update_best(tracked_stats_indices stat, int new_best)
{
    if (!stats_can_change() || stat == STATS_INVALID || stat >= STATS_NUM_STATS)
    {
        return;
    }
    int current;
    switch (Stats[stat].value_type)
    {
    case TRACKED_STAT_INT8:
        current = Stats[stat].value.integer8;
        break;
    case TRACKED_STAT_INT16:
        current = Stats[stat].value.integer16;
        break;
    case TRACKED_STAT_INT:
        current = Stats[stat].value.integer;
        break;
    default:
        return;
    }
    int delta = new_best - current;
    if (delta > 0)
    {
        stats_increment(stat, &delta);
    }
}

// 0x007EA0F0 (:3347)
// A float stat raised to new_best; a time stat is a best time: lowered to it, or set when it has none yet (0 or the
// initial value).
void stats_update_best_fl(tracked_stats_indices stat, float new_best)
{
    if (!stats_can_change() || stat == STATS_INVALID || stat >= STATS_NUM_STATS)
    {
        return;
    }
    if ((unsigned int)(Stats[stat].value_type - TRACKED_STAT_FLOAT) >= 4)
    {
        return;
    }
    const float current = Stats[stat].value.floating;
    float delta = new_best - current;
    if (Stats[stat].value_type == TRACKED_STAT_TIME)
    {
        // Magic number: 2e9, INITIAL_TIME_VALUE as a double.
        if (current == 0.0f || (double)current == 2000000000.0)
        {
            stats_set_stat_fl(stat, new_best);
            return;
        }
        if (delta < 0.0f)
        {
            stats_increment(stat, &delta);
        }
    }
    else if (0.0f < delta)
    {
        stats_increment(stat, &delta);
    }
}

// 0x007C78E0 (:3397)
int stats_get_num_stats()
{
    return Num_stats;
}

// 0x007C78F0 (:3404)
tracked_stats_indices stats_get_by_name(const char* name)
{
    if (name == nullptr)
    {
        return STATS_INVALID;
    }
    const int index = stats_find_index_by_name(name); // inlined in the binary
    return index < 0 ? STATS_INVALID : (tracked_stats_indices)index;
}

// 0x007C7950 (:3426)
bool stats_get_stat(const tracked_stat*& tsp, tracked_stats_indices index)
{
    if ((unsigned int)index < STATS_NUM_STATS)
    {
        tsp = &Stats[index];
        return true;
    }
    return false;
}

// 0x007E5B10 (:3436)
// An integer stat set (other types are left), then its achievements and hidden challenges checked.
void stats_set_stat(tracked_stats_indices stat, int value)
{
    if (!stats_can_change() || (unsigned int)stat >= STATS_NUM_STATS)
    {
        return;
    }
    switch (Stats[stat].value_type)
    {
    case TRACKED_STAT_INT8:
        Stats[stat].value.integer8 = (signed char)value;
        break;
    case TRACKED_STAT_INT16:
        Stats[stat].value.integer16 = (short)value;
        break;
    case TRACKED_STAT_INT:
    case TRACKED_STAT_WRECKING_CREW_MAP_MODE_COMBINATIONS_PLAYED:
        Stats[stat].value.integer = value;
        break;
    default:
        break;
    }
    for (int i = 0; i < Stats[stat].num_achievement_refs; i++)
    {
        check_achievement(Stats[stat].achievement_refs[i], false);
    }
    // Original: Hidden_challenges[i], not the stat's references.
    for (int i = 0; i < Stats[stat].num_hidden_challenge_refs; i++)
    {
        check_hidden_challenge(&Hidden_challenges[i], true);
    }
}

// 0x007E5C10 (:3482)
void stats_set_stat_fl(tracked_stats_indices stat, float value)
{
    if (!stats_can_change() || (unsigned int)stat >= STATS_NUM_STATS || (unsigned int)(Stats[stat].value_type - TRACKED_STAT_FLOAT) >= 4)
    {
        return;
    }
    Stats[stat].value.floating = value;
    for (int i = 0; i < Stats[stat].num_achievement_refs; i++)
    {
        check_achievement(Stats[stat].achievement_refs[i], false);
    }
    // Original: Hidden_challenges[i], not the stat's references.
    for (int i = 0; i < Stats[stat].num_hidden_challenge_refs; i++)
    {
        check_hidden_challenge(&Hidden_challenges[i], true);
    }
}

// 0x007D4CA0 (:3521)
int stats_get_stat_value(tracked_stats_indices index)
{
    return stats_get_stat_value(stats_get_stat_ptr(index)); // inlined in the binary
}

// 0x007C7980 (:3529)
// The value as an integer (floats truncated; 0 for the others).
int stats_get_stat_value(const tracked_stat* tsp)
{
    if (tsp == nullptr)
    {
        return 0;
    }
    switch (tsp->value_type)
    {
    case TRACKED_STAT_INT8:
        return tsp->value.integer8;
    case TRACKED_STAT_INT16:
        return tsp->value.integer16;
    case TRACKED_STAT_INT:
    case TRACKED_STAT_PERCENT_COMPLETE:
    case TRACKED_STAT_MISSIONS_COMPLETED:
        return tsp->value.integer;
    case TRACKED_STAT_FLOAT:
    case TRACKED_STAT_DISTANCE:
    case TRACKED_STAT_TIME:
    case TRACKED_STAT_MONEY:
    case TRACKED_STAT_PERCENT:
        return (int)tsp->value.floating;
    default:
        return 0;
    }
}

// 0x007D4D20 (:3561)
float stats_get_stat_value_fl(tracked_stats_indices index)
{
    return stats_get_stat_value_fl(stats_get_stat_ptr(index));
}

// 0x007C79E0 (:3569)
float stats_get_stat_value_fl(const tracked_stat* stat)
{
    if (stat == nullptr)
    {
        return 0.0f;
    }
    switch (stat->value_type)
    {
    case TRACKED_STAT_INT8:
        return (float)stat->value.integer8;
    case TRACKED_STAT_INT16:
        return (float)stat->value.integer16;
    case TRACKED_STAT_INT:
    case TRACKED_STAT_PERCENT_COMPLETE:
    case TRACKED_STAT_MISSIONS_COMPLETED:
        return (float)stat->value.integer;
    case TRACKED_STAT_FLOAT:
    case TRACKED_STAT_DISTANCE:
    case TRACKED_STAT_TIME:
    case TRACKED_STAT_MONEY:
    case TRACKED_STAT_PERCENT:
        return stat->value.floating;
    default:
        return 0.0f;
    }
}

// 0x007D4D50 (:3602)
// Original: no null check for an index past the table.
bool stats_stat_is_int_value(tracked_stats_indices index)
{
    return stats_stat_is_int_value(stats_get_stat_ptr(index)); // inlined in the binary
}

// 0x007C7A80 (:3610)
bool stats_stat_is_int_value(const tracked_stat* tsp)
{
    switch (tsp->value_type)
    {
    case TRACKED_STAT_INT8:
    case TRACKED_STAT_INT16:
    case TRACKED_STAT_INT:
    case TRACKED_STAT_BOOL:
    case TRACKED_STAT_PERCENT_COMPLETE:
    case TRACKED_STAT_MISSIONS_COMPLETED:
    case TRACKED_STAT_WRECKING_CREW_MAP_MODE_COMBINATIONS_PLAYED:
    case TRACKED_STAT_WRECKING_CREW_MODES_PLAYED:
        return true;
    default:
        return false;
    }
}

// 0x007C7AC0 (:3629)
// The requirement as an integer (a bool requirement: 1 for at least).
int stats_get_requirement_value(const achievement_requirement* req)
{
    switch (req->stat->value_type)
    {
    case TRACKED_STAT_INT8:
        return req->requirement.i8_value;
    case TRACKED_STAT_INT16:
        return req->requirement.i16_value;
    case TRACKED_STAT_INT:
    case TRACKED_STAT_PERCENT_COMPLETE:
    case TRACKED_STAT_MISSIONS_COMPLETED:
    case TRACKED_STAT_WRECKING_CREW_MAP_MODE_COMBINATIONS_PLAYED:
    case TRACKED_STAT_WRECKING_CREW_MODES_PLAYED:
        // Original: the integer requirement is read as a float and truncated.
        return (int)req->requirement.f_value;
    case TRACKED_STAT_BOOL:
        return req->condition == REQUIREMENT_CONDITION_AT_LEAST;
    default:
        VLIB_CRITICAL_FAILURE();
        return -1;
    }
}

// 0x007C7B40 (:3656)
float stats_get_requirement_value_fl(const achievement_requirement* req)
{
    const tracked_stat_type type = req->stat->value_type;
    if (type > TRACKED_STAT_INT && (type < TRACKED_STAT_BOOL || type == TRACKED_STAT_PERCENT))
    {
        return req->requirement.f_value;
    }
    VLIB_CRITICAL_FAILURE();
    return -1.0f;
}

// 0x007C7B80 (:3678)
int stats_get_num_achievements()
{
    return Num_achievements;
}

// 0x007C7B90 (:3688)
const achievement* stats_get_achievement(int index)
{
    if (index > -1 && index < Num_achievements)
    {
        return &Achievements[index];
    }
    return nullptr;
}

// 0x007C7BB0 (:3699)
achievement* stats_get_achievement_by_id(int id)
{
    const int index = get_achievement_index(id); // inlined in the binary
    if (index < 0 || index >= Num_achievements)
    {
        return nullptr;
    }
    return &Achievements[index];
}

// 0x007C7BF0 (:3713)
// The achievement's cheat unlocked (with a HUD message) and the game completion flags of the profile; awarded through
// the platform when signed in (unless only cheats are unlocked or achievements are off).
void stats_award_achievement(achievement* ap, bool unlock_cheats_only)
{
    if (!stats_can_change())
    {
        return;
    }
    if (ap->unlocks_cheat && !ap->has_unlocked_cheat)
    {
        cheats_menu_unlock_cheat_for_achievement((steam_achievement_ids)ap->achievement_id);
        // Magic number: 5 seconds on screen.
        hud_message_add_minor(localization_get_display_string_req("HM_CHEAT_UNLOCKED", nullptr), 5.0f, true);
    }
    // Magic numbers: achievement 8 completes the game, 0x35 the DLC campaign.
    if (ap->achievement_id == 8)
    {
        Last_profile_data_from_save_data.game_completed = true;
        Last_profile_data_from_save_data.wc_maps_unlocked = true;
        Last_profile_data_from_save_data.golden_hammer_unlocked = true;
    }
    else if (ap->achievement_id == 0x35)
    {
        Last_profile_data_from_save_data.new_data.game_completed_dlc = true;
    }
    ap->has_unlocked_cheat = true;
    if (!ap->has_been_achieved && !unlock_cheats_only && Achievements_enabled)
    {
        ap->has_been_achieved = true;
        if (multi_core_platform_is_signed_into_profile(Player_controller_index != 0xFFFFFFFFu ? (int)Player_controller_index : 0))
        {
            multi_core_platform_award_achievement(ap->achievement_id);
        }
    }
}

// 0x007C7CF0 (:3772)
void stats_award_achievement(int achievement_id)
{
    const int index = get_achievement_index(achievement_id); // inlined in the binary
    if (index > -1)
    {
        stats_award_achievement(&Achievements[index], false);
    }
}

// 0x007C7D40 (:3788)
void stats_award_achievement_failed(int achievement_id)
{
    const int index = get_achievement_index(achievement_id); // inlined in the binary
    if (index > -1)
    {
        Achievements[index].has_been_achieved = false;
    }
}

// 0x007C7D80 (:3801)
// Awarded or its cheat unlocked; an unknown id is a critical failure.
bool stats_has_achievement_been_awarded(int achievement_id)
{
    const int index = get_achievement_index(achievement_id); // inlined in the binary
    if (index > -1)
    {
        return Achievements[index].has_been_achieved || Achievements[index].has_unlocked_cheat;
    }
    VLIB_CRITICAL_FAILURE();
    return false;
}

// 0x007C7DF0 (:3822)
void stats_get_string_value(char* string_value, int max_len, const tracked_stat& tsp)
{
    if (Stat_type_functions[tsp.value_type].get_string_value_fn != nullptr)
    {
        Stat_type_functions[tsp.value_type].get_string_value_fn(string_value, max_len, &tsp);
        return;
    }
    string_value[0] = '\0';
}

// 0x007C7E20 (:3837)
// The requirement's value as text (integer, short, float, time, percent).
void stats_get_req_string_value(char* string_value, int max_len, const achievement_requirement& req)
{
    if (req.stat == nullptr)
    {
        return;
    }
    switch (req.stat->value_type)
    {
    case TRACKED_STAT_INT8:
        _snprintf_safe(string_value, max_len, "%d", (int)req.requirement.i8_value);
        return;
    case TRACKED_STAT_INT16:
        _snprintf_safe(string_value, max_len, "%hd", (unsigned int)(unsigned short)req.requirement.i16_value);
        return;
    case TRACKED_STAT_INT:
    case TRACKED_STAT_PERCENT_COMPLETE:
    case TRACKED_STAT_MISSIONS_COMPLETED:
        _snprintf_safe(string_value, max_len, "%d", req.requirement.i_value);
        return;
    case TRACKED_STAT_FLOAT:
        _snprintf_safe(string_value, max_len, "%0.2f", (double)req.requirement.f_value);
        return;
    case TRACKED_STAT_TIME:
        get_time_string_value(string_value, max_len, (int)req.requirement.f_value);
        return;
    case TRACKED_STAT_PERCENT:
        _snprintf_safe(string_value, max_len, "%0.2f%%", (double)req.requirement.f_value);
        return;
    default:
        VLIB_CRITICAL_FAILURE();
        return;
    }
}

// 0x007C7F30 (:3875)
// The text cut at its first '.'.
static void remove_decimal(char* str)
{
    for (int i = 0; str[i] != '\0'; i++)
    {
        if (str[i] == '.')
        {
            str[i] = '\0';
            return;
        }
    }
}

// 0x007C7F50 (:3895)
// The achievement's progress (0-1) and its text for the achievements menu: "value/required" for one requirement (a
// localization tag for a single yes/no requirement), "met/total" for several; 0 when not signed in.
float stats_get_achievement_progress(char* progress_text, int achievement_id)
{
    const int index = get_achievement_index(achievement_id); // inlined in the binary
    // Original: Achievements[-1] for an unknown id.
    achievement* ap = &Achievements[index];
    if (ap->num_requirements == 0)
    {
        if (ap->has_been_achieved)
        {
            sprintf(progress_text, "MENU_ACHIEVEMENT_COMPLETE");
            return 1.0f;
        }
        sprintf(progress_text, "MENU_ACHIEVEMENT_INCOMPLETE");
        return 0.0f;
    }
    const bool signed_in = multi_core_platform_is_signed_into_profile(Player_controller_index != 0xFFFFFFFFu ? (int)Player_controller_index : 0);
    float progress;
    if (ap->num_requirements < 2)
    {
        const achievement_requirement* req = ap->requirements;
        const tracked_stat* tsp = req->stat;
        const bool is_int = stats_stat_is_int_value(tsp);
        int value = -1;
        int required = -1;
        if (is_int)
        {
            value = stats_get_stat_value(tsp);
            required = stats_get_requirement_value(req);
            progress = (float)value / (float)required;
        }
        else
        {
            const float value_fl = stats_get_stat_value_fl(tsp);
            progress = value_fl / stats_get_requirement_value_fl(req);
        }
        if (!signed_in)
        {
            value = 0;
            progress = 0.0f;
        }
        if (!is_int || required != 1)
        {
            char required_text[32];
            char value_text[32];
            // Magic number: 0x1E characters.
            stats_get_req_string_value(required_text, 0x1E, *req);
            remove_decimal(required_text);
            if (!signed_in)
            {
                sprintf(value_text, "0");
            }
            else
            {
                stats_get_string_value(value_text, 0x1E, *tsp);
                remove_decimal(value_text);
            }
            const char* first;
            const char* second;
            if (localization_get_score_reversed())
            {
                first = required_text;
                second = (!signed_in || !ap->has_been_achieved) ? value_text : required_text;
            }
            else
            {
                first = (!signed_in || !ap->has_been_achieved) ? value_text : required_text;
                second = required_text;
            }
            sprintf(progress_text, "%s/%s", first, second);
        }
        else if (!signed_in)
        {
            sprintf(progress_text, "MENU_ACHIEVEMENT_INCOMPLETE");
        }
        else if (!ap->has_been_achieved && (req->condition != REQUIREMENT_CONDITION_AT_LEAST || value != 1) && (req->condition != REQUIREMENT_CONDITION_AT_MOST || value != 0))
        {
            sprintf(progress_text, "MENU_ACHIEVEMENT_INCOMPLETE");
        }
        else
        {
            sprintf(progress_text, "MENU_ACHIEVEMENT_COMPLETE");
        }
    }
    else
    {
        int met = 0;
        if (signed_in)
        {
            if (ap->has_been_achieved)
            {
                met = ap->num_requirements;
            }
            else
            {
                for (int i = 0; i < ap->num_requirements; i++)
                {
                    const achievement_requirement& req = ap->requirements[i];
                    const stat_function_pointers& functions = Stat_type_functions[req.stat->value_type];
                    if (functions.check_req_fn != nullptr && functions.check_req_fn(req.stat, &req))
                    {
                        met++;
                    }
                }
            }
        }
        if (localization_get_score_reversed())
        {
            sprintf(progress_text, "%i/%i", ap->num_requirements, met);
        }
        else
        {
            sprintf(progress_text, "%i/%i", met, ap->num_requirements);
        }
        progress = signed_in ? (float)met / (float)ap->num_requirements : 0.0f;
    }
    // Magic number: 1.0, the progress is capped.
    if (!((double)progress - 1.0 < 0.0))
    {
        progress = 1.0f;
    }
    return progress;
}

// 0x007C8330 (:4041)
int stats_get_num_hidden_challenges()
{
    return Num_hidden_challenges;
}

// 0x007C8340 (:4046)
const hidden_challenge* stats_get_hidden_challenge(int index)
{
    if (index > -1 && index < Num_hidden_challenges)
    {
        return &Hidden_challenges[index];
    }
    return nullptr;
}

// 0x007C8360 (:4052)
const hidden_challenge* stats_get_hidden_challenge_by_display_name(const char* display_name)
{
    for (int i = 0; i < Num_hidden_challenges; i++)
    {
        if (v_stricmp(Hidden_challenges[i].display_name, display_name) == 0)
        {
            return &Hidden_challenges[i];
        }
    }
    return nullptr;
}

// 0x007C83B0 (:4062)
// The stat with the highest value (the first of equals).
const tracked_stat* stats_get_hidden_challenge_most_complete_stat(const hidden_challenge* hcp)
{
    if (hcp == nullptr || hcp->num_stats < 1)
    {
        return nullptr;
    }
    const tracked_stat* best = hcp->stats[0];
    float best_value = stats_get_stat_value_fl(best); // inlined in the binary
    for (int i = 1; i < hcp->num_stats; i++)
    {
        const float value = stats_get_stat_value_fl(hcp->stats[i]); // inlined in the binary
        if (best_value < value)
        {
            best = hcp->stats[i];
            best_value = value;
        }
    }
    return best;
}

// 0x007C84F0 (:4125)
// The stats at or above the required value (within 0.001).
int stats_get_hidden_challenge_num_complete_stats(const hidden_challenge* hcp)
{
    if (hcp == nullptr || hcp->num_stats <= 0)
    {
        return 0;
    }
    int complete = 0;
    for (int i = 0; i < hcp->num_stats; i++)
    {
        // Magic number: 0.001, the comparison tolerance.
        if (fl_greater_than_equal_robust(stats_get_stat_value_fl(hcp->stats[i]), hcp->required_value, 0.001f))
        {
            complete++;
        }
    }
    return complete;
}

// 0x007EA1E0 (:4167)
// The profile stats (those not stored in the save game) from the profile data, length-prefixed in table order; a
// short or bad buffer resets every stat (with the corrupt message when there was data). Then the achievements.
void stats_serialize_in_profile_stats()
{
    stats_reset_profile_stats();
    Stats_serialize_cs.enter();
    player_profile_data& profile = Last_profile_data_from_save_data;
    unsigned int offset = 0;
    int i = 0;
    for (; i < STATS_NUM_STATS; i++)
    {
        tracked_stat& stat = Stats[i];
        if (stat.name == nullptr || stat.flags.is_stored_in_save_game)
        {
            continue;
        }
        const stat_function_pointers& functions = Stat_type_functions[stat.value_type];
        if (functions.set_stat_data_fn == nullptr)
        {
            // Original: an update with a null delta (the percent complete and missions completed updates ignore it).
            if (functions.update_value_fn != nullptr)
            {
                functions.update_value_fn(&stat, nullptr);
            }
            continue;
        }
        if (profile.stats_buffer_size == 0 || profile.stats_buffer_size <= offset)
        {
            break;
        }
        const unsigned int next = profile.stats_buffer[offset] + 1 + offset;
        if (profile.stats_buffer_size < next || !functions.set_stat_data_fn(&stat, profile.stats_buffer + offset + 1, profile.stats_buffer[offset]))
        {
            break;
        }
        offset = next;
    }
    if (i != STATS_NUM_STATS)
    {
        if (profile.stats_buffer_size != 0)
        {
            Stats_should_open_reset_message = true;
        }
        stats_reset_specific(true, true);
    }
    update_achievements();
    Stats_serialize_cs.leave();
}

// 0x007C85E0 (:4269)
// The profile stats into the profile data, each as its length byte and data (false when they do not fit).
bool stats_serialize_out_profile_stats()
{
    Stats_serialize_cs.enter();
    player_profile_data& profile = Last_profile_data_from_save_data;
    void* data = nullptr;
    int length;
    profile.stats_buffer_size = 0;
    for (int i = 0; i < STATS_NUM_STATS; i++)
    {
        const tracked_stat& stat = Stats[i];
        if (stat.name == nullptr || stat.flags.is_stored_in_save_game || Stat_type_functions[stat.value_type].get_stat_data_fn == nullptr)
        {
            continue;
        }
        Stat_type_functions[stat.value_type].get_stat_data_fn(data, length, &stat);
        // Magic number: 0x100, a value's length must fit its length byte.
        if (length >= 0x100)
        {
            continue;
        }
        if (profile.stats_buffer_size + 1 + length > (int)sizeof(profile.stats_buffer))
        {
            keen::debug::nativePrintArgs("Ran out of space trying to save stats to the gamer profile.");
            Stats_serialize_cs.leave();
            return false;
        }
        profile.stats_buffer[profile.stats_buffer_size] = (unsigned char)length;
        profile.stats_buffer_size++;
        memcpy(profile.stats_buffer + profile.stats_buffer_size, data, length);
        profile.stats_buffer_size = (unsigned short)(profile.stats_buffer_size + length);
    }
    Stats_serialize_cs.leave();
    return true;
}

// 0x007C86F0 (:4356)
// The save game stats as text, one null-terminated string each.
bool stats_serialize_out_save_game_stats(cfile* save_game)
{
    Stats_serialize_cs.enter();
    for (int i = 0; i < STATS_NUM_STATS; i++)
    {
        const tracked_stat& stat = Stats[i];
        if (stat.name != nullptr && stat.flags.is_stored_in_save_game)
        {
            char text[STATS_STRING_SIZE];
            Stat_type_functions[stat.value_type].get_string_value_fn(text, STATS_STRING_SIZE, &stat);
            cf_write_string_null_term(text, save_game);
        }
    }
    Stats_serialize_cs.leave();
    return true;
}

// 0x007C8780 (:4395)
// The save game stats from their text (times as [hh:]mm:ss).
void stats_serialize_in_save_game_stats(cfile* save_game)
{
    Stats_serialize_cs.enter();
    for (int i = 0; i < STATS_NUM_STATS; i++)
    {
        tracked_stat& stat = Stats[i];
        if (stat.name == nullptr || !stat.flags.is_stored_in_save_game)
        {
            continue;
        }
        char text[STATS_STRING_SIZE];
        cf_read_string_null_term(text, STATS_STRING_SIZE, save_game);
        signed char value8;
        unsigned int value16;
        int value_int;
        float value_float;
        void* data;
        int length = 4;
        switch (stat.value_type)
        {
        case TRACKED_STAT_INT8:
            value8 = (signed char)atoi(text);
            data = &value8;
            length = 1;
            break;
        case TRACKED_STAT_INT16:
            value16 = (unsigned int)atoi(text) & 0xFFFF;
            data = &value16;
            length = 2;
            break;
        case TRACKED_STAT_INT:
        case TRACKED_STAT_PERCENT_COMPLETE:
        case TRACKED_STAT_MISSIONS_COMPLETED:
        case TRACKED_STAT_WRECKING_CREW_MAP_MODE_COMBINATIONS_PLAYED:
        case TRACKED_STAT_WRECKING_CREW_MODES_PLAYED:
            value_int = atoi(text);
            data = &value_int;
            break;
        case TRACKED_STAT_FLOAT:
        case TRACKED_STAT_DISTANCE:
        case TRACKED_STAT_MONEY:
        case TRACKED_STAT_PERCENT:
            value_float = (float)atof(text);
            data = &value_float;
            break;
        case TRACKED_STAT_TIME:
        {
            const char* first = strtok(text, ":");
            const char* second = strtok(nullptr, ":");
            const char* third = strtok(nullptr, ":");
            // Magic number: 60 seconds a minute, minutes an hour.
            int seconds = atoi(first) * 60;
            int rest = atoi(second);
            if (third != nullptr)
            {
                seconds = (rest + seconds) * 60;
                rest = atoi(third);
            }
            value_float = (float)(rest + seconds);
            data = &value_float;
            break;
        }
        case TRACKED_STAT_BOOL:
        {
            // Original: read and dropped (never set).
            const bool flagged = strcmp(text, "true") == 0;
            (void)flagged;
            continue;
        }
        default:
            VLIB_CRITICAL_FAILURE();
            continue;
        }
        // Original: no null check (the percent complete, missions completed and Wrecking Crew types have none).
        Stat_type_functions[stat.value_type].set_stat_data_fn(&stat, data, length);
    }
    Stats_serialize_cs.leave();
}

// 0x007C89B0 (:4520)
// Neither achieved nor cheat unlocked.
void stats_reset_all_achievements()
{
    for (int i = 0; i < Num_achievements; i++)
    {
        Achievements[i].has_been_achieved = false;
        Achievements[i].has_unlocked_cheat = false;
    }
}

// 0x007E5CC0 (:4528)
// The save game and/or profile stats reset; the best time stats get their initial value and the save becomes owned.
static void stats_reset_specific(bool reset_save_game_stats, bool reset_profile_stats)
{
    if (!Stats_system_initialized)
    {
        return;
    }
    for (int i = 0; i < STATS_NUM_STATS; i++)
    {
        tracked_stat& stat = Stats[i];
        if (stat.name == nullptr || Stat_type_functions[stat.value_type].reset_value_fn == nullptr)
        {
            continue;
        }
        if ((reset_save_game_stats && stat.flags.is_stored_in_save_game) || (reset_profile_stats && !stat.flags.is_stored_in_save_game))
        {
            Stat_type_functions[stat.value_type].reset_value_fn(&stat);
        }
    }
    if (reset_save_game_stats)
    {
        for (int stat = STATS_DEMO_MASTER_TIME_1; stat <= STATS_DEMO_MASTER_TIME_17; stat++)
        {
            stats_set_stat_fl((tracked_stats_indices)stat, INITIAL_TIME_VALUE);
        }
        for (int stat = STATS_GUERRILLA_EXPRESS_TIME_1; stat <= STATS_GUERRILLA_EXPRESS_TIME_19; stat++)
        {
            stats_set_stat_fl((tracked_stats_indices)stat, INITIAL_TIME_VALUE);
        }
        Savegame_owned = true;
    }
}

// 0x007E5D80 (:4573)
void stats_reset_all_stats()
{
    stats_reset_specific(true, true);
}

// 0x007C89D0 (:4578)
void stats_reset(tracked_stats_indices stat)
{
    if (!stats_can_change() || stat == STATS_INVALID)
    {
        return;
    }
    if (Stat_type_functions[Stats[stat].value_type].reset_value_fn != nullptr && stat < STATS_NUM_STATS)
    {
        Stat_type_functions[Stats[stat].value_type].reset_value_fn(&Stats[stat]);
    }
}

// 0x007E5D90 (:4600)
void stats_reset_profile_stats()
{
    if (!Stats_system_initialized)
    {
        return;
    }
    for (int i = 0; i < STATS_NUM_STATS; i++)
    {
        tracked_stat& stat = Stats[i];
        if (stat.name != nullptr && Stat_type_functions[stat.value_type].reset_value_fn != nullptr && !stat.flags.is_stored_in_save_game)
        {
            Stat_type_functions[stat.value_type].reset_value_fn(&stat);
        }
    }
}

// 0x007E5DE0 (:4607)
void stats_reset_save_game_stats()
{
    stats_reset_specific(true, false);
}

// 0x007C8A30 (:4614)
// The corrupt stats message, once no cutscene is playing.
void stats_maybe_open_reset_message()
{
    if (Stats_should_open_reset_message && !cutscene_is_playing() && !cutscene_is_finishing())
    {
        ui_add_msgbox(MBT_OK, "MENU_MBOX_STATS_CORRUPT", "MENU_STATS_CORRUPT", nullptr, false, false, nullptr, nullptr, false);
        Stats_should_open_reset_message = false;
    }
}

// 0x007C8A80 (:4626)
void stats_enable_achievements(bool enable)
{
    Achievements_enabled = enable;
}

// 0x007E5DF0 (:4812)
// The completed activities of the world: house arrests, riding shotgun, deliveries, demolitions masters, convoys,
// couriers, area defenses and the raids that are not part of another raid.
void stats_count_completed_activities()
{
    int completed = 0;
    for (int i = 0; i < World.get_num_objects<object_house_arrest_node>(); i++)
    {
        if (World.get_object<object_house_arrest_node>(i)->get_completed_flag())
        {
            completed++;
        }
    }
    for (int i = 0; i < World.get_num_objects<object_riding_shotgun_node>(); i++)
    {
        if (World.get_object<object_riding_shotgun_node>(i)->is_completed())
        {
            completed++;
        }
    }
    for (int i = 0; i < World.get_num_objects<object_delivery_node>(); i++)
    {
        if (World.get_object<object_delivery_node>(i)->is_completed())
        {
            completed++;
        }
    }
    for (int i = 0; i < World.get_num_objects<object_demolitions_master_node>(); i++)
    {
        // The level data's flag directly (no null check, unlike is_completed).
        if (World.get_object<object_demolitions_master_node>(i)->get_demolitions_master_data()->dm_stats.completed)
        {
            completed++;
        }
    }
    for (int i = 0; i < World.get_num_objects<object_convoy_end_point>(); i++)
    {
        if (World.get_object<object_convoy_end_point>(i)->get_completed_flag())
        {
            completed++;
        }
    }
    for (int i = 0; i < World.get_num_objects<object_courier_end_point>(); i++)
    {
        if (World.get_object<object_courier_end_point>(i)->get_completed_flag())
        {
            completed++;
        }
    }
    for (int i = 0; i < World.get_num_objects<object_area_defense_node>(); i++)
    {
        if (World.get_object<object_area_defense_node>(i)->is_completed())
        {
            completed++;
        }
    }
    for (int i = 0; i < World.get_num_objects<object_raid_node>(); i++)
    {
        const object_raid_node* raid = World.get_object<object_raid_node>(i);
        const unsigned int parent = raid->get_parent();
        object* parent_objp = (parent == 0 || parent == 0xFFFFFFFFu) ? nullptr : World.object_from_handle(parent);
        if ((parent_objp == nullptr || parent_objp->get_obj_type() != OT_RAID_NODE) && raid->get_completed_flag())
        {
            completed++;
        }
    }
    stats_set_stat(STATS_ACTIVITIES_COMPLETED, completed);
}

// 0x007E6070 (:5060)
void stats_district_liberated(const char* dist_name)
{
    if (strcmp(dist_name, "Parker") == 0)
    {
        stats_set_stat(STATS_RF_CONTROLS_PARKER, 1);
    }
    else if (strcmp(dist_name, "Dust") == 0)
    {
        stats_set_stat(STATS_RF_CONTROLS_DUST, 1);
    }
    else if (strcmp(dist_name, "Badlands") == 0)
    {
        stats_set_stat(STATS_RF_CONTROLS_BADLANDS, 1);
    }
    else if (strcmp(dist_name, "Oasis") == 0)
    {
        stats_set_stat(STATS_RF_CONTROLS_OASIS, 1);
    }
    else if (strcmp(dist_name, "Eos") == 0)
    {
        stats_set_stat(STATS_RF_CONTROLS_EOS, 1);
    }
}

// 0x007EA2E0 (:5078)
// The chapter's best score (je_rs_01-07), then the total of the seven.
void stats_update_riding_shotgun_score(const char* chapter_name, float score)
{
    if (chapter_name == nullptr)
    {
        return;
    }
    static const char* const chapters[7] = {"je_rs_01", "je_rs_02", "je_rs_03", "je_rs_04", "je_rs_05", "je_rs_06", "je_rs_07"};
    int chapter = 0;
    while (chapter < 7 && strcmp(chapter_name, chapters[chapter]) != 0)
    {
        chapter++;
    }
    if (chapter < 7)
    {
        stats_update_best_fl((tracked_stats_indices)(STATS_RIDING_SHOTGUN_SCORE_1 + chapter), score);
    }
    else
    {
        VLIB_CRITICAL_FAILURE();
    }
    float total = (float)((double)Stats[STATS_RIDING_SHOTGUN_SCORE_1].value.floating + 0.0);
    for (int i = STATS_RIDING_SHOTGUN_SCORE_2; i <= STATS_RIDING_SHOTGUN_SCORE_7; i++)
    {
        total += Stats[i].value.floating;
    }
    stats_update_best_fl(STATS_RIDING_SHOTGUN_TOTAL_SCORE, total);
}

// 0x007EA5E0 (:5129)
// The activity's best time in seconds (td_01-19 in its name).
void stats_update_guerrilla_express_time(const char* activity_name, float time_in_milliseconds)
{
    // Magic number: 1000 ms a second.
    const float new_best = (float)((double)time_in_milliseconds / 1000.0);
    char tag[8];
    for (int i = 0; i < 19; i++)
    {
        sprintf(tag, "td_%02d", i + 1);
        if (strstr(activity_name, tag) != nullptr)
        {
            stats_update_best_fl((tracked_stats_indices)(STATS_GUERRILLA_EXPRESS_TIME_1 + i), new_best);
            return;
        }
    }
    VLIB_CRITICAL_FAILURE();
}

// 0x007EA920 (:5182)
// The activity's best time in whole seconds (dm_01-17 in its name).
void stats_update_demolitions_master(const char* activity_name, float time_in_milliseconds)
{
    // Magic number: 1000 ms a second; rounded down (in doubles).
    const float new_best = (float)floor((double)(float)((double)time_in_milliseconds / 1000.0));
    char tag[8];
    for (int i = 0; i < 17; i++)
    {
        sprintf(tag, "dm_%02d", i + 1);
        if (strstr(activity_name, tag) != nullptr)
        {
            stats_update_best_fl((tracked_stats_indices)(STATS_DEMO_MASTER_TIME_1 + i), new_best);
            return;
        }
    }
    VLIB_CRITICAL_FAILURE();
}

// 0x007E61C0 (:5251)
// A district at full morale: added to the max morale districts (bitfield and count) while achievement 0x16 is open.
void stats_update_district_morale(int district_index, float morale_pct)
{
    if (morale_pct < 1.0f)
    {
        return;
    }
    // Magic number: achievement 0x16, every district at maximum morale.
    const int index = get_achievement_index(0x16); // inlined in the binary
    if (index < 0 || index >= Num_achievements || Achievements[index].has_been_achieved)
    {
        return;
    }
    unsigned int districts = (unsigned int)stats_get_stat_value(&Stats[STAT_MAX_MORALE_DISTRICTS_BITFIELD]);
    const unsigned int bit = 1u << (district_index & 0x1F);
    if ((districts & bit) != 0)
    {
        return;
    }
    districts |= bit;
    // Magic number: 15 district bits counted.
    int count = 0;
    for (unsigned int i = 0; i < 15; i++)
    {
        if ((districts & (1u << i)) != 0)
        {
            count++;
        }
    }
    stats_set_stat(STAT_NUM_MAX_MORALE_DISTRICTS, count);
    stats_set_stat(STAT_MAX_MORALE_DISTRICTS_BITFIELD, (int)districts);
}

// 0x007EAC70 (:5299)
const char* stats_get_best_weapon_name()
{
    set_best_and_worst_weapons();
    const weapon_info* info = get_weapon_info_from_stat((tracked_stats_indices)stats_get_stat_value(&Stats[STATS_SP_TOTAL_WEAPON_KILLS_MOST]));
    if (info != nullptr)
    {
        return info->weapon_inv_item_info->display_name;
    }
    return nullptr;
}

// 0x007EACF0 (:5311)
const char* stats_get_worst_weapon_name()
{
    set_best_and_worst_weapons();
    const weapon_info* info = get_weapon_info_from_stat((tracked_stats_indices)stats_get_stat_value(&Stats[STATS_SP_TOTAL_WEAPON_KILLS_LEAST]));
    if (info != nullptr)
    {
        return info->weapon_inv_item_info->display_name;
    }
    return nullptr;
}

// 0x007E6270 (:5323)
// Single player: the destruction cost total and the destroyed vehicles total (its growth goes to the hero stats).
void stats_update_destruction_totals()
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return;
    }
    const float buildings = stats_get_stat_value_fl(&Stats[STATS_DESTRUCTION_COST_BUILDINGS]);
    const float vehicles = stats_get_stat_value_fl(&Stats[STATS_DESTRUCTION_COST_VEHICLES]);
    const float people = stats_get_stat_value_fl(&Stats[STATS_DESTRUCTION_COST_PEOPLE]);
    stats_set_stat_fl(STATS_DESTRUCTION_COST_TOTAL, people + vehicles + buildings + (float)0.0);
    const int before = stats_get_stat_value(&Stats[STATS_VEHICLES_DESTROYED_TOTAL]);
    const int edf_wheeled = stats_get_stat_value(&Stats[STATS_VEHICLES_DESTROYED_EDF_WHEELED]);
    const int civilian_wheeled = stats_get_stat_value(&Stats[STATS_VEHICLES_DESTROYED_CIVILIAN_WHEELED]);
    const int civilian_walkers = stats_get_stat_value(&Stats[STATS_VEHICLES_DESTROYED_CIVILIAN_WALKERS]);
    const int tanks = stats_get_stat_value(&Stats[STATS_VEHICLES_DESTROYED_TANKS]);
    const int flyers = stats_get_stat_value(&Stats[STATS_VEHICLES_DESTROYED_FLYERS]);
    stats_set_stat(STATS_VEHICLES_DESTROYED_TOTAL, flyers + civilian_wheeled + edf_wheeled + civilian_walkers + tanks);
    const int after = stats_get_stat_value(&Stats[STATS_VEHICLES_DESTROYED_TOTAL]);
    if (after - before > 0)
    {
        unit4::LocalGameSession* session = rfg_get_local_game_session();
        if (session != nullptr)
        {
            // Magic number: hero stat 2, vehicles destroyed.
            session->updateHeroStatAdd(nullptr, 2, (double)(after - before));
        }
    }
}

// 0x007D4DA0 (:5357)
// Single player, for a building that was not destroyed before: a destroyed (damage -1) high or medium priority
// target outside riding shotgun; a destroyed player-attacked building (and EDF building), or its damage cost.
void stats_register_building_damage(const rfg_mover* objp, float damage_percent)
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return;
    }
    // Magic number: sub type 1 with a parent is a child piece.
    if (objp->get_sub_type() == 1 && objp->get_parent() != 0xFFFFFFFFu)
    {
        return;
    }
    if (World.object_was_destroyed(objp->get_handle()))
    {
        return;
    }
    // Magic number: -1, the damage percent of a destroyed building.
    if (!activity_vrail_is_riding_shotgun_running() && (double)damage_percent == -1.0)
    {
        if (objp->gameplay_props->importance == OBJECT_IMPORTANCE_LEVEL_HIGH)
        {
            stats_increment_by_one(STATS_HIGH_PRIORITY_TARGETS_DESTROYED);
        }
        if (objp->gameplay_props->importance == OBJECT_IMPORTANCE_LEVEL_MEDIUM)
        {
            stats_increment_by_one(STATS_MEDIUM_PRIORITY_TARGETS_DESTROYED);
        }
    }
    if (!activity_register_damage_is_player_controlled(objp))
    {
        return;
    }
    const human_teams team = human_teams_get_team(objp);
    if ((double)damage_percent == -1.0)
    {
        if (objp->flags.building)
        {
            stats_increment_by_one(STATS_BUILDINGS_DESTROYED);
            if (team == HUMAN_TEAM_EDF)
            {
                stats_increment_by_one(STATS_EDF_BUILDINGS_DESTROYED);
            }
            unit4::LocalGameSession* session = rfg_get_local_game_session();
            if (session != nullptr)
            {
                // Magic number: hero stat 1, buildings destroyed.
                session->updateHeroStatAdd(nullptr, 1, 1.0);
            }
        }
        return;
    }
    float cost = activity_register_damage_get_cost(objp) * damage_percent;
    if (team == HUMAN_TEAM_EDF)
    {
        stats_increment(STATS_EDF_PROPERTY_DAMAGE, &cost);
        stats_increment(STATS_DESTRUCTION_COST_BUILDINGS, &cost);
        return;
    }
    if (objp->flags.building)
    {
        stats_increment(STATS_DESTRUCTION_COST_BUILDINGS, &cost);
    }
}

// 0x007D4F10 (:5427)
// Single player: the player's and the EDF's deaths by cause (crushed when no attacker), unknown killers, and the
// victim's cost (and EDF damage when the player killed an EDF soldier).
void stats_register_human_death(const human* victim, const human* attacker, int damaged_by_type)
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return;
    }
    const bool victim_is_player = object_is_player(victim);
    const bool attacker_is_player = object_is_player(attacker);
    // Magic numbers: damage type 2 is an explosion, 3 a gun.
    if (attacker == nullptr)
    {
        if (victim_is_player)
        {
            stats_increment_by_one(STATS_PLAYER_KILLED_CRUSHED);
            stats_increment_by_one(STATS_PLAYER_KILLED_TOTAL);
            return;
        }
        if (victim->current_team == HUMAN_TEAM_EDF)
        {
            stats_increment_by_one(STATS_EDF_KILLED_CRUSHED);
            stats_increment_by_one(STATS_EDF_KILLED_TOTAL);
            unit4::LocalGameSession* session = rfg_get_local_game_session();
            if (session != nullptr)
            {
                // Magic number: hero stat 0, EDF killed.
                session->updateHeroStatAdd(nullptr, 0, 1.0);
            }
        }
        else
        {
            stats_increment_by_one(STAT_HUMANS_KILLED_BY_UNKNOWN);
        }
    }
    else
    {
        if (victim_is_player)
        {
            stats_increment_by_one(damaged_by_type == 2 ? STATS_PLAYER_KILLED_EXPLOSION : (damaged_by_type == 3 ? STATS_PLAYER_KILLED_GUN : STATS_PLAYER_KILLED_CRUSHED));
            stats_increment_by_one(STATS_PLAYER_KILLED_TOTAL);
            return;
        }
        if (victim->current_team == HUMAN_TEAM_EDF)
        {
            stats_increment_by_one(damaged_by_type == 2 ? STATS_EDF_KILLED_EXPLOSION : (damaged_by_type == 3 ? STATS_EDF_KILLED_GUN : STATS_EDF_KILLED_CRUSHED));
            stats_increment_by_one(STATS_EDF_KILLED_TOTAL);
            unit4::LocalGameSession* session = rfg_get_local_game_session();
            if (session != nullptr)
            {
                session->updateHeroStatAdd(nullptr, 0, 1.0);
            }
        }
    }
    float cost = activity_register_damage_get_cost(victim);
    stats_increment(STATS_DESTRUCTION_COST_PEOPLE, &cost);
    if (attacker_is_player && victim->current_team == HUMAN_TEAM_EDF)
    {
        stats_increment(STATS_EDF_PROPERTY_DAMAGE, &cost);
    }
}

// 0x007D5080 (:5503)
// Single player, an EDF vehicle damaged by the player (or a player-controlled one) above 750 hit points: the cost of
// the damage fraction (down to 750 left) as EDF property damage.
void stats_register_vehicle_damage(const object* objp, const object* attacker, int hitpoints_before_damage)
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return;
    }
    if (!object_is_local_player(attacker) && !activity_register_damage_is_player_controlled(objp))
    {
        return;
    }
    // Magic number: 750 (0x2EE) hit points, the damage below them does not count.
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE || ((const vehicle*)objp)->info->default_team != HUMAN_TEAM_EDF || hitpoints_before_damage <= 0x2ED)
    {
        return;
    }
    const unsigned int hit_points = vehicle_get_hit_points((const vehicle*)objp);
    if ((int)hit_points > 0x2EE)
    {
        hitpoints_before_damage -= (int)hit_points;
    }
    float cost = (float)hitpoints_before_damage;
    if (0.0f < cost)
    {
        const double damage = (double)hitpoints_before_damage;
        const unsigned int max_hit_points = vehicle_get_max_hit_points((const vehicle*)objp);
        const double fraction = (double)((float)damage / (float)max_hit_points);
        cost = activity_register_damage_get_cost(objp) * (float)fraction;
        stats_increment(STATS_EDF_PROPERTY_DAMAGE, &cost);
    }
}

// 0x007D5170 (:5549)
void stats_turn_off_all_cheats()
{
    for (int i = 0; i < NUM_CHEAT_INDEXES; i++)
    {
        Cheats_data[i] = false;
    }
    Cheats_enabled = false;
    Savegame_owned = true;
}

// 0x007C8A90 (:5560)
// Off in multiplayer (Wrecking Crew keeps them).
bool stats_cheat_enabled(cheat_indexes cheat_index)
{
    if ((Multiplayer || wrecking_crew_is_active()) && !wrecking_crew_is_active())
    {
        return false;
    }
    if ((int)cheat_index > 10)
    {
        return false;
    }
    return Cheats_data[cheat_index];
}

// 0x007EAD70 (:5572)
// Enabling a cheat marks cheats as used (for good: the stats freeze).
void stats_set_cheat(cheat_indexes cheat_index, bool enable)
{
    if ((int)cheat_index >= NUM_CHEAT_INDEXES)
    {
        return;
    }
    set_best_and_worst_weapons();
    if (Cheats_enabled || enable)
    {
        Cheats_enabled = true;
    }
    Cheats_data[cheat_index] = enable;
}

// 0x007C8AC0 (:5587)
void stats_set_savegame_owned(bool is_owned)
{
    Savegame_owned = is_owned;
}

// 0x007C8AD0 (:5601)
bool stats_get_savegame_owned()
{
    return Savegame_owned;
}

// 0x007C8AE0 (:5606)
void stats_system_shutdown()
{
    stats_shutdown_update_achievements_thread(); // inlined in the binary
}

// 0x007C8AF0 (:5611)
void stats_update_game_progress_hero_stat()
{
}
