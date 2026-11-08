// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/prismatic/hkpprismaticconstraintdata.h
//
// Havok 2014.2.5: the slider (no rotation, travel along one axis within a range; RFG's stuck projectiles use it with a
// zero range), from rfg.pdb. Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/atom/hkpconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverresults.h"

// size 0x110
class hkpPrismaticConstraintData : public hkpConstraintData
{
public:
    enum
    {
        SOLVER_RESULT_MOTOR = 0,
        SOLVER_RESULT_MOTOR_INTERNAL = 1,
        SOLVER_RESULT_FRICTION = 2,
        SOLVER_RESULT_FRICTION_INTERNAL = 3,
        SOLVER_RESULT_ANG_0 = 4,
        SOLVER_RESULT_ANG_1 = 5,
        SOLVER_RESULT_ANG_2 = 6,
        SOLVER_RESULT_LIN_0 = 7,
        SOLVER_RESULT_LIN_1 = 8,
        SOLVER_RESULT_LIMIT = 9,
        SOLVER_RESULT_MAX = 10,
    };

    // size 0x58
    struct Runtime
    {
        hkpSolverResults m_solverResults[10]; // 0x00
        unsigned char m_initialized; // 0x50
        float m_previousTargetPosition; // 0x54
    };

    // size 0x100
    struct Atoms
    {
        enum Axis
        {
            AXIS_SHAFT = 0,
            AXIS_PERP_TO_SHAFT = 1,
        };

        Atoms(); // 0x00D6DF90
        const hkpConstraintAtom* getAtoms() const { return &m_transforms; }

        hkpSetLocalTransformsConstraintAtom m_transforms; // 0x00
        hkpLinMotorConstraintAtom m_motor; // 0x90
        hkpLinFrictionConstraintAtom m_friction; // 0xB0
        hkpAngConstraintAtom m_ang; // 0xC0
        hkpLinConstraintAtom m_lin0; // 0xD0
        hkpLinConstraintAtom m_lin1; // 0xE0
        hkpLinLimitConstraintAtom m_linLimit; // 0xF0
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new 0x00815F10
    hkpPrismaticConstraintData(); // 0x00D6D6B0
    virtual ~hkpPrismaticConstraintData(); // vtbl+0x00 0x00D6D7D0
    void setInWorldSpace(const hkTransformf& bodyATransform, const hkTransformf& bodyBTransform, const hkVector4f& pivot, const hkVector4f& axisIn); // 0x00D6D920
    static Runtime* getRuntime(hkpConstraintRuntime* runtime) { return reinterpret_cast<Runtime*>(runtime); }
    void setMotor(hkpConstraintMotor* motor); // 0x00D6D7F0
    void setMaxLinearLimit(float mmax) { m_atoms.m_linLimit.m_max = mmax; }
    void setMinLinearLimit(float mmin) { m_atoms.m_linLimit.m_min = mmin; }
    void setMaxFrictionForce(float fmag) { m_atoms.m_friction.m_maxFrictionForce = fmag; }
    virtual int getType() const; // vtbl+0x10 0x00D6D650
    virtual void getConstraintInfo(hkpConstraintData::ConstraintInfo& infoOut) const; // vtbl+0x14 0x00D6D5B0
    virtual hkBool isValid() const; // vtbl+0x18 0x00D6D5F0
    virtual void getAppliedLinearImpulse(const hkTransformf& worldFromBodyA, const hkTransformf& worldFromBodyB, const hkpConstraintRuntime* runtimeIn, hkVector4f& impulseOut) const; // vtbl+0x48 0x00D6D820
    virtual void getRuntimeInfo(hkBool wantRuntime, hkpConstraintData::RuntimeInfo& infoOut) const; // vtbl+0x4C 0x00D6D5D0

    Atoms m_atoms; // 0x10
};
