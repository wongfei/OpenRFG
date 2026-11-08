// Original: volition/audio_lib/code/audiolib_callbacks.cpp
//
// The internal event queue (EventQueue_list: nine priority lists, read from the highest given priority down) and the
// cue callbacks: marker and end-of-cue events are queued here and forwarded to the game on the
// "marker_user_callback_thread" (SFX.md SX2). Compiled in audio_lib_unity.cpp (the queue and the callback thread info
// are statics of audiolib.cpp there).

#include "volition/audio_lib/code/audiolib_callbacks.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_internal.h"
#include "volition/audio_lib/code/audiolib_thread.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/sync_event.h"
#include "volition/vlib/code/os/thread.h"
#include "volition/vlib/code/os/timer.h"

#include <string.h>

// ---- public globals (rfg.pdb; no header declares them, see audiolib_internal.h)
bool Audiolib_release_assert; // 0x01A7AEEC

// ---- file-static globals
// audiolib_thread_info audiolib_callback_thread_info (0x014F9AC8) is used here too; placed in audiolib.cpp.
// sync_event Audiolib_Marker_Trigger (0x01A7AF18) is used here too; placed in audiolib.cpp.
static object_pool<AUDIOLIB_CUE_CALLBACK_DATA,100,1> Audiolib_Marker_Callback_List; // 0x01A7BBE0
static cs_wrapper Audiolib_Callback_list_cs(0); // 0x01A7E4EC
// object_pool<audiolib_internal_eventqueue_data,250,9> EventQueue_list (0x01A7E530) is used here too; placed in audiolib.cpp.
// cs_wrapper Audiolib_EventQueue_list_cs (0x01A811EC) is used here too; placed in audiolib.cpp.

// 0x00485CF0 (:52)
void audiolib_internal_eventqueue_data::invalidate()
{
    memset(user_data, 0, sizeof(user_data));
    user_data_size = 0;
}

// 0x00485D10 (:54)
audiolib_internal_eventqueue_data::audiolib_internal_eventqueue_data()
{
    // timestamp is left as it is.
    memset(user_data, 0, sizeof(user_data));
    user_data_size = 0;
}

// 0x004A0870 (:72)
bool audiolib_internal_eventqueue_system_init()
{
    Audiolib_EventQueue_list_cs.enter();
    EventQueue_list.clear();
    Audiolib_EventQueue_list_cs.leave();
    return true;
}

// 0x004A08A0 (:79)
void audiolib_internal_eventqueue_system_shutdown()
{
    Audiolib_EventQueue_list_cs.enter();
    EventQueue_list.clear();
    Audiolib_EventQueue_list_cs.leave();
}

// 0x00499E70 (:89)
bool audiolib_internal_eventqueue_add_event(void* user_data, int user_data_size, AUDIOLIB_EVENTQUEUE_PRIORITIES priority)
{
    Audiolib_EventQueue_list_cs.enter();
    // Allocated in list 0, then moved to the priority's list.
    audiolib_internal_eventqueue_data* event = EventQueue_list.alloc(0);
    if (event == nullptr)
    {
        if (!Audiolib_release_assert)
        {
            Audiolib_release_assert = true;
        }
        Audiolib_EventQueue_list_cs.leave();
        return false;
    }
    event->timestamp = timer_get(TIMER_MILLISECONDS);
    if (user_data != nullptr && user_data_size != 0)
    {
        memcpy(event->user_data, user_data, user_data_size);
        event->user_data_size = user_data_size;
    }
    EventQueue_list.move(event, priority);
    Audiolib_EventQueue_list_cs.leave();
    return true;
}

// 0x00499F00 (:125)
bool audiolib_internal_eventqueue_get_event(void* user_data, AUDIOLIB_EVENTQUEUE_PRIORITIES highest_priority_to_get, AUDIOLIB_EVENTQUEUE_PRIORITIES lowest_priority_to_get)
{
    Audiolib_EventQueue_list_cs.enter();
    for (int priority = highest_priority_to_get; priority >= (int)lowest_priority_to_get; --priority)
    {
        audiolib_internal_eventqueue_data* event = EventQueue_list.get_first(priority);
        if (event != nullptr)
        {
            if (user_data != nullptr && event->user_data_size != 0)
            {
                memcpy(user_data, event->user_data, event->user_data_size);
            }
            EventQueue_list.free(event);
            Audiolib_EventQueue_list_cs.leave();
            return true;
        }
    }
    Audiolib_EventQueue_list_cs.leave();
    return false;
}

// 0x0048F150 (:160)
bool audiolib_internal_eventqueue_peek_event(void* user_data, AUDIOLIB_EVENTQUEUE_PRIORITIES highest_priority_to_get, AUDIOLIB_EVENTQUEUE_PRIORITIES lowest_priority_to_get)
{
    Audiolib_EventQueue_list_cs.enter();
    for (int priority = highest_priority_to_get; priority >= (int)lowest_priority_to_get; --priority)
    {
        audiolib_internal_eventqueue_data* event = EventQueue_list.get_first(priority);
        if (event != nullptr)
        {
            if (user_data != nullptr && event->user_data_size != 0)
            {
                memcpy(user_data, event->user_data, event->user_data_size);
            }
            Audiolib_EventQueue_list_cs.leave();
            return true;
        }
    }
    Audiolib_EventQueue_list_cs.leave();
    return false;
}

// 0x0048F1D0 (:195)
AUDIOLIB_CUE_CALLBACK_DATA* audiolib_internal_get_first_callback()
{
    Audiolib_Callback_list_cs.enter();
    AUDIOLIB_CUE_CALLBACK_DATA* callback = Audiolib_Marker_Callback_List.get_first(0);
    Audiolib_Callback_list_cs.leave();
    return callback;
}

// 0x00499F90 (:201)
void audiolib_internal_clear_callback(AUDIOLIB_CUE_CALLBACK_DATA* clearme)
{
    Audiolib_Callback_list_cs.enter();
    Audiolib_Marker_Callback_List.free(clearme);
    Audiolib_Callback_list_cs.leave();
}

// The callback of an event queued without one is the play's, if the play still exists.
static void audiolib_internal_callback_call(AUDIOLIB_CUE_CALLBACK_DATA* callback_event)
{
    if (callback_event->function == nullptr)
    {
        Audiolib_play_cs.enter();
        audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(callback_event->play_id);
        if (play != nullptr)
        {
            callback_event->function = play->params.callback_func;
            callback_event->user_data = play->params.callback_data;
        }
        Audiolib_play_cs.leave();
    }
    if (callback_event->function != nullptr)
    {
        callback_event->function(callback_event);
    }
}

// 0x00499FC0 (:209)
bool audiolib_internal_callback_to_game(AUDIOLIB_CUE_CALLBACK_DATA* callback_event)
{
    if (callback_event == nullptr)
    {
        return false;
    }
    audiolib_internal_callback_call(callback_event);
    audiolib_internal_clear_callback(callback_event);
    return true;
}

// 0x004950E0 (:226)
bool audiolib_internal_callback_event_add(AUDIOLIB_CUE_CALLBACK_DATA* callback_event)
{
    if (callback_event == nullptr)
    {
        return false;
    }
    Audiolib_Callback_list_cs.enter();
    AUDIOLIB_CUE_CALLBACK_DATA* queued = Audiolib_Marker_Callback_List.alloc(0);
    if (queued == nullptr)
    {
        Audiolib_Callback_list_cs.leave();
        return false;
    }
    memset(queued, 0, sizeof(*queued));
    queued->function = callback_event->function;
    queued->action = callback_event->action;
    queued->play_id = callback_event->play_id;
    queued->user_data = callback_event->user_data;
    queued->time = callback_event->time;
    strncpy(queued->marker_text, callback_event->marker_text, 64);
    Audiolib_Marker_Trigger.signal();
    Audiolib_Callback_list_cs.leave();
    return true;
}

// 0x00495170 (:253)
bool audiolib_internal_callback_event_add(audiolib_play_data* play, unsigned short action, int time, char* text)
{
    if (play == nullptr || play->params.callback_func == nullptr || (play->params.callback_flags & action) == 0)
    {
        return false;
    }
    AUDIOLIB_CUE_CALLBACK_DATA callback_event;
    memset(&callback_event, 0, sizeof(callback_event));
    callback_event.play_id = play->play_id;
    callback_event.user_data = play->params.callback_data;
    callback_event.action = action;
    callback_event.time = time;
    callback_event.marker_text[0] = '\0';
    callback_event.function = play->params.callback_func;
    if (text != nullptr)
    {
        strncpy(callback_event.marker_text, text, 64);
    }
    return audiolib_internal_callback_event_add(&callback_event);
}

// 0x0049A040 (:276)
void audiolib_marker_user_callback_thread(audiolib_thread_info* info)
{
    // The loop tests the static thread info, not info (the same object).
    (void)info;
    while (!audiolib_callback_thread_info.shutdown)
    {
        Audiolib_Marker_Trigger.wait();
        for (;;)
        {
            AUDIOLIB_CUE_CALLBACK_DATA* callback_event = audiolib_internal_get_first_callback();
            if (callback_event == nullptr)
            {
                break;
            }
            audiolib_internal_callback_call(callback_event);
            audiolib_internal_clear_callback(callback_event);
        }
    }
}

// 0x004A08C0 (:306)
bool audiolib_internal_callback_init()
{
    strncpy(audiolib_callback_thread_info.name, "marker_user_callback_thread", 64);
    audiolib_callback_thread_info.function = audiolib_marker_user_callback_thread;
    audiolib_callback_thread_info.handle = 0;
    audiolib_callback_thread_info.priority = THREAD_PRIOR_NORMAL;
    audiolib_callback_thread_info.cpu = 0;
    audiolib_callback_thread_info.shutdown = false;
    audiolib_callback_thread_info.stack_size = 0x2000;
    Audiolib_Marker_Callback_List.clear();
    audiolib_start_new_thread(&audiolib_callback_thread_info);
    return true;
}

// 0x00485D50 (:327)
void audiolib_internal_callback_shutdown()
{
    audiolib_callback_thread_info.shutdown = true;
    Audiolib_Marker_Trigger.signal();
    thread_wait_until_done(audiolib_callback_thread_info.handle);
}
