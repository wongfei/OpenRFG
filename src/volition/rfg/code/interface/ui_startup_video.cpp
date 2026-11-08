// Original: volition/rfg/code/interface/ui_startup_video.cpp
//
// The start-up videos (stub_impl.md S7): startup_video_sequence.xtbl's "Default" list played one after the other before
// the boot screen, centered with the interface's ratio; a video flagged skip_on_user_input ends on a button (or an
// invite). rfg_init_stage_1_loop_update runs them until Exit_startup_videos.

#include "volition/rfg/code/interface/ui_startup_video.h"

#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_project.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/util/special_allocators.h"
#include "volition/rfg/code/video_player/video_player.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <string.h>

// ---- public globals
bool Startup_videos_initialized; // 0x02C081C2
bool Exit_startup_videos; // 0x02C081C3
// Application::processInputEvent sets it for a button of a pad no player holds (application.cpp).
bool Pressing_any_button_for_startup_videos; // 0x02C081E4
bool g_openrfgSkipStartupVideos = false;

// ---- file-static globals
static video_sequence_data Video_list; // elements at 0x016676D4
static int Bink_handle_startup; // 0x02C081DC
static int Cur_video; // 0x02C081E0
static bool Button_down; // 0x02C081E5
static mempool_base* Bink_pool_startup; // 0x02C081E8

// 0x008CCB30
video_sequence_data::video_sequence_data()
{
}

// 0x008CCB50
video_sequence_data::~video_sequence_data()
{
}

// 0x008CCB60 (:77)
// The element's Video_List; false without any video. Filename and Video_Flags of each, as many as fit.
bool ui_startup_video_parse_sequence(xml_element* xml_root, video_sequence_data& video_sequence)
{
    if (xml_root == nullptr)
    {
        return false;
    }
    xml_element* list = xml_find(xml_root, "Video_List");
    if (list == nullptr)
    {
        return false;
    }
    xml_element* video = xml_find(list, "Video");
    if (video == nullptr)
    {
        return false;
    }
    while (video != nullptr)
    {
        xml_element* file = xml_find(video, "Video_File");
        if (file == nullptr)
        {
            return true;
        }
        xml_element* filename = xml_find(file, "Filename");
        if (filename == nullptr || filename->text == nullptr || video_sequence.videos.size() >= video_sequence.videos.capacity())
        {
            return true;
        }
        video_data data;
        strncpy(data.filename, filename->text, 0x40);
        data.flags = video_data::video_flags();
        xml_element* flags = xml_find(video, "Video_Flags");
        if (flags != nullptr)
        {
            for (xml_element* flag = xml_find(flags, "Flag"); flag != nullptr; flag = xml_find_next(flags, flag, "Flag"))
            {
                if (v_stricmp(flag->text, "skip_on_user_input") == 0)
                {
                    data.flags.skip_on_user_input = 1;
                }
            }
        }
        video_sequence.videos.add(data);
        video = xml_find_next(list, video, "Video");
    }
    return true;
}

// 0x008CCCC0 (:134)
void ui_startup_video_parse_table()
{
    xml_element* root = xml_table_parse("startup_video_sequence.xtbl");
    for (xml_element* entry = xml_find(root, "Startup_Video"); entry != nullptr; entry = xml_find_next(root, entry, "Startup_Video"))
    {
        xml_element* name = xml_find(entry, "Name");
        if (name != nullptr && v_stricmp(name->text, "Default") == 0)
        {
            if (!ui_startup_video_parse_sequence(entry, Video_list)) // inlined
            {
                VLIB_CRITICAL_FAILURE();
            }
        }
    }
    xml_table_close(root);
}

// 0x008A1CA0 (:165)
static void ui_startup_video_stop_video_cb(int bink_handle)
{
    Bink_handle_startup = 0;
    Bink_pool_startup->clear();
    Cur_video++;
}

// 0x008B98D0 (:175)
// The screen less the interface's centering offsets.
void ui_build_video_rect(rect_i& video_rect)
{
    const rl_screen_info* screen = rl_render_lib::get_instance()->get_screen_info();
    const int height = screen->get_screen_height();
    const int width = screen->get_screen_width();
    ui_update_resolution(width, height);
    // Original: the scale is asked for and not used.
    vint_project_get_scale(VINT_RES_SCALE_MAINTAIN_RATIO, VINT_RES_SCALE_MAINTAIN_RATIO);
    const vector2 offset = vint_project_get_offset(VINT_RES_OFFSET_CENTER, VINT_RES_OFFSET_CENTER);
    video_rect.nw.x = (int)offset.x;
    video_rect.nw.y = (int)offset.y;
    video_rect.se.x = (int)((float)width - offset.x);
    video_rect.se.y = (int)((float)height - offset.y);
}

// 0x008B9970 (:192)
void ui_startup_video_play_video(const char* filename)
{
    rect_i rect;
    ui_build_video_rect(rect);
    // Magic number: volume 0.6, which video_player_play does not read.
    Bink_handle_startup = video_player_play(filename, VPM_ONCE, rect.nw.x, rect.nw.y, rect.se.x - rect.nw.x, rect.se.y - rect.nw.y, 0.6f, true, ui_startup_video_stop_video_cb, Bink_pool_startup, Bink_pool_startup, Bink_pool_startup, 0, false, true);
    if (Bink_handle_startup == 0)
    {
        Bink_pool_startup->clear();
        Cur_video++;
        VLIB_CRITICAL_FAILURE();
    }
}

// 0x008A1CC0 (:220)
static bool startup_any_buttons_pressed()
{
    if (Pressing_any_button_for_startup_videos)
    {
        Pressing_any_button_for_startup_videos = false;
        return true;
    }
    // Magic numbers: the pressed-button results of controls_is_any_button_pressed that count.
    const unsigned int button = controls_is_any_button_pressed();
    return button == 0x40000010 || button == 0x40000012 || button == 0x20000000 || button == 0x20000002;
}

// 0x008CCEB0 (:251)
bool ui_startup_video_init()
{
    Bink_handle_startup = 0;
    Cur_video = 0;
    Exit_startup_videos = false;
    // Magic number: 12 MB, the videos' memory.
    Bink_pool_startup = (mempool_base*)salloc_pregame_split_lock(0xC00000, 0);
    if (Bink_pool_startup == nullptr)
    {
        return false;
    }
    // Deviation (UI.md deviation 4): OpenRFG's command line skips the table, as an empty list would (the pool stays
    // locked for ui_startup_video_cleanup).
    if (!g_openrfgSkipStartupVideos)
    {
        ui_startup_video_parse_table();
    }
    if (Video_list.videos.size() != 0)
    {
        Startup_videos_initialized = true;
        return true;
    }
    Startup_videos_initialized = false;
    return false;
}

// 0x008B9A10 (:273)
void ui_startup_video_process()
{
    controls_poll();
    bool just_pressed = false;
    if (startup_any_buttons_pressed()) // inlined
    {
        just_pressed = !Button_down;
        Button_down = true;
    }
    else
    {
        Button_down = false;
    }
    if (Bink_handle_startup == 0)
    {
        if (Cur_video < Video_list.videos.size())
        {
            ui_startup_video_play_video(Video_list.videos[Cur_video].filename);
        }
        else
        {
            Exit_startup_videos = true;
        }
    }
    else
    {
        if ((just_pressed && Video_list.videos[Cur_video].flags.skip_on_user_input != 0) || (multi_core_platform_has_pending_invite() && Video_list.videos[Cur_video].flags.skip_on_user_input != 0))
        {
            video_player_stop(Bink_handle_startup);
        }
        // Magic number: a video ends 3 frames before its last.
        if (video_player_get_total_frames(Bink_handle_startup) - video_player_get_current_frame(Bink_handle_startup) < 3)
        {
            video_player_stop(Bink_handle_startup);
        }
    }
    if (Game_boot_exit_flag)
    {
        Exit_startup_videos = true;
    }
    video_player_process_all();
}

// 0x008B9B30 (:332)
void ui_startup_video_render(rl_renderer& renderer)
{
    video_player_update_render_all(renderer);
    if (Bink_handle_startup == 0)
    {
        return;
    }
    gr_set_color(0xFF, 0xFF, 0xFF, 0xFF);
    renderer.clear(rl_color::BLACK, 0xFFFFFFFF, 1.0f, 0);
    rect_i rect;
    ui_build_video_rect(rect);
    video_player_resize(Bink_handle_startup, rect.nw.x, rect.nw.y, rect.se.x - rect.nw.x, rect.se.y - rect.nw.y);
    video_player_render(Bink_handle_startup);
}

// 0x008A1D00 (:351)
void ui_startup_video_cleanup()
{
    if (Bink_handle_startup != 0)
    {
        video_player_stop(Bink_handle_startup);
    }
    Bink_pool_startup->clear();
    salloc_pregame_split_unlock((rfg_split_mempool*)Bink_pool_startup);
}
