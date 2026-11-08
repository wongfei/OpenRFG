// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgsk.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the GSK (GJK with support features) closest-feature search of the physics2012
// agents, from rfg.exe. No SDK reference exists for this file; everything is ported from the binary's instruction
// order (see hkpgskexact.h).
//
// The search keeps a simplex of m_dimA vertices of A (m_verticesA, in A's space) and m_dimB vertices of B
// (m_verticesBinB, in B's space; m_verticesBinA are the same in A's space). m_dummy holds A's vertices "in A's own
// space" for the functions that treat A and B alike (processEdgeTriangle and checkTriangleBoundaries address
// vertices + 4 as the second copy). Each iteration reduces the simplex to the closest feature pair (reduceDimension),
// which sets m_support (the direction from B to A), then adds a new supporting vertex of A (along -support) or of B
// (along support), alternating which shape goes first.
//
// The shipped build keeps a few out-of-line copies of inlined code with no caller (process_point_edge,
// hkGskRecalcContact_pointTriangle/_pointEdge/_edgeEdge, hkpGsk::reduceDimension): reduceDimension and
// process_point_edge are written once and used by the functions that inline them; the hkGskRecalcContact_* copies are
// inlined into hkGskRecalcContactInternal.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgsk.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4fcomparison.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4util.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/hkpconvexshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/hkpcollidetriangleutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/penetration/hkpconvexpenetrationutil.h"

using namespace hkGskExact;

// hkpCollideTriangleUtil tables (hkpcollidetriangleutil.cpp): the triangle edge outside the three boundary planes
// (or, as vertex - 8, the vertex outside two of them; -1 for none or all), and the edge vertex lookup.
static inline int hkGsk_maskToIndex(int mask) { return hkpCollideTriangleUtil::maskToIndex[mask]; }
static inline int hkGsk_edgeVertex(int i) { return hkpCollideTriangleUtil::vertexToEdgeLut[i]; }

// 0x00E7E380
void hkpGsk::print()
{
}

// 0x00E7E3A0
void hkpGsk::codeBegin()
{
}

// 0x00DC56F0 (.h:239)
void hkpGsk::init(const hkpConvexShape* shapeA, const hkpConvexShape* shapeB, const hkpGskCache& cache)
{
    m_dimA = cache.m_dimA;
    m_dimB = cache.m_dimB;
    m_maxDimA = cache.m_maxDimA;
    m_maxDimB = cache.m_maxDimB;
    m_featureChange = 0;
    shapeA->convertVertexIdsToVertices(cache.m_vertices, m_dimA, m_verticesA);
    shapeB->convertVertexIdsToVertices(cache.m_vertices + m_dimA, m_dimB, m_verticesBinB);
}

// 0x00E7E4A0 (findClosestTriangleBackup, file static)
// The triangle of the tetrahedron whose plane the point is in front of when findClosestTriangle cannot decide: for
// each face with a positive old dot, the two edge neighbours are compared and the farther one's dot is replaced by -1.
// Returns the first face still positive, -1 for none.
static int findClosestTriangleBackup(const hkVector4f& a0, const hkVector4f* tetra, hkVector4f& oldDots)
{
    const Q minusOne = g_vectorfConstants[HK_QUADREAL_MINUS1];
    for (int i = 0; i < 3; i++)
    {
        const int e2 = hkGsk_edgeVertex(i + 2);
        const int e0 = hkGsk_edgeVertex(i);
        const Q mask2 = laneMask(1 << e2);
        Q v2 = _mm_and_ps(oldDots.m_quad, mask2);
        v2 = _mm_or_ps(_mm_shuffle_ps(v2, v2, 0x4E), v2);
        v2 = _mm_or_ps(_mm_shuffle_ps(v2, v2, 0xB1), v2);
        if (lessOrNaN(v2, _mm_setzero_ps()))
        {
            continue;
        }
        const Q mask0 = laneMask(1 << e0);
        Q v0 = _mm_and_ps(mask0, oldDots.m_quad);
        v0 = _mm_or_ps(_mm_shuffle_ps(v0, v0, 0x4E), v0);
        v0 = _mm_or_ps(_mm_shuffle_ps(v0, v0, 0xB1), v0);
        if (lessOrNaN(v0, _mm_setzero_ps()))
        {
            continue;
        }
        const Q t = tetra[i].m_quad;
        const Q a = sub(a0.m_quad, t);
        const Q d0 = sub(tetra[e0].m_quad, t);
        const Q d3 = sub(tetra[3].m_quad, t);
        const Q d2 = sub(tetra[e2].m_quad, t);
        const Q det = sub(mul(dot3(d3, d0), dot3(a, d2)), mul(hadd3(mul(d3, d2)), hadd3(mul(a, d0))));
        if (greaterEqual(det, _mm_setzero_ps()))
        {
            oldDots.m_quad = _mm_or_ps(_mm_andnot_ps(mask2, oldDots.m_quad), _mm_and_ps(mask2, minusOne));
        }
        else
        {
            oldDots.m_quad = _mm_or_ps(_mm_andnot_ps(mask0, oldDots.m_quad), _mm_and_ps(mask0, minusOne));
        }
    }
    const int mask = _mm_movemask_ps(_mm_cmplt_ps(_mm_setzero_ps(), oldDots.m_quad));
    if (mask & 7)
    {
        return hkVector4Comparison_maskToFirstIndex[mask];
    }
    return -1;
}

// 0x00E7E660
// The point against the three edge planes of triangle b (perpendicular to the triangle through each edge): bit i of
// the result is set where the point is outside edge i (m_checkTriangleDots holds the dots: edge (1,2), (2,0), (0,1)).
// With a support type the triangle normal becomes m_support; a point inside all three on the back side of A's first
// vertex flips the triangle (vertices 0 and 1 swapped in both spaces) and marks a feature change.
int hkpGsk::checkTriangleBoundaries(const hkVector4f& a, hkVector4f* b, SupportTypes checkSupport)
{
    const Q b0 = b[0].m_quad;
    const Q b1 = b[1].m_quad;
    const Q b2 = b[2].m_quad;
    const Q b02 = sub(b0, b2);
    const Q b21 = sub(b2, b1);
    const Q b10 = sub(b1, b0);
    const Q normal = cross(b21, b02);
    const Q a1 = sub(a.m_quad, b1);
    const Q a2 = sub(a.m_quad, b2);
    const Q a0 = sub(a.m_quad, b0);
    const Q c2 = cross(a2, b02);
    const Q c1 = cross(a1, b21);
    const Q c0 = cross(a0, b10);
    {
        const hkVector4f vc0(c1);
        const hkVector4f vc1(c2);
        const hkVector4f vc2(c0);
        const hkVector4f vn(normal);
        hkVector4fUtil::dot3_3vs3(vc0, vn, vc1, vn, vc2, vn, m_checkTriangleDots);
    }
    const int mask = _mm_movemask_ps(_mm_cmplt_ps(m_checkTriangleDots.m_quad, _mm_setzero_ps())) & 7;
    if (checkSupport != BUILD_NO_SUPPORT)
    {
        m_support.m_quad = normal;
        if (mask == 7)
        {
            const Q d = dot3(sub(m_verticesA[0].m_quad, m_verticesBinA[0].m_quad), normal);
            if (lessOrNaN(d, _mm_setzero_ps()))
            {
                m_support.m_quad = neg(normal);
                hkVector4f t = b[0];
                b[0] = b[1];
                b[1] = t;
                t = b[4];
                b[4] = b[5];
                b[5] = t;
                const float x = m_checkTriangleDots.m_quad.m128_f32[0];
                m_checkTriangleDots.m_quad.m128_f32[0] = m_checkTriangleDots.m_quad.m128_f32[1];
                m_checkTriangleDots.m_quad.m128_f32[1] = x;
                m_featureChange = 1;
            }
        }
    }
    return mask;
}

// 0x00E7EF70 (findClosestTriangle, file static)
// The face of the tetrahedron (opposite vertex 0, 1 or 2: faces (1,2,3), (0,2,3), (0,1,3)) nearest to a0, by squared
// signed plane distance: -1 when no face is at least eps in front, the backup search when the largest two are within
// 10%. sign orients the faces (the tetrahedron is A's or B's).
static int findClosestTriangle(const hkVector4f& a0, const hkVector4f* tetra, float sign)
{
    const Q t0 = tetra[0].m_quad;
    const Q t1 = tetra[1].m_quad;
    const Q t2 = tetra[2].m_quad;
    const Q t3 = tetra[3].m_quad;
    const Q e02 = sub(t0, t2);
    const Q e30 = sub(t3, t0);
    const Q e10 = sub(t1, t0);
    const Q e32 = sub(t3, t2);
    const Q e31 = sub(t3, t1);
    const Q e21 = sub(t2, t1);
    const Q n0 = cross(e21, e31);
    const Q n1 = cross(e02, e32);
    const Q n2 = cross(e10, e30);
    const Q d = mul(splat(sign), sub(a0.m_quad, t3));

    // the normals transposed, then d.y * row1 + d.x * row0 + d.z * row2 (not dot3_1vs3, whose w lane differs)
    hkVector4f dots;
    {
        hkVector4f r0(n0);
        hkVector4f r1(n1);
        hkVector4f r2(n2);
        HK_TRANSPOSE3f(r0, r1, r2);
        dots.m_quad = add(add(mul(bcast<1>(d), r1.m_quad), mul(bcast<0>(d), r0.m_quad)), mul(bcast<2>(d), r2.m_quad));
    }
    hkVector4f lengths;
    {
        const hkVector4f v0(n0);
        const hkVector4f v1(n1);
        const hkVector4f v2(n2);
        hkVector4fUtil::dot3_3vs3(v0, v0, v1, v1, v2, v2, lengths);
    }

    // the squared distances: |dot| * dot / |n|^2, HK_REAL_MAX for a degenerate face (the divisor is at least eps)
    const Q sq = signedSquare(dots.m_quad);
    const Q isZero = _mm_cmpeq_ps(lengths.m_quad, _mm_setzero_ps());
    const Q divisor = _mm_max_ps(lengths.m_quad, g_vectorfConstants[HK_QUADREAL_EPS]);
    const Q quotient = mul(recip23(divisor), sq);
    const Q dist = _mm_or_ps(_mm_andnot_ps(isZero, quotient), _mm_and_ps(isZero, g_vectorfConstants[HK_QUADREAL_MAX]));

    const Q y = bcast<1>(dist);
    const Q x = bcast<0>(dist);
    const Q z = bcast<2>(dist);
    const Q minAll = _mm_min_ps(z, _mm_min_ps(y, x));
    const Q maxAll = _mm_max_ps(z, _mm_max_ps(y, x));
    // the largest after replacing the largest by the smallest: the second largest
    const Q isMax = _mm_cmple_ps(maxAll, dist);
    const Q rest = _mm_or_ps(_mm_andnot_ps(isMax, dist), _mm_and_ps(isMax, minAll));
    const Q second = _mm_max_ps(bcast<2>(rest), _mm_max_ps(bcast<1>(rest), bcast<0>(rest)));
    const int maxMask = _mm_movemask_ps(_mm_and_ps(_mm_cmple_ps(maxAll, dist), laneMask(7)));

    if (lessOrNaN(maxAll, g_vectorfConstants[HK_QUADREAL_EPS]))
    {
        return -1;
    }
    const Q secondScaled = mul(second, splat(1.1f)); // 0x3F8CCCCD
    if (greater(maxAll, secondScaled))
    {
        return hkVector4Comparison_maskToLastIndex[maxMask];
    }
    return findClosestTriangleBackup(a0, tetra, dots);
}

// 0x00E7F270 (process_point_edge, file static)
// A point (vertexA[0]) against an edge (vertexB[0..1]): the support is perpendicular to the edge towards the point when
// the point projects inside the edge, else the edge shrinks to its nearer vertex (dimB - 1) and the support is the
// difference of the points. stype orients the support (B to A).
static void process_point_edge(hkVector4f* vertexA, hkVector4f* vertexB, hkPadSpu<int>& dimB, hkVector4f& support, hkpGsk::SupportTypes stype)
{
    const Q b0 = vertexB[0].m_quad;
    const Q b1 = vertexB[1].m_quad;
    const Q a = vertexA[0].m_quad;
    const Q edge = sub(b1, b0);
    const Q toB0 = sub(b0, a);
    const Q toB1 = sub(b1, a);
    const bool c1 = lessOrNaN(dot3(toB1, edge), _mm_setzero_ps());
    const bool c0 = lessOrNaN(dot3(toB0, edge), _mm_setzero_ps());
    if (c1 != c0)
    {
        const Q n = mul(_mm_rsqrt_ps(dot3(edge, edge)), edge);
        const Q c = cross(toB1, toB0);
        if (stype == hkpGsk::BUILD_POS_SUPPORT)
        {
            support.m_quad = cross(n, c);
        }
        else
        {
            support.m_quad = cross(c, n);
        }
        return;
    }
    if (c1)
    {
        vertexB[0] = vertexB[1];
        vertexB[4] = vertexB[5];
    }
    dimB -= 1;
    if (stype == hkpGsk::BUILD_POS_SUPPORT)
    {
        support.m_quad = sub(a, vertexB[0].m_quad);
    }
    else
    {
        support.m_quad = sub(vertexB[0].m_quad, a);
    }
}

// 0x00E7E850
// An edge (vertexA[0..1]) against a triangle (vertexB[0..2]). Coming from the edge-edge case, the edge is tested for
// crossing the triangle (penetration), then its nearer vertex against the triangle; otherwise the edge's second vertex
// is tested against the triangle and the triangle is reduced by the edge planes the vertex is outside of. Returns
// NEXT_IS_PENETRATION, NEXT_IS_BREAK (a point-triangle feature remains; dimA reduced) or NEXT_IS_EDGE_EDGE (dimB
// reduced to the triangle edge to test next).
hkpGsk::NextCase hkpGsk::processEdgeTriangle(hkVector4f* vertexA, hkVector4f* vertexB, hkPadSpu<int>& dimA, hkPadSpu<int>& dimB, hkBool comingFromEdgeEdge, SupportTypes supportType)
{
    if (comingFromEdgeEdge)
    {
        const Q t0 = vertexB[0].m_quad;
        const Q e12 = sub(vertexB[1].m_quad, vertexB[2].m_quad);
        const Q e02 = sub(t0, vertexB[2].m_quad);
        const Q normal = cross(e12, e02);
        const Q a0 = vertexA[0].m_quad;
        const Q a1 = vertexA[1].m_quad;
        const Q d0 = dot3(sub(a0, t0), normal);
        const Q d1 = dot3(sub(a1, t0), normal);
        if (lessOrNaN(d1, _mm_setzero_ps()) != lessOrNaN(d0, _mm_setzero_ps()))
        {
            // the point where the edge crosses the triangle plane
            const Q t = mul(recip23(sub(d0, d1)), d0);
            hkVector4f p;
            p.m_quad = add(mul(sub(a1, a0), t), a0);
            if (checkTriangleBoundaries(p, vertexB, BUILD_NO_SUPPORT) == 7)
            {
                return NEXT_IS_PENETRATION;
            }
        }
        else
        {
            if (greater(abs(d0), abs(d1)))
            {
                hkVector4f t = vertexA[0];
                vertexA[0] = vertexA[1];
                vertexA[1] = t;
                t = vertexA[4];
                vertexA[4] = vertexA[5];
                vertexA[5] = t;
            }
            if (checkTriangleBoundaries(vertexA[0], vertexB, BUILD_POS_SUPPORT) == 7)
            {
                dimA -= 1;
                return NEXT_IS_BREAK;
            }
        }
        // the triangle edge (2,1) or (2,0) nearer to the edge
        hkVector4f dA;
        dA.m_quad = sub(vertexA[1].m_quad, vertexA[0].m_quad);
        hkLineSegmentUtil::ClosestLineSegLineSegResult result1;
        const hkVector4f edge12(e12);
        const hkVector4f edge02(e02);
        hkLineSegmentUtil::closestLineSegLineSeg(vertexA[0], dA, vertexB[2], edge12, result1);
        hkLineSegmentUtil::ClosestLineSegLineSegResult result2;
        hkLineSegmentUtil::closestLineSegLineSeg(vertexA[0], dA, vertexB[2], edge02, result2);
        const int keep = (result1.m_distanceSquared.val() > result2.m_distanceSquared.val()) ? 1 : 0;
        vertexB[keep] = vertexB[2];
        vertexB[keep + 4] = vertexB[6];
        dimB -= 1;
        return NEXT_IS_EDGE_EDGE;
    }

    const hkVector4f oldDots = m_checkTriangleDots;
    const Q support = m_support.m_quad;
    const Q a0 = vertexA[0].m_quad;
    const Q a1 = vertexA[1].m_quad;
    const Q d1 = dot3(sub(a1, vertexB[0].m_quad), support);
    const Q d0 = dot3(sub(a0, vertexB[0].m_quad), support);
    if (lessOrNaN(mul(d0, d1), _mm_setzero_ps()))
    {
        const Q t = mul(recip23(sub(d0, d1)), d0);
        hkVector4f p;
        p.m_quad = add(mul(sub(a1, a0), t), a0);
        if (checkTriangleBoundaries(p, vertexB, BUILD_NO_SUPPORT) == 7)
        {
            return NEXT_IS_PENETRATION;
        }
    }
    const int mask = checkTriangleBoundaries(vertexA[1], vertexB, BUILD_NO_SUPPORT);
    if (mask == 7)
    {
        dimA -= 1;
        vertexA[0] = vertexA[1];
        vertexA[4] = vertexA[5];
        m_supportState = (SupportState)supportType;
        return NEXT_IS_BREAK;
    }

    // which triangle vertex to drop: 0 replaces vertex 0 by 2, 1 replaces vertex 1 by 2, 2 drops vertex 2
    int drop;
    const Q n = m_checkTriangleDots.m_quad;
    const Q o = oldDots.m_quad;
    switch (mask)
    {
    case 6:
        drop = 0;
        break;
    case 5:
        drop = 1;
        break;
    case 3:
        drop = 2;
        break;
    case 1:
        drop = greater(mul(sub(bcast<1>(n), bcast<1>(o)), bcast<2>(o)), mul(sub(bcast<2>(n), bcast<2>(o)), bcast<1>(o))) ? 2 : 1;
        break;
    case 2:
        drop = lessEqualOrNaN(mul(sub(bcast<2>(n), bcast<2>(o)), bcast<0>(o)), mul(sub(bcast<0>(n), bcast<0>(o)), bcast<2>(o))) ? 2 : 0;
        break;
    case 4:
        drop = lessEqualOrNaN(mul(sub(bcast<0>(n), bcast<0>(o)), bcast<1>(o)), mul(sub(bcast<1>(n), bcast<1>(o)), bcast<0>(o))) ? 0 : 1;
        break;
    default:
        drop = 0;
        break;
    }
    if (drop == 0)
    {
        vertexB[0] = vertexB[2];
        vertexB[4] = vertexB[6];
    }
    else if (drop == 1)
    {
        vertexB[1] = vertexB[2];
        vertexB[5] = vertexB[6];
    }
    dimB -= 1;
    return NEXT_IS_EDGE_EDGE;
}

// The edge case of a point against a triangle (pointTriangle) or a triangle against a point: idx is the edge outside
// whose plane the point is; the triangle keeps the edge or one of its vertices (verts: the triangle, copies: its
// second space or null, point: the other shape's vertex).
static void hkGsk_reduceTriangleToEdge(hkVector4f* verts, hkVector4f* copies, hkPadSpu<int>& dim, const hkVector4f& point, int idx)
{
    const int e0 = hkGsk_edgeVertex(idx);
    int e2 = hkGsk_edgeVertex(idx + 2);
    const Q pivot = verts[idx].m_quad;
    const Q toPoint = sub(point.m_quad, pivot);
    const Q d0 = dot3(sub(verts[e0].m_quad, pivot), toPoint);
    const Q edge2 = sub(verts[e2].m_quad, pivot);
    const hkVector4f v2 = verts[2];
    dim = 2;
    if (greaterEqual(d0, _mm_setzero_ps()))
    {
        verts[e2] = v2;
        if (copies != nullptr)
        {
            copies[e2] = copies[2];
        }
        return;
    }
    verts[e0] = v2;
    if (copies != nullptr)
    {
        copies[e0] = copies[2];
    }
    if (greaterEqual(dot3(edge2, toPoint), _mm_setzero_ps()))
    {
        return;
    }
    const int d = dim;
    if (e2 == d)
    {
        e2 = e0;
    }
    dim = d - 1;
    verts[e2] = verts[dim];
    if (copies != nullptr)
    {
        copies[e2] = copies[dim];
    }
}

// 0x00E80E70
// Reduces the simplex to the feature pair closest to the origin of the Minkowski difference and sets m_support (from B
// to A) for it. A case that shrinks the simplex runs the switch again. PENETRATION when the origin is inside.
hkpGsk::ReduceDimensionResult hkpGsk::reduceDimension()
{
    m_supportState = SUPPORT_CHANGED;
    for (;;)
    {
        switch (m_dimA * 8 | m_dimB)
        {
        case GSK_POINT_POINT:
            goto pointPoint;

        case GSK_POINT_EDGE:
            process_point_edge(m_verticesA, m_verticesBinA, m_dimB, m_support, BUILD_POS_SUPPORT);
            goto done;

        case GSK_EDGE_POINT:
            process_point_edge(m_verticesBinA, m_verticesA, m_dimA, m_support, BUILD_NEG_SUPPORT);
            goto done;

        case GSK_POINT_TETRA:
        {
            const int idx = findClosestTriangle(m_verticesA[0], m_verticesBinA, 1.0f);
            if (idx < 0)
            {
                return REDUCE_DIMENSION_PENETRATION;
            }
            m_dimB -= 1;
            m_verticesBinA[idx] = m_verticesBinA[3];
            m_verticesBinB[idx] = m_verticesBinB[3];
        }
            // fall through
        case GSK_POINT_TRIANGLE:
        {
            const int mask = checkTriangleBoundaries(m_verticesA[0], m_verticesBinA, BUILD_POS_SUPPORT);
            if (mask == 7)
            {
                goto done;
            }
            const int idx = hkGsk_maskToIndex(mask);
            if (idx >= 0)
            {
                hkGsk_reduceTriangleToEdge(m_verticesBinA, m_verticesBinB, m_dimB, m_verticesA[0], idx);
                continue;
            }
            const int vertex = idx + 8;
            if (vertex == 7)
            {
                m_dimB = 1;
                m_dimA = 1;
                goto pointPoint;
            }
            m_dimB -= 1;
            m_verticesBinA[vertex] = m_verticesBinA[2];
            m_verticesBinB[vertex] = m_verticesBinB[2];
            process_point_edge(m_verticesA, m_verticesBinA, m_dimB, m_support, BUILD_POS_SUPPORT);
            goto done;
        }

        case GSK_TETRA_POINT:
        {
            const int idx = findClosestTriangle(m_verticesBinA[0], m_verticesA, -1.0f);
            if (idx < 0)
            {
                return REDUCE_DIMENSION_PENETRATION;
            }
            m_dimA -= 1;
            m_verticesA[idx] = m_verticesA[3];
        }
            // fall through
        case GSK_TRIANGLE_POINT:
        {
            const int mask = checkTriangleBoundaries(m_verticesBinA[0], m_verticesA, BUILD_NEG_SUPPORT);
            if (mask == 7)
            {
                goto done;
            }
            const int idx = hkGsk_maskToIndex(mask);
            if (idx >= 0)
            {
                // A's second copies (m_dummy) are not maintained here (as in the original)
                hkGsk_reduceTriangleToEdge(m_verticesA, nullptr, m_dimA, m_verticesBinA[0], idx);
                continue;
            }
            const int vertex = idx + 8;
            if (vertex == 7)
            {
                m_dimB = 1;
                m_dimA = 1;
                goto pointPoint;
            }
            m_dimA -= 1;
            m_verticesA[vertex] = m_verticesA[2];
            process_point_edge(m_verticesBinA, m_verticesA, m_dimA, m_support, BUILD_NEG_SUPPORT);
            goto done;
        }

        case GSK_EDGE_TRIANGLE:
        case GSK_TRIANGLE_EDGE:
        {
            NextCase next;
            if ((m_dimA * 8 | m_dimB) == GSK_EDGE_TRIANGLE)
            {
                next = processEdgeTriangle(m_verticesA, m_verticesBinA, m_dimA, m_dimB, m_lastDimB == 2, BUILD_POS_SUPPORT);
            }
            else
            {
                next = processEdgeTriangle(m_verticesBinA, m_verticesA, m_dimB, m_dimA, m_lastDimB == 2, BUILD_NEG_SUPPORT);
            }
            if (next == NEXT_IS_PENETRATION)
            {
                return REDUCE_DIMENSION_PENETRATION;
            }
            if (next != NEXT_IS_EDGE_EDGE)
            {
                goto done;
            }
        }
            // fall through
        case GSK_EDGE_EDGE:
        {
            hkVector4f dB;
            dB.m_quad = sub(m_verticesBinA[1].m_quad, m_verticesBinA[0].m_quad);
            hkVector4f dA;
            dA.m_quad = sub(m_verticesA[1].m_quad, m_verticesA[0].m_quad);
            const int r = hkLineSegmentUtil::closestLineSegLineSeg(m_verticesA[0], dA, m_verticesBinA[0], dB, m_closestLineLineResult);
            if (r == 0)
            {
                // the edges cross: the support is their common normal, towards A
                const Q n = cross(dA.m_quad, dB.m_quad);
                m_support.m_quad = flipSign(n, dot3(sub(m_verticesA[0].m_quad, m_verticesBinA[0].m_quad), n));
                goto done;
            }
            // an end of an edge is closest: the edge shrinks to it (A's second copy is not updated, as in the
            // original)
            if (r & 1)
            {
                m_verticesA[0] = m_verticesA[1];
                m_dimA = 1;
            }
            else if (r & 2)
            {
                m_dimA = 1;
            }
            if (r & 4)
            {
                m_verticesBinA[0] = m_verticesBinA[1];
                m_verticesBinB[0] = m_verticesBinB[1];
                m_dimB = 1;
            }
            else if (r & 8)
            {
                m_dimB = 1;
            }
            continue;
        }

        default:
            return REDUCE_DIMENSION_PENETRATION;
        }
    }
pointPoint:
    m_support.m_quad = sub(m_verticesA[0].m_quad, m_verticesBinA[0].m_quad);
done:
    m_lastDimB = m_dimB;
    return REDUCE_DIMENSION_OK;
}

// A new supporting vertex (newVertex = vertices[dim]) is accepted when it is further along the direction than the
// first vertex and not closer than the existing others (dim 2: vertex 1; dim 3: vertices 2 and 1), each by the scaled
// tolerance (signed squared distances).
static bool hkGsk_acceptVertex(const hkVector4f* vertices, int dim, Q direction, Q tolerance)
{
    const Q v = vertices[dim].m_quad;
    if (lessEqualOrNaN(signedSquare(dot3(sub(v, vertices[0].m_quad), direction)), tolerance))
    {
        return false;
    }
    if (dim == 3)
    {
        if (lessOrNaN(signedSquare(dot3(sub(v, vertices[2].m_quad), direction)), tolerance))
        {
            return false;
        }
    }
    if (dim == 2 || dim == 3)
    {
        if (lessOrNaN(signedSquare(dot3(sub(v, vertices[1].m_quad), direction)), tolerance))
        {
            return false;
        }
    }
    return true;
}

// 0x00E7FDB0
// The closest features of A and B (aTb: B in A's space). On return the separating normal (in A's space, from B to A)
// has the distance in w. A penetration is resolved by the penetration depth search (handlePenetration) unless
// m_doNotHandlePenetration asks to report HK_GSK_PENETRATING instead.
hkpGskStatus hkpGsk::getClosestFeature(const hkpConvexShape* shapeA, const hkpConvexShape* shapeB, const hkTransformf& aTb, hkVector4f& separatingNormalOut)
{
    for (int i = m_dimB - 1; i >= 0; i--)
    {
        m_verticesBinA[i].m_quad = transformPos(aTb, m_verticesBinB[i].m_quad);
    }
    Q eps = splat(1.192093e-10f); // 0x2F03126F (0.001 * FLT_EPSILON), doubled before each iteration
    int iterations = 0;
    for (;;)
    {
        iterations++;
        const int oldDimSum = m_dimA + m_dimB;
        eps = add(eps, eps);
        const ReduceDimensionResult result = reduceDimension();

        const Q support = m_support.m_quad;
        const Q len2 = dot3(support, support);
        const Q dist = dot3(sub(m_verticesA[0].m_quad, m_verticesBinA[0].m_quad), support);
        m_featureChange |= (m_dimA - oldDimSum) + m_dimB;
        if (result != REDUCE_DIMENSION_OK)
        {
            goto penetration;
        }
        const Q tolerance = mul(eps, len2);
        if (lessEqualOrNaN(signedSquare(dist), tolerance) || lessOrNaN(len2, eps))
        {
            goto penetration;
        }

        // odd iterations try A first, even ones B first
        const bool aFirst = (iterations & 1) != 0;
        for (int pass = 0; pass < 3; pass++)
        {
            const bool tryA = (pass == 0 && aFirst) || (pass == 2 && !aFirst);
            const bool tryB = pass == 1;
            if (tryA)
            {
                const int dimA = m_dimA;
                if (dimA == m_maxDimA || m_supportState == DONT_CALL_GET_SUPPORTING_VERTEX_A)
                {
                    continue;
                }
                hkVector4f direction;
                direction.m_quad = neg(support);
                shapeA->getSupportingVertex(direction, m_verticesA[dimA]);
                if (!hkGsk_acceptVertex(m_verticesA, dimA, direction.m_quad, tolerance))
                {
                    continue;
                }
                m_dimA = dimA + 1;
                m_featureChange = 1;
                goto nextIteration;
            }
            if (tryB)
            {
                const int dimB = m_dimB;
                if (dimB == m_maxDimB || m_supportState == DONT_CALL_GET_SUPPORTING_VERTEX_B)
                {
                    continue;
                }
                hkVector4f direction;
                direction.m_quad = rotateInverse(aTb.getRotation(), support);
                shapeB->getSupportingVertex(direction, m_verticesBinB[dimB]);
                if (!hkGsk_acceptVertex(m_verticesBinB, dimB, direction.m_quad, tolerance))
                {
                    continue;
                }
                m_verticesBinA[dimB].m_quad = transformPos(aTb, m_verticesBinB[dimB].m_quad);
                m_dimB += 1;
                m_featureChange = 1;
                goto nextIteration;
            }
        }
        goto done;
    nextIteration:;
    }

penetration:
    if (m_doNotHandlePenetration)
    {
        const int dimA = m_dimA;
        const int dimB = m_dimB;
        if (dimA + dimB > 4)
        {
            if (dimA > dimB)
            {
                m_dimA = 3;
                m_dimB = 1;
                return HK_GSK_PENETRATING;
            }
            m_dimA = 1;
            m_dimB = 3;
        }
        return HK_GSK_PENETRATING;
    }
    handlePenetration(shapeA, shapeB, aTb);

done:
    {
        const Q support = m_support.m_quad;
        const Q diff = sub(m_verticesA[0].m_quad, m_verticesBinA[0].m_quad);
        const Q normal = mul(invSqrt23SetZero(dot3(support, support)), support);
        m_support.m_quad = normal;
        const Q d = dot3(m_support.m_quad, diff);
        separatingNormalOut.m_quad = setXYZ_W(m_support.m_quad, d);
        m_support.m_quad = separatingNormalOut.m_quad;
    }
    return HK_GSK_OK;
}

// 0x00E7E3C0
// The current features as the cache's vertex ids, each shape's ids in the original's partial sort (largest first).
void hkpGsk::exitAndExportCacheImpl(hkpGskCache& cache) const
{
    cache.m_dimA = (unsigned char)m_dimA.val();
    cache.m_dimB = (unsigned char)m_dimB.val();
    for (int s = 0; s < 2; s++)
    {
        const hkcdVertex* v = (s == 0) ? m_verticesA : m_verticesBinB;
        const int dim = (s == 0) ? m_dimA : m_dimB;
        unsigned short* out = (s == 0) ? cache.m_vertices : cache.m_vertices + m_dimA;
        unsigned int first = (unsigned short)v[0].getId();
        if (dim >= 2)
        {
            unsigned int second = (unsigned short)v[1].getId();
            int swaps = 0;
            if (second > first)
            {
                const unsigned int t = second;
                second = first;
                first = t;
                swaps = 1;
            }
            if (dim > 2)
            {
                unsigned int third = (unsigned short)v[2].getId();
                if (third > first)
                {
                    const unsigned int t = third;
                    third = first;
                    first = t;
                    swaps++;
                }
                if (swaps == 1)
                {
                    const unsigned int t = third;
                    third = second;
                    second = t;
                }
                out[2] = (unsigned short)third;
            }
            out[1] = (unsigned short)second;
        }
        out[0] = (unsigned short)first;
    }
}

// 0x00E7ECB0
void hkpGsk::convertFeatureToClosestDistance(const hkVector4f& separatingNormal, hkpGskOut& out)
{
    out.m_distance = separatingNormal.m_quad.m128_f32[3];
    out.m_normalInA = separatingNormal;
    if (m_dimA == 1)
    {
        out.m_pointAinA = m_verticesA[0];
        return;
    }
    if (m_dimB == 1)
    {
        out.m_pointAinA.m_quad = add(mul(bcast<3>(separatingNormal.m_quad), out.m_normalInA.m_quad), m_verticesBinA[0].m_quad);
        return;
    }
    out.m_pointAinA = m_closestLineLineResult.m_closestPointA;
}

// 0x00E80B50
// The closest points of two convex shapes from a cache (updated when the features change): the separating normal in
// world space (transformA) with the distance between the shapes' surfaces (radii subtracted) in w, and the point on
// B. HK_FAILURE when the distance is not below the collision tolerance.
hkResult hkpGsk::getClosestPoint(const GetClosesetPointInput& input, hkpGskCache& gskCache, hkVector4f& separatingNormalOut, hkVector4f& pointOnBOut)
{
    hkpGsk gsk;
    gsk.init(input.m_shapeA, input.m_shapeB, gskCache);
    hkVector4f separatingNormal;
    separatingNormal.m_quad = _mm_setzero_ps();
    gsk.getClosestFeature(input.m_shapeA, input.m_shapeB, *input.m_aTb, separatingNormal);
    gsk.checkForChangesAndUpdateCache(gskCache);

    const Q n = separatingNormal.m_quad;
    const Q w = bcast<3>(n);
    const Q radiusB = splat(input.m_shapeB->m_radius);
    const Q dist = sub(sub(w, splat(input.m_shapeA->m_radius)), radiusB);
    separatingNormalOut.m_quad = setXYZ_W(rotate(input.m_transformA->getRotation(), n), dist);
    if (greaterEqual(dist, splat(input.m_collisionTolerance)))
    {
        return HK_FAILURE;
    }
    Q pointInA;
    if (gsk.m_dimA == 1)
    {
        pointInA = add(mul(sub(radiusB, w), n), gsk.m_verticesA[0].m_quad);
    }
    else if (gsk.m_dimB == 1)
    {
        pointInA = add(mul(radiusB, n), gsk.m_verticesBinA[0].m_quad);
    }
    else
    {
        pointInA = add(mul(sub(radiusB, w), n), gsk.m_closestLineLineResult.m_closestPointA.m_quad);
    }
    pointOnBOut.m_quad = transformPos(*input.m_transformA, pointInA);
    return HK_SUCCESS;
}

// 0x00E7E3B0
void hkpGsk::codeEnd()
{
}

// 0x00E7F3A0
// The penetration depth by the expanding polytope (hkCalculatePenetrationDepth) from the current simplex, completed
// to four vertex pairs where possible. The resulting features are the polytope's nearest triangle with duplicate
// vertices removed; the heuristic sampling fallback leaves a point-point feature.
void hkpGsk::handlePenetration(const hkpConvexShape* shapeA, const hkpConvexShape* shapeB, const hkTransformf& aTb)
{
    int simplexSize = 4;
    const int dimA = m_dimA;
    const int dimB = m_dimB;
    if (dimB >= 3)
    {
        if (dimB > 3)
        {
            m_verticesA[3] = m_verticesA[0];
        }
        else if (dimA > 1)
        {
            m_verticesA[3] = m_verticesA[1];
            m_verticesBinB[3] = m_verticesBinB[0];
            m_verticesBinA[3] = m_verticesBinA[0];
        }
        else
        {
            simplexSize = 3;
        }
        m_verticesA[1] = m_verticesA[0];
        m_verticesA[2] = m_verticesA[0];
    }
    else if (dimA >= 3)
    {
        if (dimA > 3)
        {
            m_verticesBinB[3] = m_verticesBinB[0];
            m_verticesBinA[3] = m_verticesBinA[0];
        }
        else if (dimB > 1)
        {
            m_verticesA[3] = m_verticesA[0];
            m_verticesBinB[3] = m_verticesBinB[1];
            m_verticesBinA[3] = m_verticesBinA[1];
        }
        else
        {
            simplexSize = 3;
        }
        m_verticesBinB[1] = m_verticesBinB[0];
        m_verticesBinA[1] = m_verticesBinA[0];
        m_verticesBinB[2] = m_verticesBinB[0];
        m_verticesBinA[2] = m_verticesBinA[0];
    }
    else
    {
        const hkcdVertex a0 = m_verticesA[0];
        m_verticesA[dimA] = a0;
        m_verticesA[m_dimA + 1] = a0;
        const hkcdVertex b0 = m_verticesBinB[0];
        m_verticesBinB[m_dimB] = b0;
        m_verticesBinB[m_dimB + 1] = b0;
        const hkVector4f ba0 = m_verticesBinA[0];
        m_verticesBinA[m_dimB] = ba0;
        m_verticesBinA[m_dimB + 1] = ba0;
        simplexSize = 1;
    }

    hkVector4f simplex[4];
    for (int i = 0; i < simplexSize; i++)
    {
        simplex[i].m_quad = sub(m_verticesA[i].m_quad, m_verticesBinA[i].m_quad);
    }
    hkpGskOut output;
    const hkpGskStatus status = hkCalculatePenetrationDepth(shapeA, shapeB, aTb, 0.0001f, m_verticesA, m_verticesBinB, simplex, simplexSize, output);
    if (status == HK_GSK_PD_HEURISTIC_SAMPLING)
    {
        m_dimA = 1;
        m_dimB = 1;
        m_featureChange = 1;
        m_support = output.m_normalInA;
        return;
    }

    // the polytope's triangle: drop duplicate vertex ids
    m_dimA = 3;
    m_dimB = 3;
    {
        const unsigned short id1 = (unsigned short)m_verticesA[1].getId();
        const unsigned short id2 = (unsigned short)m_verticesA[2].getId();
        if (id1 == id2 || (unsigned short)m_verticesA[0].getId() == id2)
        {
            m_dimA = 2;
        }
        if ((unsigned short)m_verticesA[0].getId() == id1)
        {
            m_dimA -= 1;
            m_verticesA[0] = m_verticesA[m_dimA];
        }
    }
    {
        const unsigned short id1 = (unsigned short)m_verticesBinB[1].getId();
        const unsigned short id2 = (unsigned short)m_verticesBinB[2].getId();
        if (id1 == id2 || (unsigned short)m_verticesBinB[0].getId() == id2)
        {
            m_dimB -= 1;
        }
        if (m_dimB >= 2 && (unsigned short)m_verticesBinB[0].getId() == id1)
        {
            m_dimB -= 1;
            m_verticesBinB[0] = m_verticesBinB[m_dimB];
        }
    }
    for (int i = m_dimB - 1; i >= 0; i--)
    {
        m_verticesBinA[i].m_quad = transformPos(aTb, m_verticesBinB[i].m_quad);
    }
    if (m_dimA + m_dimB > 4)
    {
        reduceDimensionExtended();
    }
    if (m_dimA == 2 && m_dimB == 2)
    {
        hkVector4f dA;
        dA.m_quad = sub(m_verticesA[1].m_quad, m_verticesA[0].m_quad);
        hkVector4f dB;
        dB.m_quad = sub(m_verticesBinA[1].m_quad, m_verticesBinA[0].m_quad);
        hkLineSegmentUtil::closestLineSegLineSeg(m_verticesA[0], dA, m_verticesBinA[0], dB, m_closestLineLineResult);
    }
    m_featureChange = 1;
    m_support = output.m_normalInA;
}

// 0x00E7ED00
// Reduces a penetration feature with more than four vertices: a vertex of one shape inside the other's triangle
// becomes a point-triangle feature; otherwise the closest pair of edges (triangle edges for a triangle, the edge
// itself otherwise) remains.
void hkpGsk::reduceDimensionExtended()
{
    if (m_dimB == 3)
    {
        for (int i = 0; i < m_dimA; i++)
        {
            if (checkTriangleBoundaries(m_verticesA[i], m_verticesBinA, BUILD_NO_SUPPORT) == 7)
            {
                m_verticesA[0] = m_verticesA[i];
                m_dimA = 1;
                return;
            }
        }
    }
    if (m_dimA == 3)
    {
        for (int j = 0; j < m_dimB; j++)
        {
            if (checkTriangleBoundaries(m_verticesBinA[j], m_verticesA, BUILD_NO_SUPPORT) == 7)
            {
                m_verticesBinB[0] = m_verticesBinB[j];
                m_verticesBinA[0] = m_verticesBinA[j];
                m_dimB = 1;
                return;
            }
        }
    }
    float best = HK_REAL_MAX;
    int bestI = 0;
    int bestJ = 0;
    const int numA = (m_dimA == 3) ? 3 : 1;
    const int numB = (m_dimB == 3) ? 3 : 1;
    for (int i = 0; i < numA; i++)
    {
        for (int j = 0; j < numB; j++)
        {
            hkVector4f dA;
            dA.m_quad = sub(m_verticesA[hkGsk_edgeVertex(i + 2)].m_quad, m_verticesA[i].m_quad);
            hkVector4f dB;
            dB.m_quad = sub(m_verticesBinA[hkGsk_edgeVertex(j + 2)].m_quad, m_verticesBinA[j].m_quad);
            hkLineSegmentUtil::ClosestLineSegLineSegResult result;
            hkLineSegmentUtil::closestLineSegLineSeg(m_verticesA[i], dA, m_verticesBinA[j], dB, result);
            if (best > result.m_distanceSquared.val())
            {
                best = result.m_distanceSquared;
                bestI = i;
                bestJ = j;
            }
        }
    }
    if (numA == 3)
    {
        m_dimA -= 1;
        m_verticesA[hkGsk_edgeVertex(bestI)] = m_verticesA[m_dimA];
    }
    if (numB == 3)
    {
        m_dimB -= 1;
        const int k = hkGsk_edgeVertex(bestJ);
        m_verticesBinB[k] = m_verticesBinB[m_dimB];
        m_verticesBinA[k] = m_verticesBinA[m_dimB];
    }
}

// 0x00E7F8E0
// The contact of a feature pair along the master normal: pointA (the contact point, on A or projected onto A's
// feature) and support (the normal, with the distance in w).
void hkGskRecalcContactInternal(hkVector4f* verticesA, hkVector4f* verticesB, int dimA, int dimB, const hkVector4f& masterNormal, hkVector4f& pointA, hkVector4f& support)
{
    switch (dimA * 8 | dimB)
    {
    case hkpGsk::GSK_POINT_POINT:
    {
        // the normal stays the master normal unless the points' difference is clearly along it
        const Q d = sub(verticesA[0].m_quad, verticesB[0].m_quad);
        support.m_quad = d;
        const Q n = masterNormal.m_quad;
        const Q nd = mul(n, d);
        const Q dn1000 = mul(hadd3(nd), splat(1000.0f)); // 0x447A0000
        const Q len2 = dot3(d, d);
        Q dist;
        if (lessOrNaN(dn1000, len2))
        {
            dist = hadd3(nd);
            support.m_quad = n;
        }
        else
        {
            const Q inv = invSqrt23SetZero(len2);
            support.m_quad = mul(inv, d);
            dist = mul(inv, len2);
        }
        support.m_quad = setXYZ_W(support.m_quad, dist);
        pointA = verticesA[0];
        return;
    }
    case hkpGsk::GSK_POINT_EDGE:
    {
        const Q n = masterNormal.m_quad;
        const Q dist = dot3(sub(verticesA[0].m_quad, verticesB[0].m_quad), n);
        support.m_quad = setXYZ_W(n, dist);
        pointA = verticesA[0];
        return;
    }
    case hkpGsk::GSK_EDGE_POINT:
    {
        const Q n = masterNormal.m_quad;
        const Q d = sub(verticesB[0].m_quad, verticesA[0].m_quad);
        support.m_quad = n;
        const Q dist = dot3(d, n);
        pointA.m_quad = sub(verticesB[0].m_quad, mul(dist, support.m_quad));
        support.m_quad = setXYZ_W(support.m_quad, sub(_mm_setzero_ps(), dist));
        return;
    }
    case hkpGsk::GSK_POINT_TRIANGLE:
    case hkpGsk::GSK_TRIANGLE_POINT:
    {
        const bool pointA_ = (dimA * 8 | dimB) == hkpGsk::GSK_POINT_TRIANGLE;
        const hkVector4f* tri = pointA_ ? verticesB : verticesA;
        const hkVector4f* point = pointA_ ? verticesA : verticesB;
        const Q t2 = tri[2].m_quad;
        const Q n = cross(sub(t2, tri[1].m_quad), sub(tri[0].m_quad, t2));
        const Q len2 = dot3(n, n);
        support.m_quad = n;
        // the triangle normal on the master normal's side
        const Q oriented = flipSign(n, dot3(masterNormal.m_quad, n));
        const Q normal = mul(oriented, invSqrt23SetZero(len2));
        support.m_quad = normal;
        const Q p = point[0].m_quad;
        const Q dist = dot3(sub(p, tri[0].m_quad), normal);
        pointA.m_quad = p;
        if (pointA_)
        {
            support.m_quad = setXYZ_W(support.m_quad, dist);
            return;
        }
        pointA.m_quad = sub(p, mul(dist, support.m_quad));
        support.m_quad = setXYZ_W(support.m_quad, sub(_mm_setzero_ps(), dist));
        return;
    }
    case hkpGsk::GSK_EDGE_EDGE:
    {
        hkVector4f dA;
        dA.m_quad = sub(verticesA[1].m_quad, verticesA[0].m_quad);
        hkVector4f dB;
        dB.m_quad = sub(verticesB[1].m_quad, verticesB[0].m_quad);
        hkLineSegmentUtil::ClosestLineSegLineSegResult result;
        hkLineSegmentUtil::closestLineSegLineSeg(verticesA[0], dA, verticesB[0], dB, result);
        // the edges' common normal unless they are near parallel (|dA x dB|^2 * 0.001 <= (|dA|^2 + |dB|^2)^2)
        const Q c = cross(dA.m_quad, dB.m_quad);
        const Q lengths = hadd3(add(mul(dB.m_quad, dB.m_quad), mul(dA.m_quad, dA.m_quad)));
        support.m_quad = c;
        const Q c2 = mul(dot3(c, c), splat(0.001f)); // 0x3A83126F
        if (greater(c2, mul(lengths, lengths)))
        {
            const Q oriented = flipSign(c, dot3(c, masterNormal.m_quad));
            support.m_quad = oriented;
            support.m_quad = mul(invSqrt23SetZero(dot3(oriented, oriented)), oriented);
        }
        else
        {
            support = masterNormal;
        }
        const Q dist = dot3(sub(verticesA[0].m_quad, verticesB[0].m_quad), support.m_quad);
        pointA = result.m_closestPointA;
        support.m_quad = setXYZ_W(support.m_quad, dist);
        return;
    }
    default:
        return;
    }
}
