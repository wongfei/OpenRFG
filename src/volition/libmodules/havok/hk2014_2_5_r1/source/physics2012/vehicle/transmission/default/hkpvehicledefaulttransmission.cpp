// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/transmission/default/hkpvehicledefaulttransmission.cpp
//
// Havok 2014.2.5 (stub_impl.md S8): gear ratios, torque split and automatic shifting with a clutch delay, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/transmission/default/hkpvehicledefaulttransmission.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicledata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicleinstance.h"

// 0x00F80960 (hkpvehicletransmission.h:73)
hkpVehicleTransmission::hkpVehicleTransmission()
{
}

// 0x00F80910
hkpVehicleDefaultTransmission::hkpVehicleDefaultTransmission()
{
    m_downshiftRPM = 0.0f;
    m_upshiftRPM = 0.0f;
    m_primaryTransmissionRatio = 0.0f;
    m_clutchDelayTime = 0.0f;
    m_reverseGearRatio = 0.0f;
}

// 0x00F80610
float hkpVehicleDefaultTransmission::getCurrentRatio(const hkpVehicleInstance* vehicle, hkpVehicleTransmission::TransmissionOutput& transmissionOut) const
{
    if (transmissionOut.m_isReversing.m_bool != 0)
    {
        return m_primaryTransmissionRatio * -m_reverseGearRatio;
    }
    return m_primaryTransmissionRatio * m_gearsRatio[transmissionOut.m_currentGear];
}

// 0x00F80590: 1.609 km per mile, 0.2777778 = 1/3.6 m/s per km/h, 0.15915494 = 1/(2 pi)
float hkpVehicleDefaultTransmission::calculatePrimaryTransmissionRatio(const float vehicleTopSpeedMPH, const float wheelRadius, const float maxEngineRpm, const float topGearRatio)
{
    return (maxEngineRpm / (((vehicleTopSpeedMPH * 1.609f * 0.2777778f) / wheelRadius) * 60.0f * 0.15915494f)) / topGearRatio;
}

// 0x00F805C0: 0.621 miles per km
float hkpVehicleDefaultTransmission::calculatePrimaryTransmissionRatioKPH(const float vehicleTopSpeedKPH, const float wheelRadius, const float maxEngineRpm, const float topGearRatio)
{
    return calculatePrimaryTransmissionRatio(vehicleTopSpeedKPH * 0.621f, wheelRadius, maxEngineRpm, topGearRatio);
}

// 0x00F80660
void hkpVehicleDefaultTransmission::calcTransmission(const float deltaTime, const hkpVehicleInstance* vehicle, hkpVehicleTransmission::TransmissionOutput& transmissionOut)
{
    transmissionOut.m_isReversing = calcIsReversing(vehicle, transmissionOut);
    transmissionOut.m_mainTransmittedTorque = calcMainTransmittedTorque(vehicle, transmissionOut);
    transmissionOut.m_transmissionRPM = calcTransmissionRPM(vehicle, transmissionOut);
    for (int w = 0; w < m_wheelsTorqueRatio.getSize(); w++)
    {
        transmissionOut.m_wheelsTransmittedTorque[w] = m_wheelsTorqueRatio[w] * transmissionOut.m_mainTransmittedTorque;
    }
    updateCurrentGear(deltaTime, vehicle, transmissionOut);
}

// 0x00F806E0
float hkpVehicleDefaultTransmission::calcMainTransmittedTorque(const hkpVehicleInstance* vehicle, hkpVehicleTransmission::TransmissionOutput& transmissionOut) const
{
    if (transmissionOut.m_delayed.m_bool != 0)
    {
        return 0.0f;
    }
    return vehicle->m_torque * getCurrentRatio(vehicle, transmissionOut);
}

// 0x00F80710: the wheel rpm weighted by each wheel's share of the torque
float hkpVehicleDefaultTransmission::calcTransmissionRPM(const hkpVehicleInstance* vehicle, hkpVehicleTransmission::TransmissionOutput& transmissionOut) const
{
    float rpm = 0.0f;
    for (int w = 0; w < vehicle->m_data->m_numWheels; w++)
    {
        // 60 * 0.15915494: radians per second to revolutions per minute
        rpm = vehicle->m_wheelsInfo[w].m_noSlipIdealSpinVelocity * 60.0f * 0.15915494f * m_wheelsTorqueRatio[w] + rpm;
    }
    const float result = getCurrentRatio(vehicle, transmissionOut) * rpm;
    return 0.0f <= result ? result : 0.0f;
}

// 0x00F80860
void hkpVehicleDefaultTransmission::updateCurrentGear(const float deltaTime, const hkpVehicleInstance* vehicle, hkpVehicleTransmission::TransmissionOutput& transmissionOut)
{
    transmissionOut.m_clutchDelayCountdown -= deltaTime;
    if (transmissionOut.m_delayed.m_bool != 0 && transmissionOut.m_clutchDelayCountdown <= 0.0f)
    {
        transmissionOut.m_delayed = false;
    }
    if (transmissionOut.m_isReversing.m_bool != 0)
    {
        return;
    }
    if (transmissionOut.m_transmissionRPM < m_downshiftRPM && 0 < transmissionOut.m_currentGear)
    {
        transmissionOut.m_currentGear--;
        transmissionOut.m_clutchDelayCountdown = m_clutchDelayTime;
        transmissionOut.m_delayed = true;
    }
    if (m_upshiftRPM < transmissionOut.m_transmissionRPM && transmissionOut.m_currentGear + 1 < m_gearsRatio.getSize())
    {
        transmissionOut.m_currentGear++;
        transmissionOut.m_clutchDelayCountdown = m_clutchDelayTime;
        transmissionOut.m_delayed = true;
    }
}

// 0x00F808E0
hkBool hkpVehicleDefaultTransmission::calcIsReversing(const hkpVehicleInstance* vehicle, hkpVehicleTransmission::TransmissionOutput& transmissionOut) const
{
    return vehicle->m_tryingToReverse.m_bool != 0 && transmissionOut.m_currentGear < 1;
}

// 0x00B63B90
hkpVehicleTransmission::TransmissionOutput::TransmissionOutput()
{
}

// 0x00B63190
hkpVehicleTransmission::~hkpVehicleTransmission()
{
}

// 0x00B9B020
hkpVehicleDefaultTransmission::~hkpVehicleDefaultTransmission()
{
}
