// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/extendedmeshshape/hkpextendedmeshshape.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): a mesh of triangle subparts (indexed vertices under a scaled transform; each
// triangle is built on demand in the shape buffer) and shape subparts (convex children under an optional
// translation/rotation), from rfg.exe. A key holds the subpart type in bit 31, the subpart index in the next
// m_numBitsForSubpartIndex bits and the triangle or child index below. A mesh with one triangle subpart keeps it in
// m_embeddedTrianglesSubpart. The constructor from an hkpMeshShape (0x00D94FB0) has no caller in rfg.exe and is not
// written.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/extendedmeshshape/hkpextendedmeshshape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabbutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/algorithms/triangle/hkcdtriangleutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/mesh/hkpmeshmaterial.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convextransform/hkpconvextransformshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convextranslate/hkpconvextranslateshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/triangle/hkptriangleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapebuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/hkptriangleutil.h"

#include <new>
#include <string.h>

namespace
{
const unsigned int kInvalidShapeKey = 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY

// The vertex (x, y, z; w = 0) at v under the subpart transform, into the box.
void addToAabb(hkAabb& aabb, const hkQsTransformf& localToWorld, const float* v)
{
    hkVector4 vertex; vertex.set(v[0], v[1], v[2], 0.0f);
    hkVector4 p; p._setTransformedPos(localToWorld, vertex);
    aabb.includePoint(p);
}

// A triangle whose normals are shorter than the default tolerance (or whose edges are parallel).
bool isDegenerateTriangle(const hkpShape* triangle)
{
    const hkpTriangleShape* t = static_cast<const hkpTriangleShape*>(triangle);
    const hkSimdReal tolerance = hkSimdReal::fromFloat(hkDefaultTriangleDegeneracyTolerance);
    return hkcdTriangleUtil::isDegenerate(t->m_vertexA, t->m_vertexB, t->m_vertexC, tolerance) != 0;
}
}

// ---- subparts (hkpExtendedMeshShape.h/.inl)

// 0x00D936A0: no materials (one, of 8-bit indices)
hkpExtendedMeshShape::Subpart::Subpart(SubpartType type)
{
    m_typeAndFlags = (unsigned short)((type & SUBPART_TYPE_MASK) | (MATERIAL_INDICES_INT8 << SUBPART_MATERIAL_INDICES_SHIFT) | (1 << SUBPART_NUM_MATERIALS_SHIFT));
    m_materialStriding = 0;
    m_materialIndexStriding = 0;
    m_materialBase = nullptr;
    m_materialIndexBase = nullptr;
    m_shapeInfo = 0;
    m_userData = 0;
}

// 0x00D7CF00
hkpExtendedMeshShape::Subpart::Subpart(hkFinishLoadedObjectFlag flag)
{
}

// 0x00E277C0
hkpExtendedMeshShape::SubpartType hkpExtendedMeshShape::Subpart::getType() const
{
    return SubpartType(m_typeAndFlags & SUBPART_TYPE_MASK);
}

// 0x00D93630
void hkpExtendedMeshShape::Subpart::setType(SubpartType newValue)
{
    m_typeAndFlags = (unsigned short)((m_typeAndFlags & ~SUBPART_TYPE_MASK) | newValue);
}

// 0x00D93650
hkpExtendedMeshShape::MaterialIndexStridingType hkpExtendedMeshShape::Subpart::getMaterialIndexStridingType() const
{
    return MaterialIndexStridingType((m_typeAndFlags & SUBPART_MATERIAL_INDICES_MASK) >> SUBPART_MATERIAL_INDICES_SHIFT);
}

// 0x00D93660
void hkpExtendedMeshShape::Subpart::setMaterialIndexStridingType(MaterialIndexStridingType newValue)
{
    m_typeAndFlags = (unsigned short)((m_typeAndFlags & ~SUBPART_MATERIAL_INDICES_MASK) | (newValue << SUBPART_MATERIAL_INDICES_SHIFT));
}

// 0x00DA0330
unsigned short hkpExtendedMeshShape::Subpart::getNumMaterials() const
{
    return (unsigned short)(m_typeAndFlags >> SUBPART_NUM_MATERIALS_SHIFT);
}

// 0x00D93680
void hkpExtendedMeshShape::Subpart::setNumMaterials(unsigned short newValue)
{
    m_typeAndFlags = (unsigned short)((m_typeAndFlags & ~SUBPART_NUM_MATERIALS_MASK) | (newValue << SUBPART_NUM_MATERIALS_SHIFT));
}

// 0x00D95EF0: the vertex and index data are left to the caller; the transform is the identity
hkpExtendedMeshShape::TrianglesSubpart::TrianglesSubpart()
    : Subpart(SUBPART_TRIANGLES)
{
    m_flipAlternateTriangles = 0;
    m_extrusion.setZero();
    m_transform.m_translation.setZero();
    m_transform.m_rotation.m_vec = hkVector4::getConstant<HK_QUADREAL_0001>();
    m_transform.m_scale = hkVector4::getConstant<HK_QUADREAL_1>();
}

// 0x00D7D040
hkpExtendedMeshShape::TrianglesSubpart::TrianglesSubpart(hkFinishLoadedObjectFlag flag)
    : Subpart(flag)
{
}

// 0x00D96DB0: no children; the transform is left to the caller
hkpExtendedMeshShape::ShapesSubpart::ShapesSubpart()
    : Subpart(SUBPART_SHAPE)
{
}

// 0x00D94780
hkpExtendedMeshShape::ShapesSubpart::ShapesSubpart(hkFinishLoadedObjectFlag flag)
    : Subpart(flag)
    , m_childShapes(flag)
{
    if (flag.m_finishing)
    {
        afterReflectNew();
    }
}

// 0x00D94B80: the children lose their references, last first
hkpExtendedMeshShape::ShapesSubpart::~ShapesSubpart()
{
    for (int i = m_childShapes.getSize() - 1; i >= 0; i--)
    {
        m_childShapes[i] = nullptr;
    }
}

// 0x00D93BE0
// The flags in the w of m_translation: a translation beyond 0.001 in some axis, a rotation angle of 0.001 or more.
void hkpExtendedMeshShape::ShapesSubpart::afterReflectNew()
{
    const float eps = 0.001f;
    hkVector4 absTranslation; absTranslation.setAbs(m_translation);
    hkVector4 epsV; epsV.setAll(eps);
    int flags = absTranslation.lessEqual(epsV).allAreSet<hkVector4ComparisonMask::MASK_XYZ>() ? FLAG_NONE : FLAG_TRANSLATE_SET;
    if (!(m_rotation.getAngleSr().getReal() < eps))
    {
        flags |= FLAG_ROTATE_SET;
    }
    setFlags(flags);
}

// 0x00D95940
void hkpExtendedMeshShape::ShapesSubpart::setFlags(int f)
{
    m_translation.setInt24W(f);
}

// 0x00D95950
int hkpExtendedMeshShape::ShapesSubpart::getFlags() const
{
    return m_translation.getInt24W();
}

// 0x00D935C0
const hkVector4f& hkpExtendedMeshShape::ShapesSubpart::getTranslation() const
{
    return m_translation;
}

// 0x00D935D0
const hkQuaternionf& hkpExtendedMeshShape::ShapesSubpart::getRotation() const
{
    return m_rotation;
}

// ---- construction

// 0x00D94EC0
hkpExtendedMeshShape::hkpExtendedMeshShape(float radius, int numBitsForSubpartIndex)
    : hkpShapeCollection(hkcdShapeType::EXTENDED_MESH, COLLECTION_EXTENDED_MESH)
{
    m_defaultCollisionFilterInfo = 0;
    m_triangleRadius = radius;
    m_numBitsForSubpartIndex = numBitsForSubpartIndex;
    m_weldingType.m_storage = hkpWeldingUtility::WELDING_TYPE_NONE;
    m_aabbHalfExtents = hkVector4::getConstant<HK_QUADREAL_MINUS_MAX>(); // an empty box until a subpart is added
    m_cachedNumChildShapes = 0;
    m_materialClass = nullptr;
    m_aabbCenter.setZero();
}

// 0x00D95420
hkpExtendedMeshShape::hkpExtendedMeshShape(hkFinishLoadedObjectFlag flag)
    : hkpShapeCollection(flag)
    , m_embeddedTrianglesSubpart(flag)
    , m_trianglesSubparts(flag)
    , m_shapesSubparts(flag)
    , m_weldingInfo(flag)
{
    if (flag.m_finishing)
    {
        afterReflectNew();
    }
}

// 0x00D95310: the welding info goes first, then the shape subparts (last first), then the subpart arrays
hkpExtendedMeshShape::~hkpExtendedMeshShape()
{
    m_weldingInfo.clearAndDeallocate();
    for (int i = m_shapesSubparts.getSize() - 1; i >= 0; i--)
    {
        m_shapesSubparts[i].~ShapesSubpart();
    }
}

// 0x00D94C40
// Subparts without a material index type get 8-bit indices; the shape subparts finish loading; a single triangle
// subpart moves into the embedded one; an unknown child count (-1) becomes "not computed".
void hkpExtendedMeshShape::afterReflectNew()
{
    m_collectionType.m_storage = COLLECTION_EXTENDED_MESH;
    for (int i = 0; i < m_trianglesSubparts.getSize(); i++)
    {
        Subpart& part = m_trianglesSubparts[i];
        if ((part.m_typeAndFlags & SUBPART_MATERIAL_INDICES_MASK) == 0)
        {
            part.setMaterialIndexStridingType(MATERIAL_INDICES_INT8);
        }
    }
    hkFinishLoadedObjectFlag finishing;
    finishing.m_finishing = 1;
    for (int i = 0; i < m_shapesSubparts.getSize(); i++)
    {
        new (&m_shapesSubparts[i]) ShapesSubpart(finishing);
        Subpart& part = m_shapesSubparts[i];
        if ((part.m_typeAndFlags & SUBPART_MATERIAL_INDICES_MASK) == 0)
        {
            part.setMaterialIndexStridingType(MATERIAL_INDICES_INT8);
        }
    }
    if (m_trianglesSubparts.getSize() == 1)
    {
        memcpy(&m_embeddedTrianglesSubpart, &m_trianglesSubparts[0], sizeof(TrianglesSubpart)); // a raw copy
        m_trianglesSubparts.clearAndDeallocate();
        m_trianglesSubparts.m_data = &m_embeddedTrianglesSubpart;
        m_trianglesSubparts.m_size = 1;
        m_trianglesSubparts.m_capacityAndFlags = 1 | hkArrayBase<TrianglesSubpart>::DONT_DEALLOCATE_FLAG;
    }
    m_type.m_storage = hkcdShapeType::EXTENDED_MESH;
    if (m_cachedNumChildShapes == -1)
    {
        m_cachedNumChildShapes = (int)0x80000000; // computed on the first getNumChildShapes
    }
}

// 0x00D94490: the first triangle subpart is the embedded one
hkpExtendedMeshShape::TrianglesSubpart* hkpExtendedMeshShape::expandOneTriangleSubparts()
{
    if (m_trianglesSubparts.getSize() == 0)
    {
        m_trianglesSubparts.m_data = &m_embeddedTrianglesSubpart;
        m_trianglesSubparts.m_size = 1;
        m_trianglesSubparts.m_capacityAndFlags = 1 | hkArrayBase<TrianglesSubpart>::DONT_DEALLOCATE_FLAG;
        return &m_embeddedTrianglesSubpart;
    }
    const int index = m_trianglesSubparts.getSize();
    m_trianglesSubparts.reserve(index + 1);
    m_trianglesSubparts.m_size++;
    return &m_trianglesSubparts[index];
}

// 0x00D94D80
hkpExtendedMeshShape::ShapesSubpart* hkpExtendedMeshShape::expandOneShapesSubparts()
{
    const int index = m_shapesSubparts.getSize();
    m_shapesSubparts.reserve(index + 1);
    new (&m_shapesSubparts.m_data[index]) ShapesSubpart();
    m_shapesSubparts.m_size++;
    return &m_shapesSubparts[index];
}

// 0x00D946C0
// The cached box grows by the subpart's box (with the triangle radius) and the child count by its non-degenerate
// triangles.
void hkpExtendedMeshShape::addTrianglesSubpart(const TrianglesSubpart& part)
{
    TrianglesSubpart* subpart = expandOneTriangleSubparts();
    *subpart = part;

    hkVector4 currentMin; currentMin.setSub(m_aabbCenter, m_aabbHalfExtents);
    hkVector4 currentMax; currentMax.setAdd(m_aabbHalfExtents, m_aabbCenter);
    hkAabb aabb;
    calcAabbExtents(*subpart, aabb);
    hkVector4 radius; radius.setAll(m_triangleRadius);
    const int subpartIndex = m_trianglesSubparts.getSize() - 1;
    hkVector4 partMin; partMin.setSub(aabb.m_min, radius);
    hkVector4 partMax; partMax.setAdd(radius, aabb.m_max);
    hkVector4 newMin; newMin.setMin(currentMin, partMin);
    hkVector4 newMax; newMax.setMax(currentMax, partMax);
    const hkSimdReal half = hkSimdReal_Inv2;
    m_aabbCenter.setAdd(newMax, newMin);
    m_aabbCenter.mul(half);
    m_aabbHalfExtents.setSub(newMax, newMin);
    m_aabbHalfExtents.mul(half);

    m_cachedNumChildShapes += _getNumChildShapesInTrianglesSubpart(part, subpartIndex);
}

// 0x00D94E10: returns the subpart's index
int hkpExtendedMeshShape::addShapesSubpart(const ShapesSubpart& part)
{
    ShapesSubpart* subpart = expandOneShapesSubparts();

    // the implicit ShapesSubpart::operator= (0x00D969E0): the children are reference counted
    static_cast<Subpart&>(*subpart) = part;
    {
        hkArray<hkRefPtr<hkpConvexShape>, hkContainerHeapAllocator>& dst = subpart->m_childShapes;
        const hkArray<hkRefPtr<hkpConvexShape>, hkContainerHeapAllocator>& src = part.m_childShapes;
        const int oldSize = dst.getSize();
        const int newSize = src.getSize();
        const int copied = newSize < oldSize ? newSize : oldSize;
        dst.reserve(newSize);
        for (int i = oldSize - 1; i >= newSize; i--)
        {
            dst[i] = nullptr;
        }
        for (int i = 0; i < copied; i++)
        {
            dst[i] = src[i].val();
        }
        for (int i = copied; i < newSize; i++)
        {
            hkpConvexShape* child = src[i].val();
            if (child)
            {
                child->addReference();
            }
            *reinterpret_cast<hkpConvexShape**>(&dst.m_data[i]) = child; // constructed in place
        }
        dst.m_size = newSize;
        subpart->m_rotation = part.m_rotation;
        subpart->m_translation = part.m_translation;
    }

    hkVector4 currentMax; currentMax.setAdd(m_aabbHalfExtents, m_aabbCenter);
    hkVector4 currentMin; currentMin.setSub(m_aabbCenter, m_aabbHalfExtents);
    hkAabb aabb;
    calcAabbExtents(*subpart, aabb);
    hkVector4 newMax; newMax.setMax(currentMax, aabb.m_max);
    hkVector4 newMin; newMin.setMin(currentMin, aabb.m_min);
    const hkSimdReal half = hkSimdReal_Inv2;
    m_aabbCenter.setAdd(newMax, newMin);
    m_aabbCenter.mul(half);
    m_aabbHalfExtents.setSub(newMax, newMin);
    m_aabbHalfExtents.mul(half);

    m_cachedNumChildShapes += _getNumChildShapesInShapesSubpart(part);
    return m_shapesSubparts.getSize() - 1;
}

// 0x00D94630: the triangles that are not degenerate
int hkpExtendedMeshShape::_getNumChildShapesInTrianglesSubpart(const TrianglesSubpart& subpart, int subpartIndex) const
{
    int count = 0;
    hkpShapeBuffer buffer;
    for (int i = 0; i < subpart.m_numTriangleShapes; i++)
    {
        const unsigned int key = ((unsigned int)subpartIndex << (32 - m_numBitsForSubpartIndex)) | i;
        const hkpShape* triangle = hkpExtendedMeshShape::getChildShape(key, buffer);
        if (!isDegenerateTriangle(triangle))
        {
            count++;
        }
    }
    return count;
}

// 0x00D93740
int hkpExtendedMeshShape::_getNumChildShapesInShapesSubpart(const ShapesSubpart& subpart) const
{
    return subpart.m_childShapes.getSize();
}

// 0x00D93720
void hkpExtendedMeshShape::freeTriangleSubparts()
{
}

// 0x00D93730
void hkpExtendedMeshShape::freeShapesSubparts()
{
}

// 0x00D93700
void hkpExtendedMeshShape::assertTrianglesSubpartValidity(const TrianglesSubpart& part)
{
}

// 0x00D93710
void hkpExtendedMeshShape::assertShapesSubpartValidity(const ShapesSubpart& part)
{
}

// ---- boxes

// 0x00D94280
// The vertices of the triangles (by index; the alternate flip does not matter here) under the subpart transform.
void hkpExtendedMeshShape::calcAabbExtents(TrianglesSubpart& part, hkAabb& out)
{
    out.m_min = hkVector4::getConstant<HK_QUADREAL_MAX>();
    out.m_max = hkVector4::getConstant<HK_QUADREAL_MINUS_MAX>();
    for (int i = 0; i < part.m_numTriangleShapes; i++)
    {
        const char* indices = static_cast<const char*>(part.m_indexBase) + part.m_indexStriding * i;
        int i0, i1, i2;
        if (part.m_stridingType.m_storage == INDICES_INT8)
        {
            const unsigned char* idx = reinterpret_cast<const unsigned char*>(indices);
            i0 = idx[0]; i1 = idx[1]; i2 = idx[2];
        }
        else if (part.m_stridingType.m_storage == INDICES_INT16)
        {
            const unsigned short* idx = reinterpret_cast<const unsigned short*>(indices);
            i0 = idx[0]; i1 = idx[1]; i2 = idx[2];
        }
        else
        {
            const int* idx = reinterpret_cast<const int*>(indices);
            i0 = idx[0]; i1 = idx[1]; i2 = idx[2];
        }
        const char* vertices = reinterpret_cast<const char*>(part.m_vertexBase);
        addToAabb(out, part.m_transform, reinterpret_cast<const float*>(vertices + i0 * part.m_vertexStriding));
        addToAabb(out, part.m_transform, reinterpret_cast<const float*>(vertices + i1 * part.m_vertexStriding));
        addToAabb(out, part.m_transform, reinterpret_cast<const float*>(vertices + i2 * part.m_vertexStriding));
    }
}

// 0x00D93820: the children's boxes under the subpart transform (rotation and translation, whatever the flags)
void hkpExtendedMeshShape::calcAabbExtents(const ShapesSubpart& part, hkAabb& out)
{
    out.m_min = hkVector4::getConstant<HK_QUADREAL_MAX>();
    out.m_max = hkVector4::getConstant<HK_QUADREAL_MINUS_MAX>();
    hkTransform transform;
    transform.m_rotation.set(part.m_rotation);
    transform.m_translation = part.m_translation;
    for (int i = 0; i < part.m_childShapes.getSize(); i++)
    {
        hkAabb childAabb;
        part.m_childShapes[i]->getAabb(transform, 0.0f, childAabb);
        out.includeAabb(childAabb);
    }
}

// 0x00D94380
void hkpExtendedMeshShape::recalcAabbExtents()
{
    hkVector4 newMin = hkVector4::getConstant<HK_QUADREAL_MAX>();
    hkVector4 newMax = hkVector4::getConstant<HK_QUADREAL_MINUS_MAX>();
    hkVector4 radius; radius.setAll(m_triangleRadius);
    for (int i = 0; i < m_trianglesSubparts.getSize(); i++)
    {
        hkAabb aabb;
        calcAabbExtents(m_trianglesSubparts[i], aabb);
        hkVector4 partMin; partMin.setSub(aabb.m_min, radius);
        hkVector4 partMax; partMax.setAdd(radius, aabb.m_max);
        newMin.setMin(newMin, partMin);
        newMax.setMax(newMax, partMax);
    }
    for (int i = 0; i < m_shapesSubparts.getSize(); i++)
    {
        hkAabb aabb;
        calcAabbExtents(m_shapesSubparts[i], aabb);
        newMin.setMin(newMin, aabb.m_min);
        newMax.setMax(newMax, aabb.m_max);
    }
    const hkSimdReal half = hkSimdReal_Inv2;
    m_aabbCenter.setAdd(newMax, newMin);
    m_aabbCenter.mul(half);
    m_aabbHalfExtents.setSub(newMax, newMin);
    m_aabbHalfExtents.mul(half);
}

// 0x00D93B20: the cached box (the tolerance is the only radius added)
void hkpExtendedMeshShape::getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const
{
    hkAabbUtil::calcAabb(localToWorld, m_aabbHalfExtents, m_aabbCenter, hkSimdReal::fromFloat(tolerance), out);
}

// 0x00D938B0
// A shape subpart child must fit the shape buffer next to the translate or transform shape the key builds around it.
int hkpExtendedMeshShape::calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const
{
    for (int s = 0; s < m_shapesSubparts.getSize(); s++)
    {
        const ShapesSubpart& part = m_shapesSubparts[s];
        for (int i = 0; i < part.m_childShapes.getSize(); i++)
        {
            const int childSize = part.m_childShapes[i]->calcSizeForSpu(input, 256); // 256: the SPU child shape buffer
            const int flags = part.getFlags();
            int maxSize;
            if (flags == ShapesSubpart::FLAG_NONE)
            {
                maxSize = HK_SHAPE_BUFFER_SIZE;
            }
            else if (flags == ShapesSubpart::FLAG_TRANSLATE_SET)
            {
                maxSize = HK_SHAPE_BUFFER_SIZE - (int)sizeof(hkpConvexTranslateShape); // 0x1D0
            }
            else
            {
                maxSize = HK_SHAPE_BUFFER_SIZE - (int)sizeof(hkpConvexTransformShape); // 0x1A0
            }
            if (childSize < 0 || childSize > maxSize)
            {
                return -1;
            }
        }
    }
    return sizeof(*this);
}

// ---- keys and children

// 0x00D935E0
int hkpExtendedMeshShape::getSubPartIndex(unsigned int key) const
{
    return (int)((key & 0x7FFFFFFF) >> (32 - m_numBitsForSubpartIndex));
}

// 0x00D93610
int hkpExtendedMeshShape::getTerminalIndexInSubPart(unsigned int key) const
{
    return (int)(key & (0xFFFFFFFFu >> m_numBitsForSubpartIndex));
}

// 0x00D93600
hkpExtendedMeshShape::SubpartType hkpExtendedMeshShape::getSubpartType(unsigned int key) const
{
    return SubpartType(key >> 31);
}

// 0x00E27B90
const hkpExtendedMeshShape::Subpart& hkpExtendedMeshShape::getSubPart(unsigned int key) const
{
    const int index = getSubPartIndex(key);
    if ((int)key >= 0)
    {
        return m_trianglesSubparts[index];
    }
    return m_shapesSubparts[index];
}

// 0x00E27BD0
unsigned short hkpExtendedMeshShape::getShapeInfo(unsigned int shapeKey) const
{
    return getSubPart(shapeKey).m_shapeInfo;
}

// 0x00D95960
int hkpExtendedMeshShape::getNumTrianglesSubparts() const
{
    return m_trianglesSubparts.getSize();
}

// 0x00D95970
int hkpExtendedMeshShape::getNumShapesSubparts() const
{
    return m_shapesSubparts.getSize();
}

// 0x00DA1970
const hkpExtendedMeshShape::TrianglesSubpart& hkpExtendedMeshShape::getTrianglesSubpartAt(int i) const
{
    return m_trianglesSubparts[i];
}

// 0x00DA1980
const hkpExtendedMeshShape::ShapesSubpart& hkpExtendedMeshShape::getShapesSubpartAt(int i) const
{
    return m_shapesSubparts[i];
}

// 0x00DA0310
int hkpExtendedMeshShape::getNumBitsForSubpartIndex() const
{
    return m_numBitsForSubpartIndex;
}

// 0x00DA0320
float hkpExtendedMeshShape::getRadius() const
{
    return m_triangleRadius;
}

// 0x00D93750
// The material of a key through the subpart's 8- or 16-bit material indices (a single triangle subpart is read from
// the embedded copy); none without indices or a positive striding.
const hkpMeshMaterial* hkpExtendedMeshShape::getMeshMaterial(unsigned int key) const
{
    const int subpartIndex = getSubPartIndex(key);
    const Subpart* part;
    if ((int)key < 0)
    {
        part = &m_shapesSubparts[subpartIndex];
    }
    else if (m_trianglesSubparts.getSize() == 1)
    {
        part = &m_embeddedTrianglesSubpart;
    }
    else
    {
        part = &m_trianglesSubparts[subpartIndex];
    }
    if (part->m_materialIndexBase && part->m_materialStriding > 0)
    {
        const char* index = static_cast<const char*>(part->m_materialIndexBase) + part->m_materialIndexStriding * getTerminalIndexInSubPart(key);
        const int materialIndex = (part->m_typeAndFlags & SUBPART_MATERIAL_INDICES_MASK) == (MATERIAL_INDICES_INT8 << SUBPART_MATERIAL_INDICES_SHIFT) ? *reinterpret_cast<const unsigned char*>(index) : *reinterpret_cast<const unsigned short*>(index);
        return reinterpret_cast<const hkpMeshMaterial*>(reinterpret_cast<const char*>(part->m_materialBase) + materialIndex * part->m_materialStriding);
    }
    return nullptr;
}

// 0x00D937F0
unsigned int hkpExtendedMeshShape::getCollisionFilterInfo(unsigned int key) const
{
    const hkpMeshMaterial* material = getMeshMaterial(key);
    return material ? material->m_filterInfo : m_defaultCollisionFilterInfo;
}

// 0x00D936E0: counted once (the base walks the keys)
int hkpExtendedMeshShape::getNumChildShapes() const
{
    if (m_cachedNumChildShapes < 0)
    {
        const_cast<hkpExtendedMeshShape*>(this)->m_cachedNumChildShapes = hkpShapeContainer::getNumChildShapes();
    }
    return m_cachedNumChildShapes;
}

// 0x00D93960: the first triangle (or the first child when there are no triangle subparts), skipping a degenerate one
unsigned int hkpExtendedMeshShape::getFirstKey() const
{
    if (m_trianglesSubparts.getSize() + m_shapesSubparts.getSize() == 0)
    {
        return kInvalidShapeKey;
    }
    const unsigned int firstKey = m_trianglesSubparts.getSize() ? 0 : 0x80000000;
    hkpShapeBuffer buffer;
    const hkpShape* child = getChildShape(firstKey, buffer);
    if (child->m_type.m_storage == hkcdShapeType::TRIANGLE && isDegenerateTriangle(child))
    {
        return getNextKey(firstKey);
    }
    return firstKey;
}

// 0x00D93A00: the next triangle (after the last triangle subpart, the shape subparts), skipping degenerate triangles
unsigned int hkpExtendedMeshShape::getNextKey(unsigned int initialKey) const
{
    unsigned int subpartIndex = (initialKey & 0x7FFFFFFF) >> (32 - m_numBitsForSubpartIndex);
    int terminalIndex = (int)(initialKey & (0xFFFFFFFFu >> m_numBitsForSubpartIndex));
    unsigned int subpartType = initialKey & 0x80000000;
    hkpShapeBuffer buffer;
    while (true)
    {
        terminalIndex++;
        if (subpartType == 0)
        {
            if (terminalIndex >= m_trianglesSubparts[subpartIndex].m_numTriangleShapes)
            {
                subpartIndex++;
                terminalIndex = 0;
                if (subpartIndex >= (unsigned int)m_trianglesSubparts.getSize())
                {
                    if (m_shapesSubparts.getSize() == 0)
                    {
                        return kInvalidShapeKey;
                    }
                    subpartIndex = 0;
                    subpartType = 0x80000000;
                    terminalIndex = -1;
                    continue;
                }
            }
        }
        else if (terminalIndex >= m_shapesSubparts[subpartIndex].m_childShapes.getSize())
        {
            subpartIndex++;
            if (subpartIndex >= (unsigned int)m_shapesSubparts.getSize())
            {
                return kInvalidShapeKey;
            }
            terminalIndex = 0;
        }

        const unsigned int key = (subpartIndex << (32 - m_numBitsForSubpartIndex)) | subpartType | (unsigned int)terminalIndex;
        const hkpShape* child = getChildShape(key, buffer);
        if (child->m_type.m_storage != hkcdShapeType::TRIANGLE || !isDegenerateTriangle(child))
        {
            return key;
        }
    }
}

// 0x00D93D90
// A triangle key builds an hkpTriangleShape in the buffer (vertices under the subpart transform, every other triangle
// flipped when the subpart says so, the mesh's radius and welding). A shape key gives the child itself, or a
// translate/transform shape around it in the buffer.
const hkpShape* hkpExtendedMeshShape::getChildShape(unsigned int key, hkpShapeBufferStorage& buffer) const
{
    const int subpartIndex = getSubPartIndex(key);
    const int terminalIndex = getTerminalIndexInSubPart(key);
    if ((int)key >= 0)
    {
        const TrianglesSubpart& part = m_trianglesSubparts.getSize() == 1 ? m_embeddedTrianglesSubpart : m_trianglesSubparts[subpartIndex];

        // the alternate flip swaps the second and third index of odd triangles
        const char* indices = static_cast<const char*>(part.m_indexBase) + part.m_indexStriding * terminalIndex;
        const unsigned short flip = (unsigned short)((short)part.m_flipAlternateTriangles & terminalIndex);
        const short second = (short)(flip + 1);
        const short third = (short)((flip ^ 1) + 1);
        unsigned int i0, i1, i2;
        switch (part.m_stridingType.m_storage)
        {
        case INDICES_INT8:
        {
            const unsigned char* idx = reinterpret_cast<const unsigned char*>(indices);
            i0 = idx[0]; i1 = idx[second]; i2 = idx[third];
            break;
        }
        case INDICES_INT16:
        {
            const unsigned short* idx = reinterpret_cast<const unsigned short*>(indices);
            i0 = idx[0]; i1 = idx[second]; i2 = idx[third];
            break;
        }
        case INDICES_INT32:
        {
            const unsigned int* idx = reinterpret_cast<const unsigned int*>(indices);
            i0 = idx[0]; i1 = idx[second]; i2 = idx[third];
            break;
        }
        default:
            i0 = i1 = i2 = 0;
            break;
        }

        const unsigned short weldingInfo = m_weldingInfo.getSize() ? m_weldingInfo[part.m_triangleOffset + terminalIndex] : 0;
        hkpTriangleShape* triangle = new (static_cast<void*>(buffer.m_storage)) hkpTriangleShape(m_triangleRadius, weldingInfo, (hkpWeldingUtility::WeldingType)m_weldingType.m_storage);
        triangle->m_userData = part.m_userData;
        triangle->m_isExtruded = part.m_extrusion.lengthSquared<3>().isGreaterZero() ? 1 : 0;
        triangle->m_extrusion = part.m_extrusion;

        const char* vertices = reinterpret_cast<const char*>(part.m_vertexBase);
        const float* v0 = reinterpret_cast<const float*>(vertices + i0 * part.m_vertexStriding);
        const float* v1 = reinterpret_cast<const float*>(vertices + i1 * part.m_vertexStriding);
        const float* v2 = reinterpret_cast<const float*>(vertices + i2 * part.m_vertexStriding);
        hkVector4 a; a.set(v0[0], v0[1], v0[2], 0.0f);
        hkVector4 b; b.set(v1[0], v1[1], v1[2], 0.0f);
        hkVector4 c; c.set(v2[0], v2[1], v2[2], 0.0f);
        triangle->m_vertexA._setTransformedPos(part.m_transform, a);
        triangle->m_vertexB._setTransformedPos(part.m_transform, b);
        triangle->m_vertexC._setTransformedPos(part.m_transform, c);
        return triangle;
    }

    const ShapesSubpart& part = m_shapesSubparts[subpartIndex];
    const hkpConvexShape* child = part.m_childShapes[terminalIndex];
    const int flags = part.getFlags();
    if (flags == ShapesSubpart::FLAG_NONE)
    {
        return child;
    }
    if (flags == ShapesSubpart::FLAG_TRANSLATE_SET)
    {
        // (the translate shape drops the flags in w)
        return new (static_cast<void*>(buffer.m_storage)) hkpConvexTranslateShape(child, part.m_translation, hkpShapeContainer::REFERENCE_POLICY_IGNORE);
    }
    hkTransform transform;
    transform.m_rotation.set(part.m_rotation);
    transform.m_translation = part.m_translation;
    return new (static_cast<void*>(buffer.m_storage)) hkpConvexTransformShape(child, transform, hkpShapeContainer::REFERENCE_POLICY_IGNORE);
}

// ---- welding

// 0x00D94510
// The triangle subparts get their offsets into the welding info, which gets one zero per triangle; without welding
// it is freed.
void hkpExtendedMeshShape::initWeldingInfo(hkpWeldingUtility::WeldingType weldingType)
{
    m_weldingType.m_storage = (unsigned char)weldingType;
    if (weldingType != hkpWeldingUtility::WELDING_TYPE_NONE)
    {
        int numTriangles = 0;
        for (int i = 0; i < m_trianglesSubparts.getSize(); i++)
        {
            m_trianglesSubparts[i].m_triangleOffset = numTriangles;
            numTriangles += m_trianglesSubparts[i].m_numTriangleShapes;
        }
        m_weldingInfo.reserveExactly(numTriangles);
        m_weldingInfo.setSize(numTriangles, 0);
    }
    else
    {
        m_weldingInfo.clearAndDeallocate();
    }
}

// 0x00D93C70: the welding info of a triangle (the array is set up on first use; shape keys are ignored)
void hkpExtendedMeshShape::setWeldingInfo(unsigned int key, short weldingInfo)
{
    if (m_weldingInfo.getSize() == 0)
    {
        int numTriangles = 0;
        for (int i = 0; i < m_trianglesSubparts.getSize(); i++)
        {
            m_trianglesSubparts[i].m_triangleOffset = numTriangles;
            numTriangles += m_trianglesSubparts[i].m_numTriangleShapes;
        }
        m_weldingInfo.reserveExactly(numTriangles);
        m_weldingInfo.setSize(numTriangles, 0);
    }
    if ((int)key >= 0)
    {
        m_weldingInfo[getTerminalIndexInSubPart(key) + m_trianglesSubparts[getSubPartIndex(key)].m_triangleOffset] = weldingInfo;
    }
}
