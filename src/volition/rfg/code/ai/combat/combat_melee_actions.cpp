// Original: volition/rfg/code/ai/combat/combat_melee_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/combat/combat_melee_actions.h"

#include "volition/rfg/code/ai/ai_defense_coordination.h"
#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/combat/combat_cover.h"
#include "volition/rfg/code/ai/combat/cover_selection.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/ai/threaten_actions.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_cover.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"

#include <math.h>

// Position 12 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG12")

// Magic numbers: how far a melee cover search goes (tweak table items, zero until tweak_table.xtbl loads).
static float Ai_cover_dist_melee; // 0x01D67194
static float Ai_cover_dist_melee_no_npc_damage; // 0x01D67D9C
static tweak_table_item Tweak_table_item_Ai_cover_dist_melee("Ai_cover_dist_melee", &Ai_cover_dist_melee); // 0x01DA06DC
static tweak_table_item Tweak_table_item_Ai_cover_dist_melee_no_npc_damage("Ai_cover_dist_melee_no_npc_damage", &Ai_cover_dist_melee_no_npc_damage); // 0x01D755C4

static const unsigned int NO_MELEE_CONDITION = 78; // a goal pursuing this condition rules out melee
static const unsigned int THREATEN_START_CONDITION = 102;
static const float MELEE_FACING_COS = 0.8660254f; // cos(30 degrees)
static const float SIGHTED_ENEMY_RELEVANCE = 1.0f; // as in percept.cpp
static const int ANIM_STATE_MELEE_BLOCK = 0xA0; // numeric index in rfg.exe, name unknown
static const int ANIM_STATE_MELEE_BLOCK_WALK = 0xA1; // numeric index in rfg.exe, name unknown

// Inlined in rfg.exe, block with the grenade weapon check in the get_cost functions.
static bool npc_has_grenade(const npc* np)
{
    const inventory_item* item = inv_item_get_weapon_from_class(np, WEAPON_CLASS_THROWN_GRENADE);
    return item != nullptr && World.object_from_handle<weapon>(item->weapon_handle) != nullptr;
}

// get_attention_target<human> (0x005F8BE0), inlined.
static human* get_attention_human(const ai_system* ai)
{
    object* objp = ai->perception.get_attention_target();
    return objp != nullptr && objp->get_obj_type() == OT_HUMAN ? static_cast<human*>(objp) : nullptr;
}

// ai_defense_get_threaten_stage(handle, nullptr) inlined, with the goal test all its users have.
static bool is_dash_and_bash(const ai_goal* goal, unsigned int def_handle)
{
    return goal == confront_civilian_goal::get_instance() && def_handle != 0xFFFFFFFF && ai_defense_get_threaten_stage(def_handle, nullptr) == AI_DEFENSE_THREATEN_DASH_AND_BASH;
}

// 0x00678000 (:123)
static bool do_threaten_start_common(npc* np)
{
    const ai_system* ai = np->get_ai();
    if (is_dash_and_bash(ai->planner.get_current_goal(), ai->desired_defense_assignment_handle))
    {
        return ai_defense_report_threaten_start(ai->desired_defense_assignment_handle);
    }
    return true;
}

// 0x00678090 (:156). LTCG: target ESI; the callers' decompile hides it, every caller passes the attention target.
static void do_beatdown_line_common(npc* np, const human* target, bool force_play)
{
    if (target == nullptr)
    {
        return;
    }
    const unsigned int def_handle = np->get_ai()->desired_defense_assignment_handle;
    if (!force_play && vec_dist_squared(np->pos, target->pos) >= 9.0f) // 3 m
    {
        return;
    }
    if (ai_defense_should_play_beatdown_line(def_handle, target) && np->play_voice_line(VOICE_LINE_RESTRICTED_INITIATE_BEATDOWN, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false) == PLAY_VOICE_SUCCESS)
    {
        ai_defense_report_played_beatdown_line(def_handle, target);
    }
}

// 0x00678110 (:175)
static void do_threaten_failure(const npc* np)
{
    const ai_system* ai = np->get_ai();
    if (is_dash_and_bash(ai->planner.get_current_goal(), ai->desired_defense_assignment_handle))
    {
        ai_defense_report_no_path(ai->desired_defense_assignment_handle);
    }
}

// 0x005E6220 (:200). LTCG: np ECX, target EAX. rfg.exe inlines it in melee_action::enter.
static bool should_chase_melee_target(const npc* np, const human* target)
{
    return 1.5f <= target->actual_velocity * np->steering_vector;
}

// 0x006781A0 (:220)
int path_to_melee_range_action::get_cost(ai_action_setup_data&, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    if (goal_setup_data.goal->pursues_condition(ai_plan_conditions(NO_MELEE_CONDITION)))
    {
        return -1;
    }
    if (np->current_team == HUMAN_TEAM_MARAUDER && npc_has_grenade(np))
    {
        const human* hp = get_attention_human(np->get_ai());
        if (hp == nullptr || human_in_vehicle(hp) != nullptr)
        {
            return -1;
        }
    }
    if (is_dash_and_bash(goal_setup_data.goal, goal_setup_data.defense_assignment_handle))
    {
        desired_conditions |= ai_plan_conditions(THREATEN_START_CONDITION);
    }
    return 1000;
}

// 0x005E6290 (:255)
void path_to_melee_range_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x00678330 (:261)
ai_results path_to_melee_range_action::enter(npc* np, const ai_action* prev_action) const
{
    const human* hp = get_attention_human(np->get_ai());
    if (hp == nullptr || hp->is_killed() || human_in_vehicle(hp) != nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (prev_action != this && !do_threaten_start_common(np))
    {
        return AI_RESULT_FAILURE;
    }
    return AI_RESULT_SUCCESS;
}

// 0x006BC160 (:280)
ai_results path_to_melee_range_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human* target = get_attention_human(ai);
    if (target == nullptr || target->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    const bool oppressing = ai->planner.get_current_goal() == oppress_civilian_goal::get_instance();
    if (oppressing)
    {
        if (!ai_threaten_target_is_valid(np, target))
        {
            ai->stop_moving();
            return AI_RESULT_FAILURE;
        }
        if (object_is_player(target) && human_weapons_is_obvious_weapon_equipped(target) && ai->planner.get_time_in_current_goal() > 10000) // ms
        {
            ai->perception.note_possible_enemy_location(target, SIGHTED_ENEMY_RELEVANCE, true);
            return AI_RESULT_SUCCESS;
        }
    }
    do_beatdown_line_common(np, target, false);
    if (ai->perception.is_in_melee_move_range(target, ai->desired_melee_move, nullptr) && (!np->combat.melee_attack_interval_timer.valid() || np->combat.melee_attack_interval_timer.elapsed()))
    {
        return AI_RESULT_SUCCESS;
    }
    if (ai->can_pathfind())
    {
        const ai_pathfind_results result = ai->pathfind_to(target, oppressing ? HMS_WALK : HMS_RUN, HST_STAND, AI_PATHFIND_PRIORITY_HIGH, nullptr);
        if (result == AI_PF_SUCCESS)
        {
            const path_node* last = np->pf.path != nullptr ? np->pf.path->prev : nullptr;
            if (last != nullptr && last->nav_cell_handle != 0xFFFFFFFFFFFFFFFFull && np->hflags.recalculate_at_node == 0 && !ai->perception.is_in_melee_move_range(target, ai->desired_melee_move, &last->pos))
            {
                ai->stop_moving();
                do_threaten_failure(np);
                ai->planner.avoid_action(this, 10000); // ms
                return AI_RESULT_FAILURE;
            }
        }
        else if (result > AI_PF_RAGDOLLED)
        {
            do_threaten_failure(np);
            ai->planner.avoid_action(this, 5000); // ms
            return AI_RESULT_FAILURE;
        }
    }
    if (!ai->flags.less_aggressive_melee)
    {
        human_set_combat_ready(np, true, 0);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x0065A430 (:357)
int circle_melee_target_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    const unsigned int handle = np->combat.target_handle;
    const object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->is_killed())
    {
        return -1;
    }
    if (human_in_vehicle(static_cast<const human*>(objp)) != nullptr && (np->current_team != HUMAN_TEAM_MARAUDER || !npc_has_grenade(np)))
    {
        return -1;
    }
    const weapon* wp = np->get_equipped_weapon();
    if (wp == nullptr || wp->w_info->ammo_type != WEAPON_AMMO_TYPE_MELEE)
    {
        return -1;
    }
    return np->get_ai()->perception.enemy_in_los(goal_setup_data.selected_target->get_handle()) ? 5000 : -1;
}

// 0x0065A500 (:398)
ai_results circle_melee_target_action::enter(npc* np, const ai_action* prev_action) const
{
    ai_system* ai = np->get_ai();
    const object* objp = ai->perception.get_attention_target();
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    if (prev_action != this)
    {
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        ai->action_timer.set(0);
        ai->desired_goal_pos = ZERO_VECTOR;
        ai->desired_target_pos.x = fl_rand_range(0.0f, 6.2831855f); // the circling angle, 0 to 2 pi
        ai->action_substate = 0;
        human_set_stance(np, HST_STAND);
    }
    return AI_RESULT_SUCCESS;
}

// 0x00699350 (:424)
ai_results circle_melee_target_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human_set_combat_ready(np, true, 0);
    const object* target = ai->perception.get_attention_target();
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (!(ai->desired_goal_pos == ZERO_VECTOR))
    {
        ai->action_substate = 0;
        const ai_pathfind_results result = ai->pathfind_to(ai->desired_goal_pos, HMS_RUN, HST_CURRENT, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false);
        if (result == AI_PF_SUCCESS)
        {
            if (vec_dist_squared(np->pos, ai->desired_goal_pos) < 4.0f) // 2 m
            {
                ai->desired_goal_pos = ZERO_VECTOR;
                ai->action_timer.set(3000); // ms
                ai->stop_moving();
            }
        }
        else if (result > AI_PF_RAGDOLLED)
        {
            ai->stop_moving();
            ai->action_timer.set(0);
        }
        return AI_RESULT_IN_PROGRESS;
    }
    if (ai->action_substate == 0)
    {
        np->steering_vector = (target->pos - np->pos).copy_flatten_normalize_safe();
        ai->action_substate = 1;
    }
    if (ai->action_timer.elapsed())
    {
        vector offset = RIGHT_VECTOR;
        ai->desired_target_pos.x += fl_rand_range(-0.7853982f, 0.7853982f); // up to 45 degrees further around
        offset.rotate_around_y_axis(ai->desired_target_pos.x);
        offset *= fl_rand_range(0.0f, 10.0f) + 10.0f; // 10 to 20 m
        vector goal = target->pos + offset;
        havok_terrain_height(goal.y, goal.x, goal.z, false, nullptr);
        nav_cell* cell = nullptr;
        vector on_mesh;
        if (bb_pf_find_closest_point_on_nav_mesh_fast(on_mesh, cell, goal, nullptr, nullptr, false, 2.0f))
        {
            ai->desired_goal_pos = on_mesh;
        }
        else
        {
            ai->desired_target_pos.x = fl_rand_range(0.0f, 6.2831855f);
            ai->desired_goal_pos = ZERO_VECTOR;
        }
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x006783A0 (:498)
int move_to_melee_range_action::get_cost(ai_action_setup_data&, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const
{
    const ai_system* ai = np->get_ai();
    const object* selected = goal_setup_data.selected_target;
    if (selected == nullptr || selected->get_obj_type() != OT_HUMAN || goal_setup_data.goal->pursues_condition(ai_plan_conditions(NO_MELEE_CONDITION)))
    {
        return -1;
    }
    const human* hp = static_cast<const human*>(selected);
    if (np->current_team == HUMAN_TEAM_MARAUDER && npc_has_grenade(np))
    {
        const human* attention = get_attention_human(ai);
        if (attention == nullptr || human_in_vehicle(attention) != nullptr)
        {
            return -1;
        }
    }
    if (is_dash_and_bash(goal_setup_data.goal, goal_setup_data.defense_assignment_handle))
    {
        desired_conditions |= ai_plan_conditions(THREATEN_START_CONDITION);
    }
    if (!ai->perception.has_clear_line_of_travel(hp, 10.0f, 1.2f))
    {
        return -1;
    }
    for (const ai_planner_search_node* node = next_action; node != nullptr;)
    {
        const human_melee_combat_move_data* move = node->action_data.melee_move;
        if (move != nullptr)
        {
            vector dir;
            vec_normalized_dir(dir, hp->pos, np->pos);
            if (ai->perception.is_in_melee_move_range(hp, move, nullptr) && MELEE_FACING_COS < np->orient.fvec * dir && (!np->combat.melee_attack_interval_timer.valid() || np->combat.melee_attack_interval_timer.elapsed()))
            {
                return 0;
            }
            return 500;
        }
        node = node->next_node;
        if (node == next_action)
        {
            break;
        }
    }
    return -1;
}

// 0x005E62A0 (:566)
void move_to_melee_range_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x00699750 (:573)
ai_results move_to_melee_range_action::enter(npc* np, const ai_action* prev_action) const
{
    ai_system* ai = np->get_ai();
    human* hp = get_attention_human(ai);
    if (hp == nullptr || hp->is_killed() || human_in_vehicle(hp) != nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (!ai->flags.less_aggressive_melee)
    {
        human_set_combat_ready(np, true, 0);
    }
    if (prev_action != this)
    {
        if (!do_threaten_start_common(np))
        {
            return AI_RESULT_FAILURE;
        }
        const bool oppressing = ai->planner.get_current_goal() == oppress_civilian_goal::get_instance();
        if (oppressing && !object_is_player(hp))
        {
            np->play_voice_line(VOICE_LINE_PATROL_BEAT_CIVILIAN, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        }
        const float dist_sq = vec_dist_squared(hp->pos, np->pos);
        const bool marauder = np->current_team == HUMAN_TEAM_MARAUDER;
        const bool firearm = human_weapons_is_firearm_equipped(np);
        human_movement_states m_state = HMS_WALK;
        bool stand_still = false;
        if (marauder || !firearm || dist_sq > 6.25f) // 2.5 m
        {
            ai->perception.pay_attention(static_cast<const object*>(hp), ai->flags.less_aggressive_melee ? ATTENTION_LOOK_AT_OBJECT : ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
            if (!oppressing && (!firearm || dist_sq >= 9.0f)) // 3 m
            {
                m_state = HMS_RUN;
            }
            if (marauder && ai->planner.plan_contains(melee_block_action::get_instance()))
            {
                m_state = HMS_WALK;
                np->char_inst->set_state(index_alias(ANIM_STATE_MELEE_BLOCK_WALK, (vanim_aliases)-1), index_alias(-1, (vanim_aliases)-1), 0.3f, false);
                human_override_default_animation_state(np, true);
            }
        }
        else
        {
            ai->perception.pay_attention(static_cast<const object*>(hp), ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
            stand_still = ai->perception.is_in_melee_move_range(hp, ai->desired_melee_move, nullptr);
        }
        if (stand_still)
        {
            human_set_state(np, HS_STANDARD, HMS_IDLE, -1.0f);
        }
        else
        {
            ai->set_direct_movement_fast((hp->pos - np->pos).copy_flatten_normalize_safe(), m_state, HST_STAND, -1.0f);
        }
        ai->action_timer.set(ai->oppression_action < 1 ? 1500 : 10000); // ms
    }
    np->steering_vector = hp->pos - np->pos;
    np->steering_vector.flatten_normalize_safe();
    return AI_RESULT_SUCCESS;
}

// 0x006BC350 (:662)
ai_results move_to_melee_range_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (!ai->flags.less_aggressive_melee)
    {
        human_set_combat_ready(np, true, 0);
    }
    ai_combat_action_parameters params;
    params.init_npc(np);
    if (np->current_team != HUMAN_TEAM_MARAUDER && ai->action_timer.elapsed() && ai_combat_should_charge_target(np) != AI_COMBAT_MUST_CHARGE && params.weapon_inst != nullptr && params.weapon_inst->w_info->ammo_type != WEAPON_AMMO_TYPE_MELEE)
    {
        np->combat.move_to_melee_timestamp.set_rand(5000, 8000);
        do_threaten_failure(np);
        return AI_RESULT_FAILURE;
    }
    if (!params.init_target(true))
    {
        return AI_RESULT_FAILURE;
    }
    const bool oppressing = ai->planner.get_current_goal() == oppress_civilian_goal::get_instance();
    if (oppressing)
    {
        if (!ai_threaten_target_is_valid(np, params.target))
        {
            ai->stop_moving();
            return AI_RESULT_FAILURE;
        }
        if (object_is_player(params.target) && human_weapons_is_obvious_weapon_equipped(params.target) && ai->planner.get_time_in_current_goal() > 10000) // ms
        {
            ai->perception.note_possible_enemy_location(params.target, SIGHTED_ENEMY_RELEVANCE, true);
            return AI_RESULT_SUCCESS;
        }
    }
    do_beatdown_line_common(np, params.target, false);
    const bool in_range = ai->perception.is_in_melee_move_range(params.target, ai->desired_melee_move, nullptr);
    if (MELEE_FACING_COS < np->orient.fvec * params.target_dir.copy_flatten_normalize())
    {
        if (in_range)
        {
            if (human_weapons_is_firearm_equipped(np))
            {
                ai->set_desired_burst_size(0);
            }
            if (!np->combat.melee_attack_interval_timer.valid() || np->combat.melee_attack_interval_timer.elapsed())
            {
                return AI_RESULT_SUCCESS;
            }
        }
        else
        {
            const float dx = np->pos.x - params.target->pos.x;
            const float dz = np->pos.z - params.target->pos.z;
            // Original quirk kept: a squared distance compared with a distance. Over a ledge the target can't be reached.
            if (dx * dx + dz * dz <= ai->desired_melee_move->ai_max_dist && 1.2f < fabsf(np->pos.y - params.target->pos.y))
            {
                return AI_RESULT_FAILURE;
            }
            if (human_weapons_is_firearm_equipped(np) && ai->perception.get_attention_mode() == ATTENTION_AIM_AT_OBJECT)
            {
                ai_combat_fire_at_target(params, false);
            }
        }
    }
    const attention_look_at_modes wanted_mode = ai->flags.less_aggressive_melee ? ATTENTION_LOOK_AT_OBJECT : ATTENTION_AIM_AT_OBJECT;
    bool set_state = false;
    human_movement_states movement_state = HMS_IDLE;
    if (params.dist_to_target > 3.5f && np->cur_movement_state <= HMS_WALK)
    {
        set_state = true;
        movement_state = oppressing ? HMS_WALK : HMS_RUN;
    }
    else if (in_range)
    {
        set_state = np->cur_movement_state > HMS_IDLE;
    }
    else if (np->cur_movement_state == HMS_IDLE)
    {
        set_state = true;
        movement_state = oppressing ? HMS_WALK : HMS_RUN;
    }
    if (set_state)
    {
        if (ai->perception.get_attention_mode() != wanted_mode)
        {
            ai->perception.set_attention_mode(wanted_mode, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        }
        human_set_state(np, HS_STANDARD, movement_state, -1.0f);
    }
    np->steering_vector = params.target_dir;
    np->steering_vector.flatten_normalize_safe();
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E62B0 (:778)
ai_results move_to_melee_range_action::cleanup(npc* np, const ai_action* next_action) const
{
    ai_system* ai = np->get_ai();
    if (!ai->flags.less_aggressive_melee)
    {
        human_set_combat_ready(np, true, 0);
    }
    if (next_action != this)
    {
        human_override_default_animation_state(np, false);
        if (next_action != melee_action::get_instance() && next_action != beat_civilian_action::get_instance())
        {
            ai->stop_moving();
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x00699AA0 (:805)
int melee_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    const object* selected = goal_setup_data.selected_target;
    if (selected == nullptr || selected->get_obj_type() != OT_HUMAN || selected->is_killed() || human_in_vehicle(static_cast<const human*>(selected)) != nullptr)
    {
        return -1;
    }
    const weapon_info* w_info;
    const weapon* wp = human_weapons_get_weapon(np);
    if (wp != nullptr && wp->turret_handle == 0xFFFFFFFF)
    {
        w_info = wp->w_info;
    }
    else
    {
        const inventory_item* item = inv_item_get_best_weapon(np, false);
        wp = item != nullptr ? World.object_from_handle<weapon>(item->weapon_handle) : nullptr;
        if (wp == nullptr)
        {
            return -1;
        }
        w_info = wp->w_info;
        if (w_info->ammo_type == WEAPON_AMMO_TYPE_BULLET || w_info->ammo_type == WEAPON_AMMO_TYPE_PROJECTILE)
        {
            desired_conditions |= ai_plan_conditions(2);
        }
        else if (w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE)
        {
            desired_conditions |= ai_plan_conditions(3);
        }
    }
    const ai_perception& perception = np->get_ai()->perception;
    my_data.melee_move = perception.get_melee_move_in_range(selected, w_info, false);
    if (my_data.melee_move == nullptr)
    {
        my_data.melee_move = perception.get_melee_move_in_range(selected, w_info, true);
    }
    return my_data.melee_move != nullptr ? 400 : -1;
}

// 0x005E6310 (:869)
void melee_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    np->get_ai()->desired_melee_move = setup_data.melee_move;
}

// 0x00699BF0 (:875)
ai_results melee_action::enter(npc* np, const ai_action* prev_action) const
{
    ai_system* ai = np->get_ai();
    if (human_combat_is_blocking_melee(np))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    const human* hp = get_attention_human(ai);
    if (hp == nullptr || hp->is_killed() || human_in_vehicle(hp) != nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (should_chase_melee_target(np, hp))
    {
        ai->set_direct_movement_fast((hp->pos - np->pos).copy_flatten_normalize_safe(), HMS_RUN, HST_STAND, -1.0f);
        ai->action_substate = 1;
    }
    else
    {
        ai->action_substate = 0;
        ai->stop_moving();
        human_set_stance(np, HST_STAND);
    }
    if (!ai->flags.less_aggressive_melee)
    {
        human_set_combat_ready(np, true, 0);
    }
    if (prev_action != this && !do_threaten_start_common(np))
    {
        return AI_RESULT_FAILURE;
    }
    ai->perception.pay_attention(hp->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    vector dir = hp->pos - np->pos;
    dir.y = 0.0f;
    human_combat_play_combat_move(np, ai->desired_melee_move, false, true);
    matrix dest_orient;
    dest_orient.make(dir);
    np->char_inst->set_action_starting_orient(np->pos, np->orient, np->pos, dest_orient, ANIM_ALIAS_COMBAT_MOVE, 0.1f);
    np->combat.move_to_melee_timestamp.invalidate();
    ai->planner.lock_action(true);
    return AI_RESULT_SUCCESS;
}

// 0x00678650 (:935)
ai_results melee_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (!ai->flags.less_aggressive_melee)
    {
        human_set_combat_ready(np, true, 0);
    }
    const human* target = get_attention_human(ai);
    if (target == nullptr)
    {
        return np->char_inst->is_playing_action(ANIM_ALIAS_COMBAT_MOVE) ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
    }
    if (np->cur_movement_state > HMS_IDLE && ai->action_substate > 0)
    {
        np->steering_vector = (target->pos - np->pos).copy_flatten_normalize_safe();
    }
    do_beatdown_line_common(np, target, false);
    if (np->char_inst->is_playing_action(ANIM_ALIAS_COMBAT_MOVE))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    const weapon* wp = human_weapons_get_weapon(np);
    if (wp != nullptr && wp->char_inst != nullptr && wp->char_inst->is_playing_action(ANIM_ALIAS_COMBAT_MOVE))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    ai->planner.lock_action(false);
    if (is_dash_and_bash(ai->planner.get_current_goal(), ai->desired_defense_assignment_handle))
    {
        ai_defense_report_threaten_done(ai->desired_defense_assignment_handle);
    }
    return AI_RESULT_SUCCESS;
}

// 0x00699E80 (:991)
int melee_block_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const ai_system* ai = np->get_ai();
    if (np->current_team != HUMAN_TEAM_MARAUDER)
    {
        return -1;
    }
    const human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        return -1;
    }
    const weapon* target_weapon = target->get_equipped_weapon();
    if (target_weapon == nullptr || target_weapon->w_info->ammo_type != WEAPON_AMMO_TYPE_MELEE)
    {
        return -1;
    }
    const weapon* wp = np->get_equipped_weapon();
    if (wp == nullptr || wp->w_info->ammo_type != WEAPON_AMMO_TYPE_MELEE)
    {
        return -1;
    }
    // A block that has just begun goes on for 1.5 s, otherwise half of the replans start or keep one.
    if ((ai->planner.get_current_action() != this || ai->planner.get_time_in_current_action() >= 1501) && int_rand_chance(50))
    {
        return -1;
    }
    my_data.melee_move = ai->perception.get_melee_move_in_range(target, wp->w_info, false);
    if (my_data.melee_move == nullptr)
    {
        my_data.melee_move = ai->perception.get_melee_move_in_range(target, wp->w_info, true);
    }
    return my_data.melee_move != nullptr ? 50 : -1;
}

// 0x005E6330 (:1037)
void melee_block_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    np->get_ai()->desired_melee_move = setup_data.melee_move;
}

// 0x00699F90 (:1043)
ai_results melee_block_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        const human* target = human_combat_get_target(np);
        if (target == nullptr)
        {
            return AI_RESULT_FAILURE;
        }
        ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        ai->stop_moving();
        human_set_stance(np, HST_STAND);
        np->char_inst->set_state(index_alias(ANIM_STATE_MELEE_BLOCK, (vanim_aliases)-1), index_alias(-1, (vanim_aliases)-1), 0.3f, false);
        human_override_default_animation_state(np, true);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E6350 (:1065)
ai_results melee_block_action::do_action(npc* np) const
{
    const human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const vector dir = (target->pos - np->pos).copy_flatten_normalize_safe();
    if (np->steering_vector == ZERO_VECTOR || np->steering_vector * dir < 0.0f)
    {
        np->steering_vector = dir;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E64B0 (:1081)
ai_results melee_block_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        human_override_default_animation_state(np, false);
    }
    return AI_RESULT_SUCCESS;
}

// 0x006C2740 (:1095)
int melee_stay_in_cover_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const ai_system* ai = np->get_ai();
    if (human_in_vehicle_interaction(np, nullptr))
    {
        return -1;
    }
    if (np->hflags.riot_shield != 0 && np->shield_handle != 0xFFFFFFFF)
    {
        return -1;
    }
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed())
    {
        return -1;
    }
    const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, target);
    if (belief == nullptr)
    {
        return -1;
    }
    cover_node* node = human_combat_cover_get_node(np);
    vector target_pos;
    if (!ai->perception.get_effective_target_pos(target_pos, target, nullptr, nullptr, false))
    {
        return -1;
    }
    if (node == nullptr || !node->will_provide_defensive_cover(np, target_pos, false))
    {
        const vehicle* target_vehicle = World.object_from_handle<vehicle>(target->vehicle_handle);
        if (target_vehicle != nullptr && target_vehicle->get_sub_type() == OT_SUB_VEHICLE_FLYER)
        {
            return -1;
        }
        const bool allowed_to_damage = human_combat_allowed_to_damage_target(np, target);
        cover_search_parameters params(np, target_pos, allowed_to_damage ? Ai_cover_dist_melee : Ai_cover_dist_melee_no_npc_damage, true, node, allowed_to_damage);
        node = cover_selection_do_search(params);
        if (node != nullptr)
        {
            desired_conditions |= ai_plan_conditions(59) | ai_plan_conditions(60);
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
    my_data.cover_stance = stance;
    my_data.use_target = node;
    return 50;
}

// 0x005E64D0 (:1178)
void melee_stay_in_cover_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    human_combat_cover_reserve_node(np, static_cast<cover_node*>(setup_data.use_target));
    np->get_ai()->desired_cover_stance = setup_data.cover_stance;
}

// 0x0065A620 (:1185)
ai_results melee_stay_in_cover_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    }
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    if (!human_combat_allowed_to_damage_target(np, target))
    {
        ai->action_timer.invalidate();
    }
    else if (!ai->action_timer.valid())
    {
        ai->action_timer.set_rand(4000, 8000);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E6500 (:1211)
ai_results melee_stay_in_cover_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    if (ai->action_timer.valid() && ai->action_timer.elapsed())
    {
        cover_node* node = human_combat_cover_get_node(np);
        if (node != nullptr)
        {
            human_combat_cover_abandon_node(np);
            node->disable(COVER_DISABLED_TEMPORARY, 3000); // ms
        }
        return AI_RESULT_FAILURE;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E6580 (:1241)
int path_to_melee_vehicle_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 1000;
}

// 0x0069A020 (:1246)
ai_results path_to_melee_vehicle_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (!ai->flags.less_aggressive_melee)
    {
        human_set_combat_ready(np, true, 0);
    }
    const human* hp = get_attention_human(ai);
    if (hp == nullptr || hp->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    const vehicle* target = human_in_vehicle(hp);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (ai->perception.is_in_melee_move_range(target, ai->desired_melee_move, nullptr))
    {
        return AI_RESULT_SUCCESS;
    }
    if (ai->can_pathfind())
    {
        const ai_pathfind_results result = ai->pathfind_to(target->pos, HMS_RUN, HST_STAND, AI_PATHFIND_PRIORITY_HIGH, 0xFFFFFFFFFFFFFFFFull, false);
        if (result == AI_PF_SUCCESS)
        {
            const path_node* last = np->pf.path != nullptr ? np->pf.path->prev : nullptr;
            if (last != nullptr && last->nav_cell_handle != 0xFFFFFFFFFFFFFFFFull && np->hflags.recalculate_at_node == 0 && !ai->perception.is_in_melee_move_range(target, ai->desired_melee_move, &last->pos))
            {
                ai->stop_moving();
                ai->planner.avoid_action(this, 10000); // ms
                return AI_RESULT_FAILURE;
            }
        }
        else if (result > AI_PF_RAGDOLLED)
        {
            ai->planner.avoid_action(this, 5000); // ms
            return AI_RESULT_FAILURE;
        }
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x0069A170 (:1302)
int melee_vehicle_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    const object* selected = goal_setup_data.selected_target;
    if (selected == nullptr || selected->get_obj_type() != OT_HUMAN)
    {
        return -1;
    }
    const vehicle* target = human_in_vehicle(static_cast<const human*>(selected));
    if (target == nullptr)
    {
        return -1;
    }
    const weapon_info* w_info = human_weapons_get_weapon_info(np);
    if (w_info == nullptr)
    {
        const inventory_item* item = inv_item_get_best_melee(np);
        const weapon* wp = item != nullptr ? World.object_from_handle<weapon>(item->weapon_handle) : nullptr;
        if (wp == nullptr)
        {
            return -1;
        }
        w_info = wp->w_info;
    }
    const ai_perception& perception = np->get_ai()->perception;
    my_data.melee_move = perception.get_melee_move_in_range(target, w_info, false);
    if (my_data.melee_move == nullptr)
    {
        my_data.melee_move = perception.get_melee_move_in_range(target, w_info, true);
    }
    return my_data.melee_move != nullptr ? 500 : -1;
}

// 0x005E6590 (:1339)
void melee_vehicle_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    np->get_ai()->desired_melee_move = setup_data.melee_move;
}

// 0x0069A240 (:1345)
ai_results melee_vehicle_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    const human* hp = get_attention_human(ai);
    if (hp == nullptr || hp->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    const vehicle* vp = human_in_vehicle(hp);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    vector velocity;
    vehicle_get_velocity(vp, velocity);
    const vector dir = (vp->pos - np->pos).copy_flatten_normalize_safe();
    if (velocity * dir <= 1.0f)
    {
        ai->stop_moving();
        human_set_stance(np, HST_STAND);
        ai->action_substate = 0;
    }
    else
    {
        ai->set_direct_movement_fast(dir.copy_flatten_normalize_safe(), HMS_RUN, HST_STAND, -1.0f);
        ai->action_substate = 1;
    }
    if (!ai->flags.less_aggressive_melee)
    {
        human_set_combat_ready(np, true, 0);
    }
    ai->perception.pay_attention(hp->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    vector to_vehicle = vp->pos - np->pos;
    to_vehicle.y = 0.0f;
    human_combat_play_combat_move(np, ai->desired_melee_move, false, true);
    matrix dest_orient;
    dest_orient.make(to_vehicle);
    np->char_inst->set_action_starting_orient(np->pos, np->orient, np->pos, dest_orient, ANIM_ALIAS_COMBAT_MOVE, 0.1f);
    np->combat.move_to_melee_timestamp.invalidate();
    ai->planner.lock_action(true);
    return AI_RESULT_SUCCESS;
}

// 0x00625E60 (:1400)
ai_results melee_vehicle_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (!ai->flags.less_aggressive_melee)
    {
        human_set_combat_ready(np, true, 0);
    }
    const human* hp = get_attention_human(ai);
    if (hp == nullptr || human_in_vehicle(hp) == nullptr)
    {
        return np->char_inst->is_playing_action(ANIM_ALIAS_COMBAT_MOVE) ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
    }
    if (np->cur_movement_state > HMS_IDLE && ai->action_substate > 0)
    {
        np->steering_vector = (hp->pos - np->pos).copy_flatten_normalize_safe();
    }
    if (np->char_inst->is_playing_action(ANIM_ALIAS_COMBAT_MOVE))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    const weapon* wp = human_weapons_get_weapon(np);
    if (wp != nullptr && wp->char_inst != nullptr && wp->char_inst->is_playing_action(ANIM_ALIAS_COMBAT_MOVE))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    ai->planner.lock_action(false);
    return AI_RESULT_SUCCESS;
}

// 0x00625FD0 (:1455)
int circle_vehicle_target_dead_rule::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    if (np->current_team == HUMAN_TEAM_MARAUDER && human_weapons_is_melee_weapon_equipped(np) && npc_has_grenade(np))
    {
        const human* hp = human_combat_get_target(np);
        if (hp != nullptr && human_in_vehicle(hp) != nullptr)
        {
            return 10;
        }
    }
    return -1;
}

path_to_melee_range_action path_to_melee_range_action::the_instance; // 0x01DA03F0

// 0x00624360 (.h:12)
path_to_melee_range_action::path_to_melee_range_action()
    : ai_action("path to melee range", ai_plan_conditions_from_bits({4, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({25}))
{
    flags.has_enter = 1;
}

ai_action* path_to_melee_range_action::get_instance()
{
    return &the_instance;
}

// 0x005E6170
path_to_melee_range_action::~path_to_melee_range_action()
{
}

// 0x00624660 (.h:12)
int path_to_melee_range_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

circle_melee_target_action circle_melee_target_action::the_instance; // 0x01DC94F8

// 0x00624690 (.h:13)
circle_melee_target_action::circle_melee_target_action()
    : ai_action("circle melee target", ai_plan_conditions_from_bits({4, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({25}))
{
    flags.has_enter = 1;
}

ai_action* circle_melee_target_action::get_instance()
{
    return &the_instance;
}

// 0x005E6180
circle_melee_target_action::~circle_melee_target_action()
{
}

// 0x00624990 (.h:13)
int circle_melee_target_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

move_to_melee_range_action move_to_melee_range_action::the_instance; // 0x01D688C8

// 0x006249D0 (.h:14)
move_to_melee_range_action::move_to_melee_range_action()
    : ai_action("move to melee range", ai_plan_conditions_from_bits({4, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({25}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* move_to_melee_range_action::get_instance()
{
    return &the_instance;
}

// 0x005E6190
move_to_melee_range_action::~move_to_melee_range_action()
{
}

// 0x00624CD0 (.h:14)
int move_to_melee_range_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

melee_action melee_action::the_instance; // 0x01D753E0

// 0x00624D00 (.h:15)
melee_action::melee_action()
    : ai_action("melee", ai_plan_conditions_from_bits({9, 25, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({79}))
{
    flags.has_enter = 1;
}

ai_action* melee_action::get_instance()
{
    return &the_instance;
}

// 0x005E61B0
melee_action::~melee_action()
{
}

// 0x00625000 (.h:15)
int melee_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

melee_block_action melee_block_action::the_instance; // 0x01D687F4

// 0x00625030 (.h:16)
melee_block_action::melee_block_action()
    : ai_action("melee block", ai_plan_conditions_from_bits({9, 25, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({79}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* melee_block_action::get_instance()
{
    return &the_instance;
}

// 0x005E61D0
melee_block_action::~melee_block_action()
{
}

// 0x00625330 (.h:16)
int melee_block_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

melee_stay_in_cover_action melee_stay_in_cover_action::the_instance; // 0x01DDB5E0

// 0x00625360 (.h:17)
melee_stay_in_cover_action::melee_stay_in_cover_action()
    : ai_action("melee stay in cover", ai_plan_conditions_from_bits({3, 61}), ai_plan_conditions_from_bits({81}))
{
    flags.has_enter = 1;
}

ai_action* melee_stay_in_cover_action::get_instance()
{
    return &the_instance;
}

// 0x005E61E0
melee_stay_in_cover_action::~melee_stay_in_cover_action()
{
}

// 0x00625410 (.h:17)
int melee_stay_in_cover_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

melee_vehicle_action melee_vehicle_action::the_instance; // 0x01D68678

// 0x00625440 (.h:19)
melee_vehicle_action::melee_vehicle_action()
    : ai_action("melee vs. vehicle", ai_plan_conditions_from_bits({9, 26, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({80}))
{
    flags.has_enter = 1;
}

ai_action* melee_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005E61F0
melee_vehicle_action::~melee_vehicle_action()
{
}

// 0x00625740 (.h:19)
int melee_vehicle_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

path_to_melee_vehicle_action path_to_melee_vehicle_action::the_instance; // 0x01D725F4

// 0x00625770 (.h:20)
path_to_melee_vehicle_action::path_to_melee_vehicle_action()
    : ai_action("path to melee vehicle", ai_plan_conditions_from_bits({3, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({26}))
{
}

ai_action* path_to_melee_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005E6200
path_to_melee_vehicle_action::~path_to_melee_vehicle_action()
{
}

// 0x00625A60 (.h:20)
int path_to_melee_vehicle_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

circle_vehicle_target_dead_rule circle_vehicle_target_dead_rule::the_instance; // 0x01DDB140

// 0x00625AA0 (.h:22)
circle_vehicle_target_dead_rule::circle_vehicle_target_dead_rule()
    : ai_production_rule("circle vehicle = dead target", ai_plan_conditions_from_bits({4, 9, 25, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({78}))
{
}

ai_action* circle_vehicle_target_dead_rule::get_instance()
{
    return &the_instance;
}

// 0x005E6210
circle_vehicle_target_dead_rule::~circle_vehicle_target_dead_rule()
{
}

// 0x00625E30 (.h:22)
int circle_vehicle_target_dead_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

