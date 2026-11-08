// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/linearcast/hkpiterativelinearcastagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the linear cast of agents without a dedicated one, from rfg.exe. A closest
// point query with the tolerance grown by the path length finds the first contact; while it is farther than the
// configured early out distance, body A is moved along the path to the conservative time of impact and queried again
// (up to the configured number of iterations). No SDK source exists for 2014.2.5; ported from the binary.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/linearcast/hkpiterativelinearcastagent.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionagentconfig.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkplinearcastcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkpclosestcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

// 0x00DBA950
hkpIterativeLinearCastAgent::hkpIterativeLinearCastAgent(hkpContactMgr* contactMgr)
    : hkpCollisionAgent(contactMgr)
{
}

// 0x00DB70B0
hkpIterativeLinearCastAgent::~hkpIterativeLinearCastAgent()
{
}

// The collision input of the casts: the tolerance grown by the path length, contact points always accepted.
static void hkIterativeLinearCast_setupInput(const hkpLinearCastCollisionInput& input, hkpCollisionInput& castInput)
{
    castInput = input;
    castInput.m_tolerance = input.m_cachedPathLength + castInput.m_tolerance.val();
    castInput.m_forceAcceptContactPoints = 1;
}

// A cd point of the contact (position, normal with the distance or fraction in w; the unwelded normal the same).
static void hkIterativeLinearCast_addPoint(hkpCdPointCollector& collector, const hkpCdBody& bodyA, const hkpCdBody& bodyB, Q position, Q normal)
{
    hkpCdPoint event(bodyA, bodyB);
    event.m_contact.m_position.m_quad = position;
    event.m_contact.m_separatingNormal.m_quad = normal;
    event.m_unweldedNormal.m_quad = normal;
    collector.addCdPoint(event);
}

// 0x00DD12E0
void hkpIterativeLinearCastAgent::linearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
{
    const Q zero = _mm_setzero_ps();
    hkpClosestCdPointCollector closest;
    alignas(16) hkpCollisionInput castInput;
    hkIterativeLinearCast_setupInput(input, castInput);
    getClosestPoints(bodyA, bodyB, castInput, closest);
    if (!closest.hasHit())
    {
        return;
    }

    // the unwelded normal with the distance
    Q position = closest.m_hitPoint.m_contact.m_position.m_quad;
    const Q normal = setXYZ_W(closest.m_unweldedNormal.m_quad, bcast<3>(closest.m_hitPoint.m_contact.m_separatingNormal.m_quad));
    const Q dist = bcast<3>(normal);
    if (lessOrNaN(dist, splat(input.m_tolerance)) && startCollector != nullptr)
    {
        hkIterativeLinearCast_addPoint(*startCollector, bodyA, bodyB, position, normal);
    }
    const Q path = input.m_path.m_quad;
    const Q pathProjected = dot3(normal, path);
    const Q approach = add(pathProjected, dist);
    if (greater(approach, zero))
    {
        return; // moving apart
    }
    if (!lessOrNaN(add(splat(input.m_maxExtraPenetration), pathProjected), zero))
    {
        return;
    }
    const hkpCollisionAgentConfig* config = input.m_config;
    const Q fraction = mul(recip23(sub(dist, approach)), dist);
    if (!greater(dist, splat(config->m_iterativeLinearCastEarlyOutDistance)))
    {
        Q hitNormal;
        if (greater(dist, zero))
        {
            if (greater(fraction, splat(collector.m_earlyOutDistance)))
            {
                return;
            }
            hitNormal = setXYZ_W(normal, fraction);
        }
        else
        {
            hitNormal = setXYZ_W(normal, zero);
        }
        hkIterativeLinearCast_addPoint(collector, bodyA, bodyB, position, hitNormal);
        return;
    }

    // advance body A along the path to the fraction and query again
    Q hitNormal = setXYZ_W(normal, fraction);
    hkpClosestCdPointCollector next;
    alignas(16) hkTransformf movedTransform = bodyA.getTransform();
    hkpCdBody movedBodyA;
    movedBodyA.m_shape = bodyA.m_shape;
    movedBodyA.m_shapeKey = 0xFFFFFFFF;
    movedBodyA.m_motion = &movedTransform;
    movedBodyA.m_parent = &bodyA;
    for (int i = config->m_iterativeLinearCastMaxIterations - 1; i >= 0; i--)
    {
        next.reset();
        const Q f = bcast<3>(hitNormal);
        movedTransform.m_translation.m_quad = add(mul(f, path), bodyA.getTransform().m_translation.m_quad);
        getClosestPoints(movedBodyA, bodyB, castInput, next);
        // Original behaviour kept: the hit is not checked; without one the stale normals of the collector are used.
        const Q unwelded = next.m_unweldedNormal.m_quad;
        const Q projected = dot3(unwelded, input.m_path.m_quad);
        if (!lessOrNaN(projected, zero))
        {
            return;
        }
        const Q speed = sub(zero, projected);
        const Q d = bcast<3>(next.m_hitPoint.m_contact.m_separatingNormal.m_quad);
        if (greater(add(mul(speed, f), d), speed))
        {
            return;
        }
        const Q newFraction = add(mul(recip23(speed), d), f);
        if (greater(newFraction, splat(collector.m_earlyOutDistance)))
        {
            return;
        }
        position = next.m_hitPoint.m_contact.m_position.m_quad;
        hitNormal = setXYZ_W(unwelded, newFraction);
        if (!greater(d, splat(input.m_config->m_iterativeLinearCastEarlyOutDistance)))
        {
            break;
        }
    }
    if (castInput.m_weldClosestPoints.val())
    {
        // the welded normal of the last query (after no iteration: the reset collector's)
        hitNormal = setXYZ_W(next.m_hitPoint.m_contact.m_separatingNormal.m_quad, hitNormal);
    }
    hkIterativeLinearCast_addPoint(collector, bodyA, bodyB, position, hitNormal);
}

// 0x00DD1720
// The same through the dispatcher's closest point function for the shape types, with the welded normal of the first
// query, the hit checked each iteration and the fraction kept out of the direct contact's normal.
void hkpIterativeLinearCastAgent::staticLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
{
    const Q zero = _mm_setzero_ps();
    hkpClosestCdPointCollector closest;
    alignas(16) hkpCollisionInput castInput;
    hkIterativeLinearCast_setupInput(input, castInput);
    const hkpCollisionDispatcher::GetClosestPointsFunc getClosestPoints = input.m_dispatcher->getGetClosestPointsFunc((hkcdShapeType::ShapeTypeEnum)bodyA.getShape()->m_type.m_storage, (hkcdShapeType::ShapeTypeEnum)bodyB.getShape()->m_type.m_storage);
    getClosestPoints(bodyA, bodyB, castInput, closest);
    if (!closest.hasHit())
    {
        return;
    }

    Q position = closest.m_hitPoint.m_contact.m_position.m_quad;
    Q normal = closest.m_hitPoint.m_contact.m_separatingNormal.m_quad;
    if (lessOrNaN(bcast<3>(normal), splat(input.m_tolerance)) && startCollector != nullptr)
    {
        hkIterativeLinearCast_addPoint(*startCollector, bodyA, bodyB, position, normal);
    }
    const Q path = input.m_path.m_quad;
    const Q pathProjected = dot3(closest.m_unweldedNormal.m_quad, path);
    const Q dist = bcast<3>(normal);
    const Q approach = add(pathProjected, dist);
    if (greater(approach, zero))
    {
        return;
    }
    if (!lessOrNaN(add(splat(input.m_maxExtraPenetration), pathProjected), zero))
    {
        return;
    }
    const hkpCollisionAgentConfig* config = input.m_config;
    if (!greater(dist, splat(config->m_iterativeLinearCastEarlyOutDistance)))
    {
        if (greater(dist, zero) && greater(dist, mul(splat(collector.m_earlyOutDistance), sub(dist, approach))))
        {
            return;
        }
        hkIterativeLinearCast_addPoint(collector, bodyA, bodyB, position, setXYZ_W(normal, zero));
        return;
    }

    Q fraction = mul(recip23(sub(dist, approach)), dist);
    hkpClosestCdPointCollector next;
    alignas(16) hkTransformf movedTransform = bodyA.getTransform();
    hkpCdBody movedBodyA;
    movedBodyA.m_shape = bodyA.m_shape;
    movedBodyA.m_shapeKey = 0xFFFFFFFF;
    movedBodyA.m_motion = &movedTransform;
    movedBodyA.m_parent = &bodyA;
    for (int i = config->m_iterativeLinearCastMaxIterations - 1; i >= 0; i--)
    {
        next.reset();
        movedTransform.m_translation.m_quad = add(mul(fraction, path), bodyA.getTransform().m_translation.m_quad);
        getClosestPoints(movedBodyA, bodyB, castInput, next);
        if (!next.hasHit())
        {
            return;
        }
        position = next.m_hitPoint.m_contact.m_position.m_quad;
        const Q unwelded = next.m_unweldedNormal.m_quad;
        normal = setXYZ_W(unwelded, fraction);
        const Q projected = dot3(unwelded, input.m_path.m_quad);
        if (!lessOrNaN(projected, zero))
        {
            return;
        }
        const Q speed = sub(zero, projected);
        const Q d = bcast<3>(next.m_hitPoint.m_contact.m_separatingNormal.m_quad);
        if (greater(add(mul(bcast<3>(normal), speed), d), speed))
        {
            return;
        }
        if (!greater(d, splat(input.m_config->m_iterativeLinearCastEarlyOutDistance)))
        {
            break;
        }
        fraction = add(mul(recip23(speed), d), fraction);
        if (greater(fraction, splat(collector.m_earlyOutDistance)))
        {
            return;
        }
    }
    if (castInput.m_weldClosestPoints.val())
    {
        normal = setXYZ_W(next.m_hitPoint.m_contact.m_separatingNormal.m_quad, normal);
    }
    hkIterativeLinearCast_addPoint(collector, bodyA, bodyB, position, normal);
}
