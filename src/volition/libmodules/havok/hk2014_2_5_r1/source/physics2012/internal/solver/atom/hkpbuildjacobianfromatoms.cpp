// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/atom/hkpbuildjacobianfromatoms.cpp
//
// Havok 2014.2.5 (havok_impl.md HK8, multiplayer.md MP7): the Jacobians of a constraint's atoms other than a bare
// contact atom, from rfg.exe (written from the disassembly of 0x00E92680, which inlines the builders of
// hkpbuildjacobianfromatoms.hxx, in its SSE operation order). Ported are the paths a contact constraint and its
// response modifiers reach (HK8) and those of the ragdoll joints (MP7: hkpRagdollConstraintData,
// hkpLimitedHingeConstraintData and hkpBallAndSocketConstraintData as RFG builds them):
// - the local transforms or translations (the constraint bases), the 2d angular, angular limit, twist limit and cone
//   limit (plain and with its angle offset), angular friction and ball socket atoms;
// - the stabilization atom and the motors when off (RFG never calls setSolvingMethod or enables a motor), which only
//   skip their solver results;
// - since stuck projectiles (hkpPrismaticConstraintData): the linear, linear limit, angular and linear friction atoms,
//   and the linear motor when off (rfg.exe never enables a prismatic motor);
// - the hinge's angular motor when on (havok_constraint_add_motor), and the bridge atom of a breakable constraint.
// The other atom types (soft linear, pulley, chains, deformable, the stabilized ball socket, the linear and ragdoll
// motors when on, the other modifiers) report an error and end the schema list.
// The binary keeps the constraint bases (the bodies' transforms times the local transforms) on its stack and advances
// out.m_constraintRuntime by each atom's solver results (8 bytes each); the limits read the previous angle there.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/atom/hkpbuildjacobianfromatoms.h"

#include <string.h>
#include <intrin.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/atom/bridge/hkpbridgeconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/atom/hkpconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/motor/hkpmotorconstraintinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/accumulator/hkpvelocityaccumulator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/bilateral/hkp1dbilateralconstraintinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/hkpconstraintqueryin.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/hkpconstraintqueryout.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/jacobian/hkpjacobianschema.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverresults.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpmodifierconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpsimplecontactconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/contact/hkpsimplecontactconstraintinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/hkpsolversse.h"

using namespace hkSolverSse;

// 0x00E8B270: the header schema of a constraint (the query's accumulators and instance)
void hkpBeginConstraints(const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out, hkpSolverResults* sr, int solverResultStriding)
{
    hkpJacobianHeaderSchema* header = reinterpret_cast<hkpJacobianHeaderSchema*>(out.m_jacobianSchemas.val());
    header->m_bodyBIndex = (unsigned short)in.m_accumulatorBIndex.val();
    header->m_constraintInstance = in.m_constraintInstance;
    hkSolverSse::setSchemaType(header, hkpJacobianSchema::SCHEMA_TYPE_HEADER);
    header->m_bodyAIndex = (unsigned short)in.m_accumulatorAIndex.val();
    header->m_solverResultInMainMemory = sr;
    header->m_solverResultStriding = (unsigned char)solverResultStriding;
    out.m_jacobianSchemas = reinterpret_cast<hkpJacobianSchema*>(header + 1);
}

// 0x00E8B0E0 (an LTCG copy of hkpBeginConstraints, inlined in the binary's callers)
static inline void hkInitHeader(const hkpConstraintQueryIn& in, hkpSolverResults* results, int resultStriding, hkpConstraintQueryOut& out)
{
    hkpBeginConstraints(in, out, results, resultStriding);
}

namespace
{
__forceinline Q bits(unsigned int b) { return _mm_castsi128_ps(_mm_set1_epi32((int)b)); }

// the constants of hkMath (hkSseMathFuncs.inl) as rfg.exe has them (0x0134C2C0.., 0x01365580..)
__forceinline Q signMask() { return bits(0x80000000u); }
__forceinline Q pi() { return bits(0x40490FDBu); }
__forceinline Q piOver2() { return bits(0x3FC90FDBu); }
__forceinline Q floatOne() { return bits(0x3F800000u); }

// hkMath::quadAtan2 with quadAtan inlined (Moshier's rational approximation, Estrin evaluated)
__forceinline Q quadAtan2(const Q& y, const Q& x)
{
    const Q zero = _mm_setzero_ps();
    const Q ySign = _mm_and_ps(y, signMask());
    const Q xIsZero = _mm_cmpeq_ps(x, zero);
    const Q ratio = _mm_div_ps(y, x);
    const Q select = _mm_cmplt_ps(floatOne(), _mm_andnot_ps(signMask(), ratio));
    const Q bias = _mm_and_ps(_mm_or_ps(_mm_and_ps(ratio, signMask()), piOver2()), select);
    const Q invRatio = _mm_xor_ps(_mm_mul_ps(_mm_sub_ps(g_vectorfConstants[HK_QUADREAL_2], _mm_mul_ps(_mm_rcp_ps(ratio), ratio)), _mm_rcp_ps(ratio)), signMask());
    const Q xs = _mm_or_ps(_mm_and_ps(invRatio, select), _mm_andnot_ps(select, ratio));
    const Q yIsZero = _mm_cmpeq_ps(y, zero);
    const Q eitherZero = _mm_or_ps(yIsZero, xIsZero);
    const Q z = _mm_mul_ps(xs, xs);
    Q p = _mm_add_ps(_mm_mul_ps(z, bits(0x3B3BD74Au)), bits(0xBBEB7F9Eu)); // a1, a0
    p = _mm_mul_ps(p, _mm_mul_ps(z, xs));
    p = _mm_mul_ps(p, _mm_add_ps(_mm_mul_ps(_mm_add_ps(z, bits(0x3FDA2291u)), z), bits(0x400F22D2u))); // e1, e0
    const Q d = _mm_add_ps(_mm_mul_ps(_mm_add_ps(z, bits(0xBF809FF7u)), z), bits(0x40712D99u)); // d1, d0
    const Q b = _mm_add_ps(_mm_mul_ps(_mm_add_ps(z, bits(0xC0753EB3u)), z), bits(0x40B019CAu)); // b1, b0
    p = _mm_mul_ps(p, _mm_mul_ps(d, b));
    const Q atanRatio = _mm_add_ps(p, _mm_add_ps(bias, xs));
    const Q wx = _mm_xor_ps(_mm_and_ps(_mm_cmplt_ps(x, zero), pi()), ySign);
    const Q wy = _mm_andnot_ps(yIsZero, _mm_xor_ps(ySign, piOver2()));
    const Q w = _mm_or_ps(_mm_andnot_ps(xIsZero, wx), _mm_and_ps(wy, xIsZero));
    return _mm_add_ps(_mm_andnot_ps(eitherZero, atanRatio), w);
}

// hkMath::quadFloor without SSE4.1 (2^23 rounding, then the off-by-one correction)
__forceinline Q quadFloor(const Q& v)
{
    const Q two23 = bits(0x4B000000u);
    const Q d = _mm_sub_ps(_mm_add_ps(_mm_add_ps(_mm_sub_ps(v, two23), two23), two23), two23);
    const Q absV = _mm_castsi128_ps(_mm_srli_epi32(_mm_slli_epi32(_mm_castps_si128(v), 1), 1));
    const Q large = _mm_cmplt_ps(two23, absV);
    const Q t = _mm_add_ps(_mm_cvtepi32_ps(_mm_castps_si128(_mm_cmplt_ps(v, d))), d);
    return _mm_or_ps(_mm_andnot_ps(large, t), _mm_and_ps(large, v));
}

// the solver results this atom writes (8 bytes each) are skipped in out.m_constraintRuntime
__forceinline void advanceRuntime(hkpConstraintQueryOut& out, int bytes)
{
    out.m_constraintRuntime = reinterpret_cast<hkpConstraintRuntime*>(reinterpret_cast<char*>(out.m_constraintRuntime.val()) + bytes);
}

__forceinline float previousAngle(const hkpConstraintQueryOut& out)
{
    return reinterpret_cast<const hkpSolverResults*>(out.m_constraintRuntime.val())->m_internalSolverData;
}

// a disabled atom: its solver results are skipped in the solver too
__forceinline void shiftSolverResults(hkpConstraintQueryOut& out, int numResults)
{
    hkpJacobianShiftSolverResultsPointerSchema* schema = reinterpret_cast<hkpJacobianShiftSolverResultsPointerSchema*>(out.m_jacobianSchemas.val());
    schema->initShiftSolverResultsPointer((unsigned char)numResults);
    out.m_jacobianSchemas = reinterpret_cast<hkpJacobianSchema*>(schema + 1);
}

// hkp1dAngularLimitInfo as the dispatcher keeps it
struct AngularLimit
{
    Q dof;
    float min;
    float max;
    float computedAngle;
    float tau;
    float damping;
};

// hk1dAngularLimitBuildJacobian (0x00E8D630, inlined): the row around the axis, the effective mass scaled by tau, the
// angle and the limits as right hand sides (-invDt each, the limits also by tau)
void buildAngularLimit(const AngularLimit& info, const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out)
{
    hkpJacobian1dAngularLimitsSchema* schema = reinterpret_cast<hkpJacobian1dAngularLimitsSchema*>(out.m_jacobianSchemas.val());
    const hkpVelocityAccumulator* mA = in.m_bodyA.val();
    const hkpVelocityAccumulator* mB = in.m_bodyB.val();
    const Q ang0 = rotate(mA->m_scratch0.m_quad, mA->m_scratch1.m_quad, mA->m_scratch2.m_quad, info.dof);
    schema->m_jac.m_angular[0].m_quad = ang0;
    const Q negDof = _mm_xor_ps(signMask(), info.dof);
    const Q ang1 = rotate(mB->m_scratch0.m_quad, mB->m_scratch1.m_quad, mB->m_scratch2.m_quad, negDof);
    schema->m_jac.m_angular[1].m_quad = ang1;
    // hkp2AngJacobian::getAngularDiag (0x00D74040)
    const Q diag = _mm_add_ps(hadd3(_mm_add_ps(_mm_mul_ps(_mm_mul_ps(ang0, ang0), mA->m_invMasses.m_quad), _mm_mul_ps(_mm_mul_ps(ang1, ang1), mB->m_invMasses.m_quad))), g_vectorfConstants[HK_QUADREAL_EPS]);
    const Q tau = splat(info.tau);
    schema->m_jac.m_angular[0].m_quad = setW(ang0, _mm_mul_ps(rcp23(diag), tau));
    const float negInvDt = lane0(_mm_xor_ps(_mm_set_ss(in.m_subStepInvDeltaTime.val()), signMask()));
    schema->m_jac.m_angular[1].m_quad.m128_f32[3] = info.computedAngle * negInvDt;
    out.m_jacobianSchemas = reinterpret_cast<hkpJacobianSchema*>(schema + 1);
    schema->m_minRhs = (info.min * negInvDt) * info.tau;
    schema->m_maxRhs = (info.max * negInvDt) * info.tau;
    schema->m_tau = info.tau;
    schema->m_damping = info.damping;
    setSchemaType(schema, hkpJacobianSchema::SCHEMA_TYPE_1D_ANGULAR_LIMITS);
}

// the angle of v around an axis, unwrapped towards -previous: whole turns are taken off (v + pi) / 2pi until the
// angle stays in [-pi, pi]; returns angle minus those turns
float unwrapAngle(const Q& angle, float previous)
{
    const Q negPrevious = splat(previous * -1.0f);
    const Q invTwoPi = _mm_div_ps(g_vectorfConstants[HK_QUADREAL_1], g_vectorfConstants[HK_QUADREAL_TWO_PI]);
    const Q c = g_vectorfConstants[HK_QUADREAL_PI];
    const Q negPi = _mm_sub_ps(_mm_setzero_ps(), c);
    Q v = _mm_sub_ps(angle, negPrevious);
    Q turns = _mm_setzero_ps();
    for (;;)
    {
        const Q f = quadFloor(_mm_mul_ps(_mm_add_ps(v, c), invTwoPi));
        turns = _mm_add_ps(f, turns);
        v = _mm_sub_ps(v, _mm_mul_ps(f, g_vectorfConstants[HK_QUADREAL_TWO_PI]));
        const Q below = _mm_cmplt_ps(v, c);
        const Q clamped = _mm_max_ps(negPi, _mm_or_ps(_mm_and_ps(below, v), _mm_andnot_ps(below, c)));
        // UCOMISS + JNZ: an unordered compare ends the loop
        if (!(lane0(clamped) != lane0(v)) || lane0(clamped) != lane0(clamped) || lane0(v) != lane0(v))
        {
            break;
        }
    }
    return lane0(_mm_sub_ps(angle, _mm_mul_ps(turns, g_vectorfConstants[HK_QUADREAL_TWO_PI])));
}

// the angular motor case of 0x00E92680: the current angle around A's motor axis is atan2(B[a+2] . A[a+1],
// (B[a+2] x B[a]) . A[a+1]), unwrapped next to the limit's previous angle when the atom names the limit's result; the
// motor (hkCalcMotorData) gets the row's effective mass, the last results, the target's change since the previous
// target (the current angle on the first build) and the position error; the row's right hand side is the motor's
// target position by the inverse sub step, its impulses the motor's forces by the micro step. Afterwards the previous
// target becomes the atom's target.
void buildAngularMotor(const hkpAngMotorConstraintAtom& atom, const hkTransformf& baseA, const hkTransformf& baseB, const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out)
{
    const int axis = atom.m_motorAxis;
    const hkVector4f* colsA = &baseA.m_rotation.m_col0;
    const hkVector4f* colsB = &baseB.m_rotation.m_col0;
    const Q b2 = colsB[(axis + 2) % 3].m_quad;
    const Q a1 = colsA[(axis + 1) % 3].m_quad;
    const Q y = hadd3(_mm_mul_ps(b2, a1));
    const Q x = hadd3(_mm_mul_ps(cross(b2, colsB[axis].m_quad), a1));
    const Q atan = quadAtan2(y, x);
    float angle = lane0(atan);
    char* instance = reinterpret_cast<char*>(out.m_instanceRuntime.val());
    if (atom.m_correspondingAngLimitSolverResultOffset != 0)
    {
        angle = unwrapAngle(atan, *reinterpret_cast<const float*>(instance + atom.m_correspondingAngLimitSolverResultOffset + 4));
    }
    float* previousTargetAngle = reinterpret_cast<float*>(instance + atom.m_previousTargetAngleOffset);
    float previousTarget = *previousTargetAngle;
    unsigned char* initialized = reinterpret_cast<unsigned char*>(instance + atom.m_initializedOffset);
    if (*initialized == 0)
    {
        *initialized = 1;
        previousTarget = angle;
    }

    hkpJacobian1dAngularMotorSchema* schema = reinterpret_cast<hkpJacobian1dAngularMotorSchema*>(out.m_jacobianSchemas.val());
    const hkpVelocityAccumulator* mA = in.m_bodyA.val();
    const hkpVelocityAccumulator* mB = in.m_bodyB.val();
    const Q dof = colsA[axis].m_quad;
    const Q ang0 = rotate(mA->m_scratch0.m_quad, mA->m_scratch1.m_quad, mA->m_scratch2.m_quad, dof);
    const Q ang1 = rotate(mB->m_scratch0.m_quad, mB->m_scratch1.m_quad, mB->m_scratch2.m_quad, _mm_xor_ps(signMask(), dof));
    schema->m_jac.m_angular[1].m_quad = ang1;
    // hkp2AngJacobian::getAngularDiag (0x00D74040)
    const Q diag = _mm_add_ps(hadd3(_mm_add_ps(_mm_mul_ps(_mm_mul_ps(ang0, ang0), mA->m_invMasses.m_quad), _mm_mul_ps(_mm_mul_ps(ang1, ang1), mB->m_invMasses.m_quad))), g_vectorfConstants[HK_QUADREAL_EPS]);
    schema->m_jac.m_angular[0].m_quad = setW(ang0, _mm_mul_ps(rcp23(diag), splat(1.0f)));

    hkpConstraintMotorInput input;
    input.m_effMass = schema->m_jac.m_angular[0].m_quad.m128_f32[3];
    input.m_stepInfo = &in;
    input.m_lastResults = *reinterpret_cast<const hkpSolverResults*>(out.m_constraintRuntime.val());
    input.m_deltaTarget = atom.m_targetAngle - previousTarget;
    input.m_positionError = previousTarget - angle;
    hkpConstraintMotorOutput output;
    hkCalcMotorData(atom.m_motor, &input, &output);
    schema->m_jac.m_angular[1].m_quad.m128_f32[3] = in.m_subStepInvDeltaTime.val() * output.m_targetPosition;
    setSchemaType(schema, hkpJacobianSchema::SCHEMA_TYPE_1D_ANGULAR_MOTOR);
    schema->m_maxImpulsePerSubstep = output.m_maxForce * in.m_microStepDeltaTime.val();
    schema->m_minImpulsePerSubstep = output.m_minForce * in.m_microStepDeltaTime.val();
    schema->m_velocity = output.m_targetVelocity;
    schema->m_tau = output.m_tau;
    schema->m_damping = output.m_damping;
    out.m_jacobianSchemas = reinterpret_cast<hkpJacobianSchema*>(schema + 1);
    // UCOMISS + LAHF/TEST 0x44 + JNP: stored unless equal (an unordered compare stores)
    if (!(*previousTargetAngle == atom.m_targetAngle))
    {
        *previousTargetAngle = atom.m_targetAngle;
    }
}
}

// 0x00E8E800: the ball socket's three rows (hkpConstraintAtom::METHOD_OLD; the stabilized method is not ported, see
// the file comment). Each world axis gets a bilateral row through the pivots whose right hand side is the pivots'
// separation predicted by the bodies' angular velocities (scaled by 1 / (rhsFactor * velocityStabilizationFactor)),
// times rhsFactor; a breach impulse other than HK_REAL_MAX makes it a limited row. A separation beyond
// in.m_maxConstraintViolationSqrd lists the constraint in in.m_violatedConstraints.
void buildSchemaFromBallSocketAtom(const hkpBallSocketConstraintAtom& atom, const hkpConstraintQueryIn& in, const hkTransformf& baseA, const hkTransformf& baseB, const hkVector4f& vLocalArmA, const hkVector4f& vLocalArmB, const hkSimdFloat32& maxAngularImpulse, const hkSimdFloat32& maxLinearImpulse, hkpConstraintQueryOut& out)
{
    if (atom.m_solvingMethod.m_storage != hkpConstraintAtom::METHOD_OLD)
    {
        hkErrorFwd::messageError(0x4F704552, "OpenRFG: buildSchemaFromBallSocketAtom: the stabilized method is not ported", "hkpbuildjacobianfromatoms.cpp", __LINE__);
        return;
    }
    const float velocityStabilization = (float)atom.m_velocityStabilizationFactor;
    const float breachImpulse = atom.m_breachImpulse;
    const Q invFactor = rcp23(splat(in.m_rhsFactor.val() * velocityStabilization));
    const hkpVelocityAccumulator* mA = in.m_bodyA.val();
    const hkpVelocityAccumulator* mB = in.m_bodyB.val();

    // the angular velocities back to world space (the transposed core-from-world rotations)
    struct Transposed
    {
        Q r0, r1, r2;
        explicit Transposed(const hkpVelocityAccumulator* acc)
        {
            const Q c0 = acc->m_scratch0.m_quad;
            const Q c1 = acc->m_scratch1.m_quad;
            const Q c2 = acc->m_scratch2.m_quad;
            const Q lo = _mm_unpacklo_ps(c0, c1);
            r2 = _mm_shuffle_ps(_mm_unpackhi_ps(c0, c1), c2, 0xE4);
            r0 = _mm_movelh_ps(lo, c2);
            r1 = _mm_shuffle_ps(_mm_movehl_ps(r0, lo), c2, 0xD4);
        }
    };
    const Transposed tA(mA);
    const Transposed tB(mB);
    const Q angA = rotate(tA.r0, tA.r1, tA.r2, mA->m_angularVel.m_quad);
    const Q angB = rotate(tB.r0, tB.r1, tB.r2, mB->m_angularVel.m_quad);
    const Q armA = _mm_sub_ps(baseA.m_translation.m_quad, mA->m_scratch3.m_quad);
    const Q armB = _mm_sub_ps(baseB.m_translation.m_quad, mB->m_scratch3.m_quad);
    const Q separation = _mm_sub_ps(baseB.m_translation.m_quad, baseA.m_translation.m_quad);
    const Q error = _mm_sub_ps(_mm_add_ps(_mm_mul_ps(cross(armB, angB), invFactor), separation), _mm_mul_ps(cross(armA, angA), invFactor));

    hkpViolatedConstraintArray* violated = in.m_violatedConstraints.val();
    if (violated != nullptr && lane0(hadd3(_mm_mul_ps(error, error))) > lane0(in.m_maxConstraintViolationSqrd.m_real))
    {
        // hkpViolatedConstraintArray: m_nextFreeElement, then 128 instances
        long* nextFree = reinterpret_cast<long*>(violated);
        const unsigned int index = (unsigned int)_InterlockedExchangeAdd(nextFree, 1); // the binary calls KERNEL32 InterlockedExchangeAdd
        if (index < 0x80)
        {
            reinterpret_cast<hkpConstraintInstance**>(nextFree + 1)[index] = in.m_constraintInstance.val();
        }
    }

    hkpJacobian1dBilateralSchema* schema = reinterpret_cast<hkpJacobian1dBilateralSchema*>(out.m_jacobianSchemas.val());
    for (int i = 0; i < 3; i++, schema++)
    {
        const Q axis = g_vectorfConstants[HK_QUADREAL_1000 + i];
        const Q ang0 = rotate(mA->m_scratch0.m_quad, mA->m_scratch1.m_quad, mA->m_scratch2.m_quad, cross(armA, axis));
        const Q ang1 = rotate(mB->m_scratch0.m_quad, mB->m_scratch1.m_quad, mB->m_scratch2.m_quad, cross(axis, armB));
        const Q invMassSum = _mm_max_ps(_mm_add_ps(mB->m_invMasses.m_quad, mA->m_invMasses.m_quad), g_vectorfConstants[HK_QUADREAL_EPS_SQRD]);
        const Q angular = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(ang1, ang1), mB->m_invMasses.m_quad), _mm_mul_ps(_mm_mul_ps(ang0, ang0), mA->m_invMasses.m_quad));
        const Q diag = hadd4(setW(angular, invMassSum));
        schema->m_jac.m_angular[1].m_quad = setW(ang1, diag);
        schema->m_jac.m_angular[0].m_quad = setW(ang0, _mm_mul_ps(rcp23(diag), splat(in.m_virtMassFactor.val())));
        // the binary first stores (pivotB - pivotA) . axis * rhsFactor here and then overwrites it with the
        // predicted separation along the axis
        const float errorAlongAxis = error.m128_f32[i];
        schema->m_jac.m_linear0.m_quad = setW(axis, _mm_mul_ps(splat(errorAlongAxis), splat(in.m_rhsFactor.val())));
        if (breachImpulse == HK_REAL_MAX)
        {
            setSchemaType(schema, hkpJacobianSchema::SCHEMA_TYPE_1D_BILATERAL);
        }
        else
        {
            schema->m_jac.m_angular[1].m_quad.m128_f32[3] = breachImpulse;
            setSchemaType(schema, hkpJacobianSchema::SCHEMA_TYPE_1D_BILATERAL_WITH_IMPULSE_LIMIT);
        }
    }
    out.m_jacobianSchemas = reinterpret_cast<hkpJacobianSchema*>(schema);
}

// 0x00E92680: a header with the constraint's runtime as its solver results (8 bytes each), then each atom
void hkSolverBuildJacobianFromAtomsNotContact(const hkpConstraintAtom* atoms, int sizeOfAllAtoms, const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out)
{
    const char* atom = reinterpret_cast<const char*>(atoms);
    const char* atomsEnd = atom + sizeOfAllAtoms;
    // the constraint bases start as the bodies' transforms
    HK_ALIGN16(hkTransformf baseA);
    HK_ALIGN16(hkTransformf baseB);
    memcpy(&baseA, in.m_transformA.val(), sizeof(hkTransformf));
    memcpy(&baseB, in.m_transformB.val(), sizeof(hkTransformf));
    // what the stabilization atom would set up (never enabled here: the arms stay zero, the limits HK_REAL_MAX)
    hkVector4f armA;
    hkVector4f armB;
    armA.setZero();
    armB.setZero();
    hkSimdFloat32 maxAngularImpulse;
    hkSimdFloat32 maxLinearImpulse;
    maxAngularImpulse.m_real = g_vectorfConstants[HK_QUADREAL_MAX];
    maxLinearImpulse.m_real = g_vectorfConstants[HK_QUADREAL_MAX];
    hkInitHeader(in, reinterpret_cast<hkpSolverResults*>(out.m_constraintRuntime.val()), 8, out);
    while (atom < atomsEnd)
    {
        const hkpConstraintAtom* current = reinterpret_cast<const hkpConstraintAtom*>(atom);
        switch (current->m_type.m_storage)
        {
        case hkpConstraintAtom::TYPE_INVALID:
            // padding: the next atom starts at the next 16 bytes
            atom = reinterpret_cast<const char*>((reinterpret_cast<unsigned int>(atom) + 0xF) & ~0xF);
            continue;

        case hkpConstraintAtom::TYPE_BRIDGE:
        {
            // the data builds its own schemas (a breakable constraint: its child's atoms, or nothing once broken),
            // header included, so the header written above goes again
            const hkpBridgeConstraintAtom* bridge = static_cast<const hkpBridgeConstraintAtom*>(current);
            out.m_jacobianSchemas = reinterpret_cast<hkpJacobianSchema*>(reinterpret_cast<char*>(out.m_jacobianSchemas.val()) - sizeof(hkpJacobianHeaderSchema));
            bridge->m_buildJacobianFunc(bridge->m_constraintData, in, out);
            atom += sizeof(hkpBridgeConstraintAtom);
            // Original: the runtime pointer moves on by 0xC3500 bytes (nothing reads it after a bridge, the last atom)
            advanceRuntime(out, 0xC3500);
            continue;
        }

        case hkpConstraintAtom::TYPE_SET_LOCAL_TRANSFORMS:
        {
            // each base column (the pivot too) through the body's rotation; the body's position added to the pivot
            const hkpSetLocalTransformsConstraintAtom* transforms = static_cast<const hkpSetLocalTransformsConstraintAtom*>(current);
            const hkTransformf* bodies[2] = { in.m_transformA.val(), in.m_transformB.val() };
            const hkTransformf* locals[2] = { &transforms->m_transformA, &transforms->m_transformB };
            hkTransformf* bases[2] = { &baseA, &baseB };
            for (int b = 0; b < 2; b++)
            {
                const hkRotationf& r = bodies[b]->m_rotation;
                const hkVector4f* src = &locals[b]->m_rotation.m_col0;
                hkVector4f* dst = &bases[b]->m_rotation.m_col0;
                for (int c = 3; c >= 0; c--)
                {
                    dst[c].m_quad = rotate(r.m_col0.m_quad, r.m_col1.m_quad, r.m_col2.m_quad, src[c].m_quad);
                }
                bases[b]->m_translation.m_quad = _mm_add_ps(bodies[b]->m_translation.m_quad, bases[b]->m_translation.m_quad);
            }
            atom += sizeof(hkpSetLocalTransformsConstraintAtom);
            continue;
        }

        case hkpConstraintAtom::TYPE_SET_LOCAL_TRANSLATIONS:
        {
            // the pivots only; the bases keep the bodies' rotations
            const hkpSetLocalTranslationsConstraintAtom* translations = static_cast<const hkpSetLocalTranslationsConstraintAtom*>(current);
            const hkTransformf* bodyA = in.m_transformA.val();
            baseA.m_translation.m_quad = _mm_add_ps(rotate(bodyA->m_rotation.m_col0.m_quad, bodyA->m_rotation.m_col1.m_quad, bodyA->m_rotation.m_col2.m_quad, translations->m_translationA.m_quad), bodyA->m_translation.m_quad);
            const hkTransformf* bodyB = in.m_transformB.val();
            baseB.m_translation.m_quad = _mm_add_ps(rotate(bodyB->m_rotation.m_col0.m_quad, bodyB->m_rotation.m_col1.m_quad, bodyB->m_rotation.m_col2.m_quad, translations->m_translationB.m_quad), bodyB->m_translation.m_quad);
            atom += sizeof(hkpSetLocalTranslationsConstraintAtom);
            continue;
        }

        case hkpConstraintAtom::TYPE_SETUP_STABILIZATION:
            if (static_cast<const hkpSetupStabilizationAtom*>(current)->m_enabled.m_bool)
            {
                hkErrorFwd::messageError(0x4F704553, "OpenRFG: hkSolverBuildJacobianFromAtomsNotContact: stabilization not ported", "hkpbuildjacobianfromatoms.cpp", __LINE__);
                return;
            }
            atom += sizeof(hkpSetupStabilizationAtom);
            continue;

        case hkpConstraintAtom::TYPE_BALL_SOCKET:
            buildSchemaFromBallSocketAtom(*static_cast<const hkpBallSocketConstraintAtom*>(current), in, baseA, baseB, armA, armB, maxAngularImpulse, maxLinearImpulse, out);
            atom += sizeof(hkpBallSocketConstraintAtom);
            advanceRuntime(out, 3 * 8);
            continue;

        case hkpConstraintAtom::TYPE_2D_ANG:
        {
            // the two axes of B's base other than the free one stay perpendicular to A's free axis
            const int freeAxis = static_cast<const hkp2dAngConstraintAtom*>(current)->m_freeRotationAxis;
            const hkVector4f* colsA = &baseA.m_rotation.m_col0;
            const hkVector4f* colsB = &baseB.m_rotation.m_col0;
            const hkVector4f& b1 = colsB[(freeAxis + 1) % 3];
            const hkVector4f& b2 = colsB[(freeAxis + 2) % 3];
            hkp1dAngularBilateralConstraintInfo info;
            info.m_zeroErrorAxisAinW = colsA[freeAxis];
            info.m_perpZeroErrorAxisBinW = b1;
            info.m_constrainedDofW = b2;
            hk1dAngularBilateralConstraintBuildJacobian(info, in, out);
            info.m_perpZeroErrorAxisBinW = b2;
            info.m_constrainedDofW.m_quad = _mm_xor_ps(signMask(), b1.m_quad);
            hk1dAngularBilateralConstraintBuildJacobian(info, in, out);
            atom += sizeof(hkp2dAngConstraintAtom);
            advanceRuntime(out, 2 * 8);
            continue;
        }

        case hkpConstraintAtom::TYPE_LIN:
        {
            // the pivots kept together along an axis of B's base
            hkp1dLinearBilateralConstraintInfo info;
            info.m_pivotA = baseA.m_translation;
            info.m_pivotB = baseB.m_translation;
            info.m_constrainedDofW = (&baseB.m_rotation.m_col0)[static_cast<const hkpLinConstraintAtom*>(current)->m_axisIndex];
            hk1dLinearBilateralConstraintBuildJacobian(info, in, out);
            atom += sizeof(hkpLinConstraintAtom);
            advanceRuntime(out, 8);
            continue;
        }

        case hkpConstraintAtom::TYPE_LIN_LIMIT:
        {
            // the pivots' separation along an axis of B's base within the range
            const hkpLinLimitConstraintAtom* limit = static_cast<const hkpLinLimitConstraintAtom*>(current);
            hkp1dLinearLimitInfo info;
            info.m_pivotA = baseA.m_translation;
            info.m_pivotB = baseB.m_translation;
            info.m_min = limit->m_min;
            info.m_max = limit->m_max;
            info.m_constrainedDofW = (&baseB.m_rotation.m_col0)[limit->m_axisIndex];
            hk1dLinearLimitBuildJacobian(info, in, out);
            atom += sizeof(hkpLinLimitConstraintAtom);
            advanceRuntime(out, 8);
            continue;
        }

        case hkpConstraintAtom::TYPE_LIN_FRICTION:
        {
            // along B's friction axis through A's pivot, up to the friction force (none: its results are skipped)
            const hkpLinFrictionConstraintAtom* friction = static_cast<const hkpLinFrictionConstraintAtom*>(current);
            if (friction->m_isEnabled && friction->m_maxFrictionForce > 0.0f)
            {
                hkp1dLinearFrictionInfo info;
                info.m_pivot = baseA.m_translation;
                info.m_constrainedDofW = (&baseB.m_rotation.m_col0)[friction->m_frictionAxis];
                info.m_maxFrictionForce = friction->m_maxFrictionForce;
                info.m_lastSolverResults = reinterpret_cast<hkpSolverResults*>(out.m_constraintRuntime.val());
                hk1dLinearFrictionBuildJacobian(info, in, out);
            }
            else
            {
                shiftSolverResults(out, 2);
            }
            atom += sizeof(hkpLinFrictionConstraintAtom);
            advanceRuntime(out, 2 * 8);
            continue;
        }

        case hkpConstraintAtom::TYPE_LIN_MOTOR:
        {
            const hkpLinMotorConstraintAtom* motor = static_cast<const hkpLinMotorConstraintAtom*>(current);
            if (motor->m_isEnabled.m_bool && motor->m_motor != nullptr)
            {
                hkErrorFwd::messageError(0x4F704557, "OpenRFG: hkSolverBuildJacobianFromAtomsNotContact: linear motor not ported", "hkpbuildjacobianfromatoms.cpp", __LINE__);
                return;
            }
            shiftSolverResults(out, 2);
            atom += sizeof(hkpLinMotorConstraintAtom);
            advanceRuntime(out, 2 * 8);
            continue;
        }

        case hkpConstraintAtom::TYPE_ANG:
        {
            // no rotation around the listed axes. One axis: B's axis (the third listed) stays perpendicular to A's
            // (the second), the row around their cross product; two: as a 2d angular atom free around the third
            // listed axis; otherwise per listed axis A's next axis stays perpendicular to B's one after that, the row
            // around A's axis.
            const hkpAngConstraintAtom* ang = static_cast<const hkpAngConstraintAtom*>(current);
            const hkVector4f* colsA = &baseA.m_rotation.m_col0;
            const hkVector4f* colsB = &baseB.m_rotation.m_col0;
            const int numAxes = ang->m_numConstrainedAxes;
            hkp1dAngularBilateralConstraintInfo info;
            if (numAxes == 1)
            {
                info.m_zeroErrorAxisAinW = colsA[ang->m_constrainedAxes[1]];
                info.m_perpZeroErrorAxisBinW = colsB[ang->m_constrainedAxes[2]];
                info.m_constrainedDofW.m_quad = cross(info.m_zeroErrorAxisAinW.m_quad, info.m_perpZeroErrorAxisBinW.m_quad);
                hk1dAngularBilateralConstraintBuildJacobian(info, in, out);
            }
            else if (numAxes == 2)
            {
                const int freeAxis = ang->m_constrainedAxes[2];
                const hkVector4f& b1 = colsB[(freeAxis + 1) % 3];
                const hkVector4f& b2 = colsB[(freeAxis + 2) % 3];
                info.m_zeroErrorAxisAinW = colsA[freeAxis];
                info.m_perpZeroErrorAxisBinW = b1;
                info.m_constrainedDofW = b2;
                hk1dAngularBilateralConstraintBuildJacobian(info, in, out);
                info.m_perpZeroErrorAxisBinW = b2;
                info.m_constrainedDofW.m_quad = _mm_xor_ps(signMask(), b1.m_quad);
                hk1dAngularBilateralConstraintBuildJacobian(info, in, out);
            }
            else
            {
                for (int i = 0; i < numAxes; i++)
                {
                    const int axis = ang->m_constrainedAxes[i];
                    info.m_constrainedDofW = colsA[axis];
                    info.m_zeroErrorAxisAinW = colsA[(axis + 1) % 3];
                    info.m_perpZeroErrorAxisBinW = colsB[(axis + 2) % 3];
                    hk1dAngularBilateralConstraintBuildJacobian(info, in, out);
                }
            }
            atom += sizeof(hkpAngConstraintAtom);
            advanceRuntime(out, numAxes * 8);
            continue;
        }

        case hkpConstraintAtom::TYPE_ANG_LIMIT:
        {
            // the angle around B's limit axis between the cosine axes (B's turned about the limit axis against A's),
            // unwrapped next to the previous step's angle
            const hkpAngLimitConstraintAtom* limit = static_cast<const hkpAngLimitConstraintAtom*>(current);
            if (!limit->m_isEnabled)
            {
                advanceRuntime(out, 8);
                atom += sizeof(hkpAngLimitConstraintAtom);
                shiftSolverResults(out, 1);
                continue;
            }
            AngularLimit info;
            info.tau = in.m_tau.val() * limit->m_angularLimitsTauFactor;
            info.damping = in.m_damping.val() * limit->m_angularLimitsDampFactor;
            const Q axis = (&baseB.m_rotation.m_col0)[limit->m_limitAxis].m_quad;
            info.dof = axis;
            info.min = limit->m_minAngle;
            info.max = limit->m_maxAngle;
            const Q cosB = (&baseB.m_rotation.m_col0)[limit->m_cosineAxis].m_quad;
            const Q cosA = (&baseA.m_rotation.m_col0)[limit->m_cosineAxis].m_quad;
            const Q sinAngle = hadd3(_mm_mul_ps(cross(axis, cosB), cosA));
            const Q cosAngle = hadd3(_mm_mul_ps(cosB, cosA));
            const Q angle = quadAtan2(sinAngle, cosAngle);
            info.computedAngle = unwrapAngle(angle, previousAngle(out));
            advanceRuntime(out, 8);
            atom += sizeof(hkpAngLimitConstraintAtom);
            buildAngularLimit(info, in, out);
            continue;
        }

        case hkpConstraintAtom::TYPE_TWIST_LIMIT:
        {
            // the twist axis is the normalized sum of A's and B's twist axes (B's when they cancel); the angle is
            // between B's and A's reference axes around it, and tau is scaled by the squared half length of the sum
            // (the binary does not test m_isEnabled here)
            const hkpTwistLimitConstraintAtom* limit = static_cast<const hkpTwistLimitConstraintAtom*>(current);
            AngularLimit info;
            info.min = limit->m_minAngle;
            info.max = limit->m_maxAngle;
            const Q twistB = (&baseB.m_rotation.m_col0)[limit->m_twistAxis].m_quad;
            const Q sum = _mm_add_ps((&baseA.m_rotation.m_col0)[limit->m_twistAxis].m_quad, twistB);
            const Q length = sqrt23(hadd3(_mm_mul_ps(sum, sum)));
            Q twist;
            float halfLength;
            if (lane0(length) > 1.0e-16f) // 0x24E69595
            {
                halfLength = lane0(_mm_mul_ps(g_vectorfConstants[HK_QUADREAL_INV_2], length));
                twist = _mm_mul_ps(rcp23(length), sum);
            }
            else
            {
                halfLength = 0.0f;
                twist = twistB;
            }
            const Q refB = (&baseB.m_rotation.m_col0)[limit->m_refAxis].m_quad;
            const Q refA = (&baseA.m_rotation.m_col0)[limit->m_refAxis].m_quad;
            const Q perp = cross(twist, refB);
            const Q perp2 = cross(perp, twist);
            const Q sinAngle = hadd3(_mm_mul_ps(perp, refA));
            const Q cosAngle = hadd3(_mm_mul_ps(perp2, refA));
            info.dof = twist;
            info.computedAngle = lane0(quadAtan2(sinAngle, cosAngle));
            info.tau = ((in.m_tau.val() * limit->m_angularLimitsTauFactor) * halfLength) * halfLength;
            info.damping = in.m_damping.val() * limit->m_angularLimitsDampFactor;
            atom += sizeof(hkpTwistLimitConstraintAtom);
            advanceRuntime(out, 8);
            buildAngularLimit(info, in, out);
            continue;
        }

        case hkpConstraintAtom::TYPE_CONE_LIMIT:
        {
            // the angle between A's twist axis and B's reference axis around their cross product (pi/2 minus it when
            // measured from perpendicular, the axis then reversed); with an angle offset (memOffsetToAngleOffset > 1,
            // from the instance runtime) the angle is eased towards the previous one and the offset decays (the
            // binary does not test m_isEnabled here)
            const hkpConeLimitConstraintAtom* limit = static_cast<const hkpConeLimitConstraintAtom*>(current);
            const Q twistA = (&baseA.m_rotation.m_col0)[limit->m_twistAxisInA].m_quad;
            const Q refB = (&baseB.m_rotation.m_col0)[limit->m_refAxisInB].m_quad;
            const Q c = cross(twistA, refB);
            const Q lengthSqr = hadd3(_mm_mul_ps(c, c));
            if (!(lane0(lengthSqr) >= lane0(g_vectorfConstants[HK_QUADREAL_EPS])))
            {
                advanceRuntime(out, 8);
                atom += sizeof(hkpConeLimitConstraintAtom);
                shiftSolverResults(out, 1);
                continue;
            }
            const Q r = _mm_rsqrt_ps(lengthSqr);
            const Q invLength = _mm_mul_ps(_mm_sub_ps(g_vectorfConstants[HK_QUADREAL_3], _mm_mul_ps(_mm_mul_ps(r, lengthSqr), r)), _mm_mul_ps(r, _mm_set1_ps(0.5f)));
            const Q sinAngle = _mm_mul_ps(invLength, lengthSqr);
            const Q cosAngle = hadd3(_mm_mul_ps(twistA, refB));
            const Q angle = quadAtan2(sinAngle, cosAngle);
            const Q perpendicular = _mm_castsi128_ps(_mm_set1_epi32(limit->m_angleMeasurementMode.m_storage != 0 ? -1 : 0));
            float computedAngle = lane0(_mm_or_ps(_mm_and_ps(_mm_sub_ps(g_vectorfConstants[HK_QUADREAL_PI_HALF], angle), perpendicular), _mm_andnot_ps(perpendicular, angle)));
            AngularLimit info;
            info.tau = in.m_tau.val() * limit->m_angularLimitsTauFactor;
            info.damping = in.m_damping.val() * limit->m_angularLimitsDampFactor;
            // the axis sign: +1 when measured from perpendicular, -1 otherwise
            const Q sign = _mm_xor_ps(_mm_and_ps(perpendicular, signMask()), g_vectorfConstants[HK_QUADREAL_MINUS1]);
            info.dof = _mm_mul_ps(_mm_mul_ps(invLength, c), sign);
            info.min = limit->m_minAngle;
            info.max = limit->m_maxAngle;
            if (limit->m_memOffsetToAngleOffset > 1)
            {
                const float halfDt = in.m_frameDeltaTime.val() * 0.5f;
                float* angleOffset = reinterpret_cast<float*>(reinterpret_cast<char*>(out.m_instanceRuntime.val()) + limit->m_memOffsetToAngleOffset);
                const float offset = *angleOffset;
                const float negPrevious = lane0(_mm_xor_ps(_mm_set_ss(previousAngle(out)), signMask()));
                const float fromOffset = computedAngle - offset;
                const float decay = lane0(_mm_max_ss(_mm_set_ss(offset * 0.05f), _mm_set_ss(halfDt))); // 0x3D4CCCCD
                const float jump = negPrevious - fromOffset;
                float newOffset = offset - decay;
                computedAngle = decay + fromOffset;
                const float threshold = (decay * 2.0f + halfDt) * 0.5f;
                if (-jump > threshold && negPrevious != 0.0f)
                {
                    newOffset = newOffset - jump;
                    computedAngle = jump + computedAngle;
                }
                *angleOffset = lane0(_mm_max_ss(_mm_set_ss(newOffset), _mm_set_ss(1.1920929e-07f))); // FLT_EPSILON
            }
            info.computedAngle = computedAngle;
            advanceRuntime(out, 8);
            atom += sizeof(hkpConeLimitConstraintAtom);
            buildAngularLimit(info, in, out);
            continue;
        }

        case hkpConstraintAtom::TYPE_ANG_FRICTION:
        {
            // one row per axis of A's base, its impulse up to the torque times the micro step (2 results each)
            const hkpAngFrictionConstraintAtom* friction = static_cast<const hkpAngFrictionConstraintAtom*>(current);
            const int numAxes = friction->m_numFrictionAxes;
            if (!friction->m_isEnabled || friction->m_maxFrictionTorque == 0.0f)
            {
                shiftSolverResults(out, numAxes * 2);
            }
            else
            {
                const hkpVelocityAccumulator* mA = in.m_bodyA.val();
                const hkpVelocityAccumulator* mB = in.m_bodyB.val();
                const hkpSolverResults* results = reinterpret_cast<const hkpSolverResults*>(out.m_constraintRuntime.val());
                hkpJacobian1dAngularFrictionSchema* schema = reinterpret_cast<hkpJacobian1dAngularFrictionSchema*>(out.m_jacobianSchemas.val());
                for (int i = 0; i < numAxes; i++, schema++)
                {
                    const Q axis = (&baseA.m_rotation.m_col0)[friction->m_firstFrictionAxis + i].m_quad;
                    const Q ang0 = rotate(mA->m_scratch0.m_quad, mA->m_scratch1.m_quad, mA->m_scratch2.m_quad, axis);
                    schema->m_jac.m_angular[0].m_quad = ang0;
                    const Q ang1 = rotate(mB->m_scratch0.m_quad, mB->m_scratch1.m_quad, mB->m_scratch2.m_quad, _mm_xor_ps(signMask(), axis));
                    schema->m_jac.m_angular[1].m_quad = ang1;
                    const Q diag = _mm_add_ps(hadd3(_mm_add_ps(_mm_mul_ps(_mm_mul_ps(ang0, ang0), mA->m_invMasses.m_quad), _mm_mul_ps(_mm_mul_ps(ang1, ang1), mB->m_invMasses.m_quad))), g_vectorfConstants[HK_QUADREAL_EPS]);
                    schema->m_jac.m_angular[0].m_quad = setW(ang0, _mm_mul_ps(rcp23(diag), splat(1.0f)));
                    // the previous step's right hand side (the first of the axis' two results)
                    schema->m_jac.m_angular[1].m_quad = setW(ang1, _mm_mul_ps(splat(in.m_rhsFactor.val()), splat(results[i * 2].m_internalSolverData)));
                    schema->m_maxImpulse = friction->m_maxFrictionTorque * in.m_microStepDeltaTime.val();
                    setSchemaType(schema, hkpJacobianSchema::SCHEMA_TYPE_1D_ANGULAR_FRICTION);
                    schema->m_frictionIndex = (unsigned int)i;
                }
                out.m_jacobianSchemas = reinterpret_cast<hkpJacobianSchema*>(schema);
            }
            atom += sizeof(hkpAngFrictionConstraintAtom);
            advanceRuntime(out, numAxes * 0x10);
            continue;
        }

        case hkpConstraintAtom::TYPE_ANG_MOTOR:
        {
            const hkpAngMotorConstraintAtom* motor = static_cast<const hkpAngMotorConstraintAtom*>(current);
            if (motor->m_motor == nullptr || !motor->m_isEnabled.m_bool)
            {
                shiftSolverResults(out, 2);
            }
            else
            {
                buildAngularMotor(*motor, baseA, baseB, in, out);
            }
            atom += sizeof(hkpAngMotorConstraintAtom);
            advanceRuntime(out, 2 * 8);
            continue;
        }

        case hkpConstraintAtom::TYPE_RAGDOLL_MOTOR:
            if (static_cast<const hkpRagdollMotorConstraintAtom*>(current)->m_isEnabled.m_bool)
            {
                hkErrorFwd::messageError(0x4F704555, "OpenRFG: hkSolverBuildJacobianFromAtomsNotContact: ragdoll motor not ported", "hkpbuildjacobianfromatoms.cpp", __LINE__);
                return;
            }
            shiftSolverResults(out, 6);
            atom += sizeof(hkpRagdollMotorConstraintAtom);
            advanceRuntime(out, 6 * 8);
            continue;

        case hkpConstraintAtom::TYPE_MODIFIER_MASS_CHANGER:
        {
            // the child sees accumulator copies with the scaled inverse masses (the query copied, 0x80 bytes)
            const hkpMassChangerModifierConstraintAtom* massChanger = static_cast<const hkpMassChangerModifierConstraintAtom*>(current);
            hkpConstraintQueryIn childIn = in;
            const hkpConstraintAtom* child = massChanger->m_child;
            const int childSize = massChanger->m_childSize;
            hkpVelocityAccumulator accA = *in.m_bodyA.val();
            hkpVelocityAccumulator accB = *in.m_bodyB.val();
            childIn.m_bodyA = &accA;
            childIn.m_bodyB = &accB;
            accB.m_invMasses.m_quad = _mm_mul_ps(accB.m_invMasses.m_quad, massChanger->m_factorB.m_quad);
            accA.m_invMasses.m_quad = _mm_mul_ps(accA.m_invMasses.m_quad, massChanger->m_factorA.m_quad);
            hkSetInvMassBuildJacobian(accA.m_invMasses, accB.m_invMasses, out);
            if (child->m_type.m_storage == hkpConstraintAtom::TYPE_CONTACT)
            {
                hkSimpleContactConstraintDataBuildJacobian(const_cast<hkpSimpleContactConstraintAtom*>(static_cast<const hkpSimpleContactConstraintAtom*>(child)), childIn, true, out);
            }
            else
            {
                hkSolverBuildJacobianFromAtomsNotContact(child, childSize, childIn, out);
            }
            // the real inverse masses back for the schemas after this constraint
            hkSetInvMassBuildJacobian(in.m_bodyA.val()->m_invMasses, in.m_bodyB.val()->m_invMasses, out);
            atom += sizeof(hkpMassChangerModifierConstraintAtom);
            continue;
        }

        case hkpConstraintAtom::TYPE_MODIFIER_IGNORE_CONSTRAINT:
            // the header goes again: no schemas at all
            out.m_jacobianSchemas = reinterpret_cast<hkpJacobianSchema*>(reinterpret_cast<char*>(out.m_jacobianSchemas.val()) - sizeof(hkpJacobianHeaderSchema));
            atom += sizeof(hkpIgnoreModifierConstraintAtom);
            continue;

        default:
            hkErrorFwd::messageError(0x4F70454E, "OpenRFG: hkSolverBuildJacobianFromAtomsNotContact: atom type not ported", "hkpbuildjacobianfromatoms.cpp", __LINE__);
            return;
        }
    }
}
