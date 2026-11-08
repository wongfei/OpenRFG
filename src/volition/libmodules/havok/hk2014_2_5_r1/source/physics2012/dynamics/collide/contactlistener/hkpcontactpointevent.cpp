// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/hkpcontactpointevent.inl
//
// Havok 2014.2.5 (havok_impl.md HK9): the out-of-line copy of the event's separating velocity, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/hkpcontactpointevent.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpsimplecontactconstraintutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"

// 0x00E03C00: the stored separating velocity, or the one of the bodies' current velocities about their end-of-step
// centers of mass
float hkpContactPointEvent::getSeparatingVelocity() const
{
    if (m_separatingVelocity != nullptr)
    {
        return *m_separatingVelocity;
    }
    const hkpRigidBody* bodyA = m_bodies[0];
    const hkpRigidBody* bodyB = m_bodies[1];
    return hkpSimpleContactConstraintUtil::calculateSeparatingVelocity(bodyA, bodyB, bodyA->m_motion.m_motionState.m_sweptTransform.m_centerOfMass1, bodyB->m_motion.m_motionState.m_sweptTransform.m_centerOfMass1, m_contactPoint).getReal();
}
