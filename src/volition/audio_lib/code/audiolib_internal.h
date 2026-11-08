// Original: volition/audio_lib/code/audiolib_internal.h
//
// Shared state of the audio_lib modules: a playing sound (audiolib_play_data: params, foley entry, distances, occlusion,
// reverb, current/new volume, pitch, pan and position, stream and hardware state), the library state (Audiolib_state:
// listener, room, raycasts, frame stats), the parsed settings (Audiolib_settings: rolloff, min/max and near/far tables,
// field of view weights), the rolloff curves and the per-frame process list. The functions are the audiolib_internal_*
// ones of audiolib.cpp and those of audiolib_playlist.cpp (play state machine), audiolib_3d_calculate.cpp (volumes,
// speaker matrix, low pass and reverb) and audiolib_fov.cpp (occlusion and room size), which have no header of their
// own. Placement of the types and functions inferred.
#pragma once

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_enums.h"
#include "volition/audio_lib/code/audiolib_foley_category.h"
#include "volition/audio_lib/code/audiolib_hardware_types.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"

struct audiolib_multi_sound_info_struct;
struct encoded_string_info;
class cs_wrapper;
class mempool;

namespace keen
{
    class MemoryAllocator;
    class Mutex;

    namespace SoundSystem
    {
        struct System;
    }
}

// The emitters of a merged or public address multi-sound (rfg.pdb lists them as types of audiolib_playlist.cpp;
// audiolib_3d_calculate.cpp, earlier in the unity TU, uses them too, so they are here).
// size 0x18
struct audiolib_multi_sound_instance
{
    // 0x00489460 (audiolib_playlist.cpp)
    audiolib_multi_sound_instance()
    {
    }

    float dist; // 0x00
    float volume; // 0x04
    vector pos; // 0x08
    bool in_use; // 0x14
};
static_assert(sizeof(audiolib_multi_sound_instance) == 0x18, "audiolib_multi_sound_instance");

// size 0x184
struct audiolib_multi_sound_info_struct
{
    // 0x00490E40 (audiolib_playlist.cpp)
    audiolib_multi_sound_info_struct()
    {
    }

    bool in_use; // 0x00
    audiolib_multi_sound_instance data[16]; // 0x04
};
static_assert(sizeof(audiolib_multi_sound_info_struct) == 0x184, "audiolib_multi_sound_info_struct");

// File-local to audiolib_music.cpp in rfg.pdb; here because audiolib.cpp holds Audiolib_Music (unity TU order).
// size 0x1C
struct audiolib_interactive_music_data
{
    bool inited; // 0x00
    bool paused; // 0x01
    bool play_exit_cue; // 0x02
    int current_music_id; // 0x04
    float volume; // 0x08
    float dest_volume; // 0x0C
    float fsm_scalar; // 0x10
    int fade_time; // 0x14
    float fade_volume; // 0x18
};
static_assert(sizeof(audiolib_interactive_music_data) == 0x1C, "audiolib_interactive_music_data");

// size 0x74
struct audiolib_stream_info
{
    char bank_name[64]; // 0x00
    int stream_buffer_index; // 0x40
    int stream_buffer_length; // 0x44
    int data_remaining; // 0x48
    int loop_buffer_index; // 0x4C
    int loop_buffer_length; // 0x50
    int cache_index; // 0x54
    int current_buffer; // 0x58
    int next_buffer_length; // 0x5C
    unsigned int stall_timestamp; // 0x60
    char* track_ptr; // 0x64
    AUDIOLIB_STREAMING_DATA_LOCATION data_location; // 0x68
    unsigned int read_handle; // 0x6C
    bool loop_track : 1; // 0x70:0
    bool fetching_data : 1; // 0x70:1
};

// size 0x30C
struct audiolib_play_data
{
    void invalidate(); // 0x00489480 (audiolib_playlist.cpp:136)
    void drop(); // 0x004894B0 (audiolib_playlist.cpp:145)
    audiolib_play_data(); // 0x0048CEF0 (.h:907)

    audiolib_params params; // 0x00
    int play_id; // 0x134
    audiolib_foley_data foley_data; // 0x138
    short category_id; // 0x160
    int marker_id; // 0x164
    unsigned short action; // 0x168
    short curr_variant; // 0x16A
    AUDIOLIB_MULTICUE_TYPE multi_cue_type; // 0x16C
    short reference_count; // 0x170
    audiolib_multi_sound_info_struct* multi_info; // 0x174
    float dist; // 0x178
    float prev_dist; // 0x17C
    float dist_min; // 0x180
    float dist_max; // 0x184
    float dist_cross; // 0x188
    float dist_scale; // 0x18C
    float occlusion; // 0x190
    float doppler_factor; // 0x194
    float reverb_scale; // 0x198
    AUDIOLIB_REVERB_ROOM_TYPE reverb_type; // 0x19C
    float curr_pan; // 0x1A0
    vector curr_pos; // 0x1A4
    vector curr_orient; // 0x1B0
    vector curr_velocity; // 0x1BC
    float curr_vol; // 0x1C8
    float curr_pitch; // 0x1CC
    float curr_low_pass; // 0x1D0
    int fade_time; // 0x1D4
    int output_framecount; // 0x1D8
    float new_pan; // 0x1DC
    vector new_pos; // 0x1E0
    vector new_orient; // 0x1EC
    vector new_velocity; // 0x1F8
    float new_vol; // 0x204
    float new_pitch; // 0x208
    float new_low_pass; // 0x20C
    vector last_pos; // 0x210
    int delay_start; // 0x21C
    int cue_time; // 0x220
    int start_time; // 0x224
    int vol_fade_start; // 0x228
    int paused_time; // 0x22C
    int stalled_time; // 0x230
    int end_time; // 0x234
    short waiting_for_channel; // 0x238
    float pre_stalled_volume; // 0x23C
    float volume_scale; // 0x240
    audiolib_stream_info stream_info; // 0x244
    int hw_cue_id; // 0x2B8
    audiolib_hw_play_state hw_play_state; // 0x2BC
    audiolib_hw_specific_info hw_info; // 0x2C0
    audiolib_hw_play_state play_state; // 0x2FC
    int index; // 0x300
    int rolling_counter; // 0x304
    unsigned char loading_failures; // 0x308
    bool lpf_in_use : 1; // 0x309:0
    bool reverb_in_use : 1; // 0x309:1
    bool load_requested : 1; // 0x309:2
    bool affected_by_environ : 1; // 0x309:3
    bool affected_by_health : 1; // 0x309:4
    bool do_not_duck : 1; // 0x309:5
    bool ducking : 1; // 0x309:6
};

// size 0xAC
struct audiolib_rolloff_curve_data
{
    signed char type; // 0x00
    char name[32]; // 0x01
    unsigned int name_crc; // 0x24
    int num_entries; // 0x28
    float curve[16][2]; // 0x2C
};

// size 0x28
struct audiolib_room_dimensions
{
    AUDIOLIB_ROOM_SHAPE shape; // 0x00
    AUDIOLIB_ROOM_SIZE size; // 0x04
    AUDIOLIB_REVERB_ROOM_TYPE reverb_type; // 0x08
    float volume; // 0x0C
    float height; // 0x10
    float north_south; // 0x14
    float east_west; // 0x18
    float north_east_south_west; // 0x1C
    float north_west_south_east; // 0x20
    bool valid; // 0x24
};

// size 0x54
struct audiolib_raycast_data
{
    audiolib_raycast_data(); // 0x00483100

    vector direction; // 0x00
    float weight; // 0x0C
    int num_collisions; // 0x10
    float distances[3]; // 0x14
    vector positions[3]; // 0x20
    audiolib_material_type materials[3]; // 0x44
    bool valid; // 0x50
};

// size 0x38
struct audiolib_memory_use_stats
{
    int num_playing[2]; // 0x00
    int num_paused[2]; // 0x08
    int num_ready[2]; // 0x10
    int num_loading[2]; // 0x18
    int num_dry_dsp[2]; // 0x20
    int num_wet_dsp[2]; // 0x28
    int num_wet_dry_dsp[2]; // 0x30
};

// size 0x94
struct audiolib_gather_frame_stats
{
    int free_instances; // 0x00
    int total_waiting; // 0x04
    int waiting_on_channel; // 0x08
    int waiting_on_dist; // 0x0C
    int waiting_on_memory; // 0x10
    int loading; // 0x14
    int ready; // 0x18
    int playing; // 0x1C
    int stalling; // 0x20
    int stalled; // 0x24
    int paused; // 0x28
    int stopping; // 0x2C
    int stopped; // 0x30
    int streaming; // 0x34
    int dropped_sounds; // 0x38
    int started_at_once; // 0x3C
    int loops; // 0x40
    int alrs; // 0x44
    int engines; // 0x48
    int action_count; // 0x4C
    int cpu_usage; // 0x50
    int total_cpu_usage; // 0x54
    int mem_usage; // 0x58
    audiolib_memory_use_stats mem_stats; // 0x5C
};

// size 0xCB0
struct Audiolib_status
{
    Audiolib_status(); // 0x0048CF40

    char root_dir[256]; // 0x00
    const char* table_dir; // 0x100
    const char* data_dir; // 0x104
    const char* settings_file; // 0x108
    mempool* pool; // 0x10C
    encoded_string_info* encoded_strings; // 0x110
    unsigned int encoded_string_size; // 0x114
    bool packfile_build; // 0x118
    void* (*game_file_load_func)(char*, mempool*, void (*)(const char*, void*, unsigned char*, int), void (*)(const char*, void*)); // 0x11C
    audiolib_result (*game_file_load_status_func)(void*); // 0x120
    bool enabled; // 0x124
    audiolib_result last_error; // 0x128
    volatile bool initialized; // 0x12C
    volatile bool init_failed; // 0x12D
    volatile bool hw_inited; // 0x12E
    volatile bool hw_ready; // 0x12F
    volatile bool hw_init_failed; // 0x130
    volatile bool sounds_paused; // 0x131
    volatile bool music_paused; // 0x132
    volatile bool in_shutdown; // 0x133
    AUDIOLIB_GAME_STATE audio_game_state; // 0x134
    AUDIOLIB_SPEAKER_TYPE speaker_type; // 0x138
    int speaker_count; // 0x13C
    vector listener_pos; // 0x140
    vector listener_front_orient; // 0x14C
    vector listener_up_orient; // 0x158
    vector listener_right_orient; // 0x164
    vector listener_velocity; // 0x170
    matrix orient_matrix; // 0x17C
    bool position_valid; // 0x1A0
    float max_distance_to_keep_sounds; // 0x1A4
    short duck_id; // 0x1A8
    bool ducking; // 0x1AA
    float duck_volume; // 0x1AC
    float curr_duck_volume; // 0x1B0
    float dest_duck_volume; // 0x1B4
    int duck_fade_time; // 0x1B8
    short music_cat_id; // 0x1BC
    short system_cat_id; // 0x1BE
    short gur_radio_cat_id; // 0x1C0
    short gur_pc_cat_id; // 0x1C2
    bool in_cutscene; // 0x1C4
    float cutscene_volume; // 0x1C8
    short cut_duck_id; // 0x1CC
    float cut_duck_volume; // 0x1D0
    float volume_scalars[8]; // 0x1D4
    short low_health_cat_ids[10]; // 0x1F4
    float low_health_low_pass_adjust; // 0x208
    float low_health_volume_adjust; // 0x20C
    int num_raycasts; // 0x210
    audiolib_raycast_data listener_raycasts[26]; // 0x214
    audiolib_room_dimensions room_dimensions; // 0xA9C
    float listener_fov; // 0xAC4
    int estimated_mempool_usage; // 0xAC8
    int number_of_foleys; // 0xACC
    int number_of_variations; // 0xAD0
    int number_of_categories; // 0xAD4
    int number_of_ALRs; // 0xAD8
    int number_of_engines; // 0xADC
    int number_of_banks; // 0xAE0
    int num_soundbanks; // 0xAE4
    int music_data_size; // 0xAE8
    int max_simul_XACT_requests; // 0xAEC
    int dlc_id; // 0xAF0
    audiolib_gather_frame_stats last_frame_stats; // 0xAF4
    audiolib_gather_frame_stats frame_stats; // 0xB88
    audiolib_gather_frame_stats peak_stats; // 0xC1C
};

// size 0x1C
struct audiolib_fov_weights
{
    float up_weight; // 0x00
    float up_angle_angle; // 0x04
    float up_angle_weight; // 0x08
    float hz_weight; // 0x0C
    float dn_angle_angle; // 0x10
    float dn_angle_weight; // 0x14
    float dn_weight; // 0x18
};

// size 0x10
struct audiolib_min_max_entry
{
    float min; // 0x00
    float max; // 0x04
    float min_ext; // 0x08
    float max_ext; // 0x0C
};

// size 0x8
struct audiolib_near_far_entry
{
    int near_cue; // 0x00
    int far_cue; // 0x04
};

// size 0x1758
struct audiolib_global_settings
{
    float overall_volume; // 0x00
    float speed_of_sound; // 0x04
    float default_dist_min; // 0x08
    float default_dist_max; // 0x0C
    float default_volume; // 0x10
    int default_fade_out; // 0x14
    float doppler_scale; // 0x18
    float duck_volume; // 0x1C
    int duck_fade_time; // 0x20
    float low_pass_platform_factor; // 0x24
    float distance_lpf_min; // 0x28
    float distance_lpf_max; // 0x2C
    float distance_lpf_value_at_max; // 0x30
    float distance_lpf_min_freq; // 0x34
    int max_occlusions; // 0x38
    float occlusion_lpf_add; // 0x3C
    float occlusion_vol_adjust; // 0x40
    float occlusion_percentage; // 0x44
    float listener_angle_volume_dropoff; // 0x48
    float listener_angle_volume_min; // 0x4C
    float reverb_minimum_distance; // 0x50
    float reverb_maximum_distance; // 0x54
    float max_reverb_value; // 0x58
    int reverb_rolloff_type; // 0x5C
    float minimum_reverb_fov; // 0x60
    float minimum_wind_fov; // 0x64
    float maximum_wind_fov; // 0x68
    float maximum_wind_lowpass; // 0x6C
    float maximum_distance_fov; // 0x70
    audiolib_fov_weights fov_weights; // 0x74
    float center_speaker_scalar; // 0x90
    float low_freq_speaker_scalar; // 0x94
    float user_volume_defaults[8]; // 0x98
    bool use_extended_min_max_distances; // 0xB8
    int num_min_max_distance_entries; // 0xBC
    audiolib_min_max_entry min_max_distance_entries[200]; // 0xC0
    int num_near_far_entries; // 0xD40
    audiolib_near_far_entry near_far_entries[256]; // 0xD44
    int num_dont_duck_foleys; // 0x1544
    unsigned int dont_duck_foley_crcs[100]; // 0x1548
    float satellite_play_distance; // 0x16D8
    int num_satellite_foleys; // 0x16DC
    unsigned int satellite_foley_crcs[30]; // 0x16E0
};

// size 0x50
struct audiolib_process_data
{
    bool (*function)(void*, int, int); // 0x00
    void* user_data; // 0x04
    int last_called_time; // 0x08
    int max_time; // 0x0C
    char func_name[64]; // 0x10
};

extern Audiolib_status Audiolib_state; // 0x01A97BF0
extern audiolib_global_settings Audiolib_settings; // 0x014F79B8
extern audiolib_rolloff_curve_data* audiolib_rolloff_curve_data_list[74]; // 0x01A79960
extern object_pool<audiolib_process_data,15,1> Audio_process_list; // 0x01A7DF70

// Public globals of audio_lib_unity that rfg.pdb lists without a declaring header and more than one file uses
// (placement of these declarations inferred). Each is defined in the first file that uses it, in unity order.
extern bool Audiolib_release_assert; // 0x01A7AEEC (audiolib_callbacks.cpp)
extern volatile bool Audiolib_panicking; // 0x01A7AEEF (audiolib.cpp)
extern volatile bool Audiolib_Stream_Shutdown; // 0x01A79A8A (audiolib.cpp)
extern int audiolib_curve_list_num_curves; // 0x01A79A8C (audiolib_3d_calculate.cpp)
extern cs_wrapper Audiolib_play_cs; // 0x01A9D0E4 (audiolib_att_loop_rel.cpp)
extern cs_wrapper Audiolib_engine_cs; // 0x01A83A18 (audiolib.cpp)
extern cs_wrapper foley_hash_table_cs; // 0x01A97B20 (audiolib_foley_category.cpp)
extern keen::Mutex m_audio_mutex; // 0x01A839E0 (audiolib.cpp)
extern keen::SoundSystem::System* m_pSoundSystem; // 0x01A7AF00 (audiolib_playlist.cpp)
extern keen::MemoryAllocator* m_pAudioBankAllocator; // 0x01A7AF08 (audiolib.cpp)
extern vector audiolib_raycast_direction_vectors[26]; // 0x014F9B20 (audiolib_fov.cpp)

bool audiolib_internal_wildcard_match(char* wildcard, char* test_str); // 0x00483640 (audiolib.cpp:1297)
void audiolib_internal_adjust_parameters_for_dlc(audiolib_params* params, unsigned int crc); // 0x004836B0 (audiolib.cpp:1704)
unsigned char audiolib_internal_dont_start_percentage(audiolib_foley_data* foley_data); // 0x00483720 (audiolib.cpp:1727)
unsigned char audiolib_internal_max_simultaneous(audiolib_foley_data* foley_data); // 0x00483760 (audiolib.cpp:1748)
bool audiolib_internal_fetch_precached_wavebank_header(const char* filename, unsigned int offset, unsigned int size, unsigned char* buf); // 0x004A5BE0 (audiolib.cpp:2856)
audiolib_play_data* audiolib_internal_playstate_id_to_ptr(int play_id); // 0x00490E60 (audiolib_playlist.cpp:169)
char* audiolib_internal_debug_playid_name(int play_id); // 0x00490F20 (audiolib_playlist.cpp:226)
void audiolib_list_active_foley(bool everything, const char* why); // 0x00495980 (audiolib_playlist.cpp:260)
void audiolib_internal_playlist_pause(bool pause_sound, bool pause_music); // 0x0049C1E0 (audiolib_playlist.cpp:903)
void audiolib_internal_playlist_unpause(bool unpause_sound, bool unpause_music); // 0x0049C340 (audiolib_playlist.cpp:944)
void audiolib_internal_playlist_stop(bool stop_sound, bool stop_music); // 0x004A6360 (audiolib_playlist.cpp:969)
void audiolib_internal_playlist_stop_by_category(short category_id); // 0x00495FE0 (audiolib_playlist.cpp:1006)
bool audiolib_internal_get_callback_info(AUDIOLIB_CUE_CALLBACK_DATA* callback_event); // 0x00490FC0 (audiolib_playlist.cpp:1140)
void audiolib_internal_new_playstate_pending(audiolib_play_data* play, int time); // 0x0049C480 (audiolib_playlist.cpp:1159)
void audiolib_internal_load_request(audiolib_play_data* play, int time); // 0x004A64B0 (audiolib_playlist.cpp:1218)
void audiolib_internal_new_playstate_loaded(audiolib_play_data* play, int time); // 0x004A1E70 (audiolib_playlist.cpp:1282)
void audiolib_internal_new_playstate_preparing(audiolib_play_data* play, int time); // 0x0049C530 (audiolib_playlist.cpp:1305)
void audiolib_internal_new_playstate_ready(audiolib_play_data* play, int time); // 0x0049C580 (audiolib_playlist.cpp:1324)
void audiolib_internal_new_playstate_play(audiolib_play_data* play, int time); // 0x004A6510 (audiolib_playlist.cpp:1387)
void audiolib_internal_new_playstate_stalling(audiolib_play_data* play, int time); // 0x0049C6C0 (audiolib_playlist.cpp:1436)
void audiolib_internal_new_playstate_stalled(audiolib_play_data* play, int time); // 0x0049C770 (audiolib_playlist.cpp:1460)
void audiolib_internal_new_playstate_unstall(audiolib_play_data* play, int time); // 0x0049C890 (audiolib_playlist.cpp:1492)
void audiolib_internal_new_playstate_pause(audiolib_play_data* play, int time); // 0x0049C970 (audiolib_playlist.cpp:1532)
void audiolib_internal_new_playstate_unpause(audiolib_play_data* play, int time); // 0x004A1EF0 (audiolib_playlist.cpp:1569)
void audiolib_internal_new_playstate_stop(audiolib_play_data* play, int time); // 0x004A1FF0 (audiolib_playlist.cpp:1608)
void audiolib_internal_new_playstate_stopped(audiolib_play_data* play, int time); // 0x0049CA50 (audiolib_playlist.cpp:1641)
void audiolib_internal_new_playstate_destroy(audiolib_play_data* play, int time); // 0x0049CAC0 (audiolib_playlist.cpp:1660)
void audiolib_gather_stats_start(); // 0x00489690 (audiolib_playlist.cpp:1818)
bool audiolib_stalled(); // 0x004896E0 (audiolib_playlist.cpp:1840)
void audiolib_gather_stats_end(); // 0x0049CC90 (audiolib_playlist.cpp:1845)
void audiolib_throttle_scalar(float* current, float* desired, int num_steps, int this_step); // 0x004896F0 (audiolib_playlist.cpp:1930)
bool audiolib_process_playlist(void* user_data, int time, int time_step); // 0x004A65F0 (audiolib_playlist.cpp:2097)
bool audiolib_internal_playlist_system_init(); // 0x004A86C0 (audiolib_playlist.cpp:2767)
void audiolib_internal_playlist_system_shutdown(); // 0x004A8750 (audiolib_playlist.cpp:2791)
audiolib_result audiolib_internal_set_listener(const vector* position, const matrix* orient, const vector* velocity); // 0x00489D80 (audiolib_playlist.cpp:2824)
audiolib_play_data* audiolib_internal_playlist_alloc(audiolib_foley_data* foley_data, vector& pos, audiolib_params* params, int& multi_index); // 0x004960B0 (audiolib_playlist.cpp:2867)
int audiolib_internal_choose_variation(audiolib_foley_data* foley_data); // 0x00489EC0 (audiolib_playlist.cpp:3004)
audiolib_result audiolib_internal_playlist_get_variation_and_play_time(int play_id, int* play_time, int* variation); // 0x004911D0 (audiolib_playlist.cpp:3088)
audiolib_result audiolib_internal_playlist_get_caller_cue_id(int play_id, int* cue_id); // 0x004912B0 (audiolib_playlist.cpp:3118)
audiolib_result audiolib_internal_playlist_get_state(int play_id, audiolib_play_state* curr_state); // 0x00491330 (audiolib_playlist.cpp:3150)
audiolib_result audiolib_internal_playlist_add_emitter(int play_id, const vector* pos, const vector* orientation); // 0x00491380 (audiolib_playlist.cpp:3165)
audiolib_result audiolib_internal_playlist_get_pause(int play_id, bool* pause); // 0x004914A0 (audiolib_playlist.cpp:3207)
audiolib_result audiolib_internal_playlist_set_pause(int play_id, bool pause); // 0x004914F0 (audiolib_playlist.cpp:3219)
bool audiolib_internal_cue_is_fading(int play_id); // 0x00491600 (audiolib_playlist.cpp:3296)
audiolib_result audiolib_internal_playlist_fade_out(int play_id, const int fade_time_in_ms); // 0x00491650 (audiolib_playlist.cpp:3307)
void audiolib_internal_playlist_kill_sounds_from_bank(short bank_id); // 0x0049CF40 (audiolib_playlist.cpp:3394)
bool audiolib_internal_playlist_sounds_from_bank_killed(short bank_id); // 0x0049D060 (audiolib_playlist.cpp:3429)
audiolib_result audiolib_internal_playlist_get_volume(int play_id, float* volume); // 0x00491870 (audiolib_playlist.cpp:3461)
audiolib_result audiolib_internal_playlist_set_volume(int play_id, float volume, const int fade_time_ms); // 0x004918C0 (audiolib_playlist.cpp:3471)
audiolib_result audiolib_internal_playlist_get_low_pass(int play_id, float* low_pass); // 0x00491980 (audiolib_playlist.cpp:3498)
audiolib_result audiolib_internal_playlist_set_low_pass(int play_id, const float low_pass); // 0x004919D0 (audiolib_playlist.cpp:3507)
audiolib_result audiolib_internal_playlist_get_pitch(int play_id, float* pitch); // 0x00491A30 (audiolib_playlist.cpp:3518)
audiolib_result audiolib_internal_playlist_set_pitch(int play_id, const float pitch); // 0x00491A80 (audiolib_playlist.cpp:3528)
audiolib_result audiolib_internal_playlist_get_pan(int play_id, float* pan); // 0x00491B70 (audiolib_playlist.cpp:3555)
audiolib_result audiolib_internal_playlist_set_pan(int play_id, float pan); // 0x00491BC0 (audiolib_playlist.cpp:3565)
bool audiolib_internal_playlist_is_2d(int play_id); // 0x00491C20 (audiolib_playlist.cpp:3576)
audiolib_result audiolib_internal_playlist_change_to_2d(int play_id, const float volume); // 0x00491C60 (audiolib_playlist.cpp:3586)
audiolib_result audiolib_internal_playlist_change_to_3d(int play_id, const vector* pos, const vector* orientation, const vector* velocity); // 0x00491CC0 (audiolib_playlist.cpp:3597)
audiolib_result audiolib_internal_playlist_get_3d_position(int play_id, vector* pos, vector* orient, vector* velocity); // 0x00491D60 (audiolib_playlist.cpp:3610)
audiolib_result audiolib_internal_playlist_set_3d_position(int play_id, const vector* pos, const vector* orientation, const vector* velocity); // 0x00491E00 (audiolib_playlist.cpp:3624)
audiolib_result audiolib_internal_playlist_remove_emitter(int play_id, const vector& pos); // 0x00492020 (audiolib_playlist.cpp:3690)
audiolib_result audiolib_internal_playlist_get_audible_range(int play_id, float* min, float* max); // 0x00492130 (audiolib_playlist.cpp:3744)
audiolib_result audiolib_internal_playlist_set_audible_range(int play_id, float min, float max); // 0x00492190 (audiolib_playlist.cpp:3756)
void audiolib_internal_playlist_streaming_sound_unload(); // 0x00496450 (audiolib_playlist.cpp:3768)
int audiolib_internal_playlist_get_marker_id(int play_id); // 0x004921E0 (audiolib_playlist.cpp:3784)
unsigned short audiolib_internal_playlist_get_callback_flags(int play_id); // 0x00492220 (audiolib_playlist.cpp:3793)
void (*audiolib_internal_playlist_get_callback_function(int play_id))(AUDIOLIB_CUE_CALLBACK_DATA*); // 0x00492260 (audiolib_playlist.cpp:3802)
void* audiolib_internal_playlist_get_callback_user_data(int play_id); // 0x004922A0 (audiolib_playlist.cpp:3811)
void audiolib_internal_play_cs_enter(); // 0x0048A080 (audiolib_playlist.cpp:3820)
void audiolib_internal_play_cs_leave(); // 0x0048A090 (audiolib_playlist.cpp:3825)
audiolib_result audiolib_play_get_remaining_play_time(int play_id, int* play_time); // 0x004922E0 (audiolib_playlist.cpp:3830)
audiolib_rolloff_curve_data* audiolib_rolloff_curve_find_appropriate_curve(signed char curve_type, const char* curve_name); // 0x00483930 (audiolib_3d_calculate.cpp:354)
bool audiolib_create_rolloff_curve(signed char type, const char* name, int num_entries, float* rolloff_list); // 0x004839B0 (audiolib_3d_calculate.cpp:391)
float audiolib_calc_scaled_value(signed char curve_type, audiolib_rolloff_curve_data* curve_data, float min_dist, float max_dist, float val_at_min, float val_at_max, float distance); // 0x00483AC0 (audiolib_3d_calculate.cpp:426)
bool audiolib_calculate_speaker_volumes(audiolib_play_data* play, audiolib_channel_to_speaker_map* output_map); // 0x00483CA0 (audiolib_3d_calculate.cpp:495)
float audiolib_3d_scale_orientation_position_volume(audiolib_play_data* play); // 0x0048DD10 (audiolib_3d_calculate.cpp:665)
float audiolib_internal_calculate_base_volume(audiolib_play_data* play); // 0x0048E2F0 (audiolib_3d_calculate.cpp:862)
audiolib_result audiolib_3d_create_volume_matrix(audiolib_play_data* play, audiolib_channel_to_speaker_map* output_map); // 0x0048E770 (audiolib_3d_calculate.cpp:1011)
audiolib_result audiolib_3d_calculate(audiolib_play_data* play); // 0x004858F0 (audiolib_3d_calculate.cpp:1128)
audiolib_result audiolib_3d_setup_lowpass_and_reverb(audiolib_play_data* play); // 0x00494F20 (audiolib_3d_calculate.cpp:1189)
bool audiolib_3d_system_init(); // 0x00485A10 (audiolib_3d_calculate.cpp:1197)
void audiolib_3d_system_shutdown(); // 0x00485A60 (audiolib_3d_calculate.cpp:1213)
void audiolib_internal_setup_fov_from_settings_to_audiolib_state(); // 0x00487D00 (audiolib_fov.cpp:461)
bool audiolib_fov_system_init(); // 0x00487F70 (audiolib_fov.cpp:582)
void audiolib_fov_system_shutdown(); // 0x00487F90 (audiolib_fov.cpp:598)
audiolib_result audiolib_internal_fov_get_direction_vector(AUDIOLIB_RAYCAST_DIRECTION raycast_dir, vector* direction); // 0x00487FA0 (audiolib_fov.cpp:606)
void audiolib_internal_fov_compute_occlusion(audiolib_play_data* play); // 0x0048F280 (audiolib_fov.cpp:634)
void audiolib_internal_fov_determine_room_size_and_shape(); // 0x00487FE0 (audiolib_fov.cpp:894)
