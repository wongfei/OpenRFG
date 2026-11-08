// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpsimplecontactconstraintutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkContactPoint;
class hkSimdFloat32;
class hkVector4f;
class hkpRigidBody;

// size 0x1
class hkpSimpleContactConstraintUtil
{
public:
    static hkSimdFloat32 calculateSeparatingVelocity(const hkpRigidBody* bodyA, const hkpRigidBody* bodyB, const hkVector4f& centerOfMassInWorldA, const hkVector4f& centerOfMassInWorldB, const hkContactPoint* cp); // 0x00E2F310

};
