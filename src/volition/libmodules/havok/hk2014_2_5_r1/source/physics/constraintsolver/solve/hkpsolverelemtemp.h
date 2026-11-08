// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverelemtemp.h
//
// Havok 2014.2.5 (havok_impl.md HK8): one solver row's scratch value, from rfg.pdb and the Havok 2014.1 header.
// Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"

// size 0x4
struct hkpSolverElemTemp
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpSolverElemTemp);

    union
    {
        float m_impulseApplied; // 0x00
        float m_usedImpulseFactor; // 0x00
    };
};
