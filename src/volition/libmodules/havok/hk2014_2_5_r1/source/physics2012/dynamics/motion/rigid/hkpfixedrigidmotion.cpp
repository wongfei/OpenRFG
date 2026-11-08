// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpfixedrigidmotion.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the fixed motion (a keyframed motion whose velocities stay zero), from
// rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpfixedrigidmotion.h"

// 0x00E14F60
hkpFixedRigidMotion::hkpFixedRigidMotion(const hkVector4f& position, const hkQuaternionf& rotation)
    : hkpKeyframedRigidMotion(position, rotation)
{
    m_type.m_storage = MOTION_FIXED;
}

// 0x00DDED90 (hkpfixedrigidmotion.h)
hkpFixedRigidMotion::hkpFixedRigidMotion(hkFinishLoadedObjectFlag flag)
    : hkpKeyframedRigidMotion(flag)
{
}

// 0x00DDEDC0
hkpFixedRigidMotion::~hkpFixedRigidMotion()
{
}

// 0x00E14F30, 0x00E14F40, 0x00E14F50: ignored
void hkpFixedRigidMotion::setStepPosition(float position, float timestep)
{
}

void hkpFixedRigidMotion::setLinearVelocity(const hkVector4f& newVel)
{
}

void hkpFixedRigidMotion::setAngularVelocity(const hkVector4f& newVel)
{
}

// 0x00E14F90: the motion state with zero velocities
void hkpFixedRigidMotion::getPositionAndVelocities(hkpMotion* motionOut)
{
    motionOut->m_motionState = m_motionState;
    motionOut->m_linearVelocity.setZero();
    motionOut->m_angularVelocity.setZero();
}
