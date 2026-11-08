// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/driverinput/default/hkpvehicledefaultanalogdriverinput.cpp
//
// Havok 2014.2.5 (stub_impl.md S8): turns an analog stick into pedal, steering and reverse inputs, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/driverinput/default/hkpvehicledefaultanalogdriverinput.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicledata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicleinstance.h"

#include <cmath>

// Below this speed (5 km/h in m/s) the driver can change direction.
static const float VEHICLE_REVERSE_SWITCH_SPEED = 1.388889f;

// 0x00F7E5A0 (hkpvehicledriverinput.h:90)
hkpVehicleDriverInput::hkpVehicleDriverInput()
{
}

// 0x00B634D0 (hkpvehicledefaultanalogdriverinput.h)
hkpVehicleDriverInputAnalogStatus::hkpVehicleDriverInputAnalogStatus()
{
}

// 0x00B634E0
hkpVehicleDriverInputAnalogStatus::~hkpVehicleDriverInputAnalogStatus()
{
}

// 0x00F7E0C0
hkpVehicleDefaultAnalogDriverInput::hkpVehicleDefaultAnalogDriverInput()
{
    m_slopeChangePointX = 0.0f;
    m_initialSlope = 0.0f;
    m_deadZone = 0.0f;
    m_autoReverse = false;
}

// 0x00F7E280: the copied fields are written even if the allocation failed, as in rfg.exe
hkpVehicleDriverInputStatus* hkpVehicleDriverInputAnalogStatus::clone() const
{
    hkpVehicleDriverInputAnalogStatus* status = new hkpVehicleDriverInputAnalogStatus();
    status->m_positionX = m_positionX;
    status->m_positionY = m_positionY;
    status->m_handbrakeButtonPressed = m_handbrakeButtonPressed;
    status->m_reverseButtonPressed = m_reverseButtonPressed;
    return status;
}

// 0x00F7E0F0
float hkpVehicleDefaultAnalogDriverInput::calcAcceleratorInput(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInputAnalogStatus* deviceStatus, hkpVehicleDriverInput::FilteredDriverInputOutput& input) const
{
    const bool reversing = vehicle->m_isReversing.m_bool != 0;
    const bool auto_reverse = m_autoReverse.m_bool != 0;
    const float y = (reversing && auto_reverse) ? -deviceStatus->m_positionY : deviceStatus->m_positionY;
    if (!auto_reverse && reversing)
    {
        return 1.0f;
    }
    if (0.0f < y)
    {
        return 0.0f;
    }
    return y * -1.0f;
}

// 0x00F7E150
float hkpVehicleDefaultAnalogDriverInput::calcBrakeInput(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInputAnalogStatus* deviceStatus, hkpVehicleDriverInput::FilteredDriverInputOutput& FilteredDriverInputOutputOut) const
{
    const float y = (vehicle->m_isReversing.m_bool != 0 && m_autoReverse.m_bool != 0) ? -deviceStatus->m_positionY : deviceStatus->m_positionY;
    if (y < 0.0f)
    {
        return 0.0f;
    }
    return y;
}

// 0x00F7E1A0: two slopes, the second reaching 1 at full deflection
float hkpVehicleDefaultAnalogDriverInput::calcSteeringInput(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInputAnalogStatus* deviceStatus, hkpVehicleDriverInput::FilteredDriverInputOutput& FilteredDriverInputOutputOut) const
{
    const float x = fabsf(deviceStatus->m_positionX);
    if (x < m_deadZone)
    {
        return 0.0f;
    }
    const float sign = deviceStatus->m_positionX <= 0.0f ? -1.0f : 1.0f;
    if (x < m_slopeChangePointX)
    {
        return (x - m_deadZone) * m_initialSlope * sign;
    }
    const float at_change = m_initialSlope * (m_slopeChangePointX - m_deadZone);
    return (at_change + (x - m_slopeChangePointX) * ((1.0f - at_change) / ((1.0f - m_deadZone) - (m_slopeChangePointX - m_deadZone)))) * sign;
}

// 0x00F7E2D0
hkBool hkpVehicleDefaultAnalogDriverInput::calcTryingToReverse(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInputAnalogStatus* deviceStatus, hkpVehicleDriverInput::FilteredDriverInputOutput& FilteredDriverInputOutputOut)
{
    if (m_autoReverse.m_bool == 0)
    {
        return deviceStatus->m_reverseButtonPressed;
    }

    // The velocity relative to the first wheel's movable contact body.
    const hkpRigidBody* chassis = vehicle->getChassis();
    hkVector4f velocity = chassis->getLinearVelocity();
    for (int w = 0; w < vehicle->m_wheelsInfo.getSize(); w++)
    {
        const hkpVehicleInstance::WheelInfo& wheel = vehicle->m_wheelsInfo[w];
        hkpRigidBody* body = wheel.m_contactBody;
        // 5: hkpMotion::MOTION_FIXED
        if (body != NULL && body->getMotion()->m_type.m_storage != 5)
        {
            hkVector4f relative;
            relative.setSub(wheel.m_contactPoint.getPosition(), body->getCenterOfMassInWorld());
            hkVector4f point_velocity;
            point_velocity.setCross(body->getAngularVelocity(), relative);
            point_velocity.add(body->getLinearVelocity());
            velocity.sub(point_velocity);
            break;
        }
    }

    const float speed = velocity.length<3>().getReal();
    bool going_back = false;
    // 0.1: the stick's dead zone
    if (deviceStatus->m_positionY > 0.1f)
    {
        going_back = true;
        const float eps = hkSimdFloat32::getConstant<HK_QUADREAL_EPS>().getReal();
        if (eps < speed)
        {
            hkVector4f forward;
            forward.setRotatedDir(chassis->getTransform().getRotation(), vehicle->m_data->m_chassisOrientation.getColumn<1>());
            hkVector4f a;
            a.setMul(velocity.lengthInverse<3>(), velocity);
            hkVector4f b;
            b.setMul(forward.lengthInverse<3>(), forward);
            if (a.dot<3>(b).getReal() < eps)
            {
                return true;
            }
        }
    }

    if (vehicle->m_isReversing.m_bool == 0)
    {
        if (speed >= VEHICLE_REVERSE_SWITCH_SPEED || !going_back)
        {
            return false;
        }
    }
    else if (speed < VEHICLE_REVERSE_SWITCH_SPEED)
    {
        return going_back;
    }
    return true;
}

// 0x00F7DFC0
void hkpVehicleDefaultAnalogDriverInput::calcDriverInput(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInputStatus* deviceStatus, hkpVehicleDriverInput::FilteredDriverInputOutput& filteredInputOut)
{
    const hkpVehicleDriverInputAnalogStatus* status = static_cast<const hkpVehicleDriverInputAnalogStatus*>(deviceStatus);
    filteredInputOut.m_acceleratorPedalInput = calcAcceleratorInput(deltaTime, vehicle, status, filteredInputOut);
    filteredInputOut.m_brakePedalInput = calcBrakeInput(deltaTime, vehicle, status, filteredInputOut);
    filteredInputOut.m_steeringWheelInput = calcSteeringInput(deltaTime, vehicle, status, filteredInputOut);
    filteredInputOut.m_handbrakeOn = status->m_handbrakeButtonPressed;
    filteredInputOut.m_tryingToReverse = calcTryingToReverse(deltaTime, vehicle, status, filteredInputOut);
}

// 0x00B63150 (.h:25)
hkpVehicleDriverInputStatus::hkpVehicleDriverInputStatus()
{
}

// 0x00B63160 (.h:26)
hkpVehicleDriverInputStatus::~hkpVehicleDriverInputStatus()
{
}

// 0x00B63B80
hkpVehicleDriverInput::FilteredDriverInputOutput::FilteredDriverInputOutput()
{
}

// 0x00B63170
hkpVehicleDriverInput::~hkpVehicleDriverInput()
{
}

// 0x00B634F0
hkpVehicleDefaultAnalogDriverInput::~hkpVehicleDefaultAnalogDriverInput()
{
}
