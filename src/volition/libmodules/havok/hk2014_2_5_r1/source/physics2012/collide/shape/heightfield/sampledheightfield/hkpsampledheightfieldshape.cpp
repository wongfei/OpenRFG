// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/sampledheightfield/hkpsampledheightfieldshape.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): a height field sampled on an xRes x zRes grid (heights from the virtual
// getHeightAtImpl, scaled by m_intToFloatScale), from rfg.exe. Ray and sphere casts go through the static cast
// function pointers (set by hkpSampledHeightField_registerAllRayCastFunctions); the default casts walk the grid with
// the 3-DDA (x, z and the cell diagonal), or the coarse min/max tree when one is built (m_coarseness > 0).
//
// The shipped build records "rcHeightField"/"rcDda" monitor timers (TLS hkMonitorStream) around the casts; they are
// not reproduced. Some sums run on the x87 stack with 53-bit precision; they are done in double here.
//
// Not reproduced yet: the coarse min/max tree (buildCoarseMinMaxTree, setMinMaxTreeCoarseness, calcMinMax and the
// coarse tree casts). The streamed height fields have no coarse levels (m_coarseness 0).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/sampledheightfield/hkpsampledheightfieldshape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4util.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabbutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/sphere/hksphere.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/sampledheightfield/hkpsampledheightfieldbasecinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkprayhitcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastoutput.h"

#include <emmintrin.h>
#include <math.h>

// 0x03A2880C, 0x03A28810: set by hkpSampledHeightField_registerAllRayCastFunctions (hkpsampledheightfieldshape_default.cpp)
hkpSampledHeightFieldShape::RayCastInternalFunc hkpSampledHeightFieldShape::s_rayCastFunc;
hkpSampledHeightFieldShape::SphereCastInternalFunc hkpSampledHeightFieldShape::s_sphereCastFunc;

namespace
{
const float REAL_MAX = 3.40282e38f; // HK_REAL_MAX (0x7F7FFFEE)

// The nearest hit into a ray cast output (castRay).
class NearestHitCollector : public hkpRayHitCollector
{
public:
    NearestHitCollector(hkpShapeRayCastOutput& output)
        : m_output(&output)
    {
        m_earlyOutHitFraction = output.m_hitFraction;
        m_hasHit = false;
    }

    // 0x00D9BCF0
    virtual void addRayHit(const hkpCdBody& cdBody, const hkpShapeRayCastCollectorOutput& hitInfo)
    {
        if (hitInfo.m_hitFraction < m_earlyOutHitFraction)
        {
            m_earlyOutHitFraction = hitInfo.m_hitFraction;
            m_output->m_normal = hitInfo.m_normal;
            m_output->m_hitFraction = hitInfo.m_hitFraction;
            m_output->m_extraInfo = hitInfo.m_extraInfo;
            m_output->m_shapeKeys[m_output->m_shapeKeyIndex] = 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
            m_hasHit = true;
        }
    }

    hkBool m_hasHit; // 0x08
    hkpShapeRayCastOutput* m_output; // 0x0C
};

// The hits with their normals rotated to world space, forwarded to another collector (castRayWithCollector).
class RotateNormalHitCollector : public hkpRayHitCollector
{
public:
    RotateNormalHitCollector(hkpRayHitCollector& collector)
        : m_collector(&collector)
    {
        m_earlyOutHitFraction = collector.m_earlyOutHitFraction;
    }

    // 0x00D9BF10
    virtual void addRayHit(const hkpCdBody& cdBody, const hkpShapeRayCastCollectorOutput& hitInfo)
    {
        hkpShapeRayCastCollectorOutput hit = hitInfo;
        hit.m_normal._setRotatedDir(cdBody.getTransform().getRotation(), hitInfo.m_normal);
        m_collector->addRayHit(cdBody, hit);
        m_earlyOutHitFraction = m_collector->m_earlyOutHitFraction;
    }

    hkpRayHitCollector* m_collector; // 0x08
};

// floor() of the SSE build: round to an integer by adding and subtracting 2^23 (twice), one less when that rounded
// up; values beyond 2^23 are integers already.
int floorToInt(float v)
{
    const __m128 magic = _mm_set1_ps(8388608.0f); // 2^23
    const __m128 x = _mm_set_ss(v);
    const __m128 r = _mm_sub_ps(_mm_add_ps(_mm_add_ps(_mm_sub_ps(x, magic), magic), magic), magic);
    const __m128 absX = _mm_castsi128_ps(_mm_srli_epi32(_mm_slli_epi32(_mm_castps_si128(x), 1), 1));
    const __m128 down = _mm_add_ps(_mm_cvtepi32_ps(_mm_castps_si128(_mm_cmplt_ps(x, r))), r); // -1.0f where x < r
    const __m128 big = _mm_cmplt_ps(magic, absX);
    return _mm_cvttss_si32(_mm_or_ps(_mm_andnot_ps(big, down), _mm_and_ps(big, x)));
}

// normalize<3> with the 23-bit reciprocal square root (one Newton step), zero for a zero vector.
__m128 normalize3(__m128 n)
{
    const __m128 sq = _mm_mul_ps(n, n);
    const __m128 len2 = _mm_add_ps(_mm_add_ps(_mm_shuffle_ps(sq, sq, 0x55), _mm_shuffle_ps(sq, sq, 0x00)), _mm_shuffle_ps(sq, sq, 0xAA));
    const __m128 isZero = _mm_cmple_ps(len2, _mm_setzero_ps());
    const __m128 r = _mm_rsqrt_ps(len2);
    const __m128 rr = _mm_mul_ps(_mm_mul_ps(r, len2), r);
    const __m128 inv = _mm_mul_ps(_mm_sub_ps(_mm_set1_ps(3.0f), rr), _mm_mul_ps(r, _mm_set1_ps(0.5f)));
    return _mm_mul_ps(_mm_andnot_ps(isZero, inv), n);
}

// a - b on the x87 stack (53-bit), stored as a float.
float x87Sub(float a, float b)
{
    return (float)((double)a - (double)b);
}

// t * (to - from) + from, lane by lane.
void lerp4(const float* from, const float* to, float t, float* out)
{
    for (int i = 0; i < 4; i++)
    {
        out[i] = t * (to[i] - from[i]) + from[i];
    }
}
}

// 0x00D9B6B0
// The extents are the grid size times the scale (the height range max - min; a negative one when max < min, which
// makes the first getAabb scan the heights). The float to grid scale is the reciprocal of the scale (with w 1; a zero
// scale gives +-HK_REAL_MAX), the grid offset the 16-bit floor correction times the scale (no y).
hkpSampledHeightFieldShape::hkpSampledHeightFieldShape(const hkpSampledHeightFieldBaseCinfo& ci, HeightFieldType hfType)
    : hkpHeightFieldShape(hkcdShapeType::SAMPLED_HEIGHT_FIELD)
{
    m_heightfieldType.m_storage = (unsigned char)hfType;
    m_xRes = ci.m_xRes;
    m_zRes = ci.m_zRes;
    m_useProjectionBasedHeight = ci.m_useProjectionBasedHeight;
    m_extents.set((float)ci.m_xRes - 1.0f, ci.m_maxHeight - ci.m_minHeight, (float)ci.m_zRes - 1.0f, 0.0f);
    m_extents.m_quad = _mm_mul_ps(ci.m_scale.m_quad, m_extents.m_quad);
    if (ci.m_minHeight > ci.m_maxHeight)
    {
        m_raycastMinY = -REAL_MAX;
        m_raycastMaxY = REAL_MAX;
        m_heightCenter = -1.0f;
        m_extents(1) = -1.0f;
    }
    else
    {
        m_raycastMinY = ci.m_minHeight;
        m_raycastMaxY = ci.m_maxHeight;
        m_heightCenter = (ci.m_maxHeight + ci.m_minHeight) * 0.5f * ci.m_scale(1);
    }
    m_intToFloatScale = ci.m_scale;

    const __m128 scale = _mm_shuffle_ps(ci.m_scale.m_quad, _mm_unpackhi_ps(ci.m_scale.m_quad, _mm_set1_ps(1.0f)), 0xC4); // (x, y, z, 1)
    const __m128 reciprocal = _mm_div_ps(_mm_set1_ps(1.0f), scale);
    const __m128 isZero = _mm_cmpeq_ps(_mm_setzero_ps(), scale);
    const __m128 signedMax = _mm_xor_ps(_mm_castsi128_ps(_mm_slli_epi32(_mm_srli_epi32(_mm_castps_si128(scale), 31), 31)), _mm_set1_ps(REAL_MAX));
    m_floatToIntScale.m_quad = _mm_or_ps(_mm_and_ps(signedMax, isZero), _mm_andnot_ps(isZero, reciprocal));

    float correction;
    hkVector4UtilImpl<float>::getFloatToInt16FloorCorrection(correction);
    m_floatToIntOffsetFloorCorrected.m_quad = _mm_mul_ps(_mm_set1_ps(correction), m_intToFloatScale.m_quad);
    m_floatToIntOffsetFloorCorrected(1) = 0.0f;
    m_coarseness = 0;
}

// 0x00D9B830
hkpSampledHeightFieldShape::hkpSampledHeightFieldShape(hkFinishLoadedObjectFlag flag)
    : hkpHeightFieldShape(flag)
    , m_coarseTreeData(flag)
{
    if (flag.m_finishing)
    {
        afterReflectNew();
    }
}

// 0x00DA8260
hkpSampledHeightFieldShape::hkpSampledHeightFieldShape()
    : hkpHeightFieldShape(hkcdShapeType::SAMPLED_HEIGHT_FIELD)
{
}

// 0x00D9B860: the levels' arrays go before the level array (hkArray does not run element destructors)
hkpSampledHeightFieldShape::~hkpSampledHeightFieldShape()
{
    for (int i = m_coarseTreeData.getSize() - 1; i >= 0; i--)
    {
        m_coarseTreeData[i].~CoarseMinMaxLevel();
    }
    m_coarseTreeData.clearAndDeallocate();
}

// 0x00D9B4D0
void hkpSampledHeightFieldShape::afterReflectNew()
{
    m_type.m_storage = hkcdShapeType::SAMPLED_HEIGHT_FIELD;
}

// 0x00D9B100: empty in rfg.exe
void hkpSampledHeightFieldShape::getCinfo(hkpSampledHeightFieldBaseCinfo& cinfo) const
{
}

// 0x00D9AFE0: warning 0x4B50036A ("You must implement collideSpheres!") in Havok; the warning is not reproduced
void hkpSampledHeightFieldShape::collideSpheres(const hkpHeightFieldShape::CollideSpheresInput& input, hkVector4f* outputArray) const
{
}

// 0x00D9B040: warning 0x4B50036B ("You must implement getHeightAtImpl!"); the original returns whatever is on the x87
// stack
float hkpSampledHeightFieldShape::getHeightAtImpl(int x, int z) const
{
    return 0.0f;
}

// 0x00D9B0A0: warning 0x4B50036C ("You must implement getTriangleFlipImpl!"); the original returns garbage
hkBool hkpSampledHeightFieldShape::getTriangleFlipImpl() const
{
    return false;
}

// 0x00D9B4E0
// A height field built with max < min (negative y extent) scans its heights on the first call and keeps the range.
void hkpSampledHeightFieldShape::getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const
{
    float minHeight = getHeightAt(0, 0);
    float maxHeight = minHeight;
    if (0.0f > m_extents(1))
    {
        for (int x = 0; x < m_xRes; x++)
        {
            for (int z = 0; z < m_zRes; z++)
            {
                const float h = getHeightAt(x, z);
                minHeight = (minHeight < h) ? minHeight : h; // minss
                maxHeight = (maxHeight > h) ? maxHeight : h; // maxss
            }
        }
        hkpSampledHeightFieldShape* self = const_cast<hkpSampledHeightFieldShape*>(this);
        self->m_raycastMinY = minHeight;
        self->m_raycastMaxY = maxHeight;
        const float a = m_intToFloatScale(1) * minHeight;
        const float b = m_intToFloatScale(1) * maxHeight;
        const float lo = (a < b) ? a : b;
        const float hi = (a > b) ? a : b;
        self->m_heightCenter = (float)(((double)hi + (double)lo) * 0.5); // x87
        self->m_extents(1) = x87Sub(hi, lo);
    }
    hkVector4 halfExtents;
    halfExtents.m_quad = _mm_mul_ps(m_extents.m_quad, _mm_set1_ps(0.5f));
    hkVector4 center = halfExtents;
    center(1) = m_heightCenter;
    hkAabbUtil::calcAabb(localToWorld, halfExtents, center, hkSimdReal::fromFloat(tolerance), out);
}

// 0x00D9B110: the nearest hit (no cdBody: the default casts do not use it with this collector)
hkBool hkpSampledHeightFieldShape::castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const
{
    NearestHitCollector collector(results);
    (this->*s_rayCastFunc)(input, *static_cast<const hkpCdBody*>(nullptr), collector);
    return collector.m_hasHit;
}

// 0x00D9B160: the normals of the hits are rotated by the body's transform
void hkpSampledHeightFieldShape::castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const
{
    RotateNormalHitCollector rotateCollector(collector);
    (this->*s_rayCastFunc)(input, cdBody, rotateCollector);
}

// 0x00D9B440
// The sphere is cast as its lowest point: the ray moves down by the radius (up when the y scale is negative).
void hkpSampledHeightFieldShape::castSphere(const hkpHeightFieldShape::hkpSphereCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const
{
    const __m128 down = _mm_set_ps(0.0f, 0.0f, -input.m_radius, 0.0f);
    const __m128 sign = _mm_castsi128_ps(_mm_slli_epi32(_mm_srli_epi32(_mm_castps_si128(_mm_shuffle_ps(m_intToFloatScale.m_quad, m_intToFloatScale.m_quad, 0x55)), 31), 31));
    const __m128 offset = _mm_xor_ps(sign, down);
    hkpHeightFieldShape::hkpSphereCastInput sphereInput = input;
    sphereInput.m_from.m_quad = _mm_add_ps(input.m_from.m_quad, offset);
    sphereInput.m_to.m_quad = _mm_add_ps(input.m_to.m_quad, offset);
    (this->*s_sphereCastFunc)(sphereInput, cdBody, collector);
}

// 0x00D9B3F0: the minimum and maximum of four cells of a coarse level (level counts from m_coarseness)
void hkpSampledHeightFieldShape::getCoarseMinMax(int level, int x, int z, hkVector4f& minOut, hkVector4f& maxOut) const
{
    const CoarseMinMaxLevel& l = m_coarseTreeData[level - m_coarseness];
    if (x < l.m_xRes && z < l.m_zRes)
    {
        const int index = l.m_zRes * x + z;
        minOut = l.m_minMaxData[index * 2];
        maxOut = l.m_minMaxData[index * 2 + 1];
    }
}

// 0x00D9BAA0 (hkpsampledheightfieldshape.inl)
// The height and (unnormalized) normal of the triangle under (subX, subZ) in cell (x, z): the normal is
// (-dx, 1, -dz) with w kept; triangle 1 is the one away from (x, z) (or from (x + 1, z) when not flipped).
void hkpSampledHeightFieldShape::_getHeightAndNormalAt(int x, int z, float subX, float subZ, hkVector4f& normalOut, float& heightOut, int& triangleIndexOut) const
{
    normalOut(0) = 1.0f;
    normalOut(1) = 1.0f;
    normalOut(2) = 1.0f;
    if (getTriangleFlip())
    {
        const float h00 = getHeightAt(x, z);
        const float h11 = getHeightAt(x + 1, z + 1);
        if (subX > subZ)
        {
            const float h10 = getHeightAt(x + 1, z);
            const float dx = h10 - h00;
            const float dz = h11 - h10;
            heightOut = h00 + subZ * dz + subX * dx;
            normalOut(0) = -dx;
            normalOut(2) = -dz;
            triangleIndexOut = 1;
        }
        else
        {
            const float h01 = getHeightAt(x, z + 1);
            const float dx = h11 - h01;
            const float dz = h01 - h00;
            heightOut = h00 + subZ * dz + subX * dx;
            normalOut(0) = -dx;
            normalOut(2) = -dz;
            triangleIndexOut = 0;
        }
    }
    else
    {
        const float h10 = getHeightAt(x + 1, z);
        const float h01 = getHeightAt(x, z + 1);
        if (subX + subZ > 1.0f)
        {
            const float h11 = getHeightAt(x + 1, z + 1);
            const float dx = h11 - h01;
            const float dz = h11 - h10;
            heightOut = h10 + subZ * dz + (subX - 1.0f) * dx;
            normalOut(0) = -dx;
            normalOut(2) = -dz;
            triangleIndexOut = 1;
        }
        else
        {
            const float h00 = getHeightAt(x, z);
            const float dx = h10 - h00;
            const float dz = h01 - h00;
            heightOut = h00 + subZ * dz + subX * dx;
            normalOut(0) = -dx;
            normalOut(2) = -dz;
            triangleIndexOut = 0;
        }
    }
}

// 0x00D9B1A0
void hkpSampledHeightFieldShape::getHeightAndNormalAt(int xPos, int zPos, float subX, float subZ, hkVector4f& normalOut, float& heightOut, int& triangleIndexOut) const
{
    _getHeightAndNormalAt(xPos, zPos, subX, subZ, normalOut, heightOut, triangleIndexOut);
}

// 0x00DAE2B0
void HK_CALL hkpSampledHeightFieldShape::castRayDda(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const
{
    castRayDdaInternal(input, cdBody, false, 0.0f, collector);
}

// 0x00DAE2E0: a start under the surface is a hit (up to the extra penetration)
void HK_CALL hkpSampledHeightFieldShape::castSphereDda(const hkpHeightFieldShape::hkpSphereCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const
{
    const hkpShapeRayCastInput rayInput = input;
    castRayDdaInternal(rayInput, cdBody, true, input.m_maxExtraPenetration, collector);
}

// 0x00DACD30
// The ray in grid space walks the cells along x, z and the cell diagonal (x + z, or x - z when the triangles are
// flipped), testing the ray height against the height field where it crosses each grid line. Every hit goes to the
// collector with the triangle key ((z << 16) | (x << 1) | triangle) and the normal in the shape's space; the walk
// stops at the collector's early-out fraction. Nearly vertical rays (two tiny directions) test the one cell under the
// start instead. The x clip is followed by the z clip, which derives x from the cell before the x clip (rfg.exe).
void hkpSampledHeightFieldShape::castRayDdaInternal(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkBool reportPenetratingStartPosition, float maxExtraPenetration, hkpRayHitCollector& collector) const
{
    bool reportPenetrating = reportPenetratingStartPosition ? true : false;

    // start cell; cell[1] (the diagonal) is set before use
    hkIntUnion64 start;
    hkVector4UtilImpl<float>::convertToUint16WithClip(input.m_from, m_floatToIntOffsetFloorCorrected, m_floatToIntScale, hkVector4f::getZero(), hkVector4f::getConstant<HK_QUADREAL_HIGH>(), start);
    int cell[3] = { start.i16[0], 0, start.i16[2] };

    // from and to in grid space; y becomes the diagonal coordinate, w the height
    alignas(16) float fromS[4];
    alignas(16) float toS[4];
    _mm_store_ps(fromS, _mm_mul_ps(m_floatToIntScale.m_quad, input.m_from.m_quad));
    _mm_store_ps(toS, _mm_mul_ps(input.m_to.m_quad, m_floatToIntScale.m_quad));
    const bool flip = getTriangleFlip() ? true : false;
    fromS[3] = fromS[1];
    toS[3] = toS[1];
    if (!flip)
    {
        fromS[1] = fromS[2] + fromS[0];
        toS[1] = toS[2] + toS[0];
    }
    else
    {
        fromS[1] = fromS[0] - fromS[2];
        toS[1] = toS[0] - toS[2];
    }

    float absDir[3];
    float inv[3] = { 0.0f, 0.0f, 0.0f };
    float tDelta[3];
    float tNext[3];
    int step[3];
    for (int i = 0; i < 3; i++)
    {
        const float d = toS[i] - fromS[i];
        absDir[i] = fabsf(d);
        if (3.0517578e-05f > absDir[i]) // 2^-15
        {
            tDelta[i] = 0.0f;
            step[i] = -1;
            tNext[i] = REAL_MAX;
        }
        else
        {
            inv[i] = 1.0f / d;
            if (0.0f > d)
            {
                step[i] = -1;
                tDelta[i] = -inv[i];
            }
            else
            {
                step[i] = 1;
                tDelta[i] = inv[i];
            }
            if (fromS[i] * d > 0.0f)
            {
                cell[i] += step[i];
            }
            tNext[i] = ((float)cell[i] - fromS[i]) * inv[i];
        }
    }

    if (tDelta[2] + tDelta[0] == 0.0f || tDelta[1] + tDelta[0] == 0.0f || tDelta[1] + tDelta[2] == 0.0f)
    {
        // nearly vertical: the cell under the start
        const int cx = cell[0];
        const int cz = cell[2];
        if ((unsigned int)cx >= (unsigned int)(m_xRes - 1) || (unsigned int)cz >= (unsigned int)(m_zRes - 1))
        {
            return;
        }
        const float subX = fromS[0] - (float)cx;
        const float subZ = fromS[2] - (float)cz;
        hkpShapeRayCastOutput output;
        output.m_normal(3) = 0.0f; // uninitialised in rfg.exe
        output.m_hitFraction = 1.0f;
        output.m_extraInfo = -1;
        output.m_shapeKeys[0] = 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
        output.m_shapeKeyIndex = 0;
        float height;
        int triangle;
        _getHeightAndNormalAt(cx, cz, subX, subZ, output.m_normal, height, triangle);
        const float fromAbove = fromS[3] - height;
        const float toAbove = toS[3] - height;
        if (toAbove > fromAbove)
        {
            return;
        }
        float hitT;
        if (reportPenetrating && 0.0f > fromAbove)
        {
            if (toAbove > fromAbove - maxExtraPenetration)
            {
                return;
            }
            hitT = 0.0f;
        }
        else
        {
            if (0.0f > fromAbove || toAbove >= 0.0f)
            {
                return;
            }
            hitT = fromAbove / (fromAbove - toAbove);
        }
        if (collector.m_earlyOutHitFraction > hitT)
        {
            output.m_normal.m_quad = normalize3(_mm_mul_ps(output.m_normal.m_quad, m_floatToIntScale.m_quad));
            output.m_extraInfo = triangle + 2 * ((cz << 15) + cx);
            output.m_hitFraction = hitT;
            collector.addRayHit(cdBody, output);
        }
        return;
    }

    const int xRes = m_xRes;
    const float xMax = (float)(xRes - 1);
    const int stepX = step[0];
    const int stepZ = step[2];
    const int startCellX = cell[0];
    alignas(16) float pos[4] = { fromS[0], fromS[1], fromS[2], fromS[3] };

    // clip x into [1, xRes - 2]
    {
        bool clip = false;
        float t = 0.0f;
        int newCell = 0;
        if (stepX > 0)
        {
            if (0.0f > toS[0] || fromS[0] > xMax)
            {
                return;
            }
            if (cell[0] <= 0)
            {
                t = -(tDelta[0] * fromS[0]);
                newCell = 1;
                clip = true;
            }
        }
        else
        {
            if (toS[0] > xMax || 0.0f > fromS[0])
            {
                return;
            }
            if (cell[0] > xRes - 2)
            {
                t = (fromS[0] - xMax) * tDelta[0];
                newCell = xRes - 2;
                clip = true;
            }
        }
        if (clip)
        {
            tNext[0] = tDelta[0] + t;
            lerp4(fromS, toS, t, pos);
            int cz = cell[2] + floorToInt(t * absDir[2]) * stepZ - 2;
            const int floorZ = floorToInt(pos[2]);
            if (floorZ >= cz)
            {
                cz = floorZ + 1;
            }
            cell[0] = newCell;
            cz += stepZ >> 1;
            if (!(tDelta[2] == 0.0f))
            {
                tNext[2] = ((float)cz - fromS[2]) * inv[2];
            }
            cell[2] = cz;
        }
    }

    // clip z into [1, zRes - 2]
    {
        const int zRes = m_zRes;
        const float zMax = (float)(zRes - 1);
        bool clip = false;
        float t = 0.0f;
        int newCell = 0;
        if (stepZ > 0)
        {
            if (0.0f > toS[2] || fromS[2] > zMax)
            {
                return;
            }
            if (cell[2] <= 0)
            {
                t = -(tDelta[2] * fromS[2]);
                newCell = 1;
                clip = true;
            }
        }
        else
        {
            if (toS[2] > zMax || 0.0f > fromS[2])
            {
                return;
            }
            if (cell[2] > zRes - 2)
            {
                t = (fromS[2] - zMax) * tDelta[2];
                newCell = zRes - 2;
                clip = true;
            }
        }
        if (clip)
        {
            tNext[2] = tDelta[2] + t;
            lerp4(fromS, toS, t, pos);
            // original bug kept: x restarts from the cell before the x clip
            int cx = startCellX + floorToInt(absDir[0] * t) * stepX - 2;
            const int floorX = floorToInt(pos[0]);
            if (floorX >= cx)
            {
                cx = floorX + 1;
            }
            cell[2] = newCell;
            cx += stepX >> 1;
            cell[0] = cx;
            if (!(tDelta[0] == 0.0f))
            {
                tNext[0] = ((float)cx - fromS[0]) * inv[0];
            }
        }
    }

    if ((unsigned int)cell[0] >= (unsigned int)xRes || (unsigned int)(cell[0] - stepX) >= (unsigned int)xRes)
    {
        return;
    }

    // the diagonal: walked as its own axis when the ray crosses the diagonals (useDiagonal), else only stepped
    // with x or z
    const int stepD = step[1];
    bool useDiagonal = (((stepZ ^ stepX) >= 0) != flip);
    if (tDelta[1] == 0.0f)
    {
        useDiagonal = false;
    }
    int lastAxis = 2;
    const int cellD = flip ? cell[0] - cell[2] : cell[2] + cell[0];
    cell[1] = cellD;
    const float fracD = pos[1] - (float)cellD;
    if (useDiagonal)
    {
        if (fabsf(fracD) > 1.0f) // x87
        {
            cell[1] -= stepD;
        }
        else
        {
            lastAxis = 0;
        }
    }
    else if ((float)stepD * fracD > 0.0f)
    {
        lastAxis = 0;
        cell[1] = cellD + stepD;
    }
    tNext[1] = !(tDelta[1] == 0.0f) ? (((float)cell[1] - fromS[1]) * tDelta[1]) * (float)stepD : REAL_MAX;

    // start one line back on the axis crossed last
    const float tPrevX = (tDelta[0] == 0.0f) ? -REAL_MAX : tNext[0] - tDelta[0];
    const float tPrevD = (tDelta[1] == 0.0f) ? -REAL_MAX : tNext[1] - tDelta[1];
    const float tPrevZ = (tDelta[2] == 0.0f) ? -REAL_MAX : tNext[2] - tDelta[2];
    int axis;
    if (useDiagonal)
    {
        axis = (lastAxis == 0) ? 1 : ((tPrevX > tPrevZ) ? 0 : 2);
    }
    else if (absDir[2] > absDir[0])
    {
        axis = (lastAxis != 0) ? 2 : ((tPrevX > tPrevD) ? 0 : 1);
    }
    else
    {
        axis = (lastAxis != 0) ? 0 : ((tPrevD > tPrevZ) ? 1 : 2);
    }
    tNext[axis] = tNext[axis] - tDelta[axis];
    cell[axis] -= step[axis];

    float prevHeightCross = REAL_MAX;
    float prevRayHeight = 0.0f;
    float prevT = -1.0f;
    if ((unsigned int)cell[0] >= (unsigned int)xRes)
    {
        return;
    }
    while (true)
    {
        const int cx = cell[0];
        const int cz = cell[2];
        if ((unsigned int)cz >= (unsigned int)m_zRes || prevT > collector.m_earlyOutHitFraction)
        {
            return;
        }
        const float tCur = tNext[axis];
        lerp4(fromS, toS, tCur, pos);

        // the height field where the ray crosses the grid line (x87)
        float h0;
        float h1;
        float frac;
        if (axis == 0)
        {
            h0 = getHeightAt(cx, cz - stepZ);
            h1 = getHeightAt(cx, cz);
            frac = fabsf(pos[2] - (float)cz);
        }
        else if (axis == 2)
        {
            h0 = getHeightAt(cx - stepX, cz);
            h1 = getHeightAt(cx, cz);
            frac = fabsf(pos[0] - (float)cx);
        }
        else
        {
            frac = fabsf(pos[0] - (float)cx);
            if (useDiagonal)
            {
                h0 = getHeightAt(cx - stepX, cz);
                h1 = getHeightAt(cx, cz - stepZ);
            }
            else
            {
                h0 = getHeightAt(cx - stepX, cz - stepZ);
                h1 = getHeightAt(cx, cz);
            }
        }
        const float heightCross = (float)(((double)h0 - (double)h1) * (double)frac + (double)h1);
        const float rayHeight = pos[3];

        // the hit fraction where the ray goes under the height field
        bool hit = false;
        float hitT = 0.0f;
        if (!(rayHeight >= heightCross))
        {
            const float dCur = rayHeight - heightCross;
            bool interpolate = true;
            if (reportPenetrating)
            {
                const float dPrev = prevRayHeight - prevHeightCross;
                if (dCur >= dPrev - (tCur - prevT) * maxExtraPenetration)
                {
                    interpolate = false;
                }
                else if (0.0f >= dPrev)
                {
                    // started under the surface
                    hitT = (0.0f > prevT) ? 0.0f : prevT; // maxss
                    hit = true;
                    interpolate = false;
                }
            }
            else if (prevHeightCross > prevRayHeight)
            {
                interpolate = false;
            }
            if (interpolate)
            {
                if (prevT >= 0.0f)
                {
                    const float a = prevRayHeight - prevHeightCross;
                    hitT = (a / (a - dCur)) * (tCur - prevT) + prevT;
                    hit = true;
                }
                else
                {
                    // the first line: the start height against the height field interpolated back to t 0
                    const float r = tCur / (tCur - prevT);
                    const float startHeight = (1.0f - r) * heightCross + r * prevHeightCross;
                    float above = fromS[3] - startHeight;
                    if (0.0f > above)
                    {
                        if (reportPenetrating)
                        {
                            above = 0.0f;
                            hit = true;
                        }
                    }
                    else
                    {
                        hit = true;
                    }
                    if (hit)
                    {
                        hitT = (above / (above - dCur)) * tCur;
                    }
                }
            }
        }
        prevHeightCross = heightCross;
        prevRayHeight = rayHeight;
        prevT = tCur;

        if (hit)
        {
            if (hitT > collector.m_earlyOutHitFraction)
            {
                return;
            }
            hkpShapeRayCastOutput output;
            output.m_hitFraction = 1.0f;
            output.m_extraInfo = -1;
            output.m_shapeKeys[0] = 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
            output.m_shapeKeyIndex = 0;

            // the triangle's normal from the heights around the crossing (x87 differences)
            const int czPrev = cz - stepZ;
            float nx;
            float nz;
            bool greater;
            if (useDiagonal)
            {
                const float hA = getHeightAt(cx, czPrev);
                const float hB = getHeightAt(cx - stepX, cz);
                if (axis == 1)
                {
                    const float hC = getHeightAt(cx - stepX, czPrev);
                    nx = x87Sub(hC, hA);
                    nz = x87Sub(hC, hB);
                    greater = czPrev > cz;
                }
                else
                {
                    const float hC = getHeightAt(cx, cz);
                    nx = x87Sub(hB, hC);
                    nz = x87Sub(hA, hC);
                    greater = cz > czPrev;
                }
            }
            else
            {
                const float hA = getHeightAt(cx - stepX, czPrev);
                const float hB = getHeightAt(cx, cz);
                if (axis == 0 || (axis == 1 && absDir[2] > absDir[0]))
                {
                    const float hC = getHeightAt(cx, czPrev);
                    nx = x87Sub(hA, hC);
                    nz = x87Sub(hC, hB);
                    greater = czPrev > cz;
                }
                else
                {
                    const float hC = getHeightAt(cx - stepX, cz);
                    nx = x87Sub(hC, hB);
                    nz = x87Sub(hA, hC);
                    greater = cz > czPrev;
                }
            }
            const int triangle = (greater != flip) ? 1 : 0;
            nx = (float)stepX * nx;
            nz = (float)stepZ * nz;
            output.m_normal.m_quad = normalize3(_mm_mul_ps(_mm_set_ps(0.0f, nz, 1.0f, nx), m_floatToIntScale.m_quad));
            // the cell below the crossing; 0x10002 undoes the +1 of both cell coordinates
            output.m_extraInfo = triangle + 2 * ((((cz - (stepZ >> 1)) << 15) - (stepX >> 1)) + cx) - 0x10002;
            output.m_hitFraction = hitT;
            collector.addRayHit(cdBody, output);
            reportPenetrating = false;
        }

        // next grid line
        cell[axis] += step[axis];
        lastAxis ^= 2;
        tNext[axis] = tDelta[axis] + tCur;
        if (useDiagonal)
        {
            axis = (lastAxis == 0) ? 1 : ((tNext[2] > tNext[0]) ? 0 : 2);
        }
        else if (absDir[2] > absDir[0])
        {
            axis = (lastAxis != 0) ? 2 : ((tNext[1] > tNext[0]) ? 0 : 1);
        }
        else
        {
            axis = (lastAxis != 0) ? 0 : ((tNext[2] > tNext[1]) ? 1 : 2);
        }
        if ((unsigned int)cell[0] >= (unsigned int)m_xRes)
        {
            return;
        }
    }
}

// ---- CoarseMinMaxLevel

// 0x00DB0E50
hkpSampledHeightFieldShape::CoarseMinMaxLevel::CoarseMinMaxLevel()
{
}

// 0x00D7CCC0
hkpSampledHeightFieldShape::CoarseMinMaxLevel::CoarseMinMaxLevel(hkFinishLoadedObjectFlag f)
    : m_minMaxData(f)
{
}

// 0x00DA7EB0
hkpSampledHeightFieldShape::CoarseMinMaxLevel::CoarseMinMaxLevel(const CoarseMinMaxLevel& l)
    : m_xRes(l.m_xRes)
    , m_zRes(l.m_zRes)
{
    m_minMaxData = l.m_minMaxData;
}

// 0x00D7CCD0
hkpSampledHeightFieldShape::CoarseMinMaxLevel::~CoarseMinMaxLevel()
{
}
