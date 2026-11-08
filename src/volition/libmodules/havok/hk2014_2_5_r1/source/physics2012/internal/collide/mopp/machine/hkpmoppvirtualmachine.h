// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/machine/hkpmoppvirtualmachine.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0), with hkpMoppPrimitiveInfo (HK3). Hand-maintained
// (nobuild in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"

struct hkContainerHeapAllocator;

// size 0x4: a key the query found
class hkpMoppPrimitiveInfo
{
public:
    unsigned int ID; // 0x00
};

// size 0x10
class hkpMoppVirtualMachine
{
public:
    typedef int hkpMoppFixedPoint;

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpMoppVirtualMachine);
    hkpMoppVirtualMachine(); // 0x00D99F60
    ~hkpMoppVirtualMachine(); // 0x00D99F70
    int getNumHits() const; // 0x00D9A5B0
public:
    void addHit(unsigned int id, const unsigned int* properties); // 0x00E688A0
    void initQuery(hkArray<hkpMoppPrimitiveInfo,hkContainerHeapAllocator>* primitives_out); // 0x00E67B90
    static int toIntMin(float x); // 0x00E68710
    static int toIntMax(float x); // 0x00E68720
    static int read24(const unsigned char* PC); // 0x00E6B830

public:
    hkArray<hkpMoppPrimitiveInfo,hkContainerHeapAllocator>* m_primitives_out; // 0x00
    int padding[3]; // 0x04
};
