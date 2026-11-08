// Original: library/modules/runtime/base/include/keen/base/ringbuffer.hpp
//
// RingBuffer<T>, a fixed-capacity circular buffer with iterators. Written from the <LocalLoopback::Message>
// instantiation.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/staticarray.hpp"

namespace keen
{
    class MemoryAllocator;

    // size (<Message>) 0x14
    template<typename T>
    class RingBuffer
    {
    public:
        // size 0x8
        class ConstIterator
        {
            ConstIterator(const RingBuffer<T>*, uint32);
        class Iterator;
        public:
            ConstIterator(const Iterator& rhs); // <Message> 0x00C82220 (ringbuffer.inl:92)
            ConstIterator(const ConstIterator&);
            ConstIterator();
            const ConstIterator& operator++(); // <Message> 0x00C82240 (ringbuffer.inl:99)
            bool operator==(const ConstIterator&);
            bool operator!=(const ConstIterator&);
            const T& operator*(); // <Message> 0x00C87DC0 (ringbuffer.inl:119)
            const T* operator->();

        private:
            const RingBuffer<T>* m_pRingBuffer; // 0x00
            uint32 m_pos; // 0x04
        };

        // size 0x8
        class Iterator
        {
            friend class RingBuffer<T>;
            Iterator(RingBuffer<T>* pRingBuffer, uint32 pos); // <Message> 0x00C827B0 (ringbuffer.inl:14)
        public:
            Iterator(const Iterator&);
            Iterator();
            const Iterator& operator++();
            bool operator==(const Iterator&);
            bool operator!=(const Iterator&);
            T& operator*();
            T* operator->();

        private:
            RingBuffer<T>* m_pRingBuffer; // 0x00
            uint32 m_pos; // 0x04
        };

        struct ReverseConstIterator; // no definition in PDB

        RingBuffer();
        ~RingBuffer();
        bool create(MemoryAllocator* pAllocator, uint32 capacity, uint32 alignment); // <Message> 0x00C85440 (ringbuffer.inl:205)
        void destroy(MemoryAllocator* pAllocator); // <Message> 0x00C854A0 (ringbuffer.inl:229)
        void setData(T*, uint32);
        T* releaseData();
        void clear(); // <Message> 0x00C821F0 (ringbuffer.inl:444)
        T& pushBack(); // <Message> 0x00C854D0 (ringbuffer.inl:296)
        void pushBack(const T&);
        void popBack();
        void popFront();
        bool isEmpty() const; // <Message> 0x00C82200 (ringbuffer.inl:262)
        bool isFull() const; // <Message> 0x00C85510 (ringbuffer.inl:268)
        uint32 getSize() const; // <Message> 0x00C82210 (ringbuffer.inl:274)
        uint32 getCapacity() const;
        const T& getFirstElement() const;
        T& getFirstElement();
        const T& getLastElement() const;
        T& getLastElement(); // <Message> 0x00C85520 (ringbuffer.inl:372)
        Iterator getStart(); // <Message> 0x00C85540 (ringbuffer.inl:414)
        ConstIterator getStart() const;
        Iterator getEnd();
        ConstIterator getEnd() const;
        ReverseConstIterator getReverseStart() const;
        ReverseConstIterator getReverseEnd() const;
        uint32 getStartIndex() const;
    private:
        const T& getEntry(uint32 index) const; // <Message> 0x00C85810 (ringbuffer.inl:457)
        T& getEntry(uint32);
        uint32 getNextIndex(uint32) const;
        uint32 getPreviousIndex(uint32) const;

        StaticArray<T> m_data; // 0x00
        uint32 m_start; // 0x08
        uint32 m_end; // 0x0C
        uint32 m_capacity; // 0x10
    };
}

#include "../../../sources/global/ringbuffer.inl"
