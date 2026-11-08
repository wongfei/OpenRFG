// Original: volition/audio_lib/code/audiolib_hardware_types.h
//
// What the hardware layer keeps per bank and per play: the keen sound definitions and files of a bank, the streaming
// wavebank, the hardware play state and handle, and the speaker matrix of a 3D sound. Placement inferred.
#pragma once

#include "volition/audio_lib/code/stub/audiolib_stub_types.h"

struct cfile;

namespace keen
{
    struct SoundDefinitionResourceHandleType;
    class SoundFile;
}

namespace keen
{
    namespace SoundSystem
    {
        class TemporarySoundDefinition;
    }
}

enum audiolib_hw_play_state
{
    AUDIOLIB_HW_PLAY_STATE_UNUSED = 0,
    AUDIOLIB_HW_PLAY_STATE_PENDING_FREE_CHANNEL = 1,
    AUDIOLIB_HW_PLAY_STATE_LOAD_REQUEST = 2,
    AUDIOLIB_HW_PLAY_STATE_LOADING = 3,
    AUDIOLIB_HW_PLAY_STATE_LOADED = 4,
    AUDIOLIB_HW_PLAY_STATE_PREPARING = 5,
    AUDIOLIB_HW_PLAY_STATE_READY = 6,
    AUDIOLIB_HW_PLAY_STATE_PLAYING = 7,
    AUDIOLIB_HW_PLAY_STATE_STALLING = 8,
    AUDIOLIB_HW_PLAY_STATE_STALLED = 9,
    AUDIOLIB_HW_PLAY_STATE_UNSTALLED = 10,
    AUDIOLIB_HW_PLAY_STATE_PAUSED = 11,
    AUDIOLIB_HW_PLAY_STATE_STOPPING = 12,
    AUDIOLIB_HW_PLAY_STATE_STOPPED = 13,
    AUDIOLIB_HW_PLAY_STATE_DESTROYED = 14,
    NUM_AUDIOLIB_HW_PLAY_STATES = 15,
};

// size 0x9C
struct audiolib_hw_soundbank_id
{
    keen::SoundSystem::TemporarySoundDefinition* tmpSoundDef2D; // 0x00
    keen::SoundSystem::TemporarySoundDefinition* tmpSoundDef3D; // 0x04
    keen::SoundFile* soundFiles; // 0x08
    StreamingWavebankData* streamingWaveData; // 0x0C
    unsigned int waveCount; // 0x10
    char waveBankFilePath[128]; // 0x14
    cfile* preopened; // 0x94
    bool is_prepared; // 0x98
};

// size 0x3C
struct audiolib_hw_specific_info
{
    audiolib_hw_specific_info(); // 0x004830D0

    const keen::SoundDefinitionResourceHandleType* hw_play_id; // 0x00
    unsigned int hw_play_handle; // 0x04
    StreamingWavebankData streamingWaveData; // 0x08
    int hw_curr_waveIndex; // 0x38
};

// size 0x48
struct audiolib_channel_to_speaker_map
{
    int num_src_channels; // 0x00
    int num_dst_channels; // 0x04
    float matrix[16]; // 0x08
};
