// Original: volition/rfg/code/gamewide/gamemusic.cpp
//
// The game music (SFX.md SX5). Three kinds, by gamemusic_current_music_type:
// - the front end: the title theme suite (MUS_TITLE_THEME_SUITE_1-3, one at random, again when it ends) or, after a
//   multiplayer match, MUS_MP_THEME_SUITE_1;
// - multiplayer: one of the five combat tracks of the player's team (MUS_MP_EDF/GUR_COMBAT_MEDIUM_0n, never the same
//   twice in a row), again when it ends; from entering a level until the match's end (gamemusic_multiplayer_end);
// - single player interactive music: audio_lib's interactive music on the progression's music table (rfg_music.xtbl:
//   per music its .mtbl and per threshold the level data), driven by an intensity from the player's health, the EDF
//   and marauders targeting him and the alert level (rfg_music_variables.xtbl), bounded by the minimum and limited
//   levels, with the safehouse, death, night/day and marauder-zone special cases.
// The tables are parsed at init (gameaudio_system_init_stage_2); gamemusic_update runs each frame (gameplay, loading
// and the main menu).
//
// rfg.exe compiles gameaudio.cpp, gamemusic.cpp and gamewide.cpp in one unity TU, with gamestate_gameplay.cpp's statics
// next to them. This file is a TU of rfg_audio, so the statics it shares with gamestate_gameplay.cpp
// (frontend_music_play_id, frontend_mp_music_play_id, gamemusic_current_music_type, Game_clock) and gamewide.cpp
// (multiplayer_music_play_id, gamemusic_forced_stop) have external linkage there.
//
// The single player path (the interactive music state machine, gamemusic_set_current_music, final_push.md step 6) is
// only reached in single player.
// The external interface is declared in gamemusic.h.

#include "volition/rfg/code/gamewide/gamemusic.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_internal.h"
#include "volition/audio_lib/code/audiolib_music.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_safehouse.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- the unity TU's statics defined elsewhere (see the file comment)
extern int frontend_music_play_id; // 0x0165CB2C, gamestate_gameplay.cpp
extern int frontend_mp_music_play_id; // 0x0165CB70, gamestate_gameplay.cpp
extern gamemusic_music_types gamemusic_current_music_type; // 0x0165CB78, gamestate_gameplay.cpp
extern game_clock Game_clock; // 0x025054AC, gamestate_gameplay.cpp
extern int multiplayer_music_play_id; // 0x0165CB74, gamewide.cpp
extern bool gamemusic_forced_stop; // 0x0227AFC0, gamewide.cpp
extern bool gamemusic_shutdown; // 0x0253384B, gameaudio.cpp

// ---- public globals (rfg.pdb; no header declares them but gamemusic.h's two)
gameaudio_music_list Gameaudio_music_list; // 0x0227AF28
gameaudio_music_state Gamemusic_state; // 0x025111E8
const char* Gamemusic_Progression_File_Names[6] = { // 0x0165CB14
    "MUS_PROGRESSION_01_DAY", "MUS_PROGRESSION_01_NIGHT", "MUS_PROGRESSION_02_DAY",
    "MUS_PROGRESSION_02_NIGHT", "MUS_PROGRESSION_03_DAY", "MUS_PROGRESSION_03_NIGHT",
};
char music_file_name[64] = "MUS_PROGRESSION_01_DAY"; // 0x0165CB30, the music table to play next
int music_intensity_decrease = 10; // 0x0165CB7C, percent per update
int music_intensity_increase = 10; // 0x0165CB80, percent per update
// The threshold names: the music clip name suffixes, and without the underscore the xtbl tags.
const char* Gamemusic_intensity_threshold_names[6] = { // 0x0165CB84
    "_SILENCE", "_AMBIENCE", "_COMBAT_LIGHT_LULL", "_COMBAT_LIGHT", "_COMBAT_HEAVY_LULL", "_COMBAT_HEAVY",
};
AUDIOLIB_MUSIC_STATE Gamemusic_intensity_states[6] = { // 0x0165CB9C
    AUDIOLIB_MUSIC_STATE_SILENCE, AUDIOLIB_MUSIC_STATE_AMBIENCE, AUDIOLIB_MUSIC_STATE_COMBAT_LIGHT_LULL,
    AUDIOLIB_MUSIC_STATE_COMBAT_LIGHT, AUDIOLIB_MUSIC_STATE_COMBAT_HEAVY_LULL, AUDIOLIB_MUSIC_STATE_COMBAT_HEAVY,
};
int Gamemusic_intensity_factors[20]; // 0x02511248, by gamemusic_intensity_factors (rfg_music_variables.xtbl)
int Gamemusic_intensity_thresholds[6]; // 0x025328F4, intensity percent per threshold
int Gamemusic_intensity_reductions[6]; // 0x025054EC
int Gamemusic_intensity_count; // 0x02273E70
int Gamemusic_total_intensity_factor; // 0x025054BC
int Gamemusic_curr_intensity_factors[20]; // 0x02532B98, the factors present this update
int Gamemusic_combat_intensity; // 0x02532B90
int Gamemusic_intensity_factors_max_combat; // 0x02284FD4
int Gamemusic_alert_level_intensity; // 0x02511298
int Gamemusic_intensity_factors_max_alert_level; // 0x02284FD0
int Gamemusic_awareness_intensity; // 0x0252D6EC
int Gamemusic_intensity_factors_max_awareness; // 0x02273E74
int Gamemusic_curr_intensity; // 0x02532C30, percent of the total factor
GAMEMUSIC_THRESHOLDS Gamemusic_curr_threshold; // 0x0227AFC4

// ---- file-static globals
static timestamp gamemusic_restart_music; // 0x0165E514, invalid (-1) from the constructor
static bool gamemusic_inited; // 0x0227AFC1
static GAMEMUSIC_THRESHOLDS gamemusic_limited_levels[8]; // 0x025054C4, a stack of the limited levels set
// Static in rfg.exe's unity TU; external here for gameaudio_system_save/_load (gameaudio.cpp is another TU).
gamemusic_music_files Gamemusic_Progression_File; // 0x0253385C
static int gamemusic_fade_time; // 0x02533860
static bool gamemusic_safehouse_state; // 0x02533864
static bool gamemusic_dead_state; // 0x02533865
static bool gamemusic_stop_music_event; // 0x02533866
static bool gamemusic_player_exited_level; // 0x02533867
static bool gamemusic_update_night_day; // 0x02533868
static bool gamemusic_in_special_music; // 0x02533869
static bool gamemusic_using_marauder_music; // 0x0253386A
static int multi_player_cue_selection; // 0x0253386C

// The console commands' per-thread text buffer (TLS +0x38, the one gameaudio.cpp's commands use in rfg.exe's unity TU).
static __declspec(thread) char Gamemusic_console_text[1024];

// ---- internal functions
// The limited level stack's push, inlined in gamemusic_system_init, gamemusic_set_limited_music_level and
// unused_dcf_music_limited: the first free slot takes the level.
static void gamemusic_push_limited_level(GAMEMUSIC_THRESHOLDS level)
{
    for (int i = 0; i < 8; i++)
    {
        if (gamemusic_limited_levels[i] == GAMEMUSIC_THRESHOLD_SILENCE)
        {
            gamemusic_limited_levels[i] = level;
            break;
        }
    }
}

// 0x007CB9F0 (:349). The safehouse limits the music to ambience (unless a minimum level is set), so does the player's
// death; after a stop event the music may restart once loading is over or its timer has run out.
static void gamemusic_check_special_cases()
{
    if (safehouse_contains_player() && Gamemusic_state.minimum_threshold == GAMEMUSIC_THRESHOLD_SILENCE)
    {
        if (!gamemusic_safehouse_state)
        {
            gamemusic_safehouse_state = true;
            gamemusic_set_limited_music_level(GAMEMUSIC_THRESHOLD_AMBIENCE);
        }
    }
    else if (gamemusic_safehouse_state)
    {
        gamemusic_safehouse_state = false;
        gamemusic_set_limited_music_level(GAMEMUSIC_THRESHOLD_SILENCE);
    }
    player* pp = player_get_local_player();
    if (pp == nullptr || pp->is_killed())
    {
        if (!gamemusic_dead_state)
        {
            gamemusic_dead_state = true;
            gamemusic_set_limited_music_level(GAMEMUSIC_THRESHOLD_AMBIENCE);
        }
    }
    else if (gamemusic_dead_state)
    {
        gamemusic_dead_state = false;
        gamemusic_set_limited_music_level(GAMEMUSIC_THRESHOLD_SILENCE);
        Gamemusic_intensity_count = 1;
    }
    if (gamemusic_stop_music_event)
    {
        if (audiolib_system_get_game_state() == AUDIOLIB_GAME_STATE_LOADING)
        {
            gamemusic_player_exited_level = true;
            // Magic number: 1 s after loading.
            gamemusic_restart_music.set(1000);
            return;
        }
        if ((audiolib_system_get_game_state() == AUDIOLIB_GAME_STATE_IN_LEVEL && gamemusic_player_exited_level) || gamemusic_restart_music.elapsed())
        {
            gamemusic_player_exited_level = false;
            gamemusic_forced_stop = false;
            gamemusic_stop_music_event = false;
        }
    }
}

// 0x007CBAE0 (:413). Stops the music and takes the progression file's music table; the forced stop is cleared only
// when it was not already set.
// Static in rfg.exe's unity TU; external here for gameaudio_system_load (gameaudio.cpp is another TU).
void gamemusic_switch_music_files()
{
    const bool was_forced = gamemusic_forced_stop;
    gamemusic_forced_stop = true;
    gamemusic_stop();
    strcpy(music_file_name, Gamemusic_Progression_File_Names[Gamemusic_Progression_File]);
    if (!was_forced)
    {
        gamemusic_forced_stop = false;
    }
}

// 0x007BE040 (:437). The state whose threshold name the text contains.
AUDIOLIB_MUSIC_STATE gamemusic_convert_text_to_audiolib_state(char* text)
{
    for (int i = 0; i < 6; i++)
    {
        if (strstr(text, Gamemusic_intensity_threshold_names[i]) != nullptr)
        {
            return Gamemusic_intensity_states[i];
        }
    }
    return AUDIOLIB_MUSIC_STATE_UNKNOWN;
}

// 0x007CBB30 (:459). Stops the music (5 s fade) and takes another music table.
void gamemusic_start_special_music(const char* music_file)
{
    gamemusic_forced_stop = true;
    // Magic number: a 5 s fade.
    gamemusic_fade_time = 5000;
    gamemusic_in_special_music = true;
    gamemusic_stop();
    strcpy(music_file_name, music_file);
    // Original: the inlined copy loop keeps each character in gamemusic_forced_stop, so it ends false (the
    // terminator) and the forced stop is lifted.
    gamemusic_forced_stop = false;
}

// 0x007CBB70 (:474)
void gamemusic_restore_original_music()
{
    gamemusic_in_special_music = false;
    gamemusic_switch_music_files();
}

// 0x007BE080 (:483). The music list's storage, once.
// Original: the allocation comes from audio_lib's pool, not memory_pool; a failed allocation still returns true.
bool gamemusic_parse_music_list_init(mempool* memory_pool, int total)
{
    if (Gameaudio_music_list.music != nullptr)
    {
        return Gameaudio_music_list.total == total;
    }
    Gameaudio_music_list.count = 0;
    Gameaudio_music_list.total = 0;
    Gameaudio_music_list.current = 0;
    Gameaudio_music_list.music = nullptr;
    Gameaudio_music_list.music = (gameaudio_music_list_data*)Audiolib_state.pool->alloc(total * sizeof(gameaudio_music_list_data), 0);
    if (Gameaudio_music_list.music != nullptr)
    {
        memset(Gameaudio_music_list.music, 0, total * sizeof(gameaudio_music_list_data));
        Gameaudio_music_list.total = total;
    }
    return true;
}

// 0x007BE0F0 (:506)
gameaudio_music_list_data* gamamusic_parse_music_list_find(const char* music_name)
{
    gameaudio_music_list_data* music = Gameaudio_music_list.music;
    for (int i = 0; i < Gameaudio_music_list.count; i++, music++)
    {
        if (v_stricmp(music->name, music_name) == 0)
        {
            return music;
        }
    }
    return nullptr;
}

// 0x007BE130 (:521)
gameaudio_music_list_data* gameaudio_parse_music_list_add_music(const char* music_name)
{
    gameaudio_music_list_data* music = gamamusic_parse_music_list_find(music_name);
    if (music != nullptr)
    {
        return music;
    }
    if (Gameaudio_music_list.count >= Gameaudio_music_list.total)
    {
        return nullptr;
    }
    music = &Gameaudio_music_list.music[Gameaudio_music_list.count];
    strncpy(music->name, music_name, sizeof(music->name));
    Gameaudio_music_list.count++;
    return music;
}

// The two Time_Min/Time_Max reads of one element (inlined).
static void gamemusic_read_time_range(xml_element* element, int& min_time, int& max_time)
{
    int value;
    xml_table_get_req_int(value, element, "Time_Min");
    min_time = value;
    xml_table_get_req_int(value, element, "Time_Max");
    max_time = value;
}

// The exit transition's percentages (inlined).
static void gamemusic_read_exit_transition(xml_element* element, gamemusic_level_data& level)
{
    float value;
    xml_table_get_req_float(value, element, "Neutral_Percentage");
    level.neutral_pct = value;
    xml_table_get_req_float(value, element, "Melodic_Percentage");
    level.melodic_pct = value;
}

// 0x007BE180 (:541). One <Music> of rfg_music.xtbl: its music table and the level data per threshold (defaults when
// an element is missing); the times are given in seconds and kept in ms.
audiolib_result gamemusic_parse_music_list_item(xml_element* xml_music)
{
    if (xml_music == nullptr)
    {
        return AUDIOLIB_ERR_NONE;
    }
    const char* name = xml_table_get_req_string_ref(xml_music, "Name");
    if (name == nullptr)
    {
        return AUDIOLIB_ERR_FILE_FORMAT;
    }
    gameaudio_music_list_data* music = gameaudio_parse_music_list_add_music(name);
    if (music == nullptr)
    {
        return AUDIOLIB_ERR_NO_MORE_MUSIC_SLOTS;
    }
    xml_element* element = xml_find(xml_music, "MTBL_Filename");
    if (element != nullptr)
    {
        const char* filename = xml_table_get_req_string_ref(element, "Filename");
        if (filename == nullptr)
        {
            return AUDIOLIB_ERR_FILE_FORMAT;
        }
        strncpy(music->mtbl_name, filename, sizeof(music->mtbl_name));
    }

    // Magic numbers below: the defaults (volume, min/max silence and other times in seconds, percentages).
    gamemusic_level_data& silence = music->levels[GAMEMUSIC_THRESHOLD_SILENCE];
    element = xml_find(xml_music, "Silence");
    silence.volume = 0.0f;
    silence.min_time_silence = -1;
    silence.max_time_silence = -1;
    silence.min_time_other = 10;
    silence.max_time_other = 20;
    silence.neutral_pct = 50.0f;
    silence.melodic_pct = 50.0f;
    if (element != nullptr)
    {
        xml_element* time = xml_find(element, "Ambience_Time");
        if (time != nullptr)
        {
            gamemusic_read_time_range(time, silence.min_time_other, silence.max_time_other);
        }
        float pct;
        xml_table_get_req_float(pct, element, "Percentage_Neutral");
        // Original: the melodic percentage takes the neutral one too.
        silence.neutral_pct = pct;
        silence.melodic_pct = pct;
    }

    // Original: the ambience level's percentages keep the list's zero fill.
    gamemusic_level_data& ambience = music->levels[GAMEMUSIC_THRESHOLD_AMBIENCE];
    ambience.volume = 0.8f;
    ambience.min_time_silence = 60;
    ambience.max_time_silence = 120;
    ambience.min_time_other = 60;
    ambience.max_time_other = 120;
    element = xml_find(xml_music, "Ambience");
    if (element != nullptr)
    {
        xml_element* time = xml_find(element, "Silence_Time");
        if (time != nullptr)
        {
            xml_table_get_req_int(ambience.min_time_silence, time, "Time_Min");
            xml_table_get_req_int(ambience.max_time_silence, time, "Time_Max");
        }
        xml_element* neutral = xml_find(element, "Neutral");
        if (neutral != nullptr)
        {
            xml_table_get_req_float(ambience.volume, neutral, "Volume");
        }
    }

    gamemusic_level_data& light = music->levels[GAMEMUSIC_THRESHOLD_COMBAT_LIGHT];
    light.volume = 0.8f;
    light.min_time_silence = 0;
    light.max_time_silence = 0;
    light.min_time_other = 10;
    light.max_time_other = 20;
    element = xml_find(xml_music, "Combat_Light");
    if (element != nullptr)
    {
        xml_table_get_req_float(light.volume, element, "Volume");
        xml_element* time = xml_find(element, "Combat_Light_Time");
        if (time != nullptr)
        {
            gamemusic_read_time_range(time, light.min_time_other, light.max_time_other);
        }
        xml_element* exit = xml_find(element, "Exit_Transition");
        if (exit != nullptr)
        {
            gamemusic_read_exit_transition(exit, light);
        }
    }

    gamemusic_level_data& light_lull = music->levels[GAMEMUSIC_THRESHOLD_COMBAT_LIGHT_LULL];
    light_lull.volume = 0.8f;
    light_lull.min_time_silence = 0;
    light_lull.max_time_silence = 0;
    light_lull.min_time_other = 10;
    light_lull.max_time_other = 20;
    element = xml_find(xml_music, "Combat_Light_Lull");
    if (element != nullptr)
    {
        xml_table_get_req_float(light_lull.volume, element, "Volume");
        // Original: the lull's times are read from a Combat_Light_Time element too.
        xml_element* time = xml_find(element, "Combat_Light_Time");
        if (time != nullptr)
        {
            gamemusic_read_time_range(time, light_lull.min_time_other, light_lull.max_time_other);
        }
        xml_element* exit = xml_find(element, "Exit_Transition");
        if (exit != nullptr)
        {
            gamemusic_read_exit_transition(exit, light_lull);
        }
    }

    gamemusic_level_data& heavy = music->levels[GAMEMUSIC_THRESHOLD_COMBAT_HEAVY];
    heavy.volume = 0.8f;
    heavy.min_time_silence = 0;
    heavy.max_time_silence = 0;
    heavy.min_time_other = 60;
    heavy.max_time_other = 120;
    element = xml_find(xml_music, "Combat_Heavy");
    if (element != nullptr)
    {
        xml_table_get_req_float(heavy.volume, element, "Volume");
        xml_element* exit = xml_find(element, "Exit_Transition");
        if (exit != nullptr)
        {
            gamemusic_read_exit_transition(exit, heavy);
            // Original: the heavy level's times are read only when it has an exit transition.
            gamemusic_read_time_range(element, heavy.min_time_other, heavy.max_time_other);
        }
    }

    gamemusic_level_data& heavy_lull = music->levels[GAMEMUSIC_THRESHOLD_COMBAT_HEAVY_LULL];
    heavy_lull.volume = 0.8f;
    heavy_lull.min_time_silence = 0;
    heavy_lull.max_time_silence = 0;
    heavy_lull.min_time_other = 60;
    heavy_lull.max_time_other = 120;
    element = xml_find(xml_music, "Combat_Heavy_Lull");
    if (element != nullptr)
    {
        xml_table_get_req_float(heavy_lull.volume, element, "Volume");
        gamemusic_read_time_range(element, heavy_lull.min_time_other, heavy_lull.max_time_other);
        xml_element* exit = xml_find(element, "Exit_Transition");
        if (exit != nullptr)
        {
            gamemusic_read_exit_transition(exit, heavy_lull);
        }
    }

    // seconds to ms (the positive times only)
    for (int i = 0; i < NUM_GAMEMUSIC_LEVEL_THRESHOLDS; i++)
    {
        gamemusic_level_data& level = music->levels[i];
        int* times[4] = {&level.min_time_silence, &level.max_time_silence, &level.min_time_other, &level.max_time_other};
        for (int* t : times)
        {
            if (*t > 0)
            {
                *t *= 1000;
            }
        }
    }
    return AUDIOLIB_ERR_NONE;
}

// One intensity factor value (inlined): a required string, as a number.
static bool gamemusic_read_factor(xml_element* element, const char* tag, int& factor)
{
    const char* text = xml_table_get_req_string_ref(element, tag);
    if (text == nullptr)
    {
        return false;
    }
    factor = atoi(text);
    return true;
}

// 0x007BE820 (:809). rfg_music_variables.xtbl: the intensity factors, the threshold percentages and reductions.
audiolib_result gamemusic_parse_music_variable(xml_element* xml_music)
{
    if (xml_music == nullptr)
    {
        return AUDIOLIB_ERR_FILE_FORMAT;
    }
    xml_element* root = xml_find(xml_music, "RFG_Music_Variables");
    if (root == nullptr)
    {
        return AUDIOLIB_ERR_FILE_FORMAT;
    }
    xml_element* calculations = xml_find(root, "Intensity_Calculations");
    if (calculations != nullptr)
    {
        if (!gamemusic_read_factor(calculations, "Intensity_Factor_Total", Gamemusic_total_intensity_factor))
        {
            return AUDIOLIB_ERR_FILE_FORMAT;
        }
        xml_element* combat = xml_find(calculations, "Combat");
        if (combat != nullptr)
        {
            struct factor_element
            {
                const char* element;
                const char* tags[3];
                int first_factor;
            };
            static const factor_element elements[] = {
                {"Player_Health", {"Player_Health_LT_50_percent", "Player_Health_LT_30_percent", "Player_Health_LT_15_percent"}, MUSIC_INTENSITY_HEALTH_50},
                {"EDF_Human_Targeting_Player", {"EDF_Targeting_Player_GTE_1", "EDF_Targeting_Player_GTE_5", "EDF_Targeting_Player_GTE_10"}, MUSIC_INTENSITY_EDF_1},
                {"EDF_Light_Vehicles_Targeting_Player", {"EDF_LVehicle_Trgt_Player_GTE_1", "EDF_LVehicle_Trgt_Player_GTE_2", nullptr}, MUSIC_INTENSITY_LIGHT_VEH_1},
                {"EDF_Heavy_Vehicles_Targeting_Player", {"EDF_HVehicle_Trgt_Player_GTE_1", nullptr, nullptr}, MUSIC_INTENSITY_HEAVY_VEH_1},
                {"EDF_AAX_Flier_Targeting_Player", {"EDF_AAX_Targeting_Player_GTE_1", nullptr, nullptr}, MUSIC_INTENSITY_AAX},
                {"EDF_ATACX_Flier_Targeting_Player", {"EDF_A_TACX_Trgt_Player_GTE_1", nullptr, nullptr}, MUSIC_INTENSITY_ATACX},
                {"Marauder_Targeting_Player", {"Marauder_Targeting_Player_GTE_1", "Marauder_Targeting_Player_GTE_5", "Marauder_Targeting_Player_GTE_10"}, MUSIC_INTENSITY_MARAUDER_1},
            };
            for (const factor_element& fe : elements)
            {
                xml_element* element = xml_find(combat, fe.element);
                if (element == nullptr)
                {
                    continue;
                }
                for (int i = 0; i < 3 && fe.tags[i] != nullptr; i++)
                {
                    if (!gamemusic_read_factor(element, fe.tags[i], Gamemusic_intensity_factors[fe.first_factor + i]))
                    {
                        return AUDIOLIB_ERR_FILE_FORMAT;
                    }
                }
            }
            if (!gamemusic_read_factor(combat, "Max_Combat_Intensity_Factor", Gamemusic_intensity_factors_max_combat))
            {
                return AUDIOLIB_ERR_FILE_FORMAT;
            }
        }
        xml_element* awareness = xml_find(calculations, "AI_Awareness");
        if (awareness != nullptr)
        {
            Gamemusic_intensity_factors_max_awareness = 0;
            xml_element* aware = xml_find(awareness, "AI_Aware");
            if (aware != nullptr)
            {
                if (!gamemusic_read_factor(aware, "AI_Alerted_to_Player", Gamemusic_intensity_factors[MUSIC_INTENSITY_AI_ALERTED]))
                {
                    return AUDIOLIB_ERR_FILE_FORMAT;
                }
                if (Gamemusic_intensity_factors_max_awareness < Gamemusic_intensity_factors[MUSIC_INTENSITY_AI_ALERTED])
                {
                    Gamemusic_intensity_factors_max_awareness = Gamemusic_intensity_factors[MUSIC_INTENSITY_AI_ALERTED];
                }
            }
            xml_element* search = xml_find(awareness, "AI_Search");
            if (search != nullptr)
            {
                if (!gamemusic_read_factor(search, "AI_Searching_for_Player", Gamemusic_intensity_factors[MUSIC_INTENSITY_AI_SEARCHING]))
                {
                    return AUDIOLIB_ERR_FILE_FORMAT;
                }
                if (Gamemusic_intensity_factors_max_awareness < Gamemusic_intensity_factors[MUSIC_INTENSITY_AI_SEARCHING])
                {
                    Gamemusic_intensity_factors_max_awareness = Gamemusic_intensity_factors[MUSIC_INTENSITY_AI_SEARCHING];
                }
            }
        }
        xml_element* alert = xml_find(calculations, "Alert_Level");
        if (alert != nullptr)
        {
            Gamemusic_intensity_factors_max_alert_level = 0;
            static const char* const tags[4] = {"Alert_Level_E_Green", "Alert_Level_E_Yellow", "Alert_Level_E_Orange", "Alert_Level_E_Red"};
            for (int i = 0; i < 4; i++)
            {
                int& factor = Gamemusic_intensity_factors[MUSIC_INTENSITY_ALERT_GREEN + i];
                if (!gamemusic_read_factor(alert, tags[i], factor))
                {
                    return AUDIOLIB_ERR_FILE_FORMAT;
                }
                if (Gamemusic_intensity_factors_max_alert_level < factor)
                {
                    Gamemusic_intensity_factors_max_alert_level = factor;
                }
            }
        }
    }
    xml_element* thresholds = xml_find(root, "Intensity_Thresholds");
    if (thresholds != nullptr)
    {
        for (int i = GAMEMUSIC_THRESHOLD_COMBAT_LIGHT_LULL; i <= GAMEMUSIC_THRESHOLD_COMBAT_HEAVY; i++)
        {
            xml_table_get_req_int(Gamemusic_intensity_thresholds[i], thresholds, Gamemusic_intensity_threshold_names[i] + 1);
        }
    }
    xml_element* reductions = xml_find(root, "Intensity_Reduction");
    if (reductions != nullptr)
    {
        // in rfg.exe's order: light, heavy, light lull, heavy lull
        static const GAMEMUSIC_THRESHOLDS order[4] = {GAMEMUSIC_THRESHOLD_COMBAT_LIGHT, GAMEMUSIC_THRESHOLD_COMBAT_HEAVY, GAMEMUSIC_THRESHOLD_COMBAT_LIGHT_LULL, GAMEMUSIC_THRESHOLD_COMBAT_HEAVY_LULL};
        for (GAMEMUSIC_THRESHOLDS t : order)
        {
            xml_table_get_req_int(Gamemusic_intensity_reductions[t], reductions, Gamemusic_intensity_threshold_names[t] + 1);
        }
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x007BED60 (:1106)
AUDIOLIB_MUSIC_STATE gamemusic_convert_threshold_to_audiolib_state(GAMEMUSIC_THRESHOLDS threshold)
{
    switch (threshold)
    {
    case GAMEMUSIC_THRESHOLD_AMBIENCE:
        return AUDIOLIB_MUSIC_STATE_AMBIENCE;
    case GAMEMUSIC_THRESHOLD_COMBAT_LIGHT_LULL:
        return AUDIOLIB_MUSIC_STATE_COMBAT_LIGHT_LULL;
    case GAMEMUSIC_THRESHOLD_COMBAT_LIGHT:
        return AUDIOLIB_MUSIC_STATE_COMBAT_LIGHT;
    case GAMEMUSIC_THRESHOLD_COMBAT_HEAVY_LULL:
        return AUDIOLIB_MUSIC_STATE_COMBAT_HEAVY_LULL;
    case GAMEMUSIC_THRESHOLD_COMBAT_HEAVY:
        return AUDIOLIB_MUSIC_STATE_COMBAT_HEAVY;
    default:
        return AUDIOLIB_MUSIC_STATE_SILENCE;
    }
}

// 0x007BEDB0 (:1138). rfg_music.xtbl (only with a pool) and rfg_music_variables.xtbl.
static bool gamemusic_parse_music_xtables(mempool* memory_pool)
{
    audiolib_result result = AUDIOLIB_ERR_NONE;
    if (memory_pool != nullptr)
    {
        xml_element* root = xml_table_parse("rfg_music.xtbl");
        if (root == nullptr)
        {
            return false;
        }
        const int total = xml_count(root, "Music");
        if (gamemusic_parse_music_list_init(memory_pool, total))
        {
            for (xml_element* music = xml_find(root, "Music"); music != nullptr; music = xml_find_next(root, music, "Music"))
            {
                result = gamemusic_parse_music_list_item(music);
                if ((int)result < 0)
                {
                    break;
                }
            }
        }
        xml_table_close(root);
        if (result != AUDIOLIB_ERR_NONE)
        {
            return false;
        }
    }
    xml_element* root = xml_table_parse("rfg_music_variables.xtbl");
    if (root != nullptr)
    {
        result = gamemusic_parse_music_variable(root);
        xml_table_close(root);
    }
    return result == AUDIOLIB_ERR_NONE;
}

// 0x007CBB80 (:1176). Day is 4:00 to 21:59.
static bool gamemusic_is_day_time()
{
    // Magic numbers: hours 4 to 21 (unsigned compare).
    return (unsigned int)(Game_clock.m_hour - 4) < 18;
}

// 0x007CBBA0 (:1186). The progression file follows day and night (the odd files are the night ones); the switch waits
// until the music is down to ambience and no special music plays.
static bool gamemusic_check_night_day_transition()
{
    if (audiolib_system_get_game_state() != AUDIOLIB_GAME_STATE_IN_LEVEL)
    {
        return false;
    }
    const bool night_file = (Gamemusic_Progression_File & 1) != 0;
    if (gamemusic_is_day_time())
    {
        if (night_file)
        {
            Gamemusic_Progression_File = (gamemusic_music_files)(Gamemusic_Progression_File - 1);
            gamemusic_update_night_day = true;
        }
        else if (!gamemusic_update_night_day)
        {
            return false;
        }
    }
    else if (!night_file)
    {
        Gamemusic_Progression_File = (gamemusic_music_files)(Gamemusic_Progression_File + 1);
        gamemusic_update_night_day = true;
    }
    else if (!gamemusic_update_night_day)
    {
        return false;
    }
    if (!gamemusic_in_special_music && Gamemusic_curr_threshold < GAMEMUSIC_THRESHOLD_COMBAT_LIGHT_LULL &&
        Gamemusic_state.minimum_threshold < GAMEMUSIC_THRESHOLD_COMBAT_LIGHT_LULL)
    {
        gamemusic_in_special_music = false;
        gamemusic_switch_music_files();
        gamemusic_update_night_day = false;
        return true;
    }
    return false;
}

// 0x007CBC30 (:1222). The marauder music in the marauder zones (four named territory zones and the districts'
// marauder zones), started once the music is down to ambience; back to the progression music on leaving.
static bool gamemusic_check_marauder_music()
{
    if (audiolib_system_get_game_state() != AUDIOLIB_GAME_STATE_IN_LEVEL)
    {
        return false;
    }
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        obj_zone* zone = World.get_zone_by_pos(pp->pos);
        if (zone != nullptr && zone->get_name() != nullptr)
        {
            const char* name = zone->get_name();
            const bool marauder_zone = strcmp(name, "terr01_09_10") == 0 || strcmp(name, "terr01_10_10") == 0 || strcmp(name, "terr01_11_10") == 0 ||
                                       strcmp(name, "terr01_12_10") == 0 || district_marauder_zone_at_pos(pp->pos);
            if (marauder_zone)
            {
                if (gamemusic_using_marauder_music)
                {
                    return false;
                }
                if (audiolib_interactive_music_get_state(Gamemusic_state.curr_music.music_id) > AUDIOLIB_MUSIC_STATE_AMBIENCE)
                {
                    return false;
                }
                gamemusic_using_marauder_music = true;
                if (Gamemusic_Progression_File <= GAMEMUSIC_PROGRESSION_01_NIGHT)
                {
                    gamemusic_start_special_music("MUS_PROGRESSION_01_MARAUDER");
                }
                else if (Gamemusic_Progression_File <= GAMEMUSIC_PROGRESSION_02_NIGHT)
                {
                    gamemusic_start_special_music("MUS_PROGRESSION_02_MARAUDER");
                }
                else
                {
                    gamemusic_start_special_music("MUS_PROGRESSION_03_MARAUDER");
                }
                return true;
            }
        }
    }
    if (gamemusic_using_marauder_music && audiolib_interactive_music_get_state(Gamemusic_state.curr_music.music_id) <= AUDIOLIB_MUSIC_STATE_AMBIENCE)
    {
        gamemusic_using_marauder_music = false;
        gamemusic_in_special_music = false;
        gamemusic_switch_music_files();
        return true;
    }
    return false;
}

// 0x007CBE20 (:1293). A DLC's progression file (dlcNN_music_progression.xtbl).
void gamemusic_dlc_music_init(int dlc_id)
{
    char filename[64];
    sprintf(filename, "dlc%02d_music_progression.xtbl", dlc_id);
    if (!cf_exist(filename))
    {
        return;
    }
    xml_element* root = xml_table_parse(filename);
    if (root == nullptr)
    {
        return;
    }
    xml_element* name = xml_find(root, "Name");
    if (name == nullptr)
    {
        xml_table_close(root);
        return;
    }
    int progression;
    xml_table_get_req_int(progression, name, "Progression");
    Gamemusic_Progression_File = (gamemusic_music_files)progression;
    xml_table_close(root);
    gamemusic_switch_music_files();
}

// 0x007CBED0 (:1316)
void gamemusic_dlc_music_shutdown()
{
    Gamemusic_Progression_File = GAMEMUSIC_PROGRESSION_01_DAY;
    gamemusic_switch_music_files();
}

// 0x007BEE70 (:1323)
gameaudio_music_list_data* gamemusic_find_music_data(char* music_name)
{
    if (music_name == nullptr)
    {
        return nullptr;
    }
    for (int i = 0; i < Gameaudio_music_list.count; i++)
    {
        if (v_stricmp(Gameaudio_music_list.music[i].name, music_name) == 0)
        {
            return &Gameaudio_music_list.music[i];
        }
    }
    return nullptr;
}

// 0x007BEED0 (:1337)
void gamemusic_clean_music_info(gameaudio_music_state_info* music_info)
{
    if (music_info == nullptr)
    {
        return;
    }
    music_info->music_data = nullptr;
    music_info->start_time = 0;
    music_info->silence_time = -1;
    music_info->other_time = -1;
    music_info->music_id = -1;
    music_info->state = AUDIOLIB_MUSIC_STATE_UNKNOWN;
    music_info->threshold = GAMEMUSIC_THRESHOLD_SILENCE;
    music_info->intensity = 0;
    music_info->volume = 0.0f;
    music_info->active = false;
}

// 0x007BEF00 (:1378). The local player's health percent sets one of the three health factors (below 15%, 30%, 50%).
void gamemusic_determine_health_intensity()
{
    int health = 100;
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        // The SSE2 double rule: the quotient in double, rounded to float, then the scale in double.
        health = (int)((double)(float)((double)pp->hit_points / (double)(float)pp->max_hit_points) * 100.0);
    }
    Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_HEALTH_15] = 0;
    Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_HEALTH_30] = 0;
    Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_HEALTH_50] = 0;
    // Magic numbers: the 15, 30 and 50 percent bands.
    if (health < 15)
    {
        Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_HEALTH_15] = health > 0 ? health : 1;
    }
    else if (health < 30)
    {
        Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_HEALTH_30] = health;
    }
    else if (health < 50)
    {
        Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_HEALTH_50] = health;
    }
}

// 0x007D8050 (:1400). The npcs (but the ambient EDF fliers) that believe the player exists, should be engaged or is
// at a known position; of the last, the marauders, the EDF and the EDF drivers by vehicle class. Combat intensity
// once any npc knows where the player is, awareness intensity otherwise.
void gamemusic_determine_EDF_combat_intensity()
{
    int aware = 0;
    int engaging = 0;
    int edf = 0;
    int marauders = 0;
    int drivers = 0;
    int light_vehicles = 0;
    int tanks = 0;
    int fliers = 0;
    bool located = false;
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        const int num = World.get_num_objects<npc>();
        for (int i = 0; i < num; i++)
        {
            npc* np = World.get_object<npc>(i);
            if (np->npc_flags.ambient_edf_flyer)
            {
                continue;
            }
            const ai_perception& perception = np->get_ai()->perception;
            if (perception.get_belief(AI_BELIEF_ENEMY_EXISTS, pp) != nullptr)
            {
                aware++;
            }
            if (perception.get_belief(AI_BELIEF_SHOULD_ENGAGE_ENEMY, pp) != nullptr)
            {
                engaging++;
            }
            if (perception.get_belief(AI_BELIEF_ENEMY_POSITION, pp) == nullptr)
            {
                continue;
            }
            located = true;
            if (np->current_team == HUMAN_TEAM_MARAUDER)
            {
                marauders++;
                continue;
            }
            edf++;
            if (!human_is_driving(np))
            {
                continue;
            }
            drivers++;
            vehicle* vp = human_in_vehicle(np);
            if (vp == nullptr || vp->info == nullptr)
            {
                continue;
            }
            if (vp->info->vehicle_class == VCL_FLYER)
            {
                fliers++;
            }
            else if (vp->info->vehicle_class != VCL_WALKER)
            {
                if (vp->info->vehicle_classification == BVT_TANK)
                {
                    tanks++;
                }
                else
                {
                    light_vehicles++;
                }
            }
        }
    }
    Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_AI_ALERTED] = aware;
    Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_AI_SEARCHING] = engaging;
    // Magic numbers: the 1, 5 and 10 counts.
    Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_EDF_1] = 0;
    Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_EDF_5] = 0;
    Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_EDF_10] = 0;
    if (edf >= 10)
    {
        Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_EDF_10] = edf;
    }
    else if (edf >= 5)
    {
        Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_EDF_5] = edf;
    }
    else if (edf >= 1)
    {
        Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_EDF_1] = edf;
    }
    Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_MARAUDER_1] = 0;
    Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_MARAUDER_5] = 0;
    Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_MARAUDER_10] = 0;
    if (marauders >= 10)
    {
        Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_MARAUDER_10] = marauders;
    }
    else if (marauders >= 5)
    {
        Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_MARAUDER_5] = marauders;
    }
    else if (marauders >= 1)
    {
        Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_MARAUDER_1] = marauders;
    }
    // Original: the light vehicle factors take the count of all drivers, not of the light vehicles.
    Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_LIGHT_VEH_1] = 0;
    Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_LIGHT_VEH_2] = 0;
    if (light_vehicles >= 2)
    {
        Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_LIGHT_VEH_2] = drivers;
    }
    else if (light_vehicles >= 1)
    {
        Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_LIGHT_VEH_1] = drivers;
    }
    // Original: the fliers count as AAX; the ATACX factor stays 0.
    Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_HEAVY_VEH_1] = tanks;
    Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_AAX] = fliers;
    Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_ATACX] = 0;
    Gamemusic_combat_intensity = 0;
    Gamemusic_awareness_intensity = 0;
    if (located)
    {
        // every present combat factor (health to marauders: the first 14)
        int combat = 0;
        for (int i = 0; i < MUSIC_INTENSITY_AI_ALERTED; i++)
        {
            combat += (Gamemusic_curr_intensity_factors[i] != 0) * Gamemusic_intensity_factors[i];
        }
        Gamemusic_combat_intensity = combat;
        if (Gamemusic_combat_intensity > Gamemusic_intensity_factors_max_combat)
        {
            Gamemusic_combat_intensity = Gamemusic_intensity_factors_max_combat;
        }
    }
    else
    {
        Gamemusic_awareness_intensity = (aware != 0) * Gamemusic_intensity_factors[MUSIC_INTENSITY_AI_ALERTED];
        if (Gamemusic_awareness_intensity > Gamemusic_intensity_factors_max_awareness)
        {
            Gamemusic_awareness_intensity = Gamemusic_intensity_factors_max_awareness;
        }
    }
}

// 0x007BEF90 (:1558). The factor of the current alert level, capped.
void gamemusic_determine_alert_level_intensity()
{
    const gfm_alert_levels level = gsm_get_alert_level();
    Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_ALERT_GREEN] = level == ALERT_LEVEL_GREEN;
    Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_ALERT_YELLOW] = level == ALERT_LEVEL_YELLOW;
    Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_ALERT_ORANGE] = level == ALERT_LEVEL_ORANGE;
    Gamemusic_curr_intensity_factors[MUSIC_INTENSITY_ALERT_RED] = level == ALERT_LEVEL_RED;
    Gamemusic_alert_level_intensity = 0;
    for (int i = MUSIC_INTENSITY_ALERT_GREEN; i <= MUSIC_INTENSITY_ALERT_RED; i++)
    {
        const int intensity = (Gamemusic_curr_intensity_factors[i] != 0) * Gamemusic_intensity_factors[i];
        if (intensity > Gamemusic_alert_level_intensity)
        {
            Gamemusic_alert_level_intensity = intensity;
        }
    }
    if (Gamemusic_alert_level_intensity > Gamemusic_intensity_factors_max_alert_level)
    {
        Gamemusic_alert_level_intensity = Gamemusic_intensity_factors_max_alert_level;
    }
}

// 0x007CBEE0 (:1583). The intensity count moves towards the new count by the decrease/increase percent (at least
// one up), never past it; while loading it is 1.
void gamemusic_adjust_intensity(int new_intensity_count)
{
    if (new_intensity_count < Gamemusic_intensity_count)
    {
        Gamemusic_intensity_count = (100 - music_intensity_decrease) * Gamemusic_intensity_count / 100;
        if (Gamemusic_intensity_count < new_intensity_count)
        {
            Gamemusic_intensity_count = new_intensity_count;
        }
    }
    else if (new_intensity_count > Gamemusic_intensity_count)
    {
        const int at_least = Gamemusic_intensity_count + 1;
        Gamemusic_intensity_count = (music_intensity_increase + 100) * Gamemusic_intensity_count / 100;
        if (Gamemusic_intensity_count <= at_least)
        {
            Gamemusic_intensity_count = at_least;
        }
        if (Gamemusic_intensity_count > new_intensity_count)
        {
            Gamemusic_intensity_count = new_intensity_count;
        }
    }
    else
    {
        Gamemusic_intensity_count = new_intensity_count;
    }
    if (audiolib_system_get_game_state() == AUDIOLIB_GAME_STATE_LOADING)
    {
        Gamemusic_intensity_count = 1;
    }
}

// 0x007D8310 (:1602). The single player intensity: health, combat or awareness and the alert level, held at light
// combat while the alert level is up; then the threshold it reaches.
void gamemusic_compute_current_intensity_single_player()
{
    gamemusic_determine_health_intensity();
    gamemusic_determine_EDF_combat_intensity();
    gamemusic_determine_alert_level_intensity();
    int new_intensity_count = Gamemusic_alert_level_intensity + Gamemusic_awareness_intensity + Gamemusic_combat_intensity;
    // Magic numbers: the light combat threshold times 10, plus 5.
    const int light_combat = Gamemusic_intensity_thresholds[GAMEMUSIC_THRESHOLD_COMBAT_LIGHT] * 10;
    if (Gamemusic_intensity_count >= light_combat && new_intensity_count < light_combat && gsm_get_alert_level() > 0)
    {
        new_intensity_count = light_combat + 5;
    }
    gamemusic_adjust_intensity(new_intensity_count);
    Gamemusic_curr_intensity = 0;
    if (Gamemusic_total_intensity_factor != 0)
    {
        if (Gamemusic_intensity_count > Gamemusic_total_intensity_factor)
        {
            Gamemusic_intensity_count = Gamemusic_total_intensity_factor;
        }
        if (Gamemusic_intensity_count != 0)
        {
            Gamemusic_curr_intensity = Gamemusic_intensity_count * 100 / Gamemusic_total_intensity_factor;
        }
    }
    // Original: silence is never tested and ambience's threshold is always 0 (not read from the table), so silence is
    // never chosen (ambience is the floor).
    Gamemusic_curr_threshold = GAMEMUSIC_THRESHOLD_COMBAT_HEAVY;
    do
    {
        if (Gamemusic_intensity_thresholds[Gamemusic_curr_threshold] <= Gamemusic_curr_intensity)
        {
            return;
        }
        Gamemusic_curr_threshold = (GAMEMUSIC_THRESHOLDS)(Gamemusic_curr_threshold - 1);
    } while (Gamemusic_curr_threshold > GAMEMUSIC_THRESHOLD_SILENCE);
}

// 0x007BF060 (:1648). Dead in rfg.exe: nothing calls it.
[[maybe_unused]] static GAMEMUSIC_THRESHOLDS gamemusic_decide_on_exit_type()
{
    return GAMEMUSIC_THRESHOLD_AMBIENCE;
}

// 0x007BF070 (:1655); LTCG: threshold in ECX. A lull goes up to its level's base; the next threshold is set when the
// limited level allows it and it changes the music.
static bool gamemusic_get_valid_upward_state(GAMEMUSIC_THRESHOLDS threshold)
{
    if (threshold == GAMEMUSIC_THRESHOLD_COMBAT_LIGHT_LULL)
    {
        threshold = GAMEMUSIC_THRESHOLD_AMBIENCE;
    }
    else if (threshold == GAMEMUSIC_THRESHOLD_COMBAT_HEAVY_LULL)
    {
        threshold = GAMEMUSIC_THRESHOLD_COMBAT_LIGHT;
    }
    if ((Gamemusic_state.limited_level == GAMEMUSIC_THRESHOLD_SILENCE || threshold <= Gamemusic_state.limited_level) && threshold != Gamemusic_state.curr_music.threshold)
    {
        Gamemusic_state.next_music.threshold = threshold;
        return true;
    }
    return false;
}

// The way down from the current threshold once its timer ran out (silence, the lull checks): to ambience, unless the
// combat cue has no time left to fade.
static bool gamemusic_lull_to_ambience(bool other_done)
{
    return other_done && audiolib_interactive_music_enough_time_to_fade_combat_cue();
}

// 0x007CBF70 (:1689)
// The single player interactive music, each frame: pause bookkeeping, the next music table, the next threshold from the
// intensity (Gamemusic_curr_threshold), the minimum and the limited level, then a start or a transition and the new
// silence and other timers.
void gamemusic_set_current_music(int curr_time, bool game_paused)
{
    if (Multiplayer)
    {
        gamemusic_forced_stop = true;
    }
    if (audiolib_interactive_music_get_reboot_status() && !gamemusic_forced_stop)
    {
        gamemusic_forced_stop = true;
        gamemusic_stop();
        gamemusic_forced_stop = false;
    }
    const int active_id = audiolib_interactive_music_get_active_id();
    if (gamemusic_forced_stop)
    {
        if (active_id != -1)
        {
            gamemusic_stop();
        }
        return;
    }
    const AUDIOLIB_GAME_STATE game_state = audiolib_system_get_game_state();
    if (game_paused || game_state != AUDIOLIB_GAME_STATE_LOADING)
    {
        if (audiolib_interactive_music_get_pause(active_id))
        {
            if (!Gamemusic_state.paused)
            {
                Gamemusic_state.paused_time = curr_time;
                Gamemusic_state.paused = true;
            }
            return;
        }
        if (Gamemusic_state.paused)
        {
            // The timers move on by the time spent paused.
            if (Gamemusic_state.curr_music.silence_time > 0)
            {
                Gamemusic_state.curr_music.silence_time += curr_time - Gamemusic_state.paused_time;
            }
            if (Gamemusic_state.curr_music.other_time > 0)
            {
                Gamemusic_state.curr_music.other_time += curr_time - Gamemusic_state.paused_time;
            }
            Gamemusic_state.paused_time = 0;
            Gamemusic_state.paused = false;
        }
    }
    bool changed = false;
    bool silence_done = false;
    if (!Gamemusic_state.curr_music.active)
    {
        const int music_id = Gamemusic_state.curr_music.music_id;
        if (music_id != 0 && music_id != -1)
        {
            Gamemusic_state.next_music = Gamemusic_state.curr_music;
            gamemusic_clean_music_info(&Gamemusic_state.curr_music);
        }
        else if (music_file_name[0] != '\0' && Gamemusic_state.next_music.music_data == nullptr)
        {
            Gamemusic_state.next_music.music_data = gamemusic_find_music_data(music_file_name);
        }
        if (Gamemusic_state.next_music.music_data == nullptr)
        {
            return;
        }
        changed = true;
    }
    else
    {
        if (active_id != Gamemusic_state.curr_music.music_id)
        {
            // Magic number: a 500 ms fade.
            gamemusic_forced_stop = true;
            gamemusic_fade_time = 500;
            gamemusic_stop();
            gamemusic_forced_stop = false;
            return;
        }
        // Original: the state is read and not used.
        audiolib_interactive_music_get_state(Gamemusic_state.curr_music.music_id);
    }
    if (!Gamemusic_state.curr_music.active)
    {
        if (Gamemusic_state.curr_music.music_data != nullptr && Gamemusic_state.next_music.music_data == nullptr)
        {
            Gamemusic_state.next_music = Gamemusic_state.curr_music;
            gamemusic_clean_music_info(&Gamemusic_state.curr_music);
        }
        changed = true;
    }
    else
    {
        const int silence_time = Gamemusic_state.curr_music.silence_time;
        silence_done = silence_time > 0 && silence_time < curr_time;
        // The other timer ran out, or the music is below heavy combat (which waits for its timer).
        const int other_time = Gamemusic_state.curr_music.other_time;
        bool other_done = other_time > 0 && other_time < curr_time;
        if (!other_done && Gamemusic_state.curr_music.threshold != GAMEMUSIC_THRESHOLD_COMBAT_HEAVY)
        {
            other_done = true;
        }
        Gamemusic_state.next_music = Gamemusic_state.curr_music;
        const GAMEMUSIC_THRESHOLDS intensity = Gamemusic_curr_threshold;
        const GAMEMUSIC_THRESHOLDS minimum = Gamemusic_state.minimum_threshold;
        const GAMEMUSIC_THRESHOLDS limited = Gamemusic_state.limited_level;
        // Each case sets the next threshold (changed) or leaves it; "limited" counts only when set (not silence).
        GAMEMUSIC_THRESHOLDS next = GAMEMUSIC_THRESHOLD_INVALID;
        switch (Gamemusic_state.curr_music.threshold)
        {
        case GAMEMUSIC_THRESHOLD_AMBIENCE:
            if (limited == GAMEMUSIC_THRESHOLD_AMBIENCE)
            {
                break;
            }
            if (minimum > GAMEMUSIC_THRESHOLD_AMBIENCE)
            {
                changed = gamemusic_get_valid_upward_state(minimum);
            }
            else if (intensity >= GAMEMUSIC_THRESHOLD_COMBAT_LIGHT)
            {
                changed = gamemusic_get_valid_upward_state(intensity);
            }
            break;
        case GAMEMUSIC_THRESHOLD_COMBAT_LIGHT_LULL:
            if (intensity > GAMEMUSIC_THRESHOLD_COMBAT_LIGHT_LULL || minimum > GAMEMUSIC_THRESHOLD_COMBAT_LIGHT_LULL)
            {
                // Up to the highest of the intensity, the minimum and the limited level.
                GAMEMUSIC_THRESHOLDS up = intensity;
                if (minimum > up)
                {
                    up = minimum;
                }
                if (limited > up)
                {
                    up = limited;
                }
                changed = gamemusic_get_valid_upward_state(up);
            }
            else if (limited != GAMEMUSIC_THRESHOLD_SILENCE && limited < GAMEMUSIC_THRESHOLD_COMBAT_LIGHT_LULL)
            {
                next = GAMEMUSIC_THRESHOLD_AMBIENCE;
            }
            else if (intensity < GAMEMUSIC_THRESHOLD_COMBAT_LIGHT_LULL && gamemusic_lull_to_ambience(other_done))
            {
                next = GAMEMUSIC_THRESHOLD_AMBIENCE;
            }
            break;
        case GAMEMUSIC_THRESHOLD_COMBAT_LIGHT:
            if (limited != GAMEMUSIC_THRESHOLD_SILENCE && limited < GAMEMUSIC_THRESHOLD_COMBAT_LIGHT)
            {
                next = GAMEMUSIC_THRESHOLD_AMBIENCE;
            }
            else if ((intensity > GAMEMUSIC_THRESHOLD_COMBAT_HEAVY_LULL || minimum > GAMEMUSIC_THRESHOLD_COMBAT_HEAVY_LULL) && (limited == GAMEMUSIC_THRESHOLD_SILENCE || limited >= GAMEMUSIC_THRESHOLD_COMBAT_HEAVY))
            {
                next = GAMEMUSIC_THRESHOLD_COMBAT_HEAVY;
            }
            else if (intensity < GAMEMUSIC_THRESHOLD_COMBAT_LIGHT && minimum < GAMEMUSIC_THRESHOLD_COMBAT_LIGHT && other_done)
            {
                next = GAMEMUSIC_THRESHOLD_COMBAT_LIGHT_LULL;
            }
            break;
        case GAMEMUSIC_THRESHOLD_COMBAT_HEAVY_LULL:
            if ((intensity > GAMEMUSIC_THRESHOLD_COMBAT_HEAVY_LULL || minimum > GAMEMUSIC_THRESHOLD_COMBAT_HEAVY_LULL) && (limited == GAMEMUSIC_THRESHOLD_SILENCE || limited >= GAMEMUSIC_THRESHOLD_COMBAT_HEAVY))
            {
                next = GAMEMUSIC_THRESHOLD_COMBAT_HEAVY;
            }
            else if (limited != GAMEMUSIC_THRESHOLD_SILENCE && limited < GAMEMUSIC_THRESHOLD_COMBAT_LIGHT)
            {
                next = GAMEMUSIC_THRESHOLD_AMBIENCE;
            }
            else if (limited != GAMEMUSIC_THRESHOLD_SILENCE && limited < GAMEMUSIC_THRESHOLD_COMBAT_HEAVY)
            {
                next = GAMEMUSIC_THRESHOLD_COMBAT_LIGHT;
            }
            else if (intensity < GAMEMUSIC_THRESHOLD_COMBAT_LIGHT && minimum < GAMEMUSIC_THRESHOLD_COMBAT_LIGHT && gamemusic_lull_to_ambience(other_done))
            {
                // Original: straight from the heavy lull to ambience, past light combat.
                next = GAMEMUSIC_THRESHOLD_AMBIENCE;
            }
            break;
        case GAMEMUSIC_THRESHOLD_COMBAT_HEAVY:
            if (minimum == GAMEMUSIC_THRESHOLD_COMBAT_HEAVY)
            {
                break;
            }
            if (limited != GAMEMUSIC_THRESHOLD_SILENCE && limited < GAMEMUSIC_THRESHOLD_COMBAT_LIGHT)
            {
                next = GAMEMUSIC_THRESHOLD_AMBIENCE;
            }
            else if (limited != GAMEMUSIC_THRESHOLD_SILENCE && limited < GAMEMUSIC_THRESHOLD_COMBAT_HEAVY)
            {
                next = GAMEMUSIC_THRESHOLD_COMBAT_LIGHT;
            }
            else if (intensity < GAMEMUSIC_THRESHOLD_COMBAT_HEAVY && other_done)
            {
                next = GAMEMUSIC_THRESHOLD_COMBAT_HEAVY_LULL;
            }
            break;
        default:
            // Silence (and any other value).
            if (minimum > GAMEMUSIC_THRESHOLD_SILENCE)
            {
                changed = gamemusic_get_valid_upward_state(minimum);
            }
            else if (intensity >= GAMEMUSIC_THRESHOLD_COMBAT_LIGHT)
            {
                changed = gamemusic_get_valid_upward_state(intensity);
            }
            else if (other_done || game_state != AUDIOLIB_GAME_STATE_IN_LEVEL)
            {
                // Original: changed is always false here; rfg.exe tests it before the game state.
                next = GAMEMUSIC_THRESHOLD_AMBIENCE;
            }
            break;
        }
        if (next != GAMEMUSIC_THRESHOLD_INVALID)
        {
            Gamemusic_state.next_music.threshold = next;
            changed = true;
        }
    }
    // A level load drops the music to ambience.
    if (game_state == AUDIOLIB_GAME_STATE_LOADING && Gamemusic_state.curr_music.threshold > GAMEMUSIC_THRESHOLD_AMBIENCE)
    {
        Gamemusic_state.next_music.threshold = GAMEMUSIC_THRESHOLD_AMBIENCE;
        changed = true;
    }
    if (!Gamemusic_state.curr_music.active)
    {
        if (Gamemusic_state.next_music.music_id == -1 && Gamemusic_state.next_music.music_data != nullptr &&
            audiolib_interactive_music_get_id(Gamemusic_state.next_music.music_data->mtbl_name, &Gamemusic_state.next_music.music_id) < 0)
        {
            return;
        }
        changed = true;
    }
    else if (Gamemusic_state.minimum_threshold > Gamemusic_state.curr_music.threshold && Gamemusic_state.limited_level == GAMEMUSIC_THRESHOLD_SILENCE)
    {
        Gamemusic_state.next_music.threshold = Gamemusic_state.minimum_threshold;
        changed = true;
    }
    const bool reboot = audiolib_interactive_music_get_reboot_status();
    if (!changed && !reboot)
    {
        return;
    }
    gameaudio_music_state_info& next_music = Gamemusic_state.next_music;
    if (next_music.music_id != Gamemusic_state.curr_music.music_id)
    {
        // Another music table: stop the old one at once and start the new one in ambience (the limited level's state
        // when it is above ambience).
        audiolib_interactive_music_stop(Gamemusic_state.curr_music.music_id, 0);
        gamemusic_clean_music_info(&Gamemusic_state.curr_music);
        AUDIOLIB_MUSIC_STATE start_state;
        if (Gamemusic_state.limited_level <= GAMEMUSIC_THRESHOLD_AMBIENCE)
        {
            start_state = AUDIOLIB_MUSIC_STATE_AMBIENCE;
            // Original: a percent roll against the silence level's neutral percent that changes nothing (both
            // outcomes set ambience); rand() is still drawn. Magic number: 100.
            const int roll = rand() % 100;
            if ((double)(float)roll > (double)(float)(int)next_music.music_data->levels[GAMEMUSIC_THRESHOLD_SILENCE].neutral_pct)
            {
                next_music.threshold = GAMEMUSIC_THRESHOLD_AMBIENCE;
            }
        }
        else
        {
            start_state = gamemusic_convert_threshold_to_audiolib_state(Gamemusic_state.limited_level);
        }
        // Original: the threshold is ambience even when the limited level's state starts.
        next_music.threshold = GAMEMUSIC_THRESHOLD_AMBIENCE;
        next_music.state = start_state;
        next_music.volume = next_music.music_data->levels[GAMEMUSIC_THRESHOLD_AMBIENCE].volume;
        audiolib_interactive_music_start(next_music.music_id, start_state, next_music.volume);
    }
    else
    {
        next_music.state = gamemusic_convert_threshold_to_audiolib_state(next_music.threshold);
        next_music.volume = next_music.music_data->levels[next_music.threshold].volume;
        audiolib_interactive_music_transition(reboot, next_music.music_id, next_music.state, next_music.volume, false);
    }
    if (reboot)
    {
        audiolib_interactive_music_clear_reboot();
    }
    Gamemusic_state.curr_music = next_music;
    Gamemusic_state.curr_music.active = true;
    gamemusic_clean_music_info(&next_music);
    gameaudio_music_state_info& curr_music = Gamemusic_state.curr_music;
    const int now = timer_get(TIMER_MILLISECONDS);
    const gamemusic_level_data& level = curr_music.music_data->levels[curr_music.threshold];
    if (silence_done || Gamemusic_curr_threshold > GAMEMUSIC_THRESHOLD_AMBIENCE || curr_music.threshold > GAMEMUSIC_THRESHOLD_AMBIENCE || curr_music.silence_time <= 0)
    {
        // The next silence in the level's range from now (none when the range gives 0).
        const int range = level.max_time_silence - level.min_time_silence;
        int silence = 0;
        if (range != 0)
        {
            silence = rand() % range + level.min_time_silence;
        }
        curr_music.silence_time = silence > 0 ? silence + now : -1;
    }
    // Original: a random draw for the level's other time range is made and thrown away; the other timer is always
    // 22 s (magic number 22000 ms).
    if (level.max_time_other != level.min_time_other)
    {
        rand();
    }
    curr_music.other_time = now + 22000;
}

// 0x007BF0B0 (:2122). A random track number of the team's set (EDF 1-5, guerrillas 1-6), not the last one.
int multiplayer_select_cue_number(bool edf)
{
    // Magic numbers: 5 EDF tracks, 6 guerrilla tracks.
    const int count = edf ? 5 : 6;
    const int selection = int_rand_range(1, count);
    if (selection != multi_player_cue_selection)
    {
        multi_player_cue_selection = selection;
        return selection;
    }
    multi_player_cue_selection++;
    if (multi_player_cue_selection > count)
    {
        multi_player_cue_selection = 1;
    }
    return multi_player_cue_selection;
}

// 0x007BF100 (:2149). One of the five combat tracks of the player's team, not the last one.
// Original: the selection is inlined with five tracks for both teams (multiplayer_select_cue_number has six for the
// guerrillas).
void multiplayer_music_play_cue()
{
    int cue_id = -1;
    const char* prefix = multi_game_get_my_team_music() == HUMAN_TEAM_EDF ? "MUS_MP_EDF_COMBAT_MEDIUM_0" : "MUS_MP_GUR_COMBAT_MEDIUM_0";
    // Magic number: 5 tracks.
    int selection = int_rand_range(1, 5);
    if (selection == multi_player_cue_selection)
    {
        selection = multi_player_cue_selection + 1;
        if (selection > 5)
        {
            selection = 1;
        }
    }
    multi_player_cue_selection = selection;
    char cue_name[64];
    sprintf(cue_name, "%s%d", prefix, selection);
    if (audiolib_cue_get_id(cue_name, &cue_id) == AUDIOLIB_ERR_NONE)
    {
        multiplayer_music_play_id = audiolib_cue_play(cue_id, nullptr, nullptr);
    }
}

// The play no longer plays (inlined): its state cannot be read, or it has stopped.
static bool gamemusic_play_ended(int play_id)
{
    audiolib_play_state state;
    return audiolib_play_get_state(play_id, &state) != AUDIOLIB_ERR_NONE || state >= AUDIOLIB_PLAY_STATE_STOPPED;
}

// 0x007BF1B0 (:2165). The next track when the last one has ended.
void multiplayer_music_update()
{
    if (multiplayer_music_play_id == -1 || gamemusic_play_ended(multiplayer_music_play_id))
    {
        multiplayer_music_play_cue();
    }
}

// 0x007BF1F0 (:2181)
void multiplayer_music_stop()
{
    if (multiplayer_music_play_id != -1)
    {
        // Magic number: a 5 s fade.
        audiolib_play_fade_out(multiplayer_music_play_id, 5000);
        multiplayer_music_play_id = -1;
    }
}

// 0x007BF220 (:2189). The front end track: the title suite (fading out the multiplayer one) or the multiplayer suite
// (fading out the title one), started again when it has ended.
void frontend_music_update()
{
    int cue_id = -1;
    char cue_name[64];
    if (gamemusic_current_music_type == GAMEMUSIC_MUSIC_FRONTEND)
    {
        if (frontend_mp_music_play_id != -1)
        {
            // Magic number: a 1 s fade.
            audiolib_play_fade_out(frontend_mp_music_play_id, 1000);
            frontend_mp_music_play_id = -1;
        }
        if (frontend_music_play_id != -1 && !gamemusic_play_ended(frontend_music_play_id))
        {
            return;
        }
        // Magic numbers: suites 1 to 3.
        sprintf(cue_name, "%s%d", "MUS_TITLE_THEME_SUITE_", int_rand_range(1, 3));
        if (audiolib_cue_get_id(cue_name, &cue_id) == AUDIOLIB_ERR_NONE)
        {
            frontend_music_play_id = audiolib_cue_play(cue_id, nullptr, nullptr);
        }
        return;
    }
    if (gamemusic_current_music_type != GAMEMUSIC_MUSIC_FRONTEND_MP)
    {
        return;
    }
    if (frontend_music_play_id != -1)
    {
        audiolib_play_fade_out(frontend_music_play_id, 1000);
        frontend_music_play_id = -1;
    }
    if (frontend_mp_music_play_id != -1 && !gamemusic_play_ended(frontend_mp_music_play_id))
    {
        return;
    }
    strcpy(cue_name, "MUS_MP_THEME_SUITE_1");
    if (audiolib_cue_get_id(cue_name, &cue_id) == AUDIOLIB_ERR_NONE)
    {
        frontend_mp_music_play_id = audiolib_cue_play(cue_id, nullptr, nullptr);
    }
}

// 0x007BF450 (:2258)
void frontend_music_stop(int fade_time_ms)
{
    if (frontend_music_play_id != -1)
    {
        audiolib_play_fade_out(frontend_music_play_id, fade_time_ms);
        frontend_music_play_id = -1;
    }
    if (frontend_mp_music_play_id != -1)
    {
        audiolib_play_fade_out(frontend_mp_music_play_id, fade_time_ms);
        frontend_mp_music_play_id = -1;
    }
}

// 0x007CC750 (:2270). Multiplayer music from now on: the front end music fades out, the interactive music stops.
void gamemusic_multiplayer_start()
{
    gamemusic_current_music_type = GAMEMUSIC_MUSIC_MULTIPLAYER;
    if (frontend_music_play_id != -1)
    {
        audiolib_play_fade_out(frontend_music_play_id, 1000);
        frontend_music_play_id = -1;
    }
    if (frontend_mp_music_play_id != -1)
    {
        audiolib_play_fade_out(frontend_mp_music_play_id, 1000);
        frontend_mp_music_play_id = -1;
    }
    gamemusic_forced_stop = true;
    gamemusic_stop();
    gamemusic_forced_stop = false;
    multiplayer_music_update();
}

// 0x007BF4A0 (:2285). The match is over: the multiplayer track fades out (5 s), the multiplayer suite plays.
void gamemusic_multiplayer_end()
{
    multiplayer_music_stop();
    gamemusic_current_music_type = GAMEMUSIC_MUSIC_FRONTEND_MP;
    frontend_music_update();
}

// 0x007CC7F0 (:2297)
void gamemusic_frontend_start(bool multiplayer)
{
    gamemusic_forced_stop = true;
    gamemusic_stop();
    gamemusic_forced_stop = false;
    multiplayer_music_stop();
    gamemusic_current_music_type = (gamemusic_music_types)(GAMEMUSIC_MUSIC_FRONTEND + (multiplayer ? 1 : 0));
    frontend_music_update();
}

// 0x007BF4E0 (:2317)
void gamemusic_frontend_stop(int fade_time_ms)
{
    frontend_music_stop(fade_time_ms);
    gamemusic_current_music_type = GAMEMUSIC_MUSIC_NONE;
}

// 0x007CC840 (:2324)
void gamemusic_interactive_stop()
{
    if (gamemusic_current_music_type == GAMEMUSIC_MUSIC_INTERACTIVE)
    {
        gamemusic_forced_stop = true;
        gamemusic_stop();
        gamemusic_forced_stop = false;
        gamemusic_current_music_type = GAMEMUSIC_MUSIC_NONE;
    }
}

// 0x007D83C0 (:2341). Each frame (gameplay, loading, the main menu). Nothing until the music wavebank is ready; with
// no music type, entering a level fades the front end music out and starts the interactive (single player) or the
// multiplayer music.
void gamemusic_update(bool game_paused)
{
    if (gamemusic_shutdown)
    {
        return;
    }
    if (!gamemusic_inited)
    {
        if (!audiolib_music_wavebank_ready())
        {
            return;
        }
        gamemusic_inited = true;
    }
    switch (gamemusic_current_music_type)
    {
    case GAMEMUSIC_MUSIC_NONE:
        if (audiolib_system_get_game_state() == AUDIOLIB_GAME_STATE_IN_LEVEL)
        {
            frontend_music_stop(1000);
            gamemusic_current_music_type = Multiplayer ? GAMEMUSIC_MUSIC_MULTIPLAYER : GAMEMUSIC_MUSIC_INTERACTIVE;
        }
        return;
    case GAMEMUSIC_MUSIC_FRONTEND:
    case GAMEMUSIC_MUSIC_FRONTEND_MP:
        frontend_music_update();
        return;
    case GAMEMUSIC_MUSIC_MULTIPLAYER:
        multiplayer_music_update();
        return;
    default:
        break;
    }
    if (!game_paused && !Multiplayer)
    {
        gamemusic_compute_current_intensity_single_player();
    }
    gamemusic_check_special_cases();
    const int curr_time = timer_get(TIMER_MILLISECONDS);
    if (!gamemusic_check_night_day_transition() && !gamemusic_check_marauder_music())
    {
        gamemusic_set_current_music(curr_time, game_paused);
    }
}

// 0x007D8490 (:2425). The state, the intensity defaults (overridden by the tables), the tables, the first intensity
// and ambience as the limited level; then a first update. Returns whether the music wavebank was ready.
bool gamemusic_system_init(mempool* memory_pool)
{
    if (gamemusic_inited)
    {
        return true;
    }
    for (GAMEMUSIC_THRESHOLDS& level : gamemusic_limited_levels)
    {
        level = GAMEMUSIC_THRESHOLD_SILENCE;
    }
    memset(&Gamemusic_state, 0, sizeof(Gamemusic_state));
    gamemusic_clean_music_info(&Gamemusic_state.curr_music);
    gamemusic_clean_music_info(&Gamemusic_state.next_music);
    memset(Gamemusic_curr_intensity_factors, 0, sizeof(Gamemusic_curr_intensity_factors));
    // Magic numbers: the default reductions and threshold percentages.
    Gamemusic_intensity_reductions[GAMEMUSIC_THRESHOLD_COMBAT_LIGHT] = 4;
    Gamemusic_intensity_reductions[GAMEMUSIC_THRESHOLD_COMBAT_HEAVY_LULL] = 4;
    Gamemusic_intensity_reductions[GAMEMUSIC_THRESHOLD_COMBAT_HEAVY] = 4;
    Gamemusic_intensity_thresholds[GAMEMUSIC_THRESHOLD_SILENCE] = 0;
    Gamemusic_intensity_thresholds[GAMEMUSIC_THRESHOLD_AMBIENCE] = 0;
    Gamemusic_intensity_thresholds[GAMEMUSIC_THRESHOLD_COMBAT_LIGHT_LULL] = 20;
    Gamemusic_intensity_thresholds[GAMEMUSIC_THRESHOLD_COMBAT_LIGHT] = 25;
    Gamemusic_intensity_thresholds[GAMEMUSIC_THRESHOLD_COMBAT_HEAVY_LULL] = 50;
    Gamemusic_intensity_thresholds[GAMEMUSIC_THRESHOLD_COMBAT_HEAVY] = 80;
    Gamemusic_intensity_reductions[GAMEMUSIC_THRESHOLD_SILENCE] = 0;
    Gamemusic_intensity_reductions[GAMEMUSIC_THRESHOLD_AMBIENCE] = 0;
    Gamemusic_intensity_reductions[GAMEMUSIC_THRESHOLD_COMBAT_LIGHT_LULL] = 0;
    gamemusic_parse_music_xtables(memory_pool);
    gamemusic_compute_current_intensity_single_player();
    gamemusic_restart_music.set(0);
    Gamemusic_state.limited_level = GAMEMUSIC_THRESHOLD_AMBIENCE;
    gamemusic_push_limited_level(GAMEMUSIC_THRESHOLD_AMBIENCE);
    Gamemusic_intensity_count = 1;
    audiolib_interactive_music_system_set_limited_level(true);
    gamemusic_update(false);
    return gamemusic_inited;
}

// 0x007BF530 (:2499)
void gamemusic_system_shutdown()
{
    gamemusic_shutdown = true;
}

// 0x007BF540 (:2510)
GAMEMUSIC_THRESHOLDS game_music_get_index_from_name(const char* name)
{
    for (int i = 0; i < NUM_GAMEMUSIC_LEVEL_THRESHOLDS; i++)
    {
        if (v_stricmp(name, Gamemusic_intensity_threshold_names[i]) == 0)
        {
            return (GAMEMUSIC_THRESHOLDS)i;
        }
    }
    return GAMEMUSIC_THRESHOLD_SILENCE;
}

// 0x007BF580 (:2527)
bool gamemusic_start(char* music_name)
{
    if (!gamemusic_inited || music_name == nullptr)
    {
        return false;
    }
    strncpy(music_file_name, music_name, sizeof(music_file_name));
    return true;
}

// 0x007BF5B0 (:2541). Stops the interactive music (with the pending fade time, then none).
bool gamemusic_stop()
{
    if (!gamemusic_inited)
    {
        return false;
    }
    if (Gamemusic_state.curr_music.music_id != -1)
    {
        audiolib_interactive_music_stop(Gamemusic_state.curr_music.music_id, gamemusic_fade_time);
        gamemusic_fade_time = 0;
        Gamemusic_state.curr_music.music_id = -1;
        Gamemusic_state.curr_music.active = false;
        Gamemusic_state.next_music.music_data = nullptr;
        Gamemusic_state.next_music.music_id = -1;
    }
    return true;
}

// 0x007BF610 (:2558)
GAMEMUSIC_THRESHOLDS gamemusic_get_minimum_music_level()
{
    return Gamemusic_state.minimum_threshold;
}

// 0x007BF620 (:2563). Light or heavy combat; anything else clears it.
void gamemusic_set_minimum_music_level(GAMEMUSIC_THRESHOLDS level)
{
    if (level == GAMEMUSIC_THRESHOLD_COMBAT_LIGHT || level == GAMEMUSIC_THRESHOLD_COMBAT_HEAVY)
    {
        Gamemusic_state.minimum_threshold = level;
    }
    else
    {
        Gamemusic_state.minimum_threshold = GAMEMUSIC_THRESHOLD_SILENCE;
    }
}

// 0x007BF650 (:2585)
GAMEMUSIC_THRESHOLDS gamemusic_get_limited_music_level()
{
    return Gamemusic_state.limited_level;
}

// 0x007BF660 (:2589). Ambience, light or heavy combat push a limited level; anything else pops the latest one (the
// one below it becomes the limit; none left means no limit).
void gamemusic_set_limited_music_level(GAMEMUSIC_THRESHOLDS level)
{
    GAMEMUSIC_THRESHOLDS new_level;
    if (level == GAMEMUSIC_THRESHOLD_AMBIENCE || level == GAMEMUSIC_THRESHOLD_COMBAT_LIGHT || level == GAMEMUSIC_THRESHOLD_COMBAT_HEAVY)
    {
        new_level = level;
        Gamemusic_state.limited_level = new_level;
        gamemusic_push_limited_level(new_level);
    }
    else
    {
        for (int i = 7; i >= 0; i--)
        {
            if (gamemusic_limited_levels[i] != GAMEMUSIC_THRESHOLD_SILENCE)
            {
                gamemusic_limited_levels[i] = GAMEMUSIC_THRESHOLD_SILENCE;
                if (i < 1)
                {
                    Gamemusic_state.limited_level = GAMEMUSIC_THRESHOLD_SILENCE;
                    audiolib_interactive_music_system_set_limited_level(false);
                    return;
                }
                Gamemusic_state.limited_level = gamemusic_limited_levels[i - 1];
                break;
            }
        }
        new_level = Gamemusic_state.limited_level;
    }
    if (new_level == GAMEMUSIC_THRESHOLD_AMBIENCE)
    {
        Gamemusic_intensity_count = 1;
    }
    else if (new_level == GAMEMUSIC_THRESHOLD_SILENCE)
    {
        audiolib_interactive_music_system_set_limited_level(false);
        return;
    }
    audiolib_interactive_music_system_set_limited_level(true);
}

// 0x007BF720 (:2655). Back to the progression music after a 5 s fade, restarting in 5 s.
void gamemusic_mission_canceled()
{
    gamemusic_forced_stop = true;
    gamemusic_fade_time = 5000;
    gamemusic_stop();
    strcpy(music_file_name, Gamemusic_Progression_File_Names[Gamemusic_Progression_File]);
    Gamemusic_intensity_count = 1;
    gamemusic_stop_music_event = true;
    gamemusic_restart_music.set(5000);
    Gamemusic_state.minimum_threshold = GAMEMUSIC_THRESHOLD_SILENCE;
}

// 0x007D8630 (:2678). Two story missions move the progression on (to 3 and to 2, day or night by the clock).
void gamemusic_mission_completed(mission_info* mission)
{
    if (mission == nullptr)
    {
        return;
    }
    bool switch_files = true;
    if (strcmp(mission->name, "Guns of Tharsis") == 0 && Gamemusic_Progression_File <= GAMEMUSIC_PROGRESSION_02_NIGHT)
    {
        Gamemusic_Progression_File = (gamemusic_music_files)(GAMEMUSIC_PROGRESSION_03_DAY + (game_clock_get_time_of_day() > 0 ? 1 : 0));
    }
    else if (strcmp(mission->name, "Death From Above") == 0 && Gamemusic_Progression_File <= GAMEMUSIC_PROGRESSION_01_NIGHT)
    {
        Gamemusic_Progression_File = (gamemusic_music_files)(GAMEMUSIC_PROGRESSION_02_DAY + (game_clock_get_time_of_day() > 0 ? 1 : 0));
    }
    else
    {
        switch_files = false;
    }
    if (switch_files)
    {
        gamemusic_switch_music_files();
    }
    Gamemusic_intensity_count = 1;
    Gamemusic_state.minimum_threshold = GAMEMUSIC_THRESHOLD_SILENCE;
    gamemusic_compute_current_intensity_single_player();
}

// 0x007CC870 (:2710)
void gamemusic_system_load(cfile* file)
{
    const gamemusic_music_files progression_file = (gamemusic_music_files)cf_read_uint32(file);
    if ((int)progression_file < NUM_GAMEMUSIC_FILES && progression_file != Gamemusic_Progression_File)
    {
        Gamemusic_Progression_File = progression_file;
        gamemusic_switch_music_files();
    }
}

// 0x007BF790 (:2721)
void gamemusic_system_save(cfile* file)
{
    cf_write_uint32(Gamemusic_Progression_File, file);
}

// 0x007BF7B0 (:2726). The player's death or an activity's end stops the music (3 s fade) for 5 s; a mission's or
// district's end plays the exit cue and restarts in 2.5 s; the end of a UI blur restarts in 2 s.
void gamemusic_event_occurred(GAMEMUSIC_EVENT event)
{
    if (event == GAMEMUSIC_UI_BLUR_END)
    {
        gamemusic_restart_music.set(2000);
        return;
    }
    gamemusic_stop_music_event = true;
    gamemusic_forced_stop = true;
    switch (event)
    {
    case GAMEMUSIC_PLAYER_DIED:
    case GAMEMUSIC_ACTIVITY_ENDED:
        gamemusic_fade_time = 3000;
        gamemusic_restart_music.set(5000);
        return;
    case GAMEMUSIC_MISSION_FAILED_PLAYER_ALIVE:
    case GAMEMUSIC_MISSION_SUCCEEDED:
    case GAMEMUSIC_DISTRICT_LIBERATED:
        audiolib_music_play_exit_cue();
        gamemusic_restart_music.set(2500);
        return;
    default:
        gamemusic_stop_music_event = false;
        gamemusic_forced_stop = false;
        return;
    }
}

// 0x007BF840 (:2776). Console: the music category's volume.
void unused_dcf_music_volume()
{
    if (Console_command != 0 && console_get_arg(0x10, 0)) // Magic number: argument type 0x10, a float
    {
        float volume = Console_arg_float;
        if (volume < 0.0f)
        {
            volume = 0.0f;
        }
        short category_id;
        if (audiolib_category_get_id("Music", &category_id) == AUDIOLIB_ERR_NONE)
        {
            audiolib_category_set_volume(category_id, volume);
        }
    }
    if (Console_help != 0)
    {
        char* text = Gamemusic_console_text;
        sprintf(text, "Usage: music_volume <0.0 - 1.0>\n");
        console_output(text, nullptr, false);
    }
}

// 0x007BF8F0 (:2796). Console: the minimum level (h heavy, l light, else none).
void unused_dcf_music_minimum()
{
    char* text = Gamemusic_console_text;
    if (Console_command != 0)
    {
        if (console_get_arg(2, 0)) // Magic number: argument type 2, a string
        {
            if (Console_arg[0] == 'h')
            {
                Gamemusic_state.minimum_threshold = GAMEMUSIC_THRESHOLD_COMBAT_HEAVY;
            }
            else if (Console_arg[0] == 'l')
            {
                Gamemusic_state.minimum_threshold = GAMEMUSIC_THRESHOLD_COMBAT_LIGHT;
            }
            else
            {
                Gamemusic_state.minimum_threshold = GAMEMUSIC_THRESHOLD_SILENCE;
            }
        }
        sprintf(text, "music_minimum set to: %s\n", Gamemusic_intensity_threshold_names[Gamemusic_state.minimum_threshold] + 1);
        console_output(text, nullptr, false);
    }
    if (Console_help != 0)
    {
        sprintf(text, "Usage: music_minimum <s | n | m | l | h>\n");
        console_output(text, nullptr, false);
        sprintf(text, "       l = combat light, h = combat heavy, m = melodic, n = neutral, s = silence\n");
        console_output(text, nullptr, false);
    }
    text[0] = '\0';
}

// 0x007BF9E0 (:2827). Console: the limited level (h heavy, l light, a ambience, else pop).
void unused_dcf_music_limited()
{
    char* text = Gamemusic_console_text;
    if (Console_command != 0)
    {
        if (console_get_arg(2, 0)) // Magic number: argument type 2, a string
        {
            if (Console_arg[0] == 'h')
            {
                // Original: heavy combat is pushed directly (gamemusic_set_limited_music_level inlined without it).
                Gamemusic_state.limited_level = GAMEMUSIC_THRESHOLD_COMBAT_HEAVY;
                gamemusic_push_limited_level(GAMEMUSIC_THRESHOLD_COMBAT_HEAVY);
                audiolib_interactive_music_system_set_limited_level(true);
            }
            else
            {
                GAMEMUSIC_THRESHOLDS level = GAMEMUSIC_THRESHOLD_SILENCE;
                if (Console_arg[0] == 'l')
                {
                    level = GAMEMUSIC_THRESHOLD_COMBAT_LIGHT;
                }
                else if (Console_arg[0] == 'a')
                {
                    level = GAMEMUSIC_THRESHOLD_AMBIENCE;
                }
                gamemusic_set_limited_music_level(level);
            }
        }
        sprintf(text, "music_limited set to: %s\n", Gamemusic_intensity_threshold_names[Gamemusic_state.limited_level] + 1);
        console_output(text, nullptr, false);
    }
    if (Console_help != 0)
    {
        sprintf(text, "Usage: music_limited <s | n | m | l | h>\n");
        console_output(text, nullptr, false);
        sprintf(text, "       l = combat light, h = combat heavy, m = melodic, n = neutral, s = silence\n");
        console_output(text, nullptr, false);
    }
    text[0] = '\0';
}
