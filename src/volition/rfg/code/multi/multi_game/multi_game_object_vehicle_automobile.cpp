// Original: volition/rfg/code/multi/multi_game/multi_game_object_vehicle_automobile.cpp
//
// Every live function (final_push.md step 7): the server's side of a networked automobile.
// - multi_game_object_vehicle_automobile_is_client_authoritative: a car with a replicated state that a remote player
//   drives; multi_game_object_vehicle_automobile_apply_sim_state moves it to that player's state (a hard set when far
//   off, unseen or alone, else Havok's soft keyframe) and copies the driver's controls, hit points and components.
// - multi_game_auto_predict_collision: a fast car's swept box over the next look-ahead time (straight, or along the
//   circle its last two velocities turn on) pre-collides the destroyable movers in it (rfg_dyn_add_pre_collide_mp_vehicle).
// The automobile registration (automobile_multi_registration and multi_game_object_vehicle_automobile_init, the state
// pools and the compressed state) is dead in rfg.exe: nothing calls the init or references the vftable, so no car ever
// gets an mp_sim_state and is_client_authoritative is always false there. It is not ported (port-live-code-only).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_object_vehicle_automobile.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/dynamics/keyframe/hkpkeyframeutility.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_qos.h"
#include "volition/rfg/code/multi/multi_game/multi_game_player.h"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_state.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_audio.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_component.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/frametime.h"

#include <math.h>

// size 0x54: one swept box of multi_game_auto_predict_collision (rfg.pdb; start and end are never written)
struct obb_info
{
    vector center; // 0x00
    vector size; // 0x0C
    matrix orient; // 0x18
    vector start; // 0x3C
    vector end; // 0x48
};

// ---- file-static globals
static bool Full_obb = true; // 0x0167F016
static bool Clip_obb = true; // 0x0167F017
// Only the dead registration uses these (left out with it): State_mask_automo 0x0167F1EC, Rest_timestamp_automo
// 0x01693158, Automobile_object_allocator 0x01693160, Sim_state_vehicle_allocator 0x016931A0, Automobile_mr 0x02FE8B04.
static int Mp_veh_destruction_base_look_ahead_time = 100; // 0x0167F218, ms
static int Mp_veh_destruction_client_extra_lookahead = 70; // 0x0167F21C, ms
static float Mp_veh_expansion = 0.4f; // 0x0167F224, m
static float Veh_energy_scalar = 1.0f; // 0x0167F228
// dynamic_uint_hash_table<remote_object*> Network_handle_to_rop_hash (0x01691B88) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_network_to_game_hash (0x01691C60) is used here too; placed in multi_core_delta_compression.cpp.
// unsigned int Clock_current_time (0x02FEB538) is used here too; placed in multi.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.
static float Radius; // 0x02FEB6FC
static vector Center; // 0x02FEB8EC

// rfg.pdb public globals with no source file, placed here by their neighbours (the file statics above).
float Mp_vehicle_factor = 0.25f; // 0x0167F208: the soft keyframe factor is min(1 / this, 1 / frame time)
float Mp_max_acc = 50.0f; // 0x0167F20C: the soft keyframe's maximum linear acceleration
float Mp_max_lv = 200.0f; // 0x0167F210: the largest linear velocity change from the body's, m/s
float Mp_max_av = 10000.0f; // 0x0167F214: the largest angular velocity change from the body's
// The last 50 swept boxes and the last box's ends: written for debug drawing, which the retail build does not have.
int Obb_count; // 0x02FEB6F4
int Obb_pos; // 0x02FEB6F8
vector Box_end; // 0x02FEBB30
obb_info Obb_infos[50]; // 0x02FEC150
vector Box_start; // 0x0300F8B4

// ---- internal functions
// Dead in rfg.exe (only the registration calls them): automobile_alloc_sim_state 0x0099D390 (:444),
// automobile_alloc_state_data 0x009F6320 (:454), debug_mp 0x0095DE90 (:492).
static float get_steering_correction(const automobile* ap); // 0x0099D3C0 (:467)
static void add_obb(const vector& center, const vector& size, const matrix& orient); // 0x009808A0 (:2368)

// 0x0099D3C0 (:467): the heading difference to the replicated state at the network time, mapped from +-9 degrees to
// the car's full steering angle (negated when reversing)
// LTCG: ap in ESI, the result in XMM0.
static float get_steering_correction(const automobile* ap)
{
    float correction = 0.0f;
    if (ap->mp_sim_state == nullptr)
    {
        return correction;
    }
    multi_automobile_state state;
    bool exceeded_extrap_time;
    if (!ap->mp_sim_state->obtain_state(Clock_current_time, &state, exceeded_extrap_time))
    {
        return correction;
    }
    float heading;
    ap->orient.extract_heading(heading);
    float state_heading;
    state.orientation.extract_heading(state_heading);
    correction = (float)((double)state_heading - (double)heading);
    float max_angle;
    float speed_for_max_angle;
    automobile_get_steering_info(ap->get_handle(), max_angle, speed_for_max_angle);
    correction = fl_parameter(correction, -0.15707964f, 0.15707964f, -max_angle, max_angle, true); // Magic numbers: +-PI / 20 (9 degrees)
    if (automobile_moving_in_reverse(ap))
    {
        return -correction;
    }
    return correction;
}

// 0x009808A0 (:2368): the next slot of the 50 box ring
// LTCG: center on the stack, size in EDI, orient in ESI.
static void add_obb(const vector& center, const vector& size, const matrix& orient)
{
    const int pos = Obb_pos;
    Obb_count++;
    if (Obb_count > 49)
    {
        Obb_count = 50;
    }
    Obb_pos = (Obb_pos + 1) % 50;
    Obb_infos[pos].center = center;
    Obb_infos[pos].size = size;
    Obb_infos[pos].orient = orient;
}

// 0x0099D9D0 (:2148): a car with a replicated state whose driver is a remote player
bool multi_game_object_vehicle_automobile_is_client_authoritative(const vehicle* vp)
{
    if (vp == nullptr)
    {
        return false;
    }
    if (vp->get_obj_type() != OT_VEHICLE || vp->get_sub_type() != OT_SUB_VEHICLE_AUTO || ((const automobile*)vp)->mp_sim_state == nullptr)
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

// 0x0099DA50 (:2182)
// Unless the local player drives it: a state more than 10 m (or 2 m vertically) off, or a car no player can see,
// is set at once unless another vehicle is in the way at the state's place; otherwise Havok's soft keyframe pulls it
// there, with its velocities moved towards the state's by at most Mp_max_lv / Mp_max_av. Then the driver's steering
// (plus the heading correction), accelerator, brake and handbrake. A client also takes the hit points, smoke and
// fire, unlatched, detached and shattered components and the horn.
void multi_game_object_vehicle_automobile_apply_sim_state(const multi_automobile_state* auto_state, automobile* vap)
{
    hkpRigidBody* rb = vehicle_get_rigid_body(vap->get_handle());
    // Original: the position and orientation read here are never used (the body is read again below).
    vector unused_pos;
    havok_to_game_vector(unused_pos, rb->getTransform().getTranslation());
    matrix unused_orient;
    havok_to_game_matrix(unused_orient, rb->getTransform().getRotation());
    vector body_linear_velocity;
    havok_to_game_vector(body_linear_velocity, rb->getLinearVelocity());
    vector body_angular_velocity;
    havok_to_game_vector(body_angular_velocity, rb->getAngularVelocity());
    const player* local_player = player_get_local_player();
    if ((const human*)vehicle_passenger_in_seat(vap, VSI_DRIVER) != (const human*)local_player)
    {
        rb = vehicle_get_rigid_body(vap->get_handle());
        const hkVector4f cur_pos = rb->getTransform().getTranslation();
        vector body_pos;
        havok_to_game_vector(body_pos, cur_pos);
        const hkQuaternionf cur_rot = rb->getRotation();
        matrix body_orient;
        havok_to_game_matrix(body_orient, cur_rot);
        const float dx = (float)((double)body_pos.x - (double)auto_state->position.x);
        const float dy = (float)((double)body_pos.y - (double)auto_state->position.y);
        const float dz = (float)((double)body_pos.z - (double)auto_state->position.z);
        const float dist = (float)sqrt((double)(float)((double)dx * (double)dx + (double)dy * (double)dy + (double)dz * (double)dz));
        bool soft = false;
        if (!(dist > 10.0f) && !((float)fabs((double)dy) > 2.0f) && multi_game_player_can_view_object(nullptr, vap)) // Magic numbers: 10 m, 2 m
        {
            soft = true;
        }
        else
        {
            // The car's box moved to the state's place: another vehicle there keeps the soft keyframe.
            vector bmin;
            vector bmax;
            vap->get_bbox(bmin, bmax);
            const float ox = (float)((double)auto_state->position.x - (double)body_pos.x);
            const float oy = (float)((double)auto_state->position.y - (double)body_pos.y);
            const float oz = (float)((double)auto_state->position.z - (double)body_pos.z);
            const vector moved_min((float)((double)ox + (double)bmin.x), (float)((double)bmin.y + (double)oy), (float)((double)bmin.z + (double)oz));
            const vector moved_max((float)((double)bmax.x + (double)ox), (float)((double)bmax.y + (double)oy), (float)((double)bmax.z + (double)oz));
            unsigned int blocker;
            if (havok_collect_objects_fast(moved_min, moved_max, &blocker, 1, COLLIDE_OBJECT_COLLECTOR_VEHICLES_ONLY, nullptr, nullptr, nullptr) != 0)
            {
                soft = true;
            }
            else
            {
                vap->update_pos_and_orient(auto_state->position, auto_state->orientation, true);
                hkVector4f linear_velocity;
                game_to_havok_vector(linear_velocity, auto_state->linear_velocity);
                hkVector4f angular_velocity;
                game_to_havok_vector(angular_velocity, auto_state->angular_velocity);
                rb->setLinearVelocity(linear_velocity); // inlined: activate, then the motion's setLinearVelocity
                rb->setAngularVelocity(angular_velocity);
            }
        }
        if (soft)
        {
            hkRotationf target_rotation;
            game_to_havok_matrix(target_rotation, auto_state->orientation);
            hkQuaternionf target_rot;
            target_rot.set(target_rotation);
            hkVector4f target_pos;
            game_to_havok_vector(target_pos, auto_state->position);
            hkpKeyFrameUtility::KeyFrameInfo key_frame_info;
            key_frame_info.fastSetUsingPositionOrientationPair(cur_pos, cur_rot, target_pos, target_rot, (float)(1.0 / (double)Frametime));
            float factor = (float)(1.0 / (double)Mp_vehicle_factor);
            if ((float)((double)factor - 0.0) < 0.0f)
            {
                factor = 0.0f;
            }
            const float inv_frametime = (float)(1.0 / (double)Frametime);
            if (!((float)((double)factor - (double)inv_frametime) < 0.0f))
            {
                factor = inv_frametime;
            }
            hkpKeyFrameUtility::AccelerationInfo accel_info;
            const __m128 factors = _mm_setr_ps(factor, factor, factor, 0.0f);
            accel_info.m_linearPositionFactor.m_quad = factors;
            accel_info.m_angularPositionFactor.m_quad = factors;
            accel_info.m_linearVelocityFactor.m_quad = factors;
            accel_info.m_angularVelocityFactor.m_quad = factors;
            accel_info.m_maxLinearAcceleration = Mp_max_acc;
            vector linear_velocity = auto_state->linear_velocity;
            const vector lv_diff((float)((double)body_linear_velocity.x - (double)linear_velocity.x), (float)((double)body_linear_velocity.y - (double)linear_velocity.y), (float)((double)body_linear_velocity.z - (double)linear_velocity.z));
            const float lv_len = (float)sqrt((double)(float)((double)lv_diff.x * (double)lv_diff.x + (double)lv_diff.y * (double)lv_diff.y + (double)lv_diff.z * (double)lv_diff.z));
            if ((double)lv_len > (double)Mp_max_lv)
            {
                const float max_lv = Mp_max_lv;
                const vector dir = lv_diff.copy_normalize();
                linear_velocity.x = (float)((double)(float)((double)dir.x * (double)max_lv) + (double)auto_state->linear_velocity.x);
                linear_velocity.y = (float)((double)auto_state->linear_velocity.y + (double)(float)((double)dir.y * (double)max_lv));
                linear_velocity.z = (float)((double)auto_state->linear_velocity.z + (double)(float)((double)dir.z * (double)max_lv));
            }
            hkVector4f hk_linear_velocity;
            game_to_havok_vector(hk_linear_velocity, linear_velocity);
            vector angular_velocity = auto_state->angular_velocity;
            const vector av_diff((float)((double)body_angular_velocity.x - (double)angular_velocity.x), (float)((double)body_angular_velocity.y - (double)angular_velocity.y), (float)((double)body_angular_velocity.z - (double)angular_velocity.z));
            const float av_len = (float)sqrt((double)(float)((double)av_diff.x * (double)av_diff.x + (double)av_diff.y * (double)av_diff.y + (double)av_diff.z * (double)av_diff.z));
            if ((double)av_len > (double)Mp_max_av)
            {
                const float max_av = Mp_max_av;
                const vector dir = av_diff.copy_normalize();
                angular_velocity.x = (float)((double)(float)((double)max_av * (double)dir.x) + (double)auto_state->angular_velocity.x);
                angular_velocity.y = (float)((double)(float)((double)dir.y * (double)max_av) + (double)auto_state->angular_velocity.y);
                angular_velocity.z = (float)((double)(float)((double)dir.z * (double)max_av) + (double)auto_state->angular_velocity.z);
            }
            hkVector4f hk_angular_velocity;
            game_to_havok_vector(hk_angular_velocity, angular_velocity);
            rb->setLinearVelocity(hk_linear_velocity); // inlined: activate, then the motion's setLinearVelocity
            rb->setAngularVelocity(hk_angular_velocity);
            hkpKeyFrameUtility::applySoftKeyFrame(key_frame_info, accel_info, Frametime, (float)(1.0 / (double)Frametime), rb);
        }
        const float steering = auto_state->v_controls.steering;
        automobile_steer(vap, (float)((double)get_steering_correction(vap) + (double)steering), 0.0f);
        automobile_clear_accelerator(vap);
        const float accelerate = auto_state->v_controls.accelerate;
        if (accelerate > 0.0f)
        {
            automobile_set_forward_or_reverse_accelerator(vap->get_handle(), accelerate);
        }
        else if (auto_state->v_controls.brake_reverse)
        {
            automobile_set_forward_or_reverse_accelerator(vap->get_handle(), -1.0f);
        }
        // Original: the brake pedal takes the accelerator's value when both are held.
        if (auto_state->v_controls.accelerate > 0.0f && auto_state->v_controls.brake_reverse)
        {
            automobile_set_brake_pedal_input(vap->get_handle(), auto_state->v_controls.accelerate);
        }
        const vector& v = auto_state->linear_velocity;
        if (0.1 > (double)(float)sqrt((double)(float)((double)v.x * (double)v.x + (double)v.y * (double)v.y + (double)v.z * (double)v.z))) // Magic number: 0.1 m/s
        {
            automobile_set_handbrake(vap, false);
        }
    }
    if (Multiplayer && !Server)
    {
        vehicle_set_hit_points(vap->get_handle(), auto_state->hp);
        if ((double)auto_state->hp > 0.0 || auto_state->on_fire)
        {
            vehicle_set_smoking_and_fire(vap->get_handle(), auto_state->smoking, auto_state->on_fire, false);
        }
        if (vap->rdd.is_loaded())
        {
            vehicle_component* components = vap->rdd.get_data()->m_components;
            for (unsigned int i = 0; i < auto_state->component_count; i++)
            {
                vehicle_component* component = &components[i];
                if (auto_state->component_unlatched[i] && (component->get_flags() & 0x10000) == 0) // Magic number: VCF_UNLATCHED
                {
                    vehicle_component_unlatch_hinged(component, vap);
                }
                if (auto_state->component_detached[i] && (component->get_flags() & 0x20000) == 0) // Magic number: VCF_DETACHED
                {
                    vap->component_detach(component, false, 0, DBT_BULLET, false, false);
                }
                if (auto_state->component_shattered[i] && (component->get_flags() & 0x80000) == 0) // Magic number: VCF_NOT_RENDERED (broken glass)
                {
                    vehicle_component_shatter_glass(component, vap, nullptr, nullptr, nullptr);
                }
            }
            if (auto_state->v_controls.honk)
            {
                automobile_honk_horn(vap->get_handle());
            }
        }
    }
}

// 0x0099E520 (:2380)
// On the server, every 50 ms, a car faster than 5 m/s sweeps its box (widened by Mp_veh_expansion) from its leading
// edge to where that edge is after the look-ahead time: straight, or along the circle where the lines across its last
// and current velocity meet (when that circle is under 100 m). The part of the sweep the last check covered is cut
// off. Each destroyable mover in the swept box takes the car's collision energy per unit of its footprint as a
// pre-collision (rfg_dyn_add_pre_collide_mp_vehicle). A remote driver's look-ahead adds twice his latency (at most
// 250 ms) and Mp_veh_destruction_client_extra_lookahead.
void multi_game_auto_predict_collision(automobile* ap)
{
    if (!Multiplayer || !Server)
    {
        return;
    }
    mp_collision_prediction_info& prediction = ap->mp_collision_prediction;
    if (prediction.check_time.valid() && !prediction.check_time.elapsed())
    {
        return;
    }
    prediction.check_time.set(50); // Magic number: 50 ms between checks
    human* driver = vehicle_passenger_in_seat(ap, VSI_DRIVER);
    player* driver_player = nullptr;
    if (driver != nullptr && driver->get_obj_type() == OT_HUMAN && driver->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        driver_player = (player*)driver;
    }
    float look_ahead;
    if (!multi_game_object_vehicle_automobile_is_client_authoritative(ap))
    {
        look_ahead = (float)((double)(float)Mp_veh_destruction_base_look_ahead_time * 0.001); // ms to s
    }
    else
    {
        // Original: a client authoritative car always has a player driver, else this reads through null.
        int latency = multi_core_qos_find_latency_by_game_data(driver_player->mp_data) * 2;
        if (latency >= 250) // Magic number: 250 ms
        {
            latency = 250;
        }
        look_ahead = (float)((double)(float)(Mp_veh_destruction_client_extra_lookahead + Mp_veh_destruction_base_look_ahead_time + latency) * 0.001); // ms to s
    }
    vector vel;
    havok_body_get_linear_velocity(ap->havok_handle, vel);
    if (!((double)(float)((double)vel.x * (double)vel.x + (double)vel.y * (double)vel.y + (double)vel.z * (double)vel.z) > 25.0)) // Magic number: (5 m/s)^2
    {
        return;
    }
    const vector local_vel = vel.copy_rotate(ap->orient);
    const bool forward = local_vel.z > 0.0f;
    vector bmin;
    vector bmax;
    ap->get_local_bbox(bmin, bmax);
    bmin.x = (float)((double)bmin.x - (double)Mp_veh_expansion);
    bmin.y = (float)((double)bmin.y - (double)Mp_veh_expansion);
    bmax.x = (float)((double)Mp_veh_expansion + (double)bmax.x);
    bmax.y = (float)((double)Mp_veh_expansion + (double)bmax.y);

    // The lines across the last and the current velocity (on the ground plane) meet at the turn's center.
    const vector2 last_pos_2d(prediction.last_pos.x, prediction.last_pos.z);
    const vector2 last_side((float)((double)prediction.last_pos.x - (double)prediction.last_vel.z), (float)((double)prediction.last_pos.z + (double)prediction.last_vel.x));
    const vector2 pos_2d(ap->pos.x, ap->pos.z);
    const vector2 side((float)((double)ap->pos.x - (double)vel.z), (float)((double)ap->pos.z + (double)vel.x));
    vector2 ix;
    const bool turning = ix2d_line_line(ix, last_pos_2d, last_side, pos_2d, side);
    const float turn_radius = vec2_dist(pos_2d, ix);
    vector a; // the sweep end with the smaller local z
    vector b; // the sweep end with the larger local z
    if (turning && 100.0 > (double)turn_radius) // Magic number: 100 m
    {
        Radius = turn_radius;
        Center.x = ix.x;
        Center.y = ap->pos.y;
        Center.z = ix.y;
        const vector flat_vel(vel.x, 0.0f, vel.z);
        float turn_rate = flat_vel.mag() / turn_radius; // radians per second
        if (0.0f > (float)((double)vel.z * (double)prediction.last_vel.x - (double)vel.x * (double)prediction.last_vel.z))
        {
            turn_rate = -turn_rate;
        }
        const float to_pos_z = ap->pos.z - ix.y;
        const float to_pos_x = ap->pos.x - ix.x;
        const float angle = (float)atan2((double)to_pos_z, (double)to_pos_x);
        const float edge_z = forward ? bmax.z : bmin.z;
        const float edge_time = (float)((double)edge_z / (double)local_vel.z);
        const float edge_y = (float)((double)ap->orient.fvec.y * (double)edge_z);
        float edge_cos;
        float edge_sin = fl_sincos((float)((double)edge_time * (double)turn_rate + (double)angle), &edge_cos);
        vector edge;
        edge.z = (float)((double)edge_sin * (double)turn_radius + (double)ix.y);
        edge.x = (float)((double)edge_cos * (double)turn_radius + (double)ix.x);
        const double edge_y_world = (double)ap->pos.y + (double)edge_y;
        edge.y = (float)edge_y_world;
        float ahead_cos;
        const float ahead_sin = fl_sincos((float)((double)(float)((double)edge_time + (double)look_ahead) * (double)turn_rate + (double)angle), &ahead_cos);
        vector ahead;
        ahead.x = (float)((double)ahead_cos * (double)turn_radius + (double)ix.x);
        ahead.z = (float)((double)ahead_sin * (double)turn_radius + (double)ix.y);
        ahead.y = (float)((double)vel.y * (double)look_ahead + edge_y_world);
        a = forward ? edge : ahead;
        b = forward ? ahead : edge;
    }
    else
    {
        const float edge_z = forward ? bmax.z : bmin.z;
        vector edge;
        edge.x = (float)((double)(float)((double)ap->orient.fvec.x * (double)edge_z) + (double)ap->pos.x);
        edge.y = (float)((double)ap->pos.y + (double)(float)((double)ap->orient.fvec.y * (double)edge_z));
        edge.z = (float)((double)ap->pos.z + (double)(float)((double)ap->orient.fvec.z * (double)edge_z));
        vector ahead;
        ahead.x = (float)((double)(float)((double)vel.x * (double)look_ahead) + (double)edge.x);
        ahead.y = (float)((double)(float)((double)vel.y * (double)look_ahead) + (double)edge.y);
        ahead.z = (float)((double)(float)((double)vel.z * (double)look_ahead) + (double)edge.z);
        a = forward ? edge : ahead;
        b = forward ? ahead : edge;
    }
    prediction.last_pos = ap->pos;
    prediction.last_vel = vel;
    if (forward)
    {
        Box_start = b;
        Box_end = a;
    }
    else
    {
        Box_end = b;
        Box_start = a;
    }

    // The swept box: the car's widened local box, its z running from the edge to the look-ahead point.
    const vector sweep((float)((double)b.x - (double)a.x), (float)((double)b.y - (double)a.y), (float)((double)b.z - (double)a.z));
    const vector sweep_dir = sweep.copy_normalize_safe();
    matrix box_orient;
    box_orient.make_uvec(sweep_dir, ap->orient.uvec);
    vector box_min = bmin;
    vector box_max = bmax;
    vector box_pos;
    if (forward)
    {
        box_pos = a;
        box_min.z = 0.0f;
        box_max.z = sweep.mag();
    }
    else
    {
        box_pos = b;
        box_max.z = 0.0f;
        box_min.z = -sweep.mag();
    }
    vector extents;
    vector center;
    bbox_extract_obb_info(box_min, box_max, box_pos, box_orient, extents, center);
    if (Full_obb)
    {
        const vector size((float)((double)extents.x * 2.0), (float)((double)extents.y * 2.0), (float)((double)extents.z * 2.0));
        add_obb(center, size, box_orient);
    }
    const float sign = forward ? 1.0f : -1.0f;
    float half = (float)((double)extents.z * (double)sign);
    // The sweep's center line on the ground plane (y = 0).
    vector start((float)((double)center.x - (double)(float)((double)box_orient.fvec.x * (double)half)), 0.0f, (float)((double)center.z - (double)(float)((double)box_orient.fvec.z * (double)half)));
    vector end((float)((double)(float)((double)half * (double)box_orient.fvec.x) + (double)center.x), 0.0f, (float)((double)(float)((double)box_orient.fvec.z * (double)half) + (double)center.z));
    const float line_x = (float)((double)end.x - (double)start.x);
    const float line_y = 0.0f; // (float)(0.0 - 0.0) in rfg.exe: both ends have y = 0
    const float line_z = (float)((double)end.z - (double)start.z);
    const float last_x = (float)((double)prediction.last_end_pos.x - (double)prediction.last_start_pos.x);
    const float last_y = (float)((double)prediction.last_end_pos.y - (double)prediction.last_start_pos.y);
    const float last_z = (float)((double)prediction.last_end_pos.z - (double)prediction.last_start_pos.z);
    if ((float)((double)last_y * (double)last_y + (double)last_x * (double)last_x + (double)last_z * (double)last_z) > 0.01f // Magic number: (0.1 m)^2
        && (float)((double)last_x * (double)line_x + (double)last_y * (double)line_y + (double)last_z * (double)line_z) > 0.0f)
    {
        // Cut off what the last sweep covered: up to the last end along this sweep.
        vector closest;
        const float covered = ix_closest_point_on_line_segment(closest, prediction.last_end_pos, start, end);
        const vector line((float)((double)start.x - (double)end.x), (float)((double)start.y - (double)end.y), (float)((double)start.z - (double)end.z));
        const float line_len = line.mag();
        const float covered_ratio = (float)((double)covered / (double)line_len);
        if ((double)covered_ratio > 0.995) // Magic number: nearly all of it was covered
        {
            return;
        }
        const double cut = (double)line_len * (double)covered_ratio;
        if (forward)
        {
            box_min.z = (float)((double)box_min.z + cut);
        }
        else
        {
            box_max.z = (float)((double)box_max.z - cut);
        }
        bbox_extract_obb_info(box_min, box_max, box_pos, box_orient, extents, center);
        if (Clip_obb)
        {
            const vector size((float)((double)extents.x * 2.0), (float)((double)extents.y * 2.0), (float)((double)extents.z * 2.0));
            add_obb(center, size, box_orient);
        }
        half = (float)((double)extents.z * (double)sign);
        start.x = (float)((double)center.x - (double)(float)((double)half * (double)box_orient.fvec.x));
        start.y = (float)((double)center.y - (double)(float)((double)box_orient.fvec.y * (double)half));
        start.z = (float)((double)center.z - (double)(float)((double)box_orient.fvec.z * (double)half));
        end.x = (float)((double)(float)((double)half * (double)box_orient.fvec.x) + (double)center.x);
        end.y = (float)((double)(float)((double)box_orient.fvec.y * (double)half) + (double)center.y);
        end.z = (float)((double)(float)((double)box_orient.fvec.z * (double)half) + (double)center.z);
        start.y = 0.0f;
        end.y = 0.0f;
    }
    prediction.last_start_pos = start;
    prediction.last_end_pos = end;

    const vector neg_extents(-extents.x, -extents.y, -extents.z);
    unsigned int handles[10];
    const int count = World.collect_objects<rfg_mover>(neg_extents, extents, center, box_orient, handles, 10, nullptr, nullptr);
    for (int i = 0; i < count; i++)
    {
        rfg_mover* mover = nullptr;
        if (handles[i] != 0xFFFFFFFF)
        {
            object* objp = World.object_from_handle(handles[i]);
            if (objp != nullptr && objp->get_obj_type() == OT_MOVER && objp->get_sub_type() == OT_SUB_MOVER_RFG)
            {
                mover = (rfg_mover*)objp;
            }
        }
        // Original: a handle that is not an rfg mover still goes in as a null mover.
        rfg_dyn_collision_energy_info energy_info(ap, mover, ap->pos);
        if (rfg_dyn_compute_collision_energy(energy_info))
        {
            const float footprint = (float)(((double)box_max.x - (double)box_min.x) * ((double)box_max.y - (double)box_min.y)); // x87 in rfg.exe
            const float energy = (float)((double)(float)((double)energy_info.energy / (double)footprint) * (double)Veh_energy_scalar);
            rfg_dyn_add_pre_collide_mp_vehicle(mover, driver, (unsigned int)(__int64)energy, extents, center, ap->orient, false);
        }
    }
}
