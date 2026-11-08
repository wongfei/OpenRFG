// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkstoragestringmap.h
//
// Hand-written (stub_impl.md S4, serializer SA; layout from the PDB instances, code as the 2014.1 SDK's
// hkStorageStringMap): an hkStringMap that keeps its own copies of the keys (hkString::strDup with the map's
// allocator), freed on remove and clear.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkstringmap.h"

// size 0x10
template<typename V, typename Allocator = hkContainerHeapAllocator>
class hkStorageStringMap : public hkStringMap<V, Allocator>
{
public:
    typedef typename hkStringMap<V, Allocator>::Iterator Iterator;
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkStorageStringMap);

    hkStorageStringMap() {} // <int> 0x00F56160

    // <int> 0x00F56590
    ~hkStorageStringMap()
    {
        clear();
    }

    // <int> 0x00F56200: the stored key (a new copy, or the existing one whose value is replaced)
    const char* insert(const char* key, V val)
    {
        Iterator it = this->findKey(key);
        const char* ret;
        if (!this->isValid(it))
        {
            ret = hkString::strDup(key, Allocator().get(this));
            hkStringMap<V, Allocator>::insert(ret, val);
        }
        else
        {
            ret = this->getKey(it);
            this->setValue(it, val);
        }
        return ret;
    }

    V getOrInsert(const char* key, V ifNotFound)
    {
        Iterator it = this->findKey(key);
        if (this->isValid(it))
        {
            return this->getValue(it);
        }
        this->insert(key, ifNotFound);
        return ifNotFound;
    }

    void remove(Iterator it)
    {
        char* k = (char*)this->m_map.getKey(it);
        hkString::strFree(k, Allocator().get(this));
        hkStringMap<V, Allocator>::remove(it);
    }

    hkResult remove(const char* key)
    {
        Iterator it = this->findKey(key);
        if (this->isValid(it))
        {
            this->remove(it);
            return HK_SUCCESS;
        }
        return HK_FAILURE;
    }

    // <int> 0x00F56290
    void clear()
    {
        for (Iterator it = hkStringMap<V, Allocator>::getIterator(); hkStringMap<V, Allocator>::isValid(it); it = hkStringMap<V, Allocator>::getNext(it))
        {
            char* k = (char*)this->m_map.getKey(it);
            hkString::strFree(k, Allocator().get(this));
        }
        hkStringMap<V, Allocator>::clear();
    }

    void swap(hkStorageStringMap& other) { this->m_map.swap(other.m_map); }

    void merge(const hkStringMap<V, Allocator>& other)
    {
        typedef typename hkStringMap<V, Allocator>::Iterator OtherIterator;
        for (OtherIterator it = other.getIterator(); other.isValid(it); it = other.getNext(it))
        {
            insert(other.getKey(it), other.getValue(it));
        }
    }
};
