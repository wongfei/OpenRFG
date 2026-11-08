// Original: library/modules/runtime/network/include/keen/network/errorsimulationpresets.hpp
// Implementation: library/modules/runtime/network/sources/global/errorsimulationpresets.cpp
//
// The error simulation presets (perfect, LAN, DSL, bad connections, ...). Placement of ErrorSimulationPresetId inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/network/errorsimulationsocket.hpp"

namespace keen
{
    enum ErrorSimulationPresetId
    {
        ErrorSimulationPresetId_NoErrorSimulation = 0,
        ErrorSimulationPresetId_10PercentPacketLoss = 1,
        ErrorSimulationPresetId_80PercentPacketLoss = 2,
        ErrorSimulationPresetId_6PercentPacketLoss_150msLag = 3,
        ErrorSimulationPresetId_150msLagOnly = 4,
        ErrorSimulationPresetId_250msLagOnly = 5,
        ErrorSimulationPresetId_500msLagOnly = 6,
        ErrorSimulationPresetId_DuplicationOnly = 7,
        ErrorSimulationPresetId_MinimumWorkingConnection = 8,
        ErrorSimulationPresetId_TypicalConnection = 9,
        ErrorSimulationPresetId_Xb360_TCR = 10,
        ErrorSimulationPresetId_Count = 11,
    };

    // size 0x268
    struct ErrorSimulationPresets
    {
        ErrorSimulationPresets(); // 0x00C82D80 (:9)

        ErrorSimulationSettings presets[11]; // 0x00
    };

    namespace network
    {
        void fillErrorSimulationSettings(ErrorSimulationSettings* pTarget, ErrorSimulationPresetId id); // 0x00C80AF0 (:33)
        const char* getErrorSimulationPresetString(ErrorSimulationPresetId id); // 0x00C80DD0 (:230)
    }
}
