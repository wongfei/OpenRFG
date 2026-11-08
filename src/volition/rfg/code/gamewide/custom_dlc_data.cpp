// Original: volition/rfg/code/gamewide/custom_dlc_data.cpp
//
// Custom DLC values (stub_impl.md S5): gameplay variables registered by name (static custom_dlc_bool/int/float
// objects in the files that own the variables) that a DLC bundle's dlcNN_custom_dlc_values.xtbl overrides for its
// levels; level shutdown (and the next level init) puts every variable back to its value at registration.

#include "volition/rfg/code/gamewide/custom_dlc_data.h"

#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/varray.h"

#include <stdio.h>

static farray<custom_dlc_value*, 16>& get_custom_dlc_value_array(); // 0x007E8A60 (:27)
static custom_dlc_value* find_value_entry(const char* in_name); // 0x007E8AB0 (:44)
static void load_xml_value(xml_element* xml_entry_root); // 0x007E8B60 (:66)

// 0x007E8A60 (:27)
static farray<custom_dlc_value*, 16>& get_custom_dlc_value_array()
{
    // A function-local static so values constructed during static initialisation can register (guard _S5).
    static farray<custom_dlc_value*, 16> The_custom_dlc_values;
    return The_custom_dlc_values;
}

// 0x007E8AB0 (:44)
static custom_dlc_value* find_value_entry(const char* in_name)
{
    farray<custom_dlc_value*, 16>& values = get_custom_dlc_value_array(); // inlined in the binary
    const unsigned int crc = crc_stri(in_name, 0);
    for (int i = 0; i < values.size(); i++)
    {
        if (values[i]->is_matching_name(in_name, crc)) // inlined in the binary
        {
            return values[i];
        }
    }
    return nullptr;
}

// 0x007E8B60 (:66)
// LTCG: xml_entry_root in EDI. A <custom_data> entry: the registered value its <Name> names takes its <Value>.
static void load_xml_value(xml_element* xml_entry_root)
{
    if (xml_entry_root == nullptr)
    {
        return;
    }
    xml_element* name = xml_find(xml_entry_root, "Name");
    if (name == nullptr)
    {
        return;
    }
    custom_dlc_value* value = find_value_entry(name->text);
    if (value == nullptr)
    {
        return;
    }
    xml_element* new_value = xml_find(xml_entry_root, "Value");
    if (new_value != nullptr)
    {
        value->assign_new_value(new_value);
    }
}

// 0x007E8BB0 (:95)
// The first value of a name registers (at most 16).
custom_dlc_value::custom_dlc_value(const char* in_name)
{
    value_name = in_name;
    value_crc = crc_stri(in_name, 0);
    if (find_value_entry(in_name) == nullptr)
    {
        farray<custom_dlc_value*, 16>& values = get_custom_dlc_value_array();
        if (values.size() < 16)
        {
            values.add(this);
        }
    }
}

// 0x007B9D90 (:114)
bool custom_dlc_value::is_matching_name(const char* test_value_name, unsigned int test_value_crc) const
{
    if (test_value_name == nullptr || test_value_crc != value_crc)
    {
        return false;
    }
    return v_stricmp(test_value_name, value_name) == 0;
}

// 0x007E8C30 (:131)
custom_dlc_bool::custom_dlc_bool(const char* in_name, bool& in_value) : custom_dlc_value(in_name), original_in_value(in_value), value_ptr(&in_value)
{
}

// 0x007B9DC0 (:137)
void custom_dlc_bool::reset()
{
    *value_ptr = original_in_value;
}

// 0x007B9DD0 (:144)
// Read as a number: non-zero is true.
void custom_dlc_bool::assign_new_value(xml_element* xml_value)
{
    reset();
    if (xml_value != nullptr)
    {
        *value_ptr = xml_get_float(xml_value, (float)original_in_value) != 0.0f;
    }
}

// 0x007E8C60 (:169)
custom_dlc_int::custom_dlc_int(const char* in_name, int& in_value) : custom_dlc_value(in_name), original_in_value(in_value), value_ptr(&in_value)
{
}

// 0x007B9E20 (:175)
void custom_dlc_int::reset()
{
    *value_ptr = original_in_value;
}

// 0x007B9E30 (:182)
// Read as a float and truncated.
void custom_dlc_int::assign_new_value(xml_element* xml_value)
{
    reset();
    if (xml_value != nullptr)
    {
        *value_ptr = (int)xml_get_float(xml_value, (float)original_in_value);
    }
}

// 0x007E8C90 (:206)
custom_dlc_float::custom_dlc_float(const char* in_name, float& in_value) : custom_dlc_value(in_name), original_in_value(in_value), value_ptr(&in_value)
{
}

// 0x007B9E70 (:212)
void custom_dlc_float::reset()
{
    *value_ptr = original_in_value;
}

// 0x007B9E80 (:219)
void custom_dlc_float::assign_new_value(xml_element* xml_value)
{
    reset();
    if (xml_value != nullptr)
    {
        *value_ptr = xml_get_float(xml_value, original_in_value);
    }
}

// 0x007ECDA0 (:236)
// Back to the registered values, then the bundle's table (if it has one) applies its entries.
void custom_dlc_data_level_init(int dlc_bundle_id)
{
    custom_dlc_data_level_shutdown();
    if (dlc_bundle_id < 0)
    {
        return;
    }
    char filename[256];
    sprintf(filename, "dlc%02d_%s", dlc_bundle_id, "custom_dlc_values.xtbl");
    if (!cf_exist(filename))
    {
        return;
    }
    xml_element* root = xml_table_parse(filename);
    if (root == nullptr)
    {
        return;
    }
    for (xml_element* cur = xml_find(root, "custom_data"); cur != nullptr; cur = xml_find_next(root, cur, "custom_data"))
    {
        load_xml_value(cur);
    }
    xml_table_close(root);
}

// 0x007E8CC0 (:272)
void custom_dlc_data_level_shutdown()
{
    farray<custom_dlc_value*, 16>& values = get_custom_dlc_value_array(); // inlined in the binary
    for (int i = 0; i < values.size(); i++)
    {
        values[i]->reset();
    }
}
