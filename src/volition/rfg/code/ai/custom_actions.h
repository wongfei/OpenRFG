// Original: volition/rfg/code/ai/custom_actions.h
// Implementation: volition/rfg/code/ai/custom_actions.cpp
//
// Custom and scripted behaviours: custom animations and behaviours started by missions and activities, terrorizing,
// watching or following the player, trapped/rescued hostages, activity intros, marauder lookouts and saboteurs, the
// old coot, tied-up hostages, vehicle drop-offs and tech upgrade responses. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
#pragma once

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_types.h"
#include "volition/rfg/code/ai/goals.h"

struct ai_action_setup_data;
struct ai_goal_setup_data;
class ai_plan_conditions;
struct ai_planner_search_node;
class human;
class npc;
class object;
class vector;

// size 0x1C
class custom_animation_goal : public ai_goal
{
    custom_animation_goal(); // 0x005FF8E0 (.h:16)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005E8400 (:589)
    static ai_goal* get_instance();
    virtual ~custom_animation_goal(); // vtbl+0x00 0x005DD910

private:
    static custom_animation_goal the_instance; // 0x016426F4
};

// size 0x1C
class custom_behavior_goal : public ai_goal
{
    custom_behavior_goal(); // 0x005FF980 (.h:17)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005E8430 (:602)
    static ai_goal* get_instance();
    virtual ~custom_behavior_goal(); // vtbl+0x00 0x005DD920

private:
    static custom_behavior_goal the_instance; // 0x01642710
};

// size 0x1C
class terrorize_goal : public ai_goal
{
    terrorize_goal(); // 0x005FFA20 (.h:18)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0067AB80 (:681)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0065CD90 (:691)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x005E8530 (:721)
    static ai_goal* get_instance();
    virtual ~terrorize_goal(); // vtbl+0x00 0x005DD930

private:
    static terrorize_goal the_instance; // 0x0164272C
};

// size 0x34
class custom_watch_player_action : public ai_action
{
    custom_watch_player_action(); // 0x006029B0 (.h:62)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00602C50 (.h:62)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E8550 (:731)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069CCF0 (:748)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069CD40 (:768)
    static ai_action* get_instance();
    virtual ~custom_watch_player_action(); // vtbl+0x00 0x005DDA70

private:
    static custom_watch_player_action the_instance; // 0x01D68200
};

// size 0x34
class custom_animation_action : public ai_action
{
    custom_animation_action(); // 0x005FFAC0 (.h:21)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x005FFD60 (.h:21)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E8590 (:804)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069CEC0 (:809)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E85A0 (:843)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005E8600 (:860)
    static ai_action* get_instance();
    virtual ~custom_animation_action(); // vtbl+0x00 0x005DD940

private:
    static custom_animation_action the_instance; // 0x01D67DD0
};

// size 0x34
class custom_trapped_action : public ai_action
{
    custom_trapped_action(); // 0x005FFDA0 (.h:25)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00600040 (.h:25)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E8610 (:868)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005E8640 (:878)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E8690 (:889)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005E86A0 (:896)
    static ai_action* get_instance(); // 0x00A05FC0 (.h:25)
    virtual ~custom_trapped_action(); // vtbl+0x00 0x005DD950

private:
    // human.cpp reads it (friends are not recorded in rfg.pdb).
    friend bool human_can_currently_be_pushed(const human* hp);
    static custom_trapped_action the_instance; // 0x01D723B0
};

// size 0x34
class custom_rescued_action : public ai_action
{
    custom_rescued_action(); // 0x00600080 (.h:26)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0067ABD0 (:922)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00600380 (.h:26)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E8720 (:936)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0062E330 (:942)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006B6640 (:947)
    static ai_action* get_instance(); // 0x005DD960 (.h:26)
    virtual ~custom_rescued_action(); // vtbl+0x00 0x005DD970

private:
    static custom_rescued_action the_instance; // 0x01DC9AB0
};

// size 0x34
class custom_activity_intro_action : public ai_action
{
    custom_activity_intro_action(); // 0x00600970 (.h:31)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00600C10 (.h:31)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E8730 (:955)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069D070 (:976)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069D0E0 (:1009)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x0062E380 (:1032)
    static ai_action* get_instance();
    virtual ~custom_activity_intro_action(); // vtbl+0x00 0x005DD9A0

private:
    static custom_activity_intro_action the_instance; // 0x01DA0704
};

// size 0x34
class custom_turn_animate_action : public ai_action
{
    custom_turn_animate_action(); // 0x00600690 (.h:30)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00600930 (.h:30)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E8770 (:1058)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069D160 (:1068)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E8790 (:1108)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x0062E400 (:1176)
    static ai_action* get_instance();
    virtual ~custom_turn_animate_action(); // vtbl+0x00 0x005DD990

private:
    static custom_turn_animate_action the_instance; // 0x01D72674
};

// size 0x34
class custom_follow_wounded : public ai_action
{
    custom_follow_wounded(); // 0x00600C50 (.h:34)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00600EF0 (.h:34)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E8930 (:1203)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005E8950 (:1213)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069D2C0 (:1230)
    static ai_action* get_instance();
    virtual ~custom_follow_wounded(); // vtbl+0x00 0x005DD9B0

private:
    static custom_follow_wounded the_instance; // 0x01D67E60
};

// size 0x34
class custom_look_at_action : public ai_action
{
    custom_look_at_action(); // 0x006003B0 (.h:29)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00600650 (.h:29)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E8990 (:1304)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069D500 (:1314)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E89B0 (:1356)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x0062E480 (:1372)
    static ai_action* get_instance();
    virtual ~custom_look_at_action(); // vtbl+0x00 0x005DD980

private:
    static custom_look_at_action the_instance; // 0x01DDAF40
};

// size 0x34
class marauder_lookout_action : public ai_action
{
    marauder_lookout_action(); // 0x00600F30 (.h:38)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00601230 (.h:38)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E89E0 (:1403)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069D670 (:1413)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069D730 (:1434)
    static ai_action* get_instance();
    virtual ~marauder_lookout_action(); // vtbl+0x00 0x005DD9C0

private:
    static marauder_lookout_action the_instance; // 0x01DDB764
};

// size 0x34
class saboteur_goto_action : public ai_action
{
    saboteur_goto_action(); // 0x00601270 (.h:41)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0062E510 (:1504)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00601560 (.h:41)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E8A00 (:1522)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069DB80 (:1528)
    static ai_action* get_instance();
    virtual ~saboteur_goto_action(); // vtbl+0x00 0x005DD9D0

private:
    static saboteur_goto_action the_instance; // 0x01D75638
};

// size 0x34
class saboteur_bomb_action : public ai_action
{
    saboteur_bomb_action(); // 0x00601590 (.h:42)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0062E6E0 (:1616)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x006018F0 (.h:42)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005E8A10 (:1632)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0065CE70 (:1638)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0062E7A0 (:1669)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x0062E7D0 (:1681)
    static ai_action* get_instance();
    virtual ~saboteur_bomb_action(); // vtbl+0x00 0x005DD9E0

private:
    static saboteur_bomb_action the_instance; // 0x01D75304
};

// size 0x34
class terrorize_goto_action : public ai_action
{
    terrorize_goto_action(); // 0x00601920 (.h:45)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00601C70 (.h:45)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E8A20 (:1693)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005E8A60 (:1706)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069DC90 (:1720)
    static ai_action* get_instance();
    virtual ~terrorize_goto_action(); // vtbl+0x00 0x005DD9F0

private:
    static terrorize_goto_action the_instance; // 0x01D683A8
};

// size 0x34
class terrorize_action : public ai_action
{
    terrorize_action(); // 0x00601CB0 (.h:46)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00602070 (.h:46)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0062E7F0 (:1866)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005E8A90 (:1889)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069DF30 (:1914)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005E8B10 (:2021)
    static ai_action* get_instance(); // 0x005DDA00 (.h:46)
    virtual ~terrorize_action(); // vtbl+0x00 0x005DDA10

private:
    static terrorize_action the_instance; // 0x01DDAD24
};

// size 0x34
class coot_ramble_action : public ai_action
{
    coot_ramble_action(); // 0x00602610 (.h:54)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00602660 (.h:54)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E8B40 (:2065)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0065D010 (:2078)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E8B70 (:2083)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005E8B80 (:2088)
    static ai_action* get_instance();
    virtual ~coot_ramble_action(); // vtbl+0x00 0x005DDA40

private:
    static coot_ramble_action the_instance; // 0x01D75834
};

// size 0x34
class vehicle_hostage_action : public ai_action
{
    vehicle_hostage_action(); // 0x00602390 (.h:50)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006025D0 (.h:50)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E8BD0 (:2096)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005E8BF0 (:2106)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E8C10 (:2126)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005E8C20 (:2132)
    static ai_action* get_instance();
    virtual ~vehicle_hostage_action(); // vtbl+0x00 0x005DDA30

private:
    static vehicle_hostage_action the_instance; // 0x01DA3EB0
};

// size 0x34
class tied_up_action : public ai_action
{
    tied_up_action(); // 0x006020B0 (.h:49)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00602350 (.h:49)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E8C50 (:2152)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069E320 (:2162)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0062E8B0 (:2215)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x0062E9A0 (:2268)
    static ai_action* get_instance();
    virtual ~tied_up_action(); // vtbl+0x00 0x005DDA20

private:
    static tied_up_action the_instance; // 0x01DDB688
};

// size 0x34
class drop_off_vehicle_action : public ai_action
{
    drop_off_vehicle_action(); // 0x00602730 (.h:59)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00602970 (.h:59)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0069E440 (:2314)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0062EA60 (:2341)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x0069E4E0 (:2369)
    static ai_action* get_instance();
    virtual ~drop_off_vehicle_action(); // vtbl+0x00 0x005DDA60

private:
    static drop_off_vehicle_action the_instance; // 0x01DA023C
};

// size 0x34
class tech_upgrade_response : public ai_action
{
    tech_upgrade_response(); // 0x006026A0 (.h:56)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x006026F0 (.h:56)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005E8C70 (:2457)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0069E6D0 (:2467)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005E8C90 (:2510)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x0062EAF0 (:2541)
    static ai_action* get_instance();
    virtual ~tech_upgrade_response(); // vtbl+0x00 0x005DDA50

private:
    static tech_upgrade_response the_instance; // 0x01DCA01C
};

bool is_too_close_to_npc(unsigned int npc_handle, const vector& pos); // 0x0065C900 (:128)
bool move_with_human_teleport_destination(unsigned int npc_handle, const vector& desired_teleport_pos, const float MAX_DIST_SQRD, vector& teleport_pos); // 0x0065C990 (:148)
ai_results old_coot_cleanup_common(npc* np, bool last_frame); // 0x005E83B0 (:505)
bool sniper_find_new_guard_node_collect_cb(object* obj, unsigned int shape_key, const void* cb_data); // 0x005E8460 (:622)

inline ai_action* custom_rescued_action::get_instance()
{
    return &the_instance;
}

inline ai_action* terrorize_action::get_instance()
{
    return &the_instance;
}
