// Original: library/modules/runtime/base/include/keen/base/search.hpp
// Implementation: library/modules/runtime/base/sources/global/search.cpp
//
// Binary search helpers (function templates, instantiations listed as comments).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    const uint32* searchBinary(const uint32* pData, uint32 numArrayEntries, uint32 searchValue); // 0x00BF17B0 (:45)
    const uint32* searchBinary(const uint32* pData, uint32 numArrayEntries, uint32 searchValue, uint32 entryStride); // 0x00BF17F0 (:84)
    const uint16* searchBinary(const uint16* pData, uint32 numArrayEntries, uint16 searchValue); // 0x00BF1840 (:122)
    const uint16* searchBinary(const uint16* pData, uint32 numArrayEntries, uint16 searchValue, uint32 entryStride); // 0x00BF1880 (:161)
    // template instance: const CapitalizationEntry* searchBinary<CapitalizationEntry>(const CapitalizationEntry* pData, uint32 numArrayEntries, const CapitalizationEntry* pSearchValue, int (*compareFunc)(const CapitalizationEntry*, const CapitalizationEntry*)); // 0x00BF3D40 (search.inl:5)
}
