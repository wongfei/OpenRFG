// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/engine/default/hkpvehicledefaultengine.cpp
//
// Havok 2014.2.5 (stub_impl.md S8): rpm with clutch slip below the minimum, torque and resistance curves around the
// optimum rpm, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/engine/default/hkpvehicledefaultengine.h"

// 0x00F7E8E0 (hkpvehicleengine.h:59)
hkpVehicleEngine::hkpVehicleEngine()
{
}

// 0x00F7E860
hkpVehicleDefaultEngine::hkpVehicleDefaultEngine()
{
    m_minRPM = 1000.0f;
    m_optRPM = 4000.0f;
    m_maxRPM = 6000.0f;
    m_maxTorque = 800.0f;
    m_torqueFactorAtMinRPM = 1.0f;
    m_torqueFactorAtMaxRPM = 1.0f;
    m_resistanceFactorAtMinRPM = 0.0f;
    m_resistanceFactorAtOptRPM = 0.0f;
    m_resistanceFactorAtMaxRPM = 0.0f;
    m_clutchSlipRPM = 2000.0f;
}

// 0x00F7E6D0
void hkpVehicleDefaultEngine::calcEngineInfo(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInput::FilteredDriverInputOutput& filteredDriverInputInfo, const hkpVehicleTransmission::TransmissionOutput& transmissionInfo, hkpVehicleEngine::EngineOutput& engineOutput)
{
    const float transmission_rpm = transmissionInfo.m_transmissionRPM;
    engineOutput.m_rpm = transmission_rpm;
    const float accelerator = filteredDriverInputInfo.m_acceleratorPedalInput;
    if (transmission_rpm < m_minRPM)
    {
        const float half_min = m_minRPM * 0.5f;
        const float slip = m_clutchSlipRPM * accelerator;
        if (half_min <= transmission_rpm)
        {
            engineOutput.m_rpm = ((transmission_rpm - half_min) * slip) / (m_minRPM - half_min) + m_minRPM;
        }
        else
        {
            engineOutput.m_rpm = m_minRPM + slip;
        }
    }

    float torque = 0.0f;
    float resistance;
    const float from_opt = engineOutput.m_rpm - m_optRPM;
    if (0.0f <= from_opt)
    {
        if (engineOutput.m_rpm < m_maxRPM)
        {
            const float inv = 1.0f / (m_maxRPM - m_optRPM);
            engineOutput.m_torque = ((m_torqueFactorAtMaxRPM - 1.0f) * inv * inv * from_opt * from_opt + 1.0f) * m_maxTorque * accelerator
                - ((m_resistanceFactorAtMaxRPM - m_resistanceFactorAtOptRPM) * inv * from_opt + m_resistanceFactorAtOptRPM) * m_maxTorque;
            return;
        }
        engineOutput.m_rpm = m_maxRPM;
        resistance = m_resistanceFactorAtMaxRPM * m_maxTorque;
    }
    else
    {
        const float inv = 1.0f / (m_minRPM - m_optRPM);
        resistance = ((m_resistanceFactorAtMinRPM - m_resistanceFactorAtOptRPM) * inv * from_opt + m_resistanceFactorAtOptRPM) * m_maxTorque;
        torque = ((m_torqueFactorAtMinRPM - 1.0f) * inv * inv * from_opt * from_opt + 1.0f) * m_maxTorque;
        if (transmissionInfo.m_transmissionRPM < m_minRPM)
        {
            engineOutput.m_torque = torque * accelerator - resistance * (transmissionInfo.m_transmissionRPM / m_minRPM);
            return;
        }
    }
    engineOutput.m_torque = torque * accelerator - resistance;
}

// 0x00B631A0
hkpVehicleEngine::~hkpVehicleEngine()
{
}

// 0x00B63530
hkpVehicleDefaultEngine::~hkpVehicleDefaultEngine()
{
}
