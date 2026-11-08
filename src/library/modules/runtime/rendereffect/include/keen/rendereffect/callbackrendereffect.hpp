// Original: library/modules/runtime/rendereffect/include/keen/rendereffect/callbackrendereffect.hpp
// Implementation: library/modules/runtime/rendereffect/sources/global/callbackrendereffect.cpp
//
// CallBackRenderEffect, a render effect that calls back into user code for each render object. Placement of the context
// type inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    class GraphicsCommandWriter;
    class MemoryAllocator;
    struct RenderCommands;
    struct RenderContext;
    struct RenderEffectSystem;

    // size 0x1
    struct CallBackRenderEffectContext
    {
    };

    namespace CallBackRenderEffect
    {
        CallBackRenderEffectContext* createContext(RenderEffectSystem* pRenderEffectSystem, MemoryAllocator* pAllocator); // 0x00CCADA0 (:19)
        void destroyContext(CallBackRenderEffectContext* pEffectContext, RenderEffectSystem* pRenderEffectSystem, MemoryAllocator* pAllocator); // 0x00CCAE00 (:28)
        uint32 getEffectId(); // 0x00CC8340 (:34)
        void executeRenderCommands(GraphicsCommandWriter* pCommandWriter, const RenderCommands& commands, const RenderContext& renderContext, void*); // 0x00CC8350 (:49)
    }
}
