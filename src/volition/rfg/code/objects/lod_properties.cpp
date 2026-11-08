// Original: volition/rfg/code/objects/lod_properties.cpp
//
// LOD property sets (decompiled; header comment in lod_properties.h), read from lod_properties.xtbl into an array
// from the given pool and found by name through LOD_props_table; an unknown name gives the "Default" entry.
// Compiled in rfg_objects_unity.cpp.
//
// Notes (decompiled behaviour, kept as is):
// - lpm_parse_table_file does not advance to the next entry when one fails to parse, so a bad entry loops forever.
// - lpm_system_init formats a memory report that is never printed.

#include "volition/rfg/code/objects/lod_properties.h"

#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/trace.hpp"

#include <stdio.h>
#include <string.h>

// ---- file statics
static str_data_hash_table_cached<LOD_properties*, 64, unsigned short> LOD_props_table; // 0x03045058
static LOD_properties* LOD_entries; // 0x03023674
static unsigned int Num_LOD_entries; // 0x0302F100
static unsigned int Names_size; // 0x0302F104, bytes of entry names
static LOD_properties* Default_LOD_properties; // 0x0302F108

// ---- LOD_properties (header inlines)
// 0x00A2C690 (.h:19)
LOD_properties::LOD_properties()
{
    m_name = nullptr;
    m_lod_off_dist = 0.0f;
}

// 0x00A05DA0 (.h:25)
void LOD_properties::clear()
{
    m_name = nullptr;
    m_lod_off_dist = 0.0f;
}

// 0x00A12F10 (:57)
// LTCG: LOD_prop in ECX, pool in EDX. The name is copied into the pool.
static bool lpm_parse_LOD_property(LOD_properties* LOD_prop, xml_element* root, mempool_base* pool)
{
    char name[32];
    LOD_prop->clear(); // inlined
    xml_table_get_req_string(name, sizeof(name), root, "Name");
    const unsigned int size = (unsigned int)strlen(name) + 1;
    LOD_prop->m_name = (char*)pool->alloc(size, 1);
    Names_size += size;
    strcpy(LOD_prop->m_name, name);
    xml_table_get_opt_float(LOD_prop->m_lod_off_dist, root, "lod_off_distance");
    return true;
}

// 0x00A8A740 (:87)
static bool lpm_parse_table_file(mempool_base* pool)
{
    xml_element* root = xml_table_parse("lod_properties.xtbl");
    Num_LOD_entries = xml_count(root, "LOD");
    if (Num_LOD_entries == 0)
    {
        return true;
    }
    LOD_entries = (LOD_properties*)pool->alloc(Num_LOD_entries * sizeof(LOD_properties), 0);
    Num_LOD_entries = 0;
    xml_element* element = xml_table_find_opt(root, "LOD");
    while (element != nullptr)
    {
        LOD_properties prop;
        if (!lpm_parse_LOD_property(&prop, element, pool))
        {
            keen::debug::nativePrintArgs("Failed to read in an LOD property!\n"); // see the notes
            continue;
        }
        LOD_properties* entry = &LOD_entries[Num_LOD_entries];
        *entry = prop;
        LOD_props_table.insert_key_data(prop.m_name, entry); // inlined add (the result is not checked)
        if (v_stricmp("Default", prop.m_name) == 0)
        {
            Default_LOD_properties = entry;
        }
        Num_LOD_entries++;
        element = xml_table_find_next(root, element, "LOD");
    }
    xml_table_close(root);
    return true;
}

// 0x00A8A880 (:144)
void lpm_system_init(mempool_base* pool)
{
    Default_LOD_properties = nullptr;
    if (!lpm_parse_table_file(pool))
    {
        return;
    }
    if (Default_LOD_properties == nullptr)
    {
        keen::debug::nativePrintArgs("Failed to find the \"Default\" LOD properties in the table file!\n");
        return;
    }
    char report[1024];
    _snprintf(report, sizeof(report),
              "LOD Properties memory: %s bytes used for the hash table, %s bytes for the names, and %s bytes for the entries themselves.",
              str_commas((unsigned int)sizeof(LOD_props_table)), str_commas(Names_size),
              str_commas(Num_LOD_entries * (unsigned int)sizeof(LOD_properties))); // see the notes
}

// 0x00A5E6B0 (:172)
const LOD_properties* lpm_get_properties(const char* property_entry_name)
{
    LOD_properties* props;
    if (LOD_props_table.get(property_entry_name, props))
    {
        return props;
    }
    return Default_LOD_properties;
}

// 0x00A12FC0 (:192)
const LOD_properties* lpm_get_default_properties()
{
    return Default_LOD_properties;
}
