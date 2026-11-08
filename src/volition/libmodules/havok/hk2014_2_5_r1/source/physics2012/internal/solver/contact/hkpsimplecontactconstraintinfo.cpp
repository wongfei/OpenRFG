// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/contact/hkpsimplecontactconstraintinfo.cpp
//
// Havok 2014.2.5 (havok_impl.md HK8): the Jacobians of a contact constraint, from rfg.exe (written from the
// disassembly in its SSE operation order). Per point a non-penetration row (single, single with an impulse limit, or a
// pair with the previous point when the point uses the second solver path); then, when the points carry an impulse,
// friction around their average: 2d friction along two tangents, rolling friction when the multiplier is set, and 3d
// friction (angular friction around the normal, scaled by the contact radius) from two points on.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/contact/hkpsimplecontactconstraintinfo.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkufloat8.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/accumulator/hkpvelocityaccumulator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/hkpconstraintqueryin.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/hkpconstraintqueryout.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/jacobian/hkpjacobianschema.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpsimplecontactconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpcontactpointproperties.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/hkpsolversse.h"

using namespace hkSolverSse;

// 0x016F89A8, 0x016F89AC: how fast a penetration is resolved (a linear speed per frame and an exponential part)
float HK_CONTACT_LINEAR_ERROR_RECOVERY_VELOCITY = 1.0f;
float HK_CONTACT_EXPONENTIAL_ERROR_RECOVERY_VELOCITY = 0.05f;

namespace
{
// hkUFloat8 to float (hkUFloat8::decodeFloat of the encoded value, inlined)
__forceinline float decodeUFloat8(unsigned char value)
{
    return hkUFloat8::decodeFloat(hkUFloat8::getEncodedFloat(value));
}

// the effective mass of a diagonal: (1 / diag) * virtual mass factor
__forceinline Q effMass(const Q& diag, const Q& virtMassFactor)
{
    return _mm_mul_ps(rcp23(diag), virtMassFactor);
}

// the diagonal of a 1 linear 2 angular row with a unit linear part: the angular parts with the inverse inertias,
// plus the inverse masses (at least HK_REAL_EPSILON squared) in w, summed over all four lanes
__forceinline Q diag1Lin2Ang(const Q& ang0, const Q& ang1, const Q& invMassA, const Q& invMassB, const Q& massSum)
{
    const Q a = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(ang1, ang1), invMassB), _mm_mul_ps(_mm_mul_ps(ang0, ang0), invMassA));
    return hadd4(setW(a, massSum));
}

// the coupling of two rows sharing the bodies: ((a0 * b0) * mA + (a1 * b1) * mB) + (mA.w * ll + mB.w * ll), summed
__forceinline Q nonDiag(const Q& linA, const Q& ang0A, const Q& ang1A, const Q& linB, const Q& ang0B, const Q& ang1B, const Q& invMassA, const Q& invMassB)
{
    const Q ll = _mm_mul_ps(linA, linB);
    const Q ang = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(ang0A, ang0B), invMassA), _mm_mul_ps(_mm_mul_ps(ang1A, ang1B), invMassB));
    const Q lin = _mm_add_ps(_mm_mul_ps(wwww(invMassA), ll), _mm_mul_ps(wwww(invMassB), ll));
    return hadd3(_mm_add_ps(ang, lin));
}

// the diagonal of a 2 angular row: ((y + x) + z) + HK_REAL_EPSILON of ang0^2 * mA + ang1^2 * mB
__forceinline Q diag2Ang(const Q& ang0, const Q& ang1, const Q& invMassA, const Q& invMassB)
{
    const Q a = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(ang0, ang0), invMassA), _mm_mul_ps(_mm_mul_ps(ang1, ang1), invMassB));
    return _mm_add_ps(hadd3(a), g_vectorfConstants[HK_QUADREAL_EPS]);
}

__forceinline void storeQ(void* p, const Q& v) { _mm_store_ps(static_cast<float*>(p), v); }
__forceinline Q loadQ(const void* p) { return _mm_load_ps(static_cast<const float*>(p)); }

// a 2 angular row of the rolling friction: ang0 = rotate(A, dir), ang1 = rotate(B, -dir), the effective mass in
// ang0.w and the angular right hand side in ang1.w
__forceinline void buildRollingRow(float* jac, const Q& dir, const hkpVelocityAccumulator* mA, const hkpVelocityAccumulator* mB, const Q& virtMassFactor, float rhs)
{
    const Q signMask = _mm_castsi128_ps(_mm_set1_epi32(0x80000000));
    Q ang0 = rotate(mA->m_scratch0.m_quad, mA->m_scratch1.m_quad, mA->m_scratch2.m_quad, dir);
    Q ang1 = rotate(mB->m_scratch0.m_quad, mB->m_scratch1.m_quad, mB->m_scratch2.m_quad, _mm_xor_ps(dir, signMask));
    const Q a = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(ang1, ang1), mB->m_invMasses.m_quad), _mm_mul_ps(_mm_mul_ps(ang0, ang0), mA->m_invMasses.m_quad));
    const Q diag = _mm_add_ps(hadd3(a), g_vectorfConstants[HK_QUADREAL_EPS]);
    ang0 = setW(ang0, effMass(diag, virtMassFactor));
    storeQ(jac, ang0);
    storeQ(jac + 4, ang1);
    jac[7] = rhs;
}

// a 1 linear 2 angular friction row along dir through the arms rA, rB: the linear part dir (w the right hand side),
// ang0 = rotate(A, rA x dir), ang1 = rotate(B, dir x rB) with the diagonal in w, the effective mass in ang0.w
__forceinline Q buildFrictionRow(float* jac, const Q& dir, const Q& rA, const Q& rB, const hkpVelocityAccumulator* mA, const hkpVelocityAccumulator* mB, const Q& massSum, const Q& virtMassFactor, float rhs)
{
    storeQ(jac, dir);
    jac[3] = rhs;
    Q ang0 = rotate(mA->m_scratch0.m_quad, mA->m_scratch1.m_quad, mA->m_scratch2.m_quad, cross(rA, dir));
    Q ang1 = rotate(mB->m_scratch0.m_quad, mB->m_scratch1.m_quad, mB->m_scratch2.m_quad, cross(dir, rB));
    const Q diag = diag1Lin2Ang(ang0, ang1, mA->m_invMasses.m_quad, mB->m_invMasses.m_quad, massSum);
    ang1 = setW(ang1, diag);
    ang0 = setW(ang0, effMass(diag, virtMassFactor));
    storeQ(jac + 4, ang0);
    storeQ(jac + 8, ang1);
    return diag;
}
}

// 0x00E9A610: the friction state restarts
void hkSimpleContactConstraintDataAddALittleSurfaceViscosity(hkpSimpleContactConstraintDataInfo* infoExt)
{
    infoExt->m_data[2] = 0.0f;
    infoExt->m_data[3] = 0.0f;
    infoExt->m_data[1] = 0.0f;
    infoExt->m_rhsRolling[0].setZero();
    infoExt->m_rhsRolling[1].setZero();
}

// 0x00E9A6F0
void hkSimpleContactConstraintDataBuildJacobian(hkpSimpleContactConstraintAtom* atom, const hkpConstraintQueryIn& in, hkBool writeHeaderSchemas, hkpConstraintQueryOut& out)
{
    const Q zero = _mm_setzero_ps();
    Q posSum = zero;
    Q normalSum = zero;
    const int numPoints = atom->m_numContactPoints;
    const Q invNumPoints = rcp23(splat((float)numPoints));
    const int striding = atom->m_contactPointPropertiesStriding;
    char* props = reinterpret_cast<char*>(atom->getContactPointPropertiesStream());
    hkpSimpleContactConstraintDataInfo* info = &atom->m_info;
    char* schema = reinterpret_cast<char*>(out.m_jacobianSchemas.val());
    const hkpVelocityAccumulator* mA = in.m_bodyA;
    const hkpVelocityAccumulator* mB = in.m_bodyB;
    float impulseSum = 0.0f;
    float frictionSum = 0.0f;

    if (writeHeaderSchemas.m_bool)
    {
        hkpJacobianHeaderSchema* header = reinterpret_cast<hkpJacobianHeaderSchema*>(schema);
        header->m_bodyAIndex = (unsigned short)in.m_accumulatorAIndex.val();
        header->m_bodyBIndex = (unsigned short)in.m_accumulatorBIndex.val();
        header->m_constraintInstance = in.m_constraintInstance;
        setSchemaType(header, hkpJacobianSchema::SCHEMA_TYPE_HEADER);
        header->m_solverResultInMainMemory = reinterpret_cast<hkpSolverResults*>(props);
        header->m_solverResultStriding = (unsigned char)striding;
        schema += sizeof(hkpJacobianHeaderSchema);
    }

    // ---- the non-penetration rows
    const hkContactPoint* cp = atom->getContactPoints();
    char* pointProps = props;
    for (int i = 0; i < numPoints; i++, cp++, pointProps += striding)
    {
        hkpContactPointProperties* p = reinterpret_cast<hkpContactPointProperties*>(pointProps);
        const Q virtMassFactor = splat(in.m_virtMassFactor);
        const Q normal = cp->m_separatingNormal.m_quad;
        const Q pos = cp->m_position.m_quad;
        const Q rA = _mm_sub_ps(pos, mA->m_scratch3.m_quad);
        const Q rB = _mm_sub_ps(pos, mB->m_scratch3.m_quad);
        float* jac = reinterpret_cast<float*>(schema);
        storeQ(jac, normal);
        Q ang0 = rotate(mA->m_scratch0.m_quad, mA->m_scratch1.m_quad, mA->m_scratch2.m_quad, cross(rA, normal));
        Q ang1 = rotate(mB->m_scratch0.m_quad, mB->m_scratch1.m_quad, mB->m_scratch2.m_quad, cross(normal, rB));
        const Q massSum = _mm_max_ps(_mm_add_ps(mA->m_invMasses.m_quad, mB->m_invMasses.m_quad), g_vectorfConstants[HK_QUADREAL_EPS_SQRD]);
        const Q diag = diag1Lin2Ang(ang0, ang1, mA->m_invMasses.m_quad, mB->m_invMasses.m_quad, massSum);
        ang1 = setW(ang1, diag); // the user value holds the diagonal (the pair coupling below needs it)
        storeQ(jac + 8, ang1);
        ang0 = setW(ang0, effMass(diag, virtMassFactor));
        storeQ(jac + 4, ang0);

        frictionSum = decodeUFloat8(p->m_friction.m_value) + frictionSum;
        impulseSum = p->m_impulseApplied + impulseSum;

        // the right hand side: the distance to recover this step. internalDataA (the restitution's target distance
        // or the last recovery) limits the exponential recovery; 1.8446726e+19 (0x5F7FFFF0) for a disabled point.
        const unsigned char flags = p->m_flags;
        const float distance = (flags & hkContactPointMaterial::CONTACT_IS_DISABLED) != 0 ? 1.8446726e+19f : cp->m_separatingNormal.m_quad.m128_f32[3];
        const float a = p->m_internalDataA;
        const float linearRecovery = HK_CONTACT_LINEAR_ERROR_RECOVERY_VELOCITY * in.m_frameDeltaTime;
        const float expRecovery = (-HK_CONTACT_EXPONENTIAL_ERROR_RECOVERY_VELOCITY) * a;
        const float recovery = expRecovery < linearRecovery ? expRecovery : linearRecovery; // MINSS
        const float d = distance - a;
        const float overshoot = (-p->m_internalSolverData) - d;
        float rhsDistance = d - recovery;
        float newA = recovery + a;
        if (overshoot > recovery * 2.0f + linearRecovery)
        {
            newA = newA - overshoot;
            rhsDistance = rhsDistance + overshoot;
        }
        p->m_internalDataA = newA < -1.1920929e-07f ? newA : -1.1920929e-07f; // MINSS with -FLT_EPSILON (0xB4000000)
        posSum = _mm_add_ps(pos, posSum);
        normalSum = _mm_add_ps(normalSum, normal);
        jac[3] = (-rhsDistance) * in.m_rhsFactor;

        if ((flags & hkContactPointMaterial::CONTACT_USES_SOLVER_PATH2) == 0)
        {
            setSchemaType(jac, hkpJacobianSchema::SCHEMA_TYPE_SINGLE_CONTACT);
            schema += hkpJacobianSchemaInfo::SingleContact::Sizeof;
            const unsigned char maxImpulse = p->m_maxImpulse.m_value;
            if (maxImpulse != 0)
            {
                setSchemaType(jac, hkpJacobianSchema::SCHEMA_TYPE_SINGLE_LIMIT_CONTACT);
                jac[11] = decodeUFloat8(maxImpulse);
            }
        }
        else
        {
            // a pair with the previous row (the previous schema, written as a single contact)
            float* prev = jac - 12;
            const Q prevAng1 = loadQ(prev + 8);
            const Q nd = _mm_mul_ps(nonDiag(loadQ(jac), loadQ(jac + 4), loadQ(jac + 8), loadQ(prev), loadQ(prev + 4), prevAng1, mA->m_invMasses.m_quad, mB->m_invMasses.m_quad), splat(0.99f)); // 0x3F7D70A4
            const Q curDiag = wwww(loadQ(jac + 8));
            const Q prevDiag = wwww(prevAng1);
            const Q det = _mm_sub_ps(_mm_mul_ps(curDiag, prevDiag), _mm_mul_ps(nd, nd));
            const Q invDet = _mm_mul_ps(rcp23(det), splat(in.m_virtMassFactor));
            storeQ(prev + 8, setW(prevAng1, _mm_mul_ps(invDet, curDiag)));
            reinterpret_cast<hkpJacobianPairContactSchema*>(prev)->m_invJac01 = lane0(_mm_mul_ps(_mm_sub_ps(zero, nd), invDet));
            setSchemaType(prev, hkpJacobianSchema::SCHEMA_TYPE_PAIR_CONTACT);
            storeQ(jac + 8, setW(loadQ(jac + 8), _mm_mul_ps(invDet, prevDiag)));
            schema += hkpJacobianSchemaInfo::PairContact::Sizeof - hkpJacobianSchemaInfo::SingleContact::Sizeof;
        }
    }

    // ---- friction: the points' summed impulse times their average friction
    Q frictionImpulse = _mm_mul_ps(splat(frictionSum * impulseSum), invNumPoints);
    if (!(lane0(frictionImpulse) > 0.0f))
    {
        out.m_jacobianSchemas = reinterpret_cast<hkpJacobianSchema*>(schema);
        return;
    }

    // the friction normal: the average normal, else the first point's (a nearly uniform sum, or a sum too short with
    // a valid first normal), else up with no friction
    Q avgNormal;
    const Q normalLen2 = hadd3(_mm_mul_ps(normalSum, normalSum));
    if (lane0(_mm_mul_ps(_mm_mul_ps(invNumPoints, invNumPoints), normalLen2)) > 0.9999f) // 0x3F7FF972
    {
        avgNormal = atom->getContactPoints()[0].m_separatingNormal.m_quad;
    }
    else if (lane0(normalLen2) > 0.1f) // 0x3DCCCCCD
    {
        avgNormal = _mm_mul_ps(invSqrt23(normalLen2), normalSum);
    }
    else
    {
        avgNormal = atom->getContactPoints()[0].m_separatingNormal.m_quad;
        if (!(lane0(hadd3(_mm_mul_ps(avgNormal, avgNormal))) >= 0.9f)) // 0x3F666666
        {
            avgNormal = g_vectorfConstants[HK_QUADREAL_0100];
            frictionImpulse = zero;
        }
    }

    // the first tangent: the bi-normal axis x normal, or a new axis (the normal's smallest component) when they are
    // nearly parallel, which also restarts the friction state
    Q tangent = cross(g_vectorfConstants[HK_QUADREAL_1000 + info->m_biNormalAxis], avgNormal);
    if (!(lane0(hadd3(_mm_mul_ps(tangent, tangent))) > 0.1f))
    {
        const Q absNormal = _mm_castsi128_ps(_mm_srli_epi32(_mm_slli_epi32(_mm_castps_si128(avgNormal), 1), 1));
        const Q minYX = _mm_min_ps(yyyy(absNormal), xxxx(absNormal));
        const Q minComponent = _mm_min_ps(zzzz(absNormal), minYX);
        const Q isMin = _mm_and_ps(_mm_cmple_ps(absNormal, minComponent), _mm_load_ps(reinterpret_cast<const float*>(&hkVector4fComparison_maskToComparison[hkVector4ComparisonMask::MASK_XYZ * 4])));
        const int axis = hkVector4Comparison_maskToFirstIndex[_mm_movemask_ps(isMin)];
        *reinterpret_cast<unsigned int*>(&info->m_rhsRolling[0]) = 0;
        info->m_data[2] = 0.0f;
        info->m_data[3] = 0.0f;
        info->m_biNormalAxis = (unsigned short)axis;
        tangent = cross(g_vectorfConstants[HK_QUADREAL_1000 + axis], avgNormal);
    }
    const Q t1 = _mm_mul_ps(invSqrt23(hadd3(_mm_mul_ps(tangent, tangent))), tangent);
    const Q avgPos = _mm_mul_ps(invNumPoints, posSum);
    const Q t2 = cross(avgNormal, t1);
    const Q virtMassFactor = splat(in.m_virtMassFactor);
    const hkHalf rollingFrictionMultiplier = info->m_rollingFrictionMultiplier;

    // ---- rolling friction
    const Q rollingImpulse = _mm_mul_ps(splat(rollingFrictionMultiplier.getReal()), frictionImpulse);
    if (lane0(rollingImpulse) > 0.0f)
    {
        hkpJacobian2dRollingFrictionSchema* rolling = reinterpret_cast<hkpJacobian2dRollingFrictionSchema*>(schema);
        const float maxImpulse = lane0(rollingImpulse) * in.m_invNumStepsTimesMicroSteps; // x87, exact as a float product
        buildRollingRow(reinterpret_cast<float*>(&rolling->m_jac0), t2, mA, mB, virtMassFactor, in.m_frictionRhsFactor * info->m_rhsRolling[0].getReal());
        buildRollingRow(reinterpret_cast<float*>(&rolling->m_jac1), t1, mA, mB, virtMassFactor, in.m_frictionRhsFactor * info->m_rhsRolling[1].getReal());
        rolling->m_maxImpulse = maxImpulse;
        rolling->m_solverResults = reinterpret_cast<hkpSolverResults*>(info);
        rolling->m_originalRollingFrictionMultiplier = rollingFrictionMultiplier;
        setSchemaType(rolling, hkpJacobianSchema::SCHEMA_TYPE_2D_ROLLING_FRICTION);
        schema += hkpJacobianSchemaInfo::RollingFriction2D::Sizeof;
    }

    // ---- 2d friction along t2 and t1 through the average point
    char* frictionSchema = schema;
    float* jac0 = reinterpret_cast<float*>(schema);
    float* jac1 = jac0 + 12;
    const Q rA = _mm_sub_ps(avgPos, mA->m_scratch3.m_quad);
    const Q rB = _mm_sub_ps(avgPos, mB->m_scratch3.m_quad);
    const Q massSum = _mm_max_ps(_mm_add_ps(mA->m_invMasses.m_quad, mB->m_invMasses.m_quad), g_vectorfConstants[HK_QUADREAL_EPS_SQRD]);
    const Q diag0 = buildFrictionRow(jac0, t2, rA, rB, mA, mB, massSum, virtMassFactor, info->m_data[2] * in.m_frictionRhsFactor);
    const Q diag1 = buildFrictionRow(jac1, t1, rA, rB, mA, mB, massSum, virtMassFactor, info->m_data[3] * in.m_frictionRhsFactor);
    const Q nd = _mm_mul_ps(nonDiag(loadQ(jac1), loadQ(jac1 + 4), loadQ(jac1 + 8), loadQ(jac0), loadQ(jac0 + 4), loadQ(jac0 + 8), mA->m_invMasses.m_quad, mB->m_invMasses.m_quad), splat(1.0f));
    const Q det = _mm_sub_ps(_mm_mul_ps(wwww(diag1), wwww(diag0)), _mm_mul_ps(nd, nd));
    const Q invDet = _mm_mul_ps(rcp23(det), virtMassFactor);
    storeQ(jac0 + 8, setW(loadQ(jac0 + 8), _mm_mul_ps(invDet, wwww(diag1))));
    storeQ(jac1 + 8, setW(loadQ(jac1 + 8), _mm_mul_ps(invDet, wwww(diag0))));
    const float invJac01 = lane0(_mm_mul_ps(_mm_sub_ps(zero, nd), invDet));
    const float maxImpulse = lane0(frictionImpulse) * in.m_invNumStepsTimesMicroSteps; // x87, exact as a float product
    hkpJacobian2dFrictionSchema* friction2d = reinterpret_cast<hkpJacobian2dFrictionSchema*>(frictionSchema);
    setSchemaType(friction2d, hkpJacobianSchema::SCHEMA_TYPE_2D_FRICTION);
    friction2d->m_solverResults = reinterpret_cast<hkpSolverResults*>(info);
    friction2d->m_invJac01 = invJac01;
    friction2d->m_maxImpulse = maxImpulse;
    schema += hkpJacobianSchemaInfo::Friction2D::Sizeof;
    if (numPoints <= 1)
    {
        out.m_jacobianSchemas = reinterpret_cast<hkpJacobianSchema*>(schema);
        return;
    }

    // ---- 3d friction: the radius of the contact area (the mean distance of the points from their center, kept
    // until a point comes or goes); below 1e-6 (0x358637BD) the 2d friction stays
    Q radius = splat(info->m_contactRadius);
    if ((info->m_flags & hkpSimpleContactConstraintDataInfo::HK_FLAG_AREA_CHANGED) != 0)
    {
        Q distanceSum = zero;
        const hkContactPoint* point = atom->getContactPoints();
        for (int i = numPoints - 1; i >= 0; i--, point++)
        {
            const Q d = _mm_sub_ps(point->m_position.m_quad, avgPos);
            distanceSum = _mm_add_ps(distanceSum, sqrt23(hadd3(_mm_mul_ps(d, d))));
        }
        radius = _mm_mul_ps(distanceSum, invNumPoints);
        if (!(lane0(radius) >= 1e-06f))
        {
            info->m_contactRadius = lane0(radius);
            out.m_jacobianSchemas = reinterpret_cast<hkpJacobianSchema*>(schema);
            return;
        }
        info->m_flags &= ~hkpSimpleContactConstraintDataInfo::HK_FLAG_AREA_CHANGED;
    }
    // the angular row around the normal (over the 2d schema's tail: invJac01, max impulse and results move)
    hkpJacobian3dFrictionSchema* friction3d = reinterpret_cast<hkpJacobian3dFrictionSchema*>(frictionSchema);
    float* jac2 = reinterpret_cast<float*>(&friction3d->m_jac.m_jac2);
    const Q signMask = _mm_castsi128_ps(_mm_set1_epi32(0x80000000));
    Q ang0 = rotate(mA->m_scratch0.m_quad, mA->m_scratch1.m_quad, mA->m_scratch2.m_quad, avgNormal);
    storeQ(jac2, ang0);
    const Q ang1 = rotate(mB->m_scratch0.m_quad, mB->m_scratch1.m_quad, mB->m_scratch2.m_quad, _mm_xor_ps(avgNormal, signMask));
    storeQ(jac2 + 4, ang1);
    const Q diag2 = diag2Ang(ang0, ang1, mA->m_invMasses.m_quad, mB->m_invMasses.m_quad);
    ang0 = setW(ang0, effMass(diag2, virtMassFactor));
    storeQ(jac2, ang0);
    jac2[7] = info->m_data[1] * in.m_frictionRhsFactor;
    // the effective mass per unit radius
    storeQ(jac2, setW(loadQ(jac2), _mm_mul_ps(rcp23(radius), wwww(loadQ(jac2)))));
    friction3d->m_invJac01 = invJac01;
    friction3d->m_maxImpulse = maxImpulse;
    friction3d->m_contactRadius = lane0(radius);
    setSchemaType(friction3d, hkpJacobianSchema::SCHEMA_TYPE_3D_FRICTION);
    friction3d->m_solverResults = reinterpret_cast<hkpSolverResults*>(info);
    schema = frictionSchema + hkpJacobianSchemaInfo::Friction3D::Sizeof;
    info->m_contactRadius = lane0(radius);
    out.m_jacobianSchemas = reinterpret_cast<hkpJacobianSchema*>(schema);
}
