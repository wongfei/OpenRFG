// Original: volition/rfg/code/multi/multi_game/multi_game_stats.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_stats.cpp
//
// Multiplayer stats: per player and weapon kills, deaths, hits, shots and damage, team stats, counted and compared for
// the scoreboard and the end of match.
#pragma once

#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"

struct multi_backpack_info;
class multi_connection;
struct multi_game_data;
class player;
struct sender;
struct weapon_info;

// size 0x8
struct multi_damage_sub_stats
{
    int kills; // 0x00
    int hits; // 0x04
};

// size 0x9C
struct multi_weapon_sub_stats
{
    int count_kills(int target_index) const; // 0x00966590 (:2280)
    int count_kills() const; // 0x00966560 (:2271)
    int count_hits(int target_index) const; // 0x009665D0 (:2294)
    int count_hits() const; // 0x009665A0 (:2285)

    multi_damage_sub_stats damage[16]; // 0x00
    int shots_fired; // 0x80
    float time_equipped; // 0x84
    int kills_in_a_life; // 0x88
    int most_kills_in_a_life; // 0x8C
    int kills_in_a_row; // 0x90
    int most_kills_in_a_row; // 0x94
    unsigned short xp; // 0x98
    bool finalized; // 0x9A
};

// size 0x40
struct multi_weapon_stats
{
    int count_kills() const; // 0x00966BB0 (:2447)
    int count_hits() const; // 0x00966C10 (:2456)
    int count_shots() const; // 0x00966C70 (:2465)
    float count_time() const; // 0x00966CC0 (:2474)

    multi_weapon_sub_stats* players[16]; // 0x00
};

// size 0x8 (rfg.pdb; placement inferred)
struct multi_backpack_stats
{
    unsigned short xp; // 0x00
    unsigned short xp_in_a_life; // 0x02
    float time_equipped; // 0x04
};

// size 0x13C
struct multi_player_stats
{
    int count_kills(int target_index) const; // 0x00966660 (:2308)
    int count_kills() const; // 0x009665E0 (:2299)
    int count_hits(int target_index) const; // 0x00966740 (:2326)
    int count_hits() const; // 0x009666D0 (:2317)
    int count_shots() const; // 0x009667D0 (:2335)
    unsigned int get_favorite_weapon() const; // 0x00966840 (:2344)
    int get_favorite_backpack() const; // 0x009668B0 (:2361)
    int get_weapon_with_most_kills() const; // 0x00966900 (:2378)
    int get_weapon_with_highest_accuracy() const; // 0x00966A40 (:2393)
    int count_unique_bonus_xp_events() const; // 0x00986820 (:2417)
    multi_xp_events get_most_common_bonus_xp_event() const; // 0x00986940 (:2430)

    multi_weapon_sub_stats* weapons; // 0x00
    multi_backpack_stats* backpacks; // 0x04
    unsigned short score; // 0x08
    unsigned short xp; // 0x0A
    unsigned short xp_in_a_life; // 0x0C
    unsigned short bonus_xp; // 0x0E
    unsigned short bonuses_in_a_life; // 0x10
    unsigned int unique_bonus_flags[2]; // 0x14
    int suicides; // 0x1C
    int kills; // 0x20
    int team_kills; // 0x24
    int deaths; // 0x28
    int most_suicides_in_a_row; // 0x2C
    int most_kills_in_a_row; // 0x30
    int most_team_kills_in_a_row; // 0x34
    int most_deaths_in_a_row; // 0x38
    int suicides_in_a_row; // 0x3C
    int kills_in_a_row; // 0x40
    int team_kills_in_a_row; // 0x44
    int deaths_in_a_row; // 0x48
    int round_wins; // 0x4C
    int round_kills; // 0x50
    int round_team_kills; // 0x54
    int round_suicides; // 0x58
    int round_deaths; // 0x5C
    unsigned short round_score; // 0x60
    unsigned short round_xp; // 0x62
    unsigned short round_bonus_xp; // 0x64
    int win_streak; // 0x68
    int last_kill_weapon; // 0x6C
    float current_damage; // 0x70
    float total_damage; // 0x74
    float highest_damage; // 0x78
    float total_repair; // 0x7C
    unsigned char xp_event_frequency[84]; // 0x80
    float a_total_time_alive; // 0xD4
    float a_time_alive; // 0xD8
    float a_shortest_life; // 0xDC
    float a_longest_life; // 0xE0
    int a_lives; // 0xE4
    float b_total_time_holding; // 0xE8
    float b_time_holding; // 0xEC
    float b_longest_hold; // 0xF0
    int b_hold_kills; // 0xF4
    int ctf_captures; // 0xF8
    int ctf_pickups; // 0xFC
    int ctf_defenses; // 0x100
    int ctf_carrier_kills; // 0x104
    int dc_target_destruction; // 0x108
    int dc_target_repair; // 0x10C
    int d_total_destruction; // 0x110
    int d_current_destruction; // 0x114
    int d_highest_destruction; // 0x118
    int d_lowest_destruction; // 0x11C
    int d_num_destructions; // 0x120
    int d_engineer_kills; // 0x124
    unsigned char undesired_playlist; // 0x128
    unsigned char skill_mismatch; // 0x129
    unsigned char bad_lag; // 0x12A
    unsigned char game_migrated; // 0x12B
    bool finalized; // 0x12C
    multi_xp_events most_frequent_bonus_xp_event; // 0x130
    unsigned int favorite_weapon; // 0x134
    int favorite_backpack; // 0x138
};

// size 0x8
struct multi_team_stats
{
    unsigned short score; // 0x00
    unsigned short round_score; // 0x02
    int round_wins; // 0x04
};

namespace multi_game_stats_counting
{
    int count_kills(int player_index); // 0x00964F70 (:636)
    int count_deaths(int player_index); // 0x00965010 (:651)
    int count_hits(int player_index); // 0x009650E0 (:667)
    int count_shots(int player_index); // 0x00965150 (:683)
    int count_kills_with_weapon(int player_index, int weapon_index); // 0x009651E0 (:697)
    int count_deaths_by_weapon(int player_index, int weapon_index); // 0x00965240 (:710)
    int count_hits_with_weapon(int player_index, int weapon_index); // 0x009652F0 (:724)
    int count_shots_with_weapon(int player_index, int weapon_index); // 0x00965350 (:738)
    int count_kills_against_player(int player_index, int victim_index); // 0x00965380 (:750)
    int count_deaths_by_player(int player_index, int killer_index); // 0x00965420 (:764)
    int count_hits_against_player(int player_index, int victim_index); // 0x009654C0 (:779)
}

namespace multi_game_stats_comparing
{
    bool compare_kill_score_lt(int index_left, int index_right); // 0x00965580 (:799)
    bool compare_deaths_lt(int index_left, int index_right); // 0x009655B0 (:804)
    bool compare_hits_lt(int index_left, int index_right); // 0x009655E0 (:809)
    bool compare_shots_lt(int index_left, int index_right); // 0x00965610 (:814)
}

void multi_game_stats_system_init(); // 0x00965670 (:1152)
void multi_game_stats_init(); // 0x009A2880 (:1176)
void multi_game_stats_shutdown(); // 0x009656A0 (:1201)
void multi_game_stats_match_init_jip(unsigned short i); // 0x00986390 (:1210)
void multi_game_stats_match_init(unsigned short i); // 0x009656C0 (:1218)
void multi_game_stats_match_init(); // 0x009863D0 (:1328)
void multi_game_stats_round_init(unsigned short i); // 0x00965940 (:1344)
void multi_game_stats_round_init(); // 0x009659D0 (:1370)
void multi_game_stats_record_shot(const player* attacker, const weapon_info* w_info, bool weapon_was_vehicle); // 0x009A2970 (:1383)
void multi_game_stats_record_hit(const player* attacker, const player* victim, const weapon_info* w_info, damaged_by_types damage_type); // 0x009A29F0 (:1405)
void multi_game_stats_record_kill(const player* attacker, const player* victim, const weapon_info* w_info, damaged_by_types damage_type); // 0x009A2A90 (:1426)
void multi_game_stats_record_undesired_playlist(bool undesired_playlist); // 0x009A30F0 (:1594)
bool multi_game_stats_had_undesired_playlist(unsigned int multi_game_data_handle); // 0x00965A80 (:1607)
void multi_game_stats_record_bad_lag(unsigned int multi_game_data_handle); // 0x00965AB0 (:1615)
bool multi_game_stats_had_bad_lag(unsigned int multi_game_data_handle); // 0x00965AE0 (:1623)
multi_player_stats* multi_game_stats_get_player_stats(unsigned int game_data_handle); // 0x00965B10 (:1631)
const multi_player_stats* multi_game_stats_get_my_stats(); // 0x00965B40 (:1641)
const multi_weapon_stats* multi_game_stats_get_weapon_stats(unsigned int winfo_handle); // 0x00965B50 (:1646)
const multi_team_stats* multi_game_stats_get_team_stats(human_teams team); // 0x00965B70 (:1653)
void multi_game_stats_record_weapon_time(const player* pp, const weapon_info* w_info, float time_s); // 0x009A3130 (:1662)
void multi_game_stats_record_backpack_time(const player* pp, const multi_backpack_info* b_info, float time_s); // 0x009A3220 (:1692)
void multi_game_stats_record_round_win(const multi_game_data* data); // 0x009A3300 (:1717)
void multi_game_stats_record_team_round_win(human_teams team); // 0x009A3340 (:1733)
void multi_game_stats_record_played_match(); // 0x00965B90 (:1749)
void multi_game_stats_alter_player_score(unsigned int multi_game_data_handle, int score_change); // 0x009A3420 (:1758)
void multi_game_stats_alter_player_xp(unsigned int multi_game_data_handle, int xp_change, multi_xp_events event, int weapon_id, int backpack_id); // 0x009D2D60 (:1785)
void multi_game_stats_alter_team_score(human_teams team, int score_change); // 0x00986430 (:1935)
void multi_game_stats_set_team_score(human_teams team, int score); // 0x009864C0 (:1955)
void multi_game_stats_write_player_score(unsigned int multi_game_data_handle, unsigned char* packet_data, unsigned short& packet_size); // 0x00965BD0 (:1962)
void multi_game_stats_read_player_score(const unsigned char* packet_data, int& packet_offset); // 0x00965C80 (:1974)
void multi_game_stats_write_all_player_scores(unsigned char* packet_data, unsigned short& packet_size); // 0x00965D40 (:1987)
void multi_game_stats_read_all_player_scores(const unsigned char* packet_data, int& packet_offset); // 0x00965F60 (:1997)
void multi_game_stats_write_team_score(human_teams team, unsigned char* packet_data, unsigned short& packet_size); // 0x00966240 (:2007)
void multi_game_stats_read_team_score(const unsigned char* packet_data, int& packet_offset); // 0x009662B0 (:2016)
void multi_game_stats_write_all_team_scores(unsigned char* packet_data, unsigned short& packet_size); // 0x00966330 (:2027)
void multi_game_stats_read_all_team_scores(const unsigned char* packet_data, int& packet_offset); // 0x009663C0 (:2035)
void multi_game_stats_sync_jip_player(multi_connection* cp); // 0x009D31D0 (:2043)
void multi_game_stats_endgame_sync(); // 0x009D33E0 (:2077)
void multi_game_stats_process_player_stats(unsigned char* packet_data, const sender& from); // 0x00966490 (:2166)
void multi_game_stats_process_team_stats(unsigned char* packet_data, const sender& from); // 0x009664B0 (:2174)
void multi_game_stats_start_finalizing(); // 0x009664D0 (:2184)
bool multi_game_stats_are_finalizing(); // 0x009864F0 (:2201)
bool multi_game_stats_are_finalized(); // 0x009866E0 (:2233)
void multi_game_stats_sort_players_based_on_round_score(human_teams team, unsigned int* players, int& num_players); // 0x00986AC0 (:2491)
void multi_game_stats_sort_players_based_on_score(human_teams team, unsigned int* players, int& num_players); // 0x00986AE0 (:2502)
void multi_game_stats_sort_players_based_on_round_xp(human_teams team, unsigned int* players, int& num_players); // 0x00986B00 (:2513)
void multi_game_stats_sort_players_based_on_xp(human_teams team, unsigned int* players, int& num_players); // 0x00986B20 (:2524)
unsigned int multi_game_stats_get_round_winning_player(bool include_disconnected, bool ignore_ties); // 0x00986B40 (:2532)
unsigned int multi_game_stats_get_highest_round_xp_player(); // 0x00986C70 (:2562)
void multi_game_stats_sort_teams_based_on_round_score(human_teams* teams); // 0x00966DC0 (:2585)
human_teams multi_game_stats_get_round_winning_team(); // 0x00966DF0 (:2595)
