// Original: volition/rfg/code/camera/camera_death.h
// Implementation: volition/rfg/code/camera/camera_death.cpp
//
// The death camera mode: the camera after the player dies.
#pragma once

void camera_death_follow_ragdoll(); // 0x006C39E0 (:26)
void camera_death_process(float step_time, bool last_step_of_frame); // 0x006C3D10 (:59)
