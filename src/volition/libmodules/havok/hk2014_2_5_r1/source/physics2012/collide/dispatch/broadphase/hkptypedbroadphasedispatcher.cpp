// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasedispatcher.cpp
//
// Havok 2014.2.5 (havok_impl.md HK4): routes broadphase pairs to a listener chosen by the two handles' types
// (entity, phantom, border, ...), and cancels pairs that were both added and removed in one update, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasedispatcher.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasehandlepair.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollidablecollidablefilter.h"

#include <stdint.h>

// 0x0080CC60 (hkptypedbroadphasehandle.inl:44): the handle's owner (its collidable) at m_ownerOffset bytes.
void* hkpTypedBroadPhaseHandle::getOwner() const
{
    return (char*)this + m_ownerOffset;
}

// 0x00D9A6C0: every listener starts as the null listener.
hkpTypedBroadPhaseDispatcher::hkpTypedBroadPhaseDispatcher()
{
    for (int a = 0; a < 8; a++)
    {
        for (int b = 0; b < 8; b++)
        {
            m_broadPhaseListeners[a][b] = &m_nullBroadPhaseListener;
        }
    }
}

// 0x00D9A790
hkpTypedBroadPhaseDispatcher::~hkpTypedBroadPhaseDispatcher()
{
}

// 0x00D9A7B0: the pairs the filter enables go to the listener of the two types.
void hkpTypedBroadPhaseDispatcher::addPairs(hkpTypedBroadPhaseHandlePair* newPairs, int numNewPairs, const hkpCollidableCollidableFilter* filter) const
{
    for (int i = numNewPairs - 1; i >= 0; i--, newPairs++)
    {
        const hkpTypedBroadPhaseHandle* a = newPairs->getElementA();
        const hkpTypedBroadPhaseHandle* b = newPairs->getElementB();
        if (filter->isCollisionEnabled(*(const hkpCollidable*)a->getOwner(), *(const hkpCollidable*)b->getOwner()).m_bool)
        {
            m_broadPhaseListeners[a->m_type][b->m_type]->addCollisionPair(*newPairs);
        }
    }
}

// 0x00D9A820
void hkpTypedBroadPhaseDispatcher::removePairs(hkpTypedBroadPhaseHandlePair* deletedPairs, int numDeletedPairs) const
{
    for (int i = numDeletedPairs - 1; i >= 0; i--, deletedPairs++)
    {
        m_broadPhaseListeners[deletedPairs->getElementA()->m_type][deletedPairs->getElementB()->m_type]->removeCollisionPair(*deletedPairs);
    }
}

// 0x00D9A860
// A pair in both lists (either order) is dropped from both: up to 31 pairs in the smaller list by search (a found
// new pair is removed keeping the order), else through a map from the pair (lower handle in the low dword) to
// (index in newPairs << 8 | count), whose storage comes from the temp allocator.
void hkpTypedBroadPhaseDispatcher::removeDuplicates(hkArray<hkpBroadPhaseHandlePair, hkContainerHeapAllocator>& newPairs, hkArray<hkpBroadPhaseHandlePair, hkContainerHeapAllocator>& delPairs)
{
    const int smaller = newPairs.m_size < delPairs.m_size ? newPairs.m_size : delPairs.m_size;
    if (smaller < 32)
    {
        int numKept = 0;
        for (int i = 0; i < delPairs.m_size; i++)
        {
            const hkpBroadPhaseHandlePair& del = delPairs.m_data[i];
            int j = 0;
            for (; j < newPairs.m_size; j++)
            {
                const hkpBroadPhaseHandlePair& added = newPairs.m_data[j];
                if ((added.m_a == del.m_a && added.m_b == del.m_b) || (added.m_b == del.m_a && added.m_a == del.m_b))
                {
                    break;
                }
            }
            if (j == newPairs.m_size)
            {
                if (numKept != i)
                {
                    delPairs.m_data[numKept] = delPairs.m_data[i];
                }
                numKept++;
            }
            else
            {
                newPairs.removeAtAndCopy(j);
            }
        }
        delPairs.setSize(numKept);
        return;
    }

    typedef hkMapBase<uint64_t, uint64_t> PairMap;
    const int numBytes = PairMap::getSizeInBytesFor(newPairs.m_size);
    hkArray<char, hkContainerTempAllocator> storage(numBytes);
    PairMap map(storage.m_data, numBytes);
    for (int i = 0; i < newPairs.m_size; i++)
    {
        hkpBroadPhaseHandle* a = newPairs.m_data[i].m_a;
        hkpBroadPhaseHandle* b = newPairs.m_data[i].m_b;
        if (a > b)
        {
            hkpBroadPhaseHandle* t = a;
            a = b;
            b = t;
        }
        const uint64_t key = (uint64_t)(uintptr_t)a | ((uint64_t)(uintptr_t)b << 32);
        const PairMap::Iterator it = map.findKey(key);
        if (map.isValid(it))
        {
            map.m_elem[it].val++; // a duplicate within newPairs: counted, the copy dropped
            newPairs.m_data[i].m_a = nullptr;
        }
        else
        {
            map.insert(hkContainerHeapAllocator::s_alloc, key, (uint64_t)(int64_t)(i << 8 | 1));
        }
    }
    int numKept = 0;
    for (int i = 0; i < delPairs.m_size; i++)
    {
        hkpBroadPhaseHandle* a = delPairs.m_data[i].m_a;
        hkpBroadPhaseHandle* b = delPairs.m_data[i].m_b;
        if (a > b)
        {
            hkpBroadPhaseHandle* t = a;
            a = b;
            b = t;
        }
        const uint64_t key = (uint64_t)(uintptr_t)a | ((uint64_t)(uintptr_t)b << 32);
        const PairMap::Iterator it = map.findKey(key);
        if (!map.isValid(it))
        {
            delPairs.m_data[numKept++] = delPairs.m_data[i];
            continue;
        }
        const uint64_t value = map.m_elem[it].val;
        if ((int)(value & 0xFF) <= 1)
        {
            map.remove(it);
            newPairs.m_data[(unsigned int)value >> 8].m_a = nullptr;
        }
        else
        {
            map.m_elem[it].val = value - 1;
        }
    }
    delPairs.setSize(numKept);
    numKept = 0;
    for (int i = 0; i < newPairs.m_size; i++)
    {
        if (newPairs.m_data[i].m_a != nullptr)
        {
            newPairs.m_data[numKept++] = newPairs.m_data[i];
        }
    }
    newPairs.setSize(numKept);
    map.clearAndDeallocate(hkContainerHeapAllocator::s_alloc);
}
