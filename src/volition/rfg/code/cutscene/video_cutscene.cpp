// Original: volition/rfg/code/cutscene/video_cutscene.cpp
//
// Full screen Bink movies as a game state (stub_impl.md S9/S10): video_cutscene_play takes a movie and what comes after
// it (warp to a zone or an object, a DLC level, the credits, or back to the previous state); the state locks the
// gameplay GPU pool for the video, loads the movie's subtitle table and any personas it needs, plays until the end or a
// skip button, and the stop callback runs the follow-up. The main menu's new game starts here.

#include "volition/rfg/code/cutscene/video_cutscene.h"

#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_project.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/rfg/code/gamewide/game_trace.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/hud/hud_ui_handbook.h"
#include "volition/rfg/code/interface/credits.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/util/special_allocators.h"
#include "volition/rfg/code/video_player/video_player.h"
#include "volition/stream2/code/stream2.h"
#include "volition/stream2/code/stream2_os.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/memory/page_pool.h"
#include "volition/vlib/code/memory/split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/varray.h"
#include "volition/vlib/code/util/fastxml.h"

#include <string.h>

// ---- public globals
float Video_cutscene_volume = 1.0f; // 0x0164BE94

// ---- file-static globals
struct video_subtitle
{
    unsigned int start_offset; // 0x00, milliseconds
    int duration; // 0x04
    unsigned int subtitle_crc; // 0x08
    char persona[64]; // 0x0C
};

static bool Waiting_for_fade; // 0x0164BD1B
static int Video_cutscene_input_id = -1; // 0x0164BE88
static unsigned int Video_cutscene_warp_to_object = 0xFFFFFFFF; // 0x0164BE8C
static unsigned int Video_cutscene_subtitle_req = 0xFFFFFFFF; // 0x0164BE90
static farray<video_subtitle, 200> subtitles; // 0x0164BEA8
static farray<human_voice_persona*, 150> Required_personas; // 0x0164FA18
static float Milliseconds_per_frame; // 0x01E2B3FC
static char Video_cutscene_filename[64]; // 0x01E2B400
static bool Video_cutscene_using_external_mempool; // 0x01E2B45F
static int Video_cutscene_bink_handle; // 0x01E2B470
static int Video_cutscene_control_id; // 0x01E2B474
static unsigned int Video_cutscene_cpu_mark; // 0x01E2B478
static unsigned int Video_cutscene_gpu_mark; // 0x01E2B47C
static mempool_base* Video_cutscene_gpu_mempool; // 0x01E2B480
static mempool_base* Video_cutscene_cpu_mempool; // 0x01E2B484
static dlc_level_info* Video_cutscene_dlc_level; // 0x01E2B488
static bool Video_cutscene_ready_to_start; // 0x01E2B48C
static bool Video_cutscene_saving; // 0x01E2B48D
static bool Video_cutscene_exiting; // 0x01E2B48E
static bool Video_cutscene_about_to_exit; // 0x01E2B48F
static const char* Video_cutscene_warp_to_zone; // 0x01E2B490
static unsigned int Video_Start_Time; // 0x01E2B494
static int subtitle_index; // 0x01E2B498
static bool Video_cutscene_no_state_change; // 0x01E2B49C
static bool Video_cutscene_sound_pause; // 0x01E2B49D
static bool Video_cutscene_music_pause; // 0x01E2B49E
static bool Vibe_setting; // 0x01E2B49F
static bool Video_cutscene_is_paused; // 0x01E2B4A0
static bool Video_cutscene_set_state; // 0x01E2B4A1
static bool Roll_credits; // 0x01E2B4A2
static unsigned char* Video_cutscene_subtitles_buffer; // 0x01E2B4A4
static virtual_mempool* Video_cutscene_subtitle_mempool; // 0x01E2B4A8

// 0x00706E60 (:116)
static void video_cutscene_free_memory()
{
    // 0x40: the pool is a split_mempool.
    if (Video_cutscene_gpu_mempool != nullptr && (Video_cutscene_gpu_mempool->get_flags() & 0x40) != 0)
    {
        ((split_mempool*)Video_cutscene_gpu_mempool)->restore_to_mark_gpu(Video_cutscene_gpu_mark);
    }
    if (Video_cutscene_cpu_mempool != nullptr)
    {
        if ((Video_cutscene_cpu_mempool->get_flags() & 0x40) == 0)
        {
            Video_cutscene_cpu_mempool->restore_to_mark(Video_cutscene_cpu_mark);
        }
        else
        {
            ((split_mempool*)Video_cutscene_cpu_mempool)->restore_to_mark_cpu(Video_cutscene_cpu_mark);
        }
        Video_cutscene_cpu_mark = 0;
        Video_cutscene_gpu_mark = 0;
    }
    if (!Video_cutscene_using_external_mempool)
    {
        salloc_gameplay_gpu_unlock_async();
        return;
    }
    Video_cutscene_gpu_mempool = nullptr;
    Video_cutscene_cpu_mempool = nullptr;
}

// 0x00706ED0 (:152)
static void video_cutscene_enter_state()
{
    game_render_request_vsync();
    Vibe_setting = input_get_vibration_unwanted();
    input_set_vibration_unwanted(false);
    audiolib_all_sounds_get_pause(&Video_cutscene_sound_pause, &Video_cutscene_music_pause);
    audiolib_all_sounds_pause(true, true);
    // Magic number: half a second.
    gamemusic_frontend_stop(500);
    if (!Video_cutscene_set_state)
    {
        gameseq_push_state(GS_VIDEO_CUTSCENE_PLAY, false, false);
        return;
    }
    gameseq_set_state(GS_VIDEO_CUTSCENE_PLAY, false);
}

// 0x00706F30 (:176)
static void video_cutscene_exit_state()
{
    Video_cutscene_exiting = true;
    game_render_release_vsync();
    input_set_vibration_unwanted(Vibe_setting);
    ui_kill_subtitle();
    video_cutscene_free_memory();
    stream2_q_unpause();
    if (Video_cutscene_no_state_change)
    {
        Video_cutscene_no_state_change = false;
        return;
    }
    if (Roll_credits)
    {
        Roll_credits = false;
        gameseq_pop_state();
        credits_show(true, Video_cutscene_warp_to_object);
        return;
    }
    if (Video_cutscene_warp_to_object != 0xFFFFFFFF)
    {
        if (!World.set_pending_territory_load("", false, false, false, World.dlc_bundle_id, false))
        {
            gameseq_set_state(GS_MAINMENU, true);
        }
        return;
    }
    if (Video_cutscene_warp_to_zone != nullptr)
    {
        if (!World.set_pending_territory_load(Video_cutscene_warp_to_zone, true, false, false, World.dlc_bundle_id, false))
        {
            gameseq_set_state(GS_MAINMENU, true);
        }
        return;
    }
    if (Video_cutscene_dlc_level != nullptr)
    {
        dlc_load_level(Video_cutscene_dlc_level);
        return;
    }
    ui_fade_to_black_fade_in(false);
    audiolib_all_sounds_unpause(!Video_cutscene_sound_pause, !Video_cutscene_music_pause);
    game_unpause();
    if (!Video_cutscene_set_state)
    {
        gameseq_pop_state();
        return;
    }
    gameseq_set_state(GS_MAINMENU, false);
}

// 0x00707070 (:246)
static void video_cutscene_stop_callback(int bink_handle)
{
    GAME_TRACE("cutscene", "movie stops: handle=%d", bink_handle);
    video_cutscene_exit_state();
    Video_cutscene_bink_handle = 0;
}

// 0x00707080 (:252)
static void video_cutscene_play_enter(game_state prev_state)
{
    Video_cutscene_control_id = ui_request_control_id();
    Video_cutscene_input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    ui_wait_for_button_ups();
    hud_hide(true);
    game_pause(true);
    Waiting_for_fade = true;
    Video_cutscene_about_to_exit = false;
}

// 0x007070C0 (:264)
static bool video_cutscene_alloc_memory()
{
    if (!Video_cutscene_using_external_mempool)
    {
        Video_cutscene_gpu_mempool = salloc_gameplay_gpu_lock();
    }
    if (Video_cutscene_gpu_mempool == nullptr)
    {
        return false;
    }
    if ((Video_cutscene_gpu_mempool->get_flags() & 0x40) == 0)
    {
        Video_cutscene_gpu_mark = 0;
    }
    else
    {
        Video_cutscene_gpu_mark = ((split_mempool*)Video_cutscene_gpu_mempool)->mark_gpu();
    }
    if (Video_cutscene_using_external_mempool)
    {
        Video_cutscene_cpu_mark = Video_cutscene_cpu_mempool->mark();
        return true;
    }
    Video_cutscene_cpu_mempool = Video_cutscene_gpu_mempool;
    Video_cutscene_cpu_mark = Video_cutscene_gpu_mempool->mark();
    return true;
}

// 0x00709F40 (:298)
static void video_cutscene_play_exit(game_state new_state)
{
    if (Video_cutscene_control_id != 0)
    {
        ui_relinquish_control_id(Video_cutscene_control_id);
        Video_cutscene_control_id = 0;
        ui_relinquish_input_id(Video_cutscene_input_id);
        Video_cutscene_input_id = -1;
        ui_wait_for_button_ups();
    }
    for (int i = 0; i < Required_personas.size(); i++)
    {
        human_voice_cutscene_release_voice(Required_personas[i]);
    }
    Required_personas.clear();
    subtitle_index = 0;
    subtitles.clear();
    ui_kill_subtitle();
    Video_cutscene_warp_to_object = 0xFFFFFFFF;
    hud_hide(false);
    if (ui_loading_screen_is_open())
    {
        ui_remove_loading_screen();
    }
    Video_cutscene_is_paused = false;
}

// 0x00707140 (:330)
// Waits for the GPU pool's asynchronous unlock before the state may leave.
bool video_cutscene_play_exit_notify(game_state new_state)
{
    if (new_state == GS_SHUTDOWN)
    {
        return true;
    }
    if (Video_cutscene_bink_handle != 0)
    {
        Video_cutscene_no_state_change = true;
        video_player_stop(Video_cutscene_bink_handle);
        ui_show_loading_screen(false, "MENU_LOADING");
        ui_fade_to_black_instant_fade_out();
        Video_cutscene_bink_handle = 0;
    }
    stream2_q_unpause();
    Video_cutscene_about_to_exit = true;
    if (!Video_cutscene_exiting)
    {
        return false;
    }
    if (Video_cutscene_gpu_mempool != nullptr && salloc_gameplay_gpu_unlock_async_poll())
    {
        Video_cutscene_gpu_mempool = nullptr;
    }
    if (Video_cutscene_cpu_mempool == nullptr)
    {
        return Video_cutscene_gpu_mempool == nullptr;
    }
    if (Video_cutscene_gpu_mempool == nullptr)
    {
        // Both are the locked pool when it is not external, so the CPU one goes with it.
        Video_cutscene_cpu_mempool = Video_cutscene_gpu_mempool;
        return true;
    }
    return false;
}

// 0x00709FE0 (:386)
static void video_cutscene_subtitle_process()
{
    if (!Subtitles_enabled || subtitles.size() < 1 || subtitle_index >= subtitles.size())
    {
        return;
    }
    video_subtitle* subtitle = &subtitles[subtitle_index];
    if (subtitle->subtitle_crc == 0)
    {
        subtitle_index = 0;
        subtitles.clear();
        return;
    }
    const int frame = video_player_get_current_frame(Video_cutscene_bink_handle);
    // x87: start_offset is unsigned (FILD plus the 2^32 fix-up) and the quotient truncates.
    if (frame > (int)((double)subtitle->start_offset / (double)Milliseconds_per_frame))
    {
        ui_add_subtitle(subtitle->subtitle_crc, subtitle->persona, subtitle->duration);
        subtitle_index++;
    }
}

// 0x0070A070 (:420)
static void video_cutscene_play_process()
{
    if (Video_cutscene_gpu_mempool == nullptr || Video_cutscene_exiting || Video_cutscene_about_to_exit)
    {
        ui_process(Video_cutscene_control_id, false);
        return;
    }
    if (Video_cutscene_cpu_mempool == nullptr)
    {
        return;
    }
    voice_slotting_do_frame();
    for (int i = 0; i < Required_personas.size(); i++)
    {
        if (!Required_personas[i]->is_loaded())
        {
            ui_process(Video_cutscene_control_id, false);
            return;
        }
    }
    if (Video_cutscene_subtitle_req != 0xFFFFFFFF || Video_cutscene_is_paused)
    {
        ui_process(Video_cutscene_control_id, false);
        return;
    }
    if (Waiting_for_fade)
    {
        if (!ui_fade_to_black_is_faded() && !ui_fade_to_black_is_fade_in_complete())
        {
            ui_process(Video_cutscene_control_id, false);
            return;
        }
        player* pp = player_get_local_player();
        if (pp != nullptr)
        {
            player_set_zoom_state(pp, PZS_OFF, true);
        }
        ui_fade_to_black_instant_fade_in();
        Waiting_for_fade = false;
    }
    stream2_q_pause();
    if (!stream2_q_busy())
    {
        const vector2 scale = vint_project_get_scale(VINT_RES_SCALE_MAINTAIN_RATIO, VINT_RES_SCALE_MAINTAIN_RATIO);
        const vector2 offset = vint_project_get_offset(VINT_RES_OFFSET_CENTER, VINT_RES_OFFSET_CENTER);
        // Magic numbers: the movies are laid out for 1280x720.
        if (Video_cutscene_bink_handle == 0)
        {
            Video_cutscene_bink_handle = video_player_play(Video_cutscene_filename, VPM_ONCE, (int)offset.x, (int)offset.y, (int)((double)scale.x * 1280.0), (int)((double)scale.y * 720.0), Video_cutscene_volume, true, video_cutscene_stop_callback, Video_cutscene_gpu_mempool, Video_cutscene_cpu_mempool, Video_cutscene_cpu_mempool, 0, true, false);
            int rate = video_player_get_frame_rate(Video_cutscene_bink_handle);
            if (rate < 2)
            {
                rate = 1;
            }
            Milliseconds_per_frame = 1000.0f / (float)rate;
            Video_Start_Time = timer_get(TIMER_MILLISECONDS);
            GAME_TRACE("cutscene", "movie starts: %s handle=%d frames=%d rate=%d", Video_cutscene_filename, Video_cutscene_bink_handle, Video_cutscene_bink_handle != 0 ? video_player_get_total_frames(Video_cutscene_bink_handle) : 0, rate);
        }
        else
        {
            video_player_resize(Video_cutscene_bink_handle, (int)offset.x, (int)offset.y, (int)((double)scale.x * 1280.0), (int)((double)scale.y * 720.0));
        }
        if (Video_cutscene_bink_handle != 0 && Video_cutscene_control_id != 0)
        {
            if (Video_cutscene_is_paused)
            {
                ui_process(Video_cutscene_control_id, false);
                return;
            }
            // Magic numbers: buttons 0xB and 9 skip (the accept and pause actions); the last two frames count as the end.
            if (ui_get_button_info(0xB, Video_cutscene_input_id) <= 0.0f && ui_get_button_info(9, Video_cutscene_input_id) <= 0.0f &&
                video_player_get_current_frame(Video_cutscene_bink_handle) + 2 <= video_player_get_total_frames(Video_cutscene_bink_handle))
            {
                ui_process(Video_cutscene_control_id, false);
                return;
            }
            video_player_stop(Video_cutscene_bink_handle);
            ui_show_loading_screen(false, "MENU_LOADING");
            stream2_q_unpause();
            Video_cutscene_bink_handle = 0;
            ui_fade_to_black_instant_fade_out();
            ui_process(Video_cutscene_control_id, false);
            Video_cutscene_about_to_exit = true;
            return;
        }
        stream2_q_unpause();
    }
    ui_process(Video_cutscene_control_id, false);
}

// 0x0070A390 (:531)
static void video_cutscene_start_load_subtitles()
{
    subtitles.clear();
    subtitle_index = 0;
    if (!Subtitles_enabled)
    {
        return;
    }
    char table_name[64];
    cf_add_ext(table_name, sizeof(table_name), Video_cutscene_filename, "xtbl", false);
    unsigned int size;
    if (!cf_exist(table_name) || !cf_get_file_size(&size, table_name, false))
    {
        return;
    }
    stream2_os_file request;
    strcpy(request.filename, table_name);
    request.offset = 0;
    request.size = size;
    // Magic number: 128 KB of subtitle table.
    Video_cutscene_subtitle_mempool = Common_page_pool.get_virtual_mempool("video_subtitle_pool", 0x20000);
    if (Video_cutscene_subtitle_mempool == nullptr)
    {
        return;
    }
    // Room for the parser's terminator.
    size += 4;
    Video_cutscene_subtitles_buffer = (unsigned char*)Video_cutscene_subtitle_mempool->alloc(size, 0);
    if (Video_cutscene_subtitles_buffer != nullptr)
    {
        request.dest = Video_cutscene_subtitles_buffer;
        memset(Video_cutscene_subtitles_buffer, 0, size);
        Video_cutscene_subtitle_req = stream2_os_read(request);
    }
}

// The pool's clear and release (inlined twice in video_cutscene_load_subtitles).
static void release_subtitle_mempool()
{
    Video_cutscene_subtitles_buffer = nullptr;
    if (Video_cutscene_subtitle_mempool != nullptr)
    {
        Video_cutscene_subtitle_mempool->clear();
        Common_page_pool.release_virtual_mempool(Video_cutscene_subtitle_mempool);
        Video_cutscene_subtitle_mempool = nullptr;
    }
}

// 0x0070FBA0 (:573)
static void video_cutscene_load_subtitles()
{
    if (!Subtitles_enabled || Video_cutscene_subtitle_req == 0xFFFFFFFF)
    {
        return;
    }
    const stream2_os_req_status status = stream2_os_poll(Video_cutscene_subtitle_req);
    if (status == SOS_BUSY)
    {
        return;
    }
    stream2_os_release_handle(Video_cutscene_subtitle_req);
    Video_cutscene_subtitle_req = 0xFFFFFFFF;
    if (status != SOS_DONE_SUCCESS)
    {
        release_subtitle_mempool();
        return;
    }
    if (Video_cutscene_subtitles_buffer == nullptr)
    {
        return;
    }
    xml_element* root = xml_table_parse_from_memory(Video_cutscene_subtitles_buffer);
    if (root == nullptr)
    {
        return;
    }
    for (xml_element* element = xml_find(root, "Subtitle"); element != nullptr; element = xml_find_next(root, element, "Subtitle"))
    {
        const char* key = xml_table_get_req_string_ref(element, "String");
        if (key == nullptr)
        {
            // Original: leaves the table open and the buffer in use.
            return;
        }
        const char* persona_name = xml_table_get_opt_string_ref(element, "Persona");
        human_voice_persona* persona = human_voice_persona_from_name(persona_name);
        if (persona != nullptr)
        {
            Required_personas.add_unique(persona);
        }
        int start;
        int duration;
        xml_table_get_req_int(start, element, "Start");
        xml_table_get_req_int(duration, element, "Duration");
        const unsigned int tag_hash = localization_generate_hash_from_key(key);
        if (localization_get_display_string_req(tag_hash) == nullptr)
        {
            continue;
        }
        if (subtitles.size() >= 200)
        {
            break;
        }
        video_subtitle subtitle;
        subtitle.start_offset = (unsigned int)start;
        subtitle.duration = duration;
        subtitle.subtitle_crc = tag_hash;
        if (persona_name == nullptr)
        {
            subtitle.persona[0] = '\0';
        }
        else
        {
            strncpy(subtitle.persona, persona_name, sizeof(subtitle.persona));
        }
        subtitles.add(subtitle);
    }
    xml_table_close(root);
    for (int i = 0; i < Required_personas.size(); i++)
    {
        human_voice_cutscene_reserve_slot(Required_personas[i]);
    }
    release_subtitle_mempool();
}

// 0x0070FDE0 (:679)
static void video_cutscene_play_handshake(rl_renderer* renderer)
{
    if (!Video_cutscene_exiting)
    {
        if (Video_cutscene_gpu_mempool == nullptr)
        {
            if (Video_cutscene_cpu_mempool != nullptr)
            {
                return;
            }
            if (!video_cutscene_alloc_memory())
            {
                video_cutscene_exit_state();
                return;
            }
            video_cutscene_start_load_subtitles();
            if (!Video_cutscene_exiting && Video_cutscene_gpu_mempool == nullptr)
            {
                return;
            }
        }
    }
    if (Video_cutscene_subtitle_req != 0xFFFFFFFF)
    {
        video_cutscene_load_subtitles();
    }
}

// 0x0070A4C0 (:698)
static void video_cutscene_play_render(rl_renderer* renderer)
{
    if (Video_cutscene_bink_handle != 0)
    {
        gr_set_color(0, 0, 0, 255);
        gr_set_texture(-1);
        const vector2i& screen = ui_get_screen_dimensions();
        gr_set_scissor_region(0, 0, screen.x, screen.y, 0);
        gr_rect(0, 0, screen.x, screen.y, GM_FILTER);
        video_player_render(Video_cutscene_bink_handle);
        video_cutscene_subtitle_process();
    }
    ui_render(Video_cutscene_control_id, true);
}

// The movie's name with its extension, the logo when it does not exist (shared by the three ways to start one).
static void set_video_filename(const char* filename)
{
    ui_kill_subtitle();
    cf_add_ext(Video_cutscene_filename, sizeof(Video_cutscene_filename), filename, ".bik_xbox2", false);
    if (!video_player_video_exists(Video_cutscene_filename, nullptr))
    {
        cf_add_ext(Video_cutscene_filename, sizeof(Video_cutscene_filename), "logo_thq_hd.bik", ".bik_xbox2", false);
    }
}

// The fields both unused_dcf_video_cutscene_play and video_cutscene_play_with_credits set the same way.
static void reset_video_request(unsigned int warp_to_object)
{
    Video_cutscene_warp_to_object = warp_to_object;
    Video_cutscene_warp_to_zone = nullptr;
    Video_cutscene_set_state = false;
    Video_cutscene_dlc_level = nullptr;
    Video_cutscene_using_external_mempool = false;
    Video_cutscene_gpu_mempool = nullptr;
    Video_cutscene_cpu_mempool = nullptr;
    Video_cutscene_ready_to_start = true;
    Video_cutscene_saving = false;
    Video_cutscene_exiting = false;
    char name[64];
    cf_drop_ext(name, sizeof(name), Video_cutscene_filename);
    hud_ui_handbook_unlock_entry(name);
}

// 0x0070DA90 (:718)
void unused_dcf_video_cutscene_play()
{
    console_get_arg(2, 0);
    ui_fade_to_black_fade_out(false);
    set_video_filename(Console_arg);
    reset_video_request(0xFFFFFFFF);
    Roll_credits = false;
}

// 0x0070FE30 (:733)
void video_cutscene_system_init()
{
    state_info info;
    info.enter = video_cutscene_play_enter;
    info.exit = video_cutscene_play_exit;
    info.exit_notify = video_cutscene_play_exit_notify;
    info.process = video_cutscene_play_process;
    info.handshake = video_cutscene_play_handshake;
    info.render = video_cutscene_play_render;
    info.transparent = false;
    info.pause_beneath = false;
    info.hide_cursor = true;
    gameseq_register_state(GS_VIDEO_CUTSCENE_PLAY, info);
}

// 0x0070A540 (:750)
void video_cutscene_play(const char* filename, unsigned int warp_to_object, const char* warp_to_zone, bool play_immediately, bool set_state, mempool_base* gpu_mempool, mempool_base* cpu_mempool, dlc_level_info* dlc_lvl)
{
    GAME_TRACE("cutscene", "play %s: warp_object=0x%X warp_zone=%s immediately=%d set_state=%d", filename, warp_to_object, warp_to_zone != nullptr ? warp_to_zone : "-", play_immediately ? 1 : 0, set_state ? 1 : 0);
    set_video_filename(filename);
    Video_cutscene_warp_to_object = warp_to_object;
    Video_cutscene_warp_to_zone = warp_to_zone;
    Video_cutscene_set_state = set_state;
    Video_cutscene_gpu_mempool = gpu_mempool;
    Video_cutscene_cpu_mempool = cpu_mempool;
    Video_cutscene_dlc_level = dlc_lvl;
    if (gpu_mempool == nullptr || cpu_mempool == nullptr)
    {
        Video_cutscene_using_external_mempool = false;
        Video_cutscene_gpu_mempool = nullptr;
        Video_cutscene_cpu_mempool = nullptr;
    }
    else
    {
        Video_cutscene_using_external_mempool = true;
        video_cutscene_alloc_memory();
        video_cutscene_start_load_subtitles();
    }
    Video_cutscene_ready_to_start = true;
    Video_cutscene_saving = false;
    Video_cutscene_exiting = false;
    char name[64];
    cf_drop_ext(name, sizeof(name), Video_cutscene_filename);
    hud_ui_handbook_unlock_entry(name);
    if (play_immediately)
    {
        // A state change with something to warp to, or a save in progress, waits for video_cutscene_do_frame.
        if (Video_cutscene_set_state && (Video_cutscene_warp_to_object != 0xFFFFFFFF || (Video_cutscene_warp_to_zone == nullptr && Video_cutscene_dlc_level == nullptr)))
        {
            return;
        }
        if (World.sl_flags.pending_save_game || World.sl_flags.performing_save_game)
        {
            return;
        }
        if (ui_is_msgbox_active(-2))
        {
            ui_hide_msgbox(-2, true);
        }
        video_cutscene_enter_state();
        Video_cutscene_ready_to_start = false;
        Video_cutscene_saving = false;
    }
    Roll_credits = false;
}

// 0x0070A6B0 (:837)
void video_cutscene_play_with_credits(const char* filename, unsigned int warp_to_object)
{
    set_video_filename(filename);
    reset_video_request(warp_to_object);
    Roll_credits = true;
}

// 0x007071E0 (:843)
bool video_cutscene_pending_or_playing()
{
    return Video_cutscene_ready_to_start || gameseq_get_state() == GS_VIDEO_CUTSCENE_PLAY || gameseq_get_pending_state() == GS_VIDEO_CUTSCENE_PLAY;
}

// 0x00707210 (:850)
void video_cutscene_do_frame()
{
    if (World.sl_flags.pending_save_game || World.sl_flags.performing_save_game || ui_is_msgbox_active(-2) || !Video_cutscene_ready_to_start)
    {
        return;
    }
    if (gameseq_get_pending_state() != GS_NONE || gameseq_get_state() != GS_GAMEPLAY || mission_is_active())
    {
        return;
    }
    if (!Video_cutscene_saving && Video_cutscene_warp_to_object != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(Video_cutscene_warp_to_object);
        if (objp != nullptr)
        {
            World.save_and_continue_warp(objp->pos, objp->orient);
            Video_cutscene_saving = true;
            return;
        }
    }
    video_cutscene_enter_state();
    Video_cutscene_ready_to_start = false;
    Video_cutscene_saving = false;
}

// 0x007072A0 (:906)
void video_cutscene_pause()
{
    if (Video_cutscene_exiting || Video_cutscene_about_to_exit)
    {
        return;
    }
    if (Video_cutscene_bink_handle != 0)
    {
        video_player_pause(Video_cutscene_bink_handle);
    }
    Video_cutscene_is_paused = true;
}

// 0x007072D0 (:922)
void video_cutscene_unpause()
{
    if (Video_cutscene_exiting || Video_cutscene_about_to_exit)
    {
        return;
    }
    if (Video_cutscene_bink_handle != 0 && Video_cutscene_is_paused && !Video_cutscene_about_to_exit)
    {
        video_player_unpause(Video_cutscene_bink_handle);
    }
    Video_cutscene_is_paused = false;
}
