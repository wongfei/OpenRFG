// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkmap.cxx
//
// Havok 2014.2.5 (havok_impl.md HK1): the hkMapBase template functions (included by hkmap.h). Instances:
// <unsigned long, unsigned long> 0x00D0D100.. (hkDefaultError), <unsigned __int64, unsigned __int64> 0x00D0D540..,
// <hkpPairCollisionFilter::PairFilterKey, unsigned __int64> 0x00DEE4C0.. (HK4).
#pragma once

// :36 (<u64> 0x00D0DD20)
template<typename KEY, typename VAL, typename OPS>
hkMapBase<KEY, VAL, OPS>::hkMapBase(void* ptr, int sizeInBytes)
{
    init(ptr, sizeInBytes);
}

// :84 (<ulong> 0x00D0CDF0, <u64> 0x00D0CEC0)
// The storage belongs to the allocator-aware owner (hkMap's destructor frees it).
template<typename KEY, typename VAL, typename OPS>
hkMapBase<KEY, VAL, OPS>::~hkMapBase()
{
}

// :42 (<u64> 0x00D0D8B0)
// A table over caller storage (not freed by the map).
template<typename KEY, typename VAL, typename OPS>
void hkMapBase<KEY, VAL, OPS>::init(void* ptr, int sizeInBytes)
{
    const int numSlots = (int)((unsigned int)sizeInBytes / sizeof(Pair));
    m_elem = (Pair*)ptr;
    m_numElems = DONT_DEALLOCATE_FLAG;
    m_hashMod = numSlots - 1;
    for (int i = 0; i < numSlots; i++)
    {
        OPS::invalidate(m_elem[i].key);
    }
}

// :59 (<ulong> 0x00D0D350, <u64> 0x00D0D840)
template<typename KEY, typename VAL, typename OPS>
void hkMapBase<KEY, VAL, OPS>::clear()
{
    const int capacity = m_hashMod + 1;
    for (int i = 0; i < capacity; i++)
    {
        OPS::invalidate(m_elem[i].key);
    }
    m_numElems &= DONT_DEALLOCATE_FLAG;
}

// :70 (<ulong> 0x00D0D380, <u64> 0x00D0D870)
template<typename KEY, typename VAL, typename OPS>
void hkMapBase<KEY, VAL, OPS>::clearAndDeallocate(hkMemoryAllocator& alloc)
{
    clear();
    if ((m_numElems & DONT_DEALLOCATE_FLAG) == 0)
    {
        alloc.blockFree(m_elem, (m_hashMod + 1) * (int)sizeof(Pair));
    }
    m_elem = nullptr;
    m_numElems = 0;
    m_hashMod = -1;
}

// :90 (<ulong> 0x00D0D100, <u64> 0x00D0D540)
// The table doubles when it is half full (the flag bit of m_numElems drops out of the doubled count).
template<typename KEY, typename VAL, typename OPS>
int hkMapBase<KEY, VAL, OPS>::insert(hkMemoryAllocator& alloc, KEY key, VAL val)
{
    if (m_hashMod < m_numElems * 2)
    {
        resizeTable(alloc, m_hashMod * 2 + 2);
    }
    Pair* elem = m_elem;
    unsigned int i = OPS::hash(key, (unsigned int)m_hashMod);
    int isNew = 1;
    for (;;)
    {
        if (!OPS::isValid(elem[i].key))
        {
            break;
        }
        if (OPS::equal(elem[i].key, key))
        {
            isNew = 0;
            break;
        }
        i = (i + 1) & (unsigned int)m_hashMod;
    }
    m_numElems += isNew;
    elem[i].key = key;
    m_elem[i].val = val;
    return isNew;
}

// (<ulong> 0x00D0DC30)
// The slot of key, inserted with val when it is new (the table grows first, as insert's does).
template<typename KEY, typename VAL, typename OPS>
typename hkMapBase<KEY, VAL, OPS>::Iterator hkMapBase<KEY, VAL, OPS>::findOrInsertKey(hkMemoryAllocator& alloc, KEY key,
                                                                                     VAL val)
{
    if (m_hashMod < m_numElems * 2)
    {
        resizeTable(alloc, m_hashMod * 2 + 2);
    }
    Pair* elem = m_elem;
    unsigned int i = OPS::hash(key, (unsigned int)m_hashMod);
    if (!OPS::equal(elem[i].key, key))
    {
        while (OPS::isValid(elem[i].key))
        {
            i = (i + 1) & (unsigned int)m_hashMod;
            if (OPS::equal(elem[i].key, key))
            {
                return (Iterator)i;
            }
        }
        elem[i].key = key;
        m_elem[i].val = val;
        m_numElems += 1;
    }
    return (Iterator)i;
}

// :152 (<ulong> 0x00D0D180, <u64> 0x00D0D5E0)
template<typename KEY, typename VAL, typename OPS>
typename hkMapBase<KEY, VAL, OPS>::Iterator hkMapBase<KEY, VAL, OPS>::findKey(KEY key) const
{
    const unsigned int mod = (unsigned int)m_hashMod;
    if (m_hashMod > 0)
    {
        for (unsigned int i = OPS::hash(key, mod); OPS::isValid(m_elem[i].key); i = (i + 1) & mod)
        {
            if (OPS::equal(m_elem[i].key, key))
            {
                return (Iterator)i;
            }
        }
    }
    return (Iterator)(mod + 1);
}

// :215 (<ulong> 0x00D0D1C0, <u64> 0x00D0D630)
template<typename KEY, typename VAL, typename OPS>
VAL hkMapBase<KEY, VAL, OPS>::getWithDefault(KEY key, VAL def) const
{
    const unsigned int mod = (unsigned int)m_hashMod;
    if (m_hashMod > 0)
    {
        for (unsigned int i = OPS::hash(key, mod); OPS::isValid(m_elem[i].key); i = (i + 1) & mod)
        {
            if (OPS::equal(m_elem[i].key, key))
            {
                return m_elem[i].val;
            }
        }
    }
    return def;
}

// <ulong> 0x00D0D210 (stub_impl.md S4 SE): the value of a present key
template<typename KEY, typename VAL, typename OPS>
hkResult hkMapBase<KEY, VAL, OPS>::get(KEY key, VAL* out) const
{
    Iterator it = findKey(key);
    if ((int)it <= m_hashMod)
    {
        *out = m_elem[it].val;
        return HK_SUCCESS;
    }
    return HK_FAILURE;
}

// :239 (<ulong> 0x00D0D250, <u64> 0x00D0D6E0)
// Frees the slot, then moves back every entry of the following run whose home slot does not lie cyclically in
// (hole, entry]; the run is bounded by the first free slot before and after the hole.
template<typename KEY, typename VAL, typename OPS>
void hkMapBase<KEY, VAL, OPS>::remove(Iterator it)
{
    unsigned int hole = (unsigned int)it;
    m_numElems--;
    OPS::invalidate(m_elem[hole].key);
    unsigned int mod = (unsigned int)m_hashMod;
    // the start of the run (the slot after the free slot before the hole)
    unsigned int lo = (hole + mod) & mod;
    while (OPS::isValid(m_elem[lo].key))
    {
        lo = (lo + mod) & mod;
    }
    lo = (lo + 1) & mod;
    for (unsigned int j = (hole + 1) & mod; OPS::isValid(m_elem[j].key); j = (j + 1) & mod)
    {
        const unsigned int home = OPS::hash(m_elem[j].key, mod);
        if ((j < lo || home <= hole) && (hole <= j || (home <= hole && j < home)) && (home <= hole || lo <= home))
        {
            m_elem[hole].key = m_elem[j].key;
            m_elem[hole].val = m_elem[j].val;
            OPS::invalidate(m_elem[j].key);
            hole = j;
        }
        mod = (unsigned int)m_hashMod;
    }
}

// :307 (<ulong> 0x00D0DCB0)
template<typename KEY, typename VAL, typename OPS>
hkResult hkMapBase<KEY, VAL, OPS>::remove(KEY key)
{
    const Iterator it = findKey(key);
    if (it <= m_hashMod)
    {
        remove(it);
        return hkResult(HK_SUCCESS);
    }
    return hkResult(HK_FAILURE);
}

// :319 (<ulong> 0x00D0DCF0)
// Room for numElements at most half full (8 slots at least).
template<typename KEY, typename VAL, typename OPS>
void hkMapBase<KEY, VAL, OPS>::reserve(hkMemoryAllocator& alloc, int numElements)
{
    int newcap = 8;
    while (newcap < numElements * 2)
    {
        newcap *= 2;
    }
    resizeTable(alloc, newcap);
}

// :331 (<ulong> 0x00D0D410, <u64> 0x00D0D8F0)
template<typename KEY, typename VAL, typename OPS>
hkResult hkMapBase<KEY, VAL, OPS>::resizeTable(hkMemoryAllocator& alloc, int newcap)
{
    if (newcap < 9)
    {
        newcap = 8; // the smallest table
    }
    const int oldNumElems = m_numElems;
    Pair* oldElem = m_elem;
    const int oldCapacity = m_hashMod + 1;
    Pair* elem = (Pair*)alloc.blockAlloc(newcap * (int)sizeof(Pair));
    if (elem == nullptr)
    {
        return hkResult(HK_FAILURE);
    }
    m_elem = elem;
    for (int i = 0; i < newcap; i++)
    {
        OPS::invalidate(m_elem[i].key);
    }
    m_hashMod = newcap - 1;
    m_numElems = 0;
    for (int i = 0; i < oldCapacity; i++)
    {
        if (OPS::isValid(oldElem[i].key))
        {
            insert(alloc, oldElem[i].key, oldElem[i].val);
        }
    }
    if ((oldNumElems & DONT_DEALLOCATE_FLAG) == 0)
    {
        alloc.blockFree(oldElem, oldCapacity * (int)sizeof(Pair));
    }
    return hkResult(HK_SUCCESS);
}

// :413 (<u64> 0x00D0CF80)
template<typename KEY, typename VAL, typename OPS>
int hkMapBase<KEY, VAL, OPS>::getSizeInBytesFor(int numOfKeys)
{
    int capacity = 8;
    while (capacity < numOfKeys * 2)
    {
        capacity *= 2;
    }
    return capacity * (int)sizeof(Pair);
}
