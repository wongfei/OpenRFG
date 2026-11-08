// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkcachedhashmap.h
//
// Hand-written (stub_impl.md S4, serializer SA; layout from the PDB instance <hkStringMapOperations,
// hkContainerHeapAllocator>, code as the 2014.1 SDK's hkCachedHashMap, checked against rfg.exe): an open-addressing
// hash map that keeps each key's hash next to it, so lookups compare hashes before calling OPS::equal. A free slot has
// the hash -1 (no real hash: OPS::hash clears the top bit); collisions probe linearly, removal shifts the following
// entries back. The table starts at 16 slots and doubles once more than half full. The template functions are in
// hkcachedhashmap.cxx (included below).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// size 0x10 (<hkStringMapOperations,hkContainerHeapAllocator>)
template<typename Operations, typename Allocator>
class hkCachedHashMap
{
public:
    typedef hkCachedHashMap<Operations, Allocator> ThisType;
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ThisType);
    // A slot index; getCapacity() (m_hashMod + 1) means not found.
    typedef class Dummy* Iterator;

    hkCachedHashMap(Operations ops = Operations());
    ~hkCachedHashMap();

    Iterator getIterator() const;
    hkUlong getKey(Iterator it) const;
    hkUlong getValue(Iterator it) const;
    void setValue(Iterator it, hkUlong val);
    Iterator getNext(Iterator it) const;
    hkBool isValid(Iterator it) const;

    void insert(hkUlong key, hkUlong val);
    Iterator findKey(hkUlong key) const;
    Iterator findOrInsertKey(hkUlong key, hkUlong value);
    hkBool hasKey(hkUlong key) const { return isValid(findKey(key)); } // <hkStringMapOperations> 0x00D142B0
    hkUlong getOrInsert(hkUlong key, hkUlong ifNotFound);
    hkUlong getWithDefault(hkUlong key, hkUlong def) const;
    hkResult get(hkUlong key, hkUlong* out) const;
    void remove(Iterator it);
    hkResult remove(hkUlong key);
    int getSize() const { return m_numElems; } // <hkStringMapOperations> 0x00D07130
    hkBool isOk() const;
    hkCachedHashMap& operator=(const hkCachedHashMap* other);
    void clear();
    void swap(hkCachedHashMap& other);
    void merge(const hkCachedHashMap& other);
    int getCapacity() const;
    void reserve(int numElements);

protected:
    void resizeTable(int newcap);

    // size 0xC
    struct Elem
    {
        hkUlong hash; // 0x00
        hkUlong key; // 0x04
        hkUlong value; // 0x08
    };

    Elem* m_elem; // 0x00
    int m_numElems; // 0x04
    int m_hashMod; // 0x08: capacity - 1
    Operations m_ops; // 0x0C
};

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkcachedhashmap.cxx"
