// Original: volition/vlib/code/time/timestamp.cpp
//
// Partial: the timestamp_pair constructor (no body recorded).
//
// vlib_unity.obj is a unity build, so file statics are placed by the functions that use them. Initial values are read
// from the binary's .data section.

#include "volition/vlib/code/time/timestamp.h"

#include "volition/vlib/code/math/rand.h"

#include <intrin.h>

static int Timestamp_ticker = 1; // 0x01519708, game milliseconds, wraps at 1,800,000,000
static int Timestamp_realtime_ticker = 1; // 0x0151970C
static volatile int Timestamp_paused; // 0x01CE87D4, pause count

static const int Timestamp_wrap = 1800000000;
static const int Timestamp_half_window = 900000000;

// Deadline `delta` milliseconds from `ticker`, wrapped into [0, wrap].
static int timestamp_make(int ticker, int delta)
{
    int value = delta + ticker;
    if (delta < 0)
    {
        if (value < 0)
        {
            value += Timestamp_wrap;
        }
    }
    else if (Timestamp_wrap < value)
    {
        value -= Timestamp_wrap;
    }
    return value;
}

static bool timestamp_elapsed(int value, int ticker)
{
    if (value < 0)
    {
        return false;
    }
    if (ticker < value)
    {
        return Timestamp_half_window - 1 < value - ticker;
    }
    return ticker - value < Timestamp_half_window;
}

static int timestamp_time_until(int value, int ticker)
{
    if (value < 0)
    {
        return Timestamp_wrap;
    }
    if (ticker < value)
    {
        const int until = value - ticker;
        return Timestamp_half_window < until ? until - Timestamp_wrap : until;
    }
    if (Timestamp_half_window < ticker - value)
    {
        return value - ticker + Timestamp_wrap;
    }
    return -(ticker - value);
}

static int timestamp_time_since(int value, int ticker)
{
    if (value < 0)
    {
        return -Timestamp_wrap;
    }
    return -timestamp_time_until(value, ticker);
}

// 0x0058E540 (:51)
void timestamp_increment(float frametime_ms_float)
{
    static float amount_rounded; // 0x01D579A8
    const float amount = amount_rounded + frametime_ms_float;
    const int whole = (int)(amount < 0.0f ? amount - 0.5f : amount + 0.5f);
    amount_rounded = amount - (float)whole;
    if (Timestamp_paused == 0)
    {
        Timestamp_ticker += whole;
        if (Timestamp_wrap < Timestamp_ticker)
        {
            Timestamp_ticker -= Timestamp_wrap;
        }
    }
    Timestamp_realtime_ticker += whole;
    if (Timestamp_wrap < Timestamp_realtime_ticker)
    {
        Timestamp_realtime_ticker -= Timestamp_wrap;
    }
}

// 0x0058E5E0 (:79)
void timestamp_pause()
{
    _InterlockedIncrement((volatile long*)&Timestamp_paused);
}

// 0x0058E5F0 (:88)
void timestamp_unpause()
{
    _InterlockedDecrement((volatile long*)&Timestamp_paused);
    if (Timestamp_paused < 0)
    {
        Timestamp_paused = 0;
    }
}

// 0x0058E620 (:101)
void timestamp_pause_reset()
{
    Timestamp_paused = 0;
}

// 0x0058E630 (:108)
int get_timestamp_pause()
{
    return Timestamp_paused;
}

// 0x0058E640 (:113)
int get_timestamp_ticker()
{
    return Timestamp_ticker;
}

// ---- timestamp

// 0x004830E0 (.h:13)
timestamp::timestamp()
{
    value = -1;
}

// 0x004830F0 (.h:14)
void timestamp::invalidate()
{
    value = -1;
}

// 0x0058E650 (:123)
void timestamp::set(int delta_milliseconds)
{
    value = timestamp_make(Timestamp_ticker, delta_milliseconds);
}

// 0x0058E6B0 (:156)
// A random deadline in [low, high] ms from now.
void timestamp::set_rand(int low_milliseconds, int high_milliseconds)
{
    value = timestamp_make(Timestamp_ticker, Vlib_rand_instance.int_rand_range(low_milliseconds, high_milliseconds)); // inlined
}

// 0x0058E690 (:141)
void timestamp::add_time(int milliseconds)
{
    value += milliseconds;
}

// 0x0058E6A0 (:146)
void timestamp::deduct_time(int milliseconds)
{
    value -= milliseconds;
}

// 0x0058E7E0 (:237)
bool timestamp::valid() const
{
    return value >= 0;
}

// 0x0058E720 (:169)
bool timestamp::elapsed() const
{
    return timestamp_elapsed(value, Timestamp_ticker);
}

// 0x0058E750 (:201)
int timestamp::time_until() const
{
    return timestamp_time_until(value, Timestamp_ticker);
}

// 0x0058E790 (:229)
int timestamp::time_since() const
{
    return timestamp_time_since(value, Timestamp_ticker);
}

// ---- timestamp_realtime

// 0x0058AAA0 (.h:45)
timestamp_realtime::timestamp_realtime()
{
    value = -1;
}

// 0x0083F500 (.h:46)
void timestamp_realtime::invalidate()
{
    value = -1;
}

// 0x0058E7F0 (:255)
void timestamp_realtime::set(int delta_milliseconds)
{
    value = timestamp_make(Timestamp_realtime_ticker, delta_milliseconds);
}

// 0x0058E830 (:278)
void timestamp_realtime::set_rand(int low_milliseconds, int high_milliseconds)
{
    value = timestamp_make(Timestamp_realtime_ticker, Vlib_rand_instance.int_rand_range(low_milliseconds, high_milliseconds)); // inlined
}

// 0x0058E960 (:359)
bool timestamp_realtime::valid() const
{
    return value >= 0;
}

// 0x0058E8A0 (:291)
bool timestamp_realtime::elapsed() const
{
    return timestamp_elapsed(value, Timestamp_realtime_ticker);
}

// 0x0058E8D0 (:323)
int timestamp_realtime::time_until() const
{
    return timestamp_time_until(value, Timestamp_realtime_ticker);
}

// 0x0058E910 (:351)
int timestamp_realtime::time_since() const
{
    return timestamp_time_since(value, Timestamp_realtime_ticker);
}

// ---- timestamp_percent

// 0x006C3F30 (compiler-generated; the base constructor invalidates the timestamp)
timestamp_percent::timestamp_percent()
{
}

// 0x0058E970 (:372)
void timestamp_percent::set(int milliseconds)
{
    set_milliseconds = milliseconds;
    timestamp::set(milliseconds);
}

// 0x0058E9B0 (:378)
void timestamp_percent::set(int milliseconds, float starting_percent)
{
    set_milliseconds = milliseconds;
    timestamp::set((int)((float)milliseconds * starting_percent));
}

// 0x0058EA10 (:387)
float timestamp_percent::percent() const
{
    // 1 while unset, 0 once elapsed, otherwise the remaining fraction.
    if (!valid())
    {
        return 1.0f;
    }
    if (elapsed())
    {
        return 0.0f;
    }
    return (float)time_until() / (float)set_milliseconds;
}

// 0x0058EA60 (:400)
int timestamp_percent::time_since_set() const
{
    if (!valid())
    {
        return Timestamp_wrap;
    }
    if (elapsed())
    {
        return time_since() + set_milliseconds;
    }
    return set_milliseconds - time_until();
}

// 0x005EEC30
timestamp_pair::timestamp_pair()
{
}
