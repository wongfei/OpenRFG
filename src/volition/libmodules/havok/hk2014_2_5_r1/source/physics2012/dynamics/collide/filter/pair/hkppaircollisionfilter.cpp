// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/filter/pair/hkppaircollisionfilter.cpp
//
// Havok 2014.2.5 (havok_impl.md HK4): a filter that disables collisions between pairs of entities (with a count per
// pair) on top of an optional child filter, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/filter/pair/hkppaircollisionfilter.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkpagentnnmachine.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldagentutil.h"

#include <stdint.h>

// ---- map operations (the key is the pair ordered by address; a free slot has null pointers)

// 0x00DEE340: the 64-bit (a << 28) + b, its bits 4..35 times the golden ratio constant.
unsigned int hkpPairCollisionFilter::PairFilterPointerMapOperations::hash(PairFilterKey key, unsigned int modulus)
{
    const uint64_t value = ((uint64_t)(uintptr_t)key.m_a << 28) + (uint64_t)(uintptr_t)key.m_b;
    return (unsigned int)(value >> 4) * 0x9E3779B1u & modulus;
}

// 0x00DEE370
void hkpPairCollisionFilter::PairFilterPointerMapOperations::invalidate(PairFilterKey& key)
{
    key.m_a = nullptr;
    key.m_b = nullptr;
}

// 0x00DEE390
unsigned int hkpPairCollisionFilter::PairFilterPointerMapOperations::isValid(PairFilterKey key)
{
    return key.m_a != nullptr && key.m_b != nullptr;
}

// 0x00DEE3B0
unsigned int hkpPairCollisionFilter::PairFilterPointerMapOperations::equal(PairFilterKey key0, PairFilterKey key1)
{
    return key0.m_a == key1.m_a && key0.m_b == key1.m_b;
}

// ---- filter

// 0x00DEE770 (stub_impl.md S4 SD): the disabled pairs are not serialized (an empty map)
hkpPairCollisionFilter::hkpPairCollisionFilter(hkFinishLoadedObjectFlag flag)
    : hkpCollisionFilter(flag)
{
}

// 0x00E0ED90
hkpPairCollisionFilter::hkpPairCollisionFilter(const hkpCollisionFilter* childFilter)
{
    m_childFilter = childFilter;
    m_type.m_storage = HK_FILTER_PAIR;
    if (childFilter != nullptr)
    {
        childFilter->addReference();
    }
}

// 0x00E23A10
hkpPairCollisionFilter::~hkpPairCollisionFilter()
{
    if (m_childFilter != nullptr)
    {
        m_childFilter->removeReference();
    }
    m_disabledPairs.clearAndDeallocate();
}

// 0x00E23890 (inlined): the pair ordered by address.
void hkpPairCollisionFilter::calcKey(const hkpEntity* entityA, const hkpEntity* entityB, PairFilterKey& keyOut) const
{
    if (entityA < entityB)
    {
        keyOut.m_a = const_cast<hkpEntity*>(entityA);
        keyOut.m_b = const_cast<hkpEntity*>(entityB);
    }
    else
    {
        keyOut.m_a = const_cast<hkpEntity*>(entityB);
        keyOut.m_b = const_cast<hkpEntity*>(entityA);
    }
}

// 0x00E23C70
// Counts one more disable of the pair; the first removes the pair's agent (inside a critical section of the world
// operations). Returns the new count.
int hkpPairCollisionFilter::disableCollisionsBetween(const hkpEntity* entityA, const hkpEntity* entityB)
{
    PairFilterKey key;
    calcKey(entityA, entityB, key);
    const uint64_t count = m_disabledPairs.getWithDefault(key, 0) + 1;
    m_disabledPairs.insert(key, count);
    if (count == 1)
    {
        hkpAgentNnEntry* agent = hkAgentNnMachine_FindAgent(&entityA->m_collidable, &entityB->m_collidable);
        if (agent != nullptr)
        {
            hkpWorld* world = entityA->m_world;
            if (world == nullptr)
            {
                return 1;
            }
            world->m_criticalOperationsLockCount++;
            hkpWorldAgentUtil::removeAgentAndItsToiEvents(agent);
            if (--world->m_criticalOperationsLockCount == 0 && !world->m_blockExecutingPendingOperations.m_bool)
            {
                if (world->m_pendingOperationsCount != 0)
                {
                    world->internal_executePendingOperations();
                }
                if (world->m_pendingOperationQueueCount == 1 && world->m_pendingBodyOperationsCount != 0)
                {
                    world->internal_executePendingBodyOperations();
                }
            }
        }
    }
    return (int)count;
}

// 0x00E23B50
// Counts one enable of a disabled pair; when the count reaches 0 and the child filter allows the pair, the world
// creates its agent again. Returns the new count (0 for a pair that was not disabled).
int hkpPairCollisionFilter::enableCollisionsBetween(const hkpEntity* entityA, const hkpEntity* entityB)
{
    PairFilterKey key;
    calcKey(entityA, entityB, key);
    const uint64_t previous = m_disabledPairs.getWithDefault(key, 0);
    if (previous == 0)
    {
        return 0;
    }
    const uint64_t count = previous - 1;
    if (count == 0)
    {
        m_disabledPairs.remove(key);
    }
    else
    {
        m_disabledPairs.insert(key, count);
    }
    if (count == 0 && (m_childFilter == nullptr || m_childFilter->isCollisionEnabled(entityA->m_collidable, entityB->m_collidable).m_bool))
    {
        hkpWorld* world = entityA->m_world;
        if (world != nullptr)
        {
            world->m_criticalOperationsLockCount++;
            world->reenableCollisionBetweenEntityPair(const_cast<hkpEntity*>(entityA), const_cast<hkpEntity*>(entityB));
            if (--world->m_criticalOperationsLockCount == 0 && !world->m_blockExecutingPendingOperations.m_bool)
            {
                if (world->m_pendingOperationsCount != 0)
                {
                    world->internal_executePendingOperations();
                }
                if (world->m_pendingOperationQueueCount == 1 && world->m_pendingBodyOperationsCount != 0)
                {
                    world->internal_executePendingBodyOperations();
                }
            }
        }
    }
    return (int)count;
}

// 0x00E23A80
hkBool hkpPairCollisionFilter::isCollisionEnabled(const hkpEntity* entityA, const hkpEntity* entityB) const
{
    if (entityA != nullptr && entityB != nullptr)
    {
        PairFilterKey key;
        calcKey(entityA, entityB, key);
        if (m_disabledPairs.isValid(m_disabledPairs.findKey(key)))
        {
            return false;
        }
    }
    return true;
}

// 0x00E23AD0: the child filter, then the pair of the owning entities (other owners do not count).
hkBool hkpPairCollisionFilter::isCollisionEnabled(const hkpCollidable& collidableA, const hkpCollidable& collidableB) const
{
    if (m_childFilter != nullptr && !m_childFilter->isCollisionEnabled(collidableA, collidableB).m_bool)
    {
        return false;
    }
    const hkpEntity* entityA = collidableA.m_broadPhaseHandle.m_type == 1 ? (const hkpEntity*)collidableA.getOwner() : nullptr; // BROAD_PHASE_ENTITY
    const hkpEntity* entityB = collidableB.m_broadPhaseHandle.m_type == 1 ? (const hkpEntity*)collidableB.getOwner() : nullptr;
    return isCollisionEnabled(entityA, entityB);
}

// 0x00E238B0: only the child filter (the pairs are filtered per entity).
hkBool hkpPairCollisionFilter::isCollisionEnabled(const hkpCollisionInput& input, const hkpCdBody& a, const hkpCdBody& b, const hkpShapeContainer& bContainer, unsigned int bKey) const
{
    if (m_childFilter != nullptr && !m_childFilter->isCollisionEnabled(input, a, b, bContainer, bKey).m_bool)
    {
        return false;
    }
    return true;
}

// 0x00E23900
hkBool hkpPairCollisionFilter::isCollisionEnabled(const hkpCollisionInput& input, const hkpCdBody& a, const hkpCdBody& b, const hkpShapeContainer& containerShapeA, const hkpShapeContainer& containerShapeB, unsigned int keyA, unsigned int keyB) const
{
    if (m_childFilter != nullptr && !m_childFilter->isCollisionEnabled(input, a, b, containerShapeA, containerShapeB, keyA, keyB).m_bool)
    {
        return false;
    }
    return true;
}

// 0x00E23960
hkBool hkpPairCollisionFilter::isCollisionEnabled(const hkpShapeRayCastInput& aInput, const hkpShapeContainer& bContainer, unsigned int bKey) const
{
    if (m_childFilter != nullptr && !m_childFilter->isCollisionEnabled(aInput, bContainer, bKey).m_bool)
    {
        return false;
    }
    return true;
}

// 0x00E239B0
hkBool hkpPairCollisionFilter::isCollisionEnabled(const hkpWorldRayCastInput& a, const hkpCollidable& collidableB) const
{
    if (m_childFilter != nullptr && !m_childFilter->isCollisionEnabled(a, collidableB).m_bool)
    {
        return false;
    }
    return true;
}
