// Original: volition/rfg/code/ai/combat/acquire_visual_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/combat/acquire_visual_actions.h"

#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/combat/combat_cover.h"
#include "volition/rfg/code/ai/combat/cover_selection.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_cover.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/objects/object_guard_node.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"

#include <math.h>

// Position 7 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG07")

// Read from rfg.exe's .data.
static const vector GUARD_NODE_COLLECT_BMIN(-12.5f, -1.0f, 0.0f); // 0x016425B8
static const vector GUARD_NODE_COLLECT_BMAX(12.5f, 5.0f, 0.0f); // 0x016425C4

// How far a search for cover goes (a tweak table item, zero until tweak_table.xtbl loads).
static float Ai_cover_dist_acquire_visual; // 0x01D65738
static tweak_table_item Tweak_table_item_Ai_cover_dist_acquire_visual("Ai_cover_dist_acquire_visual", &Ai_cover_dist_acquire_visual); // 0x01D9F88C

// Added when a new cover node has to be gone to (go to cover, then the conditions of being in it).
static ai_plan_conditions cover_move_conditions()
{
    return ai_plan_conditions(59) | ai_plan_conditions(60) | ai_plan_conditions(62);
}

// 0x006167E0 (:128). LTCG: gnp EAX, np and target_pos on the stack.
static bool guard_node_can_be_used_for_acquire(const object_guard_node* gnp, const npc* np, const vector& target_pos)
{
    if (!gnp->can_reserve(np->get_handle()))
    {
        return false;
    }
    vector to_target = target_pos - gnp->pos;
    if (-to_target.y < 1.0f) // the node is at least 1 m above the target
    {
        return false;
    }
    to_target.flatten_normalize_safe();
    const float limit = gnp->orient.rvec * to_target < 0.0f ? gnp->get_left_angle_dot() : gnp->get_right_angle_dot();
    return limit <= gnp->orient.fvec * to_target && !np->get_ai()->outside_defensive_perimeter(gnp->pos, false, nullptr);
}

// 0x00616B30 (:171). LTCG: np ECX, target_pos EAX, gnp and npp on the stack.
static bool guard_node_acquire_same_set(const object_guard_node* gnp, const npc* np, const vector& target_pos, const navpoint* npp)
{
    if (!guard_node_can_be_used_for_acquire(gnp, np, target_pos))
    {
        return false;
    }
    navpoint* closest = nullptr;
    float dist_sq;
    return bb_pf_find_closest_navpoint(gnp->pos, closest, dist_sq) && closest->set == npp->set;
}

// 0x00616B90 (:250)
int go_to_believed_location_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const ai_system* ai = np->get_ai();
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed())
    {
        return -1;
    }
    if (target->vehicle_handle != 0xFFFFFFFF)
    {
        const object* objp = World.object_from_handle(target->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && objp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
        {
            return -1;
        }
    }
    const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(target));
    return belief != nullptr && !ai->outside_defensive_perimeter(belief->pos, false, nullptr) ? 1600 : -1;
}

// 0x00675670 (:282)
ai_results go_to_believed_location_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    const human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    ai->perception.pay_attention(target->get_handle(), AI_BELIEF_ENEMY_POSITION, ATTENTION_ACQUIRE_VISUAL, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    if (last_action != this)
    {
        ai->action_timer.invalidate();
    }
    return AI_RESULT_SUCCESS;
}

// 0x006B5E50 (:308)
ai_results go_to_believed_location_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    np->combat.combat_maneuver_timestamp.set(3000); // ms
    human_set_combat_ready(np, true, 0);
    if (ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr)
    {
        return AI_RESULT_SUCCESS;
    }
    ai_belief* belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(target));
    if (belief == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const float dist_sq = vec_dist_squared(np->pos, belief->pos);
    const human_movement_states cur_state = np->cur_movement_state;
    if (dist_sq < 1.0f) // 1 m: arrived and nothing to see, wait a little and give up
    {
        if (cur_state != HMS_IDLE || !ai->action_timer.valid())
        {
            ai->stop_moving();
            ai->action_timer.set_rand(3000, 5000);
        }
        if (!ai->action_timer.valid() || !ai->action_timer.elapsed())
        {
            return AI_RESULT_IN_PROGRESS;
        }
        ai->perception.deprecate_belief(belief);
        ai->perception.deprecate_belief(AI_BELIEF_LAST_CONTACT, static_cast<const object*>(target));
        return AI_RESULT_FAILURE;
    }
    human_movement_states movement_state = HMS_RUN;
    if (dist_sq <= 400.0f) // 20 m
    {
        if (dist_sq < 56.25f) // 7.5 m
        {
            movement_state = HMS_WALK;
        }
        else if (cur_state != HMS_IDLE)
        {
            movement_state = cur_state;
        }
        else if ((sqrtf(dist_sq) - 7.5f) / 12.5f < fl_rand())
        {
            movement_state = HMS_WALK; // from certain at 7.5 m to never at 20 m
        }
    }
    if (ai->is_pathing_to(belief->pos, 0.25f, 1.0f))
    {
        if (cur_state != movement_state)
        {
            human_set_state(np, HS_CURRENT, movement_state, -1.0f);
        }
        ai->action_timer.invalidate();
        return AI_RESULT_IN_PROGRESS;
    }
    const ai_results result = ai_pf_to_result(ai->pathfind_to(target, movement_state, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, &belief->pos));
    if (result == AI_RESULT_FAILURE)
    {
        ai->planner.avoid_action(this, 7000); // ms
        return AI_RESULT_FAILURE;
    }
    if (result == AI_RESULT_SUCCESS)
    {
        ai->action_timer.invalidate();
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x00656D90 (:423)
int go_to_ally_with_los_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const ai_system* ai = np->get_ai();
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed())
    {
        return -1;
    }
    const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(target));
    if (belief == nullptr || (belief->flags & BF_LAST_POSITION_VISIBLE) != 0)
    {
        return -1;
    }
    const int num = World.get_num_objects<npc>();
    for (int i = 0; i < num; i++)
    {
        npc* ally = World.get_object<npc>(i);
        if (ally == np || ally == nullptr || ally->is_killed() || human_teams_get_disposition(np, ally) != TEAM_DISPOSITION_ALLIED || np->combat.target_handle != ally->combat.target_handle)
        {
            continue;
        }
        if (vec_dist_squared(np->pos, ally->pos) > 1600.0f || ai->outside_defensive_perimeter(ally->pos, false, nullptr)) // 40 m
        {
            continue;
        }
        if (ally->get_ai()->perception.get_belief(AI_BELIEF_FIRST_CONTACT, ally->combat.target_handle) != nullptr)
        {
            my_data.use_target = ally;
            return 2500;
        }
    }
    return -1;
}

// 0x005E22F0 (:473)
void go_to_ally_with_los_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    if (setup_data.use_target != nullptr)
    {
        np->get_ai()->desired_follow_target = setup_data.use_target->get_handle();
    }
}

// 0x006917A0 (:481)
ai_results go_to_ally_with_los_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    np->combat.combat_maneuver_timestamp.set(5000); // ms
    const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(target));
    if (belief == nullptr || (belief->flags & BF_LAST_POSITION_VISIBLE) != 0 || ai->desired_follow_target == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    const object* objp = World.object_from_handle(ai->desired_follow_target);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return AI_RESULT_FAILURE;
    }
    const npc* ally = static_cast<const npc*>(objp);
    human_set_combat_ready(np, true, 0);
    if (last_action != this)
    {
        ai->action_timer.invalidate();
        ai->perception.pay_attention(belief, ATTENTION_AIM_AT_BELIEF, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    }
    if (human_combat_cover_is_using_node(ally))
    {
        const cover_node* node = human_combat_cover_get_node(ally);
        const cover_node_stances stance = cover_stance_from_state(ally);
        // Original bug kept: the offset is added to the old action_pos, not to the node or the ally.
        if (stance == COVER_STANCE_STAND_LEFT || stance == COVER_STANCE_CROUCH_LEFT)
        {
            ai->action_pos -= node->orient.rvec * 1.5f;
        }
        else if (stance == COVER_STANCE_STAND_RIGHT || stance == COVER_STANCE_CROUCH_RIGHT)
        {
            ai->action_pos += node->orient.rvec * 1.5f;
        }
        return AI_RESULT_SUCCESS;
    }
    // 1.5 m behind the ally, away from where the enemy is believed to be.
    vector to_belief = belief->pos - ally->pos;
    to_belief.flatten_normalize_safe();
    const vector dest = ally->pos - to_belief * 1.5f;
    nav_cell* cell = nullptr;
    navpoint* ally_npp = nullptr;
    if (ally->get_ai()->perception.get_nearest_navpoint_or_navcell(cell, ally_npp, nullptr, nullptr))
    {
        if (cell == nullptr)
        {
            navpoint* closest = nullptr;
            float dist_sq;
            if (bb_pf_find_closest_navpoint(dest, closest, dist_sq) && closest->set == ally_npp->set && dist_sq <= 1.0f)
            {
                const float dist = sqrtf(dist_sq);
                const float along = dist - closest->radius < 0.0f ? dist : closest->radius;
                ai->action_pos = closest->pos + (dest - closest->pos).copy_flatten_normalize_safe() * along;
                return AI_RESULT_SUCCESS;
            }
        }
        else
        {
            nav_cell* found = nullptr;
            if (bb_pf_get_nav_cell_from_hint(&ai->action_pos, cell, dest, false, nullptr, false, true, NAV_CELL_TRAVERSAL_ENABLED) != nullptr || bb_pf_find_closest_point_on_nav_mesh(dest, ai->action_pos, found, false))
            {
                return AI_RESULT_SUCCESS;
            }
        }
    }
    ai->planner.avoid_action(this, 5000); // ms
    return AI_RESULT_FAILURE;
}

// 0x00691D50 (:564)
ai_results go_to_ally_with_los_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    ai_belief* belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(target));
    if (belief == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr)
    {
        return AI_RESULT_SUCCESS;
    }
    bool give_up_check = vec_dist_squared(np->pos, ai->action_pos) < 0.11111112f; // a third of a metre
    int wait_min = 3000; // ms
    int wait_max = 5000;
    if (!give_up_check && !ai->is_pathing_to(ai->action_pos, 0.25f, 1.0f))
    {
        const ai_results result = ai_pf_to_result(ai->pathfind_to(ai->action_pos, HMS_RUN, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false));
        if (result == AI_RESULT_FAILURE)
        {
            give_up_check = true;
            wait_min = 2000;
            wait_max = 4000;
        }
        else if (result != AI_RESULT_SUCCESS)
        {
            return AI_RESULT_IN_PROGRESS;
        }
    }
    if (!give_up_check)
    {
        ai->action_timer.invalidate();
        return AI_RESULT_IN_PROGRESS;
    }
    if (ai->action_timer.valid() && ai->action_timer.elapsed())
    {
        ai->perception.deprecate_belief(belief);
        ai->perception.deprecate_belief(AI_BELIEF_LAST_CONTACT, static_cast<const object*>(target));
        return AI_RESULT_FAILURE;
    }
    if (!ai->action_timer.valid())
    {
        ai->action_timer.set_rand(wait_min, wait_max);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x006C1EB0 (:662)
int cover_last_location_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const ai_system* ai = np->get_ai();
    if (human_in_vehicle(np) != nullptr || (np->hflags.riot_shield != 0 && np->shield_handle != 0xFFFFFFFF))
    {
        return -1;
    }
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed())
    {
        return -1;
    }
    const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(target));
    if (belief == nullptr || (belief->flags & BF_LAST_POSITION_VISIBLE) != 0)
    {
        return -1;
    }
    cover_node* node = human_combat_cover_get_node(np);
    vector target_pos;
    if (!ai->perception.get_effective_target_pos(target_pos, target, nullptr, nullptr, false))
    {
        return -1;
    }
    if (node == nullptr || node->check_cover_type(np, target_pos, false, true, true, nullptr) != CCR_OFFENSIVE || node->flags.crouch)
    {
        const vehicle* vp = World.object_from_handle<vehicle>(target->vehicle_handle);
        if (vp != nullptr && vp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
        {
            return -1;
        }
        // 1.9 m: head height at the believed position.
        cover_search_parameters params(np, belief->pos + UP_VECTOR * 1.9f, Ai_cover_dist_acquire_visual, false, node, true);
        node = cover_selection_do_search(params);
        if (node != nullptr)
        {
            desired_conditions |= cover_move_conditions();
        }
    }
    if (node == nullptr)
    {
        return -1;
    }
    const cover_node_stances stance = cover_pick_stance(node, belief->pos);
    if (stance == COVER_STANCE_UNDEFINED)
    {
        node->disable(COVER_DISABLED_NO_STANCES, 12000); // ms
        return -1;
    }
    const cover_node_firing_positions firing_position = cover_pick_firing_position(stance, node, belief->pos);
    if (firing_position == COVER_FIRE_UNDEFINED)
    {
        node->disable(COVER_DISABLED_NO_STEPOUTS, 12000); // ms
        return -1;
    }
    my_data.cover_firing_position = firing_position;
    my_data.use_target = node;
    my_data.cover_stance = stance;
    return 500;
}

// 0x005E2320 (:743)
void cover_last_location_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    human_combat_cover_reserve_node(np, static_cast<cover_node*>(setup_data.use_target));
    ai->desired_cover_stance = setup_data.cover_stance;
    ai->desired_cover_firing_position = setup_data.cover_firing_position;
}

// 0x006756E0 (:751)
ai_results cover_last_location_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    const human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    ai->perception.pay_attention(target->get_handle(), AI_BELIEF_ENEMY_POSITION, ATTENTION_AIM_AT_BELIEF, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    if (last_action != this)
    {
        ai->action_timer.set_rand(3000, 5000);
    }
    return AI_RESULT_SUCCESS;
}

// 0x00675750 (:773)
ai_results cover_last_location_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed() || ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(target)) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    cover_node* node = human_combat_cover_get_node(np);
    vector target_pos;
    if (!ai->perception.get_effective_target_pos(target_pos, target, nullptr, nullptr, false) || node == nullptr || node->check_cover_type(np, target_pos, false, false, true, nullptr) != CCR_OFFENSIVE)
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    if (ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr)
    {
        ai->planner.request_replan();
        return AI_RESULT_IN_PROGRESS;
    }
    if (!ai->action_timer.elapsed())
    {
        return AI_RESULT_IN_PROGRESS;
    }
    human_combat_cover_abandon_node(np);
    node->disable(COVER_DISABLED_TEMPORARY, 12000); // ms
    return AI_RESULT_FAILURE;
}

// 0x005E2360 (:826)
int guard_last_location_rule::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 4000;
}

// 0x00691F70 (:831)
int guard_last_location_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const ai_system* ai = np->get_ai();
    const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, np->combat.target_handle);
    if (belief == nullptr)
    {
        return -1;
    }
    nav_cell* cell;
    navpoint* npp = nullptr;
    float xz_dist_sq;
    if (!ai->perception.get_nearest_navpoint_or_navcell(cell, npp, nullptr, &xz_dist_sq) || npp == nullptr)
    {
        return -1;
    }
    object_guard_node* cur_node = ai->get_guard_node();
    if (cur_node != nullptr && guard_node_acquire_same_set(cur_node, np, belief->pos, npp))
    {
        my_data.use_target = cur_node;
        if (!ai->is_at_guard_post())
        {
            desired_conditions |= ai_plan_conditions(36);
        }
        return 1;
    }
    // A node in a box along the way to the enemy, for an npc 1 m or more above it and between 10 and 50 m away.
    const float height = np->pos.y - belief->pos.y;
    const float dist_sq = vec_xz_dist_squared(np->pos, belief->pos);
    if (height < 1.0f || dist_sq < 100.0f || dist_sq > 2500.0f)
    {
        return -1;
    }
    const float dist = sqrtf(dist_sq);
    const float inverse = 1.0f / dist;
    const vector dir(inverse * (belief->pos.x - np->pos.x), inverse * 0.0f, inverse * (belief->pos.z - np->pos.z));
    matrix orient;
    orient.make_quick(dir);
    const vector center(belief->pos.x, np->pos.y, belief->pos.z);
    const vector bmin(GUARD_NODE_COLLECT_BMIN.x, GUARD_NODE_COLLECT_BMIN.y, -dist);
    unsigned int handles[25];
    const int num = World.collect_objects<object_guard_node>(bmin, GUARD_NODE_COLLECT_BMAX, center, orient, handles, 25, nullptr, nullptr);
    for (int i = 0; i < num; i++)
    {
        object* objp = handles[i] != 0xFFFFFFFF ? World.object_from_handle(handles[i]) : nullptr;
        if (objp != nullptr && objp->get_obj_type() == OT_GUARD_NODE && objp != cur_node && guard_node_acquire_same_set(static_cast<const object_guard_node*>(objp), np, belief->pos, npp))
        {
            my_data.use_target = objp;
            desired_conditions |= ai_plan_conditions(36);
            return 1;
        }
    }
    return -1;
}

// 0x00616E00 (:902)
void guard_last_location_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    object* objp = setup_data.use_target;
    if (objp != nullptr && objp->get_obj_type() == OT_GUARD_NODE)
    {
        np->get_ai()->reserve_guard_node(static_cast<object_guard_node*>(objp), -128); // -128: no direction
    }
}

// Inlined in enter and do_action: the npc still holds its guard node.
static bool still_holds_guard_node(npc* np)
{
    ai_system* ai = np->get_ai();
    object_guard_node* node = ai->get_guard_node();
    return node != nullptr && node->is_reserved(np->get_handle()) && ai->reserve_guard_node(node, -128);
}

// 0x00692270 (:911)
ai_results guard_last_location_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (!still_holds_guard_node(np))
    {
        return AI_RESULT_FAILURE;
    }
    if (ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr)
    {
        return AI_RESULT_SUCCESS;
    }
    const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, np->combat.target_handle);
    if (belief == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai->perception.pay_attention(belief, ATTENTION_ACQUIRE_VISUAL, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    if (last_action != this)
    {
        ai->stop_moving();
        human_set_stance(np, HST_STAND);
    }
    human_set_combat_ready(np, true, 0);
    np->steering_vector = belief->pos - np->pos;
    np->steering_vector.flatten_normalize_safe();
    return AI_RESULT_SUCCESS;
}

// 0x00657100 (:947)
ai_results guard_last_location_action::do_action(npc* np) const
{
    const ai_system* ai = np->get_ai();
    if (!still_holds_guard_node(np))
    {
        return AI_RESULT_FAILURE;
    }
    if (ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr)
    {
        return AI_RESULT_SUCCESS;
    }
    if (ai->perception.get_attention_belief() == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    return AI_RESULT_IN_PROGRESS;
}

go_to_believed_location_action go_to_believed_location_action::the_instance; // 0x01DA6074

// 0x005FEBE0 (.h:14)
go_to_believed_location_action::go_to_believed_location_action()
    : ai_action("go to believed location", ai_plan_conditions_from_bits({4, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({23}))
{
    flags.has_enter = 1;
}

ai_action* go_to_believed_location_action::get_instance()
{
    return &the_instance;
}

// 0x005DD890
go_to_believed_location_action::~go_to_believed_location_action()
{
}

// 0x005FEEE0 (.h:14)
int go_to_believed_location_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

go_to_ally_with_los_action go_to_ally_with_los_action::the_instance; // 0x01DDADAC

// 0x005FEF20 (.h:15)
go_to_ally_with_los_action::go_to_ally_with_los_action()
    : ai_action("go to ally with LOS", ai_plan_conditions_from_bits({4, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({23}))
{
    flags.has_enter = 1;
}

ai_action* go_to_ally_with_los_action::get_instance()
{
    return &the_instance;
}

// 0x005DD8A0
go_to_ally_with_los_action::~go_to_ally_with_los_action()
{
}

// 0x005FF220 (.h:15)
int go_to_ally_with_los_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

cover_last_location_action cover_last_location_action::the_instance; // 0x01DCA56C

// 0x005FF250 (.h:16)
cover_last_location_action::cover_last_location_action()
    : ai_action("cover last location", ai_plan_conditions_from_bits({0, 2, 62, 136, 137, 153}), ai_plan_conditions_from_bits({23}))
{
    flags.has_enter = 1;
}

ai_action* cover_last_location_action::get_instance()
{
    return &the_instance;
}

// 0x005DD8B0
cover_last_location_action::~cover_last_location_action()
{
}

// 0x005FF480 (.h:16)
int cover_last_location_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

guard_last_location_action guard_last_location_action::the_instance; // 0x01DC9E9C

// 0x005FF4B0 (.h:19)
guard_last_location_action::guard_last_location_action()
    : ai_action("guard last location", ai_plan_conditions_from_bits({0, 2, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({27}))
{
    flags.has_enter = 1;
}

ai_action* guard_last_location_action::get_instance()
{
    return &the_instance;
}

// 0x005DD8C0
guard_last_location_action::~guard_last_location_action()
{
}

// 0x005FF800 (.h:19)
int guard_last_location_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

guard_last_location_rule guard_last_location_rule::the_instance; // 0x01D680F4

// 0x005FF830 (.h:20)
guard_last_location_rule::guard_last_location_rule()
    : ai_production_rule("guard last loc. rule", ai_plan_conditions_from_bits({27}), ai_plan_conditions_from_bits({23}))
{
}

ai_action* guard_last_location_rule::get_instance()
{
    return &the_instance;
}

// 0x005DD8D0
guard_last_location_rule::~guard_last_location_rule()
{
}

// 0x005FF8B0 (.h:20)
int guard_last_location_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

