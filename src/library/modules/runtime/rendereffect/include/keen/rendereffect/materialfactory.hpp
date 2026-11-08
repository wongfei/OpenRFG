// Original: library/modules/runtime/rendereffect/include/keen/rendereffect/materialfactory.hpp
// Implementation: library/modules/runtime/rendereffect/sources/global/materialfactory.cpp
//
// MaterialFactory, the ResourceFactory for materials.
#pragma once

#include "keen/base/types.hpp"
#include "keen/resources/resourcefactory.hpp"

namespace keen
{
    class MemoryAllocator;
    struct RenderEffectSystem;
    class Resource;

    // size 0x10
    class MaterialFactory : public ResourceFactory
    {
    public:
        MaterialFactory();
        void create(RenderEffectSystem* pRenderEffectSystem); // 0x00CC85B0 (:26)
        void destroy(); // 0x00CC85D0 (:33)
        virtual uint32 getType() const; // vtbl+0x04 0x00CC85A0 (:21)
        virtual Resource* createResource(MemoryAllocator* pAllocator, uint32 key); // vtbl+0x08 0x00CC9300 (:39)
        virtual void destroyResource(MemoryAllocator* pAllocator, Resource* pResource); // vtbl+0x0C 0x00CCBB80 (:69)
        virtual bool initializeResource(Resource* pResource, uint32 contentFormatKey, const void* pInitializationData, uint32 initializationDataSize); // vtbl+0x14 0x00CC85E0 (:44)
        virtual void shutdownResource(Resource* pResource); // vtbl+0x1C 0x00CC8610 (:63)

    private:
        RenderEffectSystem* m_pRenderEffectSystem; // 0x08
        uint32 m_materialId; // 0x0C
    };
}
