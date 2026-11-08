// Original: volition/rfg/code/camera/camera_third_person.cpp
//
// The third person camera mode (player_minimal.md P6): over the player's shoulder at the view's distance, turned by
// the look angles (camera_first_person_pre_process), and kept out of walls: casts from the shoulder pivot (the view
// direction and four directions 1.1 rad around it) remember the nearest obstructions of the last frames, the camera
// stops in front of them and eases back out. A camera inside the player fades the player out.
// Compiled in rfg_camera_unity.cpp. Not ported: the debug console commands (unused_dcf_*). Vector operators round per
// operation as in the asm; scalar multi-op
// expressions are written as double chains rounded once, which was checked against the asm only for the start of
// camera_third_person_update_collisions.

#include "volition/rfg/code/camera/camera_third_person.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/rfg/code/camera/camera_first_person.h"
#include "volition/rfg/code/camera/camera_transition.h"
#include "volition/rfg/code/camera/camera_view.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_camera.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_move.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_autoaim.h"
#include "volition/rfg/code/objects/ladder.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/os/frametime.h"

#include <math.h>
#include <stdlib.h>

// ---- file-static globals
static const float Camera_collision_start_angle = 0.7f; // 0x012CFAD0, rad from the view direction where the obstruction stops counting
static const float predictive_cast_angle = 1.1f; // 0x012D04B4, rad: the side casts
static bool Camera_collision_enable = true; // 0x0164885A
static bool Camera_collision_alpha_enable = true; // 0x0164885B
static float Camera_collision_alpha = 1.0f; // 0x016488DC
static float Camera_collision_size_3rd_person = 0.1f; // 0x016488E0, the casts' radius
static float Camera_collision_percent = 1.0f; // 0x016488E4
static float Camera_collision_percent_rate_in_3rd_person = 15.0f; // 0x016488E8
static float Camera_collision_percent_rate_out_3rd_person = 3.0f; // 0x016488EC
static float Camera_collision_percent_snap = 0.2f; // 0x016488F0
static float Camera_ladder_tolerance = 0.7f; // 0x016488F4
static vector Camera_third_person_offset(0.58f, 1.6f, 0.3f); // 0x01648A80
static vector Camera_third_person_offset_close(0.5f, 1.57f, 0.3f); // 0x01648A8C
static vector Camera_third_person_offset_crouch(0.55f, 1.2f, 0.3f); // 0x01648A98
static vector Camera_third_person_offset_crouch_close(0.5f, 1.2f, 0.3f); // 0x01648AA4
static vector Camera_third_person_offset_crouch_scope(0.15f, 1.2f, 0.0f); // 0x01648AB0
static vector Camera_third_person_offset_scope(0.15f, 1.75f, 0.0f); // 0x01648ABC
static vector Camera_third_person_offset_sprint(0.52f, 1.45f, 0.3f); // 0x01648AC8
static vector Camera_third_person_offset_left(-0.58f, 1.6f, 0.3f); // 0x01648AD4
static vector Camera_third_person_offset_close_left(-0.5f, 1.57f, 0.3f); // 0x01648AE0
static vector Camera_third_person_offset_crouch_left(-0.55f, 1.2f, 0.3f); // 0x01648AEC
static vector Camera_third_person_offset_crouch_close_left(-0.5f, 1.2f, 0.3f); // 0x01648AF8
static vector Camera_third_person_offset_crouch_scope_left(-0.15f, 1.2f, 0.0f); // 0x01648B04
static vector Camera_third_person_offset_scope_left(-0.15f, 1.75f, 0.0f); // 0x01648B10
static vector Camera_third_person_offset_sprint_left(-0.52f, 1.45f, 0.3f); // 0x01648B1C
static vector Camera_cover_offset_right(0.58f, 1.6f, 0.3f); // 0x01648B28
static vector Camera_cover_offset_right_edge(0.95f, 1.6f, 0.3f); // 0x01648B34
static vector Camera_cover_offset_right_lean(1.15f, 1.6f, 0.3f); // 0x01648B40
static vector Camera_cover_crouch_offset_right(0.55f, 1.2f, 0.3f); // 0x01648B4C
static vector Camera_cover_crouch_offset_right_edge(0.95f, 1.0f, 0.3f); // 0x01648B58
static vector Camera_cover_crouch_offset_right_lean(1.15f, 1.0f, 0.3f); // 0x01648B64
static vector Camera_cover_crouch_offset_right_lean2(0.55f, 1.33f, 0.3f); // 0x01648B70
static vector Camera_cover_crouch_offset_right_lean2_high(0.58f, 1.65f, 0.3f); // 0x01648B7C
static vector Camera_cover_crouch_offset_right_bf2_high(0.58f, 1.75f, 0.3f); // 0x01648B88
static vector Camera_cover_offset_left(-0.58f, 1.6f, 0.3f); // 0x01648B94
static vector Camera_cover_offset_left_edge(-0.95f, 1.6f, 0.3f); // 0x01648BA0
static vector Camera_cover_offset_left_lean(-1.15f, 1.6f, 0.3f); // 0x01648BAC
static vector Camera_cover_crouch_offset_left(-0.55f, 1.2f, 0.3f); // 0x01648BB8
static vector Camera_cover_crouch_offset_left_edge(-0.95f, 1.0f, 0.3f); // 0x01648BC4
static vector Camera_cover_crouch_offset_left_lean(-1.15f, 1.0f, 0.3f); // 0x01648BD0
static vector Camera_cover_crouch_offset_left_lean2(-0.55f, 1.33f, 0.3f); // 0x01648BDC
static vector Camera_cover_crouch_offset_left_lean2_high(-0.58f, 1.65f, 0.3f); // 0x01648BE8
static vector Camera_cover_crouch_offset_left_bf2_high(-0.58f, 1.75f, 0.3f); // 0x01648BF4
static bool Camera_shoulder_switch; // 0x01DE48C9
static bool Camera_player_collision; // 0x01DE48CA
// The vehicle enter debug tuning: rfg.exe only reads these (nothing writes them), so they stay zero.
static bool Vehicle_enter_lookat_debug; // 0x01DE48CB
static vector debug_lookat_pos; // 0x01DE4B40, only written
static float Vehicle_enter_lookat_x; // 0x01DE48CC (global in rfg.exe)
static float Vehicle_enter_lookat_y; // 0x01DE48D0 (global in rfg.exe)
static float Vehicle_enter_lookat_z; // 0x01DE48D4 (global in rfg.exe)
static vector Camera_blended_offset; // 0x01DE494C
// The nearest obstructions of the last frames: per cast direction (the view direction last, index 4) the nearest to
// the camera line, the nearest to the pivot and the one cutting deepest into the view direction.
static camera_cached_collision Camera_cached_collisions[5][3]; // 0x01DE6BB0

// Function statics.
static float dot_pow = 0.5f; // 0x01648C1C (camera_third_person_interpolate)
static float alpha_blend_time = 0.25f; // 0x01648D10 (camera_third_person_adjust_for_obstruction), s
static float cam_alpha_scale_out = 3.0f; // 0x01648D14
static float cam_alpha_scale_in = 2.5f; // 0x01648D18
static float scaler = 0.5f; // 0x01648D1C (camera_third_person_process)
static float ragdoll_offset_return_rate = 1.0f; // 0x01648D20, m/s
static float max_ragdoll_offset = 0.9f; // 0x01648D24, m

static float camera_third_person_interpolate(float dot, float start_distance, float end_distance); // 0x006C7330 (:297)
static void camera_third_person_update_collisions(const vector& start_pos, const vector& end_pos, const vector& camera_desired_direction, float camera_desired_distance); // 0x006DBFC0 (:313)
static void camera_third_person_adjust_for_obstruction(const vector& target_pos, const vector& direction, const float& distance, const vector& offset, vector& final_pos); // 0x006DC720 (:409)
static const vector& camera_third_person_offset(); // 0x006CEAF0 (:649)

// 0x006C70C0 (:167)
void camera_cached_collision::set(vector new_pos)
{
    pos = new_pos;
    valid = true;
}

// 0x006C70E0 (compiler-generated)
camera_cached_collision::camera_cached_collision()
{
}

// 0x006C7330 (:297)
// The allowed distance of an obstruction at an angle from the view direction (dot its cosine, raised to dot_pow):
// in line the obstruction's own distance, from Camera_collision_start_angle on the full distance.
static float camera_third_person_interpolate(float dot, float start_distance, float end_distance)
{
    if (dot - 0.0f < 0.0f)
    {
        dot = 0.0f;
    }
    if (dot - 1.0f >= 0.0f)
    {
        dot = 1.0f;
    }
    if (dot_pow != 1.0f)
    {
        dot = (float)pow((double)dot, (double)dot_pow); // x87 __CIpow
        if (dot - 0.0f < 0.0f)
        {
            dot = 0.0f;
        }
        if (dot - 1.0f >= 0.0f)
        {
            dot = 1.0f;
        }
    }
    return fl_parameter(fl_acos(dot), Camera_collision_start_angle, 0.0f, start_distance, end_distance, true);
}

// The distance from a point to a cached obstruction minus the cast radius (inlined).
static float camera_third_person_obstruction_dist(const vector& from, const vector& pos)
{
    const vector d = from - pos;
    return (float)(sqrt((double)d.x * (double)d.x + (double)d.y * (double)d.y + (double)d.z * (double)d.z) - (double)Camera_collision_size_3rd_person);
}

// 0x006DBFC0 (:313)
// LTCG: start_pos in EDI. Four casts turned predictive_cast_angle around the up axis and the right axis; each hit
// replaces a cached obstruction of its direction when it is nearer to the camera line, nearer to the pivot, or cuts
// deeper into the view.
static void camera_third_person_update_collisions(const vector& start_pos, const vector& end_pos, const vector& camera_desired_direction, float camera_desired_distance)
{
    const vector& dir = camera_desired_direction;
    vector side;
    side.x = (float)((double)dir.z * (double)UP_VECTOR.y - (double)dir.y * (double)UP_VECTOR.z);
    side.y = (float)((double)dir.x * (double)UP_VECTOR.z - (double)dir.z * (double)UP_VECTOR.x);
    side.z = (float)((double)dir.y * (double)UP_VECTOR.x - (double)dir.x * (double)UP_VECTOR.y);
    for (int i = 0; i < 4; i++)
    {
        vector cast_dir = dir;
        if (i == 0)
        {
            cast_dir.rotate_around_vector(UP_VECTOR, predictive_cast_angle);
        }
        else if (i == 1)
        {
            cast_dir.rotate_around_vector(UP_VECTOR, -1.1f);
        }
        else if (i == 2)
        {
            cast_dir.rotate_around_vector(side, predictive_cast_angle);
        }
        else
        {
            cast_dir.rotate_around_vector(side, -1.1f);
        }
        const vector cast_end = start_pos + cast_dir * camera_desired_distance;
        const float cast_dot = (float)((double)dir.x * (double)cast_dir.x + (double)dir.y * (double)cast_dir.y + (double)cast_dir.z * (double)dir.z);
        vector hit;
        if (!camera_collision_test(start_pos, cast_end, &hit, Camera_collision_size_3rd_person, COLLIDE_RAY_DEFAULT))
        {
            continue;
        }
        const float hit_dist = camera_third_person_obstruction_dist(start_pos, hit);
        camera_cached_collision* cache = Camera_cached_collisions[i];
        // [2]: the nearest to the camera line
        if (!cache[2].valid)
        {
            cache[2].set(hit);
        }
        else
        {
            const float hit_line_dist = vec_find_closest_dist_squared_to_linesegment(hit, start_pos, end_pos, nullptr);
            if (hit_line_dist < vec_find_closest_dist_squared_to_linesegment(cache[2].pos, start_pos, end_pos, nullptr))
            {
                cache[2].set(hit);
            }
        }
        // [1]: the nearest to the pivot
        if (!cache[1].valid || hit_dist < camera_third_person_obstruction_dist(cache[1].pos, start_pos))
        {
            cache[1].set(hit);
        }
        // [0]: the deepest into the view
        if (!cache[0].valid)
        {
            cache[0].set(hit);
            continue;
        }
        vector to_cache = cache[0].pos - start_pos;
        const float cache_mag = to_cache.mag_and_normalize_safe();
        const float cache_dist = cache_mag - Camera_collision_size_3rd_person;
        const float cache_dot = (float)((double)to_cache.y * (double)cast_dir.y + (double)to_cache.x * (double)cast_dir.x + (double)to_cache.z * (double)cast_dir.z);
        const float cache_view_dot = (float)((double)to_cache.x * (double)dir.x + (double)dir.y * (double)to_cache.y + (double)to_cache.z * (double)dir.z);
        if (cast_dot <= cache_view_dot)
        {
            if (camera_third_person_interpolate(cache_dot, camera_desired_distance, hit_dist) < cache_dist)
            {
                cache[0].set(hit);
            }
        }
        // Original (as decompiled): any cached obstruction in front of the pivot is replaced.
        else if (hit_dist < camera_third_person_interpolate(cache_dot, camera_desired_distance, cache_dist) || 0.0f < cache_dist)
        {
            cache[0].set(hit);
        }
    }
}

// 0x006DC720 (:409)
// LTCG: offset in EAX, direction in ECX. From the shoulder pivot (the target plus half the offset sideways, 75% in
// cover; pulled in when the pivot itself is inside something) back along the view direction to the camera: the
// nearest obstruction (cast now or cached) limits the distance as a percentage that snaps in, eases in fast and out
// slowly. A rhino backpack charge (or no collision) puts the camera at the full distance.
static void camera_third_person_adjust_for_obstruction(const vector& target_pos, const vector& direction, const float& distance, const vector& offset, vector& final_pos)
{
    player* pp = nullptr;
    if (Rfg_camera.target_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(Rfg_camera.target_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == 3)
        {
            pp = (player*)objp;
        }
    }
    if ((pp != nullptr && pp->backpack != nullptr && pp->backpack->get_backpack_abilities()->rhino) || !Camera_collision_enable)
    {
        final_pos = target_pos + offset - direction * distance;
        return;
    }
    // Original: a target that is not the player dereferences null here.
    // Magic numbers: the pivot is half the sideways offset out (75% in cover).
    const float side_scale = pp->cur_state == HS_COVER ? 0.75f : 0.5f;
    const vector pivot_offset((float)((double)offset.x * (double)side_scale), offset.y, (float)((double)offset.z * (double)side_scale));
    const vector back = direction * distance;
    const vector to_camera((offset.x - pivot_offset.x) - back.x, (pivot_offset.y - pivot_offset.y) - back.y, (offset.z - pivot_offset.z) - back.z);
    const vector to_camera_dir = to_camera.copy_normalize_safe();
    const float full_dist = (float)sqrt((double)to_camera.y * (double)to_camera.y + (double)to_camera.x * (double)to_camera.x + (double)to_camera.z * (double)to_camera.z);
    vector pivot = target_pos + pivot_offset;
    const vector probe = target_pos + pivot_offset * 0.9f;
    vector hit;
    if (camera_collision_test(probe, pivot, &hit, Camera_collision_size_3rd_person, COLLIDE_RAY_DEFAULT))
    {
        const float d = camera_third_person_obstruction_dist(target_pos, hit);
        pivot = target_pos + pivot_offset.copy_normalize_safe() * d;
    }
    const vector cast_end = pivot + to_camera;
    camera_third_person_update_collisions(pivot, cast_end, to_camera_dir, full_dist);
    float allowed = full_dist;
    if (camera_collision_test(pivot, cast_end, &hit, Camera_collision_size_3rd_person, COLLIDE_RAY_DEFAULT))
    {
        Camera_cached_collisions[4][1].set(hit);
        allowed = camera_third_person_obstruction_dist(pivot, hit);
    }
    for (int i = 0; i < 5; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            camera_cached_collision& cache = Camera_cached_collisions[i][j];
            if (!cache.valid)
            {
                continue;
            }
            vector to_cache = cache.pos - pivot;
            const float mag = to_cache.mag_and_normalize_safe();
            const float dot = (float)((double)to_camera_dir.y * (double)to_cache.y + (double)to_cache.x * (double)to_camera_dir.x + (double)to_camera_dir.z * (double)to_cache.z);
            // Magic numbers: obstructions beyond 3 m or behind the view (dot below 0.01) are forgotten.
            if (3.0f < mag - Camera_collision_size_3rd_person || dot < 0.01f)
            {
                cache.valid = false;
            }
            else
            {
                const float limit = camera_third_person_interpolate(dot, full_dist, mag - Camera_collision_size_3rd_person);
                if (limit < allowed)
                {
                    allowed = limit;
                }
            }
        }
    }
    const float percent = allowed / full_dist;
    if (percent < Camera_collision_percent - Camera_collision_percent_snap)
    {
        Camera_collision_percent = percent;
    }
    if (percent <= Camera_collision_percent)
    {
        if (Camera_collision_percent > percent)
        {
            Camera_collision_percent = (float)(((double)percent - (double)Camera_collision_percent) * (double)Camera_collision_percent_rate_in_3rd_person * (double)Frametime + (double)Camera_collision_percent);
            if (!(percent < Camera_collision_percent) && Camera_collision_percent != percent)
            {
                Camera_collision_percent = percent;
            }
        }
    }
    else
    {
        Camera_collision_percent = (float)(((double)percent - (double)Camera_collision_percent) * (double)Camera_collision_percent_rate_out_3rd_person * (double)Frametime + (double)Camera_collision_percent);
        if (!(Camera_collision_percent < percent) && Camera_collision_percent != percent)
        {
            Camera_collision_percent = percent;
        }
    }
    final_pos = to_camera * Camera_collision_percent + pivot;
    if (!Camera_collision_alpha_enable)
    {
        return;
    }
    // The player fades while the camera is inside it (a larger sphere to fade back in).
    const float radius = (Camera_player_collision ? cam_alpha_scale_out : cam_alpha_scale_in) * Camera_collision_size_3rd_person;
    if (pp == nullptr || pp != player_get_local_player())
    {
        return;
    }
    const object_hit_location location = human_get_point_hit_location(pp, final_pos, radius, nullptr);
    if (location == HIT_NONE || location == HIT_GENERAL)
    {
        Camera_player_collision = false;
        Camera_collision_alpha = Frametime / alpha_blend_time + Camera_collision_alpha;
    }
    else
    {
        Camera_player_collision = true;
        Camera_collision_alpha = Camera_collision_alpha - Frametime / alpha_blend_time;
    }
    if (Camera_collision_alpha - 0.0f < 0.0f)
    {
        Camera_collision_alpha = 0.0f;
    }
    if (Camera_collision_alpha - 1.0f >= 0.0f)
    {
        Camera_collision_alpha = 1.0f;
    }
    pp->set_opacity_plus_child_items(Camera_collision_alpha);
}

// The cover offset blended up by the lean animation's blend tree input (inlined three times).
static const vector& camera_third_person_blend_offset(player* pp, const vector& low, const vector& high)
{
    // Magic number: 0x409, the cover lean blend tree input.
    const float t = vanim_get_blend_tree_ramped_input(pp->char_inst->vp, 0x409);
    const float s = 1.0f - t;
    Camera_blended_offset.z = low.z * s + high.z * t;
    Camera_blended_offset.y = low.y * s + high.y * t;
    Camera_blended_offset.x = low.x * s + high.x * t;
    return Camera_blended_offset;
}

// 0x006CEAF0 (:649)
// The shoulder offset of the player's state: normal, fine aim (close), scope, crouched, sprinting (the left ones
// after a shoulder switch), and the cover offsets by the cover movement (standing or crouched); another player in
// multiplayer uses the multiplayer camera's offset.
static const vector& camera_third_person_offset()
{
    player* pp = nullptr;
    if (Rfg_camera.target_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(Rfg_camera.target_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == 3)
        {
            pp = (player*)objp;
        }
    }
    const bool left = Camera_shoulder_switch;
    const vector* offset = left ? &Camera_third_person_offset_left : &Camera_third_person_offset;
    if (pp == nullptr)
    {
        return *offset;
    }
    if (pp->cur_state != HS_COVER)
    {
        if (player_is_fine_aimed(pp))
        {
            if (human_move_is_crouching(pp))
            {
                offset = left ? &Camera_third_person_offset_crouch_close_left : &Camera_third_person_offset_crouch_close;
            }
            else
            {
                offset = left ? &Camera_third_person_offset_close_left : &Camera_third_person_offset_close;
            }
        }
        else if (player_is_scoped(pp))
        {
            if (human_move_is_crouching(pp))
            {
                offset = left ? &Camera_third_person_offset_crouch_scope_left : &Camera_third_person_offset_crouch_scope;
            }
            else
            {
                offset = left ? &Camera_third_person_offset_scope_left : &Camera_third_person_offset_scope;
            }
        }
        else if (human_move_is_crouching(pp))
        {
            offset = left ? &Camera_third_person_offset_crouch_left : &Camera_third_person_offset_crouch;
        }
        else if (pp->pflags.do_sprint)
        {
            offset = left ? &Camera_third_person_offset_sprint_left : &Camera_third_person_offset_sprint;
        }
    }
    else if (pp->cur_stance == HST_STAND && !pp->hflags.crouch_cover)
    {
        switch (pp->movement_submode)
        {
        case HMSM_COVER_RIGHT:
            offset = &Camera_cover_offset_right;
            break;
        case HMSM_COVER_RIGHT_EDGE:
        case HMSM_COVER_RIGHT_BLINDFIRE:
            offset = &Camera_cover_offset_right_edge;
            break;
        case HMSM_COVER_LEFT:
            offset = &Camera_cover_offset_left;
            break;
        case HMSM_COVER_LEFT_EDGE:
        case HMSM_COVER_LEFT_BLINDFIRE:
            offset = &Camera_cover_offset_left_edge;
            break;
        case HMSM_COVER_RIGHT_LEAN:
            offset = &Camera_cover_offset_right_lean;
            break;
        case HMSM_COVER_LEFT_LEAN:
            offset = &Camera_cover_offset_left_lean;
            break;
        default:
            break;
        }
    }
    else
    {
        switch (pp->movement_submode)
        {
        case HMSM_COVER_RIGHT:
            offset = &Camera_cover_crouch_offset_right;
            break;
        case HMSM_COVER_RIGHT_EDGE:
        case HMSM_COVER_RIGHT_BLINDFIRE:
            offset = &Camera_cover_crouch_offset_right_edge;
            break;
        case HMSM_COVER_LEFT:
            offset = &Camera_cover_crouch_offset_left;
            break;
        case HMSM_COVER_LEFT_EDGE:
        case HMSM_COVER_LEFT_BLINDFIRE:
            offset = &Camera_cover_crouch_offset_left_edge;
            break;
        case HMSM_COVER_RIGHT_LEAN:
            offset = &Camera_cover_crouch_offset_right_lean;
            break;
        case HMSM_COVER_LEFT_LEAN:
            offset = &Camera_cover_crouch_offset_left_lean;
            break;
        case HMSM_COVER_RIGHT_LEANUP:
            offset = &camera_third_person_blend_offset(pp, Camera_cover_crouch_offset_right_lean2, Camera_cover_crouch_offset_right_lean2_high);
            break;
        case HMSM_COVER_LEFT_LEANUP:
            offset = &camera_third_person_blend_offset(pp, Camera_cover_crouch_offset_left_lean2, Camera_cover_crouch_offset_left_lean2_high);
            break;
        case HMSM_COVER_RIGHT_BLINDFIRE2:
            offset = &camera_third_person_blend_offset(pp, Camera_cover_crouch_offset_right, Camera_cover_crouch_offset_right_bf2_high);
            break;
        case HMSM_COVER_LEFT_BLINDFIRE2:
            offset = &camera_third_person_blend_offset(pp, Camera_cover_crouch_offset_left, Camera_cover_crouch_offset_left_bf2_high);
            break;
        default:
            break;
        }
    }
    if (Multiplayer && player_get_local_player() != pp)
    {
        offset = multi_game_camera_get_third_person_offset(pp);
    }
    return *offset;
}

// 0x006CF0C0 (:805)
// Pushed under a slew camera if one is active; the look angles start from the current orientation, the offset at the
// player's state, the obstruction caches empty.
void camera_start_third_person(bool smooth)
{
    if (Rfg_camera.mode == CAMERA_SLEW_MODE)
    {
        camera_pop_mode(CAMERA_SLEW_MODE);
        camera_push_mode(CAMERA_THIRD_PERSON_MODE);
        camera_push_mode(CAMERA_SLEW_MODE);
    }
    else
    {
        camera_push_mode(CAMERA_THIRD_PERSON_MODE);
    }
    float pitch;
    float bank;
    float heading;
    Rfg_camera.real_orient.extract_angles(pitch, bank, heading);
    Rfg_camera.lookaround_params.heading = heading;
    Rfg_camera.lookaround_params.pitch = pitch;
    if (smooth)
    {
        camera_trans_start_linear_transition(0.4f, 7, true); // inlined in the binary
    }
    Rfg_camera.lookaround_params.recoiling = 0;
    Rfg_camera.lookaround_params.recoil_start_pitch = 0.0f;
    Rfg_camera.third_person_params.cur_offset = camera_third_person_offset();
    Rfg_camera.third_person_params.interpolation_time = -1.0f;
    Camera_collision_percent = 1.0f;
    Camera_collision_alpha = 1.0f;
    Rfg_camera.lookaround_params.rot_p_multiplier = 1.0f;
    Rfg_camera.lookaround_params.rot_h_multiplier = 1.0f;
    Camera_player_collision = false;
    Rfg_camera.third_person_params.ragdoll_offset = 0.0f;
    Rfg_camera.third_person_params.desired_offset = Rfg_camera.third_person_params.cur_offset;
    camera_first_person_update_camera_sensitivity();
    for (int i = 0; i < 5; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            Camera_cached_collisions[i][j].valid = false;
        }
    }
    Camera_shoulder_switch = false;
}

// 0x006CF250 (:857)
void camera_stop_third_person(bool smooth)
{
    if (!camera_mode_is_on_stack(CAMERA_THIRD_PERSON_MODE)) // inlined in the binary
    {
        return;
    }
    camera_pop_mode(CAMERA_THIRD_PERSON_MODE);
    if (smooth)
    {
        camera_trans_start_linear_transition(0.4f, 7, true); // inlined in the binary
    }
}

// 0x006DD250 (:872)
// The ideal orientation from the look angles (the look-at direction while flag 8 is set; a ladder keeps the heading
// within Camera_ladder_tolerance of the ladder's), the shoulder offset eased toward the state's over the view
// transition (or half its change in seconds, at most 0.25), the pivot at the player (its camera bone when animated;
// lowered while ragdolled), and the camera behind it (at the head when scoped). While entering a vehicle the look angles
// aim from the player at the vehicle's enter offset (camera settings 0); a vehicle without one in its table puts the
// camera behind the player's shoulder, looking at the head, for that frame.
void camera_third_person_process(float step_time, bool last_step_of_frame)
{
    player_autoaim_apply_assisted_aiming();
    human* hp = nullptr;
    if (Rfg_camera.target_handle != 0xFFFFFFFF)
    {
        hp = (human*)World.object_from_handle(Rfg_camera.target_handle);
    }
    if (hp == nullptr || hp->get_obj_type() != OT_HUMAN || hp->get_sub_type() != 3 || !hp->streamed())
    {
        return;
    }
    if (controls_is_button_just_pressed(CBA_SHOULDER_SWITCH, Player_controller_index))
    {
        Camera_shoulder_switch = !Camera_shoulder_switch;
    }
    player* pp = (player*)hp;
    vehicle* vp = World.object_from_handle<vehicle>(pp->vehicle_handle);
    float pitch;
    float heading;
    if (vp != nullptr && human_in_vehicle_transition(pp, vp, VI_INTERACTION_ENTER))
    {
        const vehicle_camera_settings& settings = vp->info->camera_settings[0];
        // The point midway between this frame's and the last frame's player position, at the settings' height.
        const vector lookfrom = (pp->pos + pp->last_pos) * 0.5f + vector(0.0f, settings.lookfrom_height, 0.0f);
        vector lookat = settings.lookat_vehicle_enter_offset;
        if (Vehicle_enter_lookat_x != 0.0f || Vehicle_enter_lookat_y != 0.0f || Vehicle_enter_lookat_z != 0.0f)
        {
            lookat = vector(Vehicle_enter_lookat_x, Vehicle_enter_lookat_y, Vehicle_enter_lookat_z);
        }
        // Magic number: the "no offset" value vehicle_table_parser.cpp gives a vehicle without one in its table.
        if (lookat.x == 9.080706e+08f)
        {
            // Magic numbers: 0.25 m right, 1.8 m up and 1 m behind the player. The return skips the view's fov and focus.
            vector pos(0.25f, 1.8f, -1.0f);
            vec_change_to_global_coords(pos, pp->pos, pp->orient);
            Rfg_camera.ideal_pos = pos;
            human_get_head_pos(pp, pos); // the same local in the binary, keeps the camera position without a head
            vector to_head = pos - Rfg_camera.real_pos;
            to_head.normalize_safe();
            Rfg_camera.ideal_orient.make_quick(to_head);
            camera_trans_start_linear_transition(0.16f, 7, false); // Magic number: 0.16 s
            return;
        }
        if (settings.enter_distance != 0.0f)
        {
            Rfg_camera.m_camera_view_data.set_temp_distance_override(settings.enter_distance); // inlined in the binary
        }
        lookat.unrotate(vp->orient);
        if (Vehicle_enter_lookat_debug)
        {
            debug_lookat_pos = lookat;
        }
        vector look = (vp->pos + lookat) - lookfrom;
        look.normalize();
        pitch = -(float)asin((double)look.y); // x87 __CIasin
        heading = fl_atan2_safe(look.x, look.z);
    }
    else if ((Rfg_camera.flags & 8) == 0)
    {
        pitch = Rfg_camera.lookaround_params.pitch;
        heading = Rfg_camera.lookaround_params.heading;
    }
    else
    {
        vector look;
        human_get_look_at_vector(hp, look);
        look.normalize();
        pitch = -(float)asin((double)look.y); // x87 __CIasin
        heading = fl_atan2_safe(look.x, look.z);
    }
    if (pp->hflags.is_on_ladder && pp->ladder_handle != 0xFFFFFFFF)
    {
        object* lp = World.object_from_handle(pp->ladder_handle);
        if (lp != nullptr && lp->get_obj_type() == OT_LADDER)
        {
            vector fvec = lp->orient.fvec;
            fvec.normalize_safe();
            // Magic numbers: 2 pi and pi.
            float ladder_heading = fl_atan2_safe(fvec.x, fvec.z);
            if (ladder_heading < 0.0f)
            {
                ladder_heading = ladder_heading + 6.28318548f;
            }
            float diff = ladder_heading - heading;
            if (diff > 3.14159274f)
            {
                diff = diff - 6.28318548f;
            }
            else if (diff < -3.14159274f)
            {
                diff = diff + 6.28318548f;
            }
            if (fl_greater_than_robust(fabsf(diff), Camera_ladder_tolerance, 0.001f))
            {
                // Original: only the lookaround heading turns; this frame keeps the heading read above.
                Rfg_camera.lookaround_params.heading = diff <= 0.0f ? Camera_ladder_tolerance + ladder_heading : ladder_heading - Camera_ladder_tolerance;
                if (!camera_trans_active())
                {
                    camera_trans_start_linear_transition(0.4f, 7, false);
                }
            }
        }
    }
    thirdperson_mode_params& params = Rfg_camera.third_person_params;
    const vector target_offset = camera_third_person_offset();
    if (target_offset.x != params.cur_offset.x || target_offset.y != params.cur_offset.y || target_offset.z != params.cur_offset.z)
    {
        if (target_offset.x != params.desired_offset.x || target_offset.y != params.desired_offset.y || target_offset.z != params.desired_offset.z)
        {
            // Magic numbers: half the offset change in seconds, at most 0.25 s.
            params.interpolation_time = (params.cur_offset - target_offset).mag() * 0.5f;
            if (0.25f - params.interpolation_time < 0.0f)
            {
                params.interpolation_time = 0.25f;
            }
            params.desired_offset = target_offset;
        }
        if (camera_view_current_transition_time_left() > 0)
        {
            params.interpolation_time = (float)camera_view_current_transition_time_left() * 0.001f;
        }
        if (Frametime <= params.interpolation_time)
        {
            const float ft = Frametime;
            const float t = params.interpolation_time;
            params.cur_offset.x = (float)(((double)target_offset.x - (double)params.cur_offset.x) * (double)ft / (double)t + (double)params.cur_offset.x);
            params.cur_offset.y = (float)(((double)target_offset.y - (double)params.cur_offset.y) * (double)ft / (double)t + (double)params.cur_offset.y);
            params.cur_offset.z = (float)(((double)target_offset.z - (double)params.cur_offset.z) * (double)ft / (double)t + (double)params.cur_offset.z);
            params.interpolation_time = params.interpolation_time - Frametime;
        }
        else
        {
            params.cur_offset = target_offset;
            params.interpolation_time = -1.0f;
        }
    }
    // The offset turns with the heading; its depth follows the pitch.
    vector offset(params.cur_offset.x, params.cur_offset.y, -Rfg_camera.real_orient.fvec.y * params.cur_offset.z);
    offset.unrotate(matrix(0.0f, 0.0f, heading));
    const float distance = Rfg_camera.m_camera_view_data.m_camera_distance;
    Rfg_camera.ideal_orient.set(pitch, 0.0f, heading);
    vector pivot = hp->pos;
    const int camera_bone = hp->h_info->camera_bone_index;
    if (camera_bone > -1 && !Multiplayer && animlib_is_bone_animating(hp->char_inst->aim_handle, camera_bone))
    {
        matrix43* matrices = nullptr;
        if (animlib_get_instanced_model_matrices(hp->char_inst->aim_handle, &matrices))
        {
            vector bone_pos = matrices[hp->h_info->camera_bone_index].m_translation;
            bone_pos.unrotate(hp->orient);
            pivot = bone_pos + pivot;
        }
    }
    float ragdoll_offset = max_ragdoll_offset;
    if (hp->movement_submode != HMSM_KNOCKDOWN_RAGDOLL)
    {
        ragdoll_offset = params.ragdoll_offset - ragdoll_offset_return_rate * Frametime;
        if (ragdoll_offset - 0.0f < 0.0f)
        {
            ragdoll_offset = 0.0f;
        }
    }
    pivot.y = pivot.y - ragdoll_offset;
    params.ragdoll_offset = ragdoll_offset;
    if (player_is_scoped(pp))
    {
        vector head = ZERO_VECTOR;
        human_get_head_pos(pp, head);
        if (pp->cur_state == HS_COVER)
        {
            Rfg_camera.ideal_pos.y = head.y;
            Rfg_camera.ideal_pos.x = offset.x * scaler + pp->pos.x;
            Rfg_camera.ideal_pos.z = offset.z * scaler + pp->pos.z;
        }
        else
        {
            // Magic number: 0.15 m right of the head.
            Rfg_camera.ideal_pos.y = Rfg_camera.ideal_orient.rvec.y * 0.15f + head.y;
            Rfg_camera.ideal_pos.x = Rfg_camera.ideal_orient.rvec.x * 0.15f + pp->pos.x;
            Rfg_camera.ideal_pos.z = Rfg_camera.ideal_orient.rvec.z * 0.15f + pp->pos.z;
        }
    }
    else
    {
        camera_third_person_adjust_for_obstruction(pivot, Rfg_camera.ideal_orient.fvec, distance, offset, Rfg_camera.ideal_pos);
    }
    Rfg_camera.m_camera_view_data.update_fov(); // inlined in the binary
}
