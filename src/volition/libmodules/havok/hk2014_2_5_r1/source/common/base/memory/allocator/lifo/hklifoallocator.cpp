// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/lifo/hklifoallocator.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): the stack (LIFO) allocator of a memory router. Blocks are rounded up to 16 bytes
// and cut from the current slab ([m_cur, m_end)). A block larger than a slab goes to the large allocator. When the
// slab is full a new one is taken: the first slab of the stack from the internal allocator, later ones from the slab
// allocator (counted as external), or the one cached when the stack last shrank out of a slab. Frees in LIFO order
// just move m_cur back; any other free is recorded as a non-LIFO free range (sorted by slab, then address, merged
// with neighbours) and given back when m_cur reaches it.
//
// Notes (decompiled behaviour, kept as is):
// - quit frees the first slab and the cached slab to the internal allocator, also when the cached slab came from the
//   slab allocator.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/lifo/hklifoallocator.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/util/hkmemutil.h"

#include <string.h>

// 0x00CEBAA0 (:39)
hkLifoAllocator::hkLifoAllocator()
{
    m_impl = nullptr;
}

// 0x00811AC0
hkLifoAllocator::~hkLifoAllocator()
{
}

// 0x00CEC2E0 (:33)
hkLifoAllocator::Implementation::Implementation()
{
    m_numExternalAllocations = 0;
}

// 0x00CEC340
hkLifoAllocator::Implementation::~Implementation()
{
}

// 0x00CEBAB0 (:46)
void hkLifoAllocator::init(hkMemoryAllocator* slabAllocator, hkMemoryAllocator* largeAllocator, hkMemoryAllocator* internalAllocator, int slabSize)
{
    m_impl = new (internalAllocator->blockAlloc(sizeof(Implementation))) Implementation();
    m_cur = nullptr;
    m_end = nullptr;
    m_firstNonLifoEnd = nullptr;
    m_cachedEmptySlab = nullptr;
    m_internalAllocator = internalAllocator;
    m_slabAllocator = slabAllocator;
    m_largeAllocator = largeAllocator;
    m_slabSize = slabSize;
}

// 0x00CEBCA0 (:61)
// allocators (optional) receives the slab, large and internal allocators.
void hkLifoAllocator::quit(hkMemoryAllocator** allocators)
{
    if (m_cur != nullptr)
    {
        m_internalAllocator->blockFree(m_impl->m_slabPtrs.m_data[0], m_slabSize);
    }
    if (m_cachedEmptySlab != nullptr)
    {
        m_internalAllocator->blockFree(m_cachedEmptySlab, m_slabSize); // see the notes
    }

    // the implementation's arrays (hkArray clearAndDeallocate on the internal allocator)
    hkArrayBase<void*>& slabs = m_impl->m_slabPtrs;
    slabs.m_size = 0;
    if (slabs.m_capacityAndFlags >= 0)
    {
        m_internalAllocator->bufFree(slabs.m_data, slabs.m_capacityAndFlags * (int)sizeof(void*));
    }
    slabs.m_data = nullptr;
    slabs.m_capacityAndFlags = hkArrayBase<void*>::DONT_DEALLOCATE_FLAG;
    hkArrayBase<Implementation::NonLifoFree>& frees = m_impl->m_nonLifoFrees;
    frees.m_size = 0;
    if (frees.m_capacityAndFlags >= 0)
    {
        m_internalAllocator->bufFree(frees.m_data, (frees.m_capacityAndFlags & hkArrayBase<void*>::CAPACITY_MASK) * (int)sizeof(Implementation::NonLifoFree));
    }
    frees.m_data = nullptr;
    frees.m_capacityAndFlags = hkArrayBase<void*>::DONT_DEALLOCATE_FLAG;

    m_internalAllocator->blockFree(m_impl, sizeof(Implementation));
    m_impl = nullptr;
    if (allocators != nullptr)
    {
        allocators[0] = m_slabAllocator;
        allocators[1] = m_largeAllocator;
        allocators[2] = m_internalAllocator;
    }
}

// 0x0080C220 (hklifoallocator.inl:10)
void* hkLifoAllocator::fastBlockAlloc(int numBytesIn)
{
    const int numBytes = (numBytesIn + 15) & ~15;
    char* cur = (char*)m_cur;
    char* next = cur + numBytes;
    if (numBytes <= m_slabSize && next <= (char*)m_end)
    {
        m_cur = next;
        return cur;
    }
    return allocateFromNewSlab(numBytes);
}

// 0x0080C250 (hklifoallocator.inl:28)
// The top block (unless a non-LIFO free range ends there) just moves m_cur back.
void hkLifoAllocator::fastBlockFree(void* p, int numBytesIn)
{
    const int numBytes = (numBytesIn + 15) & ~15;
    if (numBytesIn <= m_slabSize && (char*)p + numBytes == (char*)m_cur && m_firstNonLifoEnd != p)
    {
        m_cur = p;
        return;
    }
    slowBlockFree(p, numBytes);
}

// 0x00CEC000 (:95)
void* hkLifoAllocator::blockAlloc(int numBytesIn)
{
    return fastBlockAlloc(numBytesIn);
}

// 0x00CEC030 (:100)
void hkLifoAllocator::blockFree(void* p, int numBytesIn)
{
    fastBlockFree(p, numBytesIn);
}

// 0x00CEC070 (:105)
void* hkLifoAllocator::bufAlloc(int& reqNumBytesInOut)
{
    const int numBytes = (reqNumBytesInOut + 15) & ~15;
    reqNumBytesInOut = numBytes;
    char* cur = (char*)m_cur;
    char* next = cur + numBytes;
    if (numBytes <= m_slabSize && next <= (char*)m_end)
    {
        m_cur = next;
        return cur;
    }
    return allocateFromNewSlab(numBytes);
}

// 0x00CEC0B0 (:111)
void hkLifoAllocator::bufFree(void* p, int numBytes)
{
    fastBlockFree(p, numBytes);
}

// 0x00CEBB20 (:116)
// Grows the top block in place when it is the last one and the slab has room.
void* hkLifoAllocator::bufRealloc(void* pold, int oldNumBytes, int& reqNumBytesInOut)
{
    const int oldBytes = (oldNumBytes + 15) & ~15;
    const int newBytes = (reqNumBytesInOut + 15) & ~15;
    reqNumBytesInOut = newBytes;
    if ((char*)pold + oldBytes == (char*)m_cur && (char*)pold + newBytes <= (char*)m_end)
    {
        m_cur = (char*)pold + newBytes;
        return pold;
    }
    void* p = blockAlloc(newBytes);
    hkMemUtil::memCpy(p, pold, oldBytes < reqNumBytesInOut ? oldBytes : reqNumBytesInOut);
    blockFree(pold, oldBytes);
    return p;
}

// 0x00CEC310 (hklifoallocator.h:49)
void hkLifoAllocator::getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& u) const
{
}

// 0x00CEC320 (hklifoallocator.h:50)
int hkLifoAllocator::getAllocatedSize(const void* obj, int nbytes) const
{
    return nbytes;
}

// 0x00CEBBA0 (:137)
bool hkLifoAllocator::isEmpty() const
{
    return m_impl->m_nonLifoFrees.m_size == 0 && m_impl->m_slabPtrs.m_size < 2;
}

// 0x00CEBA80 (:142)
int hkLifoAllocator::numExternalAllocations() const
{
    return m_impl->m_numExternalAllocations;
}

// 0x00CEBD80 (:147)
// numBytes is already rounded to 16.
void* hkLifoAllocator::allocateFromNewSlab(int numBytes)
{
    if (numBytes > m_slabSize)
    {
        m_impl->m_numExternalAllocations++;
        return m_largeAllocator->blockAlloc(numBytes);
    }
    void* slab = m_cachedEmptySlab;
    if (slab == nullptr)
    {
        if (m_impl->m_slabPtrs.m_size < 2)
        {
            slab = m_internalAllocator->blockAlloc(m_slabSize);
        }
        else
        {
            m_impl->m_numExternalAllocations++;
            slab = m_slabAllocator->blockAlloc(m_slabSize);
        }
    }
    else
    {
        m_cachedEmptySlab = nullptr;
    }
    Implementation* impl = m_impl;
    if (impl->m_slabPtrs.m_size != 0)
    {
        // the rest of the current slab becomes a non-LIFO free range up to the new slab
        if (impl->m_nonLifoFrees.m_size == (impl->m_nonLifoFrees.m_capacityAndFlags & hkArrayBase<void*>::CAPACITY_MASK))
        {
            hkArrayUtil::_reserveMore(*m_internalAllocator, &impl->m_nonLifoFrees, sizeof(Implementation::NonLifoFree));
        }
        Implementation::NonLifoFree& f = impl->m_nonLifoFrees.m_data[impl->m_nonLifoFrees.m_size++];
        f.end = slab;
        f.start = m_cur;
        f.slabIndex = m_impl->m_slabPtrs.m_size - 1;
        m_firstNonLifoEnd = slab;
    }
    impl = m_impl;
    m_end = (char*)slab + m_slabSize;
    m_cur = (char*)slab + numBytes;
    if (impl->m_slabPtrs.m_size == (impl->m_slabPtrs.m_capacityAndFlags & hkArrayBase<void*>::CAPACITY_MASK))
    {
        hkArrayUtil::_reserveMore(*m_internalAllocator, &impl->m_slabPtrs, sizeof(void*));
    }
    impl->m_slabPtrs.m_data[impl->m_slabPtrs.m_size] = slab;
    impl->m_slabPtrs.m_size++;
    return slab;
}

// 0x00CEBBC0 (:202)
// m_cur moved back: absorbs the non-LIFO frees that now end at it, and releases slabs the stack left (the last one
// released is cached).
void hkLifoAllocator::popNonLifoFrees()
{
    char* cur = (char*)m_cur;
    for (int i = m_impl->m_nonLifoFrees.m_size - 1; i >= 0; i--)
    {
        const Implementation::NonLifoFree& f = m_impl->m_nonLifoFrees.m_data[i];
        if (f.end != cur)
        {
            break;
        }
        cur = (char*)f.start;
        m_impl->m_nonLifoFrees.m_size--;
    }
    while (m_impl->m_slabPtrs.m_size > 1)
    {
        char* top = (char*)m_impl->m_slabPtrs.m_data[m_impl->m_slabPtrs.m_size - 1];
        if (cur != top && (unsigned int)(cur - top) <= (unsigned int)m_slabSize)
        {
            break;
        }
        if (m_cachedEmptySlab != nullptr)
        {
            m_slabAllocator->blockFree(m_cachedEmptySlab, m_slabSize);
            m_impl->m_numExternalAllocations--;
        }
        m_cachedEmptySlab = m_impl->m_slabPtrs.m_data[m_impl->m_slabPtrs.m_size - 1];
        m_impl->m_slabPtrs.m_size--;
    }
    m_cur = cur;
    Implementation* impl = m_impl;
    m_end = impl->m_slabPtrs.m_size == 0 ? nullptr : (char*)impl->m_slabPtrs.m_data[impl->m_slabPtrs.m_size - 1] + m_slabSize;
    m_firstNonLifoEnd = impl->m_nonLifoFrees.m_size == 0 ? nullptr : impl->m_nonLifoFrees.m_data[impl->m_nonLifoFrees.m_size - 1].end;
}

// 0x00CEBFB0 (:302)
// numBytesIn is already rounded to 16 by the callers.
void hkLifoAllocator::slowBlockFree(void* p, int numBytesIn)
{
    if (p == nullptr)
    {
        return;
    }
    if (numBytesIn > m_slabSize)
    {
        m_impl->m_numExternalAllocations--;
        m_largeAllocator->blockFree(p, numBytesIn);
        return;
    }
    const int nbytes = (numBytesIn + 15) & ~15;
    if ((char*)p + nbytes == (char*)m_cur)
    {
        m_cur = p;
        popNonLifoFrees();
        return;
    }
    insertNonLifoFree(p, nbytes);
}

// 0x00CEBE70 (:241)
// Records [pstart, pstart + nbytes): merged with a range of the same slab it touches, otherwise inserted so the
// ranges stay ordered by slab, then start.
void hkLifoAllocator::insertNonLifoFree(void* pstart, int nbytes)
{
    Implementation* impl = m_impl;
    int slabIndex = -1;
    for (int i = impl->m_slabPtrs.m_size - 1; i >= 0; i--)
    {
        if ((unsigned int)((char*)pstart - (char*)impl->m_slabPtrs.m_data[i]) < (unsigned int)m_slabSize)
        {
            slabIndex = i;
            break;
        }
    }
    void* pend = (char*)pstart + nbytes;
    int insertAt = 0;
    for (int i = impl->m_nonLifoFrees.m_size - 1; i >= 0; i--)
    {
        Implementation::NonLifoFree& f = impl->m_nonLifoFrees.m_data[i];
        if (f.slabIndex == slabIndex)
        {
            if (pstart == f.end)
            {
                f.end = pend;
                goto done;
            }
            if (pend == f.start)
            {
                f.start = pstart;
                goto done;
            }
            if (f.start < pstart)
            {
                insertAt = i + 1;
                break;
            }
        }
        else if (f.slabIndex < slabIndex)
        {
            insertAt = i + 1;
            break;
        }
    }
    {
        // hkArray::insertAt (capacity doubles, at least size + 1)
        const int size = impl->m_nonLifoFrees.m_size;
        const int capacity = impl->m_nonLifoFrees.m_capacityAndFlags & hkArrayBase<void*>::CAPACITY_MASK;
        if (capacity < size + 1)
        {
            const int newCapacity = capacity * 2 > size + 1 ? capacity * 2 : size + 1;
            hkArrayUtil::_reserve(*m_internalAllocator, &impl->m_nonLifoFrees, newCapacity, sizeof(Implementation::NonLifoFree));
        }
        Implementation::NonLifoFree* at = impl->m_nonLifoFrees.m_data + insertAt;
        hkMemUtil::memMove(at + 1, at, (size - insertAt) * (int)sizeof(Implementation::NonLifoFree));
        impl->m_nonLifoFrees.m_size = size + 1;
        at = impl->m_nonLifoFrees.m_data + insertAt;
        at->start = pstart;
        at->end = pend;
        at->slabIndex = slabIndex;
    }
done:
    impl = m_impl;
    m_firstNonLifoEnd = impl->m_nonLifoFrees.m_size == 0 ? nullptr : impl->m_nonLifoFrees.m_data[impl->m_nonLifoFrees.m_size - 1].end;
}
