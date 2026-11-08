// Original: volition/rfg/code/video_player/video_player.cpp
//
// The Bink video player (stub_impl.md S7, decision 4), through the shipped binkw32.dll: a pool of 8 trackers, each a Bink
// movie decoded into frame buffers the player allocates, three L8 textures (Y, cR, cB) updated on the render thread and
// drawn with the rl_prim_2d_bink material. A handle is the pool index with a 15-bit guard above it. Stopping only marks
// a tracker; video_player_update_render_all destroys it and calls its stop callback on the render side.
//
// Original bugs kept (commented where they are):
// - video_player_play returns without freeing the tracker when the file is missing;
// - its volume, surface_buffer_pool and contains_music are not used;
// - video_player_process_all does not throttle the disc for the second video;
// - video_player_system_shutdown closes the movies without freeing or checking the trackers.
// Never set in rfg.exe, kept: Bink_panicking and Bink_async (no asynchronous Bink thread is started). Written and never
// read, kept: Cur_processing_tracker, Bink_alloc_pool.

#include "volition/rfg/code/video_player/video_player.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/stub/audiolib_stub.h"
#include "volition/render_lib/code/rl_cached_dyn_texture.h"
#include "volition/render_lib/code/rl_draw.h"
#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/rfg.h"
#include "volition/stream2/code/stream2_os.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/cfile/keen_cfile.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/page_pool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ---- public globals
// { language, the first of its four audio tracks }.
bink_supported_audio_language Bink_localized_audio_tracks[6] = { // 0x016EBA88
    {(keen::LanguageId)1, 4},
    {(keen::LanguageId)2, 12},
    {(keen::LanguageId)5, 24},
    {(keen::LanguageId)3, 8},
    {(keen::LanguageId)4, 16},
    {(keen::LanguageId)7, 20},
};
volatile bool Bink_panicking; // 0x01A7AEEE

// ---- file-static globals
static int Next_video_handle_guard = 1; // 0x016EBA80
static unsigned int Bink_thread_index = 1; // 0x016EBA84
static video_tracker* Cur_processing_tracker; // 0x038822F0
static mempool_base* Bink_alloc_pool; // 0x038822F4
static bool Bink_async; // 0x038822F8
static unsigned int Bink_yplane_thread_index; // 0x038822FC
static object_pool<video_tracker, 8, 1> Video_tracker_pool; // 0x03882390
static bool Bink_was_panicking; // 0x03882C80

static void destroy_tracker(video_tracker* tracker);

// 0x00BD72B0 (:72)
video_tracker::video_tracker()
{
    init();
}

// 0x00BD6890 (:78)
void video_tracker::init()
{
    video_handle = 0;
    type = VT_INVALID;
    bink_handle = nullptr;
    decompression_pool = nullptr;
    internal_alloc_pool = nullptr;
    cr_tex = nullptr;
    cb_tex = nullptr;
    y_tex = nullptr;
    x = 0;
    y = 0;
    w = 0;
    h = 0;
    alpha = 1.0f;
    explicit_render = false;
    ready_for_destruction = false;
    first_frame = true;
    frame_dirty = false;
    opened_with_no_audio = false;
    stop_callback = nullptr;
}

// 0x00BD8B70 (:202)
// Console: play_video <file name> (<x> <y> <width> <height> (<volume>)); full screen without the dimensions.
void unused_dcf_play_video()
{
    bool usage = false;
    if (Console_command != 0)
    {
        // Magic numbers: 2 a string, 8 an integer, 0x10 a float; the second argument says the value is optional.
        if (console_get_arg(2, 0))
        {
            char filename[64];
            strcpy(filename, Console_arg); // unbounded in the original
            int x = 0;
            int y = 0;
            const rl_screen_info* screen = rl_render_lib::get_instance()->get_screen_info();
            int w = screen->get_screen_width();
            int h = screen->get_screen_height();
            float volume = 1.0f;
            if (console_get_arg(8, 1))
            {
                x = Console_arg_int;
                console_get_arg(8, 0);
                y = Console_arg_int;
                console_get_arg(8, 0);
                w = Console_arg_int;
                console_get_arg(8, 0);
                h = Console_arg_int;
                if (console_get_arg(0x10, 1))
                {
                    volume = Console_arg_float;
                }
            }
            video_player_play(filename, VPM_ONCE, x, y, w, h, volume, false, nullptr, nullptr, nullptr, nullptr, 0, false, false);
        }
        else
        {
            usage = true;
        }
    }
    if (Console_help != 0 || usage)
    {
        console_printf("Usage: play_video <file name> (<x> <y> <width> <height>)\n If the dimensions are not specified it will play fullscreen");
    }
}

// 0x00BD6910 (:335)
static void bink_do_frame_sub(video_tracker* tracker)
{
    if (tracker != nullptr && tracker->type == VT_BINK && tracker->bink_handle != nullptr)
    {
        Cur_processing_tracker = tracker;
        BinkDoFrame(tracker->bink_handle);
        Cur_processing_tracker = nullptr;
    }
}

// 0x00BD7A70 (:350)
// LTCG: tracker in ESI. Decodes when the frame is due, skips what is late, ends a video played once on its last frame.
static void bink_do_frame_sync(video_tracker* tracker)
{
    if (BinkWait(tracker->bink_handle) != 0)
    {
        return;
    }
    bink_do_frame_sub(tracker);
    tracker->frame_dirty = true;
    while (BinkShouldSkip(tracker->bink_handle) != 0)
    {
        BinkNextFrame(tracker->bink_handle);
        bink_do_frame_sub(tracker);
    }
    if (tracker->bink_handle->FrameNum == tracker->bink_handle->Frames && tracker->play_mode == VPM_ONCE)
    {
        video_player_stop(tracker->video_handle);
        return;
    }
    BinkNextFrame(tracker->bink_handle);
}

// 0x00BD7B20 (:381)
// LTCG: tracker in ESI. Bink_async is never set, so this does not run.
static void bink_do_frame_async(video_tracker* tracker)
{
    if (tracker->first_frame)
    {
        BinkDoFrameAsync(tracker->bink_handle, Bink_yplane_thread_index, Bink_thread_index);
    }
    if (BinkWait(tracker->bink_handle) != 0)
    {
        return;
    }
    // Magic numbers: microseconds to wait; -1 waits for the first frame without a limit.
    if (BinkDoFrameAsyncWait(tracker->bink_handle, tracker->first_frame ? -1 : 1000) == 0)
    {
        return;
    }
    while (BinkShouldSkip(tracker->bink_handle) != 0)
    {
        BinkNextFrame(tracker->bink_handle);
        BinkDoFrameAsync(tracker->bink_handle, Bink_yplane_thread_index, Bink_thread_index);
        BinkDoFrameAsyncWait(tracker->bink_handle, -1);
    }
    if (tracker->bink_handle->FrameNum == tracker->bink_handle->Frames && tracker->play_mode == VPM_ONCE)
    {
        video_player_stop(tracker->video_handle);
        return;
    }
    BinkNextFrame(tracker->bink_handle);
    BinkDoFrameAsync(tracker->bink_handle, Bink_yplane_thread_index, Bink_thread_index);
    tracker->first_frame = false;
    tracker->frame_dirty = true;
}

// 0x00BD7C10 (:431)
// LTCG: tracker in EAX.
static void bink_do_frame(video_tracker* tracker)
{
    if (tracker != nullptr && tracker->type == VT_BINK && tracker->bink_handle != nullptr)
    {
        if (Bink_async)
        {
            bink_do_frame_async(tracker);
        }
        else
        {
            bink_do_frame_sync(tracker);
        }
    }
}

// 0x00BD6940 (:444)
// LTCG: tracker in ECX, fname in EDX; on the stack volume, flags, cutscene_audio, extra_read_buffer. contains_music is
// dropped and volume is not read: the volume comes from the audio system's video and master scalars.
// cutscene_audio: tracks 0-3 hold the sound, and the localized speech starts at the language's track.
static void bink_open(bool contains_music, video_tracker* tracker, const char* fname, float volume, unsigned int flags, bool cutscene_audio, unsigned int extra_read_buffer)
{
    if (Bink_panicking)
    {
        cutscene_audio = false;
    }
    const unsigned int num_speakers = audiolib_get_num_speakers();
    unsigned int speech_track = Bink_localized_audio_tracks[0].track_no;
    Cur_processing_tracker = tracker;
    // Magic number: BINKIOSIZE.
    if ((flags & 0x1000000) != 0)
    {
        BinkSetIOSize((unsigned long long)extra_read_buffer);
    }
    // Original: the name without its path is made and not used.
    char name[64];
    cf_drop_path(name, sizeof(name), fname);
    unsigned int tracks[8];
    int num_tracks;
    tracks[0] = 0;
    if (!cutscene_audio)
    {
        num_tracks = 4;
        tracks[1] = 1;
        tracks[2] = 2;
        tracks[3] = 3;
    }
    else
    {
        const keen::LanguageId language = localization_get_audio_language();
        for (unsigned int i = 0; i < 6; i++)
        {
            if (Bink_localized_audio_tracks[i].language_id == language)
            {
                speech_track = Bink_localized_audio_tracks[i].track_no;
                break;
            }
        }
        if (num_speakers == 6)
        {
            num_tracks = 6;
            tracks[1] = 2;
            tracks[2] = 3;
            tracks[3] = speech_track;
            tracks[4] = speech_track + 1;
            tracks[5] = speech_track + 3;
        }
        else if (num_speakers == 8)
        {
            num_tracks = 8;
            tracks[1] = 1;
            tracks[2] = 2;
            tracks[3] = 3;
            tracks[4] = speech_track;
            tracks[5] = speech_track + 1;
            tracks[6] = speech_track + 2;
            tracks[7] = speech_track + 3;
        }
        else
        {
            num_tracks = 2;
            tracks[1] = speech_track;
        }
    }
    if (!Bink_panicking)
    {
        BinkSetSoundTrack(num_tracks, tracks);
        tracker->opened_with_no_audio = false;
    }
    else
    {
        BinkSetSoundTrack(0, nullptr);
        tracker->opened_with_no_audio = true;
    }
    // Magic number: BINKSNDTRACK.
    tracker->bink_handle = BinkOpen(fname, flags | 0x4000);
    Cur_processing_tracker = nullptr;
    int track_volume;
    if (gameaudio_system_init_stage_2_complete() && !audiolib_get_sound_enabled())
    {
        track_volume = 0;
    }
    else
    {
        float video_scalar = 0.0f;
        float master_scalar = 0.0f;
        // Magic numbers: audio system volumes 2 and 6.
        if (audiolib_system_get_volume_scalar(2, &video_scalar) != AUDIOLIB_ERR_NONE || audiolib_system_get_volume_scalar(6, &master_scalar) != AUDIOLIB_ERR_NONE)
        {
            video_scalar = 1.0f;
            master_scalar = 1.0f;
        }
        const float scalar = (float)((double)master_scalar * (double)video_scalar);
        // Magic number: Bink's full volume is 32768; rfg.exe stops one below it.
        track_volume = scalar > 1.0f ? 0x7FFF : (int)((double)scalar * 32767.0);
    }
    for (int i = 0; i < num_tracks; i++)
    {
        int volume_of_track = track_volume;
        if (cutscene_audio && num_speakers == 2 && tracks[i] == speech_track)
        {
            // Magic number: speech at 80% on stereo (0.8f widened to double in rfg.exe).
            volume_of_track = (int)((double)(float)track_volume * (double)0.8f);
        }
        BinkSetVolume(tracker->bink_handle, tracks[i], volume_of_track);
    }
    tracker->type = VT_BINK;
}

// 0x00BD6C00 (:826)
// LTCG: tracker in ESI.
static void bink_destroy_tracker(video_tracker* tracker)
{
    if (tracker == nullptr || tracker->type != VT_BINK)
    {
        return;
    }
    if (tracker->cb_tex != nullptr)
    {
        tracker->cb_tex->release();
        tracker->cb_tex = nullptr;
    }
    if (tracker->cr_tex != nullptr)
    {
        tracker->cr_tex->release();
        tracker->cr_tex = nullptr;
    }
    if (tracker->y_tex != nullptr)
    {
        tracker->y_tex->release();
        tracker->y_tex = nullptr;
    }
    if (tracker->bink_handle != nullptr)
    {
        BinkClose(tracker->bink_handle);
        tracker->bink_handle = nullptr;
    }
    if (tracker->decompression_pool != nullptr)
    {
        Common_page_pool.release_virtual_mempool(tracker->decompression_pool);
        tracker->decompression_pool = nullptr;
    }
    if (tracker->internal_alloc_pool != nullptr)
    {
        Common_page_pool.release_virtual_mempool(tracker->internal_alloc_pool);
        tracker->internal_alloc_pool = nullptr;
    }
}

// 0x00BD6CB0 (:862)
// LTCG: tracker in EAX, pause in CL.
static void bink_pause(video_tracker* tracker, bool pause)
{
    if (tracker != nullptr && tracker->type == VT_BINK && tracker->bink_handle != nullptr)
    {
        BinkPause(tracker->bink_handle, pause);
    }
}

// 0x00BD6CD0 (:871)
static uint64_t bink_percentage_full(BINK* bink)
{
    BINKREALTIME realtime;
    BinkGetRealtime(bink, &realtime, 0);
    return realtime.ReadBufferUsed * 100 / (realtime.ReadBufferSize + 1);
}

// 0x00BD6D30 (:880)
// LTCG: tracker in ESI. The disc is shared with the streaming system: past 80% of Bink's read buffer Bink's reads stop
// and the streaming system's go on, below 30% the other way round.
static void bink_maybe_throttle(video_tracker* tracker)
{
    if (tracker == nullptr || tracker->bink_handle == nullptr || tracker->type != VT_BINK)
    {
        return;
    }
    // Magic numbers: 1 BINKBGIOSUSPEND, 2 BINKBGIORESUME.
    if (stream2_os_disc_is_paused())
    {
        BinkControlBackgroundIO(tracker->bink_handle, 2);
        if (bink_percentage_full(tracker->bink_handle) > 80)
        {
            BinkControlBackgroundIO(tracker->bink_handle, 1);
            stream2_os_unpause_disc_processing();
        }
    }
    else
    {
        BinkControlBackgroundIO(tracker->bink_handle, 1);
        if (bink_percentage_full(tracker->bink_handle) < 30)
        {
            stream2_os_pause_disc_processing();
            BinkControlBackgroundIO(tracker->bink_handle, 2);
        }
    }
}

// 0x00BD72D0 (:909)
// LTCG: tracker in ESI.
static void bink_render(video_tracker* tracker)
{
    if (tracker == nullptr || tracker->type != VT_BINK || tracker->bink_handle == nullptr)
    {
        return;
    }
    rl_draw* draw = rl_draw::get_instance();
    draw->set_texture(-1);
    rl_color_float color;
    color.red = 1.0f;
    color.green = 1.0f;
    color.blue = 1.0f;
    color.alpha = tracker->alpha;
    draw->set_color(color);
    draw->set_bink_textures(tracker->y_tex->get_texture_handle(), tracker->cr_tex->get_texture_handle(), tracker->cb_tex->get_texture_handle());
    draw->rect((float)tracker->x, (float)tracker->y, (float)tracker->w, (float)tracker->h, rl_primitive_state::FILTER);
    draw->set_bink_textures(-1, -1, -1);
}

// 0x00BD6DB0 (:922)
// LTCG: tracker in ESI, renderer on the stack. The planes of the frame buffer Bink decoded into go to the textures.
static void bink_update_render(video_tracker* tracker, rl_renderer& renderer)
{
    if (tracker == nullptr || tracker->type != VT_BINK || tracker->bink_handle == nullptr)
    {
        return;
    }
    const BINKFRAMEPLANESET& frame = tracker->bink_buffers.Frames[tracker->bink_buffers.FrameNum];
    tracker->cb_tex->update(renderer, (unsigned char*)frame.cBPlane.Buffer, (int)frame.cBPlane.BufferPitch);
    tracker->cr_tex->update(renderer, (unsigned char*)frame.cRPlane.Buffer, (int)frame.cRPlane.BufferPitch);
    tracker->y_tex->update(renderer, (unsigned char*)frame.YPlane.Buffer, (int)frame.YPlane.BufferPitch);
    tracker->frame_dirty = false;
}

// 0x00BD6E20 (:936)
static int bink_get_current_frame(video_tracker* tracker)
{
    if (tracker != nullptr && tracker->type == VT_BINK && tracker->bink_handle != nullptr)
    {
        return (int)tracker->bink_handle->FrameNum;
    }
    return -1;
}

// 0x00BD6E40 (:945)
static int bink_get_total_frames(video_tracker* tracker)
{
    if (tracker != nullptr && tracker->type == VT_BINK && tracker->bink_handle != nullptr)
    {
        return (int)tracker->bink_handle->Frames;
    }
    return -1;
}

// 0x00BD6E60 (:954)
// LTCG: tracker in ESI.
static bool bink_goto_frame(video_tracker* tracker, int framenum)
{
    if (tracker == nullptr || tracker->type != VT_BINK || tracker->bink_handle == nullptr)
    {
        return false;
    }
    if (Bink_async)
    {
        BinkDoFrameAsyncWait(tracker->bink_handle, -1);
        tracker->first_frame = true;
    }
    // Magic numbers: 2 the closest key frame, 3 BINKGOTOQUICK | BINKGOTOQUICKSOUND.
    const unsigned int key_frame = BinkGetKeyFrame(tracker->bink_handle, (unsigned int)framenum, 2);
    BinkGoto(tracker->bink_handle, key_frame, 3);
    return true;
}

// 0x00BD6EB0 (:971)
// LTCG: volume in XMM0.
static void bink_set_volume(video_tracker* tracker, float volume)
{
    if (tracker == nullptr || tracker->type != VT_BINK || tracker->bink_handle == nullptr)
    {
        return;
    }
    const float magnitude = fabsf(volume);
    const int track_volume = magnitude > 1.0f ? 0x7FFF : (int)(magnitude * 32767.0f);
    for (unsigned int track = 0; track < 4; track++)
    {
        BinkSetVolume(tracker->bink_handle, track, track_volume);
    }
}

// 0x00BD6F30 (:985)
static int bink_get_frame_rate(video_tracker* tracker)
{
    if (tracker != nullptr && tracker->type == VT_BINK && tracker->bink_handle != nullptr)
    {
        return (int)(tracker->bink_handle->FrameRate / tracker->bink_handle->FrameRateDiv);
    }
    return -1;
}

// 0x00BD8130 (:1007)
// LTCG: tracker in EAX.
static void destroy_tracker(video_tracker* tracker)
{
    if (tracker->type == VT_BINK)
    {
        bink_destroy_tracker(tracker);
    }
    else
    {
        VLIB_CRITICAL_FAILURE();
    }
    if (tracker->stop_callback != nullptr)
    {
        // The callback may need the disc.
        const bool disc_paused = stream2_os_disc_is_paused();
        if (disc_paused)
        {
            stream2_os_unpause_disc_processing();
        }
        tracker->stop_callback(tracker->video_handle);
        tracker->stop_callback = nullptr;
        if (disc_paused)
        {
            stream2_os_pause_disc_processing();
        }
    }
    tracker->video_handle = 0;
    tracker->stop_callback = nullptr;
    tracker->type = VT_INVALID;
    Video_tracker_pool.free(tracker);
}

// 0x00BD73C0 (:1051)
// LTCG: video_handle in ECX. The tracker of a handle still in use.
static video_tracker* get_tracker_by_handle(int video_handle)
{
    if (video_handle == 0)
    {
        return nullptr;
    }
    video_tracker* tracker = Video_tracker_pool.get_entry(video_handle & 0xFFFF);
    return tracker != nullptr && tracker->video_handle == video_handle ? tracker : nullptr;
}

// 0x00BD6F50 (:1072)
void video_player_system_init()
{
    BinkSetSoundSystem((void*)BinkOpenDirectSound, 0);
}

// 0x00BD6F60 (:1122)
// In the game's own folder, then the current DLC's. found_info (optional) gets the file; only the first search makes its
// full name the native path.
bool video_player_video_exists(const char* filename, cf_found_info* found_info)
{
    char path[256];
    const char* name = filename;
    if (Use_packfiles)
    {
        sprintf(path, "%s\\%s", "/gamebuild", filename);
        name = path;
    }
    game_enable_standard_access();
    bool exists;
    if (found_info != nullptr)
    {
        exists = cf_find_file(found_info, name, false);
        if (exists)
        {
            keen_cf_getNativeFilePath(found_info->m_full_name, 0x100, name);
        }
    }
    else
    {
        exists = cf_exist(name);
    }
    game_disable_standard_access();
    if (Use_packfiles && !exists && dlc_get_current_id() >= 0)
    {
        _snprintf_safe(path, 0x100, "%s%s", dlc_path_callback(dlc_bundle_id_to_index(dlc_get_current_id())), filename);
        game_enable_standard_access();
        exists = found_info != nullptr ? cf_find_file(found_info, path, false) : cf_exist(path);
        game_disable_standard_access();
    }
    return exists;
}

// Port helper: the size rounded up to the planes' 16 bytes, as rfg.exe inlines it.
static unsigned int video_player_align_16(unsigned int size)
{
    const int remainder = (int)size % 16;
    return size + (remainder == 0 ? 0 : 16 - remainder);
}

// 0x00BD81C0 (:1176)
// Opens the file, allocates its frame buffers (from decompression_pool, else a pool of its own) and creates its
// textures. Returns the handle, 0 on failure (the stop callback has then run unless the file was missing).
int video_player_play(const char* filename, video_play_mode play_mode, int x, int y, int w, int h, float volume, bool explicit_render, void (*vsc)(int), mempool_base* surface_buffer_pool, mempool_base* internal_alloc_pool, mempool_base* decompression_pool, unsigned int extra_read_buffer, bool cutscene_audio_tracks, bool contains_music)
{
    video_tracker* tracker = Video_tracker_pool.alloc(0);
    if (tracker == nullptr)
    {
        return 0;
    }
    tracker->init();
    cf_found_info found_info;
    if (Use_packfiles)
    {
        if (!video_player_video_exists(filename, &found_info))
        {
            // Original bug kept: the tracker stays allocated (eight missing files use up the pool).
            return 0;
        }
    }
    else
    {
        cf_find_file(&found_info, filename, false);
    }
    char full_name[256];
    strcpy(full_name, found_info.m_full_name);
    if (internal_alloc_pool == nullptr)
    {
        // Magic number: 2.5 MB.
        internal_alloc_pool = Common_page_pool.get_virtual_mempool("bink internal alloc", 0x280000);
        tracker->internal_alloc_pool = (virtual_mempool*)internal_alloc_pool;
        if (internal_alloc_pool == nullptr)
        {
            destroy_tracker(tracker);
            return 0;
        }
    }
    else
    {
        tracker->internal_alloc_pool = nullptr;
    }
    Bink_alloc_pool = internal_alloc_pool;
    tracker->x = x;
    tracker->y = y;
    tracker->w = w;
    tracker->h = h;
    tracker->stop_callback = vsc;
    tracker->explicit_render = explicit_render;
    tracker->play_mode = play_mode;
    // Magic numbers: 0x400 BINKNOFRAMEBUFFERS, 0x4000 BINKSNDTRACK, 0x1000000 BINKIOSIZE.
    bink_open(contains_music, tracker, full_name, volume, extra_read_buffer != 0 ? 0x1004400 : 0x4400, cutscene_audio_tracks, extra_read_buffer);
    if (tracker->bink_handle == nullptr)
    {
        destroy_tracker(tracker);
        Bink_alloc_pool = nullptr;
        return 0;
    }
    tracker->cb_tex = rl_cached_dyn_texture::create();
    tracker->cr_tex = rl_cached_dyn_texture::create();
    tracker->y_tex = rl_cached_dyn_texture::create();
    if (tracker->cb_tex == nullptr || tracker->cr_tex == nullptr || tracker->y_tex == nullptr)
    {
        destroy_tracker(tracker);
        Bink_alloc_pool = nullptr;
        return 0;
    }
    BINKFRAMEBUFFERS& buffers = tracker->bink_buffers;
    BinkGetFrameBuffersInfo(tracker->bink_handle, &buffers);
    const unsigned int ya_pitch = video_player_align_16(buffers.YABufferWidth);
    const unsigned int c_pitch = video_player_align_16(buffers.cRcBBufferWidth);
    unsigned int total_size = 0;
    for (int i = 0; i < buffers.TotalFrames; i++)
    {
        BINKFRAMEPLANESET& frame = buffers.Frames[i];
        if (frame.APlane.Allocate != 0)
        {
            frame.APlane.BufferPitch = ya_pitch;
            total_size += buffers.YABufferHeight * ya_pitch;
        }
        total_size = video_player_align_16(total_size);
        if (frame.YPlane.Allocate != 0)
        {
            frame.YPlane.BufferPitch = ya_pitch;
            total_size += buffers.YABufferHeight * ya_pitch;
        }
        total_size = video_player_align_16(total_size);
        if (frame.cRPlane.Allocate != 0)
        {
            frame.cRPlane.BufferPitch = c_pitch;
            total_size += buffers.cRcBBufferHeight * c_pitch;
        }
        total_size = video_player_align_16(total_size);
        if (frame.cBPlane.Allocate != 0)
        {
            frame.cBPlane.BufferPitch = c_pitch;
            total_size += buffers.cRcBBufferHeight * c_pitch;
        }
        total_size = video_player_align_16(total_size);
    }
    if (decompression_pool == nullptr)
    {
        decompression_pool = Common_page_pool.get_virtual_mempool("bink decompression", total_size);
        tracker->decompression_pool = (virtual_mempool*)decompression_pool;
    }
    else
    {
        tracker->decompression_pool = nullptr;
    }
    if (decompression_pool == nullptr)
    {
        destroy_tracker(tracker);
        Bink_alloc_pool = nullptr;
        return 0;
    }
    for (int i = 0; i < buffers.TotalFrames; i++)
    {
        BINKFRAMEPLANESET& frame = buffers.Frames[i];
        if (frame.APlane.Allocate != 0)
        {
            frame.APlane.Buffer = decompression_pool->alloc(buffers.YABufferHeight * ya_pitch, 0x10);
        }
        if (frame.YPlane.Allocate != 0)
        {
            frame.YPlane.Buffer = decompression_pool->alloc(buffers.YABufferHeight * ya_pitch, 0x10);
        }
        if (frame.cRPlane.Allocate != 0)
        {
            frame.cRPlane.Buffer = decompression_pool->alloc(buffers.cRcBBufferHeight * c_pitch, 0x10);
        }
        if (frame.cBPlane.Allocate != 0)
        {
            frame.cBPlane.Buffer = decompression_pool->alloc(buffers.cRcBBufferHeight * c_pitch, 0x10);
        }
    }
    BinkRegisterFrameBuffers(tracker->bink_handle, &buffers);
    BINKSUMMARY summary;
    BinkGetSummary(tracker->bink_handle, &summary);
    // The chroma planes are half the size, rounded up.
    const int c_width = (int)summary.Width / 2 + (int)summary.Width % 2;
    const int c_height = (int)summary.Height / 2 + (int)summary.Height % 2;
    tracker->cr_tex->init(c_width, c_height, RLCDT_FORMAT_L8, RLCDT_MEM_DYNAMIC);
    tracker->cb_tex->init(c_width, c_height, RLCDT_FORMAT_L8, RLCDT_MEM_DYNAMIC);
    tracker->y_tex->init((int)summary.Width, (int)summary.Height, RLCDT_FORMAT_L8, RLCDT_MEM_DYNAMIC);
    const int index = (int)(tracker - Video_tracker_pool.get_entry(0));
    const int handle = (index & 0xFFFF) | ((Next_video_handle_guard & 0x7FFF) << 16);
    Next_video_handle_guard++;
    tracker->video_handle = handle;
    Bink_alloc_pool = nullptr;
    return handle;
}

// 0x00BD7400 (:1405)
void video_player_stop(int video_handle)
{
    video_tracker* tracker = get_tracker_by_handle(video_handle);
    if (tracker != nullptr && !tracker->ready_for_destruction)
    {
        tracker->ready_for_destruction = true;
    }
}

// 0x00BD7440 (:1421)
void video_player_pause(int video_handle)
{
    video_tracker* tracker = get_tracker_by_handle(video_handle);
    if (tracker == nullptr)
    {
        return;
    }
    if (tracker->type != VT_BINK)
    {
        VLIB_CRITICAL_FAILURE();
        return;
    }
    bink_pause(tracker, true);
}

// 0x00BD7C40 (:1445)
void video_player_pause_all()
{
    Video_tracker_pool.thread_access.enter();
    for (video_tracker* tracker = Video_tracker_pool.get_first(0); tracker != nullptr; tracker = Video_tracker_pool.get_next(tracker))
    {
        video_player_pause(tracker->video_handle);
    }
    Video_tracker_pool.thread_access.leave();
}

// 0x00BD7D40 (:1455)
void video_player_unpause_all()
{
    Video_tracker_pool.thread_access.enter();
    for (video_tracker* tracker = Video_tracker_pool.get_first(0); tracker != nullptr; tracker = Video_tracker_pool.get_next(tracker))
    {
        video_player_unpause(tracker->video_handle);
    }
    Video_tracker_pool.thread_access.leave();
}

// 0x00BD74A0 (:1465)
void video_player_unpause(int video_handle)
{
    video_tracker* tracker = get_tracker_by_handle(video_handle);
    if (tracker == nullptr)
    {
        return;
    }
    if (tracker->type != VT_BINK)
    {
        VLIB_CRITICAL_FAILURE();
        return;
    }
    bink_pause(tracker, false);
}

// 0x00BD7500 (:1489)
void video_player_resize(int video_handle, int x, int y, int w, int h)
{
    video_tracker* tracker = get_tracker_by_handle(video_handle);
    if (tracker != nullptr)
    {
        tracker->x = x;
        tracker->y = y;
        tracker->w = w;
        tracker->h = h;
    }
}

// 0x00BD7560 (:1503)
void video_player_set_alpha(int video_handle, float alpha)
{
    video_tracker* tracker = get_tracker_by_handle(video_handle);
    if (tracker != nullptr)
    {
        tracker->alpha = alpha;
    }
}

// 0x00BD86D0 (:1514)
// Each frame: while Bink panics (never in rfg.exe) the videos with sound are destroyed; each video decodes and shares
// the disc with the streaming system; without a video the streaming system has the disc.
void video_player_process_all()
{
    if (Bink_panicking)
    {
        // Magic number: 16 trackers at most (the pool has 8).
        video_tracker* to_destroy[16];
        int num_to_destroy = 0;
        Video_tracker_pool.thread_access.enter();
        for (video_tracker* tracker = Video_tracker_pool.get_first(0); tracker != nullptr; tracker = Video_tracker_pool.get_next(tracker))
        {
            if (tracker->type == VT_BINK && tracker->bink_handle != nullptr && tracker->bink_handle->NumTracks > 0 && !tracker->opened_with_no_audio && num_to_destroy < 16)
            {
                to_destroy[num_to_destroy] = tracker;
                num_to_destroy++;
            }
        }
        Video_tracker_pool.thread_access.leave();
        for (int i = 0; i < num_to_destroy; i++)
        {
            destroy_tracker(to_destroy[i]);
        }
    }
    if (Bink_panicking != Bink_was_panicking)
    {
        BinkSetSoundSystem((void*)BinkOpenDirectSound, 0);
    }
    Bink_was_panicking = Bink_panicking;
    int count = 0;
    Video_tracker_pool.thread_access.enter();
    video_tracker* next = nullptr;
    for (video_tracker* tracker = Video_tracker_pool.get_first(0); tracker != nullptr; tracker = next)
    {
        next = Video_tracker_pool.get_next(tracker);
        count++;
        if (tracker->type == VT_BINK)
        {
            bink_do_frame(tracker);
        }
        else
        {
            VLIB_CRITICAL_FAILURE();
        }
        // Original bug kept: the second video in the list does not throttle (2, not "the first").
        if (count != 2)
        {
            bink_maybe_throttle(tracker);
        }
    }
    Video_tracker_pool.thread_access.leave();
    if (count == 0)
    {
        stream2_os_unpause_disc_processing();
    }
}

// 0x00BD75A0 (:1593)
void video_player_render(int video_handle)
{
    video_tracker* tracker = get_tracker_by_handle(video_handle);
    if (tracker == nullptr)
    {
        return;
    }
    if (tracker->type != VT_BINK)
    {
        VLIB_CRITICAL_FAILURE();
        return;
    }
    bink_render(tracker);
}

// 0x00BD7E40 (:1617)
void video_player_render_all()
{
    Video_tracker_pool.thread_access.enter();
    for (video_tracker* tracker = Video_tracker_pool.get_first(0); tracker != nullptr; tracker = Video_tracker_pool.get_next(tracker))
    {
        if (!tracker->explicit_render)
        {
            video_player_render(tracker->video_handle);
        }
    }
    Video_tracker_pool.thread_access.leave();
}

// 0x00BD89E0 (:1628)
// On the render side: stopped videos are destroyed, decoded frames go to the textures.
void video_player_update_render_all(rl_renderer& renderer)
{
    Video_tracker_pool.thread_access.enter();
    video_tracker* next = nullptr;
    for (video_tracker* tracker = Video_tracker_pool.get_first(0); tracker != nullptr; tracker = next)
    {
        next = Video_tracker_pool.get_next(tracker);
        if (tracker->ready_for_destruction)
        {
            destroy_tracker(tracker);
        }
        else if (tracker->frame_dirty)
        {
            if (tracker->type == VT_BINK)
            {
                bink_update_render(tracker, renderer);
            }
            else
            {
                VLIB_CRITICAL_FAILURE();
            }
        }
    }
    Video_tracker_pool.thread_access.leave();
}

// 0x00BD7600 (:1662)
int video_player_get_current_frame(int video_handle)
{
    video_tracker* tracker = get_tracker_by_handle(video_handle);
    if (tracker == nullptr)
    {
        return -1;
    }
    if (tracker->type != VT_BINK)
    {
        VLIB_CRITICAL_FAILURE();
        return -1;
    }
    return bink_get_current_frame(tracker);
}

// 0x00BD7660 (:1687)
int video_player_get_total_frames(int video_handle)
{
    video_tracker* tracker = get_tracker_by_handle(video_handle);
    if (tracker == nullptr)
    {
        return -1;
    }
    if (tracker->type != VT_BINK)
    {
        VLIB_CRITICAL_FAILURE();
        return -1;
    }
    return bink_get_total_frames(tracker);
}

// 0x00BD76C0 (:1711)
bool video_player_goto_frame(int video_handle, int framenum)
{
    video_tracker* tracker = get_tracker_by_handle(video_handle);
    if (tracker == nullptr)
    {
        return false;
    }
    if (tracker->type != VT_BINK)
    {
        VLIB_CRITICAL_FAILURE();
        return false;
    }
    return bink_goto_frame(tracker, framenum);
}

// 0x00BD7720 (:1736)
void video_player_set_volume(int video_handle, float volume)
{
    video_tracker* tracker = get_tracker_by_handle(video_handle);
    if (tracker == nullptr)
    {
        return;
    }
    if (tracker->type != VT_BINK)
    {
        VLIB_CRITICAL_FAILURE();
        return;
    }
    bink_set_volume(tracker, volume);
}

// 0x00BD7780 (:1760)
int video_player_get_frame_rate(int video_handle)
{
    video_tracker* tracker = get_tracker_by_handle(video_handle);
    if (tracker == nullptr)
    {
        return -1;
    }
    if (tracker->type != VT_BINK)
    {
        VLIB_CRITICAL_FAILURE();
        return -1;
    }
    return bink_get_frame_rate(tracker);
}

// 0x00BD7F30 (:1784)
void video_player_system_shutdown()
{
    Video_tracker_pool.thread_access.enter();
    for (video_tracker* tracker = Video_tracker_pool.get_first(0); tracker != nullptr; tracker = Video_tracker_pool.get_next(tracker))
    {
        if (tracker->type == VT_BINK)
        {
            // Original bug kept: the tracker keeps the closed handle and is not freed.
            BinkClose(tracker->bink_handle);
        }
    }
    Video_tracker_pool.thread_access.leave();
    if (Bink_async)
    {
        BinkRequestStopAsyncThread(Bink_yplane_thread_index);
        BinkWaitStopAsyncThread(Bink_yplane_thread_index);
        BinkRequestStopAsyncThread(Bink_thread_index);
        BinkWaitStopAsyncThread(Bink_thread_index);
    }
}

// 0x00BD7080 (:1808)
// Compiled out in rfg.exe.
bool video_player_get_texture_handles(int video_handle, int& y_tex_handle, int& cb_tex_handle, int& cr_tex_handle)
{
    return false;
}
