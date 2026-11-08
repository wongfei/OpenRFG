// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdpointcollector.inl
//
// Havok 2014.2.5 (havok_impl.md HK5): the out-of-line copies of the point collector base's inline members (the
// original has no .cpp; rfg.exe keeps these copies).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdpointcollector.h"

// 0x007422F0 (hkpcdpointcollector.inl:16)
hkpCdPointCollector::hkpCdPointCollector()
{
    reset();
}

// 0x00742310 (hkpcdpointcollector.inl:21)
hkpCdPointCollector::~hkpCdPointCollector()
{
}

// 0x007422E0 (hkpcdpointcollector.inl:11)
void hkpCdPointCollector::reset()
{
    m_earlyOutDistance = 3.40282e38f; // HK_REAL_MAX (0x7F7FFFEE)
}

// 0x00D783B0
float hkpCdPointCollector::getEarlyOutDistance() const
{
    return m_earlyOutDistance;
}
