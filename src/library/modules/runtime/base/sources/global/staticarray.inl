// Original: library/modules/runtime/base/sources/global/staticarray.inl (included by staticarray.hpp)
//
// Partial: the members used by the recovered code so far (ringbuffer.inl).
#pragma once

namespace keen
{
    // <MaterialMesh> 0x00CC8BE0 (:12)
    template<typename T>
    StaticArray<T>::StaticArray()
    {
        m_pData = nullptr;
        m_size = 0;
    }

    // <MaterialMesh> 0x00CC8BF0 (:32)
    template<typename T>
    StaticArray<T>::~StaticArray()
    {
    }

    // <MaterialMesh> 0x00CC8C00 (:37)
    template<typename T>
    void StaticArray<T>::setData(T* pData, uint32 size)
    {
        m_pData = pData;
        m_size = size;
    }

    template<typename T>
    T* StaticArray<T>::releaseData()
    {
        T* pData = m_pData;
        m_pData = nullptr;
        m_size = 0;
        return pData;
    }

    // <MaterialMesh> 0x00CC8C10 (:50)
    template<typename T>
    uint32 StaticArray<T>::getSize() const
    {
        return m_size;
    }

    // <MaterialMesh> 0x00CC8C20 (:70)
    template<typename T>
    const T& StaticArray<T>::operator[](uint32 index) const
    {
        return m_pData[index];
    }

    // <MaterialMesh> 0x00CC8C30 (:62)
    template<typename T>
    T& StaticArray<T>::operator[](uint32 index)
    {
        return m_pData[index];
    }
}
