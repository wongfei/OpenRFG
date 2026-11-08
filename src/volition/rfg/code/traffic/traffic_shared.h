// Original: volition/rfg/code/traffic/traffic_shared.h
//
// The serialized traffic lane splines (et_ptr_offset links to the next, previous, yield-to and neighbouring lanes) and
// the per-spline vehicle lists. Placement inferred.
#pragma once

#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/endian_type.h"

// size 0x18
struct lane_spline_base
{
    lane_spline_base(); // 0x00BCA290

    et_ptr_offset<vector,0> knots; // 0x00
    et_ptr_offset<vector,0> in_handles; // 0x04
    et_ptr_offset<vector,0> out_handles; // 0x08
    short num_knots; // 0x0C
    short spline_flags; // 0x0E
    float arc_length[2]; // 0x10
};

// size 0xC
struct spline_vehicles
{
    spline_vehicles(); // 0x00B601F0

    et_ptr_offset<spline_vehicles,0> next; // 0x00
    et_ptr_offset<spline_vehicles,0> prev; // 0x04
    unsigned int v_handle; // 0x08
};

// size 0x5C
struct lane_spline : public lane_spline_base
{
    vector bmin; // 0x18
    vector bmax; // 0x24
    float speed_limit; // 0x30
    short num_next_splines; // 0x34
    short num_previous_splines; // 0x36
    short num_yield_to_lanes; // 0x38
    short seg_count; // 0x3A
    short exit_state; // 0x3C
    et_ptr_offset<et_ptr_offset<lane_spline,0>,0> next_splines; // 0x40
    et_ptr_offset<et_ptr_offset<lane_spline,0>,0> previous_splines; // 0x44
    et_ptr_offset<et_ptr_offset<lane_spline,0>,0> yield_to_lanes; // 0x48
    et_ptr_offset<lane_spline,0> right; // 0x4C
    et_ptr_offset<lane_spline,0> left; // 0x50
    et_ptr_offset<vector,0> approx_points; // 0x54
    et_ptr_offset<spline_vehicles,0> vehicle_list; // 0x58
};

struct lane_search_node;
struct rbb_tree;

// size 0xC
struct intersection_controller_stage
{
    int num_active_lanes; // 0x00
    et_ptr_offset<et_ptr_offset<lane_spline,0>,0> active_lanes; // 0x04
    int duration_ms; // 0x08
};

// size 0x10
struct intersection_controller_old
{
    int num_stages; // 0x00
    et_ptr_offset<intersection_controller_stage,0> stages; // 0x04
    int current_stage; // 0x08
    int stage_change_timestamp; // 0x0C: a timestamp in the PDB; only the loader sees this type
};

// size 0x68: the header of the packed traffic file.
struct traffic_data
{
    int num_lane_splines; // 0x00
    et_ptr_offset<lane_spline,0> lane_splines; // 0x04
    int num_lane_spline_knots; // 0x08
    et_ptr_offset<vector,0> lane_spline_knots; // 0x0C
    int num_lane_spline_in_handles; // 0x10
    et_ptr_offset<vector,0> lane_spline_in_handles; // 0x14
    int num_lane_spline_out_handles; // 0x18
    et_ptr_offset<vector,0> lane_spline_out_handles; // 0x1C
    int num_lane_spline_next_ptrs; // 0x20
    et_ptr_offset<et_ptr_offset<lane_spline,0>,0> lane_spline_next_ptrs; // 0x24
    int num_lane_search_nodes; // 0x28
    et_ptr_offset<lane_search_node,0> lane_search_tree; // 0x2C
    int num_lane_search_tree_links; // 0x30
    et_ptr_offset<et_ptr_offset<lane_search_node,0>,0> lane_search_tree_links; // 0x34
    int num_lane_search_work_nodes; // 0x38
    et_ptr_offset<et_ptr_offset<lane_search_node,0>,0> lane_search_work_nodes; // 0x3C
    int num_intersection_controllers; // 0x40
    et_ptr_offset<intersection_controller_old,0> intersection_controllers; // 0x44
    int num_intersection_controller_stages; // 0x48
    et_ptr_offset<intersection_controller_stage,0> intersection_controller_stages; // 0x4C
    int num_lane_spline_ptrs; // 0x50
    et_ptr_offset<et_ptr_offset<lane_spline,0>,0> lane_spline_ptrs; // 0x54
    int num_lane_spline_previous_ptrs; // 0x58
    et_ptr_offset<et_ptr_offset<lane_spline,0>,0> lane_spline_previous_ptrs; // 0x5C
    int lane_rbb_tree_offset; // 0x60
    et_ptr_offset<rbb_tree,0> lane_rbb_tree; // 0x64
};
