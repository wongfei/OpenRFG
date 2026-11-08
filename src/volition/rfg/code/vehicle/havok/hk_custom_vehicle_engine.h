// Original: volition/rfg/code/vehicle/havok/hk_custom_vehicle_engine.h
// Implementation: volition/rfg/code/vehicle/havok/hk_custom_vehicle_engine.cpp
//
// Engine: Havok's default engine with the rpm and torque computed from the custom instance data (AI torque
// multiplier, reverse and powerslide multipliers, enforced maximum speeds).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/driverinput/hkpvehicledriverinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/engine/default/hkpvehicledefaultengine.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/engine/hkpvehicleengine.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/transmission/hkpvehicletransmission.h"

class hk_custom_vehicle_instance;
class hkpVehicleInstance;

// size 0x30
class hk_custom_vehicle_engine : public hkpVehicleDefaultEngine
{
public:
    hk_custom_vehicle_engine(); // 0x00B63540 (.h:17)
    virtual void calcEngineInfo(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInput::FilteredDriverInputOutput& driver_input, const hkpVehicleTransmission::TransmissionOutput& transmission_info, hkpVehicleEngine::EngineOutput& engine_output); // vtbl+0x10 0x00B63880 (:56)
protected:
    virtual float calculate_rpm(const float deltaTime, const hk_custom_vehicle_instance* p_vehicle, const hkpVehicleDriverInput::FilteredDriverInputOutput& driver_input, const hkpVehicleTransmission::TransmissionOutput& transmission_info); // vtbl+0x14 0x00B63900 (:97)
    virtual float calculate_torque(hkpVehicleEngine::EngineOutput& engine_output, const float deltaTime, const hk_custom_vehicle_instance* p_vehicle, const hkpVehicleDriverInput::FilteredDriverInputOutput& driver_input, const hkpVehicleTransmission::TransmissionOutput& transmission_info); // vtbl+0x18 0x00B73F70 (:156)
public:
    virtual ~hk_custom_vehicle_engine(); // vtbl+0x00 0x00B63560

};
