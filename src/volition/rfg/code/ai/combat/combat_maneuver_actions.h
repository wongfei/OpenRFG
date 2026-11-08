// Original: volition/rfg/code/ai/combat/combat_maneuver_actions.h
// Implementation: volition/rfg/code/ai/combat/combat_maneuver_actions.cpp
//
// Maneuver actions: retreating to a defensive perimeter, falling back (in buildings too), advancing (partially or
// using cover). Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class cover_node;
class human;
class npc;
class vector;
class weapon;

// size 0x34
class retreat_defensive_perimeter_building_action : public ai_action
{
    retreat_defensive_perimeter_building_action(); // 0x00621A40 (.h:19)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00659C10 (:689)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00621D30 (.h:19)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E5F50 (:749)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00697360 (:755)
    static ai_action* get_instance();
    virtual ~retreat_defensive_perimeter_building_action(); // vtbl+0x00 0x005E3FB0

private:
    static retreat_defensive_perimeter_building_action the_instance; // 0x01DDB0BC
};

// size 0x34
class retreat_defensive_perimeter_default_action : public ai_action
{
    retreat_defensive_perimeter_default_action(); // 0x00621D60 (.h:20)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00622060 (.h:20)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006241F0 (:781)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00697430 (:795)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00697470 (:811)
    static ai_action* get_instance(); // 0x005E3FC0 (.h:20)
    virtual ~retreat_defensive_perimeter_default_action(); // vtbl+0x00 0x005E3FD0

private:
    static retreat_defensive_perimeter_default_action the_instance; // 0x01DC9FA0
};

// size 0x34
class follow_to_defensive_perimeter_rule : public ai_production_rule
{
    follow_to_defensive_perimeter_rule(); // 0x006220A0 (.h:22)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00622110 (.h:22)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0065A200 (:877)
public:
    static ai_action* get_instance();
    virtual ~follow_to_defensive_perimeter_rule(); // vtbl+0x00 0x005E3FE0

private:
    static follow_to_defensive_perimeter_rule the_instance; // 0x01D754A8
};

// size 0x34
class fall_back_action : public ai_action
{
    fall_back_action(); // 0x00620B20 (.h:12)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00620DB0 (.h:12)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00677B40 (:896)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00697990 (:909)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00697AA0 (:955)
    static ai_action* get_instance();
    virtual ~fall_back_action(); // vtbl+0x00 0x005E3F40

private:
    static fall_back_action the_instance; // 0x01DA0674
};

// size 0x34
class advance_action : public ai_action
{
    advance_action(); // 0x00620DF0 (.h:13)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006210F0 (.h:13)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00624250 (:1094)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00698000 (:1156)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00698090 (:1199)
    static ai_action* get_instance(); // 0x005E3F50 (.h:13)
    virtual ~advance_action(); // vtbl+0x00 0x005E3F60

private:
    static advance_action the_instance; // 0x01D75440
};

// size 0x34
class partial_advance_action : public ai_action
{
    partial_advance_action(); // 0x00621130 (.h:14)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00698CE0 (:1489)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00621430 (.h:14)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E60D0 (:1545)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00698E00 (:1551)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00698E90 (:1578)
    static ai_action* get_instance(); // 0x005E3F70 (.h:14)
    virtual ~partial_advance_action(); // vtbl+0x00 0x005E3F80

private:
    static partial_advance_action the_instance; // 0x01D72458
};

// size 0x34
class advance_using_cover_action : public ai_action
{
    advance_using_cover_action(); // 0x00621460 (.h:15)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00677B90 (:1690)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006217A0 (.h:15)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E6100 (:1815)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E6140 (:1823)
    static ai_action* get_instance();
    virtual ~advance_using_cover_action(); // vtbl+0x00 0x005E3F90

private:
    static advance_using_cover_action the_instance; // 0x01D9F8F8
};

// size 0x34
class fall_back_in_building_action : public ai_action
{
    fall_back_in_building_action(); // 0x006217D0 (.h:17)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00621A00 (.h:17)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00677FB0 (:1833)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00699070 (:1848)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00699110 (:1882)
    static ai_action* get_instance();
    virtual ~fall_back_in_building_action(); // vtbl+0x00 0x005E3FA0

private:
    static fall_back_in_building_action the_instance; // 0x01DCA330
};

cover_node* get_cover_node_for_advance(bool& defense_only, const npc* np, const weapon* wp, const human* target, const vector& target_dir, float target_dist); // 0x006597A0 (:548)
