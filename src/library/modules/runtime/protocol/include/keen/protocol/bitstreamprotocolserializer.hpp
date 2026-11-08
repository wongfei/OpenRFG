// Original: library/modules/runtime/protocol/include/keen/protocol/bitstreamprotocolserializer.hpp
// Implementation: library/modules/runtime/protocol/sources/global/bitstreamprotocolserializer.cpp
//
// Serializes protocol messages to and from bit streams, driven by their protocol descriptions.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    class BitReader;
    class BitWriter;
    struct ProtocolMessageDescription;

    namespace protocol
    {
        uint32 getStringLengthUpToLimit(const char* pSource, uint32 limit); // 0x00CD8F00 (:64)
        void writeMessageIntoBitStream(BitWriter* pBitWriter, const void* pMessage, const ProtocolMessageDescription& messageDescription); // 0x00CD9520 (:95)
        void readMessageFromBitStream(void* pMessage, BitReader* pBitReader, const ProtocolMessageDescription& messageDescription); // 0x00CDAE40 (:267)
    }
}
