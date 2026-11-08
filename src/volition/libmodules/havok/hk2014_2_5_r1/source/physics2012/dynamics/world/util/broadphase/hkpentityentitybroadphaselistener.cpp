// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/broadphase/hkpentityentitybroadphaselistener.cpp
//
// Havok 2014.2.5 (havok_impl.md HK4): creates and removes the collision agent of an entity pair when the broadphase
// reports it, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/broadphase/hkpentityentitybroadphaselistener.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionqualityinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkpagentnnmachine.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkplinkedcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasehandlepair.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldagentutil.h"

// 0x00E200D0
hkpEntityEntityBroadPhaseListener::hkpEntityEntityBroadPhaseListener(hkpWorld* world)
{
    m_world = world;
}

// 0x00E20190
hkpEntityEntityBroadPhaseListener::~hkpEntityEntityBroadPhaseListener()
{
}

// 0x00E20100: an agent unless the pair's collision quality is invalid (0); the quality decides whether the agent
// is predictive (continuous physics).
void hkpEntityEntityBroadPhaseListener::addCollisionPair(hkpTypedBroadPhaseHandlePair& pair)
{
    hkpLinkedCollidable* collA = (hkpLinkedCollidable*)pair.getElementA()->getOwner();
    hkpLinkedCollidable* collB = (hkpLinkedCollidable*)pair.getElementB()->getOwner();
    hkpProcessCollisionInput* input = m_world->m_collisionInput;
    hkpCollisionDispatcher* dispatcher = input->m_dispatcher.val();
    const signed char quality = dispatcher->m_collisionQualityTable[collA->m_broadPhaseHandle.m_objectQualityType][collB->m_broadPhaseHandle.m_objectQualityType];
    if (quality != 0)
    {
        input->m_createPredictiveAgents = dispatcher->m_collisionQualityInfo[quality].m_useContinuousPhysics.val();
        hkpWorldAgentUtil::addAgent(collA, collB, *input);
    }
}

// 0x00E20160
void hkpEntityEntityBroadPhaseListener::removeCollisionPair(hkpTypedBroadPhaseHandlePair& pair)
{
    hkpLinkedCollidable* collA = (hkpLinkedCollidable*)pair.getElementA()->getOwner();
    hkpLinkedCollidable* collB = (hkpLinkedCollidable*)pair.getElementB()->getOwner();
    hkpAgentNnEntry* agent = hkAgentNnMachine_FindAgent(collA, collB);
    if (agent != nullptr)
    {
        hkpWorldAgentUtil::removeAgent(agent);
    }
}
