// Original: volition/vlib/code/os/frametime.cpp
//
// The frame time (frametime.h). vlib_unity.obj is a unity build: frametime_calculate and frametime_unpause read
// timer.cpp's file statics Timer_inited and Global_time directly, which is timer_get(TIMER_MICROSECONDS) inlined (the
// same check, the same "Int3 critical function" failure); this file calls timer_get. Initial values are read from the
// binary's .data section.
//
// Build: compiled into the rfg_loop library (multiplayer.md MP1) with the original game loop, not into vlib; the
// headless programs that keep their own frame (headless_game.cpp) define the frame time globals in
// headless_globals_loop.cpp instead.

#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/time/timestamp.h"

#include "keen/base/thread.hpp"

#include <intrin.h>

float Frametime = 1.0f / 30.0f; // 0x01519560
float Frametime_real = 1.0f / 30.0f; // 0x01519564
float Frametime_uncompressed = 1.0f / 30.0f; // 0x01519568
float Frames_per_second = 30.0f; // 0x0151956C
int Framecount; // 0x01CE8710
volatile unsigned int Framecount_increment_always; // 0x01CE8714
unsigned int Frametime_total_milliseconds; // 0x01CE8718
unsigned int Frametime_total_milliseconds_increment_always; // 0x01CE871C

static float Frametime_divisor = 1.0f; // 0x01519570
static float Frametime_min = 1.0f / 120.0f; // 0x01519574
static float Frametime_max = 0.25f; // 0x01519578
static float Frametime_old_min = 1.0f / 120.0f; // 0x0151957C, saved by frametime_disable_cap
static float Frametime_old_max = 0.25f; // 0x01519580
static float Frametimes[16]; // 0x01CE7FD8, ring of the last frame times
static int Sample_count; // 0x01CE8720
static int Time_last_frame; // 0x01CE8724, microseconds
static float Frametime_total; // 0x01CE8728, sum of Frametimes
static bool Frametime_inited; // 0x01CE872C
static int Frametime_paused; // 0x01CE8730
static int Animation_timer_paused; // 0x01CE8734

// 0x005AD1F0 (:78)
void frametime_init()
{
    frametime_reset();
    Frametime_inited = true;
}

// 0x0058AD50 (:88)
void frametime_reset()
{
    Frametime_real = 1.0f / 30.0f;
    Frametime = 1.0f / 30.0f;
    Frames_per_second = 30.0f;
    Frametime_divisor = 1.0f;
    Frametime_min = 1.0f / 120.0f;
    Frametime_max = 0.25f;
    Frametime_total = 0.0f;
    Framecount = 0;
    Frametime_total_milliseconds = 0;
    Time_last_frame = 0;
    for (int i = 0; i < 16; i++)
    {
        Frametimes[i] = 0.0f;
    }
    Sample_count = 0;
}

// 0x005AD200 (:124)
// Precision (from the asm): each statement is evaluated in double and rounded to float once, as written below.
void frametime_calculate(bool require_framerate_multiple, float framerate_multiple, float fixed_framerate)
{
    _InterlockedIncrement((volatile long*)&Framecount_increment_always); // LOCK XADD
    if (!Frametime_inited)
    {
        frametime_reset();
        Frametime_inited = true;
    }
    if (Frametime_paused != 0)
    {
        return;
    }
    int now;
    for (;;)
    {
        now = timer_get(TIMER_MICROSECONDS);
        int elapsed = now - Time_last_frame;
        if (elapsed < 0)
        {
            elapsed = (int)((double)Frametime_real * 1e+06);
        }
        if (fixed_framerate <= 0.0f)
        {
            Frametime_real = (float)((double)(float)elapsed / 1e+06);
        }
        else
        {
            Frametime_real = (float)(1.0 / (double)fixed_framerate);
        }
        Frametime = Frametime_real;
        if (!((double)Frametime_min > (double)Frametime_real))
        {
            break;
        }
        // The frame-rate cap: sleep the rest of the minimum frame time (truncated to milliseconds), then measure again.
        const float rest = (float)((double)Frametime_min - (double)Frametime_real);
        if (rest < 0.5f)
        {
            keen::sleepCurrentThreadMilliseconds((unsigned int)(long long)((double)rest * 1000.0));
        }
    }
    if ((double)Frametime_real > (double)Frametime_max)
    {
        Frametime = Frametime_max;
    }
    if (require_framerate_multiple)
    {
        const int multiples = (int)(float)((double)Frametime / (double)framerate_multiple);
        if ((float)((double)Frametime - (double)(float)multiples * (double)framerate_multiple) != 0.0f)
        {
            // Busy-wait up to the next multiple of framerate_multiple.
            const float next_ms = (float)((double)(float)(multiples + 1) * (double)framerate_multiple * 1000.0);
            const float wait_ms = (float)((double)next_ms - (double)Frametime * 1000.0);
            const int wait_us = (int)((double)wait_ms * 1000.0);
            int waited = 0;
            if (wait_us > 0)
            {
                do
                {
                    waited = timer_get(TIMER_MICROSECONDS) - now;
                } while (waited < wait_us);
            }
            Frametime = (float)((double)wait_ms / 1000.0 + (double)Frametime);
            Frametime_real = (float)((double)(float)waited / 1e+06 + (double)Frametime_real);
            now = timer_get(TIMER_MICROSECONDS);
        }
    }
    const float oldest = Frametimes[Sample_count];
    Frametimes[Sample_count] = Frametime;
    Framecount++;
    Sample_count = (Sample_count + 1) % 16;
    Frametime_total = (float)((double)(float)((double)Frametime_total - (double)oldest) + (double)Frametime);
    if (Frametime_total <= 0.0f)
    {
        Frames_per_second = 0.0f;
    }
    else
    {
        // Magic number: 16, the size of the Frametimes ring.
        const double samples = Framecount < 16 ? (double)(float)Framecount : 16.0;
        Frames_per_second = (float)(samples / (double)Frametime_total);
    }
    Frametime_uncompressed = Frametime;
    Frametime = (float)((double)Frametime / (double)Frametime_divisor);
    Time_last_frame = now;
    timestamp_increment((float)((double)Frametime * 1000.0));
    if (Animation_timer_paused == 0)
    {
        // Rounded half away from zero to whole milliseconds.
        const float ms = (float)((double)Frametime * 1000.0);
        Frametime_total_milliseconds += (int)(ms < 0.0f ? (double)ms - 0.5 : (double)ms + 0.5);
    }
    const float real_ms = (float)((double)Frametime_real * 1000.0);
    Frametime_total_milliseconds_increment_always += (int)(real_ms < 0.0f ? (double)real_ms - 0.5 : (double)real_ms + 0.5);
}

// 0x0058AE10 (:255)
void frametime_set_compression(float divisor)
{
    Frametime_divisor = divisor;
}

// 0x0058AE20 (:264)
float frametime_get_compression()
{
    return Frametime_divisor;
}

// 0x0058AE30 (:275)
void frametime_set_cap(float min, float max)
{
    if (min < max)
    {
        Frametime_min = min;
        Frametime_max = max;
        return;
    }
    Frametime_min = max;
    Frametime_max = min;
}

// 0x0058AE70 (:293)
void frametime_get_cap(float* min, float* max)
{
    if (min != nullptr)
    {
        *min = Frametime_min;
    }
    if (max != nullptr)
    {
        *max = Frametime_max;
    }
}

// 0x0058AEA0 (:305)
void frametime_disable_cap()
{
    Frametime_old_min = Frametime_min;
    Frametime_old_max = Frametime_max;
    // Magic numbers: effectively no cap (10 us to 10 s).
    Frametime_min = 1e-05f;
    Frametime_max = 10.0f;
}

// 0x0058AEF0 (:314)
void frametime_enable_cap()
{
    if (Frametime_old_min < Frametime_old_max)
    {
        Frametime_min = Frametime_old_min;
        Frametime_max = Frametime_old_max;
        return;
    }
    Frametime_min = Frametime_old_max;
    Frametime_max = Frametime_old_min;
}

// 0x0058AF30 (:322)
void frametime_pause()
{
    Frametime_paused++;
}

// 0x005AD670 (:329)
void frametime_unpause()
{
    Frametime_paused--;
    Time_last_frame = timer_get(TIMER_MICROSECONDS);
}

// 0x0058AF40 (:340)
void frametime_pause_animation_timer()
{
    Animation_timer_paused++;
}

// 0x0058AF50 (:347)
void frametime_unpause_animation_timer()
{
    Animation_timer_paused--;
}

// 0x005AD6C0 (:360)
// Notes (decompiled behaviour, kept as is): `high` gets the shortest frame time and `low` the longest.
void frametime_get_extremes(float& high, float& low)
{
    high = 100.0f;
    low = 0.0f;
    for (int i = 0; i < (Framecount > 15 ? 16 : Framecount); i++)
    {
        if (low < Frametimes[i])
        {
            low = Frametimes[i];
        }
        if (Frametimes[i] < high)
        {
            high = Frametimes[i];
        }
    }
}
