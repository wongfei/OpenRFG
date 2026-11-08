// Original: volition/rfg/code/ai/follow_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/follow_actions.h"

#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/belief_iterator.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/custom_actions.h"
#include "volition/rfg/code/ai/follow_logic.h"
#include "volition/rfg/code/ai/formation.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/ai/self_preservation_actions.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/gameplay/activities/activity_assassin.h"
#include "volition/rfg/code/gameplay/activities/vrail_system.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_convoy.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle/vehicle_interaction_utilities.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/rfg/code/weapons/weapons.h"

#include <float.h>

// Position 21 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG21")

static const float MAX_DEFEND_HUMAN_DIST_SQ = 49.0f; // 0x016421C0, also in goals.cpp
static const float MAX_WAIT_FOR_BODYGUARDS_DIST_SQ = MAX_DEFEND_HUMAN_DIST_SQ * 25.0f; // 0x01DCA568
static const float MIN_WAIT_FOR_BODYGUARDS_DIST_SQ = MAX_DEFEND_HUMAN_DIST_SQ * 2.25f; // 0x01D75688

static const unsigned int COND_REACH_VEHICLE = 48;
static const unsigned int COND_IN_VEHICLE = 49;

// rfg.exe inlines this in every action that puts a driver or passenger in a vehicle.
static void man_seat_turret(npc* np, const vehicle* vp)
{
    if (np->vehicle_handle == vp->get_handle() && np->npc_flags.no_turret_attacks == 0)
    {
        turret* tp = vehicle_get_seat_turret(vp, np->vehicle_seat_idx);
        if (tp != nullptr && !tp->turret_flags.locked_turret)
        {
            human_combat_man_turret(np, tp);
        }
    }
}

// The human the squad's defend or roadblock orders are about, as far as the orders go; rfg.exe inlines it.
static object* get_defended_object(const ai_system* ai)
{
    const object_squad* squad = ai->get_squad();
    if (squad == nullptr)
    {
        return nullptr;
    }
    gfm_orders* orders = squad->get_squad_orders();
    if (orders == nullptr || (orders->get_orders_type() != GFM_ORDERS_TYPE_DEFEND && orders->get_orders_type() != GFM_ORDERS_TYPE_ROADBLOCK))
    {
        return nullptr;
    }
    return orders->get_target_object();
}

// 0x0065DBA0 (:110)
bool all_followers_stopped(const npc* leader_np, const ai_system* leader_aip)
{
    const object_squad* squad = leader_aip->get_squad();
    if (squad == nullptr)
    {
        return true;
    }
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        const unsigned int handle = squad->member_array[i];
        if (handle == leader_np->get_handle())
        {
            continue;
        }
        const npc* hp = World.object_from_handle<npc>(handle);
        if (hp != nullptr && !human_is_dead(hp) && hp->cur_movement_state != HMS_IDLE && hp->get_ai()->is_following_leader(leader_np))
        {
            return false;
        }
    }
    return true;
}

// 0x0067B010 (:151): near enough bodyguards are success; farther ones are waited for while they can still come.
ai_results ai_get_wait_for_bodyguards_result(const npc* np)
{
    float dist_sq;
    const npc* bodyguard = activity_assassin_get_closest_bodyguard(np, &dist_sq);
    if (bodyguard == nullptr || dist_sq >= MAX_WAIT_FOR_BODYGUARDS_DIST_SQ)
    {
        return AI_RESULT_FAILURE;
    }
    const vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return dist_sq <= MIN_WAIT_FOR_BODYGUARDS_DIST_SQ ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
    }
    if (!vehicle_is_usable(nullptr, nullptr, vp, false, false) || vp->get_sub_type() == OT_SUB_VEHICLE_WALKER)
    {
        return AI_RESULT_FAILURE;
    }
    if (vp->m_flags.player_may_capture != 0 && vehicle_get_hit_points(vp) <= (unsigned int)vp->info->m_abandon_hitpoints)
    {
        return AI_RESULT_FAILURE;
    }
    if (!vp->no_npc_entry_timer.elapsed())
    {
        return AI_RESULT_FAILURE;
    }
    if (vp->get_sub_type() != OT_SUB_VEHICLE_FLYER && vp->orient.uvec.y < 0.5f)
    {
        float speed;
        vehicle_get_speed(vp, speed);
        if (speed < 1.0f)
        {
            return AI_RESULT_FAILURE; // on its side
        }
    }
    if (vp->m_flags.m_on_fire != 0)
    {
        if (vp->m_flags.m_invulnerable == 0)
        {
            return AI_RESULT_FAILURE;
        }
    }
    else if (vp->m_flags.m_engine_smoking != 0 && vp->get_sub_type() != OT_SUB_VEHICLE_FLYER)
    {
        return AI_RESULT_FAILURE;
    }
    // Dropped: rfg.exe looks up the rail patrol squad's first member here and does nothing with it.
    if (!vehicle_has_gunner_seat(vp))
    {
        return AI_RESULT_FAILURE;
    }
    return get_first_gunner(vp) != nullptr ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

// 0x006317E0 (:192)
int follow_leader_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const object_squad* squad = np->get_ai()->get_squad();
    if (squad == nullptr)
    {
        return -1;
    }
    npc* leader = squad->get_leader();
    if (leader == nullptr || np == leader || human_in_vehicle(leader) != nullptr)
    {
        return -1;
    }
    my_data.use_target = leader;
    return 1000;
}

// 0x005E9310 (:215)
void follow_leader_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    if (setup_data.use_target != nullptr)
    {
        np->get_ai()->desired_follow_target = setup_data.use_target->get_handle();
    }
}

// 0x006BCF70 (:223)
ai_results follow_leader_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    const object_squad* squad = ai->get_squad();
    if (squad == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    npc* leader = squad->get_leader();
    if (np == leader || leader == nullptr || leader->is_killed() || ai->follow_in_formation(leader, AI_FORMATION_FLOCK) == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    ai_combat_fire_at_target_if_available(np, false, true);
    return AI_RESULT_SUCCESS;
}

// 0x006B6C10 (:258)
ai_results follow_leader_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const object_squad* squad = ai->get_squad();
    if (squad == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const npc* leader = squad->get_leader();
    if (np == leader || leader->get_ai()->planner.get_current_action() == run_around_panicked_action::get_instance())
    {
        return AI_RESULT_FAILURE;
    }
    const vehicle* vp = human_in_vehicle(leader);
    if (vp != nullptr && ai_vehicle_is_usable_and_available(vp, np, AI_VEHICLE_SEAT_TYPE_PASSENGER))
    {
        return AI_RESULT_SUCCESS;
    }
    human_set_combat_ready(np, human_is_combat_ready(leader), 0);
    const unsigned int formation = ai->get_formation_handle();
    if (formation == 0xFFFFFFFF || !ai_formation_is_leader(leader->get_handle(), formation) || !ai_do_follow_frame(np, formation, HST_STAND))
    {
        return AI_RESULT_FAILURE;
    }
    ai_combat_fire_at_target_if_available(np, false, true);
    return AI_RESULT_IN_PROGRESS;
}

// 0x006BD000 (:311)
ai_results follow_leader_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        np->get_ai()->leave_formation();
    }
    return AI_RESULT_SUCCESS;
}

// 0x00631850 (:332)
int watch_leader_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const object_squad* squad = np->get_ai()->get_squad();
    if (squad == nullptr)
    {
        return -1;
    }
    const npc* leader = squad->get_leader();
    if (leader == nullptr || np == leader || human_in_vehicle(leader) != nullptr)
    {
        return -1;
    }
    // (-x & 0xFFFFFC21) + 0x3DE: only while the leader panics.
    return leader->get_ai()->planner.get_current_action() != run_around_panicked_action::get_instance() ? -1 : 990;
}

// 0x0069F670 (:357)
ai_results watch_leader_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    const object_squad* squad = ai->get_squad();
    if (squad == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const npc* leader = squad->get_leader();
    if (leader == nullptr || np == leader)
    {
        return AI_RESULT_FAILURE;
    }
    ai->stop_moving();
    ai->perception.pay_attention(leader->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    return AI_RESULT_SUCCESS;
}

// 0x005E9340 (:377)
ai_results watch_leader_action::do_action(npc*) const
{
    return AI_RESULT_IN_PROGRESS;
}

// 0x006318E0 (:385)
int wait_for_squad_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const object_squad* squad = np->get_ai()->get_squad();
    if (squad == nullptr || np != squad->get_leader())
    {
        return -1;
    }
    return human_in_vehicle(np) != nullptr ? -1 : 200; // (-x & 0xFFFFFF37) + 200
}

// 0x00631940 (:409)
ai_results wait_for_squad_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    const object_squad* squad = ai->get_squad();
    if (squad == nullptr || np != squad->get_leader())
    {
        return AI_RESULT_FAILURE;
    }
    if (!squad->followers_near_leader() && last_action != this)
    {
        ai->stop_moving();
    }
    return AI_RESULT_SUCCESS;
}

// 0x0065DC40 (:436)
ai_results wait_for_squad_action::do_action(npc* np) const
{
    const ai_system* ai = np->get_ai();
    const object_squad* squad = ai->get_squad();
    if (squad == nullptr || np != squad->get_leader())
    {
        return AI_RESULT_FAILURE;
    }
    return squad->followers_near_leader() && all_followers_stopped(np, ai) ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

// 0x005E9350 (:467)
int wait_for_bodyguards_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 1000;
}

// 0x0069F6F0 (:472)
ai_results wait_for_bodyguards_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    const human* target = human_combat_get_target(np);
    const bool has_firearm = human_weapons_is_firearm_equipped(np);
    if (last_action != this)
    {
        ai->stop_moving();
        human_set_stance(np, HST_STAND);
        if (target == nullptr)
        {
            ai->perception.set_attention_mode(ATTENTION_IDLE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        }
        else
        {
            ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, has_firearm ? ATTENTION_AIM_AT_OBJECT : ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        }
        ai->action_timer.set_rand(1000, 1500);
    }
    if (target != nullptr && has_firearm)
    {
        human_set_combat_ready(np, true, 0);
    }
    return AI_RESULT_SUCCESS;
}

// 0x0069F870 (:506)
ai_results wait_for_bodyguards_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    ai_combat_action_parameters params;
    params.init_npc(np);
    if (params.init_target(false))
    {
        bool can_fire = false;
        if (params.weapon_inst != nullptr)
        {
            const weapon_ammo_type ammo = params.weapon_inst->w_info->ammo_type;
            can_fire = (ammo == WEAPON_AMMO_TYPE_BULLET || ammo == WEAPON_AMMO_TYPE_PROJECTILE || ammo == WEAPON_AMMO_TYPE_ELECTRICITY || ammo == WEAPON_AMMO_TYPE_REPAIR || ammo == WEAPON_AMMO_TYPE_BLANKS || ammo == WEAPON_AMMO_TYPE_FLAME) &&
                       params.weapon_inst->is_loaded();
        }
        if (can_fire)
        {
            if (ai->perception.get_attention_mode() != ATTENTION_AIM_AT_OBJECT && params.target != nullptr)
            {
                ai->perception.pay_attention(params.target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
            }
            human_set_combat_ready(np, true, 0);
            ai_combat_fire_at_target(params, false);
        }
        else if (ai->perception.get_attention_mode() != ATTENTION_LOOK_AT_OBJECT)
        {
            if (params.target != nullptr)
            {
                ai->perception.pay_attention(params.target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
            }
            human_set_combat_ready(np, false, 0);
        }
    }
    if (ai->action_timer.elapsed())
    {
        ai->action_timer.set_rand(1000, 1500);
        return ai_get_wait_for_bodyguards_result(np);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E9360 (:551)
int wait_bodyguards_vehicle_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 1000;
}

// 0x006319B0 (:556)
ai_results wait_bodyguards_vehicle_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        ai_vehicle_stop(vp);
        ai->action_timer.set_rand(1000, 1500);
    }
    return AI_RESULT_SUCCESS;
}

// 0x0067B1C0 (:577)
ai_results wait_bodyguards_vehicle_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (human_in_vehicle(np) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (ai->action_timer.elapsed())
    {
        ai->action_timer.set_rand(1000, 1500);
        return ai_get_wait_for_bodyguards_result(np);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x0067B220 (:603)
int wait_for_squad_vehicle_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const object_squad* squad = np->get_ai()->get_squad();
    if (squad == nullptr || np != squad->get_leader())
    {
        return -1;
    }
    const vehicle* vp = human_in_vehicle(np);
    return vp != nullptr && ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER) ? 200 : -1;
}

// 0x00631A00 (:628)
ai_results wait_for_squad_vehicle_action::enter(npc* np, const ai_action*) const
{
    const object_squad* squad = np->get_ai()->get_squad();
    if (squad == nullptr || np != squad->get_leader())
    {
        return AI_RESULT_FAILURE;
    }
    return human_in_vehicle(np) != nullptr ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x00631A50 (:652)
ai_results wait_for_squad_vehicle_action::do_action(npc* np) const
{
    const object_squad* squad = np->get_ai()->get_squad();
    if (squad == nullptr || np != squad->get_leader())
    {
        return AI_RESULT_FAILURE;
    }
    automobile* ap = static_cast<automobile*>(human_in_vehicle(np)); // Original: any vehicle type is treated as a car here.
    if (ap == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (squad->followers_in_vehicle())
    {
        return AI_RESULT_SUCCESS;
    }
    automobile_set_handbrake(ap, true);
    automobile_set_steering(ap->get_handle(), 0.0f);
    automobile_set_brake_pedal_input(ap->get_handle(), 1.0f);
    automobile_clear_accelerator(ap);
    return AI_RESULT_IN_PROGRESS;
}

// 0x0069F990 (:690)
int passenger_of_squadmate_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    const ai_system* ai = np->get_ai();
    const object_squad* squad = ai->get_squad();
    if (squad == nullptr || np == squad->get_leader())
    {
        return -1;
    }
    vehicle* current = ai->passenger_of_squadmate();
    my_data.use_target = current;
    if (current != nullptr)
    {
        return 510;
    }
    // The nearest vehicle driven by a squadmate who is not busy elsewhere.
    float best_dist_sq = FLT_MAX;
    vehicle* best = nullptr;
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        const npc* member = World.object_from_handle<npc>(squad->member_array[i]);
        if (member == nullptr || member == np)
        {
            continue;
        }
        if (goal_setup_data.goal == chase_target_goal::get_instance())
        {
            const ai_goal* member_goal = member->get_ai()->planner.get_current_goal();
            if (member_goal != chase_target_goal::get_instance() && member_goal != wait_for_passengers_goal::get_instance())
            {
                continue;
            }
        }
        vehicle* vp = World.object_from_handle<vehicle>(member->vehicle_handle);
        if (vp == nullptr || vehicle_passenger_in_entering_or_reserving_seat(vp, VSI_DRIVER) != member || !ai_vehicle_is_usable_and_available(vp, np, AI_VEHICLE_SEAT_TYPE_PASSENGER))
        {
            continue;
        }
        const float dist_sq = vp->get_handle() == np->vehicle_handle ? 0.0f : vec_dist_squared(np->pos, vp->pos);
        if (dist_sq < best_dist_sq)
        {
            best = vp;
            best_dist_sq = dist_sq;
        }
    }
    if (best == nullptr)
    {
        return -1;
    }
    if (np->vehicle_handle != best->get_handle())
    {
        desired_conditions |= ai_plan_conditions(COND_REACH_VEHICLE) | ai_plan_conditions(COND_IN_VEHICLE);
    }
    my_data.use_target = best;
    return 510;
}

// 0x00631B00 (:768)
void passenger_of_squadmate_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    const object* target = setup_data.use_target;
    if (target != nullptr && target->get_obj_type() == OT_VEHICLE)
    {
        const vehicle* vp = static_cast<const vehicle*>(target);
        const human* driver = vehicle_passenger_in_entering_or_reserving_seat(vp, VSI_DRIVER);
        if (driver != nullptr)
        {
            ai->desired_vehicle = vp->get_handle();
            ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_PASSENGER;
            ai->desired_follow_target = driver->get_handle();
        }
    }
}

// 0x00631B50 (:782)
ai_results passenger_of_squadmate_action::enter(npc* np, const ai_action*) const
{
    const vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    man_seat_turret(np, vp);
    return AI_RESULT_SUCCESS;
}

// 0x006B6D00 (:794)
ai_results passenger_of_squadmate_action::do_action(npc* np) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai_vehicle_use_turret_if_available(np, vp, false, false, nullptr, ZERO_VECTOR);
    return AI_RESULT_IN_PROGRESS;
}

// 0x0067B290 (:892)
int passenger_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return -1;
    }
    return ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_PASSENGER) ? 1000 : -1; // (-x & 0x3E9) - 1
}

// 0x00631BC0 (:906)
ai_results passenger_action::enter(npc* np, const ai_action*) const
{
    const vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    man_seat_turret(np, vp);
    return AI_RESULT_SUCCESS;
}

// 0x006B6D60 (:918)
ai_results passenger_action::do_action(npc* np) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai_vehicle_use_turret_if_available(np, vp, false, false, nullptr, ZERO_VECTOR);
    return AI_RESULT_IN_PROGRESS;
}

// 0x0069FC00 (:934). LTCG: vp EAX, np EDI. A vehicle an allied driver uses to defend target.
static bool is_entourage_vehicle(const vehicle* vp, const npc* np, const human* target)
{
    if (vp == nullptr || np == nullptr || target == nullptr)
    {
        return false;
    }
    if (!ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_PASSENGER) || !passenger_seat_available(vp, np))
    {
        return false;
    }
    human* driver = vehicle_passenger_in_entering_or_reserving_seat(vp, VSI_DRIVER);
    if (driver == nullptr || !human_teams_are_allies(np, driver))
    {
        return false;
    }
    const npc* driver_npc = World.object_cast<npc>(driver);
    return driver_npc != nullptr && driver_npc->get_ai()->get_defend_object() == target;
}

// 0x0069FC80 (:977)
int ride_with_human_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const ai_system* ai = np->get_ai();
    const object* defended = get_defended_object(ai);
    if (defended == nullptr || defended->get_obj_type() != OT_HUMAN)
    {
        return -1;
    }
    const human* target = static_cast<const human*>(defended);
    vehicle* target_vehicle = World.object_from_handle<vehicle>(target->vehicle_handle);
    if ((human_in_vehicle(target, target_vehicle) || human_in_vehicle_transition(target, target_vehicle, VI_INTERACTION_ENTER)) && target_vehicle != nullptr &&
        ai_vehicle_is_usable(target_vehicle, np, AI_VEHICLE_SEAT_TYPE_PASSENGER) && passenger_seat_available(target_vehicle, np))
    {
        if (target_vehicle->get_handle() != np->vehicle_handle)
        {
            desired_conditions |= ai_plan_conditions(COND_REACH_VEHICLE) | ai_plan_conditions(COND_IN_VEHICLE);
        }
        my_data.use_target = target_vehicle;
        return 500;
    }
    if (vec_dist_squared(np->pos, target->pos) < 400.0f || human_is_driving(np)) // 20 m
    {
        return -1;
    }
    vehicle* ride = human_in_vehicle(np);
    if (ride == nullptr || !is_entourage_vehicle(ride, np, target))
    {
        ride = nullptr;
        belief_obj_iterator<vehicle> it;
        it.init(ai, AI_BELIEF_VEHICLE);
        for (; it; ++it)
        {
            vehicle* vp = it;
            if (is_entourage_vehicle(vp, np, target))
            {
                ride = vp;
                break;
            }
        }
        if (ride == nullptr)
        {
            return -1;
        }
    }
    if (ride->get_handle() != np->vehicle_handle)
    {
        desired_conditions |= ai_plan_conditions(COND_REACH_VEHICLE) | ai_plan_conditions(COND_IN_VEHICLE);
    }
    my_data.use_target = ride;
    return 700;
}

// 0x005E9370 (:1051)
void ride_with_human_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    if (setup_data.use_target != nullptr)
    {
        ai->desired_vehicle = setup_data.use_target->get_handle();
        ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_PASSENGER;
    }
}

// 0x00631C30 (:1059)
ai_results ride_with_human_action::enter(npc* np, const ai_action*) const
{
    const vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    man_seat_turret(np, vp);
    return AI_RESULT_SUCCESS;
}

// 0x006B6DC0 (:1071)
ai_results ride_with_human_action::do_action(npc* np) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai_vehicle_use_turret_if_available(np, vp, false, false, nullptr, ZERO_VECTOR);
    return AI_RESULT_IN_PROGRESS;
}

// 0x00631CA0 (:1219)
int escort_vehicle_action::get_cost(const npc*, const ai_goal_setup_data& goal_setup_data) const
{
    const object* defended = goal_setup_data.defend_object;
    if (defended == nullptr)
    {
        return -1;
    }
    if (defended->get_obj_type() == OT_VEHICLE)
    {
        return 100;
    }
    if (defended->get_obj_type() == OT_HUMAN && human_in_vehicle(static_cast<const human*>(defended)) != nullptr)
    {
        return 5000;
    }
    return -1;
}

// 0x006BD020 (:1239)
ai_results escort_vehicle_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    const human* hp = World.object_from_handle<human>(ai->desired_follow_target);
    if (hp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const vehicle* vp = human_in_vehicle(hp);
    if (vp == nullptr || vp->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this && ai->follow_in_formation(vp, AI_FORMATION_VEHICLE_ESCORT) == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    ai_combat_fire_at_target_if_available(np, false, true);
    return AI_RESULT_SUCCESS;
}

// 0x006B6E20 (:1273)
ai_results escort_vehicle_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const human* hp = World.object_from_handle<human>(ai->desired_follow_target);
    if (hp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const vehicle* vp = human_in_vehicle(hp);
    if (vp == nullptr || vp->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    const unsigned int formation = ai->get_formation_handle();
    if (formation == 0xFFFFFFFF || vp != ai_formation_get_leader(formation))
    {
        return AI_RESULT_FAILURE;
    }
    // Slower than 3 m/s (9 squared): get in if there is room.
    if (np->hflags.disallow_vehicle_entry == 0 && vp->last_velocity * vp->last_velocity < 9.0f)
    {
        if (passenger_seat_available(vp, np) || gunner_seat_available(vp, np, false, nullptr) != nullptr)
        {
            return AI_RESULT_SUCCESS;
        }
    }
    if (!ai_do_follow_frame(np, formation, HST_STAND))
    {
        return AI_RESULT_FAILURE;
    }
    ai_combat_fire_at_target_if_available(np, false, true);
    return AI_RESULT_IN_PROGRESS;
}

// 0x006BD0B0 (:1321)
ai_results escort_vehicle_action::cleanup(npc* np, const ai_action* next_action) const
{
    ai_system* ai = np->get_ai();
    if (next_action != this)
    {
        ai->leave_formation();
        ai->set_desired_burst_size(0);
    }
    return AI_RESULT_SUCCESS;
}

// 0x00631CE0 (:1336)
int escort_simulated_vehicle_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (goal_setup_data.defend_object == nullptr || goal_setup_data.defend_object->get_obj_type() != OT_SQUAD)
    {
        return -1;
    }
    const object_squad* squad = np->get_ai()->get_squad();
    return squad != nullptr && squad->get_leader() == np ? 100 : -1;
}

// 0x0069FFA0 (:1355)
ai_results escort_simulated_vehicle_action::enter(npc* np, const ai_action*) const
{
    ai_combat_fire_at_target_if_available(np, false, true);
    return AI_RESULT_SUCCESS;
}

// 0x0069FFC0 (:1369)
ai_results escort_simulated_vehicle_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const object_squad* squad = World.object_from_handle<object_squad>(ai->desired_follow_target);
    if (squad == nullptr || !squad->simulated())
    {
        return AI_RESULT_FAILURE;
    }
    if (vec_dist_squared(squad->pos, np->pos) > 25.0f && !ai->is_pathing_to(squad->pos, 0.25f, 1.0f))
    {
        ai->pathfind_to(squad->pos, HMS_RUN, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false);
    }
    ai_combat_fire_at_target_if_available(np, false, true);
    return AI_RESULT_IN_PROGRESS;
}

// 0x00631D40 (:1399)
int follow_escort_simulated_rule::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (goal_setup_data.defend_object == nullptr || goal_setup_data.defend_object->get_obj_type() != OT_SQUAD)
    {
        return -1;
    }
    const object_squad* squad = np->get_ai()->get_squad();
    return squad != nullptr && squad->get_leader() != np ? 100 : -1;
}

// 0x0067B2D0 (:1421)
int leave_convoy_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->get_ai()->get_convoy_object() != nullptr ? 1 : -1; // (-x & 2) - 1
}

// 0x0067B2F0 (:1437)
ai_results leave_convoy_action::do_action(npc* np) const
{
    const ai_system* ai = np->get_ai();
    object_convoy* convoy = ai->get_convoy_object();
    if (convoy == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    convoy->leave_convoy_formation(ai->get_squad_handle());
    return AI_RESULT_SUCCESS;
}

// 0x006A0080 (:1459)
int convoy_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const object_squad* squad = np->get_ai()->get_squad();
    if (squad == nullptr || squad->get_leader() != np)
    {
        return -1;
    }
    vehicle* vp = squad->get_vehicle();
    if (vp == nullptr || !ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER) || !ai_vehicle_is_seat_available(vp, np, VSI_DRIVER))
    {
        return -1;
    }
    my_data.use_target = vp;
    return 100;
}

// 0x005E93A0 (:1488)
void convoy_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    if (setup_data.use_target != nullptr)
    {
        ai->desired_vehicle = setup_data.use_target->get_handle();
        ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_DRIVER;
    }
}

// 0x006BD0E0 (:1496)
ai_results convoy_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || vehicle_passenger_in_entering_or_reserving_seat(vp, VSI_DRIVER) != np)
    {
        return AI_RESULT_FAILURE;
    }
    man_seat_turret(np, vp);
    if (ai->vehicle_follow_convoy_start() == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
    }
    return AI_RESULT_SUCCESS;
}

// 0x006B6F20 (:1530)
ai_results convoy_action::do_action(npc* np) const
{
    const ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai_vehicle_use_turret_if_available(np, vp, false, false, nullptr, ZERO_VECTOR);
    const object_convoy* convoy = ai->get_convoy_object();
    if (convoy == nullptr || !convoy->in_convoy_formation(ai->get_squad_handle()))
    {
        return AI_RESULT_FAILURE;
    }
    const unsigned int formation = ai->get_vehicle_formation_handle();
    if (formation == 0xFFFFFFFF || ai_formation_is_leader(ai->get_squad_handle(), formation))
    {
        return AI_RESULT_FAILURE;
    }
    return ai_vehicle_do_follow_frame(np, vp, formation) ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x005E93D0 (:1570)
int move_with_human_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 10000;
}

// 0x006BD190 (:1575)
ai_results move_with_human_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    const object* target = get_defended_object(ai);
    if (target == nullptr || target->get_obj_type() != OT_HUMAN || target->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        // (-player & -3) + TIGHT: the plain follow formation behind a player.
        const ai_formation_types type = object_is_player(target) ? AI_FORMATION_ESCORT_PLAYER : AI_FORMATION_FOLLOW_THE_LEADER_TIGHT;
        if (ai->follow_in_formation(target, type) == 0xFFFFFFFF)
        {
            return AI_RESULT_FAILURE;
        }
        ai->action_timer.set(1000);
    }
    ai_combat_fire_at_target_if_available(np, false, true);
    return AI_RESULT_SUCCESS;
}

// 0x006B6FE0 (:1613)
ai_results move_with_human_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const unsigned int formation = ai->get_formation_handle();
    if (formation == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    const object* leader = ai_formation_get_leader(formation);
    if (get_defended_object(ai) != leader)
    {
        return AI_RESULT_FAILURE;
    }
    // Near a player every navpoint counts as enabled, so a follower does not lag behind at a closed link. 225 is 15 m.
    if (object_is_player(leader) && vec_dist_squared(np->pos, leader->pos) < 225.0f)
    {
        navpoint_set_treat_all_as_enabled(true);
    }
    if (!ai_do_follow_frame(np, formation, HST_STAND))
    {
        return AI_RESULT_FAILURE;
    }
    navpoint_set_treat_all_as_enabled(false);
    ai_combat_fire_at_target_if_available(np, false, true);
    if (!ai->action_timer.elapsed())
    {
        return AI_RESULT_IN_PROGRESS;
    }
    ai->action_timer.set(3000);
    leader = ai_formation_get_leader(formation);
    if (!object_is_player(leader))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    // More than 20 m (400) behind, or a path that leads that far from the player: teleport behind the camera if out of view.
    bool far_away = vec_dist_squared(np->pos, leader->pos) > 400.0f;
    if (!far_away)
    {
        const path_node* node = np->pf.path;
        while (node != nullptr)
        {
            if (vec_dist_squared(node->pos, leader->pos) > 400.0f)
            {
                far_away = true;
                break;
            }
            node = node->next;
            if (node == np->pf.path)
            {
                break;
            }
        }
    }
    if (far_away && np->is_offscreen())
    {
        const vector camera_dir = Rfg_camera.real_orient.fvec.copy_flatten_normalize_safe();
        vector desired;
        desired.x = Rfg_camera.real_pos.x - camera_dir.x * 5.0f;
        desired.y = Rfg_camera.real_pos.y - camera_dir.y * 5.0f;
        desired.z = Rfg_camera.real_pos.z - camera_dir.z * 5.0f;
        vector dest = desired;
        if (move_with_human_teleport_destination(np->get_handle(), desired, 25.0f, dest))
        {
            dest.y += 0.5f;
            if (game_render_cull_sphere(dest, 1.25f))
            {
                matrix orient;
                orient.make_uvec(camera_dir, UP_VECTOR);
                human_teleport(np, dest, orient, 6.0f, true);
            }
        }
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x006BD270 (:1693)
ai_results move_with_human_action::cleanup(npc* np, const ai_action* next_action) const
{
    ai_system* ai = np->get_ai();
    if (next_action != this)
    {
        ai->leave_formation();
        ai->set_desired_burst_size(0);
        navpoint_set_treat_all_as_enabled(false);
    }
    return AI_RESULT_SUCCESS;
}

// 0x00631DA0 (:1711)
int drive_to_human_rule::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const object* defended = get_defended_object(np->get_ai());
    if (defended == nullptr || defended->get_obj_type() != OT_HUMAN || human_in_vehicle(static_cast<const human*>(defended)) != nullptr)
    {
        return -1;
    }
    // Far enough to take a vehicle: 20 m when already driving, otherwise 30 m.
    const float min_dist_sq = human_is_driving(np) ? 400.0f : 900.0f;
    return min_dist_sq <= vec_dist_squared(np->pos, defended->pos) ? 500 : -1;
}

// 0x005E93E0 (:1741)
int move_with_human_unarmed_rule::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 10000;
}

// The singletons, in rfg.exe's construction order: they fill action_pool, and that order breaks ties in the planner.
follow_leader_action follow_leader_action::the_instance; // 0x01D68040

// 0x0062FD30 (.h:21)
follow_leader_action::follow_leader_action()
    : ai_action("follow leader", ai_plan_conditions_from_bits({4, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({39}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* follow_leader_action::get_instance()
{
    return &the_instance;
}

// 0x005E9200
follow_leader_action::~follow_leader_action()
{
}

// 0x00630030 (.h:21)
int follow_leader_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

watch_leader_action watch_leader_action::the_instance; // 0x01D6829C

// 0x00630060 (.h:22)
watch_leader_action::watch_leader_action()
    : ai_action("watch leader", ai_plan_conditions(), ai_plan_conditions_from_bits({39}))
{
    flags.has_enter = 1;
}

ai_action* watch_leader_action::get_instance()
{
    return &the_instance;
}

// 0x005E9210
watch_leader_action::~watch_leader_action()
{
}

// 0x006300B0 (.h:22)
int watch_leader_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

passenger_of_squadmate_action passenger_of_squadmate_action::the_instance; // 0x01DC95BC

// 0x006300F0 (.h:24)
passenger_of_squadmate_action::passenger_of_squadmate_action()
    : ai_action("follow squadmate vehicle", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({39, 54}))
{
    flags.has_enter = 1;
}

ai_action* passenger_of_squadmate_action::get_instance()
{
    return &the_instance;
}

// 0x005E9220
passenger_of_squadmate_action::~passenger_of_squadmate_action()
{
}

// 0x006301B0 (.h:24)
int passenger_of_squadmate_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

passenger_action passenger_action::the_instance; // 0x01D74FD8

// 0x006301E0 (.h:25)
passenger_action::passenger_action()
    : ai_action("passenger", ai_plan_conditions(), ai_plan_conditions_from_bits({52}))
{
    flags.has_enter = 1;
}

ai_action* passenger_action::get_instance()
{
    return &the_instance;
}

// 0x005E9230
passenger_action::~passenger_action()
{
}

// 0x00630230 (.h:25)
int passenger_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

wait_for_squad_action wait_for_squad_action::the_instance; // 0x01DA05D4

// 0x0062F580 (.h:15)
wait_for_squad_action::wait_for_squad_action()
    : ai_action("wait for squad", ai_plan_conditions_from_bits({0, 2, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({40}))
{
    flags.has_enter = 1;
}

ai_action* wait_for_squad_action::get_instance()
{
    return &the_instance;
}

// 0x005E91B0
wait_for_squad_action::~wait_for_squad_action()
{
}

// 0x0062F8D0 (.h:15)
int wait_for_squad_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

wait_for_squad_vehicle_action wait_for_squad_vehicle_action::the_instance; // 0x01D72100

// 0x0062F910 (.h:16)
wait_for_squad_vehicle_action::wait_for_squad_vehicle_action()
    : ai_action("wait for squad vehicle", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({40}))
{
    flags.has_enter = 1;
}

ai_action* wait_for_squad_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005E91C0
wait_for_squad_vehicle_action::~wait_for_squad_vehicle_action()
{
}

// 0x0062F970 (.h:16)
int wait_for_squad_vehicle_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

wait_for_bodyguards_action wait_for_bodyguards_action::the_instance; // 0x01DA6144

// 0x0062F9B0 (.h:17)
wait_for_bodyguards_action::wait_for_bodyguards_action()
    : ai_action("wait for bodyguards", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({150}))
{
    flags.has_enter = 1;
}

ai_action* wait_for_bodyguards_action::get_instance()
{
    return &the_instance;
}

// 0x005E91D0
wait_for_bodyguards_action::~wait_for_bodyguards_action()
{
}

// 0x0062FC50 (.h:17)
int wait_for_bodyguards_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

wait_bodyguards_vehicle_action wait_bodyguards_vehicle_action::the_instance; // 0x01D75800

// 0x0062FC90 (.h:18)
wait_bodyguards_vehicle_action::wait_bodyguards_vehicle_action()
    : ai_action("wait bodyguards vehicle", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({150}))
{
    flags.has_enter = 1;
}

ai_action* wait_bodyguards_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005E91E0
wait_bodyguards_vehicle_action::~wait_bodyguards_vehicle_action()
{
}

// 0x0062FCF0 (.h:18)
int wait_bodyguards_vehicle_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

move_with_human_action move_with_human_action::the_instance; // 0x01DC9E18

// 0x006309C0 (.h:30)
move_with_human_action::move_with_human_action()
    : ai_action("move with human", ai_plan_conditions_from_bits({9, 35, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({33}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* move_with_human_action::get_instance()
{
    return &the_instance;
}

// 0x005E9260
move_with_human_action::~move_with_human_action()
{
}

// 0x00630CC0 (.h:30)
int move_with_human_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

ride_with_human_action ride_with_human_action::the_instance; // 0x01D9F898

// 0x00630D00 (.h:31)
ride_with_human_action::ride_with_human_action()
    : ai_action("ride with human", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({33}))
{
    flags.has_enter = 1;
}

ai_action* ride_with_human_action::get_instance()
{
    return &the_instance;
}

// 0x005E9270
ride_with_human_action::~ride_with_human_action()
{
}

// 0x00630D60 (.h:31)
int ride_with_human_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

convoy_action convoy_action::the_instance; // 0x01DDB1C4

// 0x00630F30 (.h:35)
convoy_action::convoy_action()
    : ai_action("convoy", ai_plan_conditions_from_bits({9, 49, 58, 136, 137}), ai_plan_conditions_from_bits({148}))
{
    flags.has_enter = 1;
}

ai_action* convoy_action::get_instance()
{
    return &the_instance;
}

// 0x005E92A0
convoy_action::~convoy_action()
{
}

// 0x006310E0 (.h:35)
int convoy_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

leave_convoy_action leave_convoy_action::the_instance; // 0x01DCA67C

// 0x00631110 (.h:36)
leave_convoy_action::leave_convoy_action()
    : ai_action("leave convoy", ai_plan_conditions(), ai_plan_conditions_from_bits({153}))
{
}

ai_action* leave_convoy_action::get_instance()
{
    return &the_instance;
}

// 0x005E92B0
leave_convoy_action::~leave_convoy_action()
{
}

// 0x00631160 (.h:36)
int leave_convoy_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

defend_human_rule defend_human_rule::the_instance; // 0x01DCA608

// 0x00630D90 (.h:32)
defend_human_rule::defend_human_rule()
    : ai_production_rule("defend human rule", ai_plan_conditions_from_bits({0, 2}), ai_plan_conditions_from_bits({35}))
{
}

ai_action* defend_human_rule::get_instance()
{
    return &the_instance;
}

// 0x005E9280
defend_human_rule::~defend_human_rule()
{
}

move_with_human_unarmed_rule move_with_human_unarmed_rule::the_instance; // 0x01DA0500

// 0x00630E90 (.h:33)
move_with_human_unarmed_rule::move_with_human_unarmed_rule()
    : ai_production_rule("/w human unarmed rule", ai_plan_conditions(), ai_plan_conditions_from_bits({35}))
{
}

ai_action* move_with_human_unarmed_rule::get_instance()
{
    return &the_instance;
}

// 0x005E9290
move_with_human_unarmed_rule::~move_with_human_unarmed_rule()
{
}

// 0x00630F00 (.h:33)
int move_with_human_unarmed_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

drive_to_human_rule drive_to_human_rule::the_instance; // 0x01D726A8

// 0x006311A0 (.h:41)
drive_to_human_rule::drive_to_human_rule()
    : ai_production_rule("drive to human", ai_plan_conditions_from_bits({50}), ai_plan_conditions_from_bits({33}))
{
}

ai_action* drive_to_human_rule::get_instance()
{
    return &the_instance;
}

// 0x005E92C0
drive_to_human_rule::~drive_to_human_rule()
{
}

// 0x00631220 (.h:41)
int drive_to_human_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

escort_vehicle_action escort_vehicle_action::the_instance; // 0x01DC9624

// 0x00630270 (.h:26)
escort_vehicle_action::escort_vehicle_action()
    : ai_action("escort vehicle", ai_plan_conditions_from_bits({0, 2, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({33, 149}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* escort_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005E9240
escort_vehicle_action::~escort_vehicle_action()
{
}

// 0x006305F0 (.h:26)
int escort_vehicle_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

escort_simulated_vehicle_action escort_simulated_vehicle_action::the_instance; // 0x01DCA420

// 0x00630630 (.h:27)
escort_simulated_vehicle_action::escort_simulated_vehicle_action()
    : ai_action("escort simulated vehicle", ai_plan_conditions_from_bits({0, 2, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({149}))
{
    flags.has_enter = 1;
}

ai_action* escort_simulated_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005E9250
escort_simulated_vehicle_action::~escort_simulated_vehicle_action()
{
}

// 0x00630980 (.h:27)
int escort_simulated_vehicle_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

follow_escort_simulated_rule follow_escort_simulated_rule::the_instance; // 0x01DA0424

// 0x00631250 (.h:42)
follow_escort_simulated_rule::follow_escort_simulated_rule()
    : ai_production_rule("follow simulated rule", ai_plan_conditions_from_bits({39}), ai_plan_conditions_from_bits({149}))
{
}

ai_action* follow_escort_simulated_rule::get_instance()
{
    return &the_instance;
}

// 0x005E92D0
follow_escort_simulated_rule::~follow_escort_simulated_rule()
{
}

// 0x006312C0 (.h:42)
int follow_escort_simulated_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

