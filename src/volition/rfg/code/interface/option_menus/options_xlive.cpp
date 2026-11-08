// Original: volition/rfg/code/interface/option_menus/options_xlive.cpp
//
// The Xbox Live options menu (stub_impl.md S7), a debug text menu on gr_string. Only its Quickmatch and Back entries do
// anything. Nothing in rfg.exe pushes its state.

#include "volition/rfg/code/interface/option_menus/option_menu_system.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/gr/gr_font.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/ingame/interface.h"
#include "volition/rfg/code/rfg.h"

float options_alpha = 1.0f; // 0x01664274, only these two menus read it (placement inferred)

// ---- file-static globals
static int Selection_xlive; // 0x02C07FD4

// 0x00892BD0 (:87)
void xbox_live_menu_enter(game_state prev_state)
{
    Selection_xlive = 0;
}

// 0x00892BE0 (:92)
void xbox_live_menu_exit(game_state new_state)
{
}

// 0x00892BF0 (:96)
void xbox_live_menu_process()
{
    if (controls_process_button_pressed(CBA_MENU_UP, Player_controller_index))
    {
        Selection_xlive--;
        if (Selection_xlive < 0)
        {
            Selection_xlive = 3;
        }
        audiolib_cue_play(Snd_scroll, nullptr, nullptr);
    }
    else if (controls_process_button_pressed(CBA_MENU_DOWN, Player_controller_index))
    {
        Selection_xlive = (Selection_xlive + 1) % 4;
        audiolib_cue_play(Snd_scroll, nullptr, nullptr);
    }
    else if (controls_process_button_just_pressed(CBA_MENU_SELECT, Player_controller_index))
    {
        if (Selection_xlive == 2)
        {
            gameseq_push_state(GS_MAINMENU_OPTIONS_LIVE_QUICKMATCH, false, false);
        }
        else if (Selection_xlive == 3)
        {
            gameseq_pop_state();
        }
    }
}

// 0x00892CA0 (:144)
void xbox_live_menu_render(rl_renderer*)
{
    const char* const entries[4] = {"Appear Offline", "Sign me in at Startup", "Quickmatch Options...", "Back"};
    gr_set_color(255.0f, 255.0f, 0.0f, options_alpha * 255.0f);
    // Magic numbers: the title at (40, 40), the entries from (80, 80), 20 pixels apart.
    gr_string(0x28, 0x28, "Xbox Live Menu", -1, GM_FILTER);
    int y = 0x50;
    for (int i = 0; i < 4; i++)
    {
        gr_set_color(255.0f, 255.0f, 255.0f, options_alpha * 255.0f);
        if (i == Selection_xlive)
        {
            gr_set_color(255.0f, 0.0f, 255.0f, options_alpha * 255.0f);
        }
        gr_string(0x50, y, entries[i], -1, GM_FILTER);
        y += 0x14;
    }
}
