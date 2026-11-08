// Original: volition/rfg/code/vehicle/havok/hk_custom_vehicle_transmission.cpp
//
// Transmission: gear changes at per-gear up/downshift rpms with a clutch delay. An upshift waits until the wheels have
// been on the ground for a moment and stopped slipping.

#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_transmission.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicledata.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/havok/hk_custom_vehicle_instance.h"
#include "volition/rfg/code/vehicle/vehicle_effects.h"

#include <cmath>

// More forward slip than this blocks an upshift.
static float Automobile_max_forward_slip_no_shift; // 0x0377001C, from the tweak table
static tweak_table_item Tweak_table_item_Automobile_max_forward_slip_no_shift("Automobile_max_forward_slip_no_shift", &Automobile_max_forward_slip_no_shift);

// 0x00B75750 (:41)
static bool wheels_are_slipping(const hkpVehicleInstance* vehicle)
{
    for (int w = 0; w < vehicle->m_wheelsInfo.getSize(); w++)
    {
        const hkpVehicleInstance::WheelInfo& wheel = vehicle->m_wheelsInfo[w];
        if (wheel.m_contactBody != NULL && Automobile_max_forward_slip_no_shift <= fabsf(wheel.m_forwardSlipVelocity))
        {
            return true;
        }
    }
    return false;
}

// 0x00B9B0A0 (.h:20)
hk_custom_vehicle_transmission::hk_custom_vehicle_transmission()
{
    m_desired_gear = 0;
    m_wheel_on_ground_timer = 0.0f;
}

// 0x00B9B120
hk_custom_vehicle_transmission::~hk_custom_vehicle_transmission()
{
}

// 0x00B632E0 (.h:29)
signed char hk_custom_vehicle_transmission::getDesiredGear() const
{
    return m_desired_gear;
}

// 0x00B757B0 (:66): unlike the default transmission it reads the real spin, and does not clamp a negative result
float hk_custom_vehicle_transmission::calcTransmissionRPM_havok55(const hkpVehicleInstance* vehicle, hkpVehicleTransmission::TransmissionOutput& transmissionOut) const
{
    float rpm = 0.0f;
    for (int w = 0; w < vehicle->m_data->m_numWheels; w++)
    {
        // 60 * 0.15915494: radians per second to revolutions per minute
        rpm = vehicle->m_wheelsInfo[w].m_spinVelocity * 60.0f * 0.15915494f * m_wheelsTorqueRatio[w] + rpm;
    }
    return getCurrentRatio(vehicle, transmissionOut) * rpm;
}

// 0x00B75980 (:91)
void hk_custom_vehicle_transmission::calcTransmission_havok55(const float deltaTime, const hkpVehicleInstance* vehicle, hkpVehicleTransmission::TransmissionOutput& transmissionOut)
{
    transmissionOut.m_isReversing = calcIsReversing(vehicle, transmissionOut);
    transmissionOut.m_mainTransmittedTorque = calcMainTransmittedTorque(vehicle, transmissionOut);
    transmissionOut.m_transmissionRPM = calcTransmissionRPM_havok55(vehicle, transmissionOut);
    for (int w = 0; w < m_wheelsTorqueRatio.getSize(); w++)
    {
        transmissionOut.m_wheelsTransmittedTorque[w] = m_wheelsTorqueRatio[w] * transmissionOut.m_mainTransmittedTorque;
    }
    updateCurrentGear(deltaTime, vehicle, transmissionOut);
}

// 0x00B75A10 (:118)
void hk_custom_vehicle_transmission::calcTransmission(const float deltaTime, const hkpVehicleInstance* vehicle, hkpVehicleTransmission::TransmissionOutput& transmission_out)
{
    calcTransmission_havok55(deltaTime, vehicle, transmission_out);
    const_cast<hk_custom_vehicle_instance*>(static_cast<const hk_custom_vehicle_instance*>(vehicle))->set_transmission_rpm(transmission_out.m_transmissionRPM);
}

// 0x00B75A40 (:139)
void hk_custom_vehicle_transmission::updateCurrentGear(const float deltaTime, const hkpVehicleInstance* vehicle, hkpVehicleTransmission::TransmissionOutput& transmission_out)
{
    transmission_out.m_clutchDelayCountdown = transmission_out.m_clutchDelayCountdown - deltaTime;
    const float timer = m_wheel_on_ground_timer - deltaTime;
    m_wheel_on_ground_timer = 0.0f <= 0.0f - timer ? 0.0f : timer;

    // An upshift waits out m_clutch_delay_pre_shift before the gear changes.
    if (transmission_out.m_delayed.m_bool != 0)
    {
        if (transmission_out.m_clutchDelayCountdown <= m_clutchDelayTime)
        {
            transmission_out.m_currentGear = m_desired_gear;
        }
        if (transmission_out.m_clutchDelayCountdown <= 0.0f)
        {
            transmission_out.m_delayed = false;
        }
    }

    if (transmission_out.m_isReversing.m_bool != 0)
    {
        return;
    }

    const signed char gear = transmission_out.m_currentGear;
    if (0 < gear && transmission_out.m_transmissionRPM < m_downshift_rpms[gear - 1])
    {
        m_desired_gear = gear - 1;
        transmission_out.m_currentGear = gear - 1;
        transmission_out.m_clutchDelayCountdown = m_clutchDelayTime;
        transmission_out.m_delayed = true;
    }

    object* obj = object_from_world_obj(vehicle->m_entity);
    automobile* ap = NULL;
    if (obj != NULL && obj->get_obj_type() == OT_VEHICLE && obj->get_sub_type() == OT_SUB_VEHICLE_AUTO)
    {
        ap = static_cast<automobile*>(obj);
        if (!automobile_any_wheel_on_ground(ap))
        {
            // 0.3: seconds on the ground before an upshift
            m_wheel_on_ground_timer = 0.3f;
        }
    }

    if (m_wheel_on_ground_timer <= 0.0f && transmission_out.m_delayed.m_bool == 0 && !wheels_are_slipping(vehicle))
    {
        const signed char current = transmission_out.m_currentGear;
        if (current + 1 < m_gearsRatio.getSize() && m_upshift_rpms[current] < transmission_out.m_transmissionRPM)
        {
            m_desired_gear = current + 1;
            transmission_out.m_clutchDelayCountdown = m_clutch_delay_pre_shift + m_clutchDelayTime;
            transmission_out.m_delayed = true;
            // Original bug kept: a vehicle that is not an automobile passes a null vehicle here.
            vehicle_camera_shake_play_shift(ap);
            if (ap != NULL)
            {
                ap->play_shift_foley = true;
            }
        }
    }
}
