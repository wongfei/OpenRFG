// Original: volition/rfg/code/camera/camera_ms.h
// Implementation: volition/rfg/code/camera/camera_ms.cpp
//
// The mini script camera mode: scripted camera moves (positions, targets and transitions). Placement of
// mini_script_params inferred.
#pragma once

#include "volition/vlib/code/math/spline.h"
#include "volition/vlib/code/math/vector3.h"

class matrix;
class navpoint;

// size 0x4B0
struct mini_script_params
{
    mini_script_params(); // 0x006C8EF0

    int num_knots; // 0x00
    float period_per_spline_unit; // 0x04
    vector pos_knots[32]; // 0x08
    vector fvec_knots[32]; // 0x188
    vector rvec_knots[32]; // 0x308
    cardinal_spline pos_spline; // 0x488
    cardinal_spline fvec_spline; // 0x494
    cardinal_spline rvec_spline; // 0x4A0
    float cur_time; // 0x4AC
};

void camera_ms_process(float step_time, bool last_step_of_frame); // 0x006C6540 (:25)
void camera_do_mini_script(int num_navpoints, navpoint** navp, float* duration); // 0x006D40F0 (:90)
void camera_ms_setup_data(mini_script_params* mscript_params, int num_navpoints, navpoint** navp, float* duration); // 0x006CB660 (:103)
bool camera_is_mini_script_done(); // 0x006C66A0 (:195)
void camera_do_look_through_script(const vector& start_pos, const matrix& start_orient, const vector& end_pos, const matrix& end_orient, float duration); // 0x006CC500 (:217)
