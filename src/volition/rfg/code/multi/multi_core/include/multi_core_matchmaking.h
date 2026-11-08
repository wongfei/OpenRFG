// Original: volition/rfg/code/multi/multi_core/include/multi_core_matchmaking.h
// Implementation: volition/rfg/code/multi/multi_core/global/multi_core_matchmaking.cpp
//
// Matchmaking: searches for sessions matching a query and the results.
#pragma once


class matchmaking_query;
class matchmaking_result;

void multi_core_matchmake_system_init(); // 0x00947BA0 (:65)
void multi_core_matchmake_system_shutdown(); // 0x00947BB0 (:70)
void multi_core_matchmake_system_process(); // 0x00972820 (:75)
void multi_core_matchmake_find_match_synchronous(int& results_returned, matchmaking_result** result_list, int result_list_size, const matchmaking_query* search_parameters); // 0x00947BC0 (:80)
void multi_core_matchmake_find_match_async(int& results_returned, matchmaking_result** result_list, int max_results, const matchmaking_query* search_parameters); // 0x0098FF70 (:90)
bool multi_core_matchmake_async_find_complete(); // 0x00972830 (:100)
