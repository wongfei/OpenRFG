// Original: volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/algorithms/raycast/hkcdraycastsphere.h
//
// Havok 2014.2.5 (havok_impl.md HK3): ray versus sphere, single rays and bundles of four. Inline only (no PDB
// records); ported from Havok 2014.1 (hkcdRayCastSphere.h/.inl) and checked against the copies inlined in
// hkpSphereShape::castRay (0x00D7B1E0) and castRayBundle (0x00D7B4D0). The bundle version differs from 2014.1 (see
// hkcdRayBundleSphereIntersect). Hand-written (not in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkfourtransposedpointsf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdraybundle.h"

// The ray against the sphere spherePosAndRadius (centre in xyz, radius in w). On a hit, the fraction and the unit
// normal are written.
HK_FORCE_INLINE hkcdRayCastResult hkcdRayCastSphere(const hkcdRay& ray, hkVector4Parameter spherePosAndRadius, hkSimdReal* HK_RESTRICT hitFractionInOut, hkVector4* HK_RESTRICT normalOut, hkFlags<hkcdRayQueryFlags::Enum, hkUint32> flags)
{
    hkVector4Comparison insideHits; hkcdRayQueryFlags::isFlagSet(flags, hkcdRayQueryFlags::ENABLE_INSIDE_HITS, insideHits);

    const hkVector4 dir = ray.getDirection();
    hkVector4 rayToSphere; rayToSphere.setSub(ray.m_origin, spherePosAndRadius);
    hkSimdReal offset = -dir.dot<3>(rayToSphere);
    hkSimdReal a = dir.lengthSquared<3>();
    hkSimdReal r = spherePosAndRadius.getComponent<3>();
    hkSimdReal r2; r2.setMul(r, r);

    hkSimdReal projFrac; projFrac.setDiv<HK_ACC_RAYCAST, HK_DIV_SET_MAX>(offset, a);
    hkVector4 projectedVector; projectedVector.setMul(dir, projFrac);
    hkVector4 cr; cr.setAdd(projectedVector, rayToSphere);
    hkSimdReal c; c.setSub(cr.lengthSquared<3>(), r2);
    hkSimdReal d; d.setMul(-a, c);
    if (d.isLessZero())
    {
        return hkcdRayCastResult::NO_HIT;
    }

    hkSimdReal dsqrt = d.sqrt();
    hkSimdReal t0 = -dsqrt + offset;
    hkSimdReal t1 = dsqrt + offset;
    hkVector4Comparison isInside = t0.lessZero();
    hkVector4Comparison isInsideHit; isInsideHit.setAnd(isInside, insideHits);
    hkSimdReal t; t.setSelect(isInsideHit, t1, t0);

    hkSimdReal hitFraction = *hitFractionInOut;
    if (t.isLessZero() | t.isGreaterEqual(a * hitFraction))
    {
        return hkcdRayCastResult::NO_HIT;
    }

    t.setDiv<HK_ACC_RAYCAST, HK_DIV_IGNORE>(t, a);
    hkVector4 hitPoint; hitPoint.setAddMul(ray.m_origin, ray.getDirection(), t);
    hkVector4 n; n.setSub(hitPoint, spherePosAndRadius);
    n.normalize<3>();

    *hitFractionInOut = t;
    *normalOut = n;
    int isInsideHitMask = isInsideHit.getMask();
    return hkcdRayCastResult(hkcdRayCastResult::BACK_FACE_HIT | (isInsideHitMask & hkcdRayCastResult::INSIDE_HIT));
}

// Four rays against a sphere of the given radius at the origin. The active rays that hit closer than their fraction
// get the new fraction and the unit normal. Differences from Havok 2014.1, as rfg.exe has it: the closest point on a
// long ray is interpolated between start and end, the infinite ray must strictly hit (det > 0), and the fraction is
// compared before the division by the squared ray length.
HK_FORCE_INLINE hkVector4Comparison hkcdRayBundleSphereIntersect(const hkcdRayBundle& rayBundle, hkSimdRealParameter sphereRadius, hkVector4& fractionsInOut, hkFourTransposedPoints& normalsOut)
{
    hkVector4Comparison activeMask = rayBundle.m_activeRays;

    const hkVector4* start = rayBundle.m_start.m_vertices;
    const hkVector4* end = rayBundle.m_end.m_vertices;

    hkVector4 dirX; dirX.setSub(end[0], start[0]);
    hkVector4 dirY; dirY.setSub(end[1], start[1]);
    hkVector4 dirZ; dirZ.setSub(end[2], start[2]);

    // A = |dir|^2, B = dir . start
    hkVector4 A; A.setMul(dirX, dirX);
    hkVector4 tmp; tmp.setMul(dirY, dirY);
    A.add(tmp);
    tmp.setMul(dirZ, dirZ);
    A.add(tmp);
    hkVector4 B; B.setMul(dirY, start[1]);
    tmp.setMul(dirX, start[0]);
    B.add(tmp);
    tmp.setMul(dirZ, start[2]);
    B.add(tmp);

    hkSimdReal radius2; radius2.setMul(sphereRadius, sphereRadius);
    const hkSimdReal oneHundred = hkSimdReal::fromFloat(100.0f);

    // Long rays (the closest point is far from the start): solve from the closest point instead of the start.
    hkVector4 AtimeRadius2time100; AtimeRadius2time100.setMul(A, radius2);
    AtimeRadius2time100.mul(oneHundred);
    hkVector4 B2; B2.setMul(B, B);
    hkVector4Comparison longRayCheck = AtimeRadius2time100.less(B2);

    hkVector4 offset; offset.setNeg<4>(B);
    offset.zeroIfFalse(longRayCheck);
    B.zeroIfTrue(longRayCheck);
    B2.zeroIfTrue(longRayCheck);

    hkVector4 invA; invA.setReciprocal<HK_ACC_23_BIT, HK_DIV_IGNORE>(A);
    hkVector4 midPointInterp; midPointInterp.setMul(invA, offset);

    hkVector4 C;
    {
        hkVector4 oneMinus; oneMinus.setSub(hkVector4::getConstant<HK_QUADREAL_1>(), midPointInterp);
        hkVector4 mx; mx.setMul(oneMinus, start[0]);
        hkVector4 my; my.setMul(oneMinus, start[1]);
        hkVector4 mz; mz.setMul(oneMinus, start[2]);
        hkVector4 t; t.setMul(end[0], midPointInterp); mx.setAdd(t, mx);
        t.setMul(end[1], midPointInterp); my.setAdd(t, my);
        t.setMul(end[2], midPointInterp); mz.setAdd(t, mz);
        C.setMul(mx, mx);
        t.setMul(my, my); C.add(t);
        t.setMul(mz, mz); C.add(t);
    }
    hkVector4 radius2V; radius2V.setAll(radius2);
    C.setSub(C, radius2V);

    hkVector4 negA; negA.setNeg<4>(A);
    hkVector4 det; det.setMul(C, negA);
    det.add(B2);
    hkVector4Comparison infRayDoesHit = hkVector4::getConstant<HK_QUADREAL_0>().less(det);
    activeMask.setAnd(activeMask, infRayDoesHit);

    hkVector4 sqDet; sqDet.setSqrt<HK_ACC_23_BIT, HK_SQRT_SET_ZERO>(det);
    sqDet.zeroIfFalse(infRayDoesHit);

    hkVector4 tV; tV.setSub(offset, B);
    tV.sub(sqDet);

    hkVector4 fractionsTimesA; fractionsTimesA.setMul(fractionsInOut, A);
    hkVector4Comparison tLessThanHitFrac = tV.less(fractionsTimesA);
    hkVector4Comparison tGreaterThan0 = hkVector4::getConstant<HK_QUADREAL_0>().lessEqual(tV);
    activeMask.setAnd(tLessThanHitFrac, activeMask);
    activeMask.setAnd(activeMask, tGreaterThan0);

    tV.mul(invA);

    hkVector4 invRadius; invRadius.setAll(sphereRadius);
    invRadius.setReciprocal<HK_ACC_23_BIT, HK_DIV_IGNORE>(invRadius);
    {
        hkVector4 oneMinus; oneMinus.setSub(hkVector4::getConstant<HK_QUADREAL_1>(), tV);
        hkVector4 t;
        hkVector4 nx; nx.setMul(oneMinus, start[0]); t.setMul(end[0], tV); nx.setAdd(t, nx);
        hkVector4 ny; ny.setMul(oneMinus, start[1]); t.setMul(end[1], tV); ny.setAdd(t, ny);
        hkVector4 nz; nz.setMul(oneMinus, start[2]); t.setMul(end[2], tV); nz.setAdd(t, nz);
        normalsOut.m_vertices[0].setMul(nx, invRadius);
        normalsOut.m_vertices[1].setMul(ny, invRadius);
        normalsOut.m_vertices[2].setMul(nz, invRadius);
    }
    fractionsInOut = tV;
    return activeMask;
}
