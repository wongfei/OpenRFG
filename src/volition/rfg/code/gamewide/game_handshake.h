// Original: volition/rfg/code/gamewide/game_handshake.h
// Implementation: volition/rfg/code/gamewide/game_handshake.cpp
//
// The per-frame sync point between the sim thread and the render thread (decompiled behaviour).
// game_handshake_do_frame(renderer), called at the end of rfg_do_frame with the sim-thread renderer:
//   1. Under MP_send_setup_cs: activate the pending multiplayer send trigger and wait for the MP process trigger.
//   2. keen::rfg::start_middle_of_frame()  - waits until the render thread is idle (see application.hpp).
//   3. game_handshake_do_middle_of_frame:
//        In_handshake; rl_renderer::handshake_begin; audio panic check; console_do_frame_process;
//        video_player_update_render_all; ui_handshake; cutscene_do_handshake; multi_handshake;
//        gameseq_process(GS_STEP_HANDSHAKE)  (gameplay: gamestate_gp_handshake, world::do_frame, save/load);
//        stream2_do_frame(true) unless the top state has user_flags bit 0 or audio localization is loading;
//        Render_snapshot = Sim_snapshot; clone the main render camera into Deferred_scene_camera;
//        start deferred primitive rendering. The render lib is unlocked meanwhile while in gameplay.
//   4. handshake_deferred_render(Sim_snapshot, renderer).
//   5. keen::rfg::end_middle_of_frame()  - releases the render thread.
//   6. Sim_snapshot is reset (in_gameplay = false, stack_top_index = -1) for the next frame.
// World and streaming state may only change inside this window.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class rl_renderer;

// What the render thread may read about the sim. The sim fills Sim_snapshot during its frame (state stack via
// game_handshake_copy_gameseq, in_gameplay from gamestate_gp_process); the handshake copies it to Render_snapshot.
// size 0x48
struct sim_render_snapshot
{
    sim_render_snapshot(); // 0x007CC8A0 (.h:31)
    void clear(); // 0x007C19B0 (:306)

    game_state state_stack[16]; // 0x00
    int stack_top_index; // 0x40
    bool in_gameplay; // 0x44
};

extern sim_render_snapshot Sim_snapshot; // 0x0165E870
extern sim_render_snapshot Render_snapshot; // 0x0165E828
extern volatile bool In_handshake; // 0x0253387F

void game_handshake_do_middle_of_frame(rl_renderer& renderer); // 0x007EE4A0 (:86)
int game_handshake_do_frame(rl_renderer& renderer); // 0x007EE610 (:203), returns 0
void handshake_deferred_render(sim_render_snapshot& de_snapshot, rl_renderer& renderer); // 0x007CD6A0 (:240)
// True from the start of the handshake until the frame counter advances (Wait_for_prev_done_frame).
bool game_handshake_in_handshake(); // 0x007CD890 (:312)
