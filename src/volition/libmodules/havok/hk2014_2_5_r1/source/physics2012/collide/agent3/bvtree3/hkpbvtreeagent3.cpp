// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/bvtree3/hkpbvtreeagent3.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the BV tree agent3, from rfg.exe: a body against the children of a BV tree
// shape B that its swept box (calcAabbAndQueryTree, the box of hkpBvTreeAgent::processCollision) reaches, through a
// 1n agent track created by the list agent3 (the agent data: 4 unused bytes, then the track).
//
// The shipped build records the "LtBvTree3"/"StQueryTree"/"StNarrow" monitor timers; they are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/bvtree3/hkpbvtreeagent3.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hksort.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionqualityinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/list3/hkplistagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1nmachine.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1ntrack.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/hkpshapecollection.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/tree/hkpbvtreeshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/tree/mopp/hkpmoppbvtreeshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

static inline hkpAgent1nTrack& hkBvTreeAgent3_track(void* agentData)
{
    return *reinterpret_cast<hkpAgent1nTrack*>(static_cast<char*>(agentData) + 4);
}
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"

// 0x00DB1800: the list agent3's create and destroy
void hkBvTreeAgent3::initAgentFunc(hkpCollisionDispatcher::Agent3Funcs& f)
{
    f.m_createFunc = hkListAgent3::create;
    f.m_processFunc = process;
    f.m_sepNormalFunc = nullptr;
    f.m_cleanupFunc = nullptr;
    f.m_removePointFunc = nullptr;
    f.m_commitPotentialFunc = nullptr;
    f.m_createZombieFunc = nullptr;
    f.m_updateFilterFunc = updateFilter;
    f.m_invalidateTimFunc = invalidateTim;
    f.m_warpTimeFunc = warpTime;
    f.m_destroyFunc = hkListAgent3::destroy;
    f.m_isPredictive = true;
}

// 0x00DB1850
void hkBvTreeAgent3::registerAgent3(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::Agent3Funcs f;
    initAgentFunc(f);
    dispatcher->registerAgent3(f, hkcdShapeType::ALL_SHAPE_TYPES, hkcdShapeType::BV_TREE);
}

// 0x00DB1180
void hkBvTreeAgent3::invalidateTim(hkpAgentEntry* entry, void* agentData, const hkpCollisionInput& input)
{
    hkAgent1nMachine_InvalidateTim(hkBvTreeAgent3_track(agentData), input);
}

// 0x00DB11A0
void hkBvTreeAgent3::warpTime(hkpAgentEntry* entry, void* agentData, float oldTime, float newTime, const hkpCollisionInput& input)
{
    hkAgent1nMachine_WarpTime(hkBvTreeAgent3_track(agentData), oldTime, newTime, input);
}

// 0x00DB11E0
void hkBvTreeAgent3::updateFilter(hkpAgentEntry* entry, void* agentData, const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner)
{
    hkpAgent1nMachine_VisitorInput vin;
    vin.m_containerShapeB = bodyB.getShape()->getContainer();
    vin.m_bodyA = &bodyA;
    vin.m_contactMgr = mgr;
    vin.m_input = &input;
    vin.m_collectionBodyB = &bodyB;
    vin.m_constraintOwner = &constraintOwner;
    hkAgent1nMachine_UpdateShapeCollectionFilter(hkBvTreeAgent3_track(agentData), vin);
}

// 0x00DB1240
// The keys of the children of B in the swept box of A (in the space of B; bTa is the transform of A there): the box
// of A at half the tolerance, with continuous physics grown by both rotations and clipped to the sphere around the
// centre of mass of A, then stretched along the linear tim (the rotation of B moves the centre of A too). With a
// cache: -1 when the box stays inside it, else the cache grows by 0.4 of the size and twice the movement. The list
// ends with the invalid key; more hits than the capacity go to numShapeKeyHitsLimitBreached of the filter.
int hkBvTreeAgent3::calcAabbAndQueryTree(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkTransformf& bTa, const hkVector4f& linearTimInfo, const hkpProcessCollisionInput& input, hkAabb* cachedAabb, unsigned int* hitListOut, int hitListCapacity)
{
    const Q halfTolerance = mul(hkVector4f::getConstant<HK_QUADREAL_INV_2>().m_quad, splat(input.m_tolerance));
    const hkMotionState& msA = *static_cast<const hkMotionState*>(bodyA.getMotionState());
    const hkMotionState& msB = *static_cast<const hkMotionState*>(bodyB.getMotionState());
    const hkTransformf& transformB = msB.getTransform();
    const Q linearB = rotateInverse(transformB.getRotation(), linearTimInfo.m_quad);
    const Q zero = _mm_setzero_ps();
    hkAabb aabb;
    Q size;
    Q movement;
    if (input.m_collisionQualityInfo->m_useContinuousPhysics.val())
    {
        const Q angleB = bcast<3>(msB.m_deltaAngle.m_quad);
        const Q rotationB = mul(mul(angleB, angleB), splat(msB.m_objectRadius));
        const Q radiusA = splat(msA.m_objectRadius);
        const Q tolerance = add(add(mul(add(bcast<3>(msA.m_deltaAngle.m_quad), angleB), radiusA), halfTolerance), rotationB);
        bodyA.getShape()->getAabb(bTa, lane0(tolerance), aabb);
        const Q centerA = rotateInverse(transformB.getRotation(), sub(msA.m_sweptTransform.m_centerOfMass1.m_quad, transformB.getTranslation().m_quad));
        const Q extent = _mm_castsi128_ps(_mm_srli_si128(_mm_slli_si128(_mm_castps_si128(add(add(radiusA, halfTolerance), rotationB)), 4), 4));
        aabb.m_min.m_quad = _mm_max_ps(aabb.m_min.m_quad, sub(centerA, extent));
        aabb.m_max.m_quad = _mm_min_ps(aabb.m_max.m_quad, add(centerA, extent));
        size = sub(aabb.m_max.m_quad, aabb.m_min.m_quad);
        movement = linearB;
        if (greater(bcast<3>(msB.m_deltaAngle.m_quad), zero))
        {
            const Q arm = sub(centerA, msB.m_sweptTransform.m_centerOfMassLocal.m_quad);
            const Q scaleB = mul(bcast<3>(msB.m_sweptTransform.m_centerOfMass1.m_quad), splat(input.m_stepInfo.m_deltaTime));
            movement = add(mul(scaleB, cross(arm, msB.m_deltaAngle.m_quad)), linearB);
        }
        aabb.m_min.m_quad = add(_mm_min_ps(zero, movement), aabb.m_min.m_quad);
        aabb.m_max.m_quad = add(aabb.m_max.m_quad, _mm_max_ps(zero, movement));
    }
    else
    {
        // 0.5 (0x3F000000) as a scalar here
        bodyA.getShape()->getAabb(bTa, input.m_tolerance * 0.5f, aabb);
        size = sub(aabb.m_max.m_quad, aabb.m_min.m_quad);
        movement = linearB;
    }

    if (cachedAabb)
    {
        const Q inside = _mm_and_ps(_mm_cmple_ps(aabb.m_max.m_quad, cachedAabb->m_max.m_quad), _mm_cmple_ps(cachedAabb->m_min.m_quad, aabb.m_min.m_quad));
        if ((_mm_movemask_ps(inside) & 7) == 7)
        {
            return -1;
        }
        const Q tol = _mm_castsi128_ps(_mm_srli_si128(_mm_slli_si128(_mm_castps_si128(halfTolerance), 4), 4));
        const Q lo = sub(aabb.m_min.m_quad, tol);
        const Q hi = add(tol, aabb.m_max.m_quad);
        const Q grow = mul(splat(0.4f), size); // 0x012BA508: 0x3ECCCCCD
        const Q minusTwo = sub(zero, hkVector4f::getConstant<HK_QUADREAL_2>().m_quad);
        aabb.m_min.m_quad = add(_mm_max_ps(mul(_mm_max_ps(zero, movement), minusTwo), neg(grow)), lo);
        aabb.m_max.m_quad = add(_mm_min_ps(mul(_mm_min_ps(zero, movement), minusTwo), grow), hi);
        *cachedAabb = aabb;
    }

    const hkpBvTreeShape* tree = static_cast<const hkpBvTreeShape*>(bodyB.getShape());
    int numHits = (int)tree->queryAabbImpl(aabb, hitListOut, hitListCapacity);
    if (numHits >= hitListCapacity)
    {
        const hkpShapeCollectionFilter* filter = input.m_filter.val();
        numHits = filter->numShapeKeyHitsLimitBreached(input, bodyA, bodyB, tree, aabb, hitListOut, hitListCapacity);
        if (numHits >= hitListCapacity - 1)
        {
            numHits = hitListCapacity - 1;
        }
    }
    hitListOut[numHits] = 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
    return numHits;
}

// 0x00DB1630: the collection of a sampled heightfield BV tree or a MOPP (the shapes whose AABB queries use it)
const hkpShapeCollection* hkBvTreeAgent3::getShapeCollectionIfBvTreeSupportsAabbQueries(const hkpCdBody* body, hkpShapeBufferStorage& buffer)
{
    const hkpShape* shape = body->getShape();
    if (shape->m_type.m_storage == hkcdShapeType::TRI_SAMPLED_HEIGHT_FIELD_BV_TREE)
    {
        // the child collection pointer of the heightfield BV tree, at +0x18
        return *reinterpret_cast<const hkpShapeCollection* const*>(reinterpret_cast<const char*>(shape) + 0x18);
    }
    if (shape->m_type.m_storage != hkcdShapeType::MOPP)
    {
        return nullptr;
    }
    return static_cast<const hkpMoppBvTreeShape*>(shape)->getShapeCollection();
}

// 0x00DB1650: the children in the swept box of A, sorted, through the 1n machine
void* hkBvTreeAgent3::process(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkVector4f* separatingNormal, hkpProcessCollisionOutput& output)
{
    hkLifoAllocator& stack = hkMemoryRouter::getInstance().stack();
    const int capacity = 0x800; // 2048 keys
    unsigned int* hitList = static_cast<unsigned int*>(stack.fastBlockAlloc(capacity * (int)sizeof(unsigned int)));
    alignas(16) hkTransformf bTa;
    bTa.setInverse(input.m_aTb);
    const int numHits = calcAabbAndQueryTree(*input.m_bodyA.val(), *input.m_bodyB.val(), bTa, input.m_linearTimInfo, *input.m_input.val(), nullptr, hitList, capacity);
    if (numHits > 1)
    {
        hkAlgorithm::quickSortRecursive(hitList, 0, numHits - 1, hkAlgorithm::less<unsigned int>());
    }
    const hkpShapeContainer* container = input.m_bodyB.val()->getShape()->getContainer();
    hkAgent1nMachine_Process(hkBvTreeAgent3_track(agentData), input, container, numHits < 0 ? nullptr : hitList, output);
    stack.fastBlockFree(hitList, capacity * (int)sizeof(unsigned int));
    return static_cast<char*>(agentData) + 0x10;
}
