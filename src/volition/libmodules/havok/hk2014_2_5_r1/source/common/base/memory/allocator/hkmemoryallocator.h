// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/hkmemoryallocator.h
//
// Hand-written (Havok 2014.2.5, havok_impl.md HK1): the allocator interface (hkArray storage, Havok object memory).
// Implementation: hkmemoryallocator.cpp.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkMemorySnapshot;
struct hkResult;

// size 0x4
class hkMemoryAllocator
{
public:
    enum MemoryState
    {
        MEMORY_STATE_OK = 0,
        MEMORY_STATE_OUT_OF_MEMORY = 1,
    };

    typedef void (*MemoryWalkCallback)(void*, unsigned int, bool, int, void*);

    // size 0x18
    struct MemoryStatistics
    {
        MemoryStatistics(); // 0x00CF3910

        long m_allocated; // 0x00
        long m_inUse; // 0x04
        long m_peakInUse; // 0x08
        long m_available; // 0x0C
        long m_totalAvailable; // 0x10
        long m_largestBlock; // 0x14
    };

    struct ExtendedInterface;

    HK_DECLARE_PLACEMENT_ALLOCATOR(); // deleting destructor 0x00CE6D10 does not free
    virtual ~hkMemoryAllocator(); // vtbl+0x00 0x00CF11E0
    virtual void* blockAlloc(int numBytes) = 0; // vtbl+0x04
    virtual void blockFree(void* p, int numBytes) = 0; // vtbl+0x08
    virtual void* bufAlloc(int& reqBytesInOut); // vtbl+0x0C 0x00CF11F0
    virtual void bufFree(void* p, int numBytes); // vtbl+0x10 0x00CF1210
    virtual void* bufRealloc(void* pold, int oldNumBytes, int& reqBytesInOut); // vtbl+0x14 0x00CF12B0
    virtual void blockAllocBatch(void** ptrsOut, int numPtrs, int blockSize); // vtbl+0x18 0x00CF1220
    virtual void blockFreeBatch(void** ptrsIn, int numPtrs, int blockSize); // vtbl+0x1C 0x00CF1260
    virtual void getMemoryStatistics(MemoryStatistics& u) const = 0; // vtbl+0x20
    virtual int getAllocatedSize(const void* obj, int nbytes) const = 0; // vtbl+0x24
    virtual void resetPeakMemoryStatistics(); // vtbl+0x28 0x00CE6CF0
    virtual ExtendedInterface* getExtendedInterface(); // vtbl+0x2C 0x00CE6D00
    hkMemoryAllocator(); // 0x00CE6CD0
};

// size 0x4
// The interface of allocators that manage their own memory (garbage collection, soft limits).
struct hkMemoryAllocator::ExtendedInterface
{
    virtual ~ExtendedInterface(); // vtbl+0x00 0x00CF4830
    virtual void garbageCollect() = 0; // vtbl+0x04
    virtual void incrementalGarbageCollect(int numBlocks) = 0; // vtbl+0x08
    virtual hkResult setMemorySoftLimit(unsigned int maxMemory) = 0; // vtbl+0x0C
    virtual unsigned int getMemorySoftLimit() const = 0; // vtbl+0x10
    virtual bool canAllocTotal(int numBytes) = 0; // vtbl+0x14
    virtual hkResult walkMemory(void (*callback)(void*, unsigned int, bool, int, void*), void* param) = 0; // vtbl+0x18
    virtual unsigned int getApproxTotalAllocated() const = 0; // vtbl+0x1C
    virtual void setScrubValues(unsigned int allocValue, unsigned int freeValue) = 0; // vtbl+0x20
    virtual int addToSnapshot(hkMemorySnapshot& snap, int parentId) = 0; // vtbl+0x24
    ExtendedInterface(); // 0x00CF4A60
};
