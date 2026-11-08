// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/aerodynamics/default/hkpvehicledefaultaerodynamics.cpp
//
// Havok 2014.2.5 (stub_impl.md S8): drag along the chassis' forward axis, lift along its up axis, extra gravity, from
// rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/aerodynamics/default/hkpvehicledefaultaerodynamics.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicledata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicleinstance.h"

#include <cmath>

// 0x00F7F620 (hkpvehicleaerodynamics.h:50)
hkpVehicleAerodynamics::hkpVehicleAerodynamics()
{
}

// 0x00F7F640
hkpVehicleDefaultAerodynamics::~hkpVehicleDefaultAerodynamics()
{
}

// 0x00F7F450
hkpVehicleDefaultAerodynamics::hkpVehicleDefaultAerodynamics()
{
    m_airDensity = 0.0f;
    m_frontalArea = 0.0f;
    m_dragCoefficient = 0.0f;
    m_liftCoefficient = 0.0f;
    m_extraGravityws.setZero();
}

// 0x00F7F480
float hkpVehicleDefaultAerodynamics::calcAerodynamicDrag(float forwardVelocity) const
{
    return m_airDensity * -0.5f * m_dragCoefficient * m_frontalArea * fabsf(forwardVelocity) * forwardVelocity;
}

// 0x00F7F400
float hkpVehicleDefaultAerodynamics::calcAerodynamicLift(float forwardVelocity) const
{
    return forwardVelocity * m_airDensity * 0.5f * m_liftCoefficient * m_frontalArea * forwardVelocity;
}

// 0x00F7F4C0: chassis orientation column 0 is up, column 1 forward
void hkpVehicleDefaultAerodynamics::calcAerodynamics(const float deltaTime, const hkpVehicleInstance* vehicle, hkpVehicleAerodynamics::AerodynamicsDragOutput& dragInfoOut)
{
    const hkpRigidBody* chassis = vehicle->getChassis();
    const hkRotationf& rotation = chassis->getTransform().getRotation();
    hkVector4f forward;
    forward._setRotatedDir(rotation, vehicle->m_data->m_chassisOrientation.getColumn<1>());
    hkVector4f up;
    up._setRotatedDir(rotation, vehicle->m_data->m_chassisOrientation.getColumn<0>());

    hkSimdFloat32 velocity;
    velocity.setFromFloat(chassis->getLinearVelocity().dot<3>(forward).getReal());
    const float drag = calcAerodynamicDrag(velocity.getReal());
    const float lift = calcAerodynamicLift(velocity.getReal());

    dragInfoOut.m_aerodynamicsForce.setMul(forward, hkSimdFloat32::fromFloat(drag));
    dragInfoOut.m_aerodynamicsForce.addMul(up, hkSimdFloat32::fromFloat(lift));
    dragInfoOut.m_aerodynamicsForce.addMul(hkSimdFloat32::fromFloat(chassis->getMass()), m_extraGravityws);
    dragInfoOut.m_aerodynamicsTorque.setZero();
}

// 0x00F7F630
hkpVehicleAerodynamics::~hkpVehicleAerodynamics()
{
}
