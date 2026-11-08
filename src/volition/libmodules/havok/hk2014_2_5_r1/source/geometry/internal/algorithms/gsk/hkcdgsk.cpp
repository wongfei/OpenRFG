// Original: volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/algorithms/gsk/hkcdgsk.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): the out-of-line copies of the hkcdGsk query input constructors (inline in
// Havok's hkcdGsk.h). The GSK algorithms themselves come with the narrowphase (HK6).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/algorithms/gsk/hkcdgsk.h"

// 0x00D992F0: m_data = (radius A + B, (radius A + B + eps)^2, 0, 0); the rest is left alone
hkcdGsk::LinearCastInput::LinearCastInput(float convexRadiusA, float convexRadiusB, float eps)
{
    const float radius = convexRadiusA + convexRadiusB;
    const float radiusEps = radius + eps;
    m_data.set(radius, radiusEps * radiusEps, 0.0f, 0.0f);
}

// 0x00D99C20: a ray against one shape in its own space (m_aTb is the identity); m_from and m_direction are left alone
// (the base has no default constructor; m_data is set again as the original computes it, without the + 0 of the
// missing second radius)
hkcdGsk::RayCastInput::RayCastInput(float convexRadiusA, float eps)
    : LinearCastInput(convexRadiusA, 0.0f, eps)
{
    const float radiusEps = convexRadiusA + eps;
    m_data.set(convexRadiusA, radiusEps * radiusEps, 0.0f, 0.0f);
    m_flags = hkcdRayQueryFlags::NO_FLAGS;
    m_aTb.setIdentity();
}
