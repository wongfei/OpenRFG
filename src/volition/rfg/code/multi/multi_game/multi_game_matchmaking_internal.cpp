// Original: volition/rfg/code/multi/multi_game/multi_game_matchmaking_internal.cpp
//
// The matchmaking query (query type, game mode and type, playlist and its version, join in progress, world wide) with
// its packet form (PT_MATCH_QUERY, multi_game_lobby.cpp), and the match (a found session).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_matchmaking_internal.h"
#include "volition/rfg/code/multi/multi_game/multi_game_playlists.h"

// ---- file-static globals
// Original: QT_ALLGAMES has no name (null).
static const char* Query_type_names[3] = {"Quickmatch", "Playlists", nullptr}; // 0x0167F008
// multi_game_playlist* Playlists (0x02FE6C70) is used here too; placed in multi_core_matchmaking_generic.cpp.
// int Settings_version (0x02FE88B0) is used here too; placed in multi.cpp.

// ---- game_query_base

// 0x0097C0B0 (:69): the first playlist open to every level (required level below 2), else the first one; join in
// progress, no world wide search.
void game_query_base::init()
{
    multi_game_playlist* playlist = Playlists;
    while (playlist != nullptr)
    {
        if (playlist->required_xp_level < 2)
        {
            break;
        }
        playlist = playlist->next;
        if (playlist == Playlists)
        {
            playlist = nullptr;
        }
    }
    if (playlist == nullptr)
    {
        playlist = Playlists;
    }
    if (playlist != nullptr)
    {
        m_query_type = playlist->m_query_type;
        m_game_mode = PGMC_MULTIPLAYER;
        m_game_type = playlist->game_type;
        m_playlist_id = playlist->playlist_id.get_crc();
        m_playlist_version = Settings_version;
    }
    m_allow_jip = true;
    m_world_wide_search = false;
}

// 0x00956CB0 (:98)
void game_query_base::copy(const game_query_base& src)
{
    m_query_type = src.m_query_type;
    m_game_mode = src.m_game_mode;
    m_game_type = src.m_game_type;
    m_playlist_id = src.m_playlist_id;
    m_playlist_version = src.m_playlist_version;
    m_allow_jip = src.m_allow_jip;
    m_world_wide_search = src.m_world_wide_search;
}

// 0x0097C100 (:110): Original: no range check (QT_ALLGAMES gives null).
const char* game_query_base::query_type_name() const
{
    return Query_type_names[m_query_type];
}

// 0x00956CF0 (:115)
const char* game_query_base::game_mode_name() const
{
    if ((unsigned int)m_game_mode < NUM_PLATFORM_GAME_MODE_CONTEXTS)
    {
        return Platform_game_mode_context_names[m_game_mode];
    }
    return "Invalid platform_game_mode_context";
}

// 0x00956D10 (:120)
const char* game_query_base::game_type_name() const
{
    if ((unsigned int)m_game_type < NUM_PLATFORM_GAME_TYPE_CONTEXTS)
    {
        return Platform_game_type_context_names[m_game_type];
    }
    return "Invalid platform_game_type_context";
}

// 0x00956D30 (:125): the type, mode and game type as bytes, the playlist id and version (BE), the two flags.
void game_query_base::write_to_packet(unsigned char* packet_data, unsigned short& packet_size) const
{
    packet_data[packet_size++] = (unsigned char)m_query_type;
    packet_data[packet_size++] = (unsigned char)m_game_mode;
    packet_data[packet_size++] = (unsigned char)m_game_type;
    write_u32_be(packet_data, packet_size, m_playlist_id);
    write_u32_be(packet_data, packet_size, m_playlist_version);
    packet_data[packet_size++] = m_allow_jip;
    packet_data[packet_size++] = m_world_wide_search;
}

// 0x00956E30 (:136)
void game_query_base::read_from_packet(unsigned char* packet_data, int& packet_offset)
{
    m_query_type = (query_type)packet_data[packet_offset++];
    m_game_mode = (platform_game_mode_context)packet_data[packet_offset++];
    m_game_type = (platform_game_type_context)packet_data[packet_offset++];
    m_playlist_id = read_u32_be(packet_data, packet_offset);
    m_playlist_version = read_u32_be(packet_data, packet_offset);
    m_allow_jip = packet_data[packet_offset++] != 0;
    m_world_wide_search = packet_data[packet_offset++] != 0;
}

// ---- game_match_base

// 0x009AFDE0 (:147): the session info's defaults (multi_session_info::init, inlined); the mode, type and DLC mask
// are kept.
void game_match_base::init()
{
    multi_session_info::init();
}

// 0x00956F30 (:153): Original: no range check.
const char* multi_game_matchmaking_internal_get_query_type_name(query_type query)
{
    return Query_type_names[query];
}
