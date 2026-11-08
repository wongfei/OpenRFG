// Original: volition/rfg/code/objects/humans/human_voice/human_voice_shared.h
// Implementation: volition/rfg/code/objects/humans/human_voice/human_voice_shared.cpp
//
// Voice personas shared with the tools: the typed handle of lipsync data, the persona data file
// (header, lines, variations, offsets fixed up to pointers on load) and human_voice_persona, a streamed voice set with
// its group, play status and load state (methods in human_voice.cpp and human_voice_shared.cpp). Placement of the
// handle and of play_voice_status inferred (voice_line_handle is in human_voice_internal_shared.h).
#pragma once

#include "volition/audio_lib/code/audiolib_enums.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_enums.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_internal_shared.h"
#include "volition/vlib/code/util/endian_type.h"
#include "volition/vlib/code/util/varray.h"
#include "volition/vlib/code/vlib_core/vlib_core.h"

class human;
struct human_voice_line_disable_data;
struct human_voice_persona_data_header;
class mempool;
class mempool_base;
class vector;
struct voice_lines_lookup_table;

enum lipsync_data_handle
{
    INVALID_LIPSYNC_DATA_HANDLE = -1,
    LIPSYNC_DATA_HANDLE_FORCE_TO_32_BIT = 0x7FFFFFFF,
};

enum play_voice_status
{
    PLAY_VOICE_SUCCESS = 0,
    PLAY_VOICE_FAILURE_CHANCE = 1,
    PLAY_VOICE_FAILURE_MISSING = 2,
    PLAY_VOICE_FAILURE_MISSING_VARIATIONS = 3,
    PLAY_VOICE_FAILURE_NO_AVAILABLE_VARIATIONS = 4,
    PLAY_VOICE_FAILURE_NO_VALID_CUES = 5,
    PLAY_VOICE_FAILURE_AUDIOLIB = 6,
    PLAY_VOICE_FAILURE_LOCAL_COOLDOWN = 7,
    PLAY_VOICE_FAILURE_GLOBAL_COOLDOWN = 8,
    PLAY_VOICE_FAILURE_SIMULTANEOUS = 9,
    PLAY_VOICE_FAILURE_PRIORITY = 10,
    PLAY_VOICE_FAILURE_CALLBACK = 11,
    PLAY_VOICE_FAILURE_IN_VEHICLE = 12,
    PLAY_VOICE_FAILURE_ERROR = 13,
    NUM_PLAY_VOICE_STATUS = 14,
};

// size 0xC
struct human_voice_line_variation
{
    human_voice_line_variation(); // 0x00A0DFE0 (:132)
    int get_cue_id() const;
    bool game_init(const human_voice_persona_data_header* header); // 0x00A0E060 (:218)

    int filename_or_cue_id; // 0x00
    unsigned int subtitle_crc; // 0x04
    unsigned short lipsync_data_offset; // 0x08
};

// size 0x70
struct human_voice_persona_data_header
{
    human_voice_persona_data_header(const char* new_name); // 0x00A32900 (:153)
    bool is_supported_version() const; // 0x00A0E040 (:168)
    void set_name(const char* new_name); // 0x00A0E020 (:147)
    bool game_init(); // 0x00A32950 (:304)

    unsigned int signature; // 0x00
    unsigned int version; // 0x04
    char name[64]; // 0x08
    unsigned int num_lines; // 0x48
    unsigned int num_variations; // 0x4C
    unsigned int lipsync_size; // 0x50
    et_sized_native_pointer<human_voice_persona_line> lines; // 0x58
    et_sized_native_pointer<human_voice_line_variation> variations; // 0x60
    et_sized_native_pointer<unsigned char> lipsync_data; // 0x68
};

// size 0x2C
class human_voice_persona
{
    friend void critical_area_do_frame(); // the mission warning line (inlined play in rfg.exe)
    // human::play_voice_line uses the private lookups (friends are not recorded in rfg.pdb).
    friend class human;
    // human_voice.cpp's stream callbacks and persona searches read the line data directly (inferred, as above).
    friend void human_voice_persona_stream_load(unsigned int srid);
    friend void human_voice_persona_stream_unload(unsigned int srid);
    friend human_voice_persona* human_voice_find_persona_with_line(voice_line_handle line_handle, human_teams team, bool ignore_error);
    friend human_voice_persona* human_voice_find_persona_with_line(voice_lines voice_line, human_teams team, bool ignore_error);
    friend human_voice_persona* human_voice_persona_from_name(const char* persona_name);
public:
    // size 0x4
    struct human_voice_persona_flags
    {
        unsigned int background_requested : 1; // 0x00:0
        unsigned int player_persona : 1; // 0x00:1
        unsigned int preload : 1; // 0x00:2
        unsigned int always_loaded : 1; // 0x00:3
        unsigned int multiplayer : 1; // 0x00:4
        unsigned int to_be_loaded : 1; // 0x00:5
        unsigned int to_be_unloaded : 1; // 0x00:6
        unsigned int ha_hostage_lines : 1; // 0x00:7
        unsigned int raid_lines : 1; // 0x00:8
        unsigned int raid_into_lines : 1; // 0x00:9
        unsigned int miner_persona : 1; // 0x00:10
    };

    human_voice_persona(); // 0x00A2C640 (.h:94)
    const char* get_name() const; // 0x00A057B0 (.h:96)
    human_voice_persona_groups get_voice_persona_group() const; // 0x00A057C0 (.h:97)
    bool has_line(voice_line_handle line_handle) const; // 0x00A31C40 (human_voice.cpp:3049)
    bool has_line(const char* line_name) const; // 0x00A58EB0 (human_voice.cpp:3044)
    bool has_line(voice_lines line) const; // 0x00A0D970 (human_voice.cpp:3030)
    bool can_play_line(voice_line_handle line_handle, int ms_in_future, play_voice_status* last_status) const; // 0x00A31DC0 (human_voice.cpp:3199)
    bool can_play_line(const char* line_name, int ms_in_future, play_voice_status* last_status) const; // 0x00A58F70 (human_voice.cpp:3183)
    bool can_play_line(voice_lines line, int ms_in_future, play_voice_status* last_status) const; // 0x00A31D50 (human_voice.cpp:3167)
    void reset_line_cooldown(voice_line_handle line_handle) const; // 0x00A59040 (human_voice.cpp:3224)
    void reset_line_cooldown(voice_lines line) const; // 0x00A58FC0 (human_voice.cpp:3218)
    void set_line_cooldown(voice_line_handle line_handle, int cooldown_ms) const; // 0x00A31ED0 (human_voice.cpp:3254)
    void set_line_cooldown(voice_lines line, int cooldown_ms) const; // 0x00A31E50 (human_voice.cpp:3249)
    const unsigned char* get_lipsync_data(lipsync_data_handle lipsync_handle) const; // 0x00A58EE0 (human_voice.cpp:3113)
    bool get_line_crc(voice_line_handle line_handle, unsigned int& crc); // 0x00A31D10 (human_voice.cpp:3143)
    bool can_load() const; // 0x00A31C60 (human_voice.cpp:3055)
    void background_load_request(); // 0x00A31CA0 (human_voice.cpp:3081)
    play_voice_status get_last_operation_status() const; // 0x00A057D0 (.h:132)
    bool persona_play_voice_line(voice_line_handle line_handle, const vector& pos, audiolib_cue_priority cue_priority, int& play_id, unsigned short& radio_id, lipsync_data_handle* lipsync_handle, bool delayed, bool unused, bool play_intro, bool play_outro, human* hp) const; // 0x00A591B0 (human_voice.cpp:3332)
    bool persona_play_voice_line(const char* line_name, const vector& pos, audiolib_cue_priority cue_priority, int& play_id, unsigned short& radio_id, lipsync_data_handle* lipsync_handle, bool delayed) const; // 0x00A59130 (human_voice.cpp:3301)
    bool persona_play_voice_line(voice_lines line, const vector& pos, audiolib_cue_priority cue_priority, int& play_id, unsigned short& radio_id, lipsync_data_handle* lipsync_handle, bool delayed, int delay_ms) const; // 0x00A59090 (human_voice.cpp:3270)
    bool is_loaded() const; // 0x00A0D9F0 (human_voice.cpp:3403)
    bool is_loading() const; // 0x00A0DA10 (human_voice.cpp:3422)
    bool multiplayer() const; // 0x00A057E0 (.h:153)
    unsigned int get_srid() const; // 0x00A057F0 (.h:154)
    bool is_always_loaded() const; // 0x00A0D9A0 (human_voice.cpp:3381)
    bool should_preload() const; // 0x00A0D9E0 (human_voice.cpp:3395)
    static bool parse_personas_table_shared(const char* filename, mempool_base* tmp_pool, mempool* pool, bool load_from_preload, vlib_platform plat, bool verify, bool dlc, const char* lang_suffix); // 0x00A59FD0 (:415)
    static void persona_shutdown(); // 0x00A32BB0 (:651)
    static void dlc_persona_shutdown(); // 0x00A32CB0 (:674)
    bool stream(bool load); // 0x00A83180 (human_voice.cpp:2946)
    bool request_stream(bool immediate); // 0x00A0D890 (human_voice.cpp:2857)
    bool request_destream(bool immediate); // 0x00A0D8F0 (human_voice.cpp:2914)
    bool has_been_requested() const; // 0x00A0D930 (human_voice.cpp:3011)
    bool stream_failed() const; // 0x00A0D950 (human_voice.cpp:3016)
    void reset(); // 0x00A0E180 (:384)
    void set_to_be_loaded_flag(); // 0x00A0D870 (human_voice.cpp:2827)
    void set_to_be_unloaded_flag(); // 0x00A0D880 (human_voice.cpp:2839)
    bool get_dlc() const; // 0x00A05800 (.h:196)
private:
    lipsync_data_handle get_variation_lipsync(const human_voice_line_variation* varp) const; // 0x00A58430 (human_voice.cpp:2133)
    human_voice_line_variation* get_line_variation(const human_voice_persona_line* linep, int which) const; // 0x00A31A30 (human_voice.cpp:2150)
    const human_voice_line_variation* get_next_valid_variation(human_voice_persona_line* linep) const; // 0x00A31B80 (human_voice.cpp:2257)
    const human_voice_line_variation* get_next_variation(human_voice_persona_line* linep) const; // 0x00A31C00 (human_voice.cpp:2298)
    bool persona_play_voice_line_internal(human_voice_persona_line* linep, const vector& pos, audiolib_cue_priority cue_priority, int& play_id, unsigned short& radio_id, lipsync_data_handle* lipsync_handle, int override_variation, bool delayed, int delay_ms, bool play_intro, bool play_outro, human* hp) const; // 0x00A584B0 (human_voice.cpp:2353)
    human_voice_persona_line* find_voice_line(voice_line_handle line_handle) const; // 0x00A31A90 (human_voice.cpp:2183)
    human_voice_persona_line* find_voice_line(const char* line_name) const; // 0x00A58490 (human_voice.cpp:2162)
    human_voice_persona_line* find_voice_line(voice_lines line) const; // 0x00A31A50 (human_voice.cpp:2167)
    bool can_play_line_internal(const human_voice_persona_line* linep, int ms_in_future) const; // 0x00A31AF0 (human_voice.cpp:2215)
    void set_line_cooldown_internal(human_voice_persona_line* line_ptr, int cooldown_ms) const; // 0x00A31E00 (human_voice.cpp:3235)

public:
    human_voice_persona_flags flags; // 0x00
private:
    const char* name; // 0x04
    const char* resource_filename; // 0x08
    unsigned int srid; // 0x0C
    human_voice_persona_groups group; // 0x10
    bool dlc; // 0x14
    human_voice_persona_data_header* voice_line_data; // 0x18
    human_voice_line_disable_data* voice_line_dlc_disable_data; // 0x1C
    voice_lines_lookup_table* situational_lines; // 0x20
    play_voice_status last_operation_status; // 0x24
    int reference_count; // 0x28
};

class string_pool;

// The personas of rfg_voice_personas.xtbl (and the DLC table), and the pool of their names (defined in human_voice.cpp,
// with the pool's 9,600-byte storage).
extern farray<human_voice_persona,150> Voice_personas; // 0x030F7E78
extern string_pool Persona_string_pool; // 0x031A2AB8

void human_voice_persona_offsets_to_pointers(human_voice_persona_data_header& target); // 0x00A32B10 (:343)
human_voice_persona_groups human_voice_persona_group_from_name(const char* group_name); // 0x00A0E110 (:361)
const char* human_voice_persona_group_get_name(human_voice_persona_groups group); // 0x00A0E160 (:378)
human_voice_persona* human_voice_persona_from_name(const char* persona_name); // 0x00A32B60 (:402)
