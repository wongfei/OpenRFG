// Original: library/modules/runtime/graphicsbase/sources/global/viewport.cpp

#include "keen/graphics/viewport.hpp"

namespace keen
{
    // 0x00CE3BA0 (:8)
    Viewport::Viewport()
    {
        minDepth = 0.0f;
        x = 0;
        y = 0;
        width = 0;
        height = 0;
        maxDepth = 1.0f;
    }

    // 0x00CE3BD0 (:18)
    void Viewport::set(uint32 inX, uint32 inY, uint32 inWidth, uint32 inHeight, float inMinDepth, float inMaxDepth)
    {
        x = inX;
        y = inY;
        minDepth = inMinDepth;
        width = inWidth;
        height = inHeight;
        maxDepth = inMaxDepth;
    }

    // 0x00CE3C10 (:28)
    void Viewport::setOrigin(uint32 inX, uint32 inY)
    {
        x = inX;
        y = inY;
    }

    // 0x00CE3C20 (:34)
    void Viewport::setSize(uint32 inWidth, uint32 inHeight)
    {
        width = inWidth;
        height = inHeight;
    }

    // 0x00CE3C40 (:40)
    void Viewport::setDepthRange(float inMinDepth, float inMaxDepth)
    {
        minDepth = inMinDepth;
        maxDepth = inMaxDepth;
    }
}
