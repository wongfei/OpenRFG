// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldraycastinput.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// size 0x30
struct hkpWorldRayCastInput
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpWorldRayCastInput); // new[](unsigned int, void* p) 0x00811420
    hkpWorldRayCastInput(); // 0x00811430 (.h:39)

    hkVector4f m_from; // 0x00
    hkVector4f m_to; // 0x10
    hkBool m_enableShapeCollectionFilter; // 0x20
    unsigned int m_filterInfo; // 0x24
    unsigned long m_userData; // 0x28
};
