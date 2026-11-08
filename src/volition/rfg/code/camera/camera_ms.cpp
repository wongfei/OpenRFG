// Original: volition/rfg/code/camera/camera_ms.cpp
//
// Partial (stub_impl.md S5): the mini script camera mode's frame and its done query. rfg.exe never starts a mini script
// (camera_do_mini_script has no caller), so the mode is reached through the mode table only. Not ported: the setup
// from navpoints (camera_do_mini_script, camera_ms_setup_data, camera_do_look_through_script; single player
// scripting, S10). Compiled in rfg_camera_unity.cpp.

#include "volition/rfg/code/camera/camera_ms.h"

#include "volition/rfg/code/camera/rfg_camera.h"

// 0x006C6540 (:25)
// The camera follows the position and orientation splines at the script's speed.
void camera_ms_process(float step_time, bool last_step_of_frame)
{
    mini_script_params& ms = Rfg_camera.mscript_params;
    Rfg_camera.ideal_fov = 45.0f;
    float u;
    if (ms.period_per_spline_unit <= 0.0f)
    {
        u = (float)ms.num_knots;
    }
    else
    {
        u = ms.cur_time / ms.period_per_spline_unit;
    }
    vector fvec;
    vector rvec;
    if ((float)(ms.num_knots - 1) <= u)
    {
        // Original bug kept: the end reads knot num_knots, one past the last one set.
        Rfg_camera.ideal_pos = ms.pos_knots[ms.num_knots];
        fvec = ms.fvec_knots[ms.num_knots];
        rvec = ms.rvec_knots[ms.num_knots];
    }
    else
    {
        // The splines have a lead-in knot before the first point.
        u = u + 1.0f;
        ms.pos_spline.get_point(Rfg_camera.ideal_pos, u);
        ms.fvec_spline.get_point(fvec, u);
        ms.rvec_spline.get_point(rvec, u);
    }
    Rfg_camera.ideal_orient.make_rvec(fvec, rvec);
    ms.cur_time = ms.cur_time + step_time;
}

// 0x006C66A0 (:195)
bool camera_is_mini_script_done()
{
    if (Rfg_camera.mode != CAMERA_MINI_SCRIPT_MODE)
    {
        return true;
    }
    return (float)(Rfg_camera.mscript_params.num_knots - 1) <= Rfg_camera.mscript_params.cur_time / Rfg_camera.mscript_params.period_per_spline_unit;
}
