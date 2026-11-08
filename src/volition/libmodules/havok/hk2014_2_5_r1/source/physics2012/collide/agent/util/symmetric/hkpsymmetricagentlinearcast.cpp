// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagentlinearcast.cpp
//
// Havok 2014.2.5 (havok_impl.md HK5): the collectors the symmetric agents (hkpSymmetricAgent<>, HK6) wrap around the
// caller's collector to swap bodies A and B, from rfg.exe. The agents themselves come with HK6.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagentlinearcast.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdpoint.h"

// 0x00D78410 (.h:63)
hkpSymmetricAgentFlipCollector::hkpSymmetricAgentFlipCollector(hkpCdPointCollector& c)
    : m_collector(c)
{
}

// 0x00D78430
hkpSymmetricAgentFlipCollector::~hkpSymmetricAgentFlipCollector()
{
}

// 0x00D9CE60
// The point moves to the other body (position + normal * distance, all four lanes), the normal's xyz is negated (its
// distance kept) and is also the unwelded normal; the bodies swap. The early-out distance follows the wrapped
// collector.
void hkpSymmetricAgentFlipCollector::addCdPoint(const hkpCdPoint& point)
{
    hkpCdPoint flipped(point.m_cdBodyB, point.m_cdBodyA);
    const hkVector4f& normal = point.m_contact.m_separatingNormal;
    hkVector4f distance;
    distance.m_quad = _mm_shuffle_ps(normal.m_quad, normal.m_quad, _MM_SHUFFLE(3, 3, 3, 3));
    flipped.m_contact.m_position.m_quad = _mm_add_ps(_mm_mul_ps(distance.m_quad, normal.m_quad), point.m_contact.m_position.m_quad);
    flipped.m_contact.m_separatingNormal.setNeg<3>(normal);
    flipped.m_unweldedNormal = flipped.m_contact.m_separatingNormal;
    m_collector.addCdPoint(flipped);
    m_earlyOutDistance = m_collector.m_earlyOutDistance;
}

// 0x00D78440 (.h:75)
hkpSymmetricAgentFlipCastCollector::hkpSymmetricAgentFlipCastCollector(const hkVector4f& path, hkpCdPointCollector& c)
    : m_path(path), m_collector(c)
{
}

// 0x00D78480
hkpSymmetricAgentFlipCastCollector::~hkpSymmetricAgentFlipCastCollector()
{
}

// 0x00D9CEE0
// As hkpSymmetricAgentFlipCollector, but the point moves along the cast path by the hit fraction (the normal's w):
// position + path * fraction.
void hkpSymmetricAgentFlipCastCollector::addCdPoint(const hkpCdPoint& point)
{
    hkpCdPoint flipped(point.m_cdBodyB, point.m_cdBodyA);
    const hkVector4f& normal = point.m_contact.m_separatingNormal;
    hkVector4f fraction;
    fraction.m_quad = _mm_shuffle_ps(normal.m_quad, normal.m_quad, _MM_SHUFFLE(3, 3, 3, 3));
    flipped.m_contact.m_position.m_quad = _mm_add_ps(_mm_mul_ps(fraction.m_quad, m_path.m_quad), point.m_contact.m_position.m_quad);
    flipped.m_contact.m_separatingNormal.setNeg<3>(normal);
    flipped.m_unweldedNormal = flipped.m_contact.m_separatingNormal;
    m_collector.addCdPoint(flipped);
    m_earlyOutDistance = m_collector.m_earlyOutDistance;
}

// 0x00D78490 (.h:85)
hkpSymmetricAgentFlipBodyCollector::hkpSymmetricAgentFlipBodyCollector(hkpCdBodyPairCollector& c)
    : m_collector(c)
{
}

// 0x00D784B0
hkpSymmetricAgentFlipBodyCollector::~hkpSymmetricAgentFlipBodyCollector()
{
}

// 0x00D9CE30
void hkpSymmetricAgentFlipBodyCollector::addCdBodyPair(const hkpCdBody& bodyA, const hkpCdBody& bodyB)
{
    m_collector.addCdBodyPair(bodyB, bodyA);
    m_earlyOut = m_collector.m_earlyOut;
}
