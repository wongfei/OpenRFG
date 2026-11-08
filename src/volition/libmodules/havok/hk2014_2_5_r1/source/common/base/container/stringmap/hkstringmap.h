// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkstringmap.h
//
// Hand-written (stub_impl.md S4, serializer SA; layouts from the PDB instances, code as the 2014.1 SDK's
// hkStringMap): a map from C strings (compared by contents, not owned) to values that fit in a pointer, over
// hkCachedHashMap. hkStorageStringMap (hkstoragestringmap.h) owns copies of its keys.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkcachedhashmap.h"

// size 0x1
// The string hash: h = 31 * h + c over the (signed) characters, the top bit cleared (-1 marks a free slot).
struct hkStringMapOperations
{
    // 0x00D13B20
    hkBool32 equal(hkUlong k1, hkUlong k2) const
    {
        return hkString::strCmp((const char*)k1, (const char*)k2) == 0;
    }

    // 0x00D13B40
    hkUlong hash(hkUlong key) const
    {
        const char* p = (const char*)key;
        hkUlong h = 0;
        for (int i = 0; p[i] != 0; i++)
        {
            h = 31 * h + p[i];
        }
        return h & (hkUlong(-1) >> 1);
    }
};

// size 0x10
template<typename V, typename Allocator = hkContainerHeapAllocator>
class hkStringMap
{
    static_assert(sizeof(V) <= sizeof(hkUlong), "hkStringMap values must fit in a pointer");

public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkStringMap);
    typedef typename hkCachedHashMap<hkStringMapOperations, Allocator>::Iterator Iterator;

    hkStringMap() {} // <int> 0x00F405B0

    void insert(const char* key, V val) { m_map.insert(hkUlong(key), hkUlong(val)); } // <int> 0x00F405D0
    Iterator findKey(const char* key) const { return m_map.findKey(hkUlong(key)); } // <int> 0x00F55E10
    Iterator findOrInsertKey(const char* key, V val) { return m_map.findOrInsertKey(hkUlong(key), hkUlong(val)); }
    hkBool hasKey(const char* key) const { return m_map.hasKey(hkUlong(key)); }
    V getOrInsert(const char* key, V ifNotFound) { return (V)m_map.getOrInsert(hkUlong(key), hkUlong(ifNotFound)); } // <int> 0x00F405E0
    V getWithDefault(const char* key, V def) const { return (V)m_map.getWithDefault(hkUlong(key), hkUlong(def)); } // <int> 0x00F405F0

    // <int> 0x00F6A320
    hkResult get(const char* key, V* out) const
    {
        hkUlong tmp;
        if (m_map.get(hkUlong(key), &tmp) == HK_SUCCESS)
        {
            *out = V(tmp);
            return HK_SUCCESS;
        }
        return HK_FAILURE;
    }

    void remove(Iterator it) { m_map.remove(it); } // <int> 0x00FAA7A0
    hkResult remove(const char* key) { return m_map.remove(hkUlong(key)); }
    int getSize() const { return m_map.getSize(); } // <int> 0x00F60D80
    hkBool isOk() const { return m_map.isOk(); }
    Iterator getIterator() const { return m_map.getIterator(); } // <int> 0x00F55E60
    const char* getKey(Iterator i) const { return reinterpret_cast<const char*>(m_map.getKey(i)); } // <int> 0x00F55E20
    V getValue(Iterator i) const { return (V)m_map.getValue(i); } // <int> 0x00FAA7B0
    void setValue(Iterator i, V v) { m_map.setValue(i, hkUlong(v)); } // <int> 0x00F55E30
    Iterator getNext(Iterator i) const { return m_map.getNext(i); } // <int> 0x00F55E70
    hkBool isValid(Iterator i) const { return m_map.isValid(i); } // <int> 0x00F55E40
    void clear() { m_map.clear(); } // <int> 0x00F55E80

    // a fresh 16-slot table, the old one freed
    void reset()
    {
        hkCachedHashMap<hkStringMapOperations, Allocator> newMap;
        m_map.swap(newMap);
    }

    void swap(hkStringMap& other) { m_map.swap(other.m_map); }
    void merge(const hkStringMap& other) { m_map.merge(other.m_map); }
    int getCapacity() const { return m_map.getCapacity(); }
    void reserve(int numElements) { m_map.reserve(numElements); }
    // ~hkStringMap <int> 0x00F3DBD0: implicit

protected:
    hkCachedHashMap<hkStringMapOperations, Allocator> m_map; // 0x00
};
