// Original: volition/rfg/code/vehicle/havok/hk_custom_vehicle_raycast_wheel_collide.cpp
//
// Wheel collision: rays, or sphere or cylinder linear casts through a phantom per wheel. The ray results are kept for a
// fake ray that reuses them (nothing in rfg.exe selects it: get_wheel_cast_type returns the cast mode or a real ray).

#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_raycast_wheel_collide.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkplinearcastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldraycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/cylinder/hkpcylindershape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/sphere/hkpsphereshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/triangle/hkptriangleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpaabbphantom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpsimpleshapephantom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicledata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/suspension/hkpvehiclesuspension.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_instance.h"
#include "volition/vlib/code/math/floating.h"

static vehicle_wheel_cast_type vehicle_wheel_cast_default_mode = VEHICLE_WHEEL_CAST_SPHERE; // 0x016BA498

// An infinite mass vehicle only rests on zones and on rfg movers without gm_flags. 0xF8: the first field past
// object_mover (gm_flags); the type check has already said this is an rfg_mover.
static bool wheel_hit_allowed_for_infinite_mass(const hkpWorldObject* wp)
{
    if (wp == NULL)
    {
        return true;
    }
    const object* obj = object_from_world_obj(wp);
    if (obj->get_obj_type() == OT_ZONE)
    {
        return true;
    }
    return obj->get_obj_type() == OT_MOVER && obj->get_sub_type() == OT_SUB_MOVER_RFG && *reinterpret_cast<const int*>(reinterpret_cast<const char*>(obj) + 0xF8) == 0;
}

// The world object of a root collidable, or null if it is not an entity. 1: hkpWorldObject::BROAD_PHASE_ENTITY.
static const hkpWorldObject* wheel_root_world_object(const hkpCollidable* root)
{
    return root->getType() == 1 ? static_cast<const hkpWorldObject*>(root->getOwner()) : NULL;
}

// 0x00B74BB0
havok_closest_wheel_ray_hit_collector::havok_closest_wheel_ray_hit_collector()
{
}

// 0x00B74BE0
havok_closest_wheel_ray_hit_collector::~havok_closest_wheel_ray_hit_collector()
{
}

// 0x00B74AB0 (:115)
void havok_closest_wheel_ray_hit_collector::addRayHit(const hkpCdBody& cd_body, const hkpShapeRayCastCollectorOutput& hitInfo)
{
    const hkpCollidable* collidable = cd_body.getRootCollidable();
    // Original bug kept: a null owner_vehicle_instance passes the first test and is then dereferenced.
    if (owner_vehicle_instance != NULL)
    {
        const hkpCollidable* chassis = owner_vehicle_instance->m_entity->getCollidable();
        if (collidable == chassis || collidable->getCollisionFilterInfo() == chassis->getCollisionFilterInfo())
        {
            return;
        }
    }
    const hkpWorldObject* wp = wheel_root_world_object(collidable);
    if (wp == NULL)
    {
        return;
    }
    if (owner_vehicle_instance->get_is_infinite_mass() && !wheel_hit_allowed_for_infinite_mass(wp))
    {
        return;
    }
    // -0.5: only hits facing the cast
    if (cast_direction.dot<3>(hitInfo.m_normal).getReal() <= -0.5f)
    {
        havok_closest_ray_hit_collector::addRayHit(cd_body, hitInfo);
    }
}

// 0x00B960F0 (:172)
void havok_closest_wheel_ray_hit_collector::set_cast_direction(const hkVector4f& dir)
{
    cast_direction = dir;
    cast_direction.normalizeIfNotZero<3>();
}

// 0x00B63DA0 (:179)
void havok_closest_wheel_ray_hit_collector::set_owner_vehicle(const hkpVehicleInstance* vehicle)
{
    owner_vehicle_instance = static_cast<const hk_custom_vehicle_instance*>(vehicle);
}

// 0x00B74BF0
havok_closest_linear_wheel_cast_collector::havok_closest_linear_wheel_cast_collector()
{
}

// 0x00B63FD0
havok_closest_linear_wheel_cast_collector::~havok_closest_linear_wheel_cast_collector()
{
}

// 0x00B9D3E0 (:209)
void havok_closest_linear_wheel_cast_collector::addCdPoint(const hkpCdPoint& point)
{
    const hkpCollidable* collidable = point.m_cdBodyB.getRootCollidable();
    // Original bug kept: a null owner_vehicle_instance passes the first test and is then dereferenced.
    if (owner_vehicle_instance != NULL)
    {
        const hkpCollidable* chassis = owner_vehicle_instance->m_entity->getCollidable();
        if (collidable == chassis || collidable->getCollisionFilterInfo() == chassis->getCollisionFilterInfo())
        {
            return;
        }
    }
    const hkpWorldObject* wp = wheel_root_world_object(collidable);
    if (wp == NULL)
    {
        return;
    }
    if (owner_vehicle_instance->get_is_infinite_mass() && !wheel_hit_allowed_for_infinite_mass(wp))
    {
        return;
    }

    hkContactPoint contact = point.m_contact;
    hkVector4f unwelded = point.m_unweldedNormal;
    // 2: hkcdShapeType::TRIANGLE. On a triangle the face normal replaces the cast's normal.
    if (point.m_cdBodyB.getShape()->m_type.m_storage == 2)
    {
        const hkpTriangleShape* triangle = static_cast<const hkpTriangleShape*>(point.m_cdBodyB.getShape());
        hkVector4f edge1;
        edge1.setSub(triangle->getVertex(1), triangle->getVertex(0));
        hkVector4f edge2;
        edge2.setSub(triangle->getVertex(2), triangle->getVertex(0));
        hkVector4f face;
        face.setCross(edge1, edge2);
        face.normalize<3>();
        hkVector4f normal;
        normal.setRotatedDir(point.m_cdBodyB.getRootCollidable()->getTransform().getRotation(), face);
        if (normal.dot<3>(point.m_contact.getNormal()).getReal() < 0.0f)
        {
            normal.setMul(hkSimdFloat32::fromFloat(-1.0f), normal);
        }
        contact.setSeparatingNormal(normal, point.m_contact.getDistance());
        unwelded = contact.getSeparatingNormal();
    }

    // -0.5: only hits facing the cast
    if (cast_direction.dot<3>(contact.getNormal()).getReal() <= -0.5f)
    {
        hkpCdPoint adjusted(point.m_cdBodyA, point.m_cdBodyB, contact);
        adjusted.setUnweldedNormal(unwelded);
        hkpClosestCdPointCollector::addCdPoint(adjusted);
    }
}

// 0x00B96170 (:295)
void havok_closest_linear_wheel_cast_collector::set_cast_direction(const hkVector4f& dir)
{
    cast_direction = dir;
    cast_direction.normalizeIfNotZero<3>();
}

// 0x00B63DE0 (:302)
void havok_closest_linear_wheel_cast_collector::set_owner_vehicle(const hkpVehicleInstance* vehicle)
{
    owner_vehicle_instance = static_cast<const hk_custom_vehicle_instance*>(vehicle);
}

// 0x00BAFCE0 (:321)
hk_custom_vehicle_raycast_wheel_collide::hk_custom_vehicle_raycast_wheel_collide(unsigned int num_wheels)
{
    m_wheel_index_to_cast = 0;
    m_used_raycasts_last_time = false;
    bbox_offset = 0.0f;
    ray_override = true;
    npc_ray_override = false;
    m_wheel_raycast_results.setSize(num_wheels);
    for (int w = 0; w < m_wheel_raycast_results.getSize(); w++)
    {
        m_wheel_raycast_results[w].reset();
    }
    // A quarter of the wheels each step, at least one.
    const unsigned int per_step = fl_uround((float)num_wheels * 0.25f);
    m_num_wheels_to_cast_per_step = per_step < 2 ? 1 : per_step;
    m_wheel_can_use_fake_raycast.init(m_wheel_can_use_fake_raycast_buffer, num_wheels);
    m_wheel_can_use_fake_raycast.clear(false);
}

// 0x00BAFF90 (:350)
hk_custom_vehicle_raycast_wheel_collide::~hk_custom_vehicle_raycast_wheel_collide()
{
    for (int w = 0; w < wheel_phantoms.getSize(); w++)
    {
        if (wheel_phantoms[w] != NULL)
        {
            wheel_phantoms[w]->removeReference();
            wheel_phantoms[w] = NULL;
        }
    }
}

// 0x00B63DF0 (:363): steep ground grips less
void hk_custom_vehicle_raycast_wheel_collide::calcSingleWheelGroundFriction(const hkpVehicleInstance* vehicle, signed char wheelInfoNum, const hkpWorldRayCastOutput& worldRayCastOutput, float& frictionOut) const
{
    float up = worldRayCastOutput.m_normal(1);
    if (0.0f <= 0.0f - up)
    {
        up = 0.0f;
    }
    // 0.75, 0.25: a quarter of the friction on a wall
    frictionOut = (up * 0.75f + 0.25f) * frictionOut;
}

// 0x00B961F0 (:372)
void hk_custom_vehicle_raycast_wheel_collide::init(const hkpVehicleInstance* vehicle)
{
    hkpVehicleRayCastWheelCollide::init(vehicle);
    const_cast<hk_custom_vehicle_instance*>(static_cast<const hk_custom_vehicle_instance*>(vehicle))->set_is_trying_to_recover(false);
    bbox_offset = 0.0f;
    cast_mode = vehicle_wheel_cast_default_mode;
    const int num_wheels = vehicle->m_data->m_numWheels;

    hkTransformf identity;
    identity.setIdentity();
    if (cast_mode == VEHICLE_WHEEL_CAST_SPHERE)
    {
        for (int w = 0; w < num_wheels; w++)
        {
            const float radius = vehicle->m_data->m_wheelParams[w].m_radius;
            hkpSphereShape* shape = new hkpSphereShape(radius);
            hkpSimpleShapePhantom* phantom = new hkpSimpleShapePhantom(shape, identity, m_wheelCollisionFilterInfo);
            shape->removeReference();
            wheel_phantoms.pushBack(phantom);
            if (bbox_offset - radius < 0.0f)
            {
                bbox_offset = radius;
            }
        }
        return;
    }
    if (cast_mode == VEHICLE_WHEEL_CAST_CYLINDER)
    {
        // 0.09375: the convex radius
        for (int w = 0; w < num_wheels; w++)
        {
            const float radius = vehicle->m_data->m_wheelParams[w].m_radius;
            const float width = vehicle->m_data->m_wheelParams[w].m_width;
            // 0.4675: a little under half the width; 0.04: the thinnest half
            float half = width * 0.4675f - 0.09375f;
            if (half - 0.04f < 0.0f)
            {
                half = 0.04f;
            }
            const hkVector4f& axis = vehicle->m_wheelsInfo[w].m_spinAxisChassisSpace;
            hkVector4f vertex_a;
            vertex_a.setMul(hkSimdFloat32::fromFloat(-half), axis);
            hkVector4f vertex_b;
            vertex_b.setMul(hkSimdFloat32::fromFloat(half), axis);
            hkpCylinderShape* shape = new hkpCylinderShape(vertex_a, vertex_b, radius - 0.09375f, 0.09375f);
            hkpSimpleShapePhantom* phantom = new hkpSimpleShapePhantom(shape, identity, m_wheelCollisionFilterInfo);
            shape->removeReference();
            wheel_phantoms.pushBack(phantom);
            if (bbox_offset - width * 0.5f < 0.0f)
            {
                bbox_offset = width * 0.5f;
            }
            if (bbox_offset - radius < 0.0f)
            {
                bbox_offset = radius;
            }
        }
        return;
    }

    cast_mode = VEHICLE_WHEEL_CAST_RAY;
    for (int w = 0; w < num_wheels; w++)
    {
        wheel_phantoms.pushBack(NULL);
    }
}

// 0x00B8A390 (:461)
void hk_custom_vehicle_raycast_wheel_collide::collideWheels(const float deltaTime, const hkpVehicleInstance* vehicle, hkpVehicleWheelCollide::CollisionDetectionWheelOutput* cdInfoOut)
{
    hk_custom_vehicle_instance* instance = const_cast<hk_custom_vehicle_instance*>(static_cast<const hk_custom_vehicle_instance*>(vehicle));
    if (instance->get_is_infinite_mass())
    {
        ray_override = false;
    }
    updateBeforeCollisionDetection(vehicle);

    for (signed char w = 0; w < vehicle->m_data->m_numWheels; w++)
    {
        const hkpVehicleInstance::WheelInfo& wheel = vehicle->m_wheelsInfo[w];
        hkpVehicleWheelCollide::CollisionDetectionWheelOutput& cd = cdInfoOut[w];
        hkpWorldRayCastOutput output;
        castSingleWheel(wheel, vehicle, output);
        const float length = vehicle->m_suspension->m_wheelParams[w].m_length;
        const float radius = vehicle->m_data->m_wheelParams[w].m_radius;

        if (output.m_rootCollidable == NULL)
        {
            cd.m_currentSuspensionLength = length;
            cd.m_contactBody = NULL;
            cd.m_suspensionClosingSpeed = 0.0f;
            cd.m_contactPoint.setPosition(wheel.m_rayEndPointWs);
            hkVector4f normal;
            normal.setMul(hkSimdFloat32::fromFloat(-1.0f), wheel.m_suspensionDirectionWs);
            cd.m_contactPoint.setSeparatingNormal(normal, cd.m_contactPoint.getDistance());
            cd.m_contactFriction = 0.0f;
            cd.m_suspensionScalingFactor = 1.0f;
            // 0xFF: no material
            instance->set_wheel_contact_materials(w, 0xFF, 0xFF);
            continue;
        }

        cd.m_contactPoint.setSeparatingNormal(output.m_normal, cd.m_contactPoint.getDistance());
        for (int k = 0; k < hkpVehicleWheelCollide::CollisionDetectionWheelOutput::MAX_NUM_SHAPE_KEYS; k++)
        {
            cd.m_contactShapeKey[k] = 0xFFFFFFFF;
        }
        // Original quirk kept: every key is written to index 0, so the last key of the hierarchy is what stays.
        for (int k = 0; k < 8; k++)
        {
            if (output.m_shapeKeys[k] == 0xFFFFFFFF)
            {
                break;
            }
            cd.m_contactShapeKey[0] = output.m_shapeKeys[k];
        }

        hkpRigidBody* body = hkpGetRigidBody(output.m_rootCollidable);
        cd.m_contactBody = body;
        const float hit_distance = (radius + length) * output.m_hitFraction;
        cd.m_currentSuspensionLength = hit_distance - radius;
        hkVector4f position;
        position.setAddMul(wheel.m_hardPointWs, wheel.m_suspensionDirectionWs, hkSimdFloat32::fromFloat(hit_distance));
        // 0.9: a linear cast set a contact point (and no contact sits at the world origin)
        if (0.9f < actual_contact_point.lengthSquared<3>().getReal())
        {
            position = actual_contact_point;
        }
        cd.m_contactPoint.setPosition(position);

        float friction = body->m_material.m_friction;
        calcSingleWheelGroundFriction(vehicle, w, output, friction);
        cd.m_contactFriction = friction;

        const hkpRigidBody* chassis = vehicle->getChassis();
        hkVector4f r;
        r.setSub(position, chassis->getCenterOfMassInWorld());
        hkVector4f velocity;
        velocity.setCross(chassis->getAngularVelocity(), r);
        velocity.add(chassis->getLinearVelocity());
        cd.m_suspensionScalingFactor = 1.0f;
        cd.m_suspensionClosingSpeed = 0.0f - velocity.dot<3>(output.m_normal).getReal();

        const hkpWorldObject* wp = wheel_root_world_object(output.m_rootCollidable);
        object* obj = object_from_world_obj(wp);
        if (obj != NULL)
        {
            vector game_position;
            havok_to_game_vector(game_position, position);
            const unsigned char physical = obj->get_physical_material(output.m_shapeKeys[0], &game_position, wp);
            const unsigned char effect = obj->get_effect_material(output.m_shapeKeys[0], &game_position, wp);
            instance->set_wheel_contact_materials(w, physical, effect);
        }
    }

    m_wheel_index_to_cast = (m_wheel_index_to_cast + m_num_wheels_to_cast_per_step) % (unsigned int)(int)vehicle->m_data->m_numWheels;
}

// 0x00B63E40 (:710)
void hk_custom_vehicle_raycast_wheel_collide::calcWheelsAABB(const hkpVehicleInstance* vehicle, hkAabb& aabbOut) const
{
    hkpVehicleRayCastWheelCollide::calcWheelsAABB(vehicle, aabbOut);
    aabbOut.m_min(0) = aabbOut.m_min(0) - bbox_offset;
    aabbOut.m_min(1) = aabbOut.m_min(1) - bbox_offset;
    aabbOut.m_min(2) = aabbOut.m_min(2) - bbox_offset;
    aabbOut.m_max(0) = bbox_offset + aabbOut.m_max(0);
    aabbOut.m_max(1) = bbox_offset + aabbOut.m_max(1);
    aabbOut.m_max(2) = bbox_offset + aabbOut.m_max(2);

    // The eight corners of the extra limits (the treads).
    hkVector4f min;
    hkVector4f max;
    const_cast<hk_custom_vehicle_instance*>(static_cast<const hk_custom_vehicle_instance*>(vehicle))->get_extra_aabb_limits(min, max);
    const hkTransformf& transform = vehicle->getChassis()->getTransform();
    for (int corner = 0; corner < 8; corner++)
    {
        hkVector4f local;
        local.set((corner & 1) != 0 ? min(0) : max(0), (corner & 2) != 0 ? min(1) : max(1), (corner & 4) != 0 ? min(2) : max(2), 0.0f);
        hkVector4f world;
        world.setTransformedPos(transform, local);
        aabbOut.m_min.setMin(aabbOut.m_min, world);
        aabbOut.m_max.setMax(aabbOut.m_max, world);
    }
}

// 0x00BAFF40 (.h:50): only the other overload is used
void hk_custom_vehicle_raycast_wheel_collide::castSingleWheel(const hkpVehicleInstance::WheelInfo& wheelInfo, hkpWorldRayCastOutput& output) const
{
}

// 0x00BA7C80 (:753)
void hk_custom_vehicle_raycast_wheel_collide::castSingleWheel(const hkpVehicleInstance::WheelInfo& wheel_info, const hkpVehicleInstance* vehicle, hkpWorldRayCastOutput& rc_output)
{
    const unsigned int wheel_index = (unsigned int)(&wheel_info - vehicle->m_wheelsInfo.begin());
    actual_contact_point.setZero();
    const vehicle_wheel_cast_type mode = get_wheel_cast_type(wheel_index, vehicle);
    switch (mode)
    {
    case VEHICLE_WHEEL_CAST_RAY:
    {
        hkpWorldRayCastInput input;
        input.m_from = wheel_info.m_hardPointWs;
        input.m_to = wheel_info.m_rayEndPointWs;
        input.m_enableShapeCollectionFilter = true;
        input.m_filterInfo = m_wheelCollisionFilterInfo;
        input.m_userData = 0;
        havok_closest_wheel_ray_hit_collector collector;
        collector.set_owner_vehicle(vehicle);
        collector.set_cast_direction(wheel_info.m_suspensionDirectionWs);
        m_phantom->castRay(input, collector);
        raycast_process_results(rc_output, collector, wheel_index);
        break;
    }
    case VEHICLE_WHEEL_CAST_FAKE_RAY:
        fake_raycast_process_results(rc_output, wheel_index, wheel_info);
        break;
    case VEHICLE_WHEEL_CAST_SPHERE:
    case VEHICLE_WHEEL_CAST_CYLINDER:
    {
        const float radius = vehicle->m_data->m_wheelParams[wheel_index].m_radius;
        hkVector4f position;
        hkQuaternionf rotation;
        calcHk550CurrentPositionAndRotation(vehicle, vehicle->getChassis(), vehicle->m_suspension, wheel_index, position, rotation);
        hkTransformf start;
        start.getRotation().set(rotation);
        start.setTranslation(wheel_info.m_hardPointWs);
        hkpShapePhantom* phantom = wheel_phantoms[wheel_index];
        phantom->setTransform(start);

        hkpLinearCastInput input;
        input.m_to.setSubMul(wheel_info.m_rayEndPointWs, wheel_info.m_suspensionDirectionWs, hkSimdFloat32::fromFloat(radius));
        // 0.05: the allowed penetration; 2^-23: the start tolerance
        input.m_maxExtraPenetration = 0.05f;
        input.m_startPointTolerance = 1.1920929e-07f;
        havok_closest_linear_wheel_cast_collector collector;
        collector.set_owner_vehicle(vehicle);
        collector.set_cast_direction(wheel_info.m_suspensionDirectionWs);
        m_phantom->linearCast(phantom->getCollidable(), input, collector, NULL);
        linearcast_process_results(rc_output, collector, vehicle, wheel_index);
        break;
    }
    }
    // rfg.exe logs "Ray hit NULL root collidable or NULL owner" here for a hit without an owner.
}

// 0x00B63FE0 (:876)
void hk_custom_vehicle_raycast_wheel_collide::setup_for_player()
{
    ray_override = false;
}

// 0x00B63FF0 (:882)
void hk_custom_vehicle_raycast_wheel_collide::setup_for_nonplayer()
{
    ray_override = true;
    if (npc_ray_override)
    {
        ray_override = false;
    }
}

// 0x00B64000 (:893)
void hk_custom_vehicle_raycast_wheel_collide::set_npc_ray_override(bool ride)
{
    npc_ray_override = ride;
}

// 0x00B74CD0 (:899)
void hk_custom_vehicle_raycast_wheel_collide::clear_wheel_caches()
{
    for (int w = 0; w < m_wheel_raycast_results.getSize(); w++)
    {
        m_wheel_raycast_results[w].reset();
    }
}

// 0x00B8A750 (:918)
vehicle_wheel_cast_type hk_custom_vehicle_raycast_wheel_collide::get_wheel_cast_type(unsigned int wheel_index, const hkpVehicleInstance* hk_vehicle) const
{
    vehicle_wheel_cast_type mode = cast_mode;
    if (ray_override || wheel_phantoms[wheel_index] == NULL)
    {
        mode = VEHICLE_WHEEL_CAST_RAY;
    }
    hk_custom_vehicle_raycast_wheel_collide* self = const_cast<hk_custom_vehicle_raycast_wheel_collide*>(this);
    if (!m_used_raycasts_last_time && mode == VEHICLE_WHEEL_CAST_RAY)
    {
        self->m_wheel_can_use_fake_raycast.clear(false);
    }
    self->m_used_raycasts_last_time = mode == VEHICLE_WHEEL_CAST_RAY;
    return mode;
}

// 0x00B64010 (:974)
bool hk_custom_vehicle_raycast_wheel_collide::should_raycast_this_wheel(unsigned int wheel_index, unsigned int num_wheels) const
{
    return true;
}

// 0x00B8A7A0 (:1018)
void hk_custom_vehicle_raycast_wheel_collide::raycast_process_results(hkpWorldRayCastOutput& rc_output, const havok_closest_wheel_ray_hit_collector& ray_collector, unsigned int wheel_index)
{
    rc_output = ray_collector.m_rayHit;
    m_wheel_can_use_fake_raycast.set(wheel_index, 0);
    if (rc_output.m_rootCollidable != NULL)
    {
        const hkpWorldObject* wp = wheel_root_world_object(rc_output.m_rootCollidable);
        if (wp != NULL)
        {
            const object* obj = object_from_world_obj(wp);
            if (obj != NULL && obj->get_obj_type() == OT_ZONE)
            {
                m_wheel_can_use_fake_raycast.set(wheel_index, 1);
            }
        }
    }
    m_wheel_raycast_results[wheel_index] = rc_output;
}

// 0x00B74D20 (:1053): the last ray's result, with the hit fraction of the current contact along the new ray
void hk_custom_vehicle_raycast_wheel_collide::fake_raycast_process_results(hkpWorldRayCastOutput& rc_output, unsigned int wheel_index, const hkpVehicleInstance::WheelInfo& wheel_info)
{
    rc_output = m_wheel_raycast_results[wheel_index];
    const hkVector4f& normal = wheel_info.m_contactPoint.getNormal();
    hkVector4f to_contact;
    to_contact.setSub(wheel_info.m_contactPoint.getPosition(), wheel_info.m_hardPointWs);
    hkVector4f to_end;
    to_end.setSub(wheel_info.m_rayEndPointWs, wheel_info.m_hardPointWs);
    const float end = to_end.dot<3>(normal).getReal();
    float fraction = 0.0f;
    if (end != 0.0f)
    {
        hkSimdFloat32 inv;
        inv.setReciprocal(hkSimdFloat32::fromFloat(end));
        fraction = inv.getReal() * to_contact.dot<3>(normal).getReal();
    }
    fraction = 0.0f < fraction ? fraction : 0.0f;
    rc_output.m_hitFraction = fraction < 1.0f ? fraction : 1.0f;
}

// 0x00B9D630 (:1108)
void hk_custom_vehicle_raycast_wheel_collide::linearcast_process_results(hkpWorldRayCastOutput& rc_output, const havok_closest_linear_wheel_cast_collector& linear_collector, const hkpVehicleInstance* hk_vehicle, unsigned int wheel_index)
{
    const hkpRootCdPoint& hit = linear_collector.m_hitPoint;
    if (hit.m_rootCollidableA == NULL)
    {
        rc_output.reset();
        return;
    }

    const hkpVehicleInstance::WheelInfo& wheel = hk_vehicle->m_wheelsInfo[wheel_index];
    rc_output.m_rootCollidable = hit.m_rootCollidableB;
    rc_output.m_shapeKeys[0] = hit.m_shapeKeyB;
    rc_output.m_normal = hit.m_contact.getSeparatingNormal();

    // 0.01: pushed along the suspension, past the surface
    actual_contact_point.setAddMul(hit.m_contact.getPosition(), wheel.m_suspensionDirectionWs, hkSimdFloat32::fromFloat(0.01f));

    // A normal pointing along the cast is bent to face it. 0.001: just past perpendicular.
    float along = rc_output.m_normal.dot<3>(wheel.m_suspensionDirectionWs).getReal();
    if (0.0f <= along)
    {
        along = along + 0.001f;
        rc_output.m_normal.subMul(hkSimdFloat32::fromFloat(along), wheel.m_suspensionDirectionWs);
        rc_output.m_normal.normalize<3>();
    }

    const float radius = hk_vehicle->m_data->m_wheelParams[wheel_index].m_radius;
    const float length = hk_vehicle->m_suspension->m_wheelParams[wheel_index].m_length;
    const float total = length + radius;
    if (0.0f < total)
    {
        float fraction = (hit.m_contact.getDistance() * length + radius + 0.01f) / total;
        if (fraction - 0.0f < 0.0f)
        {
            fraction = 0.0f;
        }
        if (0.0f <= fraction - 1.0f)
        {
            fraction = 1.0f;
        }
        rc_output.m_hitFraction = fraction;
    }
    else
    {
        rc_output.m_hitFraction = 0.0f;
    }
}

// 0x00B74E40 (:1158): chassis * steering * spin, without the chassis frame hkpVehicleInstance's version has
void calcHk550CurrentPositionAndRotation(const hkpVehicleInstance* pInstance, const hkpRigidBody* chassis, const hkpVehicleSuspension* suspension, int wheelNo, hkVector4f& posOut, hkQuaternionf& rotOut)
{
    const hkpVehicleInstance::WheelInfo& wheel = pInstance->m_wheelsInfo[wheelNo];
    hkQuaternionf spin;
    spin.setAxisAngle(wheel.m_spinAxisChassisSpace, -wheel.m_spinAngle);
    hkQuaternionf steered;
    steered.setMul(chassis->getRotation(), wheel.m_steeringOrientationChassisSpace);
    rotOut.setMul(steered, spin);

    const float length = 0.0f <= wheel.m_currentSuspensionLength ? wheel.m_currentSuspensionLength : 0.0f;
    posOut._setTransformedPos(chassis->getTransform(), suspension->m_wheelParams[wheelNo].m_hardpointChassisSpace);
    posOut.addMul(hkSimdFloat32::fromFloat(length), wheel.m_suspensionDirectionWs);
}
