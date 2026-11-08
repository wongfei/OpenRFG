// Original: volition/rfg/code/camera/camera_third_person.h
// Implementation: volition/rfg/code/camera/camera_third_person.cpp
//
// The third person camera mode: the offset behind the player, kept out of obstructions by collision tests (the last
// result is cached). Placement of thirdperson_mode_params inferred.
#pragma once

#include "volition/vlib/code/math/vector3.h"

// size 0x10
struct camera_cached_collision
{
    void set(vector new_pos); // 0x006C70C0 (:167)
    camera_cached_collision(); // 0x006C70E0

    vector pos; // 0x00
    bool valid; // 0x0C
};

// size 0x20
struct thirdperson_mode_params
{
    thirdperson_mode_params(); // 0x006C8EE0

    vector cur_offset; // 0x00
    vector desired_offset; // 0x0C
    float interpolation_time; // 0x18
    float ragdoll_offset; // 0x1C
};

void unused_dcf_camera_third_person(); // 0x006D4B80 (:184)
void unused_dcf_camera_third_person_offset(); // 0x006C70F0 (:203)
void unused_dcf_camera_third_person_offset_close(); // 0x006C7180 (:227)
void unused_dcf_camera_third_person_offset_scope(); // 0x006C7210 (:251)
void unused_dcf_camera_third_person_offset_sprint(); // 0x006C72A0 (:274)
void camera_start_third_person(bool smooth); // 0x006CF0C0 (:805)
void camera_stop_third_person(bool smooth); // 0x006CF250 (:857)
void camera_third_person_process(float step_time, bool last_step_of_frame); // 0x006DD250 (:872)
