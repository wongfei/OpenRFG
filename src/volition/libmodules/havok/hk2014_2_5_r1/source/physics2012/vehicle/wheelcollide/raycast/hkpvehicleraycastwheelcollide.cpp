// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/wheelcollide/raycast/hkpvehicleraycastwheelcollide.cpp
//
// Havok 2014.2.5 (stub_impl.md S8): one ray per wheel, cast through an AABB phantom around all wheels, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/wheelcollide/raycast/hkpvehicleraycastwheelcollide.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldraycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldraycastoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpaabbphantom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicledata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/suspension/hkpvehiclesuspension.h"

// 0x00F7F270 (hkpvehiclewheelcollide.h:128)
hkpVehicleWheelCollide::hkpVehicleWheelCollide()
{
}

// 0x00F7F210 (hkpvehiclewheelcollide.h:104)
hkpVehicleWheelCollide::~hkpVehicleWheelCollide()
{
}

// 0x00BAFF30 (hkpvehiclewheelcollide.h:101)
void hkpVehicleWheelCollide::wheelCollideCallback(const hkpVehicleInstance* vehicle, unsigned char wheelIndex, CollisionDetectionWheelOutput& cdInfo)
{
}

// 0x00F7EE20
hkpVehicleRayCastWheelCollide::hkpVehicleRayCastWheelCollide()
{
    m_wheelCollisionFilterInfo = 0;
    m_phantom = NULL;
    m_alreadyUsed = false;
    m_type = RAY_CAST_WHEEL_COLLIDE;
}

// 0x00F7EE60
void hkpVehicleRayCastWheelCollide::init(const hkpVehicleInstance* vehicle)
{
    hkAabb aabb;
    calcWheelsAABB(vehicle, aabb);
    m_phantom = new hkpAabbPhantom(aabb, m_wheelCollisionFilterInfo);
    m_rejectRayChassisListener.m_chassis = vehicle->getChassis()->getCollidable();
    m_phantom->addPhantomOverlapListener(&m_rejectRayChassisListener);
}

// 0x00F7F1C0
void hkpVehicleRayCastWheelCollide::getPhantoms(hkArray<hkpPhantom*,hkContainerHeapAllocator>& phantomsOut)
{
    phantomsOut.pushBack(m_phantom);
}

// 0x00F7EEF0: the old listener is removed from the new phantom, which is a clone of the old one
hkpVehicleWheelCollide* hkpVehicleRayCastWheelCollide::clone(const hkpRigidBody& newChassis, const hkArray<hkpPhantom*,hkContainerHeapAllocator>& newPhantoms) const
{
    hkpVehicleRayCastWheelCollide* collide = new hkpVehicleRayCastWheelCollide();
    collide->m_phantom = static_cast<hkpAabbPhantom*>(newPhantoms[0]);
    collide->m_phantom->addReference();
    collide->m_phantom->removePhantomOverlapListener(const_cast<hkpRejectChassisListener*>(&m_rejectRayChassisListener));
    collide->m_phantom->addPhantomOverlapListener(&collide->m_rejectRayChassisListener);
    collide->m_rejectRayChassisListener.m_chassis = newChassis.getCollidable();
    collide->m_wheelCollisionFilterInfo = m_wheelCollisionFilterInfo;
    return collide;
}

// 0x00F7EA20
hkpVehicleRayCastWheelCollide::~hkpVehicleRayCastWheelCollide()
{
    if (m_phantom != NULL)
    {
        if (m_rejectRayChassisListener.m_chassis != NULL)
        {
            m_phantom->removePhantomOverlapListener(&m_rejectRayChassisListener);
        }
        m_phantom->removeReference();
    }
}

// 0x00F7EA70
void hkpVehicleRayCastWheelCollide::calcWheelsAABB(const hkpVehicleInstance* vehicle, hkAabb& aabbOut) const
{
    aabbOut.m_min.setMin(vehicle->m_wheelsInfo[0].m_hardPointWs, vehicle->m_wheelsInfo[0].m_rayEndPointWs);
    aabbOut.m_max.setMax(vehicle->m_wheelsInfo[0].m_hardPointWs, vehicle->m_wheelsInfo[0].m_rayEndPointWs);
    for (int w = 1; w < vehicle->m_data->m_numWheels; w++)
    {
        const hkpVehicleInstance::WheelInfo& wheel = vehicle->m_wheelsInfo[w];
        aabbOut.m_min.setMin(aabbOut.m_min, wheel.m_rayEndPointWs);
        aabbOut.m_min.setMin(aabbOut.m_min, wheel.m_hardPointWs);
        aabbOut.m_max.setMax(aabbOut.m_max, wheel.m_rayEndPointWs);
        aabbOut.m_max.setMax(aabbOut.m_max, wheel.m_hardPointWs);
    }
}

// 0x00F7EB00
void hkpVehicleRayCastWheelCollide::updateBeforeCollisionDetection(const hkpVehicleInstance* vehicle)
{
    hkAabb aabb;
    calcWheelsAABB(vehicle, aabb);
    m_phantom->setAabb(aabb);
}

// 0x00F7EB40
void hkpVehicleRayCastWheelCollide::castSingleWheel(const hkpVehicleInstance::WheelInfo& wheelInfo, hkpWorldRayCastOutput& output) const
{
    hkpWorldRayCastInput input;
    input.m_from = wheelInfo.m_hardPointWs;
    input.m_to = wheelInfo.m_rayEndPointWs;
    input.m_enableShapeCollectionFilter = true;
    input.m_filterInfo = m_wheelCollisionFilterInfo;
    input.m_userData = 0;
    m_phantom->castRay(input, output);
}

// 0x00F7EB90
void hkpVehicleRayCastWheelCollide::collideWheels(const float deltaTime, const hkpVehicleInstance* vehicle, hkpVehicleWheelCollide::CollisionDetectionWheelOutput* cdInfoOut)
{
    const unsigned char num_wheels = vehicle->m_data->m_numWheels;
    for (unsigned char w = 0; w < num_wheels; w++)
    {
        hkpWorldRayCastOutput output;
        castSingleWheel(vehicle->m_wheelsInfo[w], output);
        if (output.hasHit())
        {
            getCollisionOutputFromCastResult(vehicle, w, output, cdInfoOut[w]);
        }
        else
        {
            getCollisionOutputWithoutHit(vehicle, w, cdInfoOut[w]);
        }
        wheelCollideCallback(vehicle, w, cdInfoOut[w]);
    }
}

// 0x00F7EFA0: a hit on anything but an entity dereferences the null contact body, as in rfg.exe
void hkpVehicleRayCastWheelCollide::getCollisionOutputFromCastResult(const hkpVehicleInstance* vehicle, unsigned char wheelInfoNum, const hkpWorldRayCastOutput& rayCastOutput, hkpVehicleWheelCollide::CollisionDetectionWheelOutput& cdInfo) const
{
    const hkpVehicleInstance::WheelInfo& wheel = vehicle->m_wheelsInfo[wheelInfoNum];
    const float radius = vehicle->m_data->m_wheelParams[wheelInfoNum].m_radius;
    const float length = vehicle->m_suspension->m_wheelParams[wheelInfoNum].m_length;

    for (int k = 0; k < CollisionDetectionWheelOutput::MAX_NUM_SHAPE_KEYS; k++)
    {
        cdInfo.m_contactShapeKey[k] = rayCastOutput.m_shapeKeys[k];
    }
    hkpRigidBody* body = hkpGetRigidBody(rayCastOutput.m_rootCollidable);
    cdInfo.m_contactBody = body;

    const float hit_distance = (radius + length) * rayCastOutput.m_hitFraction;
    const float suspension_length = hit_distance - radius;
    cdInfo.m_currentSuspensionLength = suspension_length;
    hkVector4f position;
    position.setAddMul(wheel.m_hardPointWs, wheel.m_suspensionDirectionWs, hkSimdFloat32::fromFloat(hit_distance));
    cdInfo.m_contactPoint.setPosition(position);
    const hkVector4f& normal = rayCastOutput.m_normal;
    cdInfo.m_contactPoint.setSeparatingNormal(normal, suspension_length);
    cdInfo.m_contactFriction = body->m_material.m_friction;

    const float dot = normal.dot<3>(wheel.m_suspensionDirectionWs).getReal();
    if (dot < 0.0f - vehicle->m_data->m_normalClippingAngleCos)
    {
        // The closing speed of the contact points along the normal, projected on the suspension direction.
        const hkpRigidBody* chassis = vehicle->getChassis();
        hkVector4f r;
        r.setSub(position, chassis->getCenterOfMassInWorld());
        hkVector4f chassis_velocity;
        chassis_velocity.setCross(chassis->getAngularVelocity(), r);
        chassis_velocity.add(chassis->getLinearVelocity());
        r.setSub(position, body->getCenterOfMassInWorld());
        hkVector4f body_velocity;
        body_velocity.setCross(body->getAngularVelocity(), r);
        body_velocity.add(body->getLinearVelocity());
        hkVector4f relative;
        relative.setSub(chassis_velocity, body_velocity);

        hkSimdFloat32 inverse;
        inverse.setReciprocal(hkSimdFloat32::fromFloat(dot));
        const float scale = 0.0f - inverse.getReal();
        cdInfo.m_suspensionClosingSpeed = scale * relative.dot<3>(normal).getReal();
        cdInfo.m_suspensionScalingFactor = scale;
    }
    else
    {
        cdInfo.m_suspensionClosingSpeed = 0.0f;
        cdInfo.m_suspensionScalingFactor = 1.0f / vehicle->m_data->m_normalClippingAngleCos;
    }
}

// 0x00F7EC70
void hkpVehicleRayCastWheelCollide::getCollisionOutputWithoutHit(const hkpVehicleInstance* vehicle, unsigned char wheelNum, hkpVehicleWheelCollide::CollisionDetectionWheelOutput& cdInfo) const
{
    const hkpVehicleInstance::WheelInfo& wheel = vehicle->m_wheelsInfo[wheelNum];
    const float length = vehicle->m_suspension->m_wheelParams[wheelNum].m_length;
    cdInfo.m_contactBody = NULL;
    cdInfo.m_currentSuspensionLength = length;
    cdInfo.m_suspensionClosingSpeed = 0.0f;
    cdInfo.m_contactPoint.setPosition(wheel.m_rayEndPointWs);
    hkVector4f normal;
    normal.setMul(hkSimdFloat32::fromFloat(-1.0f), wheel.m_suspensionDirectionWs);
    cdInfo.m_contactPoint.setSeparatingNormal(normal, length);
    cdInfo.m_contactFriction = 0.0f;
    cdInfo.m_suspensionScalingFactor = 1.0f;
}

// 0x00F7ED00: for Havok's batched ray casts, which nothing in rfg.exe calls for this class (the game's wheel collide casts
// one wheel at a time). hkpShapeRayCastCommand has no header here, so its layout is written as offsets, from rfg.exe.
// A null collisionFilter is dereferenced for its type, as in rfg.exe.
int hkpVehicleRayCastWheelCollide::buildRayCastCommands(const hkpVehicleInstance* vehicle, const hkpCollisionFilter* collisionFilter, int filterSize, hkpShapeRayCastCommand* commandStorage, hkpWorldRayCastOutput* outputStorage) const
{
    m_phantom->ensureDeterministicOrder();
    const int num_collidables = m_phantom->m_overlappingCollidables.getSize();
    if (num_collidables < 1)
    {
        return 0;
    }
    const int num_wheels = vehicle->m_data->m_numWheels;
    char* command = reinterpret_cast<char*>(commandStorage);
    for (int w = 0; w < num_wheels; w++)
    {
        const hkpVehicleInstance::WheelInfo& wheel = vehicle->m_wheelsInfo[w];
        *reinterpret_cast<hkVector4f*>(command + 0x00) = wheel.m_hardPointWs;
        *reinterpret_cast<hkVector4f*>(command + 0x10) = wheel.m_rayEndPointWs;
        *reinterpret_cast<unsigned int*>(command + 0x20) = m_wheelCollisionFilterInfo;
        *reinterpret_cast<const hkpRayShapeCollectionFilter**>(command + 0x24) = collisionFilter;
        *reinterpret_cast<unsigned int*>(command + 0x30) = collisionFilter->m_type.m_storage;
        *reinterpret_cast<int*>(command + 0x34) = filterSize;
        *reinterpret_cast<hkpCollidable* const**>(command + 0x38) = m_phantom->m_overlappingCollidables.begin();
        *reinterpret_cast<int*>(command + 0x3C) = num_collidables;
        *reinterpret_cast<hkpWorldRayCastOutput**>(command + 0x40) = outputStorage + w;
        *reinterpret_cast<int*>(command + 0x44) = 1;
        *reinterpret_cast<int*>(command + 0x48) = 0;
        *(command + 0x4C) = 0;
        // 0x50: sizeof(hkpShapeRayCastCommand)
        command += 0x50;
    }
    return num_wheels;
}

// 0x00F7EA00
void hkpVehicleRayCastWheelCollide::addToWorld(hkpWorld* world)
{
    world->addPhantom(m_phantom);
}

// 0x00F7EDF0
void hkpVehicleRayCastWheelCollide::removeFromWorld()
{
    m_phantom->m_world->removePhantom(m_phantom);
}

// 0x00F7EE00
void hkpVehicleRayCastWheelCollide::setCollisionFilterInfo(unsigned int filterInfo)
{
    m_wheelCollisionFilterInfo = filterInfo;
    if (m_phantom != NULL)
    {
        m_phantom->getCollidableRw()->setCollisionFilterInfo(filterInfo);
    }
}
