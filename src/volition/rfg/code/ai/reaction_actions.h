// Original: volition/rfg/code/ai/reaction_actions.h
// Implementation: volition/rfg/code/ai/reaction_actions.cpp
//
// Reactions: catching breath, reacting to enemies, projectiles, kills, taunts, commandeered vehicles and raids (battle
// cries, pointing out enemies, celebrating). Goals and actions live together here. Each action is a singleton (get_instance) with planner preconditions and postconditions, a cost, and enter/do_action/cleanup steps the planner runs for an npc.
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
class catch_breath_goal : public ai_goal
{
    catch_breath_goal(); // 0x0063A1F0 (.h:14)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005F07A0 (:134)
    static ai_goal* get_instance();
    virtual ~catch_breath_goal(); // vtbl+0x00 0x005EBAD0

private:
    static catch_breath_goal the_instance; // 0x01DC9AFC
};

// size 0x1C
class raid_reaction_goal : public ai_goal
{
    raid_reaction_goal(); // 0x0063A330 (.h:16)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005F0810 (:160)
    static ai_goal* get_instance();
    virtual ~raid_reaction_goal(); // vtbl+0x00 0x005EBB00

private:
    static raid_reaction_goal the_instance; // 0x01D68828
};

// size 0x1C
class taunt_reaction_goal : public ai_goal
{
    taunt_reaction_goal(); // 0x0063A3D0 (.h:17)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005F0830 (:173)
    static ai_goal* get_instance();
    virtual ~taunt_reaction_goal(); // vtbl+0x00 0x005EBB10

private:
    static taunt_reaction_goal the_instance; // 0x01D68878
};

// size 0x1C
class salute_reaction_goal : public ai_goal
{
    salute_reaction_goal(); // 0x0063A470 (.h:18)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005F0870 (:192)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x00640740 (:208)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A8150 (:231)
    static ai_goal* get_instance();
    virtual ~salute_reaction_goal(); // vtbl+0x00 0x005EBB20

private:
    static salute_reaction_goal the_instance; // 0x01DC986C
};

// size 0x1C
class react_goal : public ai_goal
{
    react_goal(); // 0x0063A290 (.h:15)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005F08C0 (:240)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x005F0900 (:250)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A8170 (:273)
    static ai_goal* get_instance(); // 0x005EBAE0 (.h:15)
    virtual ~react_goal(); // vtbl+0x00 0x005EBAF0

private:
    static react_goal the_instance; // 0x01DC9A04
};

// size 0x34
class catch_breath_action : public ai_action
{
    catch_breath_action(); // 0x0063A510 (.h:21)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0063A740 (.h:21)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F0960 (:282)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006407D0 (:292)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005F0980 (:334)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005F09B0 (:341)
    static ai_action* get_instance();
    virtual ~catch_breath_action(); // vtbl+0x00 0x005EBB30

private:
    static catch_breath_action the_instance; // 0x01DDB838
};

// size 0x34
class catch_breath_prone_action : public ai_action
{
    catch_breath_prone_action();
public:
    virtual int get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const; // vtbl+0x04
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F09D0 (:353)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006409D0 (:363)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005F09F0 (:382)
    static ai_action* get_instance();

private:
    static catch_breath_prone_action the_instance;
};

// size 0x34
class react_to_enemy_action : public ai_action
{
    react_to_enemy_action(); // 0x0063AB90 (.h:27)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0063AC40 (.h:27)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F0A20 (:392)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00640A60 (:414)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00640B20 (:433)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005F0A90 (:452)
    static ai_action* get_instance();
    virtual ~react_to_enemy_action(); // vtbl+0x00 0x005EBB60

private:
    static react_to_enemy_action the_instance; // 0x01DCA160
};

// size 0x34
class surprised_by_projectile_action : public ai_action
{
    surprised_by_projectile_action(); // 0x0063AC80 (.h:28)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0063ACD0 (.h:28)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F0AE0 (:462)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00640C50 (:489)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005F0B70 (:515)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005F0BF0 (:527)
    static ai_action* get_instance();
    virtual ~surprised_by_projectile_action(); // vtbl+0x00 0x005EBB70

private:
    static surprised_by_projectile_action the_instance; // 0x01DCA3EC
};

// size 0x34
class react_killed_enemy_action : public ai_action
{
    react_killed_enemy_action(); // 0x0063B8E0 (.h:49)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0063BB80 (.h:49)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x00683B90 (:537)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00683C50 (:583)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005F0C40 (:617)
    static ai_action* get_instance();
    virtual ~react_killed_enemy_action(); // vtbl+0x00 0x005EBC00

private:
    static react_killed_enemy_action the_instance; // 0x01DC95F0
};

// size 0x34
class battle_cry_action : public ai_action
{
    battle_cry_action(); // 0x0063A780 (.h:25)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00683D40 (:642)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0063A8F0 (.h:25)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x00640CF0 (:693)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006A8190 (:701)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00640D40 (:741)
    static ai_action* get_instance();
    virtual ~battle_cry_action(); // vtbl+0x00 0x005EBB40

private:
    static battle_cry_action the_instance; // 0x01D74E30
};

// size 0x34
class point_out_enemy_action : public ai_action
{
    point_out_enemy_action(); // 0x0063A920 (.h:26)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x00683E50 (:766)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x0063AB60 (.h:26)
    virtual void commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const; // vtbl+0x0C 0x005F0D20 (:822)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006A83A0 (:829)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00640DF0 (:904)
    static ai_action* get_instance();
    virtual ~point_out_enemy_action(); // vtbl+0x00 0x005EBB50

private:
    static point_out_enemy_action the_instance; // 0x01DCA728
};

// size 0x34
class drove_off_civilian_action : public ai_action
{
    drove_off_civilian_action(); // 0x0063AEB0 (.h:37)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0063B0F0 (.h:37)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F0D40 (:945)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00683F70 (:962)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005F0D80 (:1000)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005F0DE0 (:1015)
    static ai_action* get_instance();
    virtual ~drove_off_civilian_action(); // vtbl+0x00 0x005EBBA0

private:
    static drove_off_civilian_action the_instance; // 0x01DCA0EC
};

// size 0x34
class react_watch_action : public ai_action
{
    react_watch_action(); // 0x0063AD10 (.h:31)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0063ADD0 (.h:31)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F0E30 (:1025)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00640F40 (:1047)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005F0EA0 (:1111)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005F0ED0 (:1118)
    static ai_action* get_instance();
    virtual ~react_watch_action(); // vtbl+0x00 0x005EBB80

private:
    static react_watch_action the_instance; // 0x01D72134
};

// size 0x34
class react_cower_rule : public ai_production_rule
{
    react_cower_rule(); // 0x0063B130 (.h:40)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0063B1B0 (.h:40)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F0F20 (:1128)
public:
    static ai_action* get_instance();
    virtual ~react_cower_rule(); // vtbl+0x00 0x005EBBB0

private:
    static react_cower_rule the_instance; // 0x01D71EB4
};

// size 0x34
class react_bomb_rule : public ai_production_rule
{
    react_bomb_rule(); // 0x0063B1E0 (.h:41)
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0063B260 (.h:41)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F0F60 (:1148)
public:
    static ai_action* get_instance();
    virtual ~react_bomb_rule(); // vtbl+0x00 0x005EBBC0

private:
    static react_bomb_rule the_instance; // 0x01DCA498
};

// size 0x34
class return_taunt_action : public ai_action
{
    return_taunt_action(); // 0x0063B290 (.h:44)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0063B530 (.h:44)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F0FA0 (:1163)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005F0FD0 (:1180)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00684050 (:1192)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00641050 (:1231)
    static ai_action* get_instance();
    virtual ~return_taunt_action(); // vtbl+0x00 0x005EBBD0

private:
    static return_taunt_action the_instance; // 0x01D6AB20
};

// size 0x34
class react_to_taunt_action : public ai_action
{
    react_to_taunt_action(); // 0x0063B570 (.h:45)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0063B810 (.h:45)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F1010 (:1249)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005F1020 (:1254)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x00684220 (:1266)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005F1060 (:1304)
    static ai_action* get_instance();
    virtual ~react_to_taunt_action(); // vtbl+0x00 0x005EBBE0

private:
    static react_to_taunt_action the_instance; // 0x01D7219C
};

// size 0x34
class flee_from_taunt_action : public ai_action
{
    flee_from_taunt_action(); // 0x0063B850 (.h:46)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0063B8A0 (.h:46)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F10D0 (:1319)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006410C0 (:1328)
    static ai_action* get_instance();
    virtual ~flee_from_taunt_action(); // vtbl+0x00 0x005EBBF0

private:
    static flee_from_taunt_action the_instance; // 0x01DDACF0
};

// size 0x34
class react_vehicle_commandeered : public ai_action
{
    react_vehicle_commandeered(); // 0x0063AE10 (.h:34)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0063AE70 (.h:34)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F1100 (:1343)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x0066A0A0 (:1353)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005F1140 (:1385)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x005F1170 (:1395)
    static ai_action* get_instance();
    virtual ~react_vehicle_commandeered(); // vtbl+0x00 0x005EBB90

private:
    static react_vehicle_commandeered the_instance; // 0x01DCA278
};

// size 0x34
class watch_raid_end_action : public ai_action
{
    watch_raid_end_action(); // 0x0063BBC0 (.h:52)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0063BEC0 (.h:52)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F11C0 (:1472)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x006843D0 (:1489)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005F11F0 (:1513)
    static ai_action* get_instance();
    virtual ~watch_raid_end_action(); // vtbl+0x00 0x005EBC10

private:
    static watch_raid_end_action the_instance; // 0x01D687C0
};

// size 0x34
class celebrate_raid_end_action : public ai_action
{
    celebrate_raid_end_action(); // 0x0063BF00 (.h:53)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0063C200 (.h:53)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F1210 (:1526)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x00684420 (:1535)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x005F1230 (:1567)
    virtual ai_results cleanup(npc* np, const ai_action* next_action) const; // vtbl+0x18 0x00641150 (:1598)
    static ai_action* get_instance();
    virtual ~celebrate_raid_end_action(); // vtbl+0x00 0x005EBC20

private:
    static celebrate_raid_end_action the_instance; // 0x01D68374
};

// size 0x34
class approach_raid_area_action : public ai_action
{
    approach_raid_area_action(); // 0x0063C240 (.h:54)
public:
    virtual int get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const; // vtbl+0x04 0x0063C4E0 (.h:54)
    virtual int get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x08 0x005F1300 (:1617)
    virtual ai_results enter(npc* np, const ai_action* last_action) const; // vtbl+0x10 0x005F1380 (:1643)
    virtual ai_results do_action(npc* np) const; // vtbl+0x14 0x006A8620 (:1669)
    static ai_action* get_instance();
    virtual ~approach_raid_area_action(); // vtbl+0x00 0x005EBC30

private:
    static approach_raid_area_action the_instance; // 0x01DCA1AC
};
