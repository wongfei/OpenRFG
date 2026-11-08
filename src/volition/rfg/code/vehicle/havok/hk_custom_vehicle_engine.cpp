// Original: volition/rfg/code/vehicle/havok/hk_custom_vehicle_engine.cpp
//
// Engine: the rpm follows the pedal or the speed at a limited rate; the torque fades out near the enforced maximum speed
// and turns into engine braking past it.

#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_engine.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_instance.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_transmission.h"

#include <cmath>

// 0x00B63540 (.h:17)
hk_custom_vehicle_engine::hk_custom_vehicle_engine()
{
}

// 0x00B63560
hk_custom_vehicle_engine::~hk_custom_vehicle_engine()
{
}

// 0x00B63880 (:56)
void hk_custom_vehicle_engine::calcEngineInfo(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInput::FilteredDriverInputOutput& driver_input, const hkpVehicleTransmission::TransmissionOutput& transmission_info, hkpVehicleEngine::EngineOutput& engine_output)
{
    const hk_custom_vehicle_instance* instance = static_cast<const hk_custom_vehicle_instance*>(vehicle);
    if (!instance->get_is_engine_running())
    {
        engine_output.m_rpm = 0.0f;
        engine_output.m_torque = 0.0f;
        return;
    }
    engine_output.m_rpm = calculate_rpm(deltaTime, instance, driver_input, transmission_info);
    engine_output.m_torque = calculate_torque(engine_output, deltaTime, instance, driver_input, transmission_info);
}

// 0x00B63900 (:97). m_clutchSlipRPM is used as the rpm at full pedal.
float hk_custom_vehicle_engine::calculate_rpm(const float deltaTime, const hk_custom_vehicle_instance* p_vehicle, const hkpVehicleDriverInput::FilteredDriverInputOutput& driver_input, const hkpVehicleTransmission::TransmissionOutput& transmission_info)
{
    const float rpm_range = m_clutchSlipRPM - m_minRPM;
    float target = rpm_range * driver_input.m_acceleratorPedalInput + m_minRPM;
    // 0.2: below this pedal the rpm follows the speed
    if (0.2f < driver_input.m_acceleratorPedalInput)
    {
        const hk_custom_vehicle_transmission* transmission = static_cast<const hk_custom_vehicle_transmission*>(p_vehicle->m_transmission);
        if (target <= transmission_info.m_transmissionRPM && transmission_info.m_currentGear == transmission->getDesiredGear())
        {
            target = transmission_info.m_transmissionRPM;
        }
    }
    else
    {
        const float max_speed = p_vehicle->m_tryingToReverse.m_bool == 0 ? p_vehicle->get_enforced_max_speed() : p_vehicle->get_enforced_max_reverse_speed();
        target = (p_vehicle->calcMPH() / max_speed) * rpm_range + m_minRPM;
    }

    // At most -3000 to 6000 rpm per second, slowing within the last 750 rpm.
    const float current = p_vehicle->m_rpm;
    const float delta = target - current;
    float rate = (1.0f / deltaTime) * delta;
    if (rate - -3000.0f < 0.0f)
    {
        rate = -3000.0f;
    }
    if (0.0f <= rate - 6000.0f)
    {
        rate = 6000.0f;
    }
    const float distance = fabsf(delta);
    const float scale = 750.0f <= distance ? 1.0f : distance / 750.0f;
    return scale * rate * deltaTime + current;
}

// 0x00B73F70 (:156)
float hk_custom_vehicle_engine::calculate_torque(hkpVehicleEngine::EngineOutput& engine_output, const float deltaTime, const hk_custom_vehicle_instance* p_vehicle, const hkpVehicleDriverInput::FilteredDriverInputOutput& driver_input, const hkpVehicleTransmission::TransmissionOutput& transmission_info)
{
    if (p_vehicle->get_is_trailer())
    {
        return 0.0f;
    }

    float pedal = driver_input.m_acceleratorPedalInput;
    const float speed = p_vehicle->calcMPH();
    const float max_speed = (p_vehicle->m_tryingToReverse.m_bool != 0 ? p_vehicle->get_enforced_max_reverse_speed() : p_vehicle->get_enforced_max_speed()) - 2.0f;
    bool engine_braking = false;
    if (max_speed < speed)
    {
        // The pedal fades out over the 6 mph past the limit.
        if (max_speed + 6.0f <= speed)
        {
            engine_braking = true;
        }
        else
        {
            float over = (speed - max_speed) / 6.0f;
            if (over - 0.0f < 0.0f)
            {
                over = 0.0f;
            }
            if (0.0f <= over - 1.0f)
            {
                over = 1.0f;
            }
            if (pedal - 0.0f < 0.0f)
            {
                pedal = 0.0f;
            }
            if (0.0f <= pedal - (1.0f - over))
            {
                pedal = 1.0f - over;
            }
        }
    }

    if (engine_braking || pedal == 0.0f)
    {
        const float mass = p_vehicle->getChassis()->getMass();
        float ratio = transmission_info.m_transmissionRPM / m_minRPM;
        if (ratio < -1.0f)
        {
            ratio = -1.0f;
        }
        else if (1.0f < ratio)
        {
            ratio = 1.0f;
        }
        // 0.35: the strength of the engine braking
        return -(ratio * ((mass * 0.35f * engine_output.m_rpm) / m_maxRPM));
    }

    float rpm = (m_clutchSlipRPM - m_minRPM) * pedal + m_minRPM;
    if (0.0f <= engine_output.m_rpm - rpm)
    {
        rpm = engine_output.m_rpm;
    }

    float torque;
    float resistance;
    const float from_opt = rpm - m_optRPM;
    if (0.0f <= from_opt)
    {
        if (rpm < m_maxRPM)
        {
            const float inv = 1.0f / (m_maxRPM - m_optRPM);
            resistance = ((m_resistanceFactorAtMaxRPM - m_resistanceFactorAtOptRPM) * inv * from_opt + m_resistanceFactorAtOptRPM) * m_maxTorque;
            torque = ((m_torqueFactorAtMaxRPM - 1.0f) * inv * inv * from_opt * from_opt + 1.0f) * m_maxTorque;
        }
        else
        {
            engine_output.m_rpm = m_maxRPM;
            torque = 0.0f;
            resistance = m_resistanceFactorAtMaxRPM * m_maxTorque;
        }
    }
    else
    {
        const float inv = 1.0f / (m_minRPM - m_optRPM);
        resistance = ((m_resistanceFactorAtMinRPM - m_resistanceFactorAtOptRPM) * inv * from_opt + m_resistanceFactorAtOptRPM) * m_maxTorque;
        torque = ((m_torqueFactorAtMinRPM - 1.0f) * inv * inv * from_opt * from_opt + 1.0f) * m_maxTorque;
        if (transmission_info.m_transmissionRPM < m_minRPM)
        {
            // The multiplication is missing from the decompile; the disassembly has it (0x00B74279).
            resistance = resistance * (transmission_info.m_transmissionRPM / m_minRPM);
        }
    }

    float result = (torque * pedal - resistance) * p_vehicle->get_ai_torque_multiplier();
    if (p_vehicle->m_tryingToReverse.m_bool != 0)
    {
        result = p_vehicle->get_reverse_torque_multiplier() * result;
    }
    return p_vehicle->get_powerslide_torque_multiplier() * result;
}
