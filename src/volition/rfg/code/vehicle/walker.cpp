// Original: volition/rfg/code/vehicle/walker.cpp
//
// Walker: a legged vehicle moved by its animation. The chassis and each component are rigid bodies that are pushed each
// frame to where the animation puts them, a capsule (collision_model) does the real collision with the world, and the
// legs reach for the ground by foot IK.

#include "volition/rfg/code/vehicle/walker.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/camera/camera_free.h"
#include "volition/rfg/code/camera/camera_shake.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/effects/object_decal.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character.h"
#include "volition/rfg/code/geom/decal_info.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/havok/constraint_template.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/havok/havok_game_filter.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_clock.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_vehicle_walker.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/scripting/action_script/global_event.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/vehicle/vehicle_component.h"
#include "volition/rfg/code/vehicle/vehicle_effects.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/vehicle/vehicle_interaction_info.h"
#include "volition/rfg/code/vehicle/vehicle_subinfo.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ik_2link.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/util/string_comp.h"

#include <math.h>
#include <string.h>

// ---- file-static globals
static float Walker_cs_default_move_accel = 2.0f; // 0x016BA594
static float Walker_cs_jet_move_accel = 1.0f; // 0x016BA598
static float Walker_cs_fall_move_accel = 0.5f; // 0x016BA59C
static float Walker_cs_jet_turn_speed = 2.0f; // 0x016BA5A0
static float Walker_cs_fall_turn_speed = 0.5f; // 0x016BA5A4
static float Walker_jet_boost_acceleration = 10.0f; // 0x016BA5A8
static float Walker_jet_hover_acceleration = 7.5f; // 0x016BA5AC
static float Walker_jet_energy = 2.7f; // 0x016BA5B0
static float Walker_jet_boost_time = 0.4f; // 0x016BA5B4
static float Walker_jet_recharge_time = 0.2f; // 0x016BA5B8
static float Walker_jet_jump = 7.0f; // 0x016BA5BC
static float RFG_walker_energy_scale = 0.01f; // 0x016BA5C0
static float RFG_walker_attack_energy_scale = 10.0f; // 0x016BA5C4
static float WALKER_EXTRA_GRAVITY = -5.0f; // 0x016EB67C
static float WALKER_EXTRA_DOWNFORCE = -20.0f; // 0x016EB680
static float WALKER_VERTICAL_DAMPENING = 0.2f; // 0x016EB684

static obj_class_instantiator walker_instantiator("walker", alloc_walker, types_walker, bpo_index_walker, classname_walker, walker::pool_remaining, 0x1C300, 4); // 0x03770148
static object_pool<walker,4,1> walkerpool; // 0x0384FC10

static const float WALKER_RAGDOLL_RADIUS_SQ = 16.0f; // 4 m: humans this close are knocked over by a moving walker
static const float WALKER_FOOT_EFFECT_DIST_SQ = 625.0f; // 25 m from the local player
static const float WALKER_MOUNT_MAX_SPEED = 5.0f;
static const float WALKER_PARKED_SCALE = 0.125f; // parked with nobody in it: the body drifts to its pose slowly
static const float DEGREES_TO_RADIANS_WK = 0.017453f;
static const unsigned int WALKER_PROPERTY_ANIMATED = 7; // Havok property keys, as in rfg.exe
static const unsigned int WALKER_PROPERTY_COMPONENT = 0xC;

// Indices of the walker's states and actions in its animation tables.
static const int ANIM_STATE_DEFAULT = 0x1;
static const int ANIM_STATE_WALK = 0x18;
static const int ANIM_STATE_CHARGE_READY = 0x1B;
static const int ANIM_STATE_STRAFE_LEFT = 0x4C;
static const int ANIM_STATE_STRAFE_RIGHT = 0x4D;
static const int ANIM_STATE_BACKWARD = 0x4E;
static const int ANIM_STATE_MOUNTED = 0x5D;
static const int ANIM_STATE_PARKED = 0x6A;
static const int ANIM_STATE_RAM = 0x69;
static const int ANIM_STATE_FALL = 0x95;
static const int ANIM_STATE_TURN_LEFT = 0xA7;
static const int ANIM_STATE_TURN_RIGHT = 0xA8;
static const int ANIM_STATE_ENTERED = 0x6F;
static const int ANIM_ALIAS_ACTION_ATTACK = 0x3DC;
static const int ANIM_ALIAS_ACTION_LAND = 0x3DF;
static const int ANIM_ALIAS_ACTION_JUMP_LOOP = 0x3E0;
static const int ANIM_ALIAS_ACTION_TRANSITION = 0x400;
static const int ANIM_ALIAS_ACTION_FOOTSTEPS = 0x409;
static const int ANIM_ACTION_MOUNT = 0x316;
static const int ANIM_ACTION_ENTER = 0x14F;
static const int ANIM_ACTION_EXIT_CIVILIAN = 0x14E;
static const int ANIM_ACTION_EXIT_COMBAT = 0x2E1;
static const int ANIM_ACTION_SPAWN_IDLE = 0x2A4;
static const int ANIM_ACTION_MOUNTED_VEHICLE = 0x32A;
static const int ANIM_ACTION_LAND_HARD = 0x16C;
static const int ANIM_ACTION_LAND_RUN = 0x16E;
static const int ANIM_ACTION_LAND_RUN_SOFT = 0x16F;
static const int ANIM_ACTION_JUMP_MOVING = 0x191;
static const int ANIM_ACTION_JUMP_STANDING = 0x192;

// The player driving, if the most recent driver still is one.
static object* walker_driver_object(const walker* wp)
{
    if (wp->most_recent_driver_exit_time.valid() || wp->most_recent_driver == NO_HANDLE) {
        return NULL;
    }
    object* obj = World.object_from_handle(wp->most_recent_driver);
    return obj != NULL && obj->get_obj_type() == OT_HUMAN ? obj : NULL;
}

// Towards target by at most rate, without passing it.
static float walker_ramp(float current, float target, float rate)
{
    if (current < target) {
        const float next = current + rate;
        return 0.0f <= target - next ? next : target;
    }
    if (target < current) {
        const float next = current - rate;
        return target - next < 0.0f ? next : target;
    }
    return current;
}

static float walker_clamp(float value, float low, float high)
{
    if (value - low < 0.0f) {
        value = low;
    }
    if (0.0f <= value - high) {
        value = high;
    }
    return value;
}

// 0x00B84CB0 (:192)
static void walker_setup_constraints(walker* wp)
{
    vehicle::resource_dependent_data* v_data = wp->rdd.get_data();
    vehicle_subinfo* sub_info = v_data->sub_info;
    for (unsigned int t = 0; t < sub_info->m_num_constraint_templates; t++) {
        const constraint_template& templ = sub_info->m_constraint_templates[t];
        // Body 0 is the chassis.
        vehicle_component* child = &v_data->m_components[templ.body_b.get_obj_index() - 1];
        const int host_index = templ.body_a.get_obj_index();
        const unsigned int host_handle = host_index == 0 ? wp->havok_handle : v_data->m_components[host_index - 1].havok_handle;

        const matrix43 child_transform(child->get_world_space_position(), child->get_world_space_rotation());
        const vector& local = templ.body_b_transform.m_translation;
        const matrix& child_rot = child_transform.m_rotation;
        const vector pivot(child_transform.m_translation.x + child_rot.rvec.x * local.x + child_rot.uvec.x * local.y + child_rot.fvec.x * local.z,
            child_transform.m_translation.y + child_rot.rvec.y * local.x + child_rot.uvec.y * local.y + child_rot.fvec.y * local.z,
            child_transform.m_translation.z + child_rot.rvec.z * local.x + child_rot.uvec.z * local.y + child_rot.fvec.z * local.z);
        const matrix pivot_orient = templ.body_b_transform.m_rotation * child_rot;

        switch (templ.type) {
        case 0:
            child->m_constraint = havok_create_point_constraint(host_handle, child->havok_handle, pivot, templ.threshold);
            break;
        case 1: {
            const vector axis(-pivot_orient.uvec.x, -pivot_orient.uvec.y, -pivot_orient.uvec.z);
            child->m_constraint = havok_create_hinge_constraint(host_handle, child->havok_handle, pivot, axis, templ.hinge_data.limited != 0, templ.hinge_data.limit_min_angle * DEGREES_TO_RADIANS_WK, templ.hinge_data.limit_max_angle * DEGREES_TO_RADIANS_WK, templ.threshold);
            break;
        }
        case 2:
            child->m_constraint = havok_create_slide_constraint(host_handle, child->havok_handle, wp->orient.uvec, templ.prismatic_data.limit_min, templ.prismatic_data.limit_max, templ.threshold);
            break;
        case 3: {
            ragdoll_constraint_info info;
            info.twist_axis = pivot_orient.rvec;
            info.pivot = pivot;
            info.h_fov_neg = templ.ragdoll_data.twist_min * DEGREES_TO_RADIANS_WK;
            info.h_fov_pos = templ.ragdoll_data.twist_max * DEGREES_TO_RADIANS_WK;
            info.plane_axis = pivot_orient.fvec;
            info.v_fov_neg = templ.ragdoll_data.cone_min * DEGREES_TO_RADIANS_WK;
            info.v_fov_pos = templ.ragdoll_data.cone_max * DEGREES_TO_RADIANS_WK;
            child->m_constraint = havok_create_ragdoll_constraint(host_handle, child->havok_handle, info, templ.ragdoll_data.plane_min * DEGREES_TO_RADIANS_WK, templ.ragdoll_data.plane_max * DEGREES_TO_RADIANS_WK, templ.threshold);
            break;
        }
        default:
            break;
        }
    }
}

// 0x00B6D420 (:263)
// Sets a body's velocities, correcting for its centre of mass. When teleporting components (only in multiplayer and
// wrecking crew) it sets the rotation directly. 0x00B6D420 (:263)
static void walker_move_component(walker* wp, hkpRigidBody* rb, vector& move_vel, vector& rotate_vel, const vector& pos_delta, const matrix& rotate_delta)
{
    if (wp->w_flags.teleport_components == 0) {
        hkVector4f hk_velocity;
        game_to_havok_vector(hk_velocity, move_vel);
        rb->activate();
        rb->getMotion()->setLinearVelocity(hk_velocity);
        hkVector4f hk_angular;
        game_to_havok_vector(hk_angular, rotate_vel);
        rb->activate();
        rb->getMotion()->setAngularVelocity(hk_angular);

        // Rotating around the centre of mass moves the body's origin: move it back with the velocity.
        const matrix step(rotate_vel.x * Frametime, rotate_vel.z * Frametime, rotate_vel.y * Frametime);
        vector center_of_mass;
        vector origin;
        havok_to_game_vector(center_of_mass, rb->getCenterOfMassInWorld());
        havok_to_game_vector(origin, rb->getPosition());
        vector offset(origin.x - center_of_mass.x, origin.y - center_of_mass.y, origin.z - center_of_mass.z);
        offset.rotate(step);
        const float one_over_dt = 1.0f / Frametime;
        const vector correction(((center_of_mass.x - origin.x) + offset.x) * one_over_dt, ((center_of_mass.y - origin.y) + offset.y) * one_over_dt, ((center_of_mass.z - origin.z) + offset.z) * one_over_dt);
        vector velocity;
        havok_to_game_vector(velocity, rb->getLinearVelocity());
        velocity.x += correction.x;
        velocity.y += correction.y;
        velocity.z += correction.z;
        game_to_havok_vector(hk_velocity, velocity);
        rb->activate();
        rb->getMotion()->setLinearVelocity(hk_velocity);
        return;
    }
    if (!Multiplayer && !wrecking_crew_is_active()) {
        return;
    }
    hkVector4f hk_position = rb->getPosition();
    hkQuaternionf hk_rotation = rb->getRotation();
    // Original bug kept: the delta is overwritten by the current position, so the body stays where it is.
    vector position(pos_delta);
    havok_to_game_vector(position, hk_position);
    game_to_havok_vector(hk_position, position);
    matrix rotation;
    havok_to_game_matrix(rotation, hk_rotation);
    rotation = rotation * rotate_delta;
    game_to_havok_matrix(hk_rotation, rotation);
    rb->setPosition(hk_position);
    rb->setRotation(hk_rotation);
}

// 0x00B994F0 (:321)
// The ground under a foot. 0x00B994F0 (:321)
static bool walker_find_foot_info(walker* wp, bool left, vector& foot_pos, unsigned int& hit_obj, matrix* foot_orient, unsigned int* havok_shape_key, unsigned char* effect_mat_idx)
{
    const walker::resource_dependent_data* w_data = wp->w_rdd.get_data();
    const vehicle_component* foot = &wp->rdd.get_data()->m_components[left ? w_data->left_foot_component_idx : w_data->right_foot_component_idx];
    foot_pos = foot->get_world_space_position();
    if (foot_orient != NULL) {
        *foot_orient = foot->get_world_space_rotation();
    }
    foot_pos.y = wp->pos.y;
    raycast_input input;
    input.radius = 0.0f;
    input.start = vector(foot_pos.x, foot_pos.y + 0.25f, foot_pos.z);
    input.end = vector(foot_pos.x, foot_pos.y - 1.25f, foot_pos.z);
    input.m_filter_info = COLLIDE_RAY_DEFAULT;
    input.filter_params = NULL;
    input.filter_callback = NULL;
    input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    input.m_flags = 0;
    raycast_output output;
    const bool hit = havok_raycast_against_static_level_only(input, output);
    if (hit) {
        foot_pos = output.m_hits[0].hit_pos;
        hit_obj = output.m_hits[0].hit_object_handle;
        if (effect_mat_idx != NULL) {
            *effect_mat_idx = output.m_hits[0].hit_effect_material;
        }
        if (havok_shape_key != NULL) {
            *havok_shape_key = output.m_hits[0].hit_havok_shape_key;
        }
    }
    return hit;
}

// 0x00B6D8C0 (:490)
void types_walker(signed char& type, signed char& subtype)
{
    type = OT_VEHICLE;
    subtype = OT_SUB_VEHICLE_WALKER;
}

// 0x00B6D8D0 (:490)
int bpo_index_walker()
{
    return 0;
}

// 0x00B6D8E0 (:490)
const char* classname_walker()
{
    return "walker";
}

// 0x00B630C0 (.h:101)
const char* walker::get_classname_static()
{
    return "walker";
}

// 0x00B630D0 (.h:101)
int walker::get_bpo_index_static()
{
    return 0;
}

// 0x00BA3740 (.h:101)
const char* walker::get_classname() const
{
    return "walker";
}

// 0x00B6D8F0 (:490)
unsigned int walker::get_classname_hash()
{
    return walker_instantiator.this_hash;
}

// 0x00B85300 (:490)
int walker::pool_remaining()
{
    return walkerpool.get_free();
}

// 0x00B942F0 (:490)
object* alloc_walker()
{
    return walkerpool.alloc(0);
}

// 0x00B996E0 (:490)
void walker::pool_free()
{
    walkerpool.free(this);
}

// 0x00B6D900
walker::component_info::component_info()
{
}

// 0x00B6D910
wik_joint::wik_joint()
{
}

// 0x00B6D920
mp_x_collision_prediction_info::mp_x_collision_prediction_info()
{
}

// 0x00BA36F0 (:491)
walker::walker()
{
}

// 0x00BA3750
walker::~walker()
{
}

// 0x00B6D930 (:496)
hkpRigidBody* walker::get_supporting_rigid_body() const
{
    object* ground = World.object_from_handle(ground_obj_handle);
    if (ground != NULL && ground->havok_handle != NO_HANDLE) {
        return havok_body_get_pointer(ground->havok_handle);
    }
    return NULL;
}

// 0x00B6D960 (:508)
float walker::get_energy_scale()
{
    w_flags.reduce_impact_momentum = 1;
    if (w_flags.in_ramming_mode != 0) {
        return ram_momentum * 10.0f;
    }
    float fall_speed = actual_velocity.y;
    if (fall_speed < -2.0f) {
        fall_speed = fall_speed + 2.0f;
        return fall_speed * fall_speed * 0.02f;
    }
    if (vanim_is_playing_action(vp, ANIM_ALIAS_ACTION_ATTACK)) {
        if (control_scheme == WCS_FACING_CAMERA) {
            return 30.0f;
        }
    }
    else {
        if (vanim_is_in_state(vp, ANIM_STATE_PARKED)) {
            return 0.01f;
        }
        if (control_scheme == WCS_FACING_CAMERA) {
            return 5.0f;
        }
        if (w_flags.jump_jet_on == 0 || actual_velocity.y <= 1.0f) {
            return RFG_walker_energy_scale;
        }
    }
    return RFG_walker_attack_energy_scale;
}

// 0x00B94300 (:553)
bool walker::serialize(obj_prop_block& pblock, bool load)
{
    if (!vehicle::serialize(pblock, load)) {
        return false;
    }
    if (!load) {
        pblock.set(VEHICLE_PROP_TYPE_NAME, info->name);
        pblock.set<bool>(VEHICLE_PROP_AT_GROUND, false);
        return true;
    }
    // Upright.
    orient.uvec = vector(0.0f, 1.0f, 0.0f);
    orient.rvec.x = orient.fvec.z * orient.uvec.y - orient.fvec.y * orient.uvec.z;
    orient.rvec.y = orient.fvec.x * orient.uvec.z - orient.uvec.x * orient.fvec.z;
    orient.rvec.z = orient.uvec.x * orient.fvec.y - orient.fvec.x * orient.uvec.y;
    orient.rvec.normalize();
    orient.fvec.x = orient.rvec.y * orient.uvec.z - orient.rvec.z * orient.uvec.y;
    orient.fvec.y = orient.rvec.z * orient.uvec.x - orient.rvec.x * orient.uvec.z;
    orient.fvec.z = orient.rvec.x * orient.uvec.y - orient.rvec.y * orient.uvec.x;
    chassis_orient = orient;
    chassis_pos = pos;
    memset(&w_flags, 0, sizeof(w_flags));
    move_speed = -1.0f;

    int type = 0;
    if (!pblock.get_optional(VEHICLE_PROP_TYPE, type)) {
        char type_name[256];
        pblock.get(VEHICLE_PROP_TYPE_NAME, type_name);
        type = vehicle_get_type(type_name);
    }
    if (type < 0 || info->rig_name == NULL) {
        return false;
    }
    bool at_ground;
    pblock.get_optional<bool>(VEHICLE_PROP_AT_GROUND, at_ground);
    cur_rotation_speed = 0.0f;
    mp_sim_state = NULL;
    mp_data.attack_1_id = 0;
    mp_data.attack_2_id = 0;
    mp_data.attack_3_id = 0;
    mp_data.attack_4_id = 0;
    mp_data.charge_id = 0;
    aim_handle = -1;
    jump_jet_effect = NO_HANDLE;
    jump_jet_foley = -1;
    engine_load = 0.0f;
    block_movement_anim = -1;
    ground_obj_handle = NO_HANDLE;
    jump_timeout.set(0);
    if (Multiplayer && multi_is_server()) {
        remote_obj_flags.remote_object = 1;
        multi_core_object_create_remote_object(11, get_handle()); // 11: the walker's registered multiplayer type
    }
    return true;
}

// The bones of a leg. 0x00BB04B0 (:639), part.
static void walker_setup_leg(walker* wp, wik_joint& joint, const char* thigh, const char* calf, const char* foot, walker_ik_joints location)
{
    joint.anchor_tag = animlib_find_character_bone(wp->vp->aim_handle, thigh);
    joint.joint_tag = animlib_find_character_bone(wp->vp->aim_handle, calf);
    joint.ik_tag = animlib_find_character_bone(wp->vp->aim_handle, foot);
    joint.location = location;
    joint.cur_foot_p = 0.0f;
    joint.cur_foot_b = 0.0f;
}

// 0x00BB04B0 (:639)
bool walker::stream(bool load)
{
    if (!load && collision_model != NO_HANDLE) {
        havok_body_free(collision_model, false, true, false);
        collision_model = NO_HANDLE;
    }
    if (!vehicle::stream(load)) {
        return false;
    }
    if (!load) {
        w_rdd.set_loaded(false);
        return true;
    }
    w_rdd.set_loaded(true);
    resource_dependent_data* w_data = w_rdd.get_data();
    input_velocity.zero();
    const vehicle::resource_dependent_data* v_data = rdd.get_data();
    for (unsigned int c = 0; c < v_data->m_num_components; c++) {
        const char* bone_name = v_data->sub_info->m_components[c].bone_name;
        if (strstr(bone_name, "l-foot") != NULL) {
            w_data->left_foot_component_idx = (int)c;
        }
        if (strstr(bone_name, "r-foot") != NULL) {
            w_data->right_foot_component_idx = (int)c;
        }
    }
    vanim_change_state(vp, index_alias(ANIM_STATE_PARKED, (vanim_aliases)-1), index_alias(ANIM_ACTION_SPAWN_IDLE, (vanim_aliases)-1), 0.3f, false);
    jump_jet_foley_disable.invalidate();
    ground_normal = vector(0.0f, 1.0f, 0.0f);
    if (info->flags.is_walker_flamer) {
        model_type = WMT_FLAME;
        control_scheme = WCS_FACING_CAMERA;
        jump_jet_tag_idx = -1;
        walker_setup_leg(this, ik_joints[0], "l-thigh", "l-calf", "l-foot", WALKER_IK_JOINT_LEFT_LEG);
        walker_setup_leg(this, ik_joints[1], "r-thigh", "r-calf", "r-foot", WALKER_IK_JOINT_RIGHT_LEG);
    }
    else if (v_stricmp(info->name, "walker_min") == 0 || v_stricmp(info->name, "walker_min2") == 0 || v_stricmp(info->name, "walker_min3") == 0) {
        model_type = WMT_SMALL;
        control_scheme = WCS_CHARACTER_STYLE;
        jump_jet_tag_idx = get_tag_index_from_name("Large_Thruster_1");
        walker_setup_leg(this, ik_joints[0], "l_thigh", "l_calf", "l_foot", WALKER_IK_JOINT_LEFT_LEG);
        walker_setup_leg(this, ik_joints[1], "r_thigh", "r_calf", "r_foot", WALKER_IK_JOINT_RIGHT_LEG);
    }
    else {
        model_type = WMT_CIVILIAN;
        control_scheme = WCS_FACING_CAMERA;
        jump_jet_tag_idx = -1;
        walker_setup_leg(this, ik_joints[0], "l-thigh", "l-calf", "l-foot", WALKER_IK_JOINT_LEFT_LEG);
        walker_setup_leg(this, ik_joints[1], "r-thigh", "r-calf", "r-foot", WALKER_IK_JOINT_RIGHT_LEG);
    }
    y_offset = 0.0f;
    actual_velocity.zero();
    if (!walker_build(this)) {
        World.destroy_object(this, false);
        return false;
    }
    return true;
}

// 0x00BB4C40 (:768)
void walker::free_this()
{
    if (Multiplayer) {
        multi_core_object_process_object_free(this);
    }
    vehicle::free_this();
}

// 0x00B85310 (:786)
bool walker::get_tag_local_coords(int tag_index, vector& tag_pos, matrix* tag_orient) const
{
    if (!vehicle::get_tag_local_coords(tag_index, tag_pos, tag_orient)) {
        return false;
    }
    const unsigned int tag_type = (unsigned int)tag_index & 0xFF000000;
    if (tag_type != 0 || m_flags.m_corpse != 0) {
        // Types 3-5 are the vehicle's lights; only those on no component go with the root bone.
        if (tag_type != 0x3000000 && tag_type != 0x4000000 && tag_type != 0x5000000) {
            return true;
        }
        if (!rdd.is_loaded()) {
            return false;
        }
        const unsigned int light = ((unsigned int)tag_index >> 16) & 0xFF;
        const vehicle::resource_dependent_data* v_data = rdd.get_data();
        if (v_data->m_num_vehicle_lights <= light) {
            return false;
        }
        if (v_data->m_vehicle_lights[light].parent_idx != -1) {
            return true;
        }
    }
    matrix43 root;
    animlib_get_bone_transform(aim_handle, 1, root);
    tag_pos.unrotate(root.m_rotation);
    tag_pos.x += root.m_translation.x;
    tag_pos.y += root.m_translation.y;
    tag_pos.z += root.m_translation.z;
    if (tag_orient != NULL) {
        tag_orient->unrotate(root.m_rotation);
    }
    tag_pos.y = y_offset + tag_pos.y;
    return true;
}

// 0x00B854F0 (:845)
void walker::get_bbox(vector& bmin, vector& bmax) const
{
    if (!rdd.is_loaded()) {
        get_local_bbox(bmin, bmax);
        bmin.x += pos.x;
        bmin.y += pos.y;
        bmin.z += pos.z;
        bmax.x += pos.x;
        bmax.y += pos.y;
        bmax.z += pos.z;
        return;
    }
    vector local_min;
    vector local_max;
    get_local_bbox(local_min, local_max);
    bbox_instance(bmin, bmax, orient, pos, local_min, local_max);
}

// 0x00B85610 (:867)
void walker::get_local_bbox(vector& bmin, vector& bmax) const
{
    if (!rdd.is_loaded()) {
        bmin = vector(-0.75f, -0.75f, -0.75f);
        bmax = vector(0.75f, 0.75f, 0.75f);
        return;
    }
    animlib_get_bone_bbox(aim_handle, bmin, bmax);
    if (model_type == WMT_CIVILIAN) {
        bmin.x -= 0.5f;
        bmin.y -= 0.5f;
        bmin.z -= 1.35f;
        bmax.x += 0.5f;
        bmax.y += 0.6f;
        bmax.z += 0.75f;
    }
    else if (model_type == WMT_SMALL) {
        bmin.x -= 0.4f;
        bmin.y -= 0.5f;
        bmin.z -= 0.45f;
        bmax.x += 0.4f;
        bmax.y += 0.6f;
        bmax.z += 0.5f;
    }
    else {
        bmin.x -= 0.2f;
        bmin.y -= 0.6f;
        bmin.z -= 0.2f;
        bmax.x += 0.2f;
        bmax.y += 0.5f;
        bmax.z += 0.25f;
    }
}

// 0x00BB4C60 (:905)
void walker::update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data)
{
    hkpRigidBody* body = havok_body_get_pointer(collision_model);
    hkVector4f hk_pos;
    game_to_havok_vector(hk_pos, update_pos);
    if (body != NULL) {
        body->setPosition(hk_pos);
    }
    vehicle::update_pos_and_orient(update_pos, update_orient, set_havok_data);
}

// 0x00B6DA70 (:919)
void walker::hierarchy_notify(hierarchy_notify_code code)
{
    if (code == HN_PARENT_DESTROYED) {
        World.destroy_object(this, false);
    }
}

// 0x00BB4CE0 (:932)
void walker::destroy_notify()
{
    vehicle::destroy_notify();
    // Original: 0 is also skipped, which is no effect handle.
    if (jump_jet_effect != 0) {
        if (jump_jet_effect != NO_HANDLE) {
            object* obj = World.object_from_handle(jump_jet_effect);
            if (obj != NULL && obj->get_obj_type() == OT_EFFECT) {
                static_cast<object_effect*>(obj)->stop(object_effect::SS_STOP_EMITTING);
            }
        }
        jump_jet_effect = NO_HANDLE;
    }
    if (jump_jet_foley != -1) {
        audiolib_play_stop(jump_jet_foley);
        jump_jet_foley = -1;
    }
}

// 0x00B6DA90 (:964)
void walker_system_init()
{
    Walker_footprint_decal_id = decal_info_find("civ_walker_footstep");
    Walker_jetpack_effect_info = effects_get_handle("veh_walker_thruster_a");
    Large_walker_explosion_info = explosion_lookup("large_walker_smash");
    Small_walker_explosion_info = explosion_lookup("small_walker_smash");
}

// 0x00B6DAD0 (:984)
void walker_get_velocity(const walker* wp, vector& vel)
{
    vel.x = wp->pos.x - wp->last_pos.x;
    vel.y = wp->pos.y - wp->last_pos.y;
    vel.z = wp->pos.z - wp->last_pos.z;
    const float one_over_frametime = 1.0f / Frametime;
    vel.x *= one_over_frametime;
    vel.y *= one_over_frametime;
    vel.z *= one_over_frametime;
}

// 0x00BAD320 (:991)
bool walker_build(walker* wp)
{
    vehicle::resource_dependent_data* v_data = wp->rdd.get_data();
    vehicle_process_animations(wp);
    matrix43* matrices;
    animlib_get_instanced_model_matrices(wp->aim_handle, &matrices);
    hkpRigidBody* chassis = havok_body_get_pointer(wp->havok_handle);
    havok_set_property(chassis, WALKER_PROPERTY_ANIMATED, (void*)1);

    // The chassis goes to the root bone.
    vector root_pos(matrices[1].m_translation);
    root_pos.unrotate(wp->orient);
    root_pos.x += wp->pos.x;
    root_pos.y += wp->pos.y;
    root_pos.z += wp->pos.z;
    const matrix root_orient = matrices[1].m_rotation * wp->orient;
    hkTransformf transform;
    game_to_havok_transform(transform, root_pos, root_orient);
    chassis->setTransform(transform);
    wp->chassis_pos = root_pos;
    wp->chassis_orient = root_orient;

    const vector antigravity(0.0f, 9.8f, 0.0f);
    vehicle_antigravity* chassis_action = new vehicle_antigravity(chassis, antigravity);
    Havok_world->addAction(chassis_action);
    chassis_action->removeReference();

    // rfg.exe turns off Havok's report 0x65FD5E95 around this loop; reports are not reproduced.
    hkpRigidBody* bodies[74];
    for (unsigned int c = 0; c < v_data->m_num_components; c++) {
        vehicle_component* component = &v_data->m_components[c];
        wp->comp_info[c].has_children = 0;
        vehicle_component_create_rigid_body(component, wp, false);
        const matrix43& bone = matrices[v_data->component_bone_index[c]];
        vector bone_pos(bone.m_translation);
        bone_pos.unrotate(wp->orient);
        bone_pos.x += wp->pos.x;
        bone_pos.y += wp->pos.y;
        bone_pos.z += wp->pos.z;
        const matrix bone_orient = bone.m_rotation * wp->orient;
        hkpRigidBody* body = havok_body_get_pointer(component->havok_handle);
        bodies[c] = body;
        game_to_havok_transform(transform, bone_pos, bone_orient);
        body->setTransform(transform);
        havok_set_property(body, WALKER_PROPERTY_ANIMATED, (void*)1);
        component->set_orient_pos_in_world_space(wp, bone_orient, bone_pos);
        havok_set_property(body, WALKER_PROPERTY_COMPONENT, (void*)1);
        havok_body_set_angular_dampening(body, 0.1f);
        havok_body_set_linear_dampening(body, 0.1f);
        if (!component->create_renderable_instance(wp)) {
            return false;
        }
    }
    havok_add_entity_flush();
    for (unsigned int c = 0; c < v_data->m_num_components; c++) {
        vehicle_antigravity* action = new vehicle_antigravity(bodies[c], antigravity);
        Havok_world->addAction(action);
        action->removeReference();
    }
    walker_setup_constraints(wp);
    chassis->enableDeactivation(false); // the receiver is a spilled stack slot; inferred to be the chassis

    // Parents before children: by bone index.
    int order = 0;
    for (int bone = 0; bone < v_data->rig->num_bones; bone++) {
        for (unsigned int c = 0; c < v_data->m_num_components; c++) {
            if (v_data->component_bone_index[c] == bone) {
                wp->comp_transform_order[order++] = (int)c;
            }
        }
    }
    vehicle_build_turrets(wp);

    // The capsule that collides with the world.
    physics_create_info pci;
    pci.pos = vector(wp->pos.x + UP_VECTOR.x * 0.05f, UP_VECTOR.y * 0.05f + wp->pos.y, UP_VECTOR.z * 0.05f + wp->pos.z);
    pci.orient = wp->orient;
    float bottom_z = 0.0f;
    if (wp->model_type == WMT_CIVILIAN) {
        pci.radius = 2.5f;
        bottom_z = 1.4f;
    }
    else if (wp->model_type == WMT_SMALL) {
        pci.radius = 1.6f;
    }
    else {
        pci.radius = 1.75f;
    }
    pci.bottom.zero();
    pci.bottom.z = bottom_z;
    pci.top.z = bottom_z;
    pci.bottom.y = pci.bottom.y + pci.radius;
    pci.top.y = pci.bottom.y + 0.01f;
    pci.shape_type = (havok_shape_type)2; // capsule
    pci.angular_damping = 0.1f;
    pci.mass = wp->get_mass();
    pci.obj_ptr = wp;
    pci.physical_material = wp->get_physical_material(NO_HANDLE, NULL, NULL);
    pci.effect_material = wp->get_effect_material(NO_HANDLE, NULL, NULL);
    pci.flags |= 1;
    const vector bmin(-pci.radius, pci.bottom.y, -pci.radius);
    const vector bmax(pci.radius, pci.top.y, pci.radius);
    havok_body_calc_mass_props(bmin, bmax, pci.mass, pci.precalc_center_of_mass, pci.precalc_tensor);
    pci.collision_type = Havok_filter->getNewSystemGroup() << 16 | 0xE; // its own system group, layer 0xE
    pci.simulation_quality_type = (hkpCollidableQualityType)5;
    wp->collision_model = havok_body_create(pci);
    wp->hull_radius = pci.radius;
    hkpRigidBody* capsule = havok_body_get_pointer(wp->collision_model);
    if (capsule == NULL) {
        return false;
    }
    // It never turns.
    hkMatrix3f zero;
    zero.setZero();
    capsule->setInertiaInvLocal(zero);
    havok_set_property(capsule, WALKER_PROPERTY_ANIMATED, (void*)1);
    return true;
}

// 0x00B85850 (:1164)
void walker_steer(walker* wp, float stick_v, float stick_h)
{
    if (!wp->w_rdd.is_loaded()) {
        return;
    }
    controls_map_stick_to_circle(stick_h, stick_v, stick_h, stick_v);
    if (vanim_is_playing_action(wp->vp, ANIM_ACTION_MOUNT)) {
        return;
    }
    if (wp->block_movement_anim != -1) {
        if (vanim_is_playing_action(wp->vp, wp->block_movement_anim)) {
            if (wp->block_movement_type == BLOCK_MOVEMENT_POS || wp->block_movement_type == BLOCK_MOVEMENT_POS_AND_ORIENT) {
                stick_v = wp->input_velocity.z;
                stick_h = wp->input_velocity.x;
            }
        }
        else {
            wp->block_movement_anim = -1;
        }
    }

    object* driver = walker_driver_object(wp);
    if (driver != NULL && driver->get_sub_type() != OT_SUB_HUMAN_PLAYER) {
        driver = NULL;
    }
    matrix camera_orient;
    if (object_is_local_player(driver)) {
        camera_orient = Rfg_camera.real_orient;
    }
    else if (driver == NULL) {
        camera_orient = IDENTITY_MATRIX;
    }
    else {
        camera_orient = static_cast<player*>(driver)->mp_camera_orient;
    }

    const vector old_steering = wp->steering_vector;
    if (wp->control_scheme == WCS_FACING_CAMERA) {
        const float forward = wp->input_velocity.z;
        const float side_rate = forward <= 0.3f ? (wp->model_type == WMT_FLAME ? Frametime * 1.5f : Frametime * 3.0f) : Frametime * 0.5f;
        wp->input_velocity.x = walker_ramp(wp->input_velocity.x, stick_h, side_rate);
        float forward_rate = forward <= 0.0f ? Frametime * 1.5f : Frametime * 0.5f;
        if (stick_v < forward && wp->model_type != WMT_FLAME) {
            forward_rate *= 3.0f;
        }
        wp->input_velocity.z = walker_ramp(forward, stick_v, forward_rate);
        wp->steering_vector = vector(wp->input_velocity.x, 0.0f, wp->input_velocity.z);
        wp->steering_vector.rotate_around_y_axis(wp->orient.extract_heading());
    }
    else if (wp->control_scheme == WCS_CHARACTER_STYLE) {
        float accel = Walker_cs_default_move_accel;
        if (wp->ground_obj_handle == NO_HANDLE) {
            accel = wp->w_flags.jump_jet_on != 0 ? Walker_cs_jet_move_accel : Walker_cs_fall_move_accel;
        }
        const float rate = Frametime * accel;
        wp->input_velocity.x = walker_ramp(wp->input_velocity.x, stick_h, rate);
        wp->input_velocity.z = walker_ramp(wp->input_velocity.z, stick_v, rate);
        wp->steering_vector = vector(wp->input_velocity.x, 0.0f, wp->input_velocity.z);
        wp->steering_vector.rotate_around_y_axis(camera_orient.extract_heading());
    }

    const float input_mag = sqrtf(wp->steering_vector.x * wp->steering_vector.x + wp->steering_vector.y * wp->steering_vector.y + wp->steering_vector.z * wp->steering_vector.z);
    wp->move_speed = input_mag * 9.0f; // 9: metres per second at full stick
    float blend_input = (input_mag * 9.0f) / 9.0f;
    wp->steering_vector.y = 0.0f;
    int state = ANIM_STATE_DEFAULT;
    if (wp->time_since_last_supported.elapsed() && wp->ground_obj_handle == NO_HANDLE) {
        wp->w_flags.jump_take_off = 1;
        state = ANIM_STATE_FALL;
    }
    else if (input_mag <= 0.03f) {
        if (vanim_is_in_state(wp->vp, ANIM_STATE_WALK)) {
            vanim_set_blend_tree_control_input(wp->vp, ANIM_STATE_WALK, 0.0f);
            if (0.1f <= vanim_get_blend_tree_ramped_input(wp->vp, ANIM_STATE_WALK)) {
                wp->steering_vector = old_steering;
                wp->move_speed = 0.0f;
                state = ANIM_STATE_WALK;
            }
            else {
                wp->steering_vector.zero();
            }
        }
        wp->w_flags.in_ramming_mode = 0;
        if (state == ANIM_STATE_DEFAULT && wp->control_scheme == WCS_FACING_CAMERA && (wp->w_flags.rotating_left != 0 || wp->w_flags.rotating_right != 0)) {
            state = wp->w_flags.rotating_left != 0 ? ANIM_STATE_TURN_LEFT : ANIM_STATE_TURN_RIGHT;
            blend_input = fabsf(wp->cur_rotation_speed / (Frametime * 1.14f));
        }
    }
    else if (wp->control_scheme == WCS_CHARACTER_STYLE) {
        state = ANIM_STATE_WALK;
        wp->steering_vector.normalize();
    }
    else {
        if (wp->control_scheme == WCS_FACING_CAMERA) {
            // Eight way movement reduced to four: 0.7853982 is 45 degrees, 2.3561945 is 135.
            const vector& fvec = wp->orient.fvec;
            const vector& rvec = wp->orient.rvec;
            const float angle = vec_delta_ang(fvec, wp->steering_vector, UP_VECTOR);
            if (-0.7853982f < angle && angle <= 0.7853982f) {
                wp->steering_vector = fvec;
                state = ANIM_STATE_WALK;
            }
            else if (0.7853982f < angle && angle < 2.3561945f) {
                wp->steering_vector = rvec;
                state = ANIM_STATE_STRAFE_RIGHT;
            }
            else if (-0.7853982f < angle || angle <= -2.3561945f) {
                wp->steering_vector = vector(-fvec.x, -fvec.y, -fvec.z);
                state = ANIM_STATE_BACKWARD;
            }
            else {
                wp->steering_vector = vector(-rvec.x, -rvec.y, -rvec.z);
                state = ANIM_STATE_STRAFE_LEFT;
            }
            if (input_mag < 0.4f) {
                wp->w_flags.in_ramming_mode = 0;
            }
            else {
                if (wp->w_flags.in_ramming_mode != 0) {
                    float energy_rate;
                    if (wp->w_flags.reduce_impact_momentum == 0) {
                        const float momentum = Frametime * 2.0f + wp->ram_momentum;
                        wp->ram_momentum = 1.0f - momentum < 0.0f ? 1.0f : momentum;
                        energy_rate = Frametime * 0.5f;
                    }
                    else {
                        const float momentum = wp->ram_momentum - Frametime * 4.0f;
                        wp->ram_momentum = 0.0f <= 0.0f - momentum ? 0.0f : momentum;
                        energy_rate = Frametime * 2.0f;
                    }
                    wp->ram_max_energy_used = energy_rate + wp->ram_max_energy_used;
                    if (wp->ram_momentum <= 0.0f || 1.0f < wp->ram_max_energy_used) {
                        wp->w_flags.in_ramming_mode = 0;
                    }
                    if (0.25f < wp->ram_momentum) {
                        state = ANIM_STATE_RAM;
                    }
                }
                wp->w_flags.reduce_impact_momentum = 0;
            }
        }
        wp->steering_vector.normalize();
    }

    // Landing.
    bool skip_support_timer = false;
    if (wp->w_flags.jump_take_off != 0) {
        if (wp->ground_obj_handle == NO_HANDLE) {
            skip_support_timer = true;
        }
        else if (!vanim_is_playing_action(wp->vp, ANIM_ALIAS_ACTION_LAND) && wp->w_flags.jump_jet_on == 0) {
            const int frame = vanim_get_anim_frame(wp->vp, ANIM_ALIAS_ACTION_JUMP_LOOP);
            vanim_ramp_out_action(wp->vp, ANIM_ALIAS_ACTION_JUMP_LOOP, 3);
            // -4: m/s of falling; 31: frames; 501: milliseconds in the air
            if (-4.0f <= wp->ground_impact_momentum || (frame != -1 && frame < 31) || wp->time_since_last_supported.time_since() < 501) {
                wp->w_flags.jump_take_off = 0;
                wp->time_since_last_supported.set(200);
            }
            else {
                const float strength = walker_clamp(wp->ground_impact_momentum / -12.0f, 0.0f, 1.0f);
                camera_shake_play("jump_land", strength, false);
                bool block = true;
                if (wp->desired_move_speed <= 5.5f || wp->ground_impact_momentum <= -18.0f) {
                    vanim_play_action(wp->vp, index_alias(ANIM_ACTION_LAND_HARD, (vanim_aliases)ANIM_ALIAS_ACTION_LAND), 0x20, 1.0f, 1.0f);
                    wp->block_movement_anim = ANIM_ACTION_LAND_HARD;
                }
                else if (strength != 1.0f) {
                    vanim_play_action(wp->vp, index_alias(ANIM_ACTION_LAND_RUN_SOFT, (vanim_aliases)ANIM_ALIAS_ACTION_LAND), 0x20, 1.0f, strength);
                    block = false;
                }
                else {
                    vanim_play_action(wp->vp, index_alias(ANIM_ACTION_LAND_RUN, (vanim_aliases)ANIM_ALIAS_ACTION_LAND), 0x20, 1.0f, strength);
                    wp->block_movement_anim = ANIM_ACTION_LAND_RUN;
                }
                if (block) {
                    wp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
                }
                wp->jump_timeout.set(800);
            }
            wp->ground_impact_momentum = 0.0f;
        }
    }
    if (!skip_support_timer && wp->ground_obj_handle != NO_HANDLE) {
        wp->time_since_last_supported.set(200); // 0x016EAE6C
    }

    if (vanim_has_trigger_just_elapsed(wp->vp, Trigger_release_movement, wp->block_movement_anim, -1)) {
        wp->w_flags.jump_take_off = 0;
        if (0.3f < input_mag) {
            vanim_ramp_out_action(wp->vp, wp->block_movement_anim, 3);
        }
        wp->block_movement_anim = -1;
        wp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
    }
    if (vanim_is_in_state(wp->vp, state)) {
        vanim_set_blend_tree_control_input(wp->vp, state, blend_input);
    }
    else {
        vanim_change_state(wp->vp, index_alias(state, ANIM_ALIAS_NONE), index_alias(-1, (vanim_aliases)-1), 0.3f, false);
        vanim_set_blend_tree_init_control_input(wp->vp, state, blend_input);
    }
    if (wp->w_flags.in_ramming_mode == 0) {
        wp->ram_max_energy_used -= Frametime;
    }
}

// 0x00BB83A0 (:1424)
void walker_process_movement_presim(walker* wp)
{
    human* driver = static_cast<human*>(walker_driver_object(wp));
    player* player_driver = driver != NULL && driver->get_sub_type() == OT_SUB_HUMAN_PLAYER ? static_cast<player*>(driver) : NULL;

    vector action_delta;
    vector state_delta;
    vector anim_local_delta;
    animlib_get_position_change(action_delta, state_delta, anim_local_delta, wp->aim_handle);
    action_delta.unrotate(wp->orient);
    wp->vertical_anim_speed = action_delta.y / Frametime;

    // The jump jet.
    if (wp->w_flags.jump_jet_on == 0) {
        const float energy = Frametime / Walker_jet_recharge_time + wp->jump_jet_energy;
        wp->jump_jet_energy = Walker_jet_energy - energy < 0.0f ? Walker_jet_energy : energy;
        wp->jump_jet_engage_time = 0.0f;
        if (wp->jump_jet_effect != 0) {
            if (wp->jump_jet_effect != NO_HANDLE) {
                object* obj = World.object_from_handle(wp->jump_jet_effect);
                if (obj != NULL && obj->get_obj_type() == OT_EFFECT) {
                    static_cast<object_effect*>(obj)->stop(object_effect::SS_STOP_EMITTING);
                }
            }
            wp->jump_jet_effect = NO_HANDLE;
        }
    }
    else {
        wp->jump_jet_foley_disable.set(250);
        if (wp->jump_jet_effect == NO_HANDLE) {
            wp->jump_jet_effect = effects_play(Walker_jetpack_effect_info, wp->get_handle(), wp->jump_jet_tag_idx, true, NO_HANDLE, 1.0f, false);
        }
        if (wp->ground_obj_handle != NO_HANDLE && wp->w_flags.jump_take_off == 0) {
            wp->ground_obj_handle = NO_HANDLE;
            wp->w_flags.jump_take_off = 1;
            wp->time_since_last_supported.set(0);
            if (!wp->jump_timeout.elapsed()) {
                return;
            }
            const float speed = wp->jump_jet_speed + 0.5f;
            wp->jump_jet_speed = speed - Walker_jet_jump < 0.0f ? Walker_jet_jump : speed;
            vanim_play_action(wp->vp, index_alias(wp->actual_velocity_mag <= 1.0f ? ANIM_ACTION_JUMP_STANDING : ANIM_ACTION_JUMP_MOVING, ANIM_ALIAS_JUMP), 0x20, 1.0f, 1.0f);
            wp->jump_timeout.set(1200);
        }
        if (wp->jump_jet_energy <= Walker_jet_energy - Walker_jet_boost_time) {
            wp->jump_jet_speed = Walker_jet_hover_acceleration * Frametime + wp->jump_jet_speed;
        }
        else {
            wp->jump_jet_speed = Walker_jet_boost_acceleration * Frametime + wp->jump_jet_speed;
            if (wp->jump_jet_foley == -1) {
                wp->jump_jet_foley = audiolib_cue_play(Ga_cue_ids.jetpack_loop_walker, NULL, NULL);
            }
        }
        camera_shake_play("walker_mini_thrust", 1.0f, false);
    }
    if (wp->jump_jet_foley_disable.valid() && wp->jump_jet_foley_disable.elapsed() && wp->jump_jet_foley != -1) {
        audiolib_play_stop(wp->jump_jet_foley);
        wp->jump_jet_foley = -1;
        wp->jump_jet_foley_disable.invalidate();
    }

    // The velocity: from the steering and animation, or towards the vehicle being mounted.
    vector velocity;
    float speed;
    if (wp->w_flags.is_mounting == 0) {
        if (wp->ground_obj_handle == NO_HANDLE) {
            velocity = vector(wp->steering_vector.x * 10.0f, wp->steering_vector.y * 10.0f, wp->steering_vector.z * 10.0f);
        }
        else {
            const float state_speed = sqrtf(state_delta.x * state_delta.x + state_delta.y * state_delta.y + state_delta.z * state_delta.z);
            velocity.x = action_delta.x / Frametime + (state_speed * wp->steering_vector.x) / Frametime;
            velocity.y = action_delta.y / Frametime + (wp->steering_vector.y * state_speed) / Frametime;
            velocity.z = action_delta.z / Frametime + (wp->steering_vector.z * state_speed) / Frametime;
        }
        speed = sqrtf(velocity.y * velocity.y + velocity.x * velocity.x + velocity.z * velocity.z);
    }
    else {
        vehicle* mount = NULL;
        if (wp->mount_vehicle != NO_HANDLE) {
            object* obj = World.object_from_handle(wp->mount_vehicle);
            if (obj != NULL && obj->get_obj_type() == OT_VEHICLE) {
                mount = static_cast<vehicle*>(obj);
            }
        }
        if (!vanim_is_playing_action(wp->vp, ANIM_ACTION_MOUNT)) {
            // Mounted: the driver goes along as a passenger of the vehicle. Original bug kept: no driver is not checked for.
            const vehicle::resource_dependent_data* v_data = wp->rdd.get_data();
            for (unsigned int c = 0; c < v_data->m_num_components; c++) {
                hkpRigidBody* body = havok_body_get_pointer(v_data->m_components[c].havok_handle);
                if (body != NULL) {
                    havok_set_ignore_property(body, driver->get_handle());
                }
            }
            hkpRigidBody* chassis = havok_body_get_pointer(wp->havok_handle);
            if (chassis != NULL) {
                havok_set_ignore_property(chassis, driver->get_handle());
            }
            hkpRigidBody* capsule = havok_body_get_pointer(wp->collision_model);
            if (capsule != NULL) {
                havok_set_ignore_property(capsule, driver->get_handle());
            }
            driver->hflags.disallow_vehicle_exit = 0;
            vi_exit_data exit_data;
            exit_data.flags.teleport = 1;
            exit_data.flags.maintain_position = 1;
            vehicle_interaction_request_exit(driver, &exit_data);
            human_enable_gravity(driver, false);
            vi_enter_data enter_data;
            enter_data.destination_seat = VSI_INVALID;
            memset(&enter_data.flags, 0, sizeof(enter_data.flags));
            enter_data.specified_seat = VSI_PASSENGER_1;
            enter_data.seat_selection_method = VI_SEAT_SELECTION_METHOD_SPECIFIED;
            vehicle_interaction_request_enter_internal(driver, mount, &enter_data);
            driver->hflags.disallow_vehicle_exit = 1;
            vehicle_antigravity* action = new vehicle_antigravity(capsule, vector(0.0f, 9.8f, 0.0f));
            Havok_world->addAction(action);
            action->removeReference();
            vanim_play_action(mount->vp, index_alias(ANIM_ACTION_MOUNTED_VEHICLE, (vanim_aliases)-1), 0x10, 1.0f, 1.0f);
            vanim_change_state(mount->vp, index_alias(ANIM_STATE_MOUNTED, (vanim_aliases)-1), index_alias(-1, (vanim_aliases)-1), 0.3f, false);
            mount->set_child(wp, -1, -1, true, false);
            wp->w_flags.is_mounting = 0;
        }
        wp->mount_vector.x -= anim_local_delta.x;
        wp->mount_vector.y -= anim_local_delta.y;
        wp->mount_vector.z -= anim_local_delta.z;
        if (mount != NULL) {
            vector offset(wp->mount_vector);
            offset.unrotate(mount->orient);
            const float one_over_dt = 1.0f / Frametime;
            velocity.x = walker_clamp(one_over_dt * ((mount->pos.x - offset.x) - wp->pos.x), -WALKER_MOUNT_MAX_SPEED, WALKER_MOUNT_MAX_SPEED);
            velocity.y = walker_clamp(one_over_dt * ((mount->pos.y - offset.y) - wp->pos.y), -WALKER_MOUNT_MAX_SPEED, WALKER_MOUNT_MAX_SPEED);
            velocity.z = walker_clamp(one_over_dt * ((mount->pos.z - offset.z) - wp->pos.z), -WALKER_MOUNT_MAX_SPEED, WALKER_MOUNT_MAX_SPEED);
            speed = sqrtf(velocity.y * velocity.y + velocity.x * velocity.x + velocity.z * velocity.z);
        }
        else {
            velocity.zero();
            speed = 0.0f;
        }
    }
    wp->move_speed = speed;

    // Where to face.
    matrix look;
    if (object_is_local_player(player_driver)) {
        if (vanim_is_playing_action(wp->vp, ANIM_ACTION_MOUNT)) {
            object* mount = wp->mount_vehicle != NO_HANDLE ? World.object_from_handle(wp->mount_vehicle) : NULL;
            look = mount != NULL && mount->get_obj_type() == OT_VEHICLE ? mount->orient : wp->orient;
        }
        else if (Rfg_camera.mode == CAMERA_FREE_MODE) {
            look = Rfg_camera.ideal_orient;
        }
        else {
            look = wp->orient;
        }
        if (!vanim_is_playing_action(wp->vp, ANIM_ACTION_ENTER)) {
            if (wp->model_type == WMT_SMALL) {
                ui_tool_tips_add_tip("Light_Walker", 0, NULL);
            }
            else if (wp->model_type == WMT_FLAME) {
                ui_tool_tips_add_tip("Combat_Walker", 0, NULL);
            }
        }
    }
    else if (player_driver == NULL) {
        look = wp->orient;
    }
    else {
        look = player_driver->mp_camera_orient;
    }

    bool turn = false;
    float heading = 0.0f;
    float max_turn = 0.0f;
    if (wp->control_scheme == WCS_FACING_CAMERA) {
        if (Multiplayer || !camera_free_is_swinging()) {
            heading = look.extract_heading();
            max_turn = (wp->move_speed + (wp->model_type == WMT_FLAME ? 1.7f : 1.2f)) * Frametime;
            turn = true;
        }
    }
    else if (wp->control_scheme == WCS_CHARACTER_STYLE && (Multiplayer || 0.1f < wp->steering_vector.x * wp->steering_vector.x + wp->steering_vector.y * wp->steering_vector.y + wp->steering_vector.z * wp->steering_vector.z)) {
        heading = fl_atan2_safe(wp->steering_vector.x, wp->steering_vector.z);
        float rate = wp->move_speed + 3.14f;
        if (wp->ground_obj_handle == NO_HANDLE) {
            rate = wp->w_flags.jump_jet_on != 0 ? Walker_cs_jet_turn_speed : Walker_cs_fall_turn_speed;
        }
        max_turn = rate * Frametime;
        turn = true;
    }
    if (wp->m_flags.m_occupied != 0 && turn) {
        float delta = fl_angle_cap_radians(heading - fl_atan2_safe(wp->orient.fvec.x, wp->orient.fvec.z), -3.1415927f);
        if (wp->block_movement_anim != -1 && (wp->block_movement_type == BLOCK_MOVEMENT_ORIENT || wp->block_movement_type == BLOCK_MOVEMENT_POS_AND_ORIENT)) {
            delta = 0.0f;
        }
        if (max_turn < fabsf(delta)) {
            delta = delta <= 0.0f ? -max_turn : max_turn;
        }
        wp->orient.rotate_around_local_y(delta);
        wp->w_flags.rotating_left = 0;
        wp->w_flags.rotating_right = 0;
        wp->cur_rotation_speed = delta;
        if (0.001f < delta) {
            wp->w_flags.rotating_right = 1;
        }
        else if (delta < -0.001f) {
            wp->w_flags.rotating_left = 1;
        }
    }

    const bool local_driver = object_is_local_player(walker_driver_object(wp));
    if (local_driver) {
        camera_shake_play("walker_idle", 1.0f, false);
    }
    player* local_player = player_get_local_player();

    // Footsteps.
    const bool left_step = vanim_has_trigger_just_elapsed(wp->vp, Trigger_left_foot, ANIM_ALIAS_ACTION_FOOTSTEPS, -1);
    if (left_step || vanim_has_trigger_just_elapsed(wp->vp, Trigger_right_foot, ANIM_ALIAS_ACTION_FOOTSTEPS, -1)) {
        if (local_driver) {
            float intensity = wp->model_type == WMT_CIVILIAN ? 1.0f : (wp->model_type == WMT_FLAME ? 0.6f : 0.5f);
            if (wp->move_speed == 0.0f) {
                intensity *= 0.4f;
            }
            else {
                const float scale = (wp->move_speed * 0.8f) / 9.0f + 0.2f;
                intensity *= scale - 1.0f < 0.0f ? scale : 1.0f;
            }
            camera_shake_play("walker_step", intensity, false);
        }
        if (local_player != NULL) {
            const float dx = local_player->pos.x - wp->pos.x;
            const float dy = local_player->pos.y - wp->pos.y;
            const float dz = local_player->pos.z - wp->pos.z;
            if (dx * dx + dy * dy + dz * dz < WALKER_FOOT_EFFECT_DIST_SQ) {
                vector foot_pos;
                unsigned int hit_obj;
                unsigned int shape_key;
                unsigned char effect_material;
                if (walker_find_foot_info(wp, left_step, foot_pos, hit_obj, NULL, &shape_key, &effect_material) && wp->info->footstep_effects != NULL && hit_obj != NO_HANDLE) {
                    const material_shape_type shape_type = rfg_material_get_shape_type(World.object_from_handle(hit_obj), shape_key);
                    const unsigned int effect = wp->info->footstep_effects->get_footstep_effect_handle(effect_material, shape_type, left_step ? FOOTSTEP_LEFT : FOOTSTEP_RIGHT, 5.0f < wp->move_speed ? FOOTSTEP_RUNNING : FOOTSTEP_WALKING);
                    effects_play(effect, foot_pos, wp->orient, false, NO_HANDLE, -1, NO_HANDLE, 1.0f, false, EFFECTS_CATEGORY_NONE);
                }
            }
        }
    }

    // The ground smash of an attack.
    if (vanim_has_trigger_just_elapsed(wp->vp, Trigger_impact_start, ANIM_ALIAS_ACTION_ATTACK, -1)) {
        const bool civilian = wp->model_type == WMT_CIVILIAN;
        const float forward_offset = civilian ? 4.85f : 3.37f; // 0x016EB690, 0x016EB68C
        const vector smash_pos(wp->pos.x + wp->orient.fvec.x * forward_offset, wp->pos.y + wp->orient.fvec.y * forward_offset, wp->pos.z + wp->orient.fvec.z * forward_offset);
        explosion_create(civilian ? Large_walker_explosion_info : Small_walker_explosion_info, wp, NULL, smash_pos, wp->orient, NULL, NULL, false);
    }

    // Footprints on the terrain.
    const bool left_release = vanim_has_trigger_just_elapsed(wp->vp, Trigger_left_foot_release, ANIM_ALIAS_ACTION_FOOTSTEPS, -1);
    if ((left_release || vanim_has_trigger_just_elapsed(wp->vp, Trigger_right_foot_release, ANIM_ALIAS_ACTION_FOOTSTEPS, -1)) && local_player != NULL) {
        const float dx = local_player->pos.x - wp->pos.x;
        const float dy = local_player->pos.y - wp->pos.y;
        const float dz = local_player->pos.z - wp->pos.z;
        vector foot_pos;
        matrix foot_orient;
        unsigned int hit_obj;
        if (dx * dx + dy * dy + dz * dz < WALKER_FOOT_EFFECT_DIST_SQ && walker_find_foot_info(wp, left_release, foot_pos, hit_obj, &foot_orient, NULL, NULL) && hit_obj != NO_HANDLE) {
            object* ground = World.object_from_handle(hit_obj);
            if (ground != NULL && ground->get_obj_type() == OT_ZONE) {
                rl_decal_parameters params;
                decal_info_get(&params, Walker_footprint_decal_id);
                params.m_pos = foot_pos;
                params.m_transform.make_uvec(foot_orient.uvec, foot_orient.fvec);
                const unsigned int decal = object_decal_apply(ground, params, 0.0f, NO_HANDLE, NO_HANDLE, NO_HANDLE);
                if (decal != NO_HANDLE) {
                    automobile_tire_track_decal_start_fade(decal);
                }
            }
        }
    }

    // The capsule: gravity, slopes and the push.
    hkpRigidBody* capsule = havok_body_get_pointer(wp->collision_model);
    vector capsule_pos;
    matrix capsule_orient;
    havok_to_game_transform(capsule_pos, capsule_orient, capsule->getTransform());
    vector capsule_velocity;
    havok_to_game_vector(capsule_velocity, capsule->getLinearVelocity());
    const float slope = wp->ground_obj_handle == NO_HANDLE ? 0.0f : wp->ground_normal.y;
    vector desired;
    if (wp->w_flags.is_mounting == 0) {
        if (wp->ground_obj_handle == NO_HANDLE && wp->w_flags.jump_jet_on == 0) {
            // Falling: it keeps what it had.
            velocity.x = capsule_velocity.x;
            velocity.z = capsule_velocity.z;
        }
        velocity.y = capsule_velocity.y;
        desired = vector(velocity.x, 0.0f, velocity.z);
    }
    else {
        desired = velocity;
    }
    wp->desired_move_speed = desired.is_null() ? 0.0f : sqrtf(desired.x * desired.x + desired.y * desired.y + desired.z * desired.z);

    if (wp->ground_obj_handle == NO_HANDLE) {
        if (wp->last_ground_obj_handle != NO_HANDLE && wp->w_flags.is_mounting == 0 && wp->w_flags.jump_jet_on == 0) {
            // Just stepped off something: stay down.
            float y = velocity.y;
            if (0.0f < y) {
                y = WALKER_VERTICAL_DAMPENING * y;
            }
            velocity.y = WALKER_EXTRA_DOWNFORCE * Frametime + y;
        }
    }
    else if (wp->ground_test_ts.elapsed()) {
        // Is it still ground under there?
        raycast_input input;
        const float start_y = wp->ground_world_pos.y + 0.2f;
        input.start = vector(wp->ground_world_pos.x, start_y, wp->ground_world_pos.z);
        input.end = vector(wp->ground_world_pos.x, start_y - 0.35f, wp->ground_world_pos.z);
        input.radius = 0.05f;
        input.filter_params = NULL;
        input.filter_callback = NULL;
        input.m_filter_info = COLLIDE_RAY_DEFAULT;
        input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
        input.m_flags = 0;
        raycast_output output;
        bool still_ground = false;
        if (havok_spherecast(input, output)) {
            const vector& center = output.m_hits[0].hit_center_pos;
            vector up(wp->pos.x - center.x, (wp->pos.y - center.y) + 1.2f, wp->pos.z - center.z);
            up.normalize();
            still_ground = 0.25f <= up.y;
        }
        if (!still_ground) {
            wp->ground_obj_handle = NO_HANDLE;
        }
    }
    else {
        wp->ground_test_ts.set(300);
    }
    wp->last_ground_obj_handle = wp->ground_obj_handle;
    if (wp->w_flags.jump_jet_on == 0) {
        wp->jump_jet_speed = wp->ground_obj_handle == NO_HANDLE ? velocity.y : 0.0f;
    }
    else {
        velocity.y = wp->jump_jet_speed;
    }
    if (wp->w_flags.jump_jet_on == 0 && wp->w_flags.is_mounting == 0) {
        velocity.y = WALKER_EXTRA_GRAVITY * Frametime + velocity.y;
        // Too steep: pushed back down it. 0.8 and 0.7: the up component of the ground normal
        if (0.0f < slope && ((slope < 0.8f && 0.0f < wp->move_speed) || slope < 0.7f)) {
            const vector& n = wp->ground_normal;
            const vector side(UP_VECTOR.z * n.y - UP_VECTOR.y * n.z, UP_VECTOR.x * n.z - UP_VECTOR.z * n.x, UP_VECTOR.y * n.x - UP_VECTOR.x * n.y);
            vector uphill(side.z * n.y - side.y * n.z, side.x * n.z - side.z * n.x, side.y * n.x - side.x * n.y);
            uphill.normalize();
            const float along = uphill.y * velocity.y + velocity.x * uphill.x + uphill.z * velocity.z;
            if (along < 0.0f) {
                const float push = walker_clamp((1.0f - slope) * -along * 10.0f, 0.0f, 3.0f);
                velocity.y = uphill.y * push + velocity.y;
                velocity.z = uphill.z * push + velocity.z;
                velocity.x = velocity.x + push * uphill.x;
            }
        }
    }

    // The body settles by what the feet could not reach. 2: metres per second
    wp->y_offset = walker_ramp(wp->y_offset, -(wp->ik_joints[1].correction_amount + wp->ik_joints[0].correction_amount) * 0.5f, 2.0f * Frametime);

    const vector new_pos(capsule_pos.x + velocity.x * Frametime, capsule_pos.y + velocity.y * Frametime, capsule_pos.z + velocity.z * Frametime);
    capsule_orient.orthogonalize();
    capsule_orient.inverse();
    capsule_orient.unrotate(wp->orient);
    float p;
    float b;
    float h;
    capsule_orient.extract_angles(p, b, h);
    const float one_over_dt = 1.0f / Frametime;
    vector rotate_vel(p * one_over_dt, h * one_over_dt, b * one_over_dt);
    if (wp->w_flags.jump_take_off == 0 && 0.0f < velocity.y) {
        object* ground = World.object_from_handle(wp->ground_obj_handle);
        if (ground != NULL && ground->get_obj_type() != OT_ZONE && ground->get_obj_type() != OT_MOVER) {
            velocity.y = -0.5f * velocity.y; // 0x016EB668: bounced back off anything else
        }
    }
    walker_move_component(wp, capsule, velocity, rotate_vel, new_pos, IDENTITY_MATRIX);
}

// 0x00BBA2B0 (:1946)
void walker_do_frame_presim(walker* wp)
{
    if (!wp->w_rdd.is_loaded()) {
        return;
    }
    if (Multiplayer) {
        multi_game_walker_predict_collision(wp);
    }
    if (Multiplayer && multi_is_server() && multi_game_object_vehicle_walker_is_client_authoritative(wp)) {
        multi_walker_state state;
        bool exceeded_extrap_time;
        if (wp->mp_sim_state->obtain_state(multi_core_clock_get_time(), &state, exceeded_extrap_time)) {
            multi_game_object_vehicle_walker_apply_sim_state(&state, wp);
        }
    }
    vehicle::resource_dependent_data* v_data = wp->rdd.get_data();
    if (wp->vii.num_humans != 0) {
        if (1.0f < wp->forward_velocity) {
            const int num_humans = World.get_num_objects<human>();
            for (int i = 0; i < num_humans; i++) {
                human* hp = World.get_object<human>(i);
                if (!hp->rdd.is_loaded() || hp->is_killed() || hp->vehicle_handle != NO_HANDLE) {
                    continue;
                }
                const float dx = hp->pos.x - wp->pos.x;
                const float dy = hp->pos.y - wp->pos.y;
                const float dz = hp->pos.z - wp->pos.z;
                if (dy * dy + dx * dx + dz * dz < WALKER_RAGDOLL_RADIUS_SQ) {
                    human_ragdoll_on_impact(hp, 2.0f, 7.0f, false);
                }
            }
        }
    }
    else if (wp->model_type == WMT_SMALL || wp->model_type == WMT_FLAME || wp->model_type == WMT_CIVILIAN) {
        // Nobody in it: it parks.
        if (!vanim_is_in_state(wp->vp, ANIM_STATE_PARKED)) {
            wp->steering_vector.zero();
            wp->w_flags.in_ramming_mode = 0;
            if (!vanim_is_in_state(wp->vp, ANIM_STATE_PARKED)) {
                if (vanim_is_playing_action(wp->vp, ANIM_ALIAS_ACTION_TRANSITION)) {
                    vanim_pause_and_ramp_out_action(wp->vp, ANIM_ALIAS_ACTION_TRANSITION, 5);
                }
                const int action = wp->model_type == WMT_CIVILIAN ? ANIM_ACTION_EXIT_CIVILIAN : ANIM_ACTION_EXIT_COMBAT;
                vanim_change_state(wp->vp, index_alias(ANIM_STATE_PARKED, (vanim_aliases)-1), index_alias(action, ANIM_ALIAS_VEHICLE_TRANSITION), 0.3f, false);
                wp->block_movement_anim = ANIM_ALIAS_ACTION_TRANSITION;
                wp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
                wp->input_velocity.zero();
                if (wp->get_parent() != NO_HANDLE) {
                    wp->remove_from_parent();
                }
            }
        }
    }

    walker_process_movement_presim(wp);

    const bool parked = vanim_is_in_state(wp->vp, ANIM_STATE_PARKED) && wp->get_parent() == NO_HANDLE;
    const bool attacking = vanim_is_playing_action(wp->vp, ANIM_ALIAS_ACTION_ATTACK);

    // The chassis to the root bone.
    matrix43 root;
    animlib_get_bone_transform(wp->aim_handle, 1, root);
    const matrix inverse_root = root.m_rotation.copy_inverse();
    vector root_pos(root.m_translation);
    root_pos.unrotate(wp->orient);
    root_pos.x += wp->pos.x;
    root_pos.y += wp->pos.y;
    root_pos.z += wp->pos.z;
    hkpRigidBody* chassis = havok_body_get_pointer(wp->havok_handle);
    vector chassis_body_pos;
    matrix chassis_body_orient;
    havok_to_game_transform(chassis_body_pos, chassis_body_orient, chassis->getTransform());
    wp->chassis_pos_last_frame = root_pos;
    const vector chassis_delta(root_pos.x - chassis_body_pos.x, root_pos.y - chassis_body_pos.y, root_pos.z - chassis_body_pos.z);
    vector chassis_velocity(chassis_delta.x / Frametime, chassis_delta.y / Frametime, chassis_delta.z / Frametime);
    if (parked) {
        chassis_velocity.x *= WALKER_PARKED_SCALE;
        chassis_velocity.y *= WALKER_PARKED_SCALE;
        chassis_velocity.z *= WALKER_PARKED_SCALE;
    }
    const matrix world_orient = root.m_rotation * wp->orient;
    const matrix chassis_rotate = chassis_body_orient.copy_inverse() * world_orient;
    float p;
    float b;
    float h;
    chassis_rotate.extract_angles(p, b, h);
    const float one_over_dt = 1.0f / Frametime;
    vector chassis_angular(one_over_dt * p, one_over_dt * h, one_over_dt * b);
    if (parked) {
        chassis_angular.x *= WALKER_PARKED_SCALE;
        chassis_angular.y *= WALKER_PARKED_SCALE;
        chassis_angular.z *= WALKER_PARKED_SCALE;
    }
    walker_move_component(wp, chassis, chassis_velocity, chassis_angular, chassis_delta, chassis_rotate);

    // Each component to its bone, within how fast it may go.
    const matrix root_to_world = inverse_root * world_orient;
    for (unsigned int n = 0; n < v_data->m_num_components; n++) {
        const int c = wp->comp_transform_order[n];
        walker::component_info& info = wp->comp_info[c];
        matrix43 bone;
        animlib_get_bone_transform(wp->aim_handle, v_data->component_bone_index[c], bone);
        const matrix target_orient = (bone.m_rotation * inverse_root) * world_orient;
        vector target_pos(bone.m_translation.x - root.m_translation.x, bone.m_translation.y - root.m_translation.y, bone.m_translation.z - root.m_translation.z);
        target_pos.unrotate(root_to_world);
        target_pos.z = target_pos.z + root_pos.z;
        target_pos.y = root_pos.y + target_pos.y + wp->y_offset;
        target_pos.x = target_pos.x + root_pos.x;
        info.debug_orient = target_orient;

        hkpRigidBody* body = havok_body_get_pointer(v_data->m_components[c].havok_handle);
        vector body_pos;
        matrix body_orient;
        havok_to_game_transform(body_pos, body_orient, body->getTransform());

        const float dx = target_pos.x - info.anim_pos.x;
        const float dy = target_pos.y - info.anim_pos.y;
        const float dz = target_pos.z - info.anim_pos.z;
        float max_speed = (sqrtf(dy * dy + dx * dx + dz * dz) + wp->actual_velocity_mag) / Frametime;
        if (!attacking) {
            max_speed += 12.0f;
        }
        else if (0.0f <= max_speed - 30.0f) {
            max_speed = 30.0f;
        }
        info.anim_pos = target_pos;
        info.debug_pos = body_pos;
        const vector delta(target_pos.x - body_pos.x, target_pos.y - body_pos.y, target_pos.z - body_pos.z);
        vector velocity(delta.x / Frametime, delta.y / Frametime, delta.z / Frametime);
        const float speed = sqrtf(velocity.y * velocity.y + velocity.x * velocity.x + velocity.z * velocity.z);
        info.component_error = speed * Frametime;
        if (parked) {
            max_speed *= WALKER_PARKED_SCALE;
        }
        if (speed <= max_speed * 0.7f) {
            info.collide_ts.invalidate();
        }
        else {
            // Held back for a second by something it hit: it goes through.
            if (!info.collide_ts.valid()) {
                info.collide_ts.set(1000);
            }
            if (info.collide_ts.elapsed()) {
                hkVector4f hk_target;
                game_to_havok_vector(hk_target, target_pos);
                body->setPosition(hk_target);
                velocity.zero();
            }
            else if (max_speed < speed) {
                velocity.normalize();
                velocity.x *= max_speed;
                velocity.y *= max_speed;
                velocity.z *= max_speed;
            }
        }
        info.desired_pos = vector(velocity.x * Frametime + body_pos.x, velocity.y * Frametime + body_pos.y, velocity.z * Frametime + body_pos.z);

        const matrix anim_rotate = info.anim_orient.copy_inverse() * target_orient;
        info.anim_orient = target_orient;
        float ap;
        float ab;
        float ah;
        anim_rotate.extract_angles(ap, ab, ah);
        float max_angular = sqrtf(ab * ab + ap * ap + ah * ah) / Frametime + (attacking ? 1.5f : 4.0f);

        const matrix body_rotate = body_orient.copy_inverse() * target_orient;
        body_rotate.extract_angles(p, b, h);
        p = fl_angle_cap_radians(p, -3.1415927f);
        h = fl_angle_cap_radians(h, -3.1415927f);
        b = fl_angle_cap_radians(b, -3.1415927f);
        matrix rotate_delta;
        rotate_delta.set(p, b, h);
        vector angular(one_over_dt * p, one_over_dt * h, one_over_dt * b);
        if (parked) {
            max_angular *= WALKER_PARKED_SCALE;
        }
        if (max_angular < sqrtf(angular.y * angular.y + angular.x * angular.x + angular.z * angular.z)) {
            angular.normalize();
            angular.x *= max_angular;
            angular.y *= max_angular;
            angular.z *= max_angular;
        }
        matrix step;
        step.set(angular.x * Frametime, angular.z * Frametime, angular.y * Frametime);
        info.desired_orient = body_orient * step;
        walker_move_component(wp, body, velocity, angular, delta, rotate_delta);
    }
    wp->w_flags.teleport_components = 0;
    wp->ground_obj_handle = NO_HANDLE;
}

// 0x00B6DBC0 (:2158)
static bool walker_ground_detect_callback_walker(object* hit_objp, void* filter_params)
{
    return hit_objp != filter_params;
}

// A bone of the animation in the world. 0x00BA3760 (:2163), part: rfg.exe inlines it twice, each time the bone
// transform through matrix43(wp->orient) (0x005A4490) with matrix43::operator* inlined.
static void walker_bone_in_world(const walker* wp, int bone, matrix& rotation, vector& position)
{
    matrix43 transform;
    animlib_get_bone_transform(wp->aim_handle, bone, transform);
    const matrix43 world = transform * matrix43(wp->orient);
    position = world.m_translation;
    rotation = world.m_rotation;
    position.x = wp->pos.x + position.x;
    position.z = position.z + wp->pos.z;
    position.y = wp->y_offset + wp->pos.y + position.y;
}

// 0x00BA3760 (:2163)
void walker_do_frame_postsim(walker* wp)
{
    if (!wp->w_rdd.is_loaded()) {
        return;
    }
    wp->adjust_transforms();
    wp->last_pos = wp->pos;
    hkpRigidBody* capsule = havok_body_get_pointer(wp->collision_model);

    // What it stands on: the terrain with the best normal, or else anything else.
    raycast_input input;
    input.start = wp->pos;
    if (wp->model_type == WMT_CIVILIAN) {
        input.start.x = wp->orient.fvec.x * 1.4f + input.start.x;
        input.start.y = wp->orient.fvec.y * 1.4f + input.start.y;
        input.start.z = input.start.z + wp->orient.fvec.z * 1.4f;
    }
    input.m_flags = 0;
    input.radius = wp->hull_radius;
    input.start.y = input.radius + 0.05f + input.start.y;
    input.end = vector(input.start.x, input.start.y - 0.1f, input.start.z); // 0.1: 0x016EB590
    input.m_filter_info = capsule->getCollisionFilterInfo();
    input.filter_callback = walker_ground_detect_callback_walker;
    input.filter_params = wp;
    input.m_hit_collector_type = raycast_input::COLLECT_ALL_HITS;
    raycast_output output;
    if (havok_spherecast(input, output)) {
        unsigned int terrain_handle = NO_HANDLE;
        unsigned int other_handle = NO_HANDLE;
        vector terrain_normal;
        vector terrain_pos;
        vector other_normal;
        vector other_pos;
        bool found_terrain = false;
        bool none_accepted = true;
        bool any_facing_up = false;
        for (int i = 0; i < output.m_hits.getSize(); i++) {
            const raycast_output::hit_info& hit = output.m_hits[i];
            object* obj = World.object_from_handle(hit.hit_object_handle);
            if (obj == NULL) {
                continue;
            }
            const bool not_terrain = obj->get_obj_type() != OT_ZONE;
            const float normal_y = hit.hit_normal.y;
            if (0.0f < normal_y) {
                any_facing_up = true;
            }
            if (not_terrain ? normal_y <= 0.3f : normal_y <= 0.7f) {
                continue;
            }
            if (none_accepted) {
                none_accepted = false;
                terrain_normal.y = -1.0f;
                other_normal.y = -1.0f;
            }
            if (not_terrain) {
                other_handle = hit.hit_object_handle;
                if (other_normal.y < normal_y) {
                    other_normal = hit.hit_normal;
                    other_pos = hit.hit_pos;
                }
            }
            else {
                terrain_handle = hit.hit_object_handle;
                if (terrain_normal.y < normal_y) {
                    terrain_normal = hit.hit_normal;
                    terrain_pos = hit.hit_pos;
                    found_terrain = true;
                }
            }
        }
        if (!none_accepted) {
            if (found_terrain) {
                wp->ground_normal = terrain_normal;
                wp->ground_world_pos = terrain_pos;
                wp->ground_obj_handle = terrain_handle;
            }
            else {
                wp->ground_normal = other_normal.y <= 0.8f ? vector(0.0f, 1.0f, 0.0f) : other_normal;
                wp->ground_world_pos = other_pos;
                wp->ground_obj_handle = other_handle;
            }
        }
        else {
            wp->ground_obj_handle = NO_HANDLE;
            if (any_facing_up && wp->w_flags.jump_jet_on != 0) {
                // Jetting against a slope too steep: it slides.
                const hkVector4f& current = capsule->getLinearVelocity();
                hkVector4f slid;
                slid.set(current(0), current(1) - Frametime * 9.8f, current(2), current(3));
                capsule->activate();
                capsule->getMotion()->setLinearVelocity(slid);
            }
        }
    }

    // The vehicle follows the capsule.
    if (wp->get_parent() == NO_HANDLE || wp->w_flags.is_mounting != 0) {
        vector capsule_pos;
        matrix capsule_orient;
        havok_to_game_transform(capsule_pos, capsule_orient, capsule->getTransform());
        if (wp->control_scheme == WCS_CHARACTER_STYLE && capsule_orient.uvec.y < 0.995f) {
            capsule_pos.z = capsule_pos.z + capsule_orient.uvec.z * 2.0f;
            capsule_pos.y = (capsule_orient.uvec.y * 2.0f + capsule_pos.y) - 2.0f;
            capsule_pos.x = capsule_orient.uvec.x * 2.0f + capsule_pos.x;
        }
        wp->pos = capsule_pos;
        wp->update_pos_and_orient(wp->pos, wp->orient, false);
    }

    // The feet.
    matrix43* instanced;
    if (animlib_get_instanced_model_matrices(wp->vp->aim_handle, &instanced)) {
        static matrix43 tmp_matrices[128]; // rfg.exe keeps these on the stack (0xC00 bytes)
        bool matrices_ready = false;
        for (int j = 0; j < 2; j++) {
            vector target;
            if (wp->ik_joints[j].process(wp, target)) {
                if (!matrices_ready) {
                    animlib_get_bone_matrices_from_instanced(wp->vp->aim_handle, tmp_matrices);
                    matrices_ready = true;
                }
                wp->ik_joints[j].do_ik(wp, target, tmp_matrices);
            }
        }
        if (matrices_ready) {
            animlib_instance_bone_matrices(wp->vp->aim_handle, tmp_matrices, NULL, 0);
            animlib_update_vertex_to_model_matrices(wp->vp->aim_handle);
        }
    }

    havok_to_game_vector(wp->actual_velocity, capsule->getLinearVelocity());
    const vector& actual = wp->actual_velocity;
    wp->actual_velocity_mag = sqrtf(actual.x * actual.x + actual.y * actual.y + actual.z * actual.z);
    if (0.0f < wp->desired_move_speed) {
        // Far slower than asked: walking into something.
        const float flat_speed = sqrtf(actual.x * actual.x + 0.0f + actual.z * actual.z);
        if (3.0f < fabsf(flat_speed - wp->desired_move_speed)) {
            wp->input_velocity.x *= 0.75f;
            wp->input_velocity.y *= 0.75f;
            wp->input_velocity.z *= 0.75f;
        }
    }

    // Everything rendered follows the animation.
    walker_bone_in_world(wp, 1, wp->chassis_orient, wp->chassis_pos);
    vehicle::resource_dependent_data* v_data = wp->rdd.get_data();
    for (unsigned int c = 0; c < v_data->m_num_components; c++) {
        matrix rotation;
        vector position;
        walker_bone_in_world(wp, v_data->component_bone_index[c], rotation, position);
        v_data->m_components[c].set_orient_pos_in_world_space(wp, rotation, position);
    }
    wp->adjust_child_transforms(false, false);
    wp->w_flags.jump_jet_on = 0;
}

// 0x00B86530 (:2486)
void walker_blow_up(walker* wp)
{
    if (wp->jump_jet_effect != 0) {
        if (wp->jump_jet_effect != NO_HANDLE) {
            object* obj = World.object_from_handle(wp->jump_jet_effect);
            if (obj != NULL && obj->get_obj_type() == OT_EFFECT) {
                static_cast<object_effect*>(obj)->stop(object_effect::SS_STOP_EMITTING);
            }
        }
        wp->jump_jet_effect = NO_HANDLE;
    }
    if (wp->jump_jet_foley != -1) {
        audiolib_play_stop(wp->jump_jet_foley);
        wp->jump_jet_foley = -1;
    }
    hkpRigidBody* chassis = havok_body_get_pointer(wp->havok_handle);
    // Original bug kept: removing shrinks the array while the index walks up it, so every other action is left.
    const int num_actions = chassis->m_actions.getSize();
    for (int i = 0; i < num_actions; i++) {
        hkpAction* action = chassis->m_actions[i];
        if (action != NULL) {
            Havok_world->removeAction(action);
        }
    }
    wp->pos = wp->chassis_pos;
    wp->orient = wp->chassis_orient;
    havok_remove_property(chassis, WALKER_PROPERTY_ANIMATED);
    havok_body_free(wp->collision_model, true, true, false);
    wp->collision_model = NO_HANDLE;
}

// The attacks share their conditions: not while one plays, in transition, parked, falling or ramming, nor for flamers.
static bool walker_can_attack(walker* wp)
{
    return wp->model_type != WMT_FLAME && !vanim_is_playing_action(wp->vp, ANIM_ALIAS_ACTION_ATTACK) && !vanim_is_playing_action(wp->vp, ANIM_ALIAS_ACTION_TRANSITION)
        && !vanim_is_in_state(wp->vp, ANIM_STATE_PARKED) && !vanim_is_in_state(wp->vp, ANIM_STATE_FALL) && wp->w_flags.in_ramming_mode == 0;
}

// The two swinging attacks. 0x00B6DBD0 (:2523), 0x00B6DD20 (:2567)
static void walker_play_swing(walker* wp, int anim)
{
    vanim_play_action(wp->vp, index_alias(anim, (vanim_aliases)ANIM_ALIAS_ACTION_ATTACK), 0x20, 1.0f, 1.0f);
    if (1.5f < wp->move_speed) {
        v_anim* action = vanim_get_action(wp->vp, ANIM_ALIAS_ACTION_ATTACK);
        if (action != NULL && anim_get_anim_from_handle(action->anim_handle)->num_variants != 0) {
            vanim_set_variant(wp->vp, ANIM_ALIAS_ACTION_ATTACK, ANIM_VARIANT_FORWARD, true, true);
        }
    }
    if (wp->control_scheme == WCS_CHARACTER_STYLE) {
        wp->block_movement_anim = ANIM_ALIAS_ACTION_ATTACK;
        wp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
    }
}

// 0x00B6DBD0 (:2523)
bool walker_play_attack(walker* wp)
{
    if (!walker_can_attack(wp)) {
        return false;
    }
    if (Multiplayer) {
        wp->mp_data.attack_1_id++;
    }
    walker_play_swing(wp, 0x216);
    return true;
}

// 0x00B6DD20 (:2567)
bool walker_play_attack2(walker* wp)
{
    if (!walker_can_attack(wp)) {
        return false;
    }
    if (Multiplayer) {
        wp->mp_data.attack_3_id++;
    }
    walker_play_swing(wp, 0x218);
    return true;
}

// The two attacks with a standing and a moving animation. 0x00B6DE70 (:2612), 0x00B6DF90 (:2649)
static void walker_play_directional(walker* wp, int standing_anim, int moving_anim)
{
    if (wp->move_speed < 2.0f) {
        vanim_play_action(wp->vp, index_alias(standing_anim, (vanim_aliases)ANIM_ALIAS_ACTION_ATTACK), 0x20, 1.0f, 1.0f);
        wp->block_movement_anim = ANIM_ALIAS_ACTION_ATTACK;
        wp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
    }
    else {
        vanim_play_action(wp->vp, index_alias(moving_anim, (vanim_aliases)ANIM_ALIAS_ACTION_ATTACK), 0x20, 1.0f, 1.0f);
    }
}

// 0x00B6DE70 (:2612)
bool walker_play_attack3(walker* wp)
{
    if (!walker_can_attack(wp)) {
        return false;
    }
    if (Multiplayer) {
        wp->mp_data.attack_4_id++;
    }
    walker_play_directional(wp, 0x29E, 0x219);
    return true;
}

// 0x00B6DF90 (:2649)
bool walker_play_attack4(walker* wp)
{
    if (!walker_can_attack(wp)) {
        return false;
    }
    if (Multiplayer) {
        wp->mp_data.attack_2_id++;
    }
    walker_play_directional(wp, 0x29D, 0x217);
    return true;
}

// 0x00B6E0B0 (:2687)
void walker_play_VI_anim(walker* wp, int state, int exit_type)
{
    int state_index;
    int action;
    switch (state) {
    case 0:
    case 5:
        return;
    case 1:
        if (wp->model_type == WMT_SMALL) {
            state_index = ANIM_STATE_ENTERED;
            action = 0x352;
        }
        else if (wp->model_type == WMT_FLAME) {
            state_index = ANIM_STATE_DEFAULT;
            action = ANIM_ACTION_ENTER;
        }
        else {
            return;
        }
        break;
    case 2:
        if (wp->model_type != WMT_SMALL && wp->model_type != WMT_CIVILIAN) {
            return;
        }
        state_index = ANIM_STATE_DEFAULT;
        action = ANIM_ACTION_ENTER;
        break;
    case 3:
        if (wp->model_type != WMT_SMALL || exit_type == 9) { // 9: a dive exit (name inferred)
            return;
        }
        state_index = ANIM_STATE_ENTERED;
        action = ANIM_ACTION_EXIT_CIVILIAN;
        break;
    case 4:
        if (wp->model_type == WMT_SMALL) {
            if (exit_type != 9) {
                state_index = ANIM_STATE_PARKED;
                action = ANIM_ACTION_EXIT_COMBAT;
            }
            else {
                state_index = ANIM_STATE_ENTERED;
                action = 0x18A;
            }
        }
        else if (wp->model_type == WMT_FLAME) {
            state_index = ANIM_STATE_PARKED;
            action = ANIM_ACTION_EXIT_COMBAT;
        }
        else if (wp->model_type == WMT_CIVILIAN) {
            state_index = ANIM_STATE_PARKED;
            action = ANIM_ACTION_EXIT_CIVILIAN;
        }
        else {
            return;
        }
        break;
    default:
        return;
    }
    if (vanim_is_in_state(wp->vp, state_index)) {
        return;
    }
    wp->steering_vector.zero();
    wp->w_flags.in_ramming_mode = 0;
    if (vanim_is_in_state(wp->vp, state_index)) {
        return;
    }
    if (vanim_is_playing_action(wp->vp, ANIM_ALIAS_ACTION_TRANSITION)) {
        vanim_pause_and_ramp_out_action(wp->vp, ANIM_ALIAS_ACTION_TRANSITION, 5);
    }
    vanim_change_state(wp->vp, index_alias(state_index, ANIM_ALIAS_NONE), index_alias(action, ANIM_ALIAS_VEHICLE_TRANSITION), 0.3f, false);
    wp->block_movement_anim = ANIM_ALIAS_ACTION_TRANSITION;
    wp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
    wp->input_velocity.zero();
    if (state_index == ANIM_STATE_PARKED && wp->get_parent() != NO_HANDLE) {
        wp->remove_from_parent();
    }
}

// 0x00B6E2D0 (:2832)
bool walker_charge(walker* wp)
{
    if (wp->model_type == WMT_FLAME || vanim_is_playing_action(wp->vp, ANIM_ALIAS_ACTION_ATTACK)) {
        return false;
    }
    if (wp->control_scheme == WCS_CHARACTER_STYLE) {
        // Small walkers have a jump jet instead.
        if (vanim_is_playing_action(wp->vp, ANIM_ACTION_ENTER) || vanim_is_playing_action(wp->vp, ANIM_ALIAS_ACTION_LAND)) {
            return false;
        }
        if (-0.5f < wp->jump_jet_energy) {
            const float energy = wp->jump_jet_energy - Frametime;
            wp->jump_jet_energy = energy;
            if (0.0f < energy) {
                wp->w_flags.jump_jet_on = 1;
                if (3.0f < wp->jump_jet_speed && wp->actual_velocity.y < 0.5f && !vanim_is_playing_action(wp->vp, ANIM_ALIAS_ACTION_JUMP_LOOP)) {
                    wp->jump_jet_speed -= Frametime * 20.0f;
                }
                else {
                    wp->jump_jet_speed = wp->actual_velocity.y;
                }
                return true;
            }
            wp->jump_jet_energy = energy - Frametime * 3.0f;
        }
    }
    if (wp->w_flags.in_ramming_mode != 0) {
        return false;
    }
    if (Multiplayer) {
        wp->mp_data.charge_id++;
    }
    if (vanim_is_in_state(wp->vp, ANIM_STATE_CHARGE_READY) && wp->ram_max_energy_used < 0.0f) {
        wp->w_flags.in_ramming_mode = 1;
        wp->ram_momentum = 0.0f;
        wp->ram_max_energy_used = 0.0f;
    }
    return true;
}

// 0x00B86650 (:2884)
void walker_mount_vehicle(walker* wp, vehicle* vp)
{
    wp->mount_vector = vector(0.002f, 2.37585f, 10.1582f); // on the back of the carrier
    wp->w_flags.is_mounting = 1;
    wp->mount_vehicle = vp->get_handle();
    havok_set_ignore_property(havok_body_get_pointer(vp->havok_handle), wp->get_handle());
    // The carrier is let go if it was locked in place.
    if (vp->get_handle() != NO_HANDLE) {
        object* carrier_obj = World.object_from_handle(vp->get_handle());
        if (carrier_obj != NULL && carrier_obj->get_obj_type() == OT_VEHICLE) {
            vehicle* carrier = static_cast<vehicle*>(carrier_obj);
            if (carrier->m_flags.locked_in_place != 0) {
                carrier->m_flags.locked_in_place = 0;
                if (carrier->havok_handle != NO_HANDLE) {
                    havok_body_set_motion_type(carrier->havok_handle, 3); // dynamic
                    havok_body_force_activate(carrier->havok_handle);
                    carrier->m_flags.fixed_motion = 0;
                }
            }
        }
    }
    wp->m_flags.m_invulnerable = 1;
    vanim_pause_and_ramp_out_action(wp->vp, ANIM_ALIAS_ACTION_ATTACK, 5);
    wp->block_movement_anim = -1;
    havok_body_set_collision_type(wp->collision_model, COLLIDE_NO_COLLISION, true, true);
    vanim_play_action(wp->vp, index_alias(ANIM_ACTION_MOUNT, (vanim_aliases)-1), 0x4020, 1.0f, 1.0f);
    vanim_change_state(wp->vp, index_alias(ANIM_STATE_PARKED, (vanim_aliases)-1), index_alias(-1, (vanim_aliases)-1), 0.3f, false);
    const vehicle_interaction_info* vi_info = wp->info->vi_info;
    const int num_seats = vi_info != NULL ? vi_info->num_seats : 1;
    human* passenger = NULL;
    for (int seat = VSI_DRIVER; seat < num_seats && passenger == NULL; seat++) {
        passenger = vehicle_passenger_in_seat(wp, (vehicle_seat_index)seat);
    }
    // Original bug kept: nobody in it is not checked for.
    passenger->hflags.disallow_vehicle_exit = 1;
    vp->set_child(wp);
    struct walker_mount_event : public event_data
    {
        walker_mount_event() : event_data(EVENT_WALKER_MOUNT_VEHICLE) {}
        unsigned int handle;
    } mount_event;
    mount_event.handle = wp->get_handle();
    gem_post_event(mount_event);
}

// 0x00B6E480 (:2921)
walker_attack_types walker_get_attack_type(walker* wp)
{
    if (wp == NULL || !vanim_is_playing_action(wp->vp, ANIM_ALIAS_ACTION_ATTACK)) {
        return WALKER_ATTACK_NONE;
    }
    switch (vanim_get_anim_index(wp->vp, ANIM_ALIAS_ACTION_ATTACK)) {
    case 0x216:
        return WALKER_ATTACK_THROW;
    case 0x217:
    case 0x219:
        return WALKER_ATTACK_SWEEP;
    case 0x218:
        return WALKER_ATTACK_SMASH;
    default:
        return WALKER_ATTACK_NONE;
    }
}

// 0x00B6E500 (:2954)
bool walker_should_ignore_collision_effect(const walker* const wp)
{
    if (vanim_is_playing_action(wp->vp, ANIM_ALIAS_ACTION_ATTACK)) {
        return false;
    }
    return vanim_is_in_state(wp->vp, ANIM_STATE_DEFAULT);
}

// The axis and angle that turn one unit vector to another. 0x00B6E540 (:2968), part.
static void wik_rotation_between(const vector& from, const vector& to, vector& axis, float& angle)
{
    const float cx = from.y * to.z - from.z * to.y;
    const float cy = from.z * to.x - to.z * from.x;
    const float cz = to.y * from.x - from.y * to.x;
    float mag = sqrtf(cy * cy + cx * cx + cz * cz);
    if (1e-05f <= mag) {
        const float scale = 1.0f / mag;
        axis = vector(scale * cx, cy * scale, cz * scale);
        if (0.0f <= mag - 1.0f) {
            mag = 1.0f;
        }
        angle = (float)asin(mag);
    }
    else {
        axis = UP_VECTOR;
        angle = 0.0f;
    }
}

// 0x00B6E540 (:2968)
void wik_joint::do_ik(walker* wp, vector& target_pos, matrix43* tmp_matrices)
{
    const int aim = wp->vp->aim_handle;
    vector anchor_pos;
    vector joint_pos;
    vector ik_pos;
    matrix anchor_orient;
    matrix joint_orient;
    matrix ik_orient;
    animlib_get_tag_local_coords(aim, anchor_tag, anchor_pos, &anchor_orient);
    animlib_get_tag_local_coords(aim, joint_tag, joint_pos, &joint_orient);
    if (wp->control_scheme == WCS_FACING_CAMERA) {
        joint_pos.x += 0.0f < joint_pos.x ? 0.2f : 0.1f; // 0x016EAC20, 0x016EAC1C: knees apart
    }
    animlib_get_tag_local_coords(aim, ik_tag, ik_pos, &ik_orient);
    int num_bones;
    int bones[64];
    animlib_get_bone_tree_list(aim, anchor_tag, 64, num_bones, bones);
    animlib_invalidate_cache_bone_list(aim, num_bones, bones, true, true);

    // A leg nearly straight has no knee direction to solve for: pull the target in until it bends.
    vector thigh_dir;
    vector calf_dir;
    int attempt = 1;
    do {
        vector solved_joint;
        vector solved_end;
        ik_2link_solve(anchor_pos, joint_pos, ik_pos, target_pos, 0.0f, solved_joint, solved_end);
        thigh_dir = vector(solved_joint.x - anchor_pos.x, solved_joint.y - anchor_pos.y, solved_joint.z - anchor_pos.z).copy_normalize();
        calf_dir = vector(solved_end.x - solved_joint.x, solved_end.y - solved_joint.y, solved_end.z - solved_joint.z).copy_normalize();
        if (thigh_dir.y * calf_dir.y + calf_dir.x * thigh_dir.x + thigh_dir.z * calf_dir.z < 0.96f) { // 0x016EAC18
            break;
        }
        const float t = (float)attempt * 0.1f;
        const float inverse_t = 1.0f - t;
        target_pos = vector(ik_pos.x * t + target_pos.x * inverse_t, ik_pos.y * t + target_pos.y * inverse_t, ik_pos.z * t + target_pos.z * inverse_t);
        attempt++;
    } while (attempt < 6);

    const vector calf_local = vector(ik_pos.x - joint_pos.x, ik_pos.y - joint_pos.y, ik_pos.z - joint_pos.z).copy_normalize().copy_rotate(joint_orient);
    const vector thigh_current = vector(joint_pos.x - anchor_pos.x, joint_pos.y - anchor_pos.y, joint_pos.z - anchor_pos.z).copy_normalize();

    vector axis;
    float angle;
    wik_rotation_between(thigh_current, thigh_dir, axis, angle);
    matrix rotation;
    rotation.set(axis, angle);
    matrix new_anchor = anchor_orient * rotation;
    matrix new_joint_base = (joint_orient * anchor_orient.copy_transpose()) * new_anchor;
    const vector calf_current = calf_local.copy_unrotate(new_joint_base);
    wik_rotation_between(calf_current, calf_dir, axis, angle);
    matrix new_joint = new_joint_base * matrix(axis, angle);
    if (0.0f < ik_strength) {
        animlib_register_bone_modifier(aim, BONE_MODIFIER_FORCE, anchor_tag, location * 2 + 9, new_anchor, &anchor_orient, ik_strength);
        animlib_register_bone_modifier(aim, BONE_MODIFIER_ROTATE, joint_tag, location * 2 + 10, new_joint, &new_joint_base, ik_strength);
    }
}

// 0x00B996F0 (:3109)
bool wik_joint::get_ik_pos(walker* wp, vector& ik_pos)
{
    matrix43 bone;
    animlib_get_bone_transform(wp->vp->aim_handle, ik_tag, bone);
    ik_pos = bone.m_translation;
    const v_anim* desired_state = wp->vp->desired_state;
    // Original bug kept: hit_point_valid is never set true, so the cached branch below never runs.
    if (wp->move_speed < 0.01f && hit_point_valid && (desired_state == NULL || desired_state->blend == 1.0f)
        && fabsf(bone.m_translation.x - cached_hit_point.x) < 0.01f && fabsf(bone.m_translation.z - cached_hit_point.z) < 0.01f) {
        ik_pos = cached_hit_point;
    }
    else {
        const matrix& orient = wp->orient;
        correction_amount = bone.m_translation.y;
        vector start(bone.m_translation);
        start.unrotate(orient);
        start.x = wp->pos.x + start.x;
        start.y = wp->pos.y + start.y;
        start.z = wp->pos.z + start.z;
        // 1: 0x016EB258, 0.5: 0x016EB25C: less reach for a foot far forward or back
        float reach = 1.0f - fabsf(ik_pos.z) * 0.5f;
        if (0.0f <= 0.0f - reach) {
            reach = 0.0f;
        }
        raycast_input input;
        input.start = start;
        input.end = vector(start.x, start.y - (correction_amount + reach), start.z);
        input.filter_params = NULL;
        input.filter_callback = NULL;
        input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
        input.radius = 0.0f;
        hkpRigidBody* capsule = havok_body_get_pointer(wp->collision_model);
        input.m_filter_info = capsule == NULL ? COLLIDE_WORLD : capsule->getCollisionFilterInfo();
        input.m_flags = 0x12;
        raycast_output output;
        const bool hit = havok_raycast(input, output);
        const vector& end_point = hit ? output.m_hits[0].hit_pos : input.end;
        const float bone_y = ik_pos.y;
        ik_pos.x = (end_point.x + 0.0f) - wp->pos.x;
        ik_pos.y = (end_point.y + bone_y) - wp->pos.y;
        ik_pos.z = (end_point.z + 0.0f) - wp->pos.z;
        correction_amount = correction_amount - ik_pos.y;
        ik_pos.y = ik_pos.y - wp->y_offset;
        ik_pos.rotate(orient);
        cached_hit_point = ik_pos;
        if (hit && wp->control_scheme == WCS_FACING_CAMERA) {
            // The foot lies on the slope. 0.62831855: 36 degrees
            vector normal(output.m_hits[0].hit_normal);
            normal.rotate(orient);
            vector side(normal.x, normal.y, 0.0f);
            side.normalize_safe();
            float bank = 0.0f;
            if (side.x != 0.0f) {
                bank = 1.5707964f - (float)asin(side.y);
                if (0.0f < side.x) {
                    bank = -bank;
                }
            }
            vector front(0.0f, normal.y, normal.z);
            front.normalize_safe();
            float pitch = 0.0f;
            if (front.z != 0.0f) {
                pitch = 1.5707964f - (float)asin(front.y);
                if (front.z < 0.0f) {
                    pitch = -pitch;
                }
            }
            pitch = walker_clamp(pitch, -0.62831855f, 0.62831855f);
            bank = walker_clamp(bank, -0.62831855f, 0.62831855f);
            cur_foot_p = walker_ramp(cur_foot_p, pitch, Frametime * 2.0f);
            cur_foot_b = walker_ramp(cur_foot_b, bank, Frametime * 2.0f);
        }
    }
    if (ik_strength == 0.0f) {
        cur_foot_p = 0.0f;
        cur_foot_b = 0.0f;
    }
    matrix foot_rotation;
    foot_rotation.set(cur_foot_p * ik_strength, cur_foot_b * ik_strength, 0.0f);
    const bool right = location != WALKER_IK_JOINT_LEFT_LEG;
    animlib_register_bone_modifier(wp->vp->aim_handle, BONE_MODIFIER_POST_ROTATE, right ? 0x17 : 0x15, right ? 1 : 0, foot_rotation, NULL, 1.0f); // the foot bones
    return true;
}

// 0x00B99F40 (:3228)
bool wik_joint::process(walker* wp, vector& target_pos)
{
    bool enable = false;
    if (wp->w_flags.is_mounting == 0 && wp->get_parent() == NO_HANDLE) {
        bool planted = true;
        if (!vanim_is_playing_action(wp->vp, ANIM_STATE_DEFAULT)) {
            // Is this foot on the ground in the animation?
            v_animate* vp = wp->vp;
            v_anim* states[2];
            states[1] = vp->secondary_state;
            if (states[1] == NULL || states[1]->blend < vp->desired_state->blend) {
                states[0] = vp->desired_state;
            }
            else {
                states[0] = states[1];
                states[1] = vp->desired_state;
            }
            float time = -1.0f;
            float left_down = -1.0f;
            float right_down = -1.0f;
            float left_up = -1.0f;
            float right_up = -1.0f;
            bool other_state_has_no_steps = false;
            for (int i = 0; i < 2; i++) {
                v_anim* state = states[i];
                if (state == NULL) {
                    continue;
                }
                const int frame = vanim_get_anim_frame(vp, state->instance);
                if (left_down == -1.0f && -1 < frame) {
                    left_down = vanim_get_trigger_time(vp, Trigger_left_foot, state->instance, 0);
                    right_down = vanim_get_trigger_time(vp, Trigger_right_foot, state->instance, 0);
                    left_up = vanim_get_trigger_time(vp, Trigger_left_foot_release, state->instance, 0);
                    right_up = vanim_get_trigger_time(vp, Trigger_right_foot_release, state->instance, 0);
                    time = (float)frame / 30.0f; // 30 frames per second
                }
                else if (vanim_get_trigger_time(vp, Trigger_left_foot, state->instance, 0) < 0.0f && 0.5f < state->blend) {
                    other_state_has_no_steps = true;
                }
            }
            bool left_planted = true;
            bool right_planted = true;
            if (0.0f <= left_down || 0.0f <= right_down) {
                // 0.3: seconds before the trigger the foot is treated as down
                left_down -= 0.3f;
                right_down -= 0.3f;
                if (left_down < 0.0f) {
                    left_down = vanim_get_anim_length(vp, states[0]->instance) + left_down;
                }
                if (right_down < 0.0f) {
                    right_down = vanim_get_anim_length(vp, states[0]->instance) + right_down;
                }
                if (other_state_has_no_steps) {
                    left_planted = (right_up <= time && time < left_up) || ((right_up <= time || time < left_up) && left_up < right_up);
                    if (time < left_up || right_up <= time) {
                        right_down = left_up;
                        if (time < left_up) {
                            right_planted = time < right_up && right_up < right_down;
                        }
                        else {
                            right_planted = right_up < right_down;
                        }
                    }
                    else {
                        right_planted = true;
                    }
                }
                else {
                    left_planted = (left_down <= time && time < left_up) || ((left_down <= time || time < left_up) && left_up < left_down);
                    if (right_down <= time && time < right_up) {
                        right_planted = true;
                    }
                    else if (right_down <= time) {
                        right_planted = right_up < right_down;
                    }
                    else {
                        right_planted = time < right_up && right_up < right_down;
                    }
                }
            }
            planted = (left_planted && location == WALKER_IK_JOINT_LEFT_LEG) || (right_planted && location == WALKER_IK_JOINT_RIGHT_LEG);
        }
        // Falling or jetting: the legs hang.
        enable = planted && -2.0f <= wp->actual_velocity.y && wp->w_flags.jump_jet_on == 0;
    }
    if (!enable) {
        const float offset = fabsf(wp->y_offset + correction_amount);
        float strength;
        if (0.0f <= 0.0f - offset || offset <= 0.0f) {
            strength = ik_strength - Frametime * 2.0f;
        }
        else {
            strength = ik_strength - (2.0f / offset) * Frametime;
        }
        ik_strength = walker_clamp(strength, 0.0f, 1.0f);
        if (ik_strength <= 0.0f) {
            correction_amount = 0.0f;
            hit_point_valid = false;
            return false;
        }
    }
    ik_rate_of_change = 4.0f;
    if (!get_ik_pos(wp, target_pos)) {
        return false;
    }
    if (enable) {
        float offset = fabsf(wp->y_offset + correction_amount);
        if (0.0f <= 0.0f - offset) {
            offset = 0.0f;
        }
        ik_strength = walker_clamp((2.0f / offset) * Frametime + ik_strength, 0.0f, 1.0f);
    }
    return true;
}

// 0x00B6EFB0 (:3352): the shapes of the feet, by model
bool walker_idx_is_foot(walker* wp, int idx)
{
    if (wp->model_type == WMT_CIVILIAN) {
        return (0x21 < idx && idx < 0x27) || (unsigned int)(idx - 0x30) < 5;
    }
    if (wp->model_type == WMT_FLAME) {
        return idx == 0xE || idx == 0x10 || idx == 10 || idx == 0xC || idx == 0x18 || idx == 0x1D || idx == 0x1B || idx == 0x15;
    }
    if (wp->model_type == WMT_SMALL) {
        return (0x1C < idx && idx < 0x21) || (unsigned int)(idx - 0x24) < 4;
    }
    return false;
}
