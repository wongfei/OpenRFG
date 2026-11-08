// Original: volition/audio_lib/code/stub/audiolib_stub.cpp
//
// The hardware layer of the shipped game (SFX.md SX2, 1.3/1.4): Kaiko's port of audio_lib's audiolib_hw_* interface to
// the keen sound system. Banks are read whole (in-memory banks through stream2, streaming banks kept open as a cfile);
// an in-memory wavebank becomes one 2D and one 3D TemporarySoundDefinition per wave, a streaming play reads its whole
// wave and gets a dynamic definition. Plays start, stop, pause and move through keen calls under m_audio_mutex, with
// Volition's y-up positions turned into keen's z-up ones (y and z swapped). Bus 0 is the dry bus, bus 1 the reverb bus.
// Compiled last in audio_lib_unity.cpp (Soundbank_list and Audiolib_hw_init_min are statics of audiolib.cpp there).

#include "volition/audio_lib/code/stub/audiolib_stub.h"

#include "keen/base/framenumber.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/mutex.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "keen/math/matrix43.hpp"
#include "keen/math/vector3.hpp"
#include "keen/sound/fxdefinitionreverb.hpp"
#include "keen/sound/soundfile.hpp"
#include "keen/sound/soundfilefactory.hpp"
#include "keen/sound/soundsystem.hpp"
#include "library/modules/runtime/sound/sources/global/temporarysounddefinition.hpp"
#include "volition/audio_lib/code/audiolib_foley_category.h"
#include "volition/audio_lib/code/audiolib_hardware.h"
#include "volition/audio_lib/code/audiolib_internal.h"
#include "volition/audio_lib/code/audiolib_soundbank.h"
#include "volition/stream2/code/stream2_os.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/os.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// ---- public globals (rfg.pdb; no header declares them)
// The I3DL2 reverb presets (Default, Generic, PaddedCell, ... as in the DirectX SDK), indexed by
// AUDIOLIB_REVERB_ROOM_TYPE. Values from rfg.exe.
keen::SoundSystem::FXReverbParameter m_ReverbPresets[30] = { // 0x014F9550
    {-10000.0f, -10000.0f, 0.1f, 0.1f, -10000.0f, 0.0f, -10000.0f, 0.0f, 0.0f, 0.0f, 20.0f},
    {-1000.0f, -100.0f, 1.49f, 0.83f, -2602.0f, 0.007f, 200.0f, 0.011f, 100.0f, 100.0f, 5000.0f},
    {-1000.0f, -6000.0f, 0.17f, 0.1f, -1204.0f, 0.001f, 207.0f, 0.002f, 100.0f, 100.0f, 5000.0f},
    {-1000.0f, -454.0f, 0.4f, 0.83f, -1646.0f, 0.002f, 53.0f, 0.003f, 100.0f, 100.0f, 5000.0f},
    {-1000.0f, -1200.0f, 1.49f, 0.54f, -370.0f, 0.007f, 1030.0f, 0.011f, 100.0f, 60.0f, 5000.0f},
    {-1000.0f, -6000.0f, 0.5f, 0.1f, -1376.0f, 0.003f, -1104.0f, 0.004f, 100.0f, 100.0f, 5000.0f},
    {-1000.0f, -300.0f, 2.31f, 0.64f, -711.0f, 0.012f, 83.0f, 0.017f, 100.0f, 100.0f, 5000.0f},
    {-1000.0f, -476.0f, 4.32f, 0.59f, -789.0f, 0.02f, -289.0f, 0.03f, 100.0f, 100.0f, 5000.0f},
    {-1000.0f, -500.0f, 3.92f, 0.7f, -1230.0f, 0.02f, -2.0f, 0.029f, 100.0f, 100.0f, 5000.0f},
    {-1000.0f, 0.0f, 2.91f, 1.3f, -602.0f, 0.015f, -302.0f, 0.022f, 100.0f, 100.0f, 5000.0f},
    {-1000.0f, -698.0f, 7.24f, 0.33f, -1166.0f, 0.02f, 16.0f, 0.03f, 100.0f, 100.0f, 5000.0f},
    {-1000.0f, -1000.0f, 10.05f, 0.23f, -602.0f, 0.02f, 198.0f, 0.03f, 100.0f, 100.0f, 5000.0f},
    {-1000.0f, -4000.0f, 0.3f, 0.1f, -1831.0f, 0.002f, -1630.0f, 0.03f, 100.0f, 100.0f, 5000.0f},
    {-1000.0f, -300.0f, 1.49f, 0.59f, -1219.0f, 0.007f, 441.0f, 0.011f, 100.0f, 100.0f, 5000.0f},
    {-1000.0f, -237.0f, 2.7f, 0.79f, -1214.0f, 0.013f, 395.0f, 0.02f, 100.0f, 100.0f, 5000.0f},
    {-1000.0f, -270.0f, 1.49f, 0.86f, -1204.0f, 0.007f, -4.0f, 0.011f, 100.0f, 100.0f, 5000.0f},
    {-1000.0f, -3300.0f, 1.49f, 0.54f, -2560.0f, 0.162f, -613.0f, 0.088f, 79.0f, 100.0f, 5000.0f},
    {-1000.0f, -800.0f, 1.49f, 0.67f, -2273.0f, 0.007f, -2217.0f, 0.011f, 50.0f, 100.0f, 5000.0f},
    {-1000.0f, -2500.0f, 1.49f, 0.21f, -2780.0f, 0.3f, -2014.0f, 0.1f, 27.0f, 100.0f, 5000.0f},
    {-1000.0f, -1000.0f, 1.49f, 0.83f, -10000.0f, 0.061f, 500.0f, 0.025f, 100.0f, 100.0f, 5000.0f},
    {-1000.0f, -2000.0f, 1.49f, 0.5f, -2466.0f, 0.179f, -2514.0f, 0.1f, 21.0f, 100.0f, 5000.0f},
    {-1000.0f, 0.0f, 1.65f, 1.5f, -1363.0f, 0.008f, -1153.0f, 0.012f, 100.0f, 100.0f, 5000.0f},
    {-1000.0f, -1000.0f, 2.81f, 0.14f, 429.0f, 0.014f, 648.0f, 0.021f, 80.0f, 60.0f, 5000.0f},
    {-1000.0f, -4000.0f, 1.49f, 0.1f, -449.0f, 0.007f, 1700.0f, 0.011f, 100.0f, 100.0f, 5000.0f},
    {-1000.0f, -600.0f, 1.1f, 0.83f, -400.0f, 0.005f, 500.0f, 0.01f, 100.0f, 100.0f, 5000.0f},
    {-1000.0f, -600.0f, 1.3f, 0.83f, -1000.0f, 0.01f, -200.0f, 0.02f, 100.0f, 100.0f, 5000.0f},
    {-1000.0f, -600.0f, 1.5f, 0.83f, -1600.0f, 0.02f, -1000.0f, 0.04f, 100.0f, 100.0f, 5000.0f},
    {-1000.0f, -600.0f, 1.8f, 0.7f, -1300.0f, 0.015f, -800.0f, 0.03f, 100.0f, 100.0f, 5000.0f},
    {-1000.0f, -600.0f, 1.8f, 0.7f, -2000.0f, 0.03f, -1400.0f, 0.06f, 100.0f, 100.0f, 5000.0f},
    {-1000.0f, -200.0f, 1.3f, 0.9f, 0.0f, 0.002f, 0.0f, 0.01f, 100.0f, 75.0f, 5000.0f},
};
int m_currentReverbType; // 0x01A79AF4
keen::MemoryAllocator* m_pAudioAllocator; // 0x01A7AF04
float m_currentReverbScale; // 0x01A7AF0C

// ---- file-static globals
static const short audiolib_bank_alignment[4] = {16, 2048, 2048, 16}; // 0x012BBE34
// int audiolib_output_channels[3] (0x014F9110) is used here too; placed in audiolib.cpp.
static int index_count = 1; // 0x014F9538
static const char* audiolib_stub_bank_file_ext[4] = {".xsb_pc", ".xwb_pc", ".xwb_pc", ".xgs_pc"}; // 0x014F953C
// volatile bool Audiolib_hw_init_min (0x01A7AEED) is used here too; placed in audiolib.cpp.
// object_pool<audiolib_soundbank_data,252,5> Soundbank_list (0x01A83AD0) is used here too; placed in audiolib.cpp.

// The reverb effect as setBusEffectChain takes it: keen::SoundSystem::FXDefinitionReverb, built field by field on the
// stack in rfg.exe (its constructor is not in the binary).
struct audiolib_stub_reverb_definition
{
    keen::SoundSystem::BaseEffectDefinition base; // 0x00
    keen::SoundSystem::FXReverbParameter parameter; // 0x08
};
static_assert(sizeof(audiolib_stub_reverb_definition) == 0x34, "audiolib_stub_reverb_definition");
// The per-wave arrays are sized with these (0x54 and 0x4C in rfg.exe).
static_assert(sizeof(keen::SoundSystem::TemporarySoundDefinition) == 0x54, "TemporarySoundDefinition");
static_assert(sizeof(keen::SoundFile) == 0x4C, "SoundFile");

// Volition's y-up position as keen's z-up one.
static keen::Vector3 audiolib_stub_keen_position(const vector& pos)
{
    keen::Vector3 position;
    position.x = pos.x;
    position.y = pos.z;
    position.z = pos.y;
    return position;
}

// 0x00482F40 (audiolib_stub_types.h:36)
StreamingWavebankData::StreamingWavebankData()
{
    pData = nullptr;
}

// 0x0048A600 (:82)
void audiolib_hw_soundbank_create_file_and_pathname(const char* in_name, const char* sub_dir, short bank_type, char* out_name, char* out_path)
{
    if (in_name == nullptr || out_name == nullptr)
    {
        return;
    }
    strncpy(out_name, in_name, 64);
    char* extension = keen::findLastCharacterInString(out_name, '.');
    if (!keen::isStringEqualNoCase(extension, audiolib_stub_bank_file_ext[bank_type]))
    {
        // Original: any other extension stays and the bank's is appended after it.
        strcat(out_name, audiolib_stub_bank_file_ext[bank_type]);
    }
    if (out_path != nullptr)
    {
        strcpy(out_path, Audiolib_state.root_dir);
        strcat(out_path, Audiolib_state.data_dir);
        if (sub_dir != nullptr)
        {
            strcat(out_path, sub_dir);
        }
        const size_t length = strlen(out_path);
        if (out_path[length - 1] != '\\' && out_path[length - 1] != '/')
        {
            strcat(out_path, "\\");
        }
        strcat(out_path, out_name);
    }
}

// 0x00492A00 (:140)
audiolib_result audiolib_hw_3d_setup(audiolib_play_data* play)
{
    audiolib_internal_dsp_hw_setup(play);
    return AUDIOLIB_ERR_NONE;
}

// 0x0048A790 (:149)
audiolib_result audiolib_hw_2d_to_3d(audiolib_play_data* play)
{
    (void)play;
    return AUDIOLIB_ERR_NONE;
}

// 0x0048A7A0 (:156)
audiolib_result audiolib_hw_3d_to_2d(audiolib_play_data* play)
{
    (void)play;
    return AUDIOLIB_ERR_NONE;
}

// 0x0048A7B0 (:163)
audiolib_result audiolib_hw_3d_update(audiolib_play_data* play, audiolib_channel_to_speaker_map* output_map)
{
    // The speaker matrix is not used: keen pans 3D sounds itself.
    (void)output_map;
    if (play == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_3d_update> play is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (play->hw_play_state == AUDIOLIB_HW_PLAY_STATE_PLAYING)
    {
        const keen::Vector3 position = audiolib_stub_keen_position(play->curr_pos);
        // audio_lib volumes are 0-10 (0.1 is 0.1f in rfg.exe).
        const float volume = play->volume_scale * 0.1f;
        m_audio_mutex.lock(0);
        keen::SoundSystem::setVolume(m_pSoundSystem, play->hw_info.hw_play_handle, volume, 0.0f);
        keen::SoundSystem::setSoundPosition(m_pSoundSystem, play->hw_info.hw_play_handle, position);
        m_audio_mutex.unlock();
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x0048A8B0 (:220)
audiolib_result audiolib_hw_is_sound_bank_ready(audiolib_soundbank_data* pBank)
{
    if (pBank == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_is_sound_bank_ready> pBank is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    return pBank->is_loaded ? AUDIOLIB_ERR_NONE : AUDIOLIB_ERR_HW_NOT_READY;
}

// 0x004983C0 (:236)
audiolib_result audiolib_hw_load_sound_bank(audiolib_soundbank_data* pBank, mempool* pool)
{
    if (pBank == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_load_sound_bank> pBank is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (!Audiolib_state.hw_inited)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_load_sound_bank> Hardware is not initialized yet, abort loading Bank! Bank name: %s \n", pBank->bank_name);
        return AUDIOLIB_ERR_NOT_INITED;
    }
    pBank->is_loaded = false;
    pBank->valid = false;
    pBank->hw_bank_id.is_prepared = false;
    audiolib_result result = AUDIOLIB_ERR_NONE;
    switch (pBank->bank_type)
    {
    case AUDIOLIB_SOUNDBANK:
        result = audiolib_hw_soundbank_load_file_into_RAM(pBank, pBank->bank_name, pool);
        if (result < 0)
        {
            keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_load_sound_bank> failed to load %s Soundbank into ram. Result: %d \n", pBank->bank_name, result);
        }
        break;
    case AUDIOLIB_WAVEBANK:
        result = audiolib_hw_soundbank_load_file_into_RAM(pBank, pBank->bank_name, pool);
        if (result < 0)
        {
            keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_load_sound_bank> failed to load %s Wavebank into ram. Result: %d \n", pBank->bank_name, result);
        }
        break;
    case AUDIOLIB_WAVEBANK_STREAMING:
        result = audiolib_hw_soundbank_open_file_for_audio_streaming(pBank, pBank->bank_name);
        if (result < 0)
        {
            keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_load_sound_bank> failed to load %s Wavebank_Streaming into ram. Result: %d \n", pBank->bank_name, result);
        }
        break;
    case AUDIOLIB_SETTINGS_FILE:
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_load_sound_bank> tryed to load %s Settings_File, not implemented yet!", pBank->bank_name);
        return AUDIOLIB_ERR_NONE;
    default:
        return result;
    }
    if (result != AUDIOLIB_ERR_NONE)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_load_sound_bank> Bank NOT loaded! Type: %d Name: %s \n", (int)pBank->bank_type, pBank->bank_name);
    }
    return result;
}

// 0x00492A10 (:314)
audiolib_result audiolib_hw_soundbank_load_file_into_RAM(audiolib_soundbank_data* pBank, const char* filename, mempool* pool)
{
    if (pBank == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_soundbank_load_file_into_RAM> pBank is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    char name[64];
    char path[256];
    audiolib_hw_soundbank_create_file_and_pathname(filename, pBank->sub_dir, pBank->bank_type, name, path);
    pBank->alignment = audiolib_bank_alignment[pBank->bank_type];
    pBank->is_loaded = false;
    // The packfile name is used, not the path.
    if (!cf_get_file_size((unsigned int*)&pBank->bank_size, name, false) || pBank->bank_size <= 0)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_soundbank_load_file_into_RAM> Unable to read File size for: %s(%s)\n", name, pBank->bank_name);
        return AUDIOLIB_ERR_FILE_READ;
    }
    // Original: the mark is always taken on (and restored to) Audiolib_state.pool, whatever the bank is read into.
    const unsigned int mark = Audiolib_state.pool->mark();
    unsigned char* buffer;
    if (pool == nullptr)
    {
        buffer = (unsigned char*)m_pAudioBankAllocator->allocate(pBank->bank_size + 1, pBank->alignment, 0, nullptr);
    }
    else
    {
        buffer = (unsigned char*)pool->alloc(pBank->bank_size + 1, pBank->alignment);
    }
    pBank->bank_buffer = (char*)buffer;
    if (buffer == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_soundbank_load_file_into_RAM> Could not allocate memory for bank %s\n", filename);
        return AUDIOLIB_ERR_FILE_READ;
    }
    stream2_os_file request;
    request.chain_read = false;
    request.encrypted = false;
    request.chain_num_reads = 0;
    request.chain_size = nullptr;
    request.chain_dest = nullptr;
    request.preopened = nullptr;
    strcpy(request.filename, name);
    request.size = pBank->bank_size;
    request.offset = 0;
    request.priority = 200;
    request.category = 2;
    request.dest = buffer;
    const unsigned int req = stream2_os_read(request);
    stream2_os_req_status status;
    while ((status = stream2_os_poll(req)) == SOS_BUSY)
    {
        os_sleep(1);
    }
    if (status == SOS_DONE_SUCCESS)
    {
        pBank->is_loaded = true;
        stream2_os_release_handle(req);
        return AUDIOLIB_ERR_NONE;
    }
    keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_soundbank_load_file_into_RAM> Unable to read File size for: %s(%s)\n", name, pBank->bank_name);
    stream2_os_release_handle(req);
    Audiolib_state.pool->restore_to_mark(mark);
    return AUDIOLIB_ERR_FILE_READ;
}

// 0x0048A8F0 (:485)
audiolib_result audiolib_hw_soundbank_open_file_for_audio_streaming(audiolib_soundbank_data* pWaveBank, const char* filename)
{
    if (pWaveBank == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_soundbank_open_file_for_audio_streaming> pWaveBank is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    char name[64];
    char path[256];
    audiolib_hw_soundbank_create_file_and_pathname(filename, pWaveBank->sub_dir, pWaveBank->bank_type, name, path);
    pWaveBank->alignment = audiolib_bank_alignment[pWaveBank->bank_type];
    pWaveBank->is_loaded = false;
    // Sectors read at a time, 2-256 (unused by the keen port).
    if (pWaveBank->sector_read < 2)
    {
        pWaveBank->sector_read = 2;
    }
    else if (pWaveBank->sector_read > 256)
    {
        pWaveBank->sector_read = 256;
    }
    pWaveBank->hw_bank_id.preopened = cf_open(path, "rb", PLATFORM_PC, true);
    if (pWaveBank->hw_bank_id.preopened == nullptr)
    {
        return AUDIOLIB_ERR_FILE_OPEN;
    }
    pWaveBank->bank_size = cf_size(pWaveBank->hw_bank_id.preopened);
    if (pWaveBank->bank_size < 1)
    {
        // Original: preopened keeps the closed cfile.
        cf_close(pWaveBank->hw_bank_id.preopened);
        return AUDIOLIB_ERR_FILE_READ;
    }
    pWaveBank->is_loaded = true;
    return AUDIOLIB_ERR_NONE;
}

// 0x0048AA50 (:573)
void audiolib_hw_mute_all(bool mute)
{
    if (!Audiolib_state.hw_inited)
    {
        return;
    }
    m_audio_mutex.lock(0);
    if (mute)
    {
        keen::SoundSystem::setBusSendVolume(m_pSoundSystem, 0, 0, 0.0f, 0.0f, 0.0f);
        keen::SoundSystem::setBusSendVolume(m_pSoundSystem, 1, 0, 0.0f, 0.0f, 0.0f);
    }
    else
    {
        // Original: unmuting restores bus 0's sends but leaves bus 1 (the reverb bus) muted.
        keen::SoundSystem::setBusSendVolume(m_pSoundSystem, 0, 0, 1.0f, 0.0f, 0.0f);
        keen::SoundSystem::setBusSendVolume(m_pSoundSystem, 0, 1, 0.0f, m_currentReverbScale, 0.0f);
    }
    m_audio_mutex.unlock();
}

// 0x0048AB30 (:598)
audiolib_result audiolib_hw_system_init_minimal()
{
    if (!Audiolib_hw_init_min)
    {
        Audiolib_state.speaker_type = AUDIOLIB_SPEAKER_TYPE_STEREO;
        Audiolib_state.speaker_count = audiolib_output_channels[AUDIOLIB_SPEAKER_TYPE_STEREO];
        Audiolib_hw_init_min = true;
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x0048AB60 (:612)
void audiolib_hw_system_shutdown_minimal()
{
    Audiolib_hw_init_min = false;
}

// 0x00492C50 (:617)
audiolib_result audiolib_hw_system_init_stage_1(keen::MemoryAllocator* pAudioAllocator, keen::MemoryAllocator* pAudioBankAllocator, keen::SoundSystem::System* pSoundSystem)
{
    if (pAudioAllocator == nullptr || pAudioBankAllocator == nullptr || pSoundSystem == nullptr)
    {
        // Original: reported, then initialized anyway.
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_system_init_stage_1> pAudioAllocator || pAudioBankAllocator || pSoundSystem is a nullptr! \n");
    }
    if (!Audiolib_state.hw_inited)
    {
        m_pAudioAllocator = pAudioAllocator;
        m_pAudioBankAllocator = pAudioBankAllocator;
        audiolib_keenAudio_init(pSoundSystem);
        Audiolib_state.hw_inited = true;
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x004984E0 (:638)
audiolib_result audiolib_hw_system_init_stage_2()
{
    if (Audiolib_state.hw_inited && !Audiolib_state.hw_ready)
    {
        // Every bank in every load state list.
        for (int state = 0; state < NUM_AUDIOLIB_SOUNDBANK_STATES; ++state)
        {
            Soundbank_list.thread_access.enter();
            audiolib_soundbank_data* next = Soundbank_list.get_first(state);
            while (next != nullptr)
            {
                audiolib_soundbank_data* bank = next;
                next = Soundbank_list.get_next(bank);
                if (bank->bank_type == AUDIOLIB_WAVEBANK && bank->bank_buffer != nullptr)
                {
                    if (!audiolib_process_inMemory_wavebank(bank))
                    {
                        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_system_init_stage_2> Wavebank not correct processed. %s \n", bank->bank_name);
                    }
                }
                else if (bank->bank_type == AUDIOLIB_WAVEBANK_STREAMING && !audiolib_process_streaming_wavebank(bank))
                {
                    keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_system_init_stage_2> Streaming wavebank not correct processed. %s \n", bank->bank_name);
                }
            }
            Soundbank_list.thread_access.leave();
        }
        Audiolib_state.hw_ready = true;
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x0048AB70 (:675)
void audiolib_hw_system_shutdown()
{
    m_audio_mutex.destroy();
}

// 0x00498610 (:692)
audiolib_result audiolib_hw_register_soundbank(audiolib_soundbank_data* pBank)
{
    if (pBank == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_register_soundbank> pBank is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (!Audiolib_state.hw_inited)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_register_soundbank> Hardware is not initialized yet, abort register Bank! Bank name: %s \n", pBank->bank_name);
        return AUDIOLIB_ERR_NOT_INITED;
    }
    // A registered bank's data belongs to the caller (stream2 containers): only the vehicle banks are processed here.
    if (pBank->isVehicleBank)
    {
        audiolib_process_inMemory_wavebank(pBank);
    }
    pBank->is_loaded = true;
    pBank->valid = true;
    return AUDIOLIB_ERR_NONE;
}

// 0x0048AB80 (:716)
audiolib_result audiolib_hw_unload_sound_bank(audiolib_soundbank_data* pBank)
{
    if (pBank == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_unload_sound_bank> pBank is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (!Audiolib_state.hw_inited)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_unload_sound_bank> Hardware is not initialized yet, abort unloading Bank! Bank name: %s \n", pBank->bank_name);
        return AUDIOLIB_ERR_NOT_INITED;
    }
    // Voice and vehicle bank data belongs to their stream2 containers.
    if (pBank->bank_buffer != nullptr && !pBank->isVoiceBank && !pBank->isVehicleBank)
    {
        m_pAudioBankAllocator->free(pBank->bank_buffer);
        pBank->bank_buffer = nullptr;
    }
    audiolib_hw_soundbank_id& hw = pBank->hw_bank_id;
    if (hw.tmpSoundDef2D != nullptr)
    {
        m_pAudioAllocator->free(hw.tmpSoundDef2D);
        hw.tmpSoundDef2D = nullptr;
    }
    if (hw.tmpSoundDef3D != nullptr)
    {
        m_pAudioAllocator->free(hw.tmpSoundDef3D);
        hw.tmpSoundDef3D = nullptr;
    }
    if (hw.soundFiles != nullptr)
    {
        m_pAudioAllocator->free(hw.soundFiles);
        hw.soundFiles = nullptr;
    }
    if (hw.streamingWaveData != nullptr)
    {
        // Original: only the first entry's data is checked (always null: plays own their wave data).
        if (hw.streamingWaveData->pData != nullptr)
        {
            m_pAudioBankAllocator->free(hw.streamingWaveData->pData);
        }
        m_pAudioAllocator->free(hw.streamingWaveData);
        // Original bug kept: written after the free (offset 0x20 of the freed block, in the same TLSF heap as rfg.exe).
        hw.streamingWaveData->pData = nullptr;
        hw.streamingWaveData = nullptr;
    }
    if (hw.preopened != nullptr)
    {
        cf_close(hw.preopened);
        hw.preopened = nullptr;
    }
    pBank->valid = false;
    hw.is_prepared = false;
    return AUDIOLIB_ERR_NONE;
}

// 0x0048ACB0 (:787)
audiolib_result audiolib_hw_unregister_soundbank(audiolib_soundbank_data* pBank)
{
    if (pBank == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_unregister_soundbank> pBank is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (!Audiolib_state.hw_inited)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_unregister_soundbank> Hardware is not initialized yet, abort unregister Bank! Bank name: %s \n", pBank->bank_name);
        return AUDIOLIB_ERR_NOT_INITED;
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x0048ACF0 (:804)
audiolib_result audiolib_hw_get_cue_index(audiolib_soundbank_data* pBank, const char* cue_name, int* cue_index)
{
    (void)cue_name;
    if (pBank == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_get_cue_index> pBank is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (cue_index == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_get_cue_index> cue_index is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (!Audiolib_state.hw_inited)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_get_cue_index> Hardware is not initialized yet! Bank name: %s \n", pBank->bank_name);
        return AUDIOLIB_ERR_NOT_INITED;
    }
    // Not a lookup: every call hands out the next number.
    *cue_index = index_count++;
    return AUDIOLIB_ERR_NONE;
}

// 0x0048AD60 (:834)
audiolib_result audiolib_hw_cue_is_valid(audiolib_soundbank_data* pBank, int cue_index)
{
    (void)cue_index;
    if (pBank == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_cue_is_valid> pBank is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (!Audiolib_state.hw_ready)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_cue_is_valid> Hardware is not ready yet! Bank name: %s \n", pBank->bank_name);
        return AUDIOLIB_ERR_HW_NOT_READY;
    }
    if (pBank->is_loaded && pBank->valid)
    {
        return AUDIOLIB_ERR_NONE;
    }
    return AUDIOLIB_ERR_HW_NOT_READY;
}

// 0x004A5980 (:858)
audiolib_result audiolib_hw_prepare(audiolib_play_data* play, int offset_ms)
{
    (void)offset_ms;
    if (play == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_prepare> play is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (play->foley_data.wavebank_type == 0)
    {
        return audiolib_prepare_inMemory_sound(play);
    }
    if (play->foley_data.wavebank_type == 1)
    {
        return audiolib_prepare_streaming_sound(play);
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x0048ADB0 (:883)
audiolib_result audiolib_hw_destroy(audiolib_play_data* play)
{
    if (play == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_destroy> play is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (!Audiolib_state.hw_ready)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_destroy> Hardware is not ready yet! \n");
        return AUDIOLIB_ERR_HW_NOT_READY;
    }
    // A streamed play owns its wave data and its dynamic definition.
    if (play->hw_info.streamingWaveData.dataSize != 0)
    {
        if (play->hw_info.streamingWaveData.pData != nullptr)
        {
            m_pAudioBankAllocator->free(play->hw_info.streamingWaveData.pData);
            play->hw_info.streamingWaveData.pData = nullptr;
        }
        if (play->hw_info.hw_play_id != nullptr)
        {
            m_audio_mutex.lock(0);
            keen::SoundSystem::destroyDynamicSoundDefinition(m_pSoundSystem, play->hw_info.hw_play_id);
            play->hw_info.hw_play_id = nullptr;
            m_audio_mutex.unlock();
        }
    }
    play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_DESTROYED;
    return AUDIOLIB_ERR_NONE;
}

// 0x0048AE70 (:923)
audiolib_result audiolib_hw_stop(audiolib_play_data* play)
{
    if (play == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_stop> play is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (!Audiolib_state.hw_ready)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_stop> Hardware is not ready yet! \n");
        return AUDIOLIB_ERR_HW_NOT_READY;
    }
    // 0xFFFF: keen's invalid sound handle; 0: never started.
    if (play->hw_info.hw_play_handle == 0xFFFF)
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    if (play->hw_info.hw_play_handle != 0)
    {
        m_audio_mutex.lock(0);
        if (!keen::SoundSystem::stopSound(m_pSoundSystem, play->hw_info.hw_play_handle, 0.0f))
        {
            m_audio_mutex.unlock();
            return AUDIOLIB_ERR_HW_FAILED_TO_STOP_CUE;
        }
        m_audio_mutex.unlock();
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x0048AF10 (:960)
audiolib_result audiolib_hw_play(audiolib_play_data* play, char* track_data_ptr, int length)
{
    (void)track_data_ptr;
    (void)length;
    if (play == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_play> play is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (play->hw_info.hw_play_id == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_play> Sound not prepared \n");
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    if (play->hw_play_state == AUDIOLIB_HW_PLAY_STATE_DESTROYED)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_play> Can't Play! Event state is set to Destroyed! \n");
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (!Audiolib_state.hw_ready)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_play> Hardware is not ready yet! \n");
        return AUDIOLIB_ERR_HW_NOT_READY;
    }
    const keen::Vector3 position = audiolib_stub_keen_position(play->curr_pos);
    m_audio_mutex.lock(0);
    // The looping flag is the sound flags argument (flag 1: loop).
    play->hw_info.hw_play_handle = keen::SoundSystem::startSound(m_pSoundSystem, play->hw_info.hw_play_id, position, play->foley_data.looping);
    m_audio_mutex.unlock();
    if (play->hw_info.hw_play_handle == 0xFFFF)
    {
        return AUDIOLIB_ERR_NOT_PLAYING;
    }
    // Original: outside the mutex.
    keen::SoundSystem::setVolume(m_pSoundSystem, play->hw_info.hw_play_handle, play->volume_scale * 0.1f, 0.0f);
    return AUDIOLIB_ERR_NONE;
}

// 0x0048B0B0 (:1018)
audiolib_result audiolib_hw_pause(audiolib_play_data* play, bool pause)
{
    if (play == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_pause> play is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (play->hw_info.hw_play_handle == 0xFFFF)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_pause> play is invalid! \n");
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    if (play->hw_info.hw_play_handle != 0)
    {
        if (!Audiolib_state.hw_ready)
        {
            keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_pause> Hardware is not ready yet! \n");
            return AUDIOLIB_ERR_HW_NOT_READY;
        }
        m_audio_mutex.lock(0);
        if (pause)
        {
            if (!keen::SoundSystem::pauseSound(m_pSoundSystem, play->hw_info.hw_play_handle))
            {
                m_audio_mutex.unlock();
                keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_pause> Failed to Pause!\n");
                return AUDIOLIB_ERR_HW_FAILED_TO_PAUSE_CUE;
            }
        }
        else if (!keen::SoundSystem::resumeSound(m_pSoundSystem, play->hw_info.hw_play_handle))
        {
            m_audio_mutex.unlock();
            return AUDIOLIB_ERR_HW_FAILED_TO_UNPAUSE_CUE;
        }
        m_audio_mutex.unlock();
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x0048B1A0 (:1070)
audiolib_hw_play_state audiolib_hw_play_get_audiolib_state(audiolib_play_data* play)
{
    if (play == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_play_get_audiolib_state> play is a nullptr! \n");
        return AUDIOLIB_HW_PLAY_STATE_STOPPED;
    }
    // Original: isSoundPlaying is called without m_audio_mutex.
    if (play->hw_play_state == AUDIOLIB_HW_PLAY_STATE_PLAYING && !keen::SoundSystem::isSoundPlaying(m_pSoundSystem, play->hw_info.hw_play_handle))
    {
        return AUDIOLIB_HW_PLAY_STATE_STOPPED;
    }
    return play->hw_play_state;
}

// 0x0048B1F0 (:1089)
audiolib_result audiolib_hw_set_pitch(audiolib_play_data* play, float new_pitch)
{
    if (play == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_set_pitch> play is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (play->hw_info.hw_play_handle == 0xFFFF)
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    if (play->hw_info.hw_play_handle != 0)
    {
        m_audio_mutex.lock(0);
        // audio_lib pitch in octaves (-1..1) to keen's frequency ratio: 2^(pitch + 1) / 2. x87 in rfg.exe, each step
        // stored as a float.
        const float exponent = (float)((double)new_pitch + 1.0);
        const float power = (float)pow(2.0, (double)exponent);
        const float ratio = (float)((double)power * 0.5);
        keen::SoundSystem::setPitch(m_pSoundSystem, play->hw_info.hw_play_handle, ratio, 0.0f);
        m_audio_mutex.unlock();
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x0048B2A0 (:1133)
void audiolib_hw_pre_process_work()
{
}

// 0x0048B2B0 (:1138)
void audiolib_hw_post_process_work()
{
}

// 0x0048B2C0 (:1143)
audiolib_result audiolib_hw_thread_set_hw_thread(audiolib_thread_info* thread, int thread_no)
{
    (void)thread;
    (void)thread_no;
    return AUDIOLIB_ERR_NONE;
}

// 0x0048B2D0 (:1151)
bool audiolib_hw_is_streaming(audiolib_play_data* play)
{
    if (play == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_is_streaming> play is a nullptr! \n");
        return false;
    }
    return play->foley_data.wavebank_type == 1;
}

// 0x0048B300 (:1167)
short audiolib_hw_get_currently_playing_variant(audiolib_play_data* play)
{
    if (play == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_get_currently_playing_variant> play is a nullptr! \n");
        return -100;
    }
    return play->curr_variant;
}

// 0x0048B330 (:1178)
bool audiolib_hw_is_memory_avail_to_start_a_cue(int num_this_frame)
{
    (void)num_this_frame;
    return true;
}

// 0x0048B340 (:1185)
void audiolib_check_panic(audiolib_system_init_info* init_info)
{
    (void)init_info;
}

// 0x0048B350 (:1190)
bool audiolib_keenAudio_init(keen::SoundSystem::System* pSoundSystem)
{
    if (pSoundSystem == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_keenAudio_init> pSoundSystem is a nullptr!");
        return false;
    }
    m_pSoundSystem = pSoundSystem;
    m_audio_mutex.create("Audiolib_mutex");
    // Bus 0: dry send 1.0, wet send (to the reverb bus) 0; bus 1 (reverb): wet 1.0.
    keen::SoundSystem::setBusSendVolume(m_pSoundSystem, 0, 0, 1.0f, 0.0f, 0.0f);
    keen::SoundSystem::setBusSendVolume(m_pSoundSystem, 0, 1, 0.0f, 0.0f, 0.0f);
    keen::SoundSystem::setBusSendVolume(m_pSoundSystem, 1, 0, 0.0f, 1.0f, 0.0f);
    return true;
}

// 0x00492CB0 (:1208)
bool audiolib_process_inMemory_wavebank(audiolib_soundbank_data* pWaveBank)
{
    if (pWaveBank == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_process_inMemory_wavebank> pWaveBank is a nullptr! \n");
        return false;
    }
    if (pWaveBank->hw_bank_id.is_prepared)
    {
        return true;
    }
    if (pWaveBank->bank_type != AUDIOLIB_WAVEBANK)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_process_inMemory_wavebank> pWaveBank is not a AUDIOLIB_WAVEBANK Type! Type: %d \n", (int)pWaveBank->bank_type);
        return false;
    }
    if (pWaveBank->bank_buffer == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_process_inMemory_wavebank> pWaveBank bank_buffer is a nullptr! Name: %s \n", pWaveBank->bank_name);
        return false;
    }
    if (!audiolib_check_bankData_isConverted(pWaveBank))
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_process_inMemory_wavebank> check isConverted failed! \n");
        return false;
    }
    // Kaiko's converted .xwb_pc (SFX.md 1.3): the wave count (at most 1,000), then per wave 0x2C bytes: the cue name
    // crc, the keen SoundFileHeader (0x20), the data offset in the file and the data size.
    const unsigned int waveCount = *(unsigned int*)pWaveBank->bank_buffer;
    if (waveCount > 1000)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_process_inMemory_wavebank> Wave count is too high! %d / %d Name: %s \n", waveCount, 1000, pWaveBank->bank_name);
        return false;
    }
    audiolib_hw_soundbank_id& hw = pWaveBank->hw_bank_id;
    hw.waveCount = waveCount;
    keen::SoundFileFactory factory;
    factory.create();
    const unsigned int definitionsSize = waveCount * sizeof(keen::SoundSystem::TemporarySoundDefinition);
    const unsigned int filesSize = waveCount * sizeof(keen::SoundFile);
    // Raw zeroed memory, not constructed objects: TemporarySoundDefinition::create and initializeResource set them up.
    hw.tmpSoundDef2D = (keen::SoundSystem::TemporarySoundDefinition*)m_pAudioAllocator->allocate(definitionsSize, 16, 0, nullptr);
    hw.tmpSoundDef3D = (keen::SoundSystem::TemporarySoundDefinition*)m_pAudioAllocator->allocate(definitionsSize, 16, 0, nullptr);
    hw.soundFiles = (keen::SoundFile*)m_pAudioAllocator->allocate(filesSize, 16, 0, nullptr);
    memset(hw.tmpSoundDef2D, 0, definitionsSize);
    memset(hw.tmpSoundDef3D, 0, definitionsSize);
    memset(hw.soundFiles, 0, filesSize);
    for (unsigned int i = 0; i < waveCount; ++i)
    {
        unsigned char* entry = (unsigned char*)pWaveBank->bank_buffer + 4 + i * 0x2C;
        const int caller_cue_id = audiolib_foley_hash_table_get(*(unsigned int*)entry, nullptr);
        audiolib_foley_data* foley_data = audiolib_get_foley_data_pointer(caller_cue_id);
        if (foley_data == nullptr)
        {
            keen::debug::nativePrintArgs("[Audiolib] <audiolib_process_inMemory_wavebank> foley_data is a nullptr! \n");
            // Original: the definitions and files allocated so far stay (freed on unload).
            return false;
        }
        keen::SoundSystem::SoundDefinitionParameter3d parameter3d;
        parameter3d.minDistance = 1.0f;
        parameter3d.maxDistance = 50.0f;
        parameter3d.positionVariation = 0.0f;
        // The extended distances, whatever use_extended_min_max_distances says.
        parameter3d.minDistance = Audiolib_settings.min_max_distance_entries[foley_data->min_max_index].min_ext;
        parameter3d.maxDistance = Audiolib_settings.min_max_distance_entries[foley_data->min_max_index].max_ext;
        // The data offset becomes a pointer in place, so the entry after the crc is the file's initialization data
        // {header, pData, size}. Only the header's 0x20 bytes are given as its size.
        *(unsigned char**)(entry + 0x24) = (unsigned char*)pWaveBank->bank_buffer + *(unsigned int*)(entry + 0x24);
        keen::Resource* pResource = (keen::Resource*)&hw.soundFiles[i];
        hw.tmpSoundDef2D[i].create();
        hw.tmpSoundDef3D[i].create();
        if (!factory.initializeResource(pResource, 0, entry + 4, 0x20))
        {
            keen::debug::nativePrintArgs("[Audiolib] <audiolib_process_inMemory_wavebank> Failed to initialize Resource! \n");
            return false;
        }
        const keen::SoundFileHandleType* soundFile = (const keen::SoundFileHandleType*)pResource;
        hw.tmpSoundDef2D[i].setSoundFile(soundFile, 0, keen::SoundSystem::Positioning_listener, nullptr, nullptr, -1, 0, 0, 0);
        hw.tmpSoundDef3D[i].setSoundFile(soundFile, 0, keen::SoundSystem::Positioning_world, nullptr, &parameter3d, -1, 0, 0, 0);
    }
    factory.destroy();
    hw.is_prepared = true;
    return true;
}

// 0x00492FC0 (:1336)
bool audiolib_process_streaming_wavebank(audiolib_soundbank_data* pWaveBank)
{
    if (pWaveBank == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_process_streaming_wavebank> pWaveBank is a nullptr! \n");
        return false;
    }
    audiolib_hw_soundbank_id& hw = pWaveBank->hw_bank_id;
    if (hw.is_prepared)
    {
        return true;
    }
    if (hw.preopened == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_process_streaming_wavebank> cFile not preopened! Bankname: %s\n", pWaveBank->bank_name);
        return false;
    }
    // Only the wave table is read (same 0x2C entries as an in-memory bank); the waves are read per play.
    unsigned int* count = (unsigned int*)m_pAudioAllocator->allocate(4, 16, 0, nullptr);
    if (cf_read(count, 4, hw.preopened, false) == 0)
    {
        // Original: count leaks.
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_process_streaming_wavebank> Can't read File! Bankname: %s\n", pWaveBank->bank_name);
        return false;
    }
    const unsigned int waveCount = *count;
    if (waveCount > 1000)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_process_streaming_wavebank> Wave count is too high! %d / %d Name: %s \n", waveCount, 1000, pWaveBank->bank_name);
        return false;
    }
    hw.waveCount = waveCount;
    m_pAudioAllocator->free(count);
    unsigned int* entries = (unsigned int*)m_pAudioAllocator->allocate(waveCount * 0x2C, 16, 0, nullptr);
    if (cf_read(entries, waveCount * 0x2C, hw.preopened, false) == 0)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_process_streaming_wavebank> Can't read File! Bankname: %s\n", pWaveBank->bank_name);
        return false;
    }
    hw.streamingWaveData = (StreamingWavebankData*)m_pAudioAllocator->allocate(waveCount * sizeof(StreamingWavebankData), 16, 0, nullptr);
    hw.soundFiles = (keen::SoundFile*)m_pAudioAllocator->allocate(waveCount * sizeof(keen::SoundFile), 16, 0, nullptr);
    memset(hw.soundFiles, 0, waveCount * sizeof(keen::SoundFile));
    const unsigned int* entry = entries;
    for (unsigned int i = 0; i < waveCount; ++i, entry += 0x2C / 4)
    {
        StreamingWavebankData& wave = hw.streamingWaveData[i];
        wave.crcName = entry[0];
        memcpy(&wave.soundHeader, &entry[1], sizeof(wave.soundHeader));
        wave.offsetData = entry[9];
        wave.dataSize = entry[10];
        wave.pData = nullptr;
    }
    m_pAudioAllocator->free(entries);
    hw.is_prepared = true;
    return true;
}

// 0x0049E0C0 (:1427)
audiolib_result audiolib_prepare_inMemory_sound(audiolib_play_data* play)
{
    if (play == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_prepare_inMemory_sound> play is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    // audiolib_internal_soundbank_id_to_ptr(..., true) is inlined in rfg.exe.
    audiolib_soundbank_data* pBank = audiolib_internal_soundbank_id_to_ptr(play->foley_data.wavebank_id, true);
    if (pBank == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_prepare_inMemory_sound> pWaveBank is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_BANK_ID;
    }
    if (!pBank->hw_bank_id.is_prepared)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_prepare_inMemory_sound> Soundbank not prepared! %s \n", pBank->bank_name);
        return AUDIOLIB_ERR_HW_NOT_READY;
    }
    const int waveIndex = audiolib_get_waveIndex_from_soundBank(play);
    if (waveIndex < 0 || waveIndex >= (int)pBank->hw_bank_id.waveCount)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_prepare_inMemory_sound> invalid waveIndex from soundBank! \n");
        return AUDIOLIB_ERR_HW_NOT_READY;
    }
    play->hw_info.hw_curr_waveIndex = waveIndex;
    // Params flag 1: a 3D play.
    keen::SoundSystem::TemporarySoundDefinition* definitions = (play->params.flags & 1) != 0 ? pBank->hw_bank_id.tmpSoundDef3D : pBank->hw_bank_id.tmpSoundDef2D;
    play->hw_info.hw_play_id = (const keen::SoundDefinitionResourceHandleType*)&definitions[waveIndex];
    return AUDIOLIB_ERR_NONE;
}

// 0x0049E1D0 (:1471)
audiolib_result audiolib_prepare_streaming_sound(audiolib_play_data* play)
{
    if (play == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_prepare_streaming_sound> play is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    audiolib_soundbank_data* pBank = audiolib_internal_soundbank_id_to_ptr(play->foley_data.wavebank_id, true);
    if (pBank == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_prepare_streaming_sound> pWaveBank is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_BANK_ID;
    }
    audiolib_hw_soundbank_id& hw = pBank->hw_bank_id;
    if (!hw.is_prepared)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_prepare_streaming_sound> Soundbank not prepared! %s \n", pBank->bank_name);
        return AUDIOLIB_ERR_HW_NOT_READY;
    }
    const int waveIndex = audiolib_get_waveIndex_from_soundBank(play);
    if (waveIndex < 0 || waveIndex >= (int)hw.waveCount)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_prepare_streaming_sound> invalid waveIndex from soundBank! \n");
        return AUDIOLIB_ERR_HW_NOT_READY;
    }
    if (hw.preopened == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_prepare_streaming_sound> cFile not preopened! Bankname: %s\n", pBank->bank_name);
        return AUDIOLIB_ERR_HW_NOT_READY;
    }
    play->hw_info.hw_curr_waveIndex = waveIndex;
    StreamingWavebankData& wave = play->hw_info.streamingWaveData;
    wave = hw.streamingWaveData[waveIndex];
    // The whole wave in one allocation from the bank allocator (Kaiko's streaming, SFX.md 1.3).
    wave.pData = m_pAudioBankAllocator->allocate(wave.dataSize, 16, 0, nullptr);
    if (wave.pData == nullptr)
    {
        // Original: the %d (the size) is not passed.
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_prepare_streaming_sound> Can't prepare Sound %s, allocate %d bytes failed! \n", audiolib_internal_debug_foley_name(play->foley_data.name_crc));
        return AUDIOLIB_ERR_MEM_ALLOC_FAILED;
    }
    if (!cf_seek(hw.preopened, wave.offsetData, CF_SEEK_SET))
    {
        // Original: the data stays allocated until the play is destroyed.
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_prepare_streaming_sound> Can't Seek File! Sound: %s \n", audiolib_internal_debug_foley_name(play->foley_data.name_crc));
        return AUDIOLIB_ERR_HW_SOUNDBANK_LOAD_FAILED;
    }
    if (cf_read(wave.pData, wave.dataSize, hw.preopened, false) == 0)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_prepare_streaming_sound> Can't read File! Sound: %s\n", audiolib_internal_debug_foley_name(play->foley_data.name_crc));
        return AUDIOLIB_ERR_HW_SOUNDBANK_LOAD_FAILED;
    }
    keen::SoundFileFactory factory;
    factory.create();
    // Original: the bank's one SoundFile per wave is reinitialized with this play's data, so overlapping plays of a
    // wave share the newest data.
    keen::SoundFile* pResource = &hw.soundFiles[waveIndex];
    if (!factory.initializeResource((keen::Resource*)pResource, 0, &wave, 0x20))
    {
        // Original: reported, but the prepare succeeds without a definition (the play then fails to start).
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_prepare_streaming_sound> Failed to initialize Resource! \n");
        return AUDIOLIB_ERR_NONE;
    }
    const keen::SoundFileHandleType* soundFile = (const keen::SoundFileHandleType*)pResource;
    if ((play->params.flags & 1) == 0)
    {
        m_audio_mutex.lock(0);
        play->hw_info.hw_play_id = keen::SoundSystem::createDynamicSoundDefinition2d(m_pSoundSystem, soundFile, 0, nullptr, 0, -1, 0, 0);
    }
    else
    {
        keen::SoundSystem::SoundDefinitionParameter3d parameter3d;
        parameter3d.positionVariation = 0.0f;
        parameter3d.minDistance = Audiolib_settings.min_max_distance_entries[play->foley_data.min_max_index].min_ext;
        parameter3d.maxDistance = Audiolib_settings.min_max_distance_entries[play->foley_data.min_max_index].max_ext;
        m_audio_mutex.lock(0);
        play->hw_info.hw_play_id = keen::SoundSystem::createDynamicSoundDefinition3d(m_pSoundSystem, soundFile, 0, &parameter3d, 0, -1, 0, 0);
    }
    m_audio_mutex.unlock();
    if (play->hw_info.hw_play_id == nullptr)
    {
        return AUDIOLIB_ERR_HW_NOT_READY;
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x00498670 (:1613)
int audiolib_get_waveIndex_from_soundBank(audiolib_play_data* play)
{
    if (play == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_get_waveIndex_from_soundBank> play is a nullptr! \n");
        return -1;
    }
    audiolib_soundbank_data* pBank = audiolib_internal_soundbank_id_to_ptr(play->foley_data.soundbank_id, true);
    if (pBank == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_get_waveIndex_from_soundBank> pSoundBank is a nullptr! \n");
        return -1;
    }
    const char* buffer = pBank->bank_buffer;
    if (buffer == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_get_waveIndex_from_soundBank> pSoundBank bank_buffer is a nullptr! \n");
        return -1;
    }
    // The converted .xsb_pc: per cue an (offset, count) pair into a table of wave indices.
    const unsigned int offset = *(const unsigned int*)(buffer + play->hw_cue_id * 8);
    unsigned int waveCount = *(const unsigned int*)(buffer + play->hw_cue_id * 8 + 4);
    if (waveCount == 0)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_get_waveIndex_from_soundBank> WaveCount == 0. No sounds for this Index! \n");
        return -1;
    }
    if (waveCount > 15)
    {
        // Original: the message says 16, the clamp is 15.
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_get_waveIndex_from_soundBank> WaveCount is too high! Currently max %d! WaveCount = %d \n", 16, waveCount);
        waveCount = 15;
    }
    int waveIndices[16];
    memcpy(waveIndices, buffer + offset, waveCount * sizeof(int));
    // CRT rand() on the audio thread (SFX.md 2.5).
    return waveIndices[(unsigned int)rand() % waveCount];
}

// 0x004931C0 (:1673)
void audiolib_keen_soundsystem_update()
{
    // No callers in rfg.exe: audiolib_stream_thread_main has the same code inline.
    if (!Audiolib_state.in_shutdown && m_pSoundSystem != nullptr)
    {
        audiolib_update_listener_position();
        const float timeStep = keen::getLastFrameDurationInSeconds();
        m_audio_mutex.lock(0);
        keen::SoundSystem::update(m_pSoundSystem, timeStep);
        m_audio_mutex.unlock();
    }
}

// 0x0048B420 (:1689)
void audiolib_update_listener_position()
{
    // Volition's y-up listener as keen's z-up matrix (y and z swapped in every vector).
    keen::Matrix43 listener;
    listener.pos.x = Audiolib_state.listener_pos.x;
    listener.pos.y = Audiolib_state.listener_pos.z;
    listener.pos.z = Audiolib_state.listener_pos.y;
    listener.rot.x.x = Audiolib_state.listener_right_orient.x;
    listener.rot.x.y = Audiolib_state.listener_right_orient.z;
    listener.rot.x.z = Audiolib_state.listener_right_orient.y;
    listener.rot.y.x = Audiolib_state.listener_up_orient.x;
    listener.rot.y.y = Audiolib_state.listener_up_orient.z;
    listener.rot.y.z = Audiolib_state.listener_up_orient.y;
    listener.rot.z.x = Audiolib_state.listener_front_orient.x;
    listener.rot.z.y = Audiolib_state.listener_front_orient.z;
    listener.rot.z.z = Audiolib_state.listener_front_orient.y;
    m_audio_mutex.lock(0);
    keen::SoundSystem::setListenerPosition(m_pSoundSystem, listener);
    m_audio_mutex.unlock();
}

// 0x0048B550 (:1707)
bool audiolib_check_bankData_isConverted(audiolib_soundbank_data* pBankData)
{
    if (pBankData == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_check_bankData_isConverted> pBankData is a nullptr! \n");
        return false;
    }
    if (pBankData->bank_buffer == nullptr)
    {
        return true;
    }
    // The XACT bank signatures; strstr stops at the first zero byte, which the converted banks have within 4 bytes.
    if (strstr(pBankData->bank_buffer, "SDBK") == nullptr && strstr(pBankData->bank_buffer, "WBND") == nullptr)
    {
        return true;
    }
    keen::debug::nativePrintArgs("[Audiolib] <audiolib_check_bankData_isConverted> pBankData->Bank_buffer has old SDBK/WBND Data! Need to rebuild xap resources! \n");
    return false;
}

// 0x0048B5C0 (:1727)
bool OpenWaveBankFile(char* filePath, audiolib_soundbank_data* pWaveBank)
{
    (void)filePath;
    (void)pWaveBank;
    return true;
}

// 0x0048B5D0 (:1771)
void CloseWaveBankFile(cfile** file)
{
    (void)file;
}

// 0x0048B5E0 (:1787)
void DEBUG_audiolib_CheckThreadID()
{
}

// 0x0048B5F0 (:1992)
void audiolib_hw_cue_finished(audiolib_play_data* play)
{
    if (play == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_cue_finished> play is a nullptr! \n");
        return;
    }
    play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_STOPPED;
}

// 0x0048B610 (:2002)
audiolib_result audiolib_hw_release_channel(audiolib_play_data* play)
{
    (void)play;
    return AUDIOLIB_ERR_NONE;
}

// 0x0048B620 (:2009)
audiolib_result audiolib_hw_get_channel(audiolib_play_data* play)
{
    (void)play;
    return AUDIOLIB_ERR_NONE;
}

// 0x0048B630 (:2016)
void audiolib_hw_init_play_instance(audiolib_play_data* play)
{
    (void)play;
}

// 0x0048B640 (:2021)
unsigned int audiolib_get_num_speakers()
{
    return Audiolib_state.speaker_count;
}

// 0x0048B650 (:2075)
audiolib_result audiolib_internal_dsp_hw_setup(audiolib_play_data* play)
{
    if (play == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_internal_dsp_hw_setup> play is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    // One reverb for everything: the last play with a different room type sets it.
    if (play->reverb_in_use && m_currentReverbType != play->reverb_type)
    {
        m_currentReverbScale = play->reverb_scale;
        m_currentReverbType = play->reverb_type;
        keen::SoundSystem::setBusSendVolume(m_pSoundSystem, 0, 1, 0.0f, play->reverb_scale, 0.5f);
        audiolib_stub_reverb_definition reverb;
        reverb.parameter = m_ReverbPresets[m_currentReverbType];
        reverb.base.effectId = 0x42565242; // 'BRVB'
        reverb.base.parameterSize = sizeof(reverb.parameter);
        keen::SoundSystem::EffectChainSetup setup;
        setup.gains[0] = 1.0f;
        setup.pDefinitions[0] = &reverb.base;
        keen::SoundSystem::setBusEffectChain(m_pSoundSystem, 1, setup, 0.0f);
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x0048B7A0 (:2112)
audiolib_result audiolib_internal_dsp_hw_cleanup(audiolib_play_data* play)
{
    if (play == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_internal_dsp_hw_cleanup> play is a nullptr! \n");
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    return AUDIOLIB_ERR_NONE;
}
