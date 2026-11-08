// Original: volition/rfg/code/ai/civilian/ambient_common.h
// Implementation: volition/rfg/code/ai/civilian/ambient_common.cpp
//
// Shared ambient driving helpers: road waypoints and orientations along road segments. Placement of
// ambient_road_info inferred.
#pragma once

#include "volition/rfg/code/spawn/ambient_spawn_manager.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/varray.h"

class matrix;
class object_path_road;
class road_segment_info;
struct road_spline_identifier;
class vehicle;

// size 0x14
struct ambient_road_info
{
    ambient_road_info(); // 0x005E0C10

    const object_path_road* road; // 0x00
    int destination_index; // 0x04
    vector destination_pos; // 0x08
};

bool find_route_start_point(road_spline_identifier& start_road_id, const vehicle* vp, const base_array<road_segment_info>& last_road_route); // 0x00653A40
bool build_new_waypoints_despawn(const road_spline_identifier& start_road_id, farray<road_segment_info, 8>& spawn_road_list, const base_array<road_segment_info>& last_road_route); // 0x006535C0 (:674)
bool build_orient_from_segment(const road_segment_info& road_segment, matrix& spawn_orient); // 0x00610E60 (:895)
bool ambient_build_road_waypoints(const vehicle* vp, farray<road_segment_info,8>& spawn_road_list, ped_node_select_modes desired_mode, const base_array<road_segment_info>& last_road_route); // 0x00653B40 (:1250)
bool ambient_build_road_waypoints(farray<road_segment_info,8>& spawn_road_list, matrix& spawn_orient, object_path_road* root_road, int start_index); // 0x00653BA0 (:1283)
