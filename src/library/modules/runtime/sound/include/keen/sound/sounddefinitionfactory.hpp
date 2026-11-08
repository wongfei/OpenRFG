// Original: library/modules/runtime/sound/include/keen/sound/sounddefinitionfactory.hpp
// Implementation: library/modules/runtime/sound/sources/global/sounddefinitionfactory.cpp
//
// SoundDefinitionFactory, the ResourceFactory for sound definitions. Placement of the initialization data inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/resources/resourcefactory.hpp"

namespace keen
{
    namespace SoundSystem
    {
        struct SoundDefinition;
        struct System;
    }
}

namespace keen
{
    class MemoryAllocator;
    class Resource;

    // size 0xC
    class SoundDefinitionFactory : public ResourceFactory
    {
    public:
        SoundDefinitionFactory(); // 0x00C32E40 (:8)
        void create(SoundSystem::System* pSoundSystem); // 0x00C32E80 (:18)
        void destroy(); // 0x00C32EA0 (:25)
        virtual uint32 getType() const; // vtbl+0x04 0x00C32E70 (:13)
        virtual Resource* createResource(MemoryAllocator* pAllocator, uint32 key); // vtbl+0x08 0x00C359E0 (:32)
        virtual void destroyResource(MemoryAllocator* pAllocator, Resource* pResource); // vtbl+0x0C 0x00C37C50 (:49)
        virtual bool initializeResource(Resource* pResource, uint32 contentFormatKey, const void* pInitializationData, uint32 initializationDataSize); // vtbl+0x14 0x00C32EB0 (:37)
        virtual void shutdownResource(Resource* pResource); // vtbl+0x1C 0x00C32ED0 (:43)
        virtual bool unlockAllResources(uint32 contextId); // vtbl+0x10 0x00C37C70 (:55)
        virtual ~SoundDefinitionFactory(); // vtbl+0x00 0x00C32E60

    private:
        SoundSystem::System* m_pSoundSystem; // 0x08
    };

    // size 0x4
    struct SoundDefinitionInitializationData
    {
        const SoundSystem::SoundDefinition* pSoundDefinition; // 0x00
    };
}
