// Original: library/modules/runtime/graphicsbase/include/keen/graphics/viewport.hpp
// Implementation: library/modules/runtime/graphicsbase/sources/global/viewport.cpp
//
// Viewport, a rectangle in pixels plus a depth range.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x18
    struct Viewport
    {
        Viewport(); // 0x00CE3BA0 (:8)
        void set(uint32 inX, uint32 inY, uint32 inWidth, uint32 inHeight, float inMinDepth, float inMaxDepth); // 0x00CE3BD0 (:18)
        void setOrigin(uint32 inX, uint32 inY); // 0x00CE3C10 (:28)
        void setSize(uint32 inWidth, uint32 inHeight); // 0x00CE3C20 (:34)
        void setDepthRange(float inMinDepth, float inMaxDepth); // 0x00CE3C40 (:40)

        uint32 x; // 0x00
        uint32 y; // 0x04
        uint32 width; // 0x08
        uint32 height; // 0x0C
        float minDepth; // 0x10
        float maxDepth; // 0x14
    };
}
