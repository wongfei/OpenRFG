// Original: volition/vlib/code/os/timer.cpp
//
// Partial: everything but the debug tick helpers (timer_get_debug_ticks, timer_convert_debug_ticks,
// timer_convert_ticks), which read the keen profiler. The clock is a keen::SystemTimer (milliseconds) started by
// vlib_timer_init; seconds and microseconds are derived from the milliseconds.

#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include "keen/base/systemtimer.hpp"

static bool Timer_inited; // 0x01CE8798
static keen::SystemTimer Global_time; // 0x01CE9B98, started by vlib_timer_init

// 0x0058C350 (:20)
void vlib_timer_init()
{
    if (!Timer_inited)
    {
        Timer_inited = true;
        Global_time.start();
    }
}

// 0x0058C370 (:33)
void vlib_timer_close()
{
}

// 0x0058C380 (:38)
void vlib_timer_update(float deltaTime)
{
}

// 0x0058C390 (:54)
int timer_get(timer_units frequency)
{
    if (!Timer_inited)
    {
        VLIB_CRITICAL_FAILURE();
        return 0;
    }
    if (frequency == TIMER_SECONDS)
    {
        return (int)(Global_time.getElapsedTime() / 1000);
    }
    if (frequency == TIMER_MILLISECONDS)
    {
        return (int)Global_time.getElapsedTime();
    }
    if (frequency != TIMER_MICROSECONDS)
    {
        keen::debug::nativePrintArgs("Unknown frequency passed to timer_get.");
        return 0;
    }
    return (int)(Global_time.getElapsedTime() * 1000);
}

// 0x0058C480 (:118)
int timer_get_since(int start_time, timer_units frequency)
{
    return timer_get(frequency) - start_time;
}

// 0x0058C4A0 (:129)
void timer_wait(int microseconds_to_wait)
{
    // Busy wait.
    const int start = timer_get(TIMER_MICROSECONDS);
    while (timer_get(TIMER_MICROSECONDS) - start < microseconds_to_wait)
    {
    }
}
