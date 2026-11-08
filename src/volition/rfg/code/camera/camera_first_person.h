// Original: volition/rfg/code/camera/camera_first_person.h
// Implementation: volition/rfg/code/camera/camera_first_person.cpp
//
// The first person camera mode and the camera sensitivity settings (mouse and gyro).
#pragma once

void unused_dcf_camera_first_person(); // 0x006D1630 (:122)
void unused_dcf_set_camera_bone(); // 0x006D16A0 (:148)
void camera_first_person_update_camera_sensitivity(); // 0x006C3FA0 (:190)
float camera_first_person_get_gyro_base_sensitivity_pitch(); // 0x006C4040 (:201)
float camera_first_person_get_gyro_base_sensitivity_heading(); // 0x006C4050 (:206)
void camera_start_first_person(bool smooth); // 0x006C9AC0 (:211)
void camera_stop_first_person(bool smooth); // 0x006C9B60 (:226)
void camera_first_person_pre_process(); // 0x006C9C80 (:259)
void camera_first_person_process(float step_time, bool last_step_of_frame); // 0x006CAA00 (:529)
void camera_first_person_process_fov(float step_time); // 0x006C4060 (:572)
void camera_first_person_set_fov(float new_fov, int transition_time_ms); // 0x006C41C0 (:608)

// The look sensitivity (UI.md UI6: the control options set it; globals of rfg.exe).
extern float Camera_first_person_look_sensitivity_pitch; // 0x016481D8
extern float Camera_first_person_look_sensitivity_heading; // 0x016481DC
