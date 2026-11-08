// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/hkpresponsemodifier.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkVector4f;
class hkpConstraintInstance;
class hkpConstraintOwner;
class hkpDynamicsContactMgr;
class hkpRigidBody;

// size 0x1
class hkpResponseModifier
{
public:
    enum Flags
    {
        MASS_SCALING = 1,
        CENTER_OF_MASS_DISPLACEMENT = 2,
        SURFACE_VELOCITY = 4,
        IMPULSE_SCALING = 8,
        VISCOUS_SURFACE = 16,
        ADDITIONAL_SIZE_MODIFIERS = 7,
    };

    static void setInvMassScalingForContact(hkpDynamicsContactMgr* manager, hkpRigidBody* body, hkpConstraintOwner& constraintOwner, const hkVector4f& factor); // 0x00E04580
    static void setInvMassScalingForContact(hkpDynamicsContactMgr* manager, hkpRigidBody* bodyA, hkpRigidBody* bodyB, hkpConstraintOwner& constraintOwner, const hkVector4f& factorA, const hkVector4f& factorB); // 0x00E04470
    static void setCenterOfMassDisplacementForContact(hkpDynamicsContactMgr* manager, hkpRigidBody* bodyA, hkpRigidBody* bodyB, hkpConstraintOwner& constraintOwner, const hkVector4f& displacementA, const hkVector4f& displacementB); // 0x00E04710
    static void setImpulseScalingForContact(hkpDynamicsContactMgr* manager, hkpRigidBody* bodyA, hkpRigidBody* bodyB, hkpConstraintOwner& constraintOwner, float usedImpulseFraction, float maxAcceleration); // 0x00E04820
    static void setSurfaceVelocity(hkpDynamicsContactMgr* manager, hkpRigidBody* body, hkpConstraintOwner& constraintOwner, const hkVector4f& velWorld); // 0x00E04940
    static void clearSurfaceVelocity(hkpDynamicsContactMgr* manager, hkpConstraintOwner& constraintOwner, hkpRigidBody* body); // 0x00E04310
    static void setLowSurfaceViscosity(hkpDynamicsContactMgr* manager, hkpConstraintOwner& constraintOwner); // 0x00E04A70
    static void disableConstraint(hkpConstraintInstance* instance, hkpConstraintOwner& constraintOwner); // 0x00E04B40
    static void enableConstraint(hkpConstraintInstance* instance, hkpConstraintOwner& constraintOwner); // 0x00E043A0
    static unsigned short getAdditionalSchemaSize(unsigned char flags); // 0x00E0EDF0

    static const unsigned short tableOfAdditionalSchemaSizes[]; // 0x013621D0
};
