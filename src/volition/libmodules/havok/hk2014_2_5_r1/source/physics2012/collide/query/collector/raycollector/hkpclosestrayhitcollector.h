// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/raycollector/hkpclosestrayhitcollector.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldraycastoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkprayhitcollector.h"

class hkBool;
class hkpCdBody;
struct hkpShapeRayCastCollectorOutput;

// size 0x70
class hkpClosestRayHitCollector : public hkpRayHitCollector
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpClosestRayHitCollector); // delete(void* p, unsigned int nbytes) 0x00815BE0, new[](unsigned int, void* p) 0x0080D870
    hkpClosestRayHitCollector(); // 0x0080D8B0 (hkpclosestrayhitcollector.inl:16)
    void reset(); // 0x0080D880 (hkpclosestrayhitcollector.inl:10)
    virtual ~hkpClosestRayHitCollector(); // vtbl+0x04 0x0080D8E0 (hkpclosestrayhitcollector.inl:21)
    hkBool hasHit() const; // 0x0080D8F0 (hkpclosestrayhitcollector.inl:25)
    const hkpWorldRayCastOutput& getHit() const; // 0x0080D900 (hkpclosestrayhitcollector.inl:30)
public:
    virtual void addRayHit(const hkpCdBody& cdBody, const hkpShapeRayCastCollectorOutput& hitInfo); // vtbl+0x00 0x00D7FC60

    alignas(16) hkpWorldRayCastOutput m_rayHit; // 0x10
};
