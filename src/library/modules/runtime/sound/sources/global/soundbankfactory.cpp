// Original: library/modules/runtime/sound/sources/global/soundbankfactory.cpp
//
// SoundBankFactory, the ResourceFactory of 'SBNK' sound banks (SFX.md SX1).

#include "keen/sound/soundbankfactory.hpp"
#include "keen/sound/soundbank.hpp"
#include "keen/base/memoryallocator.hpp"

#include <new>

namespace keen
{
    // 0x00C32B20 (:10)
    SoundBankFactory::SoundBankFactory()
    {
    }

    // 0x00C32B60
    SoundBankFactory::~SoundBankFactory()
    {
    }

    // 0x00C32B70 (:14)
    uint32 SoundBankFactory::getType() const
    {
        return SoundBank::ResourceClassType;
    }

    // 0x00C32B80 (:19)
    void SoundBankFactory::create()
    {
    }

    // 0x00C32B90 (:24)
    void SoundBankFactory::destroy()
    {
    }

    // 0x00C35960 (:29)
    Resource* SoundBankFactory::createResource(MemoryAllocator* pAllocator, uint32 key)
    {
        void* pMemory = pAllocator->allocate(sizeof(SoundBank), 4, 0, nullptr);
        if (pMemory == nullptr)
        {
            return nullptr;
        }
        return new (pMemory) SoundBank(key);
    }

    // 0x00C32BA0 (:34)
    // The initialization data is the bank data (relocated in place).
    bool SoundBankFactory::initializeResource(Resource* pResource, uint32 contentFormatKey, const void* pInitializationData, uint32 initializationDataSize)
    {
        (void)contentFormatKey;
        (void)initializationDataSize;
        static_cast<SoundBank*>(pResource)->m_pData = *(const SoundSystem::SoundBankData* const*)pInitializationData;
        return true;
    }

    // 0x00C32BC0 (:46)
    void SoundBankFactory::shutdownResource(Resource* pResource)
    {
        (void)pResource;
    }

    // 0x00C37C30 (:51)
    void SoundBankFactory::destroyResource(MemoryAllocator* pAllocator, Resource* pResource)
    {
        if (pResource != nullptr)
        {
            static_cast<SoundBank*>(pResource)->~SoundBank();
            pAllocator->free(pResource);
        }
    }
}
