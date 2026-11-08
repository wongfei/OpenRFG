// Original: volition/vlib/code/os/frametime.h
// Implementation: volition/vlib/code/os/frametime.cpp
//
// Variable frame time, in seconds. frametime_calculate runs once per sim frame from rfg_do_frame (decompiled):
//   1. Frametime_real = time since the previous frame (Global_time, 1/fixed_framerate when fixed_framerate > 0).
//      While it is below Frametime_min the thread sleeps and measures again (the frame-rate cap).
//   2. Frametime = min(Frametime_real, Frametime_max); optionally rounded up to a multiple of framerate_multiple
//      by busy-waiting.
//   3. 16-sample average -> Frames_per_second; Framecount++.
//   4. Frametime_uncompressed = Frametime; Frametime /= Frametime_divisor (the "compression", slow motion).
//   5. timestamp_increment(Frametime * 1000); Frametime_total_milliseconds advances unless the animation timer is
//      paused; Frametime_total_milliseconds_increment_always advances by Frametime_real.
// frametime_pause() freezes all of it. Defaults (.data): Frametime 1/30, cap [1/120, 0.25], divisor 1.
#pragma once

extern float Frametime; // 0x01519560
extern float Frametime_real; // 0x01519564
extern float Frametime_uncompressed; // 0x01519568
extern float Frames_per_second; // 0x0151956C
extern int Framecount; // 0x01CE8710
extern volatile unsigned int Framecount_increment_always; // 0x01CE8714
extern unsigned int Frametime_total_milliseconds; // 0x01CE8718
extern unsigned int Frametime_total_milliseconds_increment_always; // 0x01CE871C

void frametime_init(); // 0x005AD1F0 (:78)
void frametime_reset(); // 0x0058AD50 (:88)
void frametime_calculate(bool require_framerate_multiple, float framerate_multiple, float fixed_framerate); // 0x005AD200 (:124)
void frametime_set_compression(float divisor); // 0x0058AE10 (:255)
float frametime_get_compression(); // 0x0058AE20 (:264)
void frametime_set_cap(float min, float max); // 0x0058AE30 (:275), swaps the arguments if min >= max
void frametime_get_cap(float* min, float* max); // 0x0058AE70 (:293)
void frametime_disable_cap(); // 0x0058AEA0 (:305)
void frametime_enable_cap(); // 0x0058AEF0 (:314)
void frametime_pause(); // 0x0058AF30 (:322)
void frametime_unpause(); // 0x005AD670 (:329)
void frametime_pause_animation_timer(); // 0x0058AF40 (:340)
void frametime_unpause_animation_timer(); // 0x0058AF50 (:347)
void frametime_get_extremes(float& high, float& low); // 0x005AD6C0 (:360)
