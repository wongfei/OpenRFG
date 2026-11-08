// Original: volition/rfg/code/ai/threaten_actions.h
// Implementation: volition/rfg/code/ai/threaten_actions.cpp
//
// Threaten actions: EDF confronting, warning, beating, executing and checking the IDs of civilians, threatening from
// turrets, and taunting players, civilians and enemies. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class human;
class npc;

// size 0x34
class confront_civilian_kill_action : public ai_action
{
    confront_civilian_kill_action(); // 0x0061A280 (.h:20)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0061A510 (.h:20)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006892F0 (:961)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006BF710 (:981)
    static ai_action* get_instance(); // 0x005E2C80 (.h:20)
    virtual ~confront_civilian_kill_action(); // vtbl+0x00 0x005E2C90

private:
    static confront_civilian_kill_action the_instance; // 0x01DCA814
};

// size 0x34
class confront_civilian_guard_action : public ai_action
{
    confront_civilian_guard_action(); // 0x00619410 (.h:13)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00619650 (.h:13)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F3BA0 (:997)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006BF7C0 (:1006)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006BF830 (:1022)
    static ai_action* get_instance();
    virtual ~confront_civilian_guard_action(); // vtbl+0x00 0x005E2C20

private:
    static confront_civilian_guard_action the_instance; // 0x01D72570
};

// size 0x34
class confront_civilian_wait_action : public ai_action
{
    confront_civilian_wait_action(); // 0x00619690 (.h:14)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00619920 (.h:14)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00689340 (:1045)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006BF910 (:1056)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006BFA70 (:1082)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005F3BC0 (:1097)
    static ai_action* get_instance();
    virtual ~confront_civilian_wait_action(); // vtbl+0x00 0x005E2C30

private:
    static confront_civilian_wait_action the_instance; // 0x01DDB4E8
};

// size 0x34
class confront_civilian_accompany_action : public ai_action
{
    confront_civilian_accompany_action(); // 0x00619960 (.h:15)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006199C0 (.h:15)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F3BE0 (:1109)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006BFB90 (:1118)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006BFCB0 (:1151)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x006BFE10 (:1211)
    static ai_action* get_instance();
    virtual ~confront_civilian_accompany_action(); // vtbl+0x00 0x005E2C40

private:
    static confront_civilian_accompany_action the_instance; // 0x01D685A8
};

// size 0x34
class confront_civilian_first_moving_action : public ai_action
{
    confront_civilian_first_moving_action(); // 0x00619A00 (.h:16)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00619CA0 (.h:16)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006BB1C0 (:1229)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006BFE30 (:1251)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006BFEB0 (:1272)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00689390 (:1375)
    static ai_action* get_instance();
    virtual ~confront_civilian_first_moving_action(); // vtbl+0x00 0x005E2C50

private:
    static confront_civilian_first_moving_action the_instance; // 0x01D750F8
};

// size 0x34
class confront_civilian_first_warning_action : public ai_action
{
    confront_civilian_first_warning_action(); // 0x00619CE0 (.h:17)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00619F70 (.h:17)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00689430 (:1400)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006C01F0 (:1414)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006C0330 (:1454)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x006894C0 (:1475)
    static ai_action* get_instance();
    virtual ~confront_civilian_first_warning_action(); // vtbl+0x00 0x005E2C60

private:
    static confront_civilian_first_warning_action the_instance; // 0x01D68340
};

// size 0x34
class confront_civilian_warning_shot_action : public ai_action
{
    confront_civilian_warning_shot_action(); // 0x00619FB0 (.h:19)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0061A240 (.h:19)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00689560 (:1598)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006C0440 (:1612)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006C0560 (:1660)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x006895F0 (:1714)
    static ai_action* get_instance();
    virtual ~confront_civilian_warning_shot_action(); // vtbl+0x00 0x005E2C70

private:
    static confront_civilian_warning_shot_action the_instance; // 0x01D68160
};

// size 0x34
class confront_civilian_dont_goto_rule : public ai_production_rule
{
    confront_civilian_dont_goto_rule(); // 0x0061A830 (.h:22)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0061AB10 (.h:22)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F3C00 (:1736)
public:
    static ai_action* get_instance();
    virtual ~confront_civilian_dont_goto_rule(); // vtbl+0x00 0x005E2CB0

private:
    static confront_civilian_dont_goto_rule the_instance; // 0x01DDB1F8
};

// size 0x34
class confront_civilian_goto_action : public ai_action
{
    confront_civilian_goto_action(); // 0x0061A550 (.h:21)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0061A7F0 (.h:21)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006BB280 (:1741)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006C06F0 (:1770)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006C0760 (:1788)
    static ai_action* get_instance();
    virtual ~confront_civilian_goto_action(); // vtbl+0x00 0x005E2CA0

private:
    static confront_civilian_goto_action the_instance; // 0x01D7575C
};

// size 0x34
class threaten_civilian_from_turret_action : public ai_action
{
    threaten_civilian_from_turret_action(); // 0x0061ABF0 (.h:26)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0061AC40 (.h:26)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F3C10 (:1857)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006C0950 (:1868)
    static ai_action* get_instance();
    virtual ~threaten_civilian_from_turret_action(); // vtbl+0x00 0x005E2CD0

private:
    static threaten_civilian_from_turret_action the_instance; // 0x01DDB260
};

// size 0x34
class threaten_civilian_default_action : public ai_action
{
    threaten_civilian_default_action(); // 0x0061AC80 (.h:27)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0061AF20 (.h:27)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F3C30 (:1912)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005F3C40 (:1917)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00689680 (:1935)
    static ai_action* get_instance(); // 0x005E2CE0 (.h:27)
    virtual ~threaten_civilian_default_action(); // vtbl+0x00 0x005E2CF0

private:
    static threaten_civilian_default_action the_instance; // 0x01D6877C
};

// size 0x34
class threaten_civilian_watch_action : public ai_action
{
    threaten_civilian_watch_action(); // 0x0061AF60 (.h:28)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0061B200 (.h:28)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F3C80 (:1990)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00689970 (:1995)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00644250 (:2099)
    static ai_action* get_instance();
    virtual ~threaten_civilian_watch_action(); // vtbl+0x00 0x005E2D00

private:
    static threaten_civilian_watch_action the_instance; // 0x01D75290
};

// size 0x34
class taunt_player_action : public ai_action
{
    taunt_player_action(); // 0x0061CE20 (.h:43)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x005F3C90 (:2122)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0061D120 (.h:43)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F3D20 (:2141)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00644340 (:2147)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005F3D40 (:2177)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00644510 (:2186)
    static ai_action* get_instance();
    virtual ~taunt_player_action(); // vtbl+0x00 0x005E2DE0

private:
    static taunt_player_action the_instance; // 0x01DDAF74
};

// size 0x34
class taunt_civilian_action : public ai_action
{
    taunt_civilian_action(); // 0x0061D150 (.h:44)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0061D3F0 (.h:44)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F3D70 (:2200)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0066E5F0 (:2210)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00644540 (:2241)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00644600 (:2268)
    static ai_action* get_instance();
    virtual ~taunt_civilian_action(); // vtbl+0x00 0x005E2DF0

private:
    static taunt_civilian_action the_instance; // 0x01D758DC
};

// size 0x34
class taunt_enemy_running_action : public ai_action
{
    taunt_enemy_running_action(); // 0x0061D430 (.h:45)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0061D6D0 (.h:45)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F3DA0 (:2287)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00644650 (:2302)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00644790 (:2329)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00644800 (:2356)
    static ai_action* get_instance();
    virtual ~taunt_enemy_running_action(); // vtbl+0x00 0x005E2E00

private:
    static taunt_enemy_running_action the_instance; // 0x01DCA790
};

// size 0x34
class confront_civilian_beatdown_rule : public ai_production_rule
{
    confront_civilian_beatdown_rule(); // 0x0061AB40 (.h:23)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0061ABC0 (.h:23)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006BB3E0 (:2375)
public:
    static ai_action* get_instance();
    virtual ~confront_civilian_beatdown_rule(); // vtbl+0x00 0x005E2CC0

private:
    static confront_civilian_beatdown_rule the_instance; // 0x01D724D4
};

// size 0x34
class beat_civilian_action : public ai_action
{
    beat_civilian_action(); // 0x0061B240 (.h:29)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00644830 (:2449)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0061B540 (.h:29)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F3EA0 (:2469)
    virtual ai_results enter(npc* np, const ai_action* prev_action) const; // vtbl+0x10 0x0068A1B0 (:2475)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006448A0 (:2521)
    static ai_action* get_instance(); // 0x005E2D10 (.h:29)
    virtual ~beat_civilian_action(); // vtbl+0x00 0x005E2D20

private:
    static beat_civilian_action the_instance; // 0x01D7589C
};

// size 0x34
class execute_civilian_action : public ai_action
{
    execute_civilian_action(); // 0x0061C010 (.h:35)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006449F0 (:2586)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0061C360 (.h:35)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F3FD0 (:2650)
    virtual ai_results enter(npc* np, const ai_action* prev_action) const; // vtbl+0x10 0x0068A350 (:2612)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006C0A90 (:2655)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00644C10 (:2765)
    static ai_action* get_instance(); // 0x005E2D70 (.h:35)
    virtual ~execute_civilian_action(); // vtbl+0x00 0x005E2D80

private:
    static execute_civilian_action the_instance; // 0x01DDAFDC
};

// size 0x34
class move_to_execution_position_action : public ai_action
{
    move_to_execution_position_action(); // 0x0061BC90 (.h:34)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00644CA0 (:2787)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0061BFE0 (.h:34)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F3FE0 (:2814)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0068A530 (:2819)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006C0E60 (:2837)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00644DB0 (:2877)
    static ai_action* get_instance(); // 0x005E2D50 (.h:34)
    virtual ~move_to_execution_position_action(); // vtbl+0x00 0x005E2D60

private:
    static move_to_execution_position_action the_instance; // 0x01D72278
};

// size 0x34
class order_civilian_to_knees_action : public ai_action
{
    order_civilian_to_knees_action(); // 0x0061B900 (.h:33)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0061BC50 (.h:33)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00644E20 (:2898)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0068A5A0 (:2924)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006C1090 (:2955)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00644EC0 (:3013)
    static ai_action* get_instance();
    virtual ~order_civilian_to_knees_action(); // vtbl+0x00 0x005E2D40

private:
    static order_civilian_to_knees_action the_instance; // 0x01D75510
};

// size 0x34
class demand_civilian_id_action : public ai_action
{
    demand_civilian_id_action(); // 0x0061C390 (.h:38)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0061C6E0 (.h:38)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00644F40 (:3046)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0068A6C0 (:3073)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006C1280 (:3110)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00644FE0 (:3178)
    static ai_action* get_instance();
    virtual ~demand_civilian_id_action(); // vtbl+0x00 0x005E2D90

private:
    static demand_civilian_id_action the_instance; // 0x01DA3EE8
};

// size 0x34
class move_to_check_civilian_id_action : public ai_action
{
    move_to_check_civilian_id_action(); // 0x0061C720 (.h:39)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00645060 (:3201)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0061CA70 (.h:39)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F40D0 (:3222)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0068A7F0 (:3227)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006C1540 (:3259)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00645130 (:3299)
    static ai_action* get_instance(); // 0x005E2DA0 (.h:39)
    virtual ~move_to_check_civilian_id_action(); // vtbl+0x00 0x005E2DB0

private:
    static move_to_check_civilian_id_action the_instance; // 0x01DA3E10
};

// size 0x34
class check_civilian_id_action : public ai_action
{
    check_civilian_id_action(); // 0x0061CAA0 (.h:40)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006451A0 (:3321)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0061CDF0 (.h:40)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F40E0 (:3392)
    virtual ai_results enter(npc* np, const ai_action* prev_action) const; // vtbl+0x10 0x0068A980 (:3347)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006C16F0 (:3397)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x006C1AB0 (:3507)
    static ai_action* get_instance(); // 0x005E2DC0 (.h:40)
    virtual ~check_civilian_id_action(); // vtbl+0x00 0x005E2DD0

private:
    static check_civilian_id_action the_instance; // 0x01DC9490
};

// size 0x34
class threaten_civilian_warning_shot_action : public ai_action
{
    threaten_civilian_warning_shot_action(); // 0x0061B570 (.h:30)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0061B8C0 (.h:30)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F40F0 (:3550)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00645340 (:3555)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006C1C00 (:3579)
    static ai_action* get_instance();
    virtual ~threaten_civilian_warning_shot_action(); // vtbl+0x00 0x005E2D30

private:
    static threaten_civilian_warning_shot_action the_instance; // 0x01D75868
};

void ai_taunt_enemy_reset_delay(const npc* np); // 0x005F3B30 (:672)
bool ai_can_taunt_enemy(const npc* np); // 0x005F3B60 (:686)
bool ai_threaten_target_is_valid(const npc* np, const human* target); // 0x00643D50 (:694)
human* ai_threaten_pick_target(const npc* np); // 0x00689170 (:736)
human* ai_threaten_pick_watch_target(const npc* np, npc** ally); // 0x00643E80 (:787)
human* ai_taunt_unfriendly_pick_target(const npc* np); // 0x00644000 (:852)
human* ai_taunt_enemy_pick_target(const npc* np); // 0x00644150 (:914)
