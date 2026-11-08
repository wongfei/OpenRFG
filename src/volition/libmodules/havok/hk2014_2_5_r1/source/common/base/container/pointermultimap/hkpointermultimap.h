// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermultimap/hkpointermultimap.h
//
// Hand-written (stub_impl.md S4, serializer; layout from the PDB instance <uint32, hkTypeManager::Type*>, code as the
// 2014.1 SDK's hkPointerMultiMap): a typed front of hkMultiMap<hkUlong, hkUlong>; keys and values are stored as
// hkUlong.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermultimap/hkmultimap.h"

// size 0xC
template<typename KEY, typename VAL>
class hkPointerMultiMap
{
public:
    typedef hkPointerMultiMap<KEY, VAL> ThisType;
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ThisType);
    typedef hkUlong Storage;
    typedef hkMultiMapOperations<hkUlong> Operations;
    typedef typename hkMultiMap<Storage, Storage>::Iterator Iterator;

    hkPointerMultiMap() {} // <uint32,Type*> 0x00F52A00
    hkPointerMultiMap(void* ptr, int sizeInBytes) : m_map(ptr, sizeInBytes) {}
    void insert(KEY key, VAL val) { m_map.insert(Storage(key), Storage(val)); } // <uint32,Type*> 0x00F52FB0
    hkResult tryInsert(KEY key, VAL val) { return m_map.tryInsert(Storage(key), Storage(val)); }
    Iterator findKey(KEY key) const { return m_map.findKey(Storage(key)); } // <uint32,Type*> 0x00F52FC0
    Iterator findKeyWithValue(KEY key, VAL val) const { return m_map.findKeyWithValue(Storage(key), Storage(val)); }
    Iterator findOrInsertKey(KEY key, VAL val) { return m_map.findOrInsertKey(Storage(key), Storage(val)); }
    hkBool hasKey(KEY key) const { return m_map.hasKey(Storage(key)); }
    VAL getWithDefault(KEY key, VAL def) const { return (VAL)m_map.getWithDefault(Storage(key), Storage(def)); }

    hkResult get(KEY key, VAL* out) const
    {
        Storage tmp;
        if (m_map.get(Storage(key), &tmp) == HK_SUCCESS)
        {
            *out = (VAL)tmp;
            return HK_SUCCESS;
        }
        return HK_FAILURE;
    }

    hkResult remove(KEY key, VAL val) { return m_map.remove(Storage(key), Storage(val)); } // <uint32,Type*> 0x00F52FD0
    hkResult remove(KEY key) { return m_map.remove(Storage(key)); }
    void remove(Iterator it) { m_map.remove(it); }
    int removeAll(KEY key) { return m_map.removeAll(Storage(key)); }
    int getSize() const { return m_map.getSize(); }
    int getCapacity() const { return m_map.getCapacity(); }
    Iterator getIterator() const { return m_map.getIterator(); }
    KEY getKey(Iterator i) const { return (KEY)m_map.getKey(i); }
    VAL getValue(Iterator i) const { return (VAL)m_map.getValue(i); }
    void setValue(Iterator i, VAL val) { m_map.setValue(i, Storage(val)); }
    Iterator getNext(Iterator i) const { return m_map.getNext(i); }
    Iterator getNext(Iterator i, KEY key) const { return m_map.getNext(i, Storage(key)); }
    int findNumEntries(KEY key) const { return m_map.findNumEntries(Storage(key)); }
    int findNumEntries(KEY key, VAL val) const { return m_map.findNumEntries(Storage(key), Storage(val)); }
    hkBool isValid(Iterator i) const { return m_map.isValid(i); }
    void clear() { m_map.clear(); }
    void reserve(int numElements) { m_map.reserve(numElements); }
    void swap(hkPointerMultiMap& other) { m_map.swap(other.m_map); }

private:
    hkPointerMultiMap(const hkPointerMultiMap&);

protected:
    hkMultiMap<Storage, Storage> m_map; // 0x00
};
