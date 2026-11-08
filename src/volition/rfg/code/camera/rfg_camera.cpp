// Original: volition/rfg/code/camera/rfg_camera.cpp
//
// The game camera (player_minimal.md P6): the mode stack and its callbacks, the frame (the mode's pre-process before
// the simulation, the process, the transition, smoothing and shake after it, the render transform last), the default
// position behind the target, and the collision casts the modes use. The render half (main render camera, culling
// frustum) is rfg_camera_render.cpp in rfg_render. Compiled in rfg_camera_unity.cpp after the mode files (it reads
// their statics), with the turret camera (stub_impl.md S5). Not ported: the debug console commands (unused_dcf_*).
// The cutscene and satellite modes are in camera_cutscene.cpp and camera_satellite.cpp, in rfg_gameplay.
// Multiple-op float expressions are computed in double and rounded once where the asm was read (the collision
// callback); elsewhere they follow the decompile.

#include "volition/rfg/code/camera/rfg_camera.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/render_lib/code/rl_utility.h"
#include "volition/rfg/code/camera/camera_death.h"
#include "volition/rfg/code/camera/camera_first_person.h"
#include "volition/rfg/code/camera/camera_satellite.h"
#include "volition/rfg/code/camera/camera_shake.h"
#include "volition/rfg/code/camera/camera_slew.h"
#include "volition/rfg/code/camera/camera_transition.h"
#include "volition/rfg/code/control/control.h"
#include "volition/rfg/code/control/control_spectator.h"
#include "volition/rfg/code/game_render/fullscreen_effects/fb_accum_blur.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_compression.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_spectator.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/ladder.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/input/descent_physics.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/quaternion.h"
#include "volition/vlib/code/os/frametime.h"

#include <math.h>

// The nav cell detours' vehicle info of the Marauder raider (0x035B6080; the nav code is not recovered). The camera
// collides with the vehicle the player leaves only for this vehicle.
extern const vehicle_info* Nav_cell_detour_veh_info_mar_raider_1;

static void camera_turret_process(float step_time, bool last_step_of_frame); // 0x006DEA50 (:662)

rfg_camera Rfg_camera; // 0x01DE4B50

// ---- TU-private types (rfg.pdb; no header names them)
// The callbacks of one camera mode (Camera_mode_callbacks below). camera_system_init calls every mode's
// system_init_callback, camera_level_init_post every level_init_callback; camera_do_frame_pre and camera_do_frame
// call the active mode's pre_process_callback and process_callback.
// size 0x14
struct camera_mode_callbacks
{
    void (*system_init_callback)(); // 0x00
    void (*level_init_callback)(); // 0x04
    void (*mode_init_callback)(); // 0x08
    void (*pre_process_callback)(); // 0x0C
    void (*process_callback)(float, bool); // 0x10
};

// ---- file-static globals
// 0x012CFC80, rfg.exe .rdata (dumped in player_minimal.md P0), indexed by camera_mode. The player minimal scope reaches
// the third person entry; its pre-process slot is the first person one. The free and satellite init callbacks run
// from camera_system_init/camera_level_init_post whatever the mode is.
static const camera_mode_callbacks Camera_mode_callbacks[NUM_CAMERA_MODES] = {
    {camera_free_system_init, camera_free_level_init_post, camera_free_mode_init, nullptr, camera_free_process}, // CAMERA_FREE_MODE
    {nullptr, nullptr, camera_slew_mode_init, nullptr, camera_slew_process}, // CAMERA_SLEW_MODE
    {nullptr, nullptr, nullptr, nullptr, nullptr}, // CAMERA_CURSOR_MODE
    {nullptr, nullptr, nullptr, nullptr, camera_ms_process}, // CAMERA_MINI_SCRIPT_MODE
    {nullptr, nullptr, nullptr, nullptr, camera_cs_process}, // CAMERA_CUTSCENE_SCRIPT_MODE
    {nullptr, nullptr, nullptr, nullptr, nullptr}, // CAMERA_DUMB_MODE
    {nullptr, nullptr, nullptr, nullptr, camera_death_process}, // CAMERA_DEATH_MODE
    {nullptr, nullptr, nullptr, nullptr, camera_lookaround_process}, // CAMERA_LOOKAROUND_MODE
    {nullptr, nullptr, nullptr, camera_first_person_pre_process, camera_first_person_process}, // CAMERA_FIRST_PERSON_MODE
    {nullptr, nullptr, nullptr, nullptr, camera_turret_process}, // CAMERA_TURRET_MODE
    {nullptr, nullptr, nullptr, camera_first_person_pre_process, camera_third_person_process}, // CAMERA_THIRD_PERSON_MODE
    {nullptr, nullptr, nullptr, nullptr, nullptr}, // CAMERA_TEST_SUITE_MODE
    {nullptr, camera_satellite_level_init, camera_satellite_mode_init, nullptr, camera_satellite_process}, // CAMERA_SATELLITE_MODE
};
static const float max_speed_squared = 0.1f; // 0x012D04B0 (the collision callback's moving-object limit)
camera_mode RFG_on_foot_camera_mode = CAMERA_THIRD_PERSON_MODE; // 0x0164894C
static float CAMERA_SMOOTHING_POS_DAMPENING_FACTOR = 10.0f; // 0x0164893C
static float CAMERA_SMOOTHING_ROT_DAMPENING_FACTOR = 10.0f; // 0x01648940
static bool Camera_motion_blur_enable = true; // 0x01648950
static float Camera_motion_blur_scale = 0.3f; // 0x01648954
static float Camera_motion_blur_min_velocity = 2.0f; // 0x01648958
static float Camera_motion_blur_max_velocity = 7.0f; // 0x0164895C
static float Camera_npc_min_fade_pct; // 0x01DE4514 (a tweak; 0 until the tweak table sets it)
static float Camera_npc_min_fade_dist; // 0x01DE4548 (a tweak)
static float Camera_npc_max_fade_dist; // 0x01DE4550 (a tweak)
bool Camera_smoothing; // 0x01DE48EB
static bool Camera_system_init; // 0x01DE48F1
bool Freeze_camera; // 0x01DE48F2
static tweak_table_item Tweak_table_item_Camera_npc_min_fade_pct("Camera_npc_min_fade_pct", &Camera_npc_min_fade_pct); // 0x01DE4928
static tweak_table_item Tweak_table_item_Camera_npc_min_fade_dist("Camera_npc_min_fade_dist", &Camera_npc_min_fade_dist); // 0x01DE4B20
static tweak_table_item Tweak_table_item_Camera_npc_max_fade_dist("Camera_npc_max_fade_dist", &Camera_npc_max_fade_dist); // 0x01DE6A94
// The vehicle turret camera's height spring (turret_update_camera_post): strength and the damping ratio's damping.
static float CAMERA_TURRET_DAMP_RATIO = 0.9f; // 0x01648944
static float CAMERA_TURRET_SMOOTH_STRENGTH = 3.8f; // 0x01648948
static float CAMERA_TURRET_SMOOTH_DAMP = (float)(sqrt((double)CAMERA_TURRET_SMOOTH_STRENGTH) * (double)CAMERA_TURRET_DAMP_RATIO * 2.0); // 0x01DE48F4 (dynamic initializer 0x01068DD0)

static void turret_update_camera_post(); // 0x006DE030 (:402)
static void camera_aim_turret(); // 0x006C8CB0 (:602)

static vector find_buffered_hit_point(const vector& in_delta, const vector& hit_point, const vector& hit_normal); // 0x006C8F30 (:797)
static void snap_to_default_pos(); // 0x006DEBC0 (:821)
static bool camera_move_raycast_filter_callback(object* hit_objp, void* filter_params); // 0x006C9110 (:888)

// 0x006DE030 (:402)
// The turret camera: at the turret's camera offset (fine aim has its own) in the weapon's frame, kept above the
// turret's base plane (half a meter above a mounted turret's position; a vehicle turret's height can spring), looking
// at the smoothed aim distance along the muzzle's line to the aim point, and pulled in toward the line from the weapon
// (a vehicle turret: from the turret) when that line collides.
static void turret_update_camera_post()
{
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    turret* tp = human_combat_get_manned_turret(pp);
    if (tp == nullptr)
    {
        return;
    }
    const turret_info* info = tp->t_info;
    if (info == nullptr)
    {
        return;
    }
    weapon* wp = tp->get_weapon_inst();
    if (wp == nullptr)
    {
        return;
    }
    vector offset = info->default_camera_offset;
    if (player_is_fine_aimed(pp))
    {
        offset = info->fine_aim_camera_offset;
    }
    vector muzzle_pos;
    matrix muzzle_orient;
    wp->get_muzzle_transform(muzzle_pos, &muzzle_orient);
    if (info->flags.cycle_primary_weapon)
    {
        // Magic number: alternate body 3 is the gun of a cycling turret.
        const unsigned int handle = tp->get_alt_hk_body_handle(3);
        if (handle != 0xFFFFFFFF)
        {
            havok_body_get_pos_orient(handle, muzzle_pos, muzzle_orient);
        }
    }
    vector to_aim = pp->aim_pos - muzzle_pos;
    const float aim_dist = to_aim.mag_and_normalize_safe();
    if (Rfg_camera.last_aim_point_dist < 0.0f)
    {
        Rfg_camera.last_aim_point_dist = aim_dist;
    }
    // Magic number: the distance changes by at most 5 times itself per second.
    float delta = (float)((double)Rfg_camera.last_aim_point_dist * -5.0 * (double)Frametime);
    if ((double)aim_dist - (double)Rfg_camera.last_aim_point_dist - (double)delta >= 0.0)
    {
        delta = aim_dist - Rfg_camera.last_aim_point_dist;
    }
    float max_delta = (float)((double)Rfg_camera.last_aim_point_dist * 5.0 * (double)Frametime);
    if (delta - max_delta < 0.0f)
    {
        max_delta = delta;
    }
    Rfg_camera.last_aim_point_dist = max_delta + Rfg_camera.last_aim_point_dist;
    vector aim_point;
    aim_point.x = (float)((double)to_aim.x * (double)Rfg_camera.last_aim_point_dist + (double)muzzle_pos.x);
    aim_point.y = (float)((double)to_aim.y * (double)Rfg_camera.last_aim_point_dist + (double)muzzle_pos.y);
    aim_point.z = (float)((double)to_aim.z * (double)Rfg_camera.last_aim_point_dist + (double)muzzle_pos.z);
    const float prev_y = Rfg_camera.ideal_pos.y;
    matrix weapon_orient;
    weapon_orient.make(wp->orient.fvec);
    vector cam;
    cam.x = (float)((double)weapon_orient.rvec.x * (double)offset.x + (double)weapon_orient.uvec.x * (double)offset.y + (double)weapon_orient.fvec.x * (double)offset.z + (double)tp->pos.x);
    cam.y = (float)((double)weapon_orient.rvec.y * (double)offset.x + (double)weapon_orient.uvec.y * (double)offset.y + (double)weapon_orient.fvec.y * (double)offset.z + (double)tp->pos.y);
    Rfg_camera.ideal_pos.z = (float)((double)weapon_orient.rvec.z * (double)offset.x + (double)weapon_orient.uvec.z * (double)offset.y + (double)weapon_orient.fvec.z * (double)offset.z + (double)tp->pos.z);
    // The lowest height: the turret's position, or its base plane under the camera when the turret stands upright
    // enough (up vector above 0.4).
    float floor_y = tp->pos.y;
    if (0.4 < (double)tp->orient.uvec.y)
    {
        const double uy = (double)tp->orient.uvec.y;
        const float plane_y = (float)((double)-tp->orient.uvec.x * ((double)cam.x - (double)tp->pos.x) / uy + (double)floor_y + (double)-tp->orient.uvec.z * ((double)Rfg_camera.ideal_pos.z - (double)tp->pos.z) / uy);
        if (floor_y - plane_y < 0.0f)
        {
            floor_y = plane_y;
        }
    }
    if (!tp->turret_flags.vehicle_turret)
    {
        floor_y = (float)((double)floor_y + 0.5);
    }
    float y = cam.y;
    if (tp->turret_flags.vehicle_turret && info->flags.smooth_camera)
    {
        Rfg_camera.vertical_velocity = (float)((((double)cam.y - (double)prev_y) * (double)CAMERA_TURRET_SMOOTH_STRENGTH - (double)CAMERA_TURRET_SMOOTH_DAMP * (double)Rfg_camera.vertical_velocity) * (double)Frametime + (double)Rfg_camera.vertical_velocity);
        y = (float)((double)Rfg_camera.vertical_velocity * (double)Frametime + (double)prev_y);
    }
    if (y < floor_y)
    {
        y = floor_y;
    }
    Rfg_camera.ideal_pos.x = cam.x;
    Rfg_camera.ideal_pos.y = y;
    const vector to_point = aim_point - Rfg_camera.ideal_pos;
    Rfg_camera.ideal_orient.make(to_point.copy_normalize_safe());
    line view;
    view.point = Rfg_camera.ideal_pos;
    view.dir = Rfg_camera.ideal_orient.fvec;
    vector closest;
    ix_closest_point_on_ray(tp->turret_flags.vehicle_turret ? tp->pos : wp->pos, view, closest);
    float frac = 1.0f;
    const vector back = Rfg_camera.ideal_pos - closest;
    vector hit;
    // Magic number: the camera keeps 0.2 m from what it hits.
    if (camera_collision_test(closest, Rfg_camera.ideal_pos, &hit, 0.2f, COLLIDE_RAY_DEFAULT))
    {
        const vector to_hit = hit - closest;
        const float len = back.mag();
        float d = (float)((double)to_hit.mag() - (double)0.2f);
        if (d - 0.0f < 0.0f)
        {
            d = 0.0f;
        }
        if (d - len >= 0.0f)
        {
            d = len;
        }
        frac = d / len;
    }
    // The fraction moves in at most 5 per second, out at most 1 per second.
    if (frac < Rfg_camera.last_collision_fraction)
    {
        const float f = (float)((double)Rfg_camera.last_collision_fraction - (double)Frametime * 5.0);
        Rfg_camera.last_collision_fraction = frac;
        if (f - frac >= 0.0f)
        {
            Rfg_camera.last_collision_fraction = f;
        }
        if (Rfg_camera.last_collision_fraction - 1.0f >= 0.0f)
        {
            Rfg_camera.last_collision_fraction = 1.0f;
        }
    }
    else
    {
        float f = Rfg_camera.last_collision_fraction + Frametime;
        if (f - 0.0f < 0.0f)
        {
            f = 0.0f;
        }
        Rfg_camera.last_collision_fraction = frac;
        if (f - frac < 0.0f)
        {
            Rfg_camera.last_collision_fraction = f;
        }
    }
    Rfg_camera.ideal_pos.z = (float)((double)back.z * (double)Rfg_camera.last_collision_fraction + (double)closest.z);
    Rfg_camera.ideal_pos.y = (float)((double)back.y * (double)Rfg_camera.last_collision_fraction + (double)closest.y);
    Rfg_camera.ideal_pos.x = (float)((double)back.x * (double)Rfg_camera.last_collision_fraction + (double)closest.x);
}

// 0x006C8CB0 (:602)
// The turret aims where its bullet would hit along the muzzle at the weapon's range; the player's aim target is what
// it hits (a dead human is not one), the aim point the range's end.
static void camera_aim_turret()
{
    player* pp = player_get_local_player();
    turret* tp = human_combat_get_manned_turret(pp);
    if (tp == nullptr)
    {
        return;
    }
    const unsigned int old_target = pp->aim_target;
    const turret_info* info = tp->t_info;
    const weapon_info* wi = info->w_info;
    vector muzzle_pos;
    matrix muzzle_orient;
    tp->get_weapon_inst()->get_muzzle_transform(muzzle_pos, &muzzle_orient);
    if (info->flags.cycle_primary_weapon)
    {
        const unsigned int handle = tp->get_alt_hk_body_handle(3);
        if (handle != 0xFFFFFFFF)
        {
            havok_body_get_pos_orient(handle, muzzle_pos, muzzle_orient);
        }
    }
    const vector end = muzzle_pos + muzzle_orient.fvec * wi->max_range;
    vector target = end;
    weapon_fire_info fire_info(muzzle_pos, end, wi);
    unsigned int hit_handle = 0xFFFFFFFF;
    if (weapons_test_bullet(fire_info, pp, true, 0.0f, ZERO_VECTOR))
    {
        target = fire_info.hit_pos;
        object* objp = World.object_from_handle(fire_info.hit_handle);
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || !objp->is_killed())
        {
            hit_handle = fire_info.hit_handle;
        }
    }
    pp->aim_pos = end;
    pp->aim_target = hit_handle;
    if (old_target != hit_handle)
    {
        pp->aim_target_duration.set(0);
    }
    tp->aim_turret(target);
}

// 0x006DEA50 (:662)
// The turret mode until the player leaves the turret: aim, place the camera, and pose the player on the turret this
// frame (the body, then the head).
static void camera_turret_process(float step_time, bool last_step_of_frame)
{
    player* pp = player_get_local_player();
    if (human_combat_get_manned_turret(pp) == nullptr)
    {
        camera_stop_turret_mode(false);
        return;
    }
    camera_aim_turret();
    turret_update_camera_post();
    camera_first_person_process_fov(step_time);
    human_get_turret_state_anim(pp);
    vanim_process_states(pp->char_inst->vp);
    if (!pp->rdd.is_loaded())
    {
        return;
    }
    animlib_clear_bone_info(pp->char_inst->aim_handle);
    animlib_transform_command* cmd = animlib_request_batch_start();
    cmd->process_instance = 1;
    cmd->human_ptr = pp;
    cmd->aim_handle = pp->char_inst->aim_handle;
    cmd->frametime = 0.001f; // Magic number: a token step (the states were processed above)
    cmd->num_bone_lods = 100;
    cmd->bone_lods = Bone_lods;
    cmd->bone_lod_level = 0;
    cmd->transform_all_bones_pre = 1;
    cmd->transform_all_bones_post = 1;
    animlib_request_batch_end();
    animlib_batch_process_all_start();
    animlib_batch_process_all_end();
    human::resource_dependent_data* rdd = pp->rdd.get_data();
    if (rdd->head_manager_idx >= 0 && animlib_allocate_bones(rdd->head_aim_handle))
    {
        matrix43 tmp_matrices[20];
        animlib_clear_bone_info(rdd->head_aim_handle);
        animlib_transform_all_bones_pre(rdd->head_aim_handle, tmp_matrices, nullptr, 0);
        animlib_transform_all_bones_post(rdd->head_aim_handle, tmp_matrices, nullptr, 0);
    }
}

// 0x006D07C0 (:758)
// Only the members' constructors run: the mini script splines, the camera view data (its timestamps and effect handles
// invalid) and the culling frustum (6 planes). Rfg_camera is the only instance.
rfg_camera::rfg_camera()
{
}

// 0x006C8CA0 (compiler-generated)
rfg_camera::~rfg_camera()
{
}

// 0x006C8ED0 (compiler-generated)
free_mode_params::free_mode_params()
{
}

// 0x006C8EE0 (compiler-generated)
thirdperson_mode_params::thirdperson_mode_params()
{
}

// 0x006C8EF0 (compiler-generated)
mini_script_params::mini_script_params()
{
}

// 0x006C8F20 (compiler-generated)
cutscene_mode_params::cutscene_mode_params()
{
}

// 0x006C8F30 (:797)
// LTCG: in_delta in EAX, hit_normal in ESI, the result in EDI. A point 0.05 m off the hit surface, back along the
// cast (at most the cast's length).
static vector find_buffered_hit_point(const vector& in_delta, const vector& hit_point, const vector& hit_normal)
{
    vector back = -in_delta;
    const float len = back.mag_and_normalize();
    // Magic numbers: 0.05 m from the surface; 0.0025 = 0.05^2.
    float d = back.y * hit_normal.y * 0.05f + hit_normal.x * 0.05f * back.x + back.z * hit_normal.z * 0.05f;
    if (d == 0.0f)
    {
        return hit_point;
    }
    d = 0.0025000002f / d;
    if (d - 0.0f < 0.0f)
    {
        d = 0.0f;
    }
    if (d - len >= 0.0f)
    {
        d = len;
    }
    return vector(hit_point.x + back.x * d, hit_point.y + back.y * d, hit_point.z + back.z * d);
}

// 0x006C9110 (:888)
// The camera's cast ignores humans and the player's vehicle (or a turret attached to it).
static bool camera_move_raycast_filter_callback(object* hit_objp, void* filter_params)
{
    if (hit_objp->get_obj_type() == OT_HUMAN)
    {
        return false;
    }
    if (hit_objp->get_obj_type() == OT_VEHICLE)
    {
        if (player_get_local_player()->vehicle_handle == hit_objp->get_handle())
        {
            return false;
        }
    }
    else if (hit_objp->get_obj_type() == OT_TURRET)
    {
        player* pp = player_get_local_player();
        if (((turret*)hit_objp)->turret_flags.vehicle_turret && hit_objp->get_parent() == pp->vehicle_handle)
        {
            return false;
        }
    }
    return true;
}

// 0x006C9160 (:968)
void camera_system_shutdown()
{
    if (Camera_system_init)
    {
        Camera_system_init = false;
        if (Camera_main_scene_p != nullptr)
        {
            Camera_main_scene_p->release();
            Camera_main_scene_p = nullptr;
        }
    }
}

// 0x006C9190 (:1071)
void camera_do_frame_pre()
{
    if (Camera_mode_callbacks[Rfg_camera.mode].pre_process_callback != nullptr)
    {
        Camera_mode_callbacks[Rfg_camera.mode].pre_process_callback();
    }
}

// 0x006C91B0 (:1415)
// Applied by the next camera_do_frame.
void camera_snap_to_default_pos()
{
    Rfg_camera.flags |= 4;
}

// 0x006C91C0 (:1474)
void camera_set_pos(const vector& p)
{
    Rfg_camera.ideal_pos = p;
    Rfg_camera.real_pos = p;
}

// 0x006C91F0 (:1479)
void camera_set_orient(const matrix& o)
{
    Rfg_camera.ideal_orient = o;
    Rfg_camera.real_orient = o;
    float bank;
    o.extract_angles(Rfg_camera.lookaround_params.pitch, bank, Rfg_camera.lookaround_params.heading);
}

// 0x006C9280 (:1489)
void camera_set_transform(const vector& p, const matrix& o)
{
    camera_set_pos(p); // inlined in the binary
    camera_set_orient(o);
}

// 0x006C92C0 (:1555)
void camera_turret_set_rotation(float pitch, float heading, control_axis_action h_action, control_axis_action v_action)
{
    turret* tp = human_combat_get_manned_turret(player_get_local_player());
    if (tp != nullptr)
    {
        tp->set_turret_velocity(heading, pitch, h_action, v_action);
    }
}

// 0x006C9300 (:1579)
// A mode already on the stack moves to the top; a new active mode runs its init and stops the transition.
void camera_push_mode(camera_mode m)
{
    for (int i = Rfg_camera.mode_stack_size - 1; i >= 0; i--)
    {
        if (Rfg_camera.mode_stack[i] == m)
        {
            for (; i < Rfg_camera.mode_stack_size - 1; i++)
            {
                Rfg_camera.mode_stack[i] = Rfg_camera.mode_stack[i + 1];
            }
            Rfg_camera.mode_stack_size--;
            break;
        }
    }
    Rfg_camera.mode_stack[Rfg_camera.mode_stack_size] = m;
    Rfg_camera.mode_stack_size++;
    if (Rfg_camera.mode != m)
    {
        Rfg_camera.flags &= ~2u;
        Rfg_camera.mode = m;
        if (Camera_mode_callbacks[m].mode_init_callback != nullptr)
        {
            Camera_mode_callbacks[m].mode_init_callback();
        }
    }
}

// 0x006C9390 (:1619)
// The mode below (or the level mode) becomes active.
void camera_pop_mode(camera_mode m)
{
    if (Rfg_camera.mode_stack_size <= 0)
    {
        return;
    }
    if (Rfg_camera.mode_stack[Rfg_camera.mode_stack_size - 1] == m)
    {
        Rfg_camera.mode_stack_size--;
    }
    else
    {
        for (int i = Rfg_camera.mode_stack_size - 2; i >= 0; i--)
        {
            if (Rfg_camera.mode_stack[i] == m)
            {
                for (; i < Rfg_camera.mode_stack_size - 1; i++)
                {
                    Rfg_camera.mode_stack[i] = Rfg_camera.mode_stack[i + 1];
                }
                Rfg_camera.mode_stack_size--;
                break;
            }
        }
    }
    const camera_mode next = Rfg_camera.mode_stack_size != 0 ? Rfg_camera.mode_stack[Rfg_camera.mode_stack_size - 1] : Rfg_camera.level_mode;
    Rfg_camera.flags &= ~2u;
    const bool changed = next != Rfg_camera.mode;
    Rfg_camera.mode = next;
    if (changed && Camera_mode_callbacks[next].mode_init_callback != nullptr)
    {
        Camera_mode_callbacks[next].mode_init_callback();
    }
}

// 0x006C9440 (:1672)
camera_mode camera_get_mode()
{
    return Rfg_camera.mode;
}

// 0x006C9450 (:1677)
void camera_set_level_mode(camera_mode m, bool smooth)
{
    Rfg_camera.level_mode = m;
    if (Rfg_camera.mode_stack_size == 0 && Rfg_camera.mode != m)
    {
        Rfg_camera.flags &= ~2u;
        Rfg_camera.mode = m;
        if (smooth)
        {
            camera_trans_start_linear_transition(0.4f, 7, true); // inlined in the binary
        }
        if (Camera_mode_callbacks[Rfg_camera.mode].mode_init_callback != nullptr)
        {
            Camera_mode_callbacks[Rfg_camera.mode].mode_init_callback();
        }
    }
}

// 0x006C9510 (:1700)
bool camera_mode_is_level_mode(camera_mode m)
{
    return m == CAMERA_FREE_MODE;
}

// 0x006C9520 (:1714)
bool camera_mode_is_on_stack(camera_mode m)
{
    for (int i = 0; i < Rfg_camera.mode_stack_size; i++)
    {
        if (Rfg_camera.mode_stack[i] == m)
        {
            return true;
        }
    }
    return false;
}

// 0x006C9550 (:1735)
void camera_get_xz_orient(vector& fvec, vector& rvec)
{
    fvec = Rfg_camera.real_orient.fvec;
    fvec.y = 0.0f;
    fvec.normalize_safe();
    rvec = Rfg_camera.real_orient.rvec;
    rvec.y = 0.0f;
    rvec.normalize_safe();
}

// 0x006C95A0 (:1752)
void camera_freeze()
{
    Freeze_camera = true;
}

// 0x006C95B0 (:1761)
void camera_unfreeze()
{
    Freeze_camera = false;
}

// 0x006C95C0 (:1769)
bool camera_is_frozen()
{
    return Freeze_camera;
}

// 0x006D0900 (:1255)
// The render transform (the third person lock keeps the last one and returns the controls to gameplay) and the
// view's depth of field, distance and effect.
void camera_do_frame_post()
{
    if (Camera_third_person_lock)
    {
        control_set_mode(CM_GAMEPLAY);
    }
    else
    {
        Rfg_camera.render_pos = Rfg_camera.real_pos;
        Rfg_camera.render_orient = Rfg_camera.real_orient;
    }
    Rfg_camera.m_camera_view_data.process(); // inlined in the binary
    Rfg_camera.culling_frustum_valid = false;
}

// 0x006D09B0 (:1465)
void camera_start_slew_mode()
{
    camera_push_mode(CAMERA_SLEW_MODE);
}

// 0x006D09C0 (:1469)
void camera_stop_slew_mode()
{
    camera_pop_mode(CAMERA_SLEW_MODE);
}

// 0x006D09D0 (:1495)
void camera_start_cursor_mode()
{
    camera_push_mode(CAMERA_CURSOR_MODE);
}

// 0x006D09E0 (:1499)
void camera_stop_cursor_mode()
{
    camera_pop_mode(CAMERA_CURSOR_MODE);
}

// 0x006D09F0 (:1504)
void camera_start_dumb_mode()
{
    camera_push_mode(CAMERA_DUMB_MODE);
}

// 0x006D0A00 (:1509)
void camera_stop_dumb_mode()
{
    camera_pop_mode(CAMERA_DUMB_MODE);
}

// 0x006D0A10 (:1514)
void camera_start_turret_mode(bool smooth)
{
    player* pp = player_get_local_player();
    camera_push_mode(CAMERA_TURRET_MODE);
    human_spinebend_disable(pp);
    turret* tp = human_combat_get_manned_turret(pp);
    player_set_zoom_state(pp, tp != nullptr && tp->t_info->flags.force_fine_aim ? PZS_FINE_AIM : PZS_OFF, false);
    Rfg_camera.last_aim_point_dist = -1.0f;
    Rfg_camera.last_collision_fraction = 1.0f;
    Rfg_camera.vertical_velocity = 0.0f;
    if (smooth)
    {
        camera_trans_start_linear_transition(0.4f, 7, true); // inlined in the binary
    }
}

// 0x006D0B00 (:1538)
void camera_stop_turret_mode(bool smooth)
{
    if (!camera_mode_is_on_stack(CAMERA_TURRET_MODE))
    {
        return;
    }
    camera_pop_mode(CAMERA_TURRET_MODE);
    human_spinebend_enable(player_get_local_player());
    player_set_zoom_state(player_get_local_player(), PZS_OFF, false);
    if (smooth)
    {
        camera_trans_start_linear_transition(0.4f, 7, true); // inlined in the binary
    }
}

// 0x006D0BD0 (:1775)
// What the camera's collision casts hit: not humans (in single player, other humans near the third person camera
// fade: camera_alpha_override), not items, not moving objects (above 0.1 m/s squared), not the vehicle the player is
// in (except while leaving the Marauder raider out of a protected seat), its turret, a turret the player mans or the
// player's ladder.
bool process_camera_collision_callback(object* hit_objp, void* info)
{
    if (hit_objp == nullptr)
    {
        return true;
    }
    if (hit_objp->get_obj_type() == OT_HUMAN)
    {
        player* pp;
        if (!Multiplayer && (pp = player_get_local_player()) != nullptr && (object*)pp != hit_objp && Rfg_camera.mode == CAMERA_THIRD_PERSON_MODE)
        {
            const vector& p = hit_objp->pos;
            // Magic numbers: the human's body from 0.5 m to 1.4 m above its feet.
            const vector bottom(p.x, (float)((double)p.y + 0.5), p.z);
            const vector top(p.x, (float)((double)p.y + (double)1.4f), p.z);
            vector closest;
            ix_closest_point_on_line_segment(closest, Rfg_camera.real_pos, bottom, top);
            const float dist = vec_dist(Rfg_camera.real_pos, closest);
            if (dist < Camera_npc_max_fade_dist && Camera_npc_min_fade_dist < dist)
            {
                ((human*)hit_objp)->camera_alpha_override = 0.0f;
            }
        }
        return false;
    }
    if (hit_objp->get_obj_type() == OT_ITEM)
    {
        return false;
    }
    if (hit_objp->havok_handle != 0xFFFFFFFF && hit_objp->get_obj_type() != OT_VEHICLE)
    {
        vector vel;
        havok_body_get_linear_velocity(hit_objp->havok_handle, vel);
        const float speed_sq = (float)((double)vel.x * (double)vel.x + (double)vel.y * (double)vel.y + (double)vel.z * (double)vel.z);
        if ((double)speed_sq > (double)max_speed_squared)
        {
            return false;
        }
    }
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return true;
    }
    if (hit_objp->get_obj_type() == OT_VEHICLE && human_in_vehicle_interaction(pp, (vehicle*)hit_objp))
    {
        if (Rfg_camera.mode == CAMERA_THIRD_PERSON_MODE && human_in_vehicle_transition(pp, (vehicle*)hit_objp, VI_INTERACTION_EXIT) &&
            !human_in_protected_vehicle_pos(pp) && ((vehicle*)hit_objp)->info != Nav_cell_detour_veh_info_mar_raider_1)
        {
            return true;
        }
        return false;
    }
    turret* tp = nullptr;
    if (hit_objp->get_obj_type() == OT_TURRET)
    {
        tp = (turret*)hit_objp;
        if (tp->turret_flags.vehicle_turret && human_in_vehicle_interaction(pp, tp->get_vehicle()))
        {
            return false;
        }
    }
    human_in_vehicle(pp); // Original: the result is not used.
    if (tp != nullptr && !tp->turret_flags.vehicle_turret && tp->current_operator == pp->get_handle())
    {
        return false;
    }
    if (pp->hflags.is_on_ladder)
    {
        ladder* lp = World.object_from_handle<ladder>(pp->ladder_handle);
        if (lp != nullptr && lp->get_parent() == hit_objp->get_handle())
        {
            return false;
        }
    }
    return true;
}

// 0x006D4BE0 is camera_view_system_init (camera_view.cpp).

// 0x006D66E0 (:1426)
// Moves toward the desired position up to the first hit (static or not, by the camera filter), 0.05 m off the
// surface; with slide the rest of the move slides along the hit surface (one more cast).
vector camera_move(const vector& cur_pos, const vector& desired_pos, bool slide)
{
    raycast_input in;
    in.start = cur_pos;
    in.end = desired_pos;
    in.radius = 0.0f;
    in.filter_params = nullptr;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 1;
    in.m_filter_info = COLLIDE_CAMERA;
    in.filter_callback = camera_move_raycast_filter_callback;
    raycast_output out;
    if (!havok_raycast(in, out))
    {
        return desired_pos;
    }
    const raycast_output::hit_info& hit = out.m_hits[0];
    const vector delta(desired_pos.x - cur_pos.x, desired_pos.y - cur_pos.y, desired_pos.z - cur_pos.z);
    const vector buffered = find_buffered_hit_point(delta, hit.hit_pos, hit.hit_normal); // inlined in the binary
    if (!slide)
    {
        return buffered;
    }
    // The desired position pushed out of the hit plane (0.05 m in front of it).
    const vector& n = hit.hit_normal;
    const float push = n.y * (buffered.y - desired_pos.y) + n.x * (buffered.x - desired_pos.x) + n.z * (buffered.z - desired_pos.z) + 0.05f;
    const vector slid(desired_pos.x + push * n.x, desired_pos.y + n.y * push, desired_pos.z + n.z * push);
    return camera_move(buffered, slid, false);
}

// 0x006D6CE0 (:1871)
// A ray (radius 0) or sphere cast with the camera's filter; the hit position, or the end without a hit.
bool camera_collision_test(const vector& start_pos, const vector& end_pos, vector* hit_pos, float radius, unsigned int filter)
{
    raycast_input in;
    in.start = start_pos;
    in.end = end_pos;
    in.filter_params = nullptr;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 1;
    in.m_filter_info = filter;
    in.filter_callback = process_camera_collision_callback;
    in.radius = radius;
    raycast_output out;
    bool hit;
    if (radius <= 0.0f)
    {
        hit = havok_raycast(in, out);
    }
    else
    {
        in.m_hit_collector_type = raycast_input::COLLECT_ALL_HITS;
        hit = havok_spherecast(in, out);
    }
    if (hit)
    {
        if (hit_pos != nullptr)
        {
            *hit_pos = out.m_hits[0].hit_pos;
        }
        return true;
    }
    if (hit_pos != nullptr)
    {
        *hit_pos = end_pos;
    }
    return false;
}

// 0x006DEBC0 (:821)
// The free camera: behind the target at the submode's default position (moved out of obstructions), looking at the
// target. The first and third person cameras: at the default position with the target's orientation (their pitch
// and heading follow it).
static void snap_to_default_pos()
{
    if (Rfg_camera.mode == CAMERA_FREE_MODE)
    {
        const vector lookat = camera_free_get_lookat(0.0f); // inlined in the binary
        const vector desired = camera_free_get_default_pos(); // inlined in the binary
        const vector pos = camera_move(lookat, desired, true);
        Rfg_camera.ideal_pos = pos;
        Rfg_camera.real_pos = pos;
        Rfg_camera.last_pos = Rfg_camera.ideal_pos;
        Rfg_camera.free_params.cur_dist_from_lookat = vec_dist(lookat, desired);
        const vector to_lookat(lookat.x - Rfg_camera.ideal_pos.x, lookat.y - Rfg_camera.ideal_pos.y, lookat.z - Rfg_camera.ideal_pos.z);
        matrix orient;
        // Magic number: 0.0001, the camera on its look-at point.
        if (vec_is_equal_eps(to_lookat, ZERO_VECTOR, 0.0001f))
        {
            orient = IDENTITY_MATRIX;
        }
        else
        {
            orient.make_uvec(to_lookat, UP_VECTOR);
        }
        Rfg_camera.last_orient = orient;
        Rfg_camera.real_orient = orient;
        Rfg_camera.ideal_orient = orient;
    }
    else if (Rfg_camera.mode == CAMERA_FIRST_PERSON_MODE || Rfg_camera.mode == CAMERA_THIRD_PERSON_MODE)
    {
        object* target = World.object_from_handle(Rfg_camera.target_handle);
        if (target != nullptr)
        {
            Rfg_camera.real_pos = camera_free_get_default_pos(); // inlined in the binary
            Rfg_camera.ideal_orient = target->orient;
            Rfg_camera.real_orient = target->orient;
            Rfg_camera.last_orient = target->orient;
            Rfg_camera.ideal_pos = Rfg_camera.real_pos;
            Rfg_camera.last_pos = Rfg_camera.real_pos;
            Rfg_camera.real_orient.extract_heading(Rfg_camera.lookaround_params.heading);
            Rfg_camera.real_orient.extract_pitch(Rfg_camera.lookaround_params.pitch);
        }
    }
    Rfg_camera.flags &= ~2u;
}

// 0x006DFFA0 (:1080)
// The active mode's process (one step of a frame) gives the ideal transform; the transition blends into it, smoothing
// (off) lags the real one behind it, the shakes rotate the real orientation. The motion blur follows the heading
// change (fb_accum_blur).
void camera_do_frame()
{
    object* target = World.object_from_handle(Rfg_camera.target_handle);
    if (target == nullptr && multi_is_player_in_spectator_view(player_get_local_player()))
    {
        control_spectator_next_viewed_player(false);
        target = World.object_from_handle(Rfg_camera.target_handle);
    }
    fb_accum_blur_set_percentage(0.0f);
    fb_accum_blur_set_enable(false);
    const float step_time = Frametime;
    if (Camera_motion_blur_enable)
    {
        if ((Rfg_camera.flags & 0x10) == 0)
        {
            const vector last_fvec(Rfg_camera.last_orient.fvec.x, 0.0f, Rfg_camera.last_orient.fvec.z);
            const vector fvec(Rfg_camera.real_orient.fvec.x, 0.0f, Rfg_camera.real_orient.fvec.z);
            if (!last_fvec.is_null() && !fvec.is_null())
            {
                const float rate = fabsf(vec_delta_ang(last_fvec, fvec, UP_VECTOR)) / Frametime;
                if (Camera_motion_blur_min_velocity < rate)
                {
                    float blur = (rate - Camera_motion_blur_min_velocity) / (Camera_motion_blur_max_velocity - Camera_motion_blur_min_velocity) * Camera_motion_blur_scale;
                    if (blur - 0.0f < 0.0f)
                    {
                        blur = 0.0f;
                    }
                    if (blur - 1.0f >= 0.0f)
                    {
                        blur = 1.0f;
                    }
                    fb_accum_blur_set_enable(true);
                    fb_accum_blur_change_percentage(blur);
                }
            }
        }
        else
        {
            Rfg_camera.flags &= ~0x10u;
        }
    }
    Rfg_camera.last_pos = Rfg_camera.real_pos;
    Rfg_camera.last_orient = Rfg_camera.real_orient;
    Rfg_camera.last_fov = Rfg_camera.real_fov;
    if (Camera_mode_callbacks[Rfg_camera.mode].process_callback != nullptr)
    {
        Camera_mode_callbacks[Rfg_camera.mode].process_callback(step_time, true);
    }
    if ((Rfg_camera.flags & 2) != 0)
    {
        camera_trans_process(&Rfg_camera.ideal_pos, &Rfg_camera.ideal_orient, &Rfg_camera.ideal_fov, Rfg_camera.ideal_pos, Rfg_camera.ideal_orient, Rfg_camera.ideal_fov, step_time);
    }
    if ((Rfg_camera.flags & 4) != 0)
    {
        snap_to_default_pos();
        Rfg_camera.flags &= ~4u;
    }
    if ((!Camera_smoothing && (Rfg_camera.flags & 8) == 0) || Rfg_camera.mode == CAMERA_LOOKAROUND_MODE || Rfg_camera.mode == CAMERA_TURRET_MODE)
    {
        Rfg_camera.real_pos = Rfg_camera.ideal_pos;
        Rfg_camera.real_orient = Rfg_camera.ideal_orient;
    }
    else
    {
        float t = CAMERA_SMOOTHING_POS_DAMPENING_FACTOR * Frametime_uncompressed;
        if (t - 0.0f < 0.0f)
        {
            t = 0.0f;
        }
        if (t - 1.0f >= 0.0f)
        {
            t = 1.0f;
        }
        Rfg_camera.real_pos.x = Rfg_camera.last_pos.x + (Rfg_camera.ideal_pos.x - Rfg_camera.last_pos.x) * t;
        Rfg_camera.real_pos.y = Rfg_camera.last_pos.y + (Rfg_camera.ideal_pos.y - Rfg_camera.last_pos.y) * t;
        Rfg_camera.real_pos.z = Rfg_camera.last_pos.z + (Rfg_camera.ideal_pos.z - Rfg_camera.last_pos.z) * t;
        t = CAMERA_SMOOTHING_ROT_DAMPENING_FACTOR * Frametime_uncompressed;
        if (t - 0.0f < 0.0f)
        {
            t = 0.0f;
        }
        if (t - 1.0f >= 0.0f)
        {
            t = 1.0f;
        }
        quaternion from;
        from.set(Rfg_camera.last_orient);
        quaternion to;
        to.set(Rfg_camera.ideal_orient);
        if (quaternion_dot(from, to) < 0.0f)
        {
            from = quaternion(-from.x, -from.y, -from.z, -from.w);
        }
        quaternion q;
        pltind_slerp(q, from, to, t);
        q.extract_matrix(Rfg_camera.real_orient);
    }
    Rfg_camera.real_fov = Rfg_camera.ideal_fov;
    camera_shake_process();
    if (target != nullptr)
    {
        Rfg_camera.last_target_pos = target->pos;
        Rfg_camera.last_target_orient = target->orient;
    }
}

// 0x006E0C40 (:938)
// Once: the modes' system init, the clip distances, the shakes and views (both inlined in the binary), the main render
// camera.
void camera_system_init()
{
    if (Camera_system_init)
    {
        return;
    }
    Camera_system_init = true;
    for (int i = 0; i < NUM_CAMERA_MODES; i++)
    {
        if (Camera_mode_callbacks[i].system_init_callback != nullptr)
        {
            Camera_mode_callbacks[i].system_init_callback();
        }
    }
    Rfg_camera.real_fov = 45.0f;
    Rfg_camera.m_near_clip_dist = 0.1f;
    Rfg_camera.m_far_clip_dist = 475.0f;
    Rfg_camera.m_high_lod_far_clip_dist = 320.0f;
    Rfg_camera.mode_stack_size = 0;
    camera_shake_system_init();
    camera_view_system_init();
    Camera_main_scene_p = rl_camera::create();
}

// 0x006E0D70 (:984)
// The free camera follows the local player; every mode's level init runs; on foot the player starts in the third
// person mode (RFG_on_foot_camera_mode), snapped behind the player with the normal view.
void camera_level_init_post()
{
    camera_free_set_target(player_get_local_player());
    Rfg_camera.lookaround_params.rotvel_p = 0.0f;
    Rfg_camera.lookaround_params.rotvel_h = 0.0f;
    Rfg_camera.lookaround_params.rotvel_added_p = 0.0f;
    Rfg_camera.lookaround_params.rotvel_added_h = 0.0f;
    Rfg_camera.mode_stack_size = 0;
    Rfg_camera.mode = CAMERA_FREE_MODE;
    Rfg_camera.culling_frustum_valid = false;
    Rfg_camera.flags = 0;
    if (Rfg_camera.target_handle == 0xFFFFFFFF)
    {
        Rfg_camera.last_target_pos = ZERO_VECTOR;
        Rfg_camera.last_target_orient = IDENTITY_MATRIX;
    }
    else
    {
        object* target = World.object_from_handle(Rfg_camera.target_handle);
        Rfg_camera.last_target_pos = target->pos;
        Rfg_camera.last_target_orient = target->orient;
    }
    Rfg_camera.real_pos = ZERO_VECTOR;
    Rfg_camera.ideal_pos = ZERO_VECTOR;
    Rfg_camera.last_pos = ZERO_VECTOR;
    Rfg_camera.real_orient = IDENTITY_MATRIX;
    Rfg_camera.real_fov = 45.0f;
    Rfg_camera.ideal_orient = IDENTITY_MATRIX;
    Rfg_camera.ideal_fov = 45.0f;
    Rfg_camera.last_orient = IDENTITY_MATRIX;
    // Magic numbers: the descent physics of the slew camera (max velocity, max rotation velocity, dampings).
    descent_physics_init(10.0f, 1.6f, 0.1f, 0.1f);
    for (int i = 0; i < NUM_CAMERA_MODES; i++)
    {
        if (Camera_mode_callbacks[i].level_init_callback != nullptr)
        {
            Camera_mode_callbacks[i].level_init_callback();
        }
    }
    if ((!Multiplayer || !multi_is_dedicated_server()) && !multi_game_spectating())
    {
        if (RFG_on_foot_camera_mode == CAMERA_FIRST_PERSON_MODE)
        {
            camera_start_first_person(true);
        }
        else if (RFG_on_foot_camera_mode == CAMERA_THIRD_PERSON_MODE)
        {
            camera_start_third_person(true);
        }
    }
    camera_shake_level_init_post(); // inlined in the binary
    camera_do_frame();
    camera_do_frame_post();
    camera_trans_abort(); // inlined in the binary
    snap_to_default_pos();
    Rfg_camera.render_pos = Rfg_camera.real_pos;
    Rfg_camera.render_orient = Rfg_camera.real_orient;
    Rfg_camera.m_camera_view_data.set_view(camera_view_from_name("normal"), true);
}
