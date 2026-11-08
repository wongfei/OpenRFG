// Original: volition/rfg/code/ai/reinforcement_actions.h
// Implementation: volition/rfg/code/ai/reinforcement_actions.cpp
//
// Reinforcement actions: reinforcements arriving on foot or by vehicle and following to the activity area. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class ai_system;
class npc;
class vector;

// size 0x34
class default_reinforcement_action : public ai_action
{
    default_reinforcement_action(); // 0x006312F0 (.h:21)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00631640 (.h:21)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F1410 (:119)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006A86E0 (:137)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006A8860 (:164)
    static ai_action* get_instance();
    virtual ~default_reinforcement_action(); // vtbl+0x00 0x005E92E0

private:
    static default_reinforcement_action the_instance; // 0x01D726DC
};

// size 0x34
class reinforcement_vehicle_rule : public ai_production_rule
{
    reinforcement_vehicle_rule(); // 0x00631680 (.h:22)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00631700 (.h:22)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006A88D0 (:252)
public:
    static ai_action* get_instance();
    virtual ~reinforcement_vehicle_rule(); // vtbl+0x00 0x005E92F0

private:
    static reinforcement_vehicle_rule the_instance; // 0x01D74E98
};

// size 0x34
class follow_to_activity_area : public ai_production_rule
{
    follow_to_activity_area(); // 0x00631730 (.h:25)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006317B0 (.h:25)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0066A5C0 (:299)
public:
    static ai_action* get_instance();
    virtual ~follow_to_activity_area(); // vtbl+0x00 0x005E9300

private:
    static follow_to_activity_area the_instance; // 0x01DDB730
};

bool find_reinforcement_destination(vector& dest_pos, const npc* np, const ai_system* aip, const vector& orders_pos); // 0x0066A370 (:60)
