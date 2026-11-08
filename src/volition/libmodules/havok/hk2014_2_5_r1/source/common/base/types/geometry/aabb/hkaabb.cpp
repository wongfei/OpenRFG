// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): hkAabb::isValid, the only out-of-line hkAabb function. The rest is inline
// (hkaabb.inl).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"

// 0x00FBF6C0
bool hkAabb::isValid() const
{
    // no NaN in x, y, z of either corner, and min <= max
    if (!m_min.isOk<3>() || !m_max.isOk<3>())
    {
        return false;
    }
    hkVector4Comparison less = m_min.lessEqual(m_max);
    return less.allAreSet<hkVector4ComparisonMask::MASK_XYZ>() != 0;
}
