// Original: library/modules/runtime/graphics/sources/global/graphicssystembase.hpp
//
// GraphicsSystemBase, the platform independent part of GraphicsSystem: shader factory, state object pools and stock
// objects.
#pragma once

#include "keen/base/types.hpp"
#include "library/modules/runtime/graphics/sources/global/graphicsstateobjectpool.hpp"
#include "library/modules/runtime/graphics/sources/global/stockobjects.hpp"

namespace keen
{
    class MemoryAllocator;
    struct ShaderFactory;

    // size 0x258
    struct GraphicsSystemBase
    {
        GraphicsSystemBase(); // 0x00C6EF40 (.h:15)
        ~GraphicsSystemBase(); // 0x00C6D9C0

        ShaderFactory* pShaderFactory; // 0x00
        MemoryAllocator* pSystemAllocator; // 0x04
        CombinedGraphicsStateObjectPool stateObjectPool; // 0x08
        StockObjects stockObjects; // 0x200
    };
}
