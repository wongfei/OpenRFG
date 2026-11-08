// Original: volition/rfg/code/objects/turret_internal.cpp
//
// Turret physics (header comment in turret_internal.h). A turret has three components, each a static mesh with a Havok
// body or a vehicle component: the base (swivel), the middle (body, turns in heading) and the top (head, turns in pitch,
// carries the weapon; an item turret without a top item uses the weapon's mesh for its shape only). Hinges with
// position motors (NPC operators, script aiming) or velocity motors (a player) join middle to base and top to middle.
// Compiled in rfg_objects_unity.cpp, after object_item.cpp (create_physics_object is its file static).
//
// The tag indices of the turret_physics_* functions: 0 base, 1 middle, 2 top (primary weapon), 3 secondary weapon mount,
// 4 and 5 the left and right hand grips on the weapon.

#include "volition/rfg/code/objects/turret_internal.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/limitedhinge/hkplimitedhingeconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/motor/position/hkppositionconstraintmotor.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/motor/velocity/hkpvelocityconstraintmotor.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpmodifierconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldconstraintutil.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_instance.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/geom/mesh_tags.h"
#include "volition/rfg/code/geom/static_mesh.h"
#include "volition/rfg/code/havok/constraint_template.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_cm_manager.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/object_constraint.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/objects/turret_info.h"
#include "volition/rfg/code/pathfinding/nav_cell_detour.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_component.h"
#include "volition/rfg/code/vehicle/vehicle_component_data.h"
#include "volition/rfg/code/vehicle/vehicle_subinfo.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/trace.hpp"

#include <math.h>
#include <stdio.h>

// Magic numbers.
static const int MAX_TURRETS = 66; // the turret pool's size, so a physics object is always free
static const float HORSEPOWER_TO_WATTS = 745.7f;
static const float OFF_LIMIT_ANGLE = 1e+14f; // hinge limits of a turret that turns all the way around
static const float MIN_VEHICLE_COMPONENT_MASS = 50.0f;
static const float MIN_TURRET_TENSOR_COMPONENT = 5.0f;
static const float MAX_TURRET_TENSOR_COMPONENT = 20000.0f; // 0x01311C30
static const float DEFAULT_ITEM_COMPONENT_MASS = 10.0f;
static const float MIN_VEHICLE_ITEM_COMPONENT_MASS = 5.0f;
static const float MIN_FLOOR_RAY_LENGTH = 10.0f;
static const float WEAPON_DEBRIS_MASS = 10.0f;
// The base tag of a turret on a vehicle may stray this far (squared) or this much (dot of the up vectors) before
// turret_physics_fix_pos_orient gives up on the correction.
static const float TAG_DIST_SQ_OK = 0.040000003f;
static const float TAG_DIST_SQ_MAX = 1.0f;
static const float TAG_DOT_MIN = 0.8660254f; // cos(30 degrees)

// ---- file-static globals
static const char* const Turret_attach_tag_names[3] = { "turret_swivel", "turret_body", "turret_head" }; // 0x01311BF0
static const char* const Secondary_weapon_attach_tag_names[3] = { "turret_swivel_2", "turret_body_2", "mg_effect" }; // 0x01311BFC
static const char* const Alt_fire_effect_tag_names[6] = { "_none_", "_none_", "altturretfx", "altturretfx", "_none_", "_none_" }; // 0x01311C08
static float Vel_motor_ramp_out = 0.5f; // 0x01694788: how fast a velocity motor's target decays
static const float MAX_OVERTURNED_VEHICLE_TURRET_DOT = (float)cos((double)0.087265f); // 0x0319781C: cos(5 degrees)
static object_pool<turret_physics, MAX_TURRETS, 1> Turret_physics_pool(&turret_physics::init); // 0x03533EF0

// ---- internal functions
static int cycle_vehicle_component_attach_tag_next(unsigned int& last_attach, const char* tag_name, const vehicle* vp, int c_idx); // 0x00A20E10 (:934)

// 0x00A209E0 (:159): pos and orient arrive in EDX and ESI, the component in EAX.
static void change_to_component_space(vector& pos, matrix& orient, const vehicle_component* vcp)
{
    vec_change_to_local_coords(pos, vcp->get_chassis_space_position(), vcp->get_chassis_space_rotation());
    // Original bug kept: rotates by itself, not by the component's rotation (the asm pushes ESI and sets ECX to ESI).
    orient.rotate(orient);
}

// 0x00A43CC0 (:166): where the top hangs on the middle and the middle on the base, in the parent's space.
static bool get_constraint_attach_points(vector& top_mid_pos, vector& mid_base_pos, matrix& top_mid_ort, matrix& mid_base_ort, const turret_physics* physics, vehicle* vp)
{
    vehicle::resource_dependent_data* vdata = vp != nullptr ? (vp->rdd.is_loaded() ? vp->rdd.get_data() : nullptr) : nullptr;
    top_mid_pos = ZERO_VECTOR;
    mid_base_pos = ZERO_VECTOR;
    top_mid_ort = IDENTITY_MATRIX;
    mid_base_ort = IDENTITY_MATRIX;
    const turret_component_physics_info& middle = physics->components[TURRET_COMPONENT_MIDDLE];
    const turret_component_physics_info& base = physics->components[TURRET_COMPONENT_BASE];
    if (middle.veh_component_idx != -1)
    {
        vp->get_tag_local_coords(middle.attach_tag_idx, top_mid_pos, &top_mid_ort);
        change_to_component_space(top_mid_pos, top_mid_ort, &vdata->m_components[middle.veh_component_idx]);
    }
    else
    {
        matrix43 transform;
        middle.smesh->m_tags.get_tag_transform(transform, middle.attach_tag_idx);
        transform.get_translation(top_mid_pos);
        transform.get_orientation(top_mid_ort);
    }
    if (vp == nullptr)
    {
        matrix43 transform;
        base.smesh->m_tags.get_tag_transform(transform, base.attach_tag_idx);
        transform.get_translation(mid_base_pos);
        transform.get_orientation(mid_base_ort);
        return true;
    }
    vp->get_tag_local_coords(base.attach_tag_idx, mid_base_pos, &mid_base_ort);
    if (base.veh_component_idx != -1)
    {
        change_to_component_space(mid_base_pos, mid_base_ort, &vdata->m_components[base.veh_component_idx]);
        return true;
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_WALKER)
    {
        // Attached to the root of a walker: relative to its tag 1.
        vector root_pos;
        vp->get_tag_local_coords(1, root_pos, nullptr);
        mid_base_pos.x -= root_pos.x;
        mid_base_pos.y -= root_pos.y;
        mid_base_pos.z -= root_pos.z;
    }
    return true;
}

// 0x00A43F20 (:217): the motor reference arrives in EAX.
static bool build_pos_motor(hkpPositionConstraintMotor*& pos_motor, float power)
{
    pos_motor = new hkpPositionConstraintMotor(0.0f);
    if (pos_motor == nullptr)
    {
        return false;
    }
    pos_motor->m_tau = 1.0f;
    pos_motor->m_damping = 0.75f;
    pos_motor->m_proportionalRecoveryVelocity = 0.1f;
    pos_motor->m_constantRecoveryVelocity = 0.0f;
    pos_motor->m_minForce = -(power * HORSEPOWER_TO_WATTS);
    pos_motor->m_maxForce = power * HORSEPOWER_TO_WATTS;
    return true;
}

// 0x00A43FE0 (:229): the motor's constructor is inlined with tau 0.8, which the parameter replaces.
static bool build_vel_motor(hkpVelocityConstraintMotor*& vel_motor, float power, float tau)
{
    vel_motor = new hkpVelocityConstraintMotor();
    if (vel_motor == nullptr)
    {
        return false;
    }
    vel_motor->m_type = hkpConstraintMotor::TYPE_VELOCITY;
    vel_motor->m_velocityTarget = 0.0f;
    vel_motor->m_maxForce = power * HORSEPOWER_TO_WATTS;
    vel_motor->m_minForce = -(power * HORSEPOWER_TO_WATTS);
    vel_motor->m_useVelocityTargetFromConstraintTargets = false;
    vel_motor->m_tau = tau;
    return true;
}

// 0x00A44070 (:250): local_orient arrives in ECX. A vehicle's middle hinge on a sub type 5 vehicle (flyer) makes the
// turret weightless to the chassis (mass changer: the factors of body A are 0, of body B are 1).
static bool constrain_turret_components(turret_physics* physics, turret_components comp, const vector& local_pos, const matrix& local_orient, float min_ang, float max_ang, float tau, bool enable_limits, vehicle* vp)
{
    vector axis_a;
    vector axis_b;
    if (comp == TURRET_COMPONENT_MIDDLE)
    {
        axis_a = local_orient.uvec;
        axis_b = UP_VECTOR;
    }
    else if (comp == TURRET_COMPONENT_TOP)
    {
        axis_a = local_orient.rvec;
        axis_b = RIGHT_VECTOR;
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function"); // Original: the formatted file and line
        return false;
    }
    hkVector4f pivot_a;
    hkVector4f pivot_b;
    hkVector4f hk_axis_a;
    hkVector4f hk_axis_b;
    hkVector4f perp_a;
    hkVector4f perp_b;
    game_to_havok_vector(pivot_a, local_pos);
    game_to_havok_vector(pivot_b, ZERO_VECTOR);
    game_to_havok_vector(hk_axis_a, axis_a);
    game_to_havok_vector(hk_axis_b, axis_b);
    game_to_havok_vector(perp_a, local_orient.fvec);
    game_to_havok_vector(perp_b, FORWARD_VECTOR);
    hkpLimitedHingeConstraintData* data = new hkpLimitedHingeConstraintData();
    if (data == nullptr)
    {
        return false;
    }
    data->setInBodySpace(pivot_a, pivot_b, hk_axis_a, hk_axis_b, perp_a, perp_b);
    if (enable_limits)
    {
        data->m_atoms.m_angLimit.m_minAngle = min_ang;
        data->m_atoms.m_angLimit.m_maxAngle = max_ang;
    }
    else
    {
        data->m_atoms.m_angLimit.m_minAngle = -OFF_LIMIT_ANGLE;
        data->m_atoms.m_angLimit.m_maxAngle = OFF_LIMIT_ANGLE;
    }
    data->m_atoms.m_angLimit.m_angularLimitsTauFactor = tau;

    hkpRigidBody* comp_rb = havok_body_get_pointer(physics->components[comp].havok_handle);
    hkpRigidBody* parent_rb = havok_body_get_pointer(physics->components[comp - 1].havok_handle);
    vector parent_pos;
    matrix parent_orient;
    havok_body_get_pos_orient(parent_rb, parent_pos, parent_orient);
    vector world_pos = local_pos;
    matrix world_orient = local_orient;
    vec_change_to_global_coords(world_pos, parent_pos, parent_orient);
    world_orient.unrotate(parent_orient);
    havok_body_set_pos_orient(comp_rb, world_pos, world_orient);
    physics->components[comp].hconstraint = havok_constraint_create(parent_rb, comp_rb, data, -1.0f, hkpConstraintInstance::PRIORITY_PSI);
    if (vp != nullptr && comp == TURRET_COMPONENT_MIDDLE && vp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
    {
        hkpMassChangerModifierConstraintAtom* modifier = new hkpMassChangerModifierConstraintAtom();
        // 0x01311C90, 0x01311CA0
        modifier->m_factorA.set(0.0f, 0.0f, 0.0f, 0.0f);
        modifier->m_factorB.set(1.0f, 1.0f, 1.0f, 1.0f);
        hkpConstraintInstance* ci = physics->components[TURRET_COMPONENT_MIDDLE].hconstraint;
        hkpWorldConstraintUtil::addModifier(ci, *ci->m_owner, modifier);
        ci->m_internal->m_callbackRequest |= 2;
    }
    return true;
}

// 0x00A20A00 (:350): the floor ray skips the turret itself, items and vehicles.
static bool turret_constraint_callback(object* hit_objp, void* filter_params)
{
    return hit_objp != filter_params && hit_objp->get_obj_type() != OT_ITEM && hit_objp->get_obj_type() != OT_VEHICLE;
}

// 0x00A96D60 (:367): a free-standing turret is tied to what it stands on. The ray reaches at least 10 m below the
// mesh's height (Original: the height is the box's top minus its bottom, written out as center +- half extent).
bool turret_physics_constrain_to_floor(turret_physics* physics, turret* tp)
{
    const static_mesh* smesh = physics->components[TURRET_COMPONENT_BASE].smesh;
    if (smesh == nullptr)
    {
        return false;
    }
    if (tp->child_constraint_ptr != nullptr)
    {
        return true;
    }
    raycast_input in;
    in.start.x = tp->pos.x + UP_VECTOR.x * 0.5f;
    in.start.y = tp->pos.y + UP_VECTOR.y * 0.5f;
    in.start.z = tp->pos.z + UP_VECTOR.z * 0.5f;
    in.radius = 0.0f;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 0;
    in.m_filter_info = COLLIDE_RAY_WORLD_ONLY;
    in.filter_callback = turret_constraint_callback;
    in.filter_params = tp;
    const rl_mesh* mesh = smesh->m_shared_header.m_mesh.to_pointer();
    const rl_aabb mesh_box = mesh->get_local_aabb();
    float length = mesh_box.max.y - mesh_box.min.y;
    if (0.0f <= MIN_FLOOR_RAY_LENGTH - length)
    {
        length = MIN_FLOOR_RAY_LENGTH;
    }
    in.end.x = in.start.x;
    in.end.y = in.start.y - length;
    in.end.z = in.start.z;
    raycast_output out;
    if (!havok_raycast(in, out))
    {
        return true;
    }
    const raycast_output::hit_info& hit = out.m_hits[0];
    // Original: hit_objp is read below without the null check this reads as.
    object* hit_objp = World.object_from_handle(hit.hit_object_handle);
    int host_attach_index = -1;
    if (hit_objp != nullptr && hit_objp->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        host_attach_index = (int)(short)hit.hit_havok_shape_key;
    }
    constraint_template ct; // the constructor resets it
    ct.point_data.x_limit_min = 0.0f;
    ct.point_data.x_limit_max = 0.0f;
    ct.type = 2; // point constraint
    ct.point_data.y_limit_min = -1.0f;
    obj_constraint_props props(&ct, hit_objp->get_handle(), tp->get_handle(), hit.hit_pos, IDENTITY_MATRIX, host_attach_index, -1, -1, -1);
    constraint* con = World.create_object<constraint>(props, 0xFFFFFFFF);
    if (con == nullptr)
    {
        return false;
    }
    physics->base_constraint_handle = con->get_handle();
    tp->first_parent = hit_objp->get_handle();
    return true;
}

// 0x00A20A20 (:434): the physics arrives in EAX, tp is unused. The mass the debris pieces share.
static float get_total_item_mass(const turret_physics* physics, const turret* tp)
{
    float total = 0.0f;
    for (int i = 0; i < NUM_TURRET_COMPONENTS; i++)
    {
        if (!physics->components[i].is_veh_component && physics->components[i].havok_handle != 0xFFFFFFFF)
        {
            total += havok_body_get_mass(physics->components[i].havok_handle);
        }
    }
    return total;
}

// 0x00A20A70 (:463): the rigid body arrives in ESI. Properties 3 and 4: physical and effect material.
static void set_material_properties(hkpRigidBody* rb)
{
    if (havok_has_property(rb, 4))
    {
        havok_remove_property(rb, 4);
    }
    havok_set_property(rb, 4, (void*)(unsigned int)Default_turret_effect_material_index);
    if (havok_has_property(rb, 3))
    {
        havok_remove_property(rb, 3);
    }
    havok_set_property(rb, 3, (void*)(unsigned int)Default_turret_physical_material_index);
}

// 0x00A20AD0 (:482)
static void set_body_no_collide(hkpRigidBody* rb, unsigned int h)
{
    if (havok_has_ignore_property(rb))
    {
        havok_remove_ignore_property(rb);
        havok_entity_update_collision_filter(rb, true, false);
    }
    if (h != 0xFFFFFFFF)
    {
        havok_set_ignore_property(rb, h);
    }
}

// 0x00A20B00 (:493)
static void set_body_no_collide(unsigned int hbh, unsigned int h)
{
    if (hbh != 0xFFFFFFFF)
    {
        set_body_no_collide(havok_body_get_pointer(hbh), h);
    }
}

// 0x00A443C0 (:508): physics arrives in EDI. Grip tags on the weapon; false until the weapon is streamed.
static bool get_grip_points(turret_physics* physics, const turret* tp)
{
    weapon* wp = tp->get_weapon_inst();
    if (wp == nullptr || !wp->obj_flags.streamed)
    {
        return false;
    }
    physics->right_grip_tag_index = wp->get_tag_index_from_name("weapon_handle");
    physics->left_grip_tag_index = wp->get_tag_index_from_name("secondary_handle");
    // Original: formats "Turret %s is marked as having operator animations but is missing both IK grip points!" when
    // both are -1.
    return true;
}

// 0x00A44470 (:566): physics, tp and vp arrive in EAX, stack and stack in the original's parameter order.
static bool get_secondary_weapon_mount_point(turret_physics* physics, const turret* tp, const vehicle* vp)
{
    if (tp->t_info->flags.secondary_uses_primary_tags)
    {
        physics->secondary_weapon_attach_component = TURRET_COMPONENT_TOP;
        const int c_idx = physics->components[TURRET_COMPONENT_TOP].veh_component_idx;
        if (c_idx != -1)
        {
            int tag = vp->get_component_tag_index_from_name(c_idx, "turret_head");
            physics->secondary_weapon_attach_tag_idx = tag;
            if (tag == -1)
            {
                physics->secondary_weapon_attach_tag_idx = cycle_vehicle_component_attach_tag_next(physics->secondary_weapon_attach_offset, "turret_head", vp, c_idx);
            }
        }
        return true;
    }
    for (int c = TURRET_COMPONENT_TOP; c >= 0; c--)
    {
        const int c_idx = physics->components[c].veh_component_idx;
        const char* name = Secondary_weapon_attach_tag_names[c];
        if (c_idx != -1)
        {
            int tag = vp->get_component_tag_index_from_name(c_idx, name);
            physics->secondary_weapon_attach_tag_idx = tag;
            if (tag == -1)
            {
                physics->secondary_weapon_attach_tag_idx = cycle_vehicle_component_attach_tag_next(physics->secondary_weapon_attach_offset, name, vp, c_idx);
            }
        }
        if (physics->secondary_weapon_attach_tag_idx != -1)
        {
            physics->secondary_weapon_attach_component = (turret_components)c;
            return true;
        }
    }
    // Original: formats "Turret %s is missing mount tag for its secondary weapon!".
    physics->secondary_weapon_attach_component = TURRET_COMPONENT_TOP;
    physics->secondary_weapon_attach_tag_idx = physics->components[TURRET_COMPONENT_TOP].attach_tag_idx;
    return true;
}

// 0x00A44640 (:613): physics arrives in ESI, tp is unused.
static void detach_all(turret_physics* physics, const turret* tp)
{
    if (physics->components[TURRET_COMPONENT_MIDDLE].pos_motor != nullptr)
    {
        physics->components[TURRET_COMPONENT_MIDDLE].pos_motor->removeReference();
    }
    if (physics->components[TURRET_COMPONENT_TOP].pos_motor != nullptr)
    {
        physics->components[TURRET_COMPONENT_TOP].pos_motor->removeReference();
    }
    if (physics->components[TURRET_COMPONENT_MIDDLE].vel_motor != nullptr)
    {
        physics->components[TURRET_COMPONENT_MIDDLE].vel_motor->removeReference();
    }
    if (physics->components[TURRET_COMPONENT_TOP].vel_motor != nullptr)
    {
        physics->components[TURRET_COMPONENT_TOP].vel_motor->removeReference();
    }
    if (physics->components[TURRET_COMPONENT_MIDDLE].hconstraint != nullptr)
    {
        havok_constraint_remove_motor(physics->components[TURRET_COMPONENT_MIDDLE].hconstraint);
        havok_constraint_release(physics->components[TURRET_COMPONENT_MIDDLE].hconstraint);
    }
    if (physics->components[TURRET_COMPONENT_TOP].hconstraint != nullptr)
    {
        havok_constraint_remove_motor(physics->components[TURRET_COMPONENT_TOP].hconstraint);
        havok_constraint_release(physics->components[TURRET_COMPONENT_TOP].hconstraint);
    }
    if (physics->base_constraint_handle != 0xFFFFFFFF)
    {
        World.destroy_object(physics->base_constraint_handle, false);
        physics->base_constraint_handle = 0xFFFFFFFF;
    }
}

// 0x00A20B40 (:654)
static void destroy_components(const turret_physics* physics)
{
    for (int i = 0; i < NUM_TURRET_COMPONENTS; i++)
    {
        const turret_component_physics_info& component = physics->components[i];
        if (component.smesh_instance != nullptr)
        {
            component.smesh_instance->release();
        }
        if (!component.is_veh_component && component.havok_handle != 0xFFFFFFFF)
        {
            havok_body_free(component.havok_handle, false, true, false);
        }
    }
}

// 0x00A731C0 (:680): physics arrives in EDI. A flying piece of a destroyed turret: its share of the kill impulse (a
// quarter by mass) and a random push around up.
static void create_debris_object(const turret_physics* physics, const item_info* i_info, const vector& pos, const matrix& orient, const vector& linear_vel, const vector& angular_vel, float mass, float total_mass)
{
    obj_item_flags flags;
    flags.respawn = false;
    flags.no_physics = false;
    flags.simulate = true;
    flags.props_created = false;
    flags.item_held = false;
    flags.no_save = true;
    flags.preplaced = false;
    flags.update_attach_point = false;
    flags.dropped_by_player = false;
    flags.touched_last_frame = false;
    flags.character_prop = false;
    flags.attached_cloth_sim = false;
    flags.doing_errode = false;
    flags.cast_shadows = true;
    flags.cast_drop_shadows = true;
    flags.xray_visible = false;
    flags.dont_dampen_impulse = true;
    flags.shield_initialized = false;
    obj_item_props props(i_info, pos, orient, flags, COLLIDE_RFG_NORMAL, mass, INVALID_SALVAGE_MATERIAL);
    item* ip = World.create_object<item>(props, 0xFFFFFFFF);
    if (ip == nullptr)
    {
        return;
    }
    if (!ip->obj_flags.streamed)
    {
        World.destroy_object(ip, false);
        return;
    }
    if (ip->havok_handle == 0xFFFFFFFF)
    {
        create_physics_object(ip);
    }
    if (ip->havok_handle == 0xFFFFFFFF)
    {
        return;
    }
    hkpRigidBody* rb = havok_body_get_pointer(ip->havok_handle);
    havok_body_set_linear_velocity(rb, linear_vel);
    havok_body_set_angular_velocity(rb, angular_vel);
    set_material_properties(rb);
    if (havok_has_ignore_property(rb))
    {
        havok_remove_ignore_property(rb);
    }
    const float share = mass / total_mass;
    vector impulse;
    impulse.x = physics->kill_linear_impulse.x * share * 0.25f;
    impulse.y = physics->kill_linear_impulse.y * share * 0.25f;
    impulse.z = physics->kill_linear_impulse.z * share * 0.25f;
    vector push;
    push.rand_around_dir(UP_VECTOR, 0.5f, 1.0f);
    push.x *= mass;
    push.y *= mass;
    push.z *= mass;
    vector total;
    total.x = impulse.x + push.x;
    total.y = push.y + impulse.y;
    total.z = push.z + impulse.z;
    havok_body_apply_linear_impulse(rb, total);
}

// 0x00A73440 (:727): physics arrives in EAX.
static void kill_item_component(const turret_physics* physics, turret_components component, const turret* tp, float total_mass)
{
    const item_info* i_info = tp->t_info->components[component].i_info;
    const turret_component_physics_info& info = physics->components[component];
    if (i_info == nullptr || info.smesh == nullptr || info.havok_handle == 0xFFFFFFFF)
    {
        return;
    }
    hkpRigidBody* rb = havok_body_get_pointer(info.havok_handle);
    vector pos;
    matrix orient;
    havok_body_get_pos_orient(rb, pos, orient);
    vector linear_vel;
    vector angular_vel;
    havok_body_get_linear_velocity(rb, linear_vel);
    havok_body_get_angular_velocity(rb, angular_vel);
    const float mass = havok_body_get_mass(rb);
    if (component == TURRET_COMPONENT_BASE)
    {
        pos.y += 0.05f; // lifted off the floor it stood on
    }
    create_debris_object(physics, i_info, pos, orient, linear_vel, angular_vel, mass, total_mass);
}

// 0x00A73530 (:763): wp arrives in EAX, tp in ESI, physics in EDI.
static void kill_weapon(const weapon* wp, const turret* tp, const turret_physics* physics, float total_mass)
{
    const item_info* corpse = tp->t_info->weapon_corpse_info;
    if (corpse == nullptr)
    {
        return;
    }
    const vector pos = wp->pos;
    const matrix orient(wp->orient);
    const vector zero = ZERO_VECTOR;
    create_debris_object(physics, corpse, pos, orient, zero, zero, WEAPON_DEBRIS_MASS, total_mass);
}

// 0x00A73600 (:781): tp arrives in EAX, physics in ECX. The weapons only lose their bodies here; turret::kill destroys
// them.
static void kill_components(const turret_physics* physics, const turret* tp)
{
    const float total_mass = get_total_item_mass(physics, tp);
    for (int i = 0; i < NUM_TURRET_COMPONENTS; i++)
    {
        kill_item_component(physics, (turret_components)i, tp, total_mass);
    }
    weapon* wp = tp->get_weapon_inst();
    if (wp != nullptr)
    {
        kill_weapon(wp, tp, physics, total_mass);
        if (wp->havok_handle != 0xFFFFFFFF)
        {
            havok_body_free(wp->havok_handle, false, true, false);
            wp->havok_handle = 0xFFFFFFFF;
        }
    }
    wp = tp->get_secondary_weapon_inst();
    if (wp != nullptr && wp->havok_handle != 0xFFFFFFFF)
    {
        havok_body_free(wp->havok_handle, false, true, false);
        wp->havok_handle = 0xFFFFFFFF;
    }
}

// 0x00A20B90 (:811): tp arrives in EAX. Only a turret with a base item stands in the way of pathfinding.
static void add_detour_request(turret_physics* physics, const turret* tp)
{
    if (tp->t_info->components[TURRET_COMPONENT_BASE].i_info != nullptr)
    {
        physics->detour_request_handle = nav_cell_detour_add_request(tp->get_handle(), -1);
    }
}

// 0x00A20BC0 (:822): physics arrives in ESI.
static void remove_detour_request(turret_physics* physics)
{
    if (physics->detour_request_handle != 0xFFFFFFFF)
    {
        nav_cell_detour_remove_request(physics->detour_request_handle);
        physics->detour_request_handle = 0xFFFFFFFF;
    }
}

// 0x00A20BE0 (:843): tag_orient arrives in EAX, vp in ECX.
static void turret_component_get_tag_world_coords(const turret_component_physics_info& tcpi, int attach_tag_idx, const vehicle* vp, vector& tag_pos, matrix* tag_orient)
{
    matrix local_orient;
    if (tag_orient == nullptr)
    {
        tag_orient = &local_orient;
    }
    if (attach_tag_idx == -1)
    {
        havok_body_get_pos_orient(tcpi.havok_handle, tag_pos, *tag_orient);
        return;
    }
    if (tcpi.is_veh_component)
    {
        vp->get_tag_world_coords(attach_tag_idx, tag_pos, tag_orient, nullptr, nullptr);
        return;
    }
    if (tcpi.smesh == nullptr)
    {
        keen::debug::nativePrintArgs("Int3 critical function"); // Original: the formatted file and line
        return;
    }
    vector body_pos;
    matrix body_orient;
    havok_body_get_pos_orient(tcpi.havok_handle, body_pos, body_orient);
    matrix43 base_transform;
    base_transform.set_orientation(body_orient);
    base_transform.set_translation(body_pos);
    matrix43 tag_transform;
    tcpi.smesh->m_tags.get_tag_world_transform(tag_transform, attach_tag_idx, base_transform);
    tag_transform.get_translation(tag_pos);
    tag_transform.get_orientation(*tag_orient);
}

// 0x00A20D50 (:896): physics arrives in EAX, wp in ESI, tag_orient in EDI. A grip tag: the weapon's tag in the frame of
// the top's attach tag, which is where the weapon really sits.
static void turret_weapon_get_tag_world_coordinates(const turret_physics* physics, const vehicle* vp, const weapon* wp, int weapon_tag_idx, vector& tag_pos, matrix* tag_orient)
{
    const turret_component_physics_info& top = physics->components[TURRET_COMPONENT_TOP];
    vector top_pos;
    matrix top_orient;
    turret_component_get_tag_world_coords(top, top.attach_tag_idx, vp, top_pos, &top_orient);
    if (wp == nullptr || !wp->obj_flags.streamed || weapon_tag_idx == -1)
    {
        tag_pos = top_pos;
        if (tag_orient != nullptr)
        {
            *tag_orient = top_orient;
        }
        return;
    }
    wp->get_tag_local_coords(weapon_tag_idx, tag_pos, tag_orient);
    vec_change_to_global_coords(tag_pos, top_pos, top_orient);
    if (tag_orient != nullptr)
    {
        tag_orient->unrotate(top_orient);
    }
}

// 0x00A20E10 (:934): last_attach arrives in ESI. Tags come numbered: name1, name2, ...; wraps to 1.
static int cycle_vehicle_component_attach_tag_next(unsigned int& last_attach, const char* tag_name, const vehicle* vp, int c_idx)
{
    char name[64];
    last_attach++;
    sprintf(name, "%s%d", tag_name, last_attach);
    int tag = vp->get_component_tag_index_from_name(c_idx, name);
    if (tag == -1)
    {
        last_attach = 1;
        sprintf(name, "%s%d", tag_name, last_attach);
        tag = vp->get_component_tag_index_from_name(c_idx, name);
    }
    return tag;
}

// 0x00A20E90 (:966): component_idx arrives in ECX, attach_tag_name in EAX.
static bool cycle_vehicle_component_attach_tag(int& attach_tag_index, unsigned int& attach_tag_offset, int component_idx, const vehicle* vp, const char* attach_tag_name)
{
    if (component_idx == -1)
    {
        return false;
    }
    attach_tag_index = vp->get_component_tag_index_from_name(component_idx, attach_tag_name);
    if (attach_tag_index != -1)
    {
        return true;
    }
    attach_tag_index = cycle_vehicle_component_attach_tag_next(attach_tag_offset, attach_tag_name, vp, component_idx);
    return attach_tag_index != -1;
}

// 0x00A446C0 (:996): vc_idx arrives in ECX, component in EDX, tcpi in EDI. The vehicle part becomes a light rigid body
// with the table's mass (at least 50) and a bounded inertia.
static bool fetch_and_initialize_vehicle_component(const turret* tp, turret_component_physics_info& tcpi, vehicle* vp, int vc_idx, turret_components component)
{
    vehicle_component* vcp = &vp->rdd.get_data()->m_components[vc_idx];
    const char* tag_name = Turret_attach_tag_names[component];
    tcpi.veh_component_idx = vc_idx;
    tcpi.is_veh_component = true;
    vehicle_component_create_rigid_body(vcp, vp, false);
    if (vcp->havok_handle == 0xFFFFFFFF)
    {
        return false;
    }
    vehicle_component_set_hierarchy_collision(vcp, vp, COLLIDE_NO_COLLISION);
    if (!vcp->create_renderable_instance_subtree(vp))
    {
        return false;
    }
    tcpi.havok_handle = vcp->havok_handle;
    hkpRigidBody* rb = havok_body_get_pointer(vcp->havok_handle);
    if (rb == nullptr)
    {
        return false;
    }
    havok_body_set_linear_dampening(rb, 0.01f);
    havok_body_set_angular_dampening(rb, 0.2f);
    float mass;
    vector center_of_mass;
    matrix inertia;
    havok_body_get_mass_props(rb, mass, center_of_mass, inertia);
    mass = tp->t_info->mass[component];
    if (0.0f <= MIN_VEHICLE_COMPONENT_MASS - mass)
    {
        mass = MIN_VEHICLE_COMPONENT_MASS;
    }
    hkAabb aabb;
    havok_cm_manager_get(vcp->get_cm_index())->getAabb(hkTransformf::getIdentity(), 0.0f, aabb);
    vector bmin;
    vector bmax;
    havok_to_game_aabb(bmin, bmax, aabb);
    vector unused_center;
    havok_body_calc_mass_props(bmin, bmax, mass, unused_center, inertia);
    inertia.rvec.x = turret_clamp(inertia.rvec.x, MIN_TURRET_TENSOR_COMPONENT, MAX_TURRET_TENSOR_COMPONENT);
    inertia.uvec.y = turret_clamp(inertia.uvec.y, MIN_TURRET_TENSOR_COMPONENT, MAX_TURRET_TENSOR_COMPONENT);
    inertia.fvec.z = turret_clamp(inertia.fvec.z, MIN_TURRET_TENSOR_COMPONENT, MAX_TURRET_TENSOR_COMPONENT);
    havok_body_set_mass_props(rb, mass, center_of_mass, inertia);
    set_material_properties(rb);
    tcpi.attach_tag_idx = vp->get_component_tag_index_from_name(vc_idx, tag_name);
    if (tcpi.attach_tag_idx == -1)
    {
        cycle_vehicle_component_attach_tag(tcpi.attach_tag_idx, tcpi.attach_tag_offset, vc_idx, vp, tag_name);
    }
    return tcpi.attach_tag_idx != -1;
}

// 0x00A449C0 (:1070): finds the vehicle parts named by the turret info; the base is the first parent with a body, or
// the vehicle itself.
static bool fetch_and_initialize_vehicle_components(turret_physics* physics, vehicle* vp, const turret* tp)
{
    const turret_info* info = tp->t_info;
    if (physics->components[TURRET_COMPONENT_BASE].havok_handle != 0xFFFFFFFF || physics->components[TURRET_COMPONENT_BASE].smesh != nullptr || !vp->rdd.is_loaded())
    {
        return false;
    }
    const char* middle_name = info->components[TURRET_COMPONENT_MIDDLE].vehicle_component_name;
    const char* top_name = info->components[TURRET_COMPONENT_TOP].vehicle_component_name;
    vehicle::resource_dependent_data* vdata = vp->rdd.get_data();
    turret_component_physics_info& middle = physics->components[TURRET_COMPONENT_MIDDLE];
    turret_component_physics_info& top = physics->components[TURRET_COMPONENT_TOP];
    for (int i = 0; i < (int)vdata->sub_info->m_num_components; i++)
    {
        const vehicle_component_data& data = vdata->sub_info->m_components[i];
        if (data.m_type == VCT_TURRET)
        {
            if (middle_name != nullptr && v_stricmp(data.bone_name, middle_name) == 0)
            {
                if (middle.veh_component_idx != -1 || middle.havok_handle != 0xFFFFFFFF || middle.smesh != nullptr)
                {
                    return false;
                }
                if (!fetch_and_initialize_vehicle_component(tp, middle, vp, i, TURRET_COMPONENT_MIDDLE))
                {
                    return false;
                }
            }
            if (top_name != nullptr && v_stricmp(data.bone_name, top_name) == 0)
            {
                if (top.veh_component_idx != -1 || top.havok_handle != 0xFFFFFFFF || top.smesh != nullptr)
                {
                    return false;
                }
                if (!fetch_and_initialize_vehicle_component(tp, top, vp, i, TURRET_COMPONENT_TOP))
                {
                    return false;
                }
            }
        }
    }
    if ((middle_name != nullptr && middle.veh_component_idx == -1) || (top_name != nullptr && top.veh_component_idx == -1))
    {
        return false;
    }
    turret_component_physics_info& base = physics->components[TURRET_COMPONENT_BASE];
    if (!info->flags.transform_to_tag && middle.veh_component_idx != -1)
    {
        for (unsigned int parent = vdata->m_components[middle.veh_component_idx].get_data()->m_parent_index; parent != 0xFFFFFFFF; parent = vdata->m_components[parent].get_data()->m_parent_index)
        {
            if (vdata->m_components[parent].havok_handle != 0xFFFFFFFF)
            {
                base.veh_component_idx = parent;
                base.havok_handle = vdata->m_components[parent].havok_handle;
                break;
            }
        }
    }
    if (base.veh_component_idx == -1)
    {
        base.havok_handle = vp->havok_handle;
    }
    base.is_veh_component = true;
    if (base.havok_handle == 0xFFFFFFFF)
    {
        return false;
    }
    base.attach_tag_idx = tp->get_vehicle_attach_tag_index();
    return true;
}

// 0x00A736F0 (:1173): the mesh and body of one component. The top of an item turret without a top item takes the
// weapon's mesh for its box and collision only (the weapon draws itself), so that mesh is released again.
static bool create_item_component(turret_physics* physics, turret* tp, turret_components component, unsigned int collide_type, vector& initial_pos)
{
    const turret_info* t_info = tp->t_info;
    const bool vehicle_turret = tp->turret_flags.vehicle_turret;
    turret_component_physics_info& tcpi = physics->components[component];
    if (tcpi.havok_handle != 0xFFFFFFFF || tcpi.smesh != nullptr || tcpi.is_veh_component)
    {
        return false;
    }
    const item_info* i_info = t_info->components[component].i_info;
    bool temporary_mesh = false;
    bool load_mesh = i_info != nullptr;
    if (i_info == nullptr && t_info->components[component].vehicle_component_name == nullptr)
    {
        if (component != TURRET_COMPONENT_TOP)
        {
            return false;
        }
        item_info* weapon_item = t_info->w_info->weapon_obj_item_info;
        if (weapon_item->smesh_name != nullptr)
        {
            i_info = weapon_item;
            temporary_mesh = true;
            load_mesh = true;
        }
    }
    if (load_mesh)
    {
        if (i_info->smesh_name == nullptr)
        {
            return false;
        }
        tcpi.smesh = static_mesh_find(i_info->smesh_name, i_info->srid);
        if (tcpi.smesh == nullptr)
        {
            return false;
        }
        tcpi.smesh_instance = rl_mesh_instance::create();
        if (tcpi.smesh_instance == nullptr)
        {
            return false;
        }
        tcpi.smesh_instance->init(tcpi.smesh->m_shared_header.m_mesh.to_pointer(), tcpi.smesh->m_shared_header.m_material_map.to_pointer(), *tcpi.smesh->m_lod_submesh_id.to_pointer());
        tcpi.smesh_instance->calculate_local_aabb();
        // Deviation from rfg.exe: set_is_visible and set_lod_level, where it writes the fields inline.
        tcpi.smesh_instance->set_is_visible(true);
        tcpi.smesh_instance->set_lod_level(0);
        tcpi.smesh_instance->set_opacity(1.0f);
        if (!temporary_mesh)
        {
            tcpi.attach_tag_idx = tcpi.smesh->m_tags.get_tag(Turret_attach_tag_names[component]);
            if (tcpi.attach_tag_idx == -1)
            {
                return false;
            }
        }
    }

    vector bmin;
    vector bmax;
    vector size;
    float radius;
    if (tcpi.smesh_instance != nullptr)
    {
        const rl_aabb box = tcpi.smesh_instance->get_local_aabb();
        bmin = box.min;
        bmax = box.max;
        size.x = bmax.x - bmin.x;
        size.y = bmax.y - bmin.y;
        size.z = bmax.z - bmin.z;
        radius = tcpi.smesh_instance->get_local_bounding_sphere().m_radius;
    }
    else
    {
        // A vehicle component has no mesh here: a small box without collision.
        bmin.x = -0.25f;
        bmin.y = -0.25f;
        bmin.z = -0.25f;
        bmax.x = 0.25f;
        bmax.y = 0.25f;
        bmax.z = 0.25f;
        size.x = 0.5f;
        size.y = 0.5f;
        size.z = 0.5f;
        radius = size.mag() * 0.5f;
    }

    physics_create_info pci;
    pci.pos = initial_pos;
    pci.orient = tp->orient;
    pci.local_pos.x = (bmax.x + bmin.x) * 0.5f;
    pci.local_pos.y = (bmax.y + bmin.y) * 0.5f;
    pci.local_pos.z = (bmax.z + bmin.z) * 0.5f;
    pci.local_orient.make_identity();
    pci.size = size;
    pci.radius = radius;
    pci.shape_type = HAVOK_SHAPE_TYPE_BOX;
    pci.mass = vehicle_turret ? MIN_VEHICLE_ITEM_COMPONENT_MASS : DEFAULT_ITEM_COMPONENT_MASS;
    pci.friction = 0.2f;
    pci.restitution = 0.2f;
    pci.linear_damping = 0.01f;
    pci.angular_damping = 0.2f;
    if (i_info != nullptr)
    {
        pci.mass = i_info->mass;
        pci.friction = i_info->friction;
        pci.restitution = i_info->restitution;
        pci.linear_damping = i_info->linear_damping;
        pci.angular_damping = i_info->angular_damping;
    }
    item_calc_collision_params(pci, i_info, bmin, bmax);
    if (tcpi.smesh != nullptr)
    {
        pci.cm_index = item_smesh_get_collision_model(tcpi.smesh, pci);
        if (pci.cm_index != 0xFFFFFFFF)
        {
            pci.flags &= ~2;
            pci.shape_type = HAVOK_SHAPE_TYPE_COLLISION_MODEL;
        }
    }
    float min_mass;
    if (vehicle_turret)
    {
        // A component on a vehicle takes the table's mass.
        pci.mass = t_info->mass[component];
        if (0.0f <= MIN_VEHICLE_ITEM_COMPONENT_MASS - pci.mass)
        {
            pci.mass = MIN_VEHICLE_ITEM_COMPONENT_MASS;
        }
        min_mass = MIN_VEHICLE_ITEM_COMPONENT_MASS;
    }
    else
    {
        min_mass = DEFAULT_ITEM_COMPONENT_MASS;
    }
    const float mass = 0.0f <= pci.mass - min_mass ? pci.mass : min_mass;
    const float tensor_scale = mass * 5.0f;
    havok_adjust_tensor(pci.precalc_tensor, 1.0f, 0.1f);
    pci.precalc_tensor.rvec.x *= tensor_scale;
    pci.precalc_tensor.rvec.y *= tensor_scale;
    pci.precalc_tensor.rvec.z *= tensor_scale;
    pci.precalc_tensor.rvec.x = turret_clamp(pci.precalc_tensor.rvec.x, MIN_TURRET_TENSOR_COMPONENT, MAX_TURRET_TENSOR_COMPONENT);
    pci.precalc_tensor.uvec.x *= tensor_scale;
    pci.precalc_tensor.uvec.y *= tensor_scale;
    pci.precalc_tensor.uvec.z *= tensor_scale;
    pci.precalc_tensor.uvec.y = turret_clamp(pci.precalc_tensor.uvec.y, MIN_TURRET_TENSOR_COMPONENT, MAX_TURRET_TENSOR_COMPONENT);
    pci.precalc_tensor.fvec.x *= tensor_scale;
    pci.precalc_tensor.fvec.y *= tensor_scale;
    pci.precalc_tensor.fvec.z *= tensor_scale;
    pci.precalc_tensor.fvec.z = turret_clamp(pci.precalc_tensor.fvec.z, MIN_TURRET_TENSOR_COMPONENT, MAX_TURRET_TENSOR_COMPONENT);
    pci.mass = mass;
    pci.collision_type = tcpi.smesh != nullptr ? collide_type : COLLIDE_NO_COLLISION;
    pci.simulation_quality_type = HK_COLLIDABLE_QUALITY_MOVING;
    pci.physical_material = Default_turret_physical_material_index;
    pci.effect_material = Default_turret_effect_material_index;
    pci.obj_ptr = tp;
    tcpi.havok_handle = havok_body_create(pci);
    if (tcpi.havok_handle == 0xFFFFFFFF)
    {
        return false;
    }
    if (component == TURRET_COMPONENT_BASE)
    {
        havok_body_set_motion_type(tcpi.havok_handle, 5); // MOTION_FIXED
    }
    if (temporary_mesh)
    {
        tcpi.smesh_instance->release();
        tcpi.smesh_instance = nullptr;
        tcpi.smesh = nullptr;
    }
    return true;
}

// 0x00AB03C0 (:1352): tp arrives in ESI.
static bool assemble_components(turret_physics* physics, turret* tp, vehicle* vp)
{
    if (tp->obj_flags.destroyed)
    {
        return false;
    }
    const turret_info* info = tp->t_info;
    unsigned int collide_type;
    if (vp == nullptr)
    {
        collide_type = COLLIDE_TURRET;
        if (info->components[TURRET_COMPONENT_BASE].i_info == nullptr)
        {
            return false;
        }
    }
    else
    {
        if (!vp->rdd.is_loaded())
        {
            return false;
        }
        collide_type = vehicle_get_collision_filter(vp);
    }
    vector next_pos = tp->pos;
    for (int c = 0; c < NUM_TURRET_COMPONENTS; c++)
    {
        if (vp == nullptr || (c != TURRET_COMPONENT_BASE && info->components[c].vehicle_component_name == nullptr))
        {
            if (!create_item_component(physics, tp, (turret_components)c, collide_type, next_pos))
            {
                return false;
            }
            const turret_component_physics_info& tcpi = physics->components[c];
            if (tcpi.attach_tag_idx == -1)
            {
                next_pos = tp->pos;
            }
            else
            {
                turret_component_get_tag_world_coords(tcpi, tcpi.attach_tag_idx, vp, next_pos, nullptr);
            }
        }
    }
    if (vp != nullptr && !fetch_and_initialize_vehicle_components(physics, vp, tp))
    {
        return false;
    }
    if (physics->components[TURRET_COMPONENT_TOP].havok_handle == 0xFFFFFFFF || physics->components[TURRET_COMPONENT_MIDDLE].havok_handle == 0xFFFFFFFF || physics->components[TURRET_COMPONENT_BASE].havok_handle == 0xFFFFFFFF)
    {
        return false;
    }
    tp->havok_handle = 0xFFFFFFFF;
    for (int c = 0; c < NUM_TURRET_COMPONENTS; c++)
    {
        if (!physics->components[c].is_veh_component)
        {
            tp->havok_handle = physics->components[c].havok_handle;
            break;
        }
    }
    if (info->secondary_w_info != nullptr && !get_secondary_weapon_mount_point(physics, tp, vp))
    {
        return false;
    }
    if (tp->t_info->flags.automated || tp->t_info->flags.no_operator_animation)
    {
        physics->grip_tags_valid = true;
    }
    else
    {
        physics->grip_tags_valid = get_grip_points(physics, tp);
    }
    if (vp != nullptr && !info->flags.transform_to_tag)
    {
        vp->remove_child(tp);
        vp->set_child(tp, tp->get_vehicle_attach_tag_index(), -1, false, true);
    }
    vector top_mid_pos;
    vector mid_base_pos;
    matrix top_mid_ort;
    matrix mid_base_ort;
    if (!get_constraint_attach_points(top_mid_pos, mid_base_pos, top_mid_ort, mid_base_ort, physics, vp))
    {
        return false;
    }
    if (info->flags.transform_to_tag)
    {
        physics->last_local_tag_pos = mid_base_pos;
        physics->last_local_tag_orient = mid_base_ort;
    }
    // Motor power: the table's, else 10 (item) or 4 (vehicle) horsepower for the middle, 3 or 2 for the top.
    float power = info->middle_motor_power < 0.0f ? (physics->components[TURRET_COMPONENT_MIDDLE].is_veh_component ? 4.0f : 10.0f) : info->middle_motor_power;
    if (!build_pos_motor(physics->components[TURRET_COMPONENT_MIDDLE].pos_motor, power)
        || !build_vel_motor(physics->components[TURRET_COMPONENT_MIDDLE].vel_motor, power, 1.0f))
    {
        return false;
    }
    power = info->top_motor_power < 0.0f ? (physics->components[TURRET_COMPONENT_TOP].is_veh_component ? 2.0f : 3.0f) : info->top_motor_power;
    if (!build_pos_motor(physics->components[TURRET_COMPONENT_TOP].pos_motor, power)
        || !build_vel_motor(physics->components[TURRET_COMPONENT_TOP].vel_motor, power, 1.0f))
    {
        return false;
    }
    if (!constrain_turret_components(physics, TURRET_COMPONENT_TOP, top_mid_pos, top_mid_ort, tp->get_v_fov_neg(), tp->get_v_fov_pos(), tp->t_info->flags.has_rear_fov_limits ? 1.0f : 1.5f, true, vp))
    {
        return false;
    }
    return constrain_turret_components(physics, TURRET_COMPONENT_MIDDLE, mid_base_pos, mid_base_ort, -tp->get_h_fov_pos(), -tp->get_h_fov_neg(), 1.5f, !tp->turret_flags.full360, vp);
}

// 0x00A264A0: pool elements only need the timer cleared; init does the rest.
turret_physics::turret_physics()
{
    update_tag_time.invalidate();
}

// 0x00A20EF0 (:1494)
void turret_component_physics_info::clear()
{
    smesh = nullptr;
    smesh_instance = nullptr;
    is_veh_component = false;
    veh_component_idx = -1;
    attach_tag_idx = -1;
    attach_tag_offset = 0;
    havok_handle = 0xFFFFFFFF;
    hconstraint = nullptr;
    pos_motor = nullptr;
    vel_motor = nullptr;
}

// 0x00A44C00 (:1508)
void turret_physics::init()
{
    detour_request_handle = 0xFFFFFFFF;
    base_constraint_handle = 0xFFFFFFFF;
    grip_tags_valid = false;
    left_grip_tag_index = -1;
    right_grip_tag_index = -1;
    secondary_weapon_attach_component = TURRET_COMPONENT_NONE;
    secondary_weapon_attach_tag_idx = -1;
    secondary_weapon_attach_offset = 0;
    pitch = 0.0f;
    heading = 0.0f;
    heading_sum = 0.0f;
    kill_angular_impulse.zero();
    kill_linear_impulse.zero();
    update_tag_time.set(0);
    last_local_tag_pos.zero();
    last_local_tag_orient = IDENTITY_MATRIX;
    clear_objects();
}

// 0x00A20F20 (:1530)
void turret_physics::clear_objects()
{
    components[0].clear();
    components[1].clear();
    components[2].clear();
}

// 0x00A74050 (:1551)
turret_physics* turret_physics_alloc()
{
    turret_physics* physics = Turret_physics_pool.alloc(0);
    if (physics == nullptr)
    {
        keen::debug::nativePrintArgs("Out of turret physics objects, but there are MAX_TURRETS of them!");
    }
    return physics;
}

// 0x00A97020 (:1564)
void turret_physics_free(turret_physics* physics)
{
    Turret_physics_pool.free(physics);
}

// 0x00AB0830 (:1572)
bool turret_physics_stream_load(turret_physics* physics, turret* tp, vehicle* vp)
{
    add_detour_request(physics, tp);
    if (!assemble_components(physics, tp, vp))
    {
        return false;
    }
    bool is_player = false;
    bool is_player_aimed = false;
    if (tp->current_operator != 0xFFFFFFFF)
    {
        human* hp = World.object_from_handle<human>(tp->current_operator);
        is_player = hp != nullptr && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER;
        is_player_aimed = is_player;
        if (is_player)
        {
            vehicle* tvp = tp->get_vehicle();
            if (tvp != nullptr)
            {
                const vehicle_seat_index seat = vehicle_passenger_get_seat(tvp, hp);
                if (seat != VSI_INVALID)
                {
                    is_player_aimed = vehicle_get_seat_turret(tvp, seat) == tp;
                }
            }
        }
    }
    turret_physics_set_control_type(physics, is_player, is_player_aimed);
    turret_physics_set_no_collide(physics, tp, tp->current_operator);
    return true;
}

// 0x00A44CD0 (:1611)
void turret_physics_stream_unload(turret_physics* physics, turret* tp, vehicle* vp)
{
    remove_detour_request(physics);
    detach_all(physics, tp);
    destroy_components(physics);
    tp->havok_handle = 0xFFFFFFFF;
}

// 0x00A44D10 (:1626)
void turret_physics_children_streamed(turret_physics* physics, turret* tp)
{
    if (!physics->grip_tags_valid)
    {
        physics->grip_tags_valid = get_grip_points(physics, tp);
    }
}

// 0x00A74080 (:1638)
void turret_physics_kill_turret(turret_physics* physics, turret* tp)
{
    remove_detour_request(physics);
    detach_all(physics, tp);
    kill_components(physics, tp);
    destroy_components(physics);
    physics->clear_objects();
    tp->havok_handle = 0xFFFFFFFF;
}

// 0x00A44D40 (:1666)
void turret_physics_get_tag_world_coords(turret_physics* physics, const turret* tp, const vehicle* vp, int tag_index, vector& tag_pos, matrix* tag_orient)
{
    switch (tag_index)
    {
    case 0:
        if (vp != nullptr)
        {
            vp->get_tag_world_coords(physics->components[TURRET_COMPONENT_BASE].attach_tag_idx, tag_pos, tag_orient, nullptr, nullptr);
            return;
        }
        turret_component_get_tag_world_coords(physics->components[TURRET_COMPONENT_BASE], physics->components[TURRET_COMPONENT_BASE].attach_tag_idx, vp, tag_pos, tag_orient);
        return;
    case 1:
        turret_component_get_tag_world_coords(physics->components[TURRET_COMPONENT_MIDDLE], physics->components[TURRET_COMPONENT_MIDDLE].attach_tag_idx, vp, tag_pos, tag_orient);
        return;
    case 2:
        turret_component_get_tag_world_coords(physics->components[TURRET_COMPONENT_TOP], physics->components[TURRET_COMPONENT_TOP].attach_tag_idx, vp, tag_pos, tag_orient);
        return;
    case 3:
        if (physics->secondary_weapon_attach_component != TURRET_COMPONENT_NONE)
        {
            turret_component_get_tag_world_coords(physics->components[physics->secondary_weapon_attach_component], physics->secondary_weapon_attach_tag_idx, vp, tag_pos, tag_orient);
        }
        return;
    case 4:
        turret_weapon_get_tag_world_coordinates(physics, vp, tp->get_weapon_inst(), physics->left_grip_tag_index, tag_pos, tag_orient);
        return;
    case 5:
        turret_weapon_get_tag_world_coordinates(physics, vp, tp->get_weapon_inst(), physics->right_grip_tag_index, tag_pos, tag_orient);
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function"); // Original: the formatted file and line
        return;
    }
}

// The weapon's box in the turret's space, added to the turret's (inlined twice in the original).
static void turret_physics_expand_bbox_by_weapon(const turret* tp, const weapon* wp, vector& bmin, vector& bmax)
{
    vector weapon_min;
    vector weapon_max;
    wp->get_local_bbox(weapon_min, weapon_max);
    vector pos = wp->pos;
    matrix orient = wp->orient;
    vec_change_to_local_coords(pos, tp->pos, tp->orient);
    orient.rotate(tp->orient);
    vector box_min;
    vector box_max;
    bbox_instance(box_min, box_max, orient, pos, weapon_min, weapon_max);
    bbox_expand(bmin, bmax, box_min);
    bbox_expand(bmin, bmax, box_max);
}

// 0x00A44EC0 (:1714)
void turret_physics_get_local_bbox(turret_physics* physics, const turret* tp, vector& bmin, vector& bmax)
{
    bmin.x = -0.25f;
    bmin.y = 0.0f;
    bmin.z = -0.25f;
    bmax.x = 0.25f;
    bmax.y = 0.5f;
    bmax.z = 0.25f;
    // A vehicle turret skips its base (the flag's value 1 starts the loop at the middle).
    for (int c = tp->turret_flags.vehicle_turret ? 1 : 0; c < NUM_TURRET_COMPONENTS; c++)
    {
        const turret_component_physics_info& tcpi = physics->components[c];
        if (tcpi.havok_handle != 0xFFFFFFFF && tcpi.smesh != nullptr)
        {
            vector pos;
            matrix orient;
            havok_body_get_pos_orient(tcpi.havok_handle, pos, orient);
            vec_change_to_local_coords(pos, tp->pos, tp->orient);
            orient.rotate(tp->orient);
            vector box_min;
            vector box_max;
            havok_body_get_aabb(tcpi.havok_handle, pos, orient, box_min, box_max);
            bbox_expand(bmin, bmax, box_min);
            bbox_expand(bmin, bmax, box_max);
        }
    }
    const weapon* wp = tp->get_weapon_inst();
    if (wp != nullptr)
    {
        turret_physics_expand_bbox_by_weapon(tp, wp, bmin, bmax);
    }
    wp = tp->get_secondary_weapon_inst();
    if (wp != nullptr)
    {
        turret_physics_expand_bbox_by_weapon(tp, wp, bmin, bmax);
    }
}

// 0x00A451A0 (:1766): a firing weapon moves to its next tag. A weapon with local doors closes the old tag's doors when
// the tag moves to another component.
void turret_physics_cycle_tag(turret_physics* physics, const turret* tp, const vehicle* vp, int tag_index)
{
    const turret_component_physics_info* tcpi = nullptr;
    int* tag = nullptr;
    unsigned int* offset = nullptr;
    const char* tag_name = nullptr;
    bool local_door = false;
    if (tag_index == 2)
    {
        tcpi = &physics->components[TURRET_COMPONENT_TOP];
        local_door = tp->t_info->flags.primary_weapon_local_door;
        tag = &physics->components[TURRET_COMPONENT_TOP].attach_tag_idx;
        offset = &physics->components[TURRET_COMPONENT_TOP].attach_tag_offset;
        tag_name = "turret_head";
    }
    else if (tag_index == 3)
    {
        const turret_components comp = physics->secondary_weapon_attach_component;
        if (comp != TURRET_COMPONENT_NONE)
        {
            tcpi = &physics->components[comp];
            tag = &physics->secondary_weapon_attach_tag_idx;
            offset = &physics->secondary_weapon_attach_offset;
            tag_name = tp->t_info->flags.secondary_uses_primary_tags ? "turret_head" : Secondary_weapon_attach_tag_names[comp];
        }
        local_door = tp->t_info->flags.secondary_weapon_local_door;
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function"); // Original: the formatted file and line
    }
    // Original: with no secondary mount or a bad index tcpi is null here, and this reads through it.
    const int c_idx = tcpi->veh_component_idx;
    if (local_door)
    {
        vehicle::resource_dependent_data* vdata = const_cast<vehicle*>(vp)->rdd.is_loaded() ? const_cast<vehicle*>(vp)->rdd.get_data() : nullptr;
        vehicle_component* old_component = &vdata->m_components[vp->get_component_index_from_tag_index(*tag)];
        if (c_idx != -1)
        {
            *tag = vp->get_component_tag_index_from_name(c_idx, tag_name);
            if (*tag == -1)
            {
                *tag = cycle_vehicle_component_attach_tag_next(*offset, tag_name, vp, c_idx);
            }
        }
        if (&vdata->m_components[vp->get_component_index_from_tag_index(*tag)] != old_component)
        {
            int door_index = 0;
            for (vehicle_component* door = vehicle_component_find_child_door(old_component, vp, 0); door != nullptr; door = vehicle_component_find_child_door(old_component, vp, door_index))
            {
                vehicle_component_close_door(door, const_cast<vehicle*>(vp), 0.5f);
                door_index++;
            }
        }
        return;
    }
    if (c_idx != -1)
    {
        *tag = vp->get_component_tag_index_from_name(c_idx, tag_name);
        if (*tag == -1)
        {
            *tag = cycle_vehicle_component_attach_tag_next(*offset, tag_name, vp, c_idx);
        }
    }
}

// 0x00A45380 (:1844)
void turret_physics_reset_cycle(turret_physics* physics, const turret* tp, const vehicle* vp, int tag_index)
{
    if (tag_index == 2)
    {
        physics->components[TURRET_COMPONENT_TOP].attach_tag_offset = 0;
    }
    else if (tag_index == 3)
    {
        physics->secondary_weapon_attach_offset = 0;
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function"); // Original: the formatted file and line
    }
    turret_physics_cycle_tag(physics, tp, vp, tag_index);
}

// 0x00A20F80 (:1870)
void turret_physics_play_alt_effect(turret_physics* physics, const turret* tp, const vehicle* vp, int tag_index)
{
    unsigned int effect;
    const int* tag;
    if (tag_index == 2)
    {
        effect = tp->t_info->primary_alt_fire_effect;
        tag = &physics->components[TURRET_COMPONENT_TOP].attach_tag_idx;
    }
    else if (tag_index == 3)
    {
        if (physics->secondary_weapon_attach_component == TURRET_COMPONENT_NONE)
        {
            return;
        }
        effect = tp->t_info->secondary_alt_fire_effect;
        tag = &physics->secondary_weapon_attach_tag_idx;
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function"); // Original: the formatted file and line
        return;
    }
    if (effect == 0xFFFFFFFF)
    {
        return;
    }
    const int c_idx = vp->get_component_index_from_tag_index(*tag);
    if (c_idx == -1)
    {
        return;
    }
    const int alt_tag = vp->get_component_tag_index_from_name(c_idx, Alt_fire_effect_tag_names[tag_index]);
    if (alt_tag < 0)
    {
        return;
    }
    vector pos;
    matrix orient;
    vp->get_tag_world_coords(alt_tag, pos, &orient, nullptr, nullptr);
    // Original bug kept: the secondary weapon plays the primary's effect (its own only decides whether one plays).
    effects_play(tp->t_info->primary_alt_fire_effect, pos, orient, false, vp->get_handle(), alt_tag, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
}

// Plays a door cue for doors_moved doors: a single one at the door, several as up to 4 staggered 2D-placed copies at
// the turret (inlined twice in the original; stagger 50 ms opening, 75 ms closing).
static void turret_physics_play_door_cues(const turret* tp, int cue_id, int doors_moved, const vector& single_pos, int stagger_ms)
{
    if (doors_moved == 1)
    {
        audiolib_cue_play_3d(cue_id, &single_pos, nullptr, nullptr, nullptr);
        return;
    }
    audiolib_params params;
    params.pos = tp->pos;
    params.flags |= 1; // 3D
    int count = (doors_moved + 3) / 4;
    if (count > 4)
    {
        count = 4;
    }
    int delay = 0;
    for (int i = 0; i < count; i++)
    {
        params.delay_ms = int_rand_range(-25, 25) + delay;
        audiolib_cue_play(cue_id, &params, nullptr);
        delay += stagger_ms;
    }
}

// 0x00A453E0 (:1914): true once every door the weapon fires through is open.
bool turret_physics_process_door_weapon_ready(turret_physics* physics, const turret* tp, const vehicle* vp, int tag_index)
{
    int tag = -1;
    int c_idx = -1;
    bool all_doors = false;
    switch (tag_index)
    {
    case 0:
    case 1:
    case 4:
    case 5:
        return false;
    case 2:
        tag = physics->components[TURRET_COMPONENT_TOP].attach_tag_idx;
        c_idx = physics->components[TURRET_COMPONENT_TOP].veh_component_idx;
        all_doors = tp->t_info->flags.primary_weapon_all_doors;
        break;
    case 3:
        if (physics->secondary_weapon_attach_component != TURRET_COMPONENT_NONE)
        {
            tag = physics->secondary_weapon_attach_tag_idx;
            c_idx = physics->components[physics->secondary_weapon_attach_component].veh_component_idx;
            all_doors = tp->t_info->flags.secondary_weapon_all_doors;
        }
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function"); // Original: the formatted file and line
        return false;
    }
    vehicle* mvp = const_cast<vehicle*>(vp);
    vehicle::resource_dependent_data* vdata = mvp->rdd.is_loaded() ? mvp->rdd.get_data() : nullptr;
    if (!all_doors)
    {
        c_idx = vp->get_component_index_from_tag_index(tag);
    }
    vector door_pos = tp->pos;
    const vehicle_component* parent = &vdata->m_components[c_idx];
    int doors_moved = 0;
    bool ready = true;
    int door_index = 0;
    for (vehicle_component* door = vehicle_component_find_child_door(parent, vp, 0); door != nullptr; door = vehicle_component_find_child_door(parent, vp, door_index))
    {
        if ((door->get_flags() & 0x10000) == 0) // not moving yet
        {
            door_pos = door->get_world_space_position();
            doors_moved++;
        }
        if (!vehicle_component_door_is_fully_open(door))
        {
            vehicle_component_open_door(door, mvp, 0.4f);
            ready = false;
        }
        door_index++;
    }
    if (doors_moved != 0 && tp->t_info->door_open_cue_id != -1)
    {
        turret_physics_play_door_cues(tp, tp->t_info->door_open_cue_id, doors_moved, door_pos, 50);
    }
    return ready;
}

// 0x00A45650 (:2010)
bool turret_physics_process_door_weapon_unready(turret_physics* physics, const turret* tp, const vehicle* vp, int tag_index)
{
    int c_idx = -1;
    switch (tag_index)
    {
    case 0:
    case 1:
    case 4:
    case 5:
        return true;
    case 2:
        c_idx = physics->components[TURRET_COMPONENT_TOP].veh_component_idx;
        break;
    case 3:
        if (physics->secondary_weapon_attach_component != TURRET_COMPONENT_NONE)
        {
            c_idx = physics->components[physics->secondary_weapon_attach_component].veh_component_idx;
        }
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function"); // Original: the formatted file and line
        return true;
    }
    vehicle* mvp = const_cast<vehicle*>(vp);
    vehicle::resource_dependent_data* vdata = mvp->rdd.is_loaded() ? mvp->rdd.get_data() : nullptr;
    vector door_pos = tp->pos;
    const vehicle_component* parent = &vdata->m_components[c_idx];
    int doors_moved = 0;
    int door_index = 0;
    for (vehicle_component* door = vehicle_component_find_child_door(parent, vp, 0); door != nullptr; door = vehicle_component_find_child_door(parent, vp, door_index))
    {
        if (!vehicle_component_door_is_fully_closed(door))
        {
            vehicle_component_close_door(door, mvp, 0.5f);
            door_pos = door->get_world_space_position();
            doors_moved++;
        }
        door_index++;
    }
    // Original bug kept: tests the open cue, then plays the close cue.
    if (doors_moved != 0 && tp->t_info->door_open_cue_id != -1)
    {
        turret_physics_play_door_cues(tp, tp->t_info->door_close_cue_id, doors_moved, door_pos, 75);
    }
    return true;
}

// 0x00A21090 (:2104): follows a moving vehicle tag by moving the hinge's pivot; 250 ms past the last move it stops.
void turret_physics_transform_to_tag(turret_physics* physics, const vehicle* vp)
{
    vector tag_pos;
    matrix tag_orient;
    vp->get_tag_local_coords(physics->components[TURRET_COMPONENT_BASE].attach_tag_idx, tag_pos, &tag_orient);
    if (0.0001f <= vec_dist_squared(tag_pos, physics->last_local_tag_pos))
    {
        physics->update_tag_time.set(250);
    }
    if (physics->update_tag_time.elapsed())
    {
        return;
    }
    physics->last_local_tag_pos = tag_pos;
    physics->last_local_tag_orient = tag_orient;
    hkVector4f pivot_a;
    hkVector4f pivot_b;
    hkVector4f axis_a;
    hkVector4f axis_b;
    hkVector4f perp_a;
    hkVector4f perp_b;
    game_to_havok_vector(pivot_a, tag_pos);
    game_to_havok_vector(pivot_b, ZERO_VECTOR);
    game_to_havok_vector(axis_a, tag_orient.uvec);
    game_to_havok_vector(axis_b, UP_VECTOR);
    game_to_havok_vector(perp_a, tag_orient.fvec);
    game_to_havok_vector(perp_b, FORWARD_VECTOR);
    ((hkpLimitedHingeConstraintData*)physics->components[TURRET_COMPONENT_MIDDLE].hconstraint->m_data)->setInBodySpace(pivot_a, pivot_b, axis_a, axis_b, perp_a, perp_b);
}

// 0x00A45870 (:2142): pulls a stable turret back to its tag when the solver let it drift. False when it is too far gone
// on an upright vehicle: vehicle_turrets_process_post then destroys the vehicle.
bool turret_physics_fix_pos_orient(turret_physics* physics, vehicle* vp)
{
    vector tag_pos;
    matrix tag_orient;
    vp->get_tag_world_coords(physics->components[TURRET_COMPONENT_BASE].attach_tag_idx, tag_pos, &tag_orient, nullptr, nullptr);
    hkpRigidBody* rb = havok_body_get_pointer(physics->components[TURRET_COMPONENT_MIDDLE].havok_handle);
    if (rb == nullptr)
    {
        return false;
    }
    vector body_pos;
    matrix body_orient;
    havok_body_get_pos_orient(rb, body_pos, body_orient);
    const float up_dot = tag_orient.uvec.y * body_orient.uvec.y + body_orient.uvec.x * tag_orient.uvec.x + tag_orient.uvec.z * body_orient.uvec.z;
    const float dist_sq = vec_dist_squared(body_pos, tag_pos);
    if (MAX_OVERTURNED_VEHICLE_TURRET_DOT < up_dot && dist_sq < TAG_DIST_SQ_OK)
    {
        return true;
    }
    if (((MAX_OVERTURNED_VEHICLE_TURRET_DOT > up_dot || dist_sq > TAG_DIST_SQ_OK) && vp->orient.uvec.y < 0.0f)
        || up_dot < TAG_DOT_MIN || dist_sq > TAG_DIST_SQ_MAX)
    {
        return false;
    }
    const float f_dot = body_orient.fvec.y * tag_orient.uvec.y + body_orient.fvec.x * tag_orient.uvec.x + body_orient.fvec.z * tag_orient.uvec.z;
    vector fvec;
    fvec.x = body_orient.fvec.x - f_dot * tag_orient.uvec.x;
    fvec.y = body_orient.fvec.y - tag_orient.uvec.y * f_dot;
    fvec.z = body_orient.fvec.z - tag_orient.uvec.z * f_dot;
    fvec.normalize_safe();
    matrix new_orient;
    new_orient.make_uvec(fvec, tag_orient.uvec);
    havok_body_set_pos_orient(rb, tag_pos, new_orient);
    vector ang_vel;
    havok_body_get_angular_velocity(rb, ang_vel);
    float along = ang_vel.y * tag_orient.uvec.y + ang_vel.x * tag_orient.uvec.x + ang_vel.z * tag_orient.uvec.z;
    ang_vel.x = along * tag_orient.uvec.x;
    ang_vel.y = tag_orient.uvec.y * along;
    ang_vel.z = tag_orient.uvec.z * along;
    havok_body_set_angular_velocity(rb, ang_vel);
    if (!physics->components[TURRET_COMPONENT_MIDDLE].is_veh_component)
    {
        return true;
    }
    vehicle::resource_dependent_data* vdata = vp->rdd.is_loaded() ? vp->rdd.get_data() : nullptr;
    vdata->m_components[physics->components[TURRET_COMPONENT_MIDDLE].veh_component_idx].set_orient_pos_in_world_space(vp, new_orient, tag_pos);
    vp->get_tag_world_coords(physics->components[TURRET_COMPONENT_MIDDLE].attach_tag_idx, tag_pos, &tag_orient, nullptr, nullptr);
    rb = havok_body_get_pointer(physics->components[TURRET_COMPONENT_TOP].havok_handle);
    if (rb == nullptr)
    {
        return false;
    }
    havok_body_get_pos_orient(rb, body_pos, body_orient);
    const float u_dot = tag_orient.rvec.y * body_orient.uvec.y + tag_orient.rvec.x * body_orient.uvec.x + tag_orient.rvec.z * body_orient.uvec.z;
    vector uvec;
    uvec.x = body_orient.uvec.x - u_dot * tag_orient.rvec.x;
    uvec.y = body_orient.uvec.y - tag_orient.rvec.y * u_dot;
    uvec.z = body_orient.uvec.z - tag_orient.rvec.z * u_dot;
    uvec.normalize_safe();
    new_orient.make_ruvec(tag_orient.rvec, uvec);
    havok_body_set_pos_orient(rb, tag_pos, new_orient);
    havok_body_get_angular_velocity(rb, ang_vel);
    along = tag_orient.rvec.y * ang_vel.y + tag_orient.rvec.x * ang_vel.x + tag_orient.rvec.z * ang_vel.z;
    ang_vel.x = along * tag_orient.rvec.x;
    ang_vel.y = tag_orient.rvec.y * along;
    ang_vel.z = tag_orient.rvec.z * along;
    havok_body_set_angular_velocity(rb, ang_vel);
    if (physics->components[TURRET_COMPONENT_TOP].is_veh_component)
    {
        vdata->m_components[physics->components[TURRET_COMPONENT_TOP].veh_component_idx].set_orient_pos_in_world_space(vp, new_orient, tag_pos);
    }
    return true;
}

// 0x00A45E30 (:2232)
void turret_physics_update_renderables(turret_physics* physics)
{
    for (int i = 0; i < NUM_TURRET_COMPONENTS; i++)
    {
        const turret_component_physics_info& tcpi = physics->components[i];
        if (!tcpi.is_veh_component && tcpi.smesh_instance != nullptr)
        {
            vector pos;
            matrix orient;
            havok_body_get_pos_orient(tcpi.havok_handle, pos, orient);
            tcpi.smesh_instance->set_position(pos);
            tcpi.smesh_instance->set_rotation(orient);
            game_render_scene_update(tcpi.smesh_instance);
        }
    }
}

// 0x00A21210 (:2248)
void turret_physics_update_objects(turret_physics* physics, turret* tp, const vehicle* vp)
{
    vector pos;
    matrix orient;
    if (vp == nullptr)
    {
        const unsigned int handle = physics->components[TURRET_COMPONENT_BASE].havok_handle;
        if (handle != 0xFFFFFFFF)
        {
            havok_body_get_pos_orient(handle, pos, orient);
            tp->update_pos_and_orient(pos, orient, false);
        }
    }
    else if (tp->t_info->flags.transform_to_tag)
    {
        pos = physics->last_local_tag_pos;
        orient = physics->last_local_tag_orient;
        vec_change_to_global_coords(pos, vp->pos, vp->orient);
        orient.unrotate(vp->orient);
        tp->update_pos_and_orient(pos, orient, false);
    }
    else
    {
        vp->get_tag_world_coords(physics->components[TURRET_COMPONENT_BASE].attach_tag_idx, pos, &orient, nullptr, nullptr);
        tp->update_pos_and_orient(pos, orient, false);
    }
    tp->adjust_child_transforms(false, false);
}

// 0x00A21370 (:2288): comp arrives in EAX, physics in ECX, angle in XMM0.
static void aim_pos_motor(turret_physics* physics, turret_components comp, float angle)
{
    hkpConstraintInstance* ci = physics->components[comp].hconstraint;
    if (ci != nullptr)
    {
        ((hkpLimitedHingeConstraintData*)ci->m_data)->m_atoms.m_angMotor.m_targetAngle = angle;
    }
}

// 0x00A21390 (:2295)
void turret_physics_aim_pos_motors(turret_physics* physics)
{
    aim_pos_motor(physics, TURRET_COMPONENT_MIDDLE, -physics->heading_sum);
    aim_pos_motor(physics, TURRET_COMPONENT_TOP, physics->pitch);
}

// 0x00A213D0 (:2302): comp arrives in EAX, physics in ECX. Wakes both bodies; the target decays by half a frame unless
// the new velocity is larger.
static void aim_vel_motor(turret_physics* physics, turret_components comp, float vel)
{
    turret_component_physics_info& tcpi = physics->components[comp];
    if (tcpi.vel_motor == nullptr)
    {
        return;
    }
    if (tcpi.havok_handle != 0xFFFFFFFF)
    {
        havok_body_force_activate(tcpi.havok_handle);
    }
    if (physics->components[comp - 1].havok_handle != 0xFFFFFFFF)
    {
        havok_body_force_activate(physics->components[comp - 1].havok_handle);
    }
    const float ramp = tcpi.vel_motor->m_velocityTarget * Vel_motor_ramp_out;
    tcpi.vel_motor->m_velocityTarget = fabsf(vel) < fabsf(ramp) ? ramp : vel;
}

// 0x00A21470 (:2323): on a vehicle the velocities are relative to the base's spin.
void turret_physics_aim_vel_motors(turret_physics* physics, float h_vel, float v_vel)
{
    if (physics->components[TURRET_COMPONENT_BASE].is_veh_component && physics->components[TURRET_COMPONENT_BASE].havok_handle != 0xFFFFFFFF)
    {
        vector pos;
        matrix orient;
        vector ang_vel;
        havok_body_get_pos_orient(physics->components[TURRET_COMPONENT_MIDDLE].havok_handle, pos, orient);
        havok_body_get_angular_velocity(physics->components[TURRET_COMPONENT_BASE].havok_handle, ang_vel);
        h_vel -= ang_vel.y * orient.uvec.y + orient.uvec.x * ang_vel.x + ang_vel.z * orient.uvec.z;
        v_vel -= orient.rvec.y * ang_vel.y + orient.rvec.x * ang_vel.x + orient.rvec.z * ang_vel.z;
    }
    aim_vel_motor(physics, TURRET_COMPONENT_MIDDLE, -h_vel);
    aim_vel_motor(physics, TURRET_COMPONENT_TOP, -v_vel);
}

// 0x00A216C0 (:2349): heading_sum counts whole turns so a full 360 turret's motor target never jumps; it follows the
// body when more than 0.2 rad from the last value, to the nearer of the two matching turn counts.
void turret_physics_update_heading(turret_physics* physics, turret* tp)
{
    matrix orient(IDENTITY_MATRIX);
    vector pos;
    havok_body_get_pos_orient(physics->components[TURRET_COMPONENT_MIDDLE].havok_handle, pos, orient);
    orient.rotate(tp->orient);
    float heading = 0.0f;
    orient.extract_heading(heading);
    if (fabsf(fl_angle_between_radians(heading, physics->heading)) < 0.2f)
    {
        return;
    }
    const float turns = (physics->heading_sum - heading) / 6.2831855f;
    // In double as the asm multiplies, by the float values of 2 and pi.
    const float below = (float)((double)(float)(int)floor((double)turns) * 2.0 * (double)3.1415927f + (double)heading);
    const float above = (float)((double)(float)(int)ceil((double)turns) * 2.0 * (double)3.1415927f + (double)heading);
    if (fabsf(below - physics->heading_sum) <= fabsf(above - physics->heading_sum))
    {
        physics->heading_sum = below;
    }
    else
    {
        physics->heading_sum = above;
    }
    physics->heading = heading;
}

// 0x00A218D0 (:2384)
void turret_physics_set_no_collide(turret_physics* physics, turret* tp, unsigned int object_handle)
{
    for (int i = 0; i < NUM_TURRET_COMPONENTS; i++)
    {
        if (!physics->components[i].is_veh_component)
        {
            set_body_no_collide(physics->components[i].havok_handle, object_handle);
        }
    }
}

// One hinge's motor (inlined twice in the original). Returns false for a constraint that is not a limited hinge.
static bool turret_physics_set_hinge_motor(turret_component_physics_info& tcpi, bool player_controlled)
{
    if (tcpi.hconstraint == nullptr)
    {
        return true;
    }
    havok_constraint_remove_motor(tcpi.hconstraint);
    hkpLimitedHingeConstraintData* hinge = (hkpLimitedHingeConstraintData*)tcpi.hconstraint->m_data;
    if (hinge->getType() != hkpConstraintData::CONSTRAINT_TYPE_LIMITEDHINGE)
    {
        return false;
    }
    if (player_controlled)
    {
        hinge->setMotor(tcpi.vel_motor);
    }
    else
    {
        hinge->setMotor(tcpi.pos_motor);
    }
    hinge->setMotorEnabled(tcpi.hconstraint->m_internal->m_runtime, true);
    return true;
}

// 0x00A21940 (:2393): velocity motors under a player's aim, position motors otherwise.
void turret_physics_set_control_type(turret_physics* physics, bool is_player, bool is_player_aimed)
{
    const bool player_controlled = is_player && is_player_aimed;
    if (!turret_physics_set_hinge_motor(physics->components[TURRET_COMPONENT_TOP], player_controlled)
        || !turret_physics_set_hinge_motor(physics->components[TURRET_COMPONENT_MIDDLE], player_controlled))
    {
        return;
    }
    if (player_controlled)
    {
        turret_physics_aim_vel_motors(physics, 0.0f, 0.0f);
    }
    else
    {
        turret_physics_aim_pos_motors(physics);
    }
}
