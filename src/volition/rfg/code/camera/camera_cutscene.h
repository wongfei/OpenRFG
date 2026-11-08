// Original: volition/rfg/code/camera/camera_cutscene.h
// Implementation: volition/rfg/code/camera/camera_cutscene.cpp
//
// The cutscene camera mode: the camera follows the loaded cutscene scene. Placement of cutscene_mode_params inferred.
#pragma once

#include "volition/rfg/code/geom/simple_keyframe.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"

class mempool_base;

// size 0x68. The serialized shot of a .csc_pc camera script. Placement inferred.
struct c_cutscene_shot
{
    float start_time; // 0x00
    float end_time; // 0x04
    keylist_vector translation_keys; // 0x08
    keylist_quat rotation_keys; // 0x14
    keylist_float fov_keys; // 0x20
    keylist_float blur_keys; // 0x2C
    keylist_float near_plane_keys; // 0x38
    keylist_float focus_start_plane_keys; // 0x44
    keylist_float focus_end_plane_keys; // 0x50
    keylist_float far_plane_keys; // 0x5C
};

// size 0x8. The head of a .csc_pc camera script, the file loaded as is. Placement inferred.
struct c_cutscene_camera
{
    unsigned short csc_version; // 0x00
    unsigned short csc_num_shots; // 0x02
    et_ptr_offset<c_cutscene_shot, 0> csc_shots; // 0x04
};

// size 0x38
struct cutscene_mode_params
{
    cutscene_mode_params(); // 0x006C8F20

    const c_cutscene_camera* camera_script; // 0x00
    int flags; // 0x04
    vector base_pos; // 0x08
    matrix base_orient; // 0x14
};

void camera_cs_process(float step_time, bool last_step_of_frame); // 0x006D1190 (:92)
void camera_cutscene_load_scene(const char* filename, mempool_base* pool); // 0x006D1360 (:197)
void camera_start_cutscene_mode(); // 0x006C9A70 (:280)
void camera_stop_cutscene_mode(); // 0x006C9AA0 (:296)
