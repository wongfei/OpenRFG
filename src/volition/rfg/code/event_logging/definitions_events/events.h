// Original: volition/rfg/code/event_logging/definitions_events/events.h
//
// The logged events (inline in the header; stub_impl.md S5): one log_event_* function per event, and start/stop pairs
// for timed events. Each one asks the logger whether its type is recorded and hands it the values with their sizes
// (vectors as three floats, in parameter order; none of variable length). The logger in rfg.exe is Volition's no-op
// build (vlogger_lib/logger_stubs.cpp): no event type is recorded, so the add and start calls never run.
// Generated from the rfg.pdb signatures and the type ids of the binary; the sizes and counts were checked against
// every size table the decompile shows.
#pragma once

#include "volition/vlib/code/math/vector3.h"
#include "volition/vlogger_lib/include/logger.h"

#include <stdint.h>

// 0x0094BFD0 (.h:41)
inline int log_event_damage_control_destruction_volume(int player_checksum, short seconds_left_in_round, short pct_destruction_1, short pct_repair_1, short pct_destruction_2, short pct_repair_2, short pct_destruction_3, short pct_repair_3, short pct_destruction_4, short pct_repair_4, short pct_destruction_5, short pct_repair_5)
{
    if (!log_record_event(0))
    {
        return -1;
    }
    void* argv[12] = {&player_checksum, &seconds_left_in_round, &pct_destruction_1, &pct_repair_1, &pct_destruction_2, &pct_repair_2, &pct_destruction_3, &pct_repair_3, &pct_destruction_4, &pct_repair_4, &pct_destruction_5, &pct_repair_5};
    unsigned char sizes[12] = {4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
    bool variable_length[12] = {};
    log_add_event(0, 12, argv, sizes, variable_length);
    return 0;
}

// 0x0094C0C0 (.h:83)
inline int log_event_siege_destruction_volume(int player_checksum, short seconds_left_in_round, short pct_destruction_1, short pct_repair_1, short pct_destruction_2, short pct_repair_2, short pct_destruction_3, short pct_repair_3, short pct_destruction_4, short pct_repair_4, short pct_destruction_5, short pct_repair_5, short pct_destruction_6, short pct_repair_6, short pct_destruction_7, short pct_repair_7, short pct_destruction_8, short pct_repair_8, short pct_destruction_9, short pct_repair_9, short pct_destruction_10, short pct_repair_10, short pct_destruction_11, short pct_repair_11, short pct_destruction_12, short pct_repair_12)
{
    if (!log_record_event(1))
    {
        return -1;
    }
    void* argv[26] = {&player_checksum, &seconds_left_in_round, &pct_destruction_1, &pct_repair_1, &pct_destruction_2, &pct_repair_2, &pct_destruction_3, &pct_repair_3, &pct_destruction_4, &pct_repair_4, &pct_destruction_5, &pct_repair_5, &pct_destruction_6, &pct_repair_6, &pct_destruction_7, &pct_repair_7, &pct_destruction_8, &pct_repair_8, &pct_destruction_9, &pct_repair_9, &pct_destruction_10, &pct_repair_10, &pct_destruction_11, &pct_repair_11, &pct_destruction_12, &pct_repair_12};
    unsigned char sizes[26] = {4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
    bool variable_length[26] = {};
    log_add_event(1, 26, argv, sizes, variable_length);
    return 0;
}

// 0x00A060B0 (.h:109)
inline int log_event_campaign(int player_checksum, int time, int time_played, char pct_game_complete, int locale_id, int language, int title_id, char difficulty, vector player_pos, char gold_tier)
{
    if (!log_record_event(2))
    {
        return -1;
    }
    void* argv[12] = {&player_checksum, &time, &time_played, &pct_game_complete, &locale_id, &language, &title_id, &difficulty, &player_pos.x, &player_pos.y, &player_pos.z, &gold_tier};
    unsigned char sizes[12] = {4, 4, 4, 1, 4, 4, 4, 1, 4, 4, 4, 1};
    bool variable_length[12] = {};
    log_add_event(2, 12, argv, sizes, variable_length);
    return 0;
}

// 0x0094C2F0 (.h:131)
inline int log_event_player_score(int player_checksum, int time, char player_index, char team_id, short score, char player_xp_level)
{
    if (!log_record_event(3))
    {
        return -1;
    }
    void* argv[6] = {&player_checksum, &time, &player_index, &team_id, &score, &player_xp_level};
    unsigned char sizes[6] = {4, 4, 1, 1, 2, 1};
    bool variable_length[6] = {};
    log_add_event(3, 6, argv, sizes, variable_length);
    return 0;
}

// 0x0074F140 (.h:156)
inline int log_event_weapon_equipped(int player_checksum, int time, int weapon_crc, int time_played, char pct_game_complete, int locale_id, char difficulty, int duration, vector player_pos)
{
    if (!log_record_event(4))
    {
        return -1;
    }
    void* argv[11] = {&player_checksum, &time, &weapon_crc, &time_played, &pct_game_complete, &locale_id, &difficulty, &duration, &player_pos.x, &player_pos.y, &player_pos.z};
    unsigned char sizes[11] = {4, 4, 4, 4, 1, 4, 1, 4, 4, 4, 4};
    bool variable_length[11] = {};
    log_add_event(4, 11, argv, sizes, variable_length);
    return 0;
}

// 0x007547A0 (.h:184)
inline int log_event_mission_attempt(int player_checksum, int time, int mission_crc, int time_played, char pct_game_complete, char mission_exit_status, int salvage, int locale_id, char difficulty, int duration, vector player_pos, int end_condition_crc)
{
    if (!log_record_event(5))
    {
        return -1;
    }
    void* argv[14] = {&player_checksum, &time, &mission_crc, &time_played, &pct_game_complete, &mission_exit_status, &salvage, &locale_id, &difficulty, &duration, &player_pos.x, &player_pos.y, &player_pos.z, &end_condition_crc};
    unsigned char sizes[14] = {4, 4, 4, 4, 1, 1, 4, 4, 1, 4, 4, 4, 4, 4};
    bool variable_length[14] = {};
    log_add_event(5, 14, argv, sizes, variable_length);
    return 0;
}

// 0x007548C0 (.h:209)
inline int log_event_control_change(int player_checksum, int time, char new_control_value, int time_played, char pct_game_complete, int locale_id, char difficulty, int duration, vector player_pos)
{
    if (!log_record_event(6))
    {
        return -1;
    }
    void* argv[11] = {&player_checksum, &time, &new_control_value, &time_played, &pct_game_complete, &locale_id, &difficulty, &duration, &player_pos.x, &player_pos.y, &player_pos.z};
    unsigned char sizes[11] = {4, 4, 1, 4, 1, 4, 1, 4, 4, 4, 4};
    bool variable_length[11] = {};
    log_add_event(6, 11, argv, sizes, variable_length);
    return 0;
}

// 0x00A061A0 (.h:234)
inline int log_event_player_use_weapons_cache(int player_checksum, int time, int time_played, char pct_game_complete, int locale_id, char difficulty, int duration, vector player_pos, char is_ammo_box)
{
    if (!log_record_event(7))
    {
        return -1;
    }
    void* argv[11] = {&player_checksum, &time, &time_played, &pct_game_complete, &locale_id, &difficulty, &duration, &player_pos.x, &player_pos.y, &player_pos.z, &is_ammo_box};
    unsigned char sizes[11] = {4, 4, 4, 1, 4, 1, 4, 4, 4, 4, 1};
    bool variable_length[11] = {};
    log_add_event(7, 11, argv, sizes, variable_length);
    return 0;
}

// 0x007549B0 (.h:261)
inline int log_event_activity_attempt(int player_checksum, int time, int activity_crc, int time_played, char pct_game_complete, char activity_exit_status, int salvage, int locale_id, char difficulty, int duration, vector player_pos)
{
    if (!log_record_event(8))
    {
        return -1;
    }
    void* argv[13] = {&player_checksum, &time, &activity_crc, &time_played, &pct_game_complete, &activity_exit_status, &salvage, &locale_id, &difficulty, &duration, &player_pos.x, &player_pos.y, &player_pos.z};
    unsigned char sizes[13] = {4, 4, 4, 4, 1, 1, 4, 4, 1, 4, 4, 4, 4};
    bool variable_length[13] = {};
    log_add_event(8, 13, argv, sizes, variable_length);
    return 0;
}

// 0x007162D0 (.h:286)
inline int log_event_player_find_collectable(int player_checksum, int time, int item_type, int time_played, char pct_game_complete, int locale_id, char difficulty, int duration, vector item_pos)
{
    if (!log_record_event(9))
    {
        return -1;
    }
    void* argv[11] = {&player_checksum, &time, &item_type, &time_played, &pct_game_complete, &locale_id, &difficulty, &duration, &item_pos.x, &item_pos.y, &item_pos.z};
    unsigned char sizes[11] = {4, 4, 4, 4, 1, 4, 1, 4, 4, 4, 4};
    bool variable_length[11] = {};
    log_add_event(9, 11, argv, sizes, variable_length);
    return 0;
}

// 0x00A062A0 (.h:311)
inline int log_event_district(int player_checksum, int time, int district_crc, int time_played, char pct_game_complete, int locale_id, char difficulty, int duration, vector player_pos)
{
    if (!log_record_event(10))
    {
        return -1;
    }
    void* argv[11] = {&player_checksum, &time, &district_crc, &time_played, &pct_game_complete, &locale_id, &difficulty, &duration, &player_pos.x, &player_pos.y, &player_pos.z};
    unsigned char sizes[11] = {4, 4, 4, 4, 1, 4, 1, 4, 4, 4, 4};
    bool variable_length[11] = {};
    log_add_event(10, 11, argv, sizes, variable_length);
    return 0;
}

// 0x00A06390 (.h:337)
inline int log_event_weapon_upgrade(int player_checksum, int time, char upgrade_id, char upgrade_level, int time_played, char pct_game_complete, int locale_id, char difficulty, int duration, vector player_pos)
{
    if (!log_record_event(11))
    {
        return -1;
    }
    void* argv[12] = {&player_checksum, &time, &upgrade_id, &upgrade_level, &time_played, &pct_game_complete, &locale_id, &difficulty, &duration, &player_pos.x, &player_pos.y, &player_pos.z};
    unsigned char sizes[12] = {4, 4, 1, 1, 4, 1, 4, 1, 4, 4, 4, 4};
    bool variable_length[12] = {};
    log_add_event(11, 12, argv, sizes, variable_length);
    return 0;
}

// 0x0094C3A0 (.h:358)
inline int log_event_siege_target_destroyed(int player_checksum, int time, short seconds_left_in_round, char siege_target_index, vector position)
{
    if (!log_record_event(12))
    {
        return -1;
    }
    void* argv[7] = {&player_checksum, &time, &seconds_left_in_round, &siege_target_index, &position.x, &position.y, &position.z};
    unsigned char sizes[7] = {4, 4, 2, 1, 4, 4, 4};
    bool variable_length[7] = {};
    log_add_event(12, 7, argv, sizes, variable_length);
    return 0;
}

// 0x0074F240 (.h:388)
inline int log_event_loadout_change(int player_checksum, int time, char player_index, int old_item1_crc, int old_item2_crc, int old_item3_crc, int new_item1_crc, int new_item2_crc, int new_item3_crc, int locale_id, char difficulty, int duration, vector player_pos, char from_where)
{
    if (!log_record_event(13))
    {
        return -1;
    }
    void* argv[16] = {&player_checksum, &time, &player_index, &old_item1_crc, &old_item2_crc, &old_item3_crc, &new_item1_crc, &new_item2_crc, &new_item3_crc, &locale_id, &difficulty, &duration, &player_pos.x, &player_pos.y, &player_pos.z, &from_where};
    unsigned char sizes[16] = {4, 4, 1, 4, 4, 4, 4, 4, 4, 4, 1, 4, 4, 4, 4, 1};
    bool variable_length[16] = {};
    log_add_event(13, 16, argv, sizes, variable_length);
    return 0;
}

// 0x00B64480 (.h:415)
inline int log_event_exit_vehicle(int player_checksum, int time, char seat_index, int vehicle_type_crc, int locale_id, char difficulty, int duration, vector player_pos, int time_played, char pct_game_complete, char vehicle_exit_status)
{
    if (!log_record_event(14))
    {
        return -1;
    }
    void* argv[13] = {&player_checksum, &time, &seat_index, &vehicle_type_crc, &locale_id, &difficulty, &duration, &player_pos.x, &player_pos.y, &player_pos.z, &time_played, &pct_game_complete, &vehicle_exit_status};
    unsigned char sizes[13] = {4, 4, 1, 4, 4, 1, 4, 4, 4, 4, 4, 1, 1};
    bool variable_length[13] = {};
    log_add_event(14, 13, argv, sizes, variable_length);
    return 0;
}

// 0x00754AC0 (.h:445)
inline int log_event_wc_match(int player_checksum, char player_count, char game_mode_id, int level_crc, char round_count, int duration, int locale_id, int language, short match_id, int match_score_1, int match_score_2, int match_score_3, int match_score_4, char winner)
{
    if (!log_record_event(15))
    {
        return -1;
    }
    void* argv[14] = {&player_checksum, &player_count, &game_mode_id, &level_crc, &round_count, &duration, &locale_id, &language, &match_id, &match_score_1, &match_score_2, &match_score_3, &match_score_4, &winner};
    unsigned char sizes[14] = {4, 1, 1, 4, 1, 4, 4, 4, 2, 4, 4, 4, 4, 1};
    bool variable_length[14] = {};
    log_add_event(15, 14, argv, sizes, variable_length);
    return 0;
}

// 0x00754BE0 (.h:471)
inline int log_event_wc_round(int player_checksum, short match_id, char round, int weapon_crc, int backpack_crc, int round_score_1, int round_score_2, int round_score_3, int round_score_4, char winner)
{
    if (!log_record_event(16))
    {
        return -1;
    }
    void* argv[10] = {&player_checksum, &match_id, &round, &weapon_crc, &backpack_crc, &round_score_1, &round_score_2, &round_score_3, &round_score_4, &winner};
    unsigned char sizes[10] = {4, 2, 1, 4, 4, 4, 4, 4, 4, 1};
    bool variable_length[10] = {};
    log_add_event(16, 10, argv, sizes, variable_length);
    return 0;
}

// 0x0094C460 (.h:501)
inline int log_event_siege_destruction_snapshot(int player_checksum, short seconds_left_in_round, char pct_destruction_1, char pct_destruction_2, char pct_destruction_3, char pct_destruction_4, char pct_destruction_5, char pct_destruction_6, char pct_destruction_7, char pct_destruction_8, char pct_destruction_9, char pct_destruction_10, char pct_destruction_11, char pct_destruction_12)
{
    if (!log_record_event(17))
    {
        return -1;
    }
    void* argv[14] = {&player_checksum, &seconds_left_in_round, &pct_destruction_1, &pct_destruction_2, &pct_destruction_3, &pct_destruction_4, &pct_destruction_5, &pct_destruction_6, &pct_destruction_7, &pct_destruction_8, &pct_destruction_9, &pct_destruction_10, &pct_destruction_11, &pct_destruction_12};
    unsigned char sizes[14] = {4, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    bool variable_length[14] = {};
    log_add_event(17, 14, argv, sizes, variable_length);
    return 0;
}

// 0x0094C580 (.h:523)
inline int log_event_player_create(int player_checksum, int time, char player_index, int locale_id, int duration, vector player_pos)
{
    if (!log_record_event(18))
    {
        return -1;
    }
    void* argv[8] = {&player_checksum, &time, &player_index, &locale_id, &duration, &player_pos.x, &player_pos.y, &player_pos.z};
    unsigned char sizes[8] = {4, 4, 1, 4, 4, 4, 4, 4};
    bool variable_length[8] = {};
    log_add_event(18, 8, argv, sizes, variable_length);
    return 0;
}

// 0x00754CC0 (.h:548)
inline int log_event_morale_change(int player_checksum, int time, char new_morale_value, int time_played, char pct_game_complete, int locale_id, char difficulty, int duration, vector player_pos)
{
    if (!log_record_event(19))
    {
        return -1;
    }
    void* argv[11] = {&player_checksum, &time, &new_morale_value, &time_played, &pct_game_complete, &locale_id, &difficulty, &duration, &player_pos.x, &player_pos.y, &player_pos.z};
    unsigned char sizes[11] = {4, 4, 1, 4, 1, 4, 1, 4, 4, 4, 4};
    bool variable_length[11] = {};
    log_add_event(19, 11, argv, sizes, variable_length);
    return 0;
}

// 0x00A06480 (.h:571)
inline int log_event_player_enter_safehouse(int player_checksum, int time, char pct_game_complete, int locale_id, char difficulty, int duration, vector player_pos)
{
    if (!log_record_event(20))
    {
        return -1;
    }
    void* argv[9] = {&player_checksum, &time, &pct_game_complete, &locale_id, &difficulty, &duration, &player_pos.x, &player_pos.y, &player_pos.z};
    unsigned char sizes[9] = {4, 4, 1, 4, 1, 4, 4, 4, 4};
    bool variable_length[9] = {};
    log_add_event(20, 9, argv, sizes, variable_length);
    return 0;
}

// 0x007163C0 (.h:596)
inline int log_event_target_destroyed(int player_checksum, int time, int target_crc, int time_played, char pct_game_complete, int locale_id, char difficulty, int duration, vector player_pos)
{
    if (!log_record_event(21))
    {
        return -1;
    }
    void* argv[11] = {&player_checksum, &time, &target_crc, &time_played, &pct_game_complete, &locale_id, &difficulty, &duration, &player_pos.x, &player_pos.y, &player_pos.z};
    unsigned char sizes[11] = {4, 4, 4, 4, 1, 4, 1, 4, 4, 4, 4};
    bool variable_length[11] = {};
    log_add_event(21, 11, argv, sizes, variable_length);
    return 0;
}

// 0x00A06560 (.h:627)
inline int log_event_human_killed(int player_checksum, int time, char victim_index, vector victim_pos, int victim_weapon_crc, char attacker_index, vector attacker_pos, int attacker_weapon_crc, int locale_id, char difficulty, int duration, char attacker_in_cover, int player_playing_crc, int time_played, char pct_game_complete)
{
    if (!log_record_event(22))
    {
        return -1;
    }
    void* argv[19] = {&player_checksum, &time, &victim_index, &victim_pos.x, &victim_pos.y, &victim_pos.z, &victim_weapon_crc, &attacker_index, &attacker_pos.x, &attacker_pos.y, &attacker_pos.z, &attacker_weapon_crc, &locale_id, &difficulty, &duration, &attacker_in_cover, &player_playing_crc, &time_played, &pct_game_complete};
    unsigned char sizes[19] = {4, 4, 1, 4, 4, 4, 4, 1, 4, 4, 4, 4, 4, 1, 4, 1, 4, 4, 1};
    bool variable_length[19] = {};
    log_add_event(22, 19, argv, sizes, variable_length);
    return 0;
}

// 0x0094C640 (.h:651)
inline int log_event_backpack_used(int player_checksum, int time, char player_index, int locale_id, char difficulty, int duration, vector player_pos, char player_xp_level)
{
    if (!log_record_event(23))
    {
        return -1;
    }
    void* argv[10] = {&player_checksum, &time, &player_index, &locale_id, &difficulty, &duration, &player_pos.x, &player_pos.y, &player_pos.z, &player_xp_level};
    unsigned char sizes[10] = {4, 4, 1, 4, 1, 4, 4, 4, 4, 1};
    bool variable_length[10] = {};
    log_add_event(23, 10, argv, sizes, variable_length);
    return 0;
}

// 0x0094C720 (.h:671)
inline int log_event_team_score(int player_checksum, int time, char team_id, short score)
{
    if (!log_record_event(24))
    {
        return -1;
    }
    void* argv[4] = {&player_checksum, &time, &team_id, &score};
    unsigned char sizes[4] = {4, 4, 1, 2};
    bool variable_length[4] = {};
    log_add_event(24, 4, argv, sizes, variable_length);
    return 0;
}

// 0x0094C790 (.h:694)
inline int log_event_damage_control_target_changed_ownership(int player_checksum, int time, short seconds_left_in_round, char damage_control_target_index, char new_owner_team_id, int duration, vector position)
{
    if (!log_record_event(25))
    {
        return -1;
    }
    void* argv[9] = {&player_checksum, &time, &seconds_left_in_round, &damage_control_target_index, &new_owner_team_id, &duration, &position.x, &position.y, &position.z};
    unsigned char sizes[9] = {4, 4, 2, 1, 1, 4, 4, 4, 4};
    bool variable_length[9] = {};
    log_add_event(25, 9, argv, sizes, variable_length);
    return 0;
}

// 0x00A066F0 (.h:719)
inline int log_event_salvage(int player_checksum, int time, char salvage_value, int time_played, char pct_game_complete, int locale_id, char difficulty, int duration, vector player_pos)
{
    if (!log_record_event(26))
    {
        return -1;
    }
    void* argv[11] = {&player_checksum, &time, &salvage_value, &time_played, &pct_game_complete, &locale_id, &difficulty, &duration, &player_pos.x, &player_pos.y, &player_pos.z};
    unsigned char sizes[11] = {4, 4, 1, 4, 1, 4, 1, 4, 4, 4, 4};
    bool variable_length[11] = {};
    log_add_event(26, 11, argv, sizes, variable_length);
    return 0;
}

// 0x0094C870 (.h:741)
inline int log_event_match_player(int player_checksum, char player_index, int64_t player_uid, char player_team_id, int locale_id, int language)
{
    if (!log_record_event(27))
    {
        return -1;
    }
    void* argv[6] = {&player_checksum, &player_index, &player_uid, &player_team_id, &locale_id, &language};
    unsigned char sizes[6] = {4, 1, 8, 1, 4, 4};
    bool variable_length[6] = {};
    log_add_event(27, 6, argv, sizes, variable_length);
    return 0;
}

// 0x0094C920 (.h:760)
inline int log_start_event_mp_round(int player_checksum, int time, char round)
{
    if (!log_record_event(28))
    {
        return 0;
    }
    void* argv[3] = {&player_checksum, &time, &round};
    unsigned char sizes[3] = {4, 4, 1};
    bool variable_length[3] = {};
    return log_start_continuous_event(28, 3, argv, sizes, variable_length);
}

// 0x0094C990 (.h:775)
inline unsigned int log_stop_event_mp_round(unsigned int event_handle)
{
    if (!log_record_event(28))
    {
        return 0;
    }
    return log_stop_continuous_event(event_handle);
}

// 0x0094C9B0 (.h:792)
inline int log_start_event_backpack_equipped(int player_checksum, int time, char player_index, int backpack_crc, char player_xp_level)
{
    if (!log_record_event(29))
    {
        return 0;
    }
    void* argv[5] = {&player_checksum, &time, &player_index, &backpack_crc, &player_xp_level};
    unsigned char sizes[5] = {4, 4, 1, 4, 1};
    bool variable_length[5] = {};
    return log_start_continuous_event(29, 5, argv, sizes, variable_length);
}

// 0x0094CA50 (.h:807)
inline unsigned int log_stop_event_backpack_equipped(unsigned int event_handle)
{
    if (!log_record_event(29))
    {
        return 0;
    }
    return log_stop_continuous_event(event_handle);
}

// 0x0094CA70 (.h:828)
inline int log_start_event_mp_match(int player_checksum, int time, char player_count, int level_crc, char game_mode_id, char round_count, short round_time_limit, short round_point_limit, char friendly_fire)
{
    if (!log_record_event(30))
    {
        return 0;
    }
    void* argv[9] = {&player_checksum, &time, &player_count, &level_crc, &game_mode_id, &round_count, &round_time_limit, &round_point_limit, &friendly_fire};
    unsigned char sizes[9] = {4, 4, 1, 4, 1, 1, 2, 2, 1};
    bool variable_length[9] = {};
    return log_start_continuous_event(30, 9, argv, sizes, variable_length);
}

// 0x0094CB40 (.h:843)
inline unsigned int log_stop_event_mp_match(unsigned int event_handle)
{
    if (!log_record_event(30))
    {
        return 0;
    }
    return log_stop_continuous_event(event_handle);
}

// 0x00A067E0 (.h:862)
inline int log_start_event_jetpack_used(int player_checksum, int time, char player_index, char player_xp_level, vector player_pos, int time_played, char pct_game_complete)
{
    if (!log_record_event(31))
    {
        return 0;
    }
    void* argv[9] = {&player_checksum, &time, &player_index, &player_xp_level, &player_pos.x, &player_pos.y, &player_pos.z, &time_played, &pct_game_complete};
    unsigned char sizes[9] = {4, 4, 1, 1, 4, 4, 4, 4, 1};
    bool variable_length[9] = {};
    return log_start_continuous_event(31, 9, argv, sizes, variable_length);
}

// 0x00A068B0 (.h:877)
inline unsigned int log_stop_event_jetpack_used(unsigned int event_handle)
{
    if (!log_record_event(31))
    {
        return 0;
    }
    return log_stop_continuous_event(event_handle);
}
