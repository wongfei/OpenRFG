// Original: volition/rfg/code/gamewide/gameaudio.cpp
//
// The game's audio driver over audio_lib (SFX.md SX3): the two init stages and shutdown, game_audio.xtbl (the low health
// filter, the 2D/3D crossover cue pairs, the cue ids of Ga_cue_ids), the stream2 bank callbacks, the per-frame update
// (listener, the 26 listener LOS rays, the wind, the low health filter), cross-linked sounds and the debug console
// commands.
//
// In rfg.exe this file shares a unity TU with gamewide.cpp and gamemusic.cpp. Here it is a TU of rfg_audio, so the
// statics the three share have external linkage (each marked at its definition), and gamewide_shutdown calls
// gameaudio_system_shutdown where rfg.exe inlines it.

#include "volition/rfg/code/gamewide/gameaudio.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_enums.h"
#include "volition/audio_lib/code/audiolib_foley_category.h"
#include "volition/audio_lib/code/audiolib_soundbank.h"
#include "volition/audio_lib/code/audiolib_thread.h"
#include "volition/audio_lib/code/audiolib_vehicle_engine.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/gamewide/game_stream2_prims.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/weather/wind.h"
#include "volition/stream2/code/stream2.h"
#include "volition/stream2/code/stream2_allocator.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ---- file-local types (rfg.pdb)
// size 0x14
struct LOS_collision_info_struct
{
    AUDIOLIB_RAYCAST_DIRECTION raycast_dir; // 0x00
    vector dir; // 0x04
    float value; // 0x10
};
static_assert(sizeof(LOS_collision_info_struct) == 0x14, "LOS_collision_info_struct");

// size 0x6C
struct LOS_data_struct
{
    LOS_data_struct() // 0x007BB860
    {
    }

    int num_objects; // 0x00
    unsigned int hit_object_handle[4]; // 0x04
    vector pos[4]; // 0x14
    float distance[4]; // 0x44
    audiolib_material_type material[4]; // 0x54
    float value; // 0x64
    bool hit; // 0x68
};
static_assert(sizeof(LOS_data_struct) == 0x6C, "LOS_data_struct");

// size 0x8: a 2D cue and the 3D cue that plays for it away from the local player (game_audio.xtbl Crossover).
struct crossover_link_2d_3d
{
    int twod; // 0x00
    int threed; // 0x04
};
static_assert(sizeof(crossover_link_2d_3d) == 0x8, "crossover_link_2d_3d");

// size 0x8
struct crossover_list_data_struct
{
    int count; // 0x00
    crossover_link_2d_3d* data; // 0x04
};
static_assert(sizeof(crossover_list_data_struct) == 0x8, "crossover_list_data_struct");

// ---- globals
gameaudio_low_health_info low_health; // 0x0252D6C4
global_sound_cue_id_struct Ga_cue_ids; // 0x02532CC0
bool Audio_stage_1_init; // 0x02533848
bool Audio_stage_2_init; // 0x02533849
// Public in rfg.pdb but declared in no header: only the tweak table items below and the falling wind read them.
float Falling_wind_speed_increase_mult; // 0x0227AEE0
float Falling_wind_speed_max_increase; // 0x025328F0
// Public in rfg.pdb, declared in no header. human_voice_clear_or_radio (human_voice.cpp) reads the crossover distance.
float gameaudio_clr_radio_crossover_distance = 4.0f; // 0x0165CB0C
int Gameaudio_low_health_delay = 500; // 0x0165CB10, ms below the low health level before the filter starts
float curr_health_pct = 1.0f; // 0x0165EB64
stream2_mempool_allocator Stream_audio_allocator; // 0x0165E508
crossover_list_data_struct crossover_link_data; // 0x025054E4
void (*audiolib_loaded_callback_func)(const char*, void*, unsigned char*, int); // 0x02505524
void (*audiolib_unloaded_callback_func)(const char*, void*); // 0x02532C34

// Shared with gamewide.cpp and gamemusic.cpp (one unity TU in rfg.exe, see the top). gamemusic_shutdown is external
// for gamemusic.cpp; Sound and the event handle only gamewide_shutdown's inlined copy of gameaudio_system_shutdown
// used outside this file.
static bool Sound = true; // 0x0165C942
static notify_info* gameaudio_gem_handle; // 0x0227AEE4
bool gamemusic_shutdown; // 0x0253384B

// gamemusic.cpp's (static in rfg.exe's unity TU): the single player progression music file, for the save game.
extern gamemusic_music_files Gamemusic_Progression_File; // 0x0253385C
void gamemusic_switch_music_files(); // 0x007CBAE0 (gamemusic.cpp:413)

// ---- file-static globals
static timestamp Gameaudio_low_health_timer; // 0x0165E2E4, invalid (-1) from the constructor
static tweak_table_item Tweak_table_item_Falling_wind_speed_increase_mult("Falling_wind_speed_increase_mult", &Falling_wind_speed_increase_mult); // 0x0165E2E8
static tweak_table_item Tweak_table_item_Falling_wind_speed_max_increase("Falling_wind_speed_max_increase", &Falling_wind_speed_max_increase); // 0x0165E2F4
// The 26 listener LOS rays: direction, unit-ish vector (0.707 diagonals, 0.353 down components) and weight.
static LOS_collision_info_struct LOS_cardinal_directions[26] = { // 0x0165E300
    {AUDIOLIB_RAYCAST_DIRECTION_UP, {0.0f, 1.0f, 0.0f}, 2.0f},
    {AUDIOLIB_RAYCAST_DIRECTION_DOWN, {0.0f, -1.0f, 0.0f}, 0.1f},
    {AUDIOLIB_RAYCAST_DIRECTION_WEST_UP, {-1.0f, 0.707f, 0.0f}, 1.0f},
    {AUDIOLIB_RAYCAST_DIRECTION_EAST_DOWN, {1.0f, -0.353f, 0.0f}, 0.1f},
    {AUDIOLIB_RAYCAST_DIRECTION_NORTH, {0.0f, 0.0f, 1.0f}, 0.5f},
    {AUDIOLIB_RAYCAST_DIRECTION_SOUTH, {0.0f, 0.0f, -1.0f}, 0.5f},
    {AUDIOLIB_RAYCAST_DIRECTION_EAST_UP, {1.0f, 0.707f, 0.0f}, 1.0f},
    {AUDIOLIB_RAYCAST_DIRECTION_WEST_DOWN, {-1.0f, -0.353f, 0.0f}, 0.1f},
    {AUDIOLIB_RAYCAST_DIRECTION_SOUTH_UP, {0.0f, 0.707f, -1.0f}, 1.0f},
    {AUDIOLIB_RAYCAST_DIRECTION_NORTH_DOWN, {0.0f, -0.353f, 1.0f}, 0.1f},
    {AUDIOLIB_RAYCAST_DIRECTION_WEST, {-1.0f, 0.0f, 0.0f}, 0.5f},
    {AUDIOLIB_RAYCAST_DIRECTION_EAST, {1.0f, 0.0f, 0.0f}, 0.5f},
    {AUDIOLIB_RAYCAST_DIRECTION_SOUTH_DOWN, {0.0f, -0.353f, -1.0f}, 0.1f},
    {AUDIOLIB_RAYCAST_DIRECTION_NORTH_UP, {0.0f, 0.707f, 1.0f}, 1.0f},
    {AUDIOLIB_RAYCAST_DIRECTION_NORTH_WEST_UP, {-0.707f, 0.707f, 0.707f}, 1.0f},
    {AUDIOLIB_RAYCAST_DIRECTION_SOUTH_EAST_DOWN, {0.707f, -0.353f, -0.707f}, 0.1f},
    {AUDIOLIB_RAYCAST_DIRECTION_NORTH_EAST_UP, {0.707f, 0.707f, 0.707f}, 1.0f},
    {AUDIOLIB_RAYCAST_DIRECTION_SOUTH_WEST_DOWN, {-0.707f, -0.353f, -0.707f}, 0.1f},
    {AUDIOLIB_RAYCAST_DIRECTION_NORTH_WEST, {-0.707f, 0.0f, 0.707f}, 0.5f},
    {AUDIOLIB_RAYCAST_DIRECTION_SOUTH_EAST, {0.707f, 0.0f, -0.707f}, 0.5f},
    {AUDIOLIB_RAYCAST_DIRECTION_SOUTH_WEST_UP, {-0.707f, 0.707f, -0.707f}, 1.0f},
    {AUDIOLIB_RAYCAST_DIRECTION_NORTH_EAST_DOWN, {0.707f, -0.353f, 0.707f}, 0.1f},
    {AUDIOLIB_RAYCAST_DIRECTION_SOUTH_EAST_UP, {0.707f, 0.707f, -0.707f}, 1.0f},
    {AUDIOLIB_RAYCAST_DIRECTION_NORTH_WEST_DOWN, {-0.707f, -0.353f, 0.707f}, 0.1f},
    {AUDIOLIB_RAYCAST_DIRECTION_NORTH_EAST, {0.707f, 0.0f, 0.707f}, 0.5f},
    {AUDIOLIB_RAYCAST_DIRECTION_SOUTH_WEST, {-0.707f, 0.0f, -0.707f}, 0.5f},
};
// vector2* Tmp2[32] (0x01CD2950) is used here too; placed in polyclip.cpp.
static int wind_cue_id; // 0x0227AFCC
static int dc_play_sound_3d_foley_info; // 0x02505518
static int dc_play_sound_foley_info; // 0x0251129C
static int wind_play_id; // 0x02512560
static float Current_listener_LOS; // 0x0252D2BC
// Never set in rfg.exe (gameaudio_streaming_file_initialize has no caller): audio_lib gets no game file loader.
static bool Gameaudio_streaming_file_initialize_result; // 0x0253384A
static float wind_mph; // 0x0253384C
static float wind_LOS; // 0x02533854
static int LOS_cardinal_direction; // 0x02533858
// gamemusic_music_files Gamemusic_Progression_File (0x0253385C) is used here too; placed in gamemusic.cpp.
static LOS_data_struct LOS_data[26]; // 0x02534B60, indexed by AUDIOLIB_RAYCAST_DIRECTION
// LOS_data[26] (0x02534B60): the loop over LOS_data[i].hit ends near 0x025356B4, which Ghidra names Custom_DLC_boolReplace_crates_with_totems (not a static of this file).

// The console commands' per-thread text buffer (TLS +0x38, as rfg_dynamics.cpp's).
static __declspec(thread) char Gameaudio_console_text[1024];

// ---- internal functions
static void gameaudio_LOS_init(); // 0x007BB870 (:482)
static audiolib_material_type gameaudio_material_lookup(rfg_effect_material* effect_material, AUDIOLIB_RAYCAST_DIRECTION dir); // 0x007BB890 (:496)
static void gameaudio_update_listener_los(); // 0x007ED250 (:532)
static void ga_register_for_global_events(); // 0x007BB950 (:702)

// 0x007BB310 (:122)
void unused_dcf_gameaudio_restart()
{
    char* text = Gameaudio_console_text;
    const bool command = Console_command != 0;
    text[0] = '\0';
    if (command)
    {
        keen::debug::nativePrintArgs("This command dosen't work, because gameaudio_system_init_stage_2() need a ptr to soundsystem!");
    }
    if (Console_help != 0)
    {
        sprintf(text, "Usage: gameaudio_restart\n");
    }
    console_output(text, nullptr, false);
}

// 0x007BB370 (:145)
void unused_dcf_sound_reset_stats()
{
    audiolib_stats_reset(nullptr);
}

// 0x007BB380 (:151)
void unused_dcf_sound()
{
    char* text = Gameaudio_console_text;
    if (Console_command != 0)
    {
        // Magic numbers: argument type bits 0xC1 (true, false, none); 0x40 true, 0x80 false, 1 toggle.
        if (!console_get_arg(0xC1, 1))
        {
            sprintf(text, "Invalid syntax");
        }
        else
        {
            bool set = true;
            bool enable = false;
            if ((Console_arg_type & 0x40) != 0)
            {
                enable = true;
            }
            else if ((Console_arg_type & 0x80) != 0)
            {
                enable = false;
            }
            else if ((Console_arg_type & 1) != 0)
            {
                enable = !audiolib_get_sound_enabled();
            }
            else
            {
                set = false;
            }
            if (set)
            {
                audiolib_set_sound_enabled(enable);
            }
            sprintf(text, "Sound is now %s\n", audiolib_get_sound_enabled() ? "on" : "off");
        }
    }
    if (Console_status != 0)
    {
        sprintf(text, "Sound is currently %s\n", audiolib_get_sound_enabled() ? "on" : "off");
    }
    if (Console_help != 0)
    {
        sprintf(text, "Usage: sound <on|off>\n");
    }
    console_output(text, nullptr, false);
}

// 0x007BB490 (:182)
void unused_dcf_sound_play()
{
    char* text = Gameaudio_console_text;
    if (Console_command != 0)
    {
        // Magic number: argument type 6 (a string).
        if (console_get_arg(6, 0))
        {
            char cue_name[128];
            cue_name[0] = '\0';
            // The argument without its trailing quotes and its directory, upper case.
            char* p = Console_arg_org + strlen(Console_arg_org) - 1;
            while (*p == '"')
            {
                *p = '\0';
                p--;
            }
            while (Console_arg_org < p)
            {
                if (*p == '\\' || *p == '/')
                {
                    p++;
                    break;
                }
                p--;
            }
            strncpy(cue_name, p, sizeof(cue_name));
            for (char* c = cue_name; *c != '\0'; c++)
            {
                if (*c >= 'a' && *c <= 'z')
                {
                    *c -= 0x20;
                }
            }
            if (audiolib_cue_get_id(cue_name, &dc_play_sound_foley_info) == AUDIOLIB_ERR_NONE)
            {
                sprintf(text, "sound_play:: name:%s (%s)", Console_arg_org, cue_name);
                console_output(text, nullptr, false);
                // Magic number: argument type 2 (an optional integer), read and ignored.
                console_get_arg(2, 1);
            }
            sprintf(text, "\n");
            console_output(text, nullptr, false);
            if (!audiolib_cue_is_id_valid(dc_play_sound_foley_info))
            {
                sprintf(text, "Cue ID not valid %s !\n", cue_name);
            }
            else if (audiolib_cue_play(dc_play_sound_foley_info, nullptr, nullptr) == -1)
            {
                sprintf(text, "FAILED to play %s \n", cue_name);
            }
            else
            {
                sprintf(text, "playing %s \n", cue_name);
            }
        }
        else
        {
            sprintf(text, "Invalid syntax");
        }
    }
    if (Console_help != 0)
    {
        sprintf(text, "Usage: sound_play <cue_name> \n");
        console_output(text, nullptr, false);
    }
}

// 0x007BB680 (:257)
void unused_dcf_sound_play_3d()
{
    char* text = Gameaudio_console_text;
    if (Console_command != 0)
    {
        // Magic numbers: argument type 8 (a cue name), 0x10 (a float).
        if (console_get_arg(8, 0))
        {
            // Original: the cue name is looked up through the integer argument (Console_arg_int as a char pointer).
            if (audiolib_cue_get_id((const char*)Console_arg_int, &dc_play_sound_3d_foley_info) == AUDIOLIB_ERR_NONE)
            {
                vector offset(0.0f, 0.0f, 0.0f);
                if (console_get_arg(0x10, 0))
                {
                    offset.x = Console_arg_float;
                }
                if (console_get_arg(0x10, 0))
                {
                    offset.y = Console_arg_float;
                }
                if (console_get_arg(0x10, 0))
                {
                    offset.z = Console_arg_float;
                }
                vector listener_pos;
                audiolib_get_listener_pos(&listener_pos, nullptr, nullptr);
                vector pos(listener_pos.x + offset.x, listener_pos.y + offset.y, listener_pos.z + offset.z);
                audiolib_cue_play_3d(dc_play_sound_3d_foley_info, &pos, nullptr, nullptr, nullptr);
            }
            sprintf(text, "\n");
            console_output(text, nullptr, false);
        }
        else
        {
            sprintf(text, "Invalid syntax");
        }
    }
    if (Console_help != 0)
    {
        sprintf(text, "Usage: sound_play_3d sound x y z\n");
        console_output(text, nullptr, false);
        sprintf(text, "     : x y z - OFFSET from listener!\n");
        console_output(text, nullptr, false);
    }
    console_output(text, nullptr, false);
}

// 0x007CB740 (:368)
// The wind: the "wind" engine sound runs while the wind blows, its RPM the wind speed (mph) and its speed parameter the
// listener LOS. A local player's car in the air adds its fall speed (the falling wind tweaks), up to the maximum wind.
void gameaudio_update_weather()
{
    float mph;
    float dir;
    weather_wind_get_speed(&mph, &dir);
    if (!audiolib_vehicle_engine_is_id_valid(wind_cue_id))
    {
        wind_cue_id = audiolib_vehicle_engine_get_id("wind");
    }
    player* pp = player_get_local_player();
    if (pp != nullptr && pp->vehicle_handle != 0xFFFFFFFF)
    {
        automobile* ap = (automobile*)World.object_from_handle(pp->vehicle_handle);
        if (ap != nullptr && ap->get_obj_type() == OT_VEHICLE && ap->get_sub_type() == OT_SUB_VEHICLE_AUTO && !automobile_any_wheel_on_ground(ap))
        {
            vector vel;
            vehicle_get_velocity(ap, vel);
            float increase = -Falling_wind_speed_increase_mult * vel.y;
            if (increase < 0.0f)
            {
                increase = 0.0f;
            }
            if (increase >= Falling_wind_speed_max_increase)
            {
                increase = Falling_wind_speed_max_increase;
            }
            mph = increase + mph;
            const float max_speed = weather_wind_get_max_speed();
            if (mph > max_speed)
            {
                mph = max_speed;
            }
        }
    }
    if (mph == 0.0f)
    {
        if (wind_play_id != -1)
        {
            audiolib_vehicle_engine_stop(wind_play_id);
            wind_play_id = -1;
        }
        return;
    }
    if (!audiolib_vehicle_engine_is_playing(wind_play_id))
    {
        wind_play_id = audiolib_vehicle_engine_start(wind_cue_id, nullptr, nullptr);
    }
    if (mph != wind_mph || wind_LOS != Current_listener_LOS)
    {
        audiolib_vehicle_engine_set_RPM(wind_play_id, Current_listener_LOS, mph, 0.0f, 0.0f);
        wind_mph = mph;
        wind_LOS = Current_listener_LOS;
    }
}

// 0x007BB870 (:482)
static void gameaudio_LOS_init()
{
    LOS_cardinal_direction = 0;
    for (int i = 0; i < NUM_AUDIOLIB_RAYCAST_DIRECTIONS; i++)
    {
        LOS_data[i].hit = false;
    }
}

// 0x007BB890 (:496)
// LTCG: effect_material in EAX; dir is unused and not passed.
// The ground materials by name, else by the material's default shape type (0 thick absorber, 1 and 4 thin reflectors,
// the rest clear). No material: a thin reflector.
static audiolib_material_type gameaudio_material_lookup(rfg_effect_material* effect_material, AUDIOLIB_RAYCAST_DIRECTION dir)
{
    if (effect_material == nullptr)
    {
        return AUDIOLIB_MATERIAL_TYPE_REFLECT_THIN;
    }
    if (v_stricmp(effect_material->name, "Terrain") == 0 || v_stricmp(effect_material->name, "Gravel") == 0 || v_stricmp(effect_material->name, "Ice") == 0 || v_stricmp(effect_material->name, "Sand") == 0 || v_stricmp(effect_material->name, "Stone") == 0)
    {
        return AUDIOLIB_MATERIAL_TYPE_GROUND;
    }
    // Magic numbers: material_shape_type values.
    const int shape_type = (int)effect_material->default_shape_type;
    if (shape_type == 0)
    {
        return AUDIOLIB_MATERIAL_TYPE_ABSORB_THICK;
    }
    if (shape_type != 1 && shape_type != 4)
    {
        return AUDIOLIB_MATERIAL_TYPE_CLEAR;
    }
    return AUDIOLIB_MATERIAL_TYPE_REFLECT_THIN;
}

// 0x007ED250 (:532)
// One of the 26 listener rays per call (round robin): a 50 m sphere cast (radius 0.1) from the listener against the
// world, keeping up to 4 objects (a hit within 0.4 m of the last one is the same surface, except that a clear material
// replaces it on the down rays). The listener LOS is the weight of the rays that hit within 20 m over the weight of all
// rays; audio_lib gets it as the listener FOV and the ray's objects for its reflections.
static void gameaudio_update_listener_los()
{
    raycast_input in;
    in.m_filter_info = COLLIDE_RAY_DEFAULT;
    const AUDIOLIB_RAYCAST_DIRECTION direction = LOS_cardinal_directions[LOS_cardinal_direction].raycast_dir;
    in.radius = 0.0f;
    in.filter_params = nullptr;
    in.filter_callback = nullptr;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 0;
    raycast_output out;
    LOS_data_struct& data = LOS_data[direction];
    data.hit = false;
    data.value = 0.0f;
    data.distance[0] = 0.0f;
    data.hit_object_handle[0] = 0xFFFFFFFF;
    data.num_objects = 0;
    data.pos[0].zero();
    vector listener_pos;
    if (audiolib_get_listener_pos(&listener_pos, nullptr, nullptr) == AUDIOLIB_ERR_NONE)
    {
        // Magic numbers: a 50 m ray of radius 0.1.
        const vector& dir = LOS_cardinal_directions[LOS_cardinal_direction].dir;
        in.start = listener_pos;
        in.end = vector(listener_pos.x + dir.x * 50.0f, listener_pos.y + dir.y * 50.0f, listener_pos.z + dir.z * 50.0f);
        in.m_hit_collector_type = raycast_input::COLLECT_ALL_HITS;
        in.m_filter_info = COLLIDE_RAY_WORLD_ONLY;
        in.filter_callback = nullptr;
        in.filter_params = nullptr;
        in.m_flags = 0;
        in.radius = 0.1f;
        if (havok_raycast(in, out))
        {
            const int num_hits = out.m_hits.getSize();
            for (int i = 0; i < 4; i++)
            {
                data.hit_object_handle[i] = 0xFFFFFFFF;
            }
            data.hit = false;
            data.value = 0.0f;
            int count = 0;
            unsigned int last_handle = 0xFFFFFFFF;
            float last_distance = 0.0f;
            for (int i = 0; i < num_hits && count < 4; i++)
            {
                const raycast_output::hit_info& hit = out.m_hits[i];
                const unsigned int handle = hit.hit_object_handle;
                if (handle == 0xFFFFFFFF || handle == last_handle)
                {
                    continue;
                }
                const audiolib_material_type material = gameaudio_material_lookup(rfg_get_effect_material_by_index(hit.hit_effect_material), direction);
                const float distance = hit.hit_fraction * 50.0f;
                last_handle = handle;
                // Magic number: hits within 0.4 m are the same surface.
                if (fabsf(distance - last_distance) <= 0.4f)
                {
                    if (count > 0 && (direction == AUDIOLIB_RAYCAST_DIRECTION_DOWN || direction >= AUDIOLIB_RAYCAST_DIRECTION_NORTH_DOWN) && material == AUDIOLIB_MATERIAL_TYPE_CLEAR)
                    {
                        data.hit_object_handle[count - 1] = handle;
                        data.distance[count - 1] = distance;
                        data.pos[count - 1] = hit.hit_pos;
                        data.material[count - 1] = AUDIOLIB_MATERIAL_TYPE_CLEAR;
                        last_distance = distance;
                    }
                }
                else
                {
                    data.hit_object_handle[count] = handle;
                    data.distance[count] = distance;
                    data.pos[count] = hit.hit_pos;
                    data.material[count] = material;
                    // Magic number: a hit within 20 m counts for the LOS.
                    if (!data.hit && distance < 20.0f)
                    {
                        data.hit = true;
                        data.value = LOS_cardinal_directions[LOS_cardinal_direction].value;
                    }
                    count++;
                    last_distance = distance;
                }
            }
            data.num_objects = count;
            for (int i = count; i < 4; i++)
            {
                data.distance[i] = 0.0f;
                data.hit_object_handle[i] = 0xFFFFFFFF;
                data.pos[i].zero();
            }
        }
    }
    float hit_weight = 0.0f;
    float total_weight = 0.0f;
    // Original: LOS_data is indexed by ray direction, the weights by table order.
    for (int i = 0; i < NUM_AUDIOLIB_RAYCAST_DIRECTIONS; i++)
    {
        if (LOS_data[i].hit)
        {
            hit_weight += LOS_data[i].value;
        }
        total_weight += LOS_cardinal_directions[i].value;
    }
    Current_listener_LOS = hit_weight / total_weight;
    audiolib_set_listener_fov(Current_listener_LOS);
    audiolib_listener_raycast(direction, data.num_objects, data.distance, data.pos, data.material);
    LOS_cardinal_direction = (LOS_cardinal_direction + 1) % NUM_AUDIOLIB_RAYCAST_DIRECTIONS;
}

// 0x007BB930 (:671)
// Registered on the Main Audio Thread; nothing to do in retail.
bool gameaudio_process(void* user_data, int curr_time, int time_step)
{
    return false;
}

// 0x007BB940 (:692)
event_result ga_global_event_callback(const event_data& event, notify_info* data)
{
    return EVENT_OK;
}

// 0x007BB950 (:702)
// Registers for no event in retail.
static void ga_register_for_global_events()
{
    gameaudio_gem_handle = nullptr;
}

// 0x007BB960 (:723)
void ga_unregister_for_global_events()
{
    if (gameaudio_gem_handle != nullptr)
    {
        gem_unregister_for_event(gameaudio_gem_handle);
    }
}

// 0x007BB980 (:740)
// GSP_WAVEBANK: hands the bank to audio_lib's game file loader, then releases the stream2 request (audio_lib copied
// what it keeps).
bool gameaudio_soundbank_loaded_callback(const char* name, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size)
{
    if (audiolib_loaded_callback_func != nullptr)
    {
        audiolib_loaded_callback_func(name, (void*)srid, cpu_data, cpu_data_size);
    }
    stream2_release(srid);
    return true;
}

// 0x007BB9B0 (:755)
void gameaudio_soundbank_unloaded_callback(const char* name, unsigned int srid)
{
    if (audiolib_unloaded_callback_func != nullptr)
    {
        audiolib_unloaded_callback_func(name, (void*)srid);
    }
}

// 0x007BB9C0 (:763)
// GSP_FOLEY_BANK: a persona's foley bank (personas_<lang>.vpp_pc).
bool gameaudio_foley_bank_loaded_callback(const char* name, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size)
{
    return audiolib_foley_bank_register(name, cpu_data, cpu_data_size) == AUDIOLIB_ERR_NONE;
}

// 0x007BB9E0 (:779)
void gameaudio_foley_bank_unloaded_callback(const char* name, unsigned int srid)
{
    audiolib_foley_bank_unregister(name);
}

// 0x007BB9F0 (:786)
// The vehicle wavebank primitive: the in-memory wavebank of that name (without extension) in the first load state of
// 3, 1, 0, 2, 4 that has one is marked a vehicle bank and registered with the data. Always true.
bool gameaudio_file_vehicle_wavebank_loaded_callback(const char* name, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size)
{
    // Original: the timer reads (load time statistics) are discarded.
    timer_get(TIMER_MILLISECONDS);
    char bank_name[64];
    strncpy(bank_name, name, sizeof(bank_name));
    char* ext = strstr(bank_name, ".");
    if (ext != nullptr)
    {
        *ext = '\0';
    }
    // Magic numbers: soundbank load states in search order.
    const int load_states[5] = {3, 1, 0, 2, 4};
    for (int i = 0; i < 5; i++)
    {
        // Magic number: bank type 1 (in-memory wavebank).
        const short id = audiolib_sound_bank_get_id_from_load_state(bank_name, 1, load_states[i]);
        if (id != -1)
        {
            audiolib_soundbank_data* bank = audiolib_internal_soundbank_id_to_ptr(id, false);
            if (bank != nullptr)
            {
                bank->isVehicleBank = true;
            }
            audiolib_sound_bank_register(id, (char*)cpu_data, cpu_data_size);
            timer_get(TIMER_MILLISECONDS);
            return true;
        }
    }
    return true;
}

// 0x007BBAE0 (:853)
void gameaudio_file_vehicle_wavebank_unloaded_callback(const char* name, unsigned int srid)
{
    // Original: the timer reads (unload time statistics) are discarded.
    timer_get(TIMER_MILLISECONDS);
    char bank_name[64];
    strncpy(bank_name, name, sizeof(bank_name));
    char* ext = strstr(bank_name, ".");
    if (ext != nullptr)
    {
        *ext = '\0';
    }
    // Magic number: bank type 1 (in-memory wavebank).
    const short bank_id = audiolib_sound_bank_get_id(bank_name, 1);
    if (bank_id != -1)
    {
        audiolib_sound_bank_unregister(bank_id, true);
        timer_get(TIMER_MILLISECONDS);
    }
}

// 0x007BBB70 (:887)
// audio_lib's game file loader: one GSC_AUDIO container with the file as a GSP_WAVEBANK primitive, requested at once.
// Returns the stream2 request id (0 on failure). Unused in retail (Gameaudio_streaming_file_initialize_result).
void* gameaudio_streaming_file_load(char* filepath, mempool* pool, void (*loaded_callback)(const char*, void*, unsigned char*, int), void (*unloaded_callback)(const char*, void*))
{
    char filename[128] = {};
    strcpy(filename, filepath);
    audiolib_loaded_callback_func = loaded_callback;
    audiolib_unloaded_callback_func = unloaded_callback;
    if (stream2_container_start(filename, pool, GSC_AUDIO, false, false))
    {
        if (stream2_container_add_prim(filename, "audio", GSP_WAVEBANK, GSA_NONE, true, 0, nullptr) == nullptr)
        {
            // Original: the started container is left unfinished.
            return nullptr;
        }
    }
    const unsigned int srid = stream2_container_finish(false, false);
    return stream2_request(srid) ? (void*)srid : nullptr;
}

// 0x007BBC60 (:917)
audiolib_result gameaudio_streaming_file_status(void* id)
{
    if (id == nullptr)
    {
        return AUDIOLIB_ERR_NONE;
    }
    switch (stream2_get_status((unsigned int)id))
    {
    case ST_UNLOADED:
        return AUDIOLIB_FILE_LOAD_STATUS_UNLOADED;
    case ST_LOADING:
        return AUDIOLIB_FILE_LOAD_STATUS_LOADING;
    case ST_LOADED:
        return AUDIOLIB_FILE_LOAD_STATUS_LOADED;
    case ST_ERROR:
        return AUDIOLIB_FILE_LOAD_STATUS_ERROR;
    default:
        return AUDIOLIB_ERR_NONE;
    }
}

// 0x007BBCC0 (:946)
// No caller in rfg.exe.
bool gameaudio_streaming_file_initialize(mempool* pool)
{
    Stream_audio_allocator.init(pool);
    return true;
}

// The cue id of a game_audio.xtbl string, if the entry has it.
static void gameaudio_parse_cue(xml_element* element, const char* name, int* cue_id, char* buf)
{
    // Magic number: the 256-byte name buffer.
    if (xml_table_get_opt_string(buf, 0x100, element, name))
    {
        audiolib_cue_get_id(buf, cue_id);
    }
}

// 0x007BBCE0 (:953)
// Original: the filename argument is only checked for null; the table read is always game_audio.xtbl.
bool gameaudio_parse_settings(const char* filename)
{
    if (filename == nullptr)
    {
        return false;
    }
    // Magic numbers: the low health defaults.
    low_health.Affect_sound_below_health = 0.65f;
    low_health.Affect_sound_above_health = 0.0f;
    low_health.Max_low_pass_due_to_health = 0.4f;
    low_health.Min_volume_due_to_health = 0.2f;
    low_health.Fade_rate = 0.05f;
    for (int i = 0; i < 10; i++)
    {
        low_health.cat_ids[i] = -1;
    }
    xml_element* root = xml_table_parse("game_audio.xtbl");
    if (root == nullptr)
    {
        return true;
    }
    char buf[256];
    float value;
    xml_element* entity = xml_table_find_opt(root, "NewEntity");
    if (entity != nullptr)
    {
        // Magic numbers: the clamps of the table values (percentages for the health levels and the volume).
        if (xml_table_get_opt_float(value, entity, "Clear_or_Radio_distance"))
        {
            gameaudio_clr_radio_crossover_distance = value;
            if (value < 1.0f)
            {
                gameaudio_clr_radio_crossover_distance = 1.0f;
            }
            if (gameaudio_clr_radio_crossover_distance >= 20.0f)
            {
                gameaudio_clr_radio_crossover_distance = 20.0f;
            }
        }
        xml_element* health = xml_table_find_opt(entity, "Low_Health_Settings");
        if (health != nullptr)
        {
            if (xml_table_get_opt_float(value, health, "Adjustment_Below_Health"))
            {
                low_health.Affect_sound_below_health = value / 100.0f;
                if (low_health.Affect_sound_below_health < 0.1f)
                {
                    low_health.Affect_sound_below_health = 0.1f;
                }
                if (low_health.Affect_sound_below_health >= 0.9f)
                {
                    low_health.Affect_sound_below_health = 0.9f;
                }
            }
            if (xml_table_get_opt_float(value, health, "Adjustment_Above_Health"))
            {
                low_health.Affect_sound_above_health = value / 100.0f;
                if (low_health.Affect_sound_above_health < 0.1f)
                {
                    low_health.Affect_sound_above_health = 0.1f;
                }
                if (low_health.Affect_sound_above_health >= 0.9f)
                {
                    low_health.Affect_sound_above_health = 0.9f;
                }
            }
            if (xml_table_get_opt_float(value, health, "Max_Low_Pass"))
            {
                low_health.Max_low_pass_due_to_health = value;
                if (value < 0.0f)
                {
                    low_health.Max_low_pass_due_to_health = 0.0f;
                }
                if (low_health.Max_low_pass_due_to_health >= 1.0f)
                {
                    low_health.Max_low_pass_due_to_health = 1.0f;
                }
            }
            if (xml_table_get_opt_float(value, health, "Min_Vol_Level"))
            {
                low_health.Min_volume_due_to_health = value / 100.0f;
                if (low_health.Min_volume_due_to_health < 0.1f)
                {
                    low_health.Min_volume_due_to_health = 0.1f;
                }
                if (low_health.Min_volume_due_to_health >= 1.0f)
                {
                    low_health.Min_volume_due_to_health = 1.0f;
                }
            }
            if (xml_table_get_opt_float(value, health, "Fade_Rate"))
            {
                if (value < 0.01f)
                {
                    value = 0.01f;
                }
                low_health.Fade_rate = value;
                if (value >= 100.0f)
                {
                    low_health.Fade_rate = 100.0f;
                }
            }
            // Original: no bound on cat_ids (10 entries) for the categories listed.
            xml_element* categories = xml_table_find_opt(health, "Categories");
            if (categories != nullptr)
            {
                short* category_id = low_health.cat_ids;
                for (xml_element* category = xml_find(categories, "Category"); category != nullptr; category = xml_find_next(categories, category, "Category"))
                {
                    audiolib_category_get_id(category->text, category_id);
                    audiolib_system_low_health_set_unaffected_category(*category_id);
                    category_id++;
                }
            }
        }
        xml_element* crossover = xml_table_find_opt(entity, "Crossover");
        if (crossover != nullptr)
        {
            xml_element* two_d_three_d = xml_find(crossover, "two_d_three_d");
            const unsigned int count = xml_count(two_d_three_d, "Entry");
            if (count != 0)
            {
                crossover_link_data.count = 0;
                crossover_link_data.data = (crossover_link_2d_3d*)Audio_mempool.alloc(count * sizeof(crossover_link_2d_3d), 0);
                if (crossover_link_data.data != nullptr)
                {
                    int n = 0;
                    for (xml_element* entry = xml_find(two_d_three_d, "Entry"); entry != nullptr; entry = xml_find_next(two_d_three_d, entry, "Entry"))
                    {
                        xml_element* two = xml_table_find_opt(entry, "Two");
                        xml_element* three = xml_table_find_opt(entry, "Three");
                        if (two != nullptr && three != nullptr && audiolib_cue_get_id(two->text, &crossover_link_data.data[n].twod) == AUDIOLIB_ERR_NONE && audiolib_cue_get_id(three->text, &crossover_link_data.data[n].threed) == AUDIOLIB_ERR_NONE)
                        {
                            n++;
                        }
                    }
                    // Original: the count is the number of entries, not of the pairs that resolved; the tail keeps
                    // whatever the pool held.
                    crossover_link_data.count = count;
                }
            }
        }
    }
    // Original: looked up even without a NewEntity (xml_table_find_opt takes a null root).
    xml_element* ui = xml_table_find_opt(entity, "UI_Sounds");
    if (ui != nullptr)
    {
        xml_element* e = xml_find(ui, "UI_Navigation");
        if (e != nullptr)
        {
            gameaudio_parse_cue(e, "Move_Down", &Ga_cue_ids.ui_sound_move_down, buf);
            gameaudio_parse_cue(e, "Move_Up", &Ga_cue_ids.ui_sound_move_up, buf);
            gameaudio_parse_cue(e, "Slider_Add", &Ga_cue_ids.ui_sound_slider_add, buf);
            gameaudio_parse_cue(e, "Slider_Subtract", &Ga_cue_ids.ui_sound_slider_subtract, buf);
            gameaudio_parse_cue(e, "Press_Select", &Ga_cue_ids.ui_sound_press_select, buf);
            gameaudio_parse_cue(e, "Press_Abort", &Ga_cue_ids.ui_sound_press_abort, buf);
            gameaudio_parse_cue(e, "Press_Misc", &Ga_cue_ids.ui_sound_press_misc, buf);
            gameaudio_parse_cue(e, "Scroll_Left", &Ga_cue_ids.ui_sound_scroll_left, buf);
            gameaudio_parse_cue(e, "Scroll_Right", &Ga_cue_ids.ui_sound_scroll_right, buf);
            gameaudio_parse_cue(e, "Bumper_Left", &Ga_cue_ids.ui_sound_bumper_left, buf);
            gameaudio_parse_cue(e, "Bumper_Right", &Ga_cue_ids.ui_sound_bumper_right, buf);
            gameaudio_parse_cue(e, "Invalid", &Ga_cue_ids.ui_sound_invalid, buf);
        }
        e = xml_find(ui, "Audio_Menu");
        if (e != nullptr)
        {
            gameaudio_parse_cue(e, "User_Ambience_Adjust", &Ga_cue_ids.ui_ambience_slider_cue_id, buf);
            gameaudio_parse_cue(e, "User_Music_Adjust", &Ga_cue_ids.ui_music_slider_cue_id, buf);
            gameaudio_parse_cue(e, "User_SFX_Adjust", &Ga_cue_ids.ui_sfx_slider_cue_id, buf);
            gameaudio_parse_cue(e, "User_Voice_Adjust", &Ga_cue_ids.ui_voice_slider_cue_id, buf);
            // Original: the voice chat slider reads the voice entry too.
            gameaudio_parse_cue(e, "User_Voice_Adjust", &Ga_cue_ids.ui_voicechat_slider_cue_id, buf);
            gameaudio_parse_cue(e, "User_Overall_Adjust", &Ga_cue_ids.ui_overall_slider_cue_id, buf);
        }
        e = xml_find(ui, "Jetpack");
        if (e != nullptr)
        {
            gameaudio_parse_cue(e, "Boost_Fail_PC", &Ga_cue_ids.jetpack_boost_fail_pc, buf);
            gameaudio_parse_cue(e, "Start_PC", &Ga_cue_ids.jetpack_start_pc, buf);
            gameaudio_parse_cue(e, "Loop_PC", &Ga_cue_ids.jetpack_loop_pc, buf);
            gameaudio_parse_cue(e, "Release_PC", &Ga_cue_ids.jetpack_release_pc, buf);
            gameaudio_parse_cue(e, "Boost_Fail_NPC", &Ga_cue_ids.jetpack_boost_fail_npc, buf);
            gameaudio_parse_cue(e, "Start_NPC", &Ga_cue_ids.jetpack_start_npc, buf);
            gameaudio_parse_cue(e, "Loop_NPC", &Ga_cue_ids.jetpack_loop_npc, buf);
            gameaudio_parse_cue(e, "Release_NPC", &Ga_cue_ids.jetpack_release_npc, buf);
            gameaudio_parse_cue(e, "Boost_Fail_Walker", &Ga_cue_ids.jetpack_boost_fail_walker, buf);
            gameaudio_parse_cue(e, "Start_Walker", &Ga_cue_ids.jetpack_start_walker, buf);
            gameaudio_parse_cue(e, "Loop_Walker", &Ga_cue_ids.jetpack_loop_walker, buf);
            gameaudio_parse_cue(e, "Release_Walker", &Ga_cue_ids.jetpack_release_walker, buf);
        }
        e = xml_find(ui, "Radio_Projected");
        if (e != nullptr)
        {
            gameaudio_parse_cue(e, "Radio_Start", &Ga_cue_ids.radio_message_start, buf);
            gameaudio_parse_cue(e, "Radio_Stop", &Ga_cue_ids.radio_message_end, buf);
            gameaudio_parse_cue(e, "Player_Radio_Start", &Ga_cue_ids.player_radio_start, buf);
            gameaudio_parse_cue(e, "Projected_Start", &Ga_cue_ids.edf_projected_start, buf);
        }
        e = xml_find(ui, "Misc");
        if (e != nullptr)
        {
            gameaudio_parse_cue(e, "Heartbeat", &Ga_cue_ids.heartbeat, buf);
            gameaudio_parse_cue(e, "Radiation_start", &Ga_cue_ids.radiation_start, buf);
            gameaudio_parse_cue(e, "Radiation_loop", &Ga_cue_ids.radiation_loop, buf);
            gameaudio_parse_cue(e, "Radiation_kill", &Ga_cue_ids.radiation_kill, buf);
            gameaudio_parse_cue(e, "Bftp_pickup_tag", &Ga_cue_ids.bftp_pickup_tag, buf);
            gameaudio_parse_cue(e, "Bftp_arm_bomb", &Ga_cue_ids.bftp_arm_bomb, buf);
            gameaudio_parse_cue(e, "District_liberated", &Ga_cue_ids.district_liberated, buf);
            gameaudio_parse_cue(e, "Liberation_countdown_loop", &Ga_cue_ids.district_liberation_countdown_loop, buf);
            gameaudio_parse_cue(e, "Liberation_countdown_done", &Ga_cue_ids.district_liberation_countdown_disappear, buf);
            gameaudio_parse_cue(e, "Unlock_popup", &Ga_cue_ids.unlock_popup, buf);
            gameaudio_parse_cue(e, "Unlock_slam", &Ga_cue_ids.unlock_slam, buf);
            gameaudio_parse_cue(e, "Target_destroyed", &Ga_cue_ids.target_destroyed, buf);
            gameaudio_parse_cue(e, "Rocket_lockon", &Ga_cue_ids.rocket_lockon, buf);
        }
        e = xml_find(ui, "Weapon_Cabinet");
        if (e != nullptr)
        {
            gameaudio_parse_cue(e, "Weapon_cabinet_scroll_up", &Ga_cue_ids.weapon_cabinet_scroll_up, buf);
            gameaudio_parse_cue(e, "Weapon_cabinet_scroll_down", &Ga_cue_ids.weapon_cabinet_scroll_down, buf);
            gameaudio_parse_cue(e, "Weapon_cabinet_open", &Ga_cue_ids.weapon_cabinet_open, buf);
            gameaudio_parse_cue(e, "Weapon_cabinet_close", &Ga_cue_ids.weapon_cabinet_close, buf);
        }
        e = xml_find(ui, "Weapon_Upgrade");
        if (e != nullptr)
        {
            gameaudio_parse_cue(e, "Weapon_upgrade_menu_open", &Ga_cue_ids.weapon_upgrade_menu_open, buf);
            gameaudio_parse_cue(e, "Weapon_upgrade_menu_closed", &Ga_cue_ids.weapon_upgrade_menu_close, buf);
            gameaudio_parse_cue(e, "Weapon_upgrade_purchase", &Ga_cue_ids.weapon_upgrade_purchase, buf);
            gameaudio_parse_cue(e, "Weapon_upgrade_no_purchase", &Ga_cue_ids.weapon_upgrade_no_purchase, buf);
        }
        e = xml_find(ui, "Timers");
        if (e != nullptr)
        {
            gameaudio_parse_cue(e, "Clock_appears", &Ga_cue_ids.timer_clock_appears, buf);
            gameaudio_parse_cue(e, "One_minute_remains", &Ga_cue_ids.timer_one_minute_remains, buf);
            gameaudio_parse_cue(e, "Ten_seconds_remains", &Ga_cue_ids.timer_ten_seconds_remains, buf);
            gameaudio_parse_cue(e, "Final_seconds_remain", &Ga_cue_ids.timer_final_seconds_remains, buf);
        }
        e = xml_find(ui, "Wrecking_crew");
        if (e != nullptr)
        {
            gameaudio_parse_cue(e, "Weapon_fired", &Ga_cue_ids.wrecking_crew_weapon_fired, buf);
            gameaudio_parse_cue(e, "Red_barrel_destroyed", &Ga_cue_ids.wrecking_crew_red_barrel_destroyed, buf);
            gameaudio_parse_cue(e, "Blue_barrel_destroyed", &Ga_cue_ids.wrecking_crew_blue_barrel_destroyed, buf);
            gameaudio_parse_cue(e, "Countdown_number", &Ga_cue_ids.wrecking_crew_countdown_number, buf);
            gameaudio_parse_cue(e, "Countdown_go", &Ga_cue_ids.wrecking_crew_countdown_go, buf);
            gameaudio_parse_cue(e, "Match_won", &Ga_cue_ids.wrecking_crew_match_won, buf);
            gameaudio_parse_cue(e, "Round_won_stamp", &Ga_cue_ids.wrecking_crew_round_won_stamp, buf);
            gameaudio_parse_cue(e, "Round_over", &Ga_cue_ids.wrecking_crew_round_over, buf);
        }
        e = xml_find(ui, "General_sounds");
        if (e != nullptr)
        {
            gameaudio_parse_cue(e, "Alert_changed", &Ga_cue_ids.general_alert_changed, buf);
            gameaudio_parse_cue(e, "Alert_changed_to_red", &Ga_cue_ids.general_alert_changed_to_red, buf);
            gameaudio_parse_cue(e, "Player_dies", &Ga_cue_ids.general_player_died, buf);
            gameaudio_parse_cue(e, "New_mission_pop_up", &Ga_cue_ids.general_new_mission_pop_up, buf);
            gameaudio_parse_cue(e, "Handbook_entry_pop_up", &Ga_cue_ids.general_handbook_pop_up, buf);
            gameaudio_parse_cue(e, "Tooltip_pop_ups", &Ga_cue_ids.general_tooltip_pop_up, buf);
            gameaudio_parse_cue(e, "Other_pop_ups", &Ga_cue_ids.general_other_pop_up, buf);
            gameaudio_parse_cue(e, "Pause_menu_open", &Ga_cue_ids.general_pause_menu_open, buf);
        }
        e = xml_find(ui, "Map_sounds");
        if (e != nullptr)
        {
            gameaudio_parse_cue(e, "Map_scroll", &Ga_cue_ids.map_scroll, buf);
            gameaudio_parse_cue(e, "Select_poi", &Ga_cue_ids.map_select_point_of_interest, buf);
            gameaudio_parse_cue(e, "Gps_placed", &Ga_cue_ids.map_gps_point_placed, buf);
            gameaudio_parse_cue(e, "Gps_removed", &Ga_cue_ids.map_gps_point_removed, buf);
            gameaudio_parse_cue(e, "Zoom_in", &Ga_cue_ids.map_zoom_in_attack, buf);
            gameaudio_parse_cue(e, "Zoom_out", &Ga_cue_ids.map_zoom_out_start, buf);
            gameaudio_parse_cue(e, "Map_open", &Ga_cue_ids.map_open, buf);
            gameaudio_parse_cue(e, "Map_close", &Ga_cue_ids.map_close, buf);
            gameaudio_parse_cue(e, "Activity_lock", &Ga_cue_ids.map_activity_lock, buf);
        }
        e = xml_find(ui, "Minimap_sounds");
        if (e != nullptr)
        {
            gameaudio_parse_cue(e, "Zoom_in", &Ga_cue_ids.minimap_zoom_in_attack, buf);
            gameaudio_parse_cue(e, "Zoom_out", &Ga_cue_ids.minimap_zoom_out_start, buf);
        }
        e = xml_find(ui, "Mission_sounds");
        if (e != nullptr)
        {
            gameaudio_parse_cue(e, "Mission_briefing_intro", &Ga_cue_ids.mission_briefing_intro, buf);
            gameaudio_parse_cue(e, "Mission_briefing_outro", &Ga_cue_ids.mission_briefing_outro, buf);
            gameaudio_parse_cue(e, "Mission_heading", &Ga_cue_ids.mission_heading, buf);
            gameaudio_parse_cue(e, "Mission_text", &Ga_cue_ids.mission_text, buf);
            gameaudio_parse_cue(e, "Mission_complete", &Ga_cue_ids.mission_complete, buf);
            gameaudio_parse_cue(e, "Mission_fail", &Ga_cue_ids.mission_fail, buf);
            gameaudio_parse_cue(e, "Mission_point_tally_alr", &Ga_cue_ids.mission_point_tally_alr, buf);
            gameaudio_parse_cue(e, "Mission_complete_text_hit", &Ga_cue_ids.mission_complete_text_hit, buf);
        }
        e = xml_find(ui, "Ants_vs_magnifying_glass_sounds");
        if (e != nullptr)
        {
            gameaudio_parse_cue(e, "AVMG_target_enemy", &Ga_cue_ids.avmg_target_enemy, buf);
            gameaudio_parse_cue(e, "AVMG_zoom_to_convoy", &Ga_cue_ids.avmg_zoom_to_convoy, buf);
            gameaudio_parse_cue(e, "AVMG_stop_zoom_to_convoy", &Ga_cue_ids.avmg_stop_zoom_to_convoy, buf);
        }
        e = xml_find(ui, "Activity_sounds");
        if (e != nullptr)
        {
            gameaudio_parse_cue(e, "Activity_complete", &Ga_cue_ids.activity_complete_sound, buf);
            gameaudio_parse_cue(e, "Activity_failed", &Ga_cue_ids.activity_fail_sound, buf);
        }
        e = xml_find(ui, "Popup_sounds");
        if (e != nullptr)
        {
            gameaudio_parse_cue(e, "Killing_spree", &Ga_cue_ids.popup_killing_spree, buf);
            gameaudio_parse_cue(e, "Propeganda", &Ga_cue_ids.popup_propeganda, buf);
            gameaudio_parse_cue(e, "Mayhem_started", &Ga_cue_ids.popup_mayhem_started, buf);
        }
        e = xml_find(ui, "Morale_sounds");
        if (e != nullptr)
        {
            gameaudio_parse_cue(e, "Moral_meter_appears", &Ga_cue_ids.morale_popup, buf);
            gameaudio_parse_cue(e, "Moral_meter_lowers", &Ga_cue_ids.morale_lower, buf);
            gameaudio_parse_cue(e, "Moral_meter_raises", &Ga_cue_ids.morale_raise, buf);
            gameaudio_parse_cue(e, "Control_meter_appears", &Ga_cue_ids.control_change, buf);
            gameaudio_parse_cue(e, "Control_meter_lowers", &Ga_cue_ids.control_lower, buf);
            gameaudio_parse_cue(e, "Control_meter_mission_unlocks", &Ga_cue_ids.control_mission_unlock, buf);
            gameaudio_parse_cue(e, "Control_meter_mission_complete", &Ga_cue_ids.control_mission_complete, buf);
        }
        e = xml_find(ui, "Multiplayer_sounds");
        if (e != nullptr)
        {
            gameaudio_parse_cue(e, "Select_Start", &Ga_cue_ids.multiplayer_select_start, buf);
            gameaudio_parse_cue(e, "Camera_switch", &Ga_cue_ids.multiplayer_camera_switch, buf);
            gameaudio_parse_cue(e, "Player_death", &Ga_cue_ids.multiplayer_player_death, buf);
            gameaudio_parse_cue(e, "Player_hits_player", &Ga_cue_ids.multiplayer_player_hits_player, buf);
            gameaudio_parse_cue(e, "Player_kills_player", &Ga_cue_ids.multiplayer_player_kills_player, buf);
            gameaudio_parse_cue(e, "Player_respawns", &Ga_cue_ids.multiplayer_player_respawn, buf);
            gameaudio_parse_cue(e, "Target_repair", &Ga_cue_ids.multiplayer_target_repair, buf);
            gameaudio_parse_cue(e, "Target_destroyed", &Ga_cue_ids.multiplayer_target_destroyed, buf);
            gameaudio_parse_cue(e, "Target_unlocked", &Ga_cue_ids.multiplayer_target_unlocked, buf);
            gameaudio_parse_cue(e, "Engineer_killed", &Ga_cue_ids.multiplayer_engineer_killed, buf);
            gameaudio_parse_cue(e, "Player_became_engineer", &Ga_cue_ids.multiplayer_become_engineer, buf);
            gameaudio_parse_cue(e, "Engineer_passed_to_other_player", &Ga_cue_ids.multiplayer_engineer_passed, buf);
            gameaudio_parse_cue(e, "Destruction_point_scored", &Ga_cue_ids.multiplayer_destruction_points, buf);
            gameaudio_parse_cue(e, "Flag_dropped", &Ga_cue_ids.multiplayer_flag_dropped, buf);
            gameaudio_parse_cue(e, "Flag_taken", &Ga_cue_ids.multiplayer_flag_taken, buf);
            gameaudio_parse_cue(e, "Flag_captured", &Ga_cue_ids.multiplayer_flag_captured, buf);
            gameaudio_parse_cue(e, "Flag_returned", &Ga_cue_ids.multiplayer_flag_returned, buf);
            gameaudio_parse_cue(e, "Bag_picked_up", &Ga_cue_ids.multiplayer_bag_picked_up, buf);
            gameaudio_parse_cue(e, "Bag_dropped", &Ga_cue_ids.multiplayer_bag_dropped, buf);
            gameaudio_parse_cue(e, "Bag_team_picked_up", &Ga_cue_ids.multiplayer_bag_team_picked_up, buf);
            gameaudio_parse_cue(e, "Bag_team_dropped", &Ga_cue_ids.multiplayer_bag_team_dropped, buf);
            gameaudio_parse_cue(e, "Xp_gain", &Ga_cue_ids.multiplayer_xp_gain, buf);
        }
    }
    xml_table_close(root);
    return true;
}

// 0x007BDB30 (:1527)
// The 2D cue of a 3D crossover cue (the cue itself if it has none).
int gameaudio_get_2d_crossover(int threed)
{
    for (int i = 0; i < crossover_link_data.count; i++)
    {
        if (crossover_link_data.data[i].threed == threed)
        {
            return crossover_link_data.data[i].twod;
        }
    }
    return threed;
}

// 0x007BDB70 (:1546)
int gameaudio_get_3d_crossover(int twod)
{
    for (int i = 0; i < crossover_link_data.count; i++)
    {
        if (crossover_link_data.data[i].twod == twod)
        {
            return crossover_link_data.data[i].threed;
        }
    }
    return twod;
}

// 0x007BDBA0 (:1570)
bool gameaudio_system_init_stage_1()
{
    if (Audio_stage_1_init)
    {
        return true;
    }
    audiolib_system_init_minimal();
    Audio_stage_1_init = true;
    return true;
}

// 0x007BDBC0 (:1581)
bool gameaudio_system_init_stage_2_complete()
{
    return Audio_stage_2_init;
}

// 0x007BDBD0 (:1588)
void gameaudio_get_system_init_params(audiolib_system_init_info& audiolib_init_info)
{
    audiolib_init_info.root_dir = nullptr;
    audiolib_init_info.table_dir = nullptr;
    audiolib_init_info.data_dir = nullptr;
    audiolib_init_info.settings_file = nullptr;
    audiolib_init_info.max_distance_to_keep_sounds = 0.0f;
    audiolib_init_info.pool = nullptr;
    audiolib_init_info.game_file_load_func = nullptr;
    audiolib_init_info.game_file_load_status_func = nullptr;
    audiolib_init_info.packfile_build = false;
    audiolib_init_info.voice_banks_memory_size = 0;
    audiolib_init_info.root_dir = Use_packfiles ? "" : "data\\";
    audiolib_init_info.table_dir = Use_packfiles ? "" : "tables\\";
    audiolib_init_info.data_dir = Use_packfiles ? "" : "audio\\pc\\";
    // Stored by audio_lib and never read (SFX.md 1.3).
    audiolib_init_info.settings_file = "rfg_global";
    audiolib_init_info.pool = &Audio_mempool;
    audiolib_init_info.packfile_build = Use_packfiles;
    // Magic numbers: the voice bank memory (bytes) and the distance past which sounds are dropped (m).
    audiolib_init_info.voice_banks_memory_size = 350000;
    audiolib_init_info.max_distance_to_keep_sounds = 510.0f;
    if (Gameaudio_streaming_file_initialize_result)
    {
        audiolib_init_info.game_file_load_func = gameaudio_streaming_file_load;
        audiolib_init_info.game_file_load_status_func = gameaudio_streaming_file_status;
    }
}

// 0x007E8E60 (:1608)
// audio_lib on the keen sound system with the bank and scratch allocators; on success the game side (the process
// function, the LOS rays, the music, game_audio.xtbl). A failed init turns the sound off (Sound), and the game runs
// silent.
bool gameaudio_system_init_stage_2(keen::MemoryAllocator* pAudioAllocator, keen::MemoryAllocator* pAudioBankAllocator, keen::SoundSystem::System* pSoundSystem)
{
    if (Audio_stage_2_init)
    {
        return true;
    }
    audiolib_set_sound_enabled(true);
    audiolib_system_init_info init_info;
    gameaudio_get_system_init_params(init_info);
    const audiolib_result result = audiolib_system_init(&init_info, pAudioAllocator, pAudioBankAllocator, pSoundSystem, localization_get_audio_language());
    if (result < 0)
    {
        Sound = false;
    }
    audiolib_set_sound_enabled(Sound);
    Sound = audiolib_get_sound_enabled();
    if (result >= 0)
    {
        if (!Sound)
        {
            return false;
        }
        audiolib_thread_function_register(gameaudio_process, "rfg_gameaudio_process", nullptr);
        gameaudio_LOS_init(); // inlined
        gamemusic_system_init(&Audio_mempool);
        ga_register_for_global_events(); // inlined
        gameaudio_parse_settings("game_audio.xtbl");
    }
    if (Sound)
    {
        Audio_stage_2_init = true;
    }
    return Sound;
}

// 0x007CB980 (:1654)
// No caller in rfg.exe: gamewide_shutdown inlines it (there it is called).
void gameaudio_system_shutdown()
{
    if (!Audio_stage_1_init)
    {
        return;
    }
    if (Audio_stage_2_init)
    {
        Sound = false;
        ga_unregister_for_global_events(); // inlined
        gamemusic_shutdown = true;
        audiolib_thread_function_unregister(gameaudio_process);
        audiolib_system_shutdown();
        return;
    }
    audiolib_system_shutdown_minimal();
}

// 0x007BDC80 (:1679)
// The low health filter: the health fraction eases up (Fade_rate of the gap per frame, gaps over 5%) and drops at once.
// Below Affect_sound_below_health for Gameaudio_low_health_delay ms, the low pass rises to Max_low_pass and the volume
// falls to Min_volume as the health goes from the below level to the above level.
void gameaudio_adjust_low_pass_and_volume_due_to_player_health()
{
    float low_pass = 0.0f;
    float volume = 1.0f;
    player* pp = player_get_local_player();
    float health_pct = pp == nullptr ? 1.0f : pp->hit_points / (float)pp->max_hit_points;
    // Magic number: a 5% gap.
    if (curr_health_pct < health_pct && health_pct - curr_health_pct > 0.05f)
    {
        health_pct = low_health.Fade_rate * (health_pct - curr_health_pct) + curr_health_pct;
    }
    curr_health_pct = health_pct;
    if (health_pct >= low_health.Affect_sound_below_health)
    {
        Gameaudio_low_health_timer.invalidate();
    }
    else if (!Gameaudio_low_health_timer.valid())
    {
        Gameaudio_low_health_timer.set(Gameaudio_low_health_delay);
    }
    else if (Gameaudio_low_health_timer.elapsed())
    {
        float t = curr_health_pct - low_health.Affect_sound_above_health;
        if (t < 0.0f)
        {
            t = 0.0f;
        }
        t = t / (low_health.Affect_sound_below_health - low_health.Affect_sound_above_health);
        low_pass = low_health.Max_low_pass_due_to_health - t * low_health.Max_low_pass_due_to_health;
        if (low_pass < 0.0f)
        {
            low_pass = 0.0f;
        }
        if (low_pass >= 1.0f)
        {
            low_pass = 1.0f;
        }
        const float v = (1.0f - low_health.Min_volume_due_to_health) * t + low_health.Min_volume_due_to_health;
        volume = low_health.Min_volume_due_to_health;
        if (v >= low_health.Min_volume_due_to_health)
        {
            volume = v;
        }
        if (volume >= 1.0f)
        {
            volume = 1.0f;
        }
    }
    audiolib_system_low_health_set_lowpass_adjustment(low_pass);
    audiolib_system_low_health_set_volume_adjustment(volume);
}

// 0x007ED7B0 (:1752)
// The listener (Volition y-up; audio_lib swaps it for keen), one LOS ray (two at over 25 fps), the wind and the low
// health filter.
bool gameaudio_update(const vector& listener_pos, const matrix& listener_orient, const vector& listener_vel)
{
    vector pos = listener_pos;
    matrix orient = listener_orient;
    vector vel = listener_vel;
    audiolib_set_listener_pos(&pos, &orient, &vel);
    gameaudio_update_listener_los();
    // Magic number: 0.04 s, 25 fps.
    if (Frametime < 0.04f)
    {
        gameaudio_update_listener_los();
    }
    gameaudio_update_weather();
    gameaudio_adjust_low_pass_and_volume_due_to_player_health();
    return true;
}

// 0x007ED860 (:1783)
// No caller in rfg.exe (gamestate_gp_simulate_frame does the same inline). The listener is the spectator camera, else
// the local player at ear height (in a vehicle 0.7 m, standing 1.575 m, prone 0.3 m, else 1.05 m).
void gameaudio_update_job(cpu_job* lj, unsigned int arg1, unsigned int arg2)
{
    havok_mark_for_read();
    if (Multiplayer && multi_is_dedicated_server())
    {
        havok_unmark_for_read();
        return;
    }
    if (multi_game_spectating())
    {
        vector vel(Frames_per_second * (Rfg_camera.real_pos.x - Rfg_camera.last_pos.x), (Rfg_camera.real_pos.y - Rfg_camera.last_pos.y) * Frames_per_second, Frames_per_second * (Rfg_camera.real_pos.z - Rfg_camera.last_pos.z));
        gameaudio_update(Rfg_camera.real_pos, Rfg_camera.real_orient, vel);
        havok_unmark_for_read();
        return;
    }
    player* pp = player_get_local_player();
    vector pos = pp->pos;
    vector vel(Frames_per_second * (pos.x - pp->last_pos.x), (pos.y - pp->last_pos.y) * Frames_per_second, (pos.z - pp->last_pos.z) * Frames_per_second);
    // Magic numbers: the ear heights (gamestate_gameplay.cpp has the same).
    if ((unsigned int)(pp->cur_state - HS_VEHICLE_SITTING) < 2)
    {
        pos.y += 0.7f;
    }
    else if (pp->cur_stance == HST_STAND)
    {
        pos.y += 1.5749999f;
    }
    else if (pp->cur_stance == HST_PRONE)
    {
        pos.y += 0.3f;
    }
    else
    {
        pos.y += 1.0500001f;
    }
    gameaudio_update(pos, Rfg_camera.real_orient, vel);
    havok_unmark_for_read();
}

// 0x007BDEC0 (:1852)
float gameaudio_get_listener_los()
{
    return Current_listener_LOS;
}

// 0x007CB9D0 (:1861)
void gameaudio_system_save(cfile* file)
{
    cf_write_uint32(Gamemusic_Progression_File, file);
}

// 0x007D8020 (:1867)
void gameaudio_system_load(cfile* file)
{
    const gamemusic_music_files progression_file = (gamemusic_music_files)cf_read_uint32(file);
    // Magic number: 6 progression files.
    if ((int)progression_file < 6 && progression_file != Gamemusic_Progression_File)
    {
        Gamemusic_Progression_File = progression_file;
        gamemusic_switch_music_files();
    }
}

// 0x007BDED0 (:1877)
// Empty in retail.
void gameaudio_mp_preload()
{
}

// 0x007BDEE0 (:1946)
// Empty in retail.
void gameaudio_mp_unload()
{
}

// 0x007BDEF0 (:2009)
// A cue on an object: the 2D cue when the object or one of its parents is the local player (unless the third person
// camera is locked), else the 3D crossover cue at the object.
int gameaudio_play_cross_linked_sound(int cue_id, unsigned int source_handle, const vector* orient, const vector* velocity)
{
    object* source = World.object_from_handle(source_handle);
    if (source_handle != 0xFFFFFFFF && !Camera_third_person_lock)
    {
        if (source == nullptr)
        {
            return -1;
        }
        for (object* objp = source; objp != nullptr; objp = World.object_from_handle(objp->get_parent()))
        {
            if (object_is_local_player(objp))
            {
                return audiolib_cue_play(gameaudio_get_2d_crossover(cue_id), nullptr, nullptr);
            }
        }
    }
    if (source == nullptr)
    {
        return -1;
    }
    return audiolib_cue_play_3d(gameaudio_get_3d_crossover(cue_id), &source->pos, orient, velocity, nullptr); // inlined
}

// 0x007BDFA0 (:2049)
// No caller in rfg.exe (gameseq_do_frame has the same inline).
void gameaudio_set_game_state(game_state curr_state)
{
    AUDIOLIB_GAME_STATE audio_game_state = AUDIOLIB_GAME_STATE_IN_LEVEL;
    switch (curr_state)
    {
    case GS_LOAD:
    case GS_BOOT:
    case GS_CREDITS:
        audio_game_state = AUDIOLIB_GAME_STATE_LOADING;
        break;
    case GS_MULTI_INIT:
    case GS_MULTI_SHUTDOWN:
        audio_game_state = AUDIOLIB_GAME_STATE_UNITIALIZED;
        break;
    default:
        break;
    }
    if (hud_ui_get_loaded_module() == HUD_MODULE_SATELLITE && curr_state != GS_INGAME_HANDBOOK_HUD)
    {
        audio_game_state = AUDIOLIB_GAME_STATE_SATELLITE;
    }
    audiolib_system_set_game_state(audio_game_state);
}

// 0x007BE030 (:2075)
// No caller in rfg.exe; empty.
void gameaudio_level_start()
{
}

// 0x007E8F50 (:2088)
// No caller in rfg.exe.
void gameaudio_level_end()
{
    gamemusic_update(false);
}
