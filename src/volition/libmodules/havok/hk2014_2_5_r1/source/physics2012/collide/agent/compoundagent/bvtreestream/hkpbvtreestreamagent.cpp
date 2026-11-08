// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtreestream/hkpbvtreestreamagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the registration of the BV tree stream agent, from rfg.exe. Registered after
// hkpBvTreeAgent, it owns the BV tree (and sampled heightfield BV tree) vs CONVEX entries; their statics are the BV tree
// agent's.
//
// Deviation: the stream agent itself (an agent2 over a 1n agent track) is not ported: its create functions make the BV
// tree agent2 instead, which handles the same pairs with agent2 children. It is live in rfg.exe, not tied to the
// multithreaded paths: the character controller's capsule phantom reaches it against MOPP and heightfield shapes
// (0x00DB2D10). Its queries (vtable 0x013599CC) only forward to the hkpBvTreeAgent statics, as here. No reachable caller
// of its differing processCollision (0x00DB2D50) was found, as the world's top-level pairs use the agent3s; inferred from
// the tables, not run.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtreestream/hkpbvtreestreamagent.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpbvtreeagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagentlinearcast.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"

// 0x00DB3560: BV trees vs convex shapes, both ways
void hkpBvTreeStreamAgent::registerAgent(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::AgentFuncs af;
    af.m_createFunc = createBvTreeShapeAgent;
    af.m_getPenetrationsFunc = hkpSymmetricAgentLinearCast<hkpBvTreeAgent>::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpSymmetricAgentLinearCast<hkpBvTreeAgent>::staticGetClosestPoints;
    af.m_linearCastFunc = hkpSymmetricAgentLinearCast<hkpBvTreeAgent>::staticLinearCast;
    af.m_isFlipped = true;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::BV_TREE, hkcdShapeType::CONVEX);
    dispatcher->registerCollisionAgent(af, hkcdShapeType::TRI_SAMPLED_HEIGHT_FIELD_BV_TREE, hkcdShapeType::CONVEX);

    af.m_createFunc = createShapeBvAgent;
    af.m_getPenetrationsFunc = hkpBvTreeAgent::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpBvTreeAgent::staticGetClosestPoints;
    af.m_linearCastFunc = hkpBvTreeAgent::staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::CONVEX, hkcdShapeType::BV_TREE);
    dispatcher->registerCollisionAgent(af, hkcdShapeType::CONVEX, hkcdShapeType::TRI_SAMPLED_HEIGHT_FIELD_BV_TREE);
}

// 0x00DB2D10 (stand-in, see the file comment)
hkpCollisionAgent* hkpBvTreeStreamAgent::createShapeBvAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return hkpBvTreeAgent::createShapeBvAgent(bodyA, bodyB, input, mgr);
}

// 0x00DB3510 (stand-in, see the file comment)
hkpCollisionAgent* hkpBvTreeStreamAgent::createBvTreeShapeAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return hkpBvTreeAgent::createBvTreeShapeAgent(bodyA, bodyB, input, mgr);
}
