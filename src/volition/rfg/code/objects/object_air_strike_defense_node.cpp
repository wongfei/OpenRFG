// Original: volition/rfg/code/objects/object_air_strike_defense_node.cpp
//
// Air strike defense nodes: the start points of the air strike defense activity, each naming its definition from
// activity_air_strike_defense.xtbl. Registers the class, so zones now create these objects.
// Original: nothing in rfg.exe calls asd_definitions_init, so Asd_definitions stays empty, serialize fails for every
// node and none is ever created (the activity was cut).

#include "volition/rfg/code/objects/object_air_strike_defense_node.h"

#include "volition/rfg/code/gameplay/activities/activity_air_strike_defense_internal.h"
#include "volition/rfg/code/gameplay/activities/base_activity_data.h"
#include "volition/rfg/code/gameplay/activities/vrail_data.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/havok/havok_collision_events.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/varray.h"

static const int ASD_STRING_POOL_SIZE = 0x200; // Magic number: rfg.exe's string pool bytes
static const char ASD_STRING_POOL_NAME[] = "ASD String Pool";

static char* Asd_string_pool_data; // 0x0302F128
static string_pool Asd_string_pool(ASD_STRING_POOL_NAME); // 0x035B5940
static farray<const asd_definition*,20> Asd_definitions; // 0x016AD0F8
static object_pool<object_air_strike_defense_node, 20, 1> object_air_strike_defense_nodepool; // 0x031872B0

// 0x00A15940 (:35)
void types_object_air_strike_defense_node(signed char& type, signed char& subtype)
{
    type = OT_AIR_STRIKE_DEFENSE_NODE;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A15950 (:35)
int bpo_index_object_air_strike_defense_node()
{
    return 1;
}

// 0x00A15960 (:35)
const char* classname_object_air_strike_defense_node()
{
    return "object_air_strike_defense_node";
}

// 0x00A60D40 (:35)
object* alloc_object_air_strike_defense_node()
{
    return object_air_strike_defense_nodepool.alloc(0);
}

// Magic numbers: rfg.exe's arguments (0x01076480), the pool's bytes and capacity.
static obj_class_instantiator object_air_strike_defense_node_instantiator("object_air_strike_defense_node", alloc_object_air_strike_defense_node, types_object_air_strike_defense_node, bpo_index_object_air_strike_defense_node,
                                                                          classname_object_air_strike_defense_node, object_air_strike_defense_node::pool_remaining, 0xDD0, 0x14); // 0x030C0708

// 0x00A15980 (:36)
object_air_strike_defense_node::object_air_strike_defense_node()
{
}

// 0x00A159C0
object_air_strike_defense_node::~object_air_strike_defense_node()
{
}

// 0x00754E00 (.h:87)
const object_type object_air_strike_defense_node::get_type_static()
{
    return OT_AIR_STRIKE_DEFENSE_NODE;
}

// 0x00754E10 (.h:87)
const object_sub_type object_air_strike_defense_node::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A15910 (.h:87)
const char* object_air_strike_defense_node::get_classname_static()
{
    return "object_air_strike_defense_node";
}

// 0x00A15920 (.h:87)
int object_air_strike_defense_node::get_bpo_index_static()
{
    return 1;
}

// 0x00A159B0 (.h:87)
const char* object_air_strike_defense_node::get_classname() const
{
    return "object_air_strike_defense_node";
}

// 0x00A15970 (:35)
unsigned int object_air_strike_defense_node::get_classname_hash()
{
    return object_air_strike_defense_node_instantiator.this_hash;
}

// 0x00A393F0 (:35)
int object_air_strike_defense_node::pool_remaining()
{
    return object_air_strike_defense_nodepool.get_free();
}

// 0x00A8DB20 (:35)
void object_air_strike_defense_node::pool_free()
{
    object_air_strike_defense_nodepool.free(this);
}

// 0x00A8DB30 (:40)
// Detaches the node from its parent and its children; it does not call object::free_this.
void object_air_strike_defense_node::free_this()
{
    if (get_handle() == 0xFFFFFFFF)
    {
        return;
    }
    havok_collision_events_object_freed(this);
    if (get_parent() != 0xFFFFFFFF)
    {
        object* parent_objp = World.object_from_handle(get_parent());
        if (parent_objp != nullptr)
        {
            parent_objp->remove_child(this);
        }
    }
    object* child = child_ptr;
    while (child != nullptr)
    {
        object* next = child->child_next != child_ptr ? child->child_next : nullptr;
        object* host = World.object_from_handle(child->get_parent());
        if (host != nullptr)
        {
            host->remove_child(child);
        }
        child = next;
    }
}

// 0x00AA4890 (:45)
// A node whose definition is unknown fails, and the object is not created.
bool object_air_strike_defense_node::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (load)
    {
        *reinterpret_cast<unsigned char*>(&asd_node_flags) = 0;
        char type_name[256];
        pblock.get(AIR_STRIKE_DEFENSE_PROP_TYPE, type_name);
        asd_def = asd_definition_from_name(type_name);
        if (asd_def == nullptr)
        {
            return false;
        }
        last_used_time.set(0);
    }
    else if (asd_def != nullptr)
    {
        pblock.set(AIR_STRIKE_DEFENSE_PROP_TYPE, asd_def->name);
    }
    return true;
}

// 0x00754E20 (.h:102)
const asd_definition* object_air_strike_defense_node::get_definition() const
{
    return asd_def;
}

// 0x00A159D0 (:84)
const district* object_air_strike_defense_node::get_district() const
{
    return district_get_from_pos(pos);
}

// 0x00A60D50 (:98)
// Reads activity_air_strike_defense.xtbl (up to 20 definitions). Original: radio_lines are left as the mempool gave
// them, and nothing calls this function.
void asd_definitions_init()
{
    if (Asd_string_pool_data == nullptr)
    {
        Asd_string_pool_data = (char*)Permanent_mempool.alloc(ASD_STRING_POOL_SIZE, 0);
        Asd_string_pool.init(Asd_string_pool_data, ASD_STRING_POOL_SIZE, ASD_STRING_POOL_NAME, 0, false);
        Asd_string_pool.clear();
    }
    xml_element* root = xml_table_parse("activity_air_strike_defense.xtbl");
    if (root == nullptr)
    {
        return;
    }
    for (xml_element* cur = root->elements; cur != nullptr && Asd_definitions.size() < Asd_definitions.capacity(); cur = cur->next)
    {
        const char* name = Asd_string_pool.add_unique(xml_table_get_req_string_ref(cur, "Name"));
        if (name == nullptr)
        {
            break;
        }
        char* caption = Asd_string_pool.add_unique(xml_table_get_req_string_ref(cur, "Caption"));
        if (caption == nullptr)
        {
            break;
        }
        const base_activity_data* base_data = activity_data_find_handle(xml_table_get_req_string_ref(cur, "Base_Data"));
        const vehicle_rail_definition* vrail_def = vrail_data_get_def(xml_table_get_req_string_ref(cur, "Vrail_data"));
        int num_bombers = 0;
        float win = 0.0f;
        float loss = 0.0f;
        float ignored = 0.0f;
        xml_table_get_req_int(num_bombers, cur, "Num_bombers_to_kill");
        xml_table_get_req_float(win, cur, "global_moral_change_win");
        xml_table_get_req_float(loss, cur, "global_moral_change_loss");
        xml_table_get_req_float(ignored, cur, "global_moral_change_ignore");
        asd_definition* def = (asd_definition*)Permanent_mempool.alloc(sizeof(asd_definition), 4);
        if (def == nullptr)
        {
            // Original: reports "Out of memory in Permanent_mempool allocating courier definition for ASD" and returns
            // without closing the table.
            return;
        }
        def->name = name;
        def->caption = caption;
        def->base_data = base_data;
        def->vrail_def = vrail_def;
        def->num_bombers_to_kill = num_bombers;
        def->global_moral_change_win = win;
        def->global_moral_change_loss = loss;
        def->global_moral_change_ignored = ignored;
        const asd_definition* added = def;
        Asd_definitions.add(added);
    }
    xml_table_close(root);
}

// 0x00A39420 (:194)
const asd_definition* asd_definition_from_name(const char* name)
{
    if (name == nullptr)
    {
        return nullptr;
    }
    for (int i = 0; i < Asd_definitions.size(); i++)
    {
        if (v_stricmp(Asd_definitions[i]->name, name) == 0)
        {
            return Asd_definitions[i];
        }
    }
    return nullptr;
}
