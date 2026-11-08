// Original: volition/audio_lib/code/audiolib_vehicle_engine.h
// Implementation: volition/audio_lib/code/audiolib_vehicle_engine.cpp
//
// Vehicle engine sounds: an engine (from the engine table) cross-fades on-load and off-load clips by RPM with volume
// envelopes, plus a damage cue. Placement of the engine table types inferred.
#pragma once

#include "volition/audio_lib/code/audiolib_enums.h"
#include "volition/vlib/code/math/vector3.h"

struct audiolib_rolloff_curve_data;

// size 0x24
struct audiolib_vehicle_engine_rpm_data
{
    int cue_id; // 0x00
    int mono_cue_id; // 0x04
    float min_rpm; // 0x08
    float max_rpm; // 0x0C
    float vol_min; // 0x10
    float vol_max; // 0x14
    float pitch_min; // 0x18
    float pitch_max; // 0x1C
    unsigned char curve_type; // 0x20
    bool pitch_ok : 1; // 0x21:0
    bool mono_pitch_ok : 1; // 0x21:1
    bool parsed_set : 1; // 0x21:2
};

// size 0x4
struct audiolib_vehicle_engine_envelope
{
    float aaa; // 0x00
};

// size 0x220
struct audiolib_vehicle_engine_data
{
    char engine_name[64]; // 0x00
    char soundbank_name[64]; // 0x40
    char wavebank_name[64]; // 0x80
    short soundbank_id; // 0xC0
    short wavebank_id; // 0xC2
    AUDIOLIB_VEHICLE_ENGINE_PLAYBACK_TYPE playback_type; // 0xC4
    float idle_rpm; // 0xC8
    float max_rpm; // 0xCC
    audiolib_vehicle_engine_rpm_data onload[4]; // 0xD0
    audiolib_vehicle_engine_rpm_data offload[4]; // 0x160
    audiolib_vehicle_engine_envelope onload_env[4]; // 0x1F0
    audiolib_vehicle_engine_envelope offload_env[4]; // 0x200
    int damage_cue_id; // 0x210
    int damage_mono_cue_id; // 0x214
    float damage_vol_scale; // 0x218
    int idle_start_delay_ms; // 0x21C
};

// size 0xC
struct audiolib_veh_eng_clip_data
{
    int play_id; // 0x00
    float vol; // 0x04
    union
    {
        float pitch; // 0x08
        float lowpass; // 0x08
    };
};

// size 0xB4
struct audiolib_vehicle_engine_instance
{
    bool is_valid() const; // 0x00483010 (.h:189)
    void invalidate(); // 0x00483020 (.h:190)
    void drop(); // 0x00483050 (.h:197)
    audiolib_vehicle_engine_instance(); // 0x00483090 (.h:207)

    int engine_id; // 0x00
    audiolib_vehicle_engine_data* engine_data; // 0x04
    union
    {
        float speed; // 0x08
        float fov; // 0x08
    };
    int wait_to_start; // 0x0C
    float curr_rpm; // 0x10
    float dest_rpm; // 0x14
    float curr_onload; // 0x18
    float dest_onload; // 0x1C
    float curr_damage; // 0x20
    float dest_damage; // 0x24
    int damage_loop_play_id; // 0x28
    audiolib_rolloff_curve_data* onload_curve_data; // 0x2C
    audiolib_rolloff_curve_data* offload_curve_data; // 0x30
    bool is_3d : 1; // 0x34:0
    bool switch_to_3d : 1; // 0x34:1
    bool switch_to_2d : 1; // 0x34:2
    vector curr_pos; // 0x38
    vector curr_orient; // 0x44
    audiolib_veh_eng_clip_data onload[4]; // 0x50
    audiolib_veh_eng_clip_data offload[4]; // 0x80
    int rolling_counter; // 0xB0
};

bool audiolib_internal_engine_list_init(int total); // 0x0048A320 (:103)
int audiolib_internal_find_engine_cue_id_by_name(const char* name); // 0x0048A3A0 (:128)
audiolib_vehicle_engine_data* audiolib_internal_find_engine_by_cue_id(int cue_id); // 0x0048A470 (:146)
audiolib_result audiolib_vehicle_engine_parse_xtbl(const char* filename); // 0x00498080 (:609)
audiolib_result audiolib_vehicle_volume_delay(unsigned int time_ms); // 0x0048A500 (:720)
bool audiolib_process_vehicle_engines(void* user_data, int time, int time_step); // 0x004A2B10 (:727)
bool audiolib_internal_vehicle_engine_system_init(); // 0x004A5210 (:1456)
void audiolib_internal_vehicle_engine_system_shutdown(); // 0x004A52D0 (:1490)
int audiolib_internal_vehicle_engine_get_num_active(); // 0x004981A0 (:1507)
void audiolib_internal_vehicle_engine_kill_all(); // 0x004A5320 (:1512)
void audiolib_internal_vehicle_engine_kill_with_extreme_prejudice(short bank_id); // 0x004A53D0 (:1524)
int audiolib_vehicle_engine_get_id(const char* engine_name); // 0x0048A520 (:1550)
bool audiolib_vehicle_engine_is_id_valid(int engine_id); // 0x0048A540 (:1569)
short audiolib_vehicle_engine_get_wavebank_id(int engine_id); // 0x0048A560 (:1587)
int audiolib_vehicle_engine_start(int engine_id, const vector* position, const vector* orient); // 0x004A54B0 (:1608)
audiolib_result audiolib_vehicle_engine_get_state(int engine_inst, AUDIOLIB_VEHICLE_ENGINE_STATE* engine_state); // 0x004981B0 (:1794)
bool audiolib_vehicle_engine_is_playing(int engine_inst); // 0x00498240 (:1819)
audiolib_result audiolib_vehicle_engine_audio_to_3d(int engine_inst, vector* position, vector* orient); // 0x00492690 (:1849)
audiolib_result audiolib_vehicle_engine_audio_to_2d(int engine_inst); // 0x00492730 (:1882)
audiolib_result audiolib_vehicle_engine_set_pos(int engine_inst, vector* position, vector* orient); // 0x00492790 (:1911)
audiolib_result audiolib_vehicle_engine_get_RPM(int engine_inst, float* rpm); // 0x00492810 (:1944)
audiolib_result audiolib_vehicle_engine_set_RPM(int engine_inst, float speed, float rpm, float load, float damage); // 0x00492860 (:1973)
audiolib_result audiolib_vehicle_engine_stop(int engine_inst); // 0x0049DFE0 (:2022)
bool audiolib_internal_check_sound_used_in_engine(int id); // 0x004982B0 (:2061)
