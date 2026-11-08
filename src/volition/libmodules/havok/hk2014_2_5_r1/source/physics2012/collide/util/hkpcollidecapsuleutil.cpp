// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/hkpcollidecapsuleutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): capsule closest points for the capsule agents, from rfg.exe. No SDK source
// exists for 2014.2.5; ported from the binary.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/hkpcollidecapsuleutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/collide/linesegment/hklinesegmentutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

// 0x00DD26F0
// The closest points of the two axes (the segment parameters clamped in turn, branch free; nearly parallel axes take
// A's end), then the point on B's surface and the normal from B to A (w: the distance less the radii). Axes that
// touch take the normal of the axes' plane, or a direction perpendicular to A's axis. HK_FAILURE (cpoint unchanged)
// beyond the tolerance.
hkResult hkCollideCapsuleUtilClostestPointCapsVsCaps(const hkVector4f* capsA, const float& capsARadius, const hkVector4f* capsB, const float& capsBRadius, float collisionTolerance, hkContactPoint& cpoint)
{
    const Q zero = _mm_setzero_ps();
    const Q one = g_vectorfConstants[HK_QUADREAL_1];
    const Q eps = g_vectorfConstants[HK_QUADREAL_EPS];
    const Q tolerance = splat(collisionTolerance);
    const Q radiusB = splat(capsBRadius);
    const Q radiusA = splat(capsARadius);
    const Q a0 = capsA[0].m_quad;
    const Q dA = sub(capsA[1].m_quad, a0);
    const Q b0 = capsB[0].m_quad;
    const Q dB = sub(capsB[1].m_quad, b0);
    const Q d = sub(b0, a0);
    const Q b = dot3(dB, dA);
    const Q e = dot3(d, dA);
    const Q f = dot3(d, dB);
    const Q a = dot3(dA, dA);
    const Q c = dot3(dB, dB);
    const Q det = _mm_max_ps(sub(mul(c, a), mul(b, b)), eps);
    const Q invDet = recip23(det);
    const Q invC = recip23(c);
    const Q invA = recip23(a);

    // t on A (1 when parallel), u on B for it, then t again for u
    const Q num = sub(mul(c, e), mul(f, b));
    const Q parallel = _mm_cmple_ps(det, eps);
    Q t = mul(_mm_max_ps(zero, selectQ(_mm_cmplt_ps(num, det), num, det)), invDet);
    t = selectQ(parallel, one, t);
    Q u = sub(mul(t, mul(invC, b)), mul(invC, f));
    u = _mm_max_ps(zero, selectQ(_mm_cmplt_ps(u, one), u, one));
    t = add(mul(mul(invA, b), u), mul(invA, e));
    t = _mm_max_ps(zero, selectQ(_mm_cmplt_ps(t, one), t, one));

    const Q pA = add(mul(t, dA), a0);
    const Q pB = add(mul(u, dB), b0);
    const Q diff = sub(pA, pB);
    const Q dist2 = dot3(diff, diff);
    const Q radiusSum = add(radiusA, radiusB);
    const Q limit = add(radiusSum, tolerance);
    if (greater(dist2, mul(limit, limit)))
    {
        return HK_FAILURE;
    }

    const Q length = _mm_andnot_ps(_mm_cmple_ps(dist2, zero), mul(invSqrt23(dist2), dist2));
    Q n;
    if (greater(dist2, zero))
    {
        n = diff;
    }
    else
    {
        const Q axesNormal = cross(dA, dB);
        if (greater(dot3(axesNormal, axesNormal), eps))
        {
            n = _mm_xor_ps(signBits(_mm_cmplt_ps(dot3(axesNormal, diff), zero)), axesNormal);
        }
        else
        {
            n = perpendicular(dA);
        }
    }
    const Q normal = setXYZ_W(mul(invSqrt23SetZero(dot3(n, n)), n), sub(length, radiusSum));
    cpoint.m_position.m_quad = add(mul(sub(radiusB, length), normal), pA);
    cpoint.m_separatingNormal.m_quad = normal;
    return HK_SUCCESS;
}

// One lane of v in all four (the original's mask-and-or reduction, bit exact).
static inline Q hkCollideCapsuleUtil_lane(Q v, int i)
{
    alignas(16) float f[4];
    _mm_store_ps(f, v);
    return _mm_set1_ps(f[i]);
}

static inline int hkCollideCapsuleUtil_mask(Q m)
{
    return _mm_movemask_ps(m);
}

// The per-edge closest points of the capsule axis and the triangle edges (closestLineSegLineSeg) that the endpoint
// mask leaves outside: each closer one than *cur's distance replaces *cur. In combined mode (no manifold search, one
// point) an axis endpoint closest point moves the feature output to the second slot; otherwise it is skipped (the
// endpoint is handled on its own).
static void hkCollideCapsuleUtil_edges(const hkVector4f* capsAPoints, Q dA, const hkVector4f* triVertices, float triBRadius, Q radiusSum,
                                       const Q* edgeNormalCols, unsigned int insideMask, int endpoint, bool combined,
                                       hkContactPoint* cur, hkpFeatureOutput* featuresOut)
{
    int featureIndex = endpoint;
    int endpointBit = 1 << endpoint;
    unsigned int edgeBit = 1;
    for (int e = 0; e < 3; e++, edgeBit <<= 1)
    {
        if (insideMask & edgeBit)
        {
            continue;
        }
        const int a = hkpCollideTriangleUtil::vertexToEdgeLut[e + 2];
        const int b = hkpCollideTriangleUtil::vertexToEdgeLut[e];
        hkVector4f dB;
        dB.m_quad = sub(triVertices[b].m_quad, triVertices[a].m_quad);
        hkVector4f dAv;
        dAv.m_quad = dA;
        hkLineSegmentUtil::ClosestLineSegLineSegResult result;
        const int ret = hkLineSegmentUtil::closestLineSegLineSeg(capsAPoints[0], dAv, triVertices[a], dB, result);
        if (featuresOut)
        {
            featuresOut[featureIndex].featureIds[0] = (unsigned short)a;
            if (ret & 0xC)
            {
                featuresOut[featureIndex].numFeatures = 1;
            }
            else
            {
                featuresOut[featureIndex].numFeatures = 2;
                featuresOut[featureIndex].featureIds[1] = (unsigned short)b;
            }
        }
        const Q limit = add(bcast<3>(cur->m_separatingNormal.m_quad), radiusSum);
        if (!lessOrNaN(splat(result.m_distanceSquared.val()), mul(limit, limit)))
        {
            continue;
        }
        if (ret & endpointBit)
        {
            if (!combined)
            {
                continue;
            }
            featureIndex = 1;
            endpointBit = 2;
        }
        const Q aMinusB = result.m_closestAminusClosestB.m_quad;
        const Q epsSqrd = g_vectorfConstants[HK_QUADREAL_EPS_SQRD];
        Q n;
        Q ref;
        if (ret == 0)
        {
            n = cross(dA, dB.m_quad);
            ref = aMinusB;
        }
        else if (greater(splat(result.m_distanceSquared.val()), epsSqrd))
        {
            n = aMinusB;
            ref = aMinusB;
        }
        else
        {
            // the edge's plane normal (w 0), unless the axes give a direction
            const Q edgeNormal = _mm_setr_ps(lane0(hkCollideCapsuleUtil_lane(edgeNormalCols[0], e)), lane0(hkCollideCapsuleUtil_lane(edgeNormalCols[1], e)), lane0(hkCollideCapsuleUtil_lane(edgeNormalCols[2], e)), 0.0f);
            const Q axesNormal = cross(dA, dB.m_quad);
            ref = edgeNormal;
            n = greater(dot3(axesNormal, axesNormal), epsSqrd) ? axesNormal : edgeNormal;
        }
        n = mul(n, invSqrt23SetZero(dot3(n, n)));
        const Q d = dot3(ref, n);
        const Q s = signBits(d);
        const Q absD = _mm_xor_ps(d, s);
        const Q flipped = _mm_xor_ps(s, n);
        cur->m_position.m_quad = add(mul(sub(splat(triBRadius), absD), flipped), result.m_closestPointA.m_quad);
        cur->m_separatingNormal.m_quad = setXYZ_W(flipped, sub(absD, radiusSum));
    }
}

// 0x00DD2BC0
// The capsule axis in the triangle's plane space: (x, y, z) the distances to the three edge planes (positive
// outside), w the distance to the triangle plane. Out early (only the points' distances set) when both endpoints are
// beyond one edge plane or below the triangle by more than the radii and tolerance. An axis that pierces the triangle
// gives one point (points[1]): the deepest endpoint inside, else the closest edge crossing. Otherwise each endpoint
// over the face gives its face point, and the edges outside an endpoint their segment closest points; without a
// manifold search the edges both endpoints leave give one point. With a manifold search (endpoints not both over the
// face) each edge outside both endpoints runs capsuleCapsuleManifold into the points, keeping the deepest first point.
// Original behaviour kept: the manifold search leaves the last edge's second and third points.
void hkCollideCapsuleUtilCapsVsTri(const hkVector4f* capsAPoints, float capsARadius, const hkVector4f* triVertices, float triBRadius, const hkpCollideTriangleUtil::PointTriangleDistanceCache& cache, float collisionTolerance, int searchManifold, hkContactPoint* points, hkpFeatureOutput* featuresOut)
{
    if (featuresOut)
    {
        featuresOut[0].numFeatures = 0;
        featuresOut[1].numFeatures = 0;
        featuresOut[2].numFeatures = 0;
    }
    const Q zero = _mm_setzero_ps();
    const Q v0 = triVertices[0].m_quad;
    const Q v1 = triVertices[1].m_quad;
    const Q v2 = triVertices[2].m_quad;
    const Q e1 = sub(v2, v1);
    const Q e0 = sub(v1, v0);
    const Q e2 = sub(v0, v2);
    const Q c = _mm_loadu_ps(cache.m_invEdgeLen);
    const Q N = mul(cross(e1, e2), bcast<3>(c));
    Q na = cross(mul(bcast<0>(c), e1), N);
    Q nb = cross(mul(bcast<1>(c), e2), N);
    Q nc = cross(mul(bcast<2>(c), e0), N);
    Q nn = N;
    _MM_TRANSPOSE4_PS(na, nb, nc, nn);
    const Q cols[3] = { na, nb, nc }; // x, y, z of the edge normals (and N in w)
    const Q offset = _mm_move_ss(zero, mul(sub(zero, bcast<0>(c)), splat(cache.m_normalLen)));

    const Q c0 = sub(capsAPoints[0].m_quad, v0);
    const Q c1 = sub(capsAPoints[1].m_quad, v0);
    const Q p0 = add(add(add(mul(bcast<1>(c0), cols[1]), mul(bcast<0>(c0), cols[0])), mul(bcast<2>(c0), cols[2])), offset);
    const Q p1 = add(add(add(mul(bcast<1>(c1), cols[1]), mul(bcast<0>(c1), cols[0])), mul(bcast<2>(c1), cols[2])), offset);

    const float radiusSumF = capsARadius + triBRadius;
    const float radTolF = radiusSumF + collisionTolerance;
    const Q radiusSum = splat(radiusSumF);
    const Q radTol = splat(radTolF);
    const Q big = bits(0x5F7FFFF0); // 1.8446726e19 (about 2^64)
    const Q initDist = sub(big, add(radiusSum, radiusSum));
    for (int i = 0; i < 3; i++)
    {
        points[i].m_separatingNormal.m_quad = setXYZ_W(points[i].m_separatingNormal.m_quad, initDist);
    }

    const Q above = _mm_and_ps(_mm_cmplt_ps(radTol, p0), _mm_cmplt_ps(radTol, p1));
    const Q negRadTol = neg(radTol);
    const Q below = _mm_and_ps(_mm_cmplt_ps(p1, negRadTol), _mm_cmplt_ps(p0, negRadTol));
    if ((hkCollideCapsuleUtil_mask(below) & 8) | hkCollideCapsuleUtil_mask(above))
    {
        return;
    }

    const unsigned int neg0 = (unsigned int)hkCollideCapsuleUtil_mask(_mm_cmplt_ps(p0, zero));
    const unsigned int neg1 = (unsigned int)hkCollideCapsuleUtil_mask(_mm_cmplt_ps(p1, zero));
    const unsigned int negs[2] = { neg0, neg1 };
    if ((neg0 ^ neg1) & 8)
    {
        // the endpoints on both sides of the plane: where the axis crosses it
        const Q w0 = bcast<3>(p0);
        const Q w1 = bcast<3>(p1);
        const Q dir = sub(p1, p0);
        const Q crossing = add(mul(mul(recip23(sub(w0, w1)), w0), dir), p0);
        if ((hkCollideCapsuleUtil_mask(_mm_cmplt_ps(crossing, zero)) & 7) == 7)
        {
            hkContactPoint& cp = points[1];
            cp.m_separatingNormal.m_quad = setXYZ_W(cp.m_separatingNormal.m_quad, g_vectorfConstants[HK_QUADREAL_MINUS_MAX]);
            const Q pts[2] = { p0, p1 };
            for (int i = 0; i < 2; i++)
            {
                if ((negs[i] & 7) != 7)
                {
                    continue;
                }
                const Q w = bcast<3>(pts[i]);
                const Q dist = sub(sub(zero, abs(w)), radiusSum);
                if (!greater(dist, bcast<3>(cp.m_separatingNormal.m_quad)))
                {
                    continue;
                }
                if (negs[i] & 8)
                {
                    cp.m_separatingNormal.m_quad = N;
                    cp.m_position.m_quad = add(mul(sub(splat(triBRadius), w), N), capsAPoints[i].m_quad);
                }
                else
                {
                    cp.m_position.m_quad = add(mul(sub(sub(zero, splat(triBRadius)), w), N), capsAPoints[i].m_quad);
                    cp.m_separatingNormal.m_quad = neg(N);
                }
                cp.m_separatingNormal.m_quad = setXYZ_W(cp.m_separatingNormal.m_quad, dist);
            }

            // the axis against each edge line in the plane space
            const Q dw = bcast<3>(dir);
            const Q dw2 = mul(dw, dw);
            for (int e = 0; e < 3; e++)
            {
                const Q di = hkCollideCapsuleUtil_lane(dir, e);
                const Q inv = recip23(add(mul(di, di), dw2));
                const Q p0e = hkCollideCapsuleUtil_lane(p0, e);
                const Q num = sub(mul(di, w0), mul(p0e, dw));
                const Q dist2 = mul(mul(num, num), inv);
                const Q limit = add(bcast<3>(cp.m_separatingNormal.m_quad), radiusSum);
                if (!lessOrNaN(dist2, mul(limit, limit)))
                {
                    continue;
                }
                const Q u = mul(add(mul(p0e, di), mul(dw, w0)), sub(zero, inv));
                // within the axis, with a little slack: (-1e-4, 1.0001)
                if (!greater(u, bits(0xB8D1B717)) || !lessOrNaN(u, bits(0x3F800347)))
                {
                    continue;
                }
                const Q s = signBits(_mm_cmplt_ps(dw, zero));
                const Q edgeNormal = _mm_setr_ps(lane0(hkCollideCapsuleUtil_lane(cols[0], e)), lane0(hkCollideCapsuleUtil_lane(cols[1], e)), lane0(hkCollideCapsuleUtil_lane(cols[2], e)), 0.0f);
                Q n = add(mul(sub(zero, _mm_xor_ps(di, s)), N), mul(edgeNormal, _mm_xor_ps(dw, s)));
                n = mul(invSqrt23SetZero(dot3(n, n)), n);
                cp.m_separatingNormal.m_quad = n;
                const Q axisPoint = add(mul(sub(capsAPoints[1].m_quad, capsAPoints[0].m_quad), u), capsAPoints[0].m_quad);
                const Q dist = _mm_andnot_ps(_mm_cmple_ps(dist2, zero), mul(invSqrt23(dist2), dist2));
                const Q negDist = sub(zero, dist);
                cp.m_position.m_quad = add(mul(sub(splat(triBRadius), negDist), n), axisPoint);
                cp.m_separatingNormal.m_quad = setXYZ_W(n, sub(negDist, radiusSum));
            }
            if (featuresOut)
            {
                for (int i = 0; i < 3; i++)
                {
                    featuresOut[i].numFeatures = 3;
                    featuresOut[i].featureIds[0] = 0;
                    featuresOut[i].featureIds[1] = 1;
                    featuresOut[i].featureIds[2] = 2;
                }
            }
            return;
        }
    }

    const unsigned int combined = neg0 | neg1;
    const Q dA = sub(capsAPoints[1].m_quad, capsAPoints[0].m_quad);
    if ((combined & 7) != 7)
    {
        if (searchManifold)
        {
            const Q tol = splat(collisionTolerance);
            for (int i = 0; i < 3; i++)
            {
                points[i].m_separatingNormal.m_quad = setXYZ_W(points[i].m_separatingNormal.m_quad, tol);
            }
            Q bestPosition = g_vectorfConstants[HK_QUADREAL_0];
            Q bestNormal = setXYZ_W(g_vectorfConstants[HK_QUADREAL_0], g_vectorfConstants[HK_QUADREAL_MAX]);
            unsigned int edgeBit = 1;
            for (int e = 0; e < 3; e++, edgeBit <<= 1)
            {
                if (combined & edgeBit)
                {
                    continue;
                }
                const int a = hkpCollideTriangleUtil::vertexToEdgeLut[e + 2];
                const int b = hkpCollideTriangleUtil::vertexToEdgeLut[e];
                alignas(16) hkVector4f segment[2];
                segment[0] = triVertices[a];
                segment[1] = triVertices[b];
                int ret = hkLineSegmentUtil::capsuleCapsuleManifold(capsAPoints, capsARadius, segment, triBRadius, points);
                if (featuresOut)
                {
                    ret &= 0xC;
                    for (int i = 0; i < 3; i++)
                    {
                        featuresOut[i].featureIds[0] = (unsigned short)a;
                        if (ret)
                        {
                            featuresOut[i].numFeatures = 1;
                        }
                        else
                        {
                            featuresOut[i].numFeatures = 2;
                            featuresOut[i].featureIds[1] = (unsigned short)b;
                        }
                    }
                }
                if (greater(bcast<3>(points[0].m_separatingNormal.m_quad), bcast<3>(bestNormal)))
                {
                    points[0].m_position.m_quad = bestPosition;
                    points[0].m_separatingNormal.m_quad = bestNormal;
                }
                else
                {
                    bestPosition = points[0].m_position.m_quad;
                    bestNormal = points[0].m_separatingNormal.m_quad;
                }
            }
            return;
        }
        hkCollideCapsuleUtil_edges(capsAPoints, dA, triVertices, triBRadius, radiusSum, cols, neg0 & neg1, 0, true, points, featuresOut);
        return;
    }

    // each endpoint: its face point, or the edges outside it
    hkContactPoint* cur = points;
    const Q pts[2] = { p0, p1 };
    for (int i = 0; i < 2; i++, cur++)
    {
        if ((negs[i] & 7) != 7)
        {
            hkCollideCapsuleUtil_edges(capsAPoints, dA, triVertices, triBRadius, radiusSum, cols, negs[i], i, false, cur, featuresOut);
            continue;
        }
        const Q w = bcast<3>(pts[i]);
        if (negs[i] & 8)
        {
            cur->m_position.m_quad = add(mul(sub(sub(zero, splat(triBRadius)), w), N), capsAPoints[i].m_quad);
            cur->m_separatingNormal.m_quad = setXYZ_W(neg(N), sub(sub(zero, w), radiusSum));
        }
        else
        {
            cur->m_position.m_quad = add(mul(sub(splat(triBRadius), w), N), capsAPoints[i].m_quad);
            cur->m_separatingNormal.m_quad = setXYZ_W(N, sub(w, radiusSum));
        }
        if (featuresOut)
        {
            featuresOut[i].featureIds[0] = 0;
            featuresOut[i].featureIds[1] = 1;
            featuresOut[i].numFeatures = 3;
            featuresOut[i].featureIds[2] = 2;
        }
    }
}
