// Original: volition/rfg/code/ai/escape_actions.h
// Implementation: volition/rfg/code/ai/escape_actions.cpp
//
// Escape actions: escaping the player on foot or by vehicle along roads (bureaucrats, VIPs). Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class npc;
class vehicle;

// size 0x34
class escape_on_road_player_action : public ai_action
{
    escape_on_road_player_action(); // 0x0062EF40 (.h:12)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0069F490 (:617)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0062F1E0 (.h:12)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E9120 (:656)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0065D950 (:664)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069F510 (:705)
    static ai_action* get_instance();
    virtual ~escape_on_road_player_action(); // vtbl+0x00 0x005E90F0

private:
    static escape_on_road_player_action the_instance; // 0x01D9F858
};

// size 0x34
class default_escape_player_action : public ai_action
{
    default_escape_player_action(); // 0x0062F210 (.h:13)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0069F5F0 (:748)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0062F4B0 (.h:13)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E9160 (:783)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0065DA40 (:798)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B6A90 (:839)
    static ai_action* get_instance();
    virtual ~default_escape_player_action(); // vtbl+0x00 0x005E9100

private:
    static default_escape_player_action the_instance; // 0x01DA02CC
};

// size 0x34
class bureaucrat_escape_rule : public ai_production_rule
{
    bureaucrat_escape_rule(); // 0x0062F4E0 (.h:18)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0062F550 (.h:18)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E9190 (:969)
public:
    static ai_action* get_instance();
    virtual ~bureaucrat_escape_rule(); // vtbl+0x00 0x005E9110

private:
    static bureaucrat_escape_rule the_instance; // 0x01D68748
};

vehicle* find_escape_vehicle(const npc* np); // 0x0069F360 (:157)
