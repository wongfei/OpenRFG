// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/ragdoll/hkpragdollconstraintdata.cpp
//
// Havok 2014.2.5 (multiplayer.md MP7): the ragdoll joint, from rfg.exe. What create_ragdoll_contraint and the world
// reach: the constructors, setInBodySpace, the friction and cone stabilization setters and the virtuals; setInWorldSpace
// and setAsymmetricConeAngle for havok_create_ragdoll_constraint and havok_constraint_transfer. The motors (setMotor,
// setMotorsEnabled, the targets) have no in-scope caller.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/ragdoll/hkpragdollconstraintdata.h"

#include <stddef.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkrotationf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/quaternion/hkquaternionf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/hkpsolversse.h"

using namespace hkSolverSse;

static_assert(sizeof(hkpRagdollConstraintData) == 0x180, "hkpRagdollConstraintData");
static_assert(sizeof(hkpRagdollConstraintData::Atoms) == 0x170, "hkpRagdollConstraintData::Atoms");
static_assert(sizeof(hkpRagdollConstraintData::Runtime) == 0xB0, "hkpRagdollConstraintData::Runtime");
static_assert(offsetof(hkpRagdollConstraintData, m_atoms) == 0x10, "hkpRagdollConstraintData::m_atoms");
static_assert(offsetof(hkpRagdollConstraintData::Atoms, m_ragdollMotors) == 0xA0, "Atoms::m_ragdollMotors");
static_assert(offsetof(hkpRagdollConstraintData::Atoms, m_ballSocket) == 0x160, "Atoms::m_ballSocket");
static_assert(sizeof(hkpRagdollMotorConstraintAtom) == 0x50, "hkpRagdollMotorConstraintAtom");
static_assert(offsetof(hkpRagdollMotorConstraintAtom, m_motors) == 0x40, "hkpRagdollMotorConstraintAtom::m_motors");
static_assert(sizeof(hkpConeLimitConstraintAtom) == 0x20, "hkpConeLimitConstraintAtom");
static_assert(sizeof(hkpAngMotorConstraintAtom) == 0x20, "hkpAngMotorConstraintAtom");
static_assert(offsetof(hkpAngMotorConstraintAtom, m_targetAngle) == 0x10, "hkpAngMotorConstraintAtom::m_targetAngle");

namespace
{
// hkVector4f::normalize<3>: v * invSqrt23(|v|^2) in all four lanes
__forceinline Q normalize3(const Q& v)
{
    return _mm_mul_ps(invSqrt23(hadd3(_mm_mul_ps(v, v))), v);
}
}

// 0x00D6EF30: the transforms keep their type only (the data's constructor sets them); the ball socket uses the old
// solving method and no stabilization
hkpRagdollConstraintData::Atoms::Atoms()
{
    m_setupStabilization.m_enabled = false;
    m_setupStabilization.m_maxLinImpulse = HK_REAL_MAX;
    m_setupStabilization.m_maxAngImpulse = HK_REAL_MAX;
    m_setupStabilization.m_maxAngle = HK_REAL_HIGH;
    m_ragdollMotors.m_isEnabled = true;
    m_ragdollMotors.m_initializedOffset = -1;
    m_ragdollMotors.m_previousTargetAnglesOffset = -1;
    m_angFriction.m_isEnabled = 1;
    m_angFriction.m_numFrictionAxes = 1;
    m_twistLimit.m_isEnabled = 1;
    m_coneLimit.m_isEnabled = 1;
    m_coneLimit.m_memOffsetToAngleOffset = 0;
    m_planesLimit.m_isEnabled = 1;
    m_planesLimit.m_memOffsetToAngleOffset = 0;
    m_ballSocket.m_solvingMethod.m_storage = hkpConstraintAtom::METHOD_OLD;
    m_ballSocket.m_bodiesToNotify = 0;
    const float one = 1.0f;
    m_ballSocket.m_velocityStabilizationFactor = one;
    m_ballSocket.m_enableLinearImpulseLimit = false;
    m_ballSocket.m_breachImpulse = HK_REAL_MAX;
    m_ballSocket.m_inertiaStabilizationFactor = 0.0f;
}

// 0x00D6E4E0: identity constraint spaces, three friction axes without torque, motors off (their runtime offsets set),
// twist +-0.5236 (30 degrees), cone up to 1 (min -100 marks the cone), planes +-0.025, tau factors 0.8
hkpRagdollConstraintData::hkpRagdollConstraintData()
{
    m_atoms.m_transforms.m_transformA.m_rotation.m_col0.m_quad = g_vectorfConstants[HK_QUADREAL_1000];
    m_atoms.m_transforms.m_transformA.m_rotation.m_col1.m_quad = g_vectorfConstants[HK_QUADREAL_0100];
    m_atoms.m_transforms.m_transformA.m_rotation.m_col2.m_quad = g_vectorfConstants[HK_QUADREAL_0010];
    m_atoms.m_transforms.m_transformA.m_translation.setZero();
    m_atoms.m_transforms.m_transformB.m_rotation.m_col0.m_quad = g_vectorfConstants[HK_QUADREAL_1000];
    m_atoms.m_transforms.m_transformB.m_rotation.m_col1.m_quad = g_vectorfConstants[HK_QUADREAL_0100];
    m_atoms.m_transforms.m_transformB.m_translation.setZero();
    m_atoms.m_transforms.m_transformB.m_rotation.m_col2.m_quad = g_vectorfConstants[HK_QUADREAL_0010];
    m_atoms.m_angFriction.m_maxFrictionTorque = 0.0f;
    m_atoms.m_angFriction.m_firstFrictionAxis = 0;
    m_atoms.m_angFriction.m_numFrictionAxes = 3;
    m_atoms.m_ragdollMotors.m_isEnabled = false;
    m_atoms.m_ragdollMotors.m_initializedOffset = (short)offsetof(Runtime, m_initialized); // 0x90
    m_atoms.m_ragdollMotors.m_previousTargetAnglesOffset = (short)offsetof(Runtime, m_previousTargetAngles); // 0x94
    m_atoms.m_ragdollMotors.m_target_bRca.m_col0.m_quad = g_vectorfConstants[HK_QUADREAL_1000];
    m_atoms.m_ragdollMotors.m_target_bRca.m_col1.m_quad = g_vectorfConstants[HK_QUADREAL_0100];
    m_atoms.m_ragdollMotors.m_target_bRca.m_col2.m_quad = g_vectorfConstants[HK_QUADREAL_0010];
    m_atoms.m_twistLimit.m_minAngle = -0.5236f;
    m_atoms.m_twistLimit.m_maxAngle = 0.5236f;
    m_atoms.m_ragdollMotors.m_motors[0] = nullptr;
    m_atoms.m_ragdollMotors.m_motors[1] = nullptr;
    m_atoms.m_ragdollMotors.m_motors[2] = nullptr;
    m_atoms.m_twistLimit.m_twistAxis = Atoms::AXIS_TWIST;
    m_atoms.m_twistLimit.m_refAxis = Atoms::AXIS_PLANES;
    m_atoms.m_twistLimit.m_angularLimitsTauFactor = 0.8f;
    m_atoms.m_twistLimit.m_angularLimitsDampFactor = 1.0f;
    m_atoms.m_coneLimit.m_twistAxisInA = Atoms::AXIS_TWIST;
    m_atoms.m_coneLimit.m_refAxisInB = Atoms::AXIS_TWIST;
    m_atoms.m_coneLimit.m_angleMeasurementMode.m_storage = hkpConeLimitConstraintAtom::ZERO_WHEN_VECTORS_ALIGNED;
    m_atoms.m_coneLimit.m_minAngle = -100.0f;
    m_atoms.m_coneLimit.m_maxAngle = 1.0f;
    m_atoms.m_coneLimit.m_angularLimitsTauFactor = 0.8f;
    m_atoms.m_coneLimit.m_angularLimitsDampFactor = 1.0f;
    setConeLimitStabilization(true);
    m_atoms.m_planesLimit.m_minAngle = -0.025f;
    m_atoms.m_planesLimit.m_twistAxisInA = Atoms::AXIS_TWIST;
    m_atoms.m_planesLimit.m_refAxisInB = Atoms::AXIS_PLANES;
    m_atoms.m_planesLimit.m_angleMeasurementMode.m_storage = hkpConeLimitConstraintAtom::ZERO_WHEN_VECTORS_PERPENDICULAR;
    m_atoms.m_planesLimit.m_maxAngle = 0.025f;
    m_atoms.m_planesLimit.m_angularLimitsTauFactor = 0.8f;
    m_atoms.m_planesLimit.m_angularLimitsDampFactor = 1.0f;
}

// 0x00D6E870
hkpRagdollConstraintData::~hkpRagdollConstraintData()
{
}

// 0x00D6EC50: A's and B's constraint spaces from the twist and plane axes (normalized, the third column their
// cross product); the motor target is B's space
void hkpRagdollConstraintData::setInBodySpace(const hkVector4f& pivotA, const hkVector4f& pivotB, const hkVector4f& planeAxisA, const hkVector4f& planeAxisB, const hkVector4f& twistAxisA, const hkVector4f& twistAxisB)
{
    hkRotationf& rotA = m_atoms.m_transforms.m_transformA.m_rotation;
    hkRotationf& rotB = m_atoms.m_transforms.m_transformB.m_rotation;
    rotA.m_col0.m_quad = normalize3(twistAxisA.m_quad);
    rotA.m_col1.m_quad = normalize3(planeAxisA.m_quad);
    rotA.m_col2.m_quad = cross(rotA.m_col0.m_quad, rotA.m_col1.m_quad);
    m_atoms.m_transforms.m_transformA.m_translation = pivotA;
    rotB.m_col0.m_quad = normalize3(twistAxisB.m_quad);
    rotB.m_col1.m_quad = normalize3(planeAxisB.m_quad);
    rotB.m_col2.m_quad = cross(rotB.m_col0.m_quad, rotB.m_col1.m_quad);
    m_atoms.m_transforms.m_transformB.m_translation = pivotB;
    m_atoms.m_ragdollMotors.m_target_bRca.m_col0 = rotB.m_col0;
    m_atoms.m_ragdollMotors.m_target_bRca.m_col1 = rotB.m_col1;
    m_atoms.m_ragdollMotors.m_target_bRca.m_col2 = rotB.m_col2;
}

// 0x00D6E990: the twist and plane axes (normalized, the third column their cross product) in each body's space, the
// pivot too; the motor target is B's space (havok_create_ragdoll_constraint, havok_constraint_transfer)
void hkpRagdollConstraintData::setInWorldSpace(const hkTransformf& bodyATransform, const hkTransformf& bodyBTransform, const hkVector4f& pivot, const hkVector4f& twistAxisW, const hkVector4f& planeAxisW)
{
    hkVector4f twist;
    hkVector4f plane;
    hkVector4f third;
    twist.m_quad = normalize3(twistAxisW.m_quad);
    plane.m_quad = normalize3(planeAxisW.m_quad);
    third.m_quad = cross(twist.m_quad, plane.m_quad);
    hkRotationf& rotA = m_atoms.m_transforms.m_transformA.m_rotation;
    hkRotationf& rotB = m_atoms.m_transforms.m_transformB.m_rotation;
    rotA.m_col0.setRotatedInverseDir(bodyATransform.m_rotation, twist);
    rotA.m_col1.setRotatedInverseDir(bodyATransform.m_rotation, plane);
    rotA.m_col2.setRotatedInverseDir(bodyATransform.m_rotation, third);
    rotB.m_col0.setRotatedInverseDir(bodyBTransform.m_rotation, twist);
    rotB.m_col1.setRotatedInverseDir(bodyBTransform.m_rotation, plane);
    rotB.m_col2.setRotatedInverseDir(bodyBTransform.m_rotation, third);
    m_atoms.m_transforms.m_transformA.m_translation.setTransformedInversePos(bodyATransform, pivot);
    m_atoms.m_transforms.m_transformB.m_translation.setTransformedInversePos(bodyBTransform, pivot);
    m_atoms.m_ragdollMotors.m_target_bRca.m_col0 = rotB.m_col0;
    m_atoms.m_ragdollMotors.m_target_bRca.m_col1 = rotB.m_col1;
    m_atoms.m_ragdollMotors.m_target_bRca.m_col2 = rotB.m_col2;
}

// 0x00D6E6C0: B's twist axis turned about its plane axis to the middle of the range, the third axis again their cross
// product, and a symmetric cone half the range wide
void hkpRagdollConstraintData::setAsymmetricConeAngle(float cone_min, float cone_max)
{
    hkRotationf& rotB = m_atoms.m_transforms.m_transformB.m_rotation;
    const float angle = lane0(_mm_xor_ps(_mm_set_ss((cone_min + cone_max) * 0.5f), _mm_castsi128_ps(_mm_set1_epi32(0x80000000))));
    hkQuaternionf rotation;
    rotation.setAxisAngle(rotB.m_col1, angle);
    rotB.m_col0.setRotatedDir(rotation, rotB.m_col0);
    rotB.m_col2.m_quad = cross(rotB.m_col0.m_quad, rotB.m_col1.m_quad);
    m_atoms.m_coneLimit.m_maxAngle = (cone_max - cone_min) * 0.5f;
}

// 0x00D6E2B0: the offset is 0x38 (7 * 8, the binary's byte arithmetic) from the instance runtime. Kept as the binary
// has it, although Runtime::m_coneAngleOffset is at 0xA0: the stabilization keeps its offset in friction result 3.
void hkpRagdollConstraintData::setConeLimitStabilization(hkBool enable)
{
    m_atoms.m_coneLimit.m_memOffsetToAngleOffset = (unsigned char)((enable.m_bool != 0) * 0x38);
}

// 0x00D6E290
void hkpRagdollConstraintData::setMaxFrictionTorque(float tmag)
{
    m_atoms.m_angFriction.m_maxFrictionTorque = tmag;
}

// 0x00D6E440
int hkpRagdollConstraintData::getType() const
{
    return CONSTRAINT_TYPE_RAGDOLL;
}

// 0x00D6E250
void hkpRagdollConstraintData::getConstraintInfo(hkpConstraintData::ConstraintInfo& infoOut) const
{
    getConstraintInfoUtil(m_atoms.getAtoms(), sizeof(Atoms), infoOut);
}

// 0x00D6E750: a solving method, orthonormal spaces, a cone (min -100) between 0 and pi, ordered plane and twist limits
hkBool hkpRagdollConstraintData::isValid() const
{
    if (m_atoms.m_ballSocket.m_solvingMethod.m_storage == hkpConstraintAtom::METHOD_STABILIZED && !m_atoms.m_setupStabilization.m_enabled.m_bool)
    {
        return false;
    }
    if (m_atoms.m_transforms.m_transformA.m_rotation.isOrthonormal(1e-05f) && m_atoms.m_transforms.m_transformB.m_rotation.isOrthonormal(1e-05f)
        && m_atoms.m_coneLimit.m_minAngle == -100.0f && 0.0f <= m_atoms.m_coneLimit.m_maxAngle && m_atoms.m_coneLimit.m_maxAngle <= 3.1415927f
        && m_atoms.m_planesLimit.m_minAngle <= m_atoms.m_planesLimit.m_maxAngle && m_atoms.m_twistLimit.m_minAngle <= m_atoms.m_twistLimit.m_maxAngle)
    {
        return true;
    }
    return false;
}

// 0x00D6E1F0
void hkpRagdollConstraintData::setBreachImpulse(float breachImpulse)
{
    m_atoms.m_ballSocket.m_breachImpulse = breachImpulse;
}

// 0x00D6E210
float hkpRagdollConstraintData::getBreachImpulse() const
{
    return m_atoms.m_ballSocket.m_breachImpulse;
}

// 0x00D6E220
void hkpRagdollConstraintData::setBodyToNotify(int bodyIdx)
{
    m_atoms.m_ballSocket.m_bodiesToNotify = (unsigned char)(1 << (bodyIdx & 0x1F));
}

// 0x00D6E240
unsigned char hkpRagdollConstraintData::getNotifiedBodyIndex() const
{
    return m_atoms.m_ballSocket.m_bodiesToNotify >> 1;
}

// 0x00D6E960
void hkpRagdollConstraintData::setSolvingMethod(hkpConstraintAtom::SolvingMethod method)
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

// 0x00D6E470: clamped to [0, 1]
hkResult hkpRagdollConstraintData::setInertiaStabilizationFactor(const float inertiaStabilizationFactorIn)
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

// 0x00D6E450
hkResult hkpRagdollConstraintData::getInertiaStabilizationFactor(float& inertiaStabilizationFactorOut) const
{
    inertiaStabilizationFactorOut = m_atoms.m_ballSocket.m_inertiaStabilizationFactor;
    return HK_SUCCESS;
}

// 0x00D6E4B0: the three linear results' impulses
void hkpRagdollConstraintData::getAppliedLinearImpulse(const hkTransformf& worldFromBodyA, const hkTransformf& worldFromBodyB, const hkpConstraintRuntime* runtimeIn, hkVector4f& impulseOut) const
{
    const Runtime* runtime = reinterpret_cast<const Runtime*>(runtimeIn);
    impulseOut.m_quad = _mm_setr_ps(runtime->m_solverResults[SOLVER_RESULT_LIN_0].m_impulseApplied, runtime->m_solverResults[SOLVER_RESULT_LIN_1].m_impulseApplied, runtime->m_solverResults[SOLVER_RESULT_LIN_2].m_impulseApplied, 0.0f);
}

// 0x00D6E270
void hkpRagdollConstraintData::getRuntimeInfo(hkBool wantRuntime, hkpConstraintData::RuntimeInfo& infoOut) const
{
    infoOut.m_numSolverResults = SOLVER_RESULT_MAX;
    infoOut.m_sizeOfExternalRuntime = sizeof(Runtime);
}
