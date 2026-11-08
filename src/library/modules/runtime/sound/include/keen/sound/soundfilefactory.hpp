// Original: library/modules/runtime/sound/include/keen/sound/soundfilefactory.hpp
// Implementation: library/modules/runtime/sound/sources/global/soundfilefactory.cpp
//
// SoundFileFactory, the ResourceFactory for sound files.
#pragma once

#include "keen/base/types.hpp"
#include "keen/resources/resourcefactory.hpp"

namespace keen
{
    class MemoryAllocator;
    class Resource;

    // size 0x8
    class SoundFileFactory : public ResourceFactory
    {
    public:
        SoundFileFactory(); // 0x00C32F20 (:10)
        void create(); // 0x00C32F40 (:15)
        void destroy(); // 0x00C32F50 (:20)
        virtual uint32 getType() const; // vtbl+0x04 0x00C32F60 (:25)
        virtual Resource* createResource(MemoryAllocator* pAllocator, uint32 key); // vtbl+0x08 0x00C35A80 (:30)
        virtual void destroyResource(MemoryAllocator* pAllocator, Resource* pResource); // vtbl+0x0C 0x00C38F70 (:35)
        virtual bool initializeResource(Resource* pResource, uint32 contentFormatKey, const void* pInitializationData, uint32 initializationDataSize); // vtbl+0x14 0x00C35AC0 (:40)
        virtual void shutdownResource(Resource* pResource); // vtbl+0x1C 0x00C32F70 (:60)
        virtual ~SoundFileFactory(); // vtbl+0x00 0x0048B410

    };
}
