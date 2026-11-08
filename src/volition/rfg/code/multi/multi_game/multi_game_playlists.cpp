// Original: volition/rfg/code/multi/multi_game/multi_game_playlists.cpp
//
// Playlists: mp_playlist_entries.xtbl's entries (a mode on a level with an options set, an ideal player count and a
// default score) in one circular pool list, and mp_playlists.xtbl's playlists (the matchmaking mode, ranked or public,
// "any available match" or a list of entries, the required level, players and party size) in another; a playlist's
// entries are linked through their own next/prev pointers. Matchmaking picks queries and entries from them.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_playlists.h"
#include "volition/rfg/code/multi/multi_game/multi_game_party.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/memory/split_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/string_comp.h"

#include <stdio.h>
#include <string.h>

// ---- file-static globals
// multi_gameplay_mode_names_multi_game Multi_gameplay_mode_names[8] (0x0167E4A8) is used here too; placed in multi_core_platform.cpp.
// const char* EMPTY_CHECKBOX_NAME (0x0167E50C) is used here too; placed in multi_game_mode_common.cpp.
// unsigned char My_level (0x0167F4F8) is used here too; placed in multi_game_badge.cpp.
// multi_game_playlist* Playlists (0x02FE6C70) is used here too; placed in multi_core_matchmaking_generic.cpp.
// Moved here from rfg_multi_unity_pending_early.inl (multi_game_settings.cpp, later in the unity TU, inlines
// multi_game_playlist_system_init).
static int Num_playlists; // 0x02FE6D38
static multi_game_playlist_entry* Playlist_entries; // 0x02FE8E4C

// mp_playlists.xtbl's Online_ID 0 is the first playlist matchmaking mode; 16 playlists.
static const int Playlist_online_id_count = 16;

// ---- internal functions
// Port helper: a pool mark (the CPU and GPU marks of a split pool), as rfg.exe inlines it
struct playlist_pool_mark
{
    unsigned int cpu;
    unsigned int gpu;
};

static playlist_pool_mark playlist_pool_mark_set(mempool_base& pool, bool with_gpu)
{
    playlist_pool_mark mark = {0, 0};
    if ((pool.get_flags() & 0x40) == 0)
    {
        mark.cpu = pool.mark();
    }
    else
    {
        mark.cpu = static_cast<split_mempool&>(pool).mark_cpu();
        if (with_gpu)
        {
            mark.gpu = static_cast<split_mempool&>(pool).mark_gpu();
        }
    }
    return mark;
}

static void playlist_pool_restore(mempool_base& pool, const playlist_pool_mark& mark, bool with_gpu)
{
    if ((pool.get_flags() & 0x40) != 0)
    {
        static_cast<split_mempool&>(pool).restore_to_mark_cpu(mark.cpu);
        if (with_gpu)
        {
            static_cast<split_mempool&>(pool).restore_to_mark_gpu(mark.gpu);
        }
        return;
    }
    pool.restore_to_mark(mark.cpu);
}

// Port helper: the entry in the pool list (null if none), as rfg.exe inlines it
static multi_game_playlist_entry* playlist_find_pool_entry(unsigned int entry_id)
{
    multi_game_playlist_entry* entry = Playlist_entries;
    while (entry != nullptr)
    {
        if (entry->entry_id.get_crc() == entry_id)
        {
            return entry;
        }
        entry = entry->pool_next;
        if (entry == Playlist_entries)
        {
            break;
        }
    }
    return nullptr;
}

// 0x009B1EF0 (:84): a new entry: its mode (by internal name; the bagman modes need their DLC), its level (the mode
// must have options there), the ideal players, the options set (the level's own without one) and the default score.
// LTCG: entry in ESI.
static bool parse_playlist_entry(multi_game_playlist_entry* entry, xml_element* entry_root, mempool_base& pool)
{
    entry->entry_id = checksum_stri(xml_table_get_req_string_ref(entry_root, "Name"), 0);
    if (playlist_find_pool_entry(entry->entry_id.get_crc()) != nullptr) // inlined
    {
        return false;
    }
    entry->mode = MGPM_NONE;
    const char* mode_string = xml_table_get_req_string_ref(entry_root, "mode");
    for (int i = 0; i < 8; i++)
    {
        if (v_stricmp(mode_string, Multi_gameplay_mode_names[i].internal_name) == 0)
        {
            entry->mode = (multi_gameplay_mode)i;
            break;
        }
    }
    if (entry->mode == MGPM_NONE || (!Bagman_dlc_unlocked && (entry->mode == MGPM_BAGMAN || entry->mode == MGPM_TEAM_BAGMAN)))
    {
        return false;
    }
    entry->level = multi_game_settings_find_level_by_filename(xml_table_get_req_string_ref(entry_root, "level"));
    if (entry->level == nullptr || entry->level->options->sets[entry->mode].mode_specific_options == nullptr)
    {
        return false;
    }
    xml_table_get_req_int(entry->ideal_players, entry_root, "ideal_players");
    const char* options_set = xml_table_get_opt_string_ref(entry_root, "options_set");
    if (options_set == nullptr)
    {
        entry->settings = &entry->level->options->sets[entry->mode];
    }
    else
    {
        // Original: the named set is not checked for null.
        entry->settings = &multi_game_settings_find_game_option_full_set(options_set)->sets[entry->mode];
    }
    xml_table_get_req_int(entry->default_score, entry_root, "default_score");
    entry->score = 0;
    return true;
}

// 0x009B2040 (:163): every entry from the pool (a rejected entry's memory is reused; the last one is given back)
static void parse_playlist_entries(xml_element* entries_root, mempool_base& pool)
{
    xml_element* entry_root = xml_table_find_req(entries_root, "playlist_entries");
    multi_game_playlist_entry* unused = nullptr;
    playlist_pool_mark mark = playlist_pool_mark_set(pool, true);
    if (entry_root == nullptr)
    {
        return;
    }
    do
    {
        multi_game_playlist_entry* entry = unused;
        if (entry == nullptr)
        {
            mark = playlist_pool_mark_set(pool, true);
            entry = (multi_game_playlist_entry*)pool.alloc(sizeof(multi_game_playlist_entry), 4);
        }
        memset(entry, 0, sizeof(multi_game_playlist_entry));
        unused = entry;
        if (parse_playlist_entry(entry, entry_root, pool))
        {
            if (Playlist_entries == nullptr)
            {
                entry->pool_next = entry;
                entry->pool_prev = entry;
            }
            else
            {
                Playlist_entries->pool_prev->pool_next = entry;
                entry->pool_prev = Playlist_entries->pool_prev;
                Playlist_entries->pool_prev = entry;
                entry->pool_next = Playlist_entries;
            }
            Playlist_entries = entry;
            unused = nullptr;
        }
        entry_root = xml_table_find_next(entries_root, entry_root, "playlist_entries");
    } while (entry_root != nullptr);
    if (unused != nullptr)
    {
        playlist_pool_restore(pool, mark, true);
    }
}

// 0x009833C0 (:225): a new playlist (its names copied into the pool) and its entries (from the pool list; each adds its
// DLC and bagman needs). A playlist whose ID exists adds the entries to that one and is dropped. Valid with at least one
// entry (or as "any available match"). Original: the error messages are formatted and not shown. LTCG: playlist in
// ECX, pool in EDX.
static bool parse_playlist(multi_game_playlist* playlist, xml_element* playlist_root, mempool_base& pool)
{
    const char* name = xml_table_get_req_string_ref(playlist_root, "Name");
    playlist->playlist_id = checksum_stri(name, 0);
    bool duplicate = false;
    bool no_entries = true;
    multi_game_playlist* target = multi_game_playlist_get_by_id(playlist->playlist_id.get_crc()); // inlined
    if (target != nullptr)
    {
        duplicate = true;
    }
    else
    {
        char message[512];
        int online_id = -1;
        xml_table_get_req_int(online_id, playlist_root, "Online_ID");
        playlist->mode_type = (platform_game_mode_context)(online_id + PGMC_PLAYLIST0);
        if (playlist->mode_type < PGMC_PLAYLIST0 || playlist->mode_type >= PGMC_PLAYLIST0 + Playlist_online_id_count)
        {
            sprintf(message, "Unknown matchmaking mode parsed from %s, parsing playlist %s", "mp_playlists.xtbl", name);
            return false;
        }
        const char* display_name = xml_table_get_req_string_ref(playlist_root, "Display_Name");
        playlist->display_name = (char*)pool.alloc((unsigned int)strlen(display_name) + 1, 0);
        strcpy(playlist->display_name, display_name);
        const char* description = xml_table_get_req_string_ref(playlist_root, "Description");
        playlist->description = (char*)pool.alloc((unsigned int)strlen(description) + 1, 0);
        strcpy(playlist->description, description);
        xml_table_get_req_int(playlist->required_xp_level, playlist_root, "Required_XP_Level");
        const char* game_type = xml_table_get_req_string_ref(playlist_root, "Game_Type");
        if (v_stricmp(game_type, "Ranked") == 0)
        {
            playlist->game_type = PGTC_RANKED;
        }
        else if (v_stricmp(game_type, "Public") == 0)
        {
            playlist->game_type = PGTC_PUBLIC;
        }
        else
        {
            sprintf(message, "Unknown game type parsed from %s, parsing playlist %s", "mp_playlists.xtbl", name);
            return false;
        }
        const char* matchmaking_type = xml_table_get_req_string_ref(playlist_root, "Matchmaking_Type");
        if (v_stricmp(matchmaking_type, "Any Available Match") == 0)
        {
            playlist->m_query_type = QT_QUICKMATCH;
        }
        else if (v_stricmp(matchmaking_type, "This Playlist Only") == 0)
        {
            playlist->m_query_type = QT_PLAYLIST;
        }
        else
        {
            sprintf(message, "Unknown matchmaking type parsed from %s, parsing playlist %s", "mp_playlists.xtbl", name);
            return false;
        }
        xml_table_get_req_int(playlist->min_players, playlist_root, "min_players");
        xml_table_get_req_int(playlist->max_players, playlist_root, "max_players");
        xml_table_get_req_int(playlist->max_party_size, playlist_root, "max_party_size");
        playlist->xp_scalar = 1.0f;
        xml_table_get_opt_float(playlist->xp_scalar, playlist_root, "xp_scalar");
        playlist->required_dlc_flags = 0;
        playlist->requires_bagman = false;
        target = playlist;
    }
    if (target->m_query_type == QT_QUICKMATCH)
    {
        no_entries = false;
    }
    else if (target->m_query_type == QT_PLAYLIST)
    {
        xml_element* entries = xml_table_find_req(playlist_root, "Playlist_Entries");
        for (xml_element* entry_root = xml_table_find_opt(entries, "Entries"); entry_root != nullptr; entry_root = xml_table_find_next(entries, entry_root, "Entries"))
        {
            const unsigned int entry_id = checksum_stri(xml_table_get_req_string_ref(entry_root, nullptr), 0).get_crc();
            if (multi_game_playlist_get_entry_by_id(target, entry_id) != nullptr) // inlined
            {
                continue;
            }
            multi_game_playlist_entry* entry = playlist_find_pool_entry(entry_id); // inlined
            if (entry == nullptr)
            {
                continue;
            }
            no_entries = false;
            if (target->entries == nullptr)
            {
                entry->next = entry;
                entry->prev = entry;
            }
            else
            {
                target->entries->prev->next = entry;
                entry->prev = target->entries->prev;
                target->entries->prev = entry;
                entry->next = target->entries;
            }
            target->entries = entry;
            if (entry->level->is_downloadable_content)
            {
                target->required_dlc_flags |= 1u << entry->level->dlc_bundle_id;
            }
            target->requires_bagman = target->requires_bagman || entry->mode == MGPM_BAGMAN;
            target->requires_bagman = target->requires_bagman || entry->mode == MGPM_TEAM_BAGMAN;
        }
    }
    return !duplicate && !no_entries;
}

// 0x009837C0 (:365): every playlist from the pool (a rejected playlist's memory is reused; the last one is given back)
static void parse_playlists(xml_element* playlists_root, mempool_base& pool)
{
    xml_element* playlist_root = xml_table_find_req(playlists_root, "playlists");
    multi_game_playlist* unused = nullptr;
    playlist_pool_mark mark = playlist_pool_mark_set(pool, false);
    if (playlist_root == nullptr)
    {
        return;
    }
    do
    {
        multi_game_playlist* playlist = unused;
        if (playlist == nullptr)
        {
            mark = playlist_pool_mark_set(pool, false);
            playlist = (multi_game_playlist*)pool.alloc(sizeof(multi_game_playlist), 4);
        }
        memset(playlist, 0, sizeof(multi_game_playlist));
        unused = playlist;
        if (parse_playlist(playlist, playlist_root, pool))
        {
            if (Playlists == nullptr)
            {
                playlist->next = playlist;
                playlist->prev = playlist;
            }
            else
            {
                Playlists->prev->next = playlist;
                playlist->prev = Playlists->prev;
                Playlists->prev = playlist;
                playlist->next = Playlists;
            }
            Num_playlists++;
            Playlists = playlist;
            unused = nullptr;
        }
        playlist_root = xml_table_find_next(playlists_root, playlist_root, "playlists");
    } while (playlist_root != nullptr);
    if (unused != nullptr)
    {
        playlist_pool_restore(pool, mark, false);
    }
}

// 0x009838C0 (:422): "any available match" takes the largest party size of the entry playlists our level and every
// lobby member may play
int multi_game_playlist::get_max_party_size() const
{
    if (m_query_type != QT_QUICKMATCH)
    {
        return max_party_size;
    }
    int size = 0;
    const multi_game_playlist* playlist = Playlists;
    while (playlist != nullptr)
    {
        if (playlist->m_query_type != QT_QUICKMATCH && playlist->required_xp_level <= (int)My_level &&
            multi_game_party_all_clients_have_playlist(Lobby_session->get_connection_list() /* m_connections, inlined */, playlist) && size <= playlist->max_party_size)
        {
            size = playlist->max_party_size;
        }
        playlist = playlist->next;
        if (playlist == Playlists)
        {
            break;
        }
    }
    return size;
}

// 0x009B2160 (:444): both tables from the settings files (multi_game_settings_system_init inlines the same)
void multi_game_playlist_system_init(mempool_base& pool)
{
    Playlist_entries = nullptr;
    Playlists = nullptr;
    Num_playlists = 0;
    xml_element* entries = multi_game_settings_retrieve_settings_file("mp_playlist_entries.xtbl");
    parse_playlist_entries(entries, pool);
    xml_table_close(entries);
    xml_element* playlists = multi_game_settings_retrieve_settings_file("mp_playlists.xtbl");
    parse_playlists(playlists, pool);
    xml_table_close(playlists);
}

// 0x00960020 (:467): the pool memory belongs to the caller
void multi_game_playlist_system_shutdown()
{
}

// 0x00960030 (:473)
multi_game_playlist* multi_game_playlist_get_by_id(unsigned int playlist_id)
{
    multi_game_playlist* playlist = Playlists;
    while (playlist != nullptr)
    {
        if (playlist->playlist_id.get_crc() == playlist_id)
        {
            return playlist;
        }
        playlist = playlist->next;
        if (playlist == Playlists)
        {
            break;
        }
    }
    return nullptr;
}

// 0x00960060 (:485): by matchmaking mode (the 16 playlist modes only)
multi_game_playlist* multi_game_playlist_get_by_mode(unsigned int playlist_mode)
{
    if (playlist_mode - PGMC_PLAYLIST0 >= (unsigned int)Playlist_online_id_count)
    {
        return nullptr;
    }
    multi_game_playlist* playlist = Playlists;
    while (playlist != nullptr)
    {
        if ((unsigned int)playlist->mode_type == playlist_mode)
        {
            return playlist;
        }
        playlist = playlist->next;
        if (playlist == Playlists)
        {
            break;
        }
    }
    return nullptr;
}

// 0x00960090 (:499)
const multi_game_playlist* multi_game_playlist_get_first()
{
    return Playlists;
}

// 0x009600A0 (:504): null after the last
const multi_game_playlist* multi_game_playlist_get_next(const multi_game_playlist* plp)
{
    return plp->next != Playlists ? plp->next : nullptr;
}

// 0x009600C0 (:510): a random entry of the playlist's list (of every entry for "any available match"; the first
// without any)
const multi_game_playlist_entry* multi_game_playlist_get_random_entry(const multi_game_playlist* src_playlist)
{
    const bool own_list = src_playlist->m_query_type == QT_PLAYLIST;
    multi_game_playlist_entry* first = own_list ? src_playlist->entries : Playlist_entries;
    int count = 0;
    for (multi_game_playlist_entry* entry = first; entry != nullptr;)
    {
        count++;
        entry = own_list ? entry->next : entry->pool_next;
        if (entry == first)
        {
            break;
        }
    }
    if (count <= 0)
    {
        return Playlist_entries;
    }
    int skip = int_rand_range(0, count - 1);
    multi_game_playlist_entry* entry = first;
    while (entry != nullptr)
    {
        if (skip < 1)
        {
            return entry;
        }
        entry = own_list ? entry->next : entry->pool_next;
        skip--;
        if (entry == first)
        {
            break;
        }
    }
    // The playlist's own list falls back to its first entry, the pool list to the pool's first.
    return own_list ? first : Playlist_entries;
}

// 0x00960170 (:554): in the pool list for "any available match", else in the playlist's list
multi_game_playlist_entry* multi_game_playlist_get_entry_by_id(const multi_game_playlist* src_playlist, unsigned int entry_id)
{
    if (src_playlist == nullptr)
    {
        return nullptr;
    }
    if (src_playlist->m_query_type == QT_QUICKMATCH)
    {
        return playlist_find_pool_entry(entry_id);
    }
    multi_game_playlist_entry* entry = src_playlist->entries;
    while (entry != nullptr)
    {
        if (entry->entry_id.get_crc() == entry_id)
        {
            return entry;
        }
        entry = entry->next;
        if (entry == src_playlist->entries)
        {
            break;
        }
    }
    return nullptr;
}
