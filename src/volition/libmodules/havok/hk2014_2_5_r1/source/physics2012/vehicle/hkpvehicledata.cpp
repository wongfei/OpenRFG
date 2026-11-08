// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicledata.cpp
//
// Havok 2014.2.5 (stub_impl.md S8): the static description of a vehicle, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicledata.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"

#include <cmath>

// 0x00F81070
hkpVehicleData::hkpVehicleData()
{
    m_numWheels = 0;
    m_chassisOrientation.setZero();
    m_torqueRollFactor = 0.0f;
    m_torquePitchFactor = 0.0f;
    m_extraTorqueFactor = 0.0f;
    m_frictionEqualizer = 0.0f;
    m_chassisFrictionInertiaInvDiag.setZero();
    m_gravity.setZero();
    m_torqueYawFactor = 1.0f;
    m_maxVelocityForPositionalFriction = 10.0f;
    m_chassisUnitInertiaYaw = 1.0f;
    m_chassisUnitInertiaRoll = 1.0f;
    m_chassisUnitInertiaPitch = 1.0f;
    m_normalClippingAngleCos = 0.1f;
    m_alreadyInitialised = false;
    m_maxFrictionSolverMassRatio = 30.0f;
}

// 0x00F813A0
hkpVehicleData::~hkpVehicleData()
{
}

// 0x00F80EE0
void hkpVehicleData::init(const hkArray<hkpVehicleSuspension::SuspensionWheelParameters,hkContainerHeapAllocator>& suspensionWheelParams, hkpRigidBody* chassis)
{
    m_alreadyInitialised = true;

    int num_axles = 0;
    for (int w = 0; w < m_numWheels; w++)
    {
        if (num_axles < m_wheelParams[w].m_axle + 1)
        {
            num_axles = m_wheelParams[w].m_axle + 1;
        }
    }
    if (m_numWheelsPerAxle.getSize() < num_axles)
    {
        m_numWheelsPerAxle.setSize(num_axles);
    }
    for (int a = 0; a < m_numWheelsPerAxle.getSize(); a++)
    {
        m_numWheelsPerAxle[a] = 0;
    }
    for (int w = 0; w < m_numWheels; w++)
    {
        m_numWheelsPerAxle[m_wheelParams[w].m_axle]++;
    }

    // The diagonal of the rotated unit inertia, the absolute value of each rotation term.
    const float yaw = m_torqueYawFactor / m_chassisUnitInertiaYaw;
    const float roll = m_torqueRollFactor / m_chassisUnitInertiaRoll;
    const float pitch = m_torquePitchFactor / m_chassisUnitInertiaPitch;
    const float mass_inv = chassis->getMotion()->m_inertiaAndMassInv(3);
    const hkVector4f& c0 = m_chassisOrientation.getColumn<0>();
    const hkVector4f& c1 = m_chassisOrientation.getColumn<1>();
    const hkVector4f& c2 = m_chassisOrientation.getColumn<2>();
    m_chassisFrictionInertiaInvDiag.set(
        mass_inv * (fabsf(c0(0)) * yaw + fabsf(c1(0)) * roll + fabsf(c2(0)) * pitch),
        mass_inv * (fabsf(c0(1)) * yaw + fabsf(c1(1)) * roll + fabsf(c2(1)) * pitch),
        mass_inv * (fabsf(c0(2)) * yaw + fabsf(c1(2)) * roll + fabsf(c2(2)) * pitch),
        mass_inv * 1.0f);
}
