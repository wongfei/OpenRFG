// Original: volition/rfg/code/ai/misc_actions.h
// Implementation: volition/rfg/code/ai/misc_actions.cpp
//
// Miscellaneous actions: idling (on foot, in vehicles, at SAM sites), getting up, going prone, entering and leaving
// buildings, moving out of the way, saluting. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class npc;

// size 0x34
class idle_action : public ai_action
{
    idle_action(); // 0x00614240 (.h:12)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00614520 (.h:12)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005EC6D0 (:458)
    virtual ai_results enter(npc* np, const ai_action* next_action) const; // vtbl+0x10 0x00663110 (:463)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005EC710 (:499)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005EC6E0 (:489)
    static ai_action* get_instance();
    virtual ~idle_action(); // vtbl+0x00 0x005E1830

private:
    static idle_action the_instance; // 0x01D725A4
};

// size 0x34
class idle_vehicle_action : public ai_action
{
    idle_vehicle_action(); // 0x00614560 (.h:13)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00614620 (.h:13)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0067FFC0 (:517)
    virtual ai_results enter(npc* np, const ai_action* next_action) const; // vtbl+0x10 0x005EC760 (:546)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0063C5D0 (:563)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005EC790 (:558)
    static ai_action* get_instance();
    virtual ~idle_vehicle_action(); // vtbl+0x00 0x005E1840

private:
    static idle_vehicle_action the_instance; // 0x01D9F670
};

// size 0x34
class idle_sam_action : public ai_action
{
    idle_sam_action(); // 0x00614660 (.h:14)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00614940 (.h:14)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005EC7A0 (:588)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00663250 (:598)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005EC7D0 (:624)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005EC860 (:637)
    static ai_action* get_instance();
    virtual ~idle_sam_action(); // vtbl+0x00 0x005E1850

private:
    static idle_sam_action the_instance; // 0x01D72418
};

// size 0x34
class get_up_action : public ai_action
{
    get_up_action(); // 0x00614980 (.h:15)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00614BB0 (.h:15)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005EC8D0 (:654)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006633D0 (:659)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005EC8F0 (:690)
    static ai_action* get_instance();
    virtual ~get_up_action(); // vtbl+0x00 0x005E1860

private:
    static get_up_action the_instance; // 0x01DCA2E0
};

// size 0x34
class scramble_to_feet_action : public ai_action
{
    scramble_to_feet_action(); // 0x00614E60 (.h:17)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00615090 (.h:17)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0063C680 (:709)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00663990 (:725)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005EC940 (:756)
    static ai_action* get_instance(); // 0x005E1880 (.h:17)
    virtual ~scramble_to_feet_action(); // vtbl+0x00 0x005E1890

private:
    static scramble_to_feet_action the_instance; // 0x01D756F4
};

// size 0x34
class go_prone_action : public ai_action
{
    go_prone_action(); // 0x00614BF0 (.h:16)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00614E20 (.h:16)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005EC9D0 (:782)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00663F70 (:787)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005EC9E0 (:807)
    static ai_action* get_instance();
    virtual ~go_prone_action(); // vtbl+0x00 0x005E1870

private:
    static go_prone_action the_instance; // 0x01D68198
};

// size 0x34
class enter_building_action : public ai_action
{
    enter_building_action(); // 0x006150D0 (.h:22)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00663FF0 (:834)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00615370 (.h:22)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005ECA60 (:870)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006A4DD0 (:879)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00680030 (:898)
    static ai_action* get_instance();
    virtual ~enter_building_action(); // vtbl+0x00 0x005E18A0

private:
    static enter_building_action the_instance; // 0x01D67FB8
};

// size 0x34
class leave_building_action : public ai_action
{
    leave_building_action(); // 0x006153A0 (.h:23)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00615640 (.h:23)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00680140 (:960)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00680220 (:1010)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006A4E70 (:1072)
    static ai_action* get_instance();
    virtual ~leave_building_action(); // vtbl+0x00 0x005E18B0

private:
    static leave_building_action the_instance; // 0x01D9F808
};

// size 0x34
class move_out_of_way_action : public ai_action
{
    move_out_of_way_action(); // 0x00615680 (.h:25)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00615920 (.h:25)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005ECA90 (:1105)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00680370 (:1110)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006A4F90 (:1131)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005ECAA0 (:1191)
    static ai_action* get_instance();
    virtual ~move_out_of_way_action(); // vtbl+0x00 0x005E18C0

private:
    static move_out_of_way_action the_instance; // 0x01DCA3B8
};

// size 0x34
class salute_action : public ai_action
{
    salute_action(); // 0x00615960 (.h:27)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00615C00 (.h:27)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005ECAC0 (:1203)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0063C750 (:1208)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005ECAD0 (:1220)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x0063C7B0 (:1228)
    static ai_action* get_instance();
    virtual ~salute_action(); // vtbl+0x00 0x005E18D0

private:
    static salute_action the_instance; // 0x01DCA6D8
};
