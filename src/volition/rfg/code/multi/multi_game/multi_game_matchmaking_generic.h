// Original: volition/rfg/code/multi/multi_game/multi_game_matchmaking_generic.h
//
// Generic (GameLink) matchmaking queries and matches; match_results_list<N> holds up to N results.
#pragma once

#include "volition/rfg/code/multi/multi_game/multi_game_matchmaking_internal.h"

class matchmaking_result;

// size 0x40
class game_match : public game_match_base
{
public:
    game_match(); // 0x008884C0 (.h:31)
    virtual ~game_match(); // vtbl+0x00 0x008884E0 (.h:32)

};

// size 0x1C
class game_query : public game_query_base
{
public:
    game_query(); // 0x005D0E60 (.h:40)
    virtual ~game_query(); // vtbl+0x00 0x005D0E70 (.h:41)

};

// size (<25>) 0x6A8
template<int N>
class match_results_list
{
public:
    match_results_list(); // <25> 0x008A5BE0 (.h:59)
    ~match_results_list(); // <25> 0x0088C3C0

    static const int max_results; // <25> 0x01301630
    game_match results[N]; // 0x00
    matchmaking_result* list[N]; // 0x640
    int num_results; // 0x6A4
};

// 0x008884C0 (.h:31)
inline game_match::game_match()
{
}

// 0x008884E0 (.h:32)
inline game_match::~game_match()
{
}

// 0x005D0E60 (.h:40)
inline game_query::game_query()
{
}

// 0x005D0E70 (.h:41)
inline game_query::~game_query()
{
}

// <25> 0x008A5BE0 (.h:59): every result initialized and listed; no results yet.
template<int N>
match_results_list<N>::match_results_list()
{
    for (int i = 0; i < N; i++)
    {
        results[i].init();
        list[i] = &results[i];
    }
    num_results = 0;
}

// <25> 0x0088C3C0
template<int N>
match_results_list<N>::~match_results_list()
{
}
