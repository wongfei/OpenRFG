// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/collide/linesegment/hklinesegmentutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK2): hkLineSegmentUtil from rfg.exe. The SDK source is not available, so the SSE
// sequences are written out as the binary has them (hkdiff compares the results bit for bit). Every lane of a
// "scalar" holds the same value, as in hkSimdFloat32.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/collide/linesegment/hklinesegmentutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h"

namespace
{
// dot<3>: (y + x) + z, broadcast
HK_FORCE_INLINE __m128 dot3(const __m128& a, const __m128& b)
{
    const __m128 p = _mm_mul_ps(a, b);
    return _mm_add_ps(_mm_add_ps(_mm_shuffle_ps(p, p, 0x55), _mm_shuffle_ps(p, p, 0x00)), _mm_shuffle_ps(p, p, 0xAA));
}

// 1 / x: 12-bit _mm_rcp_ps refined by one Newton step, (2 - rcp * x) * rcp (hkMath::quadReciprocal)
HK_FORCE_INLINE __m128 reciprocal(const __m128& x)
{
    const __m128 r = _mm_rcp_ps(x);
    return _mm_mul_ps(_mm_sub_ps(*(const __m128*)hkMath::hkSse_floatTwo, _mm_mul_ps(r, x)), r);
}

// 1 / sqrt(x), zero where x <= 0: (3 - x * r * r) * (r * 0.5) (hkMath::quadReciprocalSquareRoot, HK_SQRT_SET_ZERO)
HK_FORCE_INLINE __m128 sqrtInverseSetZero(const __m128& x)
{
    const __m128 r = _mm_rsqrt_ps(x);
    const __m128 e = _mm_mul_ps(_mm_mul_ps(r, x), r);
    const __m128 n = _mm_mul_ps(_mm_sub_ps(*(const __m128*)hkMath::hkSse_floatThree, e), _mm_mul_ps(r, *(const __m128*)hkMath::hkSse_floatHalf));
    return _mm_andnot_ps(_mm_cmple_ps(x, _mm_setzero_ps()), n);
}

// sqrt(x) as x / sqrt(x), zero where x <= 0
HK_FORCE_INLINE __m128 sqrtSetZero(const __m128& x)
{
    const __m128 r = _mm_rsqrt_ps(x);
    const __m128 e = _mm_mul_ps(_mm_mul_ps(r, x), r);
    const __m128 h = _mm_mul_ps(r, *(const __m128*)hkMath::hkSse_floatHalf);
    const __m128 s = _mm_mul_ps(_mm_mul_ps(_mm_sub_ps(*(const __m128*)hkMath::hkSse_floatThree, e), h), x);
    return _mm_andnot_ps(_mm_cmple_ps(x, _mm_setzero_ps()), s);
}

HK_FORCE_INLINE __m128 absolute(const __m128& x)
{
    return _mm_castsi128_ps(_mm_srli_epi32(_mm_slli_epi32(_mm_castps_si128(x), 1), 1));
}

HK_FORCE_INLINE __m128 negate(const __m128& x)
{
    return _mm_xor_ps(x, _mm_castsi128_ps(_mm_set1_epi32(0x80000000)));
}

// (xyz of a, w of b)
HK_FORCE_INLINE __m128 xyzW(const __m128& a, const __m128& b)
{
    return _mm_shuffle_ps(a, _mm_unpackhi_ps(a, b), 0xC4);
}

HK_FORCE_INLINE float lane0(const __m128& a)
{
    return _mm_cvtss_f32(a);
}
}

// 0x00FB8200
// The point of the segment B..B2 closest to A. Returns 8 at B and 4 at B2 (the binary's values), 0 in between.
int HK_CALL hkLineSegmentUtil::closestPointLineSeg(hkVector4fParameter A, hkVector4fParameter B, hkVector4fParameter B2, ClosestPointLineSegResult& result)
{
    const __m128 d = _mm_sub_ps(B2.m_quad, B.m_quad);
    const __m128 proj = dot3(_mm_sub_ps(A.m_quad, B.m_quad), d);
    const __m128 len2 = dot3(d, d);
    if (!(lane0(proj) > 0.0f))
    {
        result.m_pointOnEdge = B;
        return 8;
    }
    if (lane0(proj) >= lane0(len2))
    {
        result.m_pointOnEdge = B2;
        return 4;
    }
    result.m_pointOnEdge.m_quad = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(reciprocal(len2), proj), d), B.m_quad);
    return 0;
}

// 0x00FB7FB0
// Closest points of the segments A + t * dA and B + u * dB (t, u in [0, 1]). The flags tell which end was clamped:
// 1 t = 1, 2 t = 0, 4 u = 1, 8 u = 0. Nearly parallel segments (|det| <= 8 * eps * (ac + bb)) take t = 1.
int HK_CALL hkLineSegmentUtil::closestLineSegLineSeg(hkVector4fParameter A, hkVector4fParameter dA, hkVector4fParameter B, hkVector4fParameter dB, ClosestLineSegLineSegResult& result)
{
    const __m128 zero = _mm_setzero_ps();
    const __m128 one = g_vectorfConstants[HK_QUADREAL_1];
    const __m128 d = _mm_sub_ps(B.m_quad, A.m_quad);
    const __m128 b = dot3(dB.m_quad, dA.m_quad);
    const __m128 d1 = dot3(dA.m_quad, d);
    const __m128 d2 = dot3(dB.m_quad, d);
    const __m128 a = dot3(dA.m_quad, dA.m_quad);
    const __m128 c = dot3(dB.m_quad, dB.m_quad);
    const __m128 ac = _mm_mul_ps(c, a);
    const __m128 bb = _mm_mul_ps(b, b);
    const __m128 num = _mm_sub_ps(_mm_mul_ps(c, d1), _mm_mul_ps(d2, b));
    const __m128 absDet = absolute(_mm_sub_ps(ac, bb));

    int flags;
    __m128 t;
    if (lane0(_mm_mul_ps(num, absDet)) >= lane0(_mm_mul_ps(absDet, absDet)))
    {
        t = one;
        flags = 1;
    }
    else if (!(lane0(num) > 0.0f))
    {
        t = zero;
        flags = 2;
    }
    else
    {
        const __m128 eps8 = _mm_mul_ps(g_vectorfConstants[HK_QUADREAL_EPS], g_vectorfConstants[HK_QUADREAL_8]);
        if (lane0(absDet) > lane0(_mm_mul_ps(_mm_add_ps(bb, ac), eps8)))
        {
            t = _mm_mul_ps(reciprocal(absDet), num);
            flags = 0;
        }
        else
        {
            t = one;
            flags = 1;
        }
    }

    __m128 u;
    const __m128 uNum = _mm_sub_ps(_mm_mul_ps(t, b), d2);
    if (lane0(uNum) >= lane0(c))
    {
        u = one;
        flags = 4;
    }
    else if (!(lane0(uNum) > 0.0f))
    {
        u = zero;
        flags = 8;
    }
    else
    {
        u = _mm_mul_ps(reciprocal(c), uNum);
        goto done;
    }

    {
        // u was clamped: t again for the clamped u
        const __m128 tNum = _mm_add_ps(_mm_mul_ps(u, b), d1);
        if (!(lane0(tNum) > 0.0f))
        {
            t = zero;
            flags |= 2;
        }
        else if (lane0(tNum) >= lane0(a))
        {
            t = one;
            flags |= 1;
        }
        else
        {
            t = _mm_mul_ps(reciprocal(a), tNum);
        }
    }

done:
    result.m_t.m_real = t;
    result.m_u.m_real = u;
    const __m128 pA = _mm_add_ps(_mm_mul_ps(dA.m_quad, t), A.m_quad);
    const __m128 pB = _mm_add_ps(_mm_mul_ps(dB.m_quad, u), B.m_quad);
    const __m128 diff = _mm_sub_ps(pA, pB);
    result.m_closestPointA.m_quad = pA;
    result.m_closestAminusClosestB.m_quad = diff;
    result.m_distanceSquared = lane0(dot3(diff, diff));
    return flags;
}

// 0x00FB82B0
// Closest points of the infinite lines; HK_FAILURE (fractionA 0) when they are nearly parallel.
hkResult HK_CALL hkLineSegmentUtil::closestPointInfLineInfLine(const hkVector4& A, const hkVector4& dA, const hkVector4& B, const hkVector4& dB, ClosestPointInfLineInfLineResult& result)
{
    const __m128 d = _mm_sub_ps(B.m_quad, A.m_quad);
    const __m128 b = dot3(dB.m_quad, dA.m_quad);
    const __m128 dAd = _mm_mul_ps(dA.m_quad, d);
    const __m128 d2 = dot3(dB.m_quad, d);
    const __m128 a = dot3(dA.m_quad, dA.m_quad);
    const __m128 c = dot3(dB.m_quad, dB.m_quad);
    const __m128 ac = _mm_mul_ps(c, a);
    const __m128 bb = _mm_mul_ps(b, b);
    const __m128 det = _mm_sub_ps(ac, bb);
    const __m128 eps8 = _mm_mul_ps(g_vectorfConstants[HK_QUADREAL_EPS], g_vectorfConstants[HK_QUADREAL_8]);
    const __m128 limit = _mm_mul_ps(_mm_add_ps(absolute(ac), bb), eps8);

    hkResult res;
    __m128 t;
    if (lane0(absolute(det)) > lane0(limit))
    {
        const __m128 d1 = _mm_add_ps(_mm_add_ps(_mm_shuffle_ps(dAd, dAd, 0x55), _mm_shuffle_ps(dAd, dAd, 0x00)), _mm_shuffle_ps(dAd, dAd, 0xAA));
        const __m128 num = _mm_sub_ps(_mm_mul_ps(d1, c), _mm_mul_ps(d2, b));
        t = _mm_mul_ps(reciprocal(det), num);
        res.m_enum = HK_SUCCESS;
    }
    else
    {
        t = _mm_setzero_ps();
        res.m_enum = HK_FAILURE;
    }

    const __m128 u = _mm_mul_ps(reciprocal(c), _mm_sub_ps(_mm_mul_ps(b, t), d2));
    const __m128 pA = _mm_add_ps(_mm_mul_ps(dA.m_quad, t), A.m_quad);
    const __m128 pB = _mm_add_ps(_mm_mul_ps(dB.m_quad, u), B.m_quad);
    result.m_closestPointA.m_quad = pA;
    result.m_closestPointB.m_quad = pB;
    const __m128 diff = _mm_sub_ps(pA, pB);
    result.m_distanceSquared = lane0(dot3(diff, diff));
    result.m_fractionA = lane0(t);
    result.m_fractionB = lane0(u);
    return res;
}

// 0x00FB8540
// Contact manifold of two capsules (segments with radii): points[0] at the closest points of the axes and, when the
// axes are nearly parallel (|det| <= a * c / 5), up to two more at the ends of capsule A's overlap. The
// separating normal's w of each output point is read as its distance tolerance on input. Returns the clamping flags
// of the closest points (see closestLineSegLineSeg).
int HK_CALL hkLineSegmentUtil::capsuleCapsuleManifold(const hkVector4* capsA, hkReal capsARadius, const hkVector4* capsB, hkReal capsBRadius, hkContactPoint* pointsOut)
{
    const __m128 zero = _mm_setzero_ps();
    const __m128 one = g_vectorfConstants[HK_QUADREAL_1];
    const __m128 epsQ = g_vectorfConstants[HK_QUADREAL_EPS];

    const __m128 B0 = capsB[0].m_quad;
    __m128 dB = _mm_sub_ps(capsB[1].m_quad, B0);
    __m128 dA = _mm_sub_ps(capsA[1].m_quad, capsA[0].m_quad);
    __m128 b = dot3(dB, dA);
    __m128 startA;
    __m128 endA;
    if (!(lane0(b) >= 0.0f))
    {
        // run capsule A from its other end, so that both axes point the same way
        b = _mm_sub_ps(zero, b);
        dA = negate(dA);
        startA = capsA[1].m_quad;
        endA = capsA[0].m_quad;
    }
    else
    {
        startA = capsA[0].m_quad;
        endA = capsA[1].m_quad;
    }

    const __m128 d = _mm_sub_ps(B0, startA);
    __m128 d1 = dot3(d, dA);
    __m128 d2 = dot3(d, dB);
    const __m128 a = dot3(dA, dA);
    const __m128 c = dot3(dB, dB);
    const __m128 ac = _mm_mul_ps(a, c);
    const __m128 bb = _mm_mul_ps(b, b);
    const __m128 num = _mm_sub_ps(_mm_mul_ps(c, d1), _mm_mul_ps(b, d2));
    const __m128 absDet = absolute(_mm_sub_ps(ac, bb));

    // closest points of the axes, as closestLineSegLineSeg (the parallel test uses |ac|)
    int flags;
    __m128 t;
    if (lane0(_mm_mul_ps(absDet, num)) >= lane0(_mm_mul_ps(absDet, absDet)))
    {
        t = one;
        flags = 1;
    }
    else if (!(lane0(num) > 0.0f))
    {
        t = zero;
        flags = 2;
    }
    else
    {
        const __m128 eps8 = _mm_mul_ps(epsQ, g_vectorfConstants[HK_QUADREAL_8]);
        if (lane0(absDet) > lane0(_mm_mul_ps(_mm_add_ps(absolute(ac), bb), eps8)))
        {
            t = _mm_mul_ps(reciprocal(absDet), num);
            flags = 0;
        }
        else
        {
            t = one;
            flags = 1;
        }
    }

    __m128 u;
    const __m128 uNum = _mm_sub_ps(_mm_mul_ps(t, b), d2);
    if (lane0(uNum) >= lane0(c))
    {
        u = one;
        flags = 4;
    }
    else if (!(lane0(uNum) > 0.0f))
    {
        u = zero;
        flags = 8;
    }
    else
    {
        u = _mm_mul_ps(reciprocal(c), uNum);
        goto closestDone;
    }
    {
        const __m128 tNum = _mm_add_ps(_mm_mul_ps(u, b), d1);
        if (!(lane0(tNum) > 0.0f))
        {
            t = zero;
            flags |= 2;
        }
        else if (lane0(tNum) >= lane0(a))
        {
            t = one;
            flags |= 1;
        }
        else
        {
            t = _mm_mul_ps(reciprocal(a), tNum);
        }
    }
closestDone:

    const __m128 pA = _mm_add_ps(_mm_mul_ps(t, dA), startA);
    const __m128 pB = _mm_add_ps(_mm_mul_ps(u, dB), B0);
    const __m128 diff = _mm_sub_ps(pA, pB);
    const float radiusSum = capsARadius + capsBRadius;
    const __m128 radiusSumQ = _mm_set1_ps(radiusSum);
    const __m128 tolerance = _mm_max_ps(epsQ, _mm_shuffle_ps(pointsOut[0].m_separatingNormal.m_quad, pointsOut[0].m_separatingNormal.m_quad, 0xFF));

    const __m128 dist2 = dot3(diff, diff);
    __m128 len = sqrtSetZero(dist2);
    const __m128 distance = _mm_sub_ps(len, radiusSumQ);
    pointsOut[0].m_separatingNormal.m_quad = diff;
    if (lane0(distance) >= lane0(tolerance))
    {
        // out of reach: only the normal and the distance of the first point
        pointsOut[0].m_separatingNormal.m_quad = xyzW(_mm_mul_ps(pointsOut[0].m_separatingNormal.m_quad, reciprocal(len)), distance);
        return flags;
    }

    __m128 normal;
    if (lane0(len) >= lane0(epsQ))
    {
        normal = _mm_mul_ps(reciprocal(len), diff);
    }
    else
    {
        // the axes touch: any direction perpendicular to A's axis (hkVector4Util::calculatePerpendicularVector)
        len = zero;
        const __m128 v = _mm_castsi128_ps(_mm_srli_si128(_mm_slli_si128(_mm_castps_si128(dA), 4), 4));
        const __m128 zw0x = _mm_shuffle_ps(v, v, 0xCE); // z, 0, x, 0
        const __m128 sq = _mm_mul_ps(zw0x, zw0x);
        const __m128 s1 = _mm_add_ps(_mm_shuffle_ps(sq, sq, 0x4E), sq);
        const __m128 xzLen = _mm_add_ps(_mm_shuffle_ps(s1, s1, 0xB1), s1);
        const __m128 a2 = _mm_mul_ps(dA, dA);
        const __m128 xyLen = _mm_add_ps(_mm_shuffle_ps(a2, a2, 0x55), _mm_shuffle_ps(a2, a2, 0x00));
        const __m128 useXz = _mm_cmplt_ps(xyLen, xzLen);
        const __m128 signX = _mm_castsi128_ps(_mm_insert_epi16(_mm_setzero_si128(), 0x8000, 1));
        const __m128 perpXy = _mm_xor_ps(_mm_shuffle_ps(v, v, 0xF1), signX); // -y, x, 0, 0
        const __m128 perpXz = _mm_xor_ps(signX, zw0x);                      // -z, 0, x, 0
        const __m128 perp = _mm_or_ps(_mm_andnot_ps(useXz, perpXy), _mm_and_ps(perpXz, useXz));
        normal = _mm_mul_ps(sqrtInverseSetZero(dot3(perp, perp)), perp);
    }
    pointsOut[0].m_separatingNormal.m_quad = normal;
    pointsOut[0].m_position.m_quad = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(_mm_set1_ps(capsBRadius), len), normal), pA);
    pointsOut[0].m_separatingNormal.m_quad = xyzW(pointsOut[0].m_separatingNormal.m_quad, distance);

    if (lane0(absDet) > lane0(_mm_mul_ps(_mm_mul_ps(a, g_vectorfConstants[HK_QUADREAL_INV_5]), c)))
    {
        return flags;
    }

    // nearly parallel: a point at each end of A's segment whose projection lies on B (and of B's on A), skipping
    // the ends the closest points already used
    const __m128 invAc = _mm_andnot_ps(_mm_cmpeq_ps(zero, ac), reciprocal(ac));
    const __m128 invC = _mm_mul_ps(invAc, a);
    const __m128 invA = _mm_mul_ps(invAc, c);
    __m128 projB = _mm_sub_ps(zero, d2); // B's parameter of A's end, times c
    __m128 projA = d1;                   // A's parameter of B's end, times a
    __m128 endPointA = startA;
    int skipMask = 10;
    hkContactPoint* point = pointsOut + 1;
    for (int i = 0;; i++)
    {
        if ((flags & skipMask) == 0)
        {
            bool reject = false;
            __m128 qA = endPointA;
            __m128 qB = (i == 0) ? B0 : capsB[1].m_quad;
            if (lane0(projB) > 0.0f)
            {
                if (lane0(projB) > lane0(c))
                {
                    reject = true;
                }
                else if (lane0(projA) > 0.0f)
                {
                    if (lane0(projA) > lane0(a))
                    {
                        reject = true;
                    }
                    else if (lane0(_mm_mul_ps(_mm_mul_ps(projB, projB), invC)) > lane0(_mm_mul_ps(_mm_mul_ps(projA, projA), invA)))
                    {
                        qA = _mm_add_ps(qA, _mm_mul_ps(_mm_mul_ps(projA, invA), dA));
                    }
                    else
                    {
                        qB = _mm_add_ps(qB, _mm_mul_ps(_mm_mul_ps(projB, invC), dB));
                    }
                }
                else
                {
                    qB = _mm_add_ps(qB, _mm_mul_ps(_mm_mul_ps(projB, invC), dB));
                }
            }
            else if (lane0(projA) > 0.0f)
            {
                if (lane0(projA) > lane0(a))
                {
                    reject = true;
                }
                else
                {
                    qA = _mm_add_ps(qA, _mm_mul_ps(_mm_mul_ps(projA, invA), dA));
                }
            }

            if (!reject)
            {
                const __m128 pd = _mm_sub_ps(qA, qB);
                const __m128 reach = _mm_add_ps(_mm_shuffle_ps(point->m_separatingNormal.m_quad, point->m_separatingNormal.m_quad, 0xFF), radiusSumQ);
                const __m128 pd2 = dot3(pd, pd);
                if (!(lane0(pd2) >= lane0(_mm_mul_ps(reach, reach))))
                {
                    const __m128 plen = sqrtSetZero(pd2);
                    __m128 pn;
                    if (lane0(pd2) > lane0(epsQ))
                    {
                        pn = _mm_mul_ps(sqrtInverseSetZero(pd2), pd);
                    }
                    else
                    {
                        pn = pointsOut[0].m_separatingNormal.m_quad;
                    }
                    point->m_separatingNormal.m_quad = pn;
                    point->m_position.m_quad = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(_mm_set1_ps(capsBRadius), plen), pn), qA);
                    point->m_separatingNormal.m_quad = xyzW(point->m_separatingNormal.m_quad, _mm_sub_ps(plen, radiusSumQ));
                }
            }
        }
        if (i == 1)
        {
            return flags;
        }

        // the other ends: run both axes backwards from A's end
        projB = _mm_sub_ps(_mm_sub_ps(c, projB), b);
        projA = _mm_sub_ps(_mm_sub_ps(a, projA), b);
        dA = negate(dA);
        dB = negate(dB);
        endPointA = endA;
        skipMask = 5;
        point++;
    }
}
