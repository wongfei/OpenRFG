// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtreestream/hkpmoppbvtreestreamagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the registration of the MOPP stream agent, from rfg.exe. Registered after
// hkpMoppAgent, it owns the MOPP vs CONVEX entries (the stream agent's create functions, see hkpbvtreestreamagent.cpp)
// and MOPP vs MOPP.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtreestream/hkpmoppbvtreestreamagent.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpbvtreeagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpmoppagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtreestream/hkpbvtreestreamagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagentlinearcast.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"

// 0x00DB2A80
void hkpMoppBvTreeStreamAgent::registerAgent(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::AgentFuncs af;
    af.m_createFunc = hkpBvTreeStreamAgent::createBvTreeShapeAgent;
    af.m_getPenetrationsFunc = hkpSymmetricAgentLinearCast<hkpMoppAgent>::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpSymmetricAgentLinearCast<hkpMoppAgent>::staticGetClosestPoints;
    af.m_linearCastFunc = hkpSymmetricAgentLinearCast<hkpMoppAgent>::staticLinearCast;
    af.m_isFlipped = true;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::MOPP, hkcdShapeType::CONVEX);

    af.m_createFunc = hkpBvTreeStreamAgent::createShapeBvAgent;
    af.m_getPenetrationsFunc = hkpBvTreeAgent::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpBvTreeAgent::staticGetClosestPoints;
    af.m_linearCastFunc = hkpMoppAgent::staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::CONVEX, hkcdShapeType::MOPP);

    af.m_createFunc = hkpMoppAgent::createBvBvAgent;
    af.m_getPenetrationsFunc = hkpBvTreeAgent::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpBvTreeAgent::staticGetClosestPoints;
    af.m_linearCastFunc = hkpMoppAgent::staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::MOPP, hkcdShapeType::MOPP);
}
