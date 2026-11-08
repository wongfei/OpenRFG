// Original: volition/rfg/code/ai/combat/combat_draw_reload_actions.h
// Implementation: volition/rfg/code/ai/combat/combat_draw_reload_actions.cpp
//
// Weapon handling actions: drawing a firearm or melee weapon, holstering, dropping and reloading, and the rules that
// mark a weapon effective. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class npc;

// size 0x34
class draw_firearm_action : public ai_action
{
    draw_firearm_action(); // 0x0061FEA0 (.h:15)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00623D10 (:88)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00620000 (.h:15)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E4280 (:128)
    virtual ai_results enter(npc* np, const ai_action* prev_action) const; // vtbl+0x10 0x006589E0 (:134)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E4290 (:153)
    static ai_action* get_instance();
    virtual ~draw_firearm_action(); // vtbl+0x00 0x005E3ED0

private:
    static draw_firearm_action the_instance; // 0x01DA60DC
};

// size 0x34
class draw_melee_action : public ai_action
{
    draw_melee_action(); // 0x00620030 (.h:16)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x005E42E0 (:179)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00620260 (.h:16)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E4360 (:202)
    virtual ai_results enter(npc* np, const ai_action* prev_action) const; // vtbl+0x10 0x00658A90 (:208)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E4370 (:233)
    static ai_action* get_instance();
    virtual ~draw_melee_action(); // vtbl+0x00 0x005E3EE0

private:
    static draw_melee_action the_instance; // 0x01DC9B34
};

// size 0x34
class holster_weapon_action : public ai_action
{
    holster_weapon_action(); // 0x00620290 (.h:17)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006202E0 (.h:17)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E43C0 (:259)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00658B40 (:264)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E43D0 (:281)
    static ai_action* get_instance();
    virtual ~holster_weapon_action(); // vtbl+0x00 0x005E3EF0

private:
    static holster_weapon_action the_instance; // 0x01DDB088
};

// size 0x34
class drop_weapon_action : public ai_action
{
    drop_weapon_action(); // 0x00620320 (.h:18)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006205C0 (.h:18)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E4420 (:302)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E4430 (:307)
    static ai_action* get_instance();
    virtual ~drop_weapon_action(); // vtbl+0x00 0x005E3F00

private:
    static drop_weapon_action the_instance; // 0x01D68714
};

// size 0x34
class reload_weapon_action : public ai_action
{
    reload_weapon_action(); // 0x00620600 (.h:20)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00620940 (.h:20)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E4450 (:318)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00694F70 (:324)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00695C80 (:384)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005E4460 (:412)
    static ai_action* get_instance();
    virtual ~reload_weapon_action(); // vtbl+0x00 0x005E3F10

private:
    static reload_weapon_action the_instance; // 0x01DCA218
};

// size 0x34
class firearm_effective_rule : public ai_production_rule
{
    firearm_effective_rule(); // 0x00620980 (.h:24)
public:
    static ai_action* get_instance();
    virtual ~firearm_effective_rule(); // vtbl+0x00 0x005E3F20

private:
    static firearm_effective_rule the_instance; // 0x01D7505C
};

// size 0x34
class melee_effective_rule : public ai_production_rule
{
    melee_effective_rule(); // 0x00620A80 (.h:25)
public:
    static ai_action* get_instance();
    virtual ~melee_effective_rule(); // vtbl+0x00 0x005E3F30

private:
    static melee_effective_rule the_instance; // 0x01DC9FE8
};
