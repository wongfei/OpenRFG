// Original: library/modules/runtime/sound/soundformat/sources/global/soundfileheader.cpp
//
// The keen sound file header's field format string (SFX.md SX1): 4 bytes, then 7 dwords (the endian swap layout of
// the converter).

#include "library/modules/runtime/sound/soundformat/include/keen/soundformat/soundfileheader.hpp"

// ---- file-static globals
static const char s_headerFormatString[12] = "11114444444"; // 0x012A5BD8

namespace keen
{
    namespace SoundSystem
    {
        // 0x00402590 (:10)
        const char* SoundFileHeader::getHeaderFormatString()
        {
            return s_headerFormatString;
        }
    }
}
