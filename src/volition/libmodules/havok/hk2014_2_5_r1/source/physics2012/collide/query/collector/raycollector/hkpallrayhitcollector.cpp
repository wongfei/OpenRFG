// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/raycollector/hkpallrayhitcollector.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): the ray hit collector that keeps every hit, with the out-of-line copies of its
// inline members (hkpAllRayHitCollector.inl in Havok 2014.1).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/raycollector/hkpallrayhitcollector.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"

#include <new>

// 0x00831840 (hkpallrayhitcollector.inl:18)
hkpAllRayHitCollector::hkpAllRayHitCollector()
{
    reset();
}

// 0x00822470 (hkpallrayhitcollector.inl:12)
void hkpAllRayHitCollector::reset()
{
    m_hits.clear();
    hkpRayHitCollector::reset();
}

// 0x00831960 (hkpallrayhitcollector.inl:23)
hkpAllRayHitCollector::~hkpAllRayHitCollector()
{
}

// 0x0080D860 (hkpallrayhitcollector.inl:28)
const hkArray<hkpWorldRayCastOutput, hkContainerHeapAllocator>& hkpAllRayHitCollector::getHits() const
{
    return m_hits;
}

// 0x00D973F0
void hkpAllRayHitCollector::addRayHit(const hkpCdBody& cdBody, const hkpShapeRayCastCollectorOutput& hitInfo)
{
    hkpWorldRayCastOutput& rayHit = *new (&m_hits.expandOne()) hkpWorldRayCastOutput();
    static_cast<hkpShapeRayCastCollectorOutput&>(rayHit) = hitInfo;
    hkpRayHitCollector::shapeKeysFromCdBody(rayHit.m_shapeKeys, hkpShapeRayCastOutput::MAX_HIERARCHY_DEPTH, cdBody);

    // hkpCdBody::getRootCollidable
    const hkpCdBody* root = &cdBody;
    while (root->m_parent != nullptr)
    {
        root = root->m_parent;
    }
    rayHit.m_rootCollidable = (const hkpCollidable*)root;
}
