// Original: library/modules/runtime/base/include/keen/base/chunkedsizedarray.hpp
//
// ChunkedSizedArray<T>, an array grown in fixed-size chunks linked in an InternalList (stable element addresses).
// Written from the <AllocationEntry> instantiation.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/internallist.hpp"
#include "keen/base/sizedarray.hpp"

namespace keen
{
    class MemoryAllocator;

    // size (<AllocationEntry>) 0x34
    template<typename T>
    class ChunkedSizedArray
    {
    public:
        typedef T ValueType;
        class Chunk;
        typedef InternalList<Chunk> ChunkList;

        // size 0x14
        class Chunk : public InternalListBase::Listable
        {
        public:
            Chunk(); // <AllocationEntry> 0x00CDE950
            ~Chunk(); // <AllocationEntry> 0x00CDD5E0

            SizedArray<T> entries; // 0x08
        };

        // size 0x8
        class ConstIterator
        {
        class Iterator;
        public:
            ConstIterator(const Iterator& other); // <AllocationEntry> 0x00CDE990 (.h:64)
            ConstIterator(const ConstIterator&);
            ConstIterator();
            ConstIterator& operator++(); // <AllocationEntry> 0x00CDE710 (.h:66)
            ConstIterator& operator--();
            bool operator==(const ConstIterator&) const;
            bool operator!=(const ConstIterator& other) const; // <AllocationEntry> 0x00CDCFA0 (.h:69)
            ConstIterator& operator=(const ConstIterator&);
            const T& operator*() const; // <AllocationEntry> 0x00CDE740 (.h:71)
            const T* operator->() const;

        private:
            typename InternalList<Chunk>::ConstIterator m_chunk; // 0x00
            uint32 m_index; // 0x04
        };

        // size 0x8
        class Iterator
        {
        public:
            Iterator(const ConstIterator&);
            Iterator(const Iterator&);
            Iterator(); // <AllocationEntry> 0x00CDE980 (.h:38)
            Iterator& operator++();
            Iterator& operator--();
            bool operator==(const Iterator&) const;
            bool operator!=(const Iterator&) const;
            Iterator& operator=(const Iterator&);
            T& operator*() const;
            T* operator->() const;

        private:
            typename InternalList<Chunk>::Iterator m_chunk; // 0x00
            uint32 m_index; // 0x04
        };

    private:
        ChunkedSizedArray(const ChunkedSizedArray<T>&);
    public:
        ChunkedSizedArray(); // <AllocationEntry> 0x00CDF020 (chunkedsizedarray.inl:13)
        ~ChunkedSizedArray(); // <AllocationEntry> 0x00CDE7E0 (chunkedsizedarray.inl:19)
        void create(MemoryAllocator* pAllocator, uint32 chunkSize, uint32 alignment); // <AllocationEntry> 0x00CDE7F0 (chunkedsizedarray.inl:25)
        void destroy(); // <AllocationEntry> 0x00CE0830 (chunkedsizedarray.inl:41)
        bool isEmpty() const;
        uint32 getSize() const; // <AllocationEntry> 0x00CDE820 (chunkedsizedarray.inl:58)
        uint32 getCapacity() const;
        void clear(); // <AllocationEntry> 0x00CE0770 (chunkedsizedarray.inl:84)
        T& pushBack();
        void pushBack(const T& value); // <AllocationEntry> 0x00CDF040 (chunkedsizedarray.inl:99)
        void popBack();
        T& getBack();
        const T& getBack() const;
        const T& operator[](uint32) const;
        T& operator[](uint32);
        ConstIterator getStart() const; // <AllocationEntry> 0x00CDF140 (chunkedsizedarray.inl:257)
        const Iterator& getStart();
        ConstIterator getEnd() const; // <AllocationEntry> 0x00CDF160 (chunkedsizedarray.inl:263)
        const Iterator& getEnd();
    private:
        ChunkedSizedArray<T>& operator=(const ChunkedSizedArray<T>&);

        MemoryAllocator* m_pAllocator; // 0x00
        InternalList<Chunk> m_chunks; // 0x04
        uint32 m_chunkSize; // 0x1C
        uint32 m_alignment; // 0x20
        Iterator m_start; // 0x24
        Iterator m_end; // 0x2C
    };
}
