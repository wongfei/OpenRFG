// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/miscagent/bv/hkpbvagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the bounding volume shape agent2, from rfg.exe. A is the BV shape: its bounding
// volume is tested against B first (penetration, or a linear cast hit); only then does the child (created lazily)
// collide with B. No SDK source exists for 2014.2.5; ported from the binary.
//
// The shipped build records the "LthkpBvAgent"/"StcheckBvShape"/"Stchild" monitor timers (TLS hkMonitorStream); they
// are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/miscagent/bv/hkpbvagent.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdbodypaircollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkplinearcastcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/bodypaircollector/hkpflagcdbodypaircollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkpsimpleclosestcontactcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/misc/bv/hkpbvshape.h"

static inline const hkpBvShape* hkBvAgent_shape(const hkpCdBody& body)
{
    return static_cast<const hkpBvShape*>(body.getShape());
}

static inline hkcdShapeType::ShapeTypeEnum hkBvAgent_type(const hkpCdBody& body)
{
    return static_cast<hkcdShapeType::ShapeTypeEnum>(body.getShape()->m_type.m_storage);
}

// The bounding volume of A's BV shape (key -1) and its child (key 0) as children of A.
static inline void hkBvAgent_setVolume(hkpCdBody& body, const hkpCdBody& bodyA)
{
    body.setShape(hkBvAgent_shape(bodyA)->m_boundingVolumeShape, 0xFFFFFFFF);
}

static inline void hkBvAgent_setChild(hkpCdBody& body, const hkpCdBody& bodyA)
{
    body.setShape(hkBvAgent_shape(bodyA)->m_childShape.m_childShape, 0);
}

// 0x00DCA350: any vs BV (the symmetric agent), BV vs any
void hkpBvAgent::registerAgent(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::AgentFuncs af;
    af.m_createFunc = createShapeBvAgent;
    af.m_getPenetrationsFunc = hkpSymmetricAgentLinearCast<hkpBvAgent>::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpSymmetricAgentLinearCast<hkpBvAgent>::staticGetClosestPoints;
    af.m_linearCastFunc = hkpSymmetricAgentLinearCast<hkpBvAgent>::staticLinearCast;
    af.m_isFlipped = true;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::ALL_SHAPE_TYPES, hkcdShapeType::BV);

    af.m_createFunc = createBvShapeAgent;
    af.m_getPenetrationsFunc = staticGetPenetrations;
    af.m_getClosestPointFunc = staticGetClosestPoints;
    af.m_linearCastFunc = staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::BV, hkcdShapeType::ALL_SHAPE_TYPES);
}

// 0x00DC9760: the bounding volume agent; the child agent comes with the first hit
hkpBvAgent::hkpBvAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
    : hkpCollisionAgent(mgr)
{
    hkpCdBody volumeBody(&bodyA);
    hkBvAgent_setVolume(volumeBody, bodyA);
    m_boundingVolumeAgent = input.m_dispatcher->getNewCollisionAgent(volumeBody, bodyB, input, mgr);
    m_childAgent = nullptr;
}

// 0x00DCA3D0
hkpBvAgent::~hkpBvAgent()
{
}

// 0x00DC9810
hkpCollisionAgent* hkpBvAgent::createBvShapeAgent(const hkpCdBody& A, const hkpCdBody& B, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpBvAgent(A, B, input, mgr);
}

// 0x00DCA300
hkpCollisionAgent* hkpBvAgent::createShapeBvAgent(const hkpCdBody& A, const hkpCdBody& B, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpSymmetricAgent<hkpBvAgent>(A, B, input, mgr);
}

// 0x00DC95B0
void hkpBvAgent::cleanup(hkpConstraintOwner& constraintOwner)
{
    m_boundingVolumeAgent->cleanup(constraintOwner);
    if (m_childAgent)
    {
        m_childAgent->cleanup(constraintOwner);
        m_childAgent = nullptr;
    }
    delete this;
}

// 0x00DC95F0
void hkpBvAgent::invalidateTim(const hkpCollisionInput& input)
{
    m_boundingVolumeAgent->invalidateTim(input);
    if (m_childAgent)
    {
        m_childAgent->invalidateTim(input);
    }
}

// 0x00DC9620
void hkpBvAgent::warpTime(float oldTime, float newTime, const hkpCollisionInput& input)
{
    m_boundingVolumeAgent->warpTime(oldTime, newTime, input);
    if (m_childAgent)
    {
        m_childAgent->warpTime(oldTime, newTime, input);
    }
}

// 0x00DC9680
void hkpBvAgent::removePoint(unsigned short idToRemove)
{
    if (m_childAgent)
    {
        m_childAgent->removePoint(idToRemove);
    }
}

// 0x00DC96A0
void hkpBvAgent::commitPotential(unsigned short newId)
{
    if (m_childAgent)
    {
        m_childAgent->commitPotential(newId);
    }
}

// 0x00DC96C0
void hkpBvAgent::createZombie(unsigned short idTobecomeZombie)
{
    if (m_childAgent)
    {
        m_childAgent->createZombie(idTobecomeZombie);
    }
}

// 0x00DC96E0
void hkpBvAgent::updateShapeCollectionFilter(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpConstraintOwner& constraintOwner)
{
    hkpCdBody body(&bodyA);
    hkBvAgent_setVolume(body, bodyA);
    m_boundingVolumeAgent->updateShapeCollectionFilter(body, bodyB, input, constraintOwner);
    if (m_childAgent)
    {
        hkBvAgent_setChild(body, bodyA);
        m_childAgent->updateShapeCollectionFilter(body, bodyB, input, constraintOwner);
    }
}

// 0x00DC9850: the child is processed while the bounding volume penetrates B; otherwise its agent is removed
void hkpBvAgent::processCollision(const hkpCdBody& bodyAin, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result)
{
    hkpCdBody body(&bodyAin);
    hkBvAgent_setVolume(body, bodyAin);
    hkpFlagCdBodyPairCollector flag;
    m_boundingVolumeAgent->getPenetrations(body, bodyB, input, flag);
    if (flag.m_earlyOut.m_bool)
    {
        hkBvAgent_setChild(body, bodyAin);
        if (m_childAgent == nullptr)
        {
            m_childAgent = input.m_dispatcher->getNewCollisionAgent(body, bodyB, input, m_contactMgr);
        }
        m_childAgent->processCollision(body, bodyB, input, result);
    }
    else if (m_childAgent)
    {
        m_childAgent->cleanup(*result.m_constraintOwner);
        m_childAgent = nullptr;
    }
}

// 0x00DC9D40
void hkpBvAgent::getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    hkpCdBody body(&bodyA);
    hkBvAgent_setVolume(body, bodyA);
    hkpFlagCdBodyPairCollector flag;
    m_boundingVolumeAgent->getPenetrations(body, bodyB, input, flag);
    if (flag.m_earlyOut.m_bool)
    {
        hkBvAgent_setChild(body, bodyA);
        if (m_childAgent == nullptr)
        {
            m_childAgent = input.m_dispatcher->getNewCollisionAgent(body, bodyB, input, m_contactMgr);
        }
        m_childAgent->getClosestPoints(body, bodyB, input, collector);
    }
}

// 0x00DCA020
void hkpBvAgent::getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    hkpCdBody body(&bodyA);
    hkBvAgent_setVolume(body, bodyA);
    hkpFlagCdBodyPairCollector flag;
    m_boundingVolumeAgent->getPenetrations(body, bodyB, input, flag);
    if (flag.m_earlyOut.m_bool)
    {
        hkBvAgent_setChild(body, bodyA);
        if (m_childAgent == nullptr)
        {
            m_childAgent = input.m_dispatcher->getNewCollisionAgent(body, bodyB, input, m_contactMgr);
        }
        m_childAgent->getPenetrations(body, bodyB, input, collector);
    }
}

// 0x00DC99F0: the bounding volume cast first (a simple closest contact collector for both collectors)
void hkpBvAgent::linearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& pointDetails, hkpCdPointCollector* startCollector)
{
    hkpCdBody body(&bodyA);
    hkBvAgent_setVolume(body, bodyA);
    hkpSimpleClosestContactCollector hit;
    m_boundingVolumeAgent->linearCast(body, bodyB, input, hit, &hit);
    if (hit.m_hasHit.m_bool)
    {
        hkBvAgent_setChild(body, bodyA);
        if (m_childAgent == nullptr)
        {
            m_childAgent = input.m_dispatcher->getNewCollisionAgent(body, bodyB, input, m_contactMgr);
        }
        m_childAgent->linearCast(body, bodyB, input, pointDetails, startCollector);
    }
}

// 0x00DC9EC0
void hkpBvAgent::staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    hkpCdBody body(&bodyA);
    hkBvAgent_setVolume(body, bodyA);
    const hkcdShapeType::ShapeTypeEnum typeB = hkBvAgent_type(bodyB);
    hkpFlagCdBodyPairCollector flag;
    const hkpCollisionDispatcher* dispatcher = input.m_dispatcher;
    dispatcher->getGetPenetrationsFunc(hkBvAgent_type(body), typeB)(body, bodyB, input, flag);
    if (flag.m_earlyOut.m_bool)
    {
        hkBvAgent_setChild(body, bodyA);
        dispatcher->getGetClosestPointsFunc(hkBvAgent_type(body), typeB)(body, bodyB, input, collector);
    }
}

// 0x00DCA1A0
void hkpBvAgent::staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    hkpCdBody body(&bodyA);
    hkBvAgent_setVolume(body, bodyA);
    const hkcdShapeType::ShapeTypeEnum typeB = hkBvAgent_type(bodyB);
    hkpFlagCdBodyPairCollector flag;
    const hkpCollisionDispatcher* dispatcher = input.m_dispatcher;
    dispatcher->getGetPenetrationsFunc(hkBvAgent_type(body), typeB)(body, bodyB, input, flag);
    if (flag.m_earlyOut.m_bool)
    {
        hkBvAgent_setChild(body, bodyA);
        dispatcher->getGetPenetrationsFunc(hkBvAgent_type(body), typeB)(body, bodyB, input, collector);
    }
}

// 0x00DC9BB0
void hkpBvAgent::staticLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& pointDetails, hkpCdPointCollector* startCollector)
{
    hkpCdBody body(&bodyA);
    hkBvAgent_setVolume(body, bodyA);
    const hkcdShapeType::ShapeTypeEnum typeB = hkBvAgent_type(bodyB);
    hkpSimpleClosestContactCollector hit;
    const hkpCollisionDispatcher* dispatcher = input.m_dispatcher;
    dispatcher->getLinearCastFunc(hkBvAgent_type(body), typeB)(body, bodyB, input, hit, &hit);
    if (hit.m_hasHit.m_bool)
    {
        hkBvAgent_setChild(body, bodyA);
        dispatcher->getLinearCastFunc(hkBvAgent_type(body), typeB)(body, bodyB, input, pointDetails, startCollector);
    }
}
