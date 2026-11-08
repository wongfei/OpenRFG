// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/welding/hkpweldingutility.h
//
// Havok 2014.2.5 (havok_impl.md HK3): the PDB declaration with the inline code of Havok 2014.1
// (hkpWeldingUtility.inl): snapping collision normals at triangle edges to the edges' allowed angle ranges. The
// table is filled by initWeldingTable (hkpweldingutility.cpp). Hand-maintained (nobuild in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"

class hkClass;
struct hkInternalClassMember;

// size 0x1
class hkpWeldingUtility
{
public:
    enum WeldingType
    {
        WELDING_TYPE_ANTICLOCKWISE = 0,
        WELDING_TYPE_CLOCKWISE = 4,
        WELDING_TYPE_TWO_SIDED = 5,
        WELDING_TYPE_NONE = 6,
    };

    // The values are offsets into an m_sinCosTable entry (the snap vector's sin and cos).
    enum SectorType
    {
        ACCEPT_0 = 1,
        SNAP_0 = 0,
        REJECT = 2,
        SNAP_1 = 4,
        ACCEPT_1 = 3,
    };

    enum NumAngles
    {
        NUM_ANGLES = 31,
    };

    // size 0x18
    struct SinCosTableEntry
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(SinCosTableEntry);

        float m_cosAccept0; // 0x00
        float m_sinAccept0; // 0x04
        float m_cosSnap0; // 0x08
        float m_cosSnap1; // 0x0C
        float m_cosAccept1; // 0x10
        float m_sinAccept1; // 0x14
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpWeldingUtility);
    static const hkClass& staticClass(); // 0x00D7D550
    static void initWeldingTable(float edgeNormalSnapDeltaAngle, float triangleNormalSnapDeltaAngle); // 0x0104CB50
    static inline void calcSnapVector(const hkVector4f& triangleNormal, const hkVector4f& edge, int edgeBitcode, SectorType sector, hkVector4f& snapVectorOut); // 0x00D83BB0
    static inline SectorType getSector(const hkVector4f& triangleNormal, const hkVector4f& collisionNormal, int edgeBitcode); // 0x00D83AA0
    static inline void snapCollisionNormal(const hkVector4f& triangleNormal, const hkVector4f& edge, int edgeBitcode, SectorType sector, hkVector4f& collisionNormalInOut); // 0x00D83D10
    static inline hkBool shouldSnapOneSided(WeldingType weldingType, const hkVector4f& triangleNormal, const hkVector4f& collisionNormal, int edgeBitcode); // 0x00D83B20
    static inline void calcSnapVectorOneSided(const hkVector4f& triangleNormal, const hkVector4f& edge, int edgeBitcode, WeldingType weldingType, hkVector4f& snapVectorOut); // 0x00D83C60
    static unsigned short calcScaledWeldingInfo(const hkVector4f* vertices, unsigned short weldingInfo, WeldingType weldingType, const hkVector4f& scale); // 0x0104D790
    static int calcEdgeAngleBitcode(float angle); // 0x0104C3F0

    static const hkInternalClassMember Members[];
    alignas(16) static SinCosTableEntry m_sinCosTable[32]; // 0x03A29A30 (NUM_ANGLES + 1)
};

// The sector of the collision normal at an edge (the angle between the triangle normal and the collision normal against
// the edge's limits).
inline hkpWeldingUtility::SectorType hkpWeldingUtility::getSector(const hkVector4f& triangleNormal, const hkVector4f& collisionNormal, int edgeBitcode)
{
    const hkSimdReal cosAngle = triangleNormal.dot<3>(collisionNormal);
    hkVector4 vCosAngle; vCosAngle.setAll(cosAngle);
    const SinCosTableEntry& e = m_sinCosTable[edgeBitcode];
    hkVector4 vCosLimits;
    vCosLimits.set(e.m_cosAccept1, e.m_cosSnap1, e.m_cosSnap0, e.m_cosAccept0);
    const hkVector4Comparison cmp = vCosAngle.greaterEqual(vCosLimits);
    // a nibble per comparison mask: ACCEPT_1 (3), SNAP_1 (4), REJECT (2), SNAP_0 (0), ACCEPT_0 (1)
    const hkUint64 retCode = 0x1111111100002243ULL;
    return (SectorType)((retCode >> (hkUint64)(cmp.getMask() << 2)) & 0xF);
}

inline hkBool hkpWeldingUtility::shouldSnapOneSided(WeldingType weldingType, const hkVector4f& triangleNormal, const hkVector4f& collisionNormal, int edgeBitcode)
{
    if (edgeBitcode == NUM_ANGLES)
    {
        return false;
    }
    const float cosAngle = triangleNormal.dot<3>(collisionNormal).getReal();
    const float* sinCosTableEntry = &m_sinCosTable[edgeBitcode].m_cosAccept0;
    return (weldingType == WELDING_TYPE_ANTICLOCKWISE) ? cosAngle < sinCosTableEntry[weldingType] : cosAngle > sinCosTableEntry[weldingType];
}

inline void hkpWeldingUtility::calcSnapVector(const hkVector4f& triangleNormal, const hkVector4f& edge, int edgeBitcode, SectorType sector, hkVector4f& snapVectorOut)
{
    hkVector4 orthNormalEdge; orthNormalEdge.setCross(edge, triangleNormal);
    // the snap vector from the sin and cos the sector indexes (SNAP_0 or SNAP_1)
    const float* sinCosTableEntry = &m_sinCosTable[edgeBitcode].m_cosAccept0;
    hkSimdReal sinus = hkSimdReal::fromFloat(sinCosTableEntry[sector + 0]);
    hkSimdReal cosin = hkSimdReal::fromFloat(sinCosTableEntry[sector + 1]);
    hkVector4 hVec0; hVec0.setMul(sinus, triangleNormal);
    hkVector4 hVec1; hVec1.setMul(cosin, orthNormalEdge);
    snapVectorOut.setAdd(hVec0, hVec1);
    snapVectorOut.normalize<3>();
}

inline void hkpWeldingUtility::calcSnapVectorOneSided(const hkVector4f& triangleNormal, const hkVector4f& edge, int edgeBitcode, WeldingType weldingType, hkVector4f& snapVectorOut)
{
    calcSnapVector(triangleNormal, edge, edgeBitcode, (SectorType)weldingType, snapVectorOut);
}

inline void hkpWeldingUtility::snapCollisionNormal(const hkVector4f& triangleNormal, const hkVector4f& edge, int edgeBitcode, SectorType sector, hkVector4f& collisionNormalInOut)
{
    hkVector4 snapVector; calcSnapVector(triangleNormal, edge, edgeBitcode, sector, snapVector);

    // the collision normal's projections onto the snap vector and the edge
    hkSimdReal dot0 = snapVector.dot<3>(collisionNormalInOut);
    hkSimdReal dot1 = edge.dot<3>(collisionNormalInOut);

    hkVector4 hVec0; hVec0.setMul(dot0, snapVector);
    hkVector4 hVec1; hVec1.setMul(dot1, edge);
    collisionNormalInOut.setAdd(hVec0, hVec1);
    collisionNormalInOut.normalize<3>();
}
