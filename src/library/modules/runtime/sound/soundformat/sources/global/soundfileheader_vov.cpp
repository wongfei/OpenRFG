// Original: library/modules/runtime/sound/soundformat/sources/global/soundfileheader_vov.cpp
//
// The Ogg Vorbis (VOV) sound file header (SFX.md SX1): the Ogg file size in the first 8 extended bytes. rfg.exe has
// no Vorbis provider; the header is compiled for the converter's format only.

#include "library/modules/runtime/sound/soundformat/include/keen/soundformat/soundfileheader_vov.hpp"

#include <string.h>

// ---- file-static globals
static const char s_headerFormatString[12] = "11114444444"; // 0x012A5BE4

namespace keen
{
    namespace SoundSystem
    {
        // 0x004025A0 (:10)
        const char* SoundFileHeader_VOV::getHeaderFormatString()
        {
            return s_headerFormatString;
        }

        // 0x004025B0 (:15)
        void SoundFileHeader_VOV::setOggFileSize(uint64 size)
        {
            memcpy(extendedData, &size, sizeof(size));
        }

        // 0x004025D0 (:21)
        uint64 SoundFileHeader_VOV::getOggFileSize() const
        {
            uint64 size;
            memcpy(&size, extendedData, sizeof(size));
            return size;
        }
    }
}
