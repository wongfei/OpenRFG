// Original: volition/rfg/code/multi/multi_game/multi_game_object_vehicle_walker.cpp
//
// Partial (final_push.md step 7): what walker_do_frame_presim calls. The server follows a walker driven by a remote
// player (is_client_authoritative, apply_sim_state with its steering correction) and cuts the movers ahead of a moving
// walker (multi_game_walker_predict_collision). Not ported: the walker registration and its state pools, as
// multi_game_object_vehicle_walker_init has no caller in rfg.exe (dead), so no walker ever gets an mp_sim_state and
// is_client_authoritative is always false there.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_object_vehicle_walker.h"

#include "volition/rfg/code/destruction/rfg_shape_cutter.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_qos.h"
#include "volition/rfg/code/multi/multi_game/multi_game_player.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_component.h"
#include "volition/rfg/code/vehicle/walker.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/frametime.h"

#include <math.h>

// ---- file-static globals
static bool MP_walker_sphere_destruction = true; // 0x0167F10E
static multi_core_state_priority State_priority_base_walker[8]; // 0x0167F22C, initial data 0200000002000000020000000200000002000000020000000000000003000000
static unsigned int State_mask_walker[8] = {0x1000, 7, 8, 0x3F0, 0xC00, 0x2000, 0x4000, 0x8000}; // 0x0167F24C
static int Mp_walker_destruction_base_look_ahead_time = 100; // 0x0167F26C
static int Mp_walker_destruction_client_extra_lookahead = 70; // 0x0167F270
static float Walker_radius = 3.0f; // 0x0167F278
static float Mp_vehicle_walker_correction_factor = 0.5f; // 0x0167F27C
static float Mp_vehicle_walker_correction_max = 0.5f; // 0x0167F280
static float Mp_vehicle_walker_correction_threshold = 0.35f; // 0x0167F284
static timestamp Rest_timestamp_walker; // 0x016931E0, initial data ffffffff
static multi_pool<object_state_data<walker_data_bundle>,100> Walker_object_allocator; // 0x016931E8
static multi_pool<multi_sim_state<multi_walker_state>,32> Walker_sim_state_allocator; // 0x01693228
// Initial data 0, 2.5, 0.8 (rfg.exe .data): the point ahead of which the walker cuts, in its own space.
static vector Center_offset(0.0f, 2.5f, 0.8f); // 0x01693268
static walker_multi_registration* Walker_mr; // 0x02FE6D84
// unsigned int Clock_current_time (0x02FEB538) is used here too; placed in multi.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// $S7 (0x03017C30) is the init guard of multi_game_walker_predict_collision's function-static last_pos (0x03017C24).

// ---- internal functions
static object_state_data<walker_data_bundle>* walker_state_data_alloc(); // 0x009F67D0 (:345)
static multi_sim_state<multi_walker_state>* walker_alloc_sim_state(); // 0x0099F820 (:358)
static void debug_spew_walker(const char*, ...); // 0x0095E690 (:399)
static void debug_mp_walker(const char*, ...); // 0x0095E6A0 (:403)
// LTCG: arguments in ECX (not mapped yet).
static unsigned short query_possible_mask_bits_base(const walker_data_bundle& cur_data, const walker_data_bundle& ref_data); // 0x0095E6D0 (:413)
static bool walker_cast_filter_callback(object* objp, void* rhinoer); // 0x0095F240 (:2189)

// The vehicle_component flag bits the client's component sync tests (vehicle_class.cpp keeps their names file-local).
static const unsigned int MP_WALKER_VCF_UNLATCHED = 0x10000;
static const unsigned int MP_WALKER_VCF_DETACHED = 0x20000;
static const unsigned int MP_WALKER_VCF_SHATTERED = 0x80000;

// 0x0095E4F0 (:370)
// When the walker is more than the threshold away from the server's position, a steering push towards it in the
// walker's own space: the factor times the local offset, x and z clamped to the maximum, all three negated.
// The arithmetic is rfg.exe's: each product and difference in double, rounded to float.
void get_steering_correction(vector& steering_correction, const walker* wp, const vector& pos_diff)
{
    const float dist_sq = (float)((double)pos_diff.x * (double)pos_diff.x + (double)pos_diff.y * (double)pos_diff.y + (double)pos_diff.z * (double)pos_diff.z);
    if (!((double)dist_sq > (double)Mp_vehicle_walker_correction_threshold * (double)Mp_vehicle_walker_correction_threshold))
    {
        steering_correction = ZERO_VECTOR;
        return;
    }
    vector local = pos_diff;
    local.rotate(wp->orient);
    float x = (float)((double)Mp_vehicle_walker_correction_factor * (double)local.x);
    const float y = (float)((double)Mp_vehicle_walker_correction_factor * (double)local.y);
    const float z_scaled = (float)((double)Mp_vehicle_walker_correction_factor * (double)local.z);
    const float min = -Mp_vehicle_walker_correction_max;
    const float max = Mp_vehicle_walker_correction_max;
    // The clamps compare differences against 0 (COMISS after the double subtraction), as rfg.exe's clamp inlines.
    if (!((float)((double)x - (double)min) >= 0.0f))
    {
        x = min;
    }
    if ((float)((double)x - (double)max) >= 0.0f)
    {
        x = max;
    }
    float z = min;
    if ((float)((double)z_scaled - (double)min) >= 0.0f)
    {
        z = z_scaled;
    }
    if ((float)((double)z - (double)max) >= 0.0f)
    {
        z = max;
    }
    steering_correction.x = -x;
    steering_correction.y = -y;
    steering_correction.z = -z;
}

// 0x0099FE10 (:2000): the server follows a walker with a sim state that a remote player drives.
bool multi_game_object_vehicle_walker_is_client_authoritative(const vehicle* vp)
{
    if (vp == nullptr)
    {
        return false;
    }
    if (vp->get_obj_type() != OT_VEHICLE || vp->get_sub_type() != OT_SUB_VEHICLE_WALKER || ((const walker*)vp)->mp_sim_state == nullptr)
    {
        return false;
    }
    const human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
    for (int i = 0; i < World.get_num_objects<player>(); i++)
    {
        player* pp = World.get_object<player>(i);
        if (driver == pp)
        {
            return !object_is_local_player(pp);
        }
    }
    return false;
}

// 0x00980D40 (:2020)
// A replicated state on a walker the local player does not drive. Far (over 10 m, or 2 m in height) or out of the
// player's view, and with nothing in the walker's bounding box moved to the state's position, it teleports there;
// otherwise a height difference over 0.0001 is closed by an exp lerp of 95% per second. The driver's steering is
// replayed from the state's steering vector along his flattened camera axes (signed square roots), plus the steering
// correction, and his attacks by their ids. A client also takes the hit points, fire, a blow-up and the components.
void multi_game_object_vehicle_walker_apply_sim_state(const multi_walker_state* walker_state, walker* vwp)
{
    if (vehicle_is_corpse(vwp))
    {
        return;
    }
    // rfg.exe reads the rigid body's position, rotation and velocities into locals it never uses (dead loads through
    // vehicle_get_rigid_body; a walker without a body would crash there). They are left out.
    player* local_player = player_get_local_player();
    human* driver_human = vehicle_passenger_in_seat(vwp, VSI_DRIVER);
    player* driver = nullptr;
    if (driver_human != nullptr && driver_human->get_obj_type() == OT_HUMAN && driver_human->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        driver = (player*)driver_human;
    }
    if (driver != local_player)
    {
        vector pos_diff;
        pos_diff.x = (float)((double)vwp->pos.x - (double)walker_state->position.x);
        pos_diff.y = (float)((double)vwp->pos.y - (double)walker_state->position.y);
        pos_diff.z = (float)((double)vwp->pos.z - (double)walker_state->position.z);
        const float dist = (float)sqrt((double)(float)((double)pos_diff.y * (double)pos_diff.y + (double)pos_diff.x * (double)pos_diff.x + (double)pos_diff.z * (double)pos_diff.z));
        bool lerp = false;
        // Magic numbers: 10 m away or 2 m in height (rfg.exe float constants)
        if (!(dist > 10.0f) && !((float)fabs((double)pos_diff.y) > 2.0f) && multi_game_player_can_view_object(nullptr, vwp))
        {
            lerp = true;
        }
        else
        {
            vector bmin;
            vector bmax;
            vwp->get_bbox(bmin, bmax);
            vector delta;
            delta.x = (float)((double)walker_state->position.x - (double)vwp->pos.x);
            delta.y = (float)((double)walker_state->position.y - (double)vwp->pos.y);
            delta.z = (float)((double)walker_state->position.z - (double)vwp->pos.z);
            bmin.x = (float)((double)bmin.x + (double)delta.x);
            bmin.y = (float)((double)bmin.y + (double)delta.y);
            bmin.z = (float)((double)bmin.z + (double)delta.z);
            bmax.x = (float)((double)bmax.x + (double)delta.x);
            bmax.y = (float)((double)bmax.y + (double)delta.y);
            bmax.z = (float)((double)bmax.z + (double)delta.z);
            unsigned int blocker;
            if (havok_collect_objects_fast(bmin, bmax, &blocker, 1, COLLIDE_OBJECT_COLLECTOR_VEHICLES_ONLY, nullptr, nullptr, nullptr) != 0)
            {
                lerp = true;
            }
            else
            {
                vwp->pos = walker_state->position;
                vwp->orient = walker_state->orientation;
                vwp->w_flags.teleport_components = 1;
            }
        }
        // Magic number: 0.0001 m, a float constant widened to double
        if (lerp && (double)(float)fabs((double)pos_diff.y) > (double)0.0001f)
        {
            const float t = exp_lerp_factor(0.95f, Frametime); // Magic number: 95% of the way per second
            vector pos;
            pos.x = (float)((double)vwp->pos.x + (double)(float)((double)(float)((double)walker_state->position.x - (double)vwp->pos.x) * (double)t));
            pos.y = (float)((double)vwp->pos.y + (double)(float)((double)(float)((double)walker_state->position.y - (double)vwp->pos.y) * (double)t));
            pos.z = (float)((double)vwp->pos.z + (double)(float)((double)(float)((double)walker_state->position.z - (double)vwp->pos.z) * (double)t));
            vwp->pos = pos;
        }
        if (driver != nullptr)
        {
            vector fvec = driver->mp_camera_orient.fvec;
            vector rvec = driver->mp_camera_orient.rvec;
            fvec.flatten_normalize_safe();
            rvec.flatten_normalize_safe();
            const vector& steering = walker_state->steering_vector;
            float forward = (float)((double)steering.y * (double)fvec.y + (double)steering.x * (double)fvec.x + (double)steering.z * (double)fvec.z);
            float side = (float)((double)steering.y * (double)rvec.y + (double)steering.x * (double)rvec.x + (double)steering.z * (double)rvec.z);
            // Signed square roots (rfg.exe takes the root branch unless the value is <= 0, NaN included).
            forward = !(forward <= 0.0f) ? (float)sqrt((double)forward) : -(float)sqrt((double)-forward);
            side = !(side <= 0.0f) ? (float)sqrt((double)side) : -(float)sqrt((double)-side);
            vwp->w_flags.rotating_left = walker_state->turning_left;
            vwp->w_flags.rotating_right = walker_state->turning_right;
            // rfg.exe sets x and z to ZERO_VECTOR's first; get_steering_correction writes all three.
            vector correction;
            correction.x = ZERO_VECTOR.x;
            correction.z = ZERO_VECTOR.z;
            get_steering_correction(correction, vwp, pos_diff);
            walker_steer(vwp, (float)((double)correction.z + (double)forward), (float)((double)correction.x + (double)side));
            if (vwp->mp_data.attack_1_id != walker_state->attack_data.attack_1_id && walker_play_attack(vwp))
            {
                vwp->mp_data.attack_1_id = walker_state->attack_data.attack_1_id;
            }
            if (vwp->mp_data.attack_2_id != walker_state->attack_data.attack_2_id && walker_play_attack2(vwp))
            {
                vwp->mp_data.attack_2_id = walker_state->attack_data.attack_2_id;
            }
            if (vwp->mp_data.attack_3_id != walker_state->attack_data.attack_3_id && walker_play_attack3(vwp))
            {
                vwp->mp_data.attack_3_id = walker_state->attack_data.attack_3_id;
            }
            if (vwp->mp_data.attack_4_id != walker_state->attack_data.attack_4_id && walker_play_attack4(vwp))
            {
                vwp->mp_data.attack_4_id = walker_state->attack_data.attack_4_id;
            }
            if (vwp->mp_data.charge_id != walker_state->attack_data.charge_id && walker_charge(vwp))
            {
                vwp->mp_data.charge_id = walker_state->attack_data.charge_id;
            }
        }
    }
    if (Multiplayer && !Server)
    {
        const unsigned int old_hit_points = vehicle_get_hit_points(vwp);
        vehicle_set_hit_points(vwp->get_handle(), walker_state->hp);
        // rfg.exe compares the unsigned hit points as x87 values against 0: the same as != 0.
        if (walker_state->hp != 0 || walker_state->on_fire)
        {
            vehicle_set_smoking_and_fire(vwp->get_handle(), walker_state->smoking, walker_state->on_fire, false);
        }
        if (old_hit_points != 0 && walker_state->hp == 0)
        {
            vehicle_mp_force_blow_up(vwp);
            return;
        }
        if (vwp->rdd.is_loaded() && walker_state->component_count != 0)
        {
            vehicle::resource_dependent_data* rdd = vwp->rdd.get_data();
            for (unsigned int i = 0; i < walker_state->component_count; i++)
            {
                vehicle_component* component = &rdd->m_components[i];
                if (walker_state->component_unlatched[i] && (component->get_flags() & MP_WALKER_VCF_UNLATCHED) == 0)
                {
                    vehicle_component_unlatch_hinged(component, vwp);
                }
                if (walker_state->component_detached[i] && (component->get_flags() & MP_WALKER_VCF_DETACHED) == 0)
                {
                    vwp->component_detach(component, false, 0, DBT_BULLET, false, false);
                }
                if (walker_state->component_shattered[i] && (component->get_flags() & MP_WALKER_VCF_SHATTERED) == 0)
                {
                    vehicle_component_shatter_glass(component, vwp, nullptr, nullptr, nullptr);
                }
            }
        }
    }
}

// 0x0095F240 (:2189): the walker's cast hits movers only
static bool walker_cast_filter_callback(object* objp, void* rhinoer)
{
    return objp->get_obj_type() == OT_MOVER;
}

// 0x009B13B0 (:2204)
// On the server, at most every 50 ms: a sphere of the walker's radius is cast from a point ahead of the walker
// (Center_offset in its space) along its velocity times the look-ahead (100 ms; for a remote driver plus 70 ms and
// twice his latency, at most 250 ms), once the end has moved 0.15 m from the last start. A mover hit there is cut
// by a shape cutter half a metre beyond the hit, so the server's buildings break before the walker reaches them.
void multi_game_walker_predict_collision(walker* wp)
{
    if (!Multiplayer || !Server || !MP_walker_sphere_destruction)
    {
        return;
    }
    if (wp->mp_collision_prediction.check_time.valid() && !wp->mp_collision_prediction.check_time.elapsed())
    {
        return;
    }
    wp->mp_collision_prediction.check_time.set(50); // Magic number: 50 ms between predictions
    human* driver = vehicle_passenger_in_seat(wp, VSI_DRIVER);
    int look_ahead_ms = Mp_walker_destruction_base_look_ahead_time;
    if (multi_game_object_vehicle_walker_is_client_authoritative(wp))
    {
        player* pp = nullptr;
        if (driver != nullptr && driver->get_obj_type() == OT_HUMAN && driver->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            pp = (player*)driver;
        }
        // Original: a driver that is not a player is read through null (cannot happen: the check above wants one).
        int latency = multi_core_qos_find_latency_by_game_data(pp->mp_data) * 2;
        if (latency >= 250) // Magic number: at most 250 ms of round trip
        {
            latency = 250;
        }
        look_ahead_ms += Mp_walker_destruction_client_extra_lookahead + latency;
    }
    const float look_ahead = (float)((double)(float)look_ahead_ms * (double)0.001f); // ms to s; 0.001 is a widened float
    vector velocity;
    havok_body_get_linear_velocity(wp->havok_handle, velocity);
    static vector last_pos = ZERO_VECTOR; // 0x03017C24 (function static, guard $S7 0x03017C30)
    static float min_delta_pos = 0.15f; // 0x016940A0 (function static, initialized data)
    vector start = Center_offset;
    vec_change_to_global_coords(start, wp->pos, wp->orient);
    vector end;
    end.x = (float)((double)(float)((double)velocity.x * (double)look_ahead) + (double)start.x);
    end.y = (float)((double)(float)((double)velocity.y * (double)look_ahead) + (double)start.y);
    end.z = (float)((double)(float)((double)velocity.z * (double)look_ahead) + (double)start.z);
    vector delta;
    delta.x = (float)((double)end.x - (double)last_pos.x);
    delta.y = (float)((double)end.y - (double)last_pos.y);
    delta.z = (float)((double)end.z - (double)last_pos.z);
    const float moved = (float)sqrt((double)(float)((double)delta.x * (double)delta.x + (double)delta.y * (double)delta.y + (double)delta.z * (double)delta.z));
    if ((double)min_delta_pos > (double)moved)
    {
        return;
    }
    // Original: the last position kept is the start of the cast, while the distance is measured from the end.
    last_pos = start;
    const vector dir = delta.copy_normalize_safe();
    raycast_input input;
    raycast_output output;
    input.start = start;
    input.end = end;
    input.radius = Walker_radius;
    input.filter_callback = walker_cast_filter_callback;
    input.filter_params = driver;
    input.m_filter_info = COLLIDE_RAY_DEFAULT;
    input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    input.m_flags = 0;
    if (!havok_spherecast(input, output) || output.m_hits.getSize() < 1)
    {
        return;
    }
    const raycast_output::hit_info& hit = output.m_hits[0];
    object* objp = World.object_from_handle(hit.hit_object_handle);
    // Original: the zone test is redundant with the mover test after it.
    if (objp == nullptr || objp->get_obj_type() == OT_ZONE || objp->get_obj_type() != OT_MOVER)
    {
        return;
    }
    vector cut_pos;
    cut_pos.x = (float)((double)(float)((double)dir.x * 0.5) + (double)hit.hit_center_pos.x); // Magic number: 0.5 m past the hit
    cut_pos.y = (float)((double)(float)((double)dir.y * 0.5) + (double)hit.hit_center_pos.y);
    cut_pos.z = (float)((double)(float)((double)dir.z * 0.5) + (double)hit.hit_center_pos.z);
    // Magic numbers: explosion info 0xFF (none), no source and no owner
    shape_cutter_props props(cut_pos, wp->orient, Walker_radius, 0xFF, 0xFFFFFFFF, 0xFFFFFFFF);
    World.create_object<shape_cutter>(props, 0xFFFFFFFF);
}
