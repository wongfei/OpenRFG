// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/list/hkplistagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the list agent, from rfg.exe: a body against the enabled children of a list
// shape B through a 1n agent track (the list as A: the symmetric agent around it). Its queries are the shape
// collection agent's with the bodies swapped.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/list/hkplistagent.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/shapecollection/hkpshapecollectionagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagentlinearcast.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/swepttransform/hkswepttransformutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/hkpagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1nmachine.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/list/hkplistshape.h"

// 0x00DBA380: the list as A (the shape collection agent's statics; flipped as the binary has it)
void hkpListAgent::initAgentFunc(hkpCollisionDispatcher::AgentFuncs& af)
{
    af.m_createFunc = createListAAgent;
    af.m_getPenetrationsFunc = hkpShapeCollectionAgent::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpShapeCollectionAgent::staticGetClosestPoints;
    af.m_linearCastFunc = hkpShapeCollectionAgent::staticLinearCast;
    af.m_isFlipped = true;
    af.m_isPredictive = true;
}

// 0x00DBA300: the list as B
void hkpListAgent::initAgentFuncInverse(hkpCollisionDispatcher::AgentFuncs& af)
{
    af.m_createFunc = createListBAgent;
    af.m_getPenetrationsFunc = hkpSymmetricAgentLinearCast<hkpShapeCollectionAgent>::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpSymmetricAgentLinearCast<hkpShapeCollectionAgent>::staticGetClosestPoints;
    af.m_linearCastFunc = hkpSymmetricAgentLinearCast<hkpShapeCollectionAgent>::staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = true;
}

// 0x00DBA3B0
void hkpListAgent::registerAgent(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::AgentFuncs af;
    initAgentFunc(af);
    dispatcher->registerCollisionAgent(af, hkcdShapeType::LIST, hkcdShapeType::ALL_SHAPE_TYPES);
    initAgentFuncInverse(af);
    dispatcher->registerCollisionAgent(af, hkcdShapeType::ALL_SHAPE_TYPES, hkcdShapeType::LIST);
}

// 0x00DBA270
hkpListAgent::hkpListAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* contactMgr)
    : hkpCollisionAgent(contactMgr)
{
    m_dispatcher = input.m_dispatcher.val();
    hkAgent1nMachine_Create(m_agentTrack);
}

// 0x00DBA430
hkpListAgent::~hkpListAgent()
{
}

// 0x00DBA330: the list as A, through the symmetric agent
hkpCollisionAgent* hkpListAgent::createListAAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* contactMgr)
{
    return new hkpSymmetricAgent<hkpListAgent>(bodyA, bodyB, input, contactMgr);
}

// 0x00DBA2C0
hkpCollisionAgent* hkpListAgent::createListBAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* contactMgr)
{
    return new hkpListAgent(bodyA, bodyB, input, contactMgr);
}

// 0x00DBA000: the list is B here (the shape collection agent, which takes the collection as A, with the bodies swapped)
void hkpListAgent::staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    hkpSymmetricAgentLinearCast<hkpShapeCollectionAgent>::staticGetClosestPoints(bodyA, bodyB, input, collector);
}

// 0x00DBA010
void hkpListAgent::staticLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
{
    hkpSymmetricAgentLinearCast<hkpShapeCollectionAgent>::staticLinearCast(bodyA, bodyB, input, collector, startCollector);
}

// 0x00DBA070
void hkpListAgent::staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    hkpSymmetricAgentLinearCast<hkpShapeCollectionAgent>::staticGetPenetrations(bodyA, bodyB, input, collector);
}

// 0x00DBA020
void hkpListAgent::getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    staticGetClosestPoints(bodyA, bodyB, input, collector);
}

// 0x00DBA040
void hkpListAgent::linearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
{
    staticLinearCast(bodyA, bodyB, input, collector, startCollector);
}

// 0x00DBA250
void hkpListAgent::getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    staticGetPenetrations(bodyA, bodyB, input, collector);
}

// 0x00DBA080: the keys of the enabled children of B (every child past the 256 the enable bits cover), through the 1n
// machine. The input's m_distAtT1 is left unset, as in the original.
void hkpListAgent::processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output)
{
    hkpAgent3ProcessInput in3;
    in3.m_bodyA = &bodyA;
    in3.m_bodyB = &bodyB;
    in3.m_overrideBodyA = nullptr;
    in3.m_contactMgr = m_contactMgr;
    in3.m_input = &input;
    const hkMotionState* msA = static_cast<const hkMotionState*>(bodyA.m_motion);
    const hkMotionState* msB = static_cast<const hkMotionState*>(bodyB.m_motion);
    hkSweptTransformUtil::calcTimInfo(*msA, *msB, input.m_stepInfo.m_deltaTime.val(), in3.m_linearTimInfo);
    in3.m_aTb.setMulInverseMul(msA->m_transform, msB->m_transform);

    const hkpListShape* list = static_cast<const hkpListShape*>(bodyB.getShape());
    const int numChildren = list->m_childInfo.getSize();
    hkLifoAllocator& stack = hkMemoryRouter::getInstance().stack();
    const int bytes = (numChildren * (int)sizeof(unsigned int) + (int)sizeof(unsigned int) + 127) & ~127; // hkAllocateStack: 128-byte multiples
    unsigned int* newKeys = static_cast<unsigned int*>(stack.fastBlockAlloc(bytes));
    int numKeys = 0;
    unsigned int bit = 1;
    for (int i = 0; i < numChildren; i++)
    {
        if (i > 255 || (list->m_enabledChildren[i >> 5] & bit) != 0)
        {
            newKeys[numKeys++] = (unsigned int)i;
        }
        bit = (bit << 1) | (bit >> 31);
    }
    newKeys[numKeys] = 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
    hkAgent1nMachine_Process(m_agentTrack, in3, list, newKeys, output);
    stack.fastBlockFree(newKeys, bytes);
}

// 0x00DB9F20: the track's agents, then the agent itself
void hkpListAgent::cleanup(hkpConstraintOwner& constraintOwner)
{
    hkAgent1nMachine_Destroy(m_agentTrack, m_dispatcher, m_contactMgr, constraintOwner);
    delete this;
}

// 0x00DB9FA0
void hkpListAgent::updateShapeCollectionFilter(const hkpCdBody& bodyA, const hkpCdBody& listShapeBodyB, const hkpCollisionInput& input, hkpConstraintOwner& constraintOwner)
{
    hkpAgent1nMachine_VisitorInput vin;
    vin.m_contactMgr = m_contactMgr;
    vin.m_input = &input;
    vin.m_bodyA = &bodyA;
    vin.m_constraintOwner = &constraintOwner;
    vin.m_collectionBodyB = &listShapeBodyB;
    vin.m_containerShapeB = listShapeBodyB.getShape()->getContainer();
    hkAgent1nMachine_UpdateShapeCollectionFilter(m_agentTrack, vin);
}

// 0x00DB9F50
void hkpListAgent::invalidateTim(const hkpCollisionInput& input)
{
    hkAgent1nMachine_InvalidateTim(m_agentTrack, input);
}

// 0x00DB9F70
void hkpListAgent::warpTime(float oldTime, float newTime, const hkpCollisionInput& input)
{
    hkAgent1nMachine_WarpTime(m_agentTrack, oldTime, newTime, input);
}
