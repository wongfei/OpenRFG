// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/capsulecapsule/hkpcapsulecapsuleagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the capsule-capsule agent2 (up to three contact points: the closest points of
// the axes and, for nearly parallel axes, the ends of the overlap), from rfg.exe. No SDK source exists for 2014.2.5;
// ported from the binary.
//
// The shipped build records the "TtCapsCaps" monitor timers (TLS hkMonitorStream); they are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/capsulecapsule/hkpcapsulecapsuleagent.h"

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
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/capsule/hkpcapsuleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/hkpcollidecapsuleutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

// The capsule's axis in world space.
static inline void hkCapsuleCapsuleAgent_worldAxis(const hkpCdBody& body, hkVector4f* axis)
{
    const hkpCapsuleShape* capsule = static_cast<const hkpCapsuleShape*>(body.getShape());
    const hkTransformf& t = body.getTransform();
    axis[1].m_quad = transformPos(t, capsule->m_vertexB.m_quad);
    axis[0].m_quad = transformPos(t, capsule->m_vertexA.m_quad);
}

// 0x00DBEA20
void hkpCapsuleCapsuleAgent::registerAgent(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::AgentFuncs af;
    af.m_createFunc = createCapsuleCapsuleAgent;
    af.m_getPenetrationsFunc = staticGetPenetrations;
    af.m_getClosestPointFunc = staticGetClosestPoints;
    af.m_linearCastFunc = staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = false;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::CAPSULE, hkcdShapeType::CAPSULE);
}

// 0x00DBE070
hkpCapsuleCapsuleAgent::hkpCapsuleCapsuleAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
    : hkpIterativeLinearCastAgent(mgr)
{
    m_contactPointId[0] = 0xFFFF;
    m_contactPointId[1] = 0xFFFF;
    m_contactPointId[2] = 0xFFFF;
}

// 0x00DBE0E0
hkpCapsuleCapsuleAgent::~hkpCapsuleCapsuleAgent()
{
}

// 0x00DBE0F0
hkpCollisionAgent* hkpCapsuleCapsuleAgent::createCapsuleCapsuleAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpCapsuleCapsuleAgent(bodyA, bodyB, input, mgr);
}

// 0x00DBE140
void hkpCapsuleCapsuleAgent::cleanup(hkpConstraintOwner& constraintOwner)
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

// 0x00DBEAC0
// Each of the three manifold points (its tolerance in the normal's w on input; a point the manifold leaves out stays
// at the tolerance) keeps its id while within the tolerance; the first point output is the shortest.
void hkpCapsuleCapsuleAgent::processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result)
{
    const Q tolerance = splat(input.m_tolerance);
    alignas(16) hkContactPoint points[3];
    for (int i = 0; i < 3; i++)
    {
        // Original behaviour kept: the points' xyz are left uninitialized, only w (the tolerance) is set.
        points[i].m_separatingNormal.m_quad = setXYZ_W(points[i].m_separatingNormal.m_quad, tolerance);
    }
    alignas(16) hkVector4f axisA[2];
    hkCapsuleCapsuleAgent_worldAxis(bodyA, axisA);
    alignas(16) hkVector4f axisB[2];
    hkCapsuleCapsuleAgent_worldAxis(bodyB, axisB);
    const float radiusA = static_cast<const hkpCapsuleShape*>(bodyA.getShape())->m_radius;
    const float radiusB = static_cast<const hkpCapsuleShape*>(bodyB.getShape())->m_radius;
    hkLineSegmentUtil::capsuleCapsuleManifold(axisA, radiusA, axisB, radiusB, points);

    unsigned int isShortestPoint = 0;
    for (int i = 0; i < 3; i++)
    {
        unsigned short& id = m_contactPointId[i];
        if (lessOrNaN(bcast<3>(points[i].m_separatingNormal.m_quad), tolerance))
        {
            if (id == 0xFFFF)
            {
                id = m_contactMgr->addContactPointImpl(bodyA, bodyB, input, result, nullptr, points[i]);
                if (id == 0xFFFF)
                {
                    continue;
                }
            }
            hkpProcessCdPoint* cp = result.m_firstFreeContactPoint;
            result.commitContactPoints();
            cp->m_contact.m_position = points[i].m_position;
            cp->m_contact.m_separatingNormal = points[i].m_separatingNormal;
            cp->m_contactPointId = id;
            cp->m_isShortestPoint = isShortestPoint;
            isShortestPoint = 1;
        }
        else if (id != 0xFFFF)
        {
            m_contactMgr->removeContactPointImpl(id, *result.m_constraintOwner);
            id = 0xFFFF;
        }
    }
}

// 0x00DBE440
void hkpCapsuleCapsuleAgent::getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    staticGetClosestPoints(bodyA, bodyB, input, collector);
}

// 0x00DBE630
void hkpCapsuleCapsuleAgent::staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    alignas(16) hkVector4f axisA[2];
    hkCapsuleCapsuleAgent_worldAxis(bodyA, axisA);
    alignas(16) hkVector4f axisB[2];
    hkCapsuleCapsuleAgent_worldAxis(bodyB, axisB);
    const hkpCapsuleShape* capsuleA = static_cast<const hkpCapsuleShape*>(bodyA.getShape());
    const hkpCapsuleShape* capsuleB = static_cast<const hkpCapsuleShape*>(bodyB.getShape());
    alignas(16) hkContactPoint contact;
    if (hkCollideCapsuleUtilClostestPointCapsVsCaps(axisA, capsuleA->m_radius, axisB, capsuleB->m_radius, input.m_tolerance, contact) == HK_SUCCESS)
    {
        hkpCdPoint point(bodyA, bodyB, contact);
        collector.addCdPoint(point);
    }
}

// 0x00DBEA00
void hkpCapsuleCapsuleAgent::getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    staticGetPenetrations(bodyA, bodyB, input, collector);
}

// 0x00DBE820: the axes closer than the radii (scalar compare)
void hkpCapsuleCapsuleAgent::staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    alignas(16) hkVector4f axisA[2];
    hkCapsuleCapsuleAgent_worldAxis(bodyA, axisA);
    alignas(16) hkVector4f axisB[2];
    hkCapsuleCapsuleAgent_worldAxis(bodyB, axisB);
    alignas(16) hkVector4f dA;
    dA.m_quad = sub(axisA[1].m_quad, axisA[0].m_quad);
    alignas(16) hkVector4f dB;
    dB.m_quad = sub(axisB[1].m_quad, axisB[0].m_quad);
    hkLineSegmentUtil::ClosestLineSegLineSegResult result;
    hkLineSegmentUtil::closestLineSegLineSeg(axisA[0], dA, axisB[0], dB, result);
    const float radiusSum = static_cast<const hkpCapsuleShape*>(bodyB.getShape())->m_radius + static_cast<const hkpCapsuleShape*>(bodyA.getShape())->m_radius;
    if (radiusSum * radiusSum > result.m_distanceSquared.val())
    {
        collector.addCdBodyPair(bodyA, bodyB);
    }
}
