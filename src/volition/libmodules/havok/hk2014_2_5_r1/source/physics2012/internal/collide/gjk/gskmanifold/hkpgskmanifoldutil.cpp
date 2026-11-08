// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/gskmanifold/hkpgskmanifoldutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the GSK agents' feature manifold, from rfg.exe. Each contact point stores the
// feature pair the GSK found (vertex ids of A and B); every frame the features are converted back to contact points
// from the shapes' current vertices (verifyAndGetPoints) and dropped when they separated or no longer describe a
// valid closest pair. A fifth point replaces the most redundant of the four (findRedundant5thPoint). No SDK reference
// exists; ported from the binary.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/gskmanifold/hkpgskmanifoldutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/contactmgr/hkpcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscdpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/hkpconvexshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/gskmanifold/hkpgskmanifold.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskcache.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

#include <string.h>

using namespace hkGskExact;

// size 0x30
struct hkGskManifoldConvertOut
{
    hkVector4f m_pointOnA; // 0x00
    hkVector4f m_AminusB; // 0x10 (along the normal: the distance before the radii)
    hkVector4f m_separatingNormal; // 0x20
};

// The results of the feature conversion (bit 0: a contact point; bit 4: a face or edge-edge feature, its projection
// tested; bit 2: an edge projection outside the edge; 10: beyond the radius sum or an unknown feature).
enum
{
    HK_GSK_CONVERT_POINT = 0x5,
    HK_GSK_CONVERT_OUTSIDE_EDGE = 0x6,
    HK_GSK_CONVERT_REJECT = 0xA,
    HK_GSK_CONVERT_FACE_POINT = 0x11,
    HK_GSK_CONVERT_FACE_OUTSIDE = 0x12,
};

static inline Q hkGskManifold_vertex(const hkpGskManifoldWork& work, unsigned char vertMult16)
{
    return work.getVertexA(vertMult16).m_quad;
}

// The three dot products (x + y) + z of a0.b0, a1.b1, a2.b2 in lanes 0-2 (lane 3: the w products of a2.b2 and
// a1.b1 summed, as the original's transposition leaves them; unused).
static inline Q hkGskManifold_dot3x3(Q p0, Q p1, Q p2)
{
    const Q lo = _mm_unpacklo_ps(p0, p1);
    const Q hi = _mm_unpackhi_ps(p0, p1);
    const Q x = _mm_movelh_ps(lo, p2);
    const Q y = _mm_shuffle_ps(_mm_movehl_ps(x, lo), p2, 0xD4);
    const Q z = _mm_shuffle_ps(hi, p2, 0xE4);
    return add(add(x, y), z);
}

// The four dot products (y + x) + z of the products p0..p3 (the dot3_4vs4 transposition).
static inline Q hkGskManifold_dot3x4(Q p0, Q p1, Q p2, Q p3)
{
    const Q a = _mm_shuffle_ps(p0, p1, 0x44);
    const Q b = _mm_shuffle_ps(p2, p3, 0x44);
    const Q z = _mm_shuffle_ps(_mm_shuffle_ps(p0, p1, 0xEE), _mm_shuffle_ps(p2, p3, 0xEE), 0x88);
    const Q x = _mm_shuffle_ps(a, b, 0x88);
    const Q y = _mm_shuffle_ps(a, b, 0xDD);
    return add(z, add(y, x));
}

// 0x00E817A0 (file static, register arguments)
static int hkGskManifold_convertPointPoint(const hkpGskManifoldWork& work, const hkpGskManifold::ContactPoint& feature, hkGskManifoldConvertOut& out)
{
    const Q a = hkGskManifold_vertex(work, feature.m_vert[0]);
    const Q d = sub(a, hkGskManifold_vertex(work, feature.m_vert[1]));
    if (greater(dot3(d, d), splat(work.m_radiusSumSqrd)))
    {
        return HK_GSK_CONVERT_REJECT;
    }
    out.m_AminusB.m_quad = d;
    out.m_separatingNormal = work.m_separatingNormal;
    out.m_pointOnA.m_quad = a;
    return HK_GSK_CONVERT_POINT;
}

// 0x00E81810 (file static, register arguments): a point of A against an edge of B
static int hkGskManifold_convertPointEdge(const hkpGskManifoldWork& work, const hkpGskManifold::ContactPoint& feature, hkGskManifoldConvertOut& out)
{
    const Q a = hkGskManifold_vertex(work, feature.m_vert[0]);
    out.m_pointOnA.m_quad = a;
    out.m_separatingNormal = work.m_separatingNormal;
    const Q b0 = hkGskManifold_vertex(work, feature.m_vert[1]);
    const Q b1 = hkGskManifold_vertex(work, feature.m_vert[2]);
    const Q toA = sub(b1, a);
    const Q edge = sub(b1, b0);
    const Q c = cross(edge, toA);
    // (b1 - a).e, (a - b0).e and |e x (b1 - a)|^2
    const Q dots = hkGskManifold_dot3x3(mul(toA, edge), mul(sub(a, b0), edge), mul(c, c));
    const Q e2 = dot3(edge, edge);
    if (greater(bcast<2>(dots), mul(e2, splat(work.m_radiusSumSqrd))))
    {
        return HK_GSK_CONVERT_REJECT;
    }
    if (_mm_movemask_ps(_mm_cmplt_ps(dots, _mm_setzero_ps())) & 3)
    {
        return HK_GSK_CONVERT_OUTSIDE_EDGE;
    }
    const Q d1 = bcast<0>(dots);
    const Q t = mul(recip23(add(bcast<1>(dots), d1)), d1);
    out.m_AminusB.m_quad = sub(a, add(mul(sub(b0, b1), t), b1));
    return HK_GSK_CONVERT_POINT;
}

// 0x00E81950 (file static, register arguments): an edge of A against a point of B
static int hkGskManifold_convertEdgePoint(const hkpGskManifoldWork& work, const hkpGskManifold::ContactPoint& feature, hkGskManifoldConvertOut& out)
{
    out.m_separatingNormal = work.m_separatingNormal;
    const Q a1 = hkGskManifold_vertex(work, feature.m_vert[1]);
    const Q a0 = hkGskManifold_vertex(work, feature.m_vert[0]);
    const Q b = hkGskManifold_vertex(work, feature.m_vert[2]);
    const Q edge = sub(a1, a0);
    const Q toB = sub(a1, b);
    const Q c = cross(edge, toB);
    const Q dots = hkGskManifold_dot3x3(mul(toB, edge), mul(sub(b, a0), edge), mul(c, c));
    const Q e2 = dot3(edge, edge);
    if (greater(bcast<2>(dots), mul(e2, splat(work.m_radiusSumSqrd))))
    {
        return HK_GSK_CONVERT_REJECT;
    }
    if (_mm_movemask_ps(_mm_cmplt_ps(dots, _mm_setzero_ps())) & 3)
    {
        return HK_GSK_CONVERT_OUTSIDE_EDGE;
    }
    const Q d1 = bcast<0>(dots);
    const Q t = mul(recip23(add(bcast<1>(dots), d1)), d1);
    const Q p = add(mul(sub(a0, a1), t), a1);
    out.m_pointOnA.m_quad = p;
    out.m_AminusB.m_quad = sub(p, b);
    return HK_GSK_CONVERT_POINT;
}

// 0x00E825A0 (file static, register arguments): a point of A against a triangle of B. The normal is the triangle's,
// towards A by the separating normal; no distance check.
static int hkGskManifold_convertPointFace(const hkpGskManifoldWork& work, const hkpGskManifold::ContactPoint& feature, hkGskManifoldConvertOut& out)
{
    const Q a = hkGskManifold_vertex(work, feature.m_vert[0]);
    const Q b0 = hkGskManifold_vertex(work, feature.m_vert[1]);
    const Q b1 = hkGskManifold_vertex(work, feature.m_vert[2]);
    const Q b2 = hkGskManifold_vertex(work, feature.m_vert[3]);
    out.m_pointOnA.m_quad = a;
    out.m_AminusB.m_quad = sub(a, b0);
    const Q e21 = sub(b2, b1);
    const Q e10 = sub(b1, b0);
    const Q e02 = sub(b0, b2);
    const Q n = cross(e21, e02);
    const Q invLen = invSqrt23SetZero(dot3(n, n));
    const Q c1 = cross(sub(a, b1), e21);
    const Q c2 = cross(sub(a, b2), e02);
    const Q c0 = cross(sub(a, b0), e10);
    const Q dots = hkGskManifold_dot3x4(mul(c1, n), mul(c2, n), mul(c0, n), mul(work.m_separatingNormal.m_quad, n));
    const Q flip = signBits(_mm_cmplt_ps(bcast<3>(dots), _mm_setzero_ps()));
    out.m_separatingNormal.m_quad = _mm_xor_ps(flip, mul(invLen, n));
    return ((~_mm_movemask_ps(_mm_cmplt_ps(dots, _mm_setzero_ps()))) & 7) ? HK_GSK_CONVERT_FACE_OUTSIDE : HK_GSK_CONVERT_FACE_POINT;
}

// 0x00E827A0 (file static, register arguments): a triangle of A against a point of B; the point on A is B's point
// projected onto the triangle's plane.
static int hkGskManifold_convertFacePoint(const hkpGskManifoldWork& work, const hkpGskManifold::ContactPoint& feature, hkGskManifoldConvertOut& out)
{
    const Q a0 = hkGskManifold_vertex(work, feature.m_vert[0]);
    const Q a1 = hkGskManifold_vertex(work, feature.m_vert[1]);
    const Q a2 = hkGskManifold_vertex(work, feature.m_vert[2]);
    const Q b = hkGskManifold_vertex(work, feature.m_vert[3]);
    out.m_pointOnA.m_quad = b;
    const Q aMinusB = sub(a0, b);
    out.m_AminusB.m_quad = aMinusB;
    const Q e02 = sub(a0, a2);
    const Q e21 = sub(a2, a1);
    const Q e10 = sub(a1, a0);
    const Q n = cross(e21, e02);
    const Q unitN = mul(invSqrt23SetZero(dot3(n, n)), n);
    const Q c1 = cross(sub(b, a1), e21);
    const Q c2 = cross(sub(b, a2), e02);
    const Q c0 = cross(sub(b, a0), e10);
    const Q dots = hkGskManifold_dot3x4(mul(c1, n), mul(c2, n), mul(c0, n), mul(work.m_separatingNormal.m_quad, n));
    out.m_pointOnA.m_quad = add(mul(dot3(unitN, aMinusB), unitN), b);
    const Q flip = signBits(_mm_cmplt_ps(bcast<3>(dots), _mm_setzero_ps()));
    out.m_separatingNormal.m_quad = _mm_xor_ps(flip, unitN);
    return ((~_mm_movemask_ps(_mm_cmplt_ps(dots, _mm_setzero_ps()))) & 7) ? HK_GSK_CONVERT_FACE_OUTSIDE : HK_GSK_CONVERT_FACE_POINT;
}

// 0x00E81A70 (file static, register arguments): two edges; a contact only while the closest points are inside both
// edges (the end points on opposite sides of the other edge's plane through the common normal).
static int hkGskManifold_convertEdgeEdge(const hkpGskManifoldWork& work, const hkpGskManifold::ContactPoint& feature, hkGskManifoldConvertOut& out)
{
    const Q a0 = hkGskManifold_vertex(work, feature.m_vert[0]);
    const Q a1 = hkGskManifold_vertex(work, feature.m_vert[1]);
    const Q b0 = hkGskManifold_vertex(work, feature.m_vert[2]);
    const Q b1 = hkGskManifold_vertex(work, feature.m_vert[3]);
    const Q dA = sub(a1, a0);
    const Q d11 = sub(a1, b1);
    const Q d00 = sub(a0, b0);
    const Q dB = sub(b1, b0);
    const Q n = cross(dA, dB);
    const Q cB = cross(dB, n);
    const Q cA = cross(dA, n);
    out.m_separatingNormal = work.m_separatingNormal;
    const Q dots = hkGskManifold_dot3x4(mul(cB, d00), mul(cA, d00), mul(cB, d11), mul(cA, d11));
    const int mask = _mm_movemask_ps(_mm_cmplt_ps(dots, _mm_setzero_ps()));
    if (mask != 3 && mask != 12)
    {
        return HK_GSK_CONVERT_FACE_OUTSIDE;
    }
    const Q s0 = bcast<0>(dots);
    const Q t = mul(recip23(sub(s0, bcast<2>(dots))), s0);
    const Q pA = add(mul(sub(a1, a0), t), a0);
    out.m_pointOnA.m_quad = pA;
    const Q s1 = bcast<1>(dots);
    const Q u = mul(recip23(sub(s1, bcast<3>(dots))), s1);
    out.m_AminusB.m_quad = sub(pA, add(mul(sub(b1, b0), u), b0));
    return HK_GSK_CONVERT_FACE_POINT;
}

// 0x00E829D0 (file static, register arguments)
static int hkGskManifold_convertFeature(const hkpGskManifoldWork& work, const hkpGskManifold::ContactPoint& feature, hkGskManifoldConvertOut& out)
{
    switch (feature.m_dimA * 4 + feature.m_dimB)
    {
    case 1 * 4 + 1:
        return hkGskManifold_convertPointPoint(work, feature, out);
    case 1 * 4 + 2:
        return hkGskManifold_convertPointEdge(work, feature, out);
    case 1 * 4 + 3:
        return hkGskManifold_convertPointFace(work, feature, out);
    case 2 * 4 + 1:
        return hkGskManifold_convertEdgePoint(work, feature, out);
    case 2 * 4 + 2:
        return hkGskManifold_convertEdgeEdge(work, feature, out);
    case 3 * 4 + 1:
        return hkGskManifold_convertFacePoint(work, feature, out);
    default:
        return HK_GSK_CONVERT_REJECT;
    }
}

// 0x00E81C00 (file static, register argument)
// Removes the vertex ids no contact point uses and renumbers the points' vertex offsets.
static void hkGskManifold_garbageCollect(hkpGskManifold& manifold)
{
    unsigned char used[16];
    memset(used, 0, sizeof(used));
    const int numPoints = manifold.m_numContactPoints;
    for (int i = 0; i < numPoints; i++)
    {
        const hkpGskManifold::ContactPoint& cp = manifold.m_contactPoints[i];
        const int dims = cp.m_dimA + cp.m_dimB;
        used[cp.m_vert[0] >> 4] = 1;
        used[cp.m_vert[1] >> 4] = 1;
        if (dims >= 3)
        {
            used[cp.m_vert[2] >> 4] = 1;
        }
        if (dims == 4)
        {
            used[cp.m_vert[3] >> 4] = 1;
        }
    }
    unsigned short* ids = manifold.getVertexIds();
    const int numVerts = manifold.m_numVertsA + manifold.m_numVertsB;
    // The original leaves newOffset uninitialized past numVerts; the remapping below reads such entries for the unused
    // vertex slots of the points (normally 0) and numVertsA reads newOffset[numVertsA] (numVertsA == numVerts only
    // without B vertices). Zero keeps it deterministic.
    int newOffset[16];
    memset(newOffset, 0, sizeof(newOffset));
    int offset = 0;
    int kept = 0;
    for (int i = 0; i < numVerts; i++)
    {
        newOffset[i] = offset;
        if (used[i])
        {
            ids[kept++] = ids[i];
            offset += 16;
        }
    }
    const unsigned char newNumA = (unsigned char)(newOffset[manifold.m_numVertsA] >> 4);
    manifold.m_numVertsA = newNumA;
    manifold.m_numVertsB = (unsigned char)(kept - newNumA);
    for (int i = 0; i < manifold.m_numContactPoints; i++)
    {
        hkpGskManifold::ContactPoint& cp = manifold.m_contactPoints[i];
        for (int k = 0; k < 4; k++)
        {
            cp.m_vert[k] = (unsigned char)newOffset[cp.m_vert[k] >> 4];
        }
    }
}

// Moves the vertex ids from after oldNumContactPoints to after newNumContactPoints (dwords, low to high).
static inline void hkGskManifold_moveVerticesDown(hkpGskManifold& manifold, int oldNumContactPoints, int newNumContactPoints)
{
    unsigned int* to = reinterpret_cast<unsigned int*>(&manifold.m_contactPoints[newNumContactPoints]);
    const unsigned int* from = reinterpret_cast<const unsigned int*>(&manifold.m_contactPoints[oldNumContactPoints]);
    for (int n = (manifold.m_numVertsA + manifold.m_numVertsB - 1) >> 1; n >= 0; n--)
    {
        *to++ = *from++;
    }
}

// 0x00E83D70
void hkGskManifold_downshiftVertices(hkpGskManifold& manifold, int oldNumContactPoints, int newNumContactPoints)
{
    hkGskManifold_moveVerticesDown(manifold, oldNumContactPoints, newNumContactPoints);
}

// 0x00E83DB0: as downshift, high to low (the arrays overlap)
void hkGskManifold_upShiftVertices(hkpGskManifold& manifold, int oldNumContactPoints, int newNumContactPoints)
{
    int n = (manifold.m_numVertsA + manifold.m_numVertsB - 1) >> 1;
    unsigned int* to = reinterpret_cast<unsigned int*>(&manifold.m_contactPoints[newNumContactPoints]) + n;
    const unsigned int* from = reinterpret_cast<const unsigned int*>(&manifold.m_contactPoints[oldNumContactPoints]) + n;
    for (; n >= 0; n--)
    {
        *to-- = *from--;
    }
}

// 0x00E81D20: the last point takes the index's place
void hkGskManifold_removePoint(hkpGskManifold& manifold, int index)
{
    const int numPoints = manifold.m_numContactPoints;
    manifold.m_numContactPoints = (unsigned char)(numPoints - 1);
    manifold.m_contactPoints[index] = manifold.m_contactPoints[numPoints - 1];
    hkGskManifold_moveVerticesDown(manifold, numPoints, numPoints - 1);
    hkGskManifold_garbageCollect(manifold);
}

// 0x00E83240
// Converts the points from firstPointIndex on into the output; a point that separated beyond the keep distance or
// whose feature no longer gives a contact is removed (the last point moves into its place and is checked next).
// Returns 1 when a removed point asks for a new GSK point (its feature turned outside an edge or face).
int hkGskManifold_verifyAndGetPoints(hkpGskManifold& manifold, const hkpGskManifoldWork& work, int firstPointIndex, hkpProcessCollisionOutput& processOut, hkpContactMgr* contactMgr)
{
    int needNewPoint = 0;
    const int oldNumPoints = manifold.m_numContactPoints;
    int numPoints = oldNumPoints;
    if (firstPointIndex >= numPoints)
    {
        return 0;
    }
    hkpGskManifold::ContactPoint* end = &manifold.m_contactPoints[oldNumPoints];
    hkpGskManifold::ContactPoint* cp = &manifold.m_contactPoints[firstPointIndex];
    for (int i = firstPointIndex; i < numPoints; i++, cp++)
    {
        hkpProcessCdPoint* point = processOut.m_firstFreeContactPoint;
        // Zeroed per point as in the original (its inlined conversion starts from zero registers): an edge-edge
        // feature outside its edges leaves A minus B at zero, so its distance is -(radiusA + radiusB) and it asks for
        // a new point.
        hkGskManifoldConvertOut out;
        out.m_pointOnA.m_quad = _mm_setzero_ps();
        out.m_AminusB.m_quad = _mm_setzero_ps();
        out.m_separatingNormal.m_quad = _mm_setzero_ps();
        const int code = hkGskManifold_convertFeature(work, *cp, out);
        if (code & 1)
        {
            const Q d = dot3(out.m_separatingNormal.m_quad, out.m_AminusB.m_quad);
            const Q radiusB = splat(work.m_radiusB);
            const Q toSurfaceB = sub(radiusB, d);
            const Q dist = sub(d, add(splat(work.m_radiusA), radiusB));
            if (lessOrNaN(dist, splat(work.m_keepContact)))
            {
                // the distance is along the feature's normal, the point moves along the manifold's separating normal
                point->m_contact.m_position.m_quad = add(mul(work.m_separatingNormal.m_quad, toSurfaceB), out.m_pointOnA.m_quad);
                point->m_contact.m_separatingNormal.m_quad = setXYZ_W(work.m_weldedNormal.m_quad, dist);
                point->m_contactPointId = cp->m_id;
                point->m_isShortestPoint = 1;
                processOut.m_firstFreeContactPoint = point + 1;
                continue;
            }
        }
        else if (code & 0x10)
        {
            const Q d = dot3(out.m_separatingNormal.m_quad, out.m_AminusB.m_quad);
            const Q dist = sub(d, add(splat(work.m_radiusB), splat(work.m_radiusA)));
            if (lessOrNaN(dist, splat(work.m_keepContact)))
            {
                needNewPoint = 1;
            }
        }
        else if (code & 4)
        {
            needNewPoint = 1;
        }

        // remove the point
        if (cp->m_id != 0xFFFF)
        {
            contactMgr->removeContactPointImpl(cp->m_id, *processOut.m_constraintOwner);
        }
        end--;
        *cp = *end;
        numPoints--;
        i--;
        cp--;
    }
    if (oldNumPoints > numPoints)
    {
        manifold.m_numContactPoints = (unsigned char)numPoints;
        hkGskManifold_moveVerticesDown(manifold, oldNumPoints, numPoints);
        hkGskManifold_garbageCollect(manifold);
    }
    return needNewPoint;
}

// 0x00E815D0
// IN_MANIFOLD (moved to index 0) when a point has the same feature. Otherwise a point sharing the new point's single
// vertex of A or of B has its feature cleared (dims 0: dropped by the next verification) and the result is
// FEATURE_WITHIN_KEEP_DISTANCE_REMOVED.
hkpGskManifoldPointExistsFlags hkGskManifold_doesPointExistAndResort(hkpGskManifold& manifold, const hkpGskCache& newPoint)
{
    for (int i = 0; i < manifold.m_numContactPoints; i++)
    {
        const hkpGskManifold::ContactPoint& cp = manifold.m_contactPoints[i];
        if (newPoint.m_dimA != cp.m_dimA || newPoint.m_dimB != cp.m_dimB)
        {
            continue;
        }
        const int dims = cp.m_dimB + cp.m_dimA;
        if (newPoint.m_vertices[0] != manifold.getVertexId(cp, 0) || newPoint.m_vertices[1] != manifold.getVertexId(cp, 1))
        {
            continue;
        }
        if (dims > 2 && newPoint.m_vertices[2] != manifold.getVertexId(cp, 2))
        {
            continue;
        }
        if (dims > 3 && newPoint.m_vertices[3] != manifold.getVertexId(cp, 3))
        {
            continue;
        }
        if (i != 0)
        {
            const hkpGskManifold::ContactPoint t = manifold.m_contactPoints[i];
            manifold.m_contactPoints[i] = manifold.m_contactPoints[0];
            manifold.m_contactPoints[0] = t;
        }
        return HK_GSK_MANIFOLD_POINT_IN_MANIFOLD;
    }
    if (newPoint.m_dimA == 1 || newPoint.m_dimB == 1)
    {
        for (int i = 0; i < manifold.m_numContactPoints; i++)
        {
            hkpGskManifold::ContactPoint& cp = manifold.m_contactPoints[i];
            if (newPoint.m_dimA == 1 && cp.m_dimA == 1 && manifold.getVertexId(cp, 0) == newPoint.m_vertices[0])
            {
                cp.m_dimA = 0;
                cp.m_dimB = 0;
                return HK_GSK_MANIFOLD_FEATURE_WITHIN_KEEP_DISTANCE_REMOVED;
            }
            if (newPoint.m_dimB == 1 && cp.m_dimB == 1 && manifold.getVertexId(cp, cp.m_dimA) == newPoint.m_vertices[newPoint.m_dimA])
            {
                cp.m_dimA = 0;
                cp.m_dimB = 0;
                return HK_GSK_MANIFOLD_FEATURE_WITHIN_KEEP_DISTANCE_REMOVED;
            }
        }
    }
    return HK_GSK_MANIFOLD_POINT_NOT_IN_MANIFOLD;
}

// 0x00E81D80 (file static, register argument)
// Which of five points (the four of the manifold, then the new one) to drop: the four kept span the largest area
// (the point farthest from the new one, the farthest from that, the farthest from their line, the farthest from that
// point). When the new point is not among them and is not deeper than the closest one by enough, the first point not
// kept goes; otherwise the new point replaces the old point whose (depth difference)^2 / distance^2 is the largest
// above (10 degrees)^2, or nothing (4).
static int hkGskManifold_findRedundant5thPoint(hkpProcessCdPoint* resultPointArray)
{
    const Q intOne = _mm_castsi128_ps(_mm_set1_epi32(1)); // g_intVectorConstants[HK_QUADINT_1]
    int used[5] = { 0, 0, 0, 0, 0 };
    alignas(16) hkVector4f dist2[4];

    // the distances of the old points from the new one
    const Q p4 = resultPointArray[4].m_contact.m_position.m_quad;
    Q minD = g_vectorfConstants[HK_QUADREAL_MAX];
    Q maxD = _mm_setzero_ps();
    Q counter = _mm_setzero_ps();
    Q maxIndex = _mm_setzero_ps();
    Q minIndex = _mm_setzero_ps();
    for (int i = 0; i < 4; i++)
    {
        const Q d = sub(p4, resultPointArray[i].m_contact.m_position.m_quad);
        const Q d2 = dot3(d, d);
        const Q isMax = _mm_cmplt_ps(maxD, d2);
        maxIndex = _mm_or_ps(_mm_and_ps(isMax, counter), _mm_andnot_ps(isMax, maxIndex));
        const Q isMin = _mm_cmplt_ps(d2, minD);
        minIndex = _mm_or_ps(_mm_and_ps(isMin, counter), _mm_andnot_ps(isMin, minIndex));
        dist2[i].m_quad = d2;
        minD = _mm_min_ps(minD, d2);
        counter = _mm_castsi128_ps(_mm_add_epi32(_mm_castps_si128(counter), _mm_castps_si128(intOne)));
        maxD = _mm_max_ps(maxD, d2);
    }
    const Q depth4 = bcast<3>(resultPointArray[4].m_contact.m_separatingNormal.m_quad);
    const int closest = _mm_cvtsi128_si32(_mm_castps_si128(minIndex));
    const int first = _mm_cvtsi128_si32(_mm_castps_si128(maxIndex));
    used[first] = 1;
    const Q depthDiff = sub(depth4, bcast<3>(resultPointArray[closest].m_contact.m_separatingNormal.m_quad));

    // Original bug kept: the candidate index counts only the points not used yet, so a pick after the first used
    // index names the point before it (the indices are rank among the unused points, used as array indices).

    // the second: the farthest from the first, more than 5% beyond the first's distance (else index 0)
    const Q pFirst = resultPointArray[first].m_contact.m_position.m_quad;
    {
        Q best = mul(splat(1.05f), maxD); // 0x3F866666
        Q index = _mm_setzero_ps();
        counter = _mm_setzero_ps();
        for (int j = 0; j < 5; j++)
        {
            if (used[j])
            {
                continue;
            }
            const Q d = sub(pFirst, resultPointArray[j].m_contact.m_position.m_quad);
            const Q d2 = dot3(d, d);
            const Q isMax = _mm_cmplt_ps(best, d2);
            index = _mm_or_ps(_mm_and_ps(isMax, counter), _mm_andnot_ps(isMax, index));
            counter = _mm_castsi128_ps(_mm_add_epi32(_mm_castps_si128(counter), _mm_castps_si128(intOne)));
            best = _mm_max_ps(best, d2);
        }
        const int second = _mm_cvtsi128_si32(_mm_castps_si128(index));
        const Q edge = sub(pFirst, resultPointArray[second].m_contact.m_position.m_quad);
        used[second] = 1;

        // the third: the farthest from the line (the largest |(p - first) x edge|^2)
        best = _mm_setzero_ps();
        index = _mm_setzero_ps();
        counter = _mm_setzero_ps();
        for (int j = 0; j < 5; j++)
        {
            if (used[j])
            {
                continue;
            }
            const Q c = cross(sub(resultPointArray[j].m_contact.m_position.m_quad, pFirst), edge);
            const Q area = dot3(c, c);
            const Q isMax = _mm_cmplt_ps(best, area);
            index = _mm_or_ps(_mm_and_ps(isMax, counter), _mm_andnot_ps(isMax, index));
            counter = _mm_castsi128_ps(_mm_add_epi32(_mm_castps_si128(counter), _mm_castps_si128(intOne)));
            best = _mm_max_ps(best, area);
        }
        const int third = _mm_cvtsi128_si32(_mm_castps_si128(index));
        used[third] = 1;

        // the fourth: the farthest from the third
        const Q pThird = resultPointArray[third].m_contact.m_position.m_quad;
        best = _mm_setzero_ps();
        index = _mm_setzero_ps();
        counter = _mm_setzero_ps();
        for (int j = 0; j < 5; j++)
        {
            if (used[j])
            {
                continue;
            }
            const Q d = sub(pThird, resultPointArray[j].m_contact.m_position.m_quad);
            const Q d2 = dot3(d, d);
            const Q isMax = _mm_cmplt_ps(best, d2);
            index = _mm_or_ps(_mm_and_ps(isMax, counter), _mm_andnot_ps(isMax, index));
            counter = _mm_castsi128_ps(_mm_add_epi32(_mm_castps_si128(counter), _mm_castps_si128(intOne)));
            best = _mm_max_ps(best, d2);
        }
        used[_mm_cvtsi128_si32(_mm_castps_si128(index))] = 1;
    }

    // The new point not kept, deeper than the closest old point by more than sqrt(minDist^2 / 526): replace the old
    // point with the steepest depth change (or none).
    if (!used[4] && lessOrNaN(depthDiff, _mm_setzero_ps()) && lessOrNaN(minD, mul(mul(splat(526.0f), depthDiff), depthDiff))) // 0x44038000
    {
        Q result = _mm_castsi128_ps(_mm_set1_epi32(4)); // 0x01365410: (4, 4, 4, 4)
        const Q eps = splat(1.0e-9f); // 0x3089705F
        Q best = splat(0.030461743f); // 0x3CF98AE8: (10 degrees)^2
        Q index = _mm_setzero_ps();
        for (int k = 0; k < 4; k++)
        {
            const Q d2 = dist2[k].m_quad;
            const Q diff = sub(bcast<3>(resultPointArray[k].m_contact.m_separatingNormal.m_quad), depth4);
            const Q num = add(mul(diff, diff), eps);
            const Q isZero = _mm_cmpeq_ps(d2, _mm_setzero_ps());
            const Q steeper = _mm_cmplt_ps(mul(d2, best), num);
            const Q ratio = mul(recip23(d2), num);
            best = _mm_or_ps(_mm_and_ps(_mm_andnot_ps(isZero, ratio), steeper), _mm_andnot_ps(steeper, best));
            result = _mm_or_ps(_mm_andnot_ps(steeper, result), _mm_and_ps(steeper, index));
            index = _mm_castsi128_ps(_mm_add_epi32(_mm_castps_si128(index), _mm_castps_si128(intOne)));
        }
        return _mm_cvtsi128_si32(_mm_castps_si128(result));
    }
    for (int j = 0; j < 5; j++)
    {
        if (!used[j])
        {
            return j;
        }
    }
    return 0;
}

// 0x00E821D0
// Adds the new point (its contact point already in newCdPointInResultArray) as contact point 0. A full manifold drops
// its redundant point first (or rejects the new one).
hkpGskManifoldAddStatus hkGskManifold_addPoint(hkpGskManifold& manifold, const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output, const hkpGskCache& newPoint, hkpProcessCdPoint* newCdPointInResultArray, hkpProcessCdPoint* resultPointArray, hkpContactMgr* mgr, hkpGskManifoldUtilMgrHandling mgrHandling)
{
    hkpGskManifoldAddStatus status = HK_GSK_MANIFOLD_POINT_ADDED;
    unsigned short id;
    if (manifold.m_numContactPoints == hkpGskManifold::MAX_POINTS)
    {
        const int redundant = hkGskManifold_findRedundant5thPoint(resultPointArray);
        if (redundant == 4)
        {
            return HK_GSK_MANIFOLD_POINT_REJECTED;
        }
        mgr->removeContactPointImpl(manifold.m_contactPoints[redundant].m_id, *output.m_constraintOwner);
        if (redundant != 0)
        {
            manifold.m_contactPoints[redundant] = manifold.m_contactPoints[0];
        }
        else
        {
            manifold.m_contactPoints[0] = manifold.m_contactPoints[1];
        }
        hkGskManifold_garbageCollect(manifold);
        id = mgr->addContactPointImpl(bodyA, bodyB, input, output, &newPoint, newCdPointInResultArray->m_contact);
        if (id == 0xFFFF)
        {
            // Original behaviour kept: point 3 moves to index 0 and result point 3 into the dropped point's slot.
            manifold.m_contactPoints[0] = manifold.m_contactPoints[3];
            const int numPoints = manifold.m_numContactPoints;
            manifold.m_numContactPoints = (unsigned char)(numPoints - 1);
            hkGskManifold_moveVerticesDown(manifold, numPoints, numPoints - 1);
            resultPointArray[redundant] = resultPointArray[3];
            return HK_GSK_MANIFOLD_TWO_POINT2_REJECTED;
        }
        newCdPointInResultArray->m_contactPointId = id;
        resultPointArray[redundant] = *newCdPointInResultArray;
        status = (hkpGskManifoldAddStatus)redundant;
    }
    else
    {
        if (mgrHandling != HK_GSK_MANIFOLD_CREATE_ID_ALWAYS && newPoint.m_dimB != 3)
        {
            id = 0xFFFF; // a potential contact: the caller asks for its id later
        }
        else
        {
            id = mgr->addContactPointImpl(bodyA, bodyB, input, output, &newPoint, newCdPointInResultArray->m_contact);
            if (id == 0xFFFF)
            {
                return HK_GSK_MANIFOLD_POINT_REJECTED;
            }
        }
        newCdPointInResultArray->m_contactPointId = id;
        const int numPoints = manifold.m_numContactPoints;
        hkGskManifold_upShiftVertices(manifold, numPoints, numPoints + 1);
        manifold.m_contactPoints[numPoints] = manifold.m_contactPoints[0];
        manifold.m_numContactPoints = (unsigned char)(numPoints + 1);
    }

    hkpGskManifold::ContactPoint& cp = manifold.m_contactPoints[0];
    cp.m_dimA = newPoint.m_dimA;
    cp.m_id = id;
    cp.m_dimB = newPoint.m_dimB;
    cp.m_allVerts = 0;
    unsigned short* ids = manifold.getVertexIds();

    // A's vertices: found or inserted at the end of A's ids (B's ids move up)
    int addedA = 0;
    for (int i = 0; i < newPoint.m_dimA; i++)
    {
        const unsigned short vid = newPoint.m_vertices[i];
        int j = 0;
        for (; j < manifold.m_numVertsA; j++)
        {
            if (ids[j] == vid)
            {
                break;
            }
        }
        if (j == manifold.m_numVertsA)
        {
            for (int k = manifold.m_numVertsB + manifold.m_numVertsA; k > j; k--)
            {
                ids[k] = ids[k - 1];
            }
            ids[j] = vid;
            manifold.m_numVertsA++;
            addedA += 16;
        }
        cp.m_vert[i] = (unsigned char)(j << 4);
    }
    if (addedA)
    {
        // the other points' B vertices moved up
        for (int c = 1; c < manifold.m_numContactPoints; c++)
        {
            hkpGskManifold::ContactPoint& other = manifold.m_contactPoints[c];
            for (int k = other.m_dimA; k < other.m_dimA + other.m_dimB; k++)
            {
                other.m_vert[k] = (unsigned char)(other.m_vert[k] + addedA);
            }
        }
    }

    // B's vertices: found or appended
    for (int i = 0; i < newPoint.m_dimB; i++)
    {
        const unsigned short vid = newPoint.m_vertices[newPoint.m_dimA + i];
        const int end = manifold.m_numVertsA + manifold.m_numVertsB;
        int j = manifold.m_numVertsA;
        for (; j < end; j++)
        {
            if (ids[j] == vid)
            {
                break;
            }
        }
        if (j == end)
        {
            ids[j] = vid;
            manifold.m_numVertsB++;
        }
        cp.m_vert[newPoint.m_dimA + i] = (unsigned char)(j << 4);
    }
    return status;
}

// 0x00E81740
void hkGskManifold_cleanup(hkpGskManifold& manifold, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner)
{
    for (int i = 0; i < manifold.m_numContactPoints; i++)
    {
        const unsigned short id = manifold.m_contactPoints[i].m_id;
        if (id != 0xFFFF)
        {
            mgr->removeContactPointImpl(id, constraintOwner);
        }
    }
    manifold.m_numContactPoints = 0;
    manifold.m_numVertsA = 0;
    manifold.m_numVertsB = 0;
}

// 0x00DC4DD0 (hkpgskmanifoldutil.h:112)
// The work data of a manifold: the radii, the squared radius sum with the keep distance, and the manifold's vertices
// in world space.
void hkGskManifold_init(const hkpGskManifold& manifold, const hkVector4f& separatingNormal, const hkVector4f& weldedNormal, const hkpCdBody& cA, const hkpCdBody& cB, float keepContactMaxDist, hkpGskManifoldWork& work)
{
    work.m_keepContact = keepContactMaxDist;
    const hkpConvexShape* shapeA = static_cast<const hkpConvexShape*>(cA.getShape());
    const hkpConvexShape* shapeB = static_cast<const hkpConvexShape*>(cB.getShape());
    work.m_radiusA = shapeA->m_radius;
    const float radiusB = shapeB->m_radius;
    const float sum = work.m_radiusA.val() + keepContactMaxDist + radiusB;
    work.m_radiusB = radiusB;
    work.m_radiusSumSqrd = sum * sum;
    work.m_separatingNormal = separatingNormal;
    work.m_weldedNormal = weldedNormal;
    if (manifold.m_numContactPoints == 0)
    {
        return;
    }
    const unsigned short* ids = manifold.getVertexIds();
    shapeA->convertVertexIdsToVertices(ids, manifold.m_numVertsA, work.m_vertices);
    {
        const hkTransformf& t = cA.getTransform();
        for (int i = manifold.m_numVertsA - 1; i >= 0; i--)
        {
            work.m_vertices[i].m_quad = transformPos(t, work.m_vertices[i].m_quad);
        }
    }
    hkcdVertex* verticesB = &work.m_vertices[manifold.m_numVertsA];
    shapeB->convertVertexIdsToVertices(ids + manifold.m_numVertsA, manifold.m_numVertsB, verticesB);
    {
        const hkTransformf& t = cB.getTransform();
        for (int i = manifold.m_numVertsB - 1; i >= 0; i--)
        {
            verticesB[i].m_quad = transformPos(t, verticesB[i].m_quad);
        }
    }
    work.m_debugTriangleShape = nullptr;
    work.m_debugBodyB = nullptr;
}
