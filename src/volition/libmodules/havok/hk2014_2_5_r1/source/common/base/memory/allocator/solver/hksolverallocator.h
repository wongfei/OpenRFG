// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/solver/hksolverallocator.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/hkmemoryallocator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/thread/criticalsection/hkcriticalsection.h"

// size 0x23C
class hkSolverAllocator : public hkMemoryAllocator
{
public:
    // size 0x8
    struct Element
    {
        HK_DECLARE_POD_TYPE();
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Element);
        char* getEnd(); // 0x00D0FE50

        char* m_start; // 0x00
        int m_size; // 0x04
    };

    HK_DECLARE_PLACEMENT_ALLOCATOR(); // delete(void*, unsigned int) 0x00D103B0
    hkSolverAllocator(); // 0x00D0FEF0
    void setBuffer(void* vbuffer, int bufferSize); // 0x00D10270
    virtual ~hkSolverAllocator(); // vtbl+0x00 0x00D0FF30
    virtual void* blockAlloc(int numBytes); // vtbl+0x04 0x00D102C0
    virtual void blockFree(void* p, int numBytes); // vtbl+0x08 0x00D0FE60
    virtual void* bufAlloc(int& reqNumInOut); // vtbl+0x0C 0x00D102F0
    virtual void bufFree(void* data, int size); // vtbl+0x10 0x00D100B0
    bool canAllocSingleBlock(int numBytes); // 0x00D0FE90
    int getBufferSize() const; // 0x00CF3990
    virtual void getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& u) const; // vtbl+0x20 0x00D0FEB0
    virtual int getAllocatedSize(const void* obj, int nbytes) const; // vtbl+0x24 0x00D103C0
    virtual void resetPeakMemoryStatistics(); // vtbl+0x28 0x00D0FEE0
public:
    void* allocate(int& reqNumInOut, bool useExactSize); // 0x00D0FF50

public:
    char* m_bufferStart; // 0x04
    char* m_bufferEnd; // 0x08
    char* m_currentEnd; // 0x0C
    int m_numAllocatedBlocks; // 0x10
    unsigned int m_peakUse; // 0x14
    hkArrayBase<Element> m_freeElems; // 0x18
    Element m_elemsBuf[64]; // 0x24
    hkCriticalSection m_criticalSection; // 0x224
};
