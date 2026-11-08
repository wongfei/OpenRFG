// Original: volition/rfg/code/ai/misc_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/misc_actions.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/rfg/code/ai/ai_building_logic.h"
#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/ai/reaction_actions.h"
#include "volition/rfg/code/ai/search_actions.h"
#include "volition/rfg/code/ai/self_preservation_goals.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/activities/activity_raid.h"
#include "volition/rfg/code/geom/character.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <math.h>

// action_pool fills in construction order and the A* action loop breaks ties by it. The section numbers follow rfg.exe's
// .CRT$XC table: position 25 of 33 blocks of AI translation units (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG25")

bool Passive_ai; // 0x01D67DB4, .data 0

// 0x01D67F64, its dynamic initializer ors these bits together.
extern bool Navpoint_treat_all_as_enabled; // object_navpoint.cpp

static const ai_plan_conditions SCRAMBLE_TO_FEET_GOAL_CONDITIONS = ai_plan_conditions_from_bits({116, 117, 118, 119, 120, 122});

// nav_type values of navpoints read here (rfg.exe tests the numbers).
static const signed char NAVPOINT_TYPE_BUILDING = 10;
static const signed char NAVPOINT_TYPE_TRANSITION_IN = 11;
static const signed char NAVPOINT_TYPE_TRANSITION_OUT = 12;
static const signed char NAVPOINT_TYPE_13 = 13;
static const signed char NAVPOINT_TYPE_17 = 17;

// 0x005EBC40 (:139)
static rfg_mover* get_plan_building(const ai_planner_search_node* rest_of_plan)
{
    for (; rest_of_plan != nullptr; rest_of_plan = rest_of_plan->next_node)
    {
        if (rest_of_plan->action_data.building_target != nullptr)
        {
            return rest_of_plan->action_data.building_target;
        }
    }
    return nullptr;
}

// 0x0063C520 (:153)
static navpoint* get_plan_transition_navpoint(const ai_planner_search_node* rest_of_plan)
{
    for (; rest_of_plan != nullptr; rest_of_plan = rest_of_plan->next_node)
    {
        navpoint* npp = (navpoint*)rest_of_plan->action_data.use_target;
        if (npp == nullptr || npp->get_obj_type() != OT_NAVPOINT)
        {
            continue;
        }
        if (npp->type == NAVPOINT_TYPE_TRANSITION_IN || npp->type == NAVPOINT_TYPE_TRANSITION_OUT)
        {
            return npp;
        }
        if (npp->type == NAVPOINT_TYPE_BUILDING || npp->type == NAVPOINT_TYPE_17 || npp->type == NAVPOINT_TYPE_13)
        {
            for (int i = 0; i < npp->num_links; i++)
            {
                navpoint* link = npp->links[i];
                if (link != nullptr && (Navpoint_treat_all_as_enabled || (npp->link_disabled_by[i] == 0 && link->heap_data.disabled_by == 0)))
                {
                    if (link->type == NAVPOINT_TYPE_TRANSITION_IN || link->type == NAVPOINT_TYPE_TRANSITION_OUT)
                    {
                        return link;
                    }
                }
            }
        }
    }
    return nullptr;
}

// 0x005EBC60 (:186). LTCG: np ESI, current_nav_cell EDI.
static bool get_move_out_of_way_pos_navcell(vector& goal_pos, uint64_t& nav_cell_hint, const npc* np, nav_cell* current_nav_cell)
{
    const vector move_dir = np->avoidance_move_dir;
    const vector hint_dir = np->avoidance_hint_dir;

    nav_cell_longest_los_output output;
    nav_cell_longest_los_input input;
    input.flags.use_disabled_nav_cells = 0;
    input.flags.use_light_nav_cells = 0;
    input.flags.check_start_in_detour_hull = 0;
    input.max_fan_angle = 0.0f;
    input.start_pos = nav_cell_force_point_to_cell(current_nav_cell, np->pos);
    input.start_nav_cell = current_nav_cell;
    input.delta_2d.x = hint_dir.x * 2.0f;
    input.delta_2d.y = hint_dir.z * 2.0f;
    input.num_fan_angle_increments = 0;
    input.max_visited_nav_cells = 0;

    // Along the hint direction, against it, then along the direction the other human wants to move in.
    nav_cell_find_longest_line_of_sight_point(output, input);
    if (output.result != NAV_CELL_LONGEST_LOS_RESULT_REACHED_DESTINATION)
    {
        input.delta_2d.x = input.delta_2d.x * -1.0f;
        input.delta_2d.y = input.delta_2d.y * -1.0f;
        nav_cell_find_longest_line_of_sight_point(output, input);
        if (output.result != NAV_CELL_LONGEST_LOS_RESULT_REACHED_DESTINATION)
        {
            input.delta_2d.x = move_dir.x * 2.0f;
            input.delta_2d.y = move_dir.z * 2.0f;
            nav_cell_find_longest_line_of_sight_point(output, input);
            if (output.result != NAV_CELL_LONGEST_LOS_RESULT_REACHED_DESTINATION)
            {
                return false;
            }
        }
    }
    goal_pos.y = np->pos.y;
    goal_pos.x = output.end_pos.x;
    goal_pos.z = output.end_pos.z;
    nav_cell_hint = nav_cell_get_handle(output.end_nav_cell);
    return true;
}

// 0x005EBE60 (:250). LTCG: np EAX.
static bool get_move_out_of_way_pos_navpoint(vector& goal_pos, const npc* np, const navpoint* closest_navpoint)
{
    const vector pnt = np->pos;
    const vector move_dir = np->avoidance_move_dir;
    const vector hint_dir = np->avoidance_hint_dir;
    const vector& nav_pos = closest_navpoint->pos;
    vector to_nav;
    to_nav.x = nav_pos.x - pnt.x;
    to_nav.y = nav_pos.y - pnt.y;
    to_nav.z = nav_pos.z - pnt.z;
    const float xz_dist = sqrtf(to_nav.x * to_nav.x + to_nav.z * to_nav.z);
    const float dist_to_edge = xz_dist - closest_navpoint->radius;

    if (dist_to_edge > 0.0f && closest_navpoint->num_links > 0)
    {
        // Outside the navpoint's radius: the nearest link segment that a human walks along.
        const navpoint* best_link = nullptr;
        vector best_point = pnt;
        float best_dist = dist_to_edge;
        for (int i = 0; i < closest_navpoint->num_links; i++)
        {
            const navpoint* link = closest_navpoint->links[i];
            if (link == nullptr || (link->type != NAVPOINT_TYPE_BUILDING && link->type != NAVPOINT_TYPE_17 && link->type != NAVPOINT_TYPE_TRANSITION_IN))
            {
                continue;
            }
            const float radius = closest_navpoint->radius - link->radius >= 0.0f ? link->radius : closest_navpoint->radius;
            vector closest;
            ix_closest_point_on_line_segment(closest, pnt, nav_pos, link->pos);
            const float dist = vec_xz_dist(closest, pnt);
            if (dist - radius < best_dist)
            {
                best_point = closest;
                best_dist = dist - radius;
                best_link = link;
            }
        }
        if (best_link != nullptr)
        {
            const float radius = closest_navpoint->radius - best_link->radius >= 0.0f ? best_link->radius : closest_navpoint->radius;
            if (radius > 0.5f)
            {
                vector to_point;
                to_point.x = best_point.x - pnt.x;
                to_point.y = best_point.y - pnt.y;
                to_point.z = best_point.z - pnt.z;
                const float dist = radius * 0.8f + to_point.mag_and_normalize_safe();
                goal_pos.x = to_point.x * dist + pnt.x;
                goal_pos.y = to_point.y * dist + pnt.y;
                goal_pos.z = to_point.z * dist + pnt.z;
                return true;
            }
            goal_pos = move_dir.y * to_nav.y + move_dir.x * to_nav.x + move_dir.z * to_nav.z <= 0.0f ? best_link->pos : nav_pos;
            return true;
        }
    }

    if (dist_to_edge >= 0.0f)
    {
        goal_pos = nav_pos;
        return true;
    }

    const float nav_radius = closest_navpoint->radius;
    if (nav_radius <= 0.5f)
    {
        // Too small to step aside in: the link most across the direction of travel. Original: no null check of the links.
        const navpoint* best_link = nullptr;
        float best_dot = 1.0f;
        for (int i = 0; i < closest_navpoint->num_links; i++)
        {
            const navpoint* link = closest_navpoint->links[i];
            vector dir;
            dir.y = link->pos.y - nav_pos.y;
            dir.z = link->pos.z - nav_pos.z;
            dir.x = link->pos.x - nav_pos.x;
            dir.normalize_safe();
            const float dot = dir.x * move_dir.x + dir.y * move_dir.y + dir.z * move_dir.z;
            if (dot > -0.707f && fabsf(dot) < best_dot)
            {
                best_link = link;
                best_dot = fabsf(dot);
            }
        }
        if (best_link == nullptr)
        {
            return false;
        }
        goal_pos = best_link->pos;
        return true;
    }

    if (xz_dist <= nav_radius * 0.5f)
    {
        const float dist = nav_radius * 0.8f;
        goal_pos.x = dist * hint_dir.x + nav_pos.x;
        goal_pos.y = hint_dir.y * dist + nav_pos.y;
        goal_pos.z = hint_dir.z * dist + nav_pos.z;
        return true;
    }
    const float move_dot = move_dir.y * to_nav.y + move_dir.x * to_nav.x + move_dir.z * to_nav.z;
    if (move_dot < 0.70710677f && move_dot > -0.70710677f) // cos 45 degrees
    {
        goal_pos = nav_pos;
        return true;
    }
    const float sign = hint_dir.y * to_nav.y + hint_dir.x * to_nav.x + hint_dir.z * to_nav.z <= 0.0f ? -2.0f : 2.0f;
    goal_pos.x = pnt.x + hint_dir.x * sign;
    goal_pos.y = pnt.y + hint_dir.y * sign;
    goal_pos.z = pnt.z + hint_dir.z * sign;
    return true;
}

// 0x0067FF20 (:411). LTCG: np EAX.
static bool get_move_out_of_way_pos(vector& goal_pos, uint64_t& nav_cell_hint, npc* np)
{
    goal_pos = np->pos;
    nav_cell_hint = 0xFFFFFFFFFFFFFFFFull;
    if (np->update_avoidance())
    {
        return false;
    }
    nav_cell* ncp = nullptr;
    navpoint* npp = nullptr;
    if (!np->get_ai()->perception.get_nearest_navpoint_or_navcell(ncp, npp, nullptr, nullptr))
    {
        return false;
    }
    if (ncp != nullptr)
    {
        return get_move_out_of_way_pos_navcell(goal_pos, nav_cell_hint, np, ncp);
    }
    return get_move_out_of_way_pos_navpoint(goal_pos, np, npp);
}

// 0x005EC670 (:443)
static navpoint* ai_get_linked_non_transition_navpoint(const navpoint* npp)
{
    for (int i = 0; i < npp->num_links; i++)
    {
        navpoint* link = npp->links[i];
        if (link != nullptr && (Navpoint_treat_all_as_enabled || (npp->link_disabled_by[i] == 0 && link->heap_data.disabled_by == 0)))
        {
            if (link->type == NAVPOINT_TYPE_17 || link->type == NAVPOINT_TYPE_BUILDING)
            {
                return link;
            }
        }
    }
    return nullptr;
}

// 0x005EC6D0 (:458)
int idle_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 200;
}

// 0x00663110 (:463)
ai_results idle_action::enter(npc* np, const ai_action* next_action) const
{
    ai_system* ai = np->get_ai();
    if (next_action != this)
    {
        ai->perception.set_attention_mode(ATTENTION_IDLE, Passive_ai ? ATTENTION_STEERING_PLANT_FEET : ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        ai->stop_moving();
        human_set_stance(np, HST_STAND);
        ai->action_timer.set(10000);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005EC6E0 (:489)
ai_results idle_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        np->get_ai()->stop_moving();
        np->look_at_speed = 1.0f;
    }
    return AI_RESULT_SUCCESS;
}

// 0x005EC710 (:499)
ai_results idle_action::do_action(npc* np) const
{
    if (np->npc_flags.allow_idle_despawn == 0 && np->get_ai()->action_timer.elapsed())
    {
        np->npc_flags.allow_idle_despawn = 1;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x0067FFC0 (:517)
int idle_vehicle_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return -1;
    }
    if (vp->is_killed() || human_team_is_non_combatant(np))
    {
        return 100000;
    }
    if (np->vehicle_seat_idx != VSI_DRIVER && !ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_PASSENGER))
    {
        return 100000;
    }
    return 100;
}

// 0x005EC760 (:546)
ai_results idle_vehicle_action::enter(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        np->get_ai()->action_timer.set(10000);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005EC790 (:558)
ai_results idle_vehicle_action::cleanup(npc*, const ai_action*) const
{
    return AI_RESULT_SUCCESS;
}

// 0x0063C5D0 (:563)
ai_results idle_vehicle_action::do_action(npc* np) const
{
    if (np->npc_flags.allow_idle_despawn == 0 && np->get_ai()->action_timer.elapsed())
    {
        np->npc_flags.allow_idle_despawn = 1;
    }
    if (human_is_driving(np) && np->vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(np->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && objp->get_sub_type() == OT_SUB_VEHICLE_AUTO)
        {
            automobile* ap = static_cast<automobile*>(objp);
            automobile_clear_accelerator(ap);
            automobile_set_handbrake(ap, true);
            automobile_set_steering(ap->get_handle(), 0.0f);
            automobile_set_brake_pedal_input(ap->get_handle(), 1.0f);
        }
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005EC7A0 (:588)
int idle_sam_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->h_info->flags.guerrilla_tech ? 50 : -1; // (-flag & 0x33) - 1
}

// 0x00663250 (:598)
ai_results idle_sam_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        ai->perception.set_attention_mode(ATTENTION_IDLE, Passive_ai ? ATTENTION_STEERING_PLANT_FEET : ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        human_override_default_animation_state(np, true);
        np->char_inst->set_state(index_alias(ANIMATION_STATE_SAM_STAND, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
        ai->stop_moving();
        human_set_stance(np, HST_STAND);
        ai->action_timer.set_rand(15000, 25000);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005EC7D0 (:624)
ai_results idle_sam_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (ai->action_timer.elapsed())
    {
        if (!np->char_inst->is_playing_action(ANIM_ALIAS_IDLE))
        {
            const int anim = int_rand_range(ANIMATION_ACTION_SAM_STAND_IDLE_B, ANIMATION_ACTION_SAM_STAND_IDLE_C);
            np->char_inst->play_action((animation_action)anim, ANIM_ALIAS_IDLE, 1.0f, 0);
        }
        ai->action_timer.set_rand(15000, 25000);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005EC860 (:637)
ai_results idle_sam_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        if (np->char_inst->is_playing_action(ANIM_ALIAS_IDLE))
        {
            np->char_inst->stop_action(ANIM_ALIAS_IDLE, 0, false);
        }
        np->get_ai()->stop_moving();
        np->look_at_speed = 1.0f;
        human_override_default_animation_state(np, false);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005EC8D0 (:654)
int get_up_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->cur_stance != HST_PRONE ? -1 : 100; // (-(cur_stance != PRONE) & 0xFFFFFF9B) + 100
}

// 0x006633D0 (:659)
ai_results get_up_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    ai->stop_moving();
    ai->perception.set_attention_mode(ATTENTION_USE_CURRENT_MODE, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_USE_CURRENT_MODE);

    const float speed = fl_rand_range(0.85f, 1.15f);
    human_override_default_animation_state(np, false);
    vector pos;
    matrix orient;
    animlib_get_tag_world_coords(np->char_inst->aim_handle, np->h_info->lower_spine_bone_index, np->pos, np->orient, pos, &orient);
    // On the back or on the front, from the lower spine's direction.
    const animation_action anim = orient.fvec.y < 0.0f ? ANIMATION_ACTION_FW_GETUP : ANIMATION_ACTION_BK_GETUP;
    np->char_inst->play_action(anim, ANIM_ALIAS_GET_UP, speed, 0x4040);
    human_set_stance(np, HST_STAND);
    if (human_weapons_is_firearm_equipped(np) && ai->perception.get_belief(AI_BELIEF_ENEMY_EXISTS) != nullptr)
    {
        human_set_combat_ready(np, true, 0);
    }
    ai->planner.lock_action(true);
    return AI_RESULT_SUCCESS;
}

// 0x005EC8F0 (:690)
ai_results get_up_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (human_weapons_is_firearm_equipped(np) && ai->perception.get_belief(AI_BELIEF_ENEMY_EXISTS) != nullptr)
    {
        human_set_combat_ready(np, true, 0);
    }
    return np->char_inst->is_playing_action(ANIM_ALIAS_GET_UP) ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x0063C680 (:709)
int scramble_to_feet_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    const ai_action* current = np->get_ai()->planner.get_current_action();
    if (np->cur_stance == HST_PRONE || current == this)
    {
        if (goal_setup_data.goal->get_conditions() & SCRAMBLE_TO_FEET_GOAL_CONDITIONS)
        {
            return 50;
        }
        if (current == this)
        {
            return 50;
        }
    }
    return -1;
}

// 0x00663990 (:725)
ai_results scramble_to_feet_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action == this)
    {
        if (np->char_inst->is_playing_action(ANIMATION_ACTION_SCRAMBLE_TO_FEET))
        {
            const ai_goal* goal = ai->planner.get_current_goal();
            if (goal == nullptr || !goal->pursues_condition(SCRAMBLE_TO_FEET_GOAL_CONDITIONS))
            {
                ai->stop_moving();
                human_set_stance(np, HST_CROUCH);
                np->char_inst->stop_action(ANIMATION_ACTION_SCRAMBLE_TO_FEET, 10, true);
                ai->action_timer.set(400);
            }
        }
        return AI_RESULT_SUCCESS;
    }
    ai->stop_moving();
    ai->perception.set_attention_mode(ATTENTION_USE_CURRENT_MODE, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_USE_CURRENT_MODE);

    const float speed = fl_rand_range(0.85f, 1.15f);
    human_override_default_animation_state(np, false);
    np->char_inst->play_action(ANIMATION_ACTION_SCRAMBLE_TO_FEET, ANIM_ALIAS_GET_UP, speed, 0x4040);
    ai->set_direct_movement_fast(np->orient.fvec.copy_flatten_normalize_safe(), HMS_SPRINT, HST_STAND, -1.0f);
    ai->action_timer.invalidate();
    return AI_RESULT_SUCCESS;
}

// 0x005EC940 (:756)
ai_results scramble_to_feet_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (ai->action_timer.valid())
    {
        return ai->action_timer.elapsed() ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
    }
    if (!np->char_inst->is_playing_action(ANIM_ALIAS_GET_UP))
    {
        return AI_RESULT_SUCCESS;
    }
    return vanim_has_trigger_just_elapsed(np->char_inst->vp, Trigger_unlock_ai, ANIM_ALIAS_GET_UP, -1) ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

// 0x005EC9D0 (:782)
int go_prone_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 200;
}

// 0x00663F70 (:787)
ai_results go_prone_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    ai->action_timer.invalidate();
    ai->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    ai->stop_moving();
    human_set_stance(np, HST_PRONE);
    ai->planner.lock_action(true);
    return AI_RESULT_SUCCESS;
}

// 0x005EC9E0 (:807)
ai_results go_prone_action::do_action(npc* np) const
{
    timestamp& timer = np->get_ai()->action_timer;
    if (timer.valid())
    {
        return timer.elapsed() ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
    }
    if (!np->char_inst->is_playing_action(ANIMATION_ACTION_STAND_TO_PRONE) && !np->char_inst->is_playing_action(ANIMATION_ACTION_KNEEL_GO_PRONE))
    {
        timer.set(200);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x00663FF0 (:834)
int enter_building_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node* next_action) const
{
    navpoint* transition = get_plan_transition_navpoint(next_action);
    rfg_mover* building = get_plan_building(next_action);
    if (building == nullptr)
    {
        return -1;
    }
    if (transition == nullptr || !ai_building_object_belongs_to_building(transition, building))
    {
        transition = ai_building_find_closest_transition_from_outside(np->pos, building, nullptr);
    }
    if (transition == nullptr)
    {
        return -1;
    }
    navpoint* target = ai_get_linked_non_transition_navpoint(transition);
    if (target == nullptr)
    {
        return -1;
    }
    my_data.use_target = target;
    my_data.building_target = building;
    return 200;
}

// 0x005ECA60 (:870)
void enter_building_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->desired_enter_building_transition = setup_data.use_target->get_handle();
    ai->desired_destination_object = setup_data.building_target->get_handle();
}

// 0x006A4DD0 (:879)
ai_results enter_building_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    if (ai->desired_enter_building_transition == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    const object* transition = World.object_from_handle(ai->desired_enter_building_transition);
    if (transition == nullptr || transition->get_obj_type() != OT_NAVPOINT)
    {
        return AI_RESULT_FAILURE;
    }
    if (ai->planner.get_current_goal() == search_goal::get_instance())
    {
        human_set_combat_ready(np, true, 0);
    }
    const ai_pathfind_results result = ai->pathfind_to(transition->pos, HMS_WALK, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false);
    ai->action_timer.set_rand(500, 750);
    if (result == AI_PF_SUCCESS)
    {
        return AI_RESULT_SUCCESS;
    }
    return result > AI_PF_RAGDOLLED ? AI_RESULT_FAILURE : AI_RESULT_IN_PROGRESS;
}

// 0x00680030 (:898)
ai_results enter_building_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (ai->planner.get_current_goal() == search_goal::get_instance())
    {
        human_set_combat_ready(np, true, 0);
    }
    if (ai->action_timer.elapsed() || np->movement_mode != HMM_PATHFINDING)
    {
        if (ai->desired_destination_object == 0xFFFFFFFF)
        {
            return AI_RESULT_FAILURE;
        }
        const object* structure = World.object_from_handle(ai->desired_destination_object);
        if (structure == nullptr || structure->get_obj_type() != OT_MOVER || structure->get_sub_type() != OT_SUB_MOVER_RFG)
        {
            return AI_RESULT_FAILURE;
        }
        rfg_mover* building = nullptr;
        if (ai->perception.is_in_closest_building(np, &building, 0.0f, 1.25f) && building == structure)
        {
            if (np->movement_mode != HMM_PATHFINDING)
            {
                return AI_RESULT_SUCCESS;
            }
            const navpoint* closest = ai_building_find_closest_navpoint(np->pos, static_cast<const rfg_mover*>(structure), true, nullptr);
            if (closest != nullptr && closest->type == NAVPOINT_TYPE_BUILDING)
            {
                return AI_RESULT_SUCCESS;
            }
        }
        ai->action_timer.set_rand(500, 750);
    }
    return np->movement_mode != HMM_PATHFINDING ? AI_RESULT_FAILURE : AI_RESULT_IN_PROGRESS;
}

// 0x00680140 (:960)
int leave_building_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    const ai_goal* goal = goal_setup_data.goal;
    if (goal == flee_to_safety_goal::get_instance() || goal == flee_enemies_goal::get_instance() || goal == react_goal::get_instance())
    {
        return -1;
    }
    const ai_perception& perception = np->get_ai()->perception;
    nav_cell* ncp;
    navpoint* npp;
    if (!perception.get_nearest_navpoint_or_navcell(ncp, npp, nullptr, nullptr))
    {
        return -1;
    }
    if (ncp != nullptr)
    {
        return 0;
    }
    rfg_mover* structure = nullptr;
    if (!perception.is_in_closest_building(np, &structure, 0.0f, 1.25f))
    {
        return -1;
    }
    if (!perception.building_is_safe(structure, false) && !activity_raid_is_in_demolition_area(np->pos))
    {
        return -1;
    }
    return ai_building_find_closest_transition_from_inside(np->pos, structure, false) != nullptr ? 500 : -1; // (-x & 0x1F5) - 1
}

// 0x00680220 (:1010)
ai_results leave_building_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    ai_system* ai = np->get_ai();
    rfg_mover* building = nullptr;
    if (!ai->perception.is_in_closest_building(np, &building, 0.0f, 1.25f) || building == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    nav_cell* ncp;
    navpoint* npp;
    if (!ai->perception.get_nearest_navpoint_or_navcell(ncp, npp, nullptr, nullptr))
    {
        return AI_RESULT_FAILURE;
    }
    if (ncp != nullptr)
    {
        nav_cell_get_centroid(&ai->desired_destination, ncp);
        ai->action_substate = HMS_RUN; // 2: action_substate holds the movement state do_action paths with
        return AI_RESULT_SUCCESS;
    }
    const navpoint* exit_point = rfg_find_best_exit_point(building, npp, false, np->current_team == HUMAN_TEAM_EDF);
    if (exit_point == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const nav_cell* exit_cell = bb_pf_get_nav_cell(exit_point->pos, false, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
    if (exit_cell == nullptr || (exit_cell->flags & NAV_CELL_FLAG_DISABLED) != 0)
    {
        ai->desired_destination = exit_point->pos;
    }
    else
    {
        nav_cell_get_centroid(&ai->desired_destination, exit_cell);
    }
    // Walk, or run out of a building about to be demolished.
    ai->action_substate = activity_raid_is_in_demolition_area(np->pos) + HMS_WALK;
    ai->action_timer.set_rand(500, 750);
    return AI_RESULT_SUCCESS;
}

// 0x006A4E70 (:1072)
ai_results leave_building_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (ai->action_timer.elapsed())
    {
        const nav_cell* ncp = nav_cell_from_handle(np->pf.last_visited_nav_cell_handle);
        if (ncp != nullptr && (ncp->flags & NAV_CELL_FLAG_DISABLED) == 0 && nav_cell_is_point_in_cell_column(ncp, np->pos))
        {
            float height;
            if (havok_terrain_height(height, np->pos.x, np->pos.z, false, nullptr) && np->pos.y <= height + 1.25f)
            {
                return AI_RESULT_SUCCESS;
            }
        }
        ai->action_timer.set_rand(500, 750);
    }
    const ai_pathfind_results result = ai->pathfind_to(ai->desired_destination, (human_movement_states)ai->action_substate, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false);
    if (result != AI_PF_SUCCESS && result > AI_PF_RAGDOLLED)
    {
        return AI_RESULT_FAILURE;
    }
    ai_combat_fire_at_target_if_available(np, false, false);
    return AI_RESULT_IN_PROGRESS;
}

// 0x005ECA90 (:1105)
int move_out_of_way_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 10;
}

// 0x00680370 (:1110)
ai_results move_out_of_way_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        if (!get_move_out_of_way_pos(ai->desired_goal_pos, ai->desired_goal_nav_cell_hint_handle, np))
        {
            np->end_avoidance();
            return AI_RESULT_FAILURE;
        }
        ai->action_timer.set(0);
        np->hflags.disable_all_path_smoothing = 1;
    }
    return AI_RESULT_SUCCESS;
}

// 0x006A4F90 (:1131)
ai_results move_out_of_way_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (ai->action_timer.time_since() > 3000)
    {
        np->end_avoidance();
        return AI_RESULT_SUCCESS;
    }
    if (vec_dist_squared(ai->desired_goal_pos, np->pos) < 0.09f)
    {
        np->end_avoidance();
        return AI_RESULT_SUCCESS;
    }
    ai_results result = AI_RESULT_IN_PROGRESS;
    if (!ai->is_pathing_to(ai->desired_goal_pos, 0.25f, 1.0f))
    {
        // Keeps up with a player squad leader who is ready for a fight.
        const player* leader = ai->is_in_player_squad(true);
        const bool hurry = leader != nullptr && (human_is_combat_ready(leader) || leader->cur_state == HS_COVER);
        human_stances stance = HST_CURRENT;
        if (!human_is_combat_ready(np) || human_weapons_get_weapon(np) == nullptr)
        {
            stance = HST_STAND;
        }
        const ai_pathfind_results pf = ai->pathfind_to(ai->desired_goal_pos, hurry ? HMS_RUN : HMS_WALK, stance, AI_PATHFIND_PRIORITY_NORMAL, ai->desired_goal_nav_cell_hint_handle, true);
        result = pf == AI_PF_SUCCESS || pf <= AI_PF_RAGDOLLED ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
    }

    ai_combat_action_parameters params;
    params.init_npc(np);
    if (np->combat.combat_ready_timestamp.valid() && !np->combat.combat_ready_timestamp.elapsed() && ai->perception.get_attention_mode() == ATTENTION_AIM_AT_OBJECT &&
        params.init_target(false) && params.weapon_inst != nullptr && params.weapon_inst->is_loaded() && params.weapon_inst->w_info->is_firearm() &&
        params.weapon_inst->get_range_category(params.dist_to_target) != WEAPON_OUT_OF_RANGE)
    {
        human_set_combat_ready(np, true, 0);
        if (ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr && ai->perception.can_hit_target())
        {
            ai_combat_fire_at_target(params, true);
        }
    }
    if (result != AI_RESULT_IN_PROGRESS)
    {
        np->end_avoidance();
    }
    return result;
}

// 0x005ECAA0 (:1191)
ai_results move_out_of_way_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        np->hflags.disable_all_path_smoothing = 0;
    }
    return AI_RESULT_SUCCESS;
}

// 0x005ECAC0 (:1203)
int salute_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 100;
}

// 0x0063C750 (:1208)
ai_results salute_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        np->char_inst->play_action(ANIMATION_ACTION_SALUTE, ANIM_ALIAS_NONE, 1.0f, 0x20);
        ai->planner.lock_action(true);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005ECAD0 (:1220)
ai_results salute_action::do_action(npc* np) const
{
    return np->char_inst->is_playing_action(ANIMATION_ACTION_SALUTE) ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x0063C7B0 (:1228)
ai_results salute_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this && np->char_inst->rdd.is_loaded())
    {
        np->char_inst->stop_action(ANIMATION_ACTION_SALUTE, 5, true);
    }
    return AI_RESULT_SUCCESS;
}

// The singletons, in rfg.exe's construction order: they fill action_pool, and that order breaks ties in the planner.
idle_action idle_action::the_instance; // 0x01D725A4

// 0x00614240 (.h:12)
idle_action::idle_action()
    : ai_action("idle", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({67, 76}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* idle_action::get_instance()
{
    return &the_instance;
}

// 0x005E1830
idle_action::~idle_action()
{
}

// 0x00614520 (.h:12)
int idle_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

idle_vehicle_action idle_vehicle_action::the_instance; // 0x01D9F670

// 0x00614560 (.h:13)
idle_vehicle_action::idle_vehicle_action()
    : ai_action("idle_vehicle", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({67, 76}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* idle_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005E1840
idle_vehicle_action::~idle_vehicle_action()
{
}

// 0x00614620 (.h:13)
int idle_vehicle_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

idle_sam_action idle_sam_action::the_instance; // 0x01D72418

// 0x00614660 (.h:14)
idle_sam_action::idle_sam_action()
    : ai_action("sam idle", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({67, 76}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* idle_sam_action::get_instance()
{
    return &the_instance;
}

// 0x005E1850
idle_sam_action::~idle_sam_action()
{
}

// 0x00614940 (.h:14)
int idle_sam_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

get_up_action get_up_action::the_instance; // 0x01DCA2E0

// 0x00614980 (.h:15)
get_up_action::get_up_action()
    : ai_action("get up", ai_plan_conditions_from_bits({47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({9}))
{
    flags.has_enter = 1;
}

ai_action* get_up_action::get_instance()
{
    return &the_instance;
}

// 0x005E1860
get_up_action::~get_up_action()
{
}

// 0x00614BB0 (.h:15)
int get_up_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

go_prone_action go_prone_action::the_instance; // 0x01D68198

// 0x00614BF0 (.h:16)
go_prone_action::go_prone_action()
    : ai_action("go prone", ai_plan_conditions_from_bits({47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({8}))
{
    flags.has_enter = 1;
}

ai_action* go_prone_action::get_instance()
{
    return &the_instance;
}

// 0x005E1870
go_prone_action::~go_prone_action()
{
}

// 0x00614E20 (.h:16)
int go_prone_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

scramble_to_feet_action scramble_to_feet_action::the_instance; // 0x01D756F4

// 0x00614E60 (.h:17)
scramble_to_feet_action::scramble_to_feet_action()
    : ai_action("scramble to feet", ai_plan_conditions_from_bits({47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({9}))
{
    flags.has_enter = 1;
}

ai_action* scramble_to_feet_action::get_instance()
{
    return &the_instance;
}

// 0x005E1890
scramble_to_feet_action::~scramble_to_feet_action()
{
}

// 0x00615090 (.h:17)
int scramble_to_feet_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

enter_building_action enter_building_action::the_instance; // 0x01D67FB8

// 0x006150D0 (.h:22)
enter_building_action::enter_building_action()
    : ai_action("enter building", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({13}))
{
    flags.has_enter = 1;
}

ai_action* enter_building_action::get_instance()
{
    return &the_instance;
}

// 0x005E18A0
enter_building_action::~enter_building_action()
{
}

// 0x00615370 (.h:22)
int enter_building_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

leave_building_action leave_building_action::the_instance; // 0x01D9F808

// 0x006153A0 (.h:23)
leave_building_action::leave_building_action()
    : ai_action("leave building", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({14}))
{
    flags.has_enter = 1;
}

ai_action* leave_building_action::get_instance()
{
    return &the_instance;
}

// 0x005E18B0
leave_building_action::~leave_building_action()
{
}

// 0x00615640 (.h:23)
int leave_building_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

move_out_of_way_action move_out_of_way_action::the_instance; // 0x01DCA3B8

// 0x00615680 (.h:25)
move_out_of_way_action::move_out_of_way_action()
    : ai_action("move", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({75}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* move_out_of_way_action::get_instance()
{
    return &the_instance;
}

// 0x005E18C0
move_out_of_way_action::~move_out_of_way_action()
{
}

// 0x00615920 (.h:25)
int move_out_of_way_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

salute_action salute_action::the_instance; // 0x01DCA6D8

// 0x00615960 (.h:27)
salute_action::salute_action()
    : ai_action("salute", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({77}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* salute_action::get_instance()
{
    return &the_instance;
}

// 0x005E18D0
salute_action::~salute_action()
{
}

// 0x00615C00 (.h:27)
int salute_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

