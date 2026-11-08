// Original: volition/rfg/code/gamewide/tweak_table.cpp
//
// Tweak table (see tweak_table.h): items register themselves (by name, first one wins) when constructed;
// tweak_table_load overrides their values from tweak_table.xtbl (or the DLC bundle's) and online_tweak_table.xtbl.
//
// Partial: the console commands unused_dcf_tweak_table and unused_dcf_reload_tweak_table are not implemented yet.

#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/varray.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <stdio.h>

// 0x007EB110 (:39)
static farray<tweak_table_item*, 350>& get_tweak_table()
{
    // A function-local static so items constructed during static initialisation can register.
    static farray<tweak_table_item*, 350> The_tweak_table; // 0x0253CE40
    return The_tweak_table;
}

// 0x007EB160 (:51)
static int tweak_table_find(const char* name)
{
    farray<tweak_table_item*, 350>& table = get_tweak_table();
    for (int i = 0; i < table.size(); ++i)
    {
        if (v_stricmp(table[i]->get_name(), name) == 0)
        {
            return i;
        }
    }
    return -1;
}

// 0x007C9160 (:62)
static bool tweak_table_sort_function(tweak_table_item* const& item1, tweak_table_item* const& item2)
{
    return v_stricmp(item1->get_name(), item2->get_name()) < 0;
}

// 0x007C9190 (:67)
static const char* get_tweak_table_file_name(int dlc_bundle_id, char* name_buffer)
{
    // "dlcNN_tweak_table.xtbl" when that file exists, otherwise the base table.
    if (dlc_bundle_id == -1)
    {
        return "tweak_table.xtbl";
    }
    sprintf(name_buffer, "dlc%02d_%s", dlc_bundle_id, "tweak_table.xtbl");
    return cf_exist(name_buffer) ? name_buffer : "tweak_table.xtbl";
}

// 0x007EF0C0 (:178)
tweak_table_item::tweak_table_item(const char* in_name, bool* in_value)
{
    init(in_name, in_value, TWEAK_VALUE_TYPE_BOOL);
}

// 0x007EF0E0 (:183)
tweak_table_item::tweak_table_item(const char* in_name, int* in_value)
{
    init(in_name, in_value, TWEAK_VALUE_TYPE_INT);
}

// 0x007EF100 (:188)
tweak_table_item::tweak_table_item(const char* in_name, float* in_value)
{
    init(in_name, in_value, TWEAK_VALUE_TYPE_FLOAT);
}

// 0x007EB460 (:194)
void tweak_table_item::init(const char* in_name, void* in_value, tweak_value_type in_type)
{
    name = in_name;
    value = in_value;
    type = in_type;
    if (tweak_table_find(in_name) == -1)
    {
        farray<tweak_table_item*, 350>& table = get_tweak_table();
        if (table.size() < 350)
        {
            table.add(this);
        }
    }
}

// 0x007C91D0 (:211)
float tweak_table_item::get_value_float() const
{
    switch (type)
    {
    case TWEAK_VALUE_TYPE_BOOL:
        return (float)*(unsigned char*)value;
    case TWEAK_VALUE_TYPE_FLOAT:
        return *(float*)value;
    case TWEAK_VALUE_TYPE_INT:
        return (float)*(int*)value;
    default:
        VLIB_CRITICAL_FAILURE();
        return 0.0f;
    }
}

// 0x007C9220 (:232)
void tweak_table_item::update_value(float new_value) const
{
    switch (type)
    {
    case TWEAK_VALUE_TYPE_BOOL:
        *(bool*)value = new_value != 0.0f;
        break;
    case TWEAK_VALUE_TYPE_FLOAT:
        *(float*)value = new_value;
        break;
    case TWEAK_VALUE_TYPE_INT:
        *(int*)value = (int)new_value;
        break;
    default:
        VLIB_CRITICAL_FAILURE();
        break;
    }
}

// 0x007C9290 (:254)
void tweak_table_item::get_string(char* buffer) const
{
    const double current = get_value_float(); // inlined in the original
    if (type == TWEAK_VALUE_TYPE_BOOL)
    {
        sprintf(buffer, "%s = %.0f (bool)", name, current);
    }
    else if (type == TWEAK_VALUE_TYPE_INT)
    {
        sprintf(buffer, "%s = %.0f (int)", name, current);
    }
    else
    {
        sprintf(buffer, "%s = %.3f (float)", name, current);
    }
}

// 0x007B9EC0 (.h:52)
const char* tweak_table_item::get_name() const
{
    return name;
}

// 0x007B9ED0 (.h:53)
tweak_table_item::tweak_value_type tweak_table_item::get_type() const
{
    return type;
}

// 0x007EB4D0 (:275)
static void tweak_parse_xml(xml_element* root, bool* match_found_array)
{
    // Every <Tweak_Table_Entry> naming a registered item sets it to <Value>. match_found_array is unused.
    if (root == nullptr)
    {
        return;
    }
    farray<tweak_table_item*, 350>& table = get_tweak_table();
    for (xml_element* entry = xml_table_find_opt(root, "Tweak_Table_Entry"); entry != nullptr; entry = xml_table_find_next(root, entry, "Tweak_Table_Entry"))
    {
        char name[256];
        xml_table_get_req_string(name, sizeof(name), entry, "Name");
        const int index = tweak_table_find(name);
        if (index >= 0)
        {
            float new_value = 0.0f;
            xml_table_get_req_float(new_value, entry, "Value");
            table[index]->update_value(new_value);
        }
    }
}

// 0x007EF120 (:325)
void tweak_table_init()
{
    tweak_table_load(-1);
}

// 0x007EB5A0 (:330)
void tweak_parse_xml(xml_element* root)
{
    tweak_parse_xml(root, nullptr);
}

// 0x007EB5B0 (:336)
void tweak_table_load(int dlc_bundle_id)
{
    // online_tweak_table.xtbl is only applied when the main table parses.
    char name_buffer[256];
    const char* filename = get_tweak_table_file_name(dlc_bundle_id, name_buffer); // inlined in the original
    if (filename == nullptr)
    {
        return;
    }
    xml_element* root = xml_table_parse(filename);
    if (root == nullptr)
    {
        return;
    }
    tweak_parse_xml(root, nullptr);
    xml_table_close(root);
    root = xml_table_parse("online_tweak_table.xtbl");
    tweak_parse_xml(root, nullptr);
    xml_table_close(root);
}

// 0x007EB660 (:389)
// The online tweaks from the multiplayer settings files (the call passes no match array: the argument is unused and
// the compiler pushes none).
void multi_game_online_tweaks_init()
{
    xml_element* root = multi_game_settings_retrieve_settings_file("online_tweak_table.xtbl");
    tweak_parse_xml(root, nullptr);
    xml_table_close(root);
}
