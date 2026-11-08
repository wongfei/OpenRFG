// Original: volition/rfg/code/ai/combat/dodge_actions.h
// Implementation: volition/rfg/code/ai/combat/dodge_actions.cpp
//
// Dodges: sideways and running dodges with or without animation, shield cover and spin deflects. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class npc;

enum dodge_directions
{
    DODGE_LEFT = 0,
    DODGE_RIGHT = 1,
    NUM_DODGE_DIRECTIONS = 2,
};

// size 0x34
class dodge_sideways_anim_action : public ai_action
{
    dodge_sideways_anim_action(); // 0x00629ED0 (.h:12)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00679490 (:174)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0062A1B0 (.h:12)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x0069C220 (:254)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0065BC90 (:265)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E7800 (:292)
    static ai_action* get_instance();
    virtual ~dodge_sideways_anim_action(); // vtbl+0x00 0x005E7720

private:
    static dodge_sideways_anim_action the_instance; // 0x01DDAE70
};

// size 0x34
class dodge_sideways_noanim_action : public ai_action
{
    dodge_sideways_noanim_action(); // 0x0062A4F0 (.h:14)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006796F0 (:308)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0062A7D0 (.h:14)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x0069C270 (:371)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0065BD70 (:380)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E7860 (:426)
    static ai_action* get_instance();
    virtual ~dodge_sideways_noanim_action(); // vtbl+0x00 0x005E7740

private:
    static dodge_sideways_noanim_action the_instance; // 0x01DDB590
};

// size 0x34
class dodge_run_anim_action : public ai_action
{
    dodge_run_anim_action(); // 0x0062A1E0 (.h:13)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00679AE0 (:443)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0062A4C0 (.h:13)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x0069C2D0 (:505)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0065BF40 (:516)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E78C0 (:540)
    static ai_action* get_instance();
    virtual ~dodge_run_anim_action(); // vtbl+0x00 0x005E7730

private:
    static dodge_run_anim_action the_instance; // 0x01D683DC
};

// size 0x34
class dodge_run_noanim_action : public ai_action
{
    dodge_run_noanim_action(); // 0x0062A800 (.h:15)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00679DD0 (:556)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0062AAE0 (.h:15)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x0069C320 (:610)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0065C020 (:618)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E7920 (:637)
    static ai_action* get_instance();
    virtual ~dodge_run_noanim_action(); // vtbl+0x00 0x005E7750

private:
    static dodge_run_noanim_action the_instance; // 0x01DA0300
};

// size 0x34
class shield_cover_action : public ai_action
{
    shield_cover_action(); // 0x0062AE20 (.h:17)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0062B130 (:655)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0062B100 (.h:17)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x0069C370 (:674)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005E7A70 (:680)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069C3A0 (:693)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005E7AA0 (:707)
    static ai_action* get_instance();
    virtual ~shield_cover_action(); // vtbl+0x00 0x005E7770

private:
    static shield_cover_action the_instance; // 0x01D751D8
};

// size 0x34
class spin_deflect_action : public ai_action
{
    spin_deflect_action(); // 0x0062AB10 (.h:16)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0062B190 (:718)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0062ADF0 (.h:16)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x0069C450 (:775)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0062B370 (:784)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E7AD0 (:834)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005E7B30 (:847)
    static ai_action* get_instance();
    virtual ~spin_deflect_action(); // vtbl+0x00 0x005E7760

private:
    static spin_deflect_action the_instance; // 0x01DC9838
};
