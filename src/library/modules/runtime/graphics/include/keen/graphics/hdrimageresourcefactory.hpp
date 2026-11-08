// Original: library/modules/runtime/graphics/include/keen/graphics/hdrimageresourcefactory.hpp
// Implementation: library/modules/runtime/graphics/sources/global/hdrimageresourcefactory.cpp
//
// HdrImageResourceFactory, the ResourceFactory for HdrImageResource. Placement of the initialization data inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/resources/resourcefactory.hpp"

namespace keen
{
    struct HdrImageResourceData;
    class MemoryAllocator;
    class Resource;

    // size 0x4
    struct HdrImageResourceInitializationData
    {
        const HdrImageResourceData* pData; // 0x00
    };

    // size 0x8
    class HdrImageResourceFactory : public ResourceFactory
    {
    public:
        virtual uint32 getType() const; // vtbl+0x04 0x00C5D190 (:8)
        virtual Resource* createResource(MemoryAllocator* pAllocator, uint32 key); // vtbl+0x08 0x00C64640 (:13)
        virtual bool initializeResource(Resource* pResource, uint32 contentFormatKey, const void* pInitializationData, uint32 initializationDataSize); // vtbl+0x14 0x00C5D1A0 (:18)
        virtual void shutdownResource(Resource* pResource); // vtbl+0x1C 0x00C5D1C0 (:29)
        virtual void destroyResource(MemoryAllocator* pAllocator, Resource* pResource); // vtbl+0x0C 0x00C6E2F0 (:35)

    };
}
