// Original: volition/rfg/code/camera/camera_first_person.cpp
//
// The first person camera mode and the look input of the player's cameras (player_minimal.md P6):
// camera_first_person_pre_process is also the pre-process callback of the third person mode, so it turns the player's
// view in both: the mouse deltas (or the stick's turn rates, with acceleration above the ramp threshold) become the
// lookaround pitch and heading, and the ideal orientation. Compiled in rfg_camera_unity.cpp.
// camera_first_person_pre_process follows the asm: scalar multi-op expressions are computed in double and rounded once
// (the SSE2 code), the decays are x87 pow. Not ported: the debug console commands (unused_dcf_*).

#include "volition/rfg/code/camera/camera_first_person.h"

#include "volition/rfg/code/camera/camera_transition.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/control/control_gameplay.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/time/timestamp.h"

#include <math.h>

// ---- TU-private types (rfg.pdb; no header names them)
// Turn rates of the first person and third person camera, in radians per second (and per second squared).
// size 0x18
struct camera_sensitivity_settings
{
    float heading_speed; // 0x00
    float heading_accel; // 0x04
    float heading_speed_max; // 0x08
    float pitch_speed; // 0x0C
    float pitch_accel; // 0x10
    float pitch_speed_max; // 0x14
};

// ---- file-static globals
static const float Camera_first_person_gyro_base_sensitivity_pitch = 2.5f; // 0x012CFAA0
static const float Camera_first_person_gyro_base_sensitivity_heading = 5.0f; // 0x012CFAA4
static float Cur_fov_scale = 1.0f; // 0x01648170
// The gamepad values: the same numbers as the Camera_first_person_heading_*/pitch_* tweaks below.
static camera_sensitivity_settings Camera_sensitivity_defaults = {3.4f, 7.8f, 11.0f, 1.4f, 4.5f, 5.0f}; // 0x01648174
// The mouse values: no acceleration (0), so mouse deltas turn the camera directly.
static camera_sensitivity_settings Camera_sensitivity_mouse = {8.0f, 0.0f, 11.0f, 7.5f, 0.0f, 7.5f}; // 0x0164818C
static float First_person_fov = 45.0f; // 0x016481A4
static float Camera_first_person_fov_change_speed = 300.0f; // 0x016481AC
static float Camera_first_person_ramp_input_min = 0.97f; // 0x016481B0
static float Camera_first_person_pitch_speed = 1.4f; // 0x016481B4
static float Camera_first_person_pitch_acceleration = 4.5f; // 0x016481B8
static float Camera_first_person_pitch_speed_max = 5.0f; // 0x016481BC
static float Camera_first_person_heading_speed = 3.4f; // 0x016481C0
static float Camera_first_person_heading_acceleration = 7.8f; // 0x016481C4
static float Camera_first_person_heading_speed_max = 11.0f; // 0x016481C8
static float Camera_first_person_fineaim_multiplier = 0.4f; // 0x016481CC
static float Camera_first_person_scope_multiplier = 0.15f; // 0x016481D0
static float Camera_first_person_sprint_multiplier = 0.3f; // 0x016481D4
float Camera_first_person_look_sensitivity_pitch = 0.688888907f; // 0x016481D8 (global in rfg.exe), 0x3F305B06; the control options set it
float Camera_first_person_look_sensitivity_heading = 0.7f; // 0x016481DC (global in rfg.exe); the control options set it
static float Camera_turret_multiplier = 0.65f; // 0x016481E0
static vector Camera_first_person_offset(0.0f, 1.65f, 0.0f); // 0x0164896C
static timestamp_percent First_person_fov_transition_time; // 0x01648978, invalid until set (constructor)
static float MAX_PITCH_ANGLE = 1.255637f; // 0x01648980, about 72 degrees
static float First_person_fov_pre_transition; // 0x01DE48AC

// Function statics of camera_first_person_pre_process (the recoil recovery).
static float recoil_auto_correct_rate = 1.0f; // 0x01648C60
static float recoil_max_speed = 0.1f; // 0x01648C64
static float pitch_move_scale = 1.5f; // 0x01648C68

static void set_camera_sensitivity_setting(camera_sensitivity_settings settings); // 0x006C3F40 (:179)
static void cap_rotation_speed(float& speed); // 0x006C9C10 (:241)

// 0x006C3F40 (:179)
static void set_camera_sensitivity_setting(camera_sensitivity_settings settings)
{
    Camera_first_person_heading_speed = settings.heading_speed;
    Camera_first_person_heading_acceleration = settings.heading_accel;
    Camera_first_person_heading_speed_max = settings.heading_speed_max;
    Camera_first_person_pitch_speed = settings.pitch_speed;
    Camera_first_person_pitch_acceleration = settings.pitch_accel;
    Camera_first_person_pitch_speed_max = settings.pitch_speed_max;
}

// 0x006C3FA0 (:190)
// The mouse (or a Steam controller's camera) uses the mouse turn rates, a gamepad the defaults.
void camera_first_person_update_camera_sensitivity()
{
    if (control_mouse_camera_control(CAA_CAMERA_ROTATE, CAA_CAMERA_ELEVATE) || control_steam_camera_control())
    {
        set_camera_sensitivity_setting(Camera_sensitivity_mouse); // inlined in the binary
        return;
    }
    set_camera_sensitivity_setting(Camera_sensitivity_defaults); // inlined in the binary
}

// 0x006C4040 (:201)
float camera_first_person_get_gyro_base_sensitivity_pitch()
{
    return Camera_first_person_gyro_base_sensitivity_pitch;
}

// 0x006C4050 (:206)
float camera_first_person_get_gyro_base_sensitivity_heading()
{
    return Camera_first_person_gyro_base_sensitivity_heading;
}

// 0x006C4060 (:572)
// Toward the first person fov: over the transition time once set, else at the fov change speed.
void camera_first_person_process_fov(float step_time)
{
    const float target = Cur_fov_scale * First_person_fov;
    if (Rfg_camera.ideal_fov == target)
    {
        First_person_fov_transition_time.invalidate();
        return;
    }
    if (First_person_fov_transition_time.elapsed())
    {
        Rfg_camera.ideal_fov = target;
        return;
    }
    if (First_person_fov_transition_time.valid())
    {
        const float pct = First_person_fov_transition_time.percent();
        Rfg_camera.ideal_fov = (float)((double)First_person_fov_pre_transition + (1.0 - (double)pct) * ((double)target - (double)First_person_fov_pre_transition));
        return;
    }
    bool before;
    if (target <= Rfg_camera.ideal_fov)
    {
        if (Rfg_camera.ideal_fov <= target)
        {
            return;
        }
        Rfg_camera.ideal_fov = (float)((double)Rfg_camera.ideal_fov - (double)Frametime * (double)Camera_first_person_fov_change_speed);
        before = target < Rfg_camera.ideal_fov;
    }
    else
    {
        Rfg_camera.ideal_fov = (float)((double)Frametime * (double)Camera_first_person_fov_change_speed + (double)Rfg_camera.ideal_fov);
        before = Rfg_camera.ideal_fov < target;
    }
    if (!before && Rfg_camera.ideal_fov != target)
    {
        Rfg_camera.ideal_fov = target;
    }
}

// 0x006C41C0 (:608)
// transition_time_ms -1 changes at the fov change speed.
void camera_first_person_set_fov(float new_fov, int transition_time_ms)
{
    First_person_fov = new_fov;
    if (transition_time_ms != -1)
    {
        First_person_fov_pre_transition = Rfg_camera.real_fov;
        First_person_fov_transition_time.set(transition_time_ms);
    }
}

// 0x006C9AC0 (:211)
void camera_start_first_person(bool smooth)
{
    camera_push_mode(CAMERA_FIRST_PERSON_MODE);
    if (smooth)
    {
        camera_trans_start_linear_transition(0.4f, 7, true); // inlined in the binary
    }
    Rfg_camera.lookaround_params.recoiling = 0;
    Rfg_camera.lookaround_params.recoil_start_pitch = 0.0f;
    First_person_fov_transition_time.invalidate();
}

// 0x006C9B60 (:226)
void camera_stop_first_person(bool smooth)
{
    if (!camera_mode_is_on_stack(CAMERA_FIRST_PERSON_MODE)) // inlined in the binary
    {
        return;
    }
    camera_pop_mode(CAMERA_FIRST_PERSON_MODE);
    if (smooth)
    {
        camera_trans_start_linear_transition(0.4f, 7, true); // inlined in the binary
    }
}

// The camera's target if it is the player (a human of sub type 3; inlined in the binary).
static human* camera_first_person_target_player()
{
    if (Rfg_camera.target_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    human* hp = (human*)World.object_from_handle(Rfg_camera.target_handle);
    if (hp == nullptr || hp->get_obj_type() != OT_HUMAN || hp->get_sub_type() != 3)
    {
        return nullptr;
    }
    return hp;
}

// 0x006C9C10 (:241)
// LTCG: speed in ESI. A manned turret caps the turn rate at its rotation speed.
static void cap_rotation_speed(float& speed)
{
    turret* tp = human_combat_get_manned_turret(camera_first_person_target_player());
    if (tp != nullptr)
    {
        const float cap = tp->t_info->h_rot_speed;
        if ((double)speed > (double)cap)
        {
            speed = cap;
        }
        else if ((double)-cap > (double)speed)
        {
            speed = -cap;
        }
    }
}

// The pitch limits (inlined twice in camera_first_person_pre_process): +-MAX_PITCH_ANGLE, 70% of it in cover.
static float camera_first_person_clamp_pitch(float pitch, bool cover)
{
    float lo = -MAX_PITCH_ANGLE;
    float hi = MAX_PITCH_ANGLE;
    if (cover)
    {
        // Magic number: 0.7, the cover pitch range.
        lo = (float)((double)lo * (double)0.7f);
        hi = (float)((double)MAX_PITCH_ANGLE * (double)0.7f);
    }
    float result = (float)((double)pitch - (double)lo) < 0.0f ? lo : pitch;
    if ((float)((double)result - (double)hi) >= 0.0f)
    {
        result = hi;
    }
    return result;
}

// 0x006C9C80 (:259)
// The player's look input for the next frame (the first and third person modes' pre-process). Changes made to the
// angles since the last call (shakes, ladders) move the base angles. The mouse turns by its pixel deltas times the
// mouse multipliers (scaled to a 30 fps frame: 0.033 / Frametime), a stick by the turn rates the controls set
// (camera_lookaround_set_rotation); fine aim, scope, a manned turret and the sprint start lower the sensitivity,
// menus and disabled camera input zero it. Recoil kicks recover toward the pitch before the shot.
void camera_first_person_pre_process()
{
    if (Rfg_camera.target_handle == 0xFFFFFFFF)
    {
        return;
    }
    player* pp = (player*)World.object_from_handle(Rfg_camera.target_handle);
    if (pp == nullptr || pp->get_obj_type() != OT_HUMAN || pp->get_sub_type() != 3)
    {
        return;
    }
    lookaround_mode_params& look = Rfg_camera.lookaround_params;
    look.pitch_base = (float)(((double)look.pitch - (double)look.prev_pitch) + (double)look.pitch_base);
    look.heading_base = (float)(((double)look.heading - (double)look.prev_heading) + (double)look.heading_base);

    // the stick's deflection (raw, on the circle) decides the acceleration ramp
    float stick_h = controls_get_raw_analog(CAA_CAMERA_ROTATE, Player_controller_index);
    float stick_v = -controls_get_raw_analog(CAA_CAMERA_ELEVATE, Player_controller_index);
    controls_map_stick_to_circle(stick_h, stick_v, stick_h, stick_v);
    float deflection = (float)sqrt((double)(float)((double)stick_h * (double)stick_h + (double)stick_v * (double)stick_v));
    stick_h = controls_get_analog(CAA_CAMERA_ROTATE, Player_controller_index);
    stick_v = -controls_get_analog(CAA_CAMERA_ELEVATE, Player_controller_index);
    controls_map_stick_to_circle(stick_h, stick_v, stick_h, stick_v);
    int mouse_dx;
    int mouse_dy;
    int mouse_wheel;
    mouse_get_deltas(mouse_dx, mouse_dy, mouse_wheel);
    float look_h = (float)mouse_dx;
    float look_v = (float)mouse_dy;
    if (control_steam_camera_control())
    {
        look_h = controls_get_raw_analog_uncapped(CAA_CAMERA_ROTATE, Player_controller_index);
        look_v = -controls_get_raw_analog_uncapped(CAA_CAMERA_ELEVATE, Player_controller_index);
    }
    // Magic numbers: the deltas are scaled to a 30 fps frame (0.033 s), Frametime at least 1 ms.
    float frametime = Frametime;
    if ((float)((double)Frametime - 0.001) < 0.0f)
    {
        frametime = 0.001f;
    }
    const float frame_scale = (float)((double)0.033f / (double)frametime);
    look_h = (float)((double)frame_scale * (double)look_h);
    look_v = (float)((double)frame_scale * (double)look_v);
    if (controls_get_axis_action_inverted_state(CAA_CAMERA_ROTATE, Player_controller_index, true))
    {
        look_h = -look_h;
    }
    if (controls_get_axis_action_inverted_state(CAA_CAMERA_ELEVATE, Player_controller_index, true))
    {
        look_v = -look_v;
    }

    // the stick's turn rates: accelerating while the stick is pushed fully, between the base speed and the maximum
    if ((double)deflection > (double)Camera_first_person_ramp_input_min && !Camera_third_person_lock)
    {
        const double ft = (double)Frametime;
        look.rotvel_p = (float)((double)look.rot_p * ft * (double)Camera_first_person_pitch_acceleration + (double)look.rotvel_p);
        double sign = signbit(stick_v) ? -1.0 : 1.0; // the sign bit (-0 counts as negative)
        if ((double)look.rot_p * sign * (double)Camera_first_person_pitch_speed > (double)look.rotvel_p * sign)
        {
            look.rotvel_p = (float)((double)look.rot_p * (double)Camera_first_person_pitch_speed);
        }
        else if ((double)look.rotvel_p * sign > sign * (double)stick_v * (double)Camera_first_person_pitch_speed_max)
        {
            look.rotvel_p = (float)((double)Camera_first_person_pitch_speed_max * (double)stick_v);
        }
        look.rotvel_h = (float)((double)look.rot_h * ft * (double)Camera_first_person_heading_acceleration + (double)look.rotvel_h);
        sign = signbit(stick_h) ? -1.0 : 1.0;
        if ((double)look.rot_h * sign * (double)Camera_first_person_heading_speed > (double)look.rotvel_h * sign)
        {
            look.rotvel_h = (float)((double)look.rot_h * (double)Camera_first_person_heading_speed);
        }
        else if ((double)look.rotvel_h * sign > sign * (double)stick_h * (double)Camera_first_person_heading_speed_max)
        {
            look.rotvel_h = (float)((double)Camera_first_person_heading_speed_max * (double)stick_h);
        }
    }
    else
    {
        look.rotvel_p = (float)((double)look.rot_p * (double)Camera_first_person_pitch_speed);
        look.rotvel_h = (float)((double)look.rot_h * (double)Camera_first_person_heading_speed);
    }

    // the sensitivity (the mouse and a Steam controller count double for the turret and the scope)
    const float device_scale = control_mouse_camera_control(CAA_CAMERA_ROTATE, CAA_CAMERA_ELEVATE) || control_steam_camera_control() ? 2.0f : 1.0f;
    float sensitivity = 1.0f;
    if (gameseq_get_state() == GS_INGAME_OPTIONS || gameseq_get_state() == GS_MAINMENU_OPTIONS_DISPLAY || gameseq_get_state() == GS_MAINMENU_OPTIONS_AUDIO ||
        gameseq_get_state() == GS_MAINMENU_OPTIONS_LIVE || gameseq_get_state() == GS_MAINMENU_OPTIONS_LIVE_QUICKMATCH ||
        gameseq_get_state() == GS_MAINMENU_OPTIONS_GAME_OPTIONS || gameseq_get_state() == GS_MAINMENU_OPTIONS_EXTRAS ||
        gameseq_get_state() == GS_CONTROLLER_OPTIONS || !control_player_camera_input_enabled() ||
        (pp->backpack != nullptr && pp->backpack->get_backpack_abilities()->charge) || (Multiplayer && Multi_match_info.match_state != MGMS_GAMEPLAY) ||
        (wrecking_crew_is_active() && !wrecking_crew_can_player_move_camera()))
    {
        sensitivity = 0.0f;
    }
    else if (human_combat_is_manning_turret(pp))
    {
        sensitivity = (float)((double)Camera_turret_multiplier * (double)device_scale);
    }
    else if (player_is_fine_aimed(pp))
    {
        sensitivity = Camera_first_person_fineaim_multiplier;
    }
    else if (player_is_scoped(pp))
    {
        // Magic number: 0.75 of the scope multiplier.
        sensitivity = (float)((double)device_scale * 0.75 * (double)Camera_first_person_scope_multiplier);
    }
    else if (!pp->sprint_delay_timestamp.elapsed())
    {
        // Magic number: the sprint start's 300 ms blend back to full sensitivity.
        const float t = (float)((double)(float)pp->sprint_delay_timestamp.time_until() / 300.0);
        sensitivity = (float)((double)Camera_first_person_sprint_multiplier * (double)device_scale * (double)t + (1.0 - (double)t));
    }

    // pitch: the added velocity decays (e^(-10 t), x87), the turn is the mouse's or the stick's
    // Magic number: 2.7182817, e as a float.
    look.rotvel_added_p = (float)pow(2.7182817459106445, (double)(float)((double)Frametime * -10.0)) * look.rotvel_added_p;
    cap_rotation_speed(look.rotvel_p); // inlined in the binary
    const bool mouse = control_mouse_camera_control(CAA_CAMERA_ROTATE, CAA_CAMERA_ELEVATE) || control_steam_camera_control();
    const double pitch_rate = mouse ? (double)Mouse_pixel_multiplier * (double)Mouse_sensitivity * (double)look_v : (double)look.rotvel_p;
    const float pitch_delta = (float)(pitch_rate * (double)sensitivity * (double)Camera_first_person_look_sensitivity_pitch * (double)Frametime);
    look.pitch_base = (float)(((double)look.rotvel_added_p * (double)Frametime + (double)pitch_delta) * (double)look.rot_p_multiplier + (double)look.pitch_base);
    const bool cover = pp->cur_state == HS_COVER;
    look.pitch_base = camera_first_person_clamp_pitch(look.pitch_base, cover);
    if (look.recoiling)
    {
        // Turning against the recoil moves the pitch it recovers to (by pitch_move_scale), not past the current one.
        if (pitch_delta < 0.0f || (pitch_delta > 0.0f && (double)look.pitch_base > (double)look.recoil_start_pitch))
        {
            look.recoil_start_pitch = (float)((double)pitch_move_scale * (double)pitch_delta + (double)look.recoil_start_pitch);
            if ((pitch_delta > 0.0f && (double)look.recoil_start_pitch > (double)look.pitch_base) ||
                (pitch_delta < 0.0f && (double)look.pitch_base > (double)look.recoil_start_pitch))
            {
                look.recoil_start_pitch = look.pitch_base;
            }
        }
        weapon* wp = human_weapons_get_weapon(pp);
        if (wp != nullptr && wp->time_since_fired() != 0)
        {
            float correction = (float)(((double)look.recoil_start_pitch - (double)look.pitch_base) * (double)recoil_auto_correct_rate);
            const float min_speed = -recoil_max_speed;
            if ((double)min_speed > (double)correction)
            {
                correction = min_speed;
            }
            else if ((double)correction > (double)recoil_max_speed)
            {
                correction = recoil_max_speed;
            }
            look.pitch_base = (float)((double)Frametime * (double)correction + (double)look.pitch_base);
            // Magic number: 0.001 rad, recovered.
            if ((float)fabs((double)look.recoil_start_pitch - (double)look.pitch_base) < 0.001f)
            {
                look.recoiling = 0;
                look.pitch_base = look.recoil_start_pitch;
            }
        }
    }
    look.pitch = look.pitch_base;
    if (input_get_gyro_enabled(GYRO_MODE_THIRD_PERSON))
    {
        // x87: the gyro value times 2.5 plus the pitch, rounded once.
        look.pitch = (float)((double)input_gyro_get_camera_orientation_value(GYRO_MODE_THIRD_PERSON, GYRO_CAMERA_AXIS_Y, Player_controller_index) * 2.5 + (double)look.pitch);
    }
    look.pitch = camera_first_person_clamp_pitch(look.pitch, cover);

    // heading
    look.rotvel_added_h = (float)pow(2.7182817459106445, (double)(float)((double)Frametime * -10.0)) * look.rotvel_added_h;
    cap_rotation_speed(look.rotvel_h); // inlined in the binary
    const double heading_rate = control_mouse_camera_control(CAA_CAMERA_ROTATE, CAA_CAMERA_ELEVATE) || control_steam_camera_control()
                                    ? (double)Mouse_pixel_multiplier * (double)Mouse_sensitivity * (double)look_h
                                    : (double)look.rotvel_h;
    const float heading_delta = (float)(heading_rate * (double)sensitivity * (double)Camera_first_person_look_sensitivity_heading * (double)Frametime);
    look.heading_base = (float)(((double)look.rotvel_added_h * (double)Frametime + (double)heading_delta) * (double)look.rot_h_multiplier + (double)look.heading_base);
    look.heading = look.heading_base;
    if (input_get_gyro_enabled(GYRO_MODE_THIRD_PERSON))
    {
        look.heading = (float)((double)input_gyro_get_camera_orientation_value(GYRO_MODE_THIRD_PERSON, GYRO_CAMERA_AXIS_X, Player_controller_index) * 5.0 + (double)look.heading);
    }
    look.heading_base = fl_angle_cap_radians(look.heading_base, 0.0f);
    look.heading = fl_angle_cap_radians(look.heading, 0.0f);
    Rfg_camera.ideal_orient.set(look.pitch, 0.0f, look.heading);
    look.prev_heading = look.heading;
    look.rot_p_multiplier = 1.0f;
    look.rot_h_multiplier = 1.0f;
    look.prev_pitch = look.pitch;
}

// 0x006CAA00 (:529)
// At the player's eyes: the camera bone's transform when the human type has one, else 1.65 m above its position
// with the look angles (set by the pre-process).
void camera_first_person_process(float step_time, bool last_step_of_frame)
{
    player* pp = player_get_local_player();
    if (pp->h_info->camera_bone_index == -1)
    {
        const vector offset = Camera_first_person_offset.copy_unrotate(pp->orient);
        Rfg_camera.ideal_pos.x = offset.x + pp->pos.x;
        Rfg_camera.ideal_pos.y = pp->pos.y + offset.y;
        Rfg_camera.ideal_pos.z = pp->pos.z + offset.z;
    }
    else
    {
        vector pos = pp->camera_bone_transform.m_translation;
        pos.unrotate(pp->orient);
        pos.y = pp->pos.y + pos.y;
        pos.x = pp->pos.x + pos.x;
        pos.z = pp->pos.z + pos.z;
        pp->camera_bone_transform.m_rotation.fvec.mag_and_normalize_safe();
        float pitch;
        float heading;
        pp->camera_bone_transform.m_rotation.extract_pitch(pitch);
        pp->camera_bone_transform.m_rotation.extract_heading(heading);
        Rfg_camera.ideal_pos = pos;
        Rfg_camera.ideal_orient.rotate_around_local_x(pitch);
        Rfg_camera.ideal_orient.rotate_around_local_y(heading);
    }
    camera_first_person_process_fov(step_time);
}
