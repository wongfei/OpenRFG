// Original: volition/rfg/code/vehicle_ai/lane_pathfind.h
// Implementation: volition/rfg/code/vehicle_ai/lane_pathfind.cpp
//
// Lane pathfinding: the closest (oriented) lane to a point, adjacent lanes, and paths over the lane graph (lane path
// node lists).
#pragma once

#include "volition/vlib/code/math/vector3.h"

struct lane_path_node;
struct lane_spline;

// size 0x2C
struct lane_path_info
{
    lane_path_info(const lane_spline* _start_lane, const lane_spline** _end_lanes, int _num_end_lanes, const vector& _goal_pos, int _lane_include_filter, int _lane_exclude_filter); // 0x00BC3A00 (:91)
    lane_path_info(const lane_spline* _start_lane, const lane_spline* _end_lane, int _lane_include_filter, int _lane_exclude_filter); // 0x00BC39B0 (:76)

    const lane_spline* start_lane; // 0x00
    const lane_spline** end_lanes_multi; // 0x04
    const lane_spline* end_lane_single; // 0x08
    int num_end_lanes; // 0x0C
    int lane_include_filter; // 0x10
    int lane_exclude_filter; // 0x14
    float max_path_length; // 0x18
    bool gps_path; // 0x1C
    vector goal_pos; // 0x20
};

// size 0x18
struct closest_lane_search_info
{
    closest_lane_search_info(const vector& _search_pos, const vector* _fvec, float _max_refind_search_dist); // 0x00BC3D30 (:795)

    const vector& search_pos; // 0x00
    unsigned int include_flags; // 0x04
    unsigned int exclude_flags; // 0x08
    const vector* fvec; // 0x0C
    float max_refine_search_dist; // 0x10
    float best_dist_sqr; // 0x14
};

// size 0x18
struct closest_spline_info
{
    closest_spline_info(float _min_dot, const vector& _pos, const vector* _fvec); // 0x00BC3E90 (:959)

    float min_dot; // 0x00
    const vector& pos; // 0x04
    const vector* fvec; // 0x08
    float best_dist_sqr; // 0x0C
    const lane_spline* lsp; // 0x10
    float parametric_pos; // 0x14
};

void lane_path_level_init_pre(); // 0x00BC3A60 (:143)
void free_lane_path_node(lane_path_node** path, lane_path_node* node); // 0x00BC3B60 (:186)
void lane_pathnode_append_lane(lane_path_node** path, const lane_spline* lane); // 0x00BC3BE0 (:203)
void vehicle_free_lane_path(lane_path_node** path); // 0x00BC3C60 (:376)
lane_path_node* lane_pathfind_get_path(const lane_path_info& pathfind_info); // 0x00BD4590 (:576)
const lane_spline* lane_pathfind_get_closest_lane(const vector& in_pos, float radius, int include_flags, int exclude_flags); // 0x00BCD070 (:633)
bool lane_pathfind_get_closest_oriented_lane(closest_spline_info& spline_info, lane_spline** lanes, int num_lanes); // 0x00BCF560 (:673)
int lane_pathfind_get_adjacent_lanes(const lane_spline* start_lane, const lane_spline** adjacent_lanes); // 0x00BCD180 (:725)
int find_smallest_multiple_of_10_radius_containing_splines(const closest_lane_search_info& search_info); // 0x00BC3D60 (:808)
const lane_spline* lane_pathfind_get_closest_oriented_traffic_lane_slow(closest_lane_search_info& search_info); // 0x00BCF720 (:862)
float lane_get_path_length(const lane_path_node* path, bool gps_search); // 0x00BC3E10 (:922)
float lane_get_path_length(const lane_path_node* path, const vector& start, const vector& end, bool gps_search); // 0x00BCD210 (:947)
