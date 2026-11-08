// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/penetration/hkpconvexpenetrationutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the penetration depth of two penetrating convex shapes, from rfg.exe. The
// simplex the GSK left is grown to a tetrahedron around the origin of the Minkowski difference A - B
// (builtTetrahedron), then expanded as a winged-edge polytope towards the origin's nearest face (the expanding
// polytope algorithm). When the polytope fails, the search restarts from one vertex with a jittered transform, up to
// 20 times; an unsolvable case falls back to heuristic sampling. No SDK reference exists; ported from the binary.
//
// The shipped build records the "TtPenetration"/"Et" monitor timers (TLS hkMonitorStream); they are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/penetration/hkpconvexpenetrationutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/algorithms/triangle/hkcdtriangleutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/hkpconvexshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/penetration/hkpcvxcvxdistbyheuristicsampling.h"

using namespace hkGskExact;

// A direction perpendicular to d (w ignored): (-z, 0, x) when x^2 + z^2 dominates x^2 + y^2, else (-y, x, 0).
static inline Q hkPenetration_perpendicular(Q d)
{
    const Q noW = _mm_castsi128_ps(_mm_srli_si128(_mm_slli_si128(_mm_castps_si128(d), 4), 4));
    const Q zx = _mm_shuffle_ps(noW, noW, 0xCE); // (z, 0, x, 0)
    const Q zx2 = mul(zx, zx);
    const Q xzSum = _mm_add_ps(_mm_shuffle_ps(zx2, zx2, 0x4E), zx2);
    const Q d2 = mul(d, d);
    const Q xy = add(bcast<1>(d2), bcast<0>(d2));
    const Q xz = add(_mm_shuffle_ps(xzSum, xzSum, 0xB1), xzSum);
    const Q useXz = _mm_cmplt_ps(xy, xz);
    const Q firstSign = _mm_castsi128_ps(_mm_insert_epi16(_mm_setzero_si128(), 0x8000, 1)); // (0x80000000, 0, 0, 0)
    const Q yx = _mm_xor_ps(_mm_shuffle_ps(noW, noW, 0xF1), firstSign); // (-y, x, 0, 0)
    const Q zxNeg = _mm_xor_ps(firstSign, zx); // (-z, 0, x, 0)
    return _mm_or_ps(_mm_andnot_ps(useXz, yx), _mm_and_ps(zxNeg, useXz));
}

// |x| from its square: x2 * invSqrt(x2), 0 for x2 <= 0
static inline Q hkPenetration_length(Q x2)
{
    const Q le0 = _mm_cmple_ps(x2, _mm_setzero_ps());
    const Q r = _mm_rsqrt_ps(x2);
    const Q t = mul(mul(_mm_sub_ps(splat(3.0f), mul(mul(r, x2), r)), mul(r, splat(0.5f))), x2);
    return _mm_andnot_ps(le0, t);
}

static inline void hkPenetration_copyVertex(hkcdWingedEdge::Vertex& to, const hkcdWingedEdge::Vertex& from)
{
    to.m_position = from.m_position;
    to.m_pointAinA = from.m_pointAinA;
    to.m_pointBinX = from.m_pointBinX;
    to.m_referee = from.m_referee;
}

// 0x00E882E0
// The better of the supporting points along direction and against it (the larger distance from 'from' along its
// direction); result holds the one against it unless the other is at least as good.
void hkpConvexPenetrationUtil::findBestProjection(const hkVector4f& direction, const hkVector4f& from, hkcdWingedEdge::Vertex& result, hkSimdFloat32& bestDistanceOut)
{
    hkVector4f negDirection;
    negDirection.m_quad = neg(direction.m_quad);
    hkcdWingedEdge::Vertex positive;
    getPointForSimplex(direction, positive);
    getPointForSimplex(negDirection, result);
    const Q d1 = dot3(sub(positive.m_position.m_quad, from.m_quad), direction.m_quad);
    const Q d2 = dot3(sub(result.m_position.m_quad, from.m_quad), negDirection.m_quad);
    bestDistanceOut.m_real = _mm_max_ps(d1, d2);
    if (greaterEqual(d1, d2))
    {
        hkPenetration_copyVertex(result, positive);
    }
}

// 0x00E8A210
// The first vertex, along -aTb.translation (y for coincident origins); true (output set: distance, normal, point on
// A) when that point is the origin within the tolerance.
hkBool hkpConvexPenetrationUtil::case0To1(hkpGskOut& output)
{
    const Q t = m_aTb.getTranslation().m_quad;
    const Q negT = neg(t);
    const Q eps2 = splat(m_epsTolSqrd);
    const Q coincident = _mm_cmple_ps(hadd3(mul(t, t)), eps2);
    hkVector4f direction;
    direction.m_quad = _mm_or_ps(_mm_andnot_ps(coincident, negT), _mm_and_ps(coincident, g_vectorfConstants[HK_QUADREAL_0100]));
    hkcdWingedEdge::Vertex v;
    getPointForSimplex(direction, v);
    m_simplex.val()[0] = v.m_position;
    m_pointsAinA.val()[0] = v.m_pointAinA;
    m_pointsBinB.val()[0] = v.m_pointBinX;
    m_simplexSize = 1;
    const Q l2 = dot3(m_simplex.val()[0].m_quad, m_simplex.val()[0].m_quad);
    if (greater(l2, eps2))
    {
        return false;
    }
    output.m_distance = lane0(hkPenetration_length(l2));
    output.m_normalInA = direction;
    output.m_pointAinA = v.m_pointAinA;
    return true;
}

// 0x00E8A350: the second vertex along or against the first, whichever is further from it
void hkpConvexPenetrationUtil::case1To2()
{
    hkVector4f* simplex = m_simplex;
    hkcdWingedEdge::Vertex v1;
    getPointForSimplex(simplex[0], v1);
    const Q d1 = mul(sub(v1.m_position.m_quad, simplex[0].m_quad), sub(v1.m_position.m_quad, simplex[0].m_quad));
    hkVector4f negDirection;
    negDirection.m_quad = neg(simplex[0].m_quad);
    hkcdWingedEdge::Vertex v2;
    getPointForSimplex(negDirection, v2);
    const Q l1 = hadd3(d1);
    const Q e2 = sub(v2.m_position.m_quad, m_simplex.val()[0].m_quad);
    const Q l2 = hadd3(mul(e2, e2));
    const hkcdWingedEdge::Vertex& v = lessOrNaN(l1, l2) ? v2 : v1;
    m_simplex.val()[1] = v.m_position;
    m_pointsAinA.val()[1] = v.m_pointAinA;
    m_pointsBinB.val()[1] = v.m_pointBinX;
    m_simplexSize = 2;
}

// 0x00E8AA40: the third vertex, the best projection perpendicular to the edge (two perpendicular directions)
void hkpConvexPenetrationUtil::case2To3()
{
    hkVector4f* simplex = m_simplex;
    const Q d = sub(simplex[0].m_quad, simplex[1].m_quad);
    const Q perp = hkPenetration_perpendicular(d);
    const Q c = cross(d, perp);
    hkVector4f dir1;
    dir1.m_quad = mul(invSqrt23SetZero(dot3(perp, perp)), perp);
    hkVector4f dir2;
    dir2.m_quad = mul(invSqrt23SetZero(dot3(c, c)), c);
    hkcdWingedEdge::Vertex v1;
    hkSimdFloat32 best1;
    findBestProjection(dir1, simplex[0], v1, best1);
    hkcdWingedEdge::Vertex v2;
    hkSimdFloat32 best2;
    findBestProjection(dir2, m_simplex.val()[0], v2, best2);
    const hkcdWingedEdge::Vertex& v = greater(best1.m_real, best2.m_real) ? v1 : v2;
    m_simplex.val()[2] = v.m_position;
    m_pointsAinA.val()[2] = v.m_pointAinA;
    m_pointsBinB.val()[2] = v.m_pointBinX;
    m_simplexSize = 3;
}

// 0x00E8A460: the fourth vertex along the triangle normal; HK_FAILURE when it is not beyond the triangle
hkResult hkpConvexPenetrationUtil::case3To4(const hkVector4f& normal)
{
    hkcdWingedEdge::Vertex v;
    hkSimdFloat32 best;
    findBestProjection(normal, m_simplex.val()[0], v, best);
    if (lessOrNaN(best.m_real, bcast<0>(m_epsTol.m_quad)))
    {
        return HK_FAILURE;
    }
    m_simplex.val()[3] = v.m_position;
    m_pointsAinA.val()[3] = v.m_pointAinA;
    m_pointsBinB.val()[3] = v.m_pointBinX;
    m_simplexSize = 4;
    return HK_SUCCESS;
}

// 0x00E87FF0
void hkpConvexPenetrationUtil::codeBegin()
{
}

// 0x00E88080
// A vertex of A - B: A's supporting vertex along the direction minus B's against it (in A's space).
void hkpConvexPenetrationUtil::getPointForSimplex(const hkVector4f& direction, hkcdWingedEdge::Vertex& result)
{
    m_shapeA->getSupportingVertex(direction, result.m_pointAinA);
    hkVector4f directionB;
    directionB.m_quad = rotateInverse(m_aTb.getRotation(), neg(direction.m_quad));
    m_shapeB->getSupportingVertex(directionB, result.m_pointBinX);
    const Q bInA = transformPos(m_aTb, result.m_pointBinX.m_quad);
    result.m_position.m_quad = sub(result.m_pointAinA.m_quad, bInA);
}

// 0x00E883B0
// Adds newVertex to the polytope: HK_FAILURE with OK when it is not beyond the nearest triangle by the tolerance
// (converged), with OUT_OF_MEMORY when the triangles run out, with UNSOLVABLE when the visibility or the patch fails.
hkResult hkpConvexPenetrationUtil::tryToExpandPolytope(hkcdWingedEdge& we, hkcdWingedEdge::Triangle* nearestTriangle, hkcdWingedEdge::Vertex* newVertex, hkpGskStatus& failureReason)
{
    {
        const Q v = newVertex->m_position.m_quad;
        hkVector4f r0;
        r0.m_quad = sub(v, nearestTriangle->m_edge[0].m_vertex->m_position.m_quad);
        hkVector4f r1;
        r1.m_quad = sub(v, nearestTriangle->m_edge[1].m_vertex->m_position.m_quad);
        hkVector4f r2;
        r2.m_quad = sub(v, nearestTriangle->m_edge[2].m_vertex->m_position.m_quad);
        HK_TRANSPOSE3f(r0, r1, r2);
        const Q n = nearestTriangle->m_normal.m_quad;
        const Q dots = add(add(mul(r1.m_quad, bcast<1>(n)), mul(bcast<0>(n), r0.m_quad)), mul(r2.m_quad, bcast<2>(n)));
        if (_mm_movemask_ps(_mm_cmplt_ps(dots, m_epsTol.m_quad)) & 7)
        {
            failureReason = HK_GSK_OK;
            return HK_FAILURE;
        }
    }
    we.initialiseVibisilityTolerance();
    hkPadSpu<hkcdWingedEdge::Edge*> edgeChain = nullptr;
    hkPadSpu<int> chainLength = 0;
    nearestTriangle->m_state = nearestTriangle->calcStateAndUpdateTolerance(newVertex->m_position, we.m_toleranceBorder, we.m_previousMinimumDistance);
    if (nearestTriangle->m_state != hkcdWingedEdge::Triangle::HKCD_VISIBLE)
    {
        failureReason = HK_GSK_PD_UNSOLVABLE;
        return HK_FAILURE;
    }
    if (we.findVisibleTriangles(nearestTriangle, newVertex->m_position, chainLength, edgeChain) != hkcdWingedEdge::HKCD_WINGED_EDGE_OK)
    {
        failureReason = HK_GSK_PD_UNSOLVABLE;
        return HK_FAILURE;
    }
    if (we.numFreeTriangles() < chainLength)
    {
        failureReason = HK_GSK_PD_OUT_OF_MEMORY;
        return HK_FAILURE;
    }
    hkcdWingedEdge::Triangle* newTriangles[hkcdWingedEdge::HKCD_WINGED_EDGE_TRIANGLE_LIMIT];
    we.allocateTriangles(chainLength, newTriangles);
    if (we.initializePolytopeExpansion(newTriangles, chainLength, edgeChain, newVertex) != hkcdWingedEdge::HKCD_WINGED_EDGE_OK)
    {
        failureReason = HK_GSK_PD_UNSOLVABLE;
        return HK_FAILURE;
    }
    return HK_SUCCESS;
}

// 0x00E881A0
// The polytope's nearest triangle as the feature: up to 10 steps to the triangle containing the origin's projection
// (barycentric coordinates all non-negative), then its vertices become the simplex and the output (point on A by the
// barycentric weights, the normal and distance negated).
void hkpConvexPenetrationUtil::convertResultToFeature(hkcdWingedEdge& polytope, hkcdWingedEdge::Triangle* nearestTriangle, hkpGskOut& output)
{
    const hkVector4f zero(_mm_setzero_ps());
    hkVector4f bary;
    for (int i = 0; i < 10; i++)
    {
        hkVector4f* simplex = m_simplex;
        simplex[0] = nearestTriangle->m_edge[0].m_vertex->m_position;
        m_simplex.val()[1] = nearestTriangle->m_edge[1].m_vertex->m_position;
        m_simplex.val()[2] = nearestTriangle->m_edge[2].m_vertex->m_position;
        simplex = m_simplex;
        hkcdTriangleUtil::calcBarycentricCoordinates(hkVector4f(g_vectorfConstants[HK_QUADREAL_0]), simplex[0], simplex[1], simplex[2], bary);
        if (!(_mm_movemask_ps(_mm_cmplt_ps(bary.m_quad, zero.m_quad)) & 7))
        {
            break;
        }
        nearestTriangle = polytope.findTriangleNearestOrigin(nearestTriangle);
    }
    for (int k = 0; k < 3; k++)
    {
        m_pointsAinA.val()[k] = nearestTriangle->m_edge[k].m_vertex->m_pointAinA;
        m_pointsBinB.val()[k] = nearestTriangle->m_edge[k].m_vertex->m_pointBinX;
    }
    m_simplexSize = 3;
    const hkcdVertex* a = m_pointsAinA;
    const Q b = bary.m_quad;
    output.m_pointAinA.m_quad = add(add(mul(bcast<1>(b), a[1].m_quad), mul(bcast<0>(b), a[0].m_quad)), mul(bcast<2>(b), a[2].m_quad));
    output.m_distance = -nearestTriangle->m_distanceToOrigin;
    output.m_normalInA.m_quad = neg(nearestTriangle->m_normal.m_quad);
}

// 0x00E88800
// Grows the simplex to a tetrahedron containing the origin. Each size checks its simplex and either grows it
// (caseNTo(N+1)) or falls back to the smaller size; 'last' (the highest size grown from) stops falling back below a
// size already passed. HK_SUCCESS with a tetrahedron. HK_FAILURE with OK when the origin is on the simplex (the
// output is the contact then), with UNSOLVABLE otherwise.
hkResult hkpConvexPenetrationUtil::builtTetrahedron(hkpGskOut& output, hkpGskStatus& gjkStatus)
{
    const Q eps2 = splat(m_epsTolSqrd);
    int last = -1;
    hkVector4f normal;
    switch (m_simplexSize)
    {
    case 0:
        goto size0;
    case 1:
        goto size1;
    case 2:
        goto size2;
    case 3:
        goto size3;
    case 4:
        goto size4;
    default:
        gjkStatus = HK_GSK_PD_UNSOLVABLE;
        return HK_FAILURE;
    }

size4:
    {
        const hkVector4f* s = m_simplex;
        const Q n = cross(sub(s[0].m_quad, s[1].m_quad), sub(s[1].m_quad, s[2].m_quad));
        normal.m_quad = n;
        const Q d = dot3(sub(s[3].m_quad, s[0].m_quad), n);
        const Q l = mul(dot3(n, n), eps2);
        if (greater(mul(d, d), l))
        {
            gjkStatus = HK_GSK_OK;
            return HK_SUCCESS;
        }
        if (last >= 3)
        {
            gjkStatus = HK_GSK_PD_UNSOLVABLE;
            return HK_FAILURE;
        }
    }
    m_simplexSize = 3;
size3:
    {
        const hkVector4f* s = m_simplex;
        const Q n = cross(sub(s[0].m_quad, s[1].m_quad), sub(s[1].m_quad, s[2].m_quad));
        normal.m_quad = n;
        const Q l = dot3(n, n);
        // ucomiss + jnz: a NaN area counts as zero
        if (lane0(l) != 0.0f && lane0(l) == lane0(l))
        {
            goto grow3To4;
        }
        if (last >= 2)
        {
            goto edgeContact;
        }
    }
    m_simplexSize = 2;
size2:
    {
        const hkVector4f* s = m_simplex;
        const Q e = sub(s[0].m_quad, s[1].m_quad);
        if (greater(dot3(e, e), eps2))
        {
            last = 2;
            case2To3();
            m_simplexSize = 3;
            goto size3;
        }
        if (last >= 1)
        {
            gjkStatus = HK_GSK_PD_UNSOLVABLE;
            return HK_FAILURE;
        }
    }
    m_simplexSize = 1;
size1:
    {
        const hkVector4f* s = m_simplex;
        if (greater(dot3(s[0].m_quad, s[0].m_quad), eps2))
        {
            last = 1;
            case1To2();
            m_simplexSize = 2;
            goto size2;
        }
        if (last >= 0)
        {
            gjkStatus = HK_GSK_PD_UNSOLVABLE;
            return HK_FAILURE;
        }
    }
    m_simplexSize = 0;
size0:
    if (case0To1(output))
    {
        gjkStatus = HK_GSK_OK;
        return HK_FAILURE;
    }
    last = 0;
    goto size1;

grow3To4:
    {
        last = 3;
        const Q n = normal.m_quad;
        normal.m_quad = mul(invSqrt23SetZero(dot3(n, n)), n);
        if (case3To4(normal) == HK_SUCCESS)
        {
            goto size4;
        }
        // the origin is on the triangle's plane: the contact is in the triangle
        output.m_distance = 0.0f;
        output.m_normalInA = normal;
        const hkVector4f* s = m_simplex;
        hkVector4f bary;
        hkcdTriangleUtil::calcBarycentricCoordinates(hkVector4f(g_vectorfConstants[HK_QUADREAL_0]), s[0], s[1], s[2], bary);
        const hkcdVertex* a = m_pointsAinA;
        const Q b = bary.m_quad;
        output.m_pointAinA.m_quad = add(add(mul(bcast<1>(b), a[1].m_quad), mul(bcast<0>(b), a[0].m_quad)), mul(bcast<2>(b), a[2].m_quad));
        gjkStatus = HK_GSK_OK;
        return HK_FAILURE;
    }

edgeContact:
    {
        // a degenerate triangle after an edge: the contact is on the edge, at |s0|_1 / (2 |s0|_1 + eps)
        output.m_distance = 0.0f;
        const hkVector4f* s = m_simplex;
        const Q perp = hkPenetration_perpendicular(sub(s[0].m_quad, s[1].m_quad));
        output.m_normalInA.m_quad = perp;
        output.m_normalInA.m_quad = mul(invSqrt23SetZero(dot3(perp, perp)), perp);
        const Q l1 = hadd3(abs(m_simplex.val()[0].m_quad));
        const Q t = mul(recip23(add(add(l1, l1), g_vectorfConstants[HK_QUADREAL_EPS])), l1);
        const hkcdVertex* a = m_pointsAinA;
        output.m_pointAinA.m_quad = add(mul(sub(a[1].m_quad, a[0].m_quad), t), a[0].m_quad);
        gjkStatus = HK_GSK_OK;
        return HK_FAILURE;
    }
}

// 0x00E890D0
// The expanding polytope search; see the file comment.
hkpGskStatus hkpConvexPenetrationUtil::calculatePenetrationDepth(hkpGskOut& output)
{
    for (;;)
    {
        hkpGskStatus status = HK_GSK_OK;
        if (builtTetrahedron(output, status) == HK_FAILURE)
        {
            if (status != HK_GSK_PD_UNSOLVABLE)
            {
                return status;
            }
        }
        else
        {
            hkcdWingedEdge we;
            we.m_numVertices = 4;
            for (int i = 0; i < 4; i++)
            {
                we.m_vertex[i].m_position = m_simplex.val()[i];
                we.m_vertex[i].m_pointAinA = m_pointsAinA.val()[i];
                we.m_vertex[i].m_pointBinX = m_pointsBinB.val()[i];
                we.m_vertex[i].m_referee = nullptr;
            }
            we.addTetrahedron();
            we.m_numFreeTriangle = 0;
            for (;;)
            {
                hkcdWingedEdge::Triangle* nearest = we.findTriangleOnNearestPlane();
                hkcdWingedEdge::Vertex* v = we.allocateVertex();
                getPointForSimplex(nearest->m_normal, *v);
                if (tryToExpandPolytope(we, nearest, v, status) == HK_FAILURE)
                {
                    if (status == HK_GSK_PD_UNSOLVABLE)
                    {
                        break;
                    }
                    convertResultToFeature(we, nearest, output);
                    return status;
                }
                if (we.m_numVertices >= hkcdWingedEdge::HKCD_WINGED_EDGE_VERTICIES_LIMIT)
                {
                    convertResultToFeature(we, nearest, output);
                    return HK_GSK_PD_OUT_OF_MEMORY;
                }
            }
            status = HK_GSK_PD_UNSOLVABLE;
        }

        // unsolvable: restart from one vertex, the second time onwards with the transform jittered by up to 1e-4
        m_backupTries += 1;
        if (m_backupTries == 1)
        {
            m_simplexSize = 1;
            continue;
        }
        if (m_backupTries >= 20)
        {
            return status;
        }
        m_simplexSize = 0;
        {
            // rand01 (an LCG of the try count), then randomVector: (r + min) * (max - min), as in the original
            const unsigned int seed = (unsigned int)m_backupTries.val();
            const int bits = (int)((0x3039u - seed * 0x3E39B193u) & 0x7FFFFFFFu);
            const float r = (float)bits * 4.656612873077393e-10f; // 0x30000000: 2^-31
            const Q minV = splat(-0.0001f); // 0xB8D1B717
            const Q maxV = splat(0.0001f); // 0x38D1B717
            const Q jitter = mul(add(splat(r), minV), sub(maxV, minV));
            m_aTb.m_translation.m_quad = add(jitter, m_aTb.m_translation.m_quad);
        }
    }
}

// 0x00E88040
void hkpConvexPenetrationUtil::codeEnd()
{
}

// 0x00E89370
// The penetration depth (or closest points) of two convex shapes from a simplex of up to four vertex pairs; the
// unsolvable case falls back to heuristic sampling (HK_GSK_PD_HEURISTIC_SAMPLING).
hkpGskStatus hkCalculatePenetrationDepth(const hkpConvexShape* shapeA, const hkpConvexShape* shapeB, const hkTransformf& btoa, float epsTol, hkcdVertex* pointsAinA, hkcdVertex* pointsBinB, hkVector4f* simplex, int simplexSize, hkpGskOut& output)
{
    hkpConvexPenetrationUtil util;
    util.init(shapeA, shapeB, btoa, epsTol, pointsAinA, pointsBinB, simplex, simplexSize);
    hkpGskStatus status = util.calculatePenetrationDepth(output);
    if (status == HK_GSK_PD_UNSOLVABLE)
    {
        hkpCvxCvxDistByHeuristicSampling(shapeA, shapeB, btoa, pointsAinA, pointsBinB, output);
        status = HK_GSK_PD_HEURISTIC_SAMPLING;
    }
    return status;
}

// 0x00E8A190 (.h:39)
void hkpConvexPenetrationUtil::init(const hkpConvexShape* shapeA, const hkpConvexShape* shapeB, const hkTransformf& aTb, float epsTolerance, hkcdVertex* pointsAinA, hkcdVertex* pointsBinB, hkVector4f* simplex, int simplexSize)
{
    m_aTb = aTb;
    m_shapeA = shapeA;
    m_shapeB = shapeB;
    m_pointsAinA = pointsAinA;
    m_pointsBinB = pointsBinB;
    m_epsTol.m_quad = splat(epsTolerance);
    m_epsTolSqrd = epsTolerance * epsTolerance;
    m_backupTries = 0;
    m_simplex = simplex;
    m_simplexSize = simplexSize;
}

namespace
{
// The work block of one shape B in hkCalcMultiPenetrationDepth (0x3000 bytes of the LIFO each): the penetration
// util with its vertex arrays, the shape and its output point, the polytope and its nearest triangle.
struct alignas(16) hkMultiPenetrationBlock
{
    hkpConvexPenetrationUtil m_util; // 0x0000
    hkcdVertex m_pointsAinA[4]; // 0x0070
    hkcdVertex m_pointsBinB[4]; // 0x00B0
    hkVector4f m_simplex[4]; // 0x00F0
    const hkpConvexShape* m_shapeB; // 0x0130
    hkContactPoint* m_point; // 0x0134
    hkcdWingedEdge m_polytope; // 0x0140
    hkcdWingedEdge::Triangle* m_nearest; // 0x2FF0
};
static_assert(sizeof(hkMultiPenetrationBlock) == 0x3000, "hkMultiPenetrationBlock");
}

// 0x00E894A0
// A common penetration of shape A with two shapes B (the conflicting children of hkAgent1nMachine_Weld): a polytope
// per pair from its tetrahedron, then the pair of triangles closest together (hkcdWingedEdge::find2ClosestMultiTriangles)
// gives one direction; each output point is moved to its pair's feature on it, its normal the direction, its distance
// the common one less both radii.
// Original behaviour kept:
// - a pair whose tetrahedron fails takes the next pair's place in the count but not in the order: the loop ends one
//   pair earlier, so the following pair is never tried.
// - the polytopes only grow when their vertex arrays are full (the free vertex check is inverted), so the closest pair
//   comes from the tetrahedra.
hkResult hkCalcMultiPenetrationDepth(const hkTransformf& transformA, const hkpConvexShape* shapeA, const hkpConvexShape** shapeBs, int numShapeBs, const hkTransformf& aTb, hkContactPoint** pointsOut)
{
    hkLifoAllocator& stack = hkMemoryRouter::getInstance().stack();
    const int bytes = numShapeBs * (int)sizeof(hkMultiPenetrationBlock);
    hkMultiPenetrationBlock* blocks = static_cast<hkMultiPenetrationBlock*>(stack.fastBlockAlloc(bytes));
    hkpGskOut out;
    hkpGskStatus status = HK_GSK_OK;
    int numPairs = numShapeBs;
    hkMultiPenetrationBlock* block = blocks;
    for (int i = 0; i < numPairs; i++)
    {
        block->m_shapeB = shapeBs[i];
        block->m_point = pointsOut[i];
        hkpConvexPenetrationUtil& util = block->m_util;
        util.m_aTb = aTb;
        util.m_shapeB = shapeBs[i];
        util.m_shapeA = shapeA;
        util.m_backupTries = 0;
        util.m_simplexSize = 0;
        util.m_pointsAinA = block->m_pointsAinA;
        util.m_pointsBinB = block->m_pointsBinB;
        util.m_simplex = block->m_simplex;
        util.m_epsTol.m_quad = _mm_castsi128_ps(_mm_set1_epi32(0x38D1B717)); // 0.0001
        util.m_epsTolSqrd = hkGskExact::lane0(_mm_castsi128_ps(_mm_set1_epi32(0x322BCC76))); // 1e-8, one ulp below 0x322BCC77
        if (util.builtTetrahedron(out, status) == HK_FAILURE)
        {
            numPairs--;
            continue;
        }
        hkcdWingedEdge& we = block->m_polytope;
        we.m_numVertices = 4;
        for (int k = 0; k < 4; k++)
        {
            we.m_vertex[k].m_pointAinA = block->m_pointsAinA[k];
            we.m_vertex[k].m_pointBinX = block->m_pointsBinB[k];
            we.m_vertex[k].m_position = block->m_simplex[k];
            we.m_vertex[k].m_referee = nullptr;
        }
        we.addTetrahedron();
        we.m_numFreeTriangle = 0;
        block++;
    }
    if (numPairs > 2)
    {
        numPairs = 2;
    }
    if (numPairs < 2)
    {
        stack.fastBlockFree(blocks, bytes);
        return hkResult(HK_FAILURE);
    }

    hkpGskStatus pairStatus[2] = { HK_GSK_OK, HK_GSK_OK };
    hkVector4f direction;
    bool expanded;
    do
    {
        hkcdWingedEdge::find2ClosestMultiTriangles(blocks[0].m_polytope, blocks[1].m_polytope, direction, blocks[0].m_nearest, blocks[1].m_nearest);
        expanded = false;
        for (int i = 0; i < numPairs; i++)
        {
            hkMultiPenetrationBlock& b = blocks[i];
            if (pairStatus[i] == HK_GSK_PD_OUT_OF_MEMORY || b.m_polytope.m_numVertices < hkcdWingedEdge::HKCD_WINGED_EDGE_VERTICIES_LIMIT)
            {
                continue;
            }
            hkcdWingedEdge::Vertex* v = &b.m_polytope.m_vertex[b.m_polytope.m_numVertices];
            b.m_polytope.m_numVertices += 1;
            v->m_referee = nullptr;
            b.m_util.getPointForSimplex(b.m_nearest->m_normal, *v);
            if (b.m_util.tryToExpandPolytope(b.m_polytope, b.m_nearest, v, pairStatus[i]) == HK_FAILURE)
            {
                if (pairStatus[i] == HK_GSK_PD_UNSOLVABLE)
                {
                    stack.fastBlockFree(blocks, bytes);
                    return hkResult(HK_FAILURE);
                }
            }
            else
            {
                expanded = true;
            }
        }
    } while (expanded);

    const Q negDirection = neg(direction.m_quad);
    const hkMatrix3f& rotA = transformA.getRotation();
    const Q normal = _mm_add_ps(_mm_add_ps(_mm_mul_ps(bcast<1>(negDirection), rotA.getColumn<1>().m_quad), _mm_mul_ps(bcast<0>(negDirection), rotA.getColumn<0>().m_quad)), _mm_mul_ps(bcast<2>(negDirection), rotA.getColumn<2>().m_quad));
    const Q distance = _mm_sub_ps(_mm_sub_ps(_mm_setzero_ps(), bcast<3>(direction.m_quad)), splat(shapeA->m_radius));
    for (int i = 0; i < numPairs; i++)
    {
        hkMultiPenetrationBlock& b = blocks[i];
        b.m_util.convertResultToFeature(b.m_polytope, b.m_nearest, out);
        const Q p = out.m_pointAinA.m_quad;
        const Q pos = _mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(bcast<0>(p), rotA.getColumn<0>().m_quad), _mm_mul_ps(bcast<1>(p), rotA.getColumn<1>().m_quad)), _mm_mul_ps(bcast<2>(p), rotA.getColumn<2>().m_quad)), transformA.getTranslation().m_quad);
        b.m_point->m_position.m_quad = pos;
        b.m_point->m_separatingNormal.m_quad = setXYZ_W(normal, _mm_sub_ps(distance, splat(b.m_shapeB->m_radius)));
    }
    stack.fastBlockFree(blocks, bytes);
    return hkResult(HK_SUCCESS);
}
