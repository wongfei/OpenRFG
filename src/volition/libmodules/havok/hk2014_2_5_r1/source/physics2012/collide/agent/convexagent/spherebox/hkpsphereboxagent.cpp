// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/spherebox/hkpsphereboxagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the sphere-box agent2 (one contact point; the box rounded by its convex radius),
// from rfg.exe; box-sphere is hkpSymmetricAgentLinearCast<hkpSphereBoxAgent>. No SDK source exists for 2014.2.5;
// ported from the binary.
//
// The shipped build records the "TtSphereBox" monitor timers (TLS hkMonitorStream); they are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/spherebox/hkpsphereboxagent.h"

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
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/box/hkpboxshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/sphere/hkpsphereshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

// The lane of a vector selected by a one-lane mask, in all lanes.
static inline Q hkSphereBoxAgent_lane(Q v, Q mask)
{
    Q t = _mm_and_ps(mask, v);
    t = _mm_or_ps(_mm_shuffle_ps(t, t, 0x4E), t);
    return _mm_or_ps(_mm_shuffle_ps(t, t, 0xB1), t);
}

// The normal from the box (B) to the sphere's centre (A) in world space, the distance less the radii in w. Outside the
// box: from the closest point of the box, false beyond the tolerance; inside: along the face of least penetration.
static inline bool hkSphereBoxAgent_normal(const hkpCdBody& sphereBody, const hkpCdBody& boxBody, float tolerance, Q& normalOut, Q& distanceOut)
{
    const hkTransformf& boxTransform = boxBody.getTransform();
    const hkpBoxShape* box = static_cast<const hkpBoxShape*>(boxBody.getShape());
    const hkpSphereShape* sphere = static_cast<const hkpSphereShape*>(sphereBody.getShape());
    const Q center = sphereBody.getTransform().getTranslation().m_quad;
    const Q p = rotateInverse(boxTransform.getRotation(), sub(center, boxTransform.getTranslation().m_quad));
    const Q radiusSum = splat(box->m_radius + sphere->m_radius);
    const Q absP = abs(p);
    const Q outside = sub(_mm_min_ps(absP, box->m_halfExtents.m_quad), absP);
    const Q zero = _mm_setzero_ps();
    Q n;
    Q distance;
    if (_mm_movemask_ps(_mm_cmplt_ps(outside, zero)) & 7)
    {
        const Q len2 = dot3(outside, outside);
        const Q inv = invSqrt23SetZero(len2);
        distance = sub(mul(inv, len2), radiusSum);
        if (greater(distance, splat(tolerance)))
        {
            return false;
        }
        const Q nLocal = neg(_mm_xor_ps(signBits(p), mul(inv, outside)));
        n = rotate(boxTransform.getRotation(), nLocal);
    }
    else
    {
        const Q inside = sub(absP, box->m_halfExtents.m_quad);
        const Q maxInside = _mm_max_ps(bcast<2>(inside), _mm_max_ps(bcast<1>(inside), bcast<0>(inside)));
        const int axis = hkVector4Comparison_maskToLastIndex[_mm_movemask_ps(_mm_and_ps(_mm_cmple_ps(maxInside, inside), laneMask(7)))];
        distance = sub(maxInside, radiusSum);
        const Q pAxis = hkSphereBoxAgent_lane(p, laneMask(1 << axis));
        n = _mm_xor_ps(signBits(_mm_cmplt_ps(pAxis, zero)), boxTransform.getRotation().getColumn(axis).m_quad);
    }
    normalOut = setXYZ_W(n, distance);
    distanceOut = distance;
    return true;
}

// The point on the box's surface: the sphere's centre moved back along the normal by the distance and the radius; w 1.
static inline Q hkSphereBoxAgent_position(const hkpCdBody& sphereBody, Q normal, Q distance)
{
    const float radius = static_cast<const hkpSphereShape*>(sphereBody.getShape())->m_radius;
    const Q p = add(mul(sub(sub(_mm_setzero_ps(), distance), splat(radius)), normal), sphereBody.getTransform().getTranslation().m_quad);
    return setXYZ_W(p, g_vectorfConstants[HK_QUADREAL_1]);
}

// 0x00DBF9E0: box-sphere (the symmetric agent) and sphere-box
void hkpSphereBoxAgent::registerAgent(hkpCollisionDispatcher* dispatcher)
{
    {
        hkpCollisionDispatcher::AgentFuncs af;
        af.m_createFunc = createBoxSphereAgent;
        af.m_getPenetrationsFunc = hkpSymmetricAgentLinearCast<hkpSphereBoxAgent>::staticGetPenetrations;
        af.m_getClosestPointFunc = hkpSymmetricAgentLinearCast<hkpSphereBoxAgent>::staticGetClosestPoints;
        af.m_linearCastFunc = hkpSymmetricAgentLinearCast<hkpSphereBoxAgent>::staticLinearCast;
        af.m_isFlipped = true;
        af.m_isPredictive = false;
        dispatcher->registerCollisionAgent(af, hkcdShapeType::BOX, hkcdShapeType::SPHERE);
    }
    {
        hkpCollisionDispatcher::AgentFuncs af;
        af.m_createFunc = createSphereBoxAgent;
        af.m_getPenetrationsFunc = staticGetPenetrations;
        af.m_getClosestPointFunc = staticGetClosestPoints;
        af.m_linearCastFunc = staticLinearCast;
        af.m_isFlipped = false;
        af.m_isPredictive = false;
        dispatcher->registerCollisionAgent(af, hkcdShapeType::SPHERE, hkcdShapeType::BOX);
    }
}

// 0x00DBFA80
hkpSphereBoxAgent::hkpSphereBoxAgent(hkpContactMgr* mgr)
    : hkpIterativeLinearCastAgent(mgr)
{
    m_contactPointId = 0xFFFF;
}

// 0x00DBFAF0
hkpSphereBoxAgent::~hkpSphereBoxAgent()
{
}

// 0x00DBEDE0
hkpCollisionAgent* hkpSphereBoxAgent::createSphereBoxAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpSphereBoxAgent(mgr);
}

// 0x00DBEE60
hkpCollisionAgent* hkpSphereBoxAgent::createBoxSphereAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpSymmetricAgentLinearCast<hkpSphereBoxAgent>(mgr);
}

// 0x00DBEE20
void hkpSphereBoxAgent::cleanup(hkpConstraintOwner& constraintOwner)
{
    if (m_contactPointId != 0xFFFF)
    {
        m_contactMgr->removeContactPointImpl(m_contactPointId, constraintOwner);
    }
    delete this;
}

// 0x00DBF070
void hkpSphereBoxAgent::processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result)
{
    Q normal;
    Q distance;
    if (hkSphereBoxAgent_normal(bodyA, bodyB, input.m_tolerance, normal, distance))
    {
        hkpProcessCdPoint* cp = result.m_firstFreeContactPoint;
        cp->m_contact.m_separatingNormal.m_quad = normal;
        cp->m_contact.m_position.m_quad = hkSphereBoxAgent_position(bodyA, normal, distance);
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

// 0x00DBF3D0
void hkpSphereBoxAgent::getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    staticGetClosestPoints(bodyA, bodyB, input, collector);
}

// 0x00DBF6E0
void hkpSphereBoxAgent::staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    Q normal;
    Q distance;
    if (hkSphereBoxAgent_normal(bodyA, bodyB, input.m_tolerance, normal, distance))
    {
        alignas(16) hkContactPoint contact;
        contact.m_position.m_quad = hkSphereBoxAgent_position(bodyA, normal, distance);
        contact.m_separatingNormal.m_quad = normal;
        hkpCdPoint point(bodyA, bodyB, contact);
        collector.addCdPoint(point);
    }
}

// 0x00DBF050
void hkpSphereBoxAgent::getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    staticGetPenetrations(bodyA, bodyB, input, collector);
}

// 0x00DBEEA0: the sphere's centre against the closest point of the box (the radii summed)
void hkpSphereBoxAgent::staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    const hkTransformf& boxTransform = bodyB.getTransform();
    const hkpBoxShape* box = static_cast<const hkpBoxShape*>(bodyB.getShape());
    const hkpSphereShape* sphere = static_cast<const hkpSphereShape*>(bodyA.getShape());
    const Q center = bodyA.getTransform().getTranslation().m_quad;
    const Q p = rotateInverse(boxTransform.getRotation(), sub(center, boxTransform.getTranslation().m_quad));
    const Q clamped = _mm_xor_ps(_mm_min_ps(abs(p), box->m_halfExtents.m_quad), signBits(p));
    const Q d = sub(transformPos(boxTransform, clamped), center);
    const Q radiusSum = splat(sphere->m_radius + box->m_radius);
    if (lessOrNaN(dot3(d, d), mul(radiusSum, radiusSum)))
    {
        collector.addCdBodyPair(bodyA, bodyB);
    }
}
