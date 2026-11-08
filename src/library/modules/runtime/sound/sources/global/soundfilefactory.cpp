// Original: library/modules/runtime/sound/sources/global/soundfilefactory.cpp
//
// SoundFileFactory, the ResourceFactory of 'SNDF' sound files (SFX.md SX1).

#include "keen/sound/soundfilefactory.hpp"
#include "keen/sound/soundfile.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/trace.hpp"
#include "library/modules/runtime/sound/soundformat/include/keen/soundformat/soundfileheader.hpp"

#include <new>

namespace keen
{
    // The initialization data of a sound file: the keen header, then the data pointer and size (layout from
    // initializeResource's reads).
    // size 0x28
    struct SoundFileInitializationData
    {
        SoundSystem::SoundFileHeader header; // 0x00
        const void* pData; // 0x20
        uint32 dataSize; // 0x24
    };
    static_assert(sizeof(SoundFileInitializationData) == 0x28, "SoundFileInitializationData size");

    // 0x00C32F20 (:10)
    SoundFileFactory::SoundFileFactory()
    {
    }

    // 0x0048B410
    SoundFileFactory::~SoundFileFactory()
    {
    }

    // 0x00C32F40 (:15)
    void SoundFileFactory::create()
    {
    }

    // 0x00C32F50 (:20)
    void SoundFileFactory::destroy()
    {
    }

    // 0x00C32F60 (:25)
    uint32 SoundFileFactory::getType() const
    {
        return SoundFile::ResourceClassType;
    }

    // 0x00C35A80 (:30)
    Resource* SoundFileFactory::createResource(MemoryAllocator* pAllocator, uint32 key)
    {
        void* pMemory = pAllocator->allocate(sizeof(SoundFile), 4, 0, nullptr);
        if (pMemory == nullptr)
        {
            return nullptr;
        }
        return new (pMemory) SoundFile(key);
    }

    // 0x00C38F70 (:35)
    void SoundFileFactory::destroyResource(MemoryAllocator* pAllocator, Resource* pResource)
    {
        if (pResource != nullptr)
        {
            static_cast<SoundFile*>(pResource)->~SoundFile();
            pAllocator->free(pResource);
        }
    }

    // 0x00C35AC0 (:40)
    // SoundFile::initialize inlined.
    bool SoundFileFactory::initializeResource(Resource* pResource, uint32 contentFormatKey, const void* pInitializationData, uint32 initializationDataSize)
    {
        (void)contentFormatKey;
        (void)initializationDataSize;
        const SoundFileInitializationData* pData = (const SoundFileInitializationData*)pInitializationData;
        if (pData == nullptr || pData->header.sampleRate == 0)
        {
            debug::nativePrintArgs("[Soundfile.cpp] <initialize> Invalid SoundFileHeader Data. \n");
            return false;
        }
        return initializeSoundFileData(&static_cast<SoundFile*>(pResource)->m_data, &pData->header, pData->pData, pData->dataSize);
    }

    // 0x00C32F70 (:60)
    void SoundFileFactory::shutdownResource(Resource* pResource)
    {
        (void)pResource;
    }
}
