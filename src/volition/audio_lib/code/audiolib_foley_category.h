// Original: volition/audio_lib/code/audiolib_foley_category.h
// Implementation: volition/audio_lib/code/audiolib_foley_category.cpp
//
// Foley cues and categories: the cue table (Audiolib_foley_list, looked up through the name hash foley_hash_table), the
// foley banks of the voice personas, the categories with their volume, mute, pause, ducking, instance limits and emitter
// cones (Audiolib_category_list), and the foley/category parse. Placement of the types inferred.
#pragma once

#include "keen/base/language.hpp"
#include "volition/audio_lib/code/audiolib_enums.h"

struct audiolib_play_data;
struct encoded_string_info;
struct volition_foley_cue_header_output_info;
struct volition_foley_cue_header_persona_info;

// size 0x4
struct audiolib_variation_info
{
    unsigned int play_time; // 0x00
};

// size 0x28
struct audiolib_foley_data
{
    unsigned int name_crc; // 0x00
    unsigned char variation_usage[4]; // 0x04
    union
    {
        audiolib_variation_info* variation_list; // 0x08
        audiolib_variation_info one_variation; // 0x08
    };
    int internal_cue_id; // 0x0C
    short soundbank_id; // 0x10
    short wavebank_id; // 0x12
    short category_id; // 0x14
    unsigned char initial_volume; // 0x16
    unsigned char min_max_index; // 0x17
    unsigned char dont_start_percentage; // 0x18
    unsigned char priority : 4; // 0x19:0
    unsigned char max_simultaneous : 4; // 0x19:4
    unsigned char curr_simul : 4; // 0x1A:0
    int last_cue_time; // 0x1C
    unsigned int num_variants : 4; // 0x20:0
    unsigned int last_variation : 4; // 0x20:4
    unsigned int input_channels : 4; // 0x20:8
    unsigned char wavebank_type : 1; // 0x24:0
    bool pitch_changes_allowed : 1; // 0x24:1
    bool didnt_start_last_time : 1; // 0x24:2
    bool max_dist_override : 1; // 0x24:3
    bool always_2d : 1; // 0x24:4
    bool do_not_kill_when_pausing : 1; // 0x24:5
    bool do_not_apply_occlusion : 1; // 0x24:6
    bool looping : 1; // 0x24:7
    bool near_far_variant : 1; // 0x25:0
    bool dont_duck : 1; // 0x25:1
    bool allow_satellite : 1; // 0x25:2
    bool in_use : 1; // 0x25:3
};

// size 0x38
struct audiolib_foley_bank_data
{
    char persona_filename[32]; // 0x00
    unsigned char* source_data; // 0x20
    int source_data_size; // 0x24
    int start_index; // 0x28
    int num_cues; // 0x2C
    audiolib_foley_data* start; // 0x30
    bool in_use; // 0x34
};

// size 0xF6C
struct audiolib_foley_list
{
    int total_cues; // 0x00
    int variation_count; // 0x04
    int total_variations; // 0x08
    int total_size; // 0x0C
    short total_banks; // 0x10
    audiolib_foley_bank_data persona_banks[64]; // 0x14
    audiolib_foley_bank_data persona_PCM01_bank; // 0xE14
    audiolib_foley_bank_data persona_OFFCAM_bank; // 0xE4C
    audiolib_foley_bank_data persona_RFC_bank; // 0xE84
    audiolib_foley_bank_data persona_SAFE_bank; // 0xEBC
    audiolib_foley_bank_data persona_HANDBOOK_bank; // 0xEF4
    audiolib_foley_bank_data persona_TUTORIAL_bank; // 0xF2C
    audiolib_foley_data* foley; // 0xF64
    audiolib_variation_info* variations; // 0xF68
};

// size 0x24
struct audiolib_category_data
{
    unsigned int name_crc; // 0x00
    short cat_id; // 0x04
    float emitter_cone_angle; // 0x08
    float emitter_cone_min_scale; // 0x0C
    float curr_volume; // 0x10
    float dest_volume; // 0x14
    int fade_time; // 0x18
    signed char rolloff_type; // 0x1C
    signed char user_volume_type; // 0x1D
    unsigned char max_simultaneous; // 0x1E
    unsigned char curr_count; // 0x1F
    bool mute : 1; // 0x20:0
    bool pause : 1; // 0x20:1
    bool ducking : 1; // 0x20:2
    bool merging_ok : 1; // 0x20:3
    bool affected_by_environ : 1; // 0x20:4
};

// size 0xC
struct audiolib_category_list
{
    int count; // 0x00
    int total; // 0x04
    audiolib_category_data* category; // 0x08
};

// size 0xC
struct audiolib_encoded_string_hash_entry
{
    audiolib_encoded_string_hash_entry* next; // 0x00
    unsigned int crc; // 0x04
    int cue_id; // 0x08
};

// size 0x1010
struct audiolib_encoded_string_hash_table
{
    int number_of_entries; // 0x00
    int number_of_slots; // 0x04
    audiolib_encoded_string_hash_entry* free_entries; // 0x08
    char* entry_ram; // 0x0C
    audiolib_encoded_string_hash_entry* slots[1024]; // 0x10
};

extern audiolib_foley_list Audiolib_foley_list; // 0x01A789E8
extern audiolib_category_list Audiolib_category_list; // 0x01A79954
extern audiolib_encoded_string_hash_table foley_hash_table; // 0x01A79ED0

void audiolib_set_game_language_hack(keen::LanguageId audio_lang); // 0x00486060 (:228)
short audiolib_foley_try_find_soundbank_id(unsigned int original_crc, short expected_bank_type); // 0x0049A110 (:233)
bool audiolib_foley_hash_table_init(int num_cues); // 0x00486070 (:353)
bool audiolib_foley_hash_table_insert(unsigned int string_crc, int cue_id, unsigned int encoded_offset); // 0x00486110 (:394)
bool audiolib_foley_hash_table_insert(const char* string, int cue_id, unsigned int encoded_offset); // 0x004861B0 (:429)
bool audiolib_foley_hash_table_set(unsigned int crc, int cue_id); // 0x004861D0 (:436)
bool audiolib_foley_hash_table_remove(unsigned int crc); // 0x00486260 (:457)
int audiolib_foley_hash_table_get(unsigned int crc, unsigned char** encoded_offset); // 0x00486330 (:510)
char* audiolib_internal_debug_foley_name(unsigned int crc); // 0x0048F220 (:549)
bool audiolib_string_library_decode_string(encoded_string_info* string_library, unsigned int name_crc, char* decoded_string); // 0x004863D0 (:559)
audiolib_foley_data* audiolib_get_foley_data_pointer(int caller_cue_id); // 0x00486400 (:678)
audiolib_category_data* audiolib_get_category_data_pointer(short caller_cat_id); // 0x00486640 (:774)
bool audiolib_internal_category_list_init(int total); // 0x00486670 (:785)
audiolib_category_data* audiolib_internal_category_find(unsigned int category_name_crc); // 0x00486700 (:813)
audiolib_category_data* audiolib_internal_add_category(unsigned int category_name_crc, unsigned int encoded_offset); // 0x00486740 (:829)
audiolib_category_data* audiolib_internal_add_category(const char* category_name, unsigned int encoded_offset); // 0x00486810 (:854)
bool audiolib_internal_foley_list_init(int total_cues, int total_variations); // 0x00486830 (:873)
int audiolib_internal_foley_get_number_of_cues(); // 0x004868F0 (:917)
audiolib_foley_data* audiolib_internal_get_foley(unsigned int name_crc); // 0x00486900 (:922)
audiolib_foley_data* audiolib_internal_foley_change_cue_id(unsigned int name_crc, int cue_id); // 0x00486920 (:930)
audiolib_foley_data* audiolib_internal_foley_get_cue_by_index(int index); // 0x00486940 (:936)
audiolib_variation_info* audiolib_internal_get_variation_pointer(audiolib_foley_data* foley, int variant); // 0x00486980 (:950)
audiolib_variation_info* audiolib_internal_get_variation_pointer(audiolib_play_data* play, int variant); // 0x004869B0 (:967)
audiolib_foley_data* audiolib_internal_find_or_add_foley(int index, unsigned int foley_name_crc, unsigned int encoded_offset, bool must_add); // 0x004869F0 (:978)
audiolib_foley_data* audiolib_internal_find_or_add_foley(int index, const char* name, unsigned int encoded_offset, bool must_add); // 0x00486AF0 (:1012)
short audiolib_internal_get_category_id(unsigned int name_crc); // 0x00486B10 (:1025)
short audiolib_internal_get_category_id(const char* category_name); // 0x00486B30 (:1034)
audiolib_result audiolib_internal_get_category_mute(short category_id, bool* mute); // 0x00486B70 (:1043)
audiolib_result audiolib_internal_set_category_mute(short category_id, bool mute); // 0x00486BE0 (:1056)
audiolib_result audiolib_internal_get_category_pause(short category_id, bool* pause); // 0x00486C50 (:1069)
audiolib_result audiolib_internal_set_category_pause(short category_id, bool pause); // 0x00486CA0 (:1085)
audiolib_result audiolib_internal_get_category_volume(short category_id, float* volume, bool include_ducking, audiolib_play_data* play); // 0x00486D00 (:1098)
audiolib_result audiolib_internal_set_category_volume(short category_id, float volume); // 0x00486DE0 (:1138)
audiolib_result audiolib_internal_category_stop(short category_id); // 0x0049A290 (:1154)
bool audiolib_internal_ok_to_start_category_instance(short category_id); // 0x00486E60 (:1167)
bool audiolib_internal_category_instance_ended(short category_id); // 0x00486EC0 (:1187)
bool audiolib_category_is_merged_ok(short cat_id); // 0x00486F10 (:1204)
audiolib_result audiolib_category_set_user_category_volumes(signed char id, float volume, bool do_it_now); // 0x00486F70 (:1222)
bool audiolib_foley_category_process(void* user_data, int time, int time_step); // 0x00486FD0 (:1243)
audiolib_result audiolib_internal_category_duck(short category_id); // 0x004872D0 (:1333)
audiolib_result audiolib_internal_category_duck_off(); // 0x004873E0 (:1375)
bool audiolib_internal_category_get_duck(short category_id); // 0x00487440 (:1387)
bool audiolib_internal_category_affected_by_low_health(short category_id); // 0x00487460 (:1398)
bool audiolib_internal_category_affected_by_environ(short category_id); // 0x00487490 (:1413)
bool audiolib_internal_foley_get_pitch_min_max(audiolib_foley_data* foley_data, float* min_pitch, float* max_pitch); // 0x004874D0 (:1422)
bool audiolib_foley_category_system_init(); // 0x0049A2D0 (:1453)
void audiolib_foley_category_system_shutdown(); // 0x004A0980 (:1470)
audiolib_foley_data* audiolib_parse_foley_find(unsigned int foley_name_crc); // 0x00487510 (:1475)
audiolib_foley_data* audiolib_parse_foley_find(const char* foley_name); // 0x00487530 (:1485)
audiolib_result audiolib_unparse_foley_persona_data(audiolib_foley_bank_data* foley_bank, int cues_in_bank); // 0x00487550 (:1502)
bool audiolib_internal_foley_dont_duck(unsigned int name_crc); // 0x00487660 (:1526)
bool audiolib_internal_foley_satellite(unsigned int name_crc); // 0x00487690 (:1540)
audiolib_result audiolib_parse_foley_persona_data(volition_foley_cue_header_persona_info* source_data, int start_index, int num_cues); // 0x0049A330 (:1554)
audiolib_result audiolib_parse_foley_data(volition_foley_cue_header_output_info* source_data, int start_index, int num_cues); // 0x0049A610 (:1737)
audiolib_result audiolib_load_category_and_foley_data(bool reload); // 0x0049AA00 (:1975)
bool audiolib_internal_foley_get_name_by_cue_id(int cue_id, char* output, int output_size); // 0x004876C0 (:2393)
bool audiolib_internal_cue_is_valid(int internal_cue_id); // 0x0049B040 (:2413)
audiolib_foley_data* audiolib_internal_foley_get_data_by_cue_id(int caller_cue_id); // 0x00487780 (:2433)
float audiolib_internal_get_foley_vol(int caller_cue_id); // 0x004877A0 (:2445)
signed char audiolib_get_rolloff_curve_from_category_id(short cat_id); // 0x004877E0 (:2458)
bool audiolib_internal_category_get_emitter_cone(short cat_id, float* min_angle, float* min_scale); // 0x00487800 (:2464)
audiolib_result audiolib_foley_bank_register(const char* filename, unsigned char* data, int data_size); // 0x0049B0C0 (:2485)
audiolib_result audiolib_foley_bank_unregister(const char* filename); // 0x00487870 (:2691)
