// Original: volition/rfg/code/vehicle/havok/hk_custom_vehicle_suspension.h
// Implementation: volition/rfg/code/vehicle/havok/hk_custom_vehicle_suspension.cpp
//
// Suspension: Havok's default spring suspension with a per-wheel zero point and spring exponent.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/suspension/default/hkpvehicledefaultsuspension.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/wheelcollide/hkpvehiclewheelcollide.h"

class hkVector4f;
class hkpMotion;
class hkpVehicleInstance;

// size 0xB8
class hk_custom_vehicle_suspension : public hkpVehicleDefaultSuspension
{
public:
    hk_custom_vehicle_suspension(); // 0x00BA4EF0 (.h:20)
    virtual void calcSuspension(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleWheelCollide::CollisionDetectionWheelOutput* cdInfo, float* suspensionForceOut); // vtbl+0x10 0x00BA7FE0 (:269)
private:
    void calcSuspension_core(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleWheelCollide::CollisionDetectionWheelOutput* cdInfo, float* suspensionForceOut); // 0x00B755B0 (:166)
    static float calculate_suspension_length(const hkVector4f& ws_hardpoint, const hkVector4f& ws_suspension_direction, float wheel_radius, float spring_max_length, const hkpVehicleWheelCollide::CollisionDetectionWheelOutput* cdInfo); // 0x00B75400 (:55)
    static float calculate_suspension_relative_velocity(const hkpMotion& proxy_motion, const hkVector4f& ws_contact_position, const hkVector4f& ws_suspension_direction); // 0x00B75540 (:138)
public:
    virtual ~hk_custom_vehicle_suspension(); // vtbl+0x00 0x00BA4F40

    hkInplaceArray<float,16,hkContainerHeapAllocator> zero_point; // 0x20
    hkInplaceArray<float,16,hkContainerHeapAllocator> spring_pow; // 0x6C
};
