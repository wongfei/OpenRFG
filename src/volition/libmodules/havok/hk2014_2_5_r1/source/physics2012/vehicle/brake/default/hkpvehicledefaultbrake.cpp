// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/brake/default/hkpvehicledefaultbrake.cpp
//
// Havok 2014.2.5 (stub_impl.md S8): braking torque per wheel, blocking the wheels after a delay on a hard pedal, from
// rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/brake/default/hkpvehicledefaultbrake.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicledata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicleinstance.h"

#include <cmath>

// 0x00F80C90 (hkpvehiclebrake.h:58)
hkpVehicleBrake::hkpVehicleBrake()
{
}

// 0x00F80C60
hkpVehicleDefaultBrake::hkpVehicleDefaultBrake()
{
    m_wheelsMinTimeToBlock = 0.0f;
}

// 0x00F80AA0
void hkpVehicleDefaultBrake::calcBreakingInfo(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInput::FilteredDriverInputOutput& filteredDriverInfo, hkpVehicleBrake::WheelBreakingOutput& breakingInfo)
{
    const float pedal = filteredDriverInfo.m_brakePedalInput;
    const bool handbrake = filteredDriverInfo.m_handbrakeOn.m_bool != 0;
    bool block = false;
    const float inv_dt = 1.0f / deltaTime;
    for (int w = 0; w < vehicle->m_data->m_numWheels; w++)
    {
        const WheelBrakingProperties& properties = m_wheelBrakingProperties[w];
        breakingInfo.m_isFixed[w] = properties.m_isConnectedToHandbrake.m_bool != 0 && handbrake;
        if (properties.m_minPedalInputToBlock < pedal)
        {
            block = true;
        }

        // The torque that would stop the wheel within this step, limited by what the pedal gives.
        const float max_torque = properties.m_maxBreakingTorque * pedal;
        const hkpVehicleData::WheelComponentParams& wheel = vehicle->m_data->m_wheelParams[w];
        const float stop_torque = -(vehicle->m_wheelsInfo[w].m_spinVelocity * wheel.m_radius * wheel.m_mass * inv_dt) * wheel.m_radius;
        float torque = stop_torque;
        if (max_torque < fabsf(stop_torque))
        {
            torque = stop_torque <= 0.0f ? -max_torque : max_torque;
        }
        breakingInfo.m_brakingTorque[w] = torque;
    }

    if (!block)
    {
        breakingInfo.m_wheelsTimeSinceMaxPedalInput = 0.0f;
        return;
    }
    if (breakingInfo.m_wheelsTimeSinceMaxPedalInput < m_wheelsMinTimeToBlock)
    {
        breakingInfo.m_wheelsTimeSinceMaxPedalInput += deltaTime;
        return;
    }
    for (int w = 0; w < vehicle->m_data->m_numWheels; w++)
    {
        if (m_wheelBrakingProperties[w].m_minPedalInputToBlock < pedal)
        {
            breakingInfo.m_isFixed[w] = true;
        }
    }
}

// 0x00BA7BC0
hkpVehicleBrake::WheelBreakingOutput::WheelBreakingOutput()
{
}

// 0x00BA7BF0
hkpVehicleBrake::WheelBreakingOutput::~WheelBreakingOutput()
{
}

// 0x00B63180
hkpVehicleBrake::~hkpVehicleBrake()
{
}

// 0x00B9B1A0
hkpVehicleDefaultBrake::~hkpVehicleDefaultBrake()
{
}
