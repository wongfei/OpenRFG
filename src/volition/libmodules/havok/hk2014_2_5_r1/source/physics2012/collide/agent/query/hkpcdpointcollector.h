// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdpointcollector.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"

struct hkpCdPoint;

// size 0x8
class hkpCdPointCollector
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpCdPointCollector); // delete(void* p, unsigned int nbytes) 0x00745140
    hkpCdPointCollector(); // 0x007422F0 (hkpcdpointcollector.inl:16)
    virtual ~hkpCdPointCollector(); // vtbl+0x00 0x00742310 (hkpcdpointcollector.inl:21)
    virtual void addCdPoint(const hkpCdPoint&) = 0; // vtbl+0x04
    virtual void reset(); // vtbl+0x08 0x007422E0 (hkpcdpointcollector.inl:11)
    float getEarlyOutDistance() const; // 0x00D783B0

    // 0x00 vfptr
public:
    float m_earlyOutDistance; // 0x04
};
