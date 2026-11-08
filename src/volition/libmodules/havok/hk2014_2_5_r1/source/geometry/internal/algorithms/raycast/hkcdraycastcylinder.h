// Original: volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/algorithms/raycast/hkcdraycastcylinder.h
//
// Havok 2014.2.5 (havok_impl.md HK3): ray versus cylinder. Inline only (no PDB records); ported from Havok 2014.1
// (hkcdRayCastCylinder.h/.inl) and checked against the copy inlined in hkpCylinderShape::castRay (0x00D91FF0).
// Hand-written (not in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4util.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdray.h"

// The ray against the cylinder from vP to vQ. The intersection must project between P and Q. Returns non-zero on a
// hit closer than *fractionInOut, which is then updated; the normal is written on every call that gets past the
// early outs.
HK_FORCE_INLINE hkBool32 hkcdRayCastCylinder(const hkcdRay& ray, hkVector4Parameter vP, hkVector4Parameter vQ, hkSimdRealParameter radius, hkSimdReal* HK_RESTRICT fractionInOut, hkVector4* HK_RESTRICT normalOut)
{
    // the normalized cylinder direction
    hkVector4 u;
    u.setSub(vQ, vP);
    u.normalize<3>();
    hkVector4 normal;

    // the intersection time limits of the end caps
    hkSimdReal t0, t1;
    hkVector4Comparison capPHitFirst;
    {
        const hkVector4 vA = ray.m_origin; // the start of the ray
        const hkVector4 vAB = ray.getDirection(); // B - A
        hkVector4 vPA; vPA.setSub(vA, vP);
        hkVector4 vQA; vQA.setSub(vA, vQ);

        // (PA . u, QA . u, AB . u)
        hkVector4 vDots;
        hkVector4Util::dot3_3vs3(vPA, u, vQA, u, vAB, u, vDots);
        t0 = vDots.getComponent<0>();
        t1 = vDots.getComponent<1>();
        hkSimdReal dotABu = vDots.getComponent<2>();

        // AB . u == 0: the ray is perpendicular to the axis; it can only hit between the caps
        hkSimdReal invDotABu; invDotABu.setReciprocal<HK_ACC_RAYCAST, HK_DIV_SET_HIGH>(dotABu);

        // perpendicular: +-infinity by the side of each cap (both the same: no hit)
        hkSimdReal tmp0 = hkSimdReal_Max;
        hkSimdReal tmp1 = tmp0;
        tmp0.setFlipSign(tmp0, t0);
        tmp1.setFlipSign(tmp1, t1);
        hkVector4Comparison cmpPerpendicular = dotABu.equalZero();
        tmp0.setSelect(cmpPerpendicular, tmp0, -t0 * invDotABu);
        tmp1.setSelect(cmpPerpendicular, tmp1, -t1 * invDotABu);
        t0.setMin(tmp0, tmp1);
        t1.setMax(tmp0, tmp1);
        if (t0.isEqual(t1))
        {
            return false; // the intersection times coincide
        }

        // tmp0 comes from the P cap (normal -u), tmp1 from the Q cap (normal u)
        capPHitFirst = tmp0.less(tmp1);
        normal.setFlipSign(u, capPHitFirst);
    }

    // the infinite cylinder; its intersection times must fall inside [t0, t1]
    hkSimdReal t2, t3;
    {
        const hkVector4 vA = ray.m_origin;
        const hkVector4 vAB = ray.getDirection();
        hkVector4 vPA; vPA.setSub(vA, vP);

        hkVector4 vDots;
        hkVector4Util::dot3_4vs4(vAB, vAB, vAB, u, vAB, vPA, vPA, u, vDots);

        // the quadratic's factors
        const hkSimdReal AB_AB = vDots.getComponent<0>();
        const hkSimdReal ABu = vDots.getComponent<1>();
        const hkSimdReal AB_PA = vDots.getComponent<2>();
        const hkSimdReal PAu = vDots.getComponent<3>();
        const hkSimdReal PA_PA = vPA.dot<3>(vPA);
        hkSimdReal a = AB_AB - ABu * ABu;
        hkSimdReal b = AB_PA - PAu * ABu;
        hkSimdReal c = PA_PA - PAu * PAu - radius * radius;

        // A ray parallel to the axis: only the cap test counts (a tiny negative discriminant would miss obvious cap
        // hits).
        if (a.isLess(hkSimdReal_Eps))
        {
            hkVector4 hitPoint; hitPoint.setAddMul(vA, vAB, t0);
            hkVector4 closestCapPoint; closestCapPoint.setSelect(capPHitFirst, vP, vQ);
            hkSimdReal distToCapCenter = hitPoint.distanceToSquared(closestCapPoint);
            if (distToCapCenter.isGreater(radius * radius))
            {
                return false;
            }
            t2 = hkSimdReal_MinusMax;
            t3 = hkSimdReal_Max;
        }
        else
        {
            hkSimdReal delta = b * b - a * c;
            if (delta.isLessZero())
            {
                return false;
            }
            delta = delta.sqrt();
            a.setReciprocal(a);

            // for any t in [t2, t3] the ray is within the radius of the axis
            hkSimdReal tmp2 = -(delta + b) * a;
            hkSimdReal tmp3 = (delta - b) * a;
            t2.setMin(tmp2, tmp3);
            t3.setMax(tmp2, tmp3);

            // the side normal at t2
            {
                hkVector4 vX; vX.setAddMul(vA, vAB, t2);
                hkVector4 vPX; vPX.setSub(vX, vP);
                hkVector4 vPX0; vPX0.setMul(vPX.dot<3>(u), u);
                hkVector4 vX0X; vX0X.setSub(vPX, vPX0);
                vX0X.normalize<3>();
                normal.setSelect(t0.greater(t2), normal, vX0X);
            }
        }
    }

    hkSimdReal fEntry, fExit;
    fEntry.setMax(t0, t2);
    fExit.setMin(t1, t3);

    {
        hkSimdReal fraction = *fractionInOut;
        hkVector4Comparison cmp0 = fEntry.lessEqual(fExit);
        hkVector4Comparison cmp1 = fEntry.greaterEqualZero();
        hkVector4Comparison cmp2 = fEntry.less(fraction);
        cmp0.setAnd(cmp0, cmp1);
        cmp0.setAnd(cmp0, cmp2);
        fractionInOut->setSelect(cmp0, fEntry, fraction);
        *normalOut = normal;
        return cmp0.anyIsSet();
    }
}
