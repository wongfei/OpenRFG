// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/steering/default/hkpvehicledefaultsteering.cpp
//
// Havok 2014.2.5 (stub_impl.md S8): the steering angle, reduced with the square of the speed above a limit, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/steering/default/hkpvehicledefaultsteering.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicledata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicleinstance.h"

// 0x00F83AA0 (hkpvehiclesteering.h:57)
hkpVehicleSteering::hkpVehicleSteering()
{
}

// 0x00F83A70
hkpVehicleDefaultSteering::hkpVehicleDefaultSteering()
{
    m_maxSteeringAngle = 0.0f;
    m_maxSpeedFullSteeringAngle = 0.0f;
}

// 0x00F839C0
void hkpVehicleDefaultSteering::calcMainSteeringAngle(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInput::FilteredDriverInputOutput& filteredInfoOutput, hkpVehicleSteering::SteeringAnglesOutput& steeringOutput)
{
    const hkpRigidBody* chassis = vehicle->getChassis();
    const float angle = filteredInfoOutput.m_steeringWheelInput * m_maxSteeringAngle;
    steeringOutput.m_mainSteeringAngle = angle;
    steeringOutput.m_mainSteeringAngleAssumingNoReduction = angle;

    hkVector4f forward;
    forward.setRotatedDir(chassis->getTransform().getRotation(), vehicle->m_data->m_chassisOrientation.getColumn<1>());
    const hkSimdFloat32 speed = chassis->getLinearVelocity().dot<3>(forward);
    if (m_maxSpeedFullSteeringAngle < speed.getReal())
    {
        hkSimdFloat32 inv;
        inv.setReciprocal(speed);
        const float ratio = inv.getReal() * m_maxSpeedFullSteeringAngle;
        steeringOutput.m_mainSteeringAngle = ratio * ratio * steeringOutput.m_mainSteeringAngle;
    }
}

// 0x00F83960
void hkpVehicleDefaultSteering::calcSteering(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInput::FilteredDriverInputOutput& filteredInfoOutput, hkpVehicleSteering::SteeringAnglesOutput& steeringOutput)
{
    calcMainSteeringAngle(deltaTime, vehicle, filteredInfoOutput, steeringOutput);
    for (int w = 0; w < m_doesWheelSteer.getSize(); w++)
    {
        steeringOutput.m_wheelsSteeringAngle[w] = m_doesWheelSteer[w].m_bool != 0 ? steeringOutput.m_mainSteeringAngle : 0.0f;
    }
}

// 0x00BA7B60
hkpVehicleSteering::SteeringAnglesOutput::SteeringAnglesOutput()
{
}

// 0x00BA7B80
hkpVehicleSteering::SteeringAnglesOutput::~SteeringAnglesOutput()
{
}

// 0x00B631B0
hkpVehicleSteering::~hkpVehicleSteering()
{
}

// 0x00B9B2A0
hkpVehicleDefaultSteering::~hkpVehicleDefaultSteering()
{
}
