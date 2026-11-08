// Original: volition/rfg/code/vehicle/havok/hk_custom_vehicle_steering.cpp
//
// Steering: the angle moves towards the input at limited turn and return speeds, with damping; the rear axle counter
// steers, less at speed.

#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_steering.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicledata.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_instance.h"
#include "volition/vlib/code/math/floating.h"

#include <cmath>

// Past the minimum reverse speed the turn speed is blended down to pct by the maximum speed.
static float Vehicle_reverse_turn_speed_min = 5.0f; // 0x016BA49C
static float Vehicle_reverse_turn_speed_max = 15.0f; // 0x016BA4A0
static float Vehicle_reverse_turn_speed_pct = 0.3f; // 0x016BA4A4

// 0x00B9D830 (:38)
hk_custom_vehicle_steering::hk_custom_vehicle_steering()
{
    m_max_turn_speed = 0.0f;
    m_max_return_speed = 0.0f;
    m_damp_angle = 0.0f;
    m_return_damp_angle = 0.0f;
}

// 0x00B9D8A0
hk_custom_vehicle_steering::~hk_custom_vehicle_steering()
{
}

// 0x00B8A8C0 (:50)
void hk_custom_vehicle_steering::calcSteering(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInput::FilteredDriverInputOutput& filtered_info_output, hkpVehicleSteering::SteeringAnglesOutput& steering_output)
{
    const float current = vehicle->m_mainSteeringAngle;
    calcMainSteeringAngle(deltaTime, vehicle, filtered_info_output, steering_output);
    const float angle = calculate_new_steering_angle(deltaTime, static_cast<const hk_custom_vehicle_instance*>(vehicle), current, steering_output.m_mainSteeringAngle);
    steering_output.m_mainSteeringAngle = angle;

    const hkpRigidBody* chassis = vehicle->getChassis();
    hkVector4f forward;
    forward.setRotatedDir(chassis->getTransform().getRotation(), vehicle->m_data->m_chassisOrientation.getColumn<1>());
    const float speed = chassis->getLinearVelocity().dot<3>(forward).getReal();

    // The axle at the back of the direction of travel gets the counter steering's reduced angle.
    const float scaled = angle * fl_parameter(fabsf(speed), m_counter_steer_max_speed, m_counter_steer_min_speed, true);
    const float axle0_angle = 0.0f <= speed ? angle : scaled;
    const float other_angle = -(0.0f <= speed ? scaled : angle);
    for (int w = 0; w < m_doesWheelSteer.getSize(); w++)
    {
        if (m_doesWheelSteer[w].m_bool == 0)
        {
            steering_output.m_wheelsSteeringAngle[w] = 0.0f;
        }
        else
        {
            steering_output.m_wheelsSteeringAngle[w] = vehicle->m_data->m_wheelParams[w].m_axle == 0 ? axle0_angle : other_angle;
        }
    }
}

// 0x00B75020 (:128)
float hk_custom_vehicle_steering::calculate_new_steering_angle(const float deltaTime, const hk_custom_vehicle_instance* vehicle, float current_angle, float destination_angle)
{
    // Turning out past half lock blends towards the turn speed; anything else is a return.
    float max_speed = m_max_return_speed;
    float damp_angle = m_return_damp_angle;
    const float half_lock = m_maxSteeringAngle * 0.5f;
    if (half_lock <= fabsf(destination_angle))
    {
        float t = (fabsf(destination_angle) - half_lock) / fabsf(destination_angle - current_angle);
        if (t - 0.0f < 0.0f)
        {
            t = 0.0f;
        }
        if (0.0f <= t - 1.0f)
        {
            t = 1.0f;
        }
        max_speed = m_max_turn_speed * t + m_max_return_speed * (1.0f - t);
        damp_angle = m_damp_angle * t + m_return_damp_angle * (1.0f - t);
    }

    if (!vehicle->get_is_tank())
    {
        const hkVector4f& velocity = vehicle->getChassis()->getLinearVelocity();
        const hkVector4f& forward = vehicle->getChassis()->getTransform().getRotation().getColumn<2>();
        const float reverse_speed = -(velocity(1) * forward(1) + velocity(0) * forward(0) + velocity(2) * forward(2));
        if (Vehicle_reverse_turn_speed_min < reverse_speed)
        {
            float t = (reverse_speed - Vehicle_reverse_turn_speed_min) / (Vehicle_reverse_turn_speed_max - Vehicle_reverse_turn_speed_min);
            if (t - 0.0f < 0.0f)
            {
                t = 0.0f;
            }
            if (0.0f <= t - 1.0f)
            {
                t = 1.0f;
            }
            max_speed = (Vehicle_reverse_turn_speed_pct * t + (1.0f - t)) * max_speed;
        }
    }

    float& turn_velocity = const_cast<hk_custom_vehicle_instance*>(vehicle)->get_turn_velocity();
    float opposite_dir_decel = 0.0f;
    if ((destination_angle - current_angle) * turn_velocity < 0.0f)
    {
        opposite_dir_decel = (m_max_return_speed * m_max_return_speed) / (m_return_damp_angle * 2.0f);
    }
    // 1e-06: close enough
    fl_ramp_plateau(current_angle, turn_velocity, destination_angle, max_speed, damp_angle, deltaTime, 1e-06f, opposite_dir_decel);
    return current_angle;
}

// 0x00B75300 (:247)
void hk_custom_vehicle_steering::calcMainSteeringAngle(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInput::FilteredDriverInputOutput& filteredInfoOutput, hkpVehicleSteering::SteeringAnglesOutput& steeringOutput)
{
    const hkpRigidBody* chassis = vehicle->getChassis();
    steeringOutput.m_mainSteeringAngle = filteredInfoOutput.m_steeringWheelInput * m_maxSteeringAngle;
    hkVector4f forward;
    forward.setRotatedDir(chassis->getTransform().getRotation(), vehicle->m_data->m_chassisOrientation.getColumn<1>());
    const float speed = fabsf(chassis->getLinearVelocity().dot<3>(forward).getReal());
    if (m_maxSpeedFullSteeringAngle < speed)
    {
        const float ratio = m_maxSpeedFullSteeringAngle / speed;
        steeringOutput.m_mainSteeringAngle = steeringOutput.m_mainSteeringAngle * ratio * ratio;
    }
}
