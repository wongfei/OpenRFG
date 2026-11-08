// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/boxbox/hkpboxboxcollisiondetection.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): box-box collision detection, from rfg.exe. The separating axis test over the
// 15 axes (checkIntersection), the closest feature pair by feature walking (findClosestPoint), and a persistent
// manifold of up to 8 feature pairs (face A/vertex B, face B/vertex A, edge/edge) refreshed every frame and grown
// with the neighbouring features (calcManifold). No SDK source exists for 2014.2.5; ported from the binary.
//
// Feature ids (hkpFeatureContactPoint): bits 0-3 the feature index (0-2 face of A, 4-6 face of B, 8-18 edge pair),
// bits 4-6 the vertex sign mask, bit 3 (faces) or bit 7 the normal sign.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/boxbox/hkpboxboxcollisiondetection.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/contactmgr/hkpcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscdpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/boxbox/hkpboxboxcontactpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/boxbox/hkpboxboxmanifold.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

#include <string.h>

using namespace hkGskExact;

hkBool hkpBoxBoxCollisionDetection::m_attemptToFindAllEdges = false; // declared only; rfg.exe uses hkpBoxBoxAgent's (0x03A29D71)
const float hkpBoxBoxCollisionDetection::m_manifoldConsistencyCheckAngularCosTolerance = 0.95f; // 0x0135B540: 0x3F733333
const float hkpBoxBoxCollisionDetection::m_coplanerAngularCosTolerance = 0.98f; // 0x0135B544: 0x3F7AE148
const float hkpBoxBoxCollisionDetection::m_contactNormalAngularCosTolerance = 0.95f; // 0x0135B548: 0x3F733333
const float hkpBoxBoxCollisionDetection::m_edgeEndpointTolerance = 1.1920929e-7f; // 0x0135B54C: 0x34000000
const int hkpBoxBoxCollisionDetection::m_maxFeaturesToReject = 8; // 0x0135B550

// ---------------------------------------------------------------------------------------------------------------------
// file statics (register arguments in the binary)

// 0x00DD46A0: result = greaterV where than < selectIf (per lane)
static inline void selectIfGT3(hkVector4f& result, const hkSimdFloat32& selectIf, const hkVector4f& greaterV, const hkSimdFloat32& than)
{
    const Q mask = _mm_cmplt_ps(than.m_real, selectIf.m_real);
    result.m_quad = _mm_or_ps(_mm_and_ps(greaterV.m_quad, mask), _mm_andnot_ps(mask, result.m_quad));
}

// 0x00DD4650: the vertex signs of a feature id as +-1 per axis (bit set: -1)
static inline void getAxisMapFromFeatureBitSet(int bitSet, hkVector4f& v)
{
    v.m_quad = _mm_xor_ps(signBits(laneMask((bitSet >> 4) & 7)), g_vectorfConstants[HK_QUADREAL_1]);
}

// 0x00DD4680: the normal sign of a face feature id as a lane mask
static inline void getNormalSignFromFeatureBitSet(unsigned char bitSet, hkVector4fComparison& signMask)
{
    signMask.m_mask = (bitSet & 8) ? _mm_castsi128_ps(_mm_set1_epi32(-1)) : _mm_setzero_ps();
}

// The axis map with one lane cleared (0-3).
static inline Q hkBoxBox_clearLane(Q a, int lane)
{
    switch (lane)
    {
    case 1:
        return _mm_shuffle_ps(_mm_unpacklo_ps(a, _mm_setzero_ps()), a, 0xE4);
    case 2:
        return _mm_shuffle_ps(a, _mm_unpackhi_ps(a, _mm_setzero_ps()), 0xB4);
    case 3:
        return _mm_castsi128_ps(_mm_srli_si128(_mm_slli_si128(_mm_castps_si128(a), 4), 4));
    default:
        return _mm_move_ss(a, _mm_setzero_ps());
    }
}

// 0x00DD46D0: the edge-edge feature ids from the two vertex sign maps (the edge axes cleared). A's id has bit 7 set,
// B's signs are inverted.
static void setEdgeFeatureBitSetFromAxisMap(const hkVector4f& axisMapAp, const hkVector4f& axisMapBp, int featureIndexA, int featureIndexB, hkpFeatureContactPoint& fcp)
{
    const Q a = hkBoxBox_clearLane(axisMapAp.m_quad, featureIndexA);
    const Q b = hkBoxBox_clearLane(axisMapBp.m_quad, featureIndexB);
    const int maskB = _mm_movemask_ps(_mm_cmplt_ps(b, _mm_setzero_ps()));
    const int maskA = _mm_movemask_ps(_mm_cmplt_ps(a, _mm_setzero_ps()));
    fcp.m_featureIdA = (unsigned char)((((maskA | 0xF8) << 4) & 0xFF) | featureIndexA);
    fcp.m_featureIdB = (unsigned char)((((~maskB) & 7) << 4) + featureIndexB);
}

// 0x00DD47E0: a face feature id from the vertex sign map and the normal sign
static inline int calcFaceFeatureBitSetFromAxisMap(const hkVector4f& axisMap, const hkVector4fComparison& normalSign)
{
    int bits = (_mm_movemask_ps(_mm_cmplt_ps(axisMap.m_quad, _mm_setzero_ps())) & 7) << 4;
    if (_mm_movemask_ps(normalSign.m_mask))
    {
        bits |= 8;
    }
    return bits;
}

// The 2-bit counter of a terminal (an edge of the box) is incremented unless it reached 2.
static inline void hkBoxBox_incrementTerminal(unsigned int& terminals, int shift)
{
    const unsigned int one = 1u << shift;
    if (!(terminals & (one + one)))
    {
        terminals += one;
    }
}

// 0x00DD4290: the three edges at a vertex (but skipAxis's)
static void setTerminalsFromVertex(int featureBits, unsigned int& terminals, int skipAxis)
{
    const int v = featureBits >> 4;
    const int sx = ~v & 1;
    const int sy = ~(v >> 1) & 1;
    const int sz = ~(v >> 2) & 1;
    if (skipAxis != 0)
    {
        hkBoxBox_incrementTerminal(terminals, (sy + sz * 2) * 2);
    }
    if (skipAxis != 1)
    {
        hkBoxBox_incrementTerminal(terminals, (sx + sz * 2 + 4) * 2);
    }
    if (skipAxis != 2)
    {
        hkBoxBox_incrementTerminal(terminals, (sx + sy * 2 + 8) * 2);
    }
}

// 0x00DD4320: the edge of an edge feature
static void setTerminalsFromEdge(int featureBits, unsigned int& terminals)
{
    const int v = featureBits >> 4;
    const int sx = ~v & 1;
    const int sy = ~(v >> 1) & 1;
    const int sz = ~(v >> 2) & 1;
    const int axis = featureBits & 0xF;
    int index;
    if (axis == 0)
    {
        index = sy + sz * 2;
    }
    else if (axis == 1)
    {
        index = sx + sz * 2 + 4;
    }
    else
    {
        index = sx + sy * 2 + 8;
    }
    hkBoxBox_incrementTerminal(terminals, index * 2);
}

// 0x00DD4CD0: the largest of x, y, z: its index, and (1 << index) << 4 returned
static inline int getMaxPlaneMask3(const hkVector4f& v, int& indexOut)
{
    const Q q = v.m_quad;
    const Q m = _mm_max_ps(bcast<2>(q), _mm_max_ps(bcast<1>(q), bcast<0>(q)));
    const int mask = _mm_movemask_ps(_mm_and_ps(_mm_cmple_ps(m, q), laneMask(7)));
    const int index = hkVector4Comparison_maskToLastIndex[mask];
    indexOut = index;
    return (1 << index) << 4;
}

// ---------------------------------------------------------------------------------------------------------------------

// 0x00DC34E0 (.inl)
hkpBoxBoxCollisionDetection::hkpBoxBoxCollisionDetection(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput* env, hkpContactMgr* mgr, hkpProcessCollisionOutput* result, const hkTransformf& aTb, const hkTransformf& wTa, const hkVector4f& radiusA, const hkTransformf& wTb, const hkVector4f& radiusB, const hkSimdFloat32& tolerance)
    : m_bodyA(bodyA), m_bodyB(bodyB), m_env(env), m_contactMgr(mgr), m_result(result), m_wTa(wTa), m_wTb(wTb)
{
    m_aTb = aTb;
    m_radiusA = radiusA;
    m_radiusB = radiusB;
    m_boundaryTolerance.m_real = splat(0.01f); // 0x3C23D70A
    m_tolerance4.m_quad = setXYZ_W(tolerance.m_real, g_vectorfConstants[HK_QUADREAL_MAX]);
    m_keepRadiusA.m_quad = add(m_radiusA.m_quad, m_tolerance4.m_quad);
    m_keepRadiusB.m_quad = add(m_radiusB.m_quad, m_tolerance4.m_quad);
}

// 0x00DC3E40 (.inl): B's origin in A and A's origin in B (dinB = rotation^T * dinA)
void hkpBoxBoxCollisionDetection::initWorkVariables() const
{
    hkpBoxBoxCollisionDetection* self = const_cast<hkpBoxBoxCollisionDetection*>(this);
    self->m_dinA = m_aTb.getTranslation();
    self->m_dinB.m_quad = rotateInverse(m_aTb.getRotation(), m_dinA.m_quad);
}

// 0x00DC3F00 (.inl)
hkBool hkpBoxBoxCollisionDetection::getPenetrations()
{
    initWorkVariables();
    return checkIntersection(m_tolerance4) == HK_SUCCESS;
}

// 0x00DD7C70 (.inl): |dinA.z * bRa.col1 - dinA.y * bRa.col2|
template<int I, int J>
static inline Q hkBoxBox_vdProj(const hkVector4f& dinA, const hkRotationf& bRa);

template<>
inline Q hkBoxBox_vdProj<1, 2>(const hkVector4f& dinA, const hkRotationf& bRa)
{
    return abs(sub(mul(bcast<2>(dinA.m_quad), bRa.getColumn<1>().m_quad), mul(bcast<1>(dinA.m_quad), bRa.getColumn<2>().m_quad)));
}

// 0x00DD7CF0 (.inl): |dinA.y * bRa.col0 - dinA.x * bRa.col1|
template<>
inline Q hkBoxBox_vdProj<0, 1>(const hkVector4f& dinA, const hkRotationf& bRa)
{
    return abs(sub(mul(bcast<1>(dinA.m_quad), bRa.getColumn<0>().m_quad), mul(bcast<0>(dinA.m_quad), bRa.getColumn<1>().m_quad)));
}

// 0x00DD7CB0 (.inl): |dinA.x * bRa.col2 - dinA.z * bRa.col0|
template<>
inline Q hkBoxBox_vdProj<2, 0>(const hkVector4f& dinA, const hkRotationf& bRa)
{
    return abs(sub(mul(bcast<0>(dinA.m_quad), bRa.getColumn<2>().m_quad), mul(bcast<2>(dinA.m_quad), bRa.getColumn<0>().m_quad)));
}

// 0x00DD7E30 (.inl): B's vertex inside A's face rectangle (grown by the keep distance)
unsigned int hkpBoxBoxCollisionDetection::isValidFaceAVertexB(const hkpFeaturePointCache& fpp) const
{
    return (_mm_movemask_ps(_mm_cmplt_ps(abs(fpp.m_vA.m_quad), m_keepRadiusA.m_quad)) & 7) == 7;
}

// 0x00DD7E60 (.inl): the distance of B's vertex from A's face (the face's coordinate of the vertex, flipped to the
// face's side, less A's half extent)
void hkpBoxBoxCollisionDetection::calcDistanceFaceAVertexB(hkpFeaturePointCache& fpp) const
{
    const Q v = sub(_mm_xor_ps(signBits(fpp.m_normalIsFlipped.m_mask), fpp.m_vA.m_quad), m_radiusA.m_quad);
    Q d = _mm_and_ps(v, laneMask(1 << fpp.m_featureIndexA));
    d = _mm_or_ps(_mm_shuffle_ps(d, d, 0x4E), d);
    fpp.m_distance.m_real = _mm_or_ps(_mm_shuffle_ps(d, d, 0xB1), d);
}

// 0x00DD7EB0 (.inl)
unsigned int hkpBoxBoxCollisionDetection::isValidFaceBVertexA(const hkpFeaturePointCache& fpp) const
{
    return (_mm_movemask_ps(_mm_cmplt_ps(abs(fpp.m_vB.m_quad), m_keepRadiusB.m_quad)) & 7) == 7;
}

// 0x00DD7EE0 (.inl): as calcDistanceFaceAVertexB on B's face (the flip inverted; the face index less 4)
void hkpBoxBoxCollisionDetection::calcDistanceFaceBVertexA(hkpFeaturePointCache& fpp) const
{
    const Q notFlipped = _mm_castsi128_ps(_mm_cmpeq_epi32(_mm_setzero_si128(), _mm_castps_si128(fpp.m_normalIsFlipped.m_mask)));
    const Q v = sub(_mm_xor_ps(signBits(notFlipped), fpp.m_vB.m_quad), m_radiusB.m_quad);
    Q d = _mm_and_ps(v, laneMask(1 << (fpp.m_featureIndexA - 4)));
    d = _mm_or_ps(_mm_shuffle_ps(d, d, 0x4E), d);
    fpp.m_distance.m_real = _mm_or_ps(_mm_shuffle_ps(d, d, 0xB1), d);
}

// The rows of a rotation (the transposed columns; the w lanes as HK_TRANSPOSE3 leaves them).
static inline void hkBoxBox_rows(Q c0, Q c1, Q c2, Q& r0, Q& r1, Q& r2)
{
    const Q lo = _mm_unpacklo_ps(c0, c1);
    const Q hi = _mm_unpackhi_ps(c0, c1);
    r0 = _mm_movelh_ps(lo, c2);
    r1 = _mm_shuffle_ps(_mm_movehl_ps(r0, lo), c2, 0xD4);
    r2 = _mm_shuffle_ps(hi, c2, 0xE4);
}

// rsqrt with one Newton step, without the zero mask
static inline Q hkBoxBox_invSqrt(Q x)
{
    const Q r = _mm_rsqrt_ps(x);
    return mul(sub(splat(3.0f), mul(mul(r, x), r)), mul(r, splat(0.5f)));
}

// 0x00DD4810
// The separating axis test: the separation along A's face normals (m_sepDist[0]), B's (m_sepDist[1]) and the nine
// edge-edge cross products, one vector per edge of A (m_sepDist[2..4]; an axis of nearly parallel edges gets -1e17).
// HK_FAILURE as soon as a separation is beyond the tolerance.
hkResult hkpBoxBoxCollisionDetection::checkIntersection(const hkVector4f& tolerance) const
{
    hkpBoxBoxCollisionDetection* self = const_cast<hkpBoxBoxCollisionDetection*>(this);
    const Q big = splat(999.99994f); // 0x4479FFFF: 1 / |axis| above this is a degenerate (parallel) axis
    const Q degenerate = splat(-1e17f); // 0xDBB1A2BC
    const Q rA = m_radiusA.m_quad;
    const Q rB = m_radiusB.m_quad;
    const Q c0 = m_aTb.getRotation().getColumn<0>().m_quad;
    const Q c1 = m_aTb.getRotation().getColumn<1>().m_quad;
    const Q c2 = m_aTb.getRotation().getColumn<2>().m_quad;
    const Q a0 = abs(c0);
    const Q a1 = abs(c1);
    const Q a2 = abs(c2);

    // A's faces
    {
        const Q proj = add(add(add(mul(bcast<0>(rB), a0), mul(bcast<1>(rB), a1)), mul(bcast<2>(rB), a2)), rA);
        const Q sep = sub(abs(m_dinA.m_quad), proj);
        if (_mm_movemask_ps(_mm_cmplt_ps(tolerance.m_quad, sep)))
        {
            return HK_FAILURE;
        }
        self->m_sepDist[0].m_quad = sep;
    }

    // B's faces
    Q absRow0, absRow1, absRow2;
    hkBoxBox_rows(a0, a1, a2, absRow0, absRow1, absRow2);
    {
        const Q proj = add(add(add(mul(bcast<1>(rA), absRow1), mul(bcast<0>(rA), absRow0)), mul(absRow2, bcast<2>(rA))), rB);
        const Q sep = sub(abs(m_dinB.m_quad), proj);
        if (_mm_movemask_ps(_mm_cmplt_ps(tolerance.m_quad, sep)))
        {
            return HK_FAILURE;
        }
        self->m_sepDist[1].m_quad = sep;
    }

    // the edge pairs: A's edge i against B's three edges (axis = e_i x b_j, |axis|^2 from the other two rows)
    Q r0, r1, r2;
    hkBoxBox_rows(c0, c1, c2, r0, r1, r2);
    const Q r0sq = mul(r0, r0);
    const Q r1sq = mul(r1, r1);
    const Q r2sq = mul(r2, r2);
    const Q rB_yxxy = _mm_shuffle_ps(rB, rB, 0x41);
    const Q rB_zzyy = _mm_shuffle_ps(rB, rB, 0x5A);
    const Q dinA = m_dinA.m_quad;

    // A's x edge
    {
        const Q len2 = _mm_max_ps(add(r2sq, r1sq), g_vectorfConstants[HK_QUADREAL_EPS_SQRD]);
        Q proj = add(mul(bcast<2>(rA), absRow1), mul(bcast<1>(rA), absRow2));
        proj = add(proj, mul(_mm_shuffle_ps(absRow0, absRow0, 0x5A), rB_yxxy));
        proj = add(proj, mul(_mm_shuffle_ps(absRow0, absRow0, 0x41), rB_zzyy));
        const Q d = abs(sub(mul(bcast<2>(dinA), r1), mul(bcast<1>(dinA), r2)));
        const Q invLen = hkBoxBox_invSqrt(len2);
        const Q sep = mul(invLen, sub(d, proj));
        if (_mm_movemask_ps(_mm_cmplt_ps(m_tolerance4.m_quad, sep)))
        {
            return HK_FAILURE;
        }
        const Q parallel = _mm_cmplt_ps(big, invLen);
        self->m_sepDist[2].m_quad = _mm_or_ps(_mm_andnot_ps(parallel, sep), _mm_and_ps(parallel, degenerate));
    }
    // A's y edge
    {
        const Q len2 = _mm_max_ps(add(r2sq, r0sq), g_vectorfConstants[HK_QUADREAL_EPS_SQRD]);
        Q proj = add(mul(bcast<2>(rA), absRow0), mul(bcast<0>(rA), absRow2));
        proj = add(proj, mul(_mm_shuffle_ps(absRow1, absRow1, 0x5A), rB_yxxy));
        proj = add(proj, mul(_mm_shuffle_ps(absRow1, absRow1, 0x41), rB_zzyy));
        const Q d = abs(sub(mul(bcast<0>(dinA), r2), mul(bcast<2>(dinA), r0)));
        const Q invLen = hkBoxBox_invSqrt(len2);
        const Q sep = mul(invLen, sub(d, proj));
        if (_mm_movemask_ps(_mm_cmplt_ps(m_tolerance4.m_quad, sep)))
        {
            return HK_FAILURE;
        }
        const Q parallel = _mm_cmplt_ps(big, invLen);
        self->m_sepDist[3].m_quad = _mm_or_ps(_mm_andnot_ps(parallel, sep), _mm_and_ps(parallel, degenerate));
    }
    // A's z edge
    {
        hkRotationf rows;
        rows.setCols(hkVector4f(r0), hkVector4f(r1), hkVector4f(r2));
        const Q d = hkBoxBox_vdProj<0, 1>(m_dinA, rows);
        const Q len2 = _mm_max_ps(add(r1sq, r0sq), g_vectorfConstants[HK_QUADREAL_EPS_SQRD]);
        Q proj = add(mul(bcast<1>(rA), absRow0), mul(bcast<0>(rA), absRow1));
        proj = add(proj, mul(_mm_shuffle_ps(absRow2, absRow2, 0xD2), _mm_shuffle_ps(rB, rB, 0xC9)));
        proj = add(proj, mul(_mm_shuffle_ps(absRow2, absRow2, 0xC9), _mm_shuffle_ps(rB, rB, 0xD2)));
        const Q invLen = hkBoxBox_invSqrt(len2);
        const Q sep = mul(sub(d, proj), invLen);
        if (_mm_movemask_ps(_mm_cmplt_ps(m_tolerance4.m_quad, sep)))
        {
            return HK_FAILURE;
        }
        const Q parallel = _mm_cmplt_ps(big, invLen);
        self->m_sepDist[4].m_quad = _mm_or_ps(_mm_andnot_ps(parallel, sep), _mm_and_ps(parallel, degenerate));
    }
    return HK_SUCCESS;
}

// The face normal in world space for a face feature (a column of the body's transform), negated unless the feature
// normal is flipped, with the distance in w.
static inline Q hkBoxBox_faceNormal(const hkTransformf& wT, int axis, const hkVector4fComparison& normalIsFlipped, const hkSimdFloat32& distance)
{
    const Q notFlipped = _mm_castsi128_ps(_mm_cmpeq_epi32(_mm_setzero_si128(), _mm_castps_si128(normalIsFlipped.m_mask)));
    const Q n = _mm_xor_ps(signBits(notFlipped), wT.getRotation().getColumn(axis).m_quad);
    return setXYZ_W(n, distance.m_real);
}

// 0x00DD7F40 (.inl): A's face against B's vertex: the vertex in B and in A
void hkpBoxBoxCollisionDetection::faceAVertexBValidationDataFromFeatureId(hkpFeaturePointCache& fpp, const hkpFeatureContactPoint& fcp) const
{
    fpp.m_featureIndexA = fcp.m_featureIdA;
    getNormalSignFromFeatureBitSet(fcp.m_featureIdB, fpp.m_normalIsFlipped);
    hkVector4f axisMap;
    getAxisMapFromFeatureBitSet(fcp.m_featureIdB, axisMap);
    fpp.m_vB.m_quad = mul(axisMap.m_quad, m_radiusB.m_quad);
    fpp.m_vA.m_quad = transformPos(m_aTb, fpp.m_vB.m_quad);
}

// 0x00DD7FD0 (.inl)
void hkpBoxBoxCollisionDetection::faceAVertexBContactPointFromFeaturePointCache(hkpProcessCdPoint& ccpOut, const hkpFeatureContactPoint& fcp, const hkpFeaturePointCache& fpp) const
{
    ccpOut.m_contactPointId = fcp.m_contactPointId;
    ccpOut.m_contact.m_position.m_quad = transformPos(m_wTb, fpp.m_vB.m_quad);
    ccpOut.m_contact.m_separatingNormal.m_quad = hkBoxBox_faceNormal(m_wTa, fcp.m_featureIdA, fpp.m_normalIsFlipped, fpp.m_distance);
}

// 0x00DD8050 (.inl): B's face against A's vertex: the vertex in A and in B
void hkpBoxBoxCollisionDetection::faceBVertexAValidationDataFromFeatureId(hkpFeaturePointCache& fpp, const hkpFeatureContactPoint& fcp) const
{
    fpp.m_featureIndexA = fcp.m_featureIdA;
    getNormalSignFromFeatureBitSet(fcp.m_featureIdB, fpp.m_normalIsFlipped);
    hkVector4f axisMap;
    getAxisMapFromFeatureBitSet(fcp.m_featureIdB, axisMap);
    fpp.m_vA.m_quad = mul(axisMap.m_quad, m_radiusA.m_quad);
    fpp.m_vB.m_quad = rotateInverse(m_aTb.getRotation(), sub(fpp.m_vA.m_quad, m_aTb.getTranslation().m_quad));
}

// 0x00DD8150 (.inl)
void hkpBoxBoxCollisionDetection::faceBVertexAContactPointFromFeaturePointCache(hkpProcessCdPoint& ccpOut, const hkpFeatureContactPoint& fcp, const hkpFeaturePointCache& fpp) const
{
    ccpOut.m_contactPointId = fcp.m_contactPointId;
    ccpOut.m_contact.m_position.m_quad = transformPos(m_wTa, fpp.m_vA.m_quad);
    ccpOut.m_contact.m_separatingNormal.m_quad = hkBoxBox_faceNormal(m_wTb, fcp.m_featureIdA - 4, fpp.m_normalIsFlipped, fpp.m_distance);
}

// 0x00DD7A60 (.inl)
void hkpBoxBoxCollisionDetection::edgeEdgeValidationDataFromFeatureId(hkpFeaturePointCache& fpp, const hkpFeatureContactPoint& fcp) const
{
    fpp.m_featureIndexA = fcp.m_featureIdA;
    fpp.m_featureIndexB = fcp.m_featureIdB;
}

// 0x00DD81E0 (.inl): the closest point on A's edge, the edge normal (in A) in world space
void hkpBoxBoxCollisionDetection::edgeEdgeContactPointFromFeaturePointCache(hkpProcessCdPoint& ccpOut, const hkpFeatureContactPoint& fcp, const hkpFeaturePointCache& fpp) const
{
    ccpOut.m_contactPointId = fcp.m_contactPointId;
    ccpOut.m_contact.m_position.m_quad = transformPos(m_wTa, fpp.m_vA.m_quad);
    ccpOut.m_contact.m_separatingNormal.m_quad = setXYZ_W(rotate(m_wTa.getRotation(), fpp.m_nA.m_quad), fpp.m_distance.m_real);
}

// 0x00DD8270
void hkpBoxBoxCollisionDetection::contactPointFromFeaturePointCache(hkpProcessCdPoint& ccpOut, const hkpFeatureContactPoint& fcp, const hkpFeaturePointCache& fpp) const
{
    if (fpp.m_featureIndexA <= 2)
    {
        faceAVertexBContactPointFromFeaturePointCache(ccpOut, fcp, fpp);
    }
    else if (fpp.m_featureIndexA <= 6)
    {
        faceBVertexAContactPointFromFeaturePointCache(ccpOut, fcp, fpp);
    }
    else
    {
        edgeEdgeContactPointFromFeaturePointCache(ccpOut, fcp, fpp);
    }
}

// The lane of a vector selected by a one-lane mask, in all lanes.
static inline Q hkBoxBox_lane(Q v, Q mask)
{
    Q t = _mm_and_ps(mask, v);
    t = _mm_or_ps(_mm_shuffle_ps(t, t, 0x4E), t);
    return _mm_or_ps(_mm_shuffle_ps(t, t, 0xB1), t);
}

// 0x00DD9010: A's face along an axis against B's vertex most opposite to it (signs from the row of the rotation with
// a 1e-5 bias)
void hkpBoxBoxCollisionDetection::faceAVertexBValidationDataFromFeatureIndex(hkpFeaturePointCache& fpp, int featureIndex) const
{
    fpp.m_featureIndexA = featureIndex;
    hkMatrix3f bRa;
    bRa.setTranspose(m_aTb.getRotation());
    const Q row = bRa.getColumn(featureIndex).m_quad;
    const Q flipped = _mm_cmplt_ps(hkBoxBox_lane(m_dinA.m_quad, laneMask(1 << featureIndex)), _mm_setzero_ps());
    fpp.m_normalIsFlipped.m_mask = flipped;
    const Q notFlipped = _mm_castsi128_ps(_mm_cmpeq_epi32(_mm_setzero_si128(), _mm_castps_si128(flipped)));
    const Q vB = _mm_xor_ps(signBits(notFlipped), m_radiusB.m_quad);
    fpp.m_vB.m_quad = vB;
    const Q bias = _mm_xor_ps(signBits(flipped), splat(1e-5f)); // 0x0135B690: 0x3727C5AC
    const Q s = add(_mm_xor_ps(signBits(_mm_cmple_ps(m_dinB.m_quad, _mm_setzero_ps())), bias), row);
    fpp.m_vB.m_quad = _mm_xor_ps(signBits(s), vB);
    fpp.m_vA.m_quad = transformPos(m_aTb, fpp.m_vB.m_quad);
}

// 0x00DD9140: as above for B's face (index + 4) and A's vertex
void hkpBoxBoxCollisionDetection::faceBVertexAValidationDataFromFeatureIndex(hkpFeaturePointCache& fpp, int featureIndex) const
{
    fpp.m_featureIndexA = featureIndex + 4;
    const Q col = m_aTb.getRotation().getColumn(featureIndex).m_quad;
    const Q flipped = _mm_cmplt_ps(hkBoxBox_lane(m_dinB.m_quad, laneMask(1 << featureIndex)), _mm_setzero_ps());
    fpp.m_normalIsFlipped.m_mask = flipped;
    const Q flipSign = signBits(flipped);
    const Q vA = _mm_xor_ps(m_radiusA.m_quad, flipSign);
    fpp.m_vA.m_quad = vA;
    const Q bias = _mm_xor_ps(splat(-1e-5f), flipSign); // 0x0135B6A0: 0xB727C5AC
    const Q s = add(_mm_xor_ps(signBits(_mm_cmple_ps(m_dinA.m_quad, _mm_setzero_ps())), bias), col);
    fpp.m_vA.m_quad = _mm_xor_ps(signBits(s), vA);
    fpp.m_vB.m_quad = rotateInverse(m_aTb.getRotation(), sub(fpp.m_vA.m_quad, m_aTb.getTranslation().m_quad));
}

// 0x00DD92C0: the edge pair's axis (A's edge x B's edge, towards B) in vA and in B's space in vB
void hkpBoxBoxCollisionDetection::edgeEdgeValidationDataFromFeatureIndex(hkpFeaturePointCache& fpp) const
{
    const Q eA = g_vectorfConstants[HK_QUADREAL_1000 + fpp.m_featureIndexA];
    const Q colB = m_aTb.getRotation().getColumn(fpp.m_featureIndexB).m_quad;
    const Q n = cross(eA, colB);
    const Q d = dot3(m_dinA.m_quad, n);
    const Q axis = _mm_xor_ps(signBits(_mm_cmplt_ps(d, _mm_setzero_ps())), n);
    fpp.m_vA.m_quad = axis;
    fpp.m_vB.m_quad = rotateInverse(m_aTb.getRotation(), axis);
}

// 0x00DD7BC0: the manifold normal (in A) of a point: the face normal, or for an edge pair the validated edge normal
// (else the old one); w kept
void hkpBoxBoxCollisionDetection::calcManifoldNormal(hkVector4f& manifoldN, const hkpFeatureContactPoint& fcp, hkpFeaturePointCache& fpp, bool isPointValidated) const
{
    const Q old = manifoldN.m_quad;
    const Q notFlipped = _mm_castsi128_ps(_mm_cmpeq_epi32(_mm_setzero_si128(), _mm_castps_si128(fpp.m_normalIsFlipped.m_mask)));
    Q n;
    if (fpp.m_featureIndexA <= 2)
    {
        n = _mm_xor_ps(signBits(notFlipped), g_vectorfConstants[HK_QUADREAL_1000 + fcp.m_featureIdA]);
    }
    else if (fpp.m_featureIndexA <= 6)
    {
        n = _mm_xor_ps(signBits(notFlipped), m_aTb.getRotation().getColumn(fcp.m_featureIdA - 4).m_quad);
    }
    else
    {
        n = isPointValidated ? fpp.m_nA.m_quad : old;
    }
    manifoldN.m_quad = setXYZ_W(n, old);
}

// 0x00DD7B80
void hkpBoxBoxCollisionDetection::removePoint(hkpBoxBoxManifold& manifold, int index) const
{
    if (manifold.m_contactPoints[index].m_featureIdA <= 6)
    {
        manifold.m_faceVertexFeatureCount--;
    }
    m_contactMgr->removeContactPointImpl(manifold.m_contactPoints[index].m_contactPointId, *m_result->m_constraintOwner);
    manifold.removePoint(index);
}

// 0x00DD4D10
// The closest points of the edge pair (A's edge from its feature corner along -axis, B's from its corner along +axis,
// both full length): false when they are at an end of either edge (within m_edgeEndpointTolerance), the edges are
// nearly parallel (|n| below the boundary tolerance) or apart beyond the tolerance. Sets vA (the point on A's edge),
// nA (the normal from B to A) and the distance.
bool hkpBoxBoxCollisionDetection::isValidEdgeEdge(hkpFeaturePointCache& fpp) const
{
    const int idA = fpp.m_featureIndexA;
    const int idB = fpp.m_featureIndexB;
    hkVector4f axisMapA;
    getAxisMapFromFeatureBitSet(idA, axisMapA);
    const Q pA = mul(m_radiusA.m_quad, axisMapA.m_quad);
    hkVector4f axisMapB;
    getAxisMapFromFeatureBitSet(idB, axisMapB);
    const Q pB = mul(axisMapB.m_quad, m_radiusB.m_quad);
    const int iB = idB & 0xF;
    const int iA = idA & 0xF;
    const Q maskB = laneMask(1 << iB);
    const Q hB = hkBoxBox_lane(m_radiusB.m_quad, maskB);
    const Q dB = mul(mul(hB, g_vectorfConstants[HK_QUADREAL_2]), m_aTb.getRotation().getColumn(iB).m_quad);
    const Q d = sub(transformPos(m_aTb, pB), pA);
    const Q maskA = laneMask(1 << iA);
    const Q eAlen = mul(hkBoxBox_lane(m_radiusA.m_quad, maskA), sub(_mm_setzero_ps(), g_vectorfConstants[HK_QUADREAL_2]));
    const Q eAdB = mul(hkBoxBox_lane(dB, maskA), eAlen);
    const Q dB2 = mul(mul(hB, hB), g_vectorfConstants[HK_QUADREAL_4]);
    const Q dBd = dot3(dB, d);
    const Q tol = splat(m_edgeEndpointTolerance);
    const Q denom = abs(sub(mul(mul(eAlen, eAlen), dB2), mul(eAdB, eAdB)));
    const Q numer = sub(mul(mul(hkBoxBox_lane(d, maskA), eAlen), dB2), mul(dBd, eAdB));
    const Q oneMinusTol = sub(g_vectorfConstants[HK_QUADREAL_1], tol);
    if (lessEqualOrNaN(numer, mul(denom, tol)) || greaterEqual(numer, mul(denom, oneMinusTol)))
    {
        return false;
    }
    const Q t = mul(recip23(denom), numer);
    const Q s = sub(mul(t, eAdB), dBd);
    if (lessEqualOrNaN(s, mul(tol, dB2)) || greaterEqual(s, mul(oneMinusTol, dB2)))
    {
        return false;
    }

    // the point on A's edge, the normal
    const Q pointA = _mm_or_ps(_mm_and_ps(add(hkBoxBox_lane(pA, maskA), mul(t, eAlen)), maskA), _mm_andnot_ps(maskA, pA));
    const Q eAhalf = _mm_and_ps(mul(eAlen, g_vectorfConstants[HK_QUADREAL_INV_2]), maskA);
    const Q n = cross(eAhalf, dB);
    const Q len2 = dot3(n, n);
    const Q invLen = invSqrt23SetZero(len2);
    if (lessOrNaN(mul(invLen, len2), m_boundaryTolerance.m_real))
    {
        return false;
    }
    const Q unit = mul(invLen, n);
    const Q outward = _mm_xor_ps(signBits(_mm_cmplt_ps(dot3(unit, axisMapA.m_quad), _mm_setzero_ps())), unit);
    const Q dist = dot3(outward, d);
    if (greater(dist, bcast<0>(m_tolerance4.m_quad)))
    {
        return false;
    }
    fpp.m_vA.m_quad = pointA;
    fpp.m_nA.m_quad = neg(outward);
    fpp.m_distance.m_real = dist;
    return true;
}

// 0x00DD5610
// The feature pair of the axis with the least penetration (the largest separation in m_sepDist; among equal ones the
// highest index): IN_MANIFOLD if the manifold has it, VALID_POINT with fpp set if its contact is valid, otherwise
// the axis is rejected (its separation set to -MAX) and the next one tried, up to 8 times.
int hkpBoxBoxCollisionDetection::findClosestPoint(hkpBoxBoxManifold& manifold, hkpFeatureContactPoint& fcp, hkpFeaturePointCache& fpp) const
{
    hkpBoxBoxCollisionDetection* self = const_cast<hkpBoxBoxCollisionDetection*>(this);
    const __m128i four = _mm_set1_epi32(4); // g_intVectorConstants[HK_QUADINT_4]
    for (int tries = 8; tries > 0; tries--)
    {
        // the largest separation of the 15 axes (lane j of m_sepDist[i] is axis 4i + j)
        Q best = m_sepDist[0].m_quad;
        __m128i index = _mm_setr_epi32(0, 1, 2, 3); // g_intVectorConstants[HK_QUADINT_0123]
        __m128i bestIndex = index;
        for (int i = 1; i < 5; i++)
        {
            const Q cur = m_sepDist[i].m_quad;
            const __m128i larger = _mm_castps_si128(_mm_cmplt_ps(best, cur));
            index = _mm_add_epi32(index, four);
            bestIndex = _mm_or_si128(_mm_and_si128(larger, index), _mm_andnot_si128(larger, bestIndex));
            best = _mm_max_ps(cur, best);
        }
        const Q maxAll = _mm_max_ps(bcast<2>(best), _mm_max_ps(bcast<1>(best), bcast<0>(best)));
        const __m128i isMax = _mm_castps_si128(_mm_cmpeq_ps(maxAll, best));
        const __m128i candidates = _mm_or_si128(_mm_and_si128(isMax, bestIndex), _mm_andnot_si128(isMax, _mm_setzero_si128()));
        const __m128i c0 = _mm_shuffle_epi32(candidates, 0x00);
        const __m128i c1 = _mm_shuffle_epi32(candidates, 0x55);
        const __m128i c2 = _mm_shuffle_epi32(candidates, 0xAA);
        __m128i gt = _mm_cmpgt_epi32(c0, c1);
        const __m128i m01 = _mm_or_si128(_mm_and_si128(gt, c0), _mm_andnot_si128(gt, c1));
        gt = _mm_cmpgt_epi32(m01, c2);
        const int axis = _mm_cvtsi128_si32(_mm_or_si128(_mm_and_si128(gt, m01), _mm_andnot_si128(gt, c2)));

        if ((unsigned int)axis <= 2)
        {
            // face of A, vertex of B
            fcp.m_featureIdA = (unsigned char)axis;
            faceAVertexBValidationDataFromFeatureIndex(fpp, axis);
            hkVector4f vB = fpp.m_vB;
            fcp.m_featureIdB = (unsigned char)calcFaceFeatureBitSetFromAxisMap(vB, fpp.m_normalIsFlipped);
            if (manifold.findInManifold(fcp))
            {
                return HK_FINDCLOSESTPOINT_POINT_IN_MANIFOLD;
            }
            if (isValidFaceAVertexB(fpp))
            {
                calcDistanceFaceAVertexB(fpp);
                return HK_FINDCLOSESTPOINT_VALID_POINT;
            }
        }
        else if ((unsigned int)axis <= 6)
        {
            // face of B, vertex of A
            fcp.m_featureIdA = (unsigned char)axis;
            faceBVertexAValidationDataFromFeatureIndex(fpp, axis - 4);
            hkVector4f vA = fpp.m_vA;
            fcp.m_featureIdB = (unsigned char)calcFaceFeatureBitSetFromAxisMap(vA, fpp.m_normalIsFlipped);
            if (manifold.findInManifold(fcp))
            {
                return HK_FINDCLOSESTPOINT_POINT_IN_MANIFOLD;
            }
            if (isValidFaceBVertexA(fpp))
            {
                calcDistanceFaceBVertexA(fpp);
                return HK_FINDCLOSESTPOINT_VALID_POINT;
            }
        }
        else
        {
            // an edge pair: the axis gives the feature corners
            const int e = axis - 8;
            fpp.m_featureIndexA = e >> 2;
            fpp.m_featureIndexB = e & 3;
            edgeEdgeValidationDataFromFeatureIndex(fpp);
            setEdgeFeatureBitSetFromAxisMap(fpp.m_vA, fpp.m_vB, e >> 2, e & 3, fcp);
            fpp.m_featureIndexA = fcp.m_featureIdA;
            fpp.m_featureIndexB = fcp.m_featureIdB;
            if (manifold.findInManifold(fcp))
            {
                return HK_FINDCLOSESTPOINT_POINT_IN_MANIFOLD;
            }
            if (isValidEdgeEdge(fpp))
            {
                return HK_FINDCLOSESTPOINT_VALID_POINT;
            }
        }

        // reject the axis
        const Q lane = laneMask(1 << (axis & 3));
        hkVector4f& sep = self->m_sepDist[axis >> 2];
        sep.m_quad = _mm_or_ps(_mm_and_ps(lane, g_vectorfConstants[HK_QUADREAL_MINUS_MAX]), _mm_andnot_ps(lane, sep.m_quad));
    }
    return HK_FINDCLOSESTPOINT_NO_VALID_POINT;
}

// 0x00DD83D0
// Adds the feature pair as a contact point (output point and contact manager id). A face-vertex point within the
// tolerance of an existing face-vertex point (its output position), a full manifold, or a normal against the
// previous output point is rejected (HK_FAILURE); a refused id drops the point silently (HK_SUCCESS).
hkResult hkpBoxBoxCollisionDetection::addPoint(hkpBoxBoxManifold& manifold, const hkpFeaturePointCache& fpp, hkpFeatureContactPoint& fcp) const
{
    if (fcp.m_featureIdA <= 6)
    {
        const Q position = (fcp.m_featureIdA <= 2) ? transformPos(m_wTb, fpp.m_vB.m_quad) : transformPos(m_wTa, fpp.m_vA.m_quad);
        const Q tol = bcast<0>(m_tolerance4.m_quad);
        const Q tol2 = add(mul(tol, tol), g_vectorfConstants[HK_QUADREAL_EPS]);
        for (int i = 0; i < manifold.m_numPoints; i++)
        {
            if (manifold.m_contactPoints[i].m_featureIdA > 6)
            {
                continue;
            }
            const Q d = sub(position, m_result->m_contactPoints[i].m_contact.m_position.m_quad);
            if (lessEqualOrNaN(dot3(d, d), tol2))
            {
                return HK_FAILURE;
            }
        }
    }
    if (manifold.m_numPoints >= 8)
    {
        return HK_FAILURE;
    }
    const int index = manifold.addPoint(m_bodyA, m_bodyB, fcp);
    if (index < 0)
    {
        return HK_SUCCESS;
    }
    hkpProcessCdPoint* cp = m_result->m_firstFreeContactPoint;
    contactPointFromFeaturePointCache(*cp, fcp, fpp);
    if (manifold.m_numPoints > 1)
    {
        // against the previous output point
        if (!greater(dot3(cp[-1].m_contact.m_separatingNormal.m_quad, cp->m_contact.m_separatingNormal.m_quad), _mm_setzero_ps()))
        {
            manifold.removePoint(index);
            return HK_FAILURE;
        }
    }
    const unsigned short id = m_contactMgr->addContactPointImpl(m_bodyA, m_bodyB, *m_env, *m_result, nullptr, cp->m_contact);
    manifold.m_contactPoints[index].m_contactPointId = id;
    if (id == 0xFFFF)
    {
        manifold.removePoint(index);
        return HK_SUCCESS;
    }
    m_result->m_firstFreeContactPoint = cp + 1;
    fcp.m_contactPointId = manifold.m_contactPoints[index].m_contactPointId;
    cp->m_contactPointId = manifold.m_contactPoints[index].m_contactPointId;
    cp->m_isShortestPoint = 2;
    if (fcp.m_featureIdA <= 6)
    {
        manifold.m_faceVertexFeatureCount++;
    }
    return HK_SUCCESS;
}

// 0x00DD5EC0: the closest point only (points of B faces and edge pairs moved onto B)
hkBool hkpBoxBoxCollisionDetection::calculateClosestPoint(hkContactPoint& contact) const
{
    hkpBoxBoxManifold manifold;
    hkpFeatureContactPoint fcp;
    hkpFeaturePointCache fpp;
    fpp.m_nA.m_quad = _mm_setzero_ps();
    fpp.m_normalIsFlipped.m_mask = _mm_setzero_ps();
    initWorkVariables();
    if (checkIntersection(m_tolerance4) != HK_SUCCESS)
    {
        return false;
    }
    if (findClosestPoint(manifold, fcp, fpp) != HK_FINDCLOSESTPOINT_VALID_POINT)
    {
        return false;
    }
    Q position;
    Q normal;
    if (fpp.m_featureIndexA <= 2)
    {
        position = transformPos(m_wTb, fpp.m_vB.m_quad);
        normal = hkBoxBox_faceNormal(m_wTa, fcp.m_featureIdA, fpp.m_normalIsFlipped, fpp.m_distance);
    }
    else if (fpp.m_featureIndexA <= 6)
    {
        position = transformPos(m_wTa, fpp.m_vA.m_quad);
        normal = hkBoxBox_faceNormal(m_wTb, fcp.m_featureIdA - 4, fpp.m_normalIsFlipped, fpp.m_distance);
    }
    else
    {
        position = transformPos(m_wTa, fpp.m_vA.m_quad);
        normal = setXYZ_W(rotate(m_wTa.getRotation(), fpp.m_nA.m_quad), fpp.m_distance.m_real);
    }
    if (fcp.m_featureIdA > 2)
    {
        position = sub(position, mul(bcast<3>(normal), normal));
    }
    contact.m_position.m_quad = position;
    contact.m_separatingNormal.m_quad = setXYZ_W(normal, bcast<3>(normal));
    return true;
}

// 0x00DDA4D0 (inlined in calcManifold)
// Re-evaluates the manifold's feature pairs: a valid one is output again (its minimum distance kept), an invalid one
// removed (its contact point removed from the contact manager; the last point moves into its place and is checked
// next).
void hkpBoxBoxCollisionDetection::refreshManifold(hkpBoxBoxManifold& manifold, hkSimdFloat32& minContactPointDistance) const
{
    for (int i = 0; i < manifold.m_numPoints; i++)
    {
        const hkpFeatureContactPoint& cp = manifold.m_contactPoints[i];
        const int idA = cp.m_featureIdA;
        bool valid = false;
        if (idA <= 2)
        {
            hkVector4fComparison flipped;
            getNormalSignFromFeatureBitSet(cp.m_featureIdB, flipped);
            hkVector4f axisMap;
            getAxisMapFromFeatureBitSet(cp.m_featureIdB, axisMap);
            const Q vB = mul(axisMap.m_quad, m_radiusB.m_quad);
            const Q vA = transformPos(m_aTb, vB);
            if ((_mm_movemask_ps(_mm_cmplt_ps(abs(vA), m_keepRadiusA.m_quad)) & 7) == 7)
            {
                hkpProcessCdPoint* out = m_result->m_firstFreeContactPoint;
                out->m_isShortestPoint = 2;
                m_result->m_firstFreeContactPoint = out + 1;
                const Q dist = hkBoxBox_lane(sub(_mm_xor_ps(signBits(flipped.m_mask), vA), m_radiusA.m_quad), laneMask(1 << idA));
                minContactPointDistance.m_real = _mm_min_ps(minContactPointDistance.m_real, dist);
                out->m_contactPointId = cp.m_contactPointId;
                out->m_contact.m_position.m_quad = transformPos(m_wTb, vB);
                hkSimdFloat32 d;
                d.m_real = dist;
                out->m_contact.m_separatingNormal.m_quad = hkBoxBox_faceNormal(m_wTa, idA, flipped, d);
                valid = true;
            }
        }
        else if (idA <= 6)
        {
            hkVector4fComparison flipped;
            getNormalSignFromFeatureBitSet(cp.m_featureIdB, flipped);
            hkVector4f axisMap;
            getAxisMapFromFeatureBitSet(cp.m_featureIdB, axisMap);
            const Q vA = mul(axisMap.m_quad, m_radiusA.m_quad);
            const Q vB = rotateInverse(m_aTb.getRotation(), sub(vA, m_aTb.getTranslation().m_quad));
            if ((_mm_movemask_ps(_mm_cmplt_ps(abs(vB), m_keepRadiusB.m_quad)) & 7) == 7)
            {
                hkpProcessCdPoint* out = m_result->m_firstFreeContactPoint;
                out->m_isShortestPoint = 2;
                m_result->m_firstFreeContactPoint = out + 1;
                const Q notFlipped = _mm_castsi128_ps(_mm_cmpeq_epi32(_mm_setzero_si128(), _mm_castps_si128(flipped.m_mask)));
                const Q dist = hkBoxBox_lane(sub(_mm_xor_ps(signBits(notFlipped), vB), m_radiusB.m_quad), laneMask(1 << (idA - 4)));
                minContactPointDistance.m_real = _mm_min_ps(minContactPointDistance.m_real, dist);
                out->m_contactPointId = cp.m_contactPointId;
                out->m_contact.m_position.m_quad = transformPos(m_wTa, vA);
                hkSimdFloat32 d;
                d.m_real = dist;
                out->m_contact.m_separatingNormal.m_quad = hkBoxBox_faceNormal(m_wTb, idA - 4, flipped, d);
                valid = true;
            }
        }
        else
        {
            hkpFeaturePointCache fpp;
            fpp.m_featureIndexA = idA;
            fpp.m_featureIndexB = cp.m_featureIdB;
            if (isValidEdgeEdge(fpp))
            {
                minContactPointDistance.m_real = _mm_min_ps(minContactPointDistance.m_real, fpp.m_distance.m_real);
                hkpProcessCdPoint* out = m_result->m_firstFreeContactPoint;
                out->m_isShortestPoint = 2;
                m_result->m_firstFreeContactPoint = out + 1;
                out->m_contactPointId = cp.m_contactPointId;
                out->m_contact.m_position.m_quad = transformPos(m_wTa, fpp.m_vA.m_quad);
                out->m_contact.m_separatingNormal.m_quad = setXYZ_W(rotate(m_wTa.getRotation(), fpp.m_nA.m_quad), fpp.m_distance.m_real);
                valid = true;
            }
        }
        if (!valid)
        {
            removePoint(manifold, i);
            i--;
        }
    }
}

// 0x00DD7D30: hkVector4UtilImpl<float>::packQuaternionIntoInt32 (the 2014.1 .inl): each lane in [-1.1, 1.1] as a byte
static unsigned int hkBoxBox_packNormal(Q v)
{
    const Q x = add(mul(splat(128.0f / 1.1f), v), g_vectorfConstants[HK_QUADREAL_INV_2]); // 0x0135B6B0: 0x42E8BA2E
    __m128i i = _mm_cvttps_epi32(x);
    // setConvertF32toS32: lanes at or above 2^31 (0x01350E10: 0x4F000000) saturate to 0x7FFFFFFF
    i = _mm_xor_si128(i, _mm_castps_si128(_mm_cmple_ps(splat(2147483648.0f), x)));
    i = _mm_and_si128(_mm_add_epi32(i, _mm_set1_epi32(0x80)), _mm_set1_epi32(0xFF)); // 0x0135B680, 0x0135B670
    i = _mm_packs_epi32(i, i);
    i = _mm_packus_epi16(i, i);
    return (unsigned int)_mm_cvtsi128_si32(i);
}

// hkVector4UtilImpl<float>::unPackInt32IntoQuaternion (inlined)
static inline Q hkBoxBox_unpackNormal(unsigned int packed)
{
    __m128i c = _mm_cvtsi32_si128((int)packed);
    c = _mm_unpacklo_epi8(c, _mm_setzero_si128());
    c = _mm_unpacklo_epi16(c, _mm_setzero_si128());
    c = _mm_sub_epi32(c, _mm_set1_epi32(0x80)); // 0x0135B680
    return mul(_mm_cvtepi32_ps(c), splat(1.1f / 128.0f)); // 0x0135B6B4: 0x3C0CCCCD
}

// 0x0135B554: the vertex sign bits flipped to reach the other vertices of a face (those along the face axis skipped)
static const unsigned short hkBoxBox_cycleMask[7] = {0x50, 0x30, 0x60, 0x10, 0x20, 0x40, 0x00};

// 0x00DD9410 (inlined in calcManifold)
// 1 when the manifold normal has to be set again: fewer than 2 points (the normal cleared), a normal not initialized
// yet (initialized now: B's normal packed from A's), or B's packed normal (in A) no longer within
// m_manifoldConsistencyCheckAngularCosTolerance of A's.
unsigned int hkpBoxBoxCollisionDetection::queryManifoldNormalConsistency(hkpBoxBoxManifold& manifold) const
{
    if (manifold.m_numPoints < 2)
    {
        manifold.clearNormalInitialized();
        return 1;
    }
    if (!manifold.isNormalInitialized())
    {
        const Q normalB = rotateInverse(m_aTb.getRotation(), manifold.m_manifoldNormalA.m_quad);
        manifold.setNormalInitialized();
        manifold.m_manifoldNormalB = hkBoxBox_packNormal(normalB);
        return 1;
    }
    const Q normalB = rotate(m_aTb.getRotation(), hkBoxBox_unpackNormal(manifold.m_manifoldNormalB));
    return lessOrNaN(dot3(normalB, manifold.m_manifoldNormalA.m_quad), splat(m_manifoldConsistencyCheckAngularCosTolerance)) ? 1 : 0;
}

// 0x00DD95C0 (inlined in calcManifold)
// The points whose output normals are not within m_contactNormalAngularCosTolerance of the manifold normal (A's, in
// world space) are removed, the last output point moved into the removed one's place; then B's normal is packed again
// (or the normal cleared below 2 points).
void hkpBoxBoxCollisionDetection::checkManifoldNormalConsistency(hkpBoxBoxManifold& manifold) const
{
    const Q normal = rotate(m_wTa.getRotation(), manifold.m_manifoldNormalA.m_quad);
    const Q tolerance = splat(m_contactNormalAngularCosTolerance);
    const int first = int(m_result->m_firstFreeContactPoint.val() - &m_result->m_contactPoints[0]) - manifold.m_numPoints;
    for (int i = 0; i < manifold.m_numPoints;)
    {
        hkpProcessCdPoint& cp = m_result->m_contactPoints[first + i];
        if (greaterEqual(dot3(cp.m_contact.m_separatingNormal.m_quad, normal), tolerance))
        {
            i++;
            continue;
        }
        memcpy(&cp, m_result->m_firstFreeContactPoint.val() - 1, sizeof(hkpProcessCdPoint));
        removePoint(manifold, i);
        m_result->uncommitContactPoints();
    }
    if (manifold.m_numPoints < 2)
    {
        manifold.clearNormalInitialized();
    }
    else
    {
        const Q normalB = rotateInverse(m_aTb.getRotation(), manifold.m_manifoldNormalA.m_quad);
        manifold.m_manifoldNormalB = hkBoxBox_packNormal(normalB);
        manifold.setNormalInitialized();
    }
}

// 0x00DD4370
// Complete when it has 4 face-vertex points, or (from 3 points) when no edge of either box is touched by one point
// only: the terminal counters of the edges at the points' vertices (but those along the face axes of planeMaskA/B)
// and of the edge points' edges.
void hkpBoxBoxCollisionDetection::checkCompleteness(hkpBoxBoxManifold& manifold, int planeMaskA, int planeMaskB) const
{
    if (manifold.m_numPoints < 3)
    {
        return;
    }
    if (manifold.m_faceVertexFeatureCount >= 4)
    {
        manifold.m_isComplete = true;
        return;
    }
    const int skipA = hkVector4Comparison_maskToFirstIndex[planeMaskA >> 4];
    const int skipB = hkVector4Comparison_maskToFirstIndex[planeMaskB >> 4];
    unsigned int terminalsA = 0;
    unsigned int terminalsB = 0;
    const int numPoints = manifold.m_numPoints;
    for (int i = 0; i < numPoints; i++)
    {
        const hkpFeatureContactPoint& cp = manifold.m_contactPoints[i];
        if (cp.m_featureIdA <= 2)
        {
            setTerminalsFromVertex(cp.m_featureIdB, terminalsB, skipB);
        }
        else if (cp.m_featureIdA <= 6)
        {
            setTerminalsFromVertex(cp.m_featureIdB, terminalsA, skipA);
        }
        else
        {
            setTerminalsFromEdge(cp.m_featureIdA, terminalsA);
            setTerminalsFromEdge(cp.m_featureIdB, terminalsB);
        }
    }
    // the low bit of every 2-bit counter: an edge seen once
    manifold.m_isComplete = !((terminalsA & 0x555555) || (terminalsB & 0x555555));
}

// 0x00DD86E0 (inlined in calcManifold)
// The other vertices of B's face (the template's vertex with the bits of hkBoxBox_cycleMask flipped, those along
// planeMask skipped) against A's face: added when valid and not separated below closestPointDist - EPS.
void hkpBoxBoxCollisionDetection::tryToAddPointFaceA(hkpBoxBoxManifold& manifold, const hkpFeatureContactPoint fcpTemplate, unsigned short planeMask, const hkSimdFloat32& closestPointDist) const
{
    for (int c = 0; c < 7; c++)
    {
        const unsigned short cycleMask = hkBoxBox_cycleMask[c];
        if (cycleMask & planeMask)
        {
            continue;
        }
        hkpFeatureContactPoint fcp = fcpTemplate;
        fcp.m_featureIdB = (unsigned char)(((fcpTemplate.m_featureIdB ^ cycleMask) & 0xFF) | (fcpTemplate.m_featureIdB & 0xF));
        if (manifold.findInManifold(fcp))
        {
            continue;
        }
        hkpFeaturePointCache fpp;
        faceAVertexBValidationDataFromFeatureId(fpp, fcp);
        if (!isValidFaceAVertexB(fpp))
        {
            continue;
        }
        calcDistanceFaceAVertexB(fpp);
        if (lessOrNaN(fpp.m_distance.m_real, sub(closestPointDist.m_real, g_vectorfConstants[HK_QUADREAL_EPS])))
        {
            continue;
        }
        addPoint(manifold, fpp, fcp);
    }
}

// 0x00DD8B40 (inlined in calcManifold): as tryToAddPointFaceA for A's vertices against B's face
void hkpBoxBoxCollisionDetection::tryToAddPointFaceB(hkpBoxBoxManifold& manifold, const hkpFeatureContactPoint fcpTemplate, unsigned short planeMask, const hkSimdFloat32& closestPointDist) const
{
    for (int c = 0; c < 7; c++)
    {
        const unsigned short cycleMask = hkBoxBox_cycleMask[c];
        if (cycleMask & planeMask)
        {
            continue;
        }
        hkpFeatureContactPoint fcp = fcpTemplate;
        fcp.m_featureIdB = (unsigned char)(((fcpTemplate.m_featureIdB ^ cycleMask) & 0xFF) | (fcpTemplate.m_featureIdB & 0xF));
        if (manifold.findInManifold(fcp))
        {
            continue;
        }
        hkpFeaturePointCache fpp;
        faceBVertexAValidationDataFromFeatureId(fpp, fcp);
        if (!isValidFaceBVertexA(fpp))
        {
            continue;
        }
        calcDistanceFaceBVertexA(fpp);
        if (lessOrNaN(fpp.m_distance.m_real, sub(closestPointDist.m_real, g_vectorfConstants[HK_QUADREAL_EPS])))
        {
            continue;
        }
        addPoint(manifold, fpp, fcp);
    }
}

// 0x00DD97F0 (inlined in calcManifold)
// When the first point's normal is along a face axis of both boxes (within m_coplanerAngularCosTolerance), the other
// vertices of the two faces are tried against the smallest separation of the manifold's points (at most 1), then
// the manifold's completeness is checked.
void hkpBoxBoxCollisionDetection::findAdditionalManifoldPoints(hkpBoxBoxManifold& manifold, hkpFeatureContactPoint fcpATemplate) const
{
    const int numPoints = manifold.m_numPoints;
    if (numPoints < 1)
    {
        return;
    }
    const int first = int(m_result->m_firstFreeContactPoint.val() - &m_result->m_contactPoints[0]) - numPoints;
    const Q n = m_result->m_contactPoints[first].m_contact.m_separatingNormal.m_quad;
    const Q nB = rotateInverse(m_wTb.getRotation(), n);
    const Q nA = neg(rotateInverse(m_wTa.getRotation(), n));
    hkVector4f absA;
    absA.m_quad = abs(nA);
    int axisA;
    const int planeMaskA = getMaxPlaneMask3(absA, axisA);
    hkVector4f absB;
    absB.m_quad = abs(nB);
    int axisB;
    const int planeMaskB = getMaxPlaneMask3(absB, axisB);
    const Q cosTolerance = splat(m_coplanerAngularCosTolerance);
    const bool notCoplanarB = lessOrNaN(hkBoxBox_lane(absB.m_quad, laneMask(1 << axisB)), cosTolerance);
    const bool notCoplanarA = lessOrNaN(hkBoxBox_lane(absA.m_quad, laneMask(1 << axisA)), cosTolerance);
    if (notCoplanarB || notCoplanarA)
    {
        return;
    }

    // A's face along the normal with B's vertex most opposite, and B's face with A's vertex
    int vertexB = (_mm_movemask_ps(_mm_cmplt_ps(nB, _mm_setzero_ps())) & 7) << 4;
    if (_mm_movemask_ps(_mm_cmplt_ps(hkBoxBox_lane(m_dinA.m_quad, laneMask(1 << axisA)), _mm_setzero_ps())))
    {
        vertexB |= 8;
    }
    int vertexA = (_mm_movemask_ps(_mm_cmplt_ps(nA, _mm_setzero_ps())) & 7) << 4;
    if (_mm_movemask_ps(_mm_cmplt_ps(hkBoxBox_lane(m_dinB.m_quad, laneMask(1 << axisB)), _mm_setzero_ps())))
    {
        vertexA |= 8;
    }
    hkpFeatureContactPoint fcpA = fcpATemplate;
    fcpA.m_featureIdA = (unsigned char)axisA;
    fcpA.m_featureIdB = (unsigned char)vertexB;
    hkpFeatureContactPoint fcpB;
    fcpB.m_featureIdA = (unsigned char)(axisB + 4);
    fcpB.m_featureIdB = (unsigned char)vertexA;
    fcpB.m_contactPointId = 0;

    Q minDistance = g_vectorfConstants[HK_QUADREAL_1];
    for (int i = 0; i < numPoints; i++)
    {
        minDistance = _mm_min_ps(minDistance, bcast<3>(m_result->m_contactPoints[first + i].m_contact.m_separatingNormal.m_quad));
    }
    hkSimdFloat32 closestPointDist;
    closestPointDist.m_real = minDistance;
    tryToAddPointFaceA(manifold, fcpA, (unsigned short)planeMaskB, closestPointDist);
    tryToAddPointFaceB(manifold, fcpB, (unsigned short)planeMaskA, closestPointDist);
    if (manifold.m_faceVertexFeatureCount >= 4)
    {
        manifold.m_isComplete = true;
    }
    else
    {
        checkCompleteness(manifold, planeMaskA, planeMaskB);
    }
}

// 0x00DD6090
// The manifold refreshed (invalid points removed, the others output again); unless its normal is consistent and it
// is complete, the separating axis test (the tolerance relative to the closest refreshed point), the closest feature
// pair (the manifold normal updated, inconsistent points dropped), and that pair added with the neighbouring face
// vertices.
void hkpBoxBoxCollisionDetection::calcManifold(hkpBoxBoxManifold& manifold) const
{
    initWorkVariables();
    hkSimdFloat32 minContactPointDistance;
    minContactPointDistance.m_real = g_vectorfConstants[HK_QUADREAL_MAX];
    refreshManifold(manifold, minContactPointDistance);
    const unsigned int normalNeedsUpdate = queryManifoldNormalConsistency(manifold);
    if (!normalNeedsUpdate && manifold.m_isComplete)
    {
        return;
    }

    hkResult result;
    if (manifold.m_numPoints)
    {
        hkVector4f tolerance;
        const Q relative = sub(minContactPointDistance.m_real, mul(bcast<0>(m_tolerance4.m_quad), splat(0.05f))); // 0x012C5BF0: 0x3D4CCCCD
        tolerance.m_quad = setXYZ_W(relative, g_vectorfConstants[HK_QUADREAL_MAX]);
        result = checkIntersection(tolerance);
    }
    else
    {
        result = checkIntersection(m_tolerance4);
    }
    if (result != HK_SUCCESS)
    {
        return;
    }

    hkpFeatureContactPoint fcp;
    fcp.m_featureIdA = 0;
    fcp.m_featureIdB = 0;
    fcp.m_contactPointId = 0;
    hkpFeaturePointCache fpp;
    fpp.m_nA.m_quad = _mm_setzero_ps();
    fpp.m_normalIsFlipped.m_mask = _mm_setzero_ps();
    fpp.m_distance.m_real = _mm_setzero_ps();
    const int status = findClosestPoint(manifold, fcp, fpp);
    if (normalNeedsUpdate && status != HK_FINDCLOSESTPOINT_NO_VALID_POINT)
    {
        const bool wasInitialized = manifold.isNormalInitialized() != 0;
        calcManifoldNormal(manifold.m_manifoldNormalA, fcp, fpp, status == HK_FINDCLOSESTPOINT_VALID_POINT);
        if (wasInitialized)
        {
            checkManifoldNormalConsistency(manifold);
        }
    }
    if (status != HK_FINDCLOSESTPOINT_VALID_POINT)
    {
        return;
    }
    if (addPoint(manifold, fpp, fcp) != HK_SUCCESS)
    {
        return;
    }
    findAdditionalManifoldPoints(manifold, fcp);
}
