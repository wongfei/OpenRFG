// Original: volition/rfg/code/gameplay/upgrades.cpp
//
// The upgrade table (upgrades.xtbl or a DLC's) and its definitions and levels, the level costs, the upgrade queries,
// the unlocks from liberation and activities and the upgrade notifications. player_minimal.md P4 started it, MP8 added
// the charge count, the sledgehammer level, the nano enhancer and the singularity increase, weapons.md WP0 the arc welder
// and grinder queries, gameflow.md GF3 the rest.
// The external interface is declared in upgrades.h.

#include "volition/rfg/code/gameplay/upgrades.h"

#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/cutscene/video_cutscene.h"
#include "volition/rfg/code/hud/hud_ui_district_liberation.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_meta_data.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/static_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/unicode_tools.h"

#include "keen/base/trace.hpp"

#include <stdio.h>

// ---- file statics
// 0x01651554, read from rfg.exe: the <Type> names, in upgrade_types order.
static const char* UPGRADE_TYPE_NAMES[NUM_UPGRADE_TYPES] = {"General", "Vehicle", "Weapon", "Weapon Upgrade"};
static int Multiple_rockets_upgrade = 1; // 0x01651564
static int RFG_max_charges = 2; // 0x01651568, the charge tweak (2: the remote charge count upgrade decides)
static float Nano_rifle_upgrade_damage_modifier = 2.0f; // 0x01651578

// 0x01311088, read from rfg.exe: the sledgehammer of each level (the golden hammer and above need it unlocked).
static const hammer_upgrade_data Hammer_upgrade_data[16] = {
    {UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL, "sledgehammer", false},
    {UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL1, "sledge_upgrade_1", false},
    {UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL2, "sledge_upgrade_2", false},
    {UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL_GOLD, "golden_hammer", true},
    {UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL3, "sledge_upgrade_3", true},
    {(upgrade_definition_names)42, "sledge_upgrade_4", true},
    {(upgrade_definition_names)43, "sledge_upgrade_5", true},
    {(upgrade_definition_names)44, "sledge_upgrade_6", true},
    {(upgrade_definition_names)45, "sledge_upgrade_7", true},
    {(upgrade_definition_names)46, "sledge_upgrade_8", true},
    {(upgrade_definition_names)47, "sledge_upgrade_9", true},
    {(upgrade_definition_names)48, "sledge_upgrade_10", true},
    {(upgrade_definition_names)49, "sledge_upgrade_11", true},
    {(upgrade_definition_names)50, "sledge_upgrade_12", true},
    {(upgrade_definition_names)51, "sledge_upgrade_13", true},
    {UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL14, "sledge_upgrade_14", true},
};
static char* Upgrades_string_pool_data; // 0x0222967C
static bool Upgrade_arc_welder_3rd_arc; // 0x02229681, debug: the third arc without the upgrade
static upgrade_definition_data Upgrade_data[NUM_OF_UPGRADE_DEFINITIONS]; // 0x0223A5F8
static string_pool Upgrades_string_pool("Upgrades String Pool"); // 0x02272DE0

// The upgrade name keys, one per upgrade_definition_names value in order (the original fills upgrade_index with their
// localization hashes in its dynamic initializer, 0x0106BF00).
static const char* const Upgrade_index_names[NUM_OF_UPGRADE_DEFINITIONS] = {
    "jetpack", "remote_charge_count", "remote_charges", "arc_welder", "arc_welder_3rd_arc", "arc_welder_smart", "grinder",
    "grinder_faster", "grinder_explosive", "proximity_mine", "proximity_mine_smart", "rocket", "rocket_multi",
    "rocket_heat_seeking", "thermo_rocket", "nano_rifle", "nano_enhancer", "personnel", "radiation", "sledgehammer",
    "sledgehammer1", "sledgehammer2", "guns_of_tharsis_complete", "guns_of_tharsis_unlock1", "guns_of_tharsis_unlock2",
    "pistol", "player_armor", "eos_real_missions", "ore_bonus", "safehouse_warp", "salvage_collector",
    "singularity_increase", "rail_driver", "spiker", "gauss_rifle", "shotgun", "gutter", "missilepod", "super_gauss",
    "reconstructor", "golden_hammer", "sledgehammer3", "sledgehammer4", "sledgehammer5", "sledgehammer6",
    "sledgehammer7", "sledgehammer8", "sledgehammer9", "sledgehammer10", "sledgehammer11", "sledgehammer12",
    "sledgehammer13", "sledgehammer14", "backpack_thrust", "backpack_fleetfoot", "backpack_stealth", "backpack_vision",
    "backpack_commando", "backpack_shockwave", "backpack_heal", "backpack_rhino", "backpack_tremor",
};

// 0x02273B50: the name hash of each upgrade.
static struct upgrade_index_table
{
    upgrade_index_type entries[NUM_OF_UPGRADE_DEFINITIONS];

    upgrade_index_table()
    {
        for (int i = 0; i < NUM_OF_UPGRADE_DEFINITIONS; i++)
        {
            entries[i].name_crc = localization_generate_hash_from_key(Upgrade_index_names[i]);
            entries[i].index = (upgrade_definition_names)i;
        }
    }

    const upgrade_index_type& operator[](int i) const
    {
        return entries[i];
    }
} upgrade_index;

// ---- upgrade_level

// 0x00775840 (:492)
// The cost scaled by the difficulty (single player) and rounded to 25 (up when the difficulty makes upgrades
// cheaper, down otherwise; 25 at least). Free: the free upgrades cheat, unlocked hammers and backpacks, and the jetpack
// past level 1.
const unsigned short upgrade_level::get_cost(unsigned int upgrade_id, bool ignore_cheats) const
{
    if (cost == 0)
    {
        return 0;
    }
    if (!ignore_cheats && stats_cheat_enabled(CHEATS_FREE_UPGRADES))
    {
        return 0;
    }
    if (meta_data_get_hammer_upgrade_index((upgrade_definition_names)upgrade_id) > -1 && meta_data_has_hammer_unlocked(upgrade_id))
    {
        return 0;
    }
    if (meta_data_get_backpack_upgrade_index(upgrade_id) > -1 && meta_data_has_backpack_unlocked(upgrade_id))
    {
        return 0;
    }
    if (upgrade_id == UPGRADE_DEFINITION_JETPACK && meta_data_get_jetpack_upgrade_level() > 1)
    {
        return 0;
    }
    const bool scaled = !Multiplayer && !wrecking_crew_is_active();
    const float scale = scaled ? difficulty_get_value(DIFFICULTY_UPGRADE_COST, NUM_DIFFICULTY_LEVELS) : 1.0f;
    unsigned int scaled_cost = (unsigned int)(int)(scale * (float)cost) & 0xFFFF;
    // Magic number: costs are multiples of 25 salvage.
    const unsigned int rest = scaled_cost % 25;
    if ((unsigned short)rest != 0)
    {
        if (!scaled || 1.0f <= scale)
        {
            scaled_cost = scaled_cost - rest;
        }
        else
        {
            scaled_cost = scaled_cost + (25 - rest);
        }
    }
    unsigned short result = (unsigned short)scaled_cost;
    if (result < 25)
    {
        result = 25;
    }
    return result;
}

// ---- upgrade_definition_data

// 0x00768C80 (.h:156)
upgrade_definition_data::upgrade_definition_data()
{
    reset();
}

// 0x0075F500 (:537)
void upgrade_definition_data::reset()
{
    upgrade_name_crc = 0;
    availability_bitfield = 0;
    show_locked_bitfield = 0;
    upgrade_type = 0;
    num_levels = 0;
    w_info = nullptr;
    icon_name = nullptr;
    blueprint_name = nullptr;
    voice_line = -1;
    always_show_upgrade = false;
}

// 0x0075F530 (:553)
void upgrade_definition_data::set_id(unsigned int index)
{
    upgrade_id = 0;
    if (index < NUM_OF_UPGRADE_DEFINITIONS)
    {
        upgrade_id = index;
    }
}

// 0x00775940 (:566)
// One <Upgrade>: its type, display name, weapon, icon, blueprint, voice line, tool tip, and its <Levels> (name,
// description, cost, value, availability, unlocks, district and activity count).
bool upgrade_definition_data::load(xml_element* xml_root)
{
    xml_element* e = xml_find(xml_root, "Type");
    if (e == nullptr)
    {
        return false;
    }
    for (unsigned char t = 0; t < NUM_UPGRADE_TYPES; t++)
    {
        if (v_stricmp(e->text, UPGRADE_TYPE_NAMES[t]) == 0)
        {
            upgrade_type = t;
            break;
        }
    }
    e = xml_find(xml_root, "Display_name");
    if (e != nullptr)
    {
        upgrade_name_crc = localization_generate_hash_from_key(e->text);
    }
    e = xml_find(xml_root, "Weapon");
    if (e != nullptr)
    {
        w_info = weapons_get_weapon_info(e->text);
    }
    e = xml_find(xml_root, "Icon_Image");
    if (e != nullptr && e->text != nullptr)
    {
        icon_name = Upgrades_string_pool.add_unique(e->text);
    }
    e = xml_find(xml_root, "Blueprint");
    if (e != nullptr && e->text != nullptr)
    {
        blueprint_name = Upgrades_string_pool.add_unique(e->text);
    }
    e = xml_find(xml_root, "voice_line");
    if (e != nullptr)
    {
        voice_line = voice_line_from_name(e->text);
    }
    e = xml_find(xml_root, "Post_Purchase_Tool_Tip");
    post_purchase_tool_tip_crc = e == nullptr ? 0 : localization_generate_hash_from_key(e->text);
    e = xml_find(xml_root, "always_show_upgrade");
    always_show_upgrade = e == nullptr ? false : xml_get_bool(e, false);
    xml_element* levels_root = xml_find(xml_root, "Levels");
    for (xml_element* le = xml_find(levels_root, "Level"); le != nullptr; le = xml_find_next(levels_root, le, "Level"))
    {
        upgrade_level& lv = levels[num_levels];
        e = xml_find(le, "Upgrade_Name");
        lv.name_crc = 0;
        if (e != nullptr)
        {
            lv.name_crc = localization_generate_hash_from_key(e->text);
        }
        e = xml_find(le, "Description");
        lv.description_crc = 0;
        if (e != nullptr)
        {
            lv.description_crc = localization_generate_hash_from_key(e->text);
        }
        e = xml_find(le, "Cost");
        if (e != nullptr)
        {
            lv.set_cost((unsigned short)xml_get_int(e, 0)); // inlined
        }
        e = xml_find(le, "Value");
        if (e != nullptr)
        {
            lv.value = (unsigned char)xml_get_int(e, 0);
        }
        e = xml_find(le, "Available");
        if (e != nullptr && xml_get_bool(e, false))
        {
            availability_bitfield |= (unsigned short)(1 << (num_levels & 0x1F));
        }
        e = xml_find(le, "Show_Locked");
        lv.show_locked = false;
        if (e != nullptr && xml_get_bool(e, false))
        {
            lv.show_locked = true;
        }
        e = xml_find(le, "Unlock_Action");
        if (e != nullptr)
        {
            lv.unlock_action = UPGRADE_WAS_UNLOCKED;
            if (v_stricmp(e->text, "Purchased") == 0)
            {
                lv.unlock_action = UPGRADE_WAS_PURCHASED;
            }
        }
        lv.unlock_upgrade_id = NUM_OF_UPGRADE_DEFINITIONS;
        e = xml_find(le, "Also_Unlock");
        if (e != nullptr)
        {
            const unsigned int handle = upgrades_get_handle(localization_generate_hash_from_key(e->text));
            if (handle != 0xFFFFFFFF)
            {
                lv.unlock_upgrade_id = handle;
            }
        }
        e = xml_find(le, "Also_Unlock_Level");
        if (e != nullptr)
        {
            lv.unlock_upgrade_level = (unsigned char)xml_get_int(e, 0);
        }
        const char* district_name = xml_table_get_opt_string_ref(le, "District_Liberated");
        lv.unlock_district_name = district_name == nullptr ? nullptr : Upgrades_string_pool.add_unique(district_name);
        e = xml_find(le, "Activity_Count");
        if (e != nullptr)
        {
            lv.activity_count = (unsigned int)xml_get_int(e, 0);
        }
        num_levels++;
    }
    return true;
}

// 0x00A05740 (.h:170)
unsigned short upgrade_definition_data::get_default_availability() const
{
    return availability_bitfield;
}

// 0x0075F550 (:717)
const upgrade_level* upgrade_definition_data::get_upgrade_level(unsigned int level_index) const
{
    if (num_levels <= level_index)
    {
        return nullptr;
    }
    return &levels[level_index];
}

// 0x00A05750 (.h:172)
const unsigned int upgrade_definition_data::get_max_level() const
{
    return num_levels - 1;
}

// 0x00753D90 (.h:173)
const unsigned int upgrade_definition_data::get_id() const
{
    return upgrade_id;
}

// 0x00753DA0 (.h:174)
const unsigned char upgrade_definition_data::get_upgrade_type() const
{
    return upgrade_type;
}

// 0x00753DB0 (.h:175)
const weapon_info* upgrade_definition_data::get_weapon_info() const
{
    return w_info;
}

// 0x0083F5D0 (.h:177)
const unsigned int upgrade_definition_data::get_post_purchase_tool_tip_crc() const
{
    return post_purchase_tool_tip_crc;
}

// 0x0083F5E0 (.h:178)
const char* upgrade_definition_data::get_icon_name() const
{
    return icon_name;
}

// 0x0083F5F0 (.h:179)
const char* upgrade_definition_data::get_blueprint_name() const
{
    return blueprint_name;
}

// 0x0074F380 (.h:176)
const unsigned int upgrade_definition_data::get_name_crc() const
{
    return upgrade_name_crc;
}

// 0x00A05760 (.h:180)
const int upgrade_definition_data::get_voice_line() const
{
    return voice_line;
}

// 0x00753D80 (.h:146)
void upgrade_level::set_cost(unsigned short new_cost)
{
    cost = new_cost;
}

// ---- the table

// 0x00790C20 (:433)
// Reads upgrades.xtbl (the base game resets every definition first) or dlcNN_upgrades.xtbl when it exists: each
// <Upgrade> named in the index is reset and loaded into its slot.
static void load_upgrade_data(int dlc_id)
{
    char filename[64];
    if (dlc_id == -1)
    {
        strcpy(filename, "upgrades.xtbl");
        for (unsigned int i = 0; i < NUM_OF_UPGRADE_DEFINITIONS; i++)
        {
            Upgrade_data[i].reset(); // inlined
        }
    }
    else
    {
        sprintf(filename, "dlc%02d_%s", dlc_id, "upgrades.xtbl");
        if (!cf_exist(filename))
        {
            return;
        }
    }
    xml_element* root = xml_table_parse(filename);
    for (xml_element* up = xml_find(root, "Upgrade"); up != nullptr; up = xml_find_next(root, up, "Upgrade"))
    {
        xml_element* name = xml_find(up, "Name");
        if (name == nullptr)
        {
            continue;
        }
        const unsigned int crc = localization_generate_hash_from_key(name->text);
        for (unsigned int i = 0; i < NUM_OF_UPGRADE_DEFINITIONS; i++)
        {
            if (crc == upgrade_index[i].name_crc)
            {
                const upgrade_definition_names index = upgrade_index[i].index;
                // Original: -1 (~UPGRADE_DEFINITION_JETPACK) skips the entry.
                if ((int)index != -1)
                {
                    upgrade_definition_data* data = &Upgrade_data[index];
                    data->set_id(index); // inlined
                    data->reset(); // inlined
                    data->load(up);
                }
                break;
            }
        }
    }
    xml_table_close(root);
}

// 0x0075F600 (:759)
const upgrade_definition_data* upgrade_get_data_by_index(unsigned int upgrade_id)
{
    if (upgrade_id > NUM_OF_UPGRADE_DEFINITIONS - 1)
    {
        return nullptr;
    }
    return &Upgrade_data[upgrade_id];
}

// 0x00790DB0 (:778)
// The string pool (2 KB of the permanent pool) is set up once, cleared, and the table read.
void upgrade_system_init(int dlc_id)
{
    if (Upgrades_string_pool_data == nullptr)
    {
        // Magic number: 0x800 bytes of upgrade strings.
        Upgrades_string_pool_data = (char*)Permanent_mempool.alloc(0x800, 0);
        if (Upgrades_string_pool_data == nullptr)
        {
            keen::debug::nativePrintArgs("Insufficient memory to allocate the raid string table");
        }
        Upgrades_string_pool.init(Upgrades_string_pool_data, 0x800, "Upgrades String Pool", 0, 0);
    }
    Upgrades_string_pool.set_thread_ownership();
    Upgrades_string_pool.clear();
    load_upgrade_data(dlc_id);
}

// 0x0075F620 (:795)
void upgrade_level_init()
{
}

// 0x0075F630 (:800)
unsigned int upgrades_get_handle(unsigned int upgrade_name_crc)
{
    for (unsigned int i = 0; i < NUM_OF_UPGRADE_DEFINITIONS; i++)
    {
        if (upgrade_name_crc == upgrade_index[i].name_crc)
        {
            return upgrade_index[i].index;
        }
    }
    return 0xFFFFFFFF;
}

// ---- queries

// 0x00775D70 (:812)
// The jetpack (single player, when its level is set or no backpack is), the first owned backpack, or none.
upgrade_definition_names upgrade_get_backpack_type()
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return NUM_OF_UPGRADE_DEFINITIONS;
    }
    player_meta_data* meta = player_get_meta_data();
    if (meta == nullptr || meta->get_upgrade_value(UPGRADE_DEFINITION_JETPACK) > 0)
    {
        return UPGRADE_DEFINITION_JETPACK;
    }
    for (unsigned int id = UPGRADE_DEFINITION_BACKPACK_THRUST; id < NUM_OF_UPGRADE_DEFINITIONS; id++)
    {
        if (meta->get_upgrade_value(id) > 0)
        {
            return (upgrade_definition_names)id;
        }
    }
    return NUM_OF_UPGRADE_DEFINITIONS;
}

// 0x00775E50 (:874)
// The rockets per shot: the upgrade's value in single player (outside Demolitions Master, while the tweak allows it).
int upgrade_get_multi_rockets()
{
    player_meta_data* meta = player_get_meta_data();
    int rockets = 1;
    if (!Multiplayer && !wrecking_crew_is_active() && !activity_is_active(ACTIVITY_MODE_DEMOLITIONS_MASTER))
    {
        rockets = Multiple_rockets_upgrade;
        if (meta != nullptr && Multiple_rockets_upgrade == 1)
        {
            rockets = meta->get_upgrade_value(UPGRADE_DEFINITION_ROCKET_MULTI);
        }
    }
    return rockets;
}

// 0x00775EF0 (:903)
// The arc welder's third arc: always in multiplayer and Wrecking Crew; in single player with the upgrade (or the debug
// switch).
bool upgrade_has_arc_welder_3rd_arc()
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return true;
    }
    player_meta_data* meta = player_get_meta_data();
    if (meta != nullptr && meta->get_upgrade_value(UPGRADE_DEFINITION_ARC_WELDER_3RD_ARC) != 0)
    {
        return true;
    }
    return Upgrade_arc_welder_3rd_arc;
}

// 0x00775F30 (:920)
// The smart arc welder (no arcs on friendly vehicles): single player only.
bool upgrade_has_arc_welder_smart()
{
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        int value = 0;
        player_meta_data* meta = player_get_meta_data();
        if (meta != nullptr)
        {
            value = meta->get_upgrade_value(UPGRADE_DEFINITION_ARC_WELDER_SMART);
        }
        return value != 0;
    }
    return false;
}

// 0x00775F70 (:937)
// The remote charges out at once: 4 in multiplayer, 12 in Wrecking Crew; in single player the tweak, or with the
// tweak at 2 the remote charge count upgrade's value.
int upgrade_get_max_charges()
{
    player_meta_data* meta = player_get_meta_data();
    if (Multiplayer)
    {
        if (!wrecking_crew_is_active())
        {
            return 4; // Magic number: 4 charges in multiplayer
        }
    }
    else if (!wrecking_crew_is_active())
    {
        if (meta == nullptr || RFG_max_charges != 2)
        {
            return RFG_max_charges;
        }
        return meta->get_upgrade_value(UPGRADE_DEFINITION_REMOTE_CHARGE_COUNT);
    }
    return 12; // Magic number: 12 charges in Wrecking Crew
}

// 0x00775FC0 (:954)
// The fast grinder charge: single player only.
bool upgrade_has_fast_grinder()
{
    int value = 0;
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        player_meta_data* meta = player_get_meta_data();
        if (meta != nullptr)
        {
            value = meta->get_upgrade_value(UPGRADE_DEFINITION_GRINDER_FASTER);
        }
    }
    return value != 0;
}

// 0x00776000 (:966)
// The explosive grinder blades: always in Wrecking Crew, else the single player upgrade.
bool upgrade_has_grinder_explosive()
{
    int value = 0;
    if (!Multiplayer)
    {
        if (wrecking_crew_is_active())
        {
            return true;
        }
        player_meta_data* meta = player_get_meta_data();
        if (meta != nullptr)
        {
            value = meta->get_upgrade_value(UPGRADE_DEFINITION_GRINDER_EXPLOSIVE);
        }
    }
    // Original: Wrecking Crew is checked again here, so it also counts in multiplayer.
    if (wrecking_crew_is_active())
    {
        return true;
    }
    return value != 0;
}

// 0x007760C0 (:1005)
bool upgrade_has_radiation_shield()
{
    int value = 0;
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        player_meta_data* meta = player_get_meta_data();
        if (meta != nullptr)
        {
            value = meta->get_upgrade_value(UPGRADE_DEFINITION_RADIATION);
        }
    }
    return value != 0;
}

// 0x00776100 (:1017)
// The sledgehammer level in single player: the first level from 1 whose upgrade value is 1 (0 with none, and in
// multiplayer and Wrecking Crew).
int upgrade_get_sledgehammer_upgrade_level()
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return 0;
    }
    player_meta_data* meta = player_get_meta_data();
    if (meta == nullptr)
    {
        return 0;
    }
    for (int level = 1; level < 16; level++)
    {
        if (meta->get_upgrade_value(Hammer_upgrade_data[level].upgrade_id) == 1)
        {
            return level;
        }
    }
    return 0;
}

// 0x00776170 (:1039)
const char* upgrade_get_sledgehamer_weapon_name_from_upgrade_level()
{
    return Hammer_upgrade_data[upgrade_get_sledgehammer_upgrade_level()].weapon_name;
}

// 0x00776240 (:1080)
// Wrecking crew players always wear level 2 armor.
int upgrade_get_armor_level()
{
    if (wrecking_crew_is_active())
    {
        return 2;
    }
    if (Multiplayer)
    {
        return 0;
    }
    player_meta_data* meta = player_get_meta_data();
    return meta != nullptr ? meta->get_upgrade_value(UPGRADE_DEFINITION_PLAYER_ARMOR) : 0;
}

// 0x00776280 (:1095)
// The damage factor of the armor: 0.8 (level 1), 0.625 (level 2, also wrecking crew and the first DLC), 0.1 with the
// super toughness cheat.
float upgrade_get_player_armor()
{
    int level = 0;
    if (wrecking_crew_is_active())
    {
        level = 2;
    }
    else if (!Multiplayer)
    {
        player_meta_data* meta = player_get_meta_data();
        if (meta != nullptr)
        {
            level = meta->get_upgrade_value(UPGRADE_DEFINITION_PLAYER_ARMOR);
        }
        if (World.dlc_bundle_id == 1)
        {
            level = 2;
        }
    }
    float factor = 1.0f;
    if (level == 1)
    {
        factor = 0.8f;
    }
    else if (level == 2)
    {
        factor = 0.625f;
    }
    if (stats_cheat_enabled(CHEATS_SUPER_TOUGHNESS))
    {
        return 0.1f;
    }
    return factor;
}

// 0x00776350 (:1140): the nano rifle damage factor: 2 with the single player upgrade, else 1
float upgrade_get_nano_enhancer()
{
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        player_meta_data* meta = player_get_meta_data();
        if (meta != nullptr && meta->get_upgrade_value(UPGRADE_DEFINITION_NANO_ENHANCER) != 0)
        {
            return Nano_rifle_upgrade_damage_modifier;
        }
    }
    return 1.0f;
}

// 0x00776420 (:1176): the salvage collector, single player only. Magic number: 0x1E, its upgrade id.
bool upgrade_has_salvage_collector()
{
    int value = 0;
    // Deviation from rfg.exe: wrecking_crew_is_active, where it reads the state directly.
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        player_meta_data* meta = player_get_meta_data();
        if (meta != nullptr)
        {
            value = meta->get_upgrade_value(0x1E);
        }
    }
    return value != 0;
}

// 0x00776460 (:1188): the singularity bomb pickup bonus, single player only
bool upgrade_has_singularity_increase()
{
    int value = 0;
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        player_meta_data* meta = player_get_meta_data();
        if (meta != nullptr)
        {
            value = meta->get_upgrade_value(UPGRADE_DEFINITION_SINGULARITY_INCREASE);
        }
    }
    return value != 0;
}

// ---- gameflow.md GF3: the other queries, the unlocks and the notifications. Placed here, not by PDB line, so the diff
// to the earlier passes stays small.

// Both are debug switches: nothing in rfg.exe writes them (console variables stripped from retail).
static bool Heat_seeking_upgrade; // 0x0222967B
static bool Smart_proximity_mines; // 0x02229680
static float Upgrade_jetpack_increased_charge = 2.0f; // 0x0165156C
static float Upgrade_jetpack_cheat_charge = 3.0f; // 0x01651570

// 0x00753DC0 (.h:184)
bool upgrade_definition_data::always_show() const
{
    return always_show_upgrade;
}

// 0x007757C0 (:411)
static upgrade_definition_data* find_upgrade_definition_slot(unsigned int upgrade_name_crc)
{
    for (unsigned int i = 0; i < NUM_OF_UPGRADE_DEFINITIONS; i++)
    {
        if (upgrade_name_crc == upgrade_index[i].name_crc)
        {
            const upgrade_definition_names index = upgrade_index[i].index;
            // Original: -1 (~UPGRADE_DEFINITION_JETPACK) has no slot.
            if ((int)index == -1)
            {
                return nullptr;
            }
            Upgrade_data[index].set_id(index); // inlined
            return &Upgrade_data[index];
        }
    }
    return nullptr;
}

// 0x00775800 (:423): the same as upgrade_definition_data::reset. rfg.exe has no caller left of it outside the table load.
void upgrade_reset_data_by_index(unsigned int upgrade_id)
{
    if (upgrade_id < NUM_OF_UPGRADE_DEFINITIONS)
    {
        Upgrade_data[upgrade_id].reset();
    }
}

// 0x0075F570 (:725)
// What it looks up goes nowhere: the print is stripped from retail, and only the two buffers remain.
void upgrade_definition_data::print_data()
{
    char name[256];
    const wchar_t* display_name = localization_get_display_string_req(upgrade_name_crc);
    if (display_name == nullptr)
    {
        return;
    }
    // Magic number: 0xFF, the buffer less its terminator.
    copy_to_ascii(name, display_name, 0xFF);
    for (unsigned int i = 0; i < num_levels; i++)
    {
        char description[256];
        copy_to_ascii(description, localization_get_display_string_req(levels[i].description_crc), 0xFF);
    }
}

// 0x00775D20 (:769)
const upgrade_definition_data* upgrade_get_data_by_name(unsigned int upgrade_name_crc)
{
    return find_upgrade_definition_slot(upgrade_name_crc);
}

// 0x00775DD0 (:855)
// Original: a missing meta data counts as level 1, and level 1 charges like no upgrade at all.
float upgrade_jetpack_charge_multiplier()
{
    int level = 0;
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        player_meta_data* meta = player_get_meta_data();
        level = meta == nullptr ? 1 : meta->get_upgrade_value(UPGRADE_DEFINITION_JETPACK);
    }
    if (stats_cheat_enabled(CHEATS_SUPER_JETPACK))
    {
        return Upgrade_jetpack_cheat_charge;
    }
    if (level == 2)
    {
        return Upgrade_jetpack_increased_charge;
    }
    return 1.0f;
}

// The queries below read one upgrade's level, single player only. The magic numbers are upgrade ids.
static int upgrade_value_single_player(unsigned int upgrade_id)
{
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        player_meta_data* meta = player_get_meta_data();
        if (meta != nullptr)
        {
            return meta->get_upgrade_value(upgrade_id);
        }
    }
    return 0;
}

// 0x00775EA0 (:891)
// Deviation: rfg.exe compares Demolitions_master_activity's mode with activity.cpp's static Current_activity_mode.
bool upgrade_has_heat_seeking_rockets()
{
    if (activity_get_mode() != ACTIVITY_MODE_DEMOLITIONS_MASTER && upgrade_value_single_player(0xD) != 0)
    {
        return true;
    }
    return Heat_seeking_upgrade;
}

// 0x00776040 (:981)
bool upgrade_has_smart_proximity_mines()
{
    if (upgrade_value_single_player(0xA) != 0)
    {
        return true;
    }
    return Smart_proximity_mines;
}

// 0x00776080 (:993)
bool upgrade_has_personnel_detector()
{
    return upgrade_value_single_player(0x11) != 0;
}

// 0x00776180 (:1045)
bool upgrade_completed_guns_of_tharsis()
{
    return upgrade_value_single_player(0x16) != 0;
}

// 0x007761C0 (:1057)
// The first weapon upgrade of this weapon decides; level 2 and up has the extra ammo.
bool upgrade_weapon_has_extra_ammo(const weapon_info* w_info)
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return false;
    }
    for (unsigned int i = 0; i < NUM_OF_UPGRADE_DEFINITIONS; i++)
    {
        const upgrade_definition_data& data = Upgrade_data[i];
        if (data.get_upgrade_type() == UPGRADE_TYPE_WEAPON && data.get_weapon_info() == w_info)
        {
            player_meta_data* meta = player_get_meta_data();
            return meta != nullptr && meta->get_upgrade_value(data.get_id()) > 1;
        }
    }
    return false;
}

// 0x00776310 (:1128)
bool upgrade_show_eos_hidden_missions()
{
    return upgrade_value_single_player(0x1B) != 0;
}

// 0x007763A0 (:1152)
bool upgrade_has_nano_rifle()
{
    return upgrade_value_single_player(0xF) != 0;
}

// 0x007763E0 (:1164)
bool upgrade_has_safehouse_warp()
{
    return upgrade_value_single_player(0x1D) != 0;
}

// 0x007764A0 (:1200)
bool upgrade_has_ore_bonus()
{
    return upgrade_value_single_player(0x1C) != 0;
}

// 0x00790E20 (:1212)
// Unlocks the levels this district's liberation gives and shows them; the screen lists at most three.
void upgrade_unlock_from_liberation(unsigned int district_handle)
{
    player_meta_data* meta = player_get_meta_data();
    if (meta == nullptr)
    {
        return;
    }
    int unlocked[3];
    int num_unlocked = 0;
    for (unsigned int id = 0; id < NUM_OF_UPGRADE_DEFINITIONS; id++)
    {
        const upgrade_definition_data* data = upgrade_get_data_by_index(id);
        if (data == nullptr)
        {
            continue;
        }
        // Magic number: 16 levels, upgrade_definition_data::levels.
        for (unsigned int level = 0; level < 16; level++)
        {
            const upgrade_level* lp = data->get_upgrade_level(level);
            if (lp == nullptr || lp->unlock_district_name == nullptr)
            {
                continue;
            }
            const unsigned int handle = get_district_by_name(lp->unlock_district_name);
            if (handle != 0xFFFFFFFF && handle == district_handle)
            {
                meta->unlock_upgrade(id, level, false);
                if (meta->is_next_upgrade_newly_unlocked(id) && num_unlocked < 3)
                {
                    unlocked[num_unlocked++] = (int)id;
                }
            }
        }
    }
    if (district_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(district_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_DISTRICT)
        {
            hud_ui_district_liberation_show(unlocked, num_unlocked, objp->get_handle(), LUT_LIBERATION, 0);
        }
    }
    meta->clear_unlock_upgrade_notify();
}

// 0x007764E0 (:1266)
// Original: a level only unlocks when the count is exactly its activity count.
bool upgrade_unlock_from_activities()
{
    stats_count_completed_activities();
    const tracked_stat* stat;
    stats_get_stat(stat, STATS_ACTIVITIES_COMPLETED);
    const unsigned int completed = (unsigned int)stat->value.integer;
    player_meta_data* meta = player_get_meta_data();
    if (meta != nullptr)
    {
        for (unsigned int id = 0; id < NUM_OF_UPGRADE_DEFINITIONS; id++)
        {
            const upgrade_definition_data* data = upgrade_get_data_by_index(id);
            if (data == nullptr)
            {
                continue;
            }
            for (unsigned int level = 0; level < 16; level++)
            {
                const upgrade_level* lp = data->get_upgrade_level(level);
                if (lp != nullptr && lp->activity_count != 0 && lp->activity_count == completed)
                {
                    meta->unlock_upgrade(id, level, false);
                }
            }
        }
    }
    return upgrade_show_new_upgrades(false, (int)completed);
}

// 0x0075F660 (:1298)
static bool should_show_upgrade(unsigned int upgrade_id, player_meta_data* meta_data)
{
    return meta_data->is_next_upgrade_available(upgrade_id) && meta_data->is_next_upgrade_newly_unlocked(upgrade_id);
}

// 0x0075F680 (:1333)
bool upgrade_show_new_upgrades(bool from_mission, int num_activities_completed)
{
    if (video_cutscene_pending_or_playing())
    {
        return false;
    }
    player_meta_data* meta = player_get_meta_data();
    if (meta == nullptr)
    {
        return false;
    }
    int unlocked[3];
    int num_unlocked = 0;
    for (unsigned int id = 0; id < NUM_OF_UPGRADE_DEFINITIONS; id++)
    {
        // Magic number: 0x27, the one upgrade that never shows (the golden sledgehammer).
        if (meta->get_next_upgrade_level(id) != nullptr && id != 0x27 && should_show_upgrade(id, meta) && num_unlocked < 3)
        {
            unlocked[num_unlocked++] = (int)id;
        }
    }
    bool shown = false;
    if (num_unlocked > 0)
    {
        // Missions show LUT_MISSION_UNLOCKS, and the type after it is the activity one.
        shown = hud_ui_district_liberation_show(unlocked, num_unlocked, 0xFFFFFFFF, (liberation_ui_type)(LUT_MISSION_UNLOCKS + (from_mission ? 0 : 1)), num_activities_completed);
    }
    meta->clear_unlock_upgrade_notify();
    return shown;
}

// 0x0075F750 (:1370)
void upgrade_unlock_free_dlc_upgrades()
{
    player_meta_data* meta = player_get_meta_data();
    if (meta == nullptr)
    {
        return;
    }
    for (unsigned int id = 0; id < NUM_OF_UPGRADE_DEFINITIONS; id++)
    {
        if (meta->get_next_upgrade_level(id) != nullptr && Upgrade_data[id].always_show())
        {
            meta->purchase_upgrade(id, false, true);
        }
    }
}
