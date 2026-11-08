// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/thread/hkthreadmemory.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): a per-thread cache in front of the shared heap allocator. Blocks up to 8 KB
// are sorted into 16 size rows (16, 32, 48, 64, 96, 128, 160, 192, 256, 320, 512, 640, 1 K, 2 K, 4 K, 8 K bytes);
// each row keeps a list of freed blocks of its size. An empty row fetches 4 blocks at once from the shared allocator
// (keeping 3), a row holding m_maxNumElemsOnFreeList blocks gives half of them back in batches of 4. Larger blocks
// go straight to the shared allocator.
//
// Notes (decompiled behaviour, kept as is):
// - initLookupTables never sets m_row_to_size_lut[0]; getMemoryStatistics multiplies it by row 0's (always 0) count.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/thread/hkthreadmemory.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/util/hkmemutil.h"

#include <intrin.h>

// ---- FreeList (hkthreadmemory.h:40-49)
// 0x00D0F4D0
hkThreadMemory::FreeList::FreeList()
    : m_head(nullptr)
    , m_numElem(0)
{
}

// 0x00D0F4E0
void hkThreadMemory::FreeList::put(void* p)
{
    m_numElem++;
    ((FreeElem*)p)->m_next = m_head;
    m_head = (FreeElem*)p;
}

// 0x00D0F4F0
void* hkThreadMemory::FreeList::get()
{
    FreeElem* e = m_head;
    if (e != nullptr)
    {
        m_numElem--;
        m_head = e->m_next;
        return e;
    }
    return nullptr;
}

// ---- rows (hkthreadmemory.inl)
// 0x00D0F550 (.inl:30)
int hkThreadMemory::constSizeToRow(int size)
{
    if (size <= 16) return 1;
    if (size <= 32) return 2;
    if (size <= 48) return 3;
    if (size <= 64) return 4;
    if (size <= 96) return 5;
    if (size <= 128) return 6;
    if (size <= 160) return 7;
    if (size <= 192) return 8;
    if (size <= 256) return 9;
    if (size <= 320) return 10;
    if (size <= 512) return 11;
    if (size <= 640) return 12;
    if (size <= 1024) return 13;
    if (size <= 2048) return 14;
    if (size <= 4096) return 15;
    if (size <= 8192) return 16;
    __debugbreak(); // HK_BREAKPOINT: larger than MEMORY_MAX_SIZE_LARGE_BLOCK
    return 0;
}

// 0x00D0F510 (.inl:10)
int hkThreadMemory::getRow(int nbytes) const
{
    if (nbytes <= MEMORY_MAX_SIZE_SMALL_BLOCK)
    {
        return m_small_size_to_row_lut[(nbytes + MEMORY_SMALL_BLOCK_ADD) >> MEMORY_SMALL_BLOCK_RSHIFT_BITS];
    }
    return m_large_size_to_row_lut[(nbytes - 1) >> MEMORY_LARGE_BLOCK_RSHIFT_BITS];
}

// 0x00D0F540 (.inl:25)
int hkThreadMemory::rowToSize(int row) const
{
    return m_row_to_size_lut[row];
}

// ---- construction
// 0x00D0FDA0 (:62)
hkThreadMemory::hkThreadMemory()
{
    initLookupTables();
    setMemory(nullptr, 8);
}

// 0x00D0FDE0 (:68)
hkThreadMemory::hkThreadMemory(hkMemoryAllocator* memoryInstance)
{
    initLookupTables();
    setMemory(memoryInstance, 8);
}

// 0x00CF38C0 (.h:83)
hkThreadMemory::~hkThreadMemory()
{
    releaseCachedMemory();
}

// 0x00D0F780 (:92)
// Small sizes in 16-byte steps (0..640), large ones in 1 KB steps (1..8 KB).
void hkThreadMemory::initLookupTables()
{
    for (int i = 0; i <= MEMORY_MAX_SIZE_SMALL_BLOCK >> MEMORY_SMALL_BLOCK_RSHIFT_BITS; i++)
    {
        const int size = i << MEMORY_SMALL_BLOCK_RSHIFT_BITS;
        const int row = constSizeToRow(size);
        m_small_size_to_row_lut[i] = (char)row;
        m_row_to_size_lut[row] = size;
    }
    int* large = m_large_size_to_row_lut;
    for (int size = 1 << MEMORY_LARGE_BLOCK_RSHIFT_BITS; size < MEMORY_MAX_SIZE_LARGE_BLOCK + (1 << MEMORY_LARGE_BLOCK_RSHIFT_BITS); size += 1 << MEMORY_LARGE_BLOCK_RSHIFT_BITS)
    {
        const int row = constSizeToRow(size);
        *large++ = row;
        m_row_to_size_lut[row] = size;
    }
}

// 0x00D0FA20 (:124)
void hkThreadMemory::setMemory(hkMemoryAllocator* memoryInstance, int maxNumElemsOnFreeList)
{
    if (memoryInstance != nullptr)
    {
        m_memory = memoryInstance;
        m_maxNumElemsOnFreeList = maxNumElemsOnFreeList;
        return;
    }
    m_memory = nullptr;
}

// ---- allocation
// 0x00D0FC20 (:227)
void* hkThreadMemory::blockAlloc(int nbytes)
{
    if (nbytes > MEMORY_MAX_SIZE_LARGE_BLOCK)
    {
        return m_memory->blockAlloc(nbytes);
    }
    const int row = getRow(nbytes);
    void* p = m_free_list[row].get();
    if (p != nullptr)
    {
        return p;
    }
    return onRowEmpty(row);
}

// 0x00D0FC80 (:244)
void hkThreadMemory::blockFree(void* p, int nbytes)
{
    if (p == nullptr)
    {
        return;
    }
    if (nbytes > MEMORY_MAX_SIZE_LARGE_BLOCK)
    {
        m_memory->blockFree(p, nbytes);
        return;
    }
    const int row = getRow(nbytes);
    if (m_free_list[row].m_numElem >= m_maxNumElemsOnFreeList)
    {
        onRowFull(row, p);
        return;
    }
    m_free_list[row].put(p);
}

// 0x00D0F620 (:13)
void* hkThreadMemory::bufAlloc(int& reqNumInOut)
{
    return blockAlloc(reqNumInOut);
}

// 0x00D0F640 (:17)
void hkThreadMemory::bufFree(void* p, int numElem)
{
    blockFree(p, numElem);
}

// 0x00D0FD50 (:21)
void* hkThreadMemory::bufRealloc(void* pold, int oldNum, int& reqNumInOut)
{
    void* p = hkThreadMemory::blockAlloc(reqNumInOut);
    if (p != nullptr)
    {
        hkMemUtil::memCpy(p, pold, oldNum <= reqNumInOut ? oldNum : reqNumInOut);
    }
    hkThreadMemory::blockFree(pold, oldNum);
    return p;
}

// 0x00D0F650 (:31)
void hkThreadMemory::blockAllocBatch(void** ptrsOut, int numPtrs, int blockSize)
{
    if (blockSize > MEMORY_MAX_SIZE_LARGE_BLOCK)
    {
        m_memory->blockAllocBatch(ptrsOut, numPtrs, blockSize);
        return;
    }
    for (int i = 0; i < numPtrs; i++)
    {
        ptrsOut[i] = blockAlloc(blockSize);
    }
}

// 0x00D0F6B0 (:45)
void hkThreadMemory::blockFreeBatch(void** ptrsIn, int numPtrs, int blockSize)
{
    if (blockSize > MEMORY_MAX_SIZE_LARGE_BLOCK)
    {
        m_memory->blockFreeBatch(ptrsIn, numPtrs, blockSize);
        return;
    }
    for (int i = 0; i < numPtrs; i++)
    {
        blockFree(ptrsIn[i], blockSize);
    }
}

// 0x00D0FA40 (:150)
// Fetches up to BATCH_SIZE blocks at once, keeps all but the first.
void* hkThreadMemory::onRowEmpty(int rowIndex)
{
    int num = m_maxNumElemsOnFreeList;
    if (num == 0)
    {
        return m_memory->blockAlloc(m_row_to_size_lut[rowIndex]);
    }
    if (num > BATCH_SIZE - 1)
    {
        num = BATCH_SIZE;
    }
    void* blocks[BATCH_SIZE];
    m_memory->blockAllocBatch(blocks, num, m_row_to_size_lut[rowIndex]);
    for (int i = 1; i < num; i++)
    {
        m_free_list[rowIndex].put(blocks[i]);
    }
    return blocks[0];
}

// 0x00D0FAD0 (:174)
// Gives the row's blocks back down to half the maximum, in batches, then keeps p.
void hkThreadMemory::onRowFull(int rowIndex, void* p)
{
    const int blockSize = m_row_to_size_lut[rowIndex];
    if (m_maxNumElemsOnFreeList == 0)
    {
        m_memory->blockFree(p, blockSize);
        return;
    }
    const int keep = m_maxNumElemsOnFreeList / 2;
    for (int n = m_free_list[rowIndex].m_numElem; n > keep;)
    {
        int num = n - keep;
        if (num > BATCH_SIZE)
        {
            num = BATCH_SIZE;
        }
        void* blocks[BATCH_SIZE];
        for (int i = 0; i < num; i++)
        {
            blocks[i] = m_free_list[rowIndex].get();
        }
        m_memory->blockFreeBatch(blocks, num, blockSize);
        n -= num;
    }
    m_free_list[rowIndex].put(p);
}

// 0x00D0FBA0 (:207)
void hkThreadMemory::clearRow(int rowIndex)
{
    const int blockSize = m_row_to_size_lut[rowIndex];
    for (int n = m_free_list[rowIndex].m_numElem; n > 0;)
    {
        const int num = n > BATCH_SIZE ? BATCH_SIZE : n;
        void* blocks[BATCH_SIZE];
        for (int i = 0; i < num; i++)
        {
            blocks[i] = m_free_list[rowIndex].get();
        }
        m_memory->blockFreeBatch(blocks, num, blockSize);
        n -= num;
    }
}

// 0x00D0FE20 (:138)
void hkThreadMemory::releaseCachedMemory()
{
    for (int row = MEMORY_MAX_ALL_ROW - 1; row >= 0; row--)
    {
        if (m_free_list[row].m_numElem != 0)
        {
            clearRow(row);
        }
    }
}

// 0x00D0F9D0 (:112)
unsigned int hkThreadMemory::getCachedSizeUnchecked()
{
    int total = 0;
    for (int row = 0; row < MEMORY_MAX_ALL_ROW; row++)
    {
        total += m_row_to_size_lut[row] * m_free_list[row].m_numElem; // see the notes (row 0)
    }
    return (unsigned int)total;
}

// 0x00D0F710 (:75)
// Only the cached bytes are known (available).
void hkThreadMemory::getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& u) const
{
    u.m_allocated = -1;
    u.m_inUse = -1;
    u.m_peakInUse = -1;
    int total = 0;
    for (int row = 0; row < MEMORY_MAX_ALL_ROW; row++)
    {
        total += m_row_to_size_lut[row] * m_free_list[row].m_numElem; // see the notes (row 0)
    }
    u.m_available = total;
    u.m_totalAvailable = -1;
    u.m_largestBlock = -1;
}

// 0x00D0FCF0 (:267)
int hkThreadMemory::getAllocatedSize(const void* obj, int numBytes) const
{
    if (numBytes <= MEMORY_MAX_SIZE_LARGE_BLOCK)
    {
        numBytes = m_row_to_size_lut[getRow(numBytes)];
    }
    return m_memory->getAllocatedSize(obj, numBytes);
}
