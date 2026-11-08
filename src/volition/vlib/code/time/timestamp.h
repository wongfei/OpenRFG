// Original: volition/vlib/code/time/timestamp.h
// Implementation: volition/vlib/code/time/timestamp.cpp
//
// Millisecond deadlines against two wrapping game clocks (decompiled):
//   Timestamp_ticker           game time; advanced by timestamp_increment unless timestamp_pause() is active.
//   Timestamp_realtime_ticker  advanced every frame, ignores pause.
// timestamp_increment(ms) is called from frametime_calculate; it accumulates the fractional part and adds whole
// milliseconds. Both tickers wrap at 1,800,000,000 ms; elapsed()/time_until()/time_since() compare modulo the wrap
// with a 900,000,000 ms half-window. A negative value means "not set" (invalidate()).
#pragma once

// size 0x4
class timestamp
{
public:
    timestamp(); // 0x004830E0 (.h:13)
    void invalidate(); // 0x004830F0 (.h:14)
    void set(int delta_milliseconds); // 0x0058E650 (:123), value = ticker + delta (wrapped)
    void set_rand(int low_milliseconds, int high_milliseconds); // 0x0058E6B0 (:156)
    void add_time(int milliseconds); // 0x0058E690 (:141)
    void deduct_time(int milliseconds); // 0x0058E6A0 (:146)
    bool valid() const; // 0x0058E7E0 (:237)
    bool elapsed() const; // 0x0058E720 (:169), false while invalid
    int time_until() const; // 0x0058E750 (:201)
    int time_since() const; // 0x0058E790 (:229)

private:
    int value; // 0x00
};

// Same as timestamp, against Timestamp_realtime_ticker.
// size 0x4
class timestamp_realtime
{
public:
    timestamp_realtime(); // 0x0058AAA0 (.h:45)
    void invalidate(); // 0x0083F500 (.h:46)
    void set(int delta_milliseconds); // 0x0058E7F0 (:255)
    void set_rand(int low_milliseconds, int high_milliseconds); // 0x0058E830 (:278)
    bool valid() const; // 0x0058E960 (:359)
    bool elapsed() const; // 0x0058E8A0 (:291)
    int time_until() const; // 0x0058E8D0 (:323)
    int time_since() const; // 0x0058E910 (:351)

private:
    int value; // 0x00
};

// A timestamp that also remembers its duration, for progress queries.
// size 0x8
class timestamp_percent : public timestamp
{
public:
    void set(int milliseconds, float starting_percent); // 0x0058E9B0 (:378)
    void set(int milliseconds); // 0x0058E970 (:372)
    float percent() const; // 0x0058EA10 (:387)
    int time_since_set() const; // 0x0058EA60 (:400)
    timestamp_percent(); // 0x006C3F30

private:
    int set_milliseconds; // 0x04
};

// Placement inferred.
// size 0x8
struct timestamp_pair
{
    timestamp_pair(); // 0x005EEC30

    timestamp last; // 0x00
    timestamp next; // 0x04
};

void timestamp_increment(float frametime_ms_float); // 0x0058E540 (:51)
void timestamp_pause(); // 0x0058E5E0 (:79), counted (Timestamp_paused)
void timestamp_unpause(); // 0x0058E5F0 (:88)
void timestamp_pause_reset(); // 0x0058E620 (:101)
int get_timestamp_pause(); // 0x0058E630 (:108)
int get_timestamp_ticker(); // 0x0058E640 (:113)
