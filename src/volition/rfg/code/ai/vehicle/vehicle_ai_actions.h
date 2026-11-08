// Original: volition/rfg/code/ai/vehicle/vehicle_ai_actions.h
// Implementation: volition/rfg/code/ai/vehicle/vehicle_ai_actions.cpp
//
// Vehicle actions: going to and entering vehicles, getting out, driving or flying to places, chasing targets (with
// fixed, manned or flying turrets), smashing buildings, manning turrets, passengers, tanks, flyers, fodder, getting
// back on the nav mesh, pulling off the road. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"
#include "volition/vlib/code/time/timestamp.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class human;
class npc;
class vehicle;

// size 0x34
class goto_vehicle_action : public ai_action
{
    // human_combat.cpp reads it (friends are not recorded in rfg.pdb).
    friend bool human_combat_can_play_large_flinch_or_stumble(const human* hp);
    goto_vehicle_action(); // 0x00622980 (.h:13)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006463F0 (:425)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00622C20 (.h:13)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F4B70 (:442)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0068B850 (:448)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006AC530 (:499)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00646480 (:613)
    static ai_action* get_instance(); // 0x005E4060 (.h:13)
    virtual ~goto_vehicle_action(); // vtbl+0x00 0x005E4070

private:
    static goto_vehicle_action the_instance; // 0x01DDAEA4
};

// size 0x34
class enter_vehicle_action : public ai_action
{
    // human_combat.cpp reads it (friends are not recorded in rfg.pdb).
    friend bool human_combat_can_play_large_flinch_or_stumble(const human* hp);
    enter_vehicle_action(); // 0x00622C50 (.h:14)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00622F50 (.h:14)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F4B80 (:682)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0068BA00 (:705)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0068BB10 (:759)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00646510 (:785)
    static ai_action* get_instance(); // 0x005E4080 (.h:14)
    virtual ~enter_vehicle_action(); // vtbl+0x00 0x005E4090

private:
    static enter_vehicle_action the_instance; // 0x01D684AC
};

// size 0x34
class get_out_of_vehicle_action : public ai_action
{
    get_out_of_vehicle_action(); // 0x00622F90 (.h:15)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00622FE0 (.h:15)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0066FAB0 (:845)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0066FD90 (:996)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00646650 (:1071)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005F4BD0 (:1178)
    static ai_action* get_instance(); // 0x005E40A0 (.h:15)
    virtual ~get_out_of_vehicle_action(); // vtbl+0x00 0x005E40B0

private:
    static get_out_of_vehicle_action the_instance; // 0x01DC9894
};

// size 0x34
class goto_with_flyer_action : public ai_action
{
    goto_with_flyer_action(); // 0x006230B0 (.h:17)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00623110 (.h:17)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00646830 (:1197)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00646870 (:1210)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006AC8D0 (:1263)
    static ai_action* get_instance();
    virtual ~goto_with_flyer_action(); // vtbl+0x00 0x005E40D0

private:
    static goto_with_flyer_action the_instance; // 0x01D75798
};

// size 0x34
class goto_with_vehicle_action : public ai_action
{
    goto_with_vehicle_action(); // 0x00623020 (.h:16)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0068BB90 (:1417)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00623080 (.h:16)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F4E30 (:1494)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0066FF90 (:1544)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006AC980 (:1586)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005F4F30 (:1654)
    static ai_action* get_instance();
    virtual ~goto_with_vehicle_action(); // vtbl+0x00 0x005E40C0

private:
    static goto_with_vehicle_action the_instance; // 0x01DA0624
};

// size 0x34
class chase_target_in_vehicle_action : public ai_action
{
    chase_target_in_vehicle_action(); // 0x00623150 (.h:18)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0068BF50 (:1667)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006231B0 (.h:18)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F4F50 (:1716)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00646AB0 (:1725)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006ACBD0 (:1784)
    static ai_action* get_instance(); // 0x00BC59E0 (.h:18)
    virtual ~chase_target_in_vehicle_action(); // vtbl+0x00 0x005E40E0

private:
    static chase_target_in_vehicle_action the_instance; // 0x01DDB02C
};

// size 0x34
class chase_target_hold_position_action : public ai_action
{
    chase_target_hold_position_action(); // 0x006231E0 (.h:19)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0068C140 (:2128)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00623240 (.h:19)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F4F80 (:2206)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00646C80 (:2215)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006ADD40 (:2253)
    static ai_action* get_instance();
    virtual ~chase_target_hold_position_action(); // vtbl+0x00 0x005E40F0

private:
    static chase_target_hold_position_action the_instance; // 0x01DA3E44
};

// size 0x34
class chase_target_with_fixed_turret : public ai_action
{
    chase_target_with_fixed_turret(); // 0x00623270 (.h:20)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0068C330 (:2320)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006232D0 (.h:20)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F4FB0 (:2349)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005F4FE0 (:2358)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006ADF60 (:2374)
    static ai_action* get_instance();
    virtual ~chase_target_with_fixed_turret(); // vtbl+0x00 0x005E4100

private:
    static chase_target_with_fixed_turret the_instance; // 0x01D7500C
};

// size 0x34
class chase_target_with_manned_turret : public ai_action
{
    chase_target_with_manned_turret(); // 0x00623300 (.h:21)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0068C450 (:2541)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00623360 (.h:21)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F5060 (:2581)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005F5090 (:2590)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006AE8E0 (:2627)
    static ai_action* get_instance();
    virtual ~chase_target_with_manned_turret(); // vtbl+0x00 0x005E4110

private:
    static chase_target_with_manned_turret the_instance; // 0x01DDAFA8
};

// size 0x34
class chase_target_with_flying_turret : public ai_action
{
    chase_target_with_flying_turret(); // 0x00623390 (.h:22)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0068C590 (:3114)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006233F0 (.h:22)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F5210 (:3142)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005F5240 (:3151)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B04A0 (:3178)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00646D80 (:3789)
    static ai_action* get_instance();
    virtual ~chase_target_with_flying_turret(); // vtbl+0x00 0x005E4120

private:
    static chase_target_with_flying_turret the_instance; // 0x01DDB79C
};

// size 0x34
class smash_building_action : public ai_action
{
    smash_building_action(); // 0x00623AA0 (.h:43)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x005F5320 (:3804)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00623B00 (.h:43)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F5330 (:3824)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005F5340 (:3831)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005F5350 (:3858)
    static ai_action* get_instance();
    virtual ~smash_building_action(); // vtbl+0x00 0x005E41E0

private:
    static smash_building_action the_instance; // 0x01D9F76C
};

// size 0x34
class drive_for_manned_turret_action : public ai_action
{
    drive_for_manned_turret_action(); // 0x00623510 (.h:24)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0068C6B0 (:4077)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00623570 (.h:24)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F5550 (:4125)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005F5580 (:4133)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005F55D0 (:4158)
    static ai_action* get_instance();
    virtual ~drive_for_manned_turret_action(); // vtbl+0x00 0x005E4140

private:
    static drive_for_manned_turret_action the_instance; // 0x01DCA648
};

// size 0x34
class man_turret_in_vehicle_action : public ai_action
{
    man_turret_in_vehicle_action(); // 0x00623420 (.h:23)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0068C760 (:4191)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006234E0 (.h:23)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F5620 (:4244)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005F5650 (:4251)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B2940 (:4280)
    static ai_action* get_instance();
    virtual ~man_turret_in_vehicle_action(); // vtbl+0x00 0x005E4130

private:
    static man_turret_in_vehicle_action the_instance; // 0x01DCA500
};

// size 0x34
class wait_passenger_clear_action : public ai_action
{
    wait_passenger_clear_action(); // 0x00623960 (.h:41)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006239C0 (.h:41)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00646DC0 (:4343)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006700C0 (:4365)
    static ai_action* get_instance();
    virtual ~wait_passenger_clear_action(); // vtbl+0x00 0x005E41C0

private:
    static wait_passenger_clear_action the_instance; // 0x01D72348
};

// size 0x34
class wait_for_passengers_action : public ai_action
{
    wait_for_passengers_action(); // 0x006238C0 (.h:40)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00623920 (.h:40)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F56F0 (:4456)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005F5700 (:4461)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B2AE0 (:4473)
    static ai_action* get_instance(); // 0x005E41A0 (.h:40)
    virtual ~wait_for_passengers_action(); // vtbl+0x00 0x005E41B0

private:
    static wait_for_passengers_action the_instance; // 0x01D7237C
};

// size 0x34
class drop_off_passengers_action : public ai_action
{
    drop_off_passengers_action(); // 0x00623A00 (.h:42)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00623A60 (.h:42)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0068C920 (:4496)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005F5770 (:4512)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0068C980 (:4527)
    static ai_action* get_instance();
    virtual ~drop_off_passengers_action(); // vtbl+0x00 0x005E41D0

private:
    static drop_off_passengers_action the_instance; // 0x01DA61DC
};

// size 0x34
class acquire_visual_in_vehicle_action : public ai_action
{
    acquire_visual_in_vehicle_action(); // 0x00623B30 (.h:45)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00623B90 (.h:45)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00670280 (:4604)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005F57C0 (:4632)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B2B60 (:4666)
    static ai_action* get_instance();
    virtual ~acquire_visual_in_vehicle_action(); // vtbl+0x00 0x005E41F0

private:
    static acquire_visual_in_vehicle_action the_instance; // 0x01DCA858
};

// size 0x34
class pwn_in_tank_action : public ai_action
{
    pwn_in_tank_action(); // 0x00623BD0 (.h:46)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00623C30 (.h:46)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00670310 (:4946)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005F5880 (:4979)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B3510 (:5018)
    static ai_action* get_instance();
    virtual ~pwn_in_tank_action(); // vtbl+0x00 0x005E4200

private:
    static pwn_in_tank_action the_instance; // 0x01D68080
};

// size 0x34
class retreat_def_perimeter_vehicle_rule : public ai_production_rule
{
    retreat_def_perimeter_vehicle_rule(); // 0x00623C70 (.h:48)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00623CE0 (.h:48)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0068CC60 (:5220)
public:
    static ai_action* get_instance();
    virtual ~retreat_def_perimeter_vehicle_rule(); // vtbl+0x00 0x005E4210

private:
    static retreat_def_perimeter_vehicle_rule the_instance; // 0x01DDADFC
};

// size 0x34
class man_flyer_action : public ai_action
{
    man_flyer_action(); // 0x006235A0 (.h:27)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00623600 (.h:27)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F5940 (:5272)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00646E20 (:5277)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00646E60 (:5287)
    static ai_action* get_instance();
    virtual ~man_flyer_action(); // vtbl+0x00 0x005E4150

private:
    static man_flyer_action the_instance; // 0x01DC9D10
};

// size 0x34
class loiter_flyer_action : public ai_action
{
    loiter_flyer_action(); // 0x00623640 (.h:28)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006236A0 (.h:28)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00646FB0 (:5322)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00646FF0 (:5332)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006470B0 (:5357)
    static ai_action* get_instance();
    virtual ~loiter_flyer_action(); // vtbl+0x00 0x005E4160

private:
    static loiter_flyer_action the_instance; // 0x01DC9C40
};

// size 0x34
class fodder_action : public ai_action
{
    fodder_action(); // 0x00623820 (.h:37)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00623880 (.h:37)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0068CD50 (:5402)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0068CDA0 (:5413)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0068CE70 (:5458)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x006472C0 (:5442)
    static ai_action* get_instance(); // 0x0075DA40 (.h:37)
    virtual ~fodder_action(); // vtbl+0x00 0x005E4190

private:
    static fodder_action the_instance; // 0x01DDAF0C
};

// size 0x34
class get_back_on_navmesh_action : public ai_action
{
    get_back_on_navmesh_action(); // 0x006236E0 (.h:31)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00623740 (.h:31)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F5950 (:5838)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00647310 (:5843)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B3D10 (:5876)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x006473E0 (:6207)
    static ai_action* get_instance();
    virtual ~get_back_on_navmesh_action(); // vtbl+0x00 0x005E4170

private:
    static get_back_on_navmesh_action the_instance; // 0x01DC9930
};

// size 0x34
class pull_off_road_action : public ai_action
{
    pull_off_road_action(); // 0x00623780 (.h:34)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006237E0 (.h:34)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F5960 (:6226)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00647430 (:6231)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006703B0 (:6258)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x006474C0 (:6620)
    static ai_action* get_instance();
    virtual ~pull_off_road_action(); // vtbl+0x00 0x005E4180

private:
    static pull_off_road_action the_instance; // 0x01DCA050
};

void unused_dcf_ai_continuous_attack_tweak(); // 0x005F4380 (:217)
ai_results enter_vehicle_common(npc* np); // 0x0068B940 (:650)
extern timestamp Yell_vehicle_timeout; // 0x016424E4
bool should_stop_man_turret_in_vehicle(const npc* np, const vehicle* vp, bool& immediate); // 0x005F5360 (:4009)
