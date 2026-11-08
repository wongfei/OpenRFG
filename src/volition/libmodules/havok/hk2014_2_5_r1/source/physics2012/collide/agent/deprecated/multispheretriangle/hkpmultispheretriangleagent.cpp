// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/deprecated/multispheretriangle/hkpmultispheretriangleagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6, stub_impl.md S4): the (deprecated) multi-sphere vs triangle agent, from rfg.exe.
// Each sphere of A against triangle B through hkpCollideTriangleUtil::closestPointTriangle (the cache is set up from
// the triangle in its own space); one contact point id per sphere. No RFG data has a multi-sphere shape
// (hkpMultiSphereShape is not ported), so nothing creates this agent.
//
// The shipped build records the "MultiSphereTriangle" monitor timers (TLS hkMonitorStream); they are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/deprecated/multispheretriangle/hkpmultispheretriangleagent.h"

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
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagentlinearcast.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/triangle/hkptriangleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/deprecated/multisphere/hkpmultisphereshape.h"

typedef hkpCollideTriangleUtil::ClosestPointTriangleResult hkMultiSphereTriangle_Result;

// hkVector4Util::transformPoints (inlined, last point first): ((x * c0 + y * c1) + z * c2) + t in all four lanes
static inline void hkMultiSphereTriangle_transformPoints(const hkTransformf& t, const hkVector4f* in, int num, hkVector4f* out)
{
    for (int i = num - 1; i >= 0; i--)
    {
        const __m128 v = in[i].m_quad;
        const __m128 x = _mm_mul_ps(_mm_shuffle_ps(v, v, 0x00), t.m_rotation.m_col0.m_quad);
        const __m128 y = _mm_mul_ps(_mm_shuffle_ps(v, v, 0x55), t.m_rotation.m_col1.m_quad);
        const __m128 z = _mm_mul_ps(_mm_shuffle_ps(v, v, 0xAA), t.m_rotation.m_col2.m_quad);
        out[i].m_quad = _mm_add_ps(_mm_add_ps(_mm_add_ps(y, x), z), t.m_translation.m_quad);
    }
}

// The world triangle of B and the world spheres of A (radius lanes transformed too, unused).
static inline int hkMultiSphereTriangle_setup(const hkpCdBody& bodyA, const hkpCdBody& bodyB, hkVector4f* triVertices, hkVector4f* worldSpheres)
{
    const hkpTriangleShape* triB = static_cast<const hkpTriangleShape*>(bodyB.getShape());
    const hkpMultiSphereShape* msA = static_cast<const hkpMultiSphereShape*>(bodyA.getShape());
    hkMultiSphereTriangle_transformPoints(bodyB.getTransform(), &triB->m_vertexA, 3, triVertices);
    const int numSpheres = msA->getNumSpheres();
    hkMultiSphereTriangle_transformPoints(bodyA.getTransform(), msA->getSpheres(), numSpheres, worldSpheres);
    return numSpheres;
}

// The contact of a sphere: on the triangle's surface (radius - distance along the hit direction from the sphere's
// center), the hit direction as the normal, the distance less the radius sum.
static inline void hkMultiSphereTriangle_contact(const hkVector4f& sphere, const hkMultiSphereTriangle_Result& r, float triRadius, float radiusSum, hkContactPoint& cp)
{
    cp.m_position.m_quad = _mm_add_ps(_mm_mul_ps(_mm_set1_ps(triRadius - r.distance), r.hitDirection.m_quad), sphere.m_quad);
    cp.m_separatingNormal.m_quad = r.hitDirection.m_quad;
    cp.m_separatingNormal.m_quad.m128_f32[3] = r.distance - radiusSum;
}

// 0x00DBB400: the triangle side symmetric and not predictive
void hkpMultiSphereTriangleAgent::registerAgent(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::AgentFuncs af;
    af.m_createFunc = createTriangleMultiSphereAgent;
    af.m_getPenetrationsFunc = hkpSymmetricAgentLinearCast<hkpMultiSphereTriangleAgent>::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpSymmetricAgentLinearCast<hkpMultiSphereTriangleAgent>::staticGetClosestPoints;
    af.m_linearCastFunc = hkpSymmetricAgentLinearCast<hkpMultiSphereTriangleAgent>::staticLinearCast;
    af.m_isFlipped = true;
    af.m_isPredictive = false;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::TRIANGLE, hkcdShapeType::MULTI_SPHERE);

    af.m_createFunc = createMultiSphereTriangleAgent;
    af.m_getPenetrationsFunc = staticGetPenetrations;
    af.m_getClosestPointFunc = staticGetClosestPoints;
    af.m_linearCastFunc = hkpIterativeLinearCastAgent::staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = false;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::MULTI_SPHERE, hkcdShapeType::TRIANGLE);
}

// 0x00DBAA20 (inlined in the create functions): no contact points yet
hkpMultiSphereTriangleAgent::hkpMultiSphereTriangleAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* contactMgr)
    : hkpIterativeLinearCastAgent(contactMgr)
{
    for (int i = 0; i < 8; i++)
    {
        m_contactPointId[i] = 0xFFFF; // HK_INVALID_CONTACT_POINT
    }
    const hkpTriangleShape* triB = static_cast<const hkpTriangleShape*>(bodyB.getShape());
    hkpCollideTriangleUtil::setupClosestPointTriangleCache(&triB->m_vertexA, m_closestPointTriangleCache);
}

// 0x00DBAAB0
hkpMultiSphereTriangleAgent::~hkpMultiSphereTriangleAgent()
{
}

// 0x00DBAAC0
hkpCollisionAgent* hkpMultiSphereTriangleAgent::createMultiSphereTriangleAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* contactMgr)
{
    return new hkpMultiSphereTriangleAgent(bodyA, bodyB, input, contactMgr);
}

// 0x00DBAB80
hkpCollisionAgent* hkpMultiSphereTriangleAgent::createTriangleMultiSphereAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* contactMgr)
{
    return new hkpSymmetricAgentLinearCast<hkpMultiSphereTriangleAgent>(bodyA, bodyB, input, contactMgr);
}

// 0x00DBAB30: the points still held, then the agent itself
void hkpMultiSphereTriangleAgent::cleanup(hkpConstraintOwner& constraintOwner)
{
    for (int i = 0; i < 8; i++)
    {
        if (m_contactPointId[i] != 0xFFFF)
        {
            m_contactMgr->removeContactPoint(m_contactPointId[i], constraintOwner);
        }
    }
    delete this;
}

// 0x00DBB670
// Original behaviour kept: sphere i keeps its point in slot numSpheres - 1 - i.
void hkpMultiSphereTriangleAgent::processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result)
{
    const hkpTriangleShape* triB = static_cast<const hkpTriangleShape*>(bodyB.getShape());
    const hkpMultiSphereShape* msA = static_cast<const hkpMultiSphereShape*>(bodyA.getShape());
    alignas(16) hkVector4f triVertices[3];
    alignas(16) hkVector4f worldSpheres[hkpMultiSphereShape::MAX_SPHERES];
    const int numSpheres = hkMultiSphereTriangle_setup(bodyA, bodyB, triVertices, worldSpheres);
    const hkVector4f* localSpheres = msA->getSpheres();
    unsigned short* id = &m_contactPointId[numSpheres - 1];
    for (int i = 0; i < numSpheres; i++, id--)
    {
        const float radiusSum = localSpheres[i].m_quad.m128_f32[3] + triB->m_radius;
        hkMultiSphereTriangle_Result cptr;
        hkpCollideTriangleUtil::closestPointTriangle(worldSpheres[i], triVertices, m_closestPointTriangleCache, cptr);
        if (input.m_tolerance.val() + radiusSum > cptr.distance)
        {
            hkpProcessCdPoint* point = result.m_firstFreeContactPoint.val();
            hkMultiSphereTriangle_contact(worldSpheres[i], cptr, triB->m_radius, radiusSum, point->m_contact);
            point->m_isShortestPoint = 2; // as in the binary
            if (*id == 0xFFFF)
            {
                *id = m_contactMgr->addContactPoint(bodyA, bodyB, input, result, nullptr, point->m_contact);
                if (*id == 0xFFFF)
                {
                    continue;
                }
            }
            result.m_firstFreeContactPoint = point + 1;
            point->m_contactPointId = *id;
        }
        else if (*id != 0xFFFF)
        {
            m_contactMgr->removeContactPoint(*id, *result.m_constraintOwner.val());
            *id = 0xFFFF;
        }
    }
}

// 0x00DBAC00
void hkpMultiSphereTriangleAgent::getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    const hkpTriangleShape* triB = static_cast<const hkpTriangleShape*>(bodyB.getShape());
    const hkpMultiSphereShape* msA = static_cast<const hkpMultiSphereShape*>(bodyA.getShape());
    alignas(16) hkVector4f triVertices[3];
    alignas(16) hkVector4f worldSpheres[hkpMultiSphereShape::MAX_SPHERES];
    const int numSpheres = hkMultiSphereTriangle_setup(bodyA, bodyB, triVertices, worldSpheres);
    const hkVector4f* localSpheres = msA->getSpheres();
    for (int i = 0; i < numSpheres; i++)
    {
        const float radiusSum = triB->m_radius + localSpheres[i].m_quad.m128_f32[3];
        hkMultiSphereTriangle_Result cptr;
        hkpCollideTriangleUtil::closestPointTriangle(worldSpheres[i], triVertices, m_closestPointTriangleCache, cptr);
        if (cptr.distance < radiusSum + input.m_tolerance.val())
        {
            hkpCdPoint event(bodyA, bodyB);
            hkMultiSphereTriangle_contact(worldSpheres[i], cptr, triB->m_radius, radiusSum, event.m_contact);
            event.m_unweldedNormal = event.m_contact.m_separatingNormal;
            collector.addCdPoint(event);
        }
    }
}

// 0x00DBAE60: as getClosestPoints with a cache of its own
void hkpMultiSphereTriangleAgent::staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    const hkpTriangleShape* triB = static_cast<const hkpTriangleShape*>(bodyB.getShape());
    const hkpMultiSphereShape* msA = static_cast<const hkpMultiSphereShape*>(bodyA.getShape());
    alignas(16) hkVector4f triVertices[3];
    alignas(16) hkVector4f worldSpheres[hkpMultiSphereShape::MAX_SPHERES];
    const int numSpheres = hkMultiSphereTriangle_setup(bodyA, bodyB, triVertices, worldSpheres);
    hkpCollideTriangleUtil::ClosestPointTriangleCache cache;
    hkpCollideTriangleUtil::setupClosestPointTriangleCache(&triB->m_vertexA, cache);
    const hkVector4f* localSpheres = msA->getSpheres();
    for (int i = 0; i < numSpheres; i++)
    {
        const float radiusSum = triB->m_radius + localSpheres[i].m_quad.m128_f32[3];
        hkMultiSphereTriangle_Result cptr;
        hkpCollideTriangleUtil::closestPointTriangle(worldSpheres[i], triVertices, cache, cptr);
        if (cptr.distance < input.m_tolerance.val() + radiusSum)
        {
            hkpCdPoint event(bodyA, bodyB);
            hkMultiSphereTriangle_contact(worldSpheres[i], cptr, triB->m_radius, radiusSum, event.m_contact);
            event.m_unweldedNormal = event.m_contact.m_separatingNormal;
            collector.addCdPoint(event);
        }
    }
}

// 0x00DBB0D0: one pair as soon as a sphere penetrates (no tolerance)
void hkpMultiSphereTriangleAgent::getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    const hkpTriangleShape* triB = static_cast<const hkpTriangleShape*>(bodyB.getShape());
    const hkpMultiSphereShape* msA = static_cast<const hkpMultiSphereShape*>(bodyA.getShape());
    alignas(16) hkVector4f triVertices[3];
    alignas(16) hkVector4f worldSpheres[hkpMultiSphereShape::MAX_SPHERES];
    const int numSpheres = hkMultiSphereTriangle_setup(bodyA, bodyB, triVertices, worldSpheres);
    const hkVector4f* localSpheres = msA->getSpheres();
    for (int i = 0; i < numSpheres; i++)
    {
        const float radiusSum = triB->m_radius + localSpheres[i].m_quad.m128_f32[3];
        hkMultiSphereTriangle_Result cptr;
        hkpCollideTriangleUtil::closestPointTriangle(worldSpheres[i], triVertices, m_closestPointTriangleCache, cptr);
        if (cptr.distance < radiusSum)
        {
            collector.addCdBodyPair(bodyA, bodyB);
            return;
        }
    }
}

// 0x00DBB270: as getPenetrations with a cache of its own
void hkpMultiSphereTriangleAgent::staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    const hkpTriangleShape* triB = static_cast<const hkpTriangleShape*>(bodyB.getShape());
    const hkpMultiSphereShape* msA = static_cast<const hkpMultiSphereShape*>(bodyA.getShape());
    alignas(16) hkVector4f triVertices[3];
    alignas(16) hkVector4f worldSpheres[hkpMultiSphereShape::MAX_SPHERES];
    const int numSpheres = hkMultiSphereTriangle_setup(bodyA, bodyB, triVertices, worldSpheres);
    hkpCollideTriangleUtil::ClosestPointTriangleCache cache;
    hkpCollideTriangleUtil::setupClosestPointTriangleCache(&triB->m_vertexA, cache);
    const hkVector4f* localSpheres = msA->getSpheres();
    for (int i = 0; i < numSpheres; i++)
    {
        const float radiusSum = triB->m_radius + localSpheres[i].m_quad.m128_f32[3];
        hkMultiSphereTriangle_Result cptr;
        hkpCollideTriangleUtil::closestPointTriangle(worldSpheres[i], triVertices, cache, cptr);
        if (cptr.distance < radiusSum)
        {
            collector.addCdBodyPair(bodyA, bodyB);
            return;
        }
    }
}
