// Original: volition/rfg/code/vehicle_ai/vehicle_repulsor.h
// Implementation: volition/rfg/code/vehicle_ai/vehicle_repulsor.cpp
//
// Obstacle avoidance of AI vehicles: lateral repulsion from nearby objects ahead of the vehicle (compound repulsors of
// the objects' oriented boxes). Placement of the types inferred.
#pragma once

#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/vlib/code/math/plane.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

class object;
class vehicle;

// size 0x58
struct repulsor_lateral_info
{
    repulsor_lateral_info(); // 0x00B64620

    plane plane_left; // 0x00
    plane plane_right; // 0x10
    plane box_left; // 0x20
    plane box_right; // 0x30
    plane box_back; // 0x40
    float angle_with_fvec; // 0x50
    float start_width_offset; // 0x54
};

// size 0xB0
struct vehicle_repulsor_info
{
    vehicle_repulsor_info(); // 0x00B76860

    repulsor_lateral_info lateral_info; // 0x00
    unsigned char affected_by_repulsors; // 0x58
    unsigned char num_path_repulsors; // 0x59
    unsigned int vpath_repulsor_handles[10]; // 0x5C
    timestamp vpath_repulsor_time[10]; // 0x84
    timestamp path_repulsors_collection_time; // 0xAC
};

// size 0x50
struct veh_compound_repulsor
{
    veh_compound_repulsor(); // 0x00BC89D0

    object* objp; // 0x00
    repulsor_oriented_bbox oriented_bbox; // 0x04
    vector pos; // 0x40
    float radius; // 0x4C
};

void vehicle_set_repulsor_lateral_limits(vehicle* vp); // 0x00BC7DA0 (:386)
void vpf_calculate_turn_to_target(vector& drive_to_pos, vehicle* vp, const vector& dest, unsigned int* ignore_handle, int num_ignore_handle, bool ignore_humans, bool ignore_buildings, bool ignore_vehicles); // 0x00BD2FB0 (:2044)
void vehicle_update_collector_info(vehicle* vp); // 0x00BC89E0 (:2360)
