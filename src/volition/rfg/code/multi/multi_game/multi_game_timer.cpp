// Original: volition/rfg/code/multi/multi_game/multi_game_timer.cpp
//
// The match timer: multi_timer counts down in milliseconds of the real-time timer (update_timer deducts the time since
// the last check from the running timers), and the HUD colour of the time left blinks below 15 s.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_timer.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/gr/gr_font.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/util/color.h"

// ---- file-static globals
static float Timer_y_pos = 0.1f; // 0x0167F4C8
// color Timer_color_normal (0x01693A50) is used here too; placed in multi_game_mode_common.cpp.
// color Timer_color_caution (0x01693A54) is used here too; placed in multi_game_mode_common.cpp.
static color Timer_color_warning(255, 0, 0, 255); // 0x01693A58, initial data ff0000ff
// multi_timer Timer_color_swap_time (0x01693A5C) is used here too; placed in multi_game_mode_common.cpp.
// multi_timer Timer_time (0x01693A64) is used here too; placed in multi_core_platform.cpp.
// int Time_last_check (0x02FEB758) is used here too; placed in multi_game_mode_common.cpp.
// color Timer_color (0x03017740) is used here too; placed in multi_game_mode_common.cpp.

// Magic numbers: the timers wrap at 1,800,000,000 ms (the network clock's range); the colour swaps every 500 ms; the
// warning colour starts at 5 s left, the caution colour at 15 s.
static const int Timer_wrap_ms = 1800000000;
static const int Timer_swap_ms = 500;
static const int Timer_warning_ms = 5000;
static const int Timer_caution_ms = 15000;

// 0x00966E30 (:107)
multi_timer::multi_timer()
{
    m_expiry_time_ms = -1;
    m_is_valid = false;
    m_is_paused = false;
}

// 0x00986DA0 (:110)
void multi_timer::set(int time_ms)
{
    if (time_ms < Timer_wrap_ms)
    {
        m_expiry_time_ms = time_ms;
        m_is_valid = true;
        m_is_paused = false;
    }
}

// 0x00966E40 (:120)
int multi_timer::time_until() const
{
    return m_expiry_time_ms;
}

// 0x00966E50 (:125)
int multi_timer::time_since() const
{
    return -m_expiry_time_ms;
}

// 0x00966E60 (:130)
bool multi_timer::valid() const
{
    return m_is_valid;
}

// 0x00966E70 (:135)
bool multi_timer::elapsed() const
{
    return m_expiry_time_ms <= 0;
}

// 0x00966E80 (:140)
void multi_timer::invalidate()
{
    m_is_valid = false;
}

// 0x00966E90 (:145)
void multi_timer::increment(int time_ms)
{
    if (m_is_valid)
    {
        m_expiry_time_ms -= time_ms;
    }
}

// 0x00966EA0 (:152)
void multi_timer::set_pause(bool pause)
{
    m_is_paused = pause;
}

// 0x00966EB0 (:157)
bool multi_timer::is_paused() const
{
    return m_is_paused;
}

// 0x00966EC0 (:162): deducts the real time since the last check from the colour swap timer and the match timer
static void update_timer()
{
    if (Timer_time.is_paused())
    {
        return;
    }
    const int now = timer_get(TIMER_MILLISECONDS) % Timer_wrap_ms;
    int delta = now - Time_last_check;
    if (delta < 0)
    {
        delta += Timer_wrap_ms;
    }
    Timer_color_swap_time.increment(delta);
    Time_last_check = now;
    Timer_time.increment(delta);
}

// 0x00986DC0 (:188)
void multi_game_timer_process()
{
    update_timer();
    if (Timer_color_swap_time.elapsed())
    {
        Timer_color_swap_time.set(Timer_color_swap_time.time_until() + Timer_swap_ms);
        if (Timer_time.time_until() < Timer_warning_ms)
        {
            // warning and caution alternate
            if (Timer_color == Timer_color_warning)
            {
                Timer_color = Timer_color_caution;
            }
            else
            {
                Timer_color = Timer_color_warning;
            }
        }
        else if (Timer_time.time_until() < Timer_caution_ms)
        {
            // caution and normal alternate
            if (Timer_color == Timer_color_caution)
            {
                Timer_color = Timer_color_normal;
            }
            else
            {
                Timer_color = Timer_color_caution;
            }
        }
    }
    if (Timer_time.elapsed())
    {
        Timer_time.invalidate();
    }
}

// 0x00986EE0 (:216): the time left as m:ss at the top of the screen
void multi_game_timer_render()
{
    if (Timer_time.time_until() > 0 && Timer_time.valid())
    {
        char text[36];
        const int seconds = Timer_time.time_until() / 1000;
        _snprintf_safe(text, sizeof(text), "%2d:%02d", seconds / 60, seconds % 60);
        gr_set_color(Timer_color);
        const rl_screen_info* screen = rl_render_lib::get_instance()->get_screen_info();
        // Magic number: 0x8000 centres the string horizontally.
        gr_string(0x8000, (int)((float)screen->get_screen_height() * Timer_y_pos), text, Header_font, GM_FILTER);
    }
}

// 0x00966F10 (:245)
bool multi_game_timer_elapsed()
{
    return Timer_time.time_until() <= 0 || !Timer_time.valid();
}

// 0x00966F30 (:250)
int multi_game_timer_time_until()
{
    return Timer_time.time_until();
}

// 0x00966F40 (:255)
void multi_game_timer_invalidate()
{
    Timer_time.invalidate();
    Timer_color_swap_time.invalidate();
}

// 0x00986FB0 (:261)
void multi_game_timer_set(int time_ms)
{
    multi_game_timer_invalidate();
    update_timer();
    Timer_time.set(time_ms);
    if (time_ms <= Timer_caution_ms)
    {
        Timer_color_swap_time.set(time_ms % Timer_swap_ms);
        Timer_color = Timer_color_caution;
    }
    else
    {
        Timer_color_swap_time.set(time_ms - Timer_caution_ms);
        Timer_color = Timer_color_normal;
    }
}

// 0x00966F50 (:278)
void multi_game_timer_init()
{
    Timer_color = Timer_color_normal;
    multi_game_timer_invalidate();
}

// 0x00966F70 (:286)
void multi_game_timer_pause()
{
    Timer_time.set_pause(true);
}

// 0x009870A0 (:291): the time paused does not count
void multi_game_timer_unpause()
{
    Timer_time.set_pause(false);
    if (Timer_time.valid())
    {
        Timer_time.set(Timer_time.time_until());
        Time_last_check = timer_get(TIMER_MILLISECONDS) % Timer_wrap_ms;
    }
}

// 0x00966F80 (:304)
bool multi_game_timer_is_valid()
{
    return Timer_time.valid();
}
