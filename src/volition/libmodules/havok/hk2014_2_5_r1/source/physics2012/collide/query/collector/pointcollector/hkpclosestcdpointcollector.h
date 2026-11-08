// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkpclosestcdpointcollector.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkprootcdpoint.h"

class hkBool;
class hkContactPoint;
struct hkpCdPoint;

// size 0x50
class hkpClosestCdPointCollector : public hkpCdPointCollector
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpClosestCdPointCollector); // delete(void* p, unsigned int nbytes) 0x00814950
    hkpClosestCdPointCollector(); // 0x00814910 (hkpclosestcdpointcollector.inl:17)
    virtual ~hkpClosestCdPointCollector(); // vtbl+0x00 0x0080D530 (hkpclosestcdpointcollector.inl:22)
    virtual void reset(); // vtbl+0x08 0x008148E0 (hkpclosestcdpointcollector.inl:10)
    hkBool hasHit() const; // 0x0080D540 (hkpclosestcdpointcollector.inl:27)
    const hkpRootCdPoint& getHit() const; // 0x0080D550 (hkpclosestcdpointcollector.inl:32)
    const hkContactPoint& getHitContact() const; // 0x00DB7040
    const hkVector4f& getUnweldedNormal() const; // 0x00DD12B0
public:
    virtual void addCdPoint(const hkpCdPoint& event); // vtbl+0x04 0x00D81130

    alignas(16) hkpRootCdPoint m_hitPoint; // 0x10
    hkVector4f m_unweldedNormal; // 0x40
};
