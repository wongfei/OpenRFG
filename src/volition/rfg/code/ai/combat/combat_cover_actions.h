// Original: volition/rfg/code/ai/combat/combat_cover_actions.h
// Implementation: volition/rfg/code/ai/combat/combat_cover_actions.cpp
//
// Cover actions: attacking and reloading from (defensive) cover, dodging in cover, going to, leaving and transitioning
// into cover, switching stance, stepping in and out. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class npc;

// size 0x34
class attack_cover_action : public ai_action
{
    attack_cover_action(); // 0x00617D50 (.h:25)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006C23F0 (:558)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00617F80 (.h:25)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E3BA0 (:572)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006948A0 (:577)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00694910 (:598)
    static ai_action* get_instance();
    virtual ~attack_cover_action(); // vtbl+0x00 0x005E2B70

private:
    static attack_cover_action the_instance; // 0x01D68268
};

// size 0x34
class attack_defensive_cover_action : public ai_action
{
    attack_defensive_cover_action(); // 0x00617FB0 (.h:26)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00676B20 (:647)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006180B0 (.h:26)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E3BC0 (:653)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00694A10 (:666)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00694AA0 (:696)
    static ai_action* get_instance(); // 0x005E2B80 (.h:26)
    virtual ~attack_defensive_cover_action(); // vtbl+0x00 0x005E2B90

private:
    static attack_defensive_cover_action the_instance; // 0x01DA0298
};

// size 0x34
class reload_cover_action : public ai_action
{
    reload_cover_action(); // 0x00617C30 (.h:24)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006C2460 (:745)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00617D20 (.h:24)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E3BF0 (:811)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0061FD60 (:820)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E3C20 (:837)
    static ai_action* get_instance();
    virtual ~reload_cover_action(); // vtbl+0x00 0x005E2B60

private:
    static reload_cover_action the_instance; // 0x01D7512C
};

// size 0x34
class reload_defensive_cover_action : public ai_action
{
    reload_defensive_cover_action(); // 0x006180E0 (.h:27)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00676B50 (:849)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006181D0 (.h:27)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E3C40 (:855)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00694B70 (:868)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E3C70 (:900)
    static ai_action* get_instance();
    virtual ~reload_defensive_cover_action(); // vtbl+0x00 0x005E2BA0

private:
    static reload_defensive_cover_action the_instance; // 0x01DC9588
};

// size 0x34
class dodge_cover_action : public ai_action
{
    dodge_cover_action(); // 0x00617B50 (.h:23)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006C2680 (:912)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00617C00 (.h:23)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E3C90 (:931)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E3CB0 (:936)
    static ai_action* get_instance();
    virtual ~dodge_cover_action(); // vtbl+0x00 0x005E2B50

private:
    static dodge_cover_action the_instance; // 0x01D68410
};

// size 0x34
class go_to_cover_action : public ai_action
{
    go_to_cover_action(); // 0x00617310 (.h:13)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006579D0 (:959)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006175A0 (.h:13)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E3D20 (:1004)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00676B80 (:1010)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00694C50 (:1118)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x0061FDE0 (:1200)
    static ai_action* get_instance(); // 0x005E2AE0 (.h:13)
    virtual ~go_to_cover_action(); // vtbl+0x00 0x005E2AF0

private:
    static go_to_cover_action the_instance; // 0x01DDB7D0
};

// size 0x34
class leave_cover_action : public ai_action
{
    leave_cover_action(); // 0x006175D0 (.h:14)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00617620 (.h:14)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E3D30 (:1214)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00676E00 (:1219)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E3D40 (:1281)
    static ai_action* get_instance();
    virtual ~leave_cover_action(); // vtbl+0x00 0x005E2B00

private:
    static leave_cover_action the_instance; // 0x01D68530
};

// size 0x34
class transition_to_cover_action : public ai_action
{
    transition_to_cover_action(); // 0x00617660 (.h:17)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0061FE00 (:1299)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00617940 (.h:17)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E3D70 (:1310)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00657C50 (:1316)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E3D80 (:1411)
    static ai_action* get_instance();
    virtual ~transition_to_cover_action(); // vtbl+0x00 0x005E2B10

private:
    static transition_to_cover_action the_instance; // 0x01DCA5A0
};

// size 0x34
class cover_switch_stance_action : public ai_action
{
    cover_switch_stance_action(); // 0x00617AB0 (.h:20)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00617B10 (.h:20)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E3E00 (:1455)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00657F80 (:1460)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E3E10 (:1497)
    static ai_action* get_instance();
    virtual ~cover_switch_stance_action(); // vtbl+0x00 0x005E2B40

private:
    static cover_switch_stance_action the_instance; // 0x01DA039C
};

// size 0x34
class cover_step_in_action : public ai_action
{
    cover_step_in_action(); // 0x00617A10 (.h:19)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00617A70 (.h:19)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E3E60 (:1525)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006580B0 (:1530)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E3E70 (:1586)
    static ai_action* get_instance();
    virtual ~cover_step_in_action(); // vtbl+0x00 0x005E2B30

private:
    static cover_step_in_action the_instance; // 0x01DCA5D4
};

// size 0x34
class cover_step_out_action : public ai_action
{
    cover_step_out_action(); // 0x00617970 (.h:18)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006179D0 (.h:18)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E3EC0 (:1613)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00658260 (:1618)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006588D0 (:1694)
    static ai_action* get_instance();
    virtual ~cover_step_out_action(); // vtbl+0x00 0x005E2B20

private:
    static cover_step_out_action the_instance; // 0x01D9F63C
};
