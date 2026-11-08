// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/capsuletriangle/hkpcapsuletriangleagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the capsule-triangle agent2 (up to three contact points from
// hkCollideCapsuleUtilCapsVsTri, welded at the triangle's edges), from rfg.exe; triangle-capsule is
// hkpSymmetricAgentLinearCast<hkpCapsuleTriangleAgent>. No SDK source exists for 2014.2.5; ported from the binary.
//
// The shipped build records the "TtCapsTriangle"/"TtCapsuleTri" monitor timers; they are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/capsuletriangle/hkpcapsuletriangleagent.h"

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
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/linearcast/hkpiterativelinearcastagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/capsule/hkpcapsuleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/triangle/hkptriangleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/hkpcollidecapsuleutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagentlinearcast.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"

// 0x00DBD400
void hkpCapsuleTriangleAgent::initAgentFunc(hkpCollisionDispatcher::AgentFuncs& af)
{
    af.m_createFunc = createCapsuleTriangleAgent;
    af.m_getPenetrationsFunc = staticGetPenetrations;
    af.m_getClosestPointFunc = staticGetClosestPoints;
    af.m_linearCastFunc = hkpIterativeLinearCastAgent::staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = false;
}

// 0x00DBD430
void hkpCapsuleTriangleAgent::initAgentFuncInverse(hkpCollisionDispatcher::AgentFuncs& af)
{
    af.m_createFunc = createTriangleCapsuleAgent;
    af.m_getPenetrationsFunc = hkpSymmetricAgentLinearCast<hkpCapsuleTriangleAgent>::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpSymmetricAgentLinearCast<hkpCapsuleTriangleAgent>::staticGetClosestPoints;
    af.m_linearCastFunc = hkpSymmetricAgentLinearCast<hkpCapsuleTriangleAgent>::staticLinearCast;
    af.m_isFlipped = true;
    af.m_isPredictive = false;
}

// 0x00DBD460
void hkpCapsuleTriangleAgent::registerAgent(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::AgentFuncs af;
    initAgentFuncInverse(af);
    dispatcher->registerCollisionAgent(af, hkcdShapeType::TRIANGLE, hkcdShapeType::CAPSULE);
    initAgentFunc(af);
    dispatcher->registerCollisionAgent(af, hkcdShapeType::CAPSULE, hkcdShapeType::TRIANGLE);
}

// 0x00DBD4C0
void hkpCapsuleTriangleAgent::registerAgent2(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::AgentFuncs af;
    initAgentFuncInverse(af);
    dispatcher->registerCollisionAgent2(af, hkcdShapeType::TRIANGLE, hkcdShapeType::CAPSULE);
    initAgentFunc(af);
    dispatcher->registerCollisionAgent2(af, hkcdShapeType::CAPSULE, hkcdShapeType::TRIANGLE);
}

using namespace hkGskExact;

// The capsule's axis and the triangle in world space, through hkCollideCapsuleUtilCapsVsTri.
static void hkCapsuleTriangleAgent_capsVsTri(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollideTriangleUtil::PointTriangleDistanceCache& cache, float tolerance, int searchManifold, hkContactPoint* points, hkpFeatureOutput* featuresOut)
{
    const hkpCapsuleShape* capsule = static_cast<const hkpCapsuleShape*>(bodyA.getShape());
    const hkpTriangleShape* triangle = static_cast<const hkpTriangleShape*>(bodyB.getShape());
    const hkTransformf& tA = bodyA.getTransform();
    const hkTransformf& tB = bodyB.getTransform();
    alignas(16) hkVector4f axis[2];
    axis[1].m_quad = transformPos(tA, capsule->m_vertexB.m_quad);
    axis[0].m_quad = transformPos(tA, capsule->m_vertexA.m_quad);
    alignas(16) hkVector4f tri[3];
    tri[2].m_quad = transformPos(tB, triangle->m_vertexC.m_quad);
    tri[1].m_quad = transformPos(tB, triangle->m_vertexB.m_quad);
    tri[0].m_quad = transformPos(tB, triangle->m_vertexA.m_quad);
    hkCollideCapsuleUtilCapsVsTri(axis, capsule->m_radius, tri, triangle->m_radius, cache, tolerance, searchManifold, points, featuresOut);
}

// The closer of the first two points within the tolerance (the first on ties lost), welded when the input asks for
// it: a rejected point is dropped unless points are force-accepted.
static void hkCapsuleTriangleAgent_closestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, const hkpCollideTriangleUtil::PointTriangleDistanceCache& cache, hkpCdPointCollector& collector)
{
    alignas(16) hkContactPoint points[3];
    hkpFeatureOutput features[3];
    hkCapsuleTriangleAgent_capsVsTri(bodyA, bodyB, cache, input.m_tolerance.val(), 0, points, features);
    const float tolerance = input.m_tolerance.val();
    const float w0 = lane0(bcast<3>(points[0].m_separatingNormal.m_quad));
    const float w1 = lane0(bcast<3>(points[1].m_separatingNormal.m_quad));
    int i;
    if (w0 < w1 && w0 < tolerance)
    {
        i = 0;
    }
    else if (!(w0 < w1) && w1 < tolerance)
    {
        i = 1;
    }
    else
    {
        return;
    }
    const Q unwelded = points[i].m_separatingNormal.m_quad;
    unsigned char numFeatures = features[i].numFeatures;
    if (input.m_weldClosestPoints.val() && numFeatures)
    {
        alignas(16) hkVector4f contactPoint;
        contactPoint.m_quad = points[i].m_position.m_quad;
        alignas(16) hkVector4f weldedNormal;
        weldedNormal.m_quad = points[i].m_separatingNormal.m_quad;
        const int weld = bodyB.getShape()->weldContactPoint(features[i].featureIds, numFeatures, contactPoint, &bodyB.getTransform(), static_cast<const hkpConvexShape*>(bodyA.getShape()), &bodyA.getTransform(), weldedNormal);
        points[i].m_position.m_quad = contactPoint.m_quad;
        if (!input.m_forceAcceptContactPoints.val() && weld == hkpConvexShape::WELD_RESULT_REJECT_CONTACT_POINT)
        {
            return;
        }
        if (weld == hkpConvexShape::WELD_RESULT_ACCEPT_CONTACT_POINT_MODIFIED)
        {
            points[i].m_separatingNormal.m_quad = setXYZ_W(weldedNormal.m_quad, points[i].m_separatingNormal.m_quad);
        }
    }
    hkpCdPoint point(bodyA, bodyB);
    point.m_contact.m_position.m_quad = points[i].m_position.m_quad;
    point.m_contact.m_separatingNormal.m_quad = points[i].m_separatingNormal.m_quad;
    point.m_unweldedNormal.m_quad = unwelded;
    collector.addCdPoint(point);
}

// Either of the first two points penetrating (no tolerance).
static bool hkCapsuleTriangleAgent_penetrates(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, const hkpCollideTriangleUtil::PointTriangleDistanceCache& cache)
{
    alignas(16) hkContactPoint points[3];
    hkCapsuleTriangleAgent_capsVsTri(bodyA, bodyB, cache, input.m_tolerance.val(), 0, points, nullptr);
    const Q zero = _mm_setzero_ps();
    const Q m = _mm_or_ps(_mm_cmplt_ps(bcast<3>(points[1].m_separatingNormal.m_quad), zero), _mm_cmplt_ps(bcast<3>(points[0].m_separatingNormal.m_quad), zero));
    return _mm_movemask_ps(m) != 0;
}

// 0x00DBC2D0: no contact points yet, the distance cache of B's triangle
hkpCapsuleTriangleAgent::hkpCapsuleTriangleAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
    : hkpIterativeLinearCastAgent(mgr)
{
    m_contactPointId[0] = 0xFFFF;
    m_contactPointId[1] = 0xFFFF;
    m_contactPointId[2] = 0xFFFF;
    const hkpTriangleShape* triangle = static_cast<const hkpTriangleShape*>(bodyB.getShape());
    hkpCollideTriangleUtil::setupPointTriangleDistanceCache(&triangle->m_vertexA, m_triangleCache);
}

// 0x00DBC360
hkpCapsuleTriangleAgent::~hkpCapsuleTriangleAgent()
{
}

// 0x00DBC380
hkpCollisionAgent* hkpCapsuleTriangleAgent::createCapsuleTriangleAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpCapsuleTriangleAgent(bodyA, bodyB, input, mgr);
}

// 0x00DBC440
hkpCollisionAgent* hkpCapsuleTriangleAgent::createTriangleCapsuleAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpSymmetricAgentLinearCast<hkpCapsuleTriangleAgent>(bodyA, bodyB, input, mgr);
}

// 0x00DBC3F0
void hkpCapsuleTriangleAgent::cleanup(hkpConstraintOwner& constraintOwner)
{
    for (int i = 0; i < 3; i++)
    {
        if (m_contactPointId[i] != 0xFFFF)
        {
            m_contactMgr->removeContactPointImpl(m_contactPointId[i], constraintOwner);
        }
    }
    delete this;
}

// 0x00DBDB20: the manifold of up to three points; each point within the tolerance is welded (a rejected one loses
// its contact) and gets its contact. Original behaviour kept: m_isShortestPoint is set on every point except the
// closer of the first two (the third is never the closest).
void hkpCapsuleTriangleAgent::processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result)
{
    alignas(16) hkContactPoint points[3];
    hkpFeatureOutput features[3];
    hkCapsuleTriangleAgent_capsVsTri(bodyA, bodyB, m_triangleCache, input.m_tolerance.val(), 1, points, features);
    const int shortest = greaterEqual(bcast<3>(points[0].m_separatingNormal.m_quad), bcast<3>(points[1].m_separatingNormal.m_quad)) ? 1 : 0;
    const Q tolerance = splat(input.m_tolerance.val());
    for (int i = 0; i < 3; i++)
    {
        unsigned short& id = m_contactPointId[i];
        if (greaterEqual(bcast<3>(points[i].m_separatingNormal.m_quad), tolerance))
        {
            if (id != 0xFFFF)
            {
                m_contactMgr->removeContactPointImpl(id, *result.m_constraintOwner);
                id = 0xFFFF;
            }
            continue;
        }
        unsigned char numFeatures = features[i].numFeatures;
        if (numFeatures)
        {
            alignas(16) hkVector4f contactPoint;
            contactPoint.m_quad = points[i].m_position.m_quad;
            alignas(16) hkVector4f weldedNormal;
            weldedNormal.m_quad = points[i].m_separatingNormal.m_quad;
            const int weld = bodyB.getShape()->weldContactPoint(features[i].featureIds, numFeatures, contactPoint, &bodyB.getTransform(), static_cast<const hkpConvexShape*>(bodyA.getShape()), &bodyA.getTransform(), weldedNormal);
            points[i].m_position.m_quad = contactPoint.m_quad;
            if (weld == hkpConvexShape::WELD_RESULT_REJECT_CONTACT_POINT)
            {
                if (id != 0xFFFF)
                {
                    m_contactMgr->removeContactPointImpl(id, *result.m_constraintOwner);
                    id = 0xFFFF;
                }
                continue;
            }
            if (weld == hkpConvexShape::WELD_RESULT_ACCEPT_CONTACT_POINT_MODIFIED)
            {
                points[i].m_separatingNormal.m_quad = setXYZ_W(weldedNormal.m_quad, points[i].m_separatingNormal.m_quad);
            }
        }
        if (id == 0xFFFF)
        {
            id = m_contactMgr->addContactPointImpl(bodyA, bodyB, input, result, nullptr, points[i]);
            if (id == 0xFFFF)
            {
                continue;
            }
        }
        hkpProcessCdPoint* cp = result.m_firstFreeContactPoint;
        result.m_firstFreeContactPoint = cp + 1;
        cp->m_contact.m_position.m_quad = points[i].m_position.m_quad;
        cp->m_contact.m_separatingNormal.m_quad = points[i].m_separatingNormal.m_quad;
        cp->m_contactPointId = id;
        cp->m_isShortestPoint = i != shortest ? 1 : 0;
    }
}

// 0x00DBC8E0: with the agent's cache
void hkpCapsuleTriangleAgent::getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    hkCapsuleTriangleAgent_closestPoints(bodyA, bodyB, input, m_triangleCache, collector);
}

// 0x00DBCC60
void hkpCapsuleTriangleAgent::staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    hkpCollideTriangleUtil::PointTriangleDistanceCache cache;
    hkpCollideTriangleUtil::setupPointTriangleDistanceCache(&static_cast<const hkpTriangleShape*>(bodyB.getShape())->m_vertexA, cache);
    hkCapsuleTriangleAgent_closestPoints(bodyA, bodyB, input, cache, collector);
}

// 0x00DBD000: with the agent's cache
void hkpCapsuleTriangleAgent::getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    if (hkCapsuleTriangleAgent_penetrates(bodyA, bodyB, input, m_triangleCache))
    {
        collector.addCdBodyPair(bodyA, bodyB);
    }
}

// 0x00DBD200
void hkpCapsuleTriangleAgent::staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    hkpCollideTriangleUtil::PointTriangleDistanceCache cache;
    hkpCollideTriangleUtil::setupPointTriangleDistanceCache(&static_cast<const hkpTriangleShape*>(bodyB.getShape())->m_vertexA, cache);
    if (hkCapsuleTriangleAgent_penetrates(bodyA, bodyB, input, cache))
    {
        collector.addCdBodyPair(bodyA, bodyB);
    }
}
