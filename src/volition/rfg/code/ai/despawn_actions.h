// Original: volition/rfg/code/ai/despawn_actions.h
// Implementation: volition/rfg/code/ai/despawn_actions.cpp
//
// Despawn actions: leaving the world on foot, by flyer or vehicle, into a building, along the ped node network, or by
// retreating or fleeing out of view. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"
#include "volition/rfg/code/objects/humans/human_enums.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class npc;

// size 0x34
class despawn_flyer_action : public ai_action
{
    despawn_flyer_action(); // 0x0062CD20 (.h:15)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0062CD80 (.h:15)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0062EB70 (:439)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0062EBC0 (:457)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069EAA0 (:485)
    static ai_action* get_instance();
    virtual ~despawn_flyer_action(); // vtbl+0x00 0x005E7F90

private:
    static despawn_flyer_action the_instance; // 0x01D72314
};

// size 0x34
class despawn_at_object_action : public ai_action
{
    despawn_at_object_action(); // 0x0062CA40 (.h:14)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0062CCE0 (.h:14)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0062EC60 (:516)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0065D030 (:535)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069EB40 (:548)
    static ai_action* get_instance();
    virtual ~despawn_at_object_action(); // vtbl+0x00 0x005E7F80

private:
    static despawn_at_object_action the_instance; // 0x01D75604
};

// size 0x34
class building_despawn_action : public ai_action
{
    building_despawn_action(); // 0x0062D0A0 (.h:17)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0062D340 (.h:17)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0067AE40 (:594)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0065D090 (:611)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0062ECA0 (:624)
    static ai_action* get_instance();
    virtual ~building_despawn_action(); // vtbl+0x00 0x005E7FB0

private:
    static building_despawn_action the_instance; // 0x01D754DC
};

// size 0x34
class despawn_ped_network_action : public ai_action
{
    despawn_ped_network_action(); // 0x0062CDC0 (.h:16)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0062D060 (.h:16)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E8F50 (:634)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0065D1A0 (:656)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069ECD0 (:671)
    static ai_action* get_instance();
    virtual ~despawn_ped_network_action(); // vtbl+0x00 0x005E7FA0

private:
    static despawn_ped_network_action the_instance; // 0x01DDB6F0
};

// size 0x34
class default_despawn_action : public ai_action
{
    default_despawn_action(); // 0x0062D380 (.h:18)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0062D680 (.h:18)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E9050 (:696)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0065D220 (:701)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069ED50 (:722)
    static ai_action* get_instance();
    virtual ~default_despawn_action(); // vtbl+0x00 0x005E7FC0

private:
    static default_despawn_action the_instance; // 0x01DA6110
};

// size 0x34
class retreat_despawn_action : public ai_action
{
    retreat_despawn_action(); // 0x0062D6C0 (.h:19)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0062DA70 (.h:19)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0062ED10 (:737)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069EDA0 (:756)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069EF20 (:781)
    static ai_action* get_instance();
    virtual ~retreat_despawn_action(); // vtbl+0x00 0x005E7FD0

private:
    static retreat_despawn_action the_instance; // 0x01DCA534
};

// size 0x34
class retreat_unarmed_despawn_action : public ai_action
{
    retreat_unarmed_despawn_action(); // 0x0062DAB0 (.h:20)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0062DDB0 (.h:20)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0062ED50 (:808)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0065D380 (:832)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069EFA0 (:853)
    static ai_action* get_instance();
    virtual ~retreat_unarmed_despawn_action(); // vtbl+0x00 0x005E7FE0

private:
    static retreat_unarmed_despawn_action the_instance; // 0x01D71FDC
};

// size 0x34
class flee_despawn_action : public ai_action
{
    flee_despawn_action(); // 0x0062DDF0 (.h:21)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0062E0F0 (.h:21)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0069EFD0 (:870)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0065D4E0 (:883)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069F000 (:896)
    static ai_action* get_instance();
    virtual ~flee_despawn_action(); // vtbl+0x00 0x005E7FF0

private:
    static flee_despawn_action the_instance; // 0x01D755D0
};

// size 0x34
class drive_despawn_road_action : public ai_action
{
    drive_despawn_road_action(); // 0x0062E130 (.h:24)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0069F020 (:905)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0062E190 (.h:24)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E9060 (:937)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0067AEC0 (:945)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B6660 (:997)
    static ai_action* get_instance(); // 0x00BC2E90 (.h:24)
    virtual ~drive_despawn_road_action(); // vtbl+0x00 0x005E8000

private:
    static drive_despawn_road_action the_instance; // 0x01D71F38
};

// size 0x34
class drive_despawn_action : public ai_action
{
    drive_despawn_action(); // 0x0062E1C0 (.h:25)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0069F150 (:1074)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0062E220 (.h:25)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E9090 (:1098)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0062EDA0 (:1106)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B67E0 (:1149)
    static ai_action* get_instance();
    virtual ~drive_despawn_action(); // vtbl+0x00 0x005E8010

private:
    static drive_despawn_action the_instance; // 0x01D68234
};

// size 0x34
class passenger_despawn_action : public ai_action
{
    passenger_despawn_action(); // 0x0062E250 (.h:26)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0069F260 (:1233)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0062E2B0 (.h:26)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E90C0 (:1252)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0062EED0 (:1260)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B6A30 (:1272)
    static ai_action* get_instance();
    virtual ~passenger_despawn_action(); // vtbl+0x00 0x005E8020

private:
    static passenger_despawn_action the_instance; // 0x01DDBA38
};

ai_results despawn_on_foot_common_enter(npc* np, bool first_frame); // 0x005E8F00 (:347)
ai_results despawn_on_foot_common_do_action(npc* np, human_movement_states m_state); // 0x0069E830 (:366)
