// Original: library/modules/runtime/sound/include/keen/sound/soundfiledata.hpp
// Implementation: library/modules/runtime/sound/sources/win32/soundfiledata_win32.cpp
//
// SoundFileData, the decoded description of a sound file (format, length, loop points).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    namespace SoundSystem
    {
        struct SoundFileHeader;
    }
}

namespace keen
{
    // size 0x44
    struct SoundFileData
    {
        enum
        {
            Sizeof_XMA2WAVEFORMATEX = 52,
        };

        uint8 providerId; // 0x00
        uint8 xmaBuffer[52]; // 0x01
        const void* pData; // 0x38
        uint32 dataLength; // 0x3C
        uint32 durationInms; // 0x40
    };

    bool initializeSoundFileData(SoundFileData* pData, const SoundSystem::SoundFileHeader* pHeader, const void* pInitData, uint32 dataSize); // 0x00C337F0 (soundfiledata_win32.cpp:19)
}
