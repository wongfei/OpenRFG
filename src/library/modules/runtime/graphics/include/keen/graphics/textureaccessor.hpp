// Original: library/modules/runtime/graphics/include/keen/graphics/textureaccessor.hpp
//
// LockedTextureRectangle, a CPU view of a locked texture rectangle. Placement inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x10
    struct LockedTextureRectangle
    {
        void* pData; // 0x00
        uint32 rowPitch; // 0x04
        uint32 width; // 0x08
        uint32 height; // 0x0C
    };
}
