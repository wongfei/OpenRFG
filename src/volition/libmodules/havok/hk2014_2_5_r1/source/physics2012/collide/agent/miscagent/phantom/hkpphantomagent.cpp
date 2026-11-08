// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/miscagent/phantom/hkpphantomagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the phantom callback shape agent, from rfg.exe. It creates no contact points: a
// phantom callback shape on either side gets enter/leave events with the root collidables. No SDK source exists for
// 2014.2.5; ported from the binary.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/miscagent/phantom/hkpphantomagent.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdbodypaircollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/misc/phantomcallback/hkpphantomcallbackshape.h"

// 0x00DC6130: phantom callback shapes against every shape type, both ways (predictive, so the pair is always created)
void hkpPhantomAgent::registerAgent(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::AgentFuncs af;
    af.m_createFunc = createPhantomAgent;
    af.m_getPenetrationsFunc = staticGetPenetrations;
    af.m_getClosestPointFunc = staticGetClosestPoints;
    af.m_linearCastFunc = staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::PHANTOM_CALLBACK, hkcdShapeType::ALL_SHAPE_TYPES);
    dispatcher->registerCollisionAgent(af, hkcdShapeType::ALL_SHAPE_TYPES, hkcdShapeType::PHANTOM_CALLBACK);
}

// 0x00DC5F60: the root collidables and the shape types (the phantom shapes are set by createPhantomAgent)
hkpPhantomAgent::hkpPhantomAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, hkpContactMgr* contactMgr)
    : hkpCollisionAgent(contactMgr)
{
    m_collidableA = bodyA.getRootCollidable();
    m_collidableB = bodyB.getRootCollidable();
    m_bodyTypeA = static_cast<hkcdShapeType::ShapeTypeEnum>(bodyA.getShape()->m_type.m_storage);
    m_bodyTypeB = static_cast<hkcdShapeType::ShapeTypeEnum>(bodyB.getShape()->m_type.m_storage);
}

// 0x00DC61B0
hkpPhantomAgent::~hkpPhantomAgent()
{
}

// 0x00DC5FD0: the enter events
hkpCollisionAgent* hkpPhantomAgent::createPhantomAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* contactMgr)
{
    hkpPhantomAgent* agent = new hkpPhantomAgent(bodyA, bodyB, contactMgr);
    if (agent->m_bodyTypeA == hkcdShapeType::PHANTOM_CALLBACK)
    {
        hkpPhantomCallbackShape* shape = const_cast<hkpPhantomCallbackShape*>(static_cast<const hkpPhantomCallbackShape*>(bodyA.getShape()));
        shape->phantomEnterEvent(bodyA.getRootCollidable(), bodyB.getRootCollidable(), input);
        agent->m_shapeA = shape;
    }
    if (agent->m_bodyTypeB == hkcdShapeType::PHANTOM_CALLBACK)
    {
        hkpPhantomCallbackShape* shape = const_cast<hkpPhantomCallbackShape*>(static_cast<const hkpPhantomCallbackShape*>(bodyB.getShape()));
        shape->phantomEnterEvent(bodyB.getRootCollidable(), bodyA.getRootCollidable(), input);
        agent->m_shapeB = shape;
    }
    return agent;
}

// 0x00DC5E80: the leave events, then the agent deleted
void hkpPhantomAgent::cleanup(hkpConstraintOwner& constraintOwner)
{
    if (m_bodyTypeA == hkcdShapeType::PHANTOM_CALLBACK)
    {
        m_shapeA->phantomLeaveEvent(m_collidableA, m_collidableB);
    }
    if (m_bodyTypeB == hkcdShapeType::PHANTOM_CALLBACK)
    {
        m_shapeB->phantomLeaveEvent(m_collidableB, m_collidableA);
    }
    delete this;
}

// 0x00DC5ED0
void hkpPhantomAgent::processCollision(const hkpCdBody& bodyAin, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result)
{
}

// 0x00DC5EE0: the pair always penetrates
void hkpPhantomAgent::getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    collector.addCdBodyPair(bodyA, bodyB);
}

// 0x00DC5F00
void hkpPhantomAgent::staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    collector.addCdBodyPair(bodyA, bodyB);
}

// 0x00DC5F20
void hkpPhantomAgent::getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& pointDetails)
{
}

// 0x00DC5F30
void hkpPhantomAgent::staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& pointDetails)
{
}

// 0x00DC5F40
void hkpPhantomAgent::linearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
{
}

// 0x00DC5F50
void hkpPhantomAgent::staticLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
{
}
