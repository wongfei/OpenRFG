// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/recall/hkrecallallocator.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): the debug allocator of the memory system. Every block gets a 16-byte header
// (the PDB Header plus padding) linking it into a list, so the live blocks can be listed; the statistics count the
// requested bytes (in use) and the allocated bytes with headers. A block not on the list is ignored on free.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/recall/hkrecallallocator.h"

namespace
{
const int HEADER_SIZE = 16; // sizeof(Header) rounded up to keep payloads 16-byte aligned

int allocationSize(int numBytes)
{
    return ((numBytes + 15) & ~15) + HEADER_SIZE;
}
}

// 0x00D11590 (:15)
hkRecallAllocator::hkRecallAllocator(hkMemoryAllocator* a)
    : m_criticalSection(1000) // 1000: spin count
{
    m_alloc = a;
    m_stats.m_available = -1;
    m_stats.m_totalAvailable = -1;
    m_stats.m_largestBlock = -1;
    m_head = nullptr;
    m_stats.m_allocated = 0;
    m_stats.m_inUse = 0;
    m_stats.m_peakInUse = 0;
}

// 0x00CF3A10
hkRecallAllocator::~hkRecallAllocator()
{
}

// 0x00D11510 (:25)
void hkRecallAllocator::init(hkMemoryAllocator* a)
{
    m_alloc = a;
}

// 0x00D115D0 (:31)
void* hkRecallAllocator::blockAlloc(int numBytes)
{
    m_criticalSection.enter();
    const int size = allocationSize(numBytes);
    Header* h = (Header*)m_alloc->blockAlloc(size);
    h->m_payloadSize = numBytes;
    h->m_requestedSize = size;
    h->m_next = m_head;
    m_stats.m_inUse += numBytes;
    m_stats.m_allocated += size;
    m_head = h;
    m_stats.m_peakInUse = m_stats.m_inUse < m_stats.m_peakInUse ? m_stats.m_peakInUse : m_stats.m_inUse;
    m_criticalSection.leave();
    return (char*)h + HEADER_SIZE;
}

// 0x00D11640 (:52)
void hkRecallAllocator::blockFree(void* p, int numBytes)
{
    if (p == nullptr)
    {
        return;
    }
    m_criticalSection.enter();
    Header* h = (Header*)((char*)p - HEADER_SIZE);
    Header head; // the list head acts as a header
    head.m_payloadSize = -1;
    head.m_requestedSize = -1;
    head.m_next = m_head;
    Header* prev = &head;
    if (m_head != nullptr)
    {
        for (;;)
        {
            Header* cur = prev->m_next;
            if (cur == h)
            {
                break;
            }
            prev = cur;
            if (cur->m_next == nullptr)
            {
                m_criticalSection.leave();
                return;
            }
        }
        prev->m_next = h->m_next;
        m_stats.m_allocated -= allocationSize(numBytes);
        m_stats.m_inUse -= numBytes;
        m_head = head.m_next;
        m_alloc->blockFree(h, h->m_requestedSize);
    }
    m_criticalSection.leave();
}

// 0x00D11520 (:93)
void hkRecallAllocator::getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& u) const
{
    const_cast<hkCriticalSection&>(m_criticalSection).enter();
    u = m_stats;
    const_cast<hkCriticalSection&>(m_criticalSection).leave();
}

// 0x00D11560 (:99)
void hkRecallAllocator::resetPeakMemoryStatistics()
{
    m_stats.m_peakInUse = m_stats.m_inUse;
}

// 0x00D11570 (:104)
int hkRecallAllocator::getAllocatedSize(const void* obj, int numBytes) const
{
    return numBytes;
}

// ---- Header (hkrecallallocator.h:41-47)
// 0x00CF39A0
int hkRecallAllocator::Header::getRequestedSize() const
{
    return m_requestedSize;
}

// 0x00CF39B0
int hkRecallAllocator::Header::getPayloadSize() const
{
    return m_payloadSize;
}

// 0x00D116F0
void* hkRecallAllocator::Header::getPayload()
{
    return (char*)this + HEADER_SIZE;
}

// 0x00CF46E0
const void* hkRecallAllocator::Header::getPayload() const
{
    return (const char*)this + HEADER_SIZE;
}

// 0x00CF39C0
const hkRecallAllocator::Header* hkRecallAllocator::Header::getNext() const
{
    return m_next;
}

// 0x00CF39D0
const hkRecallAllocator::Header* hkRecallAllocator::getHead() const
{
    return m_head;
}
