// Original: library/modules/runtime/base/sources/global/array.inl (included by array.hpp)
//
// Partial: the members used by the recovered code so far. create default-constructs every element; destroy frees the
// storage without running destructors (as in the <NativeFileDevice::ReadThreadContext> instantiation).
#pragma once

#include "keen/base/memoryallocator.hpp"

#include <new>

namespace keen
{
    // <NativeFileDevice::ReadThreadContext> 0x00BF3880
    template<typename T>
    Array<T>::Array()
    {
        m_pData = nullptr;
        m_size = 0;
    }

    // <NativeFileDevice::ReadThreadContext> 0x00BF3890
    template<typename T>
    Array<T>::~Array()
    {
    }

    // <NativeFileDevice::ReadThreadContext> 0x00C00B10
    template<typename T>
    bool Array<T>::create(MemoryAllocator* pAllocator, uint32 size, uint32 alignment)
    {
        m_size = size;
        if (size != 0)
        {
            m_pData = (T*)pAllocator->allocate(size * sizeof(T), alignment, 0, nullptr);
            if (m_pData == nullptr)
            {
                return false;
            }
            for (uint32 i = 0; i < m_size; ++i)
            {
                new (&m_pData[i]) T();
            }
        }
        return true;
    }

    // <NativeFileDevice::ReadThreadContext> 0x00C045A0
    template<typename T>
    void Array<T>::destroy(MemoryAllocator* pAllocator)
    {
        if (m_pData != nullptr)
        {
            pAllocator->free(m_pData);
            m_pData = nullptr;
            m_size = 0;
        }
    }

    template<typename T>
    bool Array<T>::isEmpty() const
    {
        return m_size == 0;
    }

    // <NativeFileDevice::ReadThreadContext> 0x00BF38A0
    template<typename T>
    uint32 Array<T>::getSize() const
    {
        return m_size;
    }

    template<typename T>
    const T& Array<T>::operator[](uint32 index) const
    {
        return m_pData[index];
    }

    // <NativeFileDevice::ReadThreadContext> 0x00BF38B0
    template<typename T>
    T& Array<T>::operator[](uint32 index)
    {
        return m_pData[index];
    }

    template<typename T>
    const T* Array<T>::getStart() const
    {
        return m_pData;
    }

    template<typename T>
    T* Array<T>::getStart()
    {
        return m_pData;
    }

    template<typename T>
    const T* Array<T>::getEnd() const
    {
        return m_pData + m_size;
    }

    template<typename T>
    T* Array<T>::getEnd()
    {
        return m_pData + m_size;
    }
}
