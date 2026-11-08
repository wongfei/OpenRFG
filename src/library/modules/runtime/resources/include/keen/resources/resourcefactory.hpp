// Original: library/modules/runtime/resources/include/keen/resources/resourcefactory.hpp
// Implementation: library/modules/runtime/resources/sources/global/resourcefactory.cpp
//
// ResourceFactory, the interface that creates, initializes and destroys the resources of one type (textures, models,
// materials, ...). Factories link themselves into a global registry.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/slice.hpp"

namespace keen
{
    class MemoryAllocator;
    class Resource;

    // size 0x8
    class ResourceFactory
    {
    public:
        ResourceFactory(); // 0x00CDCC70 (:33)
        virtual ~ResourceFactory(); // vtbl+0x00 0x00CDCC90 (:44)
        void create(); // 0x00CDCCA0 (:49)
        void destroy(); // 0x00CDCCB0 (:57)
        virtual uint32 getType() const = 0; // vtbl+0x04
        virtual Resource* createResource(MemoryAllocator*, uint32) = 0; // vtbl+0x08
        virtual void destroyResource(MemoryAllocator*, Resource*) = 0; // vtbl+0x0C
        virtual bool unlockAllResources(uint32 contextId); // vtbl+0x10 0x00C32B40 (.h:41)
        virtual bool initializeResource(Resource*, uint32, const void*, uint32) = 0; // vtbl+0x14
        virtual void postInitializeResource(Resource* pResource); // vtbl+0x18 0x00CDCCC0 (:65)
        virtual void shutdownResource(Resource*) = 0; // vtbl+0x1C
        virtual bool createEmergencyResource(Resource* pResource, const char* pFileName); // vtbl+0x20 0x00C32B50 (.h:48)
        static bool closeResourceFactoryRegistry(Slice<ResourceFactory*>* pFactories); // 0x00CDE470 (:11)

        // 0x00 vfptr
    protected:
        ResourceFactory* m_pNextRegisteredFactory; // 0x04
        static bool s_resourceFactoryRegistryOpen; // 0x016F2784
        static ResourceFactory* s_pFirstResourceFactory; // 0x03A260B4
    };
}
