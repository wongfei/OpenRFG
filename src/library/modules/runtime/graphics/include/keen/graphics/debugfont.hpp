// Original: library/modules/runtime/graphics/include/keen/graphics/debugfont.hpp
// Implementation: library/modules/runtime/graphics/sources/global/debugfont.cpp
//
// The built-in debug font. Placement of DebugFontConfiguration inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0xC
    struct DebugFontConfiguration
    {
        DebugFontConfiguration();

        int characterWidth; // 0x00
        int characterHeight; // 0x04
        int baseLineOffset; // 0x08
    };
}
