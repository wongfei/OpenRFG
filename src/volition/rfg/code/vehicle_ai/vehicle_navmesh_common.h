// Original: volition/rfg/code/vehicle_ai/vehicle_navmesh_common.h
// Implementation: volition/rfg/code/vehicle_ai/vehicle_navmesh_common.cpp
//
// Nav mesh driving: creating, updating and following vehicle paths over the vehicle nav mesh. Placement of
// navmesh_lane_spline_node inferred.
#pragma once

#include <stdint.h>
#include "volition/rfg/code/traffic/traffic_shared.h"

class pf_calc_params;
class vector;
class vehicle;
class vehicle_ai;
struct vehicle_path_info;

// size 0x20
struct navmesh_lane_spline_node
{
    navmesh_lane_spline_node(); // 0x00BCEA90

    navmesh_lane_spline_node* next; // 0x00
    navmesh_lane_spline_node* prev; // 0x04
    lane_spline_base lane; // 0x08
};

void vehicle_navmesh_common_init_path_follow(vehicle_ai* vai, navmesh_lane_spline_node* path); // 0x00BCEA00 (:179)
navmesh_lane_spline_node* vehicle_navmesh_common_create_path(vehicle_ai* vai, const pf_calc_params& params, vehicle_path_info& path_info); // 0x00BD17D0 (:224)
bool vehicle_navmesh_common_update_path(vehicle_ai* vai, const pf_calc_params& params, vehicle_path_info& path_info); // 0x00BD1A10 (:302)
uint64_t vehicle_navmesh_common_update_nav_cell_handle(const vector& pos, uint64_t nav_cell_handle_hint); // 0x00BC5A60 (:325)
void vehicle_navmesh_common_follow_path(vehicle* vp); // 0x00BD5530 (:348)
float get_spline_path_length(navmesh_lane_spline_node* path); // 0x00BC5AD0 (:652)
