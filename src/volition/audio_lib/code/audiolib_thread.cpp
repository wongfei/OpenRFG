// Original: volition/audio_lib/code/audiolib_thread.cpp
//
// The Main Audio Thread (SFX.md SX2): a ~10 ms loop over the registered process functions (the play list, the stream's
// deferred events, gameaudio_process), with the spike time of a pass over 30 ms. Compiled in audio_lib_unity.cpp: the
// thread info Audio_thread is a static of audiolib.cpp there.

#include "volition/audio_lib/code/audiolib_thread.h"

#include "volition/audio_lib/code/audiolib_internal.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/os.h"
#include "volition/vlib/code/os/timer.h"

#include <string.h>

// ---- file-static globals
// audiolib_thread_info Audio_thread (0x014F9CB8) is used here too; placed in audiolib.cpp.
static int Audio_thread_spike_time; // 0x01A7AEF0
static cs_wrapper Audio_thread_spike_cs(0); // 0x01A9D030

// 0x00482FB0 (.h:63)
audiolib_thread_info::audiolib_thread_info()
{
    // Only the stack size: the other fields are set by whoever starts the thread.
    stack_size = 0x4000;
}

// 0x0048A1F0 (:83)
void audiolib_thread_set_spike_time_ms(int start_time)
{
    const int now = timer_get(TIMER_MILLISECONDS);
    Audio_thread_spike_cs.enter();
    // A pass may take 30 ms before it counts as a spike; only the excess is recorded.
    if (now - start_time > 30)
    {
        Audio_thread_spike_time = now - start_time - 30;
    }
    Audio_thread_spike_cs.leave();
}

// 0x0048A230 (:93)
int audiolib_thread_get_spike_time_ms()
{
    Audio_thread_spike_cs.enter();
    const int spike_time = Audio_thread_spike_time;
    Audio_thread_spike_time = 0;
    Audio_thread_spike_cs.leave();
    return spike_time;
}

// 0x0048A260 (:103)
void audiolib_thread_reset_spike_time_ms()
{
    Audio_thread_spike_cs.enter();
    Audio_thread_spike_time = 0;
    Audio_thread_spike_cs.leave();
}

// 0x0049DCF0 (:116)
static void audiolib_main_thread(audiolib_thread_info* info)
{
    int process_time = 0;
    audiolib_thread_reset_spike_time_ms();
    int pass_start_us = timer_get(TIMER_MICROSECONDS);
    while (!info->shutdown)
    {
        if (Audiolib_state.initialized)
        {
            const int spike_start = timer_get(TIMER_MILLISECONDS);
            const int process_start = timer_get(TIMER_MILLISECONDS);
            Audio_process_list.thread_access.enter();
            audiolib_process_data* next = Audio_process_list.get_first(0);
            while (next != nullptr)
            {
                audiolib_process_data* process = next;
                next = Audio_process_list.get_next(process);
                if (process->function == nullptr)
                {
                    continue;
                }
                const int now = timer_get(TIMER_MILLISECONDS);
                int time_step = now - process->last_called_time;
                process->last_called_time = now;
                if (time_step < 0)
                {
                    time_step = 0;
                }
                const bool done = process->function(process->user_data, now, time_step);
                const int end = timer_get(TIMER_MILLISECONDS);
                if (done)
                {
                    // A process that returns true is finished and leaves the list.
                    Audio_process_list.free(process);
                }
                else if (process->max_time < end - now)
                {
                    process->max_time = end - now;
                }
            }
            Audio_process_list.thread_access.leave();
            process_time = timer_get(TIMER_MILLISECONDS) - process_start;
            audiolib_thread_set_spike_time_ms(spike_start);
        }
        Audiolib_state.frame_stats.total_cpu_usage = timer_get(TIMER_MICROSECONDS) - pass_start_us;
        // A 10 ms period minus this pass, at least 1 ms (the comparison is < 2 in rfg.exe, so 1 ms also when 1 is left).
        int sleep_ms = 10 - Audiolib_state.frame_stats.total_cpu_usage / 1000;
        if (sleep_ms < 2)
        {
            sleep_ms = 1;
        }
        Audiolib_state.frame_stats.cpu_usage = process_time;
        os_sleep(sleep_ms);
        pass_start_us = timer_get(TIMER_MICROSECONDS);
    }
}

// 0x0048A280 (:229)
int audiolib_thread_start_func(void* lp_param)
{
    audiolib_thread_info* info = (audiolib_thread_info*)lp_param;
    if (info == nullptr)
    {
        return -1;
    }
    if (info->function == nullptr)
    {
        return -2;
    }
    info->function(info);
    return 0;
}

// 0x0048A2B0 (:248)
audiolib_result audiolib_start_new_thread(audiolib_thread_info* thread_info)
{
    thread_info->handle = thread_spawn(audiolib_thread_start_func, thread_info, thread_info->cpu, thread_info->priority,
                                       thread_info->name, thread_info->stack_size);
    return AUDIOLIB_ERR_NONE;
}

// 0x004A2A50 (:258)
audiolib_result audiolib_thread_system_init()
{
    Audio_process_list.clear();
    strncpy(Audio_thread.name, "Main Audio Thread", 64);
    Audio_thread.function = audiolib_main_thread;
    Audio_thread.handle = 0xFFFF; // thread_spawn's failure handle
    Audio_thread.shutdown = false;
    Audio_thread.priority = THREAD_PRIOR_HIGH;
    Audio_thread.cpu = 0;
    Audio_thread.stack_size = 0x8000;
    audiolib_start_new_thread(&Audio_thread);
    return AUDIOLIB_ERR_NONE;
}

// 0x0048A2E0 (:281)
void audiolib_thread_system_shutdown()
{
    if (Audio_thread.handle != 0xFFFF)
    {
        Audio_thread.shutdown = true;
        thread_wait_until_done(Audio_thread.handle);
        Audio_thread.handle = 0xFFFF;
    }
}

// 0x00492610 (:291)
audiolib_result audiolib_thread_set_hw_thread(int thread_no)
{
    (void)thread_no;
    return AUDIOLIB_ERR_NONE;
}

// 0x004972C0 (:296)
audiolib_result audiolib_thread_function_register(bool (*function)(void*, int, int), const char* name, void* user_data)
{
    audiolib_process_data* process = Audio_process_list.alloc(0);
    if (process == nullptr)
    {
        return AUDIOLIB_ERR_NO_PROCESS_SLOTS;
    }
    process->user_data = user_data;
    process->function = function;
    strncpy(process->func_name, name != nullptr ? name : "UN-NAMED", 64);
    return AUDIOLIB_ERR_NONE;
}

// 0x0049DF20 (:319)
audiolib_result audiolib_thread_function_unregister(bool (*function)(void*, int, int))
{
    Audio_process_list.thread_access.enter();
    audiolib_process_data* next = Audio_process_list.get_first(0);
    while (next != nullptr)
    {
        audiolib_process_data* process = next;
        next = Audio_process_list.get_next(process);
        if (process->function == function)
        {
            Audio_process_list.free(process);
        }
    }
    Audio_process_list.thread_access.leave();
    return AUDIOLIB_ERR_NONE;
}
