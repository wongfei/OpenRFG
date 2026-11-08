// Original: volition/rfg/code/objects/object_delivery_node.cpp
//
// gameflow.md GF4: the paired start and end nodes of the Guerrilla Express. Registers the class, so zones now create
// these objects. serialize fails when the definition is unknown, so they only exist once delivery_common_system_init has
// run.

#include "volition/rfg/code/objects/object_delivery_node.h"

#include "volition/rfg/code/gameplay/activities/activity_delivery.h"
#include "volition/rfg/code/gameplay/activities/delivery_common.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/vlib/code/memory/object_pool.h"

static object_pool<object_delivery_node, 42, 1> object_delivery_nodepool; // 0x031A0190

static const unsigned int DELIVERY_NODE_NO_TIME = 0x7FFFFFFD; // Magic number: the time of a node that has none

// 0x00A178E0 (:24)
void types_object_delivery_node(signed char& type, signed char& subtype)
{
    type = OT_DELIVERY_NODE;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A178F0 (:24)
int bpo_index_object_delivery_node()
{
    return 1;
}

// 0x00A17900 (:24)
const char* classname_object_delivery_node()
{
    return "object_delivery_node";
}

// 0x00A62CD0 (:24)
object* alloc_object_delivery_node()
{
    return object_delivery_nodepool.alloc(0);
}

// Magic numbers: rfg.exe's arguments (0x0107719B, 0x010771A6), the pool's bytes and capacity.
static obj_class_instantiator object_delivery_node_instantiator("object_delivery_node", alloc_object_delivery_node, types_object_delivery_node, bpo_index_object_delivery_node, classname_object_delivery_node, object_delivery_node::pool_remaining, 0x1EA0, 0x2A); // 0x0304BA9C

// 0x00A17920 (:25)
object_delivery_node::object_delivery_node()
{
}

// 0x00A17950
object_delivery_node::~object_delivery_node()
{
}

// 0x00A178A0 (.h:24)
const char* object_delivery_node::get_classname_static()
{
    return "object_delivery_node";
}

// 0x00A178B0 (.h:24)
int object_delivery_node::get_bpo_index_static()
{
    return 1;
}

// 0x00A17940 (.h:24)
const char* object_delivery_node::get_classname() const
{
    return "object_delivery_node";
}

// 0x00A17910 (:24)
unsigned int object_delivery_node::get_classname_hash()
{
    return object_delivery_node_instantiator.this_hash;
}

// 0x00A3AEF0 (:24)
int object_delivery_node::pool_remaining()
{
    return object_delivery_nodepool.get_free();
}

// 0x00A90010 (:24)
void object_delivery_node::pool_free()
{
    object_delivery_nodepool.free(this);
}

// 0x00AA68D0 (:29). Original: a node that fails to find its definition fails here, and the object is not created.
bool object_delivery_node::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set(DELIVERY_PROP_TYPE, delivery_def->delivery_name);
        pblock.set(DELIVERY_PROP_PAIR_NUMBER, pair_number);
        pblock.set(DELIVERY_PROP_START_NODE, start_node);
        pblock.set(DELIVERY_PROP_DELIVERY_FLAGS, delivery_flags);
        pblock.set(DELIVERY_PROP_BEST_TIME, best_completion_time);
        return true;
    }
    best_completion_time = DELIVERY_NODE_NO_TIME;
    current_completion_time = DELIVERY_NODE_NO_TIME;
    delivery_def = nullptr;
    *reinterpret_cast<unsigned char*>(&delivery_flags) = 0;
    char type_name[256];
    type_name[0] = '\0';
    pblock.get(DELIVERY_PROP_TYPE, type_name);
    // Original: pair_number and start_node stay as the pool left them when the property is missing.
    pblock.get_optional(DELIVERY_PROP_PAIR_NUMBER, pair_number);
    pblock.get_optional(DELIVERY_PROP_START_NODE, start_node);
    pblock.get_optional(DELIVERY_PROP_DELIVERY_FLAGS, delivery_flags);
    pblock.get_optional(DELIVERY_PROP_BEST_TIME, best_completion_time);
    delivery_def = delivery_get_def_by_name(type_name);
    return delivery_def != nullptr;
}

// 0x00A17960 (:77)
bool object_delivery_node::stream(bool load)
{
    if (!load && obj_flags.destroy_on_stream)
    {
        World.destroy_object(this, false);
    }
    if (!start_node)
    {
        if (load)
        {
            activity_delivery_end_node_stream_in(this);
        }
        else
        {
            activity_delivery_end_node_stream_out(this);
        }
        // Original: tests start_node a second time, which the callees do not change.
        if (!start_node)
        {
            return true;
        }
    }
    delivery_flags.disabled_until_stream = false;
    return true;
}

// 0x00A3AF20 (:100)
obj_serialize_mode object_delivery_node::serialize_mode()
{
    return start_node && delivery_flags.completed ? OSM_FULL : OSM_RESTORE;
}

// 0x00A90020 (:110). Original: the other node of this pair, of the opposite kind.
const object_delivery_node* object_delivery_node::find_pair_node() const
{
    farray<const object_delivery_node*, 42> nodes;
    object_delivery_node_get_all(nodes);
    for (int i = 0; i < nodes.size(); i++)
    {
        const object_delivery_node* node = nodes[i];
        if (node->pair_number == pair_number && node->start_node != start_node)
        {
            return node;
        }
    }
    return nullptr;
}

// 0x00A179C0 (:154)
void object_delivery_node::set_disabled(bool disabled)
{
    delivery_flags.disabled_until_stream = disabled;
}

// 0x00A179E0 (:160)
bool object_delivery_node::is_disabled()
{
    if (!start_node)
    {
        return true;
    }
    return delivery_flags.disabled_until_stream;
}

// 0x00A17A00 (:171)
bool object_delivery_node::is_completed()
{
    if (!start_node)
    {
        return false;
    }
    return delivery_flags.completed;
}

// 0x00A17A20 (:193)
bool object_delivery_node::get_best_time(unsigned int& best_time)
{
    if (start_node && delivery_flags.completed)
    {
        best_time = best_completion_time;
        return true;
    }
    best_time = 0;
    return false;
}

// 0x00A17A60 (:208)
void object_delivery_node::record_victory(unsigned int completion_time, unsigned int par_time)
{
    if (!start_node)
    {
        return;
    }
    if (!delivery_flags.completed)
    {
        delivery_flags.completed = true;
        best_completion_time = completion_time;
    }
    else if (completion_time < best_completion_time)
    {
        best_completion_time = completion_time;
    }
    current_completion_time = completion_time;
    if (!delivery_flags.beat_par_time && best_completion_time < par_time)
    {
        delivery_flags.beat_par_time = true;
    }
}

// 0x00754450 (.h:59)
bool object_delivery_node::has_beat_par_time()
{
    return delivery_flags.beat_par_time;
}

// 0x00754460 (.h:67)
const base_delivery_definition_data* object_delivery_node::get_delivery_def() const
{
    return delivery_def;
}

// 0x00754470 (.h:68)
const bool object_delivery_node::is_start_node() const
{
    return start_node;
}

// 0x00A178C0 (.h:69)
const int object_delivery_node::get_pair_number() const
{
    return pair_number;
}

// 0x00A62CE0 (:242). Original: a world with more than 42 nodes reports the overflow and keeps the first ones.
bool object_delivery_node_get_all(farray<object_delivery_node const*, 42>& delivery_nodes)
{
    delivery_nodes.clear();
    for (int i = 0; i < World.get_num_objects<object_delivery_node>(); i++)
    {
        delivery_nodes.add(World.get_object<object_delivery_node>(i));
    }
    return delivery_nodes.size() > 0;
}

// 0x00A17AC0 (:256)
const char* const object_delivery_node_get_name(const object_delivery_node* const odn)
{
    if (odn != nullptr && odn->get_delivery_def() != nullptr)
    {
        return odn->get_delivery_def()->delivery_name;
    }
    return nullptr;
}
