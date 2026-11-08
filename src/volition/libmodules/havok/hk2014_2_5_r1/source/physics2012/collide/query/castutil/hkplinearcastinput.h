// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkplinearcastinput.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"

// size 0x20
struct alignas(16) hkpLinearCastInput
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpLinearCastInput);
    hkpLinearCastInput(); // 0x0080CC40 (.h:28)

    hkVector4f m_to; // 0x00
    float m_maxExtraPenetration; // 0x10
    float m_startPointTolerance; // 0x14
};
