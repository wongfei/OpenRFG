// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkserializemultimap.h
//
// Hand-written (stub_impl.md S4, serializer; layout from the PDB instance <void const*, int, hkPointerMap<void const*,
// int>>, interface as the 2014.1 SDK's hkSerializeMultiMap): a map from a key to a chain of values, the chains kept in
// one array (m_valueChain, linked by index, freed slots chained from m_freeChainStart) and the first index of each
// key's chain in m_indexMap. The template functions are in hkserializemultimap.inl (included below).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"

// size 0x1C (<void const*,int>)
template<typename K, typename V, typename IndexMap>
class hkSerializeMultiMap
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkSerializeMultiMap);

    // size 8 + sizeof(V) - 4 (<int>: 0x8)
    struct Value
    {
        V value; // 0x00
        int next; // 0x04 (<int>): the next index of the chain, -1 at its end
    };

    hkSerializeMultiMap() : m_freeChainStart(-1) {} // <void const*,int> 0x00F6AAD0
    ~hkSerializeMultiMap() {} // <void const*,int> 0x00F6AA30
    int getFirstIndex(K k) const; // <void const*,int> 0x00FB0EC0
    const V& getValue(int i) const { return m_valueChain[i].value; } // <void const*,int> 0x00FB0D70
    int getNextIndex(int i) const { return m_valueChain[i].next; } // <void const*,int> 0x00FB0D80
    hkBool32 insert(K k, const V& value); // <void const*,int> 0x00F6A9E0
    int addPendingValue(const V& value, int next);
    void realizePendingKey(K k, int index);
    int getNumKeys() const;
    void clear();
    int removeByIndex(K key, int removeIndex); // <void const*,int> 0x00FB0EE0
    void removeByValue(K key, const V& value);
    void removeKey(K key);
    void changeKey(K oldKey, K newKey);

protected:
    int getFreeIndex(); // <void const*,int> 0x00F6A910

public:
    hkArray<Value, hkContainerHeapAllocator> m_valueChain; // 0x00
    IndexMap m_indexMap; // 0x0C
    int m_freeChainStart; // 0x18
};

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkserializemultimap.inl"
