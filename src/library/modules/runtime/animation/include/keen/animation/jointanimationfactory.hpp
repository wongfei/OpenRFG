// Original: library/modules/runtime/animation/include/keen/animation/jointanimationfactory.hpp
// Implementation: library/modules/runtime/animation/sources/global/jointanimationfactory.cpp
//
// JointAnimationFactory, the ResourceFactory for joint animations.
#pragma once

#include "keen/base/types.hpp"
#include "keen/resources/resourcefactory.hpp"

namespace keen
{
    class MemoryAllocator;
    class Resource;

    // size 0x8
    class JointAnimationFactory : public ResourceFactory
    {
    public:
        void create(); // 0x00CBE620 (:9)
        void destroy(); // 0x00CBE630 (:14)
        virtual uint32 getType() const; // vtbl+0x04 0x00CBE640 (:19)
        virtual Resource* createResource(MemoryAllocator* pAllocator, uint32 key); // vtbl+0x08 0x00CC66C0 (:24)
        virtual bool initializeResource(Resource* pResource, uint32 contentFormatKey, const void* pInitializationData, uint32 initializationDataSize); // vtbl+0x14 0x00CBE650 (:29)
        virtual void shutdownResource(Resource* pResource); // vtbl+0x1C 0x00CBE670 (:46)
        virtual void destroyResource(MemoryAllocator* pAllocator, Resource* pResource); // vtbl+0x0C 0x00CC8040 (:52)

    };
}
