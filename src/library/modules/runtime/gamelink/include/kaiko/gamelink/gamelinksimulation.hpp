// Original: library/modules/runtime/gamelink/include/kaiko/gamelink/gamelinksimulation.hpp
// Implementation: library/modules/runtime/gamelink/sources/steam/gamelinksimulation_steam.cpp
//
// GameLink simulation: fake lobbies for testing the lobby browser without other players. Placement of
// GameLinkSimulation inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/useraccount/useraccounttypes.hpp"

namespace kaiko
{
    namespace gamelink
    {
        // size 0x4
        struct GameLinkSimulation
        {
            keen::MemoryAllocator* pAllocator; // 0x00
        };

    }

    namespace gamelink
    {
        bool beginSimulation(GameLinkSimulation* pSimulation, keen::MemoryAllocator* pAllocator, keen::UserAccountId mainUser); // 0x00CD32E0 (gamelinksimulation_steam.cpp:12)
        void createSimulatedLobbies(GameLinkSimulation* pSimulation, uint32 numLobbies, uint32 minMemberLimit, uint32 maxMemberLimit); // 0x00CD3300 (gamelinksimulation_steam.cpp:20)
        void updateSimulation(GameLinkSimulation* pSimulation); // 0x00CD07E0 (gamelinksimulation_steam.cpp:31)
    }
}
