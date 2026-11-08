// Original: volition/rfg/code/vehicle/havok/hk_custom_vehicle_driver_input.cpp
//
// Driver input: the inputs come from the instance, not from the device status. The handbrake also scales the yaw term of
// the friction inertia, which makes the vehicle slide round.

#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_driver_input.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicledata.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_instance.h"

// 0x00B63500 (.h:18)
hk_custom_vehicle_driver_input::hk_custom_vehicle_driver_input()
{
}

// 0x00B63520
hk_custom_vehicle_driver_input::~hk_custom_vehicle_driver_input()
{
}

// 0x00B63800 (:29)
void hk_custom_vehicle_driver_input::calcDriverInput(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInputStatus* deviceStatus, hkpVehicleDriverInput::FilteredDriverInputOutput& filtered_input_out)
{
    const hk_custom_vehicle_instance* instance = static_cast<const hk_custom_vehicle_instance*>(vehicle);
    filtered_input_out.m_acceleratorPedalInput = instance->get_accelerator_pedal_input();
    filtered_input_out.m_brakePedalInput = instance->get_brake_pedal_input();
    const bool handbrake = instance->get_handbrake_on();
    filtered_input_out.m_handbrakeOn = handbrake;
    filtered_input_out.m_steeringWheelInput = instance->get_steering_wheel_input();
    filtered_input_out.m_tryingToReverse = vehicle->m_tryingToReverse;
    const float yaw = instance->get_original_yaw_factor();
    vehicle->m_data->m_chassisFrictionInertiaInvDiag(1) = handbrake ? instance->get_handbrake_yaw_factor_scalar_value() * yaw : yaw;
}
