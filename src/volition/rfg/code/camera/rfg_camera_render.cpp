// Original: volition/rfg/code/camera/rfg_camera.cpp (render half)
//
// The parts of rfg_camera.cpp that the renderer needs: the main render camera and the culling frustum of the camera's
// render position. They read the screen aspect from rl_render_lib, so they live in rfg_render; the rest of the TU is
// not recovered yet (the H2 seam camera_set_pos is a headless stand-in).

#include "volition/rfg/code/camera/rfg_camera.h"

#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_screen_info.h"

#include <math.h>

static_assert(sizeof(rfg_camera) == 0xA30, "sizeof(rfg_camera)");

rl_camera* Camera_main_scene_p = nullptr; // 0x01DE48E4

// The third person camera is locked to the render position (game_render_get_apparent_distance_from_camera).
bool Camera_third_person_lock = false; // 0x01DE48DB

// 0x006C95D0 (:1950)
rl_camera* camera_get_main_render_camera()
{
    return Camera_main_scene_p;
}

// 0x006D4C40 (:761)
// Built on first use after it was invalidated, from the render position and orientation.
const rl_view_frustum& rfg_camera::get_culling_frustum()
{
    if (!culling_frustum_valid)
    {
        culling_frustum_valid = true;
        compute_culling_frustrum(render_pos, render_orient, real_fov, m_culling_frustum);
    }
    return m_culling_frustum;
}

// 0x006D0850 (:771)
// The horizontal field of view from the vertical one and the screen aspect: 2 * atan(tan(fov / 2) * aspect), in
// degrees.
void rfg_camera::compute_culling_frustrum(const vector& cull_pos, const matrix& cull_orient, float cull_fov, rl_view_frustum& cull_frustum)
{
    const rl_screen_info* screen_info = rl_render_lib::get_instance()->get_screen_info();
    const float far_clip_dist = m_far_clip_dist;
    const float near_clip_dist = m_near_clip_dist;
    // Magic numbers: degrees to radians (0.017453292) and back (57.295776).
    const float half_angle = (float)((double)cull_fov * 0.01745329238474369 * 0.5);
    const float half_tan = (float)tan(half_angle);
    const float half_horizontal = (float)atan((float)(half_tan * screen_info->get_aspect_ratio()));
    const float horizontal_fov = (float)((double)(half_horizontal + half_horizontal) * 57.295776367187500);
    cull_frustum.init(cull_pos, cull_orient, horizontal_fov, cull_fov, near_clip_dist, far_clip_dist);
}
