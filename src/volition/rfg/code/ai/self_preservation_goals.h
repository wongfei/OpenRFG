// Original: volition/rfg/code/ai/self_preservation_goals.h
// Implementation: volition/rfg/code/ai/self_preservation_goals.cpp
//
// Self-preservation goals: fleeing unsafe buildings and demolition areas, reacting to attached projectiles and fire,
// evading or dodging projectiles, thrown weapons and vehicles, fleeing debris and enemies, cowering, surrendering,
// the curfew of high alert.
#pragma once

#include "volition/rfg/code/ai/goals.h"
#include "volition/vlib/code/util/varray.h"

struct ai_belief;
struct ai_goal_setup_data;
class ai_plan_conditions;
class npc;
class object;
class vector;

// size 0x1C
class flee_unsafe_building_goal : public ai_goal
{
    flee_unsafe_building_goal(); // 0x00609F80 (.h:22)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00642E00 (:589)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x00642EC0 (:646)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006AB5E0 (:663)
    static ai_goal* get_instance(); // 0x005DE1C0 (.h:22)
    virtual ~flee_unsafe_building_goal(); // vtbl+0x00 0x005DE1D0

private:
    static flee_unsafe_building_goal the_instance; // 0x01DC9E4C
};

// size 0x1C
class leave_demolition_area_goal : public ai_goal
{
    leave_demolition_area_goal(); // 0x0060A020 (.h:23)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005F30C0 (:684)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x00642F00 (:694)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006AB630 (:720)
    static ai_goal* get_instance();
    virtual ~leave_demolition_area_goal(); // vtbl+0x00 0x005DE1E0

private:
    static leave_demolition_area_goal the_instance; // 0x01DDB294
};

// size 0x1C
class react_attached_projectile_goal : public ai_goal
{
    react_attached_projectile_goal(); // 0x00609940 (.h:12)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00642F50 (:728)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x00642FE0 (:764)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006AB650 (:794)
    static ai_goal* get_instance(); // 0x005DE0F0 (.h:12)
    virtual ~react_attached_projectile_goal(); // vtbl+0x00 0x005DE100

private:
    static react_attached_projectile_goal the_instance; // 0x01DC9DFC
};

// size 0x1C
class react_on_fire_goal : public ai_goal
{
    react_on_fire_goal(); // 0x006099E0 (.h:13)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005F30E0 (:804)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x005F3100 (:813)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006AB6D0 (:826)
    static ai_goal* get_instance();
    virtual ~react_on_fire_goal(); // vtbl+0x00 0x005DE110

private:
    static react_on_fire_goal the_instance; // 0x01D75558
};

// size 0x1C
class evade_projectile_goal : public ai_goal
{
    evade_projectile_goal(); // 0x00609A80 (.h:14)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x006430A0 (:848)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x00688A90 (:908)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006AB7D0 (:934)
    static ai_goal* get_instance();
    virtual ~evade_projectile_goal(); // vtbl+0x00 0x005DE120

private:
    static evade_projectile_goal the_instance; // 0x01D684E0
};

// size 0x1C
class dodge_projectile_goal : public ai_goal
{
    dodge_projectile_goal(); // 0x00609B20 (.h:15)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00643170 (:965)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0066DE50 (:1007)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x005F3130 (:1054)
    static ai_goal* get_instance(); // 0x005DE130 (.h:15)
    virtual ~dodge_projectile_goal(); // vtbl+0x00 0x005DE140

private:
    static dodge_projectile_goal the_instance; // 0x01D68024
};

// size 0x1C
class evade_thrown_weapon_goal : public ai_goal
{
    evade_thrown_weapon_goal(); // 0x00609BC0 (.h:16)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00643210 (:1063)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x00688B40 (:1096)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006AB910 (:1122)
    static ai_goal* get_instance();
    virtual ~evade_thrown_weapon_goal(); // vtbl+0x00 0x005DE150

private:
    static evade_thrown_weapon_goal the_instance; // 0x01D71F1C
};

// size 0x1C
class dodge_thrown_weapon_goal : public ai_goal
{
    dodge_thrown_weapon_goal(); // 0x00609C60 (.h:17)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x006432A0 (:1153)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0066E020 (:1190)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x005F31A0 (:1230)
    static ai_goal* get_instance();
    virtual ~dodge_thrown_weapon_goal(); // vtbl+0x00 0x005DE160

private:
    static dodge_thrown_weapon_goal the_instance; // 0x01D75268
};

// size 0x1C
class evade_vehicle_goal : public ai_goal
{
    evade_vehicle_goal(); // 0x00609D00 (.h:18)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00643350 (:1239)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x00688BF0 (:1261)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006BF340 (:1286)
    static ai_goal* get_instance();
    virtual ~evade_vehicle_goal(); // vtbl+0x00 0x005DE170

private:
    static evade_vehicle_goal the_instance; // 0x01DC9A54
};

// size 0x1C
class flee_debris_goal : public ai_goal
{
    flee_debris_goal(); // 0x00609E40 (.h:20)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0066E1C0 (:1333)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x005F3210 (:1361)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006AB9B0 (:1381)
    static ai_goal* get_instance(); // 0x005DE190 (.h:20)
    virtual ~flee_debris_goal(); // vtbl+0x00 0x005DE1A0

private:
    static flee_debris_goal the_instance; // 0x01D71F88
};

// size 0x1C
class flee_from_vehicle_goal : public ai_goal
{
    flee_from_vehicle_goal(); // 0x0060A480 (.h:33)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00688D10 (:1418)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x00688D90 (:1450)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006AB9E0 (:1473)
    static ai_goal* get_instance(); // 0x005DE290 (.h:33)
    virtual ~flee_from_vehicle_goal(); // vtbl+0x00 0x005DE2A0

private:
    static flee_from_vehicle_goal the_instance; // 0x01DCA7F8
};

// size 0x1C
class flee_to_safety_goal : public ai_goal
{
    flee_to_safety_goal(); // 0x0060A3E0 (.h:32)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0066E260 (:1489)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x0066E320 (:1528)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006ABA80 (:1565)
    static ai_goal* get_instance(); // 0x005DE270 (.h:32)
    virtual ~flee_to_safety_goal(); // vtbl+0x00 0x005DE280

private:
    static flee_to_safety_goal the_instance; // 0x01D71F6C
};

// size 0x1C
class flee_enemies_goal : public ai_goal
{
    flee_enemies_goal(); // 0x0060A340 (.h:31)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00688DF0 (:1626)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x00688FE0 (:1673)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006ABC60 (:1690)
    static ai_goal* get_instance(); // 0x005DE250 (.h:31)
    virtual ~flee_enemies_goal(); // vtbl+0x00 0x005DE260

private:
    static flee_enemies_goal the_instance; // 0x01DA61C0
};

// size 0x1C
class cower_goal : public ai_goal
{
    cower_goal(); // 0x00609DA0 (.h:19)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x0066E380 (:1714)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x005F32A0 (:1801)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006ABD30 (:1825)
    static ai_goal* get_instance();
    virtual ~cower_goal(); // vtbl+0x00 0x005DE180

private:
    static cower_goal the_instance; // 0x01DC9664
};

// size 0x1C
class surrender_goal : public ai_goal
{
    surrender_goal(); // 0x00609EE0 (.h:21)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005F3310 (:1840)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x006433E0 (:1873)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x006ABD60 (:1891)
    static ai_goal* get_instance();
    virtual ~surrender_goal(); // vtbl+0x00 0x005DE1B0

private:
    static surrender_goal the_instance; // 0x01DDB5C4
};

// size 0x1C
class go_into_safehouse_goal : public ai_goal
{
    go_into_safehouse_goal(); // 0x0060A0C0 (.h:24)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005F3320 (:1903)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x005F33A0 (:1934)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x00643440 (:1940)
    static ai_goal* get_instance(); // 0x005DE1F0 (.h:24)
    virtual ~go_into_safehouse_goal(); // vtbl+0x00 0x005DE200

private:
    static go_into_safehouse_goal the_instance; // 0x01642F94
};

// size 0x1C
class fetal_position_goal : public ai_goal
{
    fetal_position_goal(); // 0x0060A160 (.h:25)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005F33B0 (:1965)
    static ai_goal* get_instance();
    virtual ~fetal_position_goal(); // vtbl+0x00 0x005DE210

private:
    static fetal_position_goal the_instance; // 0x01D68320
};

// size 0x1C
class fall_back_to_player_goal : public ai_goal
{
    fall_back_to_player_goal(); // 0x0060A2A0 (.h:28)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x00643500 (:1977)
    virtual bool store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const; // vtbl+0x08 0x00643630 (:2016)
    virtual void commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const; // vtbl+0x0C 0x00643740 (:2047)
    static ai_goal* get_instance();
    virtual ~fall_back_to_player_goal(); // vtbl+0x00 0x005DE240

private:
    static fall_back_to_player_goal the_instance; // 0x01D9F750
};

// size 0x1C
class high_alert_curfew_goal : public ai_goal
{
    high_alert_curfew_goal(); // 0x0060A200 (.h:26)
public:
    virtual int get_fitness(const npc* np, ai_plan_conditions working_conditions) const; // vtbl+0x04 0x005F33D0 (:2058)
    static ai_goal* get_instance(); // 0x005DE220 (.h:26)
    virtual ~high_alert_curfew_goal(); // vtbl+0x00 0x005DE230

private:
    static high_alert_curfew_goal the_instance; // 0x01DC96E8
};

extern bool Vehicle_ai_panic_drivers; // 0x01D67DCE
bool should_evade_sledgehammer_charge(const npc* np); // 0x005F2F90 (:121)
void evaluate_evade_directions(ai_goal_setup_data& setup_data, const npc* np, const farray<vector,4>& candidate_directions, float total_distance); // 0x00687BF0 (:213)
const ai_belief* ai_get_enemy_flee_belief(const npc* np); // 0x00688800 (:420)
const ai_belief* ai_get_flee_to_safety_belief(const npc* np); // 0x0066DC10 (:518)
bool collect_nearby_EDF(object* obj, unsigned int shape_key, const void* cb_data); // 0x005F3280 (:1557)

inline ai_goal* flee_debris_goal::get_instance()
{
    return &the_instance;
}

inline ai_goal* flee_enemies_goal::get_instance()
{
    return &the_instance;
}

inline ai_goal* flee_from_vehicle_goal::get_instance()
{
    return &the_instance;
}

inline ai_goal* flee_to_safety_goal::get_instance()
{
    return &the_instance;
}

inline ai_goal* flee_unsafe_building_goal::get_instance()
{
    return &the_instance;
}
