// Original: volition/audio_lib/code/audiolib_att_loop_rel.h
// Implementation: volition/audio_lib/code/audiolib_att_loop_rel.cpp
//
// Attack/loop/release sounds (automatic weapons, engines): an ALR plays an attack cue, loops, and plays a release cue on
// stop, with near and far variants (audiolib_alr_play_type). The lookup table comes from XML. Placement of the lookup
// types inferred.
#pragma once

#include "volition/audio_lib/code/audiolib_enums.h"
#include "volition/vlib/code/math/vector3.h"

struct xml_element;

enum audiolib_alr_indexes
{
    AUDIOLIB_ALR_INDEX_NEAR_ATTACK = 0,
    AUDIOLIB_ALR_INDEX_NEAR_LOOP = 1,
    AUDIOLIB_ALR_INDEX_NEAR_RELEASE = 2,
    AUDIOLIB_ALR_INDEX_FAR_ATTACK = 3,
    AUDIOLIB_ALR_INDEX_FAR_LOOP = 4,
    AUDIOLIB_ALR_INDEX_FAR_RELEASE = 5,
    AUDIOLIB_NUM_ALR_INDEXES = 6,
};

enum audiolib_alr_play_type
{
    AUDIOLIB_ALR_TYPE_NEAR = 0,
    AUDIOLIB_ALR_TYPE_BOTH = 1,
    AUDIOLIB_ALR_TYPE_FAR = 2,
};

// size 0x3C
struct audiolib_alr_params
{
    audiolib_alr_params(); // 0x00485CD0

    int attack_cue_id; // 0x00
    int loop_cue_id; // 0x04
    int release_cue_id; // 0x08
    int far_attack_cue_id; // 0x0C
    int far_loop_cue_id; // 0x10
    int far_release_cue_id; // 0x14
    int repeat_rate; // 0x18
    int num_shots; // 0x1C
    bool is_3d; // 0x20
    vector pos; // 0x24
    vector orient; // 0x30
};

// size 0x1FC
struct audiolib_alr_lookup_data
{
    char name[64]; // 0x00
    char attack_name[64]; // 0x40
    char loop_name[64]; // 0x80
    char release_name[64]; // 0xC0
    char far_attack_name[64]; // 0x100
    char far_loop_name[64]; // 0x140
    char far_release_name[64]; // 0x180
    audiolib_alr_params params; // 0x1C0
};

// size 0x60
struct audiolib_alr_data
{
    bool is_valid() const;
    void invalidate(); // 0x00482F50 (.h:76)
    void drop(); // 0x00485AD0 (:97)
    audiolib_alr_data(); // 0x00482F60 (.h:82)

    int alr_id; // 0x00
    int cue_ids[6]; // 0x04
    int play_ids[6]; // 0x1C
    int num_shots; // 0x34
    int repeat_rate; // 0x38
    float dist; // 0x3C
    float min; // 0x40
    float max; // 0x44
    audiolib_alr_play_type type; // 0x48
    bool is_3d; // 0x4C
    bool kill_me; // 0x4D
    int fade_time; // 0x50
    int start_time; // 0x54
    int valid; // 0x58
    int rolling_counter; // 0x5C
};

audiolib_alr_data* audiolib_internal_alr_id_to_ptr(int id); // 0x0048EB90 (:108)
bool audiolib_internal_alr_list_init(int total); // 0x00485AF0 (:169)
audiolib_alr_lookup_data* audiolib_internal_find_alr_by_name(const char* name); // 0x00485B70 (:194)
audiolib_result audiolib_internal_parse_alr(xml_element* alr_xml_info); // 0x0048ED90 (:206)
audiolib_result audiolib_internal_alr_load_alr_table(); // 0x0048EFB0 (:289)
audiolib_alr_lookup_data* audiolib_internal_lookup_alr_params_in_list(int cue_id); // 0x00485BF0 (:322)
bool audiolib_alr_thread_function(void* user_data, int curr_time, int time_step); // 0x00499BC0 (:350)
bool audiolib_alr_system_init(); // 0x0049FE40 (:398)
void audiolib_alr_system_shutdown(); // 0x0049FF00 (:419)
int audiolib_alr_get_current_alr_count(); // 0x00494F50 (:430)
bool audiolib_att_loop_rel_is_valid(int cue_id); // 0x00485C60 (:435)
audiolib_result audiolib_att_loop_rel_get_cue_id(const char* alr_name, int* cue_id); // 0x00485C80 (:450)
audiolib_result audiolib_att_loop_rel_start(audiolib_alr_params* alr_info, int* alr_id); // 0x0049FF90 (:469)
audiolib_result audiolib_att_loop_rel_start(int cue_id, int* alr_id, bool skip_attack_sound, const vector* pos, const vector* orient); // 0x004A04C0 (:732)
audiolib_result audiolib_att_loop_rel_start(const char* alr_name, int* alr_id, bool skip_attack_sound, const vector* pos, const vector* orient); // 0x004A0680 (:806)
audiolib_result audiolib_att_loop_rel_set_pos(int alr_id, const vector* pos, const vector* orient); // 0x00494F60 (:878)
audiolib_result audiolib_att_loop_rel_stop(int alr_id, const int fade_time_ms); // 0x0048F0D0 (:902)
void audiolib_internal_att_loop_rel_kill_all(); // 0x00495020 (:934)
