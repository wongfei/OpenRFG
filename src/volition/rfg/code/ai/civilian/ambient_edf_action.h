// Original: volition/rfg/code/ai/civilian/ambient_edf_action.h
// Implementation: volition/rfg/code/ai/civilian/ambient_edf_action.cpp
//
// The actions of the ambient EDF events (fly-by, drive-by, harassing the player, dumping a civilian and being
// dumped). Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class ai_system;
struct ambient_road_info;
class npc;

// size 0x34
class amb_edf_fly_by_action : public ai_production_rule
{
    amb_edf_fly_by_action(); // 0x00611C30 (.h:16)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00611CB0 (.h:16)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E0C20 (:136)
public:
    static ai_action* get_instance();
    virtual ~amb_edf_fly_by_action(); // vtbl+0x00 0x005E0B70

private:
    static amb_edf_fly_by_action the_instance; // 0x01D753A0
};

// size 0x34
class amb_edf_drive_by_action : public ai_action
{
    amb_edf_drive_by_action(); // 0x006114B0 (.h:13)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006116F0 (.h:13)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006906C0 (:154)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006749D0 (:181)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00690760 (:230)
    static ai_action* get_instance();
    virtual ~amb_edf_drive_by_action(); // vtbl+0x00 0x005E0B40

private:
    static amb_edf_drive_by_action the_instance; // 0x01D9F7A0
};

// size 0x34
class amb_edf_harass_player_action : public ai_action
{
    amb_edf_harass_player_action(); // 0x00611CE0 (.h:17)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00611F20 (.h:17)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00690810 (:259)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00674AF0 (:286)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006BBD60 (:339)
    static ai_action* get_instance();
    virtual ~amb_edf_harass_player_action(); // vtbl+0x00 0x005E0B80

private:
    static amb_edf_harass_player_action the_instance; // 0x01DC9C74
};

// size 0x34
class amb_edf_dump_civ_action : public ai_action
{
    amb_edf_dump_civ_action(); // 0x00611730 (.h:14)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00611970 (.h:14)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00611F60 (:413)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00674C20 (:440)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006908B0 (:489)
    static ai_action* get_instance();
    virtual ~amb_edf_dump_civ_action(); // vtbl+0x00 0x005E0B50

private:
    static amb_edf_dump_civ_action the_instance; // 0x01D75A08
};

// size 0x34
class amb_edf_civ_be_dumped_action : public ai_action
{
    amb_edf_civ_be_dumped_action(); // 0x006119B0 (.h:15)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00611BF0 (.h:15)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E0C60 (:518)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00611FE0 (:535)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00612190 (:614)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005E0CB0 (:698)
    static ai_action* get_instance();
    virtual ~amb_edf_civ_be_dumped_action(); // vtbl+0x00 0x005E0B60

private:
    static amb_edf_civ_be_dumped_action the_instance; // 0x01DDB6BC
};

bool assign_destination_info(ai_system* aip, const ambient_road_info& destination_info); // 0x005E0B90 (:50)
