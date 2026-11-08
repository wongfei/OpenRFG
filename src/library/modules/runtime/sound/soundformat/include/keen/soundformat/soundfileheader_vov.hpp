// Original: library/modules/runtime/sound/soundformat/include/keen/soundformat/soundfileheader_vov.hpp
// Implementation: library/modules/runtime/sound/soundformat/sources/global/soundfileheader_vov.cpp
//
// The header of Ogg Vorbis (VOV) sound files.
#pragma once

#include "keen/base/types.hpp"
#include "library/modules/runtime/sound/soundformat/include/keen/soundformat/soundfileheader.hpp"

namespace keen
{
    namespace SoundSystem
    {
        // size 0x20
        struct SoundFileHeader_VOV : public SoundFileHeader
        {
            typedef SoundFileHeader BaseClass;

            static const char* getHeaderFormatString(); // 0x004025A0 (:10)
            void setOggFileSize(uint64 size); // 0x004025B0 (:15)
            uint64 getOggFileSize() const; // 0x004025D0 (:21)

        };

    }
}
