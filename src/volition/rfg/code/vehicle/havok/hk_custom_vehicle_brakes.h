// Original: volition/rfg/code/vehicle/havok/hk_custom_vehicle_brakes.h
// Implementation: volition/rfg/code/vehicle/havok/hk_custom_vehicle_brakes.cpp
//
// Brakes: Havok's default brake with the wheel blocking delay tuned separately for AI drivers.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/brake/default/hkpvehicledefaultbrake.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/brake/hkpvehiclebrake.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/driverinput/hkpvehicledriverinput.h"

class hkpVehicleInstance;

// size 0x24
class hk_custom_vehicle_brakes : public hkpVehicleDefaultBrake
{
public:
    hk_custom_vehicle_brakes(); // 0x00B9B1F0 (.h:18)
    virtual void calcBreakingInfo(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInput::FilteredDriverInputOutput& driver_input, hkpVehicleBrake::WheelBreakingOutput& brake_info); // vtbl+0x10 0x00B9CBE0 (:32)
    virtual ~hk_custom_vehicle_brakes(); // vtbl+0x00 0x00B9B250

    float m_ai_min_time_to_block; // 0x18
    float m_min_time_to_block; // 0x1C
    float m_min_pedal_input_to_block; // 0x20
};
