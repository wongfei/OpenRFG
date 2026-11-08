// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/freelist/hkfreelistallocator.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): the heap allocator. Requests up to 640 bytes go to one of up to 41 free lists
// by size class (16-byte steps; m_sizeToFreeList maps a class to the smallest list whose element size covers it),
// larger ones straight to the server allocator (the large block allocator). The free lists take their element
// blocks from the server and their 16-byte block headers from m_blockAllocator. Everything runs under one critical
// section. m_totalBytesInFreeLists tracks the free element bytes, so the bytes in use are the server's total
// allocation (getApproxTotalAllocated of its extended interface) minus that; crossing the soft limit sets the
// out-of-memory state.
//
// The ExtendedInterface virtuals (garbageCollect, incrementalGarbageCollect, soft limit, canAllocTotal,
// getApproxTotalAllocated, setScrubValues, walkMemory, addToSnapshot) are entered with `this` at the interface
// (+4); the decompiler shows their members 4 bytes early.
//
// Notes (decompiled behaviour, kept as is):
// - garbageCollect repeats its pass only while some list had free blocks and findGarbage returned a negative count,
//   which it never does, so it runs once.
// - _newFreeList breaks into the debugger (int3) when the 41 free list slots are used up.
// Not reimplemented: walkMemory (needs hkMallocAllocator, S phase) fails, addToSnapshot (hkMemorySnapshot, S phase)
// records nothing.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/freelist/hkfreelistallocator.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

#include <intrin.h>
#include <new>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/debugutil/memoryexceptiontestingutil/hkmemoryexceptiontestingutil.h"

// ---- globals
unsigned int hkFreeListAllocator::s_fillReturnedToUser; // 0x016F4284

namespace
{
// RAII lock of the allocator's critical section.
struct Lock
{
    hkCriticalSection& m_section;
    explicit Lock(hkCriticalSection& section) : m_section(section) { m_section.enter(); }
    ~Lock() { m_section.leave(); }
};
}

// ---- Cinfo
// 0x00CF4A50
hkFreeListAllocator::Cinfo::Cinfo()
{
    m_preferedBlockSize = 0;
    m_numInfos = 0;
}

// 0x00CF4B90 (:283)
int hkFreeListAllocator::Cinfo::findInfoIndex(unsigned int elementSize) const
{
    for (int i = 0; i < m_numInfos; i++)
    {
        if (m_infos[i].m_elementSize == elementSize)
        {
            return i;
        }
    }
    return -1;
}

// 0x00CF4BC0 (:296)
// Adds a free list description, or replaces the one of the same element size.
void hkFreeListAllocator::Cinfo::add(unsigned int elementSize, unsigned int alignment, unsigned int blockSize)
{
    int index = findInfoIndex(elementSize);
    if (index < 0)
    {
        index = m_numInfos++;
    }
    m_infos[index].m_elementSize = elementSize;
    m_infos[index].m_alignment = alignment;
    m_infos[index].m_blockSize = blockSize;
}

// 0x00CF4C00 (:315)
void hkFreeListAllocator::Cinfo::removeAt(int index)
{
    if (index != m_numInfos - 1)
    {
        m_infos[index] = m_infos[m_numInfos - 1];
    }
    m_numInfos--;
}

// Element size of the free list of size class `size` (16..512 in 16-byte steps): sizes are rounded down to
// multiples of 32 above 128 and of 64 above 256, then up to the alignment (32 from 64 bytes, 16 below).
static unsigned int freeListElementSize(unsigned int size, unsigned int& alignment)
{
    unsigned int s = size;
    if (size >= 0x100)
    {
        s = size & ~0x3Fu;
    }
    else if (size > 0x80)
    {
        s = size & ~0x1Fu;
    }
    alignment = s > 0x3F ? 0x20 : 0x10;
    return (s + alignment - 1) & ~(alignment - 1);
}

// 0x00CF50D0 (:325)
// Every free list gets the same block size.
void hkFreeListAllocator::setFixedSizeCinfo(unsigned int blockSize, Cinfo& info)
{
    info.m_preferedBlockSize = blockSize;
    info.add(0x200, 0x80, blockSize);
    info.add(0x100, 0x20, blockSize);
    info.add(0, 4, blockSize);
    info.add(0x280, 0x80, blockSize);
    for (unsigned int size = 0x200; (int)size > 0xF; size -= 0x10)
    {
        unsigned int alignment;
        const unsigned int elementSize = freeListElementSize(size, alignment);
        if (info.findInfoIndex(elementSize) < 0)
        {
            info.add(elementSize, alignment, blockSize);
        }
    }
}

// 0x00CF5180 (:388)
// Small lists get 4 KB blocks, lists from 256 bytes blocks of 16 elements; the 0-byte list 256-byte blocks.
void hkFreeListAllocator::setDefaultCinfo(Cinfo& info)
{
    info.add(0x200, 0x80, 0x4000);
    info.add(0x100, 0x20, 0x4000);
    info.add(0, 4, 0x100);
    info.add(0x280, 0x80, 0x4000);
    for (unsigned int size = 0x200; (int)size > 0xF; size -= 0x10)
    {
        unsigned int alignment;
        const unsigned int elementSize = freeListElementSize(size, alignment);
        unsigned int blockSize = size << 4;
        if (elementSize == 0 || size < 0x100)
        {
            blockSize = 0x1000;
        }
        if (info.findInfoIndex(elementSize) < 0)
        {
            info.add(elementSize, alignment, blockSize);
        }
    }
}

// ---- construction
// 0x00CF4AE0 (:93)
hkFreeListAllocator::hkFreeListAllocator()
    : m_criticalSection(4000) // 4000: spin count
{
    m_allocator = nullptr;
    m_allocatorExtended = nullptr;
    m_numFreeLists = 0;
}

// 0x00CF5850 (:84)
hkFreeListAllocator::hkFreeListAllocator(hkMemoryAllocator* server, hkMemoryAllocator::ExtendedInterface* allocatorExtended, const Cinfo* info)
    : m_criticalSection(4000) // 4000: spin count
{
    m_allocatorExtended = allocatorExtended;
    m_allocator = server;
    m_numFreeLists = 0;
    _construct(info);
}

// 0x00CF58D0 (:97)
void hkFreeListAllocator::init(hkMemoryAllocator* allocator, hkMemoryAllocator::ExtendedInterface* allocatorExtended, const Cinfo* info)
{
    m_allocator = allocator;
    m_allocatorExtended = allocatorExtended;
    _construct(info);
}

// 0x00CF5760 (:108)
// No info: one free list per size class with 8 KB blocks (setFixedSizeCinfo).
void hkFreeListAllocator::_construct(const Cinfo* infoIn)
{
    m_incrementalFreeListIndex = 0;
    m_totalBytesInFreeLists = 0;
    m_peakInUse = 0;
    setMemorySoftLimit(SOFT_LIMIT_MAX);
    Cinfo local;
    if (infoIn == nullptr)
    {
        setFixedSizeCinfo(0x2000, local);
        infoIn = &local;
    }
    unsigned int blockSize = infoIn->m_preferedBlockSize;
    if (blockSize == 0)
    {
        blockSize = 0x400;
    }
    m_blockAllocator.init(sizeof(hkFreeList::Block), 4, blockSize, m_allocator);
    m_topFreeList = m_freeListMemory;
    m_lastFreeList = m_freeListMemory + MAX_UNIQUE_FREELISTS;
    for (int i = 0; i < MAX_FREELISTS; i++)
    {
        m_sizeToFreeList[i] = nullptr;
    }
    for (int i = 0; i < infoIn->m_numInfos; i++)
    {
        const FreeListCinfo& fl = infoIn->m_infos[i];
        m_sizeToFreeList[fl.m_elementSize >> FREELIST_SHIFT] = _newFreeList(fl.m_elementSize, fl.m_alignment, fl.m_blockSize);
    }
    // a size class without its own list uses the next larger one
    hkFreeList* next = m_sizeToFreeList[MAX_FREELISTS - 1];
    for (int i = MAX_FREELISTS - 1; i >= 0; i--)
    {
        if (m_sizeToFreeList[i] == nullptr)
        {
            m_sizeToFreeList[i] = next;
        }
        next = m_sizeToFreeList[i];
    }
}

// 0x00CF56D0 (:528)
// A list of an element size that already exists is dropped again and the existing one returned.
hkFreeList* hkFreeListAllocator::_newFreeList(unsigned int elementSize, unsigned int alignment, unsigned int blockSize)
{
    hkFreeList* list = m_topFreeList;
    if (list >= m_lastFreeList)
    {
        __debugbreak(); // HK_BREAKPOINT: out of free list slots (see the notes)
    }
    m_topFreeList = list + 1;
    new (list) hkFreeList(elementSize, alignment, blockSize, m_allocator, &m_blockAllocator);
    for (int i = 0; i < m_numFreeLists; i++)
    {
        if (m_freeLists[i]->m_elementSize == list->m_elementSize)
        {
            _deleteFreeList(list);
            return m_freeLists[i];
        }
    }
    m_freeLists[m_numFreeLists] = list;
    m_numFreeLists++;
    return list;
}

// 0x00CF5260 (:553)
void hkFreeListAllocator::_deleteFreeList(hkFreeList* freeList)
{
    freeList->freeAllMemory();
    if (freeList + 1 == m_topFreeList)
    {
        m_topFreeList--;
    }
}

// 0x00CF5610 (:218)
hkFreeListAllocator::~hkFreeListAllocator()
{
    for (int i = 0; i < m_numFreeLists; i++)
    {
        hkFreeList* list = m_freeLists[i];
        if (list != nullptr)
        {
            _deleteFreeList(list);
            for (int j = i + 1; j < m_numFreeLists; j++)
            {
                if (m_freeLists[j] == list)
                {
                    m_freeLists[j] = nullptr;
                }
            }
        }
    }
    m_blockAllocator.m_freeList.freeAllMemory();
}

// ---- hkMemoryAllocator
// 0x00CF5290 (:581)
void* hkFreeListAllocator::bufAlloc(int& reqNumInOut)
{
    Lock lock(m_criticalSection);
    const int numBytes = reqNumInOut;
    void* p;
    if (numBytes <= MAX_FREELIST_SIZE)
    {
        hkFreeList* list = m_sizeToFreeList[(numBytes + FREELIST_ALIGNMENT - 1) >> FREELIST_SHIFT];
        const unsigned int freeBefore = list->m_numFreeElements;
        p = list->alloc();
        m_totalBytesInFreeLists += (list->m_numFreeElements - freeBefore) * list->m_elementSize;
        reqNumInOut = list->m_elementSize;
    }
    else
    {
        p = m_allocator->blockAlloc(numBytes);
    }
    if (m_allocatorExtended != nullptr)
    {
        const unsigned int inUse = m_allocatorExtended->getApproxTotalAllocated() - m_totalBytesInFreeLists;
        if (m_peakInUse < inUse)
        {
            m_peakInUse = inUse;
        }
        if (m_softLimit <= inUse)
        {
            hkSetOutOfMemoryState(hkMemoryAllocator::MEMORY_STATE_OUT_OF_MEMORY);
        }
    }
    return p;
}

// 0x00CF4E40 (:617)
void* hkFreeListAllocator::blockAlloc(int nbytes)
{
    return bufAlloc(nbytes);
}

// 0x00CF4E50 (:661)
void hkFreeListAllocator::blockFree(void* p, int nbytes)
{
    if (p == nullptr)
    {
        return;
    }
    Lock lock(m_criticalSection);
    if (nbytes <= MAX_FREELIST_SIZE)
    {
        hkFreeList* list = m_sizeToFreeList[(nbytes + FREELIST_ALIGNMENT - 1) >> FREELIST_SHIFT];
        const unsigned int freeBefore = list->m_numFreeElements;
        list->free(p);
        m_totalBytesInFreeLists += (list->m_numFreeElements - freeBefore) * list->m_elementSize;
        return;
    }
    m_allocator->blockFree(p, nbytes);
}

// 0x00CF5360 (:623)
void hkFreeListAllocator::blockAllocBatch(void** blocksOut, int nblocks, int nbytes)
{
    Lock lock(m_criticalSection);
    if (nbytes <= MAX_FREELIST_SIZE)
    {
        hkFreeList* list = m_sizeToFreeList[(nbytes + FREELIST_ALIGNMENT - 1) >> FREELIST_SHIFT];
        const unsigned int freeBefore = list->m_numFreeElements;
        list->allocBatch(blocksOut, nblocks);
        m_totalBytesInFreeLists += (list->m_numFreeElements - freeBefore) * list->m_elementSize;
    }
    else
    {
        for (int i = 0; i < nblocks; i++)
        {
            blocksOut[i] = m_allocator->blockAlloc(nbytes);
        }
    }
    if (m_allocatorExtended != nullptr)
    {
        const unsigned int inUse = m_allocatorExtended->getApproxTotalAllocated() - m_totalBytesInFreeLists;
        if (m_peakInUse < inUse)
        {
            m_peakInUse = inUse;
        }
        if (m_softLimit <= inUse)
        {
            hkSetOutOfMemoryState(hkMemoryAllocator::MEMORY_STATE_OUT_OF_MEMORY);
        }
    }
}

// 0x00CF4EC0 (:683)
void hkFreeListAllocator::blockFreeBatch(void** blocks, int nblocks, int nbytes)
{
    Lock lock(m_criticalSection);
    if (nbytes <= MAX_FREELIST_SIZE)
    {
        hkFreeList* list = m_sizeToFreeList[(nbytes + FREELIST_ALIGNMENT - 1) >> FREELIST_SHIFT];
        const unsigned int freeBefore = list->m_numFreeElements;
        list->freeBatch(blocks, nblocks);
        m_totalBytesInFreeLists += (list->m_numFreeElements - freeBefore) * list->m_elementSize;
        return;
    }
    for (int i = 0; i < nblocks; i++)
    {
        if (blocks[i] != nullptr)
        {
            m_allocator->blockFree(blocks[i], nbytes);
        }
    }
}

// 0x00CF4C40 (:458)
// The server's statistics plus the free lists' (and the block header list's): their free elements are available,
// their element bytes in use count instead of their allocated bytes, and the largest element size with a free
// element raises the largest block.
void hkFreeListAllocator::getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& stats) const
{
    Lock lock(const_cast<hkCriticalSection&>(m_criticalSection));
    m_allocator->getMemoryStatistics(stats);
    long available = 0;
    long inUse = 0;
    long allocated = 0;
    for (int i = 0; i < m_numFreeLists + 1; i++)
    {
        hkMemoryAllocator::MemoryStatistics s;
        const hkFreeList* list = i == m_numFreeLists ? &m_blockAllocator.m_freeList : m_freeLists[i];
        list->getMemoryStatistics(s);
        if (list->m_free != nullptr || list->m_top < list->m_blockEnd)
        {
            if ((unsigned long)stats.m_largestBlock < list->m_elementSize)
            {
                stats.m_largestBlock = list->m_elementSize;
            }
        }
        available += s.m_available;
        inUse += s.m_inUse;
        allocated += s.m_allocated;
    }
    stats.m_available += available;
    stats.m_inUse += inUse - allocated;
    stats.m_peakInUse = m_peakInUse;
}

// 0x00CF4E00 (:563)
int hkFreeListAllocator::getAllocatedSize(const void* p, int nbytes) const
{
    if (nbytes <= MAX_FREELIST_SIZE)
    {
        if (nbytes == 0)
        {
            return 0;
        }
        const hkFreeList* list = m_sizeToFreeList[(nbytes + FREELIST_ALIGNMENT - 1) >> FREELIST_SHIFT];
        if (list != nullptr)
        {
            return list->m_elementSize;
        }
    }
    return m_allocator->getAllocatedSize(p, nbytes);
}

// 0x00CF4D40 (:504)
void hkFreeListAllocator::resetPeakMemoryStatistics()
{
    hkMemoryAllocator::MemoryStatistics stats;
    getMemoryStatistics(stats);
    m_peakInUse = stats.m_inUse;
}

// 0x00CF5A80
hkMemoryAllocator::ExtendedInterface* hkFreeListAllocator::getExtendedInterface()
{
    return this;
}

// 0x00CF4D80 (:511)
hkBool hkFreeListAllocator::isOk() const
{
    Lock lock(const_cast<hkCriticalSection&>(m_criticalSection));
    hkBool result;
    result.m_bool = 1;
    for (int i = 0; i < m_numFreeLists; i++)
    {
        if (!m_freeLists[i]->_checkFreeBlocks())
        {
            result.m_bool = 0;
            break;
        }
    }
    return result;
}

// ---- ExtendedInterface
// 0x00CF4F80 (:883)
void hkFreeListAllocator::garbageCollect()
{
    Lock lock(m_criticalSection);
    bool negative = false;
    for (;;)
    {
        bool hadFreeBlocks = false;
        if (m_numFreeLists < 1)
        {
            break;
        }
        for (int i = 0; i < m_numFreeLists; i++)
        {
            hkFreeList* list = m_freeLists[i];
            const unsigned int freeBefore = list->m_numFreeElements;
            if (list->findGarbage() < 0)
            {
                negative = true;
            }
            if (list->m_freeBlocks != nullptr)
            {
                hadFreeBlocks = true;
            }
            list->freeAllFreeBlocks();
            m_totalBytesInFreeLists += (list->m_numFreeElements - freeBefore) * list->m_elementSize;
        }
        if (!hadFreeBlocks || !negative)
        {
            break; // see the notes
        }
    }
    if (m_allocatorExtended != nullptr)
    {
        m_allocatorExtended->garbageCollect();
    }
}

// 0x00CF5490 (:926)
// Checks up to numBlocksIn blocks (at least one) across the free lists, continuing where the last call stopped.
void hkFreeListAllocator::incrementalGarbageCollect(int numBlocksIn)
{
    Lock lock(m_criticalSection);
    int numBlocks = numBlocksIn < 1 ? 1 : numBlocksIn;
    const int start = m_incrementalFreeListIndex;
    int index = start;
    do
    {
        hkFreeList* list = m_freeLists[index];
        const unsigned int freeBefore = list->m_numFreeElements;
        int numChecked;
        if (list->incrementalFindGarbage(numBlocks, numChecked))
        {
            index++;
            if (index >= m_numFreeLists)
            {
                index = 0;
            }
        }
        list->freeAllFreeBlocks();
        numBlocks -= numChecked;
        m_totalBytesInFreeLists += (list->m_numFreeElements - freeBefore) * list->m_elementSize;
    } while (numBlocks > 0 && index != start);
    m_incrementalFreeListIndex = index;
    if (m_allocatorExtended != nullptr)
    {
        m_allocatorExtended->incrementalGarbageCollect(numBlocksIn);
    }
}

// 0x00CF5030 (:974)
hkResult hkFreeListAllocator::setMemorySoftLimit(unsigned int maxMemory)
{
    Lock lock(m_criticalSection);
    m_softLimit = maxMemory;
    return hkResult(HK_SUCCESS);
}

// 0x00CF5060 (:982)
unsigned int hkFreeListAllocator::getMemorySoftLimit() const
{
    return m_softLimit;
}

// 0x00CF5BD0 (:50)
bool hkFreeListAllocator::_hasMemoryAvailable(unsigned int size)
{
    if (m_allocatorExtended == nullptr)
    {
        return true;
    }
    const unsigned int inUse = m_allocatorExtended->getApproxTotalAllocated() - m_totalBytesInFreeLists;
    if (m_peakInUse < inUse)
    {
        m_peakInUse = inUse;
    }
    return inUse < m_softLimit - size;
}

// 0x00CF5090 (:242)
// _hasMemoryAvailable, inlined; the peak goes to m_peakInUse.
bool hkFreeListAllocator::canAllocTotal(int size)
{
    if (m_allocatorExtended == nullptr)
    {
        return true;
    }
    const unsigned int inUse = m_allocatorExtended->getApproxTotalAllocated() - m_totalBytesInFreeLists;
    if (m_peakInUse < inUse)
    {
        m_peakInUse = inUse;
    }
    return inUse < m_softLimit - (unsigned int)size;
}

// 0x00CF4B50 (:257)
unsigned int hkFreeListAllocator::getApproxTotalAllocated() const
{
    if (m_allocatorExtended == nullptr)
    {
        return 0;
    }
    return m_allocatorExtended->getApproxTotalAllocated() - m_totalBytesInFreeLists;
}

// 0x00CF4B70 (:273)
void hkFreeListAllocator::setScrubValues(unsigned int allocValue, unsigned int)
{
    Lock lock(m_criticalSection);
    s_fillReturnedToUser = allocValue;
}

// 0x00CF58F0 (:837)
// Not reimplemented (S phase: it walks the server through hkMallocAllocator bookkeeping).
hkResult hkFreeListAllocator::walkMemory(void (*callback)(void*, unsigned int, bool, int, void*), void* param)
{
    return hkResult(HK_FAILURE);
}

// 0x00CF5550 (:987)
// hkMemorySnapshot is S phase (not reimplemented): nothing is recorded.
int hkFreeListAllocator::addToSnapshot(hkMemorySnapshot& snap, int parentId)
{
    return -1;
}
