// Original: volition/rfg/code/scripting/action_script/script_timer.cpp
//
// Countdown and count-up timers that scripts create and query by handle (the pool index). Categories: 0 running,
// 1 elapsed, 2 paused.

#include "volition/rfg/code/scripting/action_script/script_timer.h"

#include "volition/rfg/code/gameplay/gameplay.h"
#include "volition/rfg/code/interface/ui_document_hud_missions.h"
#include "volition/rfg/code/scripting/action_script/action_script_internal.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/vlib/code/cfile/cfile.h"

#include <math.h>
#include <stdio.h>

static unsigned int Script_timer_displaying_handle = 0xFFFFFFFF; // 0x016B0CB4
static unsigned int Script_timer_next_displaying_handle = 0xFFFFFFFF; // 0x016B0CB8
static timer_priorities Script_timer_displaying_priority = TP_INVALID; // 0x016B0CBC
static timer_priorities Script_timer_next_displaying_priority = TP_INVALID; // 0x016B0CC0
static bool Script_timer_displayed; // 0x0361141C
static int Last_displayed_second; // 0x03611420
object_pool<script_timer,64,3> Script_timers(&script_timer::init); // 0x036F6500

// Magic numbers: the pool's categories.
static const int TIMER_RUNNING = 0;
static const int TIMER_ELAPSED = 1;
static const int TIMER_PAUSED = 2;

static script_timer* script_timer_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF || handle >= Script_timers.get_capacity())
    {
        return nullptr;
    }
    return Script_timers.get_entry((int)handle);
}

// 0x00B38DF0 (:36)
static void script_timer_hud_message_display()
{
    script_timer* timer = script_timer_from_handle(Script_timer_displaying_handle);
    if (timer == nullptr)
    {
        return;
    }
    // Magic number: milliseconds to whole seconds, rounded up.
    const int total_seconds = (int)(float)ceil((double)((float)timer->get_time() * 0.001f));
    const int minutes = total_seconds / 60;
    const int seconds = total_seconds % 60;
    wchar_t text[128];
    _snwprintf_s(text, 128, _TRUNCATE, L"%d:%02d", minutes, seconds);
    if (!Script_timer_displayed)
    {
        Script_timer_displayed = hud_ui_missions_set_timer(true, text);
        if (Script_timer_displayed)
        {
            gameplay_play_sfx(GSFX_TIMER_START);
        }
        Last_displayed_second = seconds;
        return;
    }
    if (Last_displayed_second != seconds)
    {
        if (minutes == 0 && seconds < 6)
        {
            gameplay_play_sfx(GSFX_LOW_TIME_TICK);
        }
        else if (minutes == 0 && seconds == 10)
        {
            gameplay_play_sfx(GSFX_TEN_SECONDS_REMAINING);
        }
        else if (seconds == 0 && minutes == 1)
        {
            gameplay_play_sfx(GSFX_ONE_MINUTE_REMAINING);
        }
    }
    Script_timer_displayed = hud_ui_missions_set_timer(true, text);
    Last_displayed_second = seconds;
}

// 0x00B2D160 (:109)
script_timer::script_timer()
    : enabled(false)
    , count_up(true)
    , paused(false)
    , remaining(0)
    , handle(0xFFFFFFFF)
{
    ts.invalidate();
}

// 0x00B32AC0 (:115)
int script_timer::get_time()
{
    const int left = Script_timers.get_category(this) == TIMER_PAUSED ? remaining : ts.time_until();
    if (count_up)
    {
        return -left;
    }
    return left < 1 ? 0 : left;
}

// 0x00B22EC0 (:133)
void script_timer::set(int time_in_milliseconds)
{
    count_up = time_in_milliseconds == 0;
    remaining = time_in_milliseconds;
    ts.set(time_in_milliseconds);
}

// 0x00B38F40 (:146)
void script_timer::pause()
{
    if (!paused)
    {
        remaining = ts.time_until();
        paused = true;
        Script_timers.move(this, TIMER_PAUSED);
    }
}

// 0x00B38F70 (:162)
void script_timer::unpause()
{
    if (paused)
    {
        ts.set(remaining);
        paused = false;
        Script_timers.move(this, remaining > 0 ? TIMER_RUNNING : TIMER_ELAPSED);
    }
}

// 0x00B22EE0 (:184)
void script_timer::save(cfile* file)
{
    cf_write_uint8(enabled, file);
    cf_write_uint8(count_up, file);
    cf_write_uint8(paused, file);
    cf_write_int32(remaining, file);
    cf_write_int32(ts.time_until(), file);
}

// 0x00B22F30 (:195)
void script_timer::load(cfile* file)
{
    enabled = cf_read_uint8(file) != 0;
    count_up = cf_read_uint8(file) != 0;
    paused = cf_read_uint8(file) != 0;
    remaining = cf_read_int32(file);
    ts.set(cf_read_int32(file));
}

// 0x00B22F90 (:205)
void script_timer::init()
{
    paused = false;
    remaining = 0;
    enabled = false;
    count_up = true;
    handle = 0xFFFFFFFF;
    ts.invalidate();
}

// 0x00B1F5F0 (.h:58)
unsigned int script_timer::get_handle()
{
    return handle;
}

void script_timer_display_reset()
{
    Script_timer_displayed = false;
    Last_displayed_second = 0;
    Script_timer_displaying_handle = 0xFFFFFFFF;
    Script_timer_next_displaying_handle = 0xFFFFFFFF;
    Script_timer_displaying_priority = TP_INVALID;
    Script_timer_next_displaying_priority = TP_INVALID;
}

// 0x00B22FB0 (:222)
bool script_timer_level_init()
{
    script_timer_display_reset();
    return true;
}

// 0x00B32B10 (:237)
unsigned int script_timer_create(int initial_time_in_milliseconds)
{
    if (Script_timers.get_free() < 1)
    {
        return 0xFFFFFFFF;
    }
    script_timer* timer = Script_timers.alloc(TIMER_RUNNING);
    if (timer == nullptr)
    {
        return 0xFFFFFFFF;
    }
    timer->handle = (unsigned int)Script_timers.get_index(timer);
    timer->set(initial_time_in_milliseconds);
    return timer->handle;
}

// 0x00B38FB0 (:252)
void script_timer_delete(unsigned int handle)
{
    if (handle != 0xFFFFFFFF)
    {
        if (handle == Script_timer_displaying_handle)
        {
            script_timer_hide(handle);
        }
        script_timer* timer = script_timer_from_handle(handle);
        if (timer != nullptr)
        {
            Script_timers.free(timer);
        }
    }
}

// 0x00B38FF0 (:269)
void script_timers_do_frame()
{
    script_timer* next;
    for (script_timer* timer = Script_timers.get_first(TIMER_RUNNING); timer != nullptr; timer = next)
    {
        next = Script_timers.get_next(timer);
        if (timer->ts.elapsed())
        {
            timer_elapsed_event data;
            data.handle = timer->handle;
            gem_post_event(data);
            Script_timers.move(timer, TIMER_ELAPSED);
        }
    }
    script_timer_hud_message_display();
}

// 0x00B390B0 (:290)
void script_timer_set(unsigned int handle, int time_in_milliseconds)
{
    script_timer* timer = script_timer_from_handle(handle);
    if (timer != nullptr)
    {
        if (Script_timers.get_category(timer) == TIMER_ELAPSED)
        {
            Script_timers.move(timer, TIMER_RUNNING);
        }
        timer->set(time_in_milliseconds);
    }
}

// 0x00B39110 (:307)
void script_timer_add(unsigned int handle, int time_in_milliseconds)
{
    script_timer* timer = script_timer_from_handle(handle);
    if (timer != nullptr)
    {
        script_timer_set(handle, timer->get_time() + time_in_milliseconds);
    }
}

// 0x00B32B60 (:317)
int script_timer_get_time(unsigned int handle)
{
    script_timer* timer = script_timer_from_handle(handle);
    return timer != nullptr ? timer->get_time() : 0;
}

// 0x00B22FE0 (:327)
unsigned int script_timer_get_display_handle()
{
    return Script_timer_displaying_handle;
}

// 0x00B22FF0 (:333)
void script_timer_display(unsigned int handle, timer_priorities priority)
{
    if (handle == 0xFFFFFFFF || handle == Script_timer_displaying_handle)
    {
        return;
    }
    if (Script_timer_displaying_handle != 0xFFFFFFFF)
    {
        if ((int)Script_timer_displaying_priority < (int)priority)
        {
            Script_timer_next_displaying_handle = Script_timer_displaying_handle;
            Script_timer_next_displaying_priority = Script_timer_displaying_priority;
            Script_timer_displaying_priority = priority;
            Script_timer_displaying_handle = handle;
            return;
        }
        Script_timer_next_displaying_priority = priority;
        Script_timer_next_displaying_handle = handle;
        return;
    }
    if (handle == Script_timer_next_displaying_handle)
    {
        Script_timer_next_displaying_handle = 0xFFFFFFFF;
        Script_timer_next_displaying_priority = TP_INVALID;
    }
    Script_timer_displaying_priority = priority;
    Script_timer_displaying_handle = handle;
}

// 0x00B23060 (:383)
void script_timer_hide(unsigned int handle)
{
    if (handle == Script_timer_next_displaying_handle)
    {
        Script_timer_next_displaying_handle = 0xFFFFFFFF;
        Script_timer_next_displaying_priority = TP_INVALID;
        return;
    }
    if (handle == Script_timer_displaying_handle)
    {
        Script_timer_displaying_handle = 0xFFFFFFFF;
        Script_timer_displaying_priority = TP_INVALID;
        hud_ui_missions_set_timer(false, nullptr);
        Script_timer_displayed = false;
        if (Script_timer_next_displaying_handle != 0xFFFFFFFF)
        {
            script_timer_display(Script_timer_next_displaying_handle, Script_timer_next_displaying_priority);
        }
    }
}

// 0x00B39150 (:412)
void script_timer_pause(unsigned int handle)
{
    script_timer* timer = script_timer_from_handle(handle);
    if (timer != nullptr)
    {
        timer->pause();
    }
}

// 0x00B391A0 (:423)
void script_timer_unpause(unsigned int handle)
{
    script_timer* timer = script_timer_from_handle(handle);
    if (timer != nullptr)
    {
        timer->unpause();
    }
}

static void script_timers_save_category(cfile* file, int category)
{
    cf_write_uint32((unsigned int)Script_timers.get_count(category), file);
    for (script_timer* timer = Script_timers.get_first(category); timer != nullptr; timer = Script_timers.get_next(timer))
    {
        cf_write_int32((int)timer->handle, file);
        timer->save(file);
    }
}

// 0x00B32B90 (:434)
void script_timers_save(cfile* file)
{
    cf_write_int32((int)Script_timer_displaying_handle, file);
    cf_write_int32(Script_timer_displaying_priority, file);
    cf_write_int32((int)Script_timer_next_displaying_handle, file);
    cf_write_int32(Script_timer_next_displaying_priority, file);
    script_timers_save_category(file, TIMER_RUNNING);
    script_timers_save_category(file, TIMER_ELAPSED);
    script_timers_save_category(file, TIMER_PAUSED);
}

static void script_timers_load_category(cfile* file, int category)
{
    for (unsigned int count = cf_read_uint32(file); count != 0; count--)
    {
        const unsigned int handle = (unsigned int)cf_read_int32(file);
        script_timer* timer = handle < Script_timers.get_capacity() ? Script_timers.get_entry((int)handle) : nullptr;
        timer->handle = handle;
        if (category != TIMER_PAUSED)
        {
            Script_timers.move(timer, category);
        }
        timer->load(file);
    }
}

// 0x00B39200 (:483)
void script_timers_load(cfile* file)
{
    // Magic number: the pool's size. Every entry is taken, in the paused category, so a saved handle finds its slot; the
    // ones without a saved timer are freed at the end.
    for (int i = 0; i < 64; i++)
    {
        Script_timers.alloc(TIMER_PAUSED);
    }
    Script_timer_displaying_handle = (unsigned int)cf_read_int32(file);
    Script_timer_displaying_priority = (timer_priorities)cf_read_int32(file);
    Script_timer_next_displaying_handle = (unsigned int)cf_read_int32(file);
    Script_timer_next_displaying_priority = (timer_priorities)cf_read_int32(file);
    script_timers_load_category(file, TIMER_RUNNING);
    script_timers_load_category(file, TIMER_ELAPSED);
    script_timers_load_category(file, TIMER_PAUSED);
    script_timer* next;
    for (script_timer* timer = Script_timers.get_first(TIMER_PAUSED); timer != nullptr; timer = next)
    {
        next = Script_timers.get_next(timer);
        if (timer->handle == 0xFFFFFFFF)
        {
            Script_timers.free(timer);
        }
    }
}

// 0x00B3D670 (:556)
void script_timer_clear()
{
    script_timer_hide(Script_timer_next_displaying_handle);
    script_timer_hide(Script_timer_displaying_handle);
    Script_timers.clear();
}
