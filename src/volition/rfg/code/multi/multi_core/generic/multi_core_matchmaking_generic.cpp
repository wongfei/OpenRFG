// Original: volition/rfg/code/multi/multi_core/generic/multi_core_matchmaking_generic.cpp
//
// Generic (GameLink) matchmaking: the public lobby search. A query becomes the 8 lobby filter words (the build tag,
// the modified table hash, the custom game marker, the game type, game mode and playlist, the party size and the
// match state; see multi_core_session_generic.cpp, which creates lobbies with the same words) and GameLink searches
// the Steam lobbies with them; the Steam distance filter widens from "default" to "far" after 30 s of searching. The
// results become multi_session_infos of game sessions. The synchronous search does nothing on PC.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_core/generic/multi_core_matchmaking_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_game/multi_game_matchmaking_generic.h"
#include "volition/rfg/code/multi/multi_game/multi_game_playlists.h"
#include "library/modules/runtime/base/include/keen/base/mutex.hpp"
#include "library/modules/runtime/base/include/keen/base/systemtimer.hpp"
#include "library/modules/runtime/gamelink/include/kaiko/gamelink/gamelink.hpp"
#include "library/modules/runtime/gamesession/include/unit4/gamesession/remotegamesession.hpp"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api.h"

// ---- file-static globals
// The Steam distance filter widens after this long searching (ms).
static int s_timeUntilDistanceFilterAdjustment = 30000; // 0x0167D7E4
static multi_game_playlist* Playlists; // 0x02FE6C70
static unsigned int s_modTableHash; // 0x02FEB56C
static bool s_searchTimerRunning; // 0x02FEB570
static keen::SystemTimer s_searchTimer; // 0x0300F8D0

// The search's parameters and results (multi_core_matchmaking.cpp, later in the unity TU, sets them).
extern const matchmaking_query* Search_parameters; // 0x02FE5AE4
extern int Max_results; // 0x02FE5B10
extern int* Results_returned; // 0x02FE6C7C
extern matchmaking_result** Result_list; // 0x02FE87D0

// Magic numbers: the lobby filters (8 words): 0 the build tag (banned users get their own, so they only find each
// other), 1 the modified table hash, 2 the custom game marker, 3-5 the game type, game mode and playlist id plus one
// (0 matches any), 6 the party size, 7 the match state (1 in progress, 2 not). multi_core_session_generic.cpp uses
// them when it creates a lobby.
static const unsigned int Lobby_filter_count = 8;
static const unsigned int Lobby_filter_match_state = 7;
static const unsigned int Banned_lobby_tag = 0xDEADD00D;
static const unsigned int Lobby_tag = 0xAAAA0008;
static const unsigned int Custom_game_lobby_marker = 0x1234FFFF;
static const unsigned int Match_state_in_progress = 1;
static const unsigned int Match_state_not_in_progress = 2;
// The two Steam accounts rfg.exe treats as banned.
static const unsigned __int64 Banned_steam_ids[2] = {0x011000010CEFB9A1ull, 0x011000010F5A78D4ull};
// The most results GameLink returns.
static const int Max_search_results = 25;

// ---- external interface

// 0x00941C20 (:81)
void multi_core_matchmake_internal_system_init()
{
}

// 0x00941C30 (:86)
void multi_core_matchmake_internal_system_shutdown()
{
}

// 0x00941C40 (:91)
void multi_core_matchmake_internal_find_match_synchronous(int&, multi_session_info*, int)
{
}

// 0x0098DE30 (:96): a party lobby only carries the tags, the custom game marker and the match state.
void multi_core_matchmake_internal_init_filters(unsigned int* filters, const game_query* pSearchQuery, bool partyLobby, unsigned int maxPartySize, MatchInProgressFilter matchInProgress)
{
    filters[0] = multi_core_matchmake_internal_is_my_user_banned() ? Banned_lobby_tag : Lobby_tag; // (inlined)
    filters[1] = s_modTableHash;
    if (partyLobby)
    {
        filters[2] = Custom_game_lobby_marker;
        filters[3] = 0;
        filters[4] = 0;
        filters[5] = 0;
        filters[6] = 0;
        filters[Lobby_filter_match_state] = matchInProgress;
        return;
    }
    filters[2] = 0;
    filters[3] = pSearchQuery->m_game_type + 1;
    filters[4] = pSearchQuery->m_game_mode + 1;
    filters[5] = pSearchQuery->m_playlist_id + 1;
    filters[6] = maxPartySize;
    filters[Lobby_filter_match_state] = matchInProgress;
}

// 0x0098DEF0 (:159): the search for the query's playlist (none without the playlist); in a party, only matches not
// in progress.
void multi_core_matchmake_internal_find_match_async()
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    const game_query* query = (const game_query*)Search_parameters;
    multi_game_playlist* playlist = Playlists;
    while (playlist != nullptr)
    {
        if (playlist->playlist_id.get_crc() == query->m_playlist_id)
        {
            break;
        }
        playlist = playlist->next;
        if (playlist == Playlists)
        {
            playlist = nullptr;
        }
    }
    if (playlist != nullptr)
    {
        const unsigned int max_party_size = playlist->max_party_size;
        unsigned int filters[Lobby_filter_count];
        multi_core_matchmake_internal_init_filters(filters, query, false, max_party_size,
                                                   link->getNumPartyMembers() > 1 ? MatchInProgressFilter_NotInProgress : MatchInProgressFilter_None);
        if (!s_searchTimerRunning)
        {
            multi_core_matchmake_internal_start_search_timer(); // (inlined)
            SteamMatchmaking()->AddRequestLobbyListDistanceFilter(k_ELobbyDistanceFilterDefault);
        }
        else if (s_searchTimer.hasTimeElapsed(s_timeUntilDistanceFilterAdjustment))
        {
            SteamMatchmaking()->AddRequestLobbyListDistanceFilter(k_ELobbyDistanceFilterFar);
        }
        link->startSearchLobby(Max_results, filters, Lobby_filter_count);
    }
    g_gameLinkMutex.unlock();
}

// 0x00970410 (:267): GameLink's search is no longer running.
bool multi_core_matchmake_internal_async_find_complete()
{
    g_gameLinkMutex.lock(0);
    bool running = false;
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    kaiko::GameLinkSearchResult results[Max_search_results];
    link->getSearchResults(results, Max_search_results, &running);
    const bool complete = !running;
    g_gameLinkMutex.unlock();
    return complete;
}

// 0x00970480 (:290): the found lobbies as public game sessions (at most 25); false without results.
bool multi_core_matchmake_internal_get_async_find_results(int& results_returned, matchmaking_result** result_list, int max_results)
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    results_returned = 0;
    if (max_results > Max_search_results - 1)
    {
        max_results = Max_search_results;
    }
    kaiko::GameLinkSearchResult results[Max_search_results];
    bool running;
    const unsigned int count = link->getSearchResults(results, max_results, &running);
    if (count == 0)
    {
        g_gameLinkMutex.unlock();
        return false;
    }
    for (unsigned int i = 0; i < count; i++)
    {
        result_list[i]->m_lobbyId = results[i].lobby;
        result_list[i]->max_slots = results[i].maxNumPlayers;
        result_list[i]->invite_type = SIT_ALL;
        result_list[i]->party_lobby = false;
        result_list[i]->private_match = false;
        result_list[i]->type = ST_GAME;
    }
    results_returned = (int)count;
    g_gameLinkMutex.unlock();
    return true;
}

// 0x00970570 (:352): the results are taken when the search is over.
void multi_core_matchmake_internal_system_process()
{
    if (multi_core_matchmake_internal_async_find_complete()) // (inlined)
    {
        multi_core_matchmake_internal_get_async_find_results(*Results_returned, Result_list, Max_results);
    }
}

// 0x00941C50 (:364)
void multi_core_matchmake_internal_find_match_synchronous()
{
}

// 0x00941C60 (:369)
void multi_core_matchmake_internal_set_mod_table_hash(unsigned int modTableHash)
{
    s_modTableHash = modTableHash;
}

// 0x00941C70 (:374)
unsigned int multi_core_matchmake_internal_get_mod_table_hash()
{
    return s_modTableHash;
}

// 0x00941C80 (:379): the filter words back (0, any, gives the first value).
void multi_core_matchmake_internal_convert_filters(unsigned int* filters, platform_game_type_context* pGameType, platform_game_mode_context* pGameMode, unsigned int* pPlaylistId)
{
    *pGameType = filters[3] == 0 ? PGTC_RANKED : (platform_game_type_context)(filters[3] - 1);
    *pGameMode = filters[4] == 0 ? PGMC_MAIN_MENU : (platform_game_mode_context)(filters[4] - 1);
    *pPlaylistId = filters[5] == 0 ? 0 : filters[5] - 1;
}

// 0x00941CC0 (:387)
void multi_core_matchmake_internal_start_search_timer()
{
    s_searchTimer.start();
    s_searchTimerRunning = true;
}

// 0x00941CE0 (:396)
void multi_core_matchmake_internal_stop_search_timer()
{
    s_searchTimerRunning = false;
}

// 0x00970600 (:405)
bool multi_core_matchmake_internal_is_my_user_banned()
{
    const unsigned __int64 id = SteamUser()->GetSteamID().ConvertToUint64();
    return id == Banned_steam_ids[0] || id == Banned_steam_ids[1];
}
