// Original: volition/rfg/code/gameplay/activities/activity_spawn.cpp

#include "volition/rfg/code/gameplay/activities/activity_spawn.h"

#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/level/world.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"

// Magic number: 12 districts' worth of entries, size from rfg.pdb.
static activity_spawn_properties Activity_info_list[12]; // 0x02271A88

// 0x00769270 (.h:17)
activity_spawn_property::activity_spawn_property()
{
    reset();
}

// 0x00754780 (.h:31)
void activity_spawn_property::reset()
{
    desired_mode = ACTIVITY_MODE_NONE;
    control_percentage = 0.0f;
    activity_flags.completed = 0;
}

// 0x0077EDA0 (.h:43)
activity_spawn_properties::activity_spawn_properties()
{
    reset();
}

// 0x00785A30
activity_spawn_properties::~activity_spawn_properties()
{
}

// 0x00769290 (.h:55)
void activity_spawn_properties::reset()
{
    district_handle = 0xFFFFFFFF;
    activation_levels.clear();
}

// 0x007692A0 (.h:61)
bool activity_spawn_properties::in_use() const
{
    return district_handle != 0xFFFFFFFF;
}

// 0x00785A40 (:40)
unsigned int get_district_from_name(const char* district_name)
{
    for (int i = 0; i < World.get_num_objects<district>(); i++)
    {
        object* objp = World.get_object<district>(i);
        if (v_stricmp(objp->get_name(), district_name) == 0)
        {
            return objp->get_handle();
        }
    }
    return 0xFFFFFFFF;
}

// 0x00770900 (:54). Original: an entry with levels but no district ends the search.
activity_spawn_properties* get_spawn_props_from_handle(unsigned int district_handle)
{
    for (int i = 0; i < 12; i++)
    {
        activity_spawn_properties& props = Activity_info_list[i];
        if (props.district_handle == 0xFFFFFFFF && props.activation_levels.size() > 0)
        {
            return nullptr;
        }
        if (district_handle == props.district_handle)
        {
            return &props;
        }
    }
    return nullptr;
}

// 0x0075A0C0 (:78)
bool activity_spawn_property::load(xml_element* xml_entry)
{
    reset();
    xml_element* e = xml_find(xml_entry, "activity_name");
    if (e == nullptr)
    {
        return false;
    }
    desired_mode = activity_get_mode_from_name(e->text);
    if ((unsigned int)desired_mode >= NUM_OF_ACTIVITY_MODES)
    {
        return false;
    }
    e = xml_find(xml_entry, "control_level");
    if (e == nullptr)
    {
        return false;
    }
    control_percentage = xml_get_float(e, -1.0f);
    return control_percentage >= 0.0f && control_percentage <= 1.0f;
}

// 0x00785AC0 (:105)
bool activity_spawn_properties::load(xml_element* xml_root)
{
    activity_spawn_property prop;
    reset();
    xml_element* e = xml_find(xml_root, "Name");
    if (e == nullptr)
    {
        return false;
    }
    district_handle = get_district_from_name(e->text);
    if (district_handle == 0xFFFFFFFF)
    {
        return false;
    }
    xml_element* list = xml_find(xml_root, "activity_list");
    if (list == nullptr)
    {
        return false;
    }
    for (xml_element* entry = xml_find(list, "activity_info"); entry != nullptr; entry = xml_find_next(list, entry, "activity_info"))
    {
        if (prop.load(entry))
        {
            if (activation_levels.size() >= activation_levels.capacity())
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                break;
            }
            activation_levels.add(prop);
        }
    }
    return activation_levels.size() > 0;
}

// 0x00799D40 (:149)
activity_spawn_properties* activity_get_spawn_props(const vector& pos)
{
    const district* dp = district_get_from_pos(pos);
    return dp == nullptr ? nullptr : get_spawn_props_from_handle(dp->get_handle());
}

// 0x00770940 (:163)
activity_spawn_properties* activity_get_spawn_props(unsigned int district_handle)
{
    return get_spawn_props_from_handle(district_handle); // rfg.exe inlines it
}

// 0x00785BD0 (:171)
void activity_spawn_level_init_post()
{
    for (int i = 0; i < 12; i++)
    {
        Activity_info_list[i].reset();
    }
    xml_element* root = xml_table_parse("activity_spawn.xtbl");
    if (root == nullptr)
    {
        return;
    }
    int count = 0;
    for (xml_element* entry = xml_find(root, "activity_spawn_info"); entry != nullptr; entry = xml_find_next(root, entry, "activity_spawn_info"))
    {
        if (Activity_info_list[count].load(entry))
        {
            count++;
            if (count >= 12)
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                break;
            }
        }
        else
        {
            Activity_info_list[count].reset();
        }
    }
    xml_table_close(root);
}
