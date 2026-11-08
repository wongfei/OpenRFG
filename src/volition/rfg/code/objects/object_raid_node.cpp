// Original: volition/rfg/code/objects/object_raid_node.cpp
//
// gameflow.md GF4: the raid's start point. Registers the class, so zones now create these objects.

#include "volition/rfg/code/objects/object_raid_node.h"

#include "volition/rfg/code/gameplay/activities/raid_common.h"
#include "volition/rfg/code/gameplay/activities/raid_common_internal.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/vlib/code/memory/object_pool.h"

static object_pool<object_raid_node, 64, 1> object_raid_nodepool; // 0x031847A0

// 0x00A1AF70 (:30)
void types_object_raid_node(signed char& type, signed char& subtype)
{
    type = OT_RAID_NODE;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A1AF80 (:30)
int bpo_index_object_raid_node()
{
    return 1;
}

// 0x00A1AF90 (:30)
const char* classname_object_raid_node()
{
    return "object_raid_node";
}

// 0x00A6A030 (:30)
object* alloc_object_raid_node()
{
    return object_raid_nodepool.alloc(0);
}

// Magic numbers: rfg.exe's arguments (0x0107B6F0), the pool's bytes and capacity.
static obj_class_instantiator object_raid_node_instantiator("object_raid_node", alloc_object_raid_node, types_object_raid_node, bpo_index_object_raid_node, classname_object_raid_node, object_raid_node::pool_remaining, 0x2A80, 0x40);

// 0x00A1AFA0 (:30)
unsigned int object_raid_node::get_classname_hash()
{
    return object_raid_node_instantiator.this_hash;
}

// 0x00A3D600 (:30)
int object_raid_node::pool_remaining()
{
    return object_raid_nodepool.get_free();
}

// 0x00A92A50 (:30)
void object_raid_node::pool_free()
{
    object_raid_nodepool.free(this);
}

// 0x00A15810 (.h:37)
const char* object_raid_node::get_classname_static()
{
    return "object_raid_node";
}

// 0x00A15820 (.h:37)
int object_raid_node::get_bpo_index_static()
{
    return 1;
}

// 0x00A1AFD0 (.h:37)
const char* object_raid_node::get_classname() const
{
    return "object_raid_node";
}

// 0x00A1AFB0 (:31)
object_raid_node::object_raid_node()
{
}

// 0x00A1AFE0
object_raid_node::~object_raid_node()
{
}

// 0x00A1AFF0 (:35). A completed node is saved in full (OSM_RESTORE + 1).
obj_serialize_mode object_raid_node::serialize_mode()
{
    return (obj_serialize_mode)((int)OSM_RESTORE + (raid_flags.completed ? 1 : 0));
}

// 0x00753F10 (.h:48)
const base_raid_definition_data* object_raid_node::get_raid_def() const
{
    return base_raid_def;
}

// 0x00A1B010 (:45)
const base_activity_data* object_raid_node::get_base_data() const
{
    return base_raid_def != nullptr ? base_raid_def->base_data : nullptr;
}

// 0x00AAAD80 (:56)
bool object_raid_node::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set(RAID_PROP_TYPE, base_raid_def->raid_name);
        if (raid_flags.completed)
        {
            pblock.set(ACTIVITY_COMPLETED_PROP, true);
        }
        return true;
    }
    base_raid_def = nullptr;
    raid_flags.completed = false;
    char type_name[256];
    type_name[0] = '\0';
    Cartographer.object_set_visible_over_fow(get_handle(), true);
    pblock.get(RAID_PROP_TYPE, type_name);
    base_raid_def = raid_get_def_by_name(type_name);
    if (base_raid_def == nullptr)
    {
        return false;
    }
    bool flag;
    if (pblock.get_optional(ACTIVITY_COMPLETED_PROP, flag))
    {
        raid_flags.completed = flag;
    }
    return true;
}

// 0x00A1B030 (:97)
bool object_raid_node::stream(bool load)
{
    if (!load && obj_flags.destroy_on_stream)
    {
        World.destroy_object(this, false);
    }
    return true;
}

// 0x00A6A040 (:111)
void object_raid_node::set_completed_flag()
{
    raid_flags.completed = true;
    if (get_parent() != 0xFFFFFFFF)
    {
        object* parent_objp = World.object_from_handle(get_parent());
        if (parent_objp != nullptr && parent_objp->get_obj_type() == OT_RAID_NODE)
        {
            static_cast<object_raid_node*>(parent_objp)->set_completed_flag_parent();
        }
    }
    set_completed_flag_child(); // rfg.exe inlines it
}

// 0x00A3D630 (:118)
void object_raid_node::set_completed_flag_parent()
{
    object_raid_node* node = this;
    while (true)
    {
        node->raid_flags.completed = true;
        if (node->get_parent() == 0xFFFFFFFF)
        {
            return;
        }
        object* parent_objp = World.object_from_handle(node->get_parent());
        if (parent_objp == nullptr || parent_objp->get_obj_type() != OT_RAID_NODE)
        {
            return;
        }
        node = static_cast<object_raid_node*>(parent_objp);
    }
}

// 0x00A1B050 (:131)
bool object_raid_node::is_defensive_raid() const
{
    return base_raid_def != nullptr && base_raid_def->defending_team == HUMAN_TEAM_GUERILLA;
}

// 0x00A1B070 (:139)
bool object_raid_node::can_spawn() const
{
    if ((int)gsm_get_alert_level() >= ALERT_LEVEL_ORANGE)
    {
        return false;
    }
    if (mission_final_mission_complete())
    {
        return true;
    }
    if (raid_flags.completed)
    {
        return false;
    }
    const district* dp = district_get_from_pos(pos);
    return dp == nullptr || !dp->is_liberated();
}

// 0x00A3D660 (:178)
void object_raid_node::set_completed_flag_child()
{
    raid_flags.completed = true;
    object* child = child_ptr;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_RAID_NODE)
        {
            static_cast<object_raid_node*>(child)->set_completed_flag_child();
        }
        child = child->child_next;
        if (child == child_ptr)
        {
            return;
        }
    }
}

// 0x00A1B0B0 (:198)
const char* const raid_node_get_name(const object_raid_node* const orn)
{
    return orn != nullptr && orn->get_raid_def() != nullptr ? orn->get_raid_def()->raid_name : nullptr;
}
