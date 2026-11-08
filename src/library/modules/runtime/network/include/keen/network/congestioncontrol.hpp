// Original: library/modules/runtime/network/include/keen/network/congestioncontrol.hpp
// Implementation: library/modules/runtime/network/sources/global/congestioncontrol.cpp
//
// Congestion control: adapts the bandwidth and packet rate limits from the local and remote network metrics. Placement of
// NetworkMetrics inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x8
    struct NetworkMetrics
    {
        uint32 averageKBitsPerSeconds; // 0x00
        uint32 averagePacketsPerSeconds; // 0x04
    };

    // size 0x10
    struct CongestionControlState
    {
        uint32 maxKBitsPerSeconds; // 0x00
        uint32 maxPacketsPerSeconds; // 0x04
        float dbgKbitsFract; // 0x08
        float kbitsGain; // 0x0C
    };

    namespace CongestionControl
    {
        void reset(CongestionControlState* pState); // 0x00C80A50 (:14)
        void update(CongestionControlState* pState, const NetworkMetrics& currentLocalMetrics, const NetworkMetrics& currentRemoteMetrics); // 0x00C82C30 (:25)
    }
}
