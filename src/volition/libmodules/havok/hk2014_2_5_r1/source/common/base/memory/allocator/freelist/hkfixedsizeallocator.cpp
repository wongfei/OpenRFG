// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/freelist/hkfixedsizeallocator.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): an allocator of one size over a free list (the 16-byte block headers of
// hkFreeListAllocator's free lists). Every request gets one element, whatever its size; bufRealloc keeps the block.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/freelist/hkfixedsizeallocator.h"

// 0x00CF4A70
hkFixedSizeAllocator::hkFixedSizeAllocator()
{
}

// 0x00CF4AA0
// The free list's destructor gives its blocks back.
hkFixedSizeAllocator::~hkFixedSizeAllocator()
{
}

// 0x00D12A90 (:15)
void hkFixedSizeAllocator::init(unsigned int blockSize, unsigned int align, unsigned int allocatorBlockSize, hkMemoryAllocator* allocator)
{
    m_freeList.init(blockSize, align, allocatorBlockSize, allocator, nullptr);
}

// 0x00D12AC0 (:24)
void* hkFixedSizeAllocator::blockAlloc(int numBytes)
{
    return m_freeList.alloc();
}

// 0x00D12B00 (:30)
void hkFixedSizeAllocator::blockFree(void* p, int numBytes)
{
    m_freeList.free(p);
}

// 0x00D12B20 (:36)
void* hkFixedSizeAllocator::bufAlloc(int& reqNumBytesInOut)
{
    return m_freeList.alloc();
}

// 0x00D12B60 (:42)
void hkFixedSizeAllocator::bufFree(void* p, int numBytes)
{
    m_freeList.free(p);
}

// 0x00D12B80 (:48)
void* hkFixedSizeAllocator::bufRealloc(void* pold, int oldNumBytes, int& reqNumBytesInOut)
{
    return pold;
}

// 0x00D12B90 (:54)
void hkFixedSizeAllocator::blockAllocBatch(void** ptrsOut, int numPtrs, int blockSize)
{
    m_freeList.allocBatch(ptrsOut, numPtrs);
}

// 0x00D12C20 (:60)
void hkFixedSizeAllocator::blockFreeBatch(void** ptrsIn, int numPtrs, int blockSize)
{
    m_freeList.freeBatch(ptrsIn, numPtrs);
}

// 0x00D12C60 (:66)
void hkFixedSizeAllocator::getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& u) const
{
    m_freeList.getMemoryStatistics(u);
}

// 0x00D12C70 (:71)
int hkFixedSizeAllocator::getAllocatedSize(const void* obj, int nbytes) const
{
    return m_freeList.m_elementSize;
}
