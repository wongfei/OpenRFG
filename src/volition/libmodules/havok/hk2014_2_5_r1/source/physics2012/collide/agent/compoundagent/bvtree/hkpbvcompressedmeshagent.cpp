// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpbvcompressedmeshagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6, stub_impl.md S4): hkpBvCompressedMeshAgent, from rfg.exe: the bv-tree agent with
// a linear cast that sweeps A's AABB through the mesh's tree (castAabb) instead of querying one swept AABB. The flipped
// registration creates the plain agent too (the dispatcher swaps the bodies). No RFG data has a compressed mesh
// (hkpBvCompressedMeshShape is not ported), so nothing creates this agent.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpbvcompressedmeshagent.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpbvtreeagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagentlinearcast.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/hkpshapecollection.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/tree/hkpbvtreeshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/bvcompressedmesh/hkpbvcompressedmeshshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

// 0x00DB2280
void hkpBvCompressedMeshAgent::registerAgent(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::AgentFuncs af;
    af.m_createFunc = createStaticMeshVsShapeAgent;
    af.m_getPenetrationsFunc = hkpSymmetricAgentLinearCast<hkpBvCompressedMeshAgent>::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpSymmetricAgentLinearCast<hkpBvCompressedMeshAgent>::staticGetClosestPoints;
    af.m_linearCastFunc = hkpSymmetricAgentLinearCast<hkpBvCompressedMeshAgent>::staticLinearCast;
    af.m_isFlipped = true;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::BV_COMPRESSED_MESH, hkcdShapeType::ALL_SHAPE_TYPES);

    af.m_createFunc = createShapeVsStaticMeshAgent;
    af.m_getPenetrationsFunc = hkpBvTreeAgent::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpBvTreeAgent::staticGetClosestPoints;
    af.m_linearCastFunc = staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::ALL_SHAPE_TYPES, hkcdShapeType::BV_COMPRESSED_MESH);

    af.m_createFunc = createStaticMeshVsStaticMeshAgent;
    af.m_getPenetrationsFunc = hkpBvTreeAgent::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpBvTreeAgent::staticGetClosestPoints;
    af.m_linearCastFunc = staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::BV_COMPRESSED_MESH, hkcdShapeType::BV_COMPRESSED_MESH);
}

// 0x00DB1FD0: A's AABB in B's space, cast from its center along the path (in B's space) through B's tree
void hkpBvCompressedMeshAgent::staticLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& castCollector, hkpCdPointCollector* startCollector)
{
    alignas(16) hkTransformf bTa;
    bTa.setMulInverseMul(bodyB.getTransform(), bodyA.getTransform());
    hkAabb aabb;
    bodyA.getShape()->getAabb(bTa, input.m_tolerance, aabb);
    const Q path = rotateInverse(bodyB.getTransform().getRotation(), input.m_path.m_quad);
    const Q center = mul(add(aabb.m_max.m_quad, aabb.m_min.m_quad), g_vectorfConstants[HK_QUADREAL_INV_2]);
    alignas(16) hkVector4f to;
    to.m_quad = add(path, center);
    hkpBvTreeAgent::LinearCastAabbCastCollector collector(bodyA, bodyB, input, castCollector, startCollector);
    static_cast<const hkpBvTreeShape*>(bodyB.getShape())->castAabbImpl(aabb, to, collector);
}

// 0x00DB2130
void hkpBvCompressedMeshAgent::linearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
{
    staticLinearCast(bodyA, bodyB, input, collector, startCollector);
}

// 0x00DB2160
hkpBvCompressedMeshAgent::hkpBvCompressedMeshAgent(hkpContactMgr* mgr)
    : hkpBvTreeAgent(mgr)
{
}

// 0x00DB2450
hkpBvCompressedMeshAgent::~hkpBvCompressedMeshAgent()
{
}

// 0x00DB2180
hkpCollisionAgent* hkpBvCompressedMeshAgent::createShapeVsStaticMeshAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpBvCompressedMeshAgent(mgr);
}

// 0x00DB21B0
hkpCollisionAgent* hkpBvCompressedMeshAgent::createStaticMeshVsShapeAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpBvCompressedMeshAgent(mgr);
}

// 0x00DB21E0: the mesh with fewer children is queried against the other's tree
hkpCollisionAgent* hkpBvCompressedMeshAgent::createStaticMeshVsStaticMeshAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    // the meshes' container bases (+0x14), not the virtual getContainer()
    const hkpShapeContainer* containerA = static_cast<const hkpBvCompressedMeshShape*>(bodyA.getShape());
    const hkpShapeContainer* containerB = static_cast<const hkpBvCompressedMeshShape*>(bodyB.getShape());
    if (containerA->getNumChildShapes() < containerB->getNumChildShapes())
    {
        return new hkpBvCompressedMeshAgent(mgr);
    }
    return new hkpSymmetricAgent<hkpBvCompressedMeshAgent>(mgr);
}
