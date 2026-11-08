// Original: volition/rfg/code/ai/combat/combat_maneuver.h
// Implementation: volition/rfg/code/ai/combat/combat_maneuver.cpp
//
// Combat maneuvers: retreat and spacing directions, dodge end points, vehicle retreat positions, obstructed paths and
// crossing the player's line of fire.
#pragma once

#include <stdint.h>
#include "volition/rfg/code/ai/ai_types.h"
#include "volition/vlib/code/math/vector3.h"

class human;
class npc;
class vehicle;

// size 0x14
struct ally_target_info
{
    ally_target_info(); // 0x005E5C50

    const human* ally; // 0x00
    vector target; // 0x04
    bool has_target; // 0x10
};

bool combat_maneuver_evaluate_dodge_endpoint(const npc* np, const vector& dodge_end, vector* adjusted_point); // 0x00677360 (:1149)
bool combat_maneuver_get_indoor_retreat_dir(vector& dir, vector& end_pos, const npc* np, const vector& desired_dir, unsigned int* end_navpoint_handle); // 0x005E5860 (:1255)
ai_results combat_maneuver_get_retreat_dir(vector& dir, vector& end_pos, const npc* np, const vector& desired_dir, float min_dist, float max_dist, uint64_t* end_nav_cell); // 0x00623FC0 (:1367)
ai_results combat_maneuver_get_retreat_dir_unified(vector& dir, vector& end_pos, const npc* np, const vector& desired_dir, float min_dist, float max_dist, int attempt, uint64_t* end_nav_cell); // 0x00696830 (:1422)
ai_results combat_maneuver_get_retreat_dir(vector& dir, vector& end_pos, const vehicle* vp, const vector& desired_dir, float min_dist, float max_dist); // 0x005E5A60 (:1484)
bool combat_maneuver_get_vehicle_retreat_pos(vector& end_pos, const vehicle* vp, const human* enemy, float min_travel_dist, float max_travel_dist, bool light_structures); // 0x00659160 (:1526)
bool combat_maneuver_get_spacing_move_dir(vector& force, const npc* np, const human* target); // 0x00677510 (:1578)
bool combat_maneuver_is_path_obstructed(const npc* np, const vector& dest, vector* new_pos); // 0x006778B0 (:1653)
bool combat_maneuver_path_crosses_player_line_of_fire(const npc* np, const vector& dest, float* intersection_dist_sqr); // 0x005E5C60 (:1764)
