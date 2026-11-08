// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/hkpcollidetriangleutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): point-triangle helpers of the triangle agents, from rfg.exe. No SDK reference
// exists for the .cpp; ported from the binary. setupClosestPointTriangleCache sums on the x87 FPU; the sums are
// reproduced in double (as the other x87 code of the port).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/hkpcollidetriangleutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/collide/linesegment/hklinesegmentutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

// 0x0135B414: the edge (0..2) whose plane alone the point is outside of (bit i of the mask: outside edge i), or the
// vertex outside two of them as vertex - 8 (-6: vertex 2, -7: vertex 1, -8: vertex 0); -1 for none or all three.
const signed char hkpCollideTriangleUtil::maskToIndex[] = {-1, 0, 1, -6, 2, -7, -8, -1};

// 0x0135B41C: the previous (i) and next (i + 2) vertex of vertex i. The fifth entry is the zero padding after the
// table, which closestPointTriangle reads for vertex 2 (vertexToEdgeLut[2 + 2]).
const signed char hkpCollideTriangleUtil::vertexToEdgeLut[] = {2, 0, 1, 2, 0};

// 0x00DD1C30
// Q = t0 - t1 and R = t2 - t1: (QQ, RR, QR) / (QQ RR - QR^2) and 1 / |Q x R|.
void hkpCollideTriangleUtil::setupClosestPointTriangleCache(const hkVector4f* tri, ClosestPointTriangleCache& cache)
{
    const Q q = sub(tri[0].m_quad, tri[1].m_quad);
    const Q r = sub(tri[2].m_quad, tri[1].m_quad);
    const float* qf = q.m128_f32;
    const float* rf = r.m128_f32;
    // each product rounded to float, the sums of y + x + z in double (x87)
    const double qq = ((double)(float)((double)qf[1] * qf[1]) + (double)(float)((double)qf[0] * qf[0])) + (double)(float)((double)qf[2] * qf[2]);
    const double rr = ((double)(float)((double)rf[1] * rf[1]) + (double)(float)((double)rf[0] * rf[0])) + (double)(float)((double)rf[2] * rf[2]);
    const double qr = ((double)(float)((double)rf[1] * qf[1]) + (double)(float)((double)rf[0] * qf[0])) + (double)(float)((double)rf[2] * qf[2]);
    const float qqf = (float)qq;
    const float rrf = (float)rr;
    const float qrf = (float)qr;
    const float det = (float)((double)(float)(qq * rr) - (double)(float)(qr * qr));
    const float inv = 1.0f / det;
    cache.m_QQ = inv * qqf;
    cache.m_RR = inv * rrf;
    cache.m_QR = inv * qrf;
    const Q n = cross(q, r);
    cache.m_invTriNormal = lane0(_mm_div_ps(g_vectorfConstants[HK_QUADREAL_1], _mm_sqrt_ps(dot3(n, n))));
}

// 0x00DD1D90
// 1 / |edge| of edges (t2 - t1, t0 - t2, t1 - t0), 1 / |normal| and |normal| (normal = (t2 - t1) x (t0 - t2)).
void hkpCollideTriangleUtil::setupPointTriangleDistanceCache(const hkVector4f* tri, PointTriangleDistanceCache& cache)
{
    const Q t0 = tri[0].m_quad;
    const Q t1 = tri[1].m_quad;
    const Q t2 = tri[2].m_quad;
    const Q e1 = sub(t2, t1);
    const Q e0 = sub(t1, t0);
    const Q e2 = sub(t0, t2);
    const Q n = cross(e1, e2);
    hkVector4f r0;
    r0.m_quad = mul(e1, e1);
    hkVector4f r1;
    r1.m_quad = mul(e2, e2);
    hkVector4f r2;
    r2.m_quad = mul(e0, e0);
    hkVector4f r3;
    r3.m_quad = mul(n, n);
    HK_TRANSPOSE4f(r0, r1, r2, r3);
    const Q len2 = add(add(r0.m_quad, r1.m_quad), r2.m_quad);
    const Q r = _mm_rsqrt_ps(len2);
    const Q inv = mul(sub(splat(3.0f), mul(mul(len2, r), r)), mul(r, splat(0.5f)));
    _mm_storeu_ps(cache.m_invEdgeLen, inv);
    cache.m_normalLen = lane0(mul(bcast<3>(len2), bcast<3>(inv)));
}

// 0x00DD1E40
// The triangle's unit normal and the planes through its edges (outward unit directions, edge x normal) as the rows
// of planeEquationsOut, the fourth row (0, 0, 0, -|normal| / |edge 1|).
void hkpCollideTriangleUtil::calcTrianglePlaneDirections(const hkVector4f* tri, const PointTriangleDistanceCache& cache, hkTransformf& planeEquationsOut, hkVector4f& normalOut)
{
    const Q t0 = tri[0].m_quad;
    const Q t1 = tri[1].m_quad;
    const Q t2 = tri[2].m_quad;
    const Q e1 = sub(t2, t1);
    const Q e0 = sub(t1, t0);
    const Q normalLen = splat(cache.m_normalLen);
    const Q e2 = sub(t0, t2);
    const Q inv = _mm_loadu_ps(cache.m_invEdgeLen);
    const Q normal = mul(cross(e1, e2), bcast<3>(inv));
    normalOut.m_quad = normal;
    const Q d1 = mul(bcast<1>(inv), e2);
    const Q invX = bcast<0>(inv);
    const Q d2 = mul(bcast<2>(inv), e0);
    const Q d0 = mul(invX, e1);
    hkVector4f c0;
    c0.m_quad = cross(d0, normal);
    hkVector4f c1;
    c1.m_quad = cross(d1, normal);
    hkVector4f c2;
    c2.m_quad = cross(d2, normal);
    hkVector4f c3;
    c3.m_quad = normal;
    HK_TRANSPOSE4f(c0, c1, c2, c3);
    planeEquationsOut.m_rotation.setCols(c0, c1, c2);
    planeEquationsOut.m_translation.m_quad = setXYZ_W(_mm_setzero_ps(), mul(sub(_mm_setzero_ps(), invX), normalLen));
}

// 0x00DD1F90
// The closest point of the triangle to position: the face when the barycentric test (with 0.001 slack) passes, else
// the nearest edge (one for an edge region, the nearer of two for a vertex region). result: the direction from the
// position to the point (unit) and the distance; featureOutput: the vertices of the feature.
hkpCollideTriangleUtil::ClosestPointTriangleStatus hkpCollideTriangleUtil::closestPointTriangle(const hkVector4f& position, const hkVector4f* tri, const ClosestPointTriangleCache& cache, ClosestPointTriangleResult& result, hkpFeatureOutput* featureOutput)
{
    if (featureOutput != nullptr)
    {
        featureOutput->numFeatures = 0;
    }
    const Q t1 = tri[1].m_quad;
    const Q rel = sub(t1, position.m_quad);
    const Q q = sub(tri[0].m_quad, t1);
    const float a = lane0(dot3(q, rel));
    const Q r = sub(tri[2].m_quad, t1);
    const float b = lane0(dot3(r, rel));
    const float s = cache.m_QR * a - cache.m_QQ * b;
    const float t = cache.m_QR * b - cache.m_RR * a;
    const float u = (1.001f - t) - s; // 0x3F8020C5
    const Q tests = _mm_unpacklo_ps(_mm_unpacklo_ps(_mm_set_ss(t + 0.001f), _mm_set_ss(s + 0.001f)), _mm_unpacklo_ps(_mm_set_ss(u), _mm_set_ss(0.001f)));
    const int mask = _mm_movemask_ps(_mm_cmplt_ps(_mm_setzero_ps(), tests)) & 7;
    if (mask == 7)
    {
        const Q n = mul(splat(cache.m_invTriNormal), cross(q, r));
        result.hitDirection.m_quad = n;
        const Q d = dot3(n, rel);
        Q distance;
        if (greater(d, _mm_setzero_ps()))
        {
            result.hitDirection.m_quad = neg(n);
            distance = d;
        }
        else
        {
            distance = sub(_mm_setzero_ps(), d);
        }
        result.distance = lane0(distance);
        if (featureOutput != nullptr)
        {
            featureOutput->numFeatures = 3;
            featureOutput->featureIds[0] = 0;
            featureOutput->featureIds[1] = 1;
            featureOutput->featureIds[2] = 2;
        }
        return HIT_TRIANGLE_FACE;
    }

    const int idx = maskToIndex[mask];
    if (idx < 0)
    {
        // outside two edge planes: the edge of the vertices before and after vertex idx + 8
        const int v = idx + 8;
        const int prev = vertexToEdgeLut[v];
        const int next = vertexToEdgeLut[v + 2];
        hkLineSegmentUtil::ClosestPointLineSegResult segment;
        const int where = hkLineSegmentUtil::closestPointLineSeg(position, tri[next], tri[prev], segment);
        if (featureOutput != nullptr)
        {
            if (where == 4)
            {
                featureOutput->featureIds[0] = (unsigned short)prev;
                featureOutput->numFeatures = 1;
            }
            else if (where == 8)
            {
                featureOutput->featureIds[0] = (unsigned short)next;
                featureOutput->numFeatures = 1;
            }
            else
            {
                featureOutput->featureIds[0] = (unsigned short)prev;
                featureOutput->featureIds[1] = (unsigned short)next;
                featureOutput->numFeatures = 2;
            }
        }
        const Q d = sub(position.m_quad, segment.m_pointOnEdge.m_quad);
        const Q len2 = dot3(d, d);
        result.hitDirection.m_quad = d;
        const Q inv = invSqrt23SetZero(len2);
        result.hitDirection.m_quad = mul(d, inv);
        result.distance = lane0(mul(inv, len2));
        return HIT_TRIANGLE_EDGE;
    }

    // outside one edge plane: the nearer of the two edges at vertex idx
    const int next = vertexToEdgeLut[idx + 2];
    const int prev = vertexToEdgeLut[idx];
    hkLineSegmentUtil::ClosestPointLineSegResult segment1;
    const int where1 = hkLineSegmentUtil::closestPointLineSeg(position, tri[idx], tri[next], segment1);
    hkLineSegmentUtil::ClosestPointLineSegResult segment2;
    const int where2 = hkLineSegmentUtil::closestPointLineSeg(position, tri[prev], tri[idx], segment2);
    const Q d1 = sub(position.m_quad, segment1.m_pointOnEdge.m_quad);
    const Q d2 = sub(position.m_quad, segment2.m_pointOnEdge.m_quad);
    const Q l1 = dot3(d1, d1);
    const Q l2 = dot3(d2, d2);
    if (lessOrNaN(l1, l2))
    {
        const Q inv = invSqrt23SetZero(l1);
        if (featureOutput != nullptr)
        {
            // as in the original: 4 (the second end) names vertex idx, the first end of this segment
            if (where1 == 4)
            {
                featureOutput->numFeatures = 1;
                featureOutput->featureIds[0] = (unsigned short)idx;
            }
            else if (where1 == 8)
            {
                featureOutput->numFeatures = 1;
                featureOutput->featureIds[0] = (unsigned short)next;
            }
            else
            {
                featureOutput->numFeatures = 2;
                featureOutput->featureIds[0] = (unsigned short)idx;
                featureOutput->featureIds[1] = (unsigned short)next;
            }
        }
        result.distance = lane0(mul(inv, l1));
        result.hitDirection.m_quad = mul(inv, d1);
        return HIT_TRIANGLE_EDGE;
    }
    const Q inv = invSqrt23SetZero(l2);
    if (featureOutput != nullptr)
    {
        if (where2 == 4)
        {
            featureOutput->numFeatures = 1;
            featureOutput->featureIds[0] = (unsigned short)idx;
        }
        else if (where2 == 8)
        {
            featureOutput->numFeatures = 1;
            featureOutput->featureIds[0] = (unsigned short)prev;
        }
        else
        {
            featureOutput->numFeatures = 2;
            featureOutput->featureIds[0] = (unsigned short)idx;
            featureOutput->featureIds[1] = (unsigned short)prev;
        }
    }
    result.distance = lane0(mul(inv, l2));
    result.hitDirection.m_quad = mul(inv, d2);
    return HIT_TRIANGLE_EDGE;
}
