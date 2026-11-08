// Original: volition/rfg/code/objects/humans/npc_sound_info.h
// Implementation: volition/rfg/code/objects/humans/npc_sound_info.cpp
//
// Sounds NPCs can hear: a sound event (pain, death, gunfire, bullet strikes, building damage and collapse, vehicle
// damage, footsteps, charges placed, stress) is described by an npc_sound_info subclass that decides whether it is
// relevant to a listening NPC, how far it carries by default, and informs the NPC (alerting or scaring it).
// The copy constructors are compiler generated.
#pragma once

#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/lockfree_queues.h"

class async_mempool;
class human;
class npc;
class object;
class player;
class projectile;
class rfg_mover;
class vehicle;
struct weapon_info;

// size 0x14
class npc_sound_info
{
public:
    npc_sound_info(const object* source); // 0x00A0FAD0 (:410)
    virtual ~npc_sound_info(); // vtbl+0x00 0x005DD8E0 (.h:37)
    const object* get_source() const; // 0x00A0FF00 (:606)
    static void level_init_pre(); // 0x00A0FF10 (:618)
    static void send_queued(); // 0x00A0FF50 (:633)
    template<typename T> static void queue(const T& sound_info); // npc_sound_info.inl:5, one instance per info type
protected:
    virtual void propagate(int inst); // vtbl+0x04 0x00A83DE0 (:541)
    virtual void inform_npc(npc*) const = 0; // vtbl+0x08
    virtual bool is_relevant_to_npc(npc* listener) const; // vtbl+0x0C 0x00A0F950 (:85)
    virtual float get_default_sound_distance() const; // vtbl+0x10 0x00A0F9B0 (:193)
    virtual vector get_sound_source_location() const; // vtbl+0x14 0x00A33920 (:382)

    // 0x00 vfptr
public:
    lf_node<npc_sound_info*> lfn; // 0x04
protected:
    unsigned int sound_source; // 0x10
private:
    static unsigned char queue_memory[10240]; // 0x030269D0
    static async_mempool queue_mempool; // 0x03043A80
    static slist_lock_free_stack queued_infos; // 0x016955D0
};

// size 0x1C
class npc_sound_info_pain : public npc_sound_info
{
public:
    npc_sound_info_pain(const human* _target, const human* _attacker, const weapon_info* _w_info); // 0x00A0FD50 (:451)
    const human* get_target() const; // 0x00A33B50 (:520)
    const human* get_attacker() const; // 0x00A33B70 (:525)
    const weapon_info* get_weapon_info() const;
protected:
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x00A336F0 (:329)
    virtual float get_default_sound_distance() const; // vtbl+0x10 0x00A0F9C0 (:198)
    virtual bool is_relevant_to_npc(npc* listener) const; // vtbl+0x0C 0x00A0F970 (:91)
public:
    virtual ~npc_sound_info_pain(); // vtbl+0x00 0x005EE380

protected:
    unsigned int attacker; // 0x14
    const weapon_info* w_info; // 0x18
};

// size 0x28
class npc_sound_info_building_damage : public npc_sound_info
{
public:
    npc_sound_info_building_damage(const rfg_mover* building, const human* attacker_, const vector& damage_pos_, float damage_amount_); // 0x00A0FB80 (:426)
    const rfg_mover* get_building() const; // 0x00A33A70 (:490)
    const human* get_attacker() const; // 0x00A33AA0 (:495)
    const vector& get_damage_pos() const;
    const float get_damage_amount() const;
protected:
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x00A5B310 (:278)
    virtual float get_default_sound_distance() const; // vtbl+0x10 0x00A0FA10 (:223)
    virtual vector get_sound_source_location() const; // vtbl+0x14 0x00A0FA90 (:393)
    virtual bool is_relevant_to_npc(npc* listener) const; // vtbl+0x0C 0x00A5B020 (:98)
public:
    virtual ~npc_sound_info_building_damage(); // vtbl+0x00 0x00716A40

protected:
    unsigned int attacker; // 0x14
    vector damage_pos; // 0x18
    float damage_amount; // 0x24
};

// size 0x30
class npc_sound_info_vehicle_damage : public npc_sound_info
{
public:
    npc_sound_info_vehicle_damage(const vehicle* target, const human* driver_, const human* attacker_, const vector& damage_pos_, float damage_amount_, bool was_vehicle_); // 0x00A0FC50 (:441)
    const vehicle* get_target() const; // 0x00A33AF0 (:505)
    const human* get_driver() const; // 0x00A33B10 (:510)
    const human* get_attacker() const; // 0x00A33B30 (:515)
    const vector& get_damage_pos() const;
    const float get_damage_amount() const;
protected:
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x00A5B3C0 (:305)
    virtual float get_default_sound_distance() const; // vtbl+0x10 0x00A0FA20 (:240)
    virtual vector get_sound_source_location() const; // vtbl+0x14 0x00A0FAB0 (:398)
    virtual bool is_relevant_to_npc(npc* listener) const; // vtbl+0x0C 0x00A5B120 (:138)
public:
    virtual ~npc_sound_info_vehicle_damage(); // vtbl+0x00 0x00A0FCC0

protected:
    unsigned int attacker; // 0x14
    unsigned int driver; // 0x18
    vector damage_pos; // 0x1C
    float damage_amount; // 0x28
    bool was_vehicle; // 0x2C
};

// size 0x30
class npc_sound_info_unimportant_vehicle_damage : public npc_sound_info_vehicle_damage
{
public:
    npc_sound_info_unimportant_vehicle_damage(const vehicle* target, const human* driver_, const human* attacker_, const vector& damage_pos_, float damage_amount_, bool was_vehicle_); // 0x00A0FCD0 (:446)
protected:
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x00A5B440 (:315)
    virtual bool is_relevant_to_npc(npc* listener) const; // vtbl+0x0C 0x00A0F990 (:173)
public:
    virtual ~npc_sound_info_unimportant_vehicle_damage(); // vtbl+0x00 0x00A0FD40

};

// size 0x14
class npc_sound_info_footsteps : public npc_sound_info
{
public:
    npc_sound_info_footsteps(player* pp); // 0x00A0FE30 (:471)
protected:
    virtual float get_default_sound_distance() const; // vtbl+0x10 0x00A0FA60 (:250)
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x00A33860 (:361)
    virtual bool is_relevant_to_npc(npc* listener) const; // vtbl+0x0C 0x00A33560 (:178)
public:
    virtual ~npc_sound_info_footsteps(); // vtbl+0x00 0x00A06D20

};

// size 0x18
class npc_sound_info_bomb_set : public npc_sound_info
{
public:
    npc_sound_info_bomb_set(projectile* proj, const human* _owner); // 0x00A0FE70 (:476)
    const human* get_owner() const; // 0x00A33B90 (:530)
protected:
    virtual float get_default_sound_distance() const; // vtbl+0x10 0x00A335E0 (:255)
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x00A338A0 (:368)
    virtual bool is_relevant_to_npc(npc* listener) const; // vtbl+0x0C 0x00A5B1D0 (:184)
public:
    virtual ~npc_sound_info_bomb_set(); // vtbl+0x00 0x00A0E660

protected:
    unsigned int owner; // 0x14
};

// size 0x1C
class npc_sound_info_death : public npc_sound_info_pain
{
public:
    npc_sound_info_death(const human* _target, const human* _attacker, const weapon_info* _w_info); // 0x00A0FD80 (:456)
protected:
    virtual float get_default_sound_distance() const; // vtbl+0x10 0x00A0F9D0 (:203)
public:
    virtual ~npc_sound_info_death(); // vtbl+0x00 0x00A2DAA0

};

// size 0x1C
class npc_sound_info_silent_death : public npc_sound_info_pain
{
public:
    npc_sound_info_silent_death(const human* _target, const human* _attacker, const weapon_info* _w_info); // 0x00A0FDC0 (:461)
protected:
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x00A33760 (:339)
    virtual float get_default_sound_distance() const; // vtbl+0x10 0x00A0F9E0 (:208)
public:
    virtual ~npc_sound_info_silent_death(); // vtbl+0x00 0x00A2DA90

};

// size 0x18
class npc_sound_info_firearm : public npc_sound_info
{
public:
    npc_sound_info_firearm(const human* _attacker, const weapon_info* _w_info); // 0x00A0FAF0 (:416)
    const human* get_attacker() const; // 0x00A33A50 (:485)
    const weapon_info* get_weapon_info() const;
protected:
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x00A33620 (:264)
    virtual float get_default_sound_distance() const; // vtbl+0x10 0x00A0F9F0 (:213)
public:
    virtual ~npc_sound_info_firearm(); // vtbl+0x00 0x00A0FB20

protected:
    const weapon_info* w_info; // 0x14
};

// size 0x24
class npc_sound_info_bullet_strike : public npc_sound_info_firearm
{
public:
    npc_sound_info_bullet_strike(const human* _attacker, const weapon_info* _w_info, const vector& pos); // 0x00A0FB30 (:421)
protected:
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x00A33660 (:271)
    virtual float get_default_sound_distance() const; // vtbl+0x10 0x00A0FA00 (:218)
    virtual vector get_sound_source_location() const; // vtbl+0x14 0x00A0FA70 (:388)
public:
    virtual ~npc_sound_info_bullet_strike(); // vtbl+0x00 0x00A0FB70

protected:
    vector impact_pos; // 0x18
};

// size 0x14
class npc_sound_info_building_collapse : public npc_sound_info
{
public:
    npc_sound_info_building_collapse(const rfg_mover* building); // 0x00A0FC30 (:436)
    const rfg_mover* get_building() const; // 0x00A33AC0 (:500)
protected:
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x00A5B380 (:296)
    virtual float get_default_sound_distance() const; // vtbl+0x10 0x00A5B210 (:228)
public:
    virtual ~npc_sound_info_building_collapse(); // vtbl+0x00 0x00716A30

};

// size 0x18
class npc_sound_info_stress : public npc_sound_info
{
public:
    npc_sound_info_stress(rfg_mover* stressed_object, float stress_amount_); // 0x00A0FE00 (:466)
protected:
    virtual float get_default_sound_distance() const; // vtbl+0x10 0x00A0FA50 (:245)
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x00A33810 (:354)
public:
    virtual ~npc_sound_info_stress(); // vtbl+0x00 0x007186C0

protected:
    float stress_amount; // 0x14
};

// size 0x28
class npc_sound_info_unimportant_building_damage : public npc_sound_info_building_damage
{
public:
    npc_sound_info_unimportant_building_damage(const rfg_mover* building, const human* attacker_, const vector& damage_pos_, float damage_amount_); // 0x00A0FBD0 (:431)
protected:
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x00A336B0 (:287)
public:
    virtual ~npc_sound_info_unimportant_building_damage(); // vtbl+0x00 0x00A0FC20

};

// size 0x18
class npc_sound_info_charge_placed : public npc_sound_info_bomb_set
{
public:
    npc_sound_info_charge_placed(projectile* proj, const human* _owner); // 0x00A0FEC0 (:481)
protected:
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x00A338E0 (:375)
public:
    virtual ~npc_sound_info_charge_placed(); // vtbl+0x00 0x00A0FEF0

};

#include "volition/rfg/code/objects/humans/npc_sound_info.inl"
