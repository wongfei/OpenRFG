// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/solver/hksolverallocator.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): the solver buffer allocator (the 1 MB buffer havok_memory_init asks for).
// Blocks are cut from the end of the used part ([m_bufferStart, m_currentEnd)); freed blocks below the end go into
// a sorted list of free ranges (at most 64, merged with their neighbours) and are reused best fit. blockAlloc rounds
// to 128 bytes and wants the exact size; bufAlloc may return a larger free range whole. A best-fit range is split
// when the exact size is wanted, or when it is over 1.5 times the request and the request is over 1 KB.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/solver/hksolverallocator.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/util/hkmemutil.h"

namespace
{
// RAII lock of the allocator's critical section.
struct Lock
{
    hkCriticalSection& m_section;
    explicit Lock(hkCriticalSection& section) : m_section(section) { m_section.enter(); }
    ~Lock() { m_section.leave(); }
};

int roundTo128(int numBytes)
{
    return (numBytes + 127) & ~127;
}
}

// 0x00D0FEF0 (:19)
hkSolverAllocator::hkSolverAllocator()
    : m_criticalSection(0) // 0: the spin count from the number of hardware threads
{
    m_bufferStart = nullptr;
    m_bufferEnd = nullptr;
    m_currentEnd = nullptr;
    m_numAllocatedBlocks = 0;
    m_peakUse = 0;
    m_freeElems.m_data = m_elemsBuf;
    m_freeElems.m_size = 0;
    m_freeElems.m_capacityAndFlags = 64 | hkArrayBase<Element>::DONT_DEALLOCATE_FLAG;
}

// 0x00D0FF30 (:23)
hkSolverAllocator::~hkSolverAllocator()
{
}

// 0x00D10270 (:27)
void hkSolverAllocator::setBuffer(void* vbuffer, int bufferSize)
{
    Lock lock(m_criticalSection);
    m_freeElems.m_data = m_elemsBuf;
    m_freeElems.m_size = 0;
    m_freeElems.m_capacityAndFlags = 64 | hkArrayBase<Element>::DONT_DEALLOCATE_FLAG;
    if (vbuffer == nullptr)
    {
        bufferSize = 0;
    }
    m_bufferStart = (char*)vbuffer;
    m_currentEnd = (char*)vbuffer;
    m_bufferEnd = (char*)vbuffer + bufferSize;
    m_numAllocatedBlocks = 0;
}

// 0x00D102C0 (:48)
void* hkSolverAllocator::blockAlloc(int numBytes)
{
    if (numBytes == 0)
    {
        return nullptr;
    }
    int size = roundTo128(numBytes);
    return allocate(size, true);
}

// 0x00D0FE60 (:65)
void hkSolverAllocator::blockFree(void* p, int numBytes)
{
    if (p != nullptr && numBytes != 0)
    {
        bufFree(p, roundTo128(numBytes));
    }
}

// 0x00D102F0 (:73)
void* hkSolverAllocator::bufAlloc(int& reqNumInOut)
{
    return allocate(reqNumInOut, false);
}

// 0x00D0FF50 (:78)
void* hkSolverAllocator::allocate(int& reqNumInOut, bool useExactSize)
{
    Lock lock(m_criticalSection);
    const int numBytes = reqNumInOut;
    // a free range list that may need two new entries for this block must fit in the inline storage
    if (m_numAllocatedBlocks + 1 >= m_freeElems.getCapacity() * 2 - m_freeElems.m_size)
    {
        return nullptr;
    }
    int best = -1;
    int bestSize = 0x7FFFFFF;
    for (int i = m_freeElems.m_size - 1; i >= 0; i--)
    {
        const int size = m_freeElems.m_data[i].m_size;
        if (numBytes <= size && size < bestSize)
        {
            bestSize = size;
            best = i;
        }
    }
    if (best >= 0)
    {
        Element& e = m_freeElems.m_data[best];
        char* p = e.m_start;
        if ((useExactSize && bestSize != numBytes) || (numBytes * 3 < bestSize * 2 && numBytes > 0x400)) // 0x400: 1 KB
        {
            e.m_size -= numBytes;
            e.m_start = p + numBytes;
        }
        else
        {
            reqNumInOut = e.m_size;
            m_freeElems.removeAtAndCopy(best);
        }
        const unsigned int used = (unsigned int)(m_currentEnd - m_bufferStart);
        m_numAllocatedBlocks++;
        if (m_peakUse < used)
        {
            m_peakUse = used;
        }
        return p;
    }
    char* p = m_currentEnd;
    if (numBytes > (int)(m_bufferEnd - p))
    {
        return nullptr;
    }
    m_currentEnd = p + numBytes;
    reqNumInOut = numBytes;
    const unsigned int used = (unsigned int)(m_currentEnd - m_bufferStart);
    m_numAllocatedBlocks++;
    if (m_peakUse < used)
    {
        m_peakUse = used;
    }
    return p;
}

// 0x00D100B0 (:160)
// A block at the end moves the end down (and over free ranges now at the end); others join the sorted free ranges.
void hkSolverAllocator::bufFree(void* data, int size)
{
    Lock lock(m_criticalSection);
    m_numAllocatedBlocks--;
    char* start = (char*)data;
    char* end = start + size;
    if (end == m_currentEnd)
    {
        m_currentEnd -= size;
        while (m_freeElems.m_size != 0)
        {
            Element& last = m_freeElems.m_data[m_freeElems.m_size - 1];
            if (last.m_start + last.m_size != m_currentEnd)
            {
                break;
            }
            m_currentEnd = last.m_start;
            m_freeElems.m_size--;
        }
        return;
    }
    if (m_freeElems.m_size == 0)
    {
        m_freeElems.m_size = 1;
        m_freeElems.m_data[0].m_start = start;
        m_freeElems.m_data[0].m_size = size;
        return;
    }
    const int last = m_freeElems.m_size - 1;
    // the last range below the block
    int below = last;
    for (; below >= 0; below--)
    {
        if (m_freeElems.m_data[below].m_start < start)
        {
            break;
        }
    }
    if (below >= 0)
    {
        Element& b = m_freeElems.m_data[below];
        if (b.m_start + b.m_size == start)
        {
            // joins the range below, and the one above if it touches it
            b.m_size += size;
            if (below < m_freeElems.m_size - 1 && end == m_freeElems.m_data[below + 1].m_start)
            {
                b.m_size += m_freeElems.m_data[below + 1].m_size;
                m_freeElems.removeAtAndCopy(below + 1);
            }
            return;
        }
    }
    if (below < last && m_freeElems.m_data[below + 1].m_start == end)
    {
        // joins the range above
        Element& a = m_freeElems.m_data[below + 1];
        a.m_size += size;
        a.m_start = start;
        return;
    }
    // a new range after `below`
    m_freeElems.m_size++;
    for (int i = m_freeElems.m_size - 1; i > below + 1; i--)
    {
        m_freeElems.m_data[i] = m_freeElems.m_data[i - 1];
    }
    m_freeElems.m_data[below + 1].m_start = start;
    m_freeElems.m_data[below + 1].m_size = size;
}

// 0x00D0FE90 (:245)
bool hkSolverAllocator::canAllocSingleBlock(int numBytes)
{
    return numBytes <= (int)(m_bufferEnd - m_bufferStart);
}

// 0x00D0FEB0 (:250)
// The total available and the largest block are left as they are.
void hkSolverAllocator::getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& u) const
{
    u.m_allocated = (long)(m_bufferEnd - m_bufferStart);
    u.m_available = (long)(m_bufferEnd - m_currentEnd);
    u.m_inUse = (long)(m_currentEnd - m_bufferStart);
    u.m_peakInUse = m_peakUse;
}

// 0x00D0FEE0 (:258)
void hkSolverAllocator::resetPeakMemoryStatistics()
{
    m_peakUse = (unsigned int)(m_currentEnd - m_bufferStart);
}

// 0x00D103C0 (.h:66)
int hkSolverAllocator::getAllocatedSize(const void* obj, int nbytes) const
{
    return nbytes;
}

// 0x00CF3990 (.h:63)
int hkSolverAllocator::getBufferSize() const
{
    return (int)(m_bufferEnd - m_bufferStart);
}

// 0x00D0FE50 (.h:58)
char* hkSolverAllocator::Element::getEnd()
{
    return m_start + m_size;
}
