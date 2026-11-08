// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/limitedhinge/hkplimitedhingeconstraintdata.cpp
//
// Havok 2014.2.5 (multiplayer.md MP7): the limited hinge, from rfg.exe. What create_ragdoll and the world reach: the
// constructors, setInWorldSpace and the virtuals; the motor (setMotor, setMotorEnabled) for havok_create_hinge_constraint
// and the havok_constraint_*_motor functions; setInBodySpace for the vehicle doors.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/limitedhinge/hkplimitedhingeconstraintdata.h"

#include <stddef.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkrotationf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/motor/hkpconstraintmotor.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/hkpsolversse.h"

using namespace hkSolverSse;

static_assert(sizeof(hkpLimitedHingeConstraintData) == 0x120, "hkpLimitedHingeConstraintData");
static_assert(sizeof(hkpLimitedHingeConstraintData::Atoms) == 0x110, "hkpLimitedHingeConstraintData::Atoms");
static_assert(sizeof(hkpLimitedHingeConstraintData::Runtime) == 0x60, "hkpLimitedHingeConstraintData::Runtime");
static_assert(offsetof(hkpLimitedHingeConstraintData::Atoms, m_angLimit) == 0xD0, "Atoms::m_angLimit");
static_assert(offsetof(hkpLimitedHingeConstraintData::Atoms, m_ballSocket) == 0x100, "Atoms::m_ballSocket");

// 0x00D6D390: the transforms keep their type only; the old ball socket and no stabilization
hkpLimitedHingeConstraintData::Atoms::Atoms()
{
    m_setupStabilization.m_enabled = false;
    m_setupStabilization.m_maxLinImpulse = HK_REAL_MAX;
    m_setupStabilization.m_maxAngImpulse = HK_REAL_MAX;
    m_setupStabilization.m_maxAngle = HK_REAL_HIGH;
    m_angMotor.m_isEnabled = true;
    m_angMotor.m_initializedOffset = -1;
    m_angMotor.m_previousTargetAngleOffset = -1;
    m_angMotor.m_correspondingAngLimitSolverResultOffset = -1;
    m_angFriction.m_isEnabled = 1;
    m_angFriction.m_numFrictionAxes = 1;
    m_angLimit.m_isEnabled = 1;
    m_ballSocket.m_solvingMethod.m_storage = hkpConstraintAtom::METHOD_OLD;
    m_ballSocket.m_bodiesToNotify = 0;
    const float one = 1.0f;
    m_ballSocket.m_velocityStabilizationFactor = one;
    m_ballSocket.m_enableLinearImpulseLimit = false;
    m_ballSocket.m_breachImpulse = HK_REAL_MAX;
    m_ballSocket.m_inertiaStabilizationFactor = 0.0f;
}

// 0x00D6CA10: identity spaces, the limit +-pi around the axle (cosine axis 1), one friction axis without torque, the
// motor off (its runtime offsets set; the limit's solver result 4 at 0x20)
hkpLimitedHingeConstraintData::hkpLimitedHingeConstraintData()
{
    m_atoms.m_transforms.m_transformA.m_rotation.m_col0.m_quad = g_vectorfConstants[HK_QUADREAL_1000];
    m_atoms.m_transforms.m_transformA.m_rotation.m_col1.m_quad = g_vectorfConstants[HK_QUADREAL_0100];
    m_atoms.m_transforms.m_transformA.m_rotation.m_col2.m_quad = g_vectorfConstants[HK_QUADREAL_0010];
    m_atoms.m_transforms.m_transformA.m_translation.setZero();
    m_atoms.m_transforms.m_transformB.m_rotation.m_col0.m_quad = g_vectorfConstants[HK_QUADREAL_1000];
    m_atoms.m_transforms.m_transformB.m_rotation.m_col1.m_quad = g_vectorfConstants[HK_QUADREAL_0100];
    m_atoms.m_transforms.m_transformB.m_translation.setZero();
    m_atoms.m_transforms.m_transformB.m_rotation.m_col2.m_quad = g_vectorfConstants[HK_QUADREAL_0010];
    m_atoms.m_angLimit.m_minAngle = -3.1415927f;
    m_atoms.m_angLimit.m_maxAngle = 3.1415927f;
    m_atoms.m_angLimit.m_angularLimitsTauFactor = 1.0f;
    m_atoms.m_angLimit.m_angularLimitsDampFactor = 1.0f;
    m_atoms.m_2dAng.m_freeRotationAxis = Atoms::AXIS_AXLE;
    m_atoms.m_angLimit.m_limitAxis = Atoms::AXIS_AXLE;
    m_atoms.m_angLimit.m_cosineAxis = Atoms::AXIS_PERP_TO_AXLE_1;
    m_atoms.m_angFriction.m_firstFrictionAxis = Atoms::AXIS_AXLE;
    m_atoms.m_angFriction.m_numFrictionAxes = 1;
    m_atoms.m_angFriction.m_maxFrictionTorque = 0.0f;
    m_atoms.m_angMotor.m_initializedOffset = (short)offsetof(Runtime, m_initialized); // 0x50
    m_atoms.m_angMotor.m_previousTargetAngleOffset = (short)offsetof(Runtime, m_previousTargetAngle); // 0x54
    m_atoms.m_angMotor.m_correspondingAngLimitSolverResultOffset = SOLVER_RESULT_LIMIT * sizeof(hkpSolverResults); // 0x20
    m_atoms.m_angMotor.m_isEnabled = false;
    m_atoms.m_angMotor.m_motor = nullptr;
    m_atoms.m_angMotor.m_motorAxis = Atoms::AXIS_AXLE;
    m_atoms.m_angMotor.m_targetAngle = 0.0f;
}

// 0x00D6CB50
hkpLimitedHingeConstraintData::~hkpLimitedHingeConstraintData()
{
}

// 0x00D6CB70: the new motor's reference first, then the old one's goes
void hkpLimitedHingeConstraintData::setMotor(hkpConstraintMotor* motor)
{
    if (motor != nullptr)
    {
        motor->addReference();
    }
    if (m_atoms.m_angMotor.m_motor != nullptr)
    {
        m_atoms.m_angMotor.m_motor->removeReference();
    }
    m_atoms.m_angMotor.m_motor = motor;
}

// 0x00D6C8E0: the motor and the friction take turns; the motor's and the friction's results restart
void hkpLimitedHingeConstraintData::setMotorEnabled(hkpConstraintRuntime* runtimeIn, hkBool isEnabled)
{
    m_atoms.m_angMotor.m_isEnabled = isEnabled;
    m_atoms.m_angFriction.m_isEnabled = isEnabled.m_bool == 0;
    if (runtimeIn != nullptr)
    {
        Runtime* runtime = getRuntime(runtimeIn);
        for (int i = SOLVER_RESULT_MOTOR; i <= SOLVER_RESULT_FRICTION_INTERNAL; i++)
        {
            runtime->m_solverResults[i].m_impulseApplied = 0.0f;
            runtime->m_solverResults[i].m_internalSolverData = 0.0f;
        }
    }
}

// 0x00D6CC50: the axle (normalized), a perpendicular and their cross product, in each body's space; the pivot too
void hkpLimitedHingeConstraintData::setInWorldSpace(const hkTransformf& bodyATransform, const hkTransformf& bodyBTransform, const hkVector4f& pivot, const hkVector4f& axisIn)
{
    hkVector4f axis;
    axis.m_quad = _mm_mul_ps(invSqrt23(hadd3(_mm_mul_ps(axisIn.m_quad, axisIn.m_quad))), axisIn.m_quad);
    // hkVector4Util::calculatePerpendicularVector: (-z, 0, x, 0) when x^2 + z^2 > x^2 + y^2, else (-y, x, 0, 0) (the
    // sign flips x only); the binary sums the squares of (z, 0, x, 0) over all four lanes
    const Q xyz0 = _mm_castsi128_ps(_mm_srli_si128(_mm_slli_si128(_mm_castps_si128(axis.m_quad), 4), 4));
    const Q z0x0 = _mm_shuffle_ps(xyz0, xyz0, 0xCE);
    const Q sqZX = _mm_mul_ps(z0x0, z0x0);
    const Q sumZX = _mm_add_ps(_mm_shuffle_ps(sqZX, sqZX, 0x4E), sqZX);
    const Q lenZX = _mm_add_ps(_mm_shuffle_ps(sumZX, sumZX, 0xB1), sumZX);
    const Q sq = _mm_mul_ps(axis.m_quad, axis.m_quad);
    const Q lenXY = _mm_add_ps(yyyy(sq), xxxx(sq));
    const Q useZX = _mm_cmplt_ps(lenXY, lenZX);
    const Q signMask = _mm_castsi128_ps(_mm_setr_epi32(0x80000000, 0, 0, 0));
    const Q yx00 = _mm_xor_ps(_mm_shuffle_ps(xyz0, xyz0, 0xF1), signMask);
    const Q perpRaw = _mm_or_ps(_mm_andnot_ps(useZX, yx00), _mm_and_ps(_mm_xor_ps(signMask, z0x0), useZX));
    hkVector4f perp;
    perp.m_quad = _mm_mul_ps(invSqrt23(hadd3(_mm_mul_ps(perpRaw, perpRaw))), perpRaw);
    hkVector4f third;
    third.m_quad = cross(axis.m_quad, perp.m_quad);

    hkRotationf& rotA = m_atoms.m_transforms.m_transformA.m_rotation;
    hkRotationf& rotB = m_atoms.m_transforms.m_transformB.m_rotation;
    rotA.m_col0.setRotatedInverseDir(bodyATransform.m_rotation, axis);
    rotA.m_col1.setRotatedInverseDir(bodyATransform.m_rotation, perp);
    rotA.m_col2.setRotatedInverseDir(bodyATransform.m_rotation, third);
    m_atoms.m_transforms.m_transformA.m_translation.setTransformedInversePos(bodyATransform, pivot);
    rotB.m_col0.setRotatedInverseDir(bodyBTransform.m_rotation, axis);
    rotB.m_col1.setRotatedInverseDir(bodyBTransform.m_rotation, perp);
    rotB.m_col2.setRotatedInverseDir(bodyBTransform.m_rotation, third);
    m_atoms.m_transforms.m_transformB.m_translation.setTransformedInversePos(bodyBTransform, pivot);
}

// 0x00D6CE30: normalizes the axes (the vehicle doors' hinges); column 2 is their cross product
void hkpLimitedHingeConstraintData::setInBodySpace(const hkVector4f& pivotA, const hkVector4f& pivotB, const hkVector4f& axisA, const hkVector4f& axisB, const hkVector4f& axisAPerp, const hkVector4f& axisBPerp)
{
    hkRotationf& rotA = m_atoms.m_transforms.m_transformA.m_rotation;
    hkRotationf& rotB = m_atoms.m_transforms.m_transformB.m_rotation;
    rotA.m_col0.m_quad = _mm_mul_ps(invSqrt23(hadd3(_mm_mul_ps(axisA.m_quad, axisA.m_quad))), axisA.m_quad);
    rotA.m_col1.m_quad = _mm_mul_ps(invSqrt23(hadd3(_mm_mul_ps(axisAPerp.m_quad, axisAPerp.m_quad))), axisAPerp.m_quad);
    rotA.m_col2.m_quad = cross(rotA.m_col0.m_quad, rotA.m_col1.m_quad);
    m_atoms.m_transforms.m_transformA.m_translation = pivotA;
    rotB.m_col0.m_quad = _mm_mul_ps(invSqrt23(hadd3(_mm_mul_ps(axisB.m_quad, axisB.m_quad))), axisB.m_quad);
    rotB.m_col1.m_quad = _mm_mul_ps(invSqrt23(hadd3(_mm_mul_ps(axisBPerp.m_quad, axisBPerp.m_quad))), axisBPerp.m_quad);
    rotB.m_col2.m_quad = cross(rotB.m_col0.m_quad, rotB.m_col1.m_quad);
    m_atoms.m_transforms.m_transformB.m_translation = pivotB;
}

// 0x00D6C970
int hkpLimitedHingeConstraintData::getType() const
{
    return CONSTRAINT_TYPE_LIMITEDHINGE;
}

// 0x00D6C930
void hkpLimitedHingeConstraintData::getConstraintInfo(hkpConstraintData::ConstraintInfo& infoOut) const
{
    getConstraintInfoUtil(m_atoms.getAtoms(), sizeof(Atoms), infoOut);
}

// 0x00D6CBA0: a solving method, orthonormal spaces, and the limit on when the motor is
hkBool hkpLimitedHingeConstraintData::isValid() const
{
    if (m_atoms.m_ballSocket.m_solvingMethod.m_storage == hkpConstraintAtom::METHOD_STABILIZED && !m_atoms.m_setupStabilization.m_enabled.m_bool)
    {
        return false;
    }
    if (m_atoms.m_transforms.m_transformA.m_rotation.isOrthonormal(1e-05f) && m_atoms.m_transforms.m_transformB.m_rotation.isOrthonormal(1e-05f)
        && (!m_atoms.m_angMotor.m_isEnabled.m_bool || m_atoms.m_angLimit.m_isEnabled != 0))
    {
        return true;
    }
    return false;
}

// 0x00D6C880
void hkpLimitedHingeConstraintData::setBreachImpulse(float breachImpulse)
{
    m_atoms.m_ballSocket.m_breachImpulse = breachImpulse;
}

// 0x00D6C8A0
float hkpLimitedHingeConstraintData::getBreachImpulse() const
{
    return m_atoms.m_ballSocket.m_breachImpulse;
}

// 0x00D6C8B0
void hkpLimitedHingeConstraintData::setBodyToNotify(int bodyIdx)
{
    m_atoms.m_ballSocket.m_bodiesToNotify = (unsigned char)(1 << (bodyIdx & 0x1F));
}

// 0x00D6C8D0
unsigned char hkpLimitedHingeConstraintData::getNotifiedBodyIndex() const
{
    return m_atoms.m_ballSocket.m_bodiesToNotify >> 1;
}

// 0x00D6CC20
void hkpLimitedHingeConstraintData::setSolvingMethod(hkpConstraintAtom::SolvingMethod method)
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

// 0x00D6C9A0: clamped to [0, 1]
hkResult hkpLimitedHingeConstraintData::setInertiaStabilizationFactor(const float inertiaStabilizationFactorIn)
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

// 0x00D6C980
hkResult hkpLimitedHingeConstraintData::getInertiaStabilizationFactor(float& inertiaStabilizationFactorOut) const
{
    inertiaStabilizationFactorOut = m_atoms.m_ballSocket.m_inertiaStabilizationFactor;
    return HK_SUCCESS;
}

// 0x00D6C9E0: the three linear results' impulses
void hkpLimitedHingeConstraintData::getAppliedLinearImpulse(const hkTransformf& worldFromBodyA, const hkTransformf& worldFromBodyB, const hkpConstraintRuntime* runtimeIn, hkVector4f& impulseOut) const
{
    const Runtime* runtime = reinterpret_cast<const Runtime*>(runtimeIn);
    impulseOut.m_quad = _mm_setr_ps(runtime->m_solverResults[SOLVER_RESULT_LIN_0].m_impulseApplied, runtime->m_solverResults[SOLVER_RESULT_LIN_1].m_impulseApplied, runtime->m_solverResults[SOLVER_RESULT_LIN_2].m_impulseApplied, 0.0f);
}

// 0x00D6C950
void hkpLimitedHingeConstraintData::getRuntimeInfo(hkBool wantRuntime, hkpConstraintData::RuntimeInfo& infoOut) const
{
    infoOut.m_numSolverResults = SOLVER_RESULT_MAX;
    infoOut.m_sizeOfExternalRuntime = sizeof(Runtime);
}
