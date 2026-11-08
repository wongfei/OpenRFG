// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpagentregisterutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): every collision agent registered with a dispatcher, from rfg.exe. The order
// matters: a later registration of a pair overrides an earlier one of the same specificity. Checks are off while
// agents that overlap on purpose register.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpagentregisterutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpbvcompressedmeshagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpbvtreeagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpmoppagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpstaticcompoundagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtreestream/hkpbvtreestreamagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtreestream/hkpmoppbvtreestreamagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/list/hkplistagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/shapecollection/hkpshapecollectionagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/boxbox/hkpboxboxagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/capsulecapsule/hkpcapsulecapsuleagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/capsuletriangle/hkpcapsuletriangleagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/gjk/hkppredgskfagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/spherebox/hkpsphereboxagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/spherecapsule/hkpspherecapsuleagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/spheresphere/hkpspheresphereagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/spheretriangle/hkpspheretriangleagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/deprecated/convexlist/hkpconvexlistagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/deprecated/multisphere/hkpmultisphereagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/deprecated/multispheretriangle/hkpmultispheretriangleagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/miscagent/bv/hkpbvagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/miscagent/phantom/hkpphantomagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/miscagent/transform/hkptransformagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/boxbox/hkpboxboxagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/bvtree3/hkpbvtreeagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/capsuletriangle/hkpcapsuletriangleagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/collection3/hkpcollectionagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/collectioncollection3/hkpcollectioncollectionagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/convexlist3/hkpconvexlistagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/predgskagent3/hkppredgskagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/predgskcylinderagent3/hkppredgskcylinderagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapetype.h"

// 0x00D9AF20
void hkpAgentRegisterUtil::registerAllAgents(hkpCollisionDispatcher* dis)
{
    hkpRegisterAlternateShapeTypes(dis);
    hkpBvAgent::registerAgent(dis);
    hkpMultiSphereAgent::registerAgent(dis);
    _registerBvTreeAgents(dis);
    dis->setEnableChecks(false);
    hkpShapeCollectionAgent::registerAgent(dis);
    dis->setEnableChecks(true);
    _registerListAgents(dis);
    hkpTransformAgent::registerAgent(dis);
    hkpPhantomAgent::registerAgent(dis);
    _registerTerminalAgents(dis);
}

// 0x00D9AE10
void hkpAgentRegisterUtil::_registerBvTreeAgents(hkpCollisionDispatcher* dis)
{
    dis->setEnableChecks(false);
    hkpBvTreeAgent::registerAgent(dis);
    hkpMoppAgent::registerAgent(dis);
    hkpBvTreeStreamAgent::registerAgent(dis);
    hkpMoppBvTreeStreamAgent::registerAgent(dis);
    hkpBvCompressedMeshAgent::registerAgent(dis);
    hkpStaticCompoundAgent::registerAgent(dis);
    hkBvTreeAgent3::registerAgent3(dis);
    dis->setEnableChecks(true);
}

// 0x00D9AE60: the heightfield agent is an optional component (registered through a function pointer)
void hkpAgentRegisterUtil::_registerListAgents(hkpCollisionDispatcher* dis)
{
    hkpListAgent::registerAgent(dis);
    dis->setEnableChecks(false);
    hkpCollectionAgent3::registerAgent3(dis);
    hkpCollectionCollectionAgent3::registerAgent3(dis);
    dis->setEnableChecks(true);
    hkpConvexListAgent::registerAgent(dis);
    hkConvexListAgent3::registerAgent3(dis);
    if (hkpCollisionAgent::registerHeightFieldAgent)
    {
        hkpCollisionAgent::registerHeightFieldAgent(dis);
    }
}

// 0x00D9AEC0: the convex agents (GSK for every convex pair, then the specialised ones)
void hkpAgentRegisterUtil::_registerTerminalAgents(hkpCollisionDispatcher* dis)
{
    hkpPredGskfAgent::registerAgent(dis);
    hkPredGskAgent3::registerAgent3(dis, hkcdShapeType::CONVEX, hkcdShapeType::CONVEX);
    hkPredGskCylinderAgent3::registerAgent3(dis);
    hkpBoxBoxAgent::registerAgent(dis);
    hkBoxBoxAgent3::registerAgent3(dis);
    hkpSphereSphereAgent::registerAgent(dis);
    hkpSphereCapsuleAgent::registerAgent(dis);
    hkpSphereTriangleAgent::registerAgent2(dis);
    hkpSphereBoxAgent::registerAgent(dis);
    hkpCapsuleCapsuleAgent::registerAgent(dis);
    hkpCapsuleTriangleAgent::registerAgent(dis);
    hkCapsuleTriangleAgent3::registerAgent3(dis);
    hkpMultiSphereTriangleAgent::registerAgent(dis);
}
