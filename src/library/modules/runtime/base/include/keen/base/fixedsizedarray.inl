// Original: keen/base/fixedsizedarray.inl
//
// The FixedSizedArray methods rfg.exe instantiates (<ServerInfo,256>); no bounds checks in the release build.
#pragma once

namespace keen
{
    // 0x005D3470 (:15)
    template<typename T, uint32 N>
    FixedSizedArray<T, N>::FixedSizedArray()
    {
        m_size = 0;
    }

    // 0x005D3480 (:44)
    template<typename T, uint32 N>
    bool FixedSizedArray<T, N>::isFull() const
    {
        return m_size == N;
    }

    // 0x005D3490 (:50)
    template<typename T, uint32 N>
    uint32 FixedSizedArray<T, N>::getSize() const
    {
        return m_size;
    }

    // 0x005D34A0 (:56)
    template<typename T, uint32 N>
    uint32 FixedSizedArray<T, N>::getCapacity() const
    {
        return N;
    }

    // 0x005D34B0 (:76)
    template<typename T, uint32 N>
    const T& FixedSizedArray<T, N>::operator[](uint32 index) const
    {
        return m_data[index];
    }

    // 0x005D34C0 (:92)
    template<typename T, uint32 N>
    void FixedSizedArray<T, N>::clear()
    {
        m_size = 0;
    }

    // 0x005D34D0 (:113)
    template<typename T, uint32 N>
    T& FixedSizedArray<T, N>::pushBack()
    {
        return m_data[m_size++];
    }
}
