// Original: volition/rfg/code/ai/investigation_actions.h
// Implementation: volition/rfg/code/ai/investigation_actions.cpp
//
// Investigation: investigating disturbances (looking, finding, examining), accompanying an investigator, examining
// corpses (reserved per corpse) and mines, moving away from mines. Goals and actions live together here.
// Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
struct nav_cell;
class navpoint;
class npc;

// size 0xC
struct navpoint_info
{
    const navpoint* navp; // 0x00
    float min_mine_dist_sqrd; // 0x04
    float npc_dist_sqrd; // 0x08
};

// size 0x18
struct mine_nav_cell_info
{
    mine_nav_cell_info(); // 0x005EB4B0

    const nav_cell* ncp; // 0x00
    vector center; // 0x04
    float min_mine_dist_sqrd; // 0x10
    float npc_dist_sqrd; // 0x14
};

// size 0xC
struct corpse_reservation_info
{
    corpse_reservation_info(); // 0x005EB480 (:67)

    unsigned int npc_handle; // 0x00
    unsigned int corpse_handle; // 0x04
    timestamp reservation_time; // 0x08
};

// size 0x1C
class investigate_disturbance_goal : public ai_goal
{
    investigate_disturbance_goal(); // 0x00634DF0 (.h:14)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00639C10 (:838)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0067FAC0 (:882)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A4310 (:931)
    static ai_goal* get_instance(); // 0x005E9E00 (.h:14)
    virtual ~investigate_disturbance_goal(); // vtbl+0x00 0x005E9E10

private:
    static investigate_disturbance_goal the_instance; // 0x01DCA24C
};

// size 0x1C
class accompany_investigation_goal : public ai_goal
{
    accompany_investigation_goal(); // 0x00634E90 (.h:15)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x006626B0 (:947)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x00662950 (:991)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A4350 (:1039)
    static ai_goal* get_instance();
    virtual ~accompany_investigation_goal(); // vtbl+0x00 0x005E9E20

private:
    static accompany_investigation_goal the_instance; // 0x01DDB174
};

// size 0x1C
class investigate_mine_goal : public ai_goal
{
    investigate_mine_goal(); // 0x00634F30 (.h:18)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00639E70 (:1050)
    static ai_goal* get_instance();
    virtual ~investigate_mine_goal(); // vtbl+0x00 0x005E9E30

private:
    static investigate_mine_goal the_instance; // 0x01D6858C
};

// size 0x34
class look_at_disturbance_action : public ai_action
{
    look_at_disturbance_action(); // 0x00634FD0 (.h:21)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00635200 (.h:21)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00639EE0 (:1080)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006A43A0 (:1096)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005EB970 (:1140)
    static ai_action* get_instance();
    virtual ~look_at_disturbance_action(); // vtbl+0x00 0x005E9E40

private:
    static look_at_disturbance_action the_instance; // 0x01D67ED4
};

// size 0x34
class find_disturbance_action : public ai_action
{
    find_disturbance_action(); // 0x00635240 (.h:22)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00635590 (.h:22)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006A44E0 (:1155)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00662A20 (:1172)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006A45A0 (:1185)
    static ai_action* get_instance();
    virtual ~find_disturbance_action(); // vtbl+0x00 0x005E9E50

private:
    static find_disturbance_action the_instance; // 0x01D67FF0
};

// size 0x34
class examine_disturbance_action : public ai_action
{
    examine_disturbance_action(); // 0x006355D0 (.h:23)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00635980 (.h:23)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005EB9D0 (:1232)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00662AA0 (:1237)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006A4740 (:1258)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00639F10 (:1328)
    static ai_action* get_instance();
    virtual ~examine_disturbance_action(); // vtbl+0x00 0x005E9E60

private:
    static examine_disturbance_action the_instance; // 0x01D757CC
};

// size 0x34
class accompany_investigation_action : public ai_action
{
    accompany_investigation_action(); // 0x006359C0 (.h:24)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00635D10 (.h:24)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005EB9E0 (:1345)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006BD2B0 (:1350)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006A4980 (:1392)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x006BD3D0 (:1428)
    static ai_action* get_instance();
    virtual ~accompany_investigation_action(); // vtbl+0x00 0x005E9E70

private:
    static accompany_investigation_action the_instance; // 0x01D68444
};

// size 0x34
class go_to_corpse_action : public ai_action
{
    go_to_corpse_action(); // 0x00635D50 (.h:27)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00636050 (.h:27)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0067FC10 (:1443)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00662B50 (:1461)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006A4A40 (:1479)
    static ai_action* get_instance();
    virtual ~go_to_corpse_action(); // vtbl+0x00 0x005E9E80

private:
    static go_to_corpse_action the_instance; // 0x01DA6178
};

// size 0x34
class examine_corpse_action : public ai_action
{
    examine_corpse_action(); // 0x00636090 (.h:28)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00636390 (.h:28)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00639F40 (:1530)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006A4B70 (:1548)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00662D00 (:1582)
    static ai_action* get_instance();
    virtual ~examine_corpse_action(); // vtbl+0x00 0x005E9E90

private:
    static examine_corpse_action the_instance; // 0x01D685E8
};

// size 0x34
class move_away_from_mine_action : public ai_action
{
    move_away_from_mine_action(); // 0x006363D0 (.h:31)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00636670 (.h:31)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00639F80 (:1626)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0067FC50 (:1652)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006A4C50 (:1670)
    static ai_action* get_instance();
    virtual ~move_away_from_mine_action(); // vtbl+0x00 0x005E9EA0

private:
    static move_away_from_mine_action the_instance; // 0x01DCA120
};

// size 0x34
class examine_mine_action : public ai_action
{
    examine_mine_action(); // 0x006366B0 (.h:32)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006369B0 (.h:32)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005EB9F0 (:1705)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006A4D00 (:1710)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00639FD0 (:1733)
    static ai_action* get_instance();
    virtual ~examine_mine_action(); // vtbl+0x00 0x005E9EB0

private:
    static examine_mine_action the_instance; // 0x01D723E4
};
