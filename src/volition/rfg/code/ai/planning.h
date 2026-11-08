// Original: volition/rfg/code/ai/planning.h
// Implementation: volition/rfg/code/ai/planning.cpp
//
// ai_planner, the goal and action planner of an npc: it evaluates goal fitness, searches the action graph backwards
// from the goal's conditions (search nodes on a heap) into an action list, and tracks the current plan, action and
// replanning state.
#pragma once

#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

class ai_action;
class ai_goal;
class human;
class npc;
class object;
class vehicle;

// size 0xC
struct action_list
{
    const ai_action* act; // 0x00
    action_list* next; // 0x04
    action_list* prev; // 0x08
};

// size 0x88
struct ai_planner_search_node
{
    // size 0x1
    struct planner_search_node_flags
    {
        unsigned char already_explored : 1; // 0x00:0
        unsigned char include_action_in_plan : 1; // 0x00:1
        unsigned char non_monotonic : 1; // 0x00:2
    };

    ai_planner_search_node(); // 0x005EFD90

    ai_plan_conditions desired_conditions; // 0x00
    ai_plan_conditions satisfied_conditions; // 0x14
    const ai_action* next_action; // 0x28
    ai_planner_search_node* next_node; // 0x2C
    int incurred_cost; // 0x30
    int heuristic_cost; // 0x34
    int estimated_total_cost; // 0x38
    int search_depth; // 0x3C
    ai_action_setup_data action_data; // 0x40
    planner_search_node_flags flags; // 0x80
};

// size 0xB8
class ai_planner
{
    // human_combat.cpp and human.cpp read the plan (friends are not recorded in rfg.pdb).
    friend bool human_combat_can_play_large_flinch_or_stumble(const human* hp);
    friend bool human_can_currently_be_pushed(const human* hp);
    friend void vehicle_navmesh_common_follow_path(vehicle* vp); // reads the current action
public:
    enum planner_execution_states
    {
        PLANNER_EX_NONE = 0,
        PLANNER_EX_ENTER_ACTION = 1,
        PLANNER_EX_DO_ACTION = 2,
        PLANNER_EX_CLEANUP_ACTION = 3,
        NUM_PLANNER_EX_STATES = 4,
    };

    enum goal_failure_states
    {
        GOAL_FAILURE_NONE = 0,
        GOAL_FAILURE_CONDITIONS = 1,
        GOAL_FAILURE_INITIALIZATION = 2,
        GOAL_FAILURE_PLAN = 3,
        GOAL_FAILURE_EXECUTION = 4,
        NUM_GOAL_FAILURE_STATES = 5,
    };

    // size 0x8
    struct action_avoidance_record
    {
        action_avoidance_record(); // 0x005F01E0

        const ai_action* action; // 0x00
        timestamp timer; // 0x04
    };

    // size 0xC
    struct goal_rank
    {
        int fitness; // 0x00
        ai_goal* goal; // 0x04
        goal_failure_states failure_state; // 0x08
    };

    // size 0x1
    struct planner_flags
    {
        bool action_locked : 1; // 0x00:0
        bool should_replan : 1; // 0x00:1
        bool should_rerank_goals : 1; // 0x00:2
        bool executed_this_frame : 1; // 0x00:3
        bool allow_prerequest : 1; // 0x00:4
    };

    ai_planner(); // 0x0063FE00 (:1211)
    void init(npc* np_); // 0x005EFFC0 (:1264)
    void clear(); // 0x005EFEE0 (:1223)
    void stun(); // 0x005F0040 (:1310)
    void do_frame(); // 0x006BA370 (:1459)
    void do_frame_pre(); // 0x006A7680 (:1385)
    void request_replan(); // 0x005F01B0 (:1557)
    bool skip_action(const ai_action*);
    void lock_action(bool lock); // 0x005F01C0 (:1567)
    void avoid_action(const ai_action* act, int time); // 0x0063FE40 (:1582)
    bool is_avoiding_action(const ai_action* act) const; // 0x0063FF90 (:1614)
    const ai_action* get_current_action() const; // 0x005DC8D0 (.h:79)
    const ai_goal* get_current_goal() const; // 0x005DC8E0 (.h:80)
    const ai_goal* get_last_goal() const; // 0x005DC8F0 (.h:81)
    bool plan_continues_after_current_action() const; // 0x005DC900 (.h:83)
    bool plan_contains(const ai_action* act) const; // 0x005F01F0 (:1635)
    bool is_pursuing_condition(const ai_plan_conditions& goal_conditions) const; // 0x00640000 (:1659)
    int get_time_in_current_action() const; // 0x005DC920 (.h:87)
    int get_time_in_last_action() const;
    int get_time_in_current_goal() const; // 0x005DC940 (.h:89)
private:
    bool require_high_priority_replan() const; // 0x005EFD20 (:383)
    bool can_request_replan() const; // 0x005EFD60 (:395)
    void rank_goals(ai_plan_conditions working_conditions); // 0x0063F970 (:412)
    bool make_plan_astar_search(ai_plan_conditions working_conditions, const ai_goal* new_goal, const ai_goal_setup_data& goal_setup_data); // 0x00669740 (:493)
    bool do_replan(); // 0x006BA160 (:787)
    bool do_action(); // 0x0063FB40 (:911)
    void cleanup_current_plan(bool failed_during_cleanup, const ai_action* next_plan_first_action); // 0x005EFE10 (:1119)
public:
    ~ai_planner(); // 0x005F9C60

private:
    npc* np; // 0x00
    circ_array<action_avoidance_record,8> action_avoidance_records; // 0x04
    planner_execution_states planner_execution_state; // 0x4C
    planner_flags flags; // 0x50
    goal_rank goal_rankings[5]; // 0x54
    const ai_goal* cur_goal; // 0x90
    const ai_goal* last_goal; // 0x94
    action_list* plan; // 0x98
    const ai_action* last_action; // 0x9C
    const ai_action* last_action_executed; // 0xA0
    timestamp cur_action_timer; // 0xA4
    timestamp cur_goal_timer; // 0xA8
    int cur_action_frame; // 0xAC
    int last_action_time; // 0xB0
    timestamp replan_timer; // 0xB4
};

void append_to_plan(action_list*& plan, const ai_action* act); // 0x005EFBB0 (:303)
void remove_from_plan(action_list*& plan, action_list* plan_el); // 0x005EFC50 (:322)
void free_plan(action_list*& plan); // 0x005EFCD0 (:332)
void initialize_search_node(ai_planner_search_node& node, ai_plan_conditions desired_conditions, const ai_action* next_action, ai_planner_search_node* next_node, int incurred_cost, int search_depth, const ai_action_setup_data& action_data, bool include_in_plan, bool non_monotonic); // 0x0063F710 (:339)
void update_search_node(ai_planner_search_node& node, const ai_action* next_action, ai_planner_search_node* next_node, int incurred_cost, int search_depth, const ai_action_setup_data& action_data, bool include_in_plan, bool non_monotonic); // 0x0063F860 (:360)
void ai_planning_system_init(); // 0x005F0220 (:1673)
bool match_goal_callback(const human* hp, void* data); // 0x00640020 (:1692)
bool pursuing_condition_callback(object* objp, unsigned int, const void* cb_data); // 0x00640050 (:1706)
bool pursuing_goal_callback(object* objp, unsigned int, const void* cb_data); // 0x00640090 (:1716)
bool pursuing_action_callback(object* objp, unsigned int, const void* cb_data); // 0x006400C0 (:1726)
bool pursuing_goal_set_callback(object* objp, unsigned int, const void* cb_data); // 0x00640100 (:1736)
bool pursuing_action_set_callback(object* objp, unsigned int, const void* cb_data); // 0x00640150 (:1746)

// 0x005DC8D0 (.h:79): the first action of the plan.
inline const ai_action* ai_planner::get_current_action() const
{
    return plan != nullptr ? plan->act : nullptr;
}
