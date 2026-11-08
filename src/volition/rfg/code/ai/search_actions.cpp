// Original: volition/rfg/code/ai/search_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/search_actions.h"

#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_rate_limiters.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/combat/combat_cover.h"
#include "volition/rfg/code/ai/follow_logic.h"
#include "volition/rfg/code/ai/formation.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/ai/search_system_internal.h"
#include "volition/rfg/code/ai/search_utils.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/activities/activity_raid.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_cover.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/objects/object_guard_node.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/vlib/code/math/rand.h"

#include <math.h>

// Position 29 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG29")

static const signed char NAVPOINT_TYPE_SEARCH = 10; // numeric in rfg.exe, name not recovered
static const float MAX_EXISTS_RELEVANCE = 10.0f; // past it an enemy is forgotten, unless the belief came from orders
static const int SEARCHER_KEEPALIVE_MS = 15000;
static const float ALLY_WAIT_DIST_SQ = 36.0f; // 6 m

static void keep_searcher_alive(unsigned int search_handle)
{
    ai_search_searcher* searcher = Ai_search_object_manager.searcher_from_handle(search_handle);
    if (searcher != nullptr)
    {
        searcher->reset_keepalive();
    }
}

static bool belief_is_searchable(const ai_belief* belief)
{
    return belief->get_relevance() <= MAX_EXISTS_RELEVANCE || (belief->flags & BF_INVESTIGATED) != 0;
}

// 0x0066AC30 (:142). LTCG: np ECX, current EAX, building on the stack.
static navpoint* find_building_search_navpoint(const npc* np, const rfg_mover* building, navpoint* current)
{
    if (current != nullptr && vec_dist_squared(np->pos, current->pos) > 1.0f) // still on the way to it
    {
        return current;
    }
    navpoint* found[100];
    int num = 0;
    const object* first = building->child_ptr;
    for (object* child = building->child_ptr; child != nullptr && num < 100;)
    {
        if (child->get_obj_type() == OT_NAVPOINT && static_cast<navpoint*>(child)->type == NAVPOINT_TYPE_SEARCH)
        {
            found[num++] = static_cast<navpoint*>(child);
        }
        child = child->child_next;
        if (child == first)
        {
            break;
        }
    }
    return num > 0 ? found[int_rand_range(0, num - 1)] : nullptr;
}

// 0x006A89B0 (:183)
static bool is_valid_search_follow_target(const npc* np)
{
    const ai_system* ai = np->get_ai();
    if (ai_formation_is_following(np, ai->get_formation_handle()))
    {
        return false;
    }
    const ai_search_location_info* location = ai_search_get_location_info(ai->desired_search_handle);
    return location != nullptr && location->search_type == AI_ST_EXTERNAL_AREA && ai_formation_from_leader(np->get_handle()) == 0xFFFFFFFF;
}

// 0x006A8A00 (:215): the current leader if still outside, else the closest fit searcher, squad members first.
static npc* get_search_follow_target(const npc* np, const human*)
{
    const ai_system* ai = np->get_ai();
    if (ai->get_formation_handle() != 0xFFFFFFFF)
    {
        object* leader = ai_formation_get_leader(ai->get_formation_handle());
        if (leader != nullptr && leader->get_obj_type() == OT_HUMAN && leader->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            const ai_search_location_info* location = ai_search_get_location_info(static_cast<npc*>(leader)->get_ai()->desired_search_handle);
            if (location != nullptr && location->search_type == AI_ST_EXTERNAL_AREA)
            {
                return static_cast<npc*>(leader);
            }
        }
    }
    object_squad* squad = ai->get_squad_handle() != 0xFFFFFFFF ? World.object_from_handle<object_squad>(ai->get_squad_handle()) : nullptr;
    npc* best = nullptr;
    bool best_in_squad = false;
    float best_dist_sq = -1.0f;
    csspinlock_scope lock(ai_search_get_searchers_cs_wrapper());
    for (const ai_search_searcher_info* info = ai_search_get_first_searcher(); info != nullptr; info = ai_search_get_next_searcher(info))
    {
        object* objp = info->searcher_handle != 0xFFFFFFFF ? World.object_from_handle(info->searcher_handle) : nullptr;
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC || objp == np || objp->is_killed())
        {
            continue;
        }
        npc* other = static_cast<npc*>(objp);
        if (human_teams_get_disposition(np, other) != TEAM_DISPOSITION_ALLIED || vec_dist_squared(np->pos, other->pos) > 144.0f || !is_valid_search_follow_target(other)) // 12 m
        {
            continue;
        }
        const bool in_squad = squad != nullptr && squad->is_member(other);
        if (!in_squad && best_in_squad)
        {
            continue;
        }
        const float dist_sq = vec_dist_squared(other->pos, np->pos);
        if (best == nullptr || (!best_in_squad && in_squad) || dist_sq < best_dist_sq)
        {
            best_in_squad = best_in_squad || in_squad;
            best = other;
            best_dist_sq = dist_sq;
        }
    }
    return best;
}

// 0x00641200 (:278)
bool search_goals_callback(const human* hp, void*)
{
    static const ai_plan_conditions SEARCH_GOAL_CONDITIONS = ai_plan_conditions(92) | ai_plan_conditions(95);
    if (hp == nullptr || hp->get_obj_type() != OT_HUMAN || hp->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return false;
    }
    const ai_goal* goal = static_cast<const npc*>(hp)->get_ai()->planner.get_current_goal();
    return goal != nullptr && goal->pursues_condition(SEARCH_GOAL_CONDITIONS);
}

// 0x005F1450 (:304). LTCG: node ESI, pos and np on the stack.
static bool is_valid_observation_point(const object_guard_node* node, const vector& pos, const npc* np)
{
    if (!node->can_reserve(np->get_handle()))
    {
        return false;
    }
    if (vec_xz_dist_squared(np->pos, node->pos) > 100.0f || fabsf(node->pos.y - np->pos.y) > 2.0f) // 10 m, 2 m
    {
        return false;
    }
    float ground;
    havok_terrain_height(ground, node->pos.x, node->pos.z, false, nullptr);
    if (node->pos.y - ground < 2.0f) // 2 m up
    {
        return false;
    }
    vector dir;
    vec_normalized_dir(dir, pos, node->pos);
    const float limit = node->orient.rvec * dir <= 0.0f ? node->get_left_angle_dot() : node->get_right_angle_dot();
    return limit <= node->orient.fvec * dir;
}

// 0x0066AD20 (:350). LTCG: current_node EAX, the others on the stack.
static object_guard_node* pick_observation_guard_node(const vector& pos, const npc* np, const rfg_mover* building, object_guard_node* current_node)
{
    if (current_node != nullptr && is_valid_observation_point(current_node, pos, np))
    {
        return current_node;
    }
    object_guard_node* found[10];
    int num = 0;
    const object* first = building->child_ptr;
    for (object* child = building->child_ptr; child != nullptr && num < 10;)
    {
        if (child->get_obj_type() == OT_GUARD_NODE)
        {
            object_guard_node* node = static_cast<object_guard_node*>(child);
            if (!node->is_disabled() && is_valid_observation_point(node, pos, np))
            {
                found[num++] = node;
            }
        }
        child = child->child_next;
        if (child == first)
        {
            break;
        }
    }
    return num > 0 ? found[int_rand_range(0, num - 1)] : nullptr;
}

// 0x006844E0 (:395). LTCG: np EDI.
static bool external_search_location_is_still_valid(const npc* np)
{
    const ai_search_searcher* searcher = Ai_search_object_manager.searcher_from_handle(np->get_ai()->desired_search_handle);
    if (searcher == nullptr || !searcher->info.location_history.has_current)
    {
        return false;
    }
    const vector los_pos = searcher->info.location_history.los_position;
    if (nav_cell_from_handle(searcher->info.location_history.navcell) == nullptr)
    {
        return false;
    }
    vector start = np->pos;
    nav_cell* start_cell = bb_pf_get_nav_cell_from_hint(&start, np->pf.last_visited_nav_cell_handle, np->pos, false, nullptr, false, false, NAV_CELL_TRAVERSAL_ENABLED);
    if (start_cell == nullptr && !bb_pf_find_closest_point_on_nav_mesh(np->pos, start, start_cell, false))
    {
        return false;
    }
    vector end_pos;
    const nav_cell* end_cell;
    if (nav_cell_find_longest_line_of_sight_no_detour(end_pos, end_cell, start, start_cell, los_pos, false, nullptr))
    {
        return true;
    }
    // Also from 5 cm to either side.
    const vector dir = (los_pos - start).copy_flatten_normalize_safe();
    const vector side(dir.z * 0.05f, 0.0f, -dir.x * 0.05f);
    return nav_cell_find_longest_line_of_sight_no_detour(end_pos, end_cell, start + side, start_cell, los_pos, false, nullptr) || nav_cell_find_longest_line_of_sight_no_detour(end_pos, end_cell, start - side, start_cell, los_pos, false, nullptr);
}

// 0x0066AE10 (:449)
int search_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (np->current_team == HUMAN_TEAM_MARAUDER || human_team_is_non_combatant(np))
    {
        return -1;
    }
    const vehicle* vp = human_in_vehicle(np);
    if (vp != nullptr && human_is_driving(np) && ai_vehicle_has_manned_turret(vp))
    {
        return -1;
    }
    const ai_system* ai = np->get_ai();
    const ai_belief* exists = ai->perception.get_first_belief(AI_BELIEF_ENEMY_EXISTS);
    if (exists == nullptr || ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr || !belief_is_searchable(exists))
    {
        return -1;
    }
    const human* target = World.object_from_handle<human>(exists->handle);
    if (target == nullptr || human_is_dead(target) || ai->outside_defensive_perimeter(exists->pos, false, nullptr))
    {
        return -1;
    }
    if (!object_is_player(target) && !ai->perception.enemy_in_visual_range(target))
    {
        return -1;
    }
    return 1100;
}

// 0x006BA3E0 (:500)
bool search_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_system* ai = np->get_ai();
    const ai_belief* exists = ai->perception.get_first_belief(AI_BELIEF_ENEMY_EXISTS);
    if (exists == nullptr || ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr || !belief_is_searchable(exists) || exists->handle == 0xFFFFFFFF)
    {
        return false;
    }
    object* objp = World.object_from_handle(exists->handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN)
    {
        return false;
    }
    goal_setup_data.selected_target = objp;
    goal_setup_data.attention_belief_type = AI_BELIEF_ENEMY_EXISTS;
    goal_setup_data.attention_mode = ATTENTION_USE_CURRENT_MODE;
    goal_setup_data.search_handle = ai_search_request_search(np, static_cast<const human*>(objp));
    return goal_setup_data.search_handle != 0xFFFFFFFF;
}

static void play_search_initiate(npc* np)
{
    if (np->get_ai()->perception.get_first_belief(AI_BELIEF_CLOSEST_ALLY) != nullptr && !activity_raid_has_raid_orders(np))
    {
        np->play_voice_line(VOICE_LINE_SEARCH_INITIATE, search_goals_callback, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    }
}

// 0x006A8C80 (:532)
void search_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    np->get_ai()->desired_search_handle = goal_setup_data.search_handle;
    goal_setup_data.set_attention(np, nullptr);
    play_search_initiate(np);
}

// 0x006A8CE0 (:550)
int accompany_search_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (np->current_team == HUMAN_TEAM_MARAUDER)
    {
        return -1;
    }
    const ai_system* ai = np->get_ai();
    const ai_belief* exists = ai->perception.get_first_belief(AI_BELIEF_ENEMY_EXISTS);
    if (ai->perception.get_first_belief(AI_BELIEF_ENEMY_POSITION) != nullptr || exists == nullptr || !belief_is_searchable(exists) || ai->outside_defensive_perimeter(exists->pos, true, nullptr))
    {
        return -1;
    }
    const object* objp = exists->handle != 0xFFFFFFFF ? World.object_from_handle(exists->handle) : nullptr;
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || !ai_search_is_fully_populated(static_cast<const human*>(objp)))
    {
        return -1;
    }
    const ai_search_location_info* location = ai_search_get_location_info(ai->desired_search_handle);
    if ((location != nullptr && location->search_type != AI_ST_EXTERNAL_AREA) || ai_formation_from_leader(np->get_handle()) != 0xFFFFFFFF)
    {
        return -1;
    }
    return get_search_follow_target(np, static_cast<const human*>(objp)) != nullptr ? 1110 : -1;
}

// 0x006A8F90 (:603)
bool accompany_search_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_system* ai = np->get_ai();
    const ai_belief* exists = ai->perception.get_first_belief(AI_BELIEF_ENEMY_EXISTS);
    if (ai->perception.get_first_belief(AI_BELIEF_ENEMY_POSITION) != nullptr || exists == nullptr || !belief_is_searchable(exists) || exists->handle == 0xFFFFFFFF)
    {
        return false;
    }
    object* objp = World.object_from_handle(exists->handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || !ai_search_is_fully_populated(static_cast<const human*>(objp)))
    {
        return false;
    }
    const ai_search_location_info* location = ai_search_get_location_info(ai->desired_search_handle);
    if ((location != nullptr && location->search_type != AI_ST_EXTERNAL_AREA) || ai_formation_from_leader(np->get_handle()) != 0xFFFFFFFF)
    {
        return false;
    }
    npc* leader = get_search_follow_target(np, static_cast<const human*>(objp));
    if (leader == nullptr)
    {
        return false;
    }
    goal_setup_data.selected_target = objp;
    goal_setup_data.selected_leader = leader;
    goal_setup_data.attention_belief_type = AI_BELIEF_ENEMY_EXISTS;
    goal_setup_data.attention_mode = ATTENTION_USE_CURRENT_MODE;
    return true;
}

// 0x006A9060 (:656)
void accompany_search_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    np->get_ai()->desired_follow_target = goal_setup_data.selected_leader->get_handle();
    goal_setup_data.set_attention(np, nullptr);
    play_search_initiate(np);
}

// 0x0066B100 (:674)
int watch_area_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (np->current_team == HUMAN_TEAM_MARAUDER || human_team_is_non_combatant(np))
    {
        return -1;
    }
    const ai_system* ai = np->get_ai();
    if (ai->perception.get_first_belief(AI_BELIEF_ENEMY_POSITION) != nullptr || ai->perception.get_first_belief(AI_BELIEF_ENEMY_EXISTS) == nullptr)
    {
        return -1;
    }
    return ai->perception.is_in_closest_building(np, nullptr, 1.0f, 1.25f) ? 1200 : -1;
}

// 0x0066B180 (:701)
bool watch_area_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_system* ai = np->get_ai();
    const ai_belief* exists = ai->perception.get_first_belief(AI_BELIEF_ENEMY_EXISTS);
    if (ai->perception.get_first_belief(AI_BELIEF_ENEMY_POSITION) != nullptr || exists == nullptr || exists->handle == 0xFFFFFFFF)
    {
        return false;
    }
    object* objp = World.object_from_handle(exists->handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN)
    {
        return false;
    }
    goal_setup_data.selected_target = objp;
    goal_setup_data.attention_belief_type = AI_BELIEF_ENEMY_EXISTS;
    goal_setup_data.attention_mode = ATTENTION_USE_CURRENT_MODE;
    rfg_mover* building = nullptr;
    if (!ai->perception.is_in_closest_building(np, &building, 1.0f, 1.25f))
    {
        return false;
    }
    goal_setup_data.defend_object = building;
    object_guard_node* node = pick_observation_guard_node(exists->pos, np, building, ai->get_guard_node());
    if (node == nullptr)
    {
        return false;
    }
    goal_setup_data.use_target = node;
    return true;
}

// 0x006A90C0 (:740)
void watch_area_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->desired_search_handle = 0xFFFFFFFF;
    ai_belief* belief;
    goal_setup_data.set_attention(np, &belief);
    object* objp = goal_setup_data.use_target;
    if (objp != nullptr && objp->get_obj_type() == OT_GUARD_NODE)
    {
        ai->reserve_guard_node(static_cast<object_guard_node*>(objp), -128); // -128: no direction
    }
}

// 0x006BA490 (:758)
int search_building_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    const ai_search_location_info* location = ai_search_get_location_info(goal_setup_data.search_handle);
    if (location == nullptr || location->search_type != AI_ST_BUILDING_INTERIOR)
    {
        return -1;
    }
    object* objp = location->search_obj != 0xFFFFFFFF ? World.object_from_handle(location->search_obj) : nullptr;
    my_data.building_target = objp != nullptr && objp->get_obj_type() == OT_MOVER && objp->get_sub_type() == OT_SUB_MOVER_RFG ? static_cast<rfg_mover*>(objp) : nullptr;
    if (my_data.building_target == nullptr)
    {
        ai_search_update_progress(goal_setup_data.search_handle, AI_RESULT_FAILURE);
        return -1;
    }
    navpoint* npp = find_building_search_navpoint(np, my_data.building_target, nullptr);
    my_data.use_target = npp;
    if (npp == nullptr)
    {
        return -1;
    }
    my_data.pos = npp->pos;
    return 1000;
}

// 0x005F1610 (:784)
void search_building_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    np->get_ai()->desired_search_navpoint = setup_data.use_target->get_handle();
}

// 0x006412F0 (:793)
ai_results search_building_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    human_set_combat_ready(np, true, 0);
    if (last_action != this)
    {
        ai->perception.set_attention_mode(ATTENTION_SEARCH, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
        ai->action_substate = 0;
    }
    return AI_RESULT_SUCCESS;
}

// 0x006BA560 (:814)
ai_results search_building_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human_set_combat_ready(np, true, 0);
    ai_results result = AI_RESULT_IN_PROGRESS;
    if (np->movement_mode != HMM_PATHFINDING)
    {
        const ai_search_location_info* location = ai_search_get_location_info(ai->desired_search_handle);
        if (location == nullptr)
        {
            return AI_RESULT_FAILURE;
        }
        object* objp = location->search_obj != 0xFFFFFFFF ? World.object_from_handle(location->search_obj) : nullptr;
        if (objp == nullptr || objp->get_obj_type() != OT_MOVER || objp->get_sub_type() != OT_SUB_MOVER_RFG)
        {
            result = AI_RESULT_FAILURE;
        }
        else
        {
            navpoint* current = ai->desired_search_navpoint != 0xFFFFFFFF ? World.object_from_handle<navpoint>(ai->desired_search_navpoint) : nullptr;
            navpoint* npp = find_building_search_navpoint(np, static_cast<rfg_mover*>(objp), current);
            if (npp == nullptr)
            {
                result = AI_RESULT_SUCCESS;
            }
            else if (ai->pathfind_to(npp->pos, HMS_WALK, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false) <= AI_PF_RAGDOLLED)
            {
                ai->desired_search_navpoint = npp->get_handle();
            }
            else
            {
                ai->action_substate++;
                ai->desired_search_navpoint = 0xFFFFFFFF;
                if (ai->action_substate >= 6) // tries
                {
                    result = AI_RESULT_FAILURE;
                }
            }
        }
    }
    ai_search_update_progress(ai->desired_search_handle, result);
    return result;
}

// 0x005F1630 (:871)
ai_results search_building_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        np->get_ai()->desired_search_navpoint = 0xFFFFFFFF;
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F1660 (:884)
int accompany_search_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 100;
}

// 0x006BF0E0 (:889)
ai_results accompany_search_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    object* objp = ai->desired_follow_target != 0xFFFFFFFF ? World.object_from_handle(ai->desired_follow_target) : nullptr;
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC || objp->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    const npc* leader = static_cast<const npc*>(objp);
    if (!ai->is_following_leader(leader) && ai_formation_get_num_followers(leader->get_handle()) > 0)
    {
        return AI_RESULT_FAILURE;
    }
    if (ai->follow_in_formation(leader, AI_FORMATION_FOLLOW_THE_LEADER) == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    ai->perception.set_attention_mode(ATTENTION_SEARCH_COVER_MY_SIX, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    human_set_combat_ready(np, true, 0);
    return AI_RESULT_SUCCESS;
}

// 0x006A9110 (:915)
ai_results accompany_search_action::do_action(npc* np) const
{
    const unsigned int handle = np->get_ai()->get_formation_handle();
    if (!ai_formation_is_following(np, handle) || handle == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    const object* leader = ai_formation_get_leader(handle);
    if (leader == nullptr || leader->get_obj_type() != OT_HUMAN || leader->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return AI_RESULT_FAILURE;
    }
    const ai_search_location_info* location = ai_search_get_location_info(static_cast<const npc*>(leader)->get_ai()->desired_search_handle);
    if (location == nullptr || location->search_type != AI_ST_EXTERNAL_AREA || !ai_do_follow_frame(np, handle, HST_STAND))
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    return AI_RESULT_IN_PROGRESS;
}

// 0x006BF2D0 (:955)
ai_results accompany_search_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        np->get_ai()->leave_formation();
    }
    return AI_RESULT_SUCCESS;
}

// 0x006A91D0 (:969)
int search_get_external_location::get_cost(const npc*, const ai_goal_setup_data& goal_setup_data) const
{
    const ai_search_location_info* location = ai_search_get_location_info(goal_setup_data.search_handle);
    if (location == nullptr || location->search_type != AI_ST_EXTERNAL_AREA)
    {
        return -1;
    }
    vector pos, los_pos;
    nav_cell* ncp = nullptr;
    return ai_search_get_current_exterior_search_pos(pos, los_pos, ncp, goal_setup_data.search_handle, nullptr) ? -1 : 500;
}

// 0x006BA670 (:987)
ai_results search_get_external_location::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human_set_combat_ready(np, true, 0);
    ai->perception.set_attention_mode(ATTENTION_SEARCH, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    const ai_search_location_info* location = ai_search_get_location_info(ai->desired_search_handle);
    if (location == nullptr || location->search_type != AI_ST_EXTERNAL_AREA)
    {
        return AI_RESULT_FAILURE;
    }
    const object* objp = ai->perception.get_attention_target();
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN)
    {
        return AI_RESULT_FAILURE;
    }
    vector pos, los_pos;
    nav_cell* ncp = nullptr;
    if (ai_search_get_current_exterior_search_pos(pos, los_pos, ncp, ai->desired_search_handle, nullptr))
    {
        return AI_RESULT_SUCCESS;
    }
    ai_results progress = AI_RESULT_IN_PROGRESS;
    ai_results returned = AI_RESULT_IN_PROGRESS;
    if (Ai_pathfind_rate_limiter.request_immediate(np, AI_PATHFIND_PRIORITY_LOW))
    {
        if (ai_search_get_next_search_location(pos, los_pos, ncp, np, static_cast<const human*>(objp)))
        {
            ai_search_set_current_exterior_search_pos(ai->desired_search_handle, pos, los_pos, ncp, ai_search_get_cover_node_for_location(np, pos, los_pos, ncp));
            returned = AI_RESULT_SUCCESS;
        }
        else
        {
            progress = AI_RESULT_FAILURE;
            returned = AI_RESULT_FAILURE;
        }
    }
    ai_search_update_progress(ai->desired_search_handle, progress);
    return returned;
}

// 0x006A9240 (:1051)
int search_external_area_action::get_cost(const npc*, const ai_goal_setup_data& goal_setup_data) const
{
    const ai_search_location_info* location = ai_search_get_location_info(goal_setup_data.search_handle);
    if (location == nullptr || location->search_type != AI_ST_EXTERNAL_AREA)
    {
        return -1;
    }
    vector pos, los_pos;
    nav_cell* ncp = nullptr;
    return ai_search_get_current_exterior_search_pos(pos, los_pos, ncp, goal_setup_data.search_handle, nullptr) ? 2500 : -1;
}

// 0x006A92A0 (:1069)
ai_results search_external_area_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    human_set_combat_ready(np, true, 0);
    ai->perception.set_attention_mode(ATTENTION_SEARCH, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    if (last_action != this)
    {
        ai->action_timer.invalidate();
    }
    const ai_search_location_info* location = ai_search_get_location_info(ai->desired_search_handle);
    if (location == nullptr || location->search_type != AI_ST_EXTERNAL_AREA)
    {
        return AI_RESULT_FAILURE;
    }
    if (external_search_location_is_still_valid(np))
    {
        return AI_RESULT_SUCCESS;
    }
    ai_search_abandon_exterior_search_pos(ai->desired_search_handle);
    return AI_RESULT_FAILURE;
}

// 0x006BA8E0 (:1097). The call's arguments are from the asm (0x006BAAA9): the decompile shifts them.
ai_results search_external_area_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const ai_search_location_info* location = ai_search_get_location_info(ai->desired_search_handle);
    if (location == nullptr || location->search_type != AI_ST_EXTERNAL_AREA)
    {
        return AI_RESULT_FAILURE;
    }
    const object* target = ai->perception.get_attention_target();
    if (target == nullptr || target->get_obj_type() != OT_HUMAN)
    {
        return AI_RESULT_FAILURE;
    }
    vector pos, los_pos;
    nav_cell* ncp = nullptr;
    if (!ai_search_get_current_exterior_search_pos(pos, los_pos, ncp, ai->desired_search_handle, nullptr))
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    if (nav_cell_get_closest_xz_dist_squared_to_point(ncp, np->pos) <= 0.16000001f) // 0.4 m
    {
        if (!ai->action_timer.valid())
        {
            ai->action_timer.set(1500); // ms
        }
        // Arrived when standing on terrain inside the cell, otherwise when the time is up.
        bool arrived = false;
        const nav_cell* cell = nav_cell_from_handle(np->pf.last_visited_nav_cell_handle);
        if (cell != nullptr && (cell->flags & 8) == 0 && nav_cell_is_point_in_cell_column(cell, np->pos))
        {
            float ground;
            arrived = havok_terrain_height(ground, np->pos.x, np->pos.z, false, nullptr) && np->pos.y <= ground + 1.25f;
        }
        if (arrived || ai->action_timer.elapsed())
        {
            ai_search_finish_exterior_search_pos(ai->desired_search_handle, np->pos);
            ai->set_pathfind_delay(0);
            return AI_RESULT_SUCCESS;
        }
    }
    ai_results result = AI_RESULT_IN_PROGRESS;
    if (!ai->is_pathing_to(los_pos, 0.25f, 1.0f))
    {
        result = ai_pf_to_result(ai->set_direct_movement_to_pos(los_pos, nav_cell_get_handle(ncp), HMS_WALK, HST_STAND, true));
        if (result == AI_RESULT_FAILURE)
        {
            ai_search_set_avoidance_point(ai->desired_search_handle, los_pos);
            ai_search_abandon_exterior_search_pos(ai->desired_search_handle);
        }
    }
    keep_searcher_alive(ai->desired_search_handle);
    return result;
}

// 0x005F1670 (:1191). LTCG: follower ESI, caller EAX, was_waiting on the stack.
static bool should_wait_for_search_ally(const npc* caller, const npc* follower, bool was_waiting)
{
    const bool near_ally = vec_dist_squared(follower->pos, caller->pos) <= ALLY_WAIT_DIST_SQ;
    if (!was_waiting && near_ally)
    {
        return false;
    }
    if (was_waiting && !near_ally)
    {
        return true;
    }
    return follower->cur_movement_state != HMS_IDLE;
}

struct wait_for_ally_data
{
    const npc* np; // 0x00
    bool was_waiting; // 0x04
};

// 0x00641460 (:1212)
static bool wait_for_ally_follower_callback(unsigned int handle, void* data)
{
    const wait_for_ally_data* wait = static_cast<const wait_for_ally_data*>(data);
    const object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC || objp->is_killed())
    {
        return false;
    }
    const npc* follower = static_cast<const npc*>(objp);
    return human_teams_get_disposition(follower, wait->np) == TEAM_DISPOSITION_ALLIED && should_wait_for_search_ally(wait->np, follower, wait->was_waiting);
}

// 0x006A9460 (:1222)
int search_wait_for_ally_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    const ai_search_location_info* location = ai_search_get_location_info(goal_setup_data.search_handle);
    if (location == nullptr || location->search_type != AI_ST_EXTERNAL_AREA)
    {
        return -1;
    }
    wait_for_ally_data data;
    data.np = np;
    data.was_waiting = np->get_ai()->planner.get_current_action() == &the_instance;
    unsigned int follower;
    if (ai_formation_collect_followers(np, &follower, 1, wait_for_ally_follower_callback, &data) <= 0)
    {
        return -1;
    }
    object* objp = follower != 0xFFFFFFFF ? World.object_from_handle(follower) : nullptr;
    my_data.use_target = objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_NPC ? objp : nullptr;
    return my_data.use_target != nullptr ? 110 : -1;
}

// 0x005F16C0 (:1250)
void search_wait_for_ally_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    np->get_ai()->desired_follow_target = setup_data.use_target->get_handle();
}

// 0x0066B250 (:1256)
ai_results search_wait_for_ally_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    ai->stop_moving();
    human_set_combat_ready(np, false, 0);
    const object* objp = ai->desired_follow_target != 0xFFFFFFFF ? World.object_from_handle(ai->desired_follow_target) : nullptr;
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC || objp->is_killed() || !static_cast<const npc*>(objp)->get_ai()->is_following_leader(np))
    {
        return AI_RESULT_FAILURE;
    }
    ai->perception.set_attention_mode(ATTENTION_SEARCH, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    ai->action_timer.set_rand(1000, 2000);
    return AI_RESULT_SUCCESS;
}

// 0x006BAB40 (:1278)
ai_results search_wait_for_ally_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    keep_searcher_alive(ai->desired_search_handle);
    const object* objp = ai->desired_follow_target != 0xFFFFFFFF ? World.object_from_handle(ai->desired_follow_target) : nullptr;
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC || objp->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    const npc* follower = static_cast<const npc*>(objp);
    if (!follower->get_ai()->is_following_leader(np))
    {
        return AI_RESULT_FAILURE;
    }
    if (vec_dist_squared(follower->pos, np->pos) <= ALLY_WAIT_DIST_SQ && follower->cur_movement_state == HMS_IDLE)
    {
        return AI_RESULT_SUCCESS;
    }
    if (ai->action_timer.valid() && ai->action_timer.elapsed())
    {
        ai->action_timer.invalidate();
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        human_set_look_at_target(np, ai->desired_follow_target, Default_target_object_offset, false, -1.0f);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x006A9520 (:1307)
int search_from_cover_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    const ai_system* ai = np->get_ai();
    if (np->hflags.riot_shield != 0 && np->shield_handle != 0xFFFFFFFF)
    {
        return -1;
    }
    const ai_search_location_info* location = ai_search_get_location_info(goal_setup_data.search_handle);
    if (location == nullptr || location->search_type != AI_ST_EXTERNAL_AREA)
    {
        return -1;
    }
    vector pos, los_pos;
    nav_cell* ncp = nullptr;
    cover_node* node = nullptr;
    if (!ai_search_get_current_exterior_search_pos(pos, los_pos, ncp, goal_setup_data.search_handle, &node))
    {
        return -1;
    }
    if (node == nullptr || node->is_disabled() || ai->is_pathing_to(los_pos, 0.25f, 1.0f) || node->is_occupied(np) != nullptr)
    {
        ai_search_abandon_exterior_search_cover(goal_setup_data.search_handle);
        return -1;
    }
    const cover_node_stances stance = cover_pick_stance(node, pos);
    if (stance == COVER_STANCE_UNDEFINED)
    {
        ai_search_abandon_exterior_search_cover(goal_setup_data.search_handle);
        node->disable(COVER_DISABLED_NO_STANCES, 15000); // ms
        return -1;
    }
    if (stance == COVER_STANCE_CROUCH_LEFT || stance == COVER_STANCE_CROUCH_RIGHT)
    {
        return -1;
    }
    my_data.use_target = node;
    my_data.cover_stance = stance;
    return 250;
}

// 0x005F16E0 (:1358)
void search_from_cover_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    human_combat_cover_reserve_node(np, static_cast<cover_node*>(setup_data.use_target));
    np->get_ai()->desired_cover_stance = setup_data.cover_stance;
}

// 0x006BACB0 (:1370)
ai_results search_from_cover_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        ai->action_timer.set_rand(500, 700);
    }
    ai->stop_moving();
    ai->perception.set_attention_mode(ATTENTION_AIM_STRAIGHT_AHEAD, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    human_set_combat_ready(np, true, 0);
    keep_searcher_alive(ai->desired_search_handle);
    ai->planner.lock_action(true);
    return AI_RESULT_SUCCESS;
}

// 0x006BAD80 (:1392). The call's arguments are from the asm (0x006BAE52): the decompile shifts them.
ai_results search_from_cover_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human_set_combat_ready(np, true, 0);
    keep_searcher_alive(ai->desired_search_handle);
    if (ai->perception.get_first_belief(AI_BELIEF_HAZARD) != nullptr || ai->perception.get_first_belief(AI_BELIEF_ENEMY_POSITION) != nullptr)
    {
        ai_search_abandon_exterior_search_cover(ai->desired_search_handle);
        human_combat_cover_abandon_node(np);
        return AI_RESULT_SUCCESS;
    }
    if (!ai->action_timer.elapsed())
    {
        return AI_RESULT_IN_PROGRESS;
    }
    ai_search_abandon_exterior_search_cover(ai->desired_search_handle);
    human_combat_cover_abandon_node(np);
    vector pos, los_pos;
    nav_cell* ncp;
    if (!ai_search_get_current_exterior_search_pos(pos, los_pos, ncp, ai->desired_search_handle, nullptr))
    {
        return AI_RESULT_FAILURE;
    }
    const ai_pathfind_results result = ai->set_direct_movement_to_pos(los_pos, nav_cell_get_handle(ncp), HMS_WALK, HST_STAND, true);
    if (result == AI_PF_SUCCESS)
    {
        return AI_RESULT_SUCCESS;
    }
    return result > AI_PF_RAGDOLLED ? AI_RESULT_FAILURE : AI_RESULT_IN_PROGRESS;
}

// 0x006847A0 (:1435)
int search_cover_stepout_action::get_cost(ai_action_setup_data&, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    if (np->hflags.riot_shield != 0 && np->shield_handle != 0xFFFFFFFF)
    {
        return -1;
    }
    vector pos, los_pos;
    nav_cell* ncp = nullptr;
    cover_node* node = nullptr;
    if (!ai_search_get_current_exterior_search_pos(pos, los_pos, ncp, goal_setup_data.search_handle, &node) || node == nullptr)
    {
        return -1;
    }
    vector stepped_out;
    const cover_node_stances stance = cover_pick_stance(node, pos);
    if (stance == COVER_STANCE_STAND_RIGHT)
    {
        stepped_out = node->pos + node->orient.rvec * 0.5f;
    }
    else if (stance == COVER_STANCE_STAND_LEFT)
    {
        stepped_out = node->pos - node->orient.rvec * 0.5f;
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        return -1;
    }
    if (vec_dist_squared(np->pos, stepped_out) > 0.25f) // 0.5 m
    {
        const cover_node* current = human_combat_cover_get_node(np);
        if (current != nullptr && current != node)
        {
            desired_conditions |= ai_plan_conditions(59);
        }
        return 50;
    }
    desired_conditions &= ~ai_plan_conditions(60);
    return 0;
}

// 0x005F1710 (:1485)
void search_cover_stepout_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x006BAEE0 (:1491)
ai_results search_cover_stepout_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    if (human_combat_cover_get_node(np) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai->stop_moving();
    human_set_combat_ready(np, true, 0);
    keep_searcher_alive(ai->desired_search_handle);
    human_set_stance(np, HST_STAND);
    ai->perception.set_attention_mode(ATTENTION_AIM_STRAIGHT_AHEAD, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    const cover_node_stances stance = ai->desired_cover_stance;
    if (stance != COVER_STANCE_STAND_LEFT && stance != COVER_STANCE_STAND_RIGHT)
    {
        return AI_RESULT_FAILURE;
    }
    ai->planner.lock_action(true);
    human_combat_cover_abandon_node(np);
    np->char_inst->play_action((animation_action)(ANIMATION_ACTION_COVER_STAND_LEFT_STEP_OUT + (stance != COVER_STANCE_STAND_LEFT ? 4 : 0)), ANIM_ALIAS_COVER_STEP_OUT, 1.0f, 0x4020);
    np->steering_vector = ZERO_VECTOR;
    return AI_RESULT_SUCCESS;
}

// 0x006BB020 (:1526)
ai_results search_cover_stepout_action::do_action(npc* np) const
{
    human_set_combat_ready(np, true, 0);
    keep_searcher_alive(np->get_ai()->desired_search_handle);
    return np->char_inst->is_playing_action(ANIM_ALIAS_COVER_STEP_OUT) ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x006414D0 (:1540)
int watch_area_action::get_cost(ai_action_setup_data&, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    if (goal_setup_data.get_attention_belief(np) == nullptr)
    {
        return -1;
    }
    const object_guard_node* current = np->get_ai()->get_guard_node();
    if (current != nullptr && !current->is_reserved(np->get_handle()))
    {
        current = nullptr;
    }
    const object* wanted = goal_setup_data.use_target;
    if (wanted == nullptr || wanted->get_obj_type() != OT_GUARD_NODE)
    {
        return -1;
    }
    if (current != nullptr && wanted != current)
    {
        desired_conditions |= ai_plan_conditions(36);
    }
    return 500;
}

// 0x005F1720 (:1565)
void watch_area_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x006415D0 (:1571)
ai_results watch_area_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    object_guard_node* node = ai->get_guard_node();
    if (node == nullptr || !node->is_reserved(np->get_handle()) || !ai->reserve_guard_node(node, -128))
    {
        return AI_RESULT_FAILURE;
    }
    ai->stop_moving();
    human_set_stance(np, HST_STAND);
    human_set_combat_ready(np, true, 0);
    ai->perception.set_attention_mode(ATTENTION_SEARCH, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    return AI_RESULT_SUCCESS;
}

// 0x00641790 (:1590)
ai_results watch_area_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    object_guard_node* node = ai->get_guard_node();
    if (node == nullptr || !node->is_reserved(np->get_handle()) || !ai->reserve_guard_node(node, -128) || ai->perception.get_attention_belief() == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    return AI_RESULT_IN_PROGRESS;
}

search_goal search_goal::the_instance; // 0x01DCA70C

// 0x0061E4E0 (.h:14)
search_goal::search_goal()
    : ai_goal(ai_plan_conditions(92), "search")
{
}

ai_goal* search_goal::get_instance()
{
    return &the_instance;
}

// 0x005E3370
search_goal::~search_goal()
{
}

watch_area_goal watch_area_goal::the_instance; // 0x01D75950

// 0x0061E620 (.h:16)
watch_area_goal::watch_area_goal()
    : ai_goal(ai_plan_conditions(93), "watch area")
{
}

ai_goal* watch_area_goal::get_instance()
{
    return &the_instance;
}

// 0x005E33A0
watch_area_goal::~watch_area_goal()
{
}

accompany_search_goal accompany_search_goal::the_instance; // 0x01DA03D0

// 0x0061E580 (.h:15)
accompany_search_goal::accompany_search_goal()
    : ai_goal(ai_plan_conditions(95), "accompany search")
{
}

ai_goal* accompany_search_goal::get_instance()
{
    return &the_instance;
}

// 0x005E3380
accompany_search_goal::~accompany_search_goal()
{
}

search_building_action search_building_action::the_instance; // 0x01D722AC

// 0x0061F4E0 (.h:23)
search_building_action::search_building_action()
    : ai_action("search building", ai_plan_conditions_from_bits({4, 9, 18, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({92}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* search_building_action::get_instance()
{
    return &the_instance;
}

// 0x005E3400
search_building_action::~search_building_action()
{
}

// 0x0061F830 (.h:23)
int search_building_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

search_get_external_location search_get_external_location::the_instance; // 0x01D71EE8

// 0x0061E6C0 (.h:19)
search_get_external_location::search_get_external_location()
    : ai_action("search get ext location", ai_plan_conditions_from_bits({4, 9, 14, 18, 47, 136, 137, 145, 153}), ai_plan_conditions_from_bits({92}))
{
}

ai_action* search_get_external_location::get_instance()
{
    return &the_instance;
}

// 0x005E33B0
search_get_external_location::~search_get_external_location()
{
}

// 0x0061EA00 (.h:19)
int search_get_external_location::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

search_external_area_action search_external_area_action::the_instance; // 0x01DC98C8

// 0x0061EA40 (.h:20)
search_external_area_action::search_external_area_action()
    : ai_action("search area", ai_plan_conditions_from_bits({4, 9, 14, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({92}))
{
    flags.has_enter = 1;
}

ai_action* search_external_area_action::get_instance()
{
    return &the_instance;
}

// 0x005E33C0
search_external_area_action::~search_external_area_action()
{
}

// 0x0061ED90 (.h:20)
int search_external_area_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

search_wait_for_ally_action search_wait_for_ally_action::the_instance; // 0x01DC94C4

// 0x0061EDD0 (.h:21)
search_wait_for_ally_action::search_wait_for_ally_action()
    : ai_action("search wait for backup", ai_plan_conditions_from_bits({4, 9, 14, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({92}))
{
    flags.has_enter = 1;
}

ai_action* search_wait_for_ally_action::get_instance()
{
    return &the_instance;
}

// 0x005E33E0
search_wait_for_ally_action::~search_wait_for_ally_action()
{
}

// 0x0061F120 (.h:21)
int search_wait_for_ally_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

watch_area_action watch_area_action::the_instance; // 0x01DDB9D0

// 0x0061FA20 (.h:30)
watch_area_action::watch_area_action()
    : ai_action("watch area", ai_plan_conditions_from_bits({0, 2, 9, 18, 36, 47, 58}), ai_plan_conditions_from_bits({93}))
{
    flags.has_enter = 1;
}

ai_action* watch_area_action::get_instance()
{
    return &the_instance;
}

// 0x005E3430
watch_area_action::~watch_area_action()
{
}

// 0x0061FC90 (.h:30)
int watch_area_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

accompany_search_action accompany_search_action::the_instance; // 0x01D67F78

// 0x0061F150 (.h:22)
accompany_search_action::accompany_search_action()
    : ai_action("accompany search", ai_plan_conditions_from_bits({4, 9, 18, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({95}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* accompany_search_action::get_instance()
{
    return &the_instance;
}

// 0x005E33F0
accompany_search_action::~accompany_search_action()
{
}

// 0x0061F4A0 (.h:22)
int accompany_search_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

search_from_cover_action search_from_cover_action::the_instance; // 0x01D7596C

// 0x0061F860 (.h:26)
search_from_cover_action::search_from_cover_action()
    : ai_action("search from cover", ai_plan_conditions_from_bits({64}), ai_plan_conditions_from_bits({92}))
{
    flags.has_enter = 1;
}

ai_action* search_from_cover_action::get_instance()
{
    return &the_instance;
}

// 0x005E3410
search_from_cover_action::~search_from_cover_action()
{
}

// 0x0061F8C0 (.h:26)
int search_from_cover_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

search_cover_stepout_action search_cover_stepout_action::the_instance; // 0x01DC9DC8

// 0x0061F8F0 (.h:27)
search_cover_stepout_action::search_cover_stepout_action()
    : ai_action("search cover stepout", ai_plan_conditions_from_bits({0, 2, 60}), ai_plan_conditions_from_bits({64}))
{
    flags.has_enter = 1;
}

ai_action* search_cover_stepout_action::get_instance()
{
    return &the_instance;
}

// 0x005E3420
search_cover_stepout_action::~search_cover_stepout_action()
{
}

// 0x0061F9F0 (.h:27)
int search_cover_stepout_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

