// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/gjk/hkpgskbaseagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the base of the convex-convex GSK agents (agent2), from rfg.exe: the cached GSK
// queries (closest points, penetrations) and their static versions with a fresh cache. No SDK source exists for
// 2014.2.5; ported from the binary.
//
// The shipped build records the "Gsk"/"Sttim"/"StSepNormal" monitor timers (TLS hkMonitorStream); they are not
// reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/gjk/hkpgskbaseagent.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdbodypaircollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/hkpconvexshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/triangle/hkptriangleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgsk.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;


static inline const hkpConvexShape* hkGskBaseAgent_shape(const hkpCdBody& body)
{
    return static_cast<const hkpConvexShape*>(body.getShape());
}

// The cache of a new pair: a triangle B gets its triangle feature.
static void hkGskBaseAgent_initCache(hkpGskCache& cache, const hkpConvexShape* shapeA, const hkpConvexShape* shapeB, const hkTransformf& aTb)
{
    if (shapeB->m_type.m_storage == hkcdShapeType::TRIANGLE)
    {
        cache.initTriangle(shapeA, static_cast<const hkpTriangleShape*>(shapeB), aTb);
    }
    else
    {
        cache.init(shapeA, shapeB, aTb);
    }
}

// The output from the GSK's features: the normal in world space and the distance less the radii. False beyond the
// tolerance; otherwise the point on A is moved onto A's surface and the point on B (in B's space) derived.
static bool hkGskBaseAgent_exportOutput(hkpGsk& gsk, const hkVector4f& separatingNormal, const hkTransformf& transformA, const hkTransformf& aTb, const hkpConvexShape* shapeA, const hkpConvexShape* shapeB, float tolerance, bool negatedRadius, hkpExtendedGskOut& output)
{
    gsk.convertFeatureToClosestDistance(separatingNormal, output);
    const Q n = output.m_normalInA.m_quad;
    output.m_normalInWorld.m_quad = rotate(transformA.getRotation(), n);
    output.m_unweldedNormalInWorld = output.m_normalInWorld;
    const float dist = output.m_distance.val() - shapeA->m_radius - shapeB->m_radius;
    output.m_distance = dist;
    if (!(tolerance > dist))
    {
        return false;
    }
    if (negatedRadius)
    {
        // the static version adds -radius * n (the same bits)
        output.m_pointAinA.m_quad = add(mul(splat(-shapeA->m_radius), n), output.m_pointAinA.m_quad);
    }
    else
    {
        output.m_pointAinA.m_quad = sub(output.m_pointAinA.m_quad, mul(splat(shapeA->m_radius), n));
    }
    const Q pointBinA = sub(add(mul(splat(-dist), n), output.m_pointAinA.m_quad), aTb.getTranslation().m_quad);
    output.m_pointBinB.m_quad = rotateInverse(aTb.getRotation(), pointBinA);
    return true;
}

// 0x00DCFC60
hkpGskBaseAgent::hkpGskBaseAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, hkpContactMgr* mgr)
    : hkpIterativeLinearCastAgent(mgr)
{
    alignas(16) hkTransformf aTb;
    aTb.setMulInverseMul(bodyA.getTransform(), bodyB.getTransform());
    hkGskBaseAgent_initCache(m_cache, hkGskBaseAgent_shape(bodyA), hkGskBaseAgent_shape(bodyB), aTb);
    m_separatingNormal.m_quad = _mm_setzero_ps();
    m_separatingNormal.m_quad.m128_f32[3] = -1.0f;
    m_timeOfSeparatingNormal = -1.0f;
    const float a = bodyA.getRootCollidable()->m_allowedPenetrationDepth;
    const float b = bodyB.getRootCollidable()->m_allowedPenetrationDepth;
    m_allowedPenetration = (a < b) ? a : b; // minss
}

// 0x00DB70C0
hkpGskBaseAgent::~hkpGskBaseAgent()
{
}

// 0x00DCFBD0: not used (the concrete agents process)
void hkpGskBaseAgent::processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result)
{
}

// 0x00DCFBE0
void hkpGskBaseAgent::cleanup(hkpConstraintOwner& constraintOwner)
{
    delete this;
}

// 0x00DCFC00
void hkpGskBaseAgent::invalidateTim(const hkpCollisionInput& input)
{
    m_separatingNormal.m_quad = _mm_setzero_ps();
    m_timeOfSeparatingNormal = -1.0f;
}

// 0x00DCFC20: a normal of oldTime moves to newTime, any other is invalidated
void hkpGskBaseAgent::warpTime(float oldTime, float newTime, const hkpCollisionInput& input)
{
    if (m_timeOfSeparatingNormal == oldTime)
    {
        m_timeOfSeparatingNormal = newTime;
        return;
    }
    m_timeOfSeparatingNormal = -1.0f;
    m_separatingNormal.m_quad = _mm_setzero_ps();
}

// 0x00DCFD40: the separating normal in world space with the distance less the radii in w (earlyOutTolerance unused)
void hkpGskBaseAgent::calcSeparatingNormal(const hkpCdBody& bodyA, const hkpCdBody& bodyB, float earlyOutTolerance, hkpGsk& gsk, hkVector4f& separatingNormalOut)
{
    const hkpConvexShape* shapeA = hkGskBaseAgent_shape(bodyA);
    const hkpConvexShape* shapeB = hkGskBaseAgent_shape(bodyB);
    alignas(16) hkTransformf aTb;
    aTb.setMulInverseMul(bodyA.getTransform(), bodyB.getTransform());
    hkVector4f normal;
    gsk.getClosestFeature(shapeA, shapeB, aTb, normal);
    separatingNormalOut.m_quad = rotate(bodyA.getTransform().getRotation(), normal.m_quad);
    separatingNormalOut.m_quad.m128_f32[3] = normal.m_quad.m128_f32[3] - shapeA->m_radius - shapeB->m_radius;
}

// 0x00DD0910 (.inl, the out-of-line copy of getClosestPoint)
hkBool hkpGskBaseAgent::_getClosestPoint(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpExtendedGskOut& output)
{
    return getClosestPoint(bodyA, bodyB, input, output);
}

// 0x00DD0170: the cached query (the GSK status is not checked)
hkBool hkpGskBaseAgent::getClosestPoint(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpExtendedGskOut& output)
{
    const hkpConvexShape* shapeA = hkGskBaseAgent_shape(bodyA);
    const hkpConvexShape* shapeB = hkGskBaseAgent_shape(bodyB);
    alignas(16) hkTransformf aTb;
    aTb.setMulInverseMul(bodyA.getTransform(), bodyB.getTransform());
    hkpGsk gsk;
    gsk.init(shapeA, shapeB, m_cache);
    hkVector4f normal;
    gsk.getClosestFeature(shapeA, shapeB, aTb, normal);
    gsk.checkForChangesAndUpdateCache(m_cache);
    return hkGskBaseAgent_exportOutput(gsk, normal, bodyA.getTransform(), aTb, shapeA, shapeB, input.m_tolerance, false, output);
}

// 0x00DD0AF0
// A query from the given cache, which is updated only when B is a welded triangle. Penetrations fail.
hkBool hkpGskBaseAgent::staticGetClosestPoint(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkTransformf& aTb, const hkpCollisionInput& input, hkpGskCache& cache, hkpExtendedGskOut& output)
{
    const hkpConvexShape* shapeA = hkGskBaseAgent_shape(bodyA);
    const hkpConvexShape* shapeB = hkGskBaseAgent_shape(bodyB);
    hkpGsk gsk;
    gsk.init(shapeA, shapeB, cache);
    hkVector4f normal;
    if (gsk.getClosestFeature(shapeA, shapeB, aTb, normal) != HK_GSK_OK)
    {
        return false;
    }
    if (!hkGskBaseAgent_exportOutput(gsk, normal, bodyA.getTransform(), aTb, shapeA, shapeB, input.m_tolerance, true, output))
    {
        return false;
    }
    if (shapeB->m_type.m_storage == hkcdShapeType::TRIANGLE && input.m_weldClosestPoints.val())
    {
        gsk.checkForChangesAndUpdateCache(cache);
        hkVector4f weldedNormal = output.m_normalInWorld;
        hkVector4f position;
        position.m_quad = transformPos(bodyB.getTransform(), output.m_pointBinB.m_quad);
        const int result = shapeB->weldContactPoint(&cache.m_vertices[cache.m_dimA], cache.m_dimB, position, &bodyB.getTransform(), shapeA, &bodyA.getTransform(), weldedNormal);
        if (!input.m_forceAcceptContactPoints.val() && result == hkpConvexShape::WELD_RESULT_REJECT_CONTACT_POINT)
        {
            return false;
        }
        if (result == hkpConvexShape::WELD_RESULT_ACCEPT_CONTACT_POINT_MODIFIED)
        {
            output.m_normalInWorld = weldedNormal;
        }
    }
    return true;
}

// 0x00DD0470: a closest point query with a fresh cache (welded against a triangle B)
void hkpGskBaseAgent::staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    const hkpConvexShape* shapeA = hkGskBaseAgent_shape(bodyA);
    const hkpConvexShape* shapeB = hkGskBaseAgent_shape(bodyB);
    alignas(16) hkTransformf aTb;
    aTb.setMulInverseMul(bodyA.getTransform(), bodyB.getTransform());
    hkpGskCache cache;
    hkGskBaseAgent_initCache(cache, shapeA, shapeB, aTb);
    hkpExtendedGskOut output;
    if (!staticGetClosestPoint(bodyA, bodyB, aTb, input, cache, output))
    {
        return;
    }
    hkpCdPoint event(bodyA, bodyB);
    event.m_contact.m_position.m_quad = transformPos(bodyB.getTransform(), output.m_pointBinB.m_quad);
    event.m_contact.m_separatingNormal.m_quad = setXYZ_W(output.m_normalInWorld.m_quad, splat(output.m_distance));
    event.m_unweldedNormal = output.m_unweldedNormalInWorld;
    collector.addCdPoint(event);
}

// 0x00DD0350: the cached hkpGsk::getClosestPoint
void hkpGskBaseAgent::getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    alignas(16) hkTransformf aTb;
    aTb.setMulInverseMul(bodyA.getTransform(), bodyB.getTransform());
    hkpGsk::GetClosesetPointInput gskInput;
    gskInput.m_aTb = &aTb;
    gskInput.m_transformA = &bodyA.getTransform();
    gskInput.m_shapeA = hkGskBaseAgent_shape(bodyA);
    gskInput.m_shapeB = hkGskBaseAgent_shape(bodyB);
    gskInput.m_collisionTolerance = input.m_tolerance;
    hkVector4f normal;
    hkVector4f point;
    if (hkpGsk::getClosestPoint(gskInput, m_cache, normal, point) == HK_SUCCESS)
    {
        hkpCdPoint event(bodyA, bodyB);
        event.m_contact.m_position = point;
        event.m_contact.m_separatingNormal = normal;
        event.m_unweldedNormal = normal;
        collector.addCdPoint(event);
    }
}

// 0x00DCFDD0: penetrating when the GSK stops at a penetration or the distance less the radii is negative; keeps the
// separating normal (zero for a penetration)
void hkpGskBaseAgent::getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    const hkpConvexShape* shapeA = hkGskBaseAgent_shape(bodyA);
    const hkpConvexShape* shapeB = hkGskBaseAgent_shape(bodyB);
    alignas(16) hkTransformf aTb;
    aTb.setMulInverseMul(bodyA.getTransform(), bodyB.getTransform());
    hkpGsk gsk;
    gsk.init(shapeA, shapeB, m_cache);
    gsk.m_doNotHandlePenetration = 1;
    hkVector4f normal;
    const hkpGskStatus status = gsk.getClosestFeature(shapeA, shapeB, aTb, normal);
    gsk.checkForChangesAndUpdateCache(m_cache);
    if (status == HK_GSK_OK)
    {
        const float dist = normal.m_quad.m128_f32[3] - shapeA->m_radius - shapeB->m_radius;
        m_separatingNormal.m_quad = rotate(bodyA.getTransform().getRotation(), normal.m_quad);
        m_separatingNormal.m_quad.m128_f32[3] = dist;
        if (0.0f > dist)
        {
            collector.addCdBodyPair(bodyA, bodyB);
        }
        return;
    }
    m_separatingNormal.m_quad = _mm_setzero_ps();
    collector.addCdBodyPair(bodyA, bodyB);
}

// 0x00DCFFD0: the same with a fresh cache
void hkpGskBaseAgent::staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    const hkpConvexShape* shapeA = hkGskBaseAgent_shape(bodyA);
    const hkpConvexShape* shapeB = hkGskBaseAgent_shape(bodyB);
    alignas(16) hkTransformf aTb;
    aTb.setMulInverseMul(bodyA.getTransform(), bodyB.getTransform());
    hkpGskCache cache;
    hkGskBaseAgent_initCache(cache, shapeA, shapeB, aTb);
    hkpGsk gsk;
    gsk.init(shapeA, shapeB, cache);
    gsk.m_doNotHandlePenetration = 1;
    hkVector4f normal;
    if (gsk.getClosestFeature(shapeA, shapeB, aTb, normal) == HK_GSK_OK)
    {
        const float dist = normal.m_quad.m128_f32[3] - shapeA->m_radius - shapeB->m_radius;
        if (0.0f > dist)
        {
            collector.addCdBodyPair(bodyA, bodyB);
        }
        return;
    }
    collector.addCdBodyPair(bodyA, bodyB);
}
