// Original: library/modules/runtime/systemservices/sources/global/systemservices_internal.hpp
//
// SystemServicesBase, the platform independent part of the system services.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/queue.hpp"
#include "unit4/systemservices/systemservices.hpp"

namespace unit4
{
    // size 0xD4
    struct SystemServicesBase
    {
        SystemServicesBase(); // 0x00C14990 (.h:51)
        ~SystemServicesBase(); // 0x00C14A10

        uint32 currentTimeInMs; // 0x00
        bool rankingSendInProgress; // 0x04
        RankingSendData currentRankingSendData; // 0x08
        keen::Queue<RankingSendData> rankingSendDataQueue; // 0x5C
        RankingReceiveData rankingReceiveData; // 0x70
        RankingError currentRankingError; // 0x98
        SystemServicesInteractionData rankingSendInteraction; // 0x9C
        SystemServicesInteractionData onlineInteraction; // 0xB8
    };
}
