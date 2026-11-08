// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/boxbox/hkpboxboxagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the box-box agent2, from rfg.exe. It keeps a box-box feature manifold
// (hkpBoxBoxCollisionDetection::calcManifold); thin boxes go to the GSK agent. No SDK source exists for 2014.2.5;
// ported from the binary.
//
// The shipped build records the "TtBoxBox" monitor timers (TLS hkMonitorStream); they are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/boxbox/hkpboxboxagent.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/contactmgr/hkpcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/gjk/hkpgskbaseagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/gjk/hkpgskfagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdbodypaircollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/boxbox/hkpboxboxcollisiondetection.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/box/hkpboxshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

hkBool hkpBoxBoxAgent::m_attemptToFindAllEdges = false; // 0x03A29D71

// The half extents grown by the convex radius (w: the smallest half extent grown too).
static inline hkVector4f hkBoxBoxAgent_radius(const hkpCdBody& body)
{
    const hkpBoxShape* box = static_cast<const hkpBoxShape*>(body.getShape());
    hkVector4f r;
    r.m_quad = add(splat(box->m_radius), box->m_halfExtents.m_quad);
    return r;
}

// 0x00DC3D90: closest points from the GSK agent, the linear cast iterative
void hkpBoxBoxAgent::initAgentFunc(hkpCollisionDispatcher::AgentFuncs& af)
{
    af.m_createFunc = createBoxBoxAgent;
    af.m_getPenetrationsFunc = staticGetPenetrations;
    af.m_getClosestPointFunc = hkpGskBaseAgent::staticGetClosestPoints;
    af.m_linearCastFunc = staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = false;
}

// 0x00DC3DC0
void hkpBoxBoxAgent::registerAgent(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::AgentFuncs af;
    initAgentFunc(af);
    dispatcher->registerCollisionAgent(af, hkcdShapeType::BOX, hkcdShapeType::BOX);
}

// 0x00DC3600
hkpBoxBoxAgent::hkpBoxBoxAgent(hkpContactMgr* contactMgr)
    : hkpIterativeLinearCastAgent(contactMgr)
{
}

// 0x00DC3670
hkpBoxBoxAgent::~hkpBoxBoxAgent()
{
}

// 0x00DC36D0: a box thinner than half the tolerance (1.999 x its smallest half extent) goes to the GSK agent
hkpCollisionAgent* hkpBoxBoxAgent::createBoxBoxAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* contactMgr)
{
    const hkpBoxShape* boxA = static_cast<const hkpBoxShape*>(bodyA.getShape());
    const hkpBoxShape* boxB = static_cast<const hkpBoxShape*>(bodyB.getShape());
    const Q factor = splat(1.999f); // 0x0135A960: 0x3FFFDF3B
    const Q tolerance = splat(input.m_tolerance);
    const Q thinB = _mm_cmple_ps(mul(bcast<3>(boxB->m_halfExtents.m_quad), factor), tolerance);
    const Q thinA = _mm_cmple_ps(mul(bcast<3>(boxA->m_halfExtents.m_quad), factor), tolerance);
    if (_mm_movemask_ps(_mm_or_ps(thinB, thinA)))
    {
        return hkpGskfAgent::createGskfAgent(bodyA, bodyB, input, contactMgr);
    }
    return new hkpBoxBoxAgent(contactMgr);
}

// 0x00DC3680: every point removed (refused points included), then the agent deleted
void hkpBoxBoxAgent::cleanup(hkpConstraintOwner& constraintOwner)
{
    for (int i = 0; i < m_manifold.m_numPoints; i++)
    {
        m_contactMgr->removeContactPointImpl(m_manifold.m_contactPoints[i].m_contactPointId, constraintOwner);
    }
    delete this;
}

// 0x00DC3780
void hkpBoxBoxAgent::processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result)
{
    const hkVector4f radiusA = hkBoxBoxAgent_radius(bodyA);
    const hkVector4f radiusB = hkBoxBoxAgent_radius(bodyB);
    hkSimdFloat32 tolerance;
    tolerance.m_real = splat(input.m_tolerance);
    alignas(16) hkTransformf aTb;
    aTb.setMulInverseMul(bodyA.getTransform(), bodyB.getTransform());
    hkpBoxBoxCollisionDetection detection(bodyA, bodyB, &input, m_contactMgr, &result, aTb, bodyA.getTransform(), radiusA, bodyB.getTransform(), radiusB, tolerance);
    detection.calcManifold(m_manifold);
}

// 0x00DC3B60
void hkpBoxBoxAgent::getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    staticGetClosestPoints(bodyA, bodyB, input, collector);
}

// 0x00DC3940: the closest point of the separating axis test and feature walk
void hkpBoxBoxAgent::staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    const hkVector4f radiusA = hkBoxBoxAgent_radius(bodyA);
    const hkVector4f radiusB = hkBoxBoxAgent_radius(bodyB);
    hkSimdFloat32 tolerance;
    tolerance.m_real = splat(input.m_tolerance);
    alignas(16) hkTransformf aTb;
    aTb.setMulInverseMul(bodyA.getTransform(), bodyB.getTransform());
    hkpBoxBoxCollisionDetection detection(bodyA, bodyB, nullptr, nullptr, nullptr, aTb, bodyA.getTransform(), radiusA, bodyB.getTransform(), radiusB, tolerance);
    alignas(16) hkContactPoint contact;
    if (detection.calculateClosestPoint(contact))
    {
        hkpCdPoint point(bodyA, bodyB, contact);
        collector.addCdPoint(point);
    }
}

// 0x00DC3D70
void hkpBoxBoxAgent::getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    staticGetPenetrations(bodyA, bodyB, input, collector);
}

// 0x00DC3B80: the separating axis test (within the tolerance)
void hkpBoxBoxAgent::staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    const hkVector4f radiusA = hkBoxBoxAgent_radius(bodyA);
    const hkVector4f radiusB = hkBoxBoxAgent_radius(bodyB);
    hkSimdFloat32 tolerance;
    tolerance.m_real = splat(input.m_tolerance);
    alignas(16) hkTransformf aTb;
    aTb.setMulInverseMul(bodyA.getTransform(), bodyB.getTransform());
    hkpBoxBoxCollisionDetection detection(bodyA, bodyB, nullptr, nullptr, nullptr, aTb, bodyA.getTransform(), radiusA, bodyB.getTransform(), radiusB, tolerance);
    detection.initWorkVariables();
    if (detection.checkIntersection(detection.m_tolerance4) == HK_SUCCESS)
    {
        collector.addCdBodyPair(bodyA, bodyB);
    }
}
