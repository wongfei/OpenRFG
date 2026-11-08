// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasehandlepair.h
//
// Hand-written (havok_impl.md HK4, Havok 2014.1 hkpTypedBroadPhaseHandlePair.h/.inl): a broadphase pair whose handles
// are typed (the dispatcher's listener table is indexed by the two handle types).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphasehandlepair.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasehandle.h"

// size 0x8
class hkpTypedBroadPhaseHandlePair : public hkpBroadPhaseHandlePair
{
public:
    hkpTypedBroadPhaseHandle* getElementA() const { return static_cast<hkpTypedBroadPhaseHandle*>(m_a); } // 0x00D9A680 (.inl:10)
    hkpTypedBroadPhaseHandle* getElementB() const { return static_cast<hkpTypedBroadPhaseHandle*>(m_b); } // 0x00D9A690 (.inl:15)
};
