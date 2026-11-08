// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/deprecated/multisphere/hkpmultisphereagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6, stub_impl.md S4): the (deprecated) multi-sphere agent, from rfg.exe. A multi-sphere
// shape A is handled as one sphere shape per sphere: a body with the sphere's world position (a copy of A's motion
// state with the translation moved) against B, through the dispatcher's agent for (sphere, B). The agent keeps one
// child agent per sphere. No RFG data has a multi-sphere shape (hkpMultiSphereShape is not ported), so nothing creates
// this agent.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/deprecated/multisphere/hkpmultisphereagent.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdbodypaircollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagentlinearcast.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/sphere/hkpsphereshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/deprecated/multisphere/hkpmultisphereshape.h"

// The sphere's offset in world space: (x * c0 + y * c1) + z * c2 with A's rotation columns.
static inline __m128 hkMultiSphereAgent_offset(const hkTransformf& t, const hkVector4f& localSphere)
{
    const __m128 v = localSphere.m_quad;
    const __m128 x = _mm_mul_ps(_mm_shuffle_ps(v, v, 0x00), t.m_rotation.m_col0.m_quad);
    const __m128 y = _mm_mul_ps(_mm_shuffle_ps(v, v, 0x55), t.m_rotation.m_col1.m_quad);
    const __m128 z = _mm_mul_ps(_mm_shuffle_ps(v, v, 0xAA), t.m_rotation.m_col2.m_quad);
    return _mm_add_ps(_mm_add_ps(y, x), z);
}

// The queries move only the translation of the motion state copy; collision processing moves the swept centers too.
static inline void hkMultiSphereAgent_placeSphere(hkMotionState& ms, const hkMotionState& original, const hkVector4f& localSphere)
{
    const __m128 offset = hkMultiSphereAgent_offset(ms.m_transform, localSphere);
    ms.m_transform.m_translation.m_quad = _mm_add_ps(original.m_transform.m_translation.m_quad, offset);
}

static inline void hkMultiSphereAgent_placeSphereSwept(hkMotionState& ms, const hkMotionState& original, const hkVector4f& localSphere)
{
    const __m128 offset = hkMultiSphereAgent_offset(ms.m_transform, localSphere);
    ms.m_transform.m_translation.m_quad = _mm_add_ps(original.m_transform.m_translation.m_quad, offset);
    ms.m_sweptTransform.m_centerOfMass0.m_quad = _mm_add_ps(original.m_sweptTransform.m_centerOfMass0.m_quad, offset);
    ms.m_sweptTransform.m_centerOfMass1.m_quad = _mm_add_ps(original.m_sweptTransform.m_centerOfMass1.m_quad, offset);
}

// 0x00DC8CD0
void hkpMultiSphereAgent::registerAgent(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::AgentFuncs af;
    af.m_createFunc = createListBAgent;
    af.m_getPenetrationsFunc = hkpSymmetricAgentLinearCast<hkpMultiSphereAgent>::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpSymmetricAgentLinearCast<hkpMultiSphereAgent>::staticGetClosestPoints;
    af.m_linearCastFunc = hkpSymmetricAgentLinearCast<hkpMultiSphereAgent>::staticLinearCast;
    af.m_isFlipped = true;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::ALL_SHAPE_TYPES, hkcdShapeType::MULTI_SPHERE);

    af.m_createFunc = createListAAgent;
    af.m_getPenetrationsFunc = staticGetPenetrations;
    af.m_getClosestPointFunc = staticGetClosestPoints;
    af.m_linearCastFunc = staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::MULTI_SPHERE, hkcdShapeType::ALL_SHAPE_TYPES);
}

// 0x00DC8340: the child agents, then the agent itself
void hkpMultiSphereAgent::cleanup(hkpConstraintOwner& info)
{
    for (int i = 0; i < m_agents.getSize(); i++)
    {
        m_agents[i].m_agent->cleanup(info);
    }
    delete this;
}

// 0x00DC8380
// Original bug kept: each child gets the descending loop index as its shape key while the agents and spheres go up.
void hkpMultiSphereAgent::processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result)
{
    const hkpMultiSphereShape* msA = static_cast<const hkpMultiSphereShape*>(bodyA.getShape());
    hkMotionState ms = *bodyA.getMotionState();
    hkpSphereShape sphereShape(0.0f);
    hkpCdBody newOperandA(&bodyA, &ms);
    const hkVector4f* localSpheres = msA->getSpheres();
    KeyAgentPair* agentPair = m_agents.begin();
    for (int i = m_agents.getSize() - 1; i >= 0; i--, agentPair++)
    {
        const hkVector4f& sphere = localSpheres[agentPair->m_key];
        hkMultiSphereAgent_placeSphereSwept(ms, *bodyA.getMotionState(), sphere);
        sphereShape.m_radius = sphere.m_quad.m128_f32[3];
        newOperandA.setShape(&sphereShape, (unsigned int)i);
        agentPair->m_agent->processCollision(newOperandA, bodyB, input, result);
    }
}

// 0x00DC8490: as processCollision (the descending index as the key)
void hkpMultiSphereAgent::getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    const hkpMultiSphereShape* msA = static_cast<const hkpMultiSphereShape*>(bodyA.getShape());
    hkMotionState ms = *bodyA.getMotionState();
    hkpSphereShape sphereShape(0.0f);
    hkpCdBody newOperandA(&bodyA, &ms);
    const hkVector4f* localSpheres = msA->getSpheres();
    KeyAgentPair* agentPair = m_agents.begin();
    for (int i = m_agents.getSize() - 1; i >= 0; i--, agentPair++)
    {
        const hkVector4f& sphere = localSpheres[agentPair->m_key];
        hkMultiSphereAgent_placeSphere(ms, *bodyA.getMotionState(), sphere);
        sphereShape.m_radius = sphere.m_quad.m128_f32[3];
        newOperandA.setShape(&sphereShape, (unsigned int)i);
        agentPair->m_agent->getClosestPoints(newOperandA, bodyB, input, collector);
    }
}

// 0x00DC8570: every sphere through the dispatcher's (sphere, B) function
void hkpMultiSphereAgent::staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    const hkpMultiSphereShape* msA = static_cast<const hkpMultiSphereShape*>(bodyA.getShape());
    hkMotionState ms = *bodyA.getMotionState();
    hkpSphereShape sphereShape(0.0f);
    hkpCdBody newOperandA(&bodyA, &ms);
    const hkcdShapeType::ShapeTypeEnum typeB = static_cast<hkcdShapeType::ShapeTypeEnum>(bodyB.getShape()->m_type.m_storage);
    const hkVector4f* localSpheres = msA->getSpheres();
    for (int i = 0; i < msA->getNumSpheres(); i++)
    {
        hkMultiSphereAgent_placeSphere(ms, *bodyA.getMotionState(), localSpheres[i]);
        sphereShape.m_radius = localSpheres[i].m_quad.m128_f32[3];
        newOperandA.setShape(&sphereShape, (unsigned int)i);
        const hkcdShapeType::ShapeTypeEnum typeA = static_cast<hkcdShapeType::ShapeTypeEnum>(sphereShape.m_type.m_storage);
        input.m_dispatcher->getGetClosestPointsFunc(typeA, typeB)(newOperandA, bodyB, input, collector);
    }
}

// 0x00DC8680
// Original bug kept: every sphere (descending, its index as the key) is cast with the first child agent.
void hkpMultiSphereAgent::linearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
{
    const hkpMultiSphereShape* msA = static_cast<const hkpMultiSphereShape*>(bodyA.getShape());
    hkMotionState ms = *bodyA.getMotionState();
    hkpSphereShape sphereShape(0.0f);
    hkpCdBody newOperandA(&bodyA, &ms);
    const hkVector4f* localSpheres = msA->getSpheres();
    KeyAgentPair* agentPair = m_agents.begin();
    for (int i = m_agents.getSize() - 1; i >= 0; i--)
    {
        hkMultiSphereAgent_placeSphere(ms, *bodyA.getMotionState(), localSpheres[i]);
        sphereShape.m_radius = localSpheres[i].m_quad.m128_f32[3];
        newOperandA.setShape(&sphereShape, (unsigned int)i);
        agentPair->m_agent->linearCast(newOperandA, bodyB, input, collector, startCollector);
    }
}

// 0x00DC8770
void hkpMultiSphereAgent::staticLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
{
    const hkpMultiSphereShape* msA = static_cast<const hkpMultiSphereShape*>(bodyA.getShape());
    hkMotionState ms = *bodyA.getMotionState();
    hkpSphereShape sphereShape(0.0f);
    hkpCdBody newOperandA(&bodyA, &ms);
    const hkcdShapeType::ShapeTypeEnum typeB = static_cast<hkcdShapeType::ShapeTypeEnum>(bodyB.getShape()->m_type.m_storage);
    const hkVector4f* localSpheres = msA->getSpheres();
    for (int i = 0; i < msA->getNumSpheres(); i++)
    {
        hkMultiSphereAgent_placeSphere(ms, *bodyA.getMotionState(), localSpheres[i]);
        sphereShape.m_radius = localSpheres[i].m_quad.m128_f32[3];
        newOperandA.setShape(&sphereShape, (unsigned int)i);
        const hkcdShapeType::ShapeTypeEnum typeA = static_cast<hkcdShapeType::ShapeTypeEnum>(sphereShape.m_type.m_storage);
        input.m_dispatcher->getLinearCastFunc(typeA, typeB)(newOperandA, bodyB, input, collector, startCollector);
    }
}

// 0x00DC8880: stops at the collector's early out
void hkpMultiSphereAgent::getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    const hkpMultiSphereShape* msA = static_cast<const hkpMultiSphereShape*>(bodyA.getShape());
    hkMotionState ms = *bodyA.getMotionState();
    hkpSphereShape sphereShape(0.0f);
    hkpCdBody newOperandA(&bodyA, &ms);
    const hkVector4f* localSpheres = msA->getSpheres();
    KeyAgentPair* agentPair = m_agents.begin();
    for (int i = m_agents.getSize() - 1; i >= 0; i--, agentPair++)
    {
        const hkVector4f& sphere = localSpheres[agentPair->m_key];
        hkMultiSphereAgent_placeSphere(ms, *bodyA.getMotionState(), sphere);
        sphereShape.m_radius = sphere.m_quad.m128_f32[3];
        newOperandA.setShape(&sphereShape, (unsigned int)i);
        agentPair->m_agent->getPenetrations(newOperandA, bodyB, input, collector);
        if (collector.getEarlyOut())
        {
            break;
        }
    }
}

// 0x00DC8970
void hkpMultiSphereAgent::staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    const hkpMultiSphereShape* msA = static_cast<const hkpMultiSphereShape*>(bodyA.getShape());
    hkMotionState ms = *bodyA.getMotionState();
    hkpSphereShape sphereShape(0.0f);
    hkpCdBody newOperandA(&bodyA, &ms);
    const hkcdShapeType::ShapeTypeEnum typeB = static_cast<hkcdShapeType::ShapeTypeEnum>(bodyB.getShape()->m_type.m_storage);
    const hkVector4f* localSpheres = msA->getSpheres();
    for (int i = 0; i < msA->getNumSpheres(); i++)
    {
        hkMultiSphereAgent_placeSphere(ms, *bodyA.getMotionState(), localSpheres[i]);
        sphereShape.m_radius = localSpheres[i].m_quad.m128_f32[3];
        newOperandA.setShape(&sphereShape, (unsigned int)i);
        const hkcdShapeType::ShapeTypeEnum typeA = static_cast<hkcdShapeType::ShapeTypeEnum>(sphereShape.m_type.m_storage);
        input.m_dispatcher->getGetPenetrationsFunc(typeA, typeB)(newOperandA, bodyB, input, collector);
        if (collector.getEarlyOut())
        {
            break;
        }
    }
}

// 0x00DC8A80: one child agent per sphere (the sphere's index as the key)
hkpMultiSphereAgent::hkpMultiSphereAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
    : hkpCollisionAgent(mgr)
{
    const hkpMultiSphereShape* msA = static_cast<const hkpMultiSphereShape*>(bodyA.getShape());
    const int numSpheres = msA->getNumSpheres();
    m_agents.reserve(numSpheres);
    hkpSphereShape sphereShape(0.0f);
    hkMotionState ms = *bodyA.getMotionState();
    hkpCdBody newOperandA(&bodyA, &ms);
    const hkVector4f* localSpheres = msA->getSpheres();
    for (int i = 0; i < numSpheres; i++)
    {
        hkMultiSphereAgent_placeSphereSwept(ms, *bodyA.getMotionState(), localSpheres[i]);
        sphereShape.m_radius = localSpheres[i].m_quad.m128_f32[3];
        newOperandA.setShape(&sphereShape, (unsigned int)i);
        KeyAgentPair& ap = m_agents.expandOne();
        const hkcdShapeType::ShapeTypeEnum typeA = static_cast<hkcdShapeType::ShapeTypeEnum>(sphereShape.m_type.m_storage);
        const hkcdShapeType::ShapeTypeEnum typeB = static_cast<hkcdShapeType::ShapeTypeEnum>(bodyB.getShape()->m_type.m_storage);
        const hkpCollisionDispatcher::IsAgentPredictive predictive = static_cast<hkpCollisionDispatcher::IsAgentPredictive>(input.m_createPredictiveAgents.val());
        ap.m_agent = input.m_dispatcher->getCollisionAgentCreationFunction(typeA, typeB, predictive)(newOperandA, bodyB, input, mgr);
        ap.m_key = (unsigned int)i;
    }
}

// 0x00DC90F0
hkpMultiSphereAgent::~hkpMultiSphereAgent()
{
}

// 0x00DC8C40
hkpCollisionAgent* hkpMultiSphereAgent::createListAAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpMultiSphereAgent(bodyA, bodyB, input, mgr);
}

// 0x00DC8C80
hkpCollisionAgent* hkpMultiSphereAgent::createListBAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpSymmetricAgent<hkpMultiSphereAgent>(bodyA, bodyB, input, mgr);
}
