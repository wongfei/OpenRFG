// Original: volition/rfg/code/ai/ambient_action_manager.cpp
//
// One ambient vehicle errand at a time: a ped takes a parked car, a car picks up or drops off peds, or parks. The manager
// polls, starts an action and watches the npcs in it until they finish or it cancels them.

#include "volition/rfg/code/ai/ambient_action_manager.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/spawn/ambient_spawn_manager.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/vlib/code/math/rand.h"

#include <float.h>
#include <math.h>

static timestamp Action_poll_period; // 0x016421CC
static timestamp Despawn_poll_period; // 0x016421D0
static manager_ambient_action_info Current_ambient_action_info; // 0x016421D8
timestamp Spawn_cooldown_period; // 0x01642224
static const ambient_ped_action_type NEW_AMBIENT_ACTION_TYPES[4] = { AMBIENT_PED_ACTION_TYPE_TAKE_PARKED_VEHICLE, AMBIENT_PED_ACTION_TYPE_PARK_VEHICLE, AMBIENT_PED_ACTION_TYPE_DRIVER_PICKUP_PASSENGER, AMBIENT_PED_ACTION_TYPE_DRIVER_DROP_OFF_PASSENGER }; // 0x012C8414
static const int NUM_VEHICLE_HUMANS = 8; // the size of vi_instance::humans

// 0x0064F440 (:93)
manager_ambient_action_info::manager_ambient_action_info()
{
    action_type = INVALID_AMBIENT_PED_ACTION_TYPE;
    sub_action_type = INVALID_AMBIENT_PED_ACTION_TYPE;
    action_flags.sub_action_required = 0;
}

// 0x0064F470
manager_ambient_action_info::~manager_ambient_action_info()
{
}

// 0x0060CA60 (:150)
void manager_ambient_action_info::reset()
{
    action_type = INVALID_AMBIENT_PED_ACTION_TYPE;
    sub_action_type = INVALID_AMBIENT_PED_ACTION_TYPE;
    npc_list.clear();
    action_flags.sub_action_required = 0;
}

// 0x005DF980 (:170)
bool manager_ambient_action_info::is_valid_action() const
{
    switch (action_type)
    {
    case AMBIENT_PED_ACTION_TYPE_TAKE_PARKED_VEHICLE:
    case AMBIENT_PED_ACTION_TYPE_PARK_VEHICLE:
    case AMBIENT_PED_ACTION_TYPE_DRIVER_PICKUP_PASSENGER:
    case AMBIENT_PED_ACTION_TYPE_DRIVER_DROP_OFF_PASSENGER:
        return true;
    case INVALID_AMBIENT_PED_ACTION_TYPE:
    case AMBIENT_PED_ACTION_TYPE_NONE:
    case AMBIENT_PED_ACTION_TYPE_DESPAWN:
    case AMBIENT_PED_ACTION_TYPE_PASSENGER_PICKUP_WAIT:
    case AMBIENT_PED_ACTION_TYPE_PASSENGER_DROP_OFF_WAIT:
        return false;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return false;
    }
}

// 0x0060CA80 (:159)
bool manager_ambient_action_info::is_active() const
{
    return 0 < npc_list.size() && is_valid_action();
}

static npc* get_npc_from_handle(unsigned int handle)
{
    object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
    return objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_NPC ? static_cast<npc*>(objp) : nullptr;
}

// 0x0064F480 (:210)
static bool near_ambient_road_intersection(const vector& pos)
{
    const object_path_road* road = object_road_get_closest_road(pos, false, 15.0f, false, false);
    if (road == nullptr)
    {
        return false;
    }
    const road_spline_connection* connections = road->spline_data.connections.to_pointer();
    for (int i = 0; i < road->spline_data.num_connections; i++)
    {
        if (vec_dist_squared(pos, road->get_road_point(connections[i].from_pt)) < 225.0f)
        {
            return true;
        }
    }
    return false;
}

// 0x005DF9D0 (:240). LTCG: pos ESI.
static bool is_near_ped_network(const vector& pos)
{
    const navpoint* node = ped_node_find_nearest(pos, 50.0f);
    return node != nullptr && vec_dist_squared(pos, node->pos) < 2500.0f;
}

// 0x0064F530 (:260). LTCG: action_info EDI.
static void cancel_ambient_action(manager_ambient_action_info& action_info)
{
    for (int i = 0; i < action_info.npc_list.size(); i++)
    {
        npc* np = get_npc_from_handle(action_info.npc_list[i].npc_handle);
        if (np != nullptr)
        {
            np->get_ai()->amb_action_info.reset(AMBIENT_PED_ACTION_TYPE_DESPAWN);
        }
    }
    action_info.reset();
}

// 0x0064F5D0 (:284). LTCG: np EAX, action_set ECX.
static void cancel_ambient_action(npc* np, ambient_action_set& action_set)
{
    action_set.action_type = AMBIENT_PED_ACTION_TYPE_DESPAWN;
    if (np != nullptr)
    {
        np->get_ai()->amb_action_info.reset(AMBIENT_PED_ACTION_TYPE_DESPAWN);
    }
}

// 0x0064F640 (:297). Whoever left the errand is let go; without its leading npc, or a required helper, the whole errand ends.
static void update_action_set_list(manager_ambient_action_info& action_info)
{
    if (action_info.npc_list.size() < 1)
    {
        cancel_ambient_action(action_info);
        return;
    }
    int num_main = 0;
    int num_sub = 0;
    for (int i = 0; i < action_info.npc_list.size(); i++)
    {
        ambient_action_set& set = action_info.npc_list[i];
        if (set.action_type == AMBIENT_PED_ACTION_TYPE_DESPAWN)
        {
            continue;
        }
        npc* np = get_npc_from_handle(set.npc_handle);
        bool keep = np != nullptr && !np->is_killed();
        if (keep)
        {
            const ai_ambient_action_info& info = np->get_ai()->amb_action_info;
            keep = info.action_type == set.action_type && !(info.action_timeout.valid() && info.action_timeout.elapsed());
            if (keep)
            {
                if (info.action_type == action_info.action_type)
                {
                    num_main++;
                }
                else if (info.action_type == action_info.sub_action_type)
                {
                    num_sub++;
                }
                else
                {
                    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                    keep = false;
                }
            }
        }
        if (!keep)
        {
            cancel_ambient_action(np, set);
        }
    }
    if (0 < num_main && (!action_info.action_flags.sub_action_required || 0 < num_sub))
    {
        for (int i = 0; i < action_info.npc_list.size();)
        {
            if (action_info.npc_list[i].action_type == AMBIENT_PED_ACTION_TYPE_DESPAWN)
            {
                action_info.npc_list.remove_unsorted(i);
            }
            else
            {
                i++;
            }
        }
        if (0 < action_info.npc_list.size())
        {
            return;
        }
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    cancel_ambient_action(action_info);
}

// 0x0064F7C0 (:390). LTCG: action_info ESI.
static void copy_npc_list(const npc* np, ai_ambient_action_info& action_info, const farray<npc*, 100>& selected_npc_list)
{
    action_info.npc_targets.clear();
    for (int i = 0; i < selected_npc_list.size(); i++)
    {
        if (selected_npc_list[i] == np)
        {
            continue;
        }
        if (action_info.npc_targets.full())
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
        else
        {
            action_info.npc_targets.add(selected_npc_list[i]->get_handle());
        }
    }
}

// 0x0064F820 (:431). LTCG: action_info EDI.
static bool start_new_action_take_vehicle(const automobile* ap, npc* driver_np, manager_ambient_action_info& action_info)
{
    if (ap == nullptr || driver_np == nullptr)
    {
        return false;
    }
    action_info.action_flags.sub_action_required = 0;
    action_info.action_type = AMBIENT_PED_ACTION_TYPE_TAKE_PARKED_VEHICLE;
    action_info.sub_action_type = AMBIENT_PED_ACTION_TYPE_RIDE_PARKED_VEHICLE;
    action_info.npc_list.clear();
    ai_system* ai = driver_np->get_ai();
    ai->amb_action_info.reset(AMBIENT_PED_ACTION_TYPE_NONE);
    ai->amb_action_info.vehicle_target_handle = ap->get_handle();
    ai->amb_action_info.reset_action_timeout();
    ai->amb_action_info.action_type = AMBIENT_PED_ACTION_TYPE_TAKE_PARKED_VEHICLE;
    ambient_action_set set;
    set.npc_handle = driver_np->get_handle();
    set.action_type = AMBIENT_PED_ACTION_TYPE_TAKE_PARKED_VEHICLE;
    action_info.npc_list.add(set);
    ai->planner.request_replan();
    return true;
}

// 0x0064F910 (:465). An unoccupied ambient ped within 10 m of the vehicle, facing away from it; each later one that qualifies replaces the last.
static npc* find_ambient_driver(const vehicle* vp)
{
    npc* best = nullptr;
    const float max_dist_sq = 100.0f;
    const int num_npcs = World.get_num_objects<npc>();
    for (int i = 0; i < num_npcs; i++)
    {
        npc* np = World.get_object<npc>(i);
        const ai_system* ai = np->get_ai();
        if (!np->npc_flags.ambient_spawn || np->npc_flags.ambient_vehicle_spawn || ai->amb_action_info.action_type != AMBIENT_PED_ACTION_TYPE_NONE || ai->get_squad_handle() != 0xFFFFFFFF)
        {
            continue;
        }
        const float dist_sq = vec_dist_squared(np->pos, vp->pos);
        if (max_dist_sq < dist_sq)
        {
            continue;
        }
        vector dir(np->pos.x - vp->pos.x, np->pos.y - vp->pos.y, np->pos.z - vp->pos.z);
        if (dir.is_null())
        {
            continue;
        }
        dir.normalize();
        if (0.0f < np->orient.fvec.y * dir.y + np->orient.fvec.x * dir.x + np->orient.fvec.z * dir.z)
        {
            best = np;
        }
    }
    return best;
}

// 0x0064FB30 (:528). LTCG: action_info EDI.
static void start_new_action_take_vehicle(manager_ambient_action_info& action_info)
{
    farray<const automobile*, 32> candidates;
    const player* local = player_get_local_player();
    const int num_nodes = World.get_num_objects<object_vehicle_spawn_node>();
    for (int i = 0; i < num_nodes; i++)
    {
        const object_vehicle_spawn_node* node = World.get_object<object_vehicle_spawn_node>(i);
        if (!node->allow_ambient_action() || 1600.0f < vec_dist_squared(local->pos, node->pos))
        {
            continue;
        }
        const unsigned int handle = node->get_child_vehicle_handle();
        object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
        if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE || objp->get_sub_type() != OT_SUB_VEHICLE_AUTO)
        {
            continue;
        }
        const automobile* ap = static_cast<const automobile*>(objp);
        const human_teams team = ap->info->default_team;
        if (local->last_vehicle_driven != ap->get_handle() && (team == HUMAN_TEAM_CIVILIAN || team == HUMAN_TEAM_GUERILLA) && vehicle_get_num_passengers(ap) < 1 && vec_dist_squared(node->pos, ap->pos) <= 16.0f)
        {
            candidates.add(ap);
        }
    }
    while (0 < candidates.size())
    {
        const int index = candidates.size() == 1 ? 0 : int_rand_range(0, candidates.size() - 1);
        const automobile* ap = candidates[index];
        candidates.remove_unsorted(index);
        npc* driver = find_ambient_driver(ap);
        if (driver != nullptr && start_new_action_take_vehicle(ap, driver, action_info))
        {
            return;
        }
    }
}

// 0x0060CAE0 (:631). LTCG: passenger_list EDI.
static int find_farthest_npc_index(const vector& pos, const farray<npc*, 100>& passenger_list)
{
    if (passenger_list.size() < 1)
    {
        return 0;
    }
    int best = 0;
    float best_dist_sq = vec_dist_squared(pos, passenger_list[0]->pos);
    for (int i = 1; i < passenger_list.size(); i++)
    {
        const float dist_sq = vec_dist_squared(pos, passenger_list[i]->pos);
        if (best_dist_sq < dist_sq)
        {
            best = i;
            best_dist_sq = dist_sq;
        }
    }
    return best;
}

// 0x0064FCF0 (:655). LTCG: passenger_list ECX, action_info EAX.
static bool start_new_action_pickup_in_vehicle(const ambient_pick_up_info& pick_up_info, farray<npc*, 100>& passenger_list, manager_ambient_action_info& action_info)
{
    if (pick_up_info.ap == nullptr || pick_up_info.driver_np == nullptr)
    {
        return false;
    }
    int max_passengers = pick_up_info.availible_seats;
    if (5 <= max_passengers)
    {
        max_passengers = 5;
    }
    else if (max_passengers < 1)
    {
        max_passengers = 1;
    }
    while (max_passengers < passenger_list.size())
    {
        passenger_list.remove_unsorted(find_farthest_npc_index(pick_up_info.ap->pos, passenger_list));
    }
    action_info.action_flags.sub_action_required = 1;
    action_info.action_type = AMBIENT_PED_ACTION_TYPE_DRIVER_PICKUP_PASSENGER;
    action_info.sub_action_type = AMBIENT_PED_ACTION_TYPE_PASSENGER_PICKUP_WAIT;
    action_info.npc_list.clear();
    passenger_list.add(pick_up_info.driver_np);
    for (int i = 0; i < passenger_list.size(); i++)
    {
        npc* np = passenger_list[i];
        ai_system* ai = np->get_ai();
        ai_ambient_action_info& info = ai->amb_action_info;
        info.reset(AMBIENT_PED_ACTION_TYPE_NONE);
        info.vehicle_target_handle = pick_up_info.ap->get_handle();
        info.reset_action_timeout();
        const ambient_ped_action_type type = pick_up_info.driver_np == np ? AMBIENT_PED_ACTION_TYPE_DRIVER_PICKUP_PASSENGER : AMBIENT_PED_ACTION_TYPE_PASSENGER_PICKUP_WAIT;
        info.action_type = type;
        ambient_action_set set;
        set.npc_handle = np->get_handle();
        set.action_type = type;
        action_info.npc_list.add(set);
        copy_npc_list(np, info, passenger_list);
        ai->planner.request_replan();
    }
    return true;
}

// 0x0064FE30 (:778). The ped node ahead of the object, 15 to 75 m off and next to a road, nearest to the player.
const navpoint* find_drop_off_navpoint(const object* objp)
{
    static const float MIN_NAV_DOT = (float)cos((double)(90.0f * 0.017453f)); // 0x012CEAB8, widened from float, so just above zero
    unsigned int handles[256];
    const int num_nodes = World.collect_objects<navpoint>(objp->pos, 75.0f, handles, 256, nullptr, nullptr);
    if (num_nodes <= 0)
    {
        return nullptr;
    }
    const player* local = player_get_local_player();
    const navpoint* best = nullptr;
    float best_player_dist_sq = FLT_MAX;
    for (int i = 0; i < num_nodes; i++)
    {
        const object* node_obj = handles[i] != 0xFFFFFFFF ? World.object_from_handle(handles[i]) : nullptr;
        if (node_obj == nullptr || node_obj->get_obj_type() != OT_NAVPOINT)
        {
            continue;
        }
        const navpoint* node = static_cast<const navpoint*>(node_obj);
        if (node->type != 2) // the ped network's nodes
        {
            continue;
        }
        const float dist_sq = vec_dist_squared(objp->pos, node->pos);
        if (dist_sq < 225.0f || 5625.0f < dist_sq)
        {
            continue;
        }
        vector dir(node->pos.x - objp->pos.x, node->pos.y - objp->pos.y, node->pos.z - objp->pos.z);
        if (dir.is_null())
        {
            continue;
        }
        dir.normalize_safe();
        if (objp->orient.fvec.y * dir.y + objp->orient.fvec.x * dir.x + objp->orient.fvec.z * dir.z < MIN_NAV_DOT)
        {
            continue;
        }
        vector closest;
        float u;
        if (object_road_get_closest_road(node->pos, false, closest, u, 10.0f, false, nullptr, nullptr, false) == nullptr || 100.0f < vec_dist_squared(node->pos, closest))
        {
            continue;
        }
        const float player_dist_sq = vec_dist_squared(local->pos, node->pos);
        if (player_dist_sq < best_player_dist_sq)
        {
            best = node;
            best_player_dist_sq = player_dist_sq;
        }
    }
    return best;
}

// 0x006501C0 (:918)
static void find_local_ped_npcs(const vector& pos, farray<npc*, 100>& npc_list)
{
    npc_list.clear();
    const int num_npcs = World.get_num_objects<npc>();
    for (int i = 0; i < num_npcs; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (np->npc_flags.ambient_spawn && np->vehicle_handle == 0xFFFFFFFF && fabsf(pos.y - np->pos.y) <= 4.0f && vec_dist_squared(np->pos, pos) <= 225.0f && np->get_ai()->amb_action_info.action_type == AMBIENT_PED_ACTION_TYPE_NONE)
        {
            npc_list.add(np);
        }
    }
}

static npc* get_idle_driver(human* driver)
{
    if (driver == nullptr || driver->get_obj_type() != OT_HUMAN || driver->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return nullptr;
    }
    npc* np = static_cast<npc*>(driver);
    return np->get_ai()->amb_action_info.action_type == AMBIENT_PED_ACTION_TYPE_NONE ? np : nullptr;
}

// 0x006502B0 (:959). LTCG: best_ap EDI.
static bool find_local_pick_up_driver(const player* pp, ambient_pick_up_info& best_ap)
{
    best_ap.ap = nullptr;
    best_ap.driver_np = nullptr;
    best_ap.dist_from_player_sqrd = 1600.0f;
    best_ap.availible_seats = 0;
    best_ap.max_passengers = 0;
    best_ap.num_passengers = 0;
    const int num_autos = World.get_num_objects<automobile>();
    for (int i = 0; i < num_autos; i++)
    {
        automobile* vp = World.get_object<automobile>(i);
        if (!vp->m_flags.ambient_spawn)
        {
            continue;
        }
        const float dist_sq = vec_dist_squared(pp->pos, vp->pos);
        const int passengers = vehicle_get_num_passengers(vp);
        if (best_ap.dist_from_player_sqrd < dist_sq || passengers < 1)
        {
            continue;
        }
        const int seats = vehicle_get_num_seats(vp);
        npc* driver = get_idle_driver(vehicle_passenger_in_seat(vp, VSI_DRIVER));
        if (0 < seats - passengers && driver != nullptr && !near_ambient_road_intersection(vp->pos))
        {
            best_ap.driver_np = driver;
            best_ap.ap = vp;
            best_ap.num_passengers = passengers;
            best_ap.max_passengers = seats;
            best_ap.availible_seats = seats - passengers;
            best_ap.dist_from_player_sqrd = dist_sq;
        }
    }
    return best_ap.driver_np != nullptr;
}

// 0x00650420 (:1030). LTCG: action_info EDI.
static void start_new_action_pickup_in_vehicle(manager_ambient_action_info& action_info)
{
    ambient_pick_up_info info;
    if (!find_local_pick_up_driver(player_get_local_player(), info))
    {
        return;
    }
    farray<npc*, 100> peds;
    find_local_ped_npcs(info.ap->pos, peds);
    if (0 < peds.size())
    {
        start_new_action_pickup_in_vehicle(info, peds, action_info);
    }
}

// Everyone aboard the vehicle as npcs. False on a human who is not an npc. Inlined in the drop off and the park.
static bool collect_vehicle_npcs(const automobile* ap, farray<npc*, 100>& list)
{
    list.clear();
    for (int i = 0; i < NUM_VEHICLE_HUMANS; i++)
    {
        object* objp = World.object_from_handle(ap->vii.humans[i].handle);
        if (objp == nullptr)
        {
            continue;
        }
        if (objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC)
        {
            return false;
        }
        list.add(static_cast<npc*>(objp));
    }
    return true;
}

// 0x006504A0 (:1098). LTCG: action_info EAX.
static bool start_new_action_dropoff_in_vehicle(const ambient_drop_off_info& drop_off_info, const navpoint* nearest_ped_node, manager_ambient_action_info& action_info)
{
    if (drop_off_info.ap == nullptr || drop_off_info.driver_np == nullptr)
    {
        return false;
    }
    farray<npc*, 100> aboard;
    if (!collect_vehicle_npcs(drop_off_info.ap, aboard) || aboard.size() < 2)
    {
        return false;
    }
    action_info.action_flags.sub_action_required = 0;
    action_info.action_type = AMBIENT_PED_ACTION_TYPE_DRIVER_DROP_OFF_PASSENGER;
    action_info.sub_action_type = AMBIENT_PED_ACTION_TYPE_PASSENGER_DROP_OFF_WAIT;
    action_info.npc_list.clear();
    for (int i = 0; i < aboard.size(); i++)
    {
        npc* np = aboard[i];
        ai_system* ai = np->get_ai();
        ai_ambient_action_info& info = ai->amb_action_info;
        info.reset(AMBIENT_PED_ACTION_TYPE_NONE);
        info.vehicle_target_handle = drop_off_info.ap->get_handle();
        info.obj_target_handle = nearest_ped_node->get_handle();
        info.reset_action_timeout();
        ambient_ped_action_type type;
        if (drop_off_info.driver_np == np)
        {
            info.destination_pos = drop_off_info.ap->pos;
            type = AMBIENT_PED_ACTION_TYPE_DRIVER_DROP_OFF_PASSENGER;
        }
        else
        {
            info.destination_pos = nearest_ped_node->pos;
            type = AMBIENT_PED_ACTION_TYPE_PASSENGER_DROP_OFF_WAIT;
        }
        info.action_type = type;
        ambient_action_set set;
        set.npc_handle = np->get_handle();
        set.action_type = type;
        action_info.npc_list.add(set);
        copy_npc_list(np, info, aboard);
        ai->planner.request_replan();
    }
    return true;
}

// 0x00650670 (:1167). LTCG: best_info EDI.
static bool find_local_drop_off_driver(const player* pp, ambient_drop_off_info& best_info)
{
    best_info.ap = nullptr;
    best_info.dist_from_player_sqrd = 900.0f;
    best_info.driver_np = nullptr;
    best_info.num_passengers = 0;
    const int num_autos = World.get_num_objects<automobile>();
    for (int i = 0; i < num_autos; i++)
    {
        automobile* vp = World.get_object<automobile>(i);
        if (!vp->m_flags.ambient_spawn)
        {
            continue;
        }
        const float dist_sq = vec_dist_squared(pp->pos, vp->pos);
        const int passengers = vehicle_get_num_passengers(vp);
        if (best_info.dist_from_player_sqrd < dist_sq || passengers < 2)
        {
            continue;
        }
        npc* driver = get_idle_driver(vehicle_get_first_passenger(vp, VSI_DRIVER));
        if (driver != nullptr && driver->npc_flags.ambient_spawn && !near_ambient_road_intersection(vp->pos))
        {
            best_info.driver_np = driver;
            best_info.ap = vp;
            best_info.num_passengers = passengers;
            best_info.dist_from_player_sqrd = dist_sq;
        }
    }
    return best_info.ap != nullptr;
}

// 0x006507B0 (:1230). LTCG: action_info EDI.
static void start_new_action_dropoff_in_vehicle(manager_ambient_action_info& action_info)
{
    ambient_drop_off_info info;
    if (!find_local_drop_off_driver(player_get_local_player(), info))
    {
        return;
    }
    const navpoint* node = ped_node_find_nearest(info.ap->pos, 20.0f);
    if (node != nullptr)
    {
        start_new_action_dropoff_in_vehicle(info, node, action_info);
    }
}

// 0x00650810 (:1260). LTCG: veh_node_list EAX, action_info on the stack.
static bool start_new_action_park_vehicle(const ambient_park_info& park_info, const farray<const object_vehicle_spawn_node*, 64>& veh_node_list, manager_ambient_action_info& action_info)
{
    if (veh_node_list.size() < 1 || park_info.driver_np == nullptr || park_info.ap == nullptr)
    {
        return false;
    }
    farray<const object_vehicle_spawn_node*, 64> fitting;
    for (int i = 0; i < veh_node_list.size(); i++)
    {
        const object_vehicle_spawn_node* node = veh_node_list[i];
        if (node->get_spawn_group() != nullptr && node->get_spawn_group()->v_info_list.find(park_info.ap->info, false) > -1)
        {
            fitting.add(node);
        }
    }
    if (fitting.size() < 1)
    {
        return false;
    }
    farray<npc*, 100> aboard;
    if (!collect_vehicle_npcs(park_info.ap, aboard) || aboard.size() < 1)
    {
        return false;
    }
    const object_vehicle_spawn_node* node = fitting[fitting.size() == 1 ? 0 : int_rand_range(0, fitting.size() - 1)];
    action_info.action_type = AMBIENT_PED_ACTION_TYPE_PARK_VEHICLE;
    action_info.npc_list.clear();
    for (int i = 0; i < aboard.size(); i++)
    {
        npc* np = aboard[i];
        ambient_action_set set;
        set.npc_handle = np->get_handle();
        set.action_type = AMBIENT_PED_ACTION_TYPE_PARK_VEHICLE;
        action_info.npc_list.add(set);
        ai_system* ai = np->get_ai();
        ai_ambient_action_info& info = ai->amb_action_info;
        info.reset(AMBIENT_PED_ACTION_TYPE_NONE);
        info.action_type = AMBIENT_PED_ACTION_TYPE_PARK_VEHICLE;
        info.vehicle_target_handle = park_info.ap->get_handle();
        info.obj_target_handle = node->get_handle();
        info.reset_action_timeout();
        info.destination_pos = node->pos;
        copy_npc_list(np, info, aboard);
        ai->planner.request_replan();
    }
    return true;
}

// 0x00650A60 (:1349)
static void find_empty_parking_places(const vehicle* vp, farray<const object_vehicle_spawn_node*, 64>& veh_node_list)
{
    veh_node_list.clear();
    const int num_nodes = World.get_num_objects<object_vehicle_spawn_node>();
    for (int i = 0; i < num_nodes; i++)
    {
        const object_vehicle_spawn_node* node = World.get_object<object_vehicle_spawn_node>(i);
        if (!node->allow_ambient_action() || node->get_spawn_group()->v_info_list.find(vp->info, false) <= -1)
        {
            continue;
        }
        const unsigned int handle = node->get_child_vehicle_handle();
        const object* child = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
        if (child != nullptr && child->get_obj_type() == OT_VEHICLE && vec_dist_squared(child->pos, node->pos) < 100.0f)
        {
            continue;
        }
        const float dist_sq = vec_dist_squared(vp->pos, node->pos);
        if (dist_sq < 100.0f || 2500.0f < dist_sq || !is_near_ped_network(node->pos))
        {
            continue;
        }
        vector dir(node->pos.x - vp->pos.x, node->pos.y - vp->pos.y, node->pos.z - vp->pos.z);
        if (dir.is_null())
        {
            continue;
        }
        dir.normalize_safe();
        if (0.0f <= vp->orient.fvec.y * dir.y + vp->orient.fvec.x * dir.x + vp->orient.fvec.z * dir.z)
        {
            veh_node_list.add(node);
        }
    }
}

// 0x00650CA0 (:1419). Original quirk kept: the start of 1600 is never compared with, so the last idle driver in the list wins however far it is.
static bool pick_vehicle_to_park(const player* pp, ambient_park_info& best_park_info)
{
    best_park_info.ap = nullptr;
    best_park_info.dist_from_player_sqrd = 1600.0f;
    best_park_info.driver_np = nullptr;
    best_park_info.num_passengers = 0;
    const int num_autos = World.get_num_objects<automobile>();
    for (int i = 0; i < num_autos; i++)
    {
        automobile* vp = World.get_object<automobile>(i);
        const float dist_sq = vec_dist_squared(pp->pos, vp->pos);
        const int passengers = vehicle_get_num_passengers(vp);
        if (passengers < 1)
        {
            continue;
        }
        npc* driver = get_idle_driver(vehicle_get_first_passenger(vp, VSI_DRIVER));
        if (driver != nullptr)
        {
            best_park_info.driver_np = driver;
            best_park_info.ap = vp;
            best_park_info.num_passengers = passengers;
            best_park_info.dist_from_player_sqrd = dist_sq;
        }
    }
    return best_park_info.driver_np != nullptr;
}

// 0x00650DC0 (:1465)
static void start_new_action_park_vehicle(manager_ambient_action_info& action_info)
{
    ambient_park_info info;
    if (!pick_vehicle_to_park(player_get_local_player(), info))
    {
        return;
    }
    farray<const object_vehicle_spawn_node*, 64> nodes;
    find_empty_parking_places(info.ap, nodes);
    if (0 < nodes.size())
    {
        start_new_action_park_vehicle(info, nodes, action_info);
    }
}

// 0x00650E40 (:1485). LTCG: action_info ESI.
static void start_new_ambient_action(manager_ambient_action_info& action_info)
{
    switch (action_info.action_type)
    {
    case AMBIENT_PED_ACTION_TYPE_TAKE_PARKED_VEHICLE:
        start_new_action_take_vehicle(action_info);
        return;
    case AMBIENT_PED_ACTION_TYPE_PARK_VEHICLE:
        start_new_action_park_vehicle(action_info);
        return;
    case AMBIENT_PED_ACTION_TYPE_DRIVER_PICKUP_PASSENGER:
        start_new_action_pickup_in_vehicle(action_info);
        return;
    case AMBIENT_PED_ACTION_TYPE_DRIVER_DROP_OFF_PASSENGER:
        start_new_action_dropoff_in_vehicle(action_info);
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        action_info.reset();
        return;
    }
}

// 0x00650EC0 (:1516)
static void spawn_new_ambient_action(manager_ambient_action_info& action_info)
{
    if (Spawn_cooldown_period.elapsed() && gsm_get_alert_level() <= ALERT_LEVEL_GREEN)
    {
        action_info.reset();
        action_info.action_type = NEW_AMBIENT_ACTION_TYPES[int_rand_range(0, 3)];
        start_new_ambient_action(action_info);
    }
}

// 0x00650F10 (:1541)
static void amb_manager_do_despawn()
{
    if (!Despawn_poll_period.elapsed())
    {
        return;
    }
    Despawn_poll_period.set_rand(1000, 3000);
    const int num_npcs = World.get_num_objects<npc>();
    for (int i = 0; i < num_npcs; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (np->npc_flags.ambient_spawn && np->get_ai()->amb_action_info.action_type == AMBIENT_PED_ACTION_TYPE_DESPAWN)
        {
            const timestamp& timeout = np->get_ai()->amb_action_info.despawn_timeout;
            if (timeout.valid() && timeout.elapsed())
            {
                np->start_active_despawn(NPC_DESPAWN_TYPE_LOW, nullptr);
            }
        }
    }
}

// 0x00650FC0 (:1566)
static void amb_manager_do_frame(manager_ambient_action_info& action_info)
{
    if (ALERT_LEVEL_GREEN < gsm_get_alert_level() || mission_is_active())
    {
        if (action_info.action_type != INVALID_AMBIENT_PED_ACTION_TYPE)
        {
            cancel_ambient_action(action_info);
        }
        return;
    }
    if (action_info.is_active())
    {
        update_action_set_list(action_info);
        return;
    }
    action_info.reset();
    spawn_new_ambient_action(action_info);
}

// 0x0060CB70 (:1609)
void ai_ambient_action_info::reset(ambient_ped_action_type new_action_type)
{
    action_type = new_action_type;
    obj_target_handle = 0xFFFFFFFF;
    vehicle_target_handle = 0xFFFFFFFF;
    destination_pos.zero();
    npc_targets.clear();
    action_timeout.invalidate();
    if (new_action_type == AMBIENT_PED_ACTION_TYPE_DESPAWN)
    {
        despawn_timeout.set(30000);
    }
    else
    {
        despawn_timeout.invalidate();
    }
}

// 0x005DFA60 (:1626)
void ai_ambient_action_info::reset_action_timeout()
{
    action_timeout.set(action_timeout.valid() ? 20000 : 6000);
}

// 0x005DFA90 (:1640)
bool ai_ambient_action_info::has_ambient_action() const
{
    return AMBIENT_PED_ACTION_TYPE_NONE < action_type && action_type <= AMBIENT_PED_ACTION_TYPE_PASSENGER_DROP_OFF_WAIT;
}

// 0x005DFAB0 (:1649)
float ai_ambient_action_info::get_desired_speed(const vehicle* vp) const
{
    return vec_dist_squared(vp->pos, destination_pos) < 400.0f ? 5.0f : 9.0f;
}

// 0x0060CBC0 (:1675)
void amb_action_manager_level_init_post()
{
    Action_poll_period.set_rand(250, 500);
    Despawn_poll_period.set_rand(1000, 3000);
    Current_ambient_action_info.reset();
    Spawn_cooldown_period.set(4000);
}

// 0x00651060 (:1684)
void amb_action_manager_do_frame()
{
    if (Action_poll_period.elapsed())
    {
        Action_poll_period.set_rand(250, 500);
        amb_manager_do_frame(Current_ambient_action_info);
        amb_manager_do_despawn();
    }
}

// 0x005DFAF0 (:1703)
void amb_action_manager_complete()
{
    Spawn_cooldown_period.set(4000);
}

// 0x0064C980 (.h:22)
ai_ambient_action_info::ai_ambient_action_info()
{
    action_type = AMBIENT_PED_ACTION_TYPE_NONE;
    obj_target_handle = 0xFFFFFFFF;
    vehicle_target_handle = 0xFFFFFFFF;
    destination_pos.zero();
}

// 0x005F9C80
ai_ambient_action_info::~ai_ambient_action_info()
{
}
