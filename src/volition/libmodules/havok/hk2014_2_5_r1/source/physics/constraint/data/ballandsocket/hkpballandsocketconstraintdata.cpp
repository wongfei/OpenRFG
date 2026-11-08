// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/ballandsocket/hkpballandsocketconstraintdata.cpp
//
// Havok 2014.2.5 (multiplayer.md MP7): the point constraint, from rfg.exe. The game writes the pivots directly
// (havok_ragdoll_constrain_to_point/_tag) or through setInWorldSpace (havok_create_point_constraint,
// havok_constraint_transfer); setInBodySpace has no in-scope caller.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/ballandsocket/hkpballandsocketconstraintdata.h"

#include <stddef.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"

static_assert(sizeof(hkpBallAndSocketConstraintData) == 0x60, "hkpBallAndSocketConstraintData");
static_assert(sizeof(hkpBallAndSocketConstraintData::Atoms) == 0x50, "hkpBallAndSocketConstraintData::Atoms");
static_assert(offsetof(hkpBallAndSocketConstraintData::Atoms, m_ballSocket) == 0x40, "Atoms::m_ballSocket");

// 0x00D6F2E0: zero pivots, the old ball socket, no stabilization
hkpBallAndSocketConstraintData::hkpBallAndSocketConstraintData()
{
    m_atoms.m_setupStabilization.m_enabled = false;
    m_atoms.m_setupStabilization.m_maxLinImpulse = HK_REAL_MAX;
    m_atoms.m_setupStabilization.m_maxAngImpulse = HK_REAL_MAX;
    m_atoms.m_setupStabilization.m_maxAngle = HK_REAL_HIGH;
    m_atoms.m_ballSocket.m_solvingMethod.m_storage = hkpConstraintAtom::METHOD_OLD;
    m_atoms.m_ballSocket.m_bodiesToNotify = 0;
    const float one = 1.0f;
    m_atoms.m_ballSocket.m_velocityStabilizationFactor = one;
    m_atoms.m_ballSocket.m_enableLinearImpulseLimit = false;
    m_atoms.m_ballSocket.m_breachImpulse = HK_REAL_MAX;
    m_atoms.m_ballSocket.m_inertiaStabilizationFactor = 0.0f;
    m_atoms.m_pivots.m_translationA.setZero();
    m_atoms.m_pivots.m_translationB.setZero();
}

// 0x00D6F5B0
hkpBallAndSocketConstraintData::~hkpBallAndSocketConstraintData()
{
}

// 0x00D6F380: the pivot in each body's space (havok_create_point_constraint, havok_constraint_transfer)
void hkpBallAndSocketConstraintData::setInWorldSpace(const hkTransformf& bodyATransform, const hkTransformf& bodyBTransform, const hkVector4f& pivot)
{
    m_atoms.m_pivots.m_translationA.setTransformedInversePos(bodyATransform, pivot);
    m_atoms.m_pivots.m_translationB.setTransformedInversePos(bodyBTransform, pivot);
}

// 0x00D6F240
int hkpBallAndSocketConstraintData::getType() const
{
    return CONSTRAINT_TYPE_BALLANDSOCKET;
}

// 0x00D6F1D0
void hkpBallAndSocketConstraintData::getConstraintInfo(hkpConstraintData::ConstraintInfo& infoOut) const
{
    getConstraintInfoUtil(m_atoms.getAtoms(), sizeof(Atoms), infoOut);
}

// 0x00D6F4D0
hkBool hkpBallAndSocketConstraintData::isValid() const
{
    if (m_atoms.m_ballSocket.m_solvingMethod.m_storage == hkpConstraintAtom::METHOD_STABILIZED && !m_atoms.m_setupStabilization.m_enabled.m_bool)
    {
        return false;
    }
    return true;
}

// 0x00D6F170: a limit below HK_REAL_MAX enables it
void hkpBallAndSocketConstraintData::setMaximumLinearImpulse(float maxLinearImpulse)
{
    m_atoms.m_setupStabilization.m_maxLinImpulse = maxLinearImpulse;
    m_atoms.m_ballSocket.m_enableLinearImpulseLimit = maxLinearImpulse < HK_REAL_MAX;
}

// 0x00D6F150
void hkpBallAndSocketConstraintData::setBreachImpulse(float breachImpulse)
{
    m_atoms.m_ballSocket.m_breachImpulse = breachImpulse;
}

// 0x00D6F1A0
float hkpBallAndSocketConstraintData::getMaximumLinearImpulse() const
{
    return m_atoms.m_setupStabilization.m_maxLinImpulse;
}

// 0x00D6F160
float hkpBallAndSocketConstraintData::getBreachImpulse() const
{
    return m_atoms.m_ballSocket.m_breachImpulse;
}

// 0x00D6F1B0
void hkpBallAndSocketConstraintData::setBodyToNotify(int bodyIdx)
{
    m_atoms.m_ballSocket.m_bodiesToNotify = (unsigned char)(1 << (bodyIdx & 0x1F));
}

// 0x00D6F1C0
unsigned char hkpBallAndSocketConstraintData::getNotifiedBodyIndex() const
{
    return m_atoms.m_ballSocket.m_bodiesToNotify >> 1;
}

// 0x00D6F4F0
void hkpBallAndSocketConstraintData::setSolvingMethod(hkpConstraintAtom::SolvingMethod method)
{
    if (method == hkpConstraintAtom::METHOD_STABILIZED)
    {
        m_atoms.m_setupStabilization.m_enabled = true;
        m_atoms.m_ballSocket.m_solvingMethod.m_storage = hkpConstraintAtom::METHOD_STABILIZED;
    }
    else if (method == hkpConstraintAtom::METHOD_OLD)
    {
        m_atoms.m_setupStabilization.m_enabled = false;
        m_atoms.m_ballSocket.m_solvingMethod.m_storage = hkpConstraintAtom::METHOD_OLD;
    }
}

// 0x00D6F270: clamped to [0, 1]
hkResult hkpBallAndSocketConstraintData::setInertiaStabilizationFactor(const float inertiaStabilizationFactorIn)
{
    float factor = inertiaStabilizationFactorIn;
    if (factor <= 0.0f)
    {
        factor = 0.0f;
    }
    if (1.0f <= factor)
    {
        factor = 1.0f;
    }
    m_atoms.m_ballSocket.m_inertiaStabilizationFactor = factor;
    return HK_SUCCESS;
}

// 0x00D6F250
hkResult hkpBallAndSocketConstraintData::getInertiaStabilizationFactor(float& inertiaStabilizationFactorOut) const
{
    inertiaStabilizationFactorOut = m_atoms.m_ballSocket.m_inertiaStabilizationFactor;
    return HK_SUCCESS;
}

// 0x00D6F2B0
void hkpBallAndSocketConstraintData::getAppliedLinearImpulse(const hkTransformf& worldFromBodyA, const hkTransformf& worldFromBodyB, const hkpConstraintRuntime* runtimeIn, hkVector4f& impulseOut) const
{
    const Runtime* runtime = reinterpret_cast<const Runtime*>(runtimeIn);
    impulseOut.m_quad = _mm_setr_ps(runtime->m_solverResults[SOLVER_RESULT_LIN_0].m_impulseApplied, runtime->m_solverResults[SOLVER_RESULT_LIN_1].m_impulseApplied, runtime->m_solverResults[SOLVER_RESULT_LIN_2].m_impulseApplied, 0.0f);
}

// 0x00D6F1F0: no runtime unless wanted or a breach impulse is set
void hkpBallAndSocketConstraintData::getRuntimeInfo(hkBool wantRuntime, hkpConstraintData::RuntimeInfo& infoOut) const
{
    if (!wantRuntime.m_bool && m_atoms.m_ballSocket.m_breachImpulse == HK_REAL_MAX)
    {
        infoOut.m_numSolverResults = 0;
        infoOut.m_sizeOfExternalRuntime = 0;
        return;
    }
    infoOut.m_numSolverResults = SOLVER_RESULT_MAX;
    infoOut.m_sizeOfExternalRuntime = sizeof(Runtime);
}
