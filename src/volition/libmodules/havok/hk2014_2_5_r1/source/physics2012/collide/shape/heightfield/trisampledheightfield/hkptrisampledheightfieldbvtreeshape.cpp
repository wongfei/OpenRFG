// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/trisampledheightfield/hkptrisampledheightfieldbvtreeshape.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): the bounding volume "tree" of a tri-sampled height field collection, from
// rfg.exe: queries return the triangle keys of the cells under the box (the grid is the tree). With the AABB
// rejection test, a box entirely above or below all heights of the cells' corners gives no keys.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/trisampledheightfield/hkptrisampledheightfieldbvtreeshape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4util.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/sampledheightfield/hkpsampledheightfieldshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/trisampledheightfield/hkptrisampledheightfieldcollection.h"

#include <emmintrin.h>

namespace
{
// The rejection test on one corner: the box is not entirely above the heights once a height is above its bottom
// (compared unrounded, on the x87 stack), and not entirely below once its top is above a height.
void testCorner(const hkpSampledHeightFieldShape* hf, int x, int z, float scaleY, const hkAabb& aabb, bool& allBelowBox, bool& allAboveBox)
{
    const double height = (double)hf->getHeightAt(x, z) * (double)scaleY;
    if (height > (double)aabb.m_min(1))
    {
        allBelowBox = false;
    }
    if (aabb.m_max(1) > (float)height)
    {
        allAboveBox = false;
    }
}

// The corners from (minX, maxZ + 1) to (maxX + 1, maxZ + 1) and (maxX + 1, minZ) to (maxX + 1, maxZ + 1): the row
// and column past the cells, not visited by the key loop.
void testFarCorners(const hkpSampledHeightFieldShape* hf, unsigned int minX, unsigned int maxX, unsigned int minZ, unsigned int maxZ, float scaleY, const hkAabb& aabb, bool& allBelowBox, bool& allAboveBox)
{
    for (unsigned int x = minX; x <= maxX + 1; x++)
    {
        testCorner(hf, x, maxZ + 1, scaleY, aabb, allBelowBox, allAboveBox);
    }
    for (unsigned int z = minZ; z <= maxZ + 1; z++)
    {
        testCorner(hf, maxX + 1, z, scaleY, aabb, allBelowBox, allAboveBox);
    }
}
}

// 0x00DA6450: the collection gets a reference
hkpTriSampledHeightFieldBvTreeShape::hkpTriSampledHeightFieldBvTreeShape(const hkpTriSampledHeightFieldCollection* c, hkBool doAabbRejection)
    : hkpBvTreeShape(hkcdShapeType::TRI_SAMPLED_HEIGHT_FIELD_BV_TREE, BVTREE_TRISAMPLED_HEIGHTFIELD)
    , m_childContainer(c, hkpShapeContainer::REFERENCE_POLICY_INCREMENT)
{
    m_wantAabbRejectionTest = doAabbRejection;
}

// 0x00DA64B0
hkpTriSampledHeightFieldBvTreeShape::hkpTriSampledHeightFieldBvTreeShape(hkFinishLoadedObjectFlag flag)
    : hkpBvTreeShape(flag)
    , m_childContainer(flag)
{
    if (flag.m_finishing)
    {
        afterReflectNew();
    }
}

// 0x00DA6D40 (the collection's reference goes with m_childContainer)
hkpTriSampledHeightFieldBvTreeShape::~hkpTriSampledHeightFieldBvTreeShape()
{
}

// 0x00DA64A0
void hkpTriSampledHeightFieldBvTreeShape::afterReflectNew()
{
    m_type.m_storage = hkcdShapeType::TRI_SAMPLED_HEIGHT_FIELD_BV_TREE;
    m_bvTreeType.m_storage = BVTREE_TRISAMPLED_HEIGHTFIELD;
}

// 0x0080DBB0 (hkptrisampledheightfieldbvtreeshape.inl:14)
const hkpTriSampledHeightFieldCollection* hkpTriSampledHeightFieldBvTreeShape::getShapeCollection() const
{
    return static_cast<const hkpTriSampledHeightFieldCollection*>(m_childContainer.m_childShape);
}

// 0x00DA6CF0: the collection's container
const hkpShapeContainer* hkpTriSampledHeightFieldBvTreeShape::getContainer() const
{
    return m_childContainer.m_childShape->getContainer();
}

// 0x00DA62D0: the collection's box
void hkpTriSampledHeightFieldBvTreeShape::getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const
{
    m_childContainer.m_childShape->getAabb(localToWorld, tolerance, out);
}

// 0x00DA6300: the height field's ray cast
hkBool hkpTriSampledHeightFieldBvTreeShape::castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& output) const
{
    return getShapeCollection()->getHeightFieldShape()->castRay(input, output);
}

// 0x00DA6330
void hkpTriSampledHeightFieldBvTreeShape::castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const
{
    getShapeCollection()->getHeightFieldShape()->castRayWithCollector(input, cdBody, collector);
}

// 0x00DA6350: SPU collision needs a child of at most 256 bytes
int hkpTriSampledHeightFieldBvTreeShape::calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const
{
    const unsigned int childSize = (unsigned int)m_childContainer.m_childShape->calcSizeForSpu(input, 256); // 256: the SPU child shape buffer
    if (childSize > 256)
    {
        return -1;
    }
    const_cast<hkpTriSampledHeightFieldBvTreeShape*>(this)->m_childSize = (int)childSize;
    return sizeof(*this);
}

// 0x00DA64E0
// The cells under the box: the box grows by the collection's radius and is clipped (x and z) to the height field's
// box, then converted to the 16-bit grid clamped to [0, res - 2]. False when the clipped box is empty.
hkBool hkpTriSampledHeightFieldBvTreeShape::getExtentsForQueryAabb(hkAabb& aabb, const hkpTriSampledHeightFieldCollection* collection, unsigned int& minX, unsigned int& maxX, unsigned int& minZ, unsigned int& maxZ)
{
    const hkpSampledHeightFieldShape* hf = collection->getHeightFieldShape();
    const __m128 radius = _mm_set1_ps(collection->m_radius);
    aabb.m_max.m_quad = _mm_add_ps(aabb.m_max.m_quad, radius);
    aabb.m_min.m_quad = _mm_sub_ps(aabb.m_min.m_quad, radius);

    hkAabb hfAabb;
    hf->getAabb(hkTransformf::getIdentity(), 0.0f, hfAabb);
    const __m128 xz = _mm_castsi128_ps(_mm_set_epi32(0, -1, 0, -1));
    const __m128 raiseMin = _mm_and_ps(_mm_cmplt_ps(aabb.m_min.m_quad, hfAabb.m_min.m_quad), xz);
    const __m128 lowerMax = _mm_and_ps(_mm_cmplt_ps(hfAabb.m_max.m_quad, aabb.m_max.m_quad), xz);
    aabb.m_min.m_quad = _mm_or_ps(_mm_andnot_ps(raiseMin, aabb.m_min.m_quad), _mm_and_ps(raiseMin, hfAabb.m_min.m_quad));
    aabb.m_max.m_quad = _mm_or_ps(_mm_andnot_ps(lowerMax, aabb.m_max.m_quad), _mm_and_ps(lowerMax, hfAabb.m_max.m_quad));
    if (_mm_movemask_ps(_mm_cmplt_ps(aabb.m_max.m_quad, aabb.m_min.m_quad)) & 7)
    {
        return false;
    }

    hkVector4f clip;
    clip.set((float)(hf->m_xRes - 2), 0.0f, (float)(hf->m_zRes - 2), 0.0f);
    hkIntUnion64 lo;
    hkIntUnion64 hi;
    hkVector4UtilImpl<float>::convertToUint16WithClip(aabb.m_min, hf->m_floatToIntOffsetFloorCorrected, hf->m_floatToIntScale, hkVector4f::getZero(), clip, lo);
    hkVector4UtilImpl<float>::convertToUint16WithClip(aabb.m_max, hf->m_floatToIntOffsetFloorCorrected, hf->m_floatToIntScale, hkVector4f::getZero(), clip, hi);
    // a negative scale swaps the ends
    minX = (lo.u16[0] < hi.u16[0]) ? lo.u16[0] : hi.u16[0];
    maxX = (lo.u16[0] < hi.u16[0]) ? hi.u16[0] : lo.u16[0];
    minZ = (lo.u16[2] < hi.u16[2]) ? lo.u16[2] : hi.u16[2];
    maxZ = (lo.u16[2] < hi.u16[2]) ? hi.u16[2] : lo.u16[2];
    return true;
}

// 0x00DA6950: both triangles of each cell under the box
void hkpTriSampledHeightFieldBvTreeShape::queryAabb(const hkAabb& aabbIn, hkArray<unsigned int, hkContainerHeapAllocator>& hits) const
{
    hkAabb aabb = aabbIn;
    unsigned int minX;
    unsigned int maxX;
    unsigned int minZ;
    unsigned int maxZ;
    if (!getExtentsForQueryAabb(aabb, getShapeCollection(), minX, maxX, minZ, maxZ))
    {
        return;
    }
    const hkpSampledHeightFieldShape* hf = getShapeCollection()->getHeightFieldShape();
    const int oldSize = hits.getSize();
    if (m_wantAabbRejectionTest)
    {
        const float scaleY = hf->m_intToFloatScale(1);
        bool allBelowBox = true;
        bool allAboveBox = true;
        for (unsigned int x = minX; x <= maxX; x++)
        {
            unsigned int key = (minZ * 0x8000 + x) * 2;
            for (unsigned int z = minZ; z <= maxZ; z++, key += 0x10000)
            {
                hits.pushBack(key);
                hits.pushBack(key | 1);
                if (allBelowBox || allAboveBox)
                {
                    testCorner(hf, x, z, scaleY, aabb, allBelowBox, allAboveBox);
                }
            }
        }
        if (!allBelowBox && !allAboveBox)
        {
            return;
        }
        testFarCorners(hf, minX, maxX, minZ, maxZ, scaleY, aabb, allBelowBox, allAboveBox);
        if (allBelowBox || allAboveBox)
        {
            hits.setSize(oldSize);
        }
        return;
    }
    for (unsigned int x = minX; x <= maxX; x++)
    {
        unsigned int key = (minZ * 0x8000 + x) * 2;
        for (unsigned int z = minZ; z <= maxZ; z++, key += 0x10000)
        {
            hits.pushBack(key);
            hits.pushBack(key | 1);
        }
    }
}

// 0x00DA66C0
// As queryAabb into hits (at most maxNumKeys; the count goes on), with the box first grown by the triangle extrusion.
unsigned int hkpTriSampledHeightFieldBvTreeShape::queryAabbImpl(const hkAabb& aabbIn, unsigned int* hits, int maxNumKeys) const
{
    const hkpTriSampledHeightFieldCollection* collection = getShapeCollection();
    hkAabb aabb;
    aabb.m_min.m_quad = _mm_min_ps(_mm_sub_ps(aabbIn.m_min.m_quad, collection->m_triangleExtrusion.m_quad), aabbIn.m_min.m_quad);
    aabb.m_max.m_quad = _mm_max_ps(_mm_sub_ps(aabbIn.m_max.m_quad, collection->m_triangleExtrusion.m_quad), aabbIn.m_max.m_quad);
    unsigned int minX;
    unsigned int maxX;
    unsigned int minZ;
    unsigned int maxZ;
    if (!getExtentsForQueryAabb(aabb, collection, minX, maxX, minZ, maxZ))
    {
        return 0;
    }
    int numHits = 0;
    const hkpSampledHeightFieldShape* hf = collection->getHeightFieldShape();
    if (m_wantAabbRejectionTest)
    {
        const float scaleY = hf->m_intToFloatScale(1);
        bool allBelowBox = true;
        bool allAboveBox = true;
        for (unsigned int x = minX; x <= maxX; x++)
        {
            unsigned int key = (minZ * 0x8000 + x) * 2;
            for (unsigned int z = minZ; z <= maxZ; z++, key += 0x10000)
            {
                if (numHits < maxNumKeys)
                {
                    hits[numHits++] = key;
                    if (numHits < maxNumKeys)
                    {
                        hits[numHits++] = key | 1;
                    }
                }
                if (allBelowBox || allAboveBox)
                {
                    testCorner(hf, x, z, scaleY, aabb, allBelowBox, allAboveBox);
                }
            }
        }
        if (!allBelowBox && !allAboveBox)
        {
            return numHits;
        }
        testFarCorners(hf, minX, maxX, minZ, maxZ, scaleY, aabb, allBelowBox, allAboveBox);
        if (allBelowBox || allAboveBox)
        {
            return 0;
        }
        return numHits;
    }
    for (unsigned int x = minX; x <= maxX; x++)
    {
        unsigned int key = (minZ * 0x8000 + x) * 2;
        for (unsigned int z = minZ; z <= maxZ; z++, key += 0x10000)
        {
            if (numHits < maxNumKeys)
            {
                hits[numHits++] = key;
                if (numHits < maxNumKeys)
                {
                    hits[numHits++] = key | 1;
                }
            }
        }
    }
    return numHits;
}

// 0x00DA63A0: the box around the oriented box
void hkpTriSampledHeightFieldBvTreeShape::queryObb(const hkTransformf& obbTransform, const hkVector4f& obbExtent, float tolerance, hkArray<unsigned int, hkContainerHeapAllocator>& hits) const
{
    const hkMatrix3f& r = obbTransform.getRotation();
    const __m128 ex = _mm_shuffle_ps(obbExtent.m_quad, obbExtent.m_quad, 0x00);
    const __m128 ey = _mm_shuffle_ps(obbExtent.m_quad, obbExtent.m_quad, 0x55);
    const __m128 ez = _mm_shuffle_ps(obbExtent.m_quad, obbExtent.m_quad, 0xAA);
    const __m128 signMask = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));
    const __m128 ax = _mm_and_ps(_mm_mul_ps(ex, r.getColumn(0).m_quad), signMask);
    const __m128 ay = _mm_and_ps(_mm_mul_ps(ey, r.getColumn(1).m_quad), signMask);
    const __m128 az = _mm_and_ps(_mm_mul_ps(ez, r.getColumn(2).m_quad), signMask);
    const __m128 extent = _mm_add_ps(_mm_add_ps(ay, ax), _mm_add_ps(az, _mm_set1_ps(tolerance)));
    hkAabb aabb;
    aabb.m_max.m_quad = _mm_add_ps(obbTransform.getTranslation().m_quad, extent);
    aabb.m_min.m_quad = _mm_add_ps(_mm_xor_ps(extent, _mm_set1_ps(-0.0f)), obbTransform.getTranslation().m_quad);
    queryAabb(aabb, hits);
}
