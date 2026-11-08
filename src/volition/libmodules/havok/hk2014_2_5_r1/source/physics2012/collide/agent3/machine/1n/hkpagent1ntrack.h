// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1ntrack.h
//
// Havok 2014.2.5: layouts from rfg.pdb, inline code ported from the 2014.1 SDK header (havok_impl.md HK6).
// Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkClass;
struct hkInternalClassMember;

typedef void hkpAgentData;

// size 0x200
struct hkpAgent1nSector
{
    enum
    {
        SECTOR_SIZE = 512,
        NET_SECTOR_SIZE = 496, // SECTOR_SIZE - HK_REAL_ALIGNMENT
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpAgent1nSector); // new(unsigned int nbytes) 0x00DCCBA0, delete(void* p, unsigned int nbytes) 0x00DCCBC0
    static const hkClass& staticClass(); // 0x00D7DD90

    // 0x00DCB760
    hkpAgentData* getBegin()
    {
        return reinterpret_cast<hkpAgentData*>(&m_data[0]);
    }

    // 0x00DCB770
    hkpAgentData* getEnd()
    {
        return reinterpret_cast<hkpAgentData*>(&m_data[m_bytesAllocated]);
    }

    // 0x00DCB780
    hkpAgentData* getEndOfCapacity()
    {
        return reinterpret_cast<hkpAgentData*>(&m_data[NET_SECTOR_SIZE]);
    }

    // 0x00DCB790
    int getCapacity()
    {
        return NET_SECTOR_SIZE;
    }

    int getFreeBytes()
    {
        return static_cast<int>(NET_SECTOR_SIZE - m_bytesAllocated);
    }

    // 0x00DCB7A0
    hkpAgent1nSector()
        : m_bytesAllocated(0)
    {
    }

    // 0x00D7DDD0
    hkpAgent1nSector(hkFinishLoadedObjectFlag flag)
    {
    }

    static const hkInternalClassMember Members[];
    unsigned int m_bytesAllocated; // 0x00
    unsigned int m_pad0; // 0x04
    unsigned int m_pad1; // 0x08
    unsigned int m_pad2; // 0x0C
    unsigned char m_data[496]; // 0x10
};

// size 0xC
struct hkpAgent1nTrack
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpAgent1nTrack); // new(unsigned int n, void* p) 0x00DB70A0
    // hkpAgent1nTrack 0x00DB3BE0, ~hkpAgent1nTrack 0x00DB3BA0: implicit

    hkArray<hkpAgent1nSector*, hkContainerHeapAllocator> m_sectors; // 0x00
};
