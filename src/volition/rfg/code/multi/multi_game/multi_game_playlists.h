// Original: volition/rfg/code/multi/multi_game/multi_game_playlists.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_playlists.cpp
//
// Playlists: the matchmaking playlists (modes and maps) and their entries.
#pragma once

#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_game/multi_game_matchmaking_internal.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/vlib/code/math/crc.h"

class mempool_base;
struct multi_game_options_set;
struct multi_level_info;

// size 0x2C
struct multi_game_playlist_entry
{
    checksum_stri entry_id; // 0x00
    int ideal_players; // 0x04
    multi_gameplay_mode mode; // 0x08
    const multi_level_info* level; // 0x0C
    const multi_game_options_set* settings; // 0x10
    multi_game_playlist_entry* next; // 0x14
    multi_game_playlist_entry* prev; // 0x18
    multi_game_playlist_entry* pool_next; // 0x1C
    multi_game_playlist_entry* pool_prev; // 0x20
    int default_score; // 0x24
    int score; // 0x28
};

// size 0x40
struct multi_game_playlist
{
    int get_max_party_size() const; // 0x009838C0 (:422)

    checksum_stri playlist_id; // 0x00
    platform_game_mode_context mode_type; // 0x04
    platform_game_type_context game_type; // 0x08
    query_type m_query_type; // 0x0C
    int required_xp_level; // 0x10
    unsigned int required_dlc_flags; // 0x14
    bool requires_bagman; // 0x18
    int min_players; // 0x1C
    int max_players; // 0x20
    int max_party_size; // 0x24
    char* display_name; // 0x28
    char* description; // 0x2C
    multi_game_playlist_entry* entries; // 0x30
    multi_game_playlist* next; // 0x34
    multi_game_playlist* prev; // 0x38
    float xp_scalar; // 0x3C
};

void multi_game_playlist_system_init(mempool_base& pool); // 0x009B2160 (:444)
void multi_game_playlist_system_shutdown(); // 0x00960020 (:467)
multi_game_playlist* multi_game_playlist_get_by_id(unsigned int playlist_id); // 0x00960030 (:473)
multi_game_playlist* multi_game_playlist_get_by_mode(unsigned int playlist_mode); // 0x00960060 (:485)
const multi_game_playlist* multi_game_playlist_get_first(); // 0x00960090 (:499)
const multi_game_playlist* multi_game_playlist_get_next(const multi_game_playlist* plp); // 0x009600A0 (:504)
const multi_game_playlist_entry* multi_game_playlist_get_random_entry(const multi_game_playlist* src_playlist); // 0x009600C0 (:510)
multi_game_playlist_entry* multi_game_playlist_get_entry_by_id(const multi_game_playlist* src_playlist, unsigned int entry_id); // 0x00960170 (:554)
