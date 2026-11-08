// Original: library/modules/runtime/base/sources/global/search.cpp
//
// Binary search over sorted uint32 keys.
//
// Partial: the strided uint32 variant (PakFileDevice directory lookup).

#include "keen/base/search.hpp"

namespace keen
{
    // 0x00BF17F0 (:84)
    const uint32* searchBinary(const uint32* pData, uint32 numArrayEntries, uint32 searchValue, uint32 entryStride)
    {
        uint32 low = 0;
        uint32 high = numArrayEntries;
        while (low < high)
        {
            const uint32 middle = (low + high) >> 1;
            const uint32* pEntry = (const uint32*)((const uint8*)pData + middle * entryStride);
            if (searchValue == *pEntry)
            {
                return pEntry;
            }
            if (searchValue < *pEntry)
            {
                high = middle;
            }
            else
            {
                low = middle + 1;
            }
        }
        return nullptr;
    }
}
