// Original: volition/audio_lib/code/audiolib_markers.h
// Implementation: volition/audio_lib/code/audiolib_markers.cpp
//
// Cue markers: marker files, the markers of a cue, and the process list that fires marker callbacks while a cue plays.
// Placement of the marker data types inferred.
#pragma once

#include "volition/audio_lib/code/audiolib_enums.h"

struct audiolib_play_data;

// size 0xC
struct audiolib_marker_node
{
    int time; // 0x00
    int offset; // 0x04
    char* text; // 0x08
};

// size 0x8
struct audiolib_marker_data
{
    int num_markers; // 0x00
    audiolib_marker_node* markers; // 0x04
};

// size 0x50
struct audiolib_marker_file_data
{
    unsigned int name_crc; // 0x00
    char name[64]; // 0x04
    long num_markers; // 0x44
    audiolib_marker_file_data* next_file; // 0x48
    audiolib_marker_node* markers; // 0x4C
};

// size 0x70
struct audiolib_marker_process_list_node
{
    bool is_valid() const;
    void invalidate(); // 0x00488390 (:89)
    void drop(); // 0x004883D0 (:135)
    audiolib_marker_process_list_node(); // 0x004883A0 (:95)

    int play_id; // 0x00
    audiolib_marker_data marker_data; // 0x04
    int extra_trigger_time; // 0x0C
    char extra_trigger_text[64]; // 0x10
    int start_time; // 0x50
    int offset_time; // 0x54
    int next_trigger_time; // 0x58
    int curr_marker_index; // 0x5C
    audiolib_marker_node* curr_marker; // 0x60
    int rolling_counter; // 0x64
    int paused_time; // 0x68
    bool paused; // 0x6C
    bool remove; // 0x6D
};

bool audiolib_marker_process_function(void* user_data, int curr_time, int time_step); // 0x0049B4C0 (:244)
audiolib_marker_file_data* audiolib_marker_internal_find_marker_data(unsigned int name_crc); // 0x00488620 (:341)
bool audiolib_marker_system_init(); // 0x0049B730 (:361)
void audiolib_marker_system_shutdown(); // 0x00488660 (:393)
audiolib_result audiolib_marker_add_marker_to_process_list(audiolib_play_data* play); // 0x00495260 (:402)
audiolib_result audiolib_marker_remove_marker_from_process_list(audiolib_play_data* play); // 0x0048FA80 (:449)
audiolib_result audiolib_marker_add_marker_to_data(audiolib_play_data* play); // 0x00495350 (:480)
audiolib_result audiolib_marker_remove_marker_data(audiolib_play_data* play); // 0x0048FAF0 (:490)
audiolib_result audiolib_marker_pause_marker_data(audiolib_play_data* play, bool pause); // 0x0048FB20 (:501)
audiolib_result audiolib_marker_get_next_marker(int play_id, int* time, int* start_time, char** text); // 0x00495380 (:543)
audiolib_result audiolib_play_add_extra_marker(int play_id, int time, const char* text); // 0x00495430 (:574)
audiolib_result audiolib_cue_get_num_markers(int cue_id, int* num_markers); // 0x00488670 (:621)
audiolib_result audiolib_cue_get_marker_info(int cue_id, int marker_index, int* marker_offset_ms, char** marker_text); // 0x004886D0 (:650)
