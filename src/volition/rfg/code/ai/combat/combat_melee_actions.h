// Original: volition/rfg/code/ai/combat/combat_melee_actions.h
// Implementation: volition/rfg/code/ai/combat/combat_melee_actions.cpp
//
// Melee: the melee move ids (weapon_info's standing and crouching attacks) and the melee actions (closing to melee
// range, circling the target, attacking and blocking, staying in cover, melee against vehicles). Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class npc;

enum human_melee_combat_move_id
{
    INVALID_MELEE_COMBAT_MOVE_ID = -1,
    HUMAN_MELEE_COMBAT_MOVE_ID_FORCE_TO_32_BITS = 0x7FFFFFFF,
};

// size 0x34
class path_to_melee_range_action : public ai_action
{
    path_to_melee_range_action(); // 0x00624360 (.h:12)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006781A0 (:220)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00624660 (.h:12)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E6290 (:255)
    virtual ai_results enter(npc* np, const ai_action* prev_action) const; // vtbl+0x10 0x00678330 (:261)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006BC160 (:280)
    static ai_action* get_instance();
    virtual ~path_to_melee_range_action(); // vtbl+0x00 0x005E6170

private:
    static path_to_melee_range_action the_instance; // 0x01DA03F0
};

// size 0x34
class circle_melee_target_action : public ai_action
{
    circle_melee_target_action(); // 0x00624690 (.h:13)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00624990 (.h:13)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0065A430 (:357)
    virtual ai_results enter(npc* np, const ai_action* prev_action) const; // vtbl+0x10 0x0065A500 (:398)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00699350 (:424)
    static ai_action* get_instance();
    virtual ~circle_melee_target_action(); // vtbl+0x00 0x005E6180

private:
    static circle_melee_target_action the_instance; // 0x01DC94F8
};

// size 0x34
class move_to_melee_range_action : public ai_action
{
    move_to_melee_range_action(); // 0x006249D0 (.h:14)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006783A0 (:498)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00624CD0 (.h:14)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E62A0 (:566)
    virtual ai_results enter(npc* np, const ai_action* prev_action) const; // vtbl+0x10 0x00699750 (:573)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006BC350 (:662)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005E62B0 (:778)
    static ai_action* get_instance();
    virtual ~move_to_melee_range_action(); // vtbl+0x00 0x005E6190

private:
    static move_to_melee_range_action the_instance; // 0x01D688C8
};

// size 0x34
class melee_action : public ai_action
{
    melee_action(); // 0x00624D00 (.h:15)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00699AA0 (:805)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00625000 (.h:15)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E6310 (:869)
    virtual ai_results enter(npc* np, const ai_action* prev_action) const; // vtbl+0x10 0x00699BF0 (:875)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00678650 (:935)
    static ai_action* get_instance(); // 0x005E61A0 (.h:15)
    virtual ~melee_action(); // vtbl+0x00 0x005E61B0

private:
    static melee_action the_instance; // 0x01D753E0
};

// size 0x34
class melee_block_action : public ai_action
{
    melee_block_action(); // 0x00625030 (.h:16)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00699E80 (:991)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00625330 (.h:16)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E6330 (:1037)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00699F90 (:1043)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E6350 (:1065)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005E64B0 (:1081)
    static ai_action* get_instance(); // 0x005E61C0 (.h:16)
    virtual ~melee_block_action(); // vtbl+0x00 0x005E61D0

private:
    static melee_block_action the_instance; // 0x01D687F4
};

// size 0x34
class melee_stay_in_cover_action : public ai_action
{
    melee_stay_in_cover_action(); // 0x00625360 (.h:17)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006C2740 (:1095)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00625410 (.h:17)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E64D0 (:1178)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0065A620 (:1185)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E6500 (:1211)
    static ai_action* get_instance();
    virtual ~melee_stay_in_cover_action(); // vtbl+0x00 0x005E61E0

private:
    static melee_stay_in_cover_action the_instance; // 0x01DDB5E0
};

// size 0x34
class path_to_melee_vehicle_action : public ai_action
{
    path_to_melee_vehicle_action(); // 0x00625770 (.h:20)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00625A60 (.h:20)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E6580 (:1241)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069A020 (:1246)
    static ai_action* get_instance();
    virtual ~path_to_melee_vehicle_action(); // vtbl+0x00 0x005E6200

private:
    static path_to_melee_vehicle_action the_instance; // 0x01D725F4
};

// size 0x34
class melee_vehicle_action : public ai_action
{
    melee_vehicle_action(); // 0x00625440 (.h:19)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0069A170 (:1302)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00625740 (.h:19)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E6590 (:1339)
    virtual ai_results enter(npc* np, const ai_action* prev_action) const; // vtbl+0x10 0x0069A240 (:1345)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00625E60 (:1400)
    static ai_action* get_instance();
    virtual ~melee_vehicle_action(); // vtbl+0x00 0x005E61F0

private:
    static melee_vehicle_action the_instance; // 0x01D68678
};

// size 0x34
class circle_vehicle_target_dead_rule : public ai_production_rule
{
    circle_vehicle_target_dead_rule(); // 0x00625AA0 (.h:22)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00625E30 (.h:22)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00625FD0 (:1455)
public:
    static ai_action* get_instance();
    virtual ~circle_vehicle_target_dead_rule(); // vtbl+0x00 0x005E6210

private:
    static circle_vehicle_target_dead_rule the_instance; // 0x01DDB140
};
