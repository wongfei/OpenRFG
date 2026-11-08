// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/gjk/hkppredgskfagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the predictive (continuous) convex-convex agent2, from rfg.exe. The separating
// normal is moved by the step's worst case movement (TIM): a pair moving apart beyond the keep distance drops its
// manifold, a pair that may penetrate deeper than the minimum separation gets a time of impact (continuous physics),
// and a pair still beyond the manifold TIM distance only verifies its manifold. No SDK source exists for 2014.2.5;
// ported from the binary.
//
// The shipped build records the "Gsk" monitor timer (TLS hkMonitorStream); it is not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/gjk/hkppredgskfagent.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/swepttransform/hkswepttransformutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionqualityinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/hkpagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/hkpconvexshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/continuous/hkpcontinuousgsk.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/gskmanifold/hkpgskmanifoldutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgsk.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

// 0x00DC5820
void hkpPredGskfAgent::registerAgent(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::AgentFuncs af;
    af.m_createFunc = createPredGskfAgent;
    af.m_getPenetrationsFunc = staticGetPenetrations;
    af.m_getClosestPointFunc = staticGetClosestPoints;
    af.m_linearCastFunc = staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::CONVEX, hkcdShapeType::CONVEX);
}

// 0x00DB7100 (inputIn unused)
hkpPredGskfAgent::hkpPredGskfAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& inputIn, hkpContactMgr* mgr)
    : hkpGskfAgent(bodyA, bodyB, mgr)
{
}

// 0x00DB7170
hkpPredGskfAgent::~hkpPredGskfAgent()
{
}

// 0x00DC57A0: without a contact manager the base agent
hkpCollisionAgent* hkpPredGskfAgent::createPredGskfAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    if (mgr != nullptr)
    {
        return new hkpPredGskfAgent(bodyA, bodyB, input, mgr);
    }
    return new hkpGskBaseAgent(bodyA, bodyB, nullptr);
}

// 0x00DC5860
void hkpPredGskfAgent::processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output)
{
    int needNewPoint = 0;
    const hkpCollisionQualityInfo* quality = input.m_collisionQualityInfo;
    const hkMotionState& msA = *static_cast<const hkMotionState*>(bodyA.m_motion);
    const hkMotionState& msB = *static_cast<const hkMotionState*>(bodyB.m_motion);
    if (!(m_timeOfSeparatingNormal == input.m_stepInfo.m_startTime.val()))
    {
        if (!quality->m_useContinuousPhysics.val())
        {
            m_timeOfSeparatingNormal = input.m_stepInfo.m_endTime;
            processCollisionNoTim(bodyA, bodyB, input, needNewPoint, output);
            return;
        }
        // the separating normal at the step start
        alignas(16) hkTransformf transformA;
        alignas(16) hkTransformf transformB;
        hkSweptTransformUtil::lerp2(msA.m_sweptTransform, input.m_stepInfo.m_startTime, transformA);
        hkSweptTransformUtil::lerp2(msB.m_sweptTransform, input.m_stepInfo.m_startTime, transformB);
        hkpCdBody startA;
        startA.m_shape = bodyA.m_shape;
        startA.m_shapeKey = bodyA.m_shapeKey;
        startA.m_motion = &transformA;
        startA.m_parent = &bodyA;
        hkpCdBody startB;
        startB.m_shape = bodyB.m_shape;
        startB.m_shapeKey = bodyB.m_shapeKey;
        startB.m_motion = &transformB;
        startB.m_parent = &bodyB;
        hkpGsk gsk;
        gsk.init(static_cast<const hkpConvexShape*>(bodyA.getShape()), static_cast<const hkpConvexShape*>(bodyB.getShape()), m_cache);
        calcSeparatingNormal(startA, startB, quality->m_keepContact, gsk, m_separatingNormal);
        gsk.checkForChangesAndUpdateCache(m_cache);
    }
    m_timeOfSeparatingNormal = input.m_stepInfo.m_endTime;

    // the step's worst case movement along the separating normal
    const float deltaTime = input.m_stepInfo.m_deltaTime;
    const Q dt = splat(deltaTime);
    const Q scaleB = mul(bcast<3>(msB.m_sweptTransform.m_centerOfMass1.m_quad), dt);
    const Q scaleA = mul(bcast<3>(msA.m_sweptTransform.m_centerOfMass1.m_quad), dt);
    const Q linear = add(mul(sub(msB.m_sweptTransform.m_centerOfMass1.m_quad, msB.m_sweptTransform.m_centerOfMass0.m_quad), scaleB), mul(sub(msA.m_sweptTransform.m_centerOfMass0.m_quad, msA.m_sweptTransform.m_centerOfMass1.m_quad), scaleA));
    const Q angular = add(mul(mul(bcast<3>(msA.m_deltaAngle.m_quad), scaleA), splat(msA.m_objectRadius)), mul(mul(bcast<3>(msB.m_deltaAngle.m_quad), scaleB), splat(msB.m_objectRadius)));
    const Q timInfo = setXYZ_W(linear, angular);
    const Q sep = m_separatingNormal.m_quad;
    const Q dist0 = bcast<3>(sep);
    const Q dist = sub(sub(dist0, dot3(sep, timInfo)), bcast<3>(timInfo));
    const Q allowed = splat(m_allowedPenetration);

    if (greater(dist, splat(quality->m_keepContact)) && greater(dist, mul(allowed, g_vectorfConstants[HK_QUADREAL_INV_2])))
    {
        // apart: the manifold goes
        m_separatingNormal.m_quad = setXYZ_W(sep, dist);
        if (m_manifold.m_numContactPoints)
        {
            hkGskManifold_cleanup(m_manifold, m_contactMgr, *output.m_constraintOwner);
        }
        return;
    }

    if (quality->m_useContinuousPhysics.val())
    {
        const Q minSeparation = _mm_min_ps(mul(splat(quality->m_minSeparation), allowed), add(mul(splat(quality->m_minExtraSeparation), allowed), dist0));
        if (lessOrNaN(dist, minSeparation))
        {
            hkpAgent3ProcessInput in3;
            in3.m_bodyA = &bodyA;
            in3.m_bodyB = &bodyB;
            in3.m_input = &input;
            in3.m_contactMgr = m_contactMgr;
            in3.m_distAtT1 = lane0(dist);
            in3.m_linearTimInfo.m_quad = timInfo;
            // Original behaviour kept: in3.m_aTb is left uninitialized (the TOI search computes its own transforms).
            const Q toiSeparation = _mm_min_ps(mul(splat(quality->m_toiSeparation), allowed), add(mul(splat(quality->m_toiExtraSeparation), allowed), dist0));
            hkSimdFloat32 allowedSr;
            allowedSr.m_real = allowed;
            hkSimdFloat32 minSeparationSr;
            minSeparationSr.m_real = minSeparation;
            hkSimdFloat32 toiSeparationSr;
            toiSeparationSr.m_real = toiSeparation;
            hk4dGskCollideCalcToi(in3, allowedSr, minSeparationSr, toiSeparationSr, m_cache, m_separatingNormal, output);
            processCollisionNoTim(bodyA, bodyB, input, needNewPoint, output);
            return;
        }
    }

    if (!greater(dist, splat(quality->m_manifoldTimDistance)))
    {
        processCollisionNoTim(bodyA, bodyB, input, needNewPoint, output);
        return;
    }

    // still apart enough: only the manifold's points
    m_separatingNormal.m_quad = setXYZ_W(sep, dist);
    hkpGskManifoldWork work;
    hkGskManifold_init(m_manifold, m_separatingNormal, m_separatingNormal, bodyA, bodyB, input.m_tolerance, work);
    needNewPoint = hkGskManifold_verifyAndGetPoints(m_manifold, work, 0, output, m_contactMgr);
    if (needNewPoint && (m_cache.m_gskFlags & 0x80))
    {
        // the manifold's points are taken back and the GSK runs
        output.m_firstFreeContactPoint = output.m_firstFreeContactPoint.val() - m_manifold.m_numContactPoints;
        processCollisionNoTim(bodyA, bodyB, input, needNewPoint, output);
    }
}
