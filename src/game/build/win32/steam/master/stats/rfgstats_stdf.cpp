// Original: game/build/win32/steam/master/stats/rfgstats_stdf.cpp
//
// Generated from the stat definition file (stub_impl.md S2): the Steam stat name of each StatId. The table is rfg.exe's
// data (0x01348768), read from the image.

#include "game/build/win32/steam/master/stats/rfgstats_stdf.hpp"

namespace kaiko
{
    // 0x01348768. The distance stat's Steam name is "TotalDistanceOnFoot" (StatId_TotalDistanceTravelled).
    static const unit4::StatUpdateData s_statUpdateDataMap[StatId_Count] =
    {
        { StatId_TotalEnemiesKilledSP, "TotalEnemiesKilledSP" },
        { StatId_TotalBuildingsDestroyed, "TotalBuildingsDestroyed" },
        { StatId_TotalVehiclesDestroyed, "TotalVehiclesDestroyed" },
        { StatId_TotalDistanceTravelled, "TotalDistanceOnFoot" },
        { StatId_TotalGamesMP, "TotalGamesMP" },
        { StatId_TotalWinsMP, "TotalWinsMP" },
        { StatId_TotalExpMP, "TotalExpMP" },
        { StatId_TotalEnemiesKilledMP, "TotalEnemiesKilledMP" },
        { StatId_WreckingCrewDestructionAmount, "WreckingCrewDestructionAmount" },
    };

    // 0x00CE6180 (:101)
    const unit4::StatUpdateData* getStatUpdateDataMap()
    {
        return s_statUpdateDataMap;
    }

    // 0x00CE6190 (:102)
    const uint32 getStatCount()
    {
        return StatId_Count;
    }
}
