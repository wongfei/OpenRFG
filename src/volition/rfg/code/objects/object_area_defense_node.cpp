// Original: volition/rfg/code/objects/object_area_defense_node.cpp
//
// gameflow.md GF4: the start points of Area Defense. Registers the class, so zones now create these objects. A node whose
// name is not in activity_area_defense.xtbl is still created, with no info.

#include "volition/rfg/code/objects/object_area_defense_node.h"

#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/gameplay/activities/activity_area_defense.h"
#include "volition/rfg/code/gameplay/activities/activity_area_defense_internal.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object_activity_spawn.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/scripting/objective_highlight.h"
#include "volition/vlib/code/memory/object_pool.h"

static object_pool<object_area_defense_node, 32, 1> object_area_defense_nodepool; // 0x033BFE50

// 0x00A159F0 (:13)
void types_object_area_defense_node(signed char& type, signed char& subtype)
{
    type = OT_AREA_DEFENSE_NODE;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A15A00 (:13)
int bpo_index_object_area_defense_node()
{
    return 1;
}

// 0x00A15A10 (:13)
const char* classname_object_area_defense_node()
{
    return "object_area_defense_node";
}

// 0x00A60F70 (:13)
object* alloc_object_area_defense_node()
{
    return object_area_defense_nodepool.alloc(0);
}

// Magic numbers: rfg.exe's arguments (0x010765A0), the pool's bytes and capacity.
static obj_class_instantiator object_area_defense_node_instantiator("object_area_defense_node", alloc_object_area_defense_node, types_object_area_defense_node, bpo_index_object_area_defense_node, classname_object_area_defense_node, object_area_defense_node::pool_remaining, 0x15E0, 0x20);

// 0x00A15A30 (:14)
object_area_defense_node::object_area_defense_node()
{
}

// 0x00A15A60
object_area_defense_node::~object_area_defense_node()
{
}

// 0x00A10030 (.h:25)
const char* object_area_defense_node::get_classname_static()
{
    return "object_area_defense_node";
}

// 0x00A10040 (.h:25)
int object_area_defense_node::get_bpo_index_static()
{
    return 1;
}

// 0x00A15A50 (.h:25)
const char* object_area_defense_node::get_classname() const
{
    return "object_area_defense_node";
}

// 0x00A15A20 (:13)
unsigned int object_area_defense_node::get_classname_hash()
{
    return object_area_defense_node_instantiator.this_hash;
}

// 0x00A39480 (:13)
int object_area_defense_node::pool_remaining()
{
    return object_area_defense_nodepool.get_free();
}

// 0x00A8DB40 (:13)
void object_area_defense_node::pool_free()
{
    object_area_defense_nodepool.free(this);
}

// 0x00AA4940 (:19)
bool object_area_defense_node::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set(AREA_DEFENSE_PROP_TYPE, ad_info->name);
        pblock.set(AREA_DEFENSE_PROP_COMPLETED, completed);
        return true;
    }
    char type_name[256];
    type_name[0] = '\0';
    flare_effect_handle = 0xFFFFFFFF;
    pblock.get(AREA_DEFENSE_PROP_TYPE, type_name);
    ad_info = get_area_defense_info_by_name(type_name);
    completed = false;
    pblock.get_optional(AREA_DEFENSE_PROP_COMPLETED, completed);
    return true;
}

// 0x00A15A70 (:47)
bool object_area_defense_node::stream(bool load)
{
    if (!load && obj_flags.destroy_on_stream)
    {
        World.destroy_object(this, false);
    }
    return true;
}

// 0x00A15A90 (:62): a finished node restores differently, the enum's next value.
obj_serialize_mode object_area_defense_node::serialize_mode()
{
    return (obj_serialize_mode)(OSM_RESTORE + (completed ? 1 : 0));
}

// 0x00753CB0 (.h:36)
const activity_area_defense_info* object_area_defense_node::get_area_defense_info() const
{
    return ad_info;
}

// 0x00A15AA0 (:73)
const base_activity_data* object_area_defense_node::get_base_data() const
{
    return ad_info != nullptr ? ad_info->base_data : nullptr;
}

// 0x00753CD0 (.h:42)
void object_area_defense_node::set_completed()
{
    completed = true;
}

// 0x00A60F80 (:82)
object_activity_spawn* object_area_defense_node::get_start_node() const
{
    for (int i = 0; i < World.get_num_objects<object_activity_spawn>(); i++)
    {
        object_activity_spawn* start = World.get_object<object_activity_spawn>(i);
        if (start->get_activity_handle() == get_handle())
        {
            return start;
        }
    }
    return nullptr;
}

// 0x00A394B0 (:97)
bool object_area_defense_node::flare_started() const
{
    return flare_effect_handle != 0xFFFFFFFF && World.object_from_handle<object_effect>(flare_effect_handle) != nullptr;
}

// 0x00A15AC0 (:104): any node after the final mission, otherwise one not finished in a district not yet liberated.
bool object_area_defense_node::can_spawn() const
{
    if (mission_final_mission_complete())
    {
        return true;
    }
    if (completed)
    {
        return false;
    }
    const district* dp = district_get_from_pos(pos);
    return dp == nullptr || !dp->is_liberated();
}

// 0x00A394E0 (:133): the marker sits on the start node, not on this node.
void object_area_defense_node::start_flare(const object_activity_spawn* start_node)
{
    if (start_node != nullptr && !flare_started())
    {
        flare_effect_handle = objective_highlight_activity_marker_add(start_node, ZERO_VECTOR);
    }
}

// 0x00A39530 (:142)
void object_area_defense_node::stop_flare()
{
    if (flare_started())
    {
        World.destroy_object(flare_effect_handle, false);
        flare_effect_handle = 0xFFFFFFFF;
    }
}

// 0x00A15B00 (:150)
char* area_defense_get_name(const object_area_defense_node* const orsn)
{
    if (orsn != nullptr && orsn->get_area_defense_info() != nullptr)
    {
        return orsn->get_area_defense_info()->name;
    }
    return nullptr;
}
