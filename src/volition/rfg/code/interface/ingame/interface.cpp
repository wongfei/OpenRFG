// Original: volition/rfg/code/interface/ingame/interface.cpp
//
// Partial (orig_main.md OM7): the interface mode stack, the gameplay frame's interface process (the pause action pushes
// GS_INGAME_OPTIONS, the window losing the foreground pushes GS_QUICK_PAUSE in single player) and render, the letterbox
// and the old interface message box. All vint-free. Not ported: the mouse helpers, the debug mode, the line drawing of
// the message box (render_mbox, see below) and the tight-loop box.
//
// rfg.exe never fills Interface_funcs (it has only reads), so the modes have no callbacks, and never opens Mbox
// (interface_open_mbox has no callers): the mode stack only tracks Interface_mode, and the box branches are dead.

#include "volition/rfg/code/interface/ingame/interface.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/cutscene/cutscene.h"
#include "volition/rfg/code/cutscene/video_cutscene.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/gamewide.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/destruction/rfg_destruction.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/os/os.h"
#include "volition/vlib/code/util/string_util.h"

#include <string.h>

interface_modes Interface_mode = INT_MODE_NONE; // 0x01662E6C
static interface_modes Pending_interface_mode = INT_MODE_NONE; // 0x01662E68
static int Pending_interface_push; // 0x02C0348C
static int Pending_interface_pop; // 0x02C03490
// Magic number: 8 modes deep.
static interface_modes Interface_stack[8]; // 0x02C02B94
static int Interface_stack_depth; // 0x02C03488
static int_mode_funcs Interface_funcs[NUM_INT_MODES]; // 0x02C033E8, never filled
static int_mbox Mbox; // 0x016651B0
static int Snd_confirm = -1; // 0x01662E58
static int Snd_cancel = -1; // 0x01662E5C
int Snd_scroll = -1; // 0x01662E60 (external: the options_xlive menus, one unity TU in rfg.exe)
// size 0x18
struct special_font_info
{
    char name[16]; // 0x00
    char text[4]; // 0x10
    int name_len; // 0x14
};
// The button font's glyphs in text: a token's name, its one-character text and the length to replace.
// Original bug kept: INT_A, INT_B, INT_X and INT_Y have lengths past their names (5, 10, 12, 10), so more of the text
// than the token can match and go.
static const special_font_info Special_font[15] = { // 0x01662E98
    {"INT_UP_ARROW", "\xA9", 12},
    {"INT_DOWN_ARROW", "\xAA", 14},
    {"INT_RIGHT_ARROW", "\xAB", 15},
    {"INT_LEFT_ARROW", "\xAC", 14},
    {"INT_A", "\xAD", 5},
    {"INT_B", "\xAE", 10},
    {"INT_X", "\xAF", 12},
    {"INT_Y", "\xB0", 10},
    {"INT_L1", "\xB1", 6},
    {"INT_L2", "\xB2", 6},
    {"INT_L3", "\xB3", 6},
    {"INT_R1", "\xB4", 6},
    {"INT_R2", "\xB5", 6},
    {"INT_R3", "\xB6", 6},
    {"INT_POUND", "\xA3", 9},
};
bool Letterbox_enabled; // 0x01662CD2
int Letterbox_bar_height; // 0x01662E64
float Letterbox_pct; // 0x02BF2218
bool Letterbox_fading_in; // 0x02BF2224
timestamp Letterbox_fade_timer; // 0x01665C0C

// 0x008845D0
int_mbox::int_mbox()
{
    anim_time.invalidate();
}

// ---- modes

// 0x008847B0 (:895)
void interface_set_mode(interface_modes mode)
{
    Pending_interface_mode = mode;
    Pending_interface_push = 0;
    Pending_interface_pop = 0;
}

// 0x00884880 (:902)
void interface_push_mode(interface_modes mode)
{
    if (Interface_mode != mode)
    {
        Pending_interface_mode = mode;
        Pending_interface_push = 1;
    }
}

// 0x008848A0 (:913)
void interface_pop_mode()
{
    if (Pending_interface_pop < Interface_stack_depth && Interface_stack_depth > 0)
    {
        Pending_interface_pop++;
        Pending_interface_push = 0;
    }
}

// 0x00884860
interface_modes interface_get_pending_mode()
{
    return Pending_interface_mode;
}

// 0x00884870 (:946)
interface_modes interface_get_mode()
{
    return Interface_mode;
}

// 0x008848D0
// The stack index of a mode, or -1.
int interface_find_mode(interface_modes mode)
{
    for (int i = 0; i < Interface_stack_depth; i++)
    {
        if (Interface_stack[i] == mode)
        {
            return i;
        }
    }
    return -1;
}

// 0x008847D0
// Every mode exits (top first) and the stack restarts with INT_MODE_NORMAL alone.
void interface_clear_modes()
{
    for (int i = Interface_stack_depth - 1; i >= 0; i--)
    {
        if (Interface_funcs[Interface_stack[i]].exit != nullptr)
        {
            Interface_funcs[Interface_stack[i]].exit();
        }
    }
    Interface_stack_depth = 0;
    Interface_mode = INT_MODE_NONE;
    Pending_interface_mode = INT_MODE_NORMAL;
    Pending_interface_push = 0;
    Pending_interface_pop = 0;
    if (Interface_funcs[INT_MODE_NORMAL].enter != nullptr)
    {
        Interface_funcs[INT_MODE_NORMAL].enter();
    }
    Interface_stack[0] = Pending_interface_mode;
    Interface_stack_depth = 1;
    Interface_mode = INT_MODE_NORMAL;
    if (Pending_interface_mode == INT_MODE_NORMAL)
    {
        Pending_interface_push = 0;
        Pending_interface_mode = INT_MODE_NONE;
    }
}

// 0x00884950 (:1943), a jump to interface_clear_modes
void interface_level_shutdown()
{
    interface_clear_modes();
}

// 0x008845F0
// Applies the pending pops, then the pending mode: a set replaces the stack, a push adds to it.
void interface_check_pending_mode()
{
    const int push = Pending_interface_push;
    const interface_modes old_mode = Interface_mode;
    const interface_modes new_mode = Pending_interface_mode;
    int pops = Pending_interface_pop;
    // A set (no push, no pops) of another mode pops the whole stack first.
    if (Pending_interface_pop == 0)
    {
        pops = 0;
        if (Pending_interface_mode != INT_MODE_NONE && Pending_interface_mode != Interface_mode && Pending_interface_push == 0)
        {
            pops = Interface_stack_depth;
        }
    }
    for (; pops > 0; pops--)
    {
        int exiting = -1;
        if (Interface_stack_depth < 2)
        {
            Interface_mode = INT_MODE_NORMAL;
        }
        else
        {
            exiting = Interface_stack[Interface_stack_depth - 1];
            Interface_mode = Interface_stack[Interface_stack_depth - 2];
        }
        if (exiting != -1 && Interface_funcs[exiting].exit != nullptr)
        {
            Interface_funcs[exiting].exit();
        }
        Pending_interface_pop--;
        Interface_stack_depth--;
    }
    if (Pending_interface_pop < 0)
    {
        Pending_interface_pop = 0;
    }
    if (new_mode == old_mode || new_mode == INT_MODE_NONE)
    {
        if (push != 0)
        {
            VLIB_CRITICAL_FAILURE();
        }
        return;
    }
    if (Interface_funcs[Pending_interface_mode].enter != nullptr)
    {
        Interface_funcs[Pending_interface_mode].enter();
    }
    const interface_modes entered = Pending_interface_mode;
    if (push == 0)
    {
        Interface_stack_depth = 1;
        Interface_stack[0] = Pending_interface_mode;
    }
    else
    {
        Interface_stack[Interface_stack_depth] = Pending_interface_mode;
        Interface_stack_depth++;
    }
    Interface_mode = new_mode;
    if (new_mode == entered)
    {
        Pending_interface_push = 0;
        Pending_interface_mode = INT_MODE_NONE;
    }
}

// 0x00884740
void interface_sound_init()
{
    Snd_confirm = -1;
    Snd_cancel = -1;
    Snd_scroll = -1;
}

// 0x008A6DD0 (:851): letterbox_init and interface_sound_init, inlined.
void interface_system_init()
{
    letterbox_init();
    interface_sound_init();
}

// 0x008A86A0 (:1934)
void interface_level_init_post()
{
    Letterbox_fading_in = true;
    Letterbox_pct = 0.0f;
    Letterbox_fade_timer.invalidate();
}

// 0x00884AA0 (:2047)
void interface_letterbox_fade_in(bool immediate)
{
    Letterbox_fading_in = true;
    if (immediate)
    {
        Letterbox_pct = 1.0f;
        Letterbox_fade_timer.invalidate();
        return;
    }
    Letterbox_pct = 0.0f;
    // Magic number: a one second fade.
    Letterbox_fade_timer.set(1000);
}

// 0x00884AF0 (:2063)
void interface_letterbox_fade_out(bool immediate)
{
    Letterbox_fading_in = false;
    if (immediate)
    {
        Letterbox_pct = 0.0f;
        Letterbox_fade_timer.invalidate();
        return;
    }
    Letterbox_pct = 1.0f;
    // Magic number: a one second fade.
    Letterbox_fade_timer.set(1000);
}

// ---- the frame

// 0x008DD9D0 (:1752)
// The gameplay frame's interface: the pause action opens the in-game options (GS_INGAME_OPTIONS) once nothing is in
// the way (a pause, a pending state, a cutscene, a dead single player, a fade, a multiplayer menu box), else the modes
// process; in single player a lost foreground without the cursor pauses the game (GS_QUICK_PAUSE).
void interface_process()
{
    multi_core_platform_get_active_controller();
    interface_check_pending_mode();
    const bool mbox_open = (Mbox.flags & 0x80000000) != 0;
    const bool pause_pressed = controls_is_button_just_pressed(CBA_GAMEPLAY_PAUSE, Player_controller_index) && gameseq_get_state() == GS_GAMEPLAY;
    // The platform overlay (the Steam overlay: the game framework out of the foreground) pauses single player too.
    const bool platform_ui = multi_core_platform_ui_is_active() && gameseq_get_state() == GS_GAMEPLAY && !Multiplayer;
    if (mbox_open)
    {
        interface_process_mbox();
    }
    else if ((pause_pressed || platform_ui) && !game_is_paused() && gameseq_get_pending_state() == GS_NONE && !Gameplay_shutdown_in_progress)
    {
        player* pp = player_get_local_player();
        if (Multiplayer && Multi_match_info.match_state == MGMS_VIEWING_RESULTS)
        {
            letterbox_process();
            return;
        }
        if (!cutscene_is_playing() && !video_cutscene_pending_or_playing() && (Multiplayer || (pp != nullptr && !pp->is_killed())))
        {
            if (!gameseq_state_is_active(GS_INGAME_OPTIONS) && !mission_is_terminating() && ui_fade_to_black_is_fade_in_complete() &&
                (!Multiplayer || !multi_player_menu_system_has_open_message_box()))
            {
                gameseq_push_state(GS_INGAME_OPTIONS, true, false);
            }
        }
        // Original: a single player without a local player (pp null) falls through to the quick pause check below
        // without opening the menu.
    }
    else
    {
        for (int i = 0; i < Interface_stack_depth; i++)
        {
            if (Interface_funcs[Interface_stack[i]].process != nullptr)
            {
                Interface_funcs[Interface_stack[i]].process(i != Interface_stack_depth - 1 ? 1 : 0);
            }
        }
    }
    if (!Multiplayer && !os_foreground() && !Cursor_visible)
    {
        gameseq_push_state(GS_QUICK_PAUSE, false, false);
    }
    letterbox_process();
}

// 0x008A8630
// The interface outside gameplay: the pending mode, the modes or the box, the letterbox.
void interface_nongame_process()
{
    interface_check_pending_mode();
    if ((Mbox.flags & 0x80000000) != 0)
    {
        interface_process_mbox();
    }
    else
    {
        for (int i = 0; i < Interface_stack_depth; i++)
        {
            if (Interface_funcs[Interface_stack[i]].process != nullptr)
            {
                Interface_funcs[Interface_stack[i]].process(i != Interface_stack_depth - 1 ? 1 : 0);
            }
        }
    }
    letterbox_process();
}

// 0x008BEE40 (:1899)
void interface_render()
{
    for (int i = 0; i < Interface_stack_depth; i++)
    {
        if (!cutscene_is_playing() && Interface_funcs[Interface_stack[i]].render != nullptr)
        {
            Interface_funcs[Interface_stack[i]].render();
        }
    }
    rfg_message_process_and_render();
    letterbox_render();
    interface_render_mbox();
}

// ---- the letterbox

// 0x008A6B60
// Bars above and below when the screen is narrower than 16:9 (Letterbox_bar_height each).
void letterbox_init()
{
    const rl_screen_info* screen = rl_render_lib::get_instance()->get_screen_info();
    const unsigned int height = (unsigned int)screen->get_screen_height();
    // Magic numbers: the 16:9 height of the screen width.
    const unsigned int height_16_9 = (unsigned int)(screen->get_screen_width() * 9 / 16);
    if (height_16_9 < height)
    {
        Letterbox_bar_height = (int)((height - height_16_9) >> 1);
        Letterbox_enabled = true;
        return;
    }
    Letterbox_enabled = false;
    Letterbox_bar_height = 0;
}

// 0x008A6BB0
// The fade percentage (1: bars fully in), and the screen clip inside the bars while they are in.
void letterbox_process()
{
    // Original bug kept: the fade is computed only while its timer is NOT set (timestamp::valid's sense is inverted), so
    // a running fade reads 0 and an unset timer reads its fade direction's end (0 fading in, 1 fading out: an unset
    // timer's time_until is the wrap value, 1,800,000,000 ms). Fades take 1000 ms.
    float pct = 0.0f;
    if (!Letterbox_fade_timer.valid())
    {
        if (Letterbox_fading_in)
        {
            if (Letterbox_fade_timer.elapsed())
            {
                pct = 1.0f;
                Letterbox_fade_timer.invalidate();
            }
            else
            {
                pct = (float)((double)(1000 - Letterbox_fade_timer.time_until()) / 1000.0);
            }
        }
        else if (Letterbox_fade_timer.elapsed())
        {
            pct = 0.0f;
            Letterbox_fade_timer.invalidate();
        }
        else
        {
            pct = (float)((double)Letterbox_fade_timer.time_until() / 1000.0);
        }
    }
    if ((float)((double)pct - 0.0) < 0.0f)
    {
        pct = 0.0f;
    }
    if ((float)((double)pct - 1.0) >= 0.0f)
    {
        pct = 1.0f;
    }
    Letterbox_pct = pct;
    const rl_screen_info* screen = rl_render_lib::get_instance()->get_screen_info();
    const int width = screen->get_screen_width();
    if (Letterbox_enabled && Letterbox_pct == 1.0f)
    {
        game_render_set_screen_clip(0, Letterbox_bar_height, width, screen->get_screen_height() - Letterbox_bar_height * 2);
        return;
    }
    game_render_set_screen_clip(0, 0, width, screen->get_screen_height());
}

// 0x008A6D20
void letterbox_render()
{
    if (gameseq_get_state() == GS_GAMEPLAY && Letterbox_pct > 0.0f && Letterbox_pct <= 1.0f)
    {
        // Magic number: the bars' alpha follows the fade.
        gr_set_color(0, 0, 0, fl_round(Letterbox_pct * 255.0f));
        const rl_screen_info* screen = rl_render_lib::get_instance()->get_screen_info();
        const int bar = Letterbox_bar_height;
        // Magic number: 10 pixels wider than the screen.
        const int w = screen->get_screen_width() + 10;
        gr_rect(0, 0, w, bar + 1, GM_FILTER);
        gr_rect(0, screen->get_screen_height() - bar, w, bar + 1, GM_FILTER);
    }
}

// ---- the interface message box (never opened by rfg.exe)

// 0x00884900
void interface_close_mbox()
{
    if ((Mbox.flags & 1) != 0 && game_is_paused())
    {
        game_unpause();
    }
    Mbox.flags = 0;
}

// 0x00884930
bool interface_is_mbox_open()
{
    return (Mbox.flags & 0x80000000) != 0;
}

// 0x008A6970
// Up/down move the selection, select or (unless flag 2) abort, or start with flag 4, close the box and call back with
// the option (-1 for cancel); a press during the opening animation ends the animation instead. Every control is masked
// while the box is open.
void interface_process_mbox()
{
    if ((Mbox.flags & 0x80000000) == 0)
    {
        return;
    }
    if (Mbox.anim_time.elapsed())
    {
        Mbox.anim_time.invalidate();
    }
    if (controls_process_button_pressed(CBA_MENU_UP, Player_controller_index))
    {
        if (Mbox.num_options > 1)
        {
            audiolib_cue_play(Snd_scroll, nullptr, nullptr);
            Mbox.selected_option = Mbox.selected_option < 1 ? Mbox.num_options - 1 : Mbox.selected_option - 1;
        }
    }
    else if (controls_process_button_pressed(CBA_MENU_DOWN, Player_controller_index))
    {
        if (Mbox.num_options > 1)
        {
            audiolib_cue_play(Snd_scroll, nullptr, nullptr);
            Mbox.selected_option = Mbox.selected_option >= Mbox.num_options - 1 ? 0 : Mbox.selected_option + 1;
        }
    }
    else
    {
        bool close = false;
        if ((Mbox.flags & 2) == 0 && (controls_process_button_just_pressed(CBA_MENU_ABORT, Player_controller_index) ||
                                      ((Mbox.flags & 4) != 0 && controls_process_button_just_pressed(CBA_MENU_START, Player_controller_index))))
        {
            if (Mbox.anim_time.valid())
            {
                Mbox.anim_time.invalidate();
            }
            else
            {
                audiolib_cue_play(Snd_cancel, nullptr, nullptr);
                Mbox.selected_option = -1;
                close = true;
            }
        }
        else if (controls_process_button_just_pressed(CBA_MENU_SELECT, Player_controller_index))
        {
            if (Mbox.anim_time.valid())
            {
                Mbox.anim_time.invalidate();
            }
            else
            {
                audiolib_cue_play(Snd_confirm, nullptr, nullptr);
                close = true;
            }
        }
        if (close)
        {
            interface_close_mbox();
            if (Mbox.callback != nullptr)
            {
                Mbox.callback(Mbox.selected_option);
            }
        }
    }
    controls_mask_all(Player_controller_index);
}

// 0x008BEE30, render_mbox(&Mbox) (0x008BE4D0)
void interface_render_mbox()
{
    // STUB: the box's drawing (render_mbox, render_mbox_opening, interface_draw_horz_line_thick) is not ported; rfg.exe
    // never opens the box.
    if ((Mbox.flags & 0x80000000) != 0)
    {
        VLIB_CRITICAL_FAILURE();
    }
}

// 0x00884960 (:2011)
// Each known INT_ token in the text becomes its glyph.
// Original bug kept: an unknown INT_ token is searched for again from the same place, for ever.
void interface_replace_special_font(char* str)
{
    char* search_from = str;
    char* token = strstr(str, "INT_");
    while (token != nullptr)
    {
        const int length = (int)strlen(str);
        for (int i = 0; i < 15; i++)
        {
            if (strncmp(token, Special_font[i].name, Special_font[i].name_len) == 0)
            {
                // Magic number: 1 KB of text after the token, unchecked.
                char tail[1024];
                const int tail_length = (int)strlen(token);
                strncpy_safe(tail, token, tail_length + 1);
                str[length - tail_length] = '\0';
                strncat(str, Special_font[i].text, 5);
                strcat(str, tail + Special_font[i].name_len);
                search_from = token + 1;
                break;
            }
        }
        token = strstr(search_from, "INT_");
    }
}
