// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpsimplecontactconstraintutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK8): the separating velocity of a contact point, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpsimplecontactconstraintutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"

// 0x00E2F310: (velocity of A's point - velocity of B's point) . normal, the arms from the given centers of mass
hkSimdFloat32 hkpSimpleContactConstraintUtil::calculateSeparatingVelocity(const hkpRigidBody* bodyA, const hkpRigidBody* bodyB, const hkVector4f& centerOfMassInWorldA, const hkVector4f& centerOfMassInWorldB, const hkContactPoint* cp)
{
    hkVector4f armA;
    armA.setSub(cp->m_position, centerOfMassInWorldA);
    hkVector4f armB;
    armB.setSub(cp->m_position, centerOfMassInWorldB);
    hkVector4f velA;
    velA.setCross(bodyA->m_motion.m_angularVelocity, armA);
    hkVector4f velB;
    velB.setCross(bodyB->m_motion.m_angularVelocity, armB);
    velB.add(bodyB->m_motion.m_linearVelocity);
    velA.add(bodyA->m_motion.m_linearVelocity);
    hkVector4f relVel;
    relVel.setSub(velA, velB);
    return relVel.dot<3>(cp->m_separatingNormal);
}
