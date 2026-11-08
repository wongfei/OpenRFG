// Original: volition/audio_lib/code/audiolib_stream.cpp
//
// The audio stream thread and its events (SFX.md SX2). Each pass of "audio_stream_thread_main" updates the keen
// listener and runs keen::SoundSystem::update under m_audio_mutex, then drains the event queue: stop, load/open/seek,
// prepare (the wave becomes a keen sound definition) and play (audiolib_hw_play starts the keen sound). Kaiko's keen
// port replaced Volition's chunked streaming: a streamed wave is read whole at prepare time, so the astream_* buffer
// functions are empty in rfg.exe and stay empty here. Deferred events wait at least 3 ms before they are requeued at
// their own priority. Compiled in audio_lib_unity.cpp (Stream_thread_info and the event queue are statics of
// audiolib.cpp there).

#include "volition/audio_lib/code/audiolib_stream.h"

#include "keen/base/framenumber.hpp"
#include "keen/base/mutex.hpp"
#include "keen/base/thread.hpp"
#include "keen/base/trace.hpp"
#include "keen/sound/soundsystem.hpp"
#include "volition/audio_lib/code/audiolib_callbacks.h"
#include "volition/audio_lib/code/audiolib_hardware.h"
#include "volition/audio_lib/code/audiolib_internal.h"
#include "volition/audio_lib/code/audiolib_soundbank.h"
#include "volition/audio_lib/code/audiolib_thread.h"
#include "volition/audio_lib/code/stub/audiolib_stub.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/sync_event.h"
#include "volition/vlib/code/os/thread.h"
#include "volition/vlib/code/os/timer.h"

#include <string.h>

// ---- public globals (rfg.pdb; no header declares them)
unsigned int audio_stream_thread_main_id; // 0x01A7AF14
cs_wrapper Audiostream_CS(0); // 0x01A97BB0

// ---- file-static globals
// audiolib_thread_info Stream_thread_info (0x014F9C60) is used here too; placed in audiolib.cpp.
// sync_event Audiolib_Stream_Trigger (0x01A7AF1C) is used here too; placed in audiolib.cpp.
// object_pool<audiolib_internal_eventqueue_data,250,9> EventQueue_list (0x01A7E530) is used here too; placed in audiolib.cpp.
// cs_wrapper Audiolib_EventQueue_list_cs (0x01A811EC) is used here too; placed in audiolib.cpp.
static cs_wrapper Audio_cache_cs(0); // 0x01A9D06C

// 0x0048A110 (:255)
AUDIOLIB_EVENTQUEUE_PRIORITIES audiostream_get_event_default_priority(AUDIOLIB_STREAM_EVENT_TYPE event_type)
{
    switch (event_type)
    {
    case AUDIOLIB_EVENT_STREAM_PREPARE:
    case AUDIOLIB_EVENT_STREAM_PLAY:
    case AUDIOLIB_EVENT_STREAM_PAUSE:
    case AUDIOLIB_EVENT_STREAM_STOP:
    case AUDIOLIB_EVENT_STREAM_FINISHED:
    case AUDIOLIB_EVENT_STREAM_DESTROY:
        return AUDIOLIB_EVENTQUEUE_PRIORITY_MEDIUM;
    case AUDIOLIB_EVENT_STREAM_MOREDATA:
    case AUDIOLIB_EVENT_STREAM_FETCH_NEXT_BUFFER:
        return AUDIOLIB_EVENTQUEUE_PRIORITY_HIGH;
    default:
        return AUDIOLIB_EVENTQUEUE_PRIORITY_LOW;
    }
}

// 0x0049D7F0 (:291)
audiolib_result audiolib_stream_add_play_event(audiolib_play_data* play, AUDIOLIB_STREAM_EVENT_TYPE event_type, bool pause, AUDIOLIB_EVENTQUEUE_PRIORITIES priority)
{
    audiolib_stream_event_data event_data;
    event_data.event_type = event_type;
    event_data.eu.pp.play = play;
    event_data.eu.pp.pause = pause;
    event_data.timestamp = timer_get(TIMER_MILLISECONDS);
    if (priority == AUDIOLIB_EVENTQUEUE_PRIORITY_UNKNOWN)
    {
        priority = audiostream_get_event_default_priority(event_type);
    }
    if (!audiolib_internal_eventqueue_add_event(&event_data, sizeof(event_data), priority))
    {
        if (!Audiolib_release_assert)
        {
            Audiolib_release_assert = true;
        }
        return AUDIOLIB_ERR_STREAM_MISC_ERROR;
    }
    // A deferred event waits for audiolib_stream_process_deferred_events; the others wake the stream thread.
    if (priority > AUDIOLIB_EVENTQUEUE_PRIORITY_DEFERRED)
    {
        Audiolib_Stream_Trigger.signal();
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x0049D8B0 (:333)
audiolib_result audiolib_stream_add_cache_event(AUDIOLIB_STREAM_EVENT_TYPE event_type, int cache_index, int track_length)
{
    audiolib_stream_event_data event_data;
    event_data.event_type = event_type;
    event_data.eu.cl.cache_index = cache_index;
    event_data.eu.cl.track_length = track_length;
    event_data.timestamp = timer_get(TIMER_MILLISECONDS);
    const AUDIOLIB_EVENTQUEUE_PRIORITIES priority = audiostream_get_event_default_priority(event_type);
    if (!audiolib_internal_eventqueue_add_event(&event_data, sizeof(event_data), priority))
    {
        return AUDIOLIB_ERR_STREAM_MISC_ERROR;
    }
    if (priority > AUDIOLIB_EVENTQUEUE_PRIORITY_DEFERRED)
    {
        Audiolib_Stream_Trigger.signal();
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x00492430 (:371)
audiolib_result audiolib_stream_peek_event(audiolib_stream_event_data* event_data, AUDIOLIB_EVENTQUEUE_PRIORITIES highest_priority_to_get, AUDIOLIB_EVENTQUEUE_PRIORITIES lowest_priority_to_get)
{
    return audiolib_internal_eventqueue_peek_event(event_data, highest_priority_to_get, lowest_priority_to_get)
               ? AUDIOLIB_ERR_NONE
               : AUDIOLIB_ERR_STREAM_MISC_ERROR;
}

// 0x0049D950 (:382)
audiolib_result audiolib_stream_get_event(audiolib_stream_event_data* event_data, AUDIOLIB_EVENTQUEUE_PRIORITIES highest_priority_to_get, AUDIOLIB_EVENTQUEUE_PRIORITIES lowest_priority_to_get)
{
    return audiolib_internal_eventqueue_get_event(event_data, highest_priority_to_get, lowest_priority_to_get)
               ? AUDIOLIB_ERR_NONE
               : AUDIOLIB_ERR_STREAM_MISC_ERROR;
}

// 0x00492460 (:400)
static void astream_loading_failed(audiolib_play_data* play)
{
    play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_STOPPED;
}

// The chunked streaming buffers of Volition's audio_lib: empty in rfg.exe (SFX.md 1.3).

// 0x0048A150 (:1300)
static void astream_free_stream_buffer(audiolib_play_data* play)
{
    (void)play;
}

// 0x0048A160 (:1303)
static void astream_fetch_moredata(audiolib_play_data* play)
{
    (void)play;
}

// 0x0048A170 (:1306)
static void astream_destroy_buffers(int cache_index, int track_length)
{
    (void)cache_index;
    (void)track_length;
}

// 0x0048A180 (:1309)
static void astream_check_first_buffer_load_finished(audiolib_play_data* play)
{
    (void)play;
}

// 0x0048A190 (:1313)
static void astream_check_next_buffer_load_finished(audiolib_play_data* play)
{
    (void)play;
}

// 0x0048A1A0 (:1317)
static void astream_check_cache_load_finished(audiolib_play_data* play)
{
    (void)play;
}

// 0x0048A1B0 (:1321)
static int astream_load_cache_entry(audiolib_play_data* play)
{
    (void)play;
    return -1;
}

// 0x004A27E0 (:1326), play in ESI
static void astream_play(audiolib_play_data* play, bool reloop)
{
    (void)reloop;
    if (audiolib_hw_play(play, nullptr, 0) < 0)
    {
        audiolib_stream_stop(play);
        return;
    }
    play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_PLAYING;
}

// 0x00492470 (:1362)
void astream_cue_finished(audiolib_play_data* play)
{
    if (play == nullptr)
    {
        keen::debug::nativePrintArgs("[Audiolib] <audiolib_hw_cue_finished> play is a nullptr! \n");
        return;
    }
    play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_STOPPED;
}

// 0x004A8910 (:1339), play in ESI
static void astream_prepare(audiolib_play_data* play)
{
    // audiolib_hw_prepare and astream_cue_finished are inlined in rfg.exe. Original: a null play is reported by both
    // and then written to (never reached: the events always carry a play).
    if (audiolib_hw_prepare(play, 0) < 0)
    {
        astream_cue_finished(play);
        play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_DESTROYED;
        return;
    }
    play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_READY;
}

// 0x00492490 (:1369), play in EAX
static void astream_cleanup_on_finish(audiolib_play_data* play)
{
    astream_cue_finished(play);
}

// 0x004A2810 (:1387), play in ESI
static void astream_stop(audiolib_play_data* play)
{
    const audiolib_hw_play_state state = play->hw_play_state;
    if (state == AUDIOLIB_HW_PLAY_STATE_UNUSED || state >= AUDIOLIB_HW_PLAY_STATE_STOPPED)
    {
        return;
    }
    play->stream_info.loop_track = false;
    if (state == AUDIOLIB_HW_PLAY_STATE_LOADING || state == AUDIOLIB_HW_PLAY_STATE_LOAD_REQUEST)
    {
        if (!Audiolib_state.in_shutdown)
        {
            // Still loading: retried from the deferred queue.
            if (audiolib_stream_add_play_event(play, AUDIOLIB_EVENT_STREAM_STOP, false, AUDIOLIB_EVENTQUEUE_PRIORITY_DEFERRED) < 0)
            {
                if (!Audiolib_release_assert)
                {
                    Audiolib_release_assert = true;
                }
                audiolib_stream_stop(play);
            }
            return;
        }
    }
    else
    {
        if (state == AUDIOLIB_HW_PLAY_STATE_PENDING_FREE_CHANNEL && play->stream_info.data_location == AUDIOLIB_STREAM_DATA_PERFORM_CACHE_LOAD)
        {
            play->stream_info.data_location = AUDIOLIB_STREAM_DATA_INVALID;
        }
        if ((state >= AUDIOLIB_HW_PLAY_STATE_LOADED && state <= AUDIOLIB_HW_PLAY_STATE_READY) || state == AUDIOLIB_HW_PLAY_STATE_PENDING_FREE_CHANNEL)
        {
            astream_cleanup_on_finish(play);
        }
        if (audiolib_hw_stop(play) != AUDIOLIB_ERR_NONE)
        {
            return;
        }
    }
    play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_STOPPED;
}

// 0x004A28B0 (:1483), play in ECX
static void astream_load_cue_open_seek(audiolib_play_data* play)
{
    char path[256];
    // Action 0x8000: the play is being destroyed.
    if (play->hw_play_state >= AUDIOLIB_HW_PLAY_STATE_STOPPING || (play->action & 0x8000) != 0)
    {
        return;
    }
    play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_LOADING;
    if (play->foley_data.looping)
    {
        play->stream_info.loop_track = true;
    }
    audiolib_soundbank_data* wavebank = audiolib_internal_soundbank_id_to_ptr(play->foley_data.wavebank_id, true);
    if (wavebank == nullptr)
    {
        play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_STOPPED;
        return;
    }
    const AUDIOLIB_STREAMING_DATA_LOCATION location = play->stream_info.data_location;
    if (location == AUDIOLIB_STREAM_DATA_IN_RAM || location == AUDIOLIB_STREAM_DATA_IN_CACHE)
    {
        play->stream_info.data_remaining = 0;
    }
    else
    {
        if (location == AUDIOLIB_STREAM_DATA_PERFORM_CACHE_LOAD)
        {
            // Original: the cache load only builds the name; the play stays LOADING.
            audiolib_hw_soundbank_create_file_and_pathname(wavebank->bank_name, wavebank->sub_dir, wavebank->bank_type,
                                                           play->stream_info.bank_name, path);
            return;
        }
        // Params flag 0x100: stop once loaded.
        if ((play->params.flags & 0x100) != 0)
        {
            play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_LOADED;
            astream_stop(play);
            return;
        }
        audiolib_hw_soundbank_create_file_and_pathname(wavebank->bank_name, wavebank->sub_dir, wavebank->bank_type,
                                                       play->stream_info.bank_name, path);
    }
    play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_LOADED;
}

// 0x004A8980 (:1605), event_data in EAX
static void astream_process_one_event(audiolib_stream_event_data* event_data)
{
    if (event_data == nullptr)
    {
        return;
    }
    audiolib_play_data* play = event_data->eu.pp.play;
    switch (event_data->event_type)
    {
    case AUDIOLIB_EVENT_STREAM_STOP_AFTER_LOADING:
    case AUDIOLIB_EVENT_STREAM_STOP:
        astream_stop(play);
        break;
    case AUDIOLIB_EVENT_STREAM_LOAD_START:
        astream_load_cue_open_seek(play);
        break;
    case AUDIOLIB_EVENT_STREAM_PREPARE:
        astream_prepare(play);
        break;
    case AUDIOLIB_EVENT_STREAM_PLAY:
        astream_play(play, false);
        break;
    case AUDIOLIB_EVENT_STREAM_FINISHED:
        astream_cleanup_on_finish(play);
        break;
    case AUDIOLIB_EVENT_STREAM_FETCH_FIRST_BUFFER:
    case AUDIOLIB_EVENT_STREAM_CACHELOAD:
    case AUDIOLIB_EVENT_STREAM_MOREDATA:
    case AUDIOLIB_EVENT_STREAM_FETCH_NEXT_BUFFER:
    case AUDIOLIB_EVENT_STREAM_DESTROY:
        break;
    default:
        // The pause event and unknown types (0x66B = line 1643).
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "C:/unit4projects/rfg/root/code/volition/audio_lib/code/audiolib_stream.cpp", 0x66B);
        break;
    }
}

// 0x004A8A30 (:1651)
void audiolib_stream_thread_main(audiolib_thread_info* info)
{
    (void)info;
    audio_stream_thread_main_id = keen::getCurrentThreadId();
    while (!Audiolib_Stream_Shutdown)
    {
        if (!Audiolib_state.in_shutdown && m_pSoundSystem != nullptr)
        {
            audiolib_update_listener_position();
            const float timeStep = keen::getLastFrameDurationInSeconds();
            m_audio_mutex.lock(0);
            keen::SoundSystem::update(m_pSoundSystem, timeStep);
            m_audio_mutex.unlock();
        }
        bool processed = false;
        audiolib_stream_event_data event_data;
        while (audiolib_internal_eventqueue_get_event(&event_data, AUDIOLIB_EVENTQUEUE_PRIORITY_HIGHEST, AUDIOLIB_EVENTQUEUE_PRIORITY_LOWEST))
        {
            // astream_process_one_event, inlined in rfg.exe, under Audiostream_CS.
            Audiostream_CS.enter();
            processed = true;
            astream_process_one_event(&event_data);
            Audiostream_CS.leave();
        }
        if (!processed)
        {
            // Original: a plain 30 ms sleep; Audiolib_Stream_Trigger is signalled but never waited on.
            keen::sleepCurrentThreadMilliseconds(30);
        }
    }
}

// 0x0049D980 (:1691)
audiolib_result audiolib_stream_start_load(audiolib_play_data* play, int offset_ms)
{
    (void)offset_ms;
    if (play->hw_play_state < AUDIOLIB_HW_PLAY_STATE_LOADING)
    {
        audiolib_stream_event_data event_data;
        event_data.event_type = AUDIOLIB_EVENT_STREAM_LOAD_START;
        event_data.eu.pp.play = play;
        event_data.eu.pp.pause = false;
        event_data.timestamp = timer_get(TIMER_MILLISECONDS);
        if (!audiolib_internal_eventqueue_add_event(&event_data, sizeof(event_data), AUDIOLIB_EVENTQUEUE_PRIORITY_LOW))
        {
            if (!Audiolib_release_assert)
            {
                Audiolib_release_assert = true;
            }
            astream_loading_failed(play);
            return AUDIOLIB_ERR_STREAM_MISC_ERROR;
        }
        Audiolib_Stream_Trigger.signal();
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x0049DA00 (:1711)
audiolib_result audiolib_stream_prepare(audiolib_play_data* play, int offset_ms)
{
    (void)offset_ms;
    if (play->play_id == -1)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (play->hw_play_state > AUDIOLIB_HW_PLAY_STATE_PREPARING)
    {
        return AUDIOLIB_ERR_NONE;
    }
    if (play->hw_play_state >= AUDIOLIB_HW_PLAY_STATE_LOADED)
    {
        audiolib_stream_event_data event_data;
        event_data.event_type = AUDIOLIB_EVENT_STREAM_PREPARE;
        event_data.eu.pp.play = play;
        event_data.eu.pp.pause = false;
        event_data.timestamp = timer_get(TIMER_MILLISECONDS);
        if (audiolib_internal_eventqueue_add_event(&event_data, sizeof(event_data), AUDIOLIB_EVENTQUEUE_PRIORITY_MEDIUM))
        {
            Audiolib_Stream_Trigger.signal();
            play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_PREPARING;
            return AUDIOLIB_ERR_NONE;
        }
        if (!Audiolib_release_assert)
        {
            Audiolib_release_assert = true;
        }
        astream_cue_finished(play);
    }
    // Original: also when the play is not loaded yet.
    return AUDIOLIB_ERR_STREAM_MISC_ERROR;
}

// 0x0049DAA0 (:1752)
audiolib_result audiolib_stream_play(audiolib_play_data* play)
{
    audiolib_stream_event_data event_data;
    event_data.event_type = AUDIOLIB_EVENT_STREAM_PLAY;
    event_data.eu.pp.play = play;
    event_data.eu.pp.pause = false;
    event_data.timestamp = timer_get(TIMER_MILLISECONDS);
    if (!audiolib_internal_eventqueue_add_event(&event_data, sizeof(event_data), AUDIOLIB_EVENTQUEUE_PRIORITY_MEDIUM))
    {
        if (!Audiolib_release_assert)
        {
            Audiolib_release_assert = true;
        }
        return AUDIOLIB_ERR_STREAM_MISC_ERROR;
    }
    Audiolib_Stream_Trigger.signal();
    return AUDIOLIB_ERR_NONE;
}

// 0x004924B0 (:1765)
audiolib_result audiolib_stream_pause(audiolib_play_data* play, bool pause)
{
    const audiolib_hw_play_state state = play->hw_play_state;
    if (state >= AUDIOLIB_HW_PLAY_STATE_STOPPING)
    {
        return AUDIOLIB_ERR_NONE;
    }
    audiolib_result result = AUDIOLIB_ERR_NONE;
    if (state == AUDIOLIB_HW_PLAY_STATE_PLAYING)
    {
        // Original: unpausing a playing sound does nothing.
        if (pause)
        {
            result = audiolib_hw_pause(play, true);
            if (result == AUDIOLIB_ERR_NONE)
            {
                play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_PAUSED;
            }
        }
    }
    else if ((state == AUDIOLIB_HW_PLAY_STATE_STALLING || state == AUDIOLIB_HW_PLAY_STATE_STALLED) && pause)
    {
        result = audiolib_hw_pause(play, true);
        if (result == AUDIOLIB_ERR_NONE)
        {
            play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_STALLED;
        }
    }
    else if ((state == AUDIOLIB_HW_PLAY_STATE_PAUSED || state == AUDIOLIB_HW_PLAY_STATE_UNSTALLED) && !pause)
    {
        result = audiolib_hw_pause(play, false);
        if (result == AUDIOLIB_ERR_NONE)
        {
            play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_PLAYING;
        }
    }
    else
    {
        result = audiolib_hw_pause(play, pause);
    }
    return result;
}

// 0x0049DB10 (:1817)
audiolib_result audiolib_stream_stop(audiolib_play_data* play)
{
    if (play->hw_play_state < AUDIOLIB_HW_PLAY_STATE_STOPPING)
    {
        audiolib_stream_event_data event_data;
        event_data.event_type = AUDIOLIB_EVENT_STREAM_STOP;
        event_data.eu.pp.play = play;
        event_data.eu.pp.pause = false;
        event_data.timestamp = timer_get(TIMER_MILLISECONDS);
        if (!audiolib_internal_eventqueue_add_event(&event_data, sizeof(event_data), AUDIOLIB_EVENTQUEUE_PRIORITY_MEDIUM))
        {
            if (!Audiolib_release_assert)
            {
                Audiolib_release_assert = true;
            }
            return AUDIOLIB_ERR_STREAM_MISC_ERROR;
        }
        Audiolib_Stream_Trigger.signal();
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x00492560 (:1834)
audiolib_result audiolib_stream_destroy(audiolib_play_data* play)
{
    if (play->hw_play_state < AUDIOLIB_HW_PLAY_STATE_LOAD_REQUEST && !Audiolib_state.in_shutdown)
    {
        return AUDIOLIB_ERR_NONE;
    }
    return audiolib_hw_destroy(play);
}

// 0x0048A1C0 (:1911)
audiolib_result audiolib_stream_release_channel(audiolib_play_data* play)
{
    (void)play;
    return AUDIOLIB_ERR_NONE;
}

// 0x00497260 (:1943)
audiolib_result audiolib_stream_get_channel(audiolib_play_data* play)
{
    if (play->hw_play_state == AUDIOLIB_HW_PLAY_STATE_PENDING_FREE_CHANNEL && play->stream_info.stream_buffer_index == -1 &&
        play->stream_info.cache_index == -1)
    {
        play->stream_info.data_location = audiolib_internal_soundbank_is_bank_in_RAM(play->foley_data.wavebank_id)
                                              ? AUDIOLIB_STREAM_DATA_IN_RAM
                                              : AUDIOLIB_STREAM_DATA_NEEDS_STREAMING;
        return AUDIOLIB_ERR_NONE;
    }
    return AUDIOLIB_ERR_STREAM_MISC_ERROR;
}

// 0x0049DB80 (:2047)
bool audiolib_stream_process_deferred_events(void* user_data, int curr_time, int time_step)
{
    (void)user_data;
    (void)curr_time;
    (void)time_step;
    audiolib_stream_event_data event_data;
    for (;;)
    {
        if (!audiolib_internal_eventqueue_peek_event(&event_data, AUDIOLIB_EVENTQUEUE_PRIORITY_DEFERRED, AUDIOLIB_EVENTQUEUE_PRIORITY_DEFERRED))
        {
            return false;
        }
        // A deferred event is held for 3 ms.
        if (timer_get(TIMER_MILLISECONDS) - event_data.timestamp < 3)
        {
            return false;
        }
        if (!audiolib_internal_eventqueue_get_event(&event_data, AUDIOLIB_EVENTQUEUE_PRIORITY_DEFERRED, AUDIOLIB_EVENTQUEUE_PRIORITY_DEFERRED))
        {
            return false;
        }
        // audiolib_stream_add_play_event with the default priority, inlined.
        audiolib_stream_event_data requeued;
        requeued.event_type = event_data.event_type;
        requeued.eu.pp.play = event_data.eu.pp.play;
        requeued.eu.pp.pause = event_data.eu.pp.pause;
        requeued.timestamp = timer_get(TIMER_MILLISECONDS);
        const AUDIOLIB_EVENTQUEUE_PRIORITIES priority = audiostream_get_event_default_priority(event_data.event_type);
        if (!audiolib_internal_eventqueue_add_event(&requeued, sizeof(requeued), priority))
        {
            if (!Audiolib_release_assert)
            {
                Audiolib_release_assert = true;
            }
            if (!Audiolib_release_assert)
            {
                Audiolib_release_assert = true;
            }
            return false;
        }
        if (priority > AUDIOLIB_EVENTQUEUE_PRIORITY_DEFERRED)
        {
            Audiolib_Stream_Trigger.signal();
        }
    }
}

// 0x004A8C20 (:2086)
audiolib_result audiolib_stream_init_stage_1()
{
    strncpy(Stream_thread_info.name, "audio_stream_thread_main", 64);
    Stream_thread_info.function = audiolib_stream_thread_main;
    Stream_thread_info.handle = 0;
    Stream_thread_info.priority = THREAD_PRIOR_HIGH_PLUS1;
    Stream_thread_info.cpu = 0;
    Stream_thread_info.shutdown = false;
    // The stack size is the constructor's 0x4000.
    Audiolib_Stream_Shutdown = false;
    audiolib_start_new_thread(&Stream_thread_info);
    return audiolib_thread_function_register(audiolib_stream_process_deferred_events, "audiostream_deferred_event", nullptr);
}

// 0x0048A1D0 (:2121)
audiolib_result audiolib_stream_init_stage_2()
{
    return AUDIOLIB_ERR_NONE;
}

// 0x004A2A20 (:2197)
void audiolib_stream_shutdown()
{
    audiolib_thread_function_unregister(audiolib_stream_process_deferred_events);
    Audiolib_Stream_Shutdown = true;
    Audiolib_Stream_Trigger.signal();
    thread_wait_until_done(Stream_thread_info.handle);
}

// 0x00492590 (:2217)
audiolib_result audiolib_stream_get_play_state(audiolib_play_data* play)
{
    if (play == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    audiolib_hw_play_state state = play->hw_play_state;
    if (state > AUDIOLIB_HW_PLAY_STATE_PENDING_FREE_CHANNEL)
    {
        state = audiolib_hw_play_get_audiolib_state(play);
    }
    if (state != play->hw_play_state)
    {
        play->hw_play_state = state;
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x004925D0 (:2295)
void audiolib_stream_init_play_instance(audiolib_play_data* play)
{
    memset(&play->stream_info, 0, sizeof(play->stream_info));
    play->stream_info.stream_buffer_index = -1;
    play->stream_info.loop_buffer_index = -1;
    play->stream_info.cache_index = -1;
}
