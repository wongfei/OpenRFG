// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/freelist/hkfreelistallocator.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/freelist/hkfixedsizeallocator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/freelist/hkfreelist.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/hkmemoryallocator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/thread/criticalsection/hkcriticalsection.h"

class hkBool;
class hkMemorySnapshot;
struct hkResult;

// size 0xAC0
class hkFreeListAllocator : public hkMemoryAllocator, public hkMemoryAllocator::ExtendedInterface
{
public:
    enum
    {
        SOFT_LIMIT_MAX = 0x7FFFFFFF,
    };

    enum
    {
        FREELIST_ALIGNMENT = 16,
        MAX_FREELIST_SIZE = 640,
        FREELIST_SHIFT = 4,
        MAX_FREELISTS = 41,
        MAX_UNIQUE_FREELISTS = 41,
    };

    // size 0xC
    struct FreeListCinfo
    {
        unsigned int m_elementSize; // 0x00
        unsigned int m_alignment; // 0x04
        unsigned int m_blockSize; // 0x08
    };

    // size 0x1F4
    struct Cinfo
    {
        Cinfo(); // 0x00CF4A50
        void add(unsigned int elementSize, unsigned int alignment, unsigned int blockSize); // 0x00CF4BC0
        int findInfoIndex(unsigned int elementSize) const; // 0x00CF4B90
        void removeAt(int index); // 0x00CF4C00

        unsigned int m_preferedBlockSize; // 0x00
        int m_numInfos; // 0x04
        FreeListCinfo m_infos[41]; // 0x08
    };

    HK_DECLARE_PLACEMENT_ALLOCATOR(); // new(unsigned int, void* p) 0x0080DC90, delete(void*, unsigned int) 0x00CF5A70
    hkFreeListAllocator(hkMemoryAllocator* server, hkMemoryAllocator::ExtendedInterface* allocatorExtended, const Cinfo* info); // 0x00CF5850
    hkFreeListAllocator(); // 0x00CF4AE0
    virtual ~hkFreeListAllocator(); // vtbl+0x00 0x00CF5610
    void init(hkMemoryAllocator* allocator, hkMemoryAllocator::ExtendedInterface* allocatorExtended, const Cinfo* info); // 0x00CF58D0
    virtual void* blockAlloc(int nbytes); // vtbl+0x04 0x00CF4E40
    virtual void garbageCollect(); // vtbl+0x04 0x00CF4F80
    virtual void blockFree(void* p, int nbytes); // vtbl+0x08 0x00CF4E50
    virtual void incrementalGarbageCollect(int numBlocksIn); // vtbl+0x08 0x00CF5490
    virtual void* bufAlloc(int& reqNumInOut); // vtbl+0x0C 0x00CF5290
    virtual hkResult setMemorySoftLimit(unsigned int maxMemory); // vtbl+0x0C 0x00CF5030
    virtual unsigned int getMemorySoftLimit() const; // vtbl+0x10 0x00CF5060
    virtual bool canAllocTotal(int size); // vtbl+0x14 0x00CF5090
    virtual void blockAllocBatch(void** blocksOut, int nblocks, int nbytes); // vtbl+0x18 0x00CF5360
    virtual hkResult walkMemory(void (*callback)(void*, unsigned int, bool, int, void*), void* param); // vtbl+0x18 0x00CF58F0
    virtual void blockFreeBatch(void** blocks, int nblocks, int nbytes); // vtbl+0x1C 0x00CF4EC0
    virtual unsigned int getApproxTotalAllocated() const; // vtbl+0x1C 0x00CF4B50
    virtual void getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& stats) const; // vtbl+0x20 0x00CF4C40
    virtual void setScrubValues(unsigned int allocValue, unsigned int); // vtbl+0x20 0x00CF4B70
    virtual int getAllocatedSize(const void* p, int nbytes) const; // vtbl+0x24 0x00CF4E00
    virtual int addToSnapshot(hkMemorySnapshot& snap, int parentId); // vtbl+0x24 0x00CF5550
    virtual void resetPeakMemoryStatistics(); // vtbl+0x28 0x00CF4D40
    virtual hkMemoryAllocator::ExtendedInterface* getExtendedInterface(); // vtbl+0x2C 0x00CF5A80
    virtual hkBool isOk() const; // vtbl+0x30 0x00CF4D80
    static void setDefaultCinfo(Cinfo& info); // 0x00CF5180
    static void setFixedSizeCinfo(unsigned int blockSize, Cinfo& info); // 0x00CF50D0
public:
    void _construct(const Cinfo* infoIn); // 0x00CF5760
    hkFreeList* _newFreeList(unsigned int elementSize, unsigned int alignment, unsigned int blockSize); // 0x00CF56D0
    void _deleteFreeList(hkFreeList* freeList); // 0x00CF5260
    static hkBool _comparePointers(void*, void*);
    bool _hasMemoryAvailable(unsigned int size); // 0x00CF5BD0

    hkCriticalSection m_criticalSection; // 0x08
    unsigned int m_totalBytesInFreeLists; // 0x20
    unsigned int m_peakInUse; // 0x24
    hkMemoryAllocator* m_allocator; // 0x28
    hkMemoryAllocator::ExtendedInterface* m_allocatorExtended; // 0x2C
    hkFixedSizeAllocator m_blockAllocator; // 0x30
    hkFreeList* m_sizeToFreeList[41]; // 0x6C
    hkFreeList* m_freeLists[41]; // 0x110
    int m_numFreeLists; // 0x1B4
    hkFreeList* m_topFreeList; // 0x1B8
    hkFreeList* m_lastFreeList; // 0x1BC
    hkFreeList m_freeListMemory[41]; // 0x1C0
    unsigned int m_softLimit; // 0xAB8
    int m_incrementalFreeListIndex; // 0xABC
public:
    static unsigned int s_fillReturnedToUser; // 0x016F4284
};
