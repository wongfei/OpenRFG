// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/limitedhinge/hkplimitedhingeconstraintdata.h
//
// Havok 2014.2.5 (multiplayer.md MP7): the hinge with an angle range (a ragdoll's knees and elbows), from rfg.pdb.
// Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/atom/hkpconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverresults.h"

// size 0x120
class hkpLimitedHingeConstraintData : public hkpConstraintData
{
public:
    enum
    {
        SOLVER_RESULT_MOTOR = 0,
        SOLVER_RESULT_MOTOR_INTERNAL = 1,
        SOLVER_RESULT_FRICTION = 2,
        SOLVER_RESULT_FRICTION_INTERNAL = 3,
        SOLVER_RESULT_LIMIT = 4,
        SOLVER_RESULT_ANG_0 = 5,
        SOLVER_RESULT_ANG_1 = 6,
        SOLVER_RESULT_LIN_0 = 7,
        SOLVER_RESULT_LIN_1 = 8,
        SOLVER_RESULT_LIN_2 = 9,
        SOLVER_RESULT_MAX = 10,
    };

    // size 0x60 (16-byte aligned in the binary)
    struct alignas(16) Runtime
    {
        hkpSolverResults m_solverResults[10]; // 0x00
        unsigned char m_initialized; // 0x50
        float m_previousTargetAngle; // 0x54
    };

    // size 0x110
    struct Atoms
    {
        enum Axis
        {
            AXIS_AXLE = 0,
            AXIS_PERP_TO_AXLE_1 = 1,
            AXIS_PERP_TO_AXLE_2 = 2,
        };

        Atoms(); // 0x00D6D390
        const hkpConstraintAtom* getAtoms() const { return &m_transforms; } // 0x00D6D0E0

        hkpSetLocalTransformsConstraintAtom m_transforms; // 0x00
        hkpSetupStabilizationAtom m_setupStabilization; // 0x90
        hkpAngMotorConstraintAtom m_angMotor; // 0xA0
        hkpAngFrictionConstraintAtom m_angFriction; // 0xC0
        hkpAngLimitConstraintAtom m_angLimit; // 0xD0
        hkp2dAngConstraintAtom m_2dAng; // 0xF0
        hkpBallSocketConstraintAtom m_ballSocket; // 0x100
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new 0x00815F10, delete(void* p, unsigned int nbytes) 0x00D6D4A0
    hkpLimitedHingeConstraintData(); // 0x00D6CA10
    virtual ~hkpLimitedHingeConstraintData(); // vtbl+0x00 0x00D6CB50
    void setInWorldSpace(const hkTransformf& bodyATransform, const hkTransformf& bodyBTransform, const hkVector4f& pivot, const hkVector4f& axisIn); // 0x00D6CC50
    void setInBodySpace(const hkVector4f& pivotA, const hkVector4f& pivotB, const hkVector4f& axisA, const hkVector4f& axisB, const hkVector4f& axisAPerp, const hkVector4f& axisBPerp); // 0x00D6CE30
    static Runtime* getRuntime(hkpConstraintRuntime* runtime) { return reinterpret_cast<Runtime*>(runtime); } // 0x00D6D100
    void setMotor(hkpConstraintMotor* motor); // 0x00D6CB70
    void setMotorEnabled(hkpConstraintRuntime* runtimeIn, hkBool isEnabled); // 0x00D6C8E0
    void setMaxFrictionTorque(float tmag) { m_atoms.m_angFriction.m_maxFrictionTorque = tmag; } // 0x0080DB20
    void setMaxAngularLimit(float rad) { m_atoms.m_angLimit.m_maxAngle = rad; } // 0x0080DAE0
    void setMinAngularLimit(float rad) { m_atoms.m_angLimit.m_minAngle = rad; } // 0x0080DB00
    void setAngularLimitsTauFactor(float mag) { m_atoms.m_angLimit.m_angularLimitsTauFactor = mag; } // 0x0080DB40
    virtual int getType() const; // vtbl+0x10 0x00D6C970
    virtual void getConstraintInfo(hkpConstraintData::ConstraintInfo& infoOut) const; // vtbl+0x14 0x00D6C930
    virtual hkBool isValid() const; // vtbl+0x18 0x00D6CBA0
    virtual void setBreachImpulse(float breachImpulse); // vtbl+0x24 0x00D6C880
    virtual float getBreachImpulse() const; // vtbl+0x30 0x00D6C8A0
    virtual void setBodyToNotify(int bodyIdx); // vtbl+0x34 0x00D6C8B0
    virtual unsigned char getNotifiedBodyIndex() const; // vtbl+0x38 0x00D6C8D0
    virtual void setSolvingMethod(hkpConstraintAtom::SolvingMethod method); // vtbl+0x3C 0x00D6CC20
    virtual hkResult setInertiaStabilizationFactor(const float inertiaStabilizationFactorIn); // vtbl+0x40 0x00D6C9A0
    virtual hkResult getInertiaStabilizationFactor(float& inertiaStabilizationFactorOut) const; // vtbl+0x44 0x00D6C980
    virtual void getAppliedLinearImpulse(const hkTransformf& worldFromBodyA, const hkTransformf& worldFromBodyB, const hkpConstraintRuntime* runtimeIn, hkVector4f& impulseOut) const; // vtbl+0x48 0x00D6C9E0
    virtual void getRuntimeInfo(hkBool wantRuntime, hkpConstraintData::RuntimeInfo& infoOut) const; // vtbl+0x4C 0x00D6C950

    Atoms m_atoms; // 0x10
};
