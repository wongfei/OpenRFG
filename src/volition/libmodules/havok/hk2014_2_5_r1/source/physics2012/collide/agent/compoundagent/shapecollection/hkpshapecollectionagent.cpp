// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/shapecollection/hkpshapecollectionagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the shape collection agent2, from rfg.exe. A is the collection (B's flipped
// pairs use hkpSymmetricAgent): one child agent per child the filter enables, the statics iterate all children. No
// SDK source exists for 2014.2.5; ported from the binary.
//
// The shipped build records the "TtShapeCollection" monitor timers (TLS hkMonitorStream); they are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/shapecollection/hkpshapecollectionagent.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdbodypaircollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkplinearcastcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapebuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"

static inline hkcdShapeType::ShapeTypeEnum hkShapeCollectionAgent_type(const hkpCdBody& body)
{
    return static_cast<hkcdShapeType::ShapeTypeEnum>(body.getShape()->m_type.m_storage);
}

// The filter for a child of A's collection against B (B first).
static inline bool hkShapeCollectionAgent_isEnabled(const hkpCollisionInput& input, const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpShapeContainer& container, unsigned int key)
{
    return input.m_filter.val()->isCollisionEnabled(input, bodyB, bodyA, container, key).m_bool != 0;
}

// 0x00DC7FB0: any vs collection (the symmetric agent), collection vs any; for both the collection and the sampled
// heightfield collection types (18 and 6)
void hkpShapeCollectionAgent::registerAgent(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::AgentFuncs af;
    af.m_createFunc = createListBAgent;
    af.m_getPenetrationsFunc = hkpSymmetricAgentLinearCast<hkpShapeCollectionAgent>::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpSymmetricAgentLinearCast<hkpShapeCollectionAgent>::staticGetClosestPoints;
    af.m_linearCastFunc = hkpSymmetricAgentLinearCast<hkpShapeCollectionAgent>::staticLinearCast;
    af.m_isFlipped = true;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::ALL_SHAPE_TYPES, hkcdShapeType::COLLECTION);
    dispatcher->registerCollisionAgent(af, hkcdShapeType::ALL_SHAPE_TYPES, hkcdShapeType::TRI_SAMPLED_HEIGHT_FIELD_COLLECTION);

    af.m_createFunc = createListAAgent;
    af.m_getPenetrationsFunc = staticGetPenetrations;
    af.m_getClosestPointFunc = staticGetClosestPoints;
    af.m_linearCastFunc = staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::COLLECTION, hkcdShapeType::ALL_SHAPE_TYPES);
    dispatcher->registerCollisionAgent(af, hkcdShapeType::TRI_SAMPLED_HEIGHT_FIELD_COLLECTION, hkcdShapeType::ALL_SHAPE_TYPES);
}

// 0x00DC7D90: an agent for every child of A's collection the filter enables against B
hkpShapeCollectionAgent::hkpShapeCollectionAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
    : hkpCollisionAgent(mgr)
{
    hkpCdBody childBody(&bodyA);
    const hkpShapeContainer* container = bodyA.getShape()->getContainer();
    m_agents._reserve(hkContainerHeapAllocator::s_alloc, container->getNumChildShapes());
    for (unsigned int key = container->getFirstKey(); key != 0xFFFFFFFF; key = container->getNextKey(key))
    {
        hkpShapeBuffer buffer;
        childBody.setShape(container->getChildShape(key, buffer), key);
        if (hkShapeCollectionAgent_isEnabled(input, bodyA, bodyB, *container, key))
        {
            KeyAgentPair& pair = m_agents.begin()[m_agents.getSize()];
            m_agents.setSizeUnchecked(m_agents.getSize() + 1);
            pair.m_agent = input.m_dispatcher->getNewCollisionAgent(childBody, bodyB, input, mgr);
            pair.m_key = key;
        }
    }
}

// 0x00DB3DA0
hkpShapeCollectionAgent::~hkpShapeCollectionAgent()
{
}

// 0x00DC7F20
hkpCollisionAgent* hkpShapeCollectionAgent::createListAAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpShapeCollectionAgent(bodyA, bodyB, input, mgr);
}

// 0x00DC7F60
hkpCollisionAgent* hkpShapeCollectionAgent::createListBAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpSymmetricAgent<hkpShapeCollectionAgent>(bodyA, bodyB, input, mgr);
}

// 0x00DC8050
int hkpShapeCollectionAgent::getAgentIndex(unsigned int key)
{
    for (int i = 0; i < m_agents.getSize(); i++)
    {
        if (m_agents[i].m_key == key)
        {
            return i;
        }
    }
    return -1;
}

// 0x00DC7390
void hkpShapeCollectionAgent::cleanup(hkpConstraintOwner& info)
{
    for (int i = 0; i < m_agents.getSize(); i++)
    {
        m_agents[i].m_agent->cleanup(info);
    }
    delete this;
}

// 0x00DC73D0
void hkpShapeCollectionAgent::invalidateTim(const hkpCollisionInput& input)
{
    for (int i = 0; i < m_agents.getSize(); i++)
    {
        m_agents[i].m_agent->invalidateTim(input);
    }
}

// 0x00DC7400
void hkpShapeCollectionAgent::warpTime(float oldTime, float newTime, const hkpCollisionInput& input)
{
    for (int i = 0; i < m_agents.getSize(); i++)
    {
        m_agents[i].m_agent->warpTime(oldTime, newTime, input);
    }
}

// 0x00DC7450
void hkpShapeCollectionAgent::processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result)
{
    const hkpShapeContainer* container = bodyA.getShape()->getContainer();
    hkpCdBody childBody(&bodyA);
    for (int i = 0; i < m_agents.getSize(); i++)
    {
        hkpShapeBuffer buffer;
        const unsigned int key = m_agents[i].m_key;
        childBody.setShape(container->getChildShape(key, buffer), key);
        m_agents[i].m_agent->processCollision(childBody, bodyB, input, result);
    }
}

// 0x00DC7530
void hkpShapeCollectionAgent::getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    const hkpShapeContainer* container = bodyA.getShape()->getContainer();
    hkpCdBody childBody(&bodyA);
    for (int i = 0; i < m_agents.getSize(); i++)
    {
        hkpShapeBuffer buffer;
        const unsigned int key = m_agents[i].m_key;
        childBody.setShape(container->getChildShape(key, buffer), key);
        m_agents[i].m_agent->getClosestPoints(childBody, bodyB, input, collector);
    }
}

// 0x00DC7970: stops at the collector's early out
void hkpShapeCollectionAgent::getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    const hkpShapeContainer* container = bodyA.getShape()->getContainer();
    hkpCdBody childBody(&bodyA);
    for (int i = 0; i < m_agents.getSize(); i++)
    {
        hkpShapeBuffer buffer;
        const unsigned int key = m_agents[i].m_key;
        childBody.setShape(container->getChildShape(key, buffer), key);
        m_agents[i].m_agent->getPenetrations(childBody, bodyB, input, collector);
        if (collector.m_earlyOut.m_bool)
        {
            break;
        }
    }
}

// 0x00DC7750
void hkpShapeCollectionAgent::linearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
{
    const hkpShapeContainer* container = bodyA.getShape()->getContainer();
    hkpCdBody childBody(&bodyA);
    for (int i = 0; i < m_agents.getSize(); i++)
    {
        hkpShapeBuffer buffer;
        const unsigned int key = m_agents[i].m_key;
        childBody.setShape(container->getChildShape(key, buffer), key);
        m_agents[i].m_agent->linearCast(childBody, bodyB, input, collector, startCollector);
    }
}

// 0x00DC7610: every child the filter enables
void hkpShapeCollectionAgent::staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    const hkpShapeContainer* container = bodyA.getShape()->getContainer();
    hkpCdBody childBody(&bodyA);
    const hkcdShapeType::ShapeTypeEnum typeB = hkShapeCollectionAgent_type(bodyB);
    for (unsigned int key = container->getFirstKey(); key != 0xFFFFFFFF; key = container->getNextKey(key))
    {
        if (!hkShapeCollectionAgent_isEnabled(input, bodyA, bodyB, *container, key))
        {
            continue;
        }
        hkpShapeBuffer buffer;
        childBody.setShape(container->getChildShape(key, buffer), key);
        const hkpCollisionDispatcher* dispatcher = input.m_dispatcher;
        dispatcher->getGetClosestPointsFunc(hkShapeCollectionAgent_type(childBody), typeB)(childBody, bodyB, input, collector);
    }
}

// 0x00DC7A60: stops at the collector's early out
void hkpShapeCollectionAgent::staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    const hkpShapeContainer* container = bodyA.getShape()->getContainer();
    hkpCdBody childBody(&bodyA);
    const hkcdShapeType::ShapeTypeEnum typeB = hkShapeCollectionAgent_type(bodyB);
    for (unsigned int key = container->getFirstKey(); key != 0xFFFFFFFF; key = container->getNextKey(key))
    {
        if (!hkShapeCollectionAgent_isEnabled(input, bodyA, bodyB, *container, key))
        {
            continue;
        }
        hkpShapeBuffer buffer;
        childBody.setShape(container->getChildShape(key, buffer), key);
        const hkpCollisionDispatcher* dispatcher = input.m_dispatcher;
        dispatcher->getGetPenetrationsFunc(hkShapeCollectionAgent_type(childBody), typeB)(childBody, bodyB, input, collector);
        if (collector.m_earlyOut.m_bool)
        {
            break;
        }
    }
}

// 0x00DC7830
void hkpShapeCollectionAgent::staticLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
{
    const hkpShapeContainer* container = bodyA.getShape()->getContainer();
    hkpCdBody childBody(&bodyA);
    const hkcdShapeType::ShapeTypeEnum typeB = hkShapeCollectionAgent_type(bodyB);
    for (unsigned int key = container->getFirstKey(); key != 0xFFFFFFFF; key = container->getNextKey(key))
    {
        if (!hkShapeCollectionAgent_isEnabled(input, bodyA, bodyB, *container, key))
        {
            continue;
        }
        hkpShapeBuffer buffer;
        childBody.setShape(container->getChildShape(key, buffer), key);
        const hkpCollisionDispatcher* dispatcher = input.m_dispatcher;
        dispatcher->getLinearCastFunc(hkShapeCollectionAgent_type(childBody), typeB)(childBody, bodyB, input, collector, startCollector);
    }
}

// 0x00DC7BB0
// Children the filter now disables lose their agents (the last pair moves into the hole); newly enabled children get
// one (appended); the others update their own filters.
void hkpShapeCollectionAgent::updateShapeCollectionFilter(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpConstraintOwner& constraintOwner)
{
    hkpCdBody childBody(&bodyA);
    const hkpShapeContainer* container = bodyA.getShape()->getContainer();
    for (unsigned int key = container->getFirstKey(); key != 0xFFFFFFFF; key = container->getNextKey(key))
    {
        const int index = getAgentIndex(key);
        if (hkShapeCollectionAgent_isEnabled(input, bodyA, bodyB, *container, key))
        {
            hkpShapeBuffer buffer;
            childBody.setShape(container->getChildShape(key, buffer), key);
            if (index == -1)
            {
                KeyAgentPair& pair = m_agents._expandOne(hkContainerHeapAllocator::s_alloc);
                pair.m_key = key;
                pair.m_agent = input.m_dispatcher->getNewCollisionAgent(childBody, bodyB, input, m_contactMgr);
            }
            else
            {
                m_agents[index].m_agent->updateShapeCollectionFilter(childBody, bodyB, input, constraintOwner);
            }
        }
        else if (index != -1)
        {
            m_agents[index].m_agent->cleanup(constraintOwner);
            m_agents.removeAt(index);
        }
    }
}
