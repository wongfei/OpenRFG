// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_tutorials.h
// Implementation: volition/rfg/code/interface/multi_player_menus/ui_document_mp_tutorials.cpp
//
// The multiplayer tutorials (videos). Placement of the video types inferred.
#pragma once

#include "volition/interface_lib/code/vint_external.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_internal_shared.h"

class mempool_base;
struct mission_briefing_line_info;
class rl_renderer;

enum bink_state_tutorials
{
    BINK_STATE_TUTORIALS_PLAY = 0,
    BINK_STATE_TUTORIALS_STOPPED = 1,
    BINK_STATE_TUTORIALS_PLAYING = 2,
    BINK_STATE_TUTORIALS_PAUSED = 3,
    BINK_STATE_TUTORIALS_LOOPING_END = 4,
};

// size 0x4C
struct tutorial_video_info
{
    void reset(); // 0x0088ECE0 (:130)
    tutorial_video_info(); // 0x0088ED30

    const char* filename; // 0x00
    int handle; // 0x04
    mission_briefing_line_info* line_info; // 0x08
    bink_state_tutorials state; // 0x0C
    int num_frames; // 0x10
    vector2i position; // 0x14
    vector2i size; // 0x1C
    float alpha; // 0x24
    human_voice_persona_handle curr_persona_handle; // 0x28
    voice_line_handle curr_vl_handle; // 0x2C
    int cur_vl_id; // 0x30
    bool playing_audio_line; // 0x34
    int which_line_index; // 0x38
    unsigned int mempool_mark; // 0x3C
    unsigned int mempool_mark_gpu; // 0x40
    unsigned int dec_mempool_mark; // 0x44
    unsigned int dec_mempool_mark_gpu; // 0x48
};

void tutorials_trigger_video_line(int which_line); // 0x008AEB60 (:581)
void ui_document_mp_tutorials_init(); // 0x00917490 (:1190)
void ui_document_mp_tutorials_multi_init(mempool_base& pool); // 0x008D5BD0 (:1207)
void ui_document_mp_tutorials_multi_shutdown(); // 0x0088F080 (:1215)
void ui_document_mp_tutorials_enter(game_state prev_state); // 0x0090E890 (:1222)
void ui_document_mp_tutorials_exit(game_state new_state); // 0x008D5BF0 (:1254)
void ui_document_mp_tutorials_process(); // 0x009157F0 (:1279)
void ui_document_mp_tutorials_handshake(rl_renderer*); // 0x008E5E90 (:1319)
void ui_document_mp_tutorials_render(rl_renderer*); // 0x008FC960 (:1327)
