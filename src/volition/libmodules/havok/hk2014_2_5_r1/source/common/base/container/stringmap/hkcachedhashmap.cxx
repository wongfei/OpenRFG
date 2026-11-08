// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkcachedhashmap.cxx
//
// The hkCachedHashMap template functions (stub_impl.md S4, serializer SA), as the 2014.1 SDK has them; the addresses
// are the instance <hkStringMapOperations,hkContainerHeapAllocator> in rfg.exe. Included by hkcachedhashmap.h.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"

// the hash of a free slot
#define HK_CACHED_HASH_EMPTY hkUlong(-1)

// 0x00D13FE0: 16 free slots
template<typename Operations, typename Allocator>
hkCachedHashMap<Operations, Allocator>::hkCachedHashMap(Operations ops)
    : m_ops(ops)
{
    const int initialCapacity = 16;
    m_elem = static_cast<Elem*>(Allocator().get(this).blockAlloc(initialCapacity * (int)sizeof(Elem)));
    hkString::memSet(m_elem, 0xFF, initialCapacity * (int)sizeof(Elem));
    m_numElems = 0;
    m_hashMod = initialCapacity - 1;
}

// 0x00D14020
template<typename Operations, typename Allocator>
hkCachedHashMap<Operations, Allocator>::~hkCachedHashMap()
{
    Allocator().get(this).blockFree(m_elem, (m_hashMod + 1) * (int)sizeof(Elem));
}

// 0x00D13BB0: the first used slot
template<typename Operations, typename Allocator>
typename hkCachedHashMap<Operations, Allocator>::Iterator hkCachedHashMap<Operations, Allocator>::getIterator() const
{
    int i;
    for (i = 0; i <= m_hashMod; i++)
    {
        if (m_elem[i].hash != HK_CACHED_HASH_EMPTY)
        {
            break;
        }
    }
    return reinterpret_cast<Iterator>(hkUlong(i));
}

// 0x00D13BD0
template<typename Operations, typename Allocator>
hkUlong hkCachedHashMap<Operations, Allocator>::getKey(Iterator it) const
{
    return m_elem[reinterpret_cast<hkUlong>(it)].key;
}

// 0x00D13BE0
template<typename Operations, typename Allocator>
hkUlong hkCachedHashMap<Operations, Allocator>::getValue(Iterator it) const
{
    return m_elem[reinterpret_cast<hkUlong>(it)].value;
}

// 0x00D13BF0
template<typename Operations, typename Allocator>
void hkCachedHashMap<Operations, Allocator>::setValue(Iterator it, hkUlong val)
{
    m_elem[reinterpret_cast<hkUlong>(it)].value = val;
}

// 0x00D13C10: the next used slot
template<typename Operations, typename Allocator>
typename hkCachedHashMap<Operations, Allocator>::Iterator hkCachedHashMap<Operations, Allocator>::getNext(Iterator it) const
{
    int i = static_cast<int>(reinterpret_cast<hkUlong>(it));
    for (i += 1; i <= m_hashMod; i++)
    {
        if (m_elem[i].hash != HK_CACHED_HASH_EMPTY)
        {
            break;
        }
    }
    return reinterpret_cast<Iterator>(hkUlong(i));
}

// 0x00D14040
template<typename Operations, typename Allocator>
hkBool hkCachedHashMap<Operations, Allocator>::isValid(Iterator it) const
{
    return static_cast<int>(reinterpret_cast<hkUlong>(it)) <= m_hashMod;
}

// 0x00D13C40: an equal key gets the new value; the table grows first when more than half full
template<typename Operations, typename Allocator>
void hkCachedHashMap<Operations, Allocator>::insert(hkUlong key, hkUlong val)
{
    const hkUlong hash = m_ops.hash(key);
    if (m_numElems + m_numElems > m_hashMod)
    {
        resizeTable(m_hashMod + m_hashMod + 2);
    }
    hkUlong i = hash & m_hashMod;
    while (true)
    {
        if (m_elem[i].hash == HK_CACHED_HASH_EMPTY)
        {
            m_numElems += 1;
            break;
        }
        if (m_elem[i].hash == hash && m_ops.equal(key, m_elem[i].key))
        {
            break; // overwrite
        }
        i = (i + 1) & m_hashMod;
    }
    m_elem[i].hash = hash;
    m_elem[i].key = key;
    m_elem[i].value = val;
}

// 0x00D13D00
template<typename Operations, typename Allocator>
typename hkCachedHashMap<Operations, Allocator>::Iterator hkCachedHashMap<Operations, Allocator>::findKey(hkUlong key) const
{
    const hkUlong hash = m_ops.hash(key);
    for (hkUlong i = hash & m_hashMod; m_elem[i].hash != HK_CACHED_HASH_EMPTY; i = (i + 1) & m_hashMod)
    {
        if (m_elem[i].hash == hash && m_ops.equal(key, m_elem[i].key))
        {
            return reinterpret_cast<Iterator>(i);
        }
    }
    return reinterpret_cast<Iterator>(hkUlong(m_hashMod + 1));
}

// 0x00D142E0
template<typename Operations, typename Allocator>
typename hkCachedHashMap<Operations, Allocator>::Iterator hkCachedHashMap<Operations, Allocator>::findOrInsertKey(hkUlong key, hkUlong value)
{
    if (m_numElems + m_numElems > m_hashMod)
    {
        resizeTable(m_hashMod + m_hashMod + 2);
    }
    const hkUlong hash = m_ops.hash(key);
    for (hkUlong i = hash & m_hashMod;; i = (i + 1) & m_hashMod)
    {
        if (m_elem[i].hash == hash && m_ops.equal(key, m_elem[i].key))
        {
            return reinterpret_cast<Iterator>(i);
        }
        if (m_elem[i].hash == HK_CACHED_HASH_EMPTY)
        {
            m_elem[i].hash = hash;
            m_elem[i].key = key;
            m_elem[i].value = value;
            m_numElems += 1;
            return reinterpret_cast<Iterator>(i);
        }
    }
}

// 0x00D140B0
template<typename Operations, typename Allocator>
hkResult hkCachedHashMap<Operations, Allocator>::get(hkUlong key, hkUlong* out) const
{
    Iterator it = findKey(key);
    if (isValid(it))
    {
        *out = getValue(it);
        return HK_SUCCESS;
    }
    return HK_FAILURE;
}

// 0x00D14060
template<typename Operations, typename Allocator>
hkUlong hkCachedHashMap<Operations, Allocator>::getOrInsert(hkUlong key, hkUlong ifNotFound)
{
    Iterator it = findKey(key);
    if (isValid(it))
    {
        return getValue(it);
    }
    insert(key, ifNotFound);
    return ifNotFound;
}

// 0x00D14390
template<typename Operations, typename Allocator>
hkUlong hkCachedHashMap<Operations, Allocator>::getWithDefault(hkUlong key, hkUlong def) const
{
    hkUlong ret = def;
    get(key, &ret);
    return ret;
}

// 0x00D13D80: the following entries of the run move back into the hole when their home slot allows it
template<typename Operations, typename Allocator>
void hkCachedHashMap<Operations, Allocator>::remove(Iterator it)
{
    hkUlong i = reinterpret_cast<hkUlong>(it);
    --m_numElems;
    m_elem[i].hash = HK_CACHED_HASH_EMPTY;

    // the start of the run
    hkUlong lo = (i + m_hashMod) & m_hashMod;
    while (m_elem[lo].hash != HK_CACHED_HASH_EMPTY)
    {
        lo = (lo + m_hashMod) & m_hashMod;
    }
    lo = (lo + 1) & m_hashMod;

    hkUlong empty = i;
    for (i = (i + 1) & m_hashMod; m_elem[i].hash != HK_CACHED_HASH_EMPTY; i = (i + 1) & m_hashMod)
    {
        const hkUlong hmod = m_elem[i].hash & m_hashMod;
        if (i >= lo && hmod > empty)
        {
            continue;
        }
        if (i < empty && (hmod > empty || hmod <= i))
        {
            continue;
        }
        if (hmod > empty && hmod < lo)
        {
            continue;
        }
        m_elem[empty] = m_elem[i];
        m_elem[i].hash = HK_CACHED_HASH_EMPTY;
        empty = i;
    }
}

// 0x00D14100
template<typename Operations, typename Allocator>
hkResult hkCachedHashMap<Operations, Allocator>::remove(hkUlong key)
{
    Iterator it = findKey(key);
    if (isValid(it))
    {
        remove(it);
        return HK_SUCCESS;
    }
    return HK_FAILURE;
}

// 0x00D141C0: a new table, the entries reinserted
template<typename Operations, typename Allocator>
void hkCachedHashMap<Operations, Allocator>::resizeTable(int newcap)
{
    const int oldcap = m_hashMod + 1;
    Elem* oldelem = m_elem;
    hkMemoryAllocator& mem = Allocator().get(this);
    m_elem = static_cast<Elem*>(mem.blockAlloc(newcap * (int)sizeof(Elem)));
    hkString::memSet(m_elem, 0xFF, newcap * (int)sizeof(Elem));
    m_numElems = 0;
    m_hashMod = newcap - 1;
    for (int i = 0; i < oldcap; i++)
    {
        if (oldelem[i].hash != HK_CACHED_HASH_EMPTY)
        {
            insert(oldelem[i].key, oldelem[i].value);
        }
    }
    mem.blockFree(oldelem, oldcap * (int)sizeof(Elem));
}

// 0x00D13E50: each used slot is found again from its hash (the SDK's count and empty-slot asserts are compiled out,
// so a broken run loops forever, as in rfg.exe)
template<typename Operations, typename Allocator>
hkBool hkCachedHashMap<Operations, Allocator>::isOk() const
{
    for (int i = 0; i <= m_hashMod; i++)
    {
        if (m_elem[i].hash != HK_CACHED_HASH_EMPTY)
        {
            const hkUlong hash = m_elem[i].hash;
            const hkUlong key = m_elem[i].key;
            hkUlong j = hash & m_hashMod;
            while (m_elem[j].hash != hash || !m_ops.equal(key, m_elem[j].key))
            {
                j = (j + 1) & m_hashMod;
            }
        }
    }
    return true;
}

// 0x00D14150: a copy of the other map's table
template<typename Operations, typename Allocator>
hkCachedHashMap<Operations, Allocator>& hkCachedHashMap<Operations, Allocator>::operator=(const hkCachedHashMap* other)
{
    hkMemoryAllocator& mem = Allocator().get(this);
    mem.blockFree(m_elem, (m_hashMod + 1) * (int)sizeof(Elem));
    m_hashMod = other->m_hashMod;
    m_numElems = other->m_numElems;
    m_elem = static_cast<Elem*>(mem.blockAlloc((m_hashMod + 1) * (int)sizeof(Elem)));
    hkString::memCpy(m_elem, other->m_elem, (m_hashMod + 1) * (int)sizeof(Elem));
    return *this;
}

// 0x00D13EE0
template<typename Operations, typename Allocator>
void hkCachedHashMap<Operations, Allocator>::clear()
{
    for (int i = 0; i < m_hashMod + 1; i++)
    {
        m_elem[i].hash = HK_CACHED_HASH_EMPTY;
    }
    m_numElems = 0;
}

// 0x00D13F20: the tables only (the operations stay)
template<typename Operations, typename Allocator>
void hkCachedHashMap<Operations, Allocator>::swap(hkCachedHashMap& other)
{
    Elem* te = m_elem;
    const int tn = m_numElems;
    const int th = m_hashMod;
    m_elem = other.m_elem;
    m_numElems = other.m_numElems;
    m_hashMod = other.m_hashMod;
    other.m_elem = te;
    other.m_numElems = tn;
    other.m_hashMod = th;
}

// 0x00D13F50
template<typename Operations, typename Allocator>
void hkCachedHashMap<Operations, Allocator>::merge(const hkCachedHashMap& other)
{
    const int ohm = other.m_hashMod;
    for (int i = 0; i <= ohm; i++)
    {
        if (other.m_elem[i].hash != HK_CACHED_HASH_EMPTY)
        {
            insert(other.m_elem[i].key, other.m_elem[i].value);
        }
    }
}

// 0x00D13F90
template<typename Operations, typename Allocator>
int hkCachedHashMap<Operations, Allocator>::getCapacity() const
{
    return m_hashMod + 1;
}

// 0x00D143C0: the smallest power of two (at least 8) holding twice the elements
template<typename Operations, typename Allocator>
void hkCachedHashMap<Operations, Allocator>::reserve(int numElements)
{
    int newcap = 8;
    while (newcap < numElements * 2)
    {
        newcap *= 2;
    }
    if (m_hashMod + 1 < newcap)
    {
        resizeTable(newcap);
    }
}

#undef HK_CACHED_HASH_EMPTY
