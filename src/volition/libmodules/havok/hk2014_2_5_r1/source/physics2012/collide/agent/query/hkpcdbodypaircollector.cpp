// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdbodypaircollector.inl
//
// Havok 2014.2.5 (havok_impl.md HK5): the out-of-line copies of the body pair collector base's inline members (the
// original has no .cpp; rfg.exe keeps these copies).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdbodypaircollector.h"

// 0x00710ED0 (hkpcdbodypaircollector.inl:16)
hkpCdBodyPairCollector::hkpCdBodyPairCollector()
{
    reset();
}

// 0x00710EE0 (hkpcdbodypaircollector.inl:21)
hkpCdBodyPairCollector::~hkpCdBodyPairCollector()
{
}

// 0x00710EC0 (hkpcdbodypaircollector.inl:11)
void hkpCdBodyPairCollector::reset()
{
    m_earlyOut.m_bool = 0;
}

// 0x00D78400
hkBool hkpCdBodyPairCollector::getEarlyOut() const
{
    return m_earlyOut;
}
