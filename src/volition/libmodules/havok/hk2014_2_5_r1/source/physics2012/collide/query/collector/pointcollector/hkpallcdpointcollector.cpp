// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkpallcdpointcollector.cpp
//
// Havok 2014.2.5 (havok_impl.md HK5): the point collector that keeps every point, with the out-of-line copies of its
// inline members (hkpAllCdPointCollector.inl in Havok 2014.1); sortHits since P1 (player_minimal.md).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkpallcdpointcollector.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hksort.h"

// 0x0074D4A0 (hkpallcdpointcollector.inl:16)
hkpAllCdPointCollector::hkpAllCdPointCollector()
{
    reset();
}

// 0x0074D500 (hkpallcdpointcollector.inl:21)
hkpAllCdPointCollector::~hkpAllCdPointCollector()
{
}

// 0x00747730 (hkpallcdpointcollector.inl:10)
void hkpAllCdPointCollector::reset()
{
    m_hits.clear();
    hkpCdPointCollector::reset();
}

// 0x0080CAE0 (hkpallcdpointcollector.inl:26)
const hkArray<hkpRootCdPoint, hkContainerHeapAllocator>& hkpAllCdPointCollector::getHits() const
{
    return m_hits;
}

// 0x00742340 (hkpallcdpointcollector.inl:31)
hkArray<hkpRootCdPoint, hkContainerHeapAllocator>& hkpAllCdPointCollector::getHits()
{
    return m_hits;
}

// 0x00814130 (hkpallcdpointcollector.inl:36)
int hkpAllCdPointCollector::getNumHits() const
{
    return m_hits.getSize();
}

// 0x00814140 (hkpallcdpointcollector.inl:41)
hkBool hkpAllCdPointCollector::hasHit() const
{
    return m_hits.getSize() > 0;
}

// hkpRootCdPoint::operator< (hkprootcdpoint.inl), the order of hkAlgorithm::less<hkpRootCdPoint>: by distance. The
// instance (0x00D7AED0) tests with ucomiss/jc, so a NaN distance counts as less.
static inline bool operator<(const hkpRootCdPoint& a, const hkpRootCdPoint& b)
{
    return !(a.m_contact.m_separatingNormal.m_quad.m128_f32[3] >= b.m_contact.m_separatingNormal.m_quad.m128_f32[3]);
}

// 0x00D7B0B0 (player_minimal.md P1): closest first (quickSortRecursive instance 0x00D7AED0)
void hkpAllCdPointCollector::sortHits()
{
    const int size = m_hits.getSize();
    if (size > 1)
    {
        hkAlgorithm::quickSortRecursive(m_hits.begin(), 0, size - 1, hkAlgorithm::less<hkpRootCdPoint>());
    }
}

// 0x00D7B020
// The early-out distance is not lowered: every point is kept.
void hkpAllCdPointCollector::addCdPoint(const hkpCdPoint& event)
{
    hkpRootCdPoint& hit = m_hits.expandOne();
    hit.m_contact.m_position = event.m_contact.m_position;
    hit.m_contact.m_separatingNormal = event.m_contact.m_separatingNormal;

    // hkpCdBody::getRootCollidable; the keys are the leaf bodies' keys
    const hkpCdBody* rootA = &event.m_cdBodyA;
    while (rootA->m_parent != nullptr)
    {
        rootA = rootA->m_parent;
    }
    hit.m_rootCollidableA = (const hkpCollidable*)rootA;
    hit.m_shapeKeyA = event.m_cdBodyA.m_shapeKey;

    const hkpCdBody* rootB = &event.m_cdBodyB;
    while (rootB->m_parent != nullptr)
    {
        rootB = rootB->m_parent;
    }
    hit.m_rootCollidableB = (const hkpCollidable*)rootB;
    hit.m_shapeKeyB = event.m_cdBodyB.m_shapeKey;
}
