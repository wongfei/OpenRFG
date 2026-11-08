// Original: volition/rfg/code/multi/multi_game/multi_game_stats.cpp
//
// Multiplayer stats: per player (16 slots, indexed by the game data index) score, experience, kills and deaths, lives
// and damage, per player and weapon sub stats, team stats, and the tracked weapons, backpacks and levels whose
// profile stats count multiplayer play (mp_stats_*_tracking.xtbl).
// Partial (multiplayer.md MP4, MP9): the set-up, the match and round resets, the lag and playlist flags, the getters,
// recording shots, hits, kills, equip times and round wins, the scores, the stat packets (PT_PLAYER_STATS,
// PT_TEAM_STATS), the endgame sync and finalization, the round winners; stub_impl.md S6 added the experience, the score
// packets, the player sorts and the scoreboard counting (no callers in rfg.exe), so the file is whole.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/bit_util.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/string_comp.h"
#include "unit4/gamesession/localgamesession.hpp"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"

#include <stdlib.h>
#include <string.h>

// ---- file-static globals
// multi_team_stats Mp_team_stats[2] (0x02FE82F0) is used here too; placed in multi_game_mode_bagman.cpp.
// multi_backpack_info* Backpack_info[16] (0x02FE8D18) is used here too; placed in multi_game_backpack_rack.cpp.
// multi_player_stats* Mp_player_stats (0x02FEB720) is used here too; placed in multi_game_mode_bagman.cpp.
// multi_weapon_stats* Mp_weapon_stats (0x02FEB724) is used here too; placed in multi_game.cpp.
// const multi_player_stats* My_player_stats (0x02FEB728) is used here too; placed in multi_game_mode_common.cpp.
static int Vehicle_weapon_handle; // 0x02FEB72C
static int No_weapon_handle; // 0x02FEB730
static int Num_tracked_weapons; // 0x02FEB734
static int No_backpack_handle; // 0x02FEB738
static int Num_tracked_backpacks; // 0x02FEB73C
// tracked_weapon_reference* Stat_tracked_weapons (0x02FEB740) is used here too; placed in multi_game.cpp.
static int Num_stat_tracked_weapons; // 0x02FEB744
// tracked_backpack_reference* Stat_tracked_backpacks (0x02FEB748) is used here too; placed in multi_game.cpp.
static int Num_stat_tracked_backpacks; // 0x02FEB74C
// tracked_level_reference* Stat_tracked_levels (0x02FEB750) is used here too; placed in multi_game_mode_common.cpp.
// int Num_stat_tracked_levels (0x02FEB754) is used here too; placed in multi_game_mode_common.cpp.

// Magic numbers: 16 player slots (the index range checks allow 16 too, as in rfg.exe); the backpack handles after the
// 16 backpacks are "no backpack" (17) and the count (18).
static const int Num_stats_players = 16;

// ---- internal functions

// 0x00963EE0 (:141): the tracked weapons with all five of their profile stats
static void parse_weapon_tracking_table_file()
{
    xml_element* root = xml_table_parse("mp_stats_weapon_tracking.xtbl");
    Stat_tracked_weapons = (tracked_weapon_reference*)MP_allocators_mempool.alloc(xml_table_count(root, "stat_tracked_weapons") * sizeof(tracked_weapon_reference), 0);
    Num_stat_tracked_weapons = 0;
    for (xml_element* entry = xml_table_find_opt(root, "stat_tracked_weapons"); entry != nullptr; entry = xml_table_find_next(root, entry, "stat_tracked_weapons"))
    {
        tracked_weapon_reference& ref = Stat_tracked_weapons[Num_stat_tracked_weapons];
        ref.w_info = nullptr;
        ref.total_xp_stat = STATS_INVALID;
        ref.time_equipped_stat = STATS_INVALID;
        ref.kills_stat = STATS_INVALID;
        ref.deaths_stat = STATS_INVALID;
        ref.w_info = weapons_get_weapon_info(xml_table_get_req_string_ref(entry, "weapon"));
        if (ref.w_info == nullptr)
        {
            continue;
        }
        ref.total_xp_stat = stats_get_by_name(xml_table_get_req_string_ref(entry, "xp_stat"));
        if (ref.total_xp_stat == STATS_INVALID)
        {
            continue;
        }
        ref.time_equipped_stat = stats_get_by_name(xml_table_get_req_string_ref(entry, "time_equipped_stat"));
        if (ref.time_equipped_stat == STATS_INVALID)
        {
            continue;
        }
        ref.kills_stat = stats_get_by_name(xml_table_get_req_string_ref(entry, "kills_stat"));
        if (ref.kills_stat == STATS_INVALID)
        {
            continue;
        }
        ref.deaths_stat = stats_get_by_name(xml_table_get_req_string_ref(entry, "deaths_stat"));
        if (ref.deaths_stat == STATS_INVALID)
        {
            continue;
        }
        ref.one_life_kills_in_a_row_stat = stats_get_by_name(xml_table_get_req_string_ref(entry, "one_life_kills_in_a_row_stat"));
        if (ref.one_life_kills_in_a_row_stat != STATS_INVALID)
        {
            Num_stat_tracked_weapons++;
        }
    }
    xml_table_close(root);
}

// 0x00984C60 (:190): the tracked backpacks with all four of their profile stats
static void parse_backpack_tracking_table_file()
{
    xml_element* root = xml_table_parse("mp_stats_backpack_tracking.xtbl");
    Stat_tracked_backpacks = (tracked_backpack_reference*)MP_allocators_mempool.alloc(xml_table_count(root, "stat_tracked_backpacks") * sizeof(tracked_backpack_reference), 0);
    Num_stat_tracked_backpacks = 0;
    for (xml_element* entry = xml_table_find_opt(root, "stat_tracked_backpacks"); entry != nullptr; entry = xml_table_find_next(root, entry, "stat_tracked_backpacks"))
    {
        tracked_backpack_reference& ref = Stat_tracked_backpacks[Num_stat_tracked_backpacks];
        ref.b_info = nullptr;
        ref.total_xp_stat = STATS_INVALID;
        ref.time_equipped_stat = STATS_INVALID;
        ref.kills_stat = STATS_INVALID;
        ref.deaths_stat = STATS_INVALID;
        const char* name = xml_table_get_req_string_ref(entry, "backpack");
        const int backpack_id = name != nullptr ? multi_game_backpack_get_backpack_id(name) : -1;
        ref.b_info = (unsigned int)backpack_id < 16 ? Backpack_info[backpack_id] : nullptr;
        if (ref.b_info == nullptr)
        {
            continue;
        }
        ref.total_xp_stat = stats_get_by_name(xml_table_get_req_string_ref(entry, "xp_stat"));
        if (ref.total_xp_stat == STATS_INVALID)
        {
            continue;
        }
        ref.time_equipped_stat = stats_get_by_name(xml_table_get_req_string_ref(entry, "time_equipped_stat"));
        if (ref.time_equipped_stat == STATS_INVALID)
        {
            continue;
        }
        ref.kills_stat = stats_get_by_name(xml_table_get_req_string_ref(entry, "kills_stat"));
        if (ref.kills_stat == STATS_INVALID)
        {
            continue;
        }
        ref.deaths_stat = stats_get_by_name(xml_table_get_req_string_ref(entry, "deaths_stat"));
        if (ref.deaths_stat != STATS_INVALID)
        {
            Num_stat_tracked_backpacks++;
        }
    }
    xml_table_close(root);
}

// 0x009A26E0 (:236): the tracked levels (by display name) with their games played stat
static void parse_level_tracking_table_file()
{
    xml_element* root = xml_table_parse("mp_stats_level_tracking.xtbl");
    Stat_tracked_levels = (tracked_level_reference*)MP_allocators_mempool.alloc(xml_table_count(root, "stat_tracked_levels") * sizeof(tracked_level_reference), 0);
    xml_table_find_opt(root, "stat_tracked_levels");
    Num_stat_tracked_levels = 0;
    for (xml_element* entry = xml_table_find_opt(root, "stat_tracked_levels"); entry != nullptr; entry = xml_table_find_next(root, entry, "stat_tracked_levels"))
    {
        tracked_level_reference& ref = Stat_tracked_levels[Num_stat_tracked_levels];
        ref.l_info = nullptr;
        ref.games_played_stat = STATS_INVALID;
        ref.l_info = multi_game_settings_find_level_by_name(xml_table_get_req_string_ref(entry, "level"));
        if (ref.l_info == nullptr)
        {
            continue;
        }
        ref.games_played_stat = stats_get_by_name(xml_table_get_req_string_ref(entry, "games_played_stat"));
        if (ref.games_played_stat != STATS_INVALID)
        {
            Num_stat_tracked_levels++;
        }
    }
    xml_table_close(root);
}

// The player stats slot of a game data handle (-1: none). Original (every user): the index is not range checked here;
// the recorders check it against 16.
static unsigned int stats_index_of(unsigned int multi_game_data_handle)
{
    return multi_game_data_handle == 0xFFFFFFFFu ? 0xFFFFFFFFu : multi_game_data_handle & 0xFFFF;
}

// Port helper: rfg.exe inlines the 32-bit swap of a float's bits on read (an integer swap, no x87 load).
static float read_f32_be(const unsigned char* packet_data, int& packet_offset)
{
    const unsigned int bits = read_u32_be(packet_data, packet_offset);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

// Port helper: the writes go through PACKET_SWAP_FLOAT (the value is in memory in packet order afterwards).
static void write_f32_swapped(unsigned char* packet_data, unsigned short& packet_size, float value)
{
    const float swapped = PACKET_SWAP_FLOAT(value);
    memcpy(packet_data + packet_size, &swapped, sizeof(swapped));
    packet_size = (unsigned short)(packet_size + 4);
}

// 0x009640A0 (:272): the weapon's handle; without one, the vehicle (damage by car) or "no weapon"
// LTCG: w_info in EAX.
static int mgs_get_weapon_index(const weapon_info* w_info, damaged_by_types damage_type)
{
    if (w_info != nullptr)
    {
        return (int)weapons_get_weapon_info_handle(w_info);
    }
    return damage_type == DBT_CAR ? Vehicle_weapon_handle : No_weapon_handle;
}

// 0x009640C0 (:283)
static int mgs_get_backpack_index(const multi_backpack_info* b_info)
{
    if (b_info != nullptr)
    {
        return b_info->unique_id;
    }
    return No_backpack_handle;
}

// 0x009640D0 (:291)
// LTCG: attacker_index in EAX, weapon_index in ECX.
static void mgs_record_shot(int attacker_index, int weapon_index)
{
    if ((unsigned int)attacker_index < (unsigned int)Num_stats_players)
    {
        Mp_player_stats[attacker_index].weapons[weapon_index].shots_fired++;
    }
}

// 0x00964100 (:301)
// LTCG: victim_index in EDX, weapon_index in ECX.
static void mgs_record_hit(int attacker_index, int victim_index, int weapon_index)
{
    if ((unsigned int)attacker_index < (unsigned int)Num_stats_players && victim_index >= 0)
    {
        Mp_player_stats[attacker_index].weapons[weapon_index].damage[victim_index].hits++;
    }
}

// 0x00984E10 (:313): the kill per weapon and victim, the attacker's kills (or team kills, or suicides) and streaks, the
// victim's death; a death ends the victim's life streaks
// LTCG: attacker_index in EDX.
static void mgs_record_kill(int attacker_index, human_teams attacker_team, int victim_index, human_teams victim_team, int weapon_index)
{
    if ((unsigned int)attacker_index >= (unsigned int)Num_stats_players || (unsigned int)victim_index >= (unsigned int)Num_stats_players)
    {
        return;
    }
    multi_player_stats& attacker = Mp_player_stats[attacker_index];
    multi_player_stats& victim = Mp_player_stats[victim_index];
    attacker.weapons[weapon_index].damage[victim_index].kills++;
    if (attacker_index == victim_index)
    {
        attacker.suicides++;
        attacker.round_suicides++;
        attacker.suicides_in_a_row++;
    }
    else
    {
        if (human_teams_are_enemies(attacker_team, victim_team))
        {
            attacker.kills++;
            attacker.round_kills++;
            attacker.kills_in_a_row++;
            attacker.deaths_in_a_row = 0;
            attacker.team_kills_in_a_row = 0;
            attacker.suicides_in_a_row = 0;
        }
        else
        {
            attacker.team_kills++;
            attacker.round_team_kills++;
            attacker.team_kills_in_a_row++;
            attacker.kills_in_a_row = 0;
            attacker.deaths_in_a_row = 0;
            attacker.suicides_in_a_row = 0;
        }
        // The kills in a row with one weapon (0xFF: no last weapon).
        multi_weapon_sub_stats& weapon = attacker.weapons[weapon_index];
        if (attacker.last_kill_weapon == weapon_index)
        {
            weapon.kills_in_a_row++;
        }
        else
        {
            if (attacker.last_kill_weapon != 0xFF)
            {
                attacker.weapons[attacker.last_kill_weapon].kills_in_a_row = 0;
            }
            weapon.kills_in_a_row = 1;
            attacker.last_kill_weapon = weapon_index;
        }
        if (weapon.most_kills_in_a_row < weapon.kills_in_a_row)
        {
            weapon.most_kills_in_a_row = weapon.kills_in_a_row;
        }
        weapon.kills_in_a_life++;
        if (weapon.most_kills_in_a_life < weapon.kills_in_a_life)
        {
            weapon.most_kills_in_a_life = weapon.kills_in_a_life;
        }
        victim.suicides_in_a_row = 0;
    }
    victim.deaths++;
    victim.round_deaths++;
    victim.deaths_in_a_row++;
    victim.xp_in_a_life = 0;
    victim.bonuses_in_a_life = 0;
    victim.kills_in_a_row = 0;
    victim.last_kill_weapon = 0xFF;
    victim.unique_bonus_flags[0] = 0;
    victim.unique_bonus_flags[1] = 0;
    for (int i = 0; i < Num_tracked_weapons; i++)
    {
        victim.weapons[i].kills_in_a_life = 0;
        victim.weapons[i].kills_in_a_row = 0;
    }
    for (int i = 0; i < Num_tracked_backpacks; i++)
    {
        victim.backpacks[i].xp_in_a_life = 0;
    }
    if (victim.most_deaths_in_a_row < victim.deaths_in_a_row)
    {
        victim.most_deaths_in_a_row = victim.deaths_in_a_row;
    }
    if (attacker.most_kills_in_a_row < attacker.kills_in_a_row)
    {
        attacker.most_kills_in_a_row = attacker.kills_in_a_row;
    }
    if (attacker.most_suicides_in_a_row < attacker.suicides_in_a_row)
    {
        attacker.most_suicides_in_a_row = attacker.suicides_in_a_row;
    }
    if (attacker.most_team_kills_in_a_row < attacker.team_kills_in_a_row)
    {
        attacker.most_team_kills_in_a_row = attacker.team_kills_in_a_row;
    }
}

// 0x00964130 (:387)
// LTCG: player_index in EAX, weapon_index in ECX, time_s in XMM0.
static void mgs_record_weapon_time(int player_index, int weapon_index, float time_s)
{
    if ((unsigned int)player_index < (unsigned int)Num_stats_players)
    {
        Mp_player_stats[player_index].weapons[weapon_index].time_equipped += time_s;
    }
}

// 0x00964170 (:397)
// LTCG: player_index in EAX, time_s in XMM1.
static void mgs_record_backpack_time(int player_index, int backpack_index, float time_s)
{
    if ((unsigned int)player_index < (unsigned int)Num_stats_players)
    {
        Mp_player_stats[player_index].backpacks[backpack_index].time_equipped += time_s;
    }
}

// 0x00985050 (:415): a player's stats, or one weapon's sub stats (weapon_stat), after the final byte and the handle.
// The server sends everything (and the kills per victim); a client sends what only it knows (the playlist flag and its
// bonus experience, streak and favourites). The mode's own stats follow (the server's).
// LTCG: packet_data in EDI, packet_size in ESI, final_stats in CL.
static void add_player_stat_data_to_packet(unsigned char* packet_data, unsigned short& packet_size, const multi_weapon_sub_stats* weapon_stat, unsigned int game_data_handle,
                                           unsigned char weapon_info_handle, bool final_stats)
{
    // Original: index 16 passes the range check (Mp_player_stats has 16 entries).
    if (game_data_handle == 0xFFFFFFFFu || (game_data_handle & 0xFFFF) > (unsigned int)Num_stats_players)
    {
        return;
    }
    const multi_player_stats& stats = Mp_player_stats[game_data_handle & 0xFFFF];
    const bool server = Multiplayer && Server;
    packet_data[packet_size++] = final_stats ? 1 : 0;
    write_u32_be(packet_data, packet_size, game_data_handle);
    if (weapon_stat != nullptr)
    {
        packet_data[packet_size++] = weapon_info_handle;
        write_u32_be(packet_data, packet_size, (unsigned int)weapon_stat->shots_fired);
        write_f32_swapped(packet_data, packet_size, weapon_stat->time_equipped);
        for (int i = 0; i < 16; i++)
        {
            write_u32_be(packet_data, packet_size, (unsigned int)weapon_stat->damage[i].hits);
            if (server)
            {
                write_u32_be(packet_data, packet_size, (unsigned int)weapon_stat->damage[i].kills);
            }
        }
        return;
    }
    // 0xFF: the player's own stats
    packet_data[packet_size++] = 0xFF;
    if (Multiplayer)
    {
        if (Server)
        {
            const int values[12] = {stats.kills, stats.deaths, stats.suicides, stats.team_kills, stats.most_kills_in_a_row, stats.most_deaths_in_a_row,
                                    stats.most_suicides_in_a_row, stats.most_team_kills_in_a_row, stats.kills_in_a_row, stats.deaths_in_a_row,
                                    stats.suicides_in_a_row, stats.team_kills_in_a_row};
            for (int value : values)
            {
                write_u32_be(packet_data, packet_size, (unsigned int)value);
            }
            write_u16_be(packet_data, packet_size, stats.score);
            write_u16_be(packet_data, packet_size, stats.xp);
            write_u32_be(packet_data, packet_size, (unsigned int)stats.round_wins);
            write_u32_be(packet_data, packet_size, (unsigned int)stats.round_kills);
            write_u32_be(packet_data, packet_size, (unsigned int)stats.round_deaths);
            write_u32_be(packet_data, packet_size, (unsigned int)stats.round_suicides);
            write_u32_be(packet_data, packet_size, (unsigned int)stats.round_team_kills);
            write_u16_be(packet_data, packet_size, stats.round_score);
            write_u16_be(packet_data, packet_size, stats.round_xp);
            write_f32_swapped(packet_data, packet_size, stats.highest_damage);
            write_f32_swapped(packet_data, packet_size, stats.total_damage);
            write_f32_swapped(packet_data, packet_size, stats.total_repair);
            packet_data[packet_size++] = stats.undesired_playlist;
            packet_data[packet_size++] = stats.skill_mismatch;
            packet_data[packet_size++] = stats.bad_lag;
            packet_data[packet_size++] = stats.game_migrated;
        }
        else
        {
            packet_data[packet_size++] = stats.undesired_playlist;
        }
    }
    write_u16_be(packet_data, packet_size, stats.bonus_xp);
    write_u16_be(packet_data, packet_size, stats.round_bonus_xp);
    write_u32_be(packet_data, packet_size, (unsigned int)stats.win_streak);
    packet_data[packet_size++] = (unsigned char)stats.most_frequent_bonus_xp_event;
    write_u32_be(packet_data, packet_size, stats.favorite_weapon);
    write_u32_be(packet_data, packet_size, (unsigned int)stats.favorite_backpack);
    multi_gameplay_mode_append_mode_specific_stats(packet_data, packet_size, game_data_handle);
}

// 0x009641B0 (:496): the reverse of add_player_stat_data_to_packet (a client reads what the server sends, the server
// what a client sends); the final byte marks the stats (or the weapon's) as finalized
// LTCG: packet_data in ECX.
static void read_player_stat_data_from_packet(unsigned char* packet_data, int& packet_offset)
{
    const bool final_stats = packet_data[packet_offset++] != 0;
    const unsigned int handle = read_u32_be(packet_data, packet_offset);
    // Original: an invalid handle reads into a stack copy (uninitialized there; zeroed in the port); a valid one is not
    // range checked.
    multi_player_stats scratch = {};
    multi_player_stats* stats = handle != 0xFFFFFFFFu ? &Mp_player_stats[handle & 0xFFFF] : &scratch;
    const bool client = Multiplayer && !Server;
    const unsigned char weapon_info_handle = packet_data[packet_offset++];
    if (weapon_info_handle != 0xFF)
    {
        multi_weapon_sub_stats& weapon = stats->weapons[weapon_info_handle];
        weapon.shots_fired = (int)read_u32_be(packet_data, packet_offset);
        weapon.time_equipped = read_f32_be(packet_data, packet_offset);
        for (int i = 0; i < 16; i++)
        {
            weapon.damage[i].hits = (int)read_u32_be(packet_data, packet_offset);
            if (client)
            {
                weapon.damage[i].kills = (int)read_u32_be(packet_data, packet_offset);
            }
        }
        weapon.finalized = final_stats;
        return;
    }
    if (Multiplayer)
    {
        if (!Server)
        {
            int* values[12] = {&stats->kills, &stats->deaths, &stats->suicides, &stats->team_kills, &stats->most_kills_in_a_row, &stats->most_deaths_in_a_row,
                               &stats->most_suicides_in_a_row, &stats->most_team_kills_in_a_row, &stats->kills_in_a_row, &stats->deaths_in_a_row,
                               &stats->suicides_in_a_row, &stats->team_kills_in_a_row};
            for (int* value : values)
            {
                *value = (int)read_u32_be(packet_data, packet_offset);
            }
            stats->score = read_u16_be(packet_data, packet_offset);
            stats->xp = read_u16_be(packet_data, packet_offset);
            stats->round_wins = (int)read_u32_be(packet_data, packet_offset);
            stats->round_kills = (int)read_u32_be(packet_data, packet_offset);
            stats->round_deaths = (int)read_u32_be(packet_data, packet_offset);
            stats->round_suicides = (int)read_u32_be(packet_data, packet_offset);
            stats->round_team_kills = (int)read_u32_be(packet_data, packet_offset);
            stats->round_score = read_u16_be(packet_data, packet_offset);
            stats->round_xp = read_u16_be(packet_data, packet_offset);
            stats->highest_damage = read_f32_be(packet_data, packet_offset);
            stats->total_damage = read_f32_be(packet_data, packet_offset);
            stats->total_repair = read_f32_be(packet_data, packet_offset);
            stats->undesired_playlist = packet_data[packet_offset++];
            stats->skill_mismatch = packet_data[packet_offset++];
            stats->bad_lag = packet_data[packet_offset++];
            stats->game_migrated = packet_data[packet_offset++];
        }
        else
        {
            stats->undesired_playlist = packet_data[packet_offset++];
        }
    }
    stats->bonus_xp = read_u16_be(packet_data, packet_offset);
    stats->round_bonus_xp = read_u16_be(packet_data, packet_offset);
    stats->win_streak = (int)read_u32_be(packet_data, packet_offset);
    stats->most_frequent_bonus_xp_event = (multi_xp_events)packet_data[packet_offset++];
    stats->favorite_weapon = read_u32_be(packet_data, packet_offset);
    stats->favorite_backpack = (int)read_u32_be(packet_data, packet_offset);
    if (Multi_gameplay_mode_funcs[Multi_gameplay_mode].read_mode_specific_stats != nullptr)
    {
        Multi_gameplay_mode_funcs[Multi_gameplay_mode].read_mode_specific_stats(packet_data, packet_offset, handle);
    }
    stats->finalized = final_stats;
}

// 0x00964DE0 (:583): the team, its round score, score and round wins
// LTCG: packet_data in ECX, packet_size in EAX.
static void add_team_stat_data_to_packet(human_teams team, unsigned char* packet_data, unsigned short& packet_size)
{
    packet_data[packet_size++] = (unsigned char)team;
    write_u16_be(packet_data, packet_size, Mp_team_stats[team].round_score);
    write_u16_be(packet_data, packet_size, Mp_team_stats[team].score);
    write_u32_be(packet_data, packet_size, (unsigned int)Mp_team_stats[team].round_wins);
}

// 0x00964E90 (:592)
// LTCG: packet_data in ECX, packet_offset in EAX.
static void read_team_stat_data_from_packet(unsigned char* packet_data, int& packet_offset)
{
    const unsigned int team = packet_data[packet_offset++];
    if (team >= 2)
    {
        return;
    }
    Mp_team_stats[team].round_score = read_u16_be(packet_data, packet_offset);
    Mp_team_stats[team].score = read_u16_be(packet_data, packet_offset);
    Mp_team_stats[team].round_wins = (int)read_u32_be(packet_data, packet_offset);
}

// 0x00985D60 (:605): the multiplayer stand-in weapons count for the weapons they stand for (the rocket hits, the
// charge stick, every sledgehammer variant)
// LTCG: w_info in ESI.
static const weapon_info* get_real_weapon_info(const weapon_info* w_info)
{
    if (w_info == weapons_get_weapon_info(Multi_custom_weapon_names[7]))
    {
        return weapons_get_weapon_info("rpg_launcher");
    }
    if (w_info == weapons_get_weapon_info(Multi_custom_weapon_names[8]))
    {
        return weapons_get_weapon_info("thermobaric_rockets");
    }
    if (w_info == weapons_get_weapon_info(Multi_custom_weapon_names[6]))
    {
        return weapons_get_weapon_info("charge_placer");
    }
    if (w_info != nullptr && w_info->weapon_class == WEAPON_CLASS_SLEDGEHAMMER)
    {
        return weapons_get_weapon_info("sledgehammer");
    }
    return w_info;
}

// 0x00985DE0 (:852): the players of the team (HUMAN_TEAM_NONE: all; never the spectators) in the snapshot, sorted
static void sort_players(human_teams team, unsigned int* players, int& num_players, int (*sort_func)(const void*, const void*))
{
    num_players = 0;
    for (const multi_game_data* data = multi_game_data_begin(ST_GAME_SNAPSHOT, true, false); data != nullptr; data = multi_game_data_next(data, true, false))
    {
        if ((team == HUMAN_TEAM_NONE || data->team == team) && data->team != HUMAN_TEAM_MARAUDER)
        {
            players[num_players++] = data->handle;
        }
    }
    qsort(players, num_players, sizeof(unsigned int), sort_func);
}

// 0x00985FB0 (:947): the most frequent bonus experience event and the favourite weapon and backpack, for the results
// LTCG: multi_game_data_handle in ECX.
static void calculate_end_round_best_stats(unsigned int multi_game_data_handle)
{
    if (multi_game_data_handle == 0xFFFFFFFFu)
    {
        return;
    }
    multi_player_stats& stats = Mp_player_stats[multi_game_data_handle & 0xFFFF];
    int best = (int)0x80000001;
    for (int i = 0; i < 84; i++)
    {
        if (best < (int)stats.xp_event_frequency[i])
        {
            stats.most_frequent_bonus_xp_event = (multi_xp_events)i;
            best = stats.xp_event_frequency[i];
        }
    }
    stats.favorite_weapon = stats.get_favorite_weapon();
    stats.favorite_backpack = stats.get_favorite_backpack();
}

// Port helper: the stats of a sorted handle (index 16 passes the range check, as in rfg.exe).
static const multi_player_stats* sort_stats_of(const void* handle_p)
{
    const unsigned int handle = *(const unsigned int*)handle_p;
    if (handle == 0xFFFFFFFFu || (handle & 0xFFFF) > 16)
    {
        return nullptr;
    }
    return &Mp_player_stats[handle & 0xFFFF];
}

// Port helper: the descending order the four player compares share (first, second, kills; fewer deaths first).
static int sort_compare_desc(int first1, int first2, int second1, int second2, int kills1, int kills2, int deaths1, int deaths2)
{
    if (first1 != first2)
    {
        return first1 > first2 ? -1 : 1;
    }
    if (second1 != second2)
    {
        return second1 > second2 ? -1 : 1;
    }
    if (kills1 != kills2)
    {
        return kills1 > kills2 ? -1 : 1;
    }
    if (deaths1 != deaths2)
    {
        return deaths1 < deaths2 ? -1 : 1;
    }
    return 0;
}

// 0x009860D0 (:970): round score, then round experience, round kills, fewer round deaths
static int qsort_player_round_score_compare(const void* player1p, const void* player2p)
{
    const multi_player_stats* s1 = sort_stats_of(player1p);
    const multi_player_stats* s2 = sort_stats_of(player2p);
    if (s1 == nullptr || s2 == nullptr)
    {
        return 0;
    }
    return sort_compare_desc(s1->round_score, s2->round_score, s1->round_xp, s2->round_xp, s1->round_kills, s2->round_kills, s1->round_deaths, s2->round_deaths);
}

// 0x00986180 (:1010): score, then experience, kills, fewer deaths
static int qsort_player_score_compare(const void* player1p, const void* player2p)
{
    const multi_player_stats* s1 = sort_stats_of(player1p);
    const multi_player_stats* s2 = sort_stats_of(player2p);
    if (s1 == nullptr || s2 == nullptr)
    {
        return 0;
    }
    return sort_compare_desc(s1->score, s2->score, s1->xp, s2->xp, s1->kills, s2->kills, s1->deaths, s2->deaths);
}

// 0x00986230 (:1050): round experience, then round score, round kills, fewer round deaths
static int qsort_player_round_xp_compare(const void* player1p, const void* player2p)
{
    const multi_player_stats* s1 = sort_stats_of(player1p);
    const multi_player_stats* s2 = sort_stats_of(player2p);
    if (s1 == nullptr || s2 == nullptr)
    {
        return 0;
    }
    return sort_compare_desc(s1->round_xp, s2->round_xp, s1->round_score, s2->round_score, s1->round_kills, s2->round_kills, s1->round_deaths, s2->round_deaths);
}

// 0x009862E0 (:1090): experience, then score, kills, fewer deaths
static int qsort_player_xp_compare(const void* player1p, const void* player2p)
{
    const multi_player_stats* s1 = sort_stats_of(player1p);
    const multi_player_stats* s2 = sort_stats_of(player2p);
    if (s1 == nullptr || s2 == nullptr)
    {
        return 0;
    }
    return sort_compare_desc(s1->xp, s2->xp, s1->score, s2->score, s1->kills, s2->kills, s1->deaths, s2->deaths);
}

// 0x00965640 (:1130): the better round score first
static int qsort_team_round_score_compare(const void* team1p, const void* team2p)
{
    const unsigned short score1 = Mp_team_stats[*(const int*)team1p].round_score;
    const unsigned short score2 = Mp_team_stats[*(const int*)team2p].round_score;
    if (score2 < score1)
    {
        return -1;
    }
    return score1 < score2 ? 1 : 0;
}

// ---- external functions

// ---- the scoreboard counting: no callers in rfg.exe. Player indices are range checked, weapon indices are not.
namespace multi_game_stats_counting
{
    // 0x00964F70 (:636)
    int count_kills(int player_index)
    {
        if ((unsigned int)player_index >= 16)
        {
            return 0;
        }
        return Mp_player_stats[player_index].count_kills();
    }

    // 0x00965010 (:651): every player's kills of this one
    int count_deaths(int player_index)
    {
        if ((unsigned int)player_index >= 16)
        {
            return 0;
        }
        int count = 0;
        for (int killer = 0; killer < 16; killer++)
        {
            count += Mp_player_stats[killer].count_kills(player_index);
        }
        return count;
    }

    // 0x009650E0 (:667)
    int count_hits(int player_index)
    {
        if ((unsigned int)player_index >= 16)
        {
            return 0;
        }
        return Mp_player_stats[player_index].count_hits();
    }

    // 0x00965150 (:683)
    int count_shots(int player_index)
    {
        if ((unsigned int)player_index >= 16)
        {
            return 0;
        }
        return Mp_player_stats[player_index].count_shots();
    }

    // 0x009651E0 (:697)
    int count_kills_with_weapon(int player_index, int weapon_index)
    {
        if ((unsigned int)player_index >= 16)
        {
            return 0;
        }
        return Mp_player_stats[player_index].weapons[weapon_index].count_kills();
    }

    // 0x00965240 (:710)
    int count_deaths_by_weapon(int player_index, int weapon_index)
    {
        if ((unsigned int)player_index >= 16)
        {
            return 0;
        }
        int count = 0;
        for (int killer = 0; killer < 16; killer++)
        {
            count += Mp_player_stats[killer].weapons[weapon_index].count_kills(player_index);
        }
        return count;
    }

    // 0x009652F0 (:724)
    int count_hits_with_weapon(int player_index, int weapon_index)
    {
        if ((unsigned int)player_index >= 16)
        {
            return 0;
        }
        return Mp_player_stats[player_index].weapons[weapon_index].count_hits();
    }

    // 0x00965350 (:738)
    int count_shots_with_weapon(int player_index, int weapon_index)
    {
        if ((unsigned int)player_index >= 16)
        {
            return 0;
        }
        return Mp_player_stats[player_index].weapons[weapon_index].shots_fired;
    }

    // 0x00965380 (:750)
    int count_kills_against_player(int player_index, int victim_index)
    {
        if ((unsigned int)player_index >= 16 || (unsigned int)victim_index >= 16)
        {
            return 0;
        }
        return Mp_player_stats[player_index].count_kills(victim_index);
    }

    // 0x00965420 (:764)
    int count_deaths_by_player(int player_index, int killer_index)
    {
        if ((unsigned int)player_index >= 16 || (unsigned int)killer_index >= 16)
        {
            return 0;
        }
        return Mp_player_stats[killer_index].count_kills(player_index);
    }

    // 0x009654C0 (:779)
    int count_hits_against_player(int player_index, int victim_index)
    {
        if ((unsigned int)player_index >= 16 || (unsigned int)victim_index >= 16)
        {
            return 0;
        }
        return Mp_player_stats[player_index].count_hits(victim_index);
    }
}

namespace multi_game_stats_comparing
{
    // 0x00965580 (:799)
    bool compare_kill_score_lt(int index_left, int index_right)
    {
        return multi_game_stats_counting::count_kills(index_left) < multi_game_stats_counting::count_kills(index_right);
    }

    // 0x009655B0 (:804)
    bool compare_deaths_lt(int index_left, int index_right)
    {
        return multi_game_stats_counting::count_deaths(index_left) < multi_game_stats_counting::count_deaths(index_right);
    }

    // 0x009655E0 (:809)
    bool compare_hits_lt(int index_left, int index_right)
    {
        return multi_game_stats_counting::count_hits(index_left) < multi_game_stats_counting::count_hits(index_right);
    }

    // 0x00965610 (:814)
    bool compare_shots_lt(int index_left, int index_right)
    {
        return multi_game_stats_counting::count_shots(index_left) < multi_game_stats_counting::count_shots(index_right);
    }
}

// 0x00965670 (:1152): the weapon handles after the weapon infos are "vehicle" and "no weapon"
void multi_game_stats_system_init()
{
    Vehicle_weapon_handle = Num_weapon_infos;
    No_weapon_handle = Num_weapon_infos + 1;
    Num_tracked_weapons = Num_weapon_infos + 2;
    Mp_player_stats = nullptr;
    Mp_weapon_stats = nullptr;
}

// 0x009A2880 (:1176): the stats of the 16 players with their weapon and backpack sub stats, and per weapon the 16
// players' sub stats
void multi_game_stats_init()
{
    parse_weapon_tracking_table_file();
    parse_backpack_tracking_table_file();
    parse_level_tracking_table_file();
    No_backpack_handle = 17;
    Num_tracked_backpacks = 18;
    Mp_player_stats = (multi_player_stats*)MP_allocators_mempool.alloc(Num_stats_players * sizeof(multi_player_stats), 4);
    for (int i = 0; i < Num_stats_players; i++)
    {
        Mp_player_stats[i].weapons = (multi_weapon_sub_stats*)MP_allocators_mempool.alloc(Num_tracked_weapons * sizeof(multi_weapon_sub_stats), 4);
        Mp_player_stats[i].backpacks = (multi_backpack_stats*)MP_allocators_mempool.alloc(Num_tracked_backpacks * sizeof(multi_backpack_stats), 4);
    }
    Mp_weapon_stats = (multi_weapon_stats*)MP_allocators_mempool.alloc(Num_tracked_weapons * sizeof(multi_weapon_stats), 4);
    for (int weapon = 0; weapon < Num_tracked_weapons; weapon++)
    {
        for (int i = 0; i < Num_stats_players; i++)
        {
            Mp_weapon_stats[weapon].players[i] = &Mp_player_stats[i].weapons[weapon];
        }
    }
}

// 0x009656A0 (:1201)
void multi_game_stats_shutdown()
{
    Stat_tracked_weapons = nullptr;
    Stat_tracked_backpacks = nullptr;
    Stat_tracked_levels = nullptr;
    Mp_player_stats = nullptr;
    Mp_weapon_stats = nullptr;
}

// Port helper: our stats (the snapshot set's slot), as rfg.exe inlines it after the match resets.
static void set_my_player_stats()
{
    if (Local_game_data_handles[ST_GAME_SNAPSHOT] != 0xFFFFFFFF && (Local_game_data_handles[ST_GAME_SNAPSHOT] & 0xFFFF) <= Num_stats_players)
    {
        My_player_stats = &Mp_player_stats[Local_game_data_handles[ST_GAME_SNAPSHOT] & 0xFFFF];
        return;
    }
    My_player_stats = nullptr;
}

// 0x00986390 (:1210)
void multi_game_stats_match_init_jip(unsigned short i)
{
    multi_game_stats_match_init(i);
    set_my_player_stats();
}

// 0x009656C0 (:1218): everything of the player counted for the match
void multi_game_stats_match_init(unsigned short i)
{
    multi_player_stats& stats = Mp_player_stats[i];
    stats.round_bonus_xp = 0;
    stats.suicides = 0;
    stats.kills = 0;
    stats.team_kills = 0;
    stats.deaths = 0;
    stats.most_suicides_in_a_row = 0;
    stats.most_kills_in_a_row = 0;
    stats.most_team_kills_in_a_row = 0;
    stats.most_deaths_in_a_row = 0;
    stats.suicides_in_a_row = 0;
    stats.kills_in_a_row = 0;
    stats.team_kills_in_a_row = 0;
    stats.deaths_in_a_row = 0;
    stats.round_wins = 0;
    stats.round_suicides = 0;
    stats.round_kills = 0;
    stats.round_team_kills = 0;
    stats.round_deaths = 0;
    stats.score = 0;
    stats.xp = 0;
    stats.round_score = 0;
    stats.round_xp = 0;
    // Magic number: 0xFF is "no weapon" for the last kill.
    stats.last_kill_weapon = 0xFF;
    stats.xp_in_a_life = 0;
    stats.bonus_xp = 0;
    stats.bonuses_in_a_life = 0;
    stats.win_streak = 0;
    stats.unique_bonus_flags[0] = 0;
    stats.unique_bonus_flags[1] = 0;
    memset(stats.xp_event_frequency, 0, sizeof(stats.xp_event_frequency));
    stats.highest_damage = 0.0f;
    stats.current_damage = 0.0f;
    stats.total_repair = 0.0f;
    stats.total_damage = 0.0f;
    stats.a_total_time_alive = 0.0f;
    stats.a_time_alive = 0.0f;
    stats.a_shortest_life = 0.0f;
    stats.a_longest_life = 0.0f;
    stats.a_lives = 0;
    stats.b_total_time_holding = 0.0f;
    stats.b_time_holding = 0.0f;
    stats.b_longest_hold = 0.0f;
    stats.b_hold_kills = 0;
    stats.ctf_captures = 0;
    stats.ctf_pickups = 0;
    stats.ctf_defenses = 0;
    stats.ctf_carrier_kills = 0;
    stats.dc_target_destruction = 0;
    stats.dc_target_repair = 0;
    stats.d_total_destruction = 0;
    stats.d_current_destruction = 0;
    stats.d_highest_destruction = 0;
    stats.d_lowest_destruction = 0;
    stats.d_num_destructions = 0;
    stats.undesired_playlist = 0;
    stats.skill_mismatch = 0;
    stats.bad_lag = 0;
    stats.game_migrated = 0;
    stats.finalized = false;
    for (int weapon = 0; weapon < Num_tracked_weapons; weapon++)
    {
        // rfg.exe clears the fields one by one before clearing the whole sub stats
        memset(&stats.weapons[weapon], 0, sizeof(multi_weapon_sub_stats));
    }
    for (int backpack = 0; backpack < Num_tracked_backpacks; backpack++)
    {
        stats.backpacks[backpack].xp = 0;
        stats.backpacks[backpack].xp_in_a_life = 0;
        stats.backpacks[backpack].time_equipped = 0.0f;
    }
}

// 0x009863D0 (:1328)
void multi_game_stats_match_init()
{
    for (unsigned short i = 0; i < Num_stats_players; i++)
    {
        multi_game_stats_match_init(i);
    }
    for (int team = 0; team < 2; team++)
    {
        Mp_team_stats[team].round_wins = 0;
        Mp_team_stats[team].score = 0;
        Mp_team_stats[team].round_score = 0;
    }
    set_my_player_stats();
}

// 0x00965940 (:1344): what counts for one round
void multi_game_stats_round_init(unsigned short i)
{
    multi_player_stats& stats = Mp_player_stats[i];
    stats.round_suicides = 0;
    stats.round_kills = 0;
    stats.round_deaths = 0;
    stats.round_score = 0;
    stats.round_team_kills = 0;
    stats.suicides_in_a_row = 0;
    stats.kills_in_a_row = 0;
    stats.team_kills_in_a_row = 0;
    stats.deaths_in_a_row = 0;
    stats.round_xp = 0;
    stats.round_bonus_xp = 0;
    stats.last_kill_weapon = 0xFF;
    stats.xp_in_a_life = 0;
    stats.bonuses_in_a_life = 0;
    stats.unique_bonus_flags[0] = 0;
    stats.unique_bonus_flags[1] = 0;
    stats.finalized = false;
    for (int weapon = 0; weapon < Num_tracked_weapons; weapon++)
    {
        stats.weapons[weapon].kills_in_a_life = 0;
        stats.weapons[weapon].finalized = false;
    }
}

// 0x009659D0 (:1370)
void multi_game_stats_round_init()
{
    for (unsigned short i = 0; i < Num_stats_players; i++)
    {
        multi_game_stats_round_init(i);
    }
    Mp_team_stats[0].round_score = 0;
    Mp_team_stats[1].round_score = 0;
}

// 0x009A2970 (:1383): nothing counts once the stats are being finalized
void multi_game_stats_record_shot(const player* attacker, const weapon_info* w_info, bool weapon_was_vehicle)
{
    if (multi_game_stats_are_finalizing())
    {
        return;
    }
    // mgs_get_weapon_index, inlined (the vehicle when the shot was a vehicle's)
    const int weapon_index = w_info != nullptr ? (int)weapons_get_weapon_info_handle(w_info) : (weapon_was_vehicle ? Vehicle_weapon_handle : No_weapon_handle);
    mgs_record_shot((int)stats_index_of(attacker->mp_data), weapon_index);
}

// 0x009A29F0 (:1405)
void multi_game_stats_record_hit(const player* attacker, const player* victim, const weapon_info* w_info, damaged_by_types damage_type)
{
    if (multi_game_stats_are_finalizing())
    {
        return;
    }
    const weapon_info* real_info = get_real_weapon_info(w_info);
    const unsigned int victim_index = victim != nullptr ? stats_index_of(victim->mp_data) : 0xFFFFFFFFu;
    mgs_record_hit((int)stats_index_of(attacker->mp_data), (int)victim_index, mgs_get_weapon_index(real_info, damage_type));
}

// 0x009A2A90 (:1426): the kill in the match stats; the local player's kills and deaths also go to its profile stats
// (per tracked weapon and backpack, overall, ranked or social, per mode; stand-ins in the test programs).
void multi_game_stats_record_kill(const player* attacker, const player* victim, const weapon_info* w_info, damaged_by_types damage_type)
{
    if (multi_game_stats_are_finalizing())
    {
        return;
    }
    const unsigned int attacker_index = stats_index_of(attacker->mp_data);
    // Original: the victim is dereferenced below although it is checked here.
    const unsigned int victim_index = victim != nullptr ? stats_index_of(victim->mp_data) : 0xFFFFFFFFu;
    const weapon_info* real_info = get_real_weapon_info(w_info);
    const int weapon_index = mgs_get_weapon_index(real_info, damage_type);
    mgs_record_kill((int)attacker_index, attacker->current_team, (int)victim_index, victim->current_team, weapon_index);
    const multi_session_info_base* info = Game_session->get_info();
    const bool ranked_or_social = !info->systemlink_or_lan && !info->private_match;
    if (object_is_local_player(attacker) && attacker != victim && human_teams_get_disposition(attacker, victim) == TEAM_DISPOSITION_HOSTILE)
    {
        const multi_player_stats& stats = Mp_player_stats[attacker_index];
        // The weapons with a kill in this life count for the variety stat.
        int variety = 0;
        for (int i = 0; i < Num_stat_tracked_weapons; i++)
        {
            const tracked_weapon_reference& ref = Stat_tracked_weapons[i];
            if (ref.w_info == real_info)
            {
                stats_increment_by_one(ref.kills_stat);
                stats_update_best(ref.one_life_kills_in_a_row_stat, stats.weapons[weapon_index].kills_in_a_row);
            }
            const unsigned int handle = weapons_get_weapon_info_handle(ref.w_info);
            if (handle != 0xFFFFFFFFu && stats.weapons[handle].kills_in_a_life > 0)
            {
                variety++;
            }
        }
        stats_increment_by_one(STATS_MP_TOTAL_WEAPON_KILLS_OVERALL);
        stats_update_best(STATS_MP_ONE_LIFE_WEAPON_KILLS_VARIETY, variety);
        if (attacker->backpack != nullptr)
        {
            for (int i = 0; i < Num_stat_tracked_backpacks; i++)
            {
                if (Stat_tracked_backpacks[i].b_info == attacker->backpack->get_backpack_info())
                {
                    stats_increment_by_one(Stat_tracked_backpacks[i].kills_stat);
                }
            }
            stats_increment_by_one(STATS_MP_TOTAL_BACKPACK_KILLS_OVERALL);
        }
        stats_increment_by_one(STATS_MP_TOTAL_KILLS_OVERALL);
        unit4::LocalGameSession* session = rfg_get_local_game_session();
        if (session != nullptr)
        {
            // Magic number: hero stat 7, the kills.
            session->updateHeroStatAdd(nullptr, 7, 1);
        }
        stats_update_best(STATS_MP_MATCH_HIGH_KILLS_OVERALL, stats.kills);
        stats_update_best(STATS_MP_ONE_LIFE_WEAPON_KILLS_IN_A_ROW_OVERALL, stats.kills_in_a_row);
        if (ranked_or_social)
        {
            stats_increment_by_one(STATS_MP_TOTAL_KILLS_RANKED_OR_SOCIAL);
            stats_update_best(STATS_MP_MATCH_HIGH_KILLS_RANKED_OR_SOCIAL, stats.kills);
            stats_update_best(STATS_MP_MATCH_HIGH_KILLS_RANKED_GAMES, stats.kills);
        }
        if (Multiplayer)
        {
            static const tracked_stats_indices total[8] = {STATS_MP_TOTAL_KILLS_DEATHMATCH, STATS_MP_TOTAL_KILLS_TEAM_DEATHMATCH, STATS_MP_TOTAL_KILLS_CAPTURE_THE_FLAG,
                                                          STATS_MP_TOTAL_KILLS_SIEGE, STATS_MP_TOTAL_KILLS_KING_OF_THE_HILL, STATS_MP_TOTAL_KILLS_BAGMAN,
                                                          STATS_MP_TOTAL_KILLS_TEAM_BAGMAN, STATS_MP_TOTAL_KILLS_DEMOLITION};
            static const tracked_stats_indices high[8] = {STATS_MP_MATCH_HIGH_KILLS_DEATHMATCH, STATS_MP_MATCH_HIGH_KILLS_TEAM_DEATHMATCH, STATS_MP_MATCH_HIGH_KILLS_CAPTURE_THE_FLAG,
                                                         STATS_MP_MATCH_HIGH_KILLS_SIEGE, STATS_MP_MATCH_HIGH_KILLS_KING_OF_THE_HILL, STATS_MP_MATCH_HIGH_KILLS_BAGMAN,
                                                         STATS_MP_MATCH_HIGH_KILLS_TEAM_BAGMAN, STATS_MP_MATCH_HIGH_KILLS_DEMOLITION};
            // The switch over the modes in MGPM_* order.
            if ((unsigned int)Multi_gameplay_mode < 8)
            {
                stats_increment_by_one(total[Multi_gameplay_mode]);
                stats_update_best(high[Multi_gameplay_mode], stats.kills);
            }
        }
    }
    if (!object_is_local_player(victim))
    {
        return;
    }
    const multi_player_stats& stats = Mp_player_stats[victim_index];
    for (int i = 0; i < Num_stat_tracked_weapons; i++)
    {
        if (Stat_tracked_weapons[i].w_info == real_info)
        {
            stats_increment_by_one(Stat_tracked_weapons[i].deaths_stat);
            stats_increment_by_one(STATS_MP_TOTAL_WEAPON_DEATHS_OVERALL);
            break;
        }
    }
    if (victim->backpack != nullptr)
    {
        for (int i = 0; i < Num_stat_tracked_backpacks; i++)
        {
            if (Stat_tracked_backpacks[i].b_info == victim->backpack->get_backpack_info())
            {
                stats_increment_by_one(Stat_tracked_backpacks[i].deaths_stat);
                stats_increment_by_one(STATS_MP_TOTAL_BACKPACK_DEATHS_OVERALL);
                break;
            }
        }
    }
    stats_increment_by_one(STATS_MP_TOTAL_DEATHS_OVERALL);
    stats_update_best(STATS_MP_MATCH_HIGH_DEATHS_OVERALL, stats.deaths);
    if (ranked_or_social)
    {
        stats_increment_by_one(STATS_MP_TOTAL_DEATHS_RANKED_GAMES);
        stats_update_best(STATS_MP_MATCH_HIGH_DEATHS_RANKED_GAMES, stats.deaths);
    }
    if (Multiplayer && (unsigned int)Multi_gameplay_mode < 8)
    {
        static const tracked_stats_indices total[8] = {STATS_MP_TOTAL_DEATHS_DEATHMATCH, STATS_MP_TOTAL_DEATHS_TEAM_DEATHMATCH, STATS_MP_TOTAL_DEATHS_CAPTURE_THE_FLAG,
                                                      STATS_MP_TOTAL_DEATHS_SIEGE, STATS_MP_TOTAL_DEATHS_KING_OF_THE_HILL, STATS_MP_TOTAL_DEATHS_BAGMAN,
                                                      STATS_MP_TOTAL_DEATHS_TEAM_BAGMAN, STATS_MP_TOTAL_DEATHS_DEMOLITION};
        static const tracked_stats_indices high[8] = {STATS_MP_MATCH_HIGH_DEATHS_DEATHMATCH, STATS_MP_MATCH_HIGH_DEATHS_TEAM_DEATHMATCH, STATS_MP_MATCH_HIGH_DEATHS_CAPTURE_THE_FLAG,
                                                     STATS_MP_MATCH_HIGH_DEATHS_SIEGE, STATS_MP_MATCH_HIGH_DEATHS_KING_OF_THE_HILL, STATS_MP_MATCH_HIGH_DEATHS_BAGMAN,
                                                     STATS_MP_MATCH_HIGH_DEATHS_TEAM_BAGMAN, STATS_MP_MATCH_HIGH_DEATHS_DEMOLITION};
        stats_increment_by_one(total[Multi_gameplay_mode]);
        stats_update_best(high[Multi_gameplay_mode], stats.deaths);
    }
}

// 0x009A30F0 (:1594): the local player played a playlist it did not want (not while finalizing). Original: the local
// player is not checked for null.
void multi_game_stats_record_undesired_playlist(bool undesired_playlist)
{
    if (!undesired_playlist || multi_game_stats_are_finalizing())
    {
        return;
    }
    const player* pp = player_get_local_player();
    if (pp->mp_data != 0xFFFFFFFFu && (pp->mp_data & 0xFFFF) < (unsigned int)Num_stats_players)
    {
        Mp_player_stats[pp->mp_data & 0xFFFF].undesired_playlist = 1;
    }
}

// 0x00965A80 (:1607)
bool multi_game_stats_had_undesired_playlist(unsigned int multi_game_data_handle)
{
    if (multi_game_data_handle != 0xFFFFFFFF && (multi_game_data_handle & 0xFFFF) < (unsigned int)Num_stats_players)
    {
        return Mp_player_stats[multi_game_data_handle & 0xFFFF].undesired_playlist == 1;
    }
    return false;
}

// 0x00965AB0 (:1615)
void multi_game_stats_record_bad_lag(unsigned int multi_game_data_handle)
{
    if (multi_game_data_handle != 0xFFFFFFFF && (multi_game_data_handle & 0xFFFF) < (unsigned int)Num_stats_players)
    {
        Mp_player_stats[multi_game_data_handle & 0xFFFF].bad_lag = 1;
    }
}

// 0x00965AE0 (:1623)
bool multi_game_stats_had_bad_lag(unsigned int multi_game_data_handle)
{
    if (multi_game_data_handle != 0xFFFFFFFF && (multi_game_data_handle & 0xFFFF) < (unsigned int)Num_stats_players)
    {
        return Mp_player_stats[multi_game_data_handle & 0xFFFF].bad_lag == 1;
    }
    return false;
}

// 0x00965B10 (:1631). As in rfg.exe: index 16 passes the range check.
multi_player_stats* multi_game_stats_get_player_stats(unsigned int game_data_handle)
{
    if (game_data_handle != 0xFFFFFFFF && (game_data_handle & 0xFFFF) <= (unsigned int)Num_stats_players)
    {
        return &Mp_player_stats[game_data_handle & 0xFFFF];
    }
    return nullptr;
}

// 0x00965B40 (:1641)
const multi_player_stats* multi_game_stats_get_my_stats()
{
    return My_player_stats;
}

// 0x00965B50 (:1646)
const multi_weapon_stats* multi_game_stats_get_weapon_stats(unsigned int winfo_handle)
{
    if (winfo_handle >= (unsigned int)Num_tracked_weapons)
    {
        return nullptr;
    }
    return &Mp_weapon_stats[winfo_handle];
}

// 0x00965B70 (:1653): the Guerilla and EDF teams
const multi_team_stats* multi_game_stats_get_team_stats(human_teams team)
{
    if ((unsigned int)team < HUMAN_TEAM_CIVILIAN)
    {
        return &Mp_team_stats[team];
    }
    return nullptr;
}

// 0x00965B90 (:1749): the games played on the current level, when the level is tracked (orig_main.md OM7, for the
// loss of leaving early). Stat_tracked_levels is a static of multi_game_mode_common.cpp, earlier in the multi unity build.
void multi_game_stats_record_played_match()
{
    for (int i = 0; i < Num_stat_tracked_levels; i++)
    {
        if (Stat_tracked_levels[i].l_info == Mp_server_options.get_level())
        {
            stats_increment_by_one(Stat_tracked_levels[i].games_played_stat);
        }
    }
}

// 0x009A3130 (:1662): the local player's tracked weapons also count in its profile stats
void multi_game_stats_record_weapon_time(const player* pp, const weapon_info* w_info, float time_s)
{
    if (multi_game_stats_are_finalizing() || pp == nullptr)
    {
        return;
    }
    // mgs_get_weapon_index, inlined (no vehicle here)
    const int weapon_index = w_info != nullptr ? (int)weapons_get_weapon_info_handle(w_info) : No_weapon_handle;
    mgs_record_weapon_time((int)stats_index_of(pp->mp_data), weapon_index, time_s);
    if (!object_is_local_player(pp))
    {
        return;
    }
    for (int i = 0; i < Num_stat_tracked_weapons; i++)
    {
        if (Stat_tracked_weapons[i].w_info == w_info)
        {
            stats_increment(Stat_tracked_weapons[i].time_equipped_stat, &time_s);
            stats_increment(STATS_MP_TOTAL_WEAPON_TIME_EQUIPPED_OVERALL, &time_s);
            return;
        }
    }
}

// 0x009A3220 (:1692)
void multi_game_stats_record_backpack_time(const player* pp, const multi_backpack_info* b_info, float time_s)
{
    if (multi_game_stats_are_finalizing() || pp == nullptr)
    {
        return;
    }
    mgs_record_backpack_time((int)stats_index_of(pp->mp_data), mgs_get_backpack_index(b_info), time_s);
    if (!object_is_local_player(pp))
    {
        return;
    }
    for (int i = 0; i < Num_stat_tracked_backpacks; i++)
    {
        if (Stat_tracked_backpacks[i].b_info == b_info)
        {
            stats_increment(Stat_tracked_backpacks[i].time_equipped_stat, &time_s);
            stats_increment(STATS_MP_TOTAL_BACKPACK_TIME_EQUIPPED_OVERALL, &time_s);
            return;
        }
    }
}

// 0x009A3300 (:1717)
void multi_game_stats_record_round_win(const multi_game_data* data)
{
    if (multi_game_stats_are_finalizing() || !data->in_use || data->handle == 0xFFFFFFFFu)
    {
        return;
    }
    Mp_player_stats[data->handle & 0xFFFF].round_wins++;
}

// 0x009A3340 (:1733): the team's round wins and each of its players' (those in the game too)
void multi_game_stats_record_team_round_win(human_teams team)
{
    if (team > HUMAN_TEAM_EDF)
    {
        return;
    }
    Mp_team_stats[team].round_wins++;
    for (const multi_game_data* data = multi_game_data_begin(ST_GAME_SNAPSHOT, true, false); data != nullptr; data = multi_game_data_next(data, true, false))
    {
        if (data->team == team)
        {
            multi_game_stats_record_round_win(data);
        }
    }
}

// 0x009A3420 (:1758): the score and round score change, at least 0 and at most the point limit (999 without one)
void multi_game_stats_alter_player_score(unsigned int multi_game_data_handle, int score_change)
{
    if (multi_game_stats_are_finalizing() || multi_game_data_handle == 0xFFFFFFFFu)
    {
        return;
    }
    // Original: the index is not range checked.
    multi_player_stats& stats = Mp_player_stats[multi_game_data_handle & 0xFFFF];
    const int score = (int)stats.score + score_change;
    const int round_score = (int)stats.round_score + score_change;
    const unsigned short new_score = score < 1 ? 0 : (unsigned short)score;
    const unsigned short new_round_score = round_score < 1 ? 0 : (unsigned short)round_score;
    // Magic number: 999 points without a point limit.
    const unsigned short limit = Mp_server_options.generic_options.point_limit != 0 ? Mp_server_options.generic_options.point_limit : 999;
    stats.score = new_score < limit ? new_score : limit;
    stats.round_score = new_round_score < limit ? new_round_score : limit;
}

// 0x009D2D60 (:1785): an experience event's experience for a player (not while finalizing): bonus or regular, the
// event's count; outside team events also the life's experience and the weapon's (the event's weapon, else the
// equipped one) and backpack's (the event's, else the worn one). For the local player the profile stats too: the
// one-life bests, the tracked weapon and backpack totals, the event's count and best count, the totals (ranked outside
// system link and private matches), the hero stat and the bonus variety.
void multi_game_stats_alter_player_xp(unsigned int multi_game_data_handle, int xp_change, multi_xp_events event, int weapon_id, int backpack_id)
{
    if (multi_game_stats_are_finalizing() || multi_game_data_handle == 0xFFFFFFFFu)
    {
        return;
    }
    // (rfg.exe also compares the 16-bit index with -1.) Original: the index is not range checked.
    const unsigned int index = multi_game_data_handle & 0xFFFF;
    const bool is_me = object_is_local_player(multi_game_data_get_player_instance(multi_game_data_handle)) || multi_game_data_handle == Local_game_data_handles[ST_GAME_SNAPSHOT];
    multi_player_stats& stats = Mp_player_stats[index];
    const unsigned short change = (unsigned short)xp_change;
    const bool bonus = (unsigned int)event < NUM_MULTI_XP_EVENTS && Multi_xp_events[event].valid && Multi_xp_events[event].bonus;
    if (bonus)
    {
        stats.bonus_xp += change;
        stats.round_bonus_xp += change;
        stats.bonuses_in_a_life++;
        // The bonus events start at MXE_MULTIPLE_KILLS.
        const int bit = event - MXE_MULTIPLE_KILLS;
        stats.unique_bonus_flags[bit / 32] |= 1u << (bit & 31);
    }
    else
    {
        stats.xp += change;
        stats.round_xp += change;
    }
    if (is_me)
    {
        stats_update_best(STATS_MP_ONE_LIFE_BONUSES, (int)stats.bonuses_in_a_life);
    }
    // The count saturates at 255.
    if (stats.xp_event_frequency[event] != 0xFF)
    {
        stats.xp_event_frequency[event]++;
    }
    const unsigned char frequency = stats.xp_event_frequency[event];
    const bool team_event = (unsigned int)event < NUM_MULTI_XP_EVENTS && Multi_xp_events[event].valid && Multi_xp_events[event].team_event;
    player* pp = team_event ? nullptr : multi_game_data_get_player_instance(multi_game_data_handle);
    if (!team_event)
    {
        stats.xp_in_a_life += change;
        if (is_me)
        {
            stats_update_best(STATS_MP_ONE_LIFE_XP, (int)stats.xp_in_a_life);
        }
    }
    if (pp != nullptr)
    {
        const weapon_info* w_info = weapons_get_weapon_info_from_weapon_unique_id(weapon_id);
        if (w_info == nullptr && pp->get_equipped_weapon() != nullptr)
        {
            w_info = pp->get_equipped_weapon()->w_info;
        }
        w_info = get_real_weapon_info(w_info);
        const int weapon_handle = w_info != nullptr ? (int)weapons_get_weapon_info_handle(w_info) : No_weapon_handle;
        stats.weapons[weapon_handle].xp += change;
        const multi_backpack_info* b_info = (unsigned int)backpack_id < 16 ? Backpack_info[backpack_id] : nullptr;
        if (b_info == nullptr && pp->backpack != nullptr)
        {
            b_info = pp->backpack->get_backpack_info();
        }
        const int backpack_handle = b_info != nullptr ? b_info->unique_id : No_backpack_handle;
        stats.backpacks[backpack_handle].xp += change;
        stats.backpacks[backpack_handle].xp_in_a_life += change;
        if (!is_me)
        {
            return;
        }
        for (int i = 0; i < Num_stat_tracked_weapons; i++)
        {
            if (Stat_tracked_weapons[i].w_info == w_info)
            {
                stats_increment(Stat_tracked_weapons[i].total_xp_stat, &xp_change);
                stats_increment(STATS_MP_TOTAL_WEAPON_XP_OVERALL, &xp_change);
                break;
            }
        }
        // The backpacks with 5 or more experience in this life.
        int variety = 0;
        for (int i = 0; i < Num_stat_tracked_backpacks; i++)
        {
            if (Stat_tracked_backpacks[i].b_info == b_info)
            {
                stats_increment(Stat_tracked_backpacks[i].total_xp_stat, &xp_change);
            }
            // Original: indexed by the tracked entry, not by the entry's backpack.
            if (stats.backpacks[i].xp_in_a_life > 4)
            {
                variety++;
            }
        }
        stats_update_best(STATS_MP_ONE_LIFE_BACKPACK_5XP_VARIETY, variety);
        if (b_info != nullptr)
        {
            stats_increment(STATS_MP_TOTAL_BACKPACK_XP_OVERALL, &xp_change);
        }
    }
    if (!is_me)
    {
        return;
    }
    // The per-event profile stats: the count from 0x7B, the best count in a match from 0xD2 (multi_xp_events order).
    stats_increment_by_one((tracked_stats_indices)(event + 0x7B));
    stats_update_best((tracked_stats_indices)(event + 0xD2), (int)frequency);
    const multi_session_info_base* info = Game_session->get_info();
    if (bonus)
    {
        stats_increment(STATS_MP_TOTAL_BONUS_XP, &xp_change);
        stats_update_best(STATS_MP_MATCH_HIGH_BONUS_XP, (int)stats.bonus_xp);
        if (!info->systemlink_or_lan && !info->private_match)
        {
            stats_increment(STATS_MP_TOTAL_BONUS_XP_RANKED_GAMES, &xp_change);
        }
    }
    stats_increment(STATS_MP_TOTAL_XP, &xp_change);
    stats_update_best(STATS_MP_MATCH_HIGH_XP, (int)stats.bonus_xp + (int)stats.xp);
    unit4::LocalGameSession* session = rfg_get_local_game_session();
    if (session != nullptr)
    {
        // Magic number: hero stat 6 (experience).
        session->updateHeroStatAdd(nullptr, 6, xp_change);
    }
    if (!info->systemlink_or_lan && !info->private_match)
    {
        stats_increment(STATS_MP_TOTAL_XP_RANKED_GAMES, &xp_change);
        stats_update_best(STATS_MP_MATCH_HIGH_XP_RANKED_GAMES, (int)stats.bonus_xp + (int)stats.xp);
    }
    // The distinct bonus events of the match (49 of them).
    int unique = 0;
    for (int i = 0; i < 49; i++)
    {
        if ((stats.unique_bonus_flags[i >> 5] & (1u << (i & 31))) != 0)
        {
            unique++;
        }
    }
    stats_update_best(STATS_MP_TOTAL_BONUS_XP_VARIETY, unique);
}

// 0x00986430 (:1935): as multi_game_stats_alter_player_score for the Guerilla and EDF teams (counted while finalizing
// too)
void multi_game_stats_alter_team_score(human_teams team, int score_change)
{
    if ((unsigned int)team >= HUMAN_TEAM_CIVILIAN)
    {
        return;
    }
    multi_team_stats& stats = Mp_team_stats[team];
    const int score = (int)stats.score + score_change;
    const int round_score = (int)stats.round_score + score_change;
    const unsigned short new_score = score < 1 ? 0 : (unsigned short)score;
    const unsigned short new_round_score = round_score < 1 ? 0 : (unsigned short)round_score;
    const unsigned short limit = Mp_server_options.generic_options.point_limit != 0 ? Mp_server_options.generic_options.point_limit : 999;
    stats.score = new_score < limit ? new_score : limit;
    stats.round_score = new_round_score < limit ? new_round_score : limit;
}

// 0x009864C0 (:1955): the team's round score becomes score (its total changes by the same amount)
void multi_game_stats_set_team_score(human_teams team, int score)
{
    if ((unsigned int)team < HUMAN_TEAM_CIVILIAN)
    {
        multi_game_stats_alter_team_score(team, score - (int)Mp_team_stats[team].round_score);
    }
}

// 0x00965BD0 (:1962): the player's index, round score, score, round experience and experience (BE)
void multi_game_stats_write_player_score(unsigned int multi_game_data_handle, unsigned char* packet_data, unsigned short& packet_size)
{
    if (multi_game_data_handle == 0xFFFFFFFFu)
    {
        return;
    }
    const unsigned int index = multi_game_data_handle & 0xFFFF;
    packet_data[packet_size++] = (unsigned char)index;
    const multi_player_stats& stats = Mp_player_stats[index];
    write_u16_be(packet_data, packet_size, stats.round_score);
    write_u16_be(packet_data, packet_size, stats.score);
    write_u16_be(packet_data, packet_size, stats.round_xp);
    write_u16_be(packet_data, packet_size, stats.xp);
}

// 0x00965C80 (:1974): an index of 16 or more is skipped with its scores unread (original)
void multi_game_stats_read_player_score(const unsigned char* packet_data, int& packet_offset)
{
    const unsigned char index = packet_data[packet_offset++];
    if (index >= Num_stats_players)
    {
        return;
    }
    multi_player_stats& stats = Mp_player_stats[index];
    stats.round_score = read_u16_be(packet_data, packet_offset);
    stats.score = read_u16_be(packet_data, packet_offset);
    stats.round_xp = read_u16_be(packet_data, packet_offset);
    stats.xp = read_u16_be(packet_data, packet_offset);
}

// 0x00965D40 (:1987): the 16 players' round score, score, round experience and experience
void multi_game_stats_write_all_player_scores(unsigned char* packet_data, unsigned short& packet_size)
{
    for (int i = 0; i < Num_stats_players; i++)
    {
        const multi_player_stats& stats = Mp_player_stats[i];
        write_u16_be(packet_data, packet_size, stats.round_score);
        write_u16_be(packet_data, packet_size, stats.score);
        write_u16_be(packet_data, packet_size, stats.round_xp);
        write_u16_be(packet_data, packet_size, stats.xp);
    }
}

// 0x00965F60 (:1997): the 16 players' round score, score, round experience and experience
void multi_game_stats_read_all_player_scores(const unsigned char* packet_data, int& packet_offset)
{
    for (int i = 0; i < Num_stats_players; i++)
    {
        multi_player_stats& stats = Mp_player_stats[i];
        stats.round_score = read_u16_be(packet_data, packet_offset);
        stats.score = read_u16_be(packet_data, packet_offset);
        stats.round_xp = read_u16_be(packet_data, packet_offset);
        stats.xp = read_u16_be(packet_data, packet_offset);
    }
}

// 0x00966240 (:2007): the team, its round score and score (Guerilla and EDF only)
void multi_game_stats_write_team_score(human_teams team, unsigned char* packet_data, unsigned short& packet_size)
{
    if ((unsigned int)team >= HUMAN_TEAM_CIVILIAN)
    {
        return;
    }
    packet_data[packet_size++] = (unsigned char)team;
    write_u16_be(packet_data, packet_size, Mp_team_stats[team].round_score);
    write_u16_be(packet_data, packet_size, Mp_team_stats[team].score);
}

// 0x009662B0 (:2016): another team is skipped with its scores unread (original)
void multi_game_stats_read_team_score(const unsigned char* packet_data, int& packet_offset)
{
    const unsigned int team = packet_data[packet_offset++];
    if (team >= 2)
    {
        return;
    }
    Mp_team_stats[team].round_score = read_u16_be(packet_data, packet_offset);
    Mp_team_stats[team].score = read_u16_be(packet_data, packet_offset);
}

// 0x00966330 (:2027): the two teams' round score and score
void multi_game_stats_write_all_team_scores(unsigned char* packet_data, unsigned short& packet_size)
{
    for (int team = 0; team < 2; team++)
    {
        write_u16_be(packet_data, packet_size, Mp_team_stats[team].round_score);
        write_u16_be(packet_data, packet_size, Mp_team_stats[team].score);
    }
}

// 0x009663C0 (:2035): the two teams' round score and score
void multi_game_stats_read_all_team_scores(const unsigned char* packet_data, int& packet_offset)
{
    for (int team = 0; team < 2; team++)
    {
        Mp_team_stats[team].round_score = read_u16_be(packet_data, packet_offset);
        Mp_team_stats[team].score = read_u16_be(packet_data, packet_offset);
    }
}

// 0x009D31D0 (:2043): server: a player joined in progress: every player's stats (not final) to everyone, the teams' to
// the joining client
void multi_game_stats_sync_jip_player(multi_connection* cp)
{
    multi_session* session = multi_session_get_by_type(ST_GAME);
    unsigned char packet[1200];
    for (const multi_game_data* data = multi_game_data_begin(ST_GAME_SNAPSHOT, false, false); data != nullptr; data = multi_game_data_next(data, false, false))
    {
        packet[0] = PT_PLAYER_STATS;
        unsigned short packet_size = packet_header_size(PT_PLAYER_STATS);
        add_player_stat_data_to_packet(packet, packet_size, nullptr, data->handle, 0xFF, false);
        packet_set_size(packet, packet_size);
        multi_io_send_to_all_reliable(session, packet, packet_size);
    }
    for (int team = HUMAN_TEAM_GUERILLA; team < 2; team++)
    {
        packet[0] = PT_TEAM_STATS;
        unsigned short packet_size = packet_header_size(PT_TEAM_STATS);
        add_team_stat_data_to_packet((human_teams)team, packet, packet_size);
        packet_set_size(packet, packet_size);
        multi_io_send_reliable(cp, packet, packet_size, nullptr, nullptr, nullptr);
    }
}

// 0x009D33E0 (:2077): the end of a round. Our win streak and everyone's best stats are filled in; the server sends
// every player's final stats and the teams' to everyone, a client sends its own to the server. The match music ends.
void multi_game_stats_endgame_sync()
{
    multi_session* session = multi_session_get_by_type(ST_GAME);
    const unsigned int my_handle = Local_game_data_handles[ST_GAME_SNAPSHOT];
    // Original: our stats are not checked for null.
    multi_player_stats* my_stats = multi_game_stats_get_player_stats(my_handle);
    my_stats->win_streak = stats_get_stat_value(STATS_MP_MATCH_WIN_STREAK);
    for (const multi_game_data* data = multi_game_data_begin(ST_GAME_SNAPSHOT, false, false); data != nullptr; data = multi_game_data_next(data, false, false))
    {
        const multi_player_stats* stats = multi_game_stats_get_player_stats(data->handle);
        // Ours, and those of the players whose final stats have not arrived.
        if (stats != nullptr && (data->handle == my_handle || !stats->finalized))
        {
            calculate_end_round_best_stats(data->handle);
        }
    }
    unsigned char packet[1200];
    if (Multiplayer && Server)
    {
        for (const multi_game_data* data = multi_game_data_begin(ST_GAME_SNAPSHOT, false, false); data != nullptr; data = multi_game_data_next(data, false, false))
        {
            packet[0] = PT_PLAYER_STATS;
            unsigned short packet_size = packet_header_size(PT_PLAYER_STATS);
            add_player_stat_data_to_packet(packet, packet_size, nullptr, data->handle, 0xFF, true);
            packet_set_size(packet, packet_size);
            multi_io_send_to_all_reliable(session, packet, packet_size);
        }
        for (int team = HUMAN_TEAM_GUERILLA; team < 2; team++)
        {
            packet[0] = PT_TEAM_STATS;
            unsigned short packet_size = packet_header_size(PT_TEAM_STATS);
            add_team_stat_data_to_packet((human_teams)team, packet, packet_size);
            packet_set_size(packet, packet_size);
            multi_io_send_to_all_reliable(session, packet, packet_size);
        }
    }
    else
    {
        packet[0] = PT_PLAYER_STATS;
        unsigned short packet_size = packet_header_size(PT_PLAYER_STATS);
        add_player_stat_data_to_packet(packet, packet_size, nullptr, my_handle, 0xFF, true);
        packet_set_size(packet, packet_size);
        multi_io_send_reliable(session->get_host_connection(), packet, packet_size, nullptr, nullptr, nullptr);
    }
    gamemusic_multiplayer_end();
}

// 0x00966490 (:2166): PT_PLAYER_STATS
void multi_game_stats_process_player_stats(unsigned char* packet_data, const sender& from)
{
    int packet_offset = 0;
    read_player_stat_data_from_packet(packet_data, packet_offset);
}

// 0x009664B0 (:2174): PT_TEAM_STATS
void multi_game_stats_process_team_stats(unsigned char* packet_data, const sender& from)
{
    int packet_offset = 0;
    read_team_stat_data_from_packet(packet_data, packet_offset);
}

// 0x009664D0 (:2184): our stats (and their weapons') are final; a dedicated server and a spectator have none
void multi_game_stats_start_finalizing()
{
    const player* local_player = player_get_local_player();
    if ((Multiplayer && Server && Dedicated) || Spectating || local_player == nullptr || local_player->mp_data == 0xFFFFFFFFu)
    {
        return;
    }
    multi_player_stats& stats = Mp_player_stats[local_player->mp_data & 0xFFFF];
    stats.finalized = true;
    for (int i = 0; i < Num_tracked_weapons; i++)
    {
        stats.weapons[i].finalized = true;
    }
}

// 0x009864F0 (:2201): the finalized flags of the players (not spectators) and their weapons, in order: true when a
// finalized one is followed by one that is not. Original: all finalized (or none) is false.
bool multi_game_stats_are_finalizing()
{
    bool any_finalized = false;
    for (const multi_game_data* data = multi_game_data_begin(ST_GAME_SNAPSHOT, false, false); data != nullptr; data = multi_game_data_next(data, false, false))
    {
        if (data->team == HUMAN_TEAM_MARAUDER || data->handle == 0xFFFFFFFFu)
        {
            continue;
        }
        const multi_player_stats& stats = Mp_player_stats[data->handle & 0xFFFF];
        if (any_finalized && !stats.finalized)
        {
            return true;
        }
        any_finalized = any_finalized || stats.finalized;
        for (int i = 0; i < Num_tracked_weapons; i++)
        {
            if (any_finalized && !stats.weapons[i].finalized)
            {
                return true;
            }
            any_finalized = any_finalized || stats.weapons[i].finalized;
        }
    }
    return false;
}

// 0x009866E0 (:2233): every connected player (not a spectator) has final stats
bool multi_game_stats_are_finalized()
{
    for (const multi_game_data* data = multi_game_data_begin(ST_GAME_SNAPSHOT, false, false); data != nullptr; data = multi_game_data_next(data, false, false))
    {
        if (data->team != HUMAN_TEAM_MARAUDER && data->handle != 0xFFFFFFFFu && Game_session->get_player_connection(data->player_uid) != nullptr &&
            !Mp_player_stats[data->handle & 0xFFFF].finalized)
        {
            return false;
        }
    }
    return true;
}

// 0x00966840 (:2344): the weapon with the most experience (the first of equals; the vehicle and "no weapon" do not
// count)
unsigned int multi_player_stats::get_favorite_weapon() const
{
    unsigned int favorite = 0xFFFFFFFFu;
    for (int i = 0; i < Num_tracked_weapons; i++)
    {
        if (i == No_weapon_handle || i == Vehicle_weapon_handle)
        {
            continue;
        }
        if (favorite == 0xFFFFFFFFu ? weapons[i].xp != 0 : weapons[favorite].xp < weapons[i].xp)
        {
            favorite = (unsigned int)i;
        }
    }
    return favorite;
}

// 0x009668B0 (:2361): as get_favorite_weapon for the backpacks
int multi_player_stats::get_favorite_backpack() const
{
    int favorite = -1;
    for (int i = 0; i < Num_tracked_backpacks; i++)
    {
        if (i == No_backpack_handle)
        {
            continue;
        }
        if (favorite == -1 ? backpacks[i].xp != 0 : backpacks[favorite].xp < backpacks[i].xp)
        {
            favorite = i;
        }
    }
    return favorite;
}

// 0x00966560 (:2271)
int multi_weapon_sub_stats::count_kills() const
{
    int count = 0;
    for (int target = 0; target < 16; target++)
    {
        count += damage[target].kills;
    }
    return count;
}

// 0x00966590 (:2280)
int multi_weapon_sub_stats::count_kills(int target_index) const
{
    return damage[target_index].kills;
}

// 0x009665A0 (:2285)
int multi_weapon_sub_stats::count_hits() const
{
    int count = 0;
    for (int target = 0; target < 16; target++)
    {
        count += damage[target].hits;
    }
    return count;
}

// 0x009665D0 (:2294)
int multi_weapon_sub_stats::count_hits(int target_index) const
{
    return damage[target_index].hits;
}

// 0x009665E0 (:2299)
int multi_player_stats::count_kills() const
{
    int count = 0;
    for (int weapon = 0; weapon < Num_tracked_weapons; weapon++)
    {
        count += weapons[weapon].count_kills();
    }
    return count;
}

// 0x00966660 (:2308)
int multi_player_stats::count_kills(int target_index) const
{
    int count = 0;
    for (int weapon = 0; weapon < Num_tracked_weapons; weapon++)
    {
        count += weapons[weapon].count_kills(target_index);
    }
    return count;
}

// 0x009666D0 (:2317)
int multi_player_stats::count_hits() const
{
    int count = 0;
    for (int weapon = 0; weapon < Num_tracked_weapons; weapon++)
    {
        count += weapons[weapon].count_hits();
    }
    return count;
}

// 0x00966740 (:2326)
int multi_player_stats::count_hits(int target_index) const
{
    int count = 0;
    for (int weapon = 0; weapon < Num_tracked_weapons; weapon++)
    {
        count += weapons[weapon].count_hits(target_index);
    }
    return count;
}

// 0x009667D0 (:2335)
int multi_player_stats::count_shots() const
{
    int count = 0;
    for (int weapon = 0; weapon < Num_tracked_weapons; weapon++)
    {
        count += weapons[weapon].shots_fired;
    }
    return count;
}

// 0x00966900 (:2378): the first of the most used, -1 without a kill
int multi_player_stats::get_weapon_with_most_kills() const
{
    int best = 0;
    for (int weapon = 1; weapon < Num_tracked_weapons; weapon++)
    {
        if (weapons[best].count_kills() < weapons[weapon].count_kills())
        {
            best = weapon;
        }
    }
    return weapons[best].count_kills() < 1 ? -1 : best;
}

// Port helper (inlined twice in rfg.exe)
static float weapon_accuracy(const multi_weapon_sub_stats& weapon)
{
    return weapon.shots_fired > 0 ? (float)weapon.count_hits() / (float)weapon.shots_fired : 0.0f;
}

// 0x00966A40 (:2393): hits per shot, -1 if the best one was never fired
int multi_player_stats::get_weapon_with_highest_accuracy() const
{
    int best = 0;
    for (int weapon = 1; weapon < Num_tracked_weapons; weapon++)
    {
        if (weapon_accuracy(weapons[best]) < weapon_accuracy(weapons[weapon]))
        {
            best = weapon;
        }
    }
    return weapons[best].shots_fired < 1 ? -1 : best;
}

// 0x00986820 (:2417): the distinct valid bonus events of the match
int multi_player_stats::count_unique_bonus_xp_events() const
{
    int count = 0;
    for (int i = 0; i < NUM_MULTI_XP_EVENTS; i++)
    {
        if (Multi_xp_events[i].valid && Multi_xp_events[i].bonus && xp_event_frequency[i] != 0)
        {
            count++;
        }
    }
    return count;
}

// 0x00986940 (:2430): the first of the most frequent valid bonus events, MXE_NONE without one
multi_xp_events multi_player_stats::get_most_common_bonus_xp_event() const
{
    multi_xp_events best = MXE_NONE;
    for (int i = 0; i < NUM_MULTI_XP_EVENTS; i++)
    {
        const unsigned char best_frequency = best != MXE_NONE ? xp_event_frequency[best] : 0;
        if (Multi_xp_events[i].valid && Multi_xp_events[i].bonus && best_frequency < xp_event_frequency[i])
        {
            best = (multi_xp_events)i;
        }
    }
    return best;
}

// 0x00966BB0 (:2447)
int multi_weapon_stats::count_kills() const
{
    int count = 0;
    for (int player = 0; player < 16; player++)
    {
        count += players[player]->count_kills();
    }
    return count;
}

// 0x00966C10 (:2456)
int multi_weapon_stats::count_hits() const
{
    int count = 0;
    for (int player = 0; player < 16; player++)
    {
        count += players[player]->count_hits();
    }
    return count;
}

// 0x00966C70 (:2465)
int multi_weapon_stats::count_shots() const
{
    int count = 0;
    for (int player = 0; player < 16; player++)
    {
        count += players[player]->shots_fired;
    }
    return count;
}

// 0x00966CC0 (:2474)
float multi_weapon_stats::count_time() const
{
    float time = 0.0f;
    for (int player = 0; player < 16; player++)
    {
        time += players[player]->time_equipped;
    }
    return time;
}

// 0x00986AC0 (:2491)
void multi_game_stats_sort_players_based_on_round_score(human_teams team, unsigned int* players, int& num_players)
{
    sort_players(team, players, num_players, qsort_player_round_score_compare);
}

// 0x00986AE0 (:2502)
void multi_game_stats_sort_players_based_on_score(human_teams team, unsigned int* players, int& num_players)
{
    sort_players(team, players, num_players, qsort_player_score_compare);
}

// 0x00986B00 (:2513)
void multi_game_stats_sort_players_based_on_round_xp(human_teams team, unsigned int* players, int& num_players)
{
    sort_players(team, players, num_players, qsort_player_round_xp_compare);
}

// 0x00986B20 (:2524)
void multi_game_stats_sort_players_based_on_xp(human_teams team, unsigned int* players, int& num_players)
{
    sort_players(team, players, num_players, qsort_player_xp_compare);
}

// 0x00986B40 (:2532): the player with the best round score (the connected ones unless include_disconnected); a tie for
// the best has no winner unless ignore_ties (the first of them then)
unsigned int multi_game_stats_get_round_winning_player(bool include_disconnected, bool ignore_ties)
{
    unsigned int winner = 0xFFFFFFFFu;
    int best_score = (int)0x80000001;
    // ST_GAME, or ST_GAME_SNAPSHOT with the players who left
    const session_types set = (session_types)(ST_GAME + (include_disconnected ? 1 : 0));
    for (const multi_game_data* data = multi_game_data_begin(set, false, false); data != nullptr; data = multi_game_data_next(data, false, false))
    {
        if (data->handle == 0xFFFFFFFFu || (!include_disconnected && Game_session->get_player_connection(data->player_uid) == nullptr))
        {
            continue;
        }
        const int score = Mp_player_stats[data->handle & 0xFFFF].round_score;
        if (score > best_score)
        {
            winner = data->handle;
            best_score = score;
        }
        else if (score == best_score && !ignore_ties)
        {
            winner = 0xFFFFFFFFu;
        }
    }
    return winner;
}

// 0x00986C70 (:2562)
unsigned int multi_game_stats_get_highest_round_xp_player()
{
    unsigned int best_player = 0xFFFFFFFFu;
    int best_xp = (int)0x80000001;
    for (const multi_game_data* data = multi_game_data_begin(ST_GAME_SNAPSHOT, false, false); data != nullptr; data = multi_game_data_next(data, false, false))
    {
        if (data->handle != 0xFFFFFFFFu && best_xp < (int)Mp_player_stats[data->handle & 0xFFFF].round_xp)
        {
            best_xp = Mp_player_stats[data->handle & 0xFFFF].round_xp;
            best_player = data->handle;
        }
    }
    return best_player;
}

// 0x00966DC0 (:2585): the Guerillas and the EDF, the better round score first
void multi_game_stats_sort_teams_based_on_round_score(human_teams* teams)
{
    teams[0] = HUMAN_TEAM_GUERILLA;
    teams[1] = HUMAN_TEAM_EDF;
    qsort(teams, 2, sizeof(human_teams), qsort_team_round_score_compare);
}

// 0x00966DF0 (:2595): the team with the better round score, none at a tie
human_teams multi_game_stats_get_round_winning_team()
{
    // The sorted pair, inlined: the Guerillas lead unless the EDF has more.
    if ((int)Mp_team_stats[0].round_score < (int)Mp_team_stats[1].round_score)
    {
        return HUMAN_TEAM_EDF;
    }
    if (Mp_team_stats[1].round_score == Mp_team_stats[0].round_score)
    {
        return HUMAN_TEAM_NONE;
    }
    return HUMAN_TEAM_GUERILLA;
}
