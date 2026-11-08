// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpstaticcompoundagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6, stub_impl.md S4): hkpStaticCompoundAgent, from rfg.exe: the bv-tree agent with a
// linear cast that sweeps shape A through the compound's tree (hkpStaticCompoundShape::castAabb). Every registration
// creates the plain agent (the dispatcher swaps the bodies of the flipped one). No RFG data has a static compound shape
// (hkpStaticCompoundShape is not ported), so nothing creates this agent.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpstaticcompoundagent.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpbvtreeagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagentlinearcast.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/staticcompound/hkpstaticcompoundshape.h"

using namespace hkGskExact;

// 0x00DB1C70
void hkpStaticCompoundAgent::registerAgent(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::AgentFuncs af;
    af.m_createFunc = createStaticCompoundVsShapeAgent;
    af.m_getPenetrationsFunc = hkpSymmetricAgentLinearCast<hkpStaticCompoundAgent>::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpSymmetricAgentLinearCast<hkpStaticCompoundAgent>::staticGetClosestPoints;
    af.m_linearCastFunc = hkpSymmetricAgentLinearCast<hkpStaticCompoundAgent>::staticLinearCast;
    af.m_isFlipped = true;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::STATIC_COMPOUND, hkcdShapeType::ALL_SHAPE_TYPES);

    af.m_createFunc = createShapeVsStaticCompoundAgent;
    af.m_getPenetrationsFunc = hkpBvTreeAgent::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpBvTreeAgent::staticGetClosestPoints;
    af.m_linearCastFunc = staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::ALL_SHAPE_TYPES, hkcdShapeType::STATIC_COMPOUND);

    af.m_createFunc = createStaticCompoundVsStaticCompoundAgent;
    af.m_getPenetrationsFunc = hkpBvTreeAgent::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpBvTreeAgent::staticGetClosestPoints;
    af.m_linearCastFunc = staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::STATIC_COMPOUND, hkcdShapeType::STATIC_COMPOUND);
}

// 0x00DB1AB0: A in B's space (inverse(B) * A, in two steps), cast to its translation plus the path (in B's space)
void hkpStaticCompoundAgent::staticLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
{
    alignas(16) hkTransformf bInv;
    bInv.setInverse(bodyB.getTransform());
    alignas(16) hkTransformf bTa;
    bTa.setMul(bInv, bodyA.getTransform());
    alignas(16) hkVector4f to;
    to.m_quad = transformPos(bInv, add(bodyA.getTransform().getTranslation().m_quad, input.m_path.m_quad));
    hkpBvTreeAgent::LinearCastAabbCastCollector castCollector(bodyA, bodyB, input, collector, startCollector);
    static_cast<const hkpStaticCompoundShape*>(bodyB.getShape())->castAabb(bodyA.getShape(), bTa, to, castCollector, input.m_tolerance);
}

// 0x00DB1B90
void hkpStaticCompoundAgent::linearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
{
    staticLinearCast(bodyA, bodyB, input, collector, startCollector);
}

// 0x00DB1BC0
hkpStaticCompoundAgent::hkpStaticCompoundAgent(hkpContactMgr* mgr)
    : hkpBvTreeAgent(mgr)
{
}

// 0x00DB1E40
hkpStaticCompoundAgent::~hkpStaticCompoundAgent()
{
}

// 0x00DB1BE0
hkpCollisionAgent* hkpStaticCompoundAgent::createShapeVsStaticCompoundAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpStaticCompoundAgent(mgr);
}

// 0x00DB1C10
hkpCollisionAgent* hkpStaticCompoundAgent::createStaticCompoundVsShapeAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpStaticCompoundAgent(mgr);
}

// 0x00DB1C40
hkpCollisionAgent* hkpStaticCompoundAgent::createStaticCompoundVsStaticCompoundAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpStaticCompoundAgent(mgr);
}
