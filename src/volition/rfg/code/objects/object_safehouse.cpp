// Original: volition/rfg/code/objects/object_safehouse.cpp
//
// Safehouses: the class, its vehicle spawns, buildings and trigger regions, what enabling and disabling one does to the
// nodes inside, and the per-frame check that a player inside is ignored by enemies.

#include "volition/rfg/code/objects/object_safehouse.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/percept.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/event_logging/definitions_events/events.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/hud/fog_of_war.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/rfg_zonex_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_action_node.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_player_start.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/object_trigger_region.h"
#include "volition/rfg/code/objects/object_upgrade_node.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/scripting/objective_highlight.h"
#include "volition/rfg/code/spawn/ambient_spawn_manager.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/string_comp.h"

#include <float.h>

extern bool Player_died; // 0x0302F0A3 (player.cpp)

// ---- file statics
static float Safehouse_respawn_vehicle_dist_squared = 4000.0f; // 0x01694614
static unsigned int Safehouse_containing_player = 0xFFFFFFFF; // 0x01694678
static bool Safehouse_unfriendlies_ignore_player; // 0x0302F15A
static object_pool<object_safehouse, 64, 1> object_safehousepool; // 0x030FCE60

// 0x00A1C150 (:69): the high half of a handle's zone byte marks a managed layer object.
static bool object_is_from_managed_layer(const object* op)
{
    return ((op->get_handle() >> 16) & 0xFF) >= 0x80;
}

// 0x00A931C0 (:75)
// LTCG: the node in EDI. A live vehicle away from the node is only taken away when the player is far from it.
static bool object_safehouse_try_vehicle_spawn(object_vehicle_spawn_node* spawn_node)
{
    if (spawn_node->get_child_vehicle_handle() != 0xFFFFFFFF)
    {
        vehicle* vp = World.object_from_handle<vehicle>(spawn_node->get_child_vehicle_handle());
        if (vp != nullptr && !vp->is_killed())
        {
            bool player_far = true;
            if (Player != nullptr && vec_dist_squared(vp->pos, Player->pos) <= Safehouse_respawn_vehicle_dist_squared)
            {
                player_far = false;
            }
            if (vec_dist_squared(spawn_node->pos, vp->pos) <= Safehouse_respawn_vehicle_dist_squared || !player_far)
            {
                return false;
            }
            World.destroy_object(vp, false);
        }
    }
    // A node under a safehouse needs its place free.
    if (spawn_node->get_parent() != 0xFFFFFFFF && World.object_from_handle<object_safehouse>(spawn_node->get_parent()) != nullptr)
    {
        unsigned int blocker;
        // Magic number: 5 m.
        if (World.collect_objects<vehicle>(spawn_node->pos, 5.0f, &blocker, 1, nullptr, nullptr) > 0 || World.collect_objects<player>(spawn_node->pos, 5.0f, &blocker, 1, nullptr, nullptr) > 0)
        {
            return false;
        }
    }
    spawn_node->spawn_child_object(0xFFFFFFFF);
    return true;
}

// 0x00AAB580 (:123)
// Original: a guerrilla tech's squad node respawns even when it lies outside the safehouse's area.
// Deviation from rfg.exe: this and object_safehouse_handle_disable are external, not file-local, for the
// friend declarations.
void object_safehouse_handle_enable(object_safehouse* osp)
{
    vector bmin;
    vector bmax;
    osp->get_bbox(bmin, bmax);
    // Magic number: room for 256 objects of each kind.
    unsigned int handles[256];

    int count = World.collect_objects<object_vehicle_spawn_node>(bmin, bmax, handles, 256, nullptr, nullptr);
    for (int i = 0; i < count; i++)
    {
        object_vehicle_spawn_node* node = World.object_from_handle<object_vehicle_spawn_node>(handles[i]);
        if (node == nullptr || object_is_from_managed_layer(node))
        {
            continue;
        }
        // Not the vehicles of a delivery node.
        if (node->get_parent() != 0xFFFFFFFF)
        {
            object* parent = World.object_from_handle(node->get_parent());
            if (parent != nullptr && parent->get_obj_type() == OT_DELIVERY_NODE)
            {
                continue;
            }
        }
        if (osp->in_safehouse_area(node->pos))
        {
            node->node_flags.disabled = 0;
        }
    }

    count = World.collect_objects<object_squad_spawn_node>(bmin, bmax, handles, 256, nullptr, nullptr);
    for (int i = 0; i < count; i++)
    {
        object_squad_spawn_node* node = World.object_from_handle<object_squad_spawn_node>(handles[i]);
        if (node == nullptr || object_is_from_managed_layer(node))
        {
            continue;
        }
        if (osp->in_safehouse_area(node->pos))
        {
            node->set_mission_start_disabled(false);
        }
        const human_info* special = node->get_info()->special_npc_info;
        if (special != nullptr && special->flags.guerrilla_tech)
        {
            node->do_respawn(true);
        }
    }

    count = World.collect_objects<object_action_node>(bmin, bmax, handles, 256, nullptr, nullptr);
    for (int i = 0; i < count; i++)
    {
        object_action_node* node = World.object_from_handle<object_action_node>(handles[i]);
        if (node == nullptr || object_is_from_managed_layer(node) || !osp->in_safehouse_area(node->pos))
        {
            continue;
        }
        // Only starts its effect when the safehouse was the last reason it was disabled. Magic number: 0x10 is
        // 1 << ANDR_SAFE_HOUSE; set_disabled_by and clear_disabled_by do more than rfg.exe does here.
        if ((node->disabled_by & 0x10) != 0)
        {
            node->disabled_by &= ~0x10;
            if (node->disabled_by == 0)
            {
                node->start_hotspot_effect();
            }
        }
    }

    count = World.collect_objects<object_upgrade_node>(bmin, bmax, handles, 256, nullptr, nullptr);
    for (int i = 0; i < count; i++)
    {
        object_upgrade_node* node = World.object_from_handle<object_upgrade_node>(handles[i]);
        if (node == nullptr || object_is_from_managed_layer(node) || !osp->in_safehouse_area(node->pos))
        {
            continue;
        }
        node->set_enabled(true);
    }

    farray<navpoint*,512> nodes;
    bbox area;
    area.min = bmin;
    area.max = bmax;
    ped_node_get_network_nodes(area, nodes);
    for (int i = 0; i < nodes.size(); i++)
    {
        if (osp->in_safehouse_area(nodes[i]->pos))
        {
            nodes[i]->clear_disabled_by(NPDR_SAFEHOUSE);
        }
    }
}

// 0x00A933C0 (:221)
void object_safehouse_handle_disable(object_safehouse* osp)
{
    vector bmin;
    vector bmax;
    osp->get_bbox(bmin, bmax);
    unsigned int handles[256];

    int count = World.collect_objects<object_vehicle_spawn_node>(bmin, bmax, handles, 256, nullptr, nullptr);
    for (int i = 0; i < count; i++)
    {
        object_vehicle_spawn_node* node = World.object_from_handle<object_vehicle_spawn_node>(handles[i]);
        if (node == nullptr || object_is_from_managed_layer(node))
        {
            continue;
        }
        if (node->get_parent() != 0xFFFFFFFF)
        {
            object* parent = World.object_from_handle(node->get_parent());
            if (parent != nullptr && parent->get_obj_type() == OT_DELIVERY_NODE)
            {
                continue;
            }
        }
        if (osp->in_safehouse_area(node->pos))
        {
            node->node_flags.disabled = 1;
        }
    }

    count = World.collect_objects<object_squad_spawn_node>(bmin, bmax, handles, 256, nullptr, nullptr);
    for (int i = 0; i < count; i++)
    {
        object_squad_spawn_node* node = World.object_from_handle<object_squad_spawn_node>(handles[i]);
        if (node == nullptr || object_is_from_managed_layer(node) || !osp->in_safehouse_area(node->pos))
        {
            continue;
        }
        node->set_mission_start_disabled(true);
        if (node->get_child_squad_handle() == 0xFFFFFFFF)
        {
            continue;
        }
        object_squad* squad = World.object_from_handle<object_squad>(node->get_child_squad_handle());
        if (squad == nullptr)
        {
            continue;
        }
        // A squad with a guerrilla tech leaves.
        for (int m = 0; m < squad->member_array.size(); m++)
        {
            const unsigned int member = squad->member_array[m];
            npc* np = member != 0xFFFFFFFF ? World.object_from_handle<npc>(member) : nullptr;
            if (np != nullptr && np->h_info->flags.guerrilla_tech)
            {
                if (squad->base_orders_handle != 0xFFFFFFFF)
                {
                    squad->clear_orders(squad->base_orders_handle);
                }
                if (squad->assigned_orders_handle != 0xFFFFFFFF)
                {
                    squad->clear_orders(squad->assigned_orders_handle);
                }
                squad->request_despawn_all_members(NPC_DESPAWN_TYPE_HIGH, nullptr, false);
                break;
            }
        }
    }

    count = World.collect_objects<object_action_node>(bmin, bmax, handles, 256, nullptr, nullptr);
    for (int i = 0; i < count; i++)
    {
        object_action_node* node = World.object_from_handle<object_action_node>(handles[i]);
        if (node == nullptr || object_is_from_managed_layer(node) || !osp->in_safehouse_area(node->pos) || node->base_node_info->node_flags.not_disabled_with_safehouse)
        {
            continue;
        }
        if (node->disabled_by == 0)
        {
            human* occupant = node->get_occupant();
            if (occupant != nullptr && occupant->get_sub_type() != OT_SUB_HUMAN_PLAYER)
            {
                occupant->release_action_node(node->get_handle());
            }
            if (node->base_node_info->node_flags.ground_marker_highlight)
            {
                objective_highlight_ground_marker_remove(node->get_handle());
            }
            else
            {
                node->stop_hotspot_effect();
            }
        }
        node->disabled_by |= 0x10;
    }

    count = World.collect_objects<object_upgrade_node>(bmin, bmax, handles, 256, nullptr, nullptr);
    for (int i = 0; i < count; i++)
    {
        object_upgrade_node* node = World.object_from_handle<object_upgrade_node>(handles[i]);
        if (node == nullptr || object_is_from_managed_layer(node) || !osp->in_safehouse_area(node->pos))
        {
            continue;
        }
        node->set_enabled(false);
    }

    farray<navpoint*,512> nodes;
    bbox area;
    area.min = bmin;
    area.max = bmax;
    ped_node_get_network_nodes(area, nodes);
    for (int i = 0; i < nodes.size(); i++)
    {
        if (osp->in_safehouse_area(nodes[i]->pos))
        {
            nodes[i]->set_disabled_by(NPDR_SAFEHOUSE);
        }
    }
}

// 0x00AB44D0 (:330)
void unused_dcf_safehouse_enable()
{
    if (Player == nullptr)
    {
        return;
    }
    object_safehouse* osp = safehouse_find_nearest_safehouse(Player->pos, true, true);
    if (osp == nullptr)
    {
        return;
    }
    bool enable = !osp->is_enabled();
    if (console_get_arg(2, 1))
    {
        // Magic numbers: 0x40 and 0x80 are the true and false argument types of the console.
        if ((Console_arg_type & 0x40) != 0)
        {
            enable = true;
        }
        else if ((Console_arg_type & 0x80) != 0)
        {
            enable = false;
        }
    }
    for (int reason = SDR_NORMAL; reason < NUM_SAFEHOUSE_DISABLE_REASONS; reason++)
    {
        if (enable)
        {
            osp->clear_disabled((safehouse_disable_reason)reason);
        }
        else
        {
            osp->set_disabled((safehouse_disable_reason)reason);
        }
    }
    console_output(osp->is_enabled() ? "Safehouse is now enabled.\r\n" : "Safehouse is now disabled.\r\n", nullptr, false);
}

// 0x00A93860 (:364)
void unused_dcf_safehouse_set_visible()
{
    if (Player == nullptr)
    {
        return;
    }
    object_safehouse* osp = safehouse_find_nearest_safehouse(Player->pos, true, true);
    if (osp == nullptr)
    {
        return;
    }
    bool visible = !osp->is_visible();
    if (console_get_arg(2, 1))
    {
        if ((Console_arg_type & 0x40) != 0)
        {
            visible = true;
        }
        else if ((Console_arg_type & 0x80) != 0)
        {
            visible = false;
        }
    }
    osp->set_visible(visible);
    console_output(osp->is_visible() ? "Safehouse is now visible.\r\n" : "Safehouse is now not visible.\r\n", nullptr, false);
}

// ---- class registration (:396)
// 0x00A1C1A0
const char* classname_object_safehouse()
{
    return "object_safehouse";
}

// 0x00A6B480
object* alloc_object_safehouse()
{
    return object_safehousepool.alloc(0);
}

// 0x00A1C180
void types_object_safehouse(signed char& type, signed char& subtype)
{
    type = OT_SAFEHOUSE;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A1C190
int bpo_index_object_safehouse()
{
    return 1;
}

// Magic numbers: rfg.exe's arguments (0x0107BC20), the pool's bytes and capacity.
static obj_class_instantiator object_safehouse_instantiator("object_safehouse", alloc_object_safehouse, types_object_safehouse, bpo_index_object_safehouse, classname_object_safehouse, object_safehouse::pool_remaining, 0x4E80, 0x40); // 0x03500C94

// 0x00A3E280
int object_safehouse::pool_remaining()
{
    return object_safehousepool.get_free();
}

// 0x00A1C1B0
unsigned int object_safehouse::get_classname_hash()
{
    return object_safehouse_instantiator.this_hash;
}

// 0x00A93900
void object_safehouse::pool_free()
{
    object_safehousepool.free(this);
}

// 0x005DFB00 (.h:41)
const object_type object_safehouse::get_type_static()
{
    return OT_SAFEHOUSE;
}

// 0x005DFB10 (.h:41)
const object_sub_type object_safehouse::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A06A40 (.h:41)
const char* object_safehouse::get_classname_static()
{
    return "object_safehouse";
}

// 0x00A06A50 (.h:41)
int object_safehouse::get_bpo_index_static()
{
    return 1;
}

// 0x00A6B520 (.h:41)
const char* object_safehouse::get_classname() const
{
    return "object_safehouse";
}

// 0x00A6B490 (:397)
object_safehouse::object_safehouse()
{
}

// 0x00A6B530
object_safehouse::~object_safehouse()
{
}

// 0x00AAB900 (:409)
bool object_safehouse::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set(PROP_SAFEHOUSE_VISIBLE, visible);
        // Magic number: only the scripted reason is saved; the activity and until-stream ones do not last.
        pblock.set(PROP_SAFEHOUSE_DISABLED_FLAGS, disabled_flags & 1);
        pblock.set(PROP_SAFEHOUSE_DISPLAY_NAME_HASH, display_name_hash);
        return true;
    }
    vehicle_spawns.clear();
    buildings.clear();
    trigger_regions.clear();
    // rfg.exe inlines get_optional, which takes a property only with the expected size.
    visible = false;
    pblock.get_optional<bool>(PROP_SAFEHOUSE_VISIBLE, visible);
    disabled_flags = 0;
    bool enabled_value;
    if (pblock.get_optional<bool>(PROP_SAFEHOUSE_ENABLED, enabled_value))
    {
        if (!enabled_value)
        {
            set_disabled(SDR_NORMAL);
        }
    }
    else
    {
        pblock.get_optional<unsigned int>(PROP_SAFEHOUSE_DISABLED_FLAGS, disabled_flags);
    }
    run_enabled_func_on_stream = true;
    if (disabled_flags == 0)
    {
        Cartographer.add_icon(get_handle(), MAP_ELEMENT_SAFE_HOUSE, true, false, false);
    }
    const char* tag = pblock.get_str_ref_optional(PROP_SAFEHOUSE_DISPLAY_NAME_TAG);
    if (tag != nullptr)
    {
        display_name_hash = localization_get_id_hash(tag);
    }
    else if (!pblock.get_optional<unsigned int>(PROP_SAFEHOUSE_DISPLAY_NAME_HASH, display_name_hash))
    {
        display_name_hash = 0;
    }
    player_was_in_safehouse_last_update = false;
    in_safehouse_message_handle = INVALID_HUD_MESSAGE_HANDLE;
    return true;
}

// 0x00A1C1C0 (:472)
obj_serialize_mode object_safehouse::serialize_mode()
{
    return OSM_FULL;
}

// 0x00AB4570 (:484)
bool object_safehouse::stream(bool load)
{
    if (!load)
    {
        if (obj_flags.destroy_on_stream)
        {
            World.destroy_object(this, false);
        }
        vehicle_spawns.clear();
        trigger_regions.clear();
        buildings.clear();
        // Magic number: 4, the SDR_TEMP_DISABLE_UNTIL_STREAM bit.
        if ((disabled_flags & 4) != 0)
        {
            clear_disabled(SDR_TEMP_DISABLE_UNTIL_STREAM);
        }
        return true;
    }
    for (object* child = child_ptr; child != nullptr;)
    {
        const object_type type = child->get_obj_type();
        if (type == OT_MOVER)
        {
            if (child->get_sub_type() == OT_SUB_MOVER_RFG)
            {
                buildings.add(child->get_handle());
            }
        }
        else if (type == OT_TRIGGER_REGION)
        {
            trigger_regions.add(child->get_handle());
        }
        else if (type == OT_VEHICLE_SPAWN_NODE)
        {
            vehicle_spawns.add(child->get_handle());
        }
        child = child->child_next;
        if (child == child_ptr)
        {
            break;
        }
    }
    if (disabled_flags != 0)
    {
        object_safehouse_handle_disable(this);
    }
    else
    {
        object_safehouse_handle_enable(this);
    }
    run_enabled_func_on_stream = false;
    return true;
}

// 0x00A93950 (:554)
void object_safehouse::free_this()
{
    Cartographer.remove_icon(get_handle());
    object::free_this();
}

// 0x00A3E290 (:561)
// A half-metre box at the safehouse, grown over its trigger regions.
void object_safehouse::get_bbox(vector& bmin, vector& bmax) const
{
    bmin.set(pos.x - 0.5f, pos.y - 0.5f, pos.z - 0.5f);
    bmax.set(pos.x + 0.5f, pos.y + 0.5f, pos.z + 0.5f);
    for (int i = 0; i < trigger_regions.size(); i++)
    {
        const unsigned int region_handle = trigger_regions[i];
        trigger_region* region = region_handle != 0xFFFFFFFF ? World.object_from_handle<trigger_region>(region_handle) : nullptr;
        if (region != nullptr)
        {
            vector region_min;
            vector region_max;
            region->get_bbox(region_min, region_max);
            bbox_expand(bmin, bmax, region_min, region_max);
        }
    }
}

// 0x00A3E3C0 (:583)
void object_safehouse::get_local_bbox(vector& bmin, vector& bmax) const
{
    bmin.set(-0.5f, -0.5f, -0.5f);
    bmax.set(0.5f, 0.5f, 0.5f);
    for (int i = 0; i < trigger_regions.size(); i++)
    {
        const unsigned int region_handle = trigger_regions[i];
        trigger_region* region = region_handle != 0xFFFFFFFF ? World.object_from_handle<trigger_region>(region_handle) : nullptr;
        if (region != nullptr)
        {
            bbox world_box;
            region->get_bbox(world_box.min, world_box.max);
            bbox local_box;
            bbox_deinstance(local_box, orient, pos, world_box);
            bbox_expand(bmin, bmax, local_box.min, local_box.max);
        }
    }
}

// 0x00A6B550 (:607)
bool object_safehouse::player_is_in_safehouse() const
{
    return disabled_flags == 0 && Player != nullptr && in_safehouse_area(Player->pos);
}

// 0x00A3E490 (:627)
bool object_safehouse::in_safehouse_area(const vector& posp) const
{
    for (int i = 0; i < trigger_regions.size(); i++)
    {
        const unsigned int region_handle = trigger_regions[i];
        trigger_region* region = region_handle != 0xFFFFFFFF ? World.object_from_handle<trigger_region>(region_handle) : nullptr;
        if (region == nullptr)
        {
            continue;
        }
        bool inside = false;
        const trigger_region_shape shape = region->get_trigger_region_shape();
        if (shape == TRIGGER_REGION_SHAPE_BOX)
        {
            vector local_min;
            vector local_max;
            region->get_local_bbox(local_min, local_max);
            inside = ix_point_in_box_oriented(posp, region->pos, region->orient, local_min, local_max);
        }
        else if (shape == TRIGGER_REGION_SHAPE_SPHERE)
        {
            inside = ix_point_in_sphere(region->pos, region->get_radius(), posp);
        }
        if (inside)
        {
            return true;
        }
    }
    return false;
}

// 0x00A1C1D0 (:646)
void object_safehouse::set_visible(bool set_val)
{
    visible = set_val;
}

// 0x00A1C1E0 (:651): the DLC bundle 1 always shows them.
bool object_safehouse::is_visible() const
{
    return World.dlc_bundle_id == 1 || visible;
}

// 0x00A1C200 (:660)
bool object_safehouse::is_enabled_for_player_spawn() const
{
    return disabled_flags == 0 && (World.dlc_bundle_id == 1 || visible);
}

// 0x00A93970 (:673)
void object_safehouse::set_disabled(safehouse_disable_reason reason)
{
    const unsigned int previous = disabled_flags;
    disabled_flags = (1u << ((int)reason & 0x1F)) | previous;
    if (previous != 0)
    {
        return;
    }
    if (obj_flags.streamed)
    {
        object_safehouse_handle_disable(this);
    }
    else
    {
        run_enabled_func_on_stream = true;
    }
    Cartographer.remove_icon(get_handle());
}

// 0x00AABAD0 (:691)
void object_safehouse::clear_disabled(safehouse_disable_reason reason)
{
    disabled_flags &= ~(1u << ((int)reason & 0x1F));
    if (disabled_flags != 0)
    {
        return;
    }
    if (obj_flags.streamed)
    {
        object_safehouse_handle_enable(this);
        respawn_vehicles();
    }
    else
    {
        run_enabled_func_on_stream = true;
    }
    Cartographer.add_icon(get_handle(), MAP_ELEMENT_SAFE_HOUSE, true, false, false);
}

// 0x00A939E0 (:708)
// The first squad node of that squad definition name in the box. Original: it clears evaluated_spawn whether or not the
// node had a squad.
void object_safehouse::despawn_squad(const char* name)
{
    vector bmin;
    vector bmax;
    get_bbox(bmin, bmax);
    unsigned int handles[256];
    const int count = World.collect_objects<object_squad_spawn_node>(bmin, bmax, handles, 256, nullptr, nullptr);
    for (int i = 0; i < count; i++)
    {
        object_squad_spawn_node* node = World.object_from_handle<object_squad_spawn_node>(handles[i]);
        if (node == nullptr || object_is_from_managed_layer(node) || node->get_info() == nullptr || v_stricmp(node->get_info()->name, name) != 0)
        {
            continue;
        }
        if (node->get_child_squad_handle() != 0xFFFFFFFF)
        {
            object_squad* squad = World.object_from_handle<object_squad>(node->get_child_squad_handle());
            if (squad != nullptr)
            {
                squad->request_despawn_all_members(NPC_DESPAWN_TYPE_HIGH, nullptr, true);
            }
        }
        node->node_flags.evaluated_spawn = 0;
        return;
    }
}

// 0x00A93AF0 (:735)
void object_safehouse::respawn_vehicles()
{
    for (int i = 0; i < vehicle_spawns.size(); i++)
    {
        const unsigned int node_handle = vehicle_spawns[i];
        object_vehicle_spawn_node* node = node_handle != 0xFFFFFFFF ? World.object_from_handle<object_vehicle_spawn_node>(node_handle) : nullptr;
        if (node != nullptr)
        {
            object_safehouse_try_vehicle_spawn(node);
        }
    }
}

// 0x00A1C230 (:748)
void object_safehouse::update_safehouse_name_display(bool player_in_safehouse)
{
    if (!hud_ui_is_loaded())
    {
        return;
    }
    if (player_in_safehouse)
    {
        if (in_safehouse_message_handle == INVALID_HUD_MESSAGE_HANDLE)
        {
            // Magic number: shown for 5 s.
            in_safehouse_message_handle = hud_message_add_minor(localization_get_display_string_opt(display_name_hash), 5.0f, false);
            player_was_in_safehouse_last_update = player_in_safehouse;
            return;
        }
    }
    else if (player_was_in_safehouse_last_update)
    {
        // Magic number: 0.1 s, to take the message away.
        hud_message_change_minor_message(in_safehouse_message_handle, localization_get_display_string_opt(display_name_hash), 0.1f);
        in_safehouse_message_handle = INVALID_HUD_MESSAGE_HANDLE;
    }
    player_was_in_safehouse_last_update = player_in_safehouse;
}

// 0x00A6B570 (:833)
object_safehouse* safehouse_find_nearest_safehouse(const vector& pos, bool include_invisible, bool include_disabled)
{
    object_safehouse* best = nullptr;
    float best_dist = FLT_MAX;
    const int count = World.get_num_objects<object_safehouse>();
    for (int i = 0; i < count; i++)
    {
        object_safehouse* osp = World.get_object<object_safehouse>(i);
        if ((osp->is_visible() || include_invisible) && (osp->is_enabled() || include_disabled))
        {
            const float dx = pos.x - osp->pos.x;
            const float dz = pos.z - osp->pos.z;
            const float dist = dz * dz + dx * dx;
            if (dist < best_dist)
            {
                best_dist = dist;
                best = osp;
            }
        }
    }
    return best;
}

// 0x00A6B660 (:862)
// The nearest enabled, visible safehouse in the position's district, else the nearest anywhere. The DLC bundle 1 ignores
// visibility. Original: a safehouse only counts for its district when it is also the nearest so far overall.
object_safehouse* safehouse_find_spawn_safehouse(const vector& pos)
{
    float nearest = FLT_MAX;
    float nearest_in_district = FLT_MAX;
    object_safehouse* best = nullptr;
    object_safehouse* best_in_district = nullptr;
    const int count = World.get_num_objects<object_safehouse>();
    for (int i = 0; i < count; i++)
    {
        object_safehouse* osp = World.get_object<object_safehouse>(i);
        if (!osp->is_enabled() || (World.dlc_bundle_id != 1 && !osp->is_visible()))
        {
            continue;
        }
        const float dx = pos.x - osp->pos.x;
        const float dz = pos.z - osp->pos.z;
        const float dist = dz * dz + dx * dx;
        if (dist < nearest)
        {
            nearest = dist;
            best = osp;
            if (district_get_from_pos(pos) == district_get_from_pos(osp->pos) && dist < nearest_in_district)
            {
                nearest_in_district = dist;
                best_in_district = osp;
            }
        }
    }
    return best_in_district != nullptr ? best_in_district : best;
}

// 0x00A6B7C0 (:909)
// Original: keeps looking after a match, so the last safehouse in that cell wins.
object_safehouse* safehouse_find_safehouse_by_grid_id(const unsigned short gid)
{
    object_safehouse* found = nullptr;
    const int count = World.get_num_objects<object_safehouse>();
    for (int i = 0; i < count; i++)
    {
        object_safehouse* osp = World.get_object<object_safehouse>(i);
        int grid_x;
        int grid_y;
        if (rfg_zonex_get_grid_coords(osp->pos, grid_x, grid_y) && (unsigned short)(((grid_y & 0xF) | (grid_x << 4)) << 8) == gid)
        {
            found = osp;
        }
    }
    return found;
}

// 0x00A6B860 (:928)
// A player start among the safehouse's children, else one within 50 m. False (with the safehouse's own place) when there
// is none.
bool safehouse_get_start_pos_orient(vector& pos, matrix& orient, const object_safehouse* osp)
{
    if (osp == nullptr)
    {
        return false;
    }
    for (object* child = osp->child_ptr; child != nullptr;)
    {
        if (child->get_obj_type() == OT_PLAYER_START)
        {
            pos = child->pos;
            orient = child->orient;
            return true;
        }
        child = child->child_next;
        if (child == osp->child_ptr)
        {
            break;
        }
    }
    unsigned int handle = 0xFFFFFFFF;
    // Magic number: 50 m.
    World.collect_objects<player_start>(osp->pos, 50.0f, &handle, 1, nullptr, nullptr);
    player_start* start = World.object_from_handle<player_start>(handle);
    if (start != nullptr)
    {
        pos = start->pos;
        orient = start->orient;
        return true;
    }
    pos = osp->pos;
    orient = osp->orient;
    return false;
}

// 0x00A1C2E0 (:963)
bool safehouse_contains_player()
{
    return Safehouse_containing_player != 0xFFFFFFFF;
}

// 0x00A3E560 (:969)
object_safehouse* safehouse_get_safehouse_containing_player()
{
    if (Safehouse_containing_player == 0xFFFFFFFF)
    {
        return nullptr;
    }
    return World.object_from_handle<object_safehouse>(Safehouse_containing_player);
}

// 0x00A1C2F0 (:979)
bool safehouse_should_ignore_player()
{
    return Safehouse_unfriendlies_ignore_player;
}

// 0x00A93B40 (:985)
// Finds the safehouse holding the player. On the frame the player becomes ignored, enemies within 80 m despawn and every
// enemy forgets the player. Magic number below: 6400 = 80 m squared.
void safehouse_do_frame_postsim()
{
    const bool was_ignored = Safehouse_unfriendlies_ignore_player;
    const bool just_entered = Safehouse_containing_player == 0xFFFFFFFF;
    Safehouse_containing_player = 0xFFFFFFFF;
    object_safehouse* containing = nullptr;
    const int count = World.get_num_objects<object_safehouse>();
    for (int i = 0; i < count; i++)
    {
        object_safehouse* osp = World.get_object<object_safehouse>(i);
        if (osp->player_is_in_safehouse())
        {
            Safehouse_containing_player = osp->get_handle();
            containing = osp;
        }
        else
        {
            osp->update_safehouse_name_display(false);
        }
        if (World.dlc_bundle_id != 1 && !osp->is_visible() && !fog_of_war_is_posn_obscured(osp->pos))
        {
            osp->set_visible(true);
        }
    }
    if (containing != nullptr)
    {
        gfm_alert_levels cap_min;
        gfm_alert_levels cap_max;
        gsm_get_alert_level_cap(cap_min, cap_max);
        if (activity_is_active(ACTIVITY_MODE_HOUSE_ARREST))
        {
            gsm_release_alert_level_cap();
            cap_min = ALERT_LEVEL_GREEN;
        }
        if ((int)cap_min < (int)gsm_get_alert_level())
        {
            gsm_set_alert_level(cap_min);
        }
        if (just_entered)
        {
            containing->respawn_vehicles();
            const vector player_pos = gamelog_get_human_pos(Player);
            const int duration = gamelog_get_game_duration();
            const char difficulty = gamelog_get_difficulty();
            const int locale = gamelog_get_locale();
            const char pct_complete = gamelog_get_pct_complete();
            const int logging_time = gamelog_get_logging_time();
            const int checksum = gamelog_get_player_checksum();
            log_event_player_enter_safehouse(checksum, logging_time, pct_complete, locale, difficulty, duration, player_pos);
        }
        Safehouse_unfriendlies_ignore_player = (int)cap_min < 1;
        containing->update_safehouse_name_display(true);
        if (Player_died)
        {
            // Magic number: the tip waits 3 s.
            ui_tool_tips_add_tip("Safehouse_Respawn", 3000, nullptr);
        }
    }
    else
    {
        Safehouse_unfriendlies_ignore_player = false;
    }
    Player_died = false;
    if (was_ignored || !Safehouse_unfriendlies_ignore_player || Player == nullptr)
    {
        return;
    }
    player* pp = Player;
    const unsigned int player_handle = pp->get_handle();
    const int num_npcs = World.get_num_objects<npc>();
    for (int i = 0; i < num_npcs; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (!human_is_dead(np) && human_teams_are_unfriendly(np, pp) && vec_dist_squared(np->pos, containing->pos) <= 6400.0f && !np->npc_flags.converted_guerrilla)
        {
            np->npc_flags.despawn_npc = 1;
        }
    }
    const int num_squads = World.get_num_objects<object_squad>();
    for (int i = 0; i < num_squads; i++)
    {
        object_squad* squad = World.get_object<object_squad>(i);
        if (!human_teams_are_unfriendly(pp, squad->get_squad_team()))
        {
            continue;
        }
        const gfm_orders* orders = squad->get_squad_orders();
        const gfm_orders_types type = orders == nullptr ? GFM_ORDERS_TYPE_NONE : orders->get_orders_type();
        switch (type)
        {
        case GFM_ORDERS_TYPE_ATTACK:
            // A squad hunting the player leaves wherever it is.
            if (orders->get_target_object_handle() == player_handle)
            {
                squad->request_despawn_all_members(NPC_DESPAWN_TYPE_LOW, nullptr, true);
                break;
            }
            // fall through
        case GFM_ORDERS_TYPE_INVALID:
        case GFM_ORDERS_TYPE_NONE:
        case GFM_ORDERS_TYPE_PATROL:
        case GFM_ORDERS_TYPE_AMBIENT_ACTION:
        case GFM_ORDERS_TYPE_AMBIENT_PED:
        case GFM_ORDERS_TYPE_FODDER:
            for (int m = 0; m < squad->member_array.size(); m++)
            {
                npc* np = World.object_from_handle<npc>(squad->member_array[m]);
                if (!human_is_dead(np) && vec_dist_squared(np->pos, containing->pos) <= 6400.0f)
                {
                    squad->request_despawn_all_members(NPC_DESPAWN_TYPE_LOW, nullptr, true);
                    break;
                }
            }
            break;
        default:
            break;
        }
    }
    for (int i = 0; i < num_npcs; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (!human_is_dead(np) && human_teams_are_unfriendly(np, pp))
        {
            np->get_ai()->perception.deprecate_all_beliefs_about_object(player_handle);
        }
    }
}

// 0x00A1C300 (:1148)
bool safehouse_level_init_pre()
{
    Safehouse_containing_player = 0xFFFFFFFF;
    return true;
}
