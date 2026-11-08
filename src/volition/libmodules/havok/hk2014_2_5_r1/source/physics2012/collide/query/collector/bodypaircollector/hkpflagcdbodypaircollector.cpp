// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/bodypaircollector/hkpflagcdbodypaircollector.cpp
//
// Havok 2014.2.5 (havok_impl.md HK5): the body pair collector that only records that a pair was found (in the early-out
// flag), with the out-of-line copies of its inline members (hkpFlagCdBodyPairCollector.inl in Havok 2014.1).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/bodypaircollector/hkpflagcdbodypaircollector.h"

// 0x00DB71B0 (hkpflagcdbodypaircollector.inl:11)
hkpFlagCdBodyPairCollector::hkpFlagCdBodyPairCollector()
{
}

// 0x00DB71F0 (hkpflagcdbodypaircollector.inl:16)
hkpFlagCdBodyPairCollector::~hkpFlagCdBodyPairCollector()
{
}

// 0x00DB7200
hkBool hkpFlagCdBodyPairCollector::hasHit() const
{
    return m_earlyOut;
}

// 0x00DD1BA0
void hkpFlagCdBodyPairCollector::addCdBodyPair(const hkpCdBody& bodyA, const hkpCdBody& bodyB)
{
    m_earlyOut.m_bool = 1;
}
