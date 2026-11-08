// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/sampledheightfield/hkpstoragesampledheightfieldshape.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): a sampled height field that stores its heights as floats (x fastest), from
// rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/sampledheightfield/hkpstoragesampledheightfieldshape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4util.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/sphere/hksphere.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/sampledheightfield/hkpsampledheightfieldbasecinfo.h"

#include <emmintrin.h>
#include <new>

// 0x00DA6E20: the samples are copied (x fastest, xRes * zRes of them)
hkpStorageSampledHeightFieldShape::hkpStorageSampledHeightFieldShape(const hkpSampledHeightFieldBaseCinfo& info, hkArray<float, hkContainerHeapAllocator>& samples)
    : hkpSampledHeightFieldShape(info, HEIGHTFIELD_STORAGE)
{
    m_triangleFlip = false;
    m_storage = samples;
}

// 0x00DA7390
// A storage copy of another sampled height field: its grid, scales, coarse tree and triangle flip, with every height
// read through getHeightAt. The ray cast height range and the projection flag are not copied (rfg.exe).
hkpStorageSampledHeightFieldShape::hkpStorageSampledHeightFieldShape(const hkpSampledHeightFieldShape* hf)
    : hkpSampledHeightFieldShape()
{
    m_heightfieldType.m_storage = HEIGHTFIELD_STORAGE;
    m_xRes = hf->m_xRes;
    m_zRes = hf->m_zRes;
    m_coarseness = hf->m_coarseness;
    if (hf->m_coarseness != 0)
    {
        // hkArray::operator= of non-POD elements (hkarray.inl): assign the common part, copy-construct the rest
        const int n = hf->m_coarseTreeData.getSize();
        const int oldSize = m_coarseTreeData.getSize();
        const int common = (n < oldSize) ? n : oldSize;
        m_coarseTreeData.reserve(n);
        for (int i = oldSize - 1; i >= n; i--)
        {
            m_coarseTreeData[i].~CoarseMinMaxLevel();
        }
        for (int i = 0; i < common; i++)
        {
            m_coarseTreeData[i] = hf->m_coarseTreeData[i];
        }
        for (int i = common; i < n; i++)
        {
            new (&m_coarseTreeData[i]) CoarseMinMaxLevel(hf->m_coarseTreeData[i]);
        }
        m_coarseTreeData.setSizeUnchecked(n);
    }
    m_triangleFlip = hf->getTriangleFlip();
    m_heightCenter = hf->m_heightCenter;
    m_intToFloatScale = hf->m_intToFloatScale;
    m_floatToIntScale = hf->m_floatToIntScale;
    m_floatToIntOffsetFloorCorrected = hf->m_floatToIntOffsetFloorCorrected;
    m_extents = hf->m_extents;
    m_storage.setSize(m_zRes * m_xRes);
    for (int z = 0; z < m_zRes; z++)
    {
        for (int x = 0; x < m_xRes; x++)
        {
            m_storage[m_xRes * z + x] = hf->getHeightAt(x, z);
        }
    }
}

// 0x00D7E0E0
hkpStorageSampledHeightFieldShape::hkpStorageSampledHeightFieldShape(hkFinishLoadedObjectFlag f)
    : hkpSampledHeightFieldShape(f)
    , m_storage(f)
{
    if (f.m_finishing)
    {
        afterReflectNew();
    }
}

// 0x00D7E180
hkpStorageSampledHeightFieldShape::~hkpStorageSampledHeightFieldShape()
{
}

// 0x00D7E0C0
void hkpStorageSampledHeightFieldShape::afterReflectNew()
{
    m_heightfieldType.m_storage = HEIGHTFIELD_STORAGE;
}

// 0x00DA6DD0: the shape alone (0x90 bytes)
int hkpStorageSampledHeightFieldShape::calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const
{
    return (spuBufferSizeLeft >= (int)sizeof(*this)) ? (int)sizeof(*this) : -1;
}

// 0x00D7E160
float hkpStorageSampledHeightFieldShape::getHeightAtImpl(int x, int z) const
{
    return m_storage[m_xRes * z + x];
}

// 0x00D7E110
hkBool hkpStorageSampledHeightFieldShape::getTriangleFlipImpl() const
{
    return m_triangleFlip;
}

// 0x00DA6F60 (hkSampledHeightFieldShape_collideSpheres, hkpsampledheightfieldshape.inl)
// For each sphere: the surface normal (shape space) and in w the distance of the sphere's surface above the height
// field (along y, or along the normal with m_useProjectionBasedHeight). Spheres outside the grid get (0, 1, 0,
// HK_REAL_MAX).
void hkpStorageSampledHeightFieldShape::collideSpheres(const hkpHeightFieldShape::CollideSpheresInput& input, hkVector4f* outputArray) const
{
    const hkSphere* spheres = input.m_spheres;
    hkVector4f* output = outputArray;
    for (int i = input.m_numSpheres - 1; i >= 0; output++, spheres++, i--)
    {
        output->set(0.0f, 1.0f, 0.0f, 3.40282e38f); // HK_REAL_MAX
        hkVector4f pos;
        pos.m_quad = _mm_mul_ps(spheres->m_pos.m_quad, m_floatToIntScale.m_quad);
        hkIntUnion64 cell;
        hkVector4UtilImpl<float>::convertToUint16WithClip(spheres->m_pos, m_floatToIntOffsetFloorCorrected, m_floatToIntScale, hkVector4f::getZero(), hkVector4f::getConstant<HK_QUADREAL_HIGH>(), cell);
        const int x = cell.u16[0];
        const int z = cell.u16[2];
        if (x >= m_xRes - 1 || z >= m_zRes - 1 || !(pos(0) >= 0.0f && pos(2) >= 0.0f))
        {
            continue;
        }
        const float subX = pos(0) - (float)x;
        const float subZ = pos(2) - (float)z;

        float height;
        float vertHeight;
        int vertX;
        if (getTriangleFlip())
        {
            const float h00 = getHeightAt(x, z);
            vertHeight = h00;
            vertX = x;
            const float h11 = getHeightAt(x + 1, z + 1);
            if (subX > subZ)
            {
                const float h10 = getHeightAt(x + 1, z);
                const float dx = h10 - h00;
                const float dz = h11 - h10;
                height = h00 + subZ * dz + subX * dx;
                (*output)(0) = -dx;
                (*output)(2) = -dz;
            }
            else
            {
                const float h01 = getHeightAt(x, z + 1);
                const float dx = h11 - h01;
                const float dz = h01 - h00;
                height = h00 + subZ * dz + subX * dx;
                (*output)(0) = -dx;
                (*output)(2) = -dz;
            }
        }
        else
        {
            const float h10 = getHeightAt(x + 1, z);
            vertHeight = h10;
            vertX = x + 1;
            const float h01 = getHeightAt(x, z + 1);
            if (subX + subZ > 1.0f)
            {
                const float h11 = getHeightAt(x + 1, z + 1);
                const float dx = h11 - h01;
                const float dz = h11 - h10;
                height = h10 + subZ * dz + (subX - 1.0f) * dx;
                (*output)(0) = -dx;
                (*output)(2) = -dz;
            }
            else
            {
                const float h00 = getHeightAt(x, z);
                const float dx = h10 - h00;
                const float dz = h01 - h00;
                height = h00 + subZ * dz + subX * dx;
                (*output)(0) = -dx;
                (*output)(2) = -dz;
            }
        }

        // reverse multiply to correct for the optimized cross product, then normalize<3> (23-bit reciprocal square
        // root, one Newton step; zero for a zero vector)
        const __m128 n = _mm_mul_ps(m_floatToIntScale.m_quad, output->m_quad);
        const __m128 sq = _mm_mul_ps(n, n);
        const __m128 len2 = _mm_add_ps(_mm_add_ps(_mm_shuffle_ps(sq, sq, 0x55), _mm_shuffle_ps(sq, sq, 0x00)), _mm_shuffle_ps(sq, sq, 0xAA));
        const __m128 r = _mm_rsqrt_ps(len2);
        const __m128 inv = _mm_mul_ps(_mm_mul_ps(_mm_sub_ps(_mm_set1_ps(3.0f), _mm_mul_ps(_mm_mul_ps(r, len2), r)), r), _mm_set1_ps(0.5f));
        output->m_quad = _mm_mul_ps(_mm_andnot_ps(_mm_cmple_ps(len2, _mm_setzero_ps()), inv), n);

        float depth;
        if (!m_useProjectionBasedHeight)
        {
            depth = (pos(1) - height) * m_intToFloatScale(1) - spheres->m_pos(3);
        }
        else
        {
            const float toTriX = (pos(0) - (float)vertX) * m_intToFloatScale(0);
            const float toTriY = (pos(1) - vertHeight) * m_intToFloatScale(1);
            const float toTriZ = (pos(2) - (float)z) * m_intToFloatScale(2);
            depth = (toTriY * (*output)(1) + toTriX * (*output)(0)) + toTriZ * (*output)(2) - spheres->m_pos(3);
        }
        (*output)(3) = depth;
    }
}
