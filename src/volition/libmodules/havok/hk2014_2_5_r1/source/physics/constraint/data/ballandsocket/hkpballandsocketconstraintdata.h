// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/ballandsocket/hkpballandsocketconstraintdata.h
//
// Havok 2014.2.5 (multiplayer.md MP7): the point constraint (a ragdoll pinned to a point or tag), from rfg.pdb.
// Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/atom/hkpconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverresults.h"

// size 0x60
class hkpBallAndSocketConstraintData : public hkpConstraintData
{
public:
    enum
    {
        SOLVER_RESULT_LIN_0 = 0,
        SOLVER_RESULT_LIN_1 = 1,
        SOLVER_RESULT_LIN_2 = 2,
        SOLVER_RESULT_MAX = 3,
    };

    // size 0x18
    struct Runtime
    {
        hkpSolverResults m_solverResults[3]; // 0x00
    };

    // size 0x50
    struct Atoms
    {
        Atoms() {} // 0x00D6F530 (inlined into the data's constructor)
        const hkpConstraintAtom* getAtoms() const { return &m_pivots; } // 0x00D6F110

        hkpSetLocalTranslationsConstraintAtom m_pivots; // 0x00
        hkpSetupStabilizationAtom m_setupStabilization; // 0x30
        hkpBallSocketConstraintAtom m_ballSocket; // 0x40
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new 0x00815E90, delete(void* p, unsigned int nbytes) 0x00D6F5C0
    hkpBallAndSocketConstraintData(); // 0x00D6F2E0
    virtual ~hkpBallAndSocketConstraintData(); // vtbl+0x00 0x00D6F5B0
    void setInWorldSpace(const hkTransformf& bodyATransform, const hkTransformf& bodyBTransform, const hkVector4f& pivot); // 0x00D6F380
    virtual int getType() const; // vtbl+0x10 0x00D6F240
    virtual void getConstraintInfo(hkpConstraintData::ConstraintInfo& infoOut) const; // vtbl+0x14 0x00D6F1D0
    virtual hkBool isValid() const; // vtbl+0x18 0x00D6F4D0
    virtual void setMaximumLinearImpulse(float maxLinearImpulse); // vtbl+0x1C 0x00D6F170
    virtual void setBreachImpulse(float breachImpulse); // vtbl+0x24 0x00D6F150
    virtual float getMaximumLinearImpulse() const; // vtbl+0x28 0x00D6F1A0
    virtual float getBreachImpulse() const; // vtbl+0x30 0x00D6F160
    virtual void setBodyToNotify(int bodyIdx); // vtbl+0x34 0x00D6F1B0
    virtual unsigned char getNotifiedBodyIndex() const; // vtbl+0x38 0x00D6F1C0
    virtual void setSolvingMethod(hkpConstraintAtom::SolvingMethod method); // vtbl+0x3C 0x00D6F4F0
    virtual hkResult setInertiaStabilizationFactor(const float inertiaStabilizationFactorIn); // vtbl+0x40 0x00D6F270
    virtual hkResult getInertiaStabilizationFactor(float& inertiaStabilizationFactorOut) const; // vtbl+0x44 0x00D6F250
    virtual void getAppliedLinearImpulse(const hkTransformf& worldFromBodyA, const hkTransformf& worldFromBodyB, const hkpConstraintRuntime* runtimeIn, hkVector4f& impulseOut) const; // vtbl+0x48 0x00D6F2B0
    virtual void getRuntimeInfo(hkBool wantRuntime, hkpConstraintData::RuntimeInfo& infoOut) const; // vtbl+0x4C 0x00D6F1F0

    Atoms m_atoms; // 0x10
};
