// Original: library/modules/runtime/base/include/keen/base/radixsort.hpp
//
// RadixSort, sorts keys with a radix sort (render command sorting).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x10
    class RadixSort
    {
    public:
        RadixSort(uint32* pRanks, uint32 elementCount); // 0x004D42A0 (.h:21)
        RadixSort& sort(uint32* pInput); // 0x004FF470 (.h:25)

    protected:
        uint32* m_pRanks; // 0x00
        uint32* m_pRanks2; // 0x04
        uint32 m_elementCount; // 0x08
        bool m_invalidRanks; // 0x0C
    };

    // ---- inline members (no .cpp home)

    // 0x004D42A0 (.h:21)
    // pRanks holds 2 * elementCount indices: the result and the ping-pong buffer.
    inline RadixSort::RadixSort(uint32* pRanks, uint32 elementCount)
    {
        m_pRanks = pRanks;
        m_pRanks2 = pRanks + elementCount;
        m_elementCount = elementCount;
        m_invalidRanks = true;
    }

    // 0x004FF470 (.h:25)
    // A stable least significant byte first sort of the 32-bit keys: m_pRanks receives the key indices in ascending key
    // order (radixSortPasses<uint32,2> 0x004D93D0, inlined: histograms of all four bytes, then one pass per byte from the
    // previous ranks, swapping the rank buffers).
    inline RadixSort& RadixSort::sort(uint32* pInput)
    {
        uint32 histogram[4][256];
        for (uint32 b = 0; b < 4; ++b)
        {
            for (uint32 i = 0; i < 256; ++i)
            {
                histogram[b][i] = 0;
            }
        }
        const uint8* pBytes = (const uint8*)pInput;
        for (uint32 i = 0; i < m_elementCount; ++i)
        {
            histogram[0][pBytes[i * 4 + 0]]++;
            histogram[1][pBytes[i * 4 + 1]]++;
            histogram[2][pBytes[i * 4 + 2]]++;
            histogram[3][pBytes[i * 4 + 3]]++;
        }
        for (uint32 b = 0; b < 4; ++b)
        {
            uint32* pLink[256];
            pLink[0] = m_pRanks2;
            for (uint32 i = 1; i < 256; ++i)
            {
                pLink[i] = pLink[i - 1] + histogram[b][i - 1];
            }
            if (!m_invalidRanks)
            {
                for (uint32* pRank = m_pRanks; pRank != m_pRanks + m_elementCount; ++pRank)
                {
                    const uint32 index = *pRank;
                    *pLink[pBytes[index * 4 + b]]++ = index;
                }
            }
            else
            {
                for (uint32 index = 0; index < m_elementCount; ++index)
                {
                    *pLink[pBytes[index * 4 + b]]++ = index;
                }
                m_invalidRanks = false;
            }
            uint32* pSwap = m_pRanks;
            m_pRanks = m_pRanks2;
            m_pRanks2 = pSwap;
        }
        return *this;
    }
}
