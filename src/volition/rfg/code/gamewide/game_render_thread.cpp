// Original: volition/rfg/code/gamewide/game_render_thread.cpp
//
// The render thread's side of the game. In this build there is no render thread of its own: game_render_thread_init
// runs on the main (render) thread at the end of the boot (rfg_init_stage_2_done_2) and registers it as the
// scheduler's d3d thread, with its thread scratch pool, and the simulation's trigger of the control block (Rtcb) gets
// one input that only the shutdown fires.
//
// rfg_gamewide_unity.obj is a unity build: this file writes gameseq.cpp's Wait_for_prev_done_frame, and gamewide.cpp
// reads Rt_init (gamewide_shutdown inlines game_render_thread_shutdown).

#include "volition/rfg/code/gamewide/game_render_thread.h"

#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/scheduler.h"

render_thread_control_block Rtcb; // 0x02535838
static bool Rt_init; // 0x025338DC
static mempool Render_thread_scratch_pool; // 0x02534530

// 0x007C3430
render_thread_control_block::render_thread_control_block()
    : handshake_event(false, false), render_thread_done_event(false, false), render_thread_prev_done_event(false, false)
{
}

// 0x007C3460
render_thread_control_block::~render_thread_control_block()
{
}

// 0x007D0C90 (:261)
// Magic number: 320 KB of thread scratch, from RFG_scratch_pool.
void game_render_thread_init()
{
    if (Rt_init)
    {
        return;
    }
    Rt_init = true;
    Rtcb.exit_thread = false;
    Rtcb.has_exited = false;
    Rtcb.sim_done_trigger = nullptr;
    Rtcb.has_initialized = false;
    RFG_scratch_pool.set_thread_ownership();
    void* mem = RFG_scratch_pool.alloc(0x50000, 0);
    Render_thread_scratch_pool.init(mem, 0x50000, "Render Scratch Pool", 4, 0);
    memory_scratch_thread_init(&Render_thread_scratch_pool);
    scheduler_register_d3d_thread();
    Rtcb.sim_done_trigger = scheduler_get_event_trigger();
    Rtcb.sim_done_trigger->attach_input();
    Rtcb.render_thread_prev_done_event.signal();
}

// 0x007D0D30 (:306)
void game_render_thread_shutdown()
{
    if (Rt_init)
    {
        Rt_init = false;
        Rtcb.exit_thread = true;
        Rtcb.sim_done_trigger->fire();
    }
}

// 0x007C3480 (:329)
void game_render_thread_fire_trigger()
{
    Rtcb.sim_done_trigger->fire();
}

// 0x007C3490 (:339)
bool game_render_thread_handshake_done()
{
    Rtcb.handshake_event.wait();
    return true;
}

// 0x007C34A0 (:346)
// Notes (decompiled behaviour, kept as is): nothing is waited for; the frame of the request is recorded
// (game_handshake_in_handshake).
void game_render_thread_wait_for_prev_done(bool anythread)
{
    Wait_for_prev_done_frame = Framecount_increment_always;
}

// 0x007C34B0 (:353)
void game_render_thread_prev_done_set()
{
    Rtcb.render_thread_prev_done_event.signal();
}

// 0x007C34C0 (:361)
void game_render_thread_done_set()
{
    Rtcb.render_thread_done_event.signal();
}

// 0x007C34D0 (:369)
void game_render_thread_wait_for_done()
{
    if (!scheduler_is_pre_threading())
    {
        scheduler_relinquish_cpu_until_event(Rtcb.render_thread_done_event, ccjt_any);
    }
}

// 0x007C34F0 (:393)
event_trigger* game_render_thread_get_trigger()
{
    return Rtcb.sim_done_trigger;
}

// 0x007D0D60 (:398)
void game_render_thread_wait_for_batching_done()
{
    while (!Rtcb.batching_done)
    {
    }
}

// 0x007C3500 (:404)
bool game_render_thread_batching_done()
{
    return Rtcb.batching_done;
}

// 0x007C3510 (:409)
void game_render_thread_batching_done_set()
{
    Rtcb.batching_done = true;
}

// 0x007C3520 (:414)
void game_render_thread_batching_done_reset()
{
    Rtcb.batching_done = false;
}

// 0x007C3530 (:419)
bool game_render_thread_in_handshake()
{
    return Wait_for_prev_done_frame == Framecount_increment_always;
}
