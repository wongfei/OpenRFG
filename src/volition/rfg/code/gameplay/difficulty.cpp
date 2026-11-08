// Original: volition/rfg/code/gameplay/difficulty.cpp
//
// The difficulty level and the per-item values of difficulty.xtbl. Each item holds one value per level; the Custom level
// starts as a copy of the current one when a value is changed. Multiplayer and Wrecking Crew always answer 1.0 (no
// scaling).

#include "volition/rfg/code/gameplay/difficulty.h"

#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"

// ---- public globals
difficulty_values Difficulty_values[NUM_DIFFICULTY_ITEMS]; // 0x022258C0

// ---- file-static globals
static const difficulty_index_type Difficulty_index[NUM_DIFFICULTY_ITEMS] = { // 0x012DB990
    {"MENU_ENEMY_DAMAGE_TAKEN", DIFFICULTY_ENEMY_DAMAGE_TAKEN},
    {"MENU_ENEMY_VEHICLE_DAMAGE", DIFFICULTY_ENEMY_VEHICLE_DAMAGE},
    {"MENU_ENEMY_WEAPON_SPREAD", DIFFICULTY_ENEMY_WEAPON_SPREAD},
    {"MENU_ENEMY_AIM_DRIFT", DIFFICULTY_ENEMY_AIM_DRIFT},
    {"MENU_ENEMY_BURST_SIZE", DIFFICULTY_ENEMY_BURST_SIZE},
    {"MENU_ENEMY_REINFORCEMENTS", DIFFICULTY_ENEMY_REINFORCEMENTS},
    {"MENU_ENEMY_ALERT_LEVEL_EVENTS", DIFFICULTY_ENEMY_ALERT},
    {"MENU_PLAYER_DAMAGE_TAKEN", DIFFICULTY_PLAYER_DAMAGE_TAKEN},
    {"MENU_PLAYER_REGEN_DELAY", DIFFICULTY_PLAYER_REGEN_DELAY},
    {"MENU_PLAYER_REGEN_RATE", DIFFICULTY_PLAYER_REGEN_RATE},
    {"MENU_PLAYER_AMMO_PICKUP", DIFFICULTY_PLAYER_AMMO_PICKUP},
    {"MENU_PLAYER_AMMO_CRATES", DIFFICULTY_PLAYER_AMMO_CRATES},
    {"MENU_PLAYER_VEHICLE_DAMAGE", DIFFICULTY_PLAYER_VEHICLE_DAMAGE},
    {"MENU_PLAYER_VEHICLE_MASS", DIFFICULTY_PLAYER_VEHICLE_MASS},
    {"MENU_PLAYER_DEATH_BUFFER", DIFFICULTY_PLAYER_DEATH_BUFFER},
    {"MENU_ENV_EXPLOSION_DAMAGE", DIFFICULTY_EXPLOSION_DAMAGE},
    {"MENU_ENV_PHYSICS_COLLISIONS", DIFFICULTY_PHYSICS_COLLISIONS},
    {"MENU_PLAYER_FALLING_DAMAGE", DIFFICULTY_FALLING_DAMAGE},
    {"MENU_UPGRADE_COST_MULTIPLIER", DIFFICULTY_UPGRADE_COST},
    {"MENU_MORALE_BOOSTS", DIFFICULTY_MORALE_BOOSTS},
    {"MENU_MORALE_PENALITIES", DIFFICULTY_MORALE_PENALITIES},
    {"MENU_MORALE_PLAYER_DEATH", DIFFICULTY_MORALE_PLAYER_DEATH},
    {"MENU_CONTROL_PARKER", DIFFICULTY_CONTROL_PARKER},
    {"MENU_CONTROL_DUST", DIFFICULTY_CONTROL_DUST},
    {"MENU_CONTROL_BADLANDS", DIFFICULTY_CONTROL_BADLANDS},
    {"MENU_CONTROL_OASIS", DIFFICULTY_CONTROL_OASIS},
    {"MENU_CONTROL_EOS", DIFFICULTY_CONTROL_EOS},
};
static const char* const DIFFICULTY_LEVEL_NAMES[NUM_DIFFICULTY_LEVELS] = {"Easy", "Medium", "Hard", "Insane", "Custom"}; // 0x016513D8
static difficulty_levels Current_difficulty = DIFFICULTY_LEVEL_MEDIUM; // 0x016513EC
static difficulty_levels Difficulty_level_overwrite = DIFFICULTY_LEVEL_INVALID; // 0x016513F0
static bool Difficulty_reload; // 0x02229615

// 0x00772D10 (:81)
void unused_dcf_difficulty_level()
{
    if (Console_help == 0 && Console_command != 0 && console_get_arg(8, 1))
    {
        if (Console_arg_int == NUM_DIFFICULTY_LEVELS)
        {
            console_output("INVALID difficulty level", nullptr, false);
            return;
        }
        if (Console_arg_int < NUM_DIFFICULTY_LEVELS)
        {
            Current_difficulty = (difficulty_levels)Console_arg_int;
            Difficulty_level_overwrite = (difficulty_levels)Console_arg_int;
        }
        console_output("difficulty level set", nullptr, false);
        return;
    }
    console_output("difficulty_level <number> to set the difficulty (Use only when options screen would be available)", nullptr, false);
}

// 0x0075CD70 (:107)
static void reset_difficulty_table()
{
    for (int item = 0; item < NUM_DIFFICULTY_ITEMS; item++)
    {
        for (int level = 0; level < NUM_DIFFICULTY_LEVELS; level++)
        {
            Difficulty_values[item].level[level] = 1.0f;
        }
    }
}

// 0x0075CD90 (:118)
// Each "Difficulty" with a "Name" has a list of items, one value per level but Custom (index 4).
void load_difficulty_data()
{
    xml_element* root = xml_table_parse("difficulty.xtbl");
    for (xml_element* difficulty = xml_find(root, "Difficulty"); difficulty != nullptr; difficulty = xml_find_next(root, difficulty, "Difficulty"))
    {
        if (xml_find(difficulty, "Name") == nullptr)
        {
            continue;
        }
        xml_element* items = xml_find(difficulty, "Difficulty_Items");
        if (items == nullptr)
        {
            // Original: leaves the table open.
            return;
        }
        for (xml_element* element = xml_find(items, "Item"); element != nullptr; element = xml_find_next(items, element, "Item"))
        {
            xml_element* name = xml_find(element, "Name");
            int index = -1;
            // Original: no early out (the last match wins), and a missing Name is read through.
            for (int i = 0; i < NUM_DIFFICULTY_ITEMS; i++)
            {
                if (v_stricmp(name->text, Difficulty_index[i].name) == 0)
                {
                    index = Difficulty_index[i].index;
                }
            }
            if (index == -1)
            {
                continue;
            }
            for (int level = 0; level < NUM_DIFFICULTY_LEVELS; level++)
            {
                if (level != DIFFICULTY_LEVEL_CUSTOM)
                {
                    xml_table_get_req_float(Difficulty_values[index].level[level], element, DIFFICULTY_LEVEL_NAMES[level]);
                }
            }
        }
    }
    xml_table_close(root);
}

// 0x00772D80 (:169)
static void switch_to_custom()
{
    if (Current_difficulty == DIFFICULTY_LEVEL_CUSTOM)
    {
        return;
    }
    for (int item = 0; item < NUM_DIFFICULTY_ITEMS; item++)
    {
        Difficulty_values[item].level[DIFFICULTY_LEVEL_CUSTOM] = Difficulty_values[item].level[Current_difficulty];
    }
    Current_difficulty = DIFFICULTY_LEVEL_CUSTOM;
    Difficulty_level_overwrite = DIFFICULTY_LEVEL_CUSTOM;
}

// 0x0075CEE0 (:184)
void difficulty_system_init()
{
    reset_difficulty_table();
    load_difficulty_data();
}

// 0x0075CF00 (:190)
void difficulty_set_level(difficulty_levels level)
{
    if ((int)level < NUM_DIFFICULTY_LEVELS)
    {
        Current_difficulty = level;
        Difficulty_level_overwrite = level;
    }
}

// 0x0075CF20 (:197)
difficulty_levels difficulty_get_level()
{
    return Current_difficulty;
}

// 0x00772E10 (:205)
// NUM_DIFFICULTY_LEVELS asks for the current level.
float difficulty_get_value(difficulty_items diff_item, difficulty_levels level)
{
    if ((int)diff_item < NUM_DIFFICULTY_ITEMS && !Multiplayer && wrecking_crew_get_state() == WCS_NONE)
    {
        return Difficulty_values[diff_item].level[level != NUM_DIFFICULTY_LEVELS ? level : Current_difficulty];
    }
    return 1.0f;
}

// 0x00772E50 (:229)
void difficulty_change_value(difficulty_items diff_item, float new_value)
{
    switch_to_custom();
    if ((int)diff_item < NUM_DIFFICULTY_ITEMS)
    {
        Difficulty_values[diff_item].level[DIFFICULTY_LEVEL_CUSTOM] = new_value;
    }
}

// 0x0075CF30 (:240)
void difficulty_set_level_reload_overwrite(difficulty_levels level)
{
    Difficulty_level_overwrite = level;
}

// 0x0075CF40 (:245)
// The level and the Custom column.
void difficulty_save(cfile* save_file)
{
    cf_write_int32(Current_difficulty, save_file);
    cf_write_int32(NUM_DIFFICULTY_ITEMS, save_file);
    for (int item = 0; item < NUM_DIFFICULTY_ITEMS; item++)
    {
        cf_write_float(Difficulty_values[item].level[DIFFICULTY_LEVEL_CUSTOM], save_file);
    }
}

// 0x0075CF90 (:260)
// A reload's overwrite beats the saved level; an unexpected item count skips the saved values.
void difficulty_load(cfile* save_file)
{
    difficulty_levels level = (difficulty_levels)cf_read_uint32(save_file);
    if (Difficulty_level_overwrite != DIFFICULTY_LEVEL_INVALID)
    {
        level = Difficulty_level_overwrite;
    }
    if ((int)level < NUM_DIFFICULTY_LEVELS)
    {
        Current_difficulty = level;
        Difficulty_level_overwrite = level;
    }
    const unsigned int count = cf_read_uint32(save_file);
    if (count == NUM_DIFFICULTY_ITEMS)
    {
        for (int item = 0; item < NUM_DIFFICULTY_ITEMS; item++)
        {
            Difficulty_values[item].level[DIFFICULTY_LEVEL_CUSTOM] = cf_read_float(save_file);
        }
        return;
    }
    for (unsigned int i = 0; (int)count > 0 && i < count; i++)
    {
        cf_read_float(save_file);
    }
}

// 0x0075D000 (:286)
void unused_dcf_reload_difficulty_table()
{
    Difficulty_reload = true;
    reset_difficulty_table();
    load_difficulty_data();
    Difficulty_reload = false;
}
