// Original: volition/audio_lib/code/audiolib_music.h
// Implementation: volition/audio_lib/code/audiolib_music.cpp
//
// Interactive music: a music table (score) has states (silence, ambience, light and heavy combat, lulls) with first clips,
// exit cues and transitions; clips chain by weighted decisions at their markers. The game drives it with transitions
// and the limited level (gamemusic.cpp). Placement of the score types inferred.
#pragma once

#include "volition/audio_lib/code/audiolib_enums.h"

struct AUDIOLIB_CUE_CALLBACK_DATA;
struct audiolib_music_clip_data;
struct xml_element;

enum audiolib_music_load_state
{
    AUDIOLIB_MUSIC_LOAD_STATE_WAIT_FOR_LOAD = 0,
    AUDIOLIB_MUSIC_LOAD_STATE_LOADING = 1,
    AUDIOLIB_MUSIC_LOAD_STATE_PARSING = 2,
    AUDIOLIB_MUSIC_LOAD_STATE_READY = 3,
    AUDIOLIB_MUSIC_LOAD_STATE_PLAYING = 4,
    AUDIOLIB_MUSIC_LOAD_STATE_PAUSED = 5,
    AUDIOLIB_MUSIC_LOAD_STATE_STOPPING = 6,
    AUDIOLIB_MUSIC_LOAD_STATE_UNLOADING = 7,
    NUM_AUDIOLIB_MUSIC_LOAD_STATES = 8,
};

// size 0x10
struct audiolib_music_clip_decision_data
{
    AUDIOLIB_MUSIC_CUE_DECISION_TYPE type; // 0x00
    union
    {
        int max; // 0x04
        int percent; // 0x04
    };
    short value; // 0x08
    short count; // 0x0A
    audiolib_music_clip_data* dest_clip; // 0x0C
};

// size 0x20
struct audiolib_music_clip_data
{
    unsigned int name_crc; // 0x00
    int cue_id; // 0x04
    AUDIOLIB_MUSIC_STATE my_state; // 0x08
    audiolib_music_clip_data** lull_clips; // 0x0C
    audiolib_music_clip_data* parent_clip; // 0x10
    audiolib_music_clip_decision_data* decisions; // 0x14
    unsigned char total_weights; // 0x18
    unsigned char num_lulls; // 0x19
    unsigned char tempo; // 0x1A
    unsigned char integral_beat; // 0x1B
    unsigned char loop_count; // 0x1C
    signed char last_decision; // 0x1D
    unsigned char num_decisions; // 0x1E
    bool transition_at_next_marker; // 0x1F
};

// size 0xC
struct audiolib_music_exit_state_data
{
    AUDIOLIB_MUSIC_STATE this_state; // 0x00
    int num_decisions; // 0x04
    audiolib_music_clip_decision_data* decisions; // 0x08
};

// size 0x18
struct audiolib_music_transition_data
{
    AUDIOLIB_MUSIC_STATE this_state; // 0x00
    union
    {
        AUDIOLIB_MUSIC_STATE from_state; // 0x04
        audiolib_music_clip_data* from_clip; // 0x04
    };
    AUDIOLIB_MUSIC_TRANSITION_TYPE trans_type; // 0x08
    int num_clips; // 0x0C
    audiolib_music_clip_data* clip; // 0x10
    audiolib_music_clip_data** clips; // 0x14
};

// size 0x24
struct audiolib_music_state_data
{
    AUDIOLIB_MUSIC_STATE state_type; // 0x00
    int num_first_clips; // 0x04
    int* first_clip_usage_counts; // 0x08
    audiolib_music_clip_data** first_clips; // 0x0C
    audiolib_music_exit_state_data exit; // 0x10
    int num_transitions; // 0x1C
    audiolib_music_transition_data* transitions; // 0x20
};

// size 0x38
struct audiolib_music_score_data
{
    unsigned int name_crc; // 0x00
    AUDIOLIB_MUSIC_STATE default_first_state; // 0x04
    audiolib_music_state_data* state_data; // 0x08
    audiolib_music_clip_data* clips; // 0x0C
    AUDIOLIB_MUSIC_STATE curr_state; // 0x10
    audiolib_music_clip_data* curr_clip; // 0x14
    audiolib_music_clip_data* next_clip; // 0x18
    int curr_play_id; // 0x1C
    int next_play_id; // 0x20
    int old_clips[4]; // 0x24
    short num_clips; // 0x34
    unsigned char num_states; // 0x36
};

// size 0x94
struct audiolib_music_file_data
{
    bool is_valid() const;
    void invalidate(); // 0x00482F80 (.h:225)
    void drop(); // 0x00488760 (:250)
    audiolib_music_file_data(); // 0x00482F90 (.h:231)

    unsigned int name_crc; // 0x00
    char name[64]; // 0x04
    int music_id; // 0x44
    unsigned short action; // 0x48
    char soundbank_name[64]; // 0x4A
    short bank_id; // 0x8A
    audiolib_music_score_data* score; // 0x8C
    int rolling_counter; // 0x90
};

audiolib_result audiolib_internal_parse_music_table(audiolib_music_file_data* file, xml_element* music); // 0x0048FED0 (:707)
void audiolib_interactive_music_callback(AUDIOLIB_CUE_CALLBACK_DATA* cue_cb_data); // 0x004A0E40 (:1674)
int audiolib_internal_music_get_first_clip_index(audiolib_music_score_data* score, AUDIOLIB_MUSIC_STATE state_type); // 0x004889D0 (:1999)
void audiolib_internal_process_music_stop(audiolib_music_file_data* music); // 0x0049B840 (:2029)
bool audiolib_process_music(void* user_data, int time, int time_step); // 0x0049B920 (:2076)
bool audiolib_interactive_music_system_init(); // 0x004A5EC0 (:2225)
void audiolib_interactive_music_system_shutdown(); // 0x004A6050 (:2271)
audiolib_result audiolib_interactive_music_get_id(const char* music_file_name, int* music_id); // 0x004A6080 (:2290)
int audiolib_interactive_music_get_active_id(); // 0x00488A50 (:2300)
audiolib_result audiolib_interactive_music_load_table(const char* music_file_name, int* music_id); // 0x004A1320 (:2310)
audiolib_result audiolib_interactive_music_unload_table(int music_id); // 0x0049BFB0 (:2372)
audiolib_result audiolib_interactive_music_get_volume(float* volume); // 0x00488A70 (:2402)
audiolib_result audiolib_interactive_music_set_volume(const float volume, int fade_time_ms); // 0x00488AA0 (:2412)
audiolib_result audiolib_interactive_music_get_fade(bool* fade, float* volume); // 0x00488AE0 (:2422)
audiolib_result audiolib_interactive_music_set_fade(const float volume, int fade_time_ms); // 0x00488B30 (:2437)
audiolib_result audiolib_interactive_music_stop_fade(int fade_time_ms); // 0x00488B70 (:2447)
audiolib_result audiolib_interactive_music_start(int music_id, AUDIOLIB_MUSIC_STATE start_state, const float volume); // 0x004A60A0 (:2461)
AUDIOLIB_MUSIC_STATE audiolib_interactive_music_get_state(int music_id); // 0x00495830 (:2599)
audiolib_result audiolib_interactive_music_transition(bool reboot, int music_id, AUDIOLIB_MUSIC_STATE new_state, const float volume, bool asap); // 0x004A1470 (:2628)
audiolib_result audiolib_interactive_music_stop(int music_id, int fade_time); // 0x004A1AA0 (:3089)
void audiolib_music_play_exit_cue(); // 0x00488BB0 (:3156)
bool audiolib_interactive_music_get_pause(int music_id); // 0x00488BC0 (:3162)
bool audiolib_interactive_music_get_reboot_status(); // 0x00488BF0 (:3174)
void audiolib_interactive_music_clear_reboot(); // 0x00488C00 (:3179)
audiolib_result audiolib_interactive_music_set_pause(bool pause); // 0x0049C030 (:3184)
void audiolib_interactive_music_system_set_limited_level(bool level_set); // 0x00488C10 (:3269)
void audiolib_music_set_fsm_state(bool in_fsm); // 0x00488C20 (:3274)
bool audiolib_interactive_music_enough_time_to_fade_combat_cue(); // 0x00495890 (:3285)
