// Original: volition/rfg/code/ai/combat/combat_cover_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/combat/combat_cover_actions.h"

#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_rate_limiters.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/belief_iterator.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/combat/combat_cover.h"
#include "volition/rfg/code/ai/combat/cover_selection.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/ai/search_actions.h"
#include "volition/rfg/code/ai/sorted_list.h"
#include "volition/rfg/code/gameplay/activities/activity_raid.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_cover.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_spinebend.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/matrix.h"

#include <math.h>

// Position 9 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG09")

// Cosine of 110 degrees: a dynamic initializer in rfg.exe, and nothing reads it there either.
static const float COVER_DIVE_CUTOFF_DOT = (float)cos((double)1.91983f); // 0x01DA06E8

// Magic numbers: how far a cover search goes (tweak table items, zero until tweak_table.xtbl loads).
static float Ai_cover_dist_combat; // 0x01D67DA8
static float Ai_cover_dist_dodge_reload; // 0x01D6573C
static tweak_table_item Tweak_table_item_Ai_cover_dist_combat("Ai_cover_dist_combat", &Ai_cover_dist_combat);
static tweak_table_item Tweak_table_item_Ai_cover_dist_dodge_reload("Ai_cover_dist_dodge_reload", &Ai_cover_dist_dodge_reload);

// 2.25 in .data (1.5 squared): the 1.5 radius of the collection below as a distance squared.
static const float ATTACK_DEFENSIVE_COVER_COLLECT_RADIUS_SQ = 2.25f; // 0x01642628
static const float ATTACK_DEFENSIVE_COVER_COLLECT_RADIUS = 1.5f;

static bool is_firing_ammo_type(weapon_ammo_type type)
{
    return type == WEAPON_AMMO_TYPE_BULLET || type == WEAPON_AMMO_TYPE_PROJECTILE || type == WEAPON_AMMO_TYPE_ELECTRICITY || type == WEAPON_AMMO_TYPE_REPAIR || type == WEAPON_AMMO_TYPE_BLANKS || type == WEAPON_AMMO_TYPE_FLAME;
}

// 0x005E3A70 (:173)
static attention_look_at_modes get_stepped_out_attention_mode(const ai_system* ai, const human* target, bool stepping_out)
{
    const ai_goal* goal = ai->planner.get_current_goal();
    if (goal == search_goal::get_instance())
    {
        return ATTENTION_SEARCH;
    }
    if (goal == acquire_target_goal::get_instance())
    {
        return stepping_out ? ATTENTION_AIM_AT_BELIEF : ATTENTION_ACQUIRE_VISUAL;
    }
    if (goal == advance_goal::get_instance())
    {
        return stepping_out ? ATTENTION_AIM_AT_OBJECT : ATTENTION_LOOK_AT_OBJECT;
    }
    if (goal == defensive_combat_goal::get_instance() || goal == combat_support_goal::get_instance() || goal == flush_out_target_goal::get_instance())
    {
        return stepping_out ? ATTENTION_AIM_AT_BELIEF : ATTENTION_LOOK_AT_BELIEF;
    }
    if (goal == melee_safe_position_goal::get_instance())
    {
        return ATTENTION_LOOK_AT_OBJECT;
    }
    return target != nullptr ? ATTENTION_AIM_AT_OBJECT : ATTENTION_LOOK_AT_BELIEF;
}

// 0x0061FCC0 (:215)
static cover_node* get_cover_node_from_plan(const ai_planner_search_node* plan_node)
{
    for (; plan_node != nullptr; plan_node = plan_node->next_node)
    {
        if (plan_node->next_action != attack_defensive_cover_action::get_instance())
        {
            object* use_target = plan_node->action_data.use_target;
            if (use_target != nullptr && use_target->get_obj_type() == OT_COVER_NODE)
            {
                return (cover_node*)use_target;
            }
        }
    }
    return nullptr;
}

// 0x006C2170 (:254)
static bool cover_get_cost_common(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node* next_action, ai_plan_conditions cover_position, bool stay_in_defensive_cover, const human* preferred_target)
{
    if (ai_combat_should_charge_target(np) != AI_COMBAT_SHOULD_NOT_CHARGE || human_in_vehicle(np) != nullptr || human_combat_get_manned_turret(np) != nullptr)
    {
        return false;
    }
    const human* target = preferred_target != nullptr ? preferred_target : human_combat_get_target(np);
    const weapon* action_weapon = ai_combat_get_action_firearm(np);
    if (target == nullptr || action_weapon == nullptr || target->current_team == HUMAN_TEAM_CIVILIAN)
    {
        return false;
    }
    if (!stay_in_defensive_cover && action_weapon->get_range_category(vec_dist(np->pos, target->pos)) == WEAPON_OUT_OF_RANGE)
    {
        return false;
    }
    cover_node* current = human_combat_cover_get_node(np);
    cover_node* node = get_cover_node_from_plan(next_action);
    if (node == nullptr)
    {
        node = current;
    }
    if (node != nullptr)
    {
        const bool using_node = human_combat_cover_is_using_node(np);
        const cover_check_results result = node->check_cover_type(np, target, false, !using_node, !stay_in_defensive_cover);
        if (result == CCR_OFFENSIVE || (result == CCR_DEFENSIVE_ONLY && stay_in_defensive_cover))
        {
            my_data.use_target = node;
            return true;
        }
        if (current != node)
        {
            return false;
        }
        desired_conditions |= ai_plan_conditions(59) | ai_plan_conditions(60) | cover_position;
    }
    cover_search_parameters params(np, (const object*)target, stay_in_defensive_cover ? Ai_cover_dist_dodge_reload : Ai_cover_dist_combat, false, node, false);
    cover_node* found = cover_selection_do_search(params);
    if (found == nullptr)
    {
        return false;
    }
    my_data.use_target = found;
    return true;
}

// 0x005E3AF0 (:346)
static bool plan_specifies_cover_stance(const ai_planner_search_node* next_action)
{
    for (; next_action != nullptr; next_action = next_action->next_node)
    {
        if (next_action->action_data.cover_stance != COVER_STANCE_UNDEFINED)
        {
            return true;
        }
    }
    return false;
}

// 0x0061FCF0 (:367): a node that does not cover against the target, so the npc can shoot, but covers against another enemy.
static bool evaluate_attack_defensive_cover_node(const npc* np, const human* target, const sorted_list_base<const human*, float>& enemies, const cover_node* node)
{
    if (node->will_provide_defensive_cover(np, target->pos, false) || node->is_occupied(np) != nullptr)
    {
        return false;
    }
    for (const human* const* enemy = enemies.get_first(); enemy != nullptr; enemy = enemies.get_next(enemy))
    {
        if (node->will_provide_defensive_cover(np, (*enemy)->pos, false))
        {
            return true;
        }
    }
    return false;
}

// 0x00676700 (:391)
static void accumulate_enemies(sorted_list_base_with_identity<const human*, float>& enemies, const npc* np, ai_belief_types belief_type, const human* target)
{
    belief_obj_iterator<human> it;
    it.init(np->get_ai(), belief_type);
    for (; it; ++it)
    {
        const human* enemy = it.operator->();
        if (enemy != target && human_teams_get_disposition(np, enemy->current_team) == TEAM_DISPOSITION_HOSTILE)
        {
            enemies.add_unique(enemy, vec_dist_squared(enemy->pos, np->pos), sorted_list_base_with_identity<const human*, float>::NEVER_REPLACE);
        }
    }
}

// 0x00676800 (:410)
static cover_node* find_attack_defensive_cover_node(const npc* np, const human* target)
{
    const float target_dist = vec_dist(np->pos, target->pos);
    sorted_list_with_identity<const human*, 3, float> enemies(true);
    accumulate_enemies(enemies, np, AI_BELIEF_FIRST_CONTACT, target);
    accumulate_enemies(enemies, np, AI_BELIEF_BULLET_IMPACT, target);
    accumulate_enemies(enemies, np, AI_BELIEF_DAMAGE_TAKEN, target);
    if (enemies.size() < 1)
    {
        return nullptr;
    }
    cover_node* current = human_combat_cover_get_node(np);
    if (current != nullptr && evaluate_attack_defensive_cover_node(np, target, enemies, current))
    {
        return current;
    }
    // Magic number: at most 10 nodes.
    unsigned int handles[10];
    const int num_handles = World.collect_objects<cover_node>(np->pos, ATTACK_DEFENSIVE_COVER_COLLECT_RADIUS, handles, 10, nullptr, nullptr);
    sorted_list<cover_node*, 10, float> nodes(true);
    // Magic number: a node can be up to 0.7 metres closer to the target than the npc.
    const double min_target_dist = (double)target_dist - (double)0.7f;
    for (int i = 0; i < num_handles; ++i)
    {
        cover_node* node = handles[i] != 0xFFFFFFFF ? (cover_node*)World.object_from_handle(handles[i]) : nullptr;
        if (node != nullptr && node->get_obj_type() != OT_COVER_NODE)
        {
            node = nullptr;
        }
        // Original bug kept: a handle that no longer resolves leaves node null, and the next line reads through it.
        if ((float)min_target_dist <= vec_dist(node->pos, target->pos))
        {
            const float node_dist_sq = vec_dist_squared(np->pos, node->pos);
            if (node_dist_sq <= ATTACK_DEFENSIVE_COVER_COLLECT_RADIUS_SQ)
            {
                nodes.add(node, node_dist_sq);
            }
        }
    }
    for (cover_node* const* node = nodes.get_first(); node != nullptr; node = nodes.get_next(node))
    {
        if (evaluate_attack_defensive_cover_node(np, target, enemies, *node))
        {
            return *node;
        }
    }
    return nullptr;
}

// 0x00676A00 (:471)
static bool attack_defensive_cover_get_cost_common(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np)
{
    if (np->hflags.riot_shield != 0 && np->shield_handle != 0xFFFFFFFF)
    {
        return false;
    }
    const human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        return false;
    }
    cover_node* node = find_attack_defensive_cover_node(np, target);
    if (node == nullptr)
    {
        return false;
    }
    const cover_node* current = human_combat_cover_get_node(np);
    if (current != nullptr && current != node)
    {
        desired_conditions |= ai_plan_conditions(59);
    }
    if (cover_is_at_node(np, node))
    {
        desired_conditions &= ~ai_plan_conditions(59);
        if (human_combat_cover_is_stepped_out(np))
        {
            desired_conditions |= ai_plan_conditions(61);
        }
    }
    my_data.use_target = node;
    return true;
}

// 0x005E3B10 (:523)
static void attack_defensive_cover_enter_common(npc* np, const cover_node* node, bool remain_in_cover)
{
    if (np->pf.path != nullptr)
    {
        bb_pf_free_path(&np->pf.path);
    }
    if (!remain_in_cover)
    {
        np->cur_state = HS_STANDARD;
        np->movement_mode = HMM_NONE;
        np->movement_submode = HMSM_NONE;
    }
    np->cur_movement_state = HMS_IDLE;
    np->move_speed = -1.0f;
    np->steering_vector = ZERO_VECTOR;
    human_set_stance(np, node->flags.crouch != 0 ? HST_CROUCH : HST_STAND);
    human_set_combat_ready(np, true, 0);
}

// 0x006C23F0 (:558)
int attack_cover_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const
{
    if (np->hflags.riot_shield != 0 && np->shield_handle != 0xFFFFFFFF)
    {
        return -1;
    }
    return cover_get_cost_common(my_data, desired_conditions, np, goal_setup_data, next_action, ai_plan_conditions(62), false, nullptr) ? 100 : -1;
}

// 0x005E3BA0 (:572)
void attack_cover_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    human_combat_cover_reserve_node(np, (cover_node*)setup_data.use_target);
}

// 0x006948A0 (:577)
ai_results attack_cover_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    const cover_node_firing_positions position = cover_firing_position_from_state(np);
    const human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, position != COVER_FIRE_POPUP ? ATTENTION_STEERING_PLANT_FEET : ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    if (last_action != this)
    {
        np->combat.shoot_enemy_timestamp.invalidate();
        np->combat.hold_trigger_timestamp.invalidate();
    }
    return AI_RESULT_SUCCESS;
}

// 0x00694910 (:598)
ai_results attack_cover_action::do_action(npc* np) const
{
    const object* target = World.object_from_handle(np->combat.target_handle);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai_system* ai = np->get_ai();
    const cover_node* node = human_combat_cover_get_node(np);
    if (node == nullptr || node->check_cover_type(np, target, false, false, true) != CCR_OFFENSIVE)
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    ai_combat_action_parameters params;
    params.init_npc(np);
    if (params.weapon_inst == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (!params.weapon_inst->is_loaded())
    {
        ai->planner.request_replan();
        return AI_RESULT_IN_PROGRESS;
    }
    if (params.init_target(true))
    {
        if (params.weapon_inst->get_range_category(params.dist_to_target) == WEAPON_OUT_OF_RANGE)
        {
            return AI_RESULT_FAILURE;
        }
        ai_combat_fire_at_target(params, false);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x00676B20 (:647)
int attack_defensive_cover_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return attack_defensive_cover_get_cost_common(my_data, desired_conditions, np) ? 50 : -1;
}

// 0x005E3BC0 (:653)
void attack_defensive_cover_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    cover_node* node = (cover_node*)setup_data.use_target;
    // The answer is not used.
    node->is_occupied(np);
    human_combat_cover_reserve_node(np, node);
}

// 0x00694A10 (:666)
ai_results attack_defensive_cover_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    const human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const cover_node* node = human_combat_cover_get_node(np);
    if (node == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    attack_defensive_cover_enter_common(np, node, false);
    if (last_action != this)
    {
        np->combat.shoot_enemy_timestamp.invalidate();
        np->combat.hold_trigger_timestamp.invalidate();
    }
    return AI_RESULT_SUCCESS;
}

// 0x00694AA0 (:696)
ai_results attack_defensive_cover_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (World.object_from_handle(np->combat.target_handle) == nullptr || human_combat_cover_get_node(np) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    ai_combat_action_parameters params;
    params.init_npc(np);
    if (params.weapon_inst == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (!params.weapon_inst->is_loaded())
    {
        ai->planner.request_replan();
    }
    else if (params.init_target(true))
    {
        if (params.weapon_inst->get_range_category(params.dist_to_target) == WEAPON_OUT_OF_RANGE)
        {
            return AI_RESULT_FAILURE;
        }
        ai_combat_fire_at_target(params, false);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x006C2460 (:745)
int reload_cover_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const
{
    if (np->hflags.riot_shield != 0 && np->shield_handle != 0xFFFFFFFF)
    {
        return -1;
    }
    if (np->get_ai()->perception.get_first_belief(AI_BELIEF_ENEMY_POSITION) == nullptr || Ai_cover_search_limiter.npc_has_request_pending(np) || goal_setup_data.goal == search_goal::get_instance())
    {
        return -1;
    }
    if (next_action != nullptr && ai_plan_contains_action_precondition(next_action, ai_plan_conditions(58)))
    {
        return -1;
    }
    if (desired_conditions & ai_plan_conditions(58))
    {
        return -1;
    }
    if (!cover_get_cost_common(my_data, desired_conditions, np, goal_setup_data, next_action, ai_plan_conditions(61), true, nullptr))
    {
        return -1;
    }
    if (!plan_specifies_cover_stance(next_action) && !human_combat_cover_is_using_node(np))
    {
        const cover_node* node = (const cover_node*)my_data.use_target;
        if (node == nullptr || node->get_obj_type() != OT_COVER_NODE)
        {
            return -1;
        }
        const human* target = human_combat_get_target(np);
        // With no target, a point one metre in front of the node.
        const vector target_pos = target != nullptr ? target->pos : vector(node->pos.x + node->orient.fvec.x, node->orient.fvec.y + node->pos.y, node->orient.fvec.z + node->pos.z);
        my_data.cover_stance = cover_pick_stance(node, target_pos);
        if (my_data.cover_stance == COVER_STANCE_UNDEFINED)
        {
            return -1;
        }
    }
    return 50;
}

// 0x005E3BF0 (:811)
void reload_cover_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    human_combat_cover_reserve_node(np, (cover_node*)setup_data.use_target);
    if (setup_data.cover_stance != COVER_STANCE_UNDEFINED)
    {
        np->get_ai()->desired_cover_stance = setup_data.cover_stance;
    }
}

// 0x0061FD60 (:820)
ai_results reload_cover_action::enter(npc* np, const ai_action* last_action) const
{
    weapon* wp = human_weapons_get_weapon(np);
    if (wp == nullptr || !is_firing_ammo_type(wp->w_info->ammo_type))
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        np->get_ai()->planner.lock_action(true);
        wp->cancel_reload();
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E3C20 (:837)
ai_results reload_cover_action::do_action(npc* np) const
{
    return ai_combat_do_reload(np) ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

// 0x00676B50 (:849)
int reload_defensive_cover_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return attack_defensive_cover_get_cost_common(my_data, desired_conditions, np) ? 30 : -1;
}

// 0x005E3C40 (:855)
void reload_defensive_cover_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    cover_node* node = (cover_node*)setup_data.use_target;
    // The answer is not used.
    node->is_occupied(np);
    human_combat_cover_reserve_node(np, node);
}

// 0x00694B70 (:868)
ai_results reload_defensive_cover_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    weapon* wp = human_weapons_get_weapon(np);
    if (wp == nullptr || !is_firing_ammo_type(wp->w_info->ammo_type))
    {
        return AI_RESULT_FAILURE;
    }
    const cover_node* node = human_combat_cover_get_node(np);
    if (node == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    human_set_combat_ready(np, true, 0);
    attack_defensive_cover_enter_common(np, node, true);
    if (last_action != this)
    {
        ai->planner.lock_action(true);
        wp->cancel_reload();
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E3C70 (:900)
ai_results reload_defensive_cover_action::do_action(npc* np) const
{
    return ai_combat_do_reload(np) ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

// 0x006C2680 (:912)
int dodge_cover_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const
{
    if (np->hflags.riot_shield != 0 && np->shield_handle != 0xFFFFFFFF)
    {
        return -1;
    }
    if (np->get_ai()->is_providing_covering_fire() != nullptr)
    {
        return -1;
    }
    const object* selected = goal_setup_data.selected_target;
    const human* preferred_target = selected != nullptr && selected->get_obj_type() == OT_HUMAN ? (const human*)selected : nullptr;
    return cover_get_cost_common(my_data, desired_conditions, np, goal_setup_data, next_action, ai_plan_conditions(61), true, preferred_target) ? 50 : -1;
}

// 0x005E3C90 (:931)
void dodge_cover_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    human_combat_cover_reserve_node(np, (cover_node*)setup_data.use_target);
}

// 0x005E3CB0 (:936)
ai_results dodge_cover_action::do_action(npc* np) const
{
    if (np->combat.combat_ready_timestamp.valid() && !np->combat.combat_ready_timestamp.elapsed())
    {
        human_set_combat_ready(np, true, 0);
    }
    const human* target = human_combat_get_target(np);
    if (target != nullptr)
    {
        const cover_node* node = human_combat_cover_get_node(np);
        if (node != nullptr && node->will_provide_defensive_cover(np, (const object*)target, false))
        {
            return AI_RESULT_IN_PROGRESS;
        }
    }
    return AI_RESULT_FAILURE;
}

// 0x006579D0 (:959)
int go_to_cover_action::get_cost(ai_action_setup_data&, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const
{
    if (np->current_team == HUMAN_TEAM_MARAUDER)
    {
        return -1;
    }
    if (desired_conditions & ai_plan_conditions(60))
    {
        return -1;
    }
    // Conditions 24 and 88.
    static const ai_plan_conditions EXCLUDE_GOALS = ai_plan_conditions(24) | ai_plan_conditions(88); // 0x01DDBB94
    if (goal_setup_data.goal->pursues_condition(EXCLUDE_GOALS))
    {
        return -1;
    }
    for (const ai_planner_search_node* node = next_action; node != nullptr && node->next_action != nullptr; node = node->next_node)
    {
        if (node->next_action->get_pre_conditions() & ai_plan_conditions(58))
        {
            return -1;
        }
    }
    if (ai_plan_contains_action_postcondition(next_action, ai_plan_conditions(1)))
    {
        desired_conditions &= ~ai_plan_conditions(4);
    }
    return np->get_ai()->is_providing_covering_fire() == nullptr ? 100 : -1;
}

// 0x005E3D20 (:1004)
void go_to_cover_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x00676B80 (:1010)
ai_results go_to_cover_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    human* target = (human*)World.object_from_handle(ai->perception.get_attention_handle());
    if (target == nullptr || target->get_obj_type() != OT_HUMAN || target->is_killed())
    {
        target = human_combat_get_target(np);
    }
    cover_node* node = human_combat_cover_get_node(np);
    if (node == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const attention_look_at_modes mode = get_stepped_out_attention_mode(ai, target, false);
    attention_look_at_modes look_at_mode = mode;
    attention_steering_modes steering = ATTENTION_STEERING_PLANT_FEET;
    bool combat_ready = true;
    if (last_action != this)
    {
        ai->action_substate = 0;
        if (ai->planner.plan_contains(attack_defensive_cover_action::get_instance()))
        {
            look_at_mode = ATTENTION_AIM_AT_OBJECT;
            steering = ATTENTION_STEERING_USE_DEFAULT;
        }
        else if (mode == ATTENTION_AIM_AT_OBJECT)
        {
            bool covered = ai->get_ally_covering_me() != nullptr;
            if (!covered && ai_combat_should_request_covering_fire(np, node, target))
            {
                npc* ally = ai_combat_find_someone_to_cover_me(np, target);
                if (ally != nullptr && ai->request_covering_fire(ally))
                {
                    covered = true;
                    if (np->play_voice_line(VOICE_LINE_COMBAT_REQUEST_COVER_ME, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false) == PLAY_VOICE_SUCCESS)
                    {
                        ally->play_voice_line(VOICE_LINE_COMBAT_REQUEST_COVER_ME_RESPONSE_YES, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
                    }
                }
            }
            if (covered)
            {
                look_at_mode = ATTENTION_LOOK_AT_OBJECT;
                combat_ready = false;
                ai->action_substate = 1;
            }
        }
    }
    human_set_combat_ready(np, combat_ready, 0);
    ai->perception.set_attention_mode(look_at_mode, steering, ATTENTION_AWARENESS_NORMAL);
    if (mode == ATTENTION_SEARCH)
    {
        human_set_stance(np, HST_STAND);
    }
    if (ai->desired_cover_stance == COVER_STANCE_UNDEFINED)
    {
        if (mode != ATTENTION_AIM_AT_OBJECT || target == nullptr || target->is_killed())
        {
            return AI_RESULT_FAILURE;
        }
        const cover_node_stances stance = cover_pick_stance(node, target->pos);
        if (stance == COVER_STANCE_UNDEFINED)
        {
            human_combat_cover_abandon_node(np);
            node->disable(COVER_DISABLED_NO_STANCES, -1);
            return AI_RESULT_FAILURE;
        }
        ai->desired_cover_stance = stance;
    }
    if (!ai->in_cover_timer.valid())
    {
        ai->in_cover_timer.set(0);
    }
    ai->out_of_cover_timer.invalidate();
    return AI_RESULT_SUCCESS;
}

// 0x00694C50 (:1118)
ai_results go_to_cover_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    ai_combat_action_parameters params;
    params.init_npc(np);
    params.init_target(true);
    if (!params.init_cover_node())
    {
        return AI_RESULT_FAILURE;
    }
    if (ai->action_substate > 0)
    {
        ai->request_covering_fire(nullptr);
    }
    // rfg.exe walks the first contact beliefs here when under 5 metres from the target and does nothing with the result.
    if (ai->planner.get_current_goal() != advance_goal::get_instance())
    {
        // Magic number: 3 seconds.
        np->combat.combat_maneuver_timestamp.set(3000);
    }
    const attention_look_at_modes mode = get_stepped_out_attention_mode(ai, params.target, false);
    if (mode != ATTENTION_AIM_AT_OBJECT)
    {
        human_set_combat_ready(np, true, 0);
    }
    else
    {
        if (params.weapon_inst != nullptr && params.weapon_inst->w_info->ammo_type == WEAPON_AMMO_TYPE_BULLET && !params.weapon_inst->is_loaded())
        {
            // An npc on its way to cover gets a free round rather than an empty gun.
            params.weapon_inst->set_rounds_in_magazine(1);
        }
        if (params.weapon_inst != nullptr && params.weapon_inst->is_loaded())
        {
            if (ai->perception.get_attention_mode() == ATTENTION_AIM_AT_OBJECT)
            {
                human_set_combat_ready(np, true, 0);
                if (params.weapon_inst->get_range_category(params.dist_to_target) != WEAPON_OUT_OF_RANGE)
                {
                    ai_combat_fire_at_target(params, false);
                }
            }
        }
        else
        {
            human_set_stance(np, HST_CROUCH);
            human_set_combat_ready(np, false, 0);
            ai->perception.set_attention_mode(ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        }
    }
    if (cover_is_at_node(np, params.node))
    {
        return AI_RESULT_SUCCESS;
    }
    if (cover_generate_path_to_node(np, params.node, mode != ATTENTION_SEARCH ? HMS_RUN : HMS_WALK, HST_STAND) == AI_RESULT_FAILURE)
    {
        human_combat_cover_abandon_node(np);
        // Magic number: 20 seconds.
        params.node->disable(COVER_DISABLED_UNREACHABLE, 20000);
        return AI_RESULT_FAILURE;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x0061FDE0 (:1200)
ai_results go_to_cover_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (this != next_action)
    {
        np->get_ai()->end_covering_fire_request();
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E3D30 (:1214)
int leave_cover_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 100;
}

// 0x00676E00 (:1219)
ai_results leave_cover_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    const cover_node* node = nullptr;
    cover_node_stances stance = COVER_STANCE_UNDEFINED;
    if (human_combat_cover_is_using_node(np))
    {
        node = human_combat_cover_get_node(np);
        if (node->flags.fire_popup == 0)
        {
            stance = cover_get_current_stance(np);
        }
    }
    human_combat_cover_abandon_node(np);
    ai->action_substate = 0;
    ai->planner.lock_action(true);
    if (stance == COVER_STANCE_UNDEFINED)
    {
        return AI_RESULT_SUCCESS;
    }
    // Step back and to the side the npc is leaning to, further on the left. Magic numbers: 0.6 and 0.45 metres.
    vector leave_pos;
    if (stance == COVER_STANCE_CROUCH_LEFT || stance == COVER_STANCE_STAND_LEFT)
    {
        leave_pos.x = node->pos.x + (-node->orient.rvec.x - node->orient.fvec.x) * 0.6f;
        leave_pos.y = node->pos.y + (-node->orient.rvec.y - node->orient.fvec.y) * 0.6f;
        leave_pos.z = node->pos.z + (-node->orient.rvec.z - node->orient.fvec.z) * 0.6f;
    }
    else
    {
        leave_pos.x = node->pos.x + (node->orient.rvec.x - node->orient.fvec.x) * 0.45f;
        leave_pos.y = node->pos.y + (node->orient.rvec.y - node->orient.fvec.y) * 0.45f;
        leave_pos.z = node->pos.z + (node->orient.rvec.z - node->orient.fvec.z) * 0.45f;
    }
    nav_cell* ncp = nullptr;
    navpoint* npp = nullptr;
    if (ai->perception.get_nearest_navpoint_or_navcell(ncp, npp, nullptr, nullptr))
    {
        // Original bug kept: with only a navpoint nearby, its object handle goes in as the nav cell hint, and the navpoint
        // argument stays -1.
        const uint64_t hint = ncp != nullptr ? nav_cell_get_handle(ncp) : (uint64_t)npp->get_handle();
        if (ai->set_direct_movement_to_pos(leave_pos, hint, HMS_WALK, HST_CURRENT, true) == AI_PF_SUCCESS)
        {
            ai->action_substate = np->movement_mode == HMM_PATHFINDING ? 1 : 0;
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E3D40 (:1281)
ai_results leave_cover_action::do_action(npc* np) const
{
    if (np->get_ai()->action_substate == 0)
    {
        return AI_RESULT_SUCCESS;
    }
    return np->movement_mode == HMM_PATHFINDING ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x0061FE00 (:1299)
int transition_to_cover_action::get_cost(ai_action_setup_data&, ai_plan_conditions& desired_conditions, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return (desired_conditions & ai_plan_conditions(62)) ? -1 : 20;
}

// 0x005E3D70 (:1310)
void transition_to_cover_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x00657C50 (:1316)
ai_results transition_to_cover_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    const object* attention_target = World.object_from_handle(ai->perception.get_attention_handle());
    const human* target = attention_target != nullptr && attention_target->get_obj_type() == OT_HUMAN ? (const human*)attention_target : nullptr;
    cover_node* node = human_combat_cover_get_node(np);
    if (node == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (human_combat_cover_is_using_node(np))
    {
        ai->action_timer.invalidate();
        return AI_RESULT_SUCCESS;
    }
    if (!cover_is_at_node(np, node))
    {
        return AI_RESULT_FAILURE;
    }
    ai->stop_moving();
    ai->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    human_set_combat_ready(np, true, 0);
    cover_node_stances stance = ai->desired_cover_stance;
    if (stance == COVER_STANCE_UNDEFINED)
    {
        if (target == nullptr || get_stepped_out_attention_mode(ai, target, false) != ATTENTION_AIM_AT_OBJECT)
        {
            return AI_RESULT_FAILURE;
        }
        stance = cover_pick_stance(node, target->pos);
        if (stance == COVER_STANCE_UNDEFINED)
        {
            human_combat_cover_abandon_node(np);
            node->disable(COVER_DISABLED_NO_STANCES, -1);
            return AI_RESULT_FAILURE;
        }
        ai->desired_cover_stance = stance;
    }
    human_combat_cover_set_stepped_in_state(np, stance);
    const animation_action anim_index = human_combat_cover_get_transition_action(np, node, stance);
    const vector away_from_cover(-node->orient.fvec.x, -node->orient.fvec.y, -node->orient.fvec.z);
    matrix dest_orient;
    dest_orient.make(away_from_cover);
    // Magic number: animlib flags, not recovered as names.
    np->char_inst->play_action(anim_index, ANIM_ALIAS_TRANSITION_TO_COVER, 1.0f, 0x4040);
    np->char_inst->set_action_dest_position(np->pos, np->orient, node->pos, ANIM_ALIAS_TRANSITION_TO_COVER, -1.0f, false);
    np->char_inst->set_action_dest_orient(np->orient, dest_orient, ANIM_ALIAS_TRANSITION_TO_COVER, -1.0f);
    ai->action_timer.invalidate();
    np->steering_vector = away_from_cover;
    np->rotate_dir = HUMAN_ROTATE_SHORTEST;
    ai->planner.lock_action(true);
    if (!ai->in_cover_timer.valid())
    {
        ai->in_cover_timer.set(0);
    }
    ai->out_of_cover_timer.invalidate();
    return AI_RESULT_SUCCESS;
}

// 0x005E3D80 (:1411)
ai_results transition_to_cover_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (human_combat_cover_get_node(np) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    if (ai->action_timer.valid())
    {
        return ai->action_timer.elapsed() ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
    }
    return np->char_inst->is_playing_action(ANIM_ALIAS_TRANSITION_TO_COVER) ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x005E3E00 (:1455)
int cover_switch_stance_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 20;
}

// 0x00657F80 (:1460)
ai_results cover_switch_stance_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    const cover_node* node = human_combat_cover_get_node(np);
    if (node == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai->planner.lock_action(true);
    const cover_node_stances stance = cover_stance_from_state(np);
    const cover_node_firing_positions position = cover_firing_position_from_state(np);
    const cover_node_stances desired = ai->desired_cover_stance;
    if (stance == COVER_STANCE_UNDEFINED || desired == COVER_STANCE_UNDEFINED || stance == desired || position != COVER_FIRE_UNDEFINED)
    {
        return AI_RESULT_FAILURE;
    }
    ai->stop_moving();
    ai->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    np->steering_vector = vector(-node->orient.fvec.x, -node->orient.fvec.y, -node->orient.fvec.z);
    human_combat_cover_set_stepped_in_state(np, desired);
    human_set_combat_ready(np, true, 0);
    return AI_RESULT_SUCCESS;
}

// 0x005E3E10 (:1497)
ai_results cover_switch_stance_action::do_action(npc* np) const
{
    if (human_combat_get_target(np) == nullptr || human_combat_cover_get_node(np) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    return np->char_inst->is_playing_action(ANIM_ALIAS_COVER_TO_COVER) ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x005E3E60 (:1525)
int cover_step_in_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 20;
}

// 0x006580B0 (:1530)
ai_results cover_step_in_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    const cover_node* node = human_combat_cover_get_node(np);
    if (node == nullptr || !human_combat_cover_is_using_node(np))
    {
        return AI_RESULT_FAILURE;
    }
    ai->planner.lock_action(true);
    if (human_combat_cover_is_stepped_in(np))
    {
        ai->action_timer.invalidate();
        return AI_RESULT_SUCCESS;
    }
    const cover_node_stances stance = cover_stance_from_state(np);
    ai->stop_moving();
    // Magic number: 3 ramp out frames. 1002 and 1003 are the turn aliases, which cover uses to step.
    np->char_inst->stop_action(ANIM_ALIAS_TURN_LEFT, 3, false);
    np->char_inst->stop_action(ANIM_ALIAS_TURN_RIGHT, 3, false);
    ai->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    human_set_combat_ready(np, true, 0);
    if (stance == COVER_STANCE_CROUCH_LEFT || stance == COVER_STANCE_CROUCH_RIGHT)
    {
        human_set_stance(np, HST_CROUCH);
    }
    np->steering_vector = vector(-node->orient.fvec.x, -node->orient.fvec.y, -node->orient.fvec.z);
    np->rotate_dir = stance == COVER_STANCE_CROUCH_LEFT || stance == COVER_STANCE_STAND_LEFT ? HUMAN_ROTATE_COUNTERCLOCKWISE : HUMAN_ROTATE_CLOCKWISE;
    human_combat_cover_set_stepped_in_state(np, stance);
    // Magic number: 0.3 seconds to step in.
    ai->action_timer.set(300);
    ai->in_cover_timer.set(0);
    ai->out_of_cover_timer.invalidate();
    return AI_RESULT_SUCCESS;
}

// 0x005E3E70 (:1586)
ai_results cover_step_in_action::do_action(npc* np) const
{
    if (human_combat_cover_get_node(np) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    return np->get_ai()->action_timer.elapsed() ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

// 0x005E3EC0 (:1613)
int cover_step_out_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 20;
}

// 0x00658260 (:1618)
ai_results cover_step_out_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    const object* target = World.object_from_handle(ai->perception.get_attention_handle());
    if (target == nullptr || target->get_obj_type() != OT_HUMAN)
    {
        target = nullptr;
        if (!activity_raid_npc_should_prepare(np))
        {
            return AI_RESULT_FAILURE;
        }
    }
    cover_node* node = human_combat_cover_get_node(np);
    if (node == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai->planner.lock_action(true);
    if (human_combat_cover_is_stepped_out(np))
    {
        ai->action_timer.invalidate();
        return AI_RESULT_SUCCESS;
    }
    const cover_node_stances stance = cover_stance_from_state(np);
    cover_node_firing_positions position = ai->desired_cover_firing_position;
    if (position == COVER_FIRE_UNDEFINED)
    {
        position = cover_pick_firing_position(stance, node, target != nullptr ? target->pos : ai->desired_target_pos);
        if (position == COVER_FIRE_UNDEFINED)
        {
            human_combat_cover_abandon_node(np);
            node->disable(COVER_DISABLED_NO_STEPOUTS, -1);
            return AI_RESULT_FAILURE;
        }
        ai->desired_cover_firing_position = position;
    }
    ai->stop_moving();
    np->char_inst->stop_action(ANIM_ALIAS_TURN_LEFT, 3, false);
    np->char_inst->stop_action(ANIM_ALIAS_TURN_RIGHT, 3, false);
    human_set_combat_ready(np, true, 0);
    cover_get_stepped_out_fvec(np->steering_vector, stance, position, node);
    np->rotate_dir = stance == COVER_STANCE_CROUCH_LEFT || stance == COVER_STANCE_STAND_LEFT ? HUMAN_ROTATE_CLOCKWISE : HUMAN_ROTATE_COUNTERCLOCKWISE;
    human_combat_cover_set_stepped_out_state(np, stance, position);
    // Magic number: 0.3 seconds to step out.
    ai->action_timer.set(300);
    ai->perception.set_attention_mode(ATTENTION_USE_CURRENT_MODE, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    return AI_RESULT_SUCCESS;
}

// 0x006588D0 (:1694)
ai_results cover_step_out_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const object* attention_target = World.object_from_handle(ai->perception.get_attention_handle());
    const human* target = attention_target != nullptr && attention_target->get_obj_type() == OT_HUMAN ? (const human*)attention_target : nullptr;
    if (human_combat_cover_get_node(np) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    const attention_look_at_modes look_at_mode = get_stepped_out_attention_mode(ai, target, true);
    // Magic number: settle on the aim for the last 0.2 seconds of the step.
    if (ai->perception.get_attention_mode() != look_at_mode && ai->action_timer.time_until() < 200)
    {
        ai->perception.set_attention_mode(look_at_mode, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    }
    if (ai->action_timer.elapsed())
    {
        ai->out_of_cover_timer.set(0);
        return AI_RESULT_SUCCESS;
    }
    return AI_RESULT_IN_PROGRESS;
}

// The rest, constructors and macro get_cost overloads, from tools/headless/ai_action_gen.py.

go_to_cover_action go_to_cover_action::the_instance; // 0x01DDB7D0

// 0x00617310 (.h:13)
go_to_cover_action::go_to_cover_action()
    : ai_action("go to cover", ai_plan_conditions_from_bits({4, 9, 47, 136, 137, 145, 153}), ai_plan_conditions_from_bits({59}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* go_to_cover_action::get_instance()
{
    return &the_instance;
}

// 0x005E2AF0
go_to_cover_action::~go_to_cover_action()
{
}

// 0x006175A0 (.h:13)
int go_to_cover_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

leave_cover_action leave_cover_action::the_instance; // 0x01D68530

// 0x006175D0 (.h:14)
leave_cover_action::leave_cover_action()
    : ai_action("leave cover", ai_plan_conditions(), ai_plan_conditions_from_bits({58}))
{
    flags.has_enter = 1;
}

ai_action* leave_cover_action::get_instance()
{
    return &the_instance;
}

// 0x005E2B00
leave_cover_action::~leave_cover_action()
{
}

// 0x00617620 (.h:14)
int leave_cover_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

attack_cover_action attack_cover_action::the_instance; // 0x01D68268

// 0x00617D50 (.h:25)
attack_cover_action::attack_cover_action()
    : ai_action("attack from cover", ai_plan_conditions_from_bits({0, 2, 62, 136, 137, 153}), ai_plan_conditions_from_bits({78}))
{
    flags.has_enter = 1;
}

ai_action* attack_cover_action::get_instance()
{
    return &the_instance;
}

// 0x005E2B70
attack_cover_action::~attack_cover_action()
{
}

// 0x00617F80 (.h:25)
int attack_cover_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

reload_cover_action reload_cover_action::the_instance; // 0x01D7512C

// 0x00617C30 (.h:24)
reload_cover_action::reload_cover_action()
    : ai_action("reload in cover", ai_plan_conditions_from_bits({2, 61}), ai_plan_conditions_from_bits({0, 1}))
{
    flags.has_enter = 1;
}

ai_action* reload_cover_action::get_instance()
{
    return &the_instance;
}

// 0x005E2B60
reload_cover_action::~reload_cover_action()
{
}

// 0x00617D20 (.h:24)
int reload_cover_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

attack_defensive_cover_action attack_defensive_cover_action::the_instance; // 0x01DA0298

// 0x00617FB0 (.h:26)
attack_defensive_cover_action::attack_defensive_cover_action()
    : ai_action("attack /w def. cover", ai_plan_conditions_from_bits({0, 2, 59}), ai_plan_conditions_from_bits({78}))
{
    flags.has_enter = 1;
}

ai_action* attack_defensive_cover_action::get_instance()
{
    return &the_instance;
}

// 0x005E2B90
attack_defensive_cover_action::~attack_defensive_cover_action()
{
}

// 0x006180B0 (.h:26)
int attack_defensive_cover_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

reload_defensive_cover_action reload_defensive_cover_action::the_instance; // 0x01DC9588

// 0x006180E0 (.h:27)
reload_defensive_cover_action::reload_defensive_cover_action()
    : ai_action("reload /w def. cover", ai_plan_conditions_from_bits({2, 59}), ai_plan_conditions_from_bits({0, 1}))
{
    flags.has_enter = 1;
}

ai_action* reload_defensive_cover_action::get_instance()
{
    return &the_instance;
}

// 0x005E2BA0
reload_defensive_cover_action::~reload_defensive_cover_action()
{
}

// 0x006181D0 (.h:27)
int reload_defensive_cover_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

cover_step_in_action cover_step_in_action::the_instance; // 0x01DCA5D4

// 0x00617A10 (.h:19)
cover_step_in_action::cover_step_in_action()
    : ai_action("cover step in", ai_plan_conditions_from_bits({60}), ai_plan_conditions_from_bits({61}))
{
    flags.has_enter = 1;
}

ai_action* cover_step_in_action::get_instance()
{
    return &the_instance;
}

// 0x005E2B30
cover_step_in_action::~cover_step_in_action()
{
}

// 0x00617A70 (.h:19)
int cover_step_in_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

cover_step_out_action cover_step_out_action::the_instance; // 0x01D9F63C

// 0x00617970 (.h:18)
cover_step_out_action::cover_step_out_action()
    : ai_action("cover step out", ai_plan_conditions_from_bits({60}), ai_plan_conditions_from_bits({62}))
{
    flags.has_enter = 1;
}

ai_action* cover_step_out_action::get_instance()
{
    return &the_instance;
}

// 0x005E2B20
cover_step_out_action::~cover_step_out_action()
{
}

// 0x006179D0 (.h:18)
int cover_step_out_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

transition_to_cover_action transition_to_cover_action::the_instance; // 0x01DCA5A0

// 0x00617660 (.h:17)
transition_to_cover_action::transition_to_cover_action()
    : ai_action("transition to cover", ai_plan_conditions_from_bits({9, 47, 59, 136, 137, 145, 153}), ai_plan_conditions_from_bits({60, 61}))
{
    flags.has_enter = 1;
}

ai_action* transition_to_cover_action::get_instance()
{
    return &the_instance;
}

// 0x005E2B10
transition_to_cover_action::~transition_to_cover_action()
{
}

// 0x00617940 (.h:17)
int transition_to_cover_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

cover_switch_stance_action cover_switch_stance_action::the_instance; // 0x01DA039C

// 0x00617AB0 (.h:20)
cover_switch_stance_action::cover_switch_stance_action()
    : ai_action("cover switch stance", ai_plan_conditions_from_bits({61}), ai_plan_conditions_from_bits({63}))
{
    flags.has_enter = 1;
}

ai_action* cover_switch_stance_action::get_instance()
{
    return &the_instance;
}

// 0x005E2B40
cover_switch_stance_action::~cover_switch_stance_action()
{
}

// 0x00617B10 (.h:20)
int cover_switch_stance_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

dodge_cover_action dodge_cover_action::the_instance; // 0x01D68410

// 0x00617B50 (.h:23)
dodge_cover_action::dodge_cover_action()
    : ai_action("dodge cover", ai_plan_conditions_from_bits({61}), ai_plan_conditions_from_bits({82, 117}))
{
}

ai_action* dodge_cover_action::get_instance()
{
    return &the_instance;
}

// 0x005E2B50
dodge_cover_action::~dodge_cover_action()
{
}

// 0x00617C00 (.h:23)
int dodge_cover_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

