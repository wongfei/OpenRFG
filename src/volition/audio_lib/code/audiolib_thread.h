// Original: volition/audio_lib/code/audiolib_thread.h
// Implementation: volition/audio_lib/code/audiolib_thread.cpp
//
// The audio threads and the functions registered to run on them, with spike timing.
#pragma once

#include "volition/audio_lib/code/audiolib_enums.h"
#include "volition/vlib/code/os/thread.h"

// size 0x58
struct audiolib_thread_info
{
    audiolib_thread_info(); // 0x00482FB0 (.h:63)

    char name[64]; // 0x00
    void (*function)(audiolib_thread_info*); // 0x40
    unsigned int handle; // 0x44
    thread_prior priority; // 0x48
    unsigned int cpu; // 0x4C
    int stack_size; // 0x50
    bool shutdown; // 0x54
};

void audiolib_thread_set_spike_time_ms(int start_time); // 0x0048A1F0 (:83)
int audiolib_thread_get_spike_time_ms(); // 0x0048A230 (:93)
void audiolib_thread_reset_spike_time_ms(); // 0x0048A260 (:103)
int audiolib_thread_start_func(void* lp_param); // 0x0048A280 (:229)
audiolib_result audiolib_start_new_thread(audiolib_thread_info* thread_info); // 0x0048A2B0 (:248)
audiolib_result audiolib_thread_system_init(); // 0x004A2A50 (:258)
void audiolib_thread_system_shutdown(); // 0x0048A2E0 (:281)
audiolib_result audiolib_thread_set_hw_thread(int thread_no); // 0x00492610 (:291)
audiolib_result audiolib_thread_function_register(bool (*function)(void*, int, int), const char* name, void* user_data); // 0x004972C0 (:296)
audiolib_result audiolib_thread_function_unregister(bool (*function)(void*, int, int)); // 0x0049DF20 (:319)
