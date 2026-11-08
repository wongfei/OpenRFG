// Original: library/modules/runtime/sequence/include/keen/sequence/sequencefactory.hpp
// Implementation: library/modules/runtime/sequence/sources/global/sequencefactory.cpp
//
// SequenceFactory, the ResourceFactory for sequences. Placement of the initialization data inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/resources/resourcefactory.hpp"

namespace keen
{
    class MemoryAllocator;
    class Resource;
    struct SequenceData;

    // size 0x8
    class SequenceFactory : public ResourceFactory
    {
    public:
        void create(); // 0x00CE58D0 (:8)
        void destroy(); // 0x00CE58E0 (:13)
        virtual uint32 getType() const; // vtbl+0x04 0x00CE5900 (:45)
        virtual Resource* createResource(MemoryAllocator* pAllocator, uint32 key); // vtbl+0x08 0x00CE5A50 (:18)
        virtual void destroyResource(MemoryAllocator* pAllocator, Resource* pResource); // vtbl+0x0C 0x00CE5E20 (:23)
        virtual bool initializeResource(Resource* pResource, uint32 contentFormatKey, const void* pInitializationData, uint32 initializationDataSize); // vtbl+0x14 0x00CE5A90 (:30)
        virtual void shutdownResource(Resource* pResource); // vtbl+0x1C 0x00CE58F0 (:40)

    };

    // size 0x4
    struct SequenceInitializationData
    {
        enum
        {
            TypeSignatureCrc = 0x5FEC06B1,
        };

        static uint32 getAlignment();

        SequenceData* pSequenceData; // 0x00
    };
}
