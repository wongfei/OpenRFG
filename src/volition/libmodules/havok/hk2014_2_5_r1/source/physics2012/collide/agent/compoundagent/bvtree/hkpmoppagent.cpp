// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpmoppagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the MOPP agent, from rfg.exe: the BV tree agent with the MOPP AABB cast virtual
// machine for linear casts, and MOPP vs MOPP pairs queried with the smaller code. No SDK source exists for 2014.2.5;
// ported from the binary.
//
// The shipped build records the "TtMopp" monitor timers (TLS hkMonitorStream); they are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpmoppagent.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkplinearcastcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/tree/mopp/hkpmoppbvtreeshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/code/hkpmoppcode.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/machine/hkpmoppaabbcastvirtualmachine.h"

using namespace hkGskExact;

// 0x00DB45F0 (emitted with the agent)
hkpMoppAabbCastVirtualMachine::hkpMoppAabbCastVirtualMachine()
{
}

// 0x00DB4600
hkpMoppAabbCastVirtualMachine::~hkpMoppAabbCastVirtualMachine()
{
}

// 0x00DB4890: MOPP vs any (symmetric), any vs MOPP, MOPP vs MOPP
void hkpMoppAgent::registerAgent(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::AgentFuncs af;
    af.m_createFunc = hkpBvTreeAgent::createBvTreeShapeAgent;
    af.m_getPenetrationsFunc = hkpSymmetricAgentLinearCast<hkpMoppAgent>::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpSymmetricAgentLinearCast<hkpMoppAgent>::staticGetClosestPoints;
    af.m_linearCastFunc = hkpSymmetricAgentLinearCast<hkpMoppAgent>::staticLinearCast;
    af.m_isFlipped = true;
    af.m_isPredictive = false;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::MOPP, hkcdShapeType::ALL_SHAPE_TYPES);

    af.m_createFunc = hkpBvTreeAgent::createShapeBvAgent;
    af.m_getPenetrationsFunc = hkpBvTreeAgent::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpBvTreeAgent::staticGetClosestPoints;
    af.m_linearCastFunc = staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = false;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::ALL_SHAPE_TYPES, hkcdShapeType::MOPP);

    af.m_createFunc = createBvBvAgent;
    af.m_getPenetrationsFunc = hkpBvTreeAgent::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpBvTreeAgent::staticGetClosestPoints;
    af.m_linearCastFunc = staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::MOPP, hkcdShapeType::MOPP);
}

// 0x00DB47F0
hkpMoppAgent::hkpMoppAgent(hkpContactMgr* mgr)
    : hkpBvTreeAgent(mgr)
{
}

// 0x00DB4990
hkpMoppAgent::~hkpMoppAgent()
{
}

// 0x00DB4810: the MOPP with the smaller code is the one queried against the other's
hkpCollisionAgent* hkpMoppAgent::createBvBvAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    const hkpMoppBvTreeShape* moppA = static_cast<const hkpMoppBvTreeShape*>(bodyA.getShape());
    const hkpMoppBvTreeShape* moppB = static_cast<const hkpMoppBvTreeShape*>(bodyB.getShape());
    if (moppA->m_code->m_data.getSize() < moppB->m_code->m_data.getSize())
    {
        return new hkpMoppAgent(mgr);
    }
    return new hkpSymmetricAgent<hkpMoppAgent>(mgr);
}

// 0x00DB4630: A's AABB (in B's space) cast along the path through the MOPP
void hkpMoppAgent::staticLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
{
    alignas(16) hkTransformf bTa;
    bTa.setMulInverseMul(bodyB.getTransform(), bodyA.getTransform());
    hkAabb aabb;
    bodyA.getShape()->getAabb(bTa, input.m_tolerance, aabb);
    const Q halfExtents = mul(sub(aabb.m_max.m_quad, aabb.m_min.m_quad), g_vectorfConstants[HK_QUADREAL_INV_2]);
    const Q path = rotateInverse(bodyB.getTransform().getRotation(), input.m_path.m_quad);
    const Q tolerance = _mm_castsi128_ps(_mm_srli_si128(_mm_slli_si128(_mm_castps_si128(splat(input.m_tolerance)), 4), 4));
    hkpMoppAabbCastVirtualMachine::hkpAabbCastInput castInput;
    castInput.m_from.m_quad = add(halfExtents, aabb.m_min.m_quad);
    castInput.m_to.m_quad = add(path, castInput.m_from.m_quad);
    castInput.m_extents.m_quad = add(tolerance, halfExtents);
    castInput.m_collisionInput = &input;
    castInput.m_castBody = &bodyA;
    castInput.m_moppBody = &bodyB;
    hkpMoppAabbCastVirtualMachine machine;
    machine.aabbCast(castInput, collector, startCollector);
}
