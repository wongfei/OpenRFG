// Original: library/modules/runtime/sound/include/keen/sound/soundbankfactory.hpp
// Implementation: library/modules/runtime/sound/sources/global/soundbankfactory.cpp
//
// SoundBankFactory, the ResourceFactory for sound banks.
#pragma once

#include "keen/base/types.hpp"
#include "keen/resources/resourcefactory.hpp"

namespace keen
{
    class MemoryAllocator;
    class Resource;

    // size 0x8
    class SoundBankFactory : public ResourceFactory
    {
    public:
        SoundBankFactory(); // 0x00C32B20 (:10)
        void create(); // 0x00C32B80 (:19)
        void destroy(); // 0x00C32B90 (:24)
        virtual uint32 getType() const; // vtbl+0x04 0x00C32B70 (:14)
        virtual Resource* createResource(MemoryAllocator* pAllocator, uint32 key); // vtbl+0x08 0x00C35960 (:29)
        virtual void destroyResource(MemoryAllocator* pAllocator, Resource* pResource); // vtbl+0x0C 0x00C37C30 (:51)
        virtual bool initializeResource(Resource* pResource, uint32 contentFormatKey, const void* pInitializationData, uint32 initializationDataSize); // vtbl+0x14 0x00C32BA0 (:34)
        virtual void shutdownResource(Resource* pResource); // vtbl+0x1C 0x00C32BC0 (:46)
        virtual ~SoundBankFactory(); // vtbl+0x00 0x00C32B60

    };
}
