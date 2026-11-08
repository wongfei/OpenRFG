// Original: library/modules/runtime/sound/sources/global/sounddefinitionfactory.cpp
//
// SoundDefinitionFactory, the ResourceFactory of 'SDEF' sound definitions (SFX.md SX1). Unlocking a resource context
// stops the sounds of that tag.

#include "keen/sound/sounddefinitionfactory.hpp"
#include "keen/sound/sounddefinition.hpp"
#include "keen/sound/soundsystem.hpp"
#include "keen/base/memoryallocator.hpp"

#include <new>

namespace keen
{
    // 0x00C32E40 (:8)
    SoundDefinitionFactory::SoundDefinitionFactory()
    {
        m_pSoundSystem = nullptr;
    }

    // 0x00C32E60
    SoundDefinitionFactory::~SoundDefinitionFactory()
    {
    }

    // 0x00C32E70 (:13)
    uint32 SoundDefinitionFactory::getType() const
    {
        return SoundDefinitionResource::ResourceClassType;
    }

    // 0x00C32E80 (:18)
    void SoundDefinitionFactory::create(SoundSystem::System* pSoundSystem)
    {
        ResourceFactory::create();
        m_pSoundSystem = pSoundSystem;
    }

    // 0x00C32EA0 (:25)
    void SoundDefinitionFactory::destroy()
    {
        m_pSoundSystem = nullptr;
        ResourceFactory::destroy();
    }

    // 0x00C359E0 (:32)
    Resource* SoundDefinitionFactory::createResource(MemoryAllocator* pAllocator, uint32 key)
    {
        void* pMemory = pAllocator->allocate(sizeof(SoundDefinitionResource), 4, 0, nullptr);
        if (pMemory == nullptr)
        {
            return nullptr;
        }
        return new (pMemory) SoundDefinitionResource(key);
    }

    // 0x00C32EB0 (:37)
    bool SoundDefinitionFactory::initializeResource(Resource* pResource, uint32 contentFormatKey, const void* pInitializationData, uint32 initializationDataSize)
    {
        return static_cast<SoundDefinitionResource*>(pResource)->initialize((const SoundDefinitionInitializationData*)pInitializationData, initializationDataSize, contentFormatKey);
    }

    // 0x00C32ED0 (:43)
    void SoundDefinitionFactory::shutdownResource(Resource* pResource)
    {
        static_cast<SoundDefinitionResource*>(pResource)->destroy();
    }

    // 0x00C37C50 (:49)
    void SoundDefinitionFactory::destroyResource(MemoryAllocator* pAllocator, Resource* pResource)
    {
        if (pResource != nullptr)
        {
            static_cast<SoundDefinitionResource*>(pResource)->~SoundDefinitionResource();
            pAllocator->free(pResource);
        }
    }

    // 0x00C37C70 (:55)
    bool SoundDefinitionFactory::unlockAllResources(uint32 contextId)
    {
        if (m_pSoundSystem != nullptr)
        {
            SoundSystem::stopAllSounds(m_pSoundSystem, (uint8)contextId);
        }
        return true;
    }
}
