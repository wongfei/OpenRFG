// Original: volition/rfg/code/objects/humans/human_voice/human_voice.h
// Implementation: volition/rfg/code/objects/humans/human_voice/human_voice.cpp
//
// Human voice playback: picking a persona line and variation (cooldowns, priorities, simultaneous limits, radio or clear),
// voice slotting (which personas are streamed, persona flags and the required personas of each mission:
// All_misison_persona_lists, spelling original), the stream2 persona callbacks and the voice line disable table.
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_enums.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_internal_shared.h"
#include "volition/vlib/code/util/varray.h"

class human_voice_persona;
class mempool;
class vector;
struct xml_element;

// size 0x4
class persona_flags_struct
{
public:
    persona_flags_struct(); // 0x00A31740 (:104)
    void reset(); // 0x00A0D7A0 (:2072)
    bool slot_is_required() const; // 0x00A0D7B0 (:2078)

    unsigned int player_slot : 1; // 0x00:0
    unsigned int activity_raid : 1; // 0x00:1
    unsigned int raid_intro : 1; // 0x00:2
    unsigned int activity_ha_hostage : 1; // 0x00:3
    unsigned int miner_persona : 1; // 0x00:4
    unsigned int activity_slot : 1; // 0x00:5
    unsigned int mission_slot : 1; // 0x00:6
    unsigned int radio_slot : 1; // 0x00:7
    unsigned int pa_slot : 1; // 0x00:8
    unsigned int debug_slot : 1; // 0x00:9
    unsigned int cutscene_slot : 1; // 0x00:10
};

// size 0x14
class human_persona_information
{
public:
    human_persona_information(); // 0x00A31750 (:143)
    void reset(); // 0x00A0D7D0 (:2091)
    bool slot_is_required() const; // 0x00A319E0 (:2101)

    human_voice_persona_handle persona_handle; // 0x00
    human_voice_persona* persona; // 0x04
    int human_count; // 0x08
    persona_flags_struct persona_flags; // 0x0C
    struct
    {
        bool load_persona : 1; // 0x00:0
        bool unload_persona : 1; // 0x00:1
    } loading_flags; // 0x10
};

// size 0x14
class mission_persona_list
{
public:
    mission_persona_list(); // 0x00A31720 (:86)
    void reset(); // 0x00A0D730 (:2033)
    void add(human_voice_persona* new_persona); // 0x00A0D750 (:2042)

    human_voice_persona* mission_personas[5]; // 0x00
};

extern farray<mission_persona_list,70> All_misison_persona_lists; // 0x01694A50
extern int INVALID_VOICE_SET_HANDLE; // 0x01694400, -1 (a variable in rfg.exe)
extern const char* XML_MISSION_PERSONA_TAG_LIST[5]; // 0x0169440C, the voice_persona_<n> tags of a mission's list

void voice_slotting_do_frame(); // 0x00A58130 (:1818)
void parse_personas_flags_table(const char* filename, bool dlc_only); // 0x00A58170 (:1875)
void human_voice_line_disable_init(mempool* pool); // 0x00A83020 (:1931)
void human_voice_line_disable_shutdown(); // 0x00A319A0 (:1967)
bool human_voice_load_voice_persona_cb(const char* name, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size); // 0x00A582C0 (:1973)
void human_voice_unload_voice_persona_cb(const char* name, unsigned int srid); // 0x00A583C0 (:1997)
bool human_voice_clear_or_radio(const vector& pos); // 0x00A0D7F0 (:2315)
bool human_voice_persona_do_game_preload(bool multi, bool dlc); // 0x00A320B0 (:3534)
void human_voice_persona_level_init_pre(bool multiplayer); // 0x00A59230 (:3573)
void human_voice_system_init(); // 0x00A832A0 (:3598)
human_voice_persona_handle human_voice_radio_persona_handle(); // 0x00A0DA40 (:3625)
human_voice_persona* human_voice_radio_persona(); // 0x00A59350 (:3637)
void human_voice_system_shutdown(); // 0x00A59390 (:3653)
bool human_voice_initialized(); // 0x00A0DA60 (:3672)
void human_voice_level_init_post(); // 0x00A59410 (:3678)
void dlc_voice_persona_level_init(int dlc_id, mempool* pool); // 0x00A59490 (:3695)
void dlc_persona_mission_init(); // 0x00A32130 (:3713)
void dlc_voice_persona_shutdown_on_exit(); // 0x00A32140 (:3719)
void dlc_voice_persona_level_shutdown(); // 0x00A321D0 (:3731)
void human_voice_do_frame(); // 0x00A9D680 (:3739)
int human_voice_load_set(const farray<human_voice_persona*,150>& persona_list); // 0x00A59550 (:3754)
int human_voice_load_set(xml_element* xml_root); // 0x00A59620 (:3798)
bool human_voice_set_streamed(int set_handle); // 0x00A321F0 (:3850)
bool human_voice_load_set(int set_handle); // 0x00A59710 (:3877)
bool human_voice_activity_can_reserve(unsigned int activity_spawn_handle); // 0x00A32240 (:3914)
bool human_voice_activity_is_reserved(const human_voice_persona* desired_voice, unsigned int activity_spawn_handle); // 0x00A597C0 (:3940)
bool human_voice_activity_reserve_voice(const human_voice_persona* desired_voice, unsigned int activity_spawn_handle); // 0x00A59830 (:3974)
bool human_voice_activity_release_voice(unsigned int activity_spawn_handle); // 0x00A32290 (:4003)
bool human_voice_cutscene_reserve_slot(const human_voice_persona* desired_voice); // 0x00A598B0 (:4027)
bool human_voice_cutscene_release_voice(const human_voice_persona* reserved_voice); // 0x00A59950 (:4054)
bool human_voice_set_loaded(int set_handle); // 0x00A322E0 (:4081)
void human_voice_unload_set(int set_handle); // 0x00A32340 (:4111)
human_voice_persona_handle human_voice_persona_get_handle(const human_voice_persona* persona); // 0x00A32390 (:4129)
human_voice_persona_handle human_voice_persona_handle_from_name(const char* persona_name); // 0x00A599F0 (:4137)
human_voice_persona* human_voice_persona_from_handle(human_voice_persona_handle persona_handle); // 0x00A323C0 (:4149)
human_voice_persona* human_voice_find_persona_with_line(voice_line_handle line_handle, human_teams team, bool ignore_error); // 0x00A59A30 (:4170)
human_voice_persona* human_voice_find_persona_with_line(voice_lines voice_line, human_teams team, bool ignore_error); // 0x00A59CE0 (:4271)
bool human_voice_collect_active_personas(base_array<human_voice_persona*>& persona_list); // 0x00A59F30 (:4384)
void human_voice_persona_stream_load(unsigned int srid); // 0x00A833C0 (:4410)
void human_voice_persona_stream_unload(unsigned int srid); // 0x00A834D0 (:4424)
