// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermultimap/hkmultimap.cxx
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SC): the hkMultiMap template functions (included by hkmultimap.h), as the
// 2014.1 SDK's hkMultiMap.cxx; checked against the <unsigned long, unsigned long> instance rfg.exe keeps (the
// hkTypeManager type map). Keys -1 mark free slots; collisions probe linearly; a removal shifts the rest of its run up.
#pragma once

// :32 (<ulong> 0x00FC2240)
// A 16-slot table (0x80 bytes).
template<typename KEY, typename VAL, typename OPS, typename ALLOC>
hkMultiMap<KEY, VAL, OPS, ALLOC>::hkMultiMap()
{
    const int initialCapacity = 16;
    hkMemoryAllocator& alloc = ALLOC().get(this);
    m_elem = (Pair*)alloc.blockAlloc((int)sizeof(Pair) * initialCapacity);
    m_numElems = 0;
    m_hashMod = initialCapacity - 1;
    if (m_elem)
    {
        clear();
    }
}

// :50 (<ulong> 0x00FC21E0)
// Three slots per expected element, rounded up to a power of two (at least 4).
template<typename KEY, typename VAL, typename OPS, typename ALLOC>
hkMultiMap<KEY, VAL, OPS, ALLOC>::hkMultiMap(int numElements)
    : m_numElems(0)
{
    numElements *= 3;
    int size = 4;
    while (size < numElements)
    {
        size *= 2;
    }
    hkMemoryAllocator& alloc = ALLOC().get(this);
    m_elem = (Pair*)alloc.blockAlloc((int)sizeof(Pair) * size);
    m_hashMod = size - 1;
    clear();
}

// :70 (<ulong> 0x00FC1400)
template<typename KEY, typename VAL, typename OPS, typename ALLOC>
int hkMultiMap<KEY, VAL, OPS, ALLOC>::findNumEntries(KEY key) const
{
    int numFound = 0;
    for (unsigned int i = OPS::hash(key, m_hashMod); OPS::isValid(m_elem[i].key); i = (i + 1) & m_hashMod)
    {
        if (OPS::equal(m_elem[i].key, key))
        {
            numFound++;
        }
    }
    return numFound;
}

// :86 (<ulong> 0x00FC13B0)
template<typename KEY, typename VAL, typename OPS, typename ALLOC>
int hkMultiMap<KEY, VAL, OPS, ALLOC>::findNumEntries(KEY key, VAL val) const
{
    int numFound = 0;
    for (unsigned int i = OPS::hash(key, m_hashMod); OPS::isValid(m_elem[i].key); i = (i + 1) & m_hashMod)
    {
        if (OPS::equal(m_elem[i].key, key) && m_elem[i].val == val)
        {
            numFound++;
        }
    }
    return numFound;
}

// :102 (<ulong> 0x00FC2280)
// Each removal restarts the scan at the hash (the run has shifted).
template<typename KEY, typename VAL, typename OPS, typename ALLOC>
int hkMultiMap<KEY, VAL, OPS, ALLOC>::removeAll(KEY key)
{
    int numRemoved = 0;
    const unsigned int hash = OPS::hash(key, m_hashMod);
    unsigned int i = hash;
    while (OPS::isValid(m_elem[i].key))
    {
        if (OPS::equal(m_elem[i].key, key))
        {
            remove(HK_MULTI_MAP_INDEX_TO_ITERATOR(i));
            numRemoved++;
            i = hash;
        }
        else
        {
            i = (i + 1) & m_hashMod;
        }
    }
    return numRemoved;
}

// :131 (<ulong> 0x00FC1340)
// A table over caller storage (not freed by the map).
template<typename KEY, typename VAL, typename OPS, typename ALLOC>
hkMultiMap<KEY, VAL, OPS, ALLOC>::hkMultiMap(void* ptr, int sizeInBytes)
{
    const int maxKeys = (int)((unsigned int)sizeInBytes / sizeof(Pair));
    m_elem = static_cast<Pair*>(ptr);
    m_numElems = DONT_DEALLOCATE_FLAG;
    m_hashMod = maxKeys - 1;
    for (int i = 0; i < maxKeys; i++)
    {
        OPS::invalidate(m_elem[i].key);
    }
}

// :148
template<typename KEY, typename VAL, typename OPS, typename ALLOC>
void hkMultiMap<KEY, VAL, OPS, ALLOC>::clear()
{
    const int initialCapacity = m_hashMod + 1;
    for (int i = 0; i < initialCapacity; i++)
    {
        OPS::invalidate(m_elem[i].key);
    }
    m_numElems = 0 | (m_numElems & static_cast<int>(DONT_DEALLOCATE_FLAG));
}

// :159
template<typename KEY, typename VAL, typename OPS, typename ALLOC>
void hkMultiMap<KEY, VAL, OPS, ALLOC>::swap(hkMultiMap& other)
{
    Pair* te = m_elem;
    const int tn = m_numElems;
    const int th = m_hashMod;
    m_elem = other.m_elem;
    m_numElems = other.m_numElems;
    m_hashMod = other.m_hashMod;
    other.m_elem = te;
    other.m_numElems = tn;
    other.m_hashMod = th;
}

// :173 (<ulong> 0x00FC1380)
template<typename KEY, typename VAL, typename OPS, typename ALLOC>
hkMultiMap<KEY, VAL, OPS, ALLOC>::~hkMultiMap()
{
    if ((m_numElems & DONT_DEALLOCATE_FLAG) == 0)
    {
        hkMemoryAllocator& alloc = ALLOC().get(this);
        alloc.blockFree(m_elem, (int)sizeof(Pair) * (m_hashMod + 1));
    }
}

// :183 (<ulong> 0x00FC1440)
// The table doubles when it is half full (the flag bit of m_numElems drops out of the doubled count).
template<typename KEY, typename VAL, typename OPS, typename ALLOC>
void hkMultiMap<KEY, VAL, OPS, ALLOC>::insert(KEY key, VAL val)
{
    const int numElems = m_numElems & static_cast<int>(NUM_ELEMS_MASK);
    if ((numElems + numElems) > m_hashMod)
    {
        resizeTable(m_hashMod + m_hashMod + 2);
    }
    unsigned int i;
    for (i = OPS::hash(key, m_hashMod); OPS::isValid(m_elem[i].key); i = (i + 1) & m_hashMod)
    {
    }
    m_numElems++;
    m_elem[i].key = key;
    m_elem[i].val = val;
}

// :211 (<ulong> 0x00FC22E0)
template<typename KEY, typename VAL, typename OPS, typename ALLOC>
hkResult hkMultiMap<KEY, VAL, OPS, ALLOC>::tryInsert(KEY key, VAL val)
{
    const int numElems = m_numElems & static_cast<int>(NUM_ELEMS_MASK);
    if ((numElems + numElems) > m_hashMod)
    {
        if (resizeTable(m_hashMod + m_hashMod + 2) != HK_SUCCESS)
        {
            return HK_FAILURE;
        }
    }
    unsigned int i;
    for (i = OPS::hash(key, m_hashMod); OPS::isValid(m_elem[i].key); i = (i + 1) & m_hashMod)
    {
    }
    m_numElems++;
    m_elem[i].key = key;
    m_elem[i].val = val;
    return HK_SUCCESS;
}

// :245 (<ulong> 0x00FC14B0)
template<typename KEY, typename VAL, typename OPS, typename ALLOC>
typename hkMultiMap<KEY, VAL, OPS, ALLOC>::Iterator hkMultiMap<KEY, VAL, OPS, ALLOC>::findKey(KEY key) const
{
    for (unsigned int i = OPS::hash(key, m_hashMod); OPS::isValid(m_elem[i].key); i = (i + 1) & m_hashMod)
    {
        if (OPS::equal(m_elem[i].key, key))
        {
            return HK_MULTI_MAP_INDEX_TO_ITERATOR(i);
        }
    }
    return HK_MULTI_MAP_INDEX_TO_ITERATOR(m_hashMod + 1);
}

// :260 (<ulong> 0x00FC14F0)
template<typename KEY, typename VAL, typename OPS, typename ALLOC>
typename hkMultiMap<KEY, VAL, OPS, ALLOC>::Iterator hkMultiMap<KEY, VAL, OPS, ALLOC>::findKeyWithValue(KEY key, VAL val) const
{
    for (unsigned int i = OPS::hash(key, m_hashMod); OPS::isValid(m_elem[i].key); i = (i + 1) & m_hashMod)
    {
        const Pair& pair = m_elem[i];
        if (OPS::equal(pair.key, key) && pair.val == val)
        {
            return HK_MULTI_MAP_INDEX_TO_ITERATOR(i);
        }
    }
    return HK_MULTI_MAP_INDEX_TO_ITERATOR(m_hashMod + 1);
}

// :277 (<ulong> 0x00FC2370)
// Original: the growth test uses m_numElems with its flag bit (as the SDK).
template<typename KEY, typename VAL, typename OPS, typename ALLOC>
typename hkMultiMap<KEY, VAL, OPS, ALLOC>::Iterator hkMultiMap<KEY, VAL, OPS, ALLOC>::findOrInsertKey(KEY key, VAL val)
{
    if ((m_numElems + 1 + m_numElems + 1) > m_hashMod)
    {
        resizeTable(m_hashMod + m_hashMod + 2);
    }
    for (unsigned int i = OPS::hash(key, m_hashMod);; i = (i + 1) & m_hashMod)
    {
        if (OPS::equal(m_elem[i].key, key))
        {
            return HK_MULTI_MAP_INDEX_TO_ITERATOR(i);
        }
        else if (!OPS::isValid(m_elem[i].key))
        {
            m_elem[i].key = key;
            m_elem[i].val = val;
            m_numElems += 1;
            return HK_MULTI_MAP_INDEX_TO_ITERATOR(i);
        }
    }
}

// :309
template<typename KEY, typename VAL, typename OPS, typename ALLOC>
hkResult hkMultiMap<KEY, VAL, OPS, ALLOC>::get(KEY key, VAL* out) const
{
    Iterator it = findKey(key);
    if (isValid(it))
    {
        *out = getValue(it);
        return HK_SUCCESS;
    }
    return HK_FAILURE;
}

// :321 (<ulong> 0x00FC1530)
template<typename KEY, typename VAL, typename OPS, typename ALLOC>
VAL hkMultiMap<KEY, VAL, OPS, ALLOC>::getWithDefault(KEY key, VAL def) const
{
    for (unsigned int i = OPS::hash(key, m_hashMod); OPS::isValid(m_elem[i].key); i = (i + 1) & m_hashMod)
    {
        if (OPS::equal(m_elem[i].key, key))
        {
            return m_elem[i].val;
        }
    }
    return def;
}

// :336
// Closes the gap: walks the rest of the run and moves up every entry whose hash does not lie in (empty, i].
template<typename KEY, typename VAL, typename OPS, typename ALLOC>
void hkMultiMap<KEY, VAL, OPS, ALLOC>::remove(Iterator it)
{
    unsigned int i = HK_MULTI_MAP_ITERATOR_TO_INDEX(it);
    --m_numElems;
    OPS::invalidate(m_elem[i].key);

    // find the lowest element of this unbroken run
    unsigned int lo = (i + m_hashMod) & m_hashMod;
    while (OPS::isValid(m_elem[lo].key))
    {
        lo = (lo + m_hashMod) & m_hashMod;
    }
    lo = (lo + 1) & m_hashMod;

    unsigned int empty = i;
    for (i = (i + 1) & m_hashMod; OPS::isValid(m_elem[i].key); i = (i + 1) & m_hashMod)
    {
        const unsigned int hash = OPS::hash(m_elem[i].key, m_hashMod);
        // 'normal' (lo <= empty < i), 'i wrapped' and 'i and empty wrapped'
        if ((i >= lo) && (hash > empty))
        {
            continue;
        }
        else if ((i < empty) && (hash > empty || hash <= i))
        {
            continue;
        }
        else if (hash > empty && hash < lo)
        {
            continue;
        }
        m_elem[empty].key = m_elem[i].key;
        m_elem[empty].val = m_elem[i].val;
        OPS::invalidate(m_elem[i].key);
        empty = i;
    }
}

// :403
template<typename KEY, typename VAL, typename OPS, typename ALLOC>
hkResult hkMultiMap<KEY, VAL, OPS, ALLOC>::remove(KEY key)
{
    Iterator it = findKey(key);
    if (isValid(it))
    {
        remove(it);
        return HK_SUCCESS;
    }
    return HK_FAILURE;
}

// :415
template<typename KEY, typename VAL, typename OPS, typename ALLOC>
hkResult hkMultiMap<KEY, VAL, OPS, ALLOC>::remove(KEY key, VAL val)
{
    Iterator it = findKeyWithValue(key, val);
    if (isValid(it))
    {
        remove(it);
        return HK_SUCCESS;
    }
    return HK_FAILURE;
}

// :427
template<typename KEY, typename VAL, typename OPS, typename ALLOC>
void hkMultiMap<KEY, VAL, OPS, ALLOC>::reserve(int numElements)
{
    numElements *= 3;
    int size = 4;
    while (size < numElements)
    {
        size *= 2;
    }
    resizeTable(size);
}

// :441
// Rehashes into a fresh table (insert counts the elements again); caller storage is not freed.
template<typename KEY, typename VAL, typename OPS, typename ALLOC>
hkResult hkMultiMap<KEY, VAL, OPS, ALLOC>::resizeTable(int newcap)
{
    const int dontDeallocate = m_numElems & static_cast<int>(DONT_DEALLOCATE_FLAG);
    const int oldcap = m_hashMod + 1;
    Pair* oldelem = m_elem;

    hkMemoryAllocator& alloc = ALLOC().get(this);
    Pair* newelem = (Pair*)alloc.blockAlloc((int)sizeof(Pair) * newcap);
    if (newelem == nullptr)
    {
        return HK_FAILURE;
    }
    m_elem = newelem;
    for (int i = 0; i < newcap; i++)
    {
        OPS::invalidate(m_elem[i].key);
    }
    m_numElems = 0;
    m_hashMod = newcap - 1;
    for (int i = 0; i < oldcap; ++i)
    {
        if (OPS::isValid(oldelem[i].key))
        {
            insert(oldelem[i].key, oldelem[i].val);
        }
    }
    if (dontDeallocate == 0)
    {
        alloc.blockFree(oldelem, (int)sizeof(Pair) * oldcap);
    }
    return HK_SUCCESS;
}
