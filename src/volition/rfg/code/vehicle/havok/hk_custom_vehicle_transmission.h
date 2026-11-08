// Original: volition/rfg/code/vehicle/havok/hk_custom_vehicle_transmission.h
// Implementation: volition/rfg/code/vehicle/havok/hk_custom_vehicle_transmission.cpp
//
// Transmission: gear changes at per-gear up/downshift rpms with a clutch delay, and a timer for wheels on the ground.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/transmission/default/hkpvehicledefaulttransmission.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/transmission/hkpvehicletransmission.h"

class hkpVehicleInstance;

// size 0x58
class hk_custom_vehicle_transmission : public hkpVehicleDefaultTransmission
{
public:
    hk_custom_vehicle_transmission(); // 0x00B9B0A0 (.h:20)
    virtual void calcTransmission(const float deltaTime, const hkpVehicleInstance* vehicle, hkpVehicleTransmission::TransmissionOutput& transmission_out); // vtbl+0x10 0x00B75A10 (:118)
    signed char getDesiredGear() const; // 0x00B632E0 (.h:29)
protected:
    float calcTransmissionRPM_havok55(const hkpVehicleInstance* vehicle, hkpVehicleTransmission::TransmissionOutput& transmissionOut) const; // 0x00B757B0 (:66)
    void calcTransmission_havok55(const float deltaTime, const hkpVehicleInstance* vehicle, hkpVehicleTransmission::TransmissionOutput& transmissionOut); // 0x00B75980 (:91)
    virtual void updateCurrentGear(const float deltaTime, const hkpVehicleInstance* vehicle, hkpVehicleTransmission::TransmissionOutput& transmission_out); // vtbl+0x20 0x00B75A40 (:139)
public:
    virtual ~hk_custom_vehicle_transmission(); // vtbl+0x00 0x00B9B120

protected:
    signed char m_desired_gear; // 0x34
    float m_wheel_on_ground_timer; // 0x38
public:
    hkArray<float,hkContainerHeapAllocator> m_downshift_rpms; // 0x3C
    hkArray<float,hkContainerHeapAllocator> m_upshift_rpms; // 0x48
    float m_clutch_delay_pre_shift; // 0x54
};
