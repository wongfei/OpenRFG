// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpaabbcastcollector.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): the out-of-line copies of the inline members of the AABB cast collector
// interface (hkpAabbCastCollector.h).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpaabbcastcollector.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"

// 0x00DB18F0: no early out yet (a fraction of 1)
hkpAabbCastCollector::hkpAabbCastCollector()
{
    m_earlyOutFraction = hkSimdReal_1;
}

// 0x00DB18D0
hkpAabbCastCollector::~hkpAabbCastCollector()
{
}
