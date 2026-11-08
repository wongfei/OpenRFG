// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/ragdoll/hkpragdollconstraintdata.h
//
// Havok 2014.2.5 (multiplayer.md MP7): the ragdoll joint, from rfg.pdb. Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/atom/hkpconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverresults.h"

class hkRotationf;

// size 0x180
class hkpRagdollConstraintData : public hkpConstraintData
{
public:
    enum
    {
        SOLVER_RESULT_MOTOR_0 = 0,
        SOLVER_RESULT_MOTOR_0_INTERNAL = 1,
        SOLVER_RESULT_MOTOR_1 = 2,
        SOLVER_RESULT_MOTOR_1_INTERNAL = 3,
        SOLVER_RESULT_MOTOR_2 = 4,
        SOLVER_RESULT_MOTOR_2_INTERNAL = 5,
        SOLVER_RESULT_FRICTION_0 = 6,
        SOLVER_RESULT_FRICTION_0_INTERNAL = 7,
        SOLVER_RESULT_FRICTION_1 = 8,
        SOLVER_RESULT_FRICTION_1_INTERNAL = 9,
        SOLVER_RESULT_FRICTION_2 = 10,
        SOLVER_RESULT_FRICTION_2_INTERNAL = 11,
        SOLVER_RESULT_TWIST = 12,
        SOLVER_RESULT_CONE = 13,
        SOLVER_RESULT_PLANE = 14,
        SOLVER_RESULT_LIN_0 = 15,
        SOLVER_RESULT_LIN_1 = 16,
        SOLVER_RESULT_LIN_2 = 17,
        SOLVER_RESULT_MAX = 18,
    };

    enum MotorIndex
    {
        MOTOR_TWIST = 0,
        MOTOR_PLANE = 1,
        MOTOR_CONE = 2,
        NUM_MOTOR_INDICES = 3,
    };

    // size 0xB0 (16-byte aligned in the binary)
    struct alignas(16) Runtime
    {
        hkpSolverResults m_solverResults[18]; // 0x00
        unsigned char m_initialized[3]; // 0x90
        float m_previousTargetAngles[3]; // 0x94
        float m_coneAngleOffset; // 0xA0
    };

    // size 0x170
    struct Atoms
    {
        enum Axis
        {
            AXIS_TWIST = 0,
            AXIS_PLANES = 1,
            AXIS_CROSS_PRODUCT = 2,
        };

        Atoms(); // 0x00D6EF30
        const hkpConstraintAtom* getAtoms() const { return &m_transforms; } // 0x00D6EE50
        int getSizeOfAllAtoms() const { return (int)(reinterpret_cast<const char*>(&m_ballSocket + 1) - reinterpret_cast<const char*>(this)); } // 0x00D6EE60

        hkpSetLocalTransformsConstraintAtom m_transforms; // 0x00
        hkpSetupStabilizationAtom m_setupStabilization; // 0x90
        hkpRagdollMotorConstraintAtom m_ragdollMotors; // 0xA0
        hkpAngFrictionConstraintAtom m_angFriction; // 0xF0
        hkpTwistLimitConstraintAtom m_twistLimit; // 0x100
        hkpConeLimitConstraintAtom m_coneLimit; // 0x120
        hkpConeLimitConstraintAtom m_planesLimit; // 0x140
        hkpBallSocketConstraintAtom m_ballSocket; // 0x160
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new 0x00815ED0, delete(void* p, unsigned int nbytes) 0x00D6F060
    hkpRagdollConstraintData(); // 0x00D6E4E0
    virtual ~hkpRagdollConstraintData(); // vtbl+0x00 0x00D6E870
    void setInBodySpace(const hkVector4f& pivotA, const hkVector4f& pivotB, const hkVector4f& planeAxisA, const hkVector4f& planeAxisB, const hkVector4f& twistAxisA, const hkVector4f& twistAxisB); // 0x00D6EC50
    void setInWorldSpace(const hkTransformf& bodyATransform, const hkTransformf& bodyBTransform, const hkVector4f& pivot, const hkVector4f& twistAxisW, const hkVector4f& planeAxisW); // 0x00D6E990
    void setAsymmetricConeAngle(float cone_min, float cone_max); // 0x00D6E6C0
    void setConeLimitStabilization(hkBool enable); // 0x00D6E2B0
    void setMaxFrictionTorque(float tmag); // 0x00D6E290
    static Runtime* getRuntime(hkpConstraintRuntime* runtime) { return reinterpret_cast<Runtime*>(runtime); } // 0x00D6EE70
    virtual int getType() const; // vtbl+0x10 0x00D6E440
    virtual void getConstraintInfo(hkpConstraintData::ConstraintInfo& infoOut) const; // vtbl+0x14 0x00D6E250
    virtual hkBool isValid() const; // vtbl+0x18 0x00D6E750
    virtual void setBreachImpulse(float breachImpulse); // vtbl+0x24 0x00D6E1F0
    virtual float getBreachImpulse() const; // vtbl+0x30 0x00D6E210
    virtual void setBodyToNotify(int bodyIdx); // vtbl+0x34 0x00D6E220
    virtual unsigned char getNotifiedBodyIndex() const; // vtbl+0x38 0x00D6E240
    virtual void setSolvingMethod(hkpConstraintAtom::SolvingMethod method); // vtbl+0x3C 0x00D6E960
    virtual hkResult setInertiaStabilizationFactor(const float inertiaStabilizationFactorIn); // vtbl+0x40 0x00D6E470
    virtual hkResult getInertiaStabilizationFactor(float& inertiaStabilizationFactorOut) const; // vtbl+0x44 0x00D6E450
    virtual void getAppliedLinearImpulse(const hkTransformf& worldFromBodyA, const hkTransformf& worldFromBodyB, const hkpConstraintRuntime* runtimeIn, hkVector4f& impulseOut) const; // vtbl+0x48 0x00D6E4B0
    virtual void getRuntimeInfo(hkBool wantRuntime, hkpConstraintData::RuntimeInfo& infoOut) const; // vtbl+0x4C 0x00D6E270

    Atoms m_atoms; // 0x10
};
