// Original: volition/rfg/code/multi/multi_game/multi_game_matchmaking_internal.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_matchmaking_internal.cpp
//
// The matchmaking query and match base classes, and the query types.
#pragma once

#include "volition/rfg/code/multi/multi_core/generic/multi_core_matchmaking_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"

enum query_type
{
    QT_QUICKMATCH = 0,
    QT_PLAYLIST = 1,
    QT_ALLGAMES = 2,
    NUM_QUERY_TYPES = 3,
};

// size 0x1C
class game_query_base : public matchmaking_query
{
public:
    game_query_base(); // 0x005D0E40 (.h:55)
    virtual ~game_query_base(); // vtbl+0x00 0x005D0E50 (.h:56)
    virtual void init(); // vtbl+0x04 0x0097C0B0 (:69)
    virtual void copy(const game_query_base& src); // vtbl+0x08 0x00956CB0 (:98)
    void write_to_packet(unsigned char* packet_data, unsigned short& packet_size) const; // 0x00956D30 (:125)
    void read_from_packet(unsigned char* packet_data, int& packet_offset); // 0x00956E30 (:136)
    const char* query_type_name() const; // 0x0097C100 (:110)
    const char* game_mode_name() const; // 0x00956CF0 (:115)
    const char* game_type_name() const; // 0x00956D10 (:120)

    query_type m_query_type; // 0x04
    platform_game_mode_context m_game_mode; // 0x08
    platform_game_type_context m_game_type; // 0x0C
    bool m_allow_jip; // 0x10
    bool m_world_wide_search; // 0x11
    unsigned int m_playlist_id; // 0x14
    unsigned int m_playlist_version; // 0x18
};

// size 0x40
class game_match_base : public matchmaking_result
{
public:
    game_match_base(); // 0x00888470 (.h:39)
    virtual ~game_match_base(); // vtbl+0x00 0x00888490 (.h:40)
    virtual void init(); // vtbl+0x04 0x009AFDE0 (:147)

    platform_game_mode_context m_game_mode; // 0x30
    platform_game_type_context m_game_type; // 0x34
    unsigned int m_dlc_mask; // 0x38
};

// 0x005D0E40 (.h:55): the members are left as they are (init sets them).
inline game_query_base::game_query_base()
{
}

// 0x005D0E50 (.h:56)
inline game_query_base::~game_query_base()
{
}

// 0x00888470 (.h:39)
inline game_match_base::game_match_base()
{
}

// 0x00888490 (.h:40)
inline game_match_base::~game_match_base()
{
}

const char* multi_game_matchmaking_internal_get_query_type_name(query_type query); // 0x00956F30 (:153)
