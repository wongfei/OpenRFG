// Original: volition/rfg/code/multi/multi_game/multi_game_friend.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_friend.cpp
//
// Friends: the search for friends' games and the friend list.
#pragma once

#include "volition/rfg/code/multi/multi_core/include/multi_player_uid.h"

struct multi_game_friend;
class rfg_split_mempool;

// size 0x34
struct friend_system_data
{
    void initialize(); // 0x00955BD0 (:183)
    void reset(); // 0x00955BE0 (:192)
    void reset_query(); // 0x00955C80 (:257)
    void process(); // 0x0097B8A0 (:216)
    bool start_search(); // 0x00955CD0 (:573)
    void process_search(); // 0x00955CE0 (:574)
    void finish_search(); // 0x00955C10 (:223)

    bool query_in_progress; // 0x00
    void (*get_list_cb)(multi_game_friend*, int, void*); // 0x04
    void (*get_uid_list_cb)(multi_player_uid*, int, void*); // 0x08
    void (*check_friend_cb)(multi_player_uid*, bool*, void*); // 0x0C
    multi_game_friend* friend_list_out; // 0x10
    multi_player_uid* uid_list_out; // 0x14
    bool* friend_check_out; // 0x18
    unsigned int result_filter; // 0x1C
    int result_count; // 0x20
    int result_out_max_size; // 0x24
    int current_result; // 0x28
    int start_result; // 0x2C
    void* search_user_data; // 0x30
};

// size 0x90
struct multi_game_friend
{
    wchar_t player_name[64]; // 0x00
    multi_player_uid player_uid; // 0x80
    unsigned int status_flags; // 0x8C
};

int multi_game_friend_init(rfg_split_mempool* pool, int max_queries_allowed); // 0x00955CF0 (:588)
void multi_game_friend_shutdown(); // 0x0097B8B0 (:617)
void multi_game_friend_process(); // 0x0097B8C0 (:627)
void multi_game_friend_cancel_all(); // 0x00955D20 (:636)
bool multi_game_friend_get_list(multi_game_friend* friends, unsigned int filter, int starting_index, int max_friends, void (*cb)(multi_game_friend*, int, void*), void* user_data); // 0x00955D80 (:645)
bool multi_game_friend_get_list(multi_player_uid* uids, unsigned int filter, int starting_index, int max_friends, void (*cb)(multi_player_uid*, int, void*), void* user_data); // 0x00955DE0 (:678)
bool multi_game_friend_check(multi_player_uid* uids, bool* friend_results_out, int num_friends, void (*cb)(multi_player_uid*, bool*, void*), void* user_data); // 0x00955E40 (:714)
