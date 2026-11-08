// Original: volition/rfg/code/hud/hud_ui_mission_briefing.h
// Implementation: volition/rfg/code/hud/hud_ui_mission_briefing.cpp
//
// The mission briefing screen (Mb_info).
#pragma once

#include "volition/interface_lib/code/vint_external.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_internal_shared.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/varray.h"

class rl_renderer;

enum mission_briefing_state
{
    MBS_TRIGGER_PLAY = 0,
    MBS_PAUSED = 1,
    MBS_PLAYING = 2,
    MBS_LOOPING_END = 3,
    MBS_LAST = 4,
};

// size 0x14
struct mission_briefing_line
{
    // Defaults: what the array constructor farray<mission_briefing_line,40> (0x008760D0) writes to each element.
    mission_briefing_line(int in_start = 0, voice_line_handle handle = INVALID_VOICE_LINE_HANDLE, human_voice_persona_handle hvp_handle = INVALID_HUMAN_VOICE_PERSONA_HANDLE, bool pause_flag = true, const color& in_col = color(255, 255, 255, 255)) // 0x00856810 (.h:98)
    {
        pause_if_prev_not_done = pause_flag;
        has_triggered = false;
        start_frame = in_start;
        line_color = in_col;
        persona_handle = hvp_handle;
        vl_handle = handle;
    }
    mission_briefing_line(int in_start, const char* in_line, const char* in_persona, bool pause_flag, const color& in_col) // 0x008567A0 (.h:79)
    {
        pause_if_prev_not_done = pause_flag;
        has_triggered = false;
        start_frame = in_start;
        line_color = in_col;
        persona_handle = INVALID_HUMAN_VOICE_PERSONA_HANDLE;
        vl_handle = INVALID_VOICE_LINE_HANDLE;
        if (in_line != nullptr)
        {
            vl_handle = voice_line_handle_from_name(in_line);
        }
        if (in_persona != nullptr)
        {
            persona_handle = human_voice_persona_handle_from_name(in_persona);
        }
    }

    bool pause_if_prev_not_done; // 0x00
    bool has_triggered; // 0x01
    int start_frame; // 0x04
    color line_color; // 0x08
    human_voice_persona_handle persona_handle; // 0x0C
    voice_line_handle vl_handle; // 0x10
};

// size 0x3E0
struct mission_briefing_line_info
{
    mission_briefing_line_info() // 0x00876F50 (.h:114)
    {
        mission_description[0] = '\0';
        mission_name[0] = '\0';
        background_music[0] = '\0';
        back_music_volume = 0.8f; // Magic numbers: the table's default volumes.
        bink_video_volume = 0.6f;
        required_personas = INVALID_VOICE_SET_HANDLE;
    }
    ~mission_briefing_line_info() = default; // 0x008695E0
    mission_briefing_line_info& operator=(const mission_briefing_line_info& __that) = default; // 0x008804A0
    mission_briefing_line_info(const mission_briefing_line_info& __that) = default;

    char mission_description[64]; // 0x00
    char mission_name[64]; // 0x40
    char background_music[32]; // 0x80
    float back_music_volume; // 0xA0
    float bink_video_volume; // 0xA4
    int required_personas; // 0xA8
    int loop_frames; // 0xAC
    farray<mission_briefing_line,40> lines; // 0xB0
};

// size 0x9D2C
struct mission_briefing_info
{
    mission_briefing_info() = default; // 0x0087C930
    ~mission_briefing_info() = default; // 0x008771C0

    bool first_play; // 0x00
    bool exiting; // 0x01
    bool loaded; // 0x02
    bool unloaded_doc; // 0x03
    bool allowing_user_input; // 0x04
    bool waiting_for_world; // 0x05
    bool district_lib; // 0x06
    int bink_handle; // 0x08
    int mempool_mark; // 0x0C
    int mempool_mark_gpu; // 0x10
    int doc_handle; // 0x14
    int data_handles[3]; // 0x18
    int total_frames; // 0x24
    int which_mission; // 0x28
    int which_mission_line_index; // 0x2C
    int user_response; // 0x30
    int loop_frames; // 0x34
    float bink_volume; // 0x38
    float background_music_volume; // 0x3C
    unsigned int control_id; // 0x40
    int input_id; // 0x44
    float bink_alpha; // 0x48
    vector2i bink_position; // 0x4C
    vector2i bink_size; // 0x54
    mission_briefing_state mb_state; // 0x5C
    mission_briefing_state mb_last_state; // 0x60
    int sound_mb_background; // 0x64
    int play_sound_loop; // 0x68
    human_voice_persona_handle curr_persona_handle; // 0x6C
    voice_line_handle curr_vl_handle; // 0x70
    bool playing_audio_line; // 0x74
    char bink_filename[256]; // 0x75
    char mission_name[64]; // 0x175
    char mission_description[64]; // 0x1B5
    char background_music[32]; // 0x1F5
    farray<mission_briefing_line_info,40> mission_line_infos; // 0x218
    int loaded_persona_set; // 0x9D28
};

extern mission_briefing_info Mb_info; // 0x02BDB180
extern const char* cMission_briefing_table; // 0x01661070

void humb_clear_all_play_trigger_flags(); // 0x00858090 (:818)
bool hud_ui_mission_briefing_init_lines(const char* tbl_file); // 0x00880E40 (:1023)
void hud_ui_mission_briefing_init(); // 0x00881260 (:1162)
void hud_ui_mission_briefing_enter(game_state prev_state); // 0x00858370 (:1183)
void hud_ui_mission_briefing_exit(game_state new_state); // 0x0084A570 (:1261)
void hud_ui_mission_briefing_process(); // 0x00858490 (:1331)
void hud_ui_mission_briefing_handshake(rl_renderer*); // 0x0084A6D0 (:1542)
void hud_ui_mission_briefing_render(rl_renderer*); // 0x0084A6E0 (:1554)
void hud_ui_mission_briefing_pause(bool pause, bool from_script); // 0x0084A720 (:1568)
bool hud_ui_mission_briefing_start(const char* mission_name, bool district_lib); // 0x008589B0 (:1605)
void hud_ui_mission_briefing_done(); // 0x0084A7A0 (:1700)
mission_briefing_line_info* hud_ui_mission_briefing_get_line_info(const char* name); // 0x00858C60 (:1718)
void hud_ui_mission_briefing_mark(); // 0x00858CC0 (:1734)
void hud_ui_mission_briefing_restore_to_mark(); // 0x008812C0 (:1739)
