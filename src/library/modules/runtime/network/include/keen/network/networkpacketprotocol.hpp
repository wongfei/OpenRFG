// Original: library/modules/runtime/network/include/keen/network/networkpacketprotocol.hpp
// Implementation: library/modules/runtime/network/sources/global/networkpacketprotocol.cpp
//
// The packet protocol: packets of messages on protocol channels, with packet ids, acknowledges, flags and custom footer
// data. The encoder opens a packet, writes messages through a BitWriteBuffer and closes it; the decoder decodes received
// packets into messages and the acknowledges they carry.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/array.hpp"
#include "keen/base/bitreader.hpp"
#include "keen/base/sizedarray.hpp"

namespace keen
{
    namespace network
    {
        struct PacketProtocolReceivedMessage;
    }
}

namespace keen
{
    struct BitWriteBuffer;
    class MemoryAllocator;

    namespace network
    {
        namespace PacketProtocol
        {
            enum AcknowledgeMethod
            {
                AcknowledgeMethod_None = 0,
                AcknowledgeMethod_Ack = 1,
            };

        }
    }

    namespace network
    {
        // size 0x8
        struct CustomData
        {
            const void* pData; // 0x00
            uint32 bitSize; // 0x04
        };

        // size 0x8
        struct ProtocolChannel
        {
            PacketProtocolReceivedMessage* pFirstReceivedMessage; // 0x00
            uint32 receivedMessageCount; // 0x04
        };

        // size 0x14
        struct PacketProtocolHeader
        {
            void clear(); // 0x00C81330 (:32)

            uint32 packetId; // 0x00
            uint32 flags; // 0x04
            PacketProtocol::AcknowledgeMethod acknowledgeMethod; // 0x08
            uint32 messageCount; // 0x0C
            uint8 ackCount; // 0x10
            uint16 customDataBitSize; // 0x12
        };

        // size 0x54
        struct PacketProtocolEncoder
        {
            void clear(); // 0x00C83830 (:64)
            PacketProtocolEncoder(); // 0x00C838F0
            ~PacketProtocolEncoder(); // 0x00C825B0

            PacketProtocolHeader packetHeader; // 0x00
            uint32 protocolChannelCount; // 0x14
            uint32 requiredProtocolChannelBits; // 0x18
            uint32 maxMessageCount; // 0x1C
            uint32 maxAcknowledgesCount; // 0x20
            uint32 currentMsgBitPosition; // 0x24
            uint32 totalMsgBitSize; // 0x28
            Array<uint8> packet; // 0x2C
            SizedArray<CustomData> customData; // 0x34
            uint32 currentPacketId; // 0x40
            bool packetIsClosed; // 0x44
            uint32 lastPacketByteSize; // 0x48
            uint32 footerSizeInBits; // 0x4C
            uint32 flagBitCount; // 0x50
        };

        // size 0x10
        struct PacketProtocolReceivedMessage
        {
            BitReadBuffer bitReadBuffer; // 0x00
            uint32 protocolChannel; // 0x0C
        };

        // size 0x48
        struct PacketProtocolDecoder
        {
            void clear(); // 0x00C81350 (:98)
            PacketProtocolDecoder(); // 0x00C83B40
            ~PacketProtocolDecoder(); // 0x00C825C0

            PacketProtocolHeader packetHeader; // 0x00
            uint32 requiredProtocolChannelBits; // 0x14
            Array<ProtocolChannel> protocolChannels; // 0x18
            Array<uint8> packet; // 0x20
            Array<PacketProtocolReceivedMessage> receivedMessages; // 0x28
            bool packetIdReceived; // 0x30
            uint32 newestReceivedPacketId; // 0x34
            uint32 packetSize; // 0x38
            uint32 flagBitCount; // 0x3C
            uint32 customDataBitOffset; // 0x40
            uint32 customDataBitPosition; // 0x44
        };

    }

    namespace network
    {
        namespace PacketProtocol
        {
            PacketProtocolEncoder* createEncoder(uint32 packetSize, uint32 protocolChannelCount, uint32 maxMessageCountPerPacket, uint32 maxFooterCustomDataAllocationCount, uint32 maxAcknowledgesCount, MemoryAllocator* pAllocator); // 0x00C85BE0 (:179)
            void destroyEncoder(PacketProtocolEncoder* pEncoder, MemoryAllocator* pAllocator); // 0x00C88420 (:211)
            uint32 getMaxAcknowledges(const PacketProtocolEncoder* pEncoder); // 0x00C813A0 (:218)
            bool openPacket(PacketProtocolEncoder* pEncoder, SizedArray<uint32>* pAcknowledges, uint32* pPacketId); // 0x00C85CE0 (:238)
            void setPacketData(PacketProtocolEncoder* pEncoder, const void* pData, uint32 dataSizeInBits, uint32* pPacketId); // 0x00C83910 (:301)
            bool openMessage(PacketProtocolEncoder* pEncoder, uint32 protocolChannel, BitWriteBuffer* pBitWriteBuffer, uint32* pPacketId, AcknowledgeMethod acknowledge); // 0x00C85FC0 (:322)
            void closeMessage(PacketProtocolEncoder* pEncoder, const BitWriteBuffer& bitWriteBuffer); // 0x00C86260 (:383)
            void closePacket(PacketProtocolEncoder* pEncoder); // 0x00C86470 (:411)
            void* getLastPacket(PacketProtocolEncoder* pEncoder, uint32* pPacketSize); // 0x00C83990 (:469)
            const void* getLastReceivedPacket(const PacketProtocolDecoder* pDecoder, uint32* pPacketSize); // 0x00C839B0 (:481)
            uint32 getFreeBitSizeInPacket(const PacketProtocolEncoder* pEncoder); // 0x00C839D0 (:491)
            uint32 getFreeMessageCountInPacket(const PacketProtocolEncoder* pEncoder); // 0x00C83A70 (:521)
            uint32 getMaxFreeMessageSizeInPacket(const PacketProtocolEncoder* pEncoder); // 0x00C83AD0 (:530)
            PacketProtocolDecoder* createDecoder(uint32 packetSize, uint32 protocolChannelCount, uint32 maxMessageCountPerPacket, MemoryAllocator* pAllocator); // 0x00C86DA0 (:547)
            void destroyDecoder(PacketProtocolDecoder* pDecoder, MemoryAllocator* pAllocator); // 0x00C88480 (:578)
            void decodePacket(PacketProtocolDecoder* pDecoder, const void* pPacket, uint32 packetSize, SizedArray<uint32>* pReceivedAcknowledges); // 0x00C83B60 (:587)
            PacketProtocolReceivedMessage* getReceivedMessages(PacketProtocolDecoder* pDecoder, uint32* pMessageCount); // 0x00C84450 (:771)
            bool getReceivedPacketId(const PacketProtocolDecoder* pDecoder, uint32* pPacketId); // 0x00C813B0 (:785)
            uint32 getMaxPacketSize(const PacketProtocolEncoder* pEncoder); // 0x00C84480 (:798)
            uint32 getMaxPacketSize(const PacketProtocolDecoder* pDecoder); // 0x00C84490 (:805)
            uint32 getWrappedPacketIdDelta(uint32 packetId, uint32 newPacketId); // 0x00C813D0 (:812)
            bool isNewerPacketId(uint32 currentPacketId, uint32 newPacketId); // 0x00C813F0 (:826)
            uint32 getNewestDecodedPacketId(const PacketProtocolDecoder* pDecoder); // 0x00C81420 (:840)
            uint32 getNextPacketId(uint32 packetId); // 0x00C81430 (:847)
            bool allocateCustomData(PacketProtocolEncoder* pEncoder, const void* pData, uint32 bitCount); // 0x00C844A0 (:859)
            void freeCustomData(PacketProtocolEncoder* pEncoder, const void* pData); // 0x00C86E80 (:879)
            uint32 getMaxFooterCustomAllocationCount(const PacketProtocolEncoder* pEncoder); // 0x00C844E0 (:895)
            uint32 getPacketDataBitSize(const PacketProtocolEncoder* pEncoder); // 0x00C81440 (:902)
            const void* getIntermediatePacketData(const PacketProtocolEncoder* pEncoder); // 0x00C844F0 (:909)
            uint32 registerFlags(PacketProtocolEncoder* pEncoder, uint32 flagBitCount); // 0x00C81450 (:916)
            void setFlags(PacketProtocolEncoder* pEncoder, uint32 flags, uint32 flagBitOffset); // 0x00C81470 (:928)
            uint32 getFlags(const PacketProtocolEncoder* pEncoder, uint32 flagBitOffset); // 0x00C81490 (:936)
            uint32 registerFlags(PacketProtocolDecoder* pDecoder, uint32 flagBitCount); // 0x00C814A0 (:943)
            uint32 getFlags(const PacketProtocolDecoder* pDecoder, uint32 flagBitOffset); // 0x00C814C0 (:955)
            uint32 getPacketIdFromData(const void* pData, uint32 dataBitSize); // 0x00C84500 (:962)
            bool getCustomData(PacketProtocolDecoder* pDecoder, void* pData, uint32 dataBitSize); // 0x00C845A0 (:972)
            uint32 getCustomDataBitSize(const PacketProtocolDecoder* pDecoder); // 0x00C814D0 (:1005)
            void invalidateReceiveData(PacketProtocolDecoder* pDecoder); // 0x00C846E0 (:1012)
            void reset(PacketProtocolEncoder* pEncoder); // 0x00C84710 (:1026)
            void reset(PacketProtocolDecoder* pDecoder); // 0x00C84750 (:1036)
        }
    }
}
