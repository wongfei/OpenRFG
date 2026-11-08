// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalarray.h
//
// Havok 2014.2.5 (hand-written from the PDB instantiations, havok_impl.md HK0): hkLocalArray, an hkArray whose initial
// storage comes from the LIFO (stack) allocator. Inline bodies since HK3, from the copies inlined in
// hkpConvexVerticesShape::sortPlanes (0x00D98920); hand-maintained (nobuild in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/lifo/hklifoallocator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"

// size 0x14
template<typename T>
class hkLocalArray : public hkArray<T, hkContainerHeapAllocator>
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkLocalArray);

    // <hkpEntity *> 0x00E00D50: a LIFO block (none for a zero capacity) that the array does not own
    hkLocalArray(int capacity)
        : m_initialCapacity(capacity)
    {
        m_localMemory = capacity ? static_cast<T*>(hkMemoryRouter::getInstance().stack().fastBlockAlloc(getStackBytes(capacity))) : nullptr;
        this->m_data = m_localMemory;
        this->m_capacityAndFlags = capacity | hkArrayBase<T>::DONT_DEALLOCATE_FLAG;
    }

    void assertNotResized() {}

    // <hkpEntity *> 0x00DFF6C0
    hkBool wasReallocated() const { return this->m_data != m_localMemory; }

    // <hkpEntity *> 0x00E00DC0: the LIFO block goes back (also when the array grew onto the heap; ~hkArray then frees
    // the heap buffer)
    ~hkLocalArray()
    {
        if (m_localMemory == this->m_data)
        {
            this->m_size = 0;
        }
        hkMemoryRouter::getInstance().stack().fastBlockFree(m_localMemory, getStackBytes(m_initialCapacity));
    }

    T* m_localMemory; // 0x0C
    int m_initialCapacity; // 0x10

private:
    // hkAllocateStack/hkDeallocateStack round the block up to 128 bytes
    static int getStackBytes(int capacity) { return (capacity * (int)sizeof(T) + 127) & ~127; }
};
