// Original: volition/rfg/code/multi_server_browser.cpp
//
// The Steam server browser (PC): searches Steam lobbies by server type, game mode and level, lists them as ServerInfo,
// joins one and publishes the host's lobby data. Talks to Steam directly, not through GameLink, under g_gameLinkMutex.

#include "volition/rfg/code/multi_server_browser.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_matchmaking_generic.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_matchmaking_generic.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_live.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_live_find_servers.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_lobby.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "keen/base/mutex.hpp"
#include "keen/base/string.hpp"
#include "keen/base/stringencoding.hpp"
#include "keen/base/stringscanner.hpp"
#include "keen/base/systemtimer.hpp"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api.h"

// ---- globals
ServerBrowser Live_server_browser; // 0x02C541E0

// ---- file-static globals
// The Steam lobby data keys (rfg.exe has one copy per file including its header, 0x0163EF74..0x0163F0D8 here).
static const char* s_keyGameMode = "gamemode";
static const char* s_keyLevelName = "levelname";
static const char* s_keyServerName = "servername";
static const char* s_keyHostPlayerName = "hostplayername";
static const char* s_keyInProgress = "inprogress";
static const char* s_keyCustomMatch = "custommatch";
static unsigned int s_lastTimeLobbyDataUpdated; // 0x01D651F4
// Magic number: the third lobby filter word of a custom game (multi_core_matchmaking_generic.cpp's marker)
static const unsigned int Custom_game_filter_marker = 0x1234FFFF;

// 0x005D0DF0 (.h:33)
ServerInfo::ServerInfo()
{
}

// 0x00883590 (.h:36)
bool ServerInfo::isValid() const
{
    return serverName[0] != '\0' && gameMode != MGPM_NONE && levelName[0] != '\0' && maxPlayers != 0 && hostPlayerName[0] != '\0' && lobby.id.asUint64 != 0;
}

// 0x005D6E00 (:21). m_searchRunning is left to the static zero initialization (rfg.exe).
ServerBrowser::ServerBrowser()
{
}

// 0x005D6E50 (:26): the call result member cancels itself
ServerBrowser::~ServerBrowser()
{
}

// 0x005D6E90 (:31): at most 256 results, the filters of a public search, Steam's far distance filter.
// Original bug kept: the query given to the filter words is uninitialized in rfg.exe (its constructor only sets the
// vtable); only the first two words are sent.
bool ServerBrowser::startSearch(unsigned int maxResults, ServerTypeFilter serverTypeFilter, multi_gameplay_mode gameMode, const char* pLevelName)
{
    if (m_searchRunning)
    {
        return false;
    }
    if (maxResults > 256)
    {
        maxResults = 256;
    }
    g_gameLinkMutex.lock(0);
    ISteamMatchmaking* matchmaking = SteamMatchmaking();
    if (matchmaking == nullptr)
    {
        g_gameLinkMutex.unlock();
        return false;
    }
    m_searchResults.clear();
    m_currentServerTypeFilter = serverTypeFilter;
    matchmaking->AddRequestLobbyListResultCountFilter((int)maxResults);
    if (serverTypeFilter == ServerTypeFilter_OnlyCustomGames)
    {
        matchmaking->AddRequestLobbyListStringFilter(s_keyCustomMatch, "1", k_ELobbyComparisonEqual);
    }
    else if (serverTypeFilter == ServerTypeFilter_OnlyPublicGamesInProgress)
    {
        matchmaking->AddRequestLobbyListStringFilter(s_keyInProgress, "1", k_ELobbyComparisonEqual);
    }
    if (gameMode != MGPM_NONE)
    {
        matchmaking->AddRequestLobbyListNumericalFilter(s_keyGameMode, gameMode, k_ELobbyComparisonEqual);
    }
    if (pLevelName != nullptr)
    {
        matchmaking->AddRequestLobbyListStringFilter(s_keyLevelName, pLevelName, k_ELobbyComparisonEqual);
    }
    game_query query;
    unsigned int filters[8];
    multi_core_matchmake_internal_init_filters(filters, &query, false, 0, MatchInProgressFilter_None);
    for (unsigned int i = 0; i < 2; i++)
    {
        char key[32];
        char value[32];
        keen::formatString(key, sizeof(key), "f%u", i);
        keen::formatString(value, sizeof(value), "%u", filters[i]);
        SteamMatchmaking()->AddRequestLobbyListStringFilter(key, value, k_ELobbyComparisonEqual);
    }
    SteamMatchmaking()->AddRequestLobbyListDistanceFilter(k_ELobbyDistanceFilterFar);
    const SteamAPICall_t call = matchmaking->RequestLobbyList();
    m_callResultLobbyListRequested.Set(call, this, &ServerBrowser::handleLobbyListRequest);
    m_searchRunning = true;
    g_gameLinkMutex.unlock();
    return true;
}

// 0x005D3AC0 (:102). Original bug kept: capacity is not checked.
unsigned int ServerBrowser::getSearchResults(ServerInfo* pResults, unsigned int capacity, bool* pSearchRunning) const
{
    *pSearchRunning = m_searchRunning;
    if (m_searchRunning)
    {
        return 0;
    }
    const unsigned int count = m_searchResults.getSize();
    for (unsigned int i = 0; i < count; i++)
    {
        pResults[i] = m_searchResults[i];
    }
    return count;
}

// 0x005D3B10 (:118): at most every 100 ms, the lobby owner's data for the browsers of the others. The published game
// mode is one above the enum (handleLobbyListRequest takes it back), but startSearch's filter does not add the one
// (rfg.exe): a game mode search finds the mode before the chosen one.
void ServerBrowser::updateLobbyDataAsHost()
{
    const unsigned int now = keen::SystemTimer::getCurrentMilliseconds();
    if (now - s_lastTimeLobbyDataUpdated <= 100)
    {
        return;
    }
    s_lastTimeLobbyDataUpdated = now;
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = get_game_link_session();
    unsigned int filters[8];
    if (link != nullptr && link->isLobbyOwner() && link->getLobbyFilters(filters, 8) != 0)
    {
        for (unsigned int i = 0; i < 8; i++)
        {
            if (filters[i] == 0)
            {
                continue;
            }
            ISteamMatchmaking* matchmaking = SteamMatchmaking();
            const kaiko::GameLinkLobby lobby = matchmaking != nullptr ? link->getLobbyId() : kaiko::GameLinkLobby();
            if (matchmaking != nullptr && lobby.id.asUint64 != 0)
            {
                const CSteamID lobby_id(lobby.id.asUint64);
                if (filters[2] == Custom_game_filter_marker && Party_lobby_type == MPPLT_CUSTOM_GAME)
                {
                    matchmaking->SetLobbyData(lobby_id, s_keyCustomMatch, "1");
                }
                char text[128];
                const multi_gameplay_mode mode = Mp_server_options.get_mode();
                if (mode != MGPM_NONE)
                {
                    keen::formatString(text, 64, "%d", (int)mode + 1);
                    matchmaking->SetLobbyData(lobby_id, s_keyGameMode, text);
                }
                const multi_level_info* level = Mp_server_options.get_level();
                if (level != nullptr)
                {
                    matchmaking->SetLobbyData(lobby_id, s_keyLevelName, level->display_name);
                }
                if (Lobby_session != nullptr)
                {
                    matchmaking->SetLobbyData(lobby_id, s_keyInProgress, Lobby_session->get_state() == MSS_IN_GAME ? "1" : "0");
                }
                const multi_game_data* data = multi_game_data_get_my_data(ST_GAME, false, false);
                if (data != nullptr)
                {
                    wchar_t name[64];
                    _snprintf_safe(name, 64, "%s", data->display_name);
                    rfg_reverse_escape_characters(name);
                    const unsigned int length = keen::convertUTF16ToUTF8(text, sizeof(text), (const uint16*)name);
                    matchmaking->SetLobbyData(lobby_id, s_keyHostPlayerName, text);
                    keen::copyUTF8String(text + length, sizeof(text) - length, "'s game");
                    matchmaking->SetLobbyData(lobby_id, s_keyServerName, text);
                }
            }
            break;
        }
    }
    g_gameLinkMutex.unlock();
}

// 0x005D0E80 (:219): the join, then the lobby menu (a custom match remembers its lobby)
void ServerBrowser::joinLobby(const ServerInfo& serverInfo)
{
    multi_session_info info;
    info.m_lobbyId = serverInfo.lobby;
    info.max_slots = (int)serverInfo.maxPlayers;
    info.party_lobby = serverInfo.partyLobby;
    info.invite_type = SIT_ALL;
    info.private_match = false;
    info.type = ST_GAME;
    multi_game_lobby_join(&info, nullptr, false, true);
    const game_state state = gameseq_get_state();
    if (serverInfo.partyLobby)
    {
        m_customMatchLobby = serverInfo.lobby;
    }
    const game_state lobby_state = serverInfo.partyLobby ? GS_MULTIPLAYER_PARTY_LOBBY : GS_MULTIPLAYER_GAME_LOBBY;
    if (state == GS_MULTIPLAYER_LIVE)
    {
        ui_document_mp_live_ui_push(lobby_state);
    }
    else if (state == GS_MULTIPLAYER_LIVE_FIND_SERVERS)
    {
        lfs_ui_push(lobby_state);
    }
}

// 0x005D0F60 (:270)
bool ServerBrowser::isInCustomMatch() const
{
    return m_customMatchLobby.id.asUint64 != 0;
}

// 0x005D0F80 (:275): forgets the custom match after leaving its lobby (no lobby id yet in the lobby state keeps it)
void ServerBrowser::updateInCustomMatchStatus()
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = get_game_link_session();
    if (link == nullptr)
    {
        g_gameLinkMutex.unlock();
        return;
    }
    if (link->getCurrentState() == kaiko::GameLinkState_InLobby)
    {
        const kaiko::GameLinkLobby lobby = link->getLobbyId();
        if (lobby.id.asUint64 == 0 || lobby.is_equal(m_customMatchLobby))
        {
            g_gameLinkMutex.unlock();
            return;
        }
    }
    if (m_customMatchLobby.id.asUint64 != 0)
    {
        m_customMatchLobby.id.asUint64 = 0;
    }
    g_gameLinkMutex.unlock();
}

// 0x005D3D60 (:293): the lobbies with a free slot that pass the server type filter. An I/O failure ends the search
// with what the list has (rfg.exe does not look at it).
void ServerBrowser::handleLobbyListRequest(LobbyMatchList_t* pLobbyMatchList, bool bIOFailure)
{
    g_gameLinkMutex.lock(0);
    ISteamMatchmaking* matchmaking = SteamMatchmaking();
    if (matchmaking != nullptr)
    {
        for (unsigned int i = 0; i < pLobbyMatchList->m_nLobbiesMatching && !m_searchResults.isFull(); i++)
        {
            const CSteamID lobby_id = SteamMatchmaking()->GetLobbyByIndex((int)i);
            const unsigned int num_players = (unsigned int)SteamMatchmaking()->GetNumLobbyMembers(lobby_id);
            const unsigned int max_players = (unsigned int)SteamMatchmaking()->GetLobbyMemberLimit(lobby_id);
            if (max_players == num_players)
            {
                continue;
            }
            const bool custom_match = keen::compareString(matchmaking->GetLobbyData(lobby_id, s_keyCustomMatch), "1") == 0;
            const char* in_progress_text = matchmaking->GetLobbyData(lobby_id, s_keyInProgress);
            uint32 in_progress = 0;
            keen::readUint32FromDecimalString(&in_progress, &in_progress_text);
            if (m_currentServerTypeFilter == ServerTypeFilter_OnlyCustomGames && !custom_match)
            {
                continue;
            }
            if (m_currentServerTypeFilter == ServerTypeFilter_OnlyPublicGamesInProgress && (custom_match || in_progress == 0))
            {
                continue;
            }
            ServerInfo& result = m_searchResults.pushBack();
            result.lobby.id.asUint64 = lobby_id.ConvertToUint64();
            result.partyLobby = custom_match;
            result.numPlayers = num_players;
            result.maxPlayers = max_players;
            const char* mode_text = matchmaking->GetLobbyData(lobby_id, s_keyGameMode);
            int mode = -1;
            if (keen::readSint32FromDecimalString(&mode, &mode_text) == keen::StringReadResult_Ok)
            {
                result.gameMode = (multi_gameplay_mode)(mode - 1);
            }
            else
            {
                result.gameMode = MGPM_NONE;
            }
            keen::copyString(result.levelName, sizeof(result.levelName), matchmaking->GetLobbyData(lobby_id, s_keyLevelName));
            keen::copyUTF8String(result.serverName, sizeof(result.serverName), matchmaking->GetLobbyData(lobby_id, s_keyServerName));
            keen::copyUTF8String(result.hostPlayerName, sizeof(result.hostPlayerName), matchmaking->GetLobbyData(lobby_id, s_keyHostPlayerName));
        }
    }
    m_searchRunning = false;
    g_gameLinkMutex.unlock();
}
