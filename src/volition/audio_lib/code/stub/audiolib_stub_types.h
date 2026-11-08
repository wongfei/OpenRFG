// Original: volition/audio_lib/code/stub/audiolib_stub_types.h
//
// A streaming wavebank as the keen-backed stub sees it: the keen sound file header and the wave data.
#pragma once

#include "library/modules/runtime/sound/soundformat/include/keen/soundformat/soundfileheader.hpp"

// size 0x30
struct StreamingWavebankData
{
    StreamingWavebankData(); // 0x00482F40 (.h:36)

    keen::SoundSystem::SoundFileHeader soundHeader; // 0x00
    void* pData; // 0x20
    unsigned int dataSize; // 0x24
    unsigned int crcName; // 0x28
    unsigned int offsetData; // 0x2C
};
