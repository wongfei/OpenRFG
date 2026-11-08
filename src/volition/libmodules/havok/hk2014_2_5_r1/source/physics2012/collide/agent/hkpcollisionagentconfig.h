// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionagentconfig.h
//
// Havok 2014.2.5: layout from rfg.pdb, inline constructor (hkpcollisionagentconfig.inl) from the binary (havok_impl.md
// HK6). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"

// The iterative linear cast's parameters (hkpIterativeLinearCastAgent).
// size 0x10
struct alignas(16) hkpCollisionAgentConfig
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpCollisionAgentConfig); // new 0x00DEA1F0, delete 0x00DEA210

    // 0x00DE86A0
    hkpCollisionAgentConfig()
    {
        m_iterativeLinearCastEarlyOutDistance = 0.01f;
        m_iterativeLinearCastMaxIterations = 20;
    }

    float m_iterativeLinearCastEarlyOutDistance; // 0x00
    int m_iterativeLinearCastMaxIterations; // 0x04
};
