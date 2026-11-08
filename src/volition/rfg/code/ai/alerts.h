// Original: volition/rfg/code/ai/alerts.h
// Implementation: volition/rfg/code/ai/alerts.cpp
//
// Alerts: the shouts NPCs hear from each other (threats, disturbances, enemies, unfriendlies, projectiles, movers,
// vehicles, collapsing buildings, corpses), npc_sound_info subclasses that decide whether a listener cares and inform
// it.
#pragma once

#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/objects/humans/npc_sound_info.h"

class human;
class npc;
class object_mover;
class projectile;
class rfg_mover;
class vehicle;

// size 0x14
class alert_generic : public npc_sound_info
{
public:
    alert_generic(const alert_generic& __that); // 0x005F9D40
    alert_generic(const npc* _source); // 0x005DF640 (:199)
protected:
    virtual bool is_relevant_to_npc(npc* listener) const; // vtbl+0x0C 0x0060C5F0 (:33)
public:
    virtual ~alert_generic(); // vtbl+0x00 0x005DF660

};

// size 0x18
class alert_shout_threaten : public npc_sound_info
{
public:
    alert_shout_threaten(const alert_shout_threaten& __that); // 0x0064B0F0
    alert_shout_threaten(const npc* source_, const human* target_); // 0x005DF790 (:290)
protected:
    virtual void propagate(int inst); // vtbl+0x04 0x0060C690 (:180)
    virtual bool is_relevant_to_npc(npc* listener) const; // vtbl+0x0C 0x0060C640 (:42)
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x00674990 (:157)
    virtual float get_default_sound_distance() const; // vtbl+0x10 0x005DF620 (:60)
public:
    virtual ~alert_shout_threaten(); // vtbl+0x00 0x005DF7C0

protected:
    unsigned int threatener; // 0x14
};

// size 0x14
class alert_shout : public alert_generic
{
public:
    alert_shout(const alert_shout& __that); // 0x0064B1A0
    alert_shout(const npc* _source); // 0x005DF670 (:204)
    const npc* get_npc_source() const;
protected:
    virtual float get_default_sound_distance() const; // vtbl+0x10 0x005DF610 (:54)
public:
    virtual ~alert_shout(); // vtbl+0x00 0x005DF690

};

// size 0x3C
class alert_shout_disturbance : public alert_shout
{
public:
    alert_shout_disturbance(const alert_shout_disturbance& __that); // 0x00673100
    alert_shout_disturbance(const npc* _source, const ai_belief* source_belief); // 0x0060C720 (:209)
protected:
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x006BBCD0 (:67)
public:
    virtual ~alert_shout_disturbance(); // vtbl+0x00 0x005DF6B0

protected:
    ai_belief belief; // 0x14
};

// size 0x18
class alert_shout_enemy : public alert_shout
{
public:
    alert_shout_enemy(const alert_shout_enemy& __that); // 0x006731B0
    alert_shout_enemy(const npc* _source, const human* _enemy); // 0x005DF7D0 (:299)
    alert_shout_enemy(const npc* _source); // 0x0060C7F0 (:233)
    const human* get_enemy() const;
protected:
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x006BBCF0 (:80)
public:
    virtual ~alert_shout_enemy(); // vtbl+0x00 0x005DF740

protected:
    unsigned int spotted_enemy; // 0x14
};

// size 0x18
class alert_shout_unfriendly : public alert_shout
{
public:
    alert_shout_unfriendly(const alert_shout_unfriendly& __that); // 0x006731F0
    alert_shout_unfriendly(const npc* _source, const human* target); // 0x005DF6C0 (:217)
protected:
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x00674720 (:90)
public:
    virtual ~alert_shout_unfriendly(); // vtbl+0x00 0x005DF6F0

protected:
    unsigned int spotted_target; // 0x14
};

// size 0x18
class alert_shout_projectile : public alert_shout
{
public:
    alert_shout_projectile(const npc* _source, const projectile* _incoming); // 0x005DF800 (:305)
    alert_shout_projectile(const npc* _source); // 0x0060C860 (:243)
    const projectile* get_projectile() const;
protected:
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x006747D0 (:103)
public:
    virtual ~alert_shout_projectile(); // vtbl+0x00 0x005DF830

protected:
    unsigned int threatening_projectile; // 0x14
};

// size 0x18
class alert_shout_mover : public alert_shout
{
public:
    alert_shout_mover(const npc* _source, const object_mover* _incoming); // 0x005DF840 (:310)
    alert_shout_mover(const npc* _source); // 0x0060C8D0 (:252)
    const object_mover* get_mover() const;
protected:
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x00674840 (:113)
public:
    virtual ~alert_shout_mover(); // vtbl+0x00 0x005DF870

protected:
    unsigned int mover; // 0x14
};

// size 0x18
class alert_shout_vehicle : public alert_shout
{
public:
    alert_shout_vehicle(const npc* _source, const vehicle* _vehicle); // 0x005DF880 (:315)
    alert_shout_vehicle(const npc* _source); // 0x0060C940 (:261)
    const vehicle* get_vehicle() const;
protected:
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x006748B0 (:123)
public:
    virtual ~alert_shout_vehicle(); // vtbl+0x00 0x005DF8B0

protected:
    unsigned int rogue_vehicle; // 0x14
};

// size 0x18
class alert_shout_collapsing_building : public alert_shout
{
public:
    alert_shout_collapsing_building(const npc* _source, const rfg_mover* building); // 0x005DF8C0 (:320)
    alert_shout_collapsing_building(const npc* _source); // 0x0060C9B0 (:270)
    const rfg_mover* get_building() const;
protected:
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x00674920 (:133)
public:
    virtual ~alert_shout_collapsing_building(); // vtbl+0x00 0x005DF8F0

protected:
    unsigned int falling_building; // 0x14
};

// size 0x1C
class alert_shout_corpse : public alert_shout
{
public:
    alert_shout_corpse(const alert_shout_corpse& __that); // 0x00673170
    alert_shout_corpse(const npc* source_, const human* corpse_, const human* killer_); // 0x005DF750 (:279)
protected:
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x006C1E20 (:143)
public:
    virtual ~alert_shout_corpse(); // vtbl+0x00 0x005DF780

protected:
    unsigned int corpse; // 0x14
    unsigned int killer; // 0x18
};

// size 0x14
class meaningless_shout : public alert_shout
{
public:
    meaningless_shout(const npc* _source); // 0x005DF700 (:223)
protected:
    virtual void propagate(int inst); // vtbl+0x04 0x005DF630 (:172)
    virtual void inform_npc(npc* listener) const; // vtbl+0x08 0x005DF720 (.h:92)
public:
    virtual ~meaningless_shout(); // vtbl+0x00 0x005DF730

};
