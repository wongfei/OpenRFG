// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1, HK6): the accessors of hkcontactpoint.inl, as out-of-line copies. The distance is
// the w component of the separating normal.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"

// 0x00710860 (.inl:10)
const hkVector4f& hkContactPoint::getPosition() const
{
    return m_position;
}

// 0x00710870 (.inl:15)
const hkVector4f& hkContactPoint::getNormal() const
{
    return m_separatingNormal;
}

// 0x00D79610
const hkVector4f& hkContactPoint::getSeparatingNormal() const
{
    return m_separatingNormal;
}

// 0x0080CAB0 (.inl:62)
float hkContactPoint::getDistance() const
{
    return m_separatingNormal.m_quad.m128_f32[3];
}

// 0x00710880 (.inl:72)
void hkContactPoint::setDistance(float d)
{
    m_separatingNormal.m_quad.m128_f32[3] = d;
}

// 0x0080CAA0 (.inl:30)
void hkContactPoint::setPosition(const hkVector4f& position)
{
    m_position = position;
}

// 0x00D79620
void hkContactPoint::setSeparatingNormal(const hkVector4f& separatingNormal)
{
    m_separatingNormal = separatingNormal;
}

// 0x00D8FA80
void hkContactPoint::setSeparatingNormal(const hkVector4f& normal, const hkSimdFloat32& dist)
{
    m_separatingNormal.setXYZ_W(normal, dist);
}

// 0x00B62D60 (.inl:42)
void hkContactPoint::setSeparatingNormal(const hkVector4f& normal, float dist)
{
    m_separatingNormal = normal;
    m_separatingNormal.m_quad.m128_f32[3] = dist;
}

// 0x00B62D80 (.inl:105)
void hkContactPoint::setNormalOnly(const hkVector4f& normal)
{
    m_separatingNormal.setXYZ(normal);
}

// 0x00D7ADD0
hkSimdFloat32 hkContactPoint::getDistanceSimdReal() const
{
    return m_separatingNormal.getComponent<3>();
}

// 0x0080CAC0 (.inl:80)
void hkContactPoint::setDistanceSimdReal(const hkSimdFloat32& newValue)
{
    m_separatingNormal.setW(newValue);
}

// 0x00F82DF0
void hkContactPoint::set(const hkVector4f& position, const hkVector4f& normal, float dist)
{
    m_position = position;
    m_separatingNormal = normal;
    m_separatingNormal.m_quad.m128_f32[3] = dist;
}

// 0x00DD1B70
void hkContactPoint::setPositionNormalAndDistance(const hkVector4f& position, const hkVector4f& normal, const hkSimdFloat32& dist)
{
    m_position = position;
    m_separatingNormal.setXYZ_W(normal, dist);
}

// 0x00D79740
// The point on the other body (moved along the normal by the distance, all four lanes), the normal reversed.
void hkContactPoint::setFlipped(const hkContactPoint& point)
{
    m_position.setAddMul(point.m_position, point.m_separatingNormal, point.m_separatingNormal.getComponent<3>());
    m_separatingNormal.setNeg<3>(point.m_separatingNormal);
}

// 0x00DCF8C0
void hkContactPoint::flip()
{
    setFlipped(*this);
}
