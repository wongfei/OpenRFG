// Original: library/modules/runtime/sound/sources/global/soundfile.cpp
//
// SoundFile, a sound file resource: the keen header and the wave data (SFX.md SX1). audio_lib makes its wavebank waves
// into SoundFiles through SoundFileFactory::initializeResource.

#include "keen/sound/soundfile.hpp"
#include "keen/sound/soundhandle.hpp"
#include "keen/base/trace.hpp"
#include "library/modules/runtime/sound/soundformat/include/keen/soundformat/soundfileheader.hpp"

namespace keen
{
    // 0x00C32860 (soundfile.hpp:18)
    const SoundFileData& SoundFile::getData() const
    {
        return m_data;
    }

    // 0x00C32EE0 (:8)
    SoundFile::SoundFile(uint32 key)
        : Resource(key)
    {
    }

    // 0x00C34A90
    SoundFile::~SoundFile()
    {
    }

    // 0x00C35A20 (:13)
    bool SoundFile::initialize(const SoundSystem::SoundFileHeader* pHeader, const void* pInitData, uint32 dataSize)
    {
        if (pHeader == nullptr || pHeader->sampleRate == 0)
        {
            debug::nativePrintArgs("[Soundfile.cpp] <initialize> Invalid SoundFileHeader Data. \n");
            return false;
        }
        return initializeSoundFileData(&m_data, pHeader, pInitData, dataSize);
    }

    // 0x00C32F00 (:24)
    float SoundFile::getLengthInSeconds() const
    {
        return (float)((double)m_data.durationInms / 1000.0);
    }

    // 0x00C32870 (soundfile.hpp:41)
    // The handle is the resource.
    const SoundFile* getFromHandle(const SoundFileHandleType* handle)
    {
        return (const SoundFile*)handle;
    }
}
