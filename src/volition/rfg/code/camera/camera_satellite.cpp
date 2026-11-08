// Original: volition/rfg/code/camera/camera_satellite.cpp
//
// The satellite camera mode: the overhead view of the satellite targeting, started by mission scripts. The player's body
// hangs behind the camera, which follows a target the script names, or the player's input within a range of it. Firing
// drops an artillery round from a source position the script gives. Stopping waits for the zone at the original
// position, or warps back with a save when no mission is running.

#include "volition/rfg/code/camera/camera_satellite.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_att_loop_rel.h"
#include "volition/interface_lib/code/vint_project.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/rfg/code/ai/percept.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/scripting/objective_highlight.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/rfg/code/gamewide/tweak_table.h"

#include <math.h>

extern float VEHICLE_MAX_SPEED; // 0x016BA434 (automobile.cpp)

// ---- tuning: file statics with these values in rfg.exe's data, then the tweak table
static float Satellite_fov_deg = 40.0f; // 0x01648878
static tweak_table_item Tweak_table_item_Satellite_fov_deg("Satellite_fov_deg", &Satellite_fov_deg); // 0x01DE6B4C
static float Satellite_cam_input_vel_mag = 35.0f; // 0x0164887C
static tweak_table_item Tweak_table_item_Satellite_cam_input_vel_mag("Satellite_cam_input_vel_mag", &Satellite_cam_input_vel_mag); // 0x01DE6AD0
static float Satellite_cam_max_acc_mag = 7.0f; // 0x01648884
static tweak_table_item Tweak_table_item_Satellite_cam_max_acc_mag("Satellite_cam_max_acc_mag", &Satellite_cam_max_acc_mag); // 0x01DE6AA0
static float Satellite_cam_max_acc_mag_seek = 7.0f; // 0x01648888
static tweak_table_item Tweak_table_item_Satellite_cam_max_acc_mag_seek("Satellite_cam_max_acc_mag_seek", &Satellite_cam_max_acc_mag_seek); // 0x01DE48F8
static float Satellite_cam_max_heading_acc = 0.5f; // 0x0164888C
static tweak_table_item Tweak_table_item_Satellite_cam_max_heading_acc("Satellite_cam_max_heading_acc", &Satellite_cam_max_heading_acc); // 0x01DE4904
static float Satellite_cam_height = 60.0f; // 0x01648890
static tweak_table_item Tweak_table_item_Satellite_cam_height("Satellite_cam_height", &Satellite_cam_height); // 0x01DE6ADC
static int Satellite_weapon_refire_delay = 1000; // 0x01648898, ms
static tweak_table_item Tweak_table_item_Satellite_weapon_refire_delay("Satellite_weapon_refire_delay", &Satellite_weapon_refire_delay); // 0x01DE6AAC
static float Satellite_weapon_angle = 70.0f; // 0x0164889C, degrees
static tweak_table_item Tweak_table_item_Satellite_weapon_angle("Satellite_weapon_angle", &Satellite_weapon_angle); // 0x01DE4934
static float Satellite_weapon_launch_dist = 50.0f; // 0x016488A0
static tweak_table_item Tweak_table_item_Satellite_weapon_launch_dist("Satellite_weapon_launch_dist", &Satellite_weapon_launch_dist); // 0x01DE6B68
// Registered in rfg.exe, read by nothing there.
static float Satellite_cam_input_zoom_vel_mag = 10.0f; // 0x01648880
static tweak_table_item Tweak_table_item_Satellite_cam_input_zoom_vel_mag("Satellite_cam_input_zoom_vel_mag", &Satellite_cam_input_zoom_vel_mag); // 0x01DE6AE8
static float Satellite_cam_zoom_height = 30.0f; // 0x01648894
static tweak_table_item Tweak_table_item_Satellite_cam_zoom_height("Satellite_cam_zoom_height", &Satellite_cam_zoom_height); // 0x01DE4A6C
static float Satellite_cam_aim_dist = 200.0f; // 0x016488A8
static float Satellite_cam_input_deadzone_mag_sq = 1.0f; // 0x016488AC
static float Satellite_cam_stop_dist_to_target_sq = 1.0f; // 0x016488B0
static float Satellite_cam_zoom_speed = 75.0f; // 0x016488B4
static float Satellite_cam_angle_to_horizon_rad = 1.3962635f; // 0x01648988, 80 degrees
static float Satellite_cam_max_ang_vel_mag = 0.7853982f; // 0x0164898C, 45 degrees a second
static float Satellite_cam_max_vel_mag = VEHICLE_MAX_SPEED; // 0x01DE6B58, at init; VEHICLE_MAX_SPEED is constant-initialized

// Magic number: the range a script's limit cannot pass, and what each start resets it to: 100 * sqrt(2).
static const float SATELLITE_CAM_DEFAULT_RANGE = 141.42136f;

// ---- state
static float Satellite_cam_max_range = SATELLITE_CAM_DEFAULT_RANGE; // 0x016488A4
static unsigned int Satellite_weapon_info_handle = 0xFFFFFFFF; // 0x016488B8
static unsigned int Satellite_target_handle = 0xFFFFFFFF; // 0x016488BC
int Satellite_target_sound_handle = -1; // 0x016488C0
static control_axis_action Satellite_last_v_action = (control_axis_action)1; // 0x016488C4
static timestamp Satellite_weapon_timer; // 0x01648990, -1 in rfg.exe's data, as timestamp's constructor leaves it
static int Satellite_scrolling_alr_id = -1; // 0x01648D0C
static unsigned char Satellite_exit_hour; // 0x01DE48A8
static bool Satellite_exit; // 0x01DE48C0
static bool Satellite_camera_is_active; // 0x01DE48C1
static bool Start_stop_sound_pair_active; // 0x01DE48C2
static bool Satellite_shutting_down; // 0x01DE48C3
static control_axis_action Satellite_last_h_action; // 0x01DE48C4
static vector Satellite_cam_previous_vel; // 0x01DE4910
static vector Satellite_original_player_pos; // 0x01DE491C
static vector Satellite_weapon_source_pos; // 0x01DE4940
static vector2 Satellite_input_vel_cam_rel; // 0x01DE4958
static unsigned char Satellite_exit_minute; // 0x01DE4510
static bool Satellite_return_to_target_user_initiated; // 0x01DE4511
static bool Satellite_return_to_target; // 0x01DE4512
static bool Satellite_hold_position; // 0x01DE4513
static float Satellite_cam_previous_ang_vel; // 0x01DE4518
static unsigned int Satellite_previous_target_handle; // 0x01DE453C
static int cue_id_artillery_fire; // 0x01DE4540
static float Satellite_previous_height; // 0x01DE454C
static matrix Satellite_original_player_orient; // 0x01DE4A78
static vector Camera_satellite_prev_gyro; // 0x01DE6A88
static vector2 Satellite_input_vel_gyro; // 0x01DE6AB8
static vector Satellite_target_previous_pos; // 0x01DE6B00
static vector Satellite_last_hit_pos; // 0x01DE6B5C
static bool old_reticule; // get_satellite_target's previous answer

// 0x008987C0
camera_satellite_hud_data::camera_satellite_hud_data()
{
}

// 0x006C4200 (global_event_data.h:273)
satellite_reaches_target_event::satellite_reaches_target_event() : event_data(EVENT_SATELLITE_REACHES_TARGET)
{
}

// 0x006C66F0 (:167)
float camera_satellite_get_gyro_pitch()
{
    return Satellite_input_vel_gyro.y;
}

// 0x006C6700 (:172)
float camera_satellite_get_gyro_heading()
{
    return Satellite_input_vel_gyro.x;
}

// 0x006D4120 (:178)
void unused_dcf_satellite_test()
{
    if (Rfg_camera.mode != CAMERA_SATELLITE_MODE)
    {
        camera_start_satellite_mode();
    }
    const player* const pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    // Magic number: a test source 100 m along x from the player.
    Satellite_weapon_source_pos.z = pp->pos.z + 0.0f;
    Satellite_weapon_source_pos.y = pp->pos.y + 0.0f;
    Satellite_weapon_source_pos.x = pp->pos.x + 100.0f;
    vehicle* last_vehicle = nullptr;
    if (Satellite_target_handle != 0xFFFFFFFF)
    {
        object* const objp = World.object_from_handle(Satellite_target_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            last_vehicle = (vehicle*)objp;
        }
    }
    const unsigned int target_handle = vehicle_get_closest_vehicle(Rfg_camera.ideal_pos, -1.0f, last_vehicle, false);
    if (target_handle != 0xFFFFFFFF)
    {
        camera_satellite_set_target(target_handle);
    }
}

// 0x006D4200 (:194)
void unused_dcf_satellite_test_stop()
{
    if (Rfg_camera.mode == CAMERA_SATELLITE_MODE)
    {
        Satellite_exit = true;
        Satellite_shutting_down = true;
    }
}

// 0x006C6710 (:206)
// Looks down at the fixed angle, along the heading.
static void camera_satellite_update_ideal_orient(float heading)
{
    const float sin_heading = (float)sin(heading);
    const float cos_heading = (float)cos(heading);
    const float sin_angle = (float)sin(Satellite_cam_angle_to_horizon_rad);
    const float cos_angle = (float)cos(Satellite_cam_angle_to_horizon_rad);
    const vector fvec(cos_angle * sin_heading - UP_VECTOR.x * sin_angle, cos_angle * 0.0f - UP_VECTOR.y * sin_angle, cos_angle * cos_heading - UP_VECTOR.z * sin_angle);
    const vector uvec(cos_angle * UP_VECTOR.x + sin_angle * sin_heading, cos_angle * UP_VECTOR.y + sin_angle * 0.0f, cos_angle * UP_VECTOR.z + sin_angle * cos_heading);
    Rfg_camera.ideal_orient.make_uvec(fvec, uvec);
}

// 0x006CCB40 (:218)
static void camera_stop_satellite_mode_internal()
{
    if (Rfg_camera.mode == CAMERA_SATELLITE_MODE && !mission_is_active())
    {
        Satellite_exit = false;
        Satellite_camera_is_active = false;
        ai_ignore_player(false);
        activity_reinstate_diversions(ADR_SATELLITE);
        World.save_and_warp(Satellite_original_player_pos, Satellite_original_player_orient);
    }
}

// 0x006DA650 (:233)
// LTCG: y in ESI, target_op in EAX. The ground, or at most the target, near the height asked about.
static void camera_satellite_get_base_height(float& y, const float x, const float z, const object* const target_op)
{
    if (target_op != nullptr)
    {
        y = target_op->pos.y;
    }
    else
    {
        y = Rfg_camera.ideal_pos.y - Satellite_cam_height;
    }
    raycast_input input;
    input.radius = 0.0f;
    input.filter_params = nullptr;
    input.filter_callback = nullptr;
    input.m_filter_info = COLLIDE_RAY_DEFAULT;
    input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    input.m_flags = 0;
    // Magic number: 20 m each way.
    input.start.set(x, (float)((double)y + 20.0), z);
    input.end.set(x, (float)((double)y - 20.0), z);
    raycast_output output;
    if (havok_raycast_against_static_level_only(input, output))
    {
        const float hit_y = output.m_hits[0].hit_pos.y;
        if (hit_y - y < 0.0f)
        {
            y = hit_y;
        }
    }
}

// 0x006C6920 (:260)
bool allowed_to_fire()
{
    return Satellite_weapon_timer.elapsed() && (!Satellite_return_to_target || Satellite_return_to_target_user_initiated);
}

// 0x006C6950 (:270)
void start_return_to_target_sound()
{
    if (!Start_stop_sound_pair_active)
    {
        audiolib_cue_play(Ga_cue_ids.avmg_zoom_to_convoy, nullptr, nullptr);
        Start_stop_sound_pair_active = true;
    }
}

// 0x006C6980 (:280)
void stop_return_to_target_sound()
{
    if (Start_stop_sound_pair_active)
    {
        audiolib_cue_play(Ga_cue_ids.avmg_stop_zoom_to_convoy, nullptr, nullptr);
        Start_stop_sound_pair_active = false;
    }
}

// 0x006C69B0 (:293)
void camera_satellite_mode_init()
{
    Rfg_camera.ideal_fov = Satellite_fov_deg;
}

// 0x006CCB90 (:298)
void camera_start_satellite_mode()
{
    if (gameseq_get_state() != GS_GAMEPLAY || Rfg_camera.mode == CAMERA_SATELLITE_MODE)
    {
        return;
    }
    camera_push_mode(CAMERA_SATELLITE_MODE);
    Satellite_exit_hour = Gamewide_game_clock->m_hour;
    Satellite_exit_minute = Gamewide_game_clock->m_minutes;
    // Magic number: always 10:00 through the satellite.
    game_clock_set_time(10, 0, 0);
    game_clock_freeze_ingame_time(true);
    player* const pp = player_get_local_player();
    if (pp == nullptr)
    {
        // Original quirk kept: the mode stays pushed and the clock frozen.
        return;
    }
    human_set_cast_shadows(pp, false, true);
    human_set_cast_drop_shadows(pp, false, true);
    pp->health_restore_timestamp.set(0);
    ai_ignore_player(true);
    human_update_collision_filter(pp, false);
    pp->refill_health(1.0f);
    pp->knockdown_hits = (float)pp->max_knockdown_hits;
    if (pp->ragdoll_state == HUMAN_RAGDOLL_STATE_NONE)
    {
        pp->hflags.dont_deform_bones = 0;
    }
    human_set_state(pp, HS_SATELLITE, HMS_IDLE, -1.0f);
    human_set_movement_mode(pp, HMM_SATELLITE, HMSM_NONE);
    // Magic number: no firing for the first 10 s.
    Satellite_weapon_timer.set(10000);
    Satellite_hold_position = false;
    Satellite_input_vel_cam_rel.zero();
    Satellite_input_vel_gyro.zero();
    Satellite_cam_previous_vel.zero();
    Satellite_target_previous_pos = pp->pos;
    Satellite_previous_target_handle = pp->get_handle();
    Satellite_last_hit_pos = ZERO_VECTOR;
    Satellite_original_player_pos = pp->pos;
    Satellite_original_player_orient = pp->orient;
    Satellite_cam_previous_ang_vel = 0.0f;
    Satellite_target_handle = 0xFFFFFFFF;
    Satellite_exit = false;
    Satellite_shutting_down = false;
    Start_stop_sound_pair_active = false;
    Satellite_camera_is_active = true;
    Satellite_previous_height = 0.0f;
    hud_ui_set_loaded_module(HUD_MODULE_SATELLITE);
    Satellite_cam_max_range = SATELLITE_CAM_DEFAULT_RANGE;
    activity_suspend_diversions(ADR_SATELLITE);
    Camera_satellite_prev_gyro.zero();
}

// 0x006CCDB0 (:392)
void camera_stop_satellite_mode()
{
    if (Rfg_camera.mode == CAMERA_SATELLITE_MODE)
    {
        Satellite_exit = true;
        Satellite_shutting_down = true;
    }
}

// 0x006C69D0 (:402)
unsigned int camera_satellite_get_target()
{
    return Satellite_target_handle;
}

// 0x006C69E0 (:407)
void camera_satellite_set_target(unsigned int target_handle)
{
    Satellite_target_handle = target_handle;
    Satellite_return_to_target = true;
    if (Satellite_weapon_timer.elapsed())
    {
        // rfg.exe inlines this call.
        start_return_to_target_sound();
    }
    Satellite_return_to_target_user_initiated = false;
    Satellite_input_vel_cam_rel.zero();
    Satellite_input_vel_gyro.zero();
}

// 0x006C6A40 (:421)
void camera_satellite_set_max_range(float max_range)
{
    Satellite_cam_max_range = SATELLITE_CAM_DEFAULT_RANGE;
    if (0.0f < max_range && max_range < SATELLITE_CAM_DEFAULT_RANGE)
    {
        Satellite_cam_max_range = max_range;
    }
}

// 0x006C6A70 (:429)
void camera_satellite_set_weapon_source_pos(const vector& weapon_source_pos)
{
    Satellite_weapon_source_pos = weapon_source_pos;
}

// 0x006D5D00 (:434)
void camera_satellite_fire_weapon()
{
    if (Rfg_camera.mode != CAMERA_SATELLITE_MODE || !allowed_to_fire())
    {
        return;
    }
    const weapon_info* const w_info = weapons_get_weapon_info(Satellite_weapon_info_handle);
    if (w_info == nullptr)
    {
        return;
    }
    audiolib_params sound;
    sound.distance_min = 1.0f;
    sound.distance_max = 1000.0f;
    sound.volume = 1.0f;
    audiolib_cue_play(cue_id_artillery_fire, &sound, nullptr);

    raycast_input input;
    input.radius = 0.0f;
    input.start = Rfg_camera.real_pos;
    input.end.set(Rfg_camera.real_pos.x + Rfg_camera.real_orient.fvec.x * Satellite_cam_aim_dist, Rfg_camera.real_pos.y + Rfg_camera.real_orient.fvec.y * Satellite_cam_aim_dist, Rfg_camera.real_pos.z + Rfg_camera.real_orient.fvec.z * Satellite_cam_aim_dist);
    input.m_filter_info = COLLIDE_RAY_DEFAULT;
    input.filter_params = nullptr;
    input.filter_callback = nullptr;
    input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    input.m_flags = 0;
    raycast_output output;
    vector aim_pos = input.end;
    if (havok_raycast(input, output))
    {
        Satellite_last_hit_pos = output.m_hits[0].hit_pos;
        aim_pos = Satellite_last_hit_pos;
    }

    // The round comes in along the heading from the source, tilted down by the weapon angle.
    vector flat_dir(aim_pos.x - Satellite_weapon_source_pos.x, aim_pos.y - Satellite_weapon_source_pos.y, aim_pos.z - Satellite_weapon_source_pos.z);
    flat_dir.flatten_normalize_safe();
    // Magic number: pi, in single precision as rfg.exe's constant is.
    const float angle = (Satellite_weapon_angle * 3.1415927f) / 180.0f;
    const float sin_angle = (float)sin(angle);
    const float cos_angle = (float)cos(angle);
    const vector launch_dir(cos_angle * flat_dir.x - sin_angle * UP_VECTOR.x, flat_dir.y * cos_angle - sin_angle * UP_VECTOR.y, flat_dir.z * cos_angle - sin_angle * UP_VECTOR.z);
    const vector launch_pos(aim_pos.x - launch_dir.x * Satellite_weapon_launch_dist, aim_pos.y - launch_dir.y * Satellite_weapon_launch_dist, aim_pos.z - launch_dir.z * Satellite_weapon_launch_dist);
    matrix launch_orient;
    launch_orient.make(launch_dir);
    const float speed = w_info->projectile_info.start_speed;
    const vector launch_vel(launch_dir.x * speed, launch_dir.y * speed, launch_dir.z * speed);

    obj_item_flags flags;
    flags.simulate = 1;
    flags.cast_shadows = 1;
    flags.cast_drop_shadows = 1;
    obj_projectile_props props(0xFFFFFFFF, w_info->projectile_info.obj_item_info, flags, w_info, launch_pos, launch_orient, launch_vel, ZERO_VECTOR, 0xFFFFFFFF);
    World.create_object<projectile>(props, 0xFFFFFFFF);
    Satellite_weapon_timer.set(Satellite_weapon_refire_delay);
}

// 0x006C6A90 (:493)
void camera_satellite_control(float vert_input, float horz_input, control_axis_action h_action, control_axis_action v_action)
{
    Satellite_last_h_action = h_action;
    Satellite_last_v_action = v_action;
    if (Satellite_return_to_target)
    {
        if (!Satellite_return_to_target_user_initiated)
        {
            Satellite_input_vel_cam_rel.zero();
            Satellite_input_vel_gyro.zero();
            if (input_get_gyro_enabled(GYRO_MODE_SATELLITE))
            {
                input_reset_gyro_orientation_origin();
            }
            return;
        }
        if (input_get_gyro_enabled(GYRO_MODE_SATELLITE))
        {
            input_reset_gyro_orientation_origin();
        }
    }
    Satellite_input_vel_cam_rel.x = Satellite_cam_input_vel_mag * horz_input;
    Satellite_input_vel_cam_rel.y = Satellite_cam_input_vel_mag * vert_input;
    // Magic numbers: the gyro's scales on each axis.
    Satellite_input_vel_gyro.x = input_gyro_get_camera_orientation_value(GYRO_MODE_SATELLITE, GYRO_CAMERA_AXIS_X, Player_controller_index) * 1.25f * Satellite_cam_input_vel_mag;
    Satellite_input_vel_gyro.y = input_gyro_get_camera_orientation_value(GYRO_MODE_SATELLITE, GYRO_CAMERA_AXIS_Y, Player_controller_index) * 2.5f * Satellite_cam_input_vel_mag;
}

// A value's magnitude, at most limit, with the value's sign. rfg.exe writes this out for each limit.
static float satellite_limit_magnitude(const float value, const float limit)
{
    // rfg.exe tests the sign bit.
    const float sign = copysignf(1.0f, value);
    const float magnitude = fabsf(value);
    return (0.0f <= magnitude - limit ? limit : magnitude) * sign;
}

static float satellite_input_mag_sq()
{
    return Satellite_input_vel_cam_rel.x * Satellite_input_vel_cam_rel.x + Satellite_input_vel_cam_rel.y * Satellite_input_vel_cam_rel.y;
}

static float satellite_gyro_mag_sq()
{
    return Satellite_input_vel_gyro.x * Satellite_input_vel_gyro.x + Satellite_input_vel_gyro.y * Satellite_input_vel_gyro.y;
}

// 0x006DA7A0 (:526)
void camera_satellite_process(float step_time, bool last_step_of_frame)
{
    player* const pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }

    if (Satellite_exit)
    {
        if (!mission_is_active())
        {
            camera_stop_satellite_mode_internal();
            game_clock_set_time(Satellite_exit_hour, Satellite_exit_minute, 0);
            game_clock_freeze_ingame_time(false);
            objective_highlight_remove_all();
            return;
        }
        if (obj_zone_streamed(Satellite_original_player_pos))
        {
            pp->update_pos_and_orient(Satellite_original_player_pos, Satellite_original_player_orient, true);
            camera_pop_mode(CAMERA_SATELLITE_MODE);
            hud_ui_set_loaded_module(HUD_MODULE_NONE);
            game_clock_set_time(Satellite_exit_hour, Satellite_exit_minute, 0);
            game_clock_freeze_ingame_time(false);
            objective_highlight_remove_all();
            ai_ignore_player(false);
            human_update_collision_filter(pp, true);
            human_set_state(pp, HS_STANDARD, HMS_IDLE, -1.0f);
            human_set_movement_mode(pp, HMM_NONE, HMSM_NONE);
            human_set_cast_shadows(pp, true, true);
            human_set_cast_drop_shadows(pp, true, true);
            activity_reinstate_diversions(ADR_SATELLITE);
            return;
        }
        // Fly back towards the original position until its zone streams in.
        vector to_original(Satellite_original_player_pos.x - Rfg_camera.ideal_pos.x, Satellite_original_player_pos.y - Rfg_camera.ideal_pos.y, Satellite_original_player_pos.z - Rfg_camera.ideal_pos.z);
        float dist = to_original.flatten_mag_and_normalize_safe();
        if (0.0f <= dist - Satellite_cam_max_vel_mag * step_time)
        {
            dist = Satellite_cam_max_vel_mag * step_time;
        }
        Rfg_camera.ideal_pos.x = to_original.x * dist + Rfg_camera.ideal_pos.x;
        Rfg_camera.ideal_pos.y = to_original.y * dist + Rfg_camera.ideal_pos.y;
        Rfg_camera.ideal_pos.z = to_original.z * dist + Rfg_camera.ideal_pos.z;
        // Magic number: the player's body hangs 2 m behind the camera.
        const vector body_pos(Rfg_camera.ideal_pos.x - Rfg_camera.ideal_orient.fvec.x * 2.0f, Rfg_camera.ideal_pos.y - Rfg_camera.ideal_orient.fvec.y * 2.0f, Rfg_camera.ideal_pos.z - Rfg_camera.ideal_orient.fvec.z * 2.0f);
        pp->update_pos_and_orient(body_pos, Rfg_camera.ideal_orient, true);
        return;
    }

    hud_ui_set_loaded_module(HUD_MODULE_SATELLITE);
    const object* const target = World.object_from_handle(Satellite_target_handle);

    // Zoom towards the height.
    const float height = satellite_limit_magnitude(Satellite_cam_height - Satellite_previous_height, Satellite_cam_zoom_speed * step_time) + Satellite_previous_height;

    // Turn towards the target's heading.
    const float cur_heading = fl_atan2_safe(Rfg_camera.real_orient.fvec.x, Rfg_camera.real_orient.fvec.z);
    const float desired_heading = target != nullptr ? fl_atan2_safe(target->orient.fvec.x, target->orient.fvec.z) : cur_heading;
    const float heading_delta = fl_angle_between_radians(cur_heading, desired_heading);
    float ang_vel = 0.0f;
    vector flat_fvec;
    flat_fvec = Rfg_camera.ideal_orient.fvec.copy_flatten_normalize_safe();
    // How far back from the looked-at point the camera sits, before the turn and the zoom.
    const float old_back = -(Satellite_previous_height / (float)tan(Satellite_cam_angle_to_horizon_rad));
    const vector old_offset(old_back * flat_fvec.x, flat_fvec.y * old_back, old_back * flat_fvec.z);
    if (heading_delta != 0.0f)
    {
        const float delta_to_use = Satellite_hold_position ? 0.0f : heading_delta;
        const float ang_vel_wanted = satellite_limit_magnitude(delta_to_use / step_time - Satellite_cam_previous_ang_vel, Satellite_cam_max_heading_acc) + Satellite_cam_previous_ang_vel;
        ang_vel = satellite_limit_magnitude(ang_vel_wanted, Satellite_cam_max_ang_vel_mag);
        float pct = fabsf((ang_vel * step_time) / heading_delta);
        if (0.0f <= pct - 1.0f)
        {
            pct = 1.0f;
        }
        camera_satellite_update_ideal_orient(fl_angle_lerp_radians(cur_heading, desired_heading, pct));
    }
    flat_fvec = Rfg_camera.ideal_orient.fvec.copy_flatten_normalize_safe();
    const float new_back = -(height / (float)tan(Satellite_cam_angle_to_horizon_rad));
    const vector new_offset(new_back * flat_fvec.x, flat_fvec.y * new_back, flat_fvec.z * new_back);
    Satellite_previous_height = height;

    // The move this step: by input, or following the target. Snapping skips the acceleration limit below.
    bool snap = false;
    vector gyro_offset;
    gyro_offset.zero();
    vector move;
    bool check_range = true;
    if (Satellite_hold_position || target == nullptr || Satellite_cam_input_deadzone_mag_sq <= satellite_input_mag_sq() || (0.0f < satellite_gyro_mag_sq() && !Satellite_return_to_target))
    {
        vector flat_rvec;
        vector flat_uvec;
        flat_rvec = Rfg_camera.ideal_orient.rvec.copy_flatten_normalize_safe();
        flat_uvec = Rfg_camera.ideal_orient.uvec.copy_flatten_normalize_safe();
        move.set((flat_rvec.x * Satellite_input_vel_cam_rel.x + flat_uvec.x * Satellite_input_vel_cam_rel.y) * step_time, (flat_rvec.y * Satellite_input_vel_cam_rel.x + flat_uvec.y * Satellite_input_vel_cam_rel.y) * step_time, (flat_rvec.z * Satellite_input_vel_cam_rel.x + flat_uvec.z * Satellite_input_vel_cam_rel.y) * step_time);
        if (input_get_gyro_enabled(GYRO_MODE_SATELLITE))
        {
            gyro_offset.set(flat_uvec.x * Satellite_input_vel_gyro.y + flat_rvec.x * Satellite_input_vel_gyro.x, flat_uvec.y * Satellite_input_vel_gyro.y + flat_rvec.y * Satellite_input_vel_gyro.x, flat_uvec.z * Satellite_input_vel_gyro.y + flat_rvec.z * Satellite_input_vel_gyro.x);
        }
        if (Satellite_return_to_target)
        {
            if (satellite_input_mag_sq() < Satellite_cam_input_deadzone_mag_sq)
            {
                check_range = false;
            }
            else
            {
                stop_return_to_target_sound();
                Satellite_return_to_target = false;
            }
        }
    }
    else
    {
        if (Satellite_previous_target_handle == 0xFFFFFFFF || Satellite_previous_target_handle != Satellite_target_handle)
        {
            move.zero();
        }
        else
        {
            const vector target_moved(target->pos.x - Satellite_target_previous_pos.x, target->pos.y - Satellite_target_previous_pos.y, target->pos.z - Satellite_target_previous_pos.z);
            move = target_moved;
            if (Satellite_return_to_target)
            {
                const float to_target_x = (target->pos.x - Rfg_camera.ideal_pos.x) + old_offset.x;
                const float to_target_z = (target->pos.z - Rfg_camera.ideal_pos.z) + old_offset.z;
                move.set(to_target_x + target_moved.x, target_moved.y + 0.0f, to_target_z + target_moved.z);
                if (to_target_z * to_target_z + to_target_x * to_target_x <= Satellite_cam_stop_dist_to_target_sq)
                {
                    stop_return_to_target_sound();
                    Satellite_return_to_target = false;
                    snap = true;
                    if (!Satellite_return_to_target_user_initiated)
                    {
                        satellite_reaches_target_event reached;
                        reached.target_handle = Satellite_target_handle;
                        gem_post_event(reached);
                    }
                }
            }
        }
        check_range = !Satellite_return_to_target;
    }

    // Keep the looked-at point in a square around the target, turned with the camera.
    if (check_range && target != nullptr)
    {
        const vector looked_at((move.x + Rfg_camera.ideal_pos.x) - old_offset.x, (move.y + Rfg_camera.ideal_pos.y) - old_offset.y, (move.z + Rfg_camera.ideal_pos.z) - old_offset.z);
        vector clamped = looked_at;
        const matrix heading_orient(0.0f, 0.0f, Rfg_camera.ideal_orient.extract_heading());
        vec_change_to_local_coords(clamped, target->pos, heading_orient);
        const float range = Satellite_cam_max_range;
        clamped.x = clamped.x - -range < 0.0f ? -range : clamped.x;
        clamped.x = 0.0f <= clamped.x - range ? range : clamped.x;
        clamped.z = 0.0f <= clamped.z - -range ? clamped.z : -range;
        clamped.z = 0.0f <= clamped.z - range ? range : clamped.z;
        vec_change_to_global_coords(clamped, target->pos, heading_orient);
        move.set((clamped.x - Rfg_camera.ideal_pos.x) + old_offset.x, (clamped.y - Rfg_camera.ideal_pos.y) + old_offset.y, (clamped.z - Rfg_camera.ideal_pos.z) + old_offset.z);
        // Magic number: a clamp of more than 1 cm counts as hitting the edge.
        if (0.01f <= looked_at.x - clamped.x || 0.01f <= -(looked_at.x - clamped.x) || 0.01f <= looked_at.z - clamped.z || 0.01f <= -(looked_at.z - clamped.z))
        {
            snap = true;
        }
    }

    // The velocity, limited.
    const vector vel_wanted(move.x / step_time, move.y / step_time, move.z / step_time);
    vector accel(vel_wanted.x - Satellite_cam_previous_vel.x, vel_wanted.y - Satellite_cam_previous_vel.y, vel_wanted.z - Satellite_cam_previous_vel.z);
    const float accel_mag = accel.flatten_mag_and_normalize_safe();
    vector vel;
    if (control_mouse_camera_control(Satellite_last_h_action, Satellite_last_v_action) && mouse_just_moved())
    {
        // A mouse moves faster and has no acceleration limit. Magic number: twice the speed limit.
        vel = vel_wanted;
        float speed = vel.mag_and_normalize_safe();
        if (0.0f <= speed - Satellite_cam_max_vel_mag * 2.0f)
        {
            speed = Satellite_cam_max_vel_mag * 2.0f;
        }
        vel.set(vel.x * speed, vel.y * speed, vel.z * speed);
    }
    else
    {
        if (snap)
        {
            vel = vel_wanted;
        }
        else
        {
            const float accel_limit = Satellite_return_to_target ? Satellite_cam_max_acc_mag_seek : Satellite_cam_max_acc_mag;
            const float accel_used = 0.0f <= accel_mag - accel_limit ? accel_limit : accel_mag;
            vel.set(accel.x * accel_used + Satellite_cam_previous_vel.x, accel.y * accel_used + Satellite_cam_previous_vel.y, accel.z * accel_used + Satellite_cam_previous_vel.z);
        }
        float speed = vel.mag_and_normalize_safe();
        if (0.0f <= speed - Satellite_cam_max_vel_mag)
        {
            speed = Satellite_cam_max_vel_mag;
        }
        vel.set(vel.x * speed, vel.y * speed, vel.z * speed);
    }

    // The new position.
    Rfg_camera.ideal_pos.set(vel.x * step_time + Rfg_camera.real_pos.x, vel.y * step_time + Rfg_camera.real_pos.y, vel.z * step_time + Rfg_camera.real_pos.z);
    if (input_get_gyro_enabled(GYRO_MODE_SATELLITE))
    {
        Rfg_camera.ideal_pos.x = gyro_offset.x + (Rfg_camera.ideal_pos.x - Camera_satellite_prev_gyro.x);
        Rfg_camera.ideal_pos.y = gyro_offset.y + (Rfg_camera.ideal_pos.y - Camera_satellite_prev_gyro.y);
        Rfg_camera.ideal_pos.z = gyro_offset.z + (Rfg_camera.ideal_pos.z - Camera_satellite_prev_gyro.z);
        Camera_satellite_prev_gyro = gyro_offset;
    }
    Rfg_camera.ideal_pos.x = (new_offset.x - old_offset.x) + Rfg_camera.ideal_pos.x;
    Rfg_camera.ideal_pos.y = (new_offset.y - old_offset.y) + Rfg_camera.ideal_pos.y;
    Rfg_camera.ideal_pos.z = (new_offset.z - old_offset.z) + Rfg_camera.ideal_pos.z;

    // The height over the ground there, and at least 10 m over the terrain under the camera.
    float base_height;
    camera_satellite_get_base_height(base_height, target != nullptr ? target->pos.x : Rfg_camera.ideal_pos.x, target != nullptr ? target->pos.z : Rfg_camera.ideal_pos.z, target);
    Rfg_camera.ideal_pos.y = base_height + height;
    float terrain_y;
    if (havok_terrain_height(terrain_y, Rfg_camera.ideal_pos.x, Rfg_camera.ideal_pos.z, false, nullptr))
    {
        terrain_y = terrain_y + 10.0f;
        if (Rfg_camera.ideal_pos.y < terrain_y)
        {
            Rfg_camera.ideal_pos.y = terrain_y;
        }
    }

    // What the next step starts from.
    if (snap)
    {
        if (target == nullptr || Satellite_previous_target_handle != Satellite_target_handle)
        {
            Satellite_cam_previous_vel.zero();
        }
        else
        {
            Satellite_cam_previous_vel.z = (target->pos.z - Satellite_target_previous_pos.z) / step_time;
            Satellite_cam_previous_vel.y = (target->pos.y - Satellite_target_previous_pos.y) / step_time;
            Satellite_cam_previous_vel.x = (target->pos.x - Satellite_target_previous_pos.x) / step_time;
        }
    }
    else
    {
        Satellite_cam_previous_vel = vel;
    }
    Satellite_cam_previous_ang_vel = ang_vel;
    if (target == nullptr)
    {
        Satellite_previous_target_handle = 0xFFFFFFFF;
    }
    else
    {
        Satellite_target_previous_pos = target->pos;
        Satellite_previous_target_handle = target->get_handle();
    }
    if (last_step_of_frame)
    {
        Satellite_hold_position = false;
    }

    // Magic number: the player's body hangs 2 m behind the camera.
    const vector body_pos(Rfg_camera.ideal_pos.x - Rfg_camera.ideal_orient.fvec.x * 2.0f, Rfg_camera.ideal_pos.y - Rfg_camera.ideal_orient.fvec.y * 2.0f, Rfg_camera.ideal_pos.z - Rfg_camera.ideal_orient.fvec.z * 2.0f);
    pp->update_pos_and_orient(body_pos, Rfg_camera.ideal_orient, true);

    // The scrolling sound loops while the player moves the view.
    if (Satellite_return_to_target || (satellite_input_mag_sq() < Satellite_cam_input_deadzone_mag_sq && satellite_gyro_mag_sq() < Satellite_cam_input_deadzone_mag_sq))
    {
        if (Satellite_scrolling_alr_id != -1)
        {
            // Magic number: a 100 ms fade.
            audiolib_att_loop_rel_stop(Satellite_scrolling_alr_id, 100);
            Satellite_scrolling_alr_id = -1;
        }
    }
    else if (Satellite_scrolling_alr_id == -1)
    {
        audiolib_att_loop_rel_start(Ga_cue_ids.map_scroll, &Satellite_scrolling_alr_id, false, nullptr, nullptr);
    }
}

// 0x006C6B80 (:841)
void camera_satellite_hold_position_this_frame()
{
    if (!Satellite_return_to_target || Satellite_return_to_target_user_initiated)
    {
        Satellite_hold_position = true;
    }
}

// 0x006C6BA0 (:852)
void camera_satellite_return_to_target()
{
    if (Satellite_return_to_target)
    {
        if (Satellite_return_to_target_user_initiated)
        {
            // rfg.exe inlines this call.
            stop_return_to_target_sound();
            Satellite_return_to_target = false;
            Satellite_return_to_target_user_initiated = false;
        }
    }
    else
    {
        Satellite_return_to_target = true;
        Satellite_return_to_target_user_initiated = true;
        // rfg.exe inlines this call.
        start_return_to_target_sound();
    }
}

// 0x006C6C20 (:868)
void camera_satellite_shutdown()
{
    Satellite_shutting_down = true;
}

// 0x006C6C30 (:873)
void camera_satellite_system_init()
{
    Satellite_weapon_info_handle = weapons_get_weapon_info_handle("satellite_rpg");
    Satellite_weapon_timer.invalidate();
    Satellite_weapon_source_pos.zero();
    audiolib_cue_get_id("OBJ_ARTILLERY_GUN_FIRE_FAR_01", &cue_id_artillery_fire);
}

// 0x006C6C70 (:885)
void camera_satellite_level_init()
{
    if (Satellite_camera_is_active)
    {
        Satellite_exit = false;
        Satellite_shutting_down = false;
        Satellite_camera_is_active = false;
        ai_ignore_player(false);
    }
    Satellite_return_to_target = false;
    Satellite_return_to_target_user_initiated = false;
}

// 0x006D6240 (:900)
// Whether the view is on an EDF vehicle with passengers or an EDF human.
static bool get_satellite_target()
{
    raycast_input input;
    input.start = Rfg_camera.real_pos;
    // Magic number: one and a half times the height along the view.
    input.end.set(Rfg_camera.real_orient.fvec.x * Satellite_cam_height * 1.5f + Rfg_camera.real_pos.x, Rfg_camera.real_orient.fvec.y * Satellite_cam_height * 1.5f + Rfg_camera.real_pos.y, Rfg_camera.real_orient.fvec.z * Satellite_cam_height * 1.5f + Rfg_camera.real_pos.z);
    input.filter_params = nullptr;
    input.filter_callback = nullptr;
    input.m_flags = 0;
    input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    // Magic number: a 2.5 m sphere.
    input.radius = 2.5f;
    input.m_filter_info = COLLIDE_OBJECT_COLLECTOR_NO_ITEMS;
    raycast_output output;
    if (!havok_raycast(input, output))
    {
        return false;
    }
    const unsigned int hit_handle = output.m_hits[0].hit_object_handle;
    if (hit_handle == 0xFFFFFFFF)
    {
        return false;
    }
    object* const objp = World.object_from_handle(hit_handle);
    if (objp == nullptr)
    {
        return false;
    }
    if (objp->get_obj_type() == OT_VEHICLE)
    {
        const vehicle* const vp = (const vehicle*)objp;
        return 0 < vehicle_get_num_passengers(vp) && vp->team_of_most_recent_driver == HUMAN_TEAM_EDF;
    }
    return objp->get_obj_type() == OT_HUMAN && ((const human*)objp)->current_team == HUMAN_TEAM_EDF;
}

// 0x006D6470 (:923)
void camera_satellite_get_hud_data(camera_satellite_hud_data& data)
{
    if (Satellite_shutting_down)
    {
        data.shutting_down = true;
        return;
    }
    data.shutting_down = false;
    const bool on_enemy = get_satellite_target();
    data.enemy_target_reticule = on_enemy;
    if (!old_reticule && on_enemy && !audiolib_is_playing(Satellite_target_sound_handle, true))
    {
        Satellite_target_sound_handle = audiolib_cue_play(Ga_cue_ids.avmg_target_enemy, nullptr, nullptr);
    }
    old_reticule = data.enemy_target_reticule;
    const bool timer_elapsed = Satellite_weapon_timer.elapsed();
    data.reloading = !timer_elapsed;
    const bool show_hints = !Satellite_return_to_target || Satellite_return_to_target_user_initiated;
    data.show_hints = show_hints;
    data.can_fire = timer_elapsed && show_hints;
    data.acquiring_target = Satellite_return_to_target;
    if (Satellite_target_handle == 0xFFFFFFFF)
    {
        return;
    }
    const object* const target = World.object_from_handle(Satellite_target_handle);
    if (target == nullptr)
    {
        return;
    }
    // The looked-at point at the target's height, relative to the target and the range: the map's ticker.
    const float height_over = Rfg_camera.ideal_pos.y - target->pos.y;
    vector looked_at(Rfg_camera.ideal_pos.x + Rfg_camera.ideal_orient.fvec.x * height_over, Rfg_camera.ideal_pos.y + 0.0f, Rfg_camera.ideal_pos.z + Rfg_camera.ideal_orient.fvec.z * height_over);
    const matrix heading_orient(0.0f, 0.0f, Rfg_camera.ideal_orient.extract_heading());
    vec_change_to_local_coords(looked_at, target->pos, heading_orient);
    // Magic number: the ticker spans 1.9 times the range.
    const float span = Satellite_cam_max_range * 1.9f;
    data.ticker_pct.x = looked_at.x / span + 0.5f;
    data.ticker_pct.y = 0.5f - looked_at.z / span;
}

// 0x006C6CB0 (:964)
bool camera_satellite_get_hud_target(bool& show, vector2& fire_pos)
{
    const bool no_hit = Satellite_last_hit_pos.is_null();
    const bool reloading = !Satellite_weapon_timer.elapsed();
    show = reloading && !no_hit;
    if (show)
    {
        // The call also writes a depth, which rfg.exe puts over its own parameter and never reads.
        float screen_z;
        gr_screen_coords_from_world_coords(Satellite_last_hit_pos, &fire_pos.x, &fire_pos.y, &screen_z, 0);
        const vector2 scale = vint_project_get_scale(VINT_RES_SCALE_MAINTAIN_RATIO, VINT_RES_SCALE_MAINTAIN_RATIO);
        const vector2 offset = vint_project_get_offset(VINT_RES_OFFSET_START, VINT_RES_OFFSET_START);
        fire_pos.x = (fire_pos.x - offset.x) / scale.x;
        fire_pos.y = (fire_pos.y - offset.y) / scale.y;
    }
    return !no_hit;
}
