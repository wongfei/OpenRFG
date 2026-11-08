// Original: volition/rfg/code/gameplay/gameplay.h
// Implementation: volition/rfg/code/gameplay/gameplay.cpp
//
// Top-level gameplay systems (gameplay_do_frame and their init/shutdown).
#pragma once

#include "volition/rfg/code/objects/level_object_properties_shared.h"

enum gameplay_sfx
{
    GSFX_INVALID = -1,
    GSFX_WRECKING_CREW_FIRED_WEAPON = 0,
    GSFX_WRECKING_CREW_NORMAL_BARREL_DESTROYED = 1,
    GSFX_WRECKING_CREW_IMPORTANT_BARREL_DESTROYED = 2,
    GSFX_WRECKING_CREW_ROUND_OVER = 3,
    GSFX_WRECKING_CREW_ROUND_OVER_STAMP = 4,
    GSFX_WRECKING_CREW_MATCH_OVER = 5,
    GSFX_COUNTDOWN_TICK = 6,
    GSFX_GO = 7,
    GSFX_TIMER_START = 8,
    GSFX_ONE_MINUTE_REMAINING = 9,
    GSFX_TEN_SECONDS_REMAINING = 10,
    GSFX_LOW_TIME_TICK = 11,
    NUM_GAMEPLAY_SFX = 12,
};
class player;
struct weapon_info;

void gameplay_system_init(); // 0x007B7180 (:119)
void gameplay_system_shutdown(); // 0x0075E5D0 (:186)
void gameplay_level_shutdown(); // 0x0079D540 (:196)
void gameplay_level_init_pre(); // 0x007B4760 (:208)
void gameplay_level_init_post(); // 0x007B8A60 (:281)
void gameplay_level_init_spawn(); // 0x0075E600 (:313)
void gameplay_do_frame(); // 0x007B97F0 (:323)
bool gameplay_should_run_stress(); // 0x007746E0 (:372)
bool gameplay_can_player_attack(const weapon_info* w_info, damaged_by_types attack_type); // 0x00774710 (:387)
void gameplay_register_player_attack(player* pp, const weapon_info* w_info, damaged_by_types attack_type, bool detonating_charges); // 0x007747A0 (:410)
void gameplay_play_sfx(gameplay_sfx sfx); // 0x0075E610 (:427)
