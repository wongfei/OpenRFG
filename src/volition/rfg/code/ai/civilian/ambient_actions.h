// Original: volition/rfg/code/ai/civilian/ambient_actions.h
// Implementation: volition/rfg/code/ai/civilian/ambient_actions.cpp
//
// Ambient actions of civilians and guerrillas: using action nodes, unsticking, reacting to vehicles and the player,
// greeting, gawking, ambient voice lines, dropping ambient props. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class npc;
class object_action_node;

// size 0x34
class goto_action_node_action : public ai_action
{
    goto_action_node_action(); // 0x0060D5F0 (.h:12)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0060D830 (.h:12)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0060F990 (:632)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005E05C0 (:647)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0068F870 (:686)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005E0640 (:834)
    static ai_action* get_instance();
    virtual ~goto_action_node_action(); // vtbl+0x00 0x005E0390

private:
    static goto_action_node_action the_instance; // 0x01DC9F38
};

// size 0x34
class do_action_node_action : public ai_action
{
    do_action_node_action(); // 0x0060D870 (.h:13)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0060F9D0 (:849)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0060DAA0 (.h:13)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E0660 (:892)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006523E0 (:897)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0068FC10 (:931)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x0060FD50 (:1065)
    static ai_action* get_instance();
    virtual ~do_action_node_action(); // vtbl+0x00 0x005E03A0

private:
    static do_action_node_action the_instance; // 0x01D752C4
};

// size 0x34
class enter_action_node_action : public ai_action
{
    enter_action_node_action(); // 0x0060DAD0 (.h:16)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0060FEA0 (:1119)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0060DD10 (.h:16)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E0670 (:1165)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0068FEE0 (:1171)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00610140 (:1232)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00610290 (:1313)
    static ai_action* get_instance();
    virtual ~enter_action_node_action(); // vtbl+0x00 0x005E03B0

private:
    static enter_action_node_action the_instance; // 0x01D9F92C
};

// size 0x34
class exit_action_node_action : public ai_action
{
    exit_action_node_action(); // 0x0060DD40 (.h:17)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0060DF00 (.h:17)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E0680 (:1332)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006102C0 (:1342)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00610350 (:1377)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00610400 (:1411)
    static ai_action* get_instance();
    virtual ~exit_action_node_action(); // vtbl+0x00 0x005E03C0

private:
    static exit_action_node_action the_instance; // 0x01DC9BA8
};

// size 0x34
class unstick_action : public ai_action
{
    unstick_action(); // 0x0060EB80 (.h:26)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0060EE20 (.h:26)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E06A0 (:1452)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006104A0 (:1457)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B5500 (:1497)
    static ai_action* get_instance();
    virtual ~unstick_action(); // vtbl+0x00 0x005E0410

private:
    static unstick_action the_instance; // 0x01DC9A7C
};

// size 0x34
class yell_at_vehicle_action : public ai_action
{
    yell_at_vehicle_action(); // 0x0060EE60 (.h:29)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0060F130 (.h:29)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00652940 (:1682)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006900A0 (:1723)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E06B0 (:1754)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00610560 (:1772)
    static ai_action* get_instance();
    virtual ~yell_at_vehicle_action(); // vtbl+0x00 0x005E0420

private:
    static yell_at_vehicle_action the_instance; // 0x01DDB10C
};

// size 0x34
class mutter_player_action : public ai_action
{
    mutter_player_action(); // 0x0060E560 (.h:22)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0060E830 (.h:22)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006105D0 (:1797)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005E0710 (:1872)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E07F0 (:1916)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005E0830 (:1930)
    static ai_action* get_instance();
    virtual ~mutter_player_action(); // vtbl+0x00 0x005E03F0

private:
    static mutter_player_action the_instance; // 0x01D682D0
};

// size 0x34
class say_ambient_line_action : public ai_action
{
    say_ambient_line_action(); // 0x0060E870 (.h:23)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0060EB40 (.h:23)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006106D0 (:1947)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005E0870 (:2022)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E0910 (:2059)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005E0950 (:2073)
    static ai_action* get_instance();
    virtual ~say_ambient_line_action(); // vtbl+0x00 0x005E0400

private:
    static say_ambient_line_action the_instance; // 0x01D68844
};

// size 0x34
class greet_npc_action : public ai_action
{
    greet_npc_action(); // 0x0060DF40 (.h:20)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0060E210 (.h:20)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006529D0 (:2092)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00690180 (:2150)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006107D0 (:2197)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00610870 (:2222)
    static ai_action* get_instance();
    virtual ~greet_npc_action(); // vtbl+0x00 0x005E03D0

private:
    static greet_npc_action the_instance; // 0x01DDB8F0
};

// size 0x34
class greet_player_action : public ai_action
{
    greet_player_action(); // 0x0060E250 (.h:21)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0060E520 (.h:21)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00690290 (:2247)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00690440 (:2328)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006108E0 (:2380)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00610940 (:2395)
    static ai_action* get_instance();
    virtual ~greet_player_action(); // vtbl+0x00 0x005E03E0

private:
    static greet_player_action the_instance; // 0x01DDACA0
};

// size 0x34
class gawk_action : public ai_action
{
    gawk_action(); // 0x0060F170 (.h:32)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0060F3A0 (.h:32)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E0990 (:2417)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E09B0 (:2426)
    static ai_action* get_instance();
    virtual ~gawk_action(); // vtbl+0x00 0x005E0430

private:
    static gawk_action the_instance; // 0x01DA06A8
};

// size 0x34
class gawk_vehicle_action : public ai_action
{
    gawk_vehicle_action(); // 0x0060F3E0 (.h:33)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0060F620 (.h:33)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E09E0 (:2436)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006109B0 (:2445)
    static ai_action* get_instance();
    virtual ~gawk_vehicle_action(); // vtbl+0x00 0x005E0440

private:
    static gawk_vehicle_action the_instance; // 0x01D751A4
};

// size 0x34
class drop_ambient_prop_action : public ai_action
{
    drop_ambient_prop_action(); // 0x0060F660 (.h:36)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0060F8A0 (.h:36)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E0A00 (:2463)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00652A80 (:2468)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E0A10 (:2503)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00610A20 (:2540)
    static ai_action* get_instance();
    virtual ~drop_ambient_prop_action(); // vtbl+0x00 0x005E0450

private:
    static drop_ambient_prop_action the_instance; // 0x01DA0368
};

void play_ambient_voice_lines(npc* np, object_action_node* anp); // 0x0068F320 (:341)
