// Original: volition/rfg/code/interface/menu_transitions.cpp
//
// The menus' transition effects: a fade in and out (the screen fade and the cross fade are the same timers) and the
// wipes right and left (a scissor region that grows over the screen width). UI.md UI4.

#include "volition/rfg/code/interface/menu_transitions.h"

#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/os/frametime.h"

// ---- file-private types (rfg.pdb)
// size 0x8
struct transition_fx_funcs
{
    bool (*transition_in)(bool*, bool); // 0x00
    bool (*transition_out)(bool*); // 0x04
};
static_assert(sizeof(transition_fx_funcs) == 0x8, "transition_fx_funcs");

static bool transition_in_screen_fade(bool* finished, bool out_in_progress);
static bool transition_out_screen_fade(bool* finished);
static bool transition_in_cross_fade(bool* finished, bool out_in_progress);
static bool transition_out_cross_fade(bool* finished);
static bool transition_in_wipe_right(bool* finished, bool out_in_progress);
static bool transition_out_wipe_right(bool* finished);
static bool transition_in_wipe_left(bool* finished, bool out_in_progress);
static bool transition_out_wipe_left(bool* finished);

// ---- file-static globals
// Magic numbers: the fades last 300 ms, the wipes 500 ms.
static const float Fade_time = 300.0f;
static const float Wipe_time = 500.0f;

static transition_fx_funcs Tfx_funcs[4] = {
    {transition_in_screen_fade, transition_out_screen_fade},
    {transition_in_cross_fade, transition_out_cross_fade},
    {transition_in_wipe_right, transition_out_wipe_right},
    {transition_in_wipe_left, transition_out_wipe_left},
}; // 0x016631E4
static float Fade_in_pct = 1.0f; // 0x01663204
static float Fade_out_pct = 1.0f; // 0x01663208
static float Wipe_x_right_stamp; // 0x02BE9E24
static bool In_in_progress; // 0x02BF2227
static float Wipe_x_left_stamp; // 0x02BFE080
static bool Out_in_progress; // 0x02C00159
static float Fade_in_stamp; // 0x02C03588
static float Fade_out_stamp; // 0x02C0358C
static int Wipe_x_right; // 0x02C03590
static int Wipe_x_left; // 0x02C03594

static int screen_width()
{
    return rl_render_lib::get_instance()->get_screen_info()->get_screen_width();
}

static int screen_height()
{
    return rl_render_lib::get_instance()->get_screen_info()->get_screen_height();
}

// 0x00889A60 (:117)
static void fade_in_set()
{
    Fade_in_stamp = Fade_time;
    Fade_in_pct = 0.0f;
}

// 0x00889A80 (:123)
static void fade_out_set()
{
    Fade_out_stamp = Fade_time;
    Fade_out_pct = 1.0f;
}

// 0x00889AB0 (:129)
static void fade_in_process()
{
    if (Fade_in_stamp > 0.0f)
    {
        Fade_in_stamp = Fade_in_stamp - Frametime * 1000.0f;
        if (Fade_in_stamp < 0.0f)
        {
            Fade_in_stamp = 0.0f;
        }
        Fade_in_pct = fl_parametric_accel_decel(1.0f - Fade_in_stamp / Fade_time, 2);
        return;
    }
    Fade_in_pct = 1.0f;
    Fade_in_stamp = 0.0f;
}

// 0x00889B60 (:141)
static void fade_out_process()
{
    if (Fade_out_stamp > 0.0f)
    {
        Fade_out_stamp = Fade_out_stamp - Frametime * 1000.0f;
        if (Fade_out_stamp < 0.0f)
        {
            Fade_out_stamp = 0.0f;
        }
        Fade_out_pct = fl_parametric_accel_decel(Fade_out_stamp / Fade_time, 2);
        return;
    }
    Fade_out_pct = 1.0f;
    Fade_out_stamp = 0.0f;
}

// 0x00889C00 (:153)
static void wipe_right_set()
{
    Wipe_x_right_stamp = Wipe_time;
    Wipe_x_right = 0;
    Wipe_x_left = 0;
}

// 0x008ABDB0 (:160)
// Original bug kept: the wipe in never counts its stamp down (only the wipe out does), so a wipe in alone never ends.
static void wipe_right_in_process()
{
    if (Wipe_x_right_stamp > 0.0f)
    {
        Wipe_x_right = (int)((1.0f - Wipe_x_right_stamp / Wipe_time) * (float)screen_width());
        return;
    }
    Wipe_x_right_stamp = 0.0f;
}

// 0x008ABE10 (:171)
static void wipe_right_out_process()
{
    if (Wipe_x_right_stamp > 0.0f)
    {
        Wipe_x_right_stamp = Wipe_x_right_stamp - Frametime * 1000.0f;
        if (Wipe_x_right_stamp < 0.0f)
        {
            Wipe_x_right_stamp = 0.0f;
        }
        Wipe_x_left = (int)((1.0f - Wipe_x_right_stamp / Wipe_time) * (float)screen_width());
        return;
    }
    Wipe_x_right_stamp = 0.0f;
}

// 0x008ABEC0 (:183)
static void wipe_left_set()
{
    Wipe_x_left_stamp = Wipe_time;
    Wipe_x_right = screen_width();
    Wipe_x_left = screen_width();
}

// 0x008ABF00 (:190)
// Original bug kept: as wipe_right_in_process, the stamp is not counted down.
static void wipe_left_in_process()
{
    if (Wipe_x_left_stamp > 0.0f)
    {
        Wipe_x_left = (int)((Wipe_x_left_stamp / Wipe_time) * (float)screen_width());
        return;
    }
    Wipe_x_left_stamp = 0.0f;
}

// 0x008ABF50 (:201)
static void wipe_left_out_process()
{
    if (Wipe_x_left_stamp > 0.0f)
    {
        Wipe_x_left_stamp = Wipe_x_left_stamp - Frametime * 1000.0f;
        if (Wipe_x_left_stamp < 0.0f)
        {
            Wipe_x_left_stamp = 0.0f;
        }
        Wipe_x_right = (int)((Wipe_x_left_stamp / Wipe_time) * (float)screen_width());
        return;
    }
    Wipe_x_left_stamp = 0.0f;
}

// 0x00889C20 (:213)
// While an out transition runs, the in transition waits (and reports its state).
static bool transition_in_screen_fade(bool* finished, bool out_in_progress)
{
    if (!out_in_progress)
    {
        if (!In_in_progress)
        {
            fade_in_set();
            In_in_progress = true;
            *finished = false;
            return true;
        }
        fade_in_process();
        if (Fade_in_stamp == 0.0f)
        {
            In_in_progress = false;
            *finished = true;
            return false;
        }
    }
    return In_in_progress;
}

// 0x00889C90 (:232)
static bool transition_out_screen_fade(bool* finished)
{
    if (!Out_in_progress)
    {
        fade_out_set();
        Out_in_progress = true;
        *finished = false;
        return true;
    }
    fade_out_process();
    if (Fade_out_stamp == 0.0f)
    {
        Out_in_progress = false;
        *finished = true;
        return false;
    }
    return Out_in_progress;
}

// 0x00889DA0 (:249)
// The cross fade runs its in transition at once (out_in_progress unused).
static bool transition_in_cross_fade(bool* finished, bool)
{
    if (!In_in_progress)
    {
        fade_in_set();
        In_in_progress = true;
        *finished = false;
        return true;
    }
    fade_in_process();
    if (Fade_in_stamp == 0.0f)
    {
        In_in_progress = false;
        *finished = true;
        return false;
    }
    return In_in_progress;
}

// 0x00889EC0 (:268)
static bool transition_out_cross_fade(bool* finished)
{
    if (!Out_in_progress)
    {
        fade_out_set();
        Out_in_progress = true;
        *finished = false;
        return true;
    }
    fade_out_process();
    if (Fade_out_stamp == 0.0f)
    {
        Out_in_progress = false;
        *finished = true;
        return false;
    }
    return Out_in_progress;
}

// 0x008ABFF0 (:285)
static bool transition_in_wipe_right(bool* finished, bool)
{
    if (!In_in_progress)
    {
        wipe_right_set();
        In_in_progress = true;
        *finished = false;
        return true;
    }
    wipe_right_in_process();
    if (Wipe_x_right_stamp == 0.0f)
    {
        In_in_progress = false;
        *finished = true;
        return false;
    }
    return In_in_progress;
}

// 0x008AC0C0 (:304)
static bool transition_out_wipe_right(bool* finished)
{
    if (!Out_in_progress)
    {
        wipe_right_set();
        Out_in_progress = true;
        *finished = false;
        return true;
    }
    wipe_right_out_process();
    if (Wipe_x_right_stamp == 0.0f)
    {
        Out_in_progress = false;
        *finished = true;
        return false;
    }
    return Out_in_progress;
}

// 0x008AC1D0 (:321)
static bool transition_in_wipe_left(bool* finished, bool)
{
    if (!In_in_progress)
    {
        wipe_left_set();
        In_in_progress = true;
        *finished = false;
        return true;
    }
    wipe_left_in_process();
    if (Wipe_x_left_stamp == 0.0f)
    {
        In_in_progress = false;
        *finished = true;
        return false;
    }
    return In_in_progress;
}

// 0x008AC2A0 (:340)
static bool transition_out_wipe_left(bool* finished)
{
    if (!Out_in_progress)
    {
        wipe_left_set();
        Out_in_progress = true;
        *finished = false;
        return true;
    }
    wipe_left_out_process();
    if (Wipe_x_left_stamp == 0.0f)
    {
        Out_in_progress = false;
        *finished = true;
        return false;
    }
    return Out_in_progress;
}

// 0x00889FD0 (:363)
float transition_fade_in_pct()
{
    if (Fade_in_stamp > 0.0f)
    {
        return Fade_in_pct;
    }
    return 1.0f;
}

// 0x00889FF0 (:371)
float transition_fade_out_pct()
{
    if (Fade_out_stamp > 0.0f)
    {
        return Fade_out_pct;
    }
    return 1.0f;
}

// 0x008AC3C0 (:379)
void transition_wipe_in()
{
    if (Wipe_x_right_stamp > 0.0f)
    {
        gr_set_scissor_region(0, 0, Wipe_x_right, screen_height(), 0);
        return;
    }
    if (Wipe_x_left_stamp > 0.0f)
    {
        gr_set_scissor_region(Wipe_x_right, 0, screen_width(), screen_height(), 0);
    }
}

// 0x008AC440 (:388)
void transition_wipe_out()
{
    if (Wipe_x_right_stamp > 0.0f)
    {
        gr_set_scissor_region(Wipe_x_left, 0, screen_width(), screen_height(), 0);
        return;
    }
    if (Wipe_x_left_stamp > 0.0f)
    {
        gr_set_scissor_region(0, 0, Wipe_x_left, screen_height(), 0);
    }
}

// 0x0088A010 (:397)
bool transition_in_valid(transition_fx fx)
{
    if (fx < NUM_TFX_MODES)
    {
        return Tfx_funcs[fx].transition_in != nullptr;
    }
    return false;
}

// 0x0088A030 (:408)
bool transition_out_valid(transition_fx fx)
{
    if (fx < NUM_TFX_MODES)
    {
        return Tfx_funcs[fx].transition_out != nullptr;
    }
    return false;
}

// 0x0088A050 (:419)
bool transition_in(transition_fx fx, bool* finished, bool out_in_progress)
{
    return Tfx_funcs[fx].transition_in(finished, out_in_progress);
}

// 0x0088A070 (:424)
bool transition_out(transition_fx fx, bool* finished)
{
    return Tfx_funcs[fx].transition_out(finished);
}

// 0x0088A090 (:429)
void transition_reset_in_progresses()
{
    In_in_progress = false;
    Out_in_progress = false;
}
