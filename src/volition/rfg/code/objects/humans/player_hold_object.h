// Original: volition/rfg/code/objects/humans/player_hold_object.h
// Implementation: volition/rfg/code/objects/humans/player_hold_object.cpp
//
// Objects the player picks up and carries: PlayerHoldAction is the Havok action that drives the held body to the hold
// position every step.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/action/hkpunaryaction.h"
#include "volition/vlib/code/time/timestamp.h"

struct hkContainerHeapAllocator;
class hkStepInfo;
class hkpAction;
class hkpEntity;
class hkpPhantom;
class hkpRigidBody;

// size 0x1C
class PlayerHoldAction : public hkpUnaryAction
{
public:
    PlayerHoldAction(hkpRigidBody* body); // 0x00A115C0 (:93)
    PlayerHoldAction();
    virtual ~PlayerHoldAction(); // vtbl+0x00 0x00A115E0 (:95)
    virtual hkpAction* clone(const hkArray<hkpEntity*,hkContainerHeapAllocator>& newEntities, const hkArray<hkpPhantom*,hkContainerHeapAllocator>& newPhantoms) const; // vtbl+0x20 0x00A115F0 (:98)
    virtual void applyAction(const hkStepInfo& stepInfo); // vtbl+0x10 0x00A34F80 (:101)

};

void player_hold_object_pickup(); // 0x00ABE950 (:276)
void player_hold_object_process(); // 0x00AC35E0 (:394)
void player_hold_object_level_init(); // 0x00A11920 (:481)
void player_hold_render_debug(); // 0x00A11930 (:486)

extern timestamp_percent Player_hold_throw_timestamp; // 0x01695634, a throw blocks melee attacks until it elapses
