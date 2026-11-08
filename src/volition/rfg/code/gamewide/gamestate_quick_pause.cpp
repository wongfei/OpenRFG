// Original: volition/rfg/code/gamewide/gamestate_quick_pause.cpp
//
// The quick pause game state (GS_QUICK_PAUSE, stub_impl.md S5): the game pauses while its window is in the
// background, with a "paused" message box over a frozen frame; the state pops when the window is in front again.
// Compiled in rfg_ui (the message box and the document manager).

#include "volition/rfg/code/gamewide/gamestate_quick_pause.h"

#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/render_lib/code/rl_scene_renderer.h"
#include "volition/rfg/code/gamewide/game_handshake.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/vlib/code/os/os.h"

static unsigned int Control_id = 0xFFFFFFFF; // 0x0165CBD8
static int Msgbox_id = -1; // 0x0165CBDC

// 0x007E8FB0 (:28)
void gamestate_quick_pause_init()
{
    state_info callbacks;
    callbacks.exit_notify = nullptr;
    callbacks.user_flags = 0;
    callbacks.transparent = false;
    callbacks.pause_beneath = true;
    callbacks.hide_cursor = true;
    callbacks.enter = gamestate_quick_pause_enter;
    callbacks.exit = gamestate_quick_pause_exit;
    callbacks.process = gamestate_quick_pause_process;
    callbacks.handshake = gamestate_quick_pause_handshake;
    callbacks.render = gamestate_quick_pause_render;
    gameseq_register_state(GS_QUICK_PAUSE, callbacks);
}

// 0x007CD3D0 (:45)
// The message box, and the scene renderer keeps the last frame.
void gamestate_quick_pause_enter(game_state prev_state)
{
    Control_id = ui_request_control_id();
    Msgbox_id = ui_add_msgbox(MBT_LOADING, "MENU_PC_PAUSED", "", nullptr, false, false, nullptr, nullptr, false);
    rl_scene* scene = rl_get_active_scene();
    if (scene != nullptr)
    {
        scene->get_scene_renderer()->lock_framebuffer(true); // inlined in the binary
    }
}

// 0x007CD420 (:65)
// The scene renders again and the frozen frame's texture is released.
void gamestate_quick_pause_exit(game_state new_state)
{
    rl_scene* scene = rl_get_active_scene();
    if (scene != nullptr)
    {
        scene->get_scene_renderer()->lock_framebuffer(false); // inlined in the binary
    }
    if (Msgbox_id != -1)
    {
        ui_hide_msgbox(Msgbox_id, false);
    }
    ui_relinquish_control_id(Control_id);
    Control_id = 0xFFFFFFFF;
    Msgbox_id = -1;
}

// 0x007D9180 (:87)
void gamestate_quick_pause_process()
{
    ui_process(Control_id, false);
    if (os_foreground())
    {
        gameseq_pop_state();
    }
    Sim_snapshot.in_gameplay = true;
}

// 0x007C0570 (:100)
void gamestate_quick_pause_handshake(rl_renderer* pRenderer)
{
    os_sleep(10); // Magic number: 10 ms per frame while paused
}

// 0x007C0580 (:105)
void gamestate_quick_pause_render(rl_renderer* pRenderer)
{
    if (!os_minimized())
    {
        ui_render(Control_id, false);
    }
}
