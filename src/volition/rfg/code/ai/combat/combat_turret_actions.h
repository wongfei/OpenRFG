// Original: volition/rfg/code/ai/combat/combat_turret_actions.h
// Implementation: volition/rfg/code/ai/combat/combat_turret_actions.cpp
//
// Turret actions: going to, getting on and releasing turrets, attacking and holding position from them, destroying
// cover. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class human;
class npc;
class turret;
class vector;

enum destructive_fire_test_results
{
    DFTR_DANGER = 0,
    DFTR_HIT_OBSTRUCTION = 1,
    DFTR_HIT_NOTHING = 2,
    DFTR_HIT_BAD_TARGET = 3,
    NUM_DESTRUCTIVE_FIRE_TEST_RESULTS = 4,
};

// size 0x34
class get_on_turret_action : public ai_action
{
    get_on_turret_action(); // 0x006223C0 (.h:14)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006226C0 (.h:14)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E6960 (:330)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00629050 (:335)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00678BA0 (:378)
    static ai_action* get_instance();
    virtual ~get_on_turret_action(); // vtbl+0x00 0x005E4000

private:
    static get_on_turret_action the_instance; // 0x01D721D0
};

// size 0x34
class release_turret_action : public ai_action
{
    release_turret_action(); // 0x00622700 (.h:15)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00622750 (.h:15)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E6970 (:405)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E6980 (:410)
    static ai_action* get_instance(); // 0x005E4010 (.h:15)
    virtual ~release_turret_action(); // vtbl+0x00 0x005E4020

private:
    static release_turret_action the_instance; // 0x01DDB190
};

// size 0x34
class go_to_turret_action : public ai_action
{
    go_to_turret_action(); // 0x00622140 (.h:13)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00622380 (.h:13)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E69A0 (:419)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069AD30 (:424)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069AEF0 (:473)
    static ai_action* get_instance();
    virtual ~go_to_turret_action(); // vtbl+0x00 0x005E3FF0

private:
    static go_to_turret_action the_instance; // 0x01D686AC
};

// size 0x34
class attack_from_turret_action : public ai_action
{
    attack_from_turret_action(); // 0x00622790 (.h:18)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00629140 (:580)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00622840 (.h:18)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x006292B0 (:630)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069B210 (:638)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069B2D0 (:673)
    static ai_action* get_instance();
    virtual ~attack_from_turret_action(); // vtbl+0x00 0x005E4030

private:
    static attack_from_turret_action the_instance; // 0x01D67E10
};

// size 0x34
class hold_position_at_turret_action : public ai_action
{
    hold_position_at_turret_action(); // 0x006228F0 (.h:22)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006292E0 (:706)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00622950 (.h:22)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x006293C0 (:742)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069B360 (:750)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00678C50 (:768)
    static ai_action* get_instance();
    virtual ~hold_position_at_turret_action(); // vtbl+0x00 0x005E4050

private:
    static hold_position_at_turret_action the_instance; // 0x01DCA084
};

// size 0x34
class turret_destroy_cover_action : public ai_action
{
    turret_destroy_cover_action(); // 0x00622870 (.h:21)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006293F0 (:804)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006228C0 (.h:21)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x00629530 (:843)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069B3D0 (:851)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00678D10 (:867)
    static ai_action* get_instance();
    virtual ~turret_destroy_cover_action(); // vtbl+0x00 0x005E4040

private:
    static turret_destroy_cover_action the_instance; // 0x01DDAED8
};

turret* get_best_turret(const npc* np, const vector& target_pos); // 0x00628D40 (:189)
bool attack_from_turret_action_common(npc* np, turret* tp, human* target); // 0x0069AA10 (:252)
bool defensive_combat_turret_action_common(npc* np, turret* tp, human* target); // 0x0069AB90 (:289)
