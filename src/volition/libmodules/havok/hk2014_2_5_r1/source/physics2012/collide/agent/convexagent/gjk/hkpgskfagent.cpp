// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/gjk/hkpgskfagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the convex-convex agent2 with a GSK feature manifold, from rfg.exe. No SDK source
// exists for 2014.2.5; ported from the binary.
//
// The shipped build records the "GskAgent"/"Tim"/"Gsk" monitor timers (TLS hkMonitorStream); they are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/gjk/hkpgskfagent.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/hkpagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/agent/hkpgskagentutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/gskmanifold/hkpgskmanifoldutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

// 0x00DD0F50
void hkpGskfAgent::initAgentFunc(hkpCollisionDispatcher::AgentFuncs& af)
{
    af.m_createFunc = createGskfAgent;
    af.m_getPenetrationsFunc = staticGetPenetrations;
    af.m_getClosestPointFunc = staticGetClosestPoints;
    af.m_linearCastFunc = staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = false;
}

// 0x00DD0F80
void hkpGskfAgent::registerAgent(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::AgentFuncs af;
    initAgentFunc(af);
    dispatcher->registerCollisionAgent(af, hkcdShapeType::CONVEX, hkcdShapeType::CONVEX);
}

// 0x00DD0EC0
hkpGskfAgent::hkpGskfAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, hkpContactMgr* mgr)
    : hkpGskBaseAgent(bodyA, bodyB, mgr)
{
    m_manifold.init();
}

// 0x00DB70D0
hkpGskfAgent::~hkpGskfAgent()
{
}

// 0x00DD0EF0: without a contact manager (a query-only pair) the base agent
hkpCollisionAgent* hkpGskfAgent::createGskfAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    if (mgr != nullptr)
    {
        return new hkpGskfAgent(bodyA, bodyB, mgr);
    }
    return new hkpGskBaseAgent(bodyA, bodyB, nullptr);
}

// 0x00DD0DE0
void hkpGskfAgent::cleanup(hkpConstraintOwner& constraintOwner)
{
    hkGskManifold_cleanup(m_manifold, m_contactMgr, constraintOwner);
    delete this;
}

// 0x00DD0E10
void hkpGskfAgent::removePoint(unsigned short idToRemove)
{
    for (int i = 0; i < m_manifold.m_numContactPoints; i++)
    {
        if (m_manifold.m_contactPoints[i].m_id == idToRemove)
        {
            hkGskManifold_removePoint(m_manifold, i);
            return;
        }
    }
}

// 0x00DD0E50: the first point without an id gets it
void hkpGskfAgent::commitPotential(unsigned short idToCommit)
{
    for (int i = 0; i < m_manifold.m_numContactPoints; i++)
    {
        if (m_manifold.m_contactPoints[i].m_id == 0xFFFF)
        {
            m_manifold.m_contactPoints[i].m_id = idToCommit;
            return;
        }
    }
}

// 0x00DD0E90: the point's feature is cleared (dims 0), so the next verification drops it
void hkpGskfAgent::createZombie(unsigned short idTobecomeZombie)
{
    for (int i = 0; i < m_manifold.m_numContactPoints; i++)
    {
        if (m_manifold.m_contactPoints[i].m_id == idTobecomeZombie)
        {
            m_manifold.m_contactPoints[i].m_dimA = 0;
            m_manifold.m_contactPoints[i].m_dimB = 0;
            return;
        }
    }
}

// 0x00DD0FB0
void hkpGskfAgent::processCollisionNoTim(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, int explicitlyAllowNewPoint, hkpProcessCollisionOutput& output)
{
    hkpAgent3Input in3;
    in3.m_bodyA = &bodyA;
    in3.m_bodyB = &bodyB;
    in3.m_input = &input;
    in3.m_contactMgr = m_contactMgr;
    in3.m_aTb.setMulInverseMul(bodyA.getTransform(), bodyB.getTransform());
    // Original behaviour kept: the agent entry and agent data are two uninitialized stack slots (agent2 has none).
    hkpAgentEntry entryStandIn;
    unsigned char agentDataStandIn[16];
    hkGskAgentUtil_processCollisionNoTim(in3, &entryStandIn, agentDataStandIn, m_cache, m_manifold, m_separatingNormal, explicitlyAllowNewPoint, output);
}

// The time of impact info of the step: the relative linear movement (xyz) and the worst case angular movement (w)
// from the bodies' swept transforms.
static Q hkGskfAgent_calcTimInfo(const hkMotionState& msA, const hkMotionState& msB, float deltaTime)
{
    const Q dt = splat(deltaTime);
    const hkSweptTransformf& sA = msA.m_sweptTransform;
    const hkSweptTransformf& sB = msB.m_sweptTransform;
    const Q scaleA = mul(bcast<3>(sA.m_centerOfMass1.m_quad), dt);
    const Q scaleB = mul(bcast<3>(sB.m_centerOfMass1.m_quad), dt);
    const Q linear = add(mul(sub(sA.m_centerOfMass0.m_quad, sA.m_centerOfMass1.m_quad), scaleA), mul(sub(sB.m_centerOfMass1.m_quad, sB.m_centerOfMass0.m_quad), scaleB));
    const Q angular = add(mul(mul(bcast<3>(msA.m_deltaAngle.m_quad), scaleA), splat(msA.m_objectRadius)), mul(mul(bcast<3>(msB.m_deltaAngle.m_quad), scaleB), splat(msB.m_objectRadius)));
    return setXYZ_W(linear, angular);
}

// 0x00DD1030
// The separating normal of a step not processed last is moved by the step's worst case movement; a pair still beyond
// the tolerance is skipped. Original behaviour kept: a normal of the step start goes straight to the GSK.
void hkpGskfAgent::processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result)
{
    if (!(m_timeOfSeparatingNormal == input.m_stepInfo.m_startTime.val()))
    {
        const Q timInfo = hkGskfAgent_calcTimInfo(*static_cast<const hkMotionState*>(bodyA.m_motion), *static_cast<const hkMotionState*>(bodyB.m_motion), input.m_stepInfo.m_deltaTime);
        const Q tolerance = splat(input.m_tolerance);
        const Q sep = m_separatingNormal.m_quad;
        if (greater(bcast<3>(sep), tolerance))
        {
            // n.linear + angular, the four lanes summed as (w + y) + (z + x)
            const Q t = setXYZ_W(mul(sep, timInfo), timInfo);
            const Q pairs = _mm_add_ps(_mm_shuffle_ps(t, t, 0x4E), t);
            const Q sum = _mm_add_ps(_mm_shuffle_ps(pairs, pairs, 0xB1), pairs);
            const Q newDist = sub(bcast<3>(sep), sum);
            m_separatingNormal.m_quad = setXYZ_W(sep, newDist);
            if (greater(newDist, tolerance))
            {
                return;
            }
        }
    }
    m_timeOfSeparatingNormal = input.m_stepInfo.m_endTime;
    processCollisionNoTim(bodyA, bodyB, input, 0, result);
}
