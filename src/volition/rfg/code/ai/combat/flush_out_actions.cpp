// Original: volition/rfg/code/ai/combat/flush_out_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/combat/flush_out_actions.h"

#include "volition/rfg/code/ai/ai_building_logic.h"
#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_rate_limiters.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/combat/combat_cover.h"
#include "volition/rfg/code/ai/combat/combat_maneuver.h"
#include "volition/rfg/code/ai/combat/cover_selection.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_cover.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/pathfinding/nav_heap.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/util/varray.h"

#include <math.h>

// Position 17 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG17")

extern navpoint_heap Navpoint_heap; // nav_heap.cpp
extern bool Navpoint_treat_all_as_enabled; // object_navpoint.cpp

// Cosine of 100 degrees: a dynamic initializer in rfg.exe.
static const float MIN_HOLD_STAIRWELL_COVER_DOT = (float)cos((double)1.7453f); // 0x01DA3E78

// How far a search for cover goes (a tweak table item, zero until tweak_table.xtbl loads).
static float Ai_cover_dist_hold_position; // 0x01D67DAC
static tweak_table_item Tweak_table_item_Ai_cover_dist_hold_position("Ai_cover_dist_hold_position", &Ai_cover_dist_hold_position); // 0x01D75160

static bool Combat_maneuver_do_player_line_of_fire_checks = true; // 0x016413E2, .data 1, never written (also in goals.cpp)

// Navpoint types of a stairwell's two ends (numbers from rfg.exe; their names are not recovered).
static const signed char NAVPOINT_TYPE_STAIR_13 = 13;
static const signed char NAVPOINT_TYPE_STAIR_14 = 14;

// Added when a new cover node has to be gone to (go to cover, then the conditions of being in it).
static ai_plan_conditions cover_move_conditions()
{
    return ai_plan_conditions(59) | ai_plan_conditions(60) | ai_plan_conditions(62);
}

// 0x005E7BD0 (:44)
hold_stairwell_cover_collect_cb_data::hold_stairwell_cover_collect_cb_data(const npc* np_, const vector& close_pos_, const vector& stair_fvec_, const vector& target_pos1_, const vector& target_pos2_)
    : np(np_), ai(np_->get_ai()), close_pos(close_pos_), stair_fvec(stair_fvec_), target_pos1(target_pos1_), target_pos2(target_pos2_)
{
}

// 0x0062C560 (:133). LTCG: np EAX, believed_pos ECX, target and is_penetrating on the stack.
static bool can_hold_position(const npc* np, const human* target, bool is_penetrating, vector* believed_pos)
{
    const ai_perception& perception = np->get_ai()->perception;
    const ai_belief* position = perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(target));
    if (position == nullptr)
    {
        return false;
    }
    if (believed_pos != nullptr)
    {
        *believed_pos = position->pos;
    }
    const ai_belief* contact = perception.get_belief(AI_BELIEF_LAST_CONTACT, static_cast<const object*>(target));
    if (contact == nullptr)
    {
        return false;
    }
    const int since_contact = contact->update_time.time_since();
    if (human_combat_cover_is_using_node(np) || is_penetrating)
    {
        return since_contact < 15001; // ms
    }
    if (since_contact < 7501) // ms
    {
        return true;
    }
    if (since_contact < 15000)
    {
        const int since_shot = perception.get_time_since_last_decent_shot();
        return since_shot != -1 && since_shot < 7501;
    }
    return false;
}

// 0x005E7C40 (:193). LTCG: np ESI, target EAX.
static bool can_sidestep(const npc* np, const human* target)
{
    if (target == nullptr || target->is_killed())
    {
        return false;
    }
    const weapon_info* w_info = human_weapons_get_weapon_info(np);
    if (w_info == nullptr || !w_info->is_firearm() || vec_dist_squared(target->pos, np->pos) > w_info->max_range * w_info->max_range || human_combat_cover_is_using_node(np))
    {
        return false;
    }
    return !(np->get_ai()->planner.get_current_goal() == acquire_target_goal::get_instance() && human_combat_cover_has_node(np));
}

// 0x0069C4C0 (:233). LTCG: pos EDI, np EAX, target and go_left on the stack.
static bool get_sidestep_pos(vector& pos, const npc* np, const human* target, bool go_left)
{
    const vector side = np->orient.rvec * 2.0f; // m
    const vector dodge_end = go_left ? np->pos - side : np->pos + side;
    vector adjusted = dodge_end;
    if (combat_maneuver_evaluate_dodge_endpoint(np, dodge_end, &adjusted) && np->get_ai()->perception.firing_los_clear(target, HST_STAND, &adjusted))
    {
        pos = adjusted;
        return true;
    }
    return false;
}

// 0x005E7D00 (:264)
static bool hold_stairwell_do_navpoint_search(navpoint*& stair_close_npp, navpoint*& stair_far_npp, navpoint* start_npp, navpoint* end_npp, unsigned int session_id)
{
    start_npp->heap_data.arrival_cost = 0.0f;
    start_npp->heap_data.est_cost_to_goal = vec_dist(start_npp->pos, end_npp->pos);
    start_npp->heap_data.flags.is_open = 1;
    start_npp->heap_data.session_id = session_id;
    start_npp->prev_path_navpoint = nullptr;
    Navpoint_heap.insert_cell(start_npp);
    navpoint* cur = Navpoint_heap.remove_best_cell();
    while (true)
    {
        if (cur == nullptr)
        {
            return false;
        }
        cur->heap_data.flags.is_open = 0;
        if (cur == end_npp)
        {
            break;
        }
        for (int i = 0; i < cur->num_links; i++)
        {
            if (!Navpoint_treat_all_as_enabled && cur->link_disabled_by[i] != 0)
            {
                continue;
            }
            navpoint* link = cur->links[i];
            const bool unvisited = link->heap_data.session_id != session_id;
            if (!unvisited && link->heap_data.flags.is_open == 0)
            {
                continue;
            }
            const float cost = vec_dist(cur->pos, link->pos) + cur->heap_data.arrival_cost;
            if (unvisited)
            {
                link->heap_data.arrival_cost = cost;
                link->heap_data.est_cost_to_goal = vec_dist(link->pos, end_npp->pos);
                link->heap_data.flags.is_open = 1;
                link->heap_data.session_id = session_id;
                link->prev_path_navpoint = cur;
                Navpoint_heap.insert_cell(link);
            }
            else if (cost < link->heap_data.arrival_cost)
            {
                link->heap_data.arrival_cost = cost;
                link->prev_path_navpoint = cur;
                Navpoint_heap.adjust_cell(link);
            }
        }
        cur = Navpoint_heap.remove_best_cell();
    }
    // Forward links from the start, then the first pair of stair ends of the same type along the path.
    navpoint* node = end_npp;
    node->next_path_navpoint = nullptr;
    while (node->prev_path_navpoint != nullptr)
    {
        node->prev_path_navpoint->next_path_navpoint = node;
        node = node->prev_path_navpoint;
    }
    if (node != start_npp || node->next_path_navpoint == nullptr)
    {
        return false;
    }
    while (true)
    {
        navpoint* next = node->next_path_navpoint;
        if ((node->type == NAVPOINT_TYPE_STAIR_14 || node->type == NAVPOINT_TYPE_STAIR_13) && next->type == node->type)
        {
            stair_close_npp = node;
            stair_far_npp = next;
            return true;
        }
        node = next;
        if (node->next_path_navpoint == nullptr)
        {
            return false;
        }
    }
}

// 0x0067A210 (:353)
static bool hold_stairwell_get_cost_common(navpoint*& stair_close_npp, navpoint*& stair_far_npp, const npc* np)
{
    const ai_system* ai = np->get_ai();
    rfg_mover* building = nullptr;
    if (!ai->perception.is_in_closest_building(np, &building, 0.0f, 1.25f))
    {
        return false;
    }
    const human* target = human_combat_get_target(np);
    if (target == nullptr || !ai->perception.is_in_closest_building(target, nullptr, 0.0f, 1.25f))
    {
        return false;
    }
    navpoint* start_npp = ai_building_find_closest_navpoint(np->pos, building, true, nullptr);
    navpoint* end_npp = ai_building_find_closest_navpoint(target->pos, building, true, nullptr);
    if (start_npp == nullptr || end_npp == nullptr || start_npp->set != end_npp->set)
    {
        return false;
    }
    const bool different_floors = fabsf(start_npp->pos.y - end_npp->pos.y) >= 1.5f;
    if (ai->planner.get_current_action() == hold_stairwell_cover_action::get_instance())
    {
        stair_close_npp = World.object_from_handle<navpoint>(ai->desired_destination_object);
        stair_far_npp = World.object_from_handle<navpoint>(ai->desired_target);
        if (stair_close_npp != nullptr && stair_far_npp != nullptr)
        {
            return different_floors || end_npp == stair_close_npp || end_npp == stair_far_npp;
        }
    }
    if (!different_floors)
    {
        return false;
    }
    const unsigned int session_id = Navpoint_heap.start_session();
    const bool found = hold_stairwell_do_navpoint_search(stair_close_npp, stair_far_npp, start_npp, end_npp, session_id);
    Navpoint_heap.end_session();
    return found && !ai->outside_defensive_perimeter(stair_close_npp->pos, false, nullptr);
}

// 0x0062C640 (:416)
static bool hold_stairwell_cover_collect_cb(object* hit_objp, unsigned int, const void* user_data)
{
    const hold_stairwell_cover_collect_cb_data* data = static_cast<const hold_stairwell_cover_collect_cb_data*>(user_data);
    const cover_node* node = static_cast<const cover_node*>(hit_objp);
    if (!node->flags.fire_popup && !node->flags.fire_left && !node->flags.fire_right)
    {
        return false;
    }
    if (fabsf(node->pos.y - data->close_pos.y) > 0.75f) // m
    {
        return false;
    }
    if (vec_xz_dist_squared(node->pos, data->close_pos) > 100.0f || data->stair_fvec * node->orient.fvec < MIN_HOLD_STAIRWELL_COVER_DOT) // 10 m
    {
        return false;
    }
    if (data->ai->outside_defensive_perimeter(node->pos, false, nullptr))
    {
        return false;
    }
    return node->will_provide_offensive_cover(data->np, data->target_pos1, true, true, false, nullptr) || node->will_provide_offensive_cover(data->np, data->target_pos2, true, true, false, nullptr);
}

// 0x0065C150 (:464). LTCG: conditions ECX, close_npp EDX; far_npp is unused.
static bool hold_stairwell_get_cover_node(ai_action_setup_data& setup_data, ai_plan_conditions& conditions, const npc* np, const navpoint* close_npp, const navpoint*)
{
    const unsigned int parent = close_npp->get_parent();
    object* objp = parent != 0xFFFFFFFF ? World.object_from_handle(parent) : nullptr;
    if (objp == nullptr || objp->get_obj_type() != OT_MOVER || objp->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return false;
    }
    const rfg_mover* building = static_cast<const rfg_mover*>(objp);
    const vector close_pos = close_npp->pos;
    const vector dir = (np->pos - close_pos).copy_flatten_normalize_safe();
    const hold_stairwell_cover_collect_cb_data data(np, close_pos, dir, close_pos + dir * 3.0f, close_pos - dir * 2.0f);
    cover_node* node = human_combat_cover_get_node(np);
    if (node != nullptr)
    {
        if (node->get_parent() == building->get_handle() && hold_stairwell_cover_collect_cb(node, 0, &data))
        {
            setup_data.use_target = node;
            setup_data.cover_stance = cover_pick_stance(node, close_pos);
            if (setup_data.cover_stance != COVER_STANCE_UNDEFINED)
            {
                setup_data.cover_firing_position = cover_pick_firing_position(setup_data.cover_stance, node, close_pos);
                if (setup_data.cover_firing_position != COVER_FIRE_UNDEFINED)
                {
                    const cover_node_stances current = cover_get_current_stance(np);
                    if (current != COVER_STANCE_UNDEFINED && current != setup_data.cover_stance)
                    {
                        conditions |= ai_plan_conditions(63);
                    }
                    return true;
                }
            }
        }
        conditions |= cover_move_conditions();
    }
    farray<cover_node*, 10> nodes;
    ai_building_collect_cover_nodes(nodes, building, false, hold_stairwell_cover_collect_cb, const_cast<hold_stairwell_cover_collect_cb_data*>(&data));
    if (nodes.size() <= 0)
    {
        return false;
    }
    node = nodes[int_rand_range(0, nodes.size() - 1)];
    setup_data.use_target = node;
    setup_data.cover_stance = cover_pick_stance(node, close_pos);
    if (setup_data.cover_stance == COVER_STANCE_UNDEFINED)
    {
        return false;
    }
    setup_data.cover_firing_position = cover_pick_firing_position(setup_data.cover_stance, node, close_pos);
    return setup_data.cover_firing_position != COVER_FIRE_UNDEFINED;
}

// 0x0067A5D0 (:537)
int hold_position_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (np->current_team == HUMAN_TEAM_MARAUDER)
    {
        return -1;
    }
    const object* selected = goal_setup_data.selected_target;
    if (selected == nullptr || selected->get_obj_type() != OT_HUMAN || human_combat_cover_is_using_node(np))
    {
        return -1;
    }
    const human* target = static_cast<const human*>(selected);
    const weapon* wp = human_weapons_get_weapon(np);
    if (!can_hold_position(np, target, wp != nullptr && wp->w_info->flags.penetrating_bullets, nullptr))
    {
        return -1;
    }
    const ai_perception& perception = np->get_ai()->perception;
    if (perception.is_in_closest_building(np, nullptr, 0.0f, 1.25f) && perception.is_in_closest_building(target, nullptr, 0.0f, 1.25f))
    {
        const float xz_dist_sq = vec_xz_dist_squared(np->pos, target->pos);
        if (xz_dist_sq < 16.0f || (fabsf(np->pos.y - target->pos.y) > 1.0f && xz_dist_sq < 49.0f)) // 4 m, 7 m
        {
            return -1;
        }
    }
    return 700;
}

// 0x0069C610 (:578)
ai_results hold_position_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, np->combat.target_handle);
    if (belief == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const bool penetrating = human_weapons_is_penetrating_weapon_equipped(np);
    human_set_combat_ready(np, true, 0);
    if (np->hflags.override_default_anim_state == 0)
    {
        human_set_stance(np, Combat_maneuver_do_player_line_of_fire_checks && ai->perception.is_in_friendly_player_line_of_fire() ? HST_CROUCH : HST_STAND);
    }
    ai->perception.pay_attention(belief, penetrating ? ATTENTION_AIM_AT_OBJECT : ATTENTION_AIM_AT_BELIEF, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    if (last_action != this)
    {
        ai->stop_moving();
    }
    return AI_RESULT_SUCCESS;
}

// 0x0069C6D0 (:617)
ai_results hold_position_action::do_action(npc* np) const
{
    const ai_system* ai = np->get_ai();
    ai_combat_action_parameters params;
    params.init_npc(np);
    if (!params.init_target(true))
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    if (ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr)
    {
        return AI_RESULT_SUCCESS;
    }
    vector believed_pos;
    if (!can_hold_position(np, params.target, params.penetrating_weapon, &believed_pos))
    {
        return AI_RESULT_FAILURE;
    }
    vector to_pos = believed_pos - np->pos;
    to_pos.flatten_normalize_safe();
    if (np->orient.fvec * to_pos < 0.8660254f) // cos(30 degrees)
    {
        np->steering_vector = to_pos;
    }
    if (params.penetrating_weapon)
    {
        ai_combat_fire_at_target(params, false);
    }
    else if (human_weapons_is_firearm_equipped(np))
    {
        ai_combat_do_suppressing_fire(np, believed_pos);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x006BCBC0 (:664)
int hold_position_cover_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    const ai_system* ai = np->get_ai();
    if (np->current_team == HUMAN_TEAM_MARAUDER || (np->hflags.riot_shield != 0 && np->shield_handle != 0xFFFFFFFF))
    {
        return -1;
    }
    const object* selected = goal_setup_data.selected_target;
    if (selected == nullptr || selected->get_obj_type() != OT_HUMAN || human_weapons_is_penetrating_weapon_equipped(np))
    {
        return -1;
    }
    const human* target = static_cast<const human*>(selected);
    if (!can_hold_position(np, target, false, nullptr))
    {
        return -1;
    }
    cover_node* node = human_combat_cover_get_node(np);
    cover_node_stances stance;
    if (human_combat_cover_is_using_node(np))
    {
        if (node->orient.fvec * (target->pos - np->pos) <= 0.0f || node->is_defense_only())
        {
            desired_conditions &= ~ai_plan_conditions(62);
        }
        stance = cover_get_current_stance(np);
    }
    else
    {
        const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_LAST_CONTACT, static_cast<const object*>(target));
        if (belief == nullptr)
        {
            return 0;
        }
        if (node == nullptr)
        {
            cover_search_parameters params(np, belief->pos, Ai_cover_dist_hold_position, false, nullptr, false);
            // Also 2 m along the way from the last contact to where the target is now.
            vector to_target = target->pos - belief->pos;
            float along = to_target.mag_and_normalize_safe();
            if (along >= 2.0f)
            {
                along = 2.0f;
            }
            params.target_positions.add(belief->pos + to_target * along);
            const float quarter_dist_sq = vec_dist_squared(np->pos, belief->pos) * 0.25f;
            if (params.min_fire_range_sq >= quarter_dist_sq)
            {
                params.min_fire_range_sq = quarter_dist_sq;
            }
            node = cover_selection_do_search(params);
            if (node == nullptr)
            {
                return -1;
            }
        }
        else if (node->is_defense_only())
        {
            desired_conditions &= ~ai_plan_conditions(62);
        }
        stance = cover_pick_stance(node, belief->pos);
    }
    if (stance == COVER_STANCE_UNDEFINED)
    {
        return -1;
    }
    my_data.cover_stance = stance;
    my_data.use_target = node;
    return 200;
}

// 0x0062C930 (:749)
void hold_position_cover_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    np->get_ai()->desired_cover_stance = setup_data.cover_stance;
    object* objp = setup_data.use_target;
    if (objp != nullptr && objp->get_obj_type() == OT_COVER_NODE)
    {
        human_combat_cover_reserve_node(np, static_cast<cover_node*>(objp));
    }
}

// 0x0067A730 (:759)
ai_results hold_position_cover_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    ai_combat_action_parameters params;
    params.init_npc(np);
    if (!params.init_target(true))
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    ai->perception.pay_attention(params.target->get_handle(), AI_BELIEF_ENEMY_POSITION, ATTENTION_AIM_AT_BELIEF, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    if (last_action != this)
    {
        ai->stop_moving();
    }
    return AI_RESULT_SUCCESS;
}

// 0x0065C660 (:782)
ai_results hold_position_cover_action::do_action(npc* np) const
{
    const ai_system* ai = np->get_ai();
    ai_combat_action_parameters params;
    params.init_npc(np);
    if (!params.init_target(true))
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    if (ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr)
    {
        return AI_RESULT_SUCCESS;
    }
    vector believed_pos;
    if (!can_hold_position(np, params.target, params.penetrating_weapon, &believed_pos) || !human_weapons_is_firearm_equipped(np))
    {
        return AI_RESULT_FAILURE;
    }
    ai_combat_do_suppressing_fire(np, believed_pos);
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E7F50 (:822)
int sidestep_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return can_sidestep(np, human_combat_get_target(np)) ? 100 : -1;
}

// 0x0069C890 (:842)
ai_results sidestep_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    const human* target = human_combat_get_target(np);
    if (!can_sidestep(np, target) || ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
        ai->action_substate = int_rand_range(0, 1); // bit 0 is the side, bit 1 says the other side was tried
    }
    human_set_combat_ready(np, true, 0);
    if (!Ai_sidestep_limiter.request())
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if (get_sidestep_pos(ai->action_pos, np, target, (ai->action_substate & 1) != 0))
    {
        ai->set_direct_movement_fast((ai->action_pos - np->pos).copy_flatten_normalize_safe(), HMS_WALK, HST_STAND, -1.0f);
        ai->planner.lock_action(true);
        ai->action_timer.set(5000); // ms
        return AI_RESULT_SUCCESS;
    }
    if ((ai->action_substate & 2) != 0)
    {
        ai->planner.avoid_action(this, 8000); // ms
        return AI_RESULT_FAILURE;
    }
    ai->action_substate ^= 3;
    return AI_RESULT_IN_PROGRESS;
}

// 0x0065C720 (:907)
ai_results sidestep_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed() || ai->action_timer.elapsed())
    {
        ai->planner.avoid_action(this, 8000); // ms
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    if (ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr)
    {
        return AI_RESULT_SUCCESS;
    }
    if (vec_xz_dist_squared(np->pos, ai->action_pos) < 0.040000003f) // 0.2 m
    {
        ai->planner.avoid_action(this, 8000); // ms
        return AI_RESULT_FAILURE;
    }
    ai->set_direct_movement_fast((ai->action_pos - np->pos).copy_flatten_normalize_safe(), HMS_WALK, HST_CURRENT, -1.0f);
    return AI_RESULT_IN_PROGRESS;
}

// 0x0062C970 (:947)
int grenade_flushes_out_enemy_rule::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    if (np->npc_flags.disable_grenades != 0 || np->cur_stance == HST_PRONE)
    {
        return -1;
    }
    const human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        return -1;
    }
    const ai_belief* belief = np->get_ai()->perception.get_belief(AI_BELIEF_LAST_CONTACT, static_cast<const object*>(target));
    if (belief == nullptr || belief->update_time.time_since() >= 7501) // ms
    {
        return -1;
    }
    return (belief->flags & BF_UNCONFIRMED) != 0 ? -1 : 10;
}

// 0x0067A7B0 (:996)
int hold_stairwell_rule::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    const ai_perception& perception = np->get_ai()->perception;
    rfg_mover* building = nullptr;
    if (!perception.is_in_closest_building(np, &building, 0.0f, 1.25f) || goal_setup_data.selected_target == nullptr)
    {
        return -1;
    }
    // Original quirk kept: the selected target is taken for a human with no type test.
    return perception.is_in_closest_building(static_cast<const human*>(goal_setup_data.selected_target), nullptr, 0.0f, 1.25f) ? 1000 : -1;
}

// 0x0067A840 (:1023)
int stairwell_acquire_rule::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    const ai_system* ai = np->get_ai();
    rfg_mover* building = nullptr;
    if (!ai->perception.is_in_closest_building(np, &building, 0.0f, 1.25f))
    {
        return -1;
    }
    const object* selected = goal_setup_data.selected_target;
    if (selected == nullptr || selected->get_obj_type() != OT_HUMAN || !ai->perception.is_in_closest_building(static_cast<const human*>(selected), nullptr, 0.0f, 1.25f))
    {
        return -1;
    }
    const object* orders_object = ai->get_orders_object();
    if (orders_object == nullptr || orders_object->get_obj_type() != OT_BOUNDING_BOX)
    {
        return -1;
    }
    return ai->outside_defensive_perimeter(selected->pos, false, nullptr) ? 2000 : -1;
}

// 0x0067AAD0 (:1067)
int hold_stairwell_cover_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    if (np->hflags.riot_shield != 0 && np->shield_handle != 0xFFFFFFFF)
    {
        return -1;
    }
    navpoint* close_npp = nullptr;
    navpoint* far_npp = nullptr;
    if (!hold_stairwell_get_cost_common(close_npp, far_npp, np))
    {
        return -1;
    }
    my_data.attack_target = far_npp;
    my_data.defend_target = close_npp;
    if (!hold_stairwell_get_cover_node(my_data, desired_conditions, np, close_npp, far_npp))
    {
        return -1;
    }
    if (goal_setup_data.goal == acquire_target_goal::get_instance())
    {
        return 200;
    }
    const ai_belief* belief = np->get_ai()->perception.get_belief(AI_BELIEF_LAST_CONTACT, goal_setup_data.selected_target);
    return belief != nullptr && (float)belief->update_time.time_since() <= 15000.0f ? 200 : -1;
}

// 0x0062C9E0 (:1115)
void hold_stairwell_cover_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    object* objp = setup_data.use_target;
    if (objp != nullptr && objp->get_obj_type() == OT_COVER_NODE)
    {
        ai_system* ai = np->get_ai();
        human_combat_cover_reserve_node(np, static_cast<cover_node*>(objp));
        ai->desired_cover_stance = setup_data.cover_stance;
        ai->desired_cover_firing_position = setup_data.cover_firing_position;
        ai->desired_destination_object = setup_data.defend_target->get_handle();
        ai->desired_target = setup_data.attack_target->get_handle();
    }
}

// 0x0069CA70 (:1127)
ai_results hold_stairwell_cover_action::enter(npc* np, const ai_action*) const
{
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    np->get_ai()->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    return AI_RESULT_SUCCESS;
}

// 0x0065C890 (:1140)
ai_results hold_stairwell_cover_action::do_action(npc* np) const
{
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    return np->get_ai()->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

hold_position_action hold_position_action::the_instance; // 0x01D72098

// 0x0062B540 (.h:12)
hold_position_action::hold_position_action()
    : ai_action("hold position", ai_plan_conditions_from_bits({4, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({21}))
{
    flags.has_enter = 1;
}

ai_action* hold_position_action::get_instance()
{
    return &the_instance;
}

// 0x005E7B50
hold_position_action::~hold_position_action()
{
}

// 0x0062B840 (.h:12)
int hold_position_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

hold_position_cover_action hold_position_cover_action::the_instance; // 0x01D75090

// 0x0062B880 (.h:13)
hold_position_cover_action::hold_position_cover_action()
    : ai_action("hold in cover", ai_plan_conditions_from_bits({0, 2, 9, 47, 60, 62, 136, 137, 145, 153}), ai_plan_conditions_from_bits({21}))
{
    flags.has_enter = 1;
}

ai_action* hold_position_cover_action::get_instance()
{
    return &the_instance;
}

// 0x005E7B60
hold_position_cover_action::~hold_position_cover_action()
{
}

// 0x0062BC20 (.h:13)
int hold_position_cover_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

sidestep_action sidestep_action::the_instance; // 0x01D722E0

// 0x0062BC50 (.h:14)
sidestep_action::sidestep_action()
    : ai_action("sidestep", ai_plan_conditions_from_bits({0, 2, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({21}))
{
    flags.has_enter = 1;
}

ai_action* sidestep_action::get_instance()
{
    return &the_instance;
}

// 0x005E7B70
sidestep_action::~sidestep_action()
{
}

// 0x0062BFA0 (.h:14)
int sidestep_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

hold_stairwell_cover_action hold_stairwell_cover_action::the_instance; // 0x01D9F994

// 0x0062BFE0 (.h:17)
hold_stairwell_cover_action::hold_stairwell_cover_action()
    : ai_action("stairwell cover", ai_plan_conditions_from_bits({0, 2, 9, 47, 62, 136, 137, 145, 153}), ai_plan_conditions_from_bits({22}))
{
    flags.has_enter = 1;
}

ai_action* hold_stairwell_cover_action::get_instance()
{
    return &the_instance;
}

// 0x005E7B90
hold_stairwell_cover_action::~hold_stairwell_cover_action()
{
}

// 0x0062C320 (.h:17)
int hold_stairwell_cover_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

grenade_flushes_out_enemy_rule grenade_flushes_out_enemy_rule::the_instance; // 0x01D74FA4

// 0x0062C350 (.h:19)
grenade_flushes_out_enemy_rule::grenade_flushes_out_enemy_rule()
    : ai_production_rule("grenade = flush out", ai_plan_conditions_from_bits({85}), ai_plan_conditions_from_bits({21}))
{
}

ai_action* grenade_flushes_out_enemy_rule::get_instance()
{
    return &the_instance;
}

// 0x005E7BA0
grenade_flushes_out_enemy_rule::~grenade_flushes_out_enemy_rule()
{
}

// 0x0062C3D0 (.h:19)
int grenade_flushes_out_enemy_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

hold_stairwell_rule hold_stairwell_rule::the_instance; // 0x01D72168

// 0x0062C400 (.h:20)
hold_stairwell_rule::hold_stairwell_rule()
    : ai_production_rule("stairwell = flush out", ai_plan_conditions_from_bits({22}), ai_plan_conditions_from_bits({21}))
{
}

ai_action* hold_stairwell_rule::get_instance()
{
    return &the_instance;
}

// 0x005E7BB0
hold_stairwell_rule::~hold_stairwell_rule()
{
}

// 0x0062C480 (.h:20)
int hold_stairwell_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

stairwell_acquire_rule stairwell_acquire_rule::the_instance; // 0x01DDAC6C

// 0x0062C4B0 (.h:21)
stairwell_acquire_rule::stairwell_acquire_rule()
    : ai_production_rule("stairwell = acquire", ai_plan_conditions_from_bits({22}), ai_plan_conditions_from_bits({23}))
{
}

ai_action* stairwell_acquire_rule::get_instance()
{
    return &the_instance;
}

// 0x005E7BC0
stairwell_acquire_rule::~stairwell_acquire_rule()
{
}

// 0x0062C530 (.h:21)
int stairwell_acquire_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

