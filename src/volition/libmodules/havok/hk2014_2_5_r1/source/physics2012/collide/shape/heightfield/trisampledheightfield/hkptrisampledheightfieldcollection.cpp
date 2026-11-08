// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/trisampledheightfield/hkptrisampledheightfieldcollection.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): a sampled height field as a collection of triangles, from rfg.exe. Two
// triangles per cell; the key is (z << 16) | (x << 1) | triangle.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/trisampledheightfield/hkptrisampledheightfieldcollection.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/triangle/hkptriangleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/sampledheightfield/hkpsampledheightfieldshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapebuffer.h"

#include <emmintrin.h>
#include <new>

// 0x00D9C7C0: the height field gets a reference
hkpTriSampledHeightFieldCollection::hkpTriSampledHeightFieldCollection(const hkpSampledHeightFieldShape* shape, float radius)
    : hkpShapeCollection(hkcdShapeType::TRI_SAMPLED_HEIGHT_FIELD_COLLECTION, COLLECTION_TRISAMPLED_HEIGHTFIELD)
{
    m_radius = radius;
    m_heightfield = shape;
    m_triangleExtrusion.setZero();
    shape->addReference();
}

// 0x00D9C840
hkpTriSampledHeightFieldCollection::hkpTriSampledHeightFieldCollection(hkFinishLoadedObjectFlag flag)
    : hkpShapeCollection(flag)
    , m_weldingInfo(flag)
{
    if (flag.m_finishing)
    {
        afterReflectNew();
    }
}

// 0x00D9C870
hkpTriSampledHeightFieldCollection::~hkpTriSampledHeightFieldCollection()
{
    m_heightfield->removeReference();
}

// 0x00D9C5C0
void hkpTriSampledHeightFieldCollection::afterReflectNew()
{
    m_type.m_storage = hkcdShapeType::TRI_SAMPLED_HEIGHT_FIELD_COLLECTION;
    m_collectionType.m_storage = COLLECTION_TRISAMPLED_HEIGHTFIELD;
}

// 0x00D9C500
const hkpSampledHeightFieldShape* hkpTriSampledHeightFieldCollection::getHeightFieldShape() const
{
    return m_heightfield;
}

// 0x00DA62A0 (hkptrisampledheightfieldcollection.inl)
float hkpTriSampledHeightFieldCollection::getRadius() const
{
    return m_radius;
}

// 0x00D9C410 (hkptrisampledheightfieldcollection.inl)
const hkVector4f& hkpTriSampledHeightFieldCollection::getTriangleExtrusion() const
{
    return m_triangleExtrusion;
}

// 0x00D9C420 (hkptrisampledheightfieldcollection.inl)
int hkpTriSampledHeightFieldCollection::getXFromShapeKey(unsigned int key) const
{
    return (key & 0x0000FFFF) >> 1;
}

// 0x00D9C430 (hkptrisampledheightfieldcollection.inl)
int hkpTriSampledHeightFieldCollection::getZFromShapeKey(unsigned int key) const
{
    return key >> 16;
}

// 0x00D9C9B0 (hkptrisampledheightfieldcollection.inl): two triangles per cell, x fastest
int hkpTriSampledHeightFieldCollection::getIndexFromShapeKey(unsigned int key) const
{
    const int x = getXFromShapeKey(key);
    const int z = getZFromShapeKey(key);
    return ((getHeightFieldShape()->m_xRes - 1) * z + x) * 2 | (key & 1);
}

// 0x00D9C9E0 (hkptrisampledheightfieldcollection.inl)
unsigned short hkpTriSampledHeightFieldCollection::getWeldingInfo(unsigned int key) const
{
    return m_weldingInfo[getIndexFromShapeKey(key)];
}

// 0x00D9C440
unsigned int hkpTriSampledHeightFieldCollection::getFirstKey() const
{
    return 0;
}

// 0x00D9C450: the second triangle of the cell, then the next cell (x fastest); -1 after the last
unsigned int hkpTriSampledHeightFieldCollection::getNextKey(unsigned int oldKey) const
{
    if ((oldKey & 1) == 0)
    {
        return oldKey | 1;
    }
    int x = ((oldKey >> 1) & 0x7FFF) + 1;
    unsigned int z = oldKey >> 16;
    if (x == m_heightfield->m_xRes - 1)
    {
        z++;
        x = 0;
        if (z == (unsigned int)(m_heightfield->m_zRes - 1))
        {
            return 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
        }
    }
    return (z * 0x8000 + x) * 2;
}

// 0x00D9C4F0
unsigned int hkpTriSampledHeightFieldCollection::getCollisionFilterInfo(unsigned int key) const
{
    return 0;
}

// 0x00D9C5D0
// The triangle of the key in the buffer: the cell's corners p[i] = (x + i/2, height, z + i%2) times the int to float
// scale, wound by the triangle flip (and the key's triangle bit), with the collection's radius, welding (when
// initialised) and extrusion.
const hkpShape* hkpTriSampledHeightFieldCollection::getChildShape(unsigned int key, hkpShapeBufferStorage& buffer) const
{
    const hkpSampledHeightFieldShape* hf = getHeightFieldShape();
    const int x = getXFromShapeKey(key);
    const int z = getZFromShapeKey(key);
    hkpTriangleShape* triangle = new (static_cast<void*>(buffer.m_storage)) hkpTriangleShape(m_radius, 0, hkpWeldingUtility::WELDING_TYPE_NONE);

    hkVector4f p[4];
    for (int i = 0; i < 4; i++)
    {
        const int xi = x + (i >> 1);
        const int zi = z + (i & 1);
        const float height = hf->getHeightAt(xi, zi);
        p[i].set((float)xi, height, (float)zi, 0.0f);
        p[i].m_quad = _mm_mul_ps(p[i].m_quad, hf->m_intToFloatScale.m_quad);
    }

    const int flip = hf->getTriangleFlip() ? 1 : 0;
    const int t = key & 1;
    triangle->m_vertexA = p[(~flip & t) * 3];
    triangle->m_vertexB = p[1 + (flip + 1) * t];
    triangle->m_vertexC = p[2 + (flip - t)];

    if (m_weldingInfo.getSize())
    {
        triangle->m_weldingInfo = getWeldingInfo(key);
        triangle->m_weldingType.m_storage = hkpWeldingUtility::WELDING_TYPE_ANTICLOCKWISE;
    }
    else
    {
        triangle->m_weldingInfo = 0;
        triangle->m_weldingType.m_storage = hkpWeldingUtility::WELDING_TYPE_NONE;
    }
    triangle->m_isExtruded = m_triangleExtrusion.lengthSquared<3>().getReal() > 0.0f;
    triangle->m_extrusion = m_triangleExtrusion;
    return triangle;
}

// 0x00D9C510: the height field's box grown by the triangles' radius and the extrusion
void hkpTriSampledHeightFieldCollection::getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const
{
    hkAabb aabb;
    getHeightFieldShape()->getAabb(localToWorld, m_radius + tolerance, aabb);
    const __m128 extrusion = m_triangleExtrusion.m_quad;
    out.m_min.m_quad = _mm_min_ps(aabb.m_min.m_quad, _mm_add_ps(extrusion, aabb.m_min.m_quad));
    out.m_max.m_quad = _mm_max_ps(aabb.m_max.m_quad, _mm_add_ps(extrusion, aabb.m_max.m_quad));
}

// 0x00D9C4A0: the height field's size, counted here when it directly follows the collection
int hkpTriSampledHeightFieldCollection::calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const
{
    const int maxChildSize = spuBufferSizeLeft - (int)sizeof(*this);
    const int childSize = m_heightfield->calcSizeForSpu(input, maxChildSize);
    if (childSize < 0 || childSize > maxChildSize)
    {
        return -1;
    }
    hkpTriSampledHeightFieldCollection* self = const_cast<hkpTriSampledHeightFieldCollection*>(this);
    if (reinterpret_cast<const char*>(m_heightfield) == reinterpret_cast<const char*>(this) + sizeof(*this))
    {
        self->m_childSize = 0;
        return childSize + (int)sizeof(*this);
    }
    self->m_childSize = childSize;
    return (int)sizeof(*this);
}

// 0x00D9C8D0: one (zeroed) entry per triangle, or none for WELDING_TYPE_NONE
void hkpTriSampledHeightFieldCollection::initWeldingInfo(hkpWeldingUtility::WeldingType weldingType)
{
    if (weldingType != hkpWeldingUtility::WELDING_TYPE_NONE)
    {
        const hkpSampledHeightFieldShape* hf = getHeightFieldShape();
        const int numTriangles = (hf->m_zRes * 2 - 2) * (hf->m_xRes - 1);
        m_weldingInfo.reserveExactly(numTriangles);
        m_weldingInfo.setSize(numTriangles, 0);
        return;
    }
    m_weldingInfo.clearAndDeallocate();
}

// 0x00D9C580
void hkpTriSampledHeightFieldCollection::setWeldingInfo(unsigned int key, short weldingInfo)
{
    m_weldingInfo[getIndexFromShapeKey(key)] = (unsigned short)weldingInfo;
}
