// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkpsimpleclosestcontactcollector.cpp
//
// Havok 2014.2.5 (havok_impl.md HK5): the point collector that keeps the closest contact point only (no bodies), with
// the out-of-line copies of its inline members (hkpSimpleClosestContactCollector.inl in Havok 2014.1).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkpsimpleclosestcontactcollector.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdpoint.h"

// 0x00DB7560 (hkpsimpleclosestcontactcollector.inl:18)
hkpSimpleClosestContactCollector::hkpSimpleClosestContactCollector()
{
    reset();
}

// 0x00DB7210 (hkpsimpleclosestcontactcollector.inl:23)
hkpSimpleClosestContactCollector::~hkpSimpleClosestContactCollector()
{
}

// 0x00DB7530
void hkpSimpleClosestContactCollector::reset()
{
    m_hasHit.m_bool = 0;
    m_hitPoint.m_separatingNormal.setComponent<3>(hkSimdFloat32::getConstant<HK_QUADREAL_MAX>());
    hkpCdPointCollector::reset();
}

// 0x00DB7060
hkBool hkpSimpleClosestContactCollector::hasHit() const
{
    return m_hasHit;
}

// (inline, no out-of-line copy in rfg.exe)
const hkContactPoint& hkpSimpleClosestContactCollector::getHitContact() const
{
    return m_hitPoint;
}

// 0x00DD1BB0
void hkpSimpleClosestContactCollector::addCdPoint(const hkpCdPoint& event)
{
    const float distance = event.m_contact.m_separatingNormal.m_quad.m128_f32[3];
    if (!m_hasHit.m_bool || distance < m_hitPoint.m_separatingNormal.m_quad.m128_f32[3])
    {
        m_hasHit.m_bool = 1;
        m_hitPoint.m_position = event.m_contact.m_position;
        m_hitPoint.m_separatingNormal = event.m_contact.m_separatingNormal;
        m_earlyOutDistance = distance;
    }
}
