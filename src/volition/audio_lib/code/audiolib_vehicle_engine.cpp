// Original: volition/audio_lib/code/audiolib_vehicle_engine.cpp
//
// Vehicle engine and wind loops (SFX.md SX2): foley_engine.xtbl gives each engine four onload and four offload RPM
// ranges (idle, RPM1-3; wind: light, medium, heavy) with volume and pitch ranges, envelopes and damage sounds. A
// running engine (Active_engine_list category 1) keeps one loop per range in range: its volume follows the RPM within
// the range, crosses over into the neighbouring ranges (Engine_RPM_lower/higher curves), and mixes onload and offload
// by the load (the "<name>_on"/"<name>_off" envelopes). Engine id: the lookup index + 1 (high half) and the soundbank id;
// instance id: the pool index (low half) and the slot's rolling counter. Stopping engines (category 2) wait for their
// loops to end, stopped ones (3) are freed. Compiled in audio_lib_unity.cpp.

#include "volition/audio_lib/code/audiolib_vehicle_engine.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_foley_category.h"
#include "volition/audio_lib/code/audiolib_internal.h"
#include "volition/audio_lib/code/audiolib_soundbank.h"
#include "volition/audio_lib/code/audiolib_thread.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/fastxml.h"

#include <stdlib.h>
#include <string.h>

// ---- public globals (rfg.pdb; no header declares them)
// The RPM crossover curves: the lower range fades out and the higher fades in over a range's overlap.
float audiolib_rolloff_curve_lower_range_crossover_entries[11][2] = { // 0x014F9488
    {0.0f, 1.0f}, {0.1f, 1.0f}, {0.2f, 0.9f}, {0.3f, 0.8f}, {0.4f, 0.7f}, {0.5f, 0.5f},
    {0.6f, 0.3f}, {0.7f, 0.2f}, {0.8f, 0.1f}, {0.9f, 0.0f}, {1.0f, 0.0f},
};
float audiolib_rolloff_curve_higher_range_crossover_entries[11][2] = { // 0x014F94E0
    {0.0f, 0.0f}, {0.1f, 0.1f}, {0.2f, 0.2f}, {0.3f, 0.3f}, {0.4f, 0.5f}, {0.5f, 0.7f},
    {0.6f, 0.8f}, {0.7f, 0.9f}, {0.8f, 1.0f}, {0.9f, 1.0f}, {1.0f, 1.0f},
};
int process_frame_count; // 0x01A7AEFC

// ---- file-static globals
// audiolib_play_state play_state_lookup[15] (0x014F9428) is used here too; placed in audiolib_playlist.cpp.
static timestamp Audiolib_vehicle_volume_delay; // 0x014F9D10, invalid (-1) from the constructor
// bool Audiolib_veh_engine_inited (0x01A789C4) is used here too; placed in audiolib.cpp.
// audiolib_veh_engine_lookup_data_list Audiolib_Engine_lookup_list (0x01A79AE0) is used here too; placed in audiolib.cpp.
static audiolib_rolloff_curve_data* Audiolib_veh_engine_rpm_lower; // 0x01A7AEF4
static audiolib_rolloff_curve_data* Audiolib_veh_engine_rpm_higher; // 0x01A7AEF8
// object_pool<audiolib_vehicle_engine_instance,30,4> Active_engine_list (0x01A9BA20) is used here too; placed in audiolib.cpp.

static bool audiolib_engine_play_id_is_set(int play_id)
{
    return Audiolib_state.initialized && play_id != 0 && play_id != -1;
}

// A play still in the play list (audiolib_internal_playstate_id_to_ptr under Audiolib_play_cs, inlined).
static bool audiolib_engine_play_exists(int play_id)
{
    Audiolib_play_cs.enter();
    const bool exists = audiolib_internal_playstate_id_to_ptr(play_id) != nullptr;
    Audiolib_play_cs.leave();
    return exists;
}

// The caller cue id of a cue name crc, -1 when unknown (audiolib_cue_get_id, inlined).
static void audiolib_engine_cue_id(unsigned int crc, int& cue_id)
{
    if (Audiolib_state.initialized)
    {
        cue_id = -1;
        const audiolib_foley_data* foley = audiolib_get_foley_data_pointer(audiolib_foley_hash_table_get(crc, nullptr));
        if (foley != nullptr)
        {
            cue_id = (int)((unsigned int)(foley - Audiolib_foley_list.foley) & 0xFFFF) | (int)((unsigned int)(uintptr_t)foley << 16);
        }
    }
}

// 0x00483010 (.h:189)
bool audiolib_vehicle_engine_instance::is_valid() const
{
    return this != nullptr;
}

// 0x00483020 (.h:190)
void audiolib_vehicle_engine_instance::invalidate()
{
    const int counter = rolling_counter;
    memset(this, 0, sizeof(*this));
    rolling_counter = counter;
}

// 0x00483050 (.h:197)
void audiolib_vehicle_engine_instance::drop()
{
    // Active_engine_list's release callback.
    const int counter = rolling_counter + 1;
    memset(this, 0, sizeof(*this));
    rolling_counter = counter;
    if (counter > 0xFFFF)
    {
        rolling_counter = 1;
    }
}

// 0x00483090 (.h:207)
audiolib_vehicle_engine_instance::audiolib_vehicle_engine_instance()
{
    const int counter = rolling_counter;
    memset(this, 0, sizeof(*this));
    rolling_counter = counter & 0xFFFF;
    if (rolling_counter == 0)
    {
        rolling_counter = 1;
    }
}

// 0x00492630 (:80), engine_id in EAX
static audiolib_vehicle_engine_instance* audiolib_internal_get_vehicle_engine_inst(int engine_id)
{
    audiolib_vehicle_engine_instance* engine = nullptr;
    Audiolib_engine_cs.enter();
    const unsigned int index = engine_id & 0xFFFF;
    if ((int)index < (int)Active_engine_list.get_capacity())
    {
        engine = Active_engine_list.get_entry((int)index);
        if (engine != nullptr && (engine->rolling_counter != engine_id >> 16 || engine->engine_data == nullptr))
        {
            engine = nullptr;
        }
    }
    Audiolib_engine_cs.leave();
    return engine;
}

// 0x0048A320 (:103)
bool audiolib_internal_engine_list_init(int total)
{
    Audiolib_engine_cs.enter();
    if (Audiolib_Engine_lookup_list.engine_list == nullptr)
    {
        Audiolib_Engine_lookup_list.count = 0;
        Audiolib_Engine_lookup_list.engine_list = (audiolib_vehicle_engine_data*)Audiolib_state.pool->alloc(total * sizeof(audiolib_vehicle_engine_data), 0);
        if (Audiolib_Engine_lookup_list.engine_list == nullptr)
        {
            Audiolib_engine_cs.leave();
            return false;
        }
        Audiolib_Engine_lookup_list.total = total;
        memset(Audiolib_Engine_lookup_list.engine_list, 0, total * sizeof(audiolib_vehicle_engine_data));
    }
    else if (Audiolib_Engine_lookup_list.total != total)
    {
        Audiolib_engine_cs.leave();
        return false;
    }
    Audiolib_engine_cs.leave();
    return true;
}

// 0x0048A3A0 (:128)
int audiolib_internal_find_engine_cue_id_by_name(const char* name)
{
    Audiolib_engine_cs.enter();
    if (name != nullptr && Audiolib_Engine_lookup_list.engine_list != nullptr)
    {
        for (int i = 0; i < Audiolib_Engine_lookup_list.count; ++i)
        {
            if (strcmp(Audiolib_Engine_lookup_list.engine_list[i].engine_name, name) == 0)
            {
                const int engine_id = (unsigned short)Audiolib_Engine_lookup_list.engine_list[i].soundbank_id | (i + 1) << 16;
                Audiolib_engine_cs.leave();
                return engine_id;
            }
        }
    }
    Audiolib_engine_cs.leave();
    return -1;
}

// 0x0048A470 (:146)
audiolib_vehicle_engine_data* audiolib_internal_find_engine_by_cue_id(int cue_id)
{
    Audiolib_engine_cs.enter();
    const int index = (cue_id >> 16) - 1;
    audiolib_vehicle_engine_data* engine = nullptr;
    if (index >= 0 && index < Audiolib_Engine_lookup_list.count && Audiolib_Engine_lookup_list.engine_list[index].soundbank_id == (short)cue_id)
    {
        engine = &Audiolib_Engine_lookup_list.engine_list[index];
    }
    Audiolib_engine_cs.leave();
    return engine;
}

// 0x0048A4D0 (:159)
static audiolib_vehicle_engine_data* audiolib_internal_get_free_engine_data()
{
    if (Audiolib_Engine_lookup_list.count >= Audiolib_Engine_lookup_list.total)
    {
        return nullptr;
    }
    return &Audiolib_Engine_lookup_list.engine_list[Audiolib_Engine_lookup_list.count++];
}

// The range index of a sound name: IDLE 0, RPM1-3; for the onload list also WEA(ther) LIGHT 0, MEDIUM 1, HEAVY 2.
// Original: an unknown name gives -1 and its range is written before the array.
static int audiolib_engine_range_index(const char* sound, bool wind)
{
    if (strstr(sound, "IDLE") != nullptr)
    {
        return 0;
    }
    if (strstr(sound, "RPM1") != nullptr)
    {
        return 1;
    }
    if (strstr(sound, "RPM2") != nullptr)
    {
        return 2;
    }
    if (strstr(sound, "RPM3") != nullptr)
    {
        return 3;
    }
    if (wind && strstr(sound, "WEA") != nullptr)
    {
        if (strstr(sound, "LIGHT") != nullptr)
        {
            return 0;
        }
        if (strstr(sound, "MEDIUM") != nullptr)
        {
            return 1;
        }
        if (strstr(sound, "HEAVY") != nullptr)
        {
            return 2;
        }
    }
    return -1;
}

// The volume curve: Linear 0, Bezier (and anything else) 1.
static unsigned char audiolib_engine_curve_type(const char* curve)
{
    unsigned char type = 1;
    if (curve != nullptr)
    {
        if (strcmp(curve, "Linear") == 0)
        {
            type = 0;
        }
        else if (strcmp(curve, "Bezier") == 0)
        {
            type = 1;
        }
    }
    return type;
}

// An envelope "x,y" point list (1-15 points) becomes the curve "<engine><suffix>", closed with (1, 0 or 1).
static void audiolib_engine_parse_envelope(xml_element* engine, const char* element_name, const audiolib_vehicle_engine_data* data, const char* suffix)
{
    xml_element* envelope = xml_find(engine, element_name);
    if (envelope == nullptr || (unsigned int)xml_count(envelope, "point") - 1 >= 15)
    {
        return;
    }
    float points[32][2];
    int count = 0;
    for (xml_element* point = xml_find(envelope, "point"); point != nullptr; point = xml_find_next(envelope, point, "point"))
    {
        points[count][0] = (float)atof(point->text);
        points[count][1] = (float)atof(strchr(point->text, ',') + 1);
        ++count;
    }
    if (count > 0)
    {
        points[count][0] = 1.0f;
        points[count][1] = 0.5f < points[count - 1][1] ? 1.0f : 0.0f;
        char name[64];
        strcpy(name, data->engine_name);
        strcat(name, suffix);
        audiolib_create_rolloff_curve(3, name, count + 1, &points[0][0]);
    }
}

// 0x00497320 (:176)
static audiolib_result audiolib_vehicle_engine_parse_xml_list(xml_element* engine)
{
    Audiolib_engine_cs.enter();
    const char* name = xml_table_get_req_string_ref(engine, "Name");
    if (name == nullptr)
    {
        Audiolib_engine_cs.leave();
        return AUDIOLIB_ERR_FILE_FORMAT;
    }
    // A name already in the list is replaced.
    audiolib_vehicle_engine_data* data = audiolib_internal_find_engine_by_cue_id(audiolib_internal_find_engine_cue_id_by_name(name));
    if (data == nullptr)
    {
        data = audiolib_internal_get_free_engine_data();
        if (data == nullptr)
        {
            Audiolib_engine_cs.leave();
            return AUDIOLIB_ERR_FILE_FORMAT;
        }
    }
    strncpy(data->engine_name, name, sizeof(data->engine_name));
    const char* bank = xml_table_get_opt_string_ref(engine, "SndBnk");
    if (bank == nullptr)
    {
        data->soundbank_name[0] = '\0';
    }
    else
    {
        strncpy(data->soundbank_name, bank, sizeof(data->soundbank_name));
    }
    data->soundbank_id = audiolib_sound_bank_get_id(crc_str(data->soundbank_name, 0), 0);
    bank = xml_table_get_opt_string_ref(engine, "WvBnk");
    if (bank == nullptr)
    {
        data->wavebank_name[0] = '\0';
    }
    else
    {
        strncpy(data->wavebank_name, bank, sizeof(data->wavebank_name));
    }
    data->wavebank_id = audiolib_sound_bank_get_id(crc_str(data->wavebank_name, 0), 1);
    // Engine (idle at 750 RPM) or Wind (10); another type leaves the playback type as it was.
    const char* playback = xml_table_get_opt_string_ref(engine, "Playback_type");
    if (playback == nullptr)
    {
        data->playback_type = AUDIOLIB_VEHICLE_ENGINE_PLAYBACK_TYPE_ENGINE;
    }
    else if (strcmp(playback, "Engine") == 0)
    {
        data->idle_rpm = 750.0f;
        data->playback_type = AUDIOLIB_VEHICLE_ENGINE_PLAYBACK_TYPE_ENGINE;
    }
    else if (strcmp(playback, "Wind") == 0)
    {
        data->playback_type = AUDIOLIB_VEHICLE_ENGINE_PLAYBACK_TYPE_WIND;
        data->idle_rpm = 10.0f;
    }
    for (int i = 0; i < 4; ++i)
    {
        data->onload[i].cue_id = -1;
        data->onload[i].mono_cue_id = -1;
        data->offload[i].cue_id = -1;
        data->offload[i].mono_cue_id = -1;
    }
    data->max_rpm = 0.0f;
    xml_element* list = xml_find(engine, "RPM_ONLOAD_range_list");
    if (list != nullptr)
    {
        for (xml_element* entry = xml_find(list, "Entry"); entry != nullptr; entry = xml_find_next(list, entry, "Entry"))
        {
            int index = -1;
            int cue_id = -1;
            int mono_cue_id = -1;
            float min_rpm = 0.0f;
            float vol_min = 1.0f;
            float pitch_min = 0.0f;
            float max_rpm = 0.0f;
            float vol_max = 1.0f;
            float pitch_max = 0.0f;
            const char* sound = xml_table_get_req_string_ref(entry, "Sound");
            if (sound != nullptr)
            {
                audiolib_cue_get_id(crc_str(sound, 0), &cue_id);
                index = audiolib_engine_range_index(sound, true);
            }
            const char* mono = xml_table_get_opt_string_ref(entry, "Mono_Sound");
            if (mono != nullptr)
            {
                audiolib_cue_get_id(crc_str(mono, 0), &mono_cue_id);
            }
            // Wind ranges have no volume or pitch.
            xml_element* minimums = xml_find(entry, "Range_minimums");
            if (minimums != nullptr)
            {
                xml_table_get_req_float(min_rpm, minimums, "RPM");
                if (data->playback_type != AUDIOLIB_VEHICLE_ENGINE_PLAYBACK_TYPE_WIND)
                {
                    xml_table_get_req_float(vol_min, minimums, "Volume");
                    xml_table_get_req_float(pitch_min, minimums, "Pitch");
                }
            }
            xml_element* maximums = xml_find(entry, "Range_maximums");
            if (maximums != nullptr)
            {
                xml_table_get_req_float(max_rpm, maximums, "RPM");
                if (data->max_rpm < max_rpm)
                {
                    data->max_rpm = max_rpm;
                }
                if (data->playback_type != AUDIOLIB_VEHICLE_ENGINE_PLAYBACK_TYPE_WIND)
                {
                    xml_table_get_req_float(vol_max, maximums, "Volume");
                    xml_table_get_req_float(pitch_max, maximums, "Pitch");
                }
            }
            audiolib_vehicle_engine_rpm_data& range = data->onload[index];
            range.parsed_set = true;
            range.cue_id = cue_id;
            range.min_rpm = min_rpm;
            range.vol_min = vol_min;
            range.pitch_min = pitch_min;
            range.max_rpm = max_rpm;
            range.mono_cue_id = mono_cue_id;
            range.vol_max = vol_max;
            range.pitch_max = pitch_max;
            range.curve_type = audiolib_engine_curve_type(xml_table_get_req_string_ref(entry, "Volume_curve"));
        }
    }
    audiolib_engine_parse_envelope(engine, "Onload_Envelope", data, "_on");
    list = xml_find(engine, "RPM_OFFLOAD_range_list");
    if (list != nullptr)
    {
        for (xml_element* entry = xml_find(list, "Entry"); entry != nullptr; entry = xml_find_next(list, entry, "Entry"))
        {
            int index = -1;
            int cue_id = -1;
            int mono_cue_id = -1;
            float min_rpm = 0.0f;
            float vol_min = 1.0f;
            float pitch_min = 0.0f;
            float max_rpm = 0.0f;
            float vol_max = 1.0f;
            float pitch_max = 0.0f;
            const char* sound = xml_table_get_req_string_ref(entry, "Sound");
            if (sound != nullptr)
            {
                audiolib_engine_cue_id(crc_str(sound, 0), cue_id);
                // Original: the WEA(ther) names are not mapped in the offload list.
                index = audiolib_engine_range_index(sound, false);
            }
            const char* mono = xml_table_get_opt_string_ref(entry, "Mono_Sound");
            if (mono != nullptr)
            {
                audiolib_engine_cue_id(crc_str(mono, 0), mono_cue_id);
            }
            xml_element* minimums = xml_find(entry, "Range_minimums");
            if (minimums != nullptr)
            {
                xml_table_get_req_float(min_rpm, minimums, "RPM");
                xml_table_get_req_float(vol_min, minimums, "Volume");
                xml_table_get_req_float(pitch_min, minimums, "Pitch");
            }
            xml_element* maximums = xml_find(entry, "Range_maximums");
            if (maximums != nullptr)
            {
                xml_table_get_req_float(max_rpm, maximums, "RPM");
                if (data->max_rpm < max_rpm)
                {
                    data->max_rpm = max_rpm;
                }
                xml_table_get_req_float(vol_max, maximums, "Volume");
                xml_table_get_req_float(pitch_max, maximums, "Pitch");
            }
            audiolib_vehicle_engine_rpm_data& range = data->offload[index];
            range.parsed_set = true;
            range.cue_id = cue_id;
            range.min_rpm = min_rpm;
            range.vol_min = vol_min;
            range.pitch_min = pitch_min;
            range.max_rpm = max_rpm;
            range.mono_cue_id = mono_cue_id;
            range.vol_max = vol_max;
            range.pitch_max = pitch_max;
            range.curve_type = audiolib_engine_curve_type(xml_table_get_req_string_ref(entry, "Volume_curve"));
        }
    }
    audiolib_engine_parse_envelope(engine, "Offload_Envelope", data, "_off");
    xml_element* damage = xml_find(engine, "Damage_Sounds");
    if (damage != nullptr)
    {
        const char* foley = xml_table_get_req_string_ref(damage, "Foley");
        if (foley != nullptr)
        {
            audiolib_engine_cue_id(crc_str(foley, 0), data->damage_cue_id);
        }
        foley = xml_table_get_opt_string_ref(damage, "Mono_Foley");
        if (foley != nullptr)
        {
            audiolib_engine_cue_id(crc_str(foley, 0), data->damage_mono_cue_id);
        }
        float scale;
        if (xml_table_get_opt_float(scale, damage, "Volume_Scalar"))
        {
            data->damage_vol_scale = scale;
        }
    }
    // The idle loop starts 250 ms late by default.
    data->idle_start_delay_ms = 250;
    const char* offset = xml_table_get_opt_string_ref(engine, "Idle_time_start_offset");
    if (offset != nullptr)
    {
        data->idle_start_delay_ms = atoi(offset);
    }
    Audiolib_engine_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00498080 (:609)
audiolib_result audiolib_vehicle_engine_parse_xtbl(const char* filename)
{
    if (filename == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    audiolib_result result = AUDIOLIB_ERR_NONE;
    char path[256];
    strncpy(path, Audiolib_state.root_dir, sizeof(path));
    strncat(path, Audiolib_state.table_dir, sizeof(path));
    strncat(path, filename, sizeof(path));
    xml_element* root = xml_table_parse(path);
    int total = 0;
    if (root != nullptr)
    {
        total = xml_table_count(root, "Engine");
        if (total > 0)
        {
            if (audiolib_internal_engine_list_init(total))
            {
                for (xml_element* engine = xml_find(root, "Engine"); engine != nullptr; engine = xml_find_next(root, engine, "Engine"))
                {
                    result = audiolib_vehicle_engine_parse_xml_list(engine);
                    if (result < 0)
                    {
                        break;
                    }
                }
            }
            else
            {
                result = AUDIOLIB_ERR_FILE_FORMAT;
            }
        }
        xml_table_close(root);
        if (result < 0)
        {
            total = 0;
        }
    }
    Audiolib_state.number_of_engines = total;
    return result;
}

// 0x0048A500 (:720)
audiolib_result audiolib_vehicle_volume_delay(unsigned int time_ms)
{
    // The engines stay silent for a while (a vehicle entered).
    Audiolib_vehicle_volume_delay.set((int)time_ms);
    return AUDIOLIB_ERR_NONE;
}

// A clip's play follows the engine: a 2D switch (and full volume), a 3D switch and the position (the update block
// repeated for every clip in rfg.exe).
static void audiolib_engine_update_clip_play(audiolib_vehicle_engine_instance* engine, int play_id)
{
    if (engine->switch_to_2d && audiolib_engine_play_id_is_set(play_id))
    {
        // audiolib_internal_playlist_change_to_2d(play_id, 1), inlined.
        Audiolib_play_cs.enter();
        audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
        if (play != nullptr)
        {
            play->action |= 0x800;
            play->new_vol = 1.0f;
        }
        Audiolib_play_cs.leave();
    }
    if (engine->switch_to_3d && audiolib_engine_play_id_is_set(play_id))
    {
        audiolib_internal_playlist_change_to_3d(play_id, &engine->curr_pos, &engine->curr_orient, nullptr);
    }
    if (engine->is_3d && audiolib_engine_play_id_is_set(play_id))
    {
        audiolib_internal_playlist_set_3d_position(play_id, &engine->curr_pos, &engine->curr_orient, nullptr);
    }
}

// The play params of an engine loop: 200 ms fade in, 3D (distance-kept engine loop, 0x841), 2D engine (0x810, no low
// pass) or wind (0).
static void audiolib_engine_clip_params(audiolib_params& params, const audiolib_vehicle_engine_instance* engine, float volume, float pitch)
{
    params.fade_ms = 200;
    if (engine->is_3d)
    {
        params.flags = 0x841;
    }
    else if (engine->engine_data->playback_type == AUDIOLIB_VEHICLE_ENGINE_PLAYBACK_TYPE_WIND)
    {
        params.flags = 0;
    }
    else
    {
        params.flags = 0x810;
    }
    params.pitch = pitch;
    params.pos = engine->curr_pos;
    params.orient = engine->curr_orient;
    params.volume = volume;
    params.delay_ms = 0;
}

// A range's cue volume: the cue's own volume for the mono cue in 3D, else for the stereo cue (none: unscaled).
static float audiolib_engine_cue_volume(const audiolib_vehicle_engine_instance* engine, const audiolib_vehicle_engine_rpm_data& range, float volume)
{
    int cue_id = -1;
    if (engine->is_3d && range.mono_cue_id != -1)
    {
        cue_id = range.mono_cue_id;
    }
    else if (range.cue_id != -1)
    {
        cue_id = range.cue_id;
    }
    else
    {
        return volume;
    }
    const audiolib_foley_data* foley = audiolib_get_foley_data_pointer(cue_id);
    const float cue_volume = foley != nullptr ? (float)foley->initial_volume / 100.0f : 0.0f;
    return volume * cue_volume;
}

static float audiolib_engine_clamp(float value, float low, float high)
{
    if (value - low < 0.0f)
    {
        value = low;
    }
    if (0.0f <= value - high)
    {
        value = high;
    }
    return value;
}

// The crossover of a range with its neighbours: fading out into the next range (lower curve), fading in from the
// previous one (higher curve, which wins). Ranges with a curve type of 2 or more do not cross over.
static float audiolib_engine_crossover(const audiolib_vehicle_engine_rpm_data* ranges, int index, float rpm)
{
    float scale = 1.0f;
    const audiolib_vehicle_engine_rpm_data& range = ranges[index];
    if (index < 3 && ranges[index + 1].parsed_set && ranges[index + 1].min_rpm <= rpm && range.curve_type < 2)
    {
        const float next_min = ranges[index + 1].min_rpm;
        const float fraction = (rpm - next_min) / (range.max_rpm - next_min);
        if (Audiolib_veh_engine_rpm_lower == nullptr)
        {
            Audiolib_veh_engine_rpm_lower = audiolib_rolloff_curve_find_appropriate_curve(3, "Engine_RPM_lower");
        }
        scale = audiolib_calc_scaled_value(3, Audiolib_veh_engine_rpm_lower, 0.0f, 1.0f, 1.0f, 0.0f, fraction);
    }
    // Original: the previous range of the first onload range is read from before the array (index 0 is skipped).
    if (index > 0 && ranges[index - 1].parsed_set && rpm < ranges[index - 1].max_rpm && ranges[index - 1].curve_type < 2)
    {
        const float fraction = (rpm - range.min_rpm) / (ranges[index - 1].max_rpm - range.min_rpm);
        if (Audiolib_veh_engine_rpm_higher == nullptr)
        {
            Audiolib_veh_engine_rpm_higher = audiolib_rolloff_curve_find_appropriate_curve(3, "Engine_RPM_higher");
        }
        scale = audiolib_calc_scaled_value(3, Audiolib_veh_engine_rpm_higher, 0.0f, 1.0f, 0.0f, 1.0f, fraction);
    }
    return scale;
}

// The envelope curve of a load ("<engine>_on" or "_off"; the first curve when missing).
static audiolib_rolloff_curve_data* audiolib_engine_envelope(const audiolib_vehicle_engine_data* data, const char* suffix)
{
    char name[64];
    strcpy(name, data->engine_name);
    strcat(name, suffix);
    return audiolib_rolloff_curve_find_appropriate_curve(3, name);
}

// A range's loop: stopped out of range or silent, started (after the start wait) or updated in range.
static void audiolib_engine_update_clip(audiolib_vehicle_engine_instance* engine, audiolib_veh_eng_clip_data& clip, const audiolib_vehicle_engine_rpm_data& range,
                                        int index, bool set_low_pass)
{
    const float volume = clip.vol;
    if (volume <= 0.0f)
    {
        // Silent: the play stays at volume 0.
        const int play_id = clip.play_id;
        if (audiolib_engine_play_id_is_set(play_id) && audiolib_engine_play_exists(play_id))
        {
            if (audiolib_engine_play_id_is_set(clip.play_id))
            {
                audiolib_internal_playlist_set_volume(clip.play_id, 0.0f, 100);
            }
            audiolib_engine_update_clip_play(engine, clip.play_id);
        }
        return;
    }
    audiolib_params params;
    audiolib_engine_clip_params(params, engine, volume, clip.pitch);
    if (set_low_pass && !engine->is_3d)
    {
        params.dsp_type = 1;
        params.dsp.low_pass.effect = clip.lowpass;
    }
    const int play_id = clip.play_id;
    if (audiolib_engine_play_id_is_set(play_id) && audiolib_engine_play_exists(play_id))
    {
        if (set_low_pass)
        {
            // Wind: the low pass (audiolib_internal_playlist_set_low_pass, inlined).
            const float low_pass = clip.lowpass;
            if (audiolib_engine_play_id_is_set(clip.play_id))
            {
                Audiolib_play_cs.enter();
                audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(clip.play_id);
                if (play != nullptr)
                {
                    play->action |= 0x1000;
                    play->new_low_pass = low_pass;
                }
                Audiolib_play_cs.leave();
            }
        }
        else if (index != 0 && audiolib_engine_play_id_is_set(clip.play_id))
        {
            audiolib_internal_playlist_set_pitch(clip.play_id, clip.pitch);
        }
        if (audiolib_engine_play_id_is_set(clip.play_id))
        {
            audiolib_internal_playlist_set_volume(clip.play_id, clip.vol, 200);
        }
        audiolib_engine_update_clip_play(engine, clip.play_id);
        return;
    }
    if (engine->wait_to_start < 1)
    {
        int cue_id = -1;
        if (engine->is_3d && range.mono_cue_id != -1)
        {
            cue_id = range.mono_cue_id;
        }
        else
        {
            cue_id = range.cue_id;
        }
        if (cue_id != -1)
        {
            clip.play_id = audiolib_cue_play(cue_id, &params, nullptr);
        }
    }
}

// Every loop of the engine ends (0 ms fades).
static void audiolib_engine_silence(audiolib_vehicle_engine_instance* engine)
{
    for (int i = 0; i < 4; ++i)
    {
        if (engine->offload[i].play_id != -1)
        {
            if (audiolib_engine_play_id_is_set(engine->offload[i].play_id))
            {
                audiolib_internal_playlist_fade_out(engine->offload[i].play_id, 0);
            }
            engine->offload[i].play_id = -1;
        }
        if (engine->onload[i].play_id != -1)
        {
            if (audiolib_engine_play_id_is_set(engine->onload[i].play_id))
            {
                audiolib_internal_playlist_fade_out(engine->onload[i].play_id, 0);
            }
            engine->onload[i].play_id = -1;
        }
    }
}

// A stopping engine's play: forgotten when gone, otherwise faded out (0 ms) unless stopped. True while it exists.
static bool audiolib_engine_stop_play(int& play_id)
{
    if (!audiolib_engine_play_id_is_set(play_id) || !audiolib_engine_play_exists(play_id))
    {
        play_id = -1;
        return false;
    }
    if (audiolib_engine_play_id_is_set(play_id))
    {
        Audiolib_play_cs.enter();
        audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
        if (play == nullptr)
        {
            Audiolib_play_cs.leave();
        }
        else
        {
            const audiolib_play_state state = play_state_lookup[play->play_state];
            Audiolib_play_cs.leave();
            if (state != AUDIOLIB_PLAY_STATE_STOPPED && audiolib_engine_play_id_is_set(play_id))
            {
                audiolib_internal_playlist_fade_out(play_id, 0);
            }
        }
    }
    return true;
}

// The running engine's ranges of one load.
static void audiolib_engine_process_running(audiolib_vehicle_engine_instance* engine)
{
    audiolib_vehicle_engine_data* data = engine->engine_data;
    float onload_scale;
    float offload_scale;
    if (data->playback_type == AUDIOLIB_VEHICLE_ENGINE_PLAYBACK_TYPE_WIND)
    {
        onload_scale = 1.0f;
        offload_scale = 0.0f;
    }
    else
    {
        // The load eases in (12 % a pass); the envelopes mix the onload and offload loops.
        engine->curr_onload = (engine->dest_onload - engine->curr_onload) * 0.12f + engine->curr_onload;
        if (engine->onload_curve_data == nullptr)
        {
            engine->onload_curve_data = audiolib_engine_envelope(data, "_on");
        }
        onload_scale = audiolib_calc_scaled_value(3, engine->onload_curve_data, 0.0f, 1.0f, 0.0f, 1.0f, engine->curr_onload);
        if (engine->offload_curve_data == nullptr)
        {
            engine->offload_curve_data = audiolib_engine_envelope(data, "_off");
        }
        offload_scale = audiolib_calc_scaled_value(3, engine->offload_curve_data, 0.0f, 1.0f, 1.0f, 0.0f, engine->curr_onload);
    }
    // The damage loop follows the damage (after the start wait).
    if (engine->engine_data->playback_type != AUDIOLIB_VEHICLE_ENGINE_PLAYBACK_TYPE_WIND && engine->wait_to_start < 1)
    {
        if (engine->curr_damage != engine->dest_damage)
        {
            if (engine->curr_damage == 0.0f && 0.0f < engine->dest_damage && engine->damage_loop_play_id == -1)
            {
                audiolib_params params;
                const audiolib_vehicle_engine_data* engine_data = engine->engine_data;
                int cue_id = engine_data->damage_cue_id;
                params.volume = engine_data->damage_vol_scale * engine->dest_damage;
                params.fade_ms = 200;
                if (engine->is_3d)
                {
                    params.pos = engine->curr_pos;
                    if (engine_data->damage_mono_cue_id != -1)
                    {
                        cue_id = engine_data->damage_mono_cue_id;
                    }
                    params.flags = 0x841;
                    params.orient = engine->curr_orient;
                }
                else
                {
                    params.flags = 0x810;
                }
                engine->damage_loop_play_id = audiolib_cue_play(cue_id, &params, nullptr);
            }
            if (engine->dest_damage == 0.0f)
            {
                if (audiolib_engine_play_id_is_set(engine->damage_loop_play_id))
                {
                    audiolib_internal_playlist_fade_out(engine->damage_loop_play_id, 200);
                }
                engine->damage_loop_play_id = -1;
            }
        }
        if (engine->damage_loop_play_id != -1)
        {
            if (engine->switch_to_3d && audiolib_engine_play_id_is_set(engine->damage_loop_play_id))
            {
                audiolib_internal_playlist_change_to_3d(engine->damage_loop_play_id, &engine->curr_pos, &engine->curr_orient, nullptr);
            }
            if (engine->is_3d && audiolib_engine_play_id_is_set(engine->damage_loop_play_id))
            {
                audiolib_internal_playlist_set_3d_position(engine->damage_loop_play_id, &engine->curr_pos, &engine->curr_orient, nullptr);
            }
            if (engine->switch_to_2d && audiolib_engine_play_id_is_set(engine->damage_loop_play_id))
            {
                audiolib_internal_playlist_change_to_2d(engine->damage_loop_play_id, 1.0f);
            }
            if (audiolib_engine_play_id_is_set(engine->damage_loop_play_id))
            {
                audiolib_internal_playlist_set_volume(engine->damage_loop_play_id, engine->engine_data->damage_vol_scale * engine->dest_damage, 100);
            }
            engine->curr_damage = engine->dest_damage;
        }
    }
    // Fewer than 50 free plays: onload loops only.
    if (Audiolib_state.frame_stats.free_instances < 50)
    {
        onload_scale = 1.0f;
        offload_scale = 0.0f;
    }
    for (int i = 0; i < 4; ++i)
    {
        data = engine->engine_data;
        bool offload_silent = true;
        if (data->playback_type != AUDIOLIB_VEHICLE_ENGINE_PLAYBACK_TYPE_WIND)
        {
            audiolib_veh_eng_clip_data& clip = engine->offload[i];
            const audiolib_vehicle_engine_rpm_data& range = data->offload[i];
            const float rpm = engine->curr_rpm;
            if (offload_scale <= 0.0f || rpm < range.min_rpm || range.max_rpm <= rpm)
            {
                // Out of range: the loop ends.
                clip.vol = 0.0f;
                clip.pitch = -1.0f;
                if (clip.play_id != -1)
                {
                    if (audiolib_engine_play_id_is_set(clip.play_id))
                    {
                        audiolib_internal_playlist_fade_out(clip.play_id, 200);
                    }
                    clip.play_id = -1;
                }
            }
            else
            {
                const float crossover = audiolib_engine_crossover(data->offload, i, rpm);
                const float fraction = (engine->curr_rpm - range.min_rpm) / (range.max_rpm - range.min_rpm);
                const float volume = audiolib_engine_clamp(((range.vol_max - range.vol_min) * fraction + range.vol_min) * crossover * offload_scale, 0.0f, 1.0f);
                clip.vol = audiolib_engine_cue_volume(engine, range, volume);
                // The idle loop keeps its pitch.
                float pitch = audiolib_engine_clamp((range.pitch_max - range.pitch_min) * fraction + range.pitch_min, -1.0f, 1.0f);
                if (i == 0)
                {
                    pitch = 0.0f;
                }
                clip.pitch = pitch;
                if (clip.vol > 0.0f)
                {
                    offload_silent = false;
                }
            }
            audiolib_engine_update_clip(engine, clip, range, i, false);
            if (offload_scale == 0.0f || clip.play_id == -1)
            {
                offload_silent = true;
            }
        }
        audiolib_veh_eng_clip_data& clip = engine->onload[i];
        const audiolib_vehicle_engine_rpm_data& range = data->onload[i];
        const float rpm = engine->curr_rpm;
        if (rpm < range.min_rpm || range.max_rpm <= rpm)
        {
            clip.vol = 0.0f;
            clip.pitch = 0.0f;
            if (clip.play_id != -1)
            {
                if (audiolib_engine_play_id_is_set(clip.play_id))
                {
                    audiolib_internal_playlist_fade_out(clip.play_id, 200);
                }
                clip.play_id = -1;
            }
        }
        else
        {
            // Without an offload loop the onload loop plays at full load.
            const float load_scale = offload_silent || offload_scale == 0.0f ? 1.0f : onload_scale;
            float crossover = audiolib_engine_crossover(data->onload, i, rpm);
            if (engine->engine_data->playback_type == AUDIOLIB_VEHICLE_ENGINE_PLAYBACK_TYPE_WIND)
            {
                // Wind: the lightest wind fades in over the first quarter of its range below 26 %; the field of view
                // (open sky) scales it.
                float fraction;
                if (i == 0 && (fraction = engine->curr_rpm / engine->engine_data->onload[0].max_rpm) < 0.26f)
                {
                    if (Audiolib_veh_engine_rpm_higher == nullptr)
                    {
                        Audiolib_veh_engine_rpm_higher = audiolib_rolloff_curve_find_appropriate_curve(3, "Engine_RPM_higher");
                    }
                    crossover = audiolib_calc_scaled_value(3, Audiolib_veh_engine_rpm_higher, 0.0f, 1.0f, 0.0f, 1.0f, fraction * 4.0f);
                }
                crossover = audiolib_engine_clamp((Audiolib_settings.maximum_wind_fov - engine->fov) * crossover, 0.0f, 1.0f);
            }
            data = engine->engine_data;
            const audiolib_vehicle_engine_rpm_data& range_data = data->onload[i];
            const float fraction = (engine->curr_rpm - range_data.min_rpm) / (range_data.max_rpm - range_data.min_rpm);
            const float volume = audiolib_engine_clamp(((range_data.vol_max - range_data.vol_min) * fraction + range_data.vol_min) * load_scale * crossover, 0.0f, 1.0f);
            clip.vol = audiolib_engine_cue_volume(engine, range_data, volume);
            if (data->playback_type == AUDIOLIB_VEHICLE_ENGINE_PLAYBACK_TYPE_WIND)
            {
                // Wind: the speed sets the low pass.
                clip.lowpass = audiolib_engine_clamp(engine->speed * Audiolib_settings.maximum_wind_lowpass, 0.0f, Audiolib_settings.maximum_wind_lowpass);
            }
            else
            {
                float pitch = audiolib_engine_clamp((range_data.pitch_max - range_data.pitch_min) * fraction + range_data.pitch_min, -1.0f, 1.0f);
                if (i == 0)
                {
                    pitch = 0.0f;
                }
                clip.pitch = pitch;
            }
        }
        audiolib_engine_update_clip(engine, clip, range, i, engine->engine_data->playback_type == AUDIOLIB_VEHICLE_ENGINE_PLAYBACK_TYPE_WIND);
    }
    if (engine->switch_to_3d)
    {
        engine->is_3d = true;
        engine->switch_to_3d = false;
    }
    if (engine->switch_to_2d)
    {
        engine->is_3d = false;
        engine->switch_to_2d = false;
    }
}

// 0x004A2B10 (:727)
bool audiolib_process_vehicle_engines(void* user_data, int time, int time_step)
{
    (void)user_data;
    (void)time;
    Audiolib_engine_cs.enter();
    ++process_frame_count;
    // Stopped engines are freed.
    Active_engine_list.thread_access.enter();
    audiolib_vehicle_engine_instance* next = Active_engine_list.get_first(AUDIOLIB_VEH_ENGINE_STATE_STOPPED);
    while (next != nullptr)
    {
        audiolib_vehicle_engine_instance* engine = next;
        next = Active_engine_list.get_next(engine);
        Active_engine_list.free(engine);
    }
    Active_engine_list.thread_access.leave();
    // Stopping engines stop once their loops are gone (one live loop is faded per pass).
    Active_engine_list.thread_access.enter();
    next = Active_engine_list.get_first(AUDIOLIB_VEH_ENGINE_STATE_STOPPING);
    while (next != nullptr)
    {
        audiolib_vehicle_engine_instance* engine = next;
        next = Active_engine_list.get_next(engine);
        bool playing = false;
        for (int i = 0; i < 4 && !playing; ++i)
        {
            if (engine->onload[i].play_id != -1 && audiolib_engine_stop_play(engine->onload[i].play_id))
            {
                playing = true;
                break;
            }
            if (engine->offload[i].play_id != -1 && audiolib_engine_stop_play(engine->offload[i].play_id))
            {
                playing = true;
                break;
            }
        }
        if (engine->damage_loop_play_id != -1 && audiolib_engine_stop_play(engine->damage_loop_play_id))
        {
            continue;
        }
        if (!playing)
        {
            Active_engine_list.move(engine, AUDIOLIB_VEH_ENGINE_STATE_STOPPED);
        }
    }
    Active_engine_list.thread_access.leave();
    if (!Audiolib_state.sounds_paused)
    {
        // During a volume delay the engines are silent.
        bool audible = true;
        if (Audiolib_vehicle_volume_delay.time_until() > 0 && (!Audiolib_state.initialized || !Audiolib_state.sounds_paused))
        {
            audible = false;
        }
        Active_engine_list.thread_access.enter();
        next = Active_engine_list.get_first(AUDIOLIB_VEH_ENGINE_STATE_RUNNING);
        while (next != nullptr)
        {
            audiolib_vehicle_engine_instance* engine = next;
            next = Active_engine_list.get_next(engine);
            // The RPM eases in (a quarter a pass); the start wait counts down.
            engine->curr_rpm = (engine->dest_rpm - engine->curr_rpm) * 0.25f + engine->curr_rpm;
            if (engine->wait_to_start < 1)
            {
                engine->wait_to_start = 0;
            }
            else
            {
                engine->wait_to_start -= time_step;
            }
            if (!audible)
            {
                audiolib_engine_silence(engine);
            }
            else
            {
                audiolib_engine_process_running(engine);
            }
        }
        Active_engine_list.thread_access.leave();
    }
    Audiolib_engine_cs.leave();
    return false;
}

// 0x004A5210 (:1456)
bool audiolib_internal_vehicle_engine_system_init()
{
    Audiolib_vehicle_volume_delay.set(0);
    Audiolib_engine_cs.enter();
    Audiolib_veh_engine_inited = false;
    Audiolib_Engine_lookup_list.count = 0;
    Audiolib_Engine_lookup_list.total = 0;
    Audiolib_Engine_lookup_list.engine_list = nullptr;
    Active_engine_list.clear();
    audiolib_create_rolloff_curve(3, "Engine_RPM_lower", 11, audiolib_rolloff_curve_lower_range_crossover_entries[0]);
    audiolib_create_rolloff_curve(3, "Engine_RPM_higher", 11, audiolib_rolloff_curve_higher_range_crossover_entries[0]);
    if (audiolib_vehicle_engine_parse_xtbl("foley_engine.xtbl") < 0)
    {
        Audiolib_engine_cs.leave();
        return false;
    }
    Audiolib_engine_cs.leave();
    // audiolib_thread_function_register, inlined.
    audiolib_process_data* process = Audio_process_list.alloc(0);
    if (process == nullptr)
    {
        return false;
    }
    process->user_data = nullptr;
    process->function = audiolib_process_vehicle_engines;
    strncpy(process->func_name, "audiolib_veh_engine_process", sizeof(process->func_name));
    Audiolib_veh_engine_inited = true;
    return true;
}

// 0x004A52D0 (:1490)
void audiolib_internal_vehicle_engine_system_shutdown()
{
    Audiolib_engine_cs.enter();
    Audiolib_veh_engine_inited = false;
    audiolib_thread_function_unregister(audiolib_process_vehicle_engines);
    Audiolib_Engine_lookup_list.count = 0;
    Audiolib_Engine_lookup_list.total = 0;
    Audiolib_Engine_lookup_list.engine_list = nullptr;
    Active_engine_list.clear();
    Audiolib_engine_cs.leave();
}

// 0x004981A0 (:1507)
int audiolib_internal_vehicle_engine_get_num_active()
{
    return (int)Active_engine_list.get_capacity() - Active_engine_list.get_free();
}

// 0x004A5320 (:1512)
void audiolib_internal_vehicle_engine_kill_all()
{
    // Running engines only.
    Audiolib_engine_cs.enter();
    Active_engine_list.thread_access.enter();
    audiolib_vehicle_engine_instance* next = Active_engine_list.get_first(AUDIOLIB_VEH_ENGINE_STATE_RUNNING);
    while (next != nullptr)
    {
        audiolib_vehicle_engine_instance* engine = next;
        next = Active_engine_list.get_next(engine);
        audiolib_vehicle_engine_stop(engine->engine_id);
    }
    Active_engine_list.thread_access.leave();
    Audiolib_engine_cs.leave();
}

// 0x004A53D0 (:1524)
void audiolib_internal_vehicle_engine_kill_with_extreme_prejudice(short bank_id)
{
    if (!Audiolib_veh_engine_inited)
    {
        return;
    }
    // Every engine on the wavebank, in every state.
    for (int state = 0; state < 4; ++state)
    {
        Audiolib_engine_cs.enter();
        Active_engine_list.thread_access.enter();
        audiolib_vehicle_engine_instance* next = Active_engine_list.get_first(state);
        while (next != nullptr)
        {
            audiolib_vehicle_engine_instance* engine = next;
            next = Active_engine_list.get_next(engine);
            if (engine->engine_data->wavebank_id == bank_id)
            {
                audiolib_vehicle_engine_stop(engine->engine_id);
            }
        }
        Active_engine_list.thread_access.leave();
        Audiolib_engine_cs.leave();
    }
}

// 0x0048A520 (:1550)
int audiolib_vehicle_engine_get_id(const char* engine_name)
{
    if (!Audiolib_veh_engine_inited)
    {
        return -1;
    }
    return audiolib_internal_find_engine_cue_id_by_name(engine_name);
}

// 0x0048A540 (:1569)
bool audiolib_vehicle_engine_is_id_valid(int engine_id)
{
    return Audiolib_veh_engine_inited && audiolib_internal_find_engine_by_cue_id(engine_id) != nullptr;
}

// 0x0048A560 (:1587)
short audiolib_vehicle_engine_get_wavebank_id(int engine_id)
{
    if (Audiolib_veh_engine_inited)
    {
        const audiolib_vehicle_engine_data* data = audiolib_internal_find_engine_by_cue_id(engine_id);
        if (data != nullptr)
        {
            return data->wavebank_id;
        }
    }
    return -1;
}

// A range's loop at start: in range it plays (mono in 3D), out of range its cue is only precached.
static void audiolib_engine_start_clip(audiolib_vehicle_engine_instance* engine, audiolib_veh_eng_clip_data& clip, const audiolib_vehicle_engine_rpm_data& range,
                                       audiolib_params& params)
{
    if (engine->curr_rpm < range.min_rpm || range.max_rpm <= engine->curr_rpm)
    {
        int cue_id;
        if (engine->is_3d && range.mono_cue_id != -1)
        {
            cue_id = range.mono_cue_id;
        }
        else if (range.cue_id != -1)
        {
            cue_id = range.cue_id;
        }
        else
        {
            return;
        }
        audiolib_params precache;
        precache.flags = 0x100;
        audiolib_cue_play(cue_id, &precache, nullptr);
    }
    else if (engine->is_3d && range.mono_cue_id != -1)
    {
        clip.play_id = audiolib_cue_play(range.mono_cue_id, &params, nullptr);
    }
    else if (range.cue_id != -1)
    {
        clip.play_id = audiolib_cue_play(range.cue_id, &params, nullptr);
    }
}

// 0x004A54B0 (:1608)
int audiolib_vehicle_engine_start(int engine_id, const vector* position, const vector* orient)
{
    if (!Audiolib_veh_engine_inited)
    {
        return -1;
    }
    // Original: an unknown engine id starts the first engine.
    audiolib_vehicle_engine_data* data = audiolib_internal_find_engine_by_cue_id(engine_id);
    if (data == nullptr)
    {
        data = Audiolib_Engine_lookup_list.engine_list;
    }
    Audiolib_engine_cs.enter();
    // The last free slot is kept for a 2D engine unless one is running.
    bool allowed = position == nullptr || Active_engine_list.get_free() > 1;
    for (int i = 0; !allowed && i < 30; ++i)
    {
        const audiolib_vehicle_engine_instance* other = Active_engine_list.get_entry(i);
        if (other != nullptr && other->engine_data != nullptr && other->engine_data->playback_type == AUDIOLIB_VEHICLE_ENGINE_PLAYBACK_TYPE_ENGINE && !other->is_3d)
        {
            allowed = true;
        }
    }
    int instance_id = -1;
    audiolib_vehicle_engine_instance* engine = allowed ? Active_engine_list.alloc(AUDIOLIB_VEH_ENGINE_STATE_STARTING) : nullptr;
    if (engine != nullptr)
    {
        engine->is_3d = false;
        engine->engine_data = data;
        engine->curr_pos.zero();
        engine->curr_orient.zero();
        engine->curr_onload = 1.0f;
        engine->dest_onload = 1.0f;
        engine->wait_to_start = data->idle_start_delay_ms;
        engine->curr_damage = 0.0f;
        engine->dest_damage = 0.0f;
        engine->damage_loop_play_id = -1;
        if (position != nullptr)
        {
            engine->is_3d = true;
            engine->curr_pos = *position;
            if (orient != nullptr)
            {
                engine->curr_orient = *orient;
                engine->curr_orient.flatten_normalize();
            }
        }
        // The loops start almost silent; the first one waits for the idle start delay.
        audiolib_params params;
        params.fade_ms = 200;
        if (engine->is_3d)
        {
            params.flags = 0x841;
        }
        else if (engine->engine_data->playback_type == AUDIOLIB_VEHICLE_ENGINE_PLAYBACK_TYPE_WIND)
        {
            params.flags = 0;
        }
        else
        {
            params.flags = 0x810;
        }
        params.delay_ms = data->idle_start_delay_ms;
        params.pos = engine->curr_pos;
        params.orient = engine->curr_orient;
        params.volume = 0.01f;
        engine->curr_rpm = data->idle_rpm;
        engine->dest_rpm = data->idle_rpm;
        if (data->playback_type == AUDIOLIB_VEHICLE_ENGINE_PLAYBACK_TYPE_WIND)
        {
            // Wind starts low passed.
            params.dsp_type = 1;
            params.dsp.low_pass.effect = 0.1f;
        }
        instance_id = (Active_engine_list.get_index(engine) & 0xFFFF) | (engine->rolling_counter << 16);
        engine->engine_id = instance_id;
        for (int i = 0; i < 4; ++i)
        {
            engine->onload[i].play_id = -1;
            engine->offload[i].play_id = -1;
            audiolib_engine_start_clip(engine, engine->onload[i], engine->engine_data->onload[i], params);
            audiolib_engine_start_clip(engine, engine->offload[i], engine->engine_data->offload[i], params);
            params.delay_ms = 0;
        }
        Active_engine_list.move(engine, AUDIOLIB_VEH_ENGINE_STATE_RUNNING);
    }
    Audiolib_engine_cs.leave();
    return instance_id;
}

// 0x004981B0 (:1794)
audiolib_result audiolib_vehicle_engine_get_state(int engine_inst, AUDIOLIB_VEHICLE_ENGINE_STATE* engine_state)
{
    if (!Audiolib_veh_engine_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (engine_state == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    Audiolib_engine_cs.enter();
    audiolib_vehicle_engine_instance* engine = audiolib_internal_get_vehicle_engine_inst(engine_inst);
    if (engine == nullptr)
    {
        Audiolib_engine_cs.leave();
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    // The pool category is the state.
    *engine_state = (AUDIOLIB_VEHICLE_ENGINE_STATE)Active_engine_list.get_category(engine);
    Audiolib_engine_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00498240 (:1819)
bool audiolib_vehicle_engine_is_playing(int engine_inst)
{
    if (!Audiolib_veh_engine_inited)
    {
        return false;
    }
    Audiolib_engine_cs.enter();
    audiolib_vehicle_engine_instance* engine = audiolib_internal_get_vehicle_engine_inst(engine_inst);
    const bool playing = engine != nullptr && Active_engine_list.get_category(engine) == AUDIOLIB_VEH_ENGINE_STATE_RUNNING;
    Audiolib_engine_cs.leave();
    return playing;
}

// 0x00492690 (:1849)
audiolib_result audiolib_vehicle_engine_audio_to_3d(int engine_inst, vector* position, vector* orient)
{
    if (!Audiolib_veh_engine_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (position == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    Audiolib_engine_cs.enter();
    audiolib_vehicle_engine_instance* engine = audiolib_internal_get_vehicle_engine_inst(engine_inst);
    if (engine == nullptr)
    {
        Audiolib_engine_cs.leave();
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    // The loops switch on the next pass.
    if (!engine->is_3d)
    {
        engine->curr_pos = *position;
        if (orient != nullptr)
        {
            engine->curr_orient = *orient;
            engine->curr_orient.flatten_normalize();
        }
        engine->switch_to_3d = true;
        engine->switch_to_2d = false;
    }
    Audiolib_engine_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00492730 (:1882)
audiolib_result audiolib_vehicle_engine_audio_to_2d(int engine_inst)
{
    if (!Audiolib_veh_engine_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    Audiolib_engine_cs.enter();
    audiolib_vehicle_engine_instance* engine = audiolib_internal_get_vehicle_engine_inst(engine_inst);
    if (engine == nullptr)
    {
        Audiolib_engine_cs.leave();
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (engine->is_3d)
    {
        engine->switch_to_3d = false;
        engine->switch_to_2d = true;
    }
    Audiolib_engine_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00492790 (:1911)
audiolib_result audiolib_vehicle_engine_set_pos(int engine_inst, vector* position, vector* orient)
{
    if (!Audiolib_veh_engine_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (position == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    Audiolib_engine_cs.enter();
    audiolib_vehicle_engine_instance* engine = audiolib_internal_get_vehicle_engine_inst(engine_inst);
    if (engine == nullptr)
    {
        Audiolib_engine_cs.leave();
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    // A 2D engine ignores positions.
    if (engine->is_3d)
    {
        engine->curr_pos = *position;
        if (orient != nullptr)
        {
            engine->curr_orient = *orient;
            engine->curr_orient.flatten_normalize();
        }
    }
    Audiolib_engine_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00492810 (:1944)
audiolib_result audiolib_vehicle_engine_get_RPM(int engine_inst, float* rpm)
{
    if (!Audiolib_veh_engine_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (rpm == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    Audiolib_engine_cs.enter();
    const audiolib_vehicle_engine_instance* engine = audiolib_internal_get_vehicle_engine_inst(engine_inst);
    if (engine == nullptr)
    {
        Audiolib_engine_cs.leave();
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    *rpm = engine->curr_rpm;
    Audiolib_engine_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00492860 (:1973)
audiolib_result audiolib_vehicle_engine_set_RPM(int engine_inst, float speed, float rpm, float load, float damage)
{
    if (!Audiolib_veh_engine_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    Audiolib_engine_cs.enter();
    audiolib_vehicle_engine_instance* engine = audiolib_internal_get_vehicle_engine_inst(engine_inst);
    if (engine == nullptr)
    {
        Audiolib_engine_cs.leave();
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    // Load and damage 0..1; at the RPM limit (1 below the maximum) the engine is under full load.
    load = audiolib_engine_clamp(load, 0.0f, 1.0f);
    if (damage - 0.0f < 0.0f)
    {
        damage = 0.0f;
    }
    float clamped_damage = 1.0f;
    if (damage - 1.0f < 0.0f)
    {
        clamped_damage = damage;
    }
    float max_rpm = engine->engine_data->max_rpm;
    if (max_rpm - 1.0f < rpm)
    {
        load = 1.0f;
        rpm = max_rpm - 1.0f;
    }
    if (rpm - 0.0f < 0.0f)
    {
        rpm = 0.0f;
    }
    if (rpm - max_rpm < 0.0f)
    {
        max_rpm = rpm;
    }
    load = audiolib_engine_clamp(load, 0.0f, 1.0f);
    clamped_damage = audiolib_engine_clamp(clamped_damage, 0.0f, 1.0f);
    engine->speed = speed;
    engine->dest_rpm = max_rpm;
    engine->dest_onload = load;
    engine->dest_damage = clamped_damage;
    Audiolib_engine_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x0049DFE0 (:2022)
audiolib_result audiolib_vehicle_engine_stop(int engine_inst)
{
    if (!Audiolib_veh_engine_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    Audiolib_engine_cs.enter();
    audiolib_vehicle_engine_instance* engine = audiolib_internal_get_vehicle_engine_inst(engine_inst);
    if (engine == nullptr)
    {
        Audiolib_engine_cs.leave();
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    // The loops fade over 200 ms; the process stops the engine once they are gone.
    Active_engine_list.move(engine, AUDIOLIB_VEH_ENGINE_STATE_STOPPING);
    for (int i = 0; i < 4; ++i)
    {
        if (engine->onload[i].play_id != -1 && audiolib_engine_play_id_is_set(engine->onload[i].play_id))
        {
            audiolib_internal_playlist_fade_out(engine->onload[i].play_id, 200);
        }
        if (engine->offload[i].play_id != -1 && audiolib_engine_play_id_is_set(engine->offload[i].play_id))
        {
            audiolib_internal_playlist_fade_out(engine->offload[i].play_id, 200);
        }
    }
    if (engine->damage_loop_play_id != -1 && audiolib_engine_play_id_is_set(engine->damage_loop_play_id))
    {
        audiolib_internal_playlist_fade_out(engine->damage_loop_play_id, 200);
    }
    Audiolib_engine_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x004982B0 (:2061)
bool audiolib_internal_check_sound_used_in_engine(int id)
{
    // Any loop of an engine (not wind) in any state.
    Audiolib_engine_cs.enter();
    for (int state = 0; state < 4; ++state)
    {
        Active_engine_list.thread_access.enter();
        for (audiolib_vehicle_engine_instance* engine = Active_engine_list.get_first(state); engine != nullptr; engine = Active_engine_list.get_next(engine))
        {
            if (engine->engine_data == nullptr || engine->engine_data->playback_type != AUDIOLIB_VEHICLE_ENGINE_PLAYBACK_TYPE_ENGINE)
            {
                continue;
            }
            for (int i = 0; i < 4; ++i)
            {
                if (engine->onload[i].play_id == id || engine->offload[i].play_id == id || engine->damage_loop_play_id == id)
                {
                    Active_engine_list.thread_access.leave();
                    Audiolib_engine_cs.leave();
                    return true;
                }
            }
        }
        Active_engine_list.thread_access.leave();
    }
    Audiolib_engine_cs.leave();
    return false;
}
