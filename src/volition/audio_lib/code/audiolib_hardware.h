// Original: volition/audio_lib/code/audiolib_hardware.h
// Implementation: volition/audio_lib/code/stub/audiolib_stub.cpp
//
// The hardware layer audio_lib calls (audiolib_hw_*): banks (load, register, streaming), cues (prepare, play, pause, stop,
// pitch, 3D update), channels and the per-frame work. The shipped game implements it in stub/audiolib_stub.cpp on top of
// the keen sound system. Placement inferred: the stub's other functions are in stub/audiolib_stub.h.
#pragma once

#include "volition/audio_lib/code/audiolib_enums.h"
#include "volition/audio_lib/code/audiolib_hardware_types.h"

struct audiolib_play_data;
struct audiolib_soundbank_data;
struct audiolib_thread_info;
class mempool;

namespace keen
{
    class MemoryAllocator;
}

namespace keen
{
    namespace SoundSystem
    {
        struct System;
    }
}

void audiolib_hw_soundbank_create_file_and_pathname(const char* in_name, const char* sub_dir, short bank_type, char* out_name, char* out_path); // 0x0048A600 (audiolib_stub.cpp:82)
audiolib_result audiolib_hw_3d_setup(audiolib_play_data* play); // 0x00492A00 (audiolib_stub.cpp:140)
audiolib_result audiolib_hw_2d_to_3d(audiolib_play_data* play); // 0x0048A790 (audiolib_stub.cpp:149)
audiolib_result audiolib_hw_3d_to_2d(audiolib_play_data* play); // 0x0048A7A0 (audiolib_stub.cpp:156)
audiolib_result audiolib_hw_3d_update(audiolib_play_data* play, audiolib_channel_to_speaker_map* output_map); // 0x0048A7B0 (audiolib_stub.cpp:163)
audiolib_result audiolib_hw_is_sound_bank_ready(audiolib_soundbank_data* pBank); // 0x0048A8B0 (audiolib_stub.cpp:220)
audiolib_result audiolib_hw_load_sound_bank(audiolib_soundbank_data* pBank, mempool* pool); // 0x004983C0 (audiolib_stub.cpp:236)
audiolib_result audiolib_hw_soundbank_load_file_into_RAM(audiolib_soundbank_data* pBank, const char* filename, mempool* pool); // 0x00492A10 (audiolib_stub.cpp:314)
audiolib_result audiolib_hw_soundbank_open_file_for_audio_streaming(audiolib_soundbank_data* pWaveBank, const char* filename); // 0x0048A8F0 (audiolib_stub.cpp:485)
void audiolib_hw_mute_all(bool mute); // 0x0048AA50 (audiolib_stub.cpp:573)
audiolib_result audiolib_hw_system_init_minimal(); // 0x0048AB30 (audiolib_stub.cpp:598)
void audiolib_hw_system_shutdown_minimal(); // 0x0048AB60 (audiolib_stub.cpp:612)
audiolib_result audiolib_hw_system_init_stage_1(keen::MemoryAllocator* pAudioAllocator, keen::MemoryAllocator* pAudioBankAllocator, keen::SoundSystem::System* pSoundSystem); // 0x00492C50 (audiolib_stub.cpp:617)
audiolib_result audiolib_hw_system_init_stage_2(); // 0x004984E0 (audiolib_stub.cpp:638)
void audiolib_hw_system_shutdown(); // 0x0048AB70 (audiolib_stub.cpp:675)
audiolib_result audiolib_hw_register_soundbank(audiolib_soundbank_data* pBank); // 0x00498610 (audiolib_stub.cpp:692)
audiolib_result audiolib_hw_unload_sound_bank(audiolib_soundbank_data* pBank); // 0x0048AB80 (audiolib_stub.cpp:716)
audiolib_result audiolib_hw_unregister_soundbank(audiolib_soundbank_data* pBank); // 0x0048ACB0 (audiolib_stub.cpp:787)
audiolib_result audiolib_hw_get_cue_index(audiolib_soundbank_data* pBank, const char* cue_name, int* cue_index); // 0x0048ACF0 (audiolib_stub.cpp:804)
audiolib_result audiolib_hw_cue_is_valid(audiolib_soundbank_data* pBank, int cue_index); // 0x0048AD60 (audiolib_stub.cpp:834)
audiolib_result audiolib_hw_prepare(audiolib_play_data* play, int offset_ms); // 0x004A5980 (audiolib_stub.cpp:858)
audiolib_result audiolib_hw_destroy(audiolib_play_data* play); // 0x0048ADB0 (audiolib_stub.cpp:883)
audiolib_result audiolib_hw_stop(audiolib_play_data* play); // 0x0048AE70 (audiolib_stub.cpp:923)
audiolib_result audiolib_hw_play(audiolib_play_data* play, char* track_data_ptr, int length); // 0x0048AF10 (audiolib_stub.cpp:960)
audiolib_result audiolib_hw_pause(audiolib_play_data* play, bool pause); // 0x0048B0B0 (audiolib_stub.cpp:1018)
audiolib_hw_play_state audiolib_hw_play_get_audiolib_state(audiolib_play_data* play); // 0x0048B1A0 (audiolib_stub.cpp:1070)
audiolib_result audiolib_hw_set_pitch(audiolib_play_data* play, float new_pitch); // 0x0048B1F0 (audiolib_stub.cpp:1089)
void audiolib_hw_pre_process_work(); // 0x0048B2A0 (audiolib_stub.cpp:1133)
void audiolib_hw_post_process_work(); // 0x0048B2B0 (audiolib_stub.cpp:1138)
audiolib_result audiolib_hw_thread_set_hw_thread(audiolib_thread_info* thread, int thread_no); // 0x0048B2C0 (audiolib_stub.cpp:1143)
bool audiolib_hw_is_streaming(audiolib_play_data* play); // 0x0048B2D0 (audiolib_stub.cpp:1151)
short audiolib_hw_get_currently_playing_variant(audiolib_play_data* play); // 0x0048B300 (audiolib_stub.cpp:1167)
bool audiolib_hw_is_memory_avail_to_start_a_cue(int num_this_frame); // 0x0048B330 (audiolib_stub.cpp:1178)
void audiolib_hw_cue_finished(audiolib_play_data* play); // 0x0048B5F0 (audiolib_stub.cpp:1992)
audiolib_result audiolib_hw_release_channel(audiolib_play_data* play); // 0x0048B610 (audiolib_stub.cpp:2002)
audiolib_result audiolib_hw_get_channel(audiolib_play_data* play); // 0x0048B620 (audiolib_stub.cpp:2009)
void audiolib_hw_init_play_instance(audiolib_play_data* play); // 0x0048B630 (audiolib_stub.cpp:2016)
