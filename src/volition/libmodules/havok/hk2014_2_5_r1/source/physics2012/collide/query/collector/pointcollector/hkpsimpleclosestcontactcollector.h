// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkpsimpleclosestcontactcollector.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdpointcollector.h"

struct hkpCdPoint;

// size 0x30
class hkpSimpleClosestContactCollector : public hkpCdPointCollector
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpSimpleClosestContactCollector); // delete(void* p, unsigned int nbytes) 0x00DB75A0
    hkpSimpleClosestContactCollector(); // 0x00DB7560
    virtual ~hkpSimpleClosestContactCollector(); // vtbl+0x00 0x00DB7210
    virtual void reset(); // vtbl+0x08 0x00DB7530
    hkBool hasHit() const; // 0x00DB7060
    const hkContactPoint& getHitContact() const;
public:
    virtual void addCdPoint(const hkpCdPoint& event); // vtbl+0x04 0x00DD1BB0

    hkBool m_hasHit; // 0x08
    hkContactPoint m_hitPoint; // 0x10
};
