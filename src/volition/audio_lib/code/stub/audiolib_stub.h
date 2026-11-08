// Original: volition/audio_lib/code/stub/audiolib_stub.h
// Implementation: volition/audio_lib/code/stub/audiolib_stub.cpp
//
// The keen sound system backend of the shipped game besides the audiolib_hw_* layer (audiolib_hardware.h): keen audio
// init and update, in-memory and streaming wavebank processing and the wavebank files. Placement inferred.
#pragma once

#include "volition/audio_lib/code/audiolib_enums.h"

struct audiolib_play_data;
struct audiolib_soundbank_data;
struct audiolib_system_init_info;
struct cfile;

namespace keen
{
    namespace SoundSystem
    {
        struct System;
    }
}

void audiolib_check_panic(audiolib_system_init_info* init_info); // 0x0048B340 (:1185)
bool audiolib_keenAudio_init(keen::SoundSystem::System* pSoundSystem); // 0x0048B350 (:1190)
bool audiolib_process_inMemory_wavebank(audiolib_soundbank_data* pWaveBank); // 0x00492CB0 (:1208)
bool audiolib_process_streaming_wavebank(audiolib_soundbank_data* pWaveBank); // 0x00492FC0 (:1336)
audiolib_result audiolib_prepare_inMemory_sound(audiolib_play_data* play); // 0x0049E0C0 (:1427)
audiolib_result audiolib_prepare_streaming_sound(audiolib_play_data* play); // 0x0049E1D0 (:1471)
int audiolib_get_waveIndex_from_soundBank(audiolib_play_data* play); // 0x00498670 (:1613)
void audiolib_keen_soundsystem_update(); // 0x004931C0 (:1673)
void audiolib_update_listener_position(); // 0x0048B420 (:1689)
bool audiolib_check_bankData_isConverted(audiolib_soundbank_data* pBankData); // 0x0048B550 (:1707)
bool OpenWaveBankFile(char* filePath, audiolib_soundbank_data* pWaveBank); // 0x0048B5C0 (:1727)
void CloseWaveBankFile(cfile** file); // 0x0048B5D0 (:1771)
void DEBUG_audiolib_CheckThreadID(); // 0x0048B5E0 (:1787)
unsigned int audiolib_get_num_speakers(); // 0x0048B640 (:2021)
audiolib_result audiolib_internal_dsp_hw_setup(audiolib_play_data* play); // 0x0048B650 (:2075)
audiolib_result audiolib_internal_dsp_hw_cleanup(audiolib_play_data* play); // 0x0048B7A0 (:2112)
