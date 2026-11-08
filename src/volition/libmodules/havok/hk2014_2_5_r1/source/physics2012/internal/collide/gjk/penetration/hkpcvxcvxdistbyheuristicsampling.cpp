// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/penetration/hkpcvxcvxdistbyheuristicsampling.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the last resort of the penetration depth search, from rfg.exe. It samples
// separating directions (the six axes, then rings of four around the best one with a halving step) and keeps the one
// with the largest separation. No SDK reference exists; ported from the binary.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/penetration/hkpcvxcvxdistbyheuristicsampling.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/hkpconvexshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

// size 0x30
struct hkpCvxCvxDistByHeuristicSamplingSample
{
    // 0x00ED6B60
    const hkSimdFloat32 getDistance() const
    {
        hkSimdFloat32 d;
        d.m_real = bcast<3>(m_separatingNormalA.m_quad);
        return d;
    }

    hkcdVertex m_vertexAinA; // 0x00
    hkcdVertex m_vertexBinB; // 0x10
    hkVector4f m_separatingNormalA; // 0x20 (w: the separation)
};

// 0x00ED66C0 (file static, register arguments: shapeA, aTb and direction)
// The separation of the shapes' supporting vertices along the direction (A against it, B along it); a larger one
// replaces the sample.
static void hkpCvxCvxDistByHeuristicSamplingPerformSample(const hkpConvexShape* shapeA, const hkpConvexShape* shapeB, const hkTransformf& aTb, const hkVector4f& direction, hkpCvxCvxDistByHeuristicSamplingSample& sampleOut)
{
    hkVector4f negDirection;
    negDirection.m_quad = neg(direction.m_quad);
    hkcdVertex vertexA;
    shapeA->getSupportingVertex(negDirection, vertexA);
    hkVector4f directionB;
    directionB.m_quad = rotateInverse(aTb.getRotation(), direction.m_quad);
    hkcdVertex vertexB;
    shapeB->getSupportingVertex(directionB, vertexB);
    const Q bInA = transformPos(aTb, vertexB.m_quad);
    const Q d = dot3(sub(vertexA.m_quad, bInA), direction.m_quad);
    if (greater(d, bcast<3>(sampleOut.m_separatingNormalA.m_quad)))
    {
        sampleOut.m_vertexAinA = vertexA;
        sampleOut.m_vertexBinB = vertexB;
        sampleOut.m_separatingNormalA.m_quad = setXYZ_W(direction.m_quad, d);
    }
}

// 0x00ED6830
void hkpCvxCvxDistByHeuristicSampling(const hkpConvexShape* shapeA, const hkpConvexShape* shapeB, const hkTransformf& btoa, hkcdVertex* pointAinAOut, hkcdVertex* pointBinBOut, hkpGskOut& output)
{
    const Q zero = _mm_setzero_ps();
    hkpCvxCvxDistByHeuristicSamplingSample sample;
    sample.m_vertexAinA.m_quad = zero;
    sample.m_vertexBinB.m_quad = zero;
    sample.m_separatingNormalA.m_quad = setXYZ_W(zero, g_vectorfConstants[HK_QUADREAL_MINUS_MAX]);

    // the six axis directions
    for (int axis = 0; axis < 3; axis++)
    {
        hkVector4f direction;
        direction.m_quad = g_vectorfConstants[HK_QUADREAL_1000 + axis];
        hkpCvxCvxDistByHeuristicSamplingPerformSample(shapeA, shapeB, btoa, direction, sample);
        direction.m_quad = neg(direction.m_quad);
        hkpCvxCvxDistByHeuristicSamplingPerformSample(shapeA, shapeB, btoa, direction, sample);
    }

    // rings of four directions around the best one: step 1, then 0.99 * step / 2 while above 0.001
    const Q minStep = splat(0.001f); // 0x3A83126F
    Q step = g_vectorfConstants[HK_QUADREAL_1];
    if (greater(step, minStep))
    {
        const float shrink = 0.99f; // 0x3F7D70A4
        for (;;)
        {
            const Q n = sample.m_separatingNormalA.m_quad;
            const Q s = mul(splat(shrink), step);

            // a unit direction perpendicular to n (as hkpConvexPenetrationUtil::case2To3), and n x it
            const Q noW = _mm_castsi128_ps(_mm_srli_si128(_mm_slli_si128(_mm_castps_si128(n), 4), 4));
            const Q zx = _mm_shuffle_ps(noW, noW, 0xCE);
            const Q zx2 = mul(zx, zx);
            const Q xzSum = _mm_add_ps(_mm_shuffle_ps(zx2, zx2, 0x4E), zx2);
            const Q n2 = mul(n, n);
            const Q xy = add(bcast<1>(n2), bcast<0>(n2));
            const Q useXz = _mm_cmplt_ps(xy, add(_mm_shuffle_ps(xzSum, xzSum, 0xB1), xzSum));
            const Q firstSign = _mm_castsi128_ps(_mm_insert_epi16(_mm_setzero_si128(), 0x8000, 1));
            const Q yx = _mm_xor_ps(_mm_shuffle_ps(noW, noW, 0xF1), firstSign);
            const Q zxNeg = _mm_xor_ps(firstSign, zx);
            const Q perp = _mm_or_ps(_mm_andnot_ps(useXz, yx), _mm_and_ps(zxNeg, useXz));
            const Q p = mul(invSqrt23SetZero(dot3(perp, perp)), perp);
            const Q q = mul(cross(n, p), s);
            const Q ps = mul(p, s);

            const Q d1 = add(ps, n);
            const Q d2 = sub(n, ps);
            const Q d3 = add(q, n);
            const Q d4 = sub(n, q);
            // all four normalized by the length of the first (they are equally long for a unit n)
            const Q inv = invSqrt23SetZero(dot3(d1, d1));
            hkVector4f dir1;
            dir1.m_quad = mul(inv, d1);
            hkVector4f dir2;
            dir2.m_quad = mul(inv, d2);
            hkVector4f dir3;
            dir3.m_quad = mul(inv, d3);
            hkVector4f dir4;
            dir4.m_quad = mul(inv, d4);
            hkpCvxCvxDistByHeuristicSamplingPerformSample(shapeA, shapeB, btoa, dir1, sample);
            hkpCvxCvxDistByHeuristicSamplingPerformSample(shapeA, shapeB, btoa, dir2, sample);
            hkpCvxCvxDistByHeuristicSamplingPerformSample(shapeA, shapeB, btoa, dir3, sample);
            hkpCvxCvxDistByHeuristicSamplingPerformSample(shapeA, shapeB, btoa, dir4, sample);
            step = mul(s, g_vectorfConstants[HK_QUADREAL_INV_2]);
            if (!greater(step, minStep))
            {
                break;
            }
        }
    }

    *pointAinAOut = sample.m_vertexAinA;
    *pointBinBOut = sample.m_vertexBinB;
    output.m_distance = sample.m_separatingNormalA.m_quad.m128_f32[3];
    output.m_normalInA = sample.m_separatingNormalA;
    output.m_pointAinA = sample.m_vertexAinA;
}
