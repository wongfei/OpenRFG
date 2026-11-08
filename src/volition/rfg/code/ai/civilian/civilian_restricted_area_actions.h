// Original: volition/rfg/code/ai/civilian/civilian_restricted_area_actions.h
// Implementation: volition/rfg/code/ai/civilian/civilian_restricted_area_actions.cpp
//
// Civilians and restricted areas: fleeing a restricted region or a threatening npc, confronting or cowering from it,
// getting on their knees and showing their ID. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
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
class flee_restricted_region_action : public ai_action
{
    flee_restricted_region_action(); // 0x00612400 (.h:7)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00612700 (.h:7)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E1470 (:316)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00674E30 (:321)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00690CA0 (:330)
    static ai_action* get_instance();
    virtual ~flee_restricted_region_action(); // vtbl+0x00 0x005E1330

private:
    static flee_restricted_region_action the_instance; // 0x01DC9754
};

// size 0x34
class flee_threatening_npc_action : public ai_action
{
    flee_threatening_npc_action(); // 0x00612740 (.h:8)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00612A40 (.h:8)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00674F90 (:379)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00690CC0 (:386)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00690E30 (:421)
    static ai_action* get_instance();
    virtual ~flee_threatening_npc_action(); // vtbl+0x00 0x005E1340

private:
    static flee_threatening_npc_action the_instance; // 0x01D74F70
};

// size 0x34
class confront_threatening_npc_action : public ai_action
{
    confront_threatening_npc_action(); // 0x00612A80 (.h:9)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00675030 (:431)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00612D20 (.h:9)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x00690EB0 (:467)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005E14A0 (:473)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006551C0 (:491)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x006136B0 (:544)
    static ai_action* get_instance(); // 0x005E1350 (.h:9)
    virtual ~confront_threatening_npc_action(); // vtbl+0x00 0x005E1360

private:
    static confront_threatening_npc_action the_instance; // 0x01DDBA04
};

// size 0x34
class cower_from_threatening_npc_action : public ai_action
{
    cower_from_threatening_npc_action(); // 0x00612D50 (.h:10)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00675120 (:573)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00612FF0 (.h:10)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x00690EE0 (:609)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005E14E0 (:615)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006559A0 (:635)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00655BD0 (:672)
    static ai_action* get_instance();
    virtual ~cower_from_threatening_npc_action(); // vtbl+0x00 0x005E1370

private:
    static cower_from_threatening_npc_action the_instance; // 0x01DCA2AC
};

// size 0x34
class get_on_knees_action : public ai_action
{
    get_on_knees_action(); // 0x00613020 (.h:11)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006132C0 (.h:11)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E1520 (:685)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00613760 (:696)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00655C00 (:725)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00655D90 (:778)
    static ai_action* get_instance(); // 0x005E1380 (.h:11)
    virtual ~get_on_knees_action(); // vtbl+0x00 0x005E1390

private:
    static get_on_knees_action the_instance; // 0x01D72628
};

// size 0x34
class civilian_show_id_action : public ai_action
{
    civilian_show_id_action(); // 0x00613300 (.h:12)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006135A0 (.h:12)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E1560 (:800)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005E15A0 (:811)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006137D0 (:827)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005E15F0 (:871)
    static ai_action* get_instance(); // 0x005E13A0 (.h:12)
    virtual ~civilian_show_id_action(); // vtbl+0x00 0x005E13B0

private:
    static civilian_show_id_action the_instance; // 0x01DC9CA8
};

bool point_satisfies_search(float (*heuristic_func)(const vector&, void*), const vector& pos, void* heuristic_data); // 0x005E13C0 (:36)
bool do_get_cost_tests(const npc* np, float (*heuristic_func)(const vector&, void*), void* heuristic_data); // 0x005E13F0 (:246)
ai_results flee_enter_common(npc* np, bool first_frame, float (*heuristic_func)(const vector&, void*), void* heuristic_data); // 0x00674DC0 (:253)
ai_results flee_do_action_common(npc* np, float (*heuristic_func)(const vector&, void*), void* heuristic_data); // 0x00690C00 (:268)
