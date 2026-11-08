// Original: volition/rfg/code/gameplay/activities/vrail_common.cpp
//
// gameflow.md GF4: what both rails share. shared_vrail_patrol_data follows the player's vehicle along its patrol to the
// drop point; shared_vrail_spawn_data sends chase vehicles from the spawn regions scripts marked along the rail.

#include "volition/rfg/code/gameplay/activities/vrail_common.h"

#include "keen/base/trace.hpp"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/gameplay/activities/vrail_data.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/squad_spawn.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_spawn_region.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/math/ix.h"

extern gfm_orders_flags ACTIVITY_ORDERS_FLAGS; // 0x02231B21 (gfm_smash_building_coordinator.cpp)

static const float VRAIL_DROP_OFF_DIST_SQRD = 100.0f; // Magic number: ten metres
static const float VRAIL_REGION_ROUTE_DIST_SQRD = 10000.0f; // Magic number: a region over 100 m from the route counts as passed
static const float MIN_SPAWN_DISTANCE_SQRD_VRAIL = 10000.0f; // 0x016573D0: 100 m
static const float MAX_SPAWN_DISTANCE_SQRD_VRAIL = 160000.0f; // 0x016573D4: 400 m
static const float VRAIL_FLOAT_MAX = 3.4028235e+38f;
static const int VRAIL_REGION_EVALUATION_MS = 2000; // Magic number: the spawn regions are looked at every two seconds
static const int MAX_CHASE_VEHICLES = 12; // Magic number: the capacity of chase_vehicle_array

// Inlined in player_squad_in_vehicle: one of the player's squads, its members all aboard.
static bool squad_members_in_vehicle(unsigned int squad_handle, const vehicle* vp)
{
    const object_squad* squad = squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_handle) : nullptr;
    if (squad == nullptr)
    {
        return true;
    }
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        const unsigned int member = squad->member_array[i];
        const npc* np = member != 0xFFFFFFFF ? World.object_from_handle<npc>(member) : nullptr;
        if (np != nullptr && !human_in_vehicle(np, vp))
        {
            return false;
        }
    }
    return true;
}

// 0x007720F0 (:75)
static bool player_squad_in_vehicle(const player* pp, const vehicle* vp)
{
    return squad_members_in_vehicle(pp->ally_squad_handle, vp) && squad_members_in_vehicle(pp->escort_squad_handle, vp);
}

// 0x007721F0 (:116). rfg.exe: LTCG, region_handle in ESI, spawn_region_list in EDX.
static int find_vrail_spawn_region(unsigned int region_handle, const farray<vrail_spawn_region_info,32>& spawn_region_list)
{
    for (int i = 0; i < spawn_region_list.size(); i++)
    {
        if (spawn_region_list[i].region_handle == region_handle)
        {
            return i;
        }
    }
    return -1;
}

// 0x0078A380 (:130). rfg.exe: LTCG, osrp in EAX, navpoint_list in EDI. Finds the segment of the route nearest the region.
static void add_region_to_list(object_spawn_region* osrp, farray<vrail_spawn_region_info,32>& spawn_region_list, const farray<navpoint*,512>& navpoint_list)
{
    if (spawn_region_list.full() || navpoint_list.size() <= 1)
    {
        return;
    }
    vrail_spawn_region_info best;
    best.region_handle = osrp->get_handle();
    best.index_a = 0;
    best.index_b = 1;
    ix_closest_point_on_line_segment(best.nearest_pos, osrp->pos, navpoint_list[0]->pos, navpoint_list[1]->pos);
    best.dist_to_route = vec_dist_squared(osrp->pos, best.nearest_pos);
    for (int i = 2; i < navpoint_list.size(); i++)
    {
        vector nearest;
        ix_closest_point_on_line_segment(nearest, osrp->pos, navpoint_list[i - 1]->pos, navpoint_list[i]->pos);
        const float dist = vec_dist_squared(osrp->pos, nearest);
        if (dist <= best.dist_to_route)
        {
            best.nearest_pos = nearest;
            best.dist_to_route = dist;
            best.index_a = i - 1;
            best.index_b = i;
        }
    }
    spawn_region_list.add(best);
}

// ---- vrail_spawn_region_info and vrail_patrol_data

// 0x00768E60 (.h:42)
vrail_spawn_region_info::vrail_spawn_region_info()
{
    reset();
}

// 0x00754170 (.h:51)
void vrail_spawn_region_info::reset()
{
    region_handle = 0xFFFFFFFF;
    nearest_pos.zero();
    dist_to_route = VRAIL_FLOAT_MAX;
    past_region = true;
    index_a = -1;
    index_b = -1;
}

// 0x00768EA0 (.h:68)
vrail_patrol_data::vrail_patrol_data()
{
    reset();
}

// 0x007541B0 (.h:74)
void vrail_patrol_data::reset()
{
    patrol = nullptr;
    squad = nullptr;
    vp = nullptr;
}

// ---- shared_vrail_patrol_data

// 0x00768EB0 (.h:87)
shared_vrail_patrol_data::shared_vrail_patrol_data()
{
    reset();
}

// 0x0075BB20 (:167)
void shared_vrail_patrol_data::reset()
{
    vrail_patrol_handle = 0xFFFFFFFF;
    vrail_patrol_orders_handle = 0xFFFFFFFF;
    vrail_patrol_squad = 0xFFFFFFFF;
    last_node_handle.reset();
    last_patrol_state = INVALID_VRAIL_PATROL_STATE;
    vrail_patrol_flags = {};
    drop_off_pos.zero();
}

// 0x00772210 (:179)
unsigned int shared_vrail_patrol_data::get_patrol_vehicle_handle() const
{
    const object_squad* squad = vrail_patrol_squad != 0xFFFFFFFF ? World.object_from_handle<object_squad>(vrail_patrol_squad) : nullptr;
    return squad != nullptr ? squad->squad_vehicle : 0xFFFFFFFF;
}

// 0x00772240 (:192)
bool shared_vrail_patrol_data::player_in_vrail_vehicle() const
{
    return human_in_vehicle(player_get_local_player(), get_patrol_vehicle_handle());
}

// 0x00772290 (:204)
bool shared_vrail_patrol_data::get_frame_data(vrail_patrol_data& vrail_objects)
{
    vrail_objects.patrol = vrail_patrol_handle != 0xFFFFFFFF ? World.object_from_handle<object_patrol>(vrail_patrol_handle) : nullptr;
    if (vrail_objects.patrol == nullptr)
    {
        return false;
    }
    vrail_objects.squad = vrail_patrol_squad != 0xFFFFFFFF ? World.object_from_handle<object_squad>(vrail_patrol_squad) : nullptr;
    if (vrail_objects.squad == nullptr || vrail_objects.squad->is_killed())
    {
        return false;
    }
    vrail_objects.vp = vrail_objects.squad->get_vehicle();
    return vrail_objects.vp == nullptr || !vrail_objects.vp->is_killed();
}

// 0x0075BB60 (:237)
bool shared_vrail_patrol_data::is_in_use() const
{
    return vrail_patrol_handle != 0xFFFFFFFF;
}

// 0x00772330 (:243): the squad's first member, not necessarily in the driver's seat.
npc* shared_vrail_patrol_data::get_vrail_driver() const
{
    const object_squad* squad = vrail_patrol_squad != 0xFFFFFFFF ? World.object_from_handle<object_squad>(vrail_patrol_squad) : nullptr;
    if (squad == nullptr)
    {
        return nullptr;
    }
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        const unsigned int member = squad->member_array[i];
        npc* np = member != 0xFFFFFFFF ? World.object_from_handle<npc>(member) : nullptr;
        if (np != nullptr)
        {
            return np;
        }
    }
    return nullptr;
}

// 0x0078A500 (:267)
bool shared_vrail_patrol_data::vrail_init(object_squad* squad, const vector& drop_point)
{
    if (squad == nullptr || squad->simulated() || vrail_patrol_handle != 0xFFFFFFFF)
    {
        return false;
    }
    reset();
    if (!squad->npc_flags.activity_driver_vr)
    {
        return false;
    }
    gfm_orders* orders = squad->get_squad_orders();
    if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_PATROL)
    {
        return false;
    }
    object* target = orders->get_target_object();
    if (target == nullptr || target->get_obj_type() != OT_PATROL || squad->get_vehicle() == nullptr)
    {
        return false;
    }
    object_patrol* patrol = static_cast<object_patrol*>(target);
    farray<npc*,10> members;
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        npc* np = World.object_from_handle<npc>(squad->member_array[i]);
        if (np != nullptr)
        {
            members.add(np);
            np->get_ai()->planner.request_replan();
        }
    }
    if (members.size() <= 0)
    {
        return false;
    }
    if (!patrol->get_last_patrol_node_handle(last_node_handle))
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return false;
    }
    vrail_patrol_handle = patrol->get_handle();
    vrail_patrol_orders_handle = orders->get_orders_handle();
    vrail_patrol_squad = squad->get_handle();
    drop_off_pos = drop_point;
    squad->current_node_handle = patrol->get_first_patrol_point_handle();
    last_patrol_state = VRAIL_PATROL_STATE_WAIT;
    return true;
}

// 0x007A9E10 (:336)
void shared_vrail_patrol_data::release_and_reset()
{
    if (vrail_patrol_orders_handle != 0xFFFFFFFF)
    {
        Orders_manager.destroy_orders(vrail_patrol_orders_handle); // rfg.exe inlines it
        vrail_patrol_orders_handle = 0xFFFFFFFF;
    }
    object_squad* squad = vrail_patrol_squad != 0xFFFFFFFF ? World.object_from_handle<object_squad>(vrail_patrol_squad) : nullptr;
    if (squad != nullptr)
    {
        squad->request_despawn_all_members(NPC_DESPAWN_TYPE_LOW, nullptr, true);
    }
    reset();
}

// 0x007723A0 (:357)
vrail_patrol_state shared_vrail_patrol_data::get_patrol_status(const vrail_patrol_data& vrail_objects) const
{
    if (vrail_patrol_flags.vrail_complete)
    {
        return vrail_patrol_flags.player_exited_vehicle ? VRAIL_PATROL_STATE_COMPLETE : VRAIL_PATROL_STATE_COMPLETE_WAIT;
    }
    const player* pp = player_get_local_player();
    if (human_in_vehicle(pp, vrail_objects.vp) && player_squad_in_vehicle(pp, vrail_objects.vp))
    {
        return vrail_patrol_flags.visited_last_node ? VRAIL_PATROL_STATE_GOING_TO_DROP_OFF : VRAIL_PATROL_STATE_PATROL;
    }
    return VRAIL_PATROL_STATE_WAIT;
}

// 0x0075BB70 (:391)
void shared_vrail_patrol_data::update_vrail_progress_patrol(const vrail_patrol_data& vrail_objects)
{
    const bool at_last_node = patrol_handles_are_equal(last_node_handle, vrail_objects.squad->current_node_handle);
    if (vrail_patrol_flags.found_last_node)
    {
        if (!at_last_node)
        {
            vrail_patrol_flags.visited_last_node = true;
        }
    }
    else if (at_last_node)
    {
        vrail_patrol_flags.found_last_node = true;
    }
}

// 0x0075BBC0 (:410)
void shared_vrail_patrol_data::update_vrail_progress_drop_off(const vrail_patrol_data& vrail_objects)
{
    if (vec_dist_squared(vrail_objects.squad->pos, drop_off_pos) < VRAIL_DROP_OFF_DIST_SQRD)
    {
        vrail_patrol_flags.vrail_complete = true;
    }
}

// 0x0075BBF0 (:421)
void shared_vrail_patrol_data::update_vrail_progress_complete_wait(const vrail_patrol_data& vrail_objects)
{
    // Deviation: rfg.exe reads through null when the squad has lost its vehicle.
    if (vrail_objects.vp == nullptr || vrail_objects.vp->get_handle() != player_get_local_player()->vehicle_handle)
    {
        vrail_patrol_flags.player_exited_vehicle = true;
    }
}

// 0x0075BC20 (:435)
void shared_vrail_patrol_data::update_vrail_progress(const vrail_patrol_data& vrail_objects)
{
    switch (last_patrol_state)
    {
    case VRAIL_PATROL_STATE_PATROL:
        update_vrail_progress_patrol(vrail_objects);
        break;
    case VRAIL_PATROL_STATE_GOING_TO_DROP_OFF:
        update_vrail_progress_drop_off(vrail_objects);
        break;
    case VRAIL_PATROL_STATE_COMPLETE_WAIT:
        update_vrail_progress_complete_wait(vrail_objects);
        break;
    default:
        break;
    }
}

// 0x0079B610 (:459)
vrail_patrol_state shared_vrail_patrol_data::do_frame()
{
    if (vrail_patrol_handle == 0xFFFFFFFF)
    {
        return INVALID_VRAIL_PATROL_STATE;
    }
    if (vrail_patrol_flags.vrail_complete && vrail_patrol_flags.player_exited_vehicle)
    {
        return VRAIL_PATROL_STATE_COMPLETE;
    }
    if (vrail_patrol_flags.vrail_failed)
    {
        return VRAIL_PATROL_STATE_FAILED;
    }
    vrail_patrol_data vrail_objects;
    if (!get_frame_data(vrail_objects))
    {
        vrail_patrol_flags.vrail_failed = true;
        last_patrol_state = VRAIL_PATROL_STATE_FAILED;
        return VRAIL_PATROL_STATE_FAILED;
    }
    update_vrail_progress(vrail_objects);
    last_patrol_state = get_patrol_status(vrail_objects);

    // Original: asks the global queries rather than this rail's own squad and vehicle, so a mission rail wins over
    // riding shotgun. Deviation: starts invalid, where rfg.exe would read an unset stack word if no rail answered.
    unsigned int handle = 0xFFFFFFFF;
    vrail_get_patrol_squad(handle);
    object_squad* squad = handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(handle) : nullptr;
    if (squad != nullptr)
    {
        for (int i = 0; i < squad->member_array.size(); i++)
        {
            npc* np = World.object_from_handle<npc>(squad->member_array[i]);
            if (np == nullptr)
            {
                continue;
            }
            vrail_get_patrol_vehicle(handle);
            if (human_in_vehicle(np, handle))
            {
                squad->hflags.limited_vehicle_exit = 1;
                np->hflags.limited_vehicle_exit = 1;
            }
        }
    }
    return last_patrol_state;
}

// ---- shared_vrail_spawn_data

// 0x007A0080 (.h:180)
shared_vrail_spawn_data::shared_vrail_spawn_data()
{
    reset();
}

// 0x0077E3A0
shared_vrail_spawn_data::~shared_vrail_spawn_data()
{
}

// 0x0079B730 (:520)
void shared_vrail_spawn_data::reset()
{
    current_vrail_def = nullptr;
    spawn_region_list.clear();
    spawn_evaluation_period.set(0);
    region_evaluation_period.set(0);
    orders_evaluation_period.invalidate();
    vrail_patrol_handle = 0xFFFFFFFF;
    vrail_patrol_squad = 0xFFFFFFFF;
    chase_vehicle_count = 0;
    for (int i = 0; i < chase_vehicle_array.capacity(); i++)
    {
        chase_vehicle_array[i].release_and_reset(); // rfg.exe inlines it
    }
}

// 0x0079B7F0 (:540). Original: clears these flags on every region and NPC, also those of the other rail.
void shared_vrail_spawn_data::release_and_reset()
{
    for (int i = 0; i < chase_vehicle_array.capacity(); i++)
    {
        chase_vehicle_array[i].release_and_reset(); // rfg.exe inlines it
    }
    for (int i = 0; i < World.get_num_objects<object_spawn_region>(); i++)
    {
        World.get_object<object_spawn_region>(i)->set_vrail_spawn_flag(false);
    }
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        World.get_object<npc>(i)->npc_flags.activity_pursuit_vr = 0;
    }
    reset();
}

// 0x0075BCC0 (:566)
bool shared_vrail_spawn_data::is_in_use() const
{
    return current_vrail_def != nullptr;
}

// 0x0079B920 (:577)
bool shared_vrail_spawn_data::vrail_init(const object_squad* squad, const vehicle_rail_definition* new_vrail_def, const object_patrol* vrail_patrol, int new_chase_vehicle_count)
{
    if (new_vrail_def == nullptr || vrail_patrol == nullptr || squad == nullptr || current_vrail_def != nullptr)
    {
        return false;
    }
    reset();
    current_vrail_def = new_vrail_def;
    vrail_patrol_handle = vrail_patrol->get_handle();
    set_chase_vehicle_count(new_chase_vehicle_count);
    vrail_patrol_squad = squad->get_handle();
    orders_evaluation_period.set(0);
    return true;
}

// 0x00772410 (:600)
void shared_vrail_spawn_data::set_chase_vehicle_count(int new_chase_vehicle_count)
{
    chase_vehicle_count = new_chase_vehicle_count;
    if (new_chase_vehicle_count < 0)
    {
        chase_vehicle_count = 0;
    }
    else if (new_chase_vehicle_count > MAX_CHASE_VEHICLES)
    {
        chase_vehicle_count = MAX_CHASE_VEHICLES;
    }
}

// 0x007B7080 (:607)
void shared_vrail_spawn_data::do_frame_spawn()
{
    if (current_vrail_def != nullptr)
    {
        do_frame_spawn_new();
    }
}

// 0x007AF620 (:617)
void shared_vrail_spawn_data::do_frame()
{
    for (int i = 0; i < chase_vehicle_array.capacity(); i++)
    {
        chase_vehicle_array[i].do_frame();
    }
}

// 0x00772440 (:627)
bool shared_vrail_spawn_data::get_next_patrol_node_handle(patrol_node_handle& node_handle)
{
    const object_squad* squad = vrail_patrol_squad != 0xFFFFFFFF ? World.object_from_handle<object_squad>(vrail_patrol_squad) : nullptr;
    if (squad == nullptr)
    {
        return false;
    }
    node_handle = squad->current_node_handle;
    return node_handle.is_valid();
}

// 0x0078A690 (:643): keeps the marked regions, each with the part of the route it sits beside.
bool shared_vrail_spawn_data::build_spawn_region_info()
{
    if (spawn_region_list.size() > 0 && !region_evaluation_period.elapsed())
    {
        return true;
    }
    region_evaluation_period.set(VRAIL_REGION_EVALUATION_MS);
    object_patrol* patrol = vrail_patrol_handle != 0xFFFFFFFF ? World.object_from_handle<object_patrol>(vrail_patrol_handle) : nullptr;
    patrol_node_handle next_node;
    if (patrol == nullptr || !get_next_patrol_node_handle(next_node))
    {
        return false;
    }
    farray<navpoint*,512> navpoints;
    patrol->get_all_patrol_nodes(navpoints);
    int current_index = 0;
    while (current_index < navpoints.size() && navpoints[current_index]->get_handle() != next_node.nav_handle)
    {
        current_index++;
    }
    if (current_index >= navpoints.size())
    {
        return false;
    }
    for (int i = 0; i < spawn_region_list.size();)
    {
        const unsigned int handle = spawn_region_list[i].region_handle;
        const object_spawn_region* region = handle != 0xFFFFFFFF ? World.object_from_handle<object_spawn_region>(handle) : nullptr;
        if (region == nullptr || !region->get_vrail_spawn_flag())
        {
            spawn_region_list.remove_unsorted(i); // rfg.exe inlines it
        }
        else
        {
            i++;
        }
    }
    for (int i = 0; i < World.get_num_objects<object_spawn_region>(); i++)
    {
        object_spawn_region* region = World.get_object<object_spawn_region>(i);
        if (region->get_vrail_spawn_flag() && find_vrail_spawn_region(region->get_handle(), spawn_region_list) < 0)
        {
            add_region_to_list(region, spawn_region_list, navpoints);
        }
    }
    for (int i = 0; i < spawn_region_list.size(); i++)
    {
        vrail_spawn_region_info& info = spawn_region_list[i];
        info.past_region = info.dist_to_route >= VRAIL_REGION_ROUTE_DIST_SQRD || info.index_a <= current_index;
    }
    return true;
}

// 0x00772490 (:723)
const object_spawn_region* shared_vrail_spawn_data::find_vehicle_spawn_region_both()
{
    const player* pp = player_get_local_player();
    const object_spawn_region* best = nullptr;
    float best_dist = VRAIL_FLOAT_MAX;
    for (int i = 0; i < spawn_region_list.size(); i++)
    {
        const unsigned int handle = spawn_region_list[i].region_handle;
        const object_spawn_region* region = handle != 0xFFFFFFFF ? World.object_from_handle<object_spawn_region>(handle) : nullptr;
        if (region == nullptr)
        {
            continue;
        }
        const float dist = vec_dist_squared(pp->pos, region->pos);
        if (MIN_SPAWN_DISTANCE_SQRD_VRAIL <= dist && dist <= MAX_SPAWN_DISTANCE_SQRD_VRAIL && dist < best_dist)
        {
            best = region;
            best_dist = dist;
        }
    }
    return best;
}

// 0x00772590 (:753)
const object_spawn_region* shared_vrail_spawn_data::find_vehicle_spawn_region_dir(bool behind_vrail)
{
    const player* pp = player_get_local_player();
    const object_spawn_region* best = nullptr;
    float best_dist = VRAIL_FLOAT_MAX;
    for (int i = 0; i < spawn_region_list.size(); i++)
    {
        const vrail_spawn_region_info& info = spawn_region_list[i];
        if (info.past_region != behind_vrail)
        {
            continue;
        }
        const object_spawn_region* region = info.region_handle != 0xFFFFFFFF ? World.object_from_handle<object_spawn_region>(info.region_handle) : nullptr;
        if (region == nullptr)
        {
            continue;
        }
        const float dist = vec_dist_squared(pp->pos, region->pos);
        if (MIN_SPAWN_DISTANCE_SQRD_VRAIL <= dist && dist <= MAX_SPAWN_DISTANCE_SQRD_VRAIL && dist < best_dist)
        {
            best = region;
            best_dist = dist;
        }
    }
    return best;
}

// 0x0078A950 (:794)
const object_spawn_region* shared_vrail_spawn_data::find_vehicle_spawn_region()
{
    if (!build_spawn_region_info() && spawn_region_list.size() <= 0)
    {
        return nullptr;
    }
    if (current_vrail_def->vrail_flags.spawn_both)
    {
        return find_vehicle_spawn_region_both();
    }
    return find_vehicle_spawn_region_dir(current_vrail_def->vrail_flags.spawn_back);
}

// 0x007B4300 (:813)
void shared_vrail_spawn_data::do_spawn(int squad_index, reinforcement_squad& chase_vehicle_info)
{
    if (squad_index < 0 || squad_index >= current_vrail_def->chase_vehicle_list.size())
    {
        return;
    }
    chase_vehicle_info.release_and_reset();
    const object_spawn_region* region = find_vehicle_spawn_region();
    if (region == nullptr)
    {
        return;
    }
    const player* pp = player_get_local_player();
    const squad_vehicle_pair& info = current_vrail_def->chase_vehicle_list[squad_index];
    squad_spawn_parameters params;
    params.orders.ofp = ACTIVITY_ORDERS_FLAGS;
    params.orders.orders_type = GFM_ORDERS_TYPE_ATTACK;
    params.orders.location_target = pp->pos;
    params.orders.object_target = pp->get_handle();
    params.squad.osrp = region;
    params.squad.squad_def = info.squad_def;
    params.squad.veh_spawn_group = info.veh_group;
    params.squad.npc_flags.activity_pursuit_vr = 1;
    params.squad.hflags.disallow_vehicle_exit = 1;
    params.squad.squad_pos = region->pos;
    params.squad.squad_flags.disallow_reassignment = 1;
    vector to_player = pp->pos - params.squad.squad_pos;
    to_player.y = 0.0f;
    if (to_player.is_null())
    {
        params.squad.squad_orient.make_identity();
    }
    else
    {
        params.squad.squad_orient.make(to_player);
    }
    if (chase_vehicle_info.spawn_squad(pp->get_handle(), params, false, true) && params.created_orders != nullptr && params.spawned_squad != nullptr)
    {
        chase_vehicle_info.squad_handle = params.spawned_squad->get_handle();
        chase_vehicle_info.orders_handle = params.created_orders->get_orders_handle();
        params.spawned_squad->squad_flags.ignore_road_spawn = 1;
    }
}

// 0x007B44D0 (:877)
void shared_vrail_spawn_data::do_frame_spawn_new()
{
    if (chase_vehicle_count > MAX_CHASE_VEHICLES)
    {
        return;
    }
    for (int i = 0; i < chase_vehicle_count; i++)
    {
        reinforcement_squad& chase = chase_vehicle_array[i];
        if (chase.squad_despawned() && current_vrail_def->chase_vehicle_list[i].respawn_delay < chase.get_deathtime())
        {
            do_spawn(i, chase);
        }
    }
}

// 0x0078A990 (:898)
int shared_vrail_spawn_data::get_squads(base_array<object_squad*>& squad_list)
{
    squad_list.clear();
    for (int i = 0; i < chase_vehicle_array.capacity(); i++)
    {
        const unsigned int handle = chase_vehicle_array[i].squad_handle;
        object_squad* squad = handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(handle) : nullptr;
        if (squad != nullptr)
        {
            squad_list.add(squad); // base_array::add reports a full list
        }
    }
    return squad_list.size();
}
