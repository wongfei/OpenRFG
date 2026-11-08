// Original: library/modules/runtime/sound/soundformat/include/keen/soundformat/soundfileheader.hpp
// Implementation: library/modules/runtime/sound/soundformat/sources/global/soundfileheader.cpp
//
// The header of keen sound files.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    namespace SoundSystem
    {
        // size 0x20
        struct SoundFileHeader
        {
            enum
            {
                SoundFileHeaderFlag_Loopable = 1,
                SoundFileHeaderFlag_BufferInterleaved = 2,
            };

            static const char* getHeaderFormatString(); // 0x00402590 (:10)

            uint8 platform; // 0x00
            uint8 version; // 0x01
            uint8 numChannels; // 0x02
            uint8 flags; // 0x03
            uint32 typeKey; // 0x04
            uint32 sampleRate; // 0x08
            uint32 bufferSize; // 0x0C
            uint32 numSamples; // 0x10
            uint8 extendedData[12]; // 0x14
        };

    }
}
