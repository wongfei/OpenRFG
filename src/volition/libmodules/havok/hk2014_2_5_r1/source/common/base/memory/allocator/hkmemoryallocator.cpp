// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/hkmemoryallocator.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): the allocator interface. The buffer functions forward to the block functions;
// bufRealloc allocates, copies the smaller size and frees.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/hkmemoryallocator.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/util/hkmemutil.h"

// 0x00CF11D0 (:13)
// HK_ASSERT_OBJECT_SIZE_OK: an empty function in the shipped build.
void HK_ASSERT_OBJECT_SIZE_OK_FUNC(unsigned int nbytes)
{
}

// 0x00CE6CD0
hkMemoryAllocator::hkMemoryAllocator()
{
}

// 0x00CF11E0 (:19)
hkMemoryAllocator::~hkMemoryAllocator()
{
}

// 0x00CF11F0 (:23)
void* hkMemoryAllocator::bufAlloc(int& reqBytesInOut)
{
    return blockAlloc(reqBytesInOut);
}

// 0x00CF1210 (:27)
void hkMemoryAllocator::bufFree(void* p, int numBytes)
{
    blockFree(p, numBytes);
}

// 0x00CF12B0 (:31)
void* hkMemoryAllocator::bufRealloc(void* pold, int oldNumBytes, int& reqBytesInOut)
{
    void* p = bufAlloc(reqBytesInOut);
    if (p != nullptr)
    {
        hkMemUtil::memCpy(p, pold, oldNumBytes < reqBytesInOut ? oldNumBytes : reqBytesInOut);
    }
    bufFree(pold, oldNumBytes);
    return p;
}

// 0x00CF1220 (:42)
void hkMemoryAllocator::blockAllocBatch(void** ptrsOut, int numPtrs, int blockSize)
{
    for (int i = 0; i < numPtrs; i++)
    {
        ptrsOut[i] = blockAlloc(blockSize);
    }
}

// 0x00CF1260 (:49)
void hkMemoryAllocator::blockFreeBatch(void** ptrsIn, int numPtrs, int blockSize)
{
    for (int i = 0; i < numPtrs; i++)
    {
        if (ptrsIn[i] != nullptr)
        {
            blockFree(ptrsIn[i], blockSize);
        }
    }
}

// 0x00CE6CF0 (hkmemoryallocator.h:179)
void hkMemoryAllocator::resetPeakMemoryStatistics()
{
}

// 0x00CE6D00 (hkmemoryallocator.h:182)
hkMemoryAllocator::ExtendedInterface* hkMemoryAllocator::getExtendedInterface()
{
    return nullptr;
}

// 0x00CF3910
// Every statistic starts unknown (-1); allocators fill what they track.
hkMemoryAllocator::MemoryStatistics::MemoryStatistics()
    : m_allocated(-1)
    , m_inUse(-1)
    , m_peakInUse(-1)
    , m_available(-1)
    , m_totalAvailable(-1)
    , m_largestBlock(-1)
{
}

// 0x00CF4A60
hkMemoryAllocator::ExtendedInterface::ExtendedInterface()
{
}

// 0x00CF4830
hkMemoryAllocator::ExtendedInterface::~ExtendedInterface()
{
}
