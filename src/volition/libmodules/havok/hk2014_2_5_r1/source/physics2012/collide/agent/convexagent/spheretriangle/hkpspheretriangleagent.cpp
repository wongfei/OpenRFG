// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/spheretriangle/hkpspheretriangleagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the sphere-triangle agent2 (one contact point, welded at the triangle's edges),
// from rfg.exe; triangle-sphere is hkpSymmetricAgentLinearCast<hkpSphereTriangleAgent>. No SDK source exists for
// 2014.2.5; ported from the binary.
//
// The shipped build records the "TtSphereTri" monitor timers (TLS hkMonitorStream); they are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/spheretriangle/hkpspheretriangleagent.h"

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
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/hkpconvexshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/sphere/hkpsphereshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/triangle/hkptriangleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

// The triangle's vertices in world space.
static inline void hkSphereTriangleAgent_worldTriangle(const hkpCdBody& triangleBody, hkVector4f* tri)
{
    const hkpTriangleShape* triangle = static_cast<const hkpTriangleShape*>(triangleBody.getShape());
    const hkTransformf& t = triangleBody.getTransform();
    tri[2].m_quad = transformPos(t, triangle->m_vertexC.m_quad);
    tri[1].m_quad = transformPos(t, triangle->m_vertexB.m_quad);
    tri[0].m_quad = transformPos(t, triangle->m_vertexA.m_quad);
}

static inline Q hkSphereTriangleAgent_radiusSum(const hkpCdBody& sphereBody, const hkpCdBody& triangleBody)
{
    const float sphereRadius = static_cast<const hkpSphereShape*>(sphereBody.getShape())->m_radius;
    const float triangleRadius = static_cast<const hkpTriangleShape*>(triangleBody.getShape())->m_radius;
    return splat(sphereRadius + triangleRadius);
}

// The sphere's centre against the world triangle: false beyond the tolerance; else the point on the triangle's
// surface and the normal from the triangle to the centre (w: the distance less the radii).
static inline bool hkSphereTriangleAgent_closest(const hkpCdBody& bodyA, const hkpCdBody& bodyB, float tolerance, const hkpCollideTriangleUtil::ClosestPointTriangleCache& cache, hkpFeatureOutput* featureOutput, Q& positionOut, Q& normalOut)
{
    alignas(16) hkVector4f tri[3];
    hkSphereTriangleAgent_worldTriangle(bodyB, tri);
    const hkVector4f& center = bodyA.getTransform().getTranslation();
    hkpCollideTriangleUtil::ClosestPointTriangleResult result;
    hkpCollideTriangleUtil::closestPointTriangle(center, tri, cache, result, featureOutput);
    const Q radiusSum = hkSphereTriangleAgent_radiusSum(bodyA, bodyB);
    const Q distance = splat(result.distance);
    if (!lessOrNaN(distance, add(splat(tolerance), radiusSum)))
    {
        return false;
    }
    const float triangleRadius = static_cast<const hkpTriangleShape*>(bodyB.getShape())->m_radius;
    positionOut = add(mul(sub(splat(triangleRadius), distance), result.hitDirection.m_quad), center.m_quad);
    normalOut = setXYZ_W(result.hitDirection.m_quad, sub(distance, radiusSum));
    return true;
}

// The welding of a point at the triangle's edges: the triangle shape's weldContactPoint on copies of the point.
static inline int hkSphereTriangleAgent_weld(const hkpCdBody& bodyA, const hkpCdBody& bodyB, hkpFeatureOutput& featureOutput, Q position, Q normal, hkVector4f& weldedNormal)
{
    unsigned char numFeatures = featureOutput.numFeatures;
    alignas(16) hkVector4f contactPoint;
    contactPoint.m_quad = position;
    weldedNormal.m_quad = normal;
    const hkpShape* triangle = bodyB.getShape();
    return triangle->weldContactPoint(featureOutput.featureIds, numFeatures, contactPoint, &bodyB.getTransform(), static_cast<const hkpConvexShape*>(bodyA.getShape()), &bodyA.getTransform(), weldedNormal);
}

// 0x00DC0B70
void hkpSphereTriangleAgent::initAgentFunc(hkpCollisionDispatcher::AgentFuncs& af)
{
    af.m_createFunc = createSphereTriangleAgent;
    af.m_getPenetrationsFunc = staticGetPenetrations;
    af.m_getClosestPointFunc = staticGetClosestPoints;
    af.m_linearCastFunc = staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = false;
}

// 0x00DC0BA0
void hkpSphereTriangleAgent::initAgentFuncInverse(hkpCollisionDispatcher::AgentFuncs& af)
{
    af.m_createFunc = createTriangleSphereAgent;
    af.m_getPenetrationsFunc = hkpSymmetricAgentLinearCast<hkpSphereTriangleAgent>::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpSymmetricAgentLinearCast<hkpSphereTriangleAgent>::staticGetClosestPoints;
    af.m_linearCastFunc = hkpSymmetricAgentLinearCast<hkpSphereTriangleAgent>::staticLinearCast;
    af.m_isFlipped = true;
    af.m_isPredictive = false;
}

// 0x00DC0C30: agent2 entries only (registerCollisionAgent2), triangle-sphere first
void hkpSphereTriangleAgent::registerAgent2(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::AgentFuncs af;
    initAgentFuncInverse(af);
    dispatcher->registerCollisionAgent2(af, hkcdShapeType::TRIANGLE, hkcdShapeType::SPHERE);
    initAgentFunc(af);
    dispatcher->registerCollisionAgent2(af, hkcdShapeType::SPHERE, hkcdShapeType::TRIANGLE);
}

// 0x00DC0110: the closest point cache of B's triangle
hkpSphereTriangleAgent::hkpSphereTriangleAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
    : hkpIterativeLinearCastAgent(mgr)
{
    m_contactPointId = 0xFFFF;
    const hkpTriangleShape* triangle = static_cast<const hkpTriangleShape*>(bodyB.getShape());
    hkpCollideTriangleUtil::setupClosestPointTriangleCache(&triangle->m_vertexA, m_closestPointTriangleCache);
}

// 0x00DC0190
hkpSphereTriangleAgent::~hkpSphereTriangleAgent()
{
}

// 0x00DC01A0
hkpCollisionAgent* hkpSphereTriangleAgent::createSphereTriangleAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpSphereTriangleAgent(bodyA, bodyB, input, mgr);
}

// 0x00DC0240
hkpCollisionAgent* hkpSphereTriangleAgent::createTriangleSphereAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpSymmetricAgentLinearCast<hkpSphereTriangleAgent>(bodyA, bodyB, input, mgr);
}

// 0x00DC0200
void hkpSphereTriangleAgent::cleanup(hkpConstraintOwner& constraintOwner)
{
    if (m_contactPointId != 0xFFFF)
    {
        m_contactMgr->removeContactPointImpl(m_contactPointId, constraintOwner);
    }
    delete this;
}

// 0x00DC1040: a point the welding rejects removes the contact; a welded normal keeps the distance
void hkpSphereTriangleAgent::processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result)
{
    hkpProcessCdPoint* cp = result.m_firstFreeContactPoint;
    hkpFeatureOutput featureOutput;
    Q position;
    Q normal;
    if (hkSphereTriangleAgent_closest(bodyA, bodyB, input.m_tolerance, m_closestPointTriangleCache, &featureOutput, position, normal))
    {
        cp->m_contact.m_position.m_quad = position;
        cp->m_contact.m_separatingNormal.m_quad = normal;
        if (featureOutput.numFeatures)
        {
            alignas(16) hkVector4f weldedNormal;
            const int weld = hkSphereTriangleAgent_weld(bodyA, bodyB, featureOutput, position, normal, weldedNormal);
            if (weld == hkpConvexShape::WELD_RESULT_REJECT_CONTACT_POINT)
            {
                if (m_contactPointId != 0xFFFF)
                {
                    m_contactMgr->removeContactPointImpl(m_contactPointId, *result.m_constraintOwner);
                    m_contactPointId = 0xFFFF;
                }
                return;
            }
            if (weld == hkpConvexShape::WELD_RESULT_ACCEPT_CONTACT_POINT_MODIFIED)
            {
                cp->m_contact.m_separatingNormal.m_quad = setXYZ_W(weldedNormal.m_quad, cp->m_contact.m_separatingNormal.m_quad);
                cp->m_isShortestPoint = 0;
            }
        }
        if (m_contactPointId == 0xFFFF)
        {
            m_contactPointId = m_contactMgr->addContactPointImpl(bodyA, bodyB, input, result, nullptr, cp->m_contact);
        }
        if (m_contactPointId != 0xFFFF)
        {
            cp->m_contactPointId = m_contactPointId;
            result.commitContactPoints();
        }
    }
    else if (m_contactPointId != 0xFFFF)
    {
        m_contactMgr->removeContactPointImpl(m_contactPointId, *result.m_constraintOwner);
        m_contactPointId = 0xFFFF;
    }
}

// 0x00DC0710: with the agent's cache, not welded
void hkpSphereTriangleAgent::getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    Q position;
    Q normal;
    if (hkSphereTriangleAgent_closest(bodyA, bodyB, input.m_tolerance, m_closestPointTriangleCache, nullptr, position, normal))
    {
        hkpCdPoint point(bodyA, bodyB);
        point.m_contact.m_position.m_quad = position;
        point.m_contact.m_separatingNormal.m_quad = normal;
        point.m_unweldedNormal.m_quad = normal;
        collector.addCdPoint(point);
    }
}

// 0x00DC08D0: welded when the input asks for it (a rejected point is dropped unless points are force-accepted)
void hkpSphereTriangleAgent::staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    hkpCollideTriangleUtil::ClosestPointTriangleCache cache;
    const hkpTriangleShape* triangle = static_cast<const hkpTriangleShape*>(bodyB.getShape());
    hkpCollideTriangleUtil::setupClosestPointTriangleCache(&triangle->m_vertexA, cache);
    hkpFeatureOutput featureOutput;
    Q position;
    Q normal;
    if (!hkSphereTriangleAgent_closest(bodyA, bodyB, input.m_tolerance, cache, &featureOutput, position, normal))
    {
        return;
    }
    hkpCdPoint point(bodyA, bodyB);
    point.m_contact.m_position.m_quad = position;
    point.m_contact.m_separatingNormal.m_quad = normal;
    point.m_unweldedNormal.m_quad = normal;
    if (input.m_weldClosestPoints.val() && featureOutput.numFeatures)
    {
        alignas(16) hkVector4f weldedNormal;
        const int weld = hkSphereTriangleAgent_weld(bodyA, bodyB, featureOutput, position, normal, weldedNormal);
        if (!input.m_forceAcceptContactPoints.val() && weld == hkpConvexShape::WELD_RESULT_REJECT_CONTACT_POINT)
        {
            return;
        }
        if (weld == hkpConvexShape::WELD_RESULT_ACCEPT_CONTACT_POINT_MODIFIED)
        {
            point.m_contact.m_separatingNormal.m_quad = setXYZ_W(weldedNormal.m_quad, point.m_contact.m_separatingNormal.m_quad);
        }
    }
    collector.addCdPoint(point);
}

// The sphere's centre in the triangle's space against the triangle (no tolerance).
static inline bool hkSphereTriangleAgent_penetrates(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollideTriangleUtil::ClosestPointTriangleCache& cache)
{
    const hkpTriangleShape* triangle = static_cast<const hkpTriangleShape*>(bodyB.getShape());
    const hkTransformf& t = bodyB.getTransform();
    alignas(16) hkVector4f center;
    center.m_quad = rotateInverse(t.getRotation(), sub(bodyA.getTransform().getTranslation().m_quad, t.getTranslation().m_quad));
    hkpCollideTriangleUtil::ClosestPointTriangleResult result;
    hkpCollideTriangleUtil::closestPointTriangle(center, &triangle->m_vertexA, cache, result, nullptr);
    return lessOrNaN(splat(result.distance), hkSphereTriangleAgent_radiusSum(bodyA, bodyB));
}

// 0x00DC02A0: with the agent's cache
void hkpSphereTriangleAgent::getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    if (hkSphereTriangleAgent_penetrates(bodyA, bodyB, m_closestPointTriangleCache))
    {
        collector.addCdBodyPair(bodyA, bodyB);
    }
}

// 0x00DC0430
void hkpSphereTriangleAgent::staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    hkpCollideTriangleUtil::ClosestPointTriangleCache cache;
    const hkpTriangleShape* triangle = static_cast<const hkpTriangleShape*>(bodyB.getShape());
    hkpCollideTriangleUtil::setupClosestPointTriangleCache(&triangle->m_vertexA, cache);
    if (hkSphereTriangleAgent_penetrates(bodyA, bodyB, cache))
    {
        collector.addCdBodyPair(bodyA, bodyB);
    }
}
