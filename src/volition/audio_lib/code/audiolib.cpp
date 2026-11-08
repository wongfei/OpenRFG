// Original: volition/audio_lib/code/audiolib.cpp
//
// The audio_lib front end (SFX.md SX2): system init/shutdown (settings, threads, hardware, every sub-system, the
// soundbank and voice tables, the foley data), the global switches (enable, mute, speaker type, user volume scalars,
// low health, game state, listener, cutscene), the category and cue queries, audiolib_cue_play (every refusal sets
// Audiolib_state.last_error), the per-play API over the play list, the precached wavebank headers and the DLC and
// voice banks. Compiled in audio_lib_unity.cpp (first, so the shared statics of the unity TU start here).

#include "volition/audio_lib/code/audiolib_api.h"

#include "keen/base/language.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/mutex.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_att_loop_rel.h"
#include "volition/audio_lib/code/audiolib_callbacks.h"
#include "volition/audio_lib/code/audiolib_dsp.h"
#include "volition/audio_lib/code/audiolib_foley_category.h"
#include "volition/audio_lib/code/audiolib_hardware.h"
#include "volition/audio_lib/code/audiolib_internal.h"
#include "volition/audio_lib/code/audiolib_markers.h"
#include "volition/audio_lib/code/audiolib_music.h"
#include "volition/audio_lib/code/audiolib_parse.h"
#include "volition/audio_lib/code/audiolib_soundbank.h"
#include "volition/audio_lib/code/audiolib_stream.h"
#include "volition/audio_lib/code/audiolib_thread.h"
#include "volition/audio_lib/code/audiolib_vehicle_engine.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/cfile/packfile.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/sync_event.h"
#include "volition/vlib/code/os/thread.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/string_util.h"

#include <ctype.h>
#include <direct.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// ---- file-local types (rfg.pdb)
// size 0x8
struct preloaded_soundheader_data
{
    unsigned char* data; // 0x00
    unsigned int size; // 0x04
};
static_assert(sizeof(preloaded_soundheader_data) == 0x8, "preloaded_soundheader_data");

// size 0xC
struct audiolib_veh_engine_lookup_data_list
{
    int count; // 0x00
    int total; // 0x04
    audiolib_vehicle_engine_data* engine_list; // 0x08
};
static_assert(sizeof(audiolib_veh_engine_lookup_data_list) == 0xC, "audiolib_veh_engine_lookup_data_list");

// ---- public globals (rfg.pdb; no header declares them)
// The defaults until foley_settings.xtbl is read (rfg.exe .data).
audiolib_global_settings Audiolib_settings = { // 0x014F79B8
    1.0f, 343.5f, 5.0f, 100.0f, 0.8f, 500, 30.0f, 0.5f, 500,
    1.0f, 50.0f, 200.0f, 0.75f, 1000.0f,
    3, 0.1f, 1.0f, 0.9f,
    45.0f, 0.9f,
    10.0f, 30.0f, 0.75f, 0,
    0.3f, 0.3f, 0.9f, 0.75f, 10.0f,
    {2.0f, 60.0f, 1.0f, 0.5f, -60.0f, 0.5f, 0.1f},
    0.2f, 0.1f,
    // The user volume scalars; scalar 7 is off.
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f},
};
volatile bool Audiolib_Stream_Shutdown; // 0x01A79A8A
volatile bool Audiolib_panicking; // 0x01A7AEEF
keen::MemoryAllocator* m_pAudioBankAllocator; // 0x01A7AF08
bool Audiolib_panicking_deferred_enabled_state; // 0x01A7AF10
keen::Mutex m_audio_mutex; // 0x01A839E0
cs_wrapper Audiolib_engine_cs(0); // 0x01A83A18
Audiolib_status Audiolib_state; // 0x01A97BF0

// ---- file-static globals
static int audiolib_output_channels[3] = {2, 6, 8}; // 0x014F9110
static char last_matching_string_default[2] = "*"; // 0x014F911C
// The same cue never starts twice within 100 ms.
static bool s_audiolib_checkLastPlayed = true; // 0x014F911E
static float s_audiolib_checkLastPlayed_Time = 100.0f; // 0x014F9120
static dynamic_string_hash_table<preloaded_soundheader_data,64> soundheaders_hash_table; // 0x014F9A78
static timestamp Audiolib_collision_timer; // 0x014F9AC0, invalid (-1) from the constructor
static audiolib_thread_info audiolib_callback_thread_info; // 0x014F9AC8
static audiolib_thread_info Stream_thread_info; // 0x014F9C60
static audiolib_thread_info Audio_thread; // 0x014F9CB8
static bool Audiolib_veh_engine_inited; // 0x01A789C4
static bool Audio_fov_Inited; // 0x01A789C5
static bool Audio_DSP_Inited; // 0x01A789C7
static audiolib_interactive_music_data Audiolib_Music; // 0x01A789C8
static short last_matching_cat_id; // 0x01A79A88
static bool Audiolib_markers_inited; // 0x01A79A8B
static int last_matching_string_index; // 0x01A79A90
static char last_cue_played_name[64]; // 0x01A79A98
static audiolib_veh_engine_lookup_data_list Audiolib_Engine_lookup_list; // 0x01A79AE0
static int last_matching_cat_index; // 0x01A79AEC
static char curr_cue_name[64]; // 0x01A79BC0
static char last_matching_string[128]; // 0x01A79C40
static bool Audiolib_Booting; // 0x01A7AEE0
static bool Audiolib_Wavebank_Precache_Enabled; // 0x01A7AEE1
static bool Audiolib_state_muted; // 0x01A7AEE2
static volatile bool Audiolib_hw_init_min; // 0x01A7AEED
static sync_event Audiolib_Marker_Trigger(false, false); // 0x01A7AF18
static sync_event Audiolib_Stream_Trigger(false, false); // 0x01A7AF1C
static object_pool<audiolib_internal_eventqueue_data,250,9> EventQueue_list(nullptr, &audiolib_internal_eventqueue_data::invalidate, 0x4A); // 0x01A7E530
static cs_wrapper Audiolib_EventQueue_list_cs(0); // 0x01A811EC
static mempool Audio_voice_banks_mempool; // 0x01A81228
static object_pool<audiolib_soundbank_data,252,5> Soundbank_list(nullptr, &audiolib_soundbank_data::drop, 0x49); // 0x01A83AD0
static object_pool<audiolib_vehicle_engine_instance,30,4> Active_engine_list(nullptr, &audiolib_vehicle_engine_instance::drop, 0x3E); // 0x01A9BA20
static cs_wrapper Audiolib_wavebank_precache_cs(0); // 0x01A9D0A8

// 0x004830D0
audiolib_hw_specific_info::audiolib_hw_specific_info()
{
    streamingWaveData.pData = nullptr;
}

// 0x0048CDE0 (.h:177)
audiolib_params::audiolib_params()
{
    // Original: callback_func is left uninitialized.
    volume = 0.0f;
    pitch = 0.0f;
    pan = 0.0f;
    curve_midpoint = 0.0f;
    distance_min = 0.0f;
    distance_max = 0.0f;
    flags = 0;
    callback_flags = 0;
    callback_data = nullptr;
    // Medium priority.
    priority = 4;
    rolloff_curve = 0;
    delay_ms = 0;
    fade_ms = 0;
    offset_ms = 0;
    dsp_type = 0;
    pos = ZERO_VECTOR;
    orient = ZERO_VECTOR;
    velocity = ZERO_VECTOR;
    num_positions = 0;
    dsp.low_pass.freq = 0;
    dsp.low_pass.effect = 0.0f;
    dsp.reverb.room_type = AUDIOLIB_REVERB_ROOM_TYPE_DEFAULT;
    dsp.reverb.effect = 0.0f;
    memset(position_list, 0, sizeof(position_list));
}

// 0x00483100
audiolib_raycast_data::audiolib_raycast_data()
{
}

// 0x0048CF40
Audiolib_status::Audiolib_status()
{
}

// A refused play: the error is kept in Audiolib_state.last_error and given back through error.
static int audiolib_cue_play_refused(audiolib_result result, audiolib_result* error)
{
    Audiolib_state.last_error = result;
    if (error != nullptr)
    {
        *error = result;
    }
    return -1;
}

// The caller cue id of a foley entry (index | address << 16).
static int audiolib_caller_cue_id(const audiolib_foley_data* foley)
{
    return (int)((unsigned int)(foley - Audiolib_foley_list.foley) & 0xFFFF) | (int)((unsigned int)(uintptr_t)foley << 16);
}

// 0x00483120 (:157)
bool audiolib_booting()
{
    return Audiolib_Booting;
}

// 0x00483130 (:172)
bool audiolib_get_sound_enabled()
{
    if (Audiolib_panicking)
    {
        return Audiolib_panicking_deferred_enabled_state;
    }
    return Audiolib_state.initialized && Audiolib_state.enabled;
}

// 0x0048CF60 (:185)
static void audiolib_internal_update_sound_enabled()
{
    audiolib_hw_mute_all(Audiolib_state_muted);
}

// 0x0048CF70 (:196)
bool audiolib_set_sound_enabled(bool enable)
{
    // A panic (device loss) keeps the request for later.
    if (Audiolib_panicking)
    {
        Audiolib_panicking_deferred_enabled_state = enable;
        return enable;
    }
    Audiolib_state.enabled = enable;
    audiolib_internal_update_sound_enabled();
    return Audiolib_state.enabled;
}

// 0x0048CFB0 (:208)
void audiolib_mute_sound(bool mute)
{
    Audiolib_state_muted = mute;
    audiolib_hw_mute_all(mute);
}

// 0x0048CFD0 (:220)
audiolib_result audiolib_system_init_minimal()
{
    if (!Audiolib_hw_init_min)
    {
        Audiolib_state.speaker_type = AUDIOLIB_SPEAKER_TYPE_STEREO;
        Audiolib_state.speaker_count = audiolib_output_channels[0];
        Audiolib_hw_init_min = true;
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x0048D000 (:229)
void audiolib_system_shutdown_minimal()
{
    Audiolib_hw_init_min = false;
}

// A voice bank table name for a language: lower case, '-' as '_' ("soundbanks_voice_en_us.xtbl").
static void audiolib_language_table_name(char (&name)[64], const char* format, keen::LanguageId language)
{
    keen::formatString(name, sizeof(name), format, keen::getLanguageIdString(language));
    keen::changeStringToLowercase(name);
    keen::replaceStringCharacters(name, '-', '_');
}

// 0x004A93D0 (:234)
audiolib_result audiolib_system_init(audiolib_system_init_info* init_info, keen::MemoryAllocator* pAudioAllocator, keen::MemoryAllocator* pAudioBankAllocator,
                                     keen::SoundSystem::System* pSoundSystem, keen::LanguageId audio_lang)
{
    Audiolib_Booting = true;
    if (Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NONE;
    }
    if (init_info == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (init_info->pool == nullptr)
    {
        return AUDIOLIB_ERR_NO_MEMPOOL;
    }
    atexit(audiolib_system_shutdown);
    memset(&Audiolib_state, 0, sizeof(Audiolib_state));
    Audiolib_state.low_health_low_pass_adjust = 1.0f;
    Audiolib_state.low_health_volume_adjust = 1.0f;
    for (short& cat_id : Audiolib_state.low_health_cat_ids)
    {
        cat_id = -1;
    }
    Audiolib_state.root_dir[0] = '\0';
    if (init_info->root_dir == nullptr)
    {
        // The working directory, with a trailing backslash.
        _getcwd(Audiolib_state.root_dir, sizeof(Audiolib_state.root_dir));
        if (Audiolib_state.root_dir[strlen(Audiolib_state.root_dir) - 1] != '\\')
        {
            strcat(Audiolib_state.root_dir, "\\");
        }
    }
    else if (strstr(init_info->root_dir, "\\\\") == nullptr && strstr(init_info->root_dir, ":") == nullptr)
    {
        // A relative root is appended (to the empty root_dir: a copy either way).
        strcat(Audiolib_state.root_dir, init_info->root_dir);
    }
    else
    {
        strcpy(Audiolib_state.root_dir, init_info->root_dir);
    }
    Audiolib_state.table_dir = init_info->table_dir != nullptr ? init_info->table_dir : "tables\\";
    Audiolib_state.data_dir = init_info->data_dir != nullptr ? init_info->data_dir : "audio\\pc\\";
    Audiolib_state.settings_file = init_info->settings_file;
    Audiolib_state.pool = init_info->pool;
    Audiolib_state.game_file_load_func = init_info->game_file_load_func;
    Audiolib_state.game_file_load_status_func = init_info->game_file_load_status_func;
    // Sounds out of range are kept up to 500 m by default.
    Audiolib_state.max_distance_to_keep_sounds = 500.0f;
    if (0.0f < init_info->max_distance_to_keep_sounds)
    {
        Audiolib_state.max_distance_to_keep_sounds = init_info->max_distance_to_keep_sounds;
    }
    // Original: the speaker count was just cleared, so this is always stereo.
    if (Audiolib_state.speaker_count != 2 && (unsigned int)Audiolib_state.speaker_count > 6)
    {
        Audiolib_state.speaker_type = AUDIOLIB_SPEAKER_TYPE_7_1;
        Audiolib_state.speaker_count = audiolib_output_channels[2];
    }
    else if (Audiolib_state.speaker_count != 2 && (unsigned int)Audiolib_state.speaker_count > 4)
    {
        Audiolib_state.speaker_type = AUDIOLIB_SPEAKER_TYPE_5_1;
        Audiolib_state.speaker_count = audiolib_output_channels[1];
    }
    else
    {
        Audiolib_state.speaker_count = audiolib_output_channels[0];
        Audiolib_state.speaker_type = AUDIOLIB_SPEAKER_TYPE_STEREO;
    }
    Audiolib_state.position_valid = false;
    Audiolib_state.listener_pos = vector(0.0f, 0.0f, 0.0f);
    Audiolib_state.listener_velocity = vector(0.0f, 0.0f, 0.0f);
    // Facing +x; up and right turned from it by -1.57 rad.
    Audiolib_state.listener_front_orient = vector(1.0f, 0.0f, 0.0f);
    Audiolib_state.listener_front_orient.normalize();
    Audiolib_state.listener_up_orient = Audiolib_state.listener_front_orient;
    Audiolib_state.listener_up_orient.rotate_around_z_axis(-1.57f);
    Audiolib_state.listener_up_orient.normalize();
    Audiolib_state.listener_right_orient = Audiolib_state.listener_front_orient;
    Audiolib_state.listener_right_orient.rotate_around_y_axis(-1.57f);
    Audiolib_state.listener_right_orient.normalize();
    Audiolib_state.initialized = false;
    Audiolib_state.init_failed = false;
    // Original: the low pass adjustment set to 1 above ends at 0.
    Audiolib_state.low_health_low_pass_adjust = 0.0f;
    Audiolib_state.hw_inited = false;
    Audiolib_state.hw_init_failed = false;
    Audiolib_state.low_health_volume_adjust = 1.0f;
    Audiolib_state.audio_game_state = AUDIOLIB_GAME_STATE_UNITIALIZED;
    Audiolib_state.packfile_build = init_info->packfile_build;
    if (audiolib_parse_settings_xtbl("foley_settings.xtbl", true) != AUDIOLIB_ERR_NONE)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    Audiolib_state.curr_duck_volume = 1.0f;
    Audiolib_state.ducking = false;
    Audiolib_state.duck_id = -1;
    Audiolib_state.duck_volume = Audiolib_settings.duck_volume;
    Audiolib_state.duck_fade_time = Audiolib_settings.duck_fade_time;
    if (audiolib_thread_system_init() < 0)
    {
        audiolib_system_shutdown();
        return AUDIOLIB_ERR_THREAD_FAILED;
    }
    if (audiolib_hw_system_init_stage_1(pAudioAllocator, pAudioBankAllocator, pSoundSystem) < 0)
    {
        return AUDIOLIB_ERR_HW_INIT_FAILED;
    }
    // Original: a failing sub-system leaves the ones before it running.
    if (!audiolib_internal_callback_init() || audiolib_stream_init_stage_1() < 0 || !audiolib_dsp_system_init() ||
        !audiolib_internal_eventqueue_system_init() || !audiolib_internal_soundbank_system_init() ||
        !audiolib_internal_playlist_system_init() || !audiolib_foley_category_system_init() || !audiolib_marker_system_init() ||
        !audiolib_3d_system_init())
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    // The space_used calls around each table are left from a memory report.
    Audiolib_state.pool->space_used();
    audiolib_parse_soundbank_xtbl("soundbanks.xtbl", false, false, nullptr);
    Audiolib_state.pool->space_used();
    Audiolib_state.pool->space_used();
    audiolib_parse_wavebank_xtbl("wavebanks.xtbl", false, false);
    Audiolib_state.pool->space_used();
    // The voice banks of the language get their own pool (16-byte aligned).
    void* voice_memory = pAudioBankAllocator->allocate(init_info->voice_banks_memory_size, 0x10, 0, nullptr);
    if (voice_memory != nullptr)
    {
        Audio_voice_banks_mempool.init(voice_memory, init_info->voice_banks_memory_size, "audio voice banks", 0x10, 8);
        char name[64];
        audiolib_language_table_name(name, "soundbanks_voice_%s.xtbl", audio_lang);
        Audiolib_state.pool->space_used();
        audiolib_parse_soundbank_xtbl(name, true, false, &Audio_voice_banks_mempool);
        Audiolib_state.pool->space_used();
        audiolib_language_table_name(name, "wavebanks_voice_%s.xtbl", audio_lang);
        Audiolib_state.pool->space_used();
        audiolib_parse_wavebank_xtbl(name, true, false);
        Audiolib_state.pool->space_used();
    }
    if (audiolib_load_category_and_foley_data(false) < 0)
    {
        return AUDIOLIB_ERR_HW_INIT_FAILED;
    }
    if (audiolib_parse_settings_xtbl("foley_settings.xtbl", false) != AUDIOLIB_ERR_NONE)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    audiolib_fov_system_init();
    Audiolib_state.music_cat_id = audiolib_internal_get_category_id("Music");
    Audiolib_state.system_cat_id = audiolib_internal_get_category_id("SYSTEM");
    Audiolib_state.gur_radio_cat_id = audiolib_internal_get_category_id("VOICE_GUR_RADIO");
    Audiolib_state.gur_pc_cat_id = audiolib_internal_get_category_id("VOICE_GUR_PC");
    Audiolib_state.number_of_banks = audiolib_internal_soundbank_get_count();
    if (audiolib_hw_system_init_stage_2() < 0)
    {
        return AUDIOLIB_ERR_HW_INIT_FAILED;
    }
    for (int i = 0; i < 8; ++i)
    {
        Audiolib_state.volume_scalars[i] = Audiolib_settings.user_volume_defaults[i];
    }
    Audiolib_state.dlc_id = -1;
    Audiolib_state.initialized = true;
    audiolib_interactive_music_system_init();
    audiolib_alr_system_init();
    audiolib_internal_vehicle_engine_system_init();
    return AUDIOLIB_ERR_NONE;
}

// 0x004A91D0 (:620)
void audiolib_system_shutdown()
{
    if (Audiolib_state.in_shutdown)
    {
        return;
    }
    Audiolib_state.in_shutdown = true;
    if (Audiolib_state.initialized)
    {
        audiolib_internal_playlist_stop(true, true);
        Audiolib_state.sounds_paused = false;
        Audiolib_state.music_paused = false;
    }
    audiolib_internal_playlist_system_shutdown();
    // The Main Audio Thread (0xFFFF: no thread).
    if (Audio_thread.handle != 0xFFFF)
    {
        Audio_thread.shutdown = true;
        thread_wait_until_done(Audio_thread.handle);
        Audio_thread.handle = 0xFFFF;
    }
    Audiolib_state.enabled = false;
    Audiolib_state.initialized = false;
    // audiolib_internal_vehicle_engine_system_shutdown, inlined.
    Audiolib_engine_cs.enter();
    Audiolib_veh_engine_inited = false;
    audiolib_thread_function_unregister(audiolib_process_vehicle_engines);
    Audiolib_Engine_lookup_list.count = 0;
    Audiolib_Engine_lookup_list.total = 0;
    Audiolib_Engine_lookup_list.engine_list = nullptr;
    Active_engine_list.clear();
    Audiolib_engine_cs.leave();
    audiolib_alr_system_shutdown();
    // audiolib_fov_system_shutdown, inlined.
    Audio_fov_Inited = false;
    // audiolib_interactive_music_system_shutdown, inlined.
    if (Audiolib_Music.inited)
    {
        audiolib_interactive_music_stop(Audiolib_Music.current_music_id, 0);
        audiolib_thread_function_unregister(audiolib_process_music);
        Audiolib_Music.inited = false;
    }
    // audiolib_dsp_system_shutdown and audiolib_3d_system_shutdown, inlined.
    Audio_DSP_Inited = false;
    audiolib_curve_list_num_curves = 0;
    memset(audiolib_rolloff_curve_data_list, 0, sizeof(audiolib_rolloff_curve_data_list));
    // audiolib_marker_system_shutdown and audiolib_foley_category_system_shutdown, inlined.
    Audiolib_markers_inited = false;
    audiolib_thread_function_unregister(audiolib_foley_category_process);
    // audiolib_stream_shutdown, inlined: the stream thread, then the marker callback thread.
    audiolib_thread_function_unregister(audiolib_stream_process_deferred_events);
    Audiolib_Stream_Shutdown = true;
    Audiolib_Stream_Trigger.signal();
    thread_wait_until_done(Stream_thread_info.handle);
    audiolib_callback_thread_info.shutdown = true;
    Audiolib_Marker_Trigger.signal();
    thread_wait_until_done(audiolib_callback_thread_info.handle);
    audiolib_internal_soundbank_system_shutdown();
    // audiolib_internal_eventqueue_system_shutdown, inlined.
    Audiolib_EventQueue_list_cs.enter();
    EventQueue_list.clear();
    Audiolib_EventQueue_list_cs.leave();
    // Original: without a stage 1 hardware init the bank allocator is null here.
    Audio_voice_banks_mempool.set_thread_ownership();
    Audio_voice_banks_mempool.clear();
    m_pAudioBankAllocator->free(Audio_voice_banks_mempool.get_base());
    Audio_voice_banks_mempool.destroy();
    m_audio_mutex.destroy();
    Audiolib_Booting = false;
}

// 0x00483150 (:690)
audiolib_result audiolib_set_speaker_type(AUDIOLIB_SPEAKER_TYPE type)
{
    Audiolib_state.speaker_type = type;
    Audiolib_state.speaker_count = audiolib_output_channels[type];
    return AUDIOLIB_ERR_NONE;
}

// 0x00483170 (:699)
AUDIOLIB_SPEAKER_TYPE audiolib_get_speaker_type()
{
    return Audiolib_state.speaker_type;
}

// 0x00483180 (:706)
audiolib_result audiolib_system_get_volume_scalar(int8_t id, float* volume)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if ((unsigned char)id >= 8 || volume == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    *volume = Audiolib_state.volume_scalars[id];
    return AUDIOLIB_ERR_NONE;
}

// 0x0048D010 (:717)
audiolib_result audiolib_system_set_volume_scalar(int8_t id, float volume)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if ((unsigned char)id >= 8)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    // Scalar 6 is the master; the others scale their categories (at once when coming back from 0).
    const float old_volume = Audiolib_state.volume_scalars[id];
    Audiolib_state.volume_scalars[id] = volume;
    if (id != 6)
    {
        audiolib_category_set_user_category_volumes(id, volume, old_volume == 0.0f);
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x004831C0 (:734)
audiolib_result audiolib_system_low_health_get_lowpass_adjustment(float* low_pass)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (low_pass == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    *low_pass = Audiolib_state.low_health_low_pass_adjust;
    return AUDIOLIB_ERR_NONE;
}

// 0..1 (the clamps of the low health setters).
static float audiolib_clamp_unit(float value)
{
    if (value - 0.0f < 0.0f)
    {
        value = 0.0f;
    }
    if (0.0f <= value - 1.0f)
    {
        value = 1.0f;
    }
    return value;
}

// 0x004831F0 (:744)
audiolib_result audiolib_system_low_health_set_lowpass_adjustment(float low_pass)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    Audiolib_state.low_health_low_pass_adjust = audiolib_clamp_unit(low_pass);
    return AUDIOLIB_ERR_NONE;
}

// 0x00483250 (:754)
audiolib_result audiolib_system_low_health_get_volume_adjustment(float* volume)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (volume == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    *volume = Audiolib_state.low_health_volume_adjust;
    return AUDIOLIB_ERR_NONE;
}

// 0x00483280 (:764)
audiolib_result audiolib_system_low_health_set_volume_adjustment(float volume)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    Audiolib_state.low_health_volume_adjust = audiolib_clamp_unit(volume);
    return AUDIOLIB_ERR_NONE;
}

// 0x004832E0 (:774)
audiolib_result audiolib_system_low_health_set_unaffected_category(short cat_id)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (cat_id == -1)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    // Up to 10 categories.
    for (short& slot : Audiolib_state.low_health_cat_ids)
    {
        if (slot == -1)
        {
            slot = cat_id;
            return AUDIOLIB_ERR_NONE;
        }
    }
    return AUDIOLIB_ERR_NO_MORE_ENTRIES;
}

// 0x00483330 (:791)
audiolib_result audiolib_system_low_health_unset_unaffected_category(short cat_id)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (cat_id == -1)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    for (short& slot : Audiolib_state.low_health_cat_ids)
    {
        if (slot == cat_id)
        {
            slot = -1;
            return AUDIOLIB_ERR_NONE;
        }
    }
    return AUDIOLIB_ERR_NO_MORE_ENTRIES;
}

// 0x00483380 (:808)
bool audiolib_min_max_distance_get_extended(bool extended)
{
    (void)extended;
    return Audiolib_state.initialized && Audiolib_settings.use_extended_min_max_distances;
}

// 0x00483390 (:817)
audiolib_result audiolib_min_max_distance_set_extended(bool extended)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    Audiolib_settings.use_extended_min_max_distances = extended;
    return AUDIOLIB_ERR_NONE;
}

// 0x004833B0 (:826)
void audiolib_reset_collision_timer()
{
    // No collision sounds for 3.5 s.
    Audiolib_collision_timer.set(3500);
}

// 0x004833C0 (:831)
bool audiolib_allow_collision_sounds()
{
    return Audiolib_collision_timer.elapsed();
}

// 0x004833D0 (:839)
audiolib_result audiolib_system_set_game_state(AUDIOLIB_GAME_STATE audio_game_state)
{
    const bool was_valid = Audiolib_state.position_valid;
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (audio_game_state == AUDIOLIB_GAME_STATE_LOADING || audio_game_state == AUDIOLIB_GAME_STATE_UNITIALIZED)
    {
        Audiolib_state.position_valid = false;
    }
    else if (audio_game_state == AUDIOLIB_GAME_STATE_IN_LEVEL || audio_game_state == AUDIOLIB_GAME_STATE_SATELLITE)
    {
        Audiolib_state.position_valid = true;
    }
    Audiolib_state.audio_game_state = audio_game_state;
    // Entering a level: no collision sounds for 3.5 s.
    if (!Audiolib_state.position_valid)
    {
        Audiolib_collision_timer.invalidate();
    }
    else if (!was_valid)
    {
        Audiolib_collision_timer.set(3500);
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x00483440 (:869)
AUDIOLIB_GAME_STATE audiolib_system_get_game_state()
{
    return Audiolib_state.initialized ? Audiolib_state.audio_game_state : AUDIOLIB_GAME_STATE_UNITIALIZED;
}

// 0x00483460 (:880)
audiolib_result audiolib_get_listener_pos(vector* position, vector* orient, vector* velocity)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (position != nullptr)
    {
        *position = Audiolib_state.listener_pos;
    }
    if (orient != nullptr)
    {
        *orient = Audiolib_state.listener_front_orient;
    }
    if (velocity != nullptr)
    {
        *velocity = Audiolib_state.listener_velocity;
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x0048D080 (:892)
audiolib_result audiolib_set_listener_pos(const vector* position, const matrix* orient, const vector* velocity)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    return audiolib_internal_set_listener(position, orient, velocity);
}

// 0x004834D0 (:899)
float audiolib_get_listener_fov()
{
    return Audiolib_state.initialized ? Audiolib_state.listener_fov : 0.0f;
}

// 0x004834F0 (:905)
audiolib_result audiolib_set_listener_fov(float listener_fov)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    Audiolib_state.listener_fov = listener_fov;
    return AUDIOLIB_ERR_NONE;
}

// 0x0048D0A0 (:914)
audiolib_result audiolib_listener_get_raycast_dir(AUDIOLIB_RAYCAST_DIRECTION raycast_dir, vector* direction)
{
    if (direction == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (!Audio_fov_Inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    *direction = Audiolib_state.listener_raycasts[raycast_dir].direction;
    return AUDIOLIB_ERR_NONE;
}

// 0x0048D0E0 (:921)
audiolib_result audiolib_listener_raycast(AUDIOLIB_RAYCAST_DIRECTION direction, int num_collisions, float* distances, vector* positions,
                                          audiolib_material_type* materials)
{
    if (direction > AUDIOLIB_RAYCAST_DIRECTION_NORTH_WEST_DOWN)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    // At most 3 collisions are kept.
    if (num_collisions > 2)
    {
        num_collisions = 3;
    }
    audiolib_raycast_data& raycast = Audiolib_state.listener_raycasts[direction];
    raycast.num_collisions = num_collisions;
    // No hit, or the first one beyond 45 m: open in that direction.
    if (num_collisions == 0 || (distances != nullptr && *distances > 45.0f))
    {
        raycast.valid = false;
        audiolib_internal_fov_determine_room_size_and_shape();
        return AUDIOLIB_ERR_NONE;
    }
    for (int i = 0; i < num_collisions; ++i)
    {
        // Missing data: 10 km away, at the origin, thin reflective material.
        raycast.distances[i] = distances != nullptr ? distances[i] : 10000.0f;
        if (positions != nullptr)
        {
            raycast.positions[i] = positions[i];
        }
        else
        {
            raycast.positions[i].zero();
        }
        raycast.materials[i] = materials != nullptr ? materials[i] : AUDIOLIB_MATERIAL_TYPE_REFLECT_THIN;
    }
    raycast.valid = true;
    audiolib_internal_fov_determine_room_size_and_shape();
    return AUDIOLIB_ERR_NONE;
}

// 0x00483510 (:966)
audiolib_result audiolib_all_sounds_get_pause(bool* sounds, bool* music)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (sounds != nullptr)
    {
        *sounds = Audiolib_state.sounds_paused;
    }
    if (music != nullptr)
    {
        *music = Audiolib_state.music_paused;
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x0049EF00 (:977)
audiolib_result audiolib_all_sounds_pause(bool sounds, bool music)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    audiolib_internal_playlist_pause(sounds, music);
    if (sounds)
    {
        Audiolib_state.sounds_paused = true;
    }
    if (music)
    {
        Audiolib_state.music_paused = true;
    }
    // Original: success is reported as not implemented.
    return AUDIOLIB_ERR_NOT_IMPLEMENTED;
}

// 0x0049EF40 (:993)
audiolib_result audiolib_all_sounds_unpause(bool sounds, bool music)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    audiolib_internal_playlist_unpause(sounds, music);
    if (sounds)
    {
        Audiolib_state.sounds_paused = false;
    }
    if (music)
    {
        Audiolib_state.music_paused = false;
    }
    // Original: success is reported as not implemented.
    return AUDIOLIB_ERR_NOT_IMPLEMENTED;
}

// 0x004A8CC0 (:1009)
audiolib_result audiolib_all_sounds_stop(bool sounds, bool music)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    audiolib_internal_playlist_stop(sounds, music);
    Audiolib_state.sounds_paused = false;
    Audiolib_state.music_paused = false;
    // Original: success is reported as not implemented.
    return AUDIOLIB_ERR_NOT_IMPLEMENTED;
}

// 0x00499AD0 (:1020)
void audiolib_all_streaming_sounds_unload()
{
    // audiolib_internal_playlist_streaming_sound_unload, inlined (Play_instances is placed later in the unity TU).
    audiolib_internal_playlist_streaming_sound_unload();
}

// 0x0048D200 (:1027)
audiolib_result audiolib_category_get_id(const char* category_name, short* category_id)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (category_name == nullptr || category_id == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    *category_id = audiolib_internal_get_category_id(category_name);
    return AUDIOLIB_ERR_NONE;
}

// 0x0048D240 (:1039)
audiolib_result audiolib_category_get_mute(short category_id, bool* mute)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (category_id == -1)
    {
        return AUDIOLIB_ERR_BAD_CATEGORY_ID;
    }
    if (mute == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    return audiolib_internal_get_category_mute(category_id, mute);
}

// 0x0048D280 (:1050)
audiolib_result audiolib_category_set_mute(short category_id, bool mute)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (category_id == -1)
    {
        return AUDIOLIB_ERR_BAD_CATEGORY_ID;
    }
    // Original: the null check of the getter is kept, so an unmute is refused.
    if (!mute)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    return audiolib_internal_set_category_mute(category_id, mute);
}

// 0x0048D2C0 (:1062)
audiolib_result audiolib_category_get_pause(short category_id, bool* pause)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (category_id == -1)
    {
        return AUDIOLIB_ERR_BAD_CATEGORY_ID;
    }
    if (pause == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    return audiolib_internal_get_category_pause(category_id, pause);
}

// 0x0048D300 (:1073)
audiolib_result audiolib_category_set_pause(short category_id, bool pause)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (category_id == -1)
    {
        return AUDIOLIB_ERR_BAD_CATEGORY_ID;
    }
    // Original: the null check of the getter is kept, so an unpause is refused.
    if (!pause)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    return audiolib_internal_set_category_pause(category_id, pause);
}

// 0x0048D340 (:1084)
audiolib_result audiolib_category_get_volume(short category_id, float* volume)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (category_id == -1)
    {
        return AUDIOLIB_ERR_BAD_CATEGORY_ID;
    }
    if (volume == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (Audiolib_category_list.count == 0)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    // Original: one past the last category passes the bound.
    if (category_id > Audiolib_category_list.count)
    {
        return AUDIOLIB_ERR_BAD_CATEGORY_ID;
    }
    *volume = Audiolib_category_list.category[category_id].curr_volume;
    return AUDIOLIB_ERR_NONE;
}

// 0x0048D390 (:1095)
audiolib_result audiolib_category_set_volume(short category_id, float volume)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (category_id == -1)
    {
        return AUDIOLIB_ERR_BAD_CATEGORY_ID;
    }
    return audiolib_internal_set_category_volume(category_id, volume);
}

// 0x0049EF80 (:1105)
audiolib_result audiolib_category_stop(short category_id)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (category_id == -1)
    {
        return AUDIOLIB_ERR_BAD_CATEGORY_ID;
    }
    if (Audiolib_category_list.count == 0)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (category_id > Audiolib_category_list.count)
    {
        return AUDIOLIB_ERR_BAD_CATEGORY_ID;
    }
    audiolib_internal_playlist_stop_by_category(category_id);
    return AUDIOLIB_ERR_NONE;
}

// 0x00483540 (:1115)
audiolib_result audiolib_category_get_duck_volume(float* volume)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (volume == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    *volume = Audiolib_state.duck_volume;
    return AUDIOLIB_ERR_NONE;
}

// 0x00483570 (:1126)
audiolib_result audiolib_category_set_duck_volume(float volume)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    // A negative volume keeps the current one.
    if (0.0f <= volume)
    {
        Audiolib_state.duck_volume = volume;
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x004835A0 (:1138)
bool audiolib_ducking()
{
    return Audiolib_state.initialized && Audiolib_state.ducking;
}

// 0x0048D3D0 (:1147)
audiolib_result audiolib_category_duck(short category_id, float volume)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (category_id == -1)
    {
        return AUDIOLIB_ERR_BAD_CATEGORY_ID;
    }
    if (0.0f <= volume)
    {
        Audiolib_state.duck_volume = volume;
    }
    return audiolib_internal_category_duck(category_id);
}

// 0x004835B0 (:1160)
audiolib_result audiolib_category_get_duck_id(short* category_id)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (category_id == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    *category_id = Audiolib_state.duck_id;
    return AUDIOLIB_ERR_NONE;
}

// 0x0048D410 (:1172)
audiolib_result audiolib_category_duck_off()
{
    if (!Audiolib_state.initialized || Audiolib_category_list.count == 0)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    // audiolib_internal_category_duck_off, inlined: back to full volume over the duck fade time.
    if (Audiolib_state.ducking && Audiolib_state.dest_duck_volume != 1.0f)
    {
        Audiolib_state.dest_duck_volume = 1.0f;
        Audiolib_state.duck_fade_time = timer_get(TIMER_MILLISECONDS) + Audiolib_settings.duck_fade_time;
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x004835E0 (:1183)
bool audiolib_in_cutscene()
{
    return Audiolib_state.initialized && Audiolib_state.in_cutscene;
}

// 0x0048D470 (:1190)
audiolib_result audiolib_cutscene_set_status(bool in_cutscene)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (Audiolib_category_list.count != 0)
    {
        // audiolib_internal_get_category_id("CUTSCENE_AUDIO"), inlined. Original: through the foley hash table.
        const short cat_id = (short)audiolib_foley_hash_table_get(crc_str("CUTSCENE_AUDIO", 0), nullptr);
        if (cat_id != -1)
        {
            if (in_cutscene)
            {
                // The cutscene category ducks the rest at the cutscene volume; the duck before is kept.
                Audiolib_state.cut_duck_id = Audiolib_state.duck_id;
                Audiolib_state.cut_duck_volume = Audiolib_state.duck_volume;
                if (0.0f <= Audiolib_state.cutscene_volume)
                {
                    Audiolib_state.duck_volume = Audiolib_state.cutscene_volume;
                }
                audiolib_internal_category_duck(cat_id);
            }
            else if (Audiolib_state.ducking)
            {
                if (0.0f <= Audiolib_state.cut_duck_volume)
                {
                    Audiolib_state.duck_volume = Audiolib_state.cut_duck_volume;
                }
                audiolib_category_duck(Audiolib_state.cut_duck_id, -1.0f);
            }
            Audiolib_state.in_cutscene = in_cutscene;
        }
    }
    // Original: always an unknown category.
    return AUDIOLIB_ERR_HW_UNKNOWN_CATEGORY;
}

// 0x004835F0 (:1219)
audiolib_result audiolib_cutscene_get_volume(float* volume)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (volume == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    *volume = Audiolib_state.cutscene_volume;
    return AUDIOLIB_ERR_NONE;
}

// 0x00483620 (:1229)
audiolib_result audiolib_cutscene_set_volume(float volume)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    Audiolib_state.cutscene_volume = volume;
    return AUDIOLIB_ERR_NONE;
}

// 0x0048D580 (:1241), cue_info in ECX, foley_data in EDI
static bool audiolib_copy_foley_data_to_cue_info(audiolib_foley_data* foley_data, audiolib_cue_info* cue_info)
{
    memset(cue_info->name, 0, sizeof(cue_info->name));
    cue_info->cue_id = -1;
    // The name is "crc" (original: a 4-character buffer for "crc:0x%x"); with encoded strings it stays empty.
    char name[64];
    name[0] = '\0';
    if (Audiolib_state.encoded_strings == nullptr)
    {
        _snprintf_safe(name, 4, "crc:0x%x", foley_data->name_crc);
        strncpy(cue_info->name, name, sizeof(cue_info->name));
    }
    cue_info->cue_id = audiolib_caller_cue_id(foley_data);
    cue_info->category_id = foley_data->category_id;
    cue_info->far_variant = -1;
    if (foley_data->near_far_variant)
    {
        for (int i = 0; i < Audiolib_settings.num_near_far_entries; ++i)
        {
            if (Audiolib_settings.near_far_entries[i].near_cue == cue_info->cue_id)
            {
                cue_info->far_variant = Audiolib_settings.near_far_entries[i].far_cue;
                break;
            }
        }
    }
    cue_info->volume = (float)((double)(float)foley_data->initial_volume / 100.0);
    const audiolib_min_max_entry& distances = Audiolib_settings.min_max_distance_entries[foley_data->min_max_index];
    if (Audiolib_settings.use_extended_min_max_distances)
    {
        cue_info->distance_min = distances.min_ext;
        cue_info->distance_max = distances.max_ext;
    }
    else
    {
        cue_info->distance_min = distances.min;
        cue_info->distance_max = distances.max;
    }
    cue_info->distance_override = foley_data->max_dist_override;
    cue_info->always_2d = foley_data->always_2d;
    cue_info->max_simultaneous = foley_data->max_simultaneous;
    cue_info->source_channels = foley_data->input_channels;
    cue_info->loop_count = foley_data->looping;
    if (foley_data->pitch_changes_allowed)
    {
        cue_info->lowest_pitch = -1.0f;
        cue_info->highest_pitch = 1.0f;
    }
    // audiolib_internal_get_variation_pointer, inlined.
    for (unsigned int i = 0; i < foley_data->num_variants; ++i)
    {
        cue_info->play_time[i] = foley_data->num_variants < 2 ? foley_data->one_variation.play_time : foley_data->variation_list[i].play_time;
    }
    return true;
}

// 0x00483640 (:1297)
bool audiolib_internal_wildcard_match(const char* wildcard, const char* test_str)
{
    if (wildcard == nullptr || test_str == nullptr)
    {
        return false;
    }
    // '?' matches any character, '*' the rest; the end of either string matches.
    for (; *wildcard != '\0' && *test_str != '\0'; ++wildcard, ++test_str)
    {
        if (*wildcard == '?')
        {
            continue;
        }
        if (*wildcard == '*')
        {
            return true;
        }
        if (toupper(*wildcard) != toupper(*test_str))
        {
            return false;
        }
    }
    return true;
}

// The name a cue is matched by (audiolib_copy_foley_data_to_cue_info's).
static bool audiolib_cue_name_matches(const audiolib_foley_data& foley, const char* wildcard)
{
    char name[64];
    name[0] = '\0';
    if (Audiolib_state.encoded_strings != nullptr)
    {
        return false;
    }
    _snprintf_safe(name, 4, "crc:0x%x", foley.name_crc);
    return audiolib_internal_wildcard_match(wildcard, name);
}

// 0x0048D750 (:1319)
int audiolib_get_number_of_cues_by_wildcard(const char* wildcard)
{
    if (!Audiolib_state.initialized)
    {
        return -1;
    }
    if (wildcard == nullptr)
    {
        return Audiolib_foley_list.total_cues;
    }
    int count = 0;
    for (int i = 0; i < Audiolib_foley_list.total_cues; ++i)
    {
        if (Audiolib_foley_list.foley[i].in_use && audiolib_cue_name_matches(Audiolib_foley_list.foley[i], wildcard))
        {
            ++count;
        }
    }
    return count;
}

// 0x0048D830 (:1350)
audiolib_result audiolib_get_first_cue_by_wildcard(const char* wildcard, audiolib_cue_info* cue_info)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (wildcard == nullptr)
    {
        wildcard = last_matching_string_default;
    }
    last_matching_string_index = 0;
    for (int i = 0; i < Audiolib_foley_list.total_cues; ++i)
    {
        audiolib_foley_data& foley = Audiolib_foley_list.foley[i];
        if (foley.in_use && audiolib_cue_name_matches(foley, wildcard))
        {
            strncpy(last_matching_string, wildcard, sizeof(last_matching_string));
            last_matching_string_index = i;
            audiolib_copy_foley_data_to_cue_info(&foley, cue_info);
            return AUDIOLIB_ERR_NONE;
        }
    }
    return AUDIOLIB_ERR_NO_MORE_ENTRIES;
}

// 0x0048D950 (:1380)
audiolib_result audiolib_get_next_cue_by_wildcard(audiolib_cue_info* cue_info)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    for (int i = last_matching_string_index + 1; i < Audiolib_foley_list.total_cues; ++i)
    {
        audiolib_foley_data& foley = Audiolib_foley_list.foley[i];
        if (foley.in_use && audiolib_cue_name_matches(foley, last_matching_string))
        {
            last_matching_string_index = i;
            audiolib_copy_foley_data_to_cue_info(&foley, cue_info);
            return AUDIOLIB_ERR_NONE;
        }
    }
    return AUDIOLIB_ERR_NO_MORE_ENTRIES;
}

// 0x0048DA60 (:1408)
int audiolib_get_number_of_cues_by_category(short cat_id)
{
    if (!Audiolib_state.initialized)
    {
        return -1;
    }
    if (cat_id == -1)
    {
        return Audiolib_foley_list.total_cues;
    }
    int count = 0;
    for (int i = 0; i < Audiolib_foley_list.total_cues; ++i)
    {
        if (Audiolib_foley_list.foley[i].in_use && Audiolib_foley_list.foley[i].category_id == cat_id)
        {
            ++count;
        }
    }
    return count;
}

// 0x0048DAC0 (:1431)
audiolib_result audiolib_get_first_cue_by_category(short cat_id, audiolib_cue_info* cue_info)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (cue_info == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    for (int i = 0; i < Audiolib_foley_list.total_cues; ++i)
    {
        audiolib_foley_data& foley = Audiolib_foley_list.foley[i];
        if (foley.in_use && (cat_id == -1 || foley.category_id == cat_id))
        {
            last_matching_cat_id = cat_id;
            last_matching_cat_index = i;
            audiolib_copy_foley_data_to_cue_info(&foley, cue_info);
            return AUDIOLIB_ERR_NONE;
        }
    }
    return AUDIOLIB_ERR_NO_MORE_ENTRIES;
}

// 0x0048DB50 (:1456)
audiolib_result audiolib_get_next_cue_by_category(audiolib_cue_info* cue_info)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (cue_info == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    for (int i = last_matching_cat_index + 1; i < Audiolib_foley_list.total_cues; ++i)
    {
        audiolib_foley_data& foley = Audiolib_foley_list.foley[i];
        if (foley.in_use && (last_matching_cat_id == -1 || foley.category_id == last_matching_cat_id))
        {
            last_matching_cat_index = i;
            audiolib_copy_foley_data_to_cue_info(&foley, cue_info);
            return AUDIOLIB_ERR_NONE;
        }
    }
    return AUDIOLIB_ERR_NO_MORE_ENTRIES;
}

// 0x0049EFC0 (:1483), internal_cue_id in EAX
static bool audiolib_internal_cue_is_id_valid(int internal_cue_id)
{
    return Audiolib_state.initialized && internal_cue_id != 0 && internal_cue_id != -1 && audiolib_internal_cue_is_valid(internal_cue_id);
}

// 0x00499AE0 (:1492), cue_name in EAX, bank_id in SI, cue_id in EDI
static audiolib_result audiolib_cue_get_id_internal(short bank_id, const char* cue_name, int* cue_id)
{
    if (cue_name == nullptr || cue_id == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (bank_id == 0 || bank_id == -1)
    {
        return AUDIOLIB_ERR_BAD_BANK_ID;
    }
    // The internal cue id: the cue's index in its XACT soundbank (high half) and the bank id.
    *cue_id = -1;
    int cue_index;
    if (audiolib_internal_soundbank_get_cue_index(bank_id, cue_name, &cue_index) != AUDIOLIB_ERR_NONE)
    {
        return AUDIOLIB_ERR_CUE_NOT_FOUND;
    }
    *cue_id = (cue_index << 16) | (unsigned short)bank_id;
    return AUDIOLIB_ERR_NONE;
}

// 0x0049EFE0 (:1513), foley_data_handle in EAX
static audiolib_result audiolib_cue_get_id_using_foley_table(const char* cue_name, int* caller_cue_id, audiolib_foley_data** foley_data_handle)
{
    if (cue_name == nullptr || caller_cue_id == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    audiolib_foley_data* foley = audiolib_get_foley_data_pointer(audiolib_foley_hash_table_get(crc_str(cue_name, 0), nullptr));
    if (foley != nullptr)
    {
        if (foley_data_handle != nullptr)
        {
            *foley_data_handle = foley;
        }
        // Banks no longer registered are forgotten, the internal cue id looked up again.
        if (foley->wavebank_id != -1 && !audiolib_sound_bank_is_valid(foley->wavebank_id, true))
        {
            foley->wavebank_id = -1;
        }
        if (foley->soundbank_id != -1 && !audiolib_sound_bank_is_valid(foley->soundbank_id, true))
        {
            foley->soundbank_id = -1;
        }
        bool valid = false;
        if (foley->internal_cue_id != -1)
        {
            valid = audiolib_internal_cue_is_id_valid(foley->internal_cue_id);
            if (!valid)
            {
                foley->internal_cue_id = -1;
            }
        }
        if (!valid && foley->soundbank_id != -1)
        {
            int internal_cue_id;
            if (audiolib_cue_get_id_internal(foley->soundbank_id, cue_name, &internal_cue_id) == AUDIOLIB_ERR_NONE)
            {
                foley->internal_cue_id = internal_cue_id;
                valid = true;
            }
        }
        if (valid)
        {
            *caller_cue_id = audiolib_caller_cue_id(foley);
            return AUDIOLIB_ERR_NONE;
        }
    }
    *caller_cue_id = -1;
    return AUDIOLIB_ERR_HW_UNKNOWN_FOLEY;
}

// 0x0048DBE0 (:1584)
audiolib_result audiolib_cue_get_id(const unsigned int cue_name_crc, int* caller_cue_id)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (caller_cue_id == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    *caller_cue_id = -1;
    const audiolib_foley_data* foley = audiolib_get_foley_data_pointer(audiolib_foley_hash_table_get(cue_name_crc, nullptr));
    if (foley == nullptr)
    {
        return AUDIOLIB_ERR_CUE_NOT_FOUND;
    }
    *caller_cue_id = audiolib_caller_cue_id(foley);
    return AUDIOLIB_ERR_NONE;
}

// Deviation (SFX.md 3.1): the --audio-log test seam, off (null) by default. The retail data has no cue names, so the
// log learns them from the lookups by name.
void (*Audiolib_cue_get_id_log_hook)(const char* cue_name, int caller_cue_id) = nullptr;
void (*Audiolib_cue_play_log_hook)(int cue_id, const audiolib_params* params, int play_id, audiolib_result error) = nullptr;

// 0x0048DC50 (:1618)
audiolib_result audiolib_cue_get_id(const char* cue_name, int* caller_cue_id)
{
    if (cue_name == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    const audiolib_result result = audiolib_cue_get_id(crc_str(cue_name, 0), caller_cue_id);
    if (Audiolib_cue_get_id_log_hook != nullptr && result == AUDIOLIB_ERR_NONE) // Deviation (SFX.md 3.1)
    {
        Audiolib_cue_get_id_log_hook(cue_name, *caller_cue_id);
    }
    return result;
}

// 0x0049F0F0 (:1646)
bool audiolib_cue_is_id_valid(int caller_cue_id)
{
    if (!Audiolib_state.initialized || caller_cue_id == -1 || caller_cue_id == 0)
    {
        return false;
    }
    const audiolib_foley_data* foley = audiolib_get_foley_data_pointer(caller_cue_id);
    return foley != nullptr && audiolib_internal_cue_is_id_valid(foley->internal_cue_id);
}

// 0x0048DC80 (:1665)
audiolib_result audiolib_cue_get_info(int cue_id, audiolib_cue_info* cue_info)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (cue_info == nullptr || cue_id == -1)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    audiolib_foley_data* foley = audiolib_get_foley_data_pointer(cue_id);
    if (foley == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    memset(cue_info, 0, sizeof(*cue_info));
    audiolib_copy_foley_data_to_cue_info(foley, cue_info);
    return AUDIOLIB_ERR_NONE;
}

// 0x0048DCE0 (:1691)
int audiolib_cue_is_looping(int cue_id)
{
    if (!Audiolib_state.initialized || cue_id == -1)
    {
        return -1;
    }
    const audiolib_foley_data* foley = audiolib_get_foley_data_pointer(cue_id);
    return foley != nullptr ? foley->looping : -1;
}

// 0x004836B0 (:1704)
void audiolib_internal_adjust_parameters_for_dlc(audiolib_params* params, unsigned int crc)
{
    // DLC 1: nine cues (by name crc) play at 0.1.
    if (Audiolib_state.dlc_id != 1)
    {
        return;
    }
    switch (crc)
    {
    case 0x11C7B45F:
    case 0x211614F9:
    case 0x60D87899:
    case 0x9AD745FA:
    case 0xDB19299A:
    case 0xE6AD9CF8:
    case 0xE91E4F36:
    case 0xF0BD297A:
        params->volume = 0.1f;
        break;
    default:
        break;
    }
}

// The four DLC 1 cues with their own start chance and limit.
static bool audiolib_dlc_limited_cue(unsigned int crc)
{
    return crc == 0x11C7B45F || crc == 0xE6AD9CF8 || crc == 0xE91E4F36 || crc == 0xF0BD297A;
}

// 0x00483720 (:1727)
unsigned char audiolib_internal_dont_start_percentage(audiolib_foley_data* foley_data)
{
    // 75 % of these DLC 1 cues do not start.
    if (Audiolib_state.dlc_id == 1 && audiolib_dlc_limited_cue(foley_data->name_crc))
    {
        return 75;
    }
    return foley_data->dont_start_percentage;
}

// 0x00483760 (:1748)
unsigned char audiolib_internal_max_simultaneous(audiolib_foley_data* foley_data)
{
    // At most 2 at a time in DLC 1.
    if (Audiolib_state.dlc_id == 1 && audiolib_dlc_limited_cue(foley_data->name_crc))
    {
        return 2;
    }
    return foley_data->max_simultaneous;
}

static int audiolib_cue_play_internal(int cue_id, const audiolib_params* passed_params, audiolib_result* error);

// Deviation (SFX.md 3.1): with the --audio-log seam on, each result is logged; rfg.exe's audiolib_cue_play is the
// body below (audiolib_cue_play_internal).
int audiolib_cue_play(int cue_id, const audiolib_params* passed_params, audiolib_result* error)
{
    if (Audiolib_cue_play_log_hook == nullptr)
    {
        return audiolib_cue_play_internal(cue_id, passed_params, error);
    }
    // A sentinel shows whether the call wrote an error (the caller's is written only then, as without the seam).
    const audiolib_result unset = (audiolib_result)0x7FFFFFFF;
    audiolib_result result = unset;
    const int play_id = audiolib_cue_play_internal(cue_id, passed_params, &result);
    if (error != nullptr && result != unset)
    {
        *error = result;
    }
    Audiolib_cue_play_log_hook(cue_id, passed_params, play_id, result == unset ? AUDIOLIB_ERR_NONE : result);
    return play_id;
}

// 0x0049F120 (:1778)
static int audiolib_cue_play_internal(int cue_id, const audiolib_params* passed_params, audiolib_result* error)
{
    if (!Audiolib_state.initialized || !Audiolib_state.hw_ready)
    {
        return audiolib_cue_play_refused(AUDIOLIB_ERR_NOT_INITED, error);
    }
    if (cue_id == -1 || cue_id == 0)
    {
        return audiolib_cue_play_refused(AUDIOLIB_ERR_BAD_CUE_ID, error);
    }
    bool use_far_variant = false;
    audiolib_params params;
    if (passed_params != nullptr)
    {
        params = *passed_params;
    }
    float dist = 0.0f;
    float delay = 0.0f;
    audiolib_foley_data* foley = audiolib_get_foley_data_pointer(cue_id);
    if (foley == nullptr || foley->name_crc == 0)
    {
        return audiolib_cue_play_refused(AUDIOLIB_ERR_BAD_CUE_ID, error);
    }
    strncpy(curr_cue_name, audiolib_internal_debug_foley_name(foley->name_crc), sizeof(curr_cue_name));
    audiolib_internal_adjust_parameters_for_dlc(&params, foley->name_crc);
    if (Audiolib_state.audio_game_state == AUDIOLIB_GAME_STATE_LOADING)
    {
        // While loading only music and distance-kept sounds.
        if (foley->category_id != Audiolib_state.music_cat_id && !foley->max_dist_override && (params.flags & 0x40) == 0)
        {
            return audiolib_cue_play_refused(AUDIOLIB_ERR_CANT_PLAY_WHEN_LOADING, error);
        }
    }
    else if (Audiolib_state.audio_game_state == AUDIOLIB_GAME_STATE_SATELLITE)
    {
        // In the satellite view only satellite cues (below the listener) and the music, system and radio categories.
        if (foley->allow_satellite)
        {
            if ((params.flags & 1) != 0)
            {
                params.pos.y = Audiolib_state.listener_pos.y - Audiolib_settings.satellite_play_distance;
            }
        }
        else
        {
            const short cat_id = foley->category_id;
            if (cat_id != Audiolib_state.music_cat_id && cat_id != Audiolib_state.system_cat_id && cat_id != Audiolib_state.gur_radio_cat_id &&
                cat_id != Audiolib_state.gur_pc_cat_id)
            {
                return audiolib_cue_play_refused(AUDIOLIB_ERR_DIST_TOO_FAR, error);
            }
        }
    }
    // The wavebank must be registered and not unloading (audiolib_internal_soundbank_id_to_ptr, inlined).
    const short wavebank_id = foley->wavebank_id;
    if (wavebank_id == -1)
    {
        return audiolib_cue_play_refused(AUDIOLIB_ERR_BAD_CUE_ID, error);
    }
    const unsigned int bank_index = wavebank_id & 0xFF;
    audiolib_soundbank_data* bank = bank_index < 0xFC ? Soundbank_list.get_entry((int)bank_index) : nullptr;
    if (bank == nullptr || ((int)wavebank_id >> 8) != (int)bank->rolling_counter || audiolib_internal_get_soundbank_state(bank) >= AUDIOLIB_SOUNDBANK_STATE_UNLOADING)
    {
        return audiolib_cue_play_refused(AUDIOLIB_ERR_BAD_CUE_ID, error);
    }
    const audiolib_min_max_entry& distances = Audiolib_settings.min_max_distance_entries[foley->min_max_index];
    float dist_min = Audiolib_settings.use_extended_min_max_distances ? distances.min_ext : distances.min;
    float dist_max = Audiolib_settings.use_extended_min_max_distances ? distances.max_ext : distances.max;
    if (foley->always_2d)
    {
        params.flags &= 0xFFFE;
    }
    audiolib_foley_data* chosen = foley;
    // A 3D sound with caller params, not distance-kept: refused beyond its maximum distance, or played as its far
    // variant beyond the far cue's minimum distance.
    if (passed_params != nullptr && (params.flags & 1) != 0 && (params.flags & 0x40) == 0 && !foley->max_dist_override)
    {
        if ((params.flags & 0x80) != 0 && params.num_positions > 0)
        {
            // A public address sound: the nearest emitter.
            float nearest = vec_dist_squared(Audiolib_state.listener_pos, params.position_list[0]);
            for (int i = 1; i < params.num_positions; ++i)
            {
                const float squared = vec_dist_squared(Audiolib_state.listener_pos, params.position_list[i]);
                if (squared < nearest)
                {
                    nearest = squared;
                }
            }
            dist = (float)sqrt((double)nearest);
        }
        else
        {
            dist = vec_dist(Audiolib_state.listener_pos, params.pos);
        }
        // The sound's travel time delays the start.
        delay = (float)((double)dist * 1000.0 / (double)Audiolib_settings.speed_of_sound);
        bool range_checked = false;
        if (foley->near_far_variant)
        {
            for (int i = 0; i < Audiolib_settings.num_near_far_entries; ++i)
            {
                if (Audiolib_settings.near_far_entries[i].near_cue != cue_id)
                {
                    continue;
                }
                const int far_cue = Audiolib_settings.near_far_entries[i].far_cue;
                audiolib_foley_data* far_foley = far_cue != -1 ? audiolib_get_foley_data_pointer(far_cue) : nullptr;
                if (far_foley != nullptr)
                {
                    // Original: the far cue's distances never use the extended table.
                    const audiolib_min_max_entry& far_distances = Audiolib_settings.min_max_distance_entries[far_foley->min_max_index];
                    if (0.0f < far_distances.min && far_distances.min < dist)
                    {
                        use_far_variant = true;
                        chosen = far_foley;
                        dist_max = far_distances.max;
                        dist_min = far_distances.min;
                        if (far_distances.max > 0.0f && dist > far_distances.max)
                        {
                            return audiolib_cue_play_refused(AUDIOLIB_ERR_DIST_TOO_FAR, error);
                        }
                        range_checked = true;
                    }
                }
                break;
            }
        }
        if (!range_checked && 0.0f < dist_max && dist_max < dist)
        {
            return audiolib_cue_play_refused(AUDIOLIB_ERR_DIST_TOO_FAR, error);
        }
    }
    const unsigned char max_simultaneous = audiolib_internal_max_simultaneous(chosen);
    if (max_simultaneous != 0 && max_simultaneous <= chosen->curr_simul)
    {
        return audiolib_cue_play_refused(AUDIOLIB_ERR_EXCEED_FOLEY_LIMIT, error);
    }
    if (s_audiolib_checkLastPlayed)
    {
        const int now = timer_get(TIMER_MILLISECONDS);
        if (chosen->last_cue_time > 0 && (float)(now - chosen->last_cue_time) <= s_audiolib_checkLastPlayed_Time)
        {
            return audiolib_cue_play_refused(AUDIOLIB_ERR_EXCEED_FOLEY_LIMIT, error);
        }
    }
    // A cue with a "don't start" chance (CRT rand(), SFX.md 2.5) never skips twice in a row.
    const unsigned char dont_start = audiolib_internal_dont_start_percentage(chosen);
    if (dont_start != 0 && rand() % 100 < (int)dont_start)
    {
        if (!chosen->didnt_start_last_time)
        {
            chosen->didnt_start_last_time = true;
            return audiolib_cue_play_refused(AUDIOLIB_ERR_DONT_PLAY_PERCENTAGE, error);
        }
        chosen->didnt_start_last_time = false;
    }
    const short cat_id = chosen->category_id;
    if (Audiolib_category_list.count != 0 && cat_id != -1 && cat_id <= Audiolib_category_list.count &&
        Audiolib_category_list.category[cat_id].curr_volume <= 1e-05f)
    {
        return audiolib_cue_play_refused(AUDIOLIB_ERR_CATEGORY_VOLUME_TOO_LOW, error);
    }
    if (!audiolib_internal_ok_to_start_category_instance(cat_id))
    {
        return audiolib_cue_play_refused(AUDIOLIB_ERR_EXCEED_CATEGORY_LIMIT, error);
    }
    if (chosen->wavebank_id == -1 || chosen->soundbank_id == -1)
    {
        // The banks are looked up again by name. Original: the category instance taken above is not given back.
        int found_cue_id;
        audiolib_foley_data* found_foley;
        if (audiolib_cue_get_id_using_foley_table(audiolib_internal_debug_foley_name(chosen->name_crc), &found_cue_id, &found_foley) < 0)
        {
            return audiolib_cue_play_refused(AUDIOLIB_ERR_BAD_BANK_ID, error);
        }
    }
    if ((params.flags & 1) == 0)
    {
        params.pos.zero();
    }
    if (params.volume == 0.0f)
    {
        const float initial_volume = (float)((double)(float)chosen->initial_volume / 100.0);
        if (0.0f < initial_volume)
        {
            params.volume = initial_volume;
        }
    }
    int multi_index;
    audiolib_play_data* play = audiolib_internal_playlist_alloc(chosen, params.pos, &params, multi_index);
    if (play == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_cue_play> too many sound in category:'%s' NOT PLAYED\n", audiolib_internal_debug_foley_name(chosen->name_crc));
        audiolib_internal_category_instance_ended(chosen->category_id);
        return audiolib_cue_play_refused(AUDIOLIB_ERR_NO_PLAY_SLOTS, error);
    }
    // A merged emitter's id carries its emitter index (bits 12-15).
    int play_id;
    if (multi_index < 0)
    {
        play_id = (play->index & 0xFFF) | (play->rolling_counter << 16);
    }
    else
    {
        play_id = ((play->rolling_counter << 4 | (multi_index & 0xF)) << 12) | (play->index & 0xFFF);
    }
    if (chosen->max_simultaneous != 0)
    {
        chosen->curr_simul = chosen->curr_simul + 1;
    }
    // Original: a merge sets up the shared play again with this cue's params.
    play->foley_data = *chosen;
    play->params = params;
    play->curr_variant = (short)audiolib_internal_choose_variation(&play->foley_data);
    const audiolib_variation_info* variation = audiolib_internal_get_variation_pointer(&play->foley_data, play->curr_variant);
    // Original: the offset is wrapped into the variation in the local copy only (and never ends for a zero play time).
    while (params.offset_ms != 0 && variation->play_time <= params.offset_ms)
    {
        params.offset_ms -= variation->play_time;
    }
    if (play->params.volume == 0.0f)
    {
        play->params.volume = (float)((double)(float)play->foley_data.initial_volume / 100.0);
    }
    if (!chosen->pitch_changes_allowed)
    {
        play->params.pitch = 0.0f;
    }
    const unsigned short flags = play->params.flags;
    // A precache (params flag 0x100) has the lowest priority, others at least the cue's.
    if ((flags & 0x100) == 0)
    {
        if (play->params.priority < chosen->priority)
        {
            play->params.priority = chosen->priority;
        }
    }
    else
    {
        play->params.priority = 0;
    }
    play->curr_vol = play->params.volume;
    play->curr_pos = play->params.pos;
    play->new_pos = play->params.pos;
    play->curr_pan = play->params.pan;
    play->dist = dist;
    play->curr_orient = play->params.orient;
    if ((flags & 1) != 0 && play->curr_orient != ZERO_VECTOR)
    {
        play->curr_orient.flatten_normalize();
    }
    play->curr_velocity = play->params.velocity;
    play->curr_pitch = 0.0f;
    // DSP type 1: the caller's low pass.
    play->new_low_pass = (play->params.dsp_type & 1) != 0 ? play->params.dsp.low_pass.effect : 0.0f;
    play->curr_low_pass = 0.0f;
    if (use_far_variant)
    {
        play->params.flags |= 0x10;
    }
    play->marker_id = -1;
    play->category_id = chosen->category_id;
    play->affected_by_environ = audiolib_internal_category_affected_by_environ(play->category_id);
    play->affected_by_health = audiolib_internal_category_affected_by_low_health(play->category_id);
    if (play->params.rolloff_curve == 0)
    {
        play->params.rolloff_curve = Audiolib_category_list.category[play->category_id].rolloff_type;
    }
    if (play->curr_vol == 0.0f)
    {
        const float initial_volume = (float)((double)(float)chosen->initial_volume / 100.0);
        if (0.0f < initial_volume)
        {
            play->curr_vol = initial_volume;
        }
    }
    play->dist_min = play->params.distance_min;
    if (play->dist_min == 0.0f && 0.0f < dist_min)
    {
        play->dist_min = dist_min;
    }
    play->dist_max = play->params.distance_max;
    if (play->dist_max == 0.0f && 0.0f < dist_max)
    {
        play->dist_max = dist_max;
    }
    if (play->dist_max < play->dist_min)
    {
        play->dist_max = Audiolib_settings.default_dist_max;
    }
    if (play->curr_vol <= 0.0f)
    {
        play->curr_vol = Audiolib_settings.default_volume;
    }
    if (play->dist_min == 0.0f)
    {
        play->dist_min = Audiolib_settings.default_dist_min;
    }
    if (play->dist_max == 0.0f)
    {
        play->dist_max = Audiolib_settings.default_dist_max;
    }
    audiolib_internal_fov_compute_occlusion(play);
    const int now = timer_get(TIMER_MILLISECONDS);
    play->delay_start = (int)play->params.delay_ms + (int)delay + now;
    if (play->multi_info == nullptr || multi_index < 0)
    {
        play->output_framecount = 0;
    }
    if ((play->params.flags & 8) == 0)
    {
        play->new_vol = play->params.volume;
        play->curr_vol = play->params.volume;
        play->volume_scale = (float)(audiolib_internal_calculate_base_volume(play) * 0.5);
        if (!Audiolib_state.enabled)
        {
            play->volume_scale = 0.0f;
        }
    }
    else
    {
        // Params flag 8: a fade in (at least 100 ms) from nearly silent.
        play->action |= 0x20;
        play->new_vol = play->params.volume;
        play->curr_vol = 2e-05f;
        play->fade_time = (int)play->params.fade_ms;
        if ((int)play->params.fade_ms < 100)
        {
            play->fade_time = 100;
        }
    }
    // Params flag 0x400: never ducked.
    play->do_not_duck = play->foley_data.dont_duck;
    if ((play->params.flags & 0x400) != 0)
    {
        play->do_not_duck = true;
    }
    audiolib_internal_new_playstate_pending(play, now);
    if (error != nullptr)
    {
        *error = AUDIOLIB_ERR_NONE;
    }
    strncpy(last_cue_played_name, audiolib_internal_debug_foley_name(chosen->name_crc), sizeof(last_cue_played_name));
    chosen->last_cue_time = play->cue_time;
    return play_id;
}

// 0x0049FD00 (:2324)
audiolib_result audiolib_cue_precache(int cue_id)
{
    // Params flag 0x100: load only.
    audiolib_params params;
    params.flags = 0x100;
    return audiolib_cue_play(cue_id, &params, nullptr) != -1 ? AUDIOLIB_ERR_NONE : AUDIOLIB_ERR_BAD_PARAM;
}

// 0x0049FD60 (:2334)
int audiolib_cue_play_3d(int cue_id, const vector* pos, const vector* orient, const vector* velocity, audiolib_result* error)
{
    audiolib_params params;
    params.flags = 1;
    if (pos != nullptr)
    {
        params.pos = *pos;
    }
    params.orient = orient != nullptr ? *orient : vector(0.0f, 0.0f, 0.0f);
    params.velocity = velocity != nullptr ? *velocity : vector(0.0f, 0.0f, 0.0f);
    return audiolib_cue_play(cue_id, &params, error);
}

static bool audiolib_play_id_is_set(int play_id)
{
    return play_id != 0 && play_id != -1;
}

// 0x00494910 (:2361)
audiolib_result audiolib_play_get_state(int play_id, audiolib_play_state* state)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    if (state != nullptr)
    {
        *state = AUDIOLIB_PLAY_STATE_ERROR;
    }
    return audiolib_internal_playlist_get_state(play_id, state) != AUDIOLIB_ERR_NONE ? AUDIOLIB_ERR_BAD_PLAY_ID : AUDIOLIB_ERR_NONE;
}

// 0x00494950 (:2378)
bool audiolib_is_playing(int play_id, bool if_paused)
{
    if (!Audiolib_state.initialized || !audiolib_play_id_is_set(play_id))
    {
        return false;
    }
    audiolib_play_state state = AUDIOLIB_PLAY_STATE_UNUSED;
    if (audiolib_internal_playlist_get_state(play_id, &state) != AUDIOLIB_ERR_NONE)
    {
        return false;
    }
    return state == AUDIOLIB_PLAY_STATE_PLAYING || (if_paused && state == AUDIOLIB_PLAY_STATE_PAUSED);
}

// 0x004949A0 (:2397)
audiolib_result audiolib_play_get_play_time(int play_id, int* play_time, int* variation)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    if (play_time == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    return audiolib_internal_playlist_get_variation_and_play_time(play_id, play_time, variation);
}

// 0x004949E0 (:2407)
audiolib_result audiolib_play_get_cue_id(int play_id, int* cue_id)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    if (cue_id == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    return audiolib_internal_playlist_get_caller_cue_id(play_id, cue_id);
}

// 0x00494A20 (:2419)
audiolib_result audiolib_play_add_emitter(int play_id, const vector* pos, const vector* orientation)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    return audiolib_internal_playlist_add_emitter(play_id, pos, orientation);
}

// 0x00494A50 (:2431)
audiolib_result audiolib_play_get_pause(int play_id, bool* pause)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    if (pause == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    return audiolib_internal_playlist_get_pause(play_id, pause);
}

// 0x00494A90 (:2441)
audiolib_result audiolib_play_set_pause(int play_id, bool pause)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    return audiolib_internal_playlist_set_pause(play_id, pause);
}

// 0x00494AC0 (:2451)
audiolib_result audiolib_play_stop(int play_id)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    // The shortest fade (100 ms).
    return audiolib_internal_playlist_fade_out(play_id, 0);
}

// 0x00494AF0 (:2460)
audiolib_result audiolib_play_fade_out(int play_id, const int fade_time_ms)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    return audiolib_internal_playlist_fade_out(play_id, fade_time_ms < 0 ? Audiolib_settings.default_fade_out : fade_time_ms);
}

// 0x00494B30 (:2476)
audiolib_result audiolib_play_get_volume(int play_id, float* volume)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    if (volume == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    return audiolib_internal_playlist_get_volume(play_id, volume);
}

// 0x00494B70 (:2486)
audiolib_result audiolib_play_set_volume(int play_id, const float volume, int fade_time_ms)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    return audiolib_internal_playlist_set_volume(play_id, volume, fade_time_ms);
}

// 0x00494BB0 (:2495)
audiolib_result audiolib_play_get_pitch(int play_id, float* pitch)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    return audiolib_internal_playlist_get_pitch(play_id, pitch);
}

// 0x00494BE0 (:2504)
audiolib_result audiolib_play_set_pitch(int play_id, const float pitch)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    return audiolib_internal_playlist_set_pitch(play_id, pitch);
}

// 0x00494C20 (:2513)
audiolib_result audiolib_play_get_low_pass(int play_id, float* low_pass)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    return audiolib_internal_playlist_get_low_pass(play_id, low_pass);
}

// 0x00494C50 (:2521)
audiolib_result audiolib_play_set_low_pass(int play_id, const float low_pass)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    return audiolib_internal_playlist_set_low_pass(play_id, low_pass);
}

// 0x00494C90 (:2530)
audiolib_result audiolib_play_get_pan(int play_id, float* pan)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    return audiolib_internal_playlist_get_pan(play_id, pan);
}

// 0x00494CC0 (:2539)
audiolib_result audiolib_play_set_pan(int play_id, const float pan)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    return audiolib_internal_playlist_set_pan(play_id, pan);
}

// 0x00494D00 (:2553)
bool audiolib_play_is_2d(int play_id)
{
    return Audiolib_state.initialized && audiolib_play_id_is_set(play_id) && audiolib_internal_playlist_is_2d(play_id);
}

// 0x00494D30 (:2562)
audiolib_result audiolib_play_change_to_2d(int play_id, const float volume)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    return audiolib_internal_playlist_change_to_2d(play_id, volume);
}

// 0x00494D70 (:2572)
audiolib_result audiolib_play_change_to_3d(int play_id, const vector* pos, const vector* orientation, const vector* velocity)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    if (pos == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    return audiolib_internal_playlist_change_to_3d(play_id, pos, orientation, velocity);
}

// 0x00494DB0 (:2582)
audiolib_result audiolib_play_get_3d_position(int play_id, vector* pos, vector* orient, vector* velocity)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    if (pos == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    return audiolib_internal_playlist_get_3d_position(play_id, pos, orient, velocity);
}

// 0x00494DF0 (:2592)
audiolib_result audiolib_play_set_3d_position(int play_id, const vector* pos, const vector* orientation, const vector* velocity)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    if (pos == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    return audiolib_internal_playlist_set_3d_position(play_id, pos, orientation, velocity);
}

// 0x00494E30 (:2621)
audiolib_result audiolib_play_remove_emitter(int play_id, vector* pos)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    if (pos == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    const vector position = *pos;
    return audiolib_internal_playlist_remove_emitter(play_id, position);
}

// 0x00494EA0 (:2640)
audiolib_result audiolib_play_get_audible_range(int play_id, float* min, float* max)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    return audiolib_internal_playlist_get_audible_range(play_id, min, max);
}

// 0x00494ED0 (:2650)
audiolib_result audiolib_play_set_audible_range(int play_id, float min, float max)
{
    if (!Audiolib_state.initialized)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (!audiolib_play_id_is_set(play_id))
    {
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    return audiolib_internal_playlist_set_audible_range(play_id, min, max);
}

// 0x004837A0 (:2660)
void audiolib_stats_get(audiolib_stats* stats)
{
    if (stats == nullptr)
    {
        return;
    }
    stats->initialized = false;
    stats->enabled = false;
    stats->sounds_paused = false;
    stats->music_paused = false;
    stats->speakers = AUDIOLIB_SPEAKER_TYPE_STEREO;
    stats->pos_x = 0.0f;
    stats->pos_y = 0.0f;
    stats->pos_z = 0.0f;
    stats->initialized = Audiolib_state.initialized;
    if (Audiolib_state.initialized)
    {
        stats->enabled = Audiolib_state.enabled;
        stats->sounds_paused = Audiolib_state.sounds_paused;
        stats->music_paused = Audiolib_state.music_paused;
        stats->speakers = Audiolib_state.speaker_type;
        stats->pos_x = Audiolib_state.listener_pos.x;
        stats->pos_y = Audiolib_state.listener_pos.y;
        stats->pos_z = Audiolib_state.listener_pos.z;
    }
}

// 0x00483820 (:2682)
void audiolib_stats_reset(audiolib_stats* stats)
{
    if (stats != nullptr)
    {
        audiolib_stats_get(stats);
    }
    // The peaks restart (the field stores after the clear are left from separate resets).
    memset(&Audiolib_state.peak_stats, 0, sizeof(Audiolib_state.peak_stats));
    Audiolib_state.peak_stats.playing = 0;
    Audiolib_state.peak_stats.mem_usage = 0;
    Audiolib_state.peak_stats.cpu_usage = 0;
    Audiolib_state.peak_stats.total_cpu_usage = 0;
    Audiolib_state.peak_stats.action_count = 0;
}

// 0x00499B40 (:2801)
bool audiolib_music_wavebank_ready()
{
    // The front end music wavebank (bank type 2) loaded and ready.
    const short bank_id = audiolib_sound_bank_get_id(crc_str("mus_frontend", 0), 2);
    if (bank_id == -1)
    {
        return false;
    }
    const unsigned int index = bank_id & 0xFF;
    audiolib_soundbank_data* bank = index < 0xFC ? Soundbank_list.get_entry((int)index) : nullptr;
    return bank != nullptr && ((int)bank_id >> 8) == (int)bank->rolling_counter && audiolib_hw_is_sound_bank_ready(bank) >= 0 &&
           Soundbank_list.get_category(bank) == AUDIOLIB_SOUNDBANK_STATE_READY;
}

// 0x004A5A60 (:2812)
bool audiolib_enable_wavebank_precache(const char* filename, mempool_base* soundheaders_preload_pool_ptr, mempool_base* temp_mempool,
                                       unsigned int num_sound_files)
{
    // The wavebank headers packfile: its directory to temp_mempool (released again), its data indexed by file name.
    mempool_mark_scope mark(temp_mempool);
    soundheaders_hash_table.init(*soundheaders_preload_pool_ptr, num_sound_files);
    v_packfile packfile;
    if (!packfile_read_condensed(filename, temp_mempool, soundheaders_preload_pool_ptr, &packfile, false))
    {
        soundheaders_preload_pool_ptr->clear();
        return false;
    }
    for (unsigned int i = 0; i < packfile.num_files; ++i)
    {
        const v_packfile_entry& entry = packfile.dir[i];
        preloaded_soundheader_data header;
        header.data = packfile.data + entry.start;
        header.size = entry.size;
        soundheaders_hash_table.insert(entry.name, header); // find(name, true) + data store, inlined in the original
    }
    Audiolib_Wavebank_Precache_Enabled = true;
    return true;
}

// 0x00483870 (:2843)
bool audiolib_wavebank_precaching_enabled()
{
    return Audiolib_Wavebank_Precache_Enabled;
}

// 0x00483880 (:2848)
void audiolib_disable_wavebank_precache()
{
    Audiolib_wavebank_precache_cs.enter();
    Audiolib_Wavebank_Precache_Enabled = false;
    Audiolib_wavebank_precache_cs.leave();
}

// 0x004A5BE0 (:2856)
bool audiolib_internal_fetch_precached_wavebank_header(const char* filename, unsigned int offset, unsigned int size, unsigned char* buf)
{
    Audiolib_wavebank_precache_cs.enter();
    if (Audiolib_Wavebank_Precache_Enabled)
    {
        // The headers are stored as "<wavebank>.xwbh_xbox2".
        char name[64];
        char extension[64];
        cf_drop_ext(name, sizeof(name), filename);
        _snprintf_safe(extension, sizeof(extension), ".xwbh_xbox2");
        strcat(name, extension);
        preloaded_soundheader_data header;
        if (soundheaders_hash_table.get(name, header))
        {
            memcpy(buf, header.data + offset, size);
            Audiolib_wavebank_precache_cs.leave();
            return true;
        }
    }
    Audiolib_wavebank_precache_cs.leave();
    return false;
}

// 0x004838A0 (:2883)
void audiolib_set_thread_ownership()
{
    if (Audiolib_state.pool != nullptr)
    {
        Audiolib_state.pool->set_thread_ownership();
    }
}

// 0x004838C0 (:2890)
bool audiolib_scalar_will_play(int8_t scalar)
{
    // Sound enabled, the master scalar (6) and the given scalar above 0.
    const bool enabled = Audiolib_panicking ? Audiolib_panicking_deferred_enabled_state : (Audiolib_state.initialized && Audiolib_state.enabled);
    if (!enabled)
    {
        return false;
    }
    float volume = 1.0f;
    if (Audiolib_state.initialized)
    {
        volume = Audiolib_state.volume_scalars[6];
        if (!(0.0f < volume))
        {
            return false;
        }
    }
    if (Audiolib_state.initialized && (unsigned char)scalar < 8)
    {
        volume = Audiolib_state.volume_scalars[scalar];
    }
    return 0.0f < volume;
}

// 0x004A5CF0 (:2910)
void audiolib_dlc_init(int bundle_num, mempool* pool, keen::LanguageId audio_lang)
{
    char soundbanks[64];
    char wavebanks[64];
    _snprintf_safe(soundbanks, sizeof(soundbanks), "dlc%02d_soundbanks_%s.xtbl", bundle_num, keen::getLanguageIdString(audio_lang));
    _snprintf_safe(wavebanks, sizeof(wavebanks), "dlc%02d_wavebanks_%s.xtbl", bundle_num, keen::getLanguageIdString(audio_lang));
    keen::changeStringToLowercase(soundbanks);
    keen::replaceStringCharacters(soundbanks, '-', '_');
    keen::changeStringToLowercase(wavebanks);
    keen::replaceStringCharacters(wavebanks, '-', '_');
    // A bundle without bank tables still becomes the DLC id.
    if (cf_exist(soundbanks) && cf_exist(wavebanks))
    {
        if (audiolib_parse_soundbank_xtbl(soundbanks, true, true, pool) != AUDIOLIB_ERR_NONE)
        {
            return;
        }
        if (audiolib_parse_wavebank_xtbl(wavebanks, true, true) != AUDIOLIB_ERR_NONE)
        {
            return;
        }
    }
    audiolib_soundbank_dlc_init();
    Audiolib_state.dlc_id = bundle_num;
}

// 0x004A93A0 (:2939)
void audiolib_dlc_shutdown()
{
    audiolib_soundbank_dlc_shutdown();
    Audiolib_state.dlc_id = -1;
}

// 0x004A5E00 (:2946)
void audiolib_voices_init(keen::LanguageId audio_lang)
{
    Audio_voice_banks_mempool.set_thread_ownership();
    char name[64];
    audiolib_language_table_name(name, "soundbanks_voice_%s.xtbl", audio_lang);
    audiolib_parse_soundbank_xtbl(name, true, false, &Audio_voice_banks_mempool);
    audiolib_language_table_name(name, "wavebanks_voice_%s.xtbl", audio_lang);
    audiolib_parse_wavebank_xtbl(name, true, false);
    audiolib_soundbank_voice_init();
}

// 0x004A93B0 (:2972)
void audiolib_voices_shutdown()
{
    Audio_voice_banks_mempool.set_thread_ownership();
    audiolib_soundbank_voice_shutdown();
    Audio_voice_banks_mempool.clear();
}
