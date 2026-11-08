// Original: volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/algorithms/raycast/hkcdraycastbox.h
//
// Havok 2014.2.5 (havok_impl.md HK3): ray versus box (slab test). Inline only (no PDB records); ported from Havok
// 2014.1 (hkcdRayCastBox.h/.inl) and checked against the copy inlined in hkpBoxShape::castRay (0x00D822D0).
// Hand-written (not in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdray.h"

// The ray against the box [-vHalfExtents, vHalfExtents]. Returns non-zero on a hit closer than *fractionInOut; the
// fraction is then updated. The normal of the entry face is written on every call that gets past the early outs (the
// first axis whose slab gives the entry time; with no axis, the constant before HK_QUADREAL_1000 is read, as in the
// original).
inline hkBool32 hkcdRayCastBox(const hkcdRay& ray, hkVector4Parameter vHalfExtents, hkSimdReal* HK_RESTRICT fractionInOut, hkVector4* HK_RESTRICT normalOut)
{
    // A and B are the start and end of the ray
    hkVector4 vA = ray.m_origin;
    hkVector4 vAB = ray.getDirection();
    hkVector4 vB; ray.getEndPoint(vB);

    {
        hkVector4 negHalfExtents; negHalfExtents.setNeg<3>(vHalfExtents);
        const hkVector4Comparison cmpRayStartOutR = vHalfExtents.lessEqual(vA); // the start is outside, to the right
        const hkVector4Comparison cmpRayStartOutL = vA.lessEqual(negHalfExtents); // the start is outside, to the left
        hkVector4Comparison cmpRayOutR; cmpRayOutR.setAnd(cmpRayStartOutR, vHalfExtents.lessEqual(vB));
        hkVector4Comparison cmpRayOutL; cmpRayOutL.setAnd(cmpRayStartOutL, vB.lessEqual(negHalfExtents));
        hkVector4Comparison cmpRayOut; cmpRayOut.setOr(cmpRayOutL, cmpRayOutR); // the whole ray is outside one slab
        hkVector4Comparison cmpRayStartOut; cmpRayStartOut.setOr(cmpRayStartOutL, cmpRayStartOutR);
        if (cmpRayOut.anyIsSet<hkVector4ComparisonMask::MASK_XYZ>() || // no intersection
            !cmpRayStartOut.anyIsSet<hkVector4ComparisonMask::MASK_XYZ>()) // the start is inside
        {
            return false;
        }
    }

    // The slab times: T1 = -(halfExtents + A) / AB, T2 = (halfExtents - A) / AB, per component.
    hkSimdReal fEntry, fExit;
    {
        hkVector4 T1; T1.setAdd(vHalfExtents, vA);
        hkVector4 T2; T2.setSub(vHalfExtents, vA);
        hkVector4Comparison cmp0 = vAB.notEqualZero();

        // no division by zero (HK_QUADREAL_EPS where the direction is 0)
        vAB.setSelect(cmp0, vAB, hkVector4::getConstant<HK_QUADREAL_EPS>());
        vAB.setReciprocal(vAB);
        T1.mul(vAB);
        T2.mul(vAB);
        T1.setNeg<4>(T1);

        hkVector4 tEntry, tExit;
        tEntry.setMin(T1, T2);
        tExit.setMax(T1, T2);

        // only the slabs the ray is not parallel to limit it
        hkVector4 tFar; tFar.setConstant<HK_QUADREAL_MAX>();
        hkVector4 tNear; tNear.setConstant<HK_QUADREAL_MINUS_MAX>();
        tNear.setSelect(cmp0, tEntry, tNear);
        tFar.setSelect(cmp0, tExit, tFar);

        fEntry = tNear.horizontalMax<3>();
        fExit = tFar.horizontalMin<3>();

        // the entry face's normal; negated where the entry time comes from T1 (the negative planes)
        int normalIndex = tNear.findComponent<3>(fEntry);
        normalOut->setFlipSign(hkVector4::getConstant((hkVectorConstant)(HK_QUADREAL_1000 + normalIndex)), T1.less(T2));
    }

    // a hit when 0 <= tEntry <= tExit and tEntry is closer than the fraction
    {
        hkVector4Comparison cmp0 = fEntry.lessEqual(fExit);
        hkVector4Comparison cmp1 = fEntry.greaterEqualZero();
        hkVector4Comparison cmp2 = fEntry.less(*fractionInOut);
        cmp0.setAnd(cmp0, cmp1);
        cmp0.setAnd(cmp0, cmp2);
        fractionInOut->setSelect(cmp0, fEntry, *fractionInOut);
        return cmp0.anyIsSet();
    }
}
