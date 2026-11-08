// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/hkpconstraintqueryin.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the constraint query input of a solver step, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/hkpconstraintqueryin.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/hkstepinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverinfo.h"

// 0x00DEBA90
hkpConstraintQueryIn::hkpConstraintQueryIn(void (*beginConstraintsFunc)(const hkpConstraintQueryIn&, hkpConstraintQueryOut&, hkpSolverResults*, int))
{
    m_beginConstraints = beginConstraintsFunc;
}

// 0x00DF8DA0: the step's factors from the solver info (the base's m_maxConstraintViolationSqrd stays unset)
void hkpConstraintQueryIn::set(const hkpSolverInfo& si, const hkStepInfo& stepInfo, hkpViolatedConstraintArray* violatedConstraintsBuffer)
{
    m_subStepDeltaTime = si.m_deltaTime;
    m_microStepDeltaTime = si.m_invNumMicroSteps * si.m_deltaTime;
    m_subStepInvDeltaTime = si.m_invDeltaTime;
    m_invNumSteps = si.m_invNumSteps;
    m_invNumStepsTimesMicroSteps = si.m_invNumSteps * si.m_invNumMicroSteps;
    m_tau = si.m_tau;
    m_violatedConstraints = violatedConstraintsBuffer;
    m_damping = si.m_damping;
    m_rhsFactor = si.m_tauDivDamp * si.m_invDeltaTime;
    m_virtMassFactor = si.m_damping;
    m_frictionRhsFactor = si.m_frictionTauDivDamp * si.m_invDeltaTime;
    m_frameDeltaTime = stepInfo.m_deltaTime;
    m_frameInvDeltaTime = stepInfo.m_invDeltaTime;
    m_maxConstraintViolationSqrd.m_real = _mm_set1_ps(si.m_maxConstraintViolationSqrd);
}

// 0x00DEA740: leaves everything uninitialized
hkpConstraintQueryStepInfo::hkpConstraintQueryStepInfo()
{
}
