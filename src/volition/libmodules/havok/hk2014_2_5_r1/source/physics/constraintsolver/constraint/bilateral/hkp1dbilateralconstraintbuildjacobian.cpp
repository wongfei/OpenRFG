// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/bilateral/hkp1dbilateralconstraintbuildjacobian.cpp
//
// Havok 2014.2.5 (havok_impl.md HK8): the schemas of the response modifiers, from rfg.exe; the angular bilateral
// Jacobian came with the ragdoll's hinges (multiplayer.md MP7), the linear bilateral and limit Jacobians with the
// prismatic constraint of stuck projectiles, the linear friction Jacobian with the prismatic friction. The user tau
// and motor Jacobians of this file belong to constraint types no in-scope code creates and are not ported (the ball
// socket inlines its own rows).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/bilateral/hkp1dbilateralconstraintinfo.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/accumulator/hkpvelocityaccumulator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/hkpconstraintqueryin.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/hkpconstraintqueryout.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/jacobian/hkpjacobianschema.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverresults.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/hkpsolversse.h"

using namespace hkSolverSse;

// 0x00D730B0: the row around the constrained axis (A's and B's angular parts in core space: A's rotation of the axis,
// B's of its negation), the effective mass rcp(diag + eps) * virtMassFactor in m_angular[0].w and the right hand side
// -(perpB . zeroErrorA) * rhsFactor in m_angular[1].w
void hk1dAngularBilateralConstraintBuildJacobian(const hkp1dAngularBilateralConstraintInfo& info, const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out)
{
    hkpJacobian1dAngularSchema* schema = reinterpret_cast<hkpJacobian1dAngularSchema*>(out.m_jacobianSchemas.val());
    const hkpVelocityAccumulator* mA = in.m_bodyA.val();
    const hkpVelocityAccumulator* mB = in.m_bodyB.val();
    const Q dof = info.m_constrainedDofW.m_quad;
    const Q negDof = _mm_xor_ps(_mm_castsi128_ps(_mm_set1_epi32(0x80000000)), dof);
    const Q ang0 = rotate(mA->m_scratch0.m_quad, mA->m_scratch1.m_quad, mA->m_scratch2.m_quad, dof);
    const Q ang1 = rotate(mB->m_scratch0.m_quad, mB->m_scratch1.m_quad, mB->m_scratch2.m_quad, negDof);
    schema->m_jac.m_angular[1].m_quad = ang1;
    const Q diag = _mm_add_ps(hadd3(_mm_add_ps(_mm_mul_ps(_mm_mul_ps(ang1, ang1), mB->m_invMasses.m_quad), _mm_mul_ps(_mm_mul_ps(ang0, ang0), mA->m_invMasses.m_quad))), g_vectorfConstants[HK_QUADREAL_EPS]);
    const Q effMass = _mm_mul_ps(rcp23(diag), splat(in.m_virtMassFactor.val()));
    schema->m_jac.m_angular[0].m_quad = setW(ang0, effMass);
    const Q error = hadd3(_mm_mul_ps(info.m_perpZeroErrorAxisBinW.m_quad, info.m_zeroErrorAxisAinW.m_quad));
    const Q rhs = _mm_mul_ps(_mm_sub_ps(_mm_setzero_ps(), error), splat(in.m_rhsFactor.val()));
    schema->m_jac.m_angular[1].m_quad = setW(schema->m_jac.m_angular[1].m_quad, rhs);
    setSchemaType(schema, hkpJacobianSchema::SCHEMA_TYPE_1D_ANGULAR);
    out.m_jacobianSchemas = reinterpret_cast<hkpJacobianSchema*>(schema + 1);
}

namespace
{
// the 1 linear 2 angular row of a linear constraint along dof (core space angular parts: A's of armA x dof, B's of
// dof x armB): ang1.w the diagonal (both bodies' angular terms plus the inverse masses' sum, at least EPS_SQRD), ang0.w
// the effective mass rcp(diag) * virtMassFactor; lin = dof with lin.w the right hand side
__forceinline void buildLinearRow(const Q& armA, const Q& armB, const Q& dof, const Q& rhs, const hkpConstraintQueryIn& in, float* jac)
{
    const hkpVelocityAccumulator* mA = in.m_bodyA.val();
    const hkpVelocityAccumulator* mB = in.m_bodyB.val();
    const Q ang0 = rotate(mA->m_scratch0.m_quad, mA->m_scratch1.m_quad, mA->m_scratch2.m_quad, cross(armA, dof));
    const Q ang1 = rotate(mB->m_scratch0.m_quad, mB->m_scratch1.m_quad, mB->m_scratch2.m_quad, cross(dof, armB));
    const Q invMassSum = _mm_max_ps(_mm_add_ps(mB->m_invMasses.m_quad, mA->m_invMasses.m_quad), g_vectorfConstants[HK_QUADREAL_EPS_SQRD]);
    const Q angular = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(ang1, ang1), mB->m_invMasses.m_quad), _mm_mul_ps(_mm_mul_ps(ang0, ang0), mA->m_invMasses.m_quad));
    const Q diag = hadd4(setW(angular, invMassSum));
    _mm_store_ps(jac + 8, setW(ang1, diag));
    _mm_store_ps(jac + 4, setW(ang0, _mm_mul_ps(rcp23(diag), splat(in.m_virtMassFactor.val()))));
    _mm_store_ps(jac, setW(dof, rhs));
}

// the separation (pivotB - pivotA) . dof times rhsFactor
__forceinline Q separationRhs(const Q& pivotA, const Q& pivotB, const Q& dof, const hkpConstraintQueryIn& in)
{
    return _mm_mul_ps(hadd3(_mm_mul_ps(_mm_sub_ps(pivotB, pivotA), dof)), splat(in.m_rhsFactor.val()));
}
}

// 0x00D73200 (inlined in hkSolverBuildJacobianFromAtomsNotContact): the friction row along the dof through the pivot
// (both arms from it); the right hand side is the last step's (the first result's internal data) times rhsFactor, the
// impulse limit the friction force by the micro step
void hk1dLinearFrictionBuildJacobian(const hkp1dLinearFrictionInfo& info, const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out)
{
    hkpJacobian1dFrictionSchema* schema = reinterpret_cast<hkpJacobian1dFrictionSchema*>(out.m_jacobianSchemas.val());
    const Q pivot = info.m_pivot.m_quad;
    const Q armA = _mm_sub_ps(pivot, in.m_bodyA.val()->m_scratch3.m_quad);
    const Q armB = _mm_sub_ps(pivot, in.m_bodyB.val()->m_scratch3.m_quad);
    const Q rhs = _mm_mul_ps(splat(info.m_lastSolverResults->m_internalSolverData), splat(in.m_rhsFactor.val()));
    buildLinearRow(armA, armB, info.m_constrainedDofW.m_quad, rhs, in, reinterpret_cast<float*>(&schema->m_jac));
    out.m_jacobianSchemas = reinterpret_cast<hkpJacobianSchema*>(schema + 1);
    schema->m_maxFrictionImpulse = in.m_microStepDeltaTime.val() * info.m_maxFrictionForce;
    setSchemaType(schema, hkpJacobianSchema::SCHEMA_TYPE_1D_FRICTION);
}

// 0x00D73380: one bilateral row along the dof. Both arms start at pivot A (the binary also computes a zero error from
// pivot A twice and overwrites it).
void hk1dLinearBilateralConstraintBuildJacobian(const hkp1dLinearBilateralConstraintInfo& info, const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out)
{
    hkpJacobian1dBilateralSchema* schema = reinterpret_cast<hkpJacobian1dBilateralSchema*>(out.m_jacobianSchemas.val());
    const Q pivotA = info.m_pivotA.m_quad;
    const Q armA = _mm_sub_ps(pivotA, in.m_bodyA.val()->m_scratch3.m_quad);
    const Q armB = _mm_sub_ps(pivotA, in.m_bodyB.val()->m_scratch3.m_quad);
    buildLinearRow(armA, armB, info.m_constrainedDofW.m_quad, separationRhs(pivotA, info.m_pivotB.m_quad, info.m_constrainedDofW.m_quad, in), in, reinterpret_cast<float*>(&schema->m_jac));
    setSchemaType(schema, hkpJacobianSchema::SCHEMA_TYPE_1D_BILATERAL);
    out.m_jacobianSchemas = reinterpret_cast<hkpJacobianSchema*>(schema + 1);
}

// 0x00D73B30: the row along the dof between the limits (each times -rhsFactor); the arms from each body's pivot
void hk1dLinearLimitBuildJacobian(const hkp1dLinearLimitInfo& info, const hkpConstraintQueryIn& in, hkpConstraintQueryOut& out)
{
    hkpJacobian1dLinearLimitsSchema* schema = reinterpret_cast<hkpJacobian1dLinearLimitsSchema*>(out.m_jacobianSchemas.val());
    const Q pivotA = info.m_pivotA.m_quad;
    const Q pivotB = info.m_pivotB.m_quad;
    const Q armA = _mm_sub_ps(pivotA, in.m_bodyA.val()->m_scratch3.m_quad);
    const Q armB = _mm_sub_ps(pivotB, in.m_bodyB.val()->m_scratch3.m_quad);
    buildLinearRow(armA, armB, info.m_constrainedDofW.m_quad, separationRhs(pivotA, pivotB, info.m_constrainedDofW.m_quad, in), in, reinterpret_cast<float*>(&schema->m_jac));
    const float negRhsFactor = lane0(_mm_xor_ps(_mm_set_ss(in.m_rhsFactor.val()), _mm_castsi128_ps(_mm_set1_epi32(0x80000000))));
    out.m_jacobianSchemas = reinterpret_cast<hkpJacobianSchema*>(schema + 1);
    setSchemaType(schema, hkpJacobianSchema::SCHEMA_TYPE_1D_LINEAR_LIMITS);
    schema->m_minRhs = info.m_min * negRhsFactor;
    schema->m_maxRhs = info.m_max * negRhsFactor;
}

// 0x00D72FF0: the type byte overwrites invMassA.x's low byte (no epsilon added)
void hkSetInvMassBuildJacobian(const hkVector4f& invMassA, const hkVector4f& invMassB, hkpConstraintQueryOut& out)
{
    hkpJacobianSetMassSchema* schema = reinterpret_cast<hkpJacobianSetMassSchema*>(out.m_jacobianSchemas.val());
    schema->initSetMass(invMassA, invMassB);
    out.m_jacobianSchemas = reinterpret_cast<hkpJacobianSchema*>(schema + 1);
}

// 0x00D73020
void hkAddVelocityBuildJacobian(const hkVector4f& deltaVel, int bodyIndex, hkpConstraintQueryOut& out)
{
    hkpJacobianAddVelocitySchema* schema = reinterpret_cast<hkpJacobianAddVelocitySchema*>(out.m_jacobianSchemas.val());
    schema->initAddVelocity(deltaVel, bodyIndex);
    out.m_jacobianSchemas = reinterpret_cast<hkpJacobianSchema*>(schema + 1);
}

// 0x00D73050
void hkSetCenterOfMassBuildJacobian(const hkMatrix3f& angToLinVelA, const hkMatrix3f& angToLinVelB, hkpConstraintQueryOut& out)
{
    hkpJacobianSetCenterOfMassSchema* schema = reinterpret_cast<hkpJacobianSetCenterOfMassSchema*>(out.m_jacobianSchemas.val());
    schema->initAddAngularVelocity(angToLinVelA, angToLinVelB);
    out.m_jacobianSchemas = reinterpret_cast<hkpJacobianSchema*>(schema + 1);
}
