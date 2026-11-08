// Original: library/modules/runtime/rendereffect/sources/global/rendereffectsystem.cpp
//
// Partial (orig_main.md OM2): the creation and destruction GameFramework::initialize/shutdown make. Not ported: the
// effect registration and the render command dispatch (the Volition renderer registers no keen render effects).

#include "keen/rendereffect/rendereffectsystem.hpp"
#include "keen/base/memoryallocator.hpp"

#include <new>

namespace keen
{
    // 0x00CC9680 (:27)
    RenderEffectSystem::RenderEffectSystem()
    {
        fallbackRenderEffectId = 0;
        pAllocator = nullptr;
    }

    // 0x00CC8DD0
    RenderEffectSystem::~RenderEffectSystem()
    {
    }

    namespace rendereffect
    {
        // 0x00CCBC20 (:48)
        // Every slot starts as the fallback effect (0 here) with no setup data.
        // Original behaviour kept: a failed allocation of the system itself is not checked.
        RenderEffectSystem* createRenderEffectSystem(uint32 renderEffectCapacity, MemoryAllocator* pAllocator)
        {
            void* pMemory = pAllocator->allocate(sizeof(RenderEffectSystem), 4u, 0u, nullptr);
            RenderEffectSystem* pSystem = pMemory != nullptr ? new (pMemory) RenderEffectSystem() : nullptr;
            pSystem->fallbackRenderEffectId = 0;
            pSystem->pAllocator = pAllocator;
            pSystem->effectSlots.create(pAllocator, renderEffectCapacity, 0x10u);
            for (uint32 i = 0; i < pSystem->effectSlots.getSize(); ++i)
            {
                RenderEffectSlot& slot = pSystem->effectSlots[i];
                slot.effectId = pSystem->fallbackRenderEffectId;
                slot.refCount = 0;
                slot.data.materialDataFormatCrc = 0;
                slot.data.pCreateCommandsFunc = nullptr;
                slot.data.pEffectContextData = nullptr;
            }
            return pSystem;
        }

        // 0x00CCC270 (:69)
        void destroyRenderEffectSystem(RenderEffectSystem* pRenderEffectSystem)
        {
            pRenderEffectSystem->effectSlots.destroy(pRenderEffectSystem->pAllocator);
            pRenderEffectSystem->pAllocator->free(pRenderEffectSystem);
        }
    }
}
