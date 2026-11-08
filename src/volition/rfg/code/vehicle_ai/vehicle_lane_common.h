// Original: volition/rfg/code/vehicle_ai/vehicle_lane_common.h
// Implementation: volition/rfg/code/vehicle_ai/vehicle_lane_common.cpp
//
// Lane spline math: positions, tangents, curvature and distances along lane splines, closest points.
#pragma once


class cubic_bezier_spline;
struct lane_spline_base;
class vector;

float spline_common_get_dist_remaining_on_lane_spline(const lane_spline_base* ls, const vector& pos); // 0x00BCE430 (:19)
float spline_common_get_dist_remaining_on_lane_spline(const lane_spline_base* ls, float u); // 0x00BCE4A0 (:31)
float spline_common_get_closest_point_on_spline(const lane_spline_base* ls, const vector& in_pos, vector& spline_pos); // 0x00BCE500 (:40)
float spline_common_get_dist_to_point(const lane_spline_base* ls, float u); // 0x00BCE570 (:54)
float spline_common_get_closest_point_and_tangent_on_spline(const lane_spline_base* ls, const vector& in_pos, vector& spline_pos, vector& tangent); // 0x00BCE5D0 (:62)
float spline_common_get_spline_parametric_pos_closest_to_point(const lane_spline_base* ls, const vector& in_pos); // 0x00BCE650 (:79)
void spline_common_get_position_on_spline(const lane_spline_base* ls, float u, vector& pos); // 0x00BCE690 (:88)
void spline_common_get_tangent_on_spline(const lane_spline_base* ls, float u, vector& tan); // 0x00BCE6E0 (:96)
void spline_common_get_position_and_tangent_on_spline(const lane_spline_base* ls, float u, vector& pos, vector& tan); // 0x00BCE730 (:104)
void spline_common_get_second_derivative_on_spline(const lane_spline_base* ls, float u, vector& sec_dir); // 0x00BCE7A0 (:113)
float spline_common_get_parametric_pos_for_dist(const cubic_bezier_spline& cbs, float desired_dist, const float* spline_length); // 0x00BC56D0 (:128)
float spline_common_get_point_fixed_distance_on_lane_spline(const lane_spline_base* lsb, float desired_dist, vector& target_pos); // 0x00BCE7F0 (:189)
float spline_common_get_curvature_on_lane_spline(const lane_spline_base* ls, float u); // 0x00BCE880 (:204)
