// Original: volition/rfg/code/multi/multi_game/multi_game_friend.cpp
//
// Friends: the queries of the friend list, the friends' uids and whether players are friends. The PC build has no
// platform search: start_search answers false, so a query sets its parameters up and never runs (no callback is
// called). multi_game.cpp's multi_game_init allocates the system data itself (multi_game_friend_init's body).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_friend.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"

#include <string.h>

// ---- file-static globals
// friend_system_data* Friend_system_data (0x02FEB6A4) is used here too; placed in multi_game.cpp.

// 0x00955BD0 (:183)
void friend_system_data::initialize()
{
    memset(this, 0, sizeof(friend_system_data));
}

// 0x00955BE0 (:192)
void friend_system_data::reset()
{
    query_in_progress = false;
    get_list_cb = nullptr;
    get_uid_list_cb = nullptr;
    check_friend_cb = nullptr;
    friend_list_out = nullptr;
    uid_list_out = nullptr;
    friend_check_out = nullptr;
    result_filter = 0;
    result_count = 0;
    result_out_max_size = 0;
    current_result = 0;
    start_result = 0;
    search_user_data = nullptr;
}

// 0x0097B8A0 (:216)
void friend_system_data::process()
{
}

// 0x00955C10 (:223): the query's callback gets the results found.
void friend_system_data::finish_search()
{
    if (!query_in_progress)
    {
        return;
    }
    void* user_data = search_user_data;
    query_in_progress = false;
    if (get_list_cb != nullptr)
    {
        get_list_cb(friend_list_out, result_count, user_data);
        return;
    }
    if (get_uid_list_cb != nullptr)
    {
        get_uid_list_cb(uid_list_out, result_count, user_data);
        return;
    }
    if (check_friend_cb != nullptr)
    {
        check_friend_cb(uid_list_out, friend_check_out, user_data);
    }
}

// 0x00955C80 (:257): a running query ends with no results.
void friend_system_data::reset_query()
{
    if (!query_in_progress)
    {
        return;
    }
    void* user_data = search_user_data;
    query_in_progress = false;
    if (get_list_cb != nullptr)
    {
        get_list_cb(friend_list_out, 0, user_data);
        return;
    }
    if (get_uid_list_cb != nullptr)
    {
        get_uid_list_cb(uid_list_out, 0, user_data);
        return;
    }
    if (check_friend_cb != nullptr)
    {
        check_friend_cb(uid_list_out, friend_check_out, user_data);
    }
}

// 0x00955CD0 (:573): no platform search on PC.
bool friend_system_data::start_search()
{
    return false;
}

// 0x00955CE0 (:574)
void friend_system_data::process_search()
{
}

// 0x00955CF0 (:588): no caller in rfg.exe (multi_game_init inlines it). Returns the bytes allocated.
int multi_game_friend_init(rfg_split_mempool* pool, int max_queries_allowed)
{
    Friend_system_data = (friend_system_data*)pool->alloc_cpu(sizeof(friend_system_data), 0);
    Friend_system_data->initialize(); // (inlined)
    return sizeof(friend_system_data);
}

// 0x0097B8B0 (:617)
void multi_game_friend_shutdown()
{
    multi_game_friend_cancel_all();
    Friend_system_data = nullptr;
}

// 0x0097B8C0 (:627): friend_system_data::process is empty (inlined to nothing).
void multi_game_friend_process()
{
}

// 0x00955D20 (:636)
void multi_game_friend_cancel_all()
{
    if (Friend_system_data != nullptr)
    {
        Friend_system_data->reset_query(); // (inlined)
    }
}

// 0x00955D80 (:645): the friends from starting_index on, at most max_friends, filtered.
bool multi_game_friend_get_list(multi_game_friend* friends, unsigned int filter, int starting_index, int max_friends, void (*cb)(multi_game_friend*, int, void*), void* user_data)
{
    if (cb == nullptr || Friend_system_data == nullptr || Friend_system_data->query_in_progress || !multi_core_platform_is_connected_to_internet())
    {
        return false;
    }
    Friend_system_data->reset();
    Friend_system_data->friend_list_out = friends;
    Friend_system_data->result_filter = filter;
    Friend_system_data->result_out_max_size = max_friends;
    Friend_system_data->start_result = starting_index;
    Friend_system_data->current_result = 0;
    Friend_system_data->get_list_cb = cb;
    Friend_system_data->search_user_data = user_data;
    return Friend_system_data->start_search(); // (inlined)
}

// 0x00955DE0 (:678): the same for the friends' uids.
bool multi_game_friend_get_list(multi_player_uid* uids, unsigned int filter, int starting_index, int max_friends, void (*cb)(multi_player_uid*, int, void*), void* user_data)
{
    if (cb == nullptr || Friend_system_data == nullptr || Friend_system_data->query_in_progress || !multi_core_platform_is_connected_to_internet())
    {
        return false;
    }
    Friend_system_data->reset();
    Friend_system_data->uid_list_out = uids;
    Friend_system_data->result_filter = filter;
    Friend_system_data->result_out_max_size = max_friends;
    Friend_system_data->start_result = starting_index;
    Friend_system_data->current_result = 0;
    Friend_system_data->get_uid_list_cb = cb;
    Friend_system_data->search_user_data = user_data;
    return Friend_system_data->start_search(); // (inlined)
}

// 0x00955E40 (:714): whether each of the players is a friend.
bool multi_game_friend_check(multi_player_uid* uids, bool* friend_results_out, int num_friends, void (*cb)(multi_player_uid*, bool*, void*), void* user_data)
{
    if (cb == nullptr || Friend_system_data == nullptr || Friend_system_data->query_in_progress || !multi_core_platform_is_connected_to_internet())
    {
        return false;
    }
    Friend_system_data->reset();
    Friend_system_data->uid_list_out = uids;
    Friend_system_data->friend_check_out = friend_results_out;
    Friend_system_data->result_filter = 0;
    Friend_system_data->result_out_max_size = num_friends;
    Friend_system_data->start_result = 0;
    Friend_system_data->current_result = 0;
    Friend_system_data->check_friend_cb = cb;
    Friend_system_data->search_user_data = user_data;
    return Friend_system_data->start_search(); // (inlined)
}
