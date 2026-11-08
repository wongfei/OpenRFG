// Original: volition/rfg/code/ai/combat/combat_thrown_actions.h
// Implementation: volition/rfg/code/ai/combat/combat_thrown_actions.cpp
//
// Thrown weapon actions: grenades thrown directly or over cover, placing bombs, and the rules that check a throw kills
// the enemy. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class npc;
class vector;

// size 0x34
class attack_grenade_direct_action : public ai_action
{
    attack_grenade_direct_action(); // 0x00626050 (.h:15)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00626330 (.h:15)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00626F20 (:466)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069A7D0 (:496)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E6720 (:523)
    static ai_action* get_instance();
    virtual ~attack_grenade_direct_action(); // vtbl+0x00 0x005E65B0

private:
    static attack_grenade_direct_action the_instance; // 0x01DC9F04
};

// size 0x34
class attack_grenade_cover_action : public ai_action
{
    attack_grenade_cover_action(); // 0x00626370 (.h:16)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006263D0 (.h:16)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00626F70 (:545)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069A850 (:563)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E6770 (:571)
    static ai_action* get_instance();
    virtual ~attack_grenade_cover_action(); // vtbl+0x00 0x005E65C0

private:
    static attack_grenade_cover_action the_instance; // 0x01DA3E7C
};

// size 0x34
class place_bomb_action : public ai_action
{
    place_bomb_action(); // 0x00626410 (.h:18)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00678840 (:579)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00626710 (.h:18)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E67A0 (:627)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069A880 (:634)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E6830 (:673)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005E6860 (:678)
    static ai_action* get_instance();
    virtual ~place_bomb_action(); // vtbl+0x00 0x005E65D0

private:
    static place_bomb_action the_instance; // 0x01D75574
};

// size 0x34
class goto_enemy_to_place_bomb_action : public ai_action
{
    goto_enemy_to_place_bomb_action(); // 0x00626740 (.h:19)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00626FB0 (:688)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006269E0 (.h:19)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E6870 (:721)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069A980 (:727)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B6270 (:755)
    static ai_action* get_instance();
    virtual ~goto_enemy_to_place_bomb_action(); // vtbl+0x00 0x005E65E0

private:
    static goto_enemy_to_place_bomb_action the_instance; // 0x01DDB940
};

// size 0x34
class grenade_kills_enemy_rule : public ai_production_rule
{
    grenade_kills_enemy_rule(); // 0x00626A10 (.h:21)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00626A90 (.h:21)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006789D0 (:809)
public:
    static ai_action* get_instance();
    virtual ~grenade_kills_enemy_rule(); // vtbl+0x00 0x005E65F0

private:
    static grenade_kills_enemy_rule the_instance; // 0x01DA6040
};

// size 0x34
class bomb_kills_enemy_rule : public ai_production_rule
{
    bomb_kills_enemy_rule(); // 0x00626AC0 (.h:22)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00626B40 (.h:22)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006270B0 (:875)
public:
    static ai_action* get_instance();
    virtual ~bomb_kills_enemy_rule(); // vtbl+0x00 0x005E6600

private:
    static bomb_kills_enemy_rule the_instance; // 0x01D71FA8
};

ai_results attack_grenade_enter_common(npc* np, bool try_to_lead_target, vector* throw_vel_output); // 0x0069A4B0 (:325)
ai_results attack_grenade_do_action_common(npc* np); // 0x005E6700 (:441)
