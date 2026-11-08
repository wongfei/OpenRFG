// Original: volition/rfg/code/camera/camera_slew.cpp
//
// The slew camera mode (stub_impl.md S5): a debug camera its controls move (control_debug_slew); the mode only sets
// the fov. Compiled in rfg_camera_unity.cpp.

#include "volition/rfg/code/camera/camera_slew.h"

#include "volition/rfg/code/camera/rfg_camera.h"

static float Camera_slew_fov = 45.0f; // 0x016488D8

// 0x006C7040 (:80)
void camera_slew_mode_init()
{
    Rfg_camera.ideal_fov = Camera_slew_fov;
}

// 0x006C7060 (:90)
void camera_slew_set_fov(float fov_deg)
{
    Camera_slew_fov = fov_deg;
    if (Rfg_camera.mode == CAMERA_SLEW_MODE || Camera_third_person_lock)
    {
        Rfg_camera.ideal_fov = fov_deg;
    }
}

// 0x006C7090 (:99)
// Empty in rfg.exe: the slew controls move the camera directly.
void camera_slew_process(float step_time, bool last_step_of_frame)
{
}
