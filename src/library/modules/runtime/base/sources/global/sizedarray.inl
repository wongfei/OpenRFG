// Original: library/modules/runtime/base/sources/global/sizedarray.inl (included by sizedarray.hpp)
//
// Partial: the members used by the recovered code so far (bodies from the <uint32> instantiation). pushBack does not
// check the capacity.
#pragma once

#include "keen/base/memoryallocator.hpp"

#include <new>

namespace keen
{
    // <uint32> 0x004D84B0
    template<typename T>
    SizedArray<T>::SizedArray()
    {
        m_pData = nullptr;
        m_size = 0;
        m_capacity = 0;
    }

    // <uint32> 0x004D84C0
    template<typename T>
    SizedArray<T>::~SizedArray()
    {
    }

    // <uint32> 0x00BF3740
    template<typename T>
    bool SizedArray<T>::create(MemoryAllocator* pAllocator, uint32 capacity, uint32 alignment)
    {
        m_size = 0;
        if (capacity != 0)
        {
            m_pData = (T*)pAllocator->allocate(capacity * sizeof(T), alignment, 0, nullptr);
            if (m_pData == nullptr)
            {
                return false;
            }
            m_capacity = capacity;
        }
        return true;
    }

    // <uint32> 0x00BFB9A0
    template<typename T>
    void SizedArray<T>::destroy(MemoryAllocator* pAllocator)
    {
        if (m_pData != nullptr)
        {
            clear();
            pAllocator->free(m_pData);
            m_pData = nullptr;
            m_size = 0;
            m_capacity = 0;
        }
    }

    // <uint32> 0x00BF3790
    template<typename T>
    bool SizedArray<T>::isEmpty() const
    {
        return m_size == 0;
    }

    // <uint32> 0x00C82400
    template<typename T>
    bool SizedArray<T>::isFull() const
    {
        return m_size == m_capacity;
    }

    // <uint32> 0x00C60740
    template<typename T>
    uint32 SizedArray<T>::getSize() const
    {
        return m_size;
    }

    // <uint32> 0x00BF37A0
    template<typename T>
    uint32 SizedArray<T>::getCapacity() const
    {
        return m_capacity;
    }

    template<typename T>
    const T& SizedArray<T>::operator[](uint32 index) const
    {
        return m_pData[index];
    }

    // <uint32> 0x00C82410
    template<typename T>
    T& SizedArray<T>::operator[](uint32 index)
    {
        return m_pData[index];
    }

    // <uint32> 0x00BF3E70
    template<typename T>
    void SizedArray<T>::clear()
    {
        for (uint32 i = 0; i < m_size; ++i)
        {
            m_pData[i].~T();
        }
        m_size = 0;
    }

    template<typename T>
    T& SizedArray<T>::pushBack()
    {
        T* pElement = new (&m_pData[m_size++]) T();
        return *pElement;
    }

    // <uint32> 0x00BF37B0
    template<typename T>
    void SizedArray<T>::pushBack(const T& value)
    {
        new (&m_pData[m_size++]) T(value);
    }

    // <uint32> 0x00BF37D0
    template<typename T>
    void SizedArray<T>::popBack()
    {
        --m_size;
        m_pData[m_size].~T();
    }

    // <uint32> 0x00BF37E0
    template<typename T>
    T& SizedArray<T>::getBack()
    {
        return m_pData[m_size - 1];
    }

    template<typename T>
    const T& SizedArray<T>::getBack() const
    {
        return m_pData[m_size - 1];
    }

    template<typename T>
    const T* SizedArray<T>::getStart() const
    {
        return m_pData;
    }

    // <uint32> 0x00C82420
    template<typename T>
    T* SizedArray<T>::getStart()
    {
        return m_pData;
    }

    template<typename T>
    const T* SizedArray<T>::getEnd() const
    {
        return m_pData + m_size;
    }

    // <uint32> 0x00C82760
    template<typename T>
    T* SizedArray<T>::getEnd()
    {
        return m_pData + m_size;
    }

    // <uint32> 0x00C856B0 (body written from keen::task::removeTaskQueue, where it is inlined)
    // Moves the following elements down by assignment; no range check.
    template<typename T>
    T* SizedArray<T>::erase(T* it)
    {
        T* pLast = m_pData + (m_size - 1);
        for (T* p = it; p < pLast; ++p)
        {
            *p = p[1];
        }
        --m_size;
        return it;
    }

    // Moves the following elements up by assignment (written from keen::task::addTaskQueue, where it is inlined); no
    // capacity check.
    template<typename T>
    T* SizedArray<T>::insert(T* it, const T& value)
    {
        T* p = m_pData + m_size;
        ++m_size;
        for (; it < p; --p)
        {
            *p = p[-1];
        }
        *it = value;
        return it;
    }
}
