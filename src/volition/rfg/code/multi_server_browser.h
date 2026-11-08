// Original: volition/rfg/code/multi_server_browser.h
// Implementation: volition/rfg/code/multi_server_browser.cpp
//
// The Steam server browser (PC): searches Steam lobbies by server type and game mode, lists them as ServerInfo, joins one
// and publishes the host's lobby data.
#pragma once

#include "keen/base/fixedsizedarray.hpp"
#include "kaiko/gamelink/gamelink.hpp"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteammatchmaking.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api_internal.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"

enum ServerTypeFilter
{
    ServerTypeFilter_None = 0,
    ServerTypeFilter_OnlyCustomGames = 1,
    ServerTypeFilter_OnlyPublicGamesInProgress = 2,
};

// size 0x2A0
struct ServerInfo
{
    ServerInfo(); // 0x005D0DF0 (.h:33)
    bool isValid() const; // 0x00883590 (.h:36)

    char serverName[256]; // 0x00
    multi_gameplay_mode gameMode; // 0x100
    char levelName[128]; // 0x104
    unsigned int numPlayers; // 0x184
    unsigned int maxPlayers; // 0x188
    char hostPlayerName[256]; // 0x18C
    kaiko::GameLinkLobby lobby; // 0x290
    bool partyLobby; // 0x298
};

// size 0x2A038
class ServerBrowser
{
public:
    ServerBrowser(); // 0x005D6E00 (:21)
    ~ServerBrowser(); // 0x005D6E50 (:26)
    bool startSearch(unsigned int maxResults, ServerTypeFilter serverTypeFilter, multi_gameplay_mode gameMode, const char* pLevelName); // 0x005D6E90 (:31)
    unsigned int getSearchResults(ServerInfo* pResults, unsigned int capacity, bool* pSearchRunning) const; // 0x005D3AC0 (:102)
    void updateLobbyDataAsHost(); // 0x005D3B10 (:118)
    void joinLobby(const ServerInfo& serverInfo); // 0x005D0E80 (:219)
    bool isInCustomMatch() const; // 0x005D0F60 (:270)
    void updateInCustomMatchStatus(); // 0x005D0F80 (:275)
private:
    void handleLobbyListRequest(LobbyMatchList_t* pLobbyMatchList, bool bIOFailure); // 0x005D3D60 (:293)

    keen::FixedSizedArray<ServerInfo,256> m_searchResults; // 0x00
    CCallResult<ServerBrowser,LobbyMatchList_t> m_callResultLobbyListRequested; // 0x2A008
    bool m_searchRunning; // 0x2A028
    ServerTypeFilter m_currentServerTypeFilter; // 0x2A02C
    kaiko::GameLinkLobby m_customMatchLobby; // 0x2A030
};

extern ServerBrowser Live_server_browser; // 0x02C541E0, the Steam server browser (multi_server_browser.cpp)
