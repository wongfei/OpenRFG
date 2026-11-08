// Original: volition/rfg/code/ai/reinforcement_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/reinforcement_actions.h"

#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"

// Position 28 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG28")

// 0x0066A370 (:60): the orders position on the mesh, or the navpoint nearest to np within 15 m of it.
bool find_reinforcement_destination(vector& dest_pos, const npc* np, const ai_system*, const vector& orders_pos)
{
    static const vector COLLECTION_BBOX(15.0f, 15.0f, 15.0f);
    const nav_cell* cell = bb_pf_get_nav_cell(orders_pos, false, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
    if (cell != nullptr && (cell->flags & NAV_CELL_FLAG_DISABLED) == 0)
    {
        dest_pos = orders_pos;
        rfg_snap_down_to_nav_mesh(dest_pos, cell);
        return true;
    }
    vector bmin;
    vector bmax;
    bmin.x = orders_pos.x - COLLECTION_BBOX.x;
    bmin.y = orders_pos.y - COLLECTION_BBOX.y;
    bmin.z = orders_pos.z - COLLECTION_BBOX.z;
    bmax.x = orders_pos.x + COLLECTION_BBOX.x;
    bmax.y = orders_pos.y + COLLECTION_BBOX.y;
    bmax.z = orders_pos.z + COLLECTION_BBOX.z;
    unsigned int navpoints[128]; // 0x80
    const int count = World.collect_objects<navpoint>(bmin, bmax, navpoints, 128, nullptr, nullptr);
    const navpoint* best = count >= 1 ? World.object_from_handle<navpoint>(navpoints[0]) : nullptr;
    if (best == nullptr)
    {
        return false;
    }
    float best_dist_sq = vec_dist_squared(np->pos, best->pos);
    for (int i = 1; i < count; i++)
    {
        const navpoint* npp = World.object_from_handle<navpoint>(navpoints[i]);
        if (npp != nullptr)
        {
            const float dist_sq = vec_dist_squared(np->pos, npp->pos);
            if (dist_sq < best_dist_sq)
            {
                best = npp;
                best_dist_sq = dist_sq;
            }
        }
    }
    dest_pos = best->pos;
    return true;
}

// 0x005F1410 (:119)
int default_reinforcement_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 16000;
}

// 0x006A86E0 (:137)
ai_results default_reinforcement_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        ai->perception.set_attention_mode(ATTENTION_IDLE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        const object_squad* squad = ai->get_squad();
        if (squad == nullptr)
        {
            return AI_RESULT_SUCCESS;
        }
        const gfm_orders* orders = squad->get_squad_orders();
        if (orders == nullptr)
        {
            return AI_RESULT_SUCCESS;
        }
        if (!find_reinforcement_destination(ai->desired_destination, np, ai, orders->get_orders_pos()))
        {
            ai->planner.avoid_action(this, 5000);
            return AI_RESULT_FAILURE;
        }
    }
    ai_combat_fire_at_target_if_available(np, false, true);
    return AI_RESULT_SUCCESS;
}

// 0x006A8860 (:164)
ai_results default_reinforcement_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const ai_pathfind_results result = ai->pathfind_to(ai->desired_destination, HMS_RUN, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false);
    if (result != AI_PF_SUCCESS && result > AI_PF_RAGDOLLED)
    {
        ai->planner.avoid_action(this, 15000);
        return AI_RESULT_FAILURE;
    }
    ai_combat_fire_at_target_if_available(np, false, true);
    return AI_RESULT_IN_PROGRESS;
}

// 0x006A88D0 (:252)
int reinforcement_vehicle_rule::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const ai_system* ai = np->get_ai();
    if (human_is_driving(np) || ai->passenger_of_squadmate() != nullptr)
    {
        return 100;
    }
    const object_squad* squad = ai->get_squad();
    if (squad == nullptr)
    {
        return 100;
    }
    const npc* leader = squad->get_leader();
    if (leader == nullptr || leader == np || vec_dist_squared(np->pos, leader->pos) > 1600.0f) // 40 m
    {
        return 100;
    }
    if (human_in_vehicle(leader) == nullptr)
    {
        return leader->get_ai()->near_orders_location(0.0f) ? 100 : -1;
    }
    const vehicle* vp = World.object_from_handle<vehicle>(leader->vehicle_handle);
    return ai_vehicle_is_usable_and_available(vp, np, AI_VEHICLE_SEAT_TYPE_PASSENGER) ? -1 : 100;
}

// 0x0066A5C0 (:299)
int follow_to_activity_area::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const ai_system* ai = np->get_ai();
    const object_squad* squad = ai->get_squad();
    if (squad == nullptr)
    {
        return -1;
    }
    const npc* leader = squad->get_leader();
    if (leader == nullptr || leader == np || human_is_driving(np))
    {
        return -1;
    }
    if (ai->passenger_of_squadmate() == nullptr && vec_dist_squared(np->pos, leader->pos) > 625.0f) // 25 m
    {
        return -1;
    }
    return 1;
}

// The singletons, in rfg.exe's construction order: they fill action_pool, and that order breaks ties in the planner.
default_reinforcement_action default_reinforcement_action::the_instance; // 0x01D726DC

// 0x006312F0 (.h:21)
default_reinforcement_action::default_reinforcement_action()
    : ai_action("default reinforcement", ai_plan_conditions_from_bits({0, 2, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({41}))
{
    flags.has_enter = 1;
}

ai_action* default_reinforcement_action::get_instance()
{
    return &the_instance;
}

// 0x005E92E0
default_reinforcement_action::~default_reinforcement_action()
{
}

// 0x00631640 (.h:21)
int default_reinforcement_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

reinforcement_vehicle_rule reinforcement_vehicle_rule::the_instance; // 0x01D74E98

// 0x00631680 (.h:22)
reinforcement_vehicle_rule::reinforcement_vehicle_rule()
    : ai_production_rule("reinforcement vehicle rule", ai_plan_conditions_from_bits({50}), ai_plan_conditions_from_bits({41}))
{
}

ai_action* reinforcement_vehicle_rule::get_instance()
{
    return &the_instance;
}

// 0x005E92F0
reinforcement_vehicle_rule::~reinforcement_vehicle_rule()
{
}

// 0x00631700 (.h:22)
int reinforcement_vehicle_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

follow_to_activity_area follow_to_activity_area::the_instance; // 0x01DDB730

// 0x00631730 (.h:25)
follow_to_activity_area::follow_to_activity_area()
    : ai_production_rule("follow to activity area", ai_plan_conditions_from_bits({39}), ai_plan_conditions_from_bits({41}))
{
}

ai_action* follow_to_activity_area::get_instance()
{
    return &the_instance;
}

// 0x005E9300
follow_to_activity_area::~follow_to_activity_area()
{
}

// 0x006317B0 (.h:25)
int follow_to_activity_area::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

