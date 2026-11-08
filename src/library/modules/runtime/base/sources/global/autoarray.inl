// Original: library/modules/runtime/base/sources/global/autoarray.inl (included by autoarray.hpp)
//
// Partial: the members used by the recovered code so far. Elements are not constructed or destroyed (as in the
// <uint8> instantiation).
#pragma once

#include "keen/base/memoryallocator.hpp"

namespace keen
{
    // <uint8> 0x00C606D0 (:14)
    template<typename T>
    AutoArray<T>::AutoArray()
    {
        m_pAllocator = nullptr;
        m_pData = nullptr;
        m_size = 0;
    }

    // <uint8> 0x00C6D5B0 (:22)
    template<typename T>
    AutoArray<T>::~AutoArray()
    {
        destroy();
    }

    // <uint8> 0x00C6D5E0 (:28)
    template<typename T>
    bool AutoArray<T>::create(MemoryAllocator* pAllocator, uint32 size, uint32 alignment)
    {
        if (m_pData != nullptr)
        {
            // An array of the same size from the same allocator and with a suitable alignment is kept.
            if (m_size == size && m_pAllocator == pAllocator && (T*)((uint32)m_pData & ~(alignment - 1)) == m_pData)
            {
                return true;
            }
            destroy();
        }
        m_pAllocator = pAllocator;
        m_size = size;
        if (size != 0)
        {
            m_pData = (T*)pAllocator->allocate(size * sizeof(T), alignment, 0, nullptr);
            if (m_pData == nullptr)
            {
                return false;
            }
        }
        return true;
    }

    // <uint8> 0x00C6A4E0 (:87)
    template<typename T>
    void AutoArray<T>::destroy()
    {
        if (m_pData != nullptr)
        {
            m_pAllocator->free(m_pData);
            m_pData = nullptr;
            m_pAllocator = nullptr;
            m_size = 0;
        }
    }

    // <uint8> 0x00C606E0 (:109)
    template<typename T>
    uint32 AutoArray<T>::getSize() const
    {
        return m_size;
    }

    // <uint8> 0x00C606F0 (:121)
    template<typename T>
    T& AutoArray<T>::operator[](uint32 index)
    {
        return m_pData[index];
    }

    template<typename T>
    T* AutoArray<T>::getStart()
    {
        return m_pData;
    }
}
