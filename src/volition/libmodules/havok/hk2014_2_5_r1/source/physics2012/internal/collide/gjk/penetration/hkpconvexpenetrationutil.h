// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/penetration/hkpconvexpenetrationutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0). Hand-maintained since HK6.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdwingededge.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgsk.h"

class hkContactPoint;
class hkSimdFloat32;
class hkcdVertex;
class hkpConvexShape;

// size 0x70
class hkpConvexPenetrationUtil
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpConvexPenetrationUtil);
    void init(const hkpConvexShape* shapeA, const hkpConvexShape* shapeB, const hkTransformf& aTb, float epsTolerance, hkcdVertex* pointsAinA, hkcdVertex* pointsBinB, hkVector4f* simplex, int simplexSize); // 0x00E8A190
    static void codeBegin(); // 0x00E87FF0
    static void codeEnd(); // 0x00E88040
    hkpGskStatus calculatePenetrationDepth(hkpGskOut& output); // 0x00E890D0
public:
    hkBool case0To1(hkpGskOut& output); // 0x00E8A210
    void case1To2(); // 0x00E8A350
    void case2To3(); // 0x00E8AA40
    hkResult case3To4(const hkVector4f& normal); // 0x00E8A460
public:
    hkResult builtTetrahedron(hkpGskOut& output, hkpGskStatus& gjkStatus); // 0x00E88800
    void convertResultToFeature(hkcdWingedEdge& polytope, hkcdWingedEdge::Triangle* nearestTriangle, hkpGskOut& output); // 0x00E881A0
    hkResult tryToExpandPolytope(hkcdWingedEdge& we, hkcdWingedEdge::Triangle* nearestTriangle, hkcdWingedEdge::Vertex* newVertex, hkpGskStatus& failureReason); // 0x00E883B0
    void getPointForSimplex(const hkVector4f& direction, hkcdWingedEdge::Vertex& result); // 0x00E88080
    void findBestProjection(const hkVector4f& direction, const hkVector4f& from, hkcdWingedEdge::Vertex& result, hkSimdFloat32& bestDistanceOut); // 0x00E882E0
    hkpConvexPenetrationUtil() {} // 0x00E89CC0

public:
    hkTransformf m_aTb; // 0x00
    hkVector4f m_epsTol; // 0x40
    hkPadSpu<float> m_epsTolSqrd; // 0x50
    hkPadSpu<hkpConvexShape const*> m_shapeA; // 0x54
    hkPadSpu<hkpConvexShape const*> m_shapeB; // 0x58
    hkPadSpu<int> m_backupTries; // 0x5C
    hkPadSpu<hkcdVertex*> m_pointsAinA; // 0x60
    hkPadSpu<hkcdVertex*> m_pointsBinB; // 0x64
    hkPadSpu<hkVector4f*> m_simplex; // 0x68
    hkPadSpu<int> m_simplexSize; // 0x6C
};

hkpGskStatus hkCalculatePenetrationDepth(const hkpConvexShape* shapeA, const hkpConvexShape* shapeB, const hkTransformf& btoa, float epsTol, hkcdVertex* pointsAinA, hkcdVertex* pointsBinB, hkVector4f* simplex, int simplexSize, hkpGskOut& output); // 0x00E89370
hkResult hkCalcMultiPenetrationDepth(const hkTransformf& transformA, const hkpConvexShape* shapeA, const hkpConvexShape** shapeBs, int numShapeBs, const hkTransformf& aTb, hkContactPoint** pointsOut); // 0x00E894A0
