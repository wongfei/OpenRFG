// Original: library/modules/runtime/network/include/keen/network/errorsimulationsocket.hpp
// Implementation: library/modules/runtime/network/sources/global/errorsimulationsocket.cpp
//
// Network error simulation: a socket wrapper that delays, drops, duplicates and reorders packets and limits the bandwidth,
// switching between a good and a bad condition. The settings can follow ini variables. Placement of
// ErrorSimulationSettings inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/array.hpp"
#include "keen/base/randomnumbergenerator.hpp"
#include "keen/base/ringbuffer.hpp"
#include "keen/network/networkaddress.hpp"

namespace keen
{
    class MemoryAllocator;

    namespace network
    {
        enum Condition
        {
            Condition_Good = 0,
            Condition_Bad = 1,
        };

    }

    // size 0x1C
    struct ErrorSimulationSettingsPart
    {
        ErrorSimulationSettingsPart(); // 0x00C80A70 (.h:20)

        float lossRatio; // 0x00
        uint32 minLatency; // 0x04
        uint32 maxLatency; // 0x08
        float duplicationRatio; // 0x0C
        float transitionProbability; // 0x10
        float burstTime; // 0x14
        uint32 bandwidthLimitInKBits; // 0x18
    };

    // size 0x38
    struct ErrorSimulationSettings
    {
        ErrorSimulationSettings(); // 0x00C80AA0

        ErrorSimulationSettingsPart goodCondition; // 0x00
        ErrorSimulationSettingsPart badCondition; // 0x1C
    };

    // size 0xA40
    struct ErrorSimulationSocket
    {
        // size 0x5F0
        struct StoredPacket
        {
            NetworkAddress targetAddress; // 0x00
            uint8 data[1500]; // 0x08
            uint32 size; // 0x5E4
            uint32 deliveryTime; // 0x5E8
            uint32 seriesCounter; // 0x5EC
        };

        // size 0x8
        struct BandwidthData
        {
            float timeStep; // 0x00
            uint32 byteCount; // 0x04
        };

        ErrorSimulationSocket(); // 0x00C858D0
        ~ErrorSimulationSocket(); // 0x00C857E0

        Array<StoredPacket> storedPackets; // 0x00
        MemoryAllocator* pAllocator; // 0x08
        ErrorSimulationSettingsPart settings[2]; // 0x0C
        RandomNumberGenerator randomizer; // 0x44
        network::Condition currentSettingToUse; // 0xA08
        uint32 msInCurrentState; // 0xA0C
        uint32 msWhenLastCallingSendTo; // 0xA10
        uint32 currentTime; // 0xA14
        uint32 seriesCounter; // 0xA18
        bool updateFromIniVariables; // 0xA1C
        RingBuffer<BandwidthData> bandwidthData; // 0xA20
        BandwidthData* pCurrentBandwidthData; // 0xA34
        uint32 averageBandwidthInKBits; // 0xA38
        float bandwidthPacketDropProbability; // 0xA3C
    };

    namespace network
    {
        namespace ErrorSimulation
        {
            void disableUpdateFromIniVariables(ErrorSimulationSocket* pSocket); // 0x00C80DE0 (:87)
            void setSimulationSettings(ErrorSimulationSocket* pSocket, const ErrorSimulationSettings& settings); // 0x00C80DF0 (:92)
            ErrorSimulationSocket* createSocket(MemoryAllocator* pAllocator, uint32 maxStoredPacketsCount, uint32 randomSeed, uint32 maxBandwidthBufferFrames); // 0x00C87EB0 (:159)
            void destroySocket(ErrorSimulationSocket* pSocket); // 0x00C88DF0 (:202)
            void reset(ErrorSimulationSocket* pSocket); // 0x00C82DF0 (:210)
            void sendTo(ErrorSimulationSocket* pSocket, const NetworkAddress& targetAddress, const void* pMessage, uint32 messageSize); // 0x00C82E20 (:221)
        }
    }

    ErrorSimulationSocket::StoredPacket* findPacketToForward(ErrorSimulationSocket* pSocket, NetworkAddress* pTargetAddress, uint32* pMessageSize); // 0x00C83050 (:339)

    namespace network
    {
        namespace ErrorSimulation
        {
            const void* peekPacketToForward(ErrorSimulationSocket* pSocket, NetworkAddress* pTargetAddress, uint32* pMessageSize); // 0x00C830E0 (:381)
            const void* popPacketToForward(ErrorSimulationSocket* pSocket, NetworkAddress* pTargetAddress, uint32* pMessageSize); // 0x00C83110 (:396)
            float getBandwidthPacketDropProbability(const ErrorSimulationSocket* pSocket); // 0x00C80EF0 (:409)
            void update(ErrorSimulationSocket* pSocket, float timeStep); // 0x00C88130 (:416)
        }
    }
}
