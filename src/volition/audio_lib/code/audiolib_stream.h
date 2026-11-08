// Original: volition/audio_lib/code/audiolib_stream.h
// Implementation: volition/audio_lib/code/audiolib_stream.cpp
//
// Streamed sounds: the stream thread processes play and cache events (load, prepare, play, pause, more data, stop) from
// the event queue. Placement of the event types inferred.
#pragma once

#include "volition/audio_lib/code/audiolib_enums.h"

struct audiolib_play_data;
struct audiolib_thread_info;

// size 0x8
struct audiolib_stream_play_pause
{
    audiolib_play_data* play; // 0x00
    bool pause; // 0x04
};

// size 0x8
struct audiolib_stream_cache_length
{
    int cache_index; // 0x00
    int track_length; // 0x04
};

// size 0x10
struct audiolib_stream_event_data
{
    AUDIOLIB_STREAM_EVENT_TYPE event_type; // 0x00
    int timestamp; // 0x04
    union
    {
        audiolib_stream_play_pause pp; // 0x00
        audiolib_stream_cache_length cl; // 0x00
    } eu; // 0x08
};

AUDIOLIB_EVENTQUEUE_PRIORITIES audiostream_get_event_default_priority(AUDIOLIB_STREAM_EVENT_TYPE event_type); // 0x0048A110 (:255)
audiolib_result audiolib_stream_add_play_event(audiolib_play_data* play, AUDIOLIB_STREAM_EVENT_TYPE event_type, bool pause, AUDIOLIB_EVENTQUEUE_PRIORITIES priority); // 0x0049D7F0 (:291)
audiolib_result audiolib_stream_add_cache_event(AUDIOLIB_STREAM_EVENT_TYPE event_type, int cache_index, int track_length); // 0x0049D8B0 (:333)
audiolib_result audiolib_stream_peek_event(audiolib_stream_event_data* event_data, AUDIOLIB_EVENTQUEUE_PRIORITIES highest_priority_to_get, AUDIOLIB_EVENTQUEUE_PRIORITIES lowest_priority_to_get); // 0x00492430 (:371)
audiolib_result audiolib_stream_get_event(audiolib_stream_event_data* event_data, AUDIOLIB_EVENTQUEUE_PRIORITIES highest_priority_to_get, AUDIOLIB_EVENTQUEUE_PRIORITIES lowest_priority_to_get); // 0x0049D950 (:382)
void astream_cue_finished(audiolib_play_data* play); // 0x00492470 (:1362)
void audiolib_stream_thread_main(audiolib_thread_info* info); // 0x004A8A30 (:1651)
audiolib_result audiolib_stream_start_load(audiolib_play_data* play, int offset_ms); // 0x0049D980 (:1691)
audiolib_result audiolib_stream_prepare(audiolib_play_data* play, int offset_ms); // 0x0049DA00 (:1711)
audiolib_result audiolib_stream_play(audiolib_play_data* play); // 0x0049DAA0 (:1752)
audiolib_result audiolib_stream_pause(audiolib_play_data* play, bool pause); // 0x004924B0 (:1765)
audiolib_result audiolib_stream_stop(audiolib_play_data* play); // 0x0049DB10 (:1817)
audiolib_result audiolib_stream_destroy(audiolib_play_data* play); // 0x00492560 (:1834)
audiolib_result audiolib_stream_release_channel(audiolib_play_data* play); // 0x0048A1C0 (:1911)
audiolib_result audiolib_stream_get_channel(audiolib_play_data* play); // 0x00497260 (:1943)
bool audiolib_stream_process_deferred_events(void* user_data, int curr_time, int time_step); // 0x0049DB80 (:2047)
audiolib_result audiolib_stream_init_stage_1(); // 0x004A8C20 (:2086)
audiolib_result audiolib_stream_init_stage_2(); // 0x0048A1D0 (:2121)
void audiolib_stream_shutdown(); // 0x004A2A20 (:2197)
audiolib_result audiolib_stream_get_play_state(audiolib_play_data* play); // 0x00492590 (:2217)
void audiolib_stream_init_play_instance(audiolib_play_data* play); // 0x004925D0 (:2295)
