// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/hkpshapecollection.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): the generic shape collection (a shape and a container of child shapes), whose
// queries visit every child through the container interface, from rfg.exe. The shipped build records "rcShpCollect"
// and other monitor timers (TLS hkMonitorStream) around them; they are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/hkpshapecollection.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapebuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkprayshapecollectionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastoutput.h"

// 0x00D80B20
hkpShapeCollection::hkpShapeCollection(hkcdShapeType::ShapeTypeEnum type, CollectionType subType)
    : hkpShape(type)
{
    m_disableWelding = false;
    m_collectionType.m_storage = (unsigned char)subType;
}

// 0x00D81030
hkpShapeCollection::hkpShapeCollection(hkFinishLoadedObjectFlag flag)
    : hkpShape(flag)
{
    if (flag.m_finishing)
    {
        afterReflectNew();
    }
}

// 0x0080CAF0
hkpShapeCollection::~hkpShapeCollection()
{
}

// 0x00D80B60: a collection loaded on its own is a user collection
void hkpShapeCollection::afterReflectNew()
{
    m_type.m_storage = hkcdShapeType::COLLECTION;
    m_collectionType.m_storage = COLLECTION_USER;
}

// 0x008118F0 (.h:104)
bool hkpShapeCollection::isWeldingEnabled() const
{
    return !m_disableWelding;
}

// 0x00D80AF0
const hkpShapeContainer* hkpShapeCollection::getContainer() const
{
    return this;
}

// 0x00D80B10
void hkpShapeCollection::initWeldingInfo(hkpWeldingUtility::WeldingType weldingType)
{
}

// 0x00D80B00
void hkpShapeCollection::setWeldingInfo(unsigned int key, short weldingInfo)
{
}

// 0x00D80CE0: the union of the children's boxes
void hkpShapeCollection::getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const
{
    out.setEmpty();
    hkpShapeBuffer shapeBuffer;
    for (unsigned int key = getFirstKey(); key != 0xFFFFFFFF; key = getNextKey(key))
    {
        const hkpShape* child = getChildShape(key, shapeBuffer);
        hkAabb childAabb;
        child->getAabb(localToWorld, tolerance, childAabb);
        out.includeAabb(childAabb);
    }
}

// 0x00D80DF0
float hkpShapeCollection::getMaximumProjection(const hkVector4f& direction) const
{
    float result = -HK_REAL_MAX;
    hkpShapeBuffer shapeBuffer;
    for (unsigned int key = getFirstKey(); key != 0xFFFFFFFF; key = getNextKey(key))
    {
        const hkpShape* child = getChildShape(key, shapeBuffer);
        const float p = child->getMaximumProjection(direction);
        result = result > p ? result : p; // maxss: a NaN projection replaces the result
    }
    return result;
}

// 0x00D80B70
// Every (filtered) child is cast one key level down; the last child that hits gives the key.
hkBool hkpShapeCollection::castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const
{
    unsigned int bestKey = 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
    results.changeLevel(1);
    hkpShapeBuffer shapeBuffer;
    if (!input.m_rayShapeCollectionFilter)
    {
        for (unsigned int key = getFirstKey(); key != 0xFFFFFFFF; key = getNextKey(key))
        {
            const hkpShape* child = getChildShape(key, shapeBuffer);
            if (child->castRay(input, results))
            {
                bestKey = key;
            }
        }
    }
    else
    {
        for (unsigned int key = getFirstKey(); key != 0xFFFFFFFF; key = getNextKey(key))
        {
            if (input.m_rayShapeCollectionFilter->isCollisionEnabled(input, *this, key))
            {
                const hkpShape* child = getChildShape(key, shapeBuffer);
                if (child->castRay(input, results))
                {
                    bestKey = key;
                }
            }
        }
    }
    results.changeLevel(-1);
    if (bestKey != 0xFFFFFFFF)
    {
        results.setKey(bestKey);
    }
    return bestKey != 0xFFFFFFFF;
}

// 0x00D80EE0: every (filtered) child with its key, under the collection's body
void hkpShapeCollection::castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const
{
    hkpShapeBuffer shapeBuffer;
    if (!input.m_rayShapeCollectionFilter)
    {
        for (unsigned int key = getFirstKey(); key != 0xFFFFFFFF; key = getNextKey(key))
        {
            const hkpShape* child = getChildShape(key, shapeBuffer);
            hkpCdBody childBody(&cdBody);
            childBody.setShape(child, key);
            child->castRayWithCollector(input, childBody, collector);
        }
    }
    else
    {
        for (unsigned int key = getFirstKey(); key != 0xFFFFFFFF; key = getNextKey(key))
        {
            if (input.m_rayShapeCollectionFilter->isCollisionEnabled(input, *this, key))
            {
                const hkpShape* child = getChildShape(key, shapeBuffer);
                hkpCdBody childBody(&cdBody);
                childBody.setShape(child, key);
                child->castRayWithCollector(input, childBody, collector);
            }
        }
    }
}
