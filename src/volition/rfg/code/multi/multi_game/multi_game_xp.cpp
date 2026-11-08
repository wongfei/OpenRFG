// Original: volition/rfg/code/multi/multi_game/multi_game_xp.cpp
//
// Multiplayer experience: the experience events (mp_xp.xtbl: value, repeat delay, bonus events and the level that
// unlocks them), the levels (Xp_required_per_level), the unlocks per level (bonus events, playlists, weapons,
// characters, badge parts) sorted by level, the profile's experience (read at init, written at the match's end), and
// the event and level-up packets (PT_XP_EVENT, PT_LEVELED_UP).
// Ported whole (stub_impl.md S6, ahead of its tier: the match end's experience tally reads the event table). The
// game mode files fire the events (multi_game_mode_*.cpp).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_session_base.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game_badge.h"
#include "volition/rfg/code/multi/multi_game/multi_game_character.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_playlists.h"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/string_pool.h"

#include <stdio.h>
#include <stdlib.h>

// ---- file-static globals
// unsigned int Local_game_data_handles[4] (0x0167E5F4) is used here too; placed in multi_join.cpp.
static float Multi_game_xp_requirement_multiplier = 0.5f; // 0x0167F4F4
// unsigned char My_level (0x0167F4F8) is used here too; placed in multi_game_badge.cpp.
// multi_game_playlist* Playlists (0x02FE6C70) is used here too; placed in multi_core_matchmaking_generic.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.
// preferences_members* Preferences (0x02FEB70C) is used here too; placed in multi_core_session_generic.cpp.
// multi_player_stats* Mp_player_stats (0x02FEB720) is used here too; placed in multi_game_mode_bagman.cpp.
// int My_xp (0x02FEB760) is used here too; placed in multi_core_platform.cpp.
static int My_bonus_xp; // 0x02FEB764
static multi_xp_unlock* Multi_xp_unlocks; // 0x02FEB768
static int Num_xp_unlocks; // 0x02FEB76C
static string_pool Multi_xp_string_pool(nullptr); // 0x02FED1F0 (dynamic initializer 0x010749C0: no name)
// multi_xp_event Multi_xp_events[84] (0x0300D240) is used here too; placed in multi_game_mode_bagman.cpp.
// obj_class_instantiator multi_object_backpack_instantiator (0x0300F850) is used here too; placed in multi_game_object_backpack.cpp.
// string_pool Backpack_string_pool (0x0300F858) is used here too; placed in multi_game_object_backpack.cpp.
// __m128i s_value112 (0x0300F8C0): a compiler constant of the unity TU; none of this file's functions reads it.

// The experience a level starts at (index level - 1), scaled by the requirement multiplier: the dynamic initializer
// 0x01071940 (the table is a const in rfg.pdb but lives in .bss and is written at start-up).
#define XP_LEVEL(base) (int)(Multi_game_xp_requirement_multiplier * (base))
static const int Xp_required_per_level[126] = { // 0x03017810
    XP_LEVEL(0.0f), XP_LEVEL(50.0f), XP_LEVEL(100.0f), XP_LEVEL(150.0f), XP_LEVEL(200.0f), XP_LEVEL(250.0f),
    XP_LEVEL(325.0f), XP_LEVEL(400.0f), XP_LEVEL(475.0f), XP_LEVEL(550.0f), XP_LEVEL(625.0f), XP_LEVEL(700.0f),
    XP_LEVEL(800.0f), XP_LEVEL(900.0f), XP_LEVEL(1000.0f), XP_LEVEL(1100.0f), XP_LEVEL(1200.0f), XP_LEVEL(1300.0f),
    XP_LEVEL(1400.0f), XP_LEVEL(1500.0f), XP_LEVEL(1600.0f), XP_LEVEL(1700.0f), XP_LEVEL(1800.0f), XP_LEVEL(1900.0f),
    XP_LEVEL(2000.0f), XP_LEVEL(2150.0f), XP_LEVEL(2300.0f), XP_LEVEL(2450.0f), XP_LEVEL(2600.0f), XP_LEVEL(2750.0f),
    XP_LEVEL(2900.0f), XP_LEVEL(3050.0f), XP_LEVEL(3200.0f), XP_LEVEL(3350.0f), XP_LEVEL(3500.0f), XP_LEVEL(3650.0f),
    XP_LEVEL(3800.0f), XP_LEVEL(3950.0f), XP_LEVEL(4100.0f), XP_LEVEL(4250.0f), XP_LEVEL(4400.0f), XP_LEVEL(4550.0f),
    XP_LEVEL(4700.0f), XP_LEVEL(4850.0f), XP_LEVEL(5000.0f), XP_LEVEL(5200.0f), XP_LEVEL(5400.0f), XP_LEVEL(5600.0f),
    XP_LEVEL(5800.0f), XP_LEVEL(6000.0f), XP_LEVEL(6200.0f), XP_LEVEL(6400.0f), XP_LEVEL(6600.0f), XP_LEVEL(6800.0f),
    XP_LEVEL(7000.0f), XP_LEVEL(7200.0f), XP_LEVEL(7400.0f), XP_LEVEL(7600.0f), XP_LEVEL(7800.0f), XP_LEVEL(8000.0f),
    XP_LEVEL(8250.0f), XP_LEVEL(8500.0f), XP_LEVEL(8750.0f), XP_LEVEL(9000.0f), XP_LEVEL(9250.0f), XP_LEVEL(9500.0f),
    XP_LEVEL(9750.0f), XP_LEVEL(10000.0f), XP_LEVEL(10300.0f), XP_LEVEL(10600.0f), XP_LEVEL(10900.0f),
    XP_LEVEL(11200.0f), XP_LEVEL(11500.0f), XP_LEVEL(11800.0f), XP_LEVEL(12200.0f), XP_LEVEL(12600.0f),
    XP_LEVEL(13000.0f), XP_LEVEL(13400.0f), XP_LEVEL(13800.0f), XP_LEVEL(14200.0f), XP_LEVEL(14600.0f),
    XP_LEVEL(15000.0f), XP_LEVEL(15500.0f), XP_LEVEL(16000.0f), XP_LEVEL(16500.0f), XP_LEVEL(17000.0f),
    XP_LEVEL(17500.0f), XP_LEVEL(18000.0f), XP_LEVEL(18750.0f), XP_LEVEL(19500.0f), XP_LEVEL(20250.0f),
    XP_LEVEL(21000.0f), XP_LEVEL(21750.0f), XP_LEVEL(22500.0f), XP_LEVEL(23250.0f), XP_LEVEL(24000.0f),
    XP_LEVEL(25000.0f), XP_LEVEL(26000.0f), XP_LEVEL(27000.0f), XP_LEVEL(28000.0f), XP_LEVEL(29000.0f),
    XP_LEVEL(30000.0f), XP_LEVEL(31000.0f), XP_LEVEL(32000.0f), XP_LEVEL(33000.0f), XP_LEVEL(34000.0f),
    XP_LEVEL(36000.0f), XP_LEVEL(38000.0f), XP_LEVEL(40000.0f), XP_LEVEL(42000.0f), XP_LEVEL(44000.0f),
    XP_LEVEL(46000.0f), XP_LEVEL(48000.0f), XP_LEVEL(50000.0f), XP_LEVEL(52000.0f), XP_LEVEL(54000.0f),
    XP_LEVEL(56000.0f), XP_LEVEL(58000.0f), XP_LEVEL(60000.0f), XP_LEVEL(65000.0f), XP_LEVEL(70000.0f),
    XP_LEVEL(75000.0f), XP_LEVEL(85000.0f), XP_LEVEL(100000.0f), XP_LEVEL(120000.0f), XP_LEVEL(150000.0f)};
#undef XP_LEVEL

// Magic numbers: 126 levels (the last one has no next level), at most 256 unlocks (the 0xC00-byte array), the
// experience totals are clamped to 999999 (what the tally and the profile show).
static const int Xp_num_levels = 126;
static const int Xp_max_unlocks = 256;
static const int Xp_max_total = 999999;
// Magic number: the "no weapon" sledge id (the profile's and the character select's).
static const int Xp_no_sledge = 0xFF;

// ---- internal functions

// Port helper: the level of an experience total, as rfg.exe inlines multi_game_xp_calculate_level.
static unsigned char level_of(int xp)
{
    int level = 0;
    while (level < Xp_num_levels && xp >= Xp_required_per_level[level])
    {
        level++;
    }
    return (unsigned char)level;
}

// Port helper: the profile of the controller that plays (-1 maps to 0, as rfg.exe's sbb/and idiom does).
static player_profile_data* xp_profile(const char* calling_system_name)
{
    return player_profile_get(calling_system_name, Player_controller_index != 0xFFFFFFFFu ? (int)Player_controller_index : 0);
}

// 0x009D6FC0 (:1078): the server tells everyone but the player its new level; a client tells the server. LTCG: the
// game data handle in EDI.
static void send_leveled_up(unsigned int multi_game_data_handle, unsigned char new_level)
{
    multi_session* session = multi_session_get_by_type(ST_GAME);
    if (session == nullptr || session->get_state() == MSS_NONE)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_LEVELED_UP;
    unsigned short packet_size = packet_header_size(PT_LEVELED_UP);
    if (Multiplayer && Server)
    {
        write_u32_be(packet, packet_size, multi_game_data_handle);
        packet[packet_size++] = new_level;
        packet_set_size(packet, packet_size);
        multi_io_send_to_all_except_one_reliable(session, packet, packet_size, session->get_player_connection(multi_game_data_handle));
        return;
    }
    packet[packet_size++] = new_level;
    packet_set_size(packet, packet_size);
    multi_io_send_reliable(session->get_host_connection(), packet, packet_size, nullptr, nullptr, nullptr);
}

// 0x009E2760 (:314): the local player's new level is kept (and its best level stat) and sent; the server passes
// another player's on. LTCG: the game data handle in EAX.
static void player_leveled_up(unsigned int multi_game_data_handle, unsigned char new_level)
{
    if (multi_game_data_handle != 0xFFFFFFFFu && Local_game_data_handles[multi_game_data_handle >> 16] == multi_game_data_handle)
    {
        My_level = new_level;
        stats_update_best(STATS_MP_XP_LEVEL, new_level);
        send_leveled_up(multi_game_data_handle, new_level);
        multi_game_data_set_player_level(multi_game_data_handle, new_level);
        return;
    }
    if (Multiplayer && Server)
    {
        send_leveled_up(multi_game_data_handle, new_level);
    }
    multi_game_data_set_player_level(multi_game_data_handle, new_level);
}

// 0x009E27D0 (:340): a connected non-marauder player gets the event's experience (scaled by the ranked playlist) once
// per repetition, with the bonus events only from their unlock level on and only in a match with opponents; the
// local player sees it on the HUD and may level up. LTCG: the game data handle in EDI.
static void trigger_event_internal(unsigned int multi_game_data_handle, multi_xp_events event, int repititions, int weapon_id, int backpack_id)
{
    // Magic number: no experience in a one-player match.
    if (Multi_match_info.max_num_players < 2 || Mp_game_data == nullptr || multi_game_data_handle == 0xFFFFFFFFu)
    {
        return;
    }
    multi_game_data* data = game_data_of(multi_game_data_handle);
    if (!data->in_use)
    {
        // rfg.exe tail-calls timestamp::valid on the lingering timer and drops the result.
        data->linger_time.valid();
        return;
    }
    if (data->team == HUMAN_TEAM_MARAUDER || !multi_game_data_is_connected(multi_game_data_handle))
    {
        return;
    }
    const bool mine = multi_game_data_is_my_handle(multi_game_data_handle);
    const multi_xp_event& info = Multi_xp_events[event];
    if ((info.bonus && (int)data->player_level < info.minimum_level) || !Mp_last_man_standing)
    {
        return;
    }
    int xp_change = info.value;
    const multi_session_info_base* session_info = Lobby_session->get_info();
    if (!session_info->systemlink_or_lan && !session_info->private_match)
    {
        const multi_game_playlist* playlist = multi_game_playlist_get_by_id(Preferences->current_match.m_playlist_id);
        if (playlist != nullptr)
        {
            // As rfg.exe: the experience goes through float, the product through double, truncated.
            xp_change = (int)((double)playlist->xp_scalar * (double)(float)xp_change);
        }
    }
    for (int i = repititions; i > 0; i--)
    {
        multi_game_stats_alter_player_xp(multi_game_data_handle, xp_change, event, weapon_id, backpack_id);
    }
    if (!mine)
    {
        return;
    }
    int total_xp = 0;
    int total_bonus_xp = 0;
    const multi_player_stats* stats = multi_game_stats_get_player_stats(Local_game_data_handles[3] == 0xFFFFFFFFu ? 0xFFFFFFFFu : Local_game_data_handles[3] & 0xFFFF);
    if (stats != nullptr)
    {
        total_bonus_xp = stats->bonus_xp;
        total_xp = stats->xp;
    }
    // Original behaviour kept: the level is computed from the profile's experience (My_xp), which the match's
    // experience joins only at the match's end (multi_game_xp_match_shutdown).
    const unsigned char level = multi_game_xp_calculate_level(My_xp);
    bool leveled_up = false;
    if (level > My_level)
    {
        player_leveled_up(multi_game_data_handle, level);
        leveled_up = true;
    }
    hud_ui_multiplayer_local_player_triggered_xp_event(info.display_name, xp_change * repititions, info.bonus, total_xp, total_bonus_xp, leveled_up);
}

// 0x00967510 (:422): by level, then by type. Original behaviour kept: two entries of the same level and type compare
// as "greater" both ways (the qsort order of equal entries is unspecified).
static int xp_unlock_sort(const void* left, const void* right)
{
    const multi_xp_unlock* l = (const multi_xp_unlock*)left;
    const multi_xp_unlock* r = (const multi_xp_unlock*)right;
    if (l->level_required < r->level_required)
    {
        return -1;
    }
    if (r->level_required < l->level_required)
    {
        return 1;
    }
    return r->unlock_type <= l->unlock_type ? 1 : -1;
}

// 0x00987360 (:443): the "mp_alter_xp" console command's function (not registered in rfg.exe: "unused"): sets the
// profile's experience and level.
void unused_dcf_mp_alter_xp()
{
    // Magic number: 8 is the integer argument type of console_get_arg.
    if (Console_command == 0 || !console_get_arg(8, 0))
    {
        console_printf("Usage: mp_alter_xp <new_xp_amount>");
        return;
    }
    My_xp = Console_arg_int;
    if (My_xp < 0)
    {
        My_xp = 0;
    }
    else if (My_xp > Xp_max_total)
    {
        My_xp = Xp_max_total;
    }
    My_level = level_of(My_xp);
    player_profile_data* profile = xp_profile("mp_alter_xp");
    if (profile == nullptr)
    {
        console_printf("You are now level %d.", (int)My_level);
        return;
    }
    profile->mp_xp = My_xp;
    player_profile_save("mp_alter_xp", Player_controller_index != 0xFFFFFFFFu ? (int)Player_controller_index : 0);
    console_printf("You are now level %d and it has been saved to your profile.", (int)My_level);
}

// 0x009A3570 (:475): the profile's experience and level; the events of mp_xp.xtbl; the unlocks (bonus events,
// playlists, weapons, characters, badge icons, emblems and color schemes above level 1), sorted by level.
void multi_game_xp_init(mempool_base& pool)
{
    My_xp = 0;
    My_bonus_xp = 0;
    const player_profile_data* profile = xp_profile("multi_game_xp_init");
    if (profile != nullptr)
    {
        My_xp = profile->mp_xp;
        My_bonus_xp = profile->mp_bonus_xp;
    }
    My_level = level_of(My_xp);
    // Magic number: 6 KB of event names and descriptions.
    Multi_xp_string_pool.init(pool.alloc(0x1800, 4), 0x1800, nullptr, 0, 0);
    for (int i = 0; i < NUM_MULTI_XP_EVENTS; i++)
    {
        Multi_xp_events[i].valid = false;
    }
    xml_element* root = xml_table_parse("mp_xp.xtbl");
    if (root == nullptr)
    {
        return;
    }
    xml_element* event_root = xml_table_find_opt(root, "event");
    while (event_root != nullptr)
    {
        const char* name = xml_table_get_req_string_ref(event_root, "Name");
        // Original behaviour kept: without a name the loop does not advance (rfg.exe spins on the entry).
        if (name == nullptr)
        {
            continue;
        }
        const checksum_stri name_crc(name, 0);
        int i = 0;
        while (i < NUM_MULTI_XP_EVENTS && Multi_xp_events[i].name_crc.get_crc() != name_crc.get_crc())
        {
            i++;
        }
        if (i < NUM_MULTI_XP_EVENTS)
        {
            multi_xp_event& info = Multi_xp_events[i];
            info.display_name = Multi_xp_string_pool.add(xml_table_get_req_string_ref(event_root, "display_name"));
            info.description = Multi_xp_string_pool.add(xml_table_get_req_string_ref(event_root, "event_description"));
            xml_table_get_req_int(info.value, event_root, "value");
            xml_table_get_req_int(info.repeat_delay_ms, event_root, "repeat_delay_ms");
            xml_table_get_req_bool(info.player_event, event_root, "player_event");
            xml_table_get_req_bool(info.team_event, event_root, "team_event");
            xml_table_get_req_bool(info.server_authoritative, event_root, "server_authoritative");
            xml_table_get_req_bool(info.bagman_locked, event_root, "bagman_locked");
            info.bonus = false;
            info.minimum_level = 1;
            xml_element* bonus = xml_table_find_opt(event_root, "bonus");
            if (bonus != nullptr)
            {
                info.bonus = true;
                xml_table_get_req_int(info.minimum_level, bonus, "minimum_level");
            }
            info.has_parameter = xml_table_get_opt_float(info.parameter, event_root, "parameter");
            info.valid = true;
        }
        else
        {
            // The message of an assert retail compiles out: the buffer is never shown.
            char message[1024];
            sprintf(message, "XP] Event \"%s\" not found. Make sure the event is named correctly in the table file.", name);
        }
        event_root = xml_table_find_next(root, event_root, "event");
    }
    xml_table_close(root);

    Num_xp_unlocks = 0;
    Multi_xp_unlocks = (multi_xp_unlock*)pool.alloc(Xp_max_unlocks * sizeof(multi_xp_unlock), 4);
    // The bonus events that unlock above level 1 (rfg.exe unrolls this loop six events at a time; a full array ends
    // it).
    for (int i = 0; i < NUM_MULTI_XP_EVENTS; i++)
    {
        if (Multi_xp_events[i].minimum_level > 1)
        {
            if (Num_xp_unlocks >= Xp_max_unlocks)
            {
                break;
            }
            multi_xp_unlock& unlock = Multi_xp_unlocks[Num_xp_unlocks++];
            unlock.unlock_type = MXUT_BONUS_XP_EVENT;
            unlock.level_required = Multi_xp_events[i].minimum_level;
            unlock.data.xp_event = (multi_xp_events)i;
        }
    }
    // The playlists (a ring).
    if (Playlists != nullptr)
    {
        const multi_game_playlist* playlist = Playlists;
        while (true)
        {
            if (playlist->required_xp_level > 1)
            {
                if (Num_xp_unlocks >= Xp_max_unlocks)
                {
                    break;
                }
                multi_xp_unlock& unlock = Multi_xp_unlocks[Num_xp_unlocks++];
                unlock.unlock_type = MXUT_PLAYLIST;
                unlock.level_required = playlist->required_xp_level;
                unlock.data.playlist_id = playlist->playlist_id.get_crc();
            }
            playlist = playlist->next;
            if (playlist == Playlists || playlist == nullptr)
            {
                break;
            }
        }
    }
    // The unlockable weapons (every one, also those of level 1).
    Mp_unlockable_weapon_pool.thread_access.enter();
    for (mp_character_unlockable_weapon* weapon = Mp_unlockable_weapon_pool.get_first(0); weapon != nullptr;
         weapon = Mp_unlockable_weapon_pool.get_next(weapon))
    {
        if (Num_xp_unlocks >= Xp_max_unlocks)
        {
            break;
        }
        multi_xp_unlock& unlock = Multi_xp_unlocks[Num_xp_unlocks++];
        unlock.unlock_type = MXUT_WEAPON;
        unlock.level_required = weapon->level_needed;
        unlock.data.weapon_unique_id = weapon->unique_weapon_id;
    }
    Mp_unlockable_weapon_pool.thread_access.leave();
    // The characters. Magic number: room for 128 infos (the stack buffer rfg.exe shares with the badges).
    static const int Max_infos = 0x80;
    int count = 0;
    multi_xp_character_unlock_info character_infos[Max_infos];
    multi_game_character_get_xp_events(count, character_infos, Max_infos);
    for (int i = 0; i < count; i++)
    {
        if (Num_xp_unlocks >= Xp_max_unlocks)
        {
            break;
        }
        if (character_infos[i].unlock_level > 1)
        {
            multi_xp_unlock& unlock = Multi_xp_unlocks[Num_xp_unlocks++];
            unlock.level_required = character_infos[i].unlock_level;
            unlock.unlock_type = MXUT_CHARACTER;
            unlock.data.character_id = character_infos[i].character_id;
        }
    }
    // The badge parts.
    static const multi_xp_unlock_types badge_types[3] = {MXUT_BADGE_ICON, MXUT_BADGE_EMBLEM, MXUT_BADGE_COLOR_SCHEME};
    int num_unlocks = Num_xp_unlocks;
    for (int t = 0; t < 3; t++)
    {
        xp_badge_unlock_info badge_infos[Max_infos];
        multi_game_badge_get_xp_unlocks(count, badge_infos, badge_types[t], Max_infos);
        for (int i = 0; i < count; i++, num_unlocks++)
        {
            if (num_unlocks >= Xp_max_unlocks)
            {
                break;
            }
            // Original bug kept: a part of level 1 still takes a slot (left as it was in the array).
            if (badge_infos[i].unlock_level > 1)
            {
                Multi_xp_unlocks[num_unlocks].level_required = badge_infos[i].unlock_level;
                Multi_xp_unlocks[num_unlocks].unlock_type = badge_types[t];
                Multi_xp_unlocks[num_unlocks].data.badge_id = badge_infos[i].unlock_info_index;
            }
        }
    }
    Num_xp_unlocks = num_unlocks;
    qsort(Multi_xp_unlocks, (size_t)Num_xp_unlocks, sizeof(multi_xp_unlock), xp_unlock_sort);
}

// 0x00967540 (:667)
void multi_game_xp_shutdown()
{
}

// 0x00967550 (:671): every event's repeat timers (16 players, 2 teams) start unset.
void multi_game_xp_match_init()
{
    for (int i = 0; i < NUM_MULTI_XP_EVENTS; i++)
    {
        for (timestamp& timer : Multi_xp_events[i].individual_repeat_delay_timers)
        {
            timer.invalidate();
        }
        Multi_xp_events[i].team_repeat_delay_timers[0].invalidate();
        Multi_xp_events[i].team_repeat_delay_timers[1].invalidate();
    }
}

// 0x009E29A0 (:685): the local player's match experience joins the profile's (halved, without the bonus, when the
// player left by choice); a level-up is announced; the profile keeps the totals unless cheats are on. (The LTCG note
// of rfg.pdb's ECX is the function's stack slot push, not an argument.)
void multi_game_xp_match_shutdown()
{
    if (Mp_game_data == nullptr || Local_game_data_handles[3] == 0xFFFFFFFFu)
    {
        return;
    }
    multi_game_data* data = game_data_of(Local_game_data_handles[3]);
    if (!data->in_use)
    {
        data->linger_time.valid();
        return;
    }
    if (data->handle == 0xFFFFFFFFu)
    {
        return;
    }
    // Magic number: 17 stats slots (16 players and the spectator slot).
    const unsigned int index = data->handle & 0xFFFF;
    if (index > 16 || Mp_player_stats == nullptr)
    {
        return;
    }
    multi_player_stats* stats = &Mp_player_stats[index];
    if (Multi_left_game_by_choice)
    {
        stats->xp >>= 1;
        stats->bonus_xp = 0;
    }
    My_xp = stats->xp + stats->bonus_xp + My_xp;
    My_bonus_xp = My_bonus_xp + stats->bonus_xp;
    if (My_xp < 0)
    {
        My_xp = 0;
    }
    else if (My_xp > Xp_max_total)
    {
        My_xp = Xp_max_total;
    }
    if (My_bonus_xp < 0)
    {
        My_bonus_xp = 0;
    }
    else if (My_bonus_xp > Xp_max_total)
    {
        My_bonus_xp = Xp_max_total;
    }
    const unsigned char level = level_of(My_xp);
    if (level > My_level)
    {
        player_leveled_up(data->handle, level);
    }
    player_profile_data* profile = xp_profile("multi_game_xp_match_shutdown");
    if (profile != nullptr && !cheats_are_enabled())
    {
        profile->mp_xp = My_xp;
        profile->mp_bonus_xp = My_bonus_xp;
    }
}

// 0x009675A0 (:722)
float multi_game_xp_get_event_parameter(multi_xp_events event)
{
    if ((unsigned int)event < NUM_MULTI_XP_EVENTS && Multi_xp_events[event].has_parameter && Multi_xp_events[event].valid)
    {
        return Multi_xp_events[event].parameter;
    }
    return 0.0f;
}

// 0x009675D0 (:734)
bool multi_game_xp_is_bonus_event(multi_xp_events event)
{
    if ((unsigned int)event < NUM_MULTI_XP_EVENTS && Multi_xp_events[event].valid)
    {
        return Multi_xp_events[event].bonus;
    }
    return false;
}

// 0x009675F0 (:744)
bool multi_game_xp_is_team_event(multi_xp_events event)
{
    if ((unsigned int)event < NUM_MULTI_XP_EVENTS && Multi_xp_events[event].valid)
    {
        return Multi_xp_events[event].team_event;
    }
    return false;
}

// 0x00967610 (:754): null for an event mp_xp.xtbl does not define.
const multi_xp_event* multi_game_xp_get_event_info(multi_xp_events event)
{
    if ((unsigned int)event < NUM_MULTI_XP_EVENTS && Multi_xp_events[event].valid)
    {
        return &Multi_xp_events[event];
    }
    return nullptr;
}

// 0x00967630 (:766)
int multi_game_xp_get_my_xp()
{
    return My_xp;
}

// 0x00967640 (:771)
int multi_game_xp_get_my_bonus_xp()
{
    return My_bonus_xp;
}

// 0x00967650 (:778)
unsigned char multi_game_xp_get_my_level()
{
    return My_level;
}

// 0x00967660 (:783): the levels whose start the experience reached (0 below level 1's, which is 0).
unsigned char multi_game_xp_calculate_level(int xp)
{
    return level_of(xp);
}

// 0x00967680 (:796): 0 at the last level.
int multi_game_xp_til_next_level(int xp)
{
    for (int level = 0; level < Xp_num_levels; level++)
    {
        if (xp < Xp_required_per_level[level])
        {
            // Magic number: '~' (126), the last level, as rfg.exe compares the level byte.
            if ((unsigned char)level == 126)
            {
                return 0;
            }
            return Xp_required_per_level[(unsigned char)level] - xp;
        }
    }
    return 0;
}

// 0x009676B0 (:806): the experience a level starts at. Original behaviour kept: no upper bound (a level above 126
// reads past the table).
int multi_game_xp_level_threshold(int level)
{
    if (level < 1)
    {
        return 0;
    }
    return Xp_required_per_level[level - 1];
}

// 0x009676D0 (:812)
int multi_game_xp_max_level()
{
    return Xp_num_levels;
}

// 0x00987440 (:819)
const multi_xp_unlock* multi_game_xp_get_all_unlocks(int& num_unlocks)
{
    return multi_game_xp_get_unlocks_for_level_range(num_unlocks, 1, Xp_num_levels);
}

// 0x009676E0 (:826): the unlocks up to the local player's level (the array's start).
const multi_xp_unlock* multi_game_xp_get_my_unlocks(int& num_unlocks)
{
    num_unlocks = 0;
    for (int i = 0; i < Num_xp_unlocks; i++)
    {
        if ((int)My_level < Multi_xp_unlocks[i].level_required)
        {
            break;
        }
        num_unlocks++;
    }
    return Multi_xp_unlocks;
}

// 0x00967720 (:845): null when none.
const multi_xp_unlock* multi_game_xp_get_unlocks_at_level(int& num_unlocks, int level)
{
    const multi_xp_unlock* first = nullptr;
    num_unlocks = 0;
    for (int i = 0; i < Num_xp_unlocks; i++)
    {
        const multi_xp_unlock* unlock = &Multi_xp_unlocks[i];
        if (unlock->level_required == level)
        {
            if (first == nullptr)
            {
                first = unlock;
            }
            num_unlocks++;
        }
        else if (level < unlock->level_required)
        {
            break;
        }
    }
    return first;
}

// 0x00967770 (:868): null when none.
const multi_xp_unlock* multi_game_xp_get_unlocks_for_level_range(int& num_unlocks, int level_start, int level_end)
{
    const multi_xp_unlock* first = nullptr;
    num_unlocks = 0;
    for (int i = 0; i < Num_xp_unlocks; i++)
    {
        const multi_xp_unlock* unlock = &Multi_xp_unlocks[i];
        if (level_end < unlock->level_required)
        {
            break;
        }
        if (unlock->level_required >= level_start)
        {
            if (first == nullptr)
            {
                first = unlock;
            }
            num_unlocks++;
        }
    }
    return first;
}

// 0x009677C0 (:889): the profile's sledge when the local player has unlocked it, else none.
int multi_game_xp_get_sledge_from_profile()
{
    const player_profile_data* profile = xp_profile("multi_game_xp_get_sledge_from_profile");
    if (profile == nullptr || profile->sledge_unique_id == Xp_no_sledge)
    {
        return Xp_no_sledge;
    }
    int num_unlocks;
    const multi_xp_unlock* unlocks = multi_game_xp_get_my_unlocks(num_unlocks);
    for (int i = 0; i < num_unlocks; i++)
    {
        if (unlocks[i].unlock_type == MXUT_WEAPON && unlocks[i].data.weapon_unique_id == profile->sledge_unique_id)
        {
            return profile->sledge_unique_id;
        }
    }
    return Xp_no_sledge;
}

// 0x009E2B10 (:914): a team event for every player of the team (once per repeat delay); the server tells the
// clients of the server-authoritative ones.
void multi_game_xp_trigger_event(human_teams team, multi_xp_events event, int repititions, bool from_server)
{
    if ((unsigned int)event >= NUM_MULTI_XP_EVENTS || (unsigned int)team > HUMAN_TEAM_EDF)
    {
        return;
    }
    multi_xp_event& info = Multi_xp_events[event];
    if (!info.valid || !info.team_event || (info.server_authoritative && !(Multiplayer && Server) && !from_server))
    {
        return;
    }
    timestamp& timer = info.team_repeat_delay_timers[team];
    if (timer.valid() && !timer.elapsed())
    {
        return;
    }
    timer.set(info.repeat_delay_ms);
    for (const multi_game_data* data = multi_game_data_begin(NUM_SESSION_TYPES, false, false); data != nullptr;
         data = multi_game_data_next(data, false, false))
    {
        if (data->team == team)
        {
            trigger_event_internal(data->handle, event, repititions, -1, -1);
        }
    }
    if (info.server_authoritative && Multiplayer && Server)
    {
        unsigned char packet[1200];
        packet[0] = PT_XP_EVENT;
        unsigned short packet_size = packet_header_size(PT_XP_EVENT);
        packet[packet_size++] = (unsigned char)event;
        write_u16_be(packet, packet_size, (unsigned short)repititions);
        packet[packet_size++] = (unsigned char)team;
        packet_set_size(packet, packet_size);
        multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, packet_size);
    }
}

// 0x009E2CE0 (:974): a player event (once per repeat delay and player); the server tells the clients of the
// server-authoritative ones.
void multi_game_xp_trigger_event(unsigned int multi_game_data_handle, multi_xp_events event, int repititions, int weapon_id, int backpack_id, bool from_server)
{
    if ((unsigned int)event >= NUM_MULTI_XP_EVENTS || multi_game_data_handle == 0xFFFFFFFFu)
    {
        return;
    }
    multi_xp_event& info = Multi_xp_events[event];
    if (!info.valid || !info.player_event || (info.server_authoritative && !(Multiplayer && Server) && !from_server))
    {
        return;
    }
    // (rfg.exe also compares the player index with -1, which a 16-bit index never is.) Original bug kept: index 16
    // (the spectator slot) is the first team timer.
    timestamp& timer = info.individual_repeat_delay_timers[multi_game_data_handle & 0xFFFF];
    if (timer.valid() && !timer.elapsed())
    {
        return;
    }
    timer.set(info.repeat_delay_ms);
    trigger_event_internal(multi_game_data_handle, event, repititions, weapon_id, backpack_id);
    if (info.server_authoritative && Multiplayer && Server)
    {
        unsigned char packet[1200];
        packet[0] = PT_XP_EVENT;
        unsigned short packet_size = packet_header_size(PT_XP_EVENT);
        packet[packet_size++] = (unsigned char)event;
        write_u16_be(packet, packet_size, (unsigned short)repititions);
        write_u32_be(packet, packet_size, multi_game_data_handle);
        packet[packet_size++] = (unsigned char)weapon_id;
        write_u32_be(packet, packet_size, (unsigned int)backpack_id);
        packet_set_size(packet, packet_size);
        multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, packet_size);
    }
}

// 0x009E2F20 (:1039): PT_XP_EVENT on a client: the server's event, as from the server.
void multi_game_xp_process_event(unsigned char* packet_data, const sender& from)
{
    if (!Multiplayer || Server)
    {
        return;
    }
    const multi_xp_events event = (multi_xp_events)packet_data[0];
    if ((unsigned int)event >= NUM_MULTI_XP_EVENTS || !Multi_xp_events[event].valid)
    {
        return;
    }
    int offset = 1;
    const int repititions = read_u16_be(packet_data, offset);
    if (Multi_xp_events[event].team_event)
    {
        multi_game_xp_trigger_event((human_teams)packet_data[3], event, repititions, true);
        return;
    }
    const unsigned int handle = read_u32_be(packet_data, offset);
    const int weapon_id = packet_data[offset++];
    const int backpack_id = (int)read_u32_be(packet_data, offset);
    multi_game_xp_trigger_event(handle, event, repititions, weapon_id, backpack_id, true);
}

// 0x009E3060 (:1110): PT_LEVELED_UP: on the server a client's own level (its connection's player), on a client
// another player's; ignored with modified tables.
void multi_game_xp_process_leveled_up(unsigned char* packet_data, const sender& from)
{
    unsigned int handle;
    int offset = 0;
    if (Multiplayer && Server)
    {
        if (from.connection == nullptr)
        {
            return;
        }
        handle = from.connection->game_data;
    }
    else
    {
        handle = read_u32_be(packet_data, offset);
    }
    if (handle != 0xFFFFFFFFu && !Modified_table_data)
    {
        player_leveled_up(handle, packet_data[offset]);
    }
}

// 0x00967830 (:1143): the first weapon among the local player's unlocks, else none.
int multi_game_xp_get_first_weapon_field()
{
    int num_mine = 0;
    while (num_mine < Num_xp_unlocks && (int)My_level >= Multi_xp_unlocks[num_mine].level_required)
    {
        num_mine++;
    }
    for (int i = 0; i < num_mine; i++)
    {
        if (Multi_xp_unlocks[i].unlock_type == MXUT_WEAPON)
        {
            return Multi_xp_unlocks[i].data.weapon_unique_id;
        }
    }
    return Xp_no_sledge;
}

// 0x00967890 (:1157): locked until the local player's level reaches it; a weapon that is no unlock is locked.
bool multi_game_xp_weapon_is_locked(int sledge_unique_id)
{
    for (int i = 0; i < Num_xp_unlocks; i++)
    {
        if (Multi_xp_unlocks[i].unlock_type == MXUT_WEAPON && Multi_xp_unlocks[i].data.weapon_unique_id == sledge_unique_id)
        {
            return (int)My_level < Multi_xp_unlocks[i].level_required;
        }
    }
    return true;
}

// Port helper: the unlock index of a weapon, -1 when it is none.
static int weapon_unlock_index(int sledge_unique_id)
{
    for (int i = 0; i < Num_xp_unlocks; i++)
    {
        if (Multi_xp_unlocks[i].unlock_type == MXUT_WEAPON && Multi_xp_unlocks[i].data.weapon_unique_id == sledge_unique_id)
        {
            return i;
        }
    }
    return -1;
}

// 0x009678E0 (:1170): the next weapon unlock after this one (wrapping), locked or not.
void multi_game_xp_increment_weapon_field(int& sledge_unique_id)
{
    const int current = weapon_unlock_index(sledge_unique_id);
    for (int i = 0; i < Num_xp_unlocks; i++)
    {
        const int index = (current + 1 + i) % Num_xp_unlocks;
        if (Multi_xp_unlocks[index].unlock_type == MXUT_WEAPON)
        {
            sledge_unique_id = Multi_xp_unlocks[index].data.weapon_unique_id;
            return;
        }
    }
}

// 0x00967950 (:1191): the weapon unlock before this one (wrapping). Original bug kept: from no weapon (-1) the index
// goes negative after the array's start and reads before it.
void multi_game_xp_decrement_weapon_field(int& sledge_unique_id)
{
    int index = Num_xp_unlocks - 1 + weapon_unlock_index(sledge_unique_id);
    for (int i = 0; i < Num_xp_unlocks; i++, index--)
    {
        if (Multi_xp_unlocks[index % Num_xp_unlocks].unlock_type == MXUT_WEAPON)
        {
            sledge_unique_id = Multi_xp_unlocks[index % Num_xp_unlocks].data.weapon_unique_id;
            return;
        }
    }
}

// 0x009679C0 (:1212): 0 outside levels 1-126.
int multi_game_xp_get_xp_for_level(int level)
{
    if ((unsigned int)(level - 1) < (unsigned int)Xp_num_levels)
    {
        return Xp_required_per_level[level - 1];
    }
    return 0;
}
