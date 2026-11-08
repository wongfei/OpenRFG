// Original: volition/rfg/code/ai/patrol_actions.h
// Implementation: volition/rfg/code/ai/patrol_actions.cpp
//
// Patrol actions: following patrol paths on foot or in vehicles, checking in, leading convoys, and driving or riding on
// rails. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class npc;

// size 0x34
class patrol_check_in_action : public ai_action
{
    patrol_check_in_action(); // 0x0063CB20 (.h:13)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0063CE20 (.h:13)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005ECD10 (:180)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00664090 (:221)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005ECDC0 (:241)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x0063D460 (:255)
    static ai_action* get_instance();
    virtual ~patrol_check_in_action(); // vtbl+0x00 0x005ECB20

private:
    static patrol_check_in_action the_instance; // 0x01DDB804
};

// size 0x34
class default_patrol_action : public ai_action
{
    default_patrol_action(); // 0x0063C7E0 (.h:12)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0063CAE0 (.h:12)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005ECE10 (:272)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00664130 (:288)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006A51D0 (:331)
    static ai_action* get_instance(); // 0x005ECB00 (.h:12)
    virtual ~default_patrol_action(); // vtbl+0x00 0x005ECB10

private:
    static default_patrol_action the_instance; // 0x01D75218
};

// size 0x34
class lead_convoy_patrol_action : public ai_action
{
    lead_convoy_patrol_action(); // 0x0063CEF0 (.h:16)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006A53A0 (:411)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0063D0A0 (.h:16)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005ECE50 (:447)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006803E0 (:455)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B7480 (:506)
    static ai_action* get_instance();
    virtual ~lead_convoy_patrol_action(); // vtbl+0x00 0x005ECB40

private:
    static lead_convoy_patrol_action the_instance; // 0x01DC9CDC
};

// size 0x34
class patrol_in_vehicle_action : public ai_action
{
    patrol_in_vehicle_action(); // 0x0063CE60 (.h:14)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006A54C0 (:585)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0063CEC0 (.h:14)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005ECE80 (:614)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00664330 (:621)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B7590 (:717)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005ECEC0 (:998)
    static ai_action* get_instance();
    virtual ~patrol_in_vehicle_action(); // vtbl+0x00 0x005ECB30

private:
    static patrol_in_vehicle_action the_instance; // 0x01DA0458
};

// size 0x34
class drive_on_rail_enter_rule : public ai_production_rule
{
    drive_on_rail_enter_rule(); // 0x0063D0D0 (.h:18)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0063D530 (:1021)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0063D140 (.h:18)
public:
    static ai_action* get_instance();
    virtual ~drive_on_rail_enter_rule(); // vtbl+0x00 0x005ECB50

private:
    static drive_on_rail_enter_rule the_instance; // 0x01D759A0
};

// size 0x34
class drive_on_rail_wait_action : public ai_action
{
    drive_on_rail_wait_action(); // 0x0063D170 (.h:19)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006A55D0 (:1044)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0063D1D0 (.h:19)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005ECF10 (:1091)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0063D5F0 (:1099)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0063D650 (:1124)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005ECF40 (:1146)
    static ai_action* get_instance();
    virtual ~drive_on_rail_wait_action(); // vtbl+0x00 0x005ECB60

private:
    static drive_on_rail_wait_action the_instance; // 0x01DDB86C
};

// size 0x34
class drive_on_rail_rule : public ai_production_rule
{
    drive_on_rail_rule(); // 0x0063D200 (.h:20)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0063D6A0 (:1162)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0063D2E0 (.h:20)
public:
    static ai_action* get_instance();
    virtual ~drive_on_rail_rule(); // vtbl+0x00 0x005ECB70

private:
    static drive_on_rail_rule the_instance; // 0x01D9F6B0
};

// size 0x34
class passenger_rail_rule : public ai_production_rule
{
    passenger_rail_rule(); // 0x0063D3B0 (.h:22)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0063D430 (.h:22)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005ECF60 (:1203)
public:
    static ai_action* get_instance();
    virtual ~passenger_rail_rule(); // vtbl+0x00 0x005ECBA0

private:
    static passenger_rail_rule the_instance; // 0x01D74E64
};

// size 0x34
class shoot_rail_rule : public ai_production_rule
{
    shoot_rail_rule(); // 0x0063D310 (.h:21)
public:
    static ai_action* get_instance(); // 0x005ECB80 (.h:21)
    virtual ~shoot_rail_rule(); // vtbl+0x00 0x005ECB90

private:
    static shoot_rail_rule the_instance; // 0x01DC9998
};
