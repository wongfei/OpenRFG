// Original: volition/rfg/code/ai/combat/dodge_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/combat/dodge_actions.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/combat/combat_maneuver.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/ai/self_preservation_goals.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"

#include <math.h>

// Position 16 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG16")

static const float DODGE_FACING_COS = 0.8660254f; // cos(30 degrees)

// 30 degrees: dynamic initializers in rfg.exe.
static const float COS_RUNNING_DODGE_ANGLE = (float)cos((double)0.5235988f); // 0x01D9F8D8
static const float SIN_RUNNING_DODGE_ANGLE = (float)sin((double)0.5235988f); // 0x01DA0620

static timestamp Marauder_spin_deflect_timeout; // 0x016426F0

// 0x005E7780 (:118). LTCG: np EDI, which_way on the stack.
static animation_action get_sideways_dodge_anim(const npc* np, dodge_directions which_way)
{
    if ((unsigned int)which_way > DODGE_RIGHT)
    {
        return ANIMATION_ACTION_NONE;
    }
    int base;
    if (human_teams_get_disposition(np, HUMAN_TEAM_EDF) == TEAM_DISPOSITION_ALLIED)
    {
        // An EDF ally rolls unless standing, and on 34 of 100 standing dodges.
        base = np->cur_stance != HST_STAND || vrand() % 100 > 65 ? ANIMATION_ACTION_COMBAT_ROLL_LEFT : ANIMATION_ACTION_STAND_DODGE_LEFT;
    }
    else if (np->cur_stance != HST_STAND)
    {
        return ANIMATION_ACTION_NONE;
    }
    else
    {
        base = ANIMATION_ACTION_STAND_DODGE_LEFT;
    }
    return (animation_action)(base + which_way);
}

// 0x005E77E0 (:157). LTCG: np ECX, which_way EAX.
static animation_action get_running_dodge_anim(const npc* np, dodge_directions which_way)
{
    if ((unsigned int)which_way <= DODGE_RIGHT && np->current_team == HUMAN_TEAM_MARAUDER)
    {
        return (animation_action)(ANIMATION_ACTION_RUN_DODGE_LEFT + (which_way != DODGE_LEFT ? 1 : 0));
    }
    return ANIMATION_ACTION_NONE;
}

// Inlined in the two get_cost functions of the animated dodges: where the animation would take the npc.
static bool dodge_anim_endpoint_is_good(const npc* np, animation_action action)
{
    const v_animate* vp = np->char_inst->vp;
    const int handle = vanim_get_action_handle(vp, vp->default_group_index, action);
    if (handle == -1)
    {
        return false;
    }
    vector end_pos;
    animlib_get_total_translation(anim_get_anim_from_handle(handle)->animlib_handle, end_pos, -1, -1);
    vec_change_to_global_coords(end_pos, np->pos, np->orient);
    return combat_maneuver_evaluate_dodge_endpoint(np, end_pos, nullptr);
}

static ai_results finish_dodge_action(npc* np, bool finished)
{
    if (finished)
    {
        np->get_ai()->stance_change_timer.set_rand(1000, 1500);
        return AI_RESULT_SUCCESS;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x00679490 (:174)
int dodge_sideways_anim_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    if (np->current_team == HUMAN_TEAM_MARAUDER && np->cur_movement_state == HMS_RUN)
    {
        return -1;
    }
    if ((np->hflags.riot_shield != 0 && np->shield_handle != 0xFFFFFFFF) || !np->combat.dodge_timestamp.elapsed())
    {
        return -1;
    }
    object* target = goal_setup_data.selected_target;
    if (target == nullptr || target->get_obj_type() != OT_HUMAN)
    {
        return 0;
    }
    if (np->get_ai()->is_providing_covering_fire() != nullptr)
    {
        return -1;
    }
    vector to_target = target->pos - np->pos;
    to_target.flatten_normalize_safe();
    if (np->orient.fvec * to_target < DODGE_FACING_COS)
    {
        return -1;
    }
    vector to_position = goal_setup_data.position - np->pos;
    to_position.flatten_normalize_safe();
    my_data.action_anim = get_sideways_dodge_anim(np, to_position * np->orient.rvec <= 0.0f ? DODGE_RIGHT : DODGE_LEFT);
    if (my_data.action_anim == ANIMATION_ACTION_NONE || !dodge_anim_endpoint_is_good(np, my_data.action_anim))
    {
        return -1;
    }
    my_data.attack_target = target;
    return 400;
}

// 0x0069C220 (:254)
void dodge_sideways_anim_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    np->combat.dodge_timestamp.set_rand(2000, 4000);
    ai_system* ai = np->get_ai();
    ai->desired_anim_action = setup_data.action_anim;
    if (setup_data.attack_target != nullptr)
    {
        ai->perception.pay_attention(setup_data.attack_target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    }
}

// 0x0065BC90 (:265)
ai_results dodge_sideways_anim_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    ai->set_desired_burst_size(0);
    ai->stop_moving();
    human_set_combat_ready(np, true, 0);
    ai->perception.set_attention_mode(ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    np->char_inst->play_action(ai->desired_anim_action, ANIM_ALIAS_DODGE, 1.0f, 0x20);
    ai->planner.lock_action(true);
    np->combat.combat_maneuver_timestamp.set_rand(3000, 5000);
    return AI_RESULT_SUCCESS;
}

// 0x005E7800 (:292)
ai_results dodge_sideways_anim_action::do_action(npc* np) const
{
    human_set_combat_ready(np, true, 0);
    return finish_dodge_action(np, !np->char_inst->is_playing_action(ANIM_ALIAS_DODGE));
}

// 0x006796F0 (:308)
int dodge_sideways_noanim_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    if (np->cur_movement_state >= HMS_RUN || (np->hflags.riot_shield != 0 && np->shield_handle != 0xFFFFFFFF) || !np->combat.dodge_timestamp.elapsed())
    {
        return -1;
    }
    object* target = goal_setup_data.selected_target;
    if (target == nullptr || target->get_obj_type() != OT_HUMAN)
    {
        return 0;
    }
    if (np->get_ai()->is_providing_covering_fire() != nullptr)
    {
        return -1;
    }
    const vector to_target = target->pos - np->pos;
    const vector across = vector(to_target.z * UP_VECTOR.y - to_target.y * UP_VECTOR.z, UP_VECTOR.z * to_target.x - to_target.z * UP_VECTOR.x, to_target.y * UP_VECTOR.x - UP_VECTOR.y * to_target.x).copy_normalize_safe();
    const float steering_across = np->steering_vector * across;
    const float position_across = across * (goal_setup_data.position - np->pos);
    if (fabsf(position_across) > 0.7f && steering_across * position_across > 0.0f)
    {
        return -1;
    }
    const vector dir = np->cur_movement_state == HMS_IDLE ? across * -fl_sign(position_across) : across * fl_sign(steering_across);
    const vector dest = np->pos + dir * 3.0f; // m
    if (!combat_maneuver_evaluate_dodge_endpoint(np, dest, nullptr))
    {
        return -1;
    }
    my_data.attack_target = target;
    my_data.pos = dest;
    return 500;
}

// 0x0069C270 (:371)
void dodge_sideways_noanim_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    np->combat.dodge_timestamp.set_rand(2000, 4000);
    ai_system* ai = np->get_ai();
    if (setup_data.attack_target != nullptr)
    {
        ai->perception.pay_attention(setup_data.attack_target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    }
    ai->desired_destination = setup_data.pos;
}

// 0x0065BD70 (:380). The call's arguments are from the asm (0x0065BE43, 0x0065BE58): the decompile shifts them.
ai_results dodge_sideways_noanim_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    const vector dest = ai->desired_destination;
    uint64_t cell_handle;
    vector cell_pos;
    const nav_cell* cell = bb_pf_get_nav_cell(dest, false, &cell_handle, true, NAV_CELL_TRAVERSAL_ENABLED, &cell_pos);
    navpoint* nav = nullptr;
    float nav_dist_sq;
    bb_pf_find_closest_navpoint(dest, nav, nav_dist_sq);
    uint64_t hint = cell_handle;
    if (nav == nullptr)
    {
        if (cell == nullptr)
        {
            np->combat.combat_maneuver_timestamp.set_rand(5000, 8000);
            return AI_RESULT_FAILURE;
        }
    }
    else if (cell == nullptr || vec_dist_squared(dest, cell_pos) > nav_dist_sq)
    {
        // Original quirk kept: a navpoint's object handle is passed as the nav cell hint.
        hint = nav->get_handle();
    }
    if (ai->set_direct_movement_to_pos(ai->desired_destination, hint, HMS_RUN, HST_STAND, true) != AI_PF_SUCCESS)
    {
        np->combat.combat_maneuver_timestamp.set_rand(5000, 8000);
        return AI_RESULT_FAILURE;
    }
    np->combat.combat_maneuver_timestamp.set_rand(3000, 5000);
    ai->set_desired_burst_size(0);
    human_set_combat_ready(np, true, 0);
    ai->perception.set_attention_mode(ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    ai->planner.lock_action(true);
    ai->action_timer.set(1500); // ms
    return AI_RESULT_SUCCESS;
}

// 0x005E7860 (:426)
ai_results dodge_sideways_noanim_action::do_action(npc* np) const
{
    human_set_combat_ready(np, true, 0);
    return finish_dodge_action(np, np->get_ai()->action_timer.elapsed() || np->movement_mode != HMM_PATHFINDING);
}

// 0x00679AE0 (:443)
int dodge_run_anim_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    if (np->current_team != HUMAN_TEAM_MARAUDER || np->cur_movement_state <= HMS_WALK || !np->combat.dodge_timestamp.elapsed())
    {
        return -1;
    }
    object* target = goal_setup_data.selected_target;
    if (target == nullptr || target->get_obj_type() != OT_HUMAN)
    {
        return 0;
    }
    // Original quirk kept: the target's offset is not normalized, so this holds for any target past about 0.87 m ahead.
    const vector to_target = target->pos - np->pos;
    if (np->orient.fvec * to_target < DODGE_FACING_COS)
    {
        return -1;
    }
    const vector across(to_target.z * UP_VECTOR.y - UP_VECTOR.z * to_target.y, UP_VECTOR.z * to_target.x - to_target.z * UP_VECTOR.x, UP_VECTOR.x * to_target.y - UP_VECTOR.y * to_target.x);
    vector to_position = goal_setup_data.position - np->pos;
    to_position.flatten_normalize_safe();
    my_data.action_anim = get_running_dodge_anim(np, to_position * across <= 0.0f ? DODGE_RIGHT : DODGE_LEFT);
    if (my_data.action_anim == ANIMATION_ACTION_NONE || !dodge_anim_endpoint_is_good(np, my_data.action_anim))
    {
        return -1;
    }
    my_data.attack_target = target;
    return 200;
}

// 0x0069C2D0 (:505)
void dodge_run_anim_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    np->combat.dodge_timestamp.set_rand(2000, 4000);
    ai_system* ai = np->get_ai();
    ai->desired_anim_action = setup_data.action_anim;
    if (setup_data.attack_target != nullptr)
    {
        ai->perception.pay_attention(setup_data.attack_target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    }
}

// 0x0065BF40 (:516)
ai_results dodge_run_anim_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    ai->set_desired_burst_size(0);
    human_set_combat_ready(np, true, 0);
    ai->perception.set_attention_mode(ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    np->char_inst->play_action(ai->desired_anim_action, ANIM_ALIAS_DODGE, 1.0f, 0x20);
    ai->planner.lock_action(true);
    np->combat.combat_maneuver_timestamp.set_rand(3000, 5000);
    return AI_RESULT_SUCCESS;
}

// 0x005E78C0 (:540)
ai_results dodge_run_anim_action::do_action(npc* np) const
{
    human_set_combat_ready(np, true, 0);
    return finish_dodge_action(np, !np->char_inst->is_playing_action(ANIM_ALIAS_DODGE));
}

// 0x00679DD0 (:556)
int dodge_run_noanim_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    if (np->cur_movement_state != HMS_RUN)
    {
        return -1;
    }
    object* target = goal_setup_data.selected_target;
    if (target == nullptr || target->get_obj_type() != OT_HUMAN)
    {
        return 0;
    }
    vector to_target = target->pos - np->pos;
    const float target_dist = to_target.flatten_mag_and_normalize_safe();
    if (np->orient.fvec * to_target < DODGE_FACING_COS)
    {
        return -1;
    }
    const vector across(to_target.z * UP_VECTOR.y - to_target.y * UP_VECTOR.z, to_target.x * UP_VECTOR.z - to_target.z * UP_VECTOR.x, to_target.y * UP_VECTOR.x - to_target.x * UP_VECTOR.y);
    vector to_position = goal_setup_data.position - np->pos;
    to_position.flatten_normalize_safe();
    const float forward = COS_RUNNING_DODGE_ANGLE * 4.0f; // 4 m at the dodge angle
    if (target_dist * 0.8f < forward)
    {
        return -1;
    }
    const float side = SIN_RUNNING_DODGE_ANGLE * 4.0f;
    const vector ahead = np->pos + to_target * forward;
    const vector dest = to_position * across <= 0.0f ? ahead + across * side : ahead - across * side;
    if (!combat_maneuver_evaluate_dodge_endpoint(np, dest, nullptr))
    {
        return -1;
    }
    my_data.attack_target = target;
    my_data.pos = dest;
    return 300;
}

// 0x0069C320 (:610)
void dodge_run_noanim_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->action_pos = setup_data.pos;
    if (setup_data.attack_target != nullptr)
    {
        ai->perception.pay_attention(setup_data.attack_target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    }
}

// 0x0065C020 (:618)
ai_results dodge_run_noanim_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    human_set_combat_ready(np, true, 0);
    ai->set_direct_movement_fast((ai->action_pos - np->pos).copy_flatten_normalize_safe(), HMS_RUN, HST_STAND, np->move_speed);
    ai->perception.set_attention_mode(ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    ai->planner.lock_action(true);
    ai->action_timer.set(2000); // ms
    return AI_RESULT_SUCCESS;
}

// 0x005E7920 (:637)
ai_results dodge_run_noanim_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human_set_combat_ready(np, true, 0);
    vector to_pos = ai->action_pos - np->pos;
    const float dist = to_pos.flatten_mag_and_normalize_safe();
    if (!ai->action_timer.elapsed() && DODGE_FACING_COS <= np->steering_vector * to_pos && 0.25f <= dist)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    return finish_dodge_action(np, true);
}

// 0x0062B130 (:655)
int shield_cover_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    if (np->hflags.riot_shield == 0 || np->shield_handle == 0xFFFFFFFF)
    {
        return -1;
    }
    if (goal_setup_data.goal == dodge_goal::get_instance() && !np->combat.dodge_timestamp.elapsed())
    {
        return -1;
    }
    object* target = goal_setup_data.selected_target;
    if (target == nullptr || target->get_obj_type() != OT_HUMAN)
    {
        return -1;
    }
    my_data.attack_target = target;
    return 100;
}

// 0x0069C370 (:674)
void shield_cover_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    if (setup_data.attack_target != nullptr)
    {
        np->get_ai()->perception.pay_attention(setup_data.attack_target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    }
}

// 0x005E7A70 (:680)
ai_results shield_cover_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        np->get_ai()->stop_moving();
        human_set_stance(np, HST_CROUCH);
    }
    return AI_RESULT_SUCCESS;
}

// 0x0069C3A0 (:693)
ai_results shield_cover_action::do_action(npc* np) const
{
    const ai_system* ai = np->get_ai();
    const ai_goal* goal = ai->planner.get_current_goal();
    if (goal != nullptr && goal->pursues_condition(ai_plan_conditions(117)) && ai->perception.get_belief(AI_BELIEF_HAZARD, BELIEF_REQUIREMENTS_NONE, OT_ITEM) == nullptr)
    {
        return AI_RESULT_SUCCESS;
    }
    ai_combat_fire_at_target_if_available(np, false, false);
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E7AA0 (:707)
ai_results shield_cover_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        np->combat.dodge_timestamp.set_rand(2000, 4000);
    }
    return AI_RESULT_SUCCESS;
}

// 0x0062B190 (:718)
int spin_deflect_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    if (np->current_team != HUMAN_TEAM_MARAUDER)
    {
        return -1;
    }
    if (Marauder_spin_deflect_timeout.valid() && !Marauder_spin_deflect_timeout.elapsed())
    {
        return -1;
    }
    if (np->combat.dodge_timestamp.valid() && !np->combat.dodge_timestamp.elapsed())
    {
        return -1;
    }
    const weapon_info* w_info = human_weapons_get_weapon_info(np);
    if (w_info == nullptr || w_info->weapon_class != WEAPON_CLASS_GUTTER)
    {
        return -1;
    }
    if (goal_setup_data.goal == dodge_projectile_goal::get_instance())
    {
        // Only a projectile that does not explode can be deflected.
        const object* objp = goal_setup_data.use_target;
        if (objp == nullptr || objp->get_obj_type() != OT_ITEM || objp->get_sub_type() != OT_SUB_ITEM_PROJECTILE || static_cast<const projectile*>(objp)->w_info->is_explosive())
        {
            return -1;
        }
    }
    object* target = goal_setup_data.selected_target;
    if (target == nullptr || target->get_obj_type() != OT_HUMAN)
    {
        return 0;
    }
    if (np->orient.fvec * (target->pos - np->pos).copy_flatten_normalize_safe() < 0.70710677f) // cos(45 degrees)
    {
        return -1;
    }
    my_data.attack_target = target;
    my_data.action_anim = ANIMATION_ACTION_SPIN_DODGE;
    const ai_goal* goal = np->get_ai()->planner.get_current_goal();
    if (np->movement_mode == HMM_PATHFINDING && (goal == advance_goal::get_instance() || goal == melee_goal::get_instance()) && vec_dist_squared(np->pos, target->pos) >= 100.0f) // 10 m
    {
        my_data.action_anim = ANIMATION_ACTION_SPIN_DODGE_MOVING;
    }
    return 100;
}

// 0x0069C450 (:775)
void spin_deflect_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    Marauder_spin_deflect_timeout.set_rand(1000, 2000);
    np->combat.dodge_timestamp.set_rand(4000, 6000);
    if (setup_data.attack_target != nullptr)
    {
        ai->perception.pay_attention(setup_data.attack_target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    }
    ai->desired_anim_action = setup_data.action_anim;
}

// 0x0062B370 (:784)
ai_results spin_deflect_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    const object* target = ai->perception.get_attention_target();
    if (target == nullptr || target->get_obj_type() != OT_HUMAN)
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    const vector dir = (target->pos - np->pos).copy_flatten_normalize_safe();
    if (ai->desired_anim_action == ANIMATION_ACTION_SPIN_DODGE_MOVING)
    {
        np->char_inst->play_action(ANIMATION_ACTION_SPIN_DODGE_MOVING, ANIM_ALIAS_DODGE, 1.0f, 0x20);
    }
    else
    {
        ai->stop_moving();
        matrix dest_orient;
        dest_orient.make_quick(dir);
        np->char_inst->play_action(ai->desired_anim_action, ANIM_ALIAS_DODGE, 1.0f, 0x20);
        np->char_inst->set_action_starting_orient(np->pos, np->orient, np->pos, dest_orient, ANIM_ALIAS_DODGE, -1.0f);
    }
    ai->planner.lock_action(true);
    np->combat.combat_maneuver_timestamp.set_rand(3000, 5000);
    static item_info* VIRTUAL_SHIELD_INFO = object_item_get_info("gutter_col_dummy"); // 0x01DDBAF8
    if (VIRTUAL_SHIELD_INFO == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    return last_action == this || np->create_shield_item(VIRTUAL_SHIELD_INFO, false) ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x005E7AD0 (:834)
ai_results spin_deflect_action::do_action(npc* np) const
{
    human_set_combat_ready(np, true, 0);
    return finish_dodge_action(np, !np->char_inst->is_playing_action(ANIM_ALIAS_DODGE));
}

// 0x005E7B30 (:847)
ai_results spin_deflect_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        np->destroy_virtual_shield();
    }
    return AI_RESULT_SUCCESS;
}

dodge_sideways_anim_action dodge_sideways_anim_action::the_instance; // 0x01DDAE70

// 0x00629ED0 (.h:12)
dodge_sideways_anim_action::dodge_sideways_anim_action()
    : ai_action("dodge sideways anim", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({82, 117}))
{
    flags.has_enter = 1;
}

ai_action* dodge_sideways_anim_action::get_instance()
{
    return &the_instance;
}

// 0x005E7720
dodge_sideways_anim_action::~dodge_sideways_anim_action()
{
}

// 0x0062A1B0 (.h:12)
int dodge_sideways_anim_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

dodge_run_anim_action dodge_run_anim_action::the_instance; // 0x01D683DC

// 0x0062A1E0 (.h:13)
dodge_run_anim_action::dodge_run_anim_action()
    : ai_action("dodge run anim", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({82, 117}))
{
    flags.has_enter = 1;
}

ai_action* dodge_run_anim_action::get_instance()
{
    return &the_instance;
}

// 0x005E7730
dodge_run_anim_action::~dodge_run_anim_action()
{
}

// 0x0062A4C0 (.h:13)
int dodge_run_anim_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

dodge_sideways_noanim_action dodge_sideways_noanim_action::the_instance; // 0x01DDB590

// 0x0062A4F0 (.h:14)
dodge_sideways_noanim_action::dodge_sideways_noanim_action()
    : ai_action("dodge sideways noanim", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({82, 117}))
{
    flags.has_enter = 1;
}

ai_action* dodge_sideways_noanim_action::get_instance()
{
    return &the_instance;
}

// 0x005E7740
dodge_sideways_noanim_action::~dodge_sideways_noanim_action()
{
}

// 0x0062A7D0 (.h:14)
int dodge_sideways_noanim_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

dodge_run_noanim_action dodge_run_noanim_action::the_instance; // 0x01DA0300

// 0x0062A800 (.h:15)
dodge_run_noanim_action::dodge_run_noanim_action()
    : ai_action("dodge run noanim", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({82, 117}))
{
    flags.has_enter = 1;
}

ai_action* dodge_run_noanim_action::get_instance()
{
    return &the_instance;
}

// 0x005E7750
dodge_run_noanim_action::~dodge_run_noanim_action()
{
}

// 0x0062AAE0 (.h:15)
int dodge_run_noanim_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

shield_cover_action shield_cover_action::the_instance; // 0x01D751D8

// 0x0062AE20 (.h:17)
shield_cover_action::shield_cover_action()
    : ai_action("dodge shield cover", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({82, 117}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* shield_cover_action::get_instance()
{
    return &the_instance;
}

// 0x005E7770
shield_cover_action::~shield_cover_action()
{
}

// 0x0062B100 (.h:17)
int shield_cover_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

spin_deflect_action spin_deflect_action::the_instance; // 0x01DC9838

// 0x0062AB10 (.h:16)
spin_deflect_action::spin_deflect_action()
    : ai_action("spin deflect", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({82, 117}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* spin_deflect_action::get_instance()
{
    return &the_instance;
}

// 0x005E7760
spin_deflect_action::~spin_deflect_action()
{
}

// 0x0062ADF0 (.h:16)
int spin_deflect_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

