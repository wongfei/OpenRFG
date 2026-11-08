// Original: volition/rfg/code/ai/combat/acquire_visual_actions.h
// Implementation: volition/rfg/code/ai/combat/acquire_visual_actions.cpp
//
// Acquiring a lost target visually: going to its believed location or to an ally with line of sight, covering or
// guarding its last location. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class npc;

// size 0x34
class go_to_believed_location_action : public ai_action
{
    go_to_believed_location_action(); // 0x005FEBE0 (.h:14)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x005FEEE0 (.h:14)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00616B90 (:250)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00675670 (:282)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B5E50 (:308)
    static ai_action* get_instance();
    virtual ~go_to_believed_location_action(); // vtbl+0x00 0x005DD890

private:
    static go_to_believed_location_action the_instance; // 0x01DA6074
};

// size 0x34
class go_to_ally_with_los_action : public ai_action
{
    go_to_ally_with_los_action(); // 0x005FEF20 (.h:15)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00656D90 (:423)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005FF220 (.h:15)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E22F0 (:473)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006917A0 (:481)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00691D50 (:564)
    static ai_action* get_instance();
    virtual ~go_to_ally_with_los_action(); // vtbl+0x00 0x005DD8A0

private:
    static go_to_ally_with_los_action the_instance; // 0x01DDADAC
};

// size 0x34
class cover_last_location_action : public ai_action
{
    cover_last_location_action(); // 0x005FF250 (.h:16)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006C1EB0 (:662)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005FF480 (.h:16)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E2320 (:743)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006756E0 (:751)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00675750 (:773)
    static ai_action* get_instance();
    virtual ~cover_last_location_action(); // vtbl+0x00 0x005DD8B0

private:
    static cover_last_location_action the_instance; // 0x01DCA56C
};

// size 0x34
class guard_last_location_rule : public ai_production_rule
{
    guard_last_location_rule(); // 0x005FF830 (.h:20)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x005FF8B0 (.h:20)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E2360 (:826)
public:
    static ai_action* get_instance();
    virtual ~guard_last_location_rule(); // vtbl+0x00 0x005DD8D0

private:
    static guard_last_location_rule the_instance; // 0x01D680F4
};

// size 0x34
class guard_last_location_action : public ai_action
{
    guard_last_location_action(); // 0x005FF4B0 (.h:19)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00691F70 (:831)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005FF800 (.h:19)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x00616E00 (:902)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00692270 (:911)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00657100 (:947)
    static ai_action* get_instance();
    virtual ~guard_last_location_action(); // vtbl+0x00 0x005DD8C0

private:
    static guard_last_location_action the_instance; // 0x01DC9E9C
};
