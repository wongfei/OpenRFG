// Original: volition/rfg/code/ai/ai_script_actions.h
// Implementation: volition/rfg/code/ai/ai_script_actions.cpp
//
// Actions of scripted NPC orders: move to a location on foot and drive to a location (move_to_location_goal,
// drive_to_location_goal). Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class npc;

// size 0x34
class move_to_location_action : public ai_action
{
    move_to_location_action(); // 0x005FE840 (.h:56)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x005FEA70 (.h:56)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005DD790 (:31)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006B4F60 (:36)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B4FD0 (:56)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005DD7B0 (:116)
    static ai_action* get_instance();
    virtual ~move_to_location_action(); // vtbl+0x00 0x005DD770

private:
    static move_to_location_action the_instance; // 0x01DC96B4
};

// size 0x34
class drive_to_location_action : public ai_action
{
    drive_to_location_action(); // 0x005FEAB0 (.h:57)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x005FEB10 (.h:57)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005DD840 (:144)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005FEB50 (:155)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B51B0 (:174)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005DD870 (:246)
    static ai_action* get_instance();
    virtual ~drive_to_location_action(); // vtbl+0x00 0x005DD780

private:
    static drive_to_location_action the_instance; // 0x01DCA380
};
