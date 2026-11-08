// Original: volition/rfg/code/gamewide/game_handshake.cpp
//
// The per-frame sync point between the simulation and the render thread (game_handshake.h), whole (multiplayer.md
// MP1). rfg_gamewide_unity.obj is a unity build: this file reads gameseq.cpp's State_stack, Stack_top_index and
// Wait_for_prev_done_frame (rfg_gamewide_unity.cpp includes gameseq.cpp first). s_groupIdGameplay is a header static of
// nvidia_highlights.h.
//
// The render calls (the renderer's handshake, the deferred camera and primitive rendering) are the render library's;
// the headless programs have stand-ins for them. keen::rfg::start_middle_of_frame / end_middle_of_frame are the
// application's thread handshake (application.hpp).

#include "volition/rfg/code/gamewide/game_handshake.h"

#include "game/sources/application.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/stub/audiolib_stub.h"
#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_draw.h"
#include "volition/render_lib/code/rl_mesh_recompile.h"
#include "volition/render_lib/code/rl_primitive_renderer.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/render_lib/code/rl_state_manager.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/cutscene/cutscene.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/nvidia_highlights.h"
#include "volition/rfg/code/video_player/video_player.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/scheduler.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

sim_render_snapshot Sim_snapshot; // 0x0165E870
sim_render_snapshot Render_snapshot; // 0x0165E828
volatile bool In_handshake; // 0x0253387F

static rl_camera* Deferred_scene_camera; // 0x02533888, copy of the main render camera taken in the handshake
static bool Deferred_scene_camera_was_allocated; // 0x0253387E
static int Scene_ref_count; // 0x025338D4

static void game_handshake_get_render_snapshot_copy(sim_render_snapshot& the_copy); // 0x007C1990 (:80)

// 0x007CC8A0 (.h:31)
sim_render_snapshot::sim_render_snapshot()
{
    clear();
}

// 0x007C19B0 (:306)
void sim_render_snapshot::clear()
{
    stack_top_index = -1;
    in_gameplay = false;
}

// 0x007C1990 (:80)
static void game_handshake_get_render_snapshot_copy(sim_render_snapshot& the_copy)
{
    the_copy = Sim_snapshot;
}

// 0x007EE4A0 (:86)
void game_handshake_do_middle_of_frame(rl_renderer& renderer)
{
    Wait_for_prev_done_frame = Framecount_increment_always;
    In_handshake = true;
    rl_mesh_recompile_pause();
    rl_camera* main_camera = camera_get_main_render_camera();
    renderer.handshake_begin();
    audiolib_system_init_info audio_init_info;
    gameaudio_get_system_init_params(audio_init_info);
    audiolib_check_panic(&audio_init_info);
    if (Sim_snapshot.in_gameplay)
    {
        // The render lib stays unlocked while the world may change.
        if (rl_is_locked())
        {
            rl_unlock();
        }
        Scene_ref_count++;
    }
    console_do_frame_process(false);
    video_player_update_render_all(renderer);
    ui_handshake();
    cutscene_do_handshake();
    multi_handshake();
    gameseq_process(GS_STEP_HANDSHAKE, &renderer, false);
    const game_state top = Stack_top_index < 0 ? GS_NONE : State_stack[Stack_top_index];
    // A state with user flag bit 0 streams by itself (the loading states).
    if ((State_info[top].user_flags & 1) == 0 && !localization_is_audio_loading())
    {
        stream2_do_frame(true);
    }
    if (Sim_snapshot.in_gameplay)
    {
        Scene_ref_count--;
        if (Scene_ref_count == 0 && !rl_is_locked())
        {
            rl_lock();
        }
    }
    Render_snapshot = Sim_snapshot;
    if (Deferred_scene_camera != nullptr && Deferred_scene_camera_was_allocated)
    {
        Deferred_scene_camera->release();
        Deferred_scene_camera = nullptr;
        Deferred_scene_camera_was_allocated = false;
    }
    // Deviation: the headless programs have no render camera (camera_get_main_render_camera returns null without the
    // renderer); there is then no deferred camera and handshake_deferred_render draws nothing. rfg.exe always has one.
    if (main_camera != nullptr)
    {
        Deferred_scene_camera = rl_camera::create();
        Deferred_scene_camera_was_allocated = true;
        *Deferred_scene_camera = *main_camera;
    }
    rl_primitive_renderer::get_instance()->set_mode(RLPRM_DEFERRED);
    rl_primitive_renderer::get_instance()->begin_deferred();
    In_handshake = false;
    rl_mesh_recompile_unpause();
}

// 0x007EE610 (:203)
int game_handshake_do_frame(rl_renderer& renderer)
{
    // The multiplayer send job of this frame may start now (multi_io_send_process_job), and the sim thread helps with it.
    MP_send_setup_cs.enter();
    if (MP_send_scheduling_trigger != nullptr)
    {
        MP_send_scheduling_trigger->activate();
        MP_send_scheduling_trigger = nullptr;
    }
    if (MP_send_process_trigger != nullptr)
    {
        scheduler_relinquish_cpu(MP_send_process_trigger, ccjt_any);
        MP_send_process_trigger = nullptr;
    }
    MP_reached_render_trigger_point = false;
    MP_send_setup_cs.leave();
    keen::rfg::start_middle_of_frame();
    game_handshake_do_middle_of_frame(renderer);
    handshake_deferred_render(Sim_snapshot, renderer);
    keen::rfg::end_middle_of_frame();
    Sim_snapshot.in_gameplay = false;
    Sim_snapshot.stack_top_index = -1;
    return 0;
}

// 0x007CD6A0 (:240)
// Notes (decompiled behaviour, kept as is): de_snapshot is not read; the render steps use Render_snapshot.
void handshake_deferred_render(sim_render_snapshot& de_snapshot, rl_renderer& renderer)
{
    if (Deferred_scene_camera == nullptr)
    {
        return;
    }
    vector render_offset;
    Deferred_scene_camera->get_scene()->get_render_offset(render_offset);
    renderer.set_render_offset(render_offset);
    Deferred_scene_camera->render_begin(renderer);
    rl_draw::get_instance()->get_rl_state().copy(renderer.get_full_state());
    rl_draw::get_instance()->get_rl_state().set_3d_transforms(Deferred_scene_camera->get_view_transform(), Deferred_scene_camera->get_projection_transform());
    if (Stack_top_index < 0 || State_stack[Stack_top_index] != GS_GAMEPLAY)
    {
        nvidia_highlights_close_group(s_groupIdGameplay, false);
    }
    else
    {
        nvidia_highlights_open_group(s_groupIdGameplay);
    }
    // The render step of the top state (and the transparent states beneath it) of the snapshot: gameseq_process's
    // render branch, inlined.
    game_state state = Render_snapshot.state_stack[Render_snapshot.stack_top_index];
    if (State_info[state].transparent)
    {
        if (Render_snapshot.stack_top_index < 1)
        {
            VLIB_CRITICAL_FAILURE();
        }
        else
        {
            gameseq_do_state(Render_snapshot.stack_top_index - 1, State_info[state].pause_beneath, GS_STEP_RENDER, false, &renderer);
        }
    }
    if (State_info[state].render != nullptr)
    {
        State_info[state].render(&renderer);
    }
    const game_state top = Stack_top_index < 0 ? GS_NONE : State_stack[Stack_top_index];
    AUDIOLIB_GAME_STATE audio_game_state = AUDIOLIB_GAME_STATE_IN_LEVEL;
    switch (top)
    {
    case GS_LOAD:
    case GS_BOOT:
    case GS_CREDITS:
        audio_game_state = AUDIOLIB_GAME_STATE_LOADING;
        break;
    case GS_MULTI_INIT:
    case GS_MULTI_SHUTDOWN:
        audio_game_state = AUDIOLIB_GAME_STATE_UNITIALIZED;
        break;
    default:
        break;
    }
    if (hud_ui_get_loaded_module() == HUD_MODULE_SATELLITE && top != GS_INGAME_HANDBOOK_HUD)
    {
        audio_game_state = AUDIOLIB_GAME_STATE_SATELLITE;
    }
    audiolib_system_set_game_state(audio_game_state);
    video_player_render_all();
    console_do_frame_render();
    multi_render();
    Deferred_scene_camera->render_end(renderer);
    rl_draw::get_instance()->get_rl_state().set_active_camera(nullptr);
}

// 0x007CD890 (:312)
bool game_handshake_in_handshake()
{
    return Wait_for_prev_done_frame == Framecount_increment_always;
}
