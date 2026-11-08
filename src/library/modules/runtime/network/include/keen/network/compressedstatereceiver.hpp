// Original: library/modules/runtime/network/include/keen/network/compressedstatereceiver.hpp
// Implementation: library/modules/runtime/network/sources/global/compressedstatereceiver.cpp
//
// The receiving side of a compressed state stream: applies full or delta updates to the last acknowledged state.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct CompressedState;
    struct CompressedStateAllocator;
    class MemoryAllocator;

    // size 0xC
    struct CompressedStateReceiver
    {
        CompressedStateAllocator* pStateAllocator; // 0x00
        CompressedState* pCurrentState; // 0x04
        uint32 stateSize; // 0x08
    };

    namespace compressedstate
    {
        uint32 getCompressedStateReceiver(); // 0x00C80800 (:27)
        CompressedStateReceiver* createReceiver(MemoryAllocator* pAllocator, CompressedStateAllocator* pStateAllocator, uint32 stateSize); // 0x00C82880 (:32)
        void destroyReceiver(CompressedStateReceiver* pReceiver, MemoryAllocator* pAllocator); // 0x00C828B0 (:66)
        void resetReceiver(CompressedStateReceiver* pReceiver); // 0x00C80810 (:73)
        bool processReceivedUpdate(CompressedStateReceiver* pReceiver, const uint8* pData, uint32 deltaSize, uint32 entityId); // 0x00C828F0 (:80)
        uint32 getCurrentStateId(const CompressedStateReceiver* pReceiver); // 0x00C80840 (:236)
        const uint8* getCurrentStateData(const CompressedStateReceiver* pReceiver); // 0x00C80860 (:250)
        uint32 getStateSize(const CompressedStateReceiver* pReceiver); // 0x00C80880 (:264)
    }
}
