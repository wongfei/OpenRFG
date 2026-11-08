// Original: volition/rfg/code/objects/object_upgrade_node.cpp
//
// gameflow.md GF3: the safehouse enables and disables these, and a type stand-in would have it walk humans instead. The
// upgrade shop markers. Registers the class, so zones now create these objects.

#include "volition/rfg/code/objects/object_upgrade_node.h"

#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/scripting/objective_highlight.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/string_comp.h"

static object_pool<object_upgrade_node, 32, 1> object_upgrade_nodepool; // 0x03066550
// 0x01694720, read from rfg.exe, in upgrade_node_types order.
static const char* UPGRADE_NODE_TYPE_NAMES[NUM_UPGRADE_NODE_TYPES] = {"Weapons", "Vehicles"};

// 0x00A1DFE0 (:38)
void types_object_upgrade_node(signed char& type, signed char& subtype)
{
    type = OT_UPGRADE_NODE;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A1DFF0 (:38)
int bpo_index_object_upgrade_node()
{
    return 1;
}

// 0x00A1E000 (:38)
const char* classname_object_upgrade_node()
{
    return "object_upgrade_node";
}

// 0x00A71150 (:38)
object* alloc_object_upgrade_node()
{
    return object_upgrade_nodepool.alloc(0);
}

// Magic numbers: rfg.exe's arguments (0x0107C740), the pool's bytes and capacity.
static obj_class_instantiator object_upgrade_node_instantiator("object_upgrade_node", alloc_object_upgrade_node, types_object_upgrade_node, bpo_index_object_upgrade_node, classname_object_upgrade_node, object_upgrade_node::pool_remaining, 0x1560, 0x20);

// 0x00A1E010 (:38)
unsigned int object_upgrade_node::get_classname_hash()
{
    return object_upgrade_node_instantiator.this_hash;
}

// 0x00A40CE0 (:38)
int object_upgrade_node::pool_remaining()
{
    return object_upgrade_nodepool.get_free();
}

// 0x00A95310 (:38)
void object_upgrade_node::pool_free()
{
    object_upgrade_nodepool.free(this);
}

// 0x00A10140 (.h:29)
const char* object_upgrade_node::get_classname_static()
{
    return "object_upgrade_node";
}

// 0x00A10150 (.h:29)
int object_upgrade_node::get_bpo_index_static()
{
    return 1;
}

// 0x00A1E040 (.h:29)
const char* object_upgrade_node::get_classname() const
{
    return "object_upgrade_node";
}

// 0x00A1E020 (:39)
object_upgrade_node::object_upgrade_node()
{
}

// 0x00A1E050
object_upgrade_node::~object_upgrade_node()
{
}

// 0x00AAD990 (:51)
// Original bug kept: the type is only compared when the property was not found, and a property that is found never
// sets it, so every node stays type 0. Deviation: rfg.exe compares an uninitialized buffer in that case; here it is
// empty, which matches no name.
bool object_upgrade_node::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (load)
    {
        if (Multiplayer)
        {
            return false;
        }
        upgrade_node_type = 0;
        char type_name[64];
        type_name[0] = '\0';
        if (!pblock.get_optional(PROP_UPGRADE_TYPE, type_name))
        {
            for (int i = 0; i < NUM_UPGRADE_NODE_TYPES; i++)
            {
                if (v_stricmp(type_name, UPGRADE_NODE_TYPE_NAMES[i]) == 0)
                {
                    upgrade_node_type = i;
                }
            }
        }
        enabled = true;
        enable_markers();
    }
    return true;
}

// 0x00A40D10 (:88)
bool object_upgrade_node::stream(bool load)
{
    if (load)
    {
        if (enabled)
        {
            start_effect();
        }
    }
    else
    {
        if (obj_flags.destroy_on_stream)
        {
            World.destroy_object(this, false);
        }
        stop_effect();
    }
    return true;
}

// 0x00A95320 (:116)
void object_upgrade_node::free_this()
{
    object::free_this();
}

// 0x00A40D80 (:123)
void object_upgrade_node::enable_markers()
{
    if (obj_flags.streamed && enabled)
    {
        start_effect();
    }
}

// 0x00A40DC0 (:134)
void object_upgrade_node::disable_markers()
{
    stop_effect();
}

// 0x00A40DD0 (:140)
void object_upgrade_node::set_enabled(bool set_val)
{
    enabled = set_val;
    if (!set_val)
    {
        stop_effect();
    }
    else if (obj_flags.streamed)
    {
        start_effect();
    }
}

// 0x00A1E060 (:160)
void object_upgrade_node::start_effect()
{
    // Magic numbers: the marker's default size, no offset.
    objective_highlight_ground_marker_add(get_handle(), 1.0f, ZERO_VECTOR, false, true);
}

// 0x00A1E090 (:166)
void object_upgrade_node::stop_effect()
{
    objective_highlight_ground_marker_remove(get_handle());
}

// 0x00A71160 (:171)
void object_upgrade_node_enable_markers()
{
    const int count = World.get_num_objects<object_upgrade_node>();
    for (int i = 0; i < count; i++)
    {
        World.get_object<object_upgrade_node>(i)->enable_markers();
    }
}

// 0x00A711E0 (:179)
void object_upgrade_node_disable_markers()
{
    const int count = World.get_num_objects<object_upgrade_node>();
    for (int i = 0; i < count; i++)
    {
        World.get_object<object_upgrade_node>(i)->disable_markers();
    }
}
