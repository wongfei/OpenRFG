// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantombroadphaselistener.cpp
//
// Havok 2014.2.5 (havok_impl.md HK4): phantoms (handle type BROAD_PHASE_PHANTOM = 2) learn the collidables that
// overlap them, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantombroadphaselistener.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasehandlepair.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantom.h"

static const int HK_BROAD_PHASE_PHANTOM = 2; // hkpWorldObject::BroadPhaseType

// 0x00E16280
void hkpPhantomBroadPhaseListener::addCollisionPair(hkpTypedBroadPhaseHandlePair& pair)
{
    hkpCollidable* collA = (hkpCollidable*)pair.getElementA()->getOwner();
    hkpCollidable* collB = (hkpCollidable*)pair.getElementB()->getOwner();
    if (pair.getElementA()->m_type == HK_BROAD_PHASE_PHANTOM)
    {
        ((hkpPhantom*)collA->getOwner())->addOverlappingCollidable(collB);
    }
    if (pair.getElementB()->m_type == HK_BROAD_PHASE_PHANTOM)
    {
        ((hkpPhantom*)collB->getOwner())->addOverlappingCollidable(collA);
    }
}

// 0x00E162E0
void hkpPhantomBroadPhaseListener::removeCollisionPair(hkpTypedBroadPhaseHandlePair& pair)
{
    hkpCollidable* collA = (hkpCollidable*)pair.getElementA()->getOwner();
    hkpCollidable* collB = (hkpCollidable*)pair.getElementB()->getOwner();
    if (pair.getElementA()->m_type == HK_BROAD_PHASE_PHANTOM)
    {
        ((hkpPhantom*)collA->getOwner())->removeOverlappingCollidable(collB);
    }
    if (pair.getElementB()->m_type == HK_BROAD_PHASE_PHANTOM)
    {
        ((hkpPhantom*)collB->getOwner())->removeOverlappingCollidable(collA);
    }
}

// 0x00DE87A0
hkpPhantomBroadPhaseListener::hkpPhantomBroadPhaseListener()
{
}

// 0x00DE87C0
hkpPhantomBroadPhaseListener::~hkpPhantomBroadPhaseListener()
{
}
