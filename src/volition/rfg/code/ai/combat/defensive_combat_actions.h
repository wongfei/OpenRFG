// Original: volition/rfg/code/ai/combat/defensive_combat_actions.h
// Implementation: volition/rfg/code/ai/combat/defensive_combat_actions.cpp
//
// Defensive combat: hiding, going to and fighting from defensive cover, preparing for raids, combat support. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"
#include "volition/rfg/code/objects/humans/human_enums.h"
#include "volition/vlib/code/math/vector3.h"

struct ai_action_setup_data;
class ai_combat_action_parameters;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class cover_node;
class npc;
class object;

enum defensive_combat_check_results
{
    DEFENSIVE_COMBAT_NOT_ALLOWED = 0,
    DEFENSIVE_COMBAT_ALLOWED = 1,
    DEFENSIVE_COMBAT_PREFERRED = 2,
    NUM_DEFENSIVE_COMBAT_CHECK_RESULTS = 3,
};

// size 0x14
struct defensive_cover_info
{
    static bool less_than_or_equal(const defensive_cover_info& first, const defensive_cover_info& second); // 0x005E7080 (:47)
    defensive_cover_info(); // 0x005E7610

    cover_node* node; // 0x00
    float eff_dist; // 0x04
    vector to_target; // 0x08
};

// size 0x34
class defensive_combat_hide_action : public ai_action
{
    defensive_combat_hide_action(); // 0x006186C0 (.h:15)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00618A00 (.h:15)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00629AC0 (:458)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069BDF0 (:507)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E7620 (:519)
    static ai_action* get_instance();
    virtual ~defensive_combat_hide_action(); // vtbl+0x00 0x005E2BE0

private:
    static defensive_combat_hide_action the_instance; // 0x01DDAE3C
};

// size 0x34
class prepare_raid_action : public ai_action
{
    prepare_raid_action(); // 0x00618200 (.h:13)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0065B0D0 (:546)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00618430 (.h:13)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E7660 (:671)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005E7640 (:663)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069BE40 (:679)
    static ai_action* get_instance();
    virtual ~prepare_raid_action(); // vtbl+0x00 0x005E2BB0

private:
    static prepare_raid_action the_instance; // 0x01DC9964
};

// size 0x34
class defensive_combat_cover_action : public ai_action
{
    defensive_combat_cover_action(); // 0x00618460 (.h:14)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0065B550 (:712)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00618690 (.h:14)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E76A0 (:812)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069BEF0 (:820)
    static ai_action* get_instance(); // 0x005E2BC0 (.h:14)
    virtual ~defensive_combat_cover_action(); // vtbl+0x00 0x005E2BD0

private:
    static defensive_combat_cover_action the_instance; // 0x01DDB55C
};

// size 0x34
class defensive_combat_go_to_cover_action : public ai_action
{
    defensive_combat_go_to_cover_action(); // 0x00619140 (.h:23)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006193D0 (.h:23)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0065BB80 (:998)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069BF40 (:1028)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069BFB0 (:1049)
    static ai_action* get_instance();
    virtual ~defensive_combat_go_to_cover_action(); // vtbl+0x00 0x005E2C10

private:
    static defensive_combat_go_to_cover_action the_instance; // 0x01DDB22C
};

// size 0x34
class combat_support_action : public ai_action
{
    combat_support_action(); // 0x00618A40 (.h:19)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00618D90 (.h:19)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00629EA0 (:1091)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069C0E0 (:1102)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069C140 (:1121)
    static ai_action* get_instance();
    virtual ~combat_support_action(); // vtbl+0x00 0x005E2BF0

private:
    static combat_support_action the_instance; // 0x01D759D4
};

// size 0x34
class combat_support_cover_action : public ai_action
{
    combat_support_cover_action(); // 0x00618DD0 (.h:20)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006BC990 (:1146)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00619110 (.h:20)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E76E0 (:1235)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069C1C0 (:1243)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00679440 (:1260)
    static ai_action* get_instance();
    virtual ~combat_support_cover_action(); // vtbl+0x00 0x005E2C00

private:
    static combat_support_cover_action the_instance; // 0x01DCA7C4
};

float get_defensive_node_cost(const npc* np, const object* node, bool sniper); // 0x005E70B0 (:129)
bool get_defensive_cover_info(defensive_cover_info& info, const npc* np, const vector& target_pos, cover_node* cnp, bool sniper); // 0x005E7210 (:193)
bool get_raid_preparation_defensive_cover_info(defensive_cover_info& info, const npc* np, const vector& target_pos, cover_node* cnp, bool sniper); // 0x005E7360 (:226)
bool do_secondary_cover_evaluation(const defensive_cover_info& info); // 0x005E74F0 (:255)
human_movement_states get_defensive_go_to_cover_movement(const ai_combat_action_parameters& params); // 0x006299B0 (:362)
bool prepare_raid_cover_collect_cb(object* obj, unsigned int shape_key, const void* cb_data); // 0x00629BF0 (:527)
