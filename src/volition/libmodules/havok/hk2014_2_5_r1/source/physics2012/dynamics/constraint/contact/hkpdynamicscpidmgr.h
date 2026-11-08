// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpdynamicscpidmgr.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"

// size 0x14
class hkpDynamicsCpIdMgr
{
public:
    enum
    {
        HK_MAX_IDS_PER_OBJECT = 8,
    };

    enum
    {
        FREE_VALUE = 255,
    };

    typedef unsigned char hkpValueType;

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpDynamicsCpIdMgr);
    hkpDynamicsCpIdMgr(); // 0x00E30350
    ~hkpDynamicsCpIdMgr(); // 0x00E1B790
    int newId(int value); // 0x00E301F0
    int indexOf(int) const;
    void freeId(int id); // 0x00E1B290
    void decrementValuesGreater(int relIndex); // 0x00E1B2A0
    void getAllUsedIds(hkArray<unsigned short,hkContainerHeapAllocator>& ids) const; // 0x00E1B6F0
    unsigned char getValueAt(int id) const; // 0x00E1B2D0

    hkInplaceArray<unsigned char,8,hkContainerHeapAllocator> m_values; // 0x00
};
