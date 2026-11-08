// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/simpleconstraints/hkpsimpleconstraintutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK8): the single 3d contact solve of the TOI and bounce responses, from rfg.exe. The
// constraint frame is the normal plus two tangents; the mass matrix is solved in that frame, then the impulse is
// clipped to the friction cone and the maximum impulse.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/simpleconstraints/hkpsimpleconstraintutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkrotationf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/hkpsolversse.h"

using namespace hkSolverSse;

namespace
{
    __forceinline Q absQ(const Q& v) { return _mm_and_ps(v, _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF))); }
    __forceinline Q select(const Q& mask, const Q& a, const Q& b) { return _mm_or_ps(_mm_and_ps(mask, a), _mm_andnot_ps(mask, b)); }
    __forceinline Q negate(const Q& v) { return _mm_xor_ps(v, _mm_castsi128_ps(_mm_set1_epi32(0x80000000))); }

    // params.m_contactImpulseLimitBreached |= any lane of mask
    __forceinline void orBreached(hkpSimpleConstraintUtilCollideParams& params, const Q& mask)
    {
        params.m_contactImpulseLimitBreached = _mm_movemask_ps(mask) != 0 || params.m_contactImpulseLimitBreached;
    }
}

// 0x00EB7300: the world impulse is impulse in the base frame; linear by the inverse masses, angular by invIjac
void hkSimpleConstraintUtil_applyImpulse(const hkpSimpleConstraintInfo& info, const hkVector4f& impulse, hkpBodyVelocity& velA, hkpBodyVelocity& velB)
{
    const Q imp = impulse.m_quad;
    const Q world = rotate(info.m_base.m_col0.m_quad, info.m_base.m_col1.m_quad, info.m_base.m_col2.m_quad, imp);
    velA.m_linear.m_quad = _mm_add_ps(_mm_mul_ps(wwww(info.m_bodyInfo[0].m_invMasses.m_quad), world), velA.m_linear.m_quad);
    velB.m_linear.m_quad = _mm_sub_ps(velB.m_linear.m_quad, _mm_mul_ps(wwww(info.m_bodyInfo[1].m_invMasses.m_quad), world));
    const hkMatrix3f& ijA = info.m_bodyInfo[0].m_invIjac;
    const hkMatrix3f& ijB = info.m_bodyInfo[1].m_invIjac;
    velA.m_angular.m_quad = _mm_add_ps(rotate(ijA.m_col0.m_quad, ijA.m_col1.m_quad, ijA.m_col2.m_quad, imp), velA.m_angular.m_quad);
    velB.m_angular.m_quad = _mm_sub_ps(velB.m_angular.m_quad, rotate(ijB.m_col0.m_quad, ijB.m_col1.m_quad, ijB.m_col2.m_quad, imp));

    // w of every velocity cleared (PSLLDQ/PSRLDQ by 4)
    const Q xyz = _mm_castsi128_ps(_mm_set_epi32(0, -1, -1, -1));
    velA.m_angular.m_quad = _mm_and_ps(velA.m_angular.m_quad, xyz);
    velB.m_angular.m_quad = _mm_and_ps(velB.m_angular.m_quad, xyz);
    velA.m_linear.m_quad = _mm_and_ps(velA.m_linear.m_quad, xyz);
    velB.m_linear.m_quad = _mm_and_ps(velB.m_linear.m_quad, xyz);
}

// 0x00EB7420: the relative velocity at the contact in the base frame (x along the normal)
void hkSimpleConstraintUtil_getPointVelocity(const hkpSimpleConstraintInfo& info, hkpBodyVelocity& velA, hkpBodyVelocity& velB, hkVector4f& out)
{
    const Q dLin = _mm_sub_ps(velA.m_linear.m_quad, velB.m_linear.m_quad);
    hkVector4f r0 = info.m_base.m_col0;
    hkVector4f r1 = info.m_base.m_col1;
    hkVector4f r2 = info.m_base.m_col2;
    HK_TRANSPOSE3f(r0, r1, r2);
    const hkRotationf& jA = info.m_bodyInfo[0].m_jacT;
    const hkRotationf& jB = info.m_bodyInfo[1].m_jacT;
    const Q angB = rotate(jB.m_col0.m_quad, jB.m_col1.m_quad, jB.m_col2.m_quad, velB.m_angular.m_quad);
    const Q angA = rotate(jA.m_col0.m_quad, jA.m_col1.m_quad, jA.m_col2.m_quad, velA.m_angular.m_quad);
    const Q lin = rotate(r0.m_quad, r1.m_quad, r2.m_quad, dLin);
    out.m_quad = _mm_add_ps(_mm_sub_ps(angA, angB), lin);
}

// 0x00EB7660: returns the normal impulse. The license check (HK_flyingcolors_constraintutil) always passes here.
float hkSimpleConstraintUtil_Collide(const hkpSimpleConstraintInfo& info, hkpSimpleConstraintUtilCollideParams& params, hkpBodyVelocity& velA, hkpBodyVelocity& velB)
{
    hkVector4f pointVel;
    hkSimpleConstraintUtil_getPointVelocity(info, velA, velB, pointVel);
    Q v = pointVel.m_quad;
    if (lane0(v) > params.m_externalSeperatingVelocity)
    {
        v = _mm_move_ss(v, _mm_set_ss(params.m_externalSeperatingVelocity));
        if (params.m_externalSeperatingVelocity >= params.m_extraSeparatingVelocity)
        {
            return 0.0f;
        }
    }

    // the wanted velocity change: the normal part bounces with the restitution plus the extra separating velocity
    const Q negV = negate(v);
    const Q negX = xxxx(negV);
    const Q restVel = _mm_mul_ps(splat(params.m_restitution), negX);
    const Q target = _mm_add_ps(splat(params.m_extraSeparatingVelocity), _mm_add_ps(restVel, negX));
    Q dv = _mm_move_ss(negV, target);
    params.m_contactImpulseLimitBreached = false;

    const Q zero = _mm_setzero_ps();
    const Q maxImp = splat(params.m_maxImpulse);
    const hkMatrix3f& mm = info.m_massMatrix;
    const hkMatrix3f& imm = info.m_invMassMatrix;
    Q imp = rotate(mm.m_col0.m_quad, mm.m_col1.m_quad, mm.m_col2.m_quad, dv);
    {
        const Q ix = xxxx(imp);
        const Q over = _mm_cmplt_ps(maxImp, ix);
        const Q scaled = _mm_mul_ps(_mm_mul_ps(rcp23(ix), maxImp), imp);
        orBreached(params, over);
        imp = select(over, scaled, imp);
    }

    Q result = imp;
    const float friction2 = params.m_friction * params.m_friction;
    const Q imp2 = _mm_mul_ps(imp, imp);
    const Q tangent2 = _mm_add_ps(zzzz(imp2), yyyy(imp2));
    const Q limit2 = _mm_mul_ps(splat(friction2), xxxx(imp2));
    if (lane0(tangent2) > lane0(limit2) || !(lane0(imp) >= 0.0f))
    {
        // sliding (or pulling): the normal alone, then the tangents clipped to the friction cone
        const Q dvT = _mm_move_ss(dv, zero);
        const Q tanVel2 = hadd3(_mm_mul_ps(dvT, dvT));
        const Q mass00 = wwww(imm.m_col0.m_quad);
        Q nImp = _mm_mul_ps(mass00, xxxx(dv));
        orBreached(params, _mm_cmplt_ps(maxImp, nImp));
        nImp = _mm_min_ps(nImp, maxImp);
        dv = _mm_sub_ps(dv, _mm_mul_ps(imm.m_col0.m_quad, nImp));
        result = _mm_move_ss(zero, nImp);

        // the tangential impulse: the 2x2 lower block of the inverse mass matrix (symmetric) inverted
        const Q invDet12 = wwww(imm.m_col1.m_quad);
        const Q c2y = yyyy(imm.m_col2.m_quad);
        const Q dy = yyyy(dv);
        const Q dz = zzzz(dv);
        const Q t1 = _mm_mul_ps(_mm_sub_ps(_mm_mul_ps(zzzz(imm.m_col2.m_quad), dy), _mm_mul_ps(c2y, dz)), invDet12);
        const Q t2 = _mm_mul_ps(_mm_sub_ps(_mm_mul_ps(yyyy(imm.m_col1.m_quad), dz), _mm_mul_ps(c2y, dy)), invDet12);
        const Q ti = _mm_movelh_ps(_mm_unpacklo_ps(zero, t1), _mm_unpacklo_ps(t2, zero));
        const Q ti2 = hadd3(_mm_mul_ps(ti, ti));
        Q maxF = _mm_mul_ps(xxxx(result), splat(params.m_friction));
        maxF = _mm_mul_ps(maxF, maxF);
        const Q scale = sqrt23(_mm_mul_ps(rcp23(ti2), maxF));
        const Q tiC = select(_mm_cmplt_ps(maxF, ti2), _mm_mul_ps(scale, ti), ti);
        result = select(_mm_load_ps(reinterpret_cast<const float*>(&hkVector4fComparison_maskToComparison[hkVector4ComparisonMask::MASK_YZ * 4])), tiC, result);
        dv = _mm_sub_ps(_mm_sub_ps(dv, _mm_mul_ps(yyyy(tiC), imm.m_col1.m_quad)), _mm_mul_ps(zzzz(tiC), imm.m_col2.m_quad));

        // the restitution part of the normal impulse, which the tangents' coupling has taken away
        const Q extra = _mm_max_ps(_mm_mul_ps(_mm_sub_ps(xxxx(dv), restVel), mass00), zero);
        const Q n0 = xxxx(result);
        const Q over = _mm_cmplt_ps(maxImp, _mm_add_ps(n0, extra));
        orBreached(params, over);
        const Q add = select(over, _mm_sub_ps(maxImp, n0), extra);
        result = _mm_move_ss(result, _mm_add_ps(n0, add));
        dv = _mm_move_ss(_mm_sub_ps(dv, _mm_mul_ps(add, imm.m_col0.m_quad)), zero);

        // extra slope: more normal impulse for the remaining tangential velocity
        const Q rem = sqrt23(_mm_min_ps(hadd3(_mm_mul_ps(dv, dv)), tanVel2));
        const Q slopeImp = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(mass00, rem), splat(params.m_extraSlope)), xxxx(result));
        orBreached(params, _mm_cmplt_ps(maxImp, slopeImp));
        result = _mm_move_ss(result, _mm_min_ps(maxImp, slopeImp));
    }

    hkVector4f impulse;
    impulse.m_quad = result;
    hkSimpleConstraintUtil_applyImpulse(info, impulse, velA, velB);
    return lane0(result);
}

// 0x00EB7AE0
void hkSimpleConstraintUtil_InitInfo(const hkpSimpleConstraintInfoInitInput& bodyA, const hkpSimpleConstraintInfoInitInput& bodyB, const hkMatrix3f& directions, hkpSimpleConstraintInfo& out)
{
    const Q sumInvMass = _mm_add_ps(wwww(bodyA.m_invMasses.m_quad), wwww(bodyB.m_invMasses.m_quad));
    out.m_invMassMatrix.m_col0.m_quad = _mm_mul_ps(sumInvMass, g_vectorfConstants[HK_QUADREAL_1000]);
    out.m_invMassMatrix.m_col1.m_quad = _mm_mul_ps(sumInvMass, g_vectorfConstants[HK_QUADREAL_0100]);
    out.m_invMassMatrix.m_col2.m_quad = _mm_mul_ps(sumInvMass, g_vectorfConstants[HK_QUADREAL_0010]);
    out.m_base.m_col0 = directions.m_col0;
    out.m_base.m_col1 = directions.m_col1;
    out.m_base.m_col2 = directions.m_col2;
    const Q epsSqrd = g_vectorfConstants[HK_QUADREAL_EPS_SQRD];

    for (int b = 0; b < 2; b++)
    {
        const hkpSimpleConstraintInfoInitInput& in = b == 0 ? bodyA : bodyB;
        hkpSimpleConstraintInfo::BodyInfo& bi = out.m_bodyInfo[b];
        const Q r = in.m_massRelPos.m_quad;
        bi.m_jacT.m_col0.m_quad = cross(r, directions.m_col0.m_quad);
        bi.m_jacT.m_col1.m_quad = cross(r, directions.m_col1.m_quad);
        bi.m_jacT.m_col2.m_quad = cross(r, directions.m_col2.m_quad);
        const Q i0 = in.m_invInertia.m_col0.m_quad;
        const Q i1 = in.m_invInertia.m_col1.m_quad;
        const Q i2 = in.m_invInertia.m_col2.m_quad;
        bi.m_invIjac.m_col2.m_quad = rotate(i0, i1, i2, bi.m_jacT.m_col2.m_quad);
        bi.m_invIjac.m_col1.m_quad = rotate(i0, i1, i2, bi.m_jacT.m_col1.m_quad);
        bi.m_invIjac.m_col0.m_quad = rotate(i0, i1, i2, bi.m_jacT.m_col0.m_quad);
        bi.m_jacT.transpose();

        // the body's part of the inverse mass matrix: jac^T * invI * jac
        const Q t0 = bi.m_jacT.m_col0.m_quad;
        const Q t1 = bi.m_jacT.m_col1.m_quad;
        const Q t2 = bi.m_jacT.m_col2.m_quad;
        hkMatrix3f part;
        part.m_col2.m_quad = rotate(t0, t1, t2, bi.m_invIjac.m_col2.m_quad);
        part.m_col1.m_quad = rotate(t0, t1, t2, bi.m_invIjac.m_col1.m_quad);
        part.m_col0.m_quad = rotate(t0, t1, t2, bi.m_invIjac.m_col0.m_quad);
        out.m_invMassMatrix.add(part);

        bi.m_invMasses = in.m_invMasses;
        bi.m_mass = lane0(rcp23(_mm_max_ps(wwww(in.m_invMasses.m_quad), epsSqrd)));
        bi.m_transform = in.m_transform;
    }

    // the mass matrix: the inverse by the cofactors, zero when the determinant is below the matrix's scale * EPS
    hkMatrix3f& im = out.m_invMassMatrix;
    const Q c0 = im.m_col0.m_quad;
    const Q c1 = im.m_col1.m_quad;
    const Q c2 = im.m_col2.m_quad;
    const Q r0 = cross(c1, c2);
    const Q r1 = cross(c2, c0);
    const Q r2 = cross(c0, c1);
    const Q det = hadd3(_mm_mul_ps(c0, r0));
    const Q tolerance = _mm_mul_ps(_mm_mul_ps(hadd3(absQ(c2)), hadd3(absQ(c1))), _mm_mul_ps(hadd3(absQ(c0)), g_vectorfConstants[HK_QUADREAL_EPS]));
    const Q ok = _mm_cmplt_ps(tolerance, absQ(det));
    const Q invDet = rcp23(det);
    out.m_massMatrix.m_col1.m_quad = _mm_mul_ps(invDet, r1);
    out.m_massMatrix.m_col0.m_quad = _mm_and_ps(_mm_mul_ps(invDet, r0), ok);
    out.m_massMatrix.m_col2.m_quad = _mm_mul_ps(invDet, r2);
    out.m_massMatrix.m_col1.m_quad = _mm_and_ps(out.m_massMatrix.m_col1.m_quad, ok);
    out.m_massMatrix.m_col2.m_quad = _mm_and_ps(out.m_massMatrix.m_col2.m_quad, ok);

    // getMass00: 1 / invM00; getInvDetM12: 1 / det of the lower 2x2 block (EPS_SQRD squared as its floor)
    im.m_col0.m_quad = setW(im.m_col0.m_quad, rcp23(_mm_max_ps(epsSqrd, xxxx(im.m_col0.m_quad))));
    const Q m = _mm_mul_ps(_mm_shuffle_ps(im.m_col2.m_quad, im.m_col2.m_quad, 0xD8), im.m_col1.m_quad);
    const Q det12 = _mm_sub_ps(yyyy(m), zzzz(m));
    im.m_col1.m_quad = setW(im.m_col1.m_quad, rcp23(_mm_max_ps(_mm_mul_ps(epsSqrd, epsSqrd), det12)));
}
