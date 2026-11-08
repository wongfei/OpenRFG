// Original: volition/rfg/code/camera/camera_transition.h
// Implementation: volition/rfg/code/camera/camera_transition.cpp
//
// Camera transitions: linear and spline blends from the current camera to the target position, orientation and fov.
#pragma once


class cubic_bezier_spline;
class matrix;
class vector;

void camera_trans_level_init_post(); // 0x006C7AA0 (:256)
void camera_trans_process(vector* out_pos, matrix* out_orient, float* out_fov, const vector& to_pos, const matrix& to_orient, const float& to_fov, float step_time); // 0x006C7AC0 (:274)
void camera_trans_start_linear_transition(float time_sec, unsigned int flags, bool start_at_zero); // 0x006C7B50 (:297)
void camera_trans_start_spline_transition(cubic_bezier_spline* spline, float* approx_segment_lengths, float spline_speed, void (*update_callback)(const vector&, const matrix&), bool in); // 0x006C7BD0 (:331)
bool camera_trans_active(); // 0x006C7C80 (:354)
void camera_trans_abort(); // 0x006C7D00 (:389)
