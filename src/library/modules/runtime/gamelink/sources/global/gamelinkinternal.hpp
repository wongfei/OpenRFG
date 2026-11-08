// Original: library/modules/runtime/gamelink/sources/global/gamelinkinternal.hpp
//
// The pending requests of the GameLink implementations (create/join lobby, set filters, type and locked state, find and
// join). Placement inferred: these types have no methods.
#pragma once

#include "keen/base/types.hpp"
#include "kaiko/gamelink/gamelink.hpp"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteammatchmaking.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamclientpublic.h"

namespace kaiko
{
    // size 0x10
    struct GameLinkRequest
    {
        uint32 id; // 0x00
        uint32 idProcessing; // 0x04
        uint8 retryCount; // 0x08
        uint32 timeRequested; // 0x0C
    };

    // size 0x18
    struct CreateLobbyRequest : public GameLinkRequest
    {
        GameLinkLobbyType lobbyType; // 0x10
        uint32 maxNumLobbyMembers; // 0x14
    };

    // size 0x18
    struct JoinLobbyRequest : public GameLinkRequest
    {
        JoinLobbyRequest(); // 0x00CD0490

        CSteamID lobbyId; // 0x10
    };

    // size 0x38
    struct SetLobbyFiltersRequest : public GameLinkRequest
    {
        uint32 lobbyFilters[8]; // 0x10
        bool lobbyFiltersToSet[8]; // 0x30
    };

    // size 0x14
    struct SetLobbyTypeRequest : public GameLinkRequest
    {
        ELobbyType lobbyType; // 0x10
    };

    // size 0x14
    struct SetLobbyLockedRequest : public GameLinkRequest
    {
        bool locked; // 0x10
    };

    // size 0x10
    struct GameLinkFindAndJoinData
    {
        GameLinkFindAndJoinStatus status; // 0x00
        uint32 maxNumLobbyMembers; // 0x04
        uint32 timeStarted; // 0x08
        uint32 timeout; // 0x0C
    };
}
