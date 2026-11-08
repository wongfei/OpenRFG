// Original: volition/rfg/code/ai/escape_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/escape_actions.h"

#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/belief_iterator.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/combat/combat_maneuver.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/gameplay/activities/activity_courier_chase.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/vlib/code/math/rand.h"

// Position 20 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG20")

static const float NEAR_ESCAPE_POS_DISTANCE = 30.0f; // 0x012C883C
static float NEAR_ESCAPE_POS_DISTANCE_SQRD = 900.0f; // 0x01642748, .data, never written
static float HALF_NEAR_ESCAPE_POS_DISTANCE = 15.0f; // 0x0164274C, .data, never written

// 0x0069F360 (:157)
vehicle* find_escape_vehicle(const npc* np)
{
    belief_obj_iterator<vehicle> it;
    it.init(np->get_ai(), AI_BELIEF_VEHICLE);
    for (; it; ++it)
    {
        vehicle* vp = it;
        const player* pl = player_get_local_player();
        // Not one that a hostile drove last, or the player, unless they left it a minute ago.
        const object* driver = World.object_from_handle(vp->most_recent_driver);
        const bool hostile_driver = driver != nullptr && driver->get_obj_type() == OT_HUMAN && human_teams_get_disposition(np, vp->team_of_most_recent_driver) == TEAM_DISPOSITION_HOSTILE;
        const bool player_vehicle = pl != nullptr && pl->last_vehicle_driven == vp->get_handle();
        if ((!(hostile_driver || player_vehicle) || (vp->most_recent_driver_exit_time.valid() && vp->most_recent_driver_exit_time.time_since() >= 60000)) &&
            ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER) && ai_vehicle_is_seat_available(vp, np, VSI_DRIVER))
        {
            return vp;
        }
    }
    return nullptr;
}

// 0x0065D630 (:186)
static bool road_nodes_near_by(const vector& point)
{
    static const float ROAD_SEARCH_RADIUS_SQRD = 10000.0f;
    static const vector ROAD_SEARCH_SIZE(100.0f, 100.0f, 100.0f);
    vector bmin;
    vector bmax;
    bmin.x = point.x - ROAD_SEARCH_SIZE.x;
    bmin.y = point.y - ROAD_SEARCH_SIZE.y;
    bmin.z = point.z - ROAD_SEARCH_SIZE.z;
    bmax.x = point.x + ROAD_SEARCH_SIZE.x;
    bmax.y = point.y + ROAD_SEARCH_SIZE.y;
    bmax.z = point.z + ROAD_SEARCH_SIZE.z;
    unsigned int roads[16];
    const int num_roads = World.collect_objects<object_path_road>(bmin, bmax, roads, 16, nullptr, nullptr);
    for (int i = 0; i < num_roads; i++)
    {
        const object_path_road* road = World.object_from_handle<object_path_road>(roads[i]);
        if (road == nullptr)
        {
            continue;
        }
        for (int j = 0; j < road->spline_data.num_points; j++)
        {
            if (vec_dist_squared(point, road->spline_data.points.to_pointer()[j].pos) < ROAD_SEARCH_RADIUS_SQRD)
            {
                return true;
            }
        }
    }
    return false;
}

// 0x0065D870 (:499). LTCG: nearest_prp EAX. A road connected to this one, or a random point on this one if it has none.
static const object_path_road* pick_escape_road(const object_path_road* nearest_prp, vector& destination_pos)
{
    const road_spline_data& data = nearest_prp->spline_data;
    if (data.connections.to_pointer() == nullptr)
    {
        if (data.num_points > 0)
        {
            destination_pos = data.points.to_pointer()[int_rand_range(0, data.num_points - 1)].pos;
            return nearest_prp;
        }
        return nullptr;
    }
    const road_spline_connection& connection = data.connections.to_pointer()[data.num_connections == 1 ? 0 : int_rand_range(0, data.num_connections - 1)];
    destination_pos = data.points.to_pointer()[connection.from_pt].pos;
    return World.object_from_handle<object_path_road>(connection.handle);
}

// 0x0069F490 (:617)
int escape_on_road_player_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    vehicle* vp;
    if (activity_cc_is_active_courier(np))
    {
        const object_squad* squad = np->get_ai()->get_squad();
        if (squad == nullptr)
        {
            return -1;
        }
        vp = squad->get_vehicle();
    }
    else
    {
        vp = find_escape_vehicle(np);
    }
    if (vp == nullptr || !road_nodes_near_by(vp->pos))
    {
        return -1;
    }
    my_data.use_target = vp;
    return 500;
}

// 0x005E9120 (:656)
void escape_on_road_player_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->desired_vehicle = setup_data.use_target->get_handle();
    ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_DRIVER;
    ai->desired_vehicle_speed = 22.5f;
}

// Inlined in both enter functions below.
static void set_escape_speed(const ai_system* ai, vehicle* vp)
{
    if (vp->vai.vai_flags.m_jumping == 0)
    {
        if (ai->desired_vehicle_speed <= 0.0f)
        {
            vehicle_ai_clear_speed_override(&vp->vai);
        }
        else
        {
            vehicle_ai_set_speed_override(&vp->vai, ai->desired_vehicle_speed);
        }
    }
}

// 0x0065D950 (:664)
ai_results escape_on_road_player_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    set_escape_speed(ai, vp);
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
    vector closest;
    float u;
    const object_path_road* nearest = object_road_get_closest_road(np->pos, np->npc_flags.can_use_special_roads != 0, closest, u, 60.0f, false, nullptr, nullptr, false);
    if (nearest == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const object_path_road* road = pick_escape_road(nearest, ai->desired_destination);
    if (road == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai->desired_destination_object = road->get_handle();
    return AI_RESULT_SUCCESS;
}

// 0x0069F510 (:705)
ai_results escape_on_road_player_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    const object_path_road* current = World.object_from_handle<object_path_road>(ai->desired_destination_object);
    if (vp == nullptr || current == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    // From the asm: the decompile shifts these one to the left.
    const ai_results result = ai_vehicle_travel_to_a_point_do_frame(np, vp, ai->desired_destination, 0xFFFFFFFFFFFFFFFFull, true, HALF_NEAR_ESCAPE_POS_DISTANCE, true, false, false, true);
    if (result != AI_RESULT_SUCCESS && vec_xz_dist_squared(vp->pos, ai->desired_destination) >= NEAR_ESCAPE_POS_DISTANCE_SQRD)
    {
        return result;
    }
    const object_path_road* road = pick_escape_road(current, ai->desired_destination);
    if (road == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai->desired_destination_object = road->get_handle();
    return AI_RESULT_IN_PROGRESS;
}

// 0x0069F5F0 (:748)
int default_escape_player_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    vehicle* vp;
    if (activity_cc_is_active_courier(np))
    {
        const object_squad* squad = np->get_ai()->get_squad();
        if (squad == nullptr)
        {
            return -1;
        }
        vp = squad->get_vehicle();
    }
    else
    {
        if (!object_is_local_player(goal_setup_data.selected_target))
        {
            return -1;
        }
        vp = find_escape_vehicle(np);
    }
    if (vp == nullptr)
    {
        return -1;
    }
    my_data.use_target = vp;
    return 500;
}

// 0x005E9160 (:783)
void default_escape_player_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->desired_vehicle = setup_data.use_target->get_handle();
    ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_DRIVER;
}

// Inlined in the enter and do_action below: 25 to 60 m away from the enemy, or stop when there is no way out.
static void pick_escape_retreat(ai_system* ai, vehicle* vp, const human* enemy)
{
    ai->action_timer.set_rand(3000, 4500);
    if (combat_maneuver_get_vehicle_retreat_pos(ai->desired_destination, vp, enemy, 25.0f, 60.0f, true))
    {
        ai->flags.cannot_retreat = false;
    }
    else
    {
        ai->flags.cannot_retreat = true;
        ai_vehicle_stop(vp);
    }
}

// 0x0065DA40 (:798)
ai_results default_escape_player_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || !human_is_driving(np))
    {
        return AI_RESULT_FAILURE;
    }
    const player* enemy = player_get_local_player();
    if (enemy == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    set_escape_speed(ai, vp);
    if (last_action != this)
    {
        vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
        pick_escape_retreat(ai, vp, enemy);
    }
    if (np->vehicle_handle == vp->get_handle() && np->npc_flags.no_turret_attacks == 0)
    {
        turret* tp = vehicle_get_seat_turret(vp, np->vehicle_seat_idx);
        if (tp != nullptr && !tp->turret_flags.locked_turret)
        {
            human_combat_man_turret(np, tp);
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x006B6A90 (:839)
ai_results default_escape_player_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || !human_is_driving(np))
    {
        return AI_RESULT_FAILURE;
    }
    const player* enemy = player_get_local_player();
    if (enemy == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai_vehicle_use_turret_if_available(np, vp, false, false, nullptr, ZERO_VECTOR);
    const bool elapsed = ai->action_timer.elapsed();
    bool arrived = false;
    if (!ai->flags.cannot_retreat)
    {
        // From the asm: the decompile shifts these one to the left. 3 m is the arrival distance; 144 is 12 m squared.
        const ai_results result = ai_vehicle_travel_to_a_point_do_frame(np, vp, ai->desired_destination, 0xFFFFFFFFFFFFFFFFull, false, 3.0f, false, true, false, true);
        arrived = result == AI_RESULT_SUCCESS || vec_xz_dist_squared(vp->pos, ai->desired_destination) < 144.0f;
    }
    if (!arrived && !elapsed)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    pick_escape_retreat(ai, vp, enemy);
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E9190 (:969)
int bureaucrat_escape_rule::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->npc_flags.bureaucrat != 0 ? 0 : -1;
}

// The singletons, in rfg.exe's construction order: they fill action_pool, and that order breaks ties in the planner.
escape_on_road_player_action escape_on_road_player_action::the_instance; // 0x01D9F858

// 0x0062EF40 (.h:12)
escape_on_road_player_action::escape_on_road_player_action()
    : ai_action("escape road player action", ai_plan_conditions_from_bits({9, 49, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({142}))
{
    flags.has_enter = 1;
}

ai_action* escape_on_road_player_action::get_instance()
{
    return &the_instance;
}

// 0x005E90F0
escape_on_road_player_action::~escape_on_road_player_action()
{
}

// 0x0062F1E0 (.h:12)
int escape_on_road_player_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

default_escape_player_action default_escape_player_action::the_instance; // 0x01DA02CC

// 0x0062F210 (.h:13)
default_escape_player_action::default_escape_player_action()
    : ai_action("default escape player action", ai_plan_conditions_from_bits({9, 49, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({142}))
{
    flags.has_enter = 1;
}

ai_action* default_escape_player_action::get_instance()
{
    return &the_instance;
}

// 0x005E9100
default_escape_player_action::~default_escape_player_action()
{
}

// 0x0062F4B0 (.h:13)
int default_escape_player_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

bureaucrat_escape_rule bureaucrat_escape_rule::the_instance; // 0x01D68748

// 0x0062F4E0 (.h:18)
bureaucrat_escape_rule::bureaucrat_escape_rule()
    : ai_production_rule("bureaucrat escape in vehicle", ai_plan_conditions_from_bits({142}), ai_plan_conditions_from_bits({120}))
{
}

ai_action* bureaucrat_escape_rule::get_instance()
{
    return &the_instance;
}

// 0x005E9110
bureaucrat_escape_rule::~bureaucrat_escape_rule()
{
}

// 0x0062F550 (.h:18)
int bureaucrat_escape_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

