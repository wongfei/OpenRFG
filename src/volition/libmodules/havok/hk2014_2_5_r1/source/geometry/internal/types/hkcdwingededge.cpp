// Original: volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdwingededge.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the winged-edge polytope of the expanding polytope penetration depth search
// (hkpConvexPenetrationUtil), from rfg.exe. Vertices hold a point of the Minkowski difference A - B with the support
// points it came from; triangles hold three edges (vertex, next edge in the triangle, the neighbour triangle's twin
// edge), the outward unit normal and the plane's distance to the origin. The .inl members with out-of-line copies
// are written here too; no SDK reference exists.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdwingededge.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

// 0x01367A30 (topologyData): the four triangles of the initial tetrahedron, three edges each: the edge's vertex, and
// the triangle and edge of its neighbour twin.
static const int hkcdWingedEdge_topology[4][3][3] = {
    {{0, 2, 2}, {2, 3, 0}, {1, 1, 0}},
    {{0, 0, 2}, {1, 3, 2}, {3, 2, 0}},
    {{0, 1, 2}, {3, 3, 1}, {2, 0, 0}},
    {{1, 0, 1}, {2, 2, 1}, {3, 1, 1}},
};

// 0x00F0CDF0
void hkcdWingedEdge::codeBegin()
{
}

// 0x00F0CE70
// Walks the triangles visible from the point of view (flood fill over the neighbours of initialTriangle): every edge
// to an invisible triangle is a boundary edge (counted, the newest in edgeChain, and its vertex refers to it, which
// links the boundary into a chain). A triangle of unknown visibility fails the expansion. Visited visible triangles
// go to the free list (distance HK_REAL_MAX); a full free list fails too.
hkcdWingedEdge::WingedEdgeStatus hkcdWingedEdge::findVisibleTriangles(Triangle* initialTriangle, const hkVector4f& pointOfView, hkPadSpu<int>& chainLength, hkPadSpu<Edge*>& edgeChain)
{
    WingedEdgeStatus status = HKCD_WINGED_EDGE_OK;
    for (int i = 0; i < 3; i++)
    {
        Edge* twin = initialTriangle->m_edge[i].m_neighbour;
        Triangle* neighbour = twin->m_triangle;
        Triangle::TriangleState state = neighbour->m_state;
        if (state == Triangle::HKCD_NOT_VISITED)
        {
            state = neighbour->calcStateAndUpdateTolerance(pointOfView, m_toleranceBorder, m_previousMinimumDistance);
            neighbour->m_state = state;
            if (state == Triangle::HKCD_VISIBLE)
            {
                status = findVisibleTriangles(neighbour, pointOfView, chainLength, edgeChain);
                if (status == HKCD_WINGED_EDGE_FAIL)
                {
                    break;
                }
                continue;
            }
            if (state == Triangle::HKCD_UNKNOWN)
            {
                status = HKCD_WINGED_EDGE_FAIL;
                break;
            }
        }
        if (state == Triangle::HKCD_INVISIBLE)
        {
            twin->m_vertex->m_referee = twin;
            chainLength += 1;
            edgeChain = twin;
        }
    }
    initialTriangle->m_distanceToOrigin = HK_REAL_MAX;
    if (m_numFreeTriangle >= HKCD_WINGED_EDGE_TRIANGLE_LIMIT)
    {
        return HKCD_WINGED_EDGE_FAIL;
    }
    m_freeTriangle[m_numFreeTriangle] = initialTriangle;
    m_numFreeTriangle += 1;
    return status;
}

// 0x00F0D550
// The tetrahedron of the first four vertices, oriented so its normals point outwards (vertices 0 and 1 swap where
// vertex 0 is on the positive side of triangle 3).
void hkcdWingedEdge::addTetrahedron()
{
    m_numTriangles = 4;
    for (int t = 0; t < 4; t++)
    {
        Triangle& tri = m_triangles[t];
        tri.m_edge[0].m_next = &tri.m_edge[1];
        tri.m_edge[1].m_next = &tri.m_edge[2];
        tri.m_state = Triangle::HKCD_NOT_VISITED;
        tri.m_edge[2].m_next = &tri.m_edge[0];
        for (int e = 0; e < 3; e++)
        {
            const int* topo = hkcdWingedEdge_topology[t][e];
            tri.m_edge[e].m_vertex = &m_vertex[topo[0]];
            tri.m_edge[e].m_triangle = &tri;
            tri.m_edge[e].m_neighbour = &m_triangles[topo[1]].m_edge[topo[2]];
        }
    }
    Triangle& tri3 = m_triangles[3];
    tri3.calculateTriangleData();
    const Q d = dot3(sub(m_vertex[0].m_position.m_quad, m_vertex[1].m_position.m_quad), tri3.m_normal.m_quad);
    if (_mm_movemask_ps(d) == 0)
    {
        m_triangles[2].m_edge[0].m_vertex = &m_vertex[1];
        m_triangles[1].m_edge[0].m_vertex = &m_vertex[1];
        m_triangles[0].m_edge[0].m_vertex = &m_vertex[1];
        m_triangles[3].m_edge[0].m_vertex = &m_vertex[0];
        m_triangles[1].m_edge[1].m_vertex = &m_vertex[0];
        m_triangles[0].m_edge[2].m_vertex = &m_vertex[0];
    }
    for (int t = 0; t < 4; t++)
    {
        m_triangles[t].calculateTriangleData();
    }
}

// 0x00F0D130
// The unit normal of (v0 - v1) x (v1 - v2) and the plane distance of v0.
void hkcdWingedEdge::Triangle::calculateTriangleData()
{
    const Q v0 = m_edge[0].m_vertex->m_position.m_quad;
    const Q v1 = m_edge[1].m_vertex->m_position.m_quad;
    const Q v2 = m_edge[2].m_vertex->m_position.m_quad;
    const Q n = cross(sub(v0, v1), sub(v1, v2));
    m_normal.m_quad = mul(invSqrt23SetZero(dot3(n, n)), n);
    m_distanceToOrigin = lane0(dot3(v0, m_normal.m_quad));
}

// 0x00E8A4F0 (.inl:232): as calculateTriangleData from the given positions; HK_FAILURE for a degenerate triangle
hkResult hkcdWingedEdge::Triangle::calculateTriangleDataWithCheck(const hkVector4f& v1, const hkVector4f& v2, const hkVector4f& v3)
{
    const Q n = cross(sub(v1.m_quad, v2.m_quad), sub(v2.m_quad, v3.m_quad));
    const Q len2 = dot3(n, n);
    if (!greater(len2, _mm_setzero_ps()))
    {
        return HK_FAILURE;
    }
    const Q unit = mul(invSqrt23SetZero(len2), n);
    m_distanceToOrigin = lane0(dot3(v1.m_quad, unit));
    m_normal.m_quad = unit;
    return HK_SUCCESS;
}

// 0x00F0CE00
void hkcdWingedEdge::codeEnd()
{
}

// 0x00F0CFC0
// From a triangle on the nearest plane, walks over edges whose plane through the origin separates (the edge's
// outward plane has the origin within -1e-10 of it, scaled) towards the triangle whose face contains the origin's
// projection, as long as the neighbour is not more than 10% nearer than the start triangle (when that is in front of
// the origin). Stops at a triangle visited before.
hkcdWingedEdge::Triangle* hkcdWingedEdge::findTriangleNearestOrigin(Triangle* triangleOnNearestPlane)
{
    resetTriangleAndVertexStates();
    const Q eps = splat(-9.9999994e-11f); // 0xAEDBE6FE (one ulp above -1e-10f)
    Triangle* tri = triangleOnNearestPlane;
    const Edge* cameFrom = nullptr;
    bool visitedAgain = false;
    while (!visitedAgain)
    {
        tri->m_state = Triangle::HKCD_FIND_NEAREST_TRIANGLE_VISITED;
        int i = 0;
        for (; i < 3; i++)
        {
            const Edge* edge = &tri->m_edge[i];
            if (edge == cameFrom)
            {
                continue;
            }
            const Q p0 = edge->m_vertex->m_position.m_quad;
            const Q e = sub(edge->m_next->m_vertex->m_position.m_quad, p0);
            const Q c = cross(e, tri->m_normal.m_quad);
            const Q d = dot3(c, p0);
            if (greaterEqual(signedSquare(d), mul(dot3(e, e), eps)))
            {
                continue;
            }
            Triangle* neighbour = edge->m_neighbour->m_triangle;
            const float startDistance = triangleOnNearestPlane->m_distanceToOrigin;
            if (startDistance > 0.0f && startDistance * 0.9f > neighbour->m_distanceToOrigin) // 0x3F666666
            {
                continue;
            }
            cameFrom = tri->m_edge[i].m_neighbour;
            visitedAgain = (neighbour->m_state == Triangle::HKCD_FIND_NEAREST_TRIANGLE_VISITED);
            tri = neighbour;
            break;
        }
        if (i == 3)
        {
            break;
        }
    }
    return tri;
}

// 0x00E89AF0 (.inl:89): the triangle whose plane is nearest to the origin (the first of equals)
hkcdWingedEdge::Triangle* hkcdWingedEdge::findTriangleOnNearestPlane()
{
    Triangle* best = &m_triangles[0];
    float bestDistance = m_triangles[0].m_distanceToOrigin;
    for (int i = 1; i < m_numTriangles; i++)
    {
        if (bestDistance > m_triangles[i].m_distanceToOrigin)
        {
            bestDistance = m_triangles[i].m_distanceToOrigin;
            best = &m_triangles[i];
        }
    }
    return best;
}

// 0x00E89BB0 (.inl:113)
void hkcdWingedEdge::resetTriangleAndVertexStates()
{
    for (int i = 0; i < m_numTriangles; i++)
    {
        m_triangles[i].m_state = Triangle::HKCD_NOT_VISITED;
    }
    for (int i = 0; i < m_numVertices; i++)
    {
        m_vertex[i].m_referee = nullptr;
    }
}

// 0x00E89C30 (.inl:201): recycled triangles first (newest freed first), then new ones
void hkcdWingedEdge::allocateTriangles(int numRequired, Triangle** newTrianglesBuffer)
{
    int out = 0;
    int freeIndex = m_numFreeTriangle - 1;
    while (numRequired != 0 && freeIndex >= 0)
    {
        newTrianglesBuffer[out++] = m_freeTriangle[freeIndex--];
        m_numFreeTriangle -= 1;
        numRequired--;
    }
    while (numRequired > 0)
    {
        newTrianglesBuffer[out++] = &m_triangles[m_numTriangles];
        m_numTriangles += 1;
        numRequired--;
    }
}

// 0x00E8A5E0 (.inl:253)
// Fans the new triangles from the point of view v over the boundary chain (each boundary edge's vertex refers to the
// next boundary edge); FAIL for a degenerate triangle, a broken chain or one that does not close.
hkcdWingedEdge::WingedEdgeStatus hkcdWingedEdge::initializePolytopeExpansion(Triangle** newTriangles, int numTriangles, Edge* edgeChain, Vertex*& v)
{
    Edge* edge = edgeChain;
    for (int i = 0; i < numTriangles; i++)
    {
        if (edge == nullptr)
        {
            return HKCD_WINGED_EDGE_FAIL;
        }
        Triangle* tri = newTriangles[i];
        Triangle* next = (i + 1 < numTriangles) ? newTriangles[i + 1] : newTriangles[0];
        tri->m_edge[0].m_triangle = tri;
        tri->m_edge[1].m_triangle = tri;
        tri->m_edge[2].m_triangle = tri;
        tri->m_edge[2].m_next = &tri->m_edge[0];
        tri->m_edge[0].m_next = &tri->m_edge[1];
        tri->m_edge[1].m_next = &tri->m_edge[2];
        Vertex* vB = edge->m_next->m_vertex;
        Vertex* vA = edge->m_vertex;
        tri->m_edge[0].m_vertex = vB;
        tri->m_edge[1].m_vertex = vA;
        tri->m_edge[2].m_vertex = v;
        edge->m_neighbour = &tri->m_edge[0];
        tri->m_edge[0].m_neighbour = edge;
        next->m_edge[1].m_neighbour = &tri->m_edge[2];
        tri->m_edge[2].m_neighbour = &next->m_edge[1];
        const Q pB = vB->m_position.m_quad;
        const Q n = cross(sub(pB, vA->m_position.m_quad), sub(vA->m_position.m_quad, v->m_position.m_quad));
        const Q len2 = dot3(n, n);
        if (!greater(len2, _mm_setzero_ps()))
        {
            return HKCD_WINGED_EDGE_FAIL;
        }
        const Q unit = mul(invSqrt23SetZero(len2), n);
        tri->m_distanceToOrigin = lane0(dot3(pB, unit));
        tri->m_normal.m_quad = unit;
        Vertex* chainVertex = edge->m_next->m_vertex;
        edge = chainVertex->m_referee;
        chainVertex->m_referee = nullptr;
    }
    resetTriangleAndVertexStates();
    return (edgeChain != edge) ? HKCD_WINGED_EDGE_FAIL : HKCD_WINGED_EDGE_OK;
}

// 0x00F0D1E0
// The pair of triangles, one of each polytope, whose planes' common closest point to the origin is nearest: for
// planes at distances d0, d1 with normals at cosine c (not more than opposed by -0.5), the point is s0 n0 + s1 n1 with
// the coefficients of the smaller plane's foot; a plane through the origin (distance within HK_QUADREAL_EPS) takes the
// other plane alone, or halves both. The direction out is the normalized combination of the best pair's normals,
// its w the distance (the root of the best squared distance).
// Original behaviour kept: the first triangle of we1 is never tried, and a NaN distance replaces the best.
void hkcdWingedEdge::find2ClosestMultiTriangles(hkcdWingedEdge& we0, hkcdWingedEdge& we1, hkVector4f& directionOut, Triangle*& nearestTriangle0, Triangle*& nearestTriangle1)
{
    const Q eps = hkVector4f::getConstant<HK_QUADREAL_EPS>().m_quad;
    const Q zero = hkVector4f::getConstant<HK_QUADREAL_0>().m_quad;
    const Q one = hkVector4f::getConstant<HK_QUADREAL_1>().m_quad;
    const Q half = hkVector4f::getConstant<HK_QUADREAL_INV_2>().m_quad;
    Q best = hkVector4f::getConstant<HK_QUADREAL_MAX>().m_quad;
    Q bestCoef0 = zero;
    Q bestCoef1 = zero;
    for (int i = we0.m_numTriangles - 1; i >= 0; i--)
    {
        Triangle* t0 = &we0.m_triangles[i];
        const Q d0 = splat(t0->m_distanceToOrigin);
        const Q d0Sqrd = _mm_mul_ps(d0, d0);
        if (lane0(d0Sqrd) > lane0(best))
        {
            continue;
        }
        for (int j = we1.m_numTriangles - 1; j >= 1; j--)
        {
            Triangle* t1 = &we1.m_triangles[j];
            const Q d1 = splat(t1->m_distanceToOrigin);
            const Q d1Sqrd = _mm_mul_ps(d1, d1);
            if (lane0(d1Sqrd) > lane0(best))
            {
                continue;
            }
            const Q c = dot3(t1->m_normal.m_quad, t0->m_normal.m_quad);
            if (lessOrNaN(c, _mm_sub_ps(zero, half)))
            {
                continue;
            }
            const Q a = _mm_sub_ps(d0, _mm_mul_ps(c, d1));
            const Q b = _mm_sub_ps(d1, _mm_mul_ps(c, d0));
            Q coef0;
            Q coef1;
            Q dist;
            if (lane0(a) > lane0(eps))
            {
                if (lane0(b) > lane0(eps))
                {
                    Q x2;
                    Q x4;
                    if (lane0(a) > lane0(b))
                    {
                        const Q la = _mm_mul_ps(recip23(a), b);
                        const Q s = recip23(_mm_add_ps(_mm_mul_ps(la, c), one));
                        coef0 = s;
                        x2 = _mm_mul_ps(s, d0);
                        coef1 = _mm_mul_ps(s, la);
                        x4 = _mm_mul_ps(la, x2);
                    }
                    else
                    {
                        const Q lb = _mm_mul_ps(recip23(b), a);
                        const Q s = recip23(_mm_add_ps(_mm_mul_ps(lb, c), one));
                        coef1 = s;
                        x4 = _mm_mul_ps(d1, s);
                        coef0 = _mm_mul_ps(lb, s);
                        x2 = _mm_mul_ps(lb, x4);
                    }
                    const Q along = _mm_add_ps(_mm_mul_ps(x4, c), x2);
                    const Q across = _mm_mul_ps(_mm_mul_ps(_mm_sub_ps(one, _mm_mul_ps(c, c)), x4), x4);
                    dist = _mm_add_ps(across, _mm_mul_ps(along, along));
                }
                else
                {
                    // B's plane through the origin
                    const Q m = _mm_cmplt_ps(zero, d1);
                    const Q h = _mm_andnot_ps(m, half);
                    coef1 = _mm_or_ps(_mm_and_ps(m, zero), h);
                    coef0 = _mm_or_ps(_mm_and_ps(m, one), h);
                    dist = _mm_or_ps(_mm_andnot_ps(m, _mm_mul_ps(_mm_sub_ps(zero, c), c)), _mm_and_ps(d0Sqrd, m));
                }
            }
            else
            {
                // A's plane through the origin
                const Q m = _mm_cmplt_ps(eps, d0);
                const Q h = _mm_andnot_ps(m, half);
                coef0 = _mm_or_ps(_mm_and_ps(m, zero), h);
                coef1 = _mm_or_ps(_mm_and_ps(m, one), h);
                dist = _mm_or_ps(_mm_andnot_ps(m, _mm_mul_ps(_mm_sub_ps(zero, c), c)), _mm_and_ps(d1Sqrd, m));
            }
            if (lessOrNaN(dist, best))
            {
                nearestTriangle0 = t0;
                bestCoef0 = coef0;
                bestCoef1 = coef1;
                nearestTriangle1 = t1;
                best = dist;
            }
        }
    }
    const Q v = _mm_add_ps(_mm_mul_ps(nearestTriangle1->m_normal.m_quad, bestCoef1), _mm_mul_ps(nearestTriangle0->m_normal.m_quad, bestCoef0));
    const Q dir = _mm_mul_ps(invSqrt23SetZero(dot3(v, v)), v);
    const Q m = _mm_max_ps(zero, best);
    const Q length = _mm_andnot_ps(_mm_cmple_ps(m, zero), _mm_mul_ps(invSqrt23(m), m));
    directionOut.m_quad = setXYZ_W(dir, length);
}
