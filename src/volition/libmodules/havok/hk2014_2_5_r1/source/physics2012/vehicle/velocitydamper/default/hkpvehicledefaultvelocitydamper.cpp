// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/velocitydamper/default/hkpvehicledefaultvelocitydamper.cpp
//
// Havok 2014.2.5 (stub_impl.md S8): damps the chassis spin, harder above a threshold, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/velocitydamper/default/hkpvehicledefaultvelocitydamper.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicleinstance.h"

// 0x00F80300 (hkpvehiclevelocitydamper.h:37)
hkpVehicleVelocityDamper::hkpVehicleVelocityDamper()
{
}

// 0x00F80320
hkpVehicleDefaultVelocityDamper::hkpVehicleDefaultVelocityDamper()
{
    m_normalSpinDamping = 0.0f;
    m_collisionSpinDamping = 0.0f;
    m_collisionThreshold = 1.0f;
}

// 0x00F80350
void hkpVehicleDefaultVelocityDamper::applyVelocityDamping(const float deltaTime, hkpVehicleInstance& vehicle)
{
    hkpEntity* chassis = vehicle.m_entity;
    hkpMotion* motion = chassis->getMotion();
    const hkVector4f spin = motion->m_angularVelocity;
    const float damping = spin.lengthSquared<3>().getReal() > m_collisionThreshold * m_collisionThreshold ? m_collisionSpinDamping : m_normalSpinDamping;
    const float factor = 1.0f - damping * deltaTime;

    hkVector4f damped;
    damped.setMul(spin, hkSimdFloat32::fromFloat(factor > 0.0f ? factor : 0.0f));
    chassis->activate();
    motion->setAngularVelocity(damped);
}

// 0x00F80310
hkpVehicleVelocityDamper::~hkpVehicleVelocityDamper()
{
}

// 0x00F80460
hkpVehicleDefaultVelocityDamper::~hkpVehicleDefaultVelocityDamper()
{
}
