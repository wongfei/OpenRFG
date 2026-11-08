// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkpclosestcdpointcollector.cpp
//
// Havok 2014.2.5 (havok_impl.md HK5): the point collector that keeps the closest point, with the out-of-line copies of
// its inline members (hkpClosestCdPointCollector.inl in Havok 2014.1).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkpclosestcdpointcollector.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdpoint.h"

// 0x00814910 (hkpclosestcdpointcollector.inl:17)
hkpClosestCdPointCollector::hkpClosestCdPointCollector()
{
    reset();
}

// 0x0080D530 (hkpclosestcdpointcollector.inl:22)
hkpClosestCdPointCollector::~hkpClosestCdPointCollector()
{
}

// 0x008148E0 (hkpclosestcdpointcollector.inl:10)
// No hit, at distance HK_REAL_MAX (the w of the separating normal); the rest of the hit point is left as it was.
void hkpClosestCdPointCollector::reset()
{
    m_hitPoint.m_rootCollidableA = nullptr;
    m_hitPoint.m_contact.m_separatingNormal.setComponent<3>(hkSimdFloat32::getConstant<HK_QUADREAL_MAX>());
    hkpCdPointCollector::reset();
}

// 0x0080D540 (hkpclosestcdpointcollector.inl:27)
hkBool hkpClosestCdPointCollector::hasHit() const
{
    return m_hitPoint.m_rootCollidableA != nullptr;
}

// 0x0080D550 (hkpclosestcdpointcollector.inl:32)
const hkpRootCdPoint& hkpClosestCdPointCollector::getHit() const
{
    return m_hitPoint;
}

// 0x00DB7040
const hkContactPoint& hkpClosestCdPointCollector::getHitContact() const
{
    return m_hitPoint.m_contact;
}

// 0x00DD12B0
const hkVector4f& hkpClosestCdPointCollector::getUnweldedNormal() const
{
    return m_unweldedNormal;
}

// 0x00D81130
// Keeps the point when there is no hit yet or it is strictly closer than the kept one; its distance becomes the
// early-out distance.
void hkpClosestCdPointCollector::addCdPoint(const hkpCdPoint& event)
{
    const float distance = event.m_contact.m_separatingNormal.m_quad.m128_f32[3];
    if (m_hitPoint.m_rootCollidableA == nullptr || distance < m_hitPoint.m_contact.m_separatingNormal.m_quad.m128_f32[3])
    {
        m_hitPoint.m_contact.m_position = event.m_contact.m_position;
        m_hitPoint.m_contact.m_separatingNormal = event.m_contact.m_separatingNormal;

        // hkpCdBody::getRootCollidable; the keys are the leaf bodies' keys
        const hkpCdBody* rootA = &event.m_cdBodyA;
        while (rootA->m_parent != nullptr)
        {
            rootA = rootA->m_parent;
        }
        m_hitPoint.m_rootCollidableA = (const hkpCollidable*)rootA;
        m_hitPoint.m_shapeKeyA = event.m_cdBodyA.m_shapeKey;

        const hkpCdBody* rootB = &event.m_cdBodyB;
        while (rootB->m_parent != nullptr)
        {
            rootB = rootB->m_parent;
        }
        m_hitPoint.m_rootCollidableB = (const hkpCollidable*)rootB;
        m_hitPoint.m_shapeKeyB = event.m_cdBodyB.m_shapeKey;

        m_earlyOutDistance = distance;
        m_unweldedNormal = event.m_unweldedNormal;
    }
}
