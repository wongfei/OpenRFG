// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkpointermap.h
//
// Hand-written (havok_impl.md HK1): a hash map of pointer-sized keys and values, stored as an hkMap of unsigned long
// (Havok 2014.1 hkPointerMap.h; the instance hkDefaultError uses is hkMapBase<unsigned long, unsigned long>). The
// key all ones is reserved (a free slot). A 64-bit key stores keys and values as unsigned __int64 (the SDK's
// hkMapStorage: hkVersionPatchManager's m_patchIndexFromUid is hkMapBase<unsigned __int64, unsigned __int64> in
// rfg.exe).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkmap.h"

#include <type_traits>

// size 0xC
template<typename K, typename V, typename ALLOCATOR = hkContainerHeapAllocator>
class hkPointerMap
{
public:
    typedef typename std::conditional<(sizeof(K) > 4), unsigned __int64, unsigned long>::type Storage;
    typedef hkMap<Storage, Storage, hkMapOperations<Storage>, ALLOCATOR> Map;
    typedef typename Map::Iterator Iterator;

    int insert(K key, V val) { return m_map.insert((Storage)key, (Storage)val); }
    V getWithDefault(K key, V def) const { return (V)m_map.getWithDefault((Storage)key, (Storage)def); }
    // the value of a present key
    hkResult get(K key, V* out) const
    {
        Storage v;
        hkResult r = m_map.get((Storage)key, &v);
        if (r == HK_SUCCESS)
        {
            *out = (V)v;
        }
        return r;
    }
    Iterator findKey(K key) const { return m_map.findKey((Storage)key); }
    Iterator findOrInsertKey(K key, V def) { return m_map.findOrInsertKey((Storage)key, (Storage)def); }
    hkResult remove(K key) { return m_map.remove((Storage)key); }
    void clear() { m_map.clear(); }
    void clearAndDeallocate() { m_map.clearAndDeallocate(); }
    int getSize() const { return m_map.getSize(); }
    Iterator getIterator() const { return m_map.getIterator(); }
    K getKey(Iterator i) const { return (K)m_map.getKey(i); }
    V getValue(Iterator i) const { return (V)m_map.getValue(i); }
    void setValue(Iterator i, V val) { m_map.setValue(i, (Storage)val); }
    void removeAtIterator(Iterator i) { m_map.remove(i); } // remove(Iterator) of the SDK (Iterator is an int here)
    hkBool32 hasKey(K key) const { return m_map.hasKey((Storage)key); }
    Iterator getNext(Iterator i) const { return m_map.getNext(i); }
    hkBool isValid(Iterator i) const { return m_map.isValid(i); }

    Map m_map; // 0x00
};
