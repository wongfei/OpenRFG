// Original: volition/anim_lib/code/anim_lib_spinebend.cpp
//
// Spine bending (player_minimal.md P4), run from the animation transform command between the pre and post bone
// transforms (animlib_process_command): the head and eye directions of human_spinebend_data become a heading and
// pitch per bone of the 9-bone chain (spine bones 0-4 from the pelvis up, the arms' 5 and 6, the eyes' 7 and 8), each
// within its limits (Bone_info, relative to the animated pose and scaled by bone_max_angle_multiplier) and ramped
// towards its target (fl_ramp_to_value), then registered as pre-rotate bone modifiers on the instance.
// Part of anim_lib_unity.cpp, after anim_lib_instance_manager.cpp (its instance table) and anim_lib.cpp.
//
// Floating point: the functions compute in SSE2, each operation on floats promoted to double and rounded back to
// float, which plain float arithmetic reproduces; the double chains (several operations rounded once) are written with
// double casts, and the trigonometry is the CRT's double functions (x87 __CIasin, __CIsin, __CIcos).
// The external interface is declared in anim_lib_spinebend.h.

#include "volition/anim_lib/code/anim_lib_spinebend.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/rfg/code/objects/humans/human_spinebend.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/os/frametime.h"

#include <math.h>

// ---- file-static globals
static const float PI = 3.14159274f; // 0x01343B40 (0x40490FDB)
static const float PI2 = 6.28318548f; // 0x01343B44 (0x40C90FDB)
// Constants the compiler folded in double from float expressions of PI (3 * PI, 0.9f * PI, 0.01f * PI).
static const double PI_TIMES_3 = 3.0 * (double)3.14159274f; // 0x01343BF0
static const double PI_TIMES_0_9 = (double)0.9f * (double)3.14159274f; // 0x01343BE8
static const double PI_TIMES_0_01 = (double)0.01f * (double)3.14159274f; // 0x01343BE0
// anim_instance_manager Anim_instance_managers[356] (0x039ACBD0) is used here too; placed in anim_lib_instance_manager.cpp.

// ---- globals (.data; the spine bend chain: 0-4 the spine from the pelvis to the head, 5 and 6 the arms, 7 and 8 the
// eyes). Placement inferred.
int Bone_parent[9] = {-1, 0, 1, 2, 3, 2, 2, 4, 4}; // 0x016F0894
// The children of each bone (-1: none). The eyes are bent on their own (process_spinebending), not as the head's.
int Bone_children[9][3] = {{1, -1, -1}, {2, -1, -1}, {3, 5, 6}, {4, -1, -1}, {-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1}}; // 0x016F08B8
// The limits (radians: 35, -23, 20 and -20 degrees for the lower spine ...) and blend shares of each bone, and the
// filter bits (human_spinebend_data::bone_filter) that let it turn or pitch.
spinebend_bone_info Bone_info[9] = {
    {0.610854983f, -0.401418984f, 0.349059999f, -0.349059999f, 0.0199999996f, 0x3F, 0x3F},
    {0.436325014f, -0.401418984f, 0.349059999f, -0.349059999f, 0.0250000004f, 0x3F, 0x3F},
    {0.698119998f, -0.471230984f, 0.349059999f, -0.349059999f, 0.0299999993f, 0x3F, 0x3F},
    {0.0872649997f, -0.436325014f, 0.349059999f, -0.349059999f, 0.0299999993f, 0x81, 0x81},
    {0.436325014f, -0.959914982f, 1.13444495f, -1.13444495f, 0.894999981f, 0x81, 0x81},
    {0.698119998f, -1.22170997f, 1.57077003f, -1.04717994f, 0.600000024f, 0x2, 0x8},
    {0.698119998f, -1.22170997f, 1.04717994f, -1.57077003f, 0.600000024f, 0x4, 0x10},
    {0.349059999f, -0.139624f, 0.436325014f, -0.436325014f, 1.0f, 0x40, 0x40},
    {0.349059999f, -0.139624f, 0.436325014f, -0.436325014f, 1.0f, 0x40, 0x40},
}; // 0x016F0928

// 0x00C8EF40 (:214)
// Moves cur_value towards dest_value under an acceleration limit, keeping its velocity: out of combat it accelerates
// and brakes to arrive (a constant deceleration over the time to stop, corrected towards the remaining distance); in
// combat it moves at 15% of the acceleration per second and the velocity is a fifth of the step.
float fl_ramp_to_value(float cur_value, float dest_value, float acceleration, float& cur_velocity, bool in_combat)
{
    if (cur_value == dest_value && cur_velocity == 0.0f)
    {
        return dest_value;
    }
    if (in_combat)
    {
        // Magic number: 15% of the acceleration per second.
        if (cur_value > dest_value)
        {
            const float limit = (float)((double)cur_value - (double)Frametime * (double)acceleration * (double)0.15f);
            if (dest_value - limit < 0.0f)
            {
                dest_value = limit;
            }
        }
        else
        {
            const float limit = (float)((double)Frametime * (double)acceleration * (double)0.15f + (double)cur_value);
            if (!(dest_value - limit < 0.0f))
            {
                dest_value = limit;
            }
        }
        // Magic number: a fifth of the step.
        cur_velocity = (float)(((double)dest_value - (double)cur_value) / (double)Frametime * (double)0.2f);
        return dest_value;
    }
    const float velocity = cur_velocity;
    float time_to_stop = fabsf((float)((double)velocity / (double)acceleration));
    if (time_to_stop - Frametime < 0.0f)
    {
        time_to_stop = Frametime;
    }
    const float braking = 0.0f < velocity ? -acceleration : acceleration;
    const float stop_distance = (float)((double)braking * 0.5 * (double)time_to_stop * (double)time_to_stop + (double)velocity * (double)time_to_stop);
    const double distance_d = (double)dest_value - (double)cur_value;
    const float distance = (float)distance_d;
    float accel;
    // Magic numbers: within 1.5 frames of stopping the velocity is set to arrive this frame (at most twice the
    // acceleration); otherwise a stop that would end within 1.5 times the distance is corrected (at most 1.25 times).
    if (!((double)Frametime * 1.5 <= (double)time_to_stop))
    {
        accel = (float)((distance_d / (double)Frametime - (double)velocity) / (double)Frametime);
        const float lo = -acceleration * 2.0f;
        if (accel - lo < 0.0f)
        {
            accel = lo;
        }
        const float hi = acceleration * 2.0f;
        if (!(accel - hi < 0.0f))
        {
            accel = hi;
        }
    }
    else
    {
        const float ratio = distance / stop_distance;
        if (0.0f < ratio && ratio < 1.5f)
        {
            float correction = distance - stop_distance;
            correction = correction / time_to_stop;
            correction = correction / time_to_stop;
            accel = (float)((double)correction * (double)time_to_stop / (double)Frametime + (double)braking);
            const float lo = -acceleration * 1.25f;
            if (accel - lo < 0.0f)
            {
                accel = lo;
            }
            const float hi = acceleration * 1.25f;
            if (!(accel - hi < 0.0f))
            {
                accel = hi;
            }
        }
        else
        {
            accel = distance < 0.0f ? -acceleration : acceleration;
        }
    }
    const float step = (float)((double)(float)((double)Frametime * (double)accel + (double)cur_velocity) * (double)Frametime);
    // Magic numbers: a step over 1 cm is scaled to the distance (slowed when it would overshoot backwards, sped up when
    // the distance is behind by more than half a step, 1.5 to 3 times).
    if (0.01f < fabsf(step))
    {
        const float q = distance / step;
        if (!(q < 0.0f) && q < 1.0f)
        {
            accel = q * accel;
        }
        else if (q < 0.0f && -0.5f < q)
        {
            accel = (float)((double)accel * (double)0.1f);
        }
        else if (q < -0.5f)
        {
            float scale = fabsf(q) * 0.5f;
            if (scale - 1.5f < 0.0f)
            {
                scale = 1.5f;
            }
            if (!(scale - 3.0f < 0.0f))
            {
                scale = 3.0f;
            }
            accel = scale * accel;
        }
    }
    const float new_velocity = (float)((double)Frametime * (double)accel + (double)cur_velocity);
    cur_velocity = new_velocity;
    // x87: the new value is computed without rounding the product.
    return (float)((double)new_velocity * (double)Frametime + (double)cur_value);
}

// 0x00C8F410 (:288)
// Moves a bone's heading and pitch towards their targets (faster for the upper spine and the eyes, slower close to
// the target), against the motion the animation and the body already turn, and returns the applied angles of the bone
// and its largest child.
void interp_bones(human_spinebend_data* sbd, int bone_idx, float& applied_heading, float& applied_pitch)
{
    if (bone_idx == -1)
    {
        applied_heading = 0.0f;
        applied_pitch = 0.0f;
        return;
    }
    // Magic number: 540 degrees per second squared at priority 1.
    float heading_accel = (float)((double)sbd->lookat_priority * PI_TIMES_3);
    float pitch_accel = heading_accel;
    spinebend_bone_state& bs = sbd->bone_state[bone_idx];
    if (!sbd->flags.aim_at_pos)
    {
        pitch_accel = pitch_accel / 3.0f;
    }
    if (bone_idx == 3 || bone_idx == 4)
    {
        heading_accel = heading_accel * 2.0f;
        pitch_accel = pitch_accel * 2.0f;
    }
    if (bone_idx == 7 || bone_idx == 8)
    {
        // Magic number: the eyes accelerate at 8 rad/s^2.
        heading_accel = 8.0f;
        pitch_accel = 8.0f;
    }
    const float heading = bs.heading;
    const float desired_heading = bs.desired_heading;
    // Magic numbers: a target moving faster than pi rad/s is followed in fast mode; close to the target (0.05 rad)
    // the ramp slows to 75%.
    if (heading == desired_heading)
    {
        bs.flags.fast_mode = 0;
    }
    else if (!((double)fabsf(desired_heading - bs.last_heading) / (double)Frametime < (double)3.14159274f))
    {
        bs.flags.fast_mode = 1;
    }
    else if (fabsf(heading - desired_heading) < 0.05f && !bs.flags.fast_mode)
    {
        heading_accel = heading_accel * 0.75f;
    }
    if ((double)fabsf(bs.desired_pitch - bs.last_pitch) / (double)Frametime < PI_TIMES_0_9 &&
        PI_TIMES_0_01 < (double)fabsf(desired_heading - bs.last_heading) / (double)Frametime && fabsf(bs.pitch - bs.desired_pitch) < 0.05f && !bs.flags.fast_mode)
    {
        pitch_accel = pitch_accel * 0.75f;
    }
    // The heading the body turns this frame (the spine bend bone, the lower body and, while blending, the animation's
    // and the root's) is taken back from the bone when it turns against the target.
    float body_turn = sbd->spinebend_bone_rotation + sbd->lower_body_rotation;
    if (sbd->flags.blending_anims)
    {
        body_turn = (float)((double)bs.anim_heading_change * (double)1.1f + (double)sbd->root_heading_change + (double)body_turn);
    }
    const double heading_diff = (double)desired_heading - (double)heading;
    const float heading_gap = (float)heading_diff - 0.0f;
    if ((!((double)heading_gap < (double)0.0001f) || !((double)-heading_gap < (double)0.0001f)) && (double)body_turn / heading_diff < 0.0)
    {
        if (fabsf(heading - desired_heading) < fabsf(body_turn))
        {
            bs.heading = desired_heading;
            bs.heading_vel = bs.heading_vel * 0.25f;
        }
        else
        {
            bs.heading = heading - body_turn;
        }
    }
    if (sbd->flags.blending_anims)
    {
        // Magic number: while blending the bone moves at 2 rad/s at most.
        const float h = bs.heading;
        if (desired_heading < h)
        {
            const double moved = (double)h - (double)Frametime * 2.0;
            if ((double)desired_heading > moved)
            {
                bs.heading = desired_heading;
                bs.heading_vel = bs.heading_vel * 0.25f;
            }
            else
            {
                bs.heading = (float)moved;
            }
        }
        else
        {
            const double moved = (double)Frametime * 2.0 + (double)h;
            if (moved > (double)desired_heading)
            {
                bs.heading = desired_heading;
                bs.heading_vel = bs.heading_vel * 0.25f;
            }
            else
            {
                bs.heading = (float)moved;
            }
        }
    }
    const float vel_limit = (float)((double)sbd->lookat_priority * PI_TIMES_3);
    if (sbd->flags.linear_interpolation)
    {
        // Magic number: a full turn per second.
        const float step = Frametime * 6.28318548f;
        const float h = bs.heading;
        if (desired_heading < h)
        {
            const double moved = (double)h - (double)step;
            bs.heading = (double)desired_heading > moved ? desired_heading : (float)moved;
        }
        else if (h < desired_heading)
        {
            const double moved = (double)h + (double)step;
            bs.heading = moved > (double)desired_heading ? desired_heading : (float)moved;
        }
    }
    else if (sbd->flags.is_player && sbd->flags.spinebend_teleport)
    {
        bs.heading = desired_heading;
        bs.heading_vel = 0.0f;
    }
    else
    {
        float h = fl_ramp_to_value(bs.heading, desired_heading, heading_accel, bs.heading_vel, sbd->flags.aim_at_pos);
        bs.heading = h;
        if (h - bs.min_heading < 0.0f)
        {
            h = bs.min_heading;
        }
        if (!(h - bs.max_heading < 0.0f))
        {
            h = bs.max_heading;
        }
        bs.heading = h;
        float v = bs.heading_vel;
        const float lo = -heading_accel * Frametime;
        if (v - lo < 0.0f)
        {
            v = lo;
        }
        const float hi = Frametime * heading_accel;
        bs.heading_vel = v - hi < 0.0f ? v : hi;
    }
    if (bs.heading == bs.desired_heading)
    {
        bs.heading_vel = 0.0f;
    }
    {
        float v = -vel_limit;
        if (!(bs.heading_vel - -vel_limit < 0.0f))
        {
            v = bs.heading_vel;
        }
        if (!(v - vel_limit < 0.0f))
        {
            v = vel_limit;
        }
        bs.heading_vel = v;
    }
    // The pitch the animation adds this frame (more while blending) is taken back the same way.
    float anim_pitch = bs.anim_pitch_change;
    if (sbd->flags.blending_anims)
    {
        anim_pitch = (float)(((double)Bone_info[bone_idx].blend_pct + (double)0.2f) * (double)anim_pitch + (double)anim_pitch);
    }
    const float desired_pitch = bs.desired_pitch;
    const float pitch = bs.pitch;
    const double pitch_diff = (double)desired_pitch - (double)pitch;
    const float pitch_gap = (float)pitch_diff - 0.0f;
    if ((!((double)pitch_gap < (double)0.0001f) || !((double)-pitch_gap < (double)0.0001f)) && (double)anim_pitch / pitch_diff < 0.0)
    {
        if (fabsf(pitch - desired_pitch) < fabsf(anim_pitch))
        {
            bs.pitch = desired_pitch;
            bs.pitch_vel = bs.pitch_vel * 0.25f;
        }
        else
        {
            bs.pitch = pitch - anim_pitch;
        }
    }
    const float pitch_vel_limit = (float)((double)sbd->lookat_priority * PI_TIMES_3);
    if (sbd->flags.is_player && sbd->flags.spinebend_teleport)
    {
        bs.pitch = desired_pitch;
        bs.pitch_vel = 0.0f;
    }
    else
    {
        const float p = fl_ramp_to_value(bs.pitch, desired_pitch, pitch_accel, bs.pitch_vel, sbd->flags.aim_at_pos);
        bs.pitch = p;
        float v = bs.pitch_vel;
        const float lo = -pitch_accel * Frametime;
        if (v - lo < 0.0f)
        {
            v = lo;
        }
        const float hi = Frametime * pitch_accel;
        bs.pitch_vel = v - hi < 0.0f ? v : hi;
        if (p == bs.desired_pitch)
        {
            bs.pitch_vel = 0.0f;
        }
    }
    if (Bash_spinebend_bones || sbd->flags.spinebend_teleport)
    {
        bs.pitch = bs.desired_pitch;
    }
    {
        float v = -pitch_vel_limit;
        if (!(bs.pitch_vel - -pitch_vel_limit < 0.0f))
        {
            v = bs.pitch_vel;
        }
        if (!(v - pitch_vel_limit < 0.0f))
        {
            v = pitch_vel_limit;
        }
        bs.last_heading = bs.desired_heading;
        bs.last_pitch = bs.desired_pitch;
        bs.pitch_vel = v;
    }
    // the children: the largest heading and pitch any of them applies
    float child_heading = 0.0f;
    float child_pitch = 0.0f;
    for (int i = 0; i < 3; i++)
    {
        const int child = Bone_children[bone_idx][i];
        if (child == -1)
        {
            break;
        }
        float h;
        float p;
        interp_bones(sbd, child, h, p);
        if (fabsf(child_heading) < fabsf(h))
        {
            child_heading = h;
        }
        if (fabsf(child_pitch) < fabsf(p))
        {
            child_pitch = p;
        }
    }
    applied_heading = bs.heading + child_heading;
    applied_pitch = child_pitch + bs.pitch;
}

// 0x00C9A580 (:473)
// The bone's rotation (heading about y; pitch about x, or about the axis turned by pitch_axis_heading) as a pre-rotate
// bone modifier of the instance, then the same for its children.
void bend_bones(human_spinebend_data* sbd, int bone_idx, float pitch_axis_heading)
{
    const spinebend_bone_state& bs = sbd->bone_state[bone_idx];
    if (bs.bone_index == -1)
    {
        return;
    }
    float cosh;
    float sinh;
    if (bs.heading == 0.0f)
    {
        sinh = 0.0f;
        cosh = 1.0f;
    }
    else
    {
        cosh = (float)cos((double)bs.heading);
        sinh = (float)sin((double)bs.heading);
    }
    matrix rot;
    if (pitch_axis_heading == 0.0f)
    {
        float sinp = 0.0f;
        float cosp;
        if (bs.pitch == 0.0f)
        {
            cosp = 1.0f;
        }
        else
        {
            cosp = (float)cos((double)bs.pitch);
            sinp = (float)sin((double)bs.pitch);
        }
        rot.set(sinp, cosp, 0.0f, 1.0f, sinh, cosh);
    }
    else
    {
        rot.set(0.0f, 1.0f, 0.0f, 1.0f, sinh, cosh);
        const float cos_axis = (float)cos((double)pitch_axis_heading);
        const float sin_axis = (float)sin((double)pitch_axis_heading);
        matrix axis;
        axis.set(0.0f, 1.0f, 0.0f, 1.0f, sin_axis, cos_axis);
        axis.set(axis.rvec, bs.pitch);
        rot = rot * axis;
    }
    if (sbd->aim_handle > -1)
    {
        anim_instance_manager* aim = aim_from_handle(sbd->aim_handle);
        if (aim != nullptr)
        {
            aim->register_bone_modifier(BONE_MODIFIER_PRE_ROTATE, bs.bone_index, bone_idx, rot, nullptr, 0.0f);
        }
    }
    for (int i = 0; i < 3; i++)
    {
        const int child = Bone_children[bone_idx][i];
        if (child > -1)
        {
            bend_bones(sbd, child, pitch_axis_heading);
        }
    }
}

// 0x00C8FDE0 (:521)
// The animated pose of each bone (heading and pitch, the spine root's relative to its rig parent) sets its limits
// (the table's, scaled by bone_max_angle_multiplier, twice for the player's pitch) and the animation's turn this frame
// (accumulated down the chain); the root bone's heading change too.
void calculate_bone_limits(human_spinebend_data* sbd, matrix43* tmp_matrices)
{
    if (!sbd->flags.no_spinebending_last_frame)
    {
        const float root_heading = tmp_matrices[1].m_rotation.extract_heading();
        sbd->root_heading_change = root_heading - sbd->root_heading_last_frame;
        sbd->root_heading_last_frame = root_heading;
    }
    else
    {
        sbd->root_heading_change = 0.0f;
        sbd->root_heading_last_frame = 0.0f;
    }
    // Original: the accumulators of bones without a rig bone stay uninitialized (zero here).
    float chain_heading[9] = {};
    float chain_pitch[9] = {};
    const anim_instance_manager* aim = aim_from_handle(sbd->aim_handle);
    for (int i = 0; i < 9; i++)
    {
        spinebend_bone_state& bs = sbd->bone_state[i];
        const int bone = bs.bone_index;
        if (bone < 0)
        {
            continue;
        }
        const spinebend_bone_info& info = Bone_info[i];
        float heading;
        float pitch;
        if (aim == nullptr || !aim->bone_info[bone].flags.tmp_matrices_valid)
        {
            heading = 0.0f;
            pitch = 0.0f;
        }
        else
        {
            float bank;
            tmp_matrices[bone].m_rotation.extract_angles(pitch, bank, heading);
            if (i == 0)
            {
                float parent_pitch;
                float parent_bank;
                float parent_heading;
                tmp_matrices[bs.rig_bone_parent].m_rotation.extract_angles(parent_pitch, parent_bank, parent_heading);
                heading = heading - parent_heading;
                pitch = pitch - parent_pitch;
            }
        }
        // the limit products stay in double: the bone's room is product - angle, rounded once
        float multiplier = sbd->bone_max_angle_multiplier;
        const double max_heading = (double)info.max_heading * (double)multiplier;
        if ((double)heading <= max_heading)
        {
            float limit = (float)(max_heading - (double)heading);
            if (PI - limit < 0.0f)
            {
                limit = PI;
            }
            bs.max_heading = limit;
        }
        else
        {
            bs.max_heading = 0.0f;
        }
        const double min_heading = (double)info.min_heading * (double)multiplier;
        if (min_heading <= (double)heading)
        {
            float limit = (float)(min_heading - (double)heading);
            if (!(-PI - limit < 0.0f))
            {
                limit = -PI;
            }
            bs.min_heading = limit;
        }
        else
        {
            bs.min_heading = 0.0f;
        }
        if (sbd->flags.is_player)
        {
            multiplier = multiplier * 2.0f;
        }
        const double max_pitch = (double)info.max_pitch * (double)multiplier;
        bs.max_pitch = (double)pitch <= max_pitch ? (float)(max_pitch - (double)pitch) : 0.0f;
        const double min_pitch = (double)info.min_pitch * (double)multiplier;
        bs.min_pitch = min_pitch <= (double)pitch ? (float)(min_pitch - (double)pitch) : 0.0f;
        const int parent = Bone_parent[i];
        if (parent == -1)
        {
            chain_heading[i] = heading;
            chain_pitch[i] = pitch;
        }
        else
        {
            chain_heading[i] = chain_heading[parent] + heading;
            chain_pitch[i] = chain_pitch[parent] + pitch;
        }
        if (!sbd->flags.no_spinebending_last_frame)
        {
            bs.anim_heading_change = chain_heading[i] - bs.anim_last_bone_heading;
            bs.anim_pitch_change = chain_pitch[i] - bs.anim_last_bone_pitch;
        }
        else
        {
            bs.anim_heading_change = 0.0f;
            bs.anim_pitch_change = 0.0f;
        }
        bs.anim_last_bone_heading = chain_heading[i];
        bs.anim_last_bone_pitch = chain_pitch[i];
    }
}

// 0x00C901A0 (:622)
// The angle range a bone and its filtered children can take together (the bone's own pitch range, its heading range
// scaled down when its pitch is near its limit, and the children's tightest), and their total blend share; false for
// a bone the filter leaves out. The children's ranges are kept in the bone's state for calculate_angles.
bool process_bone_limits_sub(float& max_heading, float& min_heading, float& max_pitch, float& min_pitch, float& total_blend, human_spinebend_data* sbd, int bone_idx)
{
    const spinebend_bone_info& info = Bone_info[bone_idx];
    if ((sbd->bone_filter & info.bone_heading_filter) == 0 && (sbd->bone_filter & info.bone_pitch_filter) == 0)
    {
        return false;
    }
    max_heading = PI2;
    max_pitch = PI2;
    min_pitch = -PI2;
    min_heading = -max_heading;
    total_blend = 1.0f;
    int counted = 0;
    for (int i = 0; i < 3; i++)
    {
        const int child = Bone_children[bone_idx][i];
        // Original: the child list ends at an index below 1.
        if (child < 1)
        {
            break;
        }
        float child_max_heading;
        float child_min_heading;
        float child_max_pitch;
        float child_min_pitch;
        float child_blend;
        if (process_bone_limits_sub(child_max_heading, child_min_heading, child_max_pitch, child_min_pitch, child_blend, sbd, child))
        {
            if (!(max_heading - child_max_heading < 0.0f))
            {
                max_heading = child_max_heading;
            }
            if (min_heading - child_min_heading < 0.0f)
            {
                min_heading = child_min_heading;
            }
            if (!(max_pitch - child_max_pitch < 0.0f))
            {
                max_pitch = child_max_pitch;
            }
            // Original: the children's minimum pitch is the lowest, not the tightest.
            if (!(min_pitch - child_min_pitch < 0.0f))
            {
                min_pitch = child_min_pitch;
            }
            if (!(total_blend - child_blend < 0.0f))
            {
                total_blend = child_blend;
            }
            counted++;
        }
    }
    if (counted == 0)
    {
        max_heading = 0.0f;
        max_pitch = 0.0f;
        min_heading = 0.0f;
        min_pitch = 0.0f;
        total_blend = 0.0f;
    }
    spinebend_bone_state& bs = sbd->bone_state[bone_idx];
    bs.max_children_heading = max_heading;
    bs.min_children_heading = min_heading;
    bs.max_children_pitch = max_pitch;
    bs.min_children_pitch = min_pitch;
    bs.children_total_blend = total_blend;
    if ((sbd->bone_filter & info.bone_pitch_filter) == 0)
    {
        max_pitch = 0.0f;
        min_pitch = 0.0f;
    }
    else
    {
        max_pitch = bs.max_pitch + max_pitch;
        min_pitch = bs.min_pitch + min_pitch;
    }
    // Magic numbers: a spine bone pitched towards a limit over 0.1 rad keeps 50% to 100% of its heading range
    // (1.5 times the share of the pitch range left).
    const float desired_pitch = bs.desired_pitch;
    float heading_scale = 1.0f;
    float limit;
    if ((desired_pitch < 0.0f && (limit = bs.min_pitch) < -0.1f) || (0.0f < desired_pitch && 0.1f < (limit = bs.max_pitch)))
    {
        heading_scale = (float)(((double)limit - (double)desired_pitch) / (double)limit * 1.5);
    }
    if (heading_scale - 0.5f < 0.0f)
    {
        heading_scale = 0.5f;
    }
    if (!(heading_scale - 1.0f < 0.0f))
    {
        heading_scale = 1.0f;
    }
    if (bone_idx > 6)
    {
        heading_scale = 1.0f;
    }
    if ((sbd->bone_filter & info.bone_heading_filter) == 0)
    {
        max_heading = 0.0f;
        min_heading = 0.0f;
        total_blend = 0.0f;
        return true;
    }
    max_heading = (float)((double)bs.max_heading * (double)heading_scale + (double)max_heading);
    min_heading = (float)((double)bs.min_heading * (double)heading_scale + (double)min_heading);
    total_blend = info.blend_pct + total_blend;
    return true;
}

// 0x00C90590 (:714)
void process_bone_limits(human_spinebend_data* sbd)
{
    float max_heading;
    float min_heading;
    float max_pitch;
    float min_pitch;
    float total_blend;
    process_bone_limits_sub(max_heading, min_heading, max_pitch, min_pitch, total_blend, sbd, 0);
}

// 0x00C905C0 (:720)
// Shares a heading and pitch out between a bone (its blend share of what the children cannot take, within its own
// limits) and its children, which get the rest; returns the heading nobody could take.
float calculate_angles(float desired_heading, float desired_pitch, human_spinebend_data* sbd, int bone_idx, unsigned int filter)
{
    if (bone_idx == -1)
    {
        return 0.0f;
    }
    const spinebend_bone_info& info = Bone_info[bone_idx];
    spinebend_bone_state& bs = sbd->bone_state[bone_idx];
    if ((filter & info.bone_heading_filter) == 0 && (filter & info.bone_pitch_filter) == 0)
    {
        bs.desired_heading = 0.0f;
        bs.desired_pitch = 0.0f;
        for (int i = 0; i < 3; i++)
        {
            calculate_angles(desired_heading, desired_pitch, sbd, Bone_children[bone_idx][i], filter);
        }
        return 0.0f;
    }
    float child_pitch;
    if ((filter & info.bone_pitch_filter) == 0)
    {
        bs.desired_pitch = 0.0f;
        child_pitch = 0.0f;
    }
    else
    {
        float p = bs.min_children_pitch;
        if (!(desired_pitch - p < 0.0f))
        {
            p = desired_pitch;
        }
        child_pitch = bs.max_children_pitch;
        if (p - child_pitch < 0.0f)
        {
            child_pitch = p;
        }
        const float own = desired_pitch - child_pitch;
        const float min_pitch = bs.min_pitch;
        float clamped = min_pitch;
        if (!(own - min_pitch < 0.0f))
        {
            clamped = own;
        }
        float within = bs.max_pitch;
        if (clamped - within < 0.0f)
        {
            within = clamped;
        }
        // the share's sum stays in double; share * child + own is rounded once
        const float share = (float)((double)info.blend_pct / ((double)bs.children_total_blend + (double)info.blend_pct));
        float pitch = (float)((double)share * (double)child_pitch + (double)own);
        if (pitch - min_pitch < 0.0f)
        {
            pitch = min_pitch;
        }
        if (!(pitch - bs.max_pitch < 0.0f))
        {
            pitch = bs.max_pitch;
        }
        bs.desired_pitch = pitch;
        child_pitch = (float)((double)child_pitch - ((double)pitch - (double)within));
    }
    float child_heading = 0.0f;
    if ((filter & info.bone_heading_filter) == 0)
    {
        bs.desired_heading = 0.0f;
    }
    else
    {
        float h = bs.min_children_heading;
        if (!(desired_heading - h < 0.0f))
        {
            h = desired_heading;
        }
        child_heading = bs.max_children_heading;
        if (h - child_heading < 0.0f)
        {
            child_heading = h;
        }
        const float own = desired_heading - child_heading;
        const float min_heading = bs.min_heading;
        float clamped = min_heading;
        if (!(own - min_heading < 0.0f))
        {
            clamped = own;
        }
        float within = bs.max_heading;
        if (clamped - within < 0.0f)
        {
            within = clamped;
        }
        const float share = (float)((double)info.blend_pct / ((double)bs.children_total_blend + (double)info.blend_pct));
        float heading = (float)((double)share * (double)child_heading + (double)own);
        if (heading - min_heading < 0.0f)
        {
            heading = min_heading;
        }
        if (!(heading - bs.max_heading < 0.0f))
        {
            heading = bs.max_heading;
        }
        bs.desired_heading = heading;
        child_heading = (float)((double)child_heading - ((double)heading - (double)within));
    }
    for (int i = 0; i < 3; i++)
    {
        calculate_angles(child_heading, child_pitch, sbd, Bone_children[bone_idx][i], filter);
    }
    // x87: computed without rounding the difference
    return (float)((double)desired_heading - (double)bs.desired_heading - (double)child_heading);
}

// 0x00C9A860 (:806)
// The limits, then the head's heading and pitch shared over the spine (the bone filter) and what is left over the eyes
// (their own filter, priority and ramp), and the bones bent; sleep mode becomes possible once the head and eyes have
// arrived.
void process_spinebending(human_spinebend_data* sbd, float pitch, float heading, matrix43* tmp_matrices, float eye_pitch, float eye_heading, float implied_anim_heading)
{
    calculate_bone_limits(sbd, tmp_matrices);
    float max_heading;
    float min_heading;
    float max_pitch;
    float min_pitch;
    float total_blend;
    process_bone_limits_sub(max_heading, min_heading, max_pitch, min_pitch, total_blend, sbd, 0);
    const float remaining = calculate_angles(heading, pitch, sbd, 0, sbd->bone_filter);
    const spinebend_bone_state* bs = sbd->bone_state;
    // the spine's share (+ 0.0f as the original: a -0 sum is +0)
    const float spine_pitch = bs[0].pitch + 0.0f + bs[1].pitch + bs[2].pitch + bs[3].pitch + bs[4].pitch;
    const float spine_heading = bs[0].heading + 0.0f + bs[1].heading + bs[2].heading + bs[3].heading + bs[4].heading;
    const float eyes_pitch = eye_pitch - spine_pitch;
    const float eyes_heading = eye_heading - spine_heading;
    // Magic number: filter 0x40, the eyes.
    calculate_angles(eyes_heading, eyes_pitch, sbd, 8, 0x40);
    calculate_angles(eyes_heading, eyes_pitch, sbd, 7, 0x40);
    sbd->remaining_heading = remaining;
    float applied_heading;
    float applied_pitch;
    interp_bones(sbd, 0, applied_heading, applied_pitch);
    const float priority = sbd->lookat_priority;
    sbd->lookat_priority = sbd->eye_lookat_priority;
    float eye_applied_heading;
    float eye_applied_pitch;
    interp_bones(sbd, 8, eye_applied_heading, eye_applied_pitch);
    interp_bones(sbd, 7, eye_applied_heading, eye_applied_pitch);
    sbd->lookat_priority = priority;
    // Magic number: arrived within 0.0001 rad.
    if (sbd->flags.sleep_mode_ready && fabsf(applied_heading) < 0.0001f && fabsf(applied_pitch) < 0.0001f)
    {
        sbd->flags.sleep_mode = 1;
    }
    const float axis_heading = applied_heading + implied_anim_heading;
    bend_bones(sbd, 8, axis_heading);
    bend_bones(sbd, 7, axis_heading);
    bend_bones(sbd, 0, axis_heading);
    sbd->flags.no_spinebending_last_frame = 0;
}

// 0x00C9AB60 (:881)
// The spine bend bone's heading change this frame, the head and eye directions as angles (the head's in the bone's
// frame when aiming), zeroed by the flags; nothing left to do puts a sleep-ready bend to sleep. Aiming adds the drift.
void animlib_spinebend_process(human_spinebend_data* sbd, matrix43* tmp_matrices)
{
    if (sbd->flags.disabled)
    {
        return;
    }
    float bone_heading = 0.0f;
    sbd->spinebend_bone_rotation = 0.0f;
    const anim_instance_manager* aim = aim_from_handle(sbd->aim_handle);
    const int sb_bone = sbd->spinebend_bone_index;
    const bool bone_valid = tmp_matrices != nullptr && sb_bone > -1 && aim->bone_info[sb_bone].flags.tmp_matrices_valid;
    if (bone_valid)
    {
        tmp_matrices[sb_bone].m_rotation.extract_heading(bone_heading);
        if (sbd->spinebend_bone_last_heading != 0.0f)
        {
            sbd->spinebend_bone_rotation = bone_heading - sbd->spinebend_bone_last_heading;
        }
    }
    sbd->spinebend_bone_last_heading = bone_heading;
    vector head = sbd->head_dest_vec;
    if (sbd->flags.aim_at_pos && bone_valid)
    {
        head.rotate(tmp_matrices[sb_bone].m_rotation);
    }
    float pitch = -(float)asin((double)head.y);
    float heading = fl_atan2_safe(head.x, head.z);
    float eye_pitch = -(float)asin((double)sbd->eyes_dest_vec.y);
    float eye_heading = fl_atan2_safe(sbd->eyes_dest_vec.x, sbd->eyes_dest_vec.z);
    if (sbd->flags.zero_pitch)
    {
        pitch = 0.0f;
    }
    if (sbd->flags.zero_heading)
    {
        heading = 0.0f;
    }
    if (sbd->flags.zero_eye_pitch)
    {
        eye_pitch = 0.0f;
    }
    if (sbd->flags.zero_eye_heading)
    {
        eye_heading = 0.0f;
    }
    // Magic number: nothing to bend within 0.0001 rad.
    if (!((double)(heading - 0.0f) < (double)0.0001f) || !((double)-(heading - 0.0f) < (double)0.0001f) || !((double)(pitch - 0.0f) < (double)0.0001f) ||
        !((double)-(pitch - 0.0f) < (double)0.0001f))
    {
        sbd->flags.sleep_mode_ready = 0;
        sbd->flags.sleep_mode = 0;
    }
    else
    {
        sbd->flags.sleep_mode_ready = 1;
        if (sbd->flags.sleep_mode)
        {
            sbd->flags.no_spinebending_last_frame = 1;
            return;
        }
    }
    heading = fl_angle_cap_radians(heading, -3.14159274f);
    if (sbd->flags.aim_at_pos)
    {
        pitch = sbd->pitch_drift + pitch;
        heading = sbd->heading_drift + heading;
    }
    if (tmp_matrices != nullptr)
    {
        process_spinebending(sbd, pitch, heading, tmp_matrices, eye_pitch, eye_heading, bone_heading);
    }
    sbd->flags.spinebend_teleport = 0;
}
