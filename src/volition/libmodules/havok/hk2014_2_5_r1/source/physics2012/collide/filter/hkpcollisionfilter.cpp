// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.cpp
//
// Havok 2014.2.5 (havok_impl.md HK4): the collision filter base (the four filter interfaces), from rfg.exe. The
// interfaces' constructors and destructors are inline in their SDK headers; they are defined here.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollidablecollidablefilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpraycollidablefilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpshapecollectionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkprayshapecollectionfilter.h"

// ---- the interfaces (inline in the SDK headers)

// 0x00D7ECD0
hkpCollidableCollidableFilter::hkpCollidableCollidableFilter()
{
}

// 0x0080CF30 (.h:25)
hkpCollidableCollidableFilter::~hkpCollidableCollidableFilter()
{
}

// 0x00D7ECE0
hkpShapeCollectionFilter::hkpShapeCollectionFilter()
{
}

// 0x0080CF90 (.h:67)
hkpShapeCollectionFilter::~hkpShapeCollectionFilter()
{
}

// 0x00D7ECF0
hkpRayShapeCollectionFilter::hkpRayShapeCollectionFilter()
{
}

// 0x0080CFC0 (.h:36)
hkpRayShapeCollectionFilter::~hkpRayShapeCollectionFilter()
{
}

// 0x00D7ED00
hkpRayCollidableFilter::hkpRayCollidableFilter()
{
}

// 0x0080CF60 (.h:26)
hkpRayCollidableFilter::~hkpRayCollidableFilter()
{
}

// ---- hkpCollisionFilter

// 0x00D92FE0
hkpCollisionFilter::hkpCollisionFilter()
{
    m_type.m_storage = HK_FILTER_UNKNOWN;
}

// 0x00D7EDC0 (.h): the loaded members stay.
hkpCollisionFilter::hkpCollisionFilter(hkFinishLoadedObjectFlag flag)
    : hkReferencedObject(flag)
{
}

// 0x0080CFF0
hkpCollisionFilter::~hkpCollisionFilter()
{
}

// 0x00D92FD0: no limit (the caller keeps the hits it has room for).
int hkpCollisionFilter::numShapeKeyHitsLimitBreached(const hkpCollisionInput& input, const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpBvTreeShape* bvTreeShapeB, hkAabb& aabb, unsigned int* shapeKeysInOut, int shapeKeysCapacity) const
{
    return shapeKeysCapacity;
}

// 0x0081BE10 (.h:57)
void hkpCollisionFilter::init(hkpWorld* world)
{
}
