// Original: volition/rfg/code/objects/humans/human_spinebend.cpp
//
// Game side of spine bending: picks where the head and eyes look (process_head_and_eye_vec, a small state machine of
// spinebend_interest values driven by vrand), turns the body toward the steering direction (human_spinebend_rotate_body,
// human_spinebend_rotate_orient), and fills human_spinebend_data for the anim_lib pass (human_spinebend_pre_process ->
// animlib_spinebend_process in the animation update; human_spinebend_post_transform after it).
//
// Float notes: MSVC promotes float expressions to double where it keeps several operations in one register; those
// places are written as double expressions rounded once (from the CVTPD2PS placement in rfg.exe). The constants are the
// float literals promoted (0.1f, 0.4f, 56.548668f, ...), not exact doubles.

#include "volition/rfg/code/objects/humans/human_spinebend.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/anim_lib/code/anim_lib_spinebend.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/os/frametime.h"

#include <math.h>
#include <string.h>

// ---- file-static globals
static const float PI = 3.14159274f; // 0x01310C80 (0x40490FDB; hdrgen printed 3.141593f)
static const float NORMAL_DEATH_SOUND_MAX_DIST = 20.0f; // 0x01310FF4
static const float COVER_NODE_MAX_NAVMESH_ATTACH_DIST = 0.7f; // 0x01311864
static const float NEAR_FOLLOW_DEST_DIST_SQ = 100.0f; // 0x01311B1C
// const float min_correction_diff (0x01315520) is used here too; placed in human.cpp (read earlier in the unity TU).
// player* Player (0x03023874) is used here too; placed in player.cpp.

// The spinebend bones, in human_spinebend_data::bone_state order.
static const char* Bone_names[9] = {"spine", "spine1", "spine2", "neck", "head", "r-upperarmtwist1", "l-upperarmtwist1", "r-eye", "l-eye"}; // 0x016943DC
// Below this dot product between the old and the new eye direction the eyes blink.
static float blarg = 0.939f; // 0x016AE138
// The player's rate scale for body turns while the jetpack runs.
static float jetpack_turn_rate = 0.4f; // 0x016AFE38
// Share of the camera's turn rate added to the end heading of a turn-in-place action.
static float scaler = 0.3f; // 0x016AFE3C
// Lets passengers aim with the single-handed spinebend filter (determine_spinebend_filter).
static bool Spinebend_passengers; // 0x0302F046

// ---- internal functions
static void process_head_and_eye_vec(human* hp, vector& lookat_fvec); // 0x00A0BE60 (:102)
static void human_spinebend_rotate_body(human* hp); // 0x00AB63F0 (:658)

static player* spinebend_get_player(human* hp)
{
    return hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)hp : nullptr;
}

// 0x00A0BE60 (:102)
// lookat_fvec is the look-at direction in the human's frame, normalized.
static void process_head_and_eye_vec(human* hp, vector& lookat_fvec)
{
    human_spinebend_data& sbd = hp->spinebend_data;
    if (hp->hflags.aim_at_pos || hp->cur_state == HS_COVER || hp->cur_state == HS_LADDER)
    {
        sbd.eyes_dest_vec = lookat_fvec;
        sbd.head_dest_vec = lookat_fvec;
        sbd.lookat_priority = hp->look_at_speed;
        sbd.main_interest = SI_CHOOSE_NEW_MAIN_FOCUS_PRIMARY;
        sbd.eye_lookat_priority = hp->look_at_speed;
        sbd.eyes_interest = SI_CHOOSE_NEW_MAIN_FOCUS;
        sbd.head_interest = SI_FOLLOW_EYES;
        return;
    }

    bool changed = false;
    // Magic number: 0.3, the cosine below which a look-at behind the human is replaced by straight ahead.
    if (lookat_fvec.x * 0.0f + lookat_fvec.y * 0.0f + lookat_fvec.z < 0.3f)
    {
        lookat_fvec = vector(0.0f, 0.0f, 1.0f);
    }
    // Magic numbers: a look-at change past 0.99 (about 8 degrees) or a lower body turning faster than 0.5 rad/s starts over.
    const float lower_body_speed = sbd.lower_body_heading_speed - 0.0f;
    if (sbd.last_lookat_vec.y * lookat_fvec.y + sbd.last_lookat_vec.x * lookat_fvec.x + sbd.last_lookat_vec.z * lookat_fvec.z < 0.99f ||
        0.5f <= lower_body_speed || 0.5f <= -lower_body_speed || hp->cur_movement_state == HMS_SPRINT ||
        (sbd.focus_target_ts.valid() && !sbd.focus_target_ts.elapsed()))
    {
        changed = true;
        sbd.main_interest = SI_CHOOSE_NEW_MAIN_FOCUS_PRIMARY;
        sbd.eyes_interest = SI_CHOOSE_NEW_MAIN_FOCUS;
        sbd.head_interest = SI_FOLLOW_EYES;
    }
    else
    {
        sbd.focus_target_ts.invalidate();
    }
    sbd.last_lookat_vec = lookat_fvec;
    const vector old_eyes = sbd.eyes_dest_vec;

    switch (sbd.main_interest)
    {
    case SI_CHOOSE_NEW_MAIN_FOCUS_PRIMARY:
        sbd.lookat_priority = hp->look_at_speed;
        sbd.eye_lookat_priority = hp->look_at_speed;
        // fall through
    case SI_CHOOSE_NEW_MAIN_FOCUS:
    {
        const unsigned int r = vrand();
        if ((sbd.main_interest != SI_CHOOSE_NEW_MAIN_FOCUS_PRIMARY && changed) || (r & 1) != 0 || sbd.main_interest == SI_CHOOSE_NEW_MAIN_FOCUS_PRIMARY)
        {
            sbd.main_interest = SI_MAIN_FOCUS_PRIMARY;
            sbd.main_interest_ts.set(int_rand_range(1000, 4000));
            sbd.eyes_interest = SI_CHOOSE_NEW_MAIN_FOCUS_PRIMARY;
            sbd.head_interest = SI_CHOOSE_NEW_MAIN_FOCUS_PRIMARY;
        }
        else
        {
            sbd.main_interest_ts.set(int_rand_range(1000, 6000));
            sbd.main_interest = SI_MAIN_FOCUS_RANDOM;
            sbd.flags.pan_dir = vrand() & 1;
        }
        break;
    }
    case SI_MAIN_FOCUS_PRIMARY:
    case SI_MAIN_FOCUS_RANDOM:
        if (sbd.main_interest_ts.elapsed() || changed)
        {
            sbd.main_interest = SI_CHOOSE_NEW_MAIN_FOCUS;
        }
        break;
    default:
        break;
    }

    vector& eyes = sbd.eyes_dest_vec;
    bool check_eyes_ts = false;
    switch (sbd.eyes_interest)
    {
    case SI_CHOOSE_NEW_MAIN_FOCUS_PRIMARY:
        sbd.eyes_interest = SI_MAIN_FOCUS_PRIMARY;
        sbd.eyes_orient_ts.set(int_rand_range(1000, 2000));
        // fall through
    case SI_MAIN_FOCUS_PRIMARY:
        eyes = lookat_fvec;
        check_eyes_ts = true;
        break;
    case SI_CHOOSE_NEW_MAIN_FOCUS:
        if (sbd.main_interest == SI_MAIN_FOCUS_PRIMARY)
        {
            const unsigned int r = vrand();
            if ((sbd.last_eyes_interest == SI_LOCAL_SCANNING && vrand() % 5 != 0) || r % 3 == 0)
            {
                sbd.eyes_interest = SI_LOCAL_SCANNING;
                sbd.eyes_orient_ts.set(int_rand_range(500, 1000));
            }
            else if (r % 3 == 1)
            {
                sbd.eyes_interest = SI_QUICK_GLANCE;
                sbd.eyes_orient_ts.set(int_rand_range(700, 1000));
            }
            else
            {
                sbd.eyes_interest = SI_MAIN_FOCUS_PRIMARY;
                sbd.eyes_orient_ts.set(int_rand_range(1000, 1500));
                sbd.head_interest = SI_MAIN_FOCUS_PRIMARY;
            }
            sbd.pan_heading = 0.0f;
            sbd.last_eyes_interest = sbd.eyes_interest;
        }
        else
        {
            if (sbd.main_interest == SI_MAIN_FOCUS_RANDOM)
            {
                const unsigned int r = vrand();
                if (r % 5 == 0)
                {
                    sbd.eyes_interest = SI_LOCAL_SCANNING;
                    sbd.eyes_orient_ts.set(int_rand_range(500, 1000));
                    sbd.last_eyes_interest = sbd.eyes_interest;
                    break;
                }
                if (r % 5 == 1)
                {
                    sbd.eyes_interest = SI_QUICK_GLANCE;
                    sbd.eyes_orient_ts.set(int_rand_range(700, 1000));
                    sbd.last_eyes_interest = sbd.eyes_interest;
                    break;
                }
                sbd.eyes_interest = SI_PANNING_START;
            }
            sbd.last_eyes_interest = sbd.eyes_interest;
        }
        break;
    case SI_HOLDING_FOCUS:
        check_eyes_ts = true;
        break;
    case SI_LOCAL_SCANNING:
        eyes.rand_around_dir(eyes, 0.995f, 1.0f);
        eyes.normalize();
        sbd.eyes_interest = SI_HOLDING_FOCUS;
        break;
    case SI_PANNING_START:
        sbd.pan_ts.set(0);
        sbd.head_orient_ts.set(int_rand_range(2000, 3500));
        sbd.eyes_orient_ts.set(int_rand_range(700, 1000));
        sbd.eyes_interest = SI_PANNING;
        sbd.head_interest = SI_PANNING_START;
        // fall through
    case SI_PANNING:
        if (sbd.pan_ts.elapsed())
        {
            // Magic numbers: one pause in nine lasts up to 3 s, the rest up to 0.8 s.
            sbd.pan_ts.set(int_rand_range(400, vrand() % 9 == 0 ? 3000 : 800));
            sbd.head_interest = SI_PANNING_START;
            sbd.eyes_orient_ts.set(1);
        }
        if (sbd.eyes_orient_ts.elapsed())
        {
            const float pan_heading = sbd.pan_heading;
            const bool panning_ahead = sbd.flags.pan_dir == 0 ? pan_heading < 0.0f : 0.0f < pan_heading;
            sbd.eye_lookat_priority = 0.6f;
            sbd.eyes_orient_ts.set(1000);
            if (vrand() % 20 == 0)
            {
                sbd.eyes_interest = SI_PANNING_GLANCE;
            }
            else if (panning_ahead || pan_heading == 0.0f)
            {
                sbd.eyes_interest = SI_PANNING_FOLLOW;
            }
            else
            {
                eyes.rand_around_dir(sbd.head_dest_vec, 0.99f, 1.0f);
                eyes.normalize();
                sbd.eyes_orient_ts.set(1500);
            }
        }
        break;
    case SI_PANNING_GLANCE:
        eyes.rand_around_dir(sbd.head_dest_vec, 0.98f, 1.0f);
        eyes.normalize();
        sbd.eye_lookat_priority = 0.8f;
        sbd.eyes_orient_ts.set(int_rand_range(700, 1500));
        sbd.eyes_interest = SI_PANNING;
        break;
    case SI_PANNING_FOLLOW:
        if ((vrand() & 3) == 0)
        {
            eyes = vector(0.0f, 0.0f, 1.0f);
            const spinebend_bone_state* bs = sbd.bone_state;
            float heading = bs[4].heading + bs[3].heading + bs[0].heading + bs[1].heading + bs[2].heading;
            const float pitch = bs[4].pitch + bs[3].pitch + bs[0].pitch + bs[1].pitch + bs[2].pitch;
            // Magic number: the eyes lead the pan by 0.26 rad (15 degrees).
            heading = sbd.flags.pan_dir == 0 ? heading - 0.26f : heading + 0.26f;
            eyes.rotate_around_y_axis(heading);
            eyes.rotate_around_x_axis(pitch);
        }
        if (sbd.eyes_orient_ts.elapsed())
        {
            sbd.eyes_interest = SI_PANNING;
        }
        break;
    case SI_QUICK_GLANCE:
    {
        const unsigned int r = vrand();
        sbd.eyes_interest = SI_HOLDING_FOCUS;
        float y;
        if (r % 5 == 0)
        {
            eyes.rand_around_dir(sbd.head_dest_vec, 0.8f, 1.0f);
            y = sbd.head_dest_vec.y * 0.9f + eyes.y * 0.1f;
            sbd.head_interest = SI_FOLLOW_EYES;
        }
        else
        {
            eyes.rand_around_dir(sbd.head_dest_vec, 0.97f, 1.0f);
            y = sbd.head_dest_vec.y * 0.8f + eyes.y * 0.2f;
        }
        eyes.y = y;
        eyes.normalize();
        break;
    }
    default:
        break;
    }
    if (check_eyes_ts && (sbd.eyes_orient_ts.elapsed() || changed))
    {
        sbd.eyes_interest = SI_CHOOSE_NEW_MAIN_FOCUS;
    }

    if (old_eyes.y * eyes.y + old_eyes.x * eyes.x + eyes.z * old_eyes.z < blarg)
    {
        hp->head_morphs.do_blink_close = 1;
    }

    switch (sbd.head_interest)
    {
    case SI_CHOOSE_NEW_MAIN_FOCUS_PRIMARY:
        sbd.head_interest = sbd.eyes_interest;
        // fall through
    case SI_CHOOSE_NEW_MAIN_FOCUS:
    case SI_MAIN_FOCUS_PRIMARY:
    {
        // x87: the CRT's asin rounded to float, negated and scaled in double. The head follows 40% of the pitch and
        // 80% of the heading, at most 90 degrees; past 108 degrees (0.6 pi) it looks straight ahead.
        const float asin_y = (float)asin((double)lookat_fvec.y);
        float pitch = (float)(-(double)asin_y * (double)0.4f);
        float heading = (float)((double)fl_atan2_safe(lookat_fvec.x, lookat_fvec.z) * (double)0.8f);
        if (heading < -1.88495564f || 1.88495564f < heading)
        {
            heading = 0.0f;
            pitch = 0.0f;
        }
        else if (heading < -1.57079637f)
        {
            heading = -1.57079637f;
        }
        else if (1.57079637f < heading)
        {
            heading = 1.57079637f;
        }
        sbd.head_dest_vec = vector(0.0f, 0.0f, 1.0f);
        sbd.head_dest_vec.rotate_around_x_axis(pitch);
        sbd.head_dest_vec.rotate_around_y_axis(heading);
        const float priority = sbd.lookat_priority;
        if (priority < hp->look_at_speed)
        {
            // Magic number: the priority ramps up by 0.1 per second.
            sbd.lookat_priority = Frametime * 0.1f + priority;
            return;
        }
        break;
    }
    case SI_PANNING_START:
    {
        float pan_scale = 1.0f;
        sbd.head_interest = SI_PANNING;
        sbd.lookat_priority = fl_rand_range(0.01f, 0.05f);
        if (vrand() % 15 == 0)
        {
            // One pan in fifteen is a wide, faster one.
            pan_scale = 3.0f;
            sbd.lookat_priority = fl_rand_range(0.03f, 0.1f);
        }
        // Magic number: the pan stays within 0.6 rad (34 degrees) of the look-at direction.
        if (sbd.flags.pan_dir == 0)
        {
            const float pan_heading = sbd.pan_heading;
            sbd.pan_heading = pan_heading - fl_rand_range(0.1f, 0.4f) * pan_scale;
            if (sbd.pan_heading < -0.6f)
            {
                sbd.pan_heading = -0.6f;
                if ((vrand() & 1) != 0)
                {
                    sbd.flags.pan_dir = 1;
                }
            }
        }
        else
        {
            sbd.pan_heading = fl_rand_range(0.1f, 0.4f) * pan_scale + sbd.pan_heading;
            if (0.6f < sbd.pan_heading)
            {
                sbd.pan_heading = 0.6f;
                if ((vrand() & 1) != 0)
                {
                    sbd.flags.pan_dir = 0;
                }
            }
        }
        sbd.head_dest_vec = lookat_fvec;
        sbd.head_dest_vec.rotate_around_y_axis(sbd.pan_heading);
        if ((vrand() & 3) == 0)
        {
            sbd.head_dest_vec.rotate_around_x_axis(fl_rand_range(-0.1f, 0.1f));
        }
        sbd.head_dest_vec.normalize();
    }
        // fall through
    case SI_PANNING:
        if (sbd.head_orient_ts.elapsed() || changed)
        {
            sbd.eyes_interest = SI_CHOOSE_NEW_MAIN_FOCUS;
            sbd.head_interest = SI_HOLDING_FOCUS;
            return;
        }
        break;
    case SI_FOLLOW_EYES:
    {
        // The head goes 40% of the way to a blend of 80% look-at and 20% body forward, 60% toward the eyes (each
        // product and sum rounded to float).
        const vector old_head = sbd.head_dest_vec;
        const vector& fvec = hp->orient.fvec;
        sbd.head_dest_vec.x = (lookat_fvec.x * 0.8f + fvec.x * 0.2f) * 0.4f + eyes.x * 0.6f;
        sbd.head_dest_vec.y = (lookat_fvec.y * 0.8f + fvec.y * 0.2f) * 0.4f + eyes.y * 0.6f;
        sbd.head_dest_vec.z = (lookat_fvec.z * 0.8f + fvec.z * 0.2f) * 0.4f + eyes.z * 0.6f;
        sbd.head_dest_vec.normalize();
        if (!changed)
        {
            const float dot = sbd.head_dest_vec * old_head;
            sbd.head_interest = SI_HOLDING_FOCUS;
            // x87: the larger the head move, the higher the priority (0.01 to 0.41).
            sbd.lookat_priority = (float)((1.0 - (double)dot) * (double)0.2f + (double)0.01f);
            return;
        }
        sbd.lookat_priority = 1.0f;
    }
        // fall through
    case SI_QUICK_GLANCE:
        sbd.head_interest = SI_HOLDING_FOCUS;
        break;
    default:
        break;
    }
}

// 0x00A82D70 (:445)
// The bone filter for animlib_spinebend_process (bits of the 9 spinebend bones; 0 disables spine bending).
unsigned int determine_spinebend_filter(human* hp)
{
    const bool manning_turret = hp->char_inst->get_animation_group() == ANIMATION_GROUP_TURRET_MACHINEGUN;
    // Editor flag 0x80: the animation disables spine bending (unless it is ramping out or the state is blending in).
    bool anim_disables = vanim_check_editor_flag(hp->char_inst->vp, 0x80);
    if (anim_disables && !vanim_check_editor_flag_ramped(hp->char_inst->vp, 0x80))
    {
        const v_anim* desired = hp->char_inst->vp->desired_state;
        if (desired == nullptr || 0.5f < desired->blend)
        {
            anim_disables = false;
        }
    }
    if (hp->is_killed() || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE || anim_disables ||
        hp->ragdoll_state == HUMAN_RAGDOLL_STATE_BLEND_OUT || human_in_vehicle_transition(hp, nullptr, VI_INTERACTION_NONE))
    {
        return 0;
    }
    if ((hp->cur_state == HS_VEHICLE_SITTING || hp->cur_state == HS_VEHICLE_DRIVING) && !manning_turret)
    {
        if (vanim_get_anim_handle(hp->char_inst->vp, ANIM_ALIAS_VEHICLE_TRANSITION) == 0)
        {
            return 1;
        }
        return hp->hflags.aim_at_pos ? 0x20 : 1;
    }
    if (hp->combat.cur_melee_combat_move != nullptr && vanim_is_playing_action(hp->char_inst->vp, ANIM_ALIAS_COMBAT_MOVE) &&
        !vanim_has_trigger_elapsed(hp->char_inst->vp, Trigger_enable_spinebend, ANIM_ALIAS_COMBAT_MOVE, -1))
    {
        return 0;
    }
    if (human_weapons_is_firearm_equipped(hp) && hp->hflags.aim_at_pos)
    {
        if (manning_turret)
        {
            return 9;
        }
        if (!human_weapons_is_two_handed_firearm_equipped(hp))
        {
            if (Spinebend_passengers && hp->vehicle_seat_idx == VSI_PASSENGER_2)
            {
                return 0x15;
            }
            return 0xB;
        }
        return 0x20;
    }
    if (manning_turret)
    {
        return 0x80;
    }
    if (!hp->hflags.aim_at_pos || hp->cur_state != HS_COVER)
    {
        return 1;
    }
    v_animate* vp = hp->char_inst->vp;
    if (vp->default_group_index != ANIMATION_GROUP_REMOTE_CHARGE)
    {
        return 1;
    }
    if (!vanim_is_playing_action(vp, ANIM_ALIAS_THROW))
    {
        return 0;
    }
    if (vanim_has_trigger_elapsed(hp->char_inst->vp, Trigger_impact_start, ANIM_ALIAS_THROW, -1))
    {
        return 0;
    }
    return 0x20;
}

// 0x00A315B0 (:539)
void human_spinebend_reset(human* hp)
{
    human_spinebend_data& sbd = hp->spinebend_data;
    memset(&sbd, 0, sizeof(human_spinebend_data)); // 0x410
    for (int i = 0; i < 9; i++)
    {
        spinebend_bone_state& bs = sbd.bone_state[i];
        bs.bone_index = animlib_find_character_bone(hp->char_inst->aim_handle, Bone_names[i]);
        if (bs.bone_index < 0)
        {
            bs.rig_bone_parent = -1;
        }
        else
        {
            bs.rig_bone_parent = animlib_get_bone_parent_index(hp->char_inst->aim_handle, bs.bone_index);
            if (bs.rig_bone_parent == 0)
            {
                bs.rig_bone_parent = animlib_find_character_bone(hp->char_inst->aim_handle, "pelvis");
            }
        }
    }
    hp->look_at_pos = ZERO_VECTOR;
    hp->hflags.aim_at_pos = 0;
    hp->look_at_handle = 0xFFFFFFFF;
    sbd.spinebend_bone_index = animlib_find_character_bone(hp->char_inst->aim_handle, "spinebend");
    animlib_set_special_bone_blend_type(hp->char_inst->aim_handle, sbd.spinebend_bone_index);
    sbd.last_lookat_vec = FORWARD_VECTOR;
    sbd.pitch_drift = 0.0f;
    sbd.heading_drift = 0.0f;
    sbd.update_timestamp.invalidate();
    sbd.eyes_orient_ts.invalidate();
    sbd.head_orient_ts.invalidate();
    sbd.focus_target_ts.invalidate();
    sbd.main_interest = SI_CHOOSE_NEW_MAIN_FOCUS_PRIMARY;
    sbd.turn_speed = 1.0f;
    sbd.flags.sleep_mode = 1;
    sbd.flags.is_player = 0;
    if (hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        sbd.flags.is_player = 1;
    }
}

// 0x00A0CE00 (:590)
void human_spinebend_reset_target(human* hp)
{
    hp->look_at_pos = ZERO_VECTOR;
    hp->hflags.aim_at_pos = 0;
    hp->look_at_handle = 0xFFFFFFFF;
}

// 0x00A0CE40 (:602)
void human_spinebend_disable(human* hp)
{
    hp->spinebend_data.flags.disabled = 1;
}

// 0x00A0CE50 (:615)
void human_spinebend_enable(human* hp)
{
    hp->spinebend_data.flags.disabled = 0;
}

// 0x00A0CE60 (:628)
bool human_spinebend_is_enabled(human* hp)
{
    return hp->spinebend_data.flags.disabled == 0;
}

// 0x00A0CE70 (:636)
// Turns the human by the animation's own heading change plus heading_change (dropped while the animation turns by 0.01
// rad or more this frame) and records the lower body turn speed, capped at 6 pi (1080 degrees) per second.
void human_spinebend_rotate_orient(human* hp, float heading_change)
{
    // Magic number: 4800 animation ticks per second.
    float total = animlib_get_heading_change(hp->char_inst->aim_handle, (int)(Frametime * 4800.0f));
    if (0.01f <= total - 0.0f || 0.01f <= -(total - 0.0f))
    {
        heading_change = 0.0f;
    }
    total = total + heading_change;
    if (total != 0.0f)
    {
        matrix turn;
        turn.set(0.0f, 0.0f, total);
        hp->orient = turn * hp->orient;
        hp->orient.orthogonalize();
    }
    human_spinebend_data& sbd = hp->spinebend_data;
    sbd.lower_body_rotation = total;
    float speed = total / Frametime;
    sbd.lower_body_heading_speed = speed;
    if (speed - -18.849556f < 0.0f)
    {
        speed = -18.849556f;
    }
    if (0.0f <= speed - 18.849556f)
    {
        speed = 18.849556f;
    }
    sbd.lower_body_heading_speed = speed;
}

// 0x00AB63F0 (:658)
// Turns the body toward the steering direction (or the look-at direction when standing), by ramped turn speed, a
// shuffle animation for large turns in place, or directly to the camera in first person.
static void human_spinebend_rotate_body(human* hp)
{
    if (!hp->rdd.is_loaded())
    {
        return;
    }
    // Restored on the early exits that do not turn; set after a HUMAN_ROTATE_USE_LIMITS turn.
    const bool heading_updated = hp->hflags.rotate_internal_heading_updated != 0;
    hp->hflags.rotate_internal_heading_updated = 0;
    player* pp = spinebend_get_player(hp);
    human_spinebend_data& sbd = hp->spinebend_data;

    // Turning is left to the animation (or physics) in these states.
    if ((hp->char_inst->vp->flags.adjusting_orient || human_combat_get_manned_turret(hp) != nullptr || hp->cur_state == HS_FALL ||
         vanim_check_editor_flag(hp->char_inst->vp, 2) || (hp->cur_state == HS_JUMP && !human_controller_is_on_ground(hp))) &&
        (!hp->hflags.in_air_melee || !vanim_is_playing_action(hp->char_inst->vp, ANIM_ALIAS_COMBAT_MOVE)) &&
        (pp == nullptr || !pp->pflags.jetpack_started))
    {
        human_spinebend_rotate_orient(hp, 0.0f);
        return;
    }
    const bool is_player = pp != nullptr;
    sbd.lower_body_rotation = 0.0f;
    float rate_scale = 1.0f;
    if (hp->cur_state == HS_JUMP)
    {
        rate_scale = 3.0f;
    }
    vector dir;
    if (hp->block_movement_anim == -1 || (hp->block_movement_type != BLOCK_MOVEMENT_ORIENT && hp->block_movement_type != BLOCK_MOVEMENT_POS_AND_ORIENT))
    {
        if (pp != nullptr || hp->inertial_motion.is_null() || human_is_combat_ready(hp))
        {
            dir = hp->steering_vector;
        }
        else
        {
            dir = hp->inertial_motion.copy_flatten_normalize_safe();
        }
    }
    else
    {
        dir = hp->block_steering_vector;
        hp->steering_heading_offset = hp->block_steering_heading_offset;
        // Magic numbers: a blocking animation turns fast for its first 8 frames, then slowly.
        rate_scale = vanim_get_anim_frame(hp->char_inst->vp, hp->block_movement_anim) < 8 ? 1.7f : 0.2f;
    }
    dir.y = 0.0f;
    if (is_player)
    {
        if (pp->pflags.jetpack_started)
        {
            rate_scale = jetpack_turn_rate;
        }
        if (!pp->sprint_delay_timestamp.elapsed())
        {
            // Magic number: 5 m/s, the speed above which a sprint start turns slowly.
            rate_scale = 5.0f < pp->actual_velocity_mag || pp->actual_velocity_mag == 5.0f ? 0.4f : 1.0f;
        }
        if (Rfg_camera.mode == CAMERA_FIRST_PERSON_MODE)
        {
            matrix cam;
            cam.set(0.0f, 0.0f, Rfg_camera.lookaround_params.heading);
            dir = cam.fvec;
        }
    }
    const bool was_turning = hp->is_turning;
    hp->is_turning = false;
    hp->starting_turn = false;
    if (dir.is_null())
    {
        if (hp->hflags.anchor_orient || is_player || (hp->look_at_handle == 0xFFFFFFFF && hp->look_at_pos.is_null()) ||
            ((hp->hflags.bones_transformed_this_frame || hp->cur_movement_state != HMS_IDLE || hp->hflags.override_default_anim_state) &&
             !vanim_is_transitioning_to_still_state(hp->char_inst->vp)))
        {
            human_spinebend_rotate_orient(hp, 0.0f);
            hp->hflags.moving_and_transitioning_states = 0;
            return;
        }
        human_get_look_at_vector(hp, dir);
        dir.flatten_normalize_safe();
    }
    dir.normalize();
    const float offset = hp->steering_heading_offset - 0.0f;
    if (0.0001f <= offset || 0.0001f <= -offset)
    {
        const vector in = dir;
        vec_rot_point_around_line(dir, in, hp->steering_heading_offset, ZERO_VECTOR, UP_VECTOR);
    }

    // The heading change of the state animation's movement since last frame; a big one (more than 8 pi per second)
    // is a new animation and ignored.
    vector action_delta;
    vector state_delta;
    vector local_delta;
    animlib_get_position_change(action_delta, state_delta, local_delta, hp->char_inst->aim_handle);
    const float last_heading = fl_atan2_safe(sbd.state_controller_movement.x, sbd.state_controller_movement.z);
    float anim_turn = last_heading - fl_atan2_safe(state_delta.x, state_delta.z);
    if (sbd.state_controller_movement.is_null())
    {
        anim_turn = 0.0f;
    }
    sbd.state_controller_movement = state_delta;
    if ((double)Frametime * (double)25.132742f < (double)fabsf(anim_turn))
    {
        anim_turn = 0.0f;
    }
    const v_anim* desired = hp->char_inst->vp->desired_state;
    hp->hflags.moving_and_transitioning_states = (0.003f <= anim_turn - 0.0f || 0.003f <= -(anim_turn - 0.0f) || (desired != nullptr && desired->blend != 1.0f)) ? 1 : 0;

    matrix& orient = hp->orient;
    const vector rvec_flat = orient.rvec.copy_flatten_normalize();
    float side_dot = (float)((double)rvec_flat.y * (double)dir.y + (double)dir.x * (double)rvec_flat.x + (double)rvec_flat.z * (double)dir.z);
    const vector fvec_flat = orient.fvec.copy_flatten_normalize();
    float fwd_dot = (float)((double)fvec_flat.y * (double)dir.y + (double)fvec_flat.x * (double)dir.x + (double)fvec_flat.z * (double)dir.z);
    if (fwd_dot - -1.0f < 0.0f)
    {
        fwd_dot = -1.0f;
    }
    if (0.0f <= fwd_dot - 1.0f)
    {
        fwd_dot = 1.0f;
    }
    float angle = fl_acos(fwd_dot);
    // Magic number: 0.0005 rad, already facing the direction.
    if (angle - 0.0f < 0.0005f && -(angle - 0.0f) < 0.0005f)
    {
        hp->hflags.rotate_internal_heading_updated = heading_updated;
        human_spinebend_rotate_orient(hp, 0.0f);
        return;
    }
    hp->starting_turn = true;
    // Magic number: 6.5 (m/s squared), slow walkers wait for the foot plant before turning.
    if (!is_player && !hp->foot_plant_time.elapsed() && !was_turning && !hp->hflags.moving_and_transitioning_states &&
        !hp->velocity.is_null() && hp->velocity.mag_squared() < 6.5f)
    {
        hp->hflags.rotate_internal_heading_updated = heading_updated;
        human_spinebend_rotate_orient(hp, 0.0f);
        return;
    }
    if (hp->hflags.bones_transformed_this_frame)
    {
        sbd.update_timestamp.invalidate();
    }
    desired = hp->char_inst->vp->desired_state;
    float blend = 1.0f;
    if (desired != nullptr)
    {
        blend = desired->blend;
    }
    if (is_player && Rfg_camera.mode == CAMERA_FIRST_PERSON_MODE)
    {
        // First person: face the camera at once and put the camera on the camera bone.
        orient.fvec = dir;
        orient.orthogonalize();
        vector cam_pos = pp->camera_bone_transform.m_translation;
        cam_pos.unrotate(orient);
        cam_pos = vector(hp->pos.x + cam_pos.x, hp->pos.y + cam_pos.y, hp->pos.z + cam_pos.z);
        if (Rfg_camera.mode == CAMERA_FIRST_PERSON_MODE)
        {
            Rfg_camera.real_pos = cam_pos;
            Rfg_camera.ideal_pos = Rfg_camera.real_pos;
        }
        human_spinebend_rotate_orient(hp, 0.0f);
        return;
    }

    switch (hp->rotate_dir)
    {
    case HUMAN_ROTATE_SHORTEST:
        // Magic number: 3.13 rad, about a half turn, where the side is taken from the look-at direction instead.
        if (fabsf(angle) <= 3.13f)
        {
            if (side_dot < 0.0f)
            {
                angle = -angle;
            }
        }
        else
        {
            vector look;
            human_get_look_at_vector(hp, look);
            look.normalize_safe();
            if (orient.rvec * look < 0.0f)
            {
                angle = -angle;
            }
        }
        break;
    case HUMAN_ROTATE_CLOCKWISE:
    {
        if (side_dot < 0.0f)
        {
            angle = -angle;
        }
        angle = fl_angle_cap_radians(angle, 0.0f);
        // Back to the shortest turn once less than 1 rad is left.
        if (angle < 1.0f)
        {
            hp->rotate_dir = HUMAN_ROTATE_SHORTEST;
        }
        break;
    }
    case HUMAN_ROTATE_COUNTERCLOCKWISE:
    {
        if (side_dot < 0.0f)
        {
            angle = -angle;
        }
        angle = fl_angle_cap_radians(angle, -6.28318548f);
        if (-1.0f < angle)
        {
            hp->rotate_dir = HUMAN_ROTATE_SHORTEST;
        }
        break;
    }
    case HUMAN_ROTATE_USE_LIMITS:
    {
        if (side_dot < 0.0f)
        {
            angle = -angle;
        }
        vector look;
        human_get_look_at_vector(hp, look);
        const float look_heading = fl_atan2_safe(look.x, look.z);
        float internal = fl_angle_cap_radians(orient.extract_heading() - look_heading, hp->rotate_heading_left);
        if (!heading_updated)
        {
            hp->rotate_internal_heading = internal;
        }
        else if (3.14159274f < fabsf(internal - hp->rotate_internal_heading))
        {
            internal = internal <= hp->rotate_internal_heading ? internal + 6.28318548f : internal - 6.28318548f;
        }
        if (-1.57079637f <= hp->rotate_internal_heading)
        {
            if (1.57079637f < hp->rotate_internal_heading || hp->rotate_heading_left <= internal + angle)
            {
                if (hp->rotate_heading_right < internal + angle)
                {
                    angle = angle - 6.28318548f;
                }
            }
            else
            {
                angle = angle + 6.28318548f;
            }
        }
        else if (internal + angle < hp->rotate_heading_left)
        {
            angle = angle + 6.28318548f;
        }
        hp->hflags.rotate_internal_heading_updated = 1;
        hp->rotate_internal_heading = internal;
        break;
    }
    default:
        break;
    }

    // The turn this frame.
    const float anim_turn_abs = fabsf(anim_turn);
    float turn = 0.0f;
    if (0.0005f <= anim_turn_abs)
    {
        turn = 0.0f;
    }
    else
    {
        // Magic number: 18 pi (3240 degrees) per second squared at turn_speed 1; 35% of it when walking relaxed.
        float speed = sbd.turn_speed * 56.548668f;
        if (hp->cur_movement_state == HMS_WALK && !human_is_combat_ready(hp))
        {
            speed = (float)((double)speed * (double)0.35f);
        }
        bool ramp = true;
        if (blend < 1.0f)
        {
            // While the state animation blends, the turn follows the blend: what is left of the angle over the time
            // the blend still takes.
            v_animate* vp = hp->char_inst->vp;
            const double unblended = 1.0 - (double)blend;
            turn = (float)((double)Frametime * (double)angle / ((double)vp->state_blend_rate * unblended + (double)Frametime) * (double)rate_scale);
            if (0.3f < fabsf(turn))
            {
                // Magic numbers: a big turn slows the blend by 0.1 per frame, at most to 0.45.
                float rate = (float)((double)vp->state_blend_rate + (double)0.1f);
                if ((float)((double)0.45f - (double)rate) < 0.0f)
                {
                    rate = 0.45f;
                }
                vp->state_blend_rate = rate;
                turn = (float)((double)Frametime * (double)angle / ((double)hp->char_inst->vp->state_blend_rate * unblended + (double)Frametime) * (double)rate_scale);
            }
            if (fabsf(turn) <= 0.6f)
            {
                const float low = (float)((double)-speed * (double)Frametime * (double)rate_scale);
                if ((float)((double)turn - (double)low) < 0.0f)
                {
                    turn = low;
                }
                const float high = (float)((double)speed * (double)Frametime * (double)rate_scale);
                if (0.0f <= (float)((double)turn - (double)high))
                {
                    turn = high;
                }
                sbd.lower_body_heading_speed = turn / Frametime;
                ramp = false;
            }
        }
        if (ramp)
        {
            turn = fl_ramp_to_value(0.0f, angle, (float)((double)speed * 2.0 * (double)rate_scale), sbd.lower_body_heading_speed, false);
        }
    }
    float rotation = angle;
    if ((double)fabsf(turn) + (double)anim_turn_abs <= (double)fabsf(angle))
    {
        rotation = angle <= turn ? turn - anim_turn_abs : anim_turn_abs + turn;
    }

    // Standing turns of more than 0.3 rad play a shuffle animation that does the turn.
    if (hp->cur_state == HS_STANDARD && hp->cur_movement_state == HMS_IDLE && !is_player && blend == 1.0f && (0.3f < angle || angle < -0.3f))
    {
        const bool right = 0.3f < angle;
        const vanim_aliases alias = right ? ANIM_ALIAS_TURN_RIGHT : ANIM_ALIAS_TURN_LEFT;
        if (!hp->char_inst->is_playing_action(alias))
        {
            hp->char_inst->stop_action(right ? ANIM_ALIAS_TURN_LEFT : ANIM_ALIAS_TURN_RIGHT, 3, true);
            animation_action anim_index = ANIMATION_ACTION_NONE;
            const bool relaxed = !hp->combat.combat_ready_timestamp.valid() || hp->combat.combat_ready_timestamp.elapsed();
            if (hp->cur_stance == HST_STAND)
            {
                anim_index = relaxed ? (right ? ANIMATION_ACTION_STAND_SHUFFLE_RIGHT : ANIMATION_ACTION_STAND_SHUFFLE_LEFT)
                                     : (right ? ANIMATION_ACTION_STAND_READY_SHUFFLE_RIGHT : ANIMATION_ACTION_STAND_READY_SHUFFLE_LEFT);
            }
            else if (hp->cur_stance == HST_CROUCH)
            {
                anim_index = relaxed ? (right ? ANIMATION_ACTION_CROUCH_SHUFFLE_RIGHT : ANIMATION_ACTION_CROUCH_SHUFFLE_LEFT)
                                     : (right ? ANIMATION_ACTION_CROUCH_READY_SHUFFLE_RIGHT : ANIMATION_ACTION_CROUCH_READY_SHUFFLE_LEFT);
            }
            if (anim_index != ANIMATION_ACTION_NONE)
            {
                hp->char_inst->play_action(anim_index, alias, 1.0f, 0);
                matrix dest;
                dest.make(dir);
                vanim_set_action_dest_orient(hp->char_inst->vp, hp->orient, dest, alias, -1.0f);
            }
        }
        rotation = 0.0f;
        sbd.lower_body_heading_speed = 0.0f;
    }
    human_spinebend_rotate_orient(hp, rotation);
    hp->is_turning = true;
}

// 0x00AB7480 (:1059)
void human_spinebend_pre_process(human* hp)
{
    human_spinebend_data& sbd = hp->spinebend_data;
    sbd.flags.do_spinebend_process = 0;
    human_spinebend_rotate_body(hp);
    // object_render_distance::get_apparent_distance, inlined (cached per Framecount).
    const float distance = hp->render_distance.get_apparent_distance(hp->pos);
    // Magic number: heads and eyes track within 30 m (apparent distance) or when aiming, in cover or on a ladder.
    if (distance <= 30.0f || hp->hflags.aim_at_pos || hp->cur_state == HS_COVER || hp->cur_state == HS_LADDER)
    {
        vector look;
        if (hp != nullptr)
        {
            vector eye_pos;
            human_get_eye_pos_orient(hp, eye_pos, nullptr);
            human_get_look_at_vector_from_eye_pos(hp, eye_pos, look);
        }
        look.rotate(hp->orient);
        if (look.is_null() || !hp->rdd.is_loaded())
        {
            return;
        }
        look.normalize();
        process_head_and_eye_vec(hp, look);
    }
    else
    {
        sbd.head_dest_vec = vector(0.0f, 0.0f, 1.0f);
        sbd.eyes_dest_vec = vector(0.0f, 0.0f, 1.0f);
    }
    // Magic number: past 20 m the bending updates less often.
    sbd.flags.delay_processing = 20.0f < distance ? 1 : 0;
    const v_anim* desired = hp->char_inst->vp->desired_state;
    sbd.flags.force_update = ((desired != nullptr && desired->blend != 1.0f) || hp->char_inst->is_playing_action(ANIM_ALIAS_TURN_LEFT) ||
                              hp->char_inst->is_playing_action(ANIM_ALIAS_TURN_RIGHT)) ? 1 : 0;
    spinebend_flags flags = sbd.flags;
    sbd.bone_max_angle_multiplier = 1.0f;
    if (sbd.flags.is_player)
    {
        // The player's bones may bend further: by stance, weapon group and zoom.
        float multiplier;
        if (Rfg_camera.mode == CAMERA_FIRST_PERSON_MODE)
        {
            multiplier = 100.0f;
        }
        else
        {
            v_animate* vp = hp->char_inst->vp;
            desired = vp->desired_state;
            if (desired == nullptr || vp->secondary_state == nullptr)
            {
                player* pp = spinebend_get_player(hp);
                if (pp != nullptr && pp->zoom_state == PZS_FINE_AIM)
                {
                    multiplier = 1.4f;
                }
                // Original bug kept: pp is used unchecked below (always the player here, as is_player is set only for one).
                else if (pp->cur_stance == HST_CROUCH)
                {
                    const human_move_submodes submode = pp->movement_submode;
                    if (0.001f < pp->actual_velocity_mag || submode == HMSM_COVER_RIGHT_LEAN || submode == HMSM_COVER_LEFT_LEAN ||
                        submode == HMSM_COVER_RIGHT_LEANUP || submode == HMSM_COVER_LEFT_LEANUP)
                    {
                        multiplier = 1.1f;
                    }
                    else
                    {
                        const animation_group group = pp->char_inst->vp->default_group_index;
                        if (pp->current_team == HUMAN_TEAM_MARAUDER && (group == ANIMATION_GROUP_RPG || group == ANIMATION_GROUP_GRINDER || group == ANIMATION_GROUP_ARC))
                        {
                            multiplier = 0.45f;
                        }
                        else
                        {
                            multiplier = 0.6f;
                        }
                    }
                }
                else
                {
                    const animation_group group = pp->char_inst->vp->default_group_index;
                    multiplier = group == ANIMATION_GROUP_GUTTER || group == ANIMATION_GROUP_HAMMER ? 0.7f : 1.1f;
                }
            }
            else
            {
                // While two states blend, the limit goes up to 2.1 at half blend.
                const float state_blend = desired->blend;
                if (state_blend <= 0.5f)
                {
                    multiplier = (float)((double)state_blend * 2.0 + (double)1.1f);
                }
                else
                {
                    multiplier = (float)((1.0 - (double)state_blend) * 2.0 + (double)1.1f);
                }
            }
        }
        sbd.bone_max_angle_multiplier = multiplier;
    }
    // zero_pitch, zero_heading, zero_eye_pitch and zero_eye_heading cleared, then set for the lower bone LODs.
    *(unsigned int*)&flags &= 0xFFFFFC3F;
    sbd.flags = flags;
    sbd.aim_handle = hp->char_inst->aim_handle;
    if (0 < hp->bone_lod_level && (!hp->hflags.aim_at_pos || 1 < hp->bone_lod_level))
    {
        *(unsigned int*)&flags |= 0x3C0;
        sbd.flags = flags;
    }
    if (hp->get_sub_type() == OT_SUB_HUMAN_PLAYER && Rfg_camera.mode == CAMERA_FIRST_PERSON_MODE)
    {
        sbd.flags.zero_heading = 1;
    }
    desired = hp->char_inst->vp->desired_state;
    sbd.flags.blending_anims = (desired == nullptr || 1.0f < desired->blend || desired->blend == 1.0f) ? 0 : 1;
    player* pp = spinebend_get_player(hp);
    sbd.flags.linear_interpolation = (sbd.flags.is_player && pp != nullptr && pp->zoom_state == PZS_FINE_AIM) ? 1 : 0;
    sbd.bone_filter = determine_spinebend_filter(hp);
    sbd.flags.aim_at_pos = hp->hflags.aim_at_pos;
    if (hp->hflags.aim_at_pos && hp->char_inst->is_playing_action(ANIM_ALIAS_RELOAD))
    {
        const weapon* wp = hp->get_equipped_weapon();
        if (wp != nullptr)
        {
            // Launchers (and weapon classes 9 and 10) do not pitch while reloading.
            const weapon_class_type weapon_class = wp->w_info->weapon_class;
            if (weapon_class == WEAPON_CLASS_LAUNCHER || (8 < (int)weapon_class && (int)weapon_class < 11))
            {
                sbd.flags.zero_pitch = 1;
            }
        }
    }
    sbd.flags.do_spinebend_process = 1;
    animlib_allocate_bone_modifiers(sbd.aim_handle);
}

// 0x00AB78F0 (:1210)
void human_spinebend_post_transform(human* hp)
{
    if (hp->block_movement_anim != -1)
    {
        bool release = true;
        if (hp->char_inst->is_playing_action(hp->block_movement_anim))
        {
            release = vanim_has_trigger_just_elapsed(hp->char_inst->vp, Trigger_release_movement, hp->block_movement_anim, -1);
            if (release)
            {
                hp->released_movement_anim = hp->block_movement_anim;
                hp->released_movement_type = hp->block_movement_type;
            }
        }
        if (release)
        {
            hp->hflags.block_forced_movement = 0;
            hp->block_movement_anim = -1;
            hp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
            hp->block_steering_heading_offset = 0.0f;
        }
    }
    human_spinebend_data& sbd = hp->spinebend_data;
    if (!sbd.flags.do_spinebend_process)
    {
        return;
    }
    if (!hp->hflags.anchor_orient)
    {
        bool turning_in_place = false;
        if (hp->char_inst->is_playing_action(ANIM_ALIAS_TURN_LEFT) || hp->char_inst->is_playing_action(ANIM_ALIAS_TURN_RIGHT))
        {
            turning_in_place = true;
            v_animate* vp = hp->char_inst->vp;
            v_anim* action = nullptr;
            if ((player*)hp == Player && vp->flags.adjusting_orient &&
                ((action = vanim_get_action(vp, ANIM_ALIAS_TURN_LEFT)) != nullptr || (action = vanim_get_action(hp->char_inst->vp, ANIM_ALIAS_TURN_RIGHT)) != nullptr))
            {
                // The player's turn-in-place ends where the camera will be: add the camera's turn over the rest of the
                // animation and spread what is left over the remaining time (x87).
                const anim* ap = anim_get_anim_from_handle(action->anim_handle);
                int prev_time;
                int cur_time;
                animlib_get_animation_times(hp->char_inst->aim_handle, action->instance, prev_time, cur_time);
                const float length = animlib_get_animation_length(ap->animlib_handle);
                // Magic number: 4800 animation ticks per second.
                const float elapsed_time = (float)((double)cur_time / 4800.0);
                if (elapsed_time < length)
                {
                    const float total_rotation = animlib_get_total_rotation(ap->animlib_handle);
                    float camera_heading;
                    Rfg_camera.real_orient.extract_heading(camera_heading);
                    const double remaining = (double)length - (double)elapsed_time;
                    camera_heading = (float)((double)Rfg_camera.lookaround_params.rotvel_h * remaining * (double)scaler + (double)camera_heading);
                    float heading;
                    hp->orient.extract_heading(heading);
                    const float left = fl_angle_cap_radians((camera_heading - heading) - (float)(remaining / (double)length * (double)total_rotation), -3.14159274f);
                    float delta = (left * length) / (float)remaining;
                    if (delta - -3.14159274f < 0.0f)
                    {
                        delta = -3.14159274f;
                    }
                    if (0.0f <= delta - 3.14159274f)
                    {
                        delta = 3.14159274f;
                    }
                    hp->char_inst->vp->end_heading_delta = delta;
                }
            }
        }
        const bool standing_still = hp->steering_vector.is_null() && !hp->is_turning;
        if (hp->cur_state == HS_STANDARD && hp->cur_movement_state == HMS_IDLE && !hp->hflags.moving_and_transitioning_states && standing_still)
        {
            const float remaining = sbd.remaining_heading;
            // Magic number: 0.001 rad of heading left by the spine bending starts a shuffle turn.
            if (remaining != 0.0f && !turning_in_place && (0.001f < remaining || remaining < -0.001f))
            {
                const bool right = 0.001f < remaining;
                int anim_index = -1;
                const bool relaxed = !hp->combat.combat_ready_timestamp.valid() || hp->combat.combat_ready_timestamp.elapsed();
                if (hp->cur_stance == HST_STAND)
                {
                    anim_index = relaxed ? (right ? ANIMATION_ACTION_STAND_SHUFFLE_RIGHT : ANIMATION_ACTION_STAND_SHUFFLE_LEFT)
                                         : (right ? ANIMATION_ACTION_STAND_READY_SHUFFLE_RIGHT : ANIMATION_ACTION_STAND_READY_SHUFFLE_LEFT);
                }
                else if (hp->cur_stance == HST_CROUCH)
                {
                    anim_index = relaxed ? (right ? ANIMATION_ACTION_CROUCH_SHUFFLE_RIGHT : ANIMATION_ACTION_CROUCH_SHUFFLE_LEFT)
                                         : (right ? ANIMATION_ACTION_CROUCH_READY_SHUFFLE_RIGHT : ANIMATION_ACTION_CROUCH_READY_SHUFFLE_LEFT);
                }
                // Magic numbers: the local player turning the camera faster than 5 rad/s or with more than 0.45 rad left
                // does a 180 shuffle (stand, or crouch for any other stance).
                const bool local_player = object_is_local_player(hp);
                const bool quick = local_player && (right ? (5.0f < Rfg_camera.lookaround_params.rotvel_h || (double)0.45f < (double)remaining)
                                                          : (Rfg_camera.lookaround_params.rotvel_h < -5.0f || (double)remaining < -(double)0.45f));
                if (quick)
                {
                    anim_index = (hp->cur_stance != HST_STAND ? 6 : 0) + (right ? ANIMATION_ACTION_STAND_SHUFFLE_RIGHT_180 : ANIMATION_ACTION_STAND_SHUFFLE_LEFT_180);
                }
                if (anim_index != -1)
                {
                    const vanim_aliases alias = right ? ANIM_ALIAS_TURN_RIGHT : ANIM_ALIAS_TURN_LEFT;
                    hp->char_inst->play_action((animation_action)anim_index, alias, 1.0f, 0);
                    if (object_is_local_player(hp))
                    {
                        player* pp = World.object_cast<player>(hp);
                        if (pp->pflags.quick_turn)
                        {
                            pp->char_inst->set_action_dest_orient(pp->orient, pp->quick_turn_orient, alias, pp->quick_turn_speed);
                        }
                        else
                        {
                            vector look;
                            human_get_look_at_vector(hp, look);
                            bool have_dest = true;
                            vector dest_fvec;
                            if (object_is_local_player(hp))
                            {
                                dest_fvec = Rfg_camera.real_orient.fvec.copy_flatten_normalize_safe();
                            }
                            // Magic number: a look-at y of -6000 or more is no look-at (the original tests y only).
                            else if (look.y < -6000.0f)
                            {
                                look.flatten_normalize_safe();
                                dest_fvec = look;
                            }
                            else
                            {
                                have_dest = false;
                            }
                            if (have_dest)
                            {
                                matrix dest;
                                dest.make_quick(dest_fvec);
                                pp->char_inst->set_action_dest_orient(pp->orient, dest, alias, -1.0f);
                            }
                        }
                    }
                }
            }
        }
        else if (turning_in_place && (hp->cur_movement_state != HMS_IDLE || standing_still))
        {
            hp->char_inst->stop_action(ANIM_ALIAS_TURN_LEFT, 3, true);
            hp->char_inst->stop_action(ANIM_ALIAS_TURN_RIGHT, 3, true);
        }
    }
    // The eye lids follow the eye pitch (morph targets 0 and 1).
    const float right_eye_pitch = sbd.bone_state[8].pitch;
    if (-0.05f < right_eye_pitch)
    {
        hp->head_morphs.target[0].blend_target = (float)(((double)right_eye_pitch + (double)0.05f) * (double)1.8f);
    }
    const float left_eye_pitch = sbd.bone_state[7].pitch;
    if (-0.05f < left_eye_pitch)
    {
        hp->head_morphs.target[1].blend_target = (float)(((double)left_eye_pitch + (double)0.05f) * (double)1.8f);
    }
}
