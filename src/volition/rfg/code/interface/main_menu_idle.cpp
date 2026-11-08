// Original: volition/rfg/code/interface/main_menu_idle.cpp
//
// The main menu's idle state (stub_impl.md S7): the attract video, until any button. The retail data has no
// RFG_Attract.bik, so the video does not open and the state goes back to the main menu at once.

#include "volition/rfg/code/interface/main_menu_idle.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/util/special_allocators.h"
#include "volition/rfg/code/video_player/video_player.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

// vector2i Screen_size (0x0166716C) is used here too; placed in rfg_lua_actions.cpp.
extern vector2i Screen_size; // 0x0166716C

// ---- file-static globals
static int Input_id_idle; // 0x016631E0
static timestamp Volume_fade_timer; // 0x01666270
static bool Paused; // 0x02C0357A, never set in rfg.exe
static unsigned int Control_id_idle; // 0x02C0357C
static int Bink_handle; // 0x02C03580
static mempool_base* Bink_pool; // 0x02C03584

// 0x00889A00 (:47)
static void mmi_stop_callback(int bink_handle)
{
    Bink_handle = 0;
    Bink_pool->clear();
    salloc_pregame_split_unlock((rfg_split_mempool*)Bink_pool);
}

// 0x009171C0 (:62)
// rfg.exe inlines this into main_menu_init (main_menu.cpp), the only copy that runs.
void main_menu_idle_init()
{
    state_info info;
    info.enter = main_menu_idle_enter;
    info.exit = main_menu_idle_exit;
    info.process = main_menu_idle_process;
    info.handshake = main_menu_idle_handshake;
    info.render = main_menu_idle_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_MAINMENU_IDLE, info);
}

// 0x008CF5E0 (:77)
// The video starts silent and comes up over 1.5 s while the front end music stops.
void main_menu_idle_enter(game_state prev_state)
{
    // Magic number: 12 MB, the video's memory.
    Bink_pool = (mempool_base*)salloc_pregame_split_lock(0xC00000, 0);
    Bink_handle = video_player_play("RFG_Attract.bik", VPM_ONCE, 0, 0, Screen_size.x, Screen_size.y, 0.0f, true, mmi_stop_callback, Bink_pool, Bink_pool, Bink_pool, 0, false, false);
    if (Bink_handle == 0)
    {
        Control_id_idle = 0;
        Bink_handle = 0;
        Bink_pool->clear();
        salloc_pregame_split_unlock((rfg_split_mempool*)Bink_pool);
    }
    else
    {
        Control_id_idle = ui_request_control_id();
        Input_id_idle = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false); // inlined
    }
    audiolib_all_sounds_pause(false, true);
    gamemusic_frontend_stop(1000);
    // Magic number: 1.5 s.
    Volume_fade_timer.set(1500);
}

// 0x008BF9B0 (:112)
void main_menu_idle_exit(game_state new_state)
{
    if (Bink_handle != 0)
    {
        video_player_stop(Bink_handle);
    }
    if (Control_id_idle != 0)
    {
        ui_relinquish_control_id(Control_id_idle);
        Control_id_idle = 0;
        ui_relinquish_input_id(Input_id_idle);
        // The button that ended the video does not act on the menu.
        for (int i = 0; i < 33; i++)
        {
            Button_info.binfo[i].m_button_wait_for_up = true;
        }
        Choice_last = -1;
    }
    audiolib_all_sounds_unpause(true, true);
    gamemusic_frontend_start(false);
}

// 0x00914580 (:134)
void main_menu_idle_process()
{
    if (Bink_handle != 0)
    {
        if (Paused)
        {
            video_player_unpause(Bink_handle);
            Paused = false;
        }
        if (Volume_fade_timer.valid())
        {
            if (Volume_fade_timer.elapsed())
            {
                // Magic number: the attract video's volume.
                video_player_set_volume(Bink_handle, 0.6f);
                Volume_fade_timer.invalidate();
            }
            else
            {
                // 0.6f widened to double, in double as in rfg.exe.
                video_player_set_volume(Bink_handle, (float)((double)(float)(1500 - Volume_fade_timer.time_until()) * (double)0.6f / 1500.0));
            }
        }
    }
    if (Button_info.m_button_pressed || multi_core_platform_has_pending_invite() || mouse_just_pressed(0))
    {
        if (Bink_handle != 0)
        {
            video_player_stop(Bink_handle);
        }
    }
    else if (Bink_handle != 0)
    {
        ui_process(Control_id_idle, true);
        return;
    }
    gameseq_set_state(GS_MAINMENU, false);
}

// 0x00889A30 (:169)
void main_menu_idle_handshake(rl_renderer*)
{
}

// 0x00889A40 (:174)
void main_menu_idle_render(rl_renderer*)
{
    if (Bink_handle != 0)
    {
        video_player_render(Bink_handle);
    }
}
