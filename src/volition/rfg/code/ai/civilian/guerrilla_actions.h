// Original: volition/rfg/code/ai/civilian/guerrilla_actions.h
// Implementation: volition/rfg/code/ai/civilian/guerrilla_actions.cpp
//
// Ambient guerrillas looking for trouble on foot, following the player or in a vehicle. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class npc;

// size 0x34
class look_for_trouble_on_foot_action : public ai_action
{
    look_for_trouble_on_foot_action(); // 0x006138F0 (.h:13)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00613BF0 (.h:13)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E16F0 (:60)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00655DF0 (:69)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00690F10 (:89)
    static ai_action* get_instance(); // 0x005E16B0 (.h:13)
    virtual ~look_for_trouble_on_foot_action(); // vtbl+0x00 0x005E16C0

private:
    static look_for_trouble_on_foot_action the_instance; // 0x01D6812C
};

// size 0x34
class look_for_trouble_follow_action : public ai_action
{
    look_for_trouble_follow_action(); // 0x00613C30 (.h:14)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00655FC0 (:100)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00613F30 (.h:14)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E17C0 (:148)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006BBF90 (:155)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B5C20 (:177)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x006BC140 (:205)
    static ai_action* get_instance();
    virtual ~look_for_trouble_follow_action(); // vtbl+0x00 0x005E16D0

private:
    static look_for_trouble_follow_action the_instance; // 0x01D9F9C8
};

// size 0x34
class look_for_trouble_in_vehicle_action : public ai_action
{
    look_for_trouble_in_vehicle_action(); // 0x00613F60 (.h:17)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00614200 (.h:17)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00675210 (:220)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00656130 (:247)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00690F30 (:280)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005E17F0 (:293)
    static ai_action* get_instance();
    virtual ~look_for_trouble_in_vehicle_action(); // vtbl+0x00 0x005E16E0

private:
    static look_for_trouble_in_vehicle_action the_instance; // 0x01D75728
};
