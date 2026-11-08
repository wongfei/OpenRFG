// Original: library/modules/runtime/base/sources/global/ringbuffer.inl (included by ringbuffer.hpp)
//
// Partial: the members used by the recovered code so far (bodies from the <GestureHelper::MoveData> uses inlined in
// gesturehelper.cpp). Positions run from m_start to m_end and wrap onto the storage modulo the capacity; pushBack
// overwrites the oldest entry when full.
#pragma once

#include "keen/base/memoryallocator.hpp"

namespace keen
{
    // <Message> 0x00C827B0 (:14)
    template<typename T>
    RingBuffer<T>::Iterator::Iterator(RingBuffer<T>* pRingBuffer, uint32 pos)
    {
        m_pRingBuffer = pRingBuffer;
        m_pos = pos;
    }

    template<typename T>
    const typename RingBuffer<T>::Iterator& RingBuffer<T>::Iterator::operator++()
    {
        ++m_pos;
        return *this;
    }

    template<typename T>
    bool RingBuffer<T>::Iterator::operator==(const Iterator& rhs)
    {
        return m_pRingBuffer == rhs.m_pRingBuffer && m_pos == rhs.m_pos;
    }

    template<typename T>
    bool RingBuffer<T>::Iterator::operator!=(const Iterator& rhs)
    {
        return !(*this == rhs);
    }

    template<typename T>
    T& RingBuffer<T>::Iterator::operator*()
    {
        return m_pRingBuffer->getEntry(m_pos);
    }

    template<typename T>
    RingBuffer<T>::RingBuffer()
    {
        m_start = 0;
        m_end = 0;
        m_capacity = 0;
    }

    template<typename T>
    RingBuffer<T>::~RingBuffer()
    {
    }

    // <Message> 0x00C85440 (:205): the storage is not constructed
    template<typename T>
    bool RingBuffer<T>::create(MemoryAllocator* pAllocator, uint32 capacity, uint32 alignment)
    {
        T* pData = (T*)pAllocator->allocate(capacity * sizeof(T), alignment, 0, nullptr);
        if (pData == nullptr)
        {
            return false;
        }
        m_data.setData(pData, capacity);
        m_start = 0;
        m_end = 0;
        m_capacity = capacity;
        return true;
    }

    // <Message> 0x00C854A0 (:229)
    template<typename T>
    void RingBuffer<T>::destroy(MemoryAllocator* pAllocator)
    {
        T* pData = m_data.releaseData();
        if (pData != nullptr)
        {
            pAllocator->free(pData);
        }
        m_start = 0;
        m_end = 0;
        m_capacity = 0;
    }

    // <Message> 0x00C821F0 (:444)
    template<typename T>
    void RingBuffer<T>::clear()
    {
        m_start = 0;
        m_end = 0;
    }

    // <Message> 0x00C854D0 (:296)
    // The positions are folded back when they pass twice the capacity.
    template<typename T>
    T& RingBuffer<T>::pushBack()
    {
        const uint32 index = m_end % m_capacity;
        ++m_end;
        if (m_end > m_capacity * 2u - 1u)
        {
            m_start = 0;
            m_end = m_capacity;
        }
        if (m_end - m_start > m_capacity)
        {
            m_start = m_end - m_capacity;
        }
        return m_data[index];
    }

    template<typename T>
    void RingBuffer<T>::pushBack(const T& value)
    {
        pushBack() = value;
    }

    // <Message> 0x00C82200 (:262)
    template<typename T>
    bool RingBuffer<T>::isEmpty() const
    {
        return m_start == m_end;
    }

    // <Message> 0x00C82210 (:274)
    template<typename T>
    uint32 RingBuffer<T>::getSize() const
    {
        return m_end - m_start;
    }

    // <Message> 0x00C85540 (:414)
    template<typename T>
    typename RingBuffer<T>::Iterator RingBuffer<T>::getStart()
    {
        return Iterator(this, m_start);
    }

    template<typename T>
    typename RingBuffer<T>::Iterator RingBuffer<T>::getEnd()
    {
        return Iterator(this, m_end);
    }

    template<typename T>
    T& RingBuffer<T>::getEntry(uint32 index)
    {
        return m_data[index % m_capacity];
    }
}
