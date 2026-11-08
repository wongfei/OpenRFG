// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpbvtreeagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the BV tree agent2 (a shape against the children of a bounding volume tree
// shape: MOPP, compressed mesh, heightfield collections), from rfg.exe. It queries the tree with A's AABB in B's
// space, grown by the step's movement and cached, keeps one child agent per key hit, and processes them. No SDK
// source exists for 2014.2.5; ported from the binary.
//
// The shipped build records the "LtBvTree"/"StQueryTree"/"StNarrowPhase" monitor timers (TLS hkMonitorStream); they
// are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpbvtreeagent.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hksort.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionqualityinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdbodypaircollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkplinearcastcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/null/hkpnullagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/tree/hkpbvtreeshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapebuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

#include <string.h>

using namespace hkGskExact;

hkBool hkpBvTreeAgent::m_useFastUpdate = false; // 0x03A274E9
hkBool hkpBvTreeAgent::m_useAabbCaching = true; // 0x016F5264

static inline const hkpBvTreeShape* hkBvTreeAgent_tree(const hkpCdBody& body)
{
    return static_cast<const hkpBvTreeShape*>(body.getShape());
}

static inline const hkMotionState& hkBvTreeAgent_motionState(const hkpCdBody& body)
{
    return *static_cast<const hkMotionState*>(body.m_motion);
}

static inline bool hkBvTreeAgent_isEnabled(const hkpCollisionInput& input, const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpShapeContainer& container, unsigned int key)
{
    const hkpShapeCollectionFilter* filter = input.m_filter.val();
    return filter->isCollisionEnabled(input, bodyA, bodyB, container, key).m_bool != 0;
}

// hkSweptTransformUtil::calcTimInfo (inlined): the relative linear movement of the step (xyz) and the worst case
// angular movement (w)
static inline Q hkBvTreeAgent_calcTimInfo(const hkMotionState& msA, const hkMotionState& msB, float deltaTime)
{
    const Q dt = splat(deltaTime);
    const hkSweptTransformf& sA = msA.m_sweptTransform;
    const hkSweptTransformf& sB = msB.m_sweptTransform;
    const Q scaleB = mul(bcast<3>(sB.m_centerOfMass1.m_quad), dt);
    const Q scaleA = mul(bcast<3>(sA.m_centerOfMass1.m_quad), dt);
    const Q linear = add(mul(sub(sA.m_centerOfMass0.m_quad, sA.m_centerOfMass1.m_quad), scaleA), mul(sub(sB.m_centerOfMass1.m_quad, sB.m_centerOfMass0.m_quad), scaleB));
    const Q angular = add(mul(mul(bcast<3>(msA.m_deltaAngle.m_quad), scaleA), splat(msA.m_objectRadius)), mul(mul(bcast<3>(msB.m_deltaAngle.m_quad), scaleB), splat(msB.m_objectRadius)));
    return setXYZ_W(linear, angular);
}

// 0x00D764A0
void hkpBvTreeAgent::setUseAabbCaching(hkBool useIt)
{
    m_useAabbCaching = useIt;
}

// 0x00D77FB0: shape vs BV tree (the BV tree and sampled heightfield BV tree types, 22 and 7), BV tree vs shape (symmetric), BV tree vs BV tree
void hkpBvTreeAgent::registerAgent(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::AgentFuncs af;
    af.m_createFunc = createBvTreeShapeAgent;
    af.m_getPenetrationsFunc = hkpSymmetricAgentLinearCast<hkpBvTreeAgent>::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpSymmetricAgentLinearCast<hkpBvTreeAgent>::staticGetClosestPoints;
    af.m_linearCastFunc = hkpSymmetricAgentLinearCast<hkpBvTreeAgent>::staticLinearCast;
    af.m_isFlipped = true;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::BV_TREE, hkcdShapeType::ALL_SHAPE_TYPES);
    dispatcher->registerCollisionAgent(af, hkcdShapeType::TRI_SAMPLED_HEIGHT_FIELD_BV_TREE, hkcdShapeType::ALL_SHAPE_TYPES);

    af.m_createFunc = createShapeBvAgent;
    af.m_getPenetrationsFunc = staticGetPenetrations;
    af.m_getClosestPointFunc = staticGetClosestPoints;
    af.m_linearCastFunc = staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::ALL_SHAPE_TYPES, hkcdShapeType::BV_TREE);
    dispatcher->registerCollisionAgent(af, hkcdShapeType::ALL_SHAPE_TYPES, hkcdShapeType::TRI_SAMPLED_HEIGHT_FIELD_BV_TREE);

    af.m_createFunc = createBvBvAgent;
    af.m_getPenetrationsFunc = staticGetPenetrations;
    af.m_getClosestPointFunc = staticGetClosestPoints;
    af.m_linearCastFunc = staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::BV_TREE, hkcdShapeType::BV_TREE);
    dispatcher->registerCollisionAgent(af, hkcdShapeType::TRI_SAMPLED_HEIGHT_FIELD_BV_TREE, hkcdShapeType::TRI_SAMPLED_HEIGHT_FIELD_BV_TREE);
}

// 0x00D76C70: no partners; the cached AABB empty (min and max at +MAX, w kept)
hkpBvTreeAgent::hkpBvTreeAgent(hkpContactMgr* mgr)
    : hkpCollisionAgent(mgr)
{
    m_cachedAabb.m_max.m_quad = setXYZ_W(g_vectorfConstants[HK_QUADREAL_MAX], m_cachedAabb.m_max.m_quad);
    m_cachedAabb.m_min.m_quad = setXYZ_W(g_vectorfConstants[HK_QUADREAL_MAX], m_cachedAabb.m_min.m_quad);
}

// 0x00D79480
hkpBvTreeAgent::~hkpBvTreeAgent()
{
}

// 0x00D76CD0
hkpCollisionAgent* hkpBvTreeAgent::createShapeBvAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpBvTreeAgent(mgr);
}

// 0x00D76D00
hkpCollisionAgent* hkpBvTreeAgent::createBvTreeShapeAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpSymmetricAgent<hkpBvTreeAgent>(mgr);
}

// 0x00D76D40: the smaller body (object radius) is the one queried against the other's tree
hkpCollisionAgent* hkpBvTreeAgent::createBvBvAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    const float radiusA = hkBvTreeAgent_motionState(bodyA).m_objectRadius;
    const float radiusB = hkBvTreeAgent_motionState(bodyB).m_objectRadius;
    if (lessOrNaN(splat(radiusA), splat(radiusB)))
    {
        return new hkpBvTreeAgent(mgr);
    }
    return new hkpSymmetricAgent<hkpBvTreeAgent>(mgr);
}

// 0x00D764B0
void hkpBvTreeAgent::cleanup(hkpConstraintOwner& info)
{
    for (int i = 0; i < m_collisionPartners.getSize(); i++)
    {
        if (m_collisionPartners[i].m_collisionAgent)
        {
            m_collisionPartners[i].m_collisionAgent->cleanup(info);
        }
    }
    delete this;
}

// 0x00D76500
void hkpBvTreeAgent::invalidateTim(const hkpCollisionInput& input)
{
    for (int i = 0; i < m_collisionPartners.getSize(); i++)
    {
        if (m_collisionPartners[i].m_collisionAgent)
        {
            m_collisionPartners[i].m_collisionAgent->invalidateTim(input);
        }
    }
}

// 0x00D76540
void hkpBvTreeAgent::warpTime(float oldTime, float newTime, const hkpCollisionInput& input)
{
    for (int i = 0; i < m_collisionPartners.getSize(); i++)
    {
        if (m_collisionPartners[i].m_collisionAgent)
        {
            m_collisionPartners[i].m_collisionAgent->warpTime(oldTime, newTime, input);
        }
    }
}

// 0x00D76640: the partners the filter now disables lose their agents (null agent); enabled null partners get one
void hkpBvTreeAgent::updateShapeCollectionFilter(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpConstraintOwner& constraintOwner)
{
    const hkpShapeContainer* container = hkBvTreeAgent_tree(bodyB)->getContainer();
    for (int i = 0; i < m_collisionPartners.getSize(); i++)
    {
        hkpShapeBuffer buffer;
        const unsigned int key = m_collisionPartners[i].m_key;
        hkpCdBody childBody(&bodyB);
        childBody.setShape(container->getChildShape(key, buffer), key);
        if (hkBvTreeAgent_isEnabled(input, bodyA, bodyB, *container, key))
        {
            if (m_collisionPartners[i].m_collisionAgent == hkpNullAgent::getNullAgent())
            {
                m_collisionPartners[i].m_collisionAgent = input.m_dispatcher->getNewCollisionAgent(bodyA, childBody, input, m_contactMgr);
            }
            else
            {
                m_collisionPartners[i].m_collisionAgent->updateShapeCollectionFilter(bodyA, childBody, input, constraintOwner);
            }
        }
        else if (m_collisionPartners[i].m_collisionAgent != hkpNullAgent::getNullAgent())
        {
            m_collisionPartners[i].m_collisionAgent->cleanup(constraintOwner);
            m_collisionPartners[i].m_collisionAgent = hkpNullAgent::getNullAgent();
        }
    }
}

// 0x00D76590: A's AABB in B's space, grown by the tolerance
void hkpBvTreeAgent::staticCalcAabb(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkAabb& aabbOut)
{
    alignas(16) hkTransformf bTa;
    bTa.setMulInverseMul(bodyB.getTransform(), bodyA.getTransform());
    bodyA.getShape()->getAabb(bTa, input.m_tolerance, aabbOut);
}

// 0x00D76A70: and swept along the cast path (in B's space)
void hkpBvTreeAgent::calcAabbLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkAabb& aabbOut)
{
    alignas(16) hkTransformf bTa;
    bTa.setMulInverseMul(bodyB.getTransform(), bodyA.getTransform());
    bodyA.getShape()->getAabb(bTa, input.m_tolerance, aabbOut);
    const Q path = rotateInverse(bodyB.getTransform().getRotation(), input.m_path.m_quad);
    const Q zero = _mm_setzero_ps();
    aabbOut.m_min.m_quad = add(_mm_min_ps(zero, path), aabbOut.m_min.m_quad);
    aabbOut.m_max.m_quad = add(_mm_max_ps(zero, path), aabbOut.m_max.m_quad);
}

// 0x00D77DC0: the children in A's AABB (queryAabb); stops at the collector's early out
void hkpBvTreeAgent::staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    hkAabb aabb;
    staticCalcAabb(bodyA, bodyB, input, aabb);
    const hkpBvTreeShape* tree = hkBvTreeAgent_tree(bodyB);
    hkInplaceArray<unsigned int, 128, hkContainerHeapAllocator> hitList;
    tree->queryAabb(aabb, hitList);
    const hkcdShapeType::ShapeTypeEnum typeA = static_cast<hkcdShapeType::ShapeTypeEnum>(bodyA.getShape()->m_type.m_storage);
    hkpCdBody childBody(&bodyB);
    const hkpShapeContainer* container = tree->getContainer();
    for (int i = 0; i < hitList.getSize(); i++)
    {
        const unsigned int key = hitList[i];
        if (!hkBvTreeAgent_isEnabled(input, bodyA, bodyB, *container, key))
        {
            continue;
        }
        hkpShapeBuffer buffer;
        childBody.setShape(container->getChildShape(key, buffer), key);
        const hkpCollisionDispatcher* dispatcher = input.m_dispatcher;
        dispatcher->getGetPenetrationsFunc(typeA, static_cast<hkcdShapeType::ShapeTypeEnum>(childBody.getShape()->m_type.m_storage))(bodyA, childBody, input, collector);
        if (collector.m_earlyOut.m_bool)
        {
            break;
        }
    }
}

// 0x00D780D0
void hkpBvTreeAgent::getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    staticGetPenetrations(bodyA, bodyB, input, collector);
}

// 0x00D767D0
// The children in A's AABB (queryAabbImpl into a LIFO buffer of 2048 keys). Original behaviour kept: without a
// collision filter nothing is reported.
void hkpBvTreeAgent::staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    hkAabb aabb;
    staticCalcAabb(bodyA, bodyB, input, aabb);
    const int maxKeys = 0x800; // 2048 keys: one 8 KB LIFO block
    hkLifoAllocator& stack = hkMemoryRouter::getInstance().stack();
    unsigned int* hits = static_cast<unsigned int*>(stack.fastBlockAlloc(maxKeys * (int)sizeof(unsigned int)));
    const hkpBvTreeShape* tree = hkBvTreeAgent_tree(bodyB);
    int numHits = (int)tree->queryAabbImpl(aabb, hits, maxKeys);
    if (numHits)
    {
        if (numHits >= maxKeys)
        {
            numHits = maxKeys;
        }
        const hkpShapeContainer* container = tree->getContainer();
        hkpCdBody childBody(&bodyB);
        const hkcdShapeType::ShapeTypeEnum typeA = static_cast<hkcdShapeType::ShapeTypeEnum>(bodyA.getShape()->m_type.m_storage);
        for (int i = 0; i < numHits; i++)
        {
            const unsigned int key = hits[i];
            if (input.m_filter.val() == nullptr || !hkBvTreeAgent_isEnabled(input, bodyA, bodyB, *container, key))
            {
                continue;
            }
            hkpShapeBuffer buffer;
            childBody.setShape(container->getChildShape(key, buffer), key);
            const hkpCollisionDispatcher* dispatcher = input.m_dispatcher;
            dispatcher->getGetClosestPointsFunc(typeA, static_cast<hkcdShapeType::ShapeTypeEnum>(childBody.getShape()->m_type.m_storage))(bodyA, childBody, input, collector);
        }
    }
    stack.fastBlockFree(hits, maxKeys * (int)sizeof(unsigned int));
}

// 0x00D76C50
void hkpBvTreeAgent::getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    staticGetClosestPoints(bodyA, bodyB, input, collector);
}

// 0x00DB1910
hkpBvTreeAgent::LinearCastAabbCastCollector::LinearCastAabbCastCollector(const hkpCdBody& castBody, const hkpCdBody& bvTreeBody, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& castCollector, hkpCdPointCollector* startCollector)
    : m_castBody(castBody), m_bvTreeBody(bvTreeBody), m_input(input), m_castCollector(castCollector), m_startCollector(startCollector)
{
}

// 0x00DB18E0
hkpBvTreeAgent::LinearCastAabbCastCollector::~LinearCastAabbCastCollector()
{
}

// 0x00D76B70: the child's linear cast, then the early out follows the cast collector's
void hkpBvTreeAgent::LinearCastAabbCastCollector::addHit(unsigned int key)
{
    const hkpShapeContainer* container = hkBvTreeAgent_tree(m_bvTreeBody)->getContainer();
    if (m_input.m_filter.val() == nullptr || !hkBvTreeAgent_isEnabled(m_input, m_castBody, m_bvTreeBody, *container, key))
    {
        return;
    }
    hkpShapeBuffer buffer;
    hkpCdBody childBody(&m_bvTreeBody);
    childBody.setShape(container->getChildShape(key, buffer), key);
    const hkpCollisionDispatcher* dispatcher = m_input.m_dispatcher;
    dispatcher->getLinearCastFunc(static_cast<hkcdShapeType::ShapeTypeEnum>(m_castBody.getShape()->m_type.m_storage), static_cast<hkcdShapeType::ShapeTypeEnum>(childBody.getShape()->m_type.m_storage))(m_castBody, childBody, m_input, m_castCollector, m_startCollector);
    m_earlyOutFraction.m_real = _mm_min_ps(m_earlyOutFraction.m_real, splat(m_castCollector.m_earlyOutDistance));
}

// 0x00D77BD0: the children in the swept AABB
void hkpBvTreeAgent::staticLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
{
    hkAabb aabb;
    calcAabbLinearCast(bodyA, bodyB, input, aabb);
    const hkpBvTreeShape* tree = hkBvTreeAgent_tree(bodyB);
    hkInplaceArray<unsigned int, 128, hkContainerHeapAllocator> hitList;
    tree->queryAabb(aabb, hitList);
    const hkcdShapeType::ShapeTypeEnum typeA = static_cast<hkcdShapeType::ShapeTypeEnum>(bodyA.getShape()->m_type.m_storage);
    hkpCdBody childBody(&bodyB);
    const hkpShapeContainer* container = tree->getContainer();
    for (int i = 0; i < hitList.getSize(); i++)
    {
        const unsigned int key = hitList[i];
        if (!hkBvTreeAgent_isEnabled(input, bodyA, bodyB, *container, key))
        {
            continue;
        }
        hkpShapeBuffer buffer;
        childBody.setShape(container->getChildShape(key, buffer), key);
        const hkpCollisionDispatcher* dispatcher = input.m_dispatcher;
        dispatcher->getLinearCastFunc(typeA, static_cast<hkcdShapeType::ShapeTypeEnum>(childBody.getShape()->m_type.m_storage))(bodyA, childBody, input, collector, startCollector);
    }
}

// 0x00D780A0
void hkpBvTreeAgent::linearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
{
    staticLinearCast(bodyA, bodyB, input, collector, startCollector);
}

// 0x00D76DD0
// The tree is queried with A's AABB in B's space (with continuous physics clipped to A's swept sphere and grown by
// the step's movement); with AABB caching an AABB inside the cached one keeps the partners as they are, otherwise
// the cache grows by the tolerance, 40% of the size and twice the movement against it. The partners then follow the
// hits (fast update: in hit order; else sorted by key, merged), new children getting an agent (a null agent when
// filtered), and every partner's agent is processed.
void hkpBvTreeAgent::processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result)
{
    hkpConstraintOwner& constraintOwner = *result.m_constraintOwner;
    hkInplaceArray<unsigned int, 128, hkContainerHeapAllocator> hitList;
    alignas(16) hkTransformf bTa;
    bTa.setMulInverseMul(bodyB.getTransform(), bodyA.getTransform());
    const hkMotionState& msA = hkBvTreeAgent_motionState(bodyA);
    const hkMotionState& msB = hkBvTreeAgent_motionState(bodyB);
    const Q timInfo = hkBvTreeAgent_calcTimInfo(msA, msB, input.m_stepInfo.m_deltaTime);
    hkAabb* cachedAabb = m_useAabbCaching.m_bool ? &m_cachedAabb : nullptr;

    // the AABB
    const hkTransformf& transformB = bodyB.getTransform();
    const Q linearB = rotateInverse(transformB.getRotation(), timInfo);
    const Q halfTolerance = mul(g_vectorfConstants[HK_QUADREAL_INV_2], splat(input.m_tolerance));
    const Q zero = _mm_setzero_ps();
    hkAabb aabb;
    Q size;
    Q movement;
    if (input.m_collisionQualityInfo->m_useContinuousPhysics.val())
    {
        // A's AABB grown by its worst case rotation, clipped to the sphere around its centre of mass
        const Q angleB = bcast<3>(msB.m_deltaAngle.m_quad);
        const Q rotationB = mul(splat(msB.m_objectRadius), mul(angleB, angleB));
        const Q radiusA = splat(msA.m_objectRadius);
        const Q tolerance = add(add(mul(add(bcast<3>(msA.m_deltaAngle.m_quad), angleB), radiusA), rotationB), halfTolerance);
        bodyA.getShape()->getAabb(bTa, lane0(tolerance), aabb);
        const Q centerA = rotateInverse(transformB.getRotation(), sub(msA.m_sweptTransform.m_centerOfMass1.m_quad, transformB.getTranslation().m_quad));
        const Q extent = _mm_castsi128_ps(_mm_srli_si128(_mm_slli_si128(_mm_castps_si128(add(add(radiusA, halfTolerance), rotationB)), 4), 4));
        aabb.m_min.m_quad = _mm_max_ps(aabb.m_min.m_quad, sub(centerA, extent));
        aabb.m_max.m_quad = _mm_min_ps(aabb.m_max.m_quad, add(centerA, extent));
        size = sub(aabb.m_max.m_quad, aabb.m_min.m_quad);
        movement = linearB;
        if (greater(bcast<3>(msB.m_deltaAngle.m_quad), zero))
        {
            // B's rotation moves A's centre too
            const Q arm = sub(centerA, msB.m_sweptTransform.m_centerOfMassLocal.m_quad);
            const Q scaleB = mul(splat(input.m_stepInfo.m_deltaTime), bcast<3>(msB.m_sweptTransform.m_centerOfMass1.m_quad));
            movement = add(mul(scaleB, cross(arm, msB.m_deltaAngle.m_quad)), linearB);
        }
        aabb.m_min.m_quad = add(_mm_min_ps(zero, movement), aabb.m_min.m_quad);
        aabb.m_max.m_quad = add(_mm_max_ps(zero, movement), aabb.m_max.m_quad);
    }
    else
    {
        bodyA.getShape()->getAabb(bTa, lane0(halfTolerance), aabb);
        size = sub(aabb.m_max.m_quad, aabb.m_min.m_quad);
        movement = linearB;
    }

    bool updatePartners = true;
    if (cachedAabb)
    {
        const Q inside = _mm_and_ps(_mm_cmple_ps(aabb.m_max.m_quad, cachedAabb->m_max.m_quad), _mm_cmple_ps(cachedAabb->m_min.m_quad, aabb.m_min.m_quad));
        if ((_mm_movemask_ps(inside) & 7) == 7)
        {
            updatePartners = false;
        }
        else
        {
            const Q tol = _mm_castsi128_ps(_mm_srli_si128(_mm_slli_si128(_mm_castps_si128(halfTolerance), 4), 4));
            const Q grow = mul(splat(0.4f), size); // 0x012BA508: 0x3ECCCCCD
            const Q minusTwo = splat(-2.0f); // 0x012CEE28: 0xC0000000
            const Q newMin = add(_mm_max_ps(mul(_mm_max_ps(zero, movement), minusTwo), neg(grow)), sub(aabb.m_min.m_quad, tol));
            const Q newMax = add(_mm_min_ps(mul(_mm_min_ps(zero, movement), minusTwo), grow), add(aabb.m_max.m_quad, tol));
            aabb.m_min.m_quad = newMin;
            aabb.m_max.m_quad = newMax;
            *cachedAabb = aabb;
        }
    }

    if (updatePartners)
    {
        const hkpBvTreeShape* tree = hkBvTreeAgent_tree(bodyB);
        tree->queryAabb(aabb, hitList);
        const hkpShapeContainer* container = tree->getContainer();
        hkpCdBody childBody(&bodyB);
        hkMemoryAllocator& alloc = hkContainerHeapAllocator::s_alloc;
        if (m_useFastUpdate.m_bool)
        {
            // the partners in hit order: those not hit any more are removed
            const unsigned int* hitsBegin = hitList.begin();
            const unsigned int* hitsEnd = hitsBegin + hitList.getSize();
            const unsigned int* cursor = hitsBegin;
            for (int i = 0; i < m_collisionPartners.getSize();)
            {
                hkpBvAgentEntryInfo& entry = m_collisionPartners[i];
                if (cursor != hitsEnd && entry.m_key == *cursor)
                {
                    cursor++;
                    i++;
                    continue;
                }
                const unsigned int* found = hitsEnd;
                for (const unsigned int* p = hitsBegin; p != hitsEnd; p++)
                {
                    if (entry.m_key == *p)
                    {
                        found = p;
                        break;
                    }
                }
                if (found != hitsEnd)
                {
                    cursor = found + 1;
                    i++;
                    continue;
                }
                entry.m_collisionAgent->cleanup(constraintOwner);
                m_collisionPartners.removeAtAndCopy(i);
                cursor = hitsEnd;
            }

            // new hits are inserted at their hit index
            if (hitList.getSize() != m_collisionPartners.getSize())
            {
                int partner = 0;
                for (int h = 0; h < hitList.getSize(); h++, partner++)
                {
                    const unsigned int key = hitList[h];
                    if (partner != m_collisionPartners.getSize() && m_collisionPartners[partner].m_key == key)
                    {
                        continue;
                    }
                    const int partnerCount = m_collisionPartners.getSize();
                    m_collisionPartners._reserve(alloc, partnerCount + 1);
                    hkMemUtil::memMove(&m_collisionPartners[h + 1], &m_collisionPartners[h], (partnerCount - h) * (int)sizeof(hkpBvAgentEntryInfo));
                    m_collisionPartners.setSizeUnchecked(partnerCount + 1);
                    partner = h;
                    hkpShapeBuffer buffer;
                    childBody.setShape(container->getChildShape(key, buffer), key);
                    hkpCollisionAgent* agent;
                    if (hkBvTreeAgent_isEnabled(input, bodyA, bodyB, *container, key))
                    {
                        agent = input.m_dispatcher->getNewCollisionAgent(bodyA, childBody, input, m_contactMgr);
                    }
                    else
                    {
                        agent = hkpNullAgent::getNullAgent();
                    }
                    // Original behaviour kept: m_userInfo is left as the moved memory leaves it.
                    m_collisionPartners[h].m_collisionAgent = agent;
                    m_collisionPartners[h].m_key = key;
                }
            }
        }
        else
        {
            // sorted hits merged with the (sorted) partners into a new array
            if (hitList.getSize() > 1)
            {
                hkAlgorithm::quickSortRecursive(hitList.begin(), 0, hitList.getSize() - 1, hkAlgorithm::less<unsigned int>());
            }
            const int numHits = hitList.getSize();
            hkLocalArray<hkpBvAgentEntryInfo> newPartners(numHits);
            newPartners._reserve(alloc, numHits);
            newPartners.setSizeUnchecked(numHits);
            hkpBvAgentEntryInfo* out = newPartners.begin();
            const unsigned int* hit = hitList.begin();
            const unsigned int* hitsEnd = hit + numHits;
            hkpBvAgentEntryInfo* old = m_collisionPartners.begin();
            hkpBvAgentEntryInfo* oldEnd = old + m_collisionPartners.getSize();
            for (; old != oldEnd;)
            {
                if (hit == hitsEnd)
                {
                    for (; old != oldEnd; old++)
                    {
                        if (old->m_collisionAgent)
                        {
                            old->m_collisionAgent->cleanup(constraintOwner);
                        }
                    }
                    break;
                }
                if (*hit == old->m_key)
                {
                    *out++ = *old++;
                    hit++;
                }
                else if (*hit > old->m_key)
                {
                    if (old->m_collisionAgent)
                    {
                        old->m_collisionAgent->cleanup(constraintOwner);
                    }
                    old++;
                }
                else
                {
                    hkpShapeBuffer buffer;
                    childBody.setShape(container->getChildShape(*hit, buffer), *hit);
                    if (hkBvTreeAgent_isEnabled(input, bodyA, bodyB, *container, *hit))
                    {
                        out->m_collisionAgent = input.m_dispatcher->getNewCollisionAgent(bodyA, childBody, input, m_contactMgr);
                    }
                    else
                    {
                        out->m_collisionAgent = hkpNullAgent::getNullAgent();
                    }
                    // Original behaviour kept: m_userInfo is left uninitialized (LIFO memory).
                    out->m_key = *hit;
                    out++;
                    hit++;
                }
            }
            for (; hit != hitsEnd; hit++, out++)
            {
                hkpShapeBuffer buffer;
                childBody.setShape(container->getChildShape(*hit, buffer), *hit);
                if (hkBvTreeAgent_isEnabled(input, bodyA, bodyB, *container, *hit))
                {
                    out->m_collisionAgent = input.m_dispatcher->getNewCollisionAgent(bodyA, childBody, input, m_contactMgr);
                }
                else
                {
                    out->m_collisionAgent = hkpNullAgent::getNullAgent();
                }
                out->m_key = *hit;
            }
            m_collisionPartners._reserve(alloc, numHits);
            for (int i = 0; i < numHits; i++)
            {
                m_collisionPartners.begin()[i] = newPartners[i];
            }
            m_collisionPartners.setSizeUnchecked(numHits);
        }
    }
    hitList._clearAndDeallocate(hkContainerHeapAllocator::s_alloc);

    // the partners' agents
    const hkpShapeContainer* container = hkBvTreeAgent_tree(bodyB)->getContainer();
    hkpCdBody childBody(&bodyB);
    for (int i = 0; i < m_collisionPartners.getSize(); i++)
    {
        hkpShapeBuffer buffer;
        const unsigned int key = m_collisionPartners[i].m_key;
        childBody.setShape(container->getChildShape(key, buffer), key);
        m_collisionPartners[i].m_collisionAgent->processCollision(bodyA, childBody, input, result);
    }
}
