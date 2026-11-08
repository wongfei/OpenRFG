// Original: volition/audio_lib/code/audiolib_shared_bank_header.h
//
// The foley bank file format shared with the tools: file and persona file headers, category headers, cue headers (with
// up to 8 variations, or the one variation of a persona cue) and the encoded string table of the cue names. Placement
// inferred.
#pragma once

enum vol_foley_flags
{
    VOL_FOLEY_FLAG_MAX_DIST_OVERRIDE = 1,
    VOL_FOLEY_FLAG_ALWAYS_2D = 2,
    VOL_FOLEY_FLAG_NO_PAUSE_KILL = 4,
    VOL_FOLEY_FLAG_NO_OCCLUSION = 8,
    VOL_FOLEY_FLAG_PITCH_CHANGE_OK = 16,
    VOL_FOLEY_FLAG_LOAD_WITH_PERSONA = 32,
    VOL_FOLEY_FLAG_WAVEBANK_TYPE = 64,
    VOL_FOLEY_FLAG_PS3_WAVEBANK_TYPE = 128,
};

// size 0x38
struct volition_foley_file_header
{
    int signature; // 0x00
    int platform; // 0x04
    unsigned int timestamp; // 0x08
    unsigned short version; // 0x0C
    unsigned short header_size; // 0x0E
    unsigned short num_categories; // 0x10
    unsigned int category_info_start; // 0x14
    unsigned int category_data_size; // 0x18
    unsigned short num_cues; // 0x1C
    unsigned short num_variations; // 0x1E
    unsigned short num_cues_w_1_variation; // 0x20
    unsigned int cue_info_start; // 0x24
    unsigned int cue_data_size; // 0x28
    unsigned int string_data_start; // 0x2C
    unsigned int string_data_size; // 0x30
    unsigned int total_file_size; // 0x34
};

// size 0x10
struct volition_foley_persona_file_header
{
    int signature; // 0x00
    int platform; // 0x04
    short version; // 0x08
    short num_cues; // 0x0A
    int total_file_size; // 0x0C
};

// size 0x14
struct volition_foley_category_header_info
{
    unsigned int name_offset; // 0x00
    unsigned int name_crc; // 0x04
    float emitter_cone_angle; // 0x08
    float emitter_cone_min_scale; // 0x0C
    unsigned char max_simultaneous; // 0x10
    char rolloff_type; // 0x11
    char user_volume_type; // 0x12
    unsigned char flags; // 0x13
};

// size 0x18
struct volition_foley_file_variation_output_info
{
    unsigned int name_crc; // 0x00
    unsigned int name_offset; // 0x04
    unsigned int offset; // 0x08
    unsigned int length; // 0x0C
    unsigned int play_time; // 0x10
    unsigned char flags; // 0x14
};

// size 0xE4
struct volition_foley_cue_header_output_info
{
    unsigned int name_crc; // 0x00
    unsigned int name_offset; // 0x04
    unsigned int cat_name_crc; // 0x08
    unsigned int soundbank_name_crc; // 0x0C
    unsigned int wavebank_name_crc; // 0x10
    float initial_volume; // 0x14
    unsigned short xact_soundbank_index; // 0x18
    char priority; // 0x1A
    unsigned char min_max_distance_index; // 0x1B
    unsigned char max_simultaneous; // 0x1C
    unsigned char dont_start_percentage; // 0x1D
    unsigned char loop_count; // 0x1E
    char input_channels; // 0x1F
    unsigned char num_variants; // 0x20
    unsigned char flags; // 0x21
    volition_foley_file_variation_output_info variations[8]; // 0x24
};

// size 0x30
struct volition_foley_cue_header_persona_info
{
    unsigned int name_crc; // 0x00
    unsigned int name_offset; // 0x04
    unsigned int cat_name_crc; // 0x08
    unsigned int soundbank_name_crc; // 0x0C
    unsigned int wavebank_name_crc; // 0x10
    float initial_volume; // 0x14
    unsigned short xact_soundbank_index; // 0x18
    char priority; // 0x1A
    unsigned char min_max_distance_index; // 0x1B
    unsigned char max_simultaneous; // 0x1C
    unsigned char dont_start_percentage; // 0x1D
    unsigned char loop_count; // 0x1E
    char input_channels; // 0x1F
    unsigned char flags; // 0x20
    unsigned int offset; // 0x24
    unsigned int length; // 0x28
    unsigned short play_time; // 0x2C
    unsigned char var_flags; // 0x2E
};

// size 0x120
struct encoded_string_info
{
    unsigned int data_size; // 0x00
    unsigned int encoded_string_start; // 0x04
    unsigned int encoded_string_size; // 0x08
    unsigned int string_start; // 0x0C
    unsigned int string_size; // 0x10
    unsigned short popular_lookup[128]; // 0x14
    unsigned short length_list_lengths[6]; // 0x114
};
