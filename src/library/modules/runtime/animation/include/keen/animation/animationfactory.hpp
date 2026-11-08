// Original: library/modules/runtime/animation/include/keen/animation/animationfactory.hpp
// Implementation: library/modules/runtime/animation/sources/global/animationfactory.cpp
//
// AnimationFactory, the common ResourceFactory base of the animation factories.
#pragma once

#include "keen/base/types.hpp"
#include "keen/resources/resourcefactory.hpp"

namespace keen
{
    class MemoryAllocator;
    class Resource;

    // size 0x8
    class AnimationFactory : public ResourceFactory
    {
    public:
        void create(); // 0x00CBE110 (:9)
        void destroy(); // 0x00CBE120 (:14)
        virtual uint32 getType() const; // vtbl+0x04 0x00CBE130 (:19)
        virtual Resource* createResource(MemoryAllocator* pAllocator, uint32 key); // vtbl+0x08 0x00CC2900 (:24)
        virtual bool initializeResource(Resource* pResource, uint32 contentFormatKey, const void* pInitializationData, uint32 initializationDataSize); // vtbl+0x14 0x00CBE140 (:29)
        virtual void shutdownResource(Resource* pResource); // vtbl+0x1C 0x00CBE170 (:60)
        virtual void destroyResource(MemoryAllocator* pAllocator, Resource* pResource); // vtbl+0x0C 0x00CC8020 (:66)

    };
}
