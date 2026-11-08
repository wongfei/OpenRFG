// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/prismatic/hkpprismaticconstraintdata.cpp
//
// Havok 2014.2.5: the prismatic constraint, from rfg.exe. What havok_create_slide_constraint and the world reach: the
// constructors, setInWorldSpace, setMotor (havok_constraint_add_motor/_remove_motor) and the virtuals.
// setMotorEnabled, setInBodySpace and allowRotationAroundAxis have no in-scope caller.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/prismatic/hkpprismaticconstraintdata.h"

#include <stddef.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkrotationf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/motor/hkpconstraintmotor.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/hkpsolversse.h"

using namespace hkSolverSse;

static_assert(sizeof(hkpPrismaticConstraintData) == 0x110, "hkpPrismaticConstraintData");
static_assert(sizeof(hkpPrismaticConstraintData::Atoms) == 0x100, "hkpPrismaticConstraintData::Atoms");
static_assert(sizeof(hkpPrismaticConstraintData::Runtime) == 0x58, "hkpPrismaticConstraintData::Runtime");
static_assert(offsetof(hkpPrismaticConstraintData::Atoms, m_ang) == 0xC0, "Atoms::m_ang");
static_assert(offsetof(hkpPrismaticConstraintData::Atoms, m_linLimit) == 0xF0, "Atoms::m_linLimit");

// 0x00D6DF90: the atoms' types; the motor and the friction start enabled
hkpPrismaticConstraintData::Atoms::Atoms()
{
    m_motor.m_isEnabled = true;
    m_motor.m_initializedOffset = -1;
    m_motor.m_previousTargetPositionOffset = -1;
    m_friction.m_isEnabled = 1;
}

// 0x00D6D6B0: identity spaces, no rotation at all (axes 0, 1 and 2), the pivots kept together along axes 1 and 2,
// the shaft (axis 0) unlimited (+-FLT_MAX), no friction force (its atom stays enabled), the motor off (its runtime
// offsets set)
hkpPrismaticConstraintData::hkpPrismaticConstraintData()
{
    m_atoms.m_transforms.m_transformA.m_rotation.m_col0.m_quad = g_vectorfConstants[HK_QUADREAL_1000];
    m_atoms.m_transforms.m_transformA.m_rotation.m_col1.m_quad = g_vectorfConstants[HK_QUADREAL_0100];
    m_atoms.m_transforms.m_transformA.m_rotation.m_col2.m_quad = g_vectorfConstants[HK_QUADREAL_0010];
    m_atoms.m_transforms.m_transformA.m_translation.setZero();
    m_atoms.m_transforms.m_transformB.m_rotation.m_col0.m_quad = g_vectorfConstants[HK_QUADREAL_1000];
    m_atoms.m_transforms.m_transformB.m_rotation.m_col1.m_quad = g_vectorfConstants[HK_QUADREAL_0100];
    m_atoms.m_transforms.m_transformB.m_translation.setZero();
    m_atoms.m_transforms.m_transformB.m_rotation.m_col2.m_quad = g_vectorfConstants[HK_QUADREAL_0010];
    m_atoms.m_linLimit.m_min = -HK_REAL_MAX;
    m_atoms.m_linLimit.m_max = HK_REAL_MAX;
    m_atoms.m_linLimit.m_axisIndex = Atoms::AXIS_SHAFT;
    m_atoms.m_friction.m_frictionAxis = Atoms::AXIS_SHAFT;
    m_atoms.m_friction.m_maxFrictionForce = 0.0f;
    m_atoms.m_motor.m_isEnabled = false;
    m_atoms.m_lin1.m_axisIndex = 2;
    m_atoms.m_motor.m_motor = nullptr;
    m_atoms.m_motor.m_motorAxis = Atoms::AXIS_SHAFT;
    m_atoms.m_motor.m_targetPosition = 0.0f;
    m_atoms.m_motor.m_initializedOffset = (short)offsetof(Runtime, m_initialized); // 0x50
    m_atoms.m_motor.m_previousTargetPositionOffset = (short)offsetof(Runtime, m_previousTargetPosition); // 0x54
    m_atoms.m_ang.m_constrainedAxes[0] = 0;
    m_atoms.m_ang.m_constrainedAxes[1] = 1;
    m_atoms.m_ang.m_constrainedAxes[2] = 2;
    m_atoms.m_ang.m_numConstrainedAxes = 3;
    m_atoms.m_lin0.m_axisIndex = Atoms::AXIS_PERP_TO_SHAFT;
}

// 0x00D6D7D0: the motor's reference goes
hkpPrismaticConstraintData::~hkpPrismaticConstraintData()
{
    hkpConstraintMotor* motor = m_atoms.m_motor.m_motor;
    if (motor != nullptr)
    {
        motor->removeReference();
    }
}

// 0x00D6D7F0: the new motor's reference first, then the old one's goes (the motor's atom stays off: rfg.exe never
// enables a prismatic motor)
void hkpPrismaticConstraintData::setMotor(hkpConstraintMotor* motor)
{
    if (motor != nullptr)
    {
        motor->addReference();
    }
    if (m_atoms.m_motor.m_motor != nullptr)
    {
        m_atoms.m_motor.m_motor->removeReference();
    }
    m_atoms.m_motor.m_motor = motor;
}

// 0x00D6D920: the shaft (normalized), a perpendicular and their cross product, in each body's space; the pivot too
// (the same code as hkpLimitedHingeConstraintData::setInWorldSpace)
void hkpPrismaticConstraintData::setInWorldSpace(const hkTransformf& bodyATransform, const hkTransformf& bodyBTransform, const hkVector4f& pivot, const hkVector4f& axisIn)
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
    rotB.m_col0.setRotatedInverseDir(bodyBTransform.m_rotation, axis);
    rotB.m_col1.setRotatedInverseDir(bodyBTransform.m_rotation, perp);
    rotB.m_col2.setRotatedInverseDir(bodyBTransform.m_rotation, third);
    m_atoms.m_transforms.m_transformA.m_translation.setTransformedInversePos(bodyATransform, pivot);
    m_atoms.m_transforms.m_transformB.m_translation.setTransformedInversePos(bodyBTransform, pivot);
}

// 0x00D6D650
int hkpPrismaticConstraintData::getType() const
{
    return CONSTRAINT_TYPE_PRISMATIC;
}

// 0x00D6D5B0
void hkpPrismaticConstraintData::getConstraintInfo(hkpConstraintData::ConstraintInfo& infoOut) const
{
    getConstraintInfoUtil(m_atoms.getAtoms(), sizeof(Atoms), infoOut);
}

// 0x00D6D5F0: orthonormal spaces and an ordered range
hkBool hkpPrismaticConstraintData::isValid() const
{
    if (m_atoms.m_transforms.m_transformA.m_rotation.isOrthonormal(1e-05f) && m_atoms.m_transforms.m_transformB.m_rotation.isOrthonormal(1e-05f) && m_atoms.m_linLimit.m_min <= m_atoms.m_linLimit.m_max)
    {
        return true;
    }
    return false;
}

// 0x00D6D820: the two linear results' and the limit's impulses along their axes of A's space in the world
void hkpPrismaticConstraintData::getAppliedLinearImpulse(const hkTransformf& worldFromBodyA, const hkTransformf& worldFromBodyB, const hkpConstraintRuntime* runtimeIn, hkVector4f& impulseOut) const
{
    const hkRotationf& r = worldFromBodyA.m_rotation;
    const hkRotationf& local = m_atoms.m_transforms.m_transformA.m_rotation;
    HK_ALIGN16(Q axes[3]);
    axes[0] = rotate(r.m_col0.m_quad, r.m_col1.m_quad, r.m_col2.m_quad, local.m_col0.m_quad);
    axes[1] = rotate(r.m_col0.m_quad, r.m_col1.m_quad, r.m_col2.m_quad, local.m_col1.m_quad);
    axes[2] = rotate(r.m_col0.m_quad, r.m_col1.m_quad, r.m_col2.m_quad, local.m_col2.m_quad);
    const Runtime* runtime = reinterpret_cast<const Runtime*>(runtimeIn);
    const Q lin0 = _mm_mul_ps(splat(runtime->m_solverResults[SOLVER_RESULT_LIN_0].m_impulseApplied), axes[m_atoms.m_lin0.m_axisIndex]);
    impulseOut.m_quad = lin0;
    const Q lin1 = _mm_add_ps(_mm_mul_ps(splat(runtime->m_solverResults[SOLVER_RESULT_LIN_1].m_impulseApplied), axes[m_atoms.m_lin1.m_axisIndex]), lin0);
    impulseOut.m_quad = lin1;
    impulseOut.m_quad = _mm_add_ps(_mm_mul_ps(splat(runtime->m_solverResults[SOLVER_RESULT_LIMIT].m_impulseApplied), axes[m_atoms.m_linLimit.m_axisIndex]), lin1);
}

// 0x00D6D5D0
void hkpPrismaticConstraintData::getRuntimeInfo(hkBool wantRuntime, hkpConstraintData::RuntimeInfo& infoOut) const
{
    infoOut.m_numSolverResults = SOLVER_RESULT_MAX;
    infoOut.m_sizeOfExternalRuntime = sizeof(Runtime);
}
