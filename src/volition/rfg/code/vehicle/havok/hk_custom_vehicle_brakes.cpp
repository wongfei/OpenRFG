// Original: volition/rfg/code/vehicle/havok/hk_custom_vehicle_brakes.cpp
//
// Brakes: on top of Havok's default brake, a torque that stops the contact point's speed along the wheel's forward axis.

#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_brakes.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicledata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicleinstance.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_raycast_wheel_collide.h"

#include <cmath>

// 0x00B9B1F0 (.h:18)
hk_custom_vehicle_brakes::hk_custom_vehicle_brakes()
{
}

// 0x00B9B250
hk_custom_vehicle_brakes::~hk_custom_vehicle_brakes()
{
}

// 0x00B9CBE0 (:32)
void hk_custom_vehicle_brakes::calcBreakingInfo(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInput::FilteredDriverInputOutput& driver_input, hkpVehicleBrake::WheelBreakingOutput& brake_info)
{
    hkpVehicleDefaultBrake::calcBreakingInfo(deltaTime, vehicle, driver_input, brake_info);

    const hkpRigidBody* chassis = vehicle->getChassis();
    const float pedal = driver_input.m_brakePedalInput;
    const int num_wheels = vehicle->m_data->m_numWheels;
    const float wheel_mass = chassis->getMass() / (float)num_wheels;
    for (int w = 0; w < num_wheels; w++)
    {
        const hkpVehicleInstance::WheelInfo& wheel = vehicle->m_wheelsInfo[w];
        if (pedal == 0.0f || wheel.m_contactBody == NULL)
        {
            brake_info.m_brakingTorque[w] = 0.0f;
            continue;
        }

        hkVector4f position;
        hkQuaternionf rotation;
        calcHk550CurrentPositionAndRotation(vehicle, chassis, vehicle->m_suspension, w, position, rotation);

        hkVector4f r;
        r.setSub(position, chassis->getCenterOfMassInWorld());
        hkVector4f velocity;
        velocity.setCross(chassis->getAngularVelocity(), r);
        velocity.add(chassis->getLinearVelocity());
        vector game_velocity;
        havok_to_game_vector(game_velocity, velocity);

        hkVector4f forward;
        forward.setCross(wheel.m_contactPoint.getNormal(), wheel.m_spinAxisWs);
        forward.normalize<3>();
        vector game_forward;
        havok_to_game_vector(game_forward, forward);

        float speed = game_velocity.y * game_forward.y + game_forward.x * game_velocity.x + game_velocity.z * game_forward.z;
        // 0.001: a standing wheel gets no torque
        if (fabsf(speed) < 0.001f)
        {
            speed = 0.0f;
        }
        // 0.5: half of what would stop the wheel's share of the vehicle in one step
        const float torque = -wheel_mass * speed * (1.0f / deltaTime) * 0.5f * pedal * vehicle->m_data->m_wheelParams[w].m_radius;
        const float limit = m_wheelBrakingProperties[w].m_maxBreakingTorque * pedal;
        float clamped = 0.0f <= torque - -limit ? torque : -limit;
        if (0.0f <= clamped - limit)
        {
            clamped = limit;
        }
        brake_info.m_brakingTorque[w] = clamped;
    }
}
