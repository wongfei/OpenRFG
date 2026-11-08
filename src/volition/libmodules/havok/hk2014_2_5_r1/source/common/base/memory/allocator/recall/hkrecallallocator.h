// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/recall/hkrecallallocator.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/hkmemoryallocator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/thread/criticalsection/hkcriticalsection.h"

// size 0x3C
class hkRecallAllocator : public hkMemoryAllocator
{
public:
    // size 0xC
    struct Header
    {
        int getRequestedSize() const; // 0x00CF39A0
        int getPayloadSize() const; // 0x00CF39B0
        void* getPayload(); // 0x00D116F0
        const void* getPayload() const; // 0x00CF46E0
        const Header* getNext() const; // 0x00CF39C0

    public:
        Header* m_next; // 0x00
        int m_requestedSize; // 0x04
        int m_payloadSize; // 0x08
    };

    hkRecallAllocator(hkMemoryAllocator* a); // 0x00D11590
    void init(hkMemoryAllocator* a); // 0x00D11510
    virtual ~hkRecallAllocator(); // vtbl+0x00 0x00CF3A10
    virtual void* blockAlloc(int numBytes); // vtbl+0x04 0x00D115D0
    virtual void blockFree(void* p, int numBytes); // vtbl+0x08 0x00D11640
    virtual void getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& u) const; // vtbl+0x20 0x00D11520
    virtual int getAllocatedSize(const void* obj, int numBytes) const; // vtbl+0x24 0x00D11570
    const Header* getHead() const; // 0x00CF39D0
    virtual void resetPeakMemoryStatistics(); // vtbl+0x28 0x00D11560

public:
    hkCriticalSection m_criticalSection; // 0x04
    hkMemoryAllocator* m_alloc; // 0x1C
    hkMemoryAllocator::MemoryStatistics m_stats; // 0x20
    Header* m_head; // 0x38
};
