// Original: volition/rfg/code/ai/ai_actions.h
// Implementation: volition/rfg/code/ai/ai_actions.cpp
//
// ai_action, the base of the planner actions: a named singleton with precondition and postcondition sets
// (ai_plan_conditions), costs (with or without per-plan setup data), and enter / do_action / cleanup run for the npc
// while the action is current. ai_production_rule is the action variant that only rewrites conditions (no enter or
// cleanup).
#pragma once

#include "volition/rfg/code/ai/ai_types.h"
#include "volition/rfg/code/ai/plan_conditions.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
struct ai_planner_search_node;
class npc;

// size 0x34
class ai_action
{
public:
    // size 0x1
    struct action_flags
    {
        unsigned char is_production_rule : 1; // 0x00:0
        unsigned char has_enter : 1; // 0x00:1
        unsigned char has_cleanup : 1; // 0x00:2
    };

    ai_action(const char* _action_name, ai_plan_conditions pre, ai_plan_conditions post); // 0x005DB7E0 (:23)
    virtual ~ai_action(); // vtbl+0x00 0x005DB6E0 (.h:38)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x005DB700 (.h:106)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005DB6F0 (.h:105)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005DB720 (.h:109)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005DB910 (:61)
    virtual ai_results do_action(npc*) const = 0; // vtbl+0x14
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005DB930 (:67)
    const char* get_name() const;
    ai_plan_conditions get_pre_conditions() const; // 0x005DB730 (.h:127)
    ai_plan_conditions get_post_conditions() const; // 0x005DB750 (.h:128)
    bool pre_conditions_met(ai_plan_conditions) const;
    bool post_conditions_met(ai_plan_conditions) const;
    bool include_in_plan() const; // 0x005DB770 (.h:135)
    bool has_enter_function() const; // 0x005DB780 (.h:136)
    bool has_cleanup_function() const; // 0x005DB790 (.h:137)
    static const ai_action* const * get_action_pool(); // 0x005DB7A0 (.h:139)
    static int get_action_pool_size(); // 0x005DB7B0 (.h:140)

    // 0x00 vfptr
protected:
    friend class ai_planner; // make_plan_astar_search walks the pool (inferred)
    ai_plan_conditions pre_conditions; // 0x04
    ai_plan_conditions post_conditions; // 0x18
    const char* action_name; // 0x2C
    action_flags flags; // 0x30
    static int num_action_types; // 0x01D67DB0
    static ai_action* action_pool[320]; // 0x01D65220
};

// size 0x34
class ai_production_rule : public ai_action
{
public:
    ai_production_rule(const char* _action_name, ai_plan_conditions pre, ai_plan_conditions post); // 0x005DB860 (:50)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x005DB8E0 (.h:177)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005DB8D0 (.h:174)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005DB8F0 (.h:179)
    virtual ~ai_production_rule(); // vtbl+0x00 0x005DB900

};
