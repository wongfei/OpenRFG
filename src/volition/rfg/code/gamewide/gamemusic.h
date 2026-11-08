// Original: volition/rfg/code/gamewide/gamemusic.h
// Implementation: volition/rfg/code/gamewide/gamemusic.cpp
//
// Game music: the music list (from XML) maps each music to an interactive music table and per-threshold level data;
// the single player intensity (health, EDF combat, alert level) picks a threshold, which becomes an audiolib music state
// (silence, ambience, light/heavy combat and their lulls), bounded by the minimum and limited levels. Multiplayer and
// the front end play plain cues. base_activity_data holds a threshold by value. Placement of the types inferred.
#pragma once

#include "volition/audio_lib/code/audiolib_enums.h"

struct cfile;
class mempool;
class mission_info;
struct xml_element;

enum GAMEMUSIC_THRESHOLDS
{
    GAMEMUSIC_THRESHOLD_INVALID = -1,
    GAMEMUSIC_THRESHOLD_SILENCE = 0,
    GAMEMUSIC_THRESHOLD_AMBIENCE = 1,
    GAMEMUSIC_THRESHOLD_COMBAT_LIGHT_LULL = 2,
    GAMEMUSIC_THRESHOLD_COMBAT_LIGHT = 3,
    GAMEMUSIC_THRESHOLD_COMBAT_HEAVY_LULL = 4,
    GAMEMUSIC_THRESHOLD_COMBAT_HEAVY = 5,
    NUM_GAMEMUSIC_LEVEL_THRESHOLDS = 6,
};

enum GAMEMUSIC_EVENT
{
    GAMEMUSIC_PLAYER_DIED = 0,
    GAMEMUSIC_MISSION_FAILED_PLAYER_ALIVE = 1,
    GAMEMUSIC_MISSION_SUCCEEDED = 2,
    GAMEMUSIC_DISTRICT_LIBERATED = 3,
    GAMEMUSIC_UI_BLUR_END = 4,
    GAMEMUSIC_ACTIVITY_ENDED = 5,
};

enum gamemusic_intensity_factors
{
    MUSIC_INTENSITY_HEALTH_50 = 0,
    MUSIC_INTENSITY_HEALTH_30 = 1,
    MUSIC_INTENSITY_HEALTH_15 = 2,
    MUSIC_INTENSITY_EDF_1 = 3,
    MUSIC_INTENSITY_EDF_5 = 4,
    MUSIC_INTENSITY_EDF_10 = 5,
    MUSIC_INTENSITY_LIGHT_VEH_1 = 6,
    MUSIC_INTENSITY_LIGHT_VEH_2 = 7,
    MUSIC_INTENSITY_HEAVY_VEH_1 = 8,
    MUSIC_INTENSITY_AAX = 9,
    MUSIC_INTENSITY_ATACX = 10,
    MUSIC_INTENSITY_MARAUDER_1 = 11,
    MUSIC_INTENSITY_MARAUDER_5 = 12,
    MUSIC_INTENSITY_MARAUDER_10 = 13,
    MUSIC_INTENSITY_AI_ALERTED = 14,
    MUSIC_INTENSITY_AI_SEARCHING = 15,
    MUSIC_INTENSITY_ALERT_GREEN = 16,
    MUSIC_INTENSITY_ALERT_YELLOW = 17,
    MUSIC_INTENSITY_ALERT_ORANGE = 18,
    MUSIC_INTENSITY_ALERT_RED = 19,
    NUM_GAMEMUSIC_INTENSITY_FACTORS = 20,
};

enum gamemusic_music_files
{
    GAMEMUSIC_PROGRESSION_01_DAY = 0,
    GAMEMUSIC_PROGRESSION_01_NIGHT = 1,
    GAMEMUSIC_PROGRESSION_02_DAY = 2,
    GAMEMUSIC_PROGRESSION_02_NIGHT = 3,
    GAMEMUSIC_PROGRESSION_03_DAY = 4,
    GAMEMUSIC_PROGRESSION_03_NIGHT = 5,
    NUM_GAMEMUSIC_FILES = 6,
};

enum gamemusic_music_types
{
    GAMEMUSIC_MUSIC_NONE = 0,
    GAMEMUSIC_MUSIC_INTERACTIVE = 1,
    GAMEMUSIC_MUSIC_FRONTEND = 2,
    GAMEMUSIC_MUSIC_FRONTEND_MP = 3,
    GAMEMUSIC_MUSIC_MULTIPLAYER = 4,
};

// size 0x1C
struct gamemusic_level_data
{
    float volume; // 0x00
    int min_time_silence; // 0x04
    int max_time_silence; // 0x08
    int min_time_other; // 0x0C
    int max_time_other; // 0x10
    float neutral_pct; // 0x14
    float melodic_pct; // 0x18
};

// size 0x128
struct gameaudio_music_list_data
{
    char name[64]; // 0x00
    char mtbl_name[64]; // 0x40
    gamemusic_level_data levels[6]; // 0x80
};

// size 0x10
struct gameaudio_music_list
{
    int count; // 0x00
    int total; // 0x04
    int current; // 0x08
    gameaudio_music_list_data* music; // 0x0C
};

// size 0x28
struct gameaudio_music_state_info
{
    gameaudio_music_list_data* music_data; // 0x00
    int start_time; // 0x04
    int silence_time; // 0x08
    int other_time; // 0x0C
    int music_id; // 0x10
    AUDIOLIB_MUSIC_STATE state; // 0x14
    GAMEMUSIC_THRESHOLDS threshold; // 0x18
    int intensity; // 0x1C
    float volume; // 0x20
    bool active; // 0x24
};

// size 0x60
struct gameaudio_music_state
{
    gameaudio_music_state_info curr_music; // 0x00
    gameaudio_music_state_info next_music; // 0x28
    GAMEMUSIC_THRESHOLDS minimum_threshold; // 0x50
    GAMEMUSIC_THRESHOLDS limited_level; // 0x54
    bool paused; // 0x58
    int paused_time; // 0x5C
};

extern gameaudio_music_list Gameaudio_music_list; // 0x0227AF28
extern gameaudio_music_state Gamemusic_state; // 0x025111E8

AUDIOLIB_MUSIC_STATE gamemusic_convert_text_to_audiolib_state(char* text); // 0x007BE040 (:437)
void gamemusic_start_special_music(const char* music_file); // 0x007CBB30 (:459)
void gamemusic_restore_original_music(); // 0x007CBB70 (:474)
bool gamemusic_parse_music_list_init(mempool* memory_pool, int total); // 0x007BE080 (:483)
gameaudio_music_list_data* gamamusic_parse_music_list_find(const char* music_name); // 0x007BE0F0 (:506)
gameaudio_music_list_data* gameaudio_parse_music_list_add_music(const char* music_name); // 0x007BE130 (:521)
audiolib_result gamemusic_parse_music_list_item(xml_element* xml_music); // 0x007BE180 (:541)
audiolib_result gamemusic_parse_music_variable(xml_element* xml_music); // 0x007BE820 (:809)
AUDIOLIB_MUSIC_STATE gamemusic_convert_threshold_to_audiolib_state(GAMEMUSIC_THRESHOLDS threshold); // 0x007BED60 (:1106)
void gamemusic_dlc_music_init(int dlc_id); // 0x007CBE20 (:1293)
void gamemusic_dlc_music_shutdown(); // 0x007CBED0 (:1316)
gameaudio_music_list_data* gamemusic_find_music_data(char* music_name); // 0x007BEE70 (:1323)
void gamemusic_clean_music_info(gameaudio_music_state_info* music_info); // 0x007BEED0 (:1337)
void gamemusic_determine_health_intensity(); // 0x007BEF00 (:1378)
void gamemusic_determine_EDF_combat_intensity(); // 0x007D8050 (:1400)
void gamemusic_determine_alert_level_intensity(); // 0x007BEF90 (:1558)
void gamemusic_adjust_intensity(int new_intensity_count); // 0x007CBEE0 (:1583)
void gamemusic_compute_current_intensity_single_player(); // 0x007D8310 (:1602)
void gamemusic_set_current_music(int curr_time, bool game_paused); // 0x007CBF70 (:1689)
int multiplayer_select_cue_number(bool edf); // 0x007BF0B0 (:2122)
void multiplayer_music_play_cue(); // 0x007BF100 (:2149)
void multiplayer_music_update(); // 0x007BF1B0 (:2165)
void multiplayer_music_stop(); // 0x007BF1F0 (:2181)
void frontend_music_update(); // 0x007BF220 (:2189)
void frontend_music_stop(int fade_time_ms); // 0x007BF450 (:2258)
void gamemusic_multiplayer_start(); // 0x007CC750 (:2270)
void gamemusic_multiplayer_end(); // 0x007BF4A0 (:2285)
void gamemusic_frontend_start(bool multiplayer); // 0x007CC7F0 (:2297)
void gamemusic_frontend_stop(int fade_time_ms); // 0x007BF4E0 (:2317)
void gamemusic_interactive_stop(); // 0x007CC840 (:2324)
void gamemusic_update(bool game_paused); // 0x007D83C0 (:2341)
bool gamemusic_system_init(mempool* memory_pool); // 0x007D8490 (:2425)
void gamemusic_system_shutdown(); // 0x007BF530 (:2499)
GAMEMUSIC_THRESHOLDS game_music_get_index_from_name(const char* name); // 0x007BF540 (:2510)
bool gamemusic_start(char* music_name); // 0x007BF580 (:2527)
bool gamemusic_stop(); // 0x007BF5B0 (:2541)
GAMEMUSIC_THRESHOLDS gamemusic_get_minimum_music_level(); // 0x007BF610 (:2558)
void gamemusic_set_minimum_music_level(GAMEMUSIC_THRESHOLDS level); // 0x007BF620 (:2563)
GAMEMUSIC_THRESHOLDS gamemusic_get_limited_music_level(); // 0x007BF650 (:2585)
void gamemusic_set_limited_music_level(GAMEMUSIC_THRESHOLDS level); // 0x007BF660 (:2589)
void gamemusic_mission_canceled(); // 0x007BF720 (:2655)
void gamemusic_mission_completed(mission_info* mission); // 0x007D8630 (:2678)
void gamemusic_system_load(cfile* file); // 0x007CC870 (:2710)
void gamemusic_system_save(cfile* file); // 0x007BF790 (:2721)
void gamemusic_event_occurred(GAMEMUSIC_EVENT event); // 0x007BF7B0 (:2726)
void unused_dcf_music_volume(); // 0x007BF840 (:2776)
void unused_dcf_music_minimum(); // 0x007BF8F0 (:2796)
void unused_dcf_music_limited(); // 0x007BF9E0 (:2827)
