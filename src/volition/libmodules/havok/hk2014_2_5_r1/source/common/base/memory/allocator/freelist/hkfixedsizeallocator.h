// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/freelist/hkfixedsizeallocator.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/freelist/hkfreelist.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/hkmemoryallocator.h"

// size 0x3C
class hkFixedSizeAllocator : public hkMemoryAllocator
{
public:
    HK_DECLARE_PLACEMENT_ALLOCATOR(); // delete(void*, unsigned int) 0x00CF4A90
    virtual ~hkFixedSizeAllocator(); // vtbl+0x00 0x00CF4AA0
    virtual void* blockAlloc(int numBytes); // vtbl+0x04 0x00D12AC0
    virtual void blockFree(void* p, int numBytes); // vtbl+0x08 0x00D12B00
    virtual void* bufAlloc(int& reqNumBytesInOut); // vtbl+0x0C 0x00D12B20
    virtual void bufFree(void* p, int numBytes); // vtbl+0x10 0x00D12B60
    virtual void* bufRealloc(void* pold, int oldNumBytes, int& reqNumBytesInOut); // vtbl+0x14 0x00D12B80
    virtual void blockAllocBatch(void** ptrsOut, int numPtrs, int blockSize); // vtbl+0x18 0x00D12B90
    virtual void blockFreeBatch(void** ptrsIn, int numPtrs, int blockSize); // vtbl+0x1C 0x00D12C20
    virtual void getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& u) const; // vtbl+0x20 0x00D12C60
    void init(unsigned int blockSize, unsigned int align, unsigned int allocatorBlockSize, hkMemoryAllocator* allocator); // 0x00D12A90
    hkFixedSizeAllocator(); // 0x00CF4A70
    virtual int getAllocatedSize(const void* obj, int nbytes) const; // vtbl+0x24 0x00D12C70

    hkFreeList m_freeList; // 0x04
};
