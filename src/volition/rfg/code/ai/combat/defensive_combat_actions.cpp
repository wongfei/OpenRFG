// Original: volition/rfg/code/ai/combat/defensive_combat_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/combat/defensive_combat_actions.h"

#include "volition/rfg/code/ai/ai_building_logic.h"
#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/combat/combat_actions.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/combat/combat_cover.h"
#include "volition/rfg/code/ai/combat/combat_maneuver.h"
#include "volition/rfg/code/ai/combat/cover_selection.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_raid.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_cover.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/objects/object_guard_node.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/util/varray.h"

#include <math.h>

// Position 15 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG15")

// Cosine of 70 degrees: a dynamic initializer in rfg.exe.
static const float RAID_PREPARE_COVER_NODE_DOT_LIMIT = (float)cos((double)1.2217305f); // 0x01D68128

// How far a search for support cover goes (a tweak table item, zero until tweak_table.xtbl loads).
static float Ai_cover_dist_combat_support; // 0x01D65960
static tweak_table_item Tweak_table_item_Ai_cover_dist_combat_support("Ai_cover_dist_combat_support", &Ai_cover_dist_combat_support); // 0x01D7520C

static bool Combat_maneuver_do_player_line_of_fire_checks = true; // 0x016413E2, .data 1, never written (also in goals.cpp)

// Added when the npc has no cover node yet or is not at it (go to cover, then the conditions of being in it).
static ai_plan_conditions cover_move_conditions()
{
    return ai_plan_conditions(59) | ai_plan_conditions(60) | ai_plan_conditions(62);
}

// 0x005E7610
defensive_cover_info::defensive_cover_info()
{
}

// 0x005E7080 (:47)
bool defensive_cover_info::less_than_or_equal(const defensive_cover_info& first, const defensive_cover_info& second)
{
    return first.eff_dist <= second.eff_dist;
}

// 0x005E70B0 (:129)
float get_defensive_node_cost(const npc* np, const object* node, bool sniper)
{
    const float xz_dist = vec_xz_dist(np->pos, node->pos);
    float cost;
    if (sniper)
    {
        cost = xz_dist - (node->pos.y - np->pos.y); // a sniper prefers to be high up
    }
    else
    {
        cost = fabsf(node->pos.y - np->pos.y) + xz_dist;
    }
    if (Combat_maneuver_do_player_line_of_fire_checks)
    {
        const player* pl = player_get_local_player();
        if (pl != nullptr && human_teams_get_disposition(pl, np) == TEAM_DISPOSITION_ALLIED && combat_maneuver_path_crosses_player_line_of_fire(np, node->pos, nullptr))
        {
            cost += 30.0f;
        }
    }
    if (node->get_obj_type() == OT_COVER_NODE)
    {
        const cover_node* cnp = static_cast<const cover_node*>(node);
        if (!cnp->flags.on_mover)
        {
            cost += 25.0f;
        }
        if (!cnp->flags.fire_popup)
        {
            cost += 15.0f;
        }
    }
    else if (node->get_obj_type() == OT_GUARD_NODE && static_cast<const object_guard_node*>(node)->on_ground_floor())
    {
        cost += 25.0f;
    }
    return cost;
}

// 0x005E7210 (:193)
bool get_defensive_cover_info(defensive_cover_info& info, const npc* np, const vector& target_pos, cover_node* cnp, bool sniper)
{
    if (cnp->is_disabled() || cnp->is_occupied(np) != nullptr || (sniper && !cnp->flags.fire_popup))
    {
        return false;
    }
    info.node = cnp;
    info.eff_dist = get_defensive_node_cost(np, cnp, sniper);
    info.to_target = target_pos - cnp->pos;
    return 0.0f < info.to_target * cnp->orient.fvec;
}

// 0x005E7360 (:226)
bool get_raid_preparation_defensive_cover_info(defensive_cover_info& info, const npc* np, const vector& target_pos, cover_node* cnp, bool sniper)
{
    if (cnp->is_disabled() || cnp->is_occupied(np) != nullptr || (sniper && !cnp->flags.fire_popup))
    {
        return false;
    }
    info.node = cnp;
    info.to_target = target_pos - cnp->pos;
    info.eff_dist = info.to_target * info.to_target;
    info.to_target.normalize();
    return RAID_PREPARE_COVER_NODE_DOT_LIMIT < cnp->orient.fvec * info.to_target;
}

// 0x005E74F0 (:255)
bool do_secondary_cover_evaluation(const defensive_cover_info& info)
{
    vector dir = info.to_target;
    float dist = dir.mag_and_normalize_safe() * 0.5f;
    if (dist >= 20.0f) // m
    {
        dist = 20.0f;
    }
    const vector test_pos = info.node->pos + dir * dist;
    return info.node->check_cover_type(nullptr, test_pos, false, true, false, nullptr) == CCR_OFFENSIVE;
}

// 0x00629970 (:274). LTCG: cnp ECX, sniper AL, np and target_pos on the stack.
static bool cover_node_can_do_defensive_combat(cover_node* cnp, const npc* np, const vector& target_pos, bool sniper)
{
    defensive_cover_info info;
    return get_defensive_cover_info(info, np, target_pos, cnp, sniper) && do_secondary_cover_evaluation(info);
}

static void set_attention_mode_if_different(ai_perception& perception, attention_look_at_modes mode)
{
    if (perception.get_attention_mode() != mode)
    {
        perception.set_attention_mode(mode, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    }
}

// 0x0069BA10 (:322)
static void do_defensive_fire_frame(const ai_combat_action_parameters& params)
{
    npc* np = params.np;
    ai_system* ai = np->get_ai();
    human_set_combat_ready(np, true, 0);
    if (params.in_penetrating_range || ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr)
    {
        set_attention_mode_if_different(ai->perception, ATTENTION_AIM_AT_OBJECT);
        if (ai->perception.can_engage_target_reason(params.target) == AI_ENGAGE_ALLOWED)
        {
            ai_combat_fire_at_target(params, true);
        }
        return;
    }
    set_attention_mode_if_different(ai->perception, ATTENTION_AIM_AT_BELIEF);
    if (human_weapons_is_firearm_equipped(np) && ai->perception.can_engage_target_reason(params.target) == AI_ENGAGE_ALLOWED)
    {
        const ai_belief* belief = ai->perception.get_attention_belief();
        if (belief != nullptr)
        {
            ai_combat_do_suppressing_fire(np, belief->pos);
        }
    }
}

// 0x006299B0 (:362)
human_movement_states get_defensive_go_to_cover_movement(const ai_combat_action_parameters& params)
{
    const npc* np = params.np;
    const ai_system* ai = np->get_ai();
    if (activity_raid_npc_should_prepare(np) || ai->perception.get_attention_mode() == ATTENTION_LOOK_AT_BELIEF)
    {
        return HMS_RUN;
    }
    if (ai->perception.get_belief(AI_BELIEF_SHOT_AT, params.target->get_handle()) != nullptr)
    {
        return HMS_RUN;
    }
    if (fabsf(np->pos.y - params.node->pos.y) > 1.2f)
    {
        return HMS_RUN;
    }
    float dist = vec_dist(np->pos, params.node->pos);
    if (np->movement_mode == HMM_PATHFINDING && np->pf.path != nullptr)
    {
        const float path_dist = vec_dist(np->pf.path->pos, np->pos) + np->pf.path->distance_left;
        if (dist - path_dist < 0.0f)
        {
            dist = path_dist;
        }
    }
    return dist <= 4.0f ? HMS_WALK : HMS_RUN; // m
}

// 0x0069BB90 (:399)
static bool do_defensive_go_to_cover_frame(npc* np, ai_combat_action_parameters* params)
{
    ai_system* ai = np->get_ai();
    ai_combat_action_parameters local_params;
    if (params == nullptr)
    {
        params = &local_params;
    }
    params->init_npc(np);
    if (!activity_raid_npc_should_prepare(np) && !params->init_target(true))
    {
        return false;
    }
    params->node = human_combat_cover_get_node(params->np);
    if (params->node == nullptr)
    {
        return false;
    }
    params->node_to_npc_dir = params->np->pos - params->node->pos;
    params->node_to_npc_dist = params->node_to_npc_dir.mag_and_normalize();
    human_set_combat_ready(np, true, 0);
    const bool first_contact = ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr;
    if (ai->action_timer.elapsed())
    {
        ai->action_timer.set(1000); // ms
        set_attention_mode_if_different(ai->perception, first_contact ? ATTENTION_AIM_AT_OBJECT : ATTENTION_LOOK_AT_BELIEF);
        if (np->movement_mode == HMM_PATHFINDING)
        {
            np->cur_movement_state = get_defensive_go_to_cover_movement(*params);
        }
    }
    if (!activity_raid_npc_should_prepare(np) && ai->perception.get_attention_mode() == ATTENTION_AIM_AT_OBJECT && ai->perception.can_engage_target_reason(params->target) == AI_ENGAGE_ALLOWED)
    {
        ai_combat_fire_at_target(*params, true);
    }
    return true;
}

// 0x00629AC0 (:458)
int defensive_combat_hide_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    cover_node* node = human_combat_cover_get_node(np);
    if (node == nullptr || !human_combat_cover_is_using_node(np))
    {
        return -1;
    }
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed())
    {
        return -1;
    }
    const ai_belief* belief = np->get_ai()->perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(target));
    if (belief == nullptr)
    {
        return -1;
    }
    vector target_pos = belief->pos;
    target_pos.y += 1.5f;
    const weapon_info* w_info = human_weapons_get_weapon_info(np);
    if (w_info == nullptr || !w_info->is_firearm() || vec_dist_squared(np->pos, target->pos) <= w_info->max_range * w_info->max_range)
    {
        return -1;
    }
    return cover_node_can_do_defensive_combat(node, np, target_pos, ai_combat_has_scoped_weapon(np)) ? 1000 : -1;
}

// 0x0069BDF0 (:507)
ai_results defensive_combat_hide_action::enter(npc* np, const ai_action*) const
{
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    np->get_ai()->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E7620 (:519)
ai_results defensive_combat_hide_action::do_action(npc*) const
{
    return AI_RESULT_IN_PROGRESS;
}

struct prepare_raid_collect_data
{
    vector raid_target_pos; // 0x00
    vector goal_pos; // 0x0C
    float max_dist_sq; // 0x18, from raid_target_pos
    const ai_system* ai; // 0x1C
};

// 0x00629BF0 (:527)
bool prepare_raid_cover_collect_cb(object* obj, unsigned int, const void* cb_data)
{
    const prepare_raid_collect_data* data = static_cast<const prepare_raid_collect_data*>(cb_data);
    return vec_dist_squared(data->raid_target_pos, obj->pos) < data->max_dist_sq && 0.0f < obj->orient.fvec * (data->goal_pos - obj->pos) && !data->ai->outside_defensive_perimeter(obj->pos, false, nullptr);
}

// 0x0065B0D0 (:546)
int prepare_raid_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    if (!activity_is_active(ACTIVITY_MODE_RAID_ATTACK) && !activity_is_active(ACTIVITY_MODE_RAID_DEFEND))
    {
        return -1;
    }
    if (np->hflags.riot_shield != 0 && np->shield_handle != 0xFFFFFFFF)
    {
        return -1;
    }
    if (!activity_raid_npc_should_prepare(np) || goal_setup_data.defend_object == nullptr)
    {
        return -1;
    }
    const vector target_pos = goal_setup_data.position;
    const weapon* firearm = ai_combat_get_action_firearm(np);
    const bool sniper = firearm != nullptr && firearm->w_info->flags.can_scope;
    cover_node* node = human_combat_cover_get_node(np);
    if (node != nullptr)
    {
        if (!cover_is_at_node(np, node))
        {
            desired_conditions |= cover_move_conditions();
        }
    }
    else
    {
        desired_conditions |= cover_move_conditions();
        farray<defensive_cover_info, 100> covers;
        farray<cover_node*, 200> nodes;
        prepare_raid_collect_data data;
        data.raid_target_pos = activity_raid_get_target_pos();
        data.max_dist_sq = vec_dist_squared(data.raid_target_pos, target_pos);
        data.goal_pos = target_pos;
        data.ai = np->get_ai();
        unsigned int handles[20];
        const int num = World.collect_objects<cover_node>(target_pos, 20.0f, handles, 20, prepare_raid_cover_collect_cb, &data);
        for (int i = 0; i < num; i++)
        {
            cover_node* cnp = World.object_from_handle<cover_node>(handles[i]);
            if (cnp != nullptr)
            {
                nodes.add(cnp);
            }
        }
        for (int i = 0; i < nodes.size(); i++)
        {
            defensive_cover_info info;
            if (nodes[i] != nullptr && get_raid_preparation_defensive_cover_info(info, np, target_pos, nodes[i], sniper))
            {
                covers.add(info);
                if (covers.full())
                {
                    break;
                }
            }
        }
        covers.sort(defensive_cover_info::less_than_or_equal);
        for (int i = 0; i < covers.size(); i++)
        {
            if (do_secondary_cover_evaluation(covers[i]))
            {
                node = covers[i].node;
                break;
            }
        }
        if (node == nullptr && covers.size() > 0)
        {
            node = covers[0].node; // the closest, with no line to the target
        }
    }
    if (node == nullptr)
    {
        return -1;
    }
    my_data.use_target = node;
    my_data.cover_stance = cover_pick_stance(node, target_pos);
    my_data.cover_firing_position = cover_pick_firing_position(my_data.cover_stance, node, target_pos);
    return my_data.cover_stance != COVER_STANCE_UNDEFINED ? 4000 : -1;
}

// 0x005E7640 (:663)
ai_results prepare_raid_action::enter(npc* np, const ai_action*) const
{
    np->play_voice_line(VOICE_LINE_RAID_PREPARE, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    return AI_RESULT_SUCCESS;
}

// Inlined in three commit_setup_data functions.
static void commit_cover_setup_data(npc* np, const ai_action_setup_data& setup_data)
{
    ai_system* ai = np->get_ai();
    human_combat_cover_reserve_node(np, static_cast<cover_node*>(setup_data.use_target));
    ai->desired_cover_stance = setup_data.cover_stance;
    ai->desired_cover_firing_position = setup_data.cover_firing_position;
}

// 0x005E7660 (:671)
void prepare_raid_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    commit_cover_setup_data(np, setup_data);
}

// 0x0069BE40 (:679)
ai_results prepare_raid_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
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
        ai_combat_fire_at_target(params, false);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x0065B550 (:712)
int defensive_combat_cover_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    const ai_system* ai = np->get_ai();
    if (np->hflags.riot_shield != 0 && np->shield_handle != 0xFFFFFFFF)
    {
        return -1;
    }
    const object* defend_object = goal_setup_data.defend_object;
    if (defend_object == nullptr)
    {
        return -1;
    }
    const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_LAST_CONTACT, np->combat.target_handle);
    if (belief == nullptr)
    {
        return -1;
    }
    vector target_pos = belief->pos;
    target_pos.y += 1.5f;
    const weapon* firearm = ai_combat_get_action_firearm(np);
    const bool sniper = firearm != nullptr && firearm->w_info->flags.can_scope;
    cover_node* best = nullptr;
    cover_node* current = human_combat_cover_get_node(np);
    if (current != nullptr && cover_node_can_do_defensive_combat(current, np, target_pos, sniper))
    {
        best = current;
    }
    else
    {
        desired_conditions |= cover_move_conditions();
        farray<defensive_cover_info, 100> covers;
        farray<cover_node*, 200> nodes;
        if (defend_object->get_sub_type() == OT_SUB_MOVER_RFG)
        {
            ai_building_collect_cover_nodes(nodes, static_cast<const rfg_mover*>(defend_object), false, nullptr, nullptr);
        }
        else
        {
            world_collect_descendants<cover_node>(defend_object, nodes, nullptr, nullptr);
        }
        if (nodes.size() == 0)
        {
            unsigned int handles[200];
            const int num = World.collect_objects<cover_node>(np->pos, 10.0f, handles, 200, nullptr, nullptr);
            for (int i = 0; i < num; i++)
            {
                cover_node* cnp = World.object_from_handle<cover_node>(handles[i]);
                if (cnp != nullptr && !ai->outside_defensive_perimeter(cnp->pos, false, nullptr))
                {
                    nodes.add(cnp);
                }
            }
        }
        for (int i = 0; i < nodes.size(); i++)
        {
            defensive_cover_info info;
            if (nodes[i] != nullptr && nodes[i] != current && get_defensive_cover_info(info, np, target_pos, nodes[i], sniper))
            {
                covers.add(info);
                if (covers.full())
                {
                    break;
                }
            }
        }
        covers.sort(defensive_cover_info::less_than_or_equal);
        for (int i = 0; i < covers.size(); i++)
        {
            if (do_secondary_cover_evaluation(covers[i]))
            {
                best = covers[i].node;
                break;
            }
        }
    }
    if (best == nullptr)
    {
        return -1;
    }
    my_data.use_target = best;
    my_data.cover_stance = cover_pick_stance(best, belief->pos);
    my_data.cover_firing_position = cover_pick_firing_position(my_data.cover_stance, best, belief->pos);
    return my_data.cover_stance != COVER_STANCE_UNDEFINED ? 4000 : -1;
}

// 0x005E76A0 (:812)
void defensive_combat_cover_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    commit_cover_setup_data(np, setup_data);
}

// 0x0069BEF0 (:820)
ai_results defensive_combat_cover_action::do_action(npc* np) const
{
    ai_combat_action_parameters params;
    params.init_npc(np);
    if (!params.init_target(true))
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    do_defensive_fire_frame(params);
    return AI_RESULT_IN_PROGRESS;
}

// 0x0065BB80 (:998)
int defensive_combat_go_to_cover_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (np->current_team == HUMAN_TEAM_MARAUDER)
    {
        return -1;
    }
    const human* target = human_combat_get_target(np);
    if (target != nullptr && target->current_team == HUMAN_TEAM_CIVILIAN)
    {
        return 0;
    }
    if (!goal_setup_data.goal->pursues_condition(ai_plan_conditions(24) | ai_plan_conditions(88)))
    {
        return -1;
    }
    return np->get_ai()->is_providing_covering_fire() != nullptr ? -1 : 2000;
}

// 0x0069BF40 (:1028)
ai_results defensive_combat_go_to_cover_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        ai->action_timer.set(0);
    }
    if (!do_defensive_go_to_cover_frame(np, nullptr))
    {
        human_combat_cover_abandon_node(np);
        return AI_RESULT_FAILURE;
    }
    if (!ai->in_cover_timer.valid())
    {
        ai->in_cover_timer.set(0);
    }
    ai->out_of_cover_timer.invalidate();
    return AI_RESULT_SUCCESS;
}

// 0x0069BFB0 (:1049)
ai_results defensive_combat_go_to_cover_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    np->combat.combat_maneuver_timestamp.set(3000); // ms
    ai_combat_action_parameters params;
    if (!do_defensive_go_to_cover_frame(np, &params))
    {
        human_combat_cover_abandon_node(np);
        return AI_RESULT_FAILURE;
    }
    if (vec_dist_squared(np->pos, params.node->pos) <= 0.64000005f) // 0.8 m
    {
        return AI_RESULT_SUCCESS;
    }
    if (!ai->is_pathing_to(params.node->pos, 0.25f, 1.0f))
    {
        const human_movement_states m_state = get_defensive_go_to_cover_movement(params);
        const uint64_t hint = params.node->flags.off_navmesh ? 0xFFFFFFFFFFFFFFFFull : params.node->closest_nav_cell_handle;
        if (ai->pathfind_to(params.node->pos, m_state, HST_CURRENT, AI_PATHFIND_PRIORITY_NORMAL, hint, false) > AI_PF_RAGDOLLED)
        {
            human_combat_cover_abandon_node(np);
            params.node->disable(COVER_DISABLED_UNREACHABLE, 20000); // ms
            return AI_RESULT_FAILURE;
        }
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x00629EA0 (:1091)
int combat_support_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const human* target = human_combat_get_target(np);
    return target != nullptr && ai_combat_get_action_firearm(np) != nullptr ? 5000 : -1;
}

// 0x0069C0E0 (:1102)
ai_results combat_support_action::enter(npc* np, const ai_action* last_action) const
{
    const human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    np->get_ai()->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    if (last_action != this)
    {
        human_set_stance(np, HST_STAND);
        combat_position_adjust_enter(np);
    }
    return AI_RESULT_SUCCESS;
}

// 0x0069C140 (:1121)
ai_results combat_support_action::do_action(npc* np) const
{
    ai_combat_action_parameters params;
    params.init_npc(np);
    if (!params.init_target(true))
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    if (np->get_ai()->perception.can_engage_target_reason(params.target) == AI_ENGAGE_ALLOWED)
    {
        return AI_RESULT_SUCCESS;
    }
    combat_position_adjust_do_frame(params, false);
    return AI_RESULT_IN_PROGRESS;
}

// 0x006BC990 (:1146)
int combat_support_cover_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const ai_system* ai = np->get_ai();
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed() || ai_combat_get_action_firearm(np) == nullptr || human_in_vehicle(np) != nullptr)
    {
        return -1;
    }
    if (np->hflags.riot_shield != 0 && np->shield_handle != 0xFFFFFFFF)
    {
        return -1;
    }
    const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(target));
    vector target_pos;
    if (belief == nullptr || !ai->perception.get_effective_target_pos(target_pos, target, nullptr, nullptr, false))
    {
        return -1;
    }
    cover_node* node = human_combat_cover_get_node(np);
    if (node == nullptr || !node->will_provide_defensive_cover(np, target_pos, false))
    {
        cover_search_parameters params(np, target_pos, Ai_cover_dist_combat_support, true, node, false);
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
    cover_node_firing_positions firing_position = COVER_FIRE_UNDEFINED;
    if (node->is_defense_only())
    {
        desired_conditions &= ~ai_plan_conditions(62);
    }
    else
    {
        firing_position = cover_pick_firing_position(stance, node, belief->pos);
        if (firing_position == COVER_FIRE_UNDEFINED)
        {
            node->disable(COVER_DISABLED_NO_STEPOUTS, 12000); // ms
            return -1;
        }
    }
    my_data.use_target = node;
    my_data.cover_stance = stance;
    my_data.cover_firing_position = firing_position;
    return 1000;
}

// 0x005E76E0 (:1235)
void combat_support_cover_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    commit_cover_setup_data(np, setup_data);
}

// 0x0069C1C0 (:1243)
ai_results combat_support_cover_action::enter(npc* np, const ai_action* last_action) const
{
    const human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        np->get_ai()->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, human_combat_cover_is_stepped_out(np) ? ATTENTION_LOOK_AT_OBJECT : ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    }
    return AI_RESULT_SUCCESS;
}

// 0x00679440 (:1260)
ai_results combat_support_cover_action::do_action(npc* np) const
{
    const human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    return np->get_ai()->perception.can_engage_target_reason(target) == AI_ENGAGE_ALLOWED ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

prepare_raid_action prepare_raid_action::the_instance; // 0x01DC9964

// 0x00618200 (.h:13)
prepare_raid_action::prepare_raid_action()
    : ai_action("prepare for raid", ai_plan_conditions_from_bits({0, 2, 62, 136, 137, 153}), ai_plan_conditions_from_bits({24}))
{
    flags.has_enter = 1;
}

ai_action* prepare_raid_action::get_instance()
{
    return &the_instance;
}

// 0x005E2BB0
prepare_raid_action::~prepare_raid_action()
{
}

// 0x00618430 (.h:13)
int prepare_raid_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

defensive_combat_cover_action defensive_combat_cover_action::the_instance; // 0x01DDB55C

// 0x00618460 (.h:14)
defensive_combat_cover_action::defensive_combat_cover_action()
    : ai_action("guard look from cover", ai_plan_conditions_from_bits({0, 2, 62, 136, 137, 153}), ai_plan_conditions_from_bits({24}))
{
}

ai_action* defensive_combat_cover_action::get_instance()
{
    return &the_instance;
}

// 0x005E2BD0
defensive_combat_cover_action::~defensive_combat_cover_action()
{
}

// 0x00618690 (.h:14)
int defensive_combat_cover_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

defensive_combat_hide_action defensive_combat_hide_action::the_instance; // 0x01DDAE3C

// 0x006186C0 (.h:15)
defensive_combat_hide_action::defensive_combat_hide_action()
    : ai_action("guard hide in cover", ai_plan_conditions_from_bits({0, 2, 9, 47, 61, 136, 137, 145, 153}), ai_plan_conditions_from_bits({24}))
{
    flags.has_enter = 1;
}

ai_action* defensive_combat_hide_action::get_instance()
{
    return &the_instance;
}

// 0x005E2BE0
defensive_combat_hide_action::~defensive_combat_hide_action()
{
}

// 0x00618A00 (.h:15)
int defensive_combat_hide_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

combat_support_action combat_support_action::the_instance; // 0x01D759D4

// 0x00618A40 (.h:19)
combat_support_action::combat_support_action()
    : ai_action("combat support", ai_plan_conditions_from_bits({0, 2, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({88}))
{
    flags.has_enter = 1;
}

ai_action* combat_support_action::get_instance()
{
    return &the_instance;
}

// 0x005E2BF0
combat_support_action::~combat_support_action()
{
}

// 0x00618D90 (.h:19)
int combat_support_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

combat_support_cover_action combat_support_cover_action::the_instance; // 0x01DCA7C4

// 0x00618DD0 (.h:20)
combat_support_cover_action::combat_support_cover_action()
    : ai_action("combat support cover", ai_plan_conditions_from_bits({0, 2, 9, 47, 62, 136, 137, 145, 153}), ai_plan_conditions_from_bits({88}))
{
    flags.has_enter = 1;
}

ai_action* combat_support_cover_action::get_instance()
{
    return &the_instance;
}

// 0x005E2C00
combat_support_cover_action::~combat_support_cover_action()
{
}

// 0x00619110 (.h:20)
int combat_support_cover_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

defensive_combat_go_to_cover_action defensive_combat_go_to_cover_action::the_instance; // 0x01DDB22C

// 0x00619140 (.h:23)
defensive_combat_go_to_cover_action::defensive_combat_go_to_cover_action()
    : ai_action("def. go to cover", ai_plan_conditions_from_bits({2, 9, 47, 136, 137, 145, 153}), ai_plan_conditions_from_bits({59}))
{
    flags.has_enter = 1;
}

ai_action* defensive_combat_go_to_cover_action::get_instance()
{
    return &the_instance;
}

// 0x005E2C10
defensive_combat_go_to_cover_action::~defensive_combat_go_to_cover_action()
{
}

// 0x006193D0 (.h:23)
int defensive_combat_go_to_cover_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

