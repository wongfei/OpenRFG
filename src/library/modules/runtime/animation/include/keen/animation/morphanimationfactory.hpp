// Original: library/modules/runtime/animation/include/keen/animation/morphanimationfactory.hpp
// Implementation: library/modules/runtime/animation/sources/global/morphanimationfactory.cpp
//
// MorphAnimationFactory, the ResourceFactory for morph animations.
#pragma once

#include "keen/base/types.hpp"
#include "keen/resources/resourcefactory.hpp"

namespace keen
{
    class MemoryAllocator;
    class Resource;

    // size 0x8
    class MorphAnimationFactory : public ResourceFactory
    {
    public:
        void create(); // 0x00CBE820 (:8)
        void destroy(); // 0x00CBE830 (:13)
        virtual uint32 getType() const; // vtbl+0x04 0x00CBE840 (:18)
        virtual Resource* createResource(MemoryAllocator* pAllocator, uint32 key); // vtbl+0x08 0x00CC7340 (:23)
        virtual bool initializeResource(Resource* pResource, uint32 contentFormatKey, const void* pInitializationData, uint32 initializationDataSize); // vtbl+0x14 0x00CBE850 (:28)
        virtual void shutdownResource(Resource* pResource); // vtbl+0x1C 0x00CBE870 (:45)
        virtual void destroyResource(MemoryAllocator* pAllocator, Resource* pResource); // vtbl+0x0C 0x00CC8080 (:51)

    };
}
