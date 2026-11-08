// Original: library/modules/runtime/base/include/keen/base/hashmap.hpp
// Implementation: library/modules/runtime/base/sources/global/hashmap.inl
//
// HashMap<K, V, Traits>, a chained hash map: entries come from a PoolAllocator (owned or shared) and are linked in one
// InternalList; each bucket is an iterator to the first entry of its chain. Written by hand; the addresses are of the
// <uint32, GraphicsStateObject*> instantiation (<GSO>); <uint32, SpecularParameters> is the other one in the binary.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/array.hpp"
#include "keen/base/hashmaptraits.hpp"
#include "keen/base/internallist.hpp"
#include "keen/base/poolallocator.hpp"

namespace keen
{
    class MemoryAllocator;

    // size 0x30
    template<typename K, typename V, typename Traits = DefaultHashmapTraits<K, V> >
    class HashMap
    {
    public:
        // size 0x10 (<GSO>)
        struct Entry : public InternalListBase::Listable
        {
            Entry(); // <GSO> 0x00C60EB0 (.h:97)

            K key; // 0x08
            V value; // 0x0C
        };

        typedef PoolAllocator<Entry> EntryAllocatorType;
        typedef InternalList<Entry> EntryList;
        typedef Array<typename InternalList<Entry>::Iterator> BucketArray;
        typedef uint32 HashIndexType;
        typedef uint32 HashSizeType;

        struct Iterator; // no definition in PDB
        struct ConstIterator; // no definition in PDB

    private:
        HashMap(const HashMap<K, V, Traits>&);
    public:
        HashMap(MemoryAllocator*, uint32);
        HashMap(); // <GSO> 0x00C69A80 (hashmap.inl:19)
        ~HashMap(); // <GSO> 0x00C69AB0 (hashmap.inl:54)
        void create(MemoryAllocator* pAllocator, uint32 capacity, PoolAllocator<Entry>* pEntryAllocator); // <GSO> 0x00C6EAA0 (hashmap.inl:70)
        void destroy(MemoryAllocator* pAllocator); // <GSO> 0x00C70060 (hashmap.inl:115)
        typename InternalList<Entry>::Iterator insert(const K& key, const V& value); // <GSO> 0x00C700E0 (hashmap.inl:150)
        void remove(typename InternalList<Entry>::Iterator it); // <GSO> 0x00C6EC10 (hashmap.inl:251)
        typename InternalList<Entry>::ConstIterator find(const K& key) const; // <GSO> 0x00C6EC80 (hashmap.inl:301)
        typename InternalList<Entry>::Iterator find(const K& key); // <GSO> 0x00C6ECD0 (hashmap.inl:279)
        void clear(); // <GSO> 0x00C6ED90 (hashmap.inl:345)
        typename InternalList<Entry>::ConstIterator getStart() const;
        typename InternalList<Entry>::Iterator getStart();
        typename InternalList<Entry>::ConstIterator getEnd() const; // <GSO> 0x00C69AC0 (hashmap.inl:339)
        typename InternalList<Entry>::Iterator getEnd(); // <GSO> 0x00C69AD0 (hashmap.inl:325)
        bool isEmpty() const;
        bool isFull() const;
        uint32 getSize() const; // <GSO> 0x00C606C0 (hashmap.inl:442)
        uint32 getCapacity() const;
        uint32 getBucketCount() const;
        V& operator[](const K&);
    private:
        HashMap<K, V, Traits>& operator=(const HashMap<K, V, Traits>&);
        uint32 getHashIndex(const K& key) const; // <GSO> 0x00C6D720 (hashmap.inl:480)

        PoolAllocator<Entry>* m_pEntryAllocator; // 0x00
        bool m_ownsAllocator; // 0x04
        Array<typename InternalList<Entry>::Iterator> m_buckets; // 0x08
        InternalList<Entry> m_entries; // 0x10
        uint32 m_bucketCount; // 0x28
        uint32 m_bucketMask; // 0x2C
    };
}

#include "../../../sources/global/hashmap.inl"
