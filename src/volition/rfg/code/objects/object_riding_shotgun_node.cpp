// Original: volition/rfg/code/objects/object_riding_shotgun_node.cpp
//
// gameflow.md GF4: the start points of Riding Shotgun. Registers the class, so zones now create these objects. A node
// whose name is not in activity_riding_shotgun.xtbl is still created, with no info.

#include "volition/rfg/code/objects/object_riding_shotgun_node.h"

#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/gameplay/activities/activity_vrail.h"
#include "volition/rfg/code/gameplay/activities/activity_vrail_internal.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object_patrol.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/scripting/objective_highlight.h"
#include "volition/vlib/code/memory/object_pool.h"

static object_pool<object_riding_shotgun_node, 32, 1> object_riding_shotgun_nodepool; // 0x0305F840

// 0x00A1BC80 (:10)
void types_object_riding_shotgun_node(signed char& type, signed char& subtype)
{
    type = OT_RIDING_SHOTGUN_NODE;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A1BC90 (:10)
int bpo_index_object_riding_shotgun_node()
{
    return 1;
}

// 0x00A1BCA0 (:10)
const char* classname_object_riding_shotgun_node()
{
    return "object_riding_shotgun_node";
}

// 0x00A6A440 (:10)
object* alloc_object_riding_shotgun_node()
{
    return object_riding_shotgun_nodepool.alloc(0);
}

// Magic numbers: rfg.exe's arguments (0x0107BA1B, 0x0107BA26), the pool's bytes and capacity.
static obj_class_instantiator object_riding_shotgun_node_instantiator("object_riding_shotgun_node", alloc_object_riding_shotgun_node, types_object_riding_shotgun_node, bpo_index_object_riding_shotgun_node, classname_object_riding_shotgun_node, object_riding_shotgun_node::pool_remaining, 0x15E0, 0x20); // 0x030FCDF0

// 0x00A1BCC0 (:11)
object_riding_shotgun_node::object_riding_shotgun_node()
{
}

// 0x00A1BCF0
object_riding_shotgun_node::~object_riding_shotgun_node()
{
}

// 0x00A10100 (.h:24)
const char* object_riding_shotgun_node::get_classname_static()
{
    return "object_riding_shotgun_node";
}

// 0x00A10110 (.h:24)
int object_riding_shotgun_node::get_bpo_index_static()
{
    return 1;
}

// 0x00A1BCE0 (.h:24)
const char* object_riding_shotgun_node::get_classname() const
{
    return "object_riding_shotgun_node";
}

// 0x00A1BCB0 (:10)
unsigned int object_riding_shotgun_node::get_classname_hash()
{
    return object_riding_shotgun_node_instantiator.this_hash;
}

// 0x00A3D730 (:10)
int object_riding_shotgun_node::pool_remaining()
{
    return object_riding_shotgun_nodepool.get_free();
}

// 0x00A92AB0 (:10)
void object_riding_shotgun_node::pool_free()
{
    object_riding_shotgun_nodepool.free(this);
}

// 0x00AAB210 (:16)
bool object_riding_shotgun_node::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set(RIDING_SHOTGUN_PROP_TYPE, rs_info->name);
        const bool completed = rs_flags.completed != 0;
        pblock.set(RIDING_SHOTGUN_PROP_COMPLETED, completed);
        return true;
    }
    rs_flags = {};
    flare_effect_handle = 0xFFFFFFFF;
    char type_name[256];
    type_name[0] = '\0';
    pblock.get(RIDING_SHOTGUN_PROP_TYPE, type_name);
    rs_info = get_riding_shotgun_info_by_name(type_name);
    bool completed = false;
    if (pblock.get_optional(RIDING_SHOTGUN_PROP_COMPLETED, completed))
    {
        rs_flags.completed = completed;
    }
    return true;
}

// 0x00A1BD00 (:50)
bool object_riding_shotgun_node::stream(bool load)
{
    if (!load && obj_flags.destroy_on_stream)
    {
        World.destroy_object(this, false);
    }
    return true;
}

// 0x00A1BD20 (:65): a finished node restores differently, the enum's next value.
obj_serialize_mode object_riding_shotgun_node::serialize_mode()
{
    return (obj_serialize_mode)(OSM_RESTORE + (rs_flags.completed != 0 ? 1 : 0));
}

// 0x00A92AC0 (:75)
object_patrol* object_riding_shotgun_node::find_patrol() const
{
    farray<object_patrol*,1> patrols;
    return world_collect_descendants<object_patrol>(this, patrols, nullptr, nullptr) ? patrols[0] : nullptr;
}

// 0x00754060 (.h:38)
const riding_shotgun_info* object_riding_shotgun_node::get_riding_shotgun_info() const
{
    return rs_info;
}

// 0x00754080 (.h:41)
void object_riding_shotgun_node::set_completed()
{
    rs_flags.completed = 1;
}

// 0x00A3D760 (:116)
bool object_riding_shotgun_node::flare_started() const
{
    return flare_effect_handle != 0xFFFFFFFF && World.object_from_handle<object_effect>(flare_effect_handle) != nullptr;
}

// 0x00A3D790 (:123)
void object_riding_shotgun_node::start_flare()
{
    if (!flare_started())
    {
        flare_effect_handle = objective_highlight_activity_marker_add(this, ZERO_VECTOR);
    }
}

// 0x00A3D7D0 (:131)
void object_riding_shotgun_node::stop_flare()
{
    if (flare_started())
    {
        World.destroy_object(flare_effect_handle, false);
        flare_effect_handle = 0xFFFFFFFF;
    }
}

// 0x00A1BD40 (:87): any node after the final mission, otherwise one not finished in a district not yet liberated.
bool object_riding_shotgun_node::can_spawn() const
{
    if (mission_final_mission_complete())
    {
        return true;
    }
    if (rs_flags.completed)
    {
        return false;
    }
    const district* dp = district_get_from_pos(pos);
    return dp == nullptr || !dp->is_liberated();
}

// 0x00A1BD80 (:139)
char* riding_shotgun_get_name(const object_riding_shotgun_node* const orsn)
{
    if (orsn != nullptr && orsn->get_riding_shotgun_info() != nullptr)
    {
        return orsn->get_riding_shotgun_info()->name;
    }
    return nullptr;
}
