// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/collectioncollection3/hkpcollectioncollectionagent3.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the collection-collection agent3, from rfg.exe: the child pairs of two
// collections whose integer boxes overlap (hk1AxisSweep), through the nm machine (or the 1n machine when the
// collection agent3 overrides body A). One side may be a BV tree: its children are found by a tree query against the
// box of the other body. The agent data: 4 unused bytes, then the track (created by the list agent3).
//
// The shipped build records the "LtCollColl3"/"LtProcessCollColl" monitor timers and the "MiNumKeyPairs" value; they
// are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/collectioncollection3/hkpcollectioncollectionagent3.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/collide/1axissweep/hk1axissweep.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hksort.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabbutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionqualityinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/bvtree3/hkpbvtreeagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/hkpagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/list3/hkplistagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1nmachine.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1ntrack.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/hkpshapecollection.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/list/hkplistshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapebuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"

// 0x03A29D70: set once the midphase agent is registered
bool hkpCollectionCollectionAgent3::g_agentRegistered;

static inline hkpAgent1nTrack& hkCollectionCollectionAgent3_track(void* agentData)
{
    return *reinterpret_cast<hkpAgent1nTrack*>(static_cast<char*>(agentData) + 4);
}

// The box list of one side: (n + 4) boxes of 0x20 bytes (four end markers for the sweep), from the LIFO stack
// (hkAllocateStack: rounded up to 128 bytes).
static inline int hkCollectionCollectionAgent3_listBytes(int n)
{
    return ((n + 4) * (int)sizeof(hk1AxisSweep::AabbInt) + 127) & ~127;
}

// PSLLD by a register count: counts above 31 give 0.
static inline unsigned int hkCollectionCollectionAgent3_shl(unsigned int x, unsigned int shift)
{
    return shift > 31 ? 0 : x << shift;
}

// A cached integer box grown by its expansion (the continuous sweep stored with it). The key slot (+0x0C) and the
// last word (+0x1C) of the output are not written.
static inline void hkCollectionCollectionAgent3_expand(const hkAabbUint32& in, hk1AxisSweep::AabbInt& out)
{
    const unsigned int shift = in.m_expansionShift;
    out.m_min[0] = in.m_min[0] - hkCollectionCollectionAgent3_shl(in.m_expansionMin[0], shift);
    out.m_min[1] = in.m_min[1] - hkCollectionCollectionAgent3_shl(in.m_expansionMin[1], shift);
    out.m_min[2] = in.m_min[2] - hkCollectionCollectionAgent3_shl(in.m_expansionMin[2], shift);
    out.m_max[0] = in.m_max[0] + hkCollectionCollectionAgent3_shl(in.m_expansionMax[0], shift);
    out.m_max[1] = in.m_max[1] + hkCollectionCollectionAgent3_shl(in.m_expansionMax[1], shift);
    out.m_max[2] = in.m_max[2] + hkCollectionCollectionAgent3_shl(in.m_expansionMax[2], shift);
}

static inline unsigned int& hkCollectionCollectionAgent3_key(hk1AxisSweep::AabbInt& aabb)
{
    return *reinterpret_cast<unsigned int*>(reinterpret_cast<char*>(&aabb) + 0xC);
}

// A child box in world space (swept when continuous, in place: the final clamp to the input box then compares the
// result with itself, as in the original), converted to the input's 32-bit space.
static inline void hkCollectionCollectionAgent3_childAabb(const hkpShape* child, const hkpCdBody* body, const hkpProcessCollisionInput& input, bool continuous, const hkAabbUtil::OffsetAabbInput& offsetInput, unsigned int key, hk1AxisSweep::AabbInt& out)
{
    hkAabb aabb;
    child->getAabb(body->getTransform(), input.m_tolerance.val(), aabb);
    if (continuous)
    {
        hkAabbUtil::sweepOffsetAabb(offsetInput, aabb, aabb);
    }
    hkAabbUtil::convertAabbToUint32(aabb, input.m_aabb32Info.m_bitOffsetLow, input.m_aabb32Info.m_bitOffsetHigh, input.m_aabb32Info.m_bitScale, out);
    hkCollectionCollectionAgent3_key(out) = key;
}

static inline const hkpCollidable* hkCollectionCollectionAgent3_rootWithChildAabbs(const hkpCdBody* body)
{
    if (body->m_parent != nullptr)
    {
        return nullptr;
    }
    const hkpCollidable* collidable = reinterpret_cast<const hkpCollidable*>(body);
    return collidable->m_boundingVolumeData.m_childShapeAabbs != nullptr ? collidable : nullptr;
}

// 0x00DB7E10 (LTCG: the collision input in ECX): the sorted boxes of a collection's children. A root collidable with
// valid bounding volume data reuses its cached child boxes (grown by their expansion when continuous; these are not
// sorted again when not continuous), or, with an override body, its own box as the only child (key 0). Otherwise each
// child's box is computed. Returns the number of boxes.
static int hkCollectionBvTreeAgent3_extractCachedAabbsOrRecalculate(const hkpCdBody* cdBody, const hkpCdBody* overrideCdBody, const hkpShapeContainer* collection, const hkpProcessCollisionInput& input, hk1AxisSweep::AabbInt* aabbs)
{
    const bool continuous = input.m_collisionQualityInfo.val()->m_useContinuousPhysics.val() != 0;
    int numAabbs = 0;
    if (cdBody->m_parent == nullptr)
    {
        const hkpCollidable::BoundingVolumeData& bv = reinterpret_cast<const hkpCollidable*>(cdBody)->m_boundingVolumeData;
        if (bv.m_childShapeAabbs != nullptr && bv.m_min[0] <= bv.m_max[0])
        {
            if (bv.m_numChildShapeAabbs == 0)
            {
                return 0;
            }
            for (int i = 0; i < (int)bv.m_numChildShapeAabbs; i++)
            {
                if (continuous)
                {
                    hkCollectionCollectionAgent3_expand(bv.m_childShapeAabbs[i], aabbs[i]);
                }
                else
                {
                    static_cast<hkAabbUint32&>(aabbs[i]) = bv.m_childShapeAabbs[i];
                }
                hkCollectionCollectionAgent3_key(aabbs[i]) = bv.m_childShapeKeys[i];
                numAabbs++;
            }
            if (!continuous)
            {
                return numAabbs;
            }
            if (numAabbs > 1)
            {
                hkAlgorithm::quickSortRecursive(aabbs, 0, numAabbs - 1, hkAlgorithm::less<hk1AxisSweep::AabbInt>());
            }
            return numAabbs;
        }
        if (bv.m_min[0] <= bv.m_max[0] && overrideCdBody != nullptr)
        {
            const hkAabbUint32& own = reinterpret_cast<const hkAabbUint32&>(bv);
            if (continuous)
            {
                hkCollectionCollectionAgent3_expand(own, aabbs[0]);
            }
            else
            {
                static_cast<hkAabbUint32&>(aabbs[0]) = own;
            }
            hkCollectionCollectionAgent3_key(aabbs[0]) = 0;
            return 1;
        }
    }

    alignas(16) hkAabbUtil::OffsetAabbInput offsetInput;
    if (continuous)
    {
        hkAabbUtil::initOffsetAabbInput(static_cast<const hkMotionState*>(cdBody->getRootCollidable()->m_motion), offsetInput);
    }
    hkpShapeBuffer buffer;
    for (unsigned int key = collection->getFirstKey(); key != 0xFFFFFFFF; key = collection->getNextKey(key)) // HK_INVALID_SHAPE_KEY
    {
        const hkpShape* child = collection->getChildShape(key, buffer);
        hkCollectionCollectionAgent3_childAabb(child, cdBody, input, continuous, offsetInput, key, aabbs[numAabbs]);
        numAabbs++;
    }
    if (numAabbs > 1)
    {
        hkAlgorithm::quickSortRecursive(aabbs, 0, numAabbs - 1, hkAlgorithm::less<hk1AxisSweep::AabbInt>());
    }
    return numAabbs;
}

// The box list of a side that is not queried as a tree: as many boxes as children (or cached child boxes).
static int hkCollectionCollectionAgent3_extractCapacity(const hkpCdBody* body, const hkpShapeContainer* container)
{
    int count = container->getNumChildShapes();
    if (body->m_parent == nullptr)
    {
        const hkpCollidable::BoundingVolumeData& bv = reinterpret_cast<const hkpCollidable*>(body)->m_boundingVolumeData;
        if (bv.m_childShapeAabbs != nullptr && bv.m_min[0] <= bv.m_max[0] && count <= (int)bv.m_numChildShapeAabbs)
        {
            count = bv.m_numChildShapeAabbs;
        }
    }
    return count;
}

// 0x00DB8410: the overlapping child key pairs (A, B), sorted and ended by (-1, -1); returns their number.
//
// The side queried as a tree: B when A is no BV tree, else A when B is none; with two trees, the side with cached
// child boxes is not the tree, then the list with more enabled children, then (no lists) the body with the larger
// object radius. The other side's children get their boxes from extractCachedAabbsOrRecalculate. Neither side a tree:
// both lists from extract (A with the override body).
int hkpCollectionCollectionAgent3::process_gatherShapeKeys(const hkpAgent3ProcessInput& input, const hkpShapeContainer* shapeContainerA, const hkpShapeContainer* shapeContainerB, hkpShapeKeyPair* shapeKeyPairs)
{
    const hkpCdBody* bodyA = input.m_bodyA.val();
    const hkpCdBody* bodyB = input.m_bodyB.val();
    const hkpProcessCollisionInput& collisionInput = *input.m_input.val();
    const hkpCollisionDispatcher* dispatcher = collisionInput.m_dispatcher.val();
    const unsigned int treeBit = 1u << hkcdShapeType::BV_TREE;
    const bool notTreeB = (dispatcher->m_hasAlternateType[bodyB->getShape()->m_type.m_storage] & treeBit) == 0;
    const bool notTreeA = (dispatcher->m_hasAlternateType[bodyA->getShape()->m_type.m_storage] & treeBit) == 0;

    // true: query B's tree with A's box
    bool queryTreeB = notTreeA;
    if (!notTreeA && !notTreeB)
    {
        const bool cachedA = hkCollectionCollectionAgent3_rootWithChildAabbs(bodyA) != nullptr;
        const bool cachedB = hkCollectionCollectionAgent3_rootWithChildAabbs(bodyB) != nullptr;
        queryTreeB = cachedA;
        if (cachedA == cachedB)
        {
            hkpShapeBuffer bufferA;
            hkpShapeBuffer bufferB;
            const hkpShapeCollection* collA = hkBvTreeAgent3::getShapeCollectionIfBvTreeSupportsAabbQueries(bodyA, bufferA);
            const hkpShapeCollection* collB = hkBvTreeAgent3::getShapeCollectionIfBvTreeSupportsAabbQueries(bodyB, bufferB);
            const bool listA = collA != nullptr && collA->m_type.m_storage == hkcdShapeType::LIST;
            const bool listB = collB != nullptr && collB->m_type.m_storage == hkcdShapeType::LIST;
            if (listA && listB)
            {
                const hkpListShape* la = static_cast<const hkpListShape*>(collA);
                const hkpListShape* lb = static_cast<const hkpListShape*>(collB);
                queryTreeB = la->m_childInfo.getSize() - (int)la->m_numDisabledChildren <= lb->m_childInfo.getSize() - (int)lb->m_numDisabledChildren;
            }
            else if (!listA && !listB)
            {
                // COMISS + JC: false when B's radius is below A's or either is NaN
                const float radiusA = static_cast<const hkMotionState*>(bodyA->m_motion)->m_objectRadius;
                const float radiusB = static_cast<const hkMotionState*>(bodyB->m_motion)->m_objectRadius;
                queryTreeB = !(radiusB < radiusA || radiusB != radiusB || radiusA != radiusA);
            }
            else
            {
                queryTreeB = listA;
            }
        }
    }

    hkLifoAllocator& stack = hkMemoryRouter::getInstance().stack();
    const bool continuous = collisionInput.m_collisionQualityInfo.val()->m_useContinuousPhysics.val() != 0;

    const hkpCdBody* treeBody;
    const hkpCdBody* otherBody;
    const hkpShapeContainer* otherContainer;
    hk1AxisSweep::AabbInt* treeList;
    int treeBytes;
    int numTree;
    if (!(notTreeA && notTreeB))
    {
        unsigned int hitList[0x800]; // 2048 keys
        const hkpShapeContainer* treeContainer;
        if (queryTreeB)
        {
            alignas(16) hkTransformf bTa;
            bTa.setInverse(input.m_aTb);
            numTree = hkBvTreeAgent3::calcAabbAndQueryTree(*bodyA, *bodyB, bTa, input.m_linearTimInfo, collisionInput, nullptr, hitList, 0x800);
            treeBody = bodyB;
            otherBody = bodyA;
            treeContainer = shapeContainerB;
            otherContainer = shapeContainerA;
        }
        else
        {
            // the time of impact info as seen from B: its linear part negated (xyz sign bits flipped)
            alignas(16) hkVector4f negTim;
            negTim.setNeg<3>(input.m_linearTimInfo);
            numTree = hkBvTreeAgent3::calcAabbAndQueryTree(*bodyB, *bodyA, input.m_aTb, negTim, collisionInput, nullptr, hitList, 0x800);
            treeBody = bodyA;
            otherBody = bodyB;
            treeContainer = shapeContainerA;
            otherContainer = shapeContainerB;
        }

        treeBytes = hkCollectionCollectionAgent3_listBytes(numTree);
        treeList = static_cast<hk1AxisSweep::AabbInt*>(stack.fastBlockAlloc(treeBytes));

        alignas(16) hkAabbUtil::OffsetAabbInput offsetInput;
        if (continuous)
        {
            hkAabbUtil::initOffsetAabbInput(static_cast<const hkMotionState*>(treeBody->getRootCollidable()->m_motion), offsetInput);
        }
        hkpShapeBuffer buffer;
        for (int i = 0; i < numTree; i++)
        {
            const unsigned int key = hitList[i];
            const hkpShape* child = treeContainer->getChildShape(key, buffer);
            hkCollectionCollectionAgent3_childAabb(child, treeBody, collisionInput, continuous, offsetInput, key, treeList[i]);
        }
        if (numTree > 1)
        {
            hkAlgorithm::quickSortRecursive(treeList, 0, numTree - 1, hkAlgorithm::less<hk1AxisSweep::AabbInt>());
        }
    }
    else
    {
        // neither side a tree: A's list stands in the tree list
        treeBytes = hkCollectionCollectionAgent3_listBytes(hkCollectionCollectionAgent3_extractCapacity(bodyA, shapeContainerA));
        treeList = static_cast<hk1AxisSweep::AabbInt*>(stack.fastBlockAlloc(treeBytes));
        numTree = hkCollectionBvTreeAgent3_extractCachedAabbsOrRecalculate(bodyA, input.m_overrideBodyA.val(), shapeContainerA, collisionInput, treeList);
        treeBody = bodyA;
        otherBody = bodyB;
        otherContainer = shapeContainerB;
    }

    hk1AxisSweep::AabbInt* otherList;
    int otherBytes;
    int numOther;
    if (numTree != 0)
    {
        otherBytes = hkCollectionCollectionAgent3_listBytes(hkCollectionCollectionAgent3_extractCapacity(otherBody, otherContainer));
        otherList = static_cast<hk1AxisSweep::AabbInt*>(stack.fastBlockAlloc(otherBytes));
        numOther = hkCollectionBvTreeAgent3_extractCachedAabbsOrRecalculate(otherBody, nullptr, otherContainer, collisionInput, otherList);
    }
    else
    {
        numOther = 0;
        otherBytes = hkCollectionCollectionAgent3_listBytes(0);
        otherList = static_cast<hk1AxisSweep::AabbInt*>(stack.fastBlockAlloc(otherBytes));
    }

    // end markers (x minimum 0xFFFFFFFF)
    for (int i = numTree; i < numTree + 4; i++)
    {
        treeList[i].m_min[0] = 0xFFFFFFFF;
    }
    for (int i = numOther; i < numOther + 4; i++)
    {
        otherList[i].m_min[0] = 0xFFFFFFFF;
    }

    hkPadSpu<int> numSkipped;
    numSkipped = 0;
    hkKeyPair* pairs = reinterpret_cast<hkKeyPair*>(shapeKeyPairs);
    const int maxPairs = 0x7FF; // 2047 pairs and the end pair fill the 0x4000-byte buffer
    int numPairs;
    if (treeBody == bodyA)
    {
        numPairs = hk1AxisSweep::collide(treeList, numTree, otherList, numOther, pairs, maxPairs, numSkipped);
    }
    else
    {
        numPairs = hk1AxisSweep::collide(otherList, numOther, treeList, numTree, pairs, maxPairs, numSkipped);
    }

    stack.fastBlockFree(otherList, otherBytes);
    stack.fastBlockFree(treeList, treeBytes);

    if (numPairs > 1)
    {
        hkAlgorithm::quickSortRecursive(shapeKeyPairs, 0, numPairs - 1, hkAlgorithm::less<hkpShapeKeyPair>());
    }
    shapeKeyPairs[numPairs].m_shapeKeyA = 0xFFFFFFFF;
    shapeKeyPairs[numPairs].m_shapeKeyB = 0xFFFFFFFF;
    return numPairs;
}

// 0x00DB8DD0
void* hkpCollectionCollectionAgent3::process(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkVector4f* separatingNormal, hkpProcessCollisionOutput& output)
{
    const hkpShapeContainer* shapeContainerA = input.m_bodyA.val()->getShape()->getContainer();
    const hkpShapeContainer* shapeContainerB = input.m_bodyB.val()->getShape()->getContainer();
    hkLifoAllocator& stack = hkMemoryRouter::getInstance().stack();
    const int pairBytes = 0x4000; // 2048 key pairs
    hkpShapeKeyPair* shapeKeyPairs = static_cast<hkpShapeKeyPair*>(stack.fastBlockAlloc(pairBytes));
    const int numPairs = process_gatherShapeKeys(input, shapeContainerA, shapeContainerB, shapeKeyPairs);
    if (numPairs >= 0)
    {
        if (input.m_overrideBodyA.val() == nullptr)
        {
            hkAgentNmMachine_Process(hkCollectionCollectionAgent3_track(agentData), input, shapeContainerA, shapeContainerB, shapeKeyPairs, output);
        }
        else
        {
            // the B keys, the end key included, against the override body
            const int keyBytes = ((numPairs + 1) * (int)sizeof(unsigned int) + 127) & ~127; // hkAllocateStack: 128-byte multiples
            unsigned int* newKeys = static_cast<unsigned int*>(stack.fastBlockAlloc(keyBytes));
            for (int i = 0; i <= numPairs; i++)
            {
                newKeys[i] = shapeKeyPairs[i].m_shapeKeyB;
            }
            hkpAgent3ProcessInput in2 = input;
            in2.m_bodyA = input.m_overrideBodyA.val();
            in2.m_overrideBodyA = nullptr;
            hkAgent1nMachine_Process(hkCollectionCollectionAgent3_track(agentData), in2, shapeContainerB, newKeys, output);
            stack.fastBlockFree(newKeys, keyBytes);
        }
    }
    stack.fastBlockFree(shapeKeyPairs, pairBytes);
    return static_cast<char*>(agentData) + 0x10;
}

// 0x00DB7A70
void hkpCollectionCollectionAgent3::updateFilter(hkpAgentEntry* entry, void* agentData, const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner)
{
    hkpAgent1nMachine_VisitorInput vin;
    vin.m_bodyA = &bodyA;
    vin.m_input = &input;
    vin.m_collectionBodyB = &bodyB;
    vin.m_constraintOwner = &constraintOwner;
    vin.m_contactMgr = mgr;
    vin.m_containerShapeA = bodyA.getShape()->getContainer();
    vin.m_containerShapeB = bodyB.getShape()->getContainer();
    hkAgentNmMachine_UpdateShapeCollectionFilter(hkCollectionCollectionAgent3_track(agentData), vin);
}

// 0x00DB7A10
void hkpCollectionCollectionAgent3::invalidateTim(hkpAgentEntry* entry, void* agentData, const hkpCollisionInput& input)
{
    hkAgent1nMachine_InvalidateTim(hkCollectionCollectionAgent3_track(agentData), input);
}

// 0x00DB7A30
void hkpCollectionCollectionAgent3::warpTime(hkpAgentEntry* entry, void* agentData, float oldTime, float newTime, const hkpCollisionInput& input)
{
    hkAgent1nMachine_WarpTime(hkCollectionCollectionAgent3_track(agentData), oldTime, newTime, input);
}

// 0x00DB9100
void hkpCollectionCollectionAgent3::initAgentFunc(hkpCollisionDispatcher::Agent3Funcs& f)
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

// 0x00DB9150: BV tree vs BV tree, then the collection pairs sharing its entry
void hkpCollectionCollectionAgent3::registerAgent3(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::Agent3Funcs f;
    initAgentFunc(f);
    dispatcher->registerAgent3(f, hkcdShapeType::BV_TREE, hkcdShapeType::BV_TREE);
    f.m_ignoreSymmetricVersion = true;
    f.m_reusePreviousEntry = true;
    dispatcher->registerAgent3(f, hkcdShapeType::COLLECTION, hkcdShapeType::BV_TREE);
    dispatcher->registerAgent3(f, hkcdShapeType::BV_TREE, hkcdShapeType::COLLECTION);
    dispatcher->registerAgent3(f, hkcdShapeType::COLLECTION, hkcdShapeType::COLLECTION);
    dispatcher->m_midphaseAgent3Registered = true;
    g_agentRegistered = true;
}
