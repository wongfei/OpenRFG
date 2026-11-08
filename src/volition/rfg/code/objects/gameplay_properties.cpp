// Original: volition/rfg/code/objects/gameplay_properties.cpp
//
// Gameplay property sets (decompiled; header comment in gameplay_properties.h), read from gameplay_properties.xtbl
// (or dlcNN_gameplay_properties.xtbl) into one array from the given pool and found by name through
// Gameplay_props_table; names live in Gameplay_properties_name_string_pool (22 KB). An unknown name gives the
// "Default" entry. Compiled in rfg_objects_unity.cpp (object_mover::serialize reads Default_gameplay_properties).
//
// Notes (decompiled behaviour, kept as is):
// - gpm_parse_table_file does not advance to the next entry when one fails to parse, so a bad entry loops forever.
// - gpm_parse_gameplay_property copies the name into the pooled string again after add_unique.
// - gpm_dlc_shutdown re-reads the base table into the existing array.

#include "volition/rfg/code/objects/gameplay_properties.h"

#include "volition/rfg/code/gameplay/gameflow/gameflow_common.h"
#include "volition/rfg/code/objects/lod_properties.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/salvage.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/trace.hpp"

#include <stdio.h>
#include <string.h>

static const unsigned int GAMEPLAY_PROPERTIES_NAMES_SIZE = 0x5800; // 22 KB of names

// ---- file statics
static str_data_hash_table_cached<gameplay_properties*, 2048, unsigned short> Gameplay_props_table; // 0x03500C98
static string_pool Gameplay_properties_name_string_pool("Gameplay Property Names"); // 0x030664F0
static unsigned int Num_gameplay_property_entries; // 0x0302F030
static unsigned int Max_gameplay_property_entries; // 0x0302F034
static gameplay_properties* Gameplay_properties; // 0x0302F038
static char* Gameplay_properties_name_string_pool_data; // 0x0302F03C
static gameplay_properties* Default_gameplay_properties; // 0x0302F040

// ---- gameplay_properties (header inlines)
// 0x00A2C6A0 (.h:33)
gameplay_properties::gameplay_properties()
{
    memset(this, 0, sizeof(*this));
}

// 0x00A05DB0 (.h:39)
void gameplay_properties::clear()
{
    memset(this, 0, sizeof(*this));
}

// 0x00A529B0 (:67)
// LTCG: table in ESI. The flags named by the <Flag> children (unknown names add nothing).
static void get_flags(xml_element* root, int& flags, str_data_hash_table<unsigned int, 32, 32, unsigned char>& table)
{
    flags = 0;
    for (xml_element* cur = xml_find(root, "Flag"); cur != nullptr; cur = xml_find_next(root, cur, "Flag"))
    {
        if (cur->text != nullptr)
        {
            unsigned int flag = 0;
            table.get(cur->text, flag); // inlined find
            flags |= flag;
        }
    }
}

// 0x00A48230, 0x00A481E0 (:92)
// LTCG: root in ECX, default_val in EAX. The enum value named by the element's text, or default_val.
template<typename T>
static T get_enum(xml_element* root, str_data_hash_table<unsigned int, 32, 32, unsigned char>& table, T default_val)
{
    if (root != nullptr && root->text != nullptr)
    {
        unsigned int value;
        if (table.get(root->text, value))
        {
            return (T)value;
        }
    }
    return default_val;
}

// 0x00A7AB80 (:113)
// A <Gameplay> entry; the percentages are given in percent (-1 = unset), the monetary value defaults to -1.
static bool gpm_parse_gameplay_property(gameplay_properties* obj_prop, xml_element* root, mempool_base* pool)
{
    char name[32];
    memset(obj_prop, 0, sizeof(*obj_prop));
    xml_table_get_req_string(name, sizeof(name), root, "Name");
    obj_prop->name = Gameplay_properties_name_string_pool.add_unique(name);
    if (obj_prop->name == nullptr)
    {
        return false;
    }
    strcpy(obj_prop->name, name); // see the notes
    xml_element* element = xml_find(root, OM_PROP_CHUNK_FLAGS.str);
    if (element != nullptr)
    {
        get_flags(element, obj_prop->chunk_flags, Chunk_flags_table);
    }
    element = xml_find(root, OM_PROP_IMPORTANCE.str);
    if (element != nullptr)
    {
        obj_prop->importance = get_enum(element, Object_importance_table, obj_prop->importance);
    }
    const char* team = xml_get_string(xml_find(root, OM_PROP_TEAM.str), nullptr);
    if (team != nullptr)
    {
        obj_prop->team = human_team_from_name(team);
    }
    xml_table_get_opt_float(obj_prop->game_destroyed_pct, root, RFGM_PROP_GAME_DESTROYED_PCT.str);
    if (obj_prop->game_destroyed_pct != -1.0f)
    {
        obj_prop->game_destroyed_pct /= 100.0f;
    }
    xml_table_get_opt_float(obj_prop->fully_destroyed_pct, root, RFGM_PROP_FULLY_DESTROYED_PCT.str);
    if (obj_prop->fully_destroyed_pct != -1.0f)
    {
        obj_prop->fully_destroyed_pct /= 100.0f;
    }
    element = xml_find(root, RFGM_PROP_SALVAGE_MATERIAL.str);
    if (element != nullptr)
    {
        obj_prop->salvage_material = get_enum(element, Salvage_material_name_table, obj_prop->salvage_material);
    }
    xml_table_get_opt_int(obj_prop->salvage_num_pieces, root, RFGM_PROP_SALVAGE_NUM_PIECES.str);
    xml_table_get_opt_float(obj_prop->control, root, OM_PROP_CONTROL.str);
    obj_prop->monetary_value = -1.0f;
    xml_table_get_opt_float(obj_prop->monetary_value, root, "value");
    const char* lod_name = xml_table_get_opt_string_ref(root, "LOD_Properties");
    obj_prop->lod_properties = lod_name != nullptr ? lpm_get_properties(lod_name) : lpm_get_default_properties(); // inlined
    return true;
}

// 0x00A7AD90 (:193)
// LTCG: pool in ECX. The array is allocated by the first read; a later (DLC) table must fit into it.
static bool gpm_parse_table_file(mempool_base* pool, int dlc_id)
{
    const char* filename = "gameplay_properties.xtbl";
    char dlc_filename[68];
    if (dlc_id != -1)
    {
        sprintf(dlc_filename, "dlc%02d_%s", dlc_id, filename);
        if (!cf_exist(dlc_filename))
        {
            return true;
        }
        filename = dlc_filename;
    }
    xml_element* root = xml_table_parse(filename);
    Num_gameplay_property_entries = xml_count(root, "Gameplay");
    if (Num_gameplay_property_entries == 0)
    {
        return true;
    }
    if (Gameplay_properties == nullptr)
    {
        if (pool == nullptr)
        {
            return false;
        }
        Max_gameplay_property_entries = Num_gameplay_property_entries;
        Gameplay_properties = (gameplay_properties*)pool->alloc(Num_gameplay_property_entries * sizeof(gameplay_properties), 0);
    }
    else if (Max_gameplay_property_entries < Num_gameplay_property_entries)
    {
        xml_table_close(root);
        return false;
    }
    Num_gameplay_property_entries = 0;
    xml_element* element = xml_table_find_opt(root, "Gameplay");
    while (element != nullptr)
    {
        gameplay_properties prop;
        if (!gpm_parse_gameplay_property(&prop, element, nullptr))
        {
            keen::debug::nativePrintArgs("Failed to read in an object property!\n"); // see the notes
            continue;
        }
        gameplay_properties* entry = &Gameplay_properties[Num_gameplay_property_entries];
        *entry = prop;
        if (!Gameplay_props_table.insert_key_data(prop.name, entry))
        {
            xml_table_close(root);
            return false;
        }
        if (v_stricmp("Default", prop.name) == 0)
        {
            Default_gameplay_properties = entry;
        }
        Num_gameplay_property_entries++;
        element = xml_table_find_next(root, element, "Gameplay");
    }
    xml_table_close(root);
    return true;
}

// 0x00A995C0 (:279)
void gpm_dlc_shutdown()
{
    Default_gameplay_properties = nullptr;
    Gameplay_props_table.clear();
    if (Gameplay_properties_name_string_pool_data != nullptr)
    {
        Gameplay_properties_name_string_pool.set_thread_ownership();
        Gameplay_properties_name_string_pool.clear();
        if (gpm_parse_table_file(nullptr, -1) && Default_gameplay_properties == nullptr)
        {
            keen::debug::nativePrintArgs("Failed to find the \"Default\" gameplay properties in the table file!\n");
        }
    }
}

// 0x00A7AF80 (:288)
void gpm_system_init(mempool_base* pool, int dlc_id)
{
    Default_gameplay_properties = nullptr;
    Gameplay_props_table.clear();
    if (Gameplay_properties_name_string_pool_data == nullptr)
    {
        if (pool == nullptr)
        {
            return;
        }
        Gameplay_properties_name_string_pool_data = (char*)pool->alloc(GAMEPLAY_PROPERTIES_NAMES_SIZE, 0);
        if (Gameplay_properties_name_string_pool_data == nullptr)
        {
            return;
        }
        Gameplay_properties_name_string_pool.init(Gameplay_properties_name_string_pool_data, GAMEPLAY_PROPERTIES_NAMES_SIZE, "Gameplay Property Names", 0, 0);
    }
    Gameplay_properties_name_string_pool.set_thread_ownership();
    Gameplay_properties_name_string_pool.clear();
    if (gpm_parse_table_file(pool, dlc_id) && Default_gameplay_properties == nullptr)
    {
        keen::debug::nativePrintArgs("Failed to find the \"Default\" gameplay properties in the table file!\n");
    }
}

// 0x00A52A80 (:323)
const gameplay_properties* gpm_get_properties(const char* property_entry_name)
{
    gameplay_properties* props;
    if (property_entry_name != nullptr && Gameplay_props_table.get(property_entry_name, props))
    {
        return props;
    }
    return Default_gameplay_properties;
}
