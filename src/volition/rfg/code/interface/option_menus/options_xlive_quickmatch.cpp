// Original: volition/rfg/code/interface/option_menus/options_xlive_quickmatch.cpp
//
// The Xbox Live quickmatch options menu (stub_impl.md S7), a debug text menu on gr_string. Only Back does anything.

#include "volition/rfg/code/interface/option_menus/option_menu_system.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/gr/gr_font.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/ingame/interface.h"
#include "volition/rfg/code/rfg.h"

extern float options_alpha; // 0x01664274, options_xlive.cpp

// ---- file-static globals
static int Selection_xlive_qm; // 0x02C07FD8

// 0x00892DE0 (:86)
void xbox_live_quickmatch_menu_enter(game_state prev_state)
{
    Selection_xlive_qm = 0;
}

// 0x00892DF0 (:91)
void xbox_live_quickmatch_menu_exit(game_state new_state)
{
}

// 0x00892E00 (:95)
void xbox_live_quickmatch_menu_process()
{
    if (controls_process_button_pressed(CBA_MENU_UP, Player_controller_index))
    {
        Selection_xlive_qm--;
        if (Selection_xlive_qm < 0)
        {
            Selection_xlive_qm = 2;
        }
        audiolib_cue_play(Snd_scroll, nullptr, nullptr);
    }
    else if (controls_process_button_pressed(CBA_MENU_DOWN, Player_controller_index))
    {
        Selection_xlive_qm = (Selection_xlive_qm + 1) % 3;
        audiolib_cue_play(Snd_scroll, nullptr, nullptr);
    }
    else if (controls_process_button_just_pressed(CBA_MENU_SELECT, Player_controller_index) && Selection_xlive_qm == 2)
    {
        gameseq_pop_state();
    }
}

// 0x00892EA0 (:139)
void xbox_live_quickmatch_menu_render(rl_renderer*)
{
    const char* const entries[3] = {"Game Type", "Include Oversized", "Back"};
    gr_set_color(255.0f, 255.0f, 0.0f, options_alpha * 255.0f);
    // Magic numbers: the title at (40, 40), the entries from (80, 80), 20 pixels apart.
    gr_string(0x28, 0x28, "Options Menu", -1, GM_FILTER);
    int y = 0x50;
    for (int i = 0; i < 3; i++)
    {
        gr_set_color(255.0f, 255.0f, 255.0f, options_alpha * 255.0f);
        if (i == Selection_xlive_qm)
        {
            gr_set_color(255.0f, 0.0f, 255.0f, options_alpha * 255.0f);
        }
        gr_string(0x50, y, entries[i], -1, GM_FILTER);
        y += 0x14;
    }
}
