// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/lifo/hklifoallocator.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/hkmemoryallocator.h"

// size 0x28
class hkLifoAllocator : public hkMemoryAllocator
{
public:
    // size 0x1C
    class Implementation
    {
    public:
        // size 0xC
        struct NonLifoFree
        {
            void* start; // 0x00
            void* end; // 0x04
            int slabIndex; // 0x08
        };

        HK_DECLARE_PLACEMENT_ALLOCATOR(); // new(unsigned int, void* p) 0x00CEBA60, delete(void*, unsigned int) 0x00CEBA70
        Implementation(); // 0x00CEC2E0
        ~Implementation(); // 0x00CEC340

        hkArrayBase<NonLifoFree> m_nonLifoFrees; // 0x00
        hkArrayBase<void*> m_slabPtrs; // 0x0C
        int m_numExternalAllocations; // 0x18
    };

    HK_DECLARE_PLACEMENT_ALLOCATOR(); // delete(void*, unsigned int) 0x00CEC300
    hkLifoAllocator(); // 0x00CEBAA0
    void init(hkMemoryAllocator* slabAllocator, hkMemoryAllocator* largeAllocator, hkMemoryAllocator* internalAllocator, int slabSize); // 0x00CEBAB0
    void quit(hkMemoryAllocator** allocators); // 0x00CEBCA0
    virtual void* blockAlloc(int numBytesIn); // vtbl+0x04 0x00CEC000
    void* fastBlockAlloc(int numBytesIn); // 0x0080C220 (hklifoallocator.inl:10)
    virtual void blockFree(void* p, int numBytesIn); // vtbl+0x08 0x00CEC030
    void fastBlockFree(void* p, int numBytesIn); // 0x0080C250 (hklifoallocator.inl:28)
    virtual void* bufAlloc(int& reqNumBytesInOut); // vtbl+0x0C 0x00CEC070
    virtual void bufFree(void* p, int numBytes); // vtbl+0x10 0x00CEC0B0
    virtual void* bufRealloc(void* pold, int oldNumBytes, int& reqNumBytesInOut); // vtbl+0x14 0x00CEBB20
    virtual void getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& u) const; // vtbl+0x20 0x00CEC310
    virtual int getAllocatedSize(const void* obj, int nbytes) const; // vtbl+0x24 0x00CEC320
    bool isEmpty() const; // 0x00CEBBA0
    int numExternalAllocations() const; // 0x00CEBA80
public:
    void* allocateFromNewSlab(int numBytes); // 0x00CEBD80
    void popNonLifoFrees(); // 0x00CEBBC0
    void slowBlockFree(void* p, int numBytesIn); // 0x00CEBFB0
    void insertNonLifoFree(void* pstart, int nbytes); // 0x00CEBE70
public:
    virtual ~hkLifoAllocator(); // vtbl+0x00 0x00811AC0

public:
    Implementation* m_impl; // 0x04
    int m_slabSize; // 0x08
    void* m_cur; // 0x0C
    void* m_end; // 0x10
    void* m_firstNonLifoEnd; // 0x14
    void* m_cachedEmptySlab; // 0x18
    hkMemoryAllocator* m_slabAllocator; // 0x1C
    hkMemoryAllocator* m_largeAllocator; // 0x20
    hkMemoryAllocator* m_internalAllocator; // 0x24
};
