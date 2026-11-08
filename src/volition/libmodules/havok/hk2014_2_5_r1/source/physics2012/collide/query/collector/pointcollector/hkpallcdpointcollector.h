// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkpallcdpointcollector.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkprootcdpoint.h"

class hkBool;
struct hkpCdPoint;

// size 0x1A0
class hkpAllCdPointCollector : public hkpCdPointCollector
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpAllCdPointCollector); // delete(void* p, unsigned int nbytes) 0x0074D4D0
    hkpAllCdPointCollector(); // 0x0074D4A0 (hkpallcdpointcollector.inl:16)
    virtual ~hkpAllCdPointCollector(); // vtbl+0x00 0x0074D500 (hkpallcdpointcollector.inl:21)
    virtual void addCdPoint(const hkpCdPoint& event); // vtbl+0x04 0x00D7B020
    hkArray<hkpRootCdPoint,hkContainerHeapAllocator>& getHits(); // 0x00742340 (hkpallcdpointcollector.inl:31)
    const hkArray<hkpRootCdPoint,hkContainerHeapAllocator>& getHits() const; // 0x0080CAE0 (hkpallcdpointcollector.inl:26)
    int getNumHits() const; // 0x00814130 (hkpallcdpointcollector.inl:36)
    hkBool hasHit() const; // 0x00814140 (hkpallcdpointcollector.inl:41)
    void sortHits(); // 0x00D7B0B0
    virtual void reset(); // vtbl+0x08 0x00747730 (hkpallcdpointcollector.inl:10)

public:
    alignas(16) hkInplaceArray<hkpRootCdPoint,8,hkContainerHeapAllocator> m_hits; // 0x10
};
