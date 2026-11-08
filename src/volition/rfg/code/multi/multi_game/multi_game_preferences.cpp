// Original: volition/rfg/code/multi/multi_game/multi_game_preferences.cpp
//
// The matchmaking preferences: the ideal and current queries and the playlist history (the last 10 played and vetoed
// playlist entries, kept in the profile, and each lobby member's copy), the query lists matchmaking searches, the
// match score (the DLC mismatch) and the preferred entry choice (random scores lowered for the members' recent and
// vetoed entries and for the veto's level and mode). The penalties are tweak table values (0 without a tweak).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_preferences.h"
#include "volition/rfg/code/multi/multi_game/multi_game_matchmaking_generic.h"
#include "volition/rfg/code/multi/multi_game/multi_game_party.h"
#include "volition/rfg/code/multi/multi_game/multi_game_playlists.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/util/bit_util.h"
#include "keen/base/randomnumbergenerator.hpp"
#include "keen/base/systemtimer.hpp"

#include <new>
#include <string.h>

// ---- file-static globals
// unsigned char My_level (0x0167F4F8) is used here too; placed in multi_game_badge.cpp.
static int Base_score_penalty_for_having_played; // 0x02FE5BE8
static int Score_penalty_for_same_level_as_veto; // 0x02FE5BF4
// multi_game_playlist* Playlists (0x02FE6C70) is used here too; placed in multi_core_matchmaking_generic.cpp.
static int Base_score_penalty_fade_increment_for_having_played; // 0x02FE7184
static int Base_score_penalty_fade_increment_for_vetoing; // 0x02FE88B4
static int Score_penalty_for_same_mode_as_veto; // 0x02FE8D5C
// multi_game_playlist_entry* Playlist_entries (0x02FE8E4C) is used here too; placed in multi_game_playlists.cpp.
static int Score_penalty_for_mismatched_dlc; // 0x02FEB348
static int Base_score_penalty_for_vetoing; // 0x02FEB37C (multi_core's references to this address are the end of Object_type_registration_list, which precedes it)
// preferences_members* Preferences (0x02FEB70C) is used here too; placed in multi_core_session_generic.cpp.
static unsigned int Available_dlc_flags; // 0x02FEB718

// The penalties' tweak table items (dynamic initializers; rfg.exe's unity TU registers them before
// multi_game_mode_capture_the_flag.cpp's).
static tweak_table_item Tweak_table_item_base_score_penalty_for_having_played("base_score_penalty_for_having_played", &Base_score_penalty_for_having_played); // 0x030176B8
static tweak_table_item Tweak_table_item_base_score_penalty_fade_increment_for_having_played("base_score_penalty_fade_increment_for_having_played", &Base_score_penalty_fade_increment_for_having_played); // 0x02FEBC10
static tweak_table_item Tweak_table_item_base_score_penalty_for_vetoing("base_score_penalty_for_vetoing", &Base_score_penalty_for_vetoing); // 0x0300F8D4
static tweak_table_item Tweak_table_item_base_score_penalty_fade_increment_for_vetoing("base_score_penalty_fade_increment_for_vetoing", &Base_score_penalty_fade_increment_for_vetoing); // 0x03017734
static tweak_table_item Tweak_table_item_score_penalty_for_same_level_as_veto("score_penalty_for_same_level_as_veto", &Score_penalty_for_same_level_as_veto); // 0x02FEDA2C
static tweak_table_item Tweak_table_item_score_penalty_for_same_mode_as_veto("score_penalty_for_same_mode_as_veto", &Score_penalty_for_same_mode_as_veto); // 0x030176CC
static tweak_table_item Tweak_table_item_score_penalty_for_mismatched_dlc("score_penalty_for_mismatched_dlc", &Score_penalty_for_mismatched_dlc); // 0x02FEB7CC

// Magic number: the history keeps the last 10 entries.
static const int History_size = 10;
// Lobby members.
static const int Max_history_players = 16;

// 0x009601D0
preferences_members::preferences_members()
{
}

// 0x00983920 (:127): the preferences in the multiplayer allocators, an empty ideal query and no history
void multi_game_prefereces_system_init()
{
    void* memory = MP_allocators_mempool.alloc(sizeof(preferences_members), 0);
    Preferences = memory != nullptr ? new (memory) preferences_members() : nullptr;
    Preferences->ideal_match.init();
    for (int i = 0; i < History_size; i++)
    {
        Preferences->my_history.playlist_history[i] = 0;
    }
    for (int i = 0; i < History_size; i++)
    {
        Preferences->my_history.veto_history[i] = 0;
    }
}

// 0x009601E0 (:140)
void multi_game_preferences_read_from_profile()
{
    player_profile_data* profile = player_profile_get("multi_game_preferences_read_from_profile", (int)Player_controller_index);
    if (profile != nullptr)
    {
        Preferences->my_history = profile->mp_prefs_history;
    }
}

// 0x00960280 (:153)
void multi_game_preferences_write_to_profile()
{
    player_profile_data* profile = player_profile_get("multi_game_preferences_write_to_profile", (int)Player_controller_index);
    if (profile != nullptr)
    {
        profile->mp_prefs_history = Preferences->my_history;
    }
}

// 0x00960320 (:167)
void multi_game_prefereces_system_shutdown()
{
}

// Port helper: the query searches the playlist (from the ideal query, joins in progress allowed), as rfg.exe inlines it
static void preferences_set_playlist_query(game_query& query, const multi_game_playlist* playlist)
{
    query.copy(Preferences->ideal_match);
    query.m_playlist_id = playlist->playlist_id.get_crc();
    query.m_query_type = QT_PLAYLIST;
    query.m_game_mode = playlist->mode_type;
    query.m_game_type = playlist->game_type;
    query.m_allow_jip = true;
}

// Port helper: the playlists our party may search (the ideal query's game type, a playlist list, our level, the party
// size and every lobby member having it), as rfg.exe inlines it
static bool preferences_playlist_is_searchable(const multi_game_playlist* playlist, int max_party_size)
{
    return playlist->game_type == Preferences->ideal_match.m_game_type && playlist->m_query_type != QT_QUICKMATCH && playlist->required_xp_level <= (int)My_level &&
           (int)multi_game_party_get_num_players_in_party() <= max_party_size && multi_game_party_all_clients_have_playlist(Lobby_session->get_connection_list(), playlist);
}

// 0x009839A0 (:172): "any available match" searches each playlist the party may play, locally then world wide (two
// queries each); a playlist query searches itself the same two ways
void multi_game_preferences_create_query_list(game_query* dest_queries, int* num_dest_queries, const int max_dest_queries)
{
    *num_dest_queries = 0;
    if (Preferences->ideal_match.m_query_type != QT_QUICKMATCH)
    {
        if (max_dest_queries < 2)
        {
            return;
        }
        *num_dest_queries = 2;
        dest_queries[0].copy(Preferences->ideal_match);
        dest_queries[0].m_allow_jip = true;
        dest_queries[1].copy(Preferences->ideal_match);
        dest_queries[1].m_allow_jip = true;
        dest_queries[1].m_world_wide_search = true;
        return;
    }
    const multi_game_playlist* playlist = Playlists;
    while (playlist != nullptr && *num_dest_queries < max_dest_queries)
    {
        // Original: the second query of a pair is written without checking the room left.
        if (preferences_playlist_is_searchable(playlist, playlist->get_max_party_size())) // inlined
        {
            preferences_set_playlist_query(dest_queries[*num_dest_queries], playlist); // inlined
            (*num_dest_queries)++;
            dest_queries[*num_dest_queries].copy(dest_queries[*num_dest_queries - 1]);
            dest_queries[*num_dest_queries].m_world_wide_search = true;
            (*num_dest_queries)++;
        }
        playlist = playlist->next;
        if (playlist == Playlists)
        {
            break;
        }
    }
}

// 0x00983B40 (:223): every playlist the party may play, locally then world wide, with its party size
void multi_game_preferences_create_any_playlist_query_list(game_query* dest_queries, unsigned int* dest_max_party_sizes, int* num_dest_queries, const int max_dest_queries)
{
    *num_dest_queries = 0;
    memset(dest_max_party_sizes, 0, max_dest_queries * sizeof(unsigned int));
    const multi_game_playlist* playlist = Playlists;
    while (playlist != nullptr && *num_dest_queries < max_dest_queries)
    {
        const int max_party_size = playlist->get_max_party_size();
        if (max_party_size >= 0 && preferences_playlist_is_searchable(playlist, max_party_size)) // inlined
        {
            // Original: only the first query of a pair gets its party size.
            dest_max_party_sizes[*num_dest_queries] = (unsigned int)max_party_size;
            preferences_set_playlist_query(dest_queries[*num_dest_queries], playlist); // inlined
            (*num_dest_queries)++;
            dest_queries[*num_dest_queries].copy(dest_queries[*num_dest_queries - 1]);
            dest_queries[*num_dest_queries].m_world_wide_search = true;
            (*num_dest_queries)++;
        }
        playlist = playlist->next;
        if (playlist == Playlists)
        {
            break;
        }
    }
}

// 0x00983CC0 (:262): the fourth playlist mode's playlist, else (a party size) a random playlist list of exactly that
// party size. Original: the mode's playlist is not checked for null, nor is a random pick from an empty id list.
void multi_game_preferences_get_random_query(game_query* pResultQuery, unsigned int maxPartySize)
{
    pResultQuery->init();
    const multi_game_playlist* playlist = multi_game_playlist_get_by_mode(PGMC_PLAYLIST3); // inlined
    pResultQuery->m_playlist_id = playlist->playlist_id.get_crc();
    pResultQuery->m_query_type = QT_PLAYLIST;
    pResultQuery->m_game_mode = playlist->mode_type;
    pResultQuery->m_game_type = playlist->game_type;
    pResultQuery->m_allow_jip = true;
    if (maxPartySize == 0)
    {
        return;
    }
    // Original: 16 playlists at most fit.
    unsigned int ids[16];
    unsigned int count = 0;
    const multi_game_playlist* candidate = Playlists;
    while (candidate != nullptr)
    {
        if ((unsigned int)candidate->get_max_party_size() == maxPartySize && candidate->m_query_type != QT_QUICKMATCH)
        {
            ids[count++] = candidate->playlist_id.get_crc();
        }
        candidate = candidate->next;
        if (candidate == Playlists)
        {
            break;
        }
    }
    keen::RandomNumberGenerator random;
    random.initFromSeed(keen::SystemTimer::getCurrentMilliseconds());
    const multi_game_playlist* chosen = multi_game_playlist_get_by_id(ids[random.getRandomValue(count)]);
    if (chosen != nullptr)
    {
        pResultQuery->m_playlist_id = chosen->playlist_id.get_crc();
        pResultQuery->m_query_type = QT_PLAYLIST;
        pResultQuery->m_game_mode = chosen->mode_type;
        pResultQuery->m_game_type = chosen->game_type;
        pResultQuery->m_allow_jip = true;
    }
}

// 0x00983DE0 (:301): each DLC the match and we do not share costs the penalty
int multi_game_preferences_calculate_score(const game_match* game)
{
    return -(bit_count<unsigned int>(game->m_dlc_mask ^ Available_dlc_flags) * Score_penalty_for_mismatched_dlc);
}

// 0x00960330 (:315)
void multi_game_preferences_set_ideal_query(const game_query* new_query)
{
    Preferences->ideal_match.copy(*new_query);
}

// 0x00960350 (:320)
void multi_game_preferences_set_current_query(const game_query* new_query)
{
    Preferences->current_match.copy(*new_query);
}

// 0x00960370 (:325)
const game_query* multi_game_preferences_get_ideal_query()
{
    return &Preferences->ideal_match;
}

// 0x00960380 (:330)
const game_query* multi_game_preferences_get_current_query()
{
    return &Preferences->current_match;
}

// 0x00960390 (:335): the entry goes to the front of the played history, which goes to the profile
void multi_game_preferences_mark_played_entry(unsigned int playlist_id)
{
    for (int i = History_size - 1; i > 0; i--)
    {
        Preferences->my_history.playlist_history[i] = Preferences->my_history.playlist_history[i - 1];
    }
    Preferences->my_history.playlist_history[0] = playlist_id;
    multi_game_preferences_write_to_profile();
}

// 0x00960480 (:347): the same for the vetoed history
void multi_game_preferences_mark_vetoed_entry(unsigned int playlist_id)
{
    for (int i = History_size - 1; i > 0; i--)
    {
        Preferences->my_history.veto_history[i] = Preferences->my_history.veto_history[i - 1];
    }
    Preferences->my_history.veto_history[0] = playlist_id;
    multi_game_preferences_write_to_profile();
}

// 0x00960570 (:359): every member's copy
void multi_game_preferences_reset_total_history()
{
    for (int idx = 0; idx < 16; idx++)
    {
        multi_game_preferences_reset_player_history(idx);
    }
}

// 0x009605C0 (:371)
void multi_game_preferences_reset_player_history(int idx)
{
    for (int i = 0; i < History_size; i++)
    {
        Preferences->total_history[idx].playlist_history[i] = 0;
        Preferences->total_history[idx].veto_history[i] = 0;
    }
}

// 0x00960670 (:381): our history as a member's copy
void multi_game_preferences_apply_my_history(int idx)
{
    Preferences->total_history[idx] = Preferences->my_history;
}

// 0x00960760 (:391)
void multi_game_preferences_clear_players_history(unsigned int player_idx)
{
    multi_game_preferences_reset_player_history((int)player_idx);
}

// 0x00960810 (:401): PT_PLAY_HISTORY: the sender's copy (played, then vetoed; BE)
void multi_game_preferences_read_players_history(unsigned char* packet_data, const sender& from)
{
    if (from.connection == nullptr)
    {
        return;
    }
    multi_game_preferences_history& history = Preferences->total_history[from.connection->index_in_session];
    int offset = 0;
    for (int i = 0; i < History_size; i++)
    {
        history.playlist_history[i] = read_u32_be(packet_data, offset);
    }
    for (int i = 0; i < History_size; i++)
    {
        history.veto_history[i] = read_u32_be(packet_data, offset);
    }
}

// 0x009D2320 (:419): PT_PLAY_HISTORY to cp. Original: it sends the copy at cp's own index (ours only when cp is our
// connection).
void multi_game_preferences_send_my_history(multi_connection* cp)
{
    if (cp == nullptr)
    {
        return;
    }
    const multi_game_preferences_history& history = Preferences->total_history[cp->index_in_session];
    unsigned char packet[1200];
    packet[0] = PT_PLAY_HISTORY;
    unsigned short packet_size = packet_header_size(PT_PLAY_HISTORY);
    for (int i = 0; i < History_size; i++)
    {
        write_u32_be(packet, packet_size, history.playlist_history[i]);
    }
    for (int i = 0; i < History_size; i++)
    {
        write_u32_be(packet, packet_size, history.veto_history[i]);
    }
    packet_set_size(packet, packet_size);
    multi_io_send_reliable(cp, packet, packet_size, nullptr, nullptr, nullptr);
}

// 0x00960EC0 (:436): the current query's playlist's entries (but the veto) get random scores up to their defaults,
// lowered for each member's recent plays and vetoes (the newest the most) and for sharing the veto's level or mode;
// a random entry of the best score wins (a random entry of the playlist without one). Original: the playlist is not
// checked for null.
const multi_game_playlist_entry* multi_game_preferences_choose_random_preferred_playlist_entry(const multi_game_playlist_entry* veto)
{
    const multi_game_playlist* playlist = multi_game_playlist_get_by_id(Preferences->current_match.m_playlist_id); // inlined
    for (multi_game_playlist_entry* entry = playlist->entries; entry != nullptr;)
    {
        if (entry != veto)
        {
            entry->score = int_rand_range(0, entry->default_score);
        }
        entry = entry->next;
        if (entry == playlist->entries)
        {
            break;
        }
    }
    for (int player = 0; player < Max_history_players; player++)
    {
        const multi_game_preferences_history& history = Preferences->total_history[player];
        for (int i = 0; i < History_size; i++)
        {
            multi_game_playlist_entry* played = multi_game_playlist_get_entry_by_id(playlist, history.playlist_history[i]); // inlined
            if (played != nullptr)
            {
                played->score += Base_score_penalty_fade_increment_for_having_played * i - Base_score_penalty_for_having_played;
            }
        }
        for (int i = 0; i < History_size; i++)
        {
            multi_game_playlist_entry* vetoed = multi_game_playlist_get_entry_by_id(playlist, history.veto_history[i]); // inlined
            if (vetoed != nullptr)
            {
                vetoed->score += Base_score_penalty_fade_increment_for_vetoing * i - Base_score_penalty_for_vetoing;
            }
        }
    }
    if (veto != nullptr)
    {
        for (multi_game_playlist_entry* entry = playlist->entries; entry != nullptr;)
        {
            if (entry != veto)
            {
                if (entry->level == veto->level)
                {
                    entry->score -= Score_penalty_for_same_level_as_veto;
                }
                if (entry->mode == veto->mode)
                {
                    entry->score -= Score_penalty_for_same_mode_as_veto;
                }
            }
            entry = entry->next;
            if (entry == playlist->entries)
            {
                break;
            }
        }
    }
    multi_game_playlist_entry* const first = playlist->entries;
    if (first == nullptr)
    {
        return multi_game_playlist_get_random_entry(playlist);
    }
    // Magic number: rfg.exe starts at INT_MIN.
    int best_score = (int)0x80000000;
    int num_best = 0;
    multi_game_playlist_entry* best = nullptr;
    for (multi_game_playlist_entry* entry = first; entry != nullptr;)
    {
        if (entry != veto)
        {
            if (best_score < entry->score)
            {
                num_best = 1;
                best = entry;
                best_score = entry->score;
            }
            else if (entry->score == best_score)
            {
                num_best++;
            }
        }
        entry = entry->next;
        if (entry == first)
        {
            break;
        }
    }
    multi_game_playlist_entry* chosen = best;
    if (num_best > 0)
    {
        int skip = int_rand_range(0, num_best - 1);
        for (multi_game_playlist_entry* entry = first; entry != nullptr;)
        {
            if (entry != veto && entry->score == best_score)
            {
                chosen = entry;
                if (skip < 1)
                {
                    break;
                }
                skip--;
            }
            entry = entry->next;
            // Original: without a hit after the skip count, the first best entry wins.
            chosen = best;
            if (entry == first)
            {
                break;
            }
        }
    }
    if (chosen != nullptr)
    {
        return chosen;
    }
    return multi_game_playlist_get_random_entry(playlist);
}
