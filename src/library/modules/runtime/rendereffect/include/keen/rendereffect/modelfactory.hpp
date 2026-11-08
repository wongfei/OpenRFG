// Original: library/modules/runtime/rendereffect/include/keen/rendereffect/modelfactory.hpp
// Implementation: library/modules/runtime/rendereffect/sources/global/modelfactory.cpp
//
// ModelFactory, the ResourceFactory for models.
#pragma once

#include "keen/base/types.hpp"
#include "keen/resources/resourcefactory.hpp"

namespace keen
{
    struct GraphicsSystem;
    class MemoryAllocator;
    class Resource;

    // size 0xC
    class ModelFactory : public ResourceFactory
    {
    public:
        void create(GraphicsSystem* pGraphicsSystem); // 0x00CC8860 (:12)
        void destroy(); // 0x00CC8870 (:18)
        virtual uint32 getType() const; // vtbl+0x04 0x00CC8890 (:24)
        virtual Resource* createResource(MemoryAllocator* pAllocator, uint32 key); // vtbl+0x08 0x00CC9550 (:29)
        virtual void destroyResource(MemoryAllocator* pAllocator, Resource* pResource); // vtbl+0x0C 0x00CCC0F0 (:64)
        virtual bool initializeResource(Resource* pResource, uint32 contentFormatKey, const void* pInitializationData, uint32 initializationDataSize); // vtbl+0x14 0x00CC9590 (:34)
        virtual void shutdownResource(Resource* pResource); // vtbl+0x1C 0x00CC95D0 (:58)

    private:
        GraphicsSystem* m_pGraphicsSystem; // 0x08
    };
}
