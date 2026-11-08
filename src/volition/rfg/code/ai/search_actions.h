// Original: volition/rfg/code/ai/search_actions.h
// Implementation: volition/rfg/code/ai/search_actions.cpp
//
// Search actions and goals: searching buildings and external areas for a lost target, accompanying a searcher, waiting
// for allies, searching from cover and watching an area. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"
#include "volition/rfg/code/ai/goals.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class human;
class npc;

// size 0x1C
class search_goal : public ai_goal
{
    search_goal(); // 0x0061E4E0 (.h:14)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0066AE10 (:449)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x006BA3E0 (:500)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A8C80 (:532)
    static ai_goal* get_instance(); // 0x005E3360 (.h:14)
    virtual ~search_goal(); // vtbl+0x00 0x005E3370

private:
    static search_goal the_instance; // 0x01DCA70C
};

// size 0x1C
class accompany_search_goal : public ai_goal
{
    accompany_search_goal(); // 0x0061E580 (.h:15)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x006A8CE0 (:550)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x006A8F90 (:603)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A9060 (:656)
    static ai_goal* get_instance();
    virtual ~accompany_search_goal(); // vtbl+0x00 0x005E3380

private:
    static accompany_search_goal the_instance; // 0x01DA03D0
};

// size 0x1C
class watch_area_goal : public ai_goal
{
    watch_area_goal(); // 0x0061E620 (.h:16)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0066B100 (:674)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0066B180 (:701)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A90C0 (:740)
    static ai_goal* get_instance(); // 0x005E3390 (.h:16)
    virtual ~watch_area_goal(); // vtbl+0x00 0x005E33A0

private:
    static watch_area_goal the_instance; // 0x01D75950
};

// size 0x34
class search_building_action : public ai_action
{
    search_building_action(); // 0x0061F4E0 (.h:23)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006BA490 (:758)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0061F830 (.h:23)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F1610 (:784)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006412F0 (:793)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006BA560 (:814)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005F1630 (:871)
    static ai_action* get_instance();
    virtual ~search_building_action(); // vtbl+0x00 0x005E3400

private:
    static search_building_action the_instance; // 0x01D722AC
};

// size 0x34
class accompany_search_action : public ai_action
{
    accompany_search_action(); // 0x0061F150 (.h:22)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0061F4A0 (.h:22)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F1660 (:884)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006BF0E0 (:889)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006A9110 (:915)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x006BF2D0 (:955)
    static ai_action* get_instance();
    virtual ~accompany_search_action(); // vtbl+0x00 0x005E33F0

private:
    static accompany_search_action the_instance; // 0x01D67F78
};

// size 0x34
class search_get_external_location : public ai_action
{
    search_get_external_location(); // 0x0061E6C0 (.h:19)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0061EA00 (.h:19)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006A91D0 (:969)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006BA670 (:987)
    static ai_action* get_instance();
    virtual ~search_get_external_location(); // vtbl+0x00 0x005E33B0

private:
    static search_get_external_location the_instance; // 0x01D71EE8
};

// size 0x34
class search_external_area_action : public ai_action
{
    search_external_area_action(); // 0x0061EA40 (.h:20)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0061ED90 (.h:20)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006A9240 (:1051)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006A92A0 (:1069)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006BA8E0 (:1097)
    static ai_action* get_instance();
    virtual ~search_external_area_action(); // vtbl+0x00 0x005E33C0

private:
    static search_external_area_action the_instance; // 0x01DC98C8
};

// size 0x34
class search_wait_for_ally_action : public ai_action
{
    search_wait_for_ally_action(); // 0x0061EDD0 (.h:21)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006A9460 (:1222)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0061F120 (.h:21)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F16C0 (:1250)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0066B250 (:1256)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006BAB40 (:1278)
    static ai_action* get_instance(); // 0x005E33D0 (.h:21)
    virtual ~search_wait_for_ally_action(); // vtbl+0x00 0x005E33E0

private:
    static search_wait_for_ally_action the_instance; // 0x01DC94C4
};

// size 0x34
class search_from_cover_action : public ai_action
{
    search_from_cover_action(); // 0x0061F860 (.h:26)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006A9520 (:1307)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0061F8C0 (.h:26)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F16E0 (:1358)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006BACB0 (:1370)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006BAD80 (:1392)
    static ai_action* get_instance();
    virtual ~search_from_cover_action(); // vtbl+0x00 0x005E3410

private:
    static search_from_cover_action the_instance; // 0x01D7596C
};

// size 0x34
class search_cover_stepout_action : public ai_action
{
    search_cover_stepout_action(); // 0x0061F8F0 (.h:27)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006847A0 (:1435)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0061F9F0 (.h:27)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F1710 (:1485)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006BAEE0 (:1491)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006BB020 (:1526)
    static ai_action* get_instance();
    virtual ~search_cover_stepout_action(); // vtbl+0x00 0x005E3420

private:
    static search_cover_stepout_action the_instance; // 0x01DC9DC8
};

// size 0x34
class watch_area_action : public ai_action
{
    watch_area_action(); // 0x0061FA20 (.h:30)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006414D0 (:1540)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0061FC90 (.h:30)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F1720 (:1565)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006415D0 (:1571)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00641790 (:1590)
    static ai_action* get_instance();
    virtual ~watch_area_action(); // vtbl+0x00 0x005E3430

private:
    static watch_area_action the_instance; // 0x01DDB9D0
};

bool search_goals_callback(const human* hp, void* data); // 0x00641200 (:278)
