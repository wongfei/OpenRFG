// Original: library/modules/runtime/base/sources/global/hashmap.inl (included by hashmap.hpp)
//
// All entries live in one InternalList, grouped by bucket in bucket order. m_buckets has m_bucketCount + 1 iterators:
// bucket i's chain runs from m_buckets[i] up to m_buckets[i + 1]; an empty bucket points at the next non-empty
// bucket's first entry or at the list end (null), and the extra last iterator stays at the list end.
//
// Partial: the members used by the recovered code so far.
#pragma once

#include "keen/base/memoryallocator.hpp"
#include "keen/base/trace.hpp"

namespace keen
{
    // <GSO> 0x00C6A530 (hashmaptraits.hpp:27)
    template<typename K, typename V>
    uint32 DefaultHashmapTraits<K, V>::getHashValue(const K& key)
    {
        // Magic number: the key is xored with 0xDEADBEEF before masking with the bucket count.
        return (uint32)key ^ 0xdeadbeefu;
    }

    // <GSO> 0x00C60D60 (hashmaptraits.hpp:32)
    template<typename K, typename V>
    bool DefaultHashmapTraits<K, V>::isKeyEqual(const K& key0, const K& key1)
    {
        return key0 == key1;
    }

    // <GSO> 0x00C60EB0 (.h:97)
    template<typename K, typename V, typename Traits>
    HashMap<K, V, Traits>::Entry::Entry()
    {
    }

    // <GSO> 0x00C69A80 (:19)
    template<typename K, typename V, typename Traits>
    HashMap<K, V, Traits>::HashMap()
    {
        m_bucketCount = 0;
        m_bucketMask = 0;
        m_pEntryAllocator = nullptr;
        m_ownsAllocator = false;
    }

    // <GSO> 0x00C69AB0 (:54)
    template<typename K, typename V, typename Traits>
    HashMap<K, V, Traits>::~HashMap()
    {
    }

    // <GSO> 0x00C6EAA0 (:70)
    template<typename K, typename V, typename Traits>
    void HashMap<K, V, Traits>::create(MemoryAllocator* pAllocator, uint32 capacity, PoolAllocator<Entry>* pEntryAllocator)
    {
        // One bucket per Traits::BucketSize entries, rounded up to a power of two, at least Traits::MinBucketCount.
        uint32 bucketCount = capacity / Traits::BucketSize;
        if (bucketCount != 0)
        {
            uint32 v = bucketCount - 1;
            v |= v >> 1;
            v |= v >> 2;
            v |= v >> 4;
            v |= v >> 8;
            v |= v >> 16;
            bucketCount = v + 1;
        }
        if (bucketCount <= Traits::MinBucketCount)
        {
            bucketCount = Traits::MinBucketCount;
        }
        m_bucketMask = bucketCount - 1;
        m_bucketCount = bucketCount;
        m_buckets.create(pAllocator, bucketCount + 1, 16);
        for (uint32 i = 0; i < m_bucketCount + 1; ++i)
        {
            m_buckets[i] = m_entries.getEnd();
        }

        if (pEntryAllocator != nullptr)
        {
            m_ownsAllocator = false;
            m_pEntryAllocator = pEntryAllocator;
            return;
        }
        void* pAllocatorMemory = pAllocator->allocate(sizeof(PoolAllocator<Entry>), 4, 0, nullptr);
        m_pEntryAllocator = pAllocatorMemory != nullptr ? new (pAllocatorMemory) PoolAllocator<Entry>() : nullptr;
        m_ownsAllocator = true;
        MemoryBlock entryMemory;
        entryMemory.size = capacity * sizeof(Entry);
        entryMemory.pStart = (uint8*)pAllocator->allocate(entryMemory.size, 4, 0, nullptr);
        m_pEntryAllocator->create(entryMemory);
    }

    // <GSO> 0x00C70060 (:115)
    template<typename K, typename V, typename Traits>
    void HashMap<K, V, Traits>::destroy(MemoryAllocator* pAllocator)
    {
        if (m_bucketCount == 0)
        {
            return;
        }
        clear();
        if (m_ownsAllocator)
        {
            const MemoryBlock entryMemory = m_pEntryAllocator->destroy();
            pAllocator->free(entryMemory.pStart);
            if (m_pEntryAllocator != nullptr)
            {
                pAllocator->free(m_pEntryAllocator);
            }
            m_ownsAllocator = false;
        }
        m_pEntryAllocator = nullptr;
        m_buckets.destroy(pAllocator);
        m_bucketCount = 0;
        m_bucketMask = 0;
    }

    // <GSO> 0x00C700E0 (:150)
    template<typename K, typename V, typename Traits>
    typename InternalList<typename HashMap<K, V, Traits>::Entry>::Iterator HashMap<K, V, Traits>::insert(const K& key, const V& value)
    {
        typename InternalList<Entry>::Iterator it = find(key);
        if (it != m_entries.getEnd())
        {
            it->value = value;
            return it;
        }

        Entry* pEntry = m_pEntryAllocator->allocateObject();
        if (pEntry == nullptr)
        {
            return m_entries.getEnd();
        }
        pEntry->key = key;
        pEntry->value = value;

        // Insert in front of the first entry of the next non-empty bucket, so that the entries stay grouped.
        const uint32 bucketIndex = getHashIndex(key);
        typename InternalList<Entry>::Iterator next = m_entries.getEnd();
        for (uint32 i = bucketIndex + 1; i < m_bucketCount; ++i)
        {
            if (m_buckets[i] != m_entries.getEnd())
            {
                next = m_buckets[i];
                break;
            }
        }

        typename InternalList<Entry>::Iterator inserted;
        if (next == m_entries.getEnd())
        {
            inserted = m_entries.insert(pEntry);
            if (m_buckets[bucketIndex] != m_entries.getEnd())
            {
                return inserted;
            }
        }
        else if (m_entries.getBegin() == next)
        {
            m_entries.pushFront(pEntry);
            inserted = m_entries.getBegin();
        }
        else
        {
            typename InternalList<Entry>::Iterator previous = next;
            --previous;
            inserted = m_entries.insert(pEntry, previous);
        }

        // A new chain start: the bucket and the empty buckets before it (that pointed at the old start) now point at
        // the new entry.
        const typename InternalList<Entry>::Iterator oldStart = m_buckets[bucketIndex];
        if (oldStart == m_entries.getEnd() || m_buckets[bucketIndex + 1] == m_buckets[bucketIndex])
        {
            m_buckets[bucketIndex] = inserted;
            for (int i = (int)bucketIndex - 1; i >= 0 && m_buckets[i] == oldStart; --i)
            {
                m_buckets[i] = inserted;
            }
        }
        return inserted;
    }

    // <GSO> 0x00C6EC10 (:251)
    template<typename K, typename V, typename Traits>
    void HashMap<K, V, Traits>::remove(typename InternalList<Entry>::Iterator it)
    {
        if (it == m_entries.getEnd())
        {
            return;
        }
        typename InternalList<Entry>::Iterator next = it;
        ++next;
        const uint32 bucketIndex = getHashIndex(it->key);
        for (uint32 i = 0; i <= bucketIndex; ++i)
        {
            if (m_buckets[i] == it)
            {
                m_buckets[i] = next;
            }
        }
        Entry* pEntry = &*it;
        m_entries.erase(it);
        if (pEntry != nullptr)
        {
            m_pEntryAllocator->freeObject(pEntry);
        }
    }

    // <GSO> 0x00C6ECD0 (:279)
    template<typename K, typename V, typename Traits>
    typename InternalList<typename HashMap<K, V, Traits>::Entry>::Iterator HashMap<K, V, Traits>::find(const K& key)
    {
        const uint32 bucketIndex = getHashIndex(key);
        if (m_buckets[bucketIndex] != m_entries.getEnd())
        {
            for (typename InternalList<Entry>::Iterator it = m_buckets[bucketIndex]; it != m_buckets[bucketIndex + 1]; ++it)
            {
                if (Traits::isKeyEqual(key, it->key))
                {
                    return it;
                }
            }
        }
        return m_entries.getEnd();
    }

    // <GSO> 0x00C6EC80 (:301)
    template<typename K, typename V, typename Traits>
    typename InternalList<typename HashMap<K, V, Traits>::Entry>::ConstIterator HashMap<K, V, Traits>::find(const K& key) const
    {
        const uint32 bucketIndex = getHashIndex(key);
        typename InternalList<Entry>::ConstIterator it = m_buckets.getStart()[bucketIndex];
        for (; it != m_buckets.getStart()[bucketIndex + 1]; ++it)
        {
            if (Traits::isKeyEqual(key, it->key))
            {
                return it;
            }
        }
        return m_entries.getEnd();
    }

    // <GSO> 0x00C69AD0 (:325)
    template<typename K, typename V, typename Traits>
    typename InternalList<typename HashMap<K, V, Traits>::Entry>::Iterator HashMap<K, V, Traits>::getEnd()
    {
        return m_entries.getEnd();
    }

    // <GSO> 0x00C69AC0 (:339)
    template<typename K, typename V, typename Traits>
    typename InternalList<typename HashMap<K, V, Traits>::Entry>::ConstIterator HashMap<K, V, Traits>::getEnd() const
    {
        return m_entries.getEnd();
    }

    // <GSO> 0x00C6ED90 (:345)
    template<typename K, typename V, typename Traits>
    void HashMap<K, V, Traits>::clear()
    {
        typename InternalList<Entry>::Iterator it = m_entries.getBegin();
        while (it != m_entries.getEnd())
        {
            Entry* pEntry = &*it;
            ++it;
            if (pEntry != nullptr)
            {
                m_pEntryAllocator->freeObject(pEntry);
            }
        }
        m_entries.clear();
        for (uint32 i = 0; i < m_bucketCount + 1; ++i)
        {
            m_buckets[i] = m_entries.getEnd();
        }
    }

    // <GSO> 0x00C606C0 (:442)
    template<typename K, typename V, typename Traits>
    uint32 HashMap<K, V, Traits>::getSize() const
    {
        return m_entries.getSize();
    }

    // <GSO> 0x00C6D720 (:480)
    template<typename K, typename V, typename Traits>
    uint32 HashMap<K, V, Traits>::getHashIndex(const K& key) const
    {
        return Traits::getHashValue(key) & m_bucketMask;
    }
}
