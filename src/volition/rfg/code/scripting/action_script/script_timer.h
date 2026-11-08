// Original: volition/rfg/code/scripting/action_script/script_timer.h
// Implementation: volition/rfg/code/scripting/action_script/script_timer.cpp
//
// script_timer, a countdown or count-up timer that scripts create and query by handle.
#pragma once

#include "volition/vlib/code/time/timestamp.h"

struct cfile;
enum timer_priorities
{
    TP_INVALID = -1,
    TP_DEFAULT = 0,
    TP_HIGH = 1,
    NUM_TIMER_PRIORITIES = 2,
};

// size 0x10
class script_timer
{
public:
    script_timer(); // 0x00B2D160 (:109)
    unsigned int get_handle(); // 0x00B1F5F0 (.h:58)
    int get_time(); // 0x00B32AC0 (:115)
    void set(int time_in_milliseconds); // 0x00B22EC0 (:133)
    void pause(); // 0x00B38F40 (:146)
    void unpause(); // 0x00B38F70 (:162)
    void save(cfile* file); // 0x00B22EE0 (:184)
    void load(cfile* file); // 0x00B22F30 (:195)
    void init(); // 0x00B22F90 (:205)

private:
    bool enabled; // 0x00
    bool count_up; // 0x01
    bool paused; // 0x02
    int remaining; // 0x04
public:
    unsigned int handle; // 0x08
    timestamp ts; // 0x0C
};

bool script_timer_level_init(); // 0x00B22FB0 (:222)
unsigned int script_timer_create(int initial_time_in_milliseconds); // 0x00B32B10 (:237)
void script_timer_delete(unsigned int handle); // 0x00B38FB0 (:252)
void script_timers_do_frame(); // 0x00B38FF0 (:269)
void script_timer_set(unsigned int handle, int time_in_milliseconds); // 0x00B390B0 (:290)
void script_timer_add(unsigned int handle, int time_in_milliseconds); // 0x00B39110 (:307)
int script_timer_get_time(unsigned int handle); // 0x00B32B60 (:317)
unsigned int script_timer_get_display_handle(); // 0x00B22FE0 (:327)
void script_timer_display(unsigned int handle, timer_priorities priority); // 0x00B22FF0 (:333)
void script_timer_hide(unsigned int handle); // 0x00B23060 (:383)
void script_timer_pause(unsigned int handle); // 0x00B39150 (:412)
void script_timer_unpause(unsigned int handle); // 0x00B391A0 (:423)
void script_timers_save(cfile* file); // 0x00B32B90 (:434)
void script_timers_load(cfile* file); // 0x00B39200 (:483)
void script_timer_clear(); // 0x00B3D670 (:556)
