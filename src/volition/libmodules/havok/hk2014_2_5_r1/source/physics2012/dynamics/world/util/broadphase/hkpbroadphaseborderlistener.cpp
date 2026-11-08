// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/broadphase/hkpbroadphaseborderlistener.cpp
//
// Havok 2014.2.5 (havok_impl.md HK4): the broadphase border's phantoms (handle type BROAD_PHASE_BORDER = 3) learn the
// collidables that touch them, from rfg.exe. Pairs of two border phantoms are ignored.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/broadphase/hkpbroadphaseborderlistener.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasehandlepair.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantom.h"

static const int HK_BROAD_PHASE_BORDER = 3; // hkpWorldObject::BroadPhaseType

// 0x00E16340
void hkpBroadPhaseBorderListener::addCollisionPair(hkpTypedBroadPhaseHandlePair& pair)
{
    hkpCollidable* collA = (hkpCollidable*)pair.getElementA()->getOwner();
    hkpCollidable* collB = (hkpCollidable*)pair.getElementB()->getOwner();
    if (pair.getElementA()->m_type == HK_BROAD_PHASE_BORDER)
    {
        if (pair.getElementB()->m_type == HK_BROAD_PHASE_BORDER)
        {
            return;
        }
        ((hkpPhantom*)collA->getOwner())->addOverlappingCollidable(collB);
    }
    if (pair.getElementB()->m_type == HK_BROAD_PHASE_BORDER)
    {
        ((hkpPhantom*)collB->getOwner())->addOverlappingCollidable(collA);
    }
}

// 0x00E163B0
void hkpBroadPhaseBorderListener::removeCollisionPair(hkpTypedBroadPhaseHandlePair& pair)
{
    hkpCollidable* collA = (hkpCollidable*)pair.getElementA()->getOwner();
    hkpCollidable* collB = (hkpCollidable*)pair.getElementB()->getOwner();
    if (pair.getElementA()->m_type == HK_BROAD_PHASE_BORDER)
    {
        if (pair.getElementB()->m_type == HK_BROAD_PHASE_BORDER)
        {
            return;
        }
        ((hkpPhantom*)collA->getOwner())->removeOverlappingCollidable(collB);
    }
    if (pair.getElementB()->m_type == HK_BROAD_PHASE_BORDER)
    {
        ((hkpPhantom*)collB->getOwner())->removeOverlappingCollidable(collA);
    }
}

// 0x00DE87D0
hkpBroadPhaseBorderListener::hkpBroadPhaseBorderListener()
{
}

// 0x00DE87F0
hkpBroadPhaseBorderListener::~hkpBroadPhaseBorderListener()
{
}
