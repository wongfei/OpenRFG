// Original: volition/rfg/code/ai/civilian/pedestrian_actions.h
// Implementation: volition/rfg/code/ai/civilian/pedestrian_actions.cpp
//
// Pedestrian goals and actions: wandering (calm or panicked), ambient animations and pauses, taking, parking and
// riding vehicles, pick-ups and drop-offs, ambient driving and despawning, stopping at checkpoints, following the
// player, returning to orders, reacting to honks. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"
#include "volition/rfg/code/ai/goals.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class npc;

// size 0x1C
class pedestrian_wander_goal : public ai_goal
{
    pedestrian_wander_goal(); // 0x00602D00 (.h:13)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00675290 (:251)
    static ai_goal* get_instance();
    virtual ~pedestrian_wander_goal(); // vtbl+0x00 0x005DDC30

private:
    static pedestrian_wander_goal the_instance; // 0x01642508
};

// size 0x1C
class pedestrian_action_goal : public ai_goal
{
    pedestrian_action_goal(); // 0x00602DA0 (.h:14)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x006561C0 (:271)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x005E1960 (:340)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006562D0 (:361)
    static ai_goal* get_instance();
    virtual ~pedestrian_action_goal(); // vtbl+0x00 0x005DDC40

private:
    static pedestrian_action_goal the_instance; // 0x016424EC
};

// size 0x1C
class pedestrian_panicked_goal : public ai_goal
{
    pedestrian_panicked_goal(); // 0x00602E40 (.h:15)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00656330 (:405)
    static ai_goal* get_instance(); // 0x005DDC50 (.h:15)
    virtual ~pedestrian_panicked_goal(); // vtbl+0x00 0x005DDC60

private:
    static pedestrian_panicked_goal the_instance; // 0x01642524
};

// size 0x1C
class ambient_action_goal : public ai_goal
{
    ambient_action_goal(); // 0x00602EE0 (.h:16)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005E19E0 (:439)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x005E1A10 (:458)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x00615C40 (:472)
    static ai_goal* get_instance(); // 0x005DDC70 (.h:16)
    virtual ~ambient_action_goal(); // vtbl+0x00 0x005DDC80

private:
    static ambient_action_goal the_instance; // 0x01642540
};

// size 0x34
class pedestrian_pause_action : public ai_action
{
    pedestrian_pause_action(); // 0x00603940 (.h:24)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00603BD0 (.h:24)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E1A50 (:505)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00656400 (:520)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E1AA0 (:542)
    static ai_action* get_instance();
    virtual ~pedestrian_pause_action(); // vtbl+0x00 0x005DDCC0

private:
    static pedestrian_pause_action the_instance; // 0x01DC9B68
};

// size 0x34
class pedestrian_anim_action : public ai_action
{
    pedestrian_anim_action(); // 0x006032C0 (.h:21)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006035C0 (.h:21)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E1AB0 (:550)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00656550 (:559)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E1AF0 (:581)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00615D30 (:590)
    static ai_action* get_instance();
    virtual ~pedestrian_anim_action(); // vtbl+0x00 0x005DDCA0

private:
    static pedestrian_anim_action the_instance; // 0x01D68894
};

// size 0x34
class ambient_take_vehicle_action : public ai_action
{
    ambient_take_vehicle_action(); // 0x00605070 (.h:35)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00615D80 (:609)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006050C0 (.h:35)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E1B20 (:653)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00615E60 (:659)
    static ai_action* get_instance();
    virtual ~ambient_take_vehicle_action(); // vtbl+0x00 0x005DDD50

private:
    static ambient_take_vehicle_action the_instance; // 0x01DC9D60
};

// size 0x34
class ambient_park_vehicle_action : public ai_action
{
    ambient_park_vehicle_action(); // 0x006050F0 (.h:36)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00615FC0 (:716)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00605140 (.h:36)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E1B30 (:746)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006160D0 (:752)
    static ai_action* get_instance();
    virtual ~ambient_park_vehicle_action(); // vtbl+0x00 0x005DDD60

private:
    static ambient_park_vehicle_action the_instance; // 0x01D756C0
};

// size 0x34
class ambient_driver_pick_up_action : public ai_action
{
    ambient_driver_pick_up_action(); // 0x00605170 (.h:37)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00616160 (:778)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00605210 (.h:37)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E1B40 (:805)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006161C0 (:811)
    static ai_action* get_instance();
    virtual ~ambient_driver_pick_up_action(); // vtbl+0x00 0x005DDD70

private:
    static ambient_driver_pick_up_action the_instance; // 0x01D684FC
};

// size 0x34
class ambient_passenger_pick_up_action : public ai_action
{
    ambient_passenger_pick_up_action(); // 0x00605240 (.h:38)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00616340 (:865)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00605290 (.h:38)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E1B50 (:888)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00690F90 (:894)
    static ai_action* get_instance();
    virtual ~ambient_passenger_pick_up_action(); // vtbl+0x00 0x005DDD80

private:
    static ambient_passenger_pick_up_action the_instance; // 0x01DC9A20
};

// size 0x34
class ambient_driver_drop_off_action : public ai_action
{
    ambient_driver_drop_off_action(); // 0x006052C0 (.h:39)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00616390 (:939)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00605310 (.h:39)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E1B60 (:965)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006163F0 (:971)
    static ai_action* get_instance();
    virtual ~ambient_driver_drop_off_action(); // vtbl+0x00 0x005DDD90

private:
    static ambient_driver_drop_off_action the_instance; // 0x01D720CC
};

// size 0x34
class ambient_passenger_drop_off_action : public ai_action
{
    ambient_passenger_drop_off_action(); // 0x00605340 (.h:40)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00616540 (:1023)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00605390 (.h:40)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E1B70 (:1045)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00616590 (:1051)
    static ai_action* get_instance();
    virtual ~ambient_passenger_drop_off_action(); // vtbl+0x00 0x005DDDA0

private:
    static ambient_passenger_drop_off_action the_instance; // 0x01D681CC
};

// size 0x34
class ambient_cough_action : public ai_action
{
    ambient_cough_action(); // 0x00603600 (.h:22)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00603900 (.h:22)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E1B80 (:1077)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006565F0 (:1093)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E1BF0 (:1118)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005E1C20 (:1129)
    static ai_action* get_instance();
    virtual ~ambient_cough_action(); // vtbl+0x00 0x005DDCB0

private:
    static ambient_cough_action the_instance; // 0x01D68478
};

// size 0x34
class pedestrian_wander_action : public ai_action
{
    pedestrian_wander_action(); // 0x00603C10 (.h:25)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00603EA0 (.h:25)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E1C50 (:1142)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00656690 (:1155)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006910A0 (:1173)
    static ai_action* get_instance();
    virtual ~pedestrian_wander_action(); // vtbl+0x00 0x005DDCD0

private:
    static pedestrian_wander_action the_instance; // 0x01DC9F6C
};

// size 0x34
class pedestrian_follow_player_action : public ai_action
{
    pedestrian_follow_player_action(); // 0x00603EE0 (.h:26)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006041E0 (.h:26)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E1D70 (:1192)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00691100 (:1213)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B5CC0 (:1242)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005E1DE0 (:1316)
    static ai_action* get_instance(); // 0x005DDCE0 (.h:26)
    virtual ~pedestrian_follow_player_action(); // vtbl+0x00 0x005DDCF0

private:
    static pedestrian_follow_player_action the_instance; // 0x01D67F14
};

// size 0x34
class wander_panicked_action : public ai_action
{
    wander_panicked_action(); // 0x00602F80 (.h:18)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00603280 (.h:18)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E1E40 (:1333)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00656800 (:1342)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00691170 (:1361)
    static ai_action* get_instance();
    virtual ~wander_panicked_action(); // vtbl+0x00 0x005DDC90

private:
    static wander_panicked_action the_instance; // 0x01D750C4
};

// size 0x34
class ambient_veh_ride_action : public ai_action
{
    ambient_veh_ride_action(); // 0x00604AC0 (.h:31)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00604D50 (.h:31)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00675330 (:1382)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E1F10 (:1418)
    static ai_action* get_instance();
    virtual ~ambient_veh_ride_action(); // vtbl+0x00 0x005DDD30

private:
    static ambient_veh_ride_action the_instance; // 0x01DC9D94
};

// size 0x34
class ambient_veh_drive_action : public ai_action
{
    ambient_veh_drive_action(); // 0x006047E0 (.h:30)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00604A80 (.h:30)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006753B0 (:1443)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006568C0 (:1482)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006911C0 (:1546)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00616620 (:1598)
    static ai_action* get_instance(); // 0x00BC2EB0 (.h:30)
    virtual ~ambient_veh_drive_action(); // vtbl+0x00 0x005DDD20

private:
    static ambient_veh_drive_action the_instance; // 0x01DC9BF8
};

// size 0x34
class ambient_veh_despawn_action : public ai_action
{
    ambient_veh_despawn_action(); // 0x00604500 (.h:29)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006047A0 (.h:29)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00675460 (:1620)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00656A30 (:1658)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006912B0 (:1726)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00616680 (:1776)
    static ai_action* get_instance();
    virtual ~ambient_veh_despawn_action(); // vtbl+0x00 0x005DDD10

private:
    static ambient_veh_despawn_action the_instance; // 0x01DA0334
};

// size 0x34
class ambient_veh_flee_action : public ai_action
{
    ambient_veh_flee_action(); // 0x00604220 (.h:28)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006044C0 (.h:28)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00675510 (:1798)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00656BB0 (:1836)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00691390 (:1879)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x006166E0 (:1958)
    static ai_action* get_instance(); // 0x00BC2EA0 (.h:28)
    virtual ~ambient_veh_flee_action(); // vtbl+0x00 0x005DDD00

private:
    static ambient_veh_flee_action the_instance; // 0x01D75338
};

// size 0x34
class ambient_veh_stop_at_checkpoint_action : public ai_action
{
    ambient_veh_stop_at_checkpoint_action(); // 0x00604D90 (.h:32)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00605030 (.h:32)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006755C0 (:1983)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00656C80 (:2025)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00691500 (:2055)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00616740 (:2103)
    static ai_action* get_instance();
    virtual ~ambient_veh_stop_at_checkpoint_action(); // vtbl+0x00 0x005DDD40

private:
    static ambient_veh_stop_at_checkpoint_action the_instance; // 0x01D9F6E4
};

// size 0x34
class return_to_orders_action : public ai_action
{
    return_to_orders_action(); // 0x006053C0 (.h:44)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00605600 (.h:44)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006167A0 (:2128)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00656D30 (:2140)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00691640 (:2153)
    static ai_action* get_instance();
    virtual ~return_to_orders_action(); // vtbl+0x00 0x005DDDB0

private:
    static return_to_orders_action the_instance; // 0x01D9F718
};

// size 0x34
class reaction_to_honk : public ai_action
{
    reaction_to_honk(); // 0x00605640 (.h:47)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00605880 (.h:47)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E1F40 (:2191)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005E1F60 (:2200)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006916F0 (:2269)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005E22A0 (:2306)
    static ai_action* get_instance();
    virtual ~reaction_to_honk(); // vtbl+0x00 0x005DDDC0

private:
    static reaction_to_honk the_instance; // 0x01D72238
};

bool ai_should_use_panicked_goal(); // 0x005E19B0 (:382)

inline ai_goal* pedestrian_panicked_goal::get_instance()
{
    return &the_instance;
}
