// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/hkpcollidetriangleutil.h
//
// Havok 2014.2.5: layouts from rfg.pdb, ported from the 2014.1 SDK header (havok_impl.md HK6). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/algorithms/triangle/hkcdtriangleutil.h"

// size 0x8
struct hkpFeatureOutput
{
    typedef unsigned short VertexId;

    unsigned char numFeatures; // 0x00
    VertexId featureIds[3]; // 0x02
};

// size 0x1
class hkpCollideTriangleUtil
{
public:
    enum ClosestPointTriangleStatus
    {
        HIT_TRIANGLE_FACE = 0,
        HIT_TRIANGLE_EDGE = 1,
    };

    // size 0x10
    struct ClosestPointTriangleCache
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ClosestPointTriangleCache);

        float m_QQ; // 0x00
        float m_RR; // 0x04
        float m_QR; // 0x08
        float m_invTriNormal; // 0x0C
    };

    // size 0x20
    struct alignas(16) ClosestPointTriangleResult
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ClosestPointTriangleResult);

        void calcClosestPoint(const hkVector4f& point, hkVector4f& closestPoint)
        {
            hkSimdFloat32 distanceSr;
            distanceSr.setFromFloat(distance);
            closestPoint.setAddMul(point, hitDirection, distanceSr);
        }

        hkVector4f hitDirection; // 0x00
        float distance; // 0x10
    };

    // size 0x14
    struct PointTriangleDistanceCache
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(PointTriangleDistanceCache);

        float m_invEdgeLen[3]; // 0x00
        float m_invNormalLen; // 0x0C
        float m_normalLen; // 0x10
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpCollideTriangleUtil);

    // 0x00E87F80
    static void calcBarycentricCoordinates(const hkVector4f& pos, const hkVector4f& t0, const hkVector4f& t1, const hkVector4f& t2, hkVector4f& result)
    {
        hkcdTriangleUtil::calcBarycentricCoordinates(pos, t0, t1, t2, result);
    }

    static void setupClosestPointTriangleCache(const hkVector4f* tri, ClosestPointTriangleCache& cache); // 0x00DD1C30
    static ClosestPointTriangleStatus closestPointTriangle(const hkVector4f& position, const hkVector4f* tri, const ClosestPointTriangleCache& cache, ClosestPointTriangleResult& result, hkpFeatureOutput* featureOutput = nullptr); // 0x00DD1F90
    static void setupPointTriangleDistanceCache(const hkVector4f* tri, PointTriangleDistanceCache& cache); // 0x00DD1D90
    static void calcTrianglePlaneDirections(const hkVector4f* tri, const PointTriangleDistanceCache& cache, hkTransformf& planeEquationsOut, hkVector4f& normalOut); // 0x00DD1E40

    static int getNextModulo3(int i) { return vertexToEdgeLut[i + 2]; } // 0x00DD2510
    static int getPrevModulo3(int i) { return vertexToEdgeLut[i]; } // 0x00DD2520

protected:
    hkpCollideTriangleUtil();

public:
    static const signed char maskToIndex[]; // 0x0135B414
    static const signed char vertexToEdgeLut[]; // 0x0135B41C
};
