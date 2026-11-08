// Original: library/modules/runtime/animation/include/keen/animation/lveanimationfactory.hpp
// Implementation: library/modules/runtime/animation/sources/global/lveanimationfactory.cpp
//
// LveAnimationFactory, the ResourceFactory for LVE animations.
#pragma once

#include "keen/base/types.hpp"
#include "keen/resources/resourcefactory.hpp"

namespace keen
{
    class MemoryAllocator;
    class Resource;

    // size 0x8
    class LveAnimationFactory : public ResourceFactory
    {
    public:
        void create(); // 0x00CBE750 (:9)
        void destroy(); // 0x00CBE760 (:14)
        virtual uint32 getType() const; // vtbl+0x04 0x00CBE770 (:19)
        virtual Resource* createResource(MemoryAllocator* pAllocator, uint32 key); // vtbl+0x08 0x00CC7100 (:24)
        virtual bool initializeResource(Resource* pResource, uint32 contentFormatKey, const void* pInitializationData, uint32 initializationDataSize); // vtbl+0x14 0x00CBE780 (:29)
        virtual void shutdownResource(Resource* pResource); // vtbl+0x1C 0x00CBE7A0 (:46)
        virtual void destroyResource(MemoryAllocator* pAllocator, Resource* pResource); // vtbl+0x0C 0x00CC8060 (:52)

    };
}
