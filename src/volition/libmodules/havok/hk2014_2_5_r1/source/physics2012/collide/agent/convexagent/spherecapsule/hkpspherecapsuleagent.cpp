// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/spherecapsule/hkpspherecapsuleagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the sphere-capsule agent2 (one contact point), from rfg.exe; capsule-sphere is
// hkpSymmetricAgentLinearCast<hkpSphereCapsuleAgent>. No SDK source exists for 2014.2.5; ported from the binary.
//
// The shipped build records the "TtSphereCapsule" monitor timers (TLS hkMonitorStream); they are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/spherecapsule/hkpspherecapsuleagent.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/collide/linesegment/hklinesegmentutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/contactmgr/hkpcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscdpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdbodypaircollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagentlinearcast.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/capsule/hkpcapsuleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/sphere/hkpsphereshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

// The closest point of the capsule's axis (in world space) to the sphere's centre.
static inline Q hkSphereCapsuleAgent_closestOnAxis(const hkpCdBody& sphereBody, const hkpCdBody& capsuleBody, Q& axisOut)
{
    const hkpCapsuleShape* capsule = static_cast<const hkpCapsuleShape*>(capsuleBody.getShape());
    const hkTransformf& t = capsuleBody.getTransform();
    hkVector4f v0;
    v0.m_quad = transformPos(t, capsule->m_vertexA.m_quad);
    hkVector4f v1;
    v1.m_quad = transformPos(t, capsule->m_vertexB.m_quad);
    hkLineSegmentUtil::ClosestPointLineSegResult result;
    hkLineSegmentUtil::closestPointLineSeg(sphereBody.getTransform().getTranslation(), v0, v1, result);
    axisOut = sub(v1.m_quad, v0.m_quad);
    return result.m_pointOnEdge.m_quad;
}

// 0x00DC2250
// The point on the capsule's surface towards the sphere's centre and the normal from the capsule to the sphere (w: the
// distance less the radii); a miss beyond the tolerance. A centre on the axis takes a direction perpendicular to it.
hkpSphereCapsuleAgent::ClosestPointResult hkpSphereCapsuleAgent::getClosestPointInl(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkContactPoint& cpoint)
{
    const hkpSphereShape* sphere = static_cast<const hkpSphereShape*>(bodyA.getShape());
    const hkpCapsuleShape* capsule = static_cast<const hkpCapsuleShape*>(bodyB.getShape());
    Q axis;
    const Q closest = hkSphereCapsuleAgent_closestOnAxis(bodyA, bodyB, axis);
    const Q radiusSum = splat(sphere->m_radius + capsule->m_radius);
    const Q limit = add(splat(input.m_tolerance), radiusSum);
    const Q center = bodyA.getTransform().getTranslation().m_quad;
    Q d = sub(center, closest);
    const Q len2 = dot3(d, d);
    if (!lessOrNaN(len2, mul(limit, limit)))
    {
        return ST_CP_MISS;
    }
    const Q zero = _mm_setzero_ps();
    Q length;
    if (greater(len2, zero))
    {
        length = mul(invSqrt23(len2), len2);
    }
    else
    {
        length = zero;
        d = perpendicular(axis);
    }
    const Q n = mul(invSqrt23SetZero(dot3(d, d)), d);
    const Q normal = setXYZ_W(n, sub(length, radiusSum));
    cpoint.m_separatingNormal.m_quad = normal;
    cpoint.m_position.m_quad = add(mul(sub(splat(capsule->m_radius), length), normal), center);
    return ST_CP_HIT;
}

// 0x00DC16C0
hkpSphereCapsuleAgent::ClosestPointResult hkpSphereCapsuleAgent::getClosestPoint(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkContactPoint& cpoint)
{
    return getClosestPointInl(bodyA, bodyB, input, cpoint);
}

// 0x00DC1EF0: capsule-sphere (the symmetric agent) and sphere-capsule
void hkpSphereCapsuleAgent::registerAgent(hkpCollisionDispatcher* dispatcher)
{
    {
        hkpCollisionDispatcher::AgentFuncs af;
        af.m_createFunc = createCapsuleSphereAgent;
        af.m_getPenetrationsFunc = hkpSymmetricAgentLinearCast<hkpSphereCapsuleAgent>::staticGetPenetrations;
        af.m_getClosestPointFunc = hkpSymmetricAgentLinearCast<hkpSphereCapsuleAgent>::staticGetClosestPoints;
        af.m_linearCastFunc = hkpSymmetricAgentLinearCast<hkpSphereCapsuleAgent>::staticLinearCast;
        af.m_isFlipped = true;
        af.m_isPredictive = false;
        dispatcher->registerCollisionAgent(af, hkcdShapeType::CAPSULE, hkcdShapeType::SPHERE);
    }
    {
        hkpCollisionDispatcher::AgentFuncs af;
        af.m_createFunc = createSphereCapsuleAgent;
        af.m_getPenetrationsFunc = staticGetPenetrations;
        af.m_getClosestPointFunc = staticGetClosestPoints;
        af.m_linearCastFunc = staticLinearCast;
        af.m_isFlipped = false;
        af.m_isPredictive = false;
        dispatcher->registerCollisionAgent(af, hkcdShapeType::SPHERE, hkcdShapeType::CAPSULE);
    }
}

// 0x00DC1F90
hkpSphereCapsuleAgent::hkpSphereCapsuleAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
    : hkpIterativeLinearCastAgent(mgr)
{
    m_contactPointId = 0xFFFF;
}

// 0x00DC2000
hkpSphereCapsuleAgent::~hkpSphereCapsuleAgent()
{
}

// 0x00DC1470
hkpCollisionAgent* hkpSphereCapsuleAgent::createSphereCapsuleAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpSphereCapsuleAgent(bodyA, bodyB, input, mgr);
}

// 0x00DC14F0
hkpCollisionAgent* hkpSphereCapsuleAgent::createCapsuleSphereAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpSymmetricAgentLinearCast<hkpSphereCapsuleAgent>(bodyA, bodyB, input, mgr);
}

// 0x00DC14B0
void hkpSphereCapsuleAgent::cleanup(hkpConstraintOwner& constraintOwner)
{
    if (m_contactPointId != 0xFFFF)
    {
        m_contactMgr->removeContactPointImpl(m_contactPointId, constraintOwner);
    }
    delete this;
}

// 0x00DC24A0
void hkpSphereCapsuleAgent::processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result)
{
    hkpProcessCdPoint* cp = result.m_firstFreeContactPoint;
    if (getClosestPointInl(bodyA, bodyB, input, cp->m_contact) == ST_CP_HIT)
    {
        if (m_contactPointId == 0xFFFF)
        {
            m_contactPointId = m_contactMgr->addContactPointImpl(bodyA, bodyB, input, result, nullptr, cp->m_contact);
        }
        if (m_contactPointId != 0xFFFF)
        {
            cp->m_contactPointId = m_contactPointId;
            cp->m_isShortestPoint = 0;
            result.commitContactPoints();
        }
    }
    else if (m_contactPointId != 0xFFFF)
    {
        m_contactMgr->removeContactPointImpl(m_contactPointId, *result.m_constraintOwner);
        m_contactPointId = 0xFFFF;
    }
}

// 0x00DC1910
void hkpSphereCapsuleAgent::getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    staticGetClosestPoints(bodyA, bodyB, input, collector);
}

// 0x00DC1C00
void hkpSphereCapsuleAgent::staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    alignas(16) hkContactPoint contact;
    if (getClosestPointInl(bodyA, bodyB, input, contact) == ST_CP_HIT)
    {
        hkpCdPoint point(bodyA, bodyB, contact);
        collector.addCdPoint(point);
    }
}

// 0x00DC16A0
void hkpSphereCapsuleAgent::getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    staticGetPenetrations(bodyA, bodyB, input, collector);
}

// 0x00DC1530
void hkpSphereCapsuleAgent::staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    const hkpSphereShape* sphere = static_cast<const hkpSphereShape*>(bodyA.getShape());
    const hkpCapsuleShape* capsule = static_cast<const hkpCapsuleShape*>(bodyB.getShape());
    Q axis;
    const Q d = sub(hkSphereCapsuleAgent_closestOnAxis(bodyA, bodyB, axis), bodyA.getTransform().getTranslation().m_quad);
    const Q radiusSum = splat(capsule->m_radius + sphere->m_radius);
    if (lessOrNaN(dot3(d, d), mul(radiusSum, radiusSum)))
    {
        collector.addCdBodyPair(bodyA, bodyB);
    }
}
