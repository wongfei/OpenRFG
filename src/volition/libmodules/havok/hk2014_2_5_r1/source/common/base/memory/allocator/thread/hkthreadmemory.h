// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/thread/hkthreadmemory.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/hkmemoryallocator.h"

// size 0x124
class hkThreadMemory : public hkMemoryAllocator
{
public:
    enum
    {
        PAGE_ALIGN = 64,
        MEMORY_SMALL_BLOCK_RSHIFT_BITS = 4,
        MEMORY_SMALL_BLOCK_ADD = 15,
        MEMORY_MAX_SMALL_ROW = 13,
        MEMORY_MAX_ALL_ROW = 17,
        MEMORY_MAX_SIZE_SMALL_BLOCK = 640,
        MEMORY_MAX_SIZE_LARGE_BLOCK = 8192,
        MEMORY_LARGE_BLOCK_RSHIFT_BITS = 10,
        MEMORY_SYSTEM_PAGE_SIZE = 8192,
        MEMORY_MAGIC_NUMBER = 0x3425234,
    };

    enum
    {
        BATCH_SIZE = 4,
    };

    // size 0x8
    struct FreeList
    {
        // size 0x4
        struct FreeElem
        {
            FreeElem* m_next; // 0x00
        };

        FreeList(); // 0x00D0F4D0
        void put(void* p); // 0x00D0F4E0
        void* get(); // 0x00D0F4F0

        FreeElem* m_head; // 0x00
        int m_numElem; // 0x04
    };

    HK_DECLARE_PLACEMENT_ALLOCATOR(); // delete(void*, unsigned int) 0x00CF38E0
    virtual ~hkThreadMemory(); // vtbl+0x00 0x00CF38C0
    virtual void* blockAlloc(int nbytes); // vtbl+0x04 0x00D0FC20
    virtual void blockFree(void* p, int nbytes); // vtbl+0x08 0x00D0FC80
    virtual void* bufAlloc(int& reqNumInOut); // vtbl+0x0C 0x00D0F620
    virtual void bufFree(void* p, int numElem); // vtbl+0x10 0x00D0F640
    virtual void* bufRealloc(void* pold, int oldNum, int& reqNumInOut); // vtbl+0x14 0x00D0FD50
    virtual void blockAllocBatch(void** ptrsOut, int numPtrs, int blockSize); // vtbl+0x18 0x00D0F650
    hkThreadMemory(hkMemoryAllocator* memoryInstance); // 0x00D0FDE0
    hkThreadMemory(); // 0x00D0FDA0
    void setMemory(hkMemoryAllocator* memoryInstance, int maxNumElemsOnFreeList); // 0x00D0FA20
    virtual void blockFreeBatch(void** ptrsIn, int numPtrs, int blockSize); // vtbl+0x1C 0x00D0F6B0
    virtual void getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& u) const; // vtbl+0x20 0x00D0F710
    unsigned int getCachedSizeUnchecked(); // 0x00D0F9D0
    virtual int getAllocatedSize(const void* obj, int numBytes) const; // vtbl+0x24 0x00D0FCF0
    virtual void releaseCachedMemory(); // vtbl+0x30 0x00D0FE20
    int getRow(int nbytes) const; // 0x00D0F510
    static int constSizeToRow(int size); // 0x00D0F550
    int rowToSize(int row) const; // 0x00D0F540
public:
    void* onRowEmpty(int rowIndex); // 0x00D0FA40
    void onRowFull(int rowIndex, void* p); // 0x00D0FAD0
    void clearRow(int rowIndex); // 0x00D0FBA0
    void initLookupTables(); // 0x00D0F780

public:
    hkMemoryAllocator* m_memory; // 0x04
    int m_maxNumElemsOnFreeList; // 0x08
    FreeList m_free_list[17]; // 0x0C
    int m_row_to_size_lut[17]; // 0x94
    char m_small_size_to_row_lut[41]; // 0xD8
    int m_large_size_to_row_lut[8]; // 0x104
};
