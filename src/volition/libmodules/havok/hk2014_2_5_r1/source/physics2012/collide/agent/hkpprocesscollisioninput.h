// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0); inline members of the 2014.1 SDK added by hand
// (HK6). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/hkstepinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"

struct hkpAgent1nSector;
struct hkpCollisionAgentConfig;
struct hkpCollisionQualityInfo;

// size 0x80
struct alignas(16) hkpProcessCollisionInput : public hkpCollisionInput
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpProcessCollisionInput); // new(unsigned int nbytes) 0x00DEA160, delete(void* p, unsigned int nbytes) 0x00DEA180
    // 0x00E03B80
    hkpProcessCollisionInput(const hkpProcessCollisionInput& other)
        : hkpCollisionInput(other), m_spareAgentSector(nullptr)
    {
        *this = other;
    }

    // 0x00DEA1B0
    hkpProcessCollisionInput()
        : m_spareAgentSector(nullptr)
    {
    }

    // 0x00DEA1C0: frees the spare 1n agent sector (hkpAgent1nSector::SECTOR_SIZE bytes)
    ~hkpProcessCollisionInput()
    {
        if (m_spareAgentSector != nullptr)
        {
            hkMemoryRouter::getInstance().heap().blockFree(m_spareAgentSector, 512);
        }
    }

    // 0x00DF9D40: everything but m_spareAgentSector
    hkpProcessCollisionInput& operator=(const hkpProcessCollisionInput& other)
    {
        (hkpCollisionInput&)*this = (const hkpCollisionInput&)other;
        m_stepInfo = other.m_stepInfo;
        m_collisionQualityInfo = other.m_collisionQualityInfo;
        m_dynamicsInfo = other.m_dynamicsInfo;
        m_enableDeprecatedWelding = other.m_enableDeprecatedWelding;
        m_allowToSkipConfirmedCallbacks = other.m_allowToSkipConfirmedCallbacks;
        m_config = other.m_config;
        return *this;
    }

    hkStepInfo m_stepInfo; // 0x50
    mutable hkPadSpu<hkpCollisionQualityInfo*> m_collisionQualityInfo; // 0x60
    mutable hkpAgent1nSector* m_spareAgentSector; // 0x64
    void* m_dynamicsInfo; // 0x68
    hkBool m_enableDeprecatedWelding; // 0x6C
    hkBool m_allowToSkipConfirmedCallbacks; // 0x6D
    hkpCollisionAgentConfig* m_config; // 0x70
};
