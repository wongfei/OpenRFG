// Original: volition/rfg/code/multi/multi_core/global/multi_core_matchmaking.cpp
//
// Matchmaking: the platform independent front of the lobby search (multi_core_matchmaking_generic.cpp). A search
// keeps its query and the caller's result list here; the results arrive in system_process.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_core/include/multi_core_matchmaking.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_matchmaking_generic.h"

// ---- file-static globals
// The search's parameters and results. External linkage: multi_core_matchmaking_generic.cpp, earlier in the unity TU,
// reads them.
const matchmaking_query* Search_parameters; // 0x02FE5AE4
int Max_results; // 0x02FE5B10
int* Results_returned; // 0x02FE6C7C
matchmaking_result** Result_list; // 0x02FE87D0

// 0x00947BA0 (:65)
void multi_core_matchmake_system_init()
{
}

// 0x00947BB0 (:70)
void multi_core_matchmake_system_shutdown()
{
}

// 0x00972820 (:75)
void multi_core_matchmake_system_process()
{
    multi_core_matchmake_internal_system_process(); // (inlined)
}

// 0x00947BC0 (:80): the search is kept; the synchronous search does nothing on PC (inlined to nothing).
void multi_core_matchmake_find_match_synchronous(int& results_returned, matchmaking_result** result_list, int result_list_size, const matchmaking_query* search_parameters)
{
    Results_returned = &results_returned;
    Result_list = result_list;
    Max_results = result_list_size;
    Search_parameters = search_parameters;
    multi_core_matchmake_internal_find_match_synchronous(); // (inlined)
}

// 0x0098FF70 (:90)
void multi_core_matchmake_find_match_async(int& results_returned, matchmaking_result** result_list, int max_results, const matchmaking_query* search_parameters)
{
    Results_returned = &results_returned;
    Result_list = result_list;
    Max_results = max_results;
    Search_parameters = search_parameters;
    multi_core_matchmake_internal_find_match_async();
}

// 0x00972830 (:100)
bool multi_core_matchmake_async_find_complete()
{
    return multi_core_matchmake_internal_async_find_complete(); // (inlined)
}
