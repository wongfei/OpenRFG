// Original: volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/algorithms/raycast/hkcdraycastconvex.h
//
// Havok 2014.2.5 (havok_impl.md HK3): ray versus a convex hull given by its plane equations. Inline only (no PDB
// records); ported from Havok 2014.1 (hkcdRayCastConvex.h/.inl) and checked against the copy inlined in
// hkpConvexVerticesShape::castRay (0x00D97A70). Hand-written (not in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkintvector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4util.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdray.h"

// The ray (up to *fractionInOut) against the planes (at least 4), pushed out by shiftPlanes. On a hit, the fraction
// is scaled by the entry lambda; the entry plane is written to normalOut whenever the ray is not rejected early.
HK_FORCE_INLINE hkcdRayCastResult hkcdRayCastConvex(const hkcdRay& ray, const hkVector4* HK_RESTRICT planeEquations, int numPlaneEquations, hkSimdRealParameter shiftPlanes, hkSimdReal* HK_RESTRICT fractionInOut, hkVector4* HK_RESTRICT normalOut, hkFlags<hkcdRayQueryFlags::Enum, hkUint32> flags)
{
    hkVector4Comparison insideHits; hkcdRayQueryFlags::isFlagSet(flags, hkcdRayQueryFlags::ENABLE_INSIDE_HITS, insideHits);

    hkIntVector bestNrm0; bestNrm0.setZero();
    hkIntVector bestNrm1; bestNrm1.setZero();
    hkVector4 vLambda0; vLambda0.setConstant<HK_QUADREAL_MINUS1>();
    hkVector4 vLambda1; vLambda1.setConstant<HK_QUADREAL_2>();

    // w = 1 for the plane distances
    hkVector4 vFrom, vTo;
    {
        const hkSimdReal one = hkSimdReal_1;
        vFrom.setXYZ_W(ray.m_origin, one);
        vTo.setAddMul(ray.m_origin, ray.getDirection(), *fractionInOut);
        vTo.setXYZ_W(vTo, one);
    }

    // four planes at a time; the leftovers first (the last four planes), so nothing past the array is read
    hkIntVector crtIdx;
    hkVector4 planes0, planes1, planes2, planes3;
    {
        int n = numPlaneEquations - 4;
        crtIdx.set(n, n + 1, n + 2, n + 3);
        planes0 = planeEquations[n + 0];
        planes1 = planeEquations[n + 1];
        planes2 = planeEquations[n + 2];
        planes3 = planeEquations[n + 3];
    }

    hkIntVector nextIdx = hkIntVector::getConstant<HK_QUADINT_0123>();
    const int numPlanesMinus4 = numPlaneEquations - 4;
    int batchIndex = 0;
    while (true)
    {
        // the start and end distances to the planes
        hkVector4 dStart, dEnd;
        hkVector4Util::dot4_1vs4(vFrom, planes0, planes1, planes2, planes3, dStart);
        hkVector4Util::dot4_1vs4(vTo, planes0, planes1, planes2, planes3, dEnd);
        hkVector4 shift; shift.setAll(shiftPlanes);
        dStart.sub(shift);
        dEnd.sub(shift);

        // the crossing lambda (a division by zero when dStart == dEnd)
        hkVector4 vLambda;
        vLambda.setSub(dStart, dEnd);
        vLambda.setDiv<HK_ACC_RAYCAST, HK_DIV_IGNORE>(dStart, vLambda);

        hkVector4Comparison ds_ge_0 = dStart.greaterEqualZero();
        hkVector4Comparison de_ge_0 = dEnd.greaterEqualZero();

        // entirely in front of a plane: no hit
        {
            hkVector4Comparison notColliding;
            notColliding.setAnd(ds_ge_0, de_ge_0);
            if (notColliding.anyIsSet())
            {
                return hkcdRayCastResult::NO_HIT;
            }
        }

        // the entry: (dStart >= 0) && (dEnd < 0) && (lambda0 < lambda)
        {
            hkVector4Comparison cmp;
            cmp.setAnd(ds_ge_0, vLambda0.less(vLambda));
            cmp.setAndNot(cmp, de_ge_0);
            vLambda0.setSelect(cmp, vLambda, vLambda0);
            bestNrm0.setSelect(cmp, crtIdx, bestNrm0);
        }

        // the exit: (dStart < 0) && (dEnd >= 0) && (lambda < lambda1)
        {
            hkVector4Comparison cmp;
            cmp.setAnd(de_ge_0, vLambda1.greater(vLambda));
            cmp.setAndNot(cmp, ds_ge_0);
            vLambda1.setSelect(cmp, vLambda, vLambda1);
            bestNrm1.setSelect(cmp, crtIdx, bestNrm1);
        }

        if (batchIndex < numPlanesMinus4)
        {
            planes0 = planeEquations[batchIndex + 0];
            planes1 = planeEquations[batchIndex + 1];
            planes2 = planeEquations[batchIndex + 2];
            planes3 = planeEquations[batchIndex + 3];
            hkIntVector four; four.splatImmediate32<4>();
            crtIdx = nextIdx;
            nextIdx.setAddU32(nextIdx, four);
            batchIndex += 4;
        }
        else
        {
            break;
        }
    }

    {
        hkSimdReal lambda0 = vLambda0.horizontalMax<4>();
        hkSimdReal lambda1 = vLambda1.horizontalMin<4>();

        hkVector4Comparison isInside = lambda0.lessZero();
        hkVector4Comparison isInsideHit; isInsideHit.setAnd(isInside, insideHits);
        hkSimdReal lambda; lambda.setSelect(isInsideHit, lambda1, lambda0);
        hkIntVector bestNrm; bestNrm.setSelect(isInsideHit, bestNrm1, bestNrm0);

        fractionInOut->mul(lambda);

        {
            hkVector4 nvLambda1; nvLambda1.setNeg<4>(vLambda1);
            hkVector4 vLambda; vLambda.setSelect(isInsideHit, nvLambda1, vLambda0);
            int bestNrmIndex = bestNrm.getComponentAtVectorMax(vLambda);
            *normalOut = planeEquations[bestNrmIndex];
        }

        // no inside hits: (lambda1 < lambda0) || (lambda0 < 0) misses; inside hits: lambda1 never set misses
        hkVector4Comparison inter0; inter0.setAnd(lambda1.greaterEqual(lambda0), lambda0.greaterEqualZero());
        hkVector4Comparison inter1 = lambda1.notEqual(hkSimdReal_2);
        hkVector4Comparison inter; inter.setSelect(isInsideHit, inter1, inter0);

        int isHitMask = inter.getMask();
        int insideHitMask = isInsideHit.getMask();
        return hkcdRayCastResult((isHitMask & hkcdRayCastResult::FRONT_FACE_HIT) | (isHitMask & insideHitMask & hkcdRayCastResult::INSIDE_HIT));
    }
}
