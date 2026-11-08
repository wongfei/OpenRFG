// Original: library/modules/runtime/rendereffect/include/keen/rendereffect/rendereffectsystem.hpp
// Implementation: library/modules/runtime/rendereffect/sources/global/rendereffectsystem.cpp
//
// RenderEffectSystem, the registry of render effects: each slot holds the effect's command creation function and
// context (RenderEffectSetupData). rendereffect::createRenderCommands dispatches render objects to their effect by the
// material's effect id. Placement of the slot types inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/array.hpp"

namespace keen
{
    class GraphicsCommandWriter;
    class MemoryAllocator;
    class RenderCommandList;
    struct RenderCommands;
    struct RenderContext;
    struct RenderObject;

    // size 0xC
    struct RenderEffectSetupData
    {
        uint32 materialDataFormatCrc; // 0x00
        void (*pCreateCommandsFunc)(RenderCommandList*, const RenderObject*, uint32, const RenderContext&, void*); // 0x04
        void* pEffectContextData; // 0x08
    };

    // size 0x14
    struct RenderEffectSlot
    {
        uint32 effectId; // 0x00
        uint32 refCount; // 0x04
        RenderEffectSetupData data; // 0x08
    };

    // size 0x10
    struct RenderEffectSystem
    {
        RenderEffectSystem(); // 0x00CC9680 (:27)
        ~RenderEffectSystem(); // 0x00CC8DD0

        Array<RenderEffectSlot> effectSlots; // 0x00
        uint32 fallbackRenderEffectId; // 0x08
        MemoryAllocator* pAllocator; // 0x0C
    };

    namespace rendereffect
    {
        bool registerRenderEffect(RenderEffectSystem* pRenderEffectSystem, uint32 effectId, uint32 materialDataFormatCrc, void (*pCreateCommandsFunc)(RenderCommandList*, const RenderObject*, uint32, const RenderContext&, void*), void* pEffectContextData); // 0x00CCAD40 (.h:47)
        RenderEffectSystem* createRenderEffectSystem(uint32 renderEffectCapacity, MemoryAllocator* pAllocator); // 0x00CCBC20 (:48)
        void destroyRenderEffectSystem(RenderEffectSystem* pRenderEffectSystem); // 0x00CCC270 (:69)
        void setFallbackRenderEffect(RenderEffectSystem* pRenderEffectSystem, uint32 effectId); // 0x00CC9690 (:76)
        bool isFallbackEffect(const RenderEffectSystem* pRenderEffectSystem, uint32 effectId); // 0x00CC9730 (:104)
        bool registerRenderEffectEx(RenderEffectSystem* pRenderEffectSystem, uint32 effectId, const RenderEffectSetupData& setupData); // 0x00CC9750 (:109)
        void unregisterRenderEffect(RenderEffectSystem* pRenderEffectSystem, uint32 effectId); // 0x00CC97A0 (:128)
        bool isRenderEffectRegistered(const RenderEffectSystem* pRenderEffectSystem, uint32 effectId); // 0x00CC9800 (:142)
        void createRenderCommands(RenderCommandList* pCommandList, const RenderObject* pFirstRenderObject, uint32 renderObjectCount, const RenderEffectSystem* pRenderEffectSystem, const RenderContext& renderContext); // 0x00CC9820 (:147)
        void executeRenderCommands(GraphicsCommandWriter* pCommandWriter, const RenderEffectSystem* pRenderEffectSystem, const RenderContext& renderContext, const RenderCommands& commands); // 0x00CC98A0 (:194)
        uint32 getRenderEffectMaterialFormat(const RenderEffectSystem* pRenderEffectSystem, uint32 effectId); // 0x00CC9A60 (:265)
    }
}
