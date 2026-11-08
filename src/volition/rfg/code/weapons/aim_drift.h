// Original: volition/rfg/code/weapons/aim_drift.h
// Implementation: volition/rfg/code/weapons/aim_drift.cpp
//
// Aim drift profiles (aim_drift table): how an NPC's aim error grows and decays, with penalties and bonuses for the
// situation (target speed, cover, offscreen, kneeling, return fire).
#pragma once


struct xml_element;

// size 0x88
struct aim_drift_profile
{
    char name[64]; // 0x00
    int min_change_direction_time; // 0x40
    int max_change_direction_time; // 0x44
    int acquire_time; // 0x48
    float max_acquire_penalty; // 0x4C
    float offscreen_penalty; // 0x50
    float player_penalty; // 0x54
    float player_in_cover_penalty; // 0x58
    float my_speed_penalty; // 0x5C
    float target_speed_penalty; // 0x60
    float kneeling_bonus_pct; // 0x64
    float return_fire_bonus_pct; // 0x68
    float min_horizontal_error; // 0x6C
    float max_horizontal_error; // 0x70
    float max_grow_rate; // 0x74
    float max_decay_rate; // 0x78
    float pos_vertical_scale; // 0x7C
    float neg_vertical_scale; // 0x80
    float max_vertical_error_dist; // 0x84
};

extern int Num_aim_drift_profiles; // 0x03882CB0

aim_drift_profile* aim_drift_profile_from_name_internal(const char* name); // 0x00BD8CB0 (:39)
bool parse_aim_drift_profile(aim_drift_profile* profile, const xml_element* profile_el); // 0x00BD8D00 (:56)
void aim_drift_read_table(bool refresh, int dlc_id); // 0x00BD8EF0 (:109)
const aim_drift_profile* aim_drift_profile_from_name(const char* name); // 0x00BD90C0 (:180)
