// Original: volition/rfg/code/interface/ui_startup_video.h
// Implementation: volition/rfg/code/interface/ui_startup_video.cpp
//
// The start-up videos (logos) before the boot screen. Placement of the video sequence types inferred.
#pragma once

#include "volition/vlib/code/util/varray.h"

class rect_i;
class rl_renderer;
struct xml_element;

// size 0x41
struct video_data
{
    // size 0x1
    struct video_flags
    {
        unsigned char skip_on_user_input : 1; // 0x00:0
    };

    char filename[64]; // 0x00
    video_flags flags; // 0x40
};

// size 0x114
struct video_sequence_data
{
    video_sequence_data(); // 0x008CCB30
    ~video_sequence_data(); // 0x008CCB50

    farray<video_data,4> videos; // 0x00
};

extern bool Startup_videos_initialized; // 0x02C081C2
extern bool Exit_startup_videos; // 0x02C081C3
extern bool Pressing_any_button_for_startup_videos; // 0x02C081E4
// Test seam (UI.md deviation 4; off by default): OpenRFG's --notitle, --host and --join play no start-up video.
extern bool g_openrfgSkipStartupVideos;

bool ui_startup_video_parse_sequence(xml_element* xml_root, video_sequence_data& video_sequence); // 0x008CCB60 (:77)
void ui_startup_video_parse_table(); // 0x008CCCC0 (:134)
void ui_build_video_rect(rect_i& video_rect); // 0x008B98D0 (:175)
void ui_startup_video_play_video(const char* filename); // 0x008B9970 (:192)
bool ui_startup_video_init(); // 0x008CCEB0 (:251)
void ui_startup_video_process(); // 0x008B9A10 (:273)
void ui_startup_video_render(rl_renderer& renderer); // 0x008B9B30 (:332)
void ui_startup_video_cleanup(); // 0x008A1D00 (:351)
