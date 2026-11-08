// Original: library/modules/runtime/resources/include/keen/resources/fallbackresource.hpp
// Implementation: library/modules/runtime/resources/sources/fallbackresource.cpp
//
// FallbackResource and its factory, used for resources of unknown type (raw data and format).
#pragma once

#include "keen/base/types.hpp"
#include "keen/resources/resource.hpp"
#include "keen/resources/resourcefactory.hpp"

namespace keen
{
    class MemoryAllocator;

    // size 0x10
    class FallbackResource : public Resource
    {
    public:
        typedef Resource BaseClass;

        FallbackResource(uint32 key); // 0x00CDD770 (.h:18)
        ~FallbackResource(); // 0x00CDD790 (.h:19)
        const void* getData() const;

    private:
        const void* m_pData; // 0x08
        uint32 m_format; // 0x0C
    };

    // size 0x8
    class FallbackResourceFactory : public ResourceFactory
    {
    public:
        virtual uint32 getType() const; // vtbl+0x04 0x00CDCCF0 (.h:35)
        virtual Resource* createResource(MemoryAllocator* pAllocator, uint32 key); // vtbl+0x08 0x00CDD7A0 (.h:37)
        virtual void destroyResource(MemoryAllocator* pAllocator, Resource* pResource); // vtbl+0x0C 0x00CDF280 (.h:41)
        virtual bool initializeResource(Resource* pResource, uint32 contentFormatKey, const void* pInitializationData, uint32 initializationDataSize); // vtbl+0x14 0x00CDCD00 (.h:47)
        virtual void shutdownResource(Resource*); // vtbl+0x1C 0x00CDCD30 (.h:63)
        FallbackResourceFactory(); // 0x00CDCCD0
        virtual ~FallbackResourceFactory(); // vtbl+0x00 0x00CDCD40

    };
}
