// Original: volition/rfg/code/gamewide/game_hudtimer.cpp
//
// The HUD timer (stub_impl.md S5 and S7): up or down in milliseconds, drawn as text at the top of the screen.

#include "volition/rfg/code/gamewide/game_hudtimer.h"

#include "volition/rfg/code/rfg.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/gr/gr_font.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/util/color.h"

#include <stdio.h>

// ---- file-private types (rfg.pdb)
enum hudtimer_states
{
    HTS_COUNTING_UP = 0,
    HTS_COUNTING_DOWN = 1,
    HTS_COUNTUP_PAUSED = 2,
    HTS_COUNTDOWN_PAUSED = 3,
    HTS_COUNTUP_FINISHED = 4,
    HTS_COUNTDOWN_FINISHED = 5,
    HTS_NUM_HUDTIMER_STATES = 6,
};

// ---- file-static globals
static hudtimer_states Hudtimer_state = HTS_COUNTDOWN_FINISHED; // 0x0165CBEC
static const color Int_color_white_text_normal(0xC8, 0xC8, 0xC8, 0xFF); // 0x01665164
static float Hudtimer_time_rounded; // 0x0227AFC8
static int Hudtimer_time; // 0x02505504

// 0x007CD8B0
void unused_dcf_hudtimer_stop()
{
    game_hudtimer_stop();
}

// 0x007CD8F0
void unused_dcf_hudtimer_pause()
{
    game_hudtimer_pause(true);
}

// 0x007CD930
void unused_dcf_hudtimer_unpause()
{
    game_hudtimer_pause(false);
}

// 0x007C19C0
void game_hudtimer_countup()
{
    Hudtimer_state = HTS_COUNTING_UP;
    Hudtimer_time = 0;
    Hudtimer_time_rounded = 0.0f;
}

// 0x007C19E0
void game_hudtimer_countdown(int duration)
{
    Hudtimer_state = HTS_COUNTING_DOWN;
    Hudtimer_time = duration;
    Hudtimer_time_rounded = 0.0f;
}

// 0x007C1A00
void game_hudtimer_countdown_add(int duration)
{
    if (Hudtimer_state == HTS_COUNTING_DOWN || Hudtimer_state == HTS_COUNTDOWN_PAUSED)
    {
        Hudtimer_time = Hudtimer_time + duration;
    }
}

// 0x007C1A20 (:160)
// The timer counts in whole milliseconds; the fraction carries to the next frame. A countdown stops at 0.
void game_hudtimer_update(float frametime)
{
    if (Hudtimer_state == HTS_COUNTING_UP)
    {
        const float ms = (float)((double)frametime * 1000.0 + (double)Hudtimer_time_rounded);
        const int whole = fl_round(ms);
        Hudtimer_time_rounded = ms - (float)whole;
        Hudtimer_time = Hudtimer_time + whole;
    }
    else if (Hudtimer_state == HTS_COUNTING_DOWN)
    {
        const float ms = (float)((double)frametime * 1000.0 + (double)Hudtimer_time_rounded);
        const int whole = fl_round(ms);
        Hudtimer_time = Hudtimer_time - whole;
        Hudtimer_time_rounded = ms - (float)whole;
        if (Hudtimer_time < 1)
        {
            Hudtimer_state = HTS_COUNTDOWN_FINISHED;
            Hudtimer_time = 0;
        }
    }
}

// 0x007CD970
void game_hudtimer_render()
{
    if (Hudtimer_state == HTS_COUNTUP_FINISHED || Hudtimer_state == HTS_COUNTDOWN_FINISHED)
    {
        return;
    }
    gr_set_color(Int_color_white_text_normal);
    const int time = Hudtimer_time;
    const int seconds = time / 1000;
    char text[64];
    sprintf(text, "%02i:%02i:%03i", seconds / 60, seconds % 60, time % 1000);
    int w, h;
    gr_get_string_size(w, h, text, -1, General_font);
    // Magic number: 25 pixels from the top.
    gr_string(rl_render_lib::get_instance()->get_screen_info()->get_screen_width() / 2 - w / 2, 25, text, General_font, GM_FILTER);
}

// 0x007C1B20
void game_hudtimer_stop()
{
    switch (Hudtimer_state)
    {
        case HTS_COUNTING_UP:
        case HTS_COUNTUP_PAUSED:
            Hudtimer_state = HTS_COUNTUP_FINISHED;
            break;
        case HTS_COUNTING_DOWN:
        case HTS_COUNTDOWN_PAUSED:
            Hudtimer_state = HTS_COUNTDOWN_FINISHED;
            break;
        default:
            break;
    }
}

// 0x007C1B60
void game_hudtimer_pause(bool pause)
{
    switch (Hudtimer_state)
    {
        case HTS_COUNTING_UP:
            if (pause)
            {
                Hudtimer_state = HTS_COUNTUP_PAUSED;
            }
            break;
        case HTS_COUNTING_DOWN:
            if (pause)
            {
                Hudtimer_state = HTS_COUNTDOWN_PAUSED;
            }
            break;
        case HTS_COUNTUP_PAUSED:
            if (!pause)
            {
                Hudtimer_state = HTS_COUNTING_UP;
            }
            break;
        case HTS_COUNTDOWN_PAUSED:
            if (!pause)
            {
                Hudtimer_state = HTS_COUNTING_DOWN;
            }
            break;
        default:
            break;
    }
}

// 0x007C1BD0
int game_hudtimer_get_time()
{
    return Hudtimer_time;
}

// 0x007C1BE0
bool game_hudtimer_running()
{
    return Hudtimer_state == HTS_COUNTING_UP || Hudtimer_state == HTS_COUNTING_DOWN;
}

// 0x007C1C20 (rfg.exe does nothing with it)
void game_hudtimer_set_expiration_callback(unsigned short lua_func_handle)
{
}
