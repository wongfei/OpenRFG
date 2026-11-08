// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgsk.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0); hkpGskStatus, hkpGskOut and the inline members
// added by hand (HK6). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/collide/linesegment/hklinesegmentutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdvertex.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskcache.h"

class hkTransformf;
class hkpConvexShape;

enum hkpGskStatus
{
    HK_GSK_OK = 0,
    HK_GSK_PD_HEURISTIC_SAMPLING = 1,
    HK_GSK_PD_OUT_OF_MEMORY = 2,
    HK_GSK_PD_UNSOLVABLE = 3,
    HK_GSK_PENETRATING = 4,
};

// size 0x30
struct alignas(16) hkpGskOut
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpGskOut);
    // hkpGskOut 0x00DD0870: implicit

    hkVector4f m_normalInA; // 0x00
    hkVector4f m_pointAinA; // 0x10
    hkPadSpu<float> m_distance; // 0x20
};

// size 0x1A0
class alignas(16) hkpGsk
{
public:
    enum
    {
        GSK_NONE_NONE = 0,
        GSK_POINT_POINT = 9,
        GSK_POINT_EDGE = 10,
        GSK_EDGE_POINT = 17,
        GSK_POINT_TRIANGLE = 11,
        GSK_TRIANGLE_POINT = 25,
        GSK_EDGE_TRIANGLE = 19,
        GSK_TRIANGLE_EDGE = 26,
        GSK_POINT_TETRA = 12,
        GSK_TETRA_POINT = 33,
        GSK_EDGE_EDGE = 18,
    };

    enum NextCase
    {
        NEXT_IS_BREAK = 0,
        NEXT_IS_PENETRATION = 1,
        NEXT_IS_EDGE_EDGE = 2,
    };

    enum SupportTypes
    {
        BUILD_NEG_SUPPORT = -1,
        BUILD_NO_SUPPORT = 0,
        BUILD_POS_SUPPORT = 1,
    };

    enum SupportState
    {
        SUPPORT_CHANGED = 0,
        DONT_CALL_GET_SUPPORTING_VERTEX_A = 1,
        DONT_CALL_GET_SUPPORTING_VERTEX_B = -1,
    };

    enum ReduceDimensionResult
    {
        REDUCE_DIMENSION_OK = 0,
        REDUCE_DIMENSION_PENETRATION = 1,
    };

    // size 0x14
    struct GetClosesetPointInput
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(GetClosesetPointInput);
        GetClosesetPointInput() {} // 0x00DB59A0

        hkPadSpu<hkTransformf const*> m_aTb; // 0x00
        hkPadSpu<hkTransformf const*> m_transformA; // 0x04
        hkPadSpu<hkpConvexShape const*> m_shapeA; // 0x08
        hkPadSpu<hkpConvexShape const*> m_shapeB; // 0x0C
        hkPadSpu<float> m_collisionTolerance; // 0x10
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpGsk);
    // 0x00DC5770
    hkpGsk()
    {
        m_doNotHandlePenetration = 0;
        // Not in the original, which leaves m_lastDimB uninitialized: reduceDimension reads it (lastDimB == 2) for
        // an edge-triangle feature from a cache before any reduction wrote it. Zero keeps the port deterministic.
        m_lastDimB = 0;
    }

    void init(const hkpConvexShape* shapeA, const hkpConvexShape* shapeB, const hkpGskCache& cache); // 0x00DC56F0 (hkpgsk.cpp)
    hkpGskStatus getClosestFeature(const hkpConvexShape* shapeA, const hkpConvexShape* shapeB, const hkTransformf& aTb, hkVector4f& separatingNormalOut); // 0x00E7FDB0
    void convertFeatureToClosestDistance(const hkVector4f& separatingNormal, hkpGskOut& out); // 0x00E7ECB0
    static hkResult getClosestPoint(const GetClosesetPointInput& input, hkpGskCache& gskCache, hkVector4f& separatingNormalOut, hkVector4f& pointOnBOut); // 0x00E80B50
    // 0x00E87E50: the closest point on A (in A's space) of the current features
    void getClosestPointAinA(const hkVector4f& separatingNormal, hkVector4f& pointInA)
    {
        if (m_dimA == 1)
        {
            pointInA = m_verticesA[0];
            return;
        }
        if (m_dimB == 1)
        {
            pointInA.setAddMul(m_verticesBinA[0], separatingNormal, separatingNormal.getComponent<3>());
            return;
        }
        pointInA = m_closestLineLineResult.m_closestPointA;
    }

    // 0x00DC5760
    void checkForChangesAndUpdateCache(hkpGskCache& cache) const
    {
        if (m_featureChange)
        {
            exitAndExportCacheImpl(cache);
        }
    }
    static void codeBegin(); // 0x00E7E3A0
    static void codeEnd(); // 0x00E7E3B0
    void print(); // 0x00E7E380
public:
    ReduceDimensionResult reduceDimension(); // 0x00E80E70
    void reduceDimensionExtended(); // 0x00E7ED00
    void handlePenetration(const hkpConvexShape* shapeA, const hkpConvexShape* shapeB, const hkTransformf& aTb); // 0x00E7F3A0
    void setFeatureChange(int f) { m_featureChange = f; } // 0x00DC56E0
public:
    void exitAndExportCacheImpl(hkpGskCache& cache) const; // 0x00E7E3C0
public:
    int checkTriangleBoundaries(const hkVector4f& a, hkVector4f* b, SupportTypes checkSupport); // 0x00E7E660
    NextCase processEdgeTriangle(hkVector4f* vertexA, hkVector4f* vertexB, hkPadSpu<int>& dimA, hkPadSpu<int>& dimB, hkBool comingFromEdgeEdge, SupportTypes supportType); // 0x00E7E850

public:
    hkPadSpu<int> m_dimA; // 0x00
    hkPadSpu<int> m_dimB; // 0x04
    hkPadSpu<int> m_maxDimA; // 0x08
    hkPadSpu<int> m_maxDimB; // 0x0C
    hkPadSpu<unsigned int> m_doNotHandlePenetration; // 0x10
    hkPadSpu<int> m_featureChange; // 0x14
    hkcdVertex m_verticesA[4]; // 0x20
    hkVector4f m_dummy[4]; // 0x60
    hkVector4f m_verticesBinA[4]; // 0xA0
    hkcdVertex m_verticesBinB[4]; // 0xE0
    hkVector4f m_support; // 0x120
    hkLineSegmentUtil::ClosestLineSegLineSegResult m_closestLineLineResult; // 0x130
public:
    hkVector4f m_checkTriangleDots; // 0x180
    hkPadSpu<int> m_lastDimB; // 0x190
    SupportState m_supportState; // 0x194
};

void hkGskRecalcContactInternal(hkVector4f* verticesA, hkVector4f* verticesB, int dimA, int dimB, const hkVector4f& masterNormal, hkVector4f& pointA, hkVector4f& support); // 0x00E7F8E0
// 0x00E87EB0 (.h): the contact point and normal of the current features along masterNormalA
inline void hkGskRecalcContact(hkpGsk& gsk, const hkVector4f& masterNormalA, hkVector4f& pointBinAOut, hkVector4f& supportOut)
{
    hkGskRecalcContactInternal(gsk.m_verticesA, gsk.m_verticesBinA, gsk.m_dimA, gsk.m_dimB, masterNormalA, pointBinAOut, supportOut);
}
