// Original: library/modules/runtime/base/sources/global/slice.inl (included by slice.hpp)
//
// Partial: only the members the recovered code uses so far. Bodies of members the binary does not contain as
// separate functions are inferred from their callers.
#pragma once

namespace keen
{
    template<typename T>
    Slice<T>::Slice()
    {
        m_pStart = nullptr;
        m_size = 0;
        m_capacity = 0;
    }

    template<typename T>
    void Slice<T>::create(T* pStart, uint32 size, uint32 capacity)
    {
        m_pStart = pStart;
        m_size = size;
        m_capacity = capacity;
    }

    template<typename T>
    T* Slice<T>::getStart()
    {
        return m_pStart;
    }

    template<typename T>
    const T* Slice<T>::getStart() const
    {
        return m_pStart;
    }

    template<typename T>
    uint32 Slice<T>::getSize() const
    {
        return m_size;
    }

    template<typename T>
    uint32 Slice<T>::getCapacity() const
    {
        return m_capacity;
    }

    template<typename T>
    T& Slice<T>::operator[](uint32 index)
    {
        return m_pStart[index];
    }

    template<typename T>
    const T& Slice<T>::operator[](uint32 index) const
    {
        return m_pStart[index];
    }

    // <MemoryBlockInfo> 0x00BF3B30
    template<typename T>
    T& Slice<T>::pushBack()
    {
        return m_pStart[m_size++];
    }
}
