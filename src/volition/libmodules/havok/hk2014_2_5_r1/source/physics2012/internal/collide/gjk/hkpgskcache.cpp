// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskcache.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the initial GSK caches, from rfg.exe. A cache holds the vertex ids of the
// current closest features (m_dimA vertices of A, then m_dimB of B); a new cache starts with one supporting vertex of
// each shape.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskcache.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdvertex.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/hkpconvexshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/triangle/hkptriangleshape.h"

// The maximum dimension of a shape's feature: its collision sphere count, 15 above 3 (MAX_DIM_MAX).
static inline int hkGskCache_maxDim(const hkpConvexShape* shape)
{
    int n = shape->getNumCollisionSpheres();
    if (n > 3)
    {
        n = hkpGskCache::MAX_DIM_MAX;
    }
    return n;
}

// 0x00E841A0
// A triangle starts with all three of its vertices (0, 1, 2) and the vertex of A furthest along the triangle normal on
// the side where A's vertex 0 lies. An extruded triangle is a general convex shape.
void hkpGskCache::initTriangle(const hkpConvexShape* shapeA, const hkpTriangleShape* shapeB, const hkTransformf& btoa)
{
    if (shapeB->m_isExtruded)
    {
        init(shapeA, shapeB, btoa);
        return;
    }
    m_vertices[1] = 0;
    m_vertices[2] = 1;
    m_vertices[3] = 2;

    hkVector4f triangle[3];
    const hkVector4f* vertices = &shapeB->m_vertexA;
    for (int i = 2; i >= 0; i--)
    {
        triangle[i]._setTransformedPos(btoa, vertices[i]);
    }
    hkVector4f e0;
    e0.setSub(triangle[1], triangle[0]);
    hkVector4f e1;
    e1.setSub(triangle[2], triangle[1]);
    hkVector4f normal;
    normal.setCross(e0, e1);

    // A's vertex 0 (the id written above as m_vertices[1])
    hkcdVertex vertexA;
    shapeA->convertVertexIdsToVertices(&m_vertices[1], 1, &vertexA);
    hkVector4f offset;
    offset.setSub(vertexA, triangle[0]);
    hkVector4f direction;
    direction.setFlipSign(normal, offset.dot<3>(normal));
    hkcdVertex supportA;
    shapeA->getSupportingVertex(direction, supportA);

    m_vertices[0] = (unsigned short)supportA.getId();
    m_dimA = 1;
    m_dimB = 3;
    m_maxDimA = (unsigned char)(hkGskCache_maxDim(shapeA) & 0xF);
    m_maxDimB = 3;
    m_gskFlags = 0;
}

// 0x00E83FD0
// The supporting vertex of A towards B's vertex 0 and the supporting vertex of B back towards it (aTb is B in A's
// space).
void hkpGskCache::init(const hkpConvexShape* shapeA, const hkpConvexShape* shapeB, const hkTransformf& aTb)
{
    unsigned short ids[2] = {0, 0};
    hkcdVertex vertexA;
    shapeA->convertVertexIdsToVertices(ids, 1, &vertexA);
    hkcdVertex vertexB;
    shapeB->convertVertexIdsToVertices(ids, 1, &vertexB);

    hkVector4f vertexBinA;
    vertexBinA._setTransformedPos(aTb, vertexB);
    hkVector4f directionA;
    directionA.setSub(vertexBinA, vertexA);
    hkcdVertex supportA;
    shapeA->getSupportingVertex(directionA, supportA);

    hkVector4f d;
    d.setSub(vertexBinA, supportA);
    hkVector4f directionB;
    directionB._setRotatedInverseDir(aTb.getRotation(), d);
    directionB.setNeg<4>(directionB);
    hkcdVertex supportB;
    shapeB->getSupportingVertex(directionB, supportB);

    m_vertices[1] = (unsigned short)supportB.getId();
    m_vertices[0] = (unsigned short)supportA.getId();
    m_vertices[2] = 0;
    m_vertices[3] = 0;
    m_dimA = 1;
    m_dimB = 1;
    int maxDimA = hkGskCache_maxDim(shapeA);
    int maxDimB = hkGskCache_maxDim(shapeB);
    m_maxDimA = (unsigned char)(maxDimA & 0xF);
    m_maxDimB = (unsigned char)(maxDimB & 0xF);
    m_gskFlags = 0;
}
