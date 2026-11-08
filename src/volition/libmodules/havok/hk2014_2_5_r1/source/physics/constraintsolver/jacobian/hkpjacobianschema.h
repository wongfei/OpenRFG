// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/jacobian/hkpjacobianschema.h
//
// Havok 2014.2.5 (havok_impl.md HK8): the solver's schema stream, from rfg.pdb (the SDK keeps these headers private).
// A schema buffer is a sequence of 16-byte aligned records; the low byte of each record's first float is its
// SchemaType. Writers add FLT_EPSILON (1.1920929e-07) to that float before they overwrite the byte (setType), except
// initSetMass, which only writes the byte. The sizes and solver element counts per type are the tables
// s_schemaSize/s_schemaNumSolverElemTemps (255 for types that are not records of their own).
// The schemas a contact constraint and its modifiers write are declared as classes, and since MP7 (multiplayer.md) the
// bilateral, angular, angular limit and angular friction schemas of the ragdoll joints; the motor, chain, stable and
// deformable schemas belong to constraint types no in-scope code creates (havok_impl.md section 2).
// Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/jacobian/hkpjacobianelement.h"

class hkpConstraintInstance;
class hkpSolverResults;

// size 0x1
class hkpJacobianSchema
{
public:
    enum SchemaType
    {
        SCHEMA_TYPE_END = 0,
        SCHEMA_TYPE_HEADER = 1,
        SCHEMA_TYPE_GOTO = 2,
        SCHEMA_TYPE_SHIFT_SOLVER_RESULTS_POINTER = 3,
        SCHEMA_TYPE_SIMPLE_BEGIN = 4,
        SCHEMA_TYPE_1D_BILATERAL = 5,
        SCHEMA_TYPE_1D_BILATERAL_WITH_IMPULSE_LIMIT = 6,
        SCHEMA_TYPE_1D_BILATERAL_USER_TAU = 7,
        SCHEMA_TYPE_1D_LINEAR_LIMITS = 8,
        SCHEMA_TYPE_1D_FRICTION = 9,
        SCHEMA_TYPE_1D_LINEAR_MOTOR = 10,
        SCHEMA_TYPE_1D_PULLEY = 11,
        SCHEMA_TYPE_1D_ANGULAR = 12,
        SCHEMA_TYPE_1D_ANGULAR_LIMITS = 13,
        SCHEMA_TYPE_1D_ANGULAR_FRICTION = 14,
        SCHEMA_TYPE_1D_ANGULAR_MOTOR = 15,
        SCHEMA_TYPE_STABLE_BALLSOCKET = 16,
        SCHEMA_TYPE_NP_STABLE_BALLSOCKET = 17,
        SCHEMA_TYPE_STABLE_STIFFSPRING = 18,
        SCHEMA_TYPE_NP_STABLE_STIFFSPRING = 19,
        SCHEMA_TYPE_SINGLE_CONTACT = 20,
        SCHEMA_TYPE_SINGLE_CONTACT_WITH_ACCESSOR_CHECK = 21,
        SCHEMA_TYPE_SINGLE_LIMIT_CONTACT = 22,
        SCHEMA_TYPE_SINGLE_LIMIT_CONTACT_WITH_ACCESSOR_CHECK = 23,
        SCHEMA_TYPE_PAIR_CONTACT = 24,
        SCHEMA_TYPE_PAIR_CONTACT_WITH_ACCESSOR_CHECK = 25,
        SCHEMA_TYPE_2D_FRICTION = 26,
        SCHEMA_TYPE_3D_FRICTION = 27,
        SCHEMA_TYPE_2D_ROLLING_FRICTION = 28,
        SCHEMA_TYPE_SET_MASS = 29,
        SCHEMA_TYPE_ADD_VELOCITY = 30,
        SCHEMA_TYPE_SET_CENTER_OF_MASS = 31,
        SCHEMA_TYPE_3D_ANGULAR = 32,
        SCHEMA_TYPE_NP_3D_ANGULAR = 33,
        SCHEMA_TYPE_DEFORMABLE_LIN_3D = 34,
        SCHEMA_TYPE_DEFORMABLE_ANG_3D = 35,
        SCHEMA_TYPE_NP_DEFORMABLE_LIN_3D = 36,
        SCHEMA_TYPE_NP_DEFORMABLE_ANG_3D = 37,
        SCHEMA_TYPE_NP_ELLIPTICAL_LIMIT = 38,
        SCHEMA_TYPE_SIMPLE_END = 39,
        SCHEMA_TYPE_STIFF_SPRING_CHAIN = 40,
        SCHEMA_TYPE_BALL_SOCKET_CHAIN = 41,
        SCHEMA_TYPE_STABILIZED_BALL_SOCKET_CHAIN = 42,
        SCHEMA_TYPE_POWERED_CHAIN = 43,
        SCHEMA_TYPE_WHEEL_FRICTION = 44,
        SCHEMA_TYPE_MAX = 45,
    };

    typedef hkEnum<SchemaType, signed char> JointSchemaType;

    static SchemaType getType(const hkpJacobianSchema* schema) { return SchemaType(*reinterpret_cast<const unsigned char*>(schema)); } // 0x00E9BA40
    static int getSize(const hkpJacobianSchema* schema) { return s_schemaSize[*reinterpret_cast<const unsigned char*>(schema)]; } // 0x00E9BA50
    static int getNumSolverElemTemps(const hkpJacobianSchema* schema) { return s_schemaNumSolverElemTemps[*reinterpret_cast<const unsigned char*>(schema)]; } // 0x00EA8830
    // 0x00D73E70: the float the type byte overwrites first gets FLT_EPSILON added (an SSE ADDSS)
    static void setType(hkpJacobianSchema* schema, SchemaType type)
    {
        float* f = reinterpret_cast<float*>(schema);
        *f = *f + 1.1920929e-07f; // 0x34000000, FLT_EPSILON
        *reinterpret_cast<unsigned char*>(schema) = (unsigned char)type;
    }
    static void setTypeAccurate(hkpJacobianSchema* schema, SchemaType type) { *reinterpret_cast<unsigned char*>(schema) = (unsigned char)type; } // 0x00D73E90
    static void clearTypeFromReal(float* real) { *reinterpret_cast<unsigned char*>(real) = 0; } // 0x00EA8FA0
    static void verifySchemaInfoArrays(); // 0x0104B630 (debug check, not reproduced)

    static const unsigned char s_schemaSize[48]; // 0x01A368BC
    static const unsigned char s_schemaNumSolverElemTemps[48]; // 0x01A368EC
};

// The size (Sizeof) and solver element count (Temps) of each schema type, as the PDB's enum constants.
namespace hkpJacobianSchemaInfo
{
    struct AddVelocity { enum { Temps = 0, Sizeof = 32 }; };
    struct Angular1D { enum { Temps = 1, Sizeof = 32 }; };
    struct AngularFriction1D { enum { Temps = 2, Sizeof = 48 }; };
    struct AngularLimits1D { enum { Temps = 1, Sizeof = 48 }; };
    struct AngularMotor1D { enum { Temps = 2, Sizeof = 64 }; };
    struct BallSocketChain { enum { Sizeof = 32 }; };
    struct Bilateral1D { enum { Temps = 1, Sizeof = 48 }; };
    struct BilateralUserTau1D { enum { Temps = 1, Sizeof = 64 }; };
    struct DeformableAngular3D { enum { Temps = 3, Sizeof = 128 }; };
    struct DeformableLinear3D { enum { Temps = 3, Sizeof = 128 }; };
    struct EllipticalLimit { enum { Temps = 2, Sizeof = 112 }; };
    struct End { enum { Sizeof = 16 }; };
    struct Friction1D { enum { Temps = 2, Sizeof = 64 }; };
    struct Friction2D { enum { Temps = 3, Sizeof = 112 }; };
    struct Friction3D { enum { Temps = 4, Sizeof_Ptr4 = 144, Sizeof_Ptr8 = 160, Sizeof = 144 }; };
    struct Goto { enum { Temps = 0, Sizeof = 16 }; };
    struct Header { enum { Sizeof = 16 }; };
    struct LinearLimits1D { enum { Temps = 1, Sizeof = 64 }; };
    struct LinearMotor1D { enum { Temps = 2, Sizeof = 80 }; };
    struct NpDeformableAngular3D { enum { Temps = 3, Sizeof = 192 }; };
    struct NpDeformableLinear3D { enum { Temps = 3, Sizeof = 192 }; };
    struct NpStableAngular3D { enum { Temps = 3, Sizeof = 96 }; };
    struct NpStableBallSocket { enum { Temps = 3, Sizeof = 128 }; };
    struct NpStableStiffSpring { enum { Temps = 1, Sizeof = 128 }; };
    struct PairContact { enum { Temps = 2, Sizeof = 112 }; };
    struct PoweredChain { enum { Sizeof = 32 }; };
    struct Pulley1D { enum { Temps = 1, Sizeof = 64 }; };
    struct RollingFriction2D { enum { Temps = 3, Sizeof_Ptr4 = 80, Sizeof_Ptr8 = 96, Sizeof = 80 }; };
    struct SetCenterOfMass { enum { Temps = 0, Sizeof = 96 }; };
    struct SetMass { enum { Temps = 0, Sizeof = 32 }; };
    struct ShiftSolverResults { enum { Temps = 0, Sizeof = 16 }; };
    struct SingleContact { enum { Temps = 1, Sizeof = 48 }; };
    struct StabilizedBallSocketChain { enum { Sizeof = 48 }; };
    struct StableAngular3D { enum { Temps = 3, Sizeof = 48 }; };
    struct StableBallSocket { enum { Temps = 3, Sizeof = 64 }; };
    struct StableStiffSpring { enum { Temps = 1, Sizeof = 64 }; };
    struct StiffSpringChain { enum { Sizeof = 32 }; };
    struct WheelFriction { enum { Temps = 2, Sizeof = 112 }; };
}

// size 0x10: starts every constraint's schemas (the accumulators and the solver results it writes to)
class hkpJacobianHeaderSchema
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpJacobianHeaderSchema);
    // 0x00E98C00
    void initHeader(hkpConstraintInstance* instance, const int bodyAIndex, const int bodyBIndex, hkpSolverResults* sr, const int solverResultStriding)
    {
        m_bodyAIndex = (unsigned short)bodyAIndex;
        m_bodyBIndex = (unsigned short)bodyBIndex;
        m_solverResultInMainMemory = sr;
        hkpJacobianSchema::setType(reinterpret_cast<hkpJacobianSchema*>(this), hkpJacobianSchema::SCHEMA_TYPE_HEADER);
        m_constraintInstance = instance;
        m_solverResultStriding = (unsigned char)solverResultStriding;
    }
    const hkpVelocityAccumulator* getBodyA(const hkpVelocityAccumulator* accumulatorBuffer) const; // 0x00E9BA60
    hkpVelocityAccumulator* getBodyA(hkpVelocityAccumulator* accumulatorBuffer) const; // 0x00EA8840
    const hkpVelocityAccumulator* getBodyB(const hkpVelocityAccumulator* accumulatorBuffer) const; // 0x00E9BA70
    hkpVelocityAccumulator* getBodyB(hkpVelocityAccumulator* accumulatorBuffer) const; // 0x00EA8850

    unsigned char m_padding0; // 0x00: the schema type
    unsigned char m_solverResultStriding; // 0x01
    unsigned short m_padding1; // 0x02
    unsigned short m_bodyAIndex; // 0x04
    unsigned short m_bodyBIndex; // 0x06
    hkpSolverResults* m_solverResultInMainMemory; // 0x08
    hkpConstraintInstance* m_constraintInstance; // 0x0C
};

// size 0x30: one non-penetration row
class hkpJacobianSingleContactSchema : public hkpJacobianSchemaInfo::SingleContact
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpJacobianSingleContactSchema);
    // 0x00E9B8B0: typeAddon selects the accessor-check variant (SCHEMA_TYPE_SINGLE_CONTACT + 1)
    void initSingleContact(const hkp1Lin2AngJacobian& /*jac*/, int typeAddon)
    {
        hkpJacobianSchema::setType(reinterpret_cast<hkpJacobianSchema*>(this), hkpJacobianSchema::SchemaType(hkpJacobianSchema::SCHEMA_TYPE_SINGLE_CONTACT + typeAddon));
    }

    hkp1Lin2AngJacobian m_jac; // 0x00
};

// size 0x70: two non-penetration rows solved together (m_invJac01 couples them)
class hkpJacobianPairContactSchema : public hkpJacobianSchemaInfo::PairContact
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpJacobianPairContactSchema);
    // 0x00E9B8D0
    void initPairContact(const hkpJacDouble2Bil& /*jac*/, float invJac01, int requestAccessorCheck)
    {
        m_invJac01 = invJac01;
        hkpJacobianSchema::setType(reinterpret_cast<hkpJacobianSchema*>(this), hkpJacobianSchema::SchemaType(hkpJacobianSchema::SCHEMA_TYPE_PAIR_CONTACT + requestAccessorCheck));
    }

    hkpJacDouble2Bil m_jac; // 0x00
    float m_invJac01; // 0x60
    float m_padding[3]; // 0x64
};

// size 0x70
class hkpJacobian2dFrictionSchema : public hkpJacobianSchemaInfo::Friction2D
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpJacobian2dFrictionSchema);
    // 0x00E9B900
    void init2dFriction(float maxImpulse, float invJac01, hkpSolverResults* sr)
    {
        m_invJac01 = invJac01;
        m_maxImpulse = maxImpulse;
        hkpJacobianSchema::setType(reinterpret_cast<hkpJacobianSchema*>(this), hkpJacobianSchema::SCHEMA_TYPE_2D_FRICTION);
        m_solverResults = sr;
    }
    void unpackSolverResultsPtr(hkpSolverResults*& solverResults) const { solverResults = m_solverResults; } // 0x00EB5A90

    hkpJacDouble2Bil m_jac; // 0x00
    float m_invJac01; // 0x60
    float m_maxImpulse; // 0x64
    hkpSolverResults* m_solverResults; // 0x68
    float m_padding55; // 0x6C
};

// size 0x90
class hkpJacobian3dFrictionSchema : public hkpJacobianSchemaInfo::Friction3D
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpJacobian3dFrictionSchema);
    // 0x00E9B940
    void init3dFriction(const hkpJacTriple2Bil1Ang* /*jac*/, float maxImpulse, float invJac01, float contactRadius, hkpSolverResults* sr)
    {
        m_invJac01 = invJac01;
        m_maxImpulse = maxImpulse;
        m_contactRadius = contactRadius;
        hkpJacobianSchema::setType(reinterpret_cast<hkpJacobianSchema*>(this), hkpJacobianSchema::SCHEMA_TYPE_3D_FRICTION);
        m_solverResults = sr;
    }
    void unpackSolverResultsPtr(hkpSolverResults*& solverResults) const { solverResults = m_solverResults; } // 0x00EB5AA0

    hkpJacTriple2Bil1Ang m_jac; // 0x00
    float m_invJac01; // 0x80
    float m_maxImpulse; // 0x84
    float m_contactRadius; // 0x88
    hkpSolverResults* m_solverResults; // 0x8C
};

// size 0x50
class hkpJacobian2dRollingFrictionSchema : public hkpJacobianSchemaInfo::RollingFriction2D
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpJacobian2dRollingFrictionSchema);
    // 0x00E9B990
    void init2dRollingFriction(const hkp2AngJacobian* /*jac*/, float maxImpulse, hkpSolverResults* sr, hkHalf originalRollingFrictionMultiplier)
    {
        m_maxImpulse = maxImpulse;
        m_solverResults = sr;
        m_originalRollingFrictionMultiplier = originalRollingFrictionMultiplier;
        hkpJacobianSchema::setType(reinterpret_cast<hkpJacobianSchema*>(this), hkpJacobianSchema::SCHEMA_TYPE_2D_ROLLING_FRICTION);
    }
    void unpackSolverResultsPtr(hkpSolverResults*& solverResults) const { solverResults = m_solverResults; } // 0x00EB5AB0

    hkp2AngJacobian m_jac0; // 0x00
    hkp2AngJacobian m_jac1; // 0x20
    float m_maxImpulse; // 0x40
    hkpSolverResults* m_solverResults; // 0x44
    hkHalf m_originalRollingFrictionMultiplier; // 0x48
    hkHalf m_padding1; // 0x4A
    unsigned int m_padding2; // 0x4C
};

// size 0x20: replaces the inverse masses of the constraint's two accumulators (mass changer modifier)
class hkpJacobianSetMassSchema : public hkpJacobianSchemaInfo::SetMass
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpJacobianSetMassSchema);
    // 0x00D73FA0: the type byte only (no epsilon)
    void initSetMass(const hkVector4f& invMass0, const hkVector4f& invMass1)
    {
        m_invMass0 = invMass0;
        m_invMass1 = invMass1;
        hkpJacobianSchema::setTypeAccurate(reinterpret_cast<hkpJacobianSchema*>(this), hkpJacobianSchema::SCHEMA_TYPE_SET_MASS);
    }

    hkVector4f m_invMass0; // 0x00
    hkVector4f m_invMass1; // 0x10
};

// size 0x20: adds a velocity to one accumulator (moving surface modifier)
class hkpJacobianAddVelocitySchema : public hkpJacobianSchemaInfo::AddVelocity
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpJacobianAddVelocitySchema);
    // 0x00D73FC0
    void initAddVelocity(const hkVector4f& deltaVel, int bodyIndex)
    {
        m_deltaVel = deltaVel;
        hkpJacobianSchema::setType(reinterpret_cast<hkpJacobianSchema*>(this), hkpJacobianSchema::SCHEMA_TYPE_ADD_VELOCITY);
        m_bodyIndex = bodyIndex;
    }

    hkVector4f m_deltaVel; // 0x00
    int m_bodyIndex; // 0x10
    int m_padding[3]; // 0x14
};

// size 0x60: the angular-to-linear velocity coupling of a moved center of mass (center of mass changer modifier)
class hkpJacobianSetCenterOfMassSchema : public hkpJacobianSchemaInfo::SetCenterOfMass
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpJacobianSetCenterOfMassSchema);
    // 0x00D73FF0
    void initAddAngularVelocity(const hkMatrix3f& angToLinVelA, const hkMatrix3f& angToLinVelB)
    {
        m_angToLinVelA = angToLinVelA;
        m_angToLinVelB = angToLinVelB;
        hkpJacobianSchema::setType(reinterpret_cast<hkpJacobianSchema*>(this), hkpJacobianSchema::SCHEMA_TYPE_SET_CENTER_OF_MASS);
    }

    hkMatrix3f m_angToLinVelA; // 0x00
    hkMatrix3f m_angToLinVelB; // 0x30
};

// size 0x30: one linear row kept at its right hand side (a ball socket's axis, METHOD_OLD); with an impulse limit
// (SCHEMA_TYPE_1D_BILATERAL_WITH_IMPULSE_LIMIT) m_jac's m_angular[1].w holds the breach impulse instead of the diagonal
// (multiplayer.md MP7)
class hkpJacobian1dBilateralSchema : public hkpJacobianSchemaInfo::Bilateral1D
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpJacobian1dBilateralSchema);

    hkp1Lin2AngJacobian m_jac; // 0x00
};

// size 0x40: one linear row between two right hand sides (a prismatic constraint's range); m_jac's lin.w holds the
// separation's right hand side
class hkpJacobian1dLinearLimitsSchema : public hkpJacobianSchemaInfo::LinearLimits1D
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpJacobian1dLinearLimitsSchema);

    hkp1Lin2AngJacobian m_jac; // 0x00
    float m_minRhs; // 0x30
    float m_maxRhs; // 0x34
    unsigned int m_padding[2]; // 0x38
};

// size 0x40: a linear friction row (its impulse limited to m_maxFrictionImpulse; a prismatic constraint's friction)
class hkpJacobian1dFrictionSchema : public hkpJacobianSchemaInfo::Friction1D
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpJacobian1dFrictionSchema);

    hkp1Lin2AngJacobian m_jac; // 0x00
    float m_maxFrictionImpulse; // 0x30
    unsigned int m_padding[3]; // 0x34
};

// size 0x40: an angular motor row (a hinge's velocity motor): the right hand side in m_jac's m_angular[1].w, the
// impulse range by the micro step, the motor's target velocity, tau and damping
class hkpJacobian1dAngularMotorSchema : public hkpJacobianSchemaInfo::AngularMotor1D
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpJacobian1dAngularMotorSchema);

    hkp2AngJacobian m_jac; // 0x00
    float m_maxImpulsePerSubstep; // 0x20
    float m_minImpulsePerSubstep; // 0x24
    float m_velocity; // 0x28
    float m_tau; // 0x2C
    float m_damping; // 0x30
    unsigned int m_padding[3]; // 0x34
};

// size 0x20: one angular row kept at its right hand side (a hinge's axes)
class hkpJacobian1dAngularSchema : public hkpJacobianSchemaInfo::Angular1D
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpJacobian1dAngularSchema);

    hkp2AngJacobian m_jac; // 0x00
};

// size 0x30: an angular row between two right hand sides (twist, cone, plane and hinge limits)
class hkpJacobian1dAngularLimitsSchema : public hkpJacobianSchemaInfo::AngularLimits1D
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpJacobian1dAngularLimitsSchema);

    hkp2AngJacobian m_jac; // 0x00
    float m_minRhs; // 0x20
    float m_maxRhs; // 0x24
    float m_tau; // 0x28
    float m_damping; // 0x2C
};

// size 0x30: an angular friction row (its impulse limited to m_maxImpulse)
class hkpJacobian1dAngularFrictionSchema : public hkpJacobianSchemaInfo::AngularFriction1D
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpJacobian1dAngularFrictionSchema);

    hkp2AngJacobian m_jac; // 0x00
    float m_maxImpulse; // 0x20
    unsigned int m_frictionIndex; // 0x24
    unsigned int m_padding[2]; // 0x28
};

// size 0x10: continues the schema stream at m_offset bytes from this record
class hkpJacobianGotoSchema : public hkpJacobianSchemaInfo::Goto
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpJacobianGotoSchema);
    void initGoto(hkpJacobianSchema* destination) { initOffset(long(reinterpret_cast<char*>(destination) - reinterpret_cast<char*>(this))); } // 0x00E0D380
    // 0x00E2AF80
    void initOffset(long offset)
    {
        hkpJacobianSchema::setType(reinterpret_cast<hkpJacobianSchema*>(this), hkpJacobianSchema::SCHEMA_TYPE_GOTO);
        m_offset = offset;
    }

    unsigned int m_padding0; // 0x00
    unsigned int m_padding1; // 0x04
    long m_offset; // 0x08
    unsigned int m_padding2; // 0x0C
};

// size 0x10: skips solver results (a constraint's results that no schema of this solve writes)
class hkpJacobianShiftSolverResultsPointerSchema : public hkpJacobianSchemaInfo::ShiftSolverResults
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpJacobianShiftSolverResultsPointerSchema);
    // 0x00E98C40: the epsilon is added to the first word as a float
    void initShiftSolverResultsPointer(unsigned char numSolverResultsToSkip)
    {
        hkpJacobianSchema::setType(reinterpret_cast<hkpJacobianSchema*>(this), hkpJacobianSchema::SCHEMA_TYPE_SHIFT_SOLVER_RESULTS_POINTER);
        m_numSolverResultsToSkip = numSolverResultsToSkip;
    }
    int getNumSolverResults() const { return m_numSolverResultsToSkip; } // 0x00EB5A80

    unsigned int m_padding_1; // 0x00
    unsigned char m_numSolverResultsToSkip; // 0x04
    unsigned int m_padding2[2]; // 0x08
};
