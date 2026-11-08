// Original: volition/rfg/code/vehicle/vehicle_flyer.cpp
//
// Flyer: an aircraft held up by a Havok action (flyer_control_action). Each physics step works out the acceleration that
// reaches the requested point or direction, the yaw that faces the look-at direction and the thrust tilt, and sets the
// body's velocities directly. Each frame moves the turbine, shakes the craft with damage, wind and hover noise, and
// plays the jet wash and contrail effects.

#include "volition/rfg/code/vehicle/vehicle_flyer.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object_constraint.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/vehicle/vehicle_component.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle/vehicle_subinfo.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle_ai/intersection_controller.h"
#include "volition/rfg/code/weather/wind.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/os/frametime.h"

#include <math.h>

// ---- file-static globals
// from the tweak table
static float flyer_wind_force_scalar; // 0x0376F654
static float flyer_damage_force_scalar; // 0x0376F65C
static float flyer_hover_noise_length; // 0x0376F660
static float flyer_damage_force_speed_scalar; // 0x0376F9E8
static float flyer_default_urgency; // 0x0376F9F0
static float flyer_damage_force_speed_mark; // 0x03770000
static float flyer_min_lookat_vel_sq; // 0x03770008
static float flyer_default_max_vel_mag; // 0x0377000C
static tweak_table_item Tweak_table_item_flyer_default_max_vel_mag("flyer_default_max_vel_mag", &flyer_default_max_vel_mag);
static tweak_table_item Tweak_table_item_flyer_default_urgency("flyer_default_urgency", &flyer_default_urgency);
static tweak_table_item Tweak_table_item_flyer_min_lookat_vel_sq("flyer_min_lookat_vel_sq", &flyer_min_lookat_vel_sq);
static tweak_table_item Tweak_table_item_flyer_damage_force_scalar("flyer_damage_force_scalar", &flyer_damage_force_scalar);
static tweak_table_item Tweak_table_item_flyer_damage_force_speed_scalar("flyer_damage_force_speed_scalar", &flyer_damage_force_speed_scalar);
static tweak_table_item Tweak_table_item_flyer_damage_force_speed_mark("flyer_damage_force_speed_mark", &flyer_damage_force_speed_mark);
static tweak_table_item Tweak_table_item_flyer_wind_force_scalar("flyer_wind_force_scalar", &flyer_wind_force_scalar);
static tweak_table_item Tweak_table_item_flyer_hover_noise_length("flyer_hover_noise_length", &flyer_hover_noise_length);

static object_pool<flyer,8,1> flyerpool; // 0x0381A650
static obj_class_instantiator flyer_instantiator("flyer", alloc_flyer, types_flyer, bpo_index_flyer, classname_flyer, flyer::pool_remaining, 0x20FB0, 8); // 0x03770B88

const vector flyer::GRAVITY_VECTOR(0.0f, -9.8f, 0.0f); // 0x016BA5E0
const vector flyer::ANTIGRAVITY_VECTOR(0.0f, 9.8f, 0.0f); // 0x016BA5EC

static const float GRAVITY_SQ = 96.04f; // 9.8 * 9.8
static const float TWO_PI = 6.2831855f;
static const float DEGREES_TO_RADIANS = 0.017453f;
static const float TURBINE_RPM_MIN = 800.0f;
static const float TURBINE_RPM_MAX = 5200.0f;
static const float TURBINE_RPM_PER_SECOND = 500.0f;
static const float JETWASH_RAY_LENGTH = 12.0f;
static const int FLYER_STARTUP_MS = 2500;
static const int ENGINE_STOP_SUSPENSION_MS = 1000;
static const int CATCH_FIRE_WING_DETACHED_MS = 20000;
static const int CATCH_FIRE_UPSIDE_DOWN_MS = 1000;
static const int CATCH_FIRE_AGAIN_MS = 20000000; // long enough not to happen again
// Animation indices in the flyer's animation tables.
static const int ANIM_STATE_LANDED = 0xFC;
static const int ANIM_ACTION_GEAR_UP = 0x352;
static const int ANIM_ACTION_GEAR_DOWN = 0x353;

static float flyer_clamp(float value, float low, float high)
{
    if (value - low < 0.0f) {
        value = low;
    }
    if (0.0f <= value - high) {
        value = high;
    }
    return value;
}

static void flyer_stop_effect(unsigned int handle)
{
    if (handle == NO_HANDLE) {
        return;
    }
    object* obj = World.object_from_handle(handle);
    if (obj != NULL && obj->get_obj_type() == OT_EFFECT) {
        static_cast<object_effect*>(obj)->stop(object_effect::SS_STOP_EMITTING);
    }
}

// 0x00B66E20 (:131)
flyer_control_action::flyer_control_action(hkpRigidBody* rb, flyer* flyer_p)
    : hkpUnaryAction(rb, 0)
{
    fp = flyer_p;
}

// 0x00B66E50
flyer_control_action::~flyer_control_action()
{
}

// 0x00B7E0B0 (:125)
hkpAction* flyer_control_action::clone(const hkArray<hkpEntity*,hkContainerHeapAllocator>& entities, const hkArray<hkpPhantom*,hkContainerHeapAllocator>& phantoms) const
{
    return new flyer_control_action(static_cast<hkpRigidBody*>(entities[0]), fp);
}

// 0x00B8F140 (:139)
void flyer_control_action::applyAction(const hkStepInfo& stepInfo)
{
    hkpRigidBody* body = static_cast<hkpRigidBody*>(m_entity);
    const float mass = body->getMotion()->getMass();
    havok_body_force_activate(fp->havok_handle);
    fp->update_flight_info();
    if (fp != NULL && fp->m_flags.m_engine_running != 0) {
        fp->flight_compute_physics(stepInfo.m_deltaTime);
    }
    // Lift against gravity, scaled by how far the engine has spun up.
    hkVector4f lift;
    lift.set(0.0f, fp->engine_force * mass * GRAVITY_MS2 * stepInfo.m_deltaTime, 0.0f, 0.0f);
    body->activate();
    body->getMotion()->applyLinearImpulse(lift);
}

// 0x00B66E70 (:187)
void types_flyer(signed char& type, signed char& subtype)
{
    type = OT_VEHICLE;
    subtype = OT_SUB_VEHICLE_FLYER;
}

// 0x00B66E80 (:187)
int bpo_index_flyer()
{
    return 0;
}

// 0x00B66E90 (:187)
const char* classname_flyer()
{
    return "flyer";
}

// 0x00B630A0 (.h:29)
const char* flyer::get_classname_static()
{
    return "flyer";
}

// 0x00B630B0 (.h:29)
int flyer::get_bpo_index_static()
{
    return 0;
}

// 0x00BA1500 (.h:29)
const char* flyer::get_classname() const
{
    return "flyer";
}

// 0x00B66EA0 (:187)
unsigned int flyer::get_classname_hash()
{
    return flyer_instantiator.this_hash;
}

// 0x00B7E160 (:187)
int flyer::pool_remaining()
{
    return flyerpool.get_free();
}

// 0x00B8F240 (:187)
object* alloc_flyer()
{
    return flyerpool.alloc(0);
}

// 0x00B97EB0 (:187)
void flyer::pool_free()
{
    flyerpool.free(this);
}

// 0x00BA14C0 (:188)
flyer::flyer()
{
}

// 0x00BA1510
flyer::~flyer()
{
}

// 0x00B66EB0 (:193)
hkpRigidBody* flyer::get_supporting_rigid_body() const
{
    return NULL;
}

// 0x00B66EC0 (:200)
float flyer::get_energy_scale()
{
    return 1.0f;
}

// 0x00B8F250 (:213)
bool flyer::serialize(obj_prop_block& pblock, bool load)
{
    if (!vehicle::serialize(pblock, load)) {
        return false;
    }
    if (!load) {
        pblock.set(VEHICLE_PROP_TYPE_NAME, info->name);
        pblock.set<bool>(VEHICLE_PROP_AT_GROUND, false);
        return true;
    }
    int type = 0;
    if (!pblock.get_optional(VEHICLE_PROP_TYPE, type)) {
        char type_name[256];
        pblock.get(VEHICLE_PROP_TYPE_NAME, type_name);
        type = vehicle_get_type(type_name);
    }
    if (type < 0) {
        return false;
    }
    memset(&flyer_flags, 0, sizeof(flyer_flags));
    wing_is_detached = false;
    last_player_damage.invalidate();
    catch_fire_timer.invalidate();
    engine_off_suspension_period.invalidate();
    return true;
}

// 0x00BAAAE0 (:261)
bool flyer::stream(bool load)
{
    if (!vehicle::stream(load)) {
        return false;
    }
    if (!load) {
        kill_effects();
        f_rdd.set_loaded(false);
        return true;
    }
    f_rdd.set_loaded(true);
    flyer_build(this);
    float ground_y = 0.0f;
    havok_terrain_height(ground_y, pos.x, pos.z, false, NULL);
    const float height = pos.y - ground_y;
    vai.original_offset = height;
    if (6.0f <= height) { // metres: starts in the air, flying
        flyer_flags.landing_gear_up = 1;
        flyer_flags.on_ground = 0;
        vanim_clear_states(vp);
        engine_start(false);
        startup_timer.set(-1);
    }
    else {
        flyer_flags.landing_gear_up = 0;
        flyer_flags.on_ground = 1;
        vanim_change_state(vp, index_alias(ANIM_STATE_LANDED, (vanim_aliases)-1), index_alias(-1, (vanim_aliases)-1), 0.3f, false);
        engine_force = 0.0f;
    }
    flyer_flags.landing_gear_collision_off = 0;
    update_flight_info();
    if (0.0f < fabsf(vai.initial_forward_speed) && get_handle() != NO_HANDLE) {
        object* self = World.object_from_handle(get_handle());
        if (self != NULL && self->get_obj_type() == OT_VEHICLE) {
            hkpRigidBody* body = havok_body_get_pointer(self->havok_handle);
            if (body != NULL) {
                hkVector4f initial;
                initial.set(orient.fvec.x * vai.initial_forward_speed, orient.fvec.y * vai.initial_forward_speed, orient.fvec.z * vai.initial_forward_speed, 0.0f);
                body->activate();
                body->getMotion()->setLinearVelocity(initial);
            }
        }
    }
    return true;
}

// 0x00BB3CD0 (:347)
void flyer::free_this()
{
    vehicle::free_this();
    if (vp != NULL) {
        vanim_delete(vp);
        vp = NULL;
        animlib_delete_anim_instance(aim_handle);
    }
}

// 0x00B8F350 (:359)
void flyer::kill_effects()
{
    flyer_stop_effect(jetwash);
    jetwash = NO_HANDLE;
    engine_effects_stop();
}

// Plays a looping effect on a tag of the mesh, if it has that tag. 0x00B66ED0 (:380), part.
static void flyer_play_tag_effect(flyer* fp, unsigned int effect_info, const char* tag_name, unsigned int& effect)
{
    const int tag = fp->get_tag_index_from_name(tag_name);
    if (tag != -1) {
        vector tag_pos;
        matrix tag_orient;
        fp->get_tag_world_coords(tag, tag_pos, &tag_orient, NULL, NULL);
        effect = effects_play(effect_info, tag_pos, tag_orient, true, fp->get_handle(), tag, NO_HANDLE, 1.0f, false, EFFECTS_CATEGORY_NONE);
    }
}

// 0x00B66ED0 (:380)
void flyer::engine_effects_start()
{
    if (info->flyer_wingtip_effect != NO_HANDLE) {
        flyer_play_tag_effect(this, info->flyer_wingtip_effect, "contrail_1", wingtip_effect[0]);
        flyer_play_tag_effect(this, info->flyer_wingtip_effect, "contrail_2", wingtip_effect[1]);
    }
    if (info->flyer_thruster_effect != NO_HANDLE) {
        flyer_play_tag_effect(this, info->flyer_thruster_effect, "large_thruster_1", front_thruster[0]);
        flyer_play_tag_effect(this, info->flyer_thruster_effect, "large_thruster_2", front_thruster[1]);
        flyer_play_tag_effect(this, info->flyer_thruster_effect, "med_thruster_1", center_thruster[0]);
        flyer_play_tag_effect(this, info->flyer_thruster_effect, "med_thruster_2", center_thruster[1]);
    }
    if (info->flyer_mainengine_effect != NO_HANDLE) {
        flyer_play_tag_effect(this, info->flyer_mainengine_effect, "engine", main_engines[0]);
    }
}

// 0x00B67210 (:441)
void flyer::suspend_engine_stop()
{
    engine_off_suspension_period.set(ENGINE_STOP_SUSPENSION_MS);
}

// 0x00B7E170 (:450)
void flyer::engine_effects_stop()
{
    flyer_stop_effect(wingtip_effect[0]);
    flyer_stop_effect(wingtip_effect[1]);
    flyer_stop_effect(front_thruster[0]);
    flyer_stop_effect(front_thruster[1]);
    flyer_stop_effect(center_thruster[0]);
    flyer_stop_effect(center_thruster[1]);
    flyer_stop_effect(main_engines[0]);
    flyer_stop_effect(main_engines[1]);
    wingtip_effect[0] = NO_HANDLE;
    wingtip_effect[1] = NO_HANDLE;
    front_thruster[0] = NO_HANDLE;
    front_thruster[1] = NO_HANDLE;
    center_thruster[0] = NO_HANDLE;
    center_thruster[1] = NO_HANDLE;
    main_engines[0] = NO_HANDLE;
    main_engines[1] = NO_HANDLE;
}

// 0x00B7E2F0 (:531)
void flyer::update_flight_info()
{
    hkpRigidBody* body = havok_body_get_pointer(havok_handle);
    vehicle_mass = body->getMotion()->getMass();
    havok_to_game_vector(chassis_com_ws, body->getCenterOfMassInWorld());
    havok_to_game_vector(chassis_com_cs, body->getCenterOfMassLocal());
    vector body_pos;
    matrix body_orient;
    havok_body_get_pos_orient(havok_handle, body_pos, body_orient);
    flyer_compute_world_turst_matrix(body_orient, thrust_mat_cs.uvec, thrust_mat_cs, thrust_mat_ws);
}

// 0x00B67230 (:551)
void flyer::set_request_urgency(float urg)
{
    req_urgency = flyer_clamp(urg, 0.0f, 1.0f);
}

// 0x00B67280 (:556)
void flyer::set_request_max_velocity_mag(float max_vel)
{
    req_max_vel = flyer_clamp(max_vel, 0.0f, flyer_default_max_vel_mag);
}

// 0x00B7E380 (:562)
void flyer::set_request_flyto_point(const vector& new_pt, bool stop_at)
{
    req_vel.zero();
    req_halt = false;
    req_stop_at_point = stop_at;
    req_point_isvalid = true;
    req_vel_isvalid = false;
    req_point = new_pt;
}

// 0x00B7E3E0 (:571)
void flyer::set_request_flyto_direction(const vector& new_vec)
{
    req_vel.zero();
    req_stop_at_point = false;
    req_halt = false;
    req_point_isvalid = false;
    req_vel_isvalid = true;
    if (0.01f <= new_vec.x * new_vec.x + new_vec.y * new_vec.y + new_vec.z * new_vec.z) {
        req_vel = new_vec.copy_normalize_safe();
    }
}

// 0x00B7E480 (:583)
void flyer::set_request_halt()
{
    req_vel.zero();
    req_point_isvalid = false;
    req_vel_isvalid = false;
    req_stop_at_point = false;
    req_halt = true;
}

// 0x00B672E0 (:589)
void flyer::clear_requested_movement()
{
    req_vel.zero();
    req_point_isvalid = false;
    req_vel_isvalid = false;
    req_stop_at_point = false;
    req_halt = false;
}

// 0x00B67310 (:599)
bool flyer::is_movement_requested()
{
    return req_vel_isvalid || req_point_isvalid;
}

// 0x00B67330 (:606)
void flyer::set_request_lookat_vector(const vector& new_vec)
{
    req_lookat = new_vec;
    const float mag_sq = req_lookat.x * req_lookat.x + req_lookat.y * req_lookat.y + req_lookat.z * req_lookat.z;
    if (mag_sq != 0.0f && (new_vec.x != 0.0f || new_vec.z != 0.0f)) {
        if (mag_sq != 1.0f) {
            req_lookat.normalize_safe();
        }
        req_lookat_isvalid = true;
        return;
    }
    req_lookat_isvalid = false;
}

// 0x00B673D0 (:629)
void flyer::clear_requested_lookat_vector()
{
    req_lookat_isvalid = false;
}

// 0x00B7E4B0 (:637)
void flyer::engine_start(bool is_player)
{
    if (this == NULL || m_flags.m_engine_running == 0) {
        vehicle::engine_start(is_player);
        startup_timer.set(FLYER_STARTUP_MS);
    }
}

// 0x00B7E4F0 (:652)
void flyer::engine_stop(bool is_player, bool unused)
{
    if (engine_off_suspension_period.valid() && !engine_off_suspension_period.elapsed()) {
        return;
    }
    startup_timer.invalidate();
    if (m_flags.m_engine_running != 0) {
        vehicle::engine_stop(is_player, false);
        engine_effects_stop();
        engine_force = 0.0f;
    }
}

// 0x00B673E0 (:681)
bool flyer::get_landing_gear_up()
{
    return flyer_flags.landing_gear_up != 0;
}

// 0x00B673F0 (:687)
void flyer::set_landing_gear_up(bool go_up)
{
    if (go_up) {
        if (flyer_flags.landing_gear_up == 0) {
            vanim_play_action(vp, index_alias(ANIM_ACTION_GEAR_UP, (vanim_aliases)-1), 0, 1.0f, 1.0f);
            vanim_clear_states(vp);
            flyer_flags.landing_gear_up = 1;
        }
    }
    else if (flyer_flags.landing_gear_up != 0) {
        vanim_play_action(vp, index_alias(ANIM_ACTION_GEAR_DOWN, (vanim_aliases)-1), 0, 1.0f, 1.0f);
        vanim_change_state(vp, index_alias(ANIM_STATE_LANDED, (vanim_aliases)-1), index_alias(-1, (vanim_aliases)-1), 0.3f, false);
        flyer_flags.landing_gear_up = 0;
    }
}

// 0x00B674D0 (:719): empty in rfg.exe
void flyer_system_init()
{
}

// 0x00B674E0 (:725)
void flyer_get_velocity(const flyer* fp, vector& vel)
{
    vel.x = fp->pos.x - fp->last_pos.x;
    vel.y = fp->pos.y - fp->last_pos.y;
    vel.z = fp->pos.z - fp->last_pos.z;
    const float one_over_frametime = 1.0f / Frametime;
    vel.x *= one_over_frametime;
    vel.y *= one_over_frametime;
    vel.z *= one_over_frametime;
}

// 0x00B7E550 (:734)
void flyer_build(flyer* fp)
{
    hkpRigidBody* body = havok_body_get_pointer(fp->havok_handle);
    fp->engine_force = 0.0f;
    fp->flyer_flags.landing_gear_up = 1;
    fp->flyer_flags.on_ground = 0;
    flyer_control_action* action = new flyer_control_action(body, fp);
    Havok_world->addAction(action);
    fp->ctl_action = action;
    action->removeReference();
    fp->thrust_mat_cs.make_identity();
    fp->thrust_up_target = vector(0.0f, 1.0f, 0.0f);
    fp->set_request_urgency(flyer_clamp(flyer_default_urgency, 0.01f, 1.0f));
    fp->set_request_max_velocity_mag(flyer_default_max_vel_mag);
    fp->clear_requested_movement();
    fp->hover_noise_vec.zero();
    fp->hover_noise_vec_old.zero();
    fp->hover_noise_vec_new.zero();
    fp->hover_noise_timer.set(-1);
    fp->turbine_rpm = TURBINE_RPM_MIN;
    fp->hover_noise_dur = 0.0f;
    fp->turbine_rpm_target = 1200.0f;
    fp->turbine_load = 0.0f;
    fp->l_wing = fp->get_component_index_from_name("Wing_L", 0);
    fp->r_wing = fp->get_component_index_from_name("Wing_R", 0);
    fp->l_canard = fp->get_component_index_from_name("ForeWing_L", 0);
    fp->r_canard = fp->get_component_index_from_name("ForeWing_R", 0);
    fp->wingtip_effect[0] = NO_HANDLE;
    fp->wingtip_effect[1] = NO_HANDLE;
    fp->front_thruster[0] = NO_HANDLE;
    fp->front_thruster[1] = NO_HANDLE;
    fp->center_thruster[0] = NO_HANDLE;
    fp->center_thruster[1] = NO_HANDLE;
    fp->main_engines[0] = NO_HANDLE;
    fp->main_engines[1] = NO_HANDLE;
    fp->jetwash = NO_HANDLE;
    // No damping: the action controls the velocities itself.
    body->setLinearDamping(0.0f);
    body->setAngularDamping(0.0f);
}

// 0x00B675D0 (:802): empty in rfg.exe
void flyer_steer(flyer* fp)
{
}

// 0x00B675E0 (:881)
void flyer_compute_world_turst_matrix(const matrix& chassis_mat, const vector& down_thruster_up, matrix& thrust_mat_local, matrix& thrust_mat_world)
{
    thrust_mat_local.uvec = down_thruster_up.copy_normalize_safe();
    const vector& up = thrust_mat_local.uvec;
    thrust_mat_local.fvec.x = up.z * RIGHT_VECTOR.y - up.y * RIGHT_VECTOR.z;
    thrust_mat_local.fvec.y = up.x * RIGHT_VECTOR.z - up.z * RIGHT_VECTOR.x;
    thrust_mat_local.fvec.z = up.y * RIGHT_VECTOR.x - up.x * RIGHT_VECTOR.y;
    thrust_mat_local.fvec.normalize_safe();
    const vector& forward = thrust_mat_local.fvec;
    thrust_mat_local.rvec.x = forward.z * up.y - forward.y * up.z;
    thrust_mat_local.rvec.y = forward.x * up.z - forward.z * up.x;
    thrust_mat_local.rvec.z = forward.y * up.x - forward.x * up.y;
    thrust_mat_world = thrust_mat_local * chassis_mat;
}

// 0x00B67790 (:912)
// The acceleration that takes a velocity towards a target distance away and stops there (:912, 0x00B67790)
float flyer_compute_target_curve(float target_dist, float vel, float vel_max, float vel_delta_max, float frame_deltat)
{
    const float dist = fabsf(target_dist);
    const float speed = fabsf(vel);
    const float one_over_dt = 1.0f / frame_deltat;
    const float frame_delta = vel_delta_max * frame_deltat;
    if (0.005f <= target_dist - 0.0f || 0.005f <= -(target_dist - 0.0f)) {
        if (vel - 0.0f < 0.001f && -(vel - 0.0f) < 0.001f) {
            if (dist < frame_delta) {
                vel_delta_max = one_over_dt * dist;
            }
            return target_dist < 0.0f ? -vel_delta_max : vel_delta_max;
        }
        if (target_dist * vel < 0.0f) {
            // Moving away from it.
            if (speed < frame_delta && sqrtf(dist * vel_delta_max) < (frame_delta - speed) * one_over_dt) {
                vel_delta_max = (sqrtf(dist * vel_delta_max) + speed) * one_over_dt;
            }
            return 0.0f <= vel ? -vel_delta_max : vel_delta_max;
        }
        if (vel_max * 1.05f < speed) {
            const float slow_down = vel_delta_max * 0.8f;
            return 0.0f <= vel ? -slow_down : slow_down;
        }
        const float stopping_dist = (speed * 0.5f * speed) / vel_delta_max;
        if (dist <= stopping_dist) {
            if (speed < frame_delta) {
                vel_delta_max = one_over_dt * speed;
            }
            return 0.0f <= vel ? -vel_delta_max : vel_delta_max;
        }
        if (stopping_dist * 2.0f <= dist && !fl_greater_than_equal_robust(speed, vel_max, 0.001f)) {
            return vel < 0.0f ? -vel_delta_max : vel_delta_max;
        }
    }
    else if (0.001f <= vel - 0.0f || 0.001f <= -(vel - 0.0f)) {
        if (speed < frame_delta) {
            vel_delta_max = one_over_dt * speed;
        }
        return 0.0f <= vel ? -vel_delta_max : vel_delta_max;
    }
    return 0.0f;
}

// 0x00B67AD0 (:1059)
void flyer_compute_target_time_deltav(float vel, float deltav_max, float& time_to_stop, float& dist_to_stop)
{
    time_to_stop = fabsf(vel) / deltav_max;
    dist_to_stop = fabsf(vel) * 0.5f * time_to_stop;
}

// 0x00B67B30 (:1073)
void flyer_compute_target_time_startstop(float dist, float deltav_max, float vel_max, float& time_to_stop)
{
    if (dist < 0.0f) {
        dist = -dist;
    }
    if (sqrtf(dist * deltav_max) <= vel_max) {
        time_to_stop = (sqrtf(dist * deltav_max) / deltav_max) * 2.0f;
        return;
    }
    const float ramp_time = fabsf(vel_max) / deltav_max;
    // Adds to the time already there; only this branch does.
    time_to_stop = (dist - fabsf(vel_max) * 0.5f * ramp_time * 2.0f) / vel_max + ramp_time * 2.0f + time_to_stop;
}

// fl_equal_robust against 0. 0x00B67C30 (:1314), part.
static bool flyer_about_zero(float value)
{
    const float magnitude = fabsf(value);
    const float tolerance = (0.0f <= 1.0f - magnitude ? 1.0f : magnitude) * 0.0001f;
    return 0.0f - tolerance <= value && value <= tolerance + 0.0f;
}

// 0x00B67C30 (:1314)
float flyer_compute_target_time(float target_dist, float vel, float vel_max, float vel_delta_max)
{
    float time_to_stop;
    float dist_to_stop;
    if (flyer_about_zero(target_dist)) {
        if (flyer_about_zero(vel)) {
            return 0.0f;
        }
        flyer_compute_target_time_deltav(vel, vel_delta_max, time_to_stop, dist_to_stop);
        const float stop_time = time_to_stop;
        flyer_compute_target_time_startstop(dist_to_stop, vel_delta_max, vel_max, time_to_stop);
        return time_to_stop + stop_time + 0.0f;
    }
    if (flyer_about_zero(vel)) {
        // Starts from vel, which is about zero (the decompiled time is seeded by it).
        time_to_stop = vel;
        flyer_compute_target_time_startstop(target_dist, vel_delta_max, vel_max, time_to_stop);
        return time_to_stop;
    }
    flyer_compute_target_time_deltav(vel, vel_delta_max, time_to_stop, dist_to_stop);
    const float stop_time = time_to_stop;
    if (target_dist * vel < 0.0f) {
        flyer_compute_target_time_startstop(dist_to_stop + target_dist, vel_delta_max, vel_max, time_to_stop);
        return time_to_stop + stop_time + 0.0f;
    }
    if (target_dist < dist_to_stop) {
        flyer_compute_target_time_startstop(dist_to_stop - target_dist, vel_delta_max, vel_max, time_to_stop);
        return time_to_stop + stop_time + 0.0f;
    }
    flyer_compute_target_time_startstop(dist_to_stop + target_dist, vel_delta_max, vel_max, time_to_stop);
    return time_to_stop + (0.0f - stop_time);
}

// 0x00B7E800 (:1522)
void flyer::flight_compute_physics(float step_time)
{
    if (step_time < 0.0f) {
        step_time = Frametime;
    }
    const float urgency = req_urgency;
    const float max_up_accel = info->flyer_max_up_thrust * GRAVITY_MS2;
    const float max_lateral_accel = sqrtf((max_up_accel + GRAVITY_MS2) * (max_up_accel + GRAVITY_MS2) - GRAVITY_SQ);
    const float urgent_up = urgency * max_up_accel + GRAVITY_MS2;
    const float lateral_accel = sqrtf(urgent_up * urgent_up - GRAVITY_SQ);
    float max_turn_vel = info->flyer_max_turn_angvel * TWO_PI * urgency;
    const float max_turn_accel_full = info->flyer_max_turn_angaccl * TWO_PI;
    const float max_turn_accel = urgency * max_turn_accel_full;
    float max_tilt_vel = info->flyer_max_tilt_angvel * TWO_PI * urgency;
    const float max_tilt_accel_full = info->flyer_max_tilt_angaccl * TWO_PI;
    const float max_tilt_accel = urgency * max_tilt_accel_full;

    hkpRigidBody* body = havok_body_get_pointer(havok_handle);
    vector body_pos;
    matrix body_orient;
    vector velocity;
    vector angular;
    havok_body_get_pos_orient(body, body_pos, body_orient);
    havok_body_get_linear_velocity(body, velocity);
    havok_body_get_angular_velocity(body, angular);

    vector target(req_point.x + hover_noise_vec.x, hover_noise_vec.y + req_point.y, hover_noise_vec.z + req_point.z);
    if (req_halt) {
        req_vel.zero();
        if (0.01f <= velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z) {
            req_point = chassis_com_ws;
            req_point_isvalid = true;
            req_vel_isvalid = false;
            req_stop_at_point = true;
            req_halt = true;
        }
        else {
            req_point_isvalid = false;
            req_vel_isvalid = false;
            req_stop_at_point = false;
            req_halt = false;
        }
    }
    if (req_vel_isvalid) {
        target = vector(req_vel.x * 100.0f + chassis_com_ws.x, req_vel.y * 100.0f + chassis_com_ws.y, req_vel.z * 100.0f + chassis_com_ws.z);
    }

    // Where to go: the acceleration, with the speed across the target line taken out first.
    const float frame_delta_v = lateral_accel * step_time;
    vector accel;
    accel.zero();
    thrust_up_target = vector(0.0f, 1.0f, 0.0f);
    if (req_vel_isvalid || req_point_isvalid) {
        const float speed = sqrtf(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
        const vector vel_dir(velocity.x / speed, velocity.y / speed, velocity.z / speed);
        const vector to_target(target.x - chassis_com_ws.x, target.y - chassis_com_ws.y, target.z - chassis_com_ws.z);
        const float dist = sqrtf(to_target.x * to_target.x + to_target.y * to_target.y + to_target.z * to_target.z);
        if (0.004f <= dist) {
            const vector dir(to_target.x / dist, to_target.y / dist, to_target.z / dist);
            const float along = dir.y * velocity.y + dir.x * velocity.x + dir.z * velocity.z;
            const vector across(velocity.x - dir.x * along, velocity.y - dir.y * along, velocity.z - dir.z * along);
            const float across_speed = sqrtf(across.x * across.x + across.y * across.y + across.z * across.z);
            if (to_target.y * velocity.y + to_target.x * velocity.x + to_target.z * velocity.z <= 0.0f) {
                if (speed <= frame_delta_v) {
                    const float remaining = sqrtf(lateral_accel * lateral_accel - across_speed * across_speed);
                    accel = vector(dir.x * remaining - across.x, dir.y * remaining - across.y, dir.z * remaining - across.z);
                }
                else {
                    accel = vector(-vel_dir.x * lateral_accel, -vel_dir.y * lateral_accel, -vel_dir.z * lateral_accel);
                }
            }
            else if (across_speed <= frame_delta_v) {
                const float along_dist = req_stop_at_point ? dist : 1e+06f; // flying through: never near it
                const float along_accel = flyer_compute_target_curve(along_dist, along, req_max_vel, sqrtf(lateral_accel * lateral_accel - across_speed * across_speed), step_time);
                accel = vector(dir.x * along_accel - across.x, dir.y * along_accel - across.y, dir.z * along_accel - across.z);
            }
            else {
                const float scale = lateral_accel / across_speed;
                accel = vector(scale * -across.x, scale * -across.y, scale * -across.z);
            }
        }
        else if (frame_delta_v < speed) {
            accel = vector(-vel_dir.x * lateral_accel, -vel_dir.y * lateral_accel, -vel_dir.z * lateral_accel);
        }
        else {
            // Original bug kept: the whole velocity, not per step, as an acceleration.
            accel = vector(-velocity.x, -velocity.y, -velocity.z);
        }
        const vector new_velocity(accel.x * step_time + velocity.x, accel.y * step_time + velocity.y, accel.z * step_time + velocity.z);
        havok_body_set_linear_velocity(body, new_velocity);

        if (0.1f < dist) {
            // Bank and pitch into the horizontal part of it.
            const vector forward = body_orient.fvec.copy_flatten_normalize_safe();
            vector side_raw;
            side_raw.x = forward.z * UP_VECTOR.y - forward.y * UP_VECTOR.z;
            side_raw.y = forward.x * UP_VECTOR.z - forward.z * UP_VECTOR.x;
            side_raw.z = forward.y * UP_VECTOR.x - forward.x * UP_VECTOR.y;
            const vector side = side_raw.copy_normalize_safe();
            const float bank = (accel.x * side.x + side.y * 0.0f + side.z * accel.z) * info->flyer_bank_tilt_scalar;
            const float thrust = (forward.x * accel.x + forward.y * 0.0f + forward.z * accel.z) * info->flyer_thrust_tilt_scalar;
            thrust_up_target.x = side.x * bank + ANTIGRAVITY_VECTOR.x + thrust * forward.x;
            thrust_up_target.y = bank * side.y + ANTIGRAVITY_VECTOR.y + thrust * forward.y;
            thrust_up_target.z = bank * side.z + ANTIGRAVITY_VECTOR.z + thrust * forward.z;
            thrust_up_target.normalize_safe();
        }
    }

    // Where to look: the yaw acceleration and the offsets of the thrust.
    const float yaw_vel = angular.y;
    float thrust_pitch = 0.0f;
    float thrust_roll = 0.0f;
    float yaw_accel = 0.0f;
    bool turn = req_lookat_isvalid;
    if (!turn) {
        const vector look(velocity.x, info->flyer_def_lookat_y_scalar * velocity.y, velocity.z);
        if (flyer_min_lookat_vel_sq < look.y * look.y + look.x * look.x + look.z * look.z) {
            req_lookat = look.copy_normalize_safe();
            if (req_vel_isvalid || req_point_isvalid) {
                vector to_target(target.x - chassis_com_ws.x, target.y - chassis_com_ws.y, target.z - chassis_com_ws.z);
                to_target.normalize_safe();
                req_lookat.x = to_target.x * 0.9f + req_lookat.x;
                req_lookat.y = req_lookat.y + to_target.y * 0.9f;
                req_lookat.z = req_lookat.z + to_target.z * 0.9f;
            }
            turn = true;
        }
        else {
            yaw_accel = -angular.y * 0.5f;
        }
    }
    if (turn) {
        const vector forward = body_orient.fvec.copy_flatten_normalize_safe();
        const vector look = req_lookat.copy_flatten_normalize_safe();
        const float angle = fl_acos(flyer_clamp(look.x * forward.x + look.y * forward.y + look.z * forward.z, -1.0f, 1.0f));
        // The short way round and the long way round: whichever gets there first.
        float short_way = angle;
        float long_way = TWO_PI - angle;
        if (body_orient.rvec.x * look.x + body_orient.rvec.y * look.y + body_orient.rvec.z * look.z < 0.0f) {
            short_way = -short_way;
        }
        else {
            long_way = -long_way;
        }
        const float long_time = flyer_compute_target_time(long_way, yaw_vel, max_turn_vel, max_turn_accel);
        const float short_time = flyer_compute_target_time(short_way, yaw_vel, max_turn_vel, max_turn_accel);
        yaw_accel = flyer_compute_target_curve(0.0f <= long_time - short_time ? short_way : long_way, yaw_vel, max_turn_vel, max_turn_accel, step_time);
        const float pitch = (float)asin(flyer_clamp(req_lookat.y, -1.0f, 1.0f));
        const float max_offset_x = info->flyer_max_thrust_offset_x;
        thrust_pitch = 0.0f <= pitch - -max_offset_x ? pitch : -max_offset_x;
        if (0.0f <= thrust_pitch - max_offset_x) {
            thrust_pitch = max_offset_x;
        }
        if (req_lookat_isvalid) {
            const float max_offset_z = info->flyer_max_thrust_offset_z;
            const float roll = info->flyer_spin_bank_scalar * yaw_vel * 2.0f;
            thrust_roll = 0.0f <= roll - -max_offset_z ? roll : -max_offset_z;
            if (0.0f <= thrust_roll - max_offset_z) {
                thrust_roll = max_offset_z;
            }
        }
    }
    thrust_mat_cs.make_identity();
    thrust_mat_cs.rotate_around_local_z(thrust_roll);
    thrust_mat_cs.rotate_around_local_x(thrust_pitch);
    flyer_compute_world_turst_matrix(body_orient, thrust_mat_cs.uvec, thrust_mat_cs, thrust_mat_ws);

    // How to tilt: the angular acceleration around the axis in the ground plane that takes the thrust to its target.
    vector tilt_accel;
    tilt_accel.zero();
    const float frame_tilt_delta = max_tilt_accel * step_time;
    vector tilt_dir(angular.x, 0.0f, angular.z);
    const float tilt_speed = tilt_dir.mag_and_normalize_safe();
    const vector up = thrust_mat_ws.uvec;
    const vector up_target = thrust_up_target;
    const vector segment_start(up_target.x + up.x, up_target.y + up.y, up.z + up_target.z);
    const vector cross(up.y * up_target.z - up.z * up_target.y, up.z * up_target.x - up_target.z * up.x, up_target.y * up.x - up.y * up_target.x);
    const vector segment_dir(cross.x - segment_start.x, cross.y - segment_start.y, cross.z - segment_start.z);
    const float cross_mag = sqrtf(cross.y * cross.y + cross.x * cross.x + cross.z * cross.z);
    if (0.003f <= cross_mag) {
        vector tilt_axis(cross);
        plane ground;
        ground.normal = vector(0.0f, 1.0f, 0.0f);
        ground.offset = 0.0f;
        ix_collision collision;
        if (ix_linesegment_plane(collision, segment_start, segment_dir, ground)) {
            tilt_axis = collision.hit_point;
        }
        tilt_axis.normalize_safe();

        const float along_axis = tilt_axis.y * tilt_speed * tilt_dir.y + tilt_dir.x * tilt_speed * tilt_axis.x + tilt_axis.z * tilt_dir.z * tilt_speed;
        vector off_axis(angular.x - along_axis * tilt_axis.x, 0.0f - tilt_axis.y * along_axis, angular.z - tilt_axis.z * along_axis);
        const float off_axis_speed = off_axis.mag_and_normalize_safe();
        if (0.0f < along_axis) {
            if (frame_tilt_delta < off_axis_speed) {
                // Original bug kept: with the off-axis spin, not against it.
                tilt_accel = vector(off_axis.x * max_tilt_accel, off_axis.y * max_tilt_accel, off_axis.z * max_tilt_accel);
            }
            else {
                const float up_along = up.y * tilt_axis.y + up.x * tilt_axis.x + up.z * tilt_axis.z;
                vector up_flat(up.x - up_along * tilt_axis.x, up.y - tilt_axis.y * up_along, up.z - tilt_axis.z * up_along);
                const float target_along = up_target.y * tilt_axis.y + up_target.x * tilt_axis.x + up_target.z * tilt_axis.z;
                vector target_flat(up_target.x - target_along * tilt_axis.x, up_target.y - tilt_axis.y * target_along, up_target.z - tilt_axis.z * target_along);
                up_flat.normalize_safe();
                target_flat.normalize_safe();
                const float angle = fl_acos(flyer_clamp(target_flat.y * up_flat.y + target_flat.x * up_flat.x + target_flat.z * up_flat.z, -1.0f, 1.0f));
                max_tilt_vel = flyer_compute_target_curve(angle, along_axis, max_tilt_vel, sqrtf(frame_tilt_delta * frame_tilt_delta - off_axis_speed * off_axis_speed) / step_time, step_time);
                const float off_axis_accel = -off_axis_speed / step_time;
                tilt_accel = vector(max_tilt_vel * tilt_axis.x + off_axis_accel * off_axis.x, off_axis_accel * off_axis.y + tilt_axis.y * max_tilt_vel, off_axis_accel * off_axis.z + tilt_axis.z * max_tilt_vel);
            }
        }
        else if (tilt_speed <= frame_tilt_delta * 2.0f) {
            const float off_axis_accel = -off_axis_speed / step_time;
            const float along_accel = sqrtf(frame_tilt_delta * frame_tilt_delta * 4.0f - off_axis_speed * off_axis_speed);
            tilt_accel = vector(along_accel * tilt_axis.x + off_axis_accel * off_axis.x, tilt_axis.y * along_accel + off_axis_accel * off_axis.y, tilt_axis.z * along_accel + off_axis_accel * off_axis.z);
        }
        else {
            const float scale = -max_tilt_accel * 2.0f;
            tilt_accel = vector(tilt_dir.x * scale, tilt_dir.y * scale, tilt_dir.z * scale);
        }
    }
    else if (frame_tilt_delta < tilt_speed) {
        tilt_accel = vector(tilt_dir.x * -max_tilt_accel, tilt_dir.y * -max_tilt_accel, tilt_dir.z * -max_tilt_accel);
    }
    else {
        const float scale = -(tilt_speed / step_time);
        tilt_accel = vector(tilt_dir.x * scale, tilt_dir.y * scale, tilt_dir.z * scale);
    }
    const vector new_angular(angular.x + tilt_accel.x * step_time, yaw_accel * step_time + angular.y, tilt_accel.z * step_time + angular.z);
    havok_body_set_angular_velocity(body, new_angular);

    // What gravity and the velocity set above do not leave to the thrust: tilted too far, it falls.
    vector shortfall;
    shortfall.zero();
    if (0.0f <= thrust_mat_ws.uvec.y) {
        const float thrust = max_up_accel - -GRAVITY_MS2;
        const float thrust_up = thrust_mat_ws.uvec.y * thrust;
        if (thrust_up - GRAVITY_MS2 < 0.0f) {
            shortfall.y = thrust_up + ((shortfall.y - GRAVITY_MS2) - accel.y);
            shortfall.x = thrust * thrust_mat_ws.uvec.x + (shortfall.x - accel.x);
            shortfall.z = thrust_mat_ws.uvec.z * thrust + (shortfall.z - accel.z);
        }
    }
    else {
        shortfall.x = shortfall.x - accel.x;
        shortfall.y = (shortfall.y - GRAVITY_MS2) - accel.y;
        shortfall.z = shortfall.z - accel.z;
    }
    const float impulse_scale = vehicle_mass * step_time;
    const vector impulse(impulse_scale * shortfall.x, shortfall.y * impulse_scale, shortfall.z * impulse_scale);
    havok_body_apply_linear_impulse(body, impulse);

    // The turbine pitch follows the work: each share adds rpm up from idle.
    const float lift_share = flyer_clamp(flyer_clamp(accel.y + max_lateral_accel, 0.0f, 100000.0f) / (max_lateral_accel * 2.0f), 0.0f, 1.0f);
    const float lateral_share = flyer_clamp(sqrtf(accel.x * accel.x + 0.0f + accel.z * accel.z) / max_lateral_accel, 0.0f, 1.0f);
    const float tilt_share = flyer_clamp(sqrtf(tilt_accel.y * tilt_accel.y + tilt_accel.x * tilt_accel.x + tilt_accel.z * tilt_accel.z) / max_tilt_accel_full, 0.0f, 1.0f);
    const float yaw_share = flyer_clamp(yaw_accel / max_turn_accel_full, 0.0f, 1.0f); // Original bug kept: turning one way only.
    const float shortfall_share = flyer_clamp(sqrtf(shortfall.x * shortfall.x + shortfall.y * shortfall.y + shortfall.z * shortfall.z) / GRAVITY_MS2, 0.0f, 1.0f);
    const float rpm = shortfall_share * 3520.0f + yaw_share * 1100.0f + tilt_share * 1100.0f + lateral_share * 1540.0f + lift_share * 2200.0f + TURBINE_RPM_MIN;
    turbine_rpm_target = flyer_clamp(rpm, TURBINE_RPM_MIN, TURBINE_RPM_MAX);
}

// 0x00B68490 (:2111)
void flyer_do_frame_presim(flyer* fp)
{
    vehicle_ai_process_u_turn(fp);
    check_for_dead_driver(fp);
    fp->vai.active_control_info.max_braking_decel = fp->vai.base_control_info.max_braking_decel;
    fp->vai.active_control_info.max_radial_accel = fp->vai.base_control_info.max_radial_accel;
    vehicle_ai_check_extra_mass(fp);
    intersection_controller_process_vehicle(fp);
}

// 0x00BAAD30 (:2128)
void flyer_do_frame_postsim(flyer* fp)
{
    vector velocity;
    havok_body_get_linear_velocity(fp->havok_handle, velocity);
    vehicle::resource_dependent_data* v_data = fp->rdd.get_data();
    const float speed = sqrtf(velocity.y * velocity.y + velocity.x * velocity.x + velocity.z * velocity.z);

    // The gear only collides when it is down; projectiles stuck in it fall off when it goes up.
    if (v_data->m_wheel_bumper_component != NO_HANDLE && fp->flyer_flags.landing_gear_collision_off != fp->flyer_flags.landing_gear_up) {
        vehicle_component* gear = &v_data->m_components[v_data->m_wheel_bumper_component];
        if (fp->flyer_flags.landing_gear_up == 0) {
            hkpRigidBody* body = havok_body_get_pointer(fp->havok_handle);
            vehicle_component_set_hierarchy_collision(gear, fp, body != NULL ? body->getCollisionFilterInfo() : COLLIDE_VEHICLE);
        }
        else {
            vehicle_component_set_hierarchy_collision(gear, fp, COLLIDE_NO_COLLISION);
            constraint* cp = fp->host_constraint_ptr;
            while (cp != NULL) {
                constraint* next = cp->host_constraint_next != fp->host_constraint_ptr ? cp->host_constraint_next : NULL;
                if (cp->child_handle != NO_HANDLE) {
                    object* child = World.object_from_handle(cp->child_handle);
                    if (child != NULL && child->get_obj_type() == OT_ITEM && child->get_sub_type() == OT_SUB_ITEM_PROJECTILE && cp->host_attach_index != NO_HANDLE
                        && vehicle_get_component_index_from_shape_key(fp, cp->host_attach_index, cp->host_alt_hk_body_index) == v_data->m_wheel_bumper_component) {
                        World.destroy_object(cp, false);
                        static_cast<projectile*>(child)->detach();
                    }
                }
                cp = next;
            }
        }
        fp->flyer_flags.landing_gear_collision_off = fp->flyer_flags.landing_gear_up;
    }

    // Engine started: hop up a little.
    if (fp->startup_timer.valid() && fp->startup_timer.elapsed()) {
        fp->engine_effects_start();
        fp->engine_force = 1.0f;
        float mass;
        vector hop;
        matrix inertia;
        havok_body_get_mass_props(fp->havok_handle, mass, hop, inertia);
        hop.unrotate(fp->orient);
        hop.x = fp->pos.x + hop.x;
        hop.y = fp->pos.y + hop.y + 0.1f;
        hop.z = fp->pos.z + hop.z;
        fp->set_request_flyto_point(hop, true);
        fp->startup_timer.invalidate();
    }

    // The turbine.
    const float rpm_step = Frametime * TURBINE_RPM_PER_SECOND;
    const float rpm_difference = fp->turbine_rpm_target - fp->turbine_rpm;
    float rpm_change = 0.0f <= rpm_difference - -rpm_step ? rpm_difference : -rpm_step;
    if (0.0f <= rpm_change - rpm_step) {
        rpm_change = rpm_step;
    }
    const float new_rpm = fp->turbine_rpm + rpm_change;
    fp->turbine_load = TURBINE_RPM_PER_SECOND < rpm_difference ? 1.0f : 0.0f;
    fp->turbine_rpm = flyer_clamp(new_rpm, TURBINE_RPM_MIN, TURBINE_RPM_MAX);

    // A wing gone: random jolts.
    if (fp->m_flags.m_engine_running != 0) {
        for (unsigned int c = 0; c < v_data->m_num_components; c++) {
            const vehicle_component* comp = &v_data->m_components[c];
            // 0x20000: read here as the component being gone (meaning inferred)
            if (comp->get_type() != VCT_WING || (comp->get_flags() & 0x20000) == 0) {
                continue;
            }
            fp->wing_is_detached = true;
            if (fp->orient.uvec.y <= 0.0f) {
                continue;
            }
            const float chance = Frametime + Frametime; // twice a second on average
            if (fl_rand() <= chance) {
                vector jolt;
                jolt.rand_around_dir(vector(0.0f, -1.0f, 0.0f), 0.7f, 1.0f);
                const float strength = (fl_rand() + 0.5f) * fp->vehicle_mass * flyer_damage_force_scalar;
                jolt.x = strength * jolt.x;
                jolt.y = jolt.y * strength;
                jolt.z = strength * jolt.z;
                if (flyer_damage_force_speed_mark < speed) {
                    jolt.x = jolt.x * flyer_damage_force_speed_scalar;
                    jolt.y = jolt.y * flyer_damage_force_speed_scalar;
                    jolt.z = jolt.z * flyer_damage_force_speed_scalar;
                }
                havok_body_apply_point_impulse(fp->havok_handle, jolt, comp->get_world_space_position());
            }
        }
    }

    // Hovering: drift around the requested point.
    if (fp->m_flags.m_engine_running == 0 || 1.0f <= speed) {
        fp->hover_noise_vec.zero();
        fp->hover_noise_vec_new.zero();
        fp->hover_noise_vec_old.zero();
    }
    else {
        if (fp->hover_noise_timer.elapsed()) {
            fp->hover_noise_vec_old = fp->hover_noise_vec_new;
            fp->hover_noise_vec_new.rand_around_z(-1.0f, 1.0f);
            const float length = (fl_rand() + 0.5f) * flyer_hover_noise_length;
            fp->hover_noise_vec_new.x = length * fp->hover_noise_vec_new.x;
            fp->hover_noise_vec_new.y = fp->hover_noise_vec_new.y * length;
            fp->hover_noise_vec_new.z = length * fp->hover_noise_vec_new.z;
            fp->hover_noise_timer.set_rand(500, 1000);
            fp->hover_noise_dur = (float)fp->hover_noise_timer.time_until();
        }
        const float t = flyer_clamp((float)fp->hover_noise_timer.time_until() / fp->hover_noise_dur, 0.0f, 1.0f);
        fp->hover_noise_vec.x = fp->hover_noise_vec_new.x + (fp->hover_noise_vec_old.x - fp->hover_noise_vec_new.x) * t;
        fp->hover_noise_vec.y = fp->hover_noise_vec_new.y + (fp->hover_noise_vec_old.y - fp->hover_noise_vec_new.y) * t;
        fp->hover_noise_vec.z = fp->hover_noise_vec_new.z + (fp->hover_noise_vec_old.z - fp->hover_noise_vec_new.z) * t;
    }

    // Wind gusts.
    if (fp->info->vehicle_classification != BVT_BOMBER && fp->flyer_flags.on_ground == 0) {
        const float chance = Frametime + Frametime; // twice a second on average
        if (fl_rand() <= chance) {
            float wind_mph;
            float wind_direction;
            weather_wind_get_speed(&wind_mph, &wind_direction);
            vector wind_dir(0.0f, 0.0f, 1.0f);
            wind_dir.rotate_around_y_axis(wind_direction * DEGREES_TO_RADIANS);
            vector gust;
            gust.rand_around_dir(wind_dir, 0.8f, 1.0f);
            const float wind_ms = wind_mph * MPH_TO_MS;
            const float random = fl_rand() + 0.5f;
            const float strength = flyer_wind_force_scalar * wind_ms;
            gust.x = fp->vehicle_mass * random * gust.x * strength;
            gust.y = gust.y * random * strength * fp->vehicle_mass;
            gust.z = gust.z * random * strength * fp->vehicle_mass;
            havok_body_apply_linear_impulse(fp->havok_handle, gust);
        }
    }

    // The ground below the thrust: landing gear and jet wash.
    raycast_input input;
    input.start = fp->chassis_com_ws;
    input.end = vector(fp->chassis_com_ws.x - fp->thrust_mat_ws.uvec.x * JETWASH_RAY_LENGTH, fp->chassis_com_ws.y - fp->thrust_mat_ws.uvec.y * JETWASH_RAY_LENGTH, fp->chassis_com_ws.z - fp->thrust_mat_ws.uvec.z * JETWASH_RAY_LENGTH);
    input.radius = 0.0f;
    input.filter_params = NULL;
    input.filter_callback = NULL;
    input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    input.m_flags = 0;
    input.m_filter_info = COLLIDE_RAY_WORLD_ONLY;
    raycast_output output;
    if (havok_raycast(input, output) && fp->m_flags.m_engine_running != 0) {
        const raycast_output::hit_info& hit = output.m_hits[0];
        matrix wash_orient;
        wash_orient.make(hit.hit_normal);
        wash_orient.rotate_around_local_x(1.5707964f); // 90 degrees
        const vector wash_pos(wash_orient.uvec.x + hit.hit_pos.x, wash_orient.uvec.y + hit.hit_pos.y, wash_orient.uvec.z + hit.hit_pos.z);
        if (0.5f < hit.hit_fraction && fp->flyer_flags.landing_gear_up == 0) {
            fp->set_landing_gear_up(true);
        }
        else if (hit.hit_fraction < 0.5f && fp->flyer_flags.landing_gear_up != 0) {
            fp->set_landing_gear_up(false);
        }
        if (hit.hit_fraction < 0.9f && fp->jetwash == NO_HANDLE && fp->info->flyer_jetwash_effect != NO_HANDLE) {
            fp->jetwash = effects_play(fp->info->flyer_jetwash_effect, wash_pos, wash_orient, true, NO_HANDLE, -1, NO_HANDLE, 1.0f, false, EFFECTS_CATEGORY_NONE);
        }
        if (fp->jetwash != NO_HANDLE) {
            object* obj = World.object_from_handle(fp->jetwash);
            if (obj != NULL && obj->get_obj_type() == OT_EFFECT) {
                obj->pos = wash_pos;
                obj->orient = wash_orient;
                static_cast<object_effect*>(obj)->set_expression_variable(EFFECTS_VAR_PERCENT, fl_parameter(hit.hit_fraction, 1.0f, 0.35f, true));
            }
        }
    }
    else if (fp->jetwash != NO_HANDLE) {
        flyer_stop_effect(fp->jetwash);
        fp->jetwash = NO_HANDLE;
    }

    // Contrails with the forward speed.
    if (fp->wingtip_effect[0] != NO_HANDLE || fp->wingtip_effect[1] != NO_HANDLE) {
        vector current;
        havok_body_get_linear_velocity(fp->havok_handle, current);
        const float percent = fl_parameter(fp->orient.fvec.y * current.y + fp->orient.fvec.x * current.x + fp->orient.fvec.z * current.z, 0.3f, 3.0f, true);
        for (int i = 0; i < 2; i++) {
            if (fp->wingtip_effect[i] != NO_HANDLE) {
                object* obj = World.object_from_handle(fp->wingtip_effect[i]);
                if (obj != NULL && obj->get_obj_type() == OT_EFFECT) {
                    static_cast<object_effect*>(obj)->set_expression_variable(EFFECTS_VAR_PERCENT, percent);
                }
            }
        }
    }

    // Catching fire: some time after losing a wing, or soon when upside down.
    if (fp->m_flags.m_engine_running != 0 && fp->wing_is_detached && !fp->catch_fire_timer.valid()) {
        fp->catch_fire_timer.set(CATCH_FIRE_WING_DETACHED_MS);
    }
    if (fp->orient.uvec.y < -0.1f && fp->m_flags.m_engine_running != 0 && fp->m_flags.m_on_fire == 0 && fp->m_flags.m_invulnerable == 0
        && (!fp->catch_fire_timer.valid() || CATCH_FIRE_UPSIDE_DOWN_MS < fp->catch_fire_timer.time_until())) {
        fp->catch_fire_timer.set(CATCH_FIRE_UPSIDE_DOWN_MS);
    }
    if (fp->catch_fire_timer.valid() && fp->catch_fire_timer.elapsed()) {
        vehicle_catch_fire(fp, false);
        fp->catch_fire_timer.set(CATCH_FIRE_AGAIN_MS);
    }

    // The driver's door shuts with the engine running. 0.03: of its travel, still open.
    const vehicle_subinfo::seat_to_door_link_info* door_links = v_data->sub_info->m_seat_to_door_links;
    if (fp->rdd.is_loaded() && door_links != NULL && door_links->m_num_seats != 0) {
        const unsigned int door = door_links->m_door_component_indices[0];
        if (door != NO_HANDLE && (v_data->m_num_components <= door || 0.03f <= v_data->m_components[door].m_type_attribute.door.m_t_actual)
            && fp->m_flags.m_engine_running != 0 && door < v_data->m_num_components) {
            vehicle_component_close_door(&v_data->m_components[door], fp, 1.0f);
        }
    }
}

// 0x00B8F390 (:2522)
void flyer_blow_up(flyer* fp)
{
    vanim_clear_states(fp->vp);
    fp->engine_stop(false, false);
    flyer_stop_effect(fp->jetwash);
    fp->jetwash = NO_HANDLE;
    fp->engine_effects_stop();

    hkpRigidBody* body = havok_body_get_pointer(fp->havok_handle);
    // Original bug kept: removing shrinks the array while the index walks up it, so every other action is left.
    const int num_actions = body->m_actions.getSize();
    for (int i = 0; i < num_actions; i++) {
        hkpAction* action = body->m_actions[i];
        if (action != NULL) {
            Havok_world->removeAction(action);
        }
    }
    // 0.2: packed as the half float 0x3E4D in rfg.exe
    body->setLinearDamping(0.2f);
    body->setAngularDamping(0.2f);
    if (fp->last_player_damage.valid() && !fp->last_player_damage.elapsed()) {
        stats_increment_by_one(STATS_VEHICLES_DESTROYED_FLYERS);
    }
}

// 0x00B684A0 (:2559)
void flyer_set_controls_when_abandoned(flyer* fp)
{
    fp->req_lookat_isvalid = false;
}

// 0x00BC2E80 (.h:163)
const vector& flyer::get_com_ws()
{
    return chassis_com_ws;
}
