// Original: volition/rfg/code/camera/camera_free.cpp
//
// The free camera mode, the vehicle camera (on foot the player uses the third person mode; the other modes also use
// its submode parameters and target: the third person mode's default position, camera_snap_to_default_pos). Whole file
// (stub_misc.md group 5). Compiled in rfg_camera_unity.cpp.
// Multiple-op float expressions are computed in double and rounded once, as the SSE2 code does; the base pitch and the
// atan2 and pow of do_camera_movement are x87 (__CIatan, __CIatan2, __CIpow).

#include "volition/rfg/code/camera/camera_free.h"

#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/gr/grd.h"
#include "volition/rfg/code/camera/camera_slew.h"
#include "volition/rfg/code/camera/camera_transition.h"
#include "volition/rfg/code/camera/camera_view.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle/walker.h"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/string_comp.h"

#include <math.h>
#include <string.h>

// Object sub types of a vehicle target, as the binary compares them.
static const int CAMERA_FREE_SUB_TYPE_AUTOMOBILE = 4;
static const int CAMERA_FREE_SUB_TYPE_WALKER = 6;

static float Sniper_zoom_min = 15.0f; // 0x016481E4 (named after unused_dcf_sniper_zoom_min), the lowest fov of camera_free_modify_fov

// 0x016481E8 (.data): per free_camera_submodes. base_pitch and dist_from_lookat are derived by camera_free_system_init.
free_submode_params Free_camera_submode_params[NUM_FREE_CAMERA_SUBMODES] = {
    {4.25f, 1.5f, 0.0f, 2.0f, 0.0f, 1.0f, -0.785385f, 1.0471799f, 45.0f, {12.0f, {1.0f, 1.0f, 1.0f}, {1.0f, 0.5f, 1.0f}, {1.0f, 1.5f, 0.3f}, {1.0f, 0.3f, 1.0f}, 1.0f, 0.4f, 0.4f}, 0.0f, 0.0f, 0.0f, 0}, // INTERIOR
    {3.5f, 1.2f, 0.0f, 2.0f, 0.0f, 1.0f, -0.785385f, 1.0471799f, 45.0f, {12.0f, {1.0f, 1.0f, 1.0f}, {1.0f, 0.5f, 1.0f}, {1.0f, 1.5f, 0.3f}, {1.0f, 0.3f, 1.0f}, 1.0f, 0.4f, 0.4f}, 0.0f, 0.0f, 0.0f, 0}, // INTERIOR_COMBAT
    {3.5f, 1.2f, 0.0f, 2.0f, 0.0f, 1.0f, -0.785385f, 1.0471799f, 45.0f, {12.0f, {1.0f, 1.0f, 1.0f}, {1.0f, 0.5f, 1.0f}, {1.0f, 1.5f, 0.3f}, {1.0f, 0.3f, 1.0f}, 1.0f, 0.4f, 0.4f}, 0.0f, 0.0f, 0.0f, 0}, // INTERIOR_CLOSE
    {8.0f, 1.4f, 0.0f, 2.0f, 0.0f, 1.0f, -0.785385f, 1.0471799f, 45.0f, {12.0f, {1.0f, 1.0f, 1.0f}, {1.0f, 0.5f, 1.0f}, {1.0f, 1.5f, 0.3f}, {1.0f, 0.3f, 1.0f}, 1.0f, 0.4f, 0.4f}, 0.0f, 0.0f, 0.0f, 0}, // EXTERIOR
    {2.89f, 0.2f, 0.35f, 0.8f, 0.0f, 1.0f, -0.785385f, 1.0471799f, 45.0f, {22.0f, {1.0f, 1.0f, 1.0f}, {1.0f, 0.5f, 1.0f}, {1.0f, 1.5f, 0.3f}, {1.0f, 0.3f, 1.0f}, 1.0f, 0.4f, 0.4f}, 0.0f, 0.0f, 0.0f, 0}, // EXTERIOR_CROUCH
    {2.89f, 0.2f, 0.35f, 1.2f, 0.0f, 1.0f, -0.785385f, 1.0471799f, 45.0f, {22.0f, {1.0f, 1.0f, 1.0f}, {1.0f, 0.5f, 1.0f}, {1.0f, 1.5f, 0.3f}, {1.0f, 0.3f, 1.0f}, 1.0f, 0.4f, 0.4f}, 0.0f, 0.0f, 0.0f, 0}, // EXTERIOR_STAND
    {8.0f, 1.5f, 0.0f, 1.3f, 0.0f, 1.0f, -0.26179498f, 0.436325f, 45.0f, {12.0f, {1.0f, 1.0f, 1.0f}, {1.0f, 0.5f, 1.0f}, {1.0f, 1.5f, 0.3f}, {1.0f, 0.3f, 1.0f}, 1.0f, 0.4f, 0.4f}, 0.0f, 0.0f, 0.0f, 0}, // VEHICLE_DRIVER
    {6.0f, 1.2f, 0.0f, 1.3f, 0.0f, 1.0f, -0.26179498f, 0.436325f, 45.0f, {12.0f, {1.0f, 1.0f, 1.0f}, {1.0f, 0.5f, 1.0f}, {1.0f, 1.5f, 0.3f}, {1.0f, 0.3f, 1.0f}, 1.0f, 0.4f, 0.4f}, 0.0f, 0.0f, 0.0f, 0}, // VEHICLE_CLOSE
    {8.0f, 1.5f, 0.0f, 1.3f, 0.0f, 1.0f, -0.26179498f, 0.436325f, 45.0f, {12.0f, {1.0f, 1.0f, 1.0f}, {1.0f, 0.5f, 1.0f}, {1.0f, 1.5f, 0.3f}, {1.0f, 0.3f, 1.0f}, 1.0f, 0.4f, 0.4f}, 0.0f, 0.0f, 0.0f, 0}, // VEHICLE_PASSENGER
    {8.0f, 1.5f, 0.0f, 1.3f, 0.0f, 1.0f, -0.26179498f, 0.436325f, 45.0f, {12.0f, {1.0f, 1.0f, 1.0f}, {1.0f, 0.5f, 1.0f}, {1.0f, 1.5f, 0.3f}, {1.0f, 0.3f, 1.0f}, 1.0f, 0.4f, 0.4f}, 0.0f, 0.0f, 0.0f, 0}, // VEHICLE_EXTERIOR
    {70.0f, 100.0f, 0.0f, 2.0f, 0.0f, 1.0f, -0.785385f, 1.0471799f, 45.0f, {12.0f, {1.0f, 1.0f, 1.0f}, {1.0f, 0.5f, 1.0f}, {1.0f, 1.5f, 0.3f}, {1.0f, 0.3f, 1.0f}, 1.0f, 0.4f, 0.4f}, 0.0f, 0.0f, 0.0f, 0}, // BIRDS_EYE_DEBUG
    {0.0f, 1.0f, 0.0f, 2.0f, 0.0f, 0.0f, -0.785385f, 1.0471799f, 45.0f, {0.0f, {1.0f, 1.0f, 1.0f}, {1.0f, 0.5f, 1.0f}, {1.0f, 1.5f, 0.3f}, {1.0f, 0.3f, 1.0f}, 0.0f, 0.4f, 0.4f}, 0.0f, 0.0f, 0.0f, 0}, // ZOOM
    {8.0f, 1.4f, 0.0f, 2.0f, 0.0f, 1.0f, -0.785385f, 1.0471799f, 45.0f, {12.0f, {1.0f, 1.0f, 1.0f}, {1.0f, 0.5f, 1.0f}, {1.0f, 1.5f, 0.3f}, {1.0f, 0.3f, 1.0f}, 1.0f, 0.4f, 0.4f}, 0.0f, 0.0f, 0.0f, 0}, // SPECTATOR
};
static float Vehicle_mode_xz_multiplier = 2.0f; // 0x016487CC, the driver submode's distance over the close one's
static float Vehicle_mode_y_multiplier = 2.0f; // 0x016487D0
// do_camera_movement overwrites these four every step, from the mouse or stick values below.
static float User_rot_accel = 5.0f; // 0x016487D4
static float User_rot_decel = 8.0f; // 0x016487D8
static float User_rot_secs_to_stop = 0.1f; // 0x016487DC
static float Max_user_rot_rate = 6.0f; // 0x016487E0
static float Elev_vel = 1.2f; // 0x016487E4
static Camera_free_control_types Vehicle_camera_control_type = CAMERA_FREE_CONTROL_TYPE_PREDICTIVE; // 0x016487E8
static Camera_free_control_types Tank_camera_control_type = CAMERA_FREE_CONTROL_TYPE_TREAD_CAM_REL; // 0x016487EC
static float Vehicle_camera_normal_rotation_rate = 0.2f; // 0x016487F0
static float Camera_exit_view_angle_tolerance = 0.65f; // 0x016487F4, radians around the flamer walker's rear
static float Camera_exit_view_adjust_rate = 0.02f; // 0x016487F8, radians per step
static float Vehicle_camera_steer_adjust_speed_mult = 4.0f; // 0x016487FC
static float Vehicle_camera_steer_angle_mult = -1.5f; // 0x01648800
static float Camera_max_follow_chassis_speed = 5.0f; // 0x01648804
static float Camera_chassis_follow_multiplier = 15.0f; // 0x01648808
static float Camera_chassis_follow_max_change = 0.01f; // 0x0164880C
static float Vehicle_camera_roll_min_speed = 12.0f; // 0x01648810
static float Vehicle_camera_roll_min_sideways = 0.8f; // 0x01648814
static float Vehicle_camera_roll_max_sideways = 9.0f; // 0x01648818
static float Vehicle_camera_roll_max_angle = 0.08f; // 0x0164881C
static float Vehicle_camera_roll_max_change = 0.002f; // 0x01648820
static float Mouse_control_free_cam_multiplier = 1.4f; // 0x01648824
static const char* Camera_free_control_type_strings[CAMERA_FREE_CONTROL_TYPE_COUNT] = {"normal", "predictive", "tread", "tread_cam_rel"}; // 0x01648828
static float Vehicle_camera_reverse_speed_max = 15.0f; // 0x01648838
static float Vehicle_camera_reverse_speed_min = 7.0f; // 0x0164883C (named here from its use)
static float Camera_pan_multiplier = 1.0f; // 0x01648840
static float Pitch_damping = 0.93f; // 0x01648844
static float Camera_collision_percent_rate_in = 5.0f; // 0x01648848
static float Camera_collision_percent_rate_in_manual = 15.0f; // 0x0164884C
static float Camera_collision_percent_rate_out = 1.0f; // 0x01648850
static float Camera_collision_size = 0.15f; // 0x01648854
static bool Vehicle_camera_lock_world_space = true; // 0x01648858
static int Vehicle_camera_default_snap_time = 250; // 0x0164885C, ms the camera holds a manual turn
static int Vehicle_camera_ramp_time = 1750; // 0x01648860, ms the swing rate falls after the hold
static float Pc_mouse_rot_accel = 6.0f; // 0x01648864
static float Pc_mouse_rot_decel = 200.0f; // 0x01648868
static float Pc_mouse_rot_max_walker = 4.0f; // 0x0164886C
static float Pc_mouse_rot_max = 7.0f; // 0x01648870
static float Pc_rot_stop_time = 0.001f; // 0x01648874
static timestamp Vehicle_camera_snap_timestamp; // 0x01648984 (.data -1: invalid until set)
static bool Vehicle_camera_g_forces_roll = true; // 0x01648001
static bool Hill_tracking_camera = true; // 0x01648002
static bool Vehicle_camera_collision_enable = true; // 0x01648003
static float Vehicle_camera_recenter_rate; // 0x01DE4534 (a tweak; 0 until the tweak table sets it)
static float Vehicle_small_angle; // 0x01DE4538 (a tweak)
static float Camera_vehicle_fov; // 0x01DE4544 (a tweak)
static float Vehicle_predictive_adjust; // 0x01DE48A0 (a tweak)
static tweak_table_item Tweak_table_item_Vehicle_camera_recenter_rate("Vehicle_camera_recenter_rate", &Vehicle_camera_recenter_rate); // 0x01DE4960
static tweak_table_item Tweak_table_item_Vehicle_small_angle("Vehicle_small_angle", &Vehicle_small_angle); // 0x01DE6B40
static tweak_table_item Tweak_table_item_Vehicle_predictive_adjust("Vehicle_predictive_adjust", &Vehicle_predictive_adjust); // 0x01DE6AF4
static tweak_table_item Tweak_table_item_Camera_vehicle_fov("Camera_vehicle_fov", &Camera_vehicle_fov); // 0x01DE4A60
static bool Vehicle_camera_use_alt; // 0x01DE48AA: Ghidra lists reads only, so the alternate steering stays off
static bool Vehicle_camera_g_forces_chassis; // 0x01DE48AB: Ghidra lists reads only, so the chassis follow stays off
vehicle_camera_control_mode Camera_free_mouse_control_mode; // 0x01DE48B0 (the controller options set it)
static float Acceleration_adjust; // 0x01DE48B4
static float Vehicle_camera_roll; // 0x01DE48B8
static bool Vehicle_camera_elevation_lock; // 0x01DE48BC
static bool Show_camera_lookat; // 0x01DE48BD
static bool Birds_eye_cam; // 0x01DE48BF
static vector Vehicle_camera_last_target_fvec; // 0x01DE6B34

// The vehicle the camera follows, if its target is one (inlined in several functions of the binary).
static vehicle* camera_free_target_vehicle()
{
    if (Rfg_camera.target_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(Rfg_camera.target_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
    {
        return nullptr;
    }
    return (vehicle*)objp;
}

// The usage text of the two control type commands (repeated in the binary).
static void camera_free_control_type_usage(const char* usage)
{
    console_output(usage, nullptr, false);
    for (int i = 0; i < CAMERA_FREE_CONTROL_TYPE_COUNT; i++)
    {
        console_output(Camera_free_control_type_strings[i], nullptr, false);
    }
}

// The body of the two control type commands (repeated in the binary).
static void camera_free_control_type_command(const char* usage, Camera_free_control_types& control_type)
{
    if (Console_command == 0)
    {
        if (Console_help != 0)
        {
            camera_free_control_type_usage(usage);
        }
        return;
    }
    if (!console_get_arg(2, 1))
    {
        camera_free_control_type_usage(usage);
        return;
    }
    for (int i = 0; i < CAMERA_FREE_CONTROL_TYPE_COUNT; i++)
    {
        if (strcmp(Console_arg, Camera_free_control_type_strings[i]) == 0) // an inlined strcmp in the binary
        {
            control_type = (Camera_free_control_types)i;
            return;
        }
    }
    console_output("Error: camera control type not found.", nullptr, false);
}

// 0x006C4230 (:376)
void unused_dcf_vehicle_camera_type()
{
    camera_free_control_type_command("Usage: vehicle_camera_type <type>.  Valid types are...", Vehicle_camera_control_type);
}

// 0x006C4320 (:404)
void unused_dcf_tank_camera_type()
{
    camera_free_control_type_command("Usage: tank_camera_type <type>.  Valid types are...", Tank_camera_control_type);
}

// 0x006C4410 (:467)
void unused_dcf_cam_heading_stop_time()
{
    if (Console_command != 0)
    {
        console_get_arg(0x10, 0);
        User_rot_secs_to_stop = Console_arg_float;
        if (Console_arg_float < 0.0001f)
        {
            User_rot_secs_to_stop = 0.0001f;
        }
        return;
    }
    if (Console_help != 0)
    {
        console_output("Usage: cam_heading_stop_time [time in seconds]", nullptr, false);
    }
}

// 0x006CABE0 (:484)
// Original: the help text and the error text list different arguments and mode names, and most of those names are not
// accepted.
void unused_dcf_cam_config()
{
    if (Console_command == 0)
    {
        if (Console_help != 0)
        {
            console_output("Usage: cam_config [mode_name] [xz_dist] [y_dist] [target_y_offset]", nullptr, false);
        }
        return;
    }
    console_get_arg(2, 0);
    free_submode_params* params;
    if (v_stricmp(Console_arg, "default_stand") == 0)
    {
        params = &Free_camera_submode_params[FREE_CAMERA_SUBMODE_EXTERIOR_STAND];
    }
    else if (v_stricmp(Console_arg, "exterior_far") == 0 || v_stricmp(Console_arg, "exterior_b") == 0)
    {
        params = &Free_camera_submode_params[FREE_CAMERA_SUBMODE_EXTERIOR];
    }
    else if (v_stricmp(Console_arg, "default_crouch") == 0)
    {
        params = &Free_camera_submode_params[FREE_CAMERA_SUBMODE_EXTERIOR_CROUCH];
    }
    else if (v_stricmp(Console_arg, "default_interior") == 0 || v_stricmp(Console_arg, "interior") == 0)
    {
        params = &Free_camera_submode_params[FREE_CAMERA_SUBMODE_INTERIOR_CLOSE];
    }
    else if (v_stricmp(Console_arg, "interior_combat") == 0)
    {
        params = &Free_camera_submode_params[FREE_CAMERA_SUBMODE_INTERIOR_COMBAT];
    }
    else if (v_stricmp(Console_arg, "vehicle_b") == 0)
    {
        console_get_arg(0x10, 0);
        Vehicle_mode_xz_multiplier = Console_arg_float;
        console_get_arg(0x10, 0);
        Vehicle_mode_y_multiplier = Console_arg_float;
        Console_arg_float = Free_camera_submode_params[FREE_CAMERA_SUBMODE_VEHICLE_DRIVER].free_fov; // an optional argument keeps it
        console_get_arg(0x10, 1);
        Free_camera_submode_params[FREE_CAMERA_SUBMODE_VEHICLE_DRIVER].free_fov = Console_arg_float;
        camera_free_recalc_vehicle_submode_params();
        return;
    }
    else
    {
        console_output("Usage: cam_config [mode_name] [xz_dist] [y_dist] [target_x_offset] [target_y_offset] [target_z_offset]", nullptr, false);
        console_output("[mode_name] can be any of: default_exterior, exterior_close, exterior_a, exterior_far, exterior_b, exterior_combat, combat, default_interior, interior, interior_combat, vehicle_b", nullptr, false);
        return;
    }
    console_get_arg(0x10, 0);
    params->xz_dist = Console_arg_float;
    console_get_arg(0x10, 0);
    params->y_dist = Console_arg_float;
    console_get_arg(0x10, 0);
    params->lookat_offset_x = Console_arg_float;
    console_get_arg(0x10, 0);
    params->lookat_offset_y = Console_arg_float;
    console_get_arg(0x10, 0);
    params->lookat_offset_z = Console_arg_float;
    Console_arg_float = params->free_fov; // an optional argument keeps it
    console_get_arg(0x10, 1);
    params->free_fov = Console_arg_float;
    params->base_pitch = (float)atan((double)(float)(params->y_dist / params->xz_dist)); // x87 __CIatan
    params->dist_from_lookat = (float)sqrt((double)(float)((double)params->xz_dist * (double)params->xz_dist + (double)params->y_dist * (double)params->y_dist));
    if (Rfg_camera.free_params.submode_params == params)
    {
        Rfg_camera.free_params.flags.elev_relaxing = 1;
        Rfg_camera.free_params.desired_elev_angle = params->base_pitch;
    }
}

// 0x006CAEC0 (:559)
void unused_dcf_cam_fov()
{
    if (Console_command == 0)
    {
        if (Console_help != 0)
        {
            console_output("Usage: cam_fov <fov_degrees>", nullptr, false);
        }
        return;
    }
    console_get_arg(0x10, 0);
    camera_free_set_fov(Console_arg_float); // inlined in the binary
    camera_slew_set_fov(Console_arg_float); // inlined in the binary, around the fov loop
}

// 0x006C4470 (:624)
void unused_dcf_sniper_zoom_min()
{
    if (Console_command != 0)
    {
        console_get_arg(0x10, 0);
        Sniper_zoom_min = Console_arg_float;
        return;
    }
    if (Console_help != 0)
    {
        console_output("Usage: sniper_zoom_min <minimum fov_degrees>", nullptr, false);
    }
}

// 0x006CAF30 (:672)
void unused_dcf_birds_eye_cam()
{
    if (Console_command == 0)
    {
        return;
    }
    console_get_arg(0xC1, 0); // Magic number: an on, off or no argument (the type bits below)
    if ((Console_arg_type & 0x40) != 0)
    {
        Birds_eye_cam = true;
        Rfg_camera.free_params.desired_elev_angle = Free_camera_submode_params[FREE_CAMERA_SUBMODE_BIRDS_EYE_DEBUG].base_pitch;
        return;
    }
    if ((Console_arg_type & 0x80) != 0)
    {
        Birds_eye_cam = false;
        return;
    }
    if ((Console_arg_type & 1) != 0)
    {
        Birds_eye_cam = !Birds_eye_cam;
    }
    if (Birds_eye_cam)
    {
        Rfg_camera.free_params.desired_elev_angle = Free_camera_submode_params[FREE_CAMERA_SUBMODE_BIRDS_EYE_DEBUG].base_pitch;
    }
}

// 0x006C44C0 (:1515)
bool camera_free_submodes_are_equivalent(free_submode_params* params, free_submode_params* desired_params)
{
    const float d_dist = params->dist_from_lookat - desired_params->dist_from_lookat;
    if (d_dist >= 0.001f || -d_dist >= 0.001f)
    {
        return false;
    }
    const float d_fov = params->free_fov - desired_params->free_fov;
    if (d_fov >= 0.001f || -d_fov >= 0.001f)
    {
        return false;
    }
    const float d_x = params->lookat_offset_x - desired_params->lookat_offset_x;
    if (d_x >= 0.001f || -d_x >= 0.001f)
    {
        return false;
    }
    const float d_y = params->lookat_offset_y - desired_params->lookat_offset_y;
    if (d_y >= 0.001f || -d_y >= 0.001f)
    {
        return false;
    }
    const float d_z = params->lookat_offset_z - desired_params->lookat_offset_z;
    return d_z < 0.001f && -d_z < 0.001f;
}

// 0x006C45B0 (:1548)
// Each submode's pitch and distance from its follow offsets.
void camera_free_system_init()
{
    for (int i = 0; i < NUM_FREE_CAMERA_SUBMODES; i++)
    {
        free_submode_params& p = Free_camera_submode_params[i];
        p.base_pitch = (float)atan((double)(float)(p.y_dist / p.xz_dist)); // x87 __CIatan
        p.dist_from_lookat = (float)sqrt((double)(float)((double)p.xz_dist * (double)p.xz_dist + (double)p.y_dist * (double)p.y_dist));
    }
}

// 0x006D17D0 (:1808)
// The point the camera looks at: the target (its ragdoll, or a vehicle's center of mass with the vehicle camera
// offset) plus the submode's look-at offset, raised as the camera pitches down.
vector camera_free_get_lookat(float pitch)
{
    vector lookat = ZERO_VECTOR;
    if ((Multiplayer && multi_is_dedicated_server()) || multi_game_spectating())
    {
        return ZERO_VECTOR;
    }
    object* target = World.object_from_handle(Rfg_camera.target_handle);
    free_submode_params* params = Rfg_camera.free_params.submode_params;
    if (target == nullptr)
    {
        return lookat;
    }
    if (target->get_obj_type() == OT_HUMAN && ((human*)target)->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE)
    {
        havok_ragdoll_get_pos(((human*)target)->ragdoll_idx, lookat, RAGDOLL_BODY);
    }
    else
    {
        lookat.x = params->lookat_offset_x;
        lookat.z = params->lookat_offset_z;
        lookat.y = 0.0f;
        lookat.unrotate(Rfg_camera.real_orient);
        lookat.x = lookat.x + 0.0f;
        lookat.y = params->lookat_offset_y + lookat.y;
        lookat.z = lookat.z + 0.0f;
        vehicle* vp = target->get_obj_type() == OT_VEHICLE ? camera_free_target_vehicle() : nullptr;
        if (vp != nullptr)
        {
            vector offset = ZERO_VECTOR;
            vector body_pos;
            matrix body_orient;
            havok_body_get_pos_orient(vp->havok_handle, body_pos, body_orient);
            float mass;
            vector center_of_mass;
            matrix inertia;
            havok_body_get_mass_props(vp->havok_handle, mass, center_of_mass, inertia);
            if (vp->get_sub_type() == 6)
            {
                // a walker: its object transform raised by its height offset
                body_pos = target->pos;
                body_pos.y = World.object_cast<walker>(vp)->y_offset + body_pos.y;
                body_orient = target->orient;
            }
            else
            {
                offset = center_of_mass + offset;
            }
            offset = camera_free_get_vehicle_camera_settings().lookat_vehicle_offset + offset;
            offset.unrotate(body_orient);
            lookat.x = (float)((double)body_pos.x + (double)offset.x + (double)lookat.x);
            lookat.y = (float)((double)offset.y + (double)body_pos.y + (double)lookat.y);
            lookat.z = (float)((double)body_pos.z + (double)offset.z + (double)lookat.z);
        }
        else
        {
            lookat.x = lookat.x + target->pos.x;
            lookat.y = target->pos.y + lookat.y;
            lookat.z = target->pos.z + lookat.z;
        }
    }
    if (pitch < 0.0f)
    {
        float t = pitch / params->min_elev_angle_rad;
        if (t - 0.0f < 0.0f)
        {
            t = 0.0f;
        }
        if (t - 1.0f >= 0.0f)
        {
            t = 1.0f;
        }
        lookat.y = (float)((double)params->lookat_offset_y_delta * (double)t + (double)lookat.y);
    }
    return lookat;
}

// 0x006D1CD0 (:1907)
// Behind the target at the submode's distance and height, from the look-at point at the base pitch.
vector camera_free_get_default_pos()
{
    free_submode_params* params = Rfg_camera.free_params.submode_params;
    if ((Multiplayer && multi_is_dedicated_server()) || multi_game_spectating())
    {
        return ZERO_VECTOR;
    }
    object* target = World.object_from_handle(Rfg_camera.target_handle);
    if (target == nullptr)
    {
        return ZERO_VECTOR;
    }
    const vector lookat = camera_free_get_lookat(params->base_pitch); // inlined in the binary
    const float d = params->xz_dist;
    vector pos;
    pos.x = (float)((double)lookat.x - (double)target->orient.fvec.x * (double)d);
    pos.y = (float)((double)params->y_dist + ((double)lookat.y - (double)target->orient.fvec.y * (double)d));
    pos.z = (float)((double)lookat.z - (double)target->orient.fvec.z * (double)d);
    return pos;
}

// The mouse (or Steam controller) delta of one camera axis for the alternate control mode: x or y pixels, inverted
// as the axis is (inlined in camera_free_set_rotation and camera_free_set_elevation).
static float camera_free_alternate_delta(control_axis_action action, bool vertical)
{
    if (control_steam_camera_control())
    {
        return controls_get_raw_analog_uncapped(action, Player_controller_index);
    }
    int x = 0;
    int y = 0;
    int wheel = 0;
    mouse_get_deltas(x, y, wheel);
    float delta = vertical ? (float)-y : (float)x;
    if (controls_get_axis_action_inverted_state(action, Player_controller_index, true))
    {
        delta = -delta;
    }
    return delta;
}

// 0x006CAFB0 (:1939)
// The stick turns the camera (faster with the mouse); in the alternate mouse mode the mouse moves an absolute
// heading offset instead. A real turn holds the vehicle camera from swinging back for the snap time.
void camera_free_set_rotation(float rot, control_axis_action h_action)
{
    free_mode_params& fp = Rfg_camera.free_params;
    fp.user_rot = (float)((double)-rot * (double)fp.control_multiplier);
    fp.last_h_action = h_action;
    if (Camera_free_mouse_control_mode == VCCM_DEFAULT && (control_mouse_camera_control(h_action, fp.last_v_action) || control_steam_camera_control()))
    {
        fp.user_rot = (float)((double)fp.user_rot * (double)Mouse_control_free_cam_multiplier);
    }
    if (Camera_free_mouse_control_mode == VCCM_ALTERNATE1 && (control_mouse_camera_control(fp.last_h_action, fp.last_v_action) || control_steam_camera_control()))
    {
        const float delta = camera_free_alternate_delta(h_action, false);
        fp.user_rot = 0.0f;
        // Magic number: 0.0025 radians per pixel at sensitivity 1.
        fp.mouse_absolute_pos.x = (float)((double)Mouse_sensitivity * (double)delta * 0.0025 + (double)fp.mouse_absolute_pos.x);
    }
    if ((double)rot - 0.0 >= 0.01 || -((double)rot - 0.0) >= 0.01)
    {
        Vehicle_camera_snap_timestamp.set(Vehicle_camera_default_snap_time);
        Vehicle_camera_last_target_fvec = World.object_from_handle(Rfg_camera.target_handle)->orient.fvec;
    }
}

// 0x006CB1A0 (:1988)
// As camera_free_set_rotation for the pitch; any input stops the elevation relaxing to the base pitch.
void camera_free_set_elevation(float elev, control_axis_action v_action)
{
    free_mode_params& fp = Rfg_camera.free_params;
    fp.user_elev = (float)((double)-elev * (double)fp.control_multiplier);
    fp.last_v_action = v_action;
    if (Camera_free_mouse_control_mode == VCCM_DEFAULT && (control_mouse_camera_control(fp.last_h_action, v_action) || control_steam_camera_control()))
    {
        fp.user_elev = (float)((double)fp.user_elev * (double)Mouse_control_free_cam_multiplier);
    }
    if (Camera_free_mouse_control_mode == VCCM_ALTERNATE1 && (control_mouse_camera_control(fp.last_h_action, fp.last_v_action) || control_steam_camera_control()))
    {
        const float delta = camera_free_alternate_delta(v_action, true);
        fp.user_elev = 0.0f;
        fp.mouse_absolute_pos.y = (float)((double)fp.mouse_absolute_pos.y - (double)Mouse_sensitivity * (double)delta * 0.0025);
    }
    if (elev != 0.0f)
    {
        fp.flags.elev_relaxing = 0;
    }
    if ((double)elev - 0.0 >= 0.01 || -((double)elev - 0.0) >= 0.01)
    {
        Vehicle_camera_snap_timestamp.set(Vehicle_camera_default_snap_time);
        Vehicle_camera_last_target_fvec = World.object_from_handle(Rfg_camera.target_handle)->orient.fvec;
    }
}

// 0x006C4640 (:2041)
void camera_free_set_control_multiplier(float multiplier)
{
    Rfg_camera.free_params.control_multiplier = multiplier;
}

// 0x006C4650 (:2051)
void camera_free_set_drift(float pitch, float heading)
{
    Rfg_camera.free_params.pitch_drift_vel = pitch;
    Rfg_camera.free_params.heading_drift_vel = heading;
}

// 0x006D5010 (:1561)
// The submodes of a level: standing outside, the interior and vehicle ones for later.
void camera_free_level_init_post()
{
    const float pitch = Free_camera_submode_params[FREE_CAMERA_SUBMODE_EXTERIOR_STAND].base_pitch;
    Rfg_camera.free_params.control_type = CAMERA_FREE_CONTROL_TYPE_NORMAL;
    Rfg_camera.free_params.submode_params = &Free_camera_submode_params[FREE_CAMERA_SUBMODE_EXTERIOR_STAND];
    Rfg_camera.free_params.cur_exterior_params = &Free_camera_submode_params[FREE_CAMERA_SUBMODE_EXTERIOR_STAND];
    Rfg_camera.free_params.cur_interior_params = &Free_camera_submode_params[FREE_CAMERA_SUBMODE_INTERIOR_CLOSE];
    Rfg_camera.free_params.cur_vehicle_params = &Free_camera_submode_params[FREE_CAMERA_SUBMODE_VEHICLE_CLOSE];
    Rfg_camera.free_params.last_lookat_pos = camera_free_get_lookat(pitch); // inlined in the binary
    Rfg_camera.free_params.last_vehicle_pitch = 0.0f;
    Rfg_camera.free_params.last_elev_angle = Rfg_camera.free_params.submode_params->base_pitch;
    Rfg_camera.free_params.flags.elev_relaxing = 0;
    Vehicle_camera_snap_timestamp.invalidate();
    Rfg_camera.free_params.control_multiplier = 1.0f;
    Rfg_camera.free_params.pitch_drift_vel = 0.0f;
    Rfg_camera.free_params.heading_drift_vel = 0.0f;
    Rfg_camera.free_params.desired_elev_angle = Rfg_camera.free_params.last_elev_angle;
    Rfg_camera.free_params.debug_offset.zero();
    memset(&Rfg_camera.free_params.flags, 0, sizeof(Rfg_camera.free_params.flags));
    Rfg_camera.free_params.suppressed_target_lag_pct = 0.0f;
    Rfg_camera.free_params.goal_dist_from_lookat = Rfg_camera.free_params.submode_params->dist_from_lookat;
    Rfg_camera.free_params.cur_dist_from_lookat = Rfg_camera.free_params.submode_params->dist_from_lookat;
    Rfg_camera.free_params.submode_params->vehicle_camera_selection = 0;
    Rfg_camera.free_params.submode_params->y_axis_rotation_speed = 1.0f;
    Rfg_camera.free_params.last_h_action = CAA_CAMERA_ROTATE;
    Rfg_camera.free_params.last_v_action = CAA_CAMERA_ELEVATE;
}

// 0x006D5610 (:1604)
// Entering the mode: the look-at point and the elevation start from the submode's base pitch, the input from rest.
void camera_free_mode_init()
{
    free_mode_params& fp = Rfg_camera.free_params;
    const float pitch = fp.submode_params->base_pitch;
    fp.last_lookat_pos = camera_free_get_lookat(pitch); // inlined in the binary
    fp.last_vehicle_pitch = 0.0f;
    if (!Vehicle_camera_elevation_lock)
    {
        fp.last_elev_angle = fp.submode_params->base_pitch;
    }
    fp.flags.elev_relaxing = 0;
    fp.desired_elev_angle = fp.last_elev_angle;
    if (Vehicle_camera_snap_timestamp.valid())
    {
        Vehicle_camera_snap_timestamp.set(0);
    }
    fp.user_rot = 0.0f;
    fp.auto_rot = 0.0f;
    fp.user_rot_vel = 0.0f;
    fp.auto_rot_vel = 0.0f;
    fp.mouse_absolute_pos.zero();
}

// A manual turn is still holding the camera from swinging back.
static bool camera_free_snap_holding()
{
    return Vehicle_camera_snap_timestamp.valid() && !Vehicle_camera_snap_timestamp.elapsed();
}

// The camera is turned by the mouse or the Steam controller.
static bool camera_free_mouse_steering()
{
    const free_mode_params& fp = Rfg_camera.free_params;
    return control_mouse_camera_control(fp.last_h_action, fp.last_v_action) || control_steam_camera_control();
}

// The distance from a point to the look-at point, as the binary computes it (inlined twice).
static float camera_free_dist_to(const vector& p, const vector& lookat_pos)
{
    const float dx = (float)((double)p.x - (double)lookat_pos.x);
    const float dy = (float)((double)p.y - (double)lookat_pos.y);
    const float dz = (float)((double)p.z - (double)lookat_pos.z);
    return (float)sqrt((double)(float)((double)dx * (double)dx + (double)dy * (double)dy + (double)dz * (double)dz));
}

// 0x006D6F20
// How far out the camera can go from the look-at point: an object in the way moves the goal in smoothly, and terrain
// moves the camera there at once.
// LTCG: camera in EDI, desired_orient in EAX, lookat_pos in ESI.
static void handle_camera_obstruction(rfg_camera* camera, const matrix* desired_orient, const vector* lookat_pos)
{
    free_mode_params& fp = camera->free_params;
    const free_submode_params* params = fp.submode_params;
    const float dist = params->dist_from_lookat;
    if (!Vehicle_camera_collision_enable)
    {
        fp.cur_dist_from_lookat = dist;
        fp.goal_dist_from_lookat = dist;
        return;
    }
    const vector start = *lookat_pos;
    vector end;
    end.x = (float)((double)start.x + (double)(float)((double)desired_orient->fvec.x * (double)dist));
    end.y = (float)((double)start.y + (double)(float)((double)desired_orient->fvec.y * (double)dist));
    end.z = (float)((double)start.z + (double)(float)((double)desired_orient->fvec.z * (double)dist));
    vector hit;
    if (!camera_collision_test(start, end, &hit, Camera_collision_size, COLLIDE_RAY_DEFAULT))
    {
        fp.goal_dist_from_lookat = params->dist_from_lookat;
        return;
    }
    fp.goal_dist_from_lookat = camera_free_dist_to(hit, *lookat_pos);
    if (!camera_collision_test(start, end, &hit, Camera_collision_size, COLLIDE_RAY_TERRAIN_ONLY))
    {
        return;
    }
    const float terrain_dist = camera_free_dist_to(hit, *lookat_pos);
    fp.cur_dist_from_lookat = terrain_dist;
    fp.goal_dist_from_lookat = terrain_dist;
}

// 0x006D7180
// One step of the camera: turns the input into a heading and an elevation, follows the target by its control type,
// keeps out of obstructions, and leans the view with the vehicle.
// LTCG: camera in EAX, the rest on the stack.
// Original: reads through a null target (camera_free_process lets one through unless spectating or a dedicated server).
static void do_camera_movement(rfg_camera* camera, const vector* lookat_pos, float frametime, float* last_min_angle, float* last_max_angle)
{
    free_mode_params& fp = camera->free_params;
    const double ft = (double)frametime;
    camera_free_update_vehicle_camera_settings();
    if (Camera_free_mouse_control_mode == VCCM_DEFAULT && camera_free_mouse_steering())
    {
        User_rot_accel = Pc_mouse_rot_accel;
        User_rot_decel = Pc_mouse_rot_decel;
        const object* objp = World.object_from_handle(camera->target_handle);
        Max_user_rot_rate = objp != nullptr && objp->get_sub_type() == CAMERA_FREE_SUB_TYPE_WALKER ? Pc_mouse_rot_max_walker : Pc_mouse_rot_max;
        User_rot_secs_to_stop = Pc_rot_stop_time;
    }
    else
    {
        User_rot_accel = 5.0f;
        User_rot_decel = 8.0f;
        User_rot_secs_to_stop = 0.1f;
        Max_user_rot_rate = 6.0f;
    }
    const free_submode_params* params = Birds_eye_cam ? &Free_camera_submode_params[FREE_CAMERA_SUBMODE_BIRDS_EYE_DEBUG] : fp.submode_params;
    object* target = World.object_from_handle(camera->target_handle);
    *last_min_angle = params->min_elev_angle_rad;
    *last_max_angle = params->max_elev_angle_rad;

    // Looking back, returning from it, or a wrecking crew map that locks the camera: a snap to that direction.
    bool snapped = false;
    if (fp.flags.look_back)
    {
        if (!fp.flags.look_back_last)
        {
            camera_free_swing_to_dir(-target->orient.fvec, 0.0f);
            snapped = true;
        }
    }
    else
    {
        const bool locked = wrecking_crew_is_active() && !wrecking_crew_can_player_move_camera();
        if (locked || fp.flags.look_back_last)
        {
            // A locked camera keeps its own ideal direction; otherwise back to the target's front.
            camera_free_swing_to_dir(locked ? camera->ideal_orient.fvec : target->orient.fvec, 0.0f);
            camera_free_clear_snap_time(); // inlined in the binary
            snapped = true;
        }
    }
    if (snapped)
    {
        Rfg_camera.flags |= 0x10; // camera_do_frame takes this position without its smoothing
        fp.user_rot_vel = 0.0f;
    }
    fp.flags.look_back_last = fp.flags.look_back;

    // The user's turn rate ramps toward the stick's, and the mouse sets it outright.
    const double pan = (double)Camera_pan_multiplier;
    const float goal_rot_vel = (float)(pan * (double)Max_user_rot_rate * (double)fp.user_rot);
    if (goal_rot_vel == 0.0f || (double)fp.user_rot_vel * (double)goal_rot_vel < 0.0)
    {
        const double decel = pan * (double)User_rot_decel * ft;
        float v;
        if (fp.user_rot_vel >= 0.0f)
        {
            v = (float)((double)fp.user_rot_vel - decel);
            if ((float)((double)v - (double)goal_rot_vel) < 0.0f)
            {
                v = goal_rot_vel;
            }
        }
        else
        {
            v = (float)(decel + (double)fp.user_rot_vel);
            if ((float)((double)v - (double)goal_rot_vel) >= 0.0f)
            {
                v = goal_rot_vel;
            }
        }
        fp.user_rot_vel = v;
    }
    else
    {
        const float abs_goal = (float)fabs((double)goal_rot_vel);
        const float abs_vel = (float)fabs((double)fp.user_rot_vel);
        float v;
        if (Camera_free_mouse_control_mode == VCCM_DEFAULT && camera_free_mouse_steering())
        {
            v = abs_goal;
        }
        else if ((double)abs_goal >= (double)abs_vel)
        {
            v = (float)((double)abs_vel + pan * (double)User_rot_accel * ft);
            if ((float)((double)v - (double)abs_goal) >= 0.0f)
            {
                v = abs_goal;
            }
            User_rot_decel = (float)((double)v / (double)User_rot_secs_to_stop);
        }
        else
        {
            v = (float)((double)abs_vel - pan * (double)User_rot_decel * ft);
            if ((float)((double)v - (double)abs_goal) < 0.0f)
            {
                v = abs_goal;
            }
        }
        fp.user_rot_vel = goal_rot_vel < 0.0f ? -v : v;
    }
    float rot = (float)((double)(float)((double)fp.user_rot_vel * ft) + (double)fp.heading_drift_vel * ft);
    if (snapped)
    {
        rot = 0.0f;
    }

    // The automatic turn rate; 2.0420351 is 0.65 pi.
    if (fp.auto_rot == 0.0f)
    {
        if (fp.auto_rot_vel > 0.0f)
        {
            const float v = (float)((double)fp.auto_rot_vel - ft * 4.0);
            fp.auto_rot_vel = (float)((double)v - 0.0) < 0.0f ? 0.0f : v;
        }
        else if (fp.auto_rot_vel < 0.0f)
        {
            const float v = (float)(ft * 4.0 + (double)fp.auto_rot_vel);
            fp.auto_rot_vel = (float)((double)v - 0.0) >= 0.0f ? 0.0f : v;
        }
    }
    else
    {
        float v = (float)((double)fp.auto_rot * 5.0 * ft + (double)fp.auto_rot_vel);
        if ((float)((double)v - 2.0420351028442383) >= 0.0f)
        {
            v = 2.0420351f;
        }
        if ((float)((double)v - -2.0420351028442383) < 0.0f)
        {
            v = -2.0420351f;
        }
        fp.auto_rot_vel = v;
    }
    const float auto_step = (float)(ft * (double)fp.auto_rot_vel);

    vector target_vel;
    if (target->get_obj_type() == OT_VEHICLE)
    {
        vehicle_get_velocity(target->get_handle(), target_vel);
    }
    else if (target->get_obj_type() == OT_HUMAN)
    {
        human_controller_get_vel((human*)target, target_vel);
    }
    else
    {
        target_vel = target->orient.fvec;
    }

    // The automobile's hill: the camera's pitch follows its direction of travel, damped.
    float vehicle_pitch = 0.0f;
    const free_camera_submodes submode = camera_free_get_submode();
    const bool vehicle_submode = submode == FREE_CAMERA_SUBMODE_VEHICLE_DRIVER || submode == FREE_CAMERA_SUBMODE_VEHICLE_CLOSE;
    if (Hill_tracking_camera && !Birds_eye_cam && vehicle_submode && target->get_sub_type() == CAMERA_FREE_SUB_TYPE_AUTOMOBILE)
    {
        vector travel = target_vel;
        const float speed_squared = (float)((double)target_vel.x * (double)target_vel.x + (double)target_vel.y * (double)target_vel.y + (double)target_vel.z * (double)target_vel.z);
        if (speed_squared < 1.0f)
        {
            travel = target->orient.fvec;
        }
        travel.normalize_safe();
        vector xz_fvec;
        vector xz_rvec;
        camera_get_xz_orient(xz_fvec, xz_rvec);
        float along = (float)((double)xz_fvec.y * (double)travel.y + (double)xz_fvec.x * (double)travel.x + (double)xz_fvec.z * (double)travel.z);
        float up = (float)((double)travel.x * 0.0 + (double)travel.y + (double)travel.z * 0.0);
        if (along < 0.0f)
        {
            along = -along;
            up = -up;
        }
        const float angle = (float)atan2((double)up, (double)along); // x87 __CIatan2
        const float scaled = (float)((double)angle * (double)along);
        vehicle_pitch = (float)((double)scaled * (1.0 - (double)Pitch_damping) + (double)Pitch_damping * (double)fp.last_vehicle_pitch);
        fp.last_vehicle_pitch = vehicle_pitch;
    }

    // The elevation: the player's own while a manual turn holds (always for a walker), else back to the base pitch.
    float goal_elev;
    float elev_rate;
    if (camera_free_snap_holding() || target->get_sub_type() == CAMERA_FREE_SUB_TYPE_WALKER)
    {
        float desired = (float)(((double)fp.user_elev * (double)Elev_vel + (double)fp.pitch_drift_vel) * ft + (double)fp.desired_elev_angle);
        fp.desired_elev_angle = desired;
        const float min_elev = (float)((double)params->min_elev_angle_rad - (double)vehicle_pitch);
        if ((float)((double)desired - (double)min_elev) < 0.0f)
        {
            desired = min_elev;
        }
        const float max_elev = (float)((double)params->max_elev_angle_rad - (double)vehicle_pitch);
        if ((float)((double)desired - (double)max_elev) >= 0.0f)
        {
            desired = max_elev;
        }
        fp.desired_elev_angle = desired;
        *last_min_angle = (float)((double)params->min_elev_angle_rad - (double)vehicle_pitch);
        *last_max_angle = (float)((double)params->max_elev_angle_rad - (double)vehicle_pitch);
        goal_elev = fp.desired_elev_angle;
        if (Camera_free_mouse_control_mode == VCCM_DEFAULT && control_mouse_camera_control(fp.last_h_action, fp.last_v_action) && mouse_just_moved())
        {
            elev_rate = (float)(ft * 4.0);
        }
        else
        {
            elev_rate = (float)(ft * 8.0);
        }
    }
    else
    {
        if (Vehicle_camera_elevation_lock || fp.control_type == CAMERA_FREE_CONTROL_TYPE_TREAD || fp.control_type == CAMERA_FREE_CONTROL_TYPE_TREAD_CAM_REL
            || (target->get_sub_type() == CAMERA_FREE_SUB_TYPE_AUTOMOBILE && vehicle_is_tank((vehicle*)target)))
        {
            elev_rate = 0.0f;
        }
        else
        {
            // Magic number: the double of 0.1f; faster vehicles relax sooner.
            elev_rate = (float)(ft * (double)0.1f * (double)target_vel.mag());
        }
        goal_elev = (float)((double)fp.submode_params->base_pitch - (double)vehicle_pitch);
    }
    if ((float)((double)elev_rate - 0.0) < 0.0f)
    {
        elev_rate = 0.0f;
    }
    if ((float)((double)elev_rate - 1.0) >= 0.0f)
    {
        elev_rate = 1.0f;
    }
    fp.last_elev_angle = (float)(((double)goal_elev - (double)fp.last_elev_angle) * (double)elev_rate + (double)fp.last_elev_angle);
    if (!camera_free_snap_holding())
    {
        fp.desired_elev_angle = fp.last_elev_angle;
    }

    // The swing rate: full while a manual turn holds, then it ramps down.
    if (!camera_free_is_swinging() && !fp.flags.heading_is_bashed && vehicle_submode && target->get_sub_type() != CAMERA_FREE_SUB_TYPE_WALKER)
    {
        float swing_rate;
        if (camera_free_snap_holding())
        {
            swing_rate = 1.0f;
        }
        else if (Vehicle_camera_snap_timestamp.elapsed() && Vehicle_camera_snap_timestamp.time_since() < Vehicle_camera_ramp_time)
        {
            const float t = (float)((double)(float)Vehicle_camera_snap_timestamp.time_since() / (double)(float)Vehicle_camera_ramp_time);
            swing_rate = (float)(1.0 - (double)t);
        }
        else
        {
            swing_rate = 0.0f;
        }
        Rfg_camera.free_params.camera_swing_rate = swing_rate;
    }

    // The direction the camera wants to look from, by control type.
    vector desired_dir;
    if (fp.control_type == CAMERA_FREE_CONTROL_TYPE_TREAD_CAM_REL || (target->get_sub_type() == CAMERA_FREE_SUB_TYPE_AUTOMOBILE && vehicle_is_tank((vehicle*)target)))
    {
        desired_dir = -camera->real_orient.fvec;
    }
    else if (fp.control_type == CAMERA_FREE_CONTROL_TYPE_NORMAL || fp.control_type == CAMERA_FREE_CONTROL_TYPE_TREAD)
    {
        const vector real_fvec = camera->real_orient.fvec.copy_flatten_normalize_safe();
        const vector target_fvec = target->orient.fvec.copy_flatten_normalize_safe();
        float target_turn = 0.0f;
        if (!Vehicle_camera_lock_world_space && camera_free_snap_holding())
        {
            Vehicle_camera_last_target_fvec.flatten_normalize_safe();
            target_turn = vec_delta_ang(Vehicle_camera_last_target_fvec, target_fvec, UP_VECTOR);
        }
        Vehicle_camera_last_target_fvec = target->orient.fvec;
        const float delta = vec_delta_ang(target_fvec, real_fvec, UP_VECTOR);
        const float moved = camera_free_dist_to(target->pos, camera->last_target_pos);
        float step = (float)((double)moved * ((1.0 - (double)fp.camera_swing_rate) * (double)Vehicle_camera_normal_rotation_rate * (double)delta));
        if ((double)(float)fabs((double)step) > (double)(float)fabs((double)delta))
        {
            step = delta;
        }
        const matrix turn(0.0f, 0.0f, (float)((double)step - (double)fp.camera_swing_rate * (double)target_turn));
        desired_dir = -real_fvec.copy_rotate(turn);
    }
    else if (fp.control_type == CAMERA_FREE_CONTROL_TYPE_PREDICTIVE)
    {
        const vector real_fvec = camera->real_orient.fvec.copy_flatten_normalize_safe();
        const vector target_fvec = target->orient.fvec.copy_flatten_normalize_safe();
        float delta = vec_delta_ang(target_fvec, real_fvec, UP_VECTOR);
        if (target->get_sub_type() == CAMERA_FREE_SUB_TYPE_AUTOMOBILE)
        {
            const vehicle_camera_settings& settings = camera_free_get_vehicle_camera_settings();
            const float steer = automobile_get_steering_t((automobile*)target) * settings.camera_steer_angle;
            float goal_steer = steer;
            if (Vehicle_camera_use_alt)
            {
                goal_steer = (float)((double)steer * (double)Vehicle_camera_steer_angle_mult);
            }
            if (fp.flags.look_back)
            {
                // Magic number: the double of pi as a float.
                delta = (double)delta > 0.0 ? (float)((double)delta - (double)3.1415927f) : (float)((double)delta + (double)3.1415927f);
            }
            float speed_mult = 1.0f;
            if (Vehicle_camera_use_alt)
            {
                const float s = (float)((double)goal_steer - 0.0);
                if (!(s < 0.001f && -s < 0.001f))
                {
                    speed_mult = Vehicle_camera_steer_adjust_speed_mult;
                }
            }
            if ((double)Vehicle_small_angle > (double)(float)fabs((double)goal_steer))
            {
                goal_steer = 0.0f;
            }
            const double steer_step = (double)settings.camera_steer_speed * (double)speed_mult * ft;
            if ((double)goal_steer > (double)fp.steering_angle)
            {
                fp.steering_angle = (float)(steer_step + (double)fp.steering_angle);
                if ((double)fp.steering_angle > (double)goal_steer)
                {
                    fp.steering_angle = goal_steer;
                }
            }
            else
            {
                fp.steering_angle = (float)((double)fp.steering_angle - steer_step);
                if ((double)goal_steer > (double)fp.steering_angle)
                {
                    fp.steering_angle = goal_steer;
                }
            }
            delta = (float)((double)delta - (double)fp.steering_angle);
        }
        vector moved;
        moved.x = (float)((double)target->pos.x - (double)camera->last_target_pos.x);
        moved.y = (float)((double)target->pos.y - (double)camera->last_target_pos.y);
        moved.z = (float)((double)target->pos.z - (double)camera->last_target_pos.z);
        const float predict = moved.mag() * Vehicle_predictive_adjust;
        float step = (float)((1.0 - (double)fp.camera_swing_rate) * (double)Vehicle_camera_recenter_rate * (double)delta);
        if (Camera_free_mouse_control_mode == VCCM_ALTERNATE1 && camera_free_mouse_steering())
        {
            step = delta;
        }
        step = (float)((double)step * (double)predict); // Original: tests the control type again, always predictive here
        if ((double)(float)fabs((double)step) > (double)(float)fabs((double)delta))
        {
            step = delta;
        }
        const matrix turn(0.0f, 0.0f, step);
        desired_dir = -real_fvec.copy_rotate(turn);
    }
    else
    {
        // Follows from behind, turned toward the direction of travel. Original: no control type reaches this; the four
        // above cover Camera_free_control_types. The console commands only accept those.
        const float dist = params->dist_from_lookat;
        vector offset;
        offset.x = (float)((double)camera->real_orient.fvec.x * (double)dist);
        offset.y = (float)((double)camera->real_orient.fvec.y * (double)dist);
        offset.z = (float)((double)camera->real_orient.fvec.z * (double)dist);
        vector to_lookat;
        to_lookat.x = (float)((double)lookat_pos->x - (double)camera->real_pos.x);
        to_lookat.y = (float)((double)lookat_pos->y - (double)camera->real_pos.y);
        to_lookat.z = (float)((double)lookat_pos->z - (double)camera->real_pos.z);
        float rate = fp.camera_swing_rate;
        if ((float)((double)rate - 0.0) < 0.0f)
        {
            rate = 0.0f;
        }
        if ((float)((double)rate - 1.0) >= 0.0f)
        {
            rate = 1.0f;
        }
        const float speed = target_vel.mag();
        if (speed > 0.01f)
        {
            const float side = target_vel.copy_normalize() * to_lookat.copy_normalize() + 1.0f;
            if (side < 0.5f)
            {
                // Magic numbers: 0.05 and 0.5 are the doubles of those floats, as the binary has them.
                float turn_angle = (float)((double)Frametime * (double)0.05f * (double)speed * ((double)side - 0.5));
                if (side > 0.001f)
                {
                    const float cross = (float)((double)target_vel.x * (double)to_lookat.z - (double)target_vel.z * (double)to_lookat.x);
                    if (cross < 0.0f)
                    {
                        turn_angle = (float)((double)turn_angle * -1.0);
                    }
                }
                const matrix turn(0.0f, 0.0f, turn_angle);
                to_lookat.rotate(turn);
            }
        }
        if ((double)Vehicle_camera_reverse_speed_max > (double)speed && target_vel * target->orient.fvec < 0.0f)
        {
            // Reversing: the follow rate falls off with speed.
            if ((double)Vehicle_camera_reverse_speed_min > (double)speed)
            {
                rate = 1.0f;
            }
            else
            {
                rate = (float)(1.0 - ((double)speed - (double)Vehicle_camera_reverse_speed_min) * (1.0 - (double)rate) / ((double)Vehicle_camera_reverse_speed_max - (double)Vehicle_camera_reverse_speed_min));
            }
        }
        // Magic number: the double of 0.0033333334f, a 300 Hz step.
        const float keep = (float)pow((double)(float)(1.0 - (double)rate), (double)(float)(ft / (double)0.0033333334f)); // x87 __CIpow
        const float follow = (float)(1.0 - (double)keep);
        const float follow_less_one = (float)((double)follow - 1.0);
        desired_dir.x = (float)((double)(float)((double)to_lookat.x * (double)follow_less_one) - (double)(float)((double)follow * (double)offset.x));
        desired_dir.y = (float)((double)(float)((double)to_lookat.y * (double)follow_less_one) - (double)(float)((double)follow * (double)offset.y));
        desired_dir.z = (float)((double)(float)((double)to_lookat.z * (double)follow_less_one) - (double)(float)((double)follow * (double)offset.z));
    }

    // A bashed heading overrides that direction.
    matrix desired_orient;
    if (!fp.flags.heading_is_bashed)
    {
        desired_orient.make_uvec(desired_dir, UP_VECTOR);
    }
    else if (fp.bash_heading_percent < 1.0f)
    {
        vector bash_dir;
        bash_dir.x = (float)((double)fp.bash_heading_start_dir.x + (double)(float)((double)(float)((double)fp.bash_heading_dir.x - (double)fp.bash_heading_start_dir.x) * (double)fp.bash_heading_percent));
        bash_dir.y = (float)((double)fp.bash_heading_start_dir.y + (double)(float)((double)(float)((double)fp.bash_heading_dir.y - (double)fp.bash_heading_start_dir.y) * (double)fp.bash_heading_percent));
        bash_dir.z = (float)((double)fp.bash_heading_start_dir.z + (double)(float)((double)(float)((double)fp.bash_heading_dir.z - (double)fp.bash_heading_start_dir.z) * (double)fp.bash_heading_percent));
        desired_orient.make_uvec(bash_dir, UP_VECTOR);
        float pct = (float)(ft / (double)fp.bash_heading_time + (double)fp.bash_heading_percent);
        if ((float)((double)pct - 0.0) < 0.0f)
        {
            pct = 0.0f;
        }
        if ((float)((double)pct - 1.0) >= 0.0f)
        {
            pct = 1.0f;
        }
        fp.bash_heading_percent = pct;
    }
    else
    {
        desired_orient.make_uvec(fp.bash_heading_dir, UP_VECTOR);
    }
    float pitch;
    float bank;
    float heading;
    desired_orient.extract_angles(pitch, bank, heading);

    // A swing to a direction runs until the player turns the camera.
    float swing_offset = 0.0f;
    if (fp.flags.swinging_to_dir)
    {
        const double small_value = (double)0.0001f; // Magic number: the double of 0.0001f
        const float rot_test = (float)((double)rot - 0.0);
        const float auto_test = (float)((double)auto_step - 0.0);
        if (small_value > (double)rot_test && small_value > (double)-rot_test && small_value > (double)auto_test && small_value > (double)-auto_test)
        {
            const float time_test = (float)((double)fp.swing_time - 0.0);
            if (small_value > (double)time_test && small_value > (double)-time_test)
            {
                fp.swing_percent = 1.0f;
            }
            else
            {
                fp.swing_percent = (float)(ft / (double)fp.swing_time + (double)fp.swing_percent);
            }
            if (fp.swing_percent >= 1.0f)
            {
                fp.flags.swinging_to_dir = 0;
                fp.swing_percent = 1.0f;
            }
            const float t = fl_parametric_accel_decel(fp.swing_percent, 2);
            const float swung = (float)(((double)fp.swing_goal_heading - (double)fp.swing_start_heading) * (double)t + (double)fp.swing_start_heading);
            swing_offset = (float)((double)swung - (double)heading);
        }
        else
        {
            fp.flags.swinging_to_dir = 0;
        }
    }

    // Leaving a flamer walker, the camera has to look along the walker's rear so the player sees where they get out.
    vehicle* target_vehicle = target->get_obj_type() == OT_VEHICLE ? (vehicle*)target : nullptr;
    player* local_player = player_get_local_player();
    float final_rot = rot;
    if (local_player != nullptr && target_vehicle != nullptr && target_vehicle->info->flags.is_walker_flamer && human_in_vehicle_transition(local_player, target_vehicle, VI_INTERACTION_EXIT))
    {
        vector rear = -target_vehicle->orient.fvec;
        rear.normalize_safe();
        float rear_heading = fl_atan2_safe(rear.x, rear.z);
        const float cur_heading = (float)((double)heading + (double)auto_step + (double)swing_offset + 0.0);
        // Magic numbers: the doubles of pi and 2 pi as floats.
        const double pi = (double)3.1415927f;
        const double two_pi = (double)6.2831855f;
        if (rear_heading < 0.0f)
        {
            rear_heading = (float)((double)rear_heading + two_pi);
        }
        float diff = (float)((double)rear_heading - (double)cur_heading);
        if ((double)diff > pi)
        {
            diff = (float)((double)diff - two_pi);
        }
        else if (-pi > (double)diff)
        {
            diff = (float)((double)diff + two_pi);
        }
        const float abs_diff = (float)fabs((double)diff);
        if (fl_less_than_robust(abs_diff, Camera_exit_view_angle_tolerance, 0.001f)) // inlined in the binary
        {
            final_rot = diff < -0.001f ? Camera_exit_view_adjust_rate : -Camera_exit_view_adjust_rate;
            fp.user_rot_vel = 0.0f;
        }
        else if (fl_less_than_robust((float)fabs((double)diff - (double)rot), Camera_exit_view_angle_tolerance, 0.001f))
        {
            fp.user_rot_vel = 0.0f;
            final_rot = 0.0f;
        }
    }

    const float final_heading = fp.flags.heading_is_bashed ? heading : (float)((double)heading + (double)final_rot + (double)auto_step + (double)swing_offset + 0.0);
    matrix cam_orient;
    cam_orient.set(-fp.last_elev_angle, bank, final_heading);

    if (Birds_eye_cam)
    {
        // Magic number: 50 m above the look-at point, for the debug command only.
        Rfg_camera.ideal_pos.x = (float)((double)lookat_pos->x + (double)(float)((double)UP_VECTOR.x * 50.0));
        Rfg_camera.ideal_pos.y = (float)((double)lookat_pos->y + (double)(float)((double)UP_VECTOR.y * 50.0));
        Rfg_camera.ideal_pos.z = (float)((double)lookat_pos->z + (double)(float)((double)UP_VECTOR.z * 50.0));
    }
    else
    {
        handle_camera_obstruction(camera, &cam_orient, lookat_pos);
        const float rate_in = camera_free_snap_holding() ? Camera_collision_percent_rate_in_manual : Camera_collision_percent_rate_in;
        if ((double)fp.goal_dist_from_lookat > (double)fp.cur_dist_from_lookat)
        {
            fp.cur_dist_from_lookat = (float)(((double)fp.goal_dist_from_lookat - (double)fp.cur_dist_from_lookat) * (double)Camera_collision_percent_rate_out * (double)Frametime + (double)fp.cur_dist_from_lookat);
            if ((double)fp.cur_dist_from_lookat > (double)fp.goal_dist_from_lookat)
            {
                fp.cur_dist_from_lookat = fp.goal_dist_from_lookat;
            }
        }
        else if ((double)fp.cur_dist_from_lookat > (double)fp.goal_dist_from_lookat)
        {
            fp.cur_dist_from_lookat = (float)(((double)fp.goal_dist_from_lookat - (double)fp.cur_dist_from_lookat) * (double)rate_in * (double)Frametime + (double)fp.cur_dist_from_lookat);
            if ((double)fp.goal_dist_from_lookat > (double)fp.cur_dist_from_lookat)
            {
                fp.cur_dist_from_lookat = fp.goal_dist_from_lookat;
            }
        }
        const double dist = (double)fp.cur_dist_from_lookat;
        camera->ideal_pos.x = (float)((double)lookat_pos->x + (double)(float)((double)cam_orient.fvec.x * dist));
        camera->ideal_pos.y = (float)((double)lookat_pos->y + (double)(float)((double)cam_orient.fvec.y * dist));
        camera->ideal_pos.z = (float)((double)lookat_pos->z + (double)(float)((double)cam_orient.fvec.z * dist));
    }

    vector to_lookat;
    to_lookat.x = (float)((double)lookat_pos->x - (double)camera->ideal_pos.x);
    to_lookat.y = (float)((double)lookat_pos->y - (double)camera->ideal_pos.y);
    to_lookat.z = (float)((double)lookat_pos->z - (double)camera->ideal_pos.z);
    vector up_dir = UP_VECTOR;
    automobile* ap = target->get_obj_type() == OT_VEHICLE && target->get_sub_type() == CAMERA_FREE_SUB_TYPE_AUTOMOBILE ? (automobile*)target : nullptr;

    // The view rises and falls with the chassis at low speed. Off: nothing writes the flag.
    if (Vehicle_camera_g_forces_chassis && ap != nullptr)
    {
        float speed = 0.0f;
        vehicle_get_speed(ap, speed);
        const double max_change = (double)Camera_chassis_follow_max_change;
        float adjust;
        if (speed >= 0.0f && (double)Camera_max_follow_chassis_speed > (double)speed)
        {
            vector body_pos;
            matrix body_orient;
            havok_body_get_pos_orient(ap->havok_handle, body_pos, body_orient);
            adjust = (float)((double)body_orient.fvec.y * (double)Camera_chassis_follow_multiplier);
            const float change = (float)((double)Acceleration_adjust - (double)adjust);
            if ((double)change > max_change)
            {
                adjust = (float)((double)Acceleration_adjust - max_change);
            }
            else if (-max_change > (double)change)
            {
                adjust = (float)(max_change + (double)Acceleration_adjust);
            }
        }
        else
        {
            // Original: a positive adjust relaxes here, a negative one stays.
            adjust = Acceleration_adjust;
            if (adjust > 0.0f)
            {
                adjust = (float)((double)adjust - max_change);
            }
        }
        if ((float)((double)adjust - -1.0) < 0.0f)
        {
            adjust = -1.0f;
        }
        if ((float)((double)adjust - 1.0) >= 0.0f)
        {
            adjust = 1.0f;
        }
        Acceleration_adjust = adjust;
        to_lookat.y = (float)((double)to_lookat.y + (double)adjust);
    }

    // The view rolls with a sideways slide.
    if (Vehicle_camera_g_forces_roll && ap != nullptr)
    {
        float speed = 0.0f;
        vehicle_get_speed(ap, speed);
        vector side = ap->orient.rvec;
        vector velocity = ZERO_VECTOR;
        side.normalize_safe();
        vehicle_get_velocity(ap->get_handle(), velocity);
        const float sideways = (float)((double)velocity.x * (double)side.x + (double)velocity.y * (double)side.y + (double)velocity.z * (double)side.z);
        const double min_sideways = (double)Vehicle_camera_roll_min_sideways;
        const double max_change = (double)Vehicle_camera_roll_max_change;
        if ((double)speed > (double)Vehicle_camera_roll_min_speed && ((double)sideways > min_sideways || (double)-Vehicle_camera_roll_min_sideways > (double)sideways))
        {
            const float amount = (float)(((double)(float)fabs((double)sideways) - min_sideways) / ((double)Vehicle_camera_roll_max_sideways - min_sideways));
            float scaled = (float)((double)amount * (sideways < 0.0f ? -1.0 : 1.0));
            if ((float)((double)scaled - -1.0) < 0.0f)
            {
                scaled = -1.0f;
            }
            if ((float)((double)scaled - 1.0) >= 0.0f)
            {
                scaled = 1.0f;
            }
            float roll = (float)((double)scaled * (double)Vehicle_camera_roll_max_angle);
            const float change = (float)((double)Vehicle_camera_roll - (double)roll);
            if ((double)change > max_change)
            {
                roll = (float)((double)Vehicle_camera_roll - max_change);
            }
            else if (-max_change > (double)change)
            {
                roll = (float)(max_change + (double)Vehicle_camera_roll);
            }
            Vehicle_camera_roll = roll;
        }
        else if (Vehicle_camera_roll > 0.0f)
        {
            // Magic numbers: the doubles of 1.4f and 0.7f (of the largest change), and 0.5 to halve small rolls.
            float roll = (double)Vehicle_camera_roll > max_change * (double)1.4f ? (float)((double)Vehicle_camera_roll - max_change * (double)0.7f) : (float)((double)Vehicle_camera_roll * 0.5);
            if ((float)((double)roll - 0.0) < 0.0f)
            {
                roll = 0.0f;
            }
            if ((float)((double)roll - 1.0) >= 0.0f)
            {
                roll = 1.0f;
            }
            Vehicle_camera_roll = roll;
        }
        else if (Vehicle_camera_roll < 0.0f)
        {
            float roll = (double)-Vehicle_camera_roll > max_change * (double)1.4f ? (float)(max_change * (double)0.7f + (double)Vehicle_camera_roll) : (float)((double)Vehicle_camera_roll * 0.5);
            if ((float)((double)roll - -1.0) < 0.0f)
            {
                roll = -1.0f;
            }
            if ((float)((double)roll - 0.0) >= 0.0f)
            {
                roll = 0.0f;
            }
            Vehicle_camera_roll = roll;
        }
        up_dir.rotate_around_vector(ap->orient.fvec, Vehicle_camera_roll);
    }
    camera->ideal_orient.make_uvec(to_lookat, up_dir);
}

// Takes the last frame's gyro or alternate mouse offset back off the camera, before the step works from it.
static void camera_free_remove_offset(float heading_offset, float pitch_offset, const vector& pos_offset)
{
    matrix m;
    m.make_ry(-heading_offset);
    Rfg_camera.ideal_orient.rotate(m);
    Rfg_camera.real_orient.rotate(m);
    Rfg_camera.ideal_orient.rotate_around_local_x(-pitch_offset);
    Rfg_camera.real_orient.rotate_around_local_x(-pitch_offset);
    Rfg_camera.ideal_pos.x = (float)((double)Rfg_camera.ideal_pos.x - (double)pos_offset.x);
    Rfg_camera.ideal_pos.y = (float)((double)Rfg_camera.ideal_pos.y - (double)pos_offset.y);
    Rfg_camera.ideal_pos.z = (float)((double)Rfg_camera.ideal_pos.z - (double)pos_offset.z);
}

// Orbits the ideal camera around the look-at point by a gyro or alternate mouse offset, and returns how far it moved.
static vector camera_free_apply_offset(float heading_offset, float pitch_offset)
{
    const vector start = Rfg_camera.ideal_pos;
    vector to_lookat;
    to_lookat.x = (float)((double)Rfg_camera.free_params.last_lookat_pos.x - (double)start.x);
    to_lookat.y = (float)((double)Rfg_camera.free_params.last_lookat_pos.y - (double)start.y);
    to_lookat.z = (float)((double)Rfg_camera.free_params.last_lookat_pos.z - (double)start.z);
    vector turned = to_lookat;
    matrix m;
    m.make_ry(heading_offset);
    turned.rotate_around_y_axis(heading_offset);
    Rfg_camera.ideal_orient.rotate(m);
    turned.rotate_around_vector(Rfg_camera.ideal_orient.rvec, pitch_offset);
    Rfg_camera.ideal_orient.rotate_around_local_x(pitch_offset);
    Rfg_camera.ideal_pos.x = (float)((double)(float)((double)to_lookat.x + (double)start.x) - (double)turned.x);
    Rfg_camera.ideal_pos.y = (float)((double)(float)((double)to_lookat.y + (double)start.y) - (double)turned.y);
    Rfg_camera.ideal_pos.z = (float)((double)(float)((double)to_lookat.z + (double)start.z) - (double)turned.z);
    vector moved;
    moved.x = (float)((double)Rfg_camera.ideal_pos.x - (double)start.x);
    moved.y = (float)((double)Rfg_camera.ideal_pos.y - (double)start.y);
    moved.z = (float)((double)Rfg_camera.ideal_pos.z - (double)start.z);
    return moved;
}

// 0x006D9780 (:1630)
// The mode's frame: the vehicle camera. On foot the player uses the third person mode instead.
// Original: with last_step_of_frame it reads through a null target, which only the early return below rules out in the
// spectator and dedicated server cases.
void camera_free_process(float step_time, bool last_step_of_frame)
{
    free_mode_params& fp = Rfg_camera.free_params;
    object* target = World.object_from_handle(Rfg_camera.target_handle);
    if (target == nullptr && ((Multiplayer && multi_is_dedicated_server()) || multi_game_spectating()))
    {
        return;
    }
    if (input_get_gyro_enabled(GYRO_MODE_VEHICLE))
    {
        camera_free_remove_offset(fp.last_gyro_heading_offset, fp.last_gyro_pitch_offset, fp.last_gyro_offset);
    }
    if (Camera_free_mouse_control_mode == VCCM_ALTERNATE1 && camera_free_mouse_steering())
    {
        camera_free_remove_offset(fp.last_mouse_heading_offset, fp.last_mouse_pitch_offset, fp.last_mouse_offset);
    }
    if (target != nullptr && target->get_obj_type() == OT_VEHICLE)
    {
        // Magic number: walkers and tanks keep the default 45 degrees.
        if (target->get_sub_type() == CAMERA_FREE_SUB_TYPE_WALKER || vehicle_is_tank((vehicle*)target))
        {
            fp.submode_params->free_fov = 45.0f;
        }
        else
        {
            fp.submode_params->free_fov = Camera_vehicle_fov;
        }
    }
    Rfg_camera.ideal_fov = fp.submode_params->free_fov;
    float pitch;
    Rfg_camera.real_orient.extract_pitch(pitch);
    const vector lookat = camera_free_get_lookat(pitch); // inlined in the binary

    float lag = fp.flags.suppress_target_lag ? (float)((double)fp.suppressed_target_lag_pct + (double)step_time) : (float)((double)fp.suppressed_target_lag_pct - (double)step_time);
    if ((float)((double)lag - 0.0) < 0.0f)
    {
        lag = 0.0f;
    }
    if ((float)((double)lag - 1.0) >= 0.0f)
    {
        lag = 1.0f;
    }
    fp.suppressed_target_lag_pct = lag;

    float min_angle = 0.0f;
    float max_angle = 0.0f;
    do_camera_movement(&Rfg_camera, &lookat, step_time, &min_angle, &max_angle);
    if (last_step_of_frame)
    {
        fp.last_lookat_pos = lookat;
        const float dx = (float)((double)target->pos.x - (double)Rfg_camera.last_target_pos.x);
        const float dy = (float)((double)target->pos.y - (double)Rfg_camera.last_target_pos.y);
        const float dz = (float)((double)target->pos.z - (double)Rfg_camera.last_target_pos.z);
        const float moved_squared = (float)((double)dx * (double)dx + (double)dy * (double)dy + (double)dz * (double)dz);
        fp.flags.target_moved_last_frame = (double)moved_squared > (double)0.0001f ? 1 : 0;
        fp.flags.suppress_target_lag = 0;
        fp.flags.unresolvable_obstruction = 0;
        fp.auto_rot = 0.0f;
        fp.user_rot = 0.0f;
        fp.user_elev = 0.0f;
    }
    if (input_get_gyro_enabled(GYRO_MODE_VEHICLE))
    {
        // Magic numbers: 5 and 2.5 radians at full gyro input.
        fp.last_gyro_heading_offset = (float)((double)input_gyro_get_camera_orientation_value(GYRO_MODE_VEHICLE, GYRO_CAMERA_AXIS_X, Player_controller_index) * 5.0);
        float gyro_pitch = (float)((double)input_gyro_get_camera_orientation_value(GYRO_MODE_VEHICLE, GYRO_CAMERA_AXIS_Y, Player_controller_index) * 2.5);
        if (min_angle > gyro_pitch)
        {
            gyro_pitch = min_angle;
        }
        else if ((double)gyro_pitch > (double)max_angle)
        {
            gyro_pitch = max_angle;
        }
        fp.last_gyro_pitch_offset = gyro_pitch;
        // Deviation from rfg.exe: the pitch input is read before the heading is applied, as camera_free_apply_offset
        // takes both. The asm reads it between them.
        fp.last_gyro_offset = camera_free_apply_offset(fp.last_gyro_heading_offset, fp.last_gyro_pitch_offset);
    }
    if (Camera_free_mouse_control_mode == VCCM_ALTERNATE1 && camera_free_mouse_steering())
    {
        fp.last_mouse_heading_offset = fp.mouse_absolute_pos.x;
        float mouse_pitch = fp.mouse_absolute_pos.y;
        if ((double)min_angle > (double)mouse_pitch)
        {
            mouse_pitch = min_angle;
        }
        else if ((double)mouse_pitch > (double)max_angle)
        {
            mouse_pitch = max_angle;
        }
        fp.last_mouse_pitch_offset = mouse_pitch;
        fp.mouse_absolute_pos.y = mouse_pitch;
        fp.last_mouse_offset = camera_free_apply_offset(fp.last_mouse_heading_offset, fp.last_mouse_pitch_offset);
    }
}

// 0x006D5BA0 (:2059)
void camera_free_set_target(object* objp)
{
    if (objp == nullptr)
    {
        Rfg_camera.last_target_pos = ZERO_VECTOR;
        Rfg_camera.last_target_orient = IDENTITY_MATRIX;
        Rfg_camera.target_handle = 0xFFFFFFFF;
    }
    else
    {
        Rfg_camera.target_handle = objp->get_handle();
        Rfg_camera.last_target_pos = objp->pos;
        Rfg_camera.last_target_orient = objp->orient;
    }
    Rfg_camera.free_params.steering_angle = 0.0f;
    if (Rfg_camera.free_params.submode_params != nullptr)
    {
        Rfg_camera.free_params.submode_params->vehicle_camera_selection = 0;
    }
    camera_free_update_vehicle_camera_settings();
}

// 0x006C4670 (:2086)
void camera_free_suppress_orientation_lag()
{
    Rfg_camera.free_params.flags.suppress_target_lag = 1;
}

// 0x006CB3B0 (:2097)
void camera_free_set_submode(free_camera_submodes submode, float smooth, float transition_time)
{
    switch (submode)
    {
    case FREE_CAMERA_SUBMODE_INTERIOR:
    case FREE_CAMERA_SUBMODE_INTERIOR_CLOSE:
        Rfg_camera.free_params.submode_params = Rfg_camera.free_params.cur_interior_params;
        break;
    case FREE_CAMERA_SUBMODE_EXTERIOR:
    case FREE_CAMERA_SUBMODE_EXTERIOR_STAND:
    case FREE_CAMERA_SUBMODE_VEHICLE_PASSENGER:
        Rfg_camera.free_params.submode_params = Rfg_camera.free_params.cur_exterior_params;
        break;
    case FREE_CAMERA_SUBMODE_VEHICLE_DRIVER:
    case FREE_CAMERA_SUBMODE_VEHICLE_CLOSE:
    case FREE_CAMERA_SUBMODE_VEHICLE_EXTERIOR:
        Rfg_camera.free_params.submode_params = Rfg_camera.free_params.cur_vehicle_params;
        break;
    default:
        Rfg_camera.free_params.submode_params = &Free_camera_submode_params[submode];
        break;
    }
    if (smooth != 0.0f)
    {
        camera_trans_start_linear_transition(transition_time, 7, true); // inlined in the binary
    }
}

// 0x006C4680 (:2128)
free_camera_submodes camera_free_get_submode()
{
    return (free_camera_submodes)(Rfg_camera.free_params.submode_params - Free_camera_submode_params);
}

// 0x006C46A0 (:2139)
// The vehicle submodes (close, and the driver one twice as far) from the vehicle's camera settings.
void camera_free_set_vehicle_submode_params(float follow_distance, float follow_height, const vector& lookat_offset)
{
    free_submode_params& close = Free_camera_submode_params[FREE_CAMERA_SUBMODE_VEHICLE_CLOSE];
    close.y_dist = follow_height;
    close.xz_dist = follow_distance;
    close.base_pitch = (float)atan((double)(float)(follow_height / follow_distance));
    close.dist_from_lookat = (float)sqrt((double)(float)((double)follow_distance * (double)follow_distance + (double)follow_height * (double)follow_height));
    close.lookat_offset_x = lookat_offset.x;
    close.lookat_offset_y = lookat_offset.y;
    close.lookat_offset_z = lookat_offset.z;
    free_submode_params& driver = Free_camera_submode_params[FREE_CAMERA_SUBMODE_VEHICLE_DRIVER];
    driver.y_dist = Vehicle_mode_y_multiplier * follow_height;
    driver.xz_dist = Vehicle_mode_xz_multiplier * follow_distance;
    driver.base_pitch = (float)atan((double)(float)(driver.y_dist / driver.xz_dist));
    driver.dist_from_lookat = (float)sqrt((double)(float)((double)driver.xz_dist * (double)driver.xz_dist + (double)driver.y_dist * (double)driver.y_dist));
    driver.lookat_offset_x = lookat_offset.x;
    driver.lookat_offset_y = lookat_offset.y;
    driver.lookat_offset_z = lookat_offset.z;
}

// 0x006C4810 (:2170)
// In the free mode: the heading swings from the camera's to the direction's over the time, the short way round.
// Both headings are of the reversed vectors (the camera looks back along them).
void camera_free_swing_to_dir(const vector& to_target, float swing_time_sec)
{
    if (Rfg_camera.mode != CAMERA_FREE_MODE)
    {
        return;
    }
    free_mode_params& fp = Rfg_camera.free_params;
    matrix m;
    m.make_uvec(-Rfg_camera.real_orient.fvec, UP_VECTOR);
    m.extract_heading(fp.swing_start_heading);
    // Magic numbers: pi and 2 pi (float constants).
    fp.swing_start_heading = fl_angle_cap_radians(fp.swing_start_heading, -3.1415927f);
    m.make_uvec(-to_target, UP_VECTOR);
    m.extract_heading(fp.swing_goal_heading);
    fp.swing_goal_heading = fl_angle_cap_radians(fp.swing_goal_heading, -3.1415927f);
    fp.flags.swinging_to_dir = 1;
    fp.swing_percent = 0.0f;
    fp.swing_time = swing_time_sec;
    float other;
    if (fp.swing_start_heading >= 0.0f)
    {
        other = (float)((double)fp.swing_start_heading - (double)6.2831855f);
    }
    else
    {
        other = (float)((double)fp.swing_start_heading + (double)6.2831855f);
    }
    if (fabs((double)fp.swing_goal_heading - (double)other) < fabs((double)fp.swing_goal_heading - (double)fp.swing_start_heading))
    {
        fp.swing_start_heading = other;
    }
}

// 0x006C49F0 (:2215)
// A swing under way moves its goal, with the start worked back so the camera stays where it is.
// Original: uses the bash percent where the swing's own is meant (my reading: nothing else makes the swing's t here).
void camera_free_update_swing_to_dir(const vector& to_target)
{
    if (Rfg_camera.mode != CAMERA_FREE_MODE || !Rfg_camera.free_params.flags.swinging_to_dir)
    {
        return;
    }
    free_mode_params& fp = Rfg_camera.free_params;
    matrix m;
    m.make_uvec(-to_target, UP_VECTOR);
    m.extract_heading(fp.swing_goal_heading);
    fp.swing_goal_heading = fl_angle_cap_radians(fp.swing_goal_heading, -3.1415927f);
    const float t = fl_parametric_accel_decel(fp.bash_heading_percent, 2);
    m.make_uvec(-Rfg_camera.real_orient.fvec, UP_VECTOR);
    float cur_heading;
    m.extract_heading(cur_heading);
    cur_heading = fl_angle_cap_radians(cur_heading, -3.1415927f);
    fp.swing_start_heading = (float)(((double)cur_heading - (double)fp.swing_goal_heading * (double)t) / (1.0 - (double)t));
    // Magic number: the double of 2 pi as a float.
    const float other = fp.swing_start_heading >= 0.0f ? (float)((double)fp.swing_start_heading - (double)6.2831855f) : (float)((double)fp.swing_start_heading + (double)6.2831855f);
    if ((double)(float)fabs((double)fp.swing_goal_heading - (double)other) < (double)(float)fabs((double)fp.swing_goal_heading - (double)fp.swing_start_heading))
    {
        fp.swing_start_heading = other;
    }
}

// 0x006C4C10 (:2264)
bool camera_free_is_swinging()
{
    return Rfg_camera.mode == CAMERA_FREE_MODE && Rfg_camera.free_params.flags.swinging_to_dir != 0;
}

// 0x006C4C30 (:2271)
void camera_free_set_swing_rate(float csr)
{
    Rfg_camera.free_params.camera_swing_rate = csr;
}

// 0x006C4C40 (:2280)
void camera_free_reset_pitch()
{
    if (Birds_eye_cam)
    {
        Rfg_camera.free_params.desired_elev_angle = Free_camera_submode_params[FREE_CAMERA_SUBMODE_BIRDS_EYE_DEBUG].base_pitch;
        return;
    }
    if (!Vehicle_camera_elevation_lock)
    {
        Rfg_camera.free_params.desired_elev_angle = Rfg_camera.free_params.submode_params->base_pitch;
    }
}

// 0x006C4C80 (:2293)
// Forces the heading toward a direction; the first call starts from where the camera looks.
void camera_free_bash_heading(const vector& to_target, float time_sec)
{
    free_mode_params& fp = Rfg_camera.free_params;
    if (!fp.flags.heading_is_bashed)
    {
        fp.bash_heading_start_dir = -Rfg_camera.real_orient.fvec;
        fp.bash_heading_percent = time_sec > 1e-06f ? 0.0f : 1.0f; // Magic number: no time means straight there
        fp.flags.heading_is_bashed = 1;
        fp.flags.swinging_to_dir = 0;
        fp.bash_heading_time = time_sec;
    }
    fp.bash_heading_dir = to_target;
}

// 0x006C4D40 (:2310)
void camera_free_set_snap_time(int snap_time_ms)
{
    Vehicle_camera_snap_timestamp.set(snap_time_ms < 1 ? Vehicle_camera_default_snap_time : snap_time_ms);
    Vehicle_camera_last_target_fvec = World.object_from_handle(Rfg_camera.target_handle)->orient.fvec;
}

// 0x006C4D80 (:2324)
void camera_free_clear_snap_time()
{
    if (Vehicle_camera_snap_timestamp.valid())
    {
        Vehicle_camera_snap_timestamp.set(0);
    }
}

// 0x006C4DA0 (:2336)
void camera_free_stop_bashing_heading(float time_sec, bool swing_to_front)
{
    free_mode_params& fp = Rfg_camera.free_params;
    const object* target = World.object_from_handle(Rfg_camera.target_handle);
    if (fp.flags.heading_is_bashed)
    {
        fp.flags.heading_is_bashed = 0;
        if (swing_to_front)
        {
            camera_free_swing_to_dir(target->orient.fvec, time_sec);
        }
    }
}

// 0x006C4DE0 (:2352)
// Ghidra lists no callers, and nothing writes the flag.
void camera_free_render_debug()
{
    if (Show_camera_lookat)
    {
        grd_set_color(0xFF, 0xFF, 0x40, 0xFF);
        grd_sphere(Rfg_camera.free_params.last_lookat_pos, 0.2f, GM_FILTER_Z);
    }
}

// 0x006CB490 (:2361)
void camera_free_toggle_close()
{
    const free_camera_submodes submode = camera_free_get_submode();
    free_mode_params& fp = Rfg_camera.free_params;
    switch (submode)
    {
    case FREE_CAMERA_SUBMODE_INTERIOR:
    case FREE_CAMERA_SUBMODE_INTERIOR_COMBAT:
    case FREE_CAMERA_SUBMODE_INTERIOR_CLOSE:
        break;
    case FREE_CAMERA_SUBMODE_EXTERIOR:
    case FREE_CAMERA_SUBMODE_EXTERIOR_CROUCH:
    case FREE_CAMERA_SUBMODE_EXTERIOR_STAND:
    case FREE_CAMERA_SUBMODE_VEHICLE_PASSENGER:
        fp.cur_exterior_params = fp.cur_exterior_params == &Free_camera_submode_params[FREE_CAMERA_SUBMODE_EXTERIOR] ? &Free_camera_submode_params[FREE_CAMERA_SUBMODE_EXTERIOR_STAND] : &Free_camera_submode_params[FREE_CAMERA_SUBMODE_EXTERIOR];
        break;
    case FREE_CAMERA_SUBMODE_VEHICLE_DRIVER:
    case FREE_CAMERA_SUBMODE_VEHICLE_CLOSE:
    case FREE_CAMERA_SUBMODE_VEHICLE_EXTERIOR:
        fp.cur_vehicle_params = fp.cur_vehicle_params == &Free_camera_submode_params[FREE_CAMERA_SUBMODE_VEHICLE_DRIVER] ? &Free_camera_submode_params[FREE_CAMERA_SUBMODE_VEHICLE_CLOSE] : &Free_camera_submode_params[FREE_CAMERA_SUBMODE_VEHICLE_DRIVER];
        break;
    default:
        // The binary prints "Toggling and unknown camera submode" through keen's debug print here (debug only).
        break;
    }
    camera_free_set_submode(submode, 1.0f, 0.3f);
}

// 0x006C4E30 (:2407)
void camera_free_recalc_vehicle_submode_params()
{
    const free_submode_params& close = Free_camera_submode_params[FREE_CAMERA_SUBMODE_VEHICLE_CLOSE];
    free_submode_params& driver = Free_camera_submode_params[FREE_CAMERA_SUBMODE_VEHICLE_DRIVER];
    driver.y_dist = Vehicle_mode_y_multiplier * close.y_dist;
    driver.xz_dist = Vehicle_mode_xz_multiplier * close.xz_dist;
    driver.base_pitch = (float)atan((double)(float)(driver.y_dist / driver.xz_dist)); // x87 __CIatan
    driver.dist_from_lookat = (float)sqrt((double)(float)((double)driver.xz_dist * (double)driver.xz_dist + (double)driver.y_dist * (double)driver.y_dist));
    driver.lookat_offset_x = close.lookat_offset_x;
    driver.lookat_offset_y = close.lookat_offset_y;
    driver.lookat_offset_z = close.lookat_offset_z;
}

// 0x006C4F10 (:2422)
// Behind the target at the base pitch, far out (the distance comes back in), the input offsets cleared.
void camera_free_reset()
{
    free_mode_params& fp = Rfg_camera.free_params;
    const vector fvec = World.object_from_handle(Rfg_camera.target_handle)->orient.fvec;
    camera_free_swing_to_dir(fvec, 0.0f);
    fp.desired_elev_angle = fp.submode_params->base_pitch;
    fp.last_elev_angle = fp.submode_params->base_pitch;
    fp.cur_dist_from_lookat = 1000.0f; // Magic number: past any submode's distance
    fp.auto_rot_vel = 0.0f;
    fp.user_rot_vel = 0.0f;
    fp.steering_angle = 0.0f;
    fp.last_gyro_heading_offset = 0.0f;
    fp.last_gyro_pitch_offset = 0.0f;
    fp.last_gyro_offset.zero();
    fp.last_mouse_heading_offset = 0.0f;
    fp.last_mouse_pitch_offset = 0.0f;
    fp.last_mouse_offset.zero();
    fp.mouse_absolute_pos.zero();
}

// 0x006C4FF0 (:2459)
// Narrows every submode's fov down to the minimum; false when a submode was already there.
bool camera_free_modify_fov(float fov_delta)
{
    const float limit = Sniper_zoom_min + fov_delta;
    bool changed = true;
    for (int i = 0; i < NUM_FREE_CAMERA_SUBMODES; i++)
    {
        float& fov = Free_camera_submode_params[i].free_fov;
        if (fov < limit)
        {
            if (fov == Sniper_zoom_min)
            {
                changed = false;
            }
            else
            {
                fov = Sniper_zoom_min;
            }
        }
        else
        {
            fov = fov - fov_delta;
        }
    }
    return changed;
}

// 0x006C5290 (:2490)
void camera_free_reset_fov()
{
    camera_free_set_fov(45.0f); // inlined in the binary
}

// 0x006C52B0 (:2501)
float camera_free_get_fov_ratio()
{
    return Rfg_camera.real_fov / 45.0f;
}

// 0x006C52D0 (:2510)
void camera_free_set_fov(float new_fov)
{
    for (int i = 0; i < NUM_FREE_CAMERA_SUBMODES; i++)
    {
        Free_camera_submode_params[i].free_fov = new_fov;
    }
}

// 0x006D5CA0 (:2519)
// A vehicle with more than one camera setting: the next one.
void camera_free_cycle_camera()
{
    vehicle* vp = camera_free_target_vehicle();
    free_submode_params* params = Rfg_camera.free_params.submode_params;
    if (vp == nullptr || vp->info->camera_settings_count <= 1)
    {
        return;
    }
    params->vehicle_camera_selection++;
    if (vp->info->camera_settings_count <= params->vehicle_camera_selection)
    {
        params->vehicle_camera_selection = 0;
    }
    camera_free_update_vehicle_camera_settings();
}

// 0x006D22F0 (:2536)
// A vehicle target's camera: a turret seat uses a turret camera setting, others a non-turret one.
void camera_free_update_vehicle_camera_settings()
{
    object* objp = World.object_from_handle(Rfg_camera.target_handle);
    free_submode_params* params = Rfg_camera.free_params.submode_params;
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
    {
        return;
    }
    vehicle* vp = camera_free_target_vehicle();
    player* pp = player_get_local_player();
    if (vp->info->camera_settings_count <= params->vehicle_camera_selection)
    {
        params->vehicle_camera_selection = 0;
    }
    const vehicle_seat_index seat = vehicle_passenger_get_seat(vp, pp);
    if (seat == VSI_DRIVER || vehicle_get_seat_turret_mount(vp, seat) == nullptr)
    {
        if (vp->info->camera_settings[params->vehicle_camera_selection].turret_camera)
        {
            for (int i = 0; i < vp->info->camera_settings_count; i++)
            {
                if (!vp->info->camera_settings[i].turret_camera)
                {
                    params->vehicle_camera_selection = i;
                    break;
                }
            }
        }
    }
    else
    {
        for (int i = 0; i < vp->info->camera_settings_count; i++)
        {
            if (vp->info->camera_settings[i].turret_camera)
            {
                params->vehicle_camera_selection = i;
                break;
            }
        }
    }
    Rfg_camera.m_camera_view_data.update_vehicle_fov(vp);
    const vehicle_camera_settings& settings = camera_free_get_vehicle_camera_settings(); // inlined in the binary
    camera_free_set_vehicle_submode_params(settings.follow_dist, settings.follow_height, settings.lookat_offset);
    params->y_axis_rotation_speed = settings.y_axis_rotation_speed;
}

// 0x006CB560 (:2588)
// Original: called with a vehicle target only; another target dereferences null.
const vehicle_camera_settings& camera_free_get_vehicle_camera_settings()
{
    free_submode_params* params = Rfg_camera.free_params.submode_params;
    vehicle* vp = camera_free_target_vehicle();
    if (vp->info->camera_settings_count <= params->vehicle_camera_selection)
    {
        params->vehicle_camera_selection = 0;
    }
    return vp->info->camera_settings[params->vehicle_camera_selection];
}

// 0x006CB5C0 (:2601)
void camera_free_change_vehicle_control_type(Camera_free_control_types control_type)
{
    Vehicle_camera_control_type = control_type;
    const player* pp = player_get_local_player();
    if (pp == nullptr || pp->vehicle_handle == 0xFFFFFFFF)
    {
        return;
    }
    const object* objp = World.object_from_handle(pp->vehicle_handle);
    if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && !vehicle_is_tank((const vehicle*)objp))
    {
        Rfg_camera.free_params.control_type = control_type;
    }
}

// 0x006C52F0 (:2616)
Camera_free_control_types camera_free_get_vehicle_control_type()
{
    return Vehicle_camera_control_type;
}

// 0x006CB610 (:2621)
void camera_free_set_tank_control_type(Camera_free_control_types control_type)
{
    Tank_camera_control_type = control_type;
    const player* pp = player_get_local_player();
    if (pp == nullptr || pp->vehicle_handle == 0xFFFFFFFF)
    {
        return;
    }
    const object* objp = World.object_from_handle(pp->vehicle_handle);
    if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && vehicle_is_tank((const vehicle*)objp))
    {
        Rfg_camera.free_params.control_type = control_type;
    }
}

// 0x006C5300 (:2636)
Camera_free_control_types camera_free_get_tank_control_type()
{
    return Tank_camera_control_type;
}
