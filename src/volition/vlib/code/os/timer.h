// Original: volition/vlib/code/os/timer.h
// Implementation: volition/vlib/code/os/timer.cpp
//
// Process clock. timer.cpp keeps a file-static keen::SystemTimer (Global_time, 0x01CE9B98) started by
// vlib_timer_init; timer_get converts its elapsed milliseconds to the requested unit (decompiled), so microsecond
// values only have millisecond resolution. Calls before vlib_timer_init return 0.
#pragma once

#include <stdint.h>

enum timer_units
{
    TIMER_SECONDS = 1,
    TIMER_MILLISECONDS = 1000,
    TIMER_MICROSECONDS = 1000000,
};

void vlib_timer_init(); // 0x0058C350 (:20)
void vlib_timer_close(); // 0x0058C370 (:33)
void vlib_timer_update(float deltaTime); // 0x0058C380 (:38)
int timer_get(timer_units frequency); // 0x0058C390 (:54)
uint64_t timer_get_debug_ticks(); // 0x0058C420 (:88)
float timer_convert_debug_ticks(int ticks); // 0x0058C450 (:101)
int timer_get_since(int start_time, timer_units frequency); // 0x0058C480 (:118)
void timer_wait(int microseconds_to_wait); // 0x0058C4A0 (:129)
int timer_convert_ticks(int ticks, timer_units frequency); // 0x0058C520 (:137)
