// Original: volition/rfg/code/gameplay/gameflow/gfm_tech_level.cpp
//
// gameflow.md GF2. The tech level of the guerrillas and its maximum (kept by the world), the EDF and marauder progression
// levels, the weapons each tech level unlocks (tech_level.xtbl) and saving them.

#include "volition/rfg/code/gameplay/gameflow/gfm_tech_level.h"

#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/level/rfg_zonex_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"

#include <stdio.h>

extern int Marauder_progression_level; // 0x02229654 (squad_progression.cpp)
extern int EDF_locked_mission_progression_level; // 0x02229650 (squad_progression.cpp)
extern mission_handle Mission_active_index; // 0x0165150C (gameplay.cpp)
extern unsigned int District_map[257]; // 0x022267D8 (object_district.cpp)
unsigned int get_default_district_handle(); // 0x00774D40 (object_district.cpp)
int EDF_progression_level; // 0x0222964C

// Magic number: progression levels run from 0 to 7 (NUM_PROGRESSION_LEVELS is 8).
static const int MAX_PROGRESSION_LEVEL = 7;
// Magic number: tech_level.xtbl has one entry for each of the five weapons.
static const int NUM_TECH_LEVEL_WEAPONS = 5;
// Magic number: console_get_arg's type for a float (ints are 8).
static const unsigned int CONSOLE_ARG_FLOAT = 0x10;
static const unsigned int CONSOLE_ARG_INT = 8;

static tech_level_weapon Tech_level_weapons[NUM_TECH_LEVEL_WEAPONS]; // 0x022267A8

static int clamp_progression_level(int level)
{
    if (level < 0)
    {
        return 0;
    }
    return level > MAX_PROGRESSION_LEVEL ? MAX_PROGRESSION_LEVEL : level;
}

// 0x0079CE90 (:64)
// rfg.exe prints to a thread-local buffer; a stack buffer of the same use here.
static void gfm_tech_level_display()
{
    const int marauder = marauder_progression_level();
    const int edf = edf_progression_level();
    char text[256];
    sprintf(text, " Guerrilla Tech Level: %.1f \n EDF Tech Level: %i \n Marauder Tech Level: %i ", (double)gfm_get_tech_level(), edf, marauder);
    console_output(text, nullptr, false);
}

// <tech_level_weapon> 0x00766FC0 (:72), generic_swap in EAX and ECX
static void swap_tech_level_weapon(tech_level_weapon& a, tech_level_weapon& b)
{
    const tech_level_weapon temp = b;
    b = a;
    a = temp;
}

// 0x0079CF50 (:85)
void unused_dcf_global_progression_level()
{
    const bool help = Console_help != 0;
    if (Console_command != 0)
    {
        if (!console_get_arg(CONSOLE_ARG_FLOAT, 1))
        {
            console_output("Usage: global_progression_level <new value>", nullptr, false);
            gfm_tech_level_display();
            return;
        }
        const int level = (int)Console_arg_float;
        Marauder_progression_level = clamp_progression_level(level);
        EDF_progression_level = clamp_progression_level(level);
        World.set_tech_level(Console_arg_float);
    }
    if (help)
    {
        console_output("Usage: global_progression_level <new value>", nullptr, false);
    }
    gfm_tech_level_display();
}

// 0x0079D000 (:107)
void unused_dcf_edf_progression_level()
{
    const bool help = Console_help != 0;
    if (Console_command != 0)
    {
        if (!console_get_arg(CONSOLE_ARG_INT, 1))
        {
            console_output("Usage: edf_progression_level <new value>", nullptr, false);
            gfm_tech_level_display();
            return;
        }
        EDF_progression_level = clamp_progression_level(Console_arg_int);
    }
    if (help)
    {
        console_output("Usage: edf_progression_level <new value>", nullptr, false);
    }
    gfm_tech_level_display();
}

// 0x0079D070 (:129)
void unused_dcf_marauder_progression_level()
{
    const bool help = Console_help != 0;
    if (Console_command != 0)
    {
        if (!console_get_arg(CONSOLE_ARG_INT, 1))
        {
            console_output("Usage: marauder_progression_level <new value>", nullptr, false);
            gfm_tech_level_display();
            return;
        }
        Marauder_progression_level = clamp_progression_level(Console_arg_int);
    }
    if (help)
    {
        console_output("Usage: marauder_progression_level <new value>", nullptr, false);
    }
    gfm_tech_level_display();
}

// 0x0079D0E0 (:152)
void unused_dcf_tech_level_set()
{
    const bool help = Console_help != 0;
    if (Console_command != 0)
    {
        if (!console_get_arg(CONSOLE_ARG_FLOAT, 1))
        {
            console_output("Usage: tech_level_set <new value>", nullptr, false);
            gfm_tech_level_display();
            return;
        }
        World.set_tech_level(Console_arg_float);
    }
    if (help)
    {
        console_output("Usage: tech_level_set <new value>", nullptr, false);
    }
    gfm_tech_level_display();
}

// 0x00773AF0 (:173)
void unused_dcf_tech_level_change()
{
    const bool help = Console_help != 0;
    bool usage = help;
    if (Console_command != 0)
    {
        if (console_get_arg(CONSOLE_ARG_FLOAT, 1))
        {
            World.change_tech_level(Console_arg_float);
        }
        else
        {
            usage = true;
        }
    }
    if (usage)
    {
        console_output("Usage: tech_level_change <delta>", nullptr, false);
    }
    char text[256];
    sprintf(text, "guerrilla tech level is %f", (double)gfm_get_tech_level());
    console_output(text, nullptr, false);
}

// 0x00773BB0 (:193)
void unused_dcf_tech_level_max_set()
{
    const bool help = Console_help != 0;
    bool usage = help;
    if (Console_command != 0)
    {
        if (console_get_arg(CONSOLE_ARG_FLOAT, 1))
        {
            World.set_tech_level_max(Console_arg_float);
        }
        else
        {
            usage = true;
        }
    }
    if (usage)
    {
        console_output("Usage: tech_level_max_set <new value>", nullptr, false);
    }
    char text[256];
    sprintf(text, "guerrilla tech level max is %f", (double)World.get_tech_level_max());
    console_output(text, nullptr, false);
}

// 0x00773C50 (:213)
void unused_dcf_tech_level_max_change()
{
    const bool help = Console_help != 0;
    bool usage = help;
    if (Console_command != 0)
    {
        if (console_get_arg(CONSOLE_ARG_FLOAT, 1))
        {
            World.change_tech_level_max(Console_arg_float);
        }
        else
        {
            usage = true;
        }
    }
    if (usage)
    {
        console_output("Usage: tech_level_max_change <delta>", nullptr, false);
    }
    char text[256];
    // rfg.pdb's spelling of "guerrila".
    sprintf(text, "guerrila tech level max is %f", (double)World.get_tech_level_max());
    console_output(text, nullptr, false);
}

// 0x0079D140 (:233)
void unused_dcf_tech_level_display()
{
    gfm_tech_level_display();
}

// 0x00773CF0 (:244)
void gfm_tech_level_init()
{
    xml_element* root = xml_table_parse("tech_level.xtbl");
    xml_element* cur = xml_table_find_req(root, "Tech_level");
    // Original quirk kept: the table must hold all five entries.
    for (tech_level_weapon& weapon : Tech_level_weapons)
    {
        weapon.tech_level_value = xml_get_float(xml_find(cur, "Tech_level_value"), 0.0f);
        weapon.i_info = inv_item_get_info(xml_table_get_req_string_ref(cur, "Name"));
        cur = xml_find_next(root, cur, "Tech_level");
    }
    // Sorted by level, the smallest first. rfg.exe's loop is the same bubble sort, unrolled four steps at a time.
    for (int pass = 0; pass < NUM_TECH_LEVEL_WEAPONS - 1; pass++)
    {
        for (int i = NUM_TECH_LEVEL_WEAPONS - 1; i > pass; i--)
        {
            if (Tech_level_weapons[i].tech_level_value < Tech_level_weapons[i - 1].tech_level_value)
            {
                swap_tech_level_weapon(Tech_level_weapons[i - 1], Tech_level_weapons[i]);
            }
        }
    }
    xml_table_close(root);
}

// 0x0078C120 (:280)
void gfm_reset_tech_level()
{
    World.set_tech_level(0.0f);
    Marauder_progression_level = 0;
    EDF_progression_level = 0;
}

// 0x0075DE90 (:289)
float gfm_get_tech_level()
{
    return stats_cheat_enabled(CHEATS_MAX_TECHNOLOGY) ? World.get_tech_level_max() : World.get_tech_level();
}

// 0x0075DEB0 (:298)
void gfm_set_tech_level(float tech_level_value)
{
    World.set_tech_level(tech_level_value);
}

// 0x0075DED0 (:303)
void gfm_change_tech_level(float tech_level_delta)
{
    World.change_tech_level(tech_level_delta);
}

// 0x0075DEF0 (:308)
float gfm_get_tech_level_max()
{
    return World.get_tech_level_max();
}

// 0x0075DF00 (:313)
void gfm_set_tech_level_max(float tech_level_max_value)
{
    World.set_tech_level_max(tech_level_max_value);
}

// 0x0075DF20 (:318)
void gfm_change_tech_level_max(float tech_level_max_delta)
{
    World.change_tech_level_max(tech_level_max_delta);
}

// 0x00773EE0 (:324)
void edf_progression_set_level(int new_progression_level)
{
    EDF_progression_level = clamp_progression_level(new_progression_level);
}

// 0x0078C140 (:334)
// A district can ask for a minimum EDF progression level of its own.
int edf_progression_level_internal()
{
    int grid_x;
    int grid_y;
    if (!rfg_zonex_get_grid_coords(player_get_local_player()->pos, grid_x, grid_y))
    {
        return EDF_progression_level;
    }
    // Magic numbers: the 16 x 16 district grid and one spare entry.
    const int cell = grid_y * 16 + grid_x;
    if (cell >= 0x101)
    {
        return EDF_progression_level;
    }
    unsigned int handle = District_map[cell];
    if (handle == 0xFFFFFFFF)
    {
        handle = get_default_district_handle();
        if (handle == 0xFFFFFFFF)
        {
            return EDF_progression_level;
        }
    }
    const object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_DISTRICT)
    {
        return EDF_progression_level;
    }
    const int level = static_cast<const district*>(objp)->get_minimum_edf_progression();
    if (level >= 0 && level <= MAX_PROGRESSION_LEVEL)
    {
        return level < EDF_progression_level ? EDF_progression_level : level;
    }
    return EDF_progression_level;
}

// 0x0078C1D0 (:364)
void edf_progression_lock_mission()
{
    EDF_locked_mission_progression_level = edf_progression_level_internal();
}

// 0x0078C1E0 (:372)
int edf_progression_level()
{
    if (stats_cheat_enabled(CHEATS_MAX_TECHNOLOGY))
    {
        return MAX_PROGRESSION_LEVEL;
    }
    return Mission_active_index != INVALID_MISSION_HANDLE ? EDF_locked_mission_progression_level : edf_progression_level_internal();
}

// 0x00773F10 (:388)
void marauder_progression_set_level(int new_progression_level)
{
    Marauder_progression_level = clamp_progression_level(new_progression_level);
}

// 0x0075DF40 (:398)
int marauder_progression_level()
{
    return stats_cheat_enabled(CHEATS_MAX_TECHNOLOGY) ? MAX_PROGRESSION_LEVEL : Marauder_progression_level;
}

// 0x0075DF60 (:408)
void progression_level_system_save(cfile* save_file)
{
    cf_write_float(stats_cheat_enabled(CHEATS_MAX_TECHNOLOGY) ? World.get_tech_level_max() : World.get_tech_level(), save_file);
    cf_write_int32(Marauder_progression_level, save_file);
    cf_write_int32(EDF_progression_level, save_file);
}

// 0x00773F40 (:417)
void progression_level_system_load(cfile* load_file)
{
    const float tech_level = cf_read_float(load_file);
    const int marauder_level = cf_read_int32(load_file);
    const int edf_level = cf_read_int32(load_file);
    World.set_tech_level(tech_level);
    Marauder_progression_level = clamp_progression_level(marauder_level);
    EDF_progression_level = clamp_progression_level(edf_level);
}
