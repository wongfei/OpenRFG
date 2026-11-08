// Original: volition/rfg/code/ai/planning.cpp

#include "volition/rfg/code/ai/planning.h"

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_rate_limiters.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/objects/humans/human_combat_cover.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_spinebend.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/spawn/ambient_prop_info.h"
#include "volition/vlib/code/math/rand.h"

#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <string.h>

// ai_planning_system_init adds four nodes on each of its 0x80 passes.
static action_list Action_list_nodes[0x200]; // 0x01D65990
static action_list* Free_action_list_nodes; // 0x01D65944

// 0x016415DC: 1 in rfg.exe's .data, and make_plan_astar_search is its only reference (a read), so non-monotonic plans are
// always rejected and the plan-contains-action branch there never runs.
static bool Ai_allow_only_monotonic_plans = true;

// idle_goal's condition (data[2] == 8 in rfg.exe): rank_goals skips it, do_replan falls back to it.
static const unsigned int UNRANKED_GOAL_CONDITION_BIT = 67;

// 0x005EFBB0 (:303)
void append_to_plan(action_list*& plan, const ai_action* act)
{
    if (act == nullptr)
    {
        return;
    }
    action_list* node = Free_action_list_nodes;
    if (node == nullptr)
    {
        keen::debug::nativePrintArgs("Out of AI plan nodes!  Please contact Dave Fried or another AI programmer immediately!");
        return;
    }
    if (node->next == node)
    {
        Free_action_list_nodes = nullptr;
    }
    else
    {
        Free_action_list_nodes = node->next;
        if (node->prev != nullptr)
        {
            node->prev->next = node->next;
        }
        if (node->next != nullptr)
        {
            node->next->prev = node->prev;
        }
    }
    node->prev = nullptr;
    node->next = nullptr;
    node->act = act;
    if (plan == nullptr)
    {
        node->next = node;
        node->prev = node;
        plan = node;
        return;
    }
    node->prev = plan->prev;
    node->next = plan;
    plan->prev->next = node;
    plan->prev = node;
}

// 0x005EFC50 (:322)
void remove_from_plan(action_list*& plan, action_list* plan_el)
{
    if (plan == plan_el && plan_el->next == plan_el)
    {
        plan = nullptr;
    }
    else
    {
        if (plan == plan_el)
        {
            plan = plan_el->next;
        }
        if (plan_el->prev != nullptr)
        {
            plan_el->prev->next = plan_el->next;
        }
        if (plan_el->next != nullptr)
        {
            plan_el->next->prev = plan_el->prev;
        }
    }
    plan_el->next = nullptr;
    plan_el->prev = nullptr;

    action_list* free_head = Free_action_list_nodes;
    if (free_head == nullptr)
    {
        plan_el->next = plan_el;
        plan_el->prev = plan_el;
        Free_action_list_nodes = plan_el;
        return;
    }
    plan_el->prev = free_head->prev;
    plan_el->next = free_head;
    free_head->prev->next = plan_el;
    free_head->prev = plan_el;
}

// 0x005EFCD0 (:332): splices the whole ring into the free ring.
void free_plan(action_list*& plan)
{
    action_list* free_head = Free_action_list_nodes;
    if (free_head == nullptr)
    {
        Free_action_list_nodes = plan;
        plan = nullptr;
        return;
    }
    if (plan != nullptr)
    {
        free_head->prev->next = plan;
        plan->prev->next = free_head;
        action_list* plan_tail = plan->prev;
        plan->prev = free_head->prev;
        free_head->prev = plan_tail;
    }
    plan = nullptr;
}

// 0x0063F710 (:339)
void initialize_search_node(ai_planner_search_node& node, ai_plan_conditions desired_conditions, const ai_action* next_action, ai_planner_search_node* next_node, int incurred_cost, int search_depth, const ai_action_setup_data& action_data, bool include_in_plan, bool non_monotonic)
{
    node.desired_conditions = desired_conditions;
    node.next_action = next_action;
    node.next_node = next_node;
    node.incurred_cost = incurred_cost;
    // 100 per unmet condition.
    node.heuristic_cost = node.desired_conditions.bit_count() * 100;
    node.estimated_total_cost = node.heuristic_cost + incurred_cost;
    node.search_depth = search_depth;
    node.action_data = action_data;
    node.flags.already_explored = 0;
    node.flags.include_action_in_plan = include_in_plan;
    node.satisfied_conditions = next_action != nullptr ? (next_node->satisfied_conditions | next_action->get_post_conditions()) : ai_plan_conditions();
    node.flags.non_monotonic = non_monotonic;
}

// 0x0063F860 (:360)
void update_search_node(ai_planner_search_node& node, const ai_action* next_action, ai_planner_search_node* next_node, int incurred_cost, int search_depth, const ai_action_setup_data& action_data, bool include_in_plan, bool non_monotonic)
{
    node.next_action = next_action;
    node.next_node = next_node;
    node.incurred_cost = incurred_cost;
    node.heuristic_cost = node.desired_conditions.bit_count() * 100;
    node.estimated_total_cost = node.heuristic_cost + incurred_cost;
    node.search_depth = search_depth;
    node.action_data = action_data;
    node.flags.already_explored = 0;
    node.flags.include_action_in_plan = include_in_plan;
    node.satisfied_conditions = next_node->satisfied_conditions | next_action->get_post_conditions();
    node.flags.non_monotonic = non_monotonic;
}

// 0x005EFD90
ai_planner_search_node::ai_planner_search_node()
{
}

// 0x005F01E0
ai_planner::action_avoidance_record::action_avoidance_record()
{
}

// 0x005EFD20 (:383)
bool ai_planner::require_high_priority_replan() const
{
    if (np->get_ai()->flags.reduced_processing)
    {
        return false;
    }
    const timestamp& combat_ready = np->combat.combat_ready_timestamp;
    return combat_ready.valid() && !combat_ready.elapsed();
}

// 0x005EFD60 (:395)
bool ai_planner::can_request_replan() const
{
    return !np->get_ai()->flags.stunned && !np->hflags.is_on_ladder;
}

// 0x0063F970 (:412): keeps the five fittest goals, an earlier goal in the pool winning a tie.
void ai_planner::rank_goals(ai_plan_conditions working_conditions)
{
    memset(goal_rankings, 0, sizeof(goal_rankings));
    for (int g = 0; g < ai_goal::num_goal_types; g++)
    {
        ai_goal* goal = ai_goal::goal_pool[g];
        if (goal->get_conditions() == ai_plan_conditions(UNRANKED_GOAL_CONDITION_BIT))
        {
            continue;
        }
        // Skips a goal whose conditions are all met.
        if ((goal->get_conditions() & ~working_conditions) == ai_plan_conditions())
        {
            continue;
        }
        const int fitness = goal->get_fitness(np, working_conditions);
        if (fitness <= 0)
        {
            continue;
        }
        for (int i = 0; i < 5; i++)
        {
            if (fitness > goal_rankings[i].fitness)
            {
                for (int j = 4; j > i; j--)
                {
                    goal_rankings[j] = goal_rankings[j - 1];
                }
                goal_rankings[i].fitness = fitness;
                goal_rankings[i].goal = goal;
                break;
            }
        }
    }
}

// 0x00669740 (:493): A* from the goal's unmet conditions backwards through the actions' post and preconditions.
bool ai_planner::make_plan_astar_search(ai_plan_conditions working_conditions, const ai_goal* new_goal, const ai_goal_setup_data& goal_setup_data)
{
    // 0x100 nodes; running out abandons the expansion of the current node, not the search.
    static ai_planner_search_node search_nodes[256];

    initialize_search_node(search_nodes[0], new_goal->get_conditions() & ~working_conditions, nullptr, nullptr, 0, 0, ai_action_setup_data(), false, false);
    int num_nodes = 1;

    for (;;)
    {
        int best = -1;
        int best_cost = 0;
        for (int i = 0; i < num_nodes; i++)
        {
            if (!search_nodes[i].flags.already_explored && (best == -1 || search_nodes[i].estimated_total_cost < best_cost))
            {
                best_cost = search_nodes[i].estimated_total_cost;
                best = i;
            }
        }
        if (best == -1)
        {
            return false;
        }

        ai_planner_search_node* node = &search_nodes[best];
        node->flags.already_explored = 1;

        if (node->desired_conditions == ai_plan_conditions())
        {
            const ai_planner_search_node* first = node;
            while (first->next_action != nullptr && !first->flags.include_action_in_plan)
            {
                first = first->next_node;
            }
            cleanup_current_plan(false, first->next_action);

            ai_system* ai = np->get_ai();
            ai->flags.less_aggressive_melee = false;
            ai->desired_cover_stance = COVER_STANCE_UNDEFINED;
            ai->desired_cover_firing_position = COVER_FIRE_UNDEFINED;
            ai->desired_vehicle = 0xFFFFFFFF;
            ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_UNDEFINED;
            ai->desired_vehicle_speed = -1.0f;
            ai->desired_vehicle_dist_to_goal_pos = 3.0f;

            new_goal->commit_setup_data(np, goal_setup_data);
            for (const ai_planner_search_node* step = node; step->next_action != nullptr; step = step->next_node)
            {
                if (step->flags.include_action_in_plan)
                {
                    step->next_action->commit_setup_data(np, step->action_data);
                    append_to_plan(plan, step->next_action);
                }
            }

            planner_execution_state = PLANNER_EX_ENTER_ACTION;
            if (!cur_action_timer.valid())
            {
                cur_action_timer.set(0);
                cur_action_frame = 0;
            }
            return true;
        }

        for (int a = 0; a < ai_action::num_action_types; a++)
        {
            const ai_action* act = ai_action::action_pool[a];
            if ((act->get_post_conditions() & node->desired_conditions) == ai_plan_conditions() || is_avoiding_action(act))
            {
                continue;
            }

            const ai_plan_conditions pre = act->get_pre_conditions();
            const bool non_monotonic = node->flags.non_monotonic || (pre & node->satisfied_conditions);
            if (Ai_allow_only_monotonic_plans)
            {
                if (non_monotonic)
                {
                    continue;
                }
            }
            else if (ai_plan_contains_action(node, act))
            {
                continue;
            }

            ai_plan_conditions desired = (node->desired_conditions & ~act->get_post_conditions()) | (pre & ~working_conditions);
            ai_action_setup_data action_data;
            const int cost = act->get_cost(action_data, desired, np, goal_setup_data, node);
            if (cost < 0)
            {
                continue;
            }
            if ((node->desired_conditions & desired) == node->desired_conditions)
            {
                keen::debug::nativePrintArgs("Action has undone all of its post-conditions!");
                continue;
            }

            // A zero cost step only rewrites conditions; it is searched through but not run.
            const bool include = cost != 0 && act->include_in_plan();
            const int total_cost = node->incurred_cost + cost;

            bool known = false;
            for (int i = 0; i < num_nodes; i++)
            {
                ai_planner_search_node& other = search_nodes[i];
                if (other.desired_conditions == desired)
                {
                    if (!other.flags.already_explored && total_cost < other.incurred_cost)
                    {
                        update_search_node(other, act, node, total_cost, node->search_depth + 1, action_data, include, non_monotonic);
                    }
                    known = true;
                    break;
                }
            }
            if (known)
            {
                continue;
            }

            if (num_nodes >= 256)
            {
                break;
            }
            initialize_search_node(search_nodes[num_nodes], desired, act, node, total_cost, node->search_depth + 1, action_data, include, non_monotonic);
            num_nodes++;
        }
    }
}

// 0x006BA160 (:787)
bool ai_planner::do_replan()
{
    ai_plan_conditions working_conditions = ai_plan_conditions_create_working_set(np);

    if (flags.should_rerank_goals)
    {
        flags.should_rerank_goals = false;
        last_goal = cur_goal;
        rank_goals(working_conditions);
    }

    int rank = 0;
    const ai_goal* goal = nullptr;
    for (; rank < 5; rank++)
    {
        goal_rank& entry = goal_rankings[rank];
        if (entry.goal != nullptr && entry.failure_state == GOAL_FAILURE_NONE)
        {
            if (!entry.goal->conditions_satisfied(working_conditions))
            {
                goal = entry.goal;
                break;
            }
            entry.failure_state = GOAL_FAILURE_CONDITIONS;
        }
    }
    if (goal == nullptr)
    {
        goal = idle_goal::get_instance();
    }

    ai_goal_setup_data setup_data(goal);
    if (!goal->store_setup_data(setup_data, np))
    {
        if (goal != idle_goal::get_instance() && rank < 5)
        {
            goal_rankings[rank].failure_state = GOAL_FAILURE_INITIALIZATION;
        }
        return false;
    }

    setup_data.initial_working_conditions = working_conditions;
    if (!make_plan_astar_search(working_conditions, goal, setup_data))
    {
        if (goal != idle_goal::get_instance() && rank < 5)
        {
            goal_rankings[rank].failure_state = GOAL_FAILURE_PLAN;
        }
        return false;
    }

    if (cur_goal != goal)
    {
        cur_goal = goal;
        cur_goal_timer.set(0);
    }
    flags.should_replan = false;
    // Replans every 1.5-3 s (0x5DC-3000), 4-6 s with reduced processing.
    const bool reduced = np->get_ai()->flags.reduced_processing;
    replan_timer.set(reduced ? int_rand_range(4000, 6000) : int_rand_range(1500, 3000));
    flags.should_rerank_goals = true;
    return true;
}

// 0x0063FB40 (:911)
bool ai_planner::do_action()
{
    action_list* current = plan;
    if (current == nullptr)
    {
        return false;
    }

    action_list* next = current->next != current ? current->next : nullptr;
    const ai_action* act = current->act;
    const ai_action* next_action = next != nullptr ? next->act : nullptr;
    if (act == nullptr)
    {
        return false;
    }

    if (planner_execution_state == PLANNER_EX_CLEANUP_ACTION && !act->has_cleanup_function())
    {
        remove_from_plan(plan, current);
        flags.action_locked = false;
        if (next_action == nullptr)
        {
            last_action_time = -1;
            cur_action_timer.invalidate();
            cur_action_frame = -1;
            last_action = nullptr;
            planner_execution_state = PLANNER_EX_NONE;
            return true;
        }
        last_action = act;
        last_action_time = cur_action_timer.time_since();

        action_list* after_next = next->next;
        const ai_action* new_next_action = (after_next == plan || after_next == nullptr) ? nullptr : after_next->act;

        planner_execution_state = PLANNER_EX_ENTER_ACTION;
        cur_action_timer.set(0);
        cur_action_frame = 0;

        act = next_action;
        next_action = new_next_action;
        current = next;
    }

    if (planner_execution_state == PLANNER_EX_ENTER_ACTION && !act->has_enter_function())
    {
        planner_execution_state = PLANNER_EX_DO_ACTION;
    }

    last_action_executed = act;

    ai_results result;
    switch (planner_execution_state)
    {
    case PLANNER_EX_ENTER_ACTION:
        // After the first frame enter sees itself as the previous action.
        result = act->enter(np, cur_action_frame == 0 ? last_action : act);
        break;
    case PLANNER_EX_DO_ACTION:
        result = act->do_action(np);
        break;
    case PLANNER_EX_CLEANUP_ACTION:
        result = act->cleanup(np, next_action);
        if (result == AI_RESULT_IN_PROGRESS)
        {
            result = AI_RESULT_FAILURE;
        }
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        return false;
    }

    flags.executed_this_frame = true;
    cur_action_frame++;

    if (result == AI_RESULT_IN_PROGRESS)
    {
        return true;
    }
    if (result == AI_RESULT_FAILURE)
    {
        flags.action_locked = false;
        cleanup_current_plan(planner_execution_state == PLANNER_EX_CLEANUP_ACTION, nullptr);
        return true;
    }

    switch (planner_execution_state)
    {
    case PLANNER_EX_ENTER_ACTION:
        planner_execution_state = PLANNER_EX_DO_ACTION;
        return true;
    case PLANNER_EX_DO_ACTION:
        planner_execution_state = PLANNER_EX_CLEANUP_ACTION;
        flags.action_locked = false;
        return true;
    case PLANNER_EX_CLEANUP_ACTION:
        if (next_action == nullptr)
        {
            last_action = nullptr;
            last_action_time = -1;
            cur_action_timer.invalidate();
            cur_action_frame = -1;
        }
        else
        {
            last_action = act;
            last_action_time = cur_action_timer.time_since();
            cur_action_timer.set(0);
            cur_action_frame = 0;
        }
        flags.action_locked = false;
        remove_from_plan(plan, current);
        planner_execution_state = plan != nullptr ? PLANNER_EX_ENTER_ACTION : PLANNER_EX_NONE;
        return true;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        return false;
    }
}

// 0x005EFE10 (:1119)
void ai_planner::cleanup_current_plan(bool failed_during_cleanup, const ai_action* next_plan_first_action)
{
    if (plan == nullptr)
    {
        return;
    }
    const ai_action* act = plan->act;
    const bool ran = act == last_action_executed;
    if (act->has_cleanup_function() && !failed_during_cleanup && ran)
    {
        planner_execution_state = PLANNER_EX_CLEANUP_ACTION;
        act->cleanup(np, next_plan_first_action);
        flags.executed_this_frame = true;
    }
    flags.action_locked = false;
    if (next_plan_first_action == nullptr)
    {
        last_action = nullptr;
        last_action_time = -1;
        cur_action_timer.invalidate();
        cur_action_frame = -1;
    }
    else
    {
        if (ran)
        {
            last_action = act;
            last_action_time = cur_action_timer.time_since();
        }
        if (next_plan_first_action != last_action)
        {
            cur_action_timer.set(0);
            cur_action_frame = 0;
        }
    }
    free_plan(plan);
    planner_execution_state = PLANNER_EX_NONE;
}

// 0x0063FE00 (:1211)
ai_planner::ai_planner()
    : plan(nullptr)
{
}

// 0x005F9C60
ai_planner::~ai_planner()
{
}

// 0x005EFEE0 (:1223)
void ai_planner::clear()
{
    cleanup_current_plan(false, nullptr);
    if (cur_goal != nullptr)
    {
        last_goal = cur_goal;
        cur_goal = nullptr;
        cur_goal_timer.invalidate();
    }
    last_action = nullptr;
    last_action_executed = nullptr;
    last_action_time = -1;
    cur_action_timer.invalidate();
    cur_action_frame = -1;
    planner_execution_state = PLANNER_EX_NONE;
    flags.action_locked = false;
    flags.allow_prerequest = false;
    flags.executed_this_frame = false;
    flags.should_rerank_goals = true;
    flags.should_replan = false;
    replan_timer.set(0);
    memset(goal_rankings, 0, sizeof(goal_rankings));
}

// 0x005EFFC0 (:1264)
void ai_planner::init(npc* np_)
{
    np = np_;
    flags.action_locked = false;
    flags.allow_prerequest = false;
    flags.executed_this_frame = false;
    flags.should_rerank_goals = true;
    flags.should_replan = false;
    plan = nullptr;
    cur_goal = nullptr;
    cur_goal_timer.invalidate();
    last_goal = nullptr;
    last_action = nullptr;
    last_action_executed = nullptr;
    last_action_time = -1;
    cur_action_timer.invalidate();
    cur_action_frame = -1;
    planner_execution_state = PLANNER_EX_NONE;
    replan_timer.set(0);
    memset(goal_rankings, 0, sizeof(goal_rankings));
}

// 0x005F0040 (:1310)
void ai_planner::stun()
{
    ai_system* ai = np->get_ai();
    cleanup_current_plan(false, nullptr);
    human_combat_cover_abandon_node(np);
    human_combat_abandon_turret(np);
    human_set_combat_ready(np, false, 0);
    human_weapons_firearm_release_trigger(np);
    human_weapons_drop_thrown_weapon(np);
    np->release_all_action_nodes();
    amb_prop_detach_props(np, false);
    if (np->vehicle_handle == 0xFFFFFFFF)
    {
        ai->stop_moving();
        np->steering_vector = ZERO_VECTOR;
        np->turnto_target = ZERO_VECTOR;
    }
    if (!np->hflags.in_fetal_position)
    {
        human_override_default_animation_state(np, false);
    }
    clear();
    ai->perception.enter_idle_attention();
    human_spinebend_reset_target(np);
    np->hflags.anchor_orient = 1;
}

// 0x006A7680 (:1385)
void ai_planner::do_frame_pre()
{
    if (replan_timer.elapsed())
    {
        request_replan();
        replan_timer.invalidate();
    }
    if (!can_request_replan())
    {
        return;
    }

    ai_replan_priorities priority;
    if (plan == nullptr && !flags.should_rerank_goals)
    {
        priority = static_cast<ai_replan_priorities>(AI_REPLAN_PRIORITY_IN_REPLAN_NORMAL + (require_high_priority_replan() ? 2 : 0));
    }
    else
    {
        if (plan != nullptr && (!flags.should_replan || flags.action_locked))
        {
            return;
        }
        priority = static_cast<ai_replan_priorities>(AI_REPLAN_PRIORITY_NORMAL + (require_high_priority_replan() ? 2 : 0));
    }
    if (flags.allow_prerequest)
    {
        Ai_replan_rate_limiter.request_queue(np, priority);
        flags.allow_prerequest = false;
    }
}

// 0x006BA370 (:1459)
void ai_planner::do_frame()
{
    flags.executed_this_frame = false;
    flags.allow_prerequest = true;
    if ((plan == nullptr || (flags.should_replan && !flags.action_locked))
        && Ai_replan_rate_limiter.request_immediate(np, AI_REPLAN_PRIORITY_LOW))
    {
        do_replan();
    }
    if (flags.executed_this_frame)
    {
        if (plan != nullptr)
        {
            return;
        }
    }
    else if (plan != nullptr)
    {
        do_action();
        return;
    }
    planner_execution_state = PLANNER_EX_NONE;
}

// 0x005F01B0 (:1557)
void ai_planner::request_replan()
{
    if (!flags.should_replan)
    {
        flags.should_replan = true;
        flags.should_rerank_goals = true;
    }
}

// 0x005F01C0 (:1567)
void ai_planner::lock_action(bool lock)
{
    if (plan != nullptr)
    {
        flags.action_locked = lock;
    }
}

// 0x0063FE40 (:1582): a repeat moves to the newest slot with the new time; otherwise the oldest record falls off.
void ai_planner::avoid_action(const ai_action* act, int time)
{
    for (int i = 0; i < action_avoidance_records.size(); i++)
    {
        if (action_avoidance_records[i].action == act)
        {
            for (; i > 0; i--)
            {
                action_avoidance_records[i] = action_avoidance_records[i - 1];
            }
            action_avoidance_records[0].action = act;
            action_avoidance_records[0].timer.set(time);
            return;
        }
    }
    action_avoidance_record record;
    record.action = act;
    record.timer.set(time);
    action_avoidance_records.add(record);
}

// 0x0063FF90 (:1614)
bool ai_planner::is_avoiding_action(const ai_action* act) const
{
    circ_array<action_avoidance_record, 8>& records = const_cast<circ_array<action_avoidance_record, 8>&>(action_avoidance_records);
    for (int i = 0; i < records.size(); i++)
    {
        if (records[i].action == act)
        {
            return !records[i].timer.elapsed();
        }
    }
    return false;
}

// 0x005DC8E0 (.h:80)
const ai_goal* ai_planner::get_current_goal() const
{
    return cur_goal;
}

// 0x005DC8F0 (.h:81)
const ai_goal* ai_planner::get_last_goal() const
{
    return last_goal;
}

// 0x005DC900 (.h:83)
bool ai_planner::plan_continues_after_current_action() const
{
    return plan != nullptr && plan->next != plan && plan->next != nullptr;
}

// 0x005DC920 (.h:87)
int ai_planner::get_time_in_current_action() const
{
    return cur_action_timer.valid() ? cur_action_timer.time_since() : -1;
}

// 0x005DC940 (.h:89)
int ai_planner::get_time_in_current_goal() const
{
    return cur_goal_timer.valid() ? cur_goal_timer.time_since() : -1;
}

// 0x005F01F0 (:1635)
bool ai_planner::plan_contains(const ai_action* act) const
{
    for (const action_list* el = plan; el != nullptr;)
    {
        if (el->act == act)
        {
            return true;
        }
        el = el->next;
        if (el == plan)
        {
            return false;
        }
    }
    return false;
}

// 0x00640000 (:1659)
bool ai_planner::is_pursuing_condition(const ai_plan_conditions& goal_conditions) const
{
    return cur_goal != nullptr && cur_goal->pursues_condition(goal_conditions);
}

// 0x005F0220 (:1673): the rfg.exe loop adds four nodes on each of its 0x80 passes.
void ai_planning_system_init()
{
    Free_action_list_nodes = nullptr;
    for (action_list& node : Action_list_nodes)
    {
        if (Free_action_list_nodes == nullptr)
        {
            Free_action_list_nodes = &node;
            node.next = &node;
            node.prev = &node;
        }
        else
        {
            node.prev = Free_action_list_nodes->prev;
            node.next = Free_action_list_nodes;
            Free_action_list_nodes->prev->next = &node;
            Free_action_list_nodes->prev = &node;
        }
    }
}

static const npc* as_npc(const object* objp)
{
    return (objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_NPC) ? static_cast<const npc*>(objp) : nullptr;
}

// 0x00640020 (:1692)
bool match_goal_callback(const human* hp, void* data)
{
    const npc* np = as_npc(hp);
    return np != nullptr && np->get_ai()->planner.get_current_goal() == data;
}

// 0x00640050 (:1706)
bool pursuing_condition_callback(object* objp, unsigned int, const void* cb_data)
{
    const npc* np = as_npc(objp);
    if (np == nullptr)
    {
        return false;
    }
    const ai_goal* goal = np->get_ai()->planner.get_current_goal();
    return goal != nullptr && goal->pursues_condition(*static_cast<const ai_plan_conditions*>(cb_data));
}

// 0x00640090 (:1716)
bool pursuing_goal_callback(object* objp, unsigned int, const void* cb_data)
{
    const npc* np = as_npc(objp);
    return np != nullptr && np->get_ai()->planner.get_current_goal() == cb_data;
}

// 0x006400C0 (:1726)
bool pursuing_action_callback(object* objp, unsigned int, const void* cb_data)
{
    const npc* np = as_npc(objp);
    return np != nullptr && np->get_ai()->planner.get_current_action() == cb_data;
}

// 0x00640100 (:1736)
bool pursuing_goal_set_callback(object* objp, unsigned int, const void* cb_data)
{
    const npc* np = as_npc(objp);
    if (np == nullptr)
    {
        return false;
    }
    const base_array<const ai_goal*>& goals = *static_cast<const base_array<const ai_goal*>*>(cb_data);
    const ai_goal* goal = np->get_ai()->planner.get_current_goal();
    int count = 0;
    for (int i = 0; i < goals.size(); i++)
    {
        if (goals[i] == goal)
        {
            count++;
        }
    }
    return count != 0;
}

// 0x00640150 (:1746)
bool pursuing_action_set_callback(object* objp, unsigned int, const void* cb_data)
{
    const npc* np = as_npc(objp);
    if (np == nullptr)
    {
        return false;
    }
    const base_array<const ai_action*>& actions = *static_cast<const base_array<const ai_action*>*>(cb_data);
    const ai_action* act = np->get_ai()->planner.get_current_action();
    int count = 0;
    for (int i = 0; i < actions.size(); i++)
    {
        if (actions[i] == act)
        {
            count++;
        }
    }
    return count != 0;
}
