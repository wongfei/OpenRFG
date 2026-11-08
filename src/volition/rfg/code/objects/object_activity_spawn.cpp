// Original: volition/rfg/code/objects/object_activity_spawn.cpp
//
// gameflow.md GF4: ties an activity's start position to its activity node. A stand-in of get_type_static would have
// find_activity_spawn_node walk humans. Registers the class, so zones now create these objects.

#include "volition/rfg/code/objects/object_activity_spawn.h"

#include "volition/rfg/code/gameplay/activities/base_activity_data.h"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object_area_defense_node.h"
#include "volition/rfg/code/objects/object_house_arrest_node.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/object_raid_node.h"
#include "volition/vlib/code/memory/object_pool.h"

static object_pool<object_activity_spawn, 36, 1> object_activity_spawnpool; // 0x03543A00

// <object_raid_node> 0x00A4A100, <object_house_arrest_node> 0x00A4A160, <object_area_defense_node> 0x00A4A1C0 (:26)
template<typename T>
static const T* find_activity_node(const base_activity_data* activity_data)
{
    if (activity_data == nullptr)
    {
        return nullptr;
    }
    for (int i = 0; i < World.get_num_objects<T>(); i++)
    {
        const T* node = World.get_object<T>(i);
        if (node->get_base_data() == activity_data)
        {
            return node;
        }
    }
    return nullptr;
}

// 0x00A39280 (:42). The first raid node below the parent, or the parent.
const object_raid_node* find_low_level_raid_object(const object_raid_node* raid_parent_objp)
{
    if (raid_parent_objp == nullptr)
    {
        return nullptr;
    }
    const object* first = raid_parent_objp->child_ptr;
    for (const object* child = first; child != nullptr; )
    {
        if (child->get_obj_type() == OT_RAID_NODE)
        {
            return static_cast<const object_raid_node*>(child);
        }
        child = child->child_next;
        if (child == first)
        {
            break;
        }
    }
    return raid_parent_objp;
}

// 0x00A60BF0 (:61). rfg.exe: LTCG, activity_data in ESI.
static const object* find_activity_node(const base_activity_data* activity_data)
{
    const object_raid_node* raid = find_activity_node<object_raid_node>(activity_data);
    if (raid != nullptr)
    {
        return find_low_level_raid_object(raid); // rfg.exe inlines it
    }
    const object_house_arrest_node* house = find_activity_node<object_house_arrest_node>(activity_data);
    if (house != nullptr)
    {
        return house;
    }
    return find_activity_node<object_area_defense_node>(activity_data);
}

// 0x00A392B0 (:87). rfg.exe: LTCG, the handle in ESI.
static bool activity_has_been_completed(const unsigned int activity_node_handle)
{
    if (activity_node_handle == 0xFFFFFFFF)
    {
        return false;
    }
    const object* objp = World.object_from_handle(activity_node_handle);
    if (objp == nullptr)
    {
        return false;
    }
    if (objp->get_obj_type() == OT_RAID_NODE)
    {
        return static_cast<const object_raid_node*>(objp)->get_completed_flag();
    }
    if (objp->get_obj_type() == OT_HOUSE_ARREST_NODE)
    {
        return static_cast<const object_house_arrest_node*>(objp)->get_completed_flag();
    }
    return false;
}

// 0x00A15840 (:113)
void types_object_activity_spawn(signed char& type, signed char& subtype)
{
    type = OT_ACTIVITY_SPAWN;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A15850 (:113)
int bpo_index_object_activity_spawn()
{
    return 1;
}

// 0x00A15860 (:113)
const char* classname_object_activity_spawn()
{
    return "object_activity_spawn";
}

// 0x00A60C50 (:113)
object* alloc_object_activity_spawn()
{
    return object_activity_spawnpool.alloc(0);
}

// Magic numbers: rfg.exe's arguments (0x01076330), the pool's bytes and capacity.
static obj_class_instantiator object_activity_spawn_instantiator("object_activity_spawn", alloc_object_activity_spawn, types_object_activity_spawn, bpo_index_object_activity_spawn, classname_object_activity_spawn, object_activity_spawn::pool_remaining, 0x18A0, 0x24);

// 0x00A15870 (:113)
unsigned int object_activity_spawn::get_classname_hash()
{
    return object_activity_spawn_instantiator.this_hash;
}

// 0x00A39300 (:113)
int object_activity_spawn::pool_remaining()
{
    return object_activity_spawnpool.get_free();
}

// 0x00A8DB00 (:113)
void object_activity_spawn::pool_free()
{
    object_activity_spawnpool.free(this);
}

// 0x00754DC0 (.h:20)
const object_type object_activity_spawn::get_type_static()
{
    return OT_ACTIVITY_SPAWN;
}

// 0x00754DD0 (.h:20)
const object_sub_type object_activity_spawn::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A10010 (.h:20)
const char* object_activity_spawn::get_classname_static()
{
    return "object_activity_spawn";
}

// 0x00A10020 (.h:20)
int object_activity_spawn::get_bpo_index_static()
{
    return 1;
}

// 0x00A158A0 (.h:20)
const char* object_activity_spawn::get_classname() const
{
    return "object_activity_spawn";
}

// 0x00754DE0 (.h:41)
unsigned int object_activity_spawn::get_activity_handle() const
{
    return activity_node_handle;
}

// 0x00754DF0 (.h:44)
object_type object_activity_spawn::get_activity_type() const
{
    return activity_obj_type;
}

// 0x00A15880 (:114)
object_activity_spawn::object_activity_spawn()
{
}

// 0x00A158B0
object_activity_spawn::~object_activity_spawn()
{
}

// 0x00AA47F0 (:118). Original: a save writes nothing of this class.
bool object_activity_spawn::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (load)
    {
        activity_node_handle = 0xFFFFFFFF;
        activity_obj_type = OT_UNDEFINED;
        activity_data = nullptr;
        char type_name[256];
        pblock.get(ACTIVITY_SPAWN_PROP_TYPE, type_name);
        activity_data = activity_data_find_handle(type_name);
        if (activity_data == nullptr)
        {
            return false;
        }
    }
    return true;
}

// 0x00A158C0 (:147)
bool object_activity_spawn::stream(bool load)
{
    if (!load && obj_flags.destroy_on_stream)
    {
        World.destroy_object(this, false);
    }
    return true;
}

// 0x00A158E0 (:158)
const char* object_activity_spawn::get_activity_name() const
{
    return activity_data->name;
}

// 0x00A39330 (:164)
object* object_activity_spawn::get_activity_node() const
{
    object* node = World.object_from_handle(activity_node_handle);
    if (node == nullptr)
    {
        return nullptr;
    }
    // After the game or on a hard difficulty, a raid runs from the parent node.
    if (node->get_obj_type() == OT_RAID_NODE && (mission_final_mission_complete() || (int)difficulty_get_level() > 1) && node->get_parent() != 0xFFFFFFFF)
    {
        object* parent_objp = World.object_from_handle(node->get_parent());
        if (parent_objp != nullptr && parent_objp->get_obj_type() == OT_RAID_NODE)
        {
            return parent_objp;
        }
    }
    return node;
}

// 0x00A39390 (:198)
bool object_activity_spawn::get_completed_flag() const
{
    return activity_has_been_completed(activity_node_handle); // rfg.exe inlines it
}

// 0x00A60C60 (:205)
void object_activity_spawn::level_init_post()
{
    const object* node = find_activity_node(activity_data); // rfg.exe inlines it
    if (node == nullptr)
    {
        World.destroy_object(this, false);
        return;
    }
    activity_node_handle = node->get_handle();
    activity_obj_type = node->get_obj_type();
}

// 0x00A8DB10 (:226). The same body as the base, inlined in rfg.exe.
void object_activity_spawn::free_this()
{
    object::free_this();
}

// 0x00A158F0 (:233)
void object_activity_spawn::hierarchy_notify(hierarchy_notify_code code)
{
}

// 0x00A15900 (:238). The same body as the base, inlined in rfg.exe.
void object_activity_spawn::destroy_notify()
{
    object::destroy_notify();
}

// 0x00A60CE0 (:279)
void obj_activity_spawn_level_init_post()
{
    // Original: the count is read once, so a node destroyed by level_init_post shifts the rest.
    const int count = World.get_num_objects<object_activity_spawn>();
    for (int i = 0; i < count; i++)
    {
        World.get_object<object_activity_spawn>(i)->level_init_post();
    }
}
