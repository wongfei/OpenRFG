// Original: volition/rfg/code/gamewide/game_render_thread.h
// Implementation: volition/rfg/code/gamewide/game_render_thread.cpp
//
// The render thread side of the game: deferred work queued for the render thread and its synchronization with the
// simulation thread.
#pragma once


#include "volition/vlib/code/os/sync_event.h"

class event_trigger;

// The render thread's handshake with the simulation (Rtcb).
// size 0x20
struct render_thread_control_block
{
    render_thread_control_block(); // 0x007C3430
    ~render_thread_control_block(); // 0x007C3460

    volatile bool has_initialized; // 0x00
    volatile bool exit_thread; // 0x01
    volatile bool has_exited; // 0x02
    event_trigger* sim_done_trigger; // 0x04
    sync_event handshake_event; // 0x08
    volatile bool render_thread_prev_done; // 0x0C
    sync_event render_thread_done_event; // 0x10
    volatile bool render_thread_done; // 0x14
    sync_event render_thread_prev_done_event; // 0x18
    volatile bool batching_done; // 0x1C
};

extern render_thread_control_block Rtcb; // 0x02535838

void game_render_thread_init(); // 0x007D0C90 (:261)
void game_render_thread_shutdown(); // 0x007D0D30 (:306)
void game_render_thread_fire_trigger(); // 0x007C3480 (:329)
bool game_render_thread_handshake_done(); // 0x007C3490 (:339)
void game_render_thread_wait_for_prev_done(bool anythread); // 0x007C34A0 (:346)
void game_render_thread_prev_done_set(); // 0x007C34B0 (:353)
void game_render_thread_done_set(); // 0x007C34C0 (:361)
void game_render_thread_wait_for_done(); // 0x007C34D0 (:369)
event_trigger* game_render_thread_get_trigger(); // 0x007C34F0 (:393)
void game_render_thread_wait_for_batching_done(); // 0x007D0D60 (:398)
bool game_render_thread_batching_done(); // 0x007C3500 (:404)
void game_render_thread_batching_done_set(); // 0x007C3510 (:409)
void game_render_thread_batching_done_reset(); // 0x007C3520 (:414)
bool game_render_thread_in_handshake(); // 0x007C3530 (:419)
