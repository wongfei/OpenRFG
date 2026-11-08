// Original: library/modules/runtime/network/include/keen/network/compressedstatesender.hpp
// Implementation: library/modules/runtime/network/sources/global/compressedstatesender.cpp
//
// The sending side of a compressed state stream: keeps the states not yet acknowledged by every receiver and fills
// send buffers with a delta against each receiver's last acknowledged state (or a full update).
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/array.hpp"

namespace keen
{
    struct CompressedState;
    struct CompressedStateAllocator;
    class MemoryAllocator;

    // size 0x4
    struct ReceiverData
    {
        uint32 lastAcknowledgedStateId; // 0x00
    };

    // size 0x18
    struct CompressedStateSender
    {
        CompressedStateSender(); // 0x00C82A20
        ~CompressedStateSender(); // 0x00C825A0

        CompressedStateAllocator* pStateAllocator; // 0x00
        CompressedState* pCurrentState; // 0x04
        Array<ReceiverData> receiverData; // 0x08
        uint32 stateSize; // 0x10
        uint32 nextStateId; // 0x14
    };

    namespace compressedstate
    {
        CompressedStateSender* createSender(MemoryAllocator* pAllocator, CompressedStateAllocator* pStateAllocator, uint32 stateSize, uint32 receiverCount); // 0x00C85830 (:35)
        void destroySender(CompressedStateSender* pSender, MemoryAllocator* pAllocator); // 0x00C87E60 (:53)
        bool updateCurrentStateData(CompressedStateSender* pSender, const void* pData, uint32 size); // 0x00C80890 (:61)
        uint32 getCurrentStateId(const CompressedStateSender* pSender); // 0x00C80930 (:88)
        void resetReceiver(CompressedStateSender* pSender, uint32 receiverIndex); // 0x00C82A30 (:102)
        uint32 getAcknowledgedReceiverStateId(const CompressedStateSender* pSender, uint32 receiverIndex); // 0x00C82A50 (:109)
        void processReceiverAcknowledge(CompressedStateSender* pSender, uint32 receiverIndex, uint32 acknowledgedStateId); // 0x00C82A60 (:115)
        uint32 fillSendBuffer(const CompressedStateSender* pSender, bool* pIsFullUpdate, uint8* pTargetData, uint32 capacity, uint32 receiverIndex, uint32 entityId); // 0x00C82AE0 (:136)
        uint32 findOldestSavedState(const CompressedStateSender* pSender); // 0x00C80950 (:318)
        uint32 getStateSize(const CompressedStateSender* pSender); // 0x00C80970 (:335)
        uint32 getStateCount(const CompressedStateSender* pSender); // 0x00C80980 (:340)
        uint32 getFullUpdateByteCount(const CompressedStateSender* pSender); // 0x00C809A0 (:352)
    }
}
