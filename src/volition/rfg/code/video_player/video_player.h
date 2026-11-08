// Original: volition/rfg/code/video_player/video_player.h
// Implementation: volition/rfg/code/video_player/video_player.cpp
//
// The Bink video player: video trackers (Video_tracker_pool, 8) play a Bink file into textures, synchronously or on a
// thread, looped or once, with volume, alpha, size and frame control; the localized audio tracks and the Bink memory
// pool. Placement of the enums inferred.
#pragma once

#include "keen/base/language.hpp"
#include "library/modules/thirdparty/bink/bink_1_300_7_0/include/bink.h"

struct cf_found_info;
class mempool_base;
class rl_cached_dyn_texture;
class rl_renderer;
class virtual_mempool;

enum video_play_mode
{
    VPM_ONCE = 0,
    VPM_LOOPING = 1,
    NUM_VIDEO_PLAY_MODES = 2,
};

enum video_type
{
    VT_INVALID = 0,
    VT_BINK = 1,
};

// size 0x8
struct bink_supported_audio_language
{
    keen::LanguageId language_id; // 0x00
    unsigned int track_no; // 0x04
};

// size 0x110
class video_tracker
{
public:
    video_tracker(); // 0x00BD72B0 (:72)
    void init(); // 0x00BD6890 (:78)

    int video_handle; // 0x00
    video_type type; // 0x04
    BINK* bink_handle; // 0x08
    BINKFRAMEBUFFERS bink_buffers; // 0x0C
    virtual_mempool* decompression_pool; // 0xD8
    virtual_mempool* internal_alloc_pool; // 0xDC
    rl_cached_dyn_texture* cr_tex; // 0xE0
    rl_cached_dyn_texture* cb_tex; // 0xE4
    rl_cached_dyn_texture* y_tex; // 0xE8
    int x; // 0xEC
    int y; // 0xF0
    int w; // 0xF4
    int h; // 0xF8
    float alpha; // 0xFC
    bool explicit_render; // 0x100
    bool ready_for_destruction; // 0x101
    bool first_frame; // 0x102
    bool frame_dirty; // 0x103
    bool opened_with_no_audio; // 0x104
    video_play_mode play_mode; // 0x108
    void (*stop_callback)(int); // 0x10C
};

extern bink_supported_audio_language Bink_localized_audio_tracks[6]; // 0x016EBA88
extern volatile bool Bink_panicking; // 0x01A7AEEE

void unused_dcf_play_video(); // 0x00BD8B70 (:202)
void video_player_system_init(); // 0x00BD6F50 (:1072)
bool video_player_video_exists(const char* filename, cf_found_info* found_info); // 0x00BD6F60 (:1122)
int video_player_play(const char* filename, video_play_mode play_mode, int x, int y, int w, int h, float volume, bool explicit_render, void (*vsc)(int), mempool_base* surface_buffer_pool, mempool_base* internal_alloc_pool, mempool_base* decompression_pool, unsigned int extra_read_buffer, bool cutscene_audio_tracks, bool contains_music); // 0x00BD81C0 (:1176)
void video_player_stop(int video_handle); // 0x00BD7400 (:1405)
void video_player_pause(int video_handle); // 0x00BD7440 (:1421)
void video_player_pause_all(); // 0x00BD7C40 (:1445)
void video_player_unpause_all(); // 0x00BD7D40 (:1455)
void video_player_unpause(int video_handle); // 0x00BD74A0 (:1465)
void video_player_resize(int video_handle, int x, int y, int w, int h); // 0x00BD7500 (:1489)
void video_player_set_alpha(int video_handle, float alpha); // 0x00BD7560 (:1503)
void video_player_process_all(); // 0x00BD86D0 (:1514)
void video_player_render(int video_handle); // 0x00BD75A0 (:1593)
void video_player_render_all(); // 0x00BD7E40 (:1617)
void video_player_update_render_all(rl_renderer& renderer); // 0x00BD89E0 (:1628)
int video_player_get_current_frame(int video_handle); // 0x00BD7600 (:1662)
int video_player_get_total_frames(int video_handle); // 0x00BD7660 (:1687)
bool video_player_goto_frame(int video_handle, int framenum); // 0x00BD76C0 (:1711)
void video_player_set_volume(int video_handle, float volume); // 0x00BD7720 (:1736)
int video_player_get_frame_rate(int video_handle); // 0x00BD7780 (:1760)
void video_player_system_shutdown(); // 0x00BD7F30 (:1784)
bool video_player_get_texture_handles(int video_handle, int& y_tex_handle, int& cb_tex_handle, int& cr_tex_handle); // 0x00BD7080 (:1808)
