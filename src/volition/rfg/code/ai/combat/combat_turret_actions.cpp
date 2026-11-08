// Original: volition/rfg/code/ai/combat/combat_turret_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/combat/combat_turret_actions.h"

#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/objects/turret_info.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"

// Position 14 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG14")

// 0x00628B30 (:114). LTCG: np EAX; the callers' decompile shows the arguments shifted.
static destructive_fire_test_results test_destructive_turret_line_of_fire(const npc* np, const turret* tp, const vector& pos)
{
    const weapon* wp = tp->get_weapon_inst();
    vector muzzle;
    wp->get_muzzle_transform(muzzle, nullptr);
    weapon_fire_info fire_info;
    fire_info.start_pos = muzzle;
    fire_info.end_pos = pos;
    fire_info.hit_handle = 0xFFFFFFFF;
    fire_info.w_info = nullptr;
    if (!weapons_test_bullet(fire_info, np, true, 0.0f, ZERO_VECTOR))
    {
        return DFTR_HIT_NOTHING;
    }
    object* hit = World.object_from_handle(fire_info.hit_handle);
    if (hit->get_obj_type() == OT_ZONE)
    {
        return DFTR_HIT_NOTHING;
    }
    if (hit->get_obj_type() == OT_MOVER && hit->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        const object_mover* mover = static_cast<const object_mover*>(hit);
        if (human_teams_get_disposition(np, mover->team) >= TEAM_DISPOSITION_FRIENDLY && mover->get_importance_level() > 0)
        {
            return DFTR_HIT_BAD_TARGET;
        }
    }
    const human* hit_human = human_combat_get_hit_human(hit);
    if (hit_human != nullptr && human_teams_get_disposition(hit_human, np) >= TEAM_DISPOSITION_FRIENDLY)
    {
        return DFTR_HIT_BAD_TARGET;
    }
    const float hit_dist = vec_dist(np->pos, fire_info.hit_pos);
    const weapon_info* w_info = tp->t_info->w_info;
    const explosion_info* explosion = w_info->m_ai_explosion_info != nullptr ? w_info->m_ai_explosion_info : w_info->m_explosion_info;
    const float danger_dist = explosion->m_knockdown_radius + 1.25f;
    const float target_dist = vec_dist(np->pos, pos);
    if (hit_dist <= danger_dist)
    {
        return DFTR_DANGER;
    }
    // An obstruction lies between 8 m in front of the target and 1 m behind it.
    if (target_dist - 8.0f < hit_dist && hit_dist <= target_dist + 1.0f)
    {
        return DFTR_HIT_OBSTRUCTION;
    }
    return DFTR_HIT_BAD_TARGET;
}

// 0x00628D40 (:189)
turret* get_best_turret(const npc* np, const vector& target_pos)
{
    const ai_system* ai = np->get_ai();
    for (const ai_belief* belief = ai->perception.get_first_belief(AI_BELIEF_TURRET); belief != nullptr; belief = ai->perception.get_next_belief(belief))
    {
        object* objp = belief->handle != 0xFFFFFFFF ? World.object_from_handle(belief->handle) : nullptr;
        if (objp == nullptr || objp->get_obj_type() != OT_TURRET)
        {
            continue;
        }
        turret* tp = static_cast<turret*>(objp);
        if (!tp->is_functional(false) || tp->is_vehicle_turret())
        {
            continue;
        }
        if (tp->current_operator != np->get_handle() && tp->disable_for_npcs_time.valid() && !tp->disable_for_npcs_time.elapsed())
        {
            continue;
        }
        const human* reserving = human_combat_turret_get_reserving_human(tp);
        if (reserving != nullptr && reserving != np)
        {
            continue;
        }
        if (vec_dist_squared(np->pos, tp->pos) > 400.0f) // 20 m
        {
            continue;
        }
        // The way to the turret must not lead within 7 m of the target.
        vector closest;
        ix_closest_point_on_line_segment(closest, target_pos, np->pos, tp->pos);
        if (vec_dist_squared(closest, target_pos) < 49.0f)
        {
            continue;
        }
        if (tp->can_hit_target(target_pos) && !ai->outside_defensive_perimeter(tp->pos, false, nullptr))
        {
            return tp;
        }
    }
    return nullptr;
}

// 0x0069AA10 (:252)
bool attack_from_turret_action_common(npc* np, turret* tp, human* target)
{
    const ai_system* ai = np->get_ai();
    vector aim_pos;
    if (!ai->perception.get_effective_target_pos(aim_pos, target, nullptr, nullptr, false))
    {
        return false;
    }
    const bool electrocuted = ai->electrocution_timer.valid() && !ai->electrocution_timer.elapsed();
    if (electrocuted)
    {
        vector offset;
        offset.rand_sphere();
        aim_pos += offset * fl_rand_range(0.5f, 5.0f);
    }
    else if (!tp->can_hit_target(aim_pos))
    {
        return false;
    }
    if (tp->aim_turret(aim_pos) && !electrocuted && np->npc_flags.no_turret_attacks == 0)
    {
        ai_combat_fire_turret_at_target(np, tp, target, false);
    }
    return true;
}

// 0x0069AB90 (:289)
bool defensive_combat_turret_action_common(npc* np, turret* tp, human* target)
{
    const ai_system* ai = np->get_ai();
    vector aim_pos;
    if (!ai->perception.get_effective_target_pos(aim_pos, target, nullptr, nullptr, false))
    {
        return false;
    }
    const bool electrocuted = ai->electrocution_timer.valid() && !ai->electrocution_timer.elapsed();
    if (electrocuted)
    {
        vector offset;
        offset.rand_sphere();
        aim_pos += offset * fl_rand_range(0.5f, 5.0f);
    }
    if (tp->aim_turret(aim_pos) && !electrocuted && np->npc_flags.no_turret_attacks == 0 && ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr)
    {
        ai_combat_fire_turret_at_target(np, tp, target, false);
    }
    return true;
}

// 0x005E6960 (:330)
int get_on_turret_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 50;
}

// 0x00629050 (:335)
ai_results get_on_turret_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    turret* tp = human_combat_get_reserved_turret(np);
    if (tp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (human_combat_get_manned_turret(np) != nullptr)
    {
        return AI_RESULT_SUCCESS;
    }
    if (vec_dist_squared(tp->get_operation_position(), np->pos) > 1.0f) // 1 m
    {
        return AI_RESULT_FAILURE;
    }
    if (!ai_combat_is_oriented_with_turret(np, tp, nullptr))
    {
        return AI_RESULT_FAILURE;
    }
    const weapon* wp = human_weapons_get_weapon(np);
    if (wp != nullptr && wp != tp->get_weapon_inst())
    {
        return AI_RESULT_FAILURE;
    }
    ai->stop_moving();
    human_set_stance(np, HST_STAND);
    if (!human_combat_man_turret(np, tp))
    {
        return AI_RESULT_FAILURE;
    }
    ai->action_timer.set(500); // ms
    ai->planner.lock_action(true);
    return AI_RESULT_SUCCESS;
}

// 0x00678BA0 (:378)
ai_results get_on_turret_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    turret* tp = human_combat_get_manned_turret(np);
    if (tp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const object* attention = ai->perception.get_attention_target();
    const human* target = attention != nullptr && attention->get_obj_type() == OT_HUMAN && human_teams_get_disposition(np, static_cast<const human*>(attention)) == TEAM_DISPOSITION_HOSTILE ? static_cast<const human*>(attention) : human_combat_get_target(np);
    vector aim_pos;
    if (target != nullptr && ai->perception.get_effective_target_pos(aim_pos, target, nullptr, nullptr, false))
    {
        tp->aim_turret(aim_pos);
    }
    return ai->action_timer.elapsed() ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

// 0x005E6970 (:405)
int release_turret_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 10;
}

// 0x005E6980 (:410)
ai_results release_turret_action::do_action(npc* np) const
{
    human_combat_abandon_turret(np);
    return AI_RESULT_SUCCESS;
}

// 0x005E69A0 (:419)
int go_to_turret_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 200;
}

// 0x0069AD30 (:424)
ai_results go_to_turret_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    const turret* tp = human_combat_get_reserved_turret(np);
    if (tp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const human* target = human_combat_get_target(np);
    bool combat_ready = false;
    if (target == nullptr || ai->perception.get_belief(AI_BELIEF_ENEMY_EXISTS, static_cast<const object*>(target)) == nullptr)
    {
        ai->perception.stop_paying_attention(true);
        ai->perception.set_attention_mode(ATTENTION_IDLE, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    }
    else if (vec_dist_squared(np->pos, tp->pos) < 25.0f) // 5 m
    {
        ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    }
    else
    {
        ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
        combat_ready = true;
    }
    human_set_combat_ready(np, combat_ready, 0);
    ai->action_substate = 0;
    return AI_RESULT_SUCCESS;
}

// 0x0069AEF0 (:473)
ai_results go_to_turret_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const turret* tp = human_combat_get_reserved_turret(np);
    if (tp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (human_combat_get_manned_turret(np) != nullptr)
    {
        return AI_RESULT_SUCCESS;
    }
    ai_combat_action_parameters params;
    params.init_npc(np);
    params.init_target(true);
    if (params.target == nullptr)
    {
        ai->perception.set_attention_mode(ATTENTION_IDLE, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
        human_set_combat_ready(np, false, 0);
    }
    else if (params.weapon_inst == nullptr || !params.weapon_inst->w_info->is_firearm() || !params.weapon_inst->is_loaded() || vec_dist_squared(np->pos, tp->pos) < 25.0f) // 5 m
    {
        if (ai->perception.get_attention_mode() != ATTENTION_LOOK_AT_OBJECT)
        {
            ai->perception.pay_attention(params.target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
            human_set_combat_ready(np, false, 0);
        }
    }
    else
    {
        if (ai->perception.get_attention_mode() != ATTENTION_AIM_AT_OBJECT)
        {
            ai->perception.pay_attention(static_cast<const object*>(params.target), ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
        }
        human_set_combat_ready(np, true, 0);
        ai_combat_fire_at_target(params, false);
    }
    vector use_pos;
    const bool close_enough = ai_combat_is_close_enough_to_use_turret(np, tp, ai->action_substate == 0 ? 0.5f : 1.0f, &use_pos);
    if (ai->action_substate == 0)
    {
        if (ai->is_pathing_to(use_pos, 0.25f, 1.0f) || !close_enough)
        {
            const ai_pathfind_results result = ai->pathfind_to(use_pos, params.target != nullptr ? HMS_RUN : HMS_WALK, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false);
            if (result > AI_PF_RAGDOLLED)
            {
                human_combat_abandon_turret(np);
                return AI_RESULT_FAILURE;
            }
        }
        else
        {
            ai->action_substate = 1;
            ai->stop_moving();
            human_set_stance(np, HST_STAND);
        }
    }
    if (ai->action_substate == 1)
    {
        if (!close_enough)
        {
            human_combat_abandon_turret(np);
            return AI_RESULT_FAILURE;
        }
        matrix use_orient;
        if (ai_combat_is_oriented_with_turret(np, tp, &use_orient))
        {
            return AI_RESULT_SUCCESS;
        }
        np->steering_vector = use_orient.fvec;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x00629140 (:580)
int attack_from_turret_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        return -1;
    }
    turret* tp = human_combat_get_reserved_turret(np);
    if (tp != nullptr)
    {
        if (ai_combat_target_too_close_to_explosive_turret(tp, target))
        {
            return -1;
        }
        if (tp->can_hit_target(target->pos))
        {
            my_data.use_target = tp;
            return 50;
        }
        desired_conditions |= ai_plan_conditions(143) | ai_plan_conditions(144);
    }
    if (np->current_team == HUMAN_TEAM_MARAUDER && human_teams_get_disposition(player_get_local_player(), np) != TEAM_DISPOSITION_ALLIED)
    {
        return -1;
    }
    vector head_pos;
    human_get_head_pos(target, head_pos);
    tp = get_best_turret(np, head_pos);
    if (tp == nullptr)
    {
        return -1;
    }
    my_data.use_target = tp;
    return 50;
}

// Inlined in three commit_setup_data functions.
static void reserve_turret_target(npc* np, object* objp)
{
    if (objp != nullptr && objp->get_obj_type() == OT_TURRET)
    {
        human_combat_reserve_turret(np, static_cast<turret*>(objp));
    }
}

// 0x006292B0 (:630)
void attack_from_turret_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    reserve_turret_target(np, setup_data.use_target);
}

// 0x0069B210 (:638)
ai_results attack_from_turret_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    turret* tp = human_combat_get_manned_turret(np);
    human* target = tp != nullptr ? human_combat_get_target(np) : nullptr;
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    if (this != last_action)
    {
        np->combat.hold_trigger_timestamp.invalidate();
        np->combat.shoot_enemy_timestamp.invalidate();
        human_set_stance(np, HST_STAND);
    }
    const bool ok = ai->planner.get_current_goal() == defensive_combat_goal::get_instance() ? defensive_combat_turret_action_common(np, tp, target) : attack_from_turret_action_common(np, tp, target);
    return ok ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x0069B2D0 (:673)
ai_results attack_from_turret_action::do_action(npc* np) const
{
    const ai_system* ai = np->get_ai();
    turret* tp = human_combat_get_manned_turret(np);
    human* target = tp != nullptr ? human_combat_get_target(np) : nullptr;
    if (target == nullptr || ai_combat_target_too_close_to_explosive_turret(tp, target))
    {
        return AI_RESULT_FAILURE;
    }
    const bool ok = ai->planner.get_current_goal() == defensive_combat_goal::get_instance() ? defensive_combat_turret_action_common(np, tp, target) : attack_from_turret_action_common(np, tp, target);
    return ok ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x006292E0 (:706)
int hold_position_at_turret_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        return -1;
    }
    const ai_belief* belief = np->get_ai()->perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(target));
    if (belief == nullptr)
    {
        return -1;
    }
    turret* tp = human_combat_get_reserved_turret(np);
    if (tp == nullptr)
    {
        vector head_pos;
        human_get_head_pos(target, head_pos);
        tp = get_best_turret(np, head_pos);
        if (tp == nullptr)
        {
            return -1;
        }
    }
    else if (!tp->can_hit_target(belief->pos))
    {
        return -1;
    }
    my_data.use_target = tp;
    return 50;
}

// 0x006293C0 (:742)
void hold_position_at_turret_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    reserve_turret_target(np, setup_data.use_target);
}

// 0x0069B360 (:750)
ai_results hold_position_at_turret_action::enter(npc* np, const ai_action* last_action) const
{
    if (this == last_action)
    {
        return AI_RESULT_SUCCESS;
    }
    ai_system* ai = np->get_ai();
    const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, np->combat.target_handle);
    if (belief == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai->perception.pay_attention(belief, ATTENTION_LOOK_AT_BELIEF, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    human_set_stance(np, HST_STAND);
    return AI_RESULT_SUCCESS;
}

// 0x00678C50 (:768)
ai_results hold_position_at_turret_action::do_action(npc* np) const
{
    const ai_system* ai = np->get_ai();
    turret* tp = human_combat_get_manned_turret(np);
    const human* target = tp != nullptr ? human_combat_get_target(np) : nullptr;
    if (target == nullptr || ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(target)) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr)
    {
        return AI_RESULT_SUCCESS;
    }
    vector aim_pos;
    if (ai->perception.get_effective_target_pos(aim_pos, target, nullptr, nullptr, false))
    {
        tp->aim_turret(aim_pos);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x006293F0 (:804)
int turret_destroy_cover_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const ai_belief* belief = np->get_ai()->perception.get_belief(AI_BELIEF_ENEMY_POSITION, np->combat.target_handle);
    if (belief == nullptr)
    {
        return -1;
    }
    const vehicle* vp = human_in_vehicle(np);
    if (vp != nullptr && (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER || human_is_driving(np)))
    {
        return -1;
    }
    turret* tp = human_combat_get_manned_turret(np);
    if (tp == nullptr || !tp->can_hit_target(belief->pos) || !tp->t_info->w_info->is_explosive())
    {
        return -1;
    }
    if (test_destructive_turret_line_of_fire(np, tp, belief->pos + UP_VECTOR) != DFTR_HIT_OBSTRUCTION)
    {
        return -1;
    }
    my_data.use_target = tp;
    return 50;
}

// 0x00629530 (:843)
void turret_destroy_cover_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    reserve_turret_target(np, setup_data.use_target);
}

// 0x0069B3D0 (:851)
ai_results turret_destroy_cover_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    ai_perception& perception = np->get_ai()->perception;
    const ai_belief* belief = perception.get_belief(AI_BELIEF_ENEMY_POSITION, np->combat.target_handle);
    if (belief == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    perception.pay_attention(belief, ATTENTION_LOOK_AT_BELIEF, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    return AI_RESULT_SUCCESS;
}

// 0x00678D10 (:867)
ai_results turret_destroy_cover_action::do_action(npc* np) const
{
    const ai_belief* belief = np->get_ai()->perception.get_belief(AI_BELIEF_ENEMY_POSITION, np->combat.target_handle);
    if (belief == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    turret* tp = human_combat_get_manned_turret(np);
    if (tp == nullptr || !tp->can_hit_target(belief->pos))
    {
        return AI_RESULT_FAILURE;
    }
    weapon* wp = tp->get_weapon_inst();
    if (wp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    tp->aim_turret(belief->pos + UP_VECTOR);
    if (wp->ready_to_fire())
    {
        if (ai_combat_turret_test_destructive_shot(np, tp, 0.1f))
        {
            human_weapons_firearm_pull_trigger(np);
        }
        else
        {
            wp->refire_delay_time = 0.0f;
        }
    }
    return AI_RESULT_IN_PROGRESS;
}

go_to_turret_action go_to_turret_action::the_instance; // 0x01D686AC

// 0x00622140 (.h:13)
go_to_turret_action::go_to_turret_action()
    : ai_action("go to turret", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 153}), ai_plan_conditions_from_bits({143}))
{
    flags.has_enter = 1;
}

ai_action* go_to_turret_action::get_instance()
{
    return &the_instance;
}

// 0x005E3FF0
go_to_turret_action::~go_to_turret_action()
{
}

// 0x00622380 (.h:13)
int go_to_turret_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

get_on_turret_action get_on_turret_action::the_instance; // 0x01D721D0

// 0x006223C0 (.h:14)
get_on_turret_action::get_on_turret_action()
    : ai_action("get on turret", ai_plan_conditions_from_bits({7, 9, 47, 58, 136, 137, 143, 153}), ai_plan_conditions_from_bits({144}))
{
    flags.has_enter = 1;
}

ai_action* get_on_turret_action::get_instance()
{
    return &the_instance;
}

// 0x005E4000
get_on_turret_action::~get_on_turret_action()
{
}

// 0x006226C0 (.h:14)
int get_on_turret_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

release_turret_action release_turret_action::the_instance; // 0x01DDB190

// 0x00622700 (.h:15)
release_turret_action::release_turret_action()
    : ai_action("release turret", ai_plan_conditions(), ai_plan_conditions_from_bits({145}))
{
}

ai_action* release_turret_action::get_instance()
{
    return &the_instance;
}

// 0x005E4020
release_turret_action::~release_turret_action()
{
}

// 0x00622750 (.h:15)
int release_turret_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

attack_from_turret_action attack_from_turret_action::the_instance; // 0x01D67E10

// 0x00622790 (.h:18)
attack_from_turret_action::attack_from_turret_action()
    : ai_action("attack turret", ai_plan_conditions_from_bits({144}), ai_plan_conditions_from_bits({24, 78}))
{
    flags.has_enter = 1;
}

ai_action* attack_from_turret_action::get_instance()
{
    return &the_instance;
}

// 0x005E4030
attack_from_turret_action::~attack_from_turret_action()
{
}

// 0x00622840 (.h:18)
int attack_from_turret_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

turret_destroy_cover_action turret_destroy_cover_action::the_instance; // 0x01DDAED8

// 0x00622870 (.h:21)
turret_destroy_cover_action::turret_destroy_cover_action()
    : ai_action("turret destroy cover", ai_plan_conditions(), ai_plan_conditions_from_bits({23}))
{
    flags.has_enter = 1;
}

ai_action* turret_destroy_cover_action::get_instance()
{
    return &the_instance;
}

// 0x005E4040
turret_destroy_cover_action::~turret_destroy_cover_action()
{
}

// 0x006228C0 (.h:21)
int turret_destroy_cover_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

hold_position_at_turret_action hold_position_at_turret_action::the_instance; // 0x01DCA084

// 0x006228F0 (.h:22)
hold_position_at_turret_action::hold_position_at_turret_action()
    : ai_action("hold position turret", ai_plan_conditions_from_bits({144}), ai_plan_conditions_from_bits({23}))
{
    flags.has_enter = 1;
}

ai_action* hold_position_at_turret_action::get_instance()
{
    return &the_instance;
}

// 0x005E4050
hold_position_at_turret_action::~hold_position_at_turret_action()
{
}

// 0x00622950 (.h:22)
int hold_position_at_turret_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

