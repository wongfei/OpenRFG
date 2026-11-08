// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkpagentnntrack.h
//
// Havok 2014.2.5: layouts from rfg.pdb, inline code ported from the 2014.1 SDK header (havok_impl.md HK6).
// Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/hkpagent3.h"

class hkpCollidable;
class hkpContactMgr;
class hkpLinkedCollidable;

// 32-bit float build of the SDK (HK_POINTER_SIZE 4, no double reals)
#define HK_AGENT3_MIDPHASE_AGENT_SIZE 64
#define HK_AGENT3_NARROWPHASE_AGENT_SIZE 128
#define HK_AGENT3_MAX_AGENT_SIZE 128
#define HK_AGENT3_SECTOR_SIZE 512

#define HK_AGENT3_FEWEST_AGENTS_PER_1N_SECTOR ((HK_AGENT3_SECTOR_SIZE - 16) / HK_AGENT3_MAX_AGENT_SIZE)
#define HK_AGENT3_MIDPHASE_AGENTS_PER_NN_SECTOR (HK_AGENT3_SECTOR_SIZE / HK_AGENT3_MIDPHASE_AGENT_SIZE)
#define HK_AGENT3_NARROWPHASE_AGENTS_PER_NN_SECTOR (HK_AGENT3_SECTOR_SIZE / HK_AGENT3_NARROWPHASE_AGENT_SIZE)

enum hkpAgentNnTrackType
{
    HK_AGENT3_INVALID_TRACK = 0,
    HK_AGENT3_MIDPHASE_TRACK = 1, // collisions which need midphase processing
    HK_AGENT3_NARROWPHASE_TRACK = 2, // convex-convex collisions only
};

// size 0x18
struct hkpAgentNnEntry : public hkpAgentEntry
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpAgentNnEntry);

    // 0x00DDC9E0
    hkpCollidable* getCollidableA()
    {
        return reinterpret_cast<hkpCollidable*>(m_collidable[0]);
    }

    // 0x00DDC9F0
    hkpCollidable* getCollidableB()
    {
        return reinterpret_cast<hkpCollidable*>(m_collidable[1]);
    }

    unsigned short m_agentIndexOnCollidable[2]; // 0x04
    hkpContactMgr* m_contactMgr; // 0x08
    unsigned char m_collisionQualityIndex; // 0x0C
    unsigned char m_forceCollideOntoPpu; // 0x0D (only used with an SPU)
    hkEnum<hkpAgentNnTrackType, unsigned char> m_nnTrackType; // 0x0E
    unsigned char m_padding; // 0x0F
    hkpLinkedCollidable* m_collidable[2]; // 0x10
};

// size 0x200
struct hkpAgentNnSector
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpAgentNnSector);

    // 0x00DDCA00
    hkpAgentNnEntry* getBegin()
    {
        return reinterpret_cast<hkpAgentNnEntry*>(&m_data[0]);
    }

    // 0x00DDCA10
    hkpAgentNnEntry* getEnd()
    {
        return reinterpret_cast<hkpAgentNnEntry*>(&m_data[HK_AGENT3_SECTOR_SIZE]);
    }

    unsigned char m_data[HK_AGENT3_SECTOR_SIZE]; // 0x00
};

// size 0x14
struct hkpAgentNnTrack
{
    // 0x00E0B5C0
    hkpAgentNnTrack(hkpAgentNnTrackType nnTrackType)
    {
        m_bytesUsedInLastSector = HK_AGENT3_SECTOR_SIZE;
        m_nnTrackType.m_storage = (unsigned char)nnTrackType;
    }

    // 0x00DDCCC0
    int getSectorSize(int sectorIndex) const
    {
        if (sectorIndex + 1 == m_sectors.getSize())
        {
            return m_bytesUsedInLastSector;
        }
        return HK_AGENT3_SECTOR_SIZE;
    }

    // 0x00DDCA20: works because HK_AGENT3_MIDPHASE_AGENT_SIZE * 2 == HK_AGENT3_NARROWPHASE_AGENT_SIZE
    static unsigned int getAgentSize(hkpAgentNnTrackType nnTrackType)
    {
        return nnTrackType * HK_AGENT3_MIDPHASE_AGENT_SIZE;
    }

    // 0x00DDCCE0
    unsigned int getAgentSize() const
    {
        return getAgentSize((hkpAgentNnTrackType)m_nnTrackType.m_storage);
    }

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpAgentNnTrack);
    // ~hkpAgentNnTrack 0x00E0B630: implicit (the sector array frees its storage)

    unsigned short m_bytesUsedInLastSector; // 0x00
    hkEnum<enum hkpAgentNnTrackType, unsigned char> m_nnTrackType; // 0x02
    unsigned char m_padding; // 0x03
    hkInplaceArray<hkpAgentNnSector*, 1, hkContainerHeapAllocator> m_sectors; // 0x04
};
