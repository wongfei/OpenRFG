// Original: volition/rfg/code/spawn/ambient_spawn_behavior.cpp
//
// Ambient behaviours (NPC.md NP4): the groups of Ambient_spawn_behavior.xtbl and the placed region class. human.cpp picks a
// behaviour of a group when an ambient NPC is created. spawn_npc_navpoint searches the regions and then uses group 2 anyway.

#include "volition/rfg/code/spawn/ambient_spawn_behavior.h"

#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/spawn/ambient_prop_info.h"
#include "volition/vlib/code/memory/object_pool.inl"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"

// Magic numbers: rfg.exe's array sizes, and the size of a group's name.
static const int MAX_AMBIENT_BEHAVIOR_GROUPS = 20;
static const int MAX_AMBIENT_BEHAVIORS_PER_GROUP = 20;
static const int AMBIENT_BEHAVIOR_NAME_SIZE = 24;

static int Num_ambient_behavior_groups; // 0x0373EFD4
static ambient_behavior_group Ambient_behavior_groups[MAX_AMBIENT_BEHAVIOR_GROUPS]; // 0x0373F018

static object_pool<object_ambient_behavior_region, 20, 1> object_ambient_behavior_regionpool; // 0x037449F0

// 0x00B4C070 (:32)
void ambient_behavior_system_init()
{
    Num_ambient_behavior_groups = 0;
    xml_element* root = xml_table_parse("Ambient_spawn_behavior.xtbl");
    if (root == nullptr)
    {
        return;
    }
    for (xml_element* group_xml = xml_table_find_opt(root, "Behaviors"); group_xml != nullptr && Num_ambient_behavior_groups < MAX_AMBIENT_BEHAVIOR_GROUPS; group_xml = xml_table_find_next(root, group_xml, "Behaviors"))
    {
        ambient_behavior_group& group = Ambient_behavior_groups[Num_ambient_behavior_groups];
        xml_table_get_req_string(group.name, AMBIENT_BEHAVIOR_NAME_SIZE, group_xml, "Name");
        group.num_behaviors = 0;
        xml_element* list_xml = xml_table_find_req(group_xml, "Behavior_List");
        for (xml_element* behavior_xml = xml_table_find_opt(list_xml, "Behavior"); behavior_xml != nullptr && group.num_behaviors < MAX_AMBIENT_BEHAVIORS_PER_GROUP; behavior_xml = xml_table_find_next(list_xml, behavior_xml, "Behavior"))
        {
            ambient_behavior& behavior = group.behaviors[group.num_behaviors];
            const char* animation = xml_table_get_opt_string_ref(behavior_xml, "Animation_Override");
            behavior.variant_anim_set = animation != nullptr && animation[0] != '\0' ? vanim_get_animation_set_variant_from_name(animation) : ANIM_SET_VARIANT_NONE;
            const char* prop = xml_table_get_opt_string_ref(behavior_xml, "Ambient_Prop");
            behavior.amb_prop_info = prop != nullptr && prop[0] != '\0' ? amb_prop_info_find(prop) : nullptr;
            xml_table_get_req_int(behavior.occurance_chance, behavior_xml, "Chance");
            xml_table_get_req_int(behavior.day_start, behavior_xml, "Day_Start");
            xml_table_get_req_int(behavior.day_end, behavior_xml, "Day_End");
            xml_table_get_req_float(behavior.male_female_ratio, behavior_xml, "MaleFemaleRatio");
            xml_table_get_req_float(behavior.morale_min, behavior_xml, "Morale_Min");
            xml_table_get_req_float(behavior.morale_max, behavior_xml, "Morale_Max");
            // Magic number: the table's ratio runs from -1 to 1; human.cpp uses this one, from 0 to 2, and 2 minus it for men.
            behavior.male_female_ratio += 1.0f;
            group.num_behaviors++;
        }
        Num_ambient_behavior_groups++;
    }
    xml_table_close(root);
}

// 0x00B40E10 (:74)
int ambient_behavior_get_id(char* group_name)
{
    for (int i = 0; i < Num_ambient_behavior_groups; i++)
    {
        if (v_stricmp(Ambient_behavior_groups[i].name, group_name) == 0)
        {
            return i;
        }
    }
    return -1;
}

// 0x00B40E50 (:85)
// Original: no range check. A region that named no known group has id -1.
ambient_behavior_group* ambient_behavior_from_id(int id)
{
    return &Ambient_behavior_groups[id];
}

// 0x00B40E70 (:91)
void types_object_ambient_behavior_region(signed char& type, signed char& subtype)
{
    type = OT_AMBIENT_SPAWN_REGION;
    subtype = -1;
}

// 0x00B40E80 (:91)
int bpo_index_object_ambient_behavior_region()
{
    return 1;
}

// 0x00B40E90 (:91)
const char* classname_object_ambient_behavior_region()
{
    return "object_ambient_behavior_region";
}

// 0x00B4C2C0 (:91)
object* alloc_object_ambient_behavior_region()
{
    return object_ambient_behavior_regionpool.alloc(0);
}

// Magic numbers: the pool usage and size, as registered at 0x0108C330.
static obj_class_instantiator object_ambient_behavior_region_instantiator("object_ambient_behavior_region", alloc_object_ambient_behavior_region, types_object_ambient_behavior_region, bpo_index_object_ambient_behavior_region, classname_object_ambient_behavior_region, object_ambient_behavior_region::pool_remaining, 0xD80, 20); // 0x0374A4E4

// 0x00B40EA0 (:91)
unsigned int object_ambient_behavior_region::get_classname_hash()
{
    return object_ambient_behavior_region_instantiator.this_hash;
}

// 0x00B46F60 (:91)
int object_ambient_behavior_region::pool_remaining()
{
    return object_ambient_behavior_regionpool.get_free();
}

// 0x00B54A90 (:91)
void object_ambient_behavior_region::pool_free()
{
    object_ambient_behavior_regionpool.free(this);
}

// 0x00B40EB0 (:92)
object_ambient_behavior_region::object_ambient_behavior_region()
{
}

// 0x00B40EE0
object_ambient_behavior_region::~object_ambient_behavior_region()
{
}

// 0x00B40D80 (.h:48)
const object_type object_ambient_behavior_region::get_type_static()
{
    return OT_AMBIENT_SPAWN_REGION;
}

// 0x00B40D90 (.h:48)
const object_sub_type object_ambient_behavior_region::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00B40DA0 (.h:48)
const char* object_ambient_behavior_region::get_classname_static()
{
    return "object_ambient_behavior_region";
}

// 0x00B40DB0 (.h:48)
int object_ambient_behavior_region::get_bpo_index_static()
{
    return 1;
}

// 0x00B40ED0 (.h:48)
const char* object_ambient_behavior_region::get_classname() const
{
    return "object_ambient_behavior_region";
}

// 0x00B4C2D0 (:96)
bool object_ambient_behavior_region::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (load)
    {
        char name[128];
        pblock.get(AMBIENT_BEHAVIOR_NAME, name);
        ambient_group_id = ambient_behavior_get_id(name);
        pblock.get_optional<float>(AMBIENT_BEHAVIOR_RADIUS, radius);
    }
    return true;
}

// 0x00B40EF0 (:115)
bool object_ambient_behavior_region::stream(bool load)
{
    return object::stream(load);
}
