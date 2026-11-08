// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/raycollector/hkpallrayhitcollector.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldraycastoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkprayhitcollector.h"

class hkpCdBody;
struct hkpShapeRayCastCollectorOutput;

// size 0x320
class hkpAllRayHitCollector : public hkpRayHitCollector
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpAllRayHitCollector); // delete(void* p, unsigned int nbytes) 0x00815BB0, new[](unsigned int, void* p) 0x0080D850
    hkpAllRayHitCollector(); // 0x00831840 (hkpallrayhitcollector.inl:18)
    void reset(); // 0x00822470 (hkpallrayhitcollector.inl:12)
    virtual ~hkpAllRayHitCollector(); // vtbl+0x04 0x00831960 (hkpallrayhitcollector.inl:23)
    const hkArray<hkpWorldRayCastOutput,hkContainerHeapAllocator>& getHits() const; // 0x0080D860 (hkpallrayhitcollector.inl:28)
    void sortHits(); // 0x00D974B0
public:
    virtual void addRayHit(const hkpCdBody& cdBody, const hkpShapeRayCastCollectorOutput& hitInfo); // vtbl+0x00 0x00D973F0

    alignas(16) hkInplaceArray<hkpWorldRayCastOutput,8,hkContainerHeapAllocator> m_hits; // 0x10
};
