// Original: volition/rfg/code/ai/self_preservation_actions.h
// Implementation: volition/rfg/code/ai/self_preservation_actions.cpp
//
// Self-preservation actions: diving and evading, fleeing buildings, debris, demolition areas and enemies (on foot or
// by vehicle), taking defensive cover, hiding in safe buildings, cowering, surrendering, panicking, burning, going into
// the safehouse. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"
#include "volition/vlib/code/math/vector3.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class cover_node;
struct nav_cell;
class navpoint;
class npc;
class object;

// size 0x18
struct cell_search
{
    bool operator==(const cell_search& a); // 0x005F1D60 (:139)
    cell_search(); // 0x005F1D70

    const nav_cell* cell; // 0x00
    vector center; // 0x04
    float dist_sqrd; // 0x10
    float closest_dist; // 0x14
};

// size 0x34
class dive_action : public ai_action
{
    dive_action(); // 0x006059D0 (.h:15)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00605CB0 (.h:15)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0066D1A0 (:1286)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0066D460 (:1385)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005F2240 (:1407)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00641DB0 (:1446)
    static ai_action* get_instance(); // 0x005DDF00 (.h:15)
    virtual ~dive_action(); // vtbl+0x00 0x005DDF10

private:
    static dive_action the_instance; // 0x01D72048
};

// size 0x34
class evade_action : public ai_action
{
    evade_action(); // 0x00605CF0 (.h:16)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00605FD0 (.h:16)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00641DE0 (:1458)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0066D4D0 (:1491)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0066D560 (:1505)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005F23B0 (:1520)
    static ai_action* get_instance(); // 0x005DDF20 (.h:16)
    virtual ~evade_action(); // vtbl+0x00 0x005DDF30

private:
    static evade_action the_instance; // 0x01DA05A0
};

// size 0x34
class stay_down_action : public ai_action
{
    stay_down_action(); // 0x00606010 (.h:17)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006062B0 (.h:17)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00641E80 (:1534)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00641F60 (:1571)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0066D5B0 (:1582)
    static ai_action* get_instance();
    virtual ~stay_down_action(); // vtbl+0x00 0x005DDF40

private:
    static stay_down_action the_instance; // 0x01D7568C
};

// size 0x34
class duck_into_building_action : public ai_action
{
    duck_into_building_action(); // 0x006062F0 (.h:18)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0066D6E0 (:1638)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00606590 (.h:18)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F23E0 (:1679)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00641FC0 (:1685)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006A9DE0 (:1695)
    static ai_action* get_instance();
    virtual ~duck_into_building_action(); // vtbl+0x00 0x005DDF50

private:
    static duck_into_building_action the_instance; // 0x01DDAD78
};

// size 0x34
class fall_back_to_human_action : public ai_action
{
    fall_back_to_human_action(); // 0x006089E0 (.h:38)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00608CE0 (.h:38)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F2410 (:1718)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006A9E70 (:1723)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006BB0C0 (:1734)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005F2420 (:1765)
    static ai_action* get_instance();
    virtual ~fall_back_to_human_action(); // vtbl+0x00 0x005DE080

private:
    static fall_back_to_human_action the_instance; // 0x01D74F3C
};

// size 0x34
class fled_building_rule : public ai_production_rule
{
    fled_building_rule(); // 0x00609820 (.h:53)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00609910 (.h:53)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F2440 (:1777)
public:
    static ai_action* get_instance();
    virtual ~fled_building_rule(); // vtbl+0x00 0x005DE0E0

private:
    static fled_building_rule the_instance; // 0x01D6861C
};

// size 0x34
class flee_building_action : public ai_action
{
    flee_building_action(); // 0x00607310 (.h:26)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0066D7D0 (:1782)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006075F0 (.h:26)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F2450 (:1814)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00642030 (:1821)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006A9EA0 (:1843)
    static ai_action* get_instance();
    virtual ~flee_building_action(); // vtbl+0x00 0x005DDFC0

private:
    static flee_building_action the_instance; // 0x01DC97CC
};

// size 0x34
class flee_to_defensive_cover_action : public ai_action
{
    flee_to_defensive_cover_action(); // 0x00607620 (.h:27)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00686EC0 (:1870)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006078C0 (.h:27)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F2490 (:1938)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00642130 (:1945)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006A9FD0 (:1966)
    static ai_action* get_instance(); // 0x005DDFD0 (.h:27)
    virtual ~flee_to_defensive_cover_action(); // vtbl+0x00 0x005DDFE0

private:
    static flee_to_defensive_cover_action the_instance; // 0x01D9F7D4
};

// size 0x34
class take_defensive_cover_action : public ai_action
{
    take_defensive_cover_action(); // 0x006078F0 (.h:28)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00607C10 (.h:28)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F24D0 (:2037)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005F2520 (:2051)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005F2580 (:2082)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x006421E0 (:2099)
    static ai_action* get_instance(); // 0x005DDFF0 (.h:28)
    virtual ~take_defensive_cover_action(); // vtbl+0x00 0x005DE000

private:
    static take_defensive_cover_action the_instance; // 0x01D7591C
};

// size 0x34
class leave_demolition_area_action : public ai_action
{
    leave_demolition_area_action(); // 0x00607C50 (.h:30)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00607EF0 (.h:30)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F2600 (:2114)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006871B0 (:2127)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006AA1F0 (:2157)
    static ai_action* get_instance();
    virtual ~leave_demolition_area_action(); // vtbl+0x00 0x005DE010

private:
    static leave_demolition_area_action the_instance; // 0x01D67EA0
};

// size 0x34
class leave_demolition_vehicle_action : public ai_action
{
    leave_demolition_vehicle_action(); // 0x00607F30 (.h:31)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00687220 (:2190)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00607F80 (.h:31)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F2620 (:2209)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00687270 (:2215)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006AA280 (:2252)
    static ai_action* get_instance();
    virtual ~leave_demolition_vehicle_action(); // vtbl+0x00 0x005DE020

private:
    static leave_demolition_vehicle_action the_instance; // 0x01DCA4CC
};

// size 0x34
class flee_to_safe_building_action : public ai_action
{
    flee_to_safe_building_action(); // 0x00607FB0 (.h:33)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00687330 (:2325)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00608290 (.h:33)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F2640 (:2422)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0066D890 (:2435)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006AA460 (:2490)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005F26D0 (:2523)
    static ai_action* get_instance(); // 0x005DE030 (.h:33)
    virtual ~flee_to_safe_building_action(); // vtbl+0x00 0x005DE040

private:
    static flee_to_safe_building_action the_instance; // 0x01DCA75C
};

// size 0x34
class hide_in_safe_building_action : public ai_action
{
    hide_in_safe_building_action(); // 0x006082C0 (.h:34)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00608460 (.h:34)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F26E0 (:2538)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0066DA20 (:2561)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0066DA80 (:2590)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00642210 (:2606)
    static ai_action* get_instance();
    virtual ~hide_in_safe_building_action(); // vtbl+0x00 0x005DE050

private:
    static hide_in_safe_building_action the_instance; // 0x01D72010
};

// size 0x34
class cower_action : public ai_action
{
    cower_action(); // 0x00608D20 (.h:40)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00608F50 (.h:40)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F2740 (:2620)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005F2750 (:2625)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005F27A0 (:2642)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00642240 (:2647)
    static ai_action* get_instance();
    virtual ~cower_action(); // vtbl+0x00 0x005DE090

private:
    static cower_action the_instance; // 0x01DCA0B8
};

// size 0x34
class cower_incoming_action : public ai_action
{
    cower_incoming_action(); // 0x00608F90 (.h:41)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006091C0 (.h:41)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F27B0 (:2659)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005F27E0 (:2675)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0066DAA0 (:2753)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00642270 (:2813)
    static ai_action* get_instance();
    virtual ~cower_incoming_action(); // vtbl+0x00 0x005DE0A0

private:
    static cower_incoming_action the_instance; // 0x01DDB4B4
};

// size 0x34
class surrender_action : public ai_action
{
    surrender_action(); // 0x00609200 (.h:42)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00609490 (.h:42)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F2A60 (:2828)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006422C0 (:2833)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005F2A70 (:2849)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005F2A90 (:2858)
    static ai_action* get_instance();
    virtual ~surrender_action(); // vtbl+0x00 0x005DE0B0

private:
    static surrender_action the_instance; // 0x01D71E80
};

// size 0x34
class fetal_position_action : public ai_action
{
    fetal_position_action(); // 0x006094D0 (.h:44)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00609520 (.h:44)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F2AA0 (:2869)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005F2AB0 (:2874)
    static ai_action* get_instance();
    virtual ~fetal_position_action(); // vtbl+0x00 0x005DE0C0

private:
    static fetal_position_action the_instance; // 0x01DC9E68
};

// size 0x34
class run_around_panicked_action : public ai_action
{
    run_around_panicked_action(); // 0x00606660 (.h:22)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00606900 (.h:22)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F2B50 (:2894)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005F2B60 (:2899)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00642350 (:2918)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x006425C0 (:2976)
    static ai_action* get_instance(); // 0x005DDF70 (.h:22)
    virtual ~run_around_panicked_action(); // vtbl+0x00 0x005DDF80

private:
    static run_around_panicked_action the_instance; // 0x01DC9720
};

// size 0x34
class on_fire_roll_action : public ai_action
{
    on_fire_roll_action(); // 0x00606940 (.h:23)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00606B70 (.h:23)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F2BC0 (:2997)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005F2C00 (:3011)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00642640 (:3042)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005F2C80 (:3077)
    static ai_action* get_instance();
    virtual ~on_fire_roll_action(); // vtbl+0x00 0x005DDF90

private:
    static on_fire_roll_action the_instance; // 0x01DDB620
};

// size 0x34
class on_fire_panic_action : public ai_action
{
    on_fire_panic_action(); // 0x00606BB0 (.h:24)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00606E50 (.h:24)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F2CE0 (:3095)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005F2D10 (:3106)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00642700 (:3131)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005F2D70 (:3193)
    static ai_action* get_instance();
    virtual ~on_fire_panic_action(); // vtbl+0x00 0x005DDFA0

private:
    static on_fire_panic_action the_instance; // 0x01D72508
};

// size 0x34
class flee_action : public ai_action
{
    flee_action(); // 0x00606E90 (.h:25)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006072D0 (.h:25)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F2DD0 (:3210)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005F2DE0 (:3215)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006AA5D0 (:3252)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00642900 (:3431)
    static ai_action* get_instance(); // 0x00AE9FF0 (.h:25)
    virtual ~flee_action(); // vtbl+0x00 0x005DDFB0

private:
    static flee_action the_instance; // 0x01D72204
};

// size 0x34
class flee_in_vehicle_action : public ai_action
{
    flee_in_vehicle_action(); // 0x006084A0 (.h:36)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00608720 (.h:36)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006AAC40 (:3450)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006875A0 (:3467)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006AACB0 (:3515)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005F2EB0 (:3599)
    static ai_action* get_instance(); // 0x00BC75E0 (.h:36)
    virtual ~flee_in_vehicle_action(); // vtbl+0x00 0x005DE060

private:
    static flee_in_vehicle_action the_instance; // 0x01DA056C
};

// size 0x34
class flee_in_vehicle_xtreme_action : public ai_action
{
    flee_in_vehicle_xtreme_action(); // 0x00608760 (.h:37)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006089A0 (.h:37)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006AAF40 (:3617)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00687780 (:3639)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006AAFB0 (:3700)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005F2EE0 (:3797)
    static ai_action* get_instance(); // 0x00BC75F0 (.h:37)
    virtual ~flee_in_vehicle_xtreme_action(); // vtbl+0x00 0x005DE070

private:
    static flee_in_vehicle_xtreme_action the_instance; // 0x01D7536C
};

// size 0x34
class go_into_safehouse_action : public ai_action
{
    go_into_safehouse_action(); // 0x00609560 (.h:46)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x005F2F10 (:3809)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006097F0 (.h:46)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F2F50 (:3833)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006AB2D0 (:3841)
    static ai_action* get_instance();
    virtual ~go_into_safehouse_action(); // vtbl+0x00 0x005DE0D0

private:
    static go_into_safehouse_action the_instance; // 0x01DDB974
};

// size 0x34
class flee_to_defensive_cover_rule : public ai_production_rule
{
    flee_to_defensive_cover_rule(); // 0x006065C0 (.h:20)
public:
    static ai_action* get_instance();
    virtual ~flee_to_defensive_cover_rule(); // vtbl+0x00 0x005DDF60

private:
    static flee_to_defensive_cover_rule the_instance; // 0x01DC9ED0
};

bool flee_navpoint_is_safe(const navpoint* new_navp, const npc* np, const object* flee_objp); // 0x005F1DE0 (:646)
bool flee_cover_node_is_safe(const cover_node* new_cnp, const npc* np, const object* flee_objp); // 0x005F1FE0 (:706)
void stop_fleeing_to_safe_building(const npc* np); // 0x00641A90 (:771)
void stop_hiding_in_defensive_cover(const npc* np); // 0x00641AB0 (:779)
void ai_maybe_play_cower_to_stand_animation(npc* np, const ai_action* next_action); // 0x00641AD0 (:1251)
