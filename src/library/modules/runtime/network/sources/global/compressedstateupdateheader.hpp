// Original: library/modules/runtime/network/sources/global/compressedstateupdateheader.hpp
// Implementation: library/modules/runtime/network/sources/global/compressedstateupdateheader.cpp
//
// The header of a compressed state update (state ids of the update and of its delta base).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x8
    struct CompressedStateUpdateHeader
    {
        bool canBeDeltaCompressed() const; // 0x00C807E0 (.h:19)
        bool readFromByteStream(const uint8** pData, uint32* pRemainingSize); // 0x00C809B0 (:9)
        bool writeToByteStream(uint8** pData, uint32* pRemainingSize) const; // 0x00C80A10 (:45)

        uint32 targetStateId; // 0x00
        uint32 baseStateId; // 0x04
    };
}
