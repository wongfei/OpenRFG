// Original: library/modules/runtime/network/include/keen/network/networkremotedata.hpp
// Implementation: library/modules/runtime/network/sources/global/networkremotedata.cpp
//
// The packet protocol state kept per remote station: its encoder and decoder and the packet ids to acknowledge.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/sizedarray.hpp"

namespace keen
{
    namespace network
    {
        struct PacketProtocolDecoder;
        struct PacketProtocolEncoder;
    }
}

namespace keen
{
    class MemoryAllocator;

    // size 0x24
    struct NetworkRemoteData
    {
        network::PacketProtocolEncoder* pEncoder; // 0x00
        network::PacketProtocolDecoder* pDecoder; // 0x04
        SizedArray<uint32> packetIdsToAcknowledge; // 0x08
        SizedArray<uint32> receivedAcknowledges; // 0x14
        bool packetOpened; // 0x20
        bool wasPacketOpened; // 0x21
    };

    namespace network
    {
        void initializeNetworkRemoteData(NetworkRemoteData* pData, MemoryAllocator* pAllocator, uint32 encoderPacketSize, uint32 protocolChannelCount); // 0x00C86EE0 (:11)
        void shutdownNetworkRemoteData(NetworkRemoteData* pData, MemoryAllocator* pAllocator); // 0x00C884F0 (:28)
        bool wasPacketOpened(const NetworkRemoteData* pData); // 0x00C814E0 (:38)
        void clearWasPacketOpened(NetworkRemoteData* pData); // 0x00C814F0 (:45)
        bool hasAcknowledges(const NetworkRemoteData* pData); // 0x00C847B0 (:52)
        bool openPacket(NetworkRemoteData* pData); // 0x00C86FC0 (:59)
        void closePacket(NetworkRemoteData* pData); // 0x00C86FF0 (:75)
        uint32 getFreePacketBits(const NetworkRemoteData* pData); // 0x00C847C0 (:86)
        void reset(NetworkRemoteData* pData); // 0x00C847D0 (:91)
        void decodePacket(NetworkRemoteData* pData, const void* pPacket, uint32 packetSize); // 0x00C84820 (:105)
    }
}
