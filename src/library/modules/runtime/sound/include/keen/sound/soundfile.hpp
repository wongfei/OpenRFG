// Original: library/modules/runtime/sound/include/keen/sound/soundfile.hpp
// Implementation: library/modules/runtime/sound/sources/global/soundfile.cpp
//
// SoundFile, a sound file resource (encoded audio data).
#pragma once

#include "keen/base/types.hpp"
#include "keen/resources/resource.hpp"
#include "keen/sound/soundfiledata.hpp"

namespace keen
{
    namespace SoundSystem
    {
        struct SoundFileHeader;
    }
}

namespace keen
{
    struct SoundFileHandleType;

    // size 0x4C
    class SoundFile : public Resource
    {
    public:
        enum
        {
            ResourceClassType = 0x46444E53,
        };

        const SoundFileData& getData() const; // 0x00C32860 (.h:18)
        float getLengthInSeconds() const; // 0x00C32F00 (:24)
    private:
        SoundFile(const SoundFile&);
    public:
        SoundFile(uint32 key); // 0x00C32EE0 (:8)
        bool initialize(const SoundSystem::SoundFileHeader* pHeader, const void* pInitData, uint32 dataSize); // 0x00C35A20 (:13)
    private:
        SoundFile& operator=(const SoundFile&);
    public:
        ~SoundFile(); // 0x00C34A90

        SoundFileData m_data; // 0x08
    };

    const SoundFile* getFromHandle(const SoundFileHandleType* handle); // 0x00C32870 (.h:41)
}
