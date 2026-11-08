// Original: volition/rfg/code/camera/camera_slew.h
// Implementation: volition/rfg/code/camera/camera_slew.cpp
//
// The slew camera mode (a free-flying debug camera); its controls are in control_debug_slew (M11).
#pragma once

void camera_slew_mode_init(); // 0x006C7040 (:80)
void camera_slew_set_fov(float fov_deg); // 0x006C7060 (:90)
void camera_slew_process(float step_time, bool last_step_of_frame); // 0x006C7090 (:99)
