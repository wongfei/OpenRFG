// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/list/hkplistshape.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): a list of child shapes (the key is the index) with a cached AABB and a bit field
// of enabled children (children from 256 on are always enabled), from rfg.exe. The shipped build records "rcList"
// monitor timers (TLS hkMonitorStream) around the ray casts; they are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/list/hkplistshape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapebuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkprayshapecollectionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastoutput.h"

// 0x00D80660
hkpListShape::hkpListShape(const hkpShape* const* shapeArray, int numShapes, hkpShapeContainer::ReferencePolicy ref)
    : hkpShapeCollection(hkcdShapeType::LIST, COLLECTION_LIST)
{
    m_childInfo.reserve(4);
    setShapes(shapeArray, numShapes, nullptr, ref);
    for (int i = 0; i < 8; i++)
    {
        m_enabledChildren[i] = 0xFFFFFFFF;
    }
    m_flags = ALL_FLAGS_CLEAR;
    m_numDisabledChildren = 0;
}

// 0x00D803D0
hkpListShape::hkpListShape(hkFinishLoadedObjectFlag flag)
    : hkpShapeCollection(flag)
    , m_childInfo(flag)
{
    if (flag.m_finishing)
    {
        afterReflectNew();
    }
}

// 0x00D80400: the children lose the list's references (whatever the reference policy was)
hkpListShape::~hkpListShape()
{
    for (int i = 0; i < m_childInfo.getSize(); i++)
    {
        m_childInfo[i].m_shape->removeReference();
    }
}

// 0x00D7FE80
void hkpListShape::afterReflectNew()
{
    m_type.m_storage = hkcdShapeType::LIST;
    m_collectionType.m_storage = COLLECTION_LIST;
}

// 0x00D804D0
// Null entries of shapeArray leave their child info as it was. Every child info gets the number of children.
void hkpListShape::setShapes(const hkpShape* const* shapeArray, int numShapes, const unsigned int* filterInfo, hkpShapeContainer::ReferencePolicy ref)
{
    m_childInfo.setSize(numShapes);
    for (int i = 0; i < numShapes; i++)
    {
        if (shapeArray[i])
        {
            ChildInfo& info = m_childInfo[i];
            info.m_shape = shapeArray[i];
            info.m_collisionFilterInfo = filterInfo ? filterInfo[i] : 0;
            info.m_numChildShapes = numShapes;
            info.m_shapeSize = 0;
            info.m_shapeInfo = 0;
        }
    }
    if (ref == hkpShapeContainer::REFERENCE_POLICY_INCREMENT)
    {
        hkReferencedObject::addReferences(reinterpret_cast<const hkReferencedObject* const*>(m_childInfo.begin()), m_childInfo.getSize(), sizeof(ChildInfo));
    }
    recalcAabbExtents();
}

// 0x00D7FFF0
// The children's local boxes (the first child's is the start, so a list needs one) give the cached centre and half
// extents.
void hkpListShape::recalcAabbExtents(hkAabb& aabb)
{
    m_childInfo[0].m_shape->getAabb(hkTransformf::getIdentity(), 0.0f, aabb);
    for (int i = 1; i < m_childInfo.getSize(); i++)
    {
        hkAabb childAabb;
        m_childInfo[i].m_shape->getAabb(hkTransformf::getIdentity(), 0.0f, childAabb);
        aabb.includeAabb(childAabb);
    }
    const hkSimdReal half = hkSimdReal_Inv2;
    m_aabbCenter.setAdd(aabb.m_max, aabb.m_min);
    m_aabbCenter.mul(half);
    m_aabbHalfExtents.setSub(aabb.m_max, aabb.m_min);
    m_aabbHalfExtents.mul(half);
}

// 0x00D803B0
void hkpListShape::recalcAabbExtents()
{
    hkAabb aabb;
    recalcAabbExtents(aabb);
}

// 0x00D805B0
// The cache is rebuilt when no half extent (w included) is positive; the box itself is the union of every child's
// (enabled or not), starting from half of HK_REAL_MAX.
void hkpListShape::getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const
{
    if (m_aabbHalfExtents.lessEqualZero().allAreSet())
    {
        const_cast<hkpListShape*>(this)->recalcAabbExtents();
    }
    hkVector4 high; high.setMul(hkVector4::getConstant<HK_QUADREAL_MAX>(), hkVector4::getConstant<HK_QUADREAL_INV_2>());
    out.m_min = high;
    out.m_max.setNeg<4>(high);
    for (int i = 0; i < m_childInfo.getSize(); i++)
    {
        hkAabb childAabb;
        m_childInfo[i].m_shape->getAabb(localToWorld, tolerance, childAabb);
        out.includeAabb(childAabb);
    }
}

// 0x00D7FDF0
// Too many children for the SPU midphase (when it would be used) or for the key bits: -1. Otherwise each child's size
// is cached (with the number of children) and the list's size returned.
int hkpListShape::calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const
{
    const int numChildren = m_childInfo.getSize();
    const bool spuMidphase = input.m_midphaseAgent3Registered && !input.m_isFixedOrKeyframed;
    if ((spuMidphase && numChildren > MAX_CHILDREN_FOR_SPU_MIDPHASE) || numChildren >= 0x7FF) // 0x7FF: 11 key bits
    {
        return -1;
    }
    for (int i = 0; i < numChildren; i++)
    {
        ChildInfo& info = const_cast<ChildInfo&>(m_childInfo[i]);
        info.m_shapeSize = (short)info.m_shape->calcSizeForSpu(input, 256); // 256: the SPU child shape buffer
        info.m_numChildShapes = m_childInfo.getSize();
        if (info.m_shapeSize == -1)
        {
            return -1;
        }
    }
    return sizeof(*this);
}

// ---- the container (hkpListShape.h/.inl)

// 0x0081BB40 (.h:145)
int hkpListShape::getNumChildShapes() const
{
    return m_childInfo.getSize() - m_numDisabledChildren;
}

// 0x00822490 (.h:148)
unsigned int hkpListShape::getFirstKey() const
{
    for (unsigned int key = 0; (int)key < m_childInfo.getSize(); key++)
    {
        if (isChildEnabled(key))
        {
            return key;
        }
    }
    return 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
}

// 0x0081BB50 (.h:152)
unsigned int hkpListShape::getNextKey(unsigned int key) const
{
    for (key++; (int)key < m_childInfo.getSize(); key++)
    {
        if (isChildEnabled(key))
        {
            return key;
        }
    }
    return 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
}

// 0x00D7FDC0
unsigned int hkpListShape::getCollisionFilterInfo(unsigned int key) const
{
    return m_childInfo[key].m_collisionFilterInfo;
}

// 0x00D7FDB0: the child itself; the buffer is not used
const hkpShape* hkpListShape::getChildShape(unsigned int key, hkpShapeBufferStorage& buffer) const
{
    return m_childInfo[key].m_shape;
}

// 0x00815F70 (.h:106)
const hkpShape* hkpListShape::getChildShapeInl(int i) const
{
    return m_childInfo[i].m_shape;
}

// 0x00E25B50
unsigned short hkpListShape::getShapeInfo(unsigned int shapeKey) const
{
    return m_childInfo[shapeKey].m_shapeInfo;
}

// 0x00D7FDD0
void hkpListShape::setCollisionFilterInfo(unsigned int index, unsigned int filterInfo)
{
    m_childInfo[index].m_collisionFilterInfo = filterInfo;
}

// 0x0081BAA0 (.h:128): only the first MAX_DISABLED_CHILDREN children have a bit
unsigned int hkpListShape::isChildEnabled(unsigned int index) const
{
    if (index >= MAX_DISABLED_CHILDREN)
    {
        return 1;
    }
    return (1u << (index & 31)) & m_enabledChildren[index >> 5];
}

// 0x00D7FCD0
void hkpListShape::disableChild(unsigned int index)
{
    const unsigned int bits = m_enabledChildren[index >> 5] & ~(1u << (index & 31));
    if (m_enabledChildren[index >> 5] != bits)
    {
        m_enabledChildren[index >> 5] = bits;
        m_numDisabledChildren++;
    }
}

// 0x00D7FD00
void hkpListShape::enableChild(unsigned int index)
{
    const unsigned int bits = m_enabledChildren[index >> 5] | (1u << (index & 31));
    if (m_enabledChildren[index >> 5] != bits)
    {
        m_enabledChildren[index >> 5] = bits;
        m_numDisabledChildren--;
    }
}

// ---- ray casts

// 0x00D80190
// Each enabled (and filtered) child is cast one key level down with its index as the key. The key level gets the best
// key; a miss puts the whole output back as it was (a child may have written to it without a hit).
hkBool hkpListShape::castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const
{
    const hkpShapeRayCastOutput original = results;
    unsigned int bestKey = 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
    for (int i = 0; i < m_childInfo.getSize(); i++)
    {
        if (isChildEnabled(i) && (!input.m_rayShapeCollectionFilter || input.m_rayShapeCollectionFilter->isCollisionEnabled(input, *this, i)))
        {
            results.setKey(i);
            results.changeLevel(1);
            if (m_childInfo[i].m_shape->castRay(input, results))
            {
                bestKey = i;
            }
            results.changeLevel(-1);
        }
    }
    results.setKey(bestKey);
    if (bestKey == 0xFFFFFFFF)
    {
        results = original;
    }
    return bestKey != 0xFFFFFFFF;
}

// 0x00D7FE90: each enabled (and filtered) child with its index, under the list's body
void hkpListShape::castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const
{
    for (int i = 0; i < m_childInfo.getSize(); i++)
    {
        if (isChildEnabled(i) && (!input.m_rayShapeCollectionFilter || input.m_rayShapeCollectionFilter->isCollisionEnabled(input, *this, i)))
        {
            const hkpShape* child = m_childInfo[i].m_shape;
            hkpCdBody childBody(&cdBody);
            childBody.setShape(child, i);
            child->castRayWithCollector(input, childBody, collector);
        }
    }
}
