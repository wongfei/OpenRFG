// Original: library/modules/runtime/network/include/keen/network/localloopback.hpp
// Implementation: library/modules/runtime/network/sources/global/localloopback.cpp
//
// The local loopback: two ring buffers of messages between the local server and the local client of a listen server.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/ringbuffer.hpp"
#include "keen/base/sizedarray.hpp"
#include "keen/base/win32/mutex_win32.hpp"

namespace keen
{
    class MemoryAllocator;

    // size 0xB0
    class LocalLoopback
    {
    public:
        // size 0x8
        struct Message
        {
            uint32 startIndex; // 0x00
            uint32 size; // 0x04
        };

        // size 0x58
        struct Data
        {
            Mutex mutex; // 0x00
            SizedArray<uint8> data; // 0x38
            RingBuffer<Message> messageData; // 0x44
        };

        void create(MemoryAllocator* pAllocator, uint32 bufferSize, uint32 maxMessageCount); // 0x00C85940 (:7)
        void destroy(MemoryAllocator* pAllocator); // 0x00C85A40 (:18)
        uint32 sendToLocalServer(const void* pData, uint32 dataSize); // 0x00C88E50 (:29)
        uint32 recvFromLocalServer(void* pData, uint32 capacity, uint32 packetIndex); // 0x00C88E90 (:34)
        bool hasPendingDataFromLocalServer();
        uint32 sendToLocalClient(const void* pData, uint32 dataSize); // 0x00C88EB0 (:39)
        uint32 recvFromLocalClient(void* pData, uint32 capacity, uint32 packetIndex); // 0x00C88EF0 (:44)
        bool hasPendingDataFromLocalClient();
        void clearPendingDataFromLocalServer(bool);
        void clearPendingDataFromLocalClient(bool);
        uint32 getPendingMessageCountFromLocalServer();
        uint32 getPendingMessageCountFromLocalClient();
    private:
        static uint32 sendDataTo(Data& data, const void* pData, uint32 dataSize); // 0x00C88360 (:49)
        static uint32 copyDataTo(Data& data, const void* pData, uint32 dataSize); // 0x00C85B00 (:61)
        static uint32 recvDataFrom(Data& data, void* pData, uint32 capacity, uint32 packetIndex); // 0x00C883A0 (:97)
        static bool hasPendingData(Data& data); // 0x00C83140 (:131)
        static uint32 getPendingMessageCount(Data& data); // 0x00C83170 (:138)
        static void clearData(Data& data, bool resetErrorSimulation); // 0x00C831A0 (:145)

        Data m_serverToClientData; // 0x00
        Data m_clientToServerData; // 0x58
    };
}
