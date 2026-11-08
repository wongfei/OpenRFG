// Original: library/modules/runtime/resources/include/keen/resources/blobresourcefactory.hpp
// Implementation: library/modules/runtime/resources/sources/blobresourcefactory.cpp
//
// BlobResourceFactory, the ResourceFactory for blob resources.
#pragma once

#include "keen/base/types.hpp"
#include "keen/resources/resourcefactory.hpp"

namespace keen
{
    class MemoryAllocator;
    class Resource;

    // size 0x8
    class BlobResourceFactory : public ResourceFactory
    {
    public:
        virtual uint32 getType() const; // vtbl+0x04 0x00CDC420 (:9)
        virtual Resource* createResource(MemoryAllocator* pAllocator, uint32 key); // vtbl+0x08 0x00CDD6E0 (:14)
        virtual void destroyResource(MemoryAllocator* pAllocator, Resource* pResource); // vtbl+0x0C 0x00CDF260 (:19)
        virtual void shutdownResource(Resource* pResource); // vtbl+0x1C 0x00CDC430 (:25)
        virtual bool initializeResource(Resource* pResource, uint32 contentFormatKey, const void* pInitializationData, uint32 initializationDataSize); // vtbl+0x14 0x00CDC440 (:30)
        BlobResourceFactory(); // 0x00CDD720
        virtual ~BlobResourceFactory(); // vtbl+0x00 0x00CDD740

    };
}
