// Original: game/build/win32/steam/master/stats/rfgstats_stdf.hpp
// Implementation: game/build/win32/steam/master/stats/rfgstats_stdf.cpp
//
// Generated from the stat definition file: kaiko::StatId and the stat update map (unit4::StatUpdateData, see
// systemservices.hpp). Placement of StatId inferred.
#pragma once

#include "keen/base/types.hpp"
#include "unit4/systemservices/systemservices.hpp"

namespace kaiko
{
    enum StatId
    {
        StatId_TotalEnemiesKilledSP = 0,
        StatId_TotalBuildingsDestroyed = 1,
        StatId_TotalVehiclesDestroyed = 2,
        StatId_TotalDistanceTravelled = 3,
        StatId_TotalGamesMP = 4,
        StatId_TotalWinsMP = 5,
        StatId_TotalExpMP = 6,
        StatId_TotalEnemiesKilledMP = 7,
        StatId_WreckingCrewDestructionAmount = 8,
        StatId_Count = 9,
    };

    const unit4::StatUpdateData* getStatUpdateDataMap(); // 0x00CE6180 (:101)
    const uint32 getStatCount(); // 0x00CE6190 (:102)
}
