// Original: volition/rfg/code/multi/multi_core/generic/multi_core_matchmaking_generic.h
// Implementation: volition/rfg/code/multi/multi_core/generic/multi_core_matchmaking_generic.cpp
//
// Generic (GameLink based) matchmaking: queries and their results. Placement of MatchInProgressFilter inferred.
#pragma once

#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"

class game_query;

enum MatchInProgressFilter
{
    MatchInProgressFilter_None = 0,
    MatchInProgressFilter_InProgress = 1,
    MatchInProgressFilter_NotInProgress = 2,
};

// size 0x30
class matchmaking_result : public multi_session_info
{
public:
    matchmaking_result(const matchmaking_result&);
    matchmaking_result(); // 0x00888420 (.h:30)
    virtual ~matchmaking_result(); // vtbl+0x00 0x00888440 (.h:32)

};

// size 0x4
class matchmaking_query
{
public:
    matchmaking_query(const matchmaking_query&);
    matchmaking_query(); // 0x005D0E20 (.h:40)
    virtual ~matchmaking_query(); // vtbl+0x00 0x005D0E30 (.h:42)

    // 0x00 vfptr
};

// 0x00888420 (.h:30)
inline matchmaking_result::matchmaking_result()
{
}

// 0x00888440 (.h:32)
inline matchmaking_result::~matchmaking_result()
{
}

// 0x005D0E20 (.h:40)
inline matchmaking_query::matchmaking_query()
{
}

// 0x005D0E30 (.h:42)
inline matchmaking_query::~matchmaking_query()
{
}

void multi_core_matchmake_internal_system_init(); // 0x00941C20 (:81)
void multi_core_matchmake_internal_system_shutdown(); // 0x00941C30 (:86)
void multi_core_matchmake_internal_find_match_synchronous(int&, multi_session_info*, int); // 0x00941C40 (:91)
void multi_core_matchmake_internal_init_filters(unsigned int* filters, const game_query* pSearchQuery, bool partyLobby, unsigned int maxPartySize, MatchInProgressFilter matchInProgress); // 0x0098DE30 (:96)
void multi_core_matchmake_internal_find_match_async(); // 0x0098DEF0 (:159)
bool multi_core_matchmake_internal_async_find_complete(); // 0x00970410 (:267)
bool multi_core_matchmake_internal_get_async_find_results(int& results_returned, matchmaking_result** result_list, int max_results); // 0x00970480 (:290)
void multi_core_matchmake_internal_system_process(); // 0x00970570 (:352)
void multi_core_matchmake_internal_find_match_synchronous(); // 0x00941C50 (:364)
void multi_core_matchmake_internal_set_mod_table_hash(unsigned int modTableHash); // 0x00941C60 (:369)
unsigned int multi_core_matchmake_internal_get_mod_table_hash(); // 0x00941C70 (:374)
void multi_core_matchmake_internal_convert_filters(unsigned int* filters, platform_game_type_context* pGameType, platform_game_mode_context* pGameMode, unsigned int* pPlaylistId); // 0x00941C80 (:379)
void multi_core_matchmake_internal_start_search_timer(); // 0x00941CC0 (:387)
void multi_core_matchmake_internal_stop_search_timer(); // 0x00941CE0 (:396)
bool multi_core_matchmake_internal_is_my_user_banned(); // 0x00970600 (:405)
