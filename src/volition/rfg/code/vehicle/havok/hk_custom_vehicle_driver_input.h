// Original: volition/rfg/code/vehicle/havok/hk_custom_vehicle_driver_input.h
// Implementation: volition/rfg/code/vehicle/havok/hk_custom_vehicle_driver_input.cpp
//
// Driver input: turns the pedal, steering and handbrake inputs stored in the vehicle instance into Havok's filtered
// driver input.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/driverinput/default/hkpvehicledefaultanalogdriverinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/driverinput/hkpvehicledriverinput.h"

class hkpVehicleDriverInputStatus;
class hkpVehicleInstance;

// size 0x18
class hk_custom_vehicle_driver_input : public hkpVehicleDefaultAnalogDriverInput
{
public:
    hk_custom_vehicle_driver_input(); // 0x00B63500 (.h:18)
    virtual void calcDriverInput(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInputStatus* deviceStatus, hkpVehicleDriverInput::FilteredDriverInputOutput& filtered_input_out); // vtbl+0x10 0x00B63800 (:29)
    virtual ~hk_custom_vehicle_driver_input(); // vtbl+0x00 0x00B63520

};
