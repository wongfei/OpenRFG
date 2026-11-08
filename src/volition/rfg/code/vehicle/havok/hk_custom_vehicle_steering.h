// Original: volition/rfg/code/vehicle/havok/hk_custom_vehicle_steering.h
// Implementation: volition/rfg/code/vehicle/havok/hk_custom_vehicle_steering.cpp
//
// Steering: the steering angle moves towards the input at limited turn and return speeds, with damping and counter
// steering between a minimum and maximum speed.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/driverinput/hkpvehicledriverinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/steering/default/hkpvehicledefaultsteering.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/steering/hkpvehiclesteering.h"

class hk_custom_vehicle_instance;
class hkpVehicleInstance;

// size 0x34
class hk_custom_vehicle_steering : public hkpVehicleDefaultSteering
{
public:
    hk_custom_vehicle_steering(); // 0x00B9D830 (:38)
    virtual void calcSteering(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInput::FilteredDriverInputOutput& filtered_info_output, hkpVehicleSteering::SteeringAnglesOutput& steering_output); // vtbl+0x10 0x00B8A8C0 (:50)
    float calculate_new_steering_angle(const float deltaTime, const hk_custom_vehicle_instance* vehicle, float current_angle, float destination_angle); // 0x00B75020 (:128)
    virtual void calcMainSteeringAngle(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInput::FilteredDriverInputOutput& filteredInfoOutput, hkpVehicleSteering::SteeringAnglesOutput& steeringOutput); // vtbl+0x14 0x00B75300 (:247)
    virtual ~hk_custom_vehicle_steering(); // vtbl+0x00 0x00B9D8A0

    float m_max_turn_speed; // 0x1C
    float m_max_return_speed; // 0x20
    float m_damp_angle; // 0x24
    float m_return_damp_angle; // 0x28
    float m_counter_steer_max_speed; // 0x2C
    float m_counter_steer_min_speed; // 0x30
};
