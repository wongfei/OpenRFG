// Original: volition/rfg/code/ai/guard_actions.h
// Implementation: volition/rfg/code/ai/guard_actions.cpp
//
// Guard actions: going to a guard post, guarding it on foot, from a turret or vehicle, scanning and animating at the
// post, roadblocks and examining vehicles. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class npc;

// size 0x34
class go_to_guard_post_action : public ai_action
{
    go_to_guard_post_action(); // 0x006271A0 (.h:13)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006274A0 (.h:13)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005EAD20 (:975)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006A2630 (:982)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006A2B40 (:1081)
    static ai_action* get_instance();
    virtual ~go_to_guard_post_action(); // vtbl+0x00 0x005E6880

private:
    static go_to_guard_post_action the_instance; // 0x01DDB8A0
};

// size 0x34
class roadblock_guard_rule : public ai_production_rule
{
    roadblock_guard_rule(); // 0x00628A80 (.h:37)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00628B00 (.h:37)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005EAD30 (:1205)
public:
    static ai_action* get_instance();
    virtual ~roadblock_guard_rule(); // vtbl+0x00 0x005E6950

private:
    static roadblock_guard_rule the_instance; // 0x01D680C0
};

// size 0x34
class roadblock_vehicle_rule : public ai_production_rule
{
    roadblock_vehicle_rule(); // 0x006289D0 (.h:36)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00628A50 (.h:36)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00638790 (:1213)
public:
    static ai_action* get_instance();
    virtual ~roadblock_vehicle_rule(); // vtbl+0x00 0x005E6940

private:
    static roadblock_vehicle_rule the_instance; // 0x01D686E0
};

// size 0x34
class default_guard_rule : public ai_production_rule
{
    default_guard_rule(); // 0x00628920 (.h:33)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006289A0 (.h:33)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005EAD40 (:1243)
public:
    static ai_action* get_instance();
    virtual ~default_guard_rule(); // vtbl+0x00 0x005E6930

private:
    static default_guard_rule the_instance; // 0x01DC9680
};

// size 0x34
class default_guard_action : public ai_action
{
    default_guard_action(); // 0x006274E0 (.h:16)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0067F2E0 (:1248)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00627830 (.h:16)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005EAD50 (:1301)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006A3310 (:1314)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006A3610 (:1366)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005EADA0 (:1414)
    static ai_action* get_instance();
    virtual ~default_guard_action(); // vtbl+0x00 0x005E6890

private:
    static default_guard_action the_instance; // 0x01D75474
};

// size 0x34
class guard_from_turret_action : public ai_action
{
    guard_from_turret_action(); // 0x00627860 (.h:19)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00660ED0 (:1467)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006278C0 (.h:19)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x00638AA0 (:1502)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00660FC0 (:1509)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00638AD0 (:1517)
    static ai_action* get_instance();
    virtual ~guard_from_turret_action(); // vtbl+0x00 0x005E68A0

private:
    static guard_from_turret_action the_instance; // 0x01DDB528
};

// size 0x34
class guard_at_post_action : public ai_action
{
    guard_at_post_action(); // 0x006278F0 (.h:22)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0067F3E0 (:1543)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00627C40 (.h:22)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x00638C30 (:1584)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006A3710 (:1589)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006A3830 (:1606)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005EADF0 (:1718)
    static ai_action* get_instance();
    virtual ~guard_at_post_action(); // vtbl+0x00 0x005E68B0

private:
    static guard_at_post_action the_instance; // 0x01DC98FC
};

// size 0x34
class guard_at_post_scan_action : public ai_action
{
    guard_at_post_scan_action(); // 0x00627C70 (.h:23)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0067F490 (:1726)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00627FC0 (.h:23)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x00638C90 (:1770)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006A3A60 (:1775)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006A3A80 (:1780)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005EAE30 (:1809)
    static ai_action* get_instance(); // 0x005E68C0 (.h:23)
    virtual ~guard_at_post_scan_action(); // vtbl+0x00 0x005E68D0

private:
    static guard_at_post_scan_action the_instance; // 0x01DC9548
};

// size 0x34
class guard_at_post_animate_action : public ai_action
{
    guard_at_post_animate_action(); // 0x00628000 (.h:24)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0067F540 (:1817)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00628350 (.h:24)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x00638CF0 (:1858)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006A3B10 (:1864)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006A3B30 (:1869)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005EAE70 (:1890)
    static ai_action* get_instance(); // 0x005E68E0 (.h:24)
    virtual ~guard_at_post_animate_action(); // vtbl+0x00 0x005E68F0

private:
    static guard_at_post_animate_action the_instance; // 0x01DDB654
};

// size 0x34
class guard_from_vehicle_action : public ai_action
{
    guard_from_vehicle_action(); // 0x00628380 (.h:27)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006A3B80 (:1904)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00628620 (.h:27)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x006A3CB0 (:1951)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005EAEF0 (:1990)
    static ai_action* get_instance();
    virtual ~guard_from_vehicle_action(); // vtbl+0x00 0x005E6900

private:
    static guard_from_vehicle_action the_instance; // 0x01DDBA6C
};

// size 0x34
class guard_examine_vehicle_action : public ai_action
{
    guard_examine_vehicle_action(); // 0x00628650 (.h:30)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0067F5F0 (:2002)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006288F0 (.h:30)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x006A3DB0 (:2083)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006610C0 (:2101)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006A3E20 (:2121)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00638D50 (:2227)
    static ai_action* get_instance(); // 0x005E6910 (.h:30)
    virtual ~guard_examine_vehicle_action(); // vtbl+0x00 0x005E6920

private:
    static guard_examine_vehicle_action the_instance; // 0x01DCA1E0
};
