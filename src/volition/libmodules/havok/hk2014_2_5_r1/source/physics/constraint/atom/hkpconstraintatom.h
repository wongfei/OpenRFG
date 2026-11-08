// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/atom/hkpconstraintatom.h
//
// Seam only (Havok 2014.2.5). Hand-maintained since HK8 (havok_impl.md): the constructor and the type helpers. MP7
// (multiplayer.md) added the atoms of the ragdoll, limited hinge and ball-and-socket constraint data, laid out as in
// rfg.pdb (2014.2.5 adds the damping factors and the angular limit's cosine axis to the 2014.1 header); the prismatic
// constraint data's linear and angular atoms came with stuck projectiles (projectile::attach).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkufloat8.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"

class hkClass;
class hkpConstraintMotor;

// size 0x2
struct hkpConstraintAtom
{
    enum AtomType
    {
        TYPE_INVALID = 0,
        TYPE_BRIDGE = 1,
        TYPE_SET_LOCAL_TRANSFORMS = 2,
        TYPE_SET_LOCAL_TRANSLATIONS = 3,
        TYPE_SET_LOCAL_ROTATIONS = 4,
        TYPE_BALL_SOCKET = 5,
        TYPE_STIFF_SPRING = 6,
        TYPE_LIN = 7,
        TYPE_LIN_SOFT = 8,
        TYPE_LIN_LIMIT = 9,
        TYPE_LIN_FRICTION = 10,
        TYPE_LIN_MOTOR = 11,
        TYPE_2D_ANG = 12,
        TYPE_ANG = 13,
        TYPE_ANG_LIMIT = 14,
        TYPE_TWIST_LIMIT = 15,
        TYPE_CONE_LIMIT = 16,
        TYPE_ANG_FRICTION = 17,
        TYPE_ANG_MOTOR = 18,
        TYPE_RAGDOLL_MOTOR = 19,
        TYPE_PULLEY = 20,
        TYPE_RACK_AND_PINION = 21,
        TYPE_COG_WHEEL = 22,
        TYPE_SETUP_STABILIZATION = 23,
        TYPE_3D_ANG = 24,
        TYPE_DEFORMABLE_3D_LIN = 25,
        TYPE_DEFORMABLE_3D_ANG = 26,
        TYPE_OVERWRITE_PIVOT = 27,
        TYPE_WHEEL_FRICTION = 28,
        TYPE_ELLIPTICAL_LIMIT = 29,
        TYPE_CONTACT = 30,
        FIRST_MODIFIER_TYPE = 31,
        TYPE_MODIFIER_SOFT_CONTACT = 31,
        TYPE_MODIFIER_MASS_CHANGER = 32,
        TYPE_MODIFIER_VISCOUS_SURFACE = 33,
        TYPE_MODIFIER_MOVING_SURFACE = 34,
        TYPE_MODIFIER_IGNORE_CONSTRAINT = 35,
        TYPE_MODIFIER_CENTER_OF_MASS_CHANGER = 36,
        LAST_MODIFIER_TYPE = 36,
        TYPE_MAX = 37,
    };

    enum CallbackRequest
    {
        CALLBACK_REQUEST_NONE = 0,
        CALLBACK_REQUEST_NEW_CONTACT_POINT = 1,
        CALLBACK_REQUEST_SETUP_PPU_ONLY = 2,
        CALLBACK_REQUEST_SETUP_CALLBACK = 4,
        CALLBACK_REQUEST_CONTACT_POINT_CALLBACK = 8,
    };

    enum SolvingMethod
    {
        METHOD_STABILIZED = 0,
        METHOD_OLD = 1,
    };

    static const hkClass& staticClass(); // 0x01038850
public:
    hkpConstraintAtom();
public:
    hkpConstraintAtom(AtomType type) { m_type = type; }
public:
    hkpConstraintAtom(hkFinishLoadedObjectFlag f) {} // 0x00D6D170
    AtomType getType() const { return m_type; } // 0x00DEB330
    int isModifierType() const { return m_type >= FIRST_MODIFIER_TYPE; } // 0x00DEB340

    hkEnum<enum AtomType,unsigned short> m_type; // 0x00
};

// size 0x10: the arms and the impulse limits of the stabilized atoms that follow
struct hkpSetupStabilizationAtom : public hkpConstraintAtom
{
    hkpSetupStabilizationAtom() : hkpConstraintAtom(TYPE_SETUP_STABILIZATION) {}
    hkpSetupStabilizationAtom(hkFinishLoadedObjectFlag f) : hkpConstraintAtom(f) {} // 2014.1 hkpConstraintAtom.h

    hkBool m_enabled; // 0x02
    unsigned char m_padding[1]; // 0x03
    float m_maxLinImpulse; // 0x04
    float m_maxAngImpulse; // 0x08
    float m_maxAngle; // 0x0C
};

// size 0x10: the pivots stay together (3 solver results)
struct hkpBallSocketConstraintAtom : public hkpConstraintAtom
{
    hkpBallSocketConstraintAtom() : hkpConstraintAtom(TYPE_BALL_SOCKET) {}
    hkpBallSocketConstraintAtom(hkFinishLoadedObjectFlag f) : hkpConstraintAtom(f) {} // 2014.1 hkpConstraintAtom.h

    hkEnum<SolvingMethod,unsigned char> m_solvingMethod; // 0x02
    unsigned char m_bodiesToNotify; // 0x03
    hkUFloat8 m_velocityStabilizationFactor; // 0x04
    hkBool m_enableLinearImpulseLimit; // 0x05
    float m_breachImpulse; // 0x08
    float m_inertiaStabilizationFactor; // 0x0C
};

// size 0x90: the constraint spaces and pivots in the bodies' spaces
struct hkpSetLocalTransformsConstraintAtom : public hkpConstraintAtom
{
    hkpSetLocalTransformsConstraintAtom() : hkpConstraintAtom(TYPE_SET_LOCAL_TRANSFORMS) {}
    hkpSetLocalTransformsConstraintAtom(hkFinishLoadedObjectFlag f) : hkpConstraintAtom(f) {} // 2014.1 hkpConstraintAtom.h

    hkTransformf m_transformA; // 0x10
    hkTransformf m_transformB; // 0x50
};

// size 0x30: the pivots only
struct hkpSetLocalTranslationsConstraintAtom : public hkpConstraintAtom
{
    hkpSetLocalTranslationsConstraintAtom() : hkpConstraintAtom(TYPE_SET_LOCAL_TRANSLATIONS) {}
    hkpSetLocalTranslationsConstraintAtom(hkFinishLoadedObjectFlag f) : hkpConstraintAtom(f) {} // 2014.1 hkpConstraintAtom.h

    hkVector4f m_translationA; // 0x10
    hkVector4f m_translationB; // 0x20
};

// size 0x10: rotation only around one axis of B's constraint space (2 solver results)
struct hkp2dAngConstraintAtom : public hkpConstraintAtom
{
    hkp2dAngConstraintAtom() : hkpConstraintAtom(TYPE_2D_ANG) {}
    hkp2dAngConstraintAtom(hkFinishLoadedObjectFlag f) : hkpConstraintAtom(f) {} // 2014.1 hkpConstraintAtom.h

    unsigned char m_freeRotationAxis; // 0x02
    unsigned char m_padding[13]; // 0x03
};

// size 0x20: an angle range around one axis (1 solver result)
struct hkpAngLimitConstraintAtom : public hkpConstraintAtom
{
    hkpAngLimitConstraintAtom() : hkpConstraintAtom(TYPE_ANG_LIMIT) {}
    hkpAngLimitConstraintAtom(hkFinishLoadedObjectFlag f) : hkpConstraintAtom(f) {} // 2014.1 hkpConstraintAtom.h

    unsigned char m_isEnabled; // 0x02
    unsigned char m_limitAxis; // 0x03
    unsigned char m_cosineAxis; // 0x04
    unsigned char m_padding2[3]; // 0x05
    float m_minAngle; // 0x08
    float m_maxAngle; // 0x0C
    float m_angularLimitsTauFactor; // 0x10
    float m_angularLimitsDampFactor; // 0x14
    unsigned char m_padding[8]; // 0x18
};

// size 0x20: the twist range (1 solver result)
struct hkpTwistLimitConstraintAtom : public hkpConstraintAtom
{
    hkpTwistLimitConstraintAtom() : hkpConstraintAtom(TYPE_TWIST_LIMIT) {}
    hkpTwistLimitConstraintAtom(hkFinishLoadedObjectFlag f) : hkpConstraintAtom(f) {} // 2014.1 hkpConstraintAtom.h

    unsigned char m_isEnabled; // 0x02
    unsigned char m_twistAxis; // 0x03
    unsigned char m_refAxis; // 0x04
    float m_minAngle; // 0x08
    float m_maxAngle; // 0x0C
    float m_angularLimitsTauFactor; // 0x10
    float m_angularLimitsDampFactor; // 0x14
    unsigned char m_padding[8]; // 0x18
};

// size 0x20: the angle between two axes (1 solver result)
struct hkpConeLimitConstraintAtom : public hkpConstraintAtom
{
    enum MeasurementMode
    {
        ZERO_WHEN_VECTORS_ALIGNED = 0,
        ZERO_WHEN_VECTORS_PERPENDICULAR = 1,
    };

    hkpConeLimitConstraintAtom() : hkpConstraintAtom(TYPE_CONE_LIMIT) {}
    hkpConeLimitConstraintAtom(hkFinishLoadedObjectFlag f) : hkpConstraintAtom(f) {} // 2014.1 hkpConstraintAtom.h

    unsigned char m_isEnabled; // 0x02
    unsigned char m_twistAxisInA; // 0x03
    unsigned char m_refAxisInB; // 0x04
    hkEnum<MeasurementMode,unsigned char> m_angleMeasurementMode; // 0x05
    unsigned char m_memOffsetToAngleOffset; // 0x06: from the instance runtime; 0 or 1 when not stabilized
    float m_minAngle; // 0x08
    float m_maxAngle; // 0x0C
    float m_angularLimitsTauFactor; // 0x10
    float m_angularLimitsDampFactor; // 0x14
    unsigned char m_padding[8]; // 0x18
};

// size 0x10: friction around 1 to 3 axes of A's constraint space (2 solver results each)
struct hkpAngFrictionConstraintAtom : public hkpConstraintAtom
{
    hkpAngFrictionConstraintAtom() : hkpConstraintAtom(TYPE_ANG_FRICTION) {}
    hkpAngFrictionConstraintAtom(hkFinishLoadedObjectFlag f) : hkpConstraintAtom(f) {} // 2014.1 hkpConstraintAtom.h

    unsigned char m_isEnabled; // 0x02
    unsigned char m_firstFrictionAxis; // 0x03
    unsigned char m_numFrictionAxes; // 0x04
    float m_maxFrictionTorque; // 0x08
    unsigned char m_padding[4]; // 0x0C
};

// size 0x20: a motor around one axis (2 solver results)
struct hkpAngMotorConstraintAtom : public hkpConstraintAtom
{
    hkpAngMotorConstraintAtom() : hkpConstraintAtom(TYPE_ANG_MOTOR) {}
    hkpAngMotorConstraintAtom(hkFinishLoadedObjectFlag f) : hkpConstraintAtom(f), m_initializedOffset(-1), m_previousTargetAngleOffset(-1), m_correspondingAngLimitSolverResultOffset(-1) {} // 2014.1 hkpConstraintAtom.h

    hkBool m_isEnabled; // 0x02
    unsigned char m_motorAxis; // 0x03
    short m_initializedOffset; // 0x04
    short m_previousTargetAngleOffset; // 0x06
    hkpConstraintMotor* m_motor; // 0x08
    unsigned int m_motorPadding; // 0x0C: the pointer is 8 bytes wide in the SDK (HK_CPU_PTR)
    float m_targetAngle; // 0x10
    short m_correspondingAngLimitSolverResultOffset; // 0x14
    unsigned char m_padding[10]; // 0x16
};

// size 0x10: the pivots stay apart by nothing along one axis of B's constraint space (1 solver result; prismatic)
struct hkpLinConstraintAtom : public hkpConstraintAtom
{
    hkpLinConstraintAtom() : hkpConstraintAtom(TYPE_LIN) {}
    hkpLinConstraintAtom(hkFinishLoadedObjectFlag f) : hkpConstraintAtom(f) {} // 2014.1 hkpConstraintAtom.h

    unsigned char m_axisIndex; // 0x02
    unsigned char m_padding[13]; // 0x03
};

// size 0x10: the pivots' separation along one axis of B's constraint space within [m_min, m_max] (1 solver result)
struct hkpLinLimitConstraintAtom : public hkpConstraintAtom
{
    hkpLinLimitConstraintAtom() : hkpConstraintAtom(TYPE_LIN_LIMIT) {}
    hkpLinLimitConstraintAtom(hkFinishLoadedObjectFlag f) : hkpConstraintAtom(f) {} // 2014.1 hkpConstraintAtom.h

    unsigned char m_axisIndex; // 0x02
    float m_min; // 0x04
    float m_max; // 0x08
    unsigned char m_padding[4]; // 0x0C
};

// size 0x10: friction along one axis of B's constraint space (2 solver results)
struct hkpLinFrictionConstraintAtom : public hkpConstraintAtom
{
    hkpLinFrictionConstraintAtom() : hkpConstraintAtom(TYPE_LIN_FRICTION) {}
    hkpLinFrictionConstraintAtom(hkFinishLoadedObjectFlag f) : hkpConstraintAtom(f) {} // 2014.1 hkpConstraintAtom.h

    unsigned char m_isEnabled; // 0x02
    unsigned char m_frictionAxis; // 0x03
    float m_maxFrictionForce; // 0x04
    unsigned char m_padding[8]; // 0x08
};

// size 0x20: a motor along one axis (2 solver results)
struct hkpLinMotorConstraintAtom : public hkpConstraintAtom
{
    hkpLinMotorConstraintAtom() : hkpConstraintAtom(TYPE_LIN_MOTOR) {}
    hkpLinMotorConstraintAtom(hkFinishLoadedObjectFlag f) : hkpConstraintAtom(f), m_initializedOffset(-1), m_previousTargetPositionOffset(-1) {} // 2014.1 hkpConstraintAtom.h

    hkBool m_isEnabled; // 0x02
    unsigned char m_motorAxis; // 0x03
    short m_initializedOffset; // 0x04
    short m_previousTargetPositionOffset; // 0x06
    hkpConstraintMotor* m_motor; // 0x08
    unsigned int m_motorPadding; // 0x0C: the pointer is 8 bytes wide in the SDK (HK_CPU_PTR)
    float m_targetPosition; // 0x10
    unsigned char m_padding[12]; // 0x14
};

// size 0x10: no rotation around 1 to 3 axes (1 solver result each). 2014.2.5 lists the axes (2014.1 has a first axis
// and a count).
struct hkpAngConstraintAtom : public hkpConstraintAtom
{
    hkpAngConstraintAtom() : hkpConstraintAtom(TYPE_ANG) {}
    hkpAngConstraintAtom(hkFinishLoadedObjectFlag f) : hkpConstraintAtom(f) {} // 2014.1 hkpConstraintAtom.h

    unsigned char m_constrainedAxes[3]; // 0x02
    unsigned char m_numConstrainedAxes; // 0x05
    unsigned char m_padding[10]; // 0x06
};

// size 0x50: three motors towards a target orientation (6 solver results)
struct hkpRagdollMotorConstraintAtom : public hkpConstraintAtom
{
    hkpRagdollMotorConstraintAtom() : hkpConstraintAtom(TYPE_RAGDOLL_MOTOR) {}
    hkpRagdollMotorConstraintAtom(hkFinishLoadedObjectFlag f) : hkpConstraintAtom(f), m_initializedOffset(-1), m_previousTargetAnglesOffset(-1) {} // 2014.1 hkpConstraintAtom.h

    hkBool m_isEnabled; // 0x02
    short m_initializedOffset; // 0x04
    short m_previousTargetAnglesOffset; // 0x06
    hkMatrix3f m_target_bRca; // 0x10
    hkpConstraintMotor* m_motors[3]; // 0x40
};
