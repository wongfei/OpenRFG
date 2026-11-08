// Original: volition/rfg/code/ai/combat/flush_out_actions.h
// Implementation: volition/rfg/code/ai/combat/flush_out_actions.cpp
//
// Flushing out: holding position (in cover), sidestepping, grenades that flush out an enemy, holding stairwells. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"
#include "volition/vlib/code/math/vector3.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class ai_system;
class npc;

// size 0x38
struct hold_stairwell_cover_collect_cb_data
{
    hold_stairwell_cover_collect_cb_data(const npc* np_, const vector& close_pos_, const vector& stair_fvec_, const vector& target_pos1_, const vector& target_pos2_); // 0x005E7BD0 (:44)

    const npc* np; // 0x00
    const ai_system* ai; // 0x04
    vector close_pos; // 0x08
    vector stair_fvec; // 0x14
    vector target_pos1; // 0x20
    vector target_pos2; // 0x2C
};

// size 0x34
class hold_position_action : public ai_action
{
    hold_position_action(); // 0x0062B540 (.h:12)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0062B840 (.h:12)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0067A5D0 (:537)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069C610 (:578)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069C6D0 (:617)
    static ai_action* get_instance();
    virtual ~hold_position_action(); // vtbl+0x00 0x005E7B50

private:
    static hold_position_action the_instance; // 0x01D72098
};

// size 0x34
class hold_position_cover_action : public ai_action
{
    hold_position_cover_action(); // 0x0062B880 (.h:13)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006BCBC0 (:664)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0062BC20 (.h:13)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x0062C930 (:749)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0067A730 (:759)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0065C660 (:782)
    static ai_action* get_instance();
    virtual ~hold_position_cover_action(); // vtbl+0x00 0x005E7B60

private:
    static hold_position_cover_action the_instance; // 0x01D75090
};

// size 0x34
class sidestep_action : public ai_action
{
    sidestep_action(); // 0x0062BC50 (.h:14)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0062BFA0 (.h:14)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E7F50 (:822)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069C890 (:842)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0065C720 (:907)
    static ai_action* get_instance();
    virtual ~sidestep_action(); // vtbl+0x00 0x005E7B70

private:
    static sidestep_action the_instance; // 0x01D722E0
};

// size 0x34
class grenade_flushes_out_enemy_rule : public ai_production_rule
{
    grenade_flushes_out_enemy_rule(); // 0x0062C350 (.h:19)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0062C3D0 (.h:19)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0062C970 (:947)
public:
    static ai_action* get_instance();
    virtual ~grenade_flushes_out_enemy_rule(); // vtbl+0x00 0x005E7BA0

private:
    static grenade_flushes_out_enemy_rule the_instance; // 0x01D74FA4
};

// size 0x34
class hold_stairwell_rule : public ai_production_rule
{
    hold_stairwell_rule(); // 0x0062C400 (.h:20)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0062C480 (.h:20)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0067A7B0 (:996)
public:
    static ai_action* get_instance();
    virtual ~hold_stairwell_rule(); // vtbl+0x00 0x005E7BB0

private:
    static hold_stairwell_rule the_instance; // 0x01D72168
};

// size 0x34
class stairwell_acquire_rule : public ai_production_rule
{
    stairwell_acquire_rule(); // 0x0062C4B0 (.h:21)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0062C530 (.h:21)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0067A840 (:1023)
public:
    static ai_action* get_instance();
    virtual ~stairwell_acquire_rule(); // vtbl+0x00 0x005E7BC0

private:
    static stairwell_acquire_rule the_instance; // 0x01DDAC6C
};

// size 0x34
class hold_stairwell_cover_action : public ai_action
{
    hold_stairwell_cover_action(); // 0x0062BFE0 (.h:17)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0067AAD0 (:1067)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0062C320 (.h:17)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x0062C9E0 (:1115)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069CA70 (:1127)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0065C890 (:1140)
    static ai_action* get_instance(); // 0x005E7B80 (.h:17)
    virtual ~hold_stairwell_cover_action(); // vtbl+0x00 0x005E7B90

private:
    static hold_stairwell_cover_action the_instance; // 0x01D9F994
};
