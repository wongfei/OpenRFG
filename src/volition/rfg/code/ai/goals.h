// Original: volition/rfg/code/ai/goals.h
// Implementation: volition/rfg/code/ai/goals.cpp
//
// Goals: the ai_goal base (a named singleton with the conditions it pursues and a fitness an npc evaluates every
// replan) and the goals of the planner (idle, combat, melee, flush out, acquire target, retreat, cover, reload, dodge,
// civilian confrontation, threats and taunts, fleeing, orders, guarding, patrols, following, reinforcements, chases,
// rails, turrets, passengers, convoys, investigations, scripted moves, flyers, recovery). ai_planner picks the
// fittest goal and searches a plan of actions that satisfies it.
#pragma once

#include "volition/rfg/code/ai/plan_conditions.h"

struct ai_goal_setup_data;
class human;
class npc;
class object;

// size 0x1C
class ai_goal
{
public:
    virtual ~ai_goal(); // vtbl+0x00 0x005DCD50 (.h:115)
    ai_plan_conditions get_conditions() const; // 0x005DCD60 (.h:117)
    bool conditions_satisfied(ai_plan_conditions conditions) const; // 0x005FB860 (.h:118)
    bool pursues_condition(const ai_plan_conditions& conditions) const; // 0x005FB8F0 (.h:119)
    const char* get_name() const; // 0x005DCD80 (.h:120)
    virtual int get_fitness(const npc*, ai_plan_conditions) const = 0; // vtbl+0x04
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x005DCD90 (.h:123)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x005DCDA0 (.h:124)
    static ai_goal* get_goal_by_name(const char* name); // 0x005E9F60 (:448)
protected:
    ai_goal(ai_plan_conditions _post_conditions, const char* _goal_name); // 0x005E9F00 (:423)

    // 0x00 vfptr
public:
    static int num_goal_types; // 0x01D67DC8
    static ai_goal* goal_pool[128]; // 0x01D65740
protected:
    ai_plan_conditions post_conditions; // 0x04
    const char* goal_name; // 0x18
};

// size 0x1C
class wait_goal : public ai_goal
{
    wait_goal(); // 0x005FBB60 (.h:225)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0065F650 (:468)
    static ai_goal* get_instance();
    virtual ~wait_goal(); // vtbl+0x00 0x005DCE10

private:
    static wait_goal the_instance; // 0x01D7566C
};

// size 0x1C
class idle_goal : public ai_goal
{
    idle_goal(); // 0x005FB980 (.h:222)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005E9FC0 (:486)
    static ai_goal* get_instance(); // 0x005DCDD0 (.h:222)
    virtual ~idle_goal(); // vtbl+0x00 0x005DCDE0

private:
    static idle_goal the_instance; // 0x01DDAC50
};

// size 0x1C
class idle_despawn_goal : public ai_goal
{
    idle_despawn_goal(); // 0x005FBAC0 (.h:224)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005E9FD0 (:497)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x005EA040 (:534)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x005EA050 (:539)
    static ai_goal* get_instance();
    virtual ~idle_despawn_goal(); // vtbl+0x00 0x005DCE00

private:
    static idle_despawn_goal the_instance; // 0x01DC9788
};

// size 0x1C
class despawn_goal : public ai_goal
{
    despawn_goal(); // 0x005FBA20 (.h:223)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x006369F0 (:555)
    static ai_goal* get_instance();
    virtual ~despawn_goal(); // vtbl+0x00 0x005DCDF0

private:
    static despawn_goal the_instance; // 0x01D755A8
};

// size 0x1C
class kill_enemy_goal : public ai_goal
{
    kill_enemy_goal(); // 0x005FBC00 (.h:230)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0067BCA0 (:580)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x005EA080 (:612)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A0E50 (:627)
    static ai_goal* get_instance();
    virtual ~kill_enemy_goal(); // vtbl+0x00 0x005DCE20

private:
    static kill_enemy_goal the_instance; // 0x01DDB9B4
};

// size 0x1C
class melee_goal : public ai_goal
{
    melee_goal(); // 0x005FBCA0 (.h:231)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0067BD90 (:640)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x005EA0C0 (:793)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A0E90 (:804)
    static ai_goal* get_instance(); // 0x005DCE30 (.h:231)
    virtual ~melee_goal(); // vtbl+0x00 0x005DCE40

private:
    static melee_goal the_instance; // 0x01DCA364
};

// size 0x1C
class melee_vehicle_goal : public ai_goal
{
    melee_vehicle_goal(); // 0x005FBD40 (.h:232)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0067C250 (:815)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x005EA0F0 (:885)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A0EC0 (:897)
    static ai_goal* get_instance();
    virtual ~melee_vehicle_goal(); // vtbl+0x00 0x005DCE50

private:
    static melee_vehicle_goal the_instance; // 0x01D75414
};

// size 0x1C
class melee_safe_position_goal : public ai_goal
{
    melee_safe_position_goal(); // 0x005FBDE0 (.h:233)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0067C410 (:908)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0067C5E0 (:980)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A0EF0 (:1006)
    static ai_goal* get_instance(); // 0x005DCE60 (.h:233)
    virtual ~melee_safe_position_goal(); // vtbl+0x00 0x005DCE70

private:
    static melee_safe_position_goal the_instance; // 0x01DC952C
};

// size 0x1C
class flush_out_target_goal : public ai_goal
{
    flush_out_target_goal(); // 0x005FC100 (.h:238)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0067C680 (:1017)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0065F6A0 (:1095)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A0F20 (:1123)
    static ai_goal* get_instance(); // 0x005DCEF0 (.h:238)
    virtual ~flush_out_target_goal(); // vtbl+0x00 0x005DCF00

private:
    static flush_out_target_goal the_instance; // 0x01DCA6B0
};

// size 0x1C
class acquire_target_goal : public ai_goal
{
    acquire_target_goal(); // 0x005FC1A0 (.h:239)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0067C7C0 (:1131)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x00636A80 (:1172)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A0F40 (:1189)
    static ai_goal* get_instance(); // 0x005DCF10 (.h:239)
    virtual ~acquire_target_goal(); // vtbl+0x00 0x005DCF20

private:
    static acquire_target_goal the_instance; // 0x01DDACD4
};

// size 0x1C
class retreat_defensive_perimeter_goal : public ai_goal
{
    retreat_defensive_perimeter_goal(); // 0x005FC420 (.h:243)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00636AF0 (:1200)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x00637260 (:1269)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x005EA140 (:1314)
    static ai_goal* get_instance();
    virtual ~retreat_defensive_perimeter_goal(); // vtbl+0x00 0x005DCF80

private:
    static retreat_defensive_perimeter_goal the_instance; // 0x01642DF0
};

// size 0x1C
class gawk_goal : public ai_goal
{
    gawk_goal(); // 0x005FDDC0 (.h:320)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x006376C0 (:1372)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x00637760 (:1400)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A0F60 (:1413)
    static ai_goal* get_instance(); // 0x005DD2D0 (.h:320)
    virtual ~gawk_goal(); // vtbl+0x00 0x005DD2E0

private:
    static gawk_goal the_instance; // 0x01642E0C
};

// size 0x1C
class look_for_trouble_goal : public ai_goal
{
    look_for_trouble_goal(); // 0x005FC4C0 (.h:246)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005EA150 (:1426)
    static ai_goal* get_instance();
    virtual ~look_for_trouble_goal(); // vtbl+0x00 0x005DCF90

private:
    static look_for_trouble_goal the_instance; // 0x01642E28
};

// size 0x1C
class respond_to_honk : public ai_goal
{
    respond_to_honk(); // 0x005FCCE0 (.h:266)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005EA190 (:1446)
    static ai_goal* get_instance();
    virtual ~respond_to_honk(); // vtbl+0x00 0x005DD0A0

private:
    static respond_to_honk the_instance; // 0x01642D2C
};

// size 0x1C
class defensive_combat_goal : public ai_goal
{
    defensive_combat_goal(); // 0x005FC240 (.h:240)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0067C900 (:1464)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0065F730 (:1509)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A0FD0 (:1575)
    static ai_goal* get_instance(); // 0x005DCF30 (.h:240)
    virtual ~defensive_combat_goal(); // vtbl+0x00 0x005DCF40

private:
    static defensive_combat_goal the_instance; // 0x01D74F04
};

// size 0x1C
class combat_support_goal : public ai_goal
{
    combat_support_goal(); // 0x005FC2E0 (.h:241)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0067C9D0 (:1599)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x00637790 (:1633)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A1040 (:1648)
    static ai_goal* get_instance(); // 0x005DCF50 (.h:241)
    virtual ~combat_support_goal(); // vtbl+0x00 0x005DCF60

private:
    static combat_support_goal the_instance; // 0x01DA0270
};

// size 0x1C
class adjust_cover_stance_goal : public ai_goal
{
    adjust_cover_stance_goal(); // 0x005FC380 (.h:242)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x006377F0 (:1656)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x006378D0 (:1705)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x005EA1B0 (:1746)
    static ai_goal* get_instance();
    virtual ~adjust_cover_stance_goal(); // vtbl+0x00 0x005DCF70

private:
    static adjust_cover_stance_goal the_instance; // 0x01DA60C0
};

// size 0x1C
class reload_weapon_goal : public ai_goal
{
    reload_weapon_goal(); // 0x005FBE80 (.h:234)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005EA1D0 (:1755)
    static ai_goal* get_instance(); // 0x005DCE80 (.h:234)
    virtual ~reload_weapon_goal(); // vtbl+0x00 0x005DCE90

private:
    static reload_weapon_goal the_instance; // 0x01D7516C
};

// size 0x1C
class dodge_goal : public ai_goal
{
    dodge_goal(); // 0x005FBF20 (.h:235)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0065F920 (:1798)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0065FA80 (:1843)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x005EA280 (:1874)
    static ai_goal* get_instance(); // 0x005DCEA0 (.h:235)
    virtual ~dodge_goal(); // vtbl+0x00 0x005DCEB0

private:
    static dodge_goal the_instance; // 0x01DC99CC
};

// size 0x1C
class fall_back_goal : public ai_goal
{
    fall_back_goal(); // 0x005FC060 (.h:237)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0067CD00 (:1960)
    static ai_goal* get_instance();
    virtual ~fall_back_goal(); // vtbl+0x00 0x005DCEE0

private:
    static fall_back_goal the_instance; // 0x01DC9704
};

// size 0x1C
class advance_goal : public ai_goal
{
    advance_goal(); // 0x005FBFC0 (.h:236)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x006A1060 (:2029)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0067CED0 (:2139)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A12C0 (:2285)
    static ai_goal* get_instance(); // 0x005DCEC0 (.h:236)
    virtual ~advance_goal(); // vtbl+0x00 0x005DCED0

private:
    static advance_goal the_instance; // 0x01D67E44
};

// size 0x1C
class custom_civ_action_goal : public ai_goal
{
    custom_civ_action_goal(); // 0x005FE220 (.h:330)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005EA290 (:2316)
    static ai_goal* get_instance();
    virtual ~custom_civ_action_goal(); // vtbl+0x00 0x005DD360

private:
    static custom_civ_action_goal the_instance; // 0x01DC981C
};

// size 0x1C
class custom_orders_goal : public ai_goal
{
    custom_orders_goal(); // 0x005FE180 (.h:329)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0065FB90 (:2331)
    static ai_goal* get_instance();
    virtual ~custom_orders_goal(); // vtbl+0x00 0x005DD350

private:
    static custom_orders_goal the_instance; // 0x01DDB8D4
};

// size 0x1C
class ambient_edf_goal : public ai_goal
{
    ambient_edf_goal(); // 0x005FE2C0 (.h:333)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005EA2B0 (:2357)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x00637980 (:2377)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x005EA320 (:2393)
    static ai_goal* get_instance();
    virtual ~ambient_edf_goal(); // vtbl+0x00 0x005DD370

private:
    static ambient_edf_goal the_instance; // 0x01DC9D44
};

// size 0x1C
class confront_civilian_goal : public ai_goal
{
    confront_civilian_goal(); // 0x005FCD80 (.h:271)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00637AE0 (:2439)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x006B7420 (:2461)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A1370 (:2484)
    static ai_goal* get_instance(); // 0x005DD0B0 (.h:271)
    virtual ~confront_civilian_goal(); // vtbl+0x00 0x005DD0C0

private:
    static confront_civilian_goal the_instance; // 0x01D9F83C
};

// size 0x1C
class threaten_watch_goal : public ai_goal
{
    threaten_watch_goal(); // 0x005FCEC0 (.h:273)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0065FBE0 (:2495)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0065FC40 (:2516)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A13A0 (:2530)
    static ai_goal* get_instance(); // 0x005DD0F0 (.h:273)
    virtual ~threaten_watch_goal(); // vtbl+0x00 0x005DD100

private:
    static threaten_watch_goal the_instance; // 0x01D74F20
};

// size 0x1C
class threaten_civilian_goal : public ai_goal
{
    threaten_civilian_goal(); // 0x005FCE20 (.h:272)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x006A13E0 (:2544)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x006A1440 (:2568)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A1470 (:2588)
    static ai_goal* get_instance(); // 0x005DD0D0 (.h:272)
    virtual ~threaten_civilian_goal(); // vtbl+0x00 0x005DD0E0

private:
    static threaten_civilian_goal the_instance; // 0x01D68570
};

// size 0x1C
class taunt_civilian_goal : public ai_goal
{
    taunt_civilian_goal(); // 0x005FD000 (.h:279)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0065FC90 (:2597)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0065FD40 (:2626)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A1490 (:2656)
    static ai_goal* get_instance();
    virtual ~taunt_civilian_goal(); // vtbl+0x00 0x005DD130

private:
    static taunt_civilian_goal the_instance; // 0x01D67F48
};

// size 0x1C
class taunt_enemy_goal : public ai_goal
{
    taunt_enemy_goal(); // 0x005FD140 (.h:281)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0065FDC0 (:2666)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0065FE10 (:2685)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A14D0 (:2702)
    static ai_goal* get_instance();
    virtual ~taunt_enemy_goal(); // vtbl+0x00 0x005DD150

private:
    static taunt_enemy_goal the_instance; // 0x01D74EE8
};

// size 0x1C
class taunt_player_goal : public ai_goal
{
    taunt_player_goal(); // 0x005FD0A0 (.h:280)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005EA3A0 (:2712)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x006A1510 (:2744)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A1640 (:2779)
    static ai_goal* get_instance();
    virtual ~taunt_player_goal(); // vtbl+0x00 0x005DD140

private:
    static taunt_player_goal the_instance; // 0x01D75040
};

// size 0x1C
class oppress_civilian_goal : public ai_goal
{
    oppress_civilian_goal(); // 0x005FCF60 (.h:274)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00637B40 (:2789)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0067D2A0 (:2824)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A1680 (:2902)
    static ai_goal* get_instance(); // 0x005DD110 (.h:274)
    virtual ~oppress_civilian_goal(); // vtbl+0x00 0x005DD120

private:
    static oppress_civilian_goal the_instance; // 0x01D9F960
};

// size 0x1C
class flee_heavy_vehicle_goal : public ai_goal
{
    flee_heavy_vehicle_goal(); // 0x005FCA60 (.h:261)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0065FE70 (:2918)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0065FF00 (:2945)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A16E0 (:2964)
    static ai_goal* get_instance(); // 0x005DD050 (.h:261)
    virtual ~flee_heavy_vehicle_goal(); // vtbl+0x00 0x005DD060

private:
    static flee_heavy_vehicle_goal the_instance; // 0x01DCA47C
};

// size 0x1C
class flee_restricted_area_goal : public ai_goal
{
    flee_restricted_area_goal(); // 0x005FC9C0 (.h:260)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00637BC0 (:2982)
    static ai_goal* get_instance(); // 0x005DD030 (.h:260)
    virtual ~flee_restricted_area_goal(); // vtbl+0x00 0x005DD040

private:
    static flee_restricted_area_goal the_instance; // 0x01DCA454
};

// size 0x1C
class return_to_orders_pos_goal : public ai_goal
{
    return_to_orders_pos_goal(); // 0x005FCBA0 (.h:263)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00637DC0 (:3071)
    static ai_goal* get_instance();
    virtual ~return_to_orders_pos_goal(); // vtbl+0x00 0x005DD080

private:
    static return_to_orders_pos_goal the_instance; // 0x01D725D8
};

// size 0x1C
class use_action_node_goal : public ai_goal
{
    use_action_node_goal(); // 0x005FCB00 (.h:262)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0067D420 (:3110)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0067D4B0 (:3156)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x005EA440 (:3274)
    static ai_goal* get_instance();
    virtual ~use_action_node_goal(); // vtbl+0x00 0x005DD070

private:
    static use_action_node_goal the_instance; // 0x01642D10
};

// size 0x1C
class roadblock_goal : public ai_goal
{
    roadblock_goal(); // 0x005FC600 (.h:252)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00637E80 (:3286)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x00637FA0 (:3343)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x005EA460 (:3368)
    static ai_goal* get_instance();
    virtual ~roadblock_goal(); // vtbl+0x00 0x005DCFB0

private:
    static roadblock_goal the_instance; // 0x01DDADE0
};

// size 0x1C
class guard_object_goal : public ai_goal
{
    guard_object_goal(); // 0x005FC560 (.h:251)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00638000 (:3379)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x00638070 (:3392)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x005EA470 (:3406)
    static ai_goal* get_instance();
    virtual ~guard_object_goal(); // vtbl+0x00 0x005DCFA0

private:
    static guard_object_goal the_instance; // 0x01DDB06C
};

// size 0x1C
class move_with_human_goal : public ai_goal
{
    move_with_human_goal(); // 0x005FC740 (.h:255)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x006A1720 (:3429)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x006380E0 (:3485)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x005EA4A0 (:3503)
    static ai_goal* get_instance(); // 0x005DCFD0 (.h:255)
    virtual ~move_with_human_goal(); // vtbl+0x00 0x005DCFE0

private:
    static move_with_human_goal the_instance; // 0x01DCA314
};

// size 0x1C
class patrol_path_goal : public ai_goal
{
    patrol_path_goal(); // 0x005FC6A0 (.h:253)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0067D7C0 (:3512)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0067D850 (:3540)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x0067D870 (:3556)
    static ai_goal* get_instance();
    virtual ~patrol_path_goal(); // vtbl+0x00 0x005DCFC0

private:
    static patrol_path_goal the_instance; // 0x01D7207C
};

// size 0x1C
class wait_for_squad_goal : public ai_goal
{
    wait_for_squad_goal(); // 0x005FC880 (.h:257)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00638160 (:3575)
    static ai_goal* get_instance();
    virtual ~wait_for_squad_goal(); // vtbl+0x00 0x005DD000

private:
    static wait_for_squad_goal the_instance; // 0x01DC99E8
};

// size 0x1C
class follow_leader_goal : public ai_goal
{
    follow_leader_goal(); // 0x005FC7E0 (.h:256)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0067D900 (:3608)
    static ai_goal* get_instance();
    virtual ~follow_leader_goal(); // vtbl+0x00 0x005DCFF0

private:
    static follow_leader_goal the_instance; // 0x01D74ECC
};

// size 0x1C
class reinforcement_goal : public ai_goal
{
    reinforcement_goal(); // 0x005FC920 (.h:258)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0067DAF0 (:3715)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x006381D0 (:3780)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x0067DD60 (:3802)
    static ai_goal* get_instance(); // 0x005DD010 (.h:258)
    virtual ~reinforcement_goal(); // vtbl+0x00 0x005DD020

private:
    static reinforcement_goal the_instance; // 0x01DDB924
};

// size 0x1C
class chase_target_goal : public ai_goal
{
    chase_target_goal(); // 0x005FD320 (.h:289)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x006A18A0 (:3827)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0067DE40 (:3966)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A1CE0 (:4038)
    static ai_goal* get_instance(); // 0x005DD180 (.h:289)
    virtual ~chase_target_goal(); // vtbl+0x00 0x005DD190

private:
    static chase_target_goal the_instance; // 0x01D9F620
};

// size 0x1C
class accompany_chase_target_goal : public ai_goal
{
    accompany_chase_target_goal(); // 0x005FD3C0 (.h:290)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0065FFA0 (:4056)
    static ai_goal* get_instance();
    virtual ~accompany_chase_target_goal(); // vtbl+0x00 0x005DD1A0

private:
    static accompany_chase_target_goal the_instance; // 0x01DC9B18
};

// size 0x1C
class drive_on_rail_goal : public ai_goal
{
    drive_on_rail_goal(); // 0x005FD280 (.h:287)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005EA4C0 (:4085)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x006A1D30 (:4102)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006382B0 (:4149)
    static ai_goal* get_instance();
    virtual ~drive_on_rail_goal(); // vtbl+0x00 0x005DD170

private:
    static drive_on_rail_goal the_instance; // 0x01642D48
};

// size 0x1C
class smash_building_goal : public ai_goal
{
    smash_building_goal(); // 0x005FD1E0 (.h:286)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00638370 (:4180)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x006383D0 (:4196)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x005EA4F0 (:4223)
    static ai_goal* get_instance();
    virtual ~smash_building_goal(); // vtbl+0x00 0x005DD160

private:
    static smash_building_goal the_instance; // 0x01DA0550
};

// size 0x1C
class man_turret_in_vehicle_goal : public ai_goal
{
    man_turret_in_vehicle_goal(); // 0x005FD460 (.h:292)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x006A1E20 (:4237)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x006A1EF0 (:4304)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x005EA530 (:4319)
    static ai_goal* get_instance();
    virtual ~man_turret_in_vehicle_goal(); // vtbl+0x00 0x005DD1B0

private:
    static man_turret_in_vehicle_goal the_instance; // 0x01DDB0F0
};

// size 0x1C
class wait_passenger_clear_goal : public ai_goal
{
    wait_passenger_clear_goal(); // 0x005FD5A0 (.h:295)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00638440 (:4356)
    static ai_goal* get_instance();
    virtual ~wait_passenger_clear_goal(); // vtbl+0x00 0x005DD1E0

private:
    static wait_passenger_clear_goal the_instance; // 0x01D75188
};

// size 0x1C
class wait_for_passengers_goal : public ai_goal
{
    wait_for_passengers_goal(); // 0x005FD500 (.h:294)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x006A1F30 (:4379)
    static ai_goal* get_instance(); // 0x005DD1C0 (.h:294)
    virtual ~wait_for_passengers_goal(); // vtbl+0x00 0x005DD1D0

private:
    static wait_for_passengers_goal the_instance; // 0x01DC97B0
};

// size 0x1C
class drop_off_passengers_goal : public ai_goal
{
    drop_off_passengers_goal(); // 0x005FD640 (.h:296)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00660080 (:4520)
    static ai_goal* get_instance();
    virtual ~drop_off_passengers_goal(); // vtbl+0x00 0x005DD1F0

private:
    static drop_off_passengers_goal the_instance; // 0x01D68304
};

// size 0x1C
class tied_up_goal : public ai_goal
{
    tied_up_goal(); // 0x005FD960 (.h:308)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005EA5B0 (:4543)
    static ai_goal* get_instance(); // 0x005DD250 (.h:308)
    virtual ~tied_up_goal(); // vtbl+0x00 0x005DD260

private:
    static tied_up_goal the_instance; // 0x01642D64
};

// size 0x1C
class convoy_lead_goal : public ai_goal
{
    convoy_lead_goal(); // 0x005FDA00 (.h:309)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0067E190 (:4555)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0067E270 (:4592)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x005EA5D0 (:4619)
    static ai_goal* get_instance();
    virtual ~convoy_lead_goal(); // vtbl+0x00 0x005DD270

private:
    static convoy_lead_goal the_instance; // 0x01642D80
};

// size 0x1C
class convoy_follow_goal : public ai_goal
{
    convoy_follow_goal(); // 0x005FDAA0 (.h:310)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0067E320 (:4633)
    static ai_goal* get_instance();
    virtual ~convoy_follow_goal(); // vtbl+0x00 0x005DD280

private:
    static convoy_follow_goal the_instance; // 0x01642D9C
};

// size 0x1C
class convoy_escort_goal : public ai_goal
{
    convoy_escort_goal(); // 0x005FDB40 (.h:311)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0067E410 (:4686)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0067E4D0 (:4727)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x005EA600 (:4770)
    static ai_goal* get_instance();
    virtual ~convoy_escort_goal(); // vtbl+0x00 0x005DD290

private:
    static convoy_escort_goal the_instance; // 0x01642DB8
};

// size 0x1C
class wait_for_bodyguards_goal : public ai_goal
{
    wait_for_bodyguards_goal(); // 0x005FDBE0 (.h:312)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0067E580 (:4780)
    static ai_goal* get_instance();
    virtual ~wait_for_bodyguards_goal(); // vtbl+0x00 0x005DD2A0

private:
    static wait_for_bodyguards_goal the_instance; // 0x01642DD4
};

// size 0x1C
class examine_corpse_goal : public ai_goal
{
    examine_corpse_goal(); // 0x005FDC80 (.h:318)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0067E5B0 (:4798)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0067E730 (:4849)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x005EA630 (:4899)
    static ai_goal* get_instance();
    virtual ~examine_corpse_goal(); // vtbl+0x00 0x005DD2B0

private:
    static examine_corpse_goal the_instance; // 0x01DA0744
};

// size 0x1C
class move_out_of_way_goal : public ai_goal
{
    move_out_of_way_goal(); // 0x005FDD20 (.h:319)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x006384A0 (:4913)
    static ai_goal* get_instance();
    virtual ~move_out_of_way_goal(); // vtbl+0x00 0x005DD2C0

private:
    static move_out_of_way_goal the_instance; // 0x01DC9800
};

// size 0x1C
class get_out_of_vehicle_goal : public ai_goal
{
    get_out_of_vehicle_goal(); // 0x005FDE60 (.h:321)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005EA660 (:4949)
    static ai_goal* get_instance();
    virtual ~get_out_of_vehicle_goal(); // vtbl+0x00 0x005DD2F0

private:
    static get_out_of_vehicle_goal the_instance; // 0x01D7524C
};

// size 0x1C
class salute_goal : public ai_goal
{
    salute_goal(); // 0x005FDF00 (.h:322)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x006A22E0 (:4965)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x006600C0 (:5009)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006A23A0 (:5054)
    static ai_goal* get_instance();
    virtual ~salute_goal(); // vtbl+0x00 0x005DD300

private:
    static salute_goal the_instance; // 0x01DA0658
};

// size 0x1C
class move_to_location_goal : public ai_goal
{
    move_to_location_goal(); // 0x005FE040 (.h:327)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005EA6A0 (:5072)
    static ai_goal* get_instance();
    virtual ~move_to_location_goal(); // vtbl+0x00 0x005DD320

private:
    static move_to_location_goal the_instance; // 0x01642E44
};

// size 0x1C
class drive_to_location_goal : public ai_goal
{
    drive_to_location_goal(); // 0x005FE0E0 (.h:328)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005EA6D0 (:5083)
    static ai_goal* get_instance(); // 0x005DD330 (.h:328)
    virtual ~drive_to_location_goal(); // vtbl+0x00 0x005DD340

private:
    static drive_to_location_goal the_instance; // 0x01642E60
};

// size 0x1C
class loiter_flyer_goal : public ai_goal
{
    loiter_flyer_goal(); // 0x005FD6E0 (.h:298)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00638560 (:5098)
    static ai_goal* get_instance();
    virtual ~loiter_flyer_goal(); // vtbl+0x00 0x005DD200

private:
    static loiter_flyer_goal the_instance; // 0x01DC9BDC
};

// size 0x1C
class man_flyer_goal : public ai_goal
{
    man_flyer_goal(); // 0x005FD780 (.h:299)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0067E8A0 (:5123)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0067EA70 (:5185)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x005EA710 (:5231)
    static ai_goal* get_instance(); // 0x005DD210 (.h:299)
    virtual ~man_flyer_goal(); // vtbl+0x00 0x005DD220

private:
    static man_flyer_goal the_instance; // 0x01DDAD58
};

// size 0x1C
class fodder_goal : public ai_goal
{
    fodder_goal(); // 0x005FCC40 (.h:265)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x006385C0 (:5244)
    static ai_goal* get_instance();
    virtual ~fodder_goal(); // vtbl+0x00 0x005DD090

private:
    static fodder_goal the_instance; // 0x01D9F8DC
};

// size 0x1C
class get_back_on_navmesh_goal : public ai_goal
{
    get_back_on_navmesh_goal(); // 0x005FD820 (.h:301)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x006602C0 (:5274)
    static ai_goal* get_instance();
    virtual ~get_back_on_navmesh_goal(); // vtbl+0x00 0x005DD230

private:
    static get_back_on_navmesh_goal the_instance; // 0x01D68650
};

// size 0x1C
class recover_from_stuck_goal : public ai_goal
{
    recover_from_stuck_goal(); // 0x005FDFA0 (.h:323)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00638620 (:5302)
    static ai_goal* get_instance();
    virtual ~recover_from_stuck_goal(); // vtbl+0x00 0x005DD310

private:
    static recover_from_stuck_goal the_instance; // 0x01DDB010
};

// size 0x1C
class pull_off_road_goal : public ai_goal
{
    pull_off_road_goal(); // 0x005FD8C0 (.h:303)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00638680 (:5323)
    static ai_goal* get_instance();
    virtual ~pull_off_road_goal(); // vtbl+0x00 0x005DD240

private:
    static pull_off_road_goal the_instance; // 0x01DA0534
};

int ai_get_pursuit_time(const npc* np, const human* hp); // 0x0065F590 (:383)
void ai_disable_threaten_behavior(); // 0x005E9EE0 (:404)
void ai_enable_threaten_behavior(); // 0x005E9EF0 (:410)
object* get_gawk_target(const npc* np); // 0x00637570 (:1322)

inline ai_goal* idle_goal::get_instance()
{
    return &the_instance;
}

inline ai_goal* flee_heavy_vehicle_goal::get_instance()
{
    return &the_instance;
}

inline ai_goal* flee_restricted_area_goal::get_instance()
{
    return &the_instance;
}

inline ai_goal* man_flyer_goal::get_instance()
{
    return &the_instance;
}

inline ai_goal* confront_civilian_goal::get_instance()
{
    return &the_instance;
}

inline ai_goal* threaten_watch_goal::get_instance()
{
    return &the_instance;
}

inline ai_goal* threaten_civilian_goal::get_instance()
{
    return &the_instance;
}
