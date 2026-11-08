// Original: volition/rfg/code/objects/object_action_node.cpp
//
// Action nodes (stub_impl.md S9): the node type table (action_node_types.xtbl, ambient_action_types.xtbl), the node
// objects and what a human does at one. Linked by OpenRFG only. Build only: nothing here has run.

#include "volition/rfg/code/objects/object_action_node.h"

#include "volition/anim_lib/code/anim_lib_internal.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/rfg_chunk_shared.h"
#include "volition/rfg/code/level/rfg_prop_name_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/pathfinding/nav_cell_detour.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/scripting/objective_highlight.h"
#include "volition/rfg/code/spawn/ambient_prop_info.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/hash_table.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// Magic numbers: rfg.exe's pools and tables.
string_pool Action_node_string_pool("Action_node_string_pool"); // 0x030F7E10
static object_pool<object_action_node,608,1> object_action_nodepool; // 0x033D66E0
static object_pool<action_node_definition,156,1> Action_node_definition_pool; // 0x0305A6E0
static object_pool<action_node_animation_info,124,1> Action_anim_info_pool; // 0x03426500
static str_data_hash_table_cached<action_node_definition*,156,unsigned char> Action_node_definition_hash; // 0x03425EB8
static farray<const char*,32> All_ambient_action_types; // 0x016AD068
static char* Action_node_string_pool_data; // 0x0302F124
static timestamp Release_poll_timer; // 0x016AD058
static timestamp Object_create_timer; // 0x016AD05C
static timestamp Effect_create_timer; // 0x016AD060

static const char* SNAP_TO_TARGET_SELECTION[3] = { "Action Node", "Parent", "Spawn Child" }; // 0x01694548
static const char* PROP_CREATION_TIME_TAGS[5] = { nullptr, "Before Entry Anim", "During Entry Anim", "After Entry Anim", "During Animation Action" }; // 0x01694554
static const char* PROP_REMOVAL_TIME_TAGS[5] = { nullptr, "Exit Animation", "Node Complete", "During Animation Action", "During Entry Anim" }; // 0x01694568

static const float PATH_TO_NODE_DISTANCE_SQRD = 0.01f; // 0x016AD038
static const float PLAYER_PATH_TO_NODE_DISTANCE_SQRD = 1.0f; // 0x016AD03C
static const vector DESTRUCTION_EVENT_EXPAND_BMAX_OFFSET(1.5f, 0.25f, 1.5f); // 0x016AD040
static const vector DESTRUCTION_EVENT_EXPAND_BMIN_OFFSET(-1.5f, -1.5f, -1.5f); // 0x016AD04C
// Magic number: 10 degrees (0x01319690), the stored float constant converted to double as rfg.exe's __CIcos argument.
static const float NEAR_NODE_FACING_DOT = (float)cos((double)0.17453f); // 0x0306E54C (0x01076140)

// Magic numbers.
static const float DEG_TO_RAD = 0.017453f;
static const float NO_ANIM_HEADING = FLT_MAX; // action_node_animation_info::anim_heading when the table has none
static const float MAX_PROP_DISTANCE_SQ = 16.0f; // a child prop further than 4 m from its node is not its prop
static const float PROP_CREATE_PLAYER_DISTANCE_SQ = 22500.0f; // props are created within 150 m of the player
static const float FIRE_CHECK_DISTANCE = 250.0f; // a scripted shot checks 250 m along the muzzle for friends
static const int STATE_ACTION_MS_PER_SECOND = 1000;
// check_action_node_definitions calls node_links_too_deep with these (0x00A6020C).
static const int NODE_LINK_START_DEPTH = 1;
static const int NODE_LINK_MAX_DEPTH = 4;

// 0x00A14FE0 (:89)
static action_node_prop_removal_time find_prop_removal_time(const char* removal_name)
{
    if (removal_name != nullptr)
    {
        for (int i = 0; i < 5; i++)
        {
            if (PROP_REMOVAL_TIME_TAGS[i] != nullptr && v_stricmp(removal_name, PROP_REMOVAL_TIME_TAGS[i]) == 0)
            {
                return (action_node_prop_removal_time)i;
            }
        }
    }
    return PROP_REMOVAL_TIME_NONE;
}

// 0x00A15030
static action_node_prop_creation_time find_prop_creation_time(const char* entry_name)
{
    if (entry_name == nullptr)
    {
        return PROP_CREATION_TIME_NONE;
    }
    for (int i = 0; i < 5; i++)
    {
        if (PROP_CREATION_TIME_TAGS[i] != nullptr && v_stricmp(entry_name, PROP_CREATION_TIME_TAGS[i]) == 0)
        {
            return (action_node_prop_creation_time)i;
        }
    }
    // Original: Int3 critical function here.
    return PROP_CREATION_TIME_NONE;
}

// 0x00A15090 (LTCG: xml_flag_root in EDI)
static bool flag_exists_in_xml_action_node(xml_element* xml_flag_root, const char* flag_tag)
{
    if (xml_flag_root == nullptr)
    {
        return false;
    }
    for (xml_element* cur = xml_find(xml_flag_root, "Flag"); cur != nullptr; cur = xml_find_next(xml_flag_root, cur, "Flag"))
    {
        if (v_stricmp(flag_tag, cur->text) == 0)
        {
            return true;
        }
    }
    return false;
}

// 0x00A150F0
static action_node_snap_target get_snap_target(const char* name)
{
    for (int i = 0; i < 3; i++)
    {
        if (SNAP_TO_TARGET_SELECTION[i] != nullptr && v_stricmp(SNAP_TO_TARGET_SELECTION[i], name) == 0)
        {
            return (action_node_snap_target)i;
        }
    }
    // Original: Int3 critical function here.
    return ACTION_NODE_SNAP_NONE;
}

// 0x00A15150 (LTCG: current_depth in ECX, max_depth in EDX)
static bool node_links_too_deep(const action_node_definition* base_node_info, int current_depth, int max_depth)
{
    while (current_depth < max_depth)
    {
        if (base_node_info == nullptr || (base_node_info = base_node_info->next_node_def) == nullptr)
        {
            return false;
        }
        current_depth++;
    }
    return true;
}

// 0x00A151A0 (:809)
void animation_effect_info::reset()
{
    anim_action = ANIMATION_ACTION_NONE;
    min_cooldown_ms = 0;
    max_cooldown_ms = 0;
}

// 0x00A33C50 (object_action_node_internal.h:82)
animation_effect_info::animation_effect_info()
{
    reset();
}

// 0x00A151B0 (:834)
animation_action action_node_animation_info::get_state_action_anim() const
{
    return exit_anim == ANIMATION_ACTION_NONE ? entry_anim : ANIMATION_ACTION_NONE;
}

// 0x00A38DE0 (:821)
void action_node_animation_info::reset()
{
    state_anim = ANIMATION_STATE_NONE;
    entry_anim = ANIMATION_ACTION_NONE;
    exit_anim = ANIMATION_ACTION_NONE;
    min_period_ms = 0;
    max_period_ms = 0;
    anim_start_offset.zero();
    anim_heading = NO_ANIM_HEADING;
    action_list.clear();
}

// 0x00A5B4E0 (object_action_node_internal.h:96)
action_node_animation_info::action_node_animation_info()
{
    reset();
}

// 0x00A38E20 (:946)
void action_node_definition::reset()
{
    name = nullptr;
    player_prompt = nullptr;
    memset(&node_flags, 0, sizeof(node_flags));
    action_anim_list.clear();
    cooldown_period_ms = 0;
    snap_target = ACTION_NODE_SNAP_NONE;
    action_type = nullptr;
    next_node_def = nullptr;
    required_node_def = nullptr;
    amb_prop_info = nullptr;
    prop_offset_pos.zero();
    prop_offset_target = ACTION_NODE_SNAP_NONE;
    prop_creation_time = PROP_CREATION_TIME_NONE;
    prop_removal_time = PROP_REMOVAL_TIME_NONE;
    prop_offset_rot_x = 0.0f;
    prop_offset_rot_y = 0.0f;
    prop_offset_rot_z = 0.0f;
    prop_pickup_trigger_id = -1;
    prop_drop_trigger_id = -1;
    hotspot_effect_info = 0xFFFFFFFF;
}

// 0x00A5B550 (object_action_node_internal.h:142)
action_node_definition::action_node_definition()
{
    reset();
}

// 0x00A151C0 (:974)
bool action_node_definition::load_prop_info(xml_element* xml_root)
{
    xml_element* element = xml_find(xml_root, "ambient_prop_type");
    if (element == nullptr)
    {
        return false;
    }
    amb_prop_info = amb_prop_info_find(element->text);
    if (amb_prop_info == nullptr)
    {
        return false;
    }
    if ((element = xml_find(xml_root, "prop_creation_time")) != nullptr)
    {
        prop_creation_time = find_prop_creation_time(element->text);
    }
    if ((element = xml_find(xml_root, "prop_removal_time")) != nullptr)
    {
        prop_removal_time = find_prop_removal_time(element->text);
    }
    xml_table_get_req_vector(prop_offset_pos, xml_root, "prop_offset_pos");
    xml_table_get_req_float(prop_offset_rot_x, xml_root, "prop_offset_rotation_x");
    xml_table_get_req_float(prop_offset_rot_y, xml_root, "prop_offset_rotation_y");
    xml_table_get_req_float(prop_offset_rot_z, xml_root, "prop_offset_rotation_z");
    prop_offset_rot_x *= DEG_TO_RAD;
    prop_offset_rot_y *= DEG_TO_RAD;
    prop_offset_rot_z *= DEG_TO_RAD;
    if ((element = xml_find(xml_root, "prop_offset_object")) != nullptr)
    {
        prop_offset_target = get_snap_target(element->text);
    }
    element = xml_find(xml_root, "pick_up_trigger");
    prop_pickup_trigger_id = element != nullptr ? animlib_get_trigger_id(element->text) : -1;
    element = xml_find(xml_root, "drop_trigger");
    prop_drop_trigger_id = element != nullptr ? animlib_get_trigger_id(element->text) : -1;
    return true;
}

// 0x00A15350 (:1159)
void types_object_action_node(signed char& type, signed char& subtype)
{
    type = OT_ACTION_NODE;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A15360 (:1159)
int bpo_index_object_action_node()
{
    return 1;
}

// 0x00A15370 (:1159)
const char* classname_object_action_node()
{
    return "object_action_node";
}

// 0x00A60620 (:1159)
object* alloc_object_action_node()
{
    return object_action_nodepool.alloc(0);
}

// Magic numbers: rfg.exe's arguments, the pool's bytes and capacity.
static obj_class_instantiator object_action_node_instantiator("object_action_node", alloc_object_action_node, types_object_action_node, bpo_index_object_action_node, classname_object_action_node, object_action_node::pool_remaining, 156320, 608); // 0x030FCE5C

// 0x005DBF10
const object_type object_action_node::get_type_static()
{
    return OT_ACTION_NODE;
}

// 0x005DBF20
const object_sub_type object_action_node::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A05610
const char* object_action_node::get_classname_static()
{
    return "object_action_node";
}

// 0x00A05620
int object_action_node::get_bpo_index_static()
{
    return 1;
}

// 0x00A60690
const char* object_action_node::get_classname() const
{
    return "object_action_node";
}

// 0x00A15380 (:1159)
unsigned int object_action_node::get_classname_hash()
{
    return object_action_node_instantiator.this_hash;
}

// 0x00A38E90 (:1159)
int object_action_node::pool_remaining()
{
    return object_action_nodepool.get_free();
}

// 0x00A8CA70 (:1159)
void object_action_node::pool_free()
{
    object_action_nodepool.free(this);
}

// 0x00A60630 (:1160)
object_action_node::object_action_node()
{
}

// 0x00A606A0
object_action_node::~object_action_node()
{
}

// 0x00A15390 (:1317)
obj_serialize_mode object_action_node::serialize_mode()
{
    return get_parent() != 0xFFFFFFFF ? OSM_FULL : OSM_RESTORE;
}

// 0x00A154A0 (:1335)
void object_action_node::get_local_bbox(vector& bmin, vector& bmax) const
{
    // Magic number: a node is a box of one metre around its position in every direction.
    bmax.x = 1.0f;
    bmax.y = 1.0f;
    bmax.z = 1.0f;
    bmin.x = -bmax.x;
    bmin.y = -bmax.y;
    bmin.z = -bmax.z;
}

// 0x00A153A0 (:1327)
void object_action_node::get_bbox(vector& bmin, vector& bmax) const
{
    get_local_bbox(bmin, bmax);
    bmin += pos;
    bmax += pos;
}

// 0x00A15520 (:1341)
void object_action_node::hierarchy_notify(hierarchy_notify_code code)
{
    if (code == HN_PARENT_DESTROYED)
    {
        World.destroy_object(this, false);
    }
}

// 0x005DBF30 (.h:113)
bool object_action_node::disabled() const
{
    return disabled_by != 0;
}

// 0x00A05630 (.h:116)
bool object_action_node::get_disabled_by(action_node_disabled_reason reason) const
{
    return (disabled_by & (1 << reason)) != 0;
}

// 0x005DBF40 (.h:121)
bool object_action_node::is_cooling_down() const
{
    return cooldown_period.valid() && !cooldown_period.elapsed();
}

// 0x00A155F0 (:1484)
const char* object_action_node::get_ambient_type() const
{
    return base_node_info->action_type;
}

// 0x00A15540 (:1374)
bool object_action_node::is_allowed_to_use(human_teams team) const
{
    switch (team)
    {
    case HUMAN_TEAM_GUERILLA:
    case HUMAN_TEAM_CIVILIAN:
        return base_node_info->node_flags.allow_guerrilla;
    case HUMAN_TEAM_EDF:
        return base_node_info->node_flags.allow_edf;
    case HUMAN_TEAM_MARAUDER:
        return base_node_info->node_flags.allow_marauder;
    default:
        // Original: Int3 critical function here.
        return false;
    }
}

// 0x00A155B0 (:1394)
bool object_action_node::is_allowed_to_use(const human* hp) const
{
    if (hp == nullptr)
    {
        return false;
    }
    if (object_is_player(hp))
    {
        return base_node_info->node_flags.allow_player;
    }
    // kept: a node that allows the player allows no NPC.
    return !base_node_info->node_flags.allow_player && is_allowed_to_use(hp->current_team);
}

// 0x00A38EA0 (:1491)
bool object_action_node::is_allowed_to_use(unsigned int human_handle) const
{
    const human* hp = human_handle != 0xFFFFFFFF ? World.object_from_handle<human>(human_handle) : nullptr;
    return is_allowed_to_use(hp);
}

// 0x00A606F0 (:1504)
human* object_action_node::get_occupant() const
{
    const int count = World.get_num_objects<human>();
    for (int i = 0; i < count; i++)
    {
        human* hp = World.get_object<human>(i);
        if (get_handle() != 0xFFFFFFFF && !hp->is_killed())
        {
            const farray<unsigned int,4>& set = hp->action_node_data.action_node_set;
            for (int j = 0; j < set.size(); j++)
            {
                if (set[j] == get_handle())
                {
                    return hp;
                }
            }
            if (hp->action_node_data.current_action_node == get_handle())
            {
                return hp;
            }
        }
    }
    return nullptr;
}

// 0x00A8CA80 (:1498)
bool object_action_node::is_occupied() const
{
    return get_occupant() != nullptr;
}

// 0x00A607A0 (:1519)
unsigned int object_action_node::get_occupant_handle() const
{
    const human* occupant = get_occupant();
    return occupant != nullptr ? occupant->get_handle() : 0xFFFFFFFF;
}

// The first time a node is disabled (inlined in several places of rfg.exe): an NPC using it leaves it.
static void action_node_on_disable(object_action_node* anp)
{
    if (anp->disabled_by == 0)
    {
        human* occupant = anp->get_occupant();
        if (occupant != nullptr && !object_is_player(occupant))
        {
            occupant->release_action_node(anp->get_handle());
        }
        anp->stop_hotspot_effect();
    }
}

// 0x00A8CA90 (:1533)
void object_action_node::set_disabled_by(action_node_disabled_reason reason)
{
    if (reason == ANDR_SAFE_HOUSE && base_node_info->node_flags.not_disabled_with_safehouse)
    {
        return;
    }
    action_node_on_disable(this);
    disabled_by |= 1 << reason;
}

// 0x00AA3BA0 (:1558)
void object_action_node::clear_disabled_by(action_node_disabled_reason reason)
{
    const int mask = 1 << reason;
    if ((disabled_by & mask) != 0)
    {
        disabled_by &= ~mask;
        if (disabled_by == 0)
        {
            start_hotspot_effect();
        }
    }
}

// 0x00A8CB90 (LTCG: target_enum in ECX, anp in EAX)
static object* get_generic_target_object(action_node_snap_target target_enum, const object_action_node* anp)
{
    if (target_enum == ACTION_NODE_SNAP_NONE)
    {
        return (object*)anp;
    }
    if (target_enum == ACTION_NODE_SNAP_PARENT)
    {
        return World.object_from_handle(anp->get_parent());
    }
    if (target_enum != ACTION_NODE_SNAP_SPAWN_CHILD)
    {
        // Original: Int3 critical function here.
        return nullptr;
    }
    object* node = World.object_from_handle(anp->get_parent());
    if (node == nullptr)
    {
        return nullptr;
    }
    switch (node->get_obj_type())
    {
    case OT_VEHICLE_SPAWN_NODE:
        return World.object_from_handle(((object_vehicle_spawn_node*)node)->get_child_vehicle_handle());
    case OT_NPC_SPAWN_NODE:
        return World.object_from_handle(((object_npc_spawn_node*)node)->get_child_npc_handle());
    case OT_SQUAD_SPAWN_NODE:
    {
        object_squad* squad = World.object_from_handle<object_squad>(((object_squad_spawn_node*)node)->get_child_squad_handle());
        if (squad == nullptr)
        {
            return nullptr;
        }
        npc* leader = squad->get_leader();
        return leader != nullptr ? (object*)leader : (object*)squad;
    }
    default:
        // Original: Int3 critical function here.
        return nullptr;
    }
}

// 0x00A8CC50 (:2152)
const object* object_action_node::get_prop_target_object() const
{
    return get_generic_target_object(base_node_info->prop_offset_target, this);
}

// 0x00A8CC60 (:2158)
const object* object_action_node::get_target_object() const
{
    return get_generic_target_object(base_node_info->snap_target, this);
}

// 0x00AA3BD0 (:1576)
bool object_action_node::has_collision_disabled() const
{
    if (!base_node_info->node_flags.disable_collision)
    {
        return false;
    }
    const object* target = get_target_object();
    return target != nullptr && havok_body_get_pointer(target->havok_handle) != nullptr;
}

// 0x00AA3C10 (:1601)
void object_action_node::disable_node_collision(human* hp)
{
    if (hp == nullptr || !base_node_info->node_flags.disable_collision)
    {
        return;
    }
    const object* target = get_target_object();
    if (target != nullptr && hp->object_filter_handle == 0xFFFFFFFF)
    {
        hp->object_filter_handle = target->get_handle();
    }
}

// 0x00A15600 (:1624)
void object_action_node::enable_node_collision(human* hp)
{
    hp->object_filter_handle = 0xFFFFFFFF;
}

// 0x00A38F00 (:1630)
int object_action_node::pick_action_state() const
{
    const int count = base_node_info->action_anim_list.size();
    return count > 1 ? int_rand_range(0, count - 1) : 0;
}

// 0x00A38F30 (:1645)
bool object_action_node::has_entry_anim() const
{
    return base_node_info->action_anim_list[0]->entry_anim != ANIMATION_ACTION_NONE;
}

// 0x00A38F50 (:1653)
bool object_action_node::has_exit_anim() const
{
    return base_node_info->action_anim_list[0]->exit_anim != ANIMATION_ACTION_NONE;
}

// 0x00A15620 (:1661)
bool object_action_node::look_at_parent() const
{
    return false;
}

// 0x00A15630 (:1668)
bool object_action_node::is_reusable() const
{
    return base_node_info->node_flags.reusable;
}

// 0x00A5FFD0 (LTCG: objp in EAX): the index of the first script point of the object's chunk with tag_name in its name.
static int get_action_node_script_index(const object* objp, const char* tag_name)
{
    if (objp == nullptr || objp->get_obj_type() != OT_MOVER)
    {
        return -1;
    }
    c_chunk_base* chunk = chunk_get_base_by_uid(((const object_mover*)objp)->chunk_uid);
    if (chunk == nullptr)
    {
        return -1;
    }
    for (int i = 0; i < chunk->num_script_points; i++)
    {
        if (strstr(chunk->script_points[i].name, tag_name) != nullptr)
        {
            return i;
        }
    }
    return -1;
}

// 0x00A8CC70 (:2164)
bool object_action_node::get_entry_point_int(const action_node_animation_info* anim_info, vector& entry_point) const
{
    if (anim_info == nullptr)
    {
        return false;
    }
    const bool has_offset = anim_info->anim_heading != NO_ANIM_HEADING;
    const object* target = get_target_object();
    if (target == nullptr || !target->obj_flags.streamed)
    {
        return false;
    }
    if (base_node_info->node_flags.script_point_enable)
    {
        object_action_node* self = const_cast<object_action_node*>(this); // caches the index in a const query
        if (target->get_handle() != last_target_object_handle)
        {
            self->action_node_script_index = get_action_node_script_index(target, "Action_Node");
            self->last_target_object_handle = target->get_handle();
        }
        if (action_node_script_index > -1)
        {
            vector tag_pos;
            matrix tag_orient;
            target->get_tag_world_coords(action_node_script_index, tag_pos, &tag_orient, nullptr, nullptr);
            if (has_offset)
            {
                entry_point = anim_info->anim_start_offset;
                vec_change_to_global_coords(entry_point, tag_pos, tag_orient);
            }
            else
            {
                entry_point = tag_pos;
            }
            return true;
        }
    }
    if (has_offset)
    {
        entry_point = anim_info->anim_start_offset;
        vec_change_to_global_coords(entry_point, target->pos, target->orient);
    }
    else
    {
        entry_point = target->pos;
    }
    return true;
}

// 0x00AA3C50 (:1675)
bool object_action_node::get_entry_point(const human* hp, vector& entry_point) const
{
    if (disabled_by != 0)
    {
        return false;
    }
    const int index = hp != nullptr && hp->has_node_reserved(get_handle()) ? hp->get_action_node_index(this) : 0;
    return get_entry_point_int(base_node_info->action_anim_list[index], entry_point);
}

// 0x00AA4270 (:2220)
bool object_action_node::at_entry_point(const player* pp) const
{
    if (disabled_by != 0)
    {
        return false;
    }
    const int index = pp != nullptr && pp->has_node_reserved(get_handle()) ? pp->get_action_node_index(this) : 0;
    vector entry_point;
    return get_entry_point_int(base_node_info->action_anim_list[index], entry_point) && vec_dist_squared(entry_point, pp->pos) <= PLAYER_PATH_TO_NODE_DISTANCE_SQRD;
}

// 0x00AA4300 (:2242)
bool object_action_node::at_entry_point(const npc* np) const
{
    const human* occupant = get_occupant();
    if (occupant == nullptr || occupant != np)
    {
        return false;
    }
    if (get_handle() != 0xFFFFFFFF && np->action_node_data.current_action_node == get_handle())
    {
        return true;
    }
    vector entry_point;
    if (!get_entry_point(np, entry_point) || entry_point.is_null() || vec_xz_dist_squared(entry_point, np->pos) > PATH_TO_NODE_DISTANCE_SQRD)
    {
        return false;
    }
    matrix entry_rotation;
    return get_entry_rotation(np, entry_rotation) && NEAR_NODE_FACING_DOT <= np->orient.fvec * entry_rotation.fvec;
}

// 0x00AA43E0 (:2286)
bool object_action_node::at_entry_point(const human* hp) const
{
    if (hp != nullptr && hp->get_obj_type() == OT_HUMAN)
    {
        if (hp->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            return at_entry_point((const npc*)hp);
        }
        if (hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            return at_entry_point((const player*)hp);
        }
    }
    // Original: Int3 critical function here.
    return false;
}

// 0x00A8CDD0 (:2307)
bool object_action_node::get_entry_rotation(const human* hp, matrix& entry_rotation) const
{
    if (disabled_by != 0)
    {
        return false;
    }
    const action_node_animation_info* anim_info = base_node_info->action_anim_list[hp != nullptr ? hp->get_action_node_index(this) : 0];
    const object* target = get_target_object();
    if (target == nullptr || !target->obj_flags.streamed)
    {
        return false;
    }
    entry_rotation = target->orient;
    if (base_node_info->node_flags.script_point_enable)
    {
        object_action_node* self = const_cast<object_action_node*>(this); // caches the index in a const query
        if (target->get_handle() != last_target_object_handle)
        {
            self->action_node_script_index = get_action_node_script_index(target, "Action_Node");
            self->last_target_object_handle = target->get_handle();
        }
        if (action_node_script_index > -1)
        {
            vector tag_pos;
            target->get_tag_world_coords(action_node_script_index, tag_pos, &entry_rotation, nullptr, nullptr);
        }
    }
    entry_rotation.fvec.y = 0.0f;
    entry_rotation.rvec.y = 0.0f;
    entry_rotation.uvec = UP_VECTOR;
    entry_rotation.fix();
    const float heading = anim_info->anim_heading;
    if (heading != 0.0f && heading != NO_ANIM_HEADING)
    {
        entry_rotation.rotate_around_local_y(heading);
    }
    return true;
}

// 0x00A391A0 (:2685)
bool object_action_node::has_entered_node(const human* hp) const
{
    return hp->has_node_reserved(get_handle()) && get_handle() != 0xFFFFFFFF && hp->action_node_data.current_action_node == get_handle();
}

// 0x00A05940 (global_event_data.h:263)
human_using_action_node_event::human_using_action_node_event()
    : event_data(EVENT_HUMAN_USING_ACTION_NODE)
{
}

static void post_human_using_action_node(const object_action_node* anp, const human* hp, bool start)
{
    human_using_action_node_event event;
    event.action_node_handle = anp->get_handle();
    event.human_handle = hp->get_handle();
    event.start = start;
    gem_post_event(event);
}

// 0x00AB9E00 (:2700)
bool object_action_node::enter_node(human* hp)
{
    if (!hp->has_node_reserved(get_handle()))
    {
        return false;
    }
    if (has_entered_node(hp))
    {
        return true;
    }
    if (!at_entry_point(hp))
    {
        return false;
    }
    disable_node_collision(hp);
    const int index = hp->get_action_node_index(this);
    hp->action_node_data.existing_action_state = index;
    const action_node_animation_info* anim_info = base_node_info->action_anim_list[index];
    const int duration = int_rand_range(anim_info->min_period_ms, anim_info->max_period_ms);
    if (duration <= 0 || !hp->enter_action_node(this, duration))
    {
        return false;
    }
    const animation_action entry_anim = anim_info->entry_anim;
    if (object_is_player(hp) && base_node_info->node_flags.hide_player_weapon)
    {
        human_weapons_immediately_holster_weapon(World.object_cast<player>(hp));
    }
    if (entry_anim != ANIMATION_ACTION_NONE)
    {
        vector entry_point;
        matrix entry_rotation;
        if (!get_entry_point(hp, entry_point) || !get_entry_rotation(hp, entry_rotation))
        {
            // Original: Int3 critical function here.
            return false;
        }
        // Magic number: rfg.exe's play flags.
        hp->char_inst->play_action(entry_anim, ANIM_ALIAS_TRANSITION_TO_ACTION_NODE, 1.0f, 0x4020);
        if (base_node_info->node_flags.weapon_cabinet)
        {
            item* cabinet = World.object_from_handle<item>(get_parent());
            if (cabinet != nullptr && cabinet->char_inst != nullptr)
            {
                cabinet->char_inst->set_state(index_alias(ANIMATION_STATE_OPEN, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
                cabinet->char_inst->play_action(ANIMATION_ACTION_OPEN_WEAPON_LOCKER, ANIM_ALIAS_NONE, 1.0f, 0);
            }
        }
        hp->block_movement_anim = entry_anim;
        hp->block_steering_vector.zero();
        hp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
        hp->steering_vector.zero();
        // Magic number: 0.3 s to swing onto the node.
        hp->char_inst->set_action_starting_orient(hp->pos, hp->orient, entry_point, entry_rotation, ANIM_ALIAS_TRANSITION_TO_ACTION_NODE, 0.3f);
    }
    action_cooldown_timer.set(0);
    const animation_state state_anim = anim_info->state_anim;
    hp->custom_fire_animation = ANIMATION_ACTION_NONE;
    hp->hflags.override_default_anim_state = 1;
    hp->hflags.override_default_fire_anim = 0;
    if (state_anim == ANIMATION_STATE_NONE)
    {
        hp->char_inst->set_state(index_alias(ANIMATION_STATE_STAND, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
        hp->move_speed = -1.0f;
        hp->cur_state = HS_STANDARD;
        hp->cur_movement_state = HMS_IDLE;
    }
    else
    {
        hp->char_inst->set_state(index_alias(state_anim, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
    }
    post_human_using_action_node(this, hp, true);
    return true;
}

// A node that is not reusable is spent when its user leaves.
static void action_node_spend_one_time_use(object_action_node* anp)
{
    if (!anp->base_node_info->node_flags.reusable)
    {
        action_node_on_disable(anp);
        anp->disabled_by |= 1 << ANDR_ONE_TIME_USE_ONLY;
    }
}

// 0x00A8D170 (:2604)
void object_action_node::exit_node(human* hp)
{
    int state = hp->action_node_data.existing_action_state;
    if (get_handle() != hp->action_node_data.current_action_node)
    {
        return;
    }
    hp->action_node_data.current_action_node = 0xFFFFFFFF;
    hp->action_node_data.state_duration.invalidate();
    if (state >= base_node_info->action_anim_list.size())
    {
        // Original: Int3 critical function here.
        state = 0;
    }
    const animation_action exit_anim = base_node_info->action_anim_list[state]->exit_anim;
    if (object_is_player(hp) && base_node_info->node_flags.hide_player_weapon)
    {
        World.object_cast<player>(hp)->hide_weapon();
    }
    if (exit_anim == ANIMATION_ACTION_NONE)
    {
        hp->block_movement_anim = -1;
    }
    else
    {
        // Magic number: rfg.exe's play flags.
        hp->char_inst->play_action(exit_anim, ANIM_ALIAS_TRANSITION_FROM_ACTION_NODE, 1.0f, 0x4020);
        const npc* np = hp->get_sub_type() == OT_SUB_HUMAN_NPC ? (const npc*)hp : nullptr;
        const ai_system* ai = np != nullptr ? np->get_ai() : nullptr;
        if (hp->action_node_data.exit_quick || (ai != nullptr && ai->is_fleeing()))
        {
            vanim_set_variant(hp->char_inst->vp, ANIM_ALIAS_TRANSITION_FROM_ACTION_NODE, ANIM_VARIANT_QUICK, true, true);
        }
        if (base_node_info->node_flags.weapon_cabinet && get_parent() != 0xFFFFFFFF)
        {
            item* cabinet = World.object_from_handle<item>(get_parent());
            if (cabinet != nullptr && cabinet->char_inst != nullptr)
            {
                cabinet->char_inst->set_state(index_alias(ANIMATION_STATE_CLOSED, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
                cabinet->char_inst->play_action(ANIMATION_ACTION_CLOSE_WEAPON_LOCKER, ANIM_ALIAS_NONE, 1.0f, 0);
            }
        }
        hp->block_movement_anim = exit_anim;
        hp->block_steering_vector.zero();
        hp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
        hp->steering_vector.zero();
    }
    hp->hflags.override_default_anim_state = 0;
    hp->hflags.override_default_fire_anim = 0;
    hp->custom_fire_animation = ANIMATION_ACTION_NONE;
    action_node_spend_one_time_use(this);
    post_human_using_action_node(this, hp, false);
}

// 0x00A8CFE0 (:2542)
void object_action_node::emergency_exit(human* hp)
{
    if (get_handle() != hp->action_node_data.current_action_node)
    {
        return;
    }
    hp->action_node_data.current_action_node = 0xFFFFFFFF;
    hp->action_node_data.state_duration.invalidate();
    hp->hflags.override_default_anim_state = 0;
    hp->hflags.override_default_fire_anim = 0;
    hp->block_movement_anim = -1;
    hp->custom_fire_animation = ANIMATION_ACTION_NONE;
    action_node_spend_one_time_use(this);
    hp->object_filter_handle = 0xFFFFFFFF;
    // Magic number: 5 ramp out frames.
    static const int ALIASES[4] = { ANIM_ALIAS_TRANSITION_TO_ACTION_NODE, ANIM_ALIAS_TRANSITION_FROM_ACTION_NODE, ANIM_ALIAS_ACTION_NODE, ANIM_ALIAS_ACTION_NODE_ACTION };
    for (int i = 0; i < 4; i++)
    {
        if (hp->char_inst->is_playing_action(ALIASES[i]))
        {
            hp->char_inst->stop_action(ALIASES[i], 5, false);
            break;
        }
    }
    v_animate* vp = hp->char_inst->vp;
    for (int i = 0; i < vp->max_simultaneous_anims && vp->anims[i].used; i++)
    {
        vp->anims[i].move_to_end_pos = 0;
    }
    vp->flags.use_velocity = 0;
    post_human_using_action_node(this, hp, false);
}

// 0x00A156F0 (:2423)
void object_action_node::set_cooldown_time()
{
    cooldown_period.set(base_node_info->cooldown_period_ms);
}

// 0x00A156D0 (:2385)
bool object_action_node::allow_player() const
{
    return base_node_info->node_flags.allow_player != 0;
}

// 0x00A156E0 (:2391)
bool object_action_node::disabled_by_destruction() const
{
    return base_node_info->node_flags.disable_by_destruction;
}

// 0x005DBF70 (.h:198)
void object_action_node::clear_cooldown()
{
    cooldown_period.set(0);
}

// 0x00AA4430 (:2803)
bool object_action_node::play_action(human* hp)
{
    if (!has_entered_node(hp))
    {
        return false;
    }
    const action_node_animation_info* anim_info = base_node_info->action_anim_list[hp->get_action_node_index(this)];
    const animation_action anim = anim_info->get_state_action_anim();
    if (anim != ANIMATION_ACTION_NONE)
    {
        hp->char_inst->play_action(anim, ANIM_ALIAS_ACTION_NODE_ACTION, 1.0f, 0);
        hp->block_movement_anim = anim;
        hp->block_steering_vector.zero();
        hp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
        vector entry_point;
        matrix entry_rotation;
        if (get_entry_point(hp, entry_point) && get_entry_rotation(hp, entry_rotation))
        {
            // Magic number: 0.25 s to swing onto the node.
            hp->char_inst->set_action_starting_orient(hp->pos, hp->orient, entry_point, entry_rotation, ANIM_ALIAS_ACTION_NODE_ACTION, 0.25f);
        }
    }
    return true;
}

// 0x00A608E0 (:2842)
bool object_action_node::doing_action(human* hp)
{
    return get_occupant() == hp && hp->char_inst->is_playing_action(ANIM_ALIAS_ACTION_NODE_ACTION);
}

// 0x00A15790 (:2536)
const char* object_action_node::get_player_action_display_prompt() const
{
    return base_node_info->player_prompt;
}

// 0x00A15710 (:2429)
bool object_action_node::player_hide_weapon() const
{
    return base_node_info->node_flags.hide_player_weapon;
}

// 0x00A15720 (:2435)
bool object_action_node::is_weapon_cabinet_action_node() const
{
    return base_node_info->node_flags.weapon_cabinet;
}

// 0x00A15730 (:2441)
bool object_action_node::is_free_hostage_action_node() const
{
    return base_node_info->node_flags.free_hostage;
}

// 0x00A60880 (:2519)
animation_action object_action_node::get_player_animation(const player* pp) const
{
    return base_node_info->action_anim_list[pp->get_action_node_index(this)]->get_state_action_anim();
}

// 0x00A39170 (:2530)
animation_state object_action_node::get_animation_state(int node_idx) const
{
    if (node_idx > -1 && node_idx < base_node_info->action_anim_list.size())
    {
        return base_node_info->action_anim_list[node_idx]->state_anim;
    }
    return ANIMATION_STATE_NONE;
}

// 0x00A390E0 (:2447)
int object_action_node::get_action_node_type() const
{
    return base_node_info != nullptr ? Action_node_definition_pool.get_index(base_node_info) : -1;
}

// 0x00A39100 (:2453)
int object_action_node::get_next_action_node_type() const
{
    const action_node_definition* next = base_node_info->next_node_def;
    return next != nullptr ? Action_node_definition_pool.get_index(next) : -1;
}

// 0x00A15740 (:2463)
bool object_action_node::is_linked_to() const
{
    return base_node_info->node_flags.linked_to_node;
}

// 0x00A15750 (:2469)
bool object_action_node::has_pickup_prop_trigger() const
{
    return base_node_info->amb_prop_info != nullptr && base_node_info->prop_pickup_trigger_id > -1;
}

// 0x00A15770 (:2480)
bool object_action_node::has_drop_prop_trigger() const
{
    return base_node_info->amb_prop_info != nullptr && base_node_info->prop_drop_trigger_id > -1;
}

// 0x00A39120 (:2495)
item* object_action_node::get_pickup_object() const
{
    if (child_prop_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    item* prop = World.object_from_handle<item>(child_prop_handle);
    return prop != nullptr && vec_dist_squared(pos, prop->pos) < MAX_PROP_DISTANCE_SQ ? prop : nullptr;
}

// 0x00A8CAF0 (:1991)
void object_action_node::destroy_old_children()
{
    if (base_node_info->amb_prop_info == nullptr)
    {
        return;
    }
    const item* current_prop = get_pickup_object();
    object* child = child_ptr;
    while (child != nullptr)
    {
        if (child != current_prop && child->get_obj_type() == OT_ITEM && ((item*)child)->info == base_node_info->amb_prop_info->prop_item_info)
        {
            remove_child(child);
        }
        // kept: child_next is read after remove_child, as in rfg.exe.
        child = child->child_next;
        if (child == child_ptr)
        {
            break;
        }
    }
}

// 0x00AA40C0 (:2023)
void object_action_node::create_prop_object()
{
    if (base_node_info->prop_creation_time != PROP_CREATION_TIME_PRE_ENTRY_ANIM || get_pickup_object() != nullptr)
    {
        return;
    }
    destroy_old_children();
    if (vec_dist_squared(Player->pos, pos) > PROP_CREATE_PLAYER_DISTANCE_SQ)
    {
        return;
    }
    const object* target = get_prop_target_object();
    if (target == nullptr || base_node_info->amb_prop_info == nullptr)
    {
        return;
    }
    vector prop_pos = base_node_info->prop_offset_pos;
    vec_change_to_global_coords(prop_pos, target->pos, target->orient);
    matrix prop_orient = target->orient;
    // kept: only positive rotations are applied.
    if (base_node_info->prop_offset_rot_x > 0.0f)
    {
        prop_orient.rotate_around_local_x(base_node_info->prop_offset_rot_x);
    }
    if (base_node_info->prop_offset_rot_y > 0.0f)
    {
        prop_orient.rotate_around_local_y(base_node_info->prop_offset_rot_y);
    }
    if (base_node_info->prop_offset_rot_z > 0.0f)
    {
        prop_orient.rotate_around_local_z(base_node_info->prop_offset_rot_z);
    }
    item* prop = amb_prop_create(base_node_info->amb_prop_info, prop_pos, prop_orient, get_handle());
    if (prop != nullptr)
    {
        child_prop_handle = prop->get_handle();
    }
}

// 0x00A38F70 (:1703)
int object_action_node::get_pickup_trigger_id() const
{
    return has_pickup_prop_trigger() ? base_node_info->prop_pickup_trigger_id : -1;
}

// 0x00A38FE0 (:1748)
int object_action_node::get_drop_trigger_id() const
{
    return has_drop_prop_trigger() ? base_node_info->prop_drop_trigger_id : -1;
}

// 0x00A38F90 (:1713)
bool object_action_node::pickup_action_trigger_elapsed(human* hp) const
{
    return !has_pickup_prop_trigger() || vanim_has_trigger_elapsed(hp->char_inst->vp, base_node_info->prop_pickup_trigger_id, ANIM_ALIAS_ACTION_NODE_ACTION, -1);
}

// 0x00A607C0 (:1730)
bool object_action_node::drop_action_trigger_elapsed(human* hp) const
{
    return !has_drop_prop_trigger() || vanim_has_trigger_elapsed(hp->char_inst->vp, base_node_info->prop_drop_trigger_id, ANIM_ALIAS_ACTION_NODE_ACTION, -1);
}

// 0x00A15640 (:1758)
const ambient_prop_info* object_action_node::get_ambient_prop_info() const
{
    return base_node_info->amb_prop_info;
}

// 0x00A39000 (:1764)
bool object_action_node::pickup_trigger_elapsed(const human* hp) const
{
    return has_pickup_prop_trigger() && hp->char_inst->is_playing_action(ANIM_ALIAS_TRANSITION_TO_ACTION_NODE) && vanim_has_trigger_elapsed(hp->char_inst->vp, base_node_info->prop_pickup_trigger_id, ANIM_ALIAS_TRANSITION_TO_ACTION_NODE, -1);
}

// 0x00A39070 (:1779)
bool object_action_node::drop_trigger_elapsed(const human* hp) const
{
    return has_drop_prop_trigger() && hp->char_inst->is_playing_action(ANIM_ALIAS_TRANSITION_FROM_ACTION_NODE) && vanim_has_trigger_elapsed(hp->char_inst->vp, base_node_info->prop_drop_trigger_id, ANIM_ALIAS_TRANSITION_FROM_ACTION_NODE, -1);
}

// 0x00AA3CB0 (:1795)
void object_action_node::destroy_child_relations()
{
    destroy_old_children();
    item* prop = get_pickup_object();
    if (prop != nullptr && prop->get_parent() == get_handle())
    {
        remove_child(prop);
    }
    child_prop_handle = 0xFFFFFFFF;
}

// 0x00A15650 (:1815)
bool object_action_node::is_ped_only() const
{
    return base_node_info->node_flags.allow_amb_peds;
}

// 0x00A15660 (:1821)
bool object_action_node::is_patrol_only() const
{
    return base_node_info->node_flags.allow_patrols;
}

// 0x00A60810 (:1827)
bool object_action_node::has_ambient_actions(const human* hp) const
{
    if (hp == nullptr)
    {
        return false;
    }
    const int index = hp->has_node_reserved(get_handle()) ? hp->get_action_node_index(this) : 0;
    const action_node_animation_info* anim_info = base_node_info->action_anim_list[index];
    return anim_info != nullptr && anim_info->action_list.size() > 0;
}

// 0x00AA3D70 (:1889)
bool object_action_node::play_ambient_action(human* hp)
{
    if (hp == nullptr)
    {
        return false;
    }
    const int index = hp->has_node_reserved(get_handle()) ? hp->get_action_node_index(this) : 0;
    const action_node_animation_info* anim_info = base_node_info->action_anim_list[index];
    if (anim_info == nullptr || anim_info->action_list.size() < 1)
    {
        return false;
    }
    const int count = anim_info->action_list.size();
    const animation_effect_info& effect = anim_info->action_list[count == 1 ? 0 : int_rand_range(0, count - 1)];
    const animation_action anim = effect.anim_action;
    const int cooldown = int_rand_range(effect.min_cooldown_ms, effect.max_cooldown_ms);
    // kept: vanim_get_anim_length is given the action's index where it takes an alias. Seconds to ms (the decompile
    // loses the FMUL at 0x00AA3E2B).
    action_cooldown_timer.set((int)(vanim_get_anim_length(hp->char_inst->vp, anim) * 1000.0) + cooldown);
    if (anim == ANIMATION_ACTION_STAND_FIRE)
    {
        const weapon* wp = hp->get_equipped_weapon();
        if (wp != nullptr)
        {
            weapon_fire_info fire_info;
            vector muzzle_pos;
            matrix muzzle_orient;
            wp->get_muzzle_transform(muzzle_pos, &muzzle_orient);
            const vector end_pos = muzzle_pos + muzzle_orient.fvec * FIRE_CHECK_DISTANCE;
            fire_info.init(muzzle_pos, end_pos, wp->w_info);
            weapons_test_bullet(fire_info, nullptr, false, 0.0f, ZERO_VECTOR);
            const human* hit = World.object_from_handle<human>(fire_info.hit_handle);
            if (hit != nullptr && human_teams_are_friendly(hit, hp))
            {
                return true;
            }
        }
    }
    hp->char_inst->play_action(anim, ANIM_ALIAS_ACTION_NODE_ACTION, 1.0f, 0);
    vector entry_point;
    matrix entry_rotation;
    if (base_node_info->action_anim_list[0]->entry_anim == ANIMATION_ACTION_NONE && get_entry_point(hp, entry_point) && get_entry_rotation(hp, entry_rotation))
    {
        // kept: the action is started a second time, with the "move to end" flag (rfg.exe's play flags 0x20).
        if (hp->char_inst->play_action(anim, ANIM_ALIAS_ACTION_NODE_ACTION, 1.0f, 0x20) == 0xFFFFFFFF)
        {
            set_disabled_by(ANDR_MISSING_ANIMATION);
            return false;
        }
        hp->block_movement_anim = anim;
        hp->block_steering_vector.zero();
        hp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
        hp->steering_vector.zero();
        if (base_node_info->node_flags.ignore_y)
        {
            entry_point.y = hp->pos.y;
        }
        // Magic number: 0.5 s to swing onto the node.
        hp->char_inst->set_action_starting_orient(hp->pos, hp->orient, entry_point, entry_rotation, ANIM_ALIAS_ACTION_NODE_ACTION, 0.5f);
    }
    return true;
}

// 0x00A05650 (.h:280)
const farray<unsigned int,8>& object_action_node::get_node_links() const
{
    return node_links;
}

// 0x005DBF80 (.h:283)
bool object_action_node::is_high_priority() const
{
    return node_flags.high_priority;
}

// 0x00A05660 (.h:286)
bool object_action_node::has_infinite_duration() const
{
    return node_flags.infinite_duration;
}

// 0x005DBF90 (.h:289)
bool object_action_node::should_run_to() const
{
    return node_flags.run_to;
}

// 0x00A15670 (:1848)
bool object_action_node::prop_requirement_met(const human* hp) const
{
    return !base_node_info->node_flags.prop_required || base_node_info->amb_prop_info == hp->amb_prop_info;
}

// 0x005DBFA0 (.h:295)
bool object_action_node::ambient_action_ready() const
{
    return action_cooldown_timer.elapsed();
}

// 0x00A156A0 (:1859)
bool object_action_node::weapon_required() const
{
    return base_node_info->node_flags.weapon_required;
}

// 0x00A156B0 (:1864)
bool object_action_node::melee_weapon_required() const
{
    return base_node_info->node_flags.melee_weapon_required;
}

// 0x00A156C0 (:1869)
bool object_action_node::allow_ambient_voice_lines() const
{
    return base_node_info->node_flags.allow_ambient_lines;
}

// 0x00AA3D20 (:1876)
bool object_action_node::is_object_target(object* test_object)
{
    return get_target_object() == test_object;
}

// 0x00AA3D40 (:1882)
bool object_action_node::is_flyer_sabotage_node()
{
    const object* target = get_target_object();
    return target != nullptr && target->get_obj_type() == OT_VEHICLE && target->get_sub_type() == OT_SUB_VEHICLE_FLYER;
}

// 0x00A157A0 (:2901)
bool object_action_node::has_ground_marker_highlight() const
{
    return base_node_info != nullptr && base_node_info->node_flags.ground_marker_highlight;
}

// 0x00A8D3C0 (:2859)
void object_action_node::start_hotspot_effect()
{
    if (disabled_by != 0 || !obj_flags.streamed || World.object_from_handle(hotspot_effect_handle) != nullptr)
    {
        return;
    }
    const object* target = get_target_object();
    if (target == nullptr)
    {
        if (base_node_info->snap_target != ACTION_NODE_SNAP_SPAWN_CHILD)
        {
            return;
        }
        // A spawn child that is not there yet: the effect stays at the spawn node.
        target = World.object_from_handle<object_vehicle_spawn_node>(get_parent());
        if (target == nullptr)
        {
            return;
        }
    }
    vector offset = base_node_info->hotspot_effect_offset;
    if (base_node_info->node_flags.ground_marker_highlight)
    {
        vec_change_to_global_coords(offset, target->pos, target->orient);
        vec_change_to_local_coords(offset, pos, orient);
        hotspot_effect_handle = objective_highlight_ground_marker_add(get_handle(), 1.0f, offset, false, true);
    }
    else if (base_node_info->hotspot_effect_info != 0xFFFFFFFF)
    {
        vec_change_to_global_coords(offset, target->pos, target->orient);
        hotspot_effect_handle = effects_play(base_node_info->hotspot_effect_info, offset, IDENTITY_MATRIX, true, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    }
}

// 0x00A391E0 (:2912)
void object_action_node::stop_hotspot_effect()
{
    if (base_node_info->node_flags.ground_marker_highlight)
    {
        objective_highlight_ground_marker_remove(get_handle());
        return;
    }
    if (hotspot_effect_handle != 0xFFFFFFFF)
    {
        object_effect* effect = World.object_from_handle<object_effect>(hotspot_effect_handle);
        if (effect != nullptr)
        {
            effect->stop(object_effect::SS_STOP_EMITTING);
            hotspot_effect_handle = 0xFFFFFFFF;
        }
    }
}

// 0x00AA3510 (:1165)
bool object_action_node::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set(ACTION_NODE_PROP_ANIM_TYPE, base_node_info->name);
        pblock.set(ACTION_NODE_PROP_DISABLED, disabled_by);
        const bool yes = true;
        if (node_flags.high_priority)
        {
            pblock.set(ACTION_NODE_PROP_HIGH_PRIORITY, yes);
        }
        if (node_flags.infinite_duration)
        {
            pblock.set(ACTION_NODE_PROP_INFINITE_DURATION, yes);
        }
        if (node_flags.run_to)
        {
            pblock.set(ACTION_NODE_PROP_RUN_TO, yes);
        }
        const int count = node_links.size();
        if (count > 0)
        {
            unsigned int links[8];
            for (int i = 0; i < 8; i++)
            {
                links[i] = i < count ? node_links[i] : 0xFFFFFFFF;
            }
            pblock.set_buf(PROP_LINKS, (const unsigned char*)links, count * 4);
        }
        return true;
    }

    hotspot_effect_handle = 0xFFFFFFFF;
    cooldown_period.set(0);
    action_cooldown_timer.set(0);
    base_node_info = nullptr;
    action_node_script_index = -1;
    last_target_object_handle = 0xFFFFFFFF;
    child_prop_handle = 0xFFFFFFFF;
    memset(&node_flags, 0, sizeof(node_flags));
    // Magic number: an up vector further than this from straight up is straightened.
    static const float UP_TOLERANCE = 0.0001f;
    const vector up_diff = orient.uvec - UP_VECTOR;
    if (fabsf(up_diff.x) >= UP_TOLERANCE || fabsf(up_diff.y) >= UP_TOLERANCE || fabsf(up_diff.z) >= UP_TOLERANCE)
    {
        orient.fvec.y = 0.0f;
        orient.rvec.y = 0.0f;
        orient.uvec = UP_VECTOR;
        orient.fix();
    }
    char anim_type[256];
    pblock.get(ACTION_NODE_PROP_ANIM_TYPE, anim_type);
    action_node_definition* definition = nullptr;
    Action_node_definition_hash.get(anim_type, definition);
    base_node_info = definition;
    if (base_node_info == nullptr)
    {
        return false;
    }
    bool flag = false;
    if (pblock.get_optional(ACTION_NODE_PROP_HIGH_PRIORITY, flag))
    {
        node_flags.high_priority = flag;
    }
    if (pblock.get_optional(ACTION_NODE_PROP_INFINITE_DURATION, flag))
    {
        node_flags.infinite_duration = flag;
    }
    disabled_by = 0;
    pblock.get_optional(ACTION_NODE_PROP_DISABLED, disabled_by);
    if (pblock.get_optional(ACTION_NODE_PROP_RUN_TO, flag))
    {
        node_flags.run_to = flag;
    }
    const object* parent_objp = World.object_from_handle(get_parent());
    if (base_node_info->snap_target == ACTION_NODE_SNAP_PARENT && parent_objp == nullptr)
    {
        return false;
    }
    node_links.clear();
    node_flags.parented = parent_objp != nullptr;
    unsigned char* links = nullptr;
    int size = pblock.get_buf_optional(PROP_LINKS, links);
    if (size == 0)
    {
        size = pblock.get_buf_optional(PROP_OLD_LINKS, links);
    }
    if (size > 0)
    {
        const unsigned int* handles = (const unsigned int*)links;
        for (int i = 0; i < size / 4; i++)
        {
            if (handles[i] != 0xFFFFFFFF)
            {
                node_links.add(handles[i]);
            }
        }
    }
    return true;
}

// 0x00AA39A0 (:1298)
bool object_action_node::stream(bool load)
{
    if (load)
    {
        voice_timeout.set(0);
        start_hotspot_effect();
        // Magic numbers: the position check runs every 2 to 4 seconds.
        position_disabled_check_timer.set_rand(2000, 4000);
        return true;
    }
    if (obj_flags.destroy_on_stream)
    {
        World.destroy_object(this, false);
    }
    nav_cell_detour_action_node_removed(this);
    stop_hotspot_effect();
    return true;
}

// rfg.exe sums the products as doubles, y first (0x00AA3A7B).
static float up_dot(const vector& v)
{
    return (float)((double)v.y * (double)UP_VECTOR.y + (double)v.x * (double)UP_VECTOR.x + (double)v.z * (double)UP_VECTOR.z);
}

// 0x00AA3A10 (:1412)
bool object_action_node::get_position_disabled_status() const
{
    // Magic number: 5 degrees (0x012C95F8), the stored float constant converted to double as rfg.exe's __CIcos argument.
    static const float MIN_UP_DOT = (float)cos((double)0.087265f); // 0x035B5A18
    if (!base_node_info->node_flags.no_object_interaction)
    {
        const object* parent_objp = World.object_from_handle(get_parent());
        if (parent_objp != nullptr && up_dot(parent_objp->orient.uvec) < MIN_UP_DOT)
        {
            return true;
        }
    }
    const action_node_definition* required = base_node_info->required_node_def;
    if (required != nullptr && linked_occupied_node_of_type(node_links, required) == nullptr)
    {
        return true;
    }
    if (base_node_info->prop_creation_time == PROP_CREATION_TIME_PRE_ENTRY_ANIM && get_pickup_object() == nullptr)
    {
        return true;
    }
    const object* target = get_target_object();
    return target != nullptr && (target->is_killed() || !target->obj_flags.streamed || up_dot(target->orient.uvec) < MIN_UP_DOT);
}

// 0x00AB3AC0 (:1473)
void object_action_node::update_position_disabled_status()
{
    if (get_position_disabled_status())
    {
        action_node_on_disable(this);
        disabled_by |= 1 << ANDR_POSITION;
    }
    else if (get_disabled_by(ANDR_POSITION))
    {
        disabled_by &= ~(1 << ANDR_POSITION);
        if (disabled_by == 0)
        {
            start_hotspot_effect();
        }
    }
}

// 0x00A5FED0
const object_action_node* linked_occupied_node_of_type(const farray<unsigned int,8>& linked_node_handles, const action_node_definition* required_node_def)
{
    const int required_type = required_node_def != nullptr ? Action_node_definition_pool.get_index(required_node_def) : -1;
    const int count = World.get_num_objects<human>();
    for (int i = 0; i < count; i++)
    {
        const human* hp = World.get_object<human>(i);
        if (hp->is_killed() || hp->action_node_data.current_action_node == 0xFFFFFFFF)
        {
            continue;
        }
        const object_action_node* node = World.object_from_handle<object_action_node>(hp->action_node_data.current_action_node);
        if (node != nullptr && node->get_action_node_type() == required_type && linked_node_handles.find(node->get_handle(), false) > -1)
        {
            return node;
        }
    }
    return nullptr;
}

// 0x00A38CB0 (LTCG: ambient_action_name in EDI)
static const char* find_ambient_action_type(const char* ambient_action_name)
{
    for (int i = 0; i < All_ambient_action_types.size(); i++)
    {
        if (v_stricmp(All_ambient_action_types[i], ambient_action_name) == 0)
        {
            return All_ambient_action_types[i];
        }
    }
    return nullptr;
}

// 0x00A38CF0 (LTCG: new_anim_info in ECX, old_anim_info in EAX)
static bool same_action_node_info(const action_node_animation_info* new_anim_info, const action_node_animation_info* old_anim_info)
{
    if (new_anim_info->state_anim != old_anim_info->state_anim || new_anim_info->entry_anim != old_anim_info->entry_anim ||
        new_anim_info->exit_anim != old_anim_info->exit_anim || new_anim_info->min_period_ms != old_anim_info->min_period_ms ||
        new_anim_info->max_period_ms != old_anim_info->max_period_ms || new_anim_info->anim_heading != old_anim_info->anim_heading ||
        new_anim_info->action_list.size() != old_anim_info->action_list.size() ||
        vec_dist_squared(new_anim_info->anim_start_offset, old_anim_info->anim_start_offset) != 0.0f)
    {
        return false;
    }
    // kept: only the actions are compared, not their cooldowns.
    for (int i = 0; i < new_anim_info->action_list.size(); i++)
    {
        if (old_anim_info->action_list[i].anim_action != new_anim_info->action_list[i].anim_action)
        {
            return false;
        }
    }
    return true;
}

// 0x00A38D90
static void set_up_no_object_interaction_flag(action_node_definition* base_node_info)
{
    base_node_info->node_flags.no_object_interaction = 1;
    for (int i = 0; i < base_node_info->action_anim_list.size(); i++)
    {
        // kept: the loop tests the first entry on every pass.
        if (base_node_info->action_anim_list[0]->anim_heading != NO_ANIM_HEADING)
        {
            base_node_info->node_flags.no_object_interaction = 0;
            return;
        }
    }
}

// 0x00A60050 (LTCG: name in ESI)
static action_node_definition* find_node_info(const char* name)
{
    action_node_definition* definition = nullptr;
    if (name != nullptr)
    {
        Action_node_definition_hash.get(name, definition);
    }
    return definition;
}

// 0x00A60090
static void load_ambient_action_types()
{
    All_ambient_action_types.clear();
    xml_element* root = xml_table_parse("ambient_action_types.xtbl");
    for (xml_element* entry = xml_find(root, "ambient_action_types"); entry != nullptr; entry = xml_find_next(root, entry, "ambient_action_types"))
    {
        xml_element* name = xml_find(entry, "name");
        if (name == nullptr)
        {
            continue;
        }
        const char* type = Action_node_string_pool.add_unique(name->text);
        if (type != nullptr && All_ambient_action_types.size() < All_ambient_action_types.capacity())
        {
            All_ambient_action_types.add_unique(type);
        }
    }
    xml_table_close(root);
}

// 0x00A60130
static void set_up_linked_to_flags()
{
    Action_node_definition_pool.thread_access.enter();
    for (action_node_definition* definition = Action_node_definition_pool.get_first(0); definition != nullptr; definition = Action_node_definition_pool.get_next(definition))
    {
        if (definition->next_node_def != nullptr)
        {
            // The flag goes on the definition that is linked to (0x00A6015D, the decompile loses the pointer).
            const_cast<action_node_definition*>(definition->next_node_def)->node_flags.linked_to_node = 1;
        }
    }
    Action_node_definition_pool.thread_access.leave();
}

// 0x00A601B0
static void check_action_node_definitions()
{
    Action_node_definition_pool.thread_access.enter();
    for (action_node_definition* definition = Action_node_definition_pool.get_first(0); definition != nullptr; definition = Action_node_definition_pool.get_next(definition))
    {
        const action_node_definition* next = definition->next_node_def;
        bool bad = next != nullptr && node_links_too_deep(next, NODE_LINK_START_DEPTH, NODE_LINK_MAX_DEPTH);
        for (int depth = 0; !bad && next != nullptr; depth++)
        {
            // A circle through this definition, or a chain deeper than three.
            bad = next == definition || (next = next->next_node_def, depth > 3);
        }
        if (bad)
        {
            // Original: Int3 critical function here.
            definition->next_node_def = nullptr;
        }
    }
    Action_node_definition_pool.thread_access.leave();
}

// 0x00A602C0 (LTCG: xml_action_node_root in EAX, base_node_info in EDI)
static void build_node_links(action_node_definition* base_node_info, xml_element* xml_action_node_root)
{
    xml_element* element = xml_find(xml_action_node_root, "node_link_type");
    if (element != nullptr)
    {
        const action_node_definition* linked = find_node_info(element->text);
        if (linked != nullptr && linked != base_node_info)
        {
            base_node_info->next_node_def = linked;
        }
    }
}

// 0x00A60320 (LTCG: xml_action_node_root in EAX, base_node_info in EDI)
static void build_required_nodes(action_node_definition* base_node_info, xml_element* xml_action_node_root)
{
    xml_element* element = xml_find(xml_action_node_root, "required_node_type");
    if (element != nullptr)
    {
        const action_node_definition* required = find_node_info(element->text);
        if (required != nullptr && required != base_node_info)
        {
            base_node_info->required_node_def = required;
        }
    }
}

// A time in the table is in seconds; a negative one counts as none.
static int xml_get_seconds_as_ms(xml_element* element)
{
    const int ms = xml_get_int(element, 0) * STATE_ACTION_MS_PER_SECOND;
    return ms < 0 ? 0 : ms;
}

// 0x00A60380 (:845)
bool action_node_animation_info::load(xml_element* xml_root)
{
    reset();
    xml_element* element = xml_find(xml_root, "anim_state");
    if (element != nullptr)
    {
        state_anim = vanim_lookup_state(element->text);
        if (state_anim == ANIMATION_STATE_NONE)
        {
            return false;
        }
    }
    if ((element = xml_find(xml_root, "min_time")) == nullptr)
    {
        return false;
    }
    min_period_ms = xml_get_seconds_as_ms(element);
    if ((element = xml_find(xml_root, "max_time")) == nullptr)
    {
        return false;
    }
    max_period_ms = xml_get_int(element, 0) * STATE_ACTION_MS_PER_SECOND;
    if (max_period_ms < min_period_ms)
    {
        max_period_ms = min_period_ms;
    }
    if ((element = xml_find(xml_root, "entry_anim")) != nullptr)
    {
        entry_anim = vanim_lookup_action(element->text);
        if (entry_anim == ANIMATION_ACTION_NONE)
        {
            return false;
        }
    }
    if ((element = xml_find(xml_root, "exit_anim")) != nullptr)
    {
        exit_anim = vanim_lookup_action(element->text);
        if (exit_anim == ANIMATION_ACTION_NONE)
        {
            return false;
        }
    }
    if ((element = xml_find(xml_root, "object_interaction_info")) != nullptr)
    {
        xml_table_get_req_vector(anim_start_offset, element, "anim_offset");
        float heading;
        xml_table_get_req_float(heading, element, "anim_heading");
        anim_heading = heading * DEG_TO_RAD;
    }
    xml_element* grid = xml_find(xml_root, "state_action_grid");
    if (grid != nullptr)
    {
        for (xml_element* info = xml_find(grid, "state_action_info"); info != nullptr; info = xml_find_next(grid, info, "state_action_info"))
        {
            animation_effect_info effect;
            xml_element* action = xml_find(info, "state_action");
            if (action == nullptr || (effect.anim_action = vanim_lookup_action(action->text)) == ANIMATION_ACTION_NONE)
            {
                continue;
            }
            xml_element* min_cooldown = xml_find(info, "action_cooldown_min");
            xml_element* max_cooldown = xml_find(info, "action_cooldown_max");
            if (min_cooldown != nullptr && max_cooldown != nullptr)
            {
                effect.min_cooldown_ms = xml_get_seconds_as_ms(min_cooldown);
                effect.max_cooldown_ms = xml_get_seconds_as_ms(max_cooldown);
            }
            action_list.add(effect);
        }
    }
    return true;
}

// 0x00A8C4E0: entries identical to one already loaded are shared.
static const action_node_animation_info* create_load_anim_action(xml_element* xml_root)
{
    action_node_animation_info* info = Action_anim_info_pool.alloc(0);
    if (info == nullptr)
    {
        return nullptr;
    }
    if (!info->load(xml_root))
    {
        Action_anim_info_pool.free(info);
        return nullptr;
    }
    Action_anim_info_pool.thread_access.enter();
    for (action_node_animation_info* other = Action_anim_info_pool.get_first(0); other != nullptr; other = Action_anim_info_pool.get_next(other))
    {
        if (other != info && same_action_node_info(info, other))
        {
            Action_anim_info_pool.free(info);
            Action_anim_info_pool.thread_access.leave();
            return other;
        }
    }
    Action_anim_info_pool.thread_access.leave();
    return info;
}

// 0x00A8C590 (:1034)
bool action_node_definition::load(xml_element* xml_root)
{
    reset();
    xml_element* element = xml_find(xml_root, "Name");
    if (element == nullptr || (name = Action_node_string_pool.add_unique(element->text)) == nullptr)
    {
        return false;
    }
    xml_element* flags = xml_find(xml_root, "node_flags");
    if (flags == nullptr)
    {
        // Original: Int3 critical function here.
        return false;
    }
    node_flags.allow_edf = flag_exists_in_xml_action_node(flags, "allow_edf");
    node_flags.allow_guerrilla = flag_exists_in_xml_action_node(flags, "allow_guerrilla");
    node_flags.allow_marauder = flag_exists_in_xml_action_node(flags, "allow_marauder");
    node_flags.allow_player = flag_exists_in_xml_action_node(flags, "allow_player");
    node_flags.allow_patrols = flag_exists_in_xml_action_node(flags, "allow_patrol");
    node_flags.allow_amb_peds = flag_exists_in_xml_action_node(flags, "allow_ambient_peds");
    node_flags.disable_by_destruction = flag_exists_in_xml_action_node(flags, "disable_by_destruction");
    node_flags.reusable = flag_exists_in_xml_action_node(flags, "reusable");
    node_flags.hide_player_weapon = flag_exists_in_xml_action_node(flags, "hide_player_weapon");
    node_flags.disable_collision = flag_exists_in_xml_action_node(flags, "disable_collision");
    node_flags.script_point_enable = flag_exists_in_xml_action_node(flags, "script_point_enable");
    node_flags.weapon_required = flag_exists_in_xml_action_node(flags, "weapon_required");
    node_flags.weapon_cabinet = flag_exists_in_xml_action_node(flags, "weapon_cabinet");
    node_flags.free_hostage = flag_exists_in_xml_action_node(flags, "free_hostage");
    node_flags.ground_marker_highlight = flag_exists_in_xml_action_node(flags, "ground_marker_highlight");
    node_flags.ignore_y = flag_exists_in_xml_action_node(flags, "ignore_y");
    node_flags.male_only = flag_exists_in_xml_action_node(flags, "male_only");
    node_flags.melee_weapon_required = flag_exists_in_xml_action_node(flags, "melee_weapon_required");
    node_flags.allow_ambient_lines = flag_exists_in_xml_action_node(flags, "allow_ambient_lines");
    node_flags.not_disabled_with_safehouse = flag_exists_in_xml_action_node(flags, "not_disabled_with_safehouse");
    if ((element = xml_find(xml_root, "snap_to_target")) != nullptr)
    {
        snap_target = get_snap_target(element->text);
    }
    if ((element = xml_find(xml_root, "ambient_action_type")) != nullptr)
    {
        action_type = find_ambient_action_type(element->text);
    }
    // kept: the table's tag is spelled this way.
    element = xml_find(xml_root, "ambinet_prop_info");
    if (element != nullptr && !load_prop_info(element))
    {
        return false;
    }
    if (node_flags.allow_player && (element = xml_find(xml_root, "player_prompt")) != nullptr)
    {
        player_prompt = Action_node_string_pool.add_unique(element->text);
        if (player_prompt == nullptr)
        {
            return false;
        }
    }
    if ((element = xml_find(xml_root, "cooldown_period")) == nullptr)
    {
        return false;
    }
    cooldown_period_ms = xml_get_seconds_as_ms(element);
    xml_element* list = xml_find(xml_root, "anim_state_list");
    if (list == nullptr)
    {
        return false;
    }
    for (xml_element* entry = xml_find(list, "anim_state_entry"); entry != nullptr; entry = xml_find_next(list, entry, "anim_state_entry"))
    {
        if (action_anim_list.size() >= action_anim_list.capacity())
        {
            // kept: a full list returns at once, without the hotspot effect or the prop_required flag.
            return true;
        }
        const action_node_animation_info* anim_info = create_load_anim_action(entry);
        if (anim_info == nullptr)
        {
            return false;
        }
        action_anim_list.add(anim_info);
    }
    if (action_anim_list.size() < 1)
    {
        return false;
    }
    if ((element = xml_find(xml_root, "hotspot_effect")) != nullptr)
    {
        const char* effect_name = xml_table_get_opt_string_ref(element, "effect");
        if (effect_name != nullptr)
        {
            hotspot_effect_info = effects_get_handle(effect_name);
            xml_table_get_req_vector(hotspot_effect_offset, element, "object_effect_offset");
        }
    }
    node_flags.prop_required = 0;
    if (amb_prop_info != nullptr)
    {
        node_flags.prop_required = prop_creation_time == PROP_CREATION_TIME_NONE;
    }
    return true;
}

// 0x00AA3190 (:731)
void load_action_node_definitions(int dlc_id)
{
    xml_element* root;
    if (dlc_id == -1)
    {
        load_ambient_action_types();
        root = xml_table_parse("action_node_types.xtbl");
    }
    else
    {
        char filename[256];
        sprintf(filename, "dlc%02d_%s", dlc_id, "action_node_types.xtbl");
        root = xml_table_parse(filename);
    }
    for (xml_element* entry = xml_find(root, "Ambient_Behaviors"); entry != nullptr; entry = xml_find_next(root, entry, "Ambient_Behaviors"))
    {
        xml_element* name = xml_find(entry, "Name");
        if (name == nullptr)
        {
            continue;
        }
        action_node_definition* definition = find_node_info(name->text);
        const bool is_new = definition == nullptr;
        if (is_new)
        {
            definition = Action_node_definition_pool.alloc(0);
            if (definition == nullptr)
            {
                continue;
            }
        }
        if (definition->load(entry))
        {
            if (is_new)
            {
                Action_node_definition_hash.insert_key_data(definition->name, definition);
            }
        }
        else
        {
            // kept: a definition that fails a DLC reload goes back to the pool while the hash still names it.
            Action_node_definition_pool.free(definition);
        }
    }
    for (xml_element* entry = xml_find(root, "Ambient_Behaviors"); entry != nullptr; entry = xml_find_next(root, entry, "Ambient_Behaviors"))
    {
        xml_element* name = xml_find(entry, "Name");
        action_node_definition* definition = name != nullptr ? find_node_info(name->text) : nullptr;
        if (definition != nullptr)
        {
            build_node_links(definition, entry);
            build_required_nodes(definition, entry);
            set_up_no_object_interaction_flag(definition);
        }
    }
    xml_table_close(root);
    check_action_node_definitions();
    set_up_linked_to_flags();
}

// 0x00A8D530
static void do_frmae_create_effects()
{
    const int count = World.get_num_objects<object_action_node>();
    for (int i = 0; i < count; i++)
    {
        object_action_node* node = World.get_object<object_action_node>(i);
        if (node->obj_flags.streamed && node->base_node_info != nullptr && node->base_node_info->node_flags.ground_marker_highlight)
        {
            node->start_hotspot_effect();
        }
    }
}

// 0x00AA4560
static void do_frame_create_objects()
{
    const int count = World.get_num_objects<object_action_node>();
    for (int i = 0; i < count; i++)
    {
        object_action_node* node = World.get_object<object_action_node>(i);
        if (node->base_node_info->prop_creation_time == PROP_CREATION_TIME_PRE_ENTRY_ANIM)
        {
            node->create_prop_object();
        }
    }
}

// 0x00AB3B20 (:2983)
void action_node_do_frame()
{
    if (Release_poll_timer.elapsed())
    {
        // Magic numbers: rfg.exe's poll times in ms. do_frame_release_nodes (0x00A157C0) is empty.
        Release_poll_timer.set_rand(1000, 4000);
    }
    if (Object_create_timer.elapsed())
    {
        Object_create_timer.set_rand(1000, 2000);
        do_frame_create_objects();
    }
    if (Effect_create_timer.elapsed())
    {
        Effect_create_timer.set_rand(1000, 2000);
        do_frmae_create_effects();
    }
    const int count = World.get_num_objects<object_action_node>();
    for (int i = 0; i < count; i++)
    {
        object_action_node* node = World.get_object<object_action_node>(i);
        if (node->position_disabled_check_timer.elapsed())
        {
            node->update_position_disabled_status();
            node->position_disabled_check_timer.set_rand(2000, 4000);
        }
    }
}

// 0x00AA45C0 (:3019)
void action_node_system_init()
{
    Release_poll_timer.set(0);
    Object_create_timer.set(0);
    Effect_create_timer.set(0);
    Action_node_definition_pool.clear();
    Action_anim_info_pool.clear();
    if (Action_node_string_pool_data == nullptr)
    {
        // Magic number: rfg.exe's string table size.
        static const unsigned int ACTION_NODE_STRING_POOL_SIZE = 0xC00;
        Action_node_string_pool_data = (char*)Permanent_mempool.alloc(ACTION_NODE_STRING_POOL_SIZE, 0);
        if (Action_node_string_pool_data == nullptr)
        {
            keen::debug::nativePrintArgs("Insufficient memory to allocate the action type name string table");
        }
        Action_node_string_pool.init(Action_node_string_pool_data, ACTION_NODE_STRING_POOL_SIZE, "Action Node String Pool", 0, 0);
    }
    Action_node_string_pool.clear();
    Action_node_definition_hash.clear();
    load_action_node_definitions(-1);
}

// 0x00A60910 (:3049)
bool action_node_collect_nodes(const npc* np, float radius, farray<object_action_node const*,608>& node_list, bool ped_only, bool patrol_only)
{
    node_list.clear();
    unsigned int handles[608];
    const int count = World.collect_objects<object_action_node>(np->pos, radius, handles, 608, nullptr, nullptr);
    if (count < 1)
    {
        return false;
    }
    const human_teams team = np->current_team;
    const char* action_type = np->get_ai()->get_action_node_type();
    for (int i = 0; i < count; i++)
    {
        const object_action_node* node = handles[i] != 0xFFFFFFFF ? World.object_from_handle<object_action_node>(handles[i]) : nullptr;
        if (node == nullptr || node->disabled_by != 0)
        {
            continue;
        }
        const auto& flags = node->base_node_info->node_flags;
        if (flags.linked_to_node || (flags.male_only && np->h_info->flags.female) || node->is_cooling_down() || !node->is_allowed_to_use(team))
        {
            continue;
        }
        const human* occupant = node->get_occupant();
        if (occupant != nullptr && occupant != np)
        {
            continue;
        }
        const bool matches = ped_only ? flags.allow_amb_peds : patrol_only ? flags.allow_patrols : node->base_node_info->action_type == action_type;
        if (matches)
        {
            node_list.add(node);
        }
    }
    return node_list.size() > 0;
}

// 0x00A60AD0 (:3154)
bool action_node_collect_nodes(const player* pp, float radius, farray<object_action_node const*,608>& node_list)
{
    node_list.clear();
    unsigned int handles[608];
    const int count = World.collect_objects<object_action_node>(pp->pos, radius, handles, 608, nullptr, nullptr);
    if (count < 1)
    {
        return false;
    }
    for (int i = 0; i < count; i++)
    {
        const object_action_node* node = handles[i] != 0xFFFFFFFF ? World.object_from_handle<object_action_node>(handles[i]) : nullptr;
        if (node != nullptr && node->disabled_by == 0 && !node->is_cooling_down() && node->base_node_info->node_flags.allow_player)
        {
            node_list.add(node);
        }
    }
    return node_list.size() > 0;
}

// 0x00A8D5A0 (:3186)
void action_node_disable_node(unsigned int node_handle, action_node_disabled_reason reason)
{
    object_action_node* node = node_handle != 0xFFFFFFFF ? World.object_from_handle<object_action_node>(node_handle) : nullptr;
    if (node != nullptr)
    {
        node->set_disabled_by(reason);
    }
}

// 0x00AA4700 (:3196)
bool action_node_reserve_node(unsigned int human_handle, const farray<unsigned int,4>& node_set)
{
    human* hp = human_handle != 0xFFFFFFFF ? World.object_from_handle<human>(human_handle) : nullptr;
    return hp != nullptr && hp->reserve_action_node(node_set);
}

// 0x00A8D5D0 (:3210)
bool action_node_reserve_node(human* hp, const farray<unsigned int,4>& node_set)
{
    return hp != nullptr && hp->reserve_action_node(node_set);
}

// 0x00A8D5F0 (:3219)
bool action_node_reserve_node(player* pp, const object_action_node* anp)
{
    if (pp == nullptr || anp == nullptr)
    {
        return false;
    }
    farray<unsigned int,4> node_set;
    const unsigned int node_handle = anp->get_handle();
    node_set.add(node_handle);
    return pp->reserve_action_node(node_set);
}

// 0x00A8D670: the root and, one per chain step, a random usable linked node of the next type.
static bool action_node_build_set_internal(const human* hp, const object_action_node* root_anp, farray<unsigned int,4>& node_set)
{
    if (root_anp == nullptr || node_set.size() >= node_set.capacity() || root_anp->disabled_by != 0 || root_anp->is_cooling_down())
    {
        return false;
    }
    const human* occupant = root_anp->get_occupant();
    if (occupant != nullptr && occupant != hp)
    {
        return false;
    }
    const unsigned int root_handle = root_anp->get_handle();
    node_set.add(root_handle);
    const int next_type = root_anp->get_next_action_node_type();
    if (next_type == -1)
    {
        return true;
    }
    farray<object_action_node const*,608> candidates;
    for (int i = 0; i < root_anp->node_links.size(); i++)
    {
        const object_action_node* linked = World.object_from_handle<object_action_node>(root_anp->node_links[i]);
        if (linked != nullptr && linked != root_anp && linked->get_action_node_type() == next_type)
        {
            candidates.add(linked);
        }
    }
    farray<unsigned int,4> attempt;
    while (candidates.size() > 0)
    {
        const int pick = candidates.size() == 1 ? 0 : int_rand_range(0, candidates.size() - 1);
        const object_action_node* next = candidates[pick];
        candidates.remove_unsorted(pick);
        attempt.copy(node_set);
        if (action_node_build_set_internal(hp, next, attempt))
        {
            node_set.copy(attempt);
            return true;
        }
    }
    return false;
}

// 0x00AA4730 (:3234)
bool action_node_build_set(unsigned int human_handle, unsigned int root_node_handle, farray<unsigned int,4>& node_set)
{
    const object_action_node* root_anp = root_node_handle != 0xFFFFFFFF ? World.object_from_handle<object_action_node>(root_node_handle) : nullptr;
    const human* hp = human_handle != 0xFFFFFFFF ? World.object_from_handle<human>(human_handle) : nullptr;
    return action_node_build_set(hp, root_anp, node_set);
}

// 0x00A8D870 (:3346)
bool action_node_build_set(const human* hp, const object_action_node* root_anp, farray<unsigned int,4>& node_set)
{
    if (root_anp == nullptr)
    {
        return false;
    }
    node_set.clear();
    return action_node_build_set_internal(hp, root_anp, node_set);
}

// 0x00A39240 (:3358)
bool action_node_has_entered_node(const human* hp, const object_action_node* anp)
{
    return anp->has_entered_node(hp);
}

// 0x00A157D0 (:3364)
bool action_node_is_entering(const human* hp)
{
    return hp->char_inst->is_playing_action(ANIM_ALIAS_TRANSITION_TO_ACTION_NODE);
}

// 0x00A157F0 (:3371)
bool action_node_is_exiting(const human* hp)
{
    return hp->char_inst->is_playing_action(ANIM_ALIAS_TRANSITION_FROM_ACTION_NODE);
}

// 0x00A8D8A0 (:3382)
void action_node_process_destruction_event(const rfg_destruction_event* eventp)
{
    const vector local_max = eventp->bmax + DESTRUCTION_EVENT_EXPAND_BMAX_OFFSET;
    const vector local_min = eventp->bmin + DESTRUCTION_EVENT_EXPAND_BMIN_OFFSET;
    vector world_min;
    vector world_max;
    bbox_instance(world_min, world_max, eventp->orient, eventp->pos, local_min, local_max);
    unsigned int handles[64];
    const int count = World.collect_objects<object_action_node>(world_min, world_max, handles, 64, nullptr, nullptr);
    for (int i = 0; i < count; i++)
    {
        object_action_node* node = handles[i] != 0xFFFFFFFF ? World.object_from_handle<object_action_node>(handles[i]) : nullptr;
        if (node != nullptr && node->disabled_by == 0 && node->base_node_info->node_flags.disable_by_destruction && ix_point_in_box_oriented(node->pos, eventp->pos, eventp->orient, local_min, local_max))
        {
            action_node_on_disable(node);
            node->disabled_by |= 1 << ANDR_DESTRUCTION;
        }
    }
}

// 0x00AA47A0 (:3421)
void enable_all_action_nodes_in_a_building(const rfg_mover* building)
{
    if (building == nullptr)
    {
        return;
    }
    object* child = building->child_ptr;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_ACTION_NODE)
        {
            ((object_action_node*)child)->clear_disabled_by(ANDR_BUILDING);
        }
        child = child->child_next;
        if (child == building->child_ptr)
        {
            break;
        }
    }
}

// 0x00A8DAA0 (:3439)
void disable_all_action_nodes_in_a_building(const rfg_mover* building)
{
    if (building == nullptr)
    {
        return;
    }
    object* child = building->child_ptr;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_ACTION_NODE)
        {
            object_action_node* node = (object_action_node*)child;
            action_node_on_disable(node);
            node->disabled_by |= 1 << ANDR_BUILDING;
        }
        child = child->child_next;
        if (child == building->child_ptr)
        {
            break;
        }
    }
}
