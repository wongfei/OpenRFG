// Original: volition/rfg/code/gameplay/activities/raid_common.h
// Implementation: volition/rfg/code/gameplay/activities/raid_common.cpp
//
// Raid definitions shared by the raid activities: loading the raid table (and DLC entries), raid handles and lookups,
// reinforcement waypoints, exit points and the defending team.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_enum.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/varray.h"

class base_raid_definition_data;
struct nav_cell;
class npc;
class object;
class object_path_road;
struct raid_exit_cell;

// size 0x38
class road_evaluation_set
{
public:
    road_evaluation_set(); // 0x0075B6A0 (:151)
    void reset(); // 0x0075B6B0 (:160)

    const object_path_road* prp; // 0x00
    float road_distance; // 0x04
    vector nearest_point; // 0x08
    bool at_min_point; // 0x14
    bool at_max_point; // 0x15
    float min_dist; // 0x18
    float max_dist; // 0x1C
    vector min_point; // 0x20
    vector max_point; // 0x2C
};

void load_raid_data(int dlc_id); // 0x0079B140 (:227)
void raid_data_unload_dlc(); // 0x0079B2F0 (:290)
bool find_seed_nav_cells(const object* objp, float raid_radius, farray<nav_cell const*,8>& nav_cell_seeds, const base_array<unsigned int>& raid_target_list); // 0x007897D0 (:598)
void raid_common_system_init(); // 0x0079B3F0 (:1053)
int activity_raid_find_raid_handle(const base_raid_definition_data* raid_info); // 0x00771B60 (:1061)
const base_raid_definition_data* raid_get_def_by_name(const char* raid_name); // 0x00789FF0 (:1108)
void raid_build_reinforcement_waypoints(const object* raid_objp, farray<vector,64>& external_dest_list, const vector& center, float radius, const base_array<unsigned int>& raid_target_list); // 0x0078A070 (:1130)
bool activity_raid_go_to_raid(const npc* np); // 0x0075B950 (:1141)
void raid_build_exit_points(const object* objp, float raid_radius, const vector& target_pos, float demo_radius, farray<raid_exit_cell,128>& exit_cell_list, const base_array<unsigned int>& raid_target_list); // 0x0078A0C0 (:1177)
raid_team_states raid_common_resolve_state(const char* state_name); // 0x0075BA60 (:1255)
human_teams raid_get_defending_team(const base_raid_definition_data* data); // 0x0075BAA0 (:1271)
