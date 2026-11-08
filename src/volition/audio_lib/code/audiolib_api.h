// Original: volition/audio_lib/code/audiolib_api.h
// Implementation: volition/audio_lib/code/audiolib.cpp
//
// The public audio_lib interface (audiolib.cpp): system init/shutdown, speakers, volume scalars and the low health
// filter, listener position, field of view and raycasts, pausing, categories and ducking, cutscene volume, cue lookup by
// name, wildcard and category, cue play (2D/3D, with audiolib_params), and per-play queries and changes (state, pause,
// volume, pitch, low pass, pan, 2D/3D, emitters, audible range), stats, wavebank precaching, DLC and voice banks.
// Callbacks get an AUDIOLIB_CUE_CALLBACK_DATA (marker text and time). The audiolib_internal_* functions of audiolib.cpp
// are in audiolib_internal.h. Placement of the types and of the functions inferred (no audiolib.h in the PDB).
#pragma once

#include "keen/base/language.hpp"
#include "volition/audio_lib/code/audiolib_enums.h"
#include "volition/vlib/code/math/vector3.h"

class matrix;
class mempool;
class mempool_base;

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

// size 0x54
struct AUDIOLIB_CUE_CALLBACK_DATA
{
    void (*function)(AUDIOLIB_CUE_CALLBACK_DATA*); // 0x00
    unsigned short action; // 0x04
    int play_id; // 0x08
    void* user_data; // 0x0C
    int time; // 0x10
    char marker_text[64]; // 0x14
};

// size 0x8
struct audiolib_low_pass_info
{
    int freq; // 0x00
    float effect; // 0x04
};

// size 0x8
struct audiolib_reverb_info
{
    AUDIOLIB_REVERB_ROOM_TYPE room_type; // 0x00
    float effect; // 0x04
};

// size 0x10
struct audiolib_dsp_info
{
    audiolib_low_pass_info low_pass; // 0x00
    audiolib_reverb_info reverb; // 0x08
};

// size 0x134
struct audiolib_params
{
    audiolib_params(); // 0x0048CDE0 (.h:177)

    unsigned short flags; // 0x00
    unsigned short callback_flags; // 0x02
    void (*callback_func)(AUDIOLIB_CUE_CALLBACK_DATA*); // 0x04
    void* callback_data; // 0x08
    unsigned char priority; // 0x0C
    float volume; // 0x10
    float pitch; // 0x14
    float pan; // 0x18
    signed char rolloff_curve; // 0x1C
    float curve_midpoint; // 0x20
    unsigned short dsp_type; // 0x24
    audiolib_dsp_info dsp; // 0x28
    float distance_min; // 0x38
    float distance_max; // 0x3C
    unsigned int delay_ms; // 0x40
    unsigned int fade_ms; // 0x44
    unsigned int offset_ms; // 0x48
    vector pos; // 0x4C
    vector orient; // 0x58
    vector velocity; // 0x64
    short num_positions; // 0x70
    vector position_list[16]; // 0x74
};

// size 0xB0
struct audiolib_cue_info
{
    char name[64]; // 0x00
    int cue_id; // 0x40
    short category_id; // 0x44
    int far_variant; // 0x48
    float volume; // 0x4C
    float distance_min; // 0x50
    float distance_max; // 0x54
    bool distance_override; // 0x58
    bool always_2d; // 0x59
    int max_simultaneous; // 0x5C
    int source_channels; // 0x60
    int loop_count; // 0x64
    float lowest_pitch; // 0x68
    float highest_pitch; // 0x6C
    int play_time[16]; // 0x70
};

// size 0x14
struct audiolib_stats
{
    bool initialized; // 0x00
    bool enabled; // 0x01
    bool sounds_paused; // 0x02
    bool music_paused; // 0x03
    AUDIOLIB_SPEAKER_TYPE speakers; // 0x04
    float pos_x; // 0x08
    float pos_y; // 0x0C
    float pos_z; // 0x10
};

// size 0x28
struct audiolib_system_init_info
{
    const char* root_dir; // 0x00
    const char* table_dir; // 0x04
    const char* data_dir; // 0x08
    const char* settings_file; // 0x0C
    float max_distance_to_keep_sounds; // 0x10
    mempool* pool; // 0x14
    void* (*game_file_load_func)(char*, mempool*, void (*)(const char*, void*, unsigned char*, int), void (*)(const char*, void*)); // 0x18
    audiolib_result (*game_file_load_status_func)(void*); // 0x1C
    bool packfile_build; // 0x20
    unsigned int voice_banks_memory_size; // 0x24
};

bool audiolib_booting(); // 0x00483120 (audiolib.cpp:157)
bool audiolib_get_sound_enabled(); // 0x00483130 (audiolib.cpp:172)
bool audiolib_set_sound_enabled(bool enable); // 0x0048CF70 (audiolib.cpp:196)
void audiolib_mute_sound(bool mute); // 0x0048CFB0 (audiolib.cpp:208)
audiolib_result audiolib_system_init_minimal(); // 0x0048CFD0 (audiolib.cpp:220)
void audiolib_system_shutdown_minimal(); // 0x0048D000 (audiolib.cpp:229)
audiolib_result audiolib_system_init(audiolib_system_init_info* init_info, keen::MemoryAllocator* pAudioAllocator, keen::MemoryAllocator* pAudioBankAllocator, keen::SoundSystem::System* pSoundSystem, keen::LanguageId audio_lang); // 0x004A93D0 (audiolib.cpp:234)
void audiolib_system_shutdown(); // 0x004A91D0 (audiolib.cpp:620)
audiolib_result audiolib_set_speaker_type(AUDIOLIB_SPEAKER_TYPE type); // 0x00483150 (audiolib.cpp:690)
AUDIOLIB_SPEAKER_TYPE audiolib_get_speaker_type(); // 0x00483170 (audiolib.cpp:699)
audiolib_result audiolib_system_get_volume_scalar(signed char id, float* volume); // 0x00483180 (audiolib.cpp:706)
audiolib_result audiolib_system_set_volume_scalar(signed char id, float volume); // 0x0048D010 (audiolib.cpp:717)
audiolib_result audiolib_system_low_health_get_lowpass_adjustment(float* low_pass); // 0x004831C0 (audiolib.cpp:734)
audiolib_result audiolib_system_low_health_set_lowpass_adjustment(float low_pass); // 0x004831F0 (audiolib.cpp:744)
audiolib_result audiolib_system_low_health_get_volume_adjustment(float* volume); // 0x00483250 (audiolib.cpp:754)
audiolib_result audiolib_system_low_health_set_volume_adjustment(float volume); // 0x00483280 (audiolib.cpp:764)
audiolib_result audiolib_system_low_health_set_unaffected_category(short cat_id); // 0x004832E0 (audiolib.cpp:774)
audiolib_result audiolib_system_low_health_unset_unaffected_category(short cat_id); // 0x00483330 (audiolib.cpp:791)
bool audiolib_min_max_distance_get_extended(bool extended); // 0x00483380 (audiolib.cpp:808)
audiolib_result audiolib_min_max_distance_set_extended(bool extended); // 0x00483390 (audiolib.cpp:817)
void audiolib_reset_collision_timer(); // 0x004833B0 (audiolib.cpp:826)
bool audiolib_allow_collision_sounds(); // 0x004833C0 (audiolib.cpp:831)
audiolib_result audiolib_system_set_game_state(AUDIOLIB_GAME_STATE audio_game_state); // 0x004833D0 (audiolib.cpp:839)
AUDIOLIB_GAME_STATE audiolib_system_get_game_state(); // 0x00483440 (audiolib.cpp:869)
audiolib_result audiolib_get_listener_pos(vector* position, vector* orient, vector* velocity); // 0x00483460 (audiolib.cpp:880)
audiolib_result audiolib_set_listener_pos(const vector* position, const matrix* orient, const vector* velocity); // 0x0048D080 (audiolib.cpp:892)
float audiolib_get_listener_fov(); // 0x004834D0 (audiolib.cpp:899)
audiolib_result audiolib_set_listener_fov(float listener_fov); // 0x004834F0 (audiolib.cpp:905)
audiolib_result audiolib_listener_get_raycast_dir(AUDIOLIB_RAYCAST_DIRECTION raycast_dir, vector* direction); // 0x0048D0A0 (audiolib.cpp:914)
audiolib_result audiolib_listener_raycast(AUDIOLIB_RAYCAST_DIRECTION direction, int num_collisions, float* distances, vector* positions, audiolib_material_type* materials); // 0x0048D0E0 (audiolib.cpp:921)
audiolib_result audiolib_all_sounds_get_pause(bool* sounds, bool* music); // 0x00483510 (audiolib.cpp:966)
audiolib_result audiolib_all_sounds_pause(bool sounds, bool music); // 0x0049EF00 (audiolib.cpp:977)
audiolib_result audiolib_all_sounds_unpause(bool sounds, bool music); // 0x0049EF40 (audiolib.cpp:993)
audiolib_result audiolib_all_sounds_stop(bool sounds, bool music); // 0x004A8CC0 (audiolib.cpp:1009)
void audiolib_all_streaming_sounds_unload(); // 0x00499AD0 (audiolib.cpp:1020)
audiolib_result audiolib_category_get_id(const char* category_name, short* category_id); // 0x0048D200 (audiolib.cpp:1027)
audiolib_result audiolib_category_get_mute(short category_id, bool* mute); // 0x0048D240 (audiolib.cpp:1039)
audiolib_result audiolib_category_set_mute(short category_id, bool mute); // 0x0048D280 (audiolib.cpp:1050)
audiolib_result audiolib_category_get_pause(short category_id, bool* pause); // 0x0048D2C0 (audiolib.cpp:1062)
audiolib_result audiolib_category_set_pause(short category_id, bool pause); // 0x0048D300 (audiolib.cpp:1073)
audiolib_result audiolib_category_get_volume(short category_id, float* volume); // 0x0048D340 (audiolib.cpp:1084)
audiolib_result audiolib_category_set_volume(short category_id, float volume); // 0x0048D390 (audiolib.cpp:1095)
audiolib_result audiolib_category_stop(short category_id); // 0x0049EF80 (audiolib.cpp:1105)
audiolib_result audiolib_category_get_duck_volume(float* volume); // 0x00483540 (audiolib.cpp:1115)
audiolib_result audiolib_category_set_duck_volume(float volume); // 0x00483570 (audiolib.cpp:1126)
bool audiolib_ducking(); // 0x004835A0 (audiolib.cpp:1138)
audiolib_result audiolib_category_duck(short category_id, float volume); // 0x0048D3D0 (audiolib.cpp:1147)
audiolib_result audiolib_category_get_duck_id(short* category_id); // 0x004835B0 (audiolib.cpp:1160)
audiolib_result audiolib_category_duck_off(); // 0x0048D410 (audiolib.cpp:1172)
bool audiolib_in_cutscene(); // 0x004835E0 (audiolib.cpp:1183)
audiolib_result audiolib_cutscene_set_status(bool in_cutscene); // 0x0048D470 (audiolib.cpp:1190)
audiolib_result audiolib_cutscene_get_volume(float* volume); // 0x004835F0 (audiolib.cpp:1219)
audiolib_result audiolib_cutscene_set_volume(float volume); // 0x00483620 (audiolib.cpp:1229)
int audiolib_get_number_of_cues_by_wildcard(char* wildcard); // 0x0048D750 (audiolib.cpp:1319)
audiolib_result audiolib_get_first_cue_by_wildcard(char* wildcard, audiolib_cue_info* cue_info); // 0x0048D830 (audiolib.cpp:1350)
audiolib_result audiolib_get_next_cue_by_wildcard(audiolib_cue_info* cue_info); // 0x0048D950 (audiolib.cpp:1380)
int audiolib_get_number_of_cues_by_category(short cat_id); // 0x0048DA60 (audiolib.cpp:1408)
audiolib_result audiolib_get_first_cue_by_category(short cat_id, audiolib_cue_info* cue_info); // 0x0048DAC0 (audiolib.cpp:1431)
audiolib_result audiolib_get_next_cue_by_category(audiolib_cue_info* cue_info); // 0x0048DB50 (audiolib.cpp:1456)
audiolib_result audiolib_cue_get_id(const unsigned int cue_name_crc, int* caller_cue_id); // 0x0048DBE0 (audiolib.cpp:1584)
audiolib_result audiolib_cue_get_id(const char* cue_name, int* caller_cue_id); // 0x0048DC50 (audiolib.cpp:1618)
bool audiolib_cue_is_id_valid(int caller_cue_id); // 0x0049F0F0 (audiolib.cpp:1646)
audiolib_result audiolib_cue_get_info(int cue_id, audiolib_cue_info* cue_info); // 0x0048DC80 (audiolib.cpp:1665)
int audiolib_cue_is_looping(int cue_id); // 0x0048DCE0 (audiolib.cpp:1691)
int audiolib_cue_play(int cue_id, const audiolib_params* passed_params, audiolib_result* error); // 0x0049F120 (audiolib.cpp:1778)
// Deviation (SFX.md 3.1, not in rfg.pdb): the --audio-log test seam, null unless a program sets them. The lookups by
// name that succeed (the name and its caller cue id), and every audiolib_cue_play result (play id or -1, the error).
extern void (*Audiolib_cue_get_id_log_hook)(const char* cue_name, int caller_cue_id);
extern void (*Audiolib_cue_play_log_hook)(int cue_id, const audiolib_params* params, int play_id, audiolib_result error);
audiolib_result audiolib_cue_precache(int cue_id); // 0x0049FD00 (audiolib.cpp:2324)
int audiolib_cue_play_3d(int cue_id, const vector* pos, const vector* orient, const vector* velocity, audiolib_result* error); // 0x0049FD60 (audiolib.cpp:2334)
audiolib_result audiolib_play_get_state(int play_id, audiolib_play_state* state); // 0x00494910 (audiolib.cpp:2361)
bool audiolib_is_playing(int play_id, bool if_paused); // 0x00494950 (audiolib.cpp:2378)
audiolib_result audiolib_play_get_play_time(int play_id, int* play_time, int* variation); // 0x004949A0 (audiolib.cpp:2397)
audiolib_result audiolib_play_get_cue_id(int play_id, int* cue_id); // 0x004949E0 (audiolib.cpp:2407)
audiolib_result audiolib_play_add_emitter(int play_id, const vector* pos, const vector* orientation); // 0x00494A20 (audiolib.cpp:2419)
audiolib_result audiolib_play_get_pause(int play_id, bool* pause); // 0x00494A50 (audiolib.cpp:2431)
audiolib_result audiolib_play_set_pause(int play_id, bool pause); // 0x00494A90 (audiolib.cpp:2441)
audiolib_result audiolib_play_stop(int play_id); // 0x00494AC0 (audiolib.cpp:2451)
audiolib_result audiolib_play_fade_out(int play_id, const int fade_time_ms); // 0x00494AF0 (audiolib.cpp:2460)
audiolib_result audiolib_play_get_volume(int play_id, float* volume); // 0x00494B30 (audiolib.cpp:2476)
audiolib_result audiolib_play_set_volume(int play_id, const float volume, int fade_time_ms); // 0x00494B70 (audiolib.cpp:2486)
audiolib_result audiolib_play_get_pitch(int play_id, float* pitch); // 0x00494BB0 (audiolib.cpp:2495)
audiolib_result audiolib_play_set_pitch(int play_id, const float pitch); // 0x00494BE0 (audiolib.cpp:2504)
audiolib_result audiolib_play_get_low_pass(int play_id, float* low_pass); // 0x00494C20 (audiolib.cpp:2513)
audiolib_result audiolib_play_set_low_pass(int play_id, const float low_pass); // 0x00494C50 (audiolib.cpp:2521)
audiolib_result audiolib_play_get_pan(int play_id, float* pan); // 0x00494C90 (audiolib.cpp:2530)
audiolib_result audiolib_play_set_pan(int play_id, const float pan); // 0x00494CC0 (audiolib.cpp:2539)
bool audiolib_play_is_2d(int play_id); // 0x00494D00 (audiolib.cpp:2553)
audiolib_result audiolib_play_change_to_2d(int play_id, const float volume); // 0x00494D30 (audiolib.cpp:2562)
audiolib_result audiolib_play_change_to_3d(int play_id, const vector* pos, const vector* orientation, const vector* velocity); // 0x00494D70 (audiolib.cpp:2572)
audiolib_result audiolib_play_get_3d_position(int play_id, vector* pos, vector* orient, vector* velocity); // 0x00494DB0 (audiolib.cpp:2582)
audiolib_result audiolib_play_set_3d_position(int play_id, const vector* pos, const vector* orientation, const vector* velocity); // 0x00494DF0 (audiolib.cpp:2592)
audiolib_result audiolib_play_remove_emitter(int play_id, vector* pos); // 0x00494E30 (audiolib.cpp:2621)
audiolib_result audiolib_play_get_audible_range(int play_id, float* min, float* max); // 0x00494EA0 (audiolib.cpp:2640)
audiolib_result audiolib_play_set_audible_range(int play_id, float min, float max); // 0x00494ED0 (audiolib.cpp:2650)
void audiolib_stats_get(audiolib_stats* stats); // 0x004837A0 (audiolib.cpp:2660)
void audiolib_stats_reset(audiolib_stats* stats); // 0x00483820 (audiolib.cpp:2682)
bool audiolib_music_wavebank_ready(); // 0x00499B40 (audiolib.cpp:2801)
bool audiolib_enable_wavebank_precache(const char* filename, mempool_base* soundheaders_preload_pool_ptr, mempool_base* temp_mempool, unsigned int num_sound_files); // 0x004A5A60 (audiolib.cpp:2812)
bool audiolib_wavebank_precaching_enabled(); // 0x00483870 (audiolib.cpp:2843)
void audiolib_disable_wavebank_precache(); // 0x00483880 (audiolib.cpp:2848)
void audiolib_set_thread_ownership(); // 0x004838A0 (audiolib.cpp:2883)
bool audiolib_scalar_will_play(signed char scalar); // 0x004838C0 (audiolib.cpp:2890)
void audiolib_dlc_init(int bundle_num, mempool* pool, keen::LanguageId audio_lang); // 0x004A5CF0 (audiolib.cpp:2910)
void audiolib_dlc_shutdown(); // 0x004A93A0 (audiolib.cpp:2939)
void audiolib_voices_init(keen::LanguageId audio_lang); // 0x004A5E00 (audiolib.cpp:2946)
void audiolib_voices_shutdown(); // 0x004A93B0 (audiolib.cpp:2972)
