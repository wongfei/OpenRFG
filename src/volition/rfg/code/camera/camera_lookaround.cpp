// Original: volition/rfg/code/camera/camera_lookaround.cpp
//
// The lookaround camera mode (player_minimal.md P6, stub_impl.md S5): an orbit around the player's head (over the
// shoulder), entered and left with a swoop along a three-knot bezier spline between the free camera and the orbit.
// rfg.exe never enters it (camera_start_lookaround_mode has no caller), so the mode is reached through the mode table
// only; the look input (camera_lookaround_set_rotation) is used by the first and third person modes. Compiled in
// rfg_camera_unity.cpp.
// Multiple-op scalar expressions are computed in double and rounded once, as the SSE2 code does; vector operators
// round per operation.

#include "volition/rfg/code/camera/camera_lookaround.h"

#include "volition/rfg/code/camera/camera_free.h"
#include "volition/rfg/code/camera/camera_transition.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_spinebend.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/spline.h"

// ---- TU-private types (rfg.pdb; no header names them)
// The swoop between the free camera (free_mode_*) and the orbit (lookaround_mode_*): knots, handles and the segment
// lengths of the spline the transition follows.
// size 0xE8
struct lookaround_transition_data
{
    bool in; // 0x00, toward the orbit
    cubic_bezier_spline spline; // 0x04
    vector spline_pts[3]; // 0x14
    vector spline_in_tangents[3]; // 0x38
    vector spline_out_tangents[3]; // 0x5C
    float approx_segment_lengths[2]; // 0x80
    vector free_mode_pos; // 0x88
    matrix free_mode_orient; // 0x94
    vector lookaround_mode_pos; // 0xB8
    matrix lookaround_mode_orient; // 0xC4
};

static lookaround_transition_data Lookaround_transition_data; // 0x01DE4978

static vector get_final_eye_pos(object* objp, float pitch); // 0x006C5320 (:135)
static void calculate_swoop_spline(const camera_swoop_params* swoop_params, const vector& start_pos, const matrix& start_orient, const vector& end_pos, const matrix& end_orient, bool in); // 0x006C5970 (:190)
static void swoop_spline_update_callback(const vector& to_pos, const matrix& to_orient); // 0x006D2480 (:277)
static void start_swoop(bool in); // 0x006D3060 (:308)

// 0x006C5320 (:135)
// LTCG: objp in ECX, the result in EAX. A human's eye, shifted over the right shoulder when the orbit is: behind,
// right and up, moved in as the camera pitches down and up as it pitches up.
static vector get_final_eye_pos(object* objp, float pitch)
{
    if (objp->get_obj_type() != OT_HUMAN)
    {
        return objp->pos;
    }
    vector eye = objp->pos;
    eye.y = (float)((double)eye.y + 1.539); // Magic number: the eye height above a human's position
    if (!Rfg_camera.lookaround_params.over_the_shoulder)
    {
        return eye;
    }
    // Magic numbers: 0.28 m right, 0.15 m up, 0.85 m back; down to 0.45 m back looking straight down, up to 0.33 m
    // up looking straight up. pi/2 is a float constant.
    const double half_pi = (double)1.5707964f;
    float right = 0.28f;
    float up = 0.15f;
    float fwd = -0.85f;
    if (pitch < 0.0f)
    {
        const float t = (float)(((double)pitch + half_pi) / (half_pi + 0.0) - 1.0);
        const float s = (float)-((double)t * (double)t * (double)t);
        right = (float)((double)s * 0.0 + 0.28);
        up = (float)((double)s * 0.0 + 0.15);
        fwd = (float)((double)s * 0.4 - 0.85);
    }
    else if (pitch > 0.0f)
    {
        const float t = (float)((half_pi - (double)pitch) / (half_pi + 0.0) - 1.0);
        const float s = (float)((double)t * (double)t);
        right = (float)((double)s * 0.0 + 0.28);
        up = (float)((double)s * 0.18 + 0.15);
        fwd = (float)((double)s * 0.0 - 0.85);
    }
    const matrix& o = Rfg_camera.ideal_orient;
    return eye + o.rvec * right + o.uvec * up + o.fvec * fwd;
}

// 0x006C5970 (:190)
// LTCG: end_pos in EAX, end_orient in EDX, start_orient in EDI. The spline runs from the end (knot 0) through a
// middle knot to the start (knot 2) when swooping in, the other way out (the handles swapped). The middle knot is
// behind the start's head, or beside it when the two views face apart (more than 135 degrees).
static void calculate_swoop_spline(const camera_swoop_params* swoop_params, const vector& start_pos, const matrix& start_orient, const vector& end_pos, const matrix& end_orient, bool in)
{
    lookaround_transition_data& d = Lookaround_transition_data;
    int e;
    int s;
    int e_handle;
    if (!in)
    {
        e_handle = 2;
        e = 2;
        s = 0;
    }
    else
    {
        e = 0;
        e_handle = 0;
        s = 2;
    }
    d.spline_pts[e] = end_pos;
    const float facing = (float)((double)start_orient.fvec.y * (double)end_orient.fvec.y + (double)end_orient.fvec.x * (double)start_orient.fvec.x + (double)start_orient.fvec.z * (double)end_orient.fvec.z);
    if (facing < -0.707106f) // Magic number: cos(135 degrees)
    {
        const vector side = start_orient.rvec * swoop_params->swing_factor_x;
        const vector back = start_orient.fvec * swoop_params->swing_factor_z;
        const vector to_end = end_pos - start_pos;
        if ((float)((double)start_orient.rvec.y * (double)to_end.y + (double)start_orient.rvec.x * (double)to_end.x + (double)start_orient.rvec.z * (double)to_end.z) <= 0.0f)
        {
            d.spline_pts[1] = start_pos - side - back;
            d.spline_in_tangents[1] = d.spline_pts[1] - start_orient.rvec * swoop_params->in_handle_lengths_swing[1];
            d.spline_out_tangents[1] = d.spline_pts[1] + start_orient.rvec * swoop_params->out_handle_lengths_swing[1];
        }
        else
        {
            d.spline_pts[1] = start_pos + side - back;
            d.spline_in_tangents[1] = d.spline_pts[1] + start_orient.rvec * swoop_params->in_handle_lengths_swing[1];
            d.spline_out_tangents[1] = d.spline_pts[1] - start_orient.rvec * swoop_params->out_handle_lengths_swing[1];
        }
        vector dir = d.spline_pts[1] - d.spline_pts[e];
        dir.normalize_safe();
        d.spline_out_tangents[e] = d.spline_pts[e] + dir * swoop_params->out_handle_lengths_swing[e_handle];
        d.spline_pts[s] = start_pos;
        d.spline_in_tangents[s] = d.spline_pts[s] - start_orient.fvec * swoop_params->in_handle_lengths_swing[s];
    }
    else
    {
        d.spline_out_tangents[e] = d.spline_pts[e] + end_orient.fvec * swoop_params->out_handle_lengths[e];
        d.spline_pts[1] = start_pos - start_orient.fvec * swoop_params->behind_head_dist;
        d.spline_in_tangents[1] = d.spline_pts[1] - start_orient.fvec * swoop_params->in_handle_lengths[1];
        d.spline_out_tangents[1] = d.spline_pts[1] + start_orient.fvec * swoop_params->out_handle_lengths[1];
        d.spline_pts[s] = start_pos;
        d.spline_in_tangents[s] = d.spline_pts[s] - start_orient.fvec * swoop_params->in_handle_lengths[s];
    }
    if (!in)
    {
        const int knots[3] = {e, 1, s};
        for (int i = 0; i < 3; i++)
        {
            const vector t = d.spline_in_tangents[knots[i]];
            d.spline_in_tangents[knots[i]] = d.spline_out_tangents[knots[i]];
            d.spline_out_tangents[knots[i]] = t;
        }
    }
}

// The free camera's view of the target for a swoop out: its default position, looking at the target (the look-at
// point at pitch 0; both inlined in the binary).
static void swoop_set_free_mode_view()
{
    lookaround_transition_data& d = Lookaround_transition_data;
    d.free_mode_pos = camera_free_get_default_pos();
    vector dir = camera_free_get_lookat(0.0f) - d.free_mode_pos;
    dir.normalize_safe();
    d.free_mode_orient.make_quick(dir);
}

// 0x006D2480 (:277)
// The transition's update: the end that moves (the orbit going in, the free camera's view going out) is taken again
// and the spline recomputed.
static void swoop_spline_update_callback(const vector& to_pos, const matrix& to_orient)
{
    lookaround_transition_data& d = Lookaround_transition_data;
    const camera_swoop_params* swoop = &Rfg_camera.free_params.submode_params->fp_swoop_params;
    if (d.in)
    {
        d.lookaround_mode_pos = to_pos;
        d.lookaround_mode_orient = to_orient;
    }
    else
    {
        swoop_set_free_mode_view();
    }
    calculate_swoop_spline(swoop, d.lookaround_mode_pos, d.lookaround_mode_orient, d.free_mode_pos, d.free_mode_orient, d.in);
    d.approx_segment_lengths[0] = d.spline.get_length_approx(8, 0.0f, 1.0f);
    d.approx_segment_lengths[1] = d.spline.get_length_approx(4, 1.0f, 2.0f);
}

// 0x006D3060 (:308)
static void start_swoop(bool in)
{
    lookaround_transition_data& d = Lookaround_transition_data;
    object* objp = World.object_from_handle(Rfg_camera.target_handle);
    const camera_swoop_params* swoop = &Rfg_camera.free_params.submode_params->fp_swoop_params;
    d.in = in;
    if (in)
    {
        d.free_mode_pos = Rfg_camera.real_pos;
        d.free_mode_orient = Rfg_camera.real_orient;
        d.lookaround_mode_pos = get_final_eye_pos(objp, 0.0f);
        d.lookaround_mode_orient = objp->orient;
    }
    else
    {
        swoop_set_free_mode_view();
        d.lookaround_mode_pos = Rfg_camera.real_pos;
        d.lookaround_mode_orient = Rfg_camera.real_orient;
    }
    calculate_swoop_spline(swoop, d.lookaround_mode_pos, d.lookaround_mode_orient, d.free_mode_pos, d.free_mode_orient, d.in);
    d.spline.set_points(3, d.spline_pts, d.spline_in_tangents, d.spline_out_tangents);
    d.approx_segment_lengths[0] = d.spline.get_length_approx(8, 0.0f, 1.0f);
    d.approx_segment_lengths[1] = d.spline.get_length_approx(4, 1.0f, 2.0f);
    camera_trans_start_spline_transition(&d.spline, d.approx_segment_lengths, swoop->swoop_in_speed, swoop_spline_update_callback, in);
}

// 0x006D3D50 (:357)
// The orbit turns with the look input; a ragdolled player swoops the camera out to the free camera's view and back in
// when the ragdoll ends.
void camera_lookaround_process(float step_time, bool last_step_of_frame)
{
    lookaround_mode_params& look = Rfg_camera.lookaround_params;
    player* pp = player_get_local_player();
    object* objp = World.object_from_handle(Rfg_camera.target_handle);
    if (look.ragdoll_cam)
    {
        if (pp->ragdoll_idx == -1)
        {
            if (!look.over_the_shoulder)
            {
                pp->render_alpha = 0.0f;
            }
            start_swoop(true);
            look.ragdoll_cam = false;
        }
        return;
    }
    if (pp->ragdoll_idx != -1)
    {
        pp->render_alpha = 1.0f;
        start_swoop(false);
        look.ragdoll_cam = true;
        return;
    }
    Rfg_camera.ideal_fov = 45.0f;
    look.rotvel_h = 0.0f;
    // Magic numbers: 8 is only tested against 0; the turn rate is 1.5 times the input.
    if ((float)((double)look.rot_p * 8.0 * (double)step_time) == 0.0f)
    {
        look.rotvel_p = 0.0f;
    }
    else
    {
        look.rotvel_p = (float)((double)look.rot_p * 1.5);
    }
    if ((float)((double)look.rot_h * 8.0 * (double)step_time) != 0.0f)
    {
        look.rotvel_h = (float)((double)look.rot_h * 1.5);
    }
    look.pitch = (float)((double)look.rotvel_p * (double)step_time + (double)look.pitch);
    // Magic number: the pitch limit, about 54 degrees.
    if (look.pitch > 0.94147784f)
    {
        look.pitch = 0.94147784f;
        look.rotvel_p = 0.0f;
    }
    else if (look.pitch < -0.94147784f)
    {
        look.pitch = -0.94147784f;
        look.rotvel_p = 0.0f;
    }
    look.heading = (float)((double)look.heading + (double)look.rotvel_h * (double)step_time);
    look.heading = fl_angle_cap_radians(look.heading, 0.0f);
    Rfg_camera.ideal_orient.set(look.pitch, 0.0f, look.heading);
    Rfg_camera.ideal_pos = get_final_eye_pos(objp, look.pitch);
    if (!look.over_the_shoulder && (Rfg_camera.flags & 2) == 0)
    {
        pp->render_alpha = 0.0f;
    }
    else
    {
        pp->render_alpha = 1.0f;
    }
}

// 0x006D3FE0 (:515)
void camera_start_lookaround_mode(bool swoop)
{
    player* pp = player_get_local_player();
    float pitch;
    float bank;
    float heading;
    Rfg_camera.real_orient.extract_angles(pitch, bank, heading);
    lookaround_mode_params& look = Rfg_camera.lookaround_params;
    look.over_the_shoulder = 1;
    look.rotvel_p = 0.0f;
    look.rotvel_h = 0.0f;
    look.rot_p = 0.0f;
    look.rot_h = 0.0f;
    look.pitch = pitch;
    look.heading = heading;
    pp->render_alpha = 1.0f;
    camera_push_mode(CAMERA_LOOKAROUND_MODE);
    if (swoop)
    {
        start_swoop(true);
    }
}

// 0x006D4080 (:560)
void camera_stop_lookaround_mode(bool swoop)
{
    if (!camera_mode_is_on_stack(CAMERA_LOOKAROUND_MODE)) // inlined in the binary
    {
        return;
    }
    player* pp = player_get_local_player();
    camera_pop_mode(CAMERA_LOOKAROUND_MODE);
    if (swoop)
    {
        start_swoop(false);
    }
    camera_free_reset();
    human_spinebend_reset(pp);
    pp->render_alpha = 1.0f;
}

// 0x006C6520 (:587)
// The turn rates of the stick (process_camera_normal_controls); camera_first_person_pre_process applies them.
void camera_lookaround_set_rotation(float pitch, float heading)
{
    Rfg_camera.lookaround_params.rot_p = pitch;
    Rfg_camera.lookaround_params.rot_h = heading;
}
