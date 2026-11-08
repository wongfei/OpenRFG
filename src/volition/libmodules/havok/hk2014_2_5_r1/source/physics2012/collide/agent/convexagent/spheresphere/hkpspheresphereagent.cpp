// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/spheresphere/hkpspheresphereagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the sphere-sphere agent2 (one contact point), from rfg.exe. No SDK source exists
// for 2014.2.5; ported from the binary.
//
// The shipped build records the "TtSphereSphere" monitor timers (TLS hkMonitorStream); they are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/spheresphere/hkpspheresphereagent.h"

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
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/sphere/hkpsphereshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

static inline float hkSphereSphereAgent_radius(const hkpCdBody& body)
{
    return static_cast<const hkpSphereShape*>(body.getShape())->m_radius;
}

// The unit vector from B's centre to A's (+x when they coincide) and the distance less the radii in w.
static inline Q hkSphereSphereAgent_normal(Q d, Q len2, Q radiusSum)
{
    const Q inv = invSqrt23(len2);
    const Q positive = _mm_cmplt_ps(_mm_setzero_ps(), len2);
    const Q n = selectQ(positive, mul(inv, d), g_vectorfConstants[HK_QUADREAL_1000]);
    const Q distance = sub(_mm_and_ps(mul(inv, len2), positive), radiusSum);
    return setXYZ_W(n, distance);
}

// 0x00DC2F00
void hkpSphereSphereAgent::initAgentFunc(hkpCollisionDispatcher::AgentFuncs& af)
{
    af.m_createFunc = createSphereSphereAgent;
    af.m_getPenetrationsFunc = staticGetPenetrations;
    af.m_getClosestPointFunc = staticGetClosestPoints;
    af.m_linearCastFunc = staticLinearCast;
}

// 0x00DC2F20
void hkpSphereSphereAgent::registerAgent(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::AgentFuncs af;
    initAgentFunc(af);
    dispatcher->registerCollisionAgent(af, hkcdShapeType::SPHERE, hkcdShapeType::SPHERE);
}

// 0x00DC2FE0
hkpSphereSphereAgent::hkpSphereSphereAgent(hkpContactMgr* contactMgr)
    : hkpIterativeLinearCastAgent(contactMgr)
{
    m_contactPointId = 0xFFFF;
}

// 0x00DC3050
hkpSphereSphereAgent::~hkpSphereSphereAgent()
{
}

// 0x00DC2910
hkpCollisionAgent* hkpSphereSphereAgent::createSphereSphereAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* contactMgr)
{
    return new hkpSphereSphereAgent(contactMgr);
}

// 0x00DC2950
void hkpSphereSphereAgent::cleanup(hkpConstraintOwner& constraintOwner)
{
    if (m_contactPointId != 0xFFFF)
    {
        m_contactMgr->removeContactPointImpl(m_contactPointId, constraintOwner);
        m_contactPointId = 0xFFFF;
    }
    delete this;
}

// 0x00DC2990: the point on B's surface towards A within the tolerance; the id kept while touching
void hkpSphereSphereAgent::processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result)
{
    const Q posA = bodyA.getTransform().getTranslation().m_quad;
    const Q posB = bodyB.getTransform().getTranslation().m_quad;
    const Q d = sub(posA, posB);
    const Q len2 = dot3(d, d);
    const Q radiusSum = splat(hkSphereSphereAgent_radius(bodyA) + hkSphereSphereAgent_radius(bodyB));
    const Q limit = add(splat(input.m_tolerance), radiusSum);
    if (lessOrNaN(len2, mul(limit, limit)))
    {
        hkpProcessCdPoint* cp = result.m_firstFreeContactPoint;
        const Q normal = hkSphereSphereAgent_normal(d, len2, radiusSum);
        cp->m_contact.m_separatingNormal.m_quad = normal;
        cp->m_contact.m_position.m_quad = add(mul(splat(hkSphereSphereAgent_radius(bodyB)), normal), posB);
        cp->m_isShortestPoint = 0;
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

// 0x00DC2B50
void hkpSphereSphereAgent::getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    staticGetClosestPoints(bodyA, bodyB, input, collector);
}

// 0x00DC2CC0
void hkpSphereSphereAgent::staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    const Q posA = bodyA.getTransform().getTranslation().m_quad;
    const Q posB = bodyB.getTransform().getTranslation().m_quad;
    const Q d = sub(posA, posB);
    const Q len2 = dot3(d, d);
    const Q radiusSum = splat(hkSphereSphereAgent_radius(bodyA) + hkSphereSphereAgent_radius(bodyB));
    const Q limit = add(splat(input.m_tolerance), radiusSum);
    if (lessOrNaN(len2, mul(limit, limit)))
    {
        const Q normal = hkSphereSphereAgent_normal(d, len2, radiusSum);
        alignas(16) hkContactPoint contact;
        contact.m_position.m_quad = add(mul(splat(hkSphereSphereAgent_radius(bodyB)), normal), posB);
        contact.m_separatingNormal.m_quad = normal;
        hkpCdPoint point(bodyA, bodyB, contact);
        collector.addCdPoint(point);
    }
}

// 0x00DC2F80
void hkpSphereSphereAgent::getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    staticGetPenetrations(bodyA, bodyB, input, collector);
}

// 0x00DC2E30
void hkpSphereSphereAgent::staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    const Q d = sub(bodyB.getTransform().getTranslation().m_quad, bodyA.getTransform().getTranslation().m_quad);
    const Q radiusSum = splat(hkSphereSphereAgent_radius(bodyA) + hkSphereSphereAgent_radius(bodyB));
    if (lessOrNaN(dot3(d, d), mul(radiusSum, radiusSum)))
    {
        collector.addCdBodyPair(bodyA, bodyB);
    }
}
