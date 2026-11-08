// Original: volition/rfg/code/objects/object_squad.cpp
//
// Squads (NPC.md NP1): a group of npcs and their vehicle, spawned and directed together, and moved along their orders by
// the simulate_* family while nobody can see them. simulate_courier alone is left out, see its comment.

#include "volition/rfg/code/objects/object_squad.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/formation.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_courier_chase_internal.h"
#include "volition/rfg/code/gameplay/activities/vrail_system.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/gameflow/squad_progression.h"
#include "volition/rfg/code/gameplay/gameflow/squad_progression_internal.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/hud/objectives.h"
#include "volition/rfg/code/level/rfg_prop_name_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/object_action_node.h"
#include "volition/rfg/code/objects/object_convoy.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_patrol.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/rfg/code/spawn/spawn_request.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_flyer.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle/walker.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/traffic/traffic_shared.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/rfg/code/vehicle_ai/vehicle_navmesh_common.h"
#include "volition/rfg/code/vehicle_ai/vehicle_navmesh_path_create.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/parse/xml_table_tools.h"

#include <math.h>
#include <stdio.h>

extern gfm_orders_manager Orders_manager;
extern float VEHICLE_MAX_SPEED; // automobile.cpp

// Magic numbers: rfg.exe's pool holds 100 squads; a squad is given up on at more than 90 NPCs in the world; at most 32
// automobiles, 8 flyers and 4 walkers; an initial spawn lasts 8 s; a fodder squad waits 5 s after a member died.
static object_pool<object_squad,100,1> object_squadpool; // 0x03396AC0
static const int MAX_NPCS_FOR_SQUAD_SPAWN = 90;
static const int MAX_SQUAD_AUTOMOBILES = 32;
static const int MAX_SQUAD_FLYERS = 8;
static const int MAX_SQUAD_WALKERS = 4;
static const int INITIAL_SPAWN_MS = 8000;
static const int FODDER_RESPAWN_MS = 5000;
static const int SQUAD_NAME_POOL_SIZE = 0x300;
// The upper bits of a handle hold the zone; above 0x7F is an object a managed layer made.
static const unsigned int HANDLE_MANAGED_LAYER_BIT_ZONE = 0x7F;
static const float ORDERS_CLOSE_DISTANCE_SQRD = 25.0f; // 0x016AE0D4, 5 m
static const float MIN_PATROL_CLOSE_DIST_SQRD = 4.0f; // 0x016AE0D8, 2 m
static const float SIMULATED_ARRIVAL_DIST = 5.0f;
static const float SIMULATED_PATROL_ARRIVAL_DIST = 2.0f; // patrols and convoys
static const float FODDER_PASS_DIST = 25.0f;
static const float FODDER_PASS_BEYOND_DIST = 100.0f;
static const float FODDER_VEHICLE_SPEED = 30.0f;
static const int FODDER_PLACEMENT_TRIES = 10;
static const float FODDER_PLACEMENT_HALF_ARC = 0.7853982f; // 45 degrees either side of the player's facing
static const float FODDER_PLACEMENT_DIST = 225.0f;
static const float FODDER_ROAD_SEARCH_RADIUS = 200.0f;
static const float FODDER_FALLBACK_DIST = 100.0f;
static const float FODDER_FALLBACK_HEIGHT = 10.0f;
static const int VEHICLE_HUMAN_SLOTS = 8; // vi_instance::humans, rfg.exe walks all of them by a fixed stride of 0x68
static const float VEHICLE_DESPAWN_NEAR_DIST_SQ = 3600.0f; // 60 m: closer than this only an offscreen vehicle goes
static const int MAX_RECRUIT_CANDIDATES = 10;
static const float RECRUIT_RADIUS = 10.0f;
static const float MISSION_CLEAN_UP_SQUAD_DIST_SQ = 10000.0f; // 100 m
static const float MISSION_CLEAN_UP_NPC_DIST_SQ = 2500.0f; // 50 m

str_data_hash_table_cached<const squad_definition*,76,unsigned char> Squad_hash_table; // 0x030C0408
object_pool<squad_definition,76,1> Npc_squad_def_pool; // 0x031A2040
string_pool Squad_name_string_pool("Squad Def String Pool"); // 0x0328B670
static char* Squad_name_string_pool_data; // 0x0302F218
static bool Immediate_spawn_flag_squad; // 0x0302F21A

// 0x00A1D3A0 (:58)
report_parameters_class::report_parameters_class(float min_hostile_dist_sqrd, float min_high_threat, int max_report_ms)
    : minimum_distance_from_hostile_sqrd(min_hostile_dist_sqrd), high_priority_minimum(min_high_threat), maximum_report_period_ms(max_report_ms)
{
}

// 0x00A1D690 (:411)
// LTCG: orders in EAX, aip in ECX.
static void apply_custom_action(ai_system* aip, const gfm_orders* orders)
{
    if (orders->get_orders_type() == GFM_ORDERS_TYPE_TRAPPED)
    {
        if (aip->custom_behavior != AI_SB_TRAPPED_IN_BUILDING)
        {
            aip->trigger_custom_behavior(AI_SB_TRAPPED_IN_BUILDING, 0xFFFFFFFF, 0, 0, false);
        }
    }
    else if (orders->get_orders_type() == GFM_ORDERS_TYPE_TRAPPED_JAIL && aip->custom_behavior != AI_SB_TRAPPED_IN_JAIL)
    {
        aip->trigger_custom_behavior(AI_SB_TRAPPED_IN_JAIL, 0xFFFFFFFF, 0, 0, false);
    }
}

// 0x00A3EF30 (:231)
static const human_info* get_radio_operator_human_info()
{
    static const human_spawn_group_info* RADIO_OPERATOR_GROUP = sog_get_human_group("Radio Operator");
    for (int i = 0; i < RADIO_OPERATOR_GROUP->h_info_list.size(); i++)
    {
        const human_info* h_info = RADIO_OPERATOR_GROUP->h_info_list[i];
        if (srm_human_info_loaded(h_info))
        {
            return h_info;
        }
    }
    return nullptr;
}

// 0x00A1D3E0 (:254)
static const object* find_top_parent_object(const object* objp)
{
    if (objp == nullptr)
    {
        return nullptr;
    }
    for (;;)
    {
        const object* parent = World.object_from_handle(objp->get_parent());
        if (parent == nullptr)
        {
            return objp;
        }
        objp = parent;
    }
}

// 0x00A1D410 (:274)
static bool parents_streamed_in_squad(const object* objp)
{
    while (objp != nullptr)
    {
        if (!objp->obj_flags.streamed)
        {
            return false;
        }
        objp = World.object_from_handle(objp->get_parent());
        if (objp == nullptr)
        {
            return true;
        }
    }
    return false;
}

// 0x00A6E330 (:293): the nearest "path" nav point within a 5 m box.
static const navpoint* find_nearest_path_node(const vector& pos)
{
    static const vector SEARCH_AREA(5.0f, 5.0f, 5.0f);
    vector bmin(pos.x - SEARCH_AREA.x, pos.y - SEARCH_AREA.y, pos.z - SEARCH_AREA.z);
    vector bmax(pos.x + SEARCH_AREA.x, pos.y + SEARCH_AREA.y, pos.z + SEARCH_AREA.z);
    unsigned int handles[128];
    int count = World.collect_objects<navpoint>(bmin, bmax, handles, 128, nullptr, nullptr);
    const navpoint* nearest = nullptr;
    float nearest_dist = 3.4028235e+38f;
    for (int i = 0; i < count; i++)
    {
        const navpoint* navp = handles[i] != 0xFFFFFFFF ? World.object_from_handle<navpoint>(handles[i]) : nullptr;
        // Magic number: nav point type 10 is "path".
        if (navp != nullptr && navp->type == 10)
        {
            float dist = vec_dist_squared(pos, navp->pos);
            if (dist < nearest_dist)
            {
                nearest = navp;
                nearest_dist = dist;
            }
        }
    }
    return nearest;
}

// 0x00A1D440 (:327): in the target's box grown by half its size, or within 15 m of the target position.
static bool is_near_defend_location(const vector& pos, const gfm_orders* orders)
{
    const object* target = orders->get_target_object();
    if (target != nullptr)
    {
        vector bmin, bmax;
        target->get_bbox(bmin, bmax);
        vector half((bmax.x - bmin.x) * 0.5f, (bmax.y - bmin.y) * 0.5f, (bmax.z - bmin.z) * 0.5f);
        bmin.x -= half.x;
        bmin.y -= half.y;
        bmin.z -= half.z;
        bmax.x += half.x;
        bmax.y += half.y;
        bmax.z += half.z;
        return ix_point_in_box(pos, bmin, bmax);
    }
    vector location = orders->get_location();
    return vec_dist_squared(location, pos) <= 225.0f;
}

// 0x00A1D600 (:352): within 20 m.
static bool is_near_ambient_location(const vector& pos, const gfm_orders* orders)
{
    const object* target = orders->get_target_object();
    vector location = target != nullptr ? target->pos : orders->get_location();
    return vec_dist_squared(pos, location) < 400.0f;
}

// 0x00A6E520 (:373)
static const object* find_building_from_human(const human* hp)
{
    nav_cell* cell = bb_pf_get_nav_cell(hp->pos, false, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
    // Magic number: nav cell flag 8 marks a cell of a building's floor.
    if (cell != nullptr && (cell->flags & 8) == 0)
    {
        vector on_cell = nav_cell_force_point_to_cell(cell, hp->pos);
        if (fabsf(on_cell.y - hp->pos.y) < 1.0f)
        {
            return nullptr;
        }
    }
    const navpoint* navp = find_nearest_path_node(hp->pos);
    if (navp != nullptr && vec_dist_squared(hp->pos, navp->pos) <= 16.0f)
    {
        return World.object_from_handle(navp->get_parent());
    }
    return nullptr;
}

// 0x00A6E5E0 (:426)
static bool vehicle_pool_full(const vehicle_info* veh_info)
{
    switch (veh_info->vehicle_class)
    {
    case VCL_AUTOMOBILE:
        return World.get_num_objects<automobile>() >= MAX_SQUAD_AUTOMOBILES;
    case VCL_FLYER:
        return World.get_num_objects<flyer>() >= MAX_SQUAD_FLYERS;
    case VCL_WALKER:
        return World.get_num_objects<walker>() >= MAX_SQUAD_WALKERS;
    default:
        return false;
    }
}

// 0x00A3EFA0 (:453)
static bool parse_npc_squad_description(squad_definition* definition, xml_element* squad_element)
{
    definition->reset();
    xml_element* element = xml_find(squad_element, "Name");
    if (element == nullptr)
    {
        return false;
    }
    definition->name = Squad_name_string_pool.add_unique(element->text);
    if (definition->name == nullptr)
    {
        return false;
    }
    element = xml_find(squad_element, "team_name");
    if (element == nullptr)
    {
        return false;
    }
    definition->squad_team = human_team_from_name(element->text);
    element = xml_find(squad_element, "member_count");
    if (element == nullptr)
    {
        return false;
    }
    definition->total_npc_count = xml_get_int(element, 0);
    element = xml_find(squad_element, "special_npc");
    if (element != nullptr)
    {
        definition->special_npc_info = human_info_find_internal(element->text);
        if (definition->special_npc_info == nullptr)
        {
            return false;
        }
        definition->squad_team = definition->special_npc_info->default_team;
    }
    if (definition->total_npc_count < 0)
    {
        return false;
    }
    // Magic number: the capacity of member_array.
    if (definition->total_npc_count > 10)
    {
        definition->total_npc_count = 10;
    }
    return true;
}

// 0x00768D70 (.h:203)
object_squad_props::object_squad_props(const vector& pos, const matrix& orient, const char* squad_name, int index, unsigned int orders)
    : obj_prop_block(pos, orient, squad_name)
{
    set(SQUAD_PROP_DEFINITION_INDEX, index);
    set(SQUAD_PROP_ORDERS, orders);
}

// 0x0075F480
object_squad_props::~object_squad_props()
{
}

// 0x00A1D6D0 (:530)
human_teams squad_definition_get_team(const squad_definition* squad_def)
{
    return squad_def != nullptr ? squad_def->squad_team : HUMAN_TEAM_NONE;
}

// 0x00A1D6E0 (:540)
void squad_definition::reset()
{
    name = nullptr;
    special_npc_info = nullptr;
    squad_team = HUMAN_TEAM_NONE;
    total_npc_count = 0;
    primary_info = nullptr;
    secondary_info = nullptr;
    reinforcement_info = nullptr;
}

// 0x00A2D370 (.h:113)
squad_definition::squad_definition()
{
    reset();
}

// 0x00A06060 (.h:134)
int squad_definition::get_member_count() const
{
    return total_npc_count;
}

// 0x005FE650 (.h:140)
squad_flags_struct::squad_flags_struct()
{
    reset();
}

// 0x005DD5B0 (.h:188)
void squad_flags_struct::reset()
{
    *(unsigned int*)this = 0;
}

// 0x00A6E620 (:555)
vehicle* object_squad::spawn_vehicle(const object_spawn_region* osrp)
{
    vehicle_spawn_params params;
    params.reset();
    if (veh_spawn_group == nullptr)
    {
        return nullptr;
    }
    params.veh_info = squad_progression_member_vehicle_info(get_squad_team(), veh_spawn_group);
    if (params.veh_info == nullptr || vehicle_pool_full(params.veh_info))
    {
        return nullptr;
    }
    params.spawn_pos = pos;
    params.spawn_orient = orient;
    params.vflags = vflags;
    params.vflags.immediate_spawn = Immediate_spawn_flag_squad;
    params.veh_spawn_flags.infinite_mass = squad_flags.vehicle_infinite_mass;
    params.veh_spawn_flags.no_save = 1;
    params.osrp = osrp;
    params.spawn_priority = SPAWN_PRIORITY_MEDIUM;
    const gfm_orders* orders = get_squad_orders();
    gfm_orders_types orders_type = orders != nullptr ? orders->get_orders_type() : GFM_ORDERS_TYPE_INVALID;
    params.veh_spawn_flags.mobile_vehicle_spawn = 1;
    if (orders_type == GFM_ORDERS_TYPE_ROADBLOCK || orders_type == GFM_ORDERS_TYPE_WAIT)
    {
        params.veh_spawn_flags.road_spawn = 0;
        params.veh_spawn_flags.mobile_vehicle_spawn = 0;
        params.veh_spawn_flags.static_spawn_placement = 1;
    }
    else
    {
        params.veh_spawn_flags.road_spawn = !squad_flags.ignore_road_spawn && osrp == nullptr && orders_type != GFM_ORDERS_TYPE_FODDER && orders_type != GFM_ORDERS_TYPE_DEFEND;
    }
    params.veh_spawn_flags.air_vehicle = params.veh_info->vehicle_class == VCL_FLYER;
    if (params.veh_spawn_flags.air_vehicle)
    {
        params.veh_spawn_flags.road_spawn = 0;
        params.veh_spawn_flags.mobile_vehicle_spawn = 0;
    }
    params.forward_speed = 0.0f;
    switch (orders_type)
    {
    case GFM_ORDERS_TYPE_DEFEND:
    {
        params.veh_spawn_flags.force_to_ground = !params.veh_spawn_flags.one_attempt;
        object* node = World.object_from_handle(spawn_node_handle);
        // Magic number: within 5 m of its node a defending squad's vehicle starts on the node, parked.
        if (node != nullptr && vec_xz_dist_squared(node->pos, pos) <= 25.0f)
        {
            params.spawn_pos = node->pos;
            params.building_handle = node->get_parent();
            params.veh_spawn_flags.mobile_vehicle_spawn = 0;
        }
        if (hflags.activity_raid_required || hflags.activity_raid_optional)
        {
            params.veh_spawn_flags.road_spawn = 1;
            params.veh_spawn_flags.mobile_vehicle_spawn = 1;
        }
        break;
    }
    case GFM_ORDERS_TYPE_ATTACK:
    case GFM_ORDERS_TYPE_SMASH:
        params.forward_speed = 5.0f;
        break;
    case GFM_ORDERS_TYPE_PATROL:
    {
        object* target = get_squad_orders()->get_target_object();
        const object_patrol* patrol = target != nullptr && target->get_obj_type() == OT_PATROL ? (const object_patrol*)target : nullptr;
        if (!vrail_is_patrol(patrol))
        {
            params.forward_speed = 5.0f;
        }
        break;
    }
    case GFM_ORDERS_TYPE_CONVOY:
    {
        gfm_orders* convoy_orders = Orders_manager.get_orders_from_handle(get_orders_handle());
        object_convoy* convoy = convoy_orders != nullptr ? World.object_from_handle<object_convoy>(convoy_orders->get_target_object_handle()) : nullptr;
        if (convoy != nullptr)
        {
            params.forward_speed = convoy->get_move_speed();
            params.veh_spawn_flags.high_priority_clean_up = 1;
            params.veh_spawn_flags.convoy_vehicle_spawn = 1;
        }
        break;
    }
    case GFM_ORDERS_TYPE_COURIER:
        if (Orders_manager.get_orders_from_handle(get_orders_handle()) != nullptr && Courier_chase_data.get_courier_squad() != nullptr)
        {
            params.forward_speed = Courier_chase_data.get_move_speed();
        }
        break;
    case GFM_ORDERS_TYPE_FODDER:
        if (npc_flags.orders_active)
        {
            params.forward_speed = 30.0f;
        }
        break;
    case GFM_ORDERS_TYPE_WAIT:
    case GFM_ORDERS_TYPE_ROADBLOCK:
    {
        object* node = World.object_from_handle(spawn_node_handle);
        if (node == nullptr)
        {
            return nullptr;
        }
        params.spawn_pos = node->pos;
        params.spawn_orient = node->orient;
        params.veh_spawn_flags.mobile_vehicle_spawn = 0;
        params.veh_spawn_flags.static_spawn_placement = 1;
        break;
    }
    case GFM_ORDERS_TYPE_DRIVE_BY:
        params.forward_speed = 9.0f;
        params.veh_spawn_flags.static_spawn_placement = 0;
        params.veh_spawn_flags.road_spawn = 1;
        params.veh_spawn_flags.mobile_vehicle_spawn = 1;
        break;
    default:
        break;
    }
    if (object_spawn_vehicle(params) != SPAWN_STATUS_SUCCESS)
    {
        return nullptr;
    }
    return params.vp;
}

// 0x00A1D700 (:734)
gfm_orders* object_squad::get_orders() const
{
    gfm_orders* orders = assigned_orders_handle != 0xFFFFFFFF ? Orders_manager.get_orders_from_handle(assigned_orders_handle) : nullptr;
    if (orders == nullptr && base_orders_handle != 0xFFFFFFFF)
    {
        orders = Orders_manager.get_orders_from_handle(base_orders_handle);
    }
    return orders;
}

// 0x00A1D750 (:758)
unsigned int object_squad::get_orders_handle() const
{
    const gfm_orders* orders = get_orders();
    return orders != nullptr ? orders->get_orders_handle() : 0xFFFFFFFF;
}

// 0x00A1D7A0 (:770)
unsigned int object_squad::get_base_orders_handle() const
{
    return base_orders_handle;
}

// 0x00A3F070 (:776)
void object_squad::set_team_override(human_teams new_team)
{
    override_team = new_team;
    for (int i = 0; i < member_array.size(); i++)
    {
        npc* np = World.object_from_handle<npc>(member_array[i]);
        human_teams team = override_team != HUMAN_TEAM_NONE ? override_team : np->h_info->default_team;
        if (team != np->current_team)
        {
            np->set_team(team);
        }
    }
}

// 0x00A1D7B0 (:789)
int object_squad::get_max_member_count() const
{
    return definition != nullptr ? definition->total_npc_count : 0;
}

// 0x00A1D7C0 (:797)
human_teams object_squad::get_squad_team() const
{
    if (override_team != HUMAN_TEAM_NONE)
    {
        return override_team;
    }
    return definition != nullptr ? definition->squad_team : HUMAN_TEAM_NONE;
}

// 0x00A6EA60 (:807)
void object_squad::set_no_save()
{
    obj_flags.no_save = 1;
    vehicle* vp = get_vehicle();
    if (vp != nullptr)
    {
        vp->obj_flags.no_save = 1;
    }
    for (int i = 0; i < member_array.size(); i++)
    {
        npc* np = World.object_from_handle<npc>(member_array[i]);
        if (np != nullptr)
        {
            np->obj_flags.no_save = 1;
        }
    }
}

// 0x00A3F100 (:833)
void object_squad::set_high_priority(bool is_high_priority)
{
    squad_flags.high_priority_targets = is_high_priority;
    vehicle* vp = get_vehicle();
    if (vp != nullptr)
    {
        vp->m_flags.high_priority_target = is_high_priority;
    }
}

// 0x00A3F170 (:843)
const object_patrol* object_squad::get_patrol() const
{
    const gfm_orders* orders = get_squad_orders();
    if (orders != nullptr && orders->get_orders_type() == GFM_ORDERS_TYPE_PATROL)
    {
        object* target = orders->get_target_object();
        if (target != nullptr && target->get_obj_type() == OT_PATROL)
        {
            return (const object_patrol*)target;
        }
    }
    return nullptr;
}

// 0x00A3F1A0 (:865): 10 m.
bool object_squad::followers_near_leader() const
{
    npc* leader = squad_leader != 0xFFFFFFFF ? World.object_from_handle<npc>(squad_leader) : nullptr;
    if (leader == nullptr || leader->is_killed())
    {
        return false;
    }
    for (int i = 0; i < member_array.size(); i++)
    {
        unsigned int member_handle = member_array[i];
        if (member_handle == squad_leader || member_handle == 0xFFFFFFFF)
        {
            continue;
        }
        npc* np = World.object_from_handle<npc>(member_handle);
        if (np != nullptr && !np->is_killed() && np->movement_mode == HMM_PATHFINDING && np->get_ai()->is_following_leader(leader) && vec_dist_squared(np->pos, np->dest_point) > 100.0f)
        {
            return false;
        }
    }
    return true;
}

// 0x00A6EAE0 (:898)
bool object_squad::followers_in_vehicle() const
{
    npc* leader = squad_leader != 0xFFFFFFFF ? World.object_from_handle<npc>(squad_leader) : nullptr;
    if (leader == nullptr || leader->is_killed())
    {
        return false;
    }
    for (int i = 0; i < member_array.size(); i++)
    {
        unsigned int member_handle = member_array[i];
        if (member_handle == squad_leader || member_handle == 0xFFFFFFFF)
        {
            continue;
        }
        npc* np = World.object_from_handle<npc>(member_handle);
        if (np != nullptr)
        {
            if (!np->is_killed() && human_in_vehicle(np) == nullptr)
            {
                return false;
            }
            if (human_in_vehicle(np) != nullptr && np->vehicle_handle != leader->vehicle_handle)
            {
                return false;
            }
        }
    }
    return true;
}

// 0x00A1D7E0 (:929)
gfm_orders* object_squad::get_squad_orders() const
{
    return get_orders();
}

// 0x00A1D840 (:936)
void object_squad::prevent_reassignment()
{
    squad_flags.disallow_reassignment = 1;
}

// 0x00A3F2A0 (:946)
bool object_squad::can_be_reassigned() const
{
    if (squad_flags.disallow_reassignment || squad_flags.managed_layer_creation || is_killed())
    {
        return false;
    }
    const gfm_orders* orders = get_squad_orders();
    if (orders == nullptr)
    {
        return true;
    }
    if (orders->get_orders_type() == GFM_ORDERS_TYPE_PATROL)
    {
        const object_patrol* patrol = get_patrol();
        if (patrol != nullptr && patrol->is_override_patrol())
        {
            return false;
        }
    }
    return orders->get_orders_type() != GFM_ORDERS_TYPE_ATTACK;
}

// 0x00A3F310 (:982)
bool object_squad::in_squad_vehicle() const
{
    if (squad_vehicle == 0xFFFFFFFF)
    {
        return false;
    }
    for (int i = 0; i < member_array.size(); i++)
    {
        npc* np = member_array[i] != 0xFFFFFFFF ? World.object_from_handle<npc>(member_array[i]) : nullptr;
        if (np != nullptr && np->vehicle_handle == squad_vehicle)
        {
            return true;
        }
    }
    return false;
}

// 0x00A3F380 (:1007)
bool object_squad::in_combat() const
{
    if (squad_flags.simulation)
    {
        return false;
    }
    for (int i = 0; i < member_array.size(); i++)
    {
        npc* np = member_array[i] != 0xFFFFFFFF ? World.object_from_handle<npc>(member_array[i]) : nullptr;
        if (np != nullptr && !np->is_killed() && np->get_ai()->has_hostile_contact())
        {
            return true;
        }
    }
    return false;
}

// 0x00A6EBD0 (:1033)
player* object_squad::get_player() const
{
    if (!squad_flags.player_squad)
    {
        return nullptr;
    }
    for (int i = 0; i < World.get_num_objects<player>(); i++)
    {
        player* pp = World.get_object<player>(i);
        if (pp->ally_squad_handle == get_handle() || pp->escort_squad_handle == get_handle())
        {
            return pp;
        }
    }
    // Original: Int3 critical function here.
    return nullptr;
}

// 0x00A946E0 (:1058)
void object_squad::report_member_death(const npc* np)
{
    if (squad_flags.simulation || np == nullptr)
    {
        return;
    }
    if (np == get_leader())
    {
        squad_flags.leader_has_died = 1;
    }
    remove_member(np->get_handle());
    last_member_removed.set(0);
    squad_member_killed_event killed;
    killed.squad_handle = get_handle();
    gem_post_event(killed);
    if (definition->total_npc_count < 1)
    {
        player* pp = get_player();
        if (pp != nullptr)
        {
            if (get_handle() == pp->ally_squad_handle)
            {
                pp->ally_squad_report_member_death(np);
            }
            else if (get_handle() == pp->escort_squad_handle)
            {
                pp->escort_squad_report_member_death(np);
            }
        }
    }
    else if (member_array.size() < 1)
    {
        const gfm_orders* orders = get_squad_orders();
        if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_FODDER)
        {
            squad_killed_event squad_killed;
            squad_killed.squad_handle = get_handle();
            gem_post_event(squad_killed);
            World.destroy_object(this, false);
            return;
        }
        squad_flags.simulation = 1;
        squad_vehicle = 0xFFFFFFFF;
    }
}

// 0x00A1D850 (:1124)
gfm_orders_types object_squad::get_squad_orders_type() const
{
    const gfm_orders* orders = get_squad_orders();
    return orders != nullptr ? orders->get_orders_type() : GFM_ORDERS_TYPE_INVALID;
}

// 0x00A3F400 (:1134)
vehicle* object_squad::get_vehicle() const
{
    return squad_vehicle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(squad_vehicle) : nullptr;
}

// 0x00A3F430 (:1147)
void object_squad::remove_vehicle(bool force_out, bool no_respawn)
{
    vehicle* vp = get_vehicle();
    if (vp != nullptr && force_out)
    {
        vp->m_flags.disable_for_npc_drivers = 1;
        human* driver = vehicle_passenger_in_or_entering_seat(vp, VSI_DRIVER);
        if (driver != nullptr && driver->get_obj_type() == OT_HUMAN && driver->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            ((npc*)driver)->get_ai()->planner.request_replan();
        }
    }
    for (int i = 0; i < member_array.size(); i++)
    {
        npc* np = member_array[i] != 0xFFFFFFFF ? World.object_from_handle<npc>(member_array[i]) : nullptr;
        if (np != nullptr)
        {
            np->hflags.disallow_vehicle_exit = 0;
            np->hflags.limited_vehicle_exit = 0;
        }
    }
    squad_flags.stay_in_vehicle = 0;
    squad_vehicle = 0xFFFFFFFF;
    if (no_respawn)
    {
        veh_spawn_group = nullptr;
        squad_flags.vehicle_damage_update = 0;
    }
}

// 0x00A3F520 (:1186)
void object_squad::assign_vehicle(vehicle* vp)
{
    if (vp != nullptr && squad_vehicle == 0xFFFFFFFF)
    {
        squad_vehicle = vp->get_handle();
        vp->m_flags.disable_for_npc_drivers = 0;
        for (int i = 0; i < member_array.size(); i++)
        {
            npc* np = World.object_from_handle<npc>(member_array[i]);
            if (!human_is_dead(np))
            {
                np->get_ai()->planner.request_replan();
            }
        }
    }
}

// 0x00A3F5A0 (:1202)
bool object_squad::squad_assets_ready() const
{
    if (!obj_flags.streamed)
    {
        return false;
    }
    if (veh_spawn_group == nullptr)
    {
        return true;
    }
    for (int i = 0; i < veh_spawn_group->v_info_list.size(); i++)
    {
        if (srm_vehicle_info_loaded(veh_spawn_group->v_info_list[i]))
        {
            return true;
        }
    }
    return false;
}

// 0x00A3F5F0 (:1230)
object_convoy* object_squad::get_convoy() const
{
    const gfm_orders* orders = Orders_manager.get_orders_from_handle(get_orders_handle());
    return orders != nullptr ? World.object_from_handle<object_convoy>(orders->get_target_object_handle()) : nullptr;
}

// 0x00A1D870 (:1244)
activity_courier_chase_data* object_squad::get_courier_data() const
{
    // Original: any squad with orders answers the one courier activity, courier or not.
    return get_squad_orders() != nullptr ? &Courier_chase_data : nullptr;
}

// 0x00A3F660 (:1257)
void object_squad::remove_icons_from_members(bool remove_from_squad)
{
    for (int i = 0; i < member_array.size(); i++)
    {
        npc* np = member_array[i] != 0xFFFFFFFF ? World.object_from_handle<npc>(member_array[i]) : nullptr;
        if (np != nullptr && !np->is_killed())
        {
            Cartographer.remove_icon(np->get_handle());
        }
    }
    if (!remove_from_squad)
    {
        Cartographer.add_icon(get_handle(), squad_map_icon, squad_flags.squad_icon_is_sticky, squad_flags.squad_icon_is_closest, false);
    }
}

// 0x00A3F700 (:1281)
void object_squad::assign_icons_to_members(minimap_elements spawn_icon, map_icon_types map_icon_type)
{
    if (squad_map_icon != MAP_ELEMENT_INVALID)
    {
        remove_icons_from_members(true);
        squad_map_icon = MAP_ELEMENT_INVALID;
    }
    squad_map_icon = spawn_icon;
    squad_flags.squad_icon_is_sticky = map_icon_type == MAP_ICON_TYPE_STICKY || map_icon_type == MAP_ICON_TYPE_CLOSEST_STICKY;
    squad_flags.squad_icon_is_closest = map_icon_type == MAP_ICON_TYPE_CLOSEST_STICKY;
    int assigned = 0;
    for (int i = 0; i < member_array.size(); i++)
    {
        npc* np = member_array[i] != 0xFFFFFFFF ? World.object_from_handle<npc>(member_array[i]) : nullptr;
        if (np != nullptr && !np->is_killed())
        {
            Cartographer.add_icon(np->get_handle(), squad_map_icon, squad_flags.squad_icon_is_sticky, squad_flags.squad_icon_is_closest, false);
            assigned++;
        }
    }
    if (assigned == 0)
    {
        Cartographer.add_icon(get_handle(), squad_map_icon, squad_flags.squad_icon_is_sticky, squad_flags.squad_icon_is_closest, false);
    }
}

// 0x00A3F830 (:1329)
void object_squad::remove_objective_icons_from_members(bool remove_from_squad)
{
    for (int i = 0; i < member_array.size(); i++)
    {
        npc* np = member_array[i] != 0xFFFFFFFF ? World.object_from_handle<npc>(member_array[i]) : nullptr;
        if (np != nullptr && !np->is_killed())
        {
            remove_objective_icon(squad_map_objective, np->get_handle());
        }
    }
    if (!remove_from_squad)
    {
        add_objective_icon(squad_map_objective, get_handle(), squad_map_icon, false);
    }
}

// 0x00A1D8D0 (:1353)
bool object_squad::simulated() const
{
    return squad_flags.simulation;
}

// 0x00AB47B0 (:1478)
void object_squad::simulate()
{
    if (!squad_flags.simulation)
    {
        return;
    }
    simulated_velocity = ZERO_VECTOR;
    gfm_orders* orders = Orders_manager.get_orders_from_handle(get_orders_handle());
    if (orders == nullptr || squad_flags.just_spawned || npc_flags.activity_driver_vr)
    {
        return;
    }
    if (squad_flags.flier_squad)
    {
        vehicle* vp = get_vehicle();
        if (vp != nullptr && vp->rdd.is_loaded() && !should_attempt_despawn())
        {
            return;
        }
    }
    else if (should_attempt_spawn())
    {
        return;
    }
    if (definition->total_npc_count > 0)
    {
        switch (orders->get_orders_type())
        {
        case GFM_ORDERS_TYPE_DEFEND:
            simulate_defend(orders);
            return;
        case GFM_ORDERS_TYPE_ATTACK:
            simulate_attack(orders);
            return;
        case GFM_ORDERS_TYPE_PATROL:
        case GFM_ORDERS_TYPE_COURIER:
        {
            object* target = orders->get_target_object();
            simulate_patrol(target != nullptr && target->get_obj_type() == OT_PATROL ? (const object_patrol*)target : nullptr);
            return;
        }
        case GFM_ORDERS_TYPE_CONVOY:
            simulate_convoy(orders);
            return;
        case GFM_ORDERS_TYPE_FODDER:
            simulate_fodder(orders);
            return;
        case GFM_ORDERS_TYPE_WAIT:
        case GFM_ORDERS_TYPE_ROADBLOCK:
        case GFM_ORDERS_TYPE_TRAPPED:
        case GFM_ORDERS_TYPE_TRAPPED_JAIL:
            simulate_go_to_spawn_node();
            return;
        case GFM_ORDERS_TYPE_DRIVE_BY:
            simulate_pathfind_to(orders);
            return;
        default:
            break;
        }
    }
    simulate_default(orders);
}

// 0x00A3F8C0 (:1360): a squad with no living member carries the icon itself.
void object_squad::assign_objective_icons_to_members(int objective, minimap_elements spawn_icon)
{
    if (objective == -1)
    {
        return;
    }
    if (squad_map_objective != -1)
    {
        remove_objective_icons_from_members(true);
        squad_map_objective = -1;
        squad_map_icon = MAP_ELEMENT_INVALID;
    }
    if (squad_map_icon != MAP_ELEMENT_INVALID)
    {
        remove_icons_from_members(true);
        squad_map_icon = MAP_ELEMENT_INVALID;
    }
    squad_map_objective = objective;
    squad_map_icon = spawn_icon;
    squad_flags.squad_icon_is_sticky = 1;
    squad_flags.squad_icon_is_closest = 0;
    int num_icons = 0;
    for (int i = 0; i < member_array.size(); i++)
    {
        const npc* np = World.object_from_handle<npc>(member_array[i]);
        if (np != nullptr && !np->is_killed())
        {
            add_objective_icon(squad_map_objective, np->get_handle(), squad_map_icon, false);
            num_icons++;
        }
    }
    if (num_icons == 0)
    {
        add_objective_icon(squad_map_objective, get_handle(), squad_map_icon, false);
    }
}

// 0x00A3F9C0 (:1415)
float object_squad::get_default_simulated_move_speed(float dist, bool ramp_speed, float vehicle_speed) const
{
    float speed = 3.0f; // rfg.exe's default, only left standing by an Int3 no input reaches
    bool have_speed = false;
    const gfm_orders* orders = get_squad_orders();
    const gfm_orders_types orders_type = orders != nullptr ? orders->get_orders_type() : GFM_ORDERS_TYPE_INVALID;
    if (orders_type == GFM_ORDERS_TYPE_CONVOY)
    {
        const object_convoy* convoy = get_convoy();
        if (convoy != nullptr)
        {
            speed = convoy->get_move_speed();
            have_speed = true;
        }
    }
    else if (orders_type == GFM_ORDERS_TYPE_COURIER)
    {
        const activity_courier_chase_data* courier = get_courier_data();
        if (courier != nullptr)
        {
            speed = courier->get_move_speed();
            have_speed = true;
        }
    }
    if (!have_speed)
    {
        if (veh_spawn_group == nullptr)
        {
            speed = Default_move_speed_block[orders_type == GFM_ORDERS_TYPE_PATROL ? HMS_WALK : HMS_RUN].average_speed;
        }
        else
        {
            speed = vehicle_speed != 0.0f ? vehicle_speed : VEHICLE_MAX_SPEED;
        }
    }
    if (ramp_speed)
    {
        // Up to half as fast again when 10 m or more behind.
        speed *= fl_parameter(dist, 2.0f, 10.0f, 1.0f, 1.5f, true);
    }
    return speed;
}

// 0x00A6EC50 (:1572)
void object_squad::simulate_default(const gfm_orders* orders)
{
    const vector dest = orders->get_orders_pos();
    if (dest.is_valid())
    {
        simulate_move_to(dest, SIMULATED_ARRIVAL_DIST, false, 0.0f);
    }
}

// 0x00AACF00 (:1585): a defending squad near its node snaps onto it, except for raid squads.
void object_squad::simulate_defend(const gfm_orders* orders)
{
    if (!hflags.activity_raid_required && !hflags.activity_raid_optional)
    {
        const object* node = World.object_from_handle(spawn_node_handle);
        if (node != nullptr && vec_dist_squared(pos, node->pos) <= ORDERS_CLOSE_DISTANCE_SQRD)
        {
            pos = node->pos;
            orient = node->orient;
            return;
        }
    }
    simulate_pathfind_to(orders);
}

// 0x00AACFB0 (:1615)
void object_squad::simulate_attack(const gfm_orders* orders)
{
    if (veh_spawn_group != nullptr)
    {
        simulate_pathfind_to(orders);
        return;
    }
    const vector dest = orders->get_orders_pos();
    if (dest.is_valid())
    {
        simulate_move_to(dest, SIMULATED_ARRIVAL_DIST, false, 0.0f);
    }
}

// 0x00A6ECC0 (:1629): a fodder squad drives at its target object, or waits on a road ahead of the player.
void object_squad::simulate_fodder(const gfm_orders* orders)
{
    if (!npc_flags.orders_active)
    {
        return;
    }
    const player* pp = Player;
    if (pp == nullptr || npc_flags.fodder_on_object)
    {
        const object* target = orders->get_target_object();
        if (target != nullptr)
        {
            vector to_target = target->pos - pos;
            if (to_target.mag_and_normalize_safe() < FODDER_PASS_DIST)
            {
                // Close: keep going past it.
                const vector beyond = target->pos + orient.fvec * FODDER_PASS_BEYOND_DIST;
                simulate_move_to(beyond, SIMULATED_ARRIVAL_DIST, true, FODDER_VEHICLE_SPEED);
                return;
            }
            simulate_move_to(target->pos, SIMULATED_ARRIVAL_DIST, true, FODDER_VEHICLE_SPEED);
        }
        return;
    }

    vector player_fvec = pp->orient.fvec;
    const object* player_vehicle = pp->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(pp->vehicle_handle) : nullptr;
    if (player_vehicle != nullptr)
    {
        player_fvec = player_vehicle->orient.fvec;
    }
    int tries = FODDER_PLACEMENT_TRIES;
    float facing;
    do
    {
        if (tries < 1)
        {
            break;
        }
        vector direction = player_fvec;
        direction.rotate_around_y_axis(fl_rand_range(-FODDER_PLACEMENT_HALF_ARC, FODDER_PLACEMENT_HALF_ARC));
        pos = pp->pos + direction * FODDER_PLACEMENT_DIST;
        vector road_point;
        float u_value;
        if (object_road_get_closest_road(pos, true, road_point, u_value, FODDER_ROAD_SEARCH_RADIUS, false, nullptr, nullptr, false) != nullptr)
        {
            pos = road_point;
        }
        vector to_player = pp->pos - pos;
        to_player.normalize_safe();
        orient.make_quick(to_player);
        tries--;
        facing = to_player.y * player_fvec.y + to_player.x * player_fvec.x + to_player.z * player_fvec.z;
    } while (facing >= 0.0f); // until the squad is ahead of the player
    if (tries == 0)
    {
        pos = pp->pos + player_fvec * FODDER_FALLBACK_DIST + UP_VECTOR * FODDER_FALLBACK_HEIGHT;
    }
}

// 0x00A6F270 (:1710)
void object_squad::simulate_go_to_spawn_node()
{
    const object_squad_spawn_node* node = spawn_node_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad_spawn_node>(spawn_node_handle) : nullptr;
    if (node != nullptr)
    {
        simulate_move_to(node->pos, SIMULATED_ARRIVAL_DIST, false, 0.0f);
    }
}

// 0x00A94800 (:1726): follows a road path to the orders' position, dropping nodes as it reaches them.
void object_squad::simulate_pathfind_to(const gfm_orders* orders)
{
    if (dest_spline_node == nullptr)
    {
        vehicle_path_info path_info;
        pf_calc_flags pf_flags(true);
        pf_flags.vehicle_pfg = 1;
        const vector dest = orders->get_orders_pos();
        if (vec_dist_squared(pos, dest) < ORDERS_CLOSE_DISTANCE_SQRD)
        {
            simulate_move_to(dest, SIMULATED_ARRIVAL_DIST, false, 0.0f);
            return;
        }
        const pf_calc_point points[2] = {pf_calc_point(pos, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF), pf_calc_point(orders->get_orders_pos(), 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF)};
        const pf_calc_params real_params(2, points, pf_flags, true, false, false, false, 0xFFFFFFFF);
        bool try_normal_pathfinding = true;
        dest_spline_node = rfg_pf_vehicle_use_road_splines(real_params, path_info, try_normal_pathfinding, nullptr);
        if (dest_spline_node == nullptr)
        {
            simulate_default(orders);
            return;
        }
    }
    vector dest;
    while (dest_spline_node != nullptr)
    {
        if (get_vehicle_destination(dest) && vec_dist_squared(pos, dest) >= ORDERS_CLOSE_DISTANCE_SQRD)
        {
            break;
        }
        vehicle_free_ls_node(&dest_spline_node, dest_spline_node);
    }
    if (dest_spline_node == nullptr)
    {
        const vector orders_pos = orders->get_orders_pos();
        if (orders_pos.is_valid())
        {
            simulate_move_to(orders_pos, SIMULATED_ARRIVAL_DIST, false, 0.0f);
        }
        return;
    }
    if (get_vehicle_destination(dest))
    {
        simulate_move_to(dest, SIMULATED_ARRIVAL_DIST, false, 0.0f);
        return;
    }
    simulate_default(orders);
}

// 0x00A6F2C0 (:1793): the second knot of the next node.
bool object_squad::get_vehicle_destination(vector& dest_pos) const
{
    if (dest_spline_node == nullptr || dest_spline_node->lane.num_knots <= 1)
    {
        return false;
    }
    dest_pos = dest_spline_node->lane.knots.to_pointer()[1];
    return true;
}

// 0x00A6F300 (:1822)
void object_squad::simulate_patrol(const object_patrol* patrol)
{
    if (patrol == nullptr || vrail_is_patrol(patrol) || patrol->is_paused())
    {
        return;
    }
    if (patrol->get_handle() != current_node_handle.patrol_handle)
    {
        current_node_handle = patrol->get_next_patrol_point_handle(pos);
    }
    if (current_node_handle.nav_handle == 0xFFFFFFFF || current_node_handle.patrol_handle == 0xFFFFFFFF)
    {
        current_node_handle = patrol->get_next_patrol_point_handle(pos);
    }
    vector node_pos;
    if (!patrol->get_node_pos(current_node_handle, node_pos))
    {
        return;
    }
    const float dist_sq = squad_flags.flier_squad ? vec_xz_dist_squared(pos, node_pos) : vec_dist_squared(pos, node_pos);
    if (dist_sq <= MIN_PATROL_CLOSE_DIST_SQRD)
    {
        patrol_squad_reached_node_event event;
        event.squad_handle = get_handle();
        event.patrol_node = &current_node_handle;
        gem_post_event(event);
        current_node_handle = patrol->get_next_patrol_point_handle(current_node_handle, pos);
        if (!patrol->get_node_pos(current_node_handle, node_pos))
        {
            return;
        }
    }
    const navpoint* nav = current_node_handle.nav_handle != 0xFFFFFFFF ? World.object_from_handle<navpoint>(current_node_handle.nav_handle) : nullptr;
    if (nav != nullptr)
    {
        simulate_move_to(node_pos, SIMULATED_PATROL_ARRIVAL_DIST, false, nav->speed_limit);
    }
}

// 0x00A94AC0 (:1885)
void object_squad::simulate_convoy(const gfm_orders* orders)
{
    const object* target = orders->get_target_object();
    if (target == nullptr || target->get_obj_type() != OT_CONVOY)
    {
        return;
    }
    object_convoy* convoy = (object_convoy*)target;
    const convoy_squad_roles role = convoy->get_squad_role(get_handle());
    if (role == CONVOY_SQUAD_ROLE_UNDEFINED)
    {
        return;
    }
    vector dest;
    if (convoy->get_lead_squad_handle() == get_handle())
    {
        if (convoy->get_ghost_destination(pos, dest))
        {
            simulate_move_to(dest, SIMULATED_PATROL_ARRIVAL_DIST, false, 0.0f);
        }
    }
    else if (role == CONVOY_SQUAD_ROLE_ON_FOOT_ESCORT)
    {
        const object_squad* follow = convoy->get_on_foot_follow_squad(get_handle());
        if (follow != nullptr)
        {
            simulate_move_to(follow->pos, SIMULATED_PATROL_ARRIVAL_DIST, true, 0.0f);
        }
    }
    else
    {
        simulate_convoy_follower(convoy);
    }
}

// 0x00A6F490 (:1926). Original: no caller in rfg.exe (simulate sends courier orders to simulate_patrol).
void object_squad::simulate_courier(const gfm_orders* orders)
{
    vector dest;
    if (Courier_chase_data.is_active() && Courier_chase_data.get_courier_squad_handle() == get_handle() &&Courier_chase_data.get_ghost_destination(pos, dest))
    {
        simulate_move_to(dest, 2.0f, false, 0.0f); // Magic number: rfg.exe's arguments
    }
}

// 0x00A6F500 (:1955)
void object_squad::simulate_convoy_follower(object_convoy* convoy)
{
    unsigned int formation = convoy->get_main_formation_handle();
    if (formation == 0xFFFFFFFF || !ai_formation_is_following(this, formation))
    {
        formation = convoy->join_convoy_formation(this);
        if (formation == 0xFFFFFFFF)
        {
            return;
        }
    }
    const follow_information* info = ai_formation_get_follower_position(formation, this);
    if (info != nullptr && info->follow_mode != AI_FFM_INVALID)
    {
        simulate_move_to(info->formation_pos, SIMULATED_PATROL_ARRIVAL_DIST, true, 0.0f);
    }
}

// 0x00A3FB50 (:1993): answers true on arrival. A flying squad ignores height.
bool object_squad::simulate_move_to(const vector& dest_pos, float min_dist, bool ramp_speed, float vehicle_speed)
{
    if (!dest_pos.is_valid())
    {
        return false;
    }
    vector direction = dest_pos - pos;
    if (squad_flags.flier_squad)
    {
        direction.y = 0.0f;
    }
    const float dist = direction.mag_and_normalize_safe();
    if (dist <= min_dist)
    {
        return true;
    }
    const float speed = get_default_simulated_move_speed(dist, ramp_speed, vehicle_speed);
    const float step = Frametime * speed;
    vector new_pos;
    bool arrived;
    if (step < dist)
    {
        new_pos = pos + direction * step;
        arrived = false;
        simulated_velocity = direction * speed;
    }
    else
    {
        new_pos = dest_pos;
        arrived = true;
        if (squad_flags.flier_squad)
        {
            new_pos.y = pos.y;
        }
        simulated_velocity = direction * (dist / Frametime);
    }
    if (new_pos.is_valid() && simulated_velocity.is_valid())
    {
        if (simulated_velocity.is_null())
        {
            update_pos_and_orient(new_pos, orient, false);
        }
        else
        {
            matrix new_orient;
            new_orient.make_quick(simulated_velocity.copy_flatten_normalize_safe());
            update_pos_and_orient(new_pos, new_orient, false);
        }
    }
    return arrived;
}

// 0x00A6F680 (:2494): gives up the vehicle, and destroys it unless someone outside the squad is using it or the player can
// see it. Its damage is kept for the next one.
void object_squad::despawn_vehicle()
{
    vehicle* vp = squad_vehicle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(squad_vehicle) : nullptr;
    squad_vehicle = 0xFFFFFFFF;
    if (vp == nullptr || vp->m_flags.do_not_destroy_with_squad)
    {
        return;
    }
    const player* pp = Player;
    if (pp->last_vehicle_driven == vp->get_handle() || pp->vehicle_handle == vp->get_handle())
    {
        return;
    }
    for (int i = 0; i < VEHICLE_HUMAN_SLOTS; i++)
    {
        const unsigned int passenger = vp->vii.humans[i].handle;
        const object* objp = passenger != 0xFFFFFFFF ? World.object_from_handle(passenger) : nullptr;
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN)
        {
            continue;
        }
        if (member_array.find(objp->get_handle(), false) > -1 || objp->obj_flags.destroyed)
        {
            continue;
        }
        if (!objp->is_killed())
        {
            return;
        }
    }
    if (vec_dist_squared(pp->pos, vp->pos) >= VEHICLE_DESPAWN_NEAR_DIST_SQ || vp->is_offscreen())
    {
        stored_damage_areas[0] = vp->m_damage_areas[0];
        squad_flags.vehicle_damage_update = 1;
        World.destroy_object(vp, false);
        squad_vehicle = 0xFFFFFFFF;
    }
}

// 0x00A94BB0 (:2069)
void object_squad::free_this()
{
    if (base_orders_handle != 0xFFFFFFFF)
    {
        clear_orders(base_orders_handle);
    }
    if (assigned_orders_handle != 0xFFFFFFFF)
    {
        clear_orders(assigned_orders_handle);
    }
    release_vehicle_path();
    object::free_this();
}

// 0x00A3FE70 (:2082)
bool object_squad::is_inital_spawn() const
{
    return member_array.size() <= 0 && squad_age.time_since() < INITIAL_SPAWN_MS;
}

// 0x00A1D8F0 (:2097)
void types_object_squad(signed char& type, signed char& subtype)
{
    type = OT_SQUAD;
    subtype = -1;
}

// 0x00A1D900 (:2097)
int bpo_index_object_squad()
{
    return 1;
}

// 0x00A1D910 (:2097)
const char* classname_object_squad()
{
    return "object_squad";
}

// 0x00A6F580 (:2097)
object* alloc_object_squad()
{
    return object_squadpool.alloc(0);
}

static obj_class_instantiator object_squad_instantiator("object_squad", alloc_object_squad, types_object_squad, bpo_index_object_squad, classname_object_squad, object_squad::pool_remaining, sizeof(object_squadpool), 100); // 0x0303CADC

// 0x00A1D920 (:2097)
unsigned int object_squad::get_classname_hash()
{
    return object_squad_instantiator.this_hash;
}

// 0x00A3FEB0 (:2097)
int object_squad::pool_remaining()
{
    return object_squadpool.get_free();
}

// 0x00A94C10 (:2097)
void object_squad::pool_free()
{
    object_squadpool.free(this);
}

// 0x00A6F590 (:2098)
object_squad::object_squad()
{
}

// 0x00A6F630
object_squad::~object_squad()
{
}

// 0x005DD5C0 (.h:216)
const object_type object_squad::get_type_static()
{
    return OT_SQUAD;
}

// 0x005DD5D0 (.h:216)
const object_sub_type object_squad::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A06070 (.h:216)
const char* object_squad::get_classname_static()
{
    return "object_squad";
}

// 0x00A06080 (.h:216)
int object_squad::get_bpo_index_static()
{
    return 1;
}

// 0x00A6F620 (.h:216)
const char* object_squad::get_classname() const
{
    return "object_squad";
}

// 0x00A1D930 (:2104)
void object_squad::get_bbox(vector& bmin, vector& bmax) const
{
    get_local_bbox(bmin, bmax);
    bmin.x += pos.x;
    bmin.y += pos.y;
    bmin.z += pos.z;
    bmax.x += pos.x;
    bmax.y += pos.y;
    bmax.z += pos.z;
}

// 0x00A1DA00 (:2111)
void object_squad::get_local_bbox(vector& bmin, vector& bmax) const
{
    bmin.x = -0.5f;
    bmin.y = -0.5f;
    bmin.z = -0.5f;
    bmax.x = 0.5f;
    bmax.y = 0.5f;
    bmax.z = 0.5f;
}

// 0x00A3FEC0 (:2118): an unoccupied squad vehicle out of sight or more than 120 m away goes with the squad; never nearer
// than 20 m or the player's last vehicle.
void object_squad::destroy_notify_destroy_vehicle()
{
    vehicle* vp = get_vehicle();
    if (vp == nullptr || vp->obj_flags.destroyed || vp->m_flags.do_not_destroy_with_squad || vp->m_flags.convoy_vehicle || Player->last_vehicle_driven == vp->get_handle())
    {
        return;
    }
    float dist = vec_dist_squared(Player->pos, vp->pos);
    if (dist < 400.0f)
    {
        return;
    }
    // Magic number: eight seats.
    for (int i = 0; i < 8; i++)
    {
        unsigned int human_handle = vp->vii.humans[i].handle;
        human* hp = human_handle != 0xFFFFFFFF ? World.object_from_handle<human>(human_handle) : nullptr;
        if (hp != nullptr)
        {
            if (hp->get_sub_type() != OT_SUB_HUMAN_NPC)
            {
                return;
            }
            if (!hp->obj_flags.destroyed && !hp->is_killed())
            {
                return;
            }
        }
    }
    if (dist > 14400.0f || vp->is_offscreen())
    {
        World.destroy_object(vp, false);
    }
}

// 0x00A3FFD0 (:2185)
void object_squad::destroy_notify()
{
    remove_constraints();
    if (hflags.activity_raid_required || hflags.activity_raid_optional)
    {
        activity_squad_destroy_notify(this);
    }
    destroy_notify_destroy_vehicle();
}

// 0x00A40000 (:2216)
bool object_squad::is_killed() const
{
    if (squad_flags.simulation)
    {
        return false;
    }
    for (int i = 0; i < member_array.size(); i++)
    {
        npc* np = member_array[i] != 0xFFFFFFFF ? World.object_from_handle<npc>(member_array[i]) : nullptr;
        if (np != nullptr && !np->is_killed())
        {
            return false;
        }
    }
    return true;
}

// 0x00A1DA40 (:2274)
bool object_squad::should_spawn_with_vehicle() const
{
    return veh_spawn_group != nullptr;
}

// 0x00A40080 (:2279): the simulation and creation flags stay this squad's.
void object_squad::assign_new_squad_flags(const squad_flags_struct& new_squad_flags)
{
    unsigned int keep = *(const unsigned int*)&squad_flags;
    unsigned int value = *(const unsigned int*)&new_squad_flags;
    // Magic number: bits 0, 20, 21 and 22.
    *(unsigned int*)&squad_flags = ((value ^ keep) & 0x700001) ^ value;
}

// 0x00AAD030 (:2295)
bool object_squad::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        return true;
    }
    obj_flags.no_save = 1;
    base_orders_handle = 0xFFFFFFFF;
    assigned_orders_handle = 0xFFFFFFFF;
    definition = nullptr;
    spawn_status = SQUAD_SPAWN_OK;
    veh_spawn_group = nullptr;
    dest_spline_node = nullptr;
    squad_leader = 0xFFFFFFFF;
    spawn_node_handle = 0xFFFFFFFF;
    squad_vehicle = 0xFFFFFFFF;
    simulated_velocity = ZERO_VECTOR;
    vehicle_hp_mult = 1.0f;
    squad_map_objective = -1;
    squad_map_icon = MAP_ELEMENT_INVALID;
    override_team = HUMAN_TEAM_NONE;
    memset(&npc_flags, 0, sizeof(npc_flags));
    memset(&hflags, 0, sizeof(hflags));
    memset(&vflags, 0, sizeof(vflags));
    member_array.clear();
    current_node_handle.reset();
    squad_age.set(0);
    squad_flags.reset();
    pblock.get_buf_copy_optional(SQUAD_PROP_SQUAD_FLAGS, (unsigned char*)&squad_flags, sizeof(squad_flags));
    squad_flags.simulation = 1;
    squad_flags.just_spawned = 1;
    pblock.get_optional(SQUAD_PROP_SPAWN_NODE_HANDLE, spawn_node_handle);
    squad_flags.dynamic_creation = spawn_node_handle == 0xFFFFFFFF;
    unsigned int spawn_node_zone = (spawn_node_handle >> 16) & 0xFF;
    if (!squad_flags.dynamic_creation)
    {
        squad_flags.managed_layer_creation = spawn_node_zone > HANDLE_MANAGED_LAYER_BIT_ZONE;
    }
    if (mission_is_active())
    {
        squad_flags.mission_created = squad_flags.managed_layer_creation || squad_flags.dynamic_creation || spawn_node_zone == 0xFF;
    }
    pblock.get_optional(SQUAD_PROP_ORDERS, base_orders_handle);
    if (base_orders_handle != 0xFFFFFFFF && Orders_manager.get_orders_from_handle(base_orders_handle) == nullptr)
    {
        base_orders_handle = 0xFFFFFFFF;
    }
    pblock.get_buf_copy_optional(SQUAD_PROP_NPC_FLAGS, (unsigned char*)&npc_flags, sizeof(npc_flags));
    pblock.get_buf_copy_optional(SQUAD_PROP_HUMAN_FLAGS, (unsigned char*)&hflags, sizeof(hflags));
    pblock.get_buf_copy_optional(SQUAD_PROP_VEHICLE_FLAGS, (unsigned char*)&vflags, sizeof(vflags));
    pblock.get_optional(SQUAD_PROP_VEHICLE_HP_MULT, vehicle_hp_mult);
    pblock.get_optional(SQUAD_PROP_SPAWN_STATUS, spawn_status);
    unsigned int definition_index = 0xFFFFFFFF;
    pblock.get_optional(SQUAD_PROP_DEFINITION_INDEX, definition_index);
    definition = squad_definition_from_index((int)definition_index);
    if (definition == nullptr)
    {
        return false;
    }
    bool has_vehicle = false;
    if (pblock.get_optional(SQUAD_PROP_SQUAD_VEHICLE, has_vehicle))
    {
        char vehicle_type[256];
        if (has_vehicle && pblock.get_optional(SQUAD_PROP_VEHICLE_TYPE, vehicle_type))
        {
            veh_spawn_group = sog_get_vehicle_group(vehicle_type);
        }
    }
    else
    {
        pblock.get_optional(SQUAD_PROP_VEHICLE_REF, veh_spawn_group);
    }
    pblock.get_optional(SQUAD_PROP_MAP_ICON, squad_map_icon);
    if (get_parent() != 0xFFFFFFFF)
    {
        remove_from_parent();
    }
    last_member_removed.invalidate();
    squad_flags.flier_squad = veh_spawn_group != nullptr && veh_spawn_group->v_info_list.size() > 0 && veh_spawn_group->v_info_list[0]->vehicle_class == VCL_FLYER;
    return true;
}

// 0x00A1DA50 (:2488)
obj_serialize_mode object_squad::serialize_mode()
{
    return OSM_NONE;
}

// 0x00A6F800 (:2561)
bool object_squad::despawn_members()
{
    for (int i = 0; i < member_array.size(); i++)
    {
        npc* np = member_array[i] != 0xFFFFFFFF ? World.object_from_handle<npc>(member_array[i]) : nullptr;
        if (np == nullptr)
        {
            continue;
        }
        if (squad_map_icon != MAP_ELEMENT_INVALID)
        {
            if (squad_map_objective == -1)
            {
                Cartographer.remove_icon(np->get_handle());
            }
            else if (Cartographer.does_icon_exist(np->get_handle()) || Cartographer.does_sticky_icon_exist(np->get_handle()))
            {
                remove_objective_icon(squad_map_objective, np->get_handle());
            }
        }
        np->get_ai()->clear_squad();
        World.destroy_object(np, false);
    }
    despawn_vehicle();
    squad_flags.simulation = 1;
    member_array.clear();
    if (Orders_manager.get_orders_from_handle(get_orders_handle()) == nullptr)
    {
        World.destroy_object(this, false);
    }
    else if (squad_map_icon != MAP_ELEMENT_INVALID)
    {
        if (squad_map_objective == -1)
        {
            Cartographer.add_icon(get_handle(), squad_map_icon, squad_flags.squad_icon_is_sticky, squad_flags.squad_icon_is_closest, false);
        }
        else
        {
            add_objective_icon(squad_map_objective, get_handle(), squad_map_icon, false);
        }
    }
    squad_members_despawned despawned;
    despawned.squad_handle = get_handle();
    gem_post_event(despawned);
    return true;
}

// 0x00A94C20 (:2632)
bool object_squad::spawn_members(bool force_spawn, const object_spawn_region* osrp)
{
    if (World.get_num_objects<npc>() > MAX_NPCS_FOR_SQUAD_SPAWN)
    {
        return false;
    }
    if (force_spawn)
    {
        squad_flags.force_spawn = 1;
    }
    squad_flags.just_spawned = 0;
    // Magic number: squads stand upright.
    if (orient.uvec.y * UP_VECTOR.y + orient.uvec.x * UP_VECTOR.x + orient.uvec.z * UP_VECTOR.z < 0.9999f)
    {
        orient.make_uvec(orient.fvec, UP_VECTOR);
    }
    if (veh_spawn_group != nullptr)
    {
        object_squad_spawn_node* node = spawn_node_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad_spawn_node>(spawn_node_handle) : nullptr;
        if (get_vehicle() == nullptr)
        {
            vehicle* vp = spawn_vehicle(osrp);
            if (vp == nullptr)
            {
                return false;
            }
            squad_vehicle = vp->get_handle();
            vp->squad_handle = get_handle();
            vp->m_flags.high_priority_target = squad_flags.high_priority_targets;
            if (vehicle_hp_mult != 1.0f)
            {
                // Magic number: rounds as rfg.exe's x87 store does.
                vehicle_set_max_hp((int)roundf((float)vehicle_get_max_hit_points(vp) * vehicle_hp_mult), squad_vehicle);
            }
            vp->m_flags.activity = squad_flags.activity;
            if (node != nullptr && get_handle() == node->get_child_squad_handle())
            {
                node->update_vehicle_handle(vp->get_handle());
            }
            if (squad_flags.vehicle_damage_update)
            {
                vp->m_damage_areas[0].m_type = stored_damage_areas[0].m_type;
                vp->m_damage_areas[0].m_max_hit_points = stored_damage_areas[0].m_max_hit_points;
                vp->m_damage_areas[0].m_hit_points = stored_damage_areas[0].m_hit_points;
                vp->m_damage_areas[0].m_effect_instances[0] = 0xFFFFFFFF;
                vp->m_damage_areas[0].m_effect_instances[1] = 0xFFFFFFFF;
            }
        }
    }
    return spawn_squad_members(osrp);
}

// 0x00A400A0 (:2716)
vehicle* object_squad::should_spawn_in_vehicle(npc_spawn_param_list& spawn_param_list)
{
    const gfm_orders* orders = Orders_manager.get_orders_from_handle(get_orders_handle());
    if (orders == nullptr || squad_flags.stay_in_vehicle)
    {
        return get_vehicle();
    }
    if (orders->get_orders_type() == GFM_ORDERS_TIED_UP)
    {
        return nullptr;
    }
    if (orders->get_orders_type() == GFM_ORDERS_TYPE_DEFEND && is_near_defend_location(pos, orders))
    {
        return nullptr;
    }
    if (orders->get_orders_type() == GFM_ORDERS_TYPE_AMBIENT_ACTION && is_near_ambient_location(pos, orders))
    {
        return nullptr;
    }
    return get_vehicle();
}

// 0x00A6F9C0 (:2754)
const object* object_squad::should_spawn_in_object_defend(npc_spawn_param_list& spawn_param_list, const gfm_orders* orders_object, const object* objp)
{
    // Magic number: 10 m around the defended object.
    static const vector NEAR_OBJECT_OFFSET(10.0f, 10.0f, 10.0f);
    if (objp == nullptr)
    {
        return nullptr;
    }
    if (objp->get_obj_type() == OT_HUMAN)
    {
        return find_building_from_human((const human*)objp);
    }
    vector bmin, bmax;
    objp->get_local_bbox(bmin, bmax);
    bmin.x -= NEAR_OBJECT_OFFSET.x;
    bmin.y -= NEAR_OBJECT_OFFSET.y;
    bmin.z -= NEAR_OBJECT_OFFSET.z;
    bmax.x += NEAR_OBJECT_OFFSET.x;
    bmax.y += NEAR_OBJECT_OFFSET.y;
    bmax.z += NEAR_OBJECT_OFFSET.z;
    vector local = pos;
    vec_change_to_local_coords(local, objp->pos, objp->orient);
    if (ix_point_in_box(local, bmin, bmax))
    {
        spawn_param_list.spawn_flags.spawn_in_guard = true;
        return find_top_parent_object(objp);
    }
    return nullptr;
}

// 0x00A40170 (:2794): within a building's box grown by its size, at least 20 m.
const object* object_squad::should_spawn_in_object_patrol(npc_spawn_param_list& spawn_param_list, const gfm_orders* orders_object, const object* objp)
{
    if (objp == nullptr || !current_node_handle.is_valid())
    {
        return nullptr;
    }
    const object* target = orders_object->get_target_object();
    if (target == nullptr || target->get_obj_type() != OT_PATROL)
    {
        return nullptr;
    }
    const rfg_mover* building = current_node_handle.is_inside_building();
    if (building == nullptr)
    {
        return nullptr;
    }
    vector bmin, bmax;
    building->get_bbox(bmin, bmax);
    float grow_x = bmax.x - bmin.x;
    float grow_y = bmax.y - bmin.y;
    float grow_z = bmax.z - bmin.z;
    grow_x = grow_x < 20.0f ? 20.0f : grow_x;
    grow_y = grow_y < 20.0f ? 20.0f : grow_y;
    grow_z = grow_z < 20.0f ? 20.0f : grow_z;
    bmin.x -= grow_x;
    bmin.y -= grow_y;
    bmin.z -= grow_z;
    bmax.x += grow_x;
    bmax.y += grow_y;
    bmax.z += grow_z;
    return ix_point_in_box(pos, bmin, bmax) ? (const object*)building : nullptr;
}

// Inlined in should_spawn_in_object_tied_up and should_spawn_in_object_wait: placed exactly on the spawn node.
static const object* spawn_at_node(npc_spawn_param_list& spawn_param_list, unsigned int spawn_node_handle)
{
    object_squad_spawn_node* node = spawn_node_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad_spawn_node>(spawn_node_handle) : nullptr;
    if (node == nullptr)
    {
        return nullptr;
    }
    spawn_param_list.spawn_pos = node->pos;
    spawn_param_list.spawn_orient = node->orient;
    spawn_param_list.spawn_flags.spawn_in_cover = false;
    spawn_param_list.spawn_flags.static_spawn_placement = true;
    spawn_param_list.spawn_flags.spawn_in_action_node = false;
    spawn_param_list.spawn_flags.spawn_on_node = false;
    spawn_param_list.spawn_flags.spawn_in_guard = false;
    return find_top_parent_object(World.object_from_handle(node->get_parent()));
}

// 0x00A403A0 (:2854)
const object* object_squad::should_spawn_in_object_tied_up(npc_spawn_param_list& spawn_param_list, const gfm_orders* orders_object, const object* objp)
{
    return spawn_at_node(spawn_param_list, spawn_node_handle);
}

// 0x00A40450 (:2880)
const object* object_squad::should_spawn_in_object_wait(npc_spawn_param_list& spawn_param_list, const gfm_orders* orders_object, const object* objp)
{
    return spawn_at_node(spawn_param_list, spawn_node_handle);
}

// 0x00A40500 (:2908): an action node within 40 m.
const object* object_squad::should_spawn_in_object_ambient(npc_spawn_param_list& spawn_param_list, const gfm_orders* orders_object, const object* objp)
{
    if (objp == nullptr || vec_dist_squared(pos, objp->pos) > 1600.0f)
    {
        return nullptr;
    }
    spawn_param_list.spawn_flags.spawn_in_action_node = true;
    if (objp->get_obj_type() == OT_ACTION_NODE)
    {
        spawn_param_list.node_type = ((const object_action_node*)objp)->base_node_info->action_type;
    }
    const object* parent_obj = World.object_from_handle(objp->get_parent());
    return parent_obj != nullptr ? find_top_parent_object(parent_obj) : objp;
}

// 0x00A6FB70 (:2939)
const object* object_squad::should_spawn_in_object(npc_spawn_param_list& spawn_param_list)
{
    const gfm_orders* orders = Orders_manager.get_orders_from_handle(get_orders_handle());
    if (orders == nullptr)
    {
        return nullptr;
    }
    const object* target = orders->get_target_object();
    switch (orders->get_orders_type())
    {
    case GFM_ORDERS_TYPE_DEFEND:
        return should_spawn_in_object_defend(spawn_param_list, orders, target);
    case GFM_ORDERS_TYPE_PATROL:
        return should_spawn_in_object_patrol(spawn_param_list, orders, target);
    case GFM_ORDERS_TYPE_AMBIENT_ACTION:
        return should_spawn_in_object_ambient(spawn_param_list, orders, target);
    case GFM_ORDERS_TYPE_WAIT:
    case GFM_ORDERS_TYPE_TRAPPED:
    case GFM_ORDERS_TYPE_TRAPPED_JAIL:
        return should_spawn_in_object_wait(spawn_param_list, orders, target);
    case GFM_ORDERS_TIED_UP:
        return should_spawn_in_object_tied_up(spawn_param_list, orders, target);
    default:
        // Within 20 m of the target.
        if (target != nullptr && vec_dist_squared(target->pos, pos) <= 400.0f)
        {
            return find_top_parent_object(target);
        }
        return nullptr;
    }
}

// 0x00A6FCC0 (:2995)
squad_spawn_status object_squad::do_spawn(npc_spawn_param_list& spawn_param_list)
{
    spawn_status_result result = object_spawn_do_spawn(spawn_param_list);
    if (result == SPAWN_STATUS_STREAMING)
    {
        return SQUAD_SPAWN_OK;
    }
    squad_spawn_status status = result == SPAWN_STATUS_SUCCESS ? SQUAD_SPAWN_OK : SQUAD_SPAWN_GENERIC_FAILURE;
    const gfm_orders* orders = Orders_manager.get_orders_from_handle(get_orders_handle());
    for (int i = 0; i < spawn_param_list.npc_spawn_info.size(); i++)
    {
        npc* np = spawn_param_list.npc_spawn_info[i].np;
        if (np == nullptr)
        {
            status = SQUAD_SPAWN_FAILED_NPC_CREATE_FAILED;
        }
        else if (member_array.size() < member_array.capacity())
        {
            np->obj_flags.no_save = 1;
            member_array.add(np->get_handle());
            ai_system* ai = np->get_ai();
            ai->assign_squad(get_handle());
            if (definition != nullptr)
            {
                np->set_team(get_squad_team());
            }
            if (orders != nullptr)
            {
                apply_custom_action(ai, orders);
            }
        }
        else
        {
            // Original: Int3 critical function here.
            World.destroy_object(np, false);
        }
    }
    return status;
}

// 0x00A6FEA0 (:3059)
bool object_squad::spawn_squad_members(const object_spawn_region* osrp)
{
    if (member_array.size() >= 1)
    {
        return true;
    }
    squad_progression_set member;
    member.reset();
    npc_spawn_params params;
    params.reset();
    npc_spawn_param_list list(NPC_MANAGER_SQUAD);
    list.spawn_pos = pos;
    list.spawn_orient = orient;
    list.npc_flags = npc_flags;
    list.npc_flags.immediate_spawn = Immediate_spawn_flag_squad;
    list.hflags = hflags;
    list.osrp = osrp;
    list.vp = should_spawn_in_vehicle(list);
    if (list.vp == nullptr)
    {
        const gfm_orders* orders = get_squad_orders();
        if (orders != nullptr && orders->get_orders_type() == GFM_ORDERS_TYPE_TERRORIZE)
        {
            list.spawn_flags.static_spawn_placement = true;
        }
        else
        {
            list.assign_spawn_parent(should_spawn_in_object(list));
        }
    }
    for (int i = 0; i < definition->total_npc_count; i++)
    {
        if (hflags.radio_operator)
        {
            params.reset();
            params.hum_info = get_radio_operator_human_info();
            if (params.hum_info == nullptr)
            {
                World.destroy_object(this, false);
                return false;
            }
            list.npc_spawn_info.add(params);
            continue;
        }
        if (i == 0 && definition->special_npc_info != nullptr)
        {
            if (!srm_human_info_loaded(definition->special_npc_info))
            {
                return false;
            }
            params.reset();
            params.hum_info = definition->special_npc_info;
        }
        else
        {
            if (!squad_progression_member_human_info(member, get_squad_team(), definition, false, squad_flags.reinforcement_squad))
            {
                if (list.vp != nullptr && list.vp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
                {
                    ((flyer*)list.vp)->suspend_engine_stop();
                }
                return false;
            }
            if (member.h_info == nullptr)
            {
                return false;
            }
            params.reset();
            params.member_load = member.member_load;
            params.hum_info = member.h_info;
        }
        list.npc_spawn_info.add(params);
    }
    if (list.npc_spawn_info.size() < 1)
    {
        return false;
    }
    release_vehicle_path();
    spawn_status = do_spawn(list);
    squad_flags.simulation = member_array.size() == 0;
    if (member_array.size() > 0)
    {
        pick_leader();
    }
    for (int i = 0; i < member_array.size(); i++)
    {
        npc* np = member_array[i] != 0xFFFFFFFF ? World.object_from_handle<npc>(member_array[i]) : nullptr;
        if (np == nullptr)
        {
            continue;
        }
        if (npc_flags.scripted_move)
        {
            np->npc_flags.scripted_move = 1;
            np->scripting_info = scripting_info;
        }
        else if (npc_flags.scripted_drive)
        {
            np->npc_flags.scripted_drive = 1;
            np->scripting_info = scripting_info;
        }
        inventory_item* item = np->inventory;
        while (item != nullptr)
        {
            weapon* wp = item->weapon_handle != 0xFFFFFFFF ? World.object_from_handle<weapon>(item->weapon_handle) : nullptr;
            if (wp != nullptr)
            {
                wp->clear_weapon_persona();
            }
            item = item->next;
            if (item == np->inventory)
            {
                break;
            }
        }
    }
    int icons = 0;
    for (int i = 0; i < member_array.size(); i++)
    {
        npc* np = member_array[i] != 0xFFFFFFFF ? World.object_from_handle<npc>(member_array[i]) : nullptr;
        if (np == nullptr)
        {
            continue;
        }
        if (squad_map_icon != MAP_ELEMENT_INVALID)
        {
            if (squad_map_objective == -1)
            {
                Cartographer.add_icon(np->get_handle(), squad_map_icon, squad_flags.squad_icon_is_sticky, squad_flags.squad_icon_is_closest, false);
            }
            else
            {
                add_objective_icon(squad_map_objective, np->get_handle(), squad_map_icon, squad_flags.squad_icon_is_closest);
            }
            icons++;
        }
        inventory_item* item = np->inventory;
        while (item != nullptr)
        {
            weapon* wp = item->weapon_handle != 0xFFFFFFFF ? World.object_from_handle<weapon>(item->weapon_handle) : nullptr;
            if (wp != nullptr)
            {
                wp->set_new_weapon_persona(get_handle());
            }
            item = item->next;
            if (item == np->inventory)
            {
                break;
            }
        }
    }
    if (member_array.size() > 0 && icons > 0 && squad_map_icon != MAP_ELEMENT_INVALID)
    {
        if (squad_map_objective == -1)
        {
            Cartographer.remove_icon(get_handle());
        }
        else
        {
            remove_objective_icon(squad_map_objective, get_handle());
        }
    }
    if (list.vp != nullptr && vehicle_is_tank(list.vp))
    {
        for (int i = 0; i < member_array.size(); i++)
        {
            npc* np = member_array[i] != 0xFFFFFFFF ? World.object_from_handle<npc>(member_array[i]) : nullptr;
            if (np != nullptr)
            {
                np->hflags.limited_vehicle_exit = 1;
            }
        }
    }
    squad_members_spawned_event spawned;
    spawned.spawn_node_handle = spawn_node_handle;
    gem_post_event(spawned);
    return true;
}

// 0x00A94E60 (:3290)
void object_squad::assign_base_orders(unsigned int new_orders)
{
    if (base_orders_handle != new_orders && new_orders != 0xFFFFFFFF && Orders_manager.get_orders_from_handle(new_orders) != nullptr)
    {
        base_orders_handle = new_orders;
        if (assigned_orders_handle != 0xFFFFFFFF)
        {
            clear_orders(assigned_orders_handle);
        }
        assign_orders_to_members();
    }
}

// 0x00A704C0 (:3321)
void object_squad::assign_orders(unsigned int new_orders)
{
    assigned_orders_handle = new_orders;
    assign_orders_to_members();
}

// 0x00A40570 (:3336)
void object_squad::assign_orders_to_members()
{
    if (squad_flags.simulation)
    {
        return;
    }
    for (int i = 0; i < member_array.size(); i++)
    {
        npc* np = member_array[i] != 0xFFFFFFFF ? World.object_from_handle<npc>(member_array[i]) : nullptr;
        if (np != nullptr && !np->is_killed())
        {
            np->get_ai()->assigned_new_orders();
        }
    }
}

// 0x00A1DA60 (:3358)
void object_squad::clear_all_orders()
{
    if (base_orders_handle != 0xFFFFFFFF)
    {
        clear_orders(base_orders_handle);
    }
    if (assigned_orders_handle != 0xFFFFFFFF)
    {
        clear_orders(assigned_orders_handle);
    }
}

// 0x00A1DA90 (:3370)
void object_squad::release_vehicle_path()
{
    if (dest_spline_node != nullptr)
    {
        vehicle_free_ls_path(&dest_spline_node);
        dest_spline_node = nullptr;
    }
}

// 0x00A704E0 (:3380)
void object_squad::evaluate_orders()
{
    const gfm_orders* base = Orders_manager.get_orders_from_handle(base_orders_handle);
    if (base == nullptr)
    {
        base_orders_handle = 0xFFFFFFFF;
    }
    const gfm_orders* assigned = Orders_manager.get_orders_from_handle(assigned_orders_handle);
    if (assigned == nullptr)
    {
        assigned_orders_handle = 0xFFFFFFFF;
    }
    if (base == nullptr && assigned == nullptr)
    {
        request_despawn_all_members(NPC_DESPAWN_TYPE_LOW, nullptr, false);
    }
}

// 0x00A70540 (:3403)
void object_squad::clear_orders(unsigned int old_orders)
{
    if (old_orders == 0xFFFFFFFF || (Player != nullptr && (Player->escort_squad_handle == old_orders || Player->ally_squad_handle == old_orders)) ||
        (base_orders_handle != old_orders && assigned_orders_handle != old_orders))
    {
        return;
    }
    if (base_orders_handle == old_orders)
    {
        base_orders_handle = 0xFFFFFFFF;
    }
    else
    {
        assigned_orders_handle = 0xFFFFFFFF;
    }
    if (get_squad_orders() == nullptr)
    {
        request_despawn_all_members(NPC_DESPAWN_TYPE_LOW, nullptr, false);
        return;
    }
    assign_orders_to_members();
}

// 0x00A405E0 (:3462)
bool object_squad::request_despawn_all_members(npc_despawn_type despawn_type, const object* despawn_objp, bool destroy_orders)
{
    if (destroy_orders)
    {
        clear_all_orders();
    }
    if (get_squad_orders() != nullptr)
    {
        return false;
    }
    if (squad_flags.simulation)
    {
        World.destroy_object(this, false);
        return true;
    }
    squad_flags.force_spawn = 0;
    squad_flags.flagged_for_despawn = 1;
    squad_flags.disallow_reassignment = 1;
    for (int i = 0; i < member_array.size(); i++)
    {
        npc* np = member_array[i] != 0xFFFFFFFF ? World.object_from_handle<npc>(member_array[i]) : nullptr;
        if (np == nullptr)
        {
            continue;
        }
        np->start_active_despawn(despawn_type, despawn_objp);
        if (squad_map_icon != MAP_ELEMENT_INVALID)
        {
            if (squad_map_objective == -1)
            {
                Cartographer.remove_icon(np->get_handle());
            }
            else if (Cartographer.does_icon_exist(np->get_handle()) || Cartographer.does_sticky_icon_exist(np->get_handle()))
            {
                remove_objective_icon(squad_map_objective, np->get_handle());
            }
        }
    }
    vehicle* vp = get_vehicle();
    if (vp != nullptr)
    {
        vp->m_flags.despawned_squad_vehicle = 1;
    }
    return true;
}

// 0x00A40740 (:3532)
void object_squad::request_ai_replan(unsigned int except_this_npc) const
{
    for (int i = 0; i < member_array.size(); i++)
    {
        unsigned int member_handle = member_array[i];
        npc* np = member_handle != except_this_npc && member_handle != 0xFFFFFFFF ? World.object_from_handle<npc>(member_handle) : nullptr;
        if (np != nullptr)
        {
            np->get_ai()->planner.request_replan();
        }
    }
}

// 0x00A407A0 (:3546)
bool object_squad::should_be_destroyed() const
{
    const gfm_orders* orders = get_orders();
    if (orders == nullptr || orders->get_orders_handle() == 0xFFFFFFFF)
    {
        return true;
    }
    if (squad_flags.simulation)
    {
        return false;
    }
    if (member_array.size() < 1)
    {
        return true;
    }
    // Original: reads through a null npc when the first member's handle went stale.
    npc* np = member_array[0] != 0xFFFFFFFF ? World.object_from_handle<npc>(member_array[0]) : nullptr;
    return np->npc_flags.despawn_npc;
}

// 0x00A40840 (:3580)
bool object_squad::reassign_orders(unsigned int new_orders_handle, unsigned int* old_orders_handle, bool make_base_orders)
{
    if (new_orders_handle == 0xFFFFFFFF || Orders_manager.get_orders_from_handle(new_orders_handle) == nullptr)
    {
        return false;
    }
    if (old_orders_handle != nullptr)
    {
        *old_orders_handle = assigned_orders_handle;
    }
    if (make_base_orders)
    {
        gfm_orders* old_base = Orders_manager.get_orders_from_handle(base_orders_handle);
        if (old_base != nullptr)
        {
            Orders_manager.destroy_orders(old_base);
        }
        base_orders_handle = new_orders_handle;
        assigned_orders_handle = 0xFFFFFFFF;
    }
    else
    {
        assigned_orders_handle = new_orders_handle;
    }
    assign_orders_to_members();
    return true;
}

// 0x00A408D0 (:3614)
bool object_squad::clear_assigned_orders(unsigned int* old_orders_handle)
{
    if (!squad_flags.player_squad)
    {
        return false;
    }
    if (old_orders_handle != nullptr)
    {
        *old_orders_handle = assigned_orders_handle;
    }
    if (Orders_manager.get_orders_from_handle(assigned_orders_handle) == nullptr)
    {
        return false;
    }
    assigned_orders_handle = 0xFFFFFFFF;
    assign_orders_to_members();
    return true;
}

// 0x00A94EB0 (:3642)
void object_squad::update_position()
{
    npc* leader = squad_leader != 0xFFFFFFFF ? World.object_from_handle<npc>(squad_leader) : nullptr;
    if (leader == nullptr || leader->is_killed())
    {
        leader = pick_leader();
    }
    if (leader == nullptr)
    {
        return;
    }
    vehicle* leader_vehicle = leader->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(leader->vehicle_handle) : nullptr;
    if (leader_vehicle != nullptr && human_in_vehicle(leader, leader_vehicle) && leader->vehicle_handle == squad_vehicle)
    {
        pos = leader_vehicle->pos;
        orient = leader_vehicle->orient;
        vehicle_get_velocity(leader_vehicle, simulated_velocity);
        return;
    }
    pos = leader->pos;
    orient = leader->orient;
    simulated_velocity = leader->actual_velocity;
}

// 0x00A705D0 (:3710)
bool object_squad::add_existing_npc(unsigned int npc_handle)
{
    if (member_array.size() >= member_array.capacity())
    {
        return false;
    }
    for (int i = 0; i < member_array.size(); i++)
    {
        if (member_array[i] == npc_handle)
        {
            return false;
        }
    }
    member_array.add(npc_handle);
    if (squad_map_icon != MAP_ELEMENT_INVALID)
    {
        if (squad_map_objective == -1)
        {
            Cartographer.add_icon(npc_handle, squad_map_icon, squad_flags.squad_icon_is_sticky, squad_flags.squad_icon_is_closest, false);
            Cartographer.remove_icon(get_handle());
        }
        else
        {
            add_objective_icon(squad_map_objective, npc_handle, squad_map_icon, squad_flags.squad_icon_is_closest);
            remove_objective_icon(squad_map_objective, get_handle());
        }
    }
    squad_flags.simulation = 0;
    return true;
}

// 0x00A70700 (:3750)
void object_squad::remove_member(unsigned int npc_handle)
{
    int index = -1;
    for (int i = 0; i < member_array.size(); i++)
    {
        if (member_array[i] == npc_handle)
        {
            index = i;
            break;
        }
    }
    if (index < 0)
    {
        return;
    }
    member_array[index] = member_array[member_array.size() - 1];
    member_array.resize(member_array.size() - 1);
    if (squad_map_icon != MAP_ELEMENT_INVALID)
    {
        if (squad_map_objective == -1)
        {
            Cartographer.remove_icon(npc_handle);
        }
        else if (Cartographer.does_icon_exist(npc_handle) || Cartographer.does_sticky_icon_exist(npc_handle))
        {
            remove_objective_icon(squad_map_objective, npc_handle);
        }
    }
    if (member_array.size() >= 1)
    {
        return;
    }
    if (assigned_orders_handle != 0xFFFFFFFF)
    {
        clear_orders(assigned_orders_handle);
    }
    const gfm_orders* orders = Orders_manager.get_orders_from_handle(get_orders_handle());
    if (orders != nullptr)
    {
        if (orders->get_orders_type() == GFM_ORDERS_TYPE_FODDER)
        {
            squad_flags.simulation = 1;
        }
        if (!orders->destroy_when_free())
        {
            if (squad_map_icon == MAP_ELEMENT_INVALID)
            {
                return;
            }
            if (squad_map_objective != -1)
            {
                add_objective_icon(squad_map_objective, get_handle(), squad_map_icon, false);
                return;
            }
            Cartographer.add_icon(get_handle(), squad_map_icon, squad_flags.squad_icon_is_sticky, squad_flags.squad_icon_is_closest, false);
            return;
        }
    }
    clear_all_orders();
    World.destroy_object(this, false);
}

// 0x00A40930 (:3822)
bool object_squad::is_member(const npc* np) const
{
    for (int i = 0; i < member_array.size(); i++)
    {
        if (member_array[i] == np->get_handle())
        {
            return true;
        }
    }
    return false;
}

// 0x00A70890 (:3836)
void object_squad::despawn()
{
    despawn_members();
    clear_all_orders();
    World.destroy_object(this, false);
}

// 0x00A708E0 (:3850)
bool object_squad::should_attempt_spawn() const
{
    if (obj_flags.destroyed || !obj_flags.streamed || definition == nullptr || definition->total_npc_count < 1 || member_array.size() > 0 || (int)spawn_status < 0 || (int)spawn_status > SQUAD_SPAWN_FAILED_NO_OPEN_SPACE ||
        !squad_flags.simulation)
    {
        return false;
    }
    const object* parent_obj = spawn_parent_object();
    if (parent_obj != nullptr && !parents_streamed_in_squad(parent_obj))
    {
        return false;
    }
    if (squad_flags.flier_squad)
    {
        if (!squad_flags.force_spawn && !obj_flags.streamed)
        {
            return false;
        }
    }
    else if (!squad_flags.inside_load_dist && !squad_flags.force_spawn)
    {
        return false;
    }
    if (!World.is_zone_streamed(pos) || World.is_zone_border_zone(pos))
    {
        return false;
    }
    const gfm_orders* orders = get_squad_orders();
    if (orders != nullptr && orders->get_orders_type() == GFM_ORDERS_TYPE_FODDER)
    {
        return should_attempt_spawn_fodder(orders);
    }
    if (veh_spawn_group == nullptr && orders != nullptr)
    {
        if (orders->get_orders_type() == GFM_ORDERS_TYPE_PATROL)
        {
            return should_attempt_spawn_patrol(orders);
        }
        if (orders->get_orders_type() == GFM_ORDERS_TYPE_DEFEND)
        {
            return should_attempt_spawn_defend(orders);
        }
    }
    return true;
}

// 0x00A40970 (:3961)
bool object_squad::should_attempt_spawn_patrol(const gfm_orders* orders) const
{
    if (current_node_handle.is_valid() && orders->get_target_object_handle() == current_node_handle.patrol_handle)
    {
        return current_node_handle.is_streamed();
    }
    return true;
}

// 0x00A409C0 (:3978)
bool object_squad::should_attempt_spawn_defend(const gfm_orders* orders) const
{
    const object* target = orders->get_target_object();
    if (target != nullptr)
    {
        vector bmin, bmax;
        target->get_bbox(bmin, bmax);
        if (ix_point_in_box(pos, bmin, bmax))
        {
            return obj_flags.streamed;
        }
    }
    return true;
}

// 0x00A1DAC0 (:4000)
bool object_squad::should_attempt_spawn_fodder(const gfm_orders* orders) const
{
    return !last_member_removed.valid() || (last_member_removed.time_since() >= FODDER_RESPAWN_MS && npc_flags.orders_active);
}

// 0x00A1DB00 (:4019)
bool object_squad::should_attempt_spawn_defalt() const
{
    return true;
}

// 0x00A1DB10 (:4027)
const object* object_squad::spawn_parent_object() const
{
    const gfm_orders* orders = get_squad_orders();
    if (orders != nullptr)
    {
        switch (orders->get_orders_type())
        {
        case GFM_ORDERS_TYPE_TERRORIZE:
        case GFM_ORDERS_TYPE_WAIT:
        case GFM_ORDERS_TYPE_TRAPPED:
        case GFM_ORDERS_TYPE_TRAPPED_JAIL:
            return World.object_from_handle(spawn_node_handle);
        default:
            break;
        }
    }
    return nullptr;
}

// 0x00A709E0 (:4057)
bool object_squad::should_attempt_despawn() const
{
    if (player_is_player_squad(get_handle()) || (int)spawn_status < 0 || (int)spawn_status > SQUAD_SPAWN_FAILED_NO_OPEN_SPACE || member_array.size() == 0)
    {
        return false;
    }
    if (squad_flags.flier_squad)
    {
        if (obj_flags.streamed)
        {
            return false;
        }
        const vehicle* vp = World.object_from_handle<vehicle>(squad_vehicle);
        if (vp != nullptr && vp->obj_flags.streamed)
        {
            return false;
        }
    }
    if (squad_flags.simulation)
    {
        return false;
    }
    if (squad_flags.special_despawn_rules)
    {
        // Magic number: never within 60 m of the player.
        for (int i = 0; i < member_array.size(); i++)
        {
            const npc* np = World.object_from_handle<npc>(member_array[i]);
            if (np != nullptr && vec_dist_squared(Player->pos, np->pos) < 3600.0f)
            {
                return false;
            }
        }
    }
    return squad_flags.outside_unload_dist && !squad_flags.force_spawn;
}

// 0x00A70AD0 (:4131)
npc* object_squad::get_leader() const
{
    npc* leader = squad_leader != 0xFFFFFFFF ? World.object_from_handle<npc>(squad_leader) : nullptr;
    if (leader != nullptr && !leader->is_killed())
    {
        return leader;
    }
    return pick_leader();
}

// 0x00A40A20 (:4145)
npc* object_squad::pick_leader() const
{
    for (int i = 0; i < member_array.size(); i++)
    {
        npc* np = member_array[i] != 0xFFFFFFFF ? World.object_from_handle<npc>(member_array[i]) : nullptr;
        if (np != nullptr && !np->is_killed())
        {
            // Original: a const method that caches the leader.
            const_cast<object_squad*>(this)->squad_leader = np->get_handle();
            if (npc_flags.squad_leader_ignore_player)
            {
                np->npc_flags.ignore_player = 1;
            }
            return np;
        }
    }
    return nullptr;
}

// 0x00A95010 (:4185)
void squad_invalidate_orders(unsigned int squad_handle, unsigned int order_handle)
{
    object_squad* squad = squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_handle) : nullptr;
    if (squad != nullptr)
    {
        squad->clear_orders(order_handle);
    }
}

// 0x00A70B20 (:4194)
void squad_invalidate_orders(object_squad* squad, unsigned int order_handle)
{
    if (squad != nullptr)
    {
        squad->clear_orders(order_handle);
    }
}

// 0x00AAD470 (:4219)
void squad_definitions_init(int dlc_id)
{
    xml_element* root;
    bool is_dlc = dlc_id != -1;
    if (is_dlc)
    {
        char name[256];
        sprintf(name, "dlc%02d_%s", dlc_id, "squads.xtbl");
        root = xml_table_parse(name);
    }
    else
    {
        if (Squad_name_string_pool_data == nullptr)
        {
            Squad_name_string_pool_data = (char*)Permanent_mempool.alloc(SQUAD_NAME_POOL_SIZE, 0);
            Squad_name_string_pool.init(Squad_name_string_pool_data, SQUAD_NAME_POOL_SIZE, "Squad Def String Pool", 0, false);
        }
        Squad_name_string_pool.clear();
        Npc_squad_def_pool.clear();
        Squad_hash_table.clear();
        root = xml_table_parse("squads.xtbl");
    }
    if (root == nullptr)
    {
        return;
    }
    for (xml_element* cur = xml_find(root, "Squads"); cur != nullptr; cur = xml_find_next(root, cur, "Squads"))
    {
        squad_definition* definition = Npc_squad_def_pool.alloc(0);
        if (definition == nullptr)
        {
            // Original: Int3 critical function here.
            break;
        }
        if (parse_npc_squad_description(definition, cur))
        {
            const squad_definition* value = definition;
            Squad_hash_table.insert_key_data(definition->name, value);
            if (is_dlc)
            {
                Dlc_npc_squad_def_list.add(definition);
            }
        }
        else
        {
            Npc_squad_def_pool.free(definition);
        }
    }
    xml_table_close(root);
}

// 0x00A40AB0 (:4279)
const squad_definition* squad_definition_from_index(int squad_index)
{
    return (unsigned int)squad_index < Npc_squad_def_pool.get_capacity() ? Npc_squad_def_pool.get_entry(squad_index) : nullptr;
}

// 0x00A70BA0 (:4302)
const squad_definition* squad_definition_from_name(const char* name)
{
    if (Squad_name_string_pool_data == nullptr || name == nullptr)
    {
        return nullptr;
    }
    const squad_definition* definition = nullptr;
    Squad_hash_table.get(name, definition);
    return definition;
}

// 0x00A95040 (:4285)
int squad_definition_index_from_name(const char* name)
{
    const squad_definition* definition = squad_definition_from_name(name);
    return definition != nullptr ? squad_definition_index_from_definition(definition) : -1;
}

// 0x00A40AE0 (:4296)
int squad_definition_index_from_definition(const squad_definition* squad_def)
{
    return Npc_squad_def_pool.get_index(squad_def);
}

// 0x00A1DB60 (:4317)
const squad_definition* squad_definition_from_type(gfm_orders_types orders_type, human_teams team)
{
    // Original: Int3 critical function here.
    return nullptr;
}

// 0x00A40B80 (:4837)
bool object_squad::delete_mission_squad() const
{
    if (definition->special_npc_info != nullptr)
    {
        return true;
    }
    if (!human_teams_are_enemies(Player, definition->squad_team))
    {
        return false;
    }
    // Magic number: within 100 m.
    return squad_flags.mission_created || (!squad_flags.simulation && vec_dist_squared(Player->pos, pos) < 10000.0f);
}

// 0x00A1DBD0 (:4872)
bool object_squad::delete_mission_squad_vehicle(const npc* np, const vehicle* vp) const
{
    return vehicle_passenger_get_seat(vp, np) == VSI_DRIVER || Player->vehicle_handle != vp->get_handle();
}

// 0x00A95250 (:4888): within 150 m.
void squad_do_immediate_spawns()
{
    Immediate_spawn_flag_squad = true;
    for (int i = 0; i < World.get_num_objects<object_squad>(); i++)
    {
        object_squad* squad = World.get_object<object_squad>(i);
        if (vec_dist_squared(Player->pos, squad->pos) < 22500.0f)
        {
            squad->set_load_dist_flag();
        }
        if (squad->should_attempt_spawn())
        {
            squad->spawn_members(false, nullptr);
        }
    }
    Immediate_spawn_flag_squad = false;
}

// 0x00A40BF0 (:4918)
void destroy_squad(object_squad* squad)
{
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        npc* np = squad->member_array[i] != 0xFFFFFFFF ? World.object_from_handle<npc>(squad->member_array[i]) : nullptr;
        if (np != nullptr)
        {
            World.destroy_object(np, false);
        }
    }
    vehicle* vp = squad->get_vehicle();
    if (vp != nullptr && vp->get_handle() != Player->last_vehicle_driven)
    {
        World.destroy_object(vp, false);
    }
    World.destroy_object(squad, false);
}

// 0x00753F30 (.h:393)
bool object_squad::has_orders(unsigned int handlep) const
{
    return base_orders_handle == handlep || assigned_orders_handle == handlep;
}

// 0x005DD5E0 (.h:411)
bool object_squad::is_high_priority() const
{
    return squad_flags.high_priority_targets;
}

// 0x005DD5F0 (.h:451)
const vector& object_squad::get_velocity() const
{
    return simulated_velocity;
}

// 0x00753F60 (.h:454)
void object_squad::set_load_dist_flag()
{
    squad_flags.inside_load_dist = 1;
    squad_flags.outside_unload_dist = 0;
}

// 0x00753F80 (.h:455)
void object_squad::set_unload_dist_flag()
{
    squad_flags.inside_load_dist = 0;
    squad_flags.outside_unload_dist = 1;
}

// 0x00753FA0 (.h:457)
void object_squad::set_just_spawned_flag(bool value)
{
    squad_flags.just_spawned = value;
}

// 0x00B1FC40 (.h:459)
void object_squad::set_infinite_mass(bool value)
{
    squad_flags.vehicle_infinite_mass = value;
}

// 0x00A06090 (.h:473)
bool object_squad::activity_spawned() const
{
    return squad_flags.activity;
}

// 0x00753FC0 (.h:481)
bool object_squad::is_player_squad() const
{
    return squad_flags.player_squad;
}

// 0x005DD600 (.h:484)
bool object_squad::stay_in_vehicle() const
{
    return squad_flags.stay_in_vehicle;
}

// 0x00753FD0 (.h:487)
bool object_squad::is_hands_off_raid_squad() const
{
    return squad_flags.hands_off_raid_squad;
}

// 0x00753FE0 (.h:490)
void object_squad::clear_hands_off_flag()
{
    squad_flags.hands_off_raid_squad = 0;
}

// 0x00753FF0 (.h:493)
void object_squad::no_road_spawn()
{
    squad_flags.ignore_road_spawn = 1;
}

// 0x005DD610 (.h:505)
const squad_definition* object_squad::get_squad_def() const
{
    return definition;
}

// 0x00A060A0 (.h:508)
bool object_squad::managed_layer_created() const
{
    return squad_flags.managed_layer_creation;
}

// 0x00754000 (.h:515)
void object_squad::set_special_despawn_rule()
{
    squad_flags.special_despawn_rules = 1;
}

// 0x00A70B40 (:4204)
void console_print_available_squads()
{
    for (const squad_definition* definition = Npc_squad_def_pool.get_first(0); definition != nullptr; definition = Npc_squad_def_pool.get_next(definition))
    {
        console_output(definition->name, nullptr, false);
    }
}

// 0x00A1DB80 (:4677)
static bool npc_dist_compare(npc* const& n1, npc* const& n2)
{
    return vec_dist_squared(Player->pos, n1->pos) <= vec_dist_squared(Player->pos, n2->pos);
}

// 0x00A40B00 (:4709): a living recruitable ally of the player who is in neither of the player's squads.
static bool recruit_collection_callback(object* hit_objp, unsigned int shape_key, const void* data)
{
    if (hit_objp == nullptr || hit_objp->get_obj_type() != OT_HUMAN || hit_objp->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return false;
    }
    const npc* np = (const npc*)hit_objp;
    const player* pp = (const player*)data;
    if (np->npc_flags.not_recruitable || np->is_killed())
    {
        return false;
    }
    if (!human_teams_are_allies(pp, np->h_info->default_team) || !human_teams_are_friendly(pp, np))
    {
        return false;
    }
    const ai_system* ai = np->get_ai();
    return ai != nullptr && ai->get_squad_handle() != pp->escort_squad_handle && ai->get_squad_handle() != pp->ally_squad_handle;
}

// 0x00A950B0 (:4734): recruits the closest candidate within 10 m into the ally squad, turning it guerrilla.
bool player_squad_recruit(player* pp)
{
    if (pp == nullptr || !pp->ally_squad_can_add())
    {
        return false;
    }
    unsigned int handles[MAX_RECRUIT_CANDIDATES];
    const int count = World.collect_objects<npc>(pp->pos, RECRUIT_RADIUS, handles, MAX_RECRUIT_CANDIDATES, recruit_collection_callback, pp);
    npc* recruit = nullptr;
    if (count == 1)
    {
        recruit = handles[0] != 0xFFFFFFFF ? World.object_from_handle<npc>(handles[0]) : nullptr;
    }
    else if (count >= 2)
    {
        farray<npc*,MAX_RECRUIT_CANDIDATES> candidates;
        for (int i = 0; i < count; i++)
        {
            candidates.add(World.object_from_handle<npc>(handles[i]));
        }
        candidates.sort(npc_dist_compare);
        recruit = candidates[0];
    }
    if (recruit == nullptr)
    {
        return false;
    }
    recruit->set_team(HUMAN_TEAM_GUERILLA);
    pp->ally_squad_add(recruit);
    return true;
}

// 0x00A70BF0 (:4814)
bool squad_human_in_player_squad(human* hp, player* pp)
{
    if (hp == nullptr || hp->get_obj_type() != OT_HUMAN || hp->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return false;
    }
    if (pp == nullptr)
    {
        pp = Player;
        if (pp == nullptr)
        {
            return false;
        }
    }
    const object_squad* squad = ((npc*)hp)->get_ai()->get_squad();
    return squad != nullptr && squad->get_player() == pp;
}

// True when the mission clean up treats the two teams as enemies. Teams above marauder never are; a multiplayer mode
// without teams makes everyone enemies.
static bool clean_up_teams_are_enemies(human_teams team1, human_teams team2)
{
    if ((unsigned int)team1 > (unsigned int)HUMAN_TEAM_MARAUDER || (unsigned int)team2 > (unsigned int)HUMAN_TEAM_MARAUDER)
    {
        return false;
    }
    if (Multiplayer && !multi_game_mode_has_teams())
    {
        return true;
    }
    return Current_human_team_dispositions[team1][team2] == TEAM_DISPOSITION_HOSTILE;
}

// 0x00A70C40 (:4945): before a mission starts, removes squads not allied to team 0 unless they came from a zone's own node
// (zone byte of the node's handle at or below HANDLE_MANAGED_LAYER_BIT_ZONE), then vehicles whose squad is gone.
void squad_pre_mission_clean_up()
{
    const int num_squads = World.get_num_objects<object_squad>();
    for (int i = 0; i < num_squads; i++)
    {
        object_squad* squad = World.get_object<object_squad>(i);
        if (squad->get_squad_def() == nullptr)
        {
            continue;
        }
        const human_teams team = squad->get_squad_team();
        const bool allied = (unsigned int)team <= (unsigned int)HUMAN_TEAM_MARAUDER && (!Multiplayer || multi_game_mode_has_teams()) && Current_human_team_dispositions[HUMAN_TEAM_GUERILLA][team] == TEAM_DISPOSITION_ALLIED;
        if (allied)
        {
            continue;
        }
        if (squad->spawn_node_handle == 0xFFFFFFFF || ((squad->spawn_node_handle >> 16) & 0xFF) > HANDLE_MANAGED_LAYER_BIT_ZONE)
        {
            destroy_squad(squad);
        }
    }
    const player* pp = Player;
    const int num_vehicles = World.get_num_objects<vehicle>();
    for (int i = 0; i < num_vehicles; i++)
    {
        vehicle* vp = World.get_object<vehicle>(i);
        if (pp->last_vehicle_driven != vp->get_handle() && vp->squad_handle != 0xFFFFFFFF && World.object_from_handle<object_squad>(vp->squad_handle) == nullptr)
        {
            World.destroy_object(vp, false);
        }
    }
}

// 0x00A70D80 (:4997): after a mission: VIPs go, squads with a special NPC or hostile to the player go when mission created
// or close, then hostile NPCs within 50 m and vehicles flagged for it or left without a squad.
// Original bug kept: a squad without a definition reads through null, and so does the vehicle pass without a player.
void squad_mission_clean_up()
{
    const int num_npcs = World.get_num_objects<npc>();
    for (int i = 0; i < num_npcs; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (np->h_info->flags.vip)
        {
            World.destroy_object(np, false);
        }
    }
    const int num_squads = World.get_num_objects<object_squad>();
    for (int i = 0; i < num_squads; i++)
    {
        object_squad* squad = World.get_object<object_squad>(i);
        const squad_definition* definition = squad->definition;
        bool clean_up = definition->special_npc_info != nullptr;
        if (!clean_up && Player != nullptr && clean_up_teams_are_enemies(Player->current_team, definition->squad_team))
        {
            clean_up = squad->squad_flags.mission_created || (!squad->squad_flags.simulation && vec_dist_squared(Player->pos, squad->pos) < MISSION_CLEAN_UP_SQUAD_DIST_SQ);
        }
        if (!clean_up)
        {
            continue;
        }
        for (int m = 0; m < squad->member_array.size(); m++)
        {
            npc* np = squad->member_array[m] != 0xFFFFFFFF ? World.object_from_handle<npc>(squad->member_array[m]) : nullptr;
            if (np == nullptr)
            {
                continue;
            }
            vehicle* vp = np->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(np->vehicle_handle) : nullptr;
            if (vp != nullptr && (vehicle_passenger_get_seat(vp, np) == VSI_DRIVER || Player->vehicle_handle != vp->get_handle()))
            {
                World.destroy_object(vp, false);
            }
            World.destroy_object(np, false);
        }
        World.destroy_object(squad, false);
    }
    const player* pp = Player;
    const int num_remaining_npcs = World.get_num_objects<npc>();
    for (int i = 0; i < num_remaining_npcs; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (pp != nullptr && clean_up_teams_are_enemies(pp->current_team, np->current_team) && vec_dist_squared(pp->pos, np->pos) < MISSION_CLEAN_UP_NPC_DIST_SQ)
        {
            World.destroy_object(np, false);
        }
    }
    const int num_vehicles = World.get_num_objects<vehicle>();
    for (int i = 0; i < num_vehicles; i++)
    {
        vehicle* vp = World.get_object<vehicle>(i);
        bool destroy = vp->m_flags.delete_mission_vehicle;
        if (!destroy && vp->squad_handle != 0xFFFFFFFF)
        {
            const object_squad* squad = World.object_from_handle<object_squad>(vp->squad_handle);
            destroy = (squad == nullptr || squad->obj_flags.destroyed) && vp->get_handle() != pp->vehicle_handle && vp->get_handle() != pp->last_vehicle_driven;
        }
        if (destroy)
        {
            World.destroy_object(vp, false);
        }
    }
}
