// Original: volition/rfg/code/multi/multi_game/multi_game_rubberbanding.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_rubberbanding.cpp
//
// Rubberbanding: handicaps by player skill level, from the rubberbanding table.
#pragma once

#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/vlib/code/math/crc.h"

enum multi_rubberbanding_stats
{
    MRS_NONE = -1,
    MRS_HEALTH = 0,
    MRS_HEALTH_REGEN = 1,
    MRS_AMMO_PICKUP = 2,
    MRS_MOVE_SPEED = 3,
    MRS_MOVE_ACCELERATION = 4,
    MRS_AUTO_AIM = 5,
    MRS_AIM_ASSIST = 6,
    MRS_RESPAWN_TIME = 7,
    MRS_BACKPACK_RECHARGE = 8,
    MRS_RELOAD_SPEED = 9,
    NUM_MULTI_RUBBERBANDING_STATS = 10,
};

// size 0x24
struct multi_rubberbanding_stat
{
    multi_rubberbanding_stat(); // 0x00961260 (:33)

    float values[8]; // 0x00
    bool valid; // 0x20
};

// size 0x19C
struct multi_rubberbanding_level_info
{
    multi_rubberbanding_level_info(const char* internal_name, const char* disp_name); // 0x00961270 (:40)

    checksum_stri_pair internal_name_pair; // 0x00
    const char* display_name; // 0x08
    float levels[8]; // 0x0C
    int num_levels; // 0x2C
    multi_rubberbanding_stat stats[10]; // 0x30
    bool valid; // 0x198
};

void unused_dcf_mp_reload_rubberbanding_table(); // 0x009844C0 (:231)
void multi_game_rubberbanding_init(); // 0x009844E0 (:243)
void multi_game_rubberbanding_process(); // 0x009B2200 (:248)
void multi_game_rubberbanding_render(); // 0x009612D0 (:264)
const char* multi_game_rubberbanding_get_level_internal_name(multi_rubberbanding_levels level); // 0x009612E0 (:286)
const char* multi_game_rubberbanding_get_level_display_name(multi_rubberbanding_levels level); // 0x00961300 (:292)
multi_rubberbanding_levels multi_game_rubberbanding_get_level(const char* level_name); // 0x00961320 (:298)
const char* multi_game_rubberbanding_get_stat_internal_name(multi_rubberbanding_stats stat); // 0x00961360 (:312)
float multi_game_rubberbanding_get_player_score(unsigned int multi_game_data_handle); // 0x009A1630 (:320)
float multi_game_rubberbanding_get_stat_value(unsigned int multi_game_data_handle, multi_rubberbanding_stats stat); // 0x009A1910 (:460)
