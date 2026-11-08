// Original: volition/rfg/code/vehicle_ai/vehicle_navmesh_path_create.h
// Implementation: volition/rfg/code/vehicle_ai/vehicle_navmesh_path_create.cpp
//
// Vehicle nav mesh paths: turning a nav mesh path into Bezier lane spline nodes (Catmull-Rom and B-spline control
// points), direct paths between points, flattening.
#pragma once

#include <stdint.h>
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/vlib/code/math/vector3.h"

struct navmesh_lane_spline_node;
struct path_node;
class pf_calc_params;
class pf_calc_point;
class vehicle;
struct vehicle_path_info;

// size 0x30
struct smooth_path_point_info
{
    smooth_path_point_info(); // 0x00BC7500

    vector pos; // 0x00
    vector dir_to_next; // 0x0C
    uint64_t m_navcell_handle; // 0x18
    vector desired_delta_pos; // 0x20
    bool jumping; // 0x2C
    bool bridge; // 0x2D
};

navmesh_lane_spline_node* get_free_ls_node(); // 0x00BC5B10 (:136)
void convert_catmull_rom_to_bezier_spline_control_points(vector* cr_control_pts, vector* bez_control_pts, int num_cr_control_pts); // 0x00BC6F60 (:513)
void vehicle_navmesh_lane_spline_system_init(); // 0x00BD5C80 (:562)
void vehicle_navmesh_lane_spline_level_init_pre(); // 0x00BC7310 (:581)
void vehicle_free_ls_node(navmesh_lane_spline_node** path, navmesh_lane_spline_node* node); // 0x00BC73C0 (:601)
void vehicle_free_individual_ls_node(navmesh_lane_spline_node* node); // 0x00BC7440 (:613)
void vehicle_free_ls_path(navmesh_lane_spline_node** path); // 0x00BC7470 (:627)
navmesh_lane_spline_node* vehicle_navmesh_setup_2pt_path_segment(const vector& pt0, const vector& pt1, bool force_success); // 0x00BCEAA0 (:644)
navmesh_lane_spline_node* create_cubic_bezier_path_from_bspline_control_points(vector* control_pts, int num_control_points, int control_pts_size); // 0x00BCF0D0 (:772)
navmesh_lane_spline_node* vehicle_get_navmesh_path_internal(const pf_calc_params& params, vehicle_path_info& path_info, vehicle* vp); // 0x00BCF0F0 (:788)
navmesh_lane_spline_node* vehicle_generate_direct_path_between_points(int num_points, const pf_calc_point* points, vehicle_path_info& path_info, vehicle* vp); // 0x00BCF480 (:964)
void vehicle_path_flatten(path_node** path, nav_cell_traversal_rule traversal_rule); // 0x00BCA2A0 (:1004)
