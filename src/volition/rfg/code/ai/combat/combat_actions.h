// Original: volition/rfg/code/ai/combat/combat_actions.h
// Implementation: volition/rfg/code/ai/combat/combat_actions.cpp
//
// The basic attack action. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"

struct ai_action_setup_data;
class ai_combat_action_parameters;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class npc;

// size 0x34
class attack_action : public ai_action
{
    attack_action(); // 0x00616EC0 (.h:18)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00617210 (.h:18)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00657440 (:359)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00693DB0 (:382)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B6100 (:447)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x006172C0 (:489)
    static ai_action* get_instance(); // 0x005E2AC0 (.h:18)
    virtual ~attack_action(); // vtbl+0x00 0x005E2AD0

private:
    static attack_action the_instance; // 0x01D7253C
};

void combat_position_adjust_enter(npc* np); // 0x00617250 (:289)
void combat_position_adjust_do_frame(const ai_combat_action_parameters& params, bool allow_stance_changes); // 0x00693C40 (:312)
