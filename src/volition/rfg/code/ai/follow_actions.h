// Original: volition/rfg/code/ai/follow_actions.h
// Implementation: volition/rfg/code/ai/follow_actions.cpp
//
// Follow actions: following a leader or formation, waiting for the squad or bodyguards, riding as a passenger,
// escorting (real or simulated) vehicles, convoys, and moving with or defending a human. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class ai_system;
class npc;

// size 0x34
class follow_leader_action : public ai_action
{
    follow_leader_action(); // 0x0062FD30 (.h:21)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006317E0 (:192)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00630030 (.h:21)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E9310 (:215)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006BCF70 (:223)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B6C10 (:258)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x006BD000 (:311)
    static ai_action* get_instance(); // 0x005E91F0 (.h:21)
    virtual ~follow_leader_action(); // vtbl+0x00 0x005E9200

private:
    static follow_leader_action the_instance; // 0x01D68040
};

// size 0x34
class watch_leader_action : public ai_action
{
    watch_leader_action(); // 0x00630060 (.h:22)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006300B0 (.h:22)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00631850 (:332)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069F670 (:357)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E9340 (:377)
    static ai_action* get_instance();
    virtual ~watch_leader_action(); // vtbl+0x00 0x005E9210

private:
    static watch_leader_action the_instance; // 0x01D6829C
};

// size 0x34
class wait_for_squad_action : public ai_action
{
    wait_for_squad_action(); // 0x0062F580 (.h:15)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0062F8D0 (.h:15)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006318E0 (:385)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00631940 (:409)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0065DC40 (:436)
    static ai_action* get_instance();
    virtual ~wait_for_squad_action(); // vtbl+0x00 0x005E91B0

private:
    static wait_for_squad_action the_instance; // 0x01DA05D4
};

// size 0x34
class wait_for_bodyguards_action : public ai_action
{
    wait_for_bodyguards_action(); // 0x0062F9B0 (.h:17)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0062FC50 (.h:17)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E9350 (:467)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069F6F0 (:472)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069F870 (:506)
    static ai_action* get_instance();
    virtual ~wait_for_bodyguards_action(); // vtbl+0x00 0x005E91D0

private:
    static wait_for_bodyguards_action the_instance; // 0x01DA6144
};

// size 0x34
class wait_bodyguards_vehicle_action : public ai_action
{
    wait_bodyguards_vehicle_action(); // 0x0062FC90 (.h:18)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0062FCF0 (.h:18)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E9360 (:551)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006319B0 (:556)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0067B1C0 (:577)
    static ai_action* get_instance();
    virtual ~wait_bodyguards_vehicle_action(); // vtbl+0x00 0x005E91E0

private:
    static wait_bodyguards_vehicle_action the_instance; // 0x01D75800
};

// size 0x34
class wait_for_squad_vehicle_action : public ai_action
{
    wait_for_squad_vehicle_action(); // 0x0062F910 (.h:16)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0062F970 (.h:16)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0067B220 (:603)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00631A00 (:628)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00631A50 (:652)
    static ai_action* get_instance();
    virtual ~wait_for_squad_vehicle_action(); // vtbl+0x00 0x005E91C0

private:
    static wait_for_squad_vehicle_action the_instance; // 0x01D72100
};

// size 0x34
class passenger_of_squadmate_action : public ai_action
{
    passenger_of_squadmate_action(); // 0x006300F0 (.h:24)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0069F990 (:690)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006301B0 (.h:24)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x00631B00 (:768)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00631B50 (:782)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B6D00 (:794)
    static ai_action* get_instance();
    virtual ~passenger_of_squadmate_action(); // vtbl+0x00 0x005E9220

private:
    static passenger_of_squadmate_action the_instance; // 0x01DC95BC
};

// size 0x34
class passenger_action : public ai_action
{
    passenger_action(); // 0x006301E0 (.h:25)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00630230 (.h:25)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0067B290 (:892)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00631BC0 (:906)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B6D60 (:918)
    static ai_action* get_instance();
    virtual ~passenger_action(); // vtbl+0x00 0x005E9230

private:
    static passenger_action the_instance; // 0x01D74FD8
};

// size 0x34
class ride_with_human_action : public ai_action
{
    ride_with_human_action(); // 0x00630D00 (.h:31)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0069FC80 (:977)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00630D60 (.h:31)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E9370 (:1051)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00631C30 (:1059)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B6DC0 (:1071)
    static ai_action* get_instance();
    virtual ~ride_with_human_action(); // vtbl+0x00 0x005E9270

private:
    static ride_with_human_action the_instance; // 0x01D9F898
};

// size 0x34
class escort_vehicle_action : public ai_action
{
    escort_vehicle_action(); // 0x00630270 (.h:26)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006305F0 (.h:26)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00631CA0 (:1219)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006BD020 (:1239)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B6E20 (:1273)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x006BD0B0 (:1321)
    static ai_action* get_instance();
    virtual ~escort_vehicle_action(); // vtbl+0x00 0x005E9240

private:
    static escort_vehicle_action the_instance; // 0x01DC9624
};

// size 0x34
class escort_simulated_vehicle_action : public ai_action
{
    escort_simulated_vehicle_action(); // 0x00630630 (.h:27)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00630980 (.h:27)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00631CE0 (:1336)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069FFA0 (:1355)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069FFC0 (:1369)
    static ai_action* get_instance();
    virtual ~escort_simulated_vehicle_action(); // vtbl+0x00 0x005E9250

private:
    static escort_simulated_vehicle_action the_instance; // 0x01DCA420
};

// size 0x34
class follow_escort_simulated_rule : public ai_production_rule
{
    follow_escort_simulated_rule(); // 0x00631250 (.h:42)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006312C0 (.h:42)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00631D40 (:1399)
public:
    static ai_action* get_instance();
    virtual ~follow_escort_simulated_rule(); // vtbl+0x00 0x005E92D0

private:
    static follow_escort_simulated_rule the_instance; // 0x01DA0424
};

// size 0x34
class leave_convoy_action : public ai_action
{
    leave_convoy_action(); // 0x00631110 (.h:36)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00631160 (.h:36)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0067B2D0 (:1421)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0067B2F0 (:1437)
    static ai_action* get_instance();
    virtual ~leave_convoy_action(); // vtbl+0x00 0x005E92B0

private:
    static leave_convoy_action the_instance; // 0x01DCA67C
};

// size 0x34
class convoy_action : public ai_action
{
    convoy_action(); // 0x00630F30 (.h:35)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006A0080 (:1459)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006310E0 (.h:35)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E93A0 (:1488)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006BD0E0 (:1496)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B6F20 (:1530)
    static ai_action* get_instance();
    virtual ~convoy_action(); // vtbl+0x00 0x005E92A0

private:
    static convoy_action the_instance; // 0x01DDB1C4
};

// size 0x34
class move_with_human_action : public ai_action
{
    move_with_human_action(); // 0x006309C0 (.h:30)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00630CC0 (.h:30)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E93D0 (:1570)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006BD190 (:1575)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B6FE0 (:1613)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x006BD270 (:1693)
    static ai_action* get_instance();
    virtual ~move_with_human_action(); // vtbl+0x00 0x005E9260

private:
    static move_with_human_action the_instance; // 0x01DC9E18
};

// size 0x34
class drive_to_human_rule : public ai_production_rule
{
    drive_to_human_rule(); // 0x006311A0 (.h:41)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00631220 (.h:41)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00631DA0 (:1711)
public:
    static ai_action* get_instance();
    virtual ~drive_to_human_rule(); // vtbl+0x00 0x005E92C0

private:
    static drive_to_human_rule the_instance; // 0x01D726A8
};

// size 0x34
class move_with_human_unarmed_rule : public ai_production_rule
{
    move_with_human_unarmed_rule(); // 0x00630E90 (.h:33)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00630F00 (.h:33)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E93E0 (:1741)
public:
    static ai_action* get_instance();
    virtual ~move_with_human_unarmed_rule(); // vtbl+0x00 0x005E9290

private:
    static move_with_human_unarmed_rule the_instance; // 0x01DA0500
};

// size 0x34
class defend_human_rule : public ai_production_rule
{
    defend_human_rule(); // 0x00630D90 (.h:32)
public:
    static ai_action* get_instance();
    virtual ~defend_human_rule(); // vtbl+0x00 0x005E9280

private:
    static defend_human_rule the_instance; // 0x01DCA608
};

bool all_followers_stopped(const npc* leader_np, const ai_system* leader_aip); // 0x0065DBA0 (:110)
ai_results ai_get_wait_for_bodyguards_result(const npc* np); // 0x0067B010 (:151)
