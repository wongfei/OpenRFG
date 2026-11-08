// Original: volition/rfg/code/camera/camera_transition.cpp
//
// Camera transitions (player_minimal.md P6): a linear blend (eased with fl_parametric_accel_decel) from the camera's
// position, orientation and fov when the transition started to the mode's ideal ones, or a flight along a spline
// (the lookaround swoop). camera_do_frame applies the active one while Rfg_camera.flags has bit 2. Compiled in
// rfg_camera_unity.cpp; the camera modes read this file's statics. The blends are written as double chains rounded
// once, as elsewhere in the camera code; their rounding was not checked against the asm.

#include "volition/rfg/code/camera/camera_transition.h"

#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/quaternion.h"
#include "volition/vlib/code/math/spline.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/frametime.h"

#include <math.h>

// ---- TU-private types (rfg.pdb; no header names them)
enum camera_transition_types
{
    CAMERA_TRANSITION_TYPE_NONE = -1,
    CAMERA_TRANSITION_TYPE_LINEAR = 0,
    CAMERA_TRANSITION_TYPE_SPLINE = 1,
    NUM_CAMERA_TRANSITION_TYPES = 2,
};

// The parts a linear transition blends (linear_transition_params::flags).
static const unsigned int CAMERA_TRANSITION_POS = 1;
static const unsigned int CAMERA_TRANSITION_ORIENT = 2;
static const unsigned int CAMERA_TRANSITION_FOV = 4;

// size 0x2C
struct linear_transition_params
{
    linear_transition_params(); // 0x006C7440

    quaternion from_qorient; // 0x00
    vector from_pos; // 0x10
    float from_fov; // 0x1C
    float cur_time; // 0x20
    float trans_time; // 0x24
    unsigned int flags; // 0x28
};

// size 0x28
struct spline_transition_params
{
    spline_transition_params(); // 0x006C7450

    bool in; // 0x00
    cubic_bezier_spline* spline; // 0x04
    float* approx_segment_lengths; // 0x08
    float approx_total_length; // 0x0C
    float spline_speed; // 0x10
    void (*update_callback)(const vector&, const matrix&); // 0x14
    float spline_pos; // 0x18
    vector start_fvec; // 0x1C
};

// 0x006C7440 (compiler-generated: the member constructors)
linear_transition_params::linear_transition_params()
{
}

// 0x006C7450 (compiler-generated)
spline_transition_params::spline_transition_params()
{
}

// ---- file-static globals
static camera_transition_types Camera_transition_type; // 0x01DE489C
static quaternion To_qorient; // 0x01DE4AA0
static spline_transition_params Spline_transition_params; // 0x01DE6B0C
static linear_transition_params Linear_transition_params; // 0x01DE6B80

// 0x006C7460 (:132)
// LTCG: to_pos in EDI, to_orient in ESI.
static void process_linear_transition(vector* out_pos, matrix* out_orient, float* out_fov, const vector& to_pos, const matrix& to_orient, const float& to_fov, float step_time)
{
    const float t = fl_parametric_accel_decel(Linear_transition_params.cur_time / Linear_transition_params.trans_time, 2);
    if ((Linear_transition_params.flags & CAMERA_TRANSITION_POS) == 0)
    {
        *out_pos = to_pos;
    }
    else
    {
        const vector& from = Linear_transition_params.from_pos;
        out_pos->x = (float)((double)from.x + ((double)to_pos.x - (double)from.x) * (double)t);
        out_pos->y = (float)((double)from.y + ((double)to_pos.y - (double)from.y) * (double)t);
        out_pos->z = (float)(((double)to_pos.z - (double)from.z) * (double)t + (double)from.z);
    }
    if ((Linear_transition_params.flags & CAMERA_TRANSITION_ORIENT) == 0)
    {
        *out_orient = to_orient;
    }
    else
    {
        To_qorient.set(to_orient);
        if (quaternion_dot(To_qorient, Linear_transition_params.from_qorient) < 0.0f)
        {
            To_qorient = quaternion(-To_qorient.x, -To_qorient.y, -To_qorient.z, -To_qorient.w);
        }
        quaternion q;
        pltind_slerp(q, Linear_transition_params.from_qorient, To_qorient, t);
        q.extract_matrix(*out_orient);
    }
    if ((Linear_transition_params.flags & CAMERA_TRANSITION_FOV) == 0)
    {
        *out_fov = to_fov;
    }
    else
    {
        *out_fov = (float)(((double)to_fov - (double)Linear_transition_params.from_fov) * (double)t + (double)Linear_transition_params.from_fov);
    }
    if (1.0f <= t)
    {
        camera_trans_abort(); // inlined in the binary
        return;
    }
    float cur_time = Linear_transition_params.cur_time + step_time;
    if ((float)((double)Linear_transition_params.cur_time + (double)step_time - (double)Linear_transition_params.trans_time) >= 0.0f)
    {
        cur_time = Linear_transition_params.trans_time;
    }
    Linear_transition_params.cur_time = cur_time;
}

// 0x006C7760 (:198)
// LTCG: to_pos in EDX, to_orient in EAX, out_pos in EDI, out_orient in ESI. Along the spline, turning from the start
// direction to the spline's tangent; the callback may move the spline (the segment lengths are summed again).
static void process_spline_transition(vector* out_pos, matrix* out_orient, float* out_fov, const vector& to_pos, const matrix& to_orient, const float& to_fov, float step_time)
{
    cubic_bezier_spline* spline = Spline_transition_params.spline;
    if ((float)(spline->num_knots - 1) <= Spline_transition_params.spline_pos)
    {
        *out_pos = to_pos;
        *out_orient = to_orient;
        camera_trans_abort(); // inlined in the binary
        *out_fov = to_fov;
        return;
    }
    if (Spline_transition_params.update_callback != nullptr)
    {
        Spline_transition_params.update_callback(to_pos, to_orient);
        Spline_transition_params.approx_total_length = 0.0f;
        for (int i = 0; i < spline->num_knots; i++)
        {
            Spline_transition_params.approx_total_length = Spline_transition_params.approx_segment_lengths[i] + Spline_transition_params.approx_total_length;
        }
    }
    spline->get_point(*out_pos, Spline_transition_params.spline_pos);
    vector tangent;
    spline->get_tangent(tangent, Spline_transition_params.spline_pos);
    tangent.normalize_safe();
    if (!Spline_transition_params.in)
    {
        tangent = -tangent;
    }
    const float t = Spline_transition_params.spline_pos / (float)(spline->num_knots - 1);
    const vector& start = Spline_transition_params.start_fvec;
    vector fvec;
    fvec.x = (float)(((double)tangent.x - (double)start.x) * (double)t + (double)start.x);
    fvec.y = (float)(((double)tangent.y - (double)start.y) * (double)t + (double)start.y);
    fvec.z = (float)(((double)tangent.z - (double)start.z) * (double)t + (double)start.z);
    fvec.normalize_safe();
    out_orient->make_quick(fvec);
    const int segment = (int)floor((double)Spline_transition_params.spline_pos);
    Spline_transition_params.spline_pos = (float)((double)Spline_transition_params.spline_speed * (double)step_time / (double)Spline_transition_params.approx_segment_lengths[segment] + (double)Spline_transition_params.spline_pos);
    *out_fov = to_fov;
}

// 0x006C7AA0 (:256)
void camera_trans_level_init_post()
{
    camera_trans_abort(); // inlined in the binary
}

// 0x006C7AC0 (:274)
void camera_trans_process(vector* out_pos, matrix* out_orient, float* out_fov, const vector& to_pos, const matrix& to_orient, const float& to_fov, float step_time)
{
    if (Camera_transition_type == CAMERA_TRANSITION_TYPE_LINEAR)
    {
        process_linear_transition(out_pos, out_orient, out_fov, to_pos, to_orient, to_fov, step_time);
        return;
    }
    if (Camera_transition_type != CAMERA_TRANSITION_TYPE_SPLINE)
    {
        VLIB_CRITICAL_FAILURE();
        return;
    }
    process_spline_transition(out_pos, out_orient, out_fov, to_pos, to_orient, to_fov, step_time);
}

// 0x006C7B50 (:297)
// From the camera's current (real) transform; with start_at_zero false the first frame already advanced.
void camera_trans_start_linear_transition(float time_sec, unsigned int flags, bool start_at_zero)
{
    Linear_transition_params.from_qorient.set(Rfg_camera.real_orient);
    Linear_transition_params.from_pos = Rfg_camera.real_pos;
    Linear_transition_params.from_fov = Rfg_camera.real_fov;
    Linear_transition_params.cur_time = start_at_zero ? 0.0f : Frametime;
    Rfg_camera.flags |= 2;
    Linear_transition_params.trans_time = time_sec;
    Linear_transition_params.flags = flags;
    Camera_transition_type = CAMERA_TRANSITION_TYPE_LINEAR;
}

// 0x006C7BD0 (:331)
void camera_trans_start_spline_transition(cubic_bezier_spline* spline, float* approx_segment_lengths, float spline_speed, void (*update_callback)(const vector&, const matrix&), bool in)
{
    Spline_transition_params.in = in;
    Spline_transition_params.spline = spline;
    Spline_transition_params.approx_segment_lengths = approx_segment_lengths;
    Spline_transition_params.approx_total_length = 0.0f;
    for (int i = 0; i < spline->num_knots; i++)
    {
        Spline_transition_params.approx_total_length = approx_segment_lengths[i] + Spline_transition_params.approx_total_length;
    }
    Rfg_camera.flags |= 2;
    Spline_transition_params.spline_speed = spline_speed;
    Spline_transition_params.update_callback = update_callback;
    Spline_transition_params.spline_pos = 0.0f;
    Spline_transition_params.start_fvec = Rfg_camera.real_orient.fvec;
    Camera_transition_type = CAMERA_TRANSITION_TYPE_SPLINE;
}

// 0x006C7C80 (:354)
bool camera_trans_active()
{
    if (Camera_transition_type == CAMERA_TRANSITION_TYPE_LINEAR)
    {
        return fl_parametric_accel_decel(Linear_transition_params.cur_time / Linear_transition_params.trans_time, 2) < 1.0f;
    }
    if (Camera_transition_type == CAMERA_TRANSITION_TYPE_SPLINE)
    {
        return Spline_transition_params.spline_pos < (float)(Spline_transition_params.spline->num_knots - 1);
    }
    return false;
}

// 0x006C7D00 (:389)
void camera_trans_abort()
{
    Rfg_camera.flags &= ~2u;
    Camera_transition_type = CAMERA_TRANSITION_TYPE_NONE;
}
