// Original: volition/audio_lib/code/audiolib_callbacks.h
// Implementation: volition/audio_lib/code/audiolib_callbacks.cpp
//
// The internal event queue (prioritized, between the audio threads) and the cue callbacks forwarded to the game
// (marker and end-of-cue events).
#pragma once

#include "volition/audio_lib/code/audiolib_enums.h"

struct AUDIOLIB_CUE_CALLBACK_DATA;
struct audiolib_play_data;
struct audiolib_thread_info;

// size 0x28
struct audiolib_internal_eventqueue_data
{
    void invalidate(); // 0x00485CF0 (:52)
    audiolib_internal_eventqueue_data(); // 0x00485D10 (:54)

    int timestamp; // 0x00
    int user_data_size; // 0x04
    char user_data[32]; // 0x08
};

bool audiolib_internal_eventqueue_system_init(); // 0x004A0870 (:72)
void audiolib_internal_eventqueue_system_shutdown(); // 0x004A08A0 (:79)
bool audiolib_internal_eventqueue_add_event(void* user_data, int user_data_size, AUDIOLIB_EVENTQUEUE_PRIORITIES priority); // 0x00499E70 (:89)
bool audiolib_internal_eventqueue_get_event(void* user_data, AUDIOLIB_EVENTQUEUE_PRIORITIES highest_priority_to_get, AUDIOLIB_EVENTQUEUE_PRIORITIES lowest_priority_to_get); // 0x00499F00 (:125)
bool audiolib_internal_eventqueue_peek_event(void* user_data, AUDIOLIB_EVENTQUEUE_PRIORITIES highest_priority_to_get, AUDIOLIB_EVENTQUEUE_PRIORITIES lowest_priority_to_get); // 0x0048F150 (:160)
AUDIOLIB_CUE_CALLBACK_DATA* audiolib_internal_get_first_callback(); // 0x0048F1D0 (:195)
void audiolib_internal_clear_callback(AUDIOLIB_CUE_CALLBACK_DATA* clearme); // 0x00499F90 (:201)
bool audiolib_internal_callback_to_game(AUDIOLIB_CUE_CALLBACK_DATA* callback_event); // 0x00499FC0 (:209)
bool audiolib_internal_callback_event_add(AUDIOLIB_CUE_CALLBACK_DATA* callback_event); // 0x004950E0 (:226)
bool audiolib_internal_callback_event_add(audiolib_play_data* play, unsigned short action, int time, char* text); // 0x00495170 (:253)
void audiolib_marker_user_callback_thread(audiolib_thread_info* info); // 0x0049A040 (:276)
bool audiolib_internal_callback_init(); // 0x004A08C0 (:306)
void audiolib_internal_callback_shutdown(); // 0x00485D50 (:327)
