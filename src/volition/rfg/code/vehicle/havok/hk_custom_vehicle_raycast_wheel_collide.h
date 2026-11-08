// Original: volition/rfg/code/vehicle/havok/hk_custom_vehicle_raycast_wheel_collide.h
// Implementation: volition/rfg/code/vehicle/havok/hk_custom_vehicle_raycast_wheel_collide.cpp
//
// Wheel collision: ray casts per wheel (or a cheaper fake ray reusing the last result, or sphere/cylinder linear
// casts, vehicle_wheel_cast_type). collideWheels casts m_num_wheels_to_cast_per_step wheels per step, round robin
// (m_wheel_index_to_cast), and stores the physical and effect material under each wheel. The ray collector skips the
// vehicle's own chassis and collision group and hits facing away from the cast (decompiled). Placement of
// vehicle_wheel_cast_type inferred.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldraycastoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkpclosestcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicleinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/wheelcollide/hkpvehiclewheelcollide.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/wheelcollide/raycast/hkpvehicleraycastwheelcollide.h"
#include "volition/rfg/code/havok/havok_raycast_collectors.h"
#include "volition/vlib/code/util/bitarray.h"

class hkAabb;
class hkQuaternionf;
class hk_custom_vehicle_instance;
class hkpCdBody;
struct hkpCdPoint;
class hkpRigidBody;
class hkpShapePhantom;
struct hkpShapeRayCastCollectorOutput;
class hkpVehicleSuspension;

enum vehicle_wheel_cast_type
{
    VEHICLE_WHEEL_CAST_RAY = 0,
    VEHICLE_WHEEL_CAST_FAKE_RAY = 1,
    VEHICLE_WHEEL_CAST_SPHERE = 2,
    VEHICLE_WHEEL_CAST_CYLINDER = 3,
};

// size 0x90
class havok_closest_wheel_ray_hit_collector : public havok_closest_ray_hit_collector
{
public:
    virtual void addRayHit(const hkpCdBody& cd_body, const hkpShapeRayCastCollectorOutput& hitInfo); // vtbl+0x00 0x00B74AB0 (:115)
    void set_owner_vehicle(const hkpVehicleInstance* vehicle); // 0x00B63DA0 (:179)
    void set_cast_direction(const hkVector4f& dir); // 0x00B960F0 (:172)
    havok_closest_wheel_ray_hit_collector(); // 0x00B74BB0
    virtual ~havok_closest_wheel_ray_hit_collector(); // vtbl+0x04 0x00B74BE0

protected:
    const hk_custom_vehicle_instance* owner_vehicle_instance; // 0x70
    hkVector4f cast_direction; // 0x80
};

// size 0x70
class havok_closest_linear_wheel_cast_collector : public hkpClosestCdPointCollector
{
public:
    virtual void addCdPoint(const hkpCdPoint& point); // vtbl+0x04 0x00B9D3E0 (:209)
    void set_owner_vehicle(const hkpVehicleInstance* vehicle); // 0x00B63DE0 (:302)
    void set_cast_direction(const hkVector4f& dir); // 0x00B96170 (:295)
    havok_closest_linear_wheel_cast_collector(); // 0x00B74BF0
    virtual ~havok_closest_linear_wheel_cast_collector(); // vtbl+0x00 0x00B63FD0

protected:
    const hk_custom_vehicle_instance* owner_vehicle_instance; // 0x50
    hkVector4f cast_direction; // 0x60
};

// size 0x6C0
class hk_custom_vehicle_raycast_wheel_collide : public hkpVehicleRayCastWheelCollide
{
public:
    hk_custom_vehicle_raycast_wheel_collide(unsigned int num_wheels); // 0x00BAFCE0 (:321)
    virtual ~hk_custom_vehicle_raycast_wheel_collide(); // vtbl+0x00 0x00BAFF90 (:350)
    virtual void init(const hkpVehicleInstance* vehicle); // vtbl+0x10 0x00B961F0 (:372)
    virtual void collideWheels(const float deltaTime, const hkpVehicleInstance* vehicle, hkpVehicleWheelCollide::CollisionDetectionWheelOutput* cdInfoOut); // vtbl+0x14 0x00B8A390 (:461)
    virtual void calcWheelsAABB(const hkpVehicleInstance* vehicle, hkAabb& aabbOut) const; // vtbl+0x38 0x00B63E40 (:710)
    virtual void castSingleWheel(const hkpVehicleInstance::WheelInfo& wheelInfo, hkpWorldRayCastOutput& output) const; // vtbl+0x3C 0x00BAFF40 (.h:50)
    virtual void castSingleWheel(const hkpVehicleInstance::WheelInfo& wheel_info, const hkpVehicleInstance* vehicle, hkpWorldRayCastOutput& rc_output); // vtbl+0x48 0x00BA7C80 (:753)
    virtual void calcSingleWheelGroundFriction(const hkpVehicleInstance* vehicle, signed char wheelInfoNum, const hkpWorldRayCastOutput& worldRayCastOutput, float& frictionOut) const; // vtbl+0x4C 0x00B63DF0 (:363)
    void setup_for_player(); // 0x00B63FE0 (:876)
    void setup_for_nonplayer(); // 0x00B63FF0 (:882)
    void set_npc_ray_override(bool ride); // 0x00B64000 (:893)
    void clear_wheel_caches(); // 0x00B74CD0 (:899)
protected:
    vehicle_wheel_cast_type get_wheel_cast_type(unsigned int wheel_index, const hkpVehicleInstance* hk_vehicle) const; // 0x00B8A750 (:918)
    bool should_raycast_this_wheel(unsigned int wheel_index, unsigned int num_wheels) const; // 0x00B64010 (:974)
    void raycast_process_results(hkpWorldRayCastOutput& rc_output, const havok_closest_wheel_ray_hit_collector& ray_collector, unsigned int wheel_index); // 0x00B8A7A0 (:1018)
    void fake_raycast_process_results(hkpWorldRayCastOutput& rc_output, unsigned int wheel_index, const hkpVehicleInstance::WheelInfo& wheel_info); // 0x00B74D20 (:1053)
    void linearcast_process_results(hkpWorldRayCastOutput& rc_output, const havok_closest_linear_wheel_cast_collector& linear_collector, const hkpVehicleInstance* hk_vehicle, unsigned int wheel_index); // 0x00B9D630 (:1108)

    float bbox_offset; // 0x24
    bool ray_override; // 0x28
    bool npc_ray_override; // 0x29
    vehicle_wheel_cast_type cast_mode; // 0x2C
    hkVector4f actual_contact_point; // 0x30
    unsigned int m_wheel_index_to_cast; // 0x40
    bool m_used_raycasts_last_time; // 0x44
    hkInplaceArray<hkpWorldRayCastOutput,16,hkContainerHeapAllocator> m_wheel_raycast_results; // 0x50
    unsigned char m_wheel_can_use_fake_raycast_buffer[2]; // 0x660
    bit_array m_wheel_can_use_fake_raycast; // 0x664
    unsigned int m_num_wheels_to_cast_per_step; // 0x670
    hkInplaceArray<hkpShapePhantom*,16,hkContainerHeapAllocator> wheel_phantoms; // 0x674
};

void calcHk550CurrentPositionAndRotation(const hkpVehicleInstance* pInstance, const hkpRigidBody* chassis, const hkpVehicleSuspension* suspension, int wheelNo, hkVector4f& posOut, hkQuaternionf& rotOut); // 0x00B74E40 (:1158)
