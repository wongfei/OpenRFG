// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/hkpconstraintqueryin.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hksimdfloat32.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkStepInfo;
class hkTransformf;
class hkpConstraintInstance;
class hkpConstraintQueryOut;
struct hkpSolverInfo;
class hkpSolverResults;
class hkpVelocityAccumulator;
struct hkpViolatedConstraintArray;

// size 0x30
class alignas(16) hkpConstraintQueryStepInfo
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpConstraintQueryStepInfo);
    hkpConstraintQueryStepInfo(); // 0x00DEA740

    hkPadSpu<float> m_subStepDeltaTime; // 0x00
    hkPadSpu<float> m_microStepDeltaTime; // 0x04
    hkPadSpu<float> m_subStepInvDeltaTime; // 0x08
    hkPadSpu<float> m_frameDeltaTime; // 0x0C
    hkPadSpu<float> m_frameInvDeltaTime; // 0x10
    hkPadSpu<float> m_invNumSteps; // 0x14
    hkPadSpu<float> m_invNumStepsTimesMicroSteps; // 0x18
    hkPadSpu<float> m_maxConstraintViolationSqrd; // 0x1C
    hkPadSpu<float> m_rhsFactor; // 0x20
    hkPadSpu<float> m_virtMassFactor; // 0x24
    hkPadSpu<float> m_frictionRhsFactor; // 0x28
};

// size 0x80
class alignas(16) hkpConstraintQueryIn : public hkpConstraintQueryStepInfo
{
public:
    typedef void (*FnBeginConstraints)(const hkpConstraintQueryIn&, hkpConstraintQueryOut&, hkpSolverResults*, int);

    void set(const hkpSolverInfo& si, const hkStepInfo& stepInfo, hkpViolatedConstraintArray* violatedConstraintsBuffer); // 0x00DF8DA0
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpConstraintQueryIn);
    hkpConstraintQueryIn(void (*beginConstraintsFunc)(const hkpConstraintQueryIn&, hkpConstraintQueryOut&, hkpSolverResults*, int)); // 0x00DEBA90
    void beginConstraints(hkpConstraintQueryOut& out, hkpSolverResults* sr, int solverResultStriding) const; // 0x00D70320

    alignas(16) hkPadSpu<hkpVelocityAccumulator const*> m_bodyA; // 0x30
    hkPadSpu<hkpVelocityAccumulator const*> m_bodyB; // 0x34
    hkPadSpu<hkTransformf const*> m_transformA; // 0x38
    hkPadSpu<hkTransformf const*> m_transformB; // 0x3C
    hkPadSpu<float> m_tau; // 0x40
    hkPadSpu<float> m_damping; // 0x44
    hkSimdFloat32 m_maxConstraintViolationSqrd; // 0x50
    hkPadSpu<hkpConstraintInstance*> m_constraintInstance; // 0x60
    hkPadSpu<hkpViolatedConstraintArray*> m_violatedConstraints; // 0x64
    hkPadSpu<unsigned int> m_accumulatorAIndex; // 0x68
    hkPadSpu<unsigned int> m_accumulatorBIndex; // 0x6C
    void (*m_beginConstraints)(const hkpConstraintQueryIn&, hkpConstraintQueryOut&, hkpSolverResults*, int); // 0x70
};
