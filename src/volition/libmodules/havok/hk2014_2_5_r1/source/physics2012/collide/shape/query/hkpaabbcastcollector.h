// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpaabbcastcollector.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hksimdfloat32.h"

// size 0x20
struct hkpAabbCastCollector
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpAabbCastCollector); // delete(void* p, unsigned int nbytes) 0x00DB1A50
    hkpAabbCastCollector(); // 0x00DB18F0
    virtual void addHit(unsigned int) = 0; // vtbl+0x00
    virtual ~hkpAabbCastCollector(); // vtbl+0x04 0x00DB18D0

    // 0x00 vfptr
    hkSimdFloat32 m_earlyOutFraction; // 0x10
};
